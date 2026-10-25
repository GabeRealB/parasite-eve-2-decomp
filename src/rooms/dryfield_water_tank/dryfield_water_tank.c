#include "types.h"

#include "main/task_types.h"
#include "../../shared/water_tank.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
/// Logical view saved while the singleton mechanism prompt holds player control.
///
/// Stores the live save's unsigned view byte in a word and restores it only
/// after a declined prompt. Requires a pending prompt; accepting hands view
/// control to the prop scene. This storage belongs to the loaded room.
static s32 _gDryfieldWaterTankMechanismPromptSavedView;

/// Live prop-scene driver, published before its child prop is initialized.
///
/// Borrows the task until scene teardown; request/skip callbacks and child
/// reparenting require its allocated work to remain live. Teardown does not
/// clear this handle, so it must not be used after the driver is released.
static Task* _gDryfieldWaterTankPropSceneTask;

#include "rooms/dryfield_water_tank.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "dryfield_water_tank_private.h"

#include "actors/actor_120400.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "overlay.h"
#include "../../shared/screen/screen_fade.h"

static WorldCoordRoomAmbientEntry _gDryfieldWaterTankViewAmbientColors[11];

/// Phases of the prop's slide, held in `_DryfieldWaterTankPropSceneWork::slidePhase`.
enum {
    DRYFIELD_WATER_TANK_PROP_SLIDE_MOVING,   // Advancing in Z towards the end placement, raising dust
    DRYFIELD_WATER_TANK_PROP_SLIDE_SETTLING, // Held at the end placement until the settle count runs out
};

/// Settling-update threshold at which the sliding prop applies its final placement.
///
/// The signed-halfword counter is incremented before comparison. A fresh zero
/// count takes 61 calls; restarting a slide retains any earlier settling count.
#define DRYFIELD_WATER_TANK_PROP_SETTLE_FRAMES 61

/// Work block of each task in the water tank room's prop scene, kept at `Task::work`.
///
/// The scene is two tasks spawned from one table: a driver without a model,
/// which carries out the requests the scene's event script posts, and the prop
/// task whose model slides across the room. Each allocates its own zeroed copy
/// of this block and stores the player task in it; beyond that the driver uses
/// only `propTask` and `request`, and the prop only its matrices and the slide
/// fields, so the remaining members of either copy stay zero.
typedef struct {
    MATRIX lightMtx;     // Prop: light matrix its model is drawn with
    MATRIX colorMtx;     // Prop: colour matrix its model is drawn with
    Task*  playerTask;   // Player task at allocation; the driver hides and shows its model
    Task*  propTask;     // Driver: the scene's prop task, which it spawns
    byte   field_48[4];  // Never accessed; role unproven
    u16    slidePhase;   // Prop: phase of the slide (DRYFIELD_WATER_TANK_PROP_SLIDE_*)
    s16    settleFrames; // Prop: frames spent settling, counted up to DRYFIELD_WATER_TANK_PROP_SETTLE_FRAMES
    u16    request;      // Driver: request to carry out this frame, then cleared (DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_*)
    s16    field_52;     // Cleared whenever a request is posted and never read; role unproven
    s16    field_54;     // Cleared whenever the slide is restarted and never read; role unproven
    byte   field_56[2];  // Never accessed; role unproven
} _DryfieldWaterTankPropSceneWork;
STATIC_ASSERT_SIZEOF(_DryfieldWaterTankPropSceneWork, 0x58);

// Message-table callbacks use the argument views required by this TU.

static void _dryfieldWaterTankSyncMechanismSprites(void);

static WorldCollisionGrid    _gDryfieldWaterTankRoomCollisionGrid;
static WorldCollisionTrigger _gDryfieldWaterTankViewBoundaryTriggers[4];
static WorldCollisionTrigger _gDryfieldWaterTankActionTriggers[9];
static WorldCoordRoomLights  _gDryfieldWaterTankRoomLights;

/// Clip keys and whole-frame blend durations for the scripted player-path scene.
enum {
    DRYFIELD_WATER_TANK_PLAYER_EXTENSION_SET_COUNT      = 19,
    DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK            = 1, // Replaced with the equipped bank by the event interpreter
    DRYFIELD_WATER_TANK_PLAYER_EXIT_STANCE_CLIP         = 1,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP48             = ANIMATION_BANK_BASE_SET_COUNT + 1,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP49             = ANIMATION_BANK_BASE_SET_COUNT + 2,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP50             = ANIMATION_BANK_BASE_SET_COUNT + 3,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP51             = ANIMATION_BANK_BASE_SET_COUNT + 4,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP52             = ANIMATION_BANK_BASE_SET_COUNT + 5,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP53             = ANIMATION_BANK_BASE_SET_COUNT + 6,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP54             = ANIMATION_BANK_BASE_SET_COUNT + 7,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP55             = ANIMATION_BANK_BASE_SET_COUNT + 8,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP56             = ANIMATION_BANK_BASE_SET_COUNT + 9,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP57             = ANIMATION_BANK_BASE_SET_COUNT + 10,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP58             = ANIMATION_BANK_BASE_SET_COUNT + 11,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP59             = ANIMATION_BANK_BASE_SET_COUNT + 12,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP60             = ANIMATION_BANK_BASE_SET_COUNT + 13,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP61             = ANIMATION_BANK_BASE_SET_COUNT + 14,
    DRYFIELD_WATER_TANK_PLAYER_PATH_CLIP62              = ANIMATION_BANK_BASE_SET_COUNT + 15,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP63             = ANIMATION_BANK_BASE_SET_COUNT + 16,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP64             = ANIMATION_BANK_BASE_SET_COUNT + 17,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP65             = ANIMATION_BANK_BASE_SET_COUNT + 18,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_BLEND_FRAMES       = 20,
    DRYFIELD_WATER_TANK_PLAYER_EVENT_SHORT_BLEND_FRAMES = 3,
};

/// Player-path clips, extended ids 47..65; 47 is NULL.
///
/// The script copies twenty entries before playback, one past these nineteen:
/// id 66 receives the following copy request's first word, this list's
/// address, and is never played. Keep the room clips loaded while the equipped
/// bank references them; the skip script only restores a base-bank stance and
/// does not install the extension.
static AnimationSet* _gDryfieldWaterTankPlayerPathSets[DRYFIELD_WATER_TANK_PLAYER_EXTENSION_SET_COUNT] = { NULL, &gDryfieldWaterTankAnimation034BC, &gDryfieldWaterTankAnimation0377C, &gDryfieldWaterTankAnimation03A60, &gDryfieldWaterTankAnimation03CB4, &gDryfieldWaterTankAnimation047BC, &gDryfieldWaterTankAnimation04C98, &gDryfieldWaterTankAnimation04FEC, &gDryfieldWaterTankAnimation051E4, &gDryfieldWaterTankAnimation05704, &gDryfieldWaterTankAnimation059E4, &gDryfieldWaterTankAnimation05DB8, &gDryfieldWaterTankAnimation061A8, &gDryfieldWaterTankAnimation06514, &gDryfieldWaterTankAnimation06840, &gDryfieldWaterTankAnimation04000, &gDryfieldWaterTankAnimation04AA0, &gDryfieldWaterTankAnimation06F48, &gDryfieldWaterTankAnimation06C68 };

/// Installs the player-path clips; reads twenty entries from a nineteen-entry list.
static AnimationBankCopyRequest _gDryfieldWaterTankPlayerAnimationBankCopy = { _gDryfieldWaterTankPlayerPathSets, ARRAY_SIZE(_gDryfieldWaterTankPlayerPathSets) + 1 };

