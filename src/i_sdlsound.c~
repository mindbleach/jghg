//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2008 David Flater
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
//	System interface for sound.
//

// Horrible hack: 
// JGHG is like... doing all the work and then putting it in the wrong place. 
// I don't want a null driver. I want a driver that -pretends- to make sound. 
// So we want to do everything you'd usually shunt off into the audio card - and then not give it to the audio card. 

// Doom sound is a little fucky because s_sound has both the generic play-this-somewhere function, and the function for picking which channel gets a specific play-this-here function. If the latter was in an i_ file then I could erase damn near all of this and do something dead simple from scratch. As-is? I'm gonna comment out SDL.h and see what breaks. 

// What exactly is all this "mix" stuff? It's usually capitalized. Is it an SDL system? 
	// Surely it's whatever is in SDL_mixer.h. 

// Well, that was shockingly fast. I #ifdef SDL_AUDIO'd like four things, and... it compiles. It segfaults the moment it tries to play a sound (which is not instant, presumably because I have music turned down to zero). And yet: it does compile. So I just have to do some shenanigans to avoid null pointers. Yeah? 
	// Oh right, SDL_mixer.h. Commenting that out causes a bunch of errors - Mix_Chunk type does not exist, properties of some chunk.abuf are null, MIX_MAX_VOLUME is not declared. Ech. 
	// Straight-up copied Mix_Chunk typedef / struct from SDL documentation. 
	// I think MIX_MAX_VOLUME is an SDL_mixer global? 
		// https://www.libsdl.org/projects/SDL_mixer/docs/SDL_mixer_27.html - 128 by default. 
		// This is relevant to JGHG because it's AllocateSound that uses this value. 
// Currently compiles and runs (but segfaults) without SDL.h or SDL_mixer.h. 
	// Need to #ifndef and write some shenanigans. 
		// Audio goes into Expand or whatever, to resample for the right sample rate. 
			// Gonna bet that's optional, and we can make do with high-pitched squawks. 
			// Might be completely optional - its only use here is behind #ifdef HAVE_LIBSAMPLERATE. 
			// ... oh, as is that function, already. Huh. 
			// Nope, that's ExpandSoundData_SRC. Delete all this HAVE_LIBSAMPLERATE stuff. It is distracting. 
		// Copy whatever's currently #ifdef'd out to initialize audio. Might fix crashes even if nothing happens?
			// Actually - it's weird we segfault at all, if we never write audio. What null pointer gets dereferenced? 
		// Copy whatever's currently #ifdef'd out to play audio.
		// Write audio to some custom buffer instead. Copy that into the re-faked audio and/or *screenbuffer. 
// Didn't end up drawing anything to the screen. Juggling "extern" and the multiple janky screen buffer pointers is a hassle I don't really need. It is sufficient to note that we have an SDL-free data format (the Mix_Chunk_Clone type slash struct, which is straight-up copied from SDL) and do SDL-free data handling (as evidenced by, ironically, segfaulting until every last step of the data-to-waveform process was accounted for) which includes Chocolate Doom's slightly goofy resampling (where one "generic" branch politely skipped SDL) - and it still outputs sound when SDL_MIXER is #define'd. SDL.h itself remains completely irrelevant. Shrug. 

// SDL.h inclusion:
// Also needed for screenbuffer type. 
//#define SDL_AUDIO

// SDL_mixer.h inclusion: 
//#define SDL_MIXER

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef SDL_AUDIO
#include "SDL.h"
#endif

#ifdef SDL_MIXER
#include "SDL_mixer.h"
#endif

#ifndef SDL_MIXER
#define MIX_MAX_VOLUME 128
#endif

//#ifdef HAVE_LIBSAMPLERATE
//#include <samplerate.h>
//#endif

#include "deh_str.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_swap.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"

#include "doomtype.h"

#define LOW_PASS_FILTER
//#define DEBUG_DUMP_WAVS
#define NUM_CHANNELS 16

// Ugh, C structs.
// I only know this code is supposed to define allocated_sound_t because that's how it's passed in functions. 
// So it would be... equivalent to declare a 'struct _s variableName' and just '_t variableName'? 
// I think I only have to replace 'Mix_Chunk chunk.' 
// https://www.libsdl.org/projects/SDL_mixer/docs/SDL_mixer_85.html
// int allocated, uint8 *abuf, uint32 alen, uint8 volume. 
// Honestly I could almost just copy the definition. Or is that too Javascipt-minded?
// Presumably the constructor needs to malloc to fill whatever abuf points to. 
// Except that's in AllocateSound, here in this file. Alrighty then. 

