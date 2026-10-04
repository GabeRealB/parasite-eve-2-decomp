#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "actors/actor_444000.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
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

extern void                      actor444000GluttonSetShakeLevel(s32 arg0);
extern PadScriptCmd              D_80144A74[2];
extern PadScriptVibrationSegment D_80144A7C[2];

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
/// `func_actor_341900_801635A4` also warps slot 3 to the last one.
extern ActorTransform D_actor_341900_80163AC8;
extern ActorTransform D_actor_341900_80163AE0;
extern ActorTransform D_actor_341900_80163AF8;
extern ActorTransform D_actor_341900_80163B10;
extern ActorTransform D_actor_341900_80163B28;
/// Event scripts in the overlay's `.data`, handed to `func_800E8634` (which
/// forwards the first to `Task_Spawn`).
extern EvsCommand D_actor_341900_80163B48[];
extern EvsCommand D_actor_341900_80163FB0[];
extern TaskDesc   D_actor_341900_80164190[];

extern EvsSceneKey D_actor_341900_80163B40;
void               func_actor_341900_80162200(Task*);
void               func_actor_341900_801625B4(Task*);
void               func_actor_341900_80162708(Task*);
void               func_actor_341900_80162EFC(Task*);
void               func_actor_341900_80163148(Task*);
void               func_actor_341900_80163334(s16);
void               func_actor_341900_80163388(s32);
void               func_actor_341900_801633C0(s32);
void               func_actor_341900_801633F8(void);
void               func_actor_341900_80163438(void);
void               func_actor_341900_80163488(void);
void               func_actor_341900_801634D0(void);
void               func_actor_341900_80163534(void);
void               func_actor_341900_80163564(s16);
void               func_actor_341900_80163584(s16);
void               func_actor_341900_801635A4(void);
void               func_actor_341900_80163638(void);
void               func_actor_341900_80163658(void);
void               func_actor_341900_80163678(void);

s32 func_actor_341900_80161FD0(Task*, s32, AnimationPlayRequest*, s32);
s32 func_actor_341900_8016332C(Task*, s32, s32, s32);

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
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_341900_8016332C },
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_341900_80161FD0 },
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

EvsCommand D_actor_341900_80163B48[47] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163534 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = ACTOR_341900_PLAYER_ACTION_PLACE_AT_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = ACTOR_341900_STAGING_DOORS_SHUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_341900_80163B40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163638 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163658 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = ACTOR_341900_PLAYER_ACTION_WALK_TO_MARK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = ACTOR_341900_STAGING_DOORS_OPEN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_801633F8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = ACTOR_341900_PLAYER_ACTION_EVENT_CLIP_0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = ACTOR_341900_STAGING_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_801634D0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = ACTOR_341900_PLAYER_ACTION_EVENT_CLIP_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_341900_801633C0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_341900_80163388 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = ACTOR_341900_STAGING_GLUTTON_ADVANCES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_341900_801633C0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163438 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = ACTOR_341900_STAGING_GLUTTON_CLIP_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = ACTOR_341900_PLAYER_ACTION_WEAPON_CLIP_9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163488 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = ACTOR_341900_PLAYER_ACTION_PLACE_AT_END }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163334 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163678 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = ACTOR_341900_PLAYER_ACTION_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = ACTOR_341900_STAGING_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_341900_80163FB0[20] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163564 }, { .value = ACTOR_341900_PLAYER_ACTION_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163584 }, { .value = ACTOR_341900_STAGING_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_341900_80163334 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163488 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_801634D0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_80163438 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_341900_801635A4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_341900_80164190[10] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_341900_80162EFC, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_341900_80163148, { .value = 0 } },
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

static s32         func_actor_341900_80161E58(Task* arg0, u16 arg1);
static inline void Actor341900_SetAnim(Task* task, u16 anim, u16 blend, u16 n);
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

/// Points `n` slots of a task's animation context at `anim`, skipping slot 0
/// on the eight-slot actor: a zero `blend` resets each slot, otherwise
/// `animationSeekSlotWithBlend` blends into it.
static inline void Actor341900_SetAnim(Task* task, u16 anim, u16 blend, u16 n)
{
    _Actor341900GluttonModelWork* ctx;
    u16                           i;

    ctx = task->work;
    if (blend == 0) {
        for (i = n == 8; i < n; i++) {
            ctx->rig.slots[i].rate = ANIMATION_RATE_ONE;
            animationResetSlot(&ctx->rig.anim, i, anim);
        }
    } else {
        for (i = n == 8; i < n; i++) {
            animationSeekSlotWithBlend(&ctx->rig.anim, i, anim, 0, blend);
        }
    }
}

