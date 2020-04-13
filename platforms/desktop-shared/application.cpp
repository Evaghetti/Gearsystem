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

#include <SDL.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "emu.h"
#include "gui.h"
#include "config.h"
#include "renderer.h"

#define APPLICATION_IMPORT
#include "application.h"

static SDL_Window* sdl_window;
static SDL_GLContext gl_context;
static bool running = true;
static Uint64 frame_time_start;
static Uint64 frame_time_end;

static int sdl_init(void);
static void sdl_destroy(void);
static void sdl_events(void);
static void sdl_events_emu(const SDL_Event* event);
static void sdl_shortcuts_gui(const SDL_Event* event);
static void run_emulator(void);
static void render(void);
static void frame_throttle(void);

int application_init(void)
{
    int ret = sdl_init();
    
    config_init();
    config_read();

    emu_init(config_root_path);
    
    gui_init();

    ImGui_ImplSDL2_InitForOpenGL(sdl_window, gl_context);

    renderer_init();

    SDL_GL_SetSwapInterval(config_video.sync ? 1 : 0);

    return ret;
}

void application_destroy(void)
{
    config_write();
    config_destroy();
    renderer_destroy();
    gui_destroy();
    emu_destroy();
    sdl_destroy();
}

void application_mainloop(void)
{
    while (running)
    {
        frame_time_start = SDL_GetPerformanceCounter();
        sdl_events();
        run_emulator();
        render();
        frame_time_end = SDL_GetPerformanceCounter();
        frame_throttle();
    }
}

void application_trigger_quit(void)
{
    SDL_Event event;
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}

static int sdl_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        Log("Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_VERSION(&application_sdl_build_version);
    SDL_GetVersion(&application_sdl_link_version);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    sdl_window = SDL_CreateWindow(GEARSYSTEM_TITLE " " GEARSYSTEM_VERSION, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 770, 698, window_flags);
    gl_context = SDL_GL_CreateContext(sdl_window);
    SDL_GL_MakeCurrent(sdl_window, gl_context);
    SDL_GL_SetSwapInterval(0);

    SDL_SetWindowMinimumSize(sdl_window, 770, 600);

    for (int i = 0; i < 2; i++)
    {
        application_gamepad[i] = SDL_JoystickOpen(i);

        if(!application_gamepad[i])
            Log("Warning: Unable to open game controller! SDL Error: %s\n", SDL_GetError());
    }

    int w, h;
    int display_w, display_h;
    SDL_GetWindowSize(sdl_window, &w, &h);
    SDL_GL_GetDrawableSize(sdl_window, &display_w, &display_h);
    
    if (w > 0 && h > 0)
    {
        float scale_w = (float)display_w / w;
        float scale_h = (float)display_h / h;

        application_display_scale = (scale_w > scale_h) ? scale_w : scale_h;
    }

    return 0;
}

static void sdl_destroy(void)
{
    SDL_JoystickClose(application_gamepad[0]);
    SDL_JoystickClose(application_gamepad[1]);
    ImGui_ImplSDL2_Shutdown();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(sdl_window);
    SDL_Quit();
}

static void sdl_events(void)
{
    SDL_Event event;
        
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            running = false;
            break;
        }

        ImGui_ImplSDL2_ProcessEvent(&event);

        if (!gui_in_use)
        {
            sdl_events_emu(&event);
            sdl_shortcuts_gui(&event);
        }
    }
}

