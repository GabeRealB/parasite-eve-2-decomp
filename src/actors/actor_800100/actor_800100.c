#include "actors/actor_800100.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachments.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#include "../../shared/pyke_flame.h"

static void func_actor_800100_801635F4(Task* arg0);
static void func_actor_800100_80163A58(Task* arg0);
static void func_actor_800100_80165528(Task* arg0);

/// Scratch-stack block of the aim beam: the coordinate the beam is drawn on.
///
/// The beam is a line from the equipped weapon along the aim, ended by a
/// textured square. `coord` is placed twice, each time by moving a node along
/// `offset`. The first move starts from a copy of the weapon model's root
/// node turned a quarter turn about X and gives the beam's start, which the
/// line is drawn from. The second moves `coord` along its own Y axis by
/// `contactDistance` plus 0x38, which is where the square is drawn.
///
/// The block lives for the one call that draws both parts. Nothing in the
/// package calls that routine.
typedef struct {
    GfxCoord coord;           // Node the beam is drawn on, parented to the view coordinate by each placement
    SVECTOR  offset;          // Displacement of the next placement, in the frame of the node it starts from; `pad` is never written
    u16      contactDistance; // X/Z distance from the beam's start to the aim capsule's contact; 0 while it has none
} _Actor800100AimBeamScratch;
STATIC_ASSERT_SIZEOF(_Actor800100AimBeamScratch, 0x5C);

/// Scratch-stack block for ranging the actor against the point it is steering toward.
///
/// Holds the point in room space and its displacement from the model's root
/// coordinate, whose X and Z give the planar distance the state handlers
/// compare with their reach limits. The point is also what the body-turn and
/// aim-turn helpers are handed. The block lives only for the one call.
typedef struct {
    VECTOR3 targetPoint; // Lock position of the actor's target node, or the player's root translation when it has none
    byte    field_C[4];  // Never accessed; role unproven
    VECTOR3 targetDelta; // `targetPoint` minus the root coordinate's translation
    byte    field_1C[4]; // Never accessed; role unproven
} _Actor800100TargetScratch;
STATIC_ASSERT_SIZEOF(_Actor800100TargetScratch, 0x20);

/// Scratch-stack block of the aim beam's line.
///
/// The line runs along the Y axis of the beam's coordinate, from its origin to
/// `tip`. Both ends go through that coordinate's composed matrix, one
/// perspective transform each, and the screen points become the ends of an
/// additive gouraud line. The line is drawn only when `otz` is at least 0x20.
typedef struct {
    DVECTOR originScreen; // Screen X/Y of `origin`
    DVECTOR tipScreen;    // Screen X/Y of `tip`
    s32     otz;          // SZ3 / 4 of the tip's transform; ordering-table depth and the blend packet's depth
    SVECTOR origin;       // Near end in the coordinate's frame, always (0, 0, 0); `pad` is never written
    SVECTOR tip;          // Far end: (0, length, 0), the contact distance or the equipped weapon's reach when that is 0
} _Actor800100AimBeamLineScratch;
STATIC_ASSERT_SIZEOF(_Actor800100AimBeamLineScratch, 0x1C);

/// One corner of the aim beam's end square, as an offset from the
/// translation of the beam's coordinate. The X offset is always 0, so a
/// corner is stored as its other two components.
typedef struct {
    s16 vy; // Y offset
    s16 vz; // Z offset
} _Actor800100AimBeamQuadCorner;
STATIC_ASSERT_SIZEOF(_Actor800100AimBeamQuadCorner, 4);

/// Scratch-stack block of the aim beam's end square.
///
/// Each corner is a `_Actor800100AimBeamQuadCorner` added to the translation
/// of the beam coordinate's composed matrix; the coordinate's rotation is not
/// applied, so the square keeps one orientation. The corners are projected
/// through `GsWSMATRIX`, corner 0 with one perspective transform and corners
/// 1..3 with a three-vertex one, onto an additive textured quad. Corners and
/// screen points share indices 0..3 in GPU quad strip order.
typedef struct {
    DVECTOR screenCorners[4]; // Screen X/Y of each corner
    s32     otz;              // SZ3 / 4 of the three-vertex transform, corner 3's depth; selects the ordering-table bucket
    VECTOR  cornerOffset;     // Corner being staged, as (0, vy, vz); `pad` is never written
    SVECTOR worldCorners[4];  // Corner positions handed to the projection, cut to 16 bits; `pad` is never written
} _Actor800100AimBeamQuadScratch;
STATIC_ASSERT_SIZEOF(_Actor800100AimBeamQuadScratch, 0x44);

/// NULL-terminated `GpuImageUpload*` frame lists for `func_actor_800100_80163A58`,
/// indexed `table[textureSequenceA - 1][textureFrameA]`; `D_actor_800100_80167210` is
/// the `textureSequenceB` sequence.
extern GpuImageUpload** D_actor_800100_80167200[];
extern GpuImageUpload** D_actor_800100_80167210[];

/// Translation the flare's own coordinate starts at, `(0, 0x200, 0x40)`.
extern SVECTOR D_actor_800100_80167128;

extern TaskMessageEntry D_actor_800100_80167130[26];
extern s16              D_actor_800100_80167218[];
extern s16              D_actor_800100_80167224[];
extern u8               D_actor_800100_80167230[];

static void func_actor_800100_80163214(Task* arg0);
static void func_actor_800100_80163C04(Task* arg0);
static void func_actor_800100_80163D54(Task* arg0);
static void func_actor_800100_80163F04(Task* arg0);
static void func_actor_800100_80164184(Task* arg0);
static void func_actor_800100_801643F4(Task* arg0);
static void func_actor_800100_80164580(Task* arg0);
static void func_actor_800100_80164710(Task* arg0);
static void func_actor_800100_80164940(Task* arg0);
static void func_actor_800100_80164B9C(Task* arg0);
static void func_actor_800100_80164E60(Task* arg0);
static void func_actor_800100_80165010(Task* arg0);
static void func_actor_800100_801652B0(Task* arg0);
static void func_actor_800100_801655C0(Task* arg0);
static void func_actor_800100_80165630(Task* arg0);
static void func_actor_800100_80165664(Task* arg0);
static void func_actor_800100_801656C8(Task* arg0);
static void func_actor_800100_801656F4(Task* arg0);
static void func_actor_800100_80165720(Task* arg0);
static void func_actor_800100_80165748(Task* arg0);
static void func_actor_800100_801657D8(Task* arg0);
static void func_actor_800100_80165818(Task* arg0);
static void func_actor_800100_80165850(Task* arg0);
static void func_actor_800100_801658E8(Task* arg0);
static void func_actor_800100_80165928(Task* arg0);
static void func_actor_800100_80165930(Task* arg0);
static void func_actor_800100_801659EC(Task* arg0);
static void func_actor_800100_80165C38(Task* arg0);
static void func_actor_800100_80165DE8(Task* arg0);
static void func_actor_800100_80165F50(Task* arg0);
static void func_actor_800100_80166190(Task* arg0);
static void func_actor_800100_8016666C(GfxCoord* arg0, s16 arg1);
static void func_actor_800100_801668C0(GfxCoord* arg0);
static s32  func_actor_800100_80166B40(WorldCollisionContact* arg0, GfxCoord* arg1, GfxCoord* arg2);
static void func_actor_800100_80166DD0(Task* arg0);
static void func_actor_800100_80166DF0(Task* arg0);
static void func_actor_800100_80166E14(Task* arg0);
void        func_actor_800100_80166E94(Task* arg0, s32 arg1);
static void func_actor_800100_80166EE8(Task* arg0);
static s32  _actor800100GetContactDistance(GfxCoord* coord, WorldCollisionContact* contact, u16* contactZY);

extern u8* D_actor_800100_801672F8[];
extern u8  D_actor_800100_80167308[];
extern u8  D_actor_800100_80167310[];

extern GpuImageUpload* D_actor_800100_80167A18[2];
extern GpuImageUpload* D_actor_800100_80167A20[4];
extern GpuImageUpload* D_actor_800100_80167A30[4];
extern GpuImageUpload* D_actor_800100_80167A40[6];
extern GpuImageUpload* D_actor_800100_80167A58[2];
extern GpuImageUpload* D_actor_800100_80167A60[2];

SVECTOR D_actor_800100_80167128 = { 0, 512, 64, 0 };

TaskMessageEntry D_actor_800100_80167130[26] = {
    { ANIMATION_MESSAGE_PLAY, func_8010C4F0 },
    { 1002, func_8010C4F0 },
    { 1003, func_8010C4F0 },
    { 1004, func_8010C4F0 },
    { GAME_ACTOR_MESSAGE_PLACE, func_80104D68 },
    { ANIMATION_MESSAGE_IS_PLAYING, func_8010583C },
    { GAME_ACTOR_MESSAGE_TURN_TO_YAW, func_8010C688 },
    { GAME_ACTOR_MESSAGE_CLIMB_STAIRS, func_8010C4F0 },
    { GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, func_8010C4F0 },
    { GAME_ACTOR_MESSAGE_END_SCRIPTED, func_8010C30C },
    { GAME_ACTOR_MESSAGE_MOVE_TO, func_8010C6C8 },
    { GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, func_80104684 },
    { ANIMATION_MESSAGE_INSTALL_AND_PLAY, func_8010C648 },
    { GAME_ACTOR_MESSAGE_ATTACH_TO_COORD, func_80105A60 },
    { GAME_ACTOR_MESSAGE_WALK_STEPS, func_801052B8 },
    { ANIMATION_MESSAGE_COPY_BANK_EXTENSION, Gp_CopyAllyAnim },
    { GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, func_8010C75C },
    { GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_HurtAlly },
    { 1018, func_8010C4F0 },
    { 1019, func_8010C4F0 },
    { 1020, func_8010C4F0 },
    { ANIMATION_MESSAGE_SET_RATE, func_801058BC },
    { GAME_ACTOR_MESSAGE_MOVE_BY, Gp_MoveActorByKeep },
    { ANIMATION_MESSAGE_REPLACE_AND_PLAY, func_8010C30C },
    { 1024, func_8010C30C },
    { GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE, func_80105AB0 },
};

GpuImageUpload** D_actor_800100_80167200[4] = {
    D_actor_800100_80167A18,
    D_actor_800100_80167A20,
    D_actor_800100_80167A30,
    D_actor_800100_80167A40,
};

GpuImageUpload** D_actor_800100_80167210[2] = {
    D_actor_800100_80167A60,
    D_actor_800100_80167A58,
};

s16 D_actor_800100_80167218[6] = {
    0,
    5,
    12,
    3,
    28,
    0,
};

s16 D_actor_800100_80167224[6] = {
    0,
    3,
    0,
    3,
    16,
    0,
};

u8 D_actor_800100_80167230[8] = {
    12,
    12,
    12,
    100,
    40,
    0,
    0,
    0,
};

