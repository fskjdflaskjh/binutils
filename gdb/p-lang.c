/* Pascal language support routines for GDB, the GNU debugger.

   Copyright (C) 2000-2020 Free Software Foundation, Inc.

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

/* This file is derived from c-lang.c */

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "parser-defs.h"
#include "language.h"
#include "varobj.h"
#include "p-lang.h"
#include "valprint.h"
#include "value.h"
#include <ctype.h>
#include "c-lang.h"
#include "gdbarch.h"
#include "cli/cli-style.h"

/* All GPC versions until now (2007-09-27) also define a symbol called
   '_p_initialize'.  Check for the presence of this symbol first.  */
static const char GPC_P_INITIALIZE[] = "_p_initialize";

/* The name of the symbol that GPC uses as the name of the main
   procedure (since version 20050212).  */
static const char GPC_MAIN_PROGRAM_NAME_1[] = "_p__M0_main_program";

/* Older versions of GPC (versions older than 20050212) were using
   a different name for the main procedure.  */
static const char GPC_MAIN_PROGRAM_NAME_2[] = "pascal_main_program";

/* Function returning the special symbol name used
   by GPC for the main procedure in the main program
   if it is found in minimal symbol list.
   This function tries to find minimal symbols generated by GPC
   so that it finds the even if the program was compiled
   without debugging information.
   According to information supplied by Waldeck Hebisch,
   this should work for all versions posterior to June 2000.  */

const char *
pascal_main_name (void)
{
  struct bound_minimal_symbol msym;

  msym = lookup_minimal_symbol (GPC_P_INITIALIZE, NULL, NULL);

  /*  If '_p_initialize' was not found, the main program is likely not
     written in Pascal.  */
  if (msym.minsym == NULL)
    return NULL;

  msym = lookup_minimal_symbol (GPC_MAIN_PROGRAM_NAME_1, NULL, NULL);
  if (msym.minsym != NULL)
    {
      return GPC_MAIN_PROGRAM_NAME_1;
    }

  msym = lookup_minimal_symbol (GPC_MAIN_PROGRAM_NAME_2, NULL, NULL);
  if (msym.minsym != NULL)
    {
      return GPC_MAIN_PROGRAM_NAME_2;
    }

  /*  No known entry procedure found, the main program is probably
      not compiled with GPC.  */
  return NULL;
}

/* Determines if type TYPE is a pascal string type.
   Returns a positive value if the type is a known pascal string type.
   This function is used by p-valprint.c code to allow better string display.
   If it is a pascal string type, then it also sets info needed
   to get the length and the data of the string
   length_pos, length_size and string_pos are given in bytes.
   char_size gives the element size in bytes.
   FIXME: if the position or the size of these fields
   are not multiple of TARGET_CHAR_BIT then the results are wrong
   but this does not happen for Free Pascal nor for GPC.  */
int
is_pascal_string_type (struct type *type,int *length_pos,
		       int *length_size, int *string_pos,
		       struct type **char_type,
		       const char **arrayname)
{
  if (type != NULL && type->code () == TYPE_CODE_STRUCT)
    {
      /* Old Borland type pascal strings from Free Pascal Compiler.  */
      /* Two fields: length and st.  */
      if (type->num_fields () == 2
	  && TYPE_FIELD_NAME (type, 0)
	  && strcmp (TYPE_FIELD_NAME (type, 0), "length") == 0
	  && TYPE_FIELD_NAME (type, 1)
	  && strcmp (TYPE_FIELD_NAME (type, 1), "st") == 0)
	{
	  if (length_pos)
	    *length_pos = TYPE_FIELD_BITPOS (type, 0) / TARGET_CHAR_BIT;
	  if (length_size)
	    *length_size = TYPE_LENGTH (type->field (0).type ());
	  if (string_pos)
	    *string_pos = TYPE_FIELD_BITPOS (type, 1) / TARGET_CHAR_BIT;
	  if (char_type)
	    *char_type = TYPE_TARGET_TYPE (type->field (1).type ());
 	  if (arrayname)
	    *arrayname = TYPE_FIELD_NAME (type, 1);
	 return 2;
	};
      /* GNU pascal strings.  */
      /* Three fields: Capacity, length and schema$ or _p_schema.  */
      if (type->num_fields () == 3
	  && TYPE_FIELD_NAME (type, 0)
	  && strcmp (TYPE_FIELD_NAME (type, 0), "Capacity") == 0
	  && TYPE_FIELD_NAME (type, 1)
	  && strcmp (TYPE_FIELD_NAME (type, 1), "length") == 0)
	{
	  if (length_pos)
	    *length_pos = TYPE_FIELD_BITPOS (type, 1) / TARGET_CHAR_BIT;
	  if (length_size)
	    *length_size = TYPE_LENGTH (type->field (1).type ());
	  if (string_pos)
	    *string_pos = TYPE_FIELD_BITPOS (type, 2) / TARGET_CHAR_BIT;
	  /* FIXME: how can I detect wide chars in GPC ??  */
	  if (char_type)
	    {
	      *char_type = TYPE_TARGET_TYPE (type->field (2).type ());

	      if ((*char_type)->code () == TYPE_CODE_ARRAY)
		*char_type = TYPE_TARGET_TYPE (*char_type);
	    }
 	  if (arrayname)
	    *arrayname = TYPE_FIELD_NAME (type, 2);
	 return 3;
	};
    }
  return 0;
}

