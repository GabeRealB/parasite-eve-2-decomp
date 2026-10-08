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

#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/player_state.h"
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
/// `D_dryfield_dilapidated_house_80183EB4` when `sceneFindPlacedActor(1)` is non-zero
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

enum {
    DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT = 6,
    DRYFIELD_DILAPIDATED_HOUSE_BEAM_POINT_COUNT     = 4 * DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT,
};

static void _dryfieldDilapidatedHouseSetNegativeCapture(s32 durationFrames);
static void _dryfieldDilapidatedHouseMorphExit(Task* task);
static s32  _dryfieldDilapidatedHouseAdvanceMorph(Task* task);
static void _dryfieldDilapidatedHouseUpdateAttachmentTransform(Task* task);
static void _dryfieldDilapidatedHouseCopyModelVisibility(TmdObject* model, const TmdObject* parentModel);

static void _dryfieldDilapidatedHouseRoomInit(Task* task);
static void _dryfieldDilapidatedHouseRoomUpdate(Task* task);
static void _dryfieldDilapidatedHouseMorphAttachmentInit(Task* task);
static void _dryfieldDilapidatedHouseMorphAttachmentUpdate(Task* task);
static void _dryfieldDilapidatedHouseCurveDebugInit(Task* task);
static void _dryfieldDilapidatedHouseCurveDebugUpdate(Task* task);
static void _dryfieldDilapidatedHouseRingBeamInit(Task* task);
static void _dryfieldDilapidatedHouseCappedBeamUpdate(Task* task);
static void _dryfieldDilapidatedHouseRingBeamExit(Task* task);
static void _dryfieldDilapidatedHouseMorphConeInit(Task* task);
static void _dryfieldDilapidatedHouseMorphConeUpdate(Task* task);
static void _dryfieldDilapidatedHouseMorphConeExit(Task* task);
static void _dryfieldDilapidatedHouseDrawLightPrism(GfxCoord* coord, s16 firstVertex);
static void _dryfieldDilapidatedHouseDrawMorphCone(Task* task, const SVECTOR* ringVertices);
static void _dryfieldDilapidatedHouseDrawTwinTrail(s16 newestSlot, s16 colorMultipliers);

enum {
    DRYFIELD_DILAPIDATED_HOUSE_CONE_RING_VERTEX_COUNT = 16,
    DRYFIELD_DILAPIDATED_HOUSE_PRISM_RING_CORNERS     = 4,
    DRYFIELD_DILAPIDATED_HOUSE_PRISM_VERTEX_COUNT     = 8,
};

/// Nonpositive script requests accepted by the room's screen-wave command.
enum {
    DRYFIELD_DILAPIDATED_HOUSE_WAVE_PREPARE_DECODE = -2,
    DRYFIELD_DILAPIDATED_HOUSE_WAVE_FAST_RISE      = -1,
    DRYFIELD_DILAPIDATED_HOUSE_WAVE_SLOW_RISE      = 0,
};

