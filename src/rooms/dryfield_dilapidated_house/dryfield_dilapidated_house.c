#include "rooms/dryfield_dilapidated_house.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_521100.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/ending.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/screen_wave.h"
#include "../../shared/bezier_curve.h"
#include "../../shared/screen_negative.h"
#include "../../shared/glow_draw.h"
#include "../../shared/sprite_quad.h"
#include "../../shared/model_morph.h"

extern SVECTOR D_dryfield_dilapidated_house_80186944[2];

extern AnimationSet* D_dryfield_dilapidated_house_80183F00[16];

/// Work block of the morphing model a script command attaches to one of the
/// player's coordinates, which its child beam and cone effects read through
/// their parent task.
///
/// The three levels all hold the task's morph progress, 0..`ONE` in 4.12 fixed
/// point, rewritten every frame. `attachMtx` is the composed matrix of the
/// attachment point. The allocation is 0x6C bytes; nothing accesses the bytes
/// after the matrix.
typedef struct {
    s32    morphLevel;     // Morph progress; written every frame but never read
    s32    beamLevel;      // Brightness and length of the two ring beams
    s32    coneLevel;      // Inner-edge grey of the two cones, saturating at 0x400
    MATRIX attachMtx;      // Parent model's coordinate chain composed up to the attachment coordinate
    byte   field_2C[0x40]; // Allocated but never accessed; role unproven
} _DryfieldDilapidatedHouseMorphWork;
STATIC_ASSERT_SIZEOF(_DryfieldDilapidatedHouseMorphWork, 0x6C);

/// Work block of the curve debug task: a model task whose model stays hidden
/// while it marks a fixed Bezier path's control points and samples on screen
/// with small coloured crosses.
///
/// Nothing in the room spawns that task. The block is filled in once as the
/// task starts and never read back: the markers are placed with the parent
/// task's `_DryfieldDilapidatedHouseMorphWork::attachMtx` instead.
typedef struct {
    MATRIX initialMtx; // The model's local coordinate matrix as the task started
    s32    field_20;   // Set to 0x1000 at start and never read; role unproven
} _DryfieldDilapidatedHouseCurveDebugWork;
STATIC_ASSERT_SIZEOF(_DryfieldDilapidatedHouseCurveDebugWork, 0x24);

/// One turn of a `_DryfieldDilapidatedHouseConeWork::rimPhase` entry, which
/// counts in quarters of the 4096-per-turn angle unit `rsin` takes.
#define DRYFIELD_DILAPIDATED_HOUSE_CONE_RIM_PHASE_PERIOD 0x4000

/// Work block of one of the two cones the morphing model spawns as children:
/// the ripple that waves the cone's outer rim back and forth along its axis.
///
/// A cone is two 16-vertex rings joined by quads, rebuilt every frame. The
/// inner ring is rigid; each outer vertex is displaced along the axis by the
/// sine of its own phase, and every phase advances at its own fixed rate, so
/// the rim never settles into a repeating outline. A cone starts with each
/// phase already advanced by the frame count in its spawn argument, which is
/// what keeps the two cones out of step with each other.
typedef struct {
    s32 rimPhase[16]; // Ripple phase of each outer-ring vertex, wrapped to one period
} _DryfieldDilapidatedHouseConeWork;
STATIC_ASSERT_SIZEOF(_DryfieldDilapidatedHouseConeWork, 0x40);

extern void func_80724608(void* owner, s32 arg1, s32 arg2, void* name);

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 gScreenWaveRamp;

/// The ramp and tint the wave task was spawned with.
extern ScreenWaveCtx* gScreenWaveCtx;

/// Phase records of the wave's 11 column edges and 30 row edges.
extern ScreenWaveOscillator gScreenWaveColumns[13];
extern ScreenWaveOscillator gScreenWaveRows[32];

extern s32          D_dryfield_dilapidated_house_80189B70;
extern s32          D_dryfield_dilapidated_house_80189B6C;
extern s32          D_dryfield_dilapidated_house_80183EFC;
extern EvsCommand   D_dryfield_dilapidated_house_80184408[];
extern EvsCommand   D_dryfield_dilapidated_house_80184C60[];
extern TaskDesc     D_dryfield_dilapidated_house_80183EB4[];
extern EvsCommand   D_dryfield_dilapidated_house_80184EA0[];
extern EvsCommand   D_dryfield_dilapidated_house_801855F0[];
extern AreaApplyRec D_dryfield_dilapidated_house_80189AA0[];
extern AreaApplyRec D_dryfield_dilapidated_house_80189B24[];

/// The room's cutscene task, spawned from entry 0 of
/// `D_dryfield_dilapidated_house_80183EB4` when `Gp_LookupSlot4(1)` is non-zero
/// as the room starts, and NULL otherwise.
extern Task* D_dryfield_dilapidated_house_80189B78;

extern TaskDesc D_dryfield_dilapidated_house_80183E64[];
extern Task*    D_dryfield_dilapidated_house_801857E8;
extern TaskDesc D_dryfield_dilapidated_house_80186854[];
extern Task*    D_dryfield_dilapidated_house_80189B7C;
/// Argument block of the room's negative freeze-frame: the capture task is
/// spawned with its address and keeps reading it, so `duration` has to be
/// stored before the spawn.
extern ScreenNegativeCaptureArgs D_dryfield_dilapidated_house_80189B80;

/// Shared in source with actor 136300: the ramp context the message handler
/// seeds and hands to the screen-wave task it starts, and that task's entry.
extern ScreenWaveCtx D_dryfield_dilapidated_house_80189C94;
extern TaskDesc      D_dryfield_dilapidated_house_80183E48[];

extern TaskMessageEntry D_dryfield_dilapidated_house_80183E8C[];
extern s32              D_dryfield_dilapidated_house_80186804[16];
extern SVECTOR          D_dryfield_dilapidated_house_80186844[2];
extern ModelMorph       D_dryfield_dilapidated_house_8018669C;
extern SVECTOR          D_dryfield_dilapidated_house_801866B4[];
extern SVECTOR          D_dryfield_dilapidated_house_80186794[2];
extern SVECTOR          D_dryfield_dilapidated_house_801867A4[6];
extern SVECTOR          D_dryfield_dilapidated_house_801867D4[6];

static void func_dryfield_dilapidated_house_8017E9A4(s32 arg0);
static void func_dryfield_dilapidated_house_8017EBB8(Task* task);
static void func_dryfield_dilapidated_house_8017EE58(Task* task);
static void func_dryfield_dilapidated_house_8017FAD4(Task* task, SVECTOR* verts, s32* arg2, s32* arg3);
static void _dryfieldDilapidatedHouseMorphExit(Task* task);
static s32  func_dryfield_dilapidated_house_80180FD8(Task* task);
static void _dryfieldDilapidatedHouseUpdateAttachmentTransform(Task* task);
static void _dryfieldDilapidatedHouseCopyModelVisibility(TmdObject* model, const TmdObject* parentModel);

static void func_dryfield_dilapidated_house_8017EAB4(Task* arg0);
static void func_dryfield_dilapidated_house_8017E014(Task* task);
static void func_dryfield_dilapidated_house_80180B84(Task* task);
static void func_dryfield_dilapidated_house_80180F5C(Task* arg0);
static void _dryfieldDilapidatedHouseCurveDebugInit(Task* task);
static void func_dryfield_dilapidated_house_80181264(Task* arg0);
static void _dryfieldDilapidatedHouseRingBeamInit(Task* task);
static void func_dryfield_dilapidated_house_801813DC(Task* task);
static void _dryfieldDilapidatedHouseRingBeamExit(Task* task);
static void func_dryfield_dilapidated_house_801814B4(Task* arg0);
static void func_dryfield_dilapidated_house_80181584(Task* task);
static void _dryfieldDilapidatedHouseMorphConeExit(Task* task);
static void _dryfieldDilapidatedHouseDrawLightPrism(GfxCoord* coord, s16 firstVertex);
static void func_dryfield_dilapidated_house_80180738(Task* task, SVECTOR* verts);
static void _dryfieldDilapidatedHouseDrawMorphCone(Task* task, const SVECTOR* ringVertices);
static void _dryfieldDilapidatedHouseDrawTwinTrail(s16 newestSlot, s16 colorMultipliers);

enum {
    DRYFIELD_DILAPIDATED_HOUSE_CONE_RING_VERTEX_COUNT = 16,
    DRYFIELD_DILAPIDATED_HOUSE_PRISM_RING_CORNERS     = 4,
    DRYFIELD_DILAPIDATED_HOUSE_PRISM_VERTEX_COUNT     = 8,
};

