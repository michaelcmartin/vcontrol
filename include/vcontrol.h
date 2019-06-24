/* 
 * VControl Library, Copyright (c) 2003, Michael Martin
 *
 * VControl is distributed under the terms of the zlib license, and as
 * such has NO WARRANTY.  See the LICENSE file for details.
 */

#include <SDL.h>

#ifndef VCONTROL_H_
#define VCONTROL_H_

#if SDL_MAJOR_VERSION == 1
typedef SDLKey sdl_key_t;
#else
typedef SDL_Keycode sdl_key_t;
#endif
#ifdef __cplusplus
extern "C" {
#endif

/* Initialization routines */
void VControl_Init (void);
void VControl_Uninit (void);

/* Control of bindings */
int  VControl_AddBinding (SDL_Event *e, int *target);
void VControl_RemoveBinding (SDL_Event *e, int *target);

/* For more specific control */				
int  VControl_AddKeyBinding (sdl_key_t symbol, int *target);
void VControl_RemoveKeyBinding (sdl_key_t symbol, int *target);
int  VControl_AddJoyAxisBinding (int port, int axis, int polarity, int *target);
void VControl_RemoveJoyAxisBinding (int port, int axis, int polarity, int *target);
int  VControl_SetJoyThreshold (int port, int threshold);
int  VControl_AddJoyButtonBinding (int port, int button, int *target);
void VControl_RemoveJoyButtonBinding (int port, int button, int *target);
int  VControl_AddJoyHatBinding (int port, int which, Uint8 dir, int *target);
void VControl_RemoveJoyHatBinding (int port, int which, Uint8 dir, int *target);

void VControl_RemoveAllBindings (void);

/* The listener.  Routines besides HandleEvent may be used to 'fake' inputs without 
 * fabricating an SDL_Event. 
 */
void VControl_HandleEvent (SDL_Event *e);
void VControl_ProcessKeyDown (sdl_key_t symbol);
void VControl_ProcessKeyUp (sdl_key_t symbol);
void VControl_ProcessJoyButtonDown (int port, int button);
void VControl_ProcessJoyButtonUp (int port, int button);
void VControl_ProcessJoyAxis (int port, int axis, int value);
void VControl_ProcessJoyHat (int port, int which, Uint8 value);

/* Force the input into the blank state.  For preventing "sticky" keys. */
void VControl_ResetInput (void);

/* Name control.  To provide a table of names and bindings, declare
 * a persistent, unchanging array of VControl_NameBinding and end it
 * with a {0, 0} entry.  Pass this array to VControl_RegisterNameTable.
 * Only one name table may be registered at a time; subsequent calls
 * replace the previous values. */

typedef struct _vcontrol_namebinding {
	char *name;
	int *target;
} VControl_NameBinding;

void VControl_RegisterNameTable (VControl_NameBinding *table);

/* Dump a configuration file corresponding to the current bindings and names. */
void VControl_Dump (FILE *out);
/* Read a configuration file.  Returns number of errors encountered. */
int VControl_ReadConfiguration (FILE *in);

#ifdef __cplusplus
}
#endif

#endif
