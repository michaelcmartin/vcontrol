/* 
 * VControl Library, Copyright (c) 2003, Michael Martin
 *
 * VControl is distributed under the terms of the zlib license, and as
 * such has NO WARRANTY.  See the LICENSE file for details.
 */

#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "vcontrol.h"
#include "keynames.h"

/* If we're in Windows, we don't have strcasecmp */
#ifdef WIN32
#define strcasecmp stricmp
#endif

/* How many binding slots are allocated at once. */
#define POOL_CHUNK_SIZE 64

/* Total number of key input buckets. SDL1 keys are a simple enum,
 * but SDL2 scatters key symbols through the entire 32-bit space,
 * so we do not rely on being able to declare an array with one
 * entry per key. */
#define KEYBOARD_INPUT_BUCKETS 512

typedef struct vcontrol_keybinding_s {
	int *target;
	sdl_key_t keycode;
	struct vcontrol_keypool_s *parent;
	struct vcontrol_keybinding_s *next;
} keybinding;

typedef struct vcontrol_keypool_s {
	keybinding pool[POOL_CHUNK_SIZE];
	int remaining;
	struct vcontrol_keypool_s *next;
} keypool;

typedef struct vcontrol_joystick_axis_s {
	keybinding *neg, *pos;
	int polarity;
} axis;

typedef struct vcontrol_joystick_hat_s {
	keybinding *left, *right, *up, *down;
	Uint8 last;
} hat;

typedef struct vcontrol_joystick_s {
	SDL_Joystick *stick;
	int numaxes, numbuttons, numhats;
	int threshold;
	axis *axes;
	keybinding **buttons;
	hat *hats;
} joystick;

static keybinding *bindings[KEYBOARD_INPUT_BUCKETS];
static joystick *joysticks;
static int joycount;

static keypool *pool;
static VControl_NameBinding *nametable;

static keypool *
allocate_key_chunk (void)
{
	keypool *x = malloc (sizeof (keypool));
	if (x)
	{
		int i;
		x->remaining = POOL_CHUNK_SIZE;
		x->next = NULL;
		for (i = 0; i < POOL_CHUNK_SIZE; i++)
		{
			x->pool[i].target = NULL;
			x->pool[i].keycode = SDLK_UNKNOWN;
			x->pool[i].next = NULL;
			x->pool[i].parent = x;
		}
	}
	return x;
}

static void
free_key_pool (keypool *x)
{
	if (x)
	{
		free_key_pool (x->next);
		free (x);
	}
}

static void
create_joystick (int index)
{
	SDL_Joystick *stick;
	int axes, buttons, hats;
	if (index >= joycount)
	{
		fprintf (stderr, "VControl warning: Tried to open a non-existent joystick!");
		return;
	}
	if (joysticks[index].stick)
	{
		// Joystick is already created.  Return.
		return;
	}
	stick = SDL_JoystickOpen (index);
	if (stick)
	{
		joystick *x = &joysticks[index];
		int j;
#if SDL_MAJOR_VERSION == 1
		fprintf (stderr, "VControl opened joystick: %s\n", SDL_JoystickName (index));
#else
		fprintf (stderr, "VControl opened joystick: %s\n", SDL_JoystickName (stick));
#endif
		axes = SDL_JoystickNumAxes (stick);
		buttons = SDL_JoystickNumButtons (stick);
		hats = SDL_JoystickNumHats (stick);
		fprintf (stderr, "%d axes, %d buttons, %d hats.\n", axes, buttons, hats);
		x->numaxes = axes;
		x->numbuttons = buttons;
		x->numhats = hats;
		x->axes = malloc (sizeof (axis) * axes);
		x->buttons = malloc (sizeof (keybinding *) * buttons);
		x->hats = malloc (sizeof (hat) * hats);
		for (j = 0; j < axes; j++)
		{
			x->axes[j].neg = x->axes[j].pos = NULL;
		}
		for (j = 0; j < hats; j++)
		{
			x->hats[j].left = x->hats[j].right = NULL;
			x->hats[j].up = x->hats[j].down = NULL;
			x->hats[j].last = SDL_HAT_CENTERED;
		}
		for (j = 0; j < buttons; j++)
		{
			x->buttons[j] = NULL;
		}
		x->stick = stick;
	}
	else
	{
		fprintf (stderr, "VControl: Could not initialize joystick #%d\n", index);
	}
}
			