// Can I just copy this into here? Will it collide, will it just use the copy in-scope? 
// "Conflicting types." Not surprised. So... My_Mix_Chunk? Mix_Chunk_Clone? 
// C is all super low-level, and accomodating enough to be a footgun. Can I just swap them out, in the struct? 
// Apparently so. The compiler understandably complains, but it's just a warning. 
// SDL itself throws some "notes." Not warnings, apparently. Just 'expected MC but argument is of type MCC.'
// But it runs. 
typedef struct Mix_Chunk_Clone {
    int allocated;
    Uint8 *abuf;
    Uint32 alen;
    Uint8 volume;     /* Per-sample volume, 0-128 */
} Mix_Chunk_Clone;


typedef struct allocated_sound_s allocated_sound_t;

struct allocated_sound_s
{
    sfxinfo_t *sfxinfo;
#ifdef SDL_MIXER
    Mix_Chunk chunk;
#else 
	Mix_Chunk_Clone chunk; 
#endif
    int use_count;
    int pitch;
    allocated_sound_t *prev, *next;
};

static boolean sound_initialized = false;

static allocated_sound_t *channels_playing[NUM_CHANNELS];

static int mixer_freq;
static Uint16 mixer_format;
static int mixer_channels;
static boolean use_sfx_prefix;
static boolean (*ExpandSoundData)(sfxinfo_t *sfxinfo,
                                  byte *data,
                                  int samplerate,
                                  int length) = NULL;

// Doubly-linked list of allocated sounds.
// When a sound is played, it is moved to the head, so that the oldest
// sounds not used recently are at the tail.
static allocated_sound_t *allocated_sounds_head = NULL;
static allocated_sound_t *allocated_sounds_tail = NULL;
static int allocated_sounds_size = 0;

int use_libsamplerate = 0;

// Scale factor used when converting libsamplerate floating point numbers
// to integers. Too high means the sounds can clip; too low means they
// will be too quiet. This is an amount that should avoid clipping most
// of the time: with all the Doom IWAD sound effects, at least. If a PWAD
// is used, clipping might occur.
float libsamplerate_scale = 0.65f;

// Hook a sound into the linked list at the head.
static void AllocatedSoundLink(allocated_sound_t *snd)
{
    snd->prev = NULL;

    snd->next = allocated_sounds_head;
    allocated_sounds_head = snd;

    if (allocated_sounds_tail == NULL)
    {
        allocated_sounds_tail = snd;
    }
    else
    {
        snd->next->prev = snd;
    }
}

// Unlink a sound from the linked list.
static void AllocatedSoundUnlink(allocated_sound_t *snd)
{
    if (snd->prev == NULL)
    {
        allocated_sounds_head = snd->next;
    }
    else
    {
        snd->prev->next = snd->next;
    }

    if (snd->next == NULL)
    {
        allocated_sounds_tail = snd->prev;
    }
    else
    {
        snd->next->prev = snd->prev;
    }
}

static void FreeAllocatedSound(allocated_sound_t *snd)
{
    // Unlink from linked list.

    AllocatedSoundUnlink(snd);

    // Keep track of the amount of allocated sound data:

    allocated_sounds_size -= snd->chunk.alen;

    free(snd);
}

// Search from the tail backwards along the allocated sounds list, find
// and free a sound that is not in use, to free up memory.  Return true
// for success.
static boolean FindAndFreeSound(void)
{
    allocated_sound_t *snd;

    snd = allocated_sounds_tail;

    while (snd != NULL)
    {
        if (snd->use_count == 0)
        {
            FreeAllocatedSound(snd);
            return true;
        }

        snd = snd->prev;
    }

    // No available sounds to free...

    return false;
}

// Enforce SFX cache size limit.  We are just about to allocate "len"
// bytes on the heap for a new sound effect, so free up some space
// so that we keep allocated_sounds_size < snd_cachesize
static void ReserveCacheSpace(size_t len)
{
    if (snd_cachesize <= 0)
    {
        return;
    }

    // Keep freeing sound effects that aren't currently being played,
    // until there is enough space for the new sound.

    while (allocated_sounds_size + len > snd_cachesize)
    {
        // Free a sound.  If there is nothing more to free, stop.

        if (!FindAndFreeSound())
        {
            break;
        }
    }
}

