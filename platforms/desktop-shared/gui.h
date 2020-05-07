/*
 * Gearsystem - Sega Master System / Game Gear Emulator
 * Copyright (C) 2013  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#ifndef GUI_H
#define	GUI_H

#ifdef GUI_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

enum gui_ShortCutEvent
{
    gui_ShortcutOpenROM = 0,
    gui_ShortcutReset,
    gui_ShortcutPause,
    gui_ShortcutFFWD,
    gui_ShortcutSaveState,
    gui_ShortcutLoadState,
};

EXTERN bool gui_in_use;

EXTERN void gui_init(void);
EXTERN void gui_destroy(void);
EXTERN void gui_render(void);
EXTERN void gui_shortcut(gui_ShortCutEvent event);
EXTERN void gui_load_rom(const char* path);

#undef GUI_IMPORT
#undef EXTERN
#endif	/* GUI_H */