static void
destroy_joystick (int index)
{
	SDL_Joystick *stick = joysticks[index].stick;
	if (stick)
	{
		SDL_JoystickClose (stick);
		joysticks[index].stick = NULL;
		free (joysticks[index].axes);
		free (joysticks[index].buttons);
		free (joysticks[index].hats);
		joysticks[index].numaxes = joysticks[index].numbuttons = 0;
		joysticks[index].axes = NULL;
		joysticks[index].buttons = NULL;
		joysticks[index].hats = NULL;
	}
}

static void
key_init (void)
{
	int i;
	pool = allocate_key_chunk ();
	for (i = 0; i < KEYBOARD_INPUT_BUCKETS; i++)
		bindings[i] = NULL;
	/* Prepare for possible joystick controls.  We don't actually
	   GRAB joysticks unless we're asked to make a joystick
	   binding, though. */
	joycount = SDL_NumJoysticks ();
	if (joycount)
	{
		joysticks = malloc (sizeof (joystick) * joycount);
		for (i = 0; i < joycount; i++)
		{
			joysticks[i].stick = NULL;	
			joysticks[i].numaxes = joysticks[i].numbuttons = 0;
			joysticks[i].axes = NULL;
			joysticks[i].buttons = NULL;
		}
	}
	else
	{
		joysticks = NULL;
	}
}

static void
key_uninit (void)
{
	int i;
	free_key_pool (pool);
	for (i = 0; i < KEYBOARD_INPUT_BUCKETS; i++)
		bindings[i] = NULL;
	pool = NULL;
	for (i = 0; i < joycount; i++)
		destroy_joystick (i);
	free (joysticks);
}

static void
name_init (void)
{
	nametable = NULL;
}

static void
name_uninit (void)
{
	nametable = NULL;
}

void
VControl_Init (void)
{
	key_init ();
	name_init ();
}

void
VControl_Uninit (void)
{
	key_uninit ();
	name_uninit ();
}

int
VControl_SetJoyThreshold (int port, int threshold)
{
	if (port >= 0 && port < joycount)
	{
		joysticks[port].threshold = threshold;
	}
	else
	{
		fprintf (stderr, "VControl_SetJoyThreshold passed illegal port %d\n", port);
		return -1;
	}
	return 0;
}


static void
add_binding (keybinding **newptr, int *target, sdl_key_t keycode)
{
	keybinding *newbinding;
	keypool *searchbase;
	int i;

	/* Acquire a pointer to the keybinding * that we'll be
	 * overwriting.  Along the way, ensure we haven't already
	 * bound this symbol to this target.  If we have, return.*/
	while (*newptr != NULL)
	{
		if (((*newptr)->target == target) && ((*newptr)->keycode == keycode))
		{
			return;
		}
		newptr = &((*newptr)->next);
	}

	/* Now hunt through the binding pool for a free binding. */

	/* First, find a chunk with free spots in it */

	searchbase = pool;
	while (searchbase->remaining == 0)
	{
		/* If we're completely full, allocate a new chunk */
		if (searchbase->next == NULL)
		{
			searchbase->next = allocate_key_chunk ();
		}
		searchbase = searchbase->next;
	}

	/* Now find a free binding within it */

	newbinding = NULL;
	for (i = 0; i < POOL_CHUNK_SIZE; i++)
	{
		if (searchbase->pool[i].target == NULL)
		{
			newbinding = &searchbase->pool[i];
			break;
		}
	}

	/* Sanity check. */
	if (!newbinding)
	{
		fprintf (stderr, "VControl_AddKeyBinding failed to find a free binding slot!\n");
		return;
	}

	newbinding->target = target;
	newbinding->keycode = keycode;
	newbinding->next = NULL;
	*newptr = newbinding;
	searchbase->remaining--;
}

static void
remove_binding (keybinding **ptr, int *target, sdl_key_t keycode)
{
	if (!(*ptr))
	{
		/* Nothing bound to symbol; return. */
		return;
	}
	else if (((*ptr)->target == target) && ((*ptr)->keycode == keycode))
	{
		keybinding *todel = *ptr;
		*ptr = todel->next;
		todel->target = NULL;
		todel->keycode = SDLK_UNKNOWN;
		todel->next = NULL;
		todel->parent->remaining++;
	}
	else
	{
		keybinding *prev = *ptr;
		while (prev->next != NULL)
		{
			if (prev->next->target == target)
			{
				keybinding *todel = prev->next;
				prev->next = todel->next;
				todel->target = NULL;
				todel->keycode = SDLK_UNKNOWN;
				todel->next = NULL;
				todel->parent->remaining++;
			}
		}
	}
}

