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

#include "gameplay/message.h"

#include "main/task_types.h"

#include "overlay.h"

/// Prefix of the work block of the task that spawns the blaze.
///
/// The fade is started with that task in `Task::spawnArg2` and reads the
/// block through this prefix. It sets `wave` to `SCREEN_WAVE_RAMP_FINISHED`
/// once the screen has washed white. Each package extends the block past the
/// prefix with its own tasks and scene state. Nothing reads the bytes before
/// `wave`.
typedef struct {
    byte          unknown_0[0x20]; // Never accessed; role unproven
    ScreenWaveCtx wave;            // Heat-haze ramp the spawner seeds and the fade finishes
} BlazeParentWork;
STATIC_ASSERT_SIZEOF(BlazeParentWork, 0x2C);

/// Sets the fade task's state from the first integer payload; the second is ignored.
///
/// Senders discard the unspecified result. This receiver uses the actor-command
/// message ID with an integer state instead of an `ActorCommand` address.
enum {
    BLAZE_FADE_MESSAGE_SET_STATE = ACTOR_COMMAND_MESSAGE_APPLY,
};

/// Per-instance task-message table controlling the incinerator fade's phases.
///
/// Each carrier supplies one record and its callback, borrowed for the fade
/// task's lifetime. There is no end marker: send only `BLAZE_FADE_MESSAGE_SET_STATE`.
/// The first payload is stored as the full signed state word. Scene scripts
/// select 2 (fast red ramp), 3 (slow red ramp), then 4 (wash to white).
/// The second payload is ignored and the callback has no defined result.
extern TaskMessageEntry gBlazeFadeMessages[1];

static void _blazeFadeTask(Task* task);
void        blazeBodyFireTask(Task* arg0);

#endif /* SRC_SHARED_INCINERATOR_BLAZE_H */