u8 D_actor_800100_80167238[48] = {
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

u8 D_actor_800100_80167268[48] = {
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

u8 D_actor_800100_80167298[48] = {
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
};

u8 D_actor_800100_801672C8[48] = {
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    1,
    1,
    1,
    1,
    1,
    1,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    3,
    3,
    3,
    3,
    3,
    3,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
};

u8* D_actor_800100_801672F8[4] = {
    D_actor_800100_80167238,
    D_actor_800100_80167268,
    D_actor_800100_80167298,
    D_actor_800100_801672C8,
};

u8 D_actor_800100_80167308[8] = {
    0,
    0,
    0,
    0,
    3,
    4,
    4,
    4,
};

u8 D_actor_800100_80167310[8] = {
    1,
    1,
    2,
    2,
    3,
    3,
    4,
    4,
};

u_long D_actor_800100_80167318[84] = {
    0xECE5B2AF,
    0xF2F0F5F5,
    0xF6F0F2F2,
    0xF2F0F5F7,
    0x4F8BECC8,
    0x89C47356,
    0xEF4F4F56,
    0xF5F0F25F,
    0xF2F0F5F5,
    0xF2F2F2F2,
    0xD6AF5DF2,
    0x676593E2,
    0x5A5A5A9A,
    0x5050588D,
    0xF0F25D60,
    0x8956DCB5,
    0x5673ADA4,
    0xF4F4C8E8,
    0x615050ED,
    0x8D616161,
    0x588D7551,
    0x7E7DBFD5,
    0x5F609A6B,
    0x5F5F5F5F,
    0x61615060,
    0x65505861,
    0xA0A086B2,
    0x50B6E5B0,
    0x5F506161,
    0x5D5D5D74,
    0x7D65605F,
    0xA0857E6B,
    0xF1657E7E,
    0xFFFFFFFF,
    0xFBFFFFFF,
    0x8D8D50F0,
    0x7E7E915A,
    0x7D7EA0A4,
    0x5F61618D,
    0xFFFFFAF8,
    0xFBFFFFFF,
    0xA07E60FA,
    0x9AA086C1,
    0x73305F9,
    0xDFFFFFC,
    0x74E1E704,
    0x7D8F7661,
    0x85A9867E,
    0x616191A4,
    0xCBE7F050,
    0x90DFFFF,
    0xE1EF33DB,
    0xAAC56B9A,
    0xF35F7D81,
    0x14371701,
    0xF30409D7,
    0x925ADAC7,
    0xADA47E68,
    0x6BAEB1C5,
    0xB6608D76,
    0xFF1304C7,
    0x100E070C,
    0xA1BFE4EA,
    0x8488B189,
    0xE9E9BFA0,
    0x40404EC,
    0xD2DCC7EC,
    0x86A681A4,
    0x8A8AB3D4,
    0x7997A6B3,
    0xEFDC9A5A,
    0xE1EC0C0E,
    0xB1D6EAEF,
    0x8973C4AE,
    0xAAAAAEB1,
    0xE8D5B2AD,
    0xBFD2E8E8,
    0xC2A8A4D2,
    0x8AAEC2C2,
    0xA7895656,
    0x7E9DA180,
    0xE4D2BFD2,
    0xC5D6D2D2,
    0xB1AEB1AE,
};

GpuImageUpload D_actor_800100_80167468[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 21, 8 }, D_actor_800100_80167318 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800100_80167488[84] = {
    0xECE5B2AF,
    0xF2F0F5F5,
    0xF6F0F2F2,
    0xF2F0F5F7,
    0x4F8BECC8,
    0x89C47356,
    0xEF4F4F56,
    0xF5F0F25F,
    0xF2F0F5F5,
    0xF2F2F2F2,
    0xD6AF5DF2,
    0x676593E2,
    0x5A5A5A9A,
    0x5050588D,
    0xF0F25D60,
    0x8956DCB5,
    0x5673ADA4,
    0xF4F4C8E8,
    0x615050ED,
    0x8D616161,
    0x588D7551,
    0x7E7DBFD5,
    0x91919191,
    0x67676767,
    0x61616161,
    0x65505861,
    0xA0A086B2,
    0x50B6E5B0,
    0x61616161,
    0x67676761,
    0x91919167,
    0xA0857E7E,
    0x677E7E7E,
    0x55F6565,
    0x5F050505,
    0x61616165,
    0x7E7E915A,
    0x7D7EA0A4,
    0x6161618D,
    0xFFFF5F65,
    0x655FFFFF,
    0xA07E6765,
    0x9AA086C1,
    0x50505F9,
    0x5050505,
    0x5050505,
    0x7D767650,
    0x85A9867E,
    0x616191A4,
    0x505F050,
    0x5050505,
    0x5050505,
    0xAAC56B9A,
    0xF35F7D81,
    0x14370417,
    0xF30409D7,
    0x925ADAC7,
    0xADA47E68,
    0x6BAEB1C5,
    0xB68D8D76,
    0xFF1304C7,
    0x170E070C,
    0xA17D5FF3,
    0x8488B189,
    0xF37DBFA0,
    0x4040404,
    0xD2DCF3F3,
    0x86A681A4,
    0x8A8AB3D4,
    0x7997A6B3,
    0xF3F39A5A,
    0x4040404,
    0xBFBF9AF3,
    0x8973C4AE,
    0xAAAAAEB1,
    0xD2D2D2AD,
    0xD2D2D2D2,
    0xC2A8A4D2,
    0x8AAEC2C2,
    0xA7895656,
    0x7E9DA180,
    0xE4D2BFD2,
    0xC5D6D2D2,
    0xB1AEB1AE,
};

GpuImageUpload D_actor_800100_801675D8[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 21, 8 }, D_actor_800100_80167488 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800100_801675F8[84] = {
    0xECE5B2AF,
    0xF2F0F5F5,
    0xF6F0F2F2,
    0xF2F0F5F7,
    0x4F8BECC8,
    0x89C47356,
    0xEF4F4F56,
    0xF5F0F25F,
    0xF2F0F5F5,
    0xF2F2F2F2,
    0xD6AF5DF2,
    0x676593E2,
    0x5A5A5A9A,
    0x5050588D,
    0xF0F25D60,
    0x8956DCB5,
    0x5673ADA4,
    0xF4F4C8E8,
    0x615050ED,
    0x51516161,
    0x588D7551,
    0x7E7DBFD5,
    0x7E7E7E7E,
    0x91919191,
    0x61616191,
    0x65505861,
    0xA0A086B2,
    0x50B6E5B0,
    0x61616161,
    0x7E916761,
    0x7E7E7E7E,
    0xA0857E7E,
    0x7E7E7E7E,
    0x9A919191,
    0x61616161,
    0x61616161,
    0x7E7E915A,
    0x7D7EA0A4,
    0x6161618D,
    0x61616161,
    0x91676161,
    0xA07E7E91,
    0xB67D86C1,
    0x7E7E91B6,
    0x7E7E7E7E,
    0x8D617691,
    0x7D76768D,
    0x85A9867E,
    0x616191A4,
    0x91766161,
    0x7E7E7E7E,
    0x91917E7E,
    0xAAC57D91,
    0x5B67D81,
    0x5D050505,
    0x55D5D5D,
    0x925AB605,
    0xADA47E68,
    0x6BAEB1C5,
    0xB68D8D76,
    0x5D5D0505,
    0x5055D5D,
    0xA1B60505,
    0x8488B189,
    0x5B6BFA0,
    0x5050505,
    0xD2B60505,
    0x86A681A4,
    0x8A8AB3D4,
    0x7997A6B3,
    0x5B69A5A,
    0x5050505,
    0xBFB60505,
    0x8973C4AE,
    0xAAAAAEB1,
    0x5A5AD2AD,
    0x5A5A5A5A,
    0xC2A8A4D2,
    0x8AAEC2C2,
    0xA7895656,
    0x7E9DA180,
    0x5A5A5AD2,
    0xBF5A5A5A,
    0xB1AEAEBF,
};

GpuImageUpload D_actor_800100_80167748[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 21, 8 }, D_actor_800100_801675F8 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800100_80167768[78] = {
    0xD3D3C2C2,
    0xD3D3D3C2,
    0xB3B1AEC4,
    0xB389B3B3,
    0x89898989,
    0xAEC5B1B3,
    0x8787AEAE,
    0xC2C287C2,
    0xAEABC2C2,
    0x8989B3B1,
    0x8989B3B3,
    0xB1B38989,
    0xABABC4AE,
    0x87A7A7A7,
    0xAB878787,
    0xA9A9C4C4,
    0xB3B3B1C4,
    0xC4C3A9C3,
    0xC4AEC5C5,
    0x8383C2AB,
    0x8887A7A7,
    0x686CAAC4,
    0x64625962,
    0x62626666,
    0x6C7B6464,
    0xABABC284,
    0xA883BCBC,
    0x597B85AA,
    0x52525151,
    0x92646363,
    0x63926868,
    0x79635952,
    0xBCBCC281,
    0xA0A5A7A7,
    0x86A07E68,
    0x8A89C6B0,
    0x8A8A8A8A,
    0xA9ADB289,
    0xA7A58686,
    0xB8BC83BC,
    0xB3B0A5CC,
    0x8A8A8A89,
    0x8A8A8A8A,
    0x8A8A8A8A,
    0x88B1898A,
    0xBCBCA2A2,
    0xBABAA2BC,
    0xA98686A5,
    0xADADADD4,
    0x8686A9D4,
    0xA4868586,
    0xBA7F9DA0,
    0xA2BCBCA2,
    0x9D7F7FBA,
    0xA06B6BA0,
    0xA0A4A4A0,
    0x7E7E6BA0,
    0x7F9D7E68,
    0xA26D6D7F,
    0x7F7FA2A2,
    0xA68180A1,
    0xABAAA885,
    0xAAACACAB,
    0x818185A9,
    0x6D7FA182,
    0xA26D6D6D,
    0x83A27FBA,
    0xAEAB8887,
    0xB17272AE,
    0xAEB1B1B1,
    0x83A78788,
    0x6D6DA2A2,
    0xA2A26D6D,
    0xC4888783,
    0x89B3B1AE,
    0x89898989,
    0x88C4B189,
    0xA283A787,
};

GpuImageUpload D_actor_800100_801678A0[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 13, 12 }, D_actor_800100_80167768 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800100_801678C0[78] = {
    0xD3D3C2C2,
    0xD3D3D3C2,
    0xB3B1AEC4,
    0xB389B3B3,
    0x89898989,
    0xAEC5B1B3,
    0x8787AEAE,
    0xC2C287C2,
    0xAEABC2C2,
    0x8989B3B1,
    0x8989B3B3,
    0xB1B38989,
    0xABABC4AE,
    0x87A7A7A7,
    0xAB878787,
    0x5179C4C4,
    0x51515151,
    0xC5685951,
    0xABAEC5C5,
    0x8383C2AB,
    0x8887A7A7,
    0x686CAAC4,
    0x50505050,
    0x50505050,
    0x6C595150,
    0xABABABAB,
    0xA883BCBC,
    0x7B8787AA,
    0x332B5051,
    0x1010135,
    0x502B3335,
    0xABAB5252,
    0xBCBCC2AB,
    0xBAA5A7A7,
    0xF05052A1,
    0xF0F0F0F0,
    0xF0F0F0F0,
    0x50F0F0F0,
    0xA7A5A152,
    0xB8BC83BC,
    0x52A1BABA,
    0xF0F0F0F0,
    0xF0F0F0F0,
    0xF0F0F0F0,
    0xA152F0F0,
    0xBCBCA2A2,
    0xBABAA2BC,
    0xF05052A1,
    0xF0F0F0F0,
    0xF0F0F0F0,
    0x50F0F0F0,
    0xBA7FA152,
    0xA2BCBCA2,
    0x7FA17FBA,
    0xF0F0F052,
    0xF0F0F0F0,
    0xF0F0F0F0,
    0xA17F52F0,
    0xA26D6D7F,
    0x7F7FA2A2,
    0x85A77F7F,
    0x89898888,
    0x88898989,
    0xA7A78588,
    0x6D7FA1A7,
    0xA26D6D6D,
    0x837F7FBA,
    0x7B6A6A83,
    0x7B7B7B7B,
    0x6A7B7B7B,
    0x83A7836A,
    0x6D6DA2A2,
    0xA2A26D6D,
    0x88888783,
    0x88888888,
    0x88888888,
    0x88888888,
    0xA283A787,
};

GpuImageUpload D_actor_800100_801679F8[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 13, 12 }, D_actor_800100_801678C0 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

GpuImageUpload* D_actor_800100_80167A18[2] = {
    D_actor_800100_80167468,
    NULL,
};

GpuImageUpload* D_actor_800100_80167A20[4] = {
    D_actor_800100_80167468,
    D_actor_800100_801675D8,
    D_actor_800100_80167748,
    NULL,
};

GpuImageUpload* D_actor_800100_80167A30[4] = {
    D_actor_800100_80167748,
    D_actor_800100_801675D8,
    D_actor_800100_80167468,
    NULL,
};

GpuImageUpload* D_actor_800100_80167A40[6] = {
    D_actor_800100_80167468,
    D_actor_800100_801675D8,
    D_actor_800100_80167748,
    D_actor_800100_801675D8,
    D_actor_800100_80167468,
    NULL,
};

GpuImageUpload* D_actor_800100_80167A58[2] = {
    D_actor_800100_801678A0,
    NULL,
};

GpuImageUpload* D_actor_800100_80167A60[2] = {
    D_actor_800100_801679F8,
    NULL,
};

static void func_actor_800100_80163BF8(Task* arg0);
static void func_actor_800100_80166514(Task* arg0);
static void func_actor_800100_80166F50(Task* arg0);

/// Per-frame flare task of the actor: while the player model is visible
/// (`field_C & 0x80` clear) and effects are visible
/// (`gRoomEffectState->effectControl < 2`) it refreshes transient point-light slot 3.
/// State 0 hangs the flare's coordinate off the actor's own at the fixed
/// offset and zeroes its `age`; state 1 then dispatches on `spawnArg1`:
///
/// - 1 draws the flare at the coordinate's `workm.t` every frame and varies
///   the light's red intensity randomly in `0x400..0xB00`, arming the flare width in
///   `scale`.
/// - 2 widens that flare by 0x40 a frame up to 0x180, spawns effect `0x60181`
///   as a child of this task, and refreshes the light with a much wider
///   (`0x400` / `0x4000`) falloff and red intensity in `0x800..0xF00`.
/// - 3 and 4 switch back to sub-state 1 and 0, and 5 releases the pool block.
///
/// While `gRoomEffectState->effectControl` is non-zero the two drawing sub-states wind
/// `age` back down instead of advancing.
void func_actor_800100_80161F20(Task* task)
{
    EffectWork*                    work;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    GfxCoord*                      light;
    GfxRotationWords*              rot;
    EffectWork*                    eff;
    u32                            ang;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    lightSlot = &gWorldCoordTransientPointLights[3];
    light     = &lightSlot->light.head.transform.coord;
    slot      = &lightSlot->light;
    if ((gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) != 0) {
        return;
    }
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            rot                 = (GfxRotationWords*)&coord->coord;
            coord->parent       = work->parent;
            rot->m00M01         = ONE;
            rot->m02M10         = 0;
            rot->m11M12         = ONE;
            rot->m20M21         = 0;
            rot->m22            = ONE;
            coord->coord.t[0]   = D_actor_800100_80167128.vx;
            coord->coord.t[1]   = D_actor_800100_80167128.vy;
            coord->coord.t[2]   = D_actor_800100_80167128.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state = 1;
            break;
        case 1:
            actorRenderComposeCoord(coord);
            switch (task->spawnArg1.value) {
                case 0:
                    break;
                case 1:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        work->age--;
                        _pykeFlameDrawNozzle(
                            MATRIX_TRANS(&coord->workm), work->age, PYKE_FLAME_NOZZLE_SIZE_SCALE);
                        break;
                    }
                    _pykeFlameDrawNozzle(
                        MATRIX_TRANS(&coord->workm), work->age, PYKE_FLAME_NOZZLE_SIZE_SCALE);
                    lightSlot->framesLeft = 4;
                    slot->inner           = 0x80;
                    slot->outer           = 0x400;
                    ang                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState       = ang;
                    slot->head.color.r    = ((ang >> 16) & 0x700) + 0x400;
                    slot->head.color.g    = (u16)slot->head.color.r >> 1;
                    slot->head.color.b    = slot->head.color.r >> 2;
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                    light->composeStamp = GRAPHICS_COORD_DIRTY;
                    work->scale         = 0x40;
                    break;
                case 2:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        work->age--;
                        break;
                    }
                    if (work->scale < 0x180) {
                        work->scale = work->scale + 0x40;
                    }
                    eff = Gp_SpawnEff(EFFECT_ACTOR_800100_PYKE_FLAME, coord, (s32)(work->scale), NULL);
                    if (eff != NULL) {
                        taskReparent(task, eff->task);
                    }
                    lightSlot->framesLeft = 4;
                    slot->inner           = 0x400;
                    slot->outer           = 0x4000;
                    ang                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState       = ang;
                    slot->head.color.r    = ((ang >> 16) & 0x700) + 0x800;
                    slot->head.color.g    = (u16)slot->head.color.r >> 1;
                    slot->head.color.b    = slot->head.color.r >> 2;
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                    light->composeStamp = GRAPHICS_COORD_DIRTY;
                    break;
                case 3:
                    task->spawnArg1.value = 1;
                    break;
                case 4:
                    task->spawnArg1.value = 0;
                    break;
                case 5:
                    effectKillTask(work, task);
                    break;
            }
            break;
    }
}