/// Equipped-weapon standing clip restored by both player-path scene exits.
///
/// The interpreter replaces the template bank in a stack copy, then resets
/// base clip 1 with world collision disabled until scripted control ends.
static AnimationPlayRequest _gDryfieldWaterTankPlayerExitStanceAnimation = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EXIT_STANCE_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Event clip 48, used at scene entry and before the first path leg.
///
/// Borrows the installed room clip, blending over twenty frames in the equipped
/// weapon bank with world collision disabled. Its physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimation48 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP48, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Event clip 49, blended after the scene actor's repeated base clip 3.
///
/// Uses the equipped bank's installed room clip, twenty blend frames and no
/// world collision; the clip's physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimation49 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP49, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Event clip 50, followed eight frames later by the scene actor's clip 5.
///
/// Uses the equipped bank's installed room clip, twenty blend frames and no
/// world collision; the clip's physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimation50 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP50, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Event clip 51, played between CAP cues before event clip 50.
///
/// Uses the equipped bank's installed room clip, twenty blend frames and no
/// world collision; the clip's physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimation51 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP51, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Event clip 52, reset after the first 52-frame player path leg.
///
/// Uses the equipped bank's installed room clip with world collision disabled;
/// the script waits for a CAP cue afterwards. Its physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimation52 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Retained, unused requests for player extension clips 53 and 54.
///
/// Both store template bank 1, twenty-frame blends and disabled grid collision.
/// The room installs their clips, but no script consumes these two records;
/// their intended scene use and physical motion are unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimations53And54[2] = {
    { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP53, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP54, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Event clip 55, blended alongside the scene actor's clip 23.
///
/// Uses the equipped bank's installed room clip, twenty blend frames and no
/// world collision; the clip's physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimation55 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP55, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Event clip 56, blended alongside the scene actor's clip 8.
///
/// Uses the equipped bank's installed room clip, twenty blend frames and no
/// world collision; the clip's physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimation56 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP56, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Event clip 57, blended alongside the scene actor's clip 23 before clip 61.
///
/// Uses the equipped bank's installed room clip, twenty blend frames and no
/// world collision; the clip's physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimation57 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP57, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Event clip 58, repeated at two CAP-cue boundaries in the player-path scene.
///
/// Uses the equipped bank's installed room clip, twenty blend frames and no
/// world collision; the clip's physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimation58 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP58, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Retained, unused requests for player extension clips 59 and 60.
///
/// Both store template bank 1, twenty-frame blends and disabled grid collision.
/// The room installs their clips, but no script consumes these two records;
/// their intended scene use and physical motion are unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimations59And60[2] = {
    { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP59, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP60, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Event clip 61, a three-frame blend between the clip-57 and clip-58 cues.
///
/// Uses the equipped bank's installed room clip with world collision disabled;
/// the clip's physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimation61 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP61, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_SHORT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player clip reset at the start of each scripted position-table path leg.
///
/// Equipped-bank clip 62 borrows the installed room animation; collision stays
/// disabled while the path tasks place the player. Its physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerPathAnimation62 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_PATH_CLIP62, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Retained, unused reset request for player extension clip 63.
///
/// Stores template bank 1 and disables grid collision. The room installs the
/// clip, but no script consumes this record; its intended use and motion are
/// unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimation63 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP63, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player extension clip 64, blended at a CAP cue alongside Kyle's clip 25.
///
/// The interpreter replaces template bank 1 in a stack copy with the equipped
/// weapon bank. Blends over three whole frames with grid collision disabled;
/// the installed room clip must remain loaded. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimation64 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP64, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_SHORT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player extension clip 65, blended at the CAP cue after clip 64.
///
/// The interpreter replaces template bank 1 in a stack copy with the equipped
/// weapon bank. Blends over three whole frames with grid collision disabled;
/// the installed room clip must remain loaded. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankPlayerEventAnimation65 = { { .index = DRYFIELD_WATER_TANK_PLAYER_TEMPLATE_BANK }, DRYFIELD_WATER_TANK_PLAYER_EVENT_CLIP65, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_PLAYER_EVENT_SHORT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player's initial world placement for the CAP-cued path scene.
///
/// Position is (2530, -12000, 610) in game units, with yaw -2047 in 4096 units
/// per turn and zero pitch/roll. Consumed synchronously before switching to
/// logical view 5; no placement pointer is retained.
static ActorTransform _gDryfieldWaterTankPlayerPathEntryPlacement = { .pos = { 2530, -12000, 610, 0 }, .rot = { 0, -2047, 0, 0 } };

/// Player-path scene's common normal/skip exit placement in world-coordinate units.
///
/// Both exits place the player here before restoring the weapon stance and
/// logical view 3. Yaw is 960 in 4096 units per turn; pitch and roll are zero.
static ActorTransform _gDryfieldWaterTankPlayerPathExitPlacement = { { 2388, -0x2EE0, 1137, 0 }, { 0, 960, 0, 0 } };

/// Retained, unreferenced words containing 62 and 63; their payload role is unproven.
static s32 D_dryfield_water_tank_80184B60[2] = {
    62,
    63,
};

enum {
    DRYFIELD_WATER_TANK_KYLE_UNUSED_BANK         = 0,
    DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP1        = 1,
    DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP2        = 2,
    DRYFIELD_WATER_TANK_KYLE_UNUSED_BLEND_FRAMES = 20,
};

/// Retained, unused Kyle bank-0 requests for clips 1, 1 and 2.
///
/// The first resets with collision disabled; the others store twenty-frame
/// blends, disabling collision for clip 1 and enabling it for clip 2.
/// Bank 0 indexes Kyle's loaded body clips. These records have no script
/// consumer; their physical motion and intended scene use are unproven.
static const AnimationPlayRequest _gDryfieldWaterTankKyleUnusedAnimations1And2[3] SECTION(".data") = {
    {
        .source               = { .index = DRYFIELD_WATER_TANK_KYLE_UNUSED_BANK },
        .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP1,
        .blend                = ANIMATION_BLEND_RESET,
        .blendFrames          = 0,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = DRYFIELD_WATER_TANK_KYLE_UNUSED_BANK },
        .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP1,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = DRYFIELD_WATER_TANK_KYLE_UNUSED_BLEND_FRAMES,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
    },
    {
        .source               = { .index = DRYFIELD_WATER_TANK_KYLE_UNUSED_BANK },
        .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP2,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = DRYFIELD_WATER_TANK_KYLE_UNUSED_BLEND_FRAMES,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
    },
};

/// Kyle's bank-0 clip keys and whole-frame blend durations for the player-path scene.
enum {
    DRYFIELD_WATER_TANK_KYLE_EVENT_BANK                = 0,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP3               = 3,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP5               = 5,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP7               = 7,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP8               = 8,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP17              = 17,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP18              = 18,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP19              = 19,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP20              = 20,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP21              = 21,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP22              = 22,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP23              = 23,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP24              = 24,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP25              = 25,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP26              = 26,
    DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP28              = 28,
    DRYFIELD_WATER_TANK_KYLE_EVENT_BLEND_FRAMES        = 20,
    DRYFIELD_WATER_TANK_KYLE_EVENT_SHORT_BLEND_FRAMES  = 2,
    DRYFIELD_WATER_TANK_KYLE_EVENT_MEDIUM_BLEND_FRAMES = 8,
};

/// Kyle's bank-0 event clip 3 for the player-path scene.
///
/// Repeated twice, before player clips 56 and 49. Resets playback on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation3 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

enum {
    DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP4 = 4,
};

/// Retained, unused Kyle bank-0 blend request for clip 4.
///
/// Stores a 8-frame blend and enabled collision, which Kyle ignores.
/// Bank 0 indexes Kyle's loaded body clips. These records have no script
/// consumer; their physical motion and intended scene use are unproven.
static const AnimationPlayRequest _gDryfieldWaterTankKyleUnusedAnimation4 SECTION(".data") = {
    .source               = { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK },
    .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP4,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_WATER_TANK_KYLE_EVENT_MEDIUM_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Kyle's bank-0 event clip 5 for the player-path scene.
///
/// Repeated eight updates after player clip 50. Blends over 8 whole frames on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation5 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP5, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_KYLE_EVENT_MEDIUM_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

enum {
    DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP6 = 6,
};

/// Retained, unused Kyle bank-0 blend request for clip 6.
///
/// Stores a 20-frame blend and enabled collision, which Kyle ignores.
/// Bank 0 indexes Kyle's loaded body clips. These records have no script
/// consumer; their physical motion and intended scene use are unproven.
static const AnimationPlayRequest _gDryfieldWaterTankKyleUnusedAnimation6 SECTION(".data") = {
    .source               = { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK },
    .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP6,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_WATER_TANK_KYLE_EVENT_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Kyle's bank-0 event clip 7 for the player-path scene.
///
/// Played after Kyle clip 17. Blends over 20 whole frames on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation7 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP7, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_KYLE_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Kyle's bank-0 event clip 8 for the player-path scene.
///
/// Repeated alongside player clips 56 and 49. Resets playback on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation8 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

enum {
    DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP9  = 9,
    DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP10 = 10,
    DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP11 = 11,
    DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP12 = 12,
    DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP13 = 13,
    DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP14 = 14,
    DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP15 = 15,
    DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP16 = 16,
};

/// Retained, unused Kyle bank-0 blend requests for clips 9..16.
///
/// All eight store twenty-frame blends and enabled collision, which Kyle
/// ignores. Bank 0 indexes Kyle's loaded body clips. These records have no script
/// consumer; their physical motion and intended scene use are unproven.
static const AnimationPlayRequest _gDryfieldWaterTankKyleUnusedAnimations9To16[8] SECTION(".data") = {
    {
        .source               = { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK },
        .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP9,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = DRYFIELD_WATER_TANK_KYLE_EVENT_BLEND_FRAMES,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
    },
    {
        .source               = { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK },
        .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP10,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = DRYFIELD_WATER_TANK_KYLE_EVENT_BLEND_FRAMES,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
    },
    {
        .source               = { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK },
        .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP11,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = DRYFIELD_WATER_TANK_KYLE_EVENT_BLEND_FRAMES,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
    },
    {
        .source               = { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK },
        .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP12,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = DRYFIELD_WATER_TANK_KYLE_EVENT_BLEND_FRAMES,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
    },
    {
        .source               = { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK },
        .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP13,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = DRYFIELD_WATER_TANK_KYLE_EVENT_BLEND_FRAMES,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
    },
    {
        .source               = { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK },
        .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP14,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = DRYFIELD_WATER_TANK_KYLE_EVENT_BLEND_FRAMES,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
    },
    {
        .source               = { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK },
        .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP15,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = DRYFIELD_WATER_TANK_KYLE_EVENT_BLEND_FRAMES,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
    },
    {
        .source               = { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK },
        .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP16,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = DRYFIELD_WATER_TANK_KYLE_EVENT_BLEND_FRAMES,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
    },
};

/// Kyle's bank-0 event clip 17 for the player-path scene.
///
/// Played between Kyle clips 22 and 7. Blends over 20 whole frames on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation17 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP17, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_KYLE_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Kyle's bank-0 event clip 18 for the player-path scene.
///
/// Played at the CAP cue after scene entry clip 19. Blends over 2 whole frames on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation18 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP18, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_KYLE_EVENT_SHORT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Kyle's bank-0 event clip 19 for the player-path scene.
///
/// Reset at scene entry beside player clip 48. Resets playback on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation19 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Kyle's bank-0 event clip 20 for the player-path scene.
///
/// Played after Kyle is shown again. Blends over 2 whole frames on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation20 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP20, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_KYLE_EVENT_SHORT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Kyle's bank-0 event clip 21 for the player-path scene.
///
/// Played at the CAP cue before Kyle is hidden. Blends over 20 whole frames on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation21 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP21, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_KYLE_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Kyle's bank-0 event clip 22 for the player-path scene.
///
/// Played at the CAP cue before clip 17. Blends over 2 whole frames on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation22 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP22, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_KYLE_EVENT_SHORT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Kyle's bank-0 event clip 23 for the player-path scene.
///
/// Played alongside player clip 55. Blends over 2 whole frames on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation23 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP23, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_KYLE_EVENT_SHORT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Kyle's bank-0 event clip 24 for the player-path scene.
///
/// Played at the CAP cue before the second clip-3 sequence. Blends over 2 whole frames on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation24 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP24, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_KYLE_EVENT_SHORT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Kyle's bank-0 event clip 25 for the player-path scene.
///
/// Repeated alongside player clips 57 and 64. Blends over 2 whole frames on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation25 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP25, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_KYLE_EVENT_SHORT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Kyle's bank-0 event clip 26 for the player-path scene.
///
/// Repeated before clip 28 and before sound cue 14. Blends over 2 whole frames on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation26 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP26, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_KYLE_EVENT_SHORT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

enum {
    DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP27 = 27,
};

/// Retained, unused Kyle bank-0 blend request for clip 27.
///
/// Stores a 2-frame blend and enabled collision, which Kyle ignores.
/// Bank 0 indexes Kyle's loaded body clips. These records have no script
/// consumer; their physical motion and intended scene use are unproven.
static const AnimationPlayRequest _gDryfieldWaterTankKyleUnusedAnimation27 SECTION(".data") = {
    .source               = { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK },
    .animationId          = DRYFIELD_WATER_TANK_KYLE_UNUSED_CLIP27,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_WATER_TANK_KYLE_EVENT_SHORT_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Kyle's bank-0 event clip 28 for the player-path scene.
///
/// Played at the CAP cue before player clip 61. Blends over 2 whole frames on the live
/// placement-index-0 actor; the stored collision choice is ignored by Kyle.
/// The bank and clip remain borrowed during playback. Physical motion is unproven.
static AnimationPlayRequest _gDryfieldWaterTankKyleEventAnimation28 = { { .index = DRYFIELD_WATER_TANK_KYLE_EVENT_BANK }, DRYFIELD_WATER_TANK_KYLE_EVENT_CLIP28, ANIMATION_BLEND_INTERPOLATE, DRYFIELD_WATER_TANK_KYLE_EVENT_SHORT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Kyle's initial placement facing +X in the player-path scene.
///
/// Position is (2530, -12000, -640) in world game units; yaw is a quarter-turn
/// in 4096 units per turn. Sent synchronously to placement index 0 before
/// logical view 5 is selected; the loaded actor must be live.
static ActorTransform _gDryfieldWaterTankKylePathSceneEntryPlacement = { .pos = { 2530, -12000, -640, 0 }, .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Kyle's placement facing +Z before the path scene's repeated clip-3 sequence.
///
/// Keeps the initial world position (2530, -12000, -640), with all Euler angles
/// zero. The placement-index-0 actor consumes the transform synchronously.
static ActorTransform _gDryfieldWaterTankKylePathSceneForwardPlacement = { .pos = { 2530, -12000, -640, 0 }, .rot = { 0, 0, 0, 0 } };

/// Kyle's world destination and final orientation during the player-path scene.
///
/// The walk message copies (2530, -12000, 10000) in game units and zero angles;
/// final yaw is in 4096 units per turn. No optional animation pair is supplied,
/// so Kyle uses his default start/closing clips 16 and 1. The target pointer
/// need only survive synchronous dispatch to placement index 0.
static ActorTransform _gDryfieldWaterTankKylePathSceneWalkTarget = { .pos = { 2530, -12000, 10000, 0 }, .rot = { 0, 0, 0, 0 } };

TaskDesc gDryfieldWaterTankPlayerPathTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, dryfieldWaterTankMovePlayerFirstLegTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, dryfieldWaterTankMovePlayerSecondLegTask, { .value = 0 } },
};

/// Selectors and timing of the CAP-cued player/Kyle path scene.
enum {
    DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX    = 0,
    DRYFIELD_WATER_TANK_PATH_SCENE_CAP_COMMAND             = 13,
    DRYFIELD_WATER_TANK_PATH_SCENE_ENTRY_VIEW              = 5,
    DRYFIELD_WATER_TANK_PATH_SCENE_EXIT_VIEW               = 3,
    DRYFIELD_WATER_TANK_PATH_SCENE_MUSIC_EVENT             = 4,
    DRYFIELD_WATER_TANK_PATH_SCENE_FADE_FRAMES             = 30,
    DRYFIELD_WATER_TANK_PATH_SCENE_MUSIC_FADE_TICKS        = 30,
    DRYFIELD_WATER_TANK_PATH_SCENE_AREA_MUSIC_FADE_TICKS   = 1,
    DRYFIELD_WATER_TANK_PATH_SCENE_SKIP_WAIT_FRAMES        = 8,
    DRYFIELD_WATER_TANK_PATH_SCENE_ANIMATION50_WAIT_FRAMES = 8,
    DRYFIELD_WATER_TANK_PATH_SCENE_FIRST_LEG_DELAY_FRAMES  = 40,
    DRYFIELD_WATER_TANK_PATH_SCENE_LEG_FRAMES              = 52,
    DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_WALK_DELAY_FRAMES  = 26,
    DRYFIELD_WATER_TANK_PATH_SCENE_FINAL_WAIT_FRAMES       = 1,
    DRYFIELD_WATER_TANK_PATH_SCENE_FIRST_LEG               = 0,
    DRYFIELD_WATER_TANK_PATH_SCENE_SECOND_LEG              = 1,
    DRYFIELD_WATER_TANK_PATH_SCENE_YIELD_FRAMES            = 0,
    DRYFIELD_WATER_TANK_PATH_SCENE_SOUND10                 = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 10),
    DRYFIELD_WATER_TANK_PATH_SCENE_SOUND11                 = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 11),
    DRYFIELD_WATER_TANK_PATH_SCENE_SOUND12                 = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 12),
    DRYFIELD_WATER_TANK_PATH_SCENE_SOUND13                 = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 13),
    DRYFIELD_WATER_TANK_PATH_SCENE_SOUND14                 = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 14),
    DRYFIELD_WATER_TANK_PATH_SCENE_SOUND15                 = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 15),
    DRYFIELD_WATER_TANK_PATH_SCENE_SOUND16                 = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 16),
};