/// States accepted by the room's script-controlled player head tracker.
enum {
    DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_INIT   = 0,
    DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_IDLE   = 1,
    DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_ACTIVE = 2,
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
static s32                        _dryfieldDilapidatedHouseResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32                        _dryfieldDilapidatedHouseIgnoreRoomCommand(Task* task, s32 messageId, s32 firstArg, s32 secondArg);
static s32                        _dryfieldDilapidatedHouseStartEncounterMsg(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg);
static void                       _dryfieldDilapidatedHouseNegativeCaptureTask(Task* task);
static void                       _dryfieldDilapidatedHouseBlackoutTask(Task* task);
static void                       _dryfieldDilapidatedHouseEncounterFinishTask(Task* task);
static void                       _dryfieldDilapidatedHouseHeadTrackTask(Task* task);
static void                       _dryfieldDilapidatedHouseShakeYTask(Task* task);
static void                       _dryfieldDilapidatedHouseModeExitCountdownTask(Task* task);
static void                       _dryfieldDilapidatedHouseSetHeadTrackState(s32 state);
static void                       _dryfieldDilapidatedHouseCancelEffects(void);
static void                       _dryfieldDilapidatedHouseControlScreenWave(s32 request);
static void                       _dryfieldDilapidatedHouseSetBlackoutDelay(s32 delayFrames);
static void                       _dryfieldDilapidatedHouseSetMorphAttachment(s32 enabled);
static void                       _dryfieldDilapidatedHouseLockAttachmentsForEvent(void);
static void                       _dryfieldDilapidatedHouseMorphAttachmentTask(Task* task);
static void                       _dryfieldDilapidatedHouseCurveDebugTask(Task* task);
static void                       _dryfieldDilapidatedHouseCappedBeamTask(Task* task);
static void                       _dryfieldDilapidatedHouseMorphConeTask(Task* task);

TaskDesc D_dryfield_dilapidated_house_80183E48[2] = {
    { { { TASK_BODY_NONE, 192 } }, _screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

TaskDesc D_dryfield_dilapidated_house_80183E64[2] = {
    { { { TASK_BODY_NONE, 32 } }, _dryfieldDilapidatedHouseNegativeCaptureTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

RECT gScreenNegativeFrameRect = { 0, 0, 320, 240 };

RECT gScreenNegativeStripRect = { 0, 0, 16, 240 };

enum { DRYFIELD_DILAPIDATED_HOUSE_MESSAGE_USE_KEY_ITEM = 5105 };

TaskMessageEntry D_dryfield_dilapidated_house_80183E8C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldDilapidatedHouseResolveRoomEvent },
    { DRYFIELD_DILAPIDATED_HOUSE_MESSAGE_USE_KEY_ITEM, _dryfieldDilapidatedHouseRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldDilapidatedHouseStartEncounterMsg },
    { ROOM_MESSAGE_COMMAND, _dryfieldDilapidatedHouseIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_dilapidated_house_80183EB4[4] = {
    { { { TASK_BODY_NONE, 97 } }, _dryfieldDilapidatedHouseHeadTrackTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _dryfieldDilapidatedHouseShakeYTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _dryfieldDilapidatedHouseBlackoutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _dryfieldDilapidatedHouseEncounterFinishTask, { .value = 0 } },
};

TaskDesc D_dryfield_dilapidated_house_80183EE4[2] = {
    { { { TASK_BODY_NONE, 192 } }, _dryfieldDilapidatedHouseModeExitCountdownTask, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetHeadTrackState }, { .value = DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_ACTIVE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801840B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_801840C4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80183FE8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetHeadTrackState }, { .value = DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_INIT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetHeadTrackState }, { .value = DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_INIT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldDilapidatedHouseCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_dilapidated_house_80184EA0[78] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldDilapidatedHouseLockAttachmentsForEvent }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseControlScreenWave }, { .value = DRYFIELD_DILAPIDATED_HOUSE_WAVE_PREPARE_DECODE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseControlScreenWave }, { .value = DRYFIELD_DILAPIDATED_HOUSE_WAVE_FAST_RISE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_dryfield_dilapidated_house_80189B40 }, { .vibrationSegments = D_dryfield_dilapidated_house_80189B50 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseControlScreenWave }, { .value = SCREEN_WAVE_RAMP_FALLING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseControlScreenWave }, { .value = DRYFIELD_DILAPIDATED_HOUSE_WAVE_SLOW_RISE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184300 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseControlScreenWave }, { .value = SCREEN_WAVE_RAMP_FINISHED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80184038 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_8018404C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetMorphAttachment }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetMorphAttachment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184268 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_dilapidated_house_80184240 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80184060 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_dryfield_dilapidated_house_80189B5C }, { .vibrationSegments = D_dryfield_dilapidated_house_80189B64 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseControlScreenWave }, { .value = SCREEN_WAVE_RAMP_FINISHED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetBlackoutDelay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _dryfieldDilapidatedHouseSetMorphAttachment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
#include "assets/dryfield_dilapidated_house_morph_08FDC.inc"
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
    { { { TASK_BODY_TMD, 192 } }, _dryfieldDilapidatedHouseMorphAttachmentTask, { .model = &_gDryfieldDilapidatedHouseModel08794 } },
    { { { TASK_BODY_TMD, 192 } }, _dryfieldDilapidatedHouseCurveDebugTask, { .model = &_gDryfieldDilapidatedHouseModel08D0C } },
    { { { TASK_BODY_COORD, 192 } }, _dryfieldDilapidatedHouseCappedBeamTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, _dryfieldDilapidatedHouseMorphConeTask, { .value = 0 } },
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

/// Captures the room's frame as a grayscale negative and holds scene drawing.
///
/// Argument 2 borrows writable `ScreenNegativeCaptureArgs` through completion.
/// Setup clears `done`; duration expiry or a later nonzero `done` releases it.
/// Use 0..32767 frames for an ordinary signed task countdown. The script pause
/// gate suspends every phase. Requires the loaded room overlay and exclusive
/// use of the resident image workspace until filtering finishes.
static void _dryfieldDilapidatedHouseNegativeCaptureTask(Task* task)
{
    _screenNegativeCaptureTask(task);
}

/// State handlers of the room task, indexed by `Task::state`: set-up, the room
/// gate, then `taskKill`.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D5C4 = {
    { _dryfieldDilapidatedHouseRoomInit, _dryfieldDilapidatedHouseRoomUpdate, taskKill },
};

/// Advances the room encounter after its introductory script releases control.
///
/// With event state zero, latch phase 1 becomes waiting phase 2. The later
/// encounter task starts once placed actor 0 reports absent, the attachment
/// wheel is closed and no display mode is pending. The receiver is unused.
/// Nonzero display debug mode also forwards actor and player names to a
/// development hook, independently of progression. Its numeric arguments'
/// meanings are unproven.
static void _dryfieldDilapidatedHouseRoomUpdate(Task* task)
{
    enum {
        EVENT_SCRIPT_IDLE       = 0,
        ENCOUNTER_INTRO_LATCHED = 1,
        ENCOUNTER_WAIT_RELEASE  = 2,
        ENCOUNTER_FINISH_TASK   = 3,
        ENCOUNTER_ACTOR_SLOT    = 0,
        HEAD_TRACK_ACTOR_SLOT   = 1,
    };

    if (gGameSession->eventState == EVENT_SCRIPT_IDLE) {
        if (D_dryfield_dilapidated_house_80183EFC == ENCOUNTER_INTRO_LATCHED) {
            D_dryfield_dilapidated_house_80183EFC = ENCOUNTER_WAIT_RELEASE;
        } else if ((D_dryfield_dilapidated_house_80183EFC == ENCOUNTER_WAIT_RELEASE) &&
                   (taskMessageDispatch(sceneFindPlacedActor(ENCOUNTER_ACTOR_SLOT), ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0)) {
            if (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    D_dryfield_dilapidated_house_80183EFC += 1;
                    taskSpawnFromTable(D_dryfield_dilapidated_house_80183EB4, ENCOUNTER_FINISH_TASK, 0, 0);
                }
            }
        }
    }
    if ((gDisplayState.debugMode != 0) && (sceneFindPlacedActor(HEAD_TRACK_ACTOR_SLOT) != 0)) {
        func_80724608(sceneFindPlacedActor(HEAD_TRACK_ACTOR_SLOT), -0x8C, 0xA, "AUNT");
        func_80724608(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), -0x8C, 0x14, "Player");
    }
}

/// Queues an opaque 320-by-256 black tile at the current ordering-table depth.
///
/// Requires room drawing coordinates centred on the screen and one free TILE
/// in the current frame's packet arena. Advances the packet cursor; the tile
/// stays borrowed by the GPU until that frame finishes.
static inline void _dryfieldDilapidatedHouseDrawBlackout(void)
{
    enum { COVER_LEFT_PIXELS   = -160,
           COVER_TOP_PIXELS    = -128,
           COVER_WIDTH_PIXELS  = 320,
           COVER_HEIGHT_PIXELS = 256 };
    TILE* tile;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    SetTile(tile);
    tile->x0 = COVER_LEFT_PIXELS;
    tile->y0 = COVER_TOP_PIXELS;
    tile->w  = COVER_WIDTH_PIXELS;
    tile->h  = COVER_HEIGHT_PIXELS;
    tile->r0 = 0;
    tile->g0 = 0;
    tile->b0 = 0;
    addPrim(gGpuCurrentOt, tile);
}

/// Times a negative freeze-frame between the room's initial and final blackouts.
///
/// States are 0 reset, 1 idle, 2 arm from argument 1's signed frame count,
/// 3 initial blackout and 4 countdown. Arming also draws the initial blackout.
/// Countdown zero starts a 15-frame negative capture; counts below -15 draw
/// black again. The shared countdown keeps decreasing. The event-script pause
/// gate suspends both state changes and drawing; other states do nothing.
static void _dryfieldDilapidatedHouseBlackoutTask(Task* task)
{
    enum { BLACKOUT_RESET          = 0,
           BLACKOUT_IDLE           = 1,
           BLACKOUT_ARM            = 2,
           BLACKOUT_INITIAL_COVER  = 3,
           BLACKOUT_COUNTDOWN      = 4,
           NEGATIVE_CAPTURE_FRAMES = 15 };
    s32 paintBlack;
    s32 remainingFrames;

    paintBlack = false;
    if (D_801156F9 == 0) {
        switch (task->state) {
            case BLACKOUT_RESET:
                task->state += 1;
                break;
            case BLACKOUT_IDLE:
                break;
            case BLACKOUT_ARM:
                D_dryfield_dilapidated_house_80189B70 = task->spawnArg1.value;
                task->state                           = task->state + 1;
                /* fallthrough */
            case BLACKOUT_INITIAL_COVER:
                paintBlack   = true;
                task->state += 1;
                break;
            case BLACKOUT_COUNTDOWN:
                remainingFrames                       = D_dryfield_dilapidated_house_80189B70 - 1;
                D_dryfield_dilapidated_house_80189B70 = remainingFrames;
                if (remainingFrames == 0) {
                    _dryfieldDilapidatedHouseSetNegativeCapture(NEGATIVE_CAPTURE_FRAMES);
                }
                if (D_dryfield_dilapidated_house_80189B70 < -NEGATIVE_CAPTURE_FRAMES) {
                    paintBlack = true;
                }
                break;
        }
        if (paintBlack != false) {
            _dryfieldDilapidatedHouseDrawBlackout();
        }
    }
}

/// Commits the encounter's saved progression and queues a captured-frame reload.
///
/// Restores player HP/MP and companion HP, selects Dryfield R08 room/warp 1
/// with scene event 1, and restores sprite variant 1. Gray Stalker's defeat
/// enables the second area-update list. Requires the live save, party state,
/// room update lists and gameplay reload resources; the spawn result is ignored.
static inline void _dryfieldDilapidatedHousePrepareNextSession(void)
{
    enum {
        COMPANION_2_POST_ENCOUNTER_SCHEDULE = 6,
        COMPANION_1_POST_ENCOUNTER_SCHEDULE = 1,
        POST_ENCOUNTER_TALK_PROGRESS        = 2,
        NEXT_SCENE_EVENT                    = 1,
        NEXT_WARP                           = 1,
        NEXT_ROOM                           = 1,
        DEFAULT_SPRITE_VARIANT              = 1,
    };
    // Commit the encounter's saved progression before reloading resources.
    areaApplySavedUpdates(D_dryfield_dilapidated_house_80189AA0);
    if (gameFlagGetNibble(GAME_FLAG_GRAY_STALKER_DEFEATED) != 0) {
        areaApplySavedUpdates(D_dryfield_dilapidated_house_80189B24);
    }
    gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, COMPANION_2_POST_ENCOUNTER_SCHEDULE);
    gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, COMPANION_1_POST_ENCOUNTER_SCHEDULE);
    gameFlagSetNibble(GAME_FLAG_GAS_STATION_MAIN_STREET_BLOCKED, true);
    gameFlagSetNibble(GAME_FLAG_GENERAL_STORE_UNDERPASS_BLOCKED, true);
    gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_CUTSCENE_SEEN, true);
    gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS, POST_ENCOUNTER_TALK_PROGRESS);
    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
    gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 0);
    playerStateRestoreFullHpMp();
    companionRestoreFullHp();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent         = NEXT_SCENE_EVENT;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_DRYFIELD;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = NEXT_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = NEXT_ROOM;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_DRYFIELD_R08;
    gDisplayState.spriteVariant                                 = DEFAULT_SPRITE_VARIANT;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
}

/// Runs the encounter's closing scene and prepares the next Dryfield session.
///
/// Starts the skippable event, waits for event state 2, credits placed actor 0's
/// battle rewards, allows four update ticks, then waits for battle reset. The
/// final state applies area and story progression, restores party HP/MP and
/// requests a captured-frame session reload. Demo scene 9 skips those final
/// changes. Every path through the final state releases this bodyless task.
static void _dryfieldDilapidatedHouseEncounterFinishTask(Task* task)
{
    enum {
        ENCOUNTER_START_SCENE       = 0,
        ENCOUNTER_WAIT_SCENE        = 1,
        ENCOUNTER_SETTLE_TICK_1     = 2,
        ENCOUNTER_SETTLE_TICK_2     = 3,
        ENCOUNTER_SETTLE_TICK_3     = 4,
        ENCOUNTER_SETTLE_TICK_4     = 5,
        ENCOUNTER_WAIT_BATTLE_RESET = 6,
        ENCOUNTER_APPLY_PROGRESSION = 7,
        EVENT_SCENE_RELEASED        = 2,
        ENCOUNTER_ACTOR_SLOT        = 0,
        UNUSED_BATTLE_RELEASE_ARG   = 0x1B,
        BATTLE_END_DELAY_FRAMES     = 3,
        DEMO_SCENE_SKIP_PROGRESSION = 9,
    };

    switch (task->state) {
        case ENCOUNTER_START_SCENE:
            evsStartScriptWithSkip(D_dryfield_dilapidated_house_80184EA0, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_dryfield_dilapidated_house_801855F0);
            task->state += 1;
            return;
        case ENCOUNTER_WAIT_SCENE:
            if (gGameSession->eventState == EVENT_SCENE_RELEASED) {
                sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(ENCOUNTER_ACTOR_SLOT), UNUSED_BATTLE_RELEASE_ARG);
                gSceneCombatState.signals.bytes.endDelayFrames = BATTLE_END_DELAY_FRAMES;
                task->state                                   += 1;
            }
            return;
        case ENCOUNTER_SETTLE_TICK_1:
        case ENCOUNTER_SETTLE_TICK_2:
        case ENCOUNTER_SETTLE_TICK_3:
        case ENCOUNTER_SETTLE_TICK_4:
            task->state += 1;
            return;
        case ENCOUNTER_WAIT_BATTLE_RESET:
            if (gGameSession->battleResetPending != 0) {
                task->state += 1;
            }
            return;
        case ENCOUNTER_APPLY_PROGRESSION:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != DEMO_SCENE_SKIP_PROGRESSION) {
                _dryfieldDilapidatedHousePrepareNextSession();
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

/// Resolves the warehouse variant and blocks departures requiring room dialogue.
///
/// Borrows a complete request and writable reply, which may alias. Executed
/// Dryfield warehouse requests select room 1 before its event or room 2 after it.
/// Warehouse departures during battle and all back-street departures return 0;
/// execution starts their CAP events, while queries suppress them. Other areas
/// return 1 to permit the transition. The receiver and message ID are unused.
static s32 _dryfieldDilapidatedHouseResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        WAREHOUSE_ROOM_BEFORE_EVENT = 1,
        WAREHOUSE_ROOM_AFTER_EVENT  = 2,
        CAP_EVENT_BACK_STREET       = 0x13,
        CAP_EVENT_WAREHOUSE_BATTLE  = 0x14,
        TRANSITION_BLOCKED          = 0,
        TRANSITION_ALLOWED          = 1,
    };

    u8 stage;

    *reply = *request;
    stage  = gGameSession->location.loc.stage;
    if (stage == GAME_STAGE_DRYFIELD) {
        if (request->areaId == GAME_AREA_DRYFIELD_WAREHOUSE) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_WAREHOUSE_EVENT_SEEN) == 0) {
                    reply->room = WAREHOUSE_ROOM_BEFORE_EVENT;
                } else {
                    reply->room = WAREHOUSE_ROOM_AFTER_EVENT;
                }
            }
        }
    }
    if ((request->areaId == GAME_AREA_DRYFIELD_WAREHOUSE) && (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED)) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            capSpawnEventIfIdle(CAP_EVENT_WAREHOUSE_BATTLE, CAP_EVENT_NO_FLAGS);
        }
        return TRANSITION_BLOCKED;
    }
    if (request->areaId == GAME_AREA_DRYFIELD_BACK_STREET) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            capSpawnEventIfIdle(CAP_EVENT_BACK_STREET, CAP_EVENT_NO_FLAGS);
        }
        return TRANSITION_BLOCKED;
    }
    return TRANSITION_ALLOWED;
}

