#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "actors/actor_444000.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/actor_presentation.h"
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
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/actor_messages.h"

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

/// Controller task of this overlay, published by `func_actor_341900_80162EFC`
/// and read by the sequence helpers that hang their work off its `Task::work`.
extern Task* D_actor_341900_80164208;

/// Where one task of the Glutton's display model hangs on the body's model:
/// the body part its root coordinate is made a child of, and its translation
/// from that part.
///
/// The model's tasks are spawned with the index of their record as
/// `Task::spawnArg1`. The body places itself in the view and never reads its
/// own record; the two legs hang from the body's root part with no offset.
typedef struct {
    s16 offsetX;    // Translation of the task's root part from the parent part, in the parent part's X
    s16 offsetY;    // Likewise in Y
    s16 offsetZ;    // Likewise in Z
    s16 parentPart; // Index of the body model's part coordinate the task's root part hangs from
} _Actor341900GluttonPartAttachment;
STATIC_ASSERT_SIZEOF(_Actor341900GluttonPartAttachment, 0x8);

extern _Actor341900GluttonPartAttachment D_actor_341900_80163A98[6];

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

/// Main-executable globals with no module header yet: `gPlayerStatus.weapon` is the
/// base weapon id records are numbered from, and `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` selects the
/// alternate set -- 1 means the second block, anything else the `+0x22` one.
/// Byte the other actor overlays' one-argument setters write; set to 0xC here
/// beside `gStageSceneMusicEntry`.

extern void                      actor444000GluttonSetShakeLevel(s8 arg0);
extern PadScriptCmd              D_actor_444000_80144A74[2];
extern PadScriptVibrationSegment D_actor_444000_80144A7C[2];

/// Parameter record `func_actor_341900_801628B8` sends with message 0x3F4.
extern AnimationSet* D_actor_341900_801639A4[2];
extern AnimationSet* D_actor_341900_801639AC[3];
extern AnimationSet* D_actor_341900_801639B8[3];
extern AnimationSet* D_actor_341900_801639C4[3];
/// Animation id `func_actor_341900_80161E58` hands every slot to
/// `animationSeekSlotWithBlend`, indexed by `_Actor341900GluttonModelWork::playRequest.animationId`; a
/// negative entry skips the call.
extern s16 D_actor_341900_801639D0[];
/// Shut placements of the two door halves (`_Actor341900EventWork::doors`),
/// sent to them by `ACTOR_341900_STAGING_DOORS_SHUT`; the first also seeds both
/// `doorPlacements` when `ACTOR_341900_STAGING_DOORS_OPEN` starts.
extern ActorTransform D_actor_341900_801639D8[2];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_341900_80163A38[2];
extern ActorTransform   D_actor_341900_80163A48;
extern ActorTransform   D_actor_341900_80163A60;
extern TaskMessageEntry D_actor_341900_80163A78[4];
/// Slot-3 placements and payloads sent by `func_actor_341900_801628B8`;
/// `_actor341900FinishSkippedScene` also warps slot 3 to the last one.
extern ActorTransform D_actor_341900_80163AC8;
extern ActorTransform D_actor_341900_80163AE0;
extern ActorTransform D_actor_341900_80163AF8;
extern ActorTransform D_actor_341900_80163B10;
extern ActorTransform D_actor_341900_80163B28;
/// Event scripts in the overlay's `.data`, handed to `evsStartScriptWithSkip` (which
/// forwards the first to `taskSpawn`).
extern EvsCommand D_actor_341900_80163B48[];
extern EvsCommand D_actor_341900_80163FB0[];
extern TaskDesc   D_actor_341900_80164190[];