extern WorldCollisionGrid         D_dryfield_dilapidated_house_801872E4[1];
extern WorldCollisionOccluder     D_dryfield_dilapidated_house_80189260[1];
extern WorldCollisionTrigger      D_dryfield_dilapidated_house_80188D08[9];
extern WorldCollisionTrigger      D_dryfield_dilapidated_house_80188FB4[9];
extern WorldCoordRoomAmbientEntry D_dryfield_dilapidated_house_801899A0[22];
extern WorldCoordRoomLights       D_dryfield_dilapidated_house_801898FC[1];
extern PadScriptCmd               D_dryfield_dilapidated_house_80189B30[2];
extern PadScriptCmd               D_dryfield_dilapidated_house_80189B40[4];
extern PadScriptCmd               D_dryfield_dilapidated_house_80189B5C[2];
extern PadScriptVibrationSegment  D_dryfield_dilapidated_house_80189B38[2];
extern PadScriptVibrationSegment  D_dryfield_dilapidated_house_80189B50[3];
extern PadScriptVibrationSegment  D_dryfield_dilapidated_house_80189B64[2];
extern SVECTOR                    D_dryfield_dilapidated_house_80189CA0[40];
static s32                        _dryfieldDilapidatedHouseRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 secondArg);
s32                               func_dryfield_dilapidated_house_8017E574(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32                        _dryfieldDilapidatedHouseIgnoreRoomCommand(Task* task, s32 messageId, s32 firstArg, s32 secondArg);
s32                               func_dryfield_dilapidated_house_8017E68C(Task* task, s32 msgId, const void* firstArg, s32 arg3);
void                              func_dryfield_dilapidated_house_8017DE88(Task*);
void                              func_dryfield_dilapidated_house_8017E144(Task*);
void                              func_dryfield_dilapidated_house_8017E2B0(Task*);
void                              func_dryfield_dilapidated_house_8017E6DC(Task*);
void                              func_dryfield_dilapidated_house_8017E780(Task*);
void                              func_dryfield_dilapidated_house_8017E858(Task*);
void                              func_dryfield_dilapidated_house_8017E8A8(s32);
void                              func_dryfield_dilapidated_house_8017E8C8(void);
void                              func_dryfield_dilapidated_house_8017E8E8(s32);
void                              func_dryfield_dilapidated_house_8017E970(s32);
void                              func_dryfield_dilapidated_house_8017EA10(s32);
void                              func_dryfield_dilapidated_house_8017EA7C(void);
void                              func_dryfield_dilapidated_house_80180F04(Task*);
void                              func_dryfield_dilapidated_house_80181134(Task*);
void                              func_dryfield_dilapidated_house_801812E8(Task*);
void                              func_dryfield_dilapidated_house_8018145C(Task*);

TaskDesc D_dryfield_dilapidated_house_80183E48[2] = {
    { { { TASK_BODY_NONE, 192 } }, screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

TaskDesc D_dryfield_dilapidated_house_80183E64[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_dilapidated_house_8017DE88, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

RECT gScreenNegativeFrameRect = { 0, 0, 320, 240 };

RECT gScreenNegativeStripRect = { 0, 0, 16, 240 };

enum { DRYFIELD_DILAPIDATED_HOUSE_MESSAGE_USE_KEY_ITEM = 5105 };

TaskMessageEntry D_dryfield_dilapidated_house_80183E8C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_dilapidated_house_8017E574 },
    { DRYFIELD_DILAPIDATED_HOUSE_MESSAGE_USE_KEY_ITEM, _dryfieldDilapidatedHouseRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_dilapidated_house_8017E68C },
    { ROOM_MESSAGE_COMMAND, _dryfieldDilapidatedHouseIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_dilapidated_house_80183EB4[4] = {
    { { { TASK_BODY_NONE, 97 } }, func_dryfield_dilapidated_house_8017E6DC, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_dilapidated_house_8017E780, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_dilapidated_house_8017E144, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_dilapidated_house_8017E2B0, { .value = 0 } },
};

TaskDesc D_dryfield_dilapidated_house_80183EE4[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_dilapidated_house_8017E858, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 D_dryfield_dilapidated_house_80183EFC = 0;

AnimationSet* D_dryfield_dilapidated_house_80183F00[16] = {
    NULL,
    &gActor521100Animation07168,
    NULL,
    NULL,
    &gActor521100Animation05474,
    NULL,
    &gActor521100Animation059C4,
    NULL,
    &gActor521100Animation05F98,
    NULL,
    &gActor521100Animation0623C,
    &gActor521100Animation07BA4,
    &gActor521100Animation0820C,
    &gActor521100Animation09030,
    &gActor521100Animation091C0,
    NULL,
};

AnimationBankCopyRequest D_dryfield_dilapidated_house_80183F40 = { { .sets = D_dryfield_dilapidated_house_80183F00 }, ARRAY_SIZE(D_dryfield_dilapidated_house_80183F00) };

AnimationPlayRequest D_dryfield_dilapidated_house_80183F48[4] = {
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_dilapidated_house_80183F98 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80183FAC = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80183FC0 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80183FD4 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80183FE8 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80183FFC[2] = {
    { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 57, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_dilapidated_house_80184024 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184038 = { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_8018404C = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184060 = { { .index = 1 }, 60, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184074 = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184088 = { { .index = 0 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_8018409C = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801840B0 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801840C4 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801840D8[5] = {
    { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_dilapidated_house_8018413C = { { .index = 0 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184150 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184164 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184178 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_8018418C = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801841A0 = { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801841B4 = { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801841C8 = { { .index = 0 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801841DC = { { .index = 0 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801841F0 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184204 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184218[2] = {
    { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_dilapidated_house_80184240 = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184254 = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184268 = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_8018427C = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184290 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

ActorCommand D_dryfield_dilapidated_house_801842A4 = { { .loc = { 2, 9 } }, 0 };

ActorCommand D_dryfield_dilapidated_house_801842A8 = { { .loc = { 2, 9 } }, 1 };

ActorCommand D_dryfield_dilapidated_house_801842AC = { { .loc = { 2, 9 } }, 2 };

ActorCommand D_dryfield_dilapidated_house_801842B0 = { { .loc = { 2, 9 } }, 3 };

ActorCommand D_dryfield_dilapidated_house_801842B4 = { { .loc = { 2, 9 } }, 4 };

ActorTransform D_dryfield_dilapidated_house_801842B8 = { { -5304, 0, -1940, 0 }, { 0, 796, 0, 0 } };

ActorTransform D_dryfield_dilapidated_house_801842D0 = { { -2630, 0, -1740, 0 }, { 0, 910, 0, 0 } };

ActorTransform D_dryfield_dilapidated_house_801842E8 = { { -2630, 0, -1900, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_dryfield_dilapidated_house_80184300 = { { 2900, 0, -120, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_dryfield_dilapidated_house_80184318 = { { 2900, 0, -408, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_dryfield_dilapidated_house_80184330 = { { 0, 0, -860, 0 }, { 0, -1137, 0, 0 } };

ActorTransform D_dryfield_dilapidated_house_80184348 = { { -1300, 0, -1300, 0 }, { 0, -1137, 0, 0 } };

ActorTransform D_dryfield_dilapidated_house_80184360 = { { -1300, 0, -1800, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_dryfield_dilapidated_house_80184378 = { { -1880, 0, -1460, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_dryfield_dilapidated_house_80184390 = { { 130, 0, -930, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_dryfield_dilapidated_house_801843A8 = { { -370, 0, -1100, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_dryfield_dilapidated_house_801843C0 = { { 500, 0, 300, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_dryfield_dilapidated_house_801843D8 = { { 1000, 0, 0, 0 }, { 0, 1024, 0, 0 } };

GameActorMoveAnim D_dryfield_dilapidated_house_801843F0 = { 19, 1 };

EvsSceneKey D_dryfield_dilapidated_house_801843F8 = { 2, 11, 11 };

EvsSceneKey D_dryfield_dilapidated_house_80184400 = { 2, 12, 11 };

EvsCommand D_dryfield_dilapidated_house_80184408[89] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2007 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_dilapidated_house_80183F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80184290 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_dryfield_dilapidated_house_801843F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801842B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184330 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184390 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801842D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80184024 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184088 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184348 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80183FC0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801842E8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184360 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E8A8 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801840B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801840C4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80183FE8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E8A8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80183F98 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184164 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_dryfield_dilapidated_house_80189B30 }, { .vibrationSegments = D_dryfield_dilapidated_house_80189B38 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_8018427C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801843A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184178 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801841C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801841DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184378 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_8018413C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842AC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184150 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_8018418C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801841A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801841B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801843C0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_dilapidated_house_80184C60[24] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E8A8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801843C0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801842E8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_8018427C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_dilapidated_house_8017E8C8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_dilapidated_house_80184EA0[78] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_dilapidated_house_8017EA7C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_dilapidated_house_80183F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80184290 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_dryfield_dilapidated_house_80184400 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184254 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184318 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_8018427C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801843D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801841F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184204 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E8E8 }, { .value = -2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E8E8 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_dryfield_dilapidated_house_80189B40 }, { .vibrationSegments = D_dryfield_dilapidated_house_80189B50 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E8E8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E8E8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184300 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E8E8 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80184038 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_8018404C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017EA10 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017EA10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184268 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184240 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80184060 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_dryfield_dilapidated_house_80189B5C }, { .vibrationSegments = D_dryfield_dilapidated_house_80189B64 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_dilapidated_house_801855F0[17] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E8E8 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_dilapidated_house_8017EA10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_dilapidated_house_80185788[4] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

Task* D_dryfield_dilapidated_house_801857E8 = NULL;

static TmdBone _gDryfieldDilapidatedHouseModel08794Skeleton[1] = {
#include "assets/dryfield_dilapidated_house_model_08794_skeleton.inc"
};

static u32 _gDryfieldDilapidatedHouseModel08794PartVerts[1] = {
#include "assets/dryfield_dilapidated_house_model_08794_partVerts.inc"
};

static SVECTOR _gDryfieldDilapidatedHouseModel08794Verts[40] = {
#include "assets/dryfield_dilapidated_house_model_08794_verts.inc"
};

static SVECTOR _gDryfieldDilapidatedHouseModel08794Normals[128] = {
#include "assets/dryfield_dilapidated_house_model_08794_normals.inc"
};

static u32 _gDryfieldDilapidatedHouseModel08794Stream[171] = {
#include "assets/dryfield_dilapidated_house_model_08794_stream.inc"
};

static TmdSource _gDryfieldDilapidatedHouseModel08794 = {
    0,
    1160,
    0,
    1,
    _gDryfieldDilapidatedHouseModel08794PartVerts,
    _gDryfieldDilapidatedHouseModel08794Verts,
    _gDryfieldDilapidatedHouseModel08794Normals,
    _gDryfieldDilapidatedHouseModel08794Skeleton,
    _gDryfieldDilapidatedHouseModel08794Stream,
};

static TmdBone _gDryfieldDilapidatedHouseModel08D0CSkeleton[1] = {
#include "assets/dryfield_dilapidated_house_model_08D0C_skeleton.inc"
};

static u32 _gDryfieldDilapidatedHouseModel08D0CPartVerts[1] = {
#include "assets/dryfield_dilapidated_house_model_08D0C_partVerts.inc"
};

static SVECTOR _gDryfieldDilapidatedHouseModel08D0CVerts[40] = {
#include "assets/dryfield_dilapidated_house_model_08D0C_verts.inc"
};

static SVECTOR _gDryfieldDilapidatedHouseModel08D0CNormals[40] = {
#include "assets/dryfield_dilapidated_house_model_08D0C_normals.inc"
};

static u32 _gDryfieldDilapidatedHouseModel08D0CStream[171] = {
#include "assets/dryfield_dilapidated_house_model_08D0C_stream.inc"
};

static TmdSource _gDryfieldDilapidatedHouseModel08D0C = {
    0,
    1160,
    0,
    1,
    _gDryfieldDilapidatedHouseModel08D0CPartVerts,
    _gDryfieldDilapidatedHouseModel08D0CVerts,
    _gDryfieldDilapidatedHouseModel08D0CNormals,
    _gDryfieldDilapidatedHouseModel08D0CSkeleton,
    _gDryfieldDilapidatedHouseModel08D0CStream,
};

SVECTOR D_dryfield_dilapidated_house_8018659C[32] = {
    { -35, 11, 91, 0 },
    { -34, 26, 85, 0 },
    { -66, 44, 227, 0 },
    { -52, 46, 231, 0 },
    { -51, 57, 226, 0 },
    { -65, 56, 223, 0 },
    { -12, 13, 97, 0 },
    { -11, 28, 90, 0 },
    { -16, 5, 99, 0 },
    { -31, 3, 96, 0 },
    { -30, 33, 82, 0 },
    { -14, 34, 86, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 29, 9, 90, 0 },
    { 28, 25, 85, 0 },
    { 58, 43, 226, 0 },
    { 48, 44, 229, 0 },
    { 48, 56, 225, 0 },
    { 58, 55, 222, 0 },
    { 8, 12, 96, 0 },
    { 8, 27, 91, 0 },
    { 14, 3, 98, 0 },
    { 24, 1, 95, 0 },
    { 23, 31, 83, 0 },
    { 12, 33, 87, 0 },
};

ModelMorph D_dryfield_dilapidated_house_8018669C = { D_dryfield_dilapidated_house_8018659C, NULL, D_dryfield_dilapidated_house_80189CA0, NULL, 40, 0, 0, 32 };

SVECTOR D_dryfield_dilapidated_house_801866B4[8] = {
    { -81, -162, -82, 0 },
    { -150, -232, -355, 0 },
    { -150, -162, -720, 0 },
    { 351, -622, -720, 0 },
    { 351, -348, -605, 0 },
    { 351, 6, -113, 0 },
    { -351, 6, -113, 0 },
    { -81, -162, -82, 0 },
};

s8 gGlowRingBeamQuads[16][4] = {
    { 0, 6, 1, 11 },
    { 0, 6, 5, 7 },
    { 0, 3, 1, 2 },
    { 0, 3, 5, 4 },
    { 6, 9, 7, 8 },
    { 6, 9, 11, 10 },
    { 2, 1, 14, 13 },
    { 3, 2, 15, 14 },
    { 3, 4, 15, 16 },
    { 4, 5, 16, 17 },
    { 5, 7, 17, 19 },
    { 8, 7, 20, 19 },
    { 9, 8, 21, 20 },
    { 9, 10, 21, 22 },
    { 10, 11, 22, 23 },
    { 1, 11, 13, 23 },
};

u8 gGlowRingBeamColors[24][4] = {
    { 255, 255, 255, 0 },
    { 255, 255, 255, 0 },
    { 255, 255, 120, 0 },
    { 255, 255, 120, 0 },
    { 255, 255, 120, 0 },
    { 255, 255, 255, 0 },
    { 255, 255, 255, 0 },
    { 255, 255, 0, 0 },
    { 255, 255, 0, 0 },
    { 255, 255, 0, 0 },
    { 255, 255, 0, 0 },
    { 255, 255, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

SVECTOR D_dryfield_dilapidated_house_80186794[2] = {
    { -81, -162, -82, 0 },
    { -150, -232, -355, 0 },
};

SVECTOR D_dryfield_dilapidated_house_801867A4[6] = {
    { 0, 0, 0, 0 },
    { 12, 0, 0, 0 },
    { 6, 5, 0, 0 },
    { 0, 5, 0, 0 },
    { -6, 5, 0, 0 },
    { -12, 0, 0, 0 },
};

SVECTOR D_dryfield_dilapidated_house_801867D4[6] = {
    { 0, 0, 0, 0 },
    { -5, 0, 0, 0 },
    { -3, -2, 0, 0 },
    { 0, -4, 0, 0 },
    { 3, -2, 0, 0 },
    { 5, 0, 0, 0 },
};

s32 D_dryfield_dilapidated_house_80186804[16] = {
    128,
    256,
    324,
    238,
    455,
    224,
    348,
    380,
    155,
    370,
    445,
    317,
    288,
    200,
    426,
    222,
};

SVECTOR D_dryfield_dilapidated_house_80186844[2] = {
    { 0, -164, -83, 0 },
    { 0, -164, -280, 0 },
};

TaskDesc D_dryfield_dilapidated_house_80186854[4] = {
    { { { TASK_BODY_TMD, 192 } }, func_dryfield_dilapidated_house_80180F04, { .model = &_gDryfieldDilapidatedHouseModel08794 } },
    { { { TASK_BODY_TMD, 192 } }, func_dryfield_dilapidated_house_80181134, { .model = &_gDryfieldDilapidatedHouseModel08D0C } },
    { { { TASK_BODY_COORD, 192 } }, func_dryfield_dilapidated_house_801812E8, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, func_dryfield_dilapidated_house_8018145C, { .value = 0 } },
};

SVECTOR D_dryfield_dilapidated_house_80186884[24] = {
    { -5500, -2250, -3030, 0 },
    { -4500, -2250, -3030, 0 },
    { -4500, -1030, -3030, 0 },
    { -5500, -1030, -3030, 0 },
    { -5780, 0, -180, 0 },
    { -5000, 0, -180, 0 },
    { -4770, 0, -1660, 0 },
    { -5660, 0, -1660, 0 },
    { -3000, -2250, -3030, 0 },
    { -2000, -2250, -3030, 0 },
    { -2000, -1030, -3030, 0 },
    { -3000, -1030, -3030, 0 },
    { -3370, 0, -180, 0 },
    { -2500, 0, -180, 0 },
    { -2770, 0, -1660, 0 },
    { -3140, 0, -1660, 0 },
    { -500, -2250, -3030, 0 },
    { 500, -2250, -3030, 0 },
    { 500, -1030, -3030, 0 },
    { -500, -1030, -3030, 0 },
    { -870, 0, -180, 0 },
    { 0, 0, -180, 0 },
    { 230, 0, -1660, 0 },
    { -650, 0, -1660, 0 },
};

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
SVECTOR D_dryfield_dilapidated_house_80186944[2] = {
    { -128, 96, 0, 0 },
    { -896, 96, 0, 0 },
};

WorldCollisionRoomResources D_dryfield_dilapidated_house_80186954[1] = {
    { D_dryfield_dilapidated_house_801872E4, D_dryfield_dilapidated_house_80188D08, D_dryfield_dilapidated_house_80188FB4, D_dryfield_dilapidated_house_80189260 },
};

u8* D_dryfield_dilapidated_house_80186964[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_dilapidated_house_80186968[1] = { 21 };

WorldCoordRoomLighting D_dryfield_dilapidated_house_8018696C[1] = {
    { D_dryfield_dilapidated_house_801898FC, D_dryfield_dilapidated_house_801899A0 },
};

DirectionWarpEntry D_dryfield_dilapidated_house_80186974[2] = {
    { { { .word = 0 }, 1596, 0, -2540 }, { 0, 0, 0, 0 }, { { .word = 0 }, 2296, 0, -1578 }, { 0, 0, 0, 0 }, 0x52090002, 0x52090001, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, 467 },
    { { { .word = 1024 }, -5403, 2, -400 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -5184, 2, 200 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldDilapidatedHouseCollision09D24Normals[12] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_normals.inc"
};

static SVECTOR _gDryfieldDilapidatedHouseCollision09D24Verts[116] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_verts.inc"
};

static WorldCollisionGridFace _gDryfieldDilapidatedHouseCollision09D24Faces[70] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_faces.inc"
};

static s16 _gDryfieldDilapidatedHouseCollision09D24Cells[208] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldDilapidatedHouseCollision09D24Cells[i])
static s16* _gDryfieldDilapidatedHouseCollision09D24Table[6] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_dilapidated_house_801872E4[1] = {
    { NULL, _gDryfieldDilapidatedHouseCollision09D24Normals, _gDryfieldDilapidatedHouseCollision09D24Verts, _gDryfieldDilapidatedHouseCollision09D24Faces, _gDryfieldDilapidatedHouseCollision09D24Table, 6000, 3200, 3, 2, 4000, 70 },
};

ViewCamera D_dryfield_dilapidated_house_80187308[21] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 900, 0x4E20, 0 } }, 490 },
    { { { { 3872, 0, -1335 }, { -1200, 1793, -3481 }, { 584, 3682, 1695 } }, { 5380, 4300, 2840 } }, 207 },
    { { { { -2538, 0, 3214 }, { 1329, 3729, 1049 }, { -2926, 1694, -2310 } }, { 200, 2400, -200 } }, 257 },
    { { { { -1264, 0, 3896 }, { 486, 4063, 158 }, { -3865, 511, -1254 } }, { -3950, 2000, -1850 } }, 230 },
    { { { { -1264, 0, -3896 }, { -486, 4063, 158 }, { 3865, 511, -1254 } }, { 3950, 2000, -1850 } }, 230 },
    { { { { -3842, 0, -1419 }, { -415, 3916, 1123 }, { 1357, 1198, -3674 } }, { -600, 2000, -600 } }, 257 },
    { { { { -2926, 0, 2866 }, { 1386, 3585, 1415 }, { -2508, 1980, -2561 } }, { -412, 2960, -716 } }, 230 },
    { { { { -2291, 0, -3395 }, { -66, 4095, 45 }, { 3394, 80, -2291 } }, { 3808, 1373, 933 } }, 329 },
    { { { { -60, 0, 4095 }, { -794, 4018, -11 }, { -4017, -794, -59 } }, { 1221, 1047, 1902 } }, 329 },
    { { { { -138, 0, -4093 }, { 165, 4092, -5 }, { 4090, -165, -137 } }, { 1372, 1030, 834 } }, 257 },
    { { { { 2330, 0, 3368 }, { 49, 4095, -34 }, { -3367, 60, 2330 } }, { -638, 1112, 2964 } }, 289 },
    { { { { 2266, 0, -3411 }, { 1314, 3780, 872 }, { 3148, -1577, 2091 } }, { 2575, 366, 1974 } }, 289 },
    { { { { 4092, 0, -172 }, { -3, 4095, -76 }, { 172, 76, 4091 } }, { 2270, 220, 2900 } }, 289 },
    { { { { -1589, 0, 3775 }, { -499, 4060, -210 }, { -3741, -541, -1575 } }, { -3420, 920, -690 } }, 257 },
    { { { { -1116, 0, -3940 }, { 314, 4082, -89 }, { 3928, -327, -1113 } }, { -1680, 1200, -170 } }, 329 },
    { { { { -3467, 0, -2179 }, { 653, 3907, -1040 }, { 2079, -1228, -3308 } }, { -2000, 700, -1330 } }, 257 },
    { { { { -3364, 0, 2336 }, { 2068, 1903, 2978 }, { -1085, 3626, -1563 } }, { -1220, 2760, -480 } }, 257 },
    { { { { 3587, 0, -1975 }, { 248, 4063, 451 }, { 1960, -515, 3559 } }, { -2080, 920, 1450 } }, 289 },
    { { { { -2069, 0, 3534 }, { -442, 4063, -259 }, { -3507, -513, -2052 } }, { -3190, 1000, -1140 } }, 257 },
    { { { { 4087, 0, -269 }, { 38, 4054, 580 }, { 266, -581, 4045 } }, { -3270, 670, 2170 } }, 257 },
    { { { { 4095, 0, -58 }, { -41, 2903, -2888 }, { 41, 2889, 2903 } }, { -2480, 2960, 2650 } }, 257 },
};

SpriteBatch D_dryfield_dilapidated_house_801875FC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_dilapidated_house_8018760C[67] = {
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, -48, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 64, -56, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 80, -40, 625, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 96, -16, 625, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 112, 8, 625, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -32, -120, 1125, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -24, -120, 1125, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -120, 1125, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -120, 1000, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -72, 937, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 8, -120, 875, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 8, -64, 937, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 16, -8, 956, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -120, 750, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -64, 875, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -8, 1136, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 40, -120, 625, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 40, -8, 1250, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 40, 8, 1147, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 56, -120, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -120, 250, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -120, 250, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 104, -120, 250, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -120, 250, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 136, -120, 250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -120, 250, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, -64, 500, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 0, 550, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, -64, 425, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -64, 425, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, -64, 375, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 136, -64, 350, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, -64, 329, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 0, 522, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, 0, 463, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, 0, 416, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 136, 0, 378, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 152, 0, 353, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, 24, 1125, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 24, 250, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 24, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, 24, 250, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, 24, 250, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 144, 24, 250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, 24, 250, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, 48, 750, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 56, 32, 750, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 72, 24, 750, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 88, 24, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 104, 24, 750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -40, 1000, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -64, 687, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -24, 1204, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, -64, 687, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 56, 250, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, 80, 350, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -48, 72, 425, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -32, 64, 425, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -16, 64, 425, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 0, 56, 400, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 16, 56, 425, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 32, 48, 425, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 48, 40, 250, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, 32, 250, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, 24, 250, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, 24, 250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 112, 32, 250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80187B48[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 49, 0, 0, { 2, 0 } },
    { 54, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_dilapidated_house_80187B70[63] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 0, 1125, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -48, 1125, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -48, 1125, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -16, -120, 1125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -120, 1125, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -72, 1125, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 8, -72, 1125, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 8, -120, 1000, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 24, -120, 1000, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -120, 1000, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -120, 1000, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 56, -120, 1000, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -72, 1062, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -16, 1125, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -120, 875, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -64, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -8, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -120, 875, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -64, 875, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -8, 875, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -120, 750, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 120, -120, 750, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -120, 750, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 152, -120, 750, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -8, 1392, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -24, 1187, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -8, 1250, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 8, 1250, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -8, -24, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -8, 1250, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 8, 1325, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 72, 750, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 56, 750, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, 16, 750, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, 56, 750, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, 8, 750, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, 56, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -128, 8, 750, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 56, 750, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 8, 750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 64, 750, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 64, 750, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -160, 16, 750, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 64, 937, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 48, 937, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, 8, 937, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -48, 875, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 80, 8, 937, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -88, 812, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -32, 875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, 24, 937, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -88, 812, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -32, 875, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 104, 24, 937, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 120, 24, 937, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -32, 812, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -88, 750, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 136, -88, 750, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 136, -32, 750, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 136, 24, 937, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 24, 937, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -32, 750, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -88, 750, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_8018805C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 31, 0, 0, { 1, 0 } },
    { 31, 32, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_dilapidated_house_8018807C[17] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -56, 2125, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, 8, 1625, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, 8, 1625, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -104, 0, 1625, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 8, 1625, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -8, 2125, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -56, -56, 2125, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -56, 2125, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -24, -56, 2125, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -8, -56, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 8, -56, 1915, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 24, -56, 2000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 40, -56, 2125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 56, -56, 2125, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 72, -56, 2125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 88, -56, 2125, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 104, -56, 2125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_801881D0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_dilapidated_house_801881E8[6] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 16, 1425, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 0, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 120, 0, 1475, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, 8, 1375, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 8, 1312, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, 8, 1250, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80188260[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_dilapidated_house_80188278[10] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -8, 750, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 48, 750, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, 8, 750, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, 48, 750, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 104, -16, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 48, 750, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, -8, 875, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 48, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 0, 875, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 48, 875, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80188340[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_dilapidated_house_80188358[15] = {
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 112, -120, 1100, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 96, -120, 1100, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 80, -120, 1100, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -32, 1300, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 64, -120, 1300, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, -120, 1250, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 32, -56, 1450, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 32, -120, 1250, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, -120, 1350, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, -56, 1375, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 0, 1375, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 8, -64, 1475, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, -32, 1100, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, -32, 1100, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 80, -32, 1125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80188484[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_dilapidated_house_8018849C[7] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -120, 375, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -120, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -32, -120, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -120, 375, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -16, -88, 375, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 0, -88, 750, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, -88, 750, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80188528[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_dilapidated_house_80188540[23] = {
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -160, -48, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, -48, 625, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -48, 625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, -48, 625, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -96, -48, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -80, -48, 625, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -64, -48, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, 40, 625, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -144, 40, 625, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -128, 40, 625, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -112, 40, 625, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, 40, 625, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, 40, 625, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 40, 625, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 88, 625, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, 72, 625, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, 72, 625, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, 72, 625, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 72, 625, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 72, 625, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 72, 625, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 72, 625, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, 72, 625, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_8018870C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188724[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_dilapidated_house_80188734[18] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 112, 175, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 40, 312, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -152, 32, 337, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -160, 24, 350, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, 48, 287, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 56, 300, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 64, 312, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, 80, 175, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -152, 80, 175, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 80, 175, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 80, 175, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 80, 175, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, 80, 175, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, 80, 175, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 88, 175, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 96, 175, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 96, 175, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 104, 175, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_8018889C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_801888B4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_dilapidated_house_801888C4[21] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 125, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 72, 125, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -128, 56, 125, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -112, 40, 125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, 40, 125, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, 48, 125, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, 56, 125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -48, 56, 125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -32, 56, 125, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, 56, 125, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 0, 48, 125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 16, 40, 125, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 32, 48, 125, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 48, 56, 125, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, 56, 125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 80, 48, 125, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, 40, 125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 104, 125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, 40, 125, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 48, 125, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 48, 125, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80188A68[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_dilapidated_house_80188A80[13] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, -32, 2000, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -32, 2125, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, 24, 1875, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -8, -32, 2050, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, 24, 2000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, -32, 2000, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 56, 2000, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -32, 1875, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 24, 24, 1875, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 40, -32, 1750, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 24, 1750, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 24, 1750, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -32, 1750, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80188B84[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188B9C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188BAC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188BBC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188BCC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188BDC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188BEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188BFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_dilapidated_house_80188C0C[21] = {
    { { .empty = D_dryfield_dilapidated_house_801875FC }, D_dryfield_dilapidated_house_801875FC, NULL },
    { { .elements = D_dryfield_dilapidated_house_8018760C }, D_dryfield_dilapidated_house_80187B48, NULL },
    { { .elements = D_dryfield_dilapidated_house_80187B70 }, D_dryfield_dilapidated_house_8018805C, NULL },
    { { .elements = D_dryfield_dilapidated_house_8018807C }, D_dryfield_dilapidated_house_801881D0, NULL },
    { { .elements = D_dryfield_dilapidated_house_801881E8 }, D_dryfield_dilapidated_house_80188260, NULL },
    { { .elements = D_dryfield_dilapidated_house_80188278 }, D_dryfield_dilapidated_house_80188340, NULL },
    { { .elements = D_dryfield_dilapidated_house_80188358 }, D_dryfield_dilapidated_house_80188484, NULL },
    { { .elements = D_dryfield_dilapidated_house_8018849C }, D_dryfield_dilapidated_house_80188528, NULL },
    { { .elements = D_dryfield_dilapidated_house_80188540 }, D_dryfield_dilapidated_house_8018870C, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188724 }, D_dryfield_dilapidated_house_80188724, NULL },
    { { .elements = D_dryfield_dilapidated_house_80188734 }, D_dryfield_dilapidated_house_8018889C, NULL },
    { { .empty = D_dryfield_dilapidated_house_801888B4 }, D_dryfield_dilapidated_house_801888B4, NULL },
    { { .elements = D_dryfield_dilapidated_house_801888C4 }, D_dryfield_dilapidated_house_80188A68, NULL },
    { { .elements = D_dryfield_dilapidated_house_80188A80 }, D_dryfield_dilapidated_house_80188B84, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188B9C }, D_dryfield_dilapidated_house_80188B9C, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188BAC }, D_dryfield_dilapidated_house_80188BAC, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188BBC }, D_dryfield_dilapidated_house_80188BBC, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188BCC }, D_dryfield_dilapidated_house_80188BCC, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188BDC }, D_dryfield_dilapidated_house_80188BDC, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188BEC }, D_dryfield_dilapidated_house_80188BEC, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188BFC }, D_dryfield_dilapidated_house_80188BFC, NULL },
};

WorldCollisionTrigger D_dryfield_dilapidated_house_80188D08[9] = {
    { NULL, NULL, NULL, { -4256, -2000, -2432, 0 }, { { 0, -3056, -2464, 0 }, { 0, -3056, 2432, 0 }, { 0, 3056, -2432, 0 }, { 0, 3056, 2464, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3924, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4064, -2080, -2368, 0 }, { { 0, -3104, 2272, 0 }, { 0, -3104, -2272, 0 }, { 0, 3104, 2272, 0 }, { 0, 3104, -2272, 0 } }, { -4106, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3840, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1937, -1376, -1937, 0 }, { { -2182, -2400, 2208, 0 }, { 2183, -2400, -2207, 0 }, { -2182, 2400, 2208, 0 }, { 2183, 2400, -2207, 0 } }, { -2920, 0, -2887, 0 }, { 0, 0, 4096, 0 }, 3916, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2017, -1360, -2017, 0 }, { { 2183, -2384, -2207, 0 }, { -2182, -2384, 2208, 0 }, { 2183, 2384, -2207, 0 }, { -2182, 2384, 2208, 0 } }, { 2913, 0, 2880, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 220, -1392, 318, 0 }, { { -30, -2416, -3113, 0 }, { 4, -2416, 3091, 0 }, { -30, 2416, -3113, 0 }, { 4, 2416, 3091, 0 } }, { 4098, 0, -23, 0 }, { 0, 0, 4096, 0 }, 3932, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2974, -1312, -1986, 0 }, { { -2235, -2336, -137, 0 }, { 2226, -2336, 127, 0 }, { -2235, 2336, -137, 0 }, { 2226, 2336, 127, 0 } }, { 241, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 3228, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 318, -1360, 352, 0 }, { { 13, -2384, 3100, 0 }, { -23, -2384, -3109, 0 }, { 13, 2384, 3100, 0 }, { -23, 2384, -3109, 0 } }, { -4098, 0, 23, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2975, -1472, -2048, 0 }, { { 2226, -2496, 127, 0 }, { -2235, -2496, -137, 0 }, { 2226, 2496, 127, 0 }, { -2235, 2496, -137, 0 } }, { -243, 0, 4090, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4961, -2048, -2225, 0 }, { { -670, -3056, -1440, 0 }, { 660, -3056, 1411, 0 }, { -659, 3056, -1410, 0 }, { 671, 3056, 1441, 0 } }, { 3712, 1, -1733, 0 }, { 0, 0, 4096, 0 }, 3444, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_dilapidated_house_80188FB4[9] = {
    { NULL, NULL, NULL, { 1504, -52, -2768, 0 }, { { -768, 0, -400, 0 }, { 768, 0, -400, 0 }, { -768, 0, 400, 0 }, { 768, 0, 400, 0 } }, { 0, 4116, 0, 0 }, { 0, 0, 4096, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_WARP, 5, 20, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5600, -52, 192, 0 }, { { 224, 0, -624, 0 }, { 224, 0, 624, 0 }, { -224, 0, -624, 0 }, { -224, 0, 624, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 662, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4688, -64, 352, 0 }, { { -512, 0, -608, 0 }, { 512, 0, -608, 0 }, { -512, 0, 608, 0 }, { 512, 0, 608, 0 } }, { 0, 4101, 0, 0 }, { -4091, 0, 201, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4704, -64, -944, 0 }, { { -512, 0, -560, 0 }, { 512, 0, -560, 0 }, { -512, 0, 560, 0 }, { 512, 0, 560, 0 } }, { 0, 4110, 0, 0 }, { -4077, 0, -402, 0 }, 757, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3472, -64, -496, 0 }, { { -336, 0, -928, 0 }, { 1456, 0, -928, 0 }, { -336, 0, 928, 0 }, { 1136, 0, 928, 0 } }, { 0, 4107, 0, 0 }, { -4091, 0, 201, 0 }, 1722, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1328, -64, -2288, 0 }, { { -912, 0, -640, 0 }, { 912, 0, -640, 0 }, { -912, 0, 640, 0 }, { 912, 0, 640, 0 } }, { 0, 4103, 0, 0 }, { -4091, 0, 201, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 18, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3969, -64, -2273, 0 }, { { -502, 0, -1200, 0 }, { 521, 0, -1249, 0 }, { -520, 0, 1249, 0 }, { 502, 0, 1199, 0 } }, { 0, 4099, 0, 0 }, { -4091, 0, 201, 0 }, 1348, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -5168, -64, 768, 0 }, { { -800, 0, -480, 0 }, { 800, 0, -480, 0 }, { -800, 0, 480, 0 }, { 800, 0, 480, 0 } }, { 0, 4100, 0, 0 }, { -201, 0, -4091, 0 }, 931, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5232, -64, -2816, 0 }, { { -720, 0, -352, 0 }, { 720, 0, -352, 0 }, { -720, 0, 352, 0 }, { 720, 0, 352, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, 4091, 0 }, 799, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_dilapidated_house_80189260[1] = {
    { NULL, NULL, { -4096, -2000, 1232, 0 }, { { 0, 2576, -2544, 0 }, { 0, -2576, -2544, 0 }, { 0, 2576, 2544, 0 }, { 0, -2576, 2544, 0 } }, { 4097, 0, 0, 0 }, 3620, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Authored point lights for model shading in every view of the Dryfield dilapidated house.
///
/// Positions and falloff radii use integer world units; RGB intensities have 12
/// fractional bits (`ONE` is full strength). The room light collection borrows
/// this writable table while the overlay is loaded: coordinate updates set its
/// parents and composed matrices, and lighting queries overwrite attenuation.
static WorldCoordPointLight _gDryfieldDilapidatedHousePointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5000, -1500, 500 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1847, 1847, 1847 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2000,
        .outer = 3549,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2500, -1500, -2500 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2666, 2666, 2666 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2256,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5000, -1500, -2500 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2666, 2666, 2666 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2000, -1500, -2500 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1028, 1028, 1028 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2256,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1500, -2500 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2666, 2666, 2666 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2256,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2500, -1500, 2000 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1028, 1028, 1028 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2000,
        .outer = 6400,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1500, 2000 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1028, 1028, 1028 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2000,
        .outer = 3003,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2500, -1500, 2000 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1028, 1028, 1028 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2000,
        .outer = 3000,
    },
};

/// Authored cone-light table for every view of the Dryfield dilapidated house.
///
/// The single entry has zero RGB intensity. Position and falloff radii use
/// integer world units; the axis has 12 fractional bits and the opening uses
/// 0x1000 units per turn. The room light collection borrows this writable table
/// while the overlay is loaded: coordinate updates rebuild its orientation and
/// composed transform, and lighting queries overwrite attenuation.
static WorldCoordSpotLight _gDryfieldDilapidatedHouseConeLights[1] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { -ONE, 0, 0 }, { 0, -2902, 2901 }, { 0, 2901, 2901 } }, { -5000, -2500, -4000 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 0, 0, 0 },
            .unknown_56 = { 0, 0 },
        },
        .axis  = { 0, 2896, 2896, 0 },
        .inner = 100,
        .outer = 3000,
        .angle = 625,
    },
};

/// Unreferenced bytes following the room's cone light in the overlay image.
///
/// Their original purpose is unproven. Preserve the representation, including
/// embedded address values, without exposing it as additional lights.
static u8 _gDryfieldDilapidatedHouseUnreferencedData[] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x08,
    0x00,
    0x00,
    0x00,
    0xA4,
    0x50,
    0x19,
    0x80,
    0x01,
    0x00,
    0x00,
    0x00,
    0xA4,
    0x53,
    0x00,
    0x00,
    0x2D,
    0xFC,
    0x80,
    0x10,
    0xD8,
    0xFF,
    0x00,
    0x00,
    0x55,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xBE,
    0xE7,
    0x30,
    0xF3,
    0xDE,
    0x15,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x07,
    0x00,
    0x00,
    0x00,
    0xD4,
    0x03,
    0x00,
    0x00,
    0xF9,
    0xFF,
    0x00,
    0x00,
    0x2D,
    0xFC,
    0x00,
    0x00,
    0x07,
    0x00,
    0x00,
    0x00,
    0xD4,
    0x03,
    0xD0,
    0x10,
    0xF9,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xEB,
    0xEF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x34,
    0x11,
    0x00,
    0x00,
    0x06,
    0x05,
    0x00,
    0x00,
    0xE0,
    0x54,
    0x19,
    0x80,
    0x48,
    0x54,
    0x19,
    0x80,
    0xF8,
    0x30,
    0x07,
    0x80,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xC0,
    0x03,
    0xE0,
    0x11,
    0x3A,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x51,
    0xF0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x38,
    0x12,
    0x00,
    0x00,
    0x07,
    0x06,
    0x61,
    0x00,
    0x2C,
    0x55,
    0x19,
    0x80,
    0x94,
    0x54,
    0x19,
    0x80,
    0x00,
    0x00,
    0x00,
    0x00,
    0x5D,
    0xE8,
    0x30,
    0xF2,
    0x3C,
    0xFE,
    0x00,
    0x00,
    0xB5,
    0x03,
    0x30,
    0xEE,
    0x0C,
    0xFF,
    0x00,
    0x00,
    0x4B,
    0xFC,
    0x00,
    0x00,
    0xF5,
    0x00,
    0x00,
    0x00,
    0xB5,
    0x03,
    0xD0,
    0x11,
    0x0C,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x78,
    0x55,
    0x19,
    0x80,
    0xE0,
    0x54,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x40,
    0xEB,
    0x60,
    0xF2,
    0x6F,
    0xF4,
    0x00,
    0x00,
    0xF8,
    0xFF,
    0x80,
    0xEF,
    0x7C,
    0xF7,
    0x00,
    0x00,
    0x08,
    0x00,
    0x80,
    0xEF,
    0x84,
    0x08,
    0x00,
    0x00,
    0xF8,
    0xFF,
    0x80,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x08,
    0x00,
    0x80,
    0x10,
    0x84,
    0x08,
    0x00,
    0x00,
    0x02,
    0x10,
    0x00,
    0x00,
    0xF0,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x8C,
    0x12,
    0x00,
    0x00,
    0x02,
    0x07,
    0x61,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x0C,
    0xF7,
    0x00,
    0x00,
    0x08,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xF8,
    0xFF,
    0x80,
    0x10,
    0x0C,
    0xF7,
    0x00,
    0x00,
    0xFA,
    0xEF,
    0x00,
    0x00,
    0x0E,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0xC2,
    0x12,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x56,
    0x19,
    0x80,
    0x78,
    0x55,
    0x19,
    0x80,
    0xF8,
    0x30,
    0x07,
    0x80,
    0x40,
    0xEB,
    0xE0,
    0xF4,
    0xE0,
    0x28,
    0x00,
    0x00,
    0xEB,
    0x00,
    0x80,
    0xEF,
    0xCE,
    0xFA,
    0x00,
    0x00,
    0x10,
    0xFF,
    0x80,
    0xEF,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x52,
    0x11,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x5C,
    0x56,
    0x19,
    0x80,
    0xC4,
    0x55,
    0x00,
    0x00,
    0xF8,
    0x30,
    0x07,
    0x80,
    0xA0,
    0xEB,
    0x00,
    0x00,
    0x40,
    0x29,
    0x00,
    0x00,
    0x11,
    0xFF,
    0x80,
    0xEF,
    0x2E,
    0x05,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xCF,
    0xFA,
    0x00,
    0x00,
    0x11,
    0xFF,
    0x80,
    0x10,
    0x2E,
    0x05,
    0x00,
    0x00,
    0xEC,
    0x00,
    0x80,
    0x10,
    0xCF,
    0xFA,
    0x00,
    0x00,
    0x3F,
    0xF0,
    0x00,
    0x00,
    0x2E,
    0xFD,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x22,
    0x05,
    0x60,
    0xEF,
    0xA3,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x5D,
    0xFD,
    0x00,
    0x00,
    0x22,
    0x05,
    0x00,
    0x00,
    0xA3,
    0x02,
    0x00,
    0x00,
    0xDF,
    0xFA,
    0x00,
    0x00,
    0x5D,
    0xFD,
    0x00,
    0x00,
    0xAA,
    0xF8,
    0x00,
    0x00,
    0x43,
    0x0E,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x8C,
    0x11,
    0x00,
    0x00,
    0x04,
    0x03,
    0x61,
    0x00,
    0xF4,
    0x56,
    0x19,
    0x80,
    0x5C,
    0x56,
    0x00,
    0x00,
    0xF8,
    0x30,
    0x07,
    0x80,
    0xCC,
    0xFB,
    0x40,
    0xF3,
    0x6D,
    0x14,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x78,
    0x07,
    0x00,
    0x00,
    0xD0,
    0xF1,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x6F,
    0x11,
    0x00,
    0x00,
    0x03,
    0x04,
    0x61,
    0x00,
    0x40,
    0x57,
    0x00,
    0x00,
    0xA8,
    0x56,
    0x19,
    0x80,
    0xF8,
    0x30,
    0x07,
    0x80,
    0x40,
    0xEC,
    0x60,
    0xF3,
};

WorldCoordRoomLights D_dryfield_dilapidated_house_801898FC[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldDilapidatedHousePointLights), _gDryfieldDilapidatedHousePointLights, ARRAY_SIZE(_gDryfieldDilapidatedHouseConeLights), _gDryfieldDilapidatedHouseConeLights },
};

AreaResource D_dryfield_dilapidated_house_80189914[3] = {
    { 34, 211, AREA_RESOURCE_FILE_GROUP_BASE_50, 0, { 0, 0 }, D_actor_521100_8015F6E4 },
    { 29, 212, AREA_RESOURCE_FILE_GROUP_BASE_50, 0, { 0, 0 }, D_actor_521100_8016A388 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_dilapidated_house_80189938[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017AF64, D_dryfield_dilapidated_house_80189914 },
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
    { NULL, NULL },
};

WorldCoordRoomAmbientEntry D_dryfield_dilapidated_house_801899A0[22] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_dilapidated_house_801899A0) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1000, 1000, 1000, 1000 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 0, 0, 0, 0 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 3000, 3000, 600, 2700 } },
    { .color = { 2000, 750, 550, 1193 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 750, 750, 750, 750 } },
};

WorldCollisionFootstepSounds D_dryfield_dilapidated_house_80189A50 = {
    0x10000045,
    0x10000047,
    0x10000045,
};

WorldCollisionFootstepSounds D_dryfield_dilapidated_house_80189A5C = {
    0x1000004D,
    0x1000004F,
    0x1000004D,
};

WorldCollisionSurfaceProperties D_dryfield_dilapidated_house_80189A68[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_dilapidated_house_80189A70[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_dilapidated_house_80189A50 },
};

WorldCollisionSurfaceProperties D_dryfield_dilapidated_house_80189A78[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_dilapidated_house_80189A5C },
};

WorldCollisionSurfaceProperties* D_dryfield_dilapidated_house_80189A80[8] = {
    D_dryfield_dilapidated_house_80189A68,
    D_dryfield_dilapidated_house_80189A70,
    D_dryfield_dilapidated_house_80189A78,
    D_dryfield_dilapidated_house_80189A68,
    D_dryfield_dilapidated_house_80189A68,
    D_dryfield_dilapidated_house_80189A68,
    D_dryfield_dilapidated_house_80189A68,
    D_dryfield_dilapidated_house_80189A68,
};

AreaApplyRec D_dryfield_dilapidated_house_80189AA0[33] = {
    { 3, 1, 1, 0 },
    { 3, 2, 1, 17 },
    { 3, 2, 7, 33 },
    { 3, 3, 1, 1 },
    { 3, 5, 1, 17 },
    { 3, 5, 7, 33 },
    { 3, 6, 1, 1 },
    { 3, 7, 1, 1 },
    { 3, 9, 1, 0 },
    { 3, 11, 1, 1 },
    { 3, 12, 3, 1 },
    { 3, 13, 1, 1 },
    { 3, 14, 1, 1 },
    { 3, 15, 3, 0 },
    { 3, 16, 1, 1 },
    { 3, 17, 1, 0 },
    { 3, 18, 2, 1 },
    { 3, 19, 1, 1 },
    { 3, 20, 1, 1 },
    { 3, 21, 1, 0 },
    { 3, 22, 1, 1 },
    { 3, 23, 1, 0 },
    { 3, 24, 1, 1 },
    { 3, 25, 1, 1 },
    { 3, 26, 1, 0 },
    { 3, 27, 1, 0 },
    { 3, 28, 1, 1 },
    { 3, 29, 1, 1 },
    { 3, 30, 1, 0 },
    { 3, 31, 1, 0 },
    { 3, 32, 1, 1 },
    { 3, 34, 1, 1 },
    { 255, 0, 0, 0 },
};

AreaApplyRec D_dryfield_dilapidated_house_80189B24[3] = {
    { 3, 38, 2, 17 },
    { 3, 38, 7, 33 },
    { 255, 0, 0, 0 },
};

PadScriptCmd D_dryfield_dilapidated_house_80189B30[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_dryfield_dilapidated_house_80189B38[2] = {
    { 0, 0, 5, 0 },
    { 200, 255, 8, 1 },
};

PadScriptCmd D_dryfield_dilapidated_house_80189B40[4] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2), PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_dryfield_dilapidated_house_80189B50[3] = {
    { 200, 255, 7, 1 },
    { 200, 90, 5, 1 },
    { 180, 60, 1, 0 },
};

PadScriptCmd D_dryfield_dilapidated_house_80189B5C[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 2) }
};

PadScriptVibrationSegment D_dryfield_dilapidated_house_80189B64[2] = {
    { 0, 0, 5, 0 },
    { 200, 255, 8, 1 },
};

s32 D_dryfield_dilapidated_house_80189B6C = 0;

s32 D_dryfield_dilapidated_house_80189B70 = 0;

ScreenWaveCtx* gScreenWaveCtx = NULL;

Task* D_dryfield_dilapidated_house_80189B78 = NULL;

Task* D_dryfield_dilapidated_house_80189B7C = NULL;

ScreenNegativeCaptureArgs D_dryfield_dilapidated_house_80189B80 = { 0, 0 };

ScreenWaveOscillator gScreenWaveColumns[13] = { 0 };

ScreenWaveOscillator gScreenWaveRows[32] = { 0 };

ScreenWaveCtx D_dryfield_dilapidated_house_80189C94 = { 0 };

SVECTOR D_dryfield_dilapidated_house_80189CA0[40];

GfxCoord D_dryfield_dilapidated_house_80189DE0[8];

GfxCoord D_dryfield_dilapidated_house_8018A060[8];

/// Prism corners in model space, eight per prism: `[0..3]` the lit ring and
/// `[4..7]` the far ring. Callers pick a prism by passing 0, 8 or 0x10.
extern SVECTOR D_dryfield_dilapidated_house_80186884[];

/// Eight-slot trail coordinates, one array per end of the pair. Every entry is
/// parented to `gGfxViewCoord`.
extern GfxCoord D_dryfield_dilapidated_house_80189DE0[8];

extern GfxCoord D_dryfield_dilapidated_house_8018A060[8];

#include "../../shared/screen_wave.inc.c"

#include "../../shared/screen_negative_capture.inc.c"

/// The scene's negative freeze-frame (see screen_negative.h).
void func_dryfield_dilapidated_house_8017DE88(Task* task)
{
    screenNegativeCaptureTask(task);
}

/// State handlers of the room task, indexed by `Task::state`: set-up, the room
/// gate, then `taskKill`.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D5C4 = {
    { func_dryfield_dilapidated_house_8017EAB4, func_dryfield_dilapidated_house_8017E014, taskKill },
};

/// Room gate task. While the session is in the room (`gGameSession->eventState`
/// is 0) it walks `D_dryfield_dilapidated_house_80183EFC` from 1 to 2 and then
/// to 3: the 1 -> 2 step is unconditional, the 2 -> 3 step waits for the room's
/// message (0x7D6) to be dispatched and answered with 0 by the slot-0 object,
/// and for no sound to be playing; reaching 3 spawns entry 3 of the room's task
/// table. Independently, once the stream file is open it starts the named
/// sequences `"AUNT"` and `"Player"` on the two slot objects.
static void func_dryfield_dilapidated_house_8017E014(Task* task)
{
    if (gGameSession->eventState == 0) {
        if (D_dryfield_dilapidated_house_80183EFC == 1) {
            D_dryfield_dilapidated_house_80183EFC = 2;
        } else if ((D_dryfield_dilapidated_house_80183EFC == 2) &&
                   (taskMessageDispatch(Gp_LookupSlot4(0), ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0)) {
            if (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    D_dryfield_dilapidated_house_80183EFC += 1;
                    taskSpawnFromTable(D_dryfield_dilapidated_house_80183EB4, 3, 0, 0);
                }
            }
        }
    }
    if ((gDisplayState.debugMode != 0) && (Gp_LookupSlot4(1) != 0)) {
        func_80724608(Gp_LookupSlot4(1), -0x8C, 0xA, "AUNT");
        func_80724608(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), -0x8C, 0x14, "Player");
    }
}

/// Screen-blackout timer of task-table entry 2: `func_dryfield_dilapidated_house_8017E970`
/// arms it by writing state 2 and a frame count into `spawnArg1` (a 0 arg resets
/// it to state 0 instead). State 2 copies that count into the shared countdown
/// `D_dryfield_dilapidated_house_80189B70` and falls through to state 3, whose
/// `var_s1` is the shared "paint the screen black" flag; state 4 runs the
/// countdown and at 0 calls `func_dryfield_dilapidated_house_8017E9A4(0xF)`,
/// which starts the room's captured-image scene, then raises the flag again once
/// the count is 15 frames past that hand-off, keeping the screen black over it.
/// The flag paints the whole frame with a zeroed `TILE` carved out of
/// `gGpuPrimCursor` and links it into `gGpuCurrentOt`. When `D_801156F9` is set
/// the task does nothing at all.
void func_dryfield_dilapidated_house_8017E144(Task* task)
{
    TILE* tile;
    s32   var_s1;
    s32   temp_v0;
    s32   temp_v1;

    var_s1 = 0;
    if (D_801156F9 == 0) {
        temp_v1 = task->state;
        switch (temp_v1) {
            case 0:
                task->state = task->state + 1;
                break;
            case 1:
                break;
            case 2:
                D_dryfield_dilapidated_house_80189B70 = task->spawnArg1.value;
                task->state                           = task->state + 1;
                /* fallthrough */
            case 3:
                var_s1      = 1;
                task->state = task->state + var_s1;
                break;
            case 4:
                temp_v0                               = D_dryfield_dilapidated_house_80189B70 - 1;
                D_dryfield_dilapidated_house_80189B70 = temp_v0;
                if (temp_v0 == 0) {
                    func_dryfield_dilapidated_house_8017E9A4(0xF);
                }
                if (D_dryfield_dilapidated_house_80189B70 < -0xF) {
                    var_s1 = 1;
                }
                break;
        }
        if (var_s1 != 0) {
            tile           = gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            SetTile(tile);
            tile->x0 = -0xA0;
            tile->y0 = -0x80;
            tile->w  = 0x140;
            tile->h  = 0x100;
            tile->r0 = 0;
            tile->g0 = 0;
            tile->b0 = 0;
            addPrim(gGpuCurrentOt, tile);
        }
    }
}

/// Scene-clear task: the room's hand-off to the rest of the game. State 0
/// starts the streamed scene named by the two blocks `func_800E8634` takes,
/// state 1 fires when the session is back in play (`gGameSession->eventState`
/// is 2) and hands slot 0 the release event 0x1B, state 6 waits for the room
/// message (`gGameSession->battleResetPending`), and state 7 -- reached once the save
/// has not already banked this clear (`gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene`) -- applies the
/// room's two area records, raises the progression flags, refills the party
/// and hands off to the results screen with `Task_Spawn(0, 0x11, 0, 0)`.
void func_dryfield_dilapidated_house_8017E2B0(Task* task)
{
    switch (task->state) {
        case 0:
            func_800E8634(D_dryfield_dilapidated_house_80184EA0, 0, D_dryfield_dilapidated_house_801855F0);
            task->state += 1;
            return;
        case 1:
            if (gGameSession->eventState == 2) {
                Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0x1B);
                gSceneCombatState.signals.bytes.endDelayFrames = 3;
                task->state                                   += 1;
            }
            return;
        case 2:
        case 3:
        case 4:
        case 5:
            task->state += 1;
            return;
        case 6:
            if (gGameSession->battleResetPending != 0) {
                task->state += 1;
            }
            return;
        case 7:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
                Gp_ApplyAreaRecs(D_dryfield_dilapidated_house_80189AA0);
                if (gameFlagGetNibble(GAME_FLAG_GRAY_STALKER_DEFEATED) != 0) {
                    Gp_ApplyAreaRecs(D_dryfield_dilapidated_house_80189B24);
                }
                gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 6);
                gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 1);
                gameFlagSetNibble(GAME_FLAG_GAS_STATION_MAIN_STREET_BLOCKED, 1);
                gameFlagSetNibble(GAME_FLAG_GENERAL_STORE_UNDERPASS_BLOCKED, 1);
                gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_CUTSCENE_SEEN, 1);
                gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS, 2);
                gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
                gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 0);
                Gp_FillPlayerHpMp();
                Gp_FillAllyHp();
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent         = 1;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_DRYFIELD;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_DRYFIELD_R08;
                gDisplayState.spriteVariant                                 = 1;
                Task_Spawn(0, 0x11, 0, 0);
            }
            taskKill(task);
            return;
    }
}

#include "../../shared/screen_negative_filter.inc.c"

/// Refuses key-item use in this room by returning zero without changing anything.
static s32 _dryfieldDilapidatedHouseRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return 0;
}

/// Message gate for the room's second hotspot. It copies the incoming record to
/// the outgoing one and then writes the answer the caller acts on to the copy's
/// `room`, returning 0 when the message was consumed and 1 when it was not.
///
/// The copy is the `RoomEventMsg` assignment; the rest is two independent id
/// checks. While the session is in the room (`gGameSession->location.loc.stage` is 2), a
/// type-7 record with no sub-id answers 1, or the session's own value when flag
/// nibble 0x3C is set. A type-7 record in play (`gSceneCombatState.signals.bytes.battlePhase` is 1) runs
/// CAP command 0x14 and a type-5 record runs 0x13, each only when the sub-id is
/// clear; everything else is left to the caller and answers 1.
s32 func_dryfield_dilapidated_house_8017E574(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 s1;

    *out = *in;
    s1   = gGameSession->location.loc.stage;
    if (s1 == 2) {
        if (in->areaId == GAME_AREA_DRYFIELD_WAREHOUSE) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_WAREHOUSE_EVENT_SEEN) == 0) {
                    out->room = 1;
                } else {
                    out->room = s1;
                }
            }
        }
    }
    if ((in->areaId == GAME_AREA_DRYFIELD_WAREHOUSE) && (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED)) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_SpawnIfCapIdle(0x14, 0);
        }
        return 0;
    }
    if (in->areaId == GAME_AREA_DRYFIELD_BACK_STREET) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_SpawnIfCapIdle(0x13, 0);
        }
        return 0;
    }
    return 1;
}

/// Ignores room-command messages and returns zero; both payload words are unused.
static s32 _dryfieldDilapidatedHouseIgnoreRoomCommand(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    return 0;
}

s32 func_dryfield_dilapidated_house_8017E68C(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    u8 actionId;

    actionId = request->actionId;
    if ((actionId == 1) && (D_dryfield_dilapidated_house_80183EFC == 0)) {
        D_dryfield_dilapidated_house_80183EFC = actionId;
        func_800E8634(D_dryfield_dilapidated_house_80184408, 0, D_dryfield_dilapidated_house_80184C60);
    }
    return 0;
}

void func_dryfield_dilapidated_house_8017E6DC(Task* arg0)
{
    Task* temp_s1;
    Task* temp_a1;
    s32   temp_v1;

    temp_s1 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    temp_a1 = Gp_LookupSlot4(1);
    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            arg0->spawnArg1.value = 0;
            arg0->state          += 1;
            return;
        case 2:
            animationAimHeadAtTask(temp_s1, temp_a1, 0x200, 0x180, 0x1000);
            /* fallthrough */
        case 1:
            return;
    }
}

void func_dryfield_dilapidated_house_8017E780(Task* arg0)
{
    s32 temp_v0;
    s32 temp_v1;
    s32 var_a0;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            D_dryfield_dilapidated_house_80189B6C = arg0->spawnArg1.value;
            arg0->state                          += 1;
            return;
        case 1:
            var_a0 = (s32)(D_dryfield_dilapidated_house_80189B6C * 3) / (s32)arg0->spawnArg1.value;
            if (D_dryfield_dilapidated_house_80189B6C & 1) {
                var_a0 = -var_a0;
            }
            displaySetShakeY(var_a0);
            temp_v0                               = D_dryfield_dilapidated_house_80189B6C - 1;
            D_dryfield_dilapidated_house_80189B6C = temp_v0;
            if (temp_v0 == 0) {
                taskKill(arg0);
            }
            return;
    }
}

void func_dryfield_dilapidated_house_8017E858(Task* arg0)
{
    s32 var_v0;

    var_v0 = arg0->spawnArg1.value;
    if (var_v0 < 0) {
        Stage_SetEndingFlag();
        taskKill(arg0);
        var_v0 = arg0->spawnArg1.value;
    }
    var_v0                = var_v0 - 1;
    arg0->spawnArg1.value = var_v0;
}

/// State handlers of the task `func_dryfield_dilapidated_house_80181134` dispatches.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D61C = {
    { _dryfieldDilapidatedHouseCurveDebugInit, func_dryfield_dilapidated_house_80181264, taskKill },
};

/// State handlers of the task `func_dryfield_dilapidated_house_801812E8` dispatches.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D628 = {
    { _dryfieldDilapidatedHouseRingBeamInit, func_dryfield_dilapidated_house_801813DC,
      _dryfieldDilapidatedHouseRingBeamExit },
};

/// State handlers of the task `func_dryfield_dilapidated_house_8018145C` dispatches.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D634 = {
    { func_dryfield_dilapidated_house_801814B4, func_dryfield_dilapidated_house_80181584,
      _dryfieldDilapidatedHouseMorphConeExit },
};

/// State handlers of the task `func_dryfield_dilapidated_house_80180F04`
/// dispatches.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D640 = {
    { func_dryfield_dilapidated_house_80180B84, func_dryfield_dilapidated_house_80180F5C, taskKill },
};

/// Script command that moves the cutscene task to the given state; does
/// nothing when that task was never spawned.
void func_dryfield_dilapidated_house_8017E8A8(s32 arg0)
{
    if (D_dryfield_dilapidated_house_80189B78 != NULL) {
        D_dryfield_dilapidated_house_80189B78->state = arg0;
    }
}

/// Script command that calls `Gp_PulseState1C`.
void func_dryfield_dilapidated_house_8017E8C8(void)
{
    Gp_PulseState1C();
}

/// Message handler for the start-countdown cue; actor 136300 carries the same
/// body. A positive argument is stored in `state`
/// (`SCREEN_WAVE_RAMP_FALLING` counts the ramp down, `SCREEN_WAVE_RAMP_FINISHED`
/// ends the task); otherwise the CD command queue is dropped into
/// Mdec_DecodeToVram mode 2 and -- except for the -2 "already ran" message --
/// the spawn block is filled and the `D_dryfield_dilapidated_house_80183E48` entry started.
///
/// Both halves of the block are written in *each* arm of the `span` test so
/// that each arm is a complete two-store address session: jump optimization
/// then merges the identical tails and the `span` collapses to one `li` per
/// arm, which is what puts the block's `lui` in the delay slot of the entry
/// test. Hoisting `scale` out of the arms compiles to a different allocation.
void func_dryfield_dilapidated_house_8017E8E8(s32 arg0)
{
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if (arg0 <= 0) {
        queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
        if (arg0 != -2) {
            if (arg0 == 0) {
                D_dryfield_dilapidated_house_80189C94.span  = 0x64;
                D_dryfield_dilapidated_house_80189C94.scale = 0x100;
            } else {
                D_dryfield_dilapidated_house_80189C94.span  = 5;
                D_dryfield_dilapidated_house_80189C94.scale = 0x100;
            }
            taskSpawnFromTable(D_dryfield_dilapidated_house_80183E48, 0, 0, &D_dryfield_dilapidated_house_80189C94);
        }
    } else {
        D_dryfield_dilapidated_house_80189C94.state = arg0;
    }
}

void func_dryfield_dilapidated_house_8017E970(s32 arg0)
{
    if (arg0 == 0) {
        D_dryfield_dilapidated_house_80189B7C->state = 0;
        D_dryfield_dilapidated_house_80189B80.done   = 1;
        return;
    }
    D_dryfield_dilapidated_house_80189B7C->state           = 2;
    D_dryfield_dilapidated_house_80189B7C->spawnArg1.value = arg0;
}

static void func_dryfield_dilapidated_house_8017E9A4(s32 arg0)
{
    if (arg0 != 0) {
        Gp_SpawnScript18(D_80114A24, D_80114A34);
        D_dryfield_dilapidated_house_80189B80.duration = arg0;
        taskSpawnFromTable(D_dryfield_dilapidated_house_80183E64, 0, 0, &D_dryfield_dilapidated_house_80189B80);
        return;
    }
    D_dryfield_dilapidated_house_80189B80.done = 1;
}

void func_dryfield_dilapidated_house_8017EA10(s32 arg0)
{
    if (arg0 != 0) {
        D_dryfield_dilapidated_house_801857E8 =
            taskSpawnFromTable(D_dryfield_dilapidated_house_80186854, 0, 3, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
        return;
    }
    if (D_dryfield_dilapidated_house_801857E8 != NULL) {
        taskKill(D_dryfield_dilapidated_house_801857E8);
        D_dryfield_dilapidated_house_801857E8 = NULL;
    }
}

/// Script command that calls `Gp_PulseState1C` and sets bit 0 of
/// `Gp_StateC08.flags`.
void func_dryfield_dilapidated_house_8017EA7C(void)
{
    Gp_PulseState1C();
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

static void func_dryfield_dilapidated_house_8017EAB4(Task* arg0)
{
    arg0->msgTable = D_dryfield_dilapidated_house_80183E8C;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (Gp_LookupSlot4(1) != 0) {
        D_dryfield_dilapidated_house_80189B78 =
            taskSpawnFromTable(D_dryfield_dilapidated_house_80183EB4, 0, 0, 0);
    }
    D_dryfield_dilapidated_house_80189C94.state = SCREEN_WAVE_RAMP_FINISHED;
    D_dryfield_dilapidated_house_80189B7C =
        taskSpawnFromTable(D_dryfield_dilapidated_house_80183EB4, 2, 0, 0);
    gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_REEQUIP_WEAPON);
    arg0->state            += 1;
}

/// The room task: runs its current state out of
/// `D_dryfield_dilapidated_house_8017D5C4`, copied onto the stack - setup, the
/// room gate, then `taskKill`.
void func_dryfield_dilapidated_house_8017EB60(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D5C4;
    sp.funcs[task->state](task);
}

/// Projects the eight local-space markers at
/// `D_dryfield_dilapidated_house_801866B4` through the parent task's
/// `_DryfieldDilapidatedHouseMorphWork::attachMtx` and `gGfxViewCoord.workm`,
/// then queues two red `LINE_F2`s as an X at each screen point in
/// `gGpuCurrentOt[10]`.
static void func_dryfield_dilapidated_house_8017EBB8(Task* task)
{
    struct {
        SVECTOR vec;
        s32     sxy;
        s32     dp;
        s32     flag;
        s32     otz;
    } sc;
    MATRIX*  mtx;
    LINE_F2* line;
    u16      sx;
    s32      sy;
    s16      x0;
    s16      y0;
    s16      x1;
    s16      y1;
    s32      i;

    i   = 0;
    mtx = &((_DryfieldDilapidatedHouseMorphWork*)((Task*)task->spawnArg2.pointer)->work)->attachMtx;
    do {
        sc.vec.vx = D_dryfield_dilapidated_house_801866B4[i].vx;
        sc.vec.vy = D_dryfield_dilapidated_house_801866B4[i].vy;
        sc.vec.vz = D_dryfield_dilapidated_house_801866B4[i].vz;
        gte_SetRotMatrix(mtx);
        gte_ldv0(&sc.vec);
        gte_rtv0();
        gte_stsv(&sc.vec);
        sc.vec.vx = (u16)sc.vec.vx + (u16)mtx->t[0];
        sc.vec.vy = (u16)sc.vec.vy + (u16)mtx->t[1];
        sc.vec.vz = (u16)sc.vec.vz + (u16)mtx->t[2];
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&sc.vec);
        gte_rtps();
        gte_stsxy(&sc.sxy);
        gte_stdp(&sc.dp);
        gte_stflg(&sc.flag);
        gte_stszotz(&sc.otz);
        line           = gGpuPrimCursor;
        sx             = sc.sxy;
        sy             = sc.sxy >> 16;
        gGpuPrimCursor = line + 1;
        x0             = sx - 5;
        y0             = sy - 5;
        x1             = sx + 5;
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        y1       = sy + 5;
        line->x0 = x0;
        line->y0 = y0;
        line->x1 = x1;
        line->y1 = y1;
        addPrim(gGpuCurrentOt + 10, line);

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        i++;
        line->x0 = x1;
        line->y0 = y0;
        line->x1 = x0;
        line->y1 = y1;
        addPrim(gGpuCurrentOt + 10, line);
    } while (i < 8);
}

/// Debug view of the two cubic Bezier segments whose control points start at
/// `D_dryfield_dilapidated_house_801866B4`: samples each at 21 positions,
/// projects every point through the parent task's
/// `_DryfieldDilapidatedHouseMorphWork::attachMtx` and `gGfxViewCoord.workm`,
/// and queues a small `LINE_F2` X at it in `gGpuCurrentOt[10]` - green for the first segment, blue for the second.
static void func_dryfield_dilapidated_house_8017EE58(Task* task)
{
    SVECTOR  vec;
    DVECTOR  sx;
    DVECTOR  sy;
    s32      out[3];
    s32      sxy;
    s32      dp;
    s32      flag;
    s32      otz;
    MATRIX*  mtx;
    LINE_F2* line;
    s32      i;

    mtx = &((_DryfieldDilapidatedHouseMorphWork*)((Task*)task->spawnArg2.pointer)->work)->attachMtx;
    for (i = 20; i >= 0; i--) {
        bezierCurveEvaluate(D_dryfield_dilapidated_house_801866B4, D_dryfield_dilapidated_house_801866B4 + 3, 20, i, out);
        vec.vx = out[0];
        vec.vy = out[1];
        vec.vz = out[2];
        gte_SetRotMatrix(mtx);
        gte_ldv0(&vec);
        gte_rtv0();
        gte_stsv(&vec);
        vec.vx = (u16)vec.vx + (u16)mtx->t[0];
        vec.vy = (u16)vec.vy + (u16)mtx->t[1];
        vec.vz = (u16)vec.vz + (u16)mtx->t[2];
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&vec);
        gte_rtps();
        gte_stsxy(&sxy);
        gte_stdp(&dp);
        gte_stflg(&flag);
        gte_stszotz(&otz);
        sx.vx = sxy;
        sy.vx = sxy >> 16;

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0xFF, 0);
        line->x0 = sx.vx - 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx + 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0xFF, 0);
        line->x0 = sx.vx + 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx - 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);
    }
    for (i = 20; i >= 0; i--) {
        bezierCurveEvaluate(D_dryfield_dilapidated_house_801866B4 + 3, D_dryfield_dilapidated_house_801866B4 + 6, 20, i, out);
        vec.vx = out[0];
        vec.vy = out[1];
        vec.vz = out[2];
        gte_SetRotMatrix(mtx);
        gte_ldv0(&vec);
        gte_rtv0();
        gte_stsv(&vec);
        vec.vx = (u16)vec.vx + (u16)mtx->t[0];
        vec.vy = (u16)vec.vy + (u16)mtx->t[1];
        vec.vz = (u16)vec.vz + (u16)mtx->t[2];
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&vec);
        gte_rtps();
        gte_stsxy(&sxy);
        gte_stdp(&dp);
        gte_stflg(&flag);
        gte_stszotz(&otz);
        sx.vx = sxy;
        sy.vx = sxy >> 16;

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0, 0xFF);
        line->x0 = sx.vx - 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx + 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0, 0xFF);
        line->x0 = sx.vx + 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx - 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);
    }
}

