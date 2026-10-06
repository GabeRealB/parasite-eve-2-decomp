/* Included subtractive full-screen fade tasks, owning their channel ramps.
 * The ordinary tasks use fadeDrawOverlay; screenFadeInTileTask queues its own
 * tile and draw-mode packets at the current draw origin.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. Each carrier of the private fade-out fragment declares its static
 * callback in its own prologue.
 */

#ifndef SRC_SHARED_SCREEN_FADE_H
#define SRC_SHARED_SCREEN_FADE_H

#include "types.h"

#include "main/task_types.h"

#ifndef SCREEN_FADE_IN_TASK
/// Function identifier for the included fade-in task, defaulting to `screenFadeInTask`.
///
/// Bind to a function with signature `void (Task* task)` before this header
/// or around a further copy of `screen_fade_in.inc.c`. A preceding static
/// prototype gives a further copy internal linkage. Undefine the previous
/// binding before rebinding, and keep each task descriptor bound to its own
/// instance. This object-like alias evaluates no arguments and constructs no
/// tokens. Before the fragment, include `overlay.h`, `main/display_types.h`,
/// `main/gameflow.h`, `main/mem.h` and `main/task.h` for its work type and APIs.
#define SCREEN_FADE_IN_TASK screenFadeInTask
#endif

void SCREEN_FADE_IN_TASK(Task* task);
/// Reveals the screen by reducing a subtractive tile overlay each update.
///
/// Start at state 0 with no owned work. The unsigned low 16 bits of `spawnArg1`
/// are intensity units removed per update; zero holds the overlay indefinitely.
/// The task owns its primary-heap `ScreenFadeWork` until teardown; allocation
/// failure kills it. Initialization sets all channels to 255, then draws and
/// steps immediately. State 1 draws before stepping; other states do nothing.
/// Each subtraction narrows to signed 16 bits without clamping, and the task
/// ends when stored red is negative. Drawing uses the low red/green/red bytes.
///
/// Requires a word-aligned frame arena with `sizeof(TILE) + sizeof(DR_TPAGE)`
/// writable bytes and foreground tag -16 in the active ordering table. Packets
/// borrow the arena until GPU drawing completes. The centred 320x240 tile uses
/// the current draw origin; its mode enables dithering and disables drawing
/// into the displayed area, remaining active until another mode replaces it.
void screenFadeInTileTask(Task* task);

#endif /* SRC_SHARED_SCREEN_FADE_H */
