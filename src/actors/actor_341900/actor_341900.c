#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"
#include "actors/glutton_display_types.h"

#include "actors/actor_444000.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/captions.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/actor/actor_messages.h"

/// `_Actor341900EventWork::playerAction`: the one-shot request the event script
/// hands the player.
///
/// The "event" clips are those of the package's own animation sets; the
/// "weapon" clips are those of the bank the equipped weapon selects.
enum {
    ACTOR_341900_PLAYER_ACTION_NONE           = 0, // Nothing pending
    ACTOR_341900_PLAYER_ACTION_PLACE_AT_START = 1, // Places the player at the opening spot and cuts to weapon clip 1
    ACTOR_341900_PLAYER_ACTION_WALK_TO_MARK   = 2, // Walks the player to the mark
    ACTOR_341900_PLAYER_ACTION_EVENT_CLIP_0   = 3, // Places the player at the event clips' spot and cuts to event clip 0
    ACTOR_341900_PLAYER_ACTION_EVENT_CLIP_1   = 4, // Blends into event clip 1 over ten frames
    ACTOR_341900_PLAYER_ACTION_WEAPON_CLIP_9  = 5, // Cuts to weapon clip 9 and places the player at the turned spot, facing the opposite way to the opening
    ACTOR_341900_PLAYER_ACTION_PLACE_AT_END   = 6, // Places the player at the closing spot, facing that same way
};

/// `_Actor341900EventWork::stagingMode`: what the event does with the Glutton
/// and the doors.
///
/// The modes marked one-shot clear themselves on the tick that runs them; the
/// others repeat every tick until another mode replaces them. A value outside
/// this list is cleared to `ACTOR_341900_STAGING_NONE` when it is run.
enum {
    ACTOR_341900_STAGING_NONE             = 0, // Nothing to do
    ACTOR_341900_STAGING_GLUTTON_ADVANCES = 1, // Blends the Glutton into its clip 1 at its first placement, waits 16 ticks, then slides it 30 units a tick along +X
    ACTOR_341900_STAGING_GLUTTON_CLIP_2   = 2, // Blends the Glutton into its clip 2 at its second placement; on the 61st tick after that spawns four effects around a point ahead of its part 2, then clears itself
    ACTOR_341900_STAGING_DOORS_SHUT       = 3, // One-shot: places both door halves shut and takes the area's placement-0 actor out of the draw
    ACTOR_341900_STAGING_DOORS_OPEN       = 4, // Starts both door halves from the shut placement and slides them apart, 20 units a tick each along Z
};

/// Work block of the package's event task: the scene in which a pair of door
/// halves slides open and the Glutton's display model comes through.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work` for
/// the task's life. The event script cannot be handed the task, so its
/// callbacks reach the block through the task pointer that state publishes.
///
/// The script drives two small machines here by leaving a request in each:
/// `playerAction` for the player, and `stagingMode` for the Glutton and the two
/// door halves. The task runs both every tick while the script plays.
///
/// Nothing accesses the three `pad` runs; their roles are unproven.
typedef struct {
    Task*          player;                 // The player task, the receiver of the event's scripted-control and animation messages
    Task*          placement0Actor;        // Task of the area's enemy with placement index 0, looked up as the block is set up; `ACTOR_341900_STAGING_DOORS_SHUT` stops it being drawn
    Task*          glutton;                // Body of the Glutton's display model, which its five part tasks attach to; `NULL` once the script has removed it
    Task*          doors[2];               // The two door halves, which slide apart along Z: 0 toward -Z, 1 toward +Z. `NULL` once the script has removed them
    ActorTransform gluttonPlacement;       // Placement last sent to the Glutton by `ACTOR_341900_STAGING_GLUTTON_ADVANCES`, stepped along X as it slides
    ActorTransform doorPlacements[2];      // Placement last sent to each door half by `ACTOR_341900_STAGING_DOORS_OPEN`, stepped along Z as it slides
    u16            playerAction;           // Pending player request, cleared once performed (`ACTOR_341900_PLAYER_ACTION_NONE`, else one of `ACTOR_341900_PLAYER_ACTION_*`)
    u16            playerActionStep;       // Zeroed whenever the script posts a player action; never read, as no action here takes more than one tick
    byte           pad_60[0x4];            // Never accessed
    u16            stagingMode;            // Current staging mode (`ACTOR_341900_STAGING_NONE`, else one of `ACTOR_341900_STAGING_*`)
    u16            stagingStep;            // Step within the mode (0 set it up, 1 run it); zeroed whenever the script posts a mode
    u16            stagingTicks;           // Ticks counted within step 1 of the two Glutton modes: up to 16 before the slide, up to 61 before the effects
    byte           pad_6A[0x2];            // Never accessed
    u16            playerEquipmentRemoved; // 1 from the scene killing the player's equipment tasks until it spawns the weapon's again (0 otherwise)
    byte           pad_6E[0x2];            // Never accessed
} _Actor341900EventWork;
STATIC_ASSERT_SIZEOF(_Actor341900EventWork, 0x70);

/// Animation id of the clip the Glutton advances in, as the event requests it
/// of the body. Two cues of this clip each make the body start a pad-vibration
/// script.
enum { ACTOR_341900_GLUTTON_CLIP_ADVANCE = 1 };

/// Work block of each task that makes up the Glutton's display model: the
/// body, its two legs and the three further parts hung off the body.
///
/// All six tasks share one spawn step, which allocates the block zeroed at
/// this size and keeps it at `Task::work`. The block supplies what the task's
/// model does not own: the light and colour matrices it is drawn with and, for
/// the body and the legs, the playback rig over the model. A leg registers
/// itself with the body as it spawns; the body then ticks both legs' playback
/// after its own and applies every play request it takes to all three.
typedef struct {
    ActorAnimRig8        rig;               // Playback over the task's model: the body drives slots 1 to 7 and a leg slots 0 to 3; the other parts never bind it
    MATRIX               light;             // Light matrix the model's `TmdObject::lightMtx` points at
    MATRIX               color;             // Colour matrix the model's `TmdObject::colorMtx` points at
    AnimationPlayRequest playRequest;       // Body only: the last play request it took, copied whole. Its `animationId` is also the entry of the package's follow-up clip table consulted once every driven slot has settled; nothing writes a leg's, so it stays 0
    byte                 unknown_228[0x8];  // Zeroed allocation bytes; no access established and role unproven
    s32                  lastCue;           // Body only: cue index of slot 2's current pose on the previous tick of the advance clip, so a cue is answered only on the first tick that shows it
    byte                 unknown_234[0x14]; // Zeroed allocation bytes; no access established and role unproven
    Task*                parent;            // Task this one was spawned for and made a child of: the event task for the body, the body for a leg or part
    Task*                legRight;          // Body only: the right leg's task, whose playback the body ticks after its own. The leg stores itself here as it spawns
    Task*                legLeft;           // Body only: the left leg's task, likewise
    u16                  requestedClip;     // Body only: low 16 bits of `playRequest.animationId`, kept beside it; the body checks its cues only while this is `ACTOR_341900_GLUTTON_CLIP_ADVANCE`
    byte                 unknown_256[0x2];  // Zeroed allocation bytes; no access established and role unproven
} _Actor341900GluttonModelWork;
STATIC_ASSERT_SIZEOF(_Actor341900GluttonModelWork, 0x258);

static void _actor341900DoorHalfTask(Task* task);
static void _actor341900GluttonPartTask(Task* task);
static void _actor341900GluttonBodyTask(Task* task);
static void _actor341900EventTask(Task* task);
static void _actor341900FadeInTask(Task* task);
static void _actor341900BroadcastActorCommand(s16 commandId);
static void _actor341900SetGluttonDrawMode(s32 drawMode);
static void _actor341900SetPlayerDrawMode(s32 drawMode);
static void _actor341900RemovePlayerEquipment(void);
static void _actor341900RestorePlayerEquipment(void);
static void _actor341900RemoveGlutton(void);
static void _actor341900RemoveDoors(void);
static void _actor341900StartFadeIn(void);
static void _actor341900RequestPlayerAction(s16 playerAction);
static void _actor341900SetStagingMode(s16 stagingMode);
static void _actor341900FinishSkippedScene(void);
static void _actor341900StageSceneAudioStart(void);
static void _actor341900EnqueueScenePlayback(void);
static void _actor341900FinishScene(void);

static void _actor341900PlayGluttonAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static void _actor341900IgnoreActorCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);

static AnimationPackedPose _gActor341900Animation01B5CBank1[6] = {
#include "assets/actor_341900_animation_01B5C_bank1.inc"
};