#include "../../shared/pyke_flame_nozzle.inc.c"

#define PYKE_FLAME_KEY                  0x21C9E
#define PYKE_FLAME_REDRAW_UPDATES_COORD 1
#include "../../shared/pyke_flame_task.inc.c"

/// Per-frame task for one flame actor_800100's Pyke throws (see pyke_flame.h).
void func_actor_800100_801624F0(Task* task)
{
    _pykeFlameTask(task);
}

#include "../../shared/pyke_flame_blob.inc.c"

#include "../../shared/pyke_flame_splash.inc.c"

#include "../../shared/pyke_flame_release.inc.c"

static void func_actor_800100_80163214(Task* arg0)
{
    GameActor*             actor;
    TmdObject*             extra;
    GfxCoord*              coord;
    GfxCoord*              next;
    GfxCoord*              third;
    McSaveData*            save;
    WorldCollisionContact* recs;
    WorldCollisionBody*    obj;
    CompanionWork*         companion;
    EffectWork*            eff;
    Task*                  task;
    SVECTOR3*              scratch;
    s32                    idx;
    s32                    packed;
    u8                     savedResourceVariant;
    void*                  head;

    actor                      = arg0->work;
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = head - 8;
    scratch                    = (SVECTOR3*)(head - 8);
    extra                      = arg0->extra.tmd;
    coord                      = extra->coords;
    arg0->state++;
    arg0->msgTable                                 = D_actor_800100_80167130;
    arg0->exitCallback                             = func_actor_800100_80163C04;
    actor->animationSlotCount                      = GAME_ACTOR_ARMED_COMPANION_ANIMATION_SLOTS;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] = arg0;
    coord->parent                                  = &gGfxViewCoord;
    coord->composeStamp                            = GRAPHICS_COORD_DIRTY;
    extra->flags                                   = 0;
    RotMatrix(&actor->rotation, &coord->coord);
    func_8010BFCC(arg0);
    actor->animationRate = ANIMATION_RATE_ONE;
    Gp_AnimResetChildSlots(arg0, actor->actionArgument);
    Gp_AnimTickChildSlots(arg0);
    recs                                       = actor->collisionContacts;
    obj                                        = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    actor->previousPosition.vx                 = coord->coord.t[0];
    actor->previousPosition.vy                 = coord->coord.t[1];
    actor->previousPosition.vz                 = coord->coord.t[2];
    obj->context.motion                        = &actor->collisionMotionContexts[0];
    obj->coord                                 = coord;
    actor->collisionMotionContexts[0].contacts = recs;
    save                                       = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    obj->pos.vy                                = -0x12C;
    obj->pos.vx                                = 0;
    obj->pos.vz                                = 0;
    packed                                     = 0x10000;
    {
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0x12C;
        obj->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        obj->key    = temp | packed | 0x80;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_BODIES, obj);
    }
    worldCollisionInitContacts(actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), 0);
    obj->flags                                |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    next                                       = arg0->extra.tmd->coords + 4;
    obj                                        = &actor->collisionBodies[GAME_ACTOR_BODY_PART4];
    obj->context.motion                        = &actor->collisionMotionContexts[1];
    obj->coord                                 = next;
    actor->collisionMotionContexts[1].contacts = recs;
    obj->pos.vx                                = 0;
    obj->pos.vy                                = 0x64;
    obj->pos.vz                                = 0;
    {
        s32 f = 0x14;
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0xDC;
        obj->flags  = f;
        obj->key    = temp | packed | 0x80;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_BODIES, obj);
    }
    obj->flags                                |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    obj                                        = &actor->collisionBodies[GAME_ACTOR_BODY_PART1];
    third                                      = arg0->extra.tmd->coords;
    obj->context.motion                        = &actor->collisionMotionContexts[2];
    obj->coord                                 = third + 1;
    actor->collisionMotionContexts[2].contacts = recs;
    obj->pos.vx                                = 0;
    obj->pos.vy                                = 0x52;
    obj->pos.vz                                = 0;
    {
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0xDC;
        obj->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        obj->key    = temp | packed | 0x80;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_BODIES, obj);
    }
    obj->flags                   |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    actor->collisionEnableMask    = GAME_ACTOR_COLLISION_REQUEST_MASK;
    savedResourceVariant          = gPlayerStatus.resourceVariant;
    gPlayerStatus.resourceVariant = save->state.companionVariant;
    actor->attachmentTasks[0]     = func_80104258(arg0, 0, 5, 1);
    actor->attachmentTasks[1]     = func_80104258(arg0, 1, 5, 1);
    gPlayerStatus.resourceVariant = savedResourceVariant;
    if (actor->attachmentTasks[1] != NULL) {
        task                     = func_80104364(actor->attachmentTasks[1], save->state.companionType + 1, save->state.companionVariant, 0);
        actor->equipmentTasks[1] = task;
        if (task != NULL) {
            companion = actor->companionWork;
            idx       = D_actor_800100_80167218[save->state.companionVariant];
            Gp_AttachActorObj(arg0, idx, D_actor_800100_80167224[save->state.companionVariant]);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key |= 0x80;
            companion->activity.combat.attacksRemaining         = D_actor_800100_80167230[save->state.companionVariant];
            if ((u8)save->state.companionVariant == 4) {
                eff = Gp_SpawnEff((EFFECT_COMPANION_WEAPON_FLARE | EFFECT_SPAWN_UNLIMITED), actor->equipmentTasks[1]->extra.tmd->coords, idx, 0);
                if (eff != NULL) {
                    actor->weaponEffectTask = eff->task;
                    taskReparent(arg0, eff->task);
                    func_80106350(arg0, idx, 0);
                }
            }
        }
    }
    scratch->vx = 0;
    scratch->vy = -0x200;
    scratch->vz = 0;
    Gp_BindActorD4(arg0, scratch, 0x1000);
    companionSetDecisionDelay(arg0, 0x3C, 0x7F);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void func_actor_800100_801635F4(Task* arg0)
{
    CompanionMoveScratch* scratch;
    void**                scratchHead;
    CompanionMoveScratch* head;
    GameActor*            actor;
    TmdObject*            work;
    TmdObject*            extra;
    GfxCoord*             coord;
    GfxCoord*             ground;
    CompanionWork*        companion;
    Task*                 task;
    WorldCollisionBody*   objs[2];
    s32                   dy;
    s32                   i;
    s8                    bits;

    scratchHead                                        = SCRATCH_HEAD_ADDR;
    head                                               = SCRATCH_HEAD_AT(scratchHead, CompanionMoveScratch);
    extra                                              = arg0->extra.tmd;
    SCRATCH_HEAD_AT(scratchHead, CompanionMoveScratch) = head - 1;
    work                                               = extra;
    scratch                                            = head - 1;
    coord                                              = work->coords;
    actor                                              = arg0->work;
    companion                                          = actor->companionWork;

    if (actor->mode != GAME_ACTOR_MODE_SCRIPTED &&
        (dy = coord->coord.t[1], dy = dy - actor->previousPosition.vy, dy = ABS(dy), dy >= 0x200)) {
        coord->coord.t[0] = actor->previousPosition.vx;
        coord->coord.t[1] = actor->previousPosition.vy;
        coord->coord.t[2] = actor->previousPosition.vz;
    } else {
        actor->previousPosition.vx = coord->coord.t[0];
        actor->previousPosition.vy = coord->coord.t[1];
        actor->previousPosition.vz = coord->coord.t[2];
        if (actor->collisionEnableMask & 1) {
            actor->gridResponse = func_801011D0(coord, actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), &actor->surfaceClass);
        } else {
            actor->gridResponse = 0;
        }
    }

    task = actor->equipmentTasks[1];
    if (task != NULL) {
        actor->weaponCollisionCoord = *task->extra.tmd->coords;
        gfxRotMatrixX(&actor->weaponCollisionCoord.workm, -0x400, GRAPHICS_ROTATION_COMPOSE);
    }

    companion->probe.coord = *arg0->extra.tmd->coords;
    gfxRotMatrixY(&companion->probe.coord.workm, companion->scanAngle, 0);

    objs[0] = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    objs[1] = &actor->collisionBodies[GAME_ACTOR_BODY_PART1];
    for (i = 0; i < 2; i++) {
        bits = actor->pendingCollisionUpdates;
        if ((bits >> i) & 1) {
            actor->collisionEnableMask |= 1 << i;
            objs[i]->flags             |= WORLD_COLLISION_BODY_GRID_ENABLED;
        } else if (bits & (8 << i)) {
            actor->collisionEnableMask &= ~(1 << i);
            objs[i]->flags             &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    actor->pendingCollisionUpdates = 0;

    if (D_80115768 == 0 && gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        func_actor_800100_80165528(arg0);
    }
    func_actor_800100_80163A58(arg0);

    worldCollisionClearContacts(actor->collisionContacts);
    worldCollisionClearContacts(actor->companionWork->probe.contacts);
    if (actor->equipmentTasks[1] != NULL) {
        worldCollisionClearContacts(actor->weaponContacts);
    }
    if (actor->collisionEnableMask & 1) {
        coord->coord.t[1] += 8;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);

    if ((s8)actor->usesPushbackDirection != 0) {
        scratch->motionDirection.vx = actor->pushbackDirection.vx;
        scratch->motionDirection.vy = actor->pushbackDirection.vy;
        scratch->motionDirection.vz = actor->pushbackDirection.vz;
    } else {
        scratch->motionDirection.vx = (u16)coord->workm.m[0][2] * (s8) * (volatile u8*)&actor->movementSign;
        scratch->motionDirection.vy = (u16)coord->workm.m[1][2] * (s8) * (volatile u8*)&actor->movementSign;
        scratch->motionDirection.vz = (u16)coord->workm.m[2][2] * (s8) * (volatile u8*)&actor->movementSign;
    }
    actor->collisionMotionContexts[0].motionDirection.vx = scratch->motionDirection.vx;
    actor->collisionMotionContexts[0].motionDirection.vy = scratch->motionDirection.vy;
    actor->collisionMotionContexts[0].motionDirection.vz = scratch->motionDirection.vz;
    actor->collisionMotionContexts[1].motionDirection.vx = scratch->motionDirection.vx;
    actor->collisionMotionContexts[1].motionDirection.vy = scratch->motionDirection.vy;
    actor->collisionMotionContexts[1].motionDirection.vz = scratch->motionDirection.vz;
    actor->collisionMotionContexts[2].motionDirection.vx = scratch->motionDirection.vx;
    actor->collisionMotionContexts[2].motionDirection.vy = scratch->motionDirection.vy;
    actor->collisionMotionContexts[2].motionDirection.vz = scratch->motionDirection.vz;

    if (!(work->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        ground               = arg0->extra.tmd->coords + 1;
        ground->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(ground);
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&ground->workm), &scratch->shadowCentre) != 0) {
            effectDrawGroundShadow(&scratch->shadowCentre, 0x200, gRoomEffectState->groundShadowShade);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(CompanionMoveScratch);
}

/// Texture-upload state of the actor: runs two independent sequences, each a
/// countdown (`textureDelayA` / `textureDelayB`, reloaded with 4 / 8) that advances a
/// frame index (`textureFrameA` / `textureFrameB`) through the NULL-terminated image
/// list `D_actor_800100_80167200` / `D_actor_800100_80167210` named by the
/// sequence number (`textureSequenceA` / `textureSequenceB`), ending the sequence when its
/// list runs out. Every upload posts its image over an 8-byte `RECT` borrowed
/// from the scratch stack and gives it back at the end of the call.
static void func_actor_800100_80163A58(Task* arg0)
{
    void**            scratch;
    u8*               head;
    u8*               temp;
    RECT*             rect;
    GameActor*        actor;
    GpuImageUpload*** frameLists;
    s32               idx;
    u32               row;
    GpuImageUpload*   uploadList;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    actor                          = arg0->work;
    temp                           = head - 8;
    SCRATCH_HEAD_AT(scratch, void) = temp;
    rect                           = (RECT*)temp;

    if ((s8)actor->textureSequenceA != 0) {
        actor->textureDelayA--;
        if ((s8)actor->textureDelayA <= 0) {
            frameLists = D_actor_800100_80167200;
            idx        = (s8)actor->textureSequenceA - 1;
            uploadList = frameLists[idx][(s8)actor->textureFrameA];
            if (uploadList != NULL) {
                ((RECT*)head)[-1].x = 0;
                rect->y             = 0x28;
                rect->w             = 0x15;
                rect->h             = 8;
                actorRenderUploadTexture(arg0, uploadList, rect);
                actor->textureDelayA = 4;
                actor->textureFrameA++;
            } else {
                actor->textureSequenceA = 0;
            }
        }
    }

    if ((s8)actor->textureSequenceB != 0) {
        actor->textureDelayB--;
        if ((s8)actor->textureDelayB <= 0) {
            frameLists = D_actor_800100_80167210;
            idx        = (row = (s8)actor->textureSequenceB - 1);
            uploadList = frameLists[row][(s8)actor->textureFrameB];
            if (uploadList != NULL) {
                rect->x = 8;
                rect->y = 0x40;
                rect->w = 0xD;
                rect->h = 0xC;
                actorRenderUploadTexture(arg0, uploadList, rect);
                actor->textureDelayB = 8;
                actor->textureFrameB++;
            } else {
                actor->textureSequenceB = 0;
            }
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void func_actor_800100_80163BF8(Task* arg0)
{
    arg0->state = 3;
}

static void func_actor_800100_80163C04(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    Task*          task;

    actor                                          = arg0->work;
    companion                                      = actor->companionWork;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] = NULL;
    task                                           = actor->weaponEffectTask;
    if (task != NULL) {
        taskKill(task);
    }
    task = actor->equipmentTasks[0];
    if (task != NULL) {
        taskKill(task);
    }
    task = actor->equipmentTasks[1];
    if (task != NULL) {
        taskKill(task);
    }
    task = actor->attachmentTasks[0];
    if (task != NULL) {
        taskKill(task);
    }
    task = actor->attachmentTasks[1];
    if (task != NULL) {
        taskKill(task);
    }
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_ROOT]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_PART4]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_PART1]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_WEAPON]);
    worldCollisionUnlinkBody(&companion->probe.body);
    taskKill(arg0);
}

