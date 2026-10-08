#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "actors/actor_444000.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_garbage_incinerator.h"
#include "../../shared/actor_messages.h"

/// Work block of each task that makes up the Glutton's display model: the
/// body, its two legs and the three further parts hung off the body.
///
/// All six tasks share one spawn step, which allocates the block zeroed at
/// this size and keeps it at `Task::work`. The block supplies what the task's
/// model does not own: the light and colour matrices it is drawn with, and a
/// coordinate node between the model and whatever it is attached to. The
/// model's root part hangs off `coord` and `coord` off `parentCoord`, so the
/// block places, turns and scales the whole model without touching the part
/// coordinates its animation drives. The exit callback takes `coord` out of
/// that chain again before the block is freed.
///
/// Only the body is placed and scaled by its own members: a placement message
/// stores the translation and `rotation`, a command or the event's staging
/// sets `scale`, and the body rebuilds `coord` from them every tick. A leg or
/// part keeps its `coord` at the identity scaled by the body's `scale`.
typedef struct {
    ActorAnimRig8 rig;              // Playback over the task's model: the body drives slots 1 to 7 and a leg slots 0 to 3; the other parts never bind it
    MATRIX        light;            // Light matrix the model's `TmdObject::lightMtx` points at
    MATRIX        color;            // Colour matrix the model's `TmdObject::colorMtx` points at
    GfxCoord      coord;            // Node the model's root part hangs from. Body: the placed translation, with `rotation` composed and each column scaled by `scale`; leg or part: the identity scaled by the body's `scale`
    VECTOR        scale;            // Factor each column of `coord`'s rotation is multiplied by, X/Y/Z, `ONE` being 1.0. Read on the body only, by itself and by its legs and parts
    s32           rotation[3];      // X/Y/Z Euler angles of the last placement, 4096 units per turn, composed Y, then X, then Z. Read on the body only
    byte          unknown_280[0x8]; // Zeroed allocation bytes; no access established and role unproven
    s32           followUpIndex;    // Entry of the package's follow-up clip table consulted once every driven slot has settled. Nothing writes it, so it stays 0
    byte          unknown_28C[0xC]; // Zeroed allocation bytes; no access established and role unproven
    Task*         parent;           // Task this one was spawned for and made a child of: the event task for the body, the body for a leg or part
    Task*         legRight;         // Body only: the right leg's task, whose playback the body ticks after its own. The leg stores itself here as it spawns
    Task*         legLeft;          // Body only: the left leg's task, likewise
    GfxCoord*     parentCoord;      // Borrowed node `coord` hangs from: the view for the body, the body model's root part for a leg, and the part of the body model its table entry names for the rest
    byte          unknown_2A8[0x2]; // Zeroed allocation bytes; no access established and role unproven
    u16           lastCommand;      // `ActorCommand::command` of the last command message the task took. Nothing reads it back
} _Actor342000GluttonModelWork;
STATIC_ASSERT_SIZEOF(_Actor342000GluttonModelWork, 0x2AC);

/// `_Actor342000EventWork::playerAction`: the one-shot request the event script
/// hands the player.
///
/// The "event" clips are those of the package's own animation sets; the
/// "weapon" clip is clip 1 of the bank the equipped weapon selects.
enum {
    ACTOR_342000_PLAYER_ACTION_NONE                = 0, // Nothing pending
    ACTOR_342000_PLAYER_ACTION_PLACE_AT_START      = 1, // Locks the attachments for the event and places the player at the opening spot
    ACTOR_342000_PLAYER_ACTION_WALK_TO_MARK        = 2, // Walks the player to the mark, waits for the walk and eleven more ticks, then plays event clip 0
    ACTOR_342000_PLAYER_ACTION_PLACE_AT_MARK       = 3, // Places the player at the mark and plays event clip 0
    ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_1        = 4, // Blends into event clip 1 over ten frames
    ACTOR_342000_PLAYER_ACTION_WEAPON_CLIP         = 5, // Cuts to the weapon clip
    ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_2        = 6, // Blends into event clip 2 over ten frames
    ACTOR_342000_PLAYER_ACTION_WEAPON_CLIP_BLENDED = 7, // Blends into the weapon clip over ten frames
    ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_3        = 8, // Cuts to event clip 3 and sounds the alert if it has not sounded yet
};

/// `_Actor342000EventWork::stagingMode`: what the event does with the Glutton,
/// the doors and the view.
///
/// The modes marked one-shot clear themselves on the tick that runs them; the
/// others repeat every tick until another mode replaces them.
enum {
    ACTOR_342000_STAGING_NONE          = 0, // Nothing to do
    ACTOR_342000_STAGING_GLUTTON_SINKS = 1, // Shows the Glutton and the doors at their first placements; the doors slide, the Glutton sinks and narrows, and after 60 ticks it and its legs change clip
    ACTOR_342000_STAGING_GLUTTON_ALONE = 2, // Hides the doors and moves the Glutton to its second placement, where it keeps narrowing
    ACTOR_342000_STAGING_DOORS_SHAKE   = 3, // Shows the doors at their second placements; they slide, shaking along Z, while the Glutton narrows
    ACTOR_342000_STAGING_BLEND_ON      = 4, // One-shot: selects view 8 and starts the framebuffer blend
    ACTOR_342000_STAGING_BLEND_OFF     = 5, // One-shot: ends the framebuffer blend
    ACTOR_342000_STAGING_DOORS_CLOSING = 6, // Starts the closing loop and slides the doors, drawn except in view 15
    ACTOR_342000_STAGING_RESTORE_VIEW  = 7, // One-shot: restores `savedView`
    ACTOR_342000_STAGING_DOORS_MEET    = 8, // Shows the doors at their third placements and slides them until they meet, then rumbles the controller and stops
    ACTOR_342000_STAGING_DOORS_SHUT    = 9, // One-shot: ends the closing loop, sounds the doors shutting and rumbles the controller
};

/// Work block of the package's event task: the scene in which the garbage
/// incinerator's two door halves slide shut on the gap the Glutton stands in.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. The event script cannot be handed the task, so its
/// callbacks reach the block through the task pointer that state publishes.
///
/// The script drives two small machines here by leaving a request in each:
/// `playerAction` for the player, and `stagingMode` for the Glutton's display
/// model, the two door halves and the view. The task runs both every tick
/// while the script plays and through the timed close that follows it.
typedef struct {
    ActorTransform doorPlacements[2]; // Placement last sent to each door half; reloaded per shot, then stepped five units a tick toward the other half
    ActorTransform gluttonPlacement;  // Placement sent to the Glutton while it sinks, three units a tick down Y
    Task*          player;            // The player task, the receiver of the event's scripted-control messages
    Task*          firstPlacedActor;  // Task of the area's enemy with placement index 0, looked up as the block is set up. Nothing reads it back; role unproven
    Task*          glutton;           // Body of the Glutton's display model, parent of its part tasks; `NULL` before it is spawned and once the script has removed it
    Task*          gluttonLegRight;   // The Glutton's right leg, animated beside the body
    Task*          gluttonLegLeft;    // The Glutton's left leg, animated beside the body
    Task*          doors[2];          // The two door halves, which slide along X until they meet: 0 from the low side, 1 from the high side. `NULL` once killed
    Task*          framebufferBlend;  // Framebuffer-blend effect task while one runs, else `NULL`
    u16            playerAction;      // Pending player request, cleared once performed (`ACTOR_342000_PLAYER_ACTION_NONE`, else one of `ACTOR_342000_PLAYER_ACTION_*`)
    u16            playerActionStep;  // Step within `ACTOR_342000_PLAYER_ACTION_WALK_TO_MARK` (0 send the walk, 1 wait for it to end, 2 count the delay)
    u16            playerActionTicks; // Ticks counted since that walk ended
    byte           unknown_6E[2];     // Zeroed allocation bytes; no access established and role unproven
    u16            stagingMode;       // Current staging mode (`ACTOR_342000_STAGING_NONE`, else one of `ACTOR_342000_STAGING_*`)
    u16            stagingStep;       // Step within the mode (0 set the shot up, 1 run it)
    u16            stagingTicks;      // Ticks `ACTOR_342000_STAGING_GLUTTON_SINKS` has run, up to the clip change
    byte           unknown_76[2];     // Zeroed allocation bytes; no access established and role unproven
    s16            savedView;         // Session view slot captured just before the timed close cuts to view 0x21, for `ACTOR_342000_STAGING_RESTORE_VIEW`
    u16            alertPlayed;       // Set once the alert has sounded, so the script and its skip path sound it once between them (0/1)
    u16            combatReset;       // Set once the scene's battle state has been wound down, which the script and its skip path each ask for (0/1)
    u16            doorSoundPlaying;  // Set while the doors' closing loop is sounding, so an enemy cull zone coming up can stop it once (0/1)
} _Actor342000EventWork;
STATIC_ASSERT_SIZEOF(_Actor342000EventWork, 0x80);

/// The task owning the `_Actor342000EventWork` block, published by
/// `_actor342000IncineratorEventTask`.
extern Task* D_actor_342000_80165070;

/// Message 0x7D4's static payload, handed to `taskMessageDispatch` by the actor's
/// spawn tick. The same record the handler takes.
extern ActorTransform D_actor_342000_801648B8;

/// Fixed placement `_actor342000RestorePlayerAfterSkip` warps the player to, sent as
/// message 0x3E9 and again as 0x3F2 by `_actor342000UpdatePlayerAction`.
extern ActorTransform D_actor_342000_80164948;

// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_342000_801648A8[2];

/// Animation-id bank `_Actor342000GluttonModelWork::followUpIndex` indexes; a negative entry
/// means the bank is empty and the slots are left alone.
extern s16 D_actor_342000_80164810[];

/// Per-`spawnArg1` translation seeds for the child model's part coordinate.
extern SVECTOR D_actor_342000_80164900[];