static AnimationPackedRotation _gActor341900Animation01B5CBank4[46] = {
#include "assets/actor_341900_animation_01B5C_bank4.inc"
};

static AnimationRecord _gActor341900Animation01B5CRecords[109] = {
#include "assets/actor_341900_animation_01B5C_records.inc"
};

static u16 _gActor341900Animation01B5CIndices[20] = {
#include "assets/actor_341900_animation_01B5C_indices.inc"
};

static AnimationSet _gActor341900Animation01B5C = {
    _gActor341900Animation01B5CRecords,
    _gActor341900Animation01B5CIndices,
    { NULL, _gActor341900Animation01B5CBank1, NULL, NULL, _gActor341900Animation01B5CBank4, NULL, NULL, NULL },
};

/// The player's two entrance-event clips, installed in place of its weapon bank.
///
/// Clip 0 uses this package's set; clip 1 uses the co-loaded Glutton package's
/// set. Both packages must remain loaded while the player borrows this table;
/// ending scripted control restores the weapon bank.
static AnimationSet* _gActor341900PlayerAnimationSets[2] = {
    &_gActor341900Animation01B5C,
    &gActor444000Animation2E198,
};

/// Glutton body clips 0 initial, 1 advance, and 2 second staging clip.
///
/// Playback drives model tracks 1..7 and leaves root track 0 alone. The table
/// and its actor_444000 sets are borrowed for the display body's lifetime.
static AnimationSet* _gActor341900GluttonBodyAnimationSets[3] = {
    &gActor444000Animation20BD4,
    &gActor444000Animation21014,
    &gActor444000Animation2C240,
};

/// Right-leg counterparts of the Glutton body's three entrance clips.
///
/// The body synchronizes clip selection on all four leg tracks (0..3).
/// The table and its actor_444000 sets must outlive the leg's playback.
static AnimationSet* _gActor341900GluttonRightLegAnimationSets[3] = {
    &gActor444000Animation20C80,
    &gActor444000Animation212D8,
    &gActor444000Animation2C2CC,
};

/// Left-leg counterparts of the Glutton body's three entrance clips.
///
/// The body synchronizes clip selection on all four leg tracks (0..3).
/// The table and its actor_444000 sets must outlive the leg's playback.
static AnimationSet* _gActor341900GluttonLeftLegAnimationSets[3] = {
    &gActor444000Animation20D2C,
    &gActor444000Animation215B4,
    &gActor444000Animation2C358,
};

/// Values stored in the Glutton's signed follow-up clip table.
enum {
    ACTOR_341900_GLUTTON_NO_FOLLOW_UP = -1,
    ACTOR_341900_GLUTTON_INITIAL_CLIP = 0,
};

/// Signed successor choices for a settled Glutton clip; -1 disables blending.
///
/// Indexed by the retained play-request clip id. Clips 0..2 have no successor.
/// Entry 3 selects clip 0, but no scene request selects 3 and the playback
/// tables contain only three sets; this entry does not establish a fourth clip.
static s16 _gActor341900GluttonFollowUpClips[4] = {
    ACTOR_341900_GLUTTON_NO_FOLLOW_UP,
    ACTOR_341900_GLUTTON_NO_FOLLOW_UP,
    ACTOR_341900_GLUTTON_NO_FOLLOW_UP,
    ACTOR_341900_GLUTTON_INITIAL_CLIP,
};

/// Closed placements of the door halves, in their view-parent coordinate frame.
///
/// Entries 0/1 address the halves that open toward -Z/+Z. Both start at the
/// same position; opening copies entry 0 into both moving placements. Position
/// is in game units and Euler angles are in 4096 units per turn.
static ActorTransform _gActor341900DoorShutPlacements[2] = {
    { { -60, 150, -2650, 0 }, { 0, 0, 0, 0 } },
    { { -60, 150, -2650, 0 }, { 0, 0, 0, 0 } },
};

/// Unreferenced transform pair with Z positions -5300 and 0; its role is unproven.
static ActorTransform D_actor_341900_80163A08[2] = {
    { { -60, 150, -5300, 0 }, { 0, 0, 0, 0 } },
    { { -60, 150, 0, 0 }, { 0, 0, 0, 0 } },
};

/// The entrance door halves' placement and model-draw message handlers.
///
/// There is no terminator: send only ACTOR_MESSAGE_SET_MODEL_DRAW (integer
/// ACTOR_MESSAGE_DRAW_* mode) or ACTOR_MESSAGE_PLACE (borrowed ActorTransform).
/// Both handlers return void, so every sender must ignore the dispatch result.
/// The installed table and code must remain loaded for each door's lifetime.
static TaskMessageEntry _gActor341900DoorMessages[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
};

/// Starting placement of the Glutton's +X advance, in game units and turn angles.
///
/// Copied into event-owned moving storage before each placement dispatch;
/// the display root is parented to the view. Angles use 4096 units per turn.
static ActorTransform _gActor341900GluttonAdvancePlacement = { { -5000, 0, -2450, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Glutton placement for clip 2 and its delayed effects, in the view-parent frame.
///
/// Consumed synchronously as an ActorTransform. Position uses game units;
/// yaw is a quarter turn, with 4096 angle units per turn.
static ActorTransform _gActor341900GluttonClip2Placement = { { -3000, 0, -2450, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Message handlers shared by the entrance Glutton body and its five child parts.
///
/// No terminator: send only the four installed IDs. Draw takes an integer
/// ACTOR_MESSAGE_DRAW_* mode; place borrows an ActorTransform; actor commands
/// are ignored. Play borrows an AnimationPlayRequest for local clip 0..2 and
/// requires the body receiver with both legs initialized; attached parts must
/// not receive play requests. All callbacks return void: ignore every result.
/// The table and callbacks must remain loaded while any part can receive messages.
static TaskMessageEntry _gActor341900GluttonMessages[4] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor341900IgnoreActorCommand },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor341900PlayGluttonAnimation },
};

/// Body-coordinate attachments indexed by the Glutton display task's part id.
///
/// Record 0 belongs to the body and is not read; 1/2 attach the right/left leg
/// to body coordinate 0. Records 3..5 attach the remaining models to coordinates
/// 4, 3 and 2. XYZ translations use each parent coordinate's local game units.
/// Child spawn arguments are 1..5; the table is consumed during initialization.
static GluttonPartAttachment _gActor341900GluttonPartAttachments[6] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 89, 100, 4 },
    { 0, 0, 0, 3 },
    { 0, 1660, 200, 2 },
};