static void sdl_events_emu(const SDL_Event* event)
{
    switch(event->type)
    {
        case SDL_WINDOWEVENT:
        {
            switch (event->window.event)
            {
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                {
                    emu_resume();
                }
                break;

                case SDL_WINDOWEVENT_FOCUS_LOST:
                {
                    emu_pause();
                }
                break;
            }
        }
        break;

        case SDL_JOYBUTTONDOWN:
        {
            for (int i = 0; i < 2; i++)
            {
                GS_Joypads pad = (i == 0) ? Joypad_1 : Joypad_2;
                SDL_JoystickID id = SDL_JoystickInstanceID(application_gamepad[i]);

                if (!config_input[i].gamepad)
                    break;

                if (event->jbutton.which != id)
                    break;
                
                if (event->jbutton.button == config_input[i].gamepad_1)
                    emu_key_pressed(pad, Key_1);
                else if (event->jbutton.button == config_input[i].gamepad_2)
                    emu_key_pressed(pad, Key_2);
                else if (event->jbutton.button == config_input[i].gamepad_start)
                    emu_key_pressed(pad, Key_Start);
            }
        }
        break;

        case SDL_JOYBUTTONUP:
        {
            for (int i = 0; i < 2; i++)
            {
                GS_Joypads pad = (i == 0) ? Joypad_1 : Joypad_2;
                SDL_JoystickID id = SDL_JoystickInstanceID(application_gamepad[i]);

                if (!config_input[i].gamepad)
                    break;

                if (event->jbutton.which != id)
                    break;

                if (event->jbutton.button == config_input[i].gamepad_1)
                    emu_key_released(pad, Key_1);
                else if (event->jbutton.button == config_input[i].gamepad_2)
                    emu_key_released(pad, Key_2);
                else if (event->jbutton.button == config_input[i].gamepad_start)
                    emu_key_released(pad, Key_Start);
            }
        }
        break;

        case SDL_JOYAXISMOTION:
        {
            for (int i = 0; i < 2; i++)
            {
                GS_Joypads pad = (i == 0) ? Joypad_1 : Joypad_2;
                SDL_JoystickID id = SDL_JoystickInstanceID(application_gamepad[i]);

                if (!config_input[i].gamepad)
                    break;

                if (event->jbutton.which != id)
                    break;
                    
                if(event->jaxis.axis == config_input[i].gamepad_x_axis)
                {
                    int x_motion = event->jaxis.value * (config_input[i].gamepad_invert_x_axis ? -1 : 1);
                    if (x_motion < 0)
                        emu_key_pressed(pad, Key_Left);
                    else if (x_motion > 0)
                        emu_key_pressed(pad, Key_Right);
                    else
                    {
                        emu_key_released(pad, Key_Left);
                        emu_key_released(pad, Key_Right);
                    }
                }
                else if(event->jaxis.axis == config_input[i].gamepad_y_axis)
                {
                    int y_motion = event->jaxis.value * (config_input[i].gamepad_invert_y_axis ? -1 : 1);
                    if (y_motion < 0)
                        emu_key_pressed(pad, Key_Up);
                    else if (y_motion > 0)
                        emu_key_pressed(pad, Key_Down);
                    else
                    {
                        emu_key_released(pad, Key_Up);
                        emu_key_released(pad, Key_Down);
                    }
                }
            }
        }
        break;

        case SDL_KEYDOWN:
        {
            if (event->key.keysym.mod & KMOD_CTRL)
                break;

            int key = event->key.keysym.scancode;

            if (key == SDL_SCANCODE_ESCAPE)
            {
                application_trigger_quit();
                break;
            }

            for (int i = 0; i < 2; i++)
            {
                GS_Joypads pad = (i == 0) ? Joypad_1 : Joypad_2;

                if (key == config_input[i].key_left)
                    emu_key_pressed(pad, Key_Left);
                else if (key == config_input[i].key_right)
                    emu_key_pressed(pad, Key_Right);
                else if (key == config_input[i].key_up)
                    emu_key_pressed(pad, Key_Up);
                else if (key == config_input[i].key_down)
                    emu_key_pressed(pad, Key_Down);
                else if (key == config_input[i].key_1)
                    emu_key_pressed(pad, Key_1);
                else if (key == config_input[i].key_2)
                    emu_key_pressed(pad, Key_2);
                else if (key == config_input[i].key_start)
                    emu_key_pressed(pad, Key_Start);
            }
        }
        break;

        case SDL_KEYUP:
        {
            int key = event->key.keysym.scancode;

            for (int i = 0; i < 2; i++)
            {
                GS_Joypads pad = (i == 0) ? Joypad_1 : Joypad_2;

                if (key == config_input[i].key_left)
                    emu_key_released(pad, Key_Left);
                else if (key == config_input[i].key_right)
                    emu_key_released(pad, Key_Right);
                else if (key == config_input[i].key_up)
                    emu_key_released(pad, Key_Up);
                else if (key == config_input[i].key_down)
                    emu_key_released(pad, Key_Down);
                else if (key == config_input[i].key_1)
                    emu_key_released(pad, Key_1);
                else if (key == config_input[i].key_2)
                    emu_key_released(pad, Key_2);
                else if (key == config_input[i].key_start)
                    emu_key_released(pad, Key_Start);
            }
        }
        break;
    }
}

static void sdl_shortcuts_gui(const SDL_Event* event)
{
    if ((event->type == SDL_KEYDOWN) && (event->key.keysym.mod & KMOD_CTRL))
    {
        int key = event->key.keysym.scancode;
        
        switch (key)
        {
            case SDL_SCANCODE_O:
                gui_shortcut(gui_ShortcutOpenROM);
                break;
            case SDL_SCANCODE_R:
                gui_shortcut(gui_ShortcutReset);
                break;
            case SDL_SCANCODE_P:
                gui_shortcut(gui_ShortcutPause);
                break;
            case SDL_SCANCODE_F:
                gui_shortcut(gui_ShortcutFFWD);
                break;
            case SDL_SCANCODE_L:
                gui_shortcut(gui_ShortcutLoadState);
                break;
            case SDL_SCANCODE_S:
                gui_shortcut(gui_ShortcutSaveState);
                break;
        }
    }
}

static void run_emulator(void)
{
    config_emulator.paused = emu_is_paused();
    emu_audio_sync = config_audio.sync;
    emu_run_to_vblank();
}

static void render(void)
{
    renderer_begin_render();
    ImGui_ImplSDL2_NewFrame(sdl_window);  
    gui_render();
    renderer_render();
    renderer_end_render();

    SDL_GL_SwapWindow(sdl_window);
}

static void frame_throttle(void)
{
    if (emu_is_empty() || emu_is_paused() || config_emulator.ffwd)
    {
        float elapsed = (float)((frame_time_end - frame_time_start) * 1000) / SDL_GetPerformanceFrequency();

        float min = config_emulator.ffwd ? 8.333f : 16.666f;

        if (elapsed < min)
            SDL_Delay((Uint32)(min - elapsed));
    }
}