static void _actor342000IncineratorDoorTask(Task* task);
static void _actor342000GluttonPartTask(Task* task);
static void _actor342000GluttonBodyTask(Task* task);
static void _actor342000IncineratorEventTask(Task* task);
static void _actor342000FadeInTask(Task* task);
static void _actor342000PlaceGluttonModel(Task* task, s32 messageId, const ActorTransform* transform, s32 unusedArgument);
static void _actor342000ApplyGluttonModelCommand(Task* task, s32 messageId, const ActorCommand* request, const VECTOR* scaleFactors);
static void _actor342000EnterIncineratorRoom(void);
static void _actor342000RemoveGluttonModel(void);
static void _actor342000KillDoors(void);
static void _actor342000PlayAlertOnce(void);
static void _actor342000RequestPlayerAction(s16 playerAction);
static void _actor342000SetStagingMode(s16 stagingMode);
static void _actor342000EndBattleOnce(void);
static void _actor342000SetPlayerDrawMode(s32 drawMode);
static void _actor342000RestorePlayerAfterSkip(void);
static void _actor342000StageSceneAudioStart(void);
static void _actor342000EnqueueScenePlayback(void);
static void _actor342000FinishScene(void);

static AnimationPackedPose _gActor342000Animation029A0Bank1[6] = {
#include "assets/actor_342000_animation_029A0_bank1.inc"
};

static AnimationPackedRotation _gActor342000Animation029A0Bank4[46] = {
#include "assets/actor_342000_animation_029A0_bank4.inc"
};

static AnimationRecord _gActor342000Animation029A0Records[109] = {
#include "assets/actor_342000_animation_029A0_records.inc"
};

static u16 _gActor342000Animation029A0Indices[20] = {
#include "assets/actor_342000_animation_029A0_indices.inc"
};

