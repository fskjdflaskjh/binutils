/* libthread_db assisted debugging support, generic parts.

   Copyright (C) 1999-2018 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include <dlfcn.h>
#include "gdb_proc_service.h"
#include "nat/gdb_thread_db.h"
#include "gdb_vecs.h"
#include "bfd.h"
#include "command.h"
#include "gdbcmd.h"
#include "gdbthread.h"
#include "inferior.h"
#include "infrun.h"
#include "symfile.h"
#include "objfiles.h"
#include "target.h"
#include "regcache.h"
#include "solib.h"
#include "solib-svr4.h"
#include "gdbcore.h"
#include "observable.h"
#include "linux-nat.h"
#include "nat/linux-procfs.h"
#include "nat/linux-ptrace.h"
#include "nat/linux-osdata.h"
#include "auto-load.h"
#include "cli/cli-utils.h"
#include <signal.h>
#include <ctype.h>
#include "nat/linux-namespaces.h"
#include <algorithm>
#include "common/pathstuff.h"
#include "valprint.h"

/* GNU/Linux libthread_db support.

   libthread_db is a library, provided along with libpthread.so, which
   exposes the internals of the thread library to a debugger.  It
   allows GDB to find existing threads, new threads as they are
   created, thread IDs (usually, the result of pthread_self), and
   thread-local variables.

   The libthread_db interface originates on Solaris, where it is both
   more powerful and more complicated.  This implementation only works
   for NPTL, the glibc threading library.  It assumes that each thread
   is permanently assigned to a single light-weight process (LWP).  At
   some point it also supported the older LinuxThreads library, but it
   no longer does.

   libthread_db-specific information is stored in the "private" field
   of struct thread_info.  When the field is NULL we do not yet have
   information about the new thread; this could be temporary (created,
   but the thread library's data structures do not reflect it yet)
   or permanent (created using clone instead of pthread_create).

   Process IDs managed by linux-thread-db.c match those used by
   linux-nat.c: a common PID for all processes, an LWP ID for each
   thread, and no TID.  We save the TID in private.  Keeping it out
   of the ptid_t prevents thread IDs changing when libpthread is
   loaded or unloaded.  */

static const target_info thread_db_target_info = {
  "multi-thread",
  N_("multi-threaded child process."),
  N_("Threads and pthreads support.")
};

class thread_db_target final : public target_ops
{
public:
  thread_db_target ();

  const target_info &info () const override
  { return thread_db_target_info; }

  void detach (inferior *, int) override;
  ptid_t wait (ptid_t, struct target_waitstatus *, int) override;
  void resume (ptid_t, int, enum gdb_signal) override;
  void mourn_inferior () override;
  void update_thread_list () override;
  const char *pid_to_str (ptid_t) override;
  CORE_ADDR get_thread_local_address (ptid_t ptid,
				      CORE_ADDR load_module_addr,
				      CORE_ADDR offset) override;
  const char *extra_thread_info (struct thread_info *) override;
  ptid_t get_ada_task_ptid (long lwp, long thread) override;

  thread_info *thread_handle_to_thread_info (const gdb_byte *thread_handle,
					     int handle_len,
					     inferior *inf) override;
};

thread_db_target::thread_db_target ()
{
  this->to_stratum = thread_stratum;
}

static char *libthread_db_search_path;

/* Set to non-zero if thread_db auto-loading is enabled
   by the "set auto-load libthread-db" command.  */
static int auto_load_thread_db = 1;

/* Set to non-zero if load-time libthread_db tests have been enabled
   by the "maintenence set check-libthread-db" command.  */
static int check_thread_db_on_load = 0;

/* "show" command for the auto_load_thread_db configuration variable.  */

static void
show_auto_load_thread_db (struct ui_file *file, int from_tty,
			  struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("Auto-loading of inferior specific libthread_db "
			    "is %s.\n"),
		    value);
}

static void
set_libthread_db_search_path (const char *ignored, int from_tty,
			      struct cmd_list_element *c)
{
  if (*libthread_db_search_path == '\0')
    {
      xfree (libthread_db_search_path);
      libthread_db_search_path = xstrdup (LIBTHREAD_DB_SEARCH_PATH);
    }
}

/* If non-zero, print details of libthread_db processing.  */

static unsigned int libthread_db_debug;

static void
show_libthread_db_debug (struct ui_file *file, int from_tty,
			 struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("libthread-db debugging is %s.\n"), value);
}

/* If we're running on GNU/Linux, we must explicitly attach to any new
   threads.  */

/* This module's target vector.  */
static thread_db_target the_thread_db_target;

/* Non-zero if we have determined the signals used by the threads
   library.  */
static int thread_signals;
static sigset_t thread_stop_set;
static sigset_t thread_print_set;

struct thread_db_info
{
  struct thread_db_info *next;

  /* Process id this object refers to.  */
  int pid;

  /* Handle from dlopen for libthread_db.so.  */
  void *handle;

  /* Absolute pathname from gdb_realpath to disk file used for dlopen-ing
     HANDLE.  It may be NULL for system library.  */
  char *filename;

  /* Structure that identifies the child process for the
     <proc_service.h> interface.  */
  struct ps_prochandle proc_handle;

  /* Connection to the libthread_db library.  */
  td_thragent_t *thread_agent;

  /* True if we need to apply the workaround for glibc/BZ5983.  When
     we catch a PTRACE_O_TRACEFORK, and go query the child's thread
     list, nptl_db returns the parent's threads in addition to the new
     (single) child thread.  If this flag is set, we do extra work to
     be able to ignore such stale entries.  */
  int need_stale_parent_threads_check;

  /* Pointers to the libthread_db functions.  */

  td_init_ftype *td_init_p;
  td_ta_new_ftype *td_ta_new_p;
  td_ta_map_lwp2thr_ftype *td_ta_map_lwp2thr_p;
  td_ta_thr_iter_ftype *td_ta_thr_iter_p;
  td_thr_get_info_ftype *td_thr_get_info_p;
  td_thr_tls_get_addr_ftype *td_thr_tls_get_addr_p;
  td_thr_tlsbase_ftype *td_thr_tlsbase_p;
};

/* List of known processes using thread_db, and the required
   bookkeeping.  */
struct thread_db_info *thread_db_list;

static void thread_db_find_new_threads_1 (thread_info *stopped);
static void thread_db_find_new_threads_2 (thread_info *stopped,
					  bool until_no_new);

static void check_thread_signals (void);

static struct thread_info *record_thread
  (struct thread_db_info *info, struct thread_info *tp,
   ptid_t ptid, const td_thrhandle_t *th_p, const td_thrinfo_t *ti_p);

/* Add the current inferior to the list of processes using libpthread.
   Return a pointer to the newly allocated object that was added to
   THREAD_DB_LIST.  HANDLE is the handle returned by dlopen'ing
   LIBTHREAD_DB_SO.  */

static struct thread_db_info *
add_thread_db_info (void *handle)
{
  struct thread_db_info *info = XCNEW (struct thread_db_info);

  info->pid = inferior_ptid.pid ();
  info->handle = handle;

  /* The workaround works by reading from /proc/pid/status, so it is
     disabled for core files.  */
  if (target_has_execution)
    info->need_stale_parent_threads_check = 1;

  info->next = thread_db_list;
  thread_db_list = info;

  return info;
}

/* Return the thread_db_info object representing the bookkeeping
   related to process PID, if any; NULL otherwise.  */

static struct thread_db_info *
get_thread_db_info (int pid)
{
  struct thread_db_info *info;

  for (info = thread_db_list; info; info = info->next)
    if (pid == info->pid)
      return info;

  return NULL;
}

