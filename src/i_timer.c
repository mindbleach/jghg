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
//      Timer functions.
//

// Horrible hack: removing SDL as a dependency. 
// Does the standard library have timing functions? I'd almost expect not. 
// In any case, that should probably also be optional, for consoles and such. 
// Straight-up not having a timer is gonna lead to some weird behavior. 

//#define SDL_TIMER

//#ifdef SDL_TIMER
#include "SDL.h"
//#endif

#include "i_timer.h"
#include "doomtype.h"

//
// I_GetTime
// returns time in 1/35th second tics
//
static Uint32 basetime = 0;
static Uint32 faketime = 1234;

int  I_GetTime (void)
{
    Uint32 ticks;

#ifdef SDL_TIMER
    ticks = SDL_GetTicks();
#else
//	ticks += 28; 		// Always-ish. 
	ticks = faketime; 
	faketime += 1; 		// Believe me, 1 is enough. 
#endif

	// Wow, no, fuck braceless conditional syntax. 
	// Wait, how do these variables work?
	// The expected pattern is, get new time, if old time minues new time is big, old = new, advance game state.
	// This is... get new time, if new time is zero...
	// Ah, this is called by some vanilla Doom function. 
	// This is not the check for if it's been long enough - this is the timer function that check calls. 
	// And what it's measuring is 'time since we started checking the time.' 
	// So we need to count up on some other variable as a fake timer. 
	// That is somehow even more broken than gameplay not starting. It's just a black screen. 
	// Ah, faketime can't start at zero. Presumably that's a sanity check. 
//    if (basetime == 0)
//        basetime = ticks;
    if (basetime == 0) 
	{
        basetime = ticks;
	}

    ticks -= basetime;

//#ifndef SDL_TIMER
//	ticks = 40; 
//#endif

    return (ticks * TICRATE) / 1000;    
}

//
// Same as I_GetTime, but returns time in milliseconds
//
// This seems to be the one Chocolate Doom uses for gameplay. 
// Kind of a DRY violation that I_GetTime doesn't just call this and divide by 28. 
int I_GetTimeMS(void)
{
    Uint32 ticks;

#ifdef SDL_TIMER
    ticks = SDL_GetTicks();
#else
//	ticks += 28; 		// Always-ish. 
	ticks = faketime; 
	faketime += 1; 
#endif

    if (basetime == 0) 
	{
        basetime = ticks;
	}

    return ticks - basetime;
}

// Sleep for a specified number of ms
// Believe it or not, this is the worst culprit for comedy speed-ups. 
// I guess Chocolate Doom does a 1ms wait after every frame, because why not? 
void I_Sleep(int ms)
{
#ifdef SDL_TIMER
    SDL_Delay(ms);
#endif
}

void I_WaitVBL(int count)
{
    I_Sleep((count * 1000) / 70);
}


void I_InitTimer(void)
{
    // initialize timer

#ifdef SDL_TIMER
    SDL_Init(SDL_INIT_TIMER);
#endif
}