extern EvsSceneKey D_actor_341900_80163B40;
void               func_actor_341900_80162200(Task*);
void               func_actor_341900_801625B4(Task*);
void               func_actor_341900_80162708(Task*);
void               func_actor_341900_80162EFC(Task*);
static void        _actor341900FadeInTask(Task* task);
static void        _actor341900BroadcastActorCommand(s16 commandId);
static void        _actor341900SetGluttonDrawMode(s32 drawMode);
static void        _actor341900SetPlayerDrawMode(s32 drawMode);
static void        _actor341900RemovePlayerEquipment(void);
void               func_actor_341900_80163438(void);
static void        _actor341900RemoveGlutton(void);
static void        _actor341900RemoveDoors(void);
void               func_actor_341900_80163534(void);
static void        _actor341900RequestPlayerAction(s16 playerAction);
static void        _actor341900SetStagingMode(s16 stagingMode);
static void        _actor341900FinishSkippedScene(void);
static void        _actor341900StageSceneAudioStart(void);
static void        _actor341900EnqueueScenePlayback(void);
static void        _actor341900FinishScene(void);

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

AnimationSet* D_actor_341900_801639A4[2] = {
    &_gActor341900Animation01B5C,
    &gActor444000Animation2E198,
};

AnimationSet* D_actor_341900_801639AC[3] = {
    &gActor444000Animation20BD4,
    &gActor444000Animation21014,
    &gActor444000Animation2C240,
};

AnimationSet* D_actor_341900_801639B8[3] = {
    &gActor444000Animation20C80,
    &gActor444000Animation212D8,
    &gActor444000Animation2C2CC,
};

AnimationSet* D_actor_341900_801639C4[3] = {
    &gActor444000Animation20D2C,
    &gActor444000Animation215B4,
    &gActor444000Animation2C358,
};

s16 D_actor_341900_801639D0[4] = {
    -1,
    -1,
    -1,
    0,
};

ActorTransform D_actor_341900_801639D8[2] = {
    { { -60, 150, -2650, 0 }, { 0, 0, 0, 0 } },
    { { -60, 150, -2650, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_actor_341900_80163A08[2] = {
    { { -60, 150, -5300, 0 }, { 0, 0, 0, 0 } },
    { { -60, 150, 0, 0 }, { 0, 0, 0, 0 } },
};

TaskMessageEntry D_actor_341900_80163A38[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
};

ActorTransform D_actor_341900_80163A48 = { { -5000, 0, -2450, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_341900_80163A60 = { { -3000, 0, -2450, 0 }, { 0, 1024, 0, 0 } };

TaskMessageEntry D_actor_341900_80163A78[4] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor341900IgnoreActorCommand },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor341900PlayGluttonAnimation },
};

_Actor341900GluttonPartAttachment D_actor_341900_80163A98[6] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 89, 100, 4 },
    { 0, 0, 0, 3 },
    { 0, 1660, 200, 2 },
};