/* When PID has exited or has been detached, we no longer want to keep
   track of it as using libpthread.  Call this function to discard
   thread_db related info related to PID.  Note that this closes
   LIBTHREAD_DB_SO's dlopen'ed handle.  */

static void
delete_thread_db_info (int pid)
{
  struct thread_db_info *info, *info_prev;

  info_prev = NULL;

  for (info = thread_db_list; info; info_prev = info, info = info->next)
    if (pid == info->pid)
      break;

  if (info == NULL)
    return;

  if (info->handle != NULL)
    dlclose (info->handle);

  xfree (info->filename);

  if (info_prev)
    info_prev->next = info->next;
  else
    thread_db_list = info->next;

  xfree (info);
}

/* Use "struct private_thread_info" to cache thread state.  This is
   a substantial optimization.  */

struct thread_db_thread_info : public private_thread_info
{
  /* Flag set when we see a TD_DEATH event for this thread.  */
  bool dying = false;

  /* Cached thread state.  */
  td_thrhandle_t th {};
  thread_t tid {};
};

static thread_db_thread_info *
get_thread_db_thread_info (thread_info *thread)
{
  return static_cast<thread_db_thread_info *> (thread->priv.get ());
}

static const char *
thread_db_err_str (td_err_e err)
{
  static char buf[64];

  switch (err)
    {
    case TD_OK:
      return "generic 'call succeeded'";
    case TD_ERR:
      return "generic error";
    case TD_NOTHR:
      return "no thread to satisfy query";
    case TD_NOSV:
      return "no sync handle to satisfy query";
    case TD_NOLWP:
      return "no LWP to satisfy query";
    case TD_BADPH:
      return "invalid process handle";
    case TD_BADTH:
      return "invalid thread handle";
    case TD_BADSH:
      return "invalid synchronization handle";
    case TD_BADTA:
      return "invalid thread agent";
    case TD_BADKEY:
      return "invalid key";
    case TD_NOMSG:
      return "no event message for getmsg";
    case TD_NOFPREGS:
      return "FPU register set not available";
    case TD_NOLIBTHREAD:
      return "application not linked with libthread";
    case TD_NOEVENT:
      return "requested event is not supported";
    case TD_NOCAPAB:
      return "capability not available";
    case TD_DBERR:
      return "debugger service failed";
    case TD_NOAPLIC:
      return "operation not applicable to";
    case TD_NOTSD:
      return "no thread-specific data for this thread";
    case TD_MALLOC:
      return "malloc failed";
    case TD_PARTIALREG:
      return "only part of register set was written/read";
    case TD_NOXREGS:
      return "X register set not available for this thread";
#ifdef THREAD_DB_HAS_TD_NOTALLOC
    case TD_NOTALLOC:
      return "thread has not yet allocated TLS for given module";
#endif
#ifdef THREAD_DB_HAS_TD_VERSION
    case TD_VERSION:
      return "versions of libpthread and libthread_db do not match";
#endif
#ifdef THREAD_DB_HAS_TD_NOTLS
    case TD_NOTLS:
      return "there is no TLS segment in the given module";
#endif
    default:
      snprintf (buf, sizeof (buf), "unknown thread_db error '%d'", err);
      return buf;
    }
}

/* Fetch the user-level thread id of PTID.  STOPPED is a stopped
   thread that we can use to access memory.  */

static struct thread_info *
thread_from_lwp (thread_info *stopped, ptid_t ptid)
{
  td_thrhandle_t th;
  td_thrinfo_t ti;
  td_err_e err;
  struct thread_db_info *info;
  struct thread_info *tp;

  /* Just in case td_ta_map_lwp2thr doesn't initialize it completely.  */
  th.th_unique = 0;

  /* This ptid comes from linux-nat.c, which should always fill in the
     LWP.  */
  gdb_assert (ptid.lwp () != 0);

  info = get_thread_db_info (ptid.pid ());

  /* Access an lwp we know is stopped.  */
  info->proc_handle.thread = stopped;
  err = info->td_ta_map_lwp2thr_p (info->thread_agent, ptid.lwp (),
				   &th);
  if (err != TD_OK)
    error (_("Cannot find user-level thread for LWP %ld: %s"),
	   ptid.lwp (), thread_db_err_str (err));

  err = info->td_thr_get_info_p (&th, &ti);
  if (err != TD_OK)
    error (_("thread_get_info_callback: cannot get thread info: %s"),
	   thread_db_err_str (err));

  /* Fill the cache.  */
  tp = find_thread_ptid (ptid);
  return record_thread (info, tp, ptid, &th, &ti);
}


/* See linux-nat.h.  */

int
thread_db_notice_clone (ptid_t parent, ptid_t child)
{
  struct thread_db_info *info;

  info = get_thread_db_info (child.pid ());

  if (info == NULL)
    return 0;

  thread_info *stopped = find_thread_ptid (parent);

  thread_from_lwp (stopped, child);

  /* If we do not know about the main thread's pthread info yet, this
     would be a good time to find it.  */
  thread_from_lwp (stopped, parent);
  return 1;
}

static void *
verbose_dlsym (void *handle, const char *name)
{
  void *sym = dlsym (handle, name);
  if (sym == NULL)
    warning (_("Symbol \"%s\" not found in libthread_db: %s"),
	     name, dlerror ());
  return sym;
}

/* Verify inferior's '\0'-terminated symbol VER_SYMBOL starts with "%d.%d" and
   return 1 if this version is lower (and not equal) to
   VER_MAJOR_MIN.VER_MINOR_MIN.  Return 0 in all other cases.  */

static int
inferior_has_bug (const char *ver_symbol, int ver_major_min, int ver_minor_min)
{
  struct bound_minimal_symbol version_msym;
  CORE_ADDR version_addr;
  gdb::unique_xmalloc_ptr<char> version;
  int err, got, retval = 0;

  version_msym = lookup_minimal_symbol (ver_symbol, NULL, NULL);
  if (version_msym.minsym == NULL)
    return 0;

  version_addr = BMSYMBOL_VALUE_ADDRESS (version_msym);
  got = target_read_string (version_addr, &version, 32, &err);
  if (err == 0 && memchr (version.get (), 0, got) == version.get () + got - 1)
    {
      int major, minor;

      retval = (sscanf (version.get (), "%d.%d", &major, &minor) == 2
		&& (major < ver_major_min
		    || (major == ver_major_min && minor < ver_minor_min)));
    }

  return retval;
}

/* Similar as thread_db_find_new_threads_1, but try to silently ignore errors
   if appropriate.

   Return 1 if the caller should abort libthread_db initialization.  Return 0
   otherwise.  */

static int
thread_db_find_new_threads_silently (thread_info *stopped)
{

  TRY
    {
      thread_db_find_new_threads_2 (stopped, true);
    }

  CATCH (except, RETURN_MASK_ERROR)
    {
      if (libthread_db_debug)
	exception_fprintf (gdb_stdlog, except,
			   "Warning: thread_db_find_new_threads_silently: ");

      /* There is a bug fixed between nptl 2.6.1 and 2.7 by
	   commit 7d9d8bd18906fdd17364f372b160d7ab896ce909
	 where calls to td_thr_get_info fail with TD_ERR for statically linked
	 executables if td_thr_get_info is called before glibc has initialized
	 itself.
	 
	 If the nptl bug is NOT present in the inferior and still thread_db
	 reports an error return 1.  It means the inferior has corrupted thread
	 list and GDB should fall back only to LWPs.

	 If the nptl bug is present in the inferior return 0 to silently ignore
	 such errors, and let gdb enumerate threads again later.  In such case
	 GDB cannot properly display LWPs if the inferior thread list is
	 corrupted.  For core files it does not apply, no 'later enumeration'
	 is possible.  */

      if (!target_has_execution || !inferior_has_bug ("nptl_version", 2, 7))
	{
	  exception_fprintf (gdb_stderr, except,
			     _("Warning: couldn't activate thread debugging "
			       "using libthread_db: "));
	  return 1;
	}
    }
  END_CATCH

  return 0;
}