static void
activate (keybinding *i, sdl_key_t keycode)
{
	while (i != NULL)
	{
		if (i->keycode == keycode) {
			*(i->target) = *(i->target)+1;
		}
		i = i->next;
	}
}

static void
deactivate (keybinding *i, sdl_key_t keycode)
{
	while (i != NULL)
	{
		if ((i->keycode == keycode) && (*(i->target) > 0))
		{
			*(i->target) = *(i->target)-1;
		}
		i = i->next;
	}
}

int
VControl_AddBinding (SDL_Event *e, int *target)
{
	int result;
	switch (e->type)
	{
	case SDL_KEYDOWN:
		result = VControl_AddKeyBinding (e->key.keysym.sym, target);
		break;
	case SDL_JOYAXISMOTION:
		result = VControl_AddJoyAxisBinding (e->jaxis.which, e->jaxis.axis, (e->jaxis.value < 0) ? -1 : 1, target);
		break;
	case SDL_JOYHATMOTION:
		result = VControl_AddJoyHatBinding (e->jhat.which, e->jhat.hat, e->jhat.value, target);
		break;
	case SDL_JOYBUTTONDOWN:
		result = VControl_AddJoyButtonBinding (e->jbutton.which, e->jbutton.button, target);
		break;
	default:
		fprintf (stderr, "VControl_AddBinding didn't understand argument event\n");
		result = -1;
		break;
	}
	return result;
}

void
VControl_RemoveBinding (SDL_Event *e, int *target)
{
	switch (e->type)
	{
	case SDL_KEYDOWN:
		VControl_RemoveKeyBinding (e->key.keysym.sym, target);
		break;
	case SDL_JOYAXISMOTION:
		VControl_RemoveJoyAxisBinding (e->jaxis.which, e->jaxis.axis, (e->jaxis.value < 0) ? -1 : 1, target);
		break;
	case SDL_JOYHATMOTION:
		VControl_RemoveJoyHatBinding (e->jhat.which, e->jhat.hat, e->jhat.value, target);
		break;
	case SDL_JOYBUTTONDOWN:
		VControl_RemoveJoyButtonBinding (e->jbutton.which, e->jbutton.button, target);
		break;
	default:
		fprintf (stderr, "VControl_RemoveBinding didn't understand argument event\n");
		break;
	}
}

int
VControl_AddKeyBinding (sdl_key_t symbol, int *target)
{
	add_binding(&bindings[symbol % KEYBOARD_INPUT_BUCKETS], target, symbol);
	return 0;
}

void
VControl_RemoveKeyBinding (sdl_key_t symbol, int *target)
{
	remove_binding (&bindings[symbol % KEYBOARD_INPUT_BUCKETS], target, symbol);
}

int
VControl_AddJoyAxisBinding (int port, int axis, int polarity, int *target)
{
	if (port >= 0 && port < joycount)
	{
		joystick *j = &joysticks[port];
		if (!(j->stick))
			create_joystick (port);
		if ((axis >= 0) && (axis < j->numaxes))
		{
			if (polarity < 0)
			{
				add_binding(&joysticks[port].axes[axis].neg, target, SDLK_UNKNOWN);
			}
			else if (polarity > 0)
			{
				add_binding(&joysticks[port].axes[axis].pos, target, SDLK_UNKNOWN);
			}
			else
			{
				fprintf (stderr, "VControl: Attempted to bind to polarity zero\n");
				return -1;
			}
		}
		else
		{
			fprintf (stderr, "VControl: Attempted to bind to illegal axis %d\n", axis);
			return -1;
		}
	}
	else
	{
		fprintf (stderr, "VControl: Attempted to bind to illegal port %d\n", port);
		return -1;
	}
	return 0;
}

void
VControl_RemoveJoyAxisBinding (int port, int axis, int polarity, int *target)
{
	if (port >= 0 && port < joycount)
	{
		joystick *j = &joysticks[port];
		if (!(j->stick))
			create_joystick (port);
		if ((axis >= 0) && (axis < j->numaxes))
		{
			if (polarity < 0)
			{
				remove_binding(&joysticks[port].axes[axis].neg, target, SDLK_UNKNOWN);
			}
			else if (polarity > 0)
			{
				remove_binding(&joysticks[port].axes[axis].pos, target, SDLK_UNKNOWN);
			}
			else
			{
				fprintf (stderr, "VControl: Attempted to unbind from polarity zero\n");
			}
		}
		else
		{
			fprintf (stderr, "VControl: Attempted to unbind from illegal axis %d\n", axis);
		}
	}
	else
	{
		fprintf (stderr, "VControl: Attempted to unbind from illegal port %d\n", port);
	}
}

