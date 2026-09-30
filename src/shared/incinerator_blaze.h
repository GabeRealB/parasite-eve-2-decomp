/* The incinerator firing up around the player, as the controller package
 * actor_342100 and the incinerator room both stage it. A fade task tints the
 * screen red in two ramps (fast to 0x50, then slowly to 0xFF), then washes
 * green and blue up to white. It stops the parent's heat-haze screen wave,
 * fills the frame buffer white and holds a full-screen white tile. A companion
 * task keeps spawning effect 3 (fire) on random parts of the player's model.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_INCINERATOR_BLAZE_H
#define SRC_SHARED_INCINERATOR_BLAZE_H

#include "types.h"

#include "main/task_types.h"

#include "overlay.h"

/// The head of the work block of the task that spawns the blaze, which each
/// package extends: the screen-wave context the fade sets to state 2 when its
/// colour ramp reaches 0x100.
typedef struct BlazeParentWork {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ OverlayWaveCtx wave;
} BlazeParentWork;

void blazeFadeTask(Task* arg0);
void blazeBodyFireTask(Task* arg0);

#endif /* SRC_SHARED_INCINERATOR_BLAZE_H */