static AnimationSet _gActor342000Animation029A0 = {
    _gActor342000Animation029A0Records,
    _gActor342000Animation029A0Indices,
    { NULL, _gActor342000Animation029A0Bank1, NULL, NULL, _gActor342000Animation029A0Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_342000_801647E8[4] = {
    &_gActor342000Animation029A0,
    &gActor444000Animation2E548,
    &gActor444000Animation2EA80,
    &gActor444000Animation2EE14,
};

AnimationSet* D_actor_342000_801647F8[2] = {
    &gActor444000Animation20BD4,
    &gActor444000Animation25BE0,
};

AnimationSet* D_actor_342000_80164800[2] = {
    &gActor444000Animation20C80,
    &gActor444000Animation25F14,
};

AnimationSet* D_actor_342000_80164808[2] = {
    &gActor444000Animation20D2C,
    &gActor444000Animation26240,
};

s16 D_actor_342000_80164810[4] = {
    -1,
    -1,
    -1,
    -1,
};

ActorTransform D_actor_342000_80164818[2] = {
    { { 0x2CEC, 4500, -0x4650, 0 }, { 0, 0, 0, 0 } },
    { { 0x4074, 4500, -0x4650, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_actor_342000_80164848[2] = {
    { { 0x30D4, 4500, -0x4650, 0 }, { 0, 0, 0, 0 } },
    { { 0x3C8C, 4500, -0x4650, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_actor_342000_80164878[2] = {
    { { 0x364C, 4500, -0x4650, 0 }, { 0, 0, 0, 0 } },
    { { 0x3714, 4500, -0x4650, 0 }, { 0, 0, 0, 0 } },
};

TaskMessageEntry D_actor_342000_801648A8[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
};

ActorTransform D_actor_342000_801648B8 = { { 0x36B0, 2000, -0x40D8, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_342000_801648D0 = { { 0x36B0, 2000, -0x3E80, 0 }, { 0, 2048, 0, 0 } };

TaskMessageEntry D_actor_342000_801648E8[3] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_MESSAGE_PLACE, _actor342000PlaceGluttonModel },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor342000ApplyGluttonModelCommand },
};

SVECTOR D_actor_342000_80164900[6] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 89, 100, 4 },
    { 0, 0, 0, 3 },
    { 0, 1660, 200, 2 },
};

ActorTransform D_actor_342000_80164930 = { { 0x3A98, 0, -0x5FB4, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_342000_80164948 = { { 0x3584, 0, -0x5460, 0 }, { 0, 0, 0, 0 } };

EvsSceneKey D_actor_342000_80164960 = { 4, 20, 11 };

EvsCommand D_actor_342000_80164968[51] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000RequestPlayerAction }, { .value = ACTOR_342000_PLAYER_ACTION_PLACE_AT_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000SetStagingMode }, { .value = ACTOR_342000_STAGING_GLUTTON_SINKS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_342000_80164960 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342000StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342000EnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000RequestPlayerAction }, { .value = ACTOR_342000_PLAYER_ACTION_WALK_TO_MARK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000SetStagingMode }, { .value = ACTOR_342000_STAGING_GLUTTON_ALONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000RequestPlayerAction }, { .value = ACTOR_342000_PLAYER_ACTION_PLACE_AT_MARK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000SetStagingMode }, { .value = ACTOR_342000_STAGING_DOORS_SHAKE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000RequestPlayerAction }, { .value = ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000RequestPlayerAction }, { .value = ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342000RemoveGluttonModel }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000SetStagingMode }, { .value = ACTOR_342000_STAGING_DOORS_MEET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000RequestPlayerAction }, { .value = ACTOR_342000_PLAYER_ACTION_WEAPON_CLIP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342000FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000SetStagingMode }, { .value = ACTOR_342000_STAGING_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342000KillDoors }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342000EndBattleOnce }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000RequestPlayerAction }, { .value = ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342000SetPlayerDrawMode }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342000SetPlayerDrawMode }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000RequestPlayerAction }, { .value = ACTOR_342000_PLAYER_ACTION_WEAPON_CLIP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342000EnterIncineratorRoom }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000SetStagingMode }, { .value = ACTOR_342000_STAGING_BLEND_ON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000SetStagingMode }, { .value = ACTOR_342000_STAGING_BLEND_OFF }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000RequestPlayerAction }, { .value = ACTOR_342000_PLAYER_ACTION_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = shelterB3GarbageIncineratorSetExitCollisionWalls }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_342000_80164E30[19] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342000EnterIncineratorRoom }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000RequestPlayerAction }, { .value = ACTOR_342000_PLAYER_ACTION_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor342000SetStagingMode }, { .value = ACTOR_342000_STAGING_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342000RemoveGluttonModel }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342000KillDoors }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342000RestorePlayerAfterSkip }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342000EndBattleOnce }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342000PlayAlertOnce }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_342000_80164FF8[10] = {
    { { { TASK_BODY_NONE, 192 } }, _actor342000IncineratorEventTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor342000FadeInTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor342000GluttonBodyTask, { .model = &gActor444000Actor403200Model10824 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor342000GluttonPartTask, { .model = &gActor444000GluttonLegRight } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor342000GluttonPartTask, { .model = &gActor444000GluttonLegLeft } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor342000GluttonPartTask, { .model = &gActor444000Actor403200Model12884 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor342000GluttonPartTask, { .model = &gActor444000Actor403200Model13774 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor342000GluttonPartTask, { .model = &gActor444000Actor403200Model18BE4 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor342000IncineratorDoorTask, { .model = &gActor444000Model1C814 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor342000IncineratorDoorTask, { .model = &gActor444000Model1D36C } },
};

Task* D_actor_342000_80165070;

static void _actor342000ExitGluttonModel(Task* task);

extern AnimationSet* D_actor_342000_801647F8[];

extern AnimationSet* D_actor_342000_80164800[];

extern AnimationSet* D_actor_342000_80164808[];

extern TaskMessageEntry D_actor_342000_801648E8[3];

/// Animation payload of the 0x3F4 messages sent to the slot-3 task.
extern AnimationSet* D_actor_342000_801647E8[4];

/// Placement sent as message 0x3E9 by sequence step 1.
extern ActorTransform D_actor_342000_80164930;

extern ActorTransform D_actor_342000_80164818[2];

extern ActorTransform D_actor_342000_80164848[2];

extern ActorTransform D_actor_342000_80164878[2];

extern ActorTransform D_actor_342000_801648D0;

extern PadScriptCmd D_actor_444000_80144A74[2];

extern PadScriptVibrationSegment D_actor_444000_80144A7C[2];

/// Spawn table of the event task's children: entry 2 is the script parent,
/// 3..7 its five script tasks and 8/9 the two effect actors.
extern TaskDesc D_actor_342000_80164FF8[];

extern EvsCommand D_actor_342000_80164968[];

extern EvsCommand D_actor_342000_80164E30[];

// Slot 0 is the body's placement root; the legs animate their roots too.
enum {
    ACTOR_342000_GLUTTON_BODY_SLOT_COUNT = 8,
    ACTOR_342000_GLUTTON_LEG_SLOT_COUNT  = 4,
};

// Blend duration counts whole frames; door shake counts parent-coordinate units.
enum {
    ACTOR_342000_GLUTTON_BLEND_FRAMES = 10,
    ACTOR_342000_DOOR_SHAKE_STEP      = 20,
};

static s32         _actor342000TickGluttonAnimation(Task* task, u16 slotCount);
static inline void _actor342000AttachGluttonModelCoord(Task* task, _Actor342000GluttonModelWork* work);
static void        _actor342000InitGluttonModel(Task* task);
static void        _actor342000UpdatePlayerAction(Task* eventTask);
static inline void _actor342000CopyPlacementComponents(ActorTransform* destination, const ActorTransform* source);
static inline void _actor342000SetGluttonAnimation(Task* task, u16 animationId, u16 blendFrames, u16 slotCount);
static inline s32  _actor342000StepDoorShake(s32 positionZ, s32 oddFrameOffset);
static void        _actor342000UpdateSceneStaging(Task* eventTask);
static inline void _actor342000KillDoorsInline(void);
static inline void _actor342000RequestPlayerActionInline(s16 playerAction);
static inline void _actor342000SetStagingModeInline(s16 stagingMode);
static inline void _actor342000EnterIncineratorRoomInline(void);

/// Advances the Glutton's driven tracks and blends to an optional follow-up clip once settled.
///
/// Requires initialized model work and loaded animation data. `slotCount` is
/// eight for the body (slots 1..7) or four for a leg (slots 0..3), and counts
/// array entries, including the skipped body root. Returns 1 when all driven
/// tracks have settled, even if the follow-up table entry is negative; otherwise
/// returns 0. A nonnegative entry selects a ten-frame blend to track offset zero.
/// The zero-initialized follow-up index remains zero throughout this event.
static s32 _actor342000TickGluttonAnimation(Task* task, u16 slotCount)
{
    enum { ACTOR_342000_GLUTTON_FOLLOW_UP_BLEND_FRAMES = 10 };
    _Actor342000GluttonModelWork* work;
    _Actor342000GluttonModelWork* followUpWork;
    u16                           slotIndex;
    u16                           allSettled;
    u16                           firstSlot;
    u16                           animationId;
    s32                           firstBlendSlot;

    firstSlot = slotCount == ACTOR_342000_GLUTTON_BODY_SLOT_COUNT;
    work      = task->work;
    // Complete every pose tick before testing the group's boundary state.
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
        if (D_actor_342000_80164810[work->followUpIndex] >= 0) {
            animationId    = D_actor_342000_80164810[work->followUpIndex];
            followUpWork   = task->work;
            firstBlendSlot = slotCount == ACTOR_342000_GLUTTON_BODY_SLOT_COUNT;
            // Keep the unsettled exit between selection and blending.
            goto blendFollowUp;
        unsettled:
            allSettled = 0;
            goto checkSettlement;
        blendFollowUp:
            for (slotIndex = firstBlendSlot; slotIndex < slotCount; slotIndex++) {
                animationSeekSlotWithBlend(&followUpWork->rig.anim, slotIndex, animationId, 0, ACTOR_342000_GLUTTON_FOLLOW_UP_BLEND_FRAMES);
            }
        }
        return 1;
    }
    return 0;
}

/// Initializes and lights one of the incinerator scene's sliding door halves.
///
/// State zero allocates and clears owned ActorLitWork, borrows its light/colour
/// matrices, attaches the root to the view and the task to its spawning parent.
/// Nonzero spawnArg1 adds 31 ordering tags of depth bias for the timed close.
/// The message table supplies placement and visibility; later updates sample
/// three lights at the root's cached composed translation without refreshing it.
/// The allocation-failure path kills the task but retains the state increment
/// and lighting call; it relies on deferred model teardown.
static void _actor342000IncineratorDoorTask(Task* task)
{
    enum {
        ACTOR_342000_DOOR_INITIALIZE      = 0,
        ACTOR_342000_DOOR_LIT             = 1,
        ACTOR_342000_DOOR_EXIT_DEPTH_BIAS = 31,
        ACTOR_342000_DOOR_LIGHT_COUNT     = 3,
    };
    TmdObject*    model;
    TmdObject*    lightingModel;
    ActorLitWork* work;
    VECTOR        lightPosition;

    if (task->state == ACTOR_342000_DOOR_INITIALIZE) {
        model      = task->extra.tmd;
        work       = memMalloc(sizeof(*work), false);
        task->work = work;
        if (work == NULL) {
            taskKill(task);
        } else {
            memFillBytes(work, 0, sizeof(*work));
            work->parent = task->spawnArg2.pointer;
            model->flags = 0;
            if (task->spawnArg1.value != 0) {
                model->otOffset = ACTOR_342000_DOOR_EXIT_DEPTH_BIAS;
            }
            task->extra.tmd->coords->parent = &gGfxViewCoord;
            model->colorMtx                 = &work->color;
            model->lightMtx                 = &work->light;
            task->msgTable                  = D_actor_342000_801648A8;
            taskReparent(work->parent, task);
        }
        task->state += ACTOR_342000_DOOR_LIT - ACTOR_342000_DOOR_INITIALIZE;
    }

    lightingModel    = task->extra.tmd;
    lightPosition.vx = task->extra.tmd->coords->workm.t[0];
    lightPosition.vy = task->extra.tmd->coords->workm.t[1];
    lightPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(lightingModel, &lightPosition, 0, ACTOR_342000_DOOR_LIGHT_COUNT);
}

/// Inserts an identity placement node between a Glutton model root and its parent.
///
/// `work` must be the task's installed work with a live `parentCoord` already
/// selected. The model must have a root coordinate. Both nodes are marked dirty;
/// work and the borrowed parent must outlive the attachment.
static inline void _actor342000AttachGluttonModelCoord(Task* task, _Actor342000GluttonModelWork* work)
{
    GfxCoord*                     modelCoord;
    _Actor342000GluttonModelWork* installedWork;

    modelCoord                      = &work->coord;
    installedWork                   = task->work;
    modelCoord->parent              = installedWork->parentCoord;
    task->extra.tmd->coords->parent = modelCoord;
    modelCoord->coord.t[0]          = 0;
    modelCoord->coord.t[1]          = 0;
    modelCoord->coord.t[2]          = 0;
    gfxSetRotIdentity(&work->coord.coord);
    work->coord.composeStamp              = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Resets the initialized Glutton model's selected slot range to clip zero.
///
/// Requires installed model work and its bound animation context. `firstSlot`
/// is inclusive and `endSlot` exclusive, with 0 <= firstSlot <= endSlot <= 8.
/// Callers select body [1, 8) or leg [0, 4); each slot's rate becomes 1.0 in
/// signed sixteenth-frame units before clip zero is restarted. Clip data remains borrowed.
static inline void _actor342000InitGluttonAnimationSlots(Task* task, u16 firstSlot, u16 endSlot)
{
    _Actor342000GluttonModelWork* work;
    u16                           slotIndex;

    work = task->work;
    for (slotIndex = firstSlot; slotIndex < endSlot; slotIndex++) {
        work->rig.slots[slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, slotIndex, 0);
    }
}

/// Initializes one task of the event's six-part Glutton display model.
///
/// Requires a live TMD model, loaded area variant and parent task in
/// `spawnArg2.pointer`. `spawnArg1.value` selects body 0, right leg 1, left leg 2
/// or attached part 3..5. The area must contain texture placement 0x20. Part
/// records supply a valid coordinate index in the parent's model; their XYZ
/// offsets are applied by the child update. Body and leg banks must contain
/// clip zero and all driven tracks. Allocates owned primary-heap work, installs
/// lighting/message bindings, attaches the model and registers its teardown.
/// Allocation failure kills the task and returns without attaching it.
static void _actor342000InitGluttonModel(Task* task)
{
    enum {
        ACTOR_342000_GLUTTON_PART_BODY      = 0,
        ACTOR_342000_GLUTTON_PART_RIGHT_LEG = 1,
        ACTOR_342000_GLUTTON_PART_LEFT_LEG  = 2,
        ACTOR_342000_GLUTTON_TEXTURE_PLACE  = 0x20,
    };
    TmdObject*                    model;
    _Actor342000GluttonModelWork* allocatedWork;
    _Actor342000GluttonModelWork* modelWork;
    AreaPlacement*                texturePlacement;

    model         = task->extra.tmd;
    allocatedWork = memMalloc(sizeof(*allocatedWork), false);
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(task);
        return;
    }
    modelWork = allocatedWork;
    memFillBytes(modelWork, 0, sizeof(*modelWork));
    modelWork->parent = task->spawnArg2.pointer;
    model->lightMtx   = &modelWork->light;
    model->colorMtx   = &modelWork->color;
    task->msgTable    = D_actor_342000_801648E8;
    texturePlacement  = (areaGetVariant(&gGameSession->location.loc))->placements;
    for (; texturePlacement->entryId != AREA_PLACEMENT_END; texturePlacement++) {
        if (texturePlacement->entryId == ACTOR_342000_GLUTTON_TEXTURE_PLACE) {
            break;
        }
    }
    // All six parts share the area placement's texture and CLUT offsets.
    tmdSetTextureOffsets(model, texturePlacement->texturePageOffset, texturePlacement->clutRowOffset);
    // Attach the body to the view; legs and further parts borrow body coordinates.
    switch (task->spawnArg1.value) {
        case ACTOR_342000_GLUTTON_PART_BODY:
            modelWork->parentCoord = &gGfxViewCoord;
            _actor342000AttachGluttonModelCoord(task, modelWork);
            animationInitContext(&modelWork->rig.anim, D_actor_342000_801647F8, model, modelWork->rig.poses, modelWork->rig.slots);
            _actor342000InitGluttonAnimationSlots(task, 1, ACTOR_342000_GLUTTON_BODY_SLOT_COUNT);
            break;
            // Retains the switch dispatch's delay-slot layout.
            do {
            } while (0);
        case ACTOR_342000_GLUTTON_PART_RIGHT_LEG:
            modelWork->parentCoord = modelWork->parent->extra.tmd->coords;
            _actor342000AttachGluttonModelCoord(task, modelWork);
            ((_Actor342000GluttonModelWork*)modelWork->parent->work)->legRight = task;
            animationInitContext(&modelWork->rig.anim, D_actor_342000_80164800, model, modelWork->rig.poses, modelWork->rig.slots);
            _actor342000InitGluttonAnimationSlots(task, 0, ACTOR_342000_GLUTTON_LEG_SLOT_COUNT);
            break;
            do {
            } while (0);
        case ACTOR_342000_GLUTTON_PART_LEFT_LEG:
            modelWork->parentCoord = modelWork->parent->extra.tmd->coords;
            _actor342000AttachGluttonModelCoord(task, modelWork);
            ((_Actor342000GluttonModelWork*)modelWork->parent->work)->legLeft = task;
            animationInitContext(&modelWork->rig.anim, D_actor_342000_80164808, model, modelWork->rig.poses, modelWork->rig.slots);
            _actor342000InitGluttonAnimationSlots(task, 0, ACTOR_342000_GLUTTON_LEG_SLOT_COUNT);
            break;
        default:
            modelWork->parentCoord = &modelWork->parent->extra.tmd->coords[D_actor_342000_80164900[task->spawnArg1.value].pad];
            _actor342000AttachGluttonModelCoord(task, modelWork);
            break;
    }
    // Coordinate attachment and task ownership are separate relationships.
    taskReparent(modelWork->parent, task);
    task->exitCallback = _actor342000ExitGluttonModel;
}
/// Attaches and updates a Glutton leg or one of its three further model parts.
///
/// SpawnArg1 selects attachment entry 1..5 and spawnArg2 borrows the body task.
/// Initialization requires successful model-work allocation, live parent work
/// and loaded model/area resources. It seeds the root's translation from the
/// attachment record. State one rebuilds the placement node with the body's
/// 12-fractional-bit column scales each tick. Every tick mirrors parent draw
/// flags and samples three lights at part one's cached composed translation.
/// The parent and its work must outlive this child. Initialization deliberately
/// continues after the allocator's failure return; callers rely on success.
static void _actor342000GluttonPartTask(Task* task)
{
    enum { ACTOR_342000_PART_INITIALIZE  = 0,
           ACTOR_342000_PART_UPDATE      = 1,
           ACTOR_342000_PART_LIGHT_COUNT = 3 };
    _Actor342000GluttonModelWork* work;
    VECTOR*                       parentScale;
    GfxCoord*                     rootCoord;
    TmdObject*                    model;
    VECTOR                        lightPosition;

    work = task->work;

    switch (task->state) {
        case ACTOR_342000_PART_INITIALIZE:
            _actor342000InitGluttonModel(task);
            work                    = task->work;
            rootCoord               = task->extra.tmd->coords;
            rootCoord->coord.t[0]   = D_actor_342000_80164900[task->spawnArg1.value].vx;
            rootCoord->coord.t[1]   = D_actor_342000_80164900[task->spawnArg1.value].vy;
            rootCoord->coord.t[2]   = D_actor_342000_80164900[task->spawnArg1.value].vz;
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state            += ACTOR_342000_PART_UPDATE - ACTOR_342000_PART_INITIALIZE;
            break;
        case ACTOR_342000_PART_UPDATE:
            parentScale = &((_Actor342000GluttonModelWork*)work->parent->work)->scale;
            gfxSetRotIdentity(&work->coord.coord);
            _gfxScaleMatrixColumns(&work->coord.coord, parentScale);
            work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
    task->extra.tmd->flags = work->parent->extra.tmd->flags;
    model                  = task->extra.tmd;
    lightPosition.vx       = task->extra.tmd->coords[1].workm.t[0];
    lightPosition.vy       = task->extra.tmd->coords[1].workm.t[1];
    lightPosition.vz       = task->extra.tmd->coords[1].workm.t[2];
    worldCoordSetModelLighting(model, &lightPosition, 0, ACTOR_342000_PART_LIGHT_COUNT);
}

/// Rebuilds the body's placement node from its stored Euler angles and column scales.
///
/// Requires initialized body work. Rotation uses 4096 units per turn, composed
/// Y/X/Z; scale uses twelve fractional bits. Translation remains intact and
/// the node's composed transform is invalidated.
static inline void _actor342000RebuildGluttonBodyTransform(_Actor342000GluttonModelWork* work)
{
    const s32* rotation;

    gfxSetRotIdentity(&work->coord.coord);
    rotation = work->rotation;
    gfxRotMatrixY(&work->coord.coord, rotation[1], GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixX(&work->coord.coord, rotation[0], GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&work->coord.coord, rotation[2], GRAPHICS_ROTATION_COMPOSE);
    _gfxScaleMatrixColumns(&work->coord.coord, &work->scale);
    work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Places, scales, animates and lights the event's Glutton body and both legs.
///
/// Initialization requires successful model-work allocation and loaded placement
/// resources; it sends the initial placement and enters state one. State one
/// rebuilds placement every tick from Y/X/Z rotations (4096 units per turn) and
/// 12-fractional-bit per-axis scale, then ticks body slots 1..7 and both legs
/// 0..3. Other later states tick animation without rebuilding placement. Both
/// leg tasks and their work must be initialized before that update. Lighting
/// samples three lights at body part one's cached composed translation. The
/// placement call is retained even when allocation failed and killed the task.
static void _actor342000GluttonBodyTask(Task* task)
{
    enum { ACTOR_342000_BODY_INITIALIZE  = 0,
           ACTOR_342000_BODY_UPDATE      = 1,
           ACTOR_342000_BODY_LIGHT_COUNT = 3 };
    _Actor342000GluttonModelWork* work;
    _Actor342000GluttonModelWork* animationWork;
    TmdObject*                    model;
    VECTOR                        lightPosition;

    work = task->work;

    switch (task->state) {
        case ACTOR_342000_BODY_INITIALIZE:
            _actor342000InitGluttonModel(task);
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &D_actor_342000_801648B8, 0);
            task->state += ACTOR_342000_BODY_UPDATE - ACTOR_342000_BODY_INITIALIZE;
            return;
        case ACTOR_342000_BODY_UPDATE:
            // Rebuild placement every tick; animation owns the model-part poses.
            _actor342000RebuildGluttonBodyTransform(work);
            /* fallthrough */
        default:
            animationWork = task->work;
            _actor342000TickGluttonAnimation(task, ACTOR_342000_GLUTTON_BODY_SLOT_COUNT);
            _actor342000TickGluttonAnimation(animationWork->legRight, ACTOR_342000_GLUTTON_LEG_SLOT_COUNT);
            _actor342000TickGluttonAnimation(animationWork->legLeft, ACTOR_342000_GLUTTON_LEG_SLOT_COUNT);
            model            = task->extra.tmd;
            lightPosition.vx = task->extra.tmd->coords[1].workm.t[0];
            lightPosition.vy = task->extra.tmd->coords[1].workm.t[1];
            lightPosition.vz = task->extra.tmd->coords[1].workm.t[2];
            worldCoordSetModelLighting(model, &lightPosition, 0, ACTOR_342000_BODY_LIGHT_COUNT);
    }
}

/// Advances the incinerator event's pending player placement, walk or animation action.
///
/// Requires initialized event work, a live player and the package's loaded player
/// clips. Weapon actions also require the current character's equipped-weapon
/// bank. Requests disable world collision while playing; stack animation requests
/// are consumed synchronously and their selected clip data remains borrowed.
/// The walk waits for scripted motion to end, then eleven update ticks before
/// playing clip zero. Completed actions clear the request; waiting steps return
/// without clearing it. Clip 3 shares the published event's one-shot alert latch.
/// The initial playback query's return value is discarded.
static void _actor342000UpdatePlayerAction(Task* eventTask)
{
    // Build and synchronously dispatch an event-bank clip using the caller's
    // existing request storage. The record lvalue is evaluated six times and must
    // be stable and side-effect-free; each scalar argument is evaluated once.
    // Expands to a compound statement; use only where a block is permitted.
    // The selected package animation data must remain loaded during playback.
#define ACTOR_342000_PLAY_PLAYER_EVENT_CLIP(requestRecord, clipId, transitionMode, durationFrames)                                      \
    {                                                                                                                                   \
        (requestRecord).source.sets          = D_actor_342000_801647E8;                                                                 \
        (requestRecord).animationId          = (clipId);                                                                                \
        (requestRecord).blend                = (transitionMode);                                                                        \
        (requestRecord).blendFrames          = (durationFrames);                                                                        \
        (requestRecord).enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                                       \
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(requestRecord), 0); \
    }

    enum {
        ACTOR_342000_PLAYER_WALK_SEND           = 0,
        ACTOR_342000_PLAYER_WALK_WAIT           = 1,
        ACTOR_342000_PLAYER_WALK_DELAY          = 2,
        ACTOR_342000_PLAYER_WALK_DELAY_TICKS    = 11,
        ACTOR_342000_PLAYER_BLEND_FRAMES        = 10,
        ACTOR_342000_PRIMARY_CHARACTER          = 1,
        ACTOR_342000_PRIMARY_WEAPON_BANK_BASE   = 1,
        ACTOR_342000_ALTERNATE_WEAPON_BANK_BASE = 34,
        ACTOR_342000_PLAYER_EVENT_CLIP_AT_MARK  = 0,
        ACTOR_342000_PLAYER_EVENT_CLIP_1        = 1,
        ACTOR_342000_PLAYER_WEAPON_CLIP         = 1,
        ACTOR_342000_PLAYER_EVENT_CLIP_2        = 2,
        ACTOR_342000_PLAYER_EVENT_CLIP_ALERT    = 3,
    };
    _Actor342000EventWork* work;
    _Actor342000EventWork* publishedEventWork;
    AnimationPlayRequest   animationRequest;

    work = eventTask->work;
    if (work->player != NULL) {
        taskMessageDispatch(work->player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0);
    }
    switch (work->playerAction) {
        case ACTOR_342000_PLAYER_ACTION_NONE:
            break;
        case ACTOR_342000_PLAYER_ACTION_PLACE_AT_START:
            roomEffectRequestCancelAll();
            Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_342000_80164930, 0);
            break;
        case ACTOR_342000_PLAYER_ACTION_WALK_TO_MARK:
            // Movement completion and the post-walk hold consume separate updates.
            switch (work->playerActionStep) {
                case ACTOR_342000_PLAYER_WALK_SEND:
                    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_MOVE_TO, &D_actor_342000_80164948, 0);
                    work->playerActionStep++;
                    return;
                case ACTOR_342000_PLAYER_WALK_WAIT:
                    if (taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                        work->playerActionTicks = 0;
                        work->playerActionStep++;
                    }
                    return;
                case ACTOR_342000_PLAYER_WALK_DELAY:
                    if (++work->playerActionTicks >= ACTOR_342000_PLAYER_WALK_DELAY_TICKS) {
                        work->playerAction = ACTOR_342000_PLAYER_ACTION_NONE;
                        ACTOR_342000_PLAY_PLAYER_EVENT_CLIP(animationRequest, ACTOR_342000_PLAYER_EVENT_CLIP_AT_MARK, ANIMATION_BLEND_RESET, 0);
                    }
                    return;
            }
            return;
        case ACTOR_342000_PLAYER_ACTION_PLACE_AT_MARK:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_342000_80164948, 0);
            ACTOR_342000_PLAY_PLAYER_EVENT_CLIP(animationRequest, ACTOR_342000_PLAYER_EVENT_CLIP_AT_MARK, ANIMATION_BLEND_RESET, 0);
            break;
        case ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_1:
            ACTOR_342000_PLAY_PLAYER_EVENT_CLIP(animationRequest, ACTOR_342000_PLAYER_EVENT_CLIP_1, ANIMATION_BLEND_INTERPOLATE, ACTOR_342000_PLAYER_BLEND_FRAMES);
            break;
        case ACTOR_342000_PLAYER_ACTION_WEAPON_CLIP: {
            s32 weaponId;
            s32 animationBankIndex;

            weaponId                              = gPlayerStatus.weapon;
            animationBankIndex                    = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_342000_PRIMARY_CHARACTER) ? weaponId + ACTOR_342000_PRIMARY_WEAPON_BANK_BASE : weaponId + ACTOR_342000_ALTERNATE_WEAPON_BANK_BASE;
            animationRequest.source.index         = animationBankIndex;
            animationRequest.animationId          = ACTOR_342000_PLAYER_WEAPON_CLIP;
            animationRequest.blend                = ANIMATION_BLEND_RESET;
            animationRequest.blendFrames          = 0;
            animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
            break;
        }
        case ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_2:
            ACTOR_342000_PLAY_PLAYER_EVENT_CLIP(animationRequest, ACTOR_342000_PLAYER_EVENT_CLIP_2, ANIMATION_BLEND_INTERPOLATE, ACTOR_342000_PLAYER_BLEND_FRAMES);
            break;
        case ACTOR_342000_PLAYER_ACTION_WEAPON_CLIP_BLENDED: {
            s32 weaponId;
            s32 animationBankIndex;

            weaponId                              = gPlayerStatus.weapon;
            animationBankIndex                    = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_342000_PRIMARY_CHARACTER) ? weaponId + ACTOR_342000_PRIMARY_WEAPON_BANK_BASE : weaponId + ACTOR_342000_ALTERNATE_WEAPON_BANK_BASE;
            animationRequest.source.index         = animationBankIndex;
            animationRequest.animationId          = ACTOR_342000_PLAYER_WEAPON_CLIP;
            animationRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
            animationRequest.blendFrames          = ACTOR_342000_PLAYER_BLEND_FRAMES;
            animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
            break;
        }
        case ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_3:
            ACTOR_342000_PLAY_PLAYER_EVENT_CLIP(animationRequest, ACTOR_342000_PLAYER_EVENT_CLIP_ALERT, ANIMATION_BLEND_RESET, 0);
            // Re-read the published work after the animation message has run.
            publishedEventWork = D_actor_342000_80165070->work;
            if (publishedEventWork->alertPlayed == 0) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_ALERT, 0, 0);
                publishedEventWork->alertPlayed = 1;
            }
            break;
    }
    // Waiting walk steps return above; every other request is one-shot.
    work->playerAction = ACTOR_342000_PLAYER_ACTION_NONE;
