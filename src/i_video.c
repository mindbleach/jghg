//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DOOM graphics stuff for SDL.
//

// Horrible hack alert:
// I am editing this file to not require SDL, but still compile and (in some sense) run. 
// But I can't go slashing out SDL functions willy-nilly because I still want to visualize this process. 
// So first I should probably distinguish "things Null Doom needs" from "things SDL needs." 
	// E.g. - I_VideoBuffer vs. screenbuffer. 

//#define SDL_VIDEO

// This goes in i_input.c but this file gets handled first. 
// There are surely correct ways of dealing with this - but doing it the wrong way first is faster. 
// Should probably be #ifndef'd versus JGHG. Input and audio could work without video... 
	// but there's no window to accept events. 
#ifdef SDL_VIDEO
// SDL keyboard doesn't really matter if there's no window to accept events. 
#define SDL_KEY_EVENTS
// This will fail to compile if SDL_KEYBOARD is not set in i_input.c. 
// We could just check that... but it's dependent on the compile order. 
// Really a bunch of this nonsense should go in .h files. 
#endif

#ifndef SDL_KEY_EVENTS
// If we skip i_input.c's functions that take the SDL event type, we throw some demo events from I_GetEvent. 
// doomkeys.h has constants representing important and useful keys like Enter and Ctrl. 
#include "doomkeys.h"
#endif

#ifdef SDL_VIDEO
#include "SDL.h"
#include "SDL_opengl.h"
#endif
// Okay, so: this now compiles and "runs" without SDL,h. 
// But gcc throws some errors? 
// Not sure it ididn't just stop replacing the executable. Dammit. 
// The latter. Dammit. 
// Argh. It's throwing unknown type errors on variables that are behind an #ifndef. 
// -And- turning SDL_VIS on causes errors. 
// That came down to a mismatched #endif screwing up braces. Which raises hairier questions. 
// Top error is that 'screen' is undefined. even though all uses and all definitions are behind identical #ifndef-s. 
// Declaring *screen / *renderer unconditionally avoids the error, but again, what? 
// Checked: no obvious #if-end mismatches. Anymore. 
// And removing the #ifndefs from those vars causes "undefined reference to 'I_GetWindowPosition'".
// Which, again, is also behind an ifndef, like everything else.
// But for some reason it stops there. Unconditional I_GetWindowPosition? Perfectly fine. 
// But there's no video. 
// And a single ctrl+c kills the program. There's no 'quit taunt.' 
// This seems like a problem with #ifndef altogether - or my understanding of what it can do. 
// ... I fucked up a bunch of #ifndef / #ifdef stuff, didn't I. 
// Fixed. 
// We can now bounce between a console-only version and the prior semi-normal visualization. 
// Unfortunately removing SDL.h still doesn't work. 
// Aaand now it does. 
// I think we are done with i_video.c. 

// Toggle visualization - i.e., window and graphics. 
#ifdef SDL_VIDEO
#ifndef SDL_VIS
#define SDL_VIS
#endif
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "icon.c"

#include "config.h"
#include "d_loop.h"
#include "deh_str.h"
#include "doomtype.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "tables.h"
#include "v_diskicon.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"







// ------- SDL stuff


// These are (1) the window (or the full screen) that our game is rendered to
// and (2) the renderer that scales the texture (see below) into this window.
#ifdef SDL_VIS
static SDL_Window *screen;
static SDL_Renderer *renderer;
#endif

// Window title
// Arguably SDL. "Safe" to leave in, at least. 
static char *window_title = "";

// These are (1) the 320x200x8 paletted buffer that we draw to (i.e. the one
// that holds I_VideoBuffer), (2) the 320x200x32 RGBA intermediate buffer that
// we blit the former buffer to, (3) the intermediate 320x200 texture that we
// load the RGBA buffer to and that we render into another texture (4) which
// is upscaled by an integer factor UPSCALE using "nearest" scaling and which
// in turn is finally rendered to screen using "linear" scaling.
#ifdef SDL_VIS
static SDL_Surface *screenbuffer = NULL;
static SDL_Surface *rgbabuffer = NULL;
static SDL_Texture *texture = NULL;
static SDL_Texture *texture_upscaled = NULL;

static SDL_Rect blit_rect = {
    0,
    0,
    SCREENWIDTH,
    SCREENHEIGHT
};
#endif
// disable mouse?
static boolean nomouse = false;
int usemouse = 1;



// Not gonna bother removing these because they're standard types and used by a pure SDL function. 

static uint32_t pixel_format;

// Save screenshots in PNG format.
int png_screenshots = 0;

// SDL video driver name
char *video_driver = "";

// Window position:
char *window_position = "center";

// SDL display number on which to run.
int video_display = 0;






// Shit I can't remove because it's referenced in d_main:

// If true, game is running as a screensaver
// Why... okay, I can see why it would be referenced in d_main.c. But still: boo. 
boolean screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen
boolean screenvisible = true;



// Gamma correction level to use
// Blegh - this is referenced in m_menu.c, even though there's no option in the game itself. 
int usegamma = 0;


/*

// Screen width and height, from configuration file.
// Hardcode to 320x200 for Null Doom - helpfully, that's the default.
int window_width = SCREENWIDTH * 2;
int window_height = SCREENHEIGHT_4_3 * 2;

// Fullscreen mode, 0x0 for SDL_WINDOW_FULLSCREEN_DESKTOP.
int fullscreen_width = 0, fullscreen_height = 0;

// Maximum number of pixels to use for intermediate scale buffer.
static int max_scaling_buffer_pixels = 16000000;

// Run in full screen mode?  (int type for config code)
// Defaults to fullscreen for some goddamn reason. 
int fullscreen = true;

// Aspect ratio correction mode
int aspect_ratio_correct = true;
static int actualheight;

// Force integer scales for resolution-independent rendering
int integer_scaling = false;

// VGA Porch palette change emulation
int vga_porch_flash = false;

// Force software rendering, for systems which lack effective hardware
// acceleration
int force_software_renderer = false;

// Grab the mouse? (int type for config code). nograbmouse_override allows
// this to be temporarily disabled via the command line.
static int grabmouse = true;
static boolean nograbmouse_override = false;


// If true, we display dots at the bottom of the screen to 
// indicate FPS.
static boolean display_fps_dots;

// If this is true, the screen is rendered but not blitted to the
// video buffer.
static boolean noblit;

// Callback function to invoke to determine whether to grab the 
// mouse pointer.
static grabmouse_callback_t grabmouse_callback = NULL;

// Does the window currently have focus?
static boolean window_focused = true;

// Window resize state.
static boolean need_resize = false;
static unsigned int last_resize_time;
#define RESIZE_DELAY 500
*/