int
VControl_AddJoyButtonBinding (int port, int button, int *target)
{
	if (port >= 0 && port < joycount)
	{
		joystick *j = &joysticks[port];
		if (!(j->stick))
			create_joystick (port);
		if ((button >= 0) && (button < j->numbuttons))
		{
			add_binding(&joysticks[port].buttons[button], target, SDLK_UNKNOWN);
		}
		else
		{
			fprintf (stderr, "VControl: Attempted to bind to illegal button %d\n", button);
			return -1;
		}
	}
	else
	{
		fprintf (stderr, "VControl: Attempted to bind to illegal port %d\n", port);
		return -1;
	}
	return 0;
}

void
VControl_RemoveJoyButtonBinding (int port, int button, int *target)
{
	if (port >= 0 && port < joycount)
	{
		joystick *j = &joysticks[port];
		if (!(j->stick))
			create_joystick (port);
		if ((button >= 0) && (button < j->numbuttons))
		{
			remove_binding (&joysticks[port].buttons[button], target, SDLK_UNKNOWN);
		}
		else
		{
			fprintf (stderr, "VControl: Attempted to unbind from illegal button %d\n", button);
		}
	}
	else
	{
		fprintf (stderr, "VControl: Attempted to unbind from illegal port %d\n", port);
	}
}

int
VControl_AddJoyHatBinding (int port, int which, Uint8 dir, int *target)
{
	if (port >= 0 && port < joycount)
	{
		joystick *j = &joysticks[port];
		if (!(j->stick))
			create_joystick (port);
		if ((which >= 0) && (which < j->numhats))
		{
			if (dir == SDL_HAT_LEFT)
			{
				add_binding(&joysticks[port].hats[which].left, target, SDLK_UNKNOWN);
			}
			else if (dir == SDL_HAT_RIGHT)
			{
				add_binding(&joysticks[port].hats[which].right, target, SDLK_UNKNOWN);
			}
			else if (dir == SDL_HAT_UP)
			{
				add_binding(&joysticks[port].hats[which].up, target, SDLK_UNKNOWN);
			}
			else if (dir == SDL_HAT_DOWN)
			{
				add_binding(&joysticks[port].hats[which].down, target, SDLK_UNKNOWN);
			}
			else
			{
				fprintf (stderr, "VControl: Attempted to bind to illegal direction\n");
				return -1;
			}
		}
		else
		{
			fprintf (stderr, "VControl: Attempted to bind to illegal hat %d\n", which);
			return -1;
		}
	}
	else
	{
		fprintf (stderr, "VControl: Attempted to bind to illegal port %d\n", port);
		return -1;
	}
	return 0;
}

void
VControl_RemoveJoyHatBinding (int port, int which, Uint8 dir, int *target)
{
	if (port >= 0 && port < joycount)
	{
		joystick *j = &joysticks[port];
		if (!(j->stick))
			create_joystick (port);
		if ((which >= 0) && (which < j->numhats))
		{
			if (dir == SDL_HAT_LEFT)
			{
				remove_binding(&joysticks[port].hats[which].left, target, SDLK_UNKNOWN);
			}
			else if (dir == SDL_HAT_RIGHT)
			{
				remove_binding(&joysticks[port].hats[which].right, target, SDLK_UNKNOWN);
			}
			else if (dir == SDL_HAT_UP)
			{
				remove_binding(&joysticks[port].hats[which].up, target, SDLK_UNKNOWN);
			}
			else if (dir == SDL_HAT_DOWN)
			{
				remove_binding(&joysticks[port].hats[which].down, target, SDLK_UNKNOWN);
			}
			else
			{
				fprintf (stderr, "VControl: Attempted to unbind from illegal direction\n");
			}
		}
		else
		{
			fprintf (stderr, "VControl: Attempted to unbind from illegal hat %d\n", which);
		}
	}
	else
	{
		fprintf (stderr, "VControl: Attempted to unbind from illegal port %d\n", port);
	}
}

void
VControl_RemoveAllBindings ()
{
	key_uninit ();
	key_init ();
}

void
VControl_ProcessKeyDown (sdl_key_t symbol)
{
	activate (bindings[symbol % KEYBOARD_INPUT_BUCKETS], symbol);
}

void
VControl_ProcessKeyUp (sdl_key_t symbol)
{
	deactivate (bindings[symbol % KEYBOARD_INPUT_BUCKETS], symbol);
}