#include "../../shared/bezier_curve_evaluate.inc.c"

#define GLOW_DRAW_RING_BEAM_BRIGHTNESS(t) ((_DryfieldDilapidatedHouseMorphWork*)((Task*)(t)->spawnArg2.pointer)->work)->beamLevel
#define GLOW_DRAW_RING_BEAM_PHASE_STEP    0x40
#define GLOW_DRAW_RING_BEAM_OT_OFFSET     3
#define GLOW_DRAW_RING_BEAM_HALO_TPAGE    0xE1000465
#include "../../shared/glow_draw_ring_beam.inc.c"

/// Lays out 24 screen-space points in `verts` as four rings of six around two
/// ends of a segment. The ends come from `D_dryfield_dilapidated_house_80186794`,
/// mirrored in x when the task's spawn arg 1 is 1, rotated by the parent task's
/// `_DryfieldDilapidatedHouseMorphWork::attachMtx`. The far end sits a quarter
/// of the way out at a parent `beamLevel` of 0 and reaches full length at
/// `ONE`; both ends are then moved by the matrix translation and projected. Each ring's offsets are rotated to the segment's screen angle and
/// scaled by the projection distance over the last projected depth, which is
/// left in `*arg2` (`*arg3` gets the GTE flags). Rings 0 and 1 use the tables at
/// their natural size, rings 2 and 3 scaled by a factor that pulses with
/// `killCountdown`.
static void func_dryfield_dilapidated_house_8017FAD4(Task* task, SVECTOR* verts, s32* arg2, s32* arg3)
{
    SVECTOR                             a;
    SVECTOR                             b;
    GfxMatrix                           rot;
    s32                                 sxy0;
    s32                                 depthCue;
    s32                                 sxy1;
    _DryfieldDilapidatedHouseMorphWork* work;
    MATRIX*                             mtx;
    SVECTOR*                            src;
    s16                                 t;
    s16                                 r;
    s32                                 scale;
    GfxRotationWords*                   words;
    s32                                 i;
    u16                                 f;
    s16                                 x0;
    s32                                 y0;
    s16                                 x1;
    s32                                 y1;
    s32                                 dx;
    s32                                 dy;
    s32                                 side;

    side = task->spawnArg1.value;
    work = ((Task*)task->spawnArg2.pointer)->work;
    mtx  = &work->attachMtx;
    f    = work->beamLevel;
    a.vx = D_dryfield_dilapidated_house_80186794[0].vx;
    a.vy = D_dryfield_dilapidated_house_80186794[0].vy;
    a.vz = D_dryfield_dilapidated_house_80186794[0].vz;
    src  = &D_dryfield_dilapidated_house_80186794[1];
    b.vx = src->vx;
    b.vy = src->vy;
    b.vz = src->vz;
    if (side == 1) {
        a.vx *= -1;
        b.vx *= -1;
    }
    gte_SetRotMatrix(mtx);
    gte_ldv0(&a);
    gte_rtv0();
    gte_stsv(&a);
    gte_ldv0(&b);
    gte_rtv0();
    gte_stsv(&b);
    t     = (s16)f * 0.75 + 1024.0;
    b.vx  = a.vx + (b.vx - a.vx) * t / 4096;
    b.vy  = a.vy + (b.vy - a.vy) * t / 4096;
    b.vz  = a.vz + (b.vz - a.vz) * t / 4096;
    a.vx += mtx->t[0];
    a.vy += mtx->t[1];
    a.vz += mtx->t[2];
    b.vx += mtx->t[0];
    b.vy += mtx->t[1];
    b.vz += mtx->t[2];
    // Project both ends. Each screen point comes back packed, x in the low
    // half and y in the high; the depth-cue coefficient is not used.
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_RotTransPers(&a, &sxy0, &depthCue, arg3, arg2);
    gte_RotTransPers(&b, &sxy1, &depthCue, arg3, arg2);
    dy                       = (sxy0 >> 16) - (sxy1 >> 16);
    x1                       = sxy1;
    x0                       = sxy0;
    dx                       = x1 - x0;
    y0                       = sxy0 >> 16;
    y1                       = sxy1 >> 16;
    i                        = ratan2(dx, dy);
    scale                    = gDisplayState.screenDistance;
    rot.rotationWords.m00M01 = ONE;
    rot.rotationWords.m02M10 = 0;
    words                    = &rot.rotationWords;
    words->m11M12            = ONE;
    rot.rotationWords.m20M21 = 0;
    words->m22               = ONE;
    RotMatrixZ(i, &rot.mat);
    gte_SetRotMatrix(&rot.mat);
    for (i = 0; i < 6; i++) {
        a.vx = D_dryfield_dilapidated_house_801867A4[i].vx * scale / *arg2;
        a.vy = D_dryfield_dilapidated_house_801867A4[i].vy * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        verts[i].vx = b.vx + x0;
        verts[i].vy = b.vy + y0;
    }
    for (i = 0; i < 6; i++) {
        a.vx = D_dryfield_dilapidated_house_801867D4[i].vx * scale / *arg2;
        a.vy = D_dryfield_dilapidated_house_801867D4[i].vy * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        verts[i + 6].vx = b.vx + x1;
        verts[i + 6].vy = b.vy + y1;
    }
    r = 4096.0 - rsin(task->killCountdown) * 0.5 + 4096.0;
    for (i = 0; i < 6; i++) {
        a.vx = ((D_dryfield_dilapidated_house_801867A4[i].vx * r) >> 12) * scale / *arg2;
        a.vy = ((D_dryfield_dilapidated_house_801867A4[i].vy * r) >> 12) * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        verts[i + 12].vx = b.vx + x0;
        verts[i + 12].vy = b.vy + y0;
    }
    for (i = 0; i < 6; i++) {
        a.vx = ((D_dryfield_dilapidated_house_801867D4[i].vx * r) >> 12) * scale / *arg2;
        a.vy = ((D_dryfield_dilapidated_house_801867D4[i].vy * r) >> 12) * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        verts[i + 18].vx = b.vx + x1;
        verts[i + 18].vy = b.vy + y1;
    }
}

