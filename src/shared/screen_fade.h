/* Full-screen fade tasks built on fadeDrawOverlay in subtractive mode (2).
 * One ramps a grey from white down to nothing, bringing the picture up out of
 * black; the other ramps it up from nothing, taking the picture down to black.
 * Each task allocates an 8-byte ScreenFadeWork, steps by its spawnArg1 each
 * frame, and kills itself at the end.
 * screenFadeInTileTask is the fade-in with its overlay tile linked in place
 * rather than drawn through fadeDrawOverlay.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
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
void screenFadeOutTask(Task* arg0);
void screenFadeInTileTask(Task* arg0);

#endif /* SRC_SHARED_SCREEN_FADE_H */
