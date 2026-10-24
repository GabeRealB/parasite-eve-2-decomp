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

/// Two bodyless priority-192 tasks for the return-to-MIST movie.
///
/// Slot 0 transfers display control and spawns slot 1 for view-100 movie playback,
/// cancellation and resource restoration. This is an indexed table without a
/// terminator: only slots 0/1 may be spawned. The room overlay and callbacks
/// must remain loaded until display control returns to gameplay.
extern TaskDesc gAcropolisHelicopterLandingPadMovieTasks[2];

/// Encounter/departure phases consulted by the room's departure callbacks.
enum {
    ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_WAITING            = 0,
    ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_ENCOUNTER_FINISHED = 2,
    ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_ACCEPTED = 3,
    ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_READY    = 4,
};

/// Progress of the encounter and subsequent helicopter departure.
///
/// Stored as a signed word: 0 waiting, 1 encounter started, 2 encounter finished,
/// 3 departure accepted, 4 departure placements ready. Phase 0 permits the
/// ordinary fire-escape exit; phase 2 starts its departure dialogue. First-time
/// departure prepares variant 2 and advances to 4; revisits remain at 3.
/// The initial value belongs to the loaded overlay and is not reset by room setup.
extern s32 gAcropolisHelicopterLandingPadEncounterPhase;

/// Latched permission to start the encounter after entering logical view 5.
///
/// Room setup clears this signed word to 0; visiting view 5 sets it to 1.
/// It stays set after leaving that view and after the encounter starts.
/// Room action 0 requires this gate and encounter phase 0.
extern s32 gAcropolisHelicopterLandingPadEncounterReady;

/// Latched permission to apply the post-encounter player placement.
///
/// Room action 1 sets this signed word to 1. Room initialization and an idle
/// event state clear it to 0. Placement callbacks test it without clearing it,
/// so the normal and skipped script paths may apply the placement repeatedly.
extern s32 gAcropolisHelicopterLandingPadPostEncounterPlacementEnabled;

/// Nine mutable action regions for the landing pad's single room.
///
/// Includes the fire-escape callback, CAP interactions, a three-step stair climb
/// on surface class 3, and automatic room actions 0 (encounter) and 1 (placement).
/// The last region carries LAST and remains part of the array. Room binding links
/// and enables every region in view space. Setup disables placement region 4;
/// starting the encounter enables it and disables CAP region 8. Origins, corner
/// offsets and radii use room units; normals use 4096 per unit. Keep these records
/// writable and loaded while linked or queried.
extern WorldCollisionTrigger gAcropolisHelicopterLandingPadActionTriggers[9];

/// Two crossing room-space quads that occlude actor line-of-sight queries.
///
/// Room binding links and enables both records, stopping after the second one's
/// LAST flag. Corners are offsets from each origin in room units; normals use
/// 4096 per unit. The low flag bit is retained without an observed interpretation.
/// The loaded room owns the writable records and their runtime links.
extern WorldCollisionOccluder gAcropolisHelicopterLandingPadSightOccluders[2];

/// The landing pad's authored collection of one directional and 22 point lights.
///
/// The room lighting descriptor borrows this single collection independently
/// of camera view. Its arrays remain writable for coordinate composition and
/// attenuation queries; no cone lights are present. Keep the overlay loaded
/// while the descriptor or any light is in use.
extern WorldCoordRoomLights gAcropolisHelicopterLandingPadRoomLights;

extern SVECTOR ActorContact_ScratchPosition;

/// Persistent fire-escape transition request and resolved reply for the stair exit.
///
/// Resolved in place before the player's turn and descent; its area, warp and
/// room are committed afterward. Keep this singleton unchanged through the
/// exit sequence. The loaded room owns all eight bytes, initially zero.
extern RoomEventMsg gAcropolisHelicopterLandingPadExitRequest;

/// Four room-message handlers followed by the task-message end marker.
///
/// Resolves departure, refuses key items, handles direction actions and accepts
/// room commands. Each handler declares its message's integer or typed pointer
/// payloads; dispatch borrows them synchronously. Unsupported IDs return zero.
/// The room task borrows this table and its callbacks while the overlay is live.
extern TaskMessageEntry gAcropolisHelicopterLandingPadRoomMessages[5];

/// Player placement at the lift origin with quarter-turn yaw 1024.
///
/// Position is in the current parent frame and angles use 4096 units per turn.
/// Applied both before attachment and in the lift's frame after attachment.
/// Dispatch copies both vectors synchronously and retains no record pointer.
extern ActorTransform gAcropolisHelicopterLandingPadLiftPlayerPlacement;

/// Departure scene-mark XYZ followed by authored storage of unproven meaning.
///
/// The approach command copies only the three signed coordinates, in the
/// player's parent frame and integer game units. The following fifteen words
/// have no established consumer; their extent preserves the stored data
/// without asserting additional placements or rotations.
typedef struct {
    VECTOR3 destination;    // Scripted approach destination in parent-coordinate game units
    s32     unknown_0C[15]; // Uninterpreted stored words; role unproven
} AcropolisHelicopterLandingPadSceneMarkStorage;
STATIC_ASSERT_SIZEOF(AcropolisHelicopterLandingPadSceneMarkStorage, 0x48);