/// Projects the adjacent inner corners and current outer corner of a cone segment.
///
/// The GTE must hold the view transform. Store packed screen words and the
/// final outer corner's SZ3 / 4; buffers supply the rest of both 16-entry rings.
static inline void _dryfieldDilapidatedHouseProjectConeSegment(const SVECTOR* vertex, s32* screen, s32* depth)
{
    gte_ldv3(vertex, vertex + 1, vertex + DRYFIELD_DILAPIDATED_HOUSE_CONE_RING_VERTEX_COUNT);
    gte_rtpt();
    gte_stsxy3(screen, screen + 1, screen + DRYFIELD_DILAPIDATED_HOUSE_CONE_RING_VERTEX_COUNT);
    gte_stszotz(depth);
}

/// Draws the morph attachment's yellow-to-black cone with quarter-additive quads.
///
/// `ringVertices` supplies 32 world-space vertices: 16 inner, then 16 outer.
/// The borrowed parent task supplies `coneLevel`; values up to 0x400 map to
/// byte intensity, with larger values clamped. Each segment sorts four OT
/// entries beyond its outer vertex's SZ3 / 4, without projection rejection.
static void _dryfieldDilapidatedHouseDrawMorphCone(Task* task, const SVECTOR* ringVertices)
{
    enum {
        CONE_RING_VERTEX_COUNT = DRYFIELD_DILAPIDATED_HOUSE_CONE_RING_VERTEX_COUNT,
        CONE_BRIGHTNESS_ONE    = 0x400,
        CONE_MAX_INTENSITY     = 0xFF,
        CONE_OT_OFFSET         = 4,
        CONE_DRAW_MODE_COMMAND = 0xE1000400,
    };

    s32                                 screenPositions[2 * CONE_RING_VERTEX_COUNT];
    s32                                 depths[CONE_RING_VERTEX_COUNT];
    CVECTOR                             innerColor;
    CVECTOR                             outerColor;
    POLY_G4*                            quad;
    DR_TPAGE*                           drawMode;
    s16                                 brightness;
    s32                                 segmentIndex;
    s32*                                screenWord;
    const SVECTOR*                      vertexCursor;
    s32*                                screenCursor;
    s32*                                depthCursor;
    Task*                               parentTask;
    _DryfieldDilapidatedHouseMorphWork* parentWork;

    vertexCursor = ringVertices;
    screenCursor = screenPositions;
    depthCursor  = depths;
    parentTask   = task->spawnArg2.pointer;
    parentWork   = parentTask->work;
    brightness   = parentWork->coneLevel;
    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);
    // Share projected ring corners between adjacent segments.
    for (segmentIndex = 0; segmentIndex < CONE_RING_VERTEX_COUNT; segmentIndex++) {
        _dryfieldDilapidatedHouseProjectConeSegment(vertexCursor, screenCursor, depthCursor);
        screenCursor++;
        depthCursor++;
        vertexCursor++;
    }
    if (brightness > CONE_BRIGHTNESS_ONE) {
        brightness = CONE_BRIGHTNESS_ONE;
    }
    innerColor.r = brightness * CONE_MAX_INTENSITY / CONE_BRIGHTNESS_ONE;
    innerColor.g = brightness * CONE_MAX_INTENSITY / CONE_BRIGHTNESS_ONE;
    innerColor.b = 0;
    outerColor.r = 0;
    outerColor.g = 0;
    outerColor.b = 0;
    for (segmentIndex = 0; segmentIndex < CONE_RING_VERTEX_COUNT; segmentIndex++) {
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyG4(quad);
        setSemiTrans(quad, 1);
        quad->r0 = innerColor.r;
        quad->g0 = innerColor.g;
        quad->b0 = innerColor.b;
        quad->r1 = innerColor.r;
        quad->g1 = innerColor.g;
        quad->b1 = innerColor.b;
        quad->r2 = outerColor.r;
        quad->g2 = outerColor.g;
        quad->b2 = outerColor.b;
        quad->r3 = outerColor.r;
        quad->g3 = outerColor.g;
        quad->b3 = outerColor.b;
        // Packed screen words, one per vertex; each `screenWord` word is two words past
        // the previous one because a colour word sits between them.
        screenWord = (s32*)&quad->x0;
        if (segmentIndex < CONE_RING_VERTEX_COUNT - 1) {
            screenWord[0] = screenPositions[segmentIndex];
            screenWord[2] = screenPositions[segmentIndex + 1];
            screenWord[4] = screenPositions[segmentIndex + CONE_RING_VERTEX_COUNT];
            screenWord[6] = screenPositions[segmentIndex + CONE_RING_VERTEX_COUNT + 1];
        } else {
            screenWord[0] = screenPositions[CONE_RING_VERTEX_COUNT - 1];
            screenWord[2] = screenPositions[0];
            screenWord[4] = screenPositions[2 * CONE_RING_VERTEX_COUNT - 1];
            screenWord[6] = screenPositions[CONE_RING_VERTEX_COUNT];
        }
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depths[segmentIndex] << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + CONE_OT_OFFSET, quad);
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setlen(drawMode, 1);
        drawMode->code[0] = CONE_DRAW_MODE_COMMAND | getTPage(0, GPU_BLEND_ADD_QUARTER, 320, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depths[segmentIndex] << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + CONE_OT_OFFSET, drawMode);
    }
}