/// Ignores room-command messages and returns zero; both payload words are unused.
static s32 _dryfieldDilapidatedHouseIgnoreRoomCommand(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    return 0;
}

/// Starts the encounter's introductory script once for room action 1.
///
/// Borrows a four-byte action request during synchronous dispatch; only its
/// action ID is read. Latches the encounter before starting its script with
/// the skip sequence and HUD restoration. Always returns 0; other arguments
/// are unused, and later action-1 requests leave the encounter unchanged.
static s32 _dryfieldDilapidatedHouseStartEncounterMsg(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg)
{
    enum { ACTION_START_ENCOUNTER = 1,
           ENCOUNTER_NOT_STARTED  = 0 };

    u8 actionId;

    actionId = request->actionId;
    if ((actionId == ACTION_START_ENCOUNTER) && (D_dryfield_dilapidated_house_80183EFC == ENCOUNTER_NOT_STARTED)) {
        D_dryfield_dilapidated_house_80183EFC = actionId;
        evsStartScriptWithSkip(D_dryfield_dilapidated_house_80184408, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_dryfield_dilapidated_house_80184C60);
    }
    return 0;
}

/// Aims the player's head at placed actor 1 while the cutscene enables tracking.
///
/// State 0 clears the first payload and enters idle state 1; state 2 applies
/// full-weight head aiming every frame. Both actors must have live head rigs.
/// This task borrows their models and remains alive when tracking is disabled.
static void _dryfieldDilapidatedHouseHeadTrackTask(Task* task)
{
    enum { HEAD_MAX_YAW   = 0x200,
           HEAD_MAX_PITCH = 0x180 }; // 4096 angle units per turn

    Task* playerTask;
    Task* targetTask;
    s32   state;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    targetTask = sceneFindPlacedActor(1);
    state      = task->state;
    switch (state) {
        case DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_INIT:
            task->spawnArg1.value = 0;
            task->state          += 1;
            return;
        case DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_ACTIVE:
            animationAimHeadAtTask(playerTask, targetTask, HEAD_MAX_YAW, HEAD_MAX_PITCH, ONE);
            /* fallthrough */
        case DRYFIELD_DILAPIDATED_HOUSE_HEAD_TRACK_IDLE:
            return;
    }
}

/// Alternates a decaying three-pixel vertical screen shake for a positive frame count.
///
/// `spawnArg1.value` is the duration, unchanged during playback. Initialization
/// seeds a shared countdown and returns without shaking. Only one instance may
/// run at a time; the task kills itself on the final shake frame.
static void _dryfieldDilapidatedHouseShakeYTask(Task* task)
{
    enum { SHAKE_INIT             = 0,
           SHAKE_RUNNING          = 1,
           SHAKE_AMPLITUDE_PIXELS = 3 };

    s32 framesLeft;
    s32 state;
    s32 shakePixels;

    state = task->state;
    switch (state) {
        case SHAKE_INIT:
            D_dryfield_dilapidated_house_80189B6C = task->spawnArg1.value;
            task->state                          += 1;
            return;
        case SHAKE_RUNNING:
            shakePixels = (D_dryfield_dilapidated_house_80189B6C * SHAKE_AMPLITUDE_PIXELS) / task->spawnArg1.value;
            if (D_dryfield_dilapidated_house_80189B6C & 1) {
                shakePixels = -shakePixels;
            }
            displaySetShakeY(shakePixels);
            framesLeft                            = D_dryfield_dilapidated_house_80189B6C - 1;
            D_dryfield_dilapidated_house_80189B6C = framesLeft;
            if (framesLeft == 0) {
                taskKill(task);
            }
            return;
    }
}

/// Requests stage-mode exit after the spawn-argument countdown becomes negative.
///
/// A nonnegative `spawnArg1.value` takes that many ticks plus two to exit.
/// The retained countdown is reloaded and decremented even after task teardown;
/// this callback requires deferred task collection.
static void _dryfieldDilapidatedHouseModeExitCountdownTask(Task* task)
{
    s32 framesLeft;

    framesLeft = task->spawnArg1.value;
    if (framesLeft < 0) {
        stageRequestModeTaskExit();
        taskKill(task);
        framesLeft = task->spawnArg1.value;
    }
    framesLeft            = framesLeft - 1;
    task->spawnArg1.value = framesLeft;
}

/// State handlers of the task `_dryfieldDilapidatedHouseCurveDebugTask` dispatches.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D61C = {
    { _dryfieldDilapidatedHouseCurveDebugInit, _dryfieldDilapidatedHouseCurveDebugUpdate, taskKill },
};

/// State handlers of the task `_dryfieldDilapidatedHouseCappedBeamTask` dispatches.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D628 = {
    { _dryfieldDilapidatedHouseRingBeamInit, _dryfieldDilapidatedHouseCappedBeamUpdate,
      _dryfieldDilapidatedHouseRingBeamExit },
};

/// State handlers of the task `_dryfieldDilapidatedHouseMorphConeTask` dispatches.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D634 = {
    { _dryfieldDilapidatedHouseMorphConeInit, _dryfieldDilapidatedHouseMorphConeUpdate,
      _dryfieldDilapidatedHouseMorphConeExit },
};

/// State handlers of the task `_dryfieldDilapidatedHouseMorphAttachmentTask`
/// dispatches.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D640 = {
    { _dryfieldDilapidatedHouseMorphAttachmentInit, _dryfieldDilapidatedHouseMorphAttachmentUpdate, taskKill },
};

/// Sets the cutscene's head tracking state, if its task exists.
///
/// Scripts pass 2 to enable tracking or 0 to reset it to idle on the next tick.
/// State 1 is already idle. Other values are stored unchanged and do no aiming.
static void _dryfieldDilapidatedHouseSetHeadTrackState(s32 state)
{
    if (D_dryfield_dilapidated_house_80189B78 != NULL) {
        D_dryfield_dilapidated_house_80189B78->state = state;
    }
}

/// Requests ordinary and parasite-energy effect cancellation at the next update.
///
/// Used as the encounter script's cleanup callback with a live room-effect state.
static void _dryfieldDilapidatedHouseCancelEffects(void)
{
    roomEffectRequestCancelAll();
}

/// Prepares mask-bit image decoding, starts a screen wave, or changes its phase.
///
/// Request -2 only prepares RGB16 decoding with pixel bit 15 set. Zero starts
/// a 100-frame rise; other negative requests start a five-frame rise. Both
/// peak at strength 256 (eight pixels of sine displacement). Positive requests
/// narrow to the context's signed-halfword phase: 1 falling, 2 finished.
/// Starting requires no other active wave and loaded room/wave resources;
/// the room's shared context remains borrowed until the wave ends.
static void _dryfieldDilapidatedHouseControlScreenWave(s32 request)
{
    enum { WAVE_SLOW_RISE_FRAMES = 100,
           WAVE_FAST_RISE_FRAMES = 5,
           WAVE_PEAK_STRENGTH    = 256 };
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if (request <= 0) {
        queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
        if (request != DRYFIELD_DILAPIDATED_HOUSE_WAVE_PREPARE_DECODE) {
            // Retain each branch's complete span/scale pair for this compiler.
            if (request == DRYFIELD_DILAPIDATED_HOUSE_WAVE_SLOW_RISE) {
                D_dryfield_dilapidated_house_80189C94.span  = WAVE_SLOW_RISE_FRAMES;
                D_dryfield_dilapidated_house_80189C94.scale = WAVE_PEAK_STRENGTH;
            } else {
                D_dryfield_dilapidated_house_80189C94.span  = WAVE_FAST_RISE_FRAMES;
                D_dryfield_dilapidated_house_80189C94.scale = WAVE_PEAK_STRENGTH;
            }
            taskSpawnFromTable(D_dryfield_dilapidated_house_80183E48, 0, 0, &D_dryfield_dilapidated_house_80189C94);
        }
    } else {
        D_dryfield_dilapidated_house_80189C94.state = request;
    }
}

