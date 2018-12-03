/* 
 * VControl test harness.  This is Public Domain, but it's worth
 * noting that VControl itself is provided under the terms of the zlib
 * license and the SDL library is provided under the terms of the
 * LGPL.
 */

#include <stdlib.h>
#include <SDL.h>
#include "vcontrol.h"

typedef struct _demo_input_state {
	int up, down, left, right, fire, special;
} DemoInput;

DemoInput old, current;

VControl_NameBinding controltable[] = {
	{"Up", &current.up},
	{"Down", &current.down},
	{"Left", &current.left},
	{"Right", &current.right},
	{"Fire", &current.fire},
	{"Special", &current.special},
	{0, 0}};

void dumpStatus (DemoInput *x)
{
	printf ("Status:");
	if (x->up) printf (" up");
	if (x->down) printf (" down");
	if (x->left) printf (" left");
	if (x->right) printf (" right");
	if (x->fire) printf (" fire");
	if (x->special) printf (" special");
	printf ("\n");
}

int changedInput (void)
{
	return ((old.up == 0) != (current.up == 0)) || 
		((old.down == 0) != (current.down == 0)) ||
		((old.left == 0) != (current.left == 0)) || 
		((old.right == 0) != (current.right == 0)) ||
		((old.fire == 0) != (current.fire == 0)) || 
		((old.special == 0) != (current.special == 0));
}

int
main (int argc, char **argv)
{
	int done = 0;
	if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0)
	{
		fprintf (stderr, "Doom!  Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}

	atexit(SDL_Quit);
	if (!SDL_SetVideoMode(100, 100, 16, 0))
	{
		fprintf (stderr, "Doom!  Couldn't initialize SDL Video: %s\n", SDL_GetError());
		exit(1);
	}

	SDL_JoystickEventState (SDL_ENABLE);

	VControl_Init ();
	VControl_RegisterNameTable (controltable);

	{
		FILE *x = fopen ("test.cfg", "rt");
		int errs = VControl_ReadConfiguration (x);
		fclose (x);
		printf ("%d errors in config file.\n", errs);
		VControl_Dump (stdout);
	}

	VControl_ResetInput ();

	while (!done)
	{
		SDL_Event event;
		old = current;
		while (SDL_PollEvent(&event))
		{
			switch (event.type) 
			{
			case SDL_QUIT:
				done = 1;
				break;
			}
			VControl_HandleEvent (&event);
		}
		if (changedInput ())
		{
			dumpStatus (&current);
		}
	}

	VControl_Uninit ();
	return 0;
}