// --------- Null Doom stuff

// palette
#ifdef SDL_VIS
// Whoops, "SDL_Color" is the type. 
static SDL_Color palette[256];
#endif
static boolean palette_to_set;

// display has been set up?

static boolean initialized = false;

// Time to wait for the screen to settle on startup before starting the
// game (ms)

static int startup_delay = 1000;


// The screen buffer; this is modified to draw things to the screen

//pixel_t *I_VideoBuffer = NULL;
// Horrible hack - directly define a fixed-size array for I_VideoBuffer.
//pixel_t I_VideoBuffer[ SCREENWIDTH * SCREENHEIGHT ]; 
uint8_t I_VideoBuffer[ SCREENWIDTH * SCREENHEIGHT ]; 

// Joystick/gamepad hysteresis
// "Hysteresis?"
// Arguably a Null Doom thing. I mean the original game supported joysticks, so. 
unsigned int joywait = 0;






// ------ SDL functions

/*
static boolean MouseShouldBeGrabbed()
{
    // never grab the mouse when in screensaver mode
   
    if (screensaver_mode)
        return false;

    // if the window doesn't have focus, never grab it

    if (!window_focused)
        return false;

    // always grab the mouse when full screen (dont want to 
    // see the mouse pointer)

    if (fullscreen)
        return true;

    // Don't grab the mouse if mouse input is disabled

    if (!usemouse || nomouse)
        return false;

    // if we specify not to grab the mouse, never grab

    if (nograbmouse_override || !grabmouse)
        return false;

    // Invoke the grabmouse callback function to determine whether
    // the mouse should be grabbed

    if (grabmouse_callback != NULL)
    {
        return grabmouse_callback();
    }
    else
    {
        return true;
    }
}
*/ 

// Called in D_DoomLoop in d_main.c. 
void I_SetGrabMouseCallback(grabmouse_callback_t func)
{
//    grabmouse_callback = func;
}

// Set the variable controlling FPS dots.
// Called in d_main.c and heretic/sb_bar.c. 
void I_DisplayFPSDots(boolean dots_on)
{
//    display_fps_dots = dots_on;
}

/*
static void SetShowCursor(boolean show)
{
    if (!screensaver_mode)
    {
        // When the cursor is hidden, grab the input.
        // Relative mode implicitly hides the cursor.
        SDL_SetRelativeMouseMode(!show);
        SDL_GetRelativeMouseState(NULL, NULL);
    }
}
*/
/*
// Adjust window_width / window_height variables to be an an aspect
// ratio consistent with the aspect_ratio_correct variable.
static void AdjustWindowSize(void)
{
    if (window_width * actualheight <= window_height * SCREENWIDTH)
    {
        // We round up window_height if the ratio is not exact; this leaves
        // the result stable.
        window_height = (window_width * actualheight + SCREENWIDTH - 1) / SCREENWIDTH;
    }
    else
    {
        window_width = window_height * SCREENWIDTH / actualheight;
    }
}
*/
/*
static void HandleWindowEvent(SDL_WindowEvent *event)
{
    int i;

    switch (event->event)
    {
#if 0 // SDL2-TODO
        case SDL_ACTIVEEVENT:
            // need to update our focus state
            UpdateFocus();
            break;
#endif
        case SDL_WINDOWEVENT_EXPOSED:
            palette_to_set = true;
            break;

        case SDL_WINDOWEVENT_RESIZED:
            need_resize = true;
            last_resize_time = SDL_GetTicks();
            break;

        // Don't render the screen when the window is minimized:

        case SDL_WINDOWEVENT_MINIMIZED:
            screenvisible = false;
            break;

        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
            screenvisible = true;
            break;

        // Update the value of window_focused when we get a focus event
        //
        // We try to make ourselves be well-behaved: the grab on the mouse
        // is removed if we lose focus (such as a popup window appearing),
        // and we dont move the mouse around if we aren't focused either.

        case SDL_WINDOWEVENT_FOCUS_GAINED:
            window_focused = true;
            break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
            window_focused = false;
            break;

        // We want to save the user's preferred monitor to use for running the
        // game, so that next time we're run we start on the same display. So
        // every time the window is moved, find which display we're now on and
        // update the video_display config variable.

        case SDL_WINDOWEVENT_MOVED:
            i = SDL_GetWindowDisplayIndex(screen);
            if (i >= 0)
            {
                video_display = i;
            }
            break;

        default:
            break;
    }
}
*/
/*
static boolean ToggleFullScreenKeyShortcut(SDL_Keysym *sym)
{
    Uint16 flags = (KMOD_LALT | KMOD_RALT);
#if defined(__MACOSX__)
    flags |= (KMOD_LGUI | KMOD_RGUI);
#endif
    return sym->scancode == SDL_SCANCODE_RETURN && (sym->mod & flags) != 0;
}
*/

/*
static void I_ToggleFullScreen(void)
{
    unsigned int flags = 0;

    // TODO: Consider implementing fullscreen toggle for SDL_WINDOW_FULLSCREEN
    // (mode-changing) setup. This is hard because we have to shut down and
    // restart again.
    if (fullscreen_width != 0 || fullscreen_height != 0)
    {
        return;
    }

    fullscreen = !fullscreen;

    if (fullscreen)
    {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    SDL_SetWindowFullscreen(screen, flags);

    if (!fullscreen)
    {
        AdjustWindowSize();
        SDL_SetWindowSize(screen, window_width, window_height);
    }
}
*/