/// Arms the room's blackout countdown, or ends the frozen negative with zero.
///
/// A nonzero `delayFrames` enters timer state 2 with that signed frame count;
/// scripts supply 40 or 180. Zero resets the timer to state 0 and requests
/// capture completion. The room must have successfully spawned its timer task.
static void _dryfieldDilapidatedHouseSetBlackoutDelay(s32 delayFrames)
{
    enum { BLACKOUT_RESET = 0,
           BLACKOUT_ARM   = 2 };

    if (delayFrames == 0) {
        D_dryfield_dilapidated_house_80189B7C->state = BLACKOUT_RESET;
        D_dryfield_dilapidated_house_80189B80.done   = true;
        return;
    }
    D_dryfield_dilapidated_house_80189B7C->state           = BLACKOUT_ARM;
    D_dryfield_dilapidated_house_80189B7C->spawnArg1.value = delayFrames;
}

/// Starts a negative freeze-frame with vibration, or requests its early release.
///
/// Nonzero durations store their low unsigned halfword before spawning the
/// capture; use 1..32767 frames for a positive signed task countdown. Zero
/// sets the shared completion flag. Starting requires no active capture;
/// the room's argument block and overlay must outlive the capture task.
static void _dryfieldDilapidatedHouseSetNegativeCapture(s32 durationFrames)
{
    if (durationFrames != 0) {
        padScriptSpawn(D_80114A24, D_80114A34);
        D_dryfield_dilapidated_house_80189B80.duration = durationFrames;
        taskSpawnFromTable(D_dryfield_dilapidated_house_80183E64, 0, 0, &D_dryfield_dilapidated_house_80189B80);
        return;
    }
    D_dryfield_dilapidated_house_80189B80.done = true;
}

/// Starts or removes the player's morph attachment and its child light effects.
///
/// Nonzero starts a new model task on player coordinate 3; zero kills and
/// clears the tracked task, if any. Callers start once before removing it:
/// repeated starts overwrite the handle without releasing the earlier task.
/// The live player's model needs coordinate 3 and must outlive the attachment.
static void _dryfieldDilapidatedHouseSetMorphAttachment(s32 enabled)
{
    enum { MORPH_ATTACHMENT_TASK   = 0,
           PLAYER_ATTACHMENT_COORD = 3 };
    if (enabled != 0) {
        D_dryfield_dilapidated_house_801857E8 =
            taskSpawnFromTable(D_dryfield_dilapidated_house_80186854, MORPH_ATTACHMENT_TASK, PLAYER_ATTACHMENT_COORD, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
        return;
    }
    if (D_dryfield_dilapidated_house_801857E8 != NULL) {
        taskKill(D_dryfield_dilapidated_house_801857E8);
        D_dryfield_dilapidated_house_801857E8 = NULL;
    }
}

/// Cancels effects and locks Parasite Energy selection for the scripted event.
///
/// Requires live room-effect and attachment state. The cancellation takes effect
/// on the next update; the event lock remains set for later code to release.
static void _dryfieldDilapidatedHouseLockAttachmentsForEvent(void)
{
    roomEffectRequestCancelAll();
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Registers the room controller and starts its persistent encounter helpers.
///
/// The bodyless task becomes the room message receiver. Spawn head tracking
/// only when placed actor 1 exists; always attempt the blackout timer and mark
/// the wave context finished. Music-suppression and weapon-reequip flow flags
/// replace the previous flags. Enter the running state even if a spawn fails.
static void _dryfieldDilapidatedHouseRoomInit(Task* task)
{
    enum { HEAD_TRACK_ACTOR_SLOT = 1,
           HEAD_TRACK_TASK       = 0,
           BLACKOUT_TASK         = 2 };
    task->msgTable = D_dryfield_dilapidated_house_80183E8C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (sceneFindPlacedActor(HEAD_TRACK_ACTOR_SLOT) != 0) {
        D_dryfield_dilapidated_house_80189B78 =
            taskSpawnFromTable(D_dryfield_dilapidated_house_80183EB4, HEAD_TRACK_TASK, 0, 0);
    }
    D_dryfield_dilapidated_house_80189C94.state = SCREEN_WAVE_RAMP_FINISHED;
    D_dryfield_dilapidated_house_80189B7C =
        taskSpawnFromTable(D_dryfield_dilapidated_house_80183EB4, BLACKOUT_TASK, 0, 0);
    gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_REEQUIP_WEAPON);
    task->state            += 1;
}

void dryfieldDilapidatedHouseRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_dilapidated_house_8017D5C4;
    stateHandlers.funcs[task->state](task);
}

/// Draws red crosses at the debug path's eight local control-point entries.
///
/// Borrows the spawning morph task's attachment transform, then view-projects
/// signed-halfword world points. Queues two lines per entry at OT tag 10,
/// including the repeated closing point, without depth or GTE-flag rejection.
static void _dryfieldDilapidatedHouseDrawCurveControlPoints(Task* task)
{
    enum { CONTROL_POINT_HALF_SIZE_PIXELS = 5,
           DEBUG_OT_OFFSET                = 10 };

    struct {
        SVECTOR point;
        s32     screenWord;
        s32     depthCue;
        s32     projectionFlags;
        s32     depth;
    } projection;
    MATRIX*                             attachmentMtx;
    Task*                               parentTask;
    _DryfieldDilapidatedHouseMorphWork* parentWork;
    LINE_F2*                            line;
    u16                                 screenX;
    s32                                 screenY;
    s16                                 leftX;
    s16                                 topY;
    s16                                 rightX;
    s16                                 bottomY;
    s32                                 pointIndex;

    pointIndex    = 0;
    parentTask    = task->spawnArg2.pointer;
    parentWork    = parentTask->work;
    attachmentMtx = &parentWork->attachMtx;
    // Place markers in the attachment's frame, narrowing world XYZ to halfwords.
    do {
        projection.point.vx = D_dryfield_dilapidated_house_801866B4[pointIndex].vx;
        projection.point.vy = D_dryfield_dilapidated_house_801866B4[pointIndex].vy;
        projection.point.vz = D_dryfield_dilapidated_house_801866B4[pointIndex].vz;
        gte_SetRotMatrix(attachmentMtx);
        gte_ldv0(&projection.point);
        gte_rtv0();
        gte_stsv(&projection.point);
        projection.point.vx = (u16)projection.point.vx + (u16)attachmentMtx->t[0];
        projection.point.vy = (u16)projection.point.vy + (u16)attachmentMtx->t[1];
        projection.point.vz = (u16)projection.point.vz + (u16)attachmentMtx->t[2];
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&projection.point);
        gte_rtps();
        gte_stsxy(&projection.screenWord);
        gte_stdp(&projection.depthCue);
        gte_stflg(&projection.projectionFlags);
        gte_stszotz(&projection.depth);
        line           = gGpuPrimCursor;
        screenX        = projection.screenWord;
        screenY        = projection.screenWord >> 16;
        gGpuPrimCursor = line + 1;
        leftX          = screenX - CONTROL_POINT_HALF_SIZE_PIXELS;
        topY           = screenY - CONTROL_POINT_HALF_SIZE_PIXELS;
        rightX         = screenX + CONTROL_POINT_HALF_SIZE_PIXELS;
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        bottomY  = screenY + CONTROL_POINT_HALF_SIZE_PIXELS;
        line->x0 = leftX;
        line->y0 = topY;
        line->x1 = rightX;
        line->y1 = bottomY;
        addPrim(gGpuCurrentOt + DEBUG_OT_OFFSET, line);

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        pointIndex++;
        line->x0 = rightX;
        line->y0 = topY;
        line->x1 = leftX;
        line->y1 = bottomY;
        addPrim(gGpuCurrentOt + DEBUG_OT_OFFSET, line);
    } while (pointIndex < ARRAY_SIZE(D_dryfield_dilapidated_house_801866B4));
}