#undef ACTOR_342000_PLAY_PLAYER_EVENT_CLIP
}

/// Copies the six placement components, leaving both vectors' fourth components intact.
///
/// Both records must be live and aligned. Positions use world-coordinate units;
/// rotations use 4096 units per turn. The source is borrowed only for this call.
static inline void _actor342000CopyPlacementComponents(ActorTransform* destination, const ActorTransform* source)
{
    destination->pos.vx = source->pos.vx;
    destination->pos.vy = source->pos.vy;
    destination->pos.vz = source->pos.vz;
    destination->rot.vx = source->rot.vx;
    destination->rot.vy = source->rot.vy;
    destination->rot.vz = source->rot.vz;
}

/// Restarts or blends all driven tracks of one Glutton body or leg.
///
/// Requires initialized model work and a loaded `animationId`. `slotCount` is
/// eight for body slots 1..7 or four for leg slots 0..3. Zero `blendFrames`
/// resets the tracks and their rates; a positive whole-frame duration captures
/// their current poses and blends to track offset zero, retaining their rates.
/// Callers use zero or ten frames; durations must fit the animation timer.
static inline void _actor342000SetGluttonAnimation(Task* task, u16 animationId, u16 blendFrames, u16 slotCount)
{
    _Actor342000GluttonModelWork* work;
    u16                           slotIndex;
    u16                           firstSlot;

    firstSlot = slotCount == ACTOR_342000_GLUTTON_BODY_SLOT_COUNT;
    work      = task->work;
    if (blendFrames == 0) {
        for (slotIndex = firstSlot; slotIndex < slotCount; slotIndex++) {
            work->rig.slots[slotIndex].rate = ANIMATION_RATE_ONE;
            animationResetSlot(&work->rig.anim, slotIndex, animationId);
        }
    } else {
        for (slotIndex = firstSlot; slotIndex < slotCount; slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, animationId, 0, blendFrames);
        }
    }
}

