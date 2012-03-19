/*!
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * \file    video_core.cpp
 * \author  ShizZy <shizzy247@gmail.com>
 * \date    2012-03-08
 * \brief   Main module for new video core
 *
 * \section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include "SDL.h"

#include "common.h"

#include "video_core.h"
#include "renderer_base.h"
#include "video/emuwindow_sdl.h"
#include "renderer_gl3/renderer_gl3.h"
#include "renderer_gl2/renderer_gl2.h"
#include "fifo.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace video_core {


RendererBase*   g_renderer;         ///< Renderer plugin
SDL_Thread      *g_video_thread;


int VideoThreadEntry(void *unused) {

    EmuWindow_SDL* emu_window = new EmuWindow_SDL;

    delete g_renderer;
    g_renderer = new RendererGL2();
    g_renderer->SetWindow(emu_window);
    g_renderer->Init();

    gp::DecodeThread(NULL);

    return 0;
}

/// Start the video core
void Start() {
    g_video_thread = SDL_CreateThread(VideoThreadEntry, NULL);

    if (g_video_thread == NULL) {
        LOG_ERROR(TVIDEO, "Unable to create thread: %s... Exiting\n", SDL_GetError());
        exit(1);
    }
}

/// Initialize the video core
void Init() {
    gp::FifoInit();

    LOG_NOTICE(TVIDEO, "initialized ok");
}

/// Shutdown the video core
void Shutdown() {
    gp::FifoShutdown();
}

} // namespace