/// Opening player placement before weapon clip 1, in game units and turn angles.
///
/// The player's placement handler consumes it synchronously in the root-parent
/// frame; yaw is a quarter turn, with 4096 angle units per turn.
static ActorTransform _gActor341900PlayerStartPlacement = { { 500, 0, -1000, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player walk destination following the entrance's opening placement.
///
/// Move-to consumes only XYZ in root-parent game units and copies its target;
/// the stored rotation has no effect on this request.
static ActorTransform _gActor341900PlayerWalkTarget = { { 3000, 0, -1000, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player placement before entrance-event clip 0, in game units and turn angles.
///
/// Consumed synchronously in the player's root-parent frame; event clip 1 then
/// blends at the same placement. Yaw is a quarter turn (4096 units per turn).
static ActorTransform _gActor341900PlayerEventClipPlacement = { { 3200, 0, -2500, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player placement paired with the return to weapon clip 9 during Glutton clip 2.
///
/// Consumed synchronously in root-parent game units. The three-quarter-turn yaw
/// faces opposite the opening placement (4096 angle units per turn).
static ActorTransform _gActor341900PlayerWeaponClip9Placement = { { 1000, 0, -1900, 0 }, { 0, 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Final player placement shared by normal entrance completion and scene skipping.
///
/// Consumed synchronously in root-parent game units. Yaw is three quarters of
/// a turn, with 4096 angle units per turn; the skip path also restores weapon clip 9.
static ActorTransform _gActor341900PlayerEndPlacement = { { 4000, 0, -1900, 0 }, { 0, 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Scene/audio stream key for the Glutton entrance: group 4, stream 19, qualifier 11.
///
/// The event interpreter retains this overlay-owned key through playback;
/// actor_341900 must remain loaded until normal or skipped script completion.
static EvsSceneKey _gActor341900GluttonEntranceSceneKey = { 4, 19, 11 };

/// Actor command broadcast by both paths that end this entrance event.
enum { ACTOR_341900_ACTOR_COMMAND_END_ENTRANCE = 1 };

/// Loaded CAP command slot used to start the entrance's cue sequence.
enum { ACTOR_341900_ENTRANCE_CAP_COMMAND = 6 };

/// Normal Glutton entrance program, synchronizing actor staging with CAP cues.
///
/// Borrows the live published event task and the co-loaded actor_444000 assets.
/// Installs scene/audio playback, opens the doors, switches player animations,
/// reveals and advances the display Glutton, then removes it and resumes play.
/// The interpreter retains this 47-command array through its END opcode.
static EvsCommand _gActor341900GluttonEntranceScript[47] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_341900_ENTRANCE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900StartFadeIn }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900RequestPlayerAction }, { .value = ACTOR_341900_PLAYER_ACTION_PLACE_AT_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900SetStagingMode }, { .value = ACTOR_341900_STAGING_DOORS_SHUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor341900GluttonEntranceSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900EnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900RequestPlayerAction }, { .value = ACTOR_341900_PLAYER_ACTION_WALK_TO_MARK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900SetStagingMode }, { .value = ACTOR_341900_STAGING_DOORS_OPEN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900RemovePlayerEquipment }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900RequestPlayerAction }, { .value = ACTOR_341900_PLAYER_ACTION_EVENT_CLIP_0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900SetStagingMode }, { .value = ACTOR_341900_STAGING_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900RemoveDoors }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900RequestPlayerAction }, { .value = ACTOR_341900_PLAYER_ACTION_EVENT_CLIP_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor341900SetPlayerDrawMode }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor341900SetGluttonDrawMode }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900SetStagingMode }, { .value = ACTOR_341900_STAGING_GLUTTON_ADVANCES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor341900SetPlayerDrawMode }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900RestorePlayerEquipment }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900SetStagingMode }, { .value = ACTOR_341900_STAGING_GLUTTON_CLIP_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900RequestPlayerAction }, { .value = ACTOR_341900_PLAYER_ACTION_WEAPON_CLIP_9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900RemoveGlutton }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900RequestPlayerAction }, { .value = ACTOR_341900_PLAYER_ACTION_PLACE_AT_END }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900BroadcastActorCommand }, { .value = ACTOR_341900_ACTOR_COMMAND_END_ENTRANCE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900RequestPlayerAction }, { .value = ACTOR_341900_PLAYER_ACTION_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900SetStagingMode }, { .value = ACTOR_341900_STAGING_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip program that removes entrance models and restores the final player state.
///
/// Replaces the normal command cursor, borrowing the same live event task.
/// Fades out, clears pending actions, tears down models, restores equipment and
/// final placement, cancels the scene, changes views, then fades in and resumes
/// battle control. This 20-command array remains live through its END opcode.
static EvsCommand _gActor341900GluttonEntranceSkipScript[20] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900RequestPlayerAction }, { .value = ACTOR_341900_PLAYER_ACTION_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900SetStagingMode }, { .value = ACTOR_341900_STAGING_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900BroadcastActorCommand }, { .value = ACTOR_341900_ACTOR_COMMAND_END_ENTRANCE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900RemoveGlutton }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900RemoveDoors }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900RestorePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900FinishSkippedScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Execution priority of the entrance controller and all its display tasks.
enum { ACTOR_341900_ENTRANCE_TASK_PRIORITY = 192 };

TaskDesc gActor341900GluttonEntranceTaskDescs[ACTOR_341900_ENTRANCE_TASK_COUNT] = {
    { { { TASK_BODY_NONE, ACTOR_341900_ENTRANCE_TASK_PRIORITY } }, _actor341900EventTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_341900_ENTRANCE_TASK_PRIORITY } }, _actor341900FadeInTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_341900_ENTRANCE_TASK_PRIORITY } }, _actor341900GluttonBodyTask, { .model = &gGluttonBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_341900_ENTRANCE_TASK_PRIORITY } }, _actor341900GluttonPartTask, { .model = &gGluttonLegRight } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_341900_ENTRANCE_TASK_PRIORITY } }, _actor341900GluttonPartTask, { .model = &gGluttonLegLeft } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_341900_ENTRANCE_TASK_PRIORITY } }, _actor341900GluttonPartTask, { .model = &gGluttonActor403200Model12884 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_341900_ENTRANCE_TASK_PRIORITY } }, _actor341900GluttonPartTask, { .model = &gGluttonActor403200Model13774 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_341900_ENTRANCE_TASK_PRIORITY } }, _actor341900GluttonPartTask, { .model = &gGluttonActor403200Model18BE4 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_341900_ENTRANCE_TASK_PRIORITY } }, _actor341900DoorHalfTask, { .model = &gActor444000Model1DC9C } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_341900_ENTRANCE_TASK_PRIORITY } }, _actor341900DoorHalfTask, { .model = &gActor444000Model1E14C } },
};

/// Borrowed entrance controller used by the event script's callbacks.
///
/// Published only after work allocation succeeds. Its Task::work is the live
/// event work; callbacks may use it only while that task remains alive. Teardown
/// leaves this pointer unchanged, so a retained non-NULL value does not prove life.
static Task* _gActor341900EventTask;

/// Descriptor indices used to build the entrance scene's task tree.
enum {
    ACTOR_341900_TASK_FADE_IN             = 1,
    ACTOR_341900_TASK_GLUTTON_BODY        = 2,
    ACTOR_341900_TASK_GLUTTON_FIRST_CHILD = 3,
    ACTOR_341900_TASK_DOOR_FIRST_HALF     = 8,
    ACTOR_341900_TASK_DOOR_SECOND_HALF    = 9,
};

/// Model-task lifecycle and Glutton attachment-record indices.
enum {
    ACTOR_341900_MODEL_STATE_INITIALIZE = 0,
    ACTOR_341900_MODEL_STATE_UPDATE     = 1,
    ACTOR_341900_GLUTTON_PART_BODY      = 0,
    ACTOR_341900_GLUTTON_PART_RIGHT_LEG = 1,
    ACTOR_341900_GLUTTON_PART_LEFT_LEG  = 2,
};

/// Playback extents: the body leaves its root slot alone; each leg drives four slots.
enum {
    ACTOR_341900_GLUTTON_BODY_SLOT_COUNT = 8,
    ACTOR_341900_GLUTTON_LEG_SLOT_COUNT  = 4,
};

static s32         _actor341900TickGluttonAnimation(Task* task, u16 slotCount);
static inline void _actor341900SetGluttonPartAnimation(Task* task, u16 animationId, u16 blendFrames, u16 slotCount);
static void        _actor341900InitGluttonModel(Task* task);
static void        _actor341900UpdatePlayerAction(Task* eventTask);
static void        _actor341900UpdateStaging(Task* eventTask);