/// Applies the door's alternating Z displacement using display-frame parity.
///
/// Inputs and result use parent-frame coordinate units. Odd frames add
/// `oddFrameOffset`; even frames subtract it. The two doors use opposite signs.
/// This is an incremental step: repeating the same parity repeats the offset.
static inline s32 _actor342000StepDoorShake(s32 positionZ, s32 oddFrameOffset)
{
    if (gDisplayState.animFrame & 1) {
        return positionZ + oddFrameOffset;
    }
    return positionZ - oddFrameOffset;
}

/// Advances the incinerator scene's Glutton, doors, framebuffer blend and view staging.
///
/// Requires initialized event work and the live tasks each staging mode uses.
/// Setup falls through to its first movement tick; repeat modes keep their mode,
/// while one-shot modes clear it. Placements use parent-coordinate units, scales
/// use twelve fractional bits, and the sink changes clips after sixty ticks.
/// The Glutton work pointer is loaded unconditionally before mode dispatch.
/// During timed closing the Glutton may already be NULL: modes 6..9 never use
/// that value, but the retained target read still occurs at low RAM address 0x1C.
/// Do not move that load into only the Glutton modes.
static void _actor342000UpdateSceneStaging(Task* eventTask)
{
    // Copies XYZ/rotation while leaving fourth components intact. Each pointer
    // is evaluated six times and must be stable, aligned and side-effect-free.
#define ACTOR_342000_COPY_STAGING_PLACEMENT(destination, source) \
    {                                                            \
        (destination)->pos.vx = (source)->pos.vx;                \
        (destination)->pos.vy = (source)->pos.vy;                \
        (destination)->pos.vz = (source)->pos.vz;                \
        (destination)->rot.vx = (source)->rot.vx;                \
        (destination)->rot.vy = (source)->rot.vy;                \
        (destination)->rot.vz = (source)->rot.vz;                \
    }
    enum {
        ACTOR_342000_STAGING_SETUP                 = 0,
        ACTOR_342000_STAGING_RUN                   = 1,
        ACTOR_342000_GLUTTON_SINK_CLIP_TICK        = 60,
        ACTOR_342000_GLUTTON_ANIM_IDLE             = 0,
        ACTOR_342000_GLUTTON_ANIM_SINK             = 1,
        ACTOR_342000_DOORS_MEETING_X               = 0x36B0,
        ACTOR_342000_BLEND_VIEW                    = 8,
        ACTOR_342000_DOORS_HIDDEN_VIEW             = 15,
        ACTOR_342000_FRAMEBUFFER_BLEND_BANK        = 1,
        ACTOR_342000_FRAMEBUFFER_BLEND_SLOT        = 0x2D,
        ACTOR_342000_FRAMEBUFFER_BLEND_DOUBLE_PASS = 16,
    };
    _Actor342000EventWork*        work;
    _Actor342000GluttonModelWork* gluttonWork;
    const ActorTransform*         initialGluttonPlacement;

    work        = eventTask->work;
    gluttonWork = work->glutton->work;
    switch (work->stagingMode) {
        case ACTOR_342000_STAGING_GLUTTON_SINKS:
            switch (work->stagingStep) {
                case ACTOR_342000_STAGING_SETUP:
                    taskMessageDispatch(work->glutton, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                    taskMessageDispatch(work->doors[0], ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                    taskMessageDispatch(work->doors[1], ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                    _actor342000CopyPlacementComponents(&work->doorPlacements[0], &D_actor_342000_80164818[0]);
                    _actor342000CopyPlacementComponents(&work->doorPlacements[1], &D_actor_342000_80164818[1]);
                    gluttonWork->scale.vx   = ONE;
                    gluttonWork->scale.vy   = ONE;
                    gluttonWork->scale.vz   = ONE;
                    initialGluttonPlacement = &D_actor_342000_801648B8;
                    ACTOR_342000_COPY_STAGING_PLACEMENT(&work->gluttonPlacement, initialGluttonPlacement);
                    work->stagingTicks = 0;
                    work->stagingStep++;
                case ACTOR_342000_STAGING_RUN:
                    if (++work->stagingTicks == ACTOR_342000_GLUTTON_SINK_CLIP_TICK) {
                        _actor342000SetGluttonAnimation(work->glutton, ACTOR_342000_GLUTTON_ANIM_SINK, ACTOR_342000_GLUTTON_BLEND_FRAMES, ACTOR_342000_GLUTTON_BODY_SLOT_COUNT);
                        _actor342000SetGluttonAnimation(work->gluttonLegRight, ACTOR_342000_GLUTTON_ANIM_SINK, ACTOR_342000_GLUTTON_BLEND_FRAMES, ACTOR_342000_GLUTTON_LEG_SLOT_COUNT);
                        _actor342000SetGluttonAnimation(work->gluttonLegLeft, ACTOR_342000_GLUTTON_ANIM_SINK, ACTOR_342000_GLUTTON_BLEND_FRAMES, ACTOR_342000_GLUTTON_LEG_SLOT_COUNT);
                    }
                    gluttonWork->scale.vx          -= 4;
                    work->doorPlacements[0].pos.vx += 5;
                    work->doorPlacements[1].pos.vx -= 5;
                    TASK_MESSAGE_DISPATCH_POINTER(work->doors[0], ACTOR_MESSAGE_PLACE, &work->doorPlacements[0], 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->doors[1], ACTOR_MESSAGE_PLACE, &work->doorPlacements[1], 0);
                    work->gluttonPlacement.pos.vy += 3;
                    TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLACE, &work->gluttonPlacement, 0);
                    break;
            }
            return;
        case ACTOR_342000_STAGING_GLUTTON_ALONE:
            switch (work->stagingStep) {
                case ACTOR_342000_STAGING_SETUP:
                    taskMessageDispatch(work->doors[0], ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE, 0);
                    taskMessageDispatch(work->doors[1], ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLACE, &D_actor_342000_801648D0, 0);
                    _actor342000SetGluttonAnimation(work->glutton, ACTOR_342000_GLUTTON_ANIM_IDLE, 0, ACTOR_342000_GLUTTON_BODY_SLOT_COUNT);
                    _actor342000SetGluttonAnimation(work->gluttonLegRight, ACTOR_342000_GLUTTON_ANIM_IDLE, 0, ACTOR_342000_GLUTTON_LEG_SLOT_COUNT);
                    _actor342000SetGluttonAnimation(work->gluttonLegLeft, ACTOR_342000_GLUTTON_ANIM_IDLE, 0, ACTOR_342000_GLUTTON_LEG_SLOT_COUNT);
                    work->stagingStep++;
                case ACTOR_342000_STAGING_RUN:
                    gluttonWork->scale.vx -= 4;
                    break;
            }
            return;
        case ACTOR_342000_STAGING_DOORS_SHAKE:
            switch (work->stagingStep) {
                case ACTOR_342000_STAGING_SETUP:
                    taskMessageDispatch(work->doors[0], ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                    taskMessageDispatch(work->doors[1], ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLACE, &D_actor_342000_801648B8, 0);
                    _actor342000CopyPlacementComponents(&work->doorPlacements[0], &D_actor_342000_80164848[0]);
                    _actor342000CopyPlacementComponents(&work->doorPlacements[1], &D_actor_342000_80164848[1]);
                    work->stagingStep++;
                case ACTOR_342000_STAGING_RUN:
                    gluttonWork->scale.vx          -= 4;
                    work->doorPlacements[0].pos.vx += 5;
                    work->doorPlacements[1].pos.vx += -5;
                    work->doorPlacements[0].pos.vz  = _actor342000StepDoorShake(work->doorPlacements[0].pos.vz, -ACTOR_342000_DOOR_SHAKE_STEP);
                    work->doorPlacements[1].pos.vz  = _actor342000StepDoorShake(work->doorPlacements[1].pos.vz, ACTOR_342000_DOOR_SHAKE_STEP);
                    TASK_MESSAGE_DISPATCH_POINTER(work->doors[0], ACTOR_MESSAGE_PLACE, &work->doorPlacements[0], 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->doors[1], ACTOR_MESSAGE_PLACE, &work->doorPlacements[1], 0);
                    break;
            }
            return;
        case ACTOR_342000_STAGING_NONE:
            break;
        case ACTOR_342000_STAGING_BLEND_ON:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_342000_BLEND_VIEW;
            work->framebufferBlend                                     = taskSpawn(ACTOR_342000_FRAMEBUFFER_BLEND_BANK, ACTOR_342000_FRAMEBUFFER_BLEND_SLOT, ACTOR_342000_FRAMEBUFFER_BLEND_DOUBLE_PASS, 0);
            break;
        case ACTOR_342000_STAGING_BLEND_OFF:
            if (work->framebufferBlend != NULL) {
                taskKill(work->framebufferBlend);
            }
            break;
        case ACTOR_342000_STAGING_DOORS_CLOSING:
            if (work->stagingStep == ACTOR_342000_STAGING_SETUP) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_DOORS_CLOSING, 0, 0);
                work->doorSoundPlaying = 1;
                work->stagingStep++;
            }
            if (gGameSession->location.loc.view == ACTOR_342000_DOORS_HIDDEN_VIEW) {
                taskMessageDispatch(work->doors[0], ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE, 0);
                taskMessageDispatch(work->doors[1], ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE, 0);
            } else {
                taskMessageDispatch(work->doors[0], ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                taskMessageDispatch(work->doors[1], ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            }
            work->doorPlacements[0].pos.vx += 5;
            work->doorPlacements[1].pos.vx -= 5;
            TASK_MESSAGE_DISPATCH_POINTER(work->doors[0], ACTOR_MESSAGE_PLACE, &work->doorPlacements[0], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->doors[1], ACTOR_MESSAGE_PLACE, &work->doorPlacements[1], 0);
            return;
        case ACTOR_342000_STAGING_RESTORE_VIEW:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = work->savedView;
            break;
        case ACTOR_342000_STAGING_DOORS_MEET:
            switch (work->stagingStep) {
                case ACTOR_342000_STAGING_SETUP:
                    taskMessageDispatch(work->doors[0], ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                    taskMessageDispatch(work->doors[1], ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
                    _actor342000CopyPlacementComponents(&work->doorPlacements[0], &D_actor_342000_80164878[0]);
                    _actor342000CopyPlacementComponents(&work->doorPlacements[1], &D_actor_342000_80164878[1]);
                    work->stagingStep++;
                case ACTOR_342000_STAGING_RUN:
                    work->doorPlacements[0].pos.vx += 5;
                    work->doorPlacements[1].pos.vx -= 5;
                    if (work->doorPlacements[0].pos.vx >= ACTOR_342000_DOORS_MEETING_X) {
                        work->doorPlacements[0].pos.vx = ACTOR_342000_DOORS_MEETING_X;
                        work->doorPlacements[1].pos.vx = ACTOR_342000_DOORS_MEETING_X;
                        taskReparent(eventTask, padScriptSpawn(D_actor_444000_80144A74, D_actor_444000_80144A7C));
                        actor444000GluttonSetShakeLevel(GLUTTON_SHAKE_LONG);
                        work->stagingMode = ACTOR_342000_STAGING_NONE;
                    }
                    TASK_MESSAGE_DISPATCH_POINTER(work->doors[0], ACTOR_MESSAGE_PLACE, &work->doorPlacements[0], 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->doors[1], ACTOR_MESSAGE_PLACE, &work->doorPlacements[1], 0);
                    break;
            }
            return;
        case ACTOR_342000_STAGING_DOORS_SHUT:
            sndEvtRequestScriptStop(SOUND_SHELTER_B3_INCINERATOR_DOORS_CLOSING, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_DOORS_SHUT, 0, 0);
            taskReparent(eventTask, padScriptSpawn(D_actor_444000_80144A74, D_actor_444000_80144A7C));
            actor444000GluttonSetShakeLevel(GLUTTON_SHAKE_LONG);
            break;
        default:
            break;
    }
    work->stagingMode = ACTOR_342000_STAGING_NONE;
}

#undef ACTOR_342000_COPY_STAGING_PLACEMENT

/// Kills the event's two door halves and clears their handles during the timed close.
///
/// Requires the published event task and initialized work; each non-null door
/// handle must name a live task. Clearing the handles permits repeated requests.
static inline void _actor342000KillDoorsInline(void)
{
    _Actor342000EventWork* work;

    work = D_actor_342000_80165070->work;
    if (work->doors[0] != NULL) {
        taskKill(work->doors[0]);
    }
    if (work->doors[1] != NULL) {
        taskKill(work->doors[1]);
    }
    work->doors[0] = NULL;
    work->doors[1] = NULL;
}

/// Replaces the timed close's pending player action and resets its first step.
///
/// Requires published event work. `playerAction` uses
/// `ACTOR_342000_PLAYER_ACTION_*`; its bits are retained in an unsigned halfword.
/// The next player-action update consumes the request.
static inline void _actor342000RequestPlayerActionInline(s16 playerAction)
{
    _Actor342000EventWork* work;

    work                   = D_actor_342000_80165070->work;
    work->playerAction     = playerAction;
    work->playerActionStep = 0;
}

/// Selects the timed close's Glutton/door/view staging and resets its first step.
///
/// Requires published event work. `stagingMode` uses `ACTOR_342000_STAGING_*`;
/// its bits are retained in an unsigned halfword for the next staging update.
static inline void _actor342000SetStagingModeInline(s16 stagingMode)
{
    _Actor342000EventWork* work;

    work              = D_actor_342000_80165070->work;
    work->stagingMode = stagingMode;
    work->stagingStep = 0;
}

/// Selects incinerator room 7 and its exit room group after the doors close.
///
/// Requires live session/save state and the loaded incinerator update table.
/// Records the zero-based event room, requests deferred room-object relinking
/// and reapplies saved area updates. The existing view is retained.
static inline void _actor342000EnterIncineratorRoomInline(void)
{
    enum {
        ACTOR_342000_INCINERATOR_EXIT_ROOM       = 7,
        ACTOR_342000_INCINERATOR_EXIT_ROOM_GROUP = 1,
    };
    gGameSession->location.loc.room                            = ACTOR_342000_INCINERATOR_EXIT_ROOM;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ACTOR_342000_INCINERATOR_EXIT_ROOM;
    gGameSession->eventRoomIndex                               = ACTOR_342000_INCINERATOR_EXIT_ROOM - 1;
    gGameSession->incineratorRoomGroup                         = ACTOR_342000_INCINERATOR_EXIT_ROOM_GROUP;
    gGameSession->roomObjsDirty                                = 1;
    areaApplySavedUpdates(D_shelter_b3_garbage_incinerator_8018FB6C);
}

/// Runs the incinerator's Glutton scene and the alternate timed door close.
///
/// Start at state 0 with the incinerator resources and placed encounter actor
/// live. Owns event work and door children; normal entry also creates the
/// Glutton and its five parts before starting the script. Intro skipping enters
/// the timed close directly: 421 ticks, a two-tick view handoff, 60 ticks in
/// view 33, then two ticks before restoring control and exit collision. Pause,
/// menus and actor freeze defer progress; enemy culling stops the door sound.
/// The setup paths require successful work/model spawns and retain unchecked
/// failure behavior. Script completion hands encounter timing back to the room.
static void _actor342000IncineratorEventTask(Task* task)
{
    enum {
        ACTOR_342000_EVENT_INITIALIZE         = 0,
        ACTOR_342000_EVENT_SPAWN_GLUTTON      = 1,
        ACTOR_342000_EVENT_START_SCRIPT       = 2,
        ACTOR_342000_EVENT_WAIT_SCRIPT        = 3,
        ACTOR_342000_EVENT_PREPARE_CLOSE      = 4,
        ACTOR_342000_EVENT_CLOSE              = 5,
        ACTOR_342000_EVENT_DELAY_VIEW         = 6,
        ACTOR_342000_EVENT_HOLD_VIEW          = 7,
        ACTOR_342000_EVENT_WAIT_DOORS         = 8,
        ACTOR_342000_EVENT_RESTORE_PLAYER     = 9,
        ACTOR_342000_EVENT_FINAL_TICK         = 10,
        ACTOR_342000_EVENT_END                = 11,
        ACTOR_342000_EVENT_SOUND_STOP_FRAMES  = 10,
        ACTOR_342000_EVENT_GLUTTON_TASK       = 2,
        ACTOR_342000_EVENT_FIRST_PART_TASK    = 3,
        ACTOR_342000_EVENT_GLUTTON_PART_COUNT = 5,
        ACTOR_342000_EVENT_FIRST_DOOR_TASK    = 8,
        ACTOR_342000_EVENT_SECOND_DOOR_TASK   = 9,
        ACTOR_342000_EVENT_CLOSE_TICKS        = 421,
        ACTOR_342000_EVENT_DOOR_VIEW_TICKS    = 60,
        ACTOR_342000_EVENT_HANDOFF_TICKS      = 2,
        ACTOR_342000_EVENT_DOOR_VIEW          = 33,
    };
    ActorCommand           sceneCommand;
    _Actor342000EventWork* work;
    _Actor342000EventWork* allocatedWork;
    Task*                  partTask;
    u16                    partIndex;
    s16                    elapsedTicks;

    work = task->work;
    if (D_shelter_b3_garbage_incinerator_801855DE != 0 || gGameSession->sceneUpdatesPaused != 0 || Gp_StateC08.menuOpen != ATTACHMENT_MENU_CLOSED || gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
        return;
    }
    if (gGameSession->enemyCullZone != 0) {
        if (work->doorSoundPlaying != 0) {
            sndEvtRequestScriptStop(SOUND_SHELTER_B3_INCINERATOR_DOORS_CLOSING, ACTOR_342000_EVENT_SOUND_STOP_FRAMES);
            work->doorSoundPlaying = 0;
        }
        return;
    }
    switch (task->state) {
        case ACTOR_342000_EVENT_INITIALIZE:
            allocatedWork = memCalloc(sizeof(*allocatedWork), false);
            task->work    = allocatedWork;
            if (allocatedWork == NULL) {
                taskKill(task);
            } else {
                memFillBytes(allocatedWork, 0U, sizeof(*allocatedWork));
                allocatedWork->player           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_actor_342000_80165070         = task;
                allocatedWork->firstPlacedActor = sceneFindEnemyByPlaceKey(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8))->task;
            }
            work = task->work;
            if (gGameSession->skipEventIntro == 0) {
                sceneCommand.context.loc.stage = gGameSession->location.loc.stage;
                sceneCommand.context.loc.area  = gGameSession->location.loc.area;
                sceneCommand.command           = 0;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &sceneCommand, ACTOR_COMMAND_MESSAGE_APPLY);
                work->doors[0] = taskSpawnFromTable(D_actor_342000_80164FF8, ACTOR_342000_EVENT_FIRST_DOOR_TASK, 0, task);
                work->doors[1] = taskSpawnFromTable(D_actor_342000_80164FF8, ACTOR_342000_EVENT_SECOND_DOOR_TASK, 0, task);
                task->state++;
                break;
            }
            work->doors[0] = taskSpawnFromTable(D_actor_342000_80164FF8, ACTOR_342000_EVENT_FIRST_DOOR_TASK, 1, task);
            work->doors[1] = taskSpawnFromTable(D_actor_342000_80164FF8, ACTOR_342000_EVENT_SECOND_DOOR_TASK, 1, task);
            shelterB3GarbageIncineratorShowTimedCaption(SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_EXIT_ENCOUNTER, SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_DEFAULT_KEY, SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_NOTICE_TICKS);
            task->state = ACTOR_342000_EVENT_PREPARE_CLOSE;
            break;
        case ACTOR_342000_EVENT_SPAWN_GLUTTON:
            work->glutton = taskSpawnFromTable(D_actor_342000_80164FF8, ACTOR_342000_EVENT_GLUTTON_TASK, 0, task);
            for (partIndex = 0; partIndex < ACTOR_342000_EVENT_GLUTTON_PART_COUNT; partIndex++) {
                partTask = taskSpawnFromTable(D_actor_342000_80164FF8, partIndex + ACTOR_342000_EVENT_FIRST_PART_TASK, partIndex + 1, work->glutton);
                if (partIndex == 0) {
                    work->gluttonLegRight = partTask;
                }
                if (partIndex == 1) {
                    work->gluttonLegLeft = partTask;
                }
            }
            task->state++;
            break;
        case ACTOR_342000_EVENT_START_SCRIPT:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            evsStartScriptWithSkip(D_actor_342000_80164968, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_342000_80164E30);
            task->state++;
            break;
        case ACTOR_342000_EVENT_WAIT_SCRIPT:
            if (gGameSession->eventState == 0) {
                gGameSession->sceneClock = D_shelter_b3_garbage_incinerator_8018FBC8[0];
                taskSpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 0, 1, 0);
                gGameSession->incineratorExitPhase = GAME_SESSION_INCINERATOR_EXIT_ENCOUNTER;
                taskRequestKill(task, 0);
                return;
            }
            _actor342000UpdatePlayerAction(task);
            _actor342000UpdateSceneStaging(task);
            break;
        case ACTOR_342000_EVENT_PREPARE_CLOSE:
            _actor342000CopyPlacementComponents(&work->doorPlacements[0], &D_actor_342000_80164818[0]);
            _actor342000CopyPlacementComponents(&work->doorPlacements[1], &D_actor_342000_80164818[1]);
            work->stagingMode   = ACTOR_342000_STAGING_DOORS_CLOSING;
            task->killCountdown = 0;
            task->state++;
            break;
        case ACTOR_342000_EVENT_CLOSE:
            elapsedTicks        = (u16)task->killCountdown + 1;
            task->killCountdown = elapsedTicks;
            if (elapsedTicks >= ACTOR_342000_EVENT_CLOSE_TICKS) {
                work->savedView = gGameSession->location.loc.view;
                _actor342000RequestPlayerActionInline(ACTOR_342000_PLAYER_ACTION_WEAPON_CLIP_BLENDED);
                _actor342000SetStagingModeInline(ACTOR_342000_STAGING_DOORS_CLOSING);
                task->killCountdown = 0;
                task->state++;
            }
            _actor342000UpdateSceneStaging(task);
            break;
        case ACTOR_342000_EVENT_DELAY_VIEW:
            elapsedTicks        = (u16)task->killCountdown + 1;
            task->killCountdown = elapsedTicks;
            if (elapsedTicks >= ACTOR_342000_EVENT_HANDOFF_TICKS) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_342000_EVENT_DOOR_VIEW;
                task->state++;
                break;
            }
            break;
        case ACTOR_342000_EVENT_HOLD_VIEW:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_342000_EVENT_DOOR_VIEW;
            task->killCountdown                                        = 0;
            task->state++;
            break;
        case ACTOR_342000_EVENT_WAIT_DOORS:
            elapsedTicks        = (u16)task->killCountdown + 1;
            task->killCountdown = elapsedTicks;
            if (elapsedTicks >= ACTOR_342000_EVENT_DOOR_VIEW_TICKS) {
                _actor342000KillDoorsInline();
                _actor342000SetStagingModeInline(ACTOR_342000_STAGING_RESTORE_VIEW);
                _actor342000EnterIncineratorRoomInline();
                sceneCommand.context.loc.stage = gGameSession->location.loc.stage;
                sceneCommand.context.loc.area  = gGameSession->location.loc.area;
                sceneCommand.command           = 0;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &sceneCommand, ACTOR_COMMAND_MESSAGE_APPLY);
                task->killCountdown = 0;
                task->state++;
                break;
            }
            break;
        case ACTOR_342000_EVENT_RESTORE_PLAYER:
            elapsedTicks        = (u16)task->killCountdown + 1;
            task->killCountdown = elapsedTicks;
            if (elapsedTicks >= ACTOR_342000_EVENT_HANDOFF_TICKS) {
                _actor342000SetStagingModeInline(ACTOR_342000_STAGING_DOORS_SHUT);
                shelterB3GarbageIncineratorSetExitCollisionWalls();
                taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gGameSession->incineratorExitPhase = GAME_SESSION_INCINERATOR_EXIT_ENCOUNTER;
                task->state++;
                break;
            }
            break;
        case ACTOR_342000_EVENT_FINAL_TICK:
            task->state++;
            break;
        case ACTOR_342000_EVENT_END:
            taskRequestKill(task, 0);
            return;
    }
    // The view handoff keeps both posted request channels ticking.
    if ((u32)(task->state - ACTOR_342000_EVENT_DELAY_VIEW) < 5U) {
        _actor342000UpdatePlayerAction(task);
        _actor342000UpdateSceneStaging(task);
    }
}

