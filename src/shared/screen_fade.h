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

void screenFadeInTask(Task* arg0);
void screenFadeOutTask(Task* arg0);
void screenFadeInTileTask(Task* arg0);

#endif /* SRC_SHARED_SCREEN_FADE_H */
