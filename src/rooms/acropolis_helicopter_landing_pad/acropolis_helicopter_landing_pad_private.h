#ifndef SRC_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_ACROPOLIS_HELICOPTER_LANDING_PAD_PRIVATE_H
#define SRC_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_ACROPOLIS_HELICOPTER_LANDING_PAD_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"

/// Two descriptors that attach no model: entry 0 runs
/// `_acropolisHelicopterLandingPadStartMovieTask`, entry 1
/// `_acropolisHelicopterLandingPadMovieTask`.
extern TaskDesc D_acropolis_helicopter_landing_pad_80184E68[];

/// Progress of the helicopter sequence. The msg 0x3EF handler moves it from 0
/// to 1, the phase tick from 1 to 2, the cap-slot task to 3 and the room
/// state-machine task to 4. Phase 0 refuses the warp into stage 0xF; phase 2
/// makes the warp start cap slot 9 and lets
/// `func_acropolis_helicopter_landing_pad_8017E570` spawn task entry 4.
extern s32 D_acropolis_helicopter_landing_pad_80184D9C;

/// Raised by the phase tick once the session reaches camera view 5 and
/// cleared when the script task starts; the msg 0x3EF handler only starts
/// phase 1 while it is set.
extern s32 D_acropolis_helicopter_landing_pad_80184E0C;

/// Latched by kind 1 of msg 0x3EF and cleared when the script task starts or
/// `gGameSession->eventState` is 0; while set, a handler forwards msg 0x3E9
/// to slot 3.
extern s32 D_acropolis_helicopter_landing_pad_80187F84;

extern WorldCollisionTrigger D_acropolis_helicopter_landing_pad_80185E7C[9];

extern WorldCollisionOccluder D_acropolis_helicopter_landing_pad_80186128[2];

extern WorldCoordRoomLights D_acropolis_helicopter_landing_pad_80186AE8[1];

extern SVECTOR ActorContact_ScratchPosition;

extern RoomEventMsg D_acropolis_helicopter_landing_pad_80187F90;

extern TaskMessageEntry D_acropolis_helicopter_landing_pad_80183710[5];

extern ActorTransform D_acropolis_helicopter_landing_pad_801837B0;

extern s32 D_acropolis_helicopter_landing_pad_801837E0[18];

extern EvsCommand D_acropolis_helicopter_landing_pad_80183A04[2];

extern EvsCommand D_acropolis_helicopter_landing_pad_80183A34[58];

extern EvsCommand D_acropolis_helicopter_landing_pad_80183FA4[16];

extern EvsCommand D_acropolis_helicopter_landing_pad_80184124[38];

extern EvsCommand D_acropolis_helicopter_landing_pad_801844B4[19];

extern EvsCommand D_acropolis_helicopter_landing_pad_8018467C[69];

extern EvsCommand D_acropolis_helicopter_landing_pad_80184CF4[7];

extern TaskDesc D_acropolis_helicopter_landing_pad_80184DA0[9];

extern AnimationPlayRequest D_acropolis_helicopter_landing_pad_80184E28;

extern AnimationPlayRequest D_acropolis_helicopter_landing_pad_80184E3C;

extern ActorTransform D_acropolis_helicopter_landing_pad_80184E50;

/// Advances encounter completion once the actor and presentation holds have cleared.
///
/// Phase 1 polls placed actor 0; absence, a closed attachment wheel and no pending
/// display mode start the ending/skip scripts, select objective 8 and set phase 2.
/// View 5 latches the encounter-start gate. An inactive session event clears the
/// post-encounter placement latch. Requires the live session and room resources;
/// `unusedTask` is ignored. This is the room task's per-frame update callback.
void acropolisHelicopterLandingPadUpdateEncounterPhase(Task* unusedTask);

// Callbacks referenced by the overlay's shared data tables.
void func_acropolis_helicopter_landing_pad_8017DA9C(Task*);

void func_acropolis_helicopter_landing_pad_8017DE78(Task*);

void func_acropolis_helicopter_landing_pad_8017DFCC(Task*);

/// Turns the live player's yaw toward `spawnArg1.value` by 256 angle units per update.
///
/// The target is a normalized yaw in 0..4095 (4096 units per turn). State 0
/// chooses the shorter unwrapped path; state 1 steps and ends only after passing
/// the target, so an exact arrival takes one more update. Requires live player
/// `GameActor` work. Uses room-global turn state: run only one instance at a time.
void acropolisHelicopterLandingPadTurnPlayerYawTask(Task* task);