#include "../../shared/screen_fade_step_down.inc.c"

/// Reveals the screen by reducing a subtractive full-screen overlay each tick.
///
/// State 0 allocates owned primary-heap work, seeds all channels to 255 and runs
/// the first ramp tick. Allocation failure kills the task; teardown frees the
/// work. State 1 requires that live allocation; other states do nothing.
/// The unsigned low halfword of `spawnArg1` is intensity removed per tick
/// (zero holds indefinitely). Drawing uses the low red/green/red bytes before
/// subtraction. Channels narrow to signed 16 bits without clamping; negative
/// stored red ends the task, so large rates may wrap rather than end at once.
/// Requires the frame packet arena and foreground tag used by `fadeDrawOverlay`.
static void _actor342000FadeInTask(Task* task)
{
    enum {
        ACTOR_342000_FADE_STATE_INITIALIZE = 0,
        ACTOR_342000_FADE_STATE_RAMP       = 1,
        ACTOR_342000_FADE_MAX_INTENSITY    = 255,
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case ACTOR_342000_FADE_STATE_INITIALIZE:
            allocatedFade = memCalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade         = allocatedFade;
            fade->b      = ACTOR_342000_FADE_MAX_INTENSITY;
            fade->g      = ACTOR_342000_FADE_MAX_INTENSITY;
            fade->r      = ACTOR_342000_FADE_MAX_INTENSITY;
            task->state += ACTOR_342000_FADE_STATE_RAMP - ACTOR_342000_FADE_STATE_INITIALIZE;
            /* fallthrough */
        case ACTOR_342000_FADE_STATE_RAMP:
            // Draw before stepping so the final visible intensity stays nonnegative.
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            _screenFadeStepDown(fade, task);
            if (fade->r < 0) {
                taskKill(task);
            }
            break;
    }
}