/// Advances a Glutton body or leg and reports whether all its driven slots settled.
///
/// Requires initialized model work and loaded clips. `slotCount` is eight for
/// body slots 1..7 or four for leg slots 0..3. The cached request must index
/// `_gActor341900GluttonFollowUpClips`; the scene requests clips 0..2, whose entries are -1.
/// A nonnegative entry starts a ten-frame blend to that clip at track offset
/// zero after every driven slot settles. Returns 1 for all settled, even when
/// that blend starts, and 0 otherwise; it does not update the cached request.
static s32 _actor341900TickGluttonAnimation(Task* task, u16 slotCount)
{
    enum { ACTOR_341900_GLUTTON_FOLLOW_UP_BLEND_FRAMES = 10 };
    _Actor341900GluttonModelWork* work;
    _Actor341900GluttonModelWork* followUpWork;
    u16                           slotIndex;
    u16                           allSettled;
    u16                           firstSlot;
    u16                           animationId;
    s32                           firstBlendSlot;

    firstSlot = slotCount == ACTOR_341900_GLUTTON_BODY_SLOT_COUNT;
    work      = task->work;
    // Tick the whole group before checking its boundary state.
    for (slotIndex = firstSlot; slotIndex < slotCount; slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    slotIndex  = firstSlot;
    allSettled = 1;
    for (; slotIndex < slotCount; slotIndex++) {
        if (!(work->rig.slots[slotIndex].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
            goto unsettled;
        }
    }
checkSettlement:
    if (allSettled) {
        if (_gActor341900GluttonFollowUpClips[work->playRequest.animationId] >= 0) {
            animationId    = _gActor341900GluttonFollowUpClips[work->playRequest.animationId];
            followUpWork   = task->work;
            firstBlendSlot = slotCount == ACTOR_341900_GLUTTON_BODY_SLOT_COUNT;
            // Keep the unsettled exit between selection and blending.
            goto blendFollowUp;
        unsettled:
            allSettled = 0;
            goto checkSettlement;
        blendFollowUp:
            for (slotIndex = firstBlendSlot; slotIndex < slotCount; slotIndex++) {
                animationSeekSlotWithBlend(&followUpWork->rig.anim, slotIndex, animationId, 0, ACTOR_341900_GLUTTON_FOLLOW_UP_BLEND_FRAMES);
            }
        }
        return 1;
    }
    return 0;
}

/// Selects a clip on the Glutton body's or a leg's driven animation slots.
///
/// Requires live, initialized model work and a valid local clip id. `slotCount`
/// is `ACTOR_341900_GLUTTON_BODY_SLOT_COUNT` (slots 1..7) or
/// `ACTOR_341900_GLUTTON_LEG_SLOT_COUNT` (slots 0..3). Zero
/// `blendFrames` restarts each slot at normal rate; a nonzero duration blends
/// to frame zero over that many normal-rate frames, retaining the slot rate.
static inline void _actor341900SetGluttonPartAnimation(Task* task, u16 animationId, u16 blendFrames, u16 slotCount)
{
    _Actor341900GluttonModelWork* work;
    u16                           slotIndex;

    work = task->work;
    if (blendFrames == 0) {
        for (slotIndex = slotCount == ACTOR_341900_GLUTTON_BODY_SLOT_COUNT; slotIndex < slotCount; slotIndex++) {
            work->rig.slots[slotIndex].rate = ANIMATION_RATE_ONE;
            animationResetSlot(&work->rig.anim, slotIndex, animationId);
        }
    } else {
        for (slotIndex = slotCount == ACTOR_341900_GLUTTON_BODY_SLOT_COUNT; slotIndex < slotCount; slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, animationId, 0, blendFrames);
        }
    }
}

/// Plays a local Glutton clip on the body and both legs, retaining the request.
///
/// Requires a live body with both initialized leg tasks. All five request fields
/// are read synchronously; the payload pointer is not retained. The cached
/// clip id and playback clip id narrow to unsigned 16 bits, as does the playback
/// blend duration; the other request words retain their complete values. Local
/// clip ids 0..2 exist in all three part tables; the source selector, blend
/// choice and collision choice are retained without affecting this playback.
/// Zero duration restarts the slots, otherwise it blends over that many frames.
/// The message id and second payload are ignored; no dispatch result is defined.
static void _actor341900PlayGluttonAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor341900GluttonModelWork* work;

    work                           = task->work;
    work->playRequest.source.index = request->source.index;
    work->playRequest.animationId = work->requestedClip = (u16)request->animationId;
    work->playRequest.blend                             = request->blend;
    work->playRequest.blendFrames                       = request->blendFrames;
    work->playRequest.enableWorldCollision              = request->enableWorldCollision;
    _actor341900SetGluttonPartAnimation(task, request->animationId, request->blendFrames, ACTOR_341900_GLUTTON_BODY_SLOT_COUNT);
    _actor341900SetGluttonPartAnimation(work->legRight, request->animationId, request->blendFrames, ACTOR_341900_GLUTTON_LEG_SLOT_COUNT);
    _actor341900SetGluttonPartAnimation(work->legLeft, request->animationId, request->blendFrames, ACTOR_341900_GLUTTON_LEG_SLOT_COUNT);
}

/// Samples a scene model's cached position for its three model lights.
///
/// Requires a live model, writable light/colour matrices and a valid
/// coordinate index (0 for a door half, 1 for a Glutton part). The cached XYZ
/// are passed unchanged to the world lighting query, in game units; no
/// coordinate is composed or converted here. Borrows model storage and retains
/// no position pointer.
static inline void _actor341900UpdateModelLighting(Task* task, s32 coordinateIndex)
{
    TmdObject* model;
    VECTOR     worldPosition;

    model            = task->extra.tmd;
    worldPosition.vx = task->extra.tmd->coords[coordinateIndex].workm.t[0];
    worldPosition.vy = task->extra.tmd->coords[coordinateIndex].workm.t[1];
    worldPosition.vz = task->extra.tmd->coords[coordinateIndex].workm.t[2];
    worldCoordSetModelLighting(model, &worldPosition, 0, ARRAY_SIZE(model->lightMtx->m));
}

/// Initializes and lights one sliding half of the entrance doors.
///
/// Requires a live TMD model and event parent in `spawnArg2.pointer`. State 0
/// allocates owned primary-heap `ActorLitWork`, installs placement/draw messages
/// and attaches the root to the view. Later ticks sample coordinate 0's cached
/// translation for lighting. Allocation failure kills the task but still reaches
/// the lighting query, as in the original; model/work resources must be live.
static void _actor341900DoorHalfTask(Task* task)
{
    enum { ACTOR_341900_DOOR_OT_OFFSET = 31 };
    TmdObject*    initialModel;
    ActorLitWork* lightingWork;

    // Bind task-owned matrices and parent teardown before the first light query.
    if (task->state == ACTOR_341900_MODEL_STATE_INITIALIZE) {
        initialModel = task->extra.tmd;
        lightingWork = memMalloc(sizeof(*lightingWork), false);
        task->work   = lightingWork;
        if (lightingWork == NULL) {
            taskKill(task);
        } else {
            memFillBytes(lightingWork, 0, sizeof(*lightingWork));
            lightingWork->parent            = task->spawnArg2.pointer;
            initialModel->flags             = 0;
            task->extra.tmd->coords->parent = &gGfxViewCoord;
            initialModel->lightMtx          = &lightingWork->light;
            initialModel->colorMtx          = &lightingWork->color;
            initialModel->otOffset          = ACTOR_341900_DOOR_OT_OFFSET;
            task->msgTable                  = _gActor341900DoorMessages;
            taskReparent(lightingWork->parent, task);
        }
        task->state++;
    }

    _actor341900UpdateModelLighting(task, 0);
}

/// Starts a Glutton part's driven slots at clip zero and normal playback rate.
///
/// Requires initialized model work and 0 <= firstSlot <= endSlot <= rig slots.
/// endSlot is the exclusive slot index, not a count from firstSlot. Slot indices
/// are u16; clip zero must cover every selected track. Owns no new storage.
static inline void _actor341900InitGluttonAnimationSlots(Task* task, u16 firstSlot, u16 endSlot)
{
    _Actor341900GluttonModelWork* work;
    u16                           slotIndex;

    work = task->work;
    for (slotIndex = firstSlot; slotIndex < endSlot; slotIndex++) {
        work->rig.slots[slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, slotIndex, 0);
    }
}

/// Initializes one task of the entrance scene's six-part Glutton display model.
///
/// `spawnArg1.value` is body 0, right leg 1, left leg 2, or an attached part
/// 3..5; `spawnArg2.pointer` is the live event parent for the body and the live
/// body task for its children. Requires the loaded area variant's placement
/// 0x20, which supplies texture offsets, and a live TMD model. Body/leg clip
/// tables must provide clip zero and their eight/four coordinate tracks.
/// Allocates owned primary-heap work, binds its lighting and messages, registers
/// each leg with the body and joins the parent's teardown tree. Failure kills
/// the task and returns; callers retain their original post-call behavior.
static void _actor341900InitGluttonModel(Task* task)
{
    enum { ACTOR_341900_GLUTTON_TEXTURE_PLACEMENT_ID = 0x20 };
    TmdObject*                    model;
    _Actor341900GluttonModelWork* work;
    _Actor341900GluttonModelWork* parentWork;
    _Actor341900GluttonModelWork* initializedWork;
    AreaPlacement*                texturePlacement;

    model      = task->extra.tmd;
    work       = memMalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    // Keep the allocation result separate from the initialization cursor.
    initializedWork = work;
    memFillBytes(initializedWork, 0, sizeof(*initializedWork));
    initializedWork->parent = task->spawnArg2.pointer;
    model->lightMtx         = &initializedWork->light;
    model->colorMtx         = &initializedWork->color;
    task->msgTable          = _gActor341900GluttonMessages;
    // The required area entry supplies the scene model's texture relocation.
    texturePlacement = areaGetVariant(&gGameSession->location)->placements;
    for (; texturePlacement->entryId != AREA_PLACEMENT_END; texturePlacement++) {
        if (texturePlacement->entryId == ACTOR_341900_GLUTTON_TEXTURE_PLACEMENT_ID) {
            break;
        }
    }
    tmdSetTextureOffsets(model, texturePlacement->texturePageOffset, texturePlacement->clutRowOffset);
    // Only the body and legs bind playback; other parts only need lighting.
    switch (task->spawnArg1.value) {
        case ACTOR_341900_GLUTTON_PART_BODY:
            animationInitContext(&initializedWork->rig.anim, _gActor341900GluttonBodyAnimationSets, model, initializedWork->rig.poses, initializedWork->rig.slots);
            _actor341900InitGluttonAnimationSlots(task, 1, ACTOR_341900_GLUTTON_BODY_SLOT_COUNT);
            break;
            /* Retained empty loop: removing it changes the right-leg dispatch
             * delay slot even with the slot initialization factored out. */
            do {
            } while (0);
        case ACTOR_341900_GLUTTON_PART_RIGHT_LEG:
            parentWork           = initializedWork->parent->work;
            parentWork->legRight = task;
            animationInitContext(&initializedWork->rig.anim, _gActor341900GluttonRightLegAnimationSets, model, initializedWork->rig.poses, initializedWork->rig.slots);
            _actor341900InitGluttonAnimationSlots(task, 0, ACTOR_341900_GLUTTON_LEG_SLOT_COUNT);
            break;
        case ACTOR_341900_GLUTTON_PART_LEFT_LEG:
            parentWork          = initializedWork->parent->work;
            parentWork->legLeft = task;
            animationInitContext(&initializedWork->rig.anim, _gActor341900GluttonLeftLegAnimationSets, model, initializedWork->rig.poses, initializedWork->rig.slots);
            _actor341900InitGluttonAnimationSlots(task, 0, ACTOR_341900_GLUTTON_LEG_SLOT_COUNT);
            break;
    }
    taskReparent(initializedWork->parent, task);
}

