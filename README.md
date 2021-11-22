This is Chocolate Doom without SDL.

No replacement is provided. 

By design, you cannot play this version of Doom. 

It is a fork with minimal dependencies - to make new source ports easier.

The only indication it's working is text in the terminal - the familiar 
start-up routine, followed by the names of sound effects. 

In the spirit of every Doom-related project having a name which is either 
completely on-the-nose (ZDoom, Boom, Smooth Doom) or else as oblique as 
possible (Zandronum, Marine's Best Friend, Hideous Destructor), I have chosen
to name this:

# Johnny Got His Gun

This is a version of Doom which, as hosted here:
 * Has no video output
 * Has no audio output
 * Has no joystick input
 * Has no mouse input
 * Has no keyboard input

And yet, with the full accuracy of Chocolate Doom 3.0.1:
 * Still maintains internal state
 * Still renders frames
 * Still reads music
 * Still tracks sound effects

As provided, several sub-projects still depend on SDL. Chocolate Doom targets
related games like Heretic, Hexen, and Strife. Those are largely unchanged. 
Much of their code is shared with Doom proper. It should be roughly as easy to
extend this project to them as it is to exclude them. 

Chocolate Doom was chosen as a basis because it is written entirely in C. Most
other simple or low-end versions retain the original 1990s ASM, which is not 
helpful for anything besides 32-bit x86 DOS, and FastDoom has that covered. 

Note that Chocolate Doom still assumes a 32-bit environment. Generic C types
like "int" are mixed in with specific C++ types like "int32_t". Compilers on
16-bit systems will require significant change. 

Fragglet's "miniwad.wad" is included for convenience. 