/// Marks each debug Bezier segment with 21 projected crosses, green then blue.
///
/// The segments share control point 3 and end at point 6. Samples run from
/// t = 0 to just below t = 1 using the reverse sampler. The spawning morph task
/// must stay live; world XYZ narrows to signed halfwords before view projection.
/// Queues two lines per sample at OT tag 10 without projection rejection.
static void _dryfieldDilapidatedHouseDrawCurveSamples(Task* task)
{
    enum { CURVE_SUBDIVISIONS = 20,
           DEBUG_OT_OFFSET    = 10 };

    SVECTOR                             point;
    DVECTOR                             screenX; // Only vx is used as a signed pixel coordinate.
    DVECTOR                             screenY; // Only vx is used; vy is never accessed.
    long                                sampleXyz[3];
    s32                                 screenWord;
    s32                                 depthCue;
    s32                                 projectionFlags;
    s32                                 depth;
    MATRIX*                             attachmentMtx;
    Task*                               parentTask;
    _DryfieldDilapidatedHouseMorphWork* parentWork;
    LINE_F2*                            line;
    s32                                 sampleIndex;

    /// Transforms and projects this function's local-space sample in place.
    ///
    /// Captures `attachmentMtx` and the writable `point`, `screenWord`,
    /// `depthCue`, `projectionFlags` and `depth` locals. XYZ narrows to signed
    /// halfwords before view projection; depth is camera Z / 4. Leaves the view
    /// transform in the GTE and does not reject flags. No arguments or hidden
    /// evaluation; use as a standalone statement. Undefined at the body's end.
#define DRYFIELD_DILAPIDATED_HOUSE_PROJECT_CURVE_SAMPLE()    \
    {                                                        \
        gte_SetRotMatrix(attachmentMtx);                     \
        gte_ldv0(&point);                                    \
        gte_rtv0();                                          \
        gte_stsv(&point);                                    \
        point.vx = (u16)point.vx + (u16)attachmentMtx->t[0]; \
        point.vy = (u16)point.vy + (u16)attachmentMtx->t[1]; \
        point.vz = (u16)point.vz + (u16)attachmentMtx->t[2]; \
        gte_SetRotMatrix(&gGfxViewCoord.workm);              \
        gte_SetTransMatrix(&gGfxViewCoord.workm);            \
        gte_ldv0(&point);                                    \
        gte_rtps();                                          \
        gte_stsxy(&screenWord);                              \
        gte_stdp(&depthCue);                                 \
        gte_stflg(&projectionFlags);                         \
        gte_stszotz(&depth);                                 \
    }

    parentTask    = task->spawnArg2.pointer;
    parentWork    = parentTask->work;
    attachmentMtx = &parentWork->attachMtx;
    // Keep the two segments distinct so their markers carry different colours.
    for (sampleIndex = CURVE_SUBDIVISIONS; sampleIndex >= 0; sampleIndex--) {
        _bezierCurveEvaluate(D_dryfield_dilapidated_house_801866B4, D_dryfield_dilapidated_house_801866B4 + 3, CURVE_SUBDIVISIONS, sampleIndex, sampleXyz);
        point.vx = sampleXyz[0];
        point.vy = sampleXyz[1];
        point.vz = sampleXyz[2];
        DRYFIELD_DILAPIDATED_HOUSE_PROJECT_CURVE_SAMPLE();
        screenX.vx = screenWord;
        screenY.vx = screenWord >> 16;

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0xFF, 0);
        line->x0 = screenX.vx - 1;
        line->y0 = screenY.vx - 1;
        line->x1 = screenX.vx + 1;
        line->y1 = screenY.vx + 1;
        addPrim(gGpuCurrentOt + DEBUG_OT_OFFSET, line);

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0xFF, 0);
        line->x0 = screenX.vx + 1;
        line->y0 = screenY.vx - 1;
        line->x1 = screenX.vx - 1;
        line->y1 = screenY.vx + 1;
        addPrim(gGpuCurrentOt + DEBUG_OT_OFFSET, line);
    }
    for (sampleIndex = CURVE_SUBDIVISIONS; sampleIndex >= 0; sampleIndex--) {
        _bezierCurveEvaluate(D_dryfield_dilapidated_house_801866B4 + 3, D_dryfield_dilapidated_house_801866B4 + 6, CURVE_SUBDIVISIONS, sampleIndex, sampleXyz);
        point.vx = sampleXyz[0];
        point.vy = sampleXyz[1];
        point.vz = sampleXyz[2];
        DRYFIELD_DILAPIDATED_HOUSE_PROJECT_CURVE_SAMPLE();
        screenX.vx = screenWord;
        screenY.vx = screenWord >> 16;

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0, 0xFF);
        line->x0 = screenX.vx - 1;
        line->y0 = screenY.vx - 1;
        line->x1 = screenX.vx + 1;
        line->y1 = screenY.vx + 1;
        addPrim(gGpuCurrentOt + DEBUG_OT_OFFSET, line);

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0, 0xFF);
        line->x0 = screenX.vx + 1;
        line->y0 = screenY.vx - 1;
        line->x1 = screenX.vx - 1;
        line->y1 = screenY.vx + 1;
        addPrim(gGpuCurrentOt + DEBUG_OT_OFFSET, line);
    }
#undef DRYFIELD_DILAPIDATED_HOUSE_PROJECT_CURVE_SAMPLE
}

#include "../../shared/bezier_curve_evaluate.inc.c"

#define GLOW_DRAW_RING_BEAM_BRIGHTNESS(t) ((_DryfieldDilapidatedHouseMorphWork*)((Task*)(t)->spawnArg2.pointer)->work)->beamLevel
#define GLOW_DRAW_RING_BEAM_PHASE_STEP    0x40
#define GLOW_DRAW_RING_BEAM_OT_OFFSET     3
#define GLOW_DRAW_RING_BEAM_HALO_TPAGE    0xE1000465
#include "../../shared/glow_draw_ring_beam.inc.c"

/// Builds the morph attachment's projected core and halo beam caps.
///
/// Writes only X/Y of 24 caller-owned points: six-point core caps at each
/// endpoint, then the corresponding halo caps. Each cap contains its centre
/// followed by five rim points. Borrows the live spawning morph task's matrix
/// and signed low-halfword Q12 beam level. Spawn argument 1 equal to 1 mirrors
/// the local endpoints in X; the beam grows from quarter length to full length.
///
/// Both caps use the last endpoint's camera Z / 4 as their size divisor and
/// return it in `endDepth`; it must be nonzero. `projectionFlags` receives only
/// that endpoint's final FLAG, with no rejection. `killCountdown` supplies a
/// 4096-unit-per-turn halo pulse angle; this builder does not advance it.
static void _dryfieldDilapidatedHouseBuildCappedBeamPoints(Task* task, SVECTOR screenPoints[DRYFIELD_DILAPIDATED_HOUSE_BEAM_POINT_COUNT], s32* endDepth, s32* projectionFlags)
{
    SVECTOR                             startPoint;
    SVECTOR                             endPoint;
    MATRIX                              screenRotation;
    s32                                 startScreenWord;
    s32                                 depthCue;
    s32                                 endScreenWord;
    _DryfieldDilapidatedHouseMorphWork* work;
    MATRIX*                             attachmentMtx;
    const SVECTOR*                      endTemplate;
    s16                                 lengthQ12;
    s16                                 haloScaleQ12;
    s32                                 projectionDistance;
    s32                                 pointIndex;
    u16                                 brightnessQ12;
    s16                                 startX;
    s32                                 startY;
    s16                                 endX;
    s32                                 endY;
    s32                                 deltaX;
    s32                                 deltaY;
    s32                                 mirrorX;
    s32                                 screenAngle;
    Task*                               parentTask;

    mirrorX       = task->spawnArg1.value;
    parentTask    = task->spawnArg2.pointer;
    work          = parentTask->work;
    attachmentMtx = &work->attachMtx;
    brightnessQ12 = work->beamLevel;
    startPoint.vx = D_dryfield_dilapidated_house_80186794[0].vx;
    startPoint.vy = D_dryfield_dilapidated_house_80186794[0].vy;
    startPoint.vz = D_dryfield_dilapidated_house_80186794[0].vz;
    endTemplate   = &D_dryfield_dilapidated_house_80186794[1];
    endPoint.vx   = endTemplate->vx;
    endPoint.vy   = endTemplate->vy;
    endPoint.vz   = endTemplate->vz;
    if (mirrorX == 1) {
        startPoint.vx *= -1;
        endPoint.vx   *= -1;
    }
    gte_SetRotMatrix(attachmentMtx);
    gte_ldv0(&startPoint);
    gte_rtv0();
    gte_stsv(&startPoint);
    gte_ldv0(&endPoint);
    gte_rtv0();
    gte_stsv(&endPoint);
    lengthQ12      = (s16)brightnessQ12 * 0.75 + ONE / 4;
    endPoint.vx    = startPoint.vx + (endPoint.vx - startPoint.vx) * lengthQ12 / ONE;
    endPoint.vy    = startPoint.vy + (endPoint.vy - startPoint.vy) * lengthQ12 / ONE;
    endPoint.vz    = startPoint.vz + (endPoint.vz - startPoint.vz) * lengthQ12 / ONE;
    startPoint.vx += attachmentMtx->t[0];
    startPoint.vy += attachmentMtx->t[1];
    startPoint.vz += attachmentMtx->t[2];
    endPoint.vx   += attachmentMtx->t[0];
    endPoint.vy   += attachmentMtx->t[1];
    endPoint.vz   += attachmentMtx->t[2];
    // Project both ends. Each screen point comes back packed, x in the low
    // half and y in the high; the depth-cue coefficient is not used.
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_RotTransPers(&startPoint, &startScreenWord, &depthCue, projectionFlags, endDepth);
    gte_RotTransPers(&endPoint, &endScreenWord, &depthCue, projectionFlags, endDepth);
    deltaY             = (startScreenWord >> 16) - (endScreenWord >> 16);
    endX               = endScreenWord;
    startX             = startScreenWord;
    deltaX             = endX - startX;
    startY             = startScreenWord >> 16;
    endY               = endScreenWord >> 16;
    screenAngle        = ratan2(deltaX, deltaY);
    projectionDistance = gDisplayState.screenDistance;
    gfxSetRotIdentity(&screenRotation);
    RotMatrixZ(screenAngle, &screenRotation);
    gte_SetRotMatrix(&screenRotation);
    // Reuse the endpoint vectors for cap offsets, retaining the start point's world Z.
    // Rotate their XY contours along the projected segment.
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(D_dryfield_dilapidated_house_801867A4); pointIndex++) {
        startPoint.vx = D_dryfield_dilapidated_house_801867A4[pointIndex].vx * projectionDistance / *endDepth;
        startPoint.vy = D_dryfield_dilapidated_house_801867A4[pointIndex].vy * projectionDistance / *endDepth;
        gte_ldv0(&startPoint);
        gte_rtv0();
        gte_stsv(&endPoint);
        screenPoints[pointIndex].vx = endPoint.vx + startX;
        screenPoints[pointIndex].vy = endPoint.vy + startY;
    }
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(D_dryfield_dilapidated_house_801867D4); pointIndex++) {
        startPoint.vx = D_dryfield_dilapidated_house_801867D4[pointIndex].vx * projectionDistance / *endDepth;
        startPoint.vy = D_dryfield_dilapidated_house_801867D4[pointIndex].vy * projectionDistance / *endDepth;
        gte_ldv0(&startPoint);
        gte_rtv0();
        gte_stsv(&endPoint);
        screenPoints[pointIndex + DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT].vx = endPoint.vx + endX;
        screenPoints[pointIndex + DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT].vy = endPoint.vy + endY;
    }
    // Pulse enlarged halo caps around the same endpoints and depth as the core.
    haloScaleQ12 = ONE - rsin(task->killCountdown) * 0.5 + ONE;
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(D_dryfield_dilapidated_house_801867A4); pointIndex++) {
        startPoint.vx = ((D_dryfield_dilapidated_house_801867A4[pointIndex].vx * haloScaleQ12) >> GLOW_TRIG_SHIFT) * projectionDistance / *endDepth;
        startPoint.vy = ((D_dryfield_dilapidated_house_801867A4[pointIndex].vy * haloScaleQ12) >> GLOW_TRIG_SHIFT) * projectionDistance / *endDepth;
        gte_ldv0(&startPoint);
        gte_rtv0();
        gte_stsv(&endPoint);
        screenPoints[pointIndex + 2 * DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT].vx = endPoint.vx + startX;
        screenPoints[pointIndex + 2 * DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT].vy = endPoint.vy + startY;
    }
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(D_dryfield_dilapidated_house_801867D4); pointIndex++) {
        startPoint.vx = ((D_dryfield_dilapidated_house_801867D4[pointIndex].vx * haloScaleQ12) >> GLOW_TRIG_SHIFT) * projectionDistance / *endDepth;
        startPoint.vy = ((D_dryfield_dilapidated_house_801867D4[pointIndex].vy * haloScaleQ12) >> GLOW_TRIG_SHIFT) * projectionDistance / *endDepth;
        gte_ldv0(&startPoint);
        gte_rtv0();
        gte_stsv(&endPoint);
        screenPoints[pointIndex + 3 * DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT].vx = endPoint.vx + endX;
        screenPoints[pointIndex + 3 * DRYFIELD_DILAPIDATED_HOUSE_BEAM_CAP_POINT_COUNT].vy = endPoint.vy + endY;
    }
}