// Some mix of keyboard and window events. 
// Control is presumably in i_input.c and so on. 
// But calling I_HandleKeyboardEvent on SDL_KEYDOWN / SDL_KEYUP is how controls happen. 
// So we could really just call that unconditionally (relative to switch / case) because we already broke resizing. 
// Obviously this gets replaced with KEYBRD_READY / KEYBRD_READ on CGA, and a dumb buffer in-between. 
// Also mouse events don't work, but they're optional, for testing. The final thing won't have keyboard input either. 
// Haha, no - we need mouse events to close the window via the X. Very silly. 
// Okay, it's smarter than that: SDL_QUIT is an event. Because it has to prompt you to exit. 
// Bluh - input. 
// Calling functions that specifically use SDL types makes cleanly removing SDL from i_input a pain. 
// So instead we should probably do a minimum of SDL-based handling right here, if SDL is enabled, and if not, basically just fake an Enter / Ctrl loop. 
void I_GetEvent(void)
{

	// void I_HandleKeyboardEvent(SDL_Event *sdlevent)
	// That calls D_PostEvent(&event); when appropriate. 

#ifdef SDL_VIS
    SDL_Event sdlevent;

    SDL_PumpEvents();
#endif

#ifdef SDL_KEY_EVENTS
    extern void I_HandleKeyboardEvent(SDL_Event *sdlevent);
    extern void I_HandleMouseEvent(SDL_Event *sdlevent);
#endif


#ifdef SDL_VIS

    while (SDL_PollEvent(&sdlevent))
    {

		if( sdlevent.type == SDL_QUIT ) { 		// Click the X to exit. 
            event_t event;
            event.type = ev_quit;
            D_PostEvent(&event);
		} else { 
#ifdef SDL_KEY_EVENTS
			I_HandleKeyboardEvent(&sdlevent);
#else
			// No SDL_KEY_EVENTS, but SDL_VIS? do i_input / I_HandleKeyboardEvent stuff here.
			// Weird testing phase. 

			event_t event;

			// Horrible debug hack: 
			// Actually, this is accidentally better. It treats all keys as this key. 
			// So... can I call a sequence of events? 
			// Sorta-kinda. Need a keyup event too. 
			// Didn't really work out. (Even with data2/3 as 0.) Ah well, ev_keydown is proof enough. 
			event.type = ev_keydown; 
			event.data1 = KEY_ENTER; 
	        D_PostEvent(&event);

			event.type = ev_keydown; 
			event.data1 = KEY_RCTRL; 
	        D_PostEvent(&event);

	/*
		void I_HandleKeyboardEvent(SDL_Event *sdlevent)
		{
			// XXX: passing pointers to event for access after this function
			// has terminated is undefined behaviour
			event_t event;

			switch (sdlevent->type)
			{
				case SDL_KEYDOWN:
				    event.type = ev_keydown;
				    event.data1 = TranslateKey(&sdlevent->key.keysym);
				    event.data2 = GetLocalizedKey(&sdlevent->key.keysym);
				    event.data3 = GetTypedChar(&sdlevent->key.keysym);

				    if (event.data1 != 0)
				    {
				        D_PostEvent(&event);
				    }
				    break;

				case SDL_KEYUP:
				    event.type = ev_keyup;
				    event.data1 = TranslateKey(&sdlevent->key.keysym);

				    // data2/data3 are initialized to zero for ev_keyup.
				    // For ev_keydown it's the shifted Unicode character
				    // that was typed, but if something wants to detect
				    // key releases it should do so based on data1
				    // (key ID), not the printable char.

				    event.data2 = 0;
				    event.data3 = 0;

				    if (event.data1 != 0)
				    {
				        D_PostEvent(&event);
				    }
				    break;

				default:
				    break;
			}
		}
	*/
#endif
		}

/*
        switch (sdlevent.type)
        {

            case SDL_KEYDOWN:		// Removing this breaks keyboard input. Shocking. 

                if (ToggleFullScreenKeyShortcut(&sdlevent.key.keysym))
                {
                    I_ToggleFullScreen();
                    break;
                }
                // deliberate fall-though

            case SDL_KEYUP:
		I_HandleKeyboardEvent(&sdlevent);
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEWHEEL:
                if (usemouse && !nomouse && window_focused)
                {
                    I_HandleMouseEvent(&sdlevent);
                }
                break;

            case SDL_QUIT:
                if (screensaver_mode)
                {
                    I_Quit();
                }
                else
                {
                    event_t event;
                    event.type = ev_quit;
                    D_PostEvent(&event);
                }
                break;

            case SDL_WINDOWEVENT:
                if (sdlevent.window.windowID == SDL_GetWindowID(screen))
                {
//                    HandleWindowEvent(&sdlevent.window);
                }
                break;

            default:
                break;
        }
*/
    }
#endif
}

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
	// (I didn't add that comment; it's original to Chocolate Doom 3.0.1.) 
	// d_main and Strife both call it, so I can't just remove it, but it's fine sitting here doing nothing. 
}

/*
static void UpdateGrab(void)
{

    static boolean currently_grabbed = false;
    boolean grab;

    grab = MouseShouldBeGrabbed();

    if (screensaver_mode)
    {
        // Hide the cursor in screensaver mode

        SetShowCursor(false);
    }
    else if (grab && !currently_grabbed)
    {
        SetShowCursor(false);
    }
    else if (!grab && currently_grabbed)
    {
        int screen_w, screen_h;

        SetShowCursor(true);

        // When releasing the mouse from grab, warp the mouse cursor to
        // the bottom-right of the screen. This is a minimally distracting
        // place for it to appear - we may only have released the grab
        // because we're at an end of level intermission screen, for
        // example.

        SDL_GetWindowSize(screen, &screen_w, &screen_h);
        SDL_WarpMouseInWindow(screen, screen_w - 16, screen_h - 16);
        SDL_GetRelativeMouseState(NULL, NULL);
    }

    currently_grabbed = grab;

}
*/

