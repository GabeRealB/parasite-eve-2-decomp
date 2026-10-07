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
#include "gameplay/captions.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
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
/// `func_actor_342000_8016382C`.
extern Task* D_actor_342000_80165070;

/// Message 0x7D4's static payload, handed to `taskMessageDispatch` by the actor's
/// spawn tick. The same record the handler takes.
extern ActorTransform D_actor_342000_801648B8;

/// Fixed placement `func_actor_342000_8016439C` warps slot 3 to, sent as
/// message 0x3E9 and again as 0x3F2 by `func_actor_342000_80162BBC`.
extern ActorTransform D_actor_342000_80164948;

// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_342000_801648A8[2];

/// Animation-id bank `_Actor342000GluttonModelWork::followUpIndex` indexes; a negative entry
/// means the bank is empty and the slots are left alone.
extern s16 D_actor_342000_80164810[];

/// Per-`spawnArg1` translation seeds for the child model's part coordinate.
extern SVECTOR D_actor_342000_80164900[];

void func_actor_342000_8016201C(Task*);
void func_actor_342000_801625D8(Task*);
void func_actor_342000_801628C8(Task*);
void func_actor_342000_8016382C(Task*);
void func_actor_342000_80163EAC(Task*);
s32  func_actor_342000_801640C0(Task* task, s32 msgId, ActorTransform* transform, s32 arg3);
s32  func_actor_342000_80164110(Task* task, s32 msgId, ActorCommand* request, ActorTransform* transform);
void func_actor_342000_80164154(void);
void func_actor_342000_801641B4(void);
void func_actor_342000_801641FC(void);
void func_actor_342000_80164260(void);
void func_actor_342000_801642B4(s16);
void func_actor_342000_801642D4(s16);
void func_actor_342000_801642F4(void);
void func_actor_342000_80164364(s32);
void func_actor_342000_8016439C(void);
void func_actor_342000_8016447C(void);
void func_actor_342000_8016449C(void);
void func_actor_342000_801644BC(void);

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
    { ACTOR_MESSAGE_PLACE, func_actor_342000_801640C0 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_342000_80164110 },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = ACTOR_342000_PLAYER_ACTION_PLACE_AT_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = ACTOR_342000_STAGING_GLUTTON_SINKS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_342000_80164960 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_8016447C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_8016449C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = ACTOR_342000_PLAYER_ACTION_WALK_TO_MARK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = ACTOR_342000_STAGING_GLUTTON_ALONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = ACTOR_342000_PLAYER_ACTION_PLACE_AT_MARK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = ACTOR_342000_STAGING_DOORS_SHAKE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801641B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = ACTOR_342000_STAGING_DOORS_MEET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = ACTOR_342000_PLAYER_ACTION_WEAPON_CLIP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801644BC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = ACTOR_342000_STAGING_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801641FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801642F4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_342000_80164364 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_342000_80164364 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = ACTOR_342000_PLAYER_ACTION_WEAPON_CLIP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_80164154 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = ACTOR_342000_STAGING_BLEND_ON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = ACTOR_342000_STAGING_BLEND_OFF }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = ACTOR_342000_PLAYER_ACTION_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = shelterB3GarbageIncineratorSetExitCollisionWalls }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_342000_80164E30[19] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_80164154 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642B4 }, { .value = ACTOR_342000_PLAYER_ACTION_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_342000_801642D4 }, { .value = ACTOR_342000_STAGING_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801641B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801641FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_8016439C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_801642F4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342000_80164260 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_342000_80164FF8[10] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_342000_8016382C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_342000_80163EAC, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_801628C8, { .model = &gActor444000Actor403200Model10824 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_801625D8, { .model = &gActor444000GluttonLegRight } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_801625D8, { .model = &gActor444000GluttonLegLeft } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_801625D8, { .model = &gActor444000Actor403200Model12884 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_801625D8, { .model = &gActor444000Actor403200Model13774 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_801625D8, { .model = &gActor444000Actor403200Model18BE4 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_8016201C, { .model = &gActor444000Model1C814 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_342000_8016201C, { .model = &gActor444000Model1D36C } },
};

Task* D_actor_342000_80165070;

static void func_actor_342000_80163F88(Task* arg0);

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

void actor444000GluttonSetShakeLevel(s8 arg0);

/// Spawn table of the event task's children: entry 2 is the script parent,
/// 3..7 its five script tasks and 8/9 the two effect actors.
extern TaskDesc D_actor_342000_80164FF8[];

extern EvsCommand D_actor_342000_80164968[];

extern EvsCommand D_actor_342000_80164E30[];

static s32         func_actor_342000_80161EA4(Task* arg0, u16 arg1);
static inline void Actor342000_InitCoord(Task* arg0, _Actor342000GluttonModelWork* w);
static void        func_actor_342000_80162158(Task* arg0);
static void        func_actor_342000_80162BBC(Task* arg0);
static inline void Actor342000_CopyMove(ActorTransform* dst, ActorTransform* src);
static inline void Actor342000_SetAnim(Task* task, u16 anim, u16 blend, u16 n);
static inline s32  Actor342000_Sway(s32 x, s32 d);
static inline void Actor342000_Add(long* value, s32 delta);
static inline void Actor342000_Store(long* dst, s32 value);
static void        func_actor_342000_80162F28(Task* arg0);
static inline void Actor342000_KillFx(void);
static inline void Actor342000_SetAction(s16 arg0);
static inline void Actor342000_SetMode(s16 arg0);
static inline void Actor342000_EnterArea(void);

/// Ticks slots `(arg1 == 8)..arg1-1` of the task's animation context (slot 0 is
/// skipped for the eight-slot actor). If every one of them then has
/// `ANIMATION_SLOT_SETTLED` set, passes them the
/// `D_actor_342000_80164810` id and returns 1; otherwise returns 0. The gotos
/// reproduce retail's block layout.
static s32 func_actor_342000_80161EA4(Task* arg0, u16 arg1)
{
    _Actor342000GluttonModelWork* work;
    _Actor342000GluttonModelWork* ctx;
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
        if (D_actor_342000_80164810[work->followUpIndex] >= 0) {
            anim  = D_actor_342000_80164810[work->followUpIndex];
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

void func_actor_342000_8016201C(Task* arg0)
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
            mtx->parent  = arg0->spawnArg2.pointer;
            extra->flags = 0;
            if (arg0->spawnArg1.value != 0) {
                extra->otOffset = 0x1F;
            }
            arg0->extra.tmd->coords->parent = &gGfxViewCoord;
            extra->colorMtx                 = &mtx->color;
            extra->lightMtx                 = &mtx->light;
            arg0->msgTable                  = D_actor_342000_801648A8;
            taskReparent(mtx->parent, arg0);
        }
        arg0->state += 1;
    }

    mdl    = arg0->extra.tmd;
    pos.vx = arg0->extra.tmd->coords->workm.t[0];
    pos.vy = arg0->extra.tmd->coords->workm.t[1];
    pos.vz = arg0->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(mdl, &pos, 0, 3);
}

/// Parents the work block's own coordinate to `_Actor342000GluttonModelWork::parentCoord`,
/// hangs the model's part coordinate off it and resets it to an identity
/// matrix with no translation. Every `func_actor_342000_80162158` case repeats
/// it; as a function its address pseudos are born at their first use instead
/// of being hoisted to the top of each case.
static inline void Actor342000_InitCoord(Task* arg0, _Actor342000GluttonModelWork* w)
{
    GfxCoord*  coord;
    GfxMatrix* mtx;

    coord                                 = &w->coord;
    coord->parent                         = ((_Actor342000GluttonModelWork*)arg0->work)->parentCoord;
    arg0->extra.tmd->coords->parent       = coord;
    coord->coord.t[0]                     = 0;
    coord->coord.t[1]                     = 0;
    coord->coord.t[2]                     = 0;
    mtx                                   = (GfxMatrix*)&w->coord.coord;
    mtx->rotationWords.m00M01             = ONE;
    mtx->rotationWords.m02M10             = 0;
    mtx->rotationWords.m11M12             = ONE;
    mtx->rotationWords.m20M21             = 0;
    mtx->rotationWords.m22                = ONE;
    w->coord.composeStamp                 = GRAPHICS_COORD_DIRTY;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Spawn tick shared by the actor and its child model tasks: allocates and
/// zeroes the work block, republishes its light/colour matrices onto the model,
/// applies the area record 0x20's TMD bytes and, per `Task::spawnArg1`, parents
/// the coordinate (view, parent model, or the parent part
/// `D_actor_342000_80164900` names) and binds the animation bank. Cases 1 and 2
/// register themselves on the parent as `legRight` / `legLeft`.
///
/// The empty loops before `case 1:` / `case 2:` make reorg fill the dispatch
/// delay slots from those arms; one `ctx` per case keeps each short-lived so
/// the work pointer outranks it for `$s1`.
static void func_actor_342000_80162158(Task* arg0)
{
    TmdObject*                    extra;
    _Actor342000GluttonModelWork* work;
    _Actor342000GluttonModelWork* ctx;
    _Actor342000GluttonModelWork* ctx2;
    _Actor342000GluttonModelWork* ctx3;
    _Actor342000GluttonModelWork* w;
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
    w->parent       = (Task*)arg0->spawnArg2.pointer;
    extra->lightMtx = &w->light;
    extra->colorMtx = &w->color;
    arg0->msgTable  = D_actor_342000_801648E8;
    rec             = (Gp_GetNestedAreaRec(&gGameSession->location.loc))->placements;
    for (; rec->entryId != AREA_PLACEMENT_END; rec++) {
        if (rec->entryId == 0x20) {
            break;
        }
    }
    tmdSetTextureOffsets(extra, rec->texturePageOffset, rec->clutRowOffset);
    switch (arg0->spawnArg1.value) {
        case 0:
            w->parentCoord = &gGfxViewCoord;
            Actor342000_InitCoord(arg0, w);
            animationInitContext(&w->rig.anim, D_actor_342000_801647F8, extra, w->rig.poses, w->rig.slots);
            ctx = arg0->work;
            for (i = 1; i < 8; i++) {
                ctx->rig.slots[i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&ctx->rig.anim, i, 0);
            }
            break;
            do {
            } while (0);
        case 1:
            w->parentCoord = w->parent->extra.tmd->coords;
            Actor342000_InitCoord(arg0, w);
            ((_Actor342000GluttonModelWork*)w->parent->work)->legRight = arg0;
            animationInitContext(&w->rig.anim, D_actor_342000_80164800, extra, w->rig.poses, w->rig.slots);
            ctx2 = arg0->work;
            for (i = 0; i < 4; i++) {
                ctx2->rig.slots[i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&ctx2->rig.anim, i, 0);
            }
            break;
            do {
            } while (0);
        case 2:
            w->parentCoord = w->parent->extra.tmd->coords;
            Actor342000_InitCoord(arg0, w);
            ((_Actor342000GluttonModelWork*)w->parent->work)->legLeft = arg0;
            animationInitContext(&w->rig.anim, D_actor_342000_80164808, extra, w->rig.poses, w->rig.slots);
            ctx3 = arg0->work;
            for (i = 0; i < 4; i++) {
                ctx3->rig.slots[i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&ctx3->rig.anim, i, 0);
            }
            break;
        default:
            w->parentCoord = &w->parent->extra.tmd->coords[D_actor_342000_80164900[arg0->spawnArg1.value].pad];
            Actor342000_InitCoord(arg0, w);
            break;
    }
    taskReparent(w->parent, arg0);
    arg0->exitCallback = func_actor_342000_80163F88;
}

/// Display handler of the actor's child model. The spawn tick seeds the
/// model's part coordinate translation from the `D_actor_342000_80164900` entry
/// `Task::spawnArg1` selects; state 1 resets the work block's coordinate to
/// identity and scales each column by the parent's `_Actor342000GluttonModelWork::scale`.
/// Every tick then mirrors the parent model's `TmdObject::flags` and hands the
/// second part translation to `worldCoordSetModelLighting`.
void func_actor_342000_801625D8(Task* arg0)
{
    _Actor342000GluttonModelWork* work;
    GfxMatrix*                    mtx;
    VECTOR*                       sc;
    GfxCoord*                     coord;
    TmdObject*                    extra;
    VECTOR                        pos;

    work = arg0->work;

    switch (arg0->state) {
        case 0:
            func_actor_342000_80162158(arg0);
            work                = arg0->work;
            coord               = arg0->extra.tmd->coords;
            coord->coord.t[0]   = D_actor_342000_80164900[arg0->spawnArg1.value].vx;
            coord->coord.t[1]   = D_actor_342000_80164900[arg0->spawnArg1.value].vy;
            coord->coord.t[2]   = D_actor_342000_80164900[arg0->spawnArg1.value].vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            arg0->state        += 1;
            break;
        case 1:
            sc                        = &((_Actor342000GluttonModelWork*)work->parent->work)->scale;
            mtx                       = (GfxMatrix*)&work->coord.coord;
            mtx->rotationWords.m00M01 = ONE;
            mtx->rotationWords.m02M10 = 0;
            mtx->rotationWords.m11M12 = ONE;
            mtx->rotationWords.m20M21 = 0;
            mtx->rotationWords.m22    = ONE;
            gfxScaleMatrixColumns(&mtx->mat, sc);
            work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
    arg0->extra.tmd->flags = work->parent->extra.tmd->flags;
    extra                  = arg0->extra.tmd;
    pos.vx                 = arg0->extra.tmd->coords[1].workm.t[0];
    pos.vy                 = arg0->extra.tmd->coords[1].workm.t[1];
    pos.vz                 = arg0->extra.tmd->coords[1].workm.t[2];
    worldCoordSetModelLighting(extra, &pos, 0, 3);
}

/// Display handler of the actor itself. The spawn tick initialises the work
/// block and sends message 0x7D4. State 1 rebuilds the actor coordinate: an
/// identity rotation, the euler angles below it composed onto it (Y, then X,
/// then Z), and every column scaled by the matching component of
/// `_Actor342000GluttonModelWork::scale`. Every later tick ticks the actor's own
/// animation bank and the two child tasks and hands the model's second part
/// translation to `worldCoordSetModelLighting`.
void func_actor_342000_801628C8(Task* arg0)
{
    _Actor342000GluttonModelWork* work;
    _Actor342000GluttonModelWork* data;
    GfxMatrix*                    mtx;
    s32*                          ang;
    TmdObject*                    extra;
    VECTOR                        pos;

    work = arg0->work;

    switch (arg0->state) {
        case 0:
            func_actor_342000_80162158(arg0);
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_342000_801648B8, 0);
            arg0->state += 1;
            return;
        case 1:
            mtx                       = (GfxMatrix*)&work->coord.coord;
            mtx->rotationWords.m00M01 = ONE;
            mtx->rotationWords.m02M10 = 0;
            mtx->rotationWords.m11M12 = ONE;
            mtx->rotationWords.m20M21 = 0;
            mtx->rotationWords.m22    = ONE;
            ang                       = work->rotation;
            gfxRotMatrixY(&mtx->mat, ang[1], 1);
            gfxRotMatrixX(&mtx->mat, ang[0], GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixZ(&mtx->mat, ang[2], GRAPHICS_ROTATION_COMPOSE);
            gfxScaleMatrixColumns(&mtx->mat, &work->scale);
            work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
            /* fallthrough */
        default:
            data = arg0->work;
            func_actor_342000_80161EA4(arg0, 8);
            func_actor_342000_80161EA4(data->legRight, 4);
            func_actor_342000_80161EA4(data->legLeft, 4);
            extra  = arg0->extra.tmd;
            pos.vx = arg0->extra.tmd->coords[1].workm.t[0];
            pos.vy = arg0->extra.tmd->coords[1].workm.t[1];
            pos.vz = arg0->extra.tmd->coords[1].workm.t[2];
            worldCoordSetModelLighting(extra, &pos, 0, 3);
    }
}

/// `gPlayerStatus.weapon` is the
/// base weapon id, `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` selects the alternate animation block.

/// Per-tick sequence driver of the event task: raises 0x3ED on `player`,
/// then runs the one-shot request latched in `playerAction` (warps, animation
/// changes for the slot-3 task, `ACTOR_342000_PLAYER_ACTION_WALK_TO_MARK`'s
/// wait on 0x3F0 plus an 11-tick delay, and the alert cue of
/// `ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_3`) and clears it. Cases 5 and 7 keep their
/// weapon id locals block-scoped; sharing one pseudo across both cases moves
/// the `gPlayerStatus.weapon` load ahead of the flag load.
static void func_actor_342000_80162BBC(Task* arg0)
{
    _Actor342000EventWork* work;
    _Actor342000EventWork* ev;
    AnimationPlayRequest   msg;

    work = arg0->work;
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
            switch (work->playerActionStep) {
                case 0:
                    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_MOVE_TO, &D_actor_342000_80164948, 0);
                    work->playerActionStep++;
                    return;
                case 1:
                    if (taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                        work->playerActionTicks = 0;
                        work->playerActionStep++;
                    }
                    return;
                case 2:
                    if (++work->playerActionTicks > 10) {
                        work->playerAction       = ACTOR_342000_PLAYER_ACTION_NONE;
                        msg.source.sets          = D_actor_342000_801647E8;
                        msg.animationId          = 0;
                        msg.blend                = ANIMATION_BLEND_RESET;
                        msg.blendFrames          = 0;
                        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
                    }
                    return;
            }
            return;
        case ACTOR_342000_PLAYER_ACTION_PLACE_AT_MARK:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_342000_80164948, 0);
            msg.source.sets          = D_actor_342000_801647E8;
            msg.animationId          = 0;
            msg.blend                = ANIMATION_BLEND_RESET;
            msg.blendFrames          = 0;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            break;
        case ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_1:
            msg.source.sets          = D_actor_342000_801647E8;
            msg.animationId          = 1;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 10;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            break;
        case ACTOR_342000_PLAYER_ACTION_WEAPON_CLIP: {
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
            break;
        }
        case ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_2:
            msg.source.sets          = D_actor_342000_801647E8;
            msg.animationId          = 2;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 10;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            break;
        case ACTOR_342000_PLAYER_ACTION_WEAPON_CLIP_BLENDED: {
            s32 weaponId;
            s32 anim;

            weaponId                 = gPlayerStatus.weapon;
            anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.source.index         = anim;
            msg.animationId          = 1;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 10;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &msg, 0);
            break;
        }
        case ACTOR_342000_PLAYER_ACTION_EVENT_CLIP_3:
            msg.source.sets          = D_actor_342000_801647E8;
            msg.animationId          = 3;
            msg.blend                = ANIMATION_BLEND_RESET;
            msg.blendFrames          = 0;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            ev = D_actor_342000_80165070->work;
            if (ev->alertPlayed == 0) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_ALERT, 0, 0);
                ev->alertPlayed = 1;
            }
            break;
    }
    work->playerAction = ACTOR_342000_PLAYER_ACTION_NONE;
}