/// Records a play request in the body's work block and applies it to the body
/// and both of its legs.
///
/// `blendFrames` alone selects the transition: zero restarts the slots on the
/// clip and anything else blends into it over that many frames. `blend` is
/// stored and not consulted.
s32 func_actor_341900_80161FD0(Task* arg0, s32 arg1, AnimationPlayRequest* cmd, s32 arg3)
{
    _Actor341900GluttonModelWork* work;

    work                           = arg0->work;
    work->playRequest.source.index = cmd->source.index;
    work->playRequest.animationId = work->requestedClip = cmd->animationId;
    work->playRequest.blend                             = cmd->blend;
    work->playRequest.blendFrames                       = cmd->blendFrames;
    work->playRequest.enableWorldCollision              = cmd->enableWorldCollision;
    Actor341900_SetAnim(arg0, cmd->animationId, cmd->blendFrames, 8);
    Actor341900_SetAnim(work->legRight, cmd->animationId, cmd->blendFrames, 4);
    Actor341900_SetAnim(work->legLeft, cmd->animationId, cmd->blendFrames, 4);
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
    func_800D7A9C(mdl, &pos, 0, 3);
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
    rec             = (Gp_GetNestedAreaRec(&gGameSession->location.loc))->placements;
    for (; rec->entryId != AREA_PLACEMENT_END; rec++) {
        if (rec->entryId == 0x20) {
            break;
        }
    }
    Gp_SetTmdBytes(extra, rec->texturePageOffset, rec->clutRowOffset);
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
    func_800D7A9C(mdl, &pos, 0, 3);
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
                                 Gp_SpawnScript18(D_80144A74, D_80144A7C));
                    actor444000GluttonSetShakeLevel(3);
                }
                cue = work->rig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                if ((cue == 0x18) && (work->lastCue != cue)) {
                    taskReparent(arg0,
                                 Gp_SpawnScript18(D_80144A74, D_80144A7C));
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
    func_800D7A9C(mdl, &pos, 0, 3);
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
                        Gp_SpawnEff(EFFECT_196, &work->glutton->extra.tmd->coords[2], 0x04402800, &ofs);
                        ofs.vx = 0xC8;
                        ofs.vy = 0xC8;
                        ofs.vz = 0x1194;
                        Gp_SpawnEff(EFFECT_196, &work->glutton->extra.tmd->coords[2], 0x04402800, &ofs);
                        ofs.vx = -0xC8;
                        ofs.vy = 0xC8;
                        ofs.vz = 0x1194;
                        Gp_SpawnEff(EFFECT_196, &work->glutton->extra.tmd->coords[2], 0x04402800, &ofs);
                        ofs.vx = 0;
                        ofs.vy = 0xC8;
                        ofs.vz = 0x1194;
                        Gp_SpawnEff(EFFECT_196, &work->glutton->extra.tmd->coords[2], 0x04402800, &ofs);
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
                work->placement0Actor   = Gp_FindWorkById(
                                            gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8))
                                            ->task;
            }
            request.context.loc.stage = gGameSession->location.loc.stage;
            sessionIdLo               = gGameSession->location.loc.area;
            request.command           = 0;
            request.context.loc.area  = sessionIdLo;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &request, ACTOR_COMMAND_MESSAGE_APPLY);
            seqWork          = arg0->work;
            seqWork->glutton = Task_SpawnFromTable(D_actor_341900_80164190, 2, 0, arg0);
            for (var_s0 = 0; (u32)(var_s0 & 0xFFFF) < 5U; var_s0++) {
                temp_a2 = var_s0 & 0xFFFF;
                Task_SpawnFromTable(D_actor_341900_80164190, temp_a2 + 3, temp_a2 + 1, seqWork->glutton);
            }
            seqWork->doors[0]        = Task_SpawnFromTable(D_actor_341900_80164190, 8, 0, arg0);
            seqWork->doors[1]        = Task_SpawnFromTable(D_actor_341900_80164190, 9, 0, arg0);
            gGameSession->flowFlags |= (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
            goto next;
        case 1:
            gStageSceneMusicEntry                               = 4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0xC;
            func_800E8634(D_actor_341900_80163B48, 0, D_actor_341900_80163FB0);
        next:
            arg0->state += 1;
            return;
        case 2:
            if (gGameSession->eventState == 0) {
                GameFlag_SetNibble(GAME_FLAG_11D, 2);
                Task_RequestKill(arg0, 0);
                return;
            }
            func_actor_341900_801628B8(arg0);
            func_actor_341900_80162AD4(arg0);
            return;
    }
}