/*
static void LimitTextureSize(int *w_upscale, int *h_upscale)
{
    SDL_RendererInfo rinfo;
    int orig_w, orig_h;

    orig_w = *w_upscale;
    orig_h = *h_upscale;

    // Query renderer and limit to maximum texture dimensions of hardware:
    if (SDL_GetRendererInfo(renderer, &rinfo) != 0)
    {
        I_Error("CreateUpscaledTexture: SDL_GetRendererInfo() call failed: %s",
                SDL_GetError());
    }

    while (*w_upscale * SCREENWIDTH > rinfo.max_texture_width)
    {
        --*w_upscale;
    }
    while (*h_upscale * SCREENHEIGHT > rinfo.max_texture_height)
    {
        --*h_upscale;
    }

    if ((*w_upscale < 1 && rinfo.max_texture_width > 0) ||
        (*h_upscale < 1 && rinfo.max_texture_height > 0))
    {
        I_Error("CreateUpscaledTexture: Can't create a texture big enough for "
                "the whole screen! Maximum texture size %dx%d",
                rinfo.max_texture_width, rinfo.max_texture_height);
    }

    // We limit the amount of texture memory used for the intermediate buffer,
    // since beyond a certain point there are diminishing returns. Also,
    // depending on the hardware there may be performance problems with very
    // huge textures, so the user can use this to reduce the maximum texture
    // size if desired.

    if (max_scaling_buffer_pixels < SCREENWIDTH * SCREENHEIGHT)
    {
        I_Error("CreateUpscaledTexture: max_scaling_buffer_pixels too small "
                "to create a texture buffer: %d < %d",
                max_scaling_buffer_pixels, SCREENWIDTH * SCREENHEIGHT);
    }

    while (*w_upscale * *h_upscale * SCREENWIDTH * SCREENHEIGHT
           > max_scaling_buffer_pixels)
    {
        if (*w_upscale > *h_upscale)
        {
            --*w_upscale;
        }
        else
        {
            --*h_upscale;
        }
    }

    if (*w_upscale != orig_w || *h_upscale != orig_h)
    {
        printf("CreateUpscaledTexture: Limited texture size to %dx%d "
               "(max %d pixels, max texture size %dx%d)\n",
               *w_upscale * SCREENWIDTH, *h_upscale * SCREENHEIGHT,
               max_scaling_buffer_pixels,
               rinfo.max_texture_width, rinfo.max_texture_height);
    }
}
*/
/*
static void CreateUpscaledTexture(boolean force)
{
    int w, h;
    int h_upscale, w_upscale;
    static int h_upscale_old, w_upscale_old;

    // Get the size of the renderer output. The units this gives us will be
    // real world pixels, which are not necessarily equivalent to the screen's
    // window size (because of highdpi).
    if (SDL_GetRendererOutputSize(renderer, &w, &h) != 0)
    {
        I_Error("Failed to get renderer output size: %s", SDL_GetError());
    }

    // When the screen or window dimensions do not match the aspect ratio
    // of the texture, the rendered area is scaled down to fit. Calculate
    // the actual dimensions of the rendered area.

    if (w * actualheight < h * SCREENWIDTH)
    {
        // Tall window.

        h = w * actualheight / SCREENWIDTH;
    }
    else
    {
        // Wide window.

        w = h * SCREENWIDTH / actualheight;
    }

    // Pick texture size the next integer multiple of the screen dimensions.
    // If one screen dimension matches an integer multiple of the original
    // resolution, there is no need to overscale in this direction.

    w_upscale = (w + SCREENWIDTH - 1) / SCREENWIDTH;
    h_upscale = (h + SCREENHEIGHT - 1) / SCREENHEIGHT;

    // Minimum texture dimensions of 320x200.

    if (w_upscale < 1)
    {
        w_upscale = 1;
    }
    if (h_upscale < 1)
    {
        h_upscale = 1;
    }

//    LimitTextureSize(&w_upscale, &h_upscale);

    // Create a new texture only if the upscale factors have actually changed.

    if (h_upscale == h_upscale_old && w_upscale == w_upscale_old && !force)
    {
        return;
    }

    h_upscale_old = h_upscale;
    w_upscale_old = w_upscale;

    if (texture_upscaled)
    {
        SDL_DestroyTexture(texture_upscaled);
    }

    // Set the scaling quality for rendering the upscaled texture to "linear",
    // which looks much softer and smoother than "nearest" but does a better
    // job at downscaling from the upscaled texture to screen.

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    texture_upscaled = SDL_CreateTexture(renderer,
                                pixel_format,
                                SDL_TEXTUREACCESS_TARGET,
                                w_upscale*SCREENWIDTH,
                                h_upscale*SCREENHEIGHT);
}
*/

// Given an RGB value, find the closest matching palette index.
// This is surely an SDL thing. I don't expect the Doom engine had this. 
// It's called from v_video.c to get some generic colors. RKYW and two greys. 
// Ironically it might be useful for my CGA shenanigans. Especially generating dithered colormaps. 
int I_GetPaletteIndex(int r, int g, int b)
{
/*
    int best, best_diff, diff;
    int i;

    best = 0; best_diff = INT_MAX;

    for (i = 0; i < 256; ++i)
    {
        diff = (r - palette[i].r) * (r - palette[i].r)
             + (g - palette[i].g) * (g - palette[i].g)
             + (b - palette[i].b) * (b - palette[i].b);

        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }

        if (diff == 0)
        {
            break;
        }
    }

    return best;
*/
	return 0; 
}

// These title-setting functions get called in d_main, Heretic, and Strife. 
// So I can't just snip them out, even though they definitely don't matter for DOS. 
// ... having no window title means you can't click the X to close the window. Hokay, not touching these yet. 
// 
// Set the window title
//
void I_SetWindowTitle(char *title)
{
//    window_title = title;
}


//
// Call the SDL function to set the window title, based on 
// the title set with I_SetWindowTitle.
//
// Apparently necessary for Heretic's d_main.c ... but only with SDL disabled? Zah? 
void I_InitWindowTitle(void)
{
#ifdef SDL_VIS
    char *buf;

    buf = M_StringJoin(window_title, " - ", PACKAGE_STRING, NULL);
    SDL_SetWindowTitle(screen, buf);
    free(buf);
#endif
}
// Set the application icon
// Called by Heretic and Hexen, but not other games. Weird. 
void I_InitWindowIcon(void)
{
#ifdef SDL_VIS
    SDL_Surface *surface;

    surface = SDL_CreateRGBSurfaceFrom((void *) icon_data, icon_w, icon_h,
                                       32, icon_w * 4,
                                       0xff << 24, 0xff << 16,
                                       0xff << 8, 0xff << 0);

    SDL_SetWindowIcon(screen, surface);
    SDL_FreeSurface(surface);
#endif
}

/*
// Set video size to a particular scale factor (1x, 2x, 3x, etc.)
static void SetScaleFactor(int factor)
{
    // Pick 320x200 or 320x240, depending on aspect ratio correct

    window_width = factor * SCREENWIDTH;
    window_height = factor * actualheight;
    fullscreen = false;
}
*/