/// CAP-cued player/Kyle scene with two scripted player path legs and a common exit.
///
/// Direction action 2 latches story progress and starts these 126 commands with
/// HUD held and player control suspended. Installs player extension clips,
/// addresses Kyle at placement index 0, and starts CAP command 13. Animation
/// IDs are bank keys; their physical motions are unproven. Zero-frame waits
/// deliberately yield so model tasks apply requests before the next sound.
///
/// Both exits restore the player placement, weapon stance, view 3 and scene
/// music event 4, hide Kyle and end scripted control. Normal playback disables
/// skipping before its final reveal. The interpreter borrows this END-terminated
/// array, its payloads and loaded room/actor/weapon/CAP resources until completion.
static EvsCommand _gDryfieldWaterTankPlayerPathSceneScript[126] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldWaterTankPlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldWaterTankPlayerPathEntryPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = true }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldWaterTankKylePathSceneEntryPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_ENTRY_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation19 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_YIELD_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_SOUND10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_AREA_MUSIC_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldWaterTankKylePathSceneForwardPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_YIELD_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_SOUND11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation56 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_YIELD_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_SOUND12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation5 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation57 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation25 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation26 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation28 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation61 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation21 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = false }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation58 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = true }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_YIELD_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_SOUND13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation22 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation17 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation7 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation64 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation25 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation65 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation55 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation23 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation58 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation26 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_YIELD_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_SOUND14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_YIELD_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_SOUND15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_SOUND16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation51 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_ANIMATION50_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gDryfieldWaterTankKyleEventAnimation5 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_FIRST_LEG_DELAY_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = dryfieldWaterTankSpawnPlayerPathTask }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_FIRST_LEG }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerPathAnimation62 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_LEG_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerEventAnimation52 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gDryfieldWaterTankKylePathSceneWalkTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_WALK_DELAY_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = dryfieldWaterTankSpawnPlayerPathTask }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_SECOND_LEG }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerPathAnimation62 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_FINAL_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_MUSIC_EVENT }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_MUSIC_FADE_TICKS }, { .value = 30 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldWaterTankPlayerPathExitPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerExitStanceAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = false }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_EXIT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Fade-covered restoration when the player/Kyle path scene is skipped.
///
/// These fourteen END-terminated commands restore the common player placement,
/// weapon stance, scene music event 4 and view 3, then hide Kyle, abort CAP and
/// clean up scene presentation before ending scripted control. Eight-update
/// waits surround restoration/reveal. Does not install extension clips or run
/// either path leg; the interpreter borrows room payloads through completion.
static EvsCommand _gDryfieldWaterTankPlayerPathSceneSkipScript[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_MUSIC_EVENT }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_MUSIC_FADE_TICKS }, { .value = 30 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_SKIP_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldWaterTankPlayerPathExitPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldWaterTankPlayerExitStanceAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_KYLE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = false }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_EXIT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_WATER_TANK_PATH_SCENE_SKIP_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Skeleton[1] = {
#include "assets/dryfield_night_water_tank_model_00FF8_skeleton.inc"
};

static u32 _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8PartVerts[1] = {
#include "assets/dryfield_night_water_tank_model_00FF8_partVerts.inc"
};

static SVECTOR _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Verts[84] = {
#include "assets/dryfield_night_water_tank_model_00FF8_verts.inc"
};

static SVECTOR _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Normals[72] = {
#include "assets/dryfield_night_water_tank_model_00FF8_normals.inc"
};

static u32 _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Stream[531] = {
#include "assets/dryfield_night_water_tank_model_00FF8_stream.inc"
};

static TmdSource _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8 = {
    0,
    3768,
    0,
    1,
    _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8PartVerts,
    _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Verts,
    _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Normals,
    _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Skeleton,
    _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Stream,
};

