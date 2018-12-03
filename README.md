# VControl
A simple digital input abstraction for SDL

Graphical applications on modern machines provide an _event loop_ to model the actions of the user.  While this is the most direct way of handling general GUI events, it is inconvenient to program many game applications with this model.  VControl provides a much simpler interface for the game developer that provides a great deal of keyboard and joystick configurability for the end user.

To use the Virtual Controller, the developer names a number of "buttons."  For example, a simple Space-Invaders style game would have buttons named "left," "right," and "fire."  The library reads a configuration file that maps keyboard or joystick actions to these names.  This configuration can be very general - multiple game actions may be mapped to a single user action, and any game action may have any number of user actions that signify that game action.  As far as the game logic is concerned, "left," "right," and "fire" are the only reality; the VControl routines filter the event stream to update the values appropriately.

VControl is built on top of the [Simple Direct-Media Layer](http://www.libsdl.org/), and is intended to provide configurable digital input for programs that use SDL as their primary user-interface library.  It is distributed under the zlib license.

VControl was first published in 2003 and only worked with SDL 1.2. The original host no longer exists, so this github incarnation of it intends to bring it back online and then modernize the code so that it can interoperate seamlessly with SDL 1.2 or 2.0. All previously published versions are available as tags here and each has been made available under the zlib license.

## Why Use VControl?

VControl offers a number of advantages to the game programmer.

- **Simplified API:** VControl implements a version of the common "listener" interface tuned for C.  This provides a very flexible, application-specific set of interface controls; almost nothing is actually hardcoded.
- **Handles complex key configurations:** If two keys map to the same virtual action, VControl transparently merges overlapped keypresses to the same action.
- **Multithreading capable** Although the VControl code does not use locks, it may still be safely used in a multithreaded application---only the event loop's thread performs any writes to shared memory, and as long as those values are properly declared volatile, all code remains consistent.  Should a coarser level of atomicity be desired, it is easy to wrap VControl with synchronization.

## Why NOT Use VControl?

VControl is a nice library.  However, there are tasks that it is bad at, or which it has no intention of being able to deal with.

- **Text entry:** VControl is trying to model a gamepad, not an actual keyboard.  It would be possible but extremely unpleasant to use it to mediate text entry.  VControl does no violence to the event stream, so other libraries can handle this.
- **Keychords:** VControl assumes one key per action.  It ignores shift/meta keys (treating the pressing of the shift keys the same as any other keypress) and cannot express that a game action should only be signalled when the user makes two user actions at the same time.  Such functionality could be added as a wrapper layer, and indeed may appear in the future.  However, the problem that VControl is attempting to solve is the presence of _too many_ buttons, not too few.
- **Analog input:** Analog joysticks are deliberately and explicitly abstracted away by the VControl layer.  While it does no violence to the event stream, it abstracts away the finer details of the joystick axes.  It cannot handle mouse or trackball events at all.