// These are all SDL things. 
void I_GraphicsCheckCommandLine(void)
{
/*
    int i;

    //!
    // @category video
    // @vanilla
    //
    // Disable blitting the screen.
    //

    noblit = M_CheckParm ("-noblit");

    //!
    // @category video 
    //
    // Don't grab the mouse when running in windowed mode.
    //

    nograbmouse_override = M_ParmExists("-nograbmouse");

    // default to fullscreen mode, allow override with command line
    // nofullscreen because we love prboom

    //!
    // @category video 
    //
    // Run in a window.
    //

    if (M_CheckParm("-window") || M_CheckParm("-nofullscreen"))
    {
        fullscreen = false;
    }

    //!
    // @category video 
    //
    // Run in fullscreen mode.
    //

    if (M_CheckParm("-fullscreen"))
    {
        fullscreen = true;
    }

    //!
    // @category video 
    //
    // Disable the mouse.
    //

    nomouse = M_CheckParm("-nomouse") > 0;

    //!
    // @category video
    // @arg <x>
    //
    // Specify the screen width, in pixels. Implies -window.
    //

    i = M_CheckParmWithArgs("-width", 1);

    if (i > 0)
    {
        window_width = atoi(myargv[i + 1]);
        window_height = window_width * 2;
        AdjustWindowSize();
        fullscreen = false;
    }

    //!
    // @category video
    // @arg <y>
    //
    // Specify the screen height, in pixels. Implies -window.
    //

    i = M_CheckParmWithArgs("-height", 1);

    if (i > 0)
    {
        window_height = atoi(myargv[i + 1]);
        window_width = window_height * 2;
        AdjustWindowSize();
        fullscreen = false;
    }

    //!
    // @category video
    // @arg <WxY>
    //
    // Specify the dimensions of the window. Implies -window.
    //

    i = M_CheckParmWithArgs("-geometry", 1);

    if (i > 0)
    {
        int w, h, s;

        s = sscanf(myargv[i + 1], "%ix%i", &w, &h);
        if (s == 2)
        {
            window_width = w;
            window_height = h;
            fullscreen = false;
        }
    }

    //!
    // @category video
    //
    // Don't scale up the screen. Implies -window.
    //

    if (M_CheckParm("-1")) 
    {
        SetScaleFactor(1);
    }

    //!
    // @category video
    //
    // Double up the screen to 2x its normal size. Implies -window.
    //

    if (M_CheckParm("-2")) 
    {
        SetScaleFactor(2);
    }

    //!
    // @category video
    //
    // Double up the screen to 3x its normal size. Implies -window.
    //

    if (M_CheckParm("-3")) 
    {
        SetScaleFactor(3);
    }

*/
}

// Check if we have been invoked as a screensaver by xscreensaver.
// Called by Doom, Heretic, and Hexen. 
void I_CheckIsScreensaver(void)
{
/*
    char *env;

    env = getenv("XSCREENSAVER_WINDOW");

    if (env != NULL)
    {
        screensaver_mode = true;
    }
*/
}

/*
static void SetSDLVideoDriver(void)
{
    // Allow a default value for the SDL video driver to be specified
    // in the configuration file.

    if (strcmp(video_driver, "") != 0)
    {
        char *env_string;

        env_string = M_StringJoin("SDL_VIDEODRIVER=", video_driver, NULL);
        putenv(env_string);
        free(env_string);
    }
}
*/

#ifdef SDL_VIS
// These two, CenterWindow and _GetWindowPosition, are pure SDL. 
	// But I_GetWindowPosition is referenced in i_videohr.c... and this is a quallity-of-life feature. 
	// So they stay in for now. 
// Check the display bounds of the display referred to by 'video_display' and
// set x and y to a location that places the window in the center of that
// display.
static void CenterWindow(int *x, int *y, int w, int h)
{
    SDL_Rect bounds;

    if (SDL_GetDisplayBounds(video_display, &bounds) < 0)
    {
        fprintf(stderr, "CenterWindow: Failed to read display bounds "
                        "for display #%d!\n", video_display);
        return;
    }

    *x = bounds.x + SDL_max((bounds.w - w) / 2, 0);
    *y = bounds.y + SDL_max((bounds.h - h) / 2, 0);
}
#endif

// Also referenced in i_videohr.c for some reason. 
// Hey genius, that means we can't remove it with SDL_VIS. 
void I_GetWindowPosition(int *x, int *y, int w, int h)
{
#ifdef SDL_VIS
    // Check that video_display corresponds to a display that really exists,
    // and if it doesn't, reset it.
    if (video_display < 0 || video_display >= SDL_GetNumVideoDisplays())
    {
        fprintf(stderr,
                "I_GetWindowPosition: We were configured to run on display #%d, "
                "but it no longer exists (max %d). Moving to display 0.\n",
                video_display, SDL_GetNumVideoDisplays() - 1);
        video_display = 0;
    }

/*
    // in fullscreen mode, the window "position" still matters, because
    // we use it to control which display we run fullscreen on.
    if (fullscreen)
    {
        CenterWindow(x, y, w, h);
        return;
    }
*/
    // in windowed mode, the desired window position can be specified
    // in the configuration file.

    if (window_position == NULL || !strcmp(window_position, ""))
    {
        *x = *y = SDL_WINDOWPOS_UNDEFINED;
    }
    else if (!strcmp(window_position, "center"))
    {
        // Note: SDL has a SDL_WINDOWPOS_CENTER, but this is useless for our
        // purposes, since we also want to control which display we appear on.
        // So we have to do this ourselves.
        CenterWindow(x, y, w, h);
    }
    else if (sscanf(window_position, "%i,%i", x, y) != 2)
    {
        // invalid format: revert to default
        fprintf(stderr, "I_GetWindowPosition: invalid window_position setting\n");
        *x = *y = SDL_WINDOWPOS_UNDEFINED;
    }
#endif
}