// Allocate a block for a new sound effect.
static allocated_sound_t *AllocateSound(sfxinfo_t *sfxinfo, size_t len)
{
    allocated_sound_t *snd;

    // Keep allocated sounds within the cache size.

    ReserveCacheSpace(len);

    // Allocate the sound structure and data.  The data will immediately
    // follow the structure, which acts as a header.

    do
    {
        snd = malloc(sizeof(allocated_sound_t) + len);

        // Out of memory?  Try to free an old sound, then loop round
        // and try again.

        if (snd == NULL && !FindAndFreeSound())
        {
            return NULL;
        }

    } while (snd == NULL);

    // Skip past the chunk structure for the audio buffer

    snd->chunk.abuf = (byte *) (snd + 1);
    snd->chunk.alen = len;
    snd->chunk.allocated = 1;
    snd->chunk.volume = MIX_MAX_VOLUME;
    snd->pitch = NORM_PITCH;

    snd->sfxinfo = sfxinfo;
    snd->use_count = 0;

    // Keep track of how much memory all these cached sounds are using...

    allocated_sounds_size += len;

    AllocatedSoundLink(snd);

    return snd;
}

// Lock a sound, to indicate that it may not be freed.
static void LockAllocatedSound(allocated_sound_t *snd)
{
    // Increase use count, to stop the sound being freed.

    ++snd->use_count;

    //printf("++ %s: Use count=%i\n", snd->sfxinfo->name, snd->use_count);

    // When we use a sound, re-link it into the list at the head, so
    // that the oldest sounds fall to the end of the list for freeing.

    AllocatedSoundUnlink(snd);
    AllocatedSoundLink(snd);
}

// Unlock a sound to indicate that it may now be freed.
static void UnlockAllocatedSound(allocated_sound_t *snd)
{
    if (snd->use_count <= 0)
    {
        I_Error("Sound effect released more times than it was locked...");
    }

    --snd->use_count;

    //printf("-- %s: Use count=%i\n", snd->sfxinfo->name, snd->use_count);
}

// Search through the list of allocated sounds and return the one that matches
// the supplied sfxinfo entry and pitch level.
static allocated_sound_t * GetAllocatedSoundBySfxInfoAndPitch(sfxinfo_t *sfxinfo, int pitch)
{
    allocated_sound_t * p = allocated_sounds_head;

    while (p != NULL)
    {
        if (p->sfxinfo == sfxinfo && p->pitch == pitch)
        {
            return p;
        }
        p = p->next;
    }

    return NULL;
}

// Allocate a new sound chunk and pitch-shift an existing sound up-or-down
// into it.
static allocated_sound_t * PitchShift(allocated_sound_t *insnd, int pitch)
{
    allocated_sound_t * outsnd;
    Sint16 *inp, *outp;
    Sint16 *srcbuf, *dstbuf;
    Uint32 srclen, dstlen;

    srcbuf = (Sint16 *)insnd->chunk.abuf;
    srclen = insnd->chunk.alen;

    // determine ratio pitch:NORM_PITCH and apply to srclen, then invert.
    // This is an approximation of vanilla behaviour based on measurements
    dstlen = (int)((1 + (1 - (float)pitch / NORM_PITCH)) * srclen);

    // ensure that the new buffer is an even length
    if ((dstlen % 2) == 0)
    {
        dstlen++;
    }

    outsnd = AllocateSound(insnd->sfxinfo, dstlen);

    if (!outsnd)
    {
        return NULL;
    }

    outsnd->pitch = pitch;
    dstbuf = (Sint16 *)outsnd->chunk.abuf;

    // loop over output buffer. find corresponding input cell, copy over
    for (outp = dstbuf; outp < dstbuf + dstlen/2; ++outp)
    {
        inp = srcbuf + (int)((float)(outp - dstbuf) / dstlen * srclen);
        *outp = *inp;
    }

    return outsnd;
}