/// State handlers of the actor's main task, indexed by its state.
static const TaskFuncTable4 D_actor_800100_80161E3C = { {
    func_actor_800100_80163214,
    func_actor_800100_801635F4,
    func_actor_800100_80163BF8,
    func_actor_800100_80163C04,
} };

/// Per-frame entry point of the actor's main task: runs the handler its state
/// selects from the four-entry state table - set-up, the per-frame update, a
/// step that only advances to the last state, and the teardown that kills the
/// actor's child tasks and unlinks its objects. The table is a local, so it is
/// copied from `.rodata` onto the stack on every call.
void func_actor_800100_80163CF0(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_800100_80161E3C;
    states.funcs[task->state](task);
}

static void func_actor_800100_80163D54(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  target;
    s32        flag;
    s32        dist;
    s32        val;
    s16        count;

    actor  = arg0->work;
    coord  = arg0->extra.tmd->coords;
    target = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    flag   = (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 42, 0, 0);
    if (((GameActor*)arg0->work)->companionWork->decisionTimer <= 0) {
        companionSetDecisionDelay(arg0, 0xA, 0x1F);
        dist = func_8010BC70(coord);
        if ((dist >= 0x600 && (rand() & 0xFF) >= 0xF1) || (dist >= 0x400 && flag != 0)) {
            func_actor_800100_801656C8(arg0);
        } else {
            count = (u16)actor->idleTicks + 1;
            do {
                actor->idleTicks = count;
            } while (0);
            if (count >= ((rand() & 3) + 3)) {
                if (actor->statePhase == 0) {
                    actor->statePhase = 1;
                    playerActorPlayChildSlotsWithBlend(arg0, 0x17, 0, 5);
                } else if ((rand() & 0xFF) >= 0xD0) {
                    func_actor_800100_80165720(arg0);
                }
            } else {
                val = func_8010BCF4(arg0, MATRIX_TRANS(&target->coord));
                if (val < 0) {
                    val = -val;
                }
                if (val >= 0x200) {
                    actor->targetNode = NULL;
                    func_actor_800100_801656F4(arg0);
                }
            }
        }
    }
    func_8010BE5C(arg0, MATRIX_TRANS(&target->coord));
}

/// Handlers `func_actor_800100_80165528` runs, indexed by `mode`.
static const TaskFuncTable3 D_actor_800100_80161E4C = { {
    func_actor_800100_80163F04,
    func_actor_800100_80165850,
    func_actor_800100_80165930,
} };

/// Handlers `func_actor_800100_80163F04` runs, indexed by `state`.
static const TaskFuncTable12 D_actor_800100_80161E58 = { {
    func_actor_800100_80165748,
    func_actor_800100_80164184,
    func_actor_800100_801643F4,
    func_actor_800100_80164580,
    func_actor_800100_801657D8,
    func_actor_800100_80164710,
    func_actor_800100_80164940,
    func_actor_800100_80164B9C,
    func_actor_800100_80165818,
    func_actor_800100_80164E60,
    func_actor_800100_80165010,
    func_actor_800100_801652B0,
} };

/// Drives the actor's `mode`/`state` callback tables while the
/// `effectTimer.waterDripTicks` countdown runs, spawning the drip effect every tenth frame.
/// `sp40` / `sp48` hold the effect position: it rides the water surface
/// (`gGameSession.waterY`) minus the actor coordinate's world Y.
static void func_actor_800100_80163F04(Task* arg0)
{
    TaskFuncTable12 sp;
    SVECTOR         sp40;
    SVECTOR         sp48;
    GameActor*      actor;
    CompanionWork*  companion;
    GfxCoord*       coord;
    s16             temp;
    s16             rem;
    s32             pan;

    sp        = D_actor_800100_80161E58;
    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    companion = actor->companionWork;
    if (companion->decisionTimer > 0) {
        companion->decisionTimer--;
    }
    sp.funcs[actor->state](arg0);
    if ((u32)(func_80105ED4(arg0) + 0xEFFFFF77) < 4) {
        actor->effectTimer.waterDripTicks = 0x78;
        sp40.vx                           = 0;
        sp40.vy                           = (u16)gGameSession->waterY - (u16)coord->coord.t[1];
        sp40.vz                           = 0;
        Gp_SpawnEff(gRoomEffectWaterSprayId, coord, 0x1202180, &sp40);
        Gp_SpawnEff(gRoomEffectWaterRippleId, coord, (rand() & 0x1F) | 0x40, &sp40);
    }
    temp = (u16)actor->effectTimer.waterDripTicks;
    if (temp != 0) {
        actor->effectTimer.waterDripTicks--;
        rem = temp % 10;
        if (rem == 0) {
            sp48.vx = 0;
            sp48.vy = (u16)gGameSession->waterY - (u16)coord->coord.t[1];
            sp48.vz = 0;
            Gp_SpawnEff(gRoomEffectWaterRippleId, coord, (rand() & 0x1F) | 0x40, &sp48);
        }
    }
    if ((s8)actor->recoveryTicks == 0) {
        func_80109BB4(arg0, actor->collisionContacts);
        if ((u16)actor->hitRegion != 0) {
            func_8010B9A4(arg0);
            pan = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant - 1) << 16) + 0x4065000A, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        }
    }
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
        Gp_StopPlayerAnim(arg0, 0);
    }
}

/// Lock-on entry and step of the actor's `statePhase` state machine. An aim
/// within `0x301` of the lock node re-arms the actor: `state` takes the
/// 10-frame delay, `turnRateIndex`/`aimTrackingState` latch the turn and decay, `stateAux`
/// keeps the old `state`, and the `companionWork` record's `scanClearance` /
/// `scanAngle` are re-armed for the next sweep before the slot-1 child
/// animation. Otherwise state 0 zeroes `stateTimer` and picks state 2 (with
/// `movementMode` 3) or state 1 (with `movementMode` 1) from `func_8010BC70`'s
/// distance, states 1-3 only raise `movementSign`. The shared drive then resets
/// the move when the target is within `0x301`, counts `stateTimer` up to `0xB4`
/// before latching state 3 through the state-1 entry, and otherwise drops
/// `actionValue` while it is positive, re-arming it to `0x3C` from a `rand()`
/// window and returning to state 0. The target's coordinate goes to
/// `func_8010BD88` and `func_8010BE5C`.
static void func_actor_800100_80164184(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    GfxCoord*      target;
    s32            flag;
    s32            dist;
    s32            r;
    s32            arg;
    u16            timer;

    coord  = arg0->extra.tmd->coords;
    target = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor  = arg0->work;
    flag   = (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 42, 0, 0);
    dist   = _actor800100GetContactDistance(coord, actor->companionWork->probe.contacts, NULL);
    if (dist != 0 && dist < 0x301 && flag == 0) {
        GameActor* actor2 = arg0->work;

        timer                    = actor2->state;
        actor2->state            = 0xA;
        actor2->turnRateIndex    = 1;
        actor2->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
        companion                = actor2->companionWork;
        actor2->mode             = GAME_ACTOR_MODE_NORMAL;
        actor2->animationState   = 0;
        actor2->statePhase       = 0;
        actor2->stateAux         = timer;
        companion->scanClearance = COMPANION_SCAN_UNTESTED;
        companion->scanAngle     = 0;
        playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 6);
        return;
    }
    switch (actor->statePhase) {
        case 1:
        case 2:
        case 3:
            actor->movementSign = 1;
            goto drive;
        case 0:
            actor->stateTimer = 0;
            if (func_8010BC70(coord) >= 0x1600) {
                actor->statePhase   = 2;
                actor->movementMode = 3;
                arg                 = 4;
            } else {
            enter:
                if (actor->statePhase != 3) {
                    actor->statePhase = 1;
                }
                actor->movementMode = 1;
                arg                 = 2;
            }
            playerActorPlayChildSlotsWithBlend(arg0, arg, 0, 5);
            actor->movementSign = 1;
        drive:
            dist = func_8010BC70(coord);
            if (dist < 0x301) {
                Gp_ResetActorMove(arg0, 0);
            } else if (actor->statePhase != 3) {
                if (++actor->stateTimer == 0xB4) {
                    actor->statePhase = 3;
                    goto enter;
                }
                if (actor->actionValue > 0) {
                    actor->actionValue = (u16)actor->actionValue - 1;
                } else {
                    r = rand() & 0x3FF;
                    if ((0x1000 - r) < dist || actor->statePhase != 2) {
                        if (dist < r + 0x1400 || actor->statePhase != 1) {
                            goto done;
                        }
                    }
                    actor->statePhase  = 0;
                    actor->actionValue = 0x3C;
                }
            }
    }
done:
    func_8010BD88(arg0, MATRIX_TRANS(&target->coord));
    func_8010BE5C(arg0, MATRIX_TRANS(&target->coord));
}