static void pascal_one_char (int, struct ui_file *, int *);

/* Print the character C on STREAM as part of the contents of a literal
   string.
   In_quotes is reset to 0 if a char is written with #4 notation.  */

static void
pascal_one_char (int c, struct ui_file *stream, int *in_quotes)
{
  if (c == '\'' || ((unsigned int) c <= 0xff && (PRINT_LITERAL_FORM (c))))
    {
      if (!(*in_quotes))
	fputs_filtered ("'", stream);
      *in_quotes = 1;
      if (c == '\'')
	{
	  fputs_filtered ("''", stream);
	}
      else
	fprintf_filtered (stream, "%c", c);
    }
  else
    {
      if (*in_quotes)
	fputs_filtered ("'", stream);
      *in_quotes = 0;
      fprintf_filtered (stream, "#%d", (unsigned int) c);
    }
}

void
pascal_printchar (int c, struct type *type, struct ui_file *stream)
{
  int in_quotes = 0;

  pascal_one_char (c, stream, &in_quotes);
  if (in_quotes)
    fputs_filtered ("'", stream);
}



/* Table mapping opcodes into strings for printing operators
   and precedences of the operators.  */

const struct op_print pascal_op_print_tab[] =
{
  {",", BINOP_COMMA, PREC_COMMA, 0},
  {":=", BINOP_ASSIGN, PREC_ASSIGN, 1},
  {"or", BINOP_BITWISE_IOR, PREC_BITWISE_IOR, 0},
  {"xor", BINOP_BITWISE_XOR, PREC_BITWISE_XOR, 0},
  {"and", BINOP_BITWISE_AND, PREC_BITWISE_AND, 0},
  {"=", BINOP_EQUAL, PREC_EQUAL, 0},
  {"<>", BINOP_NOTEQUAL, PREC_EQUAL, 0},
  {"<=", BINOP_LEQ, PREC_ORDER, 0},
  {">=", BINOP_GEQ, PREC_ORDER, 0},
  {">", BINOP_GTR, PREC_ORDER, 0},
  {"<", BINOP_LESS, PREC_ORDER, 0},
  {"shr", BINOP_RSH, PREC_SHIFT, 0},
  {"shl", BINOP_LSH, PREC_SHIFT, 0},
  {"+", BINOP_ADD, PREC_ADD, 0},
  {"-", BINOP_SUB, PREC_ADD, 0},
  {"*", BINOP_MUL, PREC_MUL, 0},
  {"/", BINOP_DIV, PREC_MUL, 0},
  {"div", BINOP_INTDIV, PREC_MUL, 0},
  {"mod", BINOP_REM, PREC_MUL, 0},
  {"@", BINOP_REPEAT, PREC_REPEAT, 0},
  {"-", UNOP_NEG, PREC_PREFIX, 0},
  {"not", UNOP_LOGICAL_NOT, PREC_PREFIX, 0},
  {"^", UNOP_IND, PREC_SUFFIX, 1},
  {"@", UNOP_ADDR, PREC_PREFIX, 0},
  {"sizeof", UNOP_SIZEOF, PREC_PREFIX, 0},
  {NULL, OP_NULL, PREC_PREFIX, 0}
};


/* Class representing the Pascal language.  */

class pascal_language : public language_defn
{
public:
  pascal_language ()
    : language_defn (language_pascal)
  { /* Nothing.  */ }

  /* See language.h.  */

  const char *name () const override
  { return "pascal"; }

  /* See language.h.  */

  const char *natural_name () const override
  { return "Pascal"; }

  /* See language.h.  */

  const std::vector<const char *> &filename_extensions () const override
  {
    static const std::vector<const char *> extensions
      = { ".pas", ".p", ".pp" };
    return extensions;
  }

  /* See language.h.  */
  void language_arch_info (struct gdbarch *gdbarch,
			   struct language_arch_info *lai) const override
  {
    const struct builtin_type *builtin = builtin_type (gdbarch);

    /* Helper function to allow shorter lines below.  */
    auto add  = [&] (struct type * t)
    {
      lai->add_primitive_type (t);
    };

    add (builtin->builtin_int);
    add (builtin->builtin_long);
    add (builtin->builtin_short);
    add (builtin->builtin_char);
    add (builtin->builtin_float);
    add (builtin->builtin_double);
    add (builtin->builtin_void);
    add (builtin->builtin_long_long);
    add (builtin->builtin_signed_char);
    add (builtin->builtin_unsigned_char);
    add (builtin->builtin_unsigned_short);
    add (builtin->builtin_unsigned_int);
    add (builtin->builtin_unsigned_long);
    add (builtin->builtin_unsigned_long_long);
    add (builtin->builtin_long_double);
    add (builtin->builtin_complex);
    add (builtin->builtin_double_complex);