/// Projects two consecutive cone vertices and the vertex one ring later.
///
/// The GTE must hold the view transform. Store packed screen words and the
/// final outer corner's SZ3 / 4. `vertex` and `screen` each provide indices
/// 0, 1 and 16 from the current cursor; `depth` provides one writable word.
/// At the last inner vertex, index 1 is the first outer vertex rather than the
/// closing inner vertex. The drawer closes that edge with its saved first point.
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

/// Rebuilds two sixteen-vertex world-space ellipses for the morph cone.
///
/// Writes XYZ of 32 borrowed output vectors: inner ring first, then outer.
/// The spawning morph task supplies the attachment frame and must stay live.
/// Each outer Z ripples by its per-vertex phase, advanced once per call in
/// 16384 units per turn. Rotation, centre offsets and translation narrow to
/// signed halfwords; each output's fourth halfword remains untouched.
static void _dryfieldDilapidatedHouseBuildMorphConeRings(Task* task, SVECTOR* ringVertices)
{
    enum {
        CONE_RING_VERTEX_COUNT = DRYFIELD_DILAPIDATED_HOUSE_CONE_RING_VERTEX_COUNT,
        INNER_RADIUS_X         = 150,
        INNER_RADIUS_Y         = 75,
        OUTER_RADIUS_X         = 250,
        OUTER_RADIUS_Y         = 125,
        RIM_RIPPLE_AMPLITUDE   = 100,
    }; // Radii and ripple amplitude are attachment-local coordinate units.

    _DryfieldDilapidatedHouseConeWork*  coneWork;
    _DryfieldDilapidatedHouseMorphWork* parentWork;
    MATRIX*                             attachmentMtx;
    const SVECTOR*                      innerCentre;
    const SVECTOR*                      outerCentre;
    SVECTOR                             ringCentres[2];
    SVECTOR*                            innerVertex;
    SVECTOR*                            outerVertex;
    s16                                 translationX;
    s16                                 translationY;
    s16                                 translationZ;
    s32                                 vertexIndex;
    s32                                 angle;
    s32                                 cosineQ12;
    s32                                 sineQ12;
    Task*                               parentTask;

    innerVertex = ringVertices;
    outerVertex = &ringVertices[CONE_RING_VERTEX_COUNT];

    coneWork   = task->work;
    parentTask = task->spawnArg2.pointer;
    parentWork = parentTask->work;

    innerCentre       = D_dryfield_dilapidated_house_80186844;
    outerCentre       = D_dryfield_dilapidated_house_80186844 + 1;
    ringCentres[0].vx = innerCentre->vx;
    ringCentres[0].vy = innerCentre->vy;
    ringCentres[0].vz = innerCentre->vz;
    ringCentres[1].vx = outerCentre->vx;
    ringCentres[1].vy = outerCentre->vy;
    ringCentres[1].vz = outerCentre->vz;

    attachmentMtx = &parentWork->attachMtx;

    translationX = attachmentMtx->t[0];
    translationY = attachmentMtx->t[1];
    translationZ = attachmentMtx->t[2];

    gte_SetRotMatrix(attachmentMtx);

    // Walk both rings together; only the outer ring's axial coordinate ripples.
    for (vertexIndex = 0; vertexIndex < CONE_RING_VERTEX_COUNT; vertexIndex++) {
        angle     = (vertexIndex << GLOW_TRIG_SHIFT) >> 4;
        cosineQ12 = rcos(angle);
        sineQ12   = rsin(angle);

        innerVertex->vx = ringCentres[0].vx + ((cosineQ12 * INNER_RADIUS_X) >> GLOW_TRIG_SHIFT);
        innerVertex->vy = ringCentres[0].vy + ((sineQ12 * INNER_RADIUS_Y) >> GLOW_TRIG_SHIFT);
        innerVertex->vz = ringCentres[0].vz;

        gte_ldv0(innerVertex);
        gte_rtv0();
        gte_stsv(innerVertex);

        innerVertex->vx += translationX;
        innerVertex->vy += translationY;
        innerVertex->vz += translationZ;
        innerVertex++;

        outerVertex->vx = ringCentres[1].vx + ((cosineQ12 * OUTER_RADIUS_X) >> GLOW_TRIG_SHIFT);
        outerVertex->vy = ringCentres[1].vy + ((sineQ12 * OUTER_RADIUS_Y) >> GLOW_TRIG_SHIFT);
        // Ripple the outer ring along its axis, then advance that vertex's phase.
        outerVertex->vz = ringCentres[1].vz + ((rsin(coneWork->rimPhase[vertexIndex] >> 2) * RIM_RIPPLE_AMPLITUDE) >> GLOW_TRIG_SHIFT);

        coneWork->rimPhase[vertexIndex] = (coneWork->rimPhase[vertexIndex] + D_dryfield_dilapidated_house_80186804[vertexIndex]) &
                                          (DRYFIELD_DILAPIDATED_HOUSE_CONE_RIM_PHASE_PERIOD - 1);

        gte_ldv0(outerVertex);
        gte_rtv0();
        gte_stsv(outerVertex);

        outerVertex->vx += translationX;
        outerVertex->vy += translationY;
        outerVertex->vz += translationZ;
        outerVertex++;
    }
}

#include "../../shared/model_morph_blend.inc.c"

/// Saves a model's rest XYZ components into a morph's borrowed snapshot buffers.
///
/// Requires a live TMD task and a readable morph record. `savedVertexCount`
/// and `normalCount` are nonnegative element counts from index zero, independent
/// of the delta range. The source and writable snapshot arrays must cover those
/// counts and must not overlap. Normals are saved only when `targetNormals` is
/// non-NULL; neither target array is read. Each vector's fourth halfword stays
/// intact. All storage is borrowed during this call; the saved XYZ values must
/// remain unchanged for subsequent blends using this rest shape.
static inline void _modelMorphSaveRestShape(const Task* task, const ModelMorph* morph)
{
    const TmdSource* source;
    SVECTOR*         savedVertices;
    SVECTOR*         savedNormals;
    const SVECTOR*   vertices;
    const SVECTOR*   normals;
    s32              elementIndex;

    source        = task->extra.tmd->source;
    savedVertices = morph->savedVertices;
    savedNormals  = morph->savedNormals;
    vertices      = source->verts;
    for (elementIndex = 0; elementIndex < morph->savedVertexCount; elementIndex++) {
        savedVertices[elementIndex].vx = vertices[elementIndex].vx;
        savedVertices[elementIndex].vy = vertices[elementIndex].vy;
        savedVertices[elementIndex].vz = vertices[elementIndex].vz;
    }
    if (morph->targetNormals != NULL) {
        normals = source->normals;
        for (elementIndex = 0; elementIndex < morph->normalCount; elementIndex++) {
            savedNormals[elementIndex].vx = normals[elementIndex].vx;
            savedNormals[elementIndex].vy = normals[elementIndex].vy;
            savedNormals[elementIndex].vz = normals[elementIndex].vz;
        }
    }
}