/// Attaches and lights one of the Glutton display model's five child parts.
///
/// Requires the live body in `spawnArg2.pointer`, a record index 1..5 in
/// `spawnArg1.value`, and both models' required coordinates. State 0 initializes
/// owned work and attaches coordinate 0 to the record's body coordinate, with
/// XYZ offsets in that coordinate's local units. Every tick copies the body's
/// draw flags and samples coordinate 1's cached translation for lighting.
/// That sample's storage is unproven for the three fixed parts' one-part sources.
/// The body drives leg playback; this task never advances it independently.
static void _actor341900GluttonPartTask(Task* task)
{
    _Actor341900GluttonModelWork* work = task->work;
    TmdObject*                    model;
    GfxCoord*                     rootCoord;

    if (task->state == ACTOR_341900_MODEL_STATE_INITIALIZE) {
        _actor341900InitGluttonModel(task);
        work = task->work;

        model                   = task->extra.tmd;
        rootCoord               = model->coords;
        rootCoord->parent       = &work->parent->extra.tmd->coords[_gActor341900GluttonPartAttachments[task->spawnArg1.value].parentPart];
        rootCoord->coord.t[0]   = _gActor341900GluttonPartAttachments[task->spawnArg1.value].offsetX;
        rootCoord->coord.t[1]   = _gActor341900GluttonPartAttachments[task->spawnArg1.value].offsetY;
        rootCoord->coord.t[2]   = _gActor341900GluttonPartAttachments[task->spawnArg1.value].offsetZ;
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        task->state++;
    }

    // Keep part visibility synchronized with the body.
    task->extra.tmd->flags =
        work->parent->extra.tmd->flags;

    _actor341900UpdateModelLighting(task, 1);
}

/// Starts the Glutton advance cue's child vibration and requests a long screen shake.
///
/// Requires a live body task and the co-loaded Glutton host's initialized work.
/// The vibration task borrows both script arrays until teardown and becomes a
/// child of the body task. Its allocation must succeed: the reparent call has
/// no NULL guard. The shake request is handled later by the Glutton host.
static inline void _actor341900StartGluttonAdvanceCue(Task* task)
{
    taskReparent(task, padScriptSpawn(gActor444000GluttonImpactPadCommands, gActor444000GluttonImpactPadSegments));
    actor444000GluttonSetShakeLevel(GLUTTON_SHAKE_LONG);
}

/// Drives the entrance scene's Glutton body, both legs and advance-clip cues.
///
/// State 0 initializes owned work and parents the root to the view, then returns.
/// Requires the five part tasks to have spawned and both legs to have initialized
/// before later ticks. State 1 answers advance-clip cues 0x12 and 0x18 once on
/// entry, starting child pad vibration and a long shake in the co-loaded Glutton
/// package. Every nonzero state advances body slots 1..7 and both legs' slots
/// 0..3, then samples coordinate 1's cached translation for lighting.
static void _actor341900GluttonBodyTask(Task* task)
{
    enum { ACTOR_341900_GLUTTON_ADVANCE_FIRST_CUE  = 0x12,
           ACTOR_341900_GLUTTON_ADVANCE_SECOND_CUE = 0x18 };
    _Actor341900GluttonModelWork* work;
    s32                           cue;

    work = task->work;
    switch (task->state) {
        case ACTOR_341900_MODEL_STATE_INITIALIZE:
            _actor341900InitGluttonModel(task);
            task->extra.tmd->coords->parent = &gGfxViewCoord;
            task->state++;
            return;
        case ACTOR_341900_MODEL_STATE_UPDATE:
            // Each of the two cues fires once, on the tick the pose first reaches it.
            if (work->requestedClip == ACTOR_341900_GLUTTON_CLIP_ADVANCE) {
                cue = work->rig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                if ((cue == ACTOR_341900_GLUTTON_ADVANCE_FIRST_CUE) && (work->lastCue != cue)) {
                    _actor341900StartGluttonAdvanceCue(task);
                }
                cue = work->rig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                if ((cue == ACTOR_341900_GLUTTON_ADVANCE_SECOND_CUE) && (work->lastCue != cue)) {
                    _actor341900StartGluttonAdvanceCue(task);
                }
                work->lastCue = work->rig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            work = task->work;
            break;
    }

    work = task->work;
    _actor341900TickGluttonAnimation(task, ACTOR_341900_GLUTTON_BODY_SLOT_COUNT);
    _actor341900TickGluttonAnimation(work->legRight, ACTOR_341900_GLUTTON_LEG_SLOT_COUNT);
    _actor341900TickGluttonAnimation(work->legLeft, ACTOR_341900_GLUTTON_LEG_SLOT_COUNT);

    _actor341900UpdateModelLighting(task, 1);
}

/// Performs and clears the entrance script's pending player action.
///
/// Requires live event work, player/save state and the selected animation banks.
/// `ACTOR_341900_PLAYER_ACTION_*` selects placement, movement or one complete
/// animation request; values outside the list are also cleared. Event clips 0/1
/// use the package's two-set table; weapon clips 1/9 use the equipped weapon's
/// character-specific bank. Requests reset or blend for ten whole frames and
/// disable world collision. Dispatch consumes stack requests synchronously.
/// The preliminary playing query's result is discarded and does not gate actions.
static void _actor341900UpdatePlayerAction(Task* eventTask)
{
    enum {
        ACTOR_341900_PRIMARY_CHARACTER_ID       = 1,
        ACTOR_341900_PRIMARY_WEAPON_BANK_BASE   = 1,
        ACTOR_341900_ALTERNATE_WEAPON_BANK_BASE = 0x22,
        ACTOR_341900_OPENING_WEAPON_CLIP        = 1,
        ACTOR_341900_END_WEAPON_CLIP            = 9,
        ACTOR_341900_EVENT_START_CLIP           = 0,
        ACTOR_341900_EVENT_NEXT_CLIP            = 1,
        ACTOR_341900_PLAYER_BLEND_FRAMES        = 10,
    };
    _Actor341900EventWork* work;
    _Actor341900EventWork* actionWork;
    AnimationPlayRequest   request;

/// Builds a reset request from the equipped weapon and the live character id.
///
/// `playRequest` is a stable writable lvalue, evaluated five times; `clipId`
/// is evaluated once. Captures the three function-local character/bank constants
/// and live player/save globals. Character 1 selects weapon+1, otherwise
/// weapon+0x22. Disables collision and expands as a standalone compound statement.
/// Dispatch requires character 1, weapon 0..32 and its loaded bank; backing
/// storage for the retained alternate +34 selector is unproven.
#define ACTOR_341900_BUILD_WEAPON_PLAY_REQUEST(playRequest, clipId)                                                                      \
    {                                                                                                                                    \
        s32 weaponId;                                                                                                                    \
        s32 animationBankIndex;                                                                                                          \
        weaponId                           = gPlayerStatus.weapon;                                                                       \
        animationBankIndex                 = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_341900_PRIMARY_CHARACTER_ID) \
                                                 ? weaponId + ACTOR_341900_PRIMARY_WEAPON_BANK_BASE                                      \
                                                 : weaponId + ACTOR_341900_ALTERNATE_WEAPON_BANK_BASE;                                   \
        (playRequest).source.index         = animationBankIndex;                                                                         \
        (playRequest).animationId          = (clipId);                                                                                   \
        (playRequest).blend                = ANIMATION_BLEND_RESET;                                                                      \
        (playRequest).blendFrames          = 0;                                                                                          \
        (playRequest).enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                                          \
    }

    work = eventTask->work;
    if (work->player != NULL) {
        taskMessageDispatch(work->player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0);
    }
    switch (work->playerAction) {
        case ACTOR_341900_PLAYER_ACTION_NONE:
            break;
        case ACTOR_341900_PLAYER_ACTION_PLACE_AT_START:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &_gActor341900PlayerStartPlacement, 0);
            ACTOR_341900_BUILD_WEAPON_PLAY_REQUEST(request, ACTOR_341900_OPENING_WEAPON_CLIP);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
            break;
        case ACTOR_341900_PLAYER_ACTION_WALK_TO_MARK:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_MOVE_TO, &_gActor341900PlayerWalkTarget, 0);
            break;
        case ACTOR_341900_PLAYER_ACTION_EVENT_CLIP_0:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &_gActor341900PlayerEventClipPlacement, 0);
            actionWork = eventTask->work;
            if (actionWork->player != NULL) {
                request.source.sets          = _gActor341900PlayerAnimationSets;
                request.animationId          = ACTOR_341900_EVENT_START_CLIP;
                request.blend                = ANIMATION_BLEND_RESET;
                request.blendFrames          = 0;
                request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(actionWork->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
            }
            break;
        case ACTOR_341900_PLAYER_ACTION_EVENT_CLIP_1:
            actionWork = eventTask->work;
            if (actionWork->player != NULL) {
                request.source.sets          = _gActor341900PlayerAnimationSets;
                request.animationId          = ACTOR_341900_EVENT_NEXT_CLIP;
                request.blend                = ANIMATION_BLEND_INTERPOLATE;
                request.blendFrames          = ACTOR_341900_PLAYER_BLEND_FRAMES;
                request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(actionWork->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
            }
            break;
        case ACTOR_341900_PLAYER_ACTION_WEAPON_CLIP_9:
            ACTOR_341900_BUILD_WEAPON_PLAY_REQUEST(request, ACTOR_341900_END_WEAPON_CLIP);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &_gActor341900PlayerWeaponClip9Placement, 0);
            break;
        case ACTOR_341900_PLAYER_ACTION_PLACE_AT_END:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &_gActor341900PlayerEndPlacement, 0);
            break;
    }
    work->playerAction = ACTOR_341900_PLAYER_ACTION_NONE;