/// Spawn recipe for the room's swaying tank model, followed by TASK_DESC_END.
///
/// Entry 0 uses a TMD body at priority `TASK_PRIORITY_CONTROL` and the shared waterTank sway task.
/// Room initialization spawns it with zero arguments; the task borrows the
/// room's one-part model geometry until teardown. Never spawn the final row.
static TaskDesc _gDryfieldWaterTankSwayTaskDescs[2] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, waterTankSwayTask, { .model = &_gDryfieldWaterTankDryfieldNightWaterTankModel00FF8 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gWaterTankYaw = 0;

s32 gWaterTankYawSpeed = 0;

s32 gWaterTankYawAcceleration = 0;

s32 gWaterTankYawTargetAcceleration = 0;

u16 gDryfieldWaterTankViewEffectGates[DRYFIELD_WATER_TANK_MAPPED_VIEW_COUNT] = {
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_DISABLED,
};

WorldCollisionRoomResources gDryfieldWaterTankRoomCollisionResources[1] = {
    { &_gDryfieldWaterTankRoomCollisionGrid, _gDryfieldWaterTankViewBoundaryTriggers, _gDryfieldWaterTankActionTriggers, NULL },
};

u8* gDryfieldWaterTankRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldWaterTankRoomViewCounts[2] = { DRYFIELD_WATER_TANK_MAPPED_VIEW_COUNT, 0 };

WorldCoordRoomLighting gDryfieldWaterTankRoomLighting[1] = {
    { .lights = &_gDryfieldWaterTankRoomLights, .ambientTable = _gDryfieldWaterTankViewAmbientColors },
};

DirectionWarpEntry gDryfieldWaterTankWarpEntries[2] = {
    { { { .word = 512 }, -2327, -0x2EDF, 1143 }, { 0, 0, 0, 0 }, { { .word = 512 }, -2327, -0x2EDF, 1143 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2560 }, 870, -0x4010, 934 }, { 0, 0, 0, 0 }, { { .word = 2560 }, 870, -0x4010, 934 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-space Q12 unit normals indexed by the room collision faces.
static SVECTOR _gDryfieldWaterTankRoomCollisionGridNormals[14] = {
#include "assets/dryfield_water_tank_collision_098FC_normals.inc"
};

/// Room collision vertices in signed game-coordinate units.
static SVECTOR _gDryfieldWaterTankRoomCollisionGridVerts[70] = {
#include "assets/dryfield_water_tank_collision_098FC_verts.inc"
};

/// Room collision quads indexing the vertex/normal pools and surface classes 0, 1, 3..5.
static WorldCollisionGridFace _gDryfieldWaterTankRoomCollisionGridFaces[36] = {
#include "assets/dryfield_water_tank_collision_098FC_faces.inc"
};

/// Four packed face-index lists, each ending in `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gDryfieldWaterTankRoomCollisionGridCells[118] = {
#include "assets/dryfield_water_tank_collision_098FC_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gDryfieldWaterTankRoomCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define DRYFIELD_WATER_TANK_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldWaterTankRoomCollisionGridCells[(halfwordOffset)])
/// X-major pointers to the room grid's four cell lists, with a Z stride of two.
static s16* _gDryfieldWaterTankRoomCollisionGridTable[4] = {
#include "assets/dryfield_water_tank_collision_098FC_table.inc"
};
#undef DRYFIELD_WATER_TANK_CELL_FACE_LIST_AT_OFFSET

/// Room 1's collision mesh, indexed by a 2x2 XZ grid of 4000-unit cells.
///
/// Activation borrows the current view coordinate. The room owns the mesh and
/// cell lists for its loaded lifetime; its origin is X=-3500, Z=-3300.
static WorldCollisionGrid _gDryfieldWaterTankRoomCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldWaterTankRoomCollisionGridNormals,
    .vertices    = _gDryfieldWaterTankRoomCollisionGridVerts,
    .faces       = _gDryfieldWaterTankRoomCollisionGridFaces,
    .cellFaceIds = _gDryfieldWaterTankRoomCollisionGridTable,
    .xBias       = 3500,
    .zBias       = 3300,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldWaterTankRoomCollisionGridFaces),
};

ViewCamera gDryfieldWaterTankViewCameras[DRYFIELD_WATER_TANK_MAPPED_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x5DC0, 0 } }, 322 },
    { { { { 1429, 0, -3838 }, { -2523, 3086, -939 }, { 2892, 2692, 1077 } }, { 5050, 0x3E4E, 1880 } }, 257 },
    { { { { -853, 0, 4006 }, { 1212, 3903, 258 }, { -3818, 1239, -813 } }, { -5920, 0x37FA, -1350 } }, 257 },
    { { { { -1180, 0, 3922 }, { 173, 4092, 52 }, { -3918, 180, -1179 } }, { -4220, 0x42EA, -1350 } }, 230 },
    { { { { -2338, 0, 3362 }, { 0, ONE, 0 }, { -3362, 0, -2338 } }, { -6615, 0x324B, -2870 } }, 541 },
    { { { { -4013, 0, 817 }, { 0, ONE, 0 }, { -817, 0, -4013 } }, { -3050, 0x348A, -2000 } }, 680 },
    { { { { 4022, 0, 773 }, { 0, ONE, 0 }, { -773, 0, 4022 } }, { -3000, 0x348A, 1950 } }, 680 },
    { { { { -1032, 0, 3963 }, { 0, ONE, 0 }, { -3963, 0, -1032 } }, { -3180, 0x335E, -310 } }, 257 },
    { { { { 3261, 0, 2478 }, { 0, ONE, 0 }, { -2478, 0, 3261 } }, { -4900, 1150, 2500 } }, 257 },
    { { { { -2401, 0, 3317 }, { 3202, 1072, 2318 }, { -868, 3953, -628 } }, { 572, 0x42B6, 1557 } }, 380 },
};

/// View 1's empty sprite list; the zero-count header keeps decoded background strips.
static SpriteBatch _gDryfieldWaterTankView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's 31 textured foreground rectangles, in view-centred pixels and OT depth units.
static SpriteSource _gDryfieldWaterTankView2SpriteSources[31] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -120, 3693, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -96, -120, 1131, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -88, -120, 1028, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -88, -64, 1355, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -72, -120, 990, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -72, -48, 1277, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -56, -120, 954, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -56, -48, 1191, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, -120, 1123, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, -120, 1128, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, -120, 1045, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -56, 1416, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 56, -120, 992, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 56, -48, 1304, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 40, -120, 954, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 40, -48, 1228, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -144, 8, 1000, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, 16, 1000, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, 16, 875, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, 8, 875, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -64, 16, 875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -40, 40, 875, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -24, 48, 875, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 64, 875, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 64, 875, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, 40, 875, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 32, 750, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, 24, 875, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 16, 875, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 8, 875, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 0, 875, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
};

/// View 2's sprite ranges [0,16) and [16,31), following the background header.
///
/// The final record terminates the list; source ranges count elements, not bytes.
static SpriteBatch _gDryfieldWaterTankView2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, ARRAY_SIZE(_gDryfieldWaterTankView2SpriteSources) - 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 88 foreground rectangles, including its final post-operation sprite.
///
/// Positions are view-centred pixels; depth is an unscaled OT sorting value.
static SpriteSource _gDryfieldWaterTankView3SpriteSources[88] = {
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -96, -120, 1275, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -88, -120, 1250, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -120, 1125, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -120, 1125, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -120, 1250, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 72, -120, 1250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -120, 1250, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -120, 1250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -120, 1250, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, -120, 1047, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -48, -120, 1000, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -32, -120, 1000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, -120, 1000, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 0, -120, 1000, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, -120, 1000, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 32, -120, 1000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 0, 1500, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -88, -56, 1250, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -80, 0, 1500, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, 0, 1375, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, 0, 1375, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, 0, 1375, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -72, 0, 1500, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -64, -56, 1250, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -56, 1250, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, 0, 1375, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -80, -56, 1225, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -56, 1250, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -56, 1125, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 40, -56, 1127, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 48, -56, 1150, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, -56, 1250, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, -56, 1250, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 72, -56, 1250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, -56, 1375, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -32, -56, 1375, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -16, -56, 1125, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 0, -56, 1125, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 16, -56, 1125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, 0, 1375, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, 0, 1375, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 32, 0, 1375, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 40, 0, 1375, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 48, 0, 1500, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 56, 0, 1500, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 64, 0, 1500, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 0, 1500, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -32, 0, 1375, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -16, 0, 1375, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 0, 0, 1375, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 24, 750, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -152, -24, 900, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -144, -24, 875, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -136, 8, 750, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 8, 750, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 40, 750, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 8, 750, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -112, 40, 750, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -96, 16, 750, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 64, 750, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -88, 16, 750, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 48, 750, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, 48, 750, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, 48, 750, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, 16, 750, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, 16, 750, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 56, 750, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -16, 16, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 24, 750, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 24, 750, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, 24, 776, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, 56, 750, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, 56, 750, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 56, 750, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, 16, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 48, 750, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 64, 16, 750, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 8, 750, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 0, 750, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 112, 0, 750, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 128, 0, 750, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 136, -8, 750, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 144, -8, 750, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 24, 750, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 144, 16, 750, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 40, 750, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 32, 750, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 56 } }, -48, -40, 1125, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
};

/// View 3's ranges [0,50), [50,87) and [87,88), following the background header.
///
/// Batch 3 is hidden before the tank mechanism is operated and shown afterwards.
/// Cached packet allocation retains all 88 sources regardless of visibility.
static SpriteBatch _gDryfieldWaterTankView3SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 50, 0, 0, { 1, 0 } },
    { 50, 37, 0, 0, { 2, 0 } },
    { 87, ARRAY_SIZE(_gDryfieldWaterTankView3SpriteSources) - 87, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's ten foreground rectangles, in view-centred pixels and OT depth units.
static SpriteSource _gDryfieldWaterTankView4SpriteSources[10] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, 8, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 8, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 64, 8, 625, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 56, 24, 625, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 56, 625, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 24, 625, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, 72, 625, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 104, 625, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 40, 80, 625, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 96, 625, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
};

/// View 4's complete foreground range, between the background header and end record.
static SpriteBatch _gDryfieldWaterTankView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldWaterTankView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's 25 foreground rectangles, in view-centred pixels and OT depth units.
static SpriteSource _gDryfieldWaterTankView5SpriteSources[25] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, -24, 500, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 32, 500, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -136, -24, 500, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -24, 500, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -24, 500, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, 16, 500, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 32, 500, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -88, -24, 500, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, -24, 500, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -48, -24, 500, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -16, -24, 500, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, 32, 500, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -48, 32, 500, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -16, 32, 500, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 136 } }, 16, -24, 500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -24, 500, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, -32, 500, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, -32, 500, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, -32, 500, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -32, 500, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 32, 40, 500, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 64, 40, 500, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 40, 500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, 40, 500, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 40, 500, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
};

/// View 5's complete foreground range, between the background header and end record.
static SpriteBatch _gDryfieldWaterTankView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldWaterTankView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's empty sprite list; the zero-count header keeps decoded background strips.
static SpriteBatch _gDryfieldWaterTankView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's empty sprite list; the zero-count header keeps decoded background strips.
static SpriteBatch _gDryfieldWaterTankView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's pre-operation mechanism sprite: an 88x240-pixel rectangle at depth 375.
///
/// Its drawing batch is shown before the mechanism is operated and hidden afterwards.
static SpriteSource _gDryfieldWaterTankView8SpriteSources[1] = {
    { 143, 0x3FC0, { .fields = { 88, 240 } }, -40, -120, 375, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

/// View 8's complete pre-operation mechanism range and decoded-background header.
///
/// Batch 1 is shown before operation and hidden afterwards; the terminal row
/// contributes no sprites. Cached allocation retains the source while hidden.
static SpriteBatch _gDryfieldWaterTankView8SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldWaterTankView8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9's 27 foreground rectangles, in view-centred pixels and OT depth units.
///
/// Four batches partition the complete array; cached packet setup snapshots
/// texture geometry while depth remains readable each frame. There is no sentinel.
static SpriteSource _gDryfieldWaterTankView9SpriteSources[27] = {
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 40, -88, 750, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 40, 750, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -16, 750, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, 32, 750, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 56, -80, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 0, 750, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 32, 750, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, 24, 750, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 0, 750, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, -72, 750, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -72, 750, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -32, 750, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 16, 750, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 0, -48, 1875, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 0, 0, 1875, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, 0, 1875, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -48, 1875, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, -48, 1875, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, 0, 1875, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 48, -8, 1875, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -56, 1875, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -80, -120, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -80, 0, 250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -64, 0, 250, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -64, -120, 250, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -48, -120, 250, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -48, 0, 250, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
};

/// View 9's ranges [0,10), [10,13), [13,21) and [21,27), after its background header.
///
/// All four ranges are initially visible and retained in the cached buffers.
/// The final SPRITE_BATCH_END row terminates traversal without a sprite payload.
static SpriteBatch _gDryfieldWaterTankView9SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 3, 0 } },
    { 10, 3, 0, 0, { 0, 0 } },
    { 13, 8, 0, 0, { 2, 0 } },
    { 21, ARRAY_SIZE(_gDryfieldWaterTankView9SpriteSources) - 21, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 10's empty foreground list, retaining decoded background strips.
///
/// The zero-count header is nonterminal; the second row ends the batch scan.
static SpriteBatch _gDryfieldWaterTankView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldWaterTankSpriteViews[DRYFIELD_WATER_TANK_MAPPED_VIEW_COUNT] = {
    { { .empty = _gDryfieldWaterTankView1SpriteBatches }, _gDryfieldWaterTankView1SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTankView2SpriteSources }, _gDryfieldWaterTankView2SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTankView3SpriteSources }, _gDryfieldWaterTankView3SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTankView4SpriteSources }, _gDryfieldWaterTankView4SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTankView5SpriteSources }, _gDryfieldWaterTankView5SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterTankView6SpriteBatches }, _gDryfieldWaterTankView6SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterTankView7SpriteBatches }, _gDryfieldWaterTankView7SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTankView8SpriteSources }, _gDryfieldWaterTankView8SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterTankView9SpriteSources }, _gDryfieldWaterTankView9SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterTankView10SpriteBatches }, _gDryfieldWaterTankView10SpriteBatches, NULL },
};