/// Fade task, entry 1 of the overlay's task table: its first tick allocates
/// the channel block and seeds every channel at 0xFF; each tick then draws the
/// full-screen fade overlay and steps the channels down by `spawnArg1`,
/// killing the task once `r` has gone negative.
void func_actor_341900_80163148(Task* arg0)
{
    ScreenFadeWork* fade;
    ScreenFadeWork* alloc;

    fade = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memCalloc(8, 0);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, GPU_BLEND_SUBTRACT);
            fade->r -= (u16)arg0->spawnArg1.value;
            fade->g -= (u16)arg0->spawnArg1.value;
            fade->b -= (u16)arg0->spawnArg1.value;
            if (fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

#include "../../shared/actor_messages_draw_mode.inc.c"

#include "../../shared/actor_messages_place_ypr.inc.c"

/// Message 0x7DB handler of the actor's second message table; ignores it.
s32 func_actor_341900_8016332C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

/// Script callback: sends message 0x7DA to the slot-4 task, tagged with the
/// current session's two id bytes and the script's selector, asking for the
/// 0x7DB reply.
void func_actor_341900_80163334(s16 arg0)
{
    ActorCommand msg;

    msg.context.loc.stage = gGameSession->location.loc.stage;
    msg.context.loc.area  = gGameSession->location.loc.area;
    msg.command           = arg0;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
}

void func_actor_341900_80163388(s32 arg0)
{
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

    taskMessageDispatch(work->glutton, ACTOR_MESSAGE_SET_MODEL_DRAW, arg0, 0);
}

void func_actor_341900_801633C0(s32 arg0)
{
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

    taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, arg0, 0);
}

void func_actor_341900_801633F8(void)
{
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

    if (work->playerEquipmentRemoved == 0) {
        work->playerEquipmentRemoved = 1;
        Gp_KillPlayerEffs();
    }
}

void func_actor_341900_80163438(void)
{
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

    if (work->playerEquipmentRemoved != 0) {
        Gp_SpawnWeaponEff();
        work->playerEquipmentRemoved = 0;
        Gp_MsgPlayerWeapon(0);
    }
}

void func_actor_341900_80163488(void)
{
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

    if (work->glutton != NULL) {
        taskKill(work->glutton);
        work->glutton = NULL;
    }
}

void func_actor_341900_801634D0(void)
{
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

    if (work->doors[0] != NULL) {
        taskKill(work->doors[0]);
        work->doors[0] = NULL;
    }
    if (work->doors[1] != NULL) {
        taskKill(work->doors[1]);
        work->doors[1] = NULL;
    }
}

void func_actor_341900_80163534(void)
{
    Task_SpawnFromTable(D_actor_341900_80164190, 1, 9, 0);
}

void func_actor_341900_80163564(s16 arg0)
{
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

    work->playerAction     = arg0;
    work->playerActionStep = 0;
}

void func_actor_341900_80163584(s16 arg0)
{
    _Actor341900EventWork* work = D_actor_341900_80164208->work;

    work->stagingMode = arg0;
    work->stagingStep = 0;
}

/// Installs one animation set on slot 3 (message 0x3E8) and then warps it to
/// the overlay's fixed placement (message 0x3E9), cancelling any pending CD
/// command replacement on the way out. The set is `gPlayerStatus.weapon + 1` for the
/// alternate weapon block and `gPlayerStatus.weapon + 0x22` for the base one; its
/// `field_4` is 9, the rest of the frame is zero.
void func_actor_341900_801635A4(void)
{
    _Actor341900EventWork* work;
    AnimationPlayRequest   msg;
    s32                    weaponId;
    s32                    anim;

    work                     = D_actor_341900_80164208->work;
    weaponId                 = gPlayerStatus.weapon;
    anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.source.index         = anim;
    msg.animationId          = 9;
    msg.blend                = ANIMATION_BLEND_RESET;
    msg.blendFrames          = 0;
    msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_PLAY, &msg, 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_341900_80163B28, 0);
    CdCmd_CancelReplaceAndActivate();
}

/// Script callback: queues the replacement overlay load.
void func_actor_341900_80163638(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Script callback: queues the overlay load.
void func_actor_341900_80163658(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Script callback: restores the stream random state, then cancels the
/// pending overlay replacement and activates the loaded one.
void func_actor_341900_80163678(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}