/// Lock-on drive for the actor's `statePhase` state machine. Builds a `VECTOR3`
/// at `the scratch stack - 0x10` from the lock node (`Gp_GetLockPos`, or the
/// linked object's coord when `actor->targetNode` is set but flagged), plays the
/// 5/6 child-slot animation on entry, mirrors `actionValue` into `turnSign` and
/// resets the actor's move once the aim is close enough.
static void func_actor_800100_801643F4(Task* arg0)
{
    void**           scratch;
    VECTOR*          head;
    VECTOR3*         pos;
    GameActor*       actor;
    WorldTargetNode* node;
    TmdObject*       extra;
    GfxCoord*        src;
    s32              val;
    s32              arg;
    s32              flag;

    actor                            = arg0->work;
    extra                            = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd;
    scratch                          = SCRATCH_HEAD_ADDR;
    head                             = SCRATCH_HEAD_AT(scratch, VECTOR);
    SCRATCH_HEAD_AT(scratch, VECTOR) = head - 1;
    pos                              = (VECTOR3*)(head - 1);
    node                             = actor->targetNode;
    src                              = extra->coords;
    if (node != NULL) {
        if (!(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            Gp_GetLockPos(node, pos);
        } else {
            actor->statePhase = 2;
        }
    } else {
        pos->vx = src->coord.t[0];
        pos->vy = src->coord.t[1];
        pos->vz = src->coord.t[2];
    }
    switch (actor->statePhase) {
        case 0:
            flag              = 1;
            actor->statePhase = flag;
            if (func_8010BCF4(arg0, pos) < 0) {
                actor->actionValue = -1;
                arg                = 5;
            } else {
                actor->actionValue = 1;
                arg                = 6;
            }
            playerActorPlayChildSlotsWithBlend(arg0, arg, 0, 5);
            /* fallthrough */
        case 1:
            actor->turnSign = (u8)actor->actionValue;
            val             = func_8010BCF4(arg0, pos);
            if (val < 0) {
                val = -val;
            }
            if ((val < 0x81) || (actor->statePhase == 2)) {
                Gp_ResetActorMove(arg0, 0);
            }
            break;
    }
    func_8010BE5C(arg0, MATRIX_TRANS(&src->coord));
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Second arm of the lock-on drive: builds the lock position at
/// `the scratch stack - 0x10` (`Gp_GetLockPos`, or `Gp_FindLockNodePad` when
/// `targetNode` is flagged) and measures the distance to it with
/// `func_8010BCF4`. Close enough latches `statePhase` to 1 and plays the slot-7
/// child animation; otherwise the target is handed to `Gp_TrackAllyLockTarget`
/// with 1. `statePhase` 2/3 waits for the chain to reach 3, which resets the
/// move fields and plays the slots 9/6 pair.
///
/// `track:` sits between the state store and `case 1` so the hand-off is
/// emitted after the store; the store's fall into case 1 is therefore a jump.
static void func_actor_800100_80164580(Task* arg0)
{
    void**     scratch;
    VECTOR*    head;
    VECTOR3*   pos;
    GameActor* actor;
    s32        flag;
    s32        arg;
    s32        val;

    actor                            = arg0->work;
    scratch                          = SCRATCH_HEAD_ADDR;
    head                             = SCRATCH_HEAD_AT(scratch, VECTOR);
    SCRATCH_HEAD_AT(scratch, VECTOR) = head - 1;
    pos                              = (VECTOR3*)(head - 1);
    flag                             = 1;

    switch (actor->statePhase) {
        case 0:
            if (actor->targetNode != NULL) {
                if (actor->targetNode->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE) {
                    actor->targetNode = Gp_FindLockNodePad(arg0);
                }
                Gp_GetLockPos(actor->targetNode, pos);
                val = func_8010BCF4(arg0, pos);
                if (val < 0) {
                    val = -val;
                }
                if (val >= 0x201) {
                    goto track;
                }
            }
            actor->statePhase = flag;
            goto caseOne;
        track:
            Gp_TrackAllyLockTarget(arg0, 1);
            break;
        caseOne:
        case 1:
            arg                   = 7;
            actor->animationState = arg;
            actor->statePhase    += 1;
            playerActorPlayChildSlotsWithBlend(arg0, arg, 0, 3);
            /* fallthrough */
        case 2:
        case 3:
            Gp_TrackAllyLockTarget(arg0, 3);
            if (actor->statePhase == 3) {
                GameActor* actor2      = arg0->work;
                actor2->mode           = GAME_ACTOR_MODE_NORMAL;
                actor2->state          = 4;
                actor2->animationState = 0;
                actor2->statePhase     = 0;
                actor2->movementSign   = 0;
                actor2->turnSign       = 0;
                playerActorPlayChildSlotsWithBlend(arg0, 9, 0, 6);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

static void func_actor_800100_80164710(Task* arg0)
{
    GameActor*                 actor;
    GameActor*                 actor2;
    GameActor*                 actor3;
    CompanionWork*             companion;
    WorldTargetNode*           node;
    WorldTargetNode*           lock;
    GfxCoord*                  coord;
    _Actor800100TargetScratch* block;
    s32                        dist;
    u16                        state;

    actor     = arg0->work;
    block     = SCRATCH_STACK_RESERVE_BLOCK(_Actor800100TargetScratch);
    companion = actor->companionWork;
    Gp_TrackAllyLockTarget(arg0, 3);
    state = actor->statePhase;
    if (state != 0) {
        if (state == 1) {
            goto block_10;
        }
    } else {
        lock = actor->targetNode;
        if ((lock == NULL) || (coord = arg0->extra.tmd->coords, Gp_GetLockPos(lock, &block->targetPoint), func_80103C74(coord, &block->targetPoint, &block->targetDelta), ((func_80103D8C(block->targetDelta.vx, block->targetDelta.vz) < 0x301) != 0))) {
            actor2                 = arg0->work;
            actor2->mode           = GAME_ACTOR_MODE_NORMAL;
            actor2->state          = 4;
            actor2->animationState = 0;
            actor2->statePhase     = 0;
            actor2->movementSign   = 0;
            actor2->turnSign       = 0;
            playerActorPlayChildSlotsWithBlend(arg0, 9, 0, 6);
        } else {
            dist = func_8010BCF4(arg0, &block->targetPoint);
            if (dist < 0) {
                dist = -dist;
            }
            if (dist < 0x181) {
                actor->statePhase += 1;
            block_10:
                if (((s8)companion->activity.combat.repeatsRemaining <= 0) || (node = actor->targetNode, node == NULL) || (node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
                    // Stored through a plain pointer: the member-access spelling schedules differently.
                    *&actor->targetNode                                   = NULL;
                    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_DECAY;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    if ((u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant == 4) {
                        func_80106350(arg0, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], 0);
                    }
                    actor3                 = arg0->work;
                    actor3->mode           = GAME_ACTOR_MODE_NORMAL;
                    actor3->state          = 4;
                    actor3->animationState = 0;
                    actor3->statePhase     = 0;
                    actor3->movementSign   = 0;
                    actor3->turnSign       = 0;
                    playerActorPlayChildSlotsWithBlend(arg0, 9, 0, 6);
                } else if (actor->attackControl.cooldownTicks == 0) {
                    func_actor_800100_80166EE8(arg0);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor800100TargetScratch);
}

/// Third arm of the lock-on drive, running the actor's `statePhase` state
/// machine over a `VECTOR3` carved from the scratch stack. Case 0 latches the
/// state and `turnRateIndex`, aims at the lock node and plays the 5/6 child-slot
/// animation with a random `stateTimer` hold; it falls into case 1, which
/// mirrors `actionValue` into `turnSign` and, once the aim distance reaches
/// `stateTimer`, advances the state and plays slot 4. Case 2 counts
/// `stateTimer` down and, while it runs, asks `_actor800100GetContactDistance` for
/// the planar distance to what the ally block's `companionWork->probe.contacts` recorded: under
/// 0x281 the actor hands over to the `0xA` / slot-1 chain (`aimTrackingState` set,
/// `state` kept in `stateAux`), otherwise `turnSign` is cleared; a spent
/// counter resets the state to slot 9.
static void func_actor_800100_80164940(Task* arg0)
{
    void**         scratch;
    VECTOR*        head;
    VECTOR3*       pos;
    GameActor*     actor;
    GfxCoord*      coord;
    CompanionWork* companion;
    u16            old;
    s16            distance;
    s32            flag;
    s32            arg;
    s32            val;
    s32            count;
    s32            dist;

    scratch                          = SCRATCH_HEAD_ADDR;
    head                             = SCRATCH_HEAD_AT(scratch, VECTOR);
    SCRATCH_HEAD_AT(scratch, VECTOR) = head - 1;
    pos                              = (VECTOR3*)(head - 1);
    actor                            = arg0->work;
    coord                            = arg0->extra.tmd->coords;
    flag                             = 1;
    switch (actor->statePhase) {
        case 0:
            actor->statePhase    = flag;
            actor->turnRateIndex = flag;
            Gp_GetLockPos(actor->targetNode, pos);
            if (func_8010BCF4(arg0, pos) < 0) {
                actor->actionValue = flag;
                arg                = 6;
            } else {
                actor->actionValue = -1;
                arg                = 5;
            }
            actor->stateTimer = (rand() & 0x1FF) + 0x400;
            playerActorPlayChildSlotsWithBlend(arg0, arg, 0, 5);
            /* fallthrough */
        case 1:
            actor->turnSign = (u8)actor->actionValue;
            Gp_GetLockPos(actor->targetNode, pos);
            val  = func_8010BCF4(arg0, pos);
            dist = actor->stateTimer;
            if (val < 0) {
                val = -val;
            }
            if (val >= dist) {
                actor->movementMode = 3;
                actor->statePhase  += 1;
                actor->stateTimer   = (rand() & 0x1F) + 0x14;
                playerActorPlayChildSlotsWithBlend(arg0, 4, 0, 5);
            }
            break;
        case 2:
            actor->movementSign = flag;
            count               = actor->stateTimer - 1;
            actor->stateTimer   = count;
            if (count <= 0) {
                GameActor* actor2      = arg0->work;
                actor2->mode           = GAME_ACTOR_MODE_NORMAL;
                actor2->state          = 4;
                actor2->animationState = 0;
                actor2->statePhase     = 0;
                actor2->movementSign   = 0;
                actor2->turnSign       = 0;
                playerActorPlayChildSlotsWithBlend(arg0, 9, 0, 6);
            } else {
                distance = _actor800100GetContactDistance(coord, actor->companionWork->probe.contacts, NULL);
                if (distance != 0) {
                    if (distance < 0x281) {
                        s16        anim          = 1;
                        GameActor* actor3        = arg0->work;
                        old                      = actor3->state;
                        actor3->state            = 0xA;
                        actor3->aimTrackingState = anim;
                        companion                = actor3->companionWork;
                        actor3->mode             = GAME_ACTOR_MODE_NORMAL;
                        actor3->turnRateIndex    = flag;
                        actor3->animationState   = 0;
                        actor3->statePhase       = 0;
                        actor3->stateAux         = old;
                        companion->scanClearance = COMPANION_SCAN_UNTESTED;
                        companion->scanAngle     = 0;
                        playerActorPlayChildSlotsWithBlend(arg0, anim, 0, 6);
                    }
                } else {
                    actor->turnSign = 0;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Aim/lock drive for the actor's `statePhase` phase machine. While the planar distance
/// to what the ally block's `companionWork->probe.contacts` recorded is nonzero and under
/// `0x301`, `actionValue` counts up and the LCG decides the next aim window:
/// once the step passes `((gRandomLcgState >> 16) & 0x3F) + 0x28` the actor
/// latches into the `0xA` / child-slot-1 chain, keeping the old `state` in
/// `stateAux` and clearing the aim offset on `companionWork`. Otherwise it reserves
/// a `_Actor800100TargetScratch` on the scratch stack, fills `targetPoint`
/// either from the lock node (`Gp_GetLockPos`) or from the player's model
/// coordinate, runs the `statePhase` switch, measures the planar length of
/// `targetDelta`, and drops back to child slot 9 once the target is within the
/// rolled reach. Both arms end by handing `targetPoint` to `func_8010BD88` /
/// `func_8010BE5C` and releasing the block.
static void func_actor_800100_80164B9C(Task* arg0)
{
    GameActor*                 actor;
    GameActor*                 actor2;
    GameActor*                 actor3;
    CompanionWork*             companion;
    GfxCoord*                  coord;
    GfxCoord*                  target;
    _Actor800100TargetScratch* block;
    WorldTargetNode*           node;
    u16                        step;
    u16                        old;
    s16                        anim;
    u32                        random;
    s32                        distance;
    s32                        val;

    coord    = arg0->extra.tmd->coords;
    target   = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor    = arg0->work;
    distance = _actor800100GetContactDistance(coord, actor->companionWork->probe.contacts, NULL);
    if (distance != 0 && distance < 0x301) {
        step               = actor->actionValue + 1;
        actor->actionValue = step;
        random             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState    = random;
        if ((s16)step >= (s32)(((random >> 16) & 0x3F) + 0x28)) {
            anim                     = 1;
            actor2                   = arg0->work;
            old                      = actor2->state;
            actor2->state            = 0xA;
            actor2->turnRateIndex    = anim;
            actor2->aimTrackingState = anim;
            companion                = actor2->companionWork;
            actor2->mode             = GAME_ACTOR_MODE_NORMAL;
            actor2->animationState   = 0;
            actor2->statePhase       = 0;
            actor2->stateAux         = old;
            companion->scanClearance = COMPANION_SCAN_UNTESTED;
            companion->scanAngle     = 0;
            playerActorPlayChildSlotsWithBlend(arg0, anim, 0, 6);
            return;
        }
    }
    block = SCRATCH_STACK_RESERVE_BLOCK(_Actor800100TargetScratch);
    node  = actor->targetNode;
    if (node != NULL) {
        if ((node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE) == 0) {
            Gp_GetLockPos(node, &block->targetPoint);
        } else {
            actor->statePhase = 2;
        }
    } else {
        block->targetPoint.vx = target->coord.t[0];
        block->targetPoint.vy = target->coord.t[1];
        block->targetPoint.vz = target->coord.t[2];
    }
    switch (actor->statePhase) {
        case 0:
            actor->statePhase   = 1;
            actor->stateTimer   = 0;
            actor->movementMode = 3;
            playerActorPlayChildSlotsWithBlend(arg0, 0xC, 0, 5);
        case 1:
        case 2:
            break;
        default:
            goto tail;
    }
    actor->movementSign = 1;
    func_80103C74(coord, &block->targetPoint, &block->targetDelta);
    distance = func_80103D8C(block->targetDelta.vx, block->targetDelta.vz);
    if (actor->targetNode != NULL) {
        val = (rand() & 0x3FF) + 0xB00;
    } else {
        val = 0xB00;
    }
    if (val >= distance || actor->statePhase == 2) {
        actor3                 = arg0->work;
        actor3->mode           = GAME_ACTOR_MODE_NORMAL;
        actor3->state          = 4;
        actor3->animationState = 0;
        actor3->statePhase     = 0;
        actor3->movementSign   = 0;
        actor3->turnSign       = 0;
        playerActorPlayChildSlotsWithBlend(arg0, 9, 0, 6);
    }
tail:
    func_8010BD88(arg0, &block->targetPoint);
    func_8010BE5C(arg0, &block->targetPoint);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor800100TargetScratch);
}

static void func_actor_800100_80164E60(Task* arg0)
{
    GameActor*             actor;
    GameActor*             target;
    CompanionWork*         companion;
    const AnimationRecord* rec;
    GfxCoord*              coord;
    s16                    sel;

    actor     = arg0->work;
    companion = actor->companionWork;
    rec       = animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1);
    coord     = actor->equipmentTasks[1]->extra.tmd->coords;
    sel       = D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant];

    switch (sel) {
        case 3:
            if (rec != NULL) {
                if (rec != actor->lastCueRecord) {
                    actor->lastCueRecord = rec;
                    if ((rec->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                        if (actor->statePhase == 0) {
                            actor->statePhase = 1;
                        }
                    }
                }
            }
            break;
        case 12:
            if (actor->statePhase == 0) {
                actor->statePhase = 1;
                Gp_SpawnEff(EFFECT_RELOAD_EMITTER, coord, 0xC, NULL);
            }
            if (rec != NULL) {
                if (rec != actor->lastCueRecord) {
                    actor->lastCueRecord = rec;
                }
            }
            break;
        default:
            if (actor->statePhase == 0) {
                actor->statePhase = 1;
            } else {
                if (rec != NULL) {
                    if (rec != actor->lastCueRecord) {
                        actor->lastCueRecord = rec;
                    }
                }
            }
            break;
    }

    companion->activity.combat.attacksRemaining = D_actor_800100_80167230[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant];
    if (rec != NULL && func_80105894(arg0, 1, 0, 0) == 0) {
        target                 = arg0->work;
        target->mode           = GAME_ACTOR_MODE_NORMAL;
        target->state          = 4;
        target->animationState = 0;
        target->statePhase     = 0;
        target->movementSign   = 0;
        target->turnSign       = 0;
        playerActorPlayChildSlotsWithBlend(arg0, 9, 0, 6);
    }
}

static void func_actor_800100_80165010(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    GfxCoord*      target;
    s32            dist;
    u32            rng;
    s32            arg;
    s32            slot;
    s32            diff;
    s32            flag;
    s32            turn;
    s32            state;
    s32            heading;
    u16            angle;

    coord     = arg0->extra.tmd->coords;
    target    = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor     = arg0->work;
    companion = actor->companionWork;
    dist      = _actor800100GetContactDistance(coord, companion->probe.contacts, NULL);
    state     = actor->statePhase;
    flag      = 1;

    switch (state) {
        case 0:
            if (companion->scanAngle < ACTOR_TRANSFORM_ANGLE_TURN) {
                // Prefer the farthest contact, with a clear direction ending the search.
                if (companion->scanClearance != COMPANION_SCAN_CLEAR && (companion->scanClearance < dist || dist == COMPANION_SCAN_CLEAR)) {
                    companion->scanClearance = dist;
                    companion->targetHeading = companion->scanAngle;
                }
                companion->scanAngle += COMPANION_SCAN_ANGLE_STEP;
            } else {
                actor->statePhase        = flag;
                angle                    = (companion->targetHeading + actor->rotation.vy) & ACTOR_TRANSFORM_ANGLE_MASK;
                companion->targetHeading = angle;
                turn                     = func_80103E7C(actor->rotation.vy, angle);
                arg                      = 5;
                if (turn > 0) {
                    arg                = 6;
                    companion->turnDir = flag;
                } else {
                    companion->turnDir = -1;
                }
                playerActorPlayChildSlotsWithBlend(arg0, arg, 0, 3);
            }
            break;
        case 1:
            actor->turnSign = (u8)companion->turnDir;
            do {
                heading = actor->rotation.vy;
                state   = companion->targetHeading;
                diff    = heading - state;
                if (diff < 0) {
                    diff = -diff;
                }
            } while (0);
            if (diff < 0x40) {
                actor->statePhase++;
                actor->rotation.vy = companion->targetHeading;
                actor->turnSign    = 0;
                if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                    slot                = 4;
                    actor->movementMode = 3;
                    rng                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState     = rng;
                    actor->stateTimer   = ((rng >> 16) & 0x3F) + 0x14;
                    playerActorPlayChildSlotsWithBlend(arg0, slot, 0, 3);
                } else {
                    slot                = 2;
                    actor->movementMode = 1;
                    rng                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState     = rng;
                    actor->stateTimer   = ((rng >> 16) & 0x7F) + 0x28;
                    playerActorPlayChildSlotsWithBlend(arg0, slot, 0, 3);
                }
            }
            break;
        case 2:
            if (dist >= 0x301 || dist == 0) {
                if (--actor->stateTimer > 0) {
                    goto setFlag;
                }
            }
            Gp_ResetActorMove(arg0, 0);
            break;
        setFlag:
            actor->movementSign = 1;
            break;
    }
    func_8010BE5C(arg0, MATRIX_TRANS(&target->coord));
}

static void func_actor_800100_801652B0(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GameActor*     target;
    CompanionWork* targetCompanion;
    s32            flag;
    s32            arg;
    s32            val;
    s32            dist;
    s32            targetDist;
    s32            turn;
    s32            state;
    s16            ang;
    s16            anim;
    u16            old;

    actor      = arg0->work;
    companion  = actor->companionWork;
    targetDist = _actor800100GetContactDistance(arg0->extra.tmd->coords, companion->probe.contacts, NULL);
    state      = actor->statePhase;
    flag       = 1;
    switch (state) {
        case 0:
            actor->statePhase        = flag;
            ang                      = (actor->rotation.vy + (rand() & ACTOR_TRANSFORM_ANGLE_MASK)) & ACTOR_TRANSFORM_ANGLE_MASK;
            companion->targetHeading = ang;
            turn                     = func_80103E7C(actor->rotation.vy, ang);
            arg                      = 5;
            if (turn << 16 > 0) {
                arg                = 6;
                companion->turnDir = flag;
            } else {
                companion->turnDir = -1;
            }
            playerActorPlayChildSlotsWithBlend(arg0, arg, 0, 3);
        case 1:
            val             = (u8)companion->turnDir;
            actor->turnSign = val;
            val             = actor->rotation.vy;
            state           = companion->targetHeading;
            dist            = val - state;
            if (dist < 0) {
                dist = -dist;
            }
            if (dist < 0x40) {
                actor->statePhase  += 1;
                actor->rotation.vy  = (u16)companion->targetHeading;
                actor->turnSign     = 0;
                actor->movementMode = 1;
                val                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState     = val;
                val                 = (((u32)val >> 16) & 0x7F) + 0x1E;
                actor->stateTimer   = val;
                playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 3);
                break;
            }
            break;
        case 2:
            val               = actor->stateTimer - 1;
            actor->stateTimer = val;
            if (val != 0) {
                break;
            }
            if (targetDist < 0x401 && targetDist != 0) {
                anim                           = 1;
                target                         = arg0->work;
                old                            = target->state;
                target->state                  = 0xA;
                target->aimTrackingState       = anim;
                targetCompanion                = target->companionWork;
                target->mode                   = GAME_ACTOR_MODE_NORMAL;
                target->turnRateIndex          = flag;
                target->animationState         = 0;
                target->statePhase             = 0;
                target->stateAux               = old;
                targetCompanion->scanClearance = COMPANION_SCAN_UNTESTED;
                targetCompanion->scanAngle     = 0;
                playerActorPlayChildSlotsWithBlend(arg0, anim, 0, 6);
                break;
            }
            actor->statePhase += 1;
            actor->stateTimer  = (rand() & 0x3F) + 0x3C;
            playerActorPlayChildSlotsWithBlend(arg0, 2, 0, 3);
            break;
        case 3:
            if (targetDist < 0x301 && targetDist != 0) {
                Gp_ResetActorMove(arg0, 0);
                break;
            }
            val               = actor->stateTimer - 1;
            actor->stateTimer = val;
            if (val <= 0) {
                Gp_ResetActorMove(arg0, 0);
                break;
            }
            actor->movementSign = flag;
            break;
    }
}

static void func_actor_800100_80165528(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable3 sp;

    sp    = D_actor_800100_80161E4C;
    actor = arg0->work;
    if (actor->attackControl.cooldownTicks > 0) {
        actor->attackControl.cooldownTicks--;
    }
    if ((s8)actor->recoveryTicks > 0) {
        actor->recoveryTicks--;
    }
    actor->movementSign = 0;
    actor->turnSign     = 0;
    sp.funcs[actor->mode](arg0);
    actor->usesPushbackDirection = 0;
}

static void func_actor_800100_801655C0(Task* arg0)
{
    GameActor* actor;

    actor                                                  = arg0->work;
    actor->state                                           = 3;
    actor->mode                                            = GAME_ACTOR_MODE_NORMAL;
    actor->animationState                                  = 0;
    actor->statePhase                                      = 0;
    actor->companionWork->activity.combat.repeatsRemaining = 0;
    actor->aimTrackingState                                = GAME_ACTOR_AIM_TRACKING_TARGET;
    actor->targetNode                                      = Gp_FindLockNode(arg0);
    playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 6);
}

static void func_actor_800100_80165630(Task* arg0)
{
    GameActor* actor;

    actor                   = arg0->work;
    actor->state            = 7;
    actor->mode             = GAME_ACTOR_MODE_NORMAL;
    actor->turnRateIndex    = 0;
    actor->animationState   = 0;
    actor->statePhase       = 0;
    actor->actionValue      = 0;
    actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    actor->movementSign     = 0;
    actor->turnSign         = 0;
}

static void func_actor_800100_80165664(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    u16            temp;

    actor                    = arg0->work;
    temp                     = actor->state;
    actor->state             = 0xA;
    actor->turnRateIndex     = 1;
    actor->aimTrackingState  = GAME_ACTOR_AIM_TRACKING_DECAY;
    companion                = actor->companionWork;
    actor->mode              = GAME_ACTOR_MODE_NORMAL;
    actor->animationState    = 0;
    actor->statePhase        = 0;
    actor->stateAux          = temp;
    companion->scanClearance = COMPANION_SCAN_UNTESTED;
    companion->scanAngle     = 0;
    playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 6);
}

static void func_actor_800100_801656C8(Task* arg0)
{
    GameActor* actor;

    actor                 = arg0->work;
    actor->state          = 1;
    actor->turnRateIndex  = 1;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->animationState = 0;
    actor->statePhase     = 0;
    actor->idleTicks      = 0;
    actor->actionValue    = 0x3C;
}

static void func_actor_800100_801656F4(Task* arg0)
{
    GameActor* actor;

    actor                 = arg0->work;
    actor->state          = 2;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode   = 0;
    actor->turnRateIndex  = 1;
    actor->animationState = 0;
    actor->statePhase     = 0;
    actor->idleTicks      = 0;
}

static void func_actor_800100_80165720(Task* arg0)
{
    GameActor* actor;

    actor                 = arg0->work;
    actor->state          = 0xB;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->turnRateIndex  = 1;
    actor->animationState = 0;
    actor->statePhase     = 0;
    actor->idleTicks      = 0;
}

static void func_actor_800100_80165748(Task* arg0)
{
    GameActor* actor;

    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        actor                                                  = arg0->work;
        actor->state                                           = 3;
        actor->mode                                            = GAME_ACTOR_MODE_NORMAL;
        actor->animationState                                  = 0;
        actor->statePhase                                      = 0;
        actor->companionWork->activity.combat.repeatsRemaining = 0;
        actor->aimTrackingState                                = GAME_ACTOR_AIM_TRACKING_TARGET;
        actor->targetNode                                      = Gp_FindLockNode(arg0);
        playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 6);
        return;
    }
    func_actor_800100_80163D54(arg0);
}

static void func_actor_800100_801657D8(Task* arg0)
{
    if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
        func_actor_800100_80166E14(arg0);
        return;
    }
    func_actor_800100_801659EC(arg0);
}

static void func_actor_800100_80165818(Task* arg0)
{
    GameActor* actor;

    actor = arg0->work;
    if (actor->statePhase != 0) {
        Gp_ResetActorMove(arg0, 0);
    }
}

/// Handlers `func_actor_800100_80165850` runs, indexed by `hitRegion`.
static const TaskFuncTable4 D_actor_800100_80161E88 = { {
    func_actor_800100_801658E8,
    func_actor_800100_801658E8,
    func_actor_800100_801658E8,
    func_actor_800100_80165928,
} };

static void func_actor_800100_80165850(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable4 handlers;

    handlers = D_actor_800100_80161E88;
    actor    = arg0->work;
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    handlers.funcs[(u16)actor->hitRegion](arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
}

static void func_actor_800100_801658E8(Task* arg0)
{
    GameActor* actor;
    u16        value;

    actor = arg0->work;
    value = actor->statePhase;
    if (value == 0) {
        return;
    }
    if (value == 1) {
        func_8010C180(arg0);
    }
}

static void func_actor_800100_80165928(Task* arg0)
{
}

/// Gameplay's player-mode handlers `func_actor_800100_80165930` runs,
/// indexed by `state`.
static const TaskFuncTable7 D_actor_800100_80161E98 = { {
    Gp_PlayerMode2State0,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State2,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State4,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State6,
} };

static void func_actor_800100_80165930(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable7 sp;

    sp    = D_actor_800100_80161E98;
    actor = arg0->work;
    sp.funcs[(u16)actor->state](arg0);
    Gp_TurnPlayer(arg0);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
        func_8010BFCC(arg0);
        Gp_StopPlayerAnim(arg0, 0);
    }
}

static void func_actor_800100_801659EC(Task* arg0)
{
    GameActor*       actor;
    CompanionWork*   companion;
    WorldTargetNode* node;
    GfxCoord*        coord;
    VECTOR3*         lock;
    VECTOR*          head;
    u8*              entry;
    u8*              offset;
    s32              kind;
    s32              angle;
    s32              index;
    s32              inRange;
    s32              mode;

    head                          = SCRATCH_STACK_CURSOR(VECTOR);
    lock                          = (VECTOR3*)(head - 1);
    SCRATCH_STACK_CURSOR(VECTOR3) = lock;
    actor                         = arg0->work;
    companion                     = actor->companionWork;
    coord                         = arg0->extra.tmd->coords;
    node                          = Gp_FindLockNode(arg0);
    actor->targetNode             = node;
    if (node != NULL) {
        Gp_GetLockPos(node, lock);
        func_80103C74(coord, lock, lock);
        kind = func_80103D8C(*(s32*)lock, lock->vz);
        mode = 2;
        if (kind >= 0x381) {
            if (kind < 0) {
                index  = kind;
                index += 0x3FF;
            } else {
                index = kind;
            }
            angle = index >> 0xA;
            if (angle >= 3) {
                if (angle < 5) {
                    angle = 3;
                }
            }
            inRange = angle < 4;
            if (inRange != 0) {
                entry  = D_actor_800100_801672F8[angle];
                offset = entry + (companionGetHealthBand() * 0x10);
                mode   = offset[rand() & 0xF];
            } else {
                mode = 3;
            }
        } else {
            mode = 2;
        }
    } else {
        if ((s8)actor->aimTrackingState == GAME_ACTOR_AIM_TRACKING_TARGET) {
            actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
        }
        mode = D_actor_800100_80167308[rand() & 7];
        if (mode == 3) {
            actor->targetNode = NULL;
        }
    }
    switch (mode) {
        case 0:
            break;
        case 1:
            if ((s8)companion->activity.combat.attacksRemaining <= 0) {
                func_actor_800100_80166E94(arg0, 0);
            } else {
                actor->aimTrackingState                     = GAME_ACTOR_AIM_TRACKING_TARGET;
                actor->attackControl.cooldownTicks          = (rand() & 0x1F) + 0xF;
                companion->activity.combat.repeatsRemaining = D_actor_800100_80167310[rand() & 7];
                func_actor_800100_80166DD0(arg0);
            }
            break;
        case 2:
            func_actor_800100_80166DF0(arg0);
            companionSetDecisionDelay(arg0, 0x14, 0x3F);
            break;
        case 3:
            func_actor_800100_80165630(arg0);
            break;
        case 4:
            func_actor_800100_80165664(arg0);
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

static void func_actor_800100_80165C38(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    GfxCoord*      place;
    u16            state;

    place = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);

    actor     = arg0->work;
    companion = actor->companionWork;
    state     = actor->stateAux;
    coord     = actor->equipmentTasks[1]->extra.tmd->coords;

    switch (state) {
        case 0:
            actor->mode                                  = GAME_ACTOR_MODE_NORMAL;
            actor->movementMode                          = 0;
            actor->turnRateIndex                         = 0;
            actor->animationState                        = 0;
            actor->stateAux                              = 1;
            companion->activity.combat.attacksRemaining -= 1;
            playerActorPlayChildSlotsWithBlend(arg0, 0xA, 1, 3);
            func_80106238(arg0, 0, 0);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x40650001, 1);
            Gp_SpawnEff(EFFECT_HANDGUN_MUZZLE_FLASH, coord, 0x21, NULL);
            break;

        case 1:
            actor->stateAux                                       = 2;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (func_actor_800100_80166B40(actor->weaponContacts, coord, place) != 0) {
                Gp_PlayObjSfx(place, 0x17, 1);
            }
            /* fallthrough */

        case 2:
            if (func_80105894(arg0, 8, 0, 0) == 0) {
                actor->attackControl.cooldownTicks           = 0xA;
                companion->activity.combat.repeatsRemaining -= 1;
                func_actor_800100_80166DD0(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

static void func_actor_800100_80165DE8(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    u16            state;
    GfxCoord*      coord;

    actor     = arg0->work;
    companion = actor->companionWork;
    state     = actor->stateAux;
    coord     = actor->equipmentTasks[1]->extra.tmd->coords;

    switch (state) {
        case 0:
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->movementMode   = 0;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            /* fallthrough */
        case 1:
            actor->stateAux                              = 2;
            companion->activity.combat.attacksRemaining -= 1;
            playerActorPlayChildSlotsWithBlend(arg0, 0xA, 1, 3);
            Gp_PlayObjSfx(coord, 0x40660001, 1);
            if (coord != NULL) {
                actor->attackControl.cooldownTicks = 0x28;
                Gp_SpawnEff(EFFECT_GRENADE_MUZZLE_FLASH, coord, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant] | 0x10000, NULL);
                func_80104490(arg0, 1, 2, 0x110C0A);
                return;
            }
            return;
        case 2:
            if (func_80105894(arg0, 8, 0, 0) == 0) {
                actor->attackControl.cooldownTicks           = 0x12;
                companion->activity.combat.repeatsRemaining -= 1;
                func_actor_800100_80166DD0(arg0);
            }
            break;
    }
}

/// Handlers `func_actor_800100_80166EE8` runs, indexed by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant`.
static const TaskFuncTable5 D_actor_800100_80161EC8 = { {
    func_actor_800100_80165C38,
    func_actor_800100_80165C38,
    func_actor_800100_80165DE8,
    func_actor_800100_80165F50,
    func_actor_800100_80166190,
} };

static void func_actor_800100_80165F50(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    GfxCoord*      place;
    u16            state;
    void**         scratch;
    GfxCoord*      head;

    scratch                            = SCRATCH_HEAD_ADDR;
    head                               = SCRATCH_HEAD_AT(scratch, GfxCoord);
    SCRATCH_HEAD_AT(scratch, GfxCoord) = head - 1;
    place                              = head - 1;

    actor     = arg0->work;
    companion = actor->companionWork;
    state     = actor->stateAux;
    coord     = actor->equipmentTasks[1]->extra.tmd->coords;

    switch (state) {
        case 0:
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->movementMode                                   = 0;
            actor->turnRateIndex                                  = 2;
            actor->animationState                                 = 0;
            companion->activity.combat.repeatsRemaining           = (rand() & 7) + 3;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
            /* fallthrough */

        case 1:
        block_4:
            actor->stateAux                    = 2;
            actor->attackControl.cooldownTicks = 0;
            actor->stateTimer                  = 3;
            func_80106238(arg0, 0, 0);
            /* fallthrough */

        case 2:
            actor->stateTimer -= 1;
            if (actor->stateTimer == 0) {
                actor->stateAux                                      += 1;
                companion->activity.combat.attacksRemaining          -= 1;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x40670001, 1);
                Gp_SpawnEff(EFFECT_HANDGUN_MUZZLE_FLASH, coord, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant] | 0x10000, NULL);
                playerActorPlayChildSlotsWithBlend(arg0, 0xA, 1, 2);
            }
            break;

        case 3:
            actor->stateAux                                      += 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (func_actor_800100_80166B40(actor->weaponContacts, coord, place) != 0) {
                Gp_PlayObjSfx(place, 0x17, 1);
            }
            /* fallthrough */

        case 4:
            companion->activity.combat.repeatsRemaining -= 1;
            if ((s8)companion->activity.combat.repeatsRemaining > 0) {
                if ((s8)companion->activity.combat.attacksRemaining > 0) {
                    goto block_4;
                }
            }
            if (func_80105894(arg0, 8, 0, 0) == 0) {
                func_actor_800100_80166DD0(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

static void func_actor_800100_80166190(Task* arg0)
{
    void**         scratch;
    GfxCoord*      head;
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    GfxCoord*      place;
    u16            state;

    scratch                            = SCRATCH_HEAD_ADDR;
    head                               = SCRATCH_HEAD_AT(scratch, GfxCoord);
    SCRATCH_HEAD_AT(scratch, GfxCoord) = head - 1;
    place                              = head - 1;

    actor     = arg0->work;
    companion = actor->companionWork;
    state     = actor->stateAux;
    coord     = actor->equipmentTasks[1]->extra.tmd->coords;

    switch (state) {
        case 0:
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->movementMode   = 0;
            actor->animationState = 0;
            actor->stateAux      += 1;
            playerActorPlayChildSlotsWithBlend(arg0, 9, 0, 1);
            break;

        case 1:
            if (animationGetCurrentRecord(&actor->animationContext,
                                          actor->animationSlots + 1) != NULL) {
                actor->stateAux += 1;
            }
            break;

        case 2:
            gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 0xFF) < 0x3F) {
                actor->stateAux                    = 5;
                actor->turnRateIndex               = 2;
                actor->attackControl.cooldownTicks = 0x28;
                actor->attackCancelTicks           = 0x1C;
                actor->actionValue                 = 0x14;
                Gp_PlayObjSfx(coord, 0x40680002, 1);
                if (actor->weaponEffectTask != NULL) {
                    actor->weaponEffectTask->spawnArg1.value = 2;
                }
                break;
            }
            actor->stateAux          = 3;
            actor->turnRateIndex     = 0;
            actor->stateTimer        = 0;
            actor->attackCancelTicks = 9;
            actor->actionValue       = 3;
            func_80106238(arg0, 0, 1);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
            /* fallthrough */

        case 3:
            if (actor->actionValue != 0) {
                if (actor->stateTimer == 0) {
                    actor->actionValue                                    = (u16)actor->actionValue - 1;
                    actor->stateTimer                                     = 3;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    companion->activity.combat.attacksRemaining          -= 1;
                    if ((s8)companion->activity.combat.attacksRemaining == 0) {
                        actor->actionValue = 0;
                    }
                    Gp_PlayObjSfx(coord, 0x40680001, 1);
                    Gp_SpawnEff(EFFECT_RIFLE_MUZZLE_FLASH, coord, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant] | 0x10000, NULL);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 2);
                    break;
                } else {
                    actor->stateTimer -= 1;
                    if (actor->stateTimer != 0) {
                        break;
                    }
                }
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                if (func_actor_800100_80166B40(actor->weaponContacts, coord, place) != 0) {
                    Gp_PlayObjSfx(place, 0x17, 1);
                }
                break;
            }
            /* fallthrough */

        case 4:
            actor->stateAux                                       = 6;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (func_actor_800100_80166B40(actor->weaponContacts, coord, place) != 0) {
                Gp_PlayObjSfx(place, 0x17, 1);
            }
            break;

        case 5:
            if (actor->actionValue == 0) {
                actor->stateAux = 6;
                if (actor->weaponEffectTask != NULL) {
                    actor->weaponEffectTask->spawnArg1.value = 3;
                }
                sndEvtRequestScriptStop(SOUND_COMPANION_PYKE_FIRE_TAIL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                playerActorPlayChildSlotsWithBlend(arg0, 0xB, 0, 2);
            } else {
                actor->actionValue = (u16)actor->actionValue - 1;
            }
            break;

        case 6:
            if (func_80105894(arg0, 8, 0, 0) == 0) {
                actor->attackControl.cooldownTicks          = 0xF;
                companion->activity.combat.repeatsRemaining = (actor->attackButton == 1) ? companion->activity.combat.repeatsRemaining - 1 : 0;
                func_actor_800100_80166DD0(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

static void func_actor_800100_80166514(Task* arg0)
{
    void**                      scratch;
    GameActor*                  actor;
    GfxCoord                    sp10;
    GfxCoord*                   src;
    WorldCollisionBody*         obj;
    _Actor800100AimBeamScratch* blk;
    s16                         distance;

    actor       = arg0->work;
    src         = actor->equipmentTasks[1]->extra.tmd->coords;
    obj         = &actor->collisionBodies[GAME_ACTOR_BODY_AIM];
    sp10        = *src;
    obj->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

    scratch = SCRATCH_HEAD_ADDR;
    blk     = SCRATCH_PUSH_AT(scratch, _Actor800100AimBeamScratch);

    worldCollisionFindContactIndex(obj->context.capsule->contacts, WORLD_COLLISION_FIND_ANY_KEY);
    gfxRotMatrixX(&sp10.workm, 0x400, GRAPHICS_ROTATION_COMPOSE);
    blk->offset.vx = 0;
    blk->offset.vy = 0x120;
    blk->offset.vz = 0x20;
    Gp_PlaceCoordOffset(&sp10, &blk->coord, &blk->offset);
    distance             = _actor800100GetContactDistance(&blk->coord, actor->aimContacts, NULL);
    blk->contactDistance = distance;
    func_actor_800100_8016666C(&blk->coord, distance);
    blk->offset.vx = 0;
    blk->offset.vz = 0;
    blk->offset.vy = blk->contactDistance + 0x38;
    Gp_PlaceCoordOffset(&blk->coord, &blk->coord, &blk->offset);
    func_actor_800100_801668C0(&blk->coord);
    worldCollisionClearContacts(actor->aimContacts);
    SCRATCH_POP_AT(scratch, _Actor800100AimBeamScratch);
}

/* The block is carved off `head` into `newhead` and stored there, but the GTE
   calls address it through `blk`: the ROM keeps that pointer as a copy of
   `newhead` in `$a3`, and one variable for both drops it. The post-`rcos`
   reads go through `newhead` for the same reason - naming `blk` there would
   keep the copy live across the call - and the two `originScreen` reads are
   spelled off `head`, whose folded address is the one the ROM uses. */
static void func_actor_800100_8016666C(GfxCoord* arg0, s16 arg1)
{
    void**                          scratch;
    _Actor800100AimBeamLineScratch* head;
    _Actor800100AimBeamLineScratch* newhead;
    _Actor800100AimBeamLineScratch* blk;
    LINE_G2*                        prim;
    s16                             angle;
    s32                             sy0;
    s32                             sy1;

    scratch                                                  = SCRATCH_HEAD_ADDR;
    head                                                     = SCRATCH_HEAD_AT(scratch, _Actor800100AimBeamLineScratch);
    newhead                                                  = head - 1;
    blk                                                      = newhead;
    SCRATCH_HEAD_AT(scratch, _Actor800100AimBeamLineScratch) = newhead;

    angle = arg1;
    if (arg1 == 0) {
        angle = D_80112F60[gPlayerStatus.weapon];
    }
    blk->tip.vy    = angle;
    blk->origin.vx = 0;
    blk->origin.vy = 0;
    blk->origin.vz = 0;
    blk->tip.vx    = 0;
    blk->tip.vz    = 0;

    gte_SetTransMatrix(&arg0->workm);
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(&blk->origin);
    gte_rtps();
    gte_stsxy(&blk->originScreen);
    gte_ldv0(&blk->tip);
    gte_rtps();
    gte_stsxy(&blk->tipScreen);
    gte_stszotz(&blk->otz);

    if (newhead->otz >= 0x20) {
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setLineG2(prim);
        prim->x0 = (head - 1)->originScreen.vx;
        /* Both `vy` loads sign-extend, which needs the `s32` locals: a direct
           16-bit field copy assembles to `lhu` for either of them. */
        sy0      = (head - 1)->originScreen.vy;
        prim->y0 = sy0;
        prim->x1 = newhead->tipScreen.vx;
        sy1      = newhead->tipScreen.vy;
        prim->y1 = sy1;
        /* Both ends pulse with the frame counter, the far one 0x50 darker. */
        prim->r0 = (rcos(gDisplayState.gameTick) & 0x1F) - 0x80;
        prim->g0 = 0x20;
        prim->b0 = 0x20;
        prim->r1 = prim->r0 - 0x50;
        prim->g1 = 0;
        prim->b1 = 0;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)newhead->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, newhead->otz);
    }
    SCRATCH_POP_AT(scratch, _Actor800100AimBeamLineScratch);
}

/// The four (y, z) corners of the quad `func_actor_800100_801668C0` draws,
/// offset off the placed coordinate's world translation. The table sits in the
/// unit's .rodata right after the jump tables, so it is written here rather
/// than left to the split: nothing else refers to it.
static const _Actor800100AimBeamQuadCorner D_actor_800100_80161F10[4] = {
    { -62, 0 },
    { -62, 124 },
    { 62, 0 },
    { 62, 124 },
};

/* The beam quad `func_actor_800100_80166514` places: the four
   `D_actor_800100_80161F10` (y, z) offsets, raised to the coordinate's world
   translation, projected through `GsWSMATRIX` and textured with one 0x20
   square of the atlas.
   The block is reserved and its pointer taken in one assignment: the ROM
   keeps the allocated pointer as a copy of the store's temporary in `$t1`,
   and splitting the two into separate statements drops that copy. */
static void func_actor_800100_801668C0(GfxCoord* arg0)
{
    void**                          scratch;
    _Actor800100AimBeamQuadScratch* blk;
    POLY_FT4*                       prim;
    s32                             i;
    s32                             ay;
    s32                             az;
    s32                             sy;

    scratch = SCRATCH_HEAD_ADDR;
    blk     = SCRATCH_PUSH_AT(scratch, _Actor800100AimBeamQuadScratch);

    for (i = 0; i < 4; i++) {
        ay                      = D_actor_800100_80161F10[i].vy;
        az                      = D_actor_800100_80161F10[i].vz;
        blk->cornerOffset.vx    = 0;
        blk->cornerOffset.vy    = ay;
        blk->cornerOffset.vz    = az;
        blk->worldCorners[i].vx = (u16)blk->cornerOffset.vx + (u16)arg0->workm.t[0];
        blk->worldCorners[i].vy = (u16)blk->cornerOffset.vy + (u16)arg0->workm.t[1];
        blk->worldCorners[i].vz = (u16)blk->cornerOffset.vz + (u16)arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);

    gte_ldv0(&blk->worldCorners[0]);
    gte_rtps();

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);

    gte_stsxy2(&blk->screenCorners[0]);

    gte_ldv3(&blk->worldCorners[1], &blk->worldCorners[2], &blk->worldCorners[3]);
    gte_rtpt();
    prim->tpage = 0x27;
    prim->clut  = 0x3CCE;
    setUV4(prim, 0x20, 0x80, 0x3F, 0x80, 0x20, 0x9F, 0x3F, 0x9F);
    prim->code |= 3;

    gte_stsxy3(&blk->screenCorners[1], &blk->screenCorners[2], &blk->screenCorners[3]);
    gte_stszotz(&blk->otz);

    /* Both `vy` loads sign-extend, which the `s32` locals keep: a direct
       16-bit field copy assembles to `lhu` for either of them. */
    prim->x0 = blk->screenCorners[0].vx;
    sy       = blk->screenCorners[0].vy;
    prim->y0 = sy;
    prim->x1 = blk->screenCorners[1].vx;
    sy       = blk->screenCorners[1].vy;
    prim->y1 = sy;
    prim->x2 = blk->screenCorners[2].vx;
    sy       = blk->screenCorners[2].vy;
    prim->y2 = sy;
    prim->x3 = blk->screenCorners[3].vx;
    sy       = blk->screenCorners[3].vy;
    prim->y3 = sy;

    addPrim(&gGpuCurrentOt[blk->otz >> 4], prim);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor800100AimBeamQuadScratch);
}

static s32 func_actor_800100_80166B40(WorldCollisionContact* arg0, GfxCoord* arg1, GfxCoord* arg2)
{
    s32                             minDist;
    s32                             idx;
    PlayerActorWeaponImpactScratch* block;
    WorldCollisionContact*          rec;
    s32                             i;
    s32                             bestIdx;
    s32                             dist;

    minDist = 0x7FFFFFFF;
    if (worldCollisionCountContactsByKind(arg0, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
        return 0;
    }
    block = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorWeaponImpactScratch);
    for (i = 0, bestIdx = 0; i < 6; i++) {
        rec = &arg0[i];
        if (rec->key.value & 0x100000) {
            dist  = abs(arg1->workm.t[0] - rec->point.vx);
            dist += abs(arg1->workm.t[1] - rec->point.vy);
            dist += abs(arg1->workm.t[2] - rec->point.vz);
            if (dist < minDist) {
                func_800E0FEC(rec, &block->pushback, 1, &idx);
                idx = worldCollisionSurfaceClassFromMask((const u8*)&idx);
                if (Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][idx]->weaponImpactEnabled != WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS) {
                    minDist = dist;
                    bestIdx = i;
                }
            }
        }
    }
    if (minDist != 0x7FFFFFFF) {
        i                               = 1;
        block->impactCoord.parent       = NULL;
        block->impactCoord.composeStamp = GRAPHICS_COORD_SUPPLIED_CACHE;
        block->impactCoord.workm.t[0]   = arg0[bestIdx].point.vx;
        block->impactCoord.workm.t[1]   = arg0[bestIdx].point.vy;
        block->impactCoord.workm.t[2]   = arg0[bestIdx].point.vz;
        block->jitter.vx                = rand() & 7;
        block->jitter.vy                = rand() & 7;
        block->jitter.vz                = rand() & 7;
        if (arg2 != NULL) {
            arg2->workm.t[0] = block->impactCoord.workm.t[0] + block->jitter.vx;
            arg2->workm.t[1] = block->impactCoord.workm.t[1] + block->jitter.vy;
            arg2->workm.t[2] = block->impactCoord.workm.t[2] + block->jitter.vz;
        }
        Gp_SpawnEff(EFFECT_IMPACT_SPARK, &block->impactCoord, 0, &block->jitter);
    } else {
        i = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorWeaponImpactScratch);
    return i;
}

static void func_actor_800100_80166DD0(Task* arg0)
{
    GameActor* actor;

    actor                 = arg0->work;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = 5;
    actor->animationState = 0;
    actor->statePhase     = 0;
    actor->stateAux       = 0;
}

static void func_actor_800100_80166DF0(Task* arg0)
{
    GameActor* actor;

    actor                   = arg0->work;
    actor->state            = 6;
    actor->mode             = GAME_ACTOR_MODE_NORMAL;
    actor->animationState   = 0;
    actor->statePhase       = 0;
    actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
}

static void func_actor_800100_80166E14(Task* arg0)
{
    GameActor* actor;

    actor                   = arg0->work;
    actor->state            = 8;
    actor->animationState   = 7;
    actor->mode             = GAME_ACTOR_MODE_NORMAL;
    actor->statePhase       = 0;
    actor->targetNode       = NULL;
    actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    func_80106350(arg0, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], 0);
    playerActorPlayChildSlotsWithBlend(arg0, 8, 1, 6);
}

void func_actor_800100_80166E94(Task* arg0, s32 arg1)
{
    GameActor* actor;

    actor                   = arg0->work;
    actor->state            = 9;
    actor->stateAux         = arg1;
    actor->mode             = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode     = 0;
    actor->turnRateIndex    = 0;
    actor->animationState   = 0;
    actor->statePhase       = 0;
    actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    playerActorPlayChildSlotsWithBlend(arg0, arg1 + 0xE, 0, 1);
}

static void func_actor_800100_80166EE8(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_actor_800100_80161EC8;
    sp.funcs[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant](arg0);
}

static void func_actor_800100_80166F50(Task* arg0)
{
    GameActor*             actor;
    WorldCollisionBody*    obj;
    WorldCollisionCapsule* rec;
    GfxCoord*              src;
    Task*                  task;

    actor = arg0->work;
    task  = actor->equipmentTasks[1];
    if (task != NULL) {
        obj                         = &actor->collisionBodies[GAME_ACTOR_BODY_AIM];
        rec                         = &actor->aimShape;
        src                         = task->extra.tmd->coords;
        actor->weaponCollisionCoord = *src;
        gfxRotMatrixX(&actor->weaponCollisionCoord.workm, 0x400, GRAPHICS_ROTATION_COMPOSE);
        obj->coord           = &actor->weaponCollisionCoord;
        obj->context.capsule = &actor->aimShape;
        obj->key             = 0x60000;
        obj->flags           = WORLD_COLLISION_BODY_CAPSULE;
        obj->pos.vx          = 0;
        obj->pos.vy          = 0;
        obj->pos.vz          = 0;
        rec->ends[1].vx      = 0;
        rec->ends[1].vy      = -0x10;
        rec->ends[0].vx      = rec->ends[1].vx;
        rec->ends[1].vz      = 0x20;
        rec->ends[0].vy      = rec->ends[1].vy;
        rec->ends[0].vz      = rec->ends[1].vz + D_80112F60[gPlayerStatus.weapon];
        rec->end1Radius      = 1;
        rec->end0Radius      = 1;
        rec->contacts        = actor->aimContacts;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, obj);
        worldCollisionInitContacts(rec->contacts, 1, 0);
        obj->flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
    }
}

/// Planar distance from the coordinate origin to a nonempty contact, or 0.
///
/// Distances use world units. Optional `contactZY` needs two halfwords and
/// receives the signed coordinate bits (Z, Y). The original X, Y, Z stores
/// deliberately retain their order, with Z overwriting X.
static s32 _actor800100GetContactDistance(GfxCoord* coord, WorldCollisionContact* contact, u16* contactZY)
{
    s32 distance;

    if (contact->key.value != 0) {
        distance = func_80103D8C(coord->workm.t[0] - contact->point.vx, coord->workm.t[2] - contact->point.vz);
        if (contactZY != NULL) {
            // Retain the original repeated first-halfword write.
            contactZY[0] = contact->point.vx;
            contactZY[1] = contact->point.vy;
            contactZY[0] = contact->point.vz;
        }
    } else {
        distance = 0;
    }
    return distance;
}