#undef ACTOR_341900_BUILD_WEAPON_PLAY_REQUEST
}

/// Advances the entrance script's Glutton and door staging for one task tick.
///
/// Requires live event work and model tasks. Mode 1 places/starts Glutton clip 1,
/// waits sixteen running ticks, then moves +X by thirty game units per tick.
/// Mode 2 places/starts clip 2 and emits four sprites on running tick 61.
/// Mode 3 shuts both doors and hides the placement-0 actor, then clears itself.
/// Mode 4 initializes door placements and immediately starts sliding them apart
/// by twenty game units per tick along Z. Other modes clear to none.
/// Animation/placement dispatch is synchronous. The Glutton requests leave the
/// collision word unwritten; its handler caches that word but never uses it.
/// Only six placement components are copied; the two fourth components survive.
static void _actor341900UpdateStaging(Task* eventTask)
{
    enum {
        ACTOR_341900_STAGING_STEP_INITIALIZE        = 0,
        ACTOR_341900_STAGING_STEP_RUN               = 1,
        ACTOR_341900_STAGING_BLEND_FRAMES           = 10,
        ACTOR_341900_GLUTTON_SECOND_STAGING_CLIP    = 2,
        ACTOR_341900_GLUTTON_ADVANCE_DELAY_TICKS    = 16,
        ACTOR_341900_GLUTTON_ADVANCE_UNITS_PER_TICK = 30,
        ACTOR_341900_GLUTTON_EFFECT_DELAY_TICKS     = 60,
        ACTOR_341900_DOOR_UNITS_PER_TICK            = 20,
        // Sprite size 2048, two ticks/cell, speed 64, direction selector 4.
        ACTOR_341900_GLUTTON_EFFECT_ARGUMENT = 0x04402800,
    };
    _Actor341900EventWork* work;
    AnimationPlayRequest   advanceRequest;
    AnimationPlayRequest   effectsRequest;
    SVECTOR                effectOffset;

/// Copies XYZ position and rotation, preserving both fourth components.
///
/// Arguments are stable ActorTransform lvalues, each evaluated six times.
/// Positions use game units; rotations use 4096 units per turn. Captures no
/// locals, requires no configuration, and expands as a standalone compound statement.
#define ACTOR_341900_COPY_PLACEMENT_COMPONENTS(destination, source) \
    {                                                               \
        (destination).pos.vx = (source).pos.vx;                     \
        (destination).pos.vy = (source).pos.vy;                     \
        (destination).pos.vz = (source).pos.vz;                     \
        (destination).rot.vx = (source).rot.vx;                     \
        (destination).rot.vy = (source).rot.vy;                     \
        (destination).rot.vz = (source).rot.vz;                     \
    }

    work = eventTask->work;
    switch (work->stagingMode) {
        case ACTOR_341900_STAGING_GLUTTON_ADVANCES:
            switch (work->stagingStep) {
                case ACTOR_341900_STAGING_STEP_INITIALIZE:
                    // Preserve the original four-word request; collision is unused by this receiver.
                    advanceRequest.animationId  = ACTOR_341900_GLUTTON_CLIP_ADVANCE;
                    advanceRequest.blend        = ANIMATION_BLEND_INTERPOLATE;
                    advanceRequest.source.index = 0;
                    advanceRequest.blendFrames  = ACTOR_341900_STAGING_BLEND_FRAMES;
                    TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLAY_ANIMATION, &advanceRequest, 0);
                    ACTOR_341900_COPY_PLACEMENT_COMPONENTS(work->gluttonPlacement, _gActor341900GluttonAdvancePlacement);
                    TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLACE, &work->gluttonPlacement, 0);
                    work->stagingTicks = 0;
                    work->stagingStep++;
                    break;
                case ACTOR_341900_STAGING_STEP_RUN:
                    // Emit all four local offsets before releasing this one-shot mode.
                    if (work->stagingTicks >= ACTOR_341900_GLUTTON_ADVANCE_DELAY_TICKS) {
                        work->gluttonPlacement.pos.vx += ACTOR_341900_GLUTTON_ADVANCE_UNITS_PER_TICK;
                        TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLACE, &work->gluttonPlacement, 0);
                    } else {
                        work->stagingTicks++;
                    }
                    break;
            }
            break;
        case ACTOR_341900_STAGING_GLUTTON_CLIP_2:
            switch (work->stagingStep) {
                case ACTOR_341900_STAGING_STEP_INITIALIZE:
                    effectsRequest.animationId  = ACTOR_341900_GLUTTON_SECOND_STAGING_CLIP;
                    effectsRequest.blend        = ANIMATION_BLEND_INTERPOLATE;
                    effectsRequest.source.index = 0;
                    effectsRequest.blendFrames  = ACTOR_341900_STAGING_BLEND_FRAMES;
                    TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLAY_ANIMATION, &effectsRequest, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLACE, &_gActor341900GluttonClip2Placement, 0);
                    work->stagingTicks = 0;
                    work->stagingStep++;
                    break;
                case ACTOR_341900_STAGING_STEP_RUN:
                    work->stagingTicks++;
                    if (work->stagingTicks > ACTOR_341900_GLUTTON_EFFECT_DELAY_TICKS) {
                        effectOffset.vx = 0;
                        effectOffset.vy = -0x64;
                        effectOffset.vz = 0x1194;
                        effectSpawn(EFFECT_196, &work->glutton->extra.tmd->coords[2], ACTOR_341900_GLUTTON_EFFECT_ARGUMENT, &effectOffset);
                        effectOffset.vx = 0xC8;
                        effectOffset.vy = 0xC8;
                        effectOffset.vz = 0x1194;
                        effectSpawn(EFFECT_196, &work->glutton->extra.tmd->coords[2], ACTOR_341900_GLUTTON_EFFECT_ARGUMENT, &effectOffset);
                        effectOffset.vx = -0xC8;
                        effectOffset.vy = 0xC8;
                        effectOffset.vz = 0x1194;
                        effectSpawn(EFFECT_196, &work->glutton->extra.tmd->coords[2], ACTOR_341900_GLUTTON_EFFECT_ARGUMENT, &effectOffset);
                        effectOffset.vx = 0;
                        effectOffset.vy = 0xC8;
                        effectOffset.vz = 0x1194;
                        effectSpawn(EFFECT_196, &work->glutton->extra.tmd->coords[2], ACTOR_341900_GLUTTON_EFFECT_ARGUMENT, &effectOffset);
                        work->stagingMode = ACTOR_341900_STAGING_NONE;
                    }
                    break;
            }
            break;
        case ACTOR_341900_STAGING_DOORS_SHUT:
            TASK_MESSAGE_DISPATCH_POINTER(work->doors[0], ACTOR_MESSAGE_PLACE, &_gActor341900DoorShutPlacements[0], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->doors[1], ACTOR_MESSAGE_PLACE, &_gActor341900DoorShutPlacements[1], 0);
            taskMessageDispatch(work->placement0Actor, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            work->stagingMode = ACTOR_341900_STAGING_NONE;
            break;
        case ACTOR_341900_STAGING_DOORS_OPEN:
            switch (work->stagingStep) {
                case ACTOR_341900_STAGING_STEP_INITIALIZE:
                    ACTOR_341900_COPY_PLACEMENT_COMPONENTS(work->doorPlacements[0], _gActor341900DoorShutPlacements[0]);
                    ACTOR_341900_COPY_PLACEMENT_COMPONENTS(work->doorPlacements[1], _gActor341900DoorShutPlacements[0]);
                    work->stagingStep++;
                    /* fallthrough: opening begins on the setup tick */
                case ACTOR_341900_STAGING_STEP_RUN:
                    work->doorPlacements[0].pos.vz -= ACTOR_341900_DOOR_UNITS_PER_TICK;
                    work->doorPlacements[1].pos.vz += ACTOR_341900_DOOR_UNITS_PER_TICK;
                    TASK_MESSAGE_DISPATCH_POINTER(work->doors[0], ACTOR_MESSAGE_PLACE, &work->doorPlacements[0], 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->doors[1], ACTOR_MESSAGE_PLACE, &work->doorPlacements[1], 0);
                    break;
            }
            break;
        case ACTOR_341900_STAGING_NONE:
        default:
            work->stagingMode = ACTOR_341900_STAGING_NONE;
            break;
    }