/// Four mutable view boundaries, switching between logical views 2 and 3.
///
/// Opposite-facing pairs cover two room locations. Coordinates use room units,
/// normals use Q12, and the last live record carries WORLD_COLLISION_TRIGGER_LAST.
/// Room setup binds and links them; their storage must remain live until unlinked.
static WorldCollisionTrigger _gDryfieldWaterTankViewBoundaryTriggers[4] = {
    { NULL, NULL, NULL, { -864, -0x32D0, 2464, 0 }, { { 0, -1968, -1024, 0 }, { 0, -1968, 1024, 0 }, { 0, 1968, -1024, 0 }, { 0, 1968, 1024, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2217, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -576, -0x3310, 2464, 0 }, { { 0, -1904, 1024, 0 }, { 0, -1904, -1024, 0 }, { 0, 1904, 1024, 0 }, { 0, 1904, -1024, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2157, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 768, -0x32D0, -2368, 0 }, { { 0, -1936, 1024, 0 }, { 0, -1936, -1024, 0 }, { 0, 1936, 1024, 0 }, { 0, 1936, -1024, 0 } }, { -4103, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2187, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 544, -0x3330, -2336, 0 }, { { 0, -1904, -1024, 0 }, { 0, -1904, 1024, 0 }, { 0, 1904, -1024, 0 }, { 0, 1904, 1024, 0 } }, { 4098, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2157, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room 1's 21 white Q12 point lights, contributing in every view.
///
/// Positions and falloff radii use world units: full strength within 1000,
/// fading to zero at 3000. Gameplay updates their parents, composition caches
/// and attenuation while the room overlay remains loaded.
static WorldCoordPointLight _gDryfieldWaterTankRoomPointLights[21] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -0x36B0, -1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -0x36B0, -3000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -0x36B0, -3000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2000, -0x36B0, -2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -0x36B0, -3000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2000, -0x36B0, -2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3000, -0x36B0, -1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -0x36B0, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -0x36B0, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1500, -0x36B0, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -0x36B0, 2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -0x36B0, 2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -0x36B0, 2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1500, -0x36B0, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3000, -0x36B0, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3000, -0x36B0, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 61, -0x4650, 1039 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1960, -0x4650, 520 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2340, -0x4718, -1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 582, -0x4650, -880 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -0x4CCC, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
};

/// Room 1's authored light collection, containing point lights only.
static WorldCoordRoomLights _gDryfieldWaterTankRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldWaterTankRoomPointLights),
    .pointLights           = _gDryfieldWaterTankRoomPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Action and resource keys authored by the room's trigger and variant tables.
enum {
    DRYFIELD_WATER_TANK_TRIGGER_MOVIE_EVENT       = 1,
    DRYFIELD_WATER_TANK_TRIGGER_PLAYER_PATH_SCENE = 2,
    DRYFIELD_WATER_TANK_TRIGGER_PLACE_IN_VIEW4    = 3,
    DRYFIELD_WATER_TANK_TRIGGER_PLACE_IN_VIEW3    = 4,
    DRYFIELD_WATER_TANK_TRIGGER_CAP_COMMAND12     = 12,
    DRYFIELD_WATER_TANK_TO_WATER_TOWER_WARP_SLOTS = (1 << 4) | 3,
    DRYFIELD_WATER_TANK_KYLE_RESOURCE_ENTRY       = 140,
    DRYFIELD_WATER_TANK_KYLE_RESOURCE_FILE        = 204,
};

// CAP uses the same command slot for the prompt and its room-message request.
enum { DRYFIELD_WATER_TANK_MECHANISM_CAP_COMMAND = 14 };

/// Nine mutable warp, CAP and scene-action quads for the water-tank room.
///
/// Includes departure slot 1 to water-tower arrival slot 3, automatic movie/path
/// scene actions, CAP commands 12/14, and paired player-placement actions 3/4.
/// The last live quad carries WORLD_COLLISION_TRIGGER_LAST. Room binding supplies
/// coordinates and list links; these records must stay loaded until unlinked.
static WorldCollisionTrigger _gDryfieldWaterTankActionTriggers[9] = {
    { NULL, NULL, NULL, { -2449, -0x2F60, 640, 0 }, { { -728, 0, -256, 0 }, { 336, 0, -584, 0 }, { -624, 0, 680, 0 }, { 1016, 0, 160, 0 } }, { 0, ONE, 0, 0 }, { 401, 0, 4076, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_WATER_TOWER, DRYFIELD_WATER_TANK_TO_WATER_TOWER_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2512, -0x2F20, 720, 0 }, { { -840, 0, -144, 0 }, { 896, 0, -152, 0 }, { -832, 0, 120, 0 }, { 776, 0, 176, 0 } }, { 0, 4100, 0, 0 }, { 401, 0, 4076, 0 }, 907, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_WATER_TANK_TRIGGER_MOVIE_EVENT, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -1668, -0x4040, -1586, 0 }, { { 1321, 0, -1157, 0 }, { 1703, 0, 329, 0 }, { -1148, 0, 1191, 0 }, { 396, 0, 1526, 0 } }, { 0, 4115, 0, 0 }, { 401, 0, 4076, 0 }, 1755, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_WATER_TANK_TRIGGER_CAP_COMMAND12, CAP_EVENT_PAUSE_ACTORS, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1432, -0x2F23, -17, 0 }, { { 159, 0, 627, 0 }, { 157, 0, -595, 0 }, { 1387, 0, 451, 0 }, { 1371, 0, -481, 0 } }, { 0, 4107, 0, 0 }, { 4094, 0, 0, 0 }, 1453, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_WATER_TANK_MECHANISM_CAP_COMMAND, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -656, -0x2F21, 2160, 0 }, { { -841, 0, 1091, 0 }, { -619, 0, -1091, 0 }, { 611, 0, 1171, 0 }, { 851, 0, -1169, 0 } }, { 0, 4100, 0, 0 }, { 4094, 0, 0, 0 }, 1442, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_WATER_TANK_TRIGGER_PLAYER_PATH_SCENE, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 1031, -0x2F40, 1303, 0 }, { { -269, 0, 617, 0 }, { 667, 0, -339, 0 }, { 261, 0, 1577, 0 }, { 1711, 0, 194, 0 } }, { 0, 4111, 0, 0 }, { 2750, 0, 3035, 0 }, 1717, WORLD_COLLISION_TRIGGER_ACTION_ROOM, DRYFIELD_WATER_TANK_TRIGGER_PLACE_IN_VIEW4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1255, -0x4040, 1247, 0 }, { { 668, 0, -496, 0 }, { -403, 0, 647, 0 }, { 39, 0, -1272, 0 }, { -1328, 0, 97, 0 } }, { 0, 4103, 0, 0 }, { -3166, 0, -2599, 0 }, 1330, WORLD_COLLISION_TRIGGER_ACTION_ROOM, DRYFIELD_WATER_TANK_TRIGGER_PLACE_IN_VIEW3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1296, -0x4046, 1232, 0 }, { { 780, 0, -320, 0 }, { -291, 0, 823, 0 }, { 439, 0, -936, 0 }, { -928, 0, 433, 0 } }, { 0, ONE, 0, 0 }, { -3166, 0, -2599, 0 }, 1031, WORLD_COLLISION_TRIGGER_ACTION_ROOM, DRYFIELD_WATER_TANK_TRIGGER_PLACE_IN_VIEW3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1280, -0x2F20, 1560, 0 }, { { -685, 0, 241, 0 }, { 187, 0, -619, 0 }, { -251, 0, 689, 0 }, { 751, 0, -310, 0 } }, { 0, 4097, 0, 0 }, { 2750, 0, 3035, 0 }, 812, WORLD_COLLISION_TRIGGER_ACTION_ROOM, DRYFIELD_WATER_TANK_TRIGGER_PLACE_IN_VIEW4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant 1's Kyle Madigan resource, keyed by placement entry 140, then the end row.