/// Removes a Glutton placement node from the coordinate chain before task teardown.
///
/// Requires initialized work, a live model root and its borrowed parent node.
/// Restores the root's parent before `taskKill` frees the placement work and
/// dispatches child exits, so deferred model release cannot retain that node.
/// The handler must be called only while the task and its work remain live.
static void _actor342000ExitGluttonModel(Task* task)
{
    _Actor342000GluttonModelWork* work;
    GfxCoord*                     modelRootCoord;

    modelRootCoord = task->extra.tmd->coords;
    work           = task->work;

    modelRootCoord->parent = work->parentCoord;
    taskKill(task);
}

#include "../../shared/actor_messages_draw_mode.inc.c"

#include "../../shared/actor_messages_place_ypr.inc.c"

/// Places the Glutton model's coordinate and records its XYZ Euler rotation.
///
/// Handles `ACTOR_MESSAGE_PLACE` on initialized Glutton model work. The borrowed
/// transform supplies parent-frame position in world-coordinate units and angles
/// in 4096 units per turn; the body composes those angles on its next tick.
/// The message ID and second payload are ignored. Dispatch has no defined result.
static void _actor342000PlaceGluttonModel(Task* task, s32 messageId, const ActorTransform* transform, s32 unusedArgument)
{
    _Actor342000GluttonModelWork* work;
    GfxCoord*                     coord;

    work                     = task->work;
    coord                    = &work->coord;
    coord->coord.t[0]        = transform->pos.vx;
    coord->coord.t[1]        = transform->pos.vy;
    coord->coord.t[2]        = transform->pos.vz;
    work->rotation[0]        = transform->rot.vx;
    work->rotation[1]        = transform->rot.vy;
    work->rotation[2]        = transform->rot.vz;
    work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Latches a Glutton model command and applies an optional per-axis scale.
///
/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` on initialized Glutton model work.
/// Command `ACTOR_342000_GLUTTON_COMMAND_SET_SCALE` borrows three readable,
/// word-aligned XYZ scale words from its second payload; the fourth vector word
/// is ignored. Copies each full word; coordinate composition uses its low signed
/// halfword as a Q12 factor (`ONE` is 1.0). Other commands do not read that payload.
/// Every command's complete halfword is retained in `lastCommand`; the context
/// and message ID are ignored. Dispatch has no defined result.
static void _actor342000ApplyGluttonModelCommand(Task* task, s32 messageId, const ActorCommand* request, const VECTOR* scaleFactors)
{
    enum { ACTOR_342000_GLUTTON_COMMAND_SET_SCALE = 10 };
    _Actor342000GluttonModelWork* work;

    work = task->work;
    if (request->command == ACTOR_342000_GLUTTON_COMMAND_SET_SCALE) {
        work->scale.vx = scaleFactors->vx;
        work->scale.vy = scaleFactors->vy;
        work->scale.vz = scaleFactors->vz;
    }
    work->lastCommand = request->command;
}

/// Applies the post-close incinerator room transition from the normal or skip script.
///
/// Requires live session/save state and the loaded incinerator update table;
/// selects room 7, its event-room index and exit room group, then reapplies
/// saved area updates and requests deferred room-object relinking.
static void _actor342000EnterIncineratorRoom(void)
{
    _actor342000EnterIncineratorRoomInline();
}

/// Removes the event's Glutton display model through its exit callback.
///
/// Requires the published event task and initialized work. A live body restores
/// its coordinate parent and tears down its children before its handle is cleared;
/// a null handle makes repeated normal/skip requests harmless.
static void _actor342000RemoveGluttonModel(void)
{
    _Actor342000EventWork* work;

    work = D_actor_342000_80165070->work;
    if (work->glutton != NULL) {
        taskCallExit(work->glutton);
    }
    work->glutton = NULL;
}

/// Kills both event door halves and clears their handles.
///
/// Requires the published event task and initialized work. Each non-null handle
/// must name a live task. Cleared handles allow normal and skip paths to repeat it.
static void _actor342000KillDoors(void)
{
    _Actor342000EventWork* work;

    work = D_actor_342000_80165070->work;
    if (work->doors[0] != NULL) {
        taskKill(work->doors[0]);
    }
    if (work->doors[1] != NULL) {
        taskKill(work->doors[1]);
    }
    work->doors[0] = NULL;
    work->doors[1] = NULL;
}

/// Requests the incinerator alert once across the event's normal and skip paths.
///
/// Requires the published event task, initialized work and loaded alert sound.
/// Shares `alertPlayed` with the player's event-clip action; the latch records
/// the request, without waiting for playback or checking whether it was accepted.
static void _actor342000PlayAlertOnce(void)
{
    _Actor342000EventWork* work;

    work = D_actor_342000_80165070->work;
    if (work->alertPlayed == 0) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_ALERT, 0, 0);
        work->alertPlayed = 1;
    }
}

/// Replaces the event's pending player action and resets its first step.
///
/// Requires the published event task and initialized work. `playerAction` uses
/// `ACTOR_342000_PLAYER_ACTION_*`; its signed input bits are stored in the unsigned
/// halfword. The event task consumes the request on a subsequent update.
static void _actor342000RequestPlayerAction(s16 playerAction)
{
    _Actor342000EventWork* work;

    work                   = D_actor_342000_80165070->work;
    work->playerAction     = playerAction;
    work->playerActionStep = 0;
}

/// Replaces the event's staging mode and resets its first step.
///
/// Requires the published event task and initialized work. `stagingMode` uses
/// `ACTOR_342000_STAGING_*`; its signed input bits are stored in the unsigned
/// halfword. The event task then drives the Glutton, doors and view in that mode.
static void _actor342000SetStagingMode(s16 stagingMode)
{
    _Actor342000EventWork* work;

    work              = D_actor_342000_80165070->work;
    work->stagingMode = stagingMode;
    work->stagingStep = 0;
}

/// Ends the event's battle once and selects its post-battle scene music.
///
/// Requires the published event task, session and live save. Clears battle holds
/// and stimuli, leaves a 15-frame end delay in the idle phase, requests weapon
/// re-equipping and latches completion across the normal and skip scripts.
static void _actor342000EndBattleOnce(void)
{
    enum {
        ACTOR_342000_BATTLE_END_DELAY_FRAMES = 15,
        ACTOR_342000_POST_BATTLE_SCENE_MUSIC = 13,
    };
    _Actor342000EventWork* work;

    work = D_actor_342000_80165070->work;
    if (work->combatReset == 0) {
        gSceneCombatState.battleRefs                        = 0;
        gSceneCombatState.signals.bytes.endDelayFrames      = ACTOR_342000_BATTLE_END_DELAY_FRAMES;
        gSceneCombatState.signals.bytes.battlePhase         = SCENE_COMBAT_BATTLE_IDLE;
        gSceneCombatState.signals.bytes.actionFlags         = 0;
        gSceneCombatState.signals.bytes.enemyAlert          = 0;
        gGameSession->flowFlags                            |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACTOR_342000_POST_BATTLE_SCENE_MUSIC;
        work->combatReset                                   = 1;
    }
}

/// Forwards a model draw mode to the event's player and attached equipment.
///
/// Requires the published event task and its live player. `drawMode` uses
/// `PLAYER_ACTOR_MODEL_DRAW_*`; the script hides with allocation (0) and resumes
/// automatic drawing (1). The player's dispatch result is discarded.
static void _actor342000SetPlayerDrawMode(s32 drawMode)
{
    _Actor342000EventWork* dispatchWork;

    dispatchWork = D_actor_342000_80165070->work;
    taskMessageDispatch(dispatchWork->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
}

/// Restores the player, exit walls and scene playback state when the event is skipped.
///
/// Requires the published event task, live player/session/save, loaded weapon
/// animation banks and a previously selected scene. Places the player at the
/// exit mark, starts weapon clip 1 with world collision disabled and restores
/// automatic model drawing. Stops the framebuffer blend, then discards deferred
/// scene requests and requests CD cancellation. Animation dispatch borrows the
/// stack request synchronously; its selected resources must outlive playback.
static void _actor342000RestorePlayerAfterSkip(void)
{
    enum {
        ACTOR_342000_PRIMARY_CHARACTER          = 1,
        ACTOR_342000_PRIMARY_WEAPON_BANK_BASE   = 1,
        ACTOR_342000_ALTERNATE_WEAPON_BANK_BASE = 34,
        ACTOR_342000_WEAPON_CLIP                = 1,
    };
    _Actor342000EventWork* work;
    AnimationPlayRequest   animationRequest;
    s32                    weaponId;
    s32                    animationBankIndex;

    work = D_actor_342000_80165070->work;
    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_342000_80164948, 0);
    shelterB3GarbageIncineratorSetExitCollisionWalls();
    // Resume clip 1 in the current character's equipped-weapon bank.
    weaponId                              = gPlayerStatus.weapon;
    animationBankIndex                    = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_342000_PRIMARY_CHARACTER)
                                                ? weaponId + ACTOR_342000_PRIMARY_WEAPON_BANK_BASE
                                                : weaponId + ACTOR_342000_ALTERNATE_WEAPON_BANK_BASE;
    animationRequest.source.index         = animationBankIndex;
    animationRequest.animationId          = ACTOR_342000_WEAPON_CLIP;
    animationRequest.blend                = ANIMATION_BLEND_RESET;
    animationRequest.blendFrames          = 0;
    animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
    {
        _Actor342000EventWork* dispatchWork;

        dispatchWork = D_actor_342000_80165070->work;
        taskMessageDispatch(dispatchWork->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);
    }
    // Release the transient blend before finishing the selected scene session.
    if (work->framebufferBlend != NULL) {
        taskKill(work->framebufferBlend);
        work->framebufferBlend = NULL;
    }
    cdCmdCancelScene();
}

/// Stages the selected scene's deferred audio-start request from the event script.
///
/// Requires a selected scene with prepared playback buffers that remain live
/// through consumption. A later commit enqueues the staged request.
static void _actor342000StageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Enqueues playback of the event script's selected scene/audio session.
///
/// Requires prepared playback buffers and room in the resident CD command ring;
/// the selected descriptor and buffers must remain live through consumption.
static void _actor342000EnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Finishes the event's scene stream, then discards deferred requests and cancels CD playback.
///
/// Requires a prior successful scene selection. The explicit stream finish and
/// the finish inside `cdCmdCancelScene` both restore the saved random values;
/// their order is retained. Buffers and tasks remain their owners' responsibility.
static void _actor342000FinishScene(void)
{
    streamFinishScene();
    cdCmdCancelScene();
}