void
VControl_ProcessJoyButtonDown (int port, int button)
{
	if (!joysticks[port].stick)
		return;
	activate (joysticks[port].buttons[button], SDLK_UNKNOWN);
}

void
VControl_ProcessJoyButtonUp (int port, int button)
{
	if (!joysticks[port].stick)
		return;
	deactivate (joysticks[port].buttons[button], SDLK_UNKNOWN);
}

void
VControl_ProcessJoyAxis (int port, int axis, int value)
{
	int t;
	if (!joysticks[port].stick)
		return;
	t = joysticks[port].threshold;
	if (value > t)
	{
		if (joysticks[port].axes[axis].polarity != 1)
		{
			if (joysticks[port].axes[axis].polarity == -1)
			{
				deactivate (joysticks[port].axes[axis].neg, SDLK_UNKNOWN);
			}
			joysticks[port].axes[axis].polarity = 1;
			activate (joysticks[port].axes[axis].pos, SDLK_UNKNOWN);
		}
	}
	else if (value < -t)
	{
		if (joysticks[port].axes[axis].polarity != -1)
		{
			if (joysticks[port].axes[axis].polarity == 1)
			{
				deactivate (joysticks[port].axes[axis].pos, SDLK_UNKNOWN);
			}
			joysticks[port].axes[axis].polarity = -1;
			activate (joysticks[port].axes[axis].neg, SDLK_UNKNOWN);
		}
	}
	else
	{
		if (joysticks[port].axes[axis].polarity == -1)
		{
			deactivate (joysticks[port].axes[axis].neg, SDLK_UNKNOWN);
		}
		else if (joysticks[port].axes[axis].polarity == 1)
		{
			deactivate (joysticks[port].axes[axis].pos, SDLK_UNKNOWN);
		}
		joysticks[port].axes[axis].polarity = 0;
	}
}

void
VControl_ProcessJoyHat (int port, int which, Uint8 value)
{
	Uint8 old;
	if (!joysticks[port].stick)
		return;
	old = joysticks[port].hats[which].last;
	if (!(old & SDL_HAT_LEFT) && (value & SDL_HAT_LEFT))
		activate (joysticks[port].hats[which].left, SDLK_UNKNOWN);
	if (!(old & SDL_HAT_RIGHT) && (value & SDL_HAT_RIGHT))
		activate (joysticks[port].hats[which].right, SDLK_UNKNOWN);
	if (!(old & SDL_HAT_UP) && (value & SDL_HAT_UP))
		activate (joysticks[port].hats[which].up, SDLK_UNKNOWN);
	if (!(old & SDL_HAT_DOWN) && (value & SDL_HAT_DOWN))
		activate (joysticks[port].hats[which].down, SDLK_UNKNOWN);
	if ((old & SDL_HAT_LEFT) && !(value & SDL_HAT_LEFT))
		deactivate (joysticks[port].hats[which].left, SDLK_UNKNOWN);
	if ((old & SDL_HAT_RIGHT) && !(value & SDL_HAT_RIGHT))
		deactivate (joysticks[port].hats[which].right, SDLK_UNKNOWN);
	if ((old & SDL_HAT_UP) && !(value & SDL_HAT_UP))
		deactivate (joysticks[port].hats[which].up, SDLK_UNKNOWN);
	if ((old & SDL_HAT_DOWN) && !(value & SDL_HAT_DOWN))
		deactivate (joysticks[port].hats[which].down, SDLK_UNKNOWN);
	joysticks[port].hats[which].last = value;
}

void
VControl_ResetInput ()
{
	/* Step through every valid entry in the binding pool and zero
	 * them out.  This will probably zero entries multiple times;
	 * oh well, no harm done. */

	keypool *base = pool;
	while (base != NULL)
	{
		int i;
		for (i = 0; i < POOL_CHUNK_SIZE; i++)
		{
			if(base->pool[i].target)
			{
				*(base->pool[i].target) = 0;
			}
		}
		base = base->next;
	}
}

void
VControl_HandleEvent (SDL_Event *e)
{
	switch (e->type)
	{
		case SDL_KEYDOWN:
#if SDL_MAJOR_VERSION > 1
			if (!e->key.repeat)
#endif
			{
				VControl_ProcessKeyDown (e->key.keysym.sym);
			}
			break;
		case SDL_KEYUP:
			VControl_ProcessKeyUp (e->key.keysym.sym);
			break;
		case SDL_JOYAXISMOTION:
			VControl_ProcessJoyAxis (e->jaxis.which, e->jaxis.axis, e->jaxis.value);
			break;
		case SDL_JOYHATMOTION:
			VControl_ProcessJoyHat (e->jhat.which, e->jhat.hat, e->jhat.value);
			break;
		case SDL_JOYBUTTONDOWN:
			VControl_ProcessJoyButtonDown (e->jbutton.which, e->jbutton.button);
			break;
		case SDL_JOYBUTTONUP:
			VControl_ProcessJoyButtonUp (e->jbutton.which, e->jbutton.button);
			break;
		default:
			break;
	}
}