static void func_dryfield_dilapidated_house_80180738(Task* task, SVECTOR* verts)
{
    _DryfieldDilapidatedHouseConeWork*  work;
    _DryfieldDilapidatedHouseMorphWork* src;
    MATRIX*                             mtx;
    SVECTOR*                            ofs;
    SVECTOR*                            ofs2;
    SVECTOR                             pos[2];
    SVECTOR*                            v0;
    SVECTOR*                            v1;
    s16                                 tx;
    s16                                 ty;
    s16                                 tz;
    s32                                 i;
    s32                                 ang;
    s32                                 c;
    s32                                 s;

    v0 = verts;
    v1 = &verts[16];

    work = task->work;
    src  = ((Task*)task->spawnArg2.pointer)->work;

    ofs       = D_dryfield_dilapidated_house_80186844;
    ofs2      = D_dryfield_dilapidated_house_80186844 + 1;
    pos[0].vx = ofs->vx;
    pos[0].vy = ofs->vy;
    pos[0].vz = ofs->vz;
    pos[1].vx = ofs2->vx;
    pos[1].vy = ofs2->vy;
    pos[1].vz = ofs2->vz;

    mtx = &src->attachMtx;

    tx = mtx->t[0];
    ty = mtx->t[1];
    tz = mtx->t[2];

    gte_SetRotMatrix(mtx);

    for (i = 0; i < 16; i++) {
        ang = (i << 12) >> 4;
        c   = rcos(ang);
        s   = rsin(ang);

        v0->vx = pos[0].vx + ((c * 0x96) >> 12);
        v0->vy = pos[0].vy + ((s * 0x4B) >> 12);
        v0->vz = pos[0].vz;

        gte_ldv0(v0);
        gte_rtv0();
        gte_stsv(v0);

        v0->vx += tx;
        v0->vy += ty;
        v0->vz += tz;
        v0++;

        v1->vx = pos[1].vx + ((c * 0xFA) >> 12);
        v1->vy = pos[1].vy + ((s * 0x7D) >> 12);
        // The outer vertex rides a sine of its own phase along the cone's
        // axis; the phase then moves on by this vertex's rate.
        v1->vz = pos[1].vz + ((rsin(work->rimPhase[i] >> 2) * 0x64) >> 12);

        work->rimPhase[i] = (work->rimPhase[i] + D_dryfield_dilapidated_house_80186804[i]) &
                            (DRYFIELD_DILAPIDATED_HOUSE_CONE_RIM_PHASE_PERIOD - 1);

        gte_ldv0(v1);
        gte_rtv0();
        gte_stsv(v1);

        v1->vx += tx;
        v1->vy += ty;
        v1->vz += tz;
        v1++;
    }
}