ActorTransform D_actor_341900_80163AC8 = { { 500, 0, -1000, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_341900_80163AE0 = { { 3000, 0, -1000, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_341900_80163AF8 = { { 3200, 0, -2500, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_341900_80163B10 = { { 1000, 0, -1900, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_actor_341900_80163B28 = { { 4000, 0, -1900, 0 }, { 0, 3072, 0, 0 } };

EvsSceneKey D_actor_341900_80163B40 = { 4, 19, 11 };

/// Actor command broadcast by both paths that end this entrance event.
enum { ACTOR_341900_ACTOR_COMMAND_END_ENTRANCE = 1 };

EvsCommand D_actor_341900_80163B48[47] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163534 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900RequestPlayerAction }, { .value = ACTOR_341900_PLAYER_ACTION_PLACE_AT_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900SetStagingMode }, { .value = ACTOR_341900_STAGING_DOORS_SHUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_341900_80163B40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163438 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_341900_80163FB0[20] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900RequestPlayerAction }, { .value = ACTOR_341900_PLAYER_ACTION_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900SetStagingMode }, { .value = ACTOR_341900_STAGING_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341900BroadcastActorCommand }, { .value = ACTOR_341900_ACTOR_COMMAND_END_ENTRANCE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900RemoveGlutton }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341900RemoveDoors }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163438 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_341900_80164190[10] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_341900_80162EFC, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor341900FadeInTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_80162708, { .model = &gActor444000Actor403200Model10824 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_801625B4, { .model = &gActor444000GluttonLegRight } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_801625B4, { .model = &gActor444000GluttonLegLeft } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_801625B4, { .model = &gActor444000Actor403200Model12884 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_801625B4, { .model = &gActor444000Actor403200Model13774 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_801625B4, { .model = &gActor444000Actor403200Model18BE4 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_80162200, { .model = &gActor444000Model1DC9C } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_341900_80162200, { .model = &gActor444000Model1E14C } },
};

Task* D_actor_341900_80164208;

/// Playback extents: the body leaves its root slot alone; each leg drives four slots.
enum {
    ACTOR_341900_GLUTTON_BODY_SLOT_COUNT = 8,
    ACTOR_341900_GLUTTON_LEG_SLOT_COUNT  = 4,
};

static s32         func_actor_341900_80161E58(Task* arg0, u16 arg1);
static inline void _actor341900SetGluttonPartAnimation(Task* task, u16 animationId, u16 blendFrames, u16 slotCount);
static void        func_actor_341900_80162330(Task* arg0);
static void        func_actor_341900_801628B8(Task* arg0);
static void        func_actor_341900_80162AD4(Task* arg0);

/// Ticks slots `(arg1 == 8)..arg1-1` of the task's animation context (slot 0
/// is skipped for the eight-slot actor). If every one of them then has
/// `ANIMATION_SLOT_SETTLED` set, passes them the `D_actor_341900_801639D0` id and
/// returns 1; otherwise returns 0. The gotos reproduce retail's block layout.
static s32 func_actor_341900_80161E58(Task* arg0, u16 arg1)
{
    _Actor341900GluttonModelWork* work;
    _Actor341900GluttonModelWork* ctx;
    u16                           i;
    u16                           done;
    u16                           start;
    u16                           anim;
    s32                           first;

    anim  = arg1 == 8;
    start = anim;
    work  = arg0->work;
    for (i = start; i < arg1; i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    i    = start;
    done = 1;
    for (; i < arg1; i++) {
        if (!(work->rig.slots[i].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
            goto fail;
        }
    }
check:
    if (done) {
        if (D_actor_341900_801639D0[work->playRequest.animationId] >= 0) {
            anim  = D_actor_341900_801639D0[work->playRequest.animationId];
            ctx   = arg0->work;
            first = arg1 == 8;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = first; i < arg1; i++) {
                animationSeekSlotWithBlend(&ctx->rig.anim, i, anim, 0, 10);
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

/// Turns the model's world translation into the light/colour matrix pair the
/// actor draws with, allocating that pair on the first frame.
void func_actor_341900_80162200(Task* arg0)
{
    TmdObject*    extra;
    TmdObject*    mdl;
    ActorLitWork* mtx;
    VECTOR        pos;

    if (arg0->state == 0) {
        extra      = arg0->extra.tmd;
        mtx        = memMalloc(sizeof(*mtx), false);
        arg0->work = mtx;
        if (mtx == NULL) {
            taskKill(arg0);
        } else {
            memFillBytes(mtx, 0, sizeof(*mtx));
            mtx->parent                     = arg0->spawnArg2.pointer;
            extra->flags                    = 0;
            arg0->extra.tmd->coords->parent = &gGfxViewCoord;
            extra->lightMtx                 = &mtx->light;
            extra->colorMtx                 = &mtx->color;
            extra->otOffset                 = 0x1F;
            arg0->msgTable                  = D_actor_341900_80163A38;
            taskReparent(mtx->parent, arg0);
        }
        arg0->state++;
    }

    mdl    = arg0->extra.tmd;
    pos.vx = arg0->extra.tmd->coords->workm.t[0];
    pos.vy = arg0->extra.tmd->coords->workm.t[1];
    pos.vz = arg0->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(mdl, &pos, 0, 3);
}

/// Shared first tick of the actor's three parts, selected by `spawnArg1`:
/// allocates and clears the `_Actor341900GluttonModelWork` block, binds its
/// matrices to the model, applies the area's tpage/clut, sets up the part's animation
/// slots (eight for the body, four for each of the two children, which also
/// register themselves with the spawner) and reparents the spawner to it.
static void func_actor_341900_80162330(Task* arg0)
{
    TmdObject*                    extra;
    _Actor341900GluttonModelWork* work;
    _Actor341900GluttonModelWork* ctx;
    _Actor341900GluttonModelWork* w;
    AreaPlacement*                rec;
    u16                           i;

    extra      = arg0->extra.tmd;
    work       = memMalloc(sizeof(*work), false);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    w = work;
    memFillBytes(w, 0, sizeof(*w));
    w->parent       = arg0->spawnArg2.pointer;
    extra->lightMtx = &w->light;
    extra->colorMtx = &w->color;
    arg0->msgTable  = D_actor_341900_80163A78;
    rec             = (areaGetVariant(&gGameSession->location.loc))->placements;
    for (; rec->entryId != AREA_PLACEMENT_END; rec++) {
        if (rec->entryId == 0x20) {
            break;
        }
    }
    tmdSetTextureOffsets(extra, rec->texturePageOffset, rec->clutRowOffset);
    switch (arg0->spawnArg1.value) {
        case 0:
            animationInitContext(&w->rig.anim, D_actor_341900_801639AC, extra, w->rig.poses, w->rig.slots);
            ctx = arg0->work;
            for (i = 1; i < 8; i++) {
                ctx->rig.slots[i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&ctx->rig.anim, i, 0);
            }
            break;
            /* The empty loop's notes before `case 1:` make reorg predict the
             * dispatch branch taken and fill its delay slot from that arm. */
            do {
            } while (0);
        case 1:
            ((_Actor341900GluttonModelWork*)w->parent->work)->legRight = arg0;
            animationInitContext(&w->rig.anim, D_actor_341900_801639B8, extra, w->rig.poses, w->rig.slots);
            ctx = arg0->work;
            for (i = 0; i < 4; i++) {
                ctx->rig.slots[i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&ctx->rig.anim, i, 0);
            }
            break;
        case 2:
            ((_Actor341900GluttonModelWork*)w->parent->work)->legLeft = arg0;
            animationInitContext(&w->rig.anim, D_actor_341900_801639C4, extra, w->rig.poses, w->rig.slots);
            ctx = arg0->work;
            for (i = 0; i < 4; i++) {
                ctx->rig.slots[i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&ctx->rig.anim, i, 0);
            }
            break;
    }
    taskReparent(w->parent, arg0);
}

/// Attaches the actor to the bone its spawn record names, copies that record's
/// offset onto the part's coordinate, inherits the spawner's colour flag and
/// pushes the part's translation through the draw matrix.
void func_actor_341900_801625B4(Task* arg0)
{
    _Actor341900GluttonModelWork* work = arg0->work;
    TmdObject*                    extra;
    TmdObject*                    mdl;
    GfxCoord*                     coord;
    VECTOR                        pos;

    if (arg0->state == 0) {
        func_actor_341900_80162330(arg0);
        work = arg0->work;

        extra               = arg0->extra.tmd;
        coord               = extra->coords;
        coord->parent       = &work->parent->extra.tmd->coords[D_actor_341900_80163A98[arg0->spawnArg1.value].parentPart];
        coord->coord.t[0]   = D_actor_341900_80163A98[arg0->spawnArg1.value].offsetX;
        coord->coord.t[1]   = D_actor_341900_80163A98[arg0->spawnArg1.value].offsetY;
        coord->coord.t[2]   = D_actor_341900_80163A98[arg0->spawnArg1.value].offsetZ;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->state++;
    }

    arg0->extra.tmd->flags =
        work->parent->extra.tmd->flags;

    mdl    = arg0->extra.tmd;
    pos.vx = arg0->extra.tmd->coords[1].workm.t[0];
    pos.vy = arg0->extra.tmd->coords[1].workm.t[1];
    pos.vz = arg0->extra.tmd->coords[1].workm.t[2];
    worldCoordSetModelLighting(mdl, &pos, 0, 3);
}

/// Per-state body of the actor task. State 0 publishes the part's draw
/// matrix. State 1, while `ACTOR_341900_GLUTTON_CLIP_ADVANCE` is the requested
/// clip, watches the cue index of slot 2's current pose for the two cues that
/// start a pad-vibration script under the task. Every state but 0 then ticks
/// the body's and both legs' playback and pushes the translation of the model's
/// second coordinate through the draw matrix.
void func_actor_341900_80162708(Task* arg0)
{
    _Actor341900GluttonModelWork* work;
    TmdObject*                    mdl;
    VECTOR                        pos;
    s32                           cue;

    work = arg0->work;
    switch (arg0->state) {
        case 0:
            func_actor_341900_80162330(arg0);
            arg0->extra.tmd->coords->parent = &gGfxViewCoord;
            arg0->state++;
            return;
        case 1:
            // Each of the two cues fires once, on the tick the pose first reaches it.
            if (work->requestedClip == ACTOR_341900_GLUTTON_CLIP_ADVANCE) {
                cue = work->rig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                if ((cue == 0x12) && (work->lastCue != cue)) {
                    taskReparent(arg0,
                                 padScriptSpawn(D_actor_444000_80144A74, D_actor_444000_80144A7C));
                    actor444000GluttonSetShakeLevel(3);
                }
                cue = work->rig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                if ((cue == 0x18) && (work->lastCue != cue)) {
                    taskReparent(arg0,
                                 padScriptSpawn(D_actor_444000_80144A74, D_actor_444000_80144A7C));
                    actor444000GluttonSetShakeLevel(3);
                }
                work->lastCue = work->rig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            }
            work = arg0->work;
            break;
    }

    work = arg0->work;
    func_actor_341900_80161E58(arg0, 8);
    func_actor_341900_80161E58(work->legRight, 4);
    func_actor_341900_80161E58(work->legLeft, 4);

    mdl    = arg0->extra.tmd;
    pos.vx = arg0->extra.tmd->coords[1].workm.t[0];
    pos.vy = arg0->extra.tmd->coords[1].workm.t[1];
    pos.vz = arg0->extra.tmd->coords[1].workm.t[2];
    worldCoordSetModelLighting(mdl, &pos, 0, 3);
}

/// Performs the pending `_Actor341900EventWork::playerAction` on the player
/// task, after sending it `ANIMATION_MESSAGE_IS_PLAYING` and discarding the
/// answer, then clears the request. `ACTOR_341900_PLAYER_ACTION_*` lists what
/// each action does.
static void func_actor_341900_801628B8(Task* arg0)
{
    _Actor341900EventWork* work;
    _Actor341900EventWork* w;
    AnimationPlayRequest   msg;

    work = arg0->work;
    if (work->player != NULL) {
        taskMessageDispatch(work->player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0);
    }
    switch (work->playerAction) {
        case ACTOR_341900_PLAYER_ACTION_NONE:
            break;
        case ACTOR_341900_PLAYER_ACTION_PLACE_AT_START:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_341900_80163AC8, 0);
            {
                s32 weaponId;
                s32 anim;

                weaponId                 = gPlayerStatus.weapon;
                anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                msg.source.index         = anim;
                msg.animationId          = 1;
                msg.blend                = ANIMATION_BLEND_RESET;
                msg.blendFrames          = 0;
                msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &msg, 0);
            }
            break;
        case ACTOR_341900_PLAYER_ACTION_WALK_TO_MARK:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_MOVE_TO, &D_actor_341900_80163AE0, 0);
            break;
        case ACTOR_341900_PLAYER_ACTION_EVENT_CLIP_0:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_341900_80163AF8, 0);
            w = arg0->work;
            if (w->player != NULL) {
                msg.source.sets          = D_actor_341900_801639A4;
                msg.animationId          = 0;
                msg.blend                = ANIMATION_BLEND_RESET;
                msg.blendFrames          = 0;
                msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(w->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            }
            break;
        case ACTOR_341900_PLAYER_ACTION_EVENT_CLIP_1:
            w = arg0->work;
            if (w->player != NULL) {
                msg.source.sets          = D_actor_341900_801639A4;
                msg.animationId          = 1;
                msg.blend                = ANIMATION_BLEND_INTERPOLATE;
                msg.blendFrames          = 10;
                msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(w->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            }
            break;
        case ACTOR_341900_PLAYER_ACTION_WEAPON_CLIP_9: {
            s32 weaponId;
            s32 anim;

            weaponId                 = gPlayerStatus.weapon;
            anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.source.index         = anim;
            msg.animationId          = 9;
            msg.blend                = ANIMATION_BLEND_RESET;
            msg.blendFrames          = 0;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &msg, 0);
        }
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_341900_80163B10, 0);
            break;
        case ACTOR_341900_PLAYER_ACTION_PLACE_AT_END:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_341900_80163B28, 0);
            break;
    }
    work->playerAction = ACTOR_341900_PLAYER_ACTION_NONE;
}

/// Runs one tick of `_Actor341900EventWork::stagingMode`, stepping through
/// `stagingStep` and counting in `stagingTicks`. `ACTOR_341900_STAGING_*`
/// lists what each mode does to `glutton`, `doors` and `placement0Actor`.
static void func_actor_341900_80162AD4(Task* arg0)
{
    _Actor341900EventWork* work;
    AnimationPlayRequest   msg;
    AnimationPlayRequest   msg2;
    SVECTOR                ofs;

    work = arg0->work;
    switch (work->stagingMode) {
        case ACTOR_341900_STAGING_GLUTTON_ADVANCES:
            switch (work->stagingStep) {
                case 0:
                    msg.animationId  = 1;
                    msg.blend        = ANIMATION_BLEND_INTERPOLATE;
                    msg.source.index = 0;
                    msg.blendFrames  = 10;
                    TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLAY_ANIMATION, &msg, 0);
                    work->gluttonPlacement.pos.vx = D_actor_341900_80163A48.pos.vx;
                    work->gluttonPlacement.pos.vy = D_actor_341900_80163A48.pos.vy;
                    work->gluttonPlacement.pos.vz = D_actor_341900_80163A48.pos.vz;
                    work->gluttonPlacement.rot.vx = D_actor_341900_80163A48.rot.vx;
                    work->gluttonPlacement.rot.vy = D_actor_341900_80163A48.rot.vy;
                    work->gluttonPlacement.rot.vz = D_actor_341900_80163A48.rot.vz;
                    TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLACE, &work->gluttonPlacement, 0);
                    work->stagingTicks = 0;
                    work->stagingStep++;
                    break;
                case 1:
                    if (work->stagingTicks >= 0x10) {
                        work->gluttonPlacement.pos.vx += 0x1E;
                        TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLACE, &work->gluttonPlacement, 0);
                    } else {
                        work->stagingTicks++;
                    }
                    break;
            }
            break;
        case ACTOR_341900_STAGING_GLUTTON_CLIP_2:
            switch (work->stagingStep) {
                case 0:
                    msg2.animationId  = 2;
                    msg2.blend        = ANIMATION_BLEND_INTERPOLATE;
                    msg2.source.index = 0;
                    msg2.blendFrames  = 10;
                    TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLAY_ANIMATION, &msg2, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLACE, &D_actor_341900_80163A60, 0);
                    work->stagingTicks = 0;
                    work->stagingStep++;
                    break;
                case 1:
                    work->stagingTicks++;
                    if (work->stagingTicks > 0x3C) {
                        ofs.vx = 0;
                        ofs.vy = -0x64;
                        ofs.vz = 0x1194;
                        effectSpawn(EFFECT_196, &work->glutton->extra.tmd->coords[2], 0x04402800, &ofs);
                        ofs.vx = 0xC8;
                        ofs.vy = 0xC8;
                        ofs.vz = 0x1194;
                        effectSpawn(EFFECT_196, &work->glutton->extra.tmd->coords[2], 0x04402800, &ofs);
                        ofs.vx = -0xC8;
                        ofs.vy = 0xC8;
                        ofs.vz = 0x1194;
                        effectSpawn(EFFECT_196, &work->glutton->extra.tmd->coords[2], 0x04402800, &ofs);
                        ofs.vx = 0;
                        ofs.vy = 0xC8;
                        ofs.vz = 0x1194;
                        effectSpawn(EFFECT_196, &work->glutton->extra.tmd->coords[2], 0x04402800, &ofs);
                        work->stagingMode = ACTOR_341900_STAGING_NONE;
                    }
                    break;
            }
            break;
        case ACTOR_341900_STAGING_DOORS_SHUT:
            TASK_MESSAGE_DISPATCH_POINTER(work->doors[0], ACTOR_MESSAGE_PLACE, &D_actor_341900_801639D8[0], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->doors[1], ACTOR_MESSAGE_PLACE, &D_actor_341900_801639D8[1], 0);
            taskMessageDispatch(work->placement0Actor, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            work->stagingMode = ACTOR_341900_STAGING_NONE;
            break;
        case ACTOR_341900_STAGING_DOORS_OPEN:
            switch (work->stagingStep) {
                case 0:
                    work->doorPlacements[0].pos.vx = D_actor_341900_801639D8[0].pos.vx;
                    work->doorPlacements[0].pos.vy = D_actor_341900_801639D8[0].pos.vy;
                    work->doorPlacements[0].pos.vz = D_actor_341900_801639D8[0].pos.vz;
                    work->doorPlacements[0].rot.vx = D_actor_341900_801639D8[0].rot.vx;
                    work->doorPlacements[0].rot.vy = D_actor_341900_801639D8[0].rot.vy;
                    work->doorPlacements[0].rot.vz = D_actor_341900_801639D8[0].rot.vz;
                    work->doorPlacements[1].pos.vx = D_actor_341900_801639D8[0].pos.vx;
                    work->doorPlacements[1].pos.vy = D_actor_341900_801639D8[0].pos.vy;
                    work->doorPlacements[1].pos.vz = D_actor_341900_801639D8[0].pos.vz;
                    work->doorPlacements[1].rot.vx = D_actor_341900_801639D8[0].rot.vx;
                    work->doorPlacements[1].rot.vy = D_actor_341900_801639D8[0].rot.vy;
                    work->doorPlacements[1].rot.vz = D_actor_341900_801639D8[0].rot.vz;
                    work->stagingStep++;
                case 1:
                    work->doorPlacements[0].pos.vz -= 0x14;
                    work->doorPlacements[1].pos.vz += 0x14;
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
}

/// Controller task of the overlay's script sequence, the one published in
/// `D_actor_341900_80164208`. State 0 clears and publishes the work block,
/// points it at the slot-3 task and at the work object of the current session
/// id, hands that id to slot 4 as message 0x7DA, spawns the five child script
/// tasks (table entries 3..7, spawn arguments 1..5) under `glutton` and the
/// two door halves (entries 8 and 9) under the task itself, then sets the
/// two `GameSession.flowFlags` flags that suppress the bank-load spawn of the
/// ending and area-enter tasks. State 1 arms the stage-3 sound byte and spawns
/// the two blob tasks. State 2 waits for `GameSession.eventState` to clear -- it
/// sets game flag nibble 0x11D and kills the task when it does -- and
/// otherwise runs the two child dispatchers.
void func_actor_341900_80162EFC(Task* arg0)
{
    ActorCommand           request;
    _Actor341900EventWork* work;
    _Actor341900EventWork* seqWork;
    u8                     sessionIdLo;
    s32                    temp_a2;
    u16                    var_s0;

    switch (arg0->state) {
        case 0:
            work       = memCalloc(sizeof(*work), false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(work, 0U, sizeof(*work));
                work->player            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_actor_341900_80164208 = arg0;
                work->placement0Actor   = sceneFindEnemyByPlaceKey(
                                            gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8))
                                            ->task;
            }
            request.context.loc.stage = gGameSession->location.loc.stage;
            sessionIdLo               = gGameSession->location.loc.area;
            request.command           = 0;
            request.context.loc.area  = sessionIdLo;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &request, ACTOR_COMMAND_MESSAGE_APPLY);
            seqWork          = arg0->work;
            seqWork->glutton = taskSpawnFromTable(D_actor_341900_80164190, 2, 0, arg0);
            for (var_s0 = 0; (u32)(var_s0 & 0xFFFF) < 5U; var_s0++) {
                temp_a2 = var_s0 & 0xFFFF;
                taskSpawnFromTable(D_actor_341900_80164190, temp_a2 + 3, temp_a2 + 1, seqWork->glutton);
            }
            seqWork->doors[0]        = taskSpawnFromTable(D_actor_341900_80164190, 8, 0, arg0);
            seqWork->doors[1]        = taskSpawnFromTable(D_actor_341900_80164190, 9, 0, arg0);
            gGameSession->flowFlags |= (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
            arg0->state             += 1;
            return;
        case 1:
            gStageSceneMusicEntry                               = 4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0xC;
            evsStartScriptWithSkip(D_actor_341900_80163B48, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_341900_80163FB0);
            arg0->state += 1;
            return;
        case 2:
            if (gGameSession->eventState == 0) {
                gameFlagSetNibble(GAME_FLAG_11D, 2);
                taskRequestKill(arg0, 0);
                return;
            }
            func_actor_341900_801628B8(arg0);
            func_actor_341900_80162AD4(arg0);
            return;
    }
}

#include "../../shared/screen_fade_step_down.inc.c"

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

#include "../../shared/actor_messages_draw_mode.inc.c"

#include "../../shared/actor_messages_place_ypr.inc.c"

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

    command.context.loc.stage = gGameSession->location.loc.stage;
    command.context.loc.area  = gGameSession->location.loc.area;
    command.command           = commandId;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &command, ACTOR_COMMAND_MESSAGE_APPLY);
}

/// Sets the live Glutton body's model drawing with an `ACTOR_MESSAGE_DRAW_*` mode.
///
/// Requires the published event work and its Glutton task to remain live.
static void _actor341900SetGluttonDrawMode(s32 drawMode)
{
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

    taskMessageDispatch(work->glutton, ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
}

/// Sets the event player's drawing and buffers with a `PLAYER_ACTOR_MODEL_DRAW_*` mode.
///
/// Requires the published event work and its player task to remain live.
static void _actor341900SetPlayerDrawMode(s32 drawMode)
{
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

    taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
}

/// Removes the player's equipment once until the event restores it.
///
/// Requires live published event work and player equipment state. The latch is
/// set before removing equipment, so repeated script calls do nothing.
static void _actor341900RemovePlayerEquipment(void)
{
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

    if (work->playerEquipmentRemoved == false) {
        work->playerEquipmentRemoved = true;
        playerActorRemoveEquipment();
    }
}

void func_actor_341900_80163438(void)
{
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

    if (work->playerEquipmentRemoved != 0) {
        playerActorRestoreEquipment();
        work->playerEquipmentRemoved = 0;
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    }
}

/// Tears down the event's Glutton and its child parts, then clears the task reference.
///
/// Requires live published event work. A null Glutton reference does nothing.
static void _actor341900RemoveGlutton(void)
{
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

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
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

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

void func_actor_341900_80163534(void)
{
    taskSpawnFromTable(D_actor_341900_80164190, 1, 9, 0);
}

/// Posts an `ACTOR_341900_PLAYER_ACTION_*` request and restarts its step counter.
///
/// Requires live published event work. Replaces any pending request; the event
/// task performs it on a later tick. The signed argument is stored as its low
/// unsigned halfword, including values outside the script's action list.
static void _actor341900RequestPlayerAction(s16 playerAction)
{
    enum { ACTOR_341900_PLAYER_ACTION_STEP_START = 0 };
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

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
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

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

    work = D_actor_341900_80164208->work;
    ACTOR_341900_BUILD_END_WEAPON_PLAY_REQUEST(request);
#undef ACTOR_341900_BUILD_END_WEAPON_PLAY_REQUEST
    TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_PLAY, &request, 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_341900_80163B28, 0);
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
