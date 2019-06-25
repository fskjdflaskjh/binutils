/* Data/register window display.

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

#ifndef TUI_TUI_WINDATA_H
#define TUI_TUI_WINDATA_H

#include "tui/tui-data.h"

extern void tui_erase_data_content (const char *);
extern void tui_display_all_data (void);
extern void tui_display_data_from_line (int);
extern void tui_delete_data_content_windows (void);
extern void tui_refresh_data_win (void);
extern void tui_display_data_from (int, int);

#endif /* TUI_TUI_WINDATA_H */