#include "../../shared/model_morph_blend.inc.c"

static void func_dryfield_dilapidated_house_80180B84(Task* task)
{
    Task*                               parent;
    TmdObject*                          obj;
    TmdObject*                          parentObj;
    GfxCoord*                           coord;
    GfxCoord*                           parentCoord;
    _DryfieldDilapidatedHouseMorphWork* work;
    ModelMorph*                         morph;
    TmdSource*                          source;
    SVECTOR*                            dst;
    SVECTOR*                            dst2;
    SVECTOR*                            src2;
    SVECTOR*                            verts;
    TaskDesc*                           table;
    Task*                               spawned;
    GfxCoord*                           childCoord;
    u16                                 flags;
    s32                                 i;

    parent      = (Task*)task->spawnArg2.pointer;
    obj         = task->extra.tmd;
    parentObj   = parent->extra.tmd;
    coord       = obj->coords;
    parentCoord = parentObj->coords;
    work        = memMalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work       = work;
    work->morphLevel = 0;
    flags            = obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    obj->flags       = flags;
    if (!(parentObj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        obj->flags = flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    obj->otOffset       = 4;
    obj->flags         |= TMD_OBJECT_SEMI_TRANS;
    parentCoord        += task->spawnArg1.value;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = parentCoord;
    obj->lightMtx       = parentObj->lightMtx;
    obj->colorMtx       = parentObj->colorMtx;
    taskReparent(parent, task);

    // Snapshot the model's rest shape into the morph record.
    morph  = &D_dryfield_dilapidated_house_8018669C;
    source = task->extra.tmd->source;
    dst    = morph->savedVertices;
    dst2   = morph->savedNormals;
    verts  = source->verts;
    for (i = 0; i < morph->savedVertexCount; i++) {
        dst[i].vx = verts[i].vx;
        dst[i].vy = verts[i].vy;
        dst[i].vz = verts[i].vz;
    }
    if (morph->targetNormals != NULL) {
        src2 = source->normals;
        for (i = 0; i < morph->normalCount; i++) {
            dst2[i].vx = src2[i].vx;
            dst2[i].vy = src2[i].vy;
            dst2[i].vz = src2[i].vz;
        }
    }

    func_dryfield_dilapidated_house_80180FD8(task);

    table   = D_dryfield_dilapidated_house_80186854;
    spawned = taskSpawnFromTable(table, 3, 9, task);
    if (spawned != NULL) {
        childCoord        = spawned->extra.tmd->coords;
        childCoord->coord = work->attachMtx;
    }
    spawned = taskSpawnFromTable(table, 3, 0x11, task);
    if (spawned != NULL) {
        childCoord        = spawned->extra.tmd->coords;
        childCoord->coord = work->attachMtx;
    }
    spawned = taskSpawnFromTable(table, 2, 0, task);
    if (spawned != NULL) {
        childCoord        = spawned->extra.tmd->coords;
        childCoord->coord = work->attachMtx;
    }
    spawned = taskSpawnFromTable(table, 2, 1, task);
    if (spawned != NULL) {
        childCoord        = spawned->extra.tmd->coords;
        childCoord->coord = work->attachMtx;
    }

    task->exitCallback = _dryfieldDilapidatedHouseMorphExit;
    task->state       += 1;
}

/// Runs the task's current state out of `D_dryfield_dilapidated_house_8017D640`,
/// copied onto the stack: `func_dryfield_dilapidated_house_80180B84`,
/// `func_dryfield_dilapidated_house_80180F5C`, then `taskKill`.
void func_dryfield_dilapidated_house_80180F04(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D640;
    sp.funcs[task->state](task);
}

static void func_dryfield_dilapidated_house_80180F5C(Task* arg0)
{
    _DryfieldDilapidatedHouseMorphWork* work;
    s32                                 temp_v0;

    work = arg0->work;
    _dryfieldDilapidatedHouseCopyModelVisibility(arg0->extra.tmd,
                                                 ((Task*)arg0->spawnArg2.pointer)->extra.tmd);
    _dryfieldDilapidatedHouseUpdateAttachmentTransform(arg0);
    temp_v0          = func_dryfield_dilapidated_house_80180FD8(arg0);
    work->morphLevel = temp_v0;
    work->coneLevel  = temp_v0;
    work->beamLevel  = temp_v0;
}

/// Releases the morph attachment task through ordinary task teardown.
static void _dryfieldDilapidatedHouseMorphExit(Task* task)
{
    taskKill(task);
}

/// Steps the task's 0..0x1000 ramp by 0x44, saturating at 0x1000, and feeds the
/// distance still to run (`0x1000 - ramp`) to the room record's matrix/vertex
/// interpolator. Returns the ramp value, which the caller stores into each level
/// of its `_DryfieldDilapidatedHouseMorphWork`.
static s32 func_dryfield_dilapidated_house_80180FD8(Task* task)
{
    s32 ramp;

    ramp = task->killCountdown + 0x44;
    if (ramp >= 0x1001) {
        ramp = 0x1000;
    }
    task->killCountdown = ramp;
    modelMorphBlend(task, &D_dryfield_dilapidated_house_8018669C, 0x1000 - ramp);
    return ramp;
}

/// Rebuilds the transform of the parent model part carrying the morph attachment.
///
/// The attachment's coordinate parent must be in its spawning model's contiguous
/// part array. Compose local transforms from the first part through that part,
/// inclusively, into the work block's `attachMtx` (12 fractional rotation bits
/// and whole-coordinate translation). This walk follows array order, not links.
static void _dryfieldDilapidatedHouseUpdateAttachmentTransform(Task* task)
{
    VECTOR                              rotatedTranslation;
    GfxCoord*                           modelCoord;
    _DryfieldDilapidatedHouseMorphWork* work;
    GfxCoord*                           parentCoord;
    MATRIX*                             attachmentMtx;
    Task*                               parentTask;

    modelCoord                          = task->extra.tmd->coords;
    work                                = task->work;
    parentTask                          = task->spawnArg2.pointer;
    parentCoord                         = parentTask->extra.tmd->coords;
    attachmentMtx                       = &work->attachMtx;
    MATRIX_PAIR(&work->attachMtx, 0, 0) = ONE;
    MATRIX_PAIR(attachmentMtx, 0, 2)    = 0;
    MATRIX_PAIR(attachmentMtx, 1, 1)    = ONE;
    MATRIX_PAIR(attachmentMtx, 2, 0)    = 0;
    attachmentMtx->m[2][2]              = ONE;
    attachmentMtx->t[0]                 = 0;
    attachmentMtx->t[1]                 = 0;
    attachmentMtx->t[2]                 = 0;
    do {
        // The SDK vector call reads the three translation words, not a fourth word.
        ApplyMatrixLV(attachmentMtx, (VECTOR*)parentCoord->coord.t, &rotatedTranslation);
        attachmentMtx->t[0] += rotatedTranslation.vx;
        attachmentMtx->t[1] += rotatedTranslation.vy;
        attachmentMtx->t[2] += rotatedTranslation.vz;
        MulMatrix0(attachmentMtx, &parentCoord->coord, attachmentMtx);
    } while (parentCoord++ != modelCoord->parent);
}

/// Copies the parent's active-draw exclusion bit while preserving other model flags.
static void _dryfieldDilapidatedHouseCopyModelVisibility(TmdObject* model, const TmdObject* parentModel)
{
    if (!(parentModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

/// Runs the task's current state out of `D_dryfield_dilapidated_house_8017D61C`,
/// copied onto the stack: `_dryfieldDilapidatedHouseCurveDebugInit`,
/// `func_dryfield_dilapidated_house_80181264`, then `taskKill`.
void func_dryfield_dilapidated_house_80181134(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D61C;
    sp.funcs[task->state](task);
}

/// Initializes the hidden model task that marks the attachment's Bezier curve.
///
/// Snapshots its initial local matrix and seeds an otherwise unread word to
/// `ONE`; the word's purpose is unproven. The task owns the allocation and
/// becomes a child of the borrowed spawning task. Allocation failure kills it.
static void _dryfieldDilapidatedHouseCurveDebugInit(Task* task)
{
    TmdObject*                               model;
    GfxCoord*                                coord;
    _DryfieldDilapidatedHouseCurveDebugWork* work;

    model = task->extra.tmd;
    coord = model->coords;
    work  = memMalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work       = work;
    work->field_20   = ONE;
    work->initialMtx = coord->coord;
    model->flags    |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    taskReparent(task->spawnArg2.pointer, task);
    task->state += 1;
}

static void func_dryfield_dilapidated_house_80181264(Task* arg0)
{
    func_dryfield_dilapidated_house_8017EBB8(arg0);
    func_dryfield_dilapidated_house_8017EE58(arg0);
}

#include "../../shared/bezier_curve_coefficients.inc.c"

/// Runs the task's current state out of `D_dryfield_dilapidated_house_8017D628`,
/// copied onto the stack.
void func_dryfield_dilapidated_house_801812E8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D628;
    sp.funcs[task->state](task);
}

/// Attaches a ring-beam coordinate task beneath the spawning morph model.
///
/// Retains a four-byte primary-heap work allocation whose contents are never
/// accessed. Teardown owns that allocation. Failure kills the task; success
/// installs coordinate detachment as its exit callback and enters the draw state.
static void _dryfieldDilapidatedHouseRingBeamInit(Task* task)
{
    enum { RING_BEAM_UNUSED_WORK_BYTES = 4 };

    GfxCoord* coord;
    void*     unusedWork;
    Task*     parentTask;

    coord      = task->extra.coordBody->coord;
    unusedWork = memMalloc(RING_BEAM_UNUSED_WORK_BYTES, false);
    if (unusedWork == NULL) {
        taskKill(task);
        return;
    }
    task->work    = unusedWork;
    parentTask    = task->spawnArg2.pointer;
    coord->parent = parentTask->extra.tmd->coords;
    taskReparent(task->spawnArg2.pointer, task);
    task->exitCallback = _dryfieldDilapidatedHouseRingBeamExit;
    task->state       += 1;
}

static void func_dryfield_dilapidated_house_801813DC(Task* task)
{
    SVECTOR verts[24];
    s32     sp0;
    s32     sp1;

    func_dryfield_dilapidated_house_8017FAD4(task, verts, &sp0, &sp1);
    glowDrawRingBeam(task, verts, sp0);
    glowDrawRingBeam(task, verts, sp0);
}

/// Detaches the ring beam from its parent model before releasing the task.
static void _dryfieldDilapidatedHouseRingBeamExit(Task* task)
{
    GfxCoord* coord;

    coord         = task->extra.coordBody->coord;
    coord->parent = &gGfxViewCoord;
    taskKill(task);
}

/// Runs the task's current state out of `D_dryfield_dilapidated_house_8017D634`,
/// copied onto the stack.
void func_dryfield_dilapidated_house_8018145C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D634;
    sp.funcs[task->state](task);
}

/// State 0 of the handler table at `D_dryfield_dilapidated_house_8017D634`,
/// dispatched by `func_dryfield_dilapidated_house_8018145C`: allocates the
/// cone's `_DryfieldDilapidatedHouseConeWork` and starts each rim phase at its
/// per-vertex rate times this task's spawn argument, wrapped into one turn,
/// links the model coordinate this task works on to the parent model's
/// coordinate array, and re-parents the task under the task that spawned it.
static void func_dryfield_dilapidated_house_801814B4(Task* arg0)
{
    _DryfieldDilapidatedHouseConeWork* work;
    GfxCoord*                          coord;
    s32                                i;

    coord = arg0->extra.tmd->coords;
    work  = memMalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = work;
    for (i = 0; i < ARRAY_SIZE(work->rimPhase); i++) {
        work->rimPhase[i] = (D_dryfield_dilapidated_house_80186804[i] * arg0->spawnArg1.value) &
                            (DRYFIELD_DILAPIDATED_HOUSE_CONE_RIM_PHASE_PERIOD - 1);
    }
    coord->parent = ((Task*)arg0->spawnArg2.pointer)->extra.tmd->coords;
    taskReparent(arg0->spawnArg2.pointer, arg0);
    arg0->state += 1;
}

static void func_dryfield_dilapidated_house_80181584(Task* task)
{
    SVECTOR verts[32];

    func_dryfield_dilapidated_house_80180738(task, verts);
    _dryfieldDilapidatedHouseDrawMorphCone(task, verts);
}

/// Detaches the morph cone's coordinate before releasing the task and rim phases.
static void _dryfieldDilapidatedHouseMorphConeExit(Task* task)
{
    GfxCoord* coord;

    coord         = task->extra.coordBody->coord;
    coord->parent = &gGfxViewCoord;
    taskKill(task);
}

/// Draws one pulsing additive light prism, with four sides and a lit end cap.
///
/// `firstVertex` is 0, 8 or 16 in the room's corner table. Its first four
/// vertices are lit; its last four are black. `coord->workm` must be composed.
/// Corners narrow to 16-bit world coordinates before view projection; there is
/// no near-plane or GTE-flag rejection. Scratch storage is released on return.
static void _dryfieldDilapidatedHouseDrawLightPrism(GfxCoord* coord, s16 firstVertex)
{
    /// Transforms a local prism corner into the scratch quad's world coordinates.
    ///
    /// Captures `coord` (composed GfxCoord*) and `scratch` (EffectQuadCornersScratch*).
    /// GTE rotation must already be `coord->workm`. Arguments have no side effects:
    /// worldIndex is 0..3 and evaluated repeatedly; vertexIndex is 0..23 and
    /// evaluated once. Each axis narrows to s16. Expands several statements, so
    /// use only as a standalone sequence here, never as an unbraced branch body.
#define DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(vertexIndex, worldIndex)                         \
    gte_ldv0(&D_dryfield_dilapidated_house_80186884[(vertexIndex)]);                                       \
    gte_rtv0();                                                                                            \
    gte_stsv(&scratch->vertices[(worldIndex)]);                                                            \
    scratch->vertices[(worldIndex)].vx = (u16)scratch->vertices[(worldIndex)].vx + (u16)coord->workm.t[0]; \
    scratch->vertices[(worldIndex)].vy = (u16)scratch->vertices[(worldIndex)].vy + (u16)coord->workm.t[1]; \
    scratch->vertices[(worldIndex)].vz = (u16)scratch->vertices[(worldIndex)].vz + (u16)coord->workm.t[2]

    enum {
        PRISM_PULSE_ANGLE_SHIFT     = 10,
        PRISM_PULSE_INTENSITY_SHIFT = 11,
        PRISM_BASE_INTENSITY        = 20,
    };

    EffectQuadCornersScratch* scratch;
    POLY_G4*                  quad;
    s32                       cornerIndex;
    s32                       nextCorner;
    s32                       farCorner;
    s32                       farNextCorner;
    u8                        intensity;

    SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    scratch = SCRATCH_STACK_CURSOR(EffectQuadCornersScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    intensity = (rsin(gDisplayState.animFrame << PRISM_PULSE_ANGLE_SHIFT) >> PRISM_PULSE_INTENSITY_SHIFT) + PRISM_BASE_INTENSITY;
    // Each side joins adjacent lit corners to the corresponding dark corners.
    for (cornerIndex = 0; cornerIndex < DRYFIELD_DILAPIDATED_HOUSE_PRISM_RING_CORNERS; cornerIndex++) {
        gte_SetRotMatrix(&coord->workm);
        DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + cornerIndex, 0);
        gte_SetRotMatrix(&coord->workm);
        nextCorner = (cornerIndex + 1) & (DRYFIELD_DILAPIDATED_HOUSE_PRISM_RING_CORNERS - 1);
        DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + nextCorner, 1);
        gte_SetRotMatrix(&coord->workm);
        farCorner = cornerIndex + DRYFIELD_DILAPIDATED_HOUSE_PRISM_RING_CORNERS;
        DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + farCorner, 2);
        gte_SetRotMatrix(&coord->workm);
        farNextCorner = nextCorner + DRYFIELD_DILAPIDATED_HOUSE_PRISM_RING_CORNERS;
        DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + farNextCorner, 3);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&scratch->vertices[0]);
        gte_rtps();
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyG4(quad);
        gte_stsxy(&quad->x0);
        gte_ldv3(&scratch->vertices[1], &scratch->vertices[2], &scratch->vertices[3]);
        gte_rtpt();
        gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
        gte_stszotz(&scratch->depth);
        setRGB0(quad, intensity, intensity, intensity);
        setRGB1(quad, intensity, intensity, intensity);
        setRGB2(quad, 0, 0, 0);
        setRGB3(quad, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->depth);
    }
    // Close the lit end in the GPU quad's strip order.
    gte_SetRotMatrix(&coord->workm);
    DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex, 0);
    gte_SetRotMatrix(&coord->workm);
    DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + 1, 1);
    gte_SetRotMatrix(&coord->workm);
    DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + 3, 2);
    gte_SetRotMatrix(&coord->workm);
    DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER(firstVertex + 2, 3);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->vertices[0]);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyG4(quad);
    gte_stsxy(&quad->x0);
    gte_ldv3(&scratch->vertices[1], &scratch->vertices[2], &scratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stszotz(&scratch->depth);
    setRGB0(quad, intensity, intensity, intensity);
    setRGB1(quad, intensity, intensity, intensity);
    setRGB2(quad, intensity, intensity, intensity);
    setRGB3(quad, intensity, intensity, intensity);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->depth);
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);
#undef DRYFIELD_DILAPIDATED_HOUSE_TRANSFORM_PRISM_CORNER
}

