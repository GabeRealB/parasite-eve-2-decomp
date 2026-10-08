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
/// state-machine task to 4. Phase 0 permits ordinary departure to fire-escape area 15; phase 2
/// makes the warp start cap slot 9 and lets
/// `acropolisHelicopterLandingPadHandleCommand` spawn task entry 4.
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
/// Raises the player on the lift and prepares the helicopter departure scene.
///
/// Starts in state 0 with the live player, scene task and lift child 40 available.
/// Selects view 18 and Q12 ambient RGB (1200,1200,1552), takes scripted control,
/// starts the lift's raise request and reparents the player to its root coordinate.
/// The lift must remain live while the player borrows that coordinate. A variant
/// below 2 loads resource 511000, replaces placements with variant 2 and sets
/// departure phase 4; later variants reuse their resources and retain the phase.
/// Uses `spawnArg1.value` as a signed 120-update countdown; loading overlaps the
/// countdown and sound entry 3 starts at 90 remaining. After both loading and the
/// delay, starts the normal/skip departure scripts and ends. Runs once per departure.
void acropolisHelicopterLandingPadPrepareDepartureTask(Task* task);

/// Times four screen shakes and their controller vibration during departure.
///
/// `state` counts updates from zero. Shakes at 0,200,410,520 have half-durations
/// 15,15,22,30 updates and amplitudes 2,2,3,4 pixels; vibration follows at
/// 7,207,421,535. Ends at 640 and still increments the retired task's state.
/// Requires the room's task descriptors and vibration programs to remain loaded.
void acropolisHelicopterLandingPadDepartureShakeTimelineTask(Task* task);

/// Restores health, starts the departure movie and reloads the M.I.S.T. arrival.
///
/// Starts in state 0. Clears collection bits for the Parthenon Key and Micro
/// Device, identifies the device and selects follow-up dialogue 7. Spawns the
/// movie handoff task, then takes one intervening update before selecting
/// Acropolis area 18, room 1, warp 1 with scene event 1 and sprite variant 1.
/// Movie playback owns the display and suspends ordinary task updates. Stops
/// non-ambient sounds, requests a captured-frame reload, releases the menu hold
/// and ends. Requires the live save, display and room movie resources.
void acropolisHelicopterLandingPadReturnToMistTask(Task* task);

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

/// Resolves ordinary fire-escape departure versus the post-encounter CAP exit.
///
/// Borrows the request and copies its full eight bytes to the writable reply;
/// they may be the same object. Returns 1 for destinations other than fire-escape
/// area 15, and for that area in phase 0, permitting ordinary departure.
/// Phase 2 returns 0 and execution starts CAP slot 9 with variant 0; all other
/// phases return 0 without starting CAP. Queries omit side effects. Phase-zero
/// execution also queues sound-stop selector -1 with control 30: the selector
/// is not a stop-all request and its intended purpose is unproven.
/// The receiver and message ID are ignored; the room and CAP resources must be live.
s32 acropolisHelicopterLandingPadResolveDeparture(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);

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

/// Starts departure confirmation for room command 4 after the encounter ends.
///
/// Phase 2 and command 4 spawn the confirmation task with zero payloads; all
/// other combinations do nothing. Returns zero, including allocation failure.
/// Requires loaded room descriptors; the receiver, ID and second payload are ignored.
s32 acropolisHelicopterLandingPadHandleCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);

/// Starts the departure screen-shake/vibration timeline at frame zero.
void acropolisHelicopterLandingPadStartDepartureShakeTimeline(void);

/// Starts the player's scripted approach to the departure scene mark.
///
/// Requires the live player and loaded room descriptors through the movement task.
void acropolisHelicopterLandingPadStartPlayerMoveToSceneMark(void);

/// Starts the movie and M.I.S.T. return task after normal or skipped departure.
///
/// Requires the live save, display and loaded room descriptors; allocation failure
/// is ignored. The spawned task releases the script's menu hold during reload.
void acropolisHelicopterLandingPadStartReturnToMist(void);

/// Applies the post-encounter player placement when room action 1 was latched.
///
/// Requires a live player when latched. Places it at (-6801, 0, -1998), yaw
/// 1024 in its current parent frame. Leaves the latch set for later callbacks.
void acropolisHelicopterLandingPadPlacePlayerAfterEncounter(void);

/// Starts the singleton scripted player turn toward `targetYaw` (0..4095).
///
/// Angles use 4096 units per turn. Requires a live player and loaded room
/// descriptors, and no other turn task using the room's shared yaw state.
void acropolisHelicopterLandingPadStartPlayerYawTurn(s32 targetYaw);

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

/// Asks for departure confirmation and starts the lift/scene preparation task.
///
/// State 0 holds the player and starts CAP slot 4. State 1 checks its retained
/// choice only if CAP is idle: index 1 resumes control and cancels; other indices
/// acquire the menu hold. State 1 advances even while CAP is busy, and advances
/// twice on acceptance. States 2 and 3 delay; state 4 sets phase 3, starts
/// departure preparation and ends. Requires live player, CAP and room resources;
/// does not continuously poll CAP completion and does not check spawn failure.
void acropolisHelicopterLandingPadConfirmDepartureTask(Task* task);

#endif // SRC_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_ACROPOLIS_HELICOPTER_LANDING_PAD_PRIVATE_H