/* Lookup a library in which given symbol resides.
   Note: this is looking in GDB process, not in the inferior.
   Returns library name, or NULL.  */

static const char *
dladdr_to_soname (const void *addr)
{
  Dl_info info;

  if (dladdr (addr, &info) != 0)
    return info.dli_fname;
  return NULL;
}

/* State for check_thread_db_callback.  */

struct check_thread_db_info
{
  /* The libthread_db under test.  */
  struct thread_db_info *info;

  /* True if progress should be logged.  */
  bool log_progress;

  /* True if the callback was called.  */
  bool threads_seen;

  /* Name of last libthread_db function called.  */
  const char *last_call;

  /* Value returned by last libthread_db call.  */
  td_err_e last_result;
};

static struct check_thread_db_info *tdb_testinfo;

/* Callback for check_thread_db.  */

static int
check_thread_db_callback (const td_thrhandle_t *th, void *arg)
{
  gdb_assert (tdb_testinfo != NULL);
  tdb_testinfo->threads_seen = true;

#define LOG(fmt, args...)						\
  do									\
    {									\
      if (tdb_testinfo->log_progress)					\
	{								\
	  debug_printf (fmt, ## args);					\
	  gdb_flush (gdb_stdlog);					\
	}								\
    }									\
  while (0)

#define CHECK_1(expr, args...)						\
  do									\
    {									\
      if (!(expr))							\
	{								\
	  LOG (" ... FAIL!\n");						\
	  error (args);							\
	}								\
    }									\
  while (0)

#define CHECK(expr)							\
  CHECK_1 (expr, "(%s) == false", #expr)

#define CALL_UNCHECKED(func, args...)					\
  do									\
    {									\
      tdb_testinfo->last_call = #func;					\
      tdb_testinfo->last_result						\
	= tdb_testinfo->info->func ## _p (args);			\
    }									\
  while (0)

#define CHECK_CALL()							\
  CHECK_1 (tdb_testinfo->last_result == TD_OK,				\
	   _("%s failed: %s"),						\
	   tdb_testinfo->last_call,					\
	   thread_db_err_str (tdb_testinfo->last_result))		\

#define CALL(func, args...)						\
  do									\
    {									\
      CALL_UNCHECKED (func, args);					\
      CHECK_CALL ();							\
    }									\
  while (0)

  LOG ("  Got thread");

  /* Check td_ta_thr_iter passed consistent arguments.  */
  CHECK (th != NULL);
  CHECK (arg == (void *) tdb_testinfo);
  CHECK (th->th_ta_p == tdb_testinfo->info->thread_agent);

  LOG (" %s", core_addr_to_string_nz ((CORE_ADDR) th->th_unique));

  /* Check td_thr_get_info.  */
  td_thrinfo_t ti;
  CALL (td_thr_get_info, th, &ti);

  LOG (" => %d", ti.ti_lid);

  CHECK (ti.ti_ta_p == th->th_ta_p);
  CHECK (ti.ti_tid == (thread_t) th->th_unique);

  /* Check td_ta_map_lwp2thr.  */
  td_thrhandle_t th2;
  memset (&th2, 23, sizeof (td_thrhandle_t));
  CALL_UNCHECKED (td_ta_map_lwp2thr, th->th_ta_p, ti.ti_lid, &th2);

  if (tdb_testinfo->last_result == TD_ERR && !target_has_execution)
    {
      /* Some platforms require execution for td_ta_map_lwp2thr.  */
      LOG (_("; can't map_lwp2thr"));
    }
  else
    {
      CHECK_CALL ();

      LOG (" => %s", core_addr_to_string_nz ((CORE_ADDR) th2.th_unique));

      CHECK (memcmp (th, &th2, sizeof (td_thrhandle_t)) == 0);
    }

  /* Attempt TLS access.  Assuming errno is TLS, this calls
     thread_db_get_thread_local_address, which in turn calls
     td_thr_tls_get_addr for live inferiors or td_thr_tlsbase
     for core files.  This test is skipped if the thread has
     not been recorded; proceeding in that case would result
     in the test having the side-effect of noticing threads
     which seems wrong.

     Note that in glibc's libthread_db td_thr_tls_get_addr is
     a thin wrapper around td_thr_tlsbase; this check always
     hits the bulk of the code.

     Note also that we don't actually check any libthread_db
     calls are made, we just assume they were; future changes
     to how GDB accesses TLS could result in this passing
     without exercising the calls it's supposed to.  */
  ptid_t ptid = ptid_t (tdb_testinfo->info->pid, ti.ti_lid, 0);
  struct thread_info *thread_info = find_thread_ptid (ptid);
  if (thread_info != NULL && thread_info->priv != NULL)
    {
      LOG ("; errno");

      scoped_restore_current_thread restore_current_thread;
      switch_to_thread (ptid);

      expression_up expr = parse_expression ("(int) errno");
      struct value *val = evaluate_expression (expr.get ());

      if (tdb_testinfo->log_progress)
	{
	  struct value_print_options opts;

	  get_user_print_options (&opts);
	  LOG (" = ");
	  value_print (val, gdb_stdlog, &opts);
	}
    }

  LOG (" ... OK\n");

#undef LOG
#undef CHECK_1
#undef CHECK
#undef CALL_UNCHECKED
#undef CHECK_CALL
#undef CALL

  return 0;
}

/* Run integrity checks on the dlopen()ed libthread_db described by
   INFO.  Returns true on success, displays a warning and returns
   false on failure.  Logs progress messages to gdb_stdlog during
   the test if LOG_PROGRESS is true.  */

static bool
check_thread_db (struct thread_db_info *info, bool log_progress)
{
  bool test_passed = true;

  if (log_progress)
    debug_printf (_("Running libthread_db integrity checks:\n"));

  /* GDB avoids using td_ta_thr_iter wherever possible (see comment
     in try_thread_db_load_1 below) so in order to test it we may
     have to locate it ourselves.  */
  td_ta_thr_iter_ftype *td_ta_thr_iter_p = info->td_ta_thr_iter_p;
  if (td_ta_thr_iter_p == NULL)
    {
      void *thr_iter = verbose_dlsym (info->handle, "td_ta_thr_iter");
      if (thr_iter == NULL)
	return 0;

      td_ta_thr_iter_p = (td_ta_thr_iter_ftype *) thr_iter;
    }

  /* Set up the test state we share with the callback.  */
  gdb_assert (tdb_testinfo == NULL);
  struct check_thread_db_info tdb_testinfo_buf;
  tdb_testinfo = &tdb_testinfo_buf;

  memset (tdb_testinfo, 0, sizeof (struct check_thread_db_info));
  tdb_testinfo->info = info;
  tdb_testinfo->log_progress = log_progress;

  /* td_ta_thr_iter shouldn't be used on running processes.  Note that
     it's possible the inferior will stop midway through modifying one
     of its thread lists, in which case the check will spuriously
     fail.  */
  linux_stop_and_wait_all_lwps ();

  TRY
    {
      td_err_e err = td_ta_thr_iter_p (info->thread_agent,
				       check_thread_db_callback,
				       tdb_testinfo,
				       TD_THR_ANY_STATE,
				       TD_THR_LOWEST_PRIORITY,
				       TD_SIGNO_MASK,
				       TD_THR_ANY_USER_FLAGS);

      if (err != TD_OK)
	error (_("td_ta_thr_iter failed: %s"), thread_db_err_str (err));

      if (!tdb_testinfo->threads_seen)
	error (_("no threads seen"));
    }
  CATCH (except, RETURN_MASK_ERROR)
    {
      if (warning_pre_print)
	fputs_unfiltered (warning_pre_print, gdb_stderr);

      exception_fprintf (gdb_stderr, except,
			 _("libthread_db integrity checks failed: "));

      test_passed = false;
    }
  END_CATCH

  if (test_passed && log_progress)
    debug_printf (_("libthread_db integrity checks passed.\n"));

  tdb_testinfo = NULL;

  linux_unstop_all_lwps ();

  return test_passed;
}

/* Attempt to initialize dlopen()ed libthread_db, described by INFO.
   Return 1 on success.
   Failure could happen if libthread_db does not have symbols we expect,
   or when it refuses to work with the current inferior (e.g. due to
   version mismatch between libthread_db and libpthread).  */

static int
try_thread_db_load_1 (struct thread_db_info *info)
{
  td_err_e err;

  /* Initialize pointers to the dynamic library functions we will use.
     Essential functions first.  */

#define TDB_VERBOSE_DLSYM(info, func)			\
  info->func ## _p = (func ## _ftype *) verbose_dlsym (info->handle, #func)

#define TDB_DLSYM(info, func)			\
  info->func ## _p = (func ## _ftype *) dlsym (info->handle, #func)

#define CHK(a)								\
  do									\
    {									\
      if ((a) == NULL)							\
	return 0;							\
  } while (0)

  CHK (TDB_VERBOSE_DLSYM (info, td_init));

  err = info->td_init_p ();
  if (err != TD_OK)
    {
      warning (_("Cannot initialize libthread_db: %s"),
	       thread_db_err_str (err));
      return 0;
    }

  CHK (TDB_VERBOSE_DLSYM (info, td_ta_new));

  /* Initialize the structure that identifies the child process.  */
  info->proc_handle.thread = inferior_thread ();

  /* Now attempt to open a connection to the thread library.  */
  err = info->td_ta_new_p (&info->proc_handle, &info->thread_agent);
  if (err != TD_OK)
    {
      if (libthread_db_debug)
	fprintf_unfiltered (gdb_stdlog, _("td_ta_new failed: %s\n"),
			    thread_db_err_str (err));
      else
        switch (err)
          {
            case TD_NOLIBTHREAD:
#ifdef THREAD_DB_HAS_TD_VERSION
            case TD_VERSION:
#endif
              /* The errors above are not unexpected and silently ignored:
                 they just mean we haven't found correct version of
                 libthread_db yet.  */
              break;
            default:
              warning (_("td_ta_new failed: %s"), thread_db_err_str (err));
          }
      return 0;
    }

  /* These are essential.  */
  CHK (TDB_VERBOSE_DLSYM (info, td_ta_map_lwp2thr));
  CHK (TDB_VERBOSE_DLSYM (info, td_thr_get_info));

  /* These are not essential.  */
  TDB_DLSYM (info, td_thr_tls_get_addr);
  TDB_DLSYM (info, td_thr_tlsbase);

  /* It's best to avoid td_ta_thr_iter if possible.  That walks data
     structures in the inferior's address space that may be corrupted,
     or, if the target is running, may change while we walk them.  If
     there's execution (and /proc is mounted), then we're already
     attached to all LWPs.  Use thread_from_lwp, which uses
     td_ta_map_lwp2thr instead, which does not walk the thread list.

     td_ta_map_lwp2thr uses ps_get_thread_area, but we can't use that
     currently on core targets, as it uses ptrace directly.  */
  if (target_has_execution
      && linux_proc_task_list_dir_exists (inferior_ptid.pid ()))
    info->td_ta_thr_iter_p = NULL;
  else
    CHK (TDB_VERBOSE_DLSYM (info, td_ta_thr_iter));

#undef TDB_VERBOSE_DLSYM
#undef TDB_DLSYM
#undef CHK

  /* Run integrity checks if requested.  */
  if (check_thread_db_on_load)
    {
      if (!check_thread_db (info, libthread_db_debug))
	return 0;
    }

  if (info->td_ta_thr_iter_p == NULL)
    {
      struct lwp_info *lp;
      int pid = inferior_ptid.pid ();
      thread_info *curr_thread = inferior_thread ();

      linux_stop_and_wait_all_lwps ();

      ALL_LWPS (lp)
	if (lp->ptid.pid () == pid)
	  thread_from_lwp (curr_thread, lp->ptid);

      linux_unstop_all_lwps ();
    }
  else if (thread_db_find_new_threads_silently (inferior_thread ()) != 0)
    {
      /* Even if libthread_db initializes, if the thread list is
         corrupted, we'd not manage to list any threads.  Better reject this
         thread_db, and fall back to at least listing LWPs.  */
      return 0;
    }

  printf_unfiltered (_("[Thread debugging using libthread_db enabled]\n"));

  if (*libthread_db_search_path || libthread_db_debug)
    {
      struct ui_file *file;
      const char *library;

      library = dladdr_to_soname ((const void *) *info->td_ta_new_p);
      if (library == NULL)
	library = LIBTHREAD_DB_SO;

      /* If we'd print this to gdb_stdout when debug output is
	 disabled, still print it to gdb_stdout if debug output is
	 enabled.  User visible output should not depend on debug
	 settings.  */
      file = *libthread_db_search_path != '\0' ? gdb_stdout : gdb_stdlog;
      fprintf_unfiltered (file, _("Using host libthread_db library \"%s\".\n"),
			  library);
    }

  /* The thread library was detected.  Activate the thread_db target
     if this is the first process using it.  */
  if (thread_db_list->next == NULL)
    push_target (&the_thread_db_target);

  return 1;
}

/* Attempt to use LIBRARY as libthread_db.  LIBRARY could be absolute,
   relative, or just LIBTHREAD_DB.  */

static int
try_thread_db_load (const char *library, int check_auto_load_safe)
{
  void *handle;
  struct thread_db_info *info;

  if (libthread_db_debug)
    fprintf_unfiltered (gdb_stdlog,
			_("Trying host libthread_db library: %s.\n"),
			library);

  if (check_auto_load_safe)
    {
      if (access (library, R_OK) != 0)
	{
	  /* Do not print warnings by file_is_auto_load_safe if the library does
	     not exist at this place.  */
	  if (libthread_db_debug)
	    fprintf_unfiltered (gdb_stdlog, _("open failed: %s.\n"),
				safe_strerror (errno));
	  return 0;
	}

      if (!file_is_auto_load_safe (library, _("auto-load: Loading libthread-db "
					      "library \"%s\" from explicit "
					      "directory.\n"),
				   library))
	return 0;
    }

  handle = dlopen (library, RTLD_NOW);
  if (handle == NULL)
    {
      if (libthread_db_debug)
	fprintf_unfiltered (gdb_stdlog, _("dlopen failed: %s.\n"), dlerror ());
      return 0;
    }

  if (libthread_db_debug && strchr (library, '/') == NULL)
    {
      void *td_init;

      td_init = dlsym (handle, "td_init");
      if (td_init != NULL)
        {
          const char *const libpath = dladdr_to_soname (td_init);

          if (libpath != NULL)
            fprintf_unfiltered (gdb_stdlog, _("Host %s resolved to: %s.\n"),
                               library, libpath);
        }
    }

  info = add_thread_db_info (handle);

  /* Do not save system library name, that one is always trusted.  */
  if (strchr (library, '/') != NULL)
    info->filename = gdb_realpath (library).release ();

  if (try_thread_db_load_1 (info))
    return 1;

  /* This library "refused" to work on current inferior.  */
  delete_thread_db_info (inferior_ptid.pid ());
  return 0;
}

/* Subroutine of try_thread_db_load_from_pdir to simplify it.
   Try loading libthread_db in directory(OBJ)/SUBDIR.
   SUBDIR may be NULL.  It may also be something like "../lib64".
   The result is true for success.  */

static int
try_thread_db_load_from_pdir_1 (struct objfile *obj, const char *subdir)
{
  const char *obj_name = objfile_name (obj);

  if (obj_name[0] != '/')
    {
      warning (_("Expected absolute pathname for libpthread in the"
		 " inferior, but got %s."), obj_name);
      return 0;
    }

  std::string path = obj_name;
  size_t cp = path.rfind ('/');
  /* This should at minimum hit the first character.  */
  gdb_assert (cp != std::string::npos);
  path.resize (cp + 1);
  if (subdir != NULL)
    path = path + subdir + "/";
  path += LIBTHREAD_DB_SO;

  return try_thread_db_load (path.c_str (), 1);
}

/* Handle $pdir in libthread-db-search-path.
   Look for libthread_db in directory(libpthread)/SUBDIR.
   SUBDIR may be NULL.  It may also be something like "../lib64".
   The result is true for success.  */

static int
try_thread_db_load_from_pdir (const char *subdir)
{
  struct objfile *obj;

  if (!auto_load_thread_db)
    return 0;

  ALL_OBJFILES (obj)
    if (libpthread_name_p (objfile_name (obj)))
      {
	if (try_thread_db_load_from_pdir_1 (obj, subdir))
	  return 1;

	/* We may have found the separate-debug-info version of
	   libpthread, and it may live in a directory without a matching
	   libthread_db.  */
	if (obj->separate_debug_objfile_backlink != NULL)
	  return try_thread_db_load_from_pdir_1 (obj->separate_debug_objfile_backlink,
						 subdir);

	return 0;
      }

  return 0;
}

/* Handle $sdir in libthread-db-search-path.
   Look for libthread_db in the system dirs, or wherever a plain
   dlopen(file_without_path) will look.
   The result is true for success.  */

static int
try_thread_db_load_from_sdir (void)
{
  return try_thread_db_load (LIBTHREAD_DB_SO, 0);
}

/* Try to load libthread_db from directory DIR of length DIR_LEN.
   The result is true for success.  */

static int
try_thread_db_load_from_dir (const char *dir, size_t dir_len)
{
  if (!auto_load_thread_db)
    return 0;

  std::string path = std::string (dir, dir_len) + "/" + LIBTHREAD_DB_SO;

  return try_thread_db_load (path.c_str (), 1);
}

/* Search libthread_db_search_path for libthread_db which "agrees"
   to work on current inferior.
   The result is true for success.  */

static int
thread_db_load_search (void)
{
  int rc = 0;

  std::vector<gdb::unique_xmalloc_ptr<char>> dir_vec
    = dirnames_to_char_ptr_vec (libthread_db_search_path);

  for (const gdb::unique_xmalloc_ptr<char> &this_dir_up : dir_vec)
    {
      const char *this_dir = this_dir_up.get ();
      const int pdir_len = sizeof ("$pdir") - 1;
      size_t this_dir_len;

      this_dir_len = strlen (this_dir);

      if (strncmp (this_dir, "$pdir", pdir_len) == 0
	  && (this_dir[pdir_len] == '\0'
	      || this_dir[pdir_len] == '/'))
	{
	  const char *subdir = NULL;

	  std::string subdir_holder;
	  if (this_dir[pdir_len] == '/')
	    {
	      subdir_holder = std::string (this_dir + pdir_len + 1);
	      subdir = subdir_holder.c_str ();
	    }
	  rc = try_thread_db_load_from_pdir (subdir);
	  if (rc)
	    break;
	}
      else if (strcmp (this_dir, "$sdir") == 0)
	{
	  if (try_thread_db_load_from_sdir ())
	    {
	      rc = 1;
	      break;
	    }
	}
      else
	{
	  if (try_thread_db_load_from_dir (this_dir, this_dir_len))
	    {
	      rc = 1;
	      break;
	    }
	}
    }

  if (libthread_db_debug)
    fprintf_unfiltered (gdb_stdlog,
			_("thread_db_load_search returning %d\n"), rc);
  return rc;
}

/* Return non-zero if the inferior has a libpthread.  */

static int
has_libpthread (void)
{
  struct objfile *obj;

  ALL_OBJFILES (obj)
    if (libpthread_name_p (objfile_name (obj)))
      return 1;

  return 0;
}

/* Attempt to load and initialize libthread_db.
   Return 1 on success.  */

static int
thread_db_load (void)
{
  struct thread_db_info *info;

  info = get_thread_db_info (inferior_ptid.pid ());

  if (info != NULL)
    return 1;

  /* Don't attempt to use thread_db on executables not running
     yet.  */
  if (!target_has_registers)
    return 0;

  /* Don't attempt to use thread_db for remote targets.  */
  if (!(target_can_run () || core_bfd))
    return 0;

  if (thread_db_load_search ())
    return 1;

  /* We couldn't find a libthread_db.
     If the inferior has a libpthread warn the user.  */
  if (has_libpthread ())
    {
      warning (_("Unable to find libthread_db matching inferior's thread"
		 " library, thread debugging will not be available."));
      return 0;
    }

  /* Either this executable isn't using libpthread at all, or it is
     statically linked.  Since we can't easily distinguish these two cases,
     no warning is issued.  */
  return 0;
}

static void
check_thread_signals (void)
{
  if (!thread_signals)
    {
      sigset_t mask;
      int i;

      lin_thread_get_thread_signals (&mask);
      sigemptyset (&thread_stop_set);
      sigemptyset (&thread_print_set);

      for (i = 1; i < NSIG; i++)
	{
	  if (sigismember (&mask, i))
	    {
	      if (signal_stop_update (gdb_signal_from_host (i), 0))
		sigaddset (&thread_stop_set, i);
	      if (signal_print_update (gdb_signal_from_host (i), 0))
		sigaddset (&thread_print_set, i);
	      thread_signals = 1;
	    }
	}
    }
}

/* Check whether thread_db is usable.  This function is called when
   an inferior is created (or otherwise acquired, e.g. attached to)
   and when new shared libraries are loaded into a running process.  */

void
check_for_thread_db (void)
{
  /* Do nothing if we couldn't load libthread_db.so.1.  */
  if (!thread_db_load ())
    return;
}

/* This function is called via the new_objfile observer.  */

static void
thread_db_new_objfile (struct objfile *objfile)
{
  /* This observer must always be called with inferior_ptid set
     correctly.  */

  if (objfile != NULL
      /* libpthread with separate debug info has its debug info file already
	 loaded (and notified without successful thread_db initialization)
	 the time gdb::observers::new_objfile.notify is called for the library itself.
	 Static executables have their separate debug info loaded already
	 before the inferior has started.  */
      && objfile->separate_debug_objfile_backlink == NULL
      /* Only check for thread_db if we loaded libpthread,
	 or if this is the main symbol file.
	 We need to check OBJF_MAINLINE to handle the case of debugging
	 a statically linked executable AND the symbol file is specified AFTER
	 the exec file is loaded (e.g., gdb -c core ; file foo).
	 For dynamically linked executables, libpthread can be near the end
	 of the list of shared libraries to load, and in an app of several
	 thousand shared libraries, this can otherwise be painful.  */
      && ((objfile->flags & OBJF_MAINLINE) != 0
	  || libpthread_name_p (objfile_name (objfile))))
    check_for_thread_db ();
}

static void
check_pid_namespace_match (void)
{
  /* Check is only relevant for local targets targets.  */
  if (target_can_run ())
    {
      /* If the child is in a different PID namespace, its idea of its
	 PID will differ from our idea of its PID.  When we scan the
	 child's thread list, we'll mistakenly think it has no threads
	 since the thread PID fields won't match the PID we give to
	 libthread_db.  */
      if (!linux_ns_same (inferior_ptid.pid (), LINUX_NS_PID))
	{
	  warning (_ ("Target and debugger are in different PID "
		      "namespaces; thread lists and other data are "
		      "likely unreliable.  "
		      "Connect to gdbserver inside the container."));
	}
    }
}

/* This function is called via the inferior_created observer.
   This handles the case of debugging statically linked executables.  */

static void
thread_db_inferior_created (struct target_ops *target, int from_tty)
{
  check_pid_namespace_match ();
  check_for_thread_db ();
}

/* Update the thread's state (what's displayed in "info threads"),
   from libthread_db thread state information.  */

static void
update_thread_state (thread_db_thread_info *priv,
		     const td_thrinfo_t *ti_p)
{
  priv->dying = (ti_p->ti_state == TD_THR_UNKNOWN
		 || ti_p->ti_state == TD_THR_ZOMBIE);
}

/* Record a new thread in GDB's thread list.  Creates the thread's
   private info.  If TP is NULL or TP is marked as having exited,
   creates a new thread.  Otherwise, uses TP.  */

static struct thread_info *
record_thread (struct thread_db_info *info,
	       struct thread_info *tp,
	       ptid_t ptid, const td_thrhandle_t *th_p,
	       const td_thrinfo_t *ti_p)
{
  /* A thread ID of zero may mean the thread library has not
     initialized yet.  Leave private == NULL until the thread library
     has initialized.  */
  if (ti_p->ti_tid == 0)
    return tp;

  /* Construct the thread's private data.  */
  thread_db_thread_info *priv = new thread_db_thread_info;

  priv->th = *th_p;
  priv->tid = ti_p->ti_tid;
  update_thread_state (priv, ti_p);

  /* Add the thread to GDB's thread list.  If we already know about a
     thread with this PTID, but it's marked exited, then the kernel
     reused the tid of an old thread.  */
  if (tp == NULL || tp->state == THREAD_EXITED)
    tp = add_thread_with_info (ptid, priv);
  else
    tp->priv.reset (priv);

  if (target_has_execution)
    check_thread_signals ();

  return tp;
}

void
thread_db_target::detach (inferior *inf, int from_tty)
{
  delete_thread_db_info (inf->pid);

  beneath ()->detach (inf, from_tty);

  /* NOTE: From this point on, inferior_ptid is null_ptid.  */

  /* If there are no more processes using libpthread, detach the
     thread_db target ops.  */
  if (!thread_db_list)
    unpush_target (this);
}

ptid_t
thread_db_target::wait (ptid_t ptid, struct target_waitstatus *ourstatus,
			int options)
{
  struct thread_db_info *info;

  ptid = beneath ()->wait (ptid, ourstatus, options);

  switch (ourstatus->kind)
    {
    case TARGET_WAITKIND_IGNORE:
    case TARGET_WAITKIND_EXITED:
    case TARGET_WAITKIND_THREAD_EXITED:
    case TARGET_WAITKIND_SIGNALLED:
      return ptid;
    }

  info = get_thread_db_info (ptid.pid ());

  /* If this process isn't using thread_db, we're done.  */
  if (info == NULL)
    return ptid;

  if (ourstatus->kind == TARGET_WAITKIND_EXECD)
    {
      /* New image, it may or may not end up using thread_db.  Assume
	 not unless we find otherwise.  */
      delete_thread_db_info (ptid.pid ());
      if (!thread_db_list)
	unpush_target (&the_thread_db_target);

      return ptid;
    }

  /* Fill in the thread's user-level thread id and status.  */
  thread_from_lwp (find_thread_ptid (ptid), ptid);

  return ptid;
}

void
thread_db_target::mourn_inferior ()
{
  delete_thread_db_info (inferior_ptid.pid ());

  beneath ()->mourn_inferior ();

  /* Detach thread_db target ops.  */
  if (!thread_db_list)
    unpush_target (&the_thread_db_target);
}

struct callback_data
{
  struct thread_db_info *info;
  int new_threads;
};

static int
find_new_threads_callback (const td_thrhandle_t *th_p, void *data)
{
  td_thrinfo_t ti;
  td_err_e err;
  struct thread_info *tp;
  struct callback_data *cb_data = (struct callback_data *) data;
  struct thread_db_info *info = cb_data->info;

  err = info->td_thr_get_info_p (th_p, &ti);
  if (err != TD_OK)
    error (_("find_new_threads_callback: cannot get thread info: %s"),
	   thread_db_err_str (err));

  if (ti.ti_lid == -1)
    {
      /* A thread with kernel thread ID -1 is either a thread that
	 exited and was joined, or a thread that is being created but
	 hasn't started yet, and that is reusing the tcb/stack of a
	 thread that previously exited and was joined.  (glibc marks
	 terminated and joined threads with kernel thread ID -1.  See
	 glibc PR17707.  */
      if (libthread_db_debug)
	fprintf_unfiltered (gdb_stdlog,
			    "thread_db: skipping exited and "
			    "joined thread (0x%lx)\n",
			    (unsigned long) ti.ti_tid);
      return 0;
    }

  if (ti.ti_tid == 0)
    {
      /* A thread ID of zero means that this is the main thread, but
	 glibc has not yet initialized thread-local storage and the
	 pthread library.  We do not know what the thread's TID will
	 be yet.  */

      /* In that case, we're not stopped in a fork syscall and don't
	 need this glibc bug workaround.  */
      info->need_stale_parent_threads_check = 0;

      return 0;
    }

  /* Ignore stale parent threads, caused by glibc/BZ5983.  This is a
     bit expensive, as it needs to open /proc/pid/status, so try to
     avoid doing the work if we know we don't have to.  */
  if (info->need_stale_parent_threads_check)
    {
      int tgid = linux_proc_get_tgid (ti.ti_lid);

      if (tgid != -1 && tgid != info->pid)
	return 0;
    }

  ptid_t ptid (info->pid, ti.ti_lid);
  tp = find_thread_ptid (ptid);
  if (tp == NULL || tp->priv == NULL)
    record_thread (info, tp, ptid, th_p, &ti);

  return 0;
}

/* Helper for thread_db_find_new_threads_2.
   Returns number of new threads found.  */

static int
find_new_threads_once (struct thread_db_info *info, int iteration,
		       td_err_e *errp)
{
  struct callback_data data;
  td_err_e err = TD_ERR;

  data.info = info;
  data.new_threads = 0;

  /* See comment in thread_db_update_thread_list.  */
  gdb_assert (info->td_ta_thr_iter_p != NULL);

  TRY
    {
      /* Iterate over all user-space threads to discover new threads.  */
      err = info->td_ta_thr_iter_p (info->thread_agent,
				    find_new_threads_callback,
				    &data,
				    TD_THR_ANY_STATE,
				    TD_THR_LOWEST_PRIORITY,
				    TD_SIGNO_MASK,
				    TD_THR_ANY_USER_FLAGS);
    }
  CATCH (except, RETURN_MASK_ERROR)
    {
      if (libthread_db_debug)
	{
	  exception_fprintf (gdb_stdlog, except,
			     "Warning: find_new_threads_once: ");
	}
    }
  END_CATCH

  if (libthread_db_debug)
    {
      fprintf_unfiltered (gdb_stdlog,
			  _("Found %d new threads in iteration %d.\n"),
			  data.new_threads, iteration);
    }

  if (errp != NULL)
    *errp = err;

  return data.new_threads;
}

/* Search for new threads, accessing memory through stopped thread
   PTID.  If UNTIL_NO_NEW is true, repeat searching until several
   searches in a row do not discover any new threads.  */

static void
thread_db_find_new_threads_2 (thread_info *stopped, bool until_no_new)
{
  td_err_e err = TD_OK;
  struct thread_db_info *info;
  int i, loop;

  info = get_thread_db_info (stopped->ptid.pid ());

  /* Access an lwp we know is stopped.  */
  info->proc_handle.thread = stopped;

  if (until_no_new)
    {
      /* Require 4 successive iterations which do not find any new threads.
	 The 4 is a heuristic: there is an inherent race here, and I have
	 seen that 2 iterations in a row are not always sufficient to
	 "capture" all threads.  */
      for (i = 0, loop = 0; loop < 4 && err == TD_OK; ++i, ++loop)
	if (find_new_threads_once (info, i, &err) != 0)
	  {
	    /* Found some new threads.  Restart the loop from beginning.  */
	    loop = -1;
	  }
    }
  else
    find_new_threads_once (info, 0, &err);

  if (err != TD_OK)
    error (_("Cannot find new threads: %s"), thread_db_err_str (err));
}

static void
thread_db_find_new_threads_1 (thread_info *stopped)
{
  thread_db_find_new_threads_2 (stopped, 0);
}

/* Implement the to_update_thread_list target method for this
   target.  */

void
thread_db_target::update_thread_list ()
{
  struct thread_db_info *info;

  prune_threads ();

  for (inferior *inf : all_inferiors ())
    {
      struct thread_info *thread;

      if (inf->pid == 0)
	continue;

      info = get_thread_db_info (inf->pid);
      if (info == NULL)
	continue;

      thread = any_live_thread_of_inferior (inf);
      if (thread == NULL || thread->executing)
	continue;

      /* It's best to avoid td_ta_thr_iter if possible.  That walks
	 data structures in the inferior's address space that may be
	 corrupted, or, if the target is running, the list may change
	 while we walk it.  In the latter case, it's possible that a
	 thread exits just at the exact time that causes GDB to get
	 stuck in an infinite loop.  To avoid pausing all threads
	 whenever the core wants to refresh the thread list, we
	 instead use thread_from_lwp immediately when we see an LWP
	 stop.  That uses thread_db entry points that do not walk
	 libpthread's thread list, so should be safe, as well as more
	 efficient.  */
      if (target_has_execution_1 (thread->ptid))
	continue;

      thread_db_find_new_threads_1 (thread);
    }

  /* Give the beneath target a chance to do extra processing.  */
  this->beneath ()->update_thread_list ();
}

const char *
thread_db_target::pid_to_str (ptid_t ptid)
{
  struct thread_info *thread_info = find_thread_ptid (ptid);

  if (thread_info != NULL && thread_info->priv != NULL)
    {
      static char buf[64];
      thread_db_thread_info *priv = get_thread_db_thread_info (thread_info);

      snprintf (buf, sizeof (buf), "Thread 0x%lx (LWP %ld)",
		(unsigned long) priv->tid, ptid.lwp ());

      return buf;
    }

  return beneath ()->pid_to_str (ptid);
}

/* Return a string describing the state of the thread specified by
   INFO.  */

const char *
thread_db_target::extra_thread_info (thread_info *info)
{
  if (info->priv == NULL)
    return NULL;

  thread_db_thread_info *priv = get_thread_db_thread_info (info);

  if (priv->dying)
    return "Exiting";

  return NULL;
}

/* Return pointer to the thread_info struct which corresponds to
   THREAD_HANDLE (having length HANDLE_LEN).  */

thread_info *
thread_db_target::thread_handle_to_thread_info (const gdb_byte *thread_handle,
						int handle_len,
						inferior *inf)
{
  thread_t handle_tid;

  /* Thread handle sizes must match in order to proceed.  We don't use an
     assert here because the resulting internal error will cause GDB to
     exit.  This isn't necessarily an internal error due to the possibility
     of garbage being passed as the thread handle via the python interface.  */
  if (handle_len != sizeof (handle_tid))
    error (_("Thread handle size mismatch: %d vs %zu (from libthread_db)"),
	   handle_len, sizeof (handle_tid));

  handle_tid = * (const thread_t *) thread_handle;

  for (thread_info *tp : inf->non_exited_threads ())
    {
      thread_db_thread_info *priv = get_thread_db_thread_info (tp);

      if (priv != NULL && handle_tid == priv->tid)
        return tp;
    }

  return NULL;
}

/* Get the address of the thread local variable in load module LM which
   is stored at OFFSET within the thread local storage for thread PTID.  */

CORE_ADDR
thread_db_target::get_thread_local_address (ptid_t ptid,
					    CORE_ADDR lm,
					    CORE_ADDR offset)
{
  struct thread_info *thread_info;

  /* Find the matching thread.  */
  thread_info = find_thread_ptid (ptid);

  /* We may not have discovered the thread yet.  */
  if (thread_info != NULL && thread_info->priv == NULL)
    thread_info = thread_from_lwp (thread_info, ptid);

  if (thread_info != NULL && thread_info->priv != NULL)
    {
      td_err_e err;
      psaddr_t address;
      thread_db_info *info = get_thread_db_info (ptid.pid ());
      thread_db_thread_info *priv = get_thread_db_thread_info (thread_info);

      /* Finally, get the address of the variable.  */
      if (lm != 0)
	{
	  /* glibc doesn't provide the needed interface.  */
	  if (!info->td_thr_tls_get_addr_p)
	    throw_error (TLS_NO_LIBRARY_SUPPORT_ERROR,
			 _("No TLS library support"));

	  /* Note the cast through uintptr_t: this interface only works if
	     a target address fits in a psaddr_t, which is a host pointer.
	     So a 32-bit debugger can not access 64-bit TLS through this.  */
	  err = info->td_thr_tls_get_addr_p (&priv->th,
					     (psaddr_t)(uintptr_t) lm,
					     offset, &address);
	}
      else
	{
	  /* If glibc doesn't provide the needed interface throw an error
	     that LM is zero - normally cases it should not be.  */
	  if (!info->td_thr_tlsbase_p)
	    throw_error (TLS_LOAD_MODULE_NOT_FOUND_ERROR,
			 _("TLS load module not found"));

	  /* This code path handles the case of -static -pthread executables:
	     https://sourceware.org/ml/libc-help/2014-03/msg00024.html
	     For older GNU libc r_debug.r_map is NULL.  For GNU libc after
	     PR libc/16831 due to GDB PR threads/16954 LOAD_MODULE is also NULL.
	     The constant number 1 depends on GNU __libc_setup_tls
	     initialization of l_tls_modid to 1.  */
	  err = info->td_thr_tlsbase_p (&priv->th, 1, &address);
	  address = (char *) address + offset;
	}

#ifdef THREAD_DB_HAS_TD_NOTALLOC
      /* The memory hasn't been allocated, yet.  */
      if (err == TD_NOTALLOC)
	  /* Now, if libthread_db provided the initialization image's
	     address, we *could* try to build a non-lvalue value from
	     the initialization image.  */
        throw_error (TLS_NOT_ALLOCATED_YET_ERROR,
                     _("TLS not allocated yet"));
#endif

      /* Something else went wrong.  */
      if (err != TD_OK)
        throw_error (TLS_GENERIC_ERROR,
                     (("%s")), thread_db_err_str (err));

      /* Cast assuming host == target.  Joy.  */
      /* Do proper sign extension for the target.  */
      gdb_assert (exec_bfd);
      return (bfd_get_sign_extend_vma (exec_bfd) > 0
	      ? (CORE_ADDR) (intptr_t) address
	      : (CORE_ADDR) (uintptr_t) address);
    }

  return beneath ()->get_thread_local_address (ptid, lm, offset);
}

/* Implement the to_get_ada_task_ptid target method for this target.  */

ptid_t
thread_db_target::get_ada_task_ptid (long lwp, long thread)
{
  /* NPTL uses a 1:1 model, so the LWP id suffices.  */
  return ptid_t (inferior_ptid.pid (), lwp, 0);
}

void
thread_db_target::resume (ptid_t ptid, int step, enum gdb_signal signo)
{
  struct thread_db_info *info;

  if (ptid == minus_one_ptid)
    info = get_thread_db_info (inferior_ptid.pid ());
  else
    info = get_thread_db_info (ptid.pid ());

  /* This workaround is only needed for child fork lwps stopped in a
     PTRACE_O_TRACEFORK event.  When the inferior is resumed, the
     workaround can be disabled.  */
  if (info)
    info->need_stale_parent_threads_check = 0;

  beneath ()->resume (ptid, step, signo);
}

/* std::sort helper function for info_auto_load_libthread_db, sort the
   thread_db_info pointers primarily by their FILENAME and secondarily by their
   PID, both in ascending order.  */

static bool
info_auto_load_libthread_db_compare (const struct thread_db_info *a,
				     const struct thread_db_info *b)
{
  int retval;

  retval = strcmp (a->filename, b->filename);
  if (retval)
    return retval < 0;

  return a->pid < b->pid;
}

/* Implement 'info auto-load libthread-db'.  */

static void
info_auto_load_libthread_db (const char *args, int from_tty)
{
  struct ui_out *uiout = current_uiout;
  const char *cs = args ? args : "";
  struct thread_db_info *info;
  unsigned unique_filenames;
  size_t max_filename_len, pids_len;
  int i;

  cs = skip_spaces (cs);
  if (*cs)
    error (_("'info auto-load libthread-db' does not accept any parameters"));

  std::vector<struct thread_db_info *> array;
  for (info = thread_db_list; info; info = info->next)
    if (info->filename != NULL)
      array.push_back (info);

  /* Sort ARRAY by filenames and PIDs.  */
  std::sort (array.begin (), array.end (),
	     info_auto_load_libthread_db_compare);

  /* Calculate the number of unique filenames (rows) and the maximum string
     length of PIDs list for the unique filenames (columns).  */

  unique_filenames = 0;
  max_filename_len = 0;
  pids_len = 0;
  for (i = 0; i < array.size (); i++)
    {
      int pid = array[i]->pid;
      size_t this_pid_len;

      for (this_pid_len = 0; pid != 0; pid /= 10)
	this_pid_len++;

      if (i == 0 || strcmp (array[i - 1]->filename, array[i]->filename) != 0)
	{
	  unique_filenames++;
	  max_filename_len = std::max (max_filename_len,
				       strlen (array[i]->filename));

	  if (i > 0)
	    pids_len -= strlen (", ");
	  pids_len = 0;
	}
      pids_len += this_pid_len + strlen (", ");
    }
  if (i)
    pids_len -= strlen (", ");

  /* Table header shifted right by preceding "libthread-db:  " would not match
     its columns.  */
  if (array.size () > 0 && args == auto_load_info_scripts_pattern_nl)
    uiout->text ("\n");

  {
    ui_out_emit_table table_emitter (uiout, 2, unique_filenames,
				     "LinuxThreadDbTable");

    uiout->table_header (max_filename_len, ui_left, "filename", "Filename");
    uiout->table_header (pids_len, ui_left, "PIDs", "Pids");
    uiout->table_body ();

    /* Note I is incremented inside the cycle, not at its end.  */
    for (i = 0; i < array.size ();)
      {
	ui_out_emit_tuple tuple_emitter (uiout, NULL);

	info = array[i];
	uiout->field_string ("filename", info->filename);

	std::string pids;
	while (i < array.size () && strcmp (info->filename,
					    array[i]->filename) == 0)
	  {
	    if (!pids.empty ())
	      pids += ", ";
	    string_appendf (pids, "%u", array[i]->pid);
	    i++;
	  }

	uiout->field_string ("pids", pids.c_str ());

	uiout->text ("\n");
      }
  }

  if (array.empty ())
    uiout->message (_("No auto-loaded libthread-db.\n"));
}

/* Implement 'maintenance check libthread-db'.  */

static void
maintenance_check_libthread_db (const char *args, int from_tty)
{
  int inferior_pid = inferior_ptid.pid ();
  struct thread_db_info *info;

  if (inferior_pid == 0)
    error (_("No inferior running"));

  info = get_thread_db_info (inferior_pid);
  if (info == NULL)
    error (_("No libthread_db loaded"));

  check_thread_db (info, true);
}

void
_initialize_thread_db (void)
{
  /* Defer loading of libthread_db.so until inferior is running.
     This allows gdb to load correct libthread_db for a given
     executable -- there could be multiple versions of glibc,
     and until there is a running inferior, we can't tell which
     libthread_db is the correct one to load.  */

  libthread_db_search_path = xstrdup (LIBTHREAD_DB_SEARCH_PATH);

  add_setshow_optional_filename_cmd ("libthread-db-search-path",
				     class_support,
				     &libthread_db_search_path, _("\
Set search path for libthread_db."), _("\
Show the current search path or libthread_db."), _("\
This path is used to search for libthread_db to be loaded into \
gdb itself.\n\
Its value is a colon (':') separate list of directories to search.\n\
Setting the search path to an empty list resets it to its default value."),
			    set_libthread_db_search_path,
			    NULL,
			    &setlist, &showlist);

  add_setshow_zuinteger_cmd ("libthread-db", class_maintenance,
			     &libthread_db_debug, _("\
Set libthread-db debugging."), _("\
Show libthread-db debugging."), _("\
When non-zero, libthread-db debugging is enabled."),
			     NULL,
			     show_libthread_db_debug,
			     &setdebuglist, &showdebuglist);

  add_setshow_boolean_cmd ("libthread-db", class_support,
			   &auto_load_thread_db, _("\
Enable or disable auto-loading of inferior specific libthread_db."), _("\
Show whether auto-loading inferior specific libthread_db is enabled."), _("\
If enabled, libthread_db will be searched in 'set libthread-db-search-path'\n\
locations to load libthread_db compatible with the inferior.\n\
Standard system libthread_db still gets loaded even with this option off.\n\
This options has security implications for untrusted inferiors."),
			   NULL, show_auto_load_thread_db,
			   auto_load_set_cmdlist_get (),
			   auto_load_show_cmdlist_get ());

  add_cmd ("libthread-db", class_info, info_auto_load_libthread_db,
	   _("Print the list of loaded inferior specific libthread_db.\n\
Usage: info auto-load libthread-db"),
	   auto_load_info_cmdlist_get ());

  add_cmd ("libthread-db", class_maintenance,
	   maintenance_check_libthread_db, _("\
Run integrity checks on the current inferior's libthread_db."),
	   &maintenancechecklist);

  add_setshow_boolean_cmd ("check-libthread-db",
			   class_maintenance,
			   &check_thread_db_on_load, _("\
Set whether to check libthread_db at load time."), _("\
Show whether to check libthread_db at load time."), _("\
If enabled GDB will run integrity checks on inferior specific libthread_db\n\
as they are loaded."),
			   NULL,
			   NULL,
			   &maintenance_set_cmdlist,
			   &maintenance_show_cmdlist);

  /* Add ourselves to objfile event chain.  */
  gdb::observers::new_objfile.attach (thread_db_new_objfile);

  /* Add ourselves to inferior_created event chain.
     This is needed to handle debugging statically linked programs where
     the new_objfile observer won't get called for libpthread.  */
  gdb::observers::inferior_created.attach (thread_db_inferior_created);
}