void
VControl_RegisterNameTable (VControl_NameBinding *table)
{
	nametable = table;
}

static char *
target2name (int *target)
{
	VControl_NameBinding *b = nametable;
	while (b->target)
	{
		if (target == b->target)
		{
			return b->name;
		}
		++b;
	}
	return NULL;
}

static int *
name2target (char *name)
{
	VControl_NameBinding *b = nametable;
	while (b->target)
	{
		if (!strcasecmp (name, b->name))
		{
			return b->target;
		}
		++b;
	}
	return NULL;
}

static void
dump_keybindings (FILE *out, keybinding *kb, char *name)
{
	char namebuffer[64];
	while (kb != NULL)
	{
		char *targetname = target2name (kb->target);
		if (kb->keycode == SDLK_UNKNOWN) {
			fprintf (out, "%s: %s\n", targetname, name);
		} else {
			sprintf (namebuffer, "key %s", VControl_code2name (kb->keycode));
			fprintf (out, "%s: %s\n", targetname, namebuffer);
		}
		kb = kb->next;
	}
}

void
VControl_Dump (FILE *out)
{
	int i;
	char namebuffer[64];

	/* Print out keyboard bindings */
	for (i = 0; i < KEYBOARD_INPUT_BUCKETS; i++)
	{
		keybinding *kb = bindings[i];		
		if (kb != NULL)
		{
			dump_keybindings (out, kb, "<Unknown key>");
		}
	}

	/* Print out joystick bindings */
	for (i = 0; i < joycount; i++)
	{
		if (joysticks[i].stick)
		{
			int j;

			fprintf (out, "joystick %d threshold %d\n", i, joysticks[i].threshold);
			for (j = 0; j < joysticks[i].numaxes; j++)
			{
				sprintf (namebuffer, "joystick %d axis %d negative", i, j);
				dump_keybindings (out, joysticks[i].axes[j].neg, namebuffer);
				sprintf (namebuffer, "joystick %d axis %d positive", i, j);
				dump_keybindings (out, joysticks[i].axes[j].pos, namebuffer);
			}
			for (j = 0; j < joysticks[i].numbuttons; j++)
			{
				keybinding *kb = joysticks[i].buttons[j];
				if (kb != NULL)
				{
					sprintf (namebuffer, "joystick %d button %d", i, j);
					dump_keybindings (out, kb, namebuffer);
				}
			}
			for (j = 0; j < joysticks[i].numhats; j++)
			{
				sprintf (namebuffer, "joystick %d hat %d left", i, j);
				dump_keybindings (out, joysticks[i].hats[j].left, namebuffer);
				sprintf (namebuffer, "joystick %d hat %d right", i, j);
				dump_keybindings (out, joysticks[i].hats[j].right, namebuffer);
				sprintf (namebuffer, "joystick %d hat %d up", i, j);
				dump_keybindings (out, joysticks[i].hats[j].up, namebuffer);
				sprintf (namebuffer, "joystick %d hat %d down", i, j);
				dump_keybindings (out, joysticks[i].hats[j].down, namebuffer);
			}
		}
	}

#ifdef VCONTROL_DEBUG
	/* Print out allocation data */
	{
		keypool bp = pool;
		i = 0;
		while (bp != NULL)
		{
			fprintf (out, "# Internal Debug: Chunk #%i: %d slots remaining.\n", i, bp->remaining);
			i++;
			bp = bp->next;
		}
		fprintf (out, "# Internal Debug: %d chunks allocated.\n", i);
	}
#endif
}

