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

#ifndef EFFECTS_INCINERATOR_BLAZE_INCINERATOR_BLAZE_H
#define EFFECTS_INCINERATOR_BLAZE_INCINERATOR_BLAZE_H

#include <core/pe2.h>

#include <gameplay/message.h>

#include <engine/display.h>
#include <engine/task_types.h>

#include <screen/wave/wave.h>

/// The start of the spawning task's work block, which the fade reaches through its `spawnArg2`; each package's
/// block goes on past it.
typedef struct {
    byte          unknown_0[0x20]; // Unused here
    ScreenWaveCtx wave;            // Heat-haze ramp the spawner seeds and the fade finishes
} BlazeParentWork;

/// Sets the fade task's state to the message's first payload, an integer (the actor-command message id, reused).
enum {
    BLAZE_FADE_MESSAGE_SET_STATE = ACTOR_COMMAND_MESSAGE_APPLY,
};

/* Each package defines its private _gBlazeFadeMessages[1], handling
 * BLAZE_FADE_MESSAGE_SET_STATE, before including the fade fragment. */

static void _blazeFadeTask(Task* task);
static void _blazeBodyFireTask(Task* task);

#endif /* EFFECTS_INCINERATOR_BLAZE_INCINERATOR_BLAZE_H */