#undef ACTOR_341900_COPY_PLACEMENT_COMPONENTS
}

/// Builds and runs the scripted Glutton entrance scene, including its skip path.
///
/// State 0 allocates/publishes owned event work, finds the current area's
/// placement-0 enemy, broadcasts command 0 and spawns the Glutton's six tasks
/// plus two door halves. Requires live player/scene tasks, that enemy, loaded
/// model/animation assets and the co-loaded Glutton package. Allocation failure
/// kills the task but retains the original subsequent dispatch and work accesses.
/// State 1 starts the normal/skip scripts and stages their music/sound selection.
/// State 2 runs player actions then staging while `GameSession::eventState` is
/// nonzero; completion sets `GAME_FLAG_11D` to 2 and requests task teardown.
/// The published task/work pointers are borrowed and valid only while it lives.
static void _actor341900EventTask(Task* task)
{
    enum {
        ACTOR_341900_EVENT_STATE_INITIALIZE   = 0,
        ACTOR_341900_EVENT_STATE_START_SCRIPT = 1,
        ACTOR_341900_EVENT_STATE_RUN          = 2,
        ACTOR_341900_ACTOR_COMMAND_STOP       = 0,
        ACTOR_341900_GLUTTON_CHILD_COUNT      = ARRAY_SIZE(_gActor341900GluttonPartAttachments) - 1,
        ACTOR_341900_SCENE_MUSIC_ENTRY        = 4,
        ACTOR_341900_SCENE_SOUND_EVENT        = 12,
        ACTOR_341900_ENTRANCE_COMPLETE_FLAG   = 2,
    };
    ActorCommand           command;
    _Actor341900EventWork* work;
    _Actor341900EventWork* spawnWork;
    u8                     areaId;
    s32                    partIndex;
    u16                    partOffset;

    switch (task->state) {
        case ACTOR_341900_EVENT_STATE_INITIALIZE:
            work       = memCalloc(sizeof(*work), false);
            task->work = work;
            if (work == NULL) {
                taskKill(task);
            } else {
                memFillBytes(work, 0U, sizeof(*work));
                work->player           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                _gActor341900EventTask = task;
                work->placement0Actor  = sceneFindEnemyByPlaceKey(
                                            gGameSession->location.area | (gGameSession->location.stage << 8))
                                            ->task;
            }
            // Suspend the live actor before constructing the scene-only display models.
            command.context.loc.stage = gGameSession->location.stage;
            areaId                    = gGameSession->location.area;
            command.command           = ACTOR_341900_ACTOR_COMMAND_STOP;
            command.context.loc.area  = areaId;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &command, ACTOR_COMMAND_MESSAGE_APPLY);
            // Build the display tasks after the live actors receive their stop command.
            spawnWork          = task->work;
            spawnWork->glutton = taskSpawnFromTable(gActor341900GluttonEntranceTaskDescs, ACTOR_341900_TASK_GLUTTON_BODY, ACTOR_341900_GLUTTON_PART_BODY, task);
            for (partOffset = 0; (u32)(partOffset & 0xFFFF) < ACTOR_341900_GLUTTON_CHILD_COUNT; partOffset++) {
                partIndex = partOffset & 0xFFFF;
                taskSpawnFromTable(gActor341900GluttonEntranceTaskDescs, partIndex + ACTOR_341900_TASK_GLUTTON_FIRST_CHILD, partIndex + 1, spawnWork->glutton);
            }
            spawnWork->doors[0]      = taskSpawnFromTable(gActor341900GluttonEntranceTaskDescs, ACTOR_341900_TASK_DOOR_FIRST_HALF, 0, task);
            spawnWork->doors[1]      = taskSpawnFromTable(gActor341900GluttonEntranceTaskDescs, ACTOR_341900_TASK_DOOR_SECOND_HALF, 0, task);
            gGameSession->flowFlags |= (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
            task->state             += 1;
            return;
        case ACTOR_341900_EVENT_STATE_START_SCRIPT:
            gStageCountdownMusicEntry                           = ACTOR_341900_SCENE_MUSIC_ENTRY;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACTOR_341900_SCENE_SOUND_EVENT;
            evsStartScriptWithSkip(_gActor341900GluttonEntranceScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor341900GluttonEntranceSkipScript);
            task->state += 1;
            return;
        case ACTOR_341900_EVENT_STATE_RUN:
            if (gGameSession->eventState == 0) {
                gameFlagSetNibble(GAME_FLAG_11D, ACTOR_341900_ENTRANCE_COMPLETE_FLAG);
                taskRequestKill(task, 0);
                return;
            }
            _actor341900UpdatePlayerAction(task);
            _actor341900UpdateStaging(task);
            return;
    }
}

#include "../../shared/screen/screen_fade_step_down.inc.c"

/// Reveals the scene by decreasing a subtractive full-screen overlay each tick.
///
/// State 0 allocates owned primary-heap work, seeds the channels to 255 and
/// also runs the first fade tick. Allocation failure kills the task; task
/// teardown releases the work. State 1 requires that live allocation.
/// The unsigned low halfword of `spawnArg1` is intensity units removed per
/// tick (zero holds indefinitely). Drawing uses the low red/green/red bytes
/// before the subtraction; stored channels narrow to signed 16 bits without
/// clamping. Negative stored red kills the task. Other states do nothing.
/// Requires the frame packet space and foreground tag used by `fadeDrawOverlay`.
static void _actor341900FadeInTask(Task* task)
{
    enum {
        ACTOR_341900_FADE_STATE_INITIALIZE = 0,
        ACTOR_341900_FADE_STATE_RAMP       = 1,
        ACTOR_341900_FADE_MAX_INTENSITY    = 255,
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case ACTOR_341900_FADE_STATE_INITIALIZE:
            allocatedFade = memCalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade         = allocatedFade;
            fade->b      = ACTOR_341900_FADE_MAX_INTENSITY;
            fade->g      = ACTOR_341900_FADE_MAX_INTENSITY;
            fade->r      = ACTOR_341900_FADE_MAX_INTENSITY;
            task->state += ACTOR_341900_FADE_STATE_RAMP - ACTOR_341900_FADE_STATE_INITIALIZE;
            /* fallthrough */
        case ACTOR_341900_FADE_STATE_RAMP:
            // Draw before stepping so the final visible value stays nonnegative.
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            _screenFadeStepDown(fade, task);
            if (fade->r < 0) {
                taskKill(task);
            }
            break;
    }
}

#include "../../shared/actor/actor_messages_draw_mode.inc.c"

