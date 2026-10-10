#ifndef SRC_ROOMS_SHELTER_B6_TRAINING_ROOM_SHELTER_B6_TRAINING_ROOM_PRIVATE_H
#define SRC_ROOMS_SHELTER_B6_TRAINING_ROOM_SHELTER_B6_TRAINING_ROOM_PRIVATE_H

#include "types.h"

#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/coord.h"
#include "main/task_types.h"

/// Number of ring-band shapes selected by the effect's band index (0..2).
enum { SHELTER_B6_TRAINING_ROOM_BAND_COUNT = 3 };

/// Stored byte stride between bands in the ring texture-phase table.
enum { SHELTER_B6_TRAINING_ROOM_RING_PHASE_ROW_STRIDE = 16 };

/// Number of vertices in each ring and quads in a band, also its used phase bytes.
///
/// Adjacent vertices differ by ONE / count angle units, rounded down;
/// the next vertex wraps with remainder because the count is not a power of two.
#define SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT 6

/// Signed texture phases indexed by band 0..2 and segment 0..5.
///
/// Initialization writes the low byte of each LCG draw in three complete
/// passes over the selected band. Drawing adds effect age and takes signed
/// remainder modulo six; negative results wrap through the packet UV bytes.
/// The 16-byte row stride is retained; bytes 6..15 have no established role.
/// Storage lives with this room overlay and owns no effect resources.
extern s8 gShelterB6TrainingRoomRingTexturePhases[SHELTER_B6_TRAINING_ROOM_BAND_COUNT][SHELTER_B6_TRAINING_ROOM_RING_PHASE_ROW_STRIDE];

/// Borrowed summon-ring coordinate supplying the beam's world-space origin.
///
/// Ring initialization clears it; completed buildup publishes the coordinate.
/// Drawing reads its composed translation without updating it. Release retains
/// the pointer, so every beam draw must end before the ring coordinate dies.
/// NULL suppresses beam drawing; a non-NULL value alone does not prove liveness.
extern const GfxCoord* gShelterB6TrainingRoomSummonBeamOrigin;

/// Borrowed masked-body joint 1 used as the shield energy arcs' endpoint.
///
/// The body glow publishes it while room effects run. Arc drawing reads the
/// cached composed translation; the body model must outlive every arc using it.
/// Publication and teardown do not clear it. The body glow must run before
/// the first arc draw, and stale non-NULL values do not establish liveness.
extern const GfxCoord* gShelterB6TrainingRoomEnergyArcAnchor;

/// Wrapping generation identifying the newest training-room hit flash.
///
/// Room-glow initialization resets it to zero. Each new flash increments the
/// 16-bit value and copies it into its task's spawn word; unequal generations
/// release older flashes. It is a replacement stamp, not an active-task count.
extern u16 gShelterB6TrainingRoomHitFlashGeneration;

/// Terminated room-controller message table, borrowed while this overlay is loaded.
///
/// Resolves transitions, rejects key items, ignores room actions, selects part
/// dialogue and starts the defeat scene. Unknown IDs return zero at the sentinel.
extern TaskMessageEntry gShelterB6TrainingRoomMessages[6];

/// Requests that placed actor 3 stop the training-room controller and its sound.
///
/// Borrowed through synchronous actor-command dispatch. Context tags 5/24 are
/// retained; the receiver ignores them and latches stop for nonzero command 1.
extern ActorCommand gShelterB6TrainingRoomControllerStopCommand;

/// Priority-96 player head-aim controller descriptor with no model or work body.
///
/// Spawned with zero payload words; the room tracks its handle until release.
extern TaskDesc gShelterB6TrainingRoomPlayerHeadAimTaskDesc;

/// Skippable defeat scene replacing the battle body with Masked Eve's scene walker.
///
/// Installs player clips 48..51, stages the player, Masked Eve and Kyle, runs
/// CAP dialogue 4, restores weapons, hides the scene walkers and releases head aim.
/// Borrowed commands and payloads require the room and actor resources to stay
/// loaded until completion. The early fade disables skip; its later target is
/// `gShelterB6TrainingRoomDefeatSkipScript`. Ends with the event-script sentinel.
extern EvsCommand gShelterB6TrainingRoomDefeatScript[58];

/// Defeat-scene skip path placing the player at the final spot and hiding the walkers.
///
/// Retains the skip sound, installs the same player clips, stops battle
/// presentation, releases head aim and requests scene cleanup before ending.
/// Borrows the room's payloads and callbacks until the interpreter completes.
extern EvsCommand gShelterB6TrainingRoomDefeatSkipScript[14];

/// Departure script handing display to the movie before reloading the nursery.
///
/// Starts the room's sound cue, waits three updates on either side of movie
/// launch, enables display and requests the saved-location reload, then ends.
/// Movie, script and callback resources must remain loaded through the handoff.
extern EvsCommand gShelterB6TrainingRoomDepartureScript[7];

/// Two zero-body priority-192 movie tasks: launcher at 0 and hold-after-playback at 1.
///
/// Shared by execution-task launch and display-task handoff; the table and
/// callbacks are borrowed from this room overlay through movie restoration.
extern TaskDesc gShelterB6TrainingRoomMovieTasks[2];

/// Refuses every selected key item without changing inventory or starting a scene.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM` and returns `ROOM_KEY_ITEM_USE_REFUSED`.
/// The selected collected-item `itemId` and all other callback arguments are unused.
s32 shelterB6TrainingRoomRefuseKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);