/// Initializes the player's morphing model and its two beams and two cones.
///
/// Argument 2 borrows a live TMD parent; argument 1 selects one of its model
/// coordinates (the script uses 3). Owns a 0x6C-byte primary-heap work block and
/// joins the parent's task tree. Allocation failure kills the task. The shared
/// morph snapshots 40 XYZ vertices, leaves normals alone, then advances once.
/// Children borrow this task/work until teardown; failed child spawns are skipped.
/// Their initial matrix copies retain the original uninitialized work contents;
/// the attachment matrix and remaining levels are first filled by the update.
static void _dryfieldDilapidatedHouseMorphAttachmentInit(Task* task)
{
    enum { ATTACHMENT_OT_OFFSET     = 4,
           CONE_TASK                = 3,
           BEAM_TASK                = 2,
           FIRST_CONE_PHASE_FRAMES  = 9,
           SECOND_CONE_PHASE_FRAMES = 17,
           BEAM_UNMIRRORED          = 0,
           BEAM_MIRRORED            = 1 };
    Task*                               parentTask;
    TmdObject*                          model;
    TmdObject*                          parentModel;
    GfxCoord*                           modelCoord;
    GfxCoord*                           attachmentCoord;
    _DryfieldDilapidatedHouseMorphWork* work;
    TaskDesc*                           effectTasks;
    Task*                               childTask;
    GfxCoord*                           childCoord;
    u16                                 modelFlags;

    parentTask      = task->spawnArg2.pointer;
    model           = task->extra.tmd;
    parentModel     = parentTask->extra.tmd;
    modelCoord      = model->coords;
    attachmentCoord = parentModel->coords;
    work            = memMalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    // Borrow the selected player model part and its lighting, owning our work.
    task->work       = work;
    work->morphLevel = 0;
    modelFlags       = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    model->flags     = modelFlags;
    if (!(parentModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        model->flags = modelFlags & ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    model->otOffset          = ATTACHMENT_OT_OFFSET;
    model->flags            |= TMD_OBJECT_SEMI_TRANS;
    attachmentCoord         += task->spawnArg1.value;
    modelCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    modelCoord->parent       = attachmentCoord;
    model->lightMtx          = parentModel->lightMtx;
    model->colorMtx          = parentModel->colorMtx;
    taskReparent(parentTask, task);

    // Preserve the shared rest geometry before applying the initial deformation.
    _modelMorphSaveRestShape(task, &D_dryfield_dilapidated_house_8018669C);

    _dryfieldDilapidatedHouseAdvanceMorph(task);

    // The child descriptors allocate coordinate bodies, not model bodies.
    effectTasks = D_dryfield_dilapidated_house_80186854;
    childTask   = taskSpawnFromTable(effectTasks, CONE_TASK, FIRST_CONE_PHASE_FRAMES, task);
    if (childTask != NULL) {
        childCoord        = childTask->extra.coordBody->coord;
        childCoord->coord = work->attachMtx;
    }
    childTask = taskSpawnFromTable(effectTasks, CONE_TASK, SECOND_CONE_PHASE_FRAMES, task);
    if (childTask != NULL) {
        childCoord        = childTask->extra.coordBody->coord;
        childCoord->coord = work->attachMtx;
    }
    childTask = taskSpawnFromTable(effectTasks, BEAM_TASK, BEAM_UNMIRRORED, task);
    if (childTask != NULL) {
        childCoord        = childTask->extra.coordBody->coord;
        childCoord->coord = work->attachMtx;
    }
    childTask = taskSpawnFromTable(effectTasks, BEAM_TASK, BEAM_MIRRORED, task);
    if (childTask != NULL) {
        childCoord        = childTask->extra.coordBody->coord;
        childCoord->coord = work->attachMtx;
    }

    task->exitCallback = _dryfieldDilapidatedHouseMorphExit;
    task->state       += 1;
}

/// Runs the morphing model attachment and its beam and cone children.
///
/// `spawnArg2.pointer` borrows a live parent model task; `spawnArg1.value`
/// selects an existing coordinate in that model. States are 0 setup, 1 update,
/// and 2 teardown; other indices are invalid. The parent and shared morph record
/// must outlive updates. Teardown releases the children and owned work block.
static void _dryfieldDilapidatedHouseMorphAttachmentTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_dilapidated_house_8017D640;
    stateHandlers.funcs[task->state](task);
}

/// Updates the attachment's visibility, transform and shared Q12 morph progress.
///
/// Requires setup-owned work, saved rest vertices and a live spawning model.
/// The parent coordinate chain is composed into the attachment matrix; progress
/// advances by 68 toward `ONE` and drives both beam and cone children.
static void _dryfieldDilapidatedHouseMorphAttachmentUpdate(Task* task)
{
    _DryfieldDilapidatedHouseMorphWork* work;
    Task*                               parentTask;
    s32                                 morphProgress;

    work       = task->work;
    parentTask = task->spawnArg2.pointer;
    _dryfieldDilapidatedHouseCopyModelVisibility(task->extra.tmd,
                                                 parentTask->extra.tmd);
    _dryfieldDilapidatedHouseUpdateAttachmentTransform(task);
    morphProgress    = _dryfieldDilapidatedHouseAdvanceMorph(task);
    work->morphLevel = morphProgress;
    work->coneLevel  = morphProgress;
    work->beamLevel  = morphProgress;
}

/// Releases the morph attachment task through ordinary task teardown.
static void _dryfieldDilapidatedHouseMorphExit(Task* task)
{
    taskKill(task);
}

/// Advances morph progress toward `ONE` and applies the complementary deformation.
///
/// The task owns a live morph model with its rest vertices already saved.
/// `killCountdown` holds signed Q12 progress, normally 0..`ONE`; each call
/// adds 68 and saturates above `ONE`, without clamping negative input.
/// Returns the full-width progress after storing its low signed halfword.
static s32 _dryfieldDilapidatedHouseAdvanceMorph(Task* task)
{
    enum { MORPH_PROGRESS_STEP_Q12 = 68 };

    s32 ramp;

    ramp = task->killCountdown + MORPH_PROGRESS_STEP_Q12;
    if (ramp >= ONE + 1) {
        ramp = ONE;
    }
    task->killCountdown = ramp;
    _modelMorphBlend(task, &D_dryfield_dilapidated_house_8018669C, ONE - ramp);
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

    modelCoord    = task->extra.tmd->coords;
    work          = task->work;
    parentTask    = task->spawnArg2.pointer;
    parentCoord   = parentTask->extra.tmd->coords;
    attachmentMtx = &work->attachMtx;
    gfxSetRotIdentity(&work->attachMtx);
    attachmentMtx->t[0] = 0;
    attachmentMtx->t[1] = 0;
    attachmentMtx->t[2] = 0;
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

/// Dispatches the hidden-model task that marks the attachment's debug Bezier path.
///
/// States 0, 1 and 2 initialize, draw markers and kill the task, respectively.
/// The spawn payload borrows the live morph task. The room retains this
/// descriptor but never spawns it; dispatch preserves the three-handler copy.
static void _dryfieldDilapidatedHouseCurveDebugTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_dilapidated_house_8017D61C;
    stateHandlers.funcs[task->state](task);
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

/// Draws the debug path's control-point crosses and both sampled segments.
static void _dryfieldDilapidatedHouseCurveDebugUpdate(Task* task)
{
    _dryfieldDilapidatedHouseDrawCurveControlPoints(task);
    _dryfieldDilapidatedHouseDrawCurveSamples(task);
}

#include "../../shared/bezier_curve_coefficients.inc.c"

/// Dispatches a capped beam attached to the spawning morph model.
///
/// Requires a coordinate body and a live morph task in `spawnArg2.pointer`;
/// argument 1 equal to 1 mirrors the endpoints. States 0, 1 and 2 initialize,
/// build/draw and detach/kill. Retains the stack copy of the three handlers.
static void _dryfieldDilapidatedHouseCappedBeamTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_dilapidated_house_8017D628;
    stateHandlers.funcs[task->state](task);
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

/// Builds one capped beam and queues two draws of the same projected geometry.
///
/// The final endpoint depth sorts both draws; projection flags are unused.
/// Each draw advances the next geometry phase by 64, wrapping at half a turn.
static void _dryfieldDilapidatedHouseCappedBeamUpdate(Task* task)
{
    SVECTOR screenPoints[DRYFIELD_DILAPIDATED_HOUSE_BEAM_POINT_COUNT];
    s32     endDepth;
    s32     projectionFlags;

    _dryfieldDilapidatedHouseBuildCappedBeamPoints(task, screenPoints, &endDepth, &projectionFlags);
    // The repeated draw also repeats the pulse-phase advance for the next frame.
    _glowDrawCappedBeam(task, screenPoints, endDepth);
    _glowDrawCappedBeam(task, screenPoints, endDepth);
}

/// Detaches the ring beam from its parent model before releasing the task.
static void _dryfieldDilapidatedHouseRingBeamExit(Task* task)
{
    GfxCoord* coord;

    coord         = task->extra.coordBody->coord;
    coord->parent = &gGfxViewCoord;
    taskKill(task);
}

/// Runs the morph cone's initialization, build/draw or detachment state.
///
/// `task->state` must be 0, 1 or 2; dispatch is unchecked. Start with a live
/// coordinate body, argument 1's phase advance in frames and argument 2's
/// spawning morph-model task. Initialization owns the rim-phase work block
/// and links the cone to that parent's teardown tree; keep both tasks and
/// this overlay live until the coordinate is detached and the cone released.
static void _dryfieldDilapidatedHouseMorphConeTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_dilapidated_house_8017D634;
    stateHandlers.funcs[task->state](task);
}

/// Attaches the morph cone's coordinate and seeds its sixteen rim phases.
///
/// Requires a coordinate body and the live spawning morph task in argument 2.
/// Argument 1 is the initial phase advance in frames (the two instances use
/// 9 and 17). Owns a primary-heap phase block; allocation failure kills the
/// task, while success attaches it to the parent's teardown tree and enters
/// the per-frame build/draw state. Phase arithmetic wraps to 16384 units/turn.
static void _dryfieldDilapidatedHouseMorphConeInit(Task* task)
{
    _DryfieldDilapidatedHouseConeWork* work;
    GfxCoord*                          coord;
    s32                                vertexIndex;
    Task*                              parentTask;

    coord = task->extra.coordBody->coord;
    work  = memMalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work = work;
    for (vertexIndex = 0; vertexIndex < ARRAY_SIZE(work->rimPhase); vertexIndex++) {
        work->rimPhase[vertexIndex] = (D_dryfield_dilapidated_house_80186804[vertexIndex] * task->spawnArg1.value) &
                                      (DRYFIELD_DILAPIDATED_HOUSE_CONE_RIM_PHASE_PERIOD - 1);
    }
    parentTask    = task->spawnArg2.pointer;
    coord->parent = parentTask->extra.tmd->coords;
    taskReparent(task->spawnArg2.pointer, task);
    task->state += 1;
}

/// Rebuilds the morph cone's two rings, advancing its ripple, then draws the cone.
static void _dryfieldDilapidatedHouseMorphConeUpdate(Task* task)
{
    SVECTOR ringVertices[2 * DRYFIELD_DILAPIDATED_HOUSE_CONE_RING_VERTEX_COUNT];

    _dryfieldDilapidatedHouseBuildMorphConeRings(task, ringVertices);
    _dryfieldDilapidatedHouseDrawMorphCone(task, ringVertices);
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

/// Stores a trail endpoint in world space, independent of later anchor motion.
///
/// `endpointCoord->workm.t` must be a current view-space endpoint position;
/// `gGfxViewCoord.workm` must be current with an orthonormal view rotation.
/// Copies the complete endpoint cache into `historyFrame->workm`, then removes
/// the view transform to form its world-space `coord`, parented to
/// `gGfxViewCoord`. Rotation coefficients are copied and rebased too, although
/// the ribbon consumes only translation. Rotations use 12 fractional bits;
/// translations are signed 32-bit game coordinates.
///
/// The word-aligned coordinates must be disjoint from each other, the view
/// nodes and the scratch reservation. The frame remains caller-owned;
/// no endpoint pointer is retained. Leaves `composeStamp` and `param` untouched:
/// the caller must mark the frame dirty before composing it again.
/// Loads endpoint rotation and translation into the GTE before rebasing and
/// changes GTE arithmetic state. Requires 48 free bytes on the initialized
/// scratch stack, released before return.
static inline void _dryfieldDilapidatedHouseStoreTrailFrame(GfxCoord* historyFrame, const GfxCoord* endpointCoord)
{
    historyFrame->parent = &gGfxViewCoord;
    historyFrame->workm  = endpointCoord->workm;
    gte_SetRotMatrix(&endpointCoord->workm);
    gte_SetTransMatrix(&endpointCoord->workm);
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

void dryfieldDilapidatedHouseFireBlastTask(Task* task)
{
    enum {
        FIRE_BLAST_INITIALIZE         = 0,
        FIRE_BLAST_EXPAND             = 1,
        FIRE_BLAST_INTENSITY          = 192,
        FIRE_BLAST_FLASH_RADIUS       = 1280,
        FIRE_BLAST_EXPANSION_RADIUS   = 896,
        FIRE_BLAST_RADIUS_STEP        = 64,
        FIRE_BLAST_LAST_RADIUS        = 1408,
        FIRE_BLAST_LIGHT_FRAMES       = 4,
        FIRE_BLAST_LIGHT_INNER_RADIUS = 512,
        FIRE_BLAST_LIGHT_OUTER_RADIUS = 8192,
        FIRE_BLAST_LIGHT_BASE_Q12     = ONE / 2,
        FIRE_BLAST_LIGHT_RANDOM_MASK  = 0x700,
        FIRE_BLAST_RING_ANGLE_STEP    = 0x2AA,
        FIRE_BLAST_RING_ANGLE_LIMIT   = 0x556,
    };

    EffectWork*                    work;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          pointLight;
    EffectWork*                    ringEffect;
    s16                            previousAge;
    s16                            age;
    s32                            ringAngle;
    u8                             flashRgb[3];

    work        = task->spawnArg2.pointer;
    coord       = task->extra.coordBody->coord;
    previousAge = work->age;
    age         = previousAge + 1;
    work->age   = age;
    lightSlot   = &gWorldCoordTransientPointLights[0];
    pointLight  = &lightSlot->light;

/// Seeds transient point-light slot 0 from the blast's world-space origin.
///
/// Captures `coord`, `lightSlot`, `pointLight` and this function's light constants.
/// The aliases must designate slot 0 and its `light` member. Advances the shared
/// random generator once for Q12 red intensity 2048..3840, with half/quarter
/// green/blue. Expiration is four gameplay frames; the slot remains borrowed.
/// No arguments are evaluated; use as a standalone statement. Undefined below.
#define DRYFIELD_DILAPIDATED_HOUSE_SEED_FIRE_BLAST_LIGHT()                                                                                         \
    {                                                                                                                                              \
        s16 lightIntensityQ12;                                                                                                                     \
        gWorldCoordTransientPointLights[0].framesLeft      = FIRE_BLAST_LIGHT_FRAMES;                                                              \
        pointLight->inner                                  = FIRE_BLAST_LIGHT_INNER_RADIUS;                                                        \
        pointLight->outer                                  = FIRE_BLAST_LIGHT_OUTER_RADIUS;                                                        \
        gRandomLcgState                                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                       \
        lightIntensityQ12                                  = ((gRandomLcgState >> 16) & FIRE_BLAST_LIGHT_RANDOM_MASK) + FIRE_BLAST_LIGHT_BASE_Q12; \
        pointLight->head.color.r                           = lightIntensityQ12;                                                                    \
        pointLight->head.color.g                           = lightIntensityQ12 >> 1;                                                               \
        pointLight->head.color.b                           = lightIntensityQ12 >> 2;                                                               \
        pointLight->head.transform.coord.coord.t[0]        = coord->coord.t[0];                                                                    \
        pointLight->head.transform.coord.coord.t[1]        = coord->coord.t[1];                                                                    \
        pointLight->head.transform.coord.coord.t[2]        = coord->coord.t[2];                                                                    \
        lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;                                                                 \
    }

    switch (task->state) {
        case FIRE_BLAST_INITIALIZE:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age = previousAge;
                if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                    effectKillTask(work, task);
                }
                return;
            }
            // Seed the flash before resetting the radius for the expanding phase.
            work->scale     = FIRE_BLAST_INTENSITY;
            work->angle     = FIRE_BLAST_FLASH_RADIUS;
            work->index     = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
            effectSpawn(EFFECT_DRYFIELD_DILAPIDATED_HOUSE_FLAME_CONE, coord, 0, NULL);
            flashRgb[0] = 0xFF;
            flashRgb[1] = 0x7F;
            flashRgb[2] = 0x3F;
            effectDrawScreenTint(flashRgb, GPU_BLEND_ADD);
            // The light uses Q12 colour and the blast origin's world translation.
            DRYFIELD_DILAPIDATED_HOUSE_SEED_FIRE_BLAST_LIGHT();
            _spriteQuadDrawFlicker(coord, work->age, work->angle, work->period);
            glowDrawFlameDisc(coord, work->angle, work->scale >> 1);
            work->angle = FIRE_BLAST_EXPANSION_RADIUS;
            // Successful rings become children so cancellation tears them down too.
            for (ringAngle = 0; ringAngle < FIRE_BLAST_RING_ANGLE_LIMIT; ringAngle += FIRE_BLAST_RING_ANGLE_STEP) {
                ringEffect = effectSpawn(EFFECT_DILAPIDATED_HOUSE_FLAME_RING, coord, ringAngle, NULL);
                if (ringEffect != NULL) {
                    taskReparent(task, ringEffect->task);
                }
            }
            task->state = FIRE_BLAST_EXPAND;
            return;
        case FIRE_BLAST_EXPAND:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age = previousAge;
                if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                    effectKillTask(work, task);
                }
                return;
            }
            _spriteQuadDrawFlicker(coord, age, work->angle, work->period);
            glowDrawFlameDisc(coord, work->angle, work->scale >> 1);
            glowDrawFlameDisc(coord, (u16)work->angle * 2, work->scale >> 1);
            work->angle += FIRE_BLAST_RADIUS_STEP;
            if (work->angle >= FIRE_BLAST_LAST_RADIUS + 1) {
                effectKillTask(work, task);
            }
            break;
    }
}

#undef DRYFIELD_DILAPIDATED_HOUSE_SEED_FIRE_BLAST_LIGHT

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