// When a sound stops, check if it is still playing.  If it is not,
// we can mark the sound data as CACHE to be freed back for other
// means.
static void ReleaseSoundOnChannel(int channel)
{
    allocated_sound_t *snd = channels_playing[channel];

#ifdef SDL_MIXER
    Mix_HaltChannel(channel);
#endif

    if (snd == NULL)
    {
        return;
    }

    channels_playing[channel] = NULL;

    UnlockAllocatedSound(snd);

    // if the sound is a pitch-shift and it's not in use, immediately
    // free it
    if (snd->pitch != NORM_PITCH && snd->use_count <= 0)
    {
        FreeAllocatedSound(snd);
    }
}


static boolean ConvertibleRatio(int freq1, int freq2)
{
    int ratio;

    if (freq1 > freq2)
    {
        return ConvertibleRatio(freq2, freq1);
    }
    else if ((freq2 % freq1) != 0)
    {
        // Not in a direct ratio

        return false;
    }
    else
    {
        // Check the ratio is a power of 2

        ratio = freq2 / freq1;

        while ((ratio & 1) == 0)
        {
            ratio = ratio >> 1;
        }

        return ratio == 1;
    }
}

//#ifdef SDL_MIXER
// Generic sound expansion function for any sample rate.
// Returns number of clipped samples (always 0).
// Gonna bet I can skip this. Audio data will be fucky, but I only need it to be present. 
static boolean ExpandSoundData_SDL(sfxinfo_t *sfxinfo,
                                   byte *data,
                                   int samplerate,
                                   int length)
{

//#ifdef SDL_MIXER 		// This one goes all the way to the end. 

#ifdef SDL_MIXER
    SDL_AudioCVT convertor; 		// Apparently in SDL_mixer.h? 
#endif

    allocated_sound_t *snd;
//    Mix_Chunk *chunk;
#ifdef SDL_MIXER
    Mix_Chunk *chunk;
#else 
	Mix_Chunk_Clone *chunk; 
#endif
    uint32_t expanded_length;

    // Calculate the length of the expanded version of the sample.
    expanded_length = (uint32_t) ((((uint64_t) length) * mixer_freq) / samplerate);

    // Double up twice: 8 -> 16 bit and mono -> stereo
    expanded_length *= 4;

    // Allocate a chunk in which to expand the sound
    snd = AllocateSound(sfxinfo, expanded_length);

    if (snd == NULL)
    {
        return false;
    }

    chunk = &snd->chunk;

    {		// This was originally an if-else's "else" scope. 
        Sint16 *expanded = (Sint16 *) chunk->abuf;
        int expanded_length;
        int expand_ratio;
        int i;

        // Generic expansion if conversion does not work:
        //
        // SDL's audio conversion only works for rate conversions that are
        // powers of 2; if the two formats are not in a direct power of 2
        // ratio, do this naive conversion instead.

        // number of samples in the converted sound

        expanded_length = ((uint64_t) length * mixer_freq) / samplerate;
        expand_ratio = (length << 8) / expanded_length;

        for (i=0; i<expanded_length; ++i)
        {
            Sint16 sample;
            int src;

            src = (i * expand_ratio) >> 8;

            sample = data[src] | (data[src] << 8);
            sample -= 32768;

            // expand 8->16 bits, mono->stereo

            expanded[i * 2] = expanded[i * 2 + 1] = sample;
        }

#ifdef LOW_PASS_FILTER
        // Perform a low-pass filter on the upscaled sound to filter
        // out high-frequency noise from the conversion process.

        {
            float rc, dt, alpha;

            // Low-pass filter for cutoff frequency f:
            //
            // For sampling rate r, dt = 1 / r
            // rc = 1 / 2*pi*f
            // alpha = dt / (rc + dt)

            // Filter to the half sample rate of the original sound effect
            // (maximum frequency, by nyquist)

            dt = 1.0f / mixer_freq;
            rc = 1.0f / (3.14f * samplerate);
            alpha = dt / (rc + dt);

            // Both channels are processed in parallel, hence [i-2]:

            for (i=2; i<expanded_length * 2; ++i)
            {
                expanded[i] = (Sint16) (alpha * expanded[i]
                                      + (1 - alpha) * expanded[i-2]);
            }
        }
#endif /* #ifdef LOW_PASS_FILTER */
    }
//#endif 		// #ifdef SDL_MIXER

	return true;
}