///
/// File 204 in base group 10 loads actor 120400; task index 0 selects its first
/// recipe. The room and actor task descriptors remain borrowed through loading
/// and spawning. The placement table uses the same entry ID.
static AreaResource _gDryfieldWaterTankVariant1KyleResources[2] = {
    { DRYFIELD_WATER_TANK_KYLE_RESOURCE_ENTRY, DRYFIELD_WATER_TANK_KYLE_RESOURCE_FILE, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor120400KyleMadiganTaskDescriptors },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 2's empty actor-resource list, containing only AREA_PLACEMENT_END.
///
/// Its paired placement list is also empty, so this variant requests no actors.
static AreaResource _gDryfieldWaterTankVariant2EmptyResources[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldWaterTankAreaVariants[DRYFIELD_WATER_TANK_VARIANT_SLOT_COUNT] = {
    { NULL, NULL },
    { gMapDryfieldWaterTankVariant1Placements, _gDryfieldWaterTankVariant1KyleResources },
    { gMapDryfieldWaterTankVariant2Placements, _gDryfieldWaterTankVariant2EmptyResources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

/// Header and minimum ambient RGB/luma levels for logical views 1..10.
///
/// Entry zero counts the ten colour entries. Levels use 16 units per 8-bit
/// colour level; views 5/6 use 1500, view 7 uses 2000, and the others use 16.
static WorldCoordRoomAmbientEntry _gDryfieldWaterTankViewAmbientColors[11] = {
    { .viewCount = ARRAY_SIZE(_gDryfieldWaterTankViewAmbientColors) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1500, 1500, 1500, 1500 } },
    { .color = { 1500, 1500, 1500, 1500 } },
    { .color = { 2000, 2000, 2000, 2000 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

// Cue-2 requests into the loaded type-1 bank; cue 1 selects the following entry.
enum {
    DRYFIELD_WATER_TANK_CLASS3_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_WATER_TANK_CLASS3_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_WATER_TANK_CLASS4_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x45,
    DRYFIELD_WATER_TANK_CLASS4_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x47,
    DRYFIELD_WATER_TANK_CLASS5_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    DRYFIELD_WATER_TANK_CLASS5_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
    DRYFIELD_WATER_TANK_CLASS5_JUMP_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x19,
};

/// Class 3's footstep cue bases; walking and scripted jumps share entries 0x35/0x36.
///
/// Running selects entries 0x37/0x38 in the loaded type-1 bank.
static WorldCollisionFootstepSounds _gDryfieldWaterTankClass3FootstepSounds = {
    .walk         = DRYFIELD_WATER_TANK_CLASS3_WALK_CUE_BASE,
    .run          = DRYFIELD_WATER_TANK_CLASS3_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_WATER_TANK_CLASS3_WALK_CUE_BASE,
};

/// Class 4's footstep cue bases; walking and scripted jumps share entries 0x45/0x46.
///
/// Running selects entries 0x47/0x48 in the loaded type-1 bank.
static WorldCollisionFootstepSounds _gDryfieldWaterTankClass4FootstepSounds = {
    .walk         = DRYFIELD_WATER_TANK_CLASS4_WALK_CUE_BASE,
    .run          = DRYFIELD_WATER_TANK_CLASS4_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_WATER_TANK_CLASS4_WALK_CUE_BASE,
};

/// Class 5's separate walking, running and scripted-jump footstep cue bases.
///
/// Requests entries 0x15/0x16, 0x17/0x18 and 0x19/0x1A in the loaded type-1 bank.
static WorldCollisionFootstepSounds _gDryfieldWaterTankClass5FootstepSounds = {
    .walk         = DRYFIELD_WATER_TANK_CLASS5_WALK_CUE_BASE,
    .run          = DRYFIELD_WATER_TANK_CLASS5_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_WATER_TANK_CLASS5_JUMP_CUE_BASE,
};

/// Silent, probe-blocking surface shared by classes 0, 2, 6 and 7.
///
/// Weapon impacts and pushback remain enabled.
static WorldCollisionSurfaceProperties _gDryfieldWaterTankSilentBlockingSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1's silent surface, passing probes and suppressing weapon impacts.
///
/// Pushback remains enabled independently of probe passage.
static WorldCollisionSurfaceProperties _gDryfieldWaterTankProbePassThroughSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 3's blocking surface with footstep cues, weapon impacts and pushback.
static WorldCollisionSurfaceProperties _gDryfieldWaterTankClass3SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldWaterTankClass3FootstepSounds,
};

/// Class 4's blocking surface with footstep cues, weapon impacts and pushback.
static WorldCollisionSurfaceProperties _gDryfieldWaterTankClass4SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldWaterTankClass4FootstepSounds,
};

/// Class 5's blocking surface with footstep cues, weapon impacts and pushback.
static WorldCollisionSurfaceProperties _gDryfieldWaterTankClass5SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldWaterTankClass5FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldWaterTankSurfacePropertiesByClass[8] = {
    &_gDryfieldWaterTankSilentBlockingSurface,
    &_gDryfieldWaterTankProbePassThroughSurface,
    &_gDryfieldWaterTankSilentBlockingSurface,
    &_gDryfieldWaterTankClass3SurfaceProperties,
    &_gDryfieldWaterTankClass4SurfaceProperties,
    &_gDryfieldWaterTankClass5SurfaceProperties,
    &_gDryfieldWaterTankSilentBlockingSurface,
    &_gDryfieldWaterTankSilentBlockingSurface,
};

/// Saved layout keys and the authored map-mark policy of the path-scene update list.
enum {
    DRYFIELD_WATER_TANK_PATH_SCENE_STANDARD_STREET_VARIANT  = 3,
    DRYFIELD_WATER_TANK_PATH_SCENE_BOUNTY_STREET_VARIANT    = 7,
    DRYFIELD_WATER_TANK_PATH_SCENE_WATER_TOWER_VARIANT      = 3,
    DRYFIELD_WATER_TANK_PATH_SCENE_GARAGE_VARIANT           = 2,
    DRYFIELD_WATER_TANK_PATH_SCENE_STANDARD_BALCONY_VARIANT = 3,
    DRYFIELD_WATER_TANK_PATH_SCENE_BOUNTY_BALCONY_VARIANT   = 8,
    DRYFIELD_WATER_TANK_PATH_SCENE_MOTEL_ROOM6_VARIANT      = 2,
    DRYFIELD_WATER_TANK_PATH_SCENE_MAP_MARK                 = 1,
};

/// Saved-area layouts and map marks applied when the player-path scene is latched.
///
/// Nine updates followed by AREA_APPLY_END. Main Street, Parking Lot and Motel
/// Balcony select different variants for normal/replay/Scavenger versus
/// Bounty/Nightmare. Water Tower and Garage update in every mode. All those
/// updates set the map mark; Motel Room 6 selects variant 2 and clears it.
/// Applying the list also removes the affected areas' saved enemy poses.
static AreaApplyRec _gDryfieldWaterTankPlayerPathSceneAreaUpdates[10] = {
    { .stage = GAME_STAGE_DRYFIELD, .area = GAME_AREA_DRYFIELD_MAIN_STREET, .variant = DRYFIELD_WATER_TANK_PATH_SCENE_STANDARD_STREET_VARIANT, .policy = AREA_APPLY_MODE_REPLAY_SCAVENGER | DRYFIELD_WATER_TANK_PATH_SCENE_MAP_MARK },
    { .stage = GAME_STAGE_DRYFIELD, .area = GAME_AREA_DRYFIELD_MAIN_STREET, .variant = DRYFIELD_WATER_TANK_PATH_SCENE_BOUNTY_STREET_VARIANT, .policy = AREA_APPLY_MODE_BOUNTY_NIGHTMARE | DRYFIELD_WATER_TANK_PATH_SCENE_MAP_MARK },
    { .stage = GAME_STAGE_DRYFIELD, .area = GAME_AREA_DRYFIELD_PARKING_LOT, .variant = DRYFIELD_WATER_TANK_PATH_SCENE_STANDARD_STREET_VARIANT, .policy = AREA_APPLY_MODE_REPLAY_SCAVENGER | DRYFIELD_WATER_TANK_PATH_SCENE_MAP_MARK },
    { .stage = GAME_STAGE_DRYFIELD, .area = GAME_AREA_DRYFIELD_PARKING_LOT, .variant = DRYFIELD_WATER_TANK_PATH_SCENE_BOUNTY_STREET_VARIANT, .policy = AREA_APPLY_MODE_BOUNTY_NIGHTMARE | DRYFIELD_WATER_TANK_PATH_SCENE_MAP_MARK },
    { .stage = GAME_STAGE_DRYFIELD, .area = GAME_AREA_DRYFIELD_WATER_TOWER, .variant = DRYFIELD_WATER_TANK_PATH_SCENE_WATER_TOWER_VARIANT, .policy = AREA_APPLY_MODE_ALWAYS | DRYFIELD_WATER_TANK_PATH_SCENE_MAP_MARK },
    { .stage = GAME_STAGE_DRYFIELD, .area = GAME_AREA_DRYFIELD_GARAGE, .variant = DRYFIELD_WATER_TANK_PATH_SCENE_GARAGE_VARIANT, .policy = AREA_APPLY_MODE_ALWAYS | DRYFIELD_WATER_TANK_PATH_SCENE_MAP_MARK },
    { .stage = GAME_STAGE_DRYFIELD, .area = GAME_AREA_DRYFIELD_MOTEL_BALCONY, .variant = DRYFIELD_WATER_TANK_PATH_SCENE_STANDARD_BALCONY_VARIANT, .policy = AREA_APPLY_MODE_REPLAY_SCAVENGER | DRYFIELD_WATER_TANK_PATH_SCENE_MAP_MARK },
    { .stage = GAME_STAGE_DRYFIELD, .area = GAME_AREA_DRYFIELD_MOTEL_BALCONY, .variant = DRYFIELD_WATER_TANK_PATH_SCENE_BOUNTY_BALCONY_VARIANT, .policy = AREA_APPLY_MODE_BOUNTY_NIGHTMARE | DRYFIELD_WATER_TANK_PATH_SCENE_MAP_MARK },
    { .stage = GAME_STAGE_DRYFIELD, .area = GAME_AREA_DRYFIELD_MOTEL_ROOM_6, .variant = DRYFIELD_WATER_TANK_PATH_SCENE_MOTEL_ROOM6_VARIANT, .policy = AREA_APPLY_MODE_ALWAYS },
    { .stage = AREA_APPLY_END },
};

/// Movie-event task borrowed by the singleton waiter until it dispatches teardown.
///
/// Replaced by each successful spawn and polled on later waiter ticks. Polling
/// stops after the exit handler runs; the stale handle is not cleared and must
/// not be dereferenced afterward. The room/event resources must remain loaded.
static Task* _gDryfieldWaterTankPendingMovieEventTask = NULL;

Task* gDryfieldWaterTankMovieEventTask;

/// Returns player control and the saved view after a declined mechanism prompt.
///
/// Requires the live player and the view latched by the singleton prompt.
/// Clears the event/HUD holds, resumes scene actors and selects automatic
/// player drawing; restoring the byte view does not request a resource reload.
static inline void _dryfieldWaterTankRestorePlayerAfterPrompt(void)
{
    gGameSession->eventState                               = 0;
    gGameSession->hideHud                                  = 0;
    gSceneCombatState.actorControl                         = SCENE_COMBAT_ACTORS_RUNNING;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = _gDryfieldWaterTankMechanismPromptSavedView;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
}

void dryfieldWaterTankMechanismPromptTask(Task* task)
{
    enum {
        DRYFIELD_WATER_TANK_PROMPT_START          = 0,
        DRYFIELD_WATER_TANK_PROMPT_WAIT_CAP       = 1,
        DRYFIELD_WATER_TANK_PROMPT_RESOLVE        = 2,
        DRYFIELD_WATER_TANK_PROMPT_VARIANT        = 0,
        DRYFIELD_WATER_TANK_ALREADY_OPERATED_KEY  = 1,
        DRYFIELD_WATER_TANK_OPERATE_CHOICE_KEY    = 10,
        DRYFIELD_WATER_TANK_PROP_SCENE_TASK_INDEX = 0,
        DRYFIELD_WATER_TANK_OPERATION_SOUND       = 4,
    };

    Task* promptTask;

    promptTask = task;
    switch (promptTask->state) {
        case DRYFIELD_WATER_TANK_PROMPT_START:
            if (gameFlagGetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE) == GAME_FLAG_WATER_TOWER_MECHANISM_TANK_OPERATED) {
                capStartSequenceSlot(DRYFIELD_WATER_TANK_MECHANISM_CAP_COMMAND, CAP_PLAYBACK_DISPLAY_TRANSITION, DRYFIELD_WATER_TANK_ALREADY_OPERATED_KEY);
                break;
            }
            gGameSession->eventState                    = 1;
            _gDryfieldWaterTankMechanismPromptSavedView = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view;
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capStartSequenceSlot(DRYFIELD_WATER_TANK_MECHANISM_CAP_COMMAND, CAP_PLAYBACK_IN_PLACE, DRYFIELD_WATER_TANK_PROMPT_VARIANT);
            task->state = promptTask->state + 1;
            return;
        case DRYFIELD_WATER_TANK_PROMPT_WAIT_CAP:
            if (capIsBusy() == 0) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
                promptTask->state              = promptTask->state + 1;
            }
            return;
        case DRYFIELD_WATER_TANK_PROMPT_RESOLVE:
            // A retained CAP choice hands presentation to the prop scene.
            if (capGetVariantKey() == DRYFIELD_WATER_TANK_OPERATE_CHOICE_KEY) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                gameFlagSetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE, GAME_FLAG_WATER_TOWER_MECHANISM_TANK_OPERATED);
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, DRYFIELD_WATER_TANK_OPERATION_SOUND), 0, 0);
                taskSpawnFromTable(gDryfieldWaterTankPropSceneTaskDescs, DRYFIELD_WATER_TANK_PROP_SCENE_TASK_INDEX, 0, 0);
                _dryfieldWaterTankSyncMechanismSprites();
            } else {
                _dryfieldWaterTankRestorePlayerAfterPrompt();
            }
            break;
        default:
            return;
    }
    taskKill(promptTask);
}