/* Configuration file grammar is as follows:  One command per line, 
 * hashes introduce comments that persist to end of line.  Blank lines
 * are ignored.
 *
 * Terminals are represented here as quoted strings, e.g. "foo" for 
 * the literal string foo.  These are matched case-insensitively.
 * Special terminals are:
 *
 * KEYNAME:  This names a key, as defined in keynames.c.
 * IDNAME:   This is an arbitrary string of alphanumerics, 
 *           case-insensitive, and ending with a colon.  This
 *           names an application-specific control value.
 * NUM:      This is an unsigned integer.
 * EOF:      End of file
 *
 * Nonterminals (the grammar itself) have the following productions:
 * 
 * configline <- IDNAME binding
 *             | "joystick" NUM "threshold" NUM
 *
 * binding    <- "key" KEYNAME
 *             | "joystick" NUM joybinding
 *
 * joybinding <- "axis" NUM polarity
 *             | "button" NUM
 *             | "hat" NUM direction
 *
 * polarity   <- "positive" | "negative"
 *
 * dir        <- "up" | "down" | "left" | "right"
 *
 * This grammar is amenable to simple recursive descent parsing;
 * in fact, it's fully LL(1). */

/* Actual maximum line and token sizes are two less than this, since
 * we need space for the \n\0 at the end */
#define LINE_SIZE 256
#define TOKEN_SIZE 64

typedef struct vcontrol_parse_state_s {
	char line[LINE_SIZE];
	char token[TOKEN_SIZE];
	int index;
	int error;
	int linenum;
} parse_state;

static void
next_token (parse_state *state)
{
	int index, base;

	state->token[0] = 0;
	/* skip preceding whitespace */
	base = state->index;
	while (state->line[base] && isspace (state->line[base]))
	{
		base++;
	}

	index = 0;
	while (index < (TOKEN_SIZE-1) && state->line[base+index] && !isspace (state->line[base+index]))
	{
		state->token[index] = state->line[base+index];
		index++;
	}
	state->token[index] = 0;

	/* If the token was too long, skip ahead until we get to whitespace */
	while (state->line[base+index] && !isspace (state->line[base+index]))
	{
		index++;
	}

	state->index = base+index;
}

static void
next_line (parse_state *state, FILE *in)
{
	int i, ch;
	int comment = 0;
	state->linenum++;
	for (i = 0; i < LINE_SIZE-1; i++)
	{
		if (feof (in))
		{
			break;
		}
		ch = fgetc (in);
		if (ch == '#' || ch == '\n' || ch == -1)
		{
			/* If this line is blank or all commented, include some
			 * whitespace.  This lets us detect EOF as a completely
			 * blank line. */
			if (i==0) 
			{
				state->line[i] = '\n';
				i++;
			} 
			break;
		}
		state->line[i] = ch;
	}
	state->line[i] = '\0';
	/* Skip to end of line */
	while (ch != '\n' && !feof (in))
	{
		ch = fgetc (in);
	}
	state->token[0] = 0;
	state->index = 0;
	state->error = 0;
}

static void
expected_error (parse_state *state, char *expected)
{
	fprintf (stderr, "VControl: Expected '%s' on config file line %d\n", expected, state->linenum);
	state->error = 1;
}

static void
consume (parse_state *state, char *expected)
{
	if (strcasecmp (expected, state->token))
	{
		expected_error (state, expected);
	}
	next_token (state);
}

static int
consume_keyname (parse_state *state)
{
	int keysym = VControl_name2code (state->token);
	if (!keysym)
	{
		fprintf (stderr, "VControl: Illegal key name '%s' on config file line %d\n", state->token, state->linenum);
		state->error = 1;
	}
	next_token (state);
	return keysym;
}

static int *
consume_idname (parse_state *state)
{
	int *result = NULL;
	int index = 0;
	while (state->token[index]) 
	{
		index++;
	}

	if (index == 0)
	{
		fprintf (stderr, "VControl: Can't happen: blank token to consume_idname (line %d)\n", state->linenum);
		state->error = 1;
		return NULL;
	}

	index--;
	if (state->token[index] != ':')
	{
		expected_error (state, ":");
		return NULL;
	}

	state->token[index] = 0;  /* remove trailing colon */

	result = name2target (state->token);
	next_token (state);

	if (!result)
	{
		fprintf (stderr, "VControl: Illegal command type '%s' on config file line %d\n", state->token, state->linenum);
		state->error = 1;
	}
	return result;
}

static int
consume_num (parse_state *state)
{
	char *end;
	int result = strtol (state->token, &end, 10);
	if (*end != '\0')
	{
		fprintf (stderr, "VControl: Expected integer on config line %d\n", state->linenum);
		state->error = 1;
	}
	next_token (state);
	return result;
}

static int
consume_polarity (parse_state *state)
{
	int result = 0;
	if (!strcasecmp (state->token, "positive"))
	{
		result = 1;
	}
	else if (!strcasecmp (state->token, "negative"))
	{
		result = -1;
	}
	else
	{
		expected_error (state, "positive' or 'negative");
	}
	next_token (state);
	return result;
}