// Only used in Heretic, Hexen, and Strife. Bizarre. 
// And commenting it all out causes a segfault. (Ah, screenbuffer remains a null pointer.)
static void SetVideoMode(void)
{
#ifdef SDL_VIS
    int w, h;
    int x, y;
    unsigned int rmask, gmask, bmask, amask;
    int unused_bpp;
    int window_flags = 0, renderer_flags = 0;
    SDL_DisplayMode mode;

/*
    w = window_width;
    h = window_height;
*/
	w = SCREENWIDTH;
	h = SCREENHEIGHT;
/*
    // In windowed mode, the window can be resized while the game is
    // running.
    window_flags = SDL_WINDOW_RESIZABLE;

    // Set the highdpi flag - this makes a big difference on Macs with
    // retina displays, especially when using small window sizes.
    window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
*/
/*
    if (fullscreen)
    {
        if (fullscreen_width == 0 && fullscreen_height == 0)
        {
            // This window_flags means "Never change the screen resolution!
            // Instead, draw to the entire screen by scaling the texture
            // appropriately".
            window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
        else
        {
            w = fullscreen_width;
            h = fullscreen_height;
            window_flags |= SDL_WINDOW_FULLSCREEN;
        }
    }
*/

	// Necessary for window centering - quality-of-life while mucking about. 
    I_GetWindowPosition(&x, &y, w, h);

    // Create window and renderer contexts. We set the window title
    // later anyway and leave the window position "undefined". If
    // "window_flags" contains the fullscreen flag (see above), then
    // w and h are ignored.

	// When disabled, this does not create a window. 
	// It compiles, Doom puts text in the terminal, it errors out. 

    if (screen == NULL)
    {
        screen = SDL_CreateWindow(NULL, x, y, w, h, window_flags);

        if (screen == NULL)
        {
            I_Error("Error creating window for video startup: %s",
            SDL_GetError());
        }

        pixel_format = SDL_GetWindowPixelFormat(screen);

//        SDL_SetWindowMinimumSize(screen, SCREENWIDTH, actualheight);

//        I_InitWindowTitle();
//        I_InitWindowIcon();
    }

    // The SDL_RENDERER_TARGETTEXTURE flag is required to render the
    // intermediate texture into the upscaled texture.
    renderer_flags = SDL_RENDERER_TARGETTEXTURE;

/*	
    if (SDL_GetCurrentDisplayMode(video_display, &mode) != 0)
    {
        I_Error("Could not get display mode for video display #%d: %s",
        video_display, SDL_GetError());
    }

    // Turn on vsync if we aren't in a -timedemo
    if (!singletics && mode.refresh_rate > 0)
    {
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    }
*/
/*
    if (force_software_renderer)
    {
        renderer_flags |= SDL_RENDERER_SOFTWARE;
        renderer_flags &= ~SDL_RENDERER_PRESENTVSYNC;
    }
*/
#endif

#ifdef SDL_VIS
    if (renderer != NULL)
    {
        SDL_DestroyRenderer(renderer);
    }

    renderer = SDL_CreateRenderer(screen, -1, renderer_flags);

    if (renderer == NULL)
    {
        I_Error("Error creating renderer for screen window: %s",
                SDL_GetError());
    }
#endif

    // Important: Set the "logical size" of the rendering context. At the same
    // time this also defines the aspect ratio that is preserved while scaling
    // and stretching the texture into the window.
/*
    SDL_RenderSetLogicalSize(renderer,
                             SCREENWIDTH,
                             actualheight);
*/
    // Force integer scales for resolution-independent rendering.

#ifdef SDL_VIS
#if SDL_VERSION_ATLEAST(2, 0, 5)
//    SDL_RenderSetIntegerScale(renderer, integer_scaling);
#endif
#endif

#ifdef SDL_VIS
    // Blank out the full screen area in case there is any junk in
    // the borders that won't otherwise be overwritten.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);


    // Create the 8-bit paletted and the 32-bit RGBA screenbuffer surfaces.
	// This has to be present or we segfault. 
    if (screenbuffer == NULL)
    {
        screenbuffer = SDL_CreateRGBSurface(0,
                                            SCREENWIDTH, SCREENHEIGHT, 8,
                                            0, 0, 0, 0);
        SDL_FillRect(screenbuffer, NULL, 0);
    }


    // Format of rgbabuffer must match the screen pixel format because we
    // import the surface data into the texture.
    if (rgbabuffer == NULL)
    {
        SDL_PixelFormatEnumToMasks(pixel_format, &unused_bpp,
                                   &rmask, &gmask, &bmask, &amask);
        rgbabuffer = SDL_CreateRGBSurface(0,
                                          SCREENWIDTH, SCREENHEIGHT, 32,
                                          rmask, gmask, bmask, amask);
        SDL_FillRect(rgbabuffer, NULL, 0);
    }

    if (texture != NULL)
    {
        SDL_DestroyTexture(texture);
    }

    // Set the scaling quality for rendering the intermediate texture into
    // the upscaled texture to "nearest", which is gritty and pixelated and
    // resembles software scaling pretty well.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    // Create the intermediate texture that the RGBA surface gets loaded into.
    // The SDL_TEXTUREACCESS_STREAMING flag means that this texture's content
    // is going to change frequently.
    texture = SDL_CreateTexture(renderer,
                                pixel_format,
                                SDL_TEXTUREACCESS_STREAMING,
                                SCREENWIDTH, SCREENHEIGHT);
#endif

    // Initially create the upscaled texture for rendering to screen
//    CreateUpscaledTexture(true);
}

/*

static const char *hw_emu_warning = 
"===========================================================================\n"
"WARNING: it looks like you are using a software GL implementation.\n"
"To improve performance, try setting force_software_renderer in your\n"
"configuration file.\n"
"===========================================================================\n";

static void CheckGLVersion(void)
{
    const char * version;
    typedef const GLubyte* (APIENTRY * glStringFn_t)(GLenum);
    glStringFn_t glfp = (glStringFn_t)SDL_GL_GetProcAddress("glGetString");

    if (glfp)
    {
        version = (const char *)glfp(GL_VERSION);

        if (version && strstr(version, "Mesa"))
        {
            printf("%s", hw_emu_warning);
        }
    }
}
*/
// Bind all variables controlling video options into the configuration
// file system.
// These are all SDL's variables... except use_mouse, sorta-kinda.
// Argh, this gets called from d_main.c.
// Fullscreen by default without it. FFS. 
// Durr comment out all the 'if fullscreen' stuff. 
void I_BindVideoVariables(void)
{
//    M_BindIntVariable("fullscreen",                &fullscreen);
/*
    M_BindIntVariable("use_mouse",                 &usemouse);
    M_BindIntVariable("video_display",             &video_display);
    M_BindIntVariable("aspect_ratio_correct",      &aspect_ratio_correct);
    M_BindIntVariable("integer_scaling",           &integer_scaling);
    M_BindIntVariable("vga_porch_flash",           &vga_porch_flash);
    M_BindIntVariable("startup_delay",             &startup_delay);
    M_BindIntVariable("fullscreen_width",          &fullscreen_width);
    M_BindIntVariable("fullscreen_height",         &fullscreen_height);
    M_BindIntVariable("force_software_renderer",   &force_software_renderer);
    M_BindIntVariable("max_scaling_buffer_pixels", &max_scaling_buffer_pixels);
    M_BindIntVariable("window_width",              &window_width);
    M_BindIntVariable("window_height",             &window_height);
    M_BindIntVariable("grabmouse",                 &grabmouse);
    M_BindStringVariable("video_driver",           &video_driver);
    M_BindStringVariable("window_position",        &window_position);
    M_BindIntVariable("usegamma",                  &usegamma);
    M_BindIntVariable("png_screenshots",           &png_screenshots);
*/
}








// -------- Null Doom functions 