s32 dryfieldWaterTankRefuseKeyItem(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 dryfieldWaterTankResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    return 1;
}

s32 dryfieldWaterTankHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg)
{
    enum {
        DRYFIELD_WATER_TANK_ACTION_MOVIE_EVENT             = 1,
        DRYFIELD_WATER_TANK_ACTION_PLAYER_PATH_SCENE       = 2,
        DRYFIELD_WATER_TANK_ACTION_FIRST_PLACEMENT_SCRIPT  = 3,
        DRYFIELD_WATER_TANK_ACTION_SECOND_PLACEMENT_SCRIPT = 4,
        DRYFIELD_WATER_TANK_MOVIE_WAITER_TASK_INDEX        = 0,
        DRYFIELD_WATER_TANK_PLAYER_PATH_SCENE_OBJECTIVE    = 14,
        DRYFIELD_WATER_TANK_PLAYER_PATH_SCENE_DIALOGUE     = 3,
    };

    // Latch each one-shot scene before it can be requested again.
    if (request->actionId == DRYFIELD_WATER_TANK_ACTION_MOVIE_EVENT) {
        if (gameFlagGetNibble(GAME_FLAG_DRYFIELD_WATER_TANK_036) == 0) {
            gameFlagSetNibble(GAME_FLAG_DRYFIELD_WATER_TANK_036, 1);
            taskSpawnFromTable(gDryfieldWaterTankRoomActionTaskDescs, DRYFIELD_WATER_TANK_MOVIE_WAITER_TASK_INDEX, 0, 0);
            gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, 1);
        }
    }
    if ((request->actionId == DRYFIELD_WATER_TANK_ACTION_PLAYER_PATH_SCENE) && (gameFlagGetNibble(GAME_FLAG_WATER_TANK_SCENE_SEEN) == 0)) {
        gameFlagSetNibble(GAME_FLAG_WATER_TANK_SCENE_SEEN, 1);
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, DRYFIELD_WATER_TANK_PLAYER_PATH_SCENE_OBJECTIVE);
        gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
        gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, DRYFIELD_WATER_TANK_PLAYER_PATH_SCENE_DIALOGUE);
        areaApplySavedUpdates(_gDryfieldWaterTankPlayerPathSceneAreaUpdates);
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        evsStartScriptWithSkip(_gDryfieldWaterTankPlayerPathSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldWaterTankPlayerPathSceneSkipScript);
    }
    if (request->actionId == DRYFIELD_WATER_TANK_ACTION_FIRST_PLACEMENT_SCRIPT) {
        evsStartScript(gDryfieldWaterTankPlacePlayerInView4Script, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    if (request->actionId == DRYFIELD_WATER_TANK_ACTION_SECOND_PLACEMENT_SCRIPT) {
        evsStartScript(gDryfieldWaterTankPlacePlayerInView3Script, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 1;
}

s32 dryfieldWaterTankHandleRoomCommand(Task* task, s32 messageId, s32 commandIndex, s32 secondArg)
{
    enum { DRYFIELD_WATER_TANK_MECHANISM_PROMPT_TASK_INDEX = 1 };

    if (commandIndex == DRYFIELD_WATER_TANK_MECHANISM_CAP_COMMAND) {
        taskSpawnFromTable(gDryfieldWaterTankRoomActionTaskDescs, DRYFIELD_WATER_TANK_MECHANISM_PROMPT_TASK_INDEX, 0, 0);
    }
    return 0;
}

void dryfieldWaterTankWaitMovieEventTask(Task* task)
{
    enum {
        DRYFIELD_WATER_TANK_MOVIE_WAITER_SPAWN = 0,
        DRYFIELD_WATER_TANK_MOVIE_WAITER_POLL  = 1,
    };
    s32 childResult;

    switch (task->state) {
        case DRYFIELD_WATER_TANK_MOVIE_WAITER_SPAWN:
            _gDryfieldWaterTankPendingMovieEventTask = taskSpawnFromTable(&gDryfieldWaterTankMovieEventTaskDesc, 0, 0, 0);
            task->state++;
            return;
        case DRYFIELD_WATER_TANK_MOVIE_WAITER_POLL:
            if (taskPollKill(_gDryfieldWaterTankPendingMovieEventTask, &childResult) != 0) {
                taskKill(task);
            }
            return;
    }
}

/// Registers room messages and starts the swaying tank model and ambience.
///
/// Runs as room-task state 0 with room resources loaded, publishes
/// `GAME_TASK_SLOT_ROOM`, initializes mechanism sprite visibility and advances
/// to the per-view ambience state. The model task borrows room-owned geometry.
static void _dryfieldWaterTankInitRoomTask(Task* task)
{
    task->msgTable = gDryfieldWaterTankRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(_gDryfieldWaterTankSwayTaskDescs, 0, 0, 0);
    sndEvtRequestScriptStart(SOUND_WATER_TANK_AMBIENCE, 0, 0);
    _dryfieldWaterTankSyncMechanismSprites();
    task->state = task->state + 1;
}

/// Maintains the room's two view-specific ambience scripts while the view is ready.
///
/// Logical views 4 and 10 start their respective scripts; other views request
/// fades of 45 and 60 audio updates. View indices here precede sprite mapping.
/// The room and sound resources must remain loaded. The task argument is unused.
static void _dryfieldWaterTankUpdateViewAmbience(Task* task)
{
    enum {
        DRYFIELD_WATER_TANK_VIEW4_AMBIENCE_FADE_TICKS  = 45,
        DRYFIELD_WATER_TANK_VIEW10_AMBIENCE_FADE_TICKS = 60,
    };

    if (gGameSession->viewReady != 0) {
        if (gGameSession->location.view == 4) {
            sndEvtRequestStageScriptStart(SOUND_WATER_TANK_VIEW4_AMBIENCE, 0, 0);
        } else {
            sndEvtRequestStageScriptStop(SOUND_WATER_TANK_VIEW4_AMBIENCE, DRYFIELD_WATER_TANK_VIEW4_AMBIENCE_FADE_TICKS);
        }
        if (gGameSession->location.view == 10) {
            sndEvtRequestStageScriptStart(SOUND_WATER_TANK_VIEW10_AMBIENCE, 0, 0);
            return;
        }
        sndEvtRequestStageScriptStop(SOUND_WATER_TANK_VIEW10_AMBIENCE, DRYFIELD_WATER_TANK_VIEW10_AMBIENCE_FADE_TICKS);
    }
}

/// Room-task handlers indexed by Task::state: initialize, ambience, then teardown.
///
/// States 0..2 are dispatched from a complete stack copy. The table and its
/// room callbacks must stay loaded for the task's lifetime; indexing is unchecked.
static const TaskFuncTable3 _gDryfieldWaterTankRoomTaskStates = {
    { _dryfieldWaterTankInitRoomTask, _dryfieldWaterTankUpdateViewAmbience, taskKill },
};

void dryfieldWaterTankRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldWaterTankRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

/// Selects pre-operation or post-operation sprites from the shared mechanism state.
///
/// States 0..2 show the pre-operation sprites; state 3 shows the post-operation
/// sprites. Other nibble values leave visibility intact. Requires the water
/// tank sprite directory when in Dryfield; the sprite setter ignores other stages.
static void _dryfieldWaterTankSyncMechanismSprites(void)
{
    switch (gameFlagGetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE)) {
        case GAME_FLAG_WATER_TOWER_MECHANISM_INITIAL:
        case GAME_FLAG_WATER_TOWER_MECHANISM_TOWER_RESTORED:
        case GAME_FLAG_WATER_TOWER_MECHANISM_TOWER_OPERATED:
            dryfieldWaterTankSetPreOperationSprites(true);
            break;
        case GAME_FLAG_WATER_TOWER_MECHANISM_TANK_OPERATED:
            dryfieldWaterTankSetPreOperationSprites(false);
            break;
    }
}

/// Advances the prop's dust-offset cycle and spawns a puff at its current position.
///
/// This task uses `killCountdown` as an index in 0..10, initialized to zero.
/// Only those eleven entries of the twelve-word offset table participate.
/// Each offset narrows to a signed halfword in the prop's local X axis.
/// Requires a live model root during the call. The puff uses copied XYZ after
/// spawning and does not read the temporary offset again.
static inline void _dryfieldWaterTankSpawnPropSlideDust(Task* task)
{
    enum {
        DRYFIELD_WATER_TANK_PROP_DUST_LAST_OFFSET = 10,
        // Optional child puffs, two updates per texture frame, initial scale 0x300.
        DRYFIELD_WATER_TANK_PROP_DUST_ARGUMENT = (s32)0x80000000 | (2 << 12) | 0x300,
    };
    GfxCoord* dustCoord;
    SVECTOR   dustOffset;

    dustCoord = task->extra.tmd->coords;
    if (task->killCountdown >= DRYFIELD_WATER_TANK_PROP_DUST_LAST_OFFSET) {
        task->killCountdown = 0;
    } else {
        task->killCountdown = (u16)task->killCountdown + 1;
    }
    dustOffset.vy = 0;
    dustOffset.vz = 0;
    dustOffset.vx = gDryfieldWaterTankPropDustLocalXOffsets[task->killCountdown];
    effectSpawn(EFFECT_DUST_PUFF, dustCoord, DRYFIELD_WATER_TANK_PROP_DUST_ARGUMENT, &dustOffset);
}

/// Advances the prop's slide and settling, returning 1 when final placement is applied.
///
/// Called once per prop-task frame with initialized work and a live model root.
/// Motion advances 20 parent-coordinate units in Z per call and starts settling
/// only after passing the endpoint. Settling lasts 61 calls, then sends
/// `ACTOR_MESSAGE_PLACE` with the endpoint. Returns 0 while unfinished and marks
/// the root dirty. Tick bit 2 adds a five-unit nudge in Y while moving and X
/// while settling. The settle counter is not reset when the slide is restarted.
static s32 _dryfieldWaterTankStepPropSlide(Task* task)
{
    enum {
        DRYFIELD_WATER_TANK_PROP_SLIDE_STEP     = 20,
        DRYFIELD_WATER_TANK_PROP_NUDGE_TICK_BIT = 1 << 2,
    };
    _DryfieldWaterTankPropSceneWork* work  = task->work;
    GfxCoord*                        coord = task->extra.tmd->coords;

    switch (work->slidePhase) {
        case DRYFIELD_WATER_TANK_PROP_SLIDE_MOVING:
            // Advance past the endpoint before entering the settling phase.
            coord->coord.t[2] += DRYFIELD_WATER_TANK_PROP_SLIDE_STEP;
            coord->coord.t[1]  = gDryfieldWaterTankPropSlidePlacements[DRYFIELD_WATER_TANK_PROP_PLACEMENT_END].pos.vy;
            if (gDisplayState.gameTick & DRYFIELD_WATER_TANK_PROP_NUDGE_TICK_BIT) {
                coord->coord.t[1] += 5;
            }
            if (coord->coord.t[2] > gDryfieldWaterTankPropSlidePlacements[DRYFIELD_WATER_TANK_PROP_PLACEMENT_END].pos.vz) {
                work->slidePhase++;
            }
            _dryfieldWaterTankSpawnPropSlideDust(task);
            break;

        case DRYFIELD_WATER_TANK_PROP_SLIDE_SETTLING:
            // Let the prop settle before applying the endpoint's complete placement.
            work->settleFrames++;
            if (work->settleFrames >= DRYFIELD_WATER_TANK_PROP_SETTLE_FRAMES) {
                TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &gDryfieldWaterTankPropSlidePlacements[DRYFIELD_WATER_TANK_PROP_PLACEMENT_END], 0);
                return 1;
            }
            coord->coord.t[0] = gDryfieldWaterTankPropSlidePlacements[DRYFIELD_WATER_TANK_PROP_PLACEMENT_END].pos.vx;
            if (gDisplayState.gameTick & DRYFIELD_WATER_TANK_PROP_NUDGE_TICK_BIT) {
                coord->coord.t[0] += 5;
            }
            break;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Allocates the scene prop's work and binds its model to the driver's lifetime.
///
/// The driver's published task and the prop's TMD body must be live. Owns a
/// zeroed primary-heap work block and primitive buffers; the model borrows the
/// work's lighting matrices. Parents the prop beneath the driver for teardown
/// and the model root beneath the view for drawing. Work-allocation failure
/// kills the prop and returns; the caller retains its subsequent state update.
static inline void _dryfieldWaterTankInitPropModel(Task* task)
{
    TmdObject*                       initialModel;
    GfxCoord*                        rootCoord;
    _DryfieldWaterTankPropSceneWork* work;

    initialModel = task->extra.tmd;
    rootCoord    = initialModel->coords;
    work         = memMalloc(sizeof(*work), false);
    task->work   = work;
    if (work == NULL) {
        taskKill(task);
    } else {
        memFillBytes(work, 0, sizeof(*work));
        work->playerTask    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        rootCoord->parent   = &gGfxViewCoord;
        initialModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        tmdAllocPrimitiveBuffer(initialModel);
        initialModel->lightMtx = &work->lightMtx;
        initialModel->colorMtx = &work->colorMtx;
        task->msgTable         = gDryfieldWaterTankPropMessages;
        taskReparent(_gDryfieldWaterTankPropSceneTask, task);
    }
}

void dryfieldWaterTankPropTask(Task* task)
{
    TmdObject* model;
    VECTOR     worldPosition;

    switch (task->state) {
        case DRYFIELD_WATER_TANK_PROP_STATE_INIT:
            _dryfieldWaterTankInitPropModel(task);
            task->state += 1;
            break;
        case DRYFIELD_WATER_TANK_PROP_STATE_IDLE:
            break;
        case DRYFIELD_WATER_TANK_PROP_STATE_SLIDING:
            if (_dryfieldWaterTankStepPropSlide(task) & 0xFFFF) {
                task->state = DRYFIELD_WATER_TANK_PROP_STATE_IDLE;
            }
            break;
    }

    // Lighting follows the composed root, even while the prop is idle.
    model            = task->extra.tmd;
    worldPosition.vx = task->extra.tmd->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(model, &worldPosition, 0, 3);
}

/// Hides the player, reveals the prop and sends its restart-and-slide command.
///
/// Borrows the driver's work and both live tasks. Draw mode 0 hides the player
/// and allocates its buffers; mode 1 shows the prop. The synchronous restart handler
/// reads only `ActorCommand.command`, so the remaining stack bytes are unused.
static inline void _dryfieldWaterTankStartPropSlide(const _DryfieldWaterTankPropSceneWork* work)
{
    ActorCommand slideCommand;

    taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE, 0);
    taskMessageDispatch(work->propTask, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
    slideCommand.command = DRYFIELD_WATER_TANK_PROP_STATE_SLIDING;
    TASK_MESSAGE_DISPATCH_POINTER(work->propTask, ACTOR_COMMAND_MESSAGE_APPLY, &slideCommand, 0);
}

void dryfieldWaterTankPropSceneTask(Task* task)
{
    enum {
        DRYFIELD_WATER_TANK_PROP_SCENE_INIT         = 0,
        DRYFIELD_WATER_TANK_PROP_SCENE_START_SCRIPT = 1,
        DRYFIELD_WATER_TANK_PROP_SCENE_RUN          = 2,
        DRYFIELD_WATER_TANK_PROP_TASK_INDEX         = 1,
        DRYFIELD_WATER_TANK_PROP_SCENE_RESTORE_VIEW = 3,
        DRYFIELD_WATER_TANK_PROP_SCENE_SLIDE_SOUND  = 2,
        DRYFIELD_WATER_TANK_PROP_SCENE_EXTRA_SOUND  = 8,
    };
    _DryfieldWaterTankPropSceneWork* work;

    work = task->work;
    switch (task->state) {
        case DRYFIELD_WATER_TANK_PROP_SCENE_INIT:
            work       = memMalloc(sizeof(*work), false);
            task->work = work;
            if (work == NULL) {
                taskKill(task);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->playerTask                 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                _gDryfieldWaterTankPropSceneTask = task;
            }
            work           = task->work;
            work->propTask = taskSpawnFromTable(gDryfieldWaterTankPropSceneTaskDescs, DRYFIELD_WATER_TANK_PROP_TASK_INDEX, 0, 0);
            task->state    = task->state + 1;
            break;
        case DRYFIELD_WATER_TANK_PROP_SCENE_START_SCRIPT:
            TASK_MESSAGE_DISPATCH_POINTER(work->propTask, ACTOR_MESSAGE_PLACE, &gDryfieldWaterTankPropSlidePlacements[DRYFIELD_WATER_TANK_PROP_PLACEMENT_START], 0);
            evsStartScriptWithSkip(gDryfieldWaterTankPropSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gDryfieldWaterTankPropSceneSkipScript);
            task->state = task->state + 1;
            break;
        case DRYFIELD_WATER_TANK_PROP_SCENE_RUN:
            if (gGameSession->eventState == 0) {
                taskRequestKill(task, 0);
            }
            break;
    }

    // Carry out the request the scene script posted, for this one frame.
    work = task->work;
    switch (work->request) {
        case DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_NONE:
            break;
        case DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_START_SLIDE:
            _dryfieldWaterTankStartPropSlide(work);
            break;
        case DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_SHOW_PLAYER:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = viewFindLogicalIndex(DRYFIELD_WATER_TANK_PROP_SCENE_RESTORE_VIEW);
            gGameSession->viewDirty                                = 1;
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);
            break;
        case DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_PLAY_SOUNDS:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, DRYFIELD_WATER_TANK_PROP_SCENE_SLIDE_SOUND), 0, 0);
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, DRYFIELD_WATER_TANK_PROP_SCENE_EXTRA_SOUND), 0, 0);
            break;
    }
    work->request = DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_NONE;
}

