#ifndef STAGES_AKROPOLIS_ROOMS_HELICOPTER_LANDING_PAD_HELICOPTER_LANDING_PAD_PRIVATE_H
#define STAGES_AKROPOLIS_ROOMS_HELICOPTER_LANDING_PAD_HELICOPTER_LANDING_PAD_PRIVATE_H

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
extern TaskDesc gAkropolisHelicopterLandingPadMovieTasks[2];

/// Encounter/departure phases consulted by the room's departure callbacks.
enum {
    AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_WAITING            = 0,
    AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_ENCOUNTER_FINISHED = 2,
    AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_ACCEPTED = 3,
    AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_READY    = 4,
};

/// Progress of the encounter and subsequent helicopter departure.
extern s32 gAkropolisHelicopterLandingPadEncounterPhase;

/// Latched permission to start the encounter after entering logical view 5.
extern s32 gAkropolisHelicopterLandingPadEncounterReady;

/// Latched permission to apply the post-encounter player placement.
extern s32 gAkropolisHelicopterLandingPadPostEncounterPlacementEnabled;

/// Nine mutable action regions for the landing pad's single room.
extern WorldCollisionTrigger gAkropolisHelicopterLandingPadActionTriggers[9];

/// Two crossing room-space quads that occlude actor line-of-sight queries.
extern WorldCollisionOccluder gAkropolisHelicopterLandingPadSightOccluders[2];

/// The landing pad's authored collection of one directional and 22 point lights.
extern WorldCoordRoomLights gAkropolisHelicopterLandingPadRoomLights;

extern SVECTOR ActorContact_ScratchPosition;

/// Persistent fire-escape transition request and resolved reply for the stair exit.
extern RoomEventMsg gAkropolisHelicopterLandingPadExitRequest;

/// Four room-message handlers followed by the task-message end marker.
extern TaskMessageEntry gAkropolisHelicopterLandingPadRoomMessages[5];

/// Player placement at the lift origin with quarter-turn yaw 1024.
extern ActorTransform gAkropolisHelicopterLandingPadLiftPlayerPlacement;

/// Departure scene-mark XYZ followed by authored storage of unproven meaning.
typedef struct {
    VECTOR3 destination;    // Scripted approach destination in parent-coordinate game units
    s32     unknown_0C[15]; // Uninterpreted stored words; role unproven
} AkropolisHelicopterLandingPadSceneMarkStorage;
STATIC_ASSERT_SIZEOF(AkropolisHelicopterLandingPadSceneMarkStorage, 0x48);

/// Destination (-3496, -2871, -1283) for the player's departure-scene approach.
extern AkropolisHelicopterLandingPadSceneMarkStorage gAkropolisHelicopterLandingPadSceneMarkStorage;

/// Starts the landing pad's ambient sound and ends the setup event.
extern EvsCommand gAkropolisHelicopterLandingPadAmbientSoundScript[2];

/// Presents the encounter introduction and starts No. 9's fight.
extern EvsCommand gAkropolisHelicopterLandingPadEncounterIntroScript[58];

/// Skips the encounter introduction and establishes its playable fight state.
extern EvsCommand gAkropolisHelicopterLandingPadEncounterIntroSkipScript[16];

/// Presents the encounter ending and sends the lift down to its lower stop.
extern EvsCommand gAkropolisHelicopterLandingPadEncounterEndingScript[38];

/// Skips the encounter ending, parks the lift and restores normal play.
extern EvsCommand gAkropolisHelicopterLandingPadEncounterEndingSkipScript[19];

/// Presents helicopter departure and starts the movie-backed return to MIST.
extern EvsCommand gAkropolisHelicopterLandingPadDepartureScript[69];

/// Skips helicopter departure and starts the same movie-backed return to MIST.
extern EvsCommand gAkropolisHelicopterLandingPadDepartureSkipScript[7];

/// Script-task indices in `gAkropolisHelicopterLandingPadScriptTasks`.
enum {
    AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_PREPARE_DEPARTURE = 0,
    AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_MOVE_PLAYER       = 1,
    AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_SCREEN_SHAKE      = 2,
    AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_SHAKE_TIMELINE    = 3,
    AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_CONFIRM_DEPARTURE = 4,
    AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_RETURN_TO_MIST    = 5,
    AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_TURN_PLAYER       = 6,
    AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_PLAYER_PITCH      = 7,
    AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_COUNT             = 8,
};