/// Snapshots a composed endpoint relative to the persistent view coordinate.
///
/// Both transforms must be current and the view rotation orthonormal. The
/// history frame remains caller-owned; its composition stamp is left untouched.
static inline void _dryfieldDilapidatedHouseStoreTrailFrame(GfxCoord* historyFrame, const GfxCoord* endpoint)
{
    historyFrame->parent = &gGfxViewCoord;
    historyFrame->workm  = endpoint->workm;
    gte_SetRotMatrix(&endpoint->workm);
    gte_SetTransMatrix(&endpoint->workm);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &historyFrame->workm, &historyFrame->coord);
}

void dryfieldDilapidatedHouseTwinTrailTask(Task* task)
{
    enum {
        TRAIL_INITIALIZE,
        TRAIL_RECORD,
        TRAIL_SLOT_MASK         = ARRAY_SIZE(D_dryfield_dilapidated_house_80189DE0) - 1,
        TRAIL_COLOR_MULTIPLIERS = (2 << 8) | (1 << 4),
    };

    GfxCoord    secondEndpointCoord; // Only its translation is initialized and consumed by the ribbon
    GfxCoord*   firstEndpointCoord;
    GfxCoord*   historyFrame;
    EffectWork* work;
    SVECTOR*    initialOffset;
    s32         slotIndex;

    work               = task->spawnArg2.pointer;
    firstEndpointCoord = task->extra.coordBody->coord;

    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age++;
        switch (task->state) {
            case TRAIL_INITIALIZE:
                // Seed both histories so the first ribbon starts collapsed.
                firstEndpointCoord->parent       = work->parent;
                firstEndpointCoord->coord.t[0]   = D_dryfield_dilapidated_house_80186944[0].vx;
                firstEndpointCoord->coord.t[1]   = D_dryfield_dilapidated_house_80186944[0].vy;
                firstEndpointCoord->coord.t[2]   = D_dryfield_dilapidated_house_80186944[0].vz;
                firstEndpointCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(firstEndpointCoord);
                task->state                      = TRAIL_RECORD;
                secondEndpointCoord.parent       = work->parent;
                initialOffset                    = &D_dryfield_dilapidated_house_80186944[1];
                secondEndpointCoord.coord.t[0]   = initialOffset->vx;
                secondEndpointCoord.coord.t[1]   = initialOffset->vy;
                secondEndpointCoord.coord.t[2]   = initialOffset->vz;
                secondEndpointCoord.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&secondEndpointCoord);
                for (slotIndex = 0; slotIndex < ARRAY_SIZE(D_dryfield_dilapidated_house_80189DE0); slotIndex++) {
                    historyFrame = &D_dryfield_dilapidated_house_80189DE0[slotIndex];
                    _dryfieldDilapidatedHouseStoreTrailFrame(historyFrame, firstEndpointCoord);
                    historyFrame = &D_dryfield_dilapidated_house_8018A060[slotIndex];
                    _dryfieldDilapidatedHouseStoreTrailFrame(historyFrame, &secondEndpointCoord);
                }
                return;

            case TRAIL_RECORD:
                // Snapshots are rebased under the view, independent of later sword motion.
                firstEndpointCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(firstEndpointCoord);
                secondEndpointCoord.parent = work->parent;
                {
                    SVECTOR* updateOffset          = &D_dryfield_dilapidated_house_80186944[1];
                    secondEndpointCoord.coord.t[0] = updateOffset->vx;
                    secondEndpointCoord.coord.t[1] = updateOffset->vy;
                    secondEndpointCoord.coord.t[2] = updateOffset->vz;
                }
                secondEndpointCoord.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&secondEndpointCoord);
                historyFrame = &D_dryfield_dilapidated_house_80189DE0[work->age & TRAIL_SLOT_MASK];
                _dryfieldDilapidatedHouseStoreTrailFrame(historyFrame, firstEndpointCoord);
                historyFrame = &D_dryfield_dilapidated_house_8018A060[work->age & TRAIL_SLOT_MASK];
                _dryfieldDilapidatedHouseStoreTrailFrame(historyFrame, &secondEndpointCoord);
                for (slotIndex = 0; slotIndex < ARRAY_SIZE(D_dryfield_dilapidated_house_80189DE0); slotIndex++) {
                    historyFrame               = &D_dryfield_dilapidated_house_80189DE0[slotIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                    historyFrame               = &D_dryfield_dilapidated_house_8018A060[slotIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                }
                _dryfieldDilapidatedHouseDrawTwinTrail(work->age & TRAIL_SLOT_MASK, TRAIL_COLOR_MULTIPLIERS);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    effectKillTask(work, task);
                }
                break;
        }
    }
}