void dryfieldWaterTankSetPropModelDraw(Task* task, s32 messageId, s32 visible, s32 secondArg)
{
    TmdObject* model;

    model = task->extra.tmd;
    if (visible != 0) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

#include "../../shared/actor/actor_messages_place_ypr.inc.c"

void dryfieldWaterTankRestartPropSlide(Task* task, s32 messageId, const ActorCommand* command, s32 secondArg)
{
    _DryfieldWaterTankPropSceneWork* work;
    s32                              nextState;

    work                = task->work;
    work->slidePhase    = DRYFIELD_WATER_TANK_PROP_SLIDE_MOVING;
    work->field_54      = 0;
    nextState           = command->command;
    task->killCountdown = 0;
    task->state         = nextState;
}

void dryfieldWaterTankPostPropSceneRequest(s16 request)
{
    _DryfieldWaterTankPropSceneWork* work;

    work           = _gDryfieldWaterTankPropSceneTask->work;
    work->request  = request;
    work->field_52 = 0;
}

void dryfieldWaterTankSkipPropScene(void)
{
    enum {
        DRYFIELD_WATER_TANK_PROP_SCENE_RESTORE_VIEW    = 3,
        DRYFIELD_WATER_TANK_PROP_SCENE_SOUND_ENTRY     = 2,
        DRYFIELD_WATER_TANK_PROP_SCENE_SKIP_FADE_TICKS = 10,
    };
    _DryfieldWaterTankPropSceneWork* work;

    work                                                   = _gDryfieldWaterTankPropSceneTask->work;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = viewFindLogicalIndex(DRYFIELD_WATER_TANK_PROP_SCENE_RESTORE_VIEW);
    taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);
    gGameSession->viewDirty = 1;
    sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, DRYFIELD_WATER_TANK_PROP_SCENE_SOUND_ENTRY), DRYFIELD_WATER_TANK_PROP_SCENE_SKIP_FADE_TICKS);
}

#include "../../shared/screen/screen_fade_in_tile.inc.c"