/// Eight bodyless room-control tasks followed by the descriptor terminator.
extern TaskDesc gAkropolisHelicopterLandingPadScriptTasks[AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_COUNT + 1];

/// Starts the lift's 120-tick rise from the lower stop for departure.
extern AnimationPlayRequest gAkropolisHelicopterLandingPadLiftRaiseRequest;

/// Mutable player-animation request used while boarding the rising lift.
extern AnimationPlayRequest gAkropolisHelicopterLandingPadLiftPlayerAnimationRequest;

/// Player placement applied after the encounter when room action 1 is latched.
extern ActorTransform gAkropolisHelicopterLandingPadPostEncounterPlayerPlacement;

/// Advances encounter completion once the actor and presentation holds have cleared.
void akropolisHelicopterLandingPadUpdateEncounterPhase(Task* unusedTask);

// Callbacks referenced by the overlay's shared data tables.
/// Raises the player on the lift and prepares the helicopter departure scene.
void akropolisHelicopterLandingPadPrepareDepartureTask(Task* task);

/// Times four screen shakes and their controller vibration during departure.
void akropolisHelicopterLandingPadDepartureShakeTimelineTask(Task* task);

/// Restores health, starts the departure movie and reloads the M.I.S.T. arrival.
void akropolisHelicopterLandingPadReturnToMistTask(Task* task);

/// Turns the live player's yaw toward `spawnArg1.value` by 256 angle units per update.
void akropolisHelicopterLandingPadTurnPlayerYawTask(Task* task);

/// States of the player's model-part pitch pulse; stored in the task's state word.
enum {
    AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_IDLE = 0,
    AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_RISE = 1,
    AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_FALL = 2,
};

/// Composes a rising then falling negative pitch into player model part 4.
void akropolisHelicopterLandingPadPlayerPitchPulseTask(Task* task);

/// Resolves ordinary fire-escape departure versus the post-encounter CAP exit.
s32 akropolisHelicopterLandingPadResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);

/// Refuses every `ROOM_MESSAGE_USE_KEY_ITEM` request without changing the room.
s32 akropolisHelicopterLandingPadRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);

/// Handles the room's automatic encounter and post-encounter placement triggers.
s32 akropolisHelicopterLandingPadHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Starts departure confirmation for room command 4 after the encounter ends.
s32 akropolisHelicopterLandingPadHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);

/// Starts the departure screen-shake/vibration timeline at frame zero.
void akropolisHelicopterLandingPadStartDepartureShakeTimeline(void);

/// Starts the player's scripted approach to the departure scene mark.
void akropolisHelicopterLandingPadStartPlayerMoveToSceneMark(void);

/// Starts the movie and M.I.S.T. return task after normal or skipped departure.
void akropolisHelicopterLandingPadStartReturnToMist(void);

/// Applies the post-encounter player placement when room action 1 was latched.
void akropolisHelicopterLandingPadPlacePlayerAfterEncounter(void);

/// Starts the singleton scripted player turn toward `targetYaw` (0..4095).
void akropolisHelicopterLandingPadStartPlayerYawTurn(s32 targetYaw);

/// Releases the encounter's battle hold, credits its enemy rewards and arms a three-frame end delay.
void akropolisHelicopterLandingPadReleaseEncounterBattle(void);

/// Requests cancellation of room effects and locks attachment actions for the encounter ending.
void akropolisHelicopterLandingPadLockAttachmentsForEncounter(void);

/// Sets the persistent player pitch task's state for a script cue.
void akropolisHelicopterLandingPadSetPlayerPitchPulseState(s32 pulseState);

/// Moves the player to the scene mark and waits for scripted motion to finish.
void akropolisHelicopterLandingPadMovePlayerToSceneMarkTask(Task* task);

/// Runs a vertical screen shake with a triangular amplitude envelope.
void akropolisHelicopterLandingPadScreenShakeTask(Task* task);

/// Asks for departure confirmation and starts the lift/scene preparation task.
void akropolisHelicopterLandingPadConfirmDepartureTask(Task* task);

#endif // STAGES_AKROPOLIS_ROOMS_HELICOPTER_LANDING_PAD_HELICOPTER_LANDING_PAD_PRIVATE_H