// Load and convert a sound effect
// Returns true if successful
static boolean CacheSFX(sfxinfo_t *sfxinfo)
{
    int lumpnum;
    unsigned int lumplen;
    int samplerate;
    unsigned int length;
    byte *data;

    // need to load the sound
    lumpnum = sfxinfo->lumpnum;
    data = W_CacheLumpNum(lumpnum, PU_STATIC);
    lumplen = W_LumpLength(lumpnum);

    // Check the header, and ensure this is a valid sound
    if (lumplen < 8
     || data[0] != 0x03 || data[1] != 0x00)
    {
        // Invalid sound

        return false;
    }

    // 16 bit sample rate field, 32 bit length field
    samplerate = (data[3] << 8) | data[2];
    length = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];

    // If the header specifies that the length of the sound is greater than
    // the length of the lump itself, this is an invalid sound lump

    // We also discard sound lumps that are less than 49 samples long,
    // as this is how DMX behaves - although the actual cut-off length
    // seems to vary slightly depending on the sample rate.  This needs
    // further investigation to better understand the correct
    // behavior.

    if (length > lumplen - 8 || length <= 48)
    {
        return false;
    }

    // The DMX sound library seems to skip the first 16 and last 16
    // bytes of the lump - reason unknown.
    data += 16;
    length -= 32;

	//return false; // Debug 
	//printf("-- %s: Use count=%i\n", sfxinfo->name, snd->use_count);
	//    int lumpnum = sfxinfo->lumpnum;

    // Sample rate conversion
    if (!ExpandSoundData(sfxinfo, data + 8, samplerate, length))
    {
        return false;
    }

#ifdef DEBUG_DUMP_WAVS
    {
        char filename[16];
        allocated_sound_t * snd;

        M_snprintf(filename, sizeof(filename), "%s.wav",
                   DEH_String(sfxinfo->name));
        snd = GetAllocatedSoundBySfxInfoAndPitch(sfxinfo, NORM_PITCH);
        WriteWAV(filename, snd->chunk.abuf, snd->chunk.alen,mixer_freq);
    }
#endif

    // don't need the original lump any more
  
    W_ReleaseLumpNum(lumpnum);

    return true;
}

static void GetSfxLumpName(sfxinfo_t *sfx, char *buf, size_t buf_len)
{
    // Linked sfx lumps? Get the lump number for the sound linked to.

    if (sfx->link != NULL)
    {
        sfx = sfx->link;
    }

    // Doom adds a DS* prefix to sound lumps; Heretic and Hexen don't
    // do this.

    if (use_sfx_prefix)
    {
        M_snprintf(buf, buf_len, "ds%s", DEH_String(sfx->name));
    }
    else
    {
        M_StringCopy(buf, DEH_String(sfx->name), buf_len);
    }
}



static void I_SDL_PrecacheSounds(sfxinfo_t *sounds, int num_sounds)
{
    // no-op
}


// Load a SFX chunk into memory and ensure that it is locked.
// Yeesh. Some of these are borderline enterprise Java: the sound-locking function function function. 
static boolean LockSound(sfxinfo_t *sfxinfo)
{

    // If the sound isn't loaded, load it now
    if (GetAllocatedSoundBySfxInfoAndPitch(sfxinfo, NORM_PITCH) == NULL)
    {
        if (!CacheSFX(sfxinfo))
        {
            return false;
        }
    }

    LockAllocatedSound(GetAllocatedSoundBySfxInfoAndPitch(sfxinfo, NORM_PITCH));

    return true;
}

//
// Retrieve the raw data lump index
//  for a given SFX name.
//
static int I_SDL_GetSfxLumpNum(sfxinfo_t *sfx)
{
    char namebuf[9];

    GetSfxLumpName(sfx, namebuf, sizeof(namebuf));

    return W_GetNumForName(namebuf);
}

