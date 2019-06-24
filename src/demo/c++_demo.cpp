/* 
 * VControl test harness.  This is Public Domain, but it's worth
 * noting that VControl itself is provided under the terms of the zlib
 * license and the SDL library is provided under the terms of the
 * LGPL.
 */

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "vcontrol.h"

using namespace std;

class DemoState {
public:
	int up, down, left, right, fire, special;
	int operator!= (const DemoState &);
	void dumpStatus(void);
};

void DemoState::dumpStatus ()
{
	cout << ("Status:");
	if (up) cout << (" up");
	if (down) cout << (" down");
	if (left) cout << (" left");
	if (right) cout << (" right");
	if (fire) cout << (" fire");
	if (special) cout << (" special");
	cout << endl;
}

int DemoState::operator!= (const DemoState &target)
{
	return ((up == 0) != (target.up == 0)) || 
		((down == 0) != (target.down == 0)) ||
		((left == 0) != (target.left == 0)) || 
		((right == 0) != (target.right == 0)) ||
		((fire == 0) != (target.fire == 0)) || 
		((special == 0) != (target.special == 0));
}

class DemoInput {
	DemoState old, current;
	VControl_NameBinding table[7];
public:
	DemoInput (void);
	~DemoInput (void);
	void update (void);
};	

DemoInput::DemoInput (void)
{
	table[0].name = strdup("Up");
	table[0].target = &current.up;
	table[1].name = strdup("Down");
	table[1].target = &current.down;
	table[2].name = strdup("Left");
	table[2].target = &current.left;
	table[3].name = strdup("Right");
	table[3].target = &current.right;
	table[4].name = strdup("Fire");
	table[4].target = &current.fire;
	table[5].name = strdup("Special");
	table[5].target = &current.special;
	table[6].name = NULL;
	table[6].target = NULL;
	VControl_RegisterNameTable (table);
}

DemoInput::~DemoInput ()
{
	for (int i = 0; i < 6; ++i) {
		free(table[i].name);
	}
}
void DemoInput::update ()
{
	if (old != current) {
		current.dumpStatus();
	}
	old = current;
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
#if SDL_MAJOR_VERSION == 1
        if (!SDL_SetVideoMode(100, 100, 16, 0))
#else
        SDL_Window *screen = SDL_CreateWindow("Test window", SDL_WINDOWPOS_UNDEFINED,
                        SDL_WINDOWPOS_UNDEFINED, 100, 100, 0);
        if (!screen)
#endif
	{
		fprintf (stderr, "Doom!  Couldn't initialize SDL Video: %s\n", SDL_GetError());
		exit(1);
	}

	SDL_JoystickEventState (SDL_ENABLE);
	VControl_Init ();

	DemoInput input;

	FILE *x = fopen ("test.cfg", "rt");
	int errs = VControl_ReadConfiguration (x);
	fclose (x);
	printf ("%d errors in config file.\n", errs);
	VControl_Dump (stdout);

	VControl_ResetInput ();

	while (!done)
	{
		SDL_Event event;
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
		input.update();
	}

	VControl_Uninit ();
	return 0;
}