// Arguably SDL, but potentially relevant on any system. Put things back to normal. 
// Looks like I_AtExit sets this to be called. 
// And d_main does the same for D_Endoom and M_SaveDefaults. 
// So it's probably not exclusive? Like maybe they all get called. 
void I_ShutdownGraphics(void)
{
    if (initialized)
    {
//        SetShowCursor(true);
#ifdef SDL_VIS
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
#endif
        initialized = false;
    }
}

//
// I_StartFrame
//
// Really expected this to be Doom-standard, but it only appears in i_video.c!
// Does explain why the full content of the function is a comment reading "// er?". 
// Leaving it here anyway because this and I_FinishUpdate are likely to be useful in Null Doom. 
// E.g. swapping buffers versus copying to VRAM. 
// Wait, I_FinishUpdate also only appears in i_video. Where does it get called? I -know- it runs. 
void I_StartFrame (void)
{
    // er?

}

//
// I_StartTic
//
void I_StartTic (void)
{
    if (!initialized)
    {
        return;
    }

    I_GetEvent();
/*
    if (usemouse && !nomouse)
    {
        I_ReadMouse();
    }

    if (joywait < I_GetTime())
    {
        I_UpdateJoystick();
    }
*/
}

//
// I_FinishUpdate
//
// This SDL-heavy function is still a "Null Doom function" because it's where any copy-to-VRAM code will end up. 
// My first target display will be a CGA card. Conveniently 320x200... just in stupid colors. 
void I_FinishUpdate (void)
{
    static int lasttic;
    int tics;
    int i;

    if (!initialized)
        return;

/*
	// This is SDL stuff. 
    if (noblit)
        return;

    if (need_resize)
    {
        if (SDL_GetTicks() > last_resize_time + RESIZE_DELAY)
        {
            int flags;
            // When the window is resized (we're not in fullscreen mode),
            // save the new window size.
            flags = SDL_GetWindowFlags(screen);
            if ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == 0)
            {
                SDL_GetWindowSize(screen, &window_width, &window_height);

                // Adjust the window by resizing again so that the window
                // is the right aspect ratio.
//                AdjustWindowSize();
                SDL_SetWindowSize(screen, window_width, window_height);
            }
//            CreateUpscaledTexture(false);
            need_resize = false;
            palette_to_set = true;
        }
        else
        {
            return;
        }
    }
*/

	// This is SDL stuff. 
//    UpdateGrab();

#if 0 // SDL2-TODO
    // Don't update the screen if the window isn't visible.
    // Not doing this breaks under Windows when we alt-tab away 
    // while fullscreen.

    if (!(SDL_GetAppState() & SDL_APPACTIVE))
        return;
#endif

/*
    // draws little dots on the bottom of the screen
	// This is technically not SDL stuff, but it's also not base Doom stuff, so cut it. 
    if (display_fps_dots)
    {
	i = I_GetTime();
	tics = i - lasttic;
	lasttic = i;
	if (tics > 20) tics = 20;

	for (i=0 ; i<tics*4 ; i+=4)
	    I_VideoBuffer[ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
	for ( ; i<20*4 ; i+=4)
	    I_VideoBuffer[ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    }
*/
/*
    // Draw disk icon before blit, if necessary.
	// Pretty sure this is default Doom behavior? Like the turtle icon. 
	// The presence of V_RestoreDiskBackground makes this seem weird and complicated.
    V_DrawDiskIcon();
*/

#ifdef SDL_VIS
	// Technically Doom stuff? Dunno if this is updating the palette or uploading an updated palette. 
	// In either case it does break palette visualization when commented out. 
	// But it's still "SDL stuff" because I think the only step happening right here is the uploading. 
	// I'm fuzzy on where 'palette' is extern'd or declared, but it does seem to be global. 
    if (palette_to_set)
    {
        SDL_SetPaletteColors(screenbuffer->format->palette, palette, 0, 256);
        palette_to_set = false;

/*
        if (vga_porch_flash)
        {
            // "flash" the pillars/letterboxes with palette changes, emulating
            // VGA "porch" behaviour (GitHub issue #832)
            SDL_SetRenderDrawColor(renderer, palette[0].r, palette[0].g,
                palette[0].b, SDL_ALPHA_OPAQUE);
        }
*/
    }
#endif

#ifdef SDL_VIS
	// Horrible hack: copy I_VideoBuffer from our non-SDL buffer to *screenbuffer->pixels. 
	// This is SDL stuff, honestly. The frame is already in I_VideoBuffer. This is a visualization. 
	pixel_t *I_VideoBufferIntermediate;
	I_VideoBufferIntermediate = screenbuffer->pixels; 
/*
	for( int j = 0; j < SCREENWIDTH * SCREENHEIGHT; j++ ) { 
//		screenbuffer->pixels[j] = I_VideoBuffer[j]; 
		// "warning: dereferencing ‘void *’ pointer" on pixels[j].
		// "error: invalid use of void expression" on pixels[j] =. 
		// But the original definition is I_VideoBuffer = screenbuffer->pixels;. 
		// Do I need to cast this pointer as pixel_t[]? 
		// As //extern pixel_t *I_VideoBuffer; per original i_video.h. 
		I_VideoBufferIntermediate[j] = I_VideoBuffer[j]; 
		// It warns about ISO C90 not liking declarations in the middle of code, but sod off. 
		// And we have video! How utterly unsurprising. 
	} 
*/
	// Shove output into the corner of the screen so we can do other stuff in the dead space.
	for( int x = 0; x < SCREENWIDTH; x+=2 ) { 
		for( int y = 0; y < SCREENHEIGHT; y+=2 ) { 
			I_VideoBufferIntermediate[ (y/2)*SCREENWIDTH + x/2 ] = I_VideoBuffer[ y*SCREENWIDTH + x ]; 
		}
	} 
#endif

	// All this shit below is for sure SDL stuff. 
	// Like even if you're supposed to draw a disk icon when things are slow, you don't erase it. 
	// Commenting all this out works fine. 
#ifdef SDL_VIS
    // Blit from the paletted 8-bit screen buffer to the intermediate
    // 32-bit RGBA buffer that we can load into the texture.

    SDL_LowerBlit(screenbuffer, &blit_rect, rgbabuffer, &blit_rect);

    // Update the intermediate texture with the contents of the RGBA buffer.

    SDL_UpdateTexture(texture, NULL, rgbabuffer->pixels, rgbabuffer->pitch);

    // Make sure the pillarboxes are kept clear each frame.

    SDL_RenderClear(renderer);

    // Render this intermediate texture into the upscaled texture
    // using "nearest" integer scaling.

    SDL_SetRenderTarget(renderer, texture_upscaled);
    SDL_RenderCopy(renderer, texture, NULL, NULL);

    // Finally, render this upscaled texture to screen using linear scaling.

    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, texture_upscaled, NULL, NULL);

    // Draw!

    SDL_RenderPresent(renderer);