static Uint8
consume_dir (parse_state *state)
{
	Uint8 result = 0;
	if (!strcasecmp (state->token, "left"))
	{
		result = SDL_HAT_LEFT;
	}
	else if (!strcasecmp (state->token, "right"))
	{
		result = SDL_HAT_RIGHT;
	}
	else if (!strcasecmp (state->token, "up"))
	{
		result = SDL_HAT_UP;
	}
	else if (!strcasecmp (state->token, "down"))
	{
		result = SDL_HAT_DOWN;
	}
	else
	{
		expected_error (state, "left', 'right', 'up' or 'down");
	}
	next_token (state);
	return result;
}

static void
parse_joybinding (parse_state *state, int *target)
{
	int sticknum;
	consume (state, "joystick");
	sticknum = consume_num (state);
	if (!state->error)
	{
		if (!strcasecmp (state->token, "axis"))
		{
			int axisnum;
			consume (state, "axis");
			axisnum = consume_num (state);
			if (!state->error)
			{
				int polarity = consume_polarity (state);
				if (!state->error)
				{
					if (VControl_AddJoyAxisBinding (sticknum, axisnum, polarity, target))
					{
						state->error = 1;
					}
				}
			}
		} 
		else if (!strcasecmp (state->token, "button"))
		{
			int buttonnum;
			consume (state, "button");
			buttonnum = consume_num (state);
			if (!state->error)
			{
				if (VControl_AddJoyButtonBinding (sticknum, buttonnum, target))
				{
					state->error = 1;
				}
			}
		}
		else if (!strcasecmp (state->token, "hat"))
		{
			int hatnum;
			consume (state, "hat");
			hatnum = consume_num (state);
			if (!state->error)
			{
				Uint8 dir = consume_dir (state);
				if (!state->error)
				{
					if (VControl_AddJoyHatBinding (sticknum, hatnum, dir, target))
					{
						state->error = 1;
					}
				}
			}
		}
		else
		{
			expected_error (state, "axis', 'button', or 'hat");
		}
	}
}

static void
parse_binding (parse_state *state)
{
	int *target = consume_idname (state);
	if (!state->error)
	{
		if (!strcasecmp (state->token, "key"))
		{
			/* Parse key binding */
			int keysym;
			consume (state, "key");
			keysym = consume_keyname (state);
			if (!state->error)
			{
				if (VControl_AddKeyBinding (keysym, target))
				{
					state->error = 1;
				}
			}
		}
		else if (!strcasecmp (state->token, "joystick"))
		{
			parse_joybinding (state, target);
		}
		else
		{
			expected_error (state, "key' or 'joystick");
		}
	}
}

static void
parse_config_line (parse_state *state)
{
	state->error = 0;
	next_token (state);
	if (!state->token[0])
	{
		/* Blank line, skip it */
		return;
	}
	if (!strcasecmp (state->token, "joystick"))
	{
		int sticknum, threshold = 0;
		consume (state, "joystick");
		sticknum = consume_num (state);
		if (!state->error) consume (state, "threshold");
		if (!state->error) threshold = consume_num (state);
		if (!state->error)
		{
			if (VControl_SetJoyThreshold (sticknum, threshold))
			{
				state->error = 1;
			}
		}
		return;
	}
	/* Otherwise, it must be a binding */
	parse_binding (state);
}

int
VControl_ReadConfiguration (FILE *in)
{
	parse_state ps;
	int errors;
	if (!in)
	{
		fprintf (stderr, "VControl: Invalid configuration file stream\n");
		return 1;
	}
	ps.linenum = 0;
	errors = 0;
	while (1)
	{
		next_line (&ps, in);
		if (!ps.line[0])
			break;
		parse_config_line (&ps);
		if (ps.error)
		{
			errors++;
		}
	}
	return errors;
}

#if 0
/* This was kinda handy for proving (lack of) buffer overrun
 * vulnerabilities, but there's no real need for it otherwise. */
void
VControl_TokenizeFile (FILE *in)
{
	parse_state ps;
	if (!in)
	{
		fprintf (stderr, "VControl: Invalid configuration file stream\n");
		return;
	}
	ps.linenum = 0;
	while (1)
	{
		next_line (&ps, in);
		if (!ps.line[0])
			break;
		printf ("%3d:", ps.linenum);
		while (1)
		{
			next_token (&ps);
			if (!ps.token[0])
				break;
			printf (" \"%s\"", ps.token);
		}
		printf ("\n");
	}
}
#endif