static inline void Actor342000_CopyMove(ActorTransform* dst, ActorTransform* src)
{
    dst->pos.vx = src->pos.vx;
    dst->pos.vy = src->pos.vy;
    dst->pos.vz = src->pos.vz;
    dst->rot.vx = src->rot.vx;
    dst->rot.vy = src->rot.vy;
    dst->rot.vz = src->rot.vz;
}

static inline void Actor342000_SetAnim(Task* task, u16 anim, u16 blend, u16 n)
{
    _Actor342000GluttonModelWork* ctx;
    u16                           i;
    u16                           first;

    first = n == 8;
    ctx   = task->work;
    if (blend == 0) {
        for (i = first; i < n; i++) {
            ctx->rig.slots[i].rate = ANIMATION_RATE_ONE;
            animationResetSlot(&ctx->rig.anim, i, anim);
        }
    } else {
        for (i = first; i < n; i++) {
            animationSeekSlotWithBlend(&ctx->rig.anim, i, anim, 0, blend);
        }
    }
}

static inline s32 Actor342000_Sway(s32 x, s32 d)
{
    if (gDisplayState.animFrame & 1) {
        return x + d;
    }
    return x - d;
}

static inline void Actor342000_Add(long* value, s32 delta)
{
    *value += delta;
}

static inline void Actor342000_Store(long* dst, s32 value)
{
    *dst = value;
}

