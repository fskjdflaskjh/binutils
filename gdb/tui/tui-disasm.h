/* Disassembly display.

   Copyright (C) 1998-2019 Free Software Foundation, Inc.
   
   Contributed by Hewlett-Packard Company.

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

#ifndef TUI_TUI_DISASM_H
#define TUI_TUI_DISASM_H

#include "tui/tui.h"
#include "tui/tui-data.h"
#include "tui-winsource.h"

/* A TUI disassembly window.  */

struct tui_disasm_window : public tui_source_window_base
{
  tui_disasm_window ()
    : tui_source_window_base (DISASSEM_WIN)
  {
  }

  DISABLE_COPY_AND_ASSIGN (tui_disasm_window);

  const char *name () const override
  {
    return DISASSEM_NAME;
  }

  bool location_matches_p (struct bp_location *loc, int line_no) override;

protected:

  void do_scroll_vertical (int num_to_scroll) override;
};

extern enum tui_status tui_set_disassem_content (tui_source_window_base *,
						 struct gdbarch *, CORE_ADDR);
extern void tui_show_disassem (struct gdbarch *, CORE_ADDR);
extern void tui_show_disassem_and_update_source (struct gdbarch *, CORE_ADDR);
extern void tui_get_begin_asm_address (struct gdbarch **, CORE_ADDR *);

#endif /* TUI_TUI_DISASM_H */
