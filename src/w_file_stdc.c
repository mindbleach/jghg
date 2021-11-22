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
//	WAD I/O functions.
//

#include <stdio.h>

#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"

// Fragglet's minimalist IWAD, as data inside the program, so you don't need an external IWAD.
// Format is unsigned char miniwad_wad[] and unsigned int miniwad_wad_len = 230497. 
// Not 100% sure I want to bother removing stdio.h the way I've removed SDL. 
// As mentioned elsewhere, JGHG's ideal form is one big dumb C file, without dependencies... 
// But that's not the minimum viable product. 
//#include "i_miniwad.h"

typedef struct
{
    wad_file_t wad;
    FILE *fstream;
} stdc_wad_file_t;

extern wad_file_class_t stdc_wad_file;

// Jesus FUCKING Christ. 
// This is - finally! - the part of the source code that opens a goddamn WAD file. 
// d_iwad is a jumble of filenames to check, in directories to check, and fuckin' registry entries to check?
// But it uses M_FileCaseExists, in m_misc. Which apparently never fires. 
// So instead I guessed - somehow - that I wanted W_AddFile in w_wad.c. 
// Which calls W_OpenFile in w_file.c. 
// Which obliquely references &stdc_wad_file here in w_file_stc.c.
// Except &stdc_wad_file->OpenFile is itself an oblique reference to W_StdC_OpenFIle, below. 
// And this function - at the very fucking least - opens doom1.wad. 
	// So.
// Presumably what I want is to drop-in replacement for a filestream, so the game reads data elsewhere in the code. 
// And then I need to copy that minimalist IWAD from the Doomworld forums into some text data format. 
// (Assuming it's licensed appropriately. If not, I'll fake it myself, in a few hours.) 
// This returns a wad_file_t, which is separate from the file stream in stdc_wad_file_t. 
// But I assume there's some disgusting low-level shenanigans invovled. 
// Otherwise it makes no sense to set result.fstream and only return result.wad. 
// Where the fuck is wad_file_t defined? 
// Okay, it's in w_file.h, but the mismatched names for a "typedef struct" patter are just... bleh. 
	// SO.
// stdc_wad_file_t contains a wad_file_t as its first member. 
// wad_file_t has a "file class" as its first member; not sure what it actually does. 
// Second member is a pointer to data. Third member is the length of that data. 
static wad_file_t *W_StdC_OpenFile(char *path)
{
    stdc_wad_file_t *result;
    FILE *fstream;
/*
//	#define FAKE_IWAD 		// Here for now, if at all. 

	// Failed first whack. Would that it were so easy. 
	// Tried removing freshly-declared null pointers - no dice. 
	// Tried moving this further down, so fstream is real - no dice. 
	// Consider: meh. 
	#ifdef FAKE_IWAD
    result = Z_Malloc(sizeof(stdc_wad_file_t), PU_STATIC, 0);
    result->wad.file_class = &stdc_wad_file;
    result->wad.mapped = NULL;
//    result->wad.length = M_FileLength(fstream);
    result->wad.path = M_StringDuplicate(path);
//    result->fstream = fstream;

	result->wad.mapped = miniwad_wad; 
	result->wad.length = miniwad_wad_len; 

    return &result->wad;
	#endif
*/

	// Debug:
	printf( "w_file_stdc.c - W_StdC_OpenFile path: %s \n", path ); 

    fstream = fopen(path, "rb");

	// This is where we'd copy a bunch of the result->etc stuff below, to instead insert a minimal IWAD.
	// 
    if (fstream == NULL)
    {
        return NULL;
    }

    // Create a new stdc_wad_file_t to hold the file handle.
	// Length will be fixed, but we can presumably use sizeof() anyhow. 
	// Path doesn't need to be real but surely does need to be unique. 
    result = Z_Malloc(sizeof(stdc_wad_file_t), PU_STATIC, 0);
    result->wad.file_class = &stdc_wad_file;
    result->wad.mapped = NULL;
    result->wad.length = M_FileLength(fstream);
    result->wad.path = M_StringDuplicate(path);
    result->fstream = fstream;

	// Debug:
//	printf( "w_file_stdc.c - stdc_wad_file_t length: %u \n", result->wad.length );
//	printf( "w_file_stdc.c - miniwad_wad_len: %u \n", miniwad_wad_len );  

	// Can I just... ? 
	// Segfaults. Not especially surprising. 
	// On a whim, tried =*miniwad_wad, which segfaults immediately. =miniwad_wad waits until M_Init. 
	// *miniwad_wad also throws a warning: 
		// assignment to ‘byte *’ {aka ‘unsigned char *’} from ‘unsigned char’ makes pointer from integer without a cast
	// Putting -iwad miniwad.wad in the terminal didn't work, but it was worth a shot. 
	// Is it because we do file things beforehand? We do open a file, not read it, and... hang on. 
	// wad.mapped is null, above. So. Once again. Where the fuck does this stupid hack need to go? 
//	result->wad.mapped = miniwad_wad; 
//	result->wad.length = miniwad_wad_len; 

    return &result->wad;
}

static void W_StdC_CloseFile(wad_file_t *wad)
{
    stdc_wad_file_t *stdc_wad;

    stdc_wad = (stdc_wad_file_t *) wad;

    fclose(stdc_wad->fstream);
    Z_Free(stdc_wad);
}

// Read data from the specified position in the file into the 
// provided buffer.  Returns the number of bytes read.

size_t W_StdC_Read(wad_file_t *wad, unsigned int offset,
                   void *buffer, size_t buffer_len)
{
    stdc_wad_file_t *stdc_wad;
    size_t result;

    stdc_wad = (stdc_wad_file_t *) wad;

    // Jump to the specified position in the file.

    fseek(stdc_wad->fstream, offset, SEEK_SET);

    // Read into the buffer.

    result = fread(buffer, 1, buffer_len, stdc_wad->fstream);

    return result;
}


wad_file_class_t stdc_wad_file = 
{
    W_StdC_OpenFile,
    W_StdC_CloseFile,
    W_StdC_Read,
};