/// Draws an additive ribbon joining the two eight-frame endpoint histories.
///
/// `newestSlot` is 0..7. Seven quads walk backwards around the histories;
/// their translations must already be in view space. `colorMultipliers` stores
/// red in the signed high byte and two-bit green/blue factors at bits 4/0;
/// the caller passes 2:1:0. Intensity falls by nine per edge from 64. Quads
/// with final-corner SZ3 / 4 below 17 are skipped after reserving their packet.
static void _dryfieldDilapidatedHouseDrawTwinTrail(s16 newestSlot, s16 colorMultipliers)
{
    enum {
        TRAIL_SLOT_MASK          = ARRAY_SIZE(D_dryfield_dilapidated_house_80189DE0) - 1,
        TRAIL_INITIAL_INTENSITY  = 64,
        TRAIL_INTENSITY_STEP     = 9,
        TRAIL_MIN_DEPTH          = 17,
        TRAIL_COLOR_CHANNEL_MASK = 3,
    };

    OverlayFlaggedQuadScratch* scratch;
    GfxCoord*                  firstEndpointFrame;
    GfxCoord*                  secondEndpointFrame;
    POLY_G4*                   quad;
    s32                        segmentIndex;
    s32                        frameIndex;
    s32                        newerSlot;
    s32                        olderSlot;
    s32                        newerIntensity;
    s32                        olderIntensity;
    s32                        segmentIntensity;

    SCRATCH_STACK_RESERVE_BLOCK(OverlayFlaggedQuadScratch);
    scratch = SCRATCH_STACK_CURSOR(OverlayFlaggedQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    // Newer endpoint pairs form the bright edge; older pairs form the dim edge.
    for (segmentIndex = 0; segmentIndex < ARRAY_SIZE(D_dryfield_dilapidated_house_80189DE0) - 1; segmentIndex++) {
        frameIndex             = newestSlot - segmentIndex;
        newerSlot              = frameIndex & TRAIL_SLOT_MASK;
        olderSlot              = (frameIndex - 1) & TRAIL_SLOT_MASK;
        firstEndpointFrame     = &D_dryfield_dilapidated_house_80189DE0[newerSlot];
        scratch->corners[0].vx = (u16)firstEndpointFrame->workm.t[0];
        scratch->corners[0].vy = (u16)firstEndpointFrame->workm.t[1];
        secondEndpointFrame    = &D_dryfield_dilapidated_house_8018A060[newerSlot];
        scratch->corners[0].vz = (u16)firstEndpointFrame->workm.t[2];
        scratch->corners[1].vx = (u16)secondEndpointFrame->workm.t[0];
        scratch->corners[1].vy = (u16)secondEndpointFrame->workm.t[1];
        firstEndpointFrame     = &D_dryfield_dilapidated_house_80189DE0[olderSlot];
        scratch->corners[1].vz = (u16)secondEndpointFrame->workm.t[2];
        scratch->corners[2].vx = (u16)firstEndpointFrame->workm.t[0];
        scratch->corners[2].vy = (u16)firstEndpointFrame->workm.t[1];
        secondEndpointFrame    = &D_dryfield_dilapidated_house_8018A060[olderSlot];
        scratch->corners[2].vz = (u16)firstEndpointFrame->workm.t[2];
        scratch->corners[3].vx = (u16)secondEndpointFrame->workm.t[0];
        scratch->corners[3].vy = (u16)secondEndpointFrame->workm.t[1];
        scratch->corners[3].vz = (u16)secondEndpointFrame->workm.t[2];
        gte_ldv0(&scratch->corners[0]);
        gte_rtps();
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyG4(quad);
        gte_stsxy(&quad->x0);
        gte_ldv3(&scratch->corners[1], &scratch->corners[2], &scratch->corners[3]);
        gte_rtpt();
        gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
        gte_stszotz(&scratch->otz);
        if (scratch->otz >= TRAIL_MIN_DEPTH) {
            segmentIntensity = TRAIL_INITIAL_INTENSITY - segmentIndex * TRAIL_INTENSITY_STEP;
            newerIntensity   = segmentIntensity & 0xFF;
            olderIntensity   = (segmentIntensity - TRAIL_INTENSITY_STEP) & 0xFF;
            setRGB0(quad, newerIntensity * (colorMultipliers >> 8), newerIntensity * ((colorMultipliers >> 4) & TRAIL_COLOR_CHANNEL_MASK), newerIntensity * (colorMultipliers & TRAIL_COLOR_CHANNEL_MASK));
            setRGB1(quad, newerIntensity * (colorMultipliers >> 8), newerIntensity * ((colorMultipliers >> 4) & TRAIL_COLOR_CHANNEL_MASK), newerIntensity * (colorMultipliers & TRAIL_COLOR_CHANNEL_MASK));
            setRGB2(quad, olderIntensity * (colorMultipliers >> 8), olderIntensity * ((colorMultipliers >> 4) & TRAIL_COLOR_CHANNEL_MASK), olderIntensity * (colorMultipliers & TRAIL_COLOR_CHANNEL_MASK));
            setRGB3(quad, olderIntensity * (colorMultipliers >> 8), olderIntensity * ((colorMultipliers >> 4) & TRAIL_COLOR_CHANNEL_MASK), olderIntensity * (colorMultipliers & TRAIL_COLOR_CHANNEL_MASK));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayFlaggedQuadScratch);
}

/// Per-frame state machine of the room's fire blast effect: state 0 seeds its
/// `EffectWork` (0xC0 / 0x500 in `scale` and `angle`, a 12-bit `gRandomLcgState`
/// draw in `period` as the flicker sprite's roll, a `Gp_SpawnEff` and a
/// fade quad), maps the task's own coordinate onto
/// `gWorldCoordTransientPointLights[0]` and spawns the ring of `0x60275` flame effects, then
/// re-parents each onto this task. State 1 steps the angle by 0x40 per frame
/// and runs two more draws against the same coordinate. While
/// `gRoomEffectState->effectControl` is not running the frame counter is rolled back and the work
/// block is released at cancellation or once the angle passes
/// 0x580.
void func_dryfield_dilapidated_house_80182744(Task* task)
{
    EffectWork*                    work;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          pointLight;
    EffectWork*                    eff;
    s16                            tick;
    s16                            tick1;
    s16                            size;
    s32                            i;
    u8                             rgb[3];

    work       = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    tick       = work->age;
    tick1      = tick + 1;
    work->age  = tick1;
    lightSlot  = &gWorldCoordTransientPointLights[0];
    pointLight = &lightSlot->light;

    switch (task->state) {
        case 0:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age = tick;
                if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                    effectKillTask(work, task);
                }
                return;
            }
            work->scale     = 0xC0;
            work->angle     = 0x500;
            work->index     = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = (gRandomLcgState >> 16) & 0xFFF;
            Gp_SpawnEff(EFFECT_DRYFIELD_DILAPIDATED_HOUSE_FLAME_CONE, coord, 0, NULL);
            rgb[0] = 0xFF;
            rgb[1] = 0x7F;
            rgb[2] = 0x3F;
            effectDrawScreenTint(rgb, GPU_BLEND_ADD);
            gWorldCoordTransientPointLights[0].framesLeft      = 4;
            pointLight->inner                                  = 0x200;
            pointLight->outer                                  = 0x2000;
            gRandomLcgState                                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            size                                               = ((gRandomLcgState >> 16) & 0x700) + 0x800;
            pointLight->head.color.r                           = size;
            pointLight->head.color.g                           = size >> 1;
            pointLight->head.color.b                           = size >> 2;
            pointLight->head.transform.coord.coord.t[0]        = coord->coord.t[0];
            pointLight->head.transform.coord.coord.t[1]        = coord->coord.t[1];
            pointLight->head.transform.coord.coord.t[2]        = coord->coord.t[2];
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            i                                                  = 0;
            spriteQuadDrawFlicker(coord, work->age, work->angle, work->period);
            glowDrawFlameDisc(coord, work->angle, work->scale >> 1);
            work->angle = 0x380;
            do {
                eff = Gp_SpawnEff(EFFECT_DILAPIDATED_HOUSE_FLAME_RING, coord, i, NULL);
                if (eff != NULL) {
                    taskReparent(task, eff->task);
                }
                i += 0x2AA;
            } while (i < 0x556);
            task->state = 1;
            return;
        case 1:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age = tick;
                if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                    effectKillTask(work, task);
                }
                return;
            }
            spriteQuadDrawFlicker(coord, tick1, work->angle, work->period);
            glowDrawFlameDisc(coord, work->angle, work->scale >> 1);
            glowDrawFlameDisc(coord, (u16)work->angle * 2, work->scale >> 1);
            work->angle += 0x40;
            if (work->angle >= 0x581) {
                effectKillTask(work, task);
            }
            break;
    }
}

#include "../../shared/glow_draw_flame_band.inc.c"

#include "../../shared/glow_draw_flame_star.inc.c"

#define SPRITE_QUAD_SCALE 55
/// Reserves a flicker packet even when the GTE rejects the quad's projection.
#define SPRITE_QUAD_RESERVE_BEFORE_PROJECTION_CHECK 1
#define SPRITE_QUAD_ODD_LOOK(p)   \
    setRGB0(p, 0xC0, 0x60, 0x40); \
    SPRITE_QUAD_CORE_CELL(p);     \
    setSemiTrans(p, 1)
#define SPRITE_QUAD_EVEN_LOOK(p) \
    SPRITE_QUAD_RIM_CELL(p);     \
    setSemiTrans(p, 1);          \
    setShadeTex(p, 1)
#include "../../shared/sprite_quad_draw_flicker.inc.c"

#include "../../shared/glow_draw_flame_ring.inc.c"

void dryfieldDilapidatedHouseLightPrismTask(Task* task)
{
    enum {
        PRISM_LEFT_VIEWS   = 0x84A9C,
        PRISM_MIDDLE_VIEWS = 0x104B98,
        PRISM_RIGHT_VIEWS  = 0xA55F8,
    };

    GfxCoord* coord;
    s32       viewMask;

    viewMask = 1 << gGameSession->location.loc.view;
    coord    = task->extra.coordBody->coord;
    if (viewMask & PRISM_LEFT_VIEWS) {
        _dryfieldDilapidatedHouseDrawLightPrism(coord, 0);
    }
    if (viewMask & PRISM_MIDDLE_VIEWS) {
        _dryfieldDilapidatedHouseDrawLightPrism(coord, DRYFIELD_DILAPIDATED_HOUSE_PRISM_VERTEX_COUNT);
    }
    if (viewMask & PRISM_RIGHT_VIEWS) {
        _dryfieldDilapidatedHouseDrawLightPrism(coord, 2 * DRYFIELD_DILAPIDATED_HOUSE_PRISM_VERTEX_COUNT);
    }
}

void dryfieldDilapidatedHouseFlameConeTask(Task* task)
{
    enum {
        FLAME_CONE_INITIALIZE,
        FLAME_CONE_ACTIVE,
        FLAME_CONE_INITIAL_INTENSITY = 192,
        FLAME_CONE_INITIAL_RADIUS    = 256,
        FLAME_CONE_RADIUS_STEP       = 64,
        FLAME_CONE_INTENSITY_STEP    = 16,
    };

    EffectWork* work;
    s16         effectControl;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    work->age++;
    if (task->state == FLAME_CONE_INITIALIZE) {
        work->scale = FLAME_CONE_INITIAL_INTENSITY;
        work->angle = FLAME_CONE_INITIAL_RADIUS;
        task->state = FLAME_CONE_ACTIVE;
    }
    glowDrawFlameCone(task->extra.coordBody->coord, work->angle, work->scale);
    work->angle += FLAME_CONE_RADIUS_STEP;
    work->scale -= FLAME_CONE_INTENSITY_STEP;
    if (work->scale < FLAME_CONE_INTENSITY_STEP) {
        effectKillTask(work, task);
    }
}

void dryfieldDilapidatedHouseFlameRingTask(Task* task)
{
    enum {
        FLAME_RING_INITIALIZE,
        FLAME_RING_ACTIVE,
        FLAME_RING_INITIAL_INTENSITY = 128,
        FLAME_RING_INITIAL_RADIUS    = 256,
        FLAME_RING_WIDTH             = 256,
        FLAME_RING_RADIUS_STEP       = 128,
        FLAME_RING_INTENSITY_STEP    = 8,
        FLAME_RING_MIN_INTENSITY     = 9,
    };

    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    if (task->state == FLAME_RING_INITIALIZE) {
        // The spawn angle tilts the local XZ ring once; only radius and intensity ramp.
        gfxRotMatrixZ(&coord->coord, task->spawnArg1.value, GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work->scale = FLAME_RING_INITIAL_INTENSITY;
        work->angle = FLAME_RING_INITIAL_RADIUS;
        task->state = FLAME_RING_ACTIVE;
    }

    glowDrawFlameRing(coord, work->angle, FLAME_RING_WIDTH, work->scale);
    work->angle += FLAME_RING_RADIUS_STEP;
    work->scale -= FLAME_RING_INTENSITY_STEP;
    if (work->scale < FLAME_RING_MIN_INTENSITY) {
        effectKillTask(work, task);
    }
}
