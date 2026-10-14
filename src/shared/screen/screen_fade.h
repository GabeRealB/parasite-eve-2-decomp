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

#include "main/task_types.h"

#ifndef SCREEN_FADE_IN_TASK
/// Selects the callback identifier declared here and defined by the fade-in fragment.
///
/// The value must be a bare function identifier with signature `void (Task* task)`.
/// The default `screenFadeInTask` is shared between gas-station translation units.
/// For a TU-local instance, declare a static prototype and bind its identifier
/// before including this header. A further fragment copy can rebind after an
/// `#undef`; give it its own prototype and task-table entry. The header guard
/// prevents a second include from declaring a rebound identifier. Linkage comes
/// from the preceding prototype, not the binding. This object-like alias takes
/// no arguments, captures no values and constructs no tokens. Before including
/// `screen_fade_in.inc.c`, include `overlay.h`, `main/display_types.h`,
/// `main/gameflow.h`, `main/mem.h` and `main/task.h` for its work type and APIs.
#define SCREEN_FADE_IN_TASK screenFadeInTask
#endif

/// Reveals the screen by reducing a subtractive full-screen overlay each update.
///
/// Start with state 0 and no owned work. Initialization allocates primary-heap
/// `ScreenFadeWork`, seeds all channels to 255 and enters state 1, drawing and
/// stepping in that same update. Allocation failure kills the task; teardown
/// releases its work. State 1 requires that live work; other states do nothing.
/// The unsigned low 16 bits of `spawnArg1` are intensity units removed per
/// update (0 holds indefinitely); the upper half and `spawnArg2` are ignored.
/// Drawing uses the low red/green/red bytes before each step. The channels
/// narrow to signed 16 bits without clamping, and negative stored red ends the
/// task. Large rates can wrap instead of completing on their first step.
///
/// Requires the frame packet arena and foreground ordering-table tag used by
/// `fadeDrawOverlay`. The carrier's code must remain loaded until task teardown.
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