#include "../../shared/actor/actor_messages_place_ypr.inc.c"

/// Ignores actor commands sent to the Glutton's display-model tasks.
///
/// Reads neither the receiver nor either payload. No dispatch result is defined.
static void _actor341900IgnoreActorCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
}

/// Broadcasts a stage/area-tagged actor command through the live scene manager.
///
/// `commandId` supplies the command's low 16 bits in the current location's
/// namespace. Dispatch consumes the stack record synchronously and discards
/// actor results; the script uses command 1 during normal and skipped teardown.
static void _actor341900BroadcastActorCommand(s16 commandId)
{
    ActorCommand command;

    command.context.loc.stage = gGameSession->location.stage;
    command.context.loc.area  = gGameSession->location.area;
    command.command           = commandId;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &command, ACTOR_COMMAND_MESSAGE_APPLY);
}

/// Sets the live Glutton body's model drawing with an `ACTOR_MESSAGE_DRAW_*` mode.
///
/// Requires the published event work and its Glutton task to remain live.
static void _actor341900SetGluttonDrawMode(s32 drawMode)
{
    _Actor341900EventWork* work = _gActor341900EventTask->work;

    taskMessageDispatch(work->glutton, ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
}

/// Sets the event player's drawing and buffers with a `PLAYER_ACTOR_MODEL_DRAW_*` mode.
///
/// Requires the published event work and its player task to remain live.
static void _actor341900SetPlayerDrawMode(s32 drawMode)
{
    _Actor341900EventWork* work = _gActor341900EventTask->work;

    taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
}

/// Removes the player's equipment once until the event restores it.
///
/// Requires live published event work and player equipment state. The latch is
/// set before removing equipment, so repeated script calls do nothing.
static void _actor341900RemovePlayerEquipment(void)
{
    _Actor341900EventWork* work = _gActor341900EventTask->work;

    if (work->playerEquipmentRemoved == false) {
        work->playerEquipmentRemoved = true;
        playerActorRemoveEquipment();
    }
}

/// Restores removed player equipment once and resumes scripted hold control.
///
/// Requires live published event work and player equipment state. The callback
/// ignores script argument words. Restore runs before clearing the latch and
/// setting hold control; repeated calls with a clear latch do nothing.
static void _actor341900RestorePlayerEquipment(void)
{
    _Actor341900EventWork* work = _gActor341900EventTask->work;

    if (work->playerEquipmentRemoved != false) {
        playerActorRestoreEquipment();
        work->playerEquipmentRemoved = false;
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    }
}

/// Tears down the event's Glutton and its child parts, then clears the task reference.
///
/// Requires live published event work. A null Glutton reference does nothing.
static void _actor341900RemoveGlutton(void)
{
    _Actor341900EventWork* work = _gActor341900EventTask->work;

    if (work->glutton != NULL) {
        taskKill(work->glutton);
        work->glutton = NULL;
    }
}

/// Tears down both event door halves in order and clears their task references.
///
/// Requires live published event work. Each null door reference is skipped.
static void _actor341900RemoveDoors(void)
{
    _Actor341900EventWork* work = _gActor341900EventTask->work;

/// Tears down a live door and clears its reference; null entries do nothing.
///
/// `eventWork` must remain writable through teardown and `doorIndex` must be
/// 0 or 1. Both arguments are evaluated repeatedly and must be stable, without
/// side effects. Expand as a standalone statement; no configuration or captures.
#define ACTOR_341900_REMOVE_DOOR(eventWork, doorIndex) \
    if ((eventWork)->doors[(doorIndex)] != NULL) {     \
        taskKill((eventWork)->doors[(doorIndex)]);     \
        (eventWork)->doors[(doorIndex)] = NULL;        \
    }

    ACTOR_341900_REMOVE_DOOR(work, 0);
    ACTOR_341900_REMOVE_DOOR(work, 1);
#undef ACTOR_341900_REMOVE_DOOR
}

/// Starts the entrance scene's subtractive fade-in at nine intensity units per tick.
///
/// A zero-argument script callback. The spawned task owns its fade work and has
/// no event parent; allocation and lifetime follow `_actor341900FadeInTask`.
static void _actor341900StartFadeIn(void)
{
    enum { ACTOR_341900_FADE_IN_UNITS_PER_TICK = 9 };
    taskSpawnFromTable(gActor341900GluttonEntranceTaskDescs, ACTOR_341900_TASK_FADE_IN, ACTOR_341900_FADE_IN_UNITS_PER_TICK, NULL);
}

/// Posts an `ACTOR_341900_PLAYER_ACTION_*` request and restarts its step counter.
///
/// Requires live published event work. Replaces any pending request; the event
/// task performs it on a later tick. The signed argument is stored as its low
/// unsigned halfword, including values outside the script's action list.
static void _actor341900RequestPlayerAction(s16 playerAction)
{
    enum { ACTOR_341900_PLAYER_ACTION_STEP_START = 0 };
    _Actor341900EventWork* work = _gActor341900EventTask->work;

    work->playerAction     = playerAction;
    work->playerActionStep = ACTOR_341900_PLAYER_ACTION_STEP_START;
}

/// Selects an `ACTOR_341900_STAGING_*` mode and restarts its step counter.
///
/// Requires live published event work. Replaces the current mode; the event task
/// runs it on later ticks. The signed argument is stored as its low unsigned
/// halfword; the staging dispatcher clears values outside its mode list.
static void _actor341900SetStagingMode(s16 stagingMode)
{
    enum { ACTOR_341900_STAGING_STEP_START = 0 };
    _Actor341900EventWork* work = _gActor341900EventTask->work;

    work->stagingMode = stagingMode;
    work->stagingStep = ACTOR_341900_STAGING_STEP_START;
}

/// Places the player at the scene's end with weapon clip 9, then cancels the scene.
///
/// Requires live published event/player work, a selected scene and the loaded
/// animation bank for the equipped weapon. Playback resets and drops world
/// collision. Both request and placement dispatches complete before cancellation.
static void _actor341900FinishSkippedScene(void)
{
    enum {
        ACTOR_341900_PRIMARY_CHARACTER_ID       = 1,
        ACTOR_341900_PRIMARY_WEAPON_BANK_BASE   = 1,
        ACTOR_341900_ALTERNATE_WEAPON_BANK_BASE = 0x22,
        ACTOR_341900_END_WEAPON_CLIP            = 9,
    };
    _Actor341900EventWork* work;
    AnimationPlayRequest   request;

/// Builds the equipped-weapon clip-9 request used at the scene's end.
///
/// `playRequest` is a writable, stable lvalue and is evaluated repeatedly.
/// Requires initialized live save/player status and the four function-local
/// ACTOR_341900 constants above. Character 1 selects bank weapon+1, otherwise
/// weapon+0x22; the complete request resets playback and disables collision.
#define ACTOR_341900_BUILD_END_WEAPON_PLAY_REQUEST(playRequest)                                         \
    {                                                                                                   \
        s32 weaponId;                                                                                   \
        s32 animationBankIndex;                                                                         \
        weaponId = gPlayerStatus.weapon;                                                                \
        animationBankIndex =                                                                            \
            (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_341900_PRIMARY_CHARACTER_ID) \
                ? weaponId + ACTOR_341900_PRIMARY_WEAPON_BANK_BASE                                      \
                : weaponId + ACTOR_341900_ALTERNATE_WEAPON_BANK_BASE;                                   \
        (playRequest).source.index         = animationBankIndex;                                        \
        (playRequest).animationId          = ACTOR_341900_END_WEAPON_CLIP;                              \
        (playRequest).blend                = ANIMATION_BLEND_RESET;                                     \
        (playRequest).blendFrames          = 0;                                                         \
        (playRequest).enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                         \
    }

    work = _gActor341900EventTask->work;
    ACTOR_341900_BUILD_END_WEAPON_PLAY_REQUEST(request);
#undef ACTOR_341900_BUILD_END_WEAPON_PLAY_REQUEST
    TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_PLAY, &request, 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &_gActor341900PlayerEndPlacement, 0);
    cdCmdCancelScene();
}

/// Stages deferred audio start for the scene selected by the event script.
///
/// Selection and playback buffers must survive the later replacement commit.
static void _actor341900StageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Enqueues playback of the event script's selected scene/audio session.
///
/// Requires valid selected-scene playback storage and free CD queue capacity.
static void _actor341900EnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Finishes the selected scene's stream state, then requests CD scene cancellation.
///
/// Requires a successfully selected scene. The cancellation also finishes the
/// stream state; both calls and their order are retained.
static void _actor341900FinishScene(void)
{
    streamFinishScene();
    cdCmdCancelScene();
}