static void I_SDL_UpdateSoundParams(int handle, int vol, int sep)
{
    int left, right;

    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
    {
        return;
    }

    left = ((254 - sep) * vol) / 127;
    right = ((sep) * vol) / 127;

    if (left < 0) left = 0;
    else if ( left > 255) left = 255;
    if (right < 0) right = 0;
    else if (right > 255) right = 255;

#ifdef SDL_MIXER
    Mix_SetPanning(handle, left, right);
#endif
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//
// Why does this take a channel number? That means it can't be the generic 'hey, play this sound' function. 
// Grep doesn't don't show it being called anywhere. Uh... huh. 
// Hmm. Seems like sound_module_t sound_sdl_module (bottom) puts it in a list to be some standard function by another name. Mildly annoying. 
// S_StartSound in s_sound.c is the generic just-play-this function. s_sound also has the 'find a free channe' function. 
static int I_SDL_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep, int pitch)
{
    allocated_sound_t *snd;

    if (!sound_initialized || channel < 0 || channel >= NUM_CHANNELS)
    {
        return -1;
    }

    // Release a sound effect if there is already one playing
    // on this channel

    ReleaseSoundOnChannel(channel);

    // Get the sound data
	// For some reason LockSound segfaults with the SDL libraries removed. 
	// 
//#ifdef SDL_MIXER
    if (!LockSound(sfxinfo))
    {
        return -1;
    }
//#endif

//#ifdef SDL_MIXER
    snd = GetAllocatedSoundBySfxInfoAndPitch(sfxinfo, pitch);

    if (snd == NULL)
    {
        allocated_sound_t *newsnd;
        // fetch the base sound effect, un-pitch-shifted
        snd = GetAllocatedSoundBySfxInfoAndPitch(sfxinfo, NORM_PITCH);

        if (snd == NULL)
        {
            return -1;
        }

        if (snd_pitchshift)
        {
            newsnd = PitchShift(snd, pitch);

            if (newsnd)
            {
                LockAllocatedSound(newsnd);
                UnlockAllocatedSound(snd);
                snd = newsnd;
            }
        }
    }
    else
    {
        LockAllocatedSound(snd);
    }

    // play sound
	// This is probably the crux of what we have to fake. 
	// Put &snd->chunk somewhere else. Read it back only as, uh, 'visualization.'
	// Audialization? Phonozation? Whatever. To check that it still works. 
	// That chunk is presumably of type Mix_Chunk, we'll then have to work backwards to elide that. 
	// Somewhere in the mix, throw the sound data onscreen, I guess. 
		// Ugh. Might have to extern some weird variables for that to work. 
		// I guess it's just screenbuffer? Yeah, we want to use the negative space, not cover the game render. 
	// snd is type allocated_sound_t. 
#ifdef SDL_MIXER
    Mix_PlayChannel(channel, &snd->chunk, 0);
#else
	// Headless demonstration that things are working: print SFX to the console. 
	// (Only SDL-free on *nix platforms. As i_system.c says, Windows 'helpfully' prints stdout to a file.) 
	// \n is undesirable, causing rapid scrolling for very little text - 
		// but without it, the console updates in great lumps of presumably 1024 characters at a time. 
	// Without SDL_TIMER it's still completely ridiculous. -playdemo finishes instantly. 
	// If you forget a -playdemo, you get a sky-high wall of six-character SFX names. 
	// Basically, whatever you're using JGHG for - comment this out as soon as possible. 
	printf("%s \n", sfxinfo->name ); 
#endif 

	// Stupid visualization stuff:
	// Ech, using screenbuffer threw some confusing errors. 
	// This is making me question how "extern" works and I see basically no reason to care.
	// We already have a chopped-down source file that gets all the way to "play sound now" and then doesn't. 
	// We know the data isn't SDL-dependent because we chopped out the SDL struct and replaced it. 
	// And even if some initial version of this fucks up and only really does the d_main half of audio stuff - oh well? 
	// The goal is a version of Doom with negligible dependencies, to include input / output. 

    channels_playing[channel] = snd;

    // set separation, etc.
    I_SDL_UpdateSoundParams(channel, vol, sep);

    return channel;
}

static void I_SDL_StopSound(int handle)
{
    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
    {
        return;
    }

    // Sound data is no longer needed; release the
    // sound data being used for this channel

    ReleaseSoundOnChannel(handle);
}


static boolean I_SDL_SoundIsPlaying(int handle)
{
    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
    {
        return false;
    }

#ifdef SDL_MIXER
    return Mix_Playing(handle);
#endif
}

//
// Periodically called to update the sound system
//

static void I_SDL_UpdateSound(void)
{
    int i;

    // Check all channels to see if a sound has finished

    for (i=0; i<NUM_CHANNELS; ++i)
    {
        if (channels_playing[i] && !I_SDL_SoundIsPlaying(i))
        {
            // Sound has finished playing on this channel,
            // but sound data has not been released to cache

            ReleaseSoundOnChannel(i);
        }
    }
}

