#ifndef __EMUWINDOW_SDL_H__
#define __EMUWINDOW_SDL_H__

#include <SDL.h>
#include "video/emuwindow.h"


#if SDL_MAJOR_VERSION == 2

class SDL_Window;
class EmuWindow_SDL : public EmuWindow
{
public:
    EmuWindow_SDL();
    ~EmuWindow_SDL();

    /// Swap buffers to display the next frame
    void SwapBuffers();

    /**
     * @brief Sets the window title
     * @param title Title to set the window to
     */
    void SetTitle(const char* title);

    /// Makes the graphics context current for the caller thread
    void MakeCurrent();
    
    /// Releases (dunno if this is the "right" word) the SDL context from the caller thread
    void DoneCurrent();

    void PollEvents();

private:
    SDL_Window*     main_window_;
    SDL_GLContext   main_context_;
};

#else
#error Unsupported SDL version
#endif

#endif // __EMUWINDOW_SDL_H__