/// States of the player's model-part pitch pulse; stored in the task's state word.
enum {
    ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_IDLE = 0,
    ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_RISE = 1,
    ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_FALL = 2,
};

/// Composes a rising then falling negative pitch into player model part 4.
///
/// Requires a live player TMD with at least five coordinates. The weight rises
/// by 400 per update to 4096, then falls to zero; each update composes up to
/// -96 angle units about X (4096 units per turn). Idle resets the weight but
/// does not undo the matrix. The animation pose supplies the matrix to modify.
/// Uses a room-global weight: run only one instance, retained for script cues.
void acropolisHelicopterLandingPadPlayerPitchPulseTask(Task* task);

s32 func_acropolis_helicopter_landing_pad_8017E3F0(Task*, s32, RoomEventMsg*, RoomEventMsg*);

/// Refuses every `ROOM_MESSAGE_USE_KEY_ITEM` request without changing the room.
s32 acropolisHelicopterLandingPadRefuseKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);

/// Handles the room's automatic encounter and post-encounter placement triggers.
///
/// Borrows a `DIRECTION_MESSAGE_ROOM_ACTION` request synchronously. Action 0
/// starts the encounter and skip scripts once view 5 has armed the room and
/// the encounter has not started, enabling trigger 4 and disabling trigger 8.
/// Action 1 latches the later player placement; other actions do nothing.
/// The request's argument byte and second payload are unused. Returns zero.
s32 acropolisHelicopterLandingPadHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

s32 func_acropolis_helicopter_landing_pad_8017E570(Task*, s32, s32, s32);

void func_acropolis_helicopter_landing_pad_8017E5B8(void);

void func_acropolis_helicopter_landing_pad_8017E5E8(void);

void func_acropolis_helicopter_landing_pad_8017E64C(void);

/// Applies the post-encounter player placement when room action 1 was latched.
///
/// Requires a live player when latched. Places it at (-6801, 0, -1998), yaw
/// 1024 in its current parent frame. Leaves the latch set for later callbacks.
void acropolisHelicopterLandingPadPlacePlayerAfterEncounter(void);

void func_acropolis_helicopter_landing_pad_8017E6C0(s32);

/// Releases the encounter's battle hold, credits its enemy rewards and arms a three-frame end delay.
///
/// Placed actor 0 must remain live with enemy bookkeeping while a hold exists.
/// Does not destroy the actor. Used by both normal and skipped encounter endings.
void acropolisHelicopterLandingPadReleaseEncounterBattle(void);

/// Requests cancellation of room effects and locks attachment actions for the encounter ending.
void acropolisHelicopterLandingPadLockAttachmentsForEncounter(void);

/// Sets the persistent player pitch task's state for a script cue.
///
/// `pulseState` is one of this header's pitch states (0..2), stored as s32.
/// Requires the room initialization's pitch task to remain live.
void acropolisHelicopterLandingPadSetPlayerPitchPulseState(s32 pulseState);

/// Moves the player to the scene mark and waits for scripted motion to finish.
///
/// State 0 sends position (-3496, -2871, -1283) in the player's parent frame
/// with approach clip 12 and arrival clip 9 in its current animation bank.
/// State 1 polls the motion latch and ends when it clears. Requires a live
/// player and the room's destination storage through synchronous dispatch;
/// the player retains destination and clip values, not the local clip record.
void acropolisHelicopterLandingPadMovePlayerToSceneMarkTask(Task* task);

/// Runs a vertical screen shake with a triangular amplitude envelope.
///
/// `spawnArg2.value` packs a half-duration of 1..255 updates in bits 0..7;
/// arithmetic shift by 8 gives the signed pixel amplitude. State 0 initializes
/// `spawnArg1.value` as the cursor; state 1 samples from minus to plus the
/// half-duration inclusive, then clears the shake and ends on the next update.
/// Samples advance the shared random sequence and alternate sign. The display
/// narrows each to s8 before clamping to [-8, 8]. Zero duration is invalid;
/// no division guard is present. Both signed amplitude products must fit s32;
/// authored requests use half-durations 15, 22, 30 and amplitudes 2, 3, 4.
/// The room overlay must remain loaded.
void acropolisHelicopterLandingPadScreenShakeTask(Task* task);

void func_acropolis_helicopter_landing_pad_8017E974(Task*);

#endif // SRC_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_ACROPOLIS_HELICOPTER_LANDING_PAD_PRIVATE_H