#endif

    // Restore background and undo the disk indicator, if it was drawn.
//    V_RestoreDiskBackground();
}

//
// I_ReadScreen
//
// Why the fuck would you ever read the entire screen? 
// This isn't what copies an immediate video buffer to the onscreen buffer. That's in I_FinishUpdate. 
// Also only shows up in i_video.c, according to grep. 
// Ctrl+F shows the only uses are the comment right above and the declaration right below. WTF. 
// Okay, commenting this out made compilation fail in f_wipe.c.
// So the real answer here is that grep sucks. 
// Ohhh, f_wipe is in /src/doom. God dammit, confusing nested directories. 
// Okay, so Chocolate Doom -does- keep all the 90s-ass scattered files. They're just extra scattered. 
// Anyway:
// This function is only ever used for screen wipes, and only in Doom and Strife. So it's a Null Doom thing. 
// Which is really convenient because it's plainly not using SDL function. 
void I_ReadScreen (pixel_t* scr)
{
    memcpy(scr, I_VideoBuffer, SCREENWIDTH*SCREENHEIGHT*sizeof(*scr));
}



// Mostly SDL, but doompal = W_CacheLumpName etc. is crucial Null Doom stuff. 
// Should maybe move that to another init function. 
void I_InitGraphics(void)
{
#ifdef SDL_VIS
    SDL_Event dummy;
#endif
    byte *doompal; 		// Putting this behind SDL_VIS runs, but I worry it means we misuse a global. 
    char *env;

    // Pass through the XSCREENSAVER_WINDOW environment variable to 
    // SDL_WINDOWID, to embed the SDL window into the Xscreensaver
    // window.
/*
    env = getenv("XSCREENSAVER_WINDOW");

    if (env != NULL)
    {
        char winenv[30];
        int winid;

        sscanf(env, "0x%x", &winid);
        M_snprintf(winenv, sizeof(winenv), "SDL_WINDOWID=%i", winid);

        putenv(winenv);
    }
*/
//    SetSDLVideoDriver();

#ifdef SDL_VIS
	// Commenting this out fucks up window centering. NBD, just a minor annoyance. 
	// The conditional is a call. 
    if (SDL_Init(SDL_INIT_VIDEO) < 0) 
    {
        I_Error("Failed to initialize video: %s", SDL_GetError());
    }
#endif
/*
    // When in screensaver mode, run full screen and auto detect
    // screen dimensions (don't change video mode)
    if (screensaver_mode)
    {
        fullscreen = true;
    }
*/

/*
	// When commented out, output stretches to fill entire window. (Still starts at 320x240.) 
    if (aspect_ratio_correct)
    {
        actualheight = SCREENHEIGHT_4_3;
    }
    else
    {
        actualheight = SCREENHEIGHT;
    }
*/

    // Create the game window; this may switch graphic modes depending
    // on configuration.
//    AdjustWindowSize();
#ifdef SDL_VIS
    SetVideoMode(); 		// Segfaults without this. 
#endif

    // We might have poor performance if we are using an emulated
    // HW accelerator. Check for Mesa and warn if we're using it.
//    CheckGLVersion();

    // Start with a clear black screen
    // (screen will be flipped after we set the palette)

//    SDL_FillRect(screenbuffer, NULL, 0);

    // Set the palette

    doompal = W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE);
#ifdef SDL_VIS
    I_SetPalette(doompal);
#endif
//    SDL_SetPaletteColors(screenbuffer->format->palette, palette, 0, 256);
			// This one's redundant anyway. It gets set immediately - right? 

    // SDL2-TODO UpdateFocus();
//    UpdateGrab();

/*
    // On some systems, it takes a second or so for the screen to settle
    // after changing modes.  We include the option to add a delay when
    // setting the screen mode, so that the game doesn't start immediately
    // with the player unable to see anything.
    if (fullscreen && !screensaver_mode)
    {
        SDL_Delay(startup_delay);
    }
*/

#ifdef SDL_VIS
    // The actual 320x200 canvas that we draw to. This is the pixel buffer of
    // the 8-bit paletted screen buffer that gets blit on an intermediate
    // 32-bit RGBA screen buffer that gets loaded into a texture that gets
    // finally rendered into our window or full screen in I_FinishUpdate().

	// See horrible hack up top - we now make I_VideoBuffer a direct array of pixel_t. 
	// Scratch that, a direct array of uint8_t. Which could also be "byte" or "char." Shrug. 
//    I_VideoBuffer = screenbuffer->pixels;
    V_RestoreBuffer();

    // Clear the screen to black.
	// Pretty sure memset and memcpy are standard library functions. 
	// Obviously, from context, it's just a loop that repeats one value. 
    memset(I_VideoBuffer, 0, SCREENWIDTH * SCREENHEIGHT);

    // clear out any events waiting at the start and center the mouse
    while (SDL_PollEvent(&dummy));

    initialized = true;

    // Call I_ShutdownGraphics on quit

    I_AtExit(I_ShutdownGraphics, true);
#endif
}

//
// I_SetPalette
//
// Bleh. This gets called in a bunch of places. Doom, Heretic, Hexen. 
// Oh, it's not an SDL thing. It is a Null Doom thing. 
// Calls look like I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));.
// So it's taking a pointer to an array of 8-bit values (RGB888) and setting the system palette. 
// This fucks up without SDL.h... because palette[i].r is undefined? 
// Haha, FFS. I keep thinking, "Where is palette[] defined? I see SDL_Color_palette."
// SDL_Color is the type. 
void I_SetPalette (byte *doompalette)
{
#ifdef SDL_VIS
    int i;

    for (i=0; i<256; ++i)
    {
        // Zero out the bottom two bits of each channel - the PC VGA
        // controller only supports 6 bits of accuracy.
/*
        palette[i].r = gammatable[usegamma][*doompalette++] & ~3;
        palette[i].g = gammatable[usegamma][*doompalette++] & ~3;
        palette[i].b = gammatable[usegamma][*doompalette++] & ~3;
*/
        palette[i].r = gammatable[0][*doompalette++] & ~3;
        palette[i].g = gammatable[0][*doompalette++] & ~3;
        palette[i].b = gammatable[0][*doompalette++] & ~3;
    }

    palette_to_set = true;
#endif
}