    lai->set_string_char_type (builtin->builtin_char);
    lai->set_bool_type (builtin->builtin_bool, "boolean");
  }

  /* See language.h.  */

  void print_type (struct type *type, const char *varstring,
		   struct ui_file *stream, int show, int level,
		   const struct type_print_options *flags) const override
  {
    pascal_print_type (type, varstring, stream, show, level, flags);
  }

  /* See language.h.  */

  void value_print (struct value *val, struct ui_file *stream,
		    const struct value_print_options *options) const override
  {
    return pascal_value_print (val, stream, options);
  }

  /* See language.h.  */

  void value_print_inner
	(struct value *val, struct ui_file *stream, int recurse,
	 const struct value_print_options *options) const override
  {
    return pascal_value_print_inner (val, stream, recurse, options);
  }

  /* See language.h.  */

  int parser (struct parser_state *ps) const override
  {
    return pascal_parse (ps);
  }

  /* See language.h.  */

  void emitchar (int ch, struct type *chtype,
		 struct ui_file *stream, int quoter) const override
  {
    int in_quotes = 0;

    pascal_one_char (ch, stream, &in_quotes);
    if (in_quotes)
      fputs_filtered ("'", stream);
  }

  /* See language.h.  */

  void printchar (int ch, struct type *chtype,
		  struct ui_file *stream) const override
  {
    pascal_printchar (ch, chtype, stream);
  }

  /* See language.h.  */

  void printstr (struct ui_file *stream, struct type *elttype,
		 const gdb_byte *string, unsigned int length,
		 const char *encoding, int force_ellipses,
		 const struct value_print_options *options) const override
  {
    enum bfd_endian byte_order = type_byte_order (elttype);
    unsigned int i;
    unsigned int things_printed = 0;
    int in_quotes = 0;
    int need_comma = 0;
    int width;

    /* Preserve ELTTYPE's original type, just set its LENGTH.  */
    check_typedef (elttype);
    width = TYPE_LENGTH (elttype);

    /* If the string was not truncated due to `set print elements', and
       the last byte of it is a null, we don't print that, in traditional C
       style.  */
    if ((!force_ellipses) && length > 0
	&& extract_unsigned_integer (string + (length - 1) * width, width,
				     byte_order) == 0)
      length--;

    if (length == 0)
      {
	fputs_filtered ("''", stream);
	return;
      }

    for (i = 0; i < length && things_printed < options->print_max; ++i)
      {
	/* Position of the character we are examining
	   to see whether it is repeated.  */
	unsigned int rep1;
	/* Number of repetitions we have detected so far.  */
	unsigned int reps;
	unsigned long int current_char;

	QUIT;

	if (need_comma)
	  {
	    fputs_filtered (", ", stream);
	    need_comma = 0;
	  }

	current_char = extract_unsigned_integer (string + i * width, width,
						 byte_order);

	rep1 = i + 1;
	reps = 1;
	while (rep1 < length
	       && extract_unsigned_integer (string + rep1 * width, width,
					    byte_order) == current_char)
	  {
	    ++rep1;
	    ++reps;
	  }

	if (reps > options->repeat_count_threshold)
	  {
	    if (in_quotes)
	      {
		fputs_filtered ("', ", stream);
		in_quotes = 0;
	      }
	    pascal_printchar (current_char, elttype, stream);
	    fprintf_filtered (stream, " %p[<repeats %u times>%p]",
			      metadata_style.style ().ptr (),
			      reps, nullptr);
	    i = rep1 - 1;
	    things_printed += options->repeat_count_threshold;
	    need_comma = 1;
	  }
	else
	  {
	    if ((!in_quotes) && (PRINT_LITERAL_FORM (current_char)))
	      {
		fputs_filtered ("'", stream);
		in_quotes = 1;
	      }
	    pascal_one_char (current_char, stream, &in_quotes);
	    ++things_printed;
	  }
      }

    /* Terminate the quotes if necessary.  */
    if (in_quotes)
      fputs_filtered ("'", stream);

    if (force_ellipses || i < length)
      fputs_filtered ("...", stream);
  }

  /* See language.h.  */

  void print_typedef (struct type *type, struct symbol *new_symbol,
		      struct ui_file *stream) const override
  {
    pascal_print_typedef (type, new_symbol, stream);
  }

  /* See language.h.  */

  bool is_string_type_p (struct type *type) const override
  {
    return is_pascal_string_type (type, nullptr, nullptr, nullptr,
				  nullptr, nullptr) > 0;
  }

  /* See language.h.  */

  const char *name_of_this () const override
  { return "this"; }

  /* See language.h.  */

  bool range_checking_on_by_default () const override
  { return true; }

  /* See language.h.  */

  const struct op_print *opcode_print_table () const override
  { return pascal_op_print_tab; }
};

/* Single instance of the Pascal language class.  */

static pascal_language pascal_language_defn;