static void func_actor_342000_80162F28(Task* arg0)
{
    _Actor342000EventWork*        work;
    _Actor342000GluttonModelWork* gluttonWork;
    ActorTransform*               src;
    s32                           v;

    work        = arg0->work;
    gluttonWork = work->glutton->work;
    switch (work->stagingMode) {
        case ACTOR_342000_STAGING_GLUTTON_SINKS:
            switch (work->stagingStep) {
                case 0:
                    taskMessageDispatch(work->glutton, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    taskMessageDispatch(work->doors[0], ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    taskMessageDispatch(work->doors[1], ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    Actor342000_CopyMove(&work->doorPlacements[0], &D_actor_342000_80164818[0]);
                    Actor342000_CopyMove(&work->doorPlacements[1], &D_actor_342000_80164818[1]);
                    gluttonWork->scale.vx         = ONE;
                    gluttonWork->scale.vy         = ONE;
                    gluttonWork->scale.vz         = ONE;
                    src                           = &D_actor_342000_801648B8;
                    work->gluttonPlacement.pos.vx = src->pos.vx;
                    work->gluttonPlacement.pos.vy = src->pos.vy;
                    work->gluttonPlacement.pos.vz = src->pos.vz;
                    work->gluttonPlacement.rot.vx = src->rot.vx;
                    work->gluttonPlacement.rot.vy = src->rot.vy;
                    work->gluttonPlacement.rot.vz = src->rot.vz;
                    work->stagingTicks            = 0;
                    work->stagingStep++;
                case 1:
                    if (++work->stagingTicks == 60) {
                        Actor342000_SetAnim(work->glutton, 1, 10, 8);
                        Actor342000_SetAnim(work->gluttonLegRight, 1, 10, 4);
                        Actor342000_SetAnim(work->gluttonLegLeft, 1, 10, 4);
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
                case 0:
                    taskMessageDispatch(work->doors[0], ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
                    taskMessageDispatch(work->doors[1], ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLACE, &D_actor_342000_801648D0, 0);
                    Actor342000_SetAnim(work->glutton, 0, 0, 8);
                    Actor342000_SetAnim(work->gluttonLegRight, 0, 0, 4);
                    Actor342000_SetAnim(work->gluttonLegLeft, 0, 0, 4);
                    work->stagingStep++;
                case 1:
                    gluttonWork->scale.vx -= 4;
                    break;
            }
            return;
        case ACTOR_342000_STAGING_DOORS_SHAKE:
            switch (work->stagingStep) {
                case 0:
                    taskMessageDispatch(work->doors[0], ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    taskMessageDispatch(work->doors[1], ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->glutton, ACTOR_MESSAGE_PLACE, &D_actor_342000_801648B8, 0);
                    Actor342000_CopyMove(&work->doorPlacements[0], &D_actor_342000_80164848[0]);
                    Actor342000_CopyMove(&work->doorPlacements[1], &D_actor_342000_80164848[1]);
                    work->stagingStep++;
                case 1:
                    gluttonWork->scale.vx -= 4;
                    Actor342000_Add(&work->doorPlacements[0].pos.vx, 5);
                    Actor342000_Add(&work->doorPlacements[1].pos.vx, -5);
                    v = Actor342000_Sway(work->doorPlacements[0].pos.vz, -20);
                    Actor342000_Store(&work->doorPlacements[0].pos.vz, v);
                    v = Actor342000_Sway(work->doorPlacements[1].pos.vz, 20);
                    Actor342000_Store(&work->doorPlacements[1].pos.vz, v);
                    TASK_MESSAGE_DISPATCH_POINTER(work->doors[0], ACTOR_MESSAGE_PLACE, &work->doorPlacements[0], 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->doors[1], ACTOR_MESSAGE_PLACE, &work->doorPlacements[1], 0);
                    break;
            }
            return;
        case ACTOR_342000_STAGING_NONE:
            break;
        case ACTOR_342000_STAGING_BLEND_ON:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 8;
            work->framebufferBlend                                     = taskSpawn(1, 0x2D, 0x10, 0);
            break;
        case ACTOR_342000_STAGING_BLEND_OFF:
            if (work->framebufferBlend != NULL) {
                taskKill(work->framebufferBlend);
            }
            break;
        case ACTOR_342000_STAGING_DOORS_CLOSING:
            if (work->stagingStep == 0) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_DOORS_CLOSING, 0, 0);
                work->doorSoundPlaying = 1;
                work->stagingStep++;
            }
            if (gGameSession->location.loc.view == 0xF) {
                taskMessageDispatch(work->doors[0], ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
                taskMessageDispatch(work->doors[1], ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
            } else {
                taskMessageDispatch(work->doors[0], ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                taskMessageDispatch(work->doors[1], ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
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
                case 0:
                    taskMessageDispatch(work->doors[0], ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    taskMessageDispatch(work->doors[1], ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    Actor342000_CopyMove(&work->doorPlacements[0], &D_actor_342000_80164878[0]);
                    Actor342000_CopyMove(&work->doorPlacements[1], &D_actor_342000_80164878[1]);
                    work->stagingStep++;
                case 1:
                    work->doorPlacements[0].pos.vx += 5;
                    work->doorPlacements[1].pos.vx -= 5;
                    if (work->doorPlacements[0].pos.vx >= 0x36B0) {
                        work->doorPlacements[0].pos.vx = 0x36B0;
                        work->doorPlacements[1].pos.vx = 0x36B0;
                        taskReparent(arg0, Gp_SpawnScript18(D_actor_444000_80144A74, D_actor_444000_80144A7C));
                        actor444000GluttonSetShakeLevel(3);
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
            taskReparent(arg0, Gp_SpawnScript18(D_actor_444000_80144A74, D_actor_444000_80144A7C));
            actor444000GluttonSetShakeLevel(3);
            break;
        default:
            break;
    }
    work->stagingMode = ACTOR_342000_STAGING_NONE;
}

/// The event task's leaf steps, inlined here; `actor_342000_3.c` carries the
/// same bodies as out-of-line functions (`func_actor_342000_801641FC`,
/// `801642B4`, `801642D4`, `80164154`).
static inline void Actor342000_KillFx(void)
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

static inline void Actor342000_SetAction(s16 arg0)
{
    _Actor342000EventWork* work;

    work                   = D_actor_342000_80165070->work;
    work->playerAction     = arg0;
    work->playerActionStep = 0;
}

static inline void Actor342000_SetMode(s16 arg0)
{
    _Actor342000EventWork* work;

    work              = D_actor_342000_80165070->work;
    work->stagingMode = arg0;
    work->stagingStep = 0;
}

static inline void Actor342000_EnterArea(void)
{
    gGameSession->location.loc.room                            = 7;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 7;
    gGameSession->eventRoomIndex                               = 6;
    gGameSession->incineratorRoomGroup                         = 1;
    gGameSession->roomObjsDirty                                = 1;
    Gp_ApplyAreaRecs(D_shelter_b3_garbage_incinerator_8018FB6C);
}

/// Event/sequence task body, idle while a cutscene, pause or mode switch is up.
/// State 0 allocates the `_Actor342000EventWork` block and spawns the two door
/// halves (a spawn with `GameSession::skipEventIntro` set skips to state 4);
/// states 1..10 spawn the script tasks, seed the placements and run the timed
/// hand-off to area 0x21, and state 11 kills the task. `SOFT_BARRIER()` keeps
/// state 7's `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` store ahead of the state load, as in retail.
void func_actor_342000_8016382C(Task* arg0)
{
    ActorCommand           msg;
    _Actor342000EventWork* work;
    _Actor342000EventWork* alloc;
    ActorTransform*        src;
    ActorTransform*        dst;
    Task*                  child;
    u16                    i;
    s16                    timer;

    work = arg0->work;
    if (D_shelter_b3_garbage_incinerator_801855DE != 0 || gGameSession->sceneUpdatesPaused != 0 || Gp_StateC08.menuOpen != ATTACHMENT_MENU_CLOSED || gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
        return;
    }
    if (gGameSession->enemyCullZone != 0) {
        if (work->doorSoundPlaying != 0) {
            sndEvtRequestScriptStop(SOUND_SHELTER_B3_INCINERATOR_DOORS_CLOSING, 0xA);
            work->doorSoundPlaying = 0;
        }
        return;
    }
    switch (arg0->state) {
        case 0:
            alloc      = memCalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(alloc, 0U, sizeof(*alloc));
                alloc->player           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_actor_342000_80165070 = arg0;
                alloc->firstPlacedActor = sceneFindEnemyByPlaceKey(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8))->task;
            }
            work = arg0->work;
            if (gGameSession->skipEventIntro == 0) {
                msg.context.loc.stage = gGameSession->location.loc.stage;
                msg.context.loc.area  = gGameSession->location.loc.area;
                msg.command           = 0;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
                work->doors[0] = taskSpawnFromTable(D_actor_342000_80164FF8, 8, 0, arg0);
                work->doors[1] = taskSpawnFromTable(D_actor_342000_80164FF8, 9, 0, arg0);
                arg0->state++;
                break;
            }
            work->doors[0] = taskSpawnFromTable(D_actor_342000_80164FF8, 8, 1, arg0);
            work->doors[1] = taskSpawnFromTable(D_actor_342000_80164FF8, 9, 1, arg0);
            func_shelter_b3_garbage_incinerator_80180FE4(0x17, 0, 0x3C);
            arg0->state = 4;
            break;
        case 1:
            work->glutton = taskSpawnFromTable(D_actor_342000_80164FF8, 2, 0, arg0);
            for (i = 0; i < 5; i++) {
                child = taskSpawnFromTable(D_actor_342000_80164FF8, i + 3, i + 1, work->glutton);
                if (i == 0) {
                    work->gluttonLegRight = child;
                }
                if (i == 1) {
                    work->gluttonLegLeft = child;
                }
            }
            arg0->state++;
            break;
        case 2:
            Gp_MsgPlayerWeapon(0);
            evsStartScriptWithSkip(D_actor_342000_80164968, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_342000_80164E30);
            arg0->state++;
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                gGameSession->sceneClock = D_shelter_b3_garbage_incinerator_8018FBC8[0];
                taskSpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 0, 1, 0);
                gGameSession->incineratorExitPhase = GAME_SESSION_INCINERATOR_EXIT_ENCOUNTER;
                taskRequestKill(arg0, 0);
                return;
            }
            func_actor_342000_80162BBC(arg0);
            func_actor_342000_80162F28(arg0);
            break;
        case 4:
            Actor342000_CopyMove(&work->doorPlacements[0], &D_actor_342000_80164818[0]);
            Actor342000_CopyMove(&work->doorPlacements[1], &D_actor_342000_80164818[1]);
            work->stagingMode   = ACTOR_342000_STAGING_DOORS_CLOSING;
            arg0->killCountdown = 0;
            arg0->state++;
            break;
        case 5:
            timer               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = timer;
            if (timer >= 0x1A5) {
                work->savedView = gGameSession->location.loc.view;
                Actor342000_SetAction(ACTOR_342000_PLAYER_ACTION_WEAPON_CLIP_BLENDED);
                Actor342000_SetMode(ACTOR_342000_STAGING_DOORS_CLOSING);
                arg0->killCountdown = 0;
                arg0->state++;
            }
            func_actor_342000_80162F28(arg0);
            break;
        case 6:
            timer               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = timer;
            if (timer >= 2) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x21;
                arg0->state++;
                break;
            }
            break;
        case 7:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x21;
            arg0->killCountdown                                        = 0;
            arg0->state++;
            break;
        case 8:
            timer               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = timer;
            if (timer >= 0x3C) {
                Actor342000_KillFx();
                Actor342000_SetMode(ACTOR_342000_STAGING_RESTORE_VIEW);
                Actor342000_EnterArea();
                msg.context.loc.stage = gGameSession->location.loc.stage;
                msg.context.loc.area  = gGameSession->location.loc.area;
                msg.command           = 0;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
                arg0->killCountdown = 0;
                arg0->state++;
                break;
            }
            break;
        case 9:
            timer               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = timer;
            if (timer >= 2) {
                Actor342000_SetMode(ACTOR_342000_STAGING_DOORS_SHUT);
                shelterB3GarbageIncineratorSetExitCollisionWalls();
                taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gGameSession->incineratorExitPhase = GAME_SESSION_INCINERATOR_EXIT_ENCOUNTER;
                arg0->state++;
                break;
            }
            break;
        case 10:
            arg0->state++;
            break;
        case 11:
            taskRequestKill(arg0, 0);
            return;
    }
    if ((u32)(arg0->state - 6) < 5U) {
        func_actor_342000_80162BBC(arg0);
        func_actor_342000_80162F28(arg0);
    }
}

/// Fade task of the overlay's task table: its first tick allocates the
/// channel block and seeds every channel at 0xFF; each tick then draws the
/// full-screen fade overlay and steps the channels down by `spawnArg1`, killing
/// the task once `r` has gone negative.
void func_actor_342000_80163EAC(Task* arg0)
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
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            fade->r -= (u16)arg0->spawnArg1.value;
            fade->g -= (u16)arg0->spawnArg1.value;
            fade->b -= (u16)arg0->spawnArg1.value;
            if (fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

static void func_actor_342000_80163F88(Task* task)
{
    _Actor342000GluttonModelWork* work;
    GfxCoord*                     coord;

    coord = task->extra.tmd->coords;
    work  = task->work;

    coord->parent = work->parentCoord;
    taskKill(task);
}

#include "../../shared/actor_messages_draw_mode.inc.c"

#include "../../shared/actor_messages_place_ypr.inc.c"

s32 func_actor_342000_801640C0(Task* arg0, s32 arg1, ActorTransform* transform, s32 arg3)
{
    _Actor342000GluttonModelWork* work;
    GfxCoord*                     coord;

    work                     = arg0->work;
    coord                    = &work->coord;
    coord->coord.t[0]        = transform->pos.vx;
    coord->coord.t[1]        = transform->pos.vy;
    coord->coord.t[2]        = transform->pos.vz;
    work->rotation[0]        = transform->rot.vx;
    work->rotation[1]        = transform->rot.vy;
    work->rotation[2]        = transform->rot.vz;
    work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
}

s32 func_actor_342000_80164110(Task* arg0, s32 arg1, ActorCommand* request, ActorTransform* transform)
{
    _Actor342000GluttonModelWork* work;

    work = arg0->work;
    if (request->command == 0xA) {
        work->scale.vx = transform->pos.vx;
        work->scale.vy = transform->pos.vy;
        work->scale.vz = transform->pos.vz;
    }
    work->lastCommand = request->command;
}

void func_actor_342000_80164154(void)
{
    gGameSession->location.loc.room                            = 7;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 7;
    gGameSession->eventRoomIndex                               = 6;
    gGameSession->incineratorRoomGroup                         = 1;
    gGameSession->roomObjsDirty                                = 1;
    Gp_ApplyAreaRecs(D_shelter_b3_garbage_incinerator_8018FB6C);
}

void func_actor_342000_801641B4(void)
{
    _Actor342000EventWork* work;

    work = D_actor_342000_80165070->work;
    if (work->glutton != NULL) {
        taskCallExit(work->glutton);
    }
    work->glutton = NULL;
}

void func_actor_342000_801641FC(void)
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

void func_actor_342000_80164260(void)
{
    _Actor342000EventWork* work;

    work = D_actor_342000_80165070->work;
    if (work->alertPlayed == 0) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_ALERT, 0, 0);
        work->alertPlayed = 1;
    }
}

void func_actor_342000_801642B4(s16 arg0)
{
    _Actor342000EventWork* work;

    work                   = D_actor_342000_80165070->work;
    work->playerAction     = arg0;
    work->playerActionStep = 0;
}

void func_actor_342000_801642D4(s16 arg0)
{
    _Actor342000EventWork* work;

    work              = D_actor_342000_80165070->work;
    work->stagingMode = arg0;
    work->stagingStep = 0;
}

void func_actor_342000_801642F4(void)
{
    _Actor342000EventWork* work;

    work = D_actor_342000_80165070->work;
    if (work->combatReset == 0) {
        gSceneCombatState.battleRefs                        = 0;
        gSceneCombatState.signals.bytes.endDelayFrames      = 0xF;
        gSceneCombatState.signals.bytes.battlePhase         = SCENE_COMBAT_BATTLE_IDLE;
        gSceneCombatState.signals.bytes.actionFlags         = 0;
        gSceneCombatState.signals.bytes.enemyAlert          = 0;
        gGameSession->flowFlags                            |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0xD;
        work->combatReset                                   = 1;
    }
}

void func_actor_342000_80164364(s32 arg0)
{
    _Actor342000EventWork* work;

    work = D_actor_342000_80165070->work;
    taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, arg0, 0);
}

/// Warps the slot-3 task to the overlay's fixed placement (0x3E9), installs
/// the animation set the current weapon selects (`gPlayerStatus.weapon + 1` for the
/// alternate block, `+ 0x22` for the base one, sent as 0x3E8 to the slot
/// `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` returns), raises 0x3F3, kills the child in
/// `framebufferBlend`, and cancels any pending CD command replacement.
void func_actor_342000_8016439C(void)
{
    _Actor342000EventWork* work;
    AnimationPlayRequest   msg;
    s32                    weaponId;
    s32                    anim;

    work = D_actor_342000_80165070->work;
    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_342000_80164948, 0);
    shelterB3GarbageIncineratorSetExitCollisionWalls();
    weaponId                 = gPlayerStatus.weapon;
    anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.source.index         = anim;
    msg.animationId          = 1;
    msg.blend                = ANIMATION_BLEND_RESET;
    msg.blendFrames          = 0;
    msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &msg, 0);
    taskMessageDispatch(((_Actor342000EventWork*)D_actor_342000_80165070->work)->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    if (work->framebufferBlend != NULL) {
        taskKill(work->framebufferBlend);
        work->framebufferBlend = NULL;
    }
    CdCmd_CancelReplaceAndActivate();
}

/// Script callback: queues the replacement overlay load.
void func_actor_342000_8016447C(void)
{
    cdCmdStageSceneAudioStart();
}

/// Script callback: queues the overlay load.
void func_actor_342000_8016449C(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Script callback: restores the stream random state, then cancels the
/// pending overlay replacement and activates the loaded one.
void func_actor_342000_801644BC(void)
{
    streamFinishScene();
    CdCmd_CancelReplaceAndActivate();
}