/// Destination (-3496, -2871, -1283) for the player's departure-scene approach.
///
/// Dispatch borrows `destination` synchronously and retains its XYZ values.
/// The trailing words are preserved but have no established use.
extern AcropolisHelicopterLandingPadSceneMarkStorage gAcropolisHelicopterLandingPadSceneMarkStorage;

/// Starts the landing pad's ambient sound and ends the setup event.
///
/// Two commands including END, with no scene selection or waits. Started with
/// HUD control retained; the room-owned array must stay live until execution ends.
extern EvsCommand gAcropolisHelicopterLandingPadAmbientSoundScript[2];

/// Presents the encounter introduction and starts No. 9's fight.
///
/// Fifty-eight commands including END. Stops ambient sound/music, selects the
/// intro stream and CAP slot 1, places and animates the actors, then restores
/// normal lighting and player drawing before enabling the fight. CAP cues and
/// frame waits suspend execution. The paired skip script establishes the fight
/// without replaying the presentation; all borrowed room/actor data must stay loaded.
extern EvsCommand gAcropolisHelicopterLandingPadEncounterIntroScript[58];

/// Skips the encounter introduction and establishes its playable fight state.
///
/// Sixteen commands including END. Fades through camera view 4, cleans up scene
/// control, places the player, restores lighting and drawing, and starts the
/// fight. The interpreter retains this room-owned array as the intro's skip target.
extern EvsCommand gAcropolisHelicopterLandingPadEncounterIntroSkipScript[16];

/// Presents the encounter ending and sends the lift down to its lower stop.
///
/// Thirty-eight commands including END. Selects CAP slot 2 and the ending
/// stream, restores the player placement, resumes music and releases battle
/// control. Uses the saved camera view. Room, actor and lift resources must
/// remain loaded through the CAP waits and command execution.
extern EvsCommand gAcropolisHelicopterLandingPadEncounterEndingScript[38];

/// Skips the encounter ending, parks the lift and restores normal play.
///
/// Nineteen commands including END. Fades through scene cleanup, restores the
/// player placement and saved view, resumes area music and releases battle
/// control. Retained as the ending event's skip target while the room is loaded.
extern EvsCommand gAcropolisHelicopterLandingPadEncounterEndingSkipScript[19];

/// Presents helicopter departure and starts the movie-backed return to MIST.
///
/// Sixty-nine commands including END. Installs two player clips, selects CAP
/// slot 3 and the departure stream, stages No. 9, Rupert and the helicopter,
/// and coordinates pitch/shake cues before fading into the return task.
/// Commands, payloads and actor_511000 resources must stay live through execution.
extern EvsCommand gAcropolisHelicopterLandingPadDepartureScript[69];

/// Skips helicopter departure and starts the same movie-backed return to MIST.
///
/// Seven commands including END. Stops music, fades out and cleans up scene
/// control before spawning the return task. The interpreter borrows this array
/// as the departure's skip target; keep the room loaded through execution.
extern EvsCommand gAcropolisHelicopterLandingPadDepartureSkipScript[7];

/// Script-task indices in `gAcropolisHelicopterLandingPadScriptTasks`.
enum {
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_PREPARE_DEPARTURE = 0,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_MOVE_PLAYER       = 1,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_SCREEN_SHAKE      = 2,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_SHAKE_TIMELINE    = 3,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_CONFIRM_DEPARTURE = 4,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_RETURN_TO_MIST    = 5,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_TURN_PLAYER       = 6,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_PLAYER_PITCH      = 7,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_COUNT             = 8,
};

/// Eight bodyless room-control tasks followed by the descriptor terminator.
///
/// Indexed spawns accept TASK_* slots 0..7; entry 8 is never spawned. The pitch
/// task runs at priority 97, after the seven priority-32 script tasks. Callbacks
/// and any borrowed room resources must remain loaded through task teardown.
extern TaskDesc gAcropolisHelicopterLandingPadScriptTasks[ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_COUNT + 1];

/// Starts the lift's 120-tick rise from the lower stop for departure.
///
/// Only the zero run selector in `animationId` is read; the stored bank, blend
/// and collision fields are ignored. Dispatch borrows the room-owned request.
extern AnimationPlayRequest gAcropolisHelicopterLandingPadLiftRaiseRequest;

/// Mutable player-animation request used while boarding the rising lift.
///
/// Departure setup sets clip 9 and the equipped weapon's bank before dispatch.
/// Resets playback and disables world collision; the receiver copies the request
/// synchronously. The request belongs to the room overlay and is reused in place.
extern AnimationPlayRequest gAcropolisHelicopterLandingPadLiftPlayerAnimationRequest;

/// Player placement applied after the encounter when room action 1 is latched.
///
/// Position (-6801, 0, -1998) uses the player's parent-coordinate units; yaw
/// 1024 is a quarter turn. All other Euler components are zero. Dispatch copies
/// the values synchronously and retains no pointer.
extern ActorTransform gAcropolisHelicopterLandingPadPostEncounterPlayerPlacement;

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