static void I_SDL_ShutdownSound(void)
{
    if (!sound_initialized)
    {
        return;
    }

#ifdef SDL_MIXER
    Mix_CloseAudio();
#endif

#ifdef SDL_AUDIO
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
#endif
    sound_initialized = false;
}

// Calculate slice size, based on snd_maxslicetime_ms.
// The result must be a power of two.
static int GetSliceSize(void)
{
    int limit;
    int n;

    limit = (snd_samplerate * snd_maxslicetime_ms) / 1000;

    // Try all powers of two, not exceeding the limit.

    for (n=0;; ++n)
    {
        // 2^n <= limit < 2^n+1 ?

        if ((1 << (n + 1)) > limit)
        {
            return (1 << n);
        }
    }

    // Should never happen?

    return 1024;
}

static boolean I_SDL_InitSound(boolean _use_sfx_prefix)
{
    int i;

#ifdef SDL_AUDIO
    // SDL 2.0.6 has a bug that makes it unusable.
    if (SDL_COMPILEDVERSION == SDL_VERSIONNUM(2, 0, 6))
    {
        I_Error(
            "I_SDL_InitSound: "
            "You are trying to launch with SDL 2.0.6 which has a known bug "
            "that makes the game crash. Please either downgrade to "
            "SDL 2.0.5 or upgrade to 2.0.7. See the following bug for some "
            "additional context:\n"
            "<https://github.com/chocolate-doom/chocolate-doom/issues/945>");
    }
#endif

    use_sfx_prefix = _use_sfx_prefix;

    // No sounds yet
    for (i=0; i<NUM_CHANNELS; ++i)
    {
        channels_playing[i] = NULL;
    }

	// Debug:
//	return false; 
	// Yeah, I figured. If we return false for an InitSound, it's just silent - no segfaults. 
#ifdef SDL_AUDIO
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        fprintf(stderr, "Unable to set up sound.\n");
        return false;
    }
#endif

	// This is surely the actual 'gimme the sound card' call. 
	// Annoyingly side-effectful. If it returned some hideous custom type, I'd know what to dummy out. 
	// There's no global handle that gets set up shortly before or after this. 
	// Sooo whatever function loads stuff 'to Mix' is surely another Mix_whatever function. 
	// Which means we don't really have to care what Mix's stuff does, exactly; 
		// we just need to dummy out whatever hands audio data to it. 
	// So find that. Comment it out. Write it to a buffer instead.
	// Write from the buffer to the Mix function to make sure it works.
	// Write from the buffer to the screen to prove we still have audio while doing the next step:
	// Remove the Mix functions and SDL_Mixer.h. 
#ifdef SDL_MIXER
    if (Mix_OpenAudio(snd_samplerate, AUDIO_S16SYS, 2, GetSliceSize()) < 0)
    {
        fprintf(stderr, "Error initialising SDL_mixer: %s\n", Mix_GetError());
        return false;
    }

    Mix_QuerySpec(&mixer_freq, &mixer_format, &mixer_channels);
#endif

    ExpandSoundData = ExpandSoundData_SDL;

    if (use_libsamplerate != 0)
    {
        fprintf(stderr, "I_SDL_InitSound: use_libsamplerate=%i, but "
                        "libsamplerate support not compiled in.\n",
                        use_libsamplerate);
    }

#ifdef SDL_MIXER
    Mix_AllocateChannels(NUM_CHANNELS);
#endif

#ifdef SDL_AUDIO
    SDL_PauseAudio(0);
#endif

    sound_initialized = true;

    return true;
}

static snddevice_t sound_sdl_devices[] = 
{
    SNDDEVICE_SB,
    SNDDEVICE_PAS,
    SNDDEVICE_GUS,
    SNDDEVICE_WAVEBLASTER,
    SNDDEVICE_SOUNDCANVAS,
    SNDDEVICE_AWE32,
};

sound_module_t sound_sdl_module = 
{
    sound_sdl_devices,
    arrlen(sound_sdl_devices),
    I_SDL_InitSound,
    I_SDL_ShutdownSound,
    I_SDL_GetSfxLumpNum,
    I_SDL_UpdateSound,
    I_SDL_UpdateSoundParams,
    I_SDL_StartSound,
    I_SDL_StopSound,
    I_SDL_SoundIsPlaying,
    I_SDL_PrecacheSounds,
};