/// Permits a room transition after resolving its destination from Neo Ark progress.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`: copies the complete eight-byte request
/// to the reply, then resolves its room on execution. Queries echo the request.
/// Both records must be live and two-byte-aligned, and the reply writable;
/// they may alias. Retains neither pointer and always returns 1 (allowed).
/// The receiver and message ID are unused callback arguments.
s32 shelterB6TrainingRoomResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);

/// Chooses Eve-part dialogue from destruction flags and the current battle phase.
///
/// Handles `ROOM_MESSAGE_COMMAND`. Commands 5 and 6 select training-room parts
/// 0 and 1: destroyed or non-battle parts use CAP 7/8, live parts in battle use
/// CAP 5/6. Command 4 checks corridor part 2 and uses CAP 7 when destroyed,
/// CAP 10 outside battle, or CAP 4 in battle. Other commands have no effect.
/// Returns zero and ignores the receiver, message ID and second payload word.
s32 shelterB6TrainingRoomHandlePartCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);

/// Ignores trigger requests delivered through `DIRECTION_MESSAGE_ROOM_ACTION`.
///
/// Returns zero without reading either payload word or changing room state.
/// All callback arguments are unused.
s32 shelterB6TrainingRoomIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

/// Starts the skippable defeat scene and latches its pending departure.
///
/// Handles `ROOM_MESSAGE_ACTOR_EVENT` from the defeated enemy. Marks the weapon
/// for re-equipping, starts the normal/skip script pair with HUD hide/restore,
/// and sends the scene command to placed actor 3, which must be live.
/// Payload words and receiver are unused; returns zero. Script and actor-command
/// storage remain borrowed from the loaded room overlay.
s32 shelterB6TrainingRoomStartDefeatScene(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

/// Starts and tracks the player head-aim controller for the defeat scene.
///
/// The script calls this once before fading head aim in. Publishes the spawned
/// priority-96, zero-body task handle; allocation failure publishes NULL.
/// Repeated calls replace the handle without releasing its previous task.
/// The player, target actor and room resources must stay live through head aim;
/// `shelterB6TrainingRoomControlPlayerHeadAim` fades or releases the controller.
void shelterB6TrainingRoomStartPlayerHeadAim(void);

/// Script commands for fading player head aim or releasing its controller task.
enum {
    SHELTER_B6_TRAINING_ROOM_HEAD_AIM_RELEASE  = -1,
    SHELTER_B6_TRAINING_ROOM_HEAD_AIM_FADE_OUT = 0,
    SHELTER_B6_TRAINING_ROOM_HEAD_AIM_FADE_IN  = 1
};

/// Fades player head aim in or out, or releases the tracked head-aim task.
///
/// Commands 0 and 1 set the existing task's fade direction; every other word
/// kills it and clears its borrowed handle. Does nothing when no task is tracked.
/// Does not spawn a task or reset its current interpolation weight.
void shelterB6TrainingRoomControlPlayerHeadAim(s32 command);

/// Blends the player's head toward placed actor 1 during the defeat scene.
///
/// State 0 uses `killCountdown` as a signed 1/4096 weight, stepping by 256
/// toward 0 for a zero `spawnArg1.value`, or toward 4096 otherwise. Halfword
/// truncation precedes signed clamping; normal operation keeps the weight in
/// 0..4096. Limits yaw to 512 and pitch to 256 angle units (4096 per turn),
/// subject to the animation helper's existing-pose limits.
/// Requires the player and target to own live TMD models with at least five
/// coordinates in the expected root-to-head order. Borrows them for each call.
/// Holds while the event-script run gate is closed; any nonzero task state
/// kills the controller once that gate opens. Allocates no work or model body.
void shelterB6TrainingRoomPlayerHeadAimTask(Task* task);

/// Queues the room's movie launcher on the current execution list.
///
/// The departure script calls this before requesting the nursery reload.
/// The loaded room descriptors, movie and display resources must remain live
/// through the handoff. Passes zero spawn words and ignores allocation failure.
void shelterB6TrainingRoomStartMovie(void);

/// Requests weapon re-equipping and optionally replaces the battle-end hold.
///
/// A zero `endDelayFrames` preserves the existing delay. Every nonzero word
/// stores its low eight bits as remaining actor-update frames; the scripts
/// use 100 normally and 8 when skipped. Does not change the battle phase.
void shelterB6TrainingRoomPrepareDefeatScene(s32 endDelayFrames);

/// Requests a session reload into the nursery's first arrival and room variant.
///
/// Updates area, warp and room in the live save, preserving stage and view.
/// Queues the reload task with frame capture; the script calls it after starting
/// the movie. Requires the active Neo Ark stage and the reload task bank.
void shelterB6TrainingRoomReloadNursery(void);

/// Locks attachment changes and stops battle effects and non-ambient sound scripts.
///
/// Shared by the normal and skipped defeat scenes. Requests cancellation of
/// all room effects and sound-script stops that retain release behavior.
/// Ambient scripts and the other attachment flags are preserved.
void shelterB6TrainingRoomStopBattlePresentation(void);

/// Gives display presentation to the room's movie player, then releases the launcher.
///
/// Launches descriptor 1 with zero spawn words, selects task-only flipping and
/// queues the current camera and sprite packets. Requires valid loaded session,
/// camera, sprite and movie resources through playback/restoration. No task work
/// is allocated here; launch failure is ignored and still releases `task`.
void shelterB6TrainingRoomStartMovieTask(Task* task);

#endif // SRC_ROOMS_SHELTER_B6_TRAINING_ROOM_SHELTER_B6_TRAINING_ROOM_PRIVATE_H
