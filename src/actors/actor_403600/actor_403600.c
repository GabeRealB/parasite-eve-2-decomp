#include "actor_403600_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_403600.h"

#include "actors/actor.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/loading.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mem.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/frame_capture.h"

/// Position and texture coordinate of one vertex of a grid quad, laid out as
/// every vertex of a POLY_FT4 is.
typedef struct {
    s16 x; // Screen position, relative to the centre of the screen
    s16 y;
    u8  u; // Texel of the frame copy drawn at the vertex; `u` is relative to the vertex's page
    u8  v;
} _Actor403600GridVertex;
STATIC_ASSERT_SIZEOF(_Actor403600GridVertex, 6);

/// A quad of the grids this package distorts the screen with, in the layout of
/// a POLY_FT4.
///
/// Its texture is the copy of the frame the grid is drawn over. That copy is
/// 320 pixels wide and a texture page reaches 256, so a vertex's U is kept
/// relative to a page shifted right by its `pageN`, held in what a POLY_FT4
/// leaves as padding. Once all four vertices are placed they are brought onto
/// one page, which `tpage` names. `clut` is never set: the copy is direct
/// colour.
typedef struct {
    u_long                 tag;
    u_char                 r0, g0, b0, code;
    _Actor403600GridVertex vertex0;
    u_short                clut;
    _Actor403600GridVertex vertex1;
    u_short                tpage;
    _Actor403600GridVertex vertex2;
    u_char                 page0, page1; // Shift of the page `vertex0` and `vertex1` measure U from (0 or 0x40 pixels)
    _Actor403600GridVertex vertex3;
    u_char                 page2, page3; // The same for `vertex2` and `vertex3`
} _Actor403600GridQuad;
STATIC_ASSERT_SIZEOF(_Actor403600GridQuad, sizeof(POLY_FT4));

/// The x and y of a grid vertex as the one word a primitive keeps them in, for
/// copying both at once.
///
/// `vertex` is an `_Actor403600GridVertex` lvalue at a word-aligned address,
/// which every vertex of an `_Actor403600GridQuad` is. It is named once and
/// not evaluated beyond taking its address; the result is a `u32` lvalue with
/// `x` in the low half.
#define ACTOR_403600_GRID_VERTEX_XY_WORD(vertex) (*(u32*)&(vertex).x)

/// Scratch block the chains hanging from the boss are laid out in.
///
/// Positions and directions are world-oriented and relative to the view, the
/// frame the chain joints are kept in. Nothing reads or writes the `pad` runs,
/// whose role is unproven.
typedef struct {
    SVECTOR segment;   // The segment being placed: its offset from the joint before it, or its direction while its part is turned
    SVECTOR aux;       // Second vector of the step at hand: a part's position, the pull on the chain, or the axis a basis is built about
    MATRIX  basis;     // The transposed view rotation, then the basis a segment's part is turned by
    MATRIX  rot;       // The transposed rotation of the part a segment hangs from, then the segment's own rotation as it is composed
    byte    pad_50[0x10];
    SVECTOR dirs[3];   // Unit direction (4096 = 1) of each segment placed this frame
    byte    pad_78[8];
    SVECTOR sweepPull; // The body chain's pull weighted by the boss's `chainSweep`, added to the limb segments while that is set
} _Actor403600ChainScratch;
STATIC_ASSERT_SIZEOF(_Actor403600ChainScratch, 0x88);

/// Scratch block the screen distortion grid is built in.
///
/// Nothing reads or writes `pad_0`, whose role is unproven.
typedef struct {
    byte    pad_0[0x10];
    s32     otz;      // Ordering-table slot each quad is linked at; always 0, the front
    SVECTOR field_14; // Zeroed and handed to every vertex placement, which never reads it; role unproven
} _Actor403600ScreenDistortionScratch;
STATIC_ASSERT_SIZEOF(_Actor403600ScreenDistortionScratch, 0x1C);

/// Scratch block the radial distortion grid is built in: twelve sectors about
/// the effect's Y axis, each sixteen quads long from the centre outwards.
///
/// Nothing reads or writes the `pad` runs, whose role is unproven.
typedef struct {
    s32     dp;             // Depth cue of the last projection; never read
    s32     flag;           // GTE flag of the last projection; negative when it failed
    s32     otz;            // A quarter of the projected vertex's view depth, then the ordering-table slot its quad is linked at
    s32     nclip;          // Winding of the probe points on screen, which tells the side the grid is seen from; positive reverses the texel displacement
    s32     sxy;            // Packed screen position of the projected vertex
    SVECTOR localVertex;    // The vertex in its sector's frame: distance from the centre along x, the wave's lift along y
    SVECTOR texelOffset;    // Direction the vertex's texel is displaced in, then that displacement in pixels
    s32     maxOtz;         // Highest ordering-table slot a quad was linked at; the frame copy is queued just behind it
    MATRIX  sectorMatrix;   // The effect's transform turned about its Y axis to the sector being built
    SVECTOR probePoints[3]; // The effect's origin and a step of 0x1000 along its Z and its X axis, projected to find `nclip`
    byte    pad_60[8];
    s32     probeSxy[3];    // Screen positions of the probe points; never read
    byte    pad_74[4];
} _Actor403600RadialGridScratch;
STATIC_ASSERT_SIZEOF(_Actor403600RadialGridScratch, 0x78);

/// Scratch block a projectile is steered and drawn in.
///
/// Nothing reads or writes `pad_28`, whose role is unproven.
typedef struct {
    VECTOR  target;         // Position of the player's part the projectile homes on, in the frame the projectile moves in
    SVECTOR dir;            // That part relative to the view, then the steering direction, then the frame's step
    DVECTOR sxy;            // Screen position of the trail point being drawn
    s32     dp;             // Depth cue of that point; never read
    s32     flag;           // GTE flag of the projection; negative when it failed
    s32     otz;            // A quarter of that point's view depth
    byte    pad_28[4];
    SVECTOR cornerOffset;   // Offset from the point to the first corner of its quad; the other corners are its quarter turns
    MATRIX  inverseViewRot; // The view rotation transposed, which takes view space back to world orientation
} _Actor403600ProjectileScratch;
STATIC_ASSERT_SIZEOF(_Actor403600ProjectileScratch, 0x54);

static const SVECTOR D_actor_403600_80131E24;
extern DamageAttack  D_actor_403600_801420F0;
extern s32           D_actor_403600_80142120[];

void func_actor_403600_80134398(Task* arg0);

static const SVECTOR D_actor_403600_80131E2C;
static const CVECTOR _gActor403600NeutralLightColor;

static void func_actor_403600_80132A18(Task* arg0, Actor403600Work* work, Actor403600FxWork* fx);

void func_actor_403600_80134288(Task*);

extern DamageAttack D_actor_403600_801420D4[5];

DamageAttack D_actor_403600_801420D4[5] = {
    { 35, 0 },
    { 25, 2 },
    { 74, 0 },
    { 74, 8 },
    { 35, 0 },
};

DamageAttack D_actor_403600_801420E8[2] = {
    { 25, 3 },
    { 25, 1 },
};

DamageAttack D_actor_403600_801420F0 = { 32, 8 };

DamageAttack D_actor_403600_801420F4[3] = {
    { 32, 2 },
    { 32, 11 },
    { 32, 10 },
};

EnemyParams D_actor_403600_80142100[2] = {
    { D_actor_403600_801420D4, 6000, 5000, 0x2710, 100, 100, 7, 100, 4 },
    { D_actor_403600_801420E8, 350, 300, 1000, 0, 100, 0, 0, 0 },
};

s32 D_actor_403600_80142120[32] = {
    0xFFFFFF,
    0xE0E0FF,
    0xC0C0FF,
    0xA0A0FF,
    0x8080FF,
    0x6060FF,
    0x6060FF,
    0x6060FF,
    0x6060FF,
    0x6060FF,
    0x6060FF,
    0x6060FF,
    0x6080FF,
    0x60A0FF,
    0x60C0FF,
    0x60E0FF,
    0x60FFFF,
    0x80E0E0,
    0xA0D0C0,
    0xC0C080,
    0xA0B060,
    0x80A060,
    0x809060,
    0x808060,
    0x807060,
    0x706050,
    0x605040,
    0x504030,
    0x403020,
    0x302010,
    0x201000,
    0x100000,
};

void        func_actor_403600_80134288(Task*);
void        func_actor_403600_80134398(Task*);
static void _actor403600RippleTask(Task* task);

TaskDesc D_actor_403600_801421A0[4] = {
    { { { TASK_BODY_NONE, 95 } }, func_actor_403600_80134288, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_403600_80134398, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, actor403600LoosePartsTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 112 } }, _actor403600RippleTask, { .value = 0 } },
};

static TmdBone _gActor403600EveBodySkeleton[20] = {
#include "assets/eve_body_skeleton.inc"
};

static u32 _gActor403600EveBodyPartVerts[20] = {
#include "assets/eve_body_partVerts.inc"
};

static SVECTOR _gActor403600EveBodyVerts[458] = {
#include "assets/eve_body_verts.inc"
};

static SVECTOR _gActor403600EveBodyNormals[453] = {
#include "assets/eve_body_normals.inc"
};

static u32 _gActor403600EveBodyStream[5548] = {
#include "assets/eve_body_stream.inc"
};

TmdSource gActor403600EveBody = {
    0,
    30316,
    8016,
    20,
    _gActor403600EveBodyPartVerts,
    _gActor403600EveBodyVerts,
    _gActor403600EveBodyNormals,
    _gActor403600EveBodySkeleton,
    _gActor403600EveBodyStream,
};

static TmdBone _gActor403600Model199B8Skeleton[20] = {
#include "assets/actor_403600_model_199B8_skeleton.inc"
};

static u32 _gActor403600Model199B8PartVerts[20] = {
#include "assets/actor_403600_model_199B8_partVerts.inc"
};

static SVECTOR _gActor403600Model199B8Verts[458] = {
#include "assets/actor_403600_model_199B8_verts.inc"
};

static SVECTOR _gActor403600Model199B8Normals[453] = {
#include "assets/actor_403600_model_199B8_normals.inc"
};

static u32 _gActor403600Model199B8Stream[5545] = {
#include "assets/actor_403600_model_199B8_stream.inc"
};

TmdSource gActor403600Model199B8 = {
    0,
    30316,
    8016,
    20,
    _gActor403600Model199B8PartVerts,
    _gActor403600Model199B8Verts,
    _gActor403600Model199B8Normals,
    _gActor403600Model199B8Skeleton,
    _gActor403600Model199B8Stream,
};

DamageAttack D_actor_403600_80150E9C = { 35, 0 };

DamageAttack D_actor_403600_80150EA0 = { 25, 2 };

u16 D_actor_403600_80150EA4 = 74;

DamageAttack D_actor_403600_80150EA8 = { 74, 8 };

u16 D_actor_403600_80150EAC = 35;

DamageAttack D_actor_403600_80150EB0 = { 25, 3 };

DamageAttack D_actor_403600_80150EB4[5] = {
    { 25, 1 },
    { 32, 8 },
    { 32, 2 },
    { 32, 11 },
    { 32, 10 },
};

EnemyParams D_actor_403600_80150EC8 = { &D_actor_403600_80150E9C, 6000, 5000, 0x2710, 100, 100, 7, 100, 4 };

EnemyParams D_actor_403600_80150ED8 = { &D_actor_403600_80150EB0, 350, 300, 1000, 0, 100, 0, 0, 0 };

static AnimationPackedPose _gActor403600Animation1FEB0Bank1[16] = {
#include "assets/actor_403600_animation_1FEB0_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation1FEB0Bank4[304] = {
#include "assets/actor_403600_animation_1FEB0_bank4.inc"
};

static AnimationRecord _gActor403600Animation1FEB0Records[528] = {
#include "assets/actor_403600_animation_1FEB0_records.inc"
};

static u16 _gActor403600Animation1FEB0Indices[20] = {
#include "assets/actor_403600_animation_1FEB0_indices.inc"
};

AnimationSet gActor403600Animation1FEB0 = {
    _gActor403600Animation1FEB0Records,
    _gActor403600Animation1FEB0Indices,
    { NULL, _gActor403600Animation1FEB0Bank1, NULL, NULL, _gActor403600Animation1FEB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation208ECBank1[14] = {
#include "assets/actor_403600_animation_208EC_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation208ECBank4[251] = {
#include "assets/actor_403600_animation_208EC_bank4.inc"
};

static AnimationRecord _gActor403600Animation208ECRecords[342] = {
#include "assets/actor_403600_animation_208EC_records.inc"
};

static u16 _gActor403600Animation208ECIndices[20] = {
#include "assets/actor_403600_animation_208EC_indices.inc"
};

AnimationSet gActor403600Animation208EC = {
    _gActor403600Animation208ECRecords,
    _gActor403600Animation208ECIndices,
    { NULL, _gActor403600Animation208ECBank1, NULL, NULL, _gActor403600Animation208ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation2139CBank1[15] = {
#include "assets/actor_403600_animation_2139C_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation2139CBank4[234] = {
#include "assets/actor_403600_animation_2139C_bank4.inc"
};

static AnimationRecord _gActor403600Animation2139CRecords[385] = {
#include "assets/actor_403600_animation_2139C_records.inc"
};

static u16 _gActor403600Animation2139CIndices[20] = {
#include "assets/actor_403600_animation_2139C_indices.inc"
};

AnimationSet gActor403600Animation2139C = {
    _gActor403600Animation2139CRecords,
    _gActor403600Animation2139CIndices,
    { NULL, _gActor403600Animation2139CBank1, NULL, NULL, _gActor403600Animation2139CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation22424Bank1[23] = {
#include "assets/actor_403600_animation_22424_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation22424Bank4[348] = {
#include "assets/actor_403600_animation_22424_bank4.inc"
};

static AnimationRecord _gActor403600Animation22424Records[621] = {
#include "assets/actor_403600_animation_22424_records.inc"
};

static u16 _gActor403600Animation22424Indices[20] = {
#include "assets/actor_403600_animation_22424_indices.inc"
};

AnimationSet gActor403600Animation22424 = {
    _gActor403600Animation22424Records,
    _gActor403600Animation22424Indices,
    { NULL, _gActor403600Animation22424Bank1, NULL, NULL, _gActor403600Animation22424Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation22904Bank1[8] = {
#include "assets/actor_403600_animation_22904_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation22904Bank4[114] = {
#include "assets/actor_403600_animation_22904_bank4.inc"
};

static AnimationRecord _gActor403600Animation22904Records[154] = {
#include "assets/actor_403600_animation_22904_records.inc"
};

static u16 _gActor403600Animation22904Indices[20] = {
#include "assets/actor_403600_animation_22904_indices.inc"
};

AnimationSet gActor403600Animation22904 = {
    _gActor403600Animation22904Records,
    _gActor403600Animation22904Indices,
    { NULL, _gActor403600Animation22904Bank1, NULL, NULL, _gActor403600Animation22904Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation237A8Bank1[22] = {
#include "assets/actor_403600_animation_237A8_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation237A8Bank4[355] = {
#include "assets/actor_403600_animation_237A8_bank4.inc"
};

static AnimationRecord _gActor403600Animation237A8Records[496] = {
#include "assets/actor_403600_animation_237A8_records.inc"
};

static u16 _gActor403600Animation237A8Indices[20] = {
#include "assets/actor_403600_animation_237A8_indices.inc"
};

AnimationSet gActor403600Animation237A8 = {
    _gActor403600Animation237A8Records,
    _gActor403600Animation237A8Indices,
    { NULL, _gActor403600Animation237A8Bank1, NULL, NULL, _gActor403600Animation237A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation241DCBank1[18] = {
#include "assets/actor_403600_animation_241DC_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation241DCBank4[259] = {
#include "assets/actor_403600_animation_241DC_bank4.inc"
};

static AnimationRecord _gActor403600Animation241DCRecords[320] = {
#include "assets/actor_403600_animation_241DC_records.inc"
};

static u16 _gActor403600Animation241DCIndices[20] = {
#include "assets/actor_403600_animation_241DC_indices.inc"
};

AnimationSet gActor403600Animation241DC = {
    _gActor403600Animation241DCRecords,
    _gActor403600Animation241DCIndices,
    { NULL, _gActor403600Animation241DCBank1, NULL, NULL, _gActor403600Animation241DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation24C54Bank1[12] = {
#include "assets/actor_403600_animation_24C54_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation24C54Bank4[252] = {
#include "assets/actor_403600_animation_24C54_bank4.inc"
};

static AnimationRecord _gActor403600Animation24C54Records[362] = {
#include "assets/actor_403600_animation_24C54_records.inc"
};

static u16 _gActor403600Animation24C54Indices[20] = {
#include "assets/actor_403600_animation_24C54_indices.inc"
};

AnimationSet gActor403600Animation24C54 = {
    _gActor403600Animation24C54Records,
    _gActor403600Animation24C54Indices,
    { NULL, _gActor403600Animation24C54Bank1, NULL, NULL, _gActor403600Animation24C54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation25670Bank1[12] = {
#include "assets/actor_403600_animation_25670_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation25670Bank4[252] = {
#include "assets/actor_403600_animation_25670_bank4.inc"
};

static AnimationRecord _gActor403600Animation25670Records[339] = {
#include "assets/actor_403600_animation_25670_records.inc"
};

static u16 _gActor403600Animation25670Indices[20] = {
#include "assets/actor_403600_animation_25670_indices.inc"
};

AnimationSet gActor403600Animation25670 = {
    _gActor403600Animation25670Records,
    _gActor403600Animation25670Indices,
    { NULL, _gActor403600Animation25670Bank1, NULL, NULL, _gActor403600Animation25670Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation26650Bank1[23] = {
#include "assets/actor_403600_animation_26650_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation26650Bank4[419] = {
#include "assets/actor_403600_animation_26650_bank4.inc"
};

static AnimationRecord _gActor403600Animation26650Records[508] = {
#include "assets/actor_403600_animation_26650_records.inc"
};

static u16 _gActor403600Animation26650Indices[20] = {
#include "assets/actor_403600_animation_26650_indices.inc"
};

AnimationSet gActor403600Animation26650 = {
    _gActor403600Animation26650Records,
    _gActor403600Animation26650Indices,
    { NULL, _gActor403600Animation26650Bank1, NULL, NULL, _gActor403600Animation26650Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation27184Bank1[17] = {
#include "assets/actor_403600_animation_27184_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation27184Bank4[297] = {
#include "assets/actor_403600_animation_27184_bank4.inc"
};

static AnimationRecord _gActor403600Animation27184Records[349] = {
#include "assets/actor_403600_animation_27184_records.inc"
};

static u16 _gActor403600Animation27184Indices[20] = {
#include "assets/actor_403600_animation_27184_indices.inc"
};

AnimationSet gActor403600Animation27184 = {
    _gActor403600Animation27184Records,
    _gActor403600Animation27184Indices,
    { NULL, _gActor403600Animation27184Bank1, NULL, NULL, _gActor403600Animation27184Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation27A60Bank1[15] = {
#include "assets/actor_403600_animation_27A60_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation27A60Bank4[230] = {
#include "assets/actor_403600_animation_27A60_bank4.inc"
};

static AnimationRecord _gActor403600Animation27A60Records[272] = {
#include "assets/actor_403600_animation_27A60_records.inc"
};

static u16 _gActor403600Animation27A60Indices[20] = {
#include "assets/actor_403600_animation_27A60_indices.inc"
};

AnimationSet gActor403600Animation27A60 = {
    _gActor403600Animation27A60Records,
    _gActor403600Animation27A60Indices,
    { NULL, _gActor403600Animation27A60Bank1, NULL, NULL, _gActor403600Animation27A60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation283F8Bank1[16] = {
#include "assets/actor_403600_animation_283F8_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation283F8Bank4[252] = {
#include "assets/actor_403600_animation_283F8_bank4.inc"
};

static AnimationRecord _gActor403600Animation283F8Records[294] = {
#include "assets/actor_403600_animation_283F8_records.inc"
};

static u16 _gActor403600Animation283F8Indices[20] = {
#include "assets/actor_403600_animation_283F8_indices.inc"
};

AnimationSet gActor403600Animation283F8 = {
    _gActor403600Animation283F8Records,
    _gActor403600Animation283F8Indices,
    { NULL, _gActor403600Animation283F8Bank1, NULL, NULL, _gActor403600Animation283F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation29234Bank1[17] = {
#include "assets/actor_403600_animation_29234_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation29234Bank4[359] = {
#include "assets/actor_403600_animation_29234_bank4.inc"
};

static AnimationRecord _gActor403600Animation29234Records[481] = {
#include "assets/actor_403600_animation_29234_records.inc"
};

static u16 _gActor403600Animation29234Indices[20] = {
#include "assets/actor_403600_animation_29234_indices.inc"
};

AnimationSet gActor403600Animation29234 = {
    _gActor403600Animation29234Records,
    _gActor403600Animation29234Indices,
    { NULL, _gActor403600Animation29234Bank1, NULL, NULL, _gActor403600Animation29234Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation29674Bank1[6] = {
#include "assets/actor_403600_animation_29674_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation29674Bank4[101] = {
#include "assets/actor_403600_animation_29674_bank4.inc"
};

static AnimationRecord _gActor403600Animation29674Records[133] = {
#include "assets/actor_403600_animation_29674_records.inc"
};

static u16 _gActor403600Animation29674Indices[20] = {
#include "assets/actor_403600_animation_29674_indices.inc"
};

AnimationSet gActor403600Animation29674 = {
    _gActor403600Animation29674Records,
    _gActor403600Animation29674Indices,
    { NULL, _gActor403600Animation29674Bank1, NULL, NULL, _gActor403600Animation29674Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation29CB4Bank1[12] = {
#include "assets/actor_403600_animation_29CB4_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation29CB4Bank4[148] = {
#include "assets/actor_403600_animation_29CB4_bank4.inc"
};

static AnimationRecord _gActor403600Animation29CB4Records[196] = {
#include "assets/actor_403600_animation_29CB4_records.inc"
};

static u16 _gActor403600Animation29CB4Indices[20] = {
#include "assets/actor_403600_animation_29CB4_indices.inc"
};

AnimationSet gActor403600Animation29CB4 = {
    _gActor403600Animation29CB4Records,
    _gActor403600Animation29CB4Indices,
    { NULL, _gActor403600Animation29CB4Bank1, NULL, NULL, _gActor403600Animation29CB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation2A708Bank1[19] = {
#include "assets/actor_403600_animation_2A708_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation2A708Bank4[256] = {
#include "assets/actor_403600_animation_2A708_bank4.inc"
};

static AnimationRecord _gActor403600Animation2A708Records[328] = {
#include "assets/actor_403600_animation_2A708_records.inc"
};

static u16 _gActor403600Animation2A708Indices[20] = {
#include "assets/actor_403600_animation_2A708_indices.inc"
};

AnimationSet gActor403600Animation2A708 = {
    _gActor403600Animation2A708Records,
    _gActor403600Animation2A708Indices,
    { NULL, _gActor403600Animation2A708Bank1, NULL, NULL, _gActor403600Animation2A708Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation2A8ECBank1[2] = {
#include "assets/actor_403600_animation_2A8EC_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation2A8ECBank4[15] = {
#include "assets/actor_403600_animation_2A8EC_bank4.inc"
};

static AnimationRecord _gActor403600Animation2A8ECRecords[80] = {
#include "assets/actor_403600_animation_2A8EC_records.inc"
};

static u16 _gActor403600Animation2A8ECIndices[20] = {
#include "assets/actor_403600_animation_2A8EC_indices.inc"
};

AnimationSet gActor403600Animation2A8EC = {
    _gActor403600Animation2A8ECRecords,
    _gActor403600Animation2A8ECIndices,
    { NULL, _gActor403600Animation2A8ECBank1, NULL, NULL, _gActor403600Animation2A8ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation2B364Bank1[13] = {
#include "assets/actor_403600_animation_2B364_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation2B364Bank4[242] = {
#include "assets/actor_403600_animation_2B364_bank4.inc"
};

static AnimationRecord _gActor403600Animation2B364Records[369] = {
#include "assets/actor_403600_animation_2B364_records.inc"
};

static u16 _gActor403600Animation2B364Indices[20] = {
#include "assets/actor_403600_animation_2B364_indices.inc"
};

AnimationSet gActor403600Animation2B364 = {
    _gActor403600Animation2B364Records,
    _gActor403600Animation2B364Indices,
    { NULL, _gActor403600Animation2B364Bank1, NULL, NULL, _gActor403600Animation2B364Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation2BF8CBank1[23] = {
#include "assets/actor_403600_animation_2BF8C_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation2BF8CBank4[309] = {
#include "assets/actor_403600_animation_2BF8C_bank4.inc"
};

static AnimationRecord _gActor403600Animation2BF8CRecords[380] = {
#include "assets/actor_403600_animation_2BF8C_records.inc"
};

static u16 _gActor403600Animation2BF8CIndices[20] = {
#include "assets/actor_403600_animation_2BF8C_indices.inc"
};

AnimationSet gActor403600Animation2BF8C = {
    _gActor403600Animation2BF8CRecords,
    _gActor403600Animation2BF8CIndices,
    { NULL, _gActor403600Animation2BF8CBank1, NULL, NULL, _gActor403600Animation2BF8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation2C0C4Bank1[2] = {
#include "assets/actor_403600_animation_2C0C4_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation2C0C4Bank4[14] = {
#include "assets/actor_403600_animation_2C0C4_bank4.inc"
};

static AnimationRecord _gActor403600Animation2C0C4Records[38] = {
#include "assets/actor_403600_animation_2C0C4_records.inc"
};

static u16 _gActor403600Animation2C0C4Indices[20] = {
#include "assets/actor_403600_animation_2C0C4_indices.inc"
};

AnimationSet gActor403600Animation2C0C4 = {
    _gActor403600Animation2C0C4Records,
    _gActor403600Animation2C0C4Indices,
    { NULL, _gActor403600Animation2C0C4Bank1, NULL, NULL, _gActor403600Animation2C0C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation2C90CBank1[21] = {
#include "assets/actor_403600_animation_2C90C_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation2C90CBank4[193] = {
#include "assets/actor_403600_animation_2C90C_bank4.inc"
};

static AnimationRecord _gActor403600Animation2C90CRecords[254] = {
#include "assets/actor_403600_animation_2C90C_records.inc"
};

static u16 _gActor403600Animation2C90CIndices[20] = {
#include "assets/actor_403600_animation_2C90C_indices.inc"
};

AnimationSet gActor403600Animation2C90C = {
    _gActor403600Animation2C90CRecords,
    _gActor403600Animation2C90CIndices,
    { NULL, _gActor403600Animation2C90CBank1, NULL, NULL, _gActor403600Animation2C90CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation2D0A8Bank1[20] = {
#include "assets/actor_403600_animation_2D0A8_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation2D0A8Bank4[172] = {
#include "assets/actor_403600_animation_2D0A8_bank4.inc"
};

static AnimationRecord _gActor403600Animation2D0A8Records[235] = {
#include "assets/actor_403600_animation_2D0A8_records.inc"
};

static u16 _gActor403600Animation2D0A8Indices[20] = {
#include "assets/actor_403600_animation_2D0A8_indices.inc"
};

AnimationSet gActor403600Animation2D0A8 = {
    _gActor403600Animation2D0A8Records,
    _gActor403600Animation2D0A8Indices,
    { NULL, _gActor403600Animation2D0A8Bank1, NULL, NULL, _gActor403600Animation2D0A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation2D8B4Bank1[15] = {
#include "assets/actor_403600_animation_2D8B4_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation2D8B4Bank4[206] = {
#include "assets/actor_403600_animation_2D8B4_bank4.inc"
};

static AnimationRecord _gActor403600Animation2D8B4Records[244] = {
#include "assets/actor_403600_animation_2D8B4_records.inc"
};

static u16 _gActor403600Animation2D8B4Indices[20] = {
#include "assets/actor_403600_animation_2D8B4_indices.inc"
};

AnimationSet gActor403600Animation2D8B4 = {
    _gActor403600Animation2D8B4Records,
    _gActor403600Animation2D8B4Indices,
    { NULL, _gActor403600Animation2D8B4Bank1, NULL, NULL, _gActor403600Animation2D8B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation2E0C8Bank1[14] = {
#include "assets/actor_403600_animation_2E0C8_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation2E0C8Bank4[207] = {
#include "assets/actor_403600_animation_2E0C8_bank4.inc"
};

static AnimationRecord _gActor403600Animation2E0C8Records[248] = {
#include "assets/actor_403600_animation_2E0C8_records.inc"
};

static u16 _gActor403600Animation2E0C8Indices[20] = {
#include "assets/actor_403600_animation_2E0C8_indices.inc"
};

AnimationSet gActor403600Animation2E0C8 = {
    _gActor403600Animation2E0C8Records,
    _gActor403600Animation2E0C8Indices,
    { NULL, _gActor403600Animation2E0C8Bank1, NULL, NULL, _gActor403600Animation2E0C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403600Animation2E6BCBank1[9] = {
#include "assets/actor_403600_animation_2E6BC_bank1.inc"
};

static AnimationPackedRotation _gActor403600Animation2E6BCBank4[118] = {
#include "assets/actor_403600_animation_2E6BC_bank4.inc"
};

static AnimationRecord _gActor403600Animation2E6BCRecords[216] = {
#include "assets/actor_403600_animation_2E6BC_records.inc"
};

static u16 _gActor403600Animation2E6BCIndices[20] = {
#include "assets/actor_403600_animation_2E6BC_indices.inc"
};

AnimationSet gActor403600Animation2E6BC = {
    _gActor403600Animation2E6BCRecords,
    _gActor403600Animation2E6BCIndices,
    { NULL, _gActor403600Animation2E6BCBank1, NULL, NULL, _gActor403600Animation2E6BCBank4, NULL, NULL, NULL },
};

/// Scratch-pad frame of a triangle pass that works in the frame of a reference
/// coordinate: the GTE state the pass restores on exit, the model-to-reference
/// matrix it builds, and the element being worked on.
typedef struct {
    VECTOR  trans;    // GTE translation on entry
    SVECTOR offset;   // Entry translation relative to the reference, rotated into its frame
    SVECTOR verts[3]; // The element's vertices in the reference frame, clamped to y <= 0
    s32     index[3]; // The element's vertex indices
    MATRIX  savedRot; // GTE rotation on entry
    MATRIX  local;    // Model-to-reference transform
} _Actor403600TriScratch;

/// Scratch-pad frame of a quad pass that draws the model in the frame of a
/// reference coordinate: the GTE state the pass restores on exit, the
/// model-to-reference matrix it builds, and the element being drawn.
typedef struct {
    VECTOR  trans;    // GTE translation on entry
    SVECTOR offset;   // Entry translation relative to the reference, rotated into its frame
    SVECTOR verts[4]; // The element's vertices in the reference frame, clamped to y <= 0
    s32     index[4]; // The element's vertex indices
    MATRIX  savedRot; // GTE rotation on entry
    MATRIX  local;    // Model-to-reference transform
} _Actor403600QuadScratch;

/// Pixel boundary and Q7 colour weight of the package's vertical screen fades.
enum {
    ACTOR_403600_SCREEN_FADE_ORIGIN_Y    = 360,
    ACTOR_403600_SCREEN_FADE_COLOR_ONE   = 128,
    ACTOR_403600_SCREEN_FADE_COLOR_SHIFT = 7,
    ACTOR_403600_GEOMETRY_OFFSET_SHIFT   = 3,      // Serialized eight-byte SVECTOR offsets to element indices
    ACTOR_403600_GEOMETRY_OFFSET_MASK    = 0xFFF8, // Strip low flag bits from serialized eight-byte SVECTOR offsets
    ACTOR_403600_DEPTH_OFFSET_MASK       = 0xFFFC, // Strip low flag bits from serialized four-byte depth-cache offsets
    ACTOR_403600_OT_DEPTH_SHIFT          = 4,      // Sixteen camera-depth units per OT bucket before the display shift
    ACTOR_403600_OT_INDEX_MASK           = 0x3FF   // Wrap relative OT buckets into 0..1023
};

static void        _actor403600UnifyGridQuadTexturePage(_Actor403600GridQuad* quad);
static void        _actor403600PlaceDistortionVertex(_Actor403600GridQuad* quad, s32 corner, SVECTOR* unusedVector, s32 distortion);
static inline void _actor403600RotateSv(const MATRIX* rotationMatrix, const SVECTOR* input, SVECTOR* output);
static inline void _actor403600TickRipple(Actor403600Ripple* state);
static inline s32  _actor403600RippleHasDiedOut(const Actor403600Ripple* state);

/// Lights GT3 corner normals using a common screen-fade RGB weight.
///
/// Arguments must have no side effects; colour, workspace and offsets are used
/// repeatedly. `lightColor` is a CVECTOR lvalue, `fadeAmount` a loss in 0..128,
/// and `normalBytes` a writable const-byte-pointer lvalue receiving the borrowed
/// normal-array view. Requires complete serialized offsets, writable packet RGB,
/// and loaded GTE light/colour matrices. Retains no pointer and overwrites GTE
/// normal, RGB, MAC, IR and FLAG state. Use as a standalone statement inside
/// a braced block.
#define ACTOR_403600_LIGHT_GT3_SCREEN_FADE(workspace, lightColor, fadeAmount, offsets, packet, normalBytes) \
    {                                                                                                       \
        (lightColor).r = -ACTOR_403600_SCREEN_FADE_COLOR_ONE - (fadeAmount);                                \
        (lightColor).g = -ACTOR_403600_SCREEN_FADE_COLOR_ONE - (fadeAmount);                                \
        (lightColor).b = -ACTOR_403600_SCREEN_FADE_COLOR_ONE - (fadeAmount);                                \
        gte_ldrgb(&(lightColor));                                                                           \
        (normalBytes) = (const u8*)(workspace)->normals;                                                    \
        gte_ldv3((normalBytes) + ((offsets)[3] & ACTOR_403600_GEOMETRY_OFFSET_MASK),                        \
                 (normalBytes) + ((offsets)[4] & ACTOR_403600_GEOMETRY_OFFSET_MASK),                        \
                 (normalBytes) + ((offsets)[5] & ACTOR_403600_GEOMETRY_OFFSET_MASK));                       \
        gte_ncct();                                                                                         \
        gte_strgb3_gt3(packet);                                                                             \
    }

/// Lights GT4 corner normals using a common screen-fade RGB weight.
///
/// Arguments must have no side effects; colour, workspace and offsets are used
/// repeatedly. `lightColor` is a CVECTOR lvalue, `fadeAmount` a loss in 0..128,
/// and `normalBytes` a writable const-byte-pointer lvalue receiving the borrowed
/// normal-array view. Requires complete serialized offsets, writable packet RGB,
/// and loaded GTE light/colour matrices. Retains no pointer and overwrites GTE
/// normal, RGB, MAC, IR and FLAG state. Use as a standalone statement inside
/// a braced block.
#define ACTOR_403600_LIGHT_GT4_SCREEN_FADE(workspace, lightColor, fadeAmount, offsets, packet, normalBytes) \
    {                                                                                                       \
        (lightColor).r = -ACTOR_403600_SCREEN_FADE_COLOR_ONE - (fadeAmount);                                \
        (lightColor).g = -ACTOR_403600_SCREEN_FADE_COLOR_ONE - (fadeAmount);                                \
        (lightColor).b = -ACTOR_403600_SCREEN_FADE_COLOR_ONE - (fadeAmount);                                \
        gte_ldrgb(&(lightColor));                                                                           \
        (normalBytes) = (const u8*)(workspace)->normals;                                                    \
        gte_ldv3((normalBytes) + ((offsets)[4] & ACTOR_403600_GEOMETRY_OFFSET_MASK),                        \
                 (normalBytes) + ((offsets)[5] & ACTOR_403600_GEOMETRY_OFFSET_MASK),                        \
                 (normalBytes) + ((offsets)[6] & ACTOR_403600_GEOMETRY_OFFSET_MASK));                       \
        gte_ncct();                                                                                         \
        gte_strgb3_gt4(packet);                                                                             \
        gte_ldv0((const u8*)(workspace)->normals + ((offsets)[7] & ACTOR_403600_GEOMETRY_OFFSET_MASK));     \
        gte_nccs();                                                                                         \
        gte_strgb(&(packet)->r3);                                                                           \
    }

/// Fades pre-transformed GT3 RGB channels using the remaining Q7 weight.
///
/// `packet` must be a side-effect-free POLY_GT3 pointer; it is evaluated
/// repeatedly. `fadeAmount` is a side-effect-free writable s32 lvalue, initially
/// a loss in 0..128 and changed to 128 minus that loss. Channel stores truncate
/// to bytes after shifting products right by 7. Use as a standalone statement
/// inside a braced block.
#define ACTOR_403600_FADE_PRE_XFORM_GT3_COLORS(packet, fadeAmount)                            \
    {                                                                                         \
        s32 fadedChannel;                                                                     \
                                                                                              \
        (fadeAmount) = ACTOR_403600_SCREEN_FADE_COLOR_ONE - (fadeAmount);                     \
        fadedChannel = ((packet)->r0 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->r0 = fadedChannel;                                                          \
        fadedChannel = ((packet)->g0 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->g0 = fadedChannel;                                                          \
        fadedChannel = ((packet)->b0 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->b0 = fadedChannel;                                                          \
        fadedChannel = ((packet)->r1 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->r1 = fadedChannel;                                                          \
        fadedChannel = ((packet)->g1 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->g1 = fadedChannel;                                                          \
        fadedChannel = ((packet)->b1 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->b1 = fadedChannel;                                                          \
        fadedChannel = ((packet)->r2 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->r2 = fadedChannel;                                                          \
        fadedChannel = ((packet)->g2 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->g2 = fadedChannel;                                                          \
        fadedChannel = ((packet)->b2 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->b2 = fadedChannel;                                                          \
    }

/// Fades pre-transformed GT4 RGB channels using the remaining Q7 weight.
///
/// `packet` must be a side-effect-free POLY_GT4 pointer; it is evaluated
/// repeatedly. `fadeAmount` is a side-effect-free writable s32 lvalue, initially
/// a loss in 0..128 and changed to 128 minus that loss. Channel stores truncate
/// to bytes after shifting products right by 7. Use as a standalone statement
/// inside a braced block.
/// Leaves corner 1 unchanged, fades its RGB into corner 3, then fades corner 3
/// again. Corners 0 and 2 fade once, preserving the packet writer's order.
#define ACTOR_403600_FADE_PRE_XFORM_GT4_COLORS(packet, fadeAmount)                            \
    {                                                                                         \
        s32 fadedChannel;                                                                     \
                                                                                              \
        (fadeAmount) = ACTOR_403600_SCREEN_FADE_COLOR_ONE - (fadeAmount);                     \
        fadedChannel = ((packet)->r0 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->r0 = fadedChannel;                                                          \
        fadedChannel = ((packet)->g0 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->g0 = fadedChannel;                                                          \
        fadedChannel = ((packet)->b0 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->b0 = fadedChannel;                                                          \
        fadedChannel = ((packet)->r1 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->r3 = fadedChannel;                                                          \
        fadedChannel = ((packet)->g1 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->g3 = fadedChannel;                                                          \
        fadedChannel = ((packet)->b1 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->b3 = fadedChannel;                                                          \
        fadedChannel = ((packet)->r2 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->r2 = fadedChannel;                                                          \
        fadedChannel = ((packet)->g2 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->g2 = fadedChannel;                                                          \
        fadedChannel = ((packet)->b2 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->b2 = fadedChannel;                                                          \
        fadedChannel = ((packet)->r3 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->r3 = fadedChannel;                                                          \
        fadedChannel = ((packet)->g3 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->g3 = fadedChannel;                                                          \
        fadedChannel = ((packet)->b3 * (fadeAmount)) >> ACTOR_403600_SCREEN_FADE_COLOR_SHIFT; \
        (packet)->b3 = fadedChannel;                                                          \
    }

/// Transforms a vertex by the loaded model-to-plane matrix, then clamps its Y.
///
/// `source` addresses a readable, word-aligned SVECTOR; `planeVertex` is a
/// writable SVECTOR lvalue with halfword-aligned xyz. Its address expression
/// must have no side effects: it is evaluated three times. Overwrites GTE V0, MAC,
/// IR and FLAG, with no scratch-stack use or pointer retention.
#define ACTOR_403600_TRANSFORM_AND_CLAMP_PLANE_VERTEX(source, planeVertex) \
    {                                                                      \
        gte_ldv0(source);                                                  \
        gte_rtv0tr();                                                      \
        gte_stsv(&(planeVertex));                                          \
        if ((planeVertex).vy > 0) {                                        \
            (planeVertex).vy = 0;                                          \
        }                                                                  \
    }

/// Links primitive `p` at the head of ordering-table entry `ot` through tag words.
///
/// `ot` points to one `u_long` tag and `p` to a packet with a tag word. Both
/// arguments must be side-effect-free: each is evaluated repeatedly. The link
/// preserves each tag's packet length and stores only the low 24 address bits.
#define ACTOR_403600_LINK_PRIMITIVE(ot, p)                                                     \
    ((p)->tag = ((p)->tag & GPU_DMA_PACKET_LENGTH_MASK) | (*(ot) & GPU_DMA_LINK_ADDRESS_MASK), \
     *(ot)    = (*(ot) & GPU_DMA_PACKET_LENGTH_MASK) | ((u32)(p) & GPU_DMA_LINK_ADDRESS_MASK))

#include "../../shared/frame_capture.inc.c"

/// Gives a captured-frame quad one texture page and rebases all four U bytes.
///
/// The incoming U bytes plus their per-corner page shifts are capture-relative
/// pixel coordinates in 0..319; shifts are 0 or 64. Uses the page at VRAM
/// (448,256), or (512,256) when a corner reaches 256 or all corners reach 64.
/// The latter fits all four corners only when their span lies in 64..319;
/// callers must keep each quad within one 256-pixel page. Y, V and packet
/// header are unchanged. Stores the chosen shift in every corner's page byte
/// so neighbouring quads can recover their shared vertices' capture-relative U.
static void _actor403600UnifyGridQuadTexturePage(_Actor403600GridQuad* quad)
{
    enum {
        ACTOR_403600_CAPTURE_PAGE_PIXELS  = 0x100,
        ACTOR_403600_CAPTURE_PAGE_SHIFT   = 0x40,
        ACTOR_403600_CAPTURE_X            = 0x1C0,
        ACTOR_403600_CAPTURE_Y            = 0x100,
        ACTOR_403600_CAPTURE_COLOR_DEPTH  = 2,
        ACTOR_403600_CAPTURE_PAGE_X_SHIFT = 6
    };

    s32 u3;
    s32 u2;
    s32 u1;
    s32 u0;
    s32 maxU;
    s32 minU;
    u8  pageShift;

    // Recover capture-relative U before selecting one page for all four corners.
    u0   = quad->vertex0.u + quad->page0;
    minU = u0;
    maxU = u0;
    u1   = quad->vertex1.u + quad->page1;
    u2   = quad->vertex2.u + quad->page2;
    u3   = quad->vertex3.u + quad->page3;
    if (u1 < minU) {
        minU = u1;
    } else if (maxU < u1) {
        maxU = u1;
    }
    if (u2 < minU) {
        minU = u2;
    } else if (maxU < u2) {
        maxU = u2;
    }
    if (u3 < minU) {
        minU = u3;
    } else if (maxU < u3) {
        maxU = u3;
    }
    pageShift       = ((maxU >= ACTOR_403600_CAPTURE_PAGE_PIXELS) || (minU >= ACTOR_403600_CAPTURE_PAGE_SHIFT))
                          ? ACTOR_403600_CAPTURE_PAGE_SHIFT
                          : 0;
    quad->tpage     = (s16)(((u32)(pageShift + ACTOR_403600_CAPTURE_X) >> ACTOR_403600_CAPTURE_PAGE_X_SHIFT) | getTPage(ACTOR_403600_CAPTURE_COLOR_DEPTH, GPU_BLEND_AVERAGE, 0, ACTOR_403600_CAPTURE_Y));
    quad->vertex0.u = (u8)(u0 - pageShift);
    quad->vertex1.u = (u8)(u1 - pageShift);
    quad->vertex2.u = (u8)(u2 - pageShift);
    quad->vertex3.u = (u8)(u3 - pageShift);
    quad->page3     = pageShift;
    quad->page2     = pageShift;
    quad->page1     = pageShift;
    quad->page0     = pageShift;
}

/// Jitters one distortion-grid corner and assigns its captured-screen texel.
///
/// Requires a live quad and distortion in 0..4096. Corners 0..2 select their
/// named vertex; every other value selects corner 3 (callers use 0..3). Input
/// geometry is centred at (0,0); screen pixels add (160,120). One private LCG
/// draw tests against distortion + 1024, with two more draws giving -4..3 pixel
/// jitter when selected. Clamps to 320x240, correcting geometry by the clamp
/// delta, then stores page shift 0/64 and byte UV. The vector argument is ignored.
static void _actor403600PlaceDistortionVertex(_Actor403600GridQuad* quad, s32 corner, SVECTOR* unusedVector, s32 distortion)
{
    enum {
        ACTOR_403600_DISTORTION_SCREEN_CENTER_X = 160,
        ACTOR_403600_DISTORTION_SCREEN_CENTER_Y = 120,
        ACTOR_403600_DISTORTION_JITTER_RADIUS   = 4,
        ACTOR_403600_DISTORTION_JITTER_MASK     = 7,
        ACTOR_403600_DISTORTION_RANDOM_MASK     = 0xFFF,
        ACTOR_403600_DISTORTION_BASE_CHANCE     = 1024,
    };
    _Actor403600GridVertex* vertex;
    s16                     localX;
    s16                     localY;
    s32                     screenX;
    s32                     screenY;
    s32                     jitterLeft;
    s32                     jitterTop;
    s32                     chanceState;
    s32                     xState;
    s32                     yState;
    u8*                     pageShift;

    switch (corner) {
        case 0:
            vertex    = &quad->vertex0;
            pageShift = &quad->page0;
            break;
        case 1:
            vertex    = &quad->vertex1;
            pageShift = &quad->page1;
            break;
        case 2:
            vertex    = &quad->vertex2;
            pageShift = &quad->page2;
            break;
        default:
            vertex    = &quad->vertex3;
            pageShift = &quad->page3;
            break;
    }
    localX  = vertex->x;
    localY  = vertex->y;
    screenX = localX + ACTOR_403600_DISTORTION_SCREEN_CENTER_X;
    screenY = localY + ACTOR_403600_DISTORTION_SCREEN_CENTER_Y;
    // Advance modulo 2^32, retaining signed snapshots for the masked draws.
    chanceState             = (u32)D_actor_403600_80160698 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    D_actor_403600_80160698 = chanceState;
    if (((chanceState >> 16) & ACTOR_403600_DISTORTION_RANDOM_MASK) < distortion + ACTOR_403600_DISTORTION_BASE_CHANCE) {
        jitterLeft              = localX + ACTOR_403600_DISTORTION_SCREEN_CENTER_X - ACTOR_403600_DISTORTION_JITTER_RADIUS;
        xState                  = (u32)chanceState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        screenX                 = jitterLeft + ((xState >> 16) & ACTOR_403600_DISTORTION_JITTER_MASK);
        yState                  = (u32)xState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        D_actor_403600_80160698 = yState;
        jitterTop               = localY + ACTOR_403600_DISTORTION_SCREEN_CENTER_Y - ACTOR_403600_DISTORTION_JITTER_RADIUS;
        screenY                 = jitterTop + ((yState >> 16) & ACTOR_403600_DISTORTION_JITTER_MASK);
    }
    /// Clamps the grid geometry and selects page/UV under the displaced pixel.
    ///
    /// Arguments must be distinct, side-effect-free pointer/scalar locals.
    /// pixelX/pixelY are writable s32 lvalues; vertex/page storage stays live.
    /// Each argument may be evaluated repeatedly. Use as a standalone statement.
#define ACTOR_403600_CLAMP_DISTORTION_VERTEX(gridVertex, pageOffset, pixelX, pixelY) \
    {                                                                                \
        enum {                                                                       \
            ACTOR_403600_DISTORTION_SCREEN_WIDTH  = 320,                             \
            ACTOR_403600_DISTORTION_SCREEN_HEIGHT = 240,                             \
            ACTOR_403600_DISTORTION_PAGE_PIXELS   = 256,                             \
            ACTOR_403600_DISTORTION_PAGE_SHIFT    = 64,                              \
        };                                                                           \
        /* Fold edge clamping back into geometry, then sample the capture page. */   \
        if (pixelY >= ACTOR_403600_DISTORTION_SCREEN_HEIGHT) {                       \
            (gridVertex)->y += (ACTOR_403600_DISTORTION_SCREEN_HEIGHT - 1) - pixelY; \
            pixelY           = ACTOR_403600_DISTORTION_SCREEN_HEIGHT - 1;            \
        } else if (pixelY < 0) {                                                     \
            (gridVertex)->y -= pixelY;                                               \
            pixelY           = 0;                                                    \
        }                                                                            \
        if (pixelX >= ACTOR_403600_DISTORTION_SCREEN_WIDTH) {                        \
            (gridVertex)->x += (ACTOR_403600_DISTORTION_SCREEN_WIDTH - 1) - pixelX;  \
            pixelX           = ACTOR_403600_DISTORTION_SCREEN_WIDTH - 1;             \
        } else if (pixelX < 0) {                                                     \
            (gridVertex)->x -= pixelX;                                               \
            pixelX           = 0;                                                    \
        }                                                                            \
        *(pageOffset) = 0;                                                           \
        if (pixelX >= ACTOR_403600_DISTORTION_PAGE_PIXELS) {                         \
            *(pageOffset) = ACTOR_403600_DISTORTION_PAGE_SHIFT;                      \
        }                                                                            \
        (gridVertex)->u = pixelX - *(pageOffset);                                    \
        (gridVertex)->v = pixelY;                                                    \
    }
    ACTOR_403600_CLAMP_DISTORTION_VERTEX(vertex, pageShift, screenX, screenY);
#undef ACTOR_403600_CLAMP_DISTORTION_VERTEX
}

static void func_actor_403600_80132A18(Task* arg0, Actor403600Work* work, Actor403600FxWork* fx)
{
    s32                                  fade;
    s32                                  x;
    s32                                  y;
    s32                                  seed;
    s32                                  step;
    u8*                                  head;
    TILE*                                tile;
    DR_TPAGE*                            draw_mode;
    _Actor403600GridQuad*                poly;
    _Actor403600GridQuad*                previous;
    _Actor403600GridQuad*                above;
    _Actor403600ScreenDistortionScratch* scratch;

    head                     = SCRATCH_STACK_CURSOR(u8) - sizeof(_Actor403600ScreenDistortionScratch);
    SCRATCH_STACK_CURSOR(u8) = head;
    scratch                  = (_Actor403600ScreenDistortionScratch*)head;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        seed                    = rand();
        D_actor_403600_80160698 = seed;
        fx->gridSeed            = seed;
    } else {
        D_actor_403600_80160698 = fx->gridSeed;
    }
    scratch->field_14.vx = 0;
    scratch->field_14.vy = 0;
    scratch->field_14.vz = 0;
    fade                 = work->screenDistortion;
    for (y = -0x78; y < 0x78; y += 0x10) {
        for (x = -0xA0; x < 0xA0; x += 0x10) {
            poly                    = (_Actor403600GridQuad*)D_actor_403600_8016069C;
            D_actor_403600_8016069C = (u8*)(poly + 1);

            // Vertices shared with the quad to the left or above are copied
            // from it; only the grid's outer edge is placed afresh.
            if (x == -0xA0) {
                poly->vertex2.x = x;
                poly->vertex2.y = y + 0x10;
                _actor403600PlaceDistortionVertex(poly, 2, &scratch->field_14, fade);
            } else {
                previous                                        = poly - 1;
                ACTOR_403600_GRID_VERTEX_XY_WORD(poly->vertex2) = ACTOR_403600_GRID_VERTEX_XY_WORD(previous->vertex3);
                poly->vertex2.u                                 = previous->vertex3.u;
                poly->vertex2.v                                 = previous->vertex3.v;
                poly->page2                                     = previous->page3;
            }
            if (y == -0x78) {
                if (x == -0xA0) {
                    poly->vertex0.x = x;
                    poly->vertex0.y = y;
                    _actor403600PlaceDistortionVertex(poly, 0, &scratch->field_14, fade);
                } else {
                    previous                                        = poly - 1;
                    ACTOR_403600_GRID_VERTEX_XY_WORD(poly->vertex0) = ACTOR_403600_GRID_VERTEX_XY_WORD(previous->vertex1);
                    poly->vertex0.u                                 = previous->vertex1.u;
                    poly->vertex0.v                                 = previous->vertex1.v;
                    poly->page0                                     = previous->page1;
                }
                poly->vertex1.x = x + 0x10;
                poly->vertex1.y = y;
                _actor403600PlaceDistortionVertex(poly, 1, &scratch->field_14, fade);
            } else {
                above                                           = poly - 20;
                ACTOR_403600_GRID_VERTEX_XY_WORD(poly->vertex0) = ACTOR_403600_GRID_VERTEX_XY_WORD(above->vertex2);
                poly->vertex0.u                                 = above->vertex2.u;
                poly->vertex0.v                                 = above->vertex2.v;
                poly->page0                                     = above->page2;
                ACTOR_403600_GRID_VERTEX_XY_WORD(poly->vertex1) = ACTOR_403600_GRID_VERTEX_XY_WORD(above->vertex3);
                poly->vertex1.u                                 = above->vertex3.u;
                poly->vertex1.v                                 = above->vertex3.v;
                poly->page1                                     = above->page3;
            }
            poly->vertex3.x = x + 0x10;
            poly->vertex3.y = y + 0x10;
            _actor403600PlaceDistortionVertex(poly, 3, &scratch->field_14, fade);
            _actor403600UnifyGridQuadTexturePage(poly);
            if (fade < 0xC00) {
                setlen(poly, 9);
                poly->code = 0x2D;
            } else {
                step                              = (fade - 0xC00) >> 3;
                GPU_PRIMITIVE_COLOR_WORD(poly, 0) = (((step / 2 + 0x80) & 0xFF) << 8) | GPU_PACK_COLOR_WORD(0, 0, 0x80, 0) | ((step + 0x7F) & 0xFF);
                setlen(poly, 9);
                poly->code = 0x2C;
            }
            scratch->otz = 0;
            addPrim(&gGpuCurrentOt[scratch->otz], poly);
        }
    }
    if (fade == 0x1000) {
        tile                    = (TILE*)D_actor_403600_8016069C;
        D_actor_403600_8016069C = (u8*)(tile + 1);
        tile->x0                = -0xA0;
        tile->y0                = -0x78;
        tile->w                 = 0x140;
        tile->h                 = 0xF0;
        setlen(tile, 3);
        GPU_PRIMITIVE_COLOR_WORD(tile, 0) = GPU_PACK_COLOR_WORD(0xC0, 0x60, 0x20, 0);
        tile->code                        = 0x62;
        draw_mode                         = gGpuPrimCursor;
        gGpuPrimCursor                    = draw_mode + 1;
        addPrim(gGpuCurrentOt - 1, tile);
        setDrawTPage(draw_mode, 0, 1, 0x20);
        addPrim(gGpuCurrentOt - 1, draw_mode);
    }
    frameCaptureQueue(0);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(_Actor403600ScreenDistortionScratch));
}

/// Applies a Q12 matrix's 3x3 part to a short vector without adding translation.
///
/// Inputs are read only; xyz products shift right by 12 and saturate to signed
/// 16 bits. `output` may alias `input`; its fourth halfword is unchanged.
/// Requires the matrix's first 20 bytes readable and word aligned, and the input
/// vector's full eight bytes readable and word aligned. The output needs writable,
/// halfword-aligned xyz. Borrows the pointers until return and overwrites GTE rotation, V0, MAC, IR and FLAG state.
static inline void _actor403600RotateSv(const MATRIX* rotationMatrix, const SVECTOR* input, SVECTOR* output)
{
    gte_SetRotMatrix(rotationMatrix);
    gte_ldv0(input);
    gte_rtv0();
    gte_stsv(output);
}

/// Converts a composed loose-part origin to the saved room-axis scratch frame.
///
/// Requires part/view workm caches in the same composed frame and the inverse
/// view rotation already in scratch->basis. Subtracts the
/// view translation before signed-halfword narrowing and Q12 rotation; borrows
/// both pointers and overwrites scratch->aux and GTE rotation/vector state.
static inline void _actor403600GetLoosePartRoomOrigin(const GfxCoord* part, _Actor403600ChainScratch* scratch)
{
    scratch->aux.vx = part->workm.t[0] - gGfxViewCoord.workm.t[0];
    scratch->aux.vy = part->workm.t[1] - gGfxViewCoord.workm.t[1];
    scratch->aux.vz = part->workm.t[2] - gGfxViewCoord.workm.t[2];
    _gfxRotateSv(&scratch->basis, &scratch->aux);
}

void actor403600SwingLooseParts(Task* fxTask, const Actor403600Work* work, Actor403600FxWork* fx)
{
    enum {
        ACTOR_403600_CHAIN_SEGMENT_LENGTH = 1157,
        ACTOR_403600_LIMB_SEGMENT_LENGTH  = 2200,
        ACTOR_403600_LOOSE_PART_BASE_PULL = 512,
        ACTOR_403600_CHAIN_ROOT_PART      = 8,
        ACTOR_403600_CHAIN_FIRST_PART     = 9,
        ACTOR_403600_LIMB_FIRST_PART      = 15,
        ACTOR_403600_LIMB_PART_STRIDE     = 4
    };
    Task*                     bossTask;
    GfxCoord*                 chainRoot;
    u8*                       scratchHead;
    _Actor403600ChainScratch* scratch;
    s32                       partIndex;

    bossTask  = fxTask->parent;
    chainRoot = &bossTask->extra.tmd->coords[ACTOR_403600_CHAIN_ROOT_PART];
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        scratchHead = SCRATCH_STACK_CURSOR(u8);
        scratch     = (_Actor403600ChainScratch*)(SCRATCH_STACK_CURSOR(u8) = scratchHead - sizeof(_Actor403600ChainScratch));
        actorRenderComposeCoord(&bossTask->extra.tmd->coords[11]);
        // Store the loose endpoints in room axes before any dynamic pulling.
        if (fx->chainsPlaced == 0) {
            TransposeMatrix(&gGfxViewCoord.workm, &scratch->basis);
            _actor403600GetLoosePartRoomOrigin(chainRoot, scratch);

            scratch->segment.vx = 0;
            scratch->segment.vy = 0;
            scratch->segment.vz = -ACTOR_403600_CHAIN_SEGMENT_LENGTH;
            _gfxRotateSv(&chainRoot->workm, &scratch->segment);

            _gfxRotateSv(&scratch->basis, &scratch->segment);

            partIndex = 0;
            do {
                fx->chain[partIndex]     = scratch->aux;
                fx->chain[partIndex].vx += scratch->segment.vx * partIndex;
                fx->chain[partIndex].vy += scratch->segment.vy * partIndex;
                fx->chain[partIndex].vz += scratch->segment.vz * partIndex;
                partIndex++;
            } while (partIndex < ARRAY_SIZE(fx->chain));

            partIndex = 0;
            do {
                GfxCoord* limb = &bossTask->extra.tmd->coords[partIndex * ACTOR_403600_LIMB_PART_STRIDE + ACTOR_403600_LIMB_FIRST_PART];
                actorRenderComposeCoord(limb);
                _actor403600GetLoosePartRoomOrigin(limb, scratch);

                scratch->segment.vx = 0;
                scratch->segment.vy = ACTOR_403600_LIMB_SEGMENT_LENGTH;
                scratch->segment.vz = 0;
                _gfxRotateSv(&chainRoot->workm, &scratch->segment);

                _gfxRotateSv(&scratch->basis, &scratch->segment);
                fx->limbTips[partIndex].vx = scratch->aux.vx + scratch->segment.vx;
                fx->limbTips[partIndex].vy = scratch->aux.vy + scratch->segment.vy;
                fx->limbTips[partIndex].vz = scratch->aux.vz + scratch->segment.vz;
                partIndex++;
            } while (partIndex < ARRAY_SIZE(fx->limbTips));
            fx->chainsPlaced += 1;
        } else {
            // Constrain lengths, then turn the model parts toward the saved endpoints.
            TransposeMatrix(&gGfxViewCoord.workm, &scratch->basis);
            _actor403600GetLoosePartRoomOrigin(chainRoot, scratch);
            fx->chain[0] = scratch->aux;

            scratch->aux.vx = 0;
            scratch->aux.vy = 0;
            scratch->aux.vz = -(work->chainPullExtra + ACTOR_403600_LOOSE_PART_BASE_PULL);
            _gfxRotateSv(&chainRoot->workm, &scratch->aux);
            _gfxRotateSv(&scratch->basis, &scratch->aux);

            if (work->chainSweep != 0) {
                gte_lddp(work->chainSweep);
                gte_ldsv(&scratch->aux);
                gte_gpf12();
                gte_stsv(&scratch->sweepPull);
            }

            partIndex = 0;
            do {
                scratch->segment.vx  = fx->chain[partIndex + 1].vx - fx->chain[partIndex].vx;
                scratch->segment.vy  = fx->chain[partIndex + 1].vy - fx->chain[partIndex].vy;
                scratch->segment.vz  = fx->chain[partIndex + 1].vz - fx->chain[partIndex].vz;
                scratch->segment.vx += scratch->aux.vx;
                scratch->segment.vy += scratch->aux.vy;
                scratch->segment.vz += scratch->aux.vz;
                scratch->aux.vx    >>= 1;
                scratch->aux.vy    >>= 1;
                scratch->aux.vz    >>= 1;
                VectorNormalSS(&scratch->segment, &scratch->segment);
                scratch->dirs[partIndex] = scratch->segment;
                gte_lddp(ACTOR_403600_CHAIN_SEGMENT_LENGTH);
                gte_ldsv(&scratch->segment);
                gte_gpf12();
                gte_stsv(&scratch->segment);
                fx->chain[partIndex + 1].vx = fx->chain[partIndex].vx + scratch->segment.vx;
                fx->chain[partIndex + 1].vy = fx->chain[partIndex].vy + scratch->segment.vy;
                fx->chain[partIndex + 1].vz = fx->chain[partIndex].vz + scratch->segment.vz;
                partIndex++;
            } while (partIndex < ARRAY_SIZE(scratch->dirs));

            partIndex = 0;
            do {
                GfxCoord* segment = &bossTask->extra.tmd->coords[partIndex + ACTOR_403600_CHAIN_FIRST_PART];
                _actor403600RotateSv(&gGfxViewCoord.workm, &scratch->dirs[partIndex], &scratch->segment);
                TransposeMatrix(&chainRoot->workm, &scratch->rot);
                _gfxRotateSv(&scratch->rot, &scratch->segment);
                scratch->aux.vx     = 0;
                scratch->aux.vy     = ONE;
                scratch->aux.vz     = 0;
                scratch->segment.vx = -scratch->segment.vx;
                scratch->segment.vy = -scratch->segment.vy;
                scratch->segment.vz = -scratch->segment.vz;
                gfxBuildOrthonormalBasis(&scratch->basis, &scratch->segment, &scratch->aux);
                gte_MulMatrix0(&scratch->rot, &segment->workm, &scratch->rot);
                gte_MulMatrix0(&scratch->basis, &scratch->rot, &scratch->rot);
                gte_MulMatrix0(&chainRoot->workm, &scratch->rot, &scratch->rot);
                TransposeMatrix(&segment->parent->workm, &scratch->basis);
                gte_MulMatrix0(&scratch->basis, &scratch->rot, &segment->coord);
                segment->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(segment);
                partIndex++;
            } while (partIndex < ARRAY_SIZE(scratch->dirs));

            partIndex = 0;
            do {
                GfxCoord* limb = &bossTask->extra.tmd->coords[partIndex * ACTOR_403600_LIMB_PART_STRIDE + ACTOR_403600_LIMB_FIRST_PART];
                TransposeMatrix(&gGfxViewCoord.workm, &scratch->basis);
                actorRenderComposeCoord(limb);
                _actor403600GetLoosePartRoomOrigin(limb, scratch);

                scratch->segment.vx = 0;
                scratch->segment.vy = work->limbPullExtra + ACTOR_403600_LOOSE_PART_BASE_PULL;
                scratch->segment.vz = 0;
                _gfxRotateSv(&limb->workm, &scratch->segment);
                _gfxRotateSv(&scratch->basis, &scratch->segment);

                scratch->segment.vx += fx->limbTips[partIndex].vx - scratch->aux.vx;
                scratch->segment.vy += fx->limbTips[partIndex].vy - scratch->aux.vy;
                scratch->segment.vz += fx->limbTips[partIndex].vz - scratch->aux.vz;
                if (work->chainSweep != 0) {
                    scratch->segment.vx += scratch->sweepPull.vx;
                    scratch->segment.vy += scratch->sweepPull.vy;
                    scratch->segment.vz += scratch->sweepPull.vz;
                }
                VectorNormalSS(&scratch->segment, &scratch->segment);
                scratch->dirs[partIndex] = scratch->segment;
                gte_lddp(ACTOR_403600_LIMB_SEGMENT_LENGTH);
                gte_ldsv(&scratch->segment);
                gte_gpf12();
                gte_stsv(&scratch->segment);
                fx->limbTips[partIndex].vx = scratch->aux.vx + scratch->segment.vx;
                fx->limbTips[partIndex].vy = scratch->aux.vy + scratch->segment.vy;
                fx->limbTips[partIndex].vz = scratch->aux.vz + scratch->segment.vz;

                _actor403600RotateSv(&gGfxViewCoord.workm, &scratch->dirs[partIndex], &scratch->segment);
                TransposeMatrix(&limb->workm, &scratch->rot);
                _gfxRotateSv(&scratch->rot, &scratch->segment);
                scratch->aux.vx = 0;
                scratch->aux.vy = 0;
                scratch->aux.vz = ONE;
                gfxBuildOrthonormalBasis(&scratch->basis, &scratch->segment, &scratch->aux);
                gte_ReadMatrixColumn(&scratch->basis, 2, &scratch->aux);

                scratch->basis.m[0][0] = -scratch->basis.m[0][0];
                scratch->basis.m[1][0] = -scratch->basis.m[1][0];
                scratch->basis.m[2][0] = -scratch->basis.m[2][0];
                scratch->basis.m[0][2] = scratch->basis.m[0][1];
                scratch->basis.m[1][2] = scratch->basis.m[1][1];
                scratch->basis.m[2][2] = scratch->basis.m[2][1];
                scratch->basis.m[0][1] = scratch->aux.vx;
                scratch->basis.m[1][1] = scratch->aux.vy;
                scratch->basis.m[2][1] = scratch->aux.vz;

                gte_MulMatrix0(&scratch->basis, &limb->coord, &limb->coord);
                limb->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(limb);
                partIndex++;
            } while (partIndex < ARRAY_SIZE(fx->limbTips));
        }
        SCRATCH_STACK_RELEASE_BYTES(sizeof(_Actor403600ChainScratch));
    }
}

void func_actor_403600_80134288(Task* arg0)
{
    Actor403600Work*   work;
    Actor403600FxWork* fx;
    Task*              child;

    work = arg0->parent->work;
    if (arg0->state == 0) {
        fx = memCalloc(sizeof(Actor403600FxWork), false);
        if (fx == NULL) {
            taskCallExit(arg0);
            return;
        }
        gGameSession->field_80 = 0;
        arg0->work             = fx;
        child                  = taskSpawnFromTable(D_actor_403600_801421A0, 2, 0, 0);
        if (child != NULL) {
            taskReparent(arg0, child);
        }
        work                         = arg0->parent->work;
        work->fxTask                 = arg0;
        gActor403600RipplePlaneCoord = NULL;
        arg0->state++;
    }
    fx                      = arg0->work;
    D_actor_403600_8016069C = (u8*)Fs_ActorLoadBase2 + (gDisplayState.otBuffer * 0xC000);
    if (work->defeated != 1 && work->screenDistortion > 0) {
        func_actor_403600_80132A18(arg0, work, fx);
    }
}
static const SVECTOR D_actor_403600_80131E24 = { -100, 700, -280, 0 };

void func_actor_403600_80134398(Task* arg0)
{
    MATRIX*                gteValue1;
    SVECTOR*               gteValue2;
    DVECTOR*               gteValue3;
    s32*                   gteValue4;
    s32*                   gteValue5;
    s32*                   gteValue6;
    SVECTOR                sp10;
    SVECTOR                sp18;
    SVECTOR*               firstVector;
    SVECTOR*               cameraVector;
    Task*                  player;
    s32                    sp24;
    s32                    sp28;
    DisplayState*          ds;
    s16                    temp_s0_6;
    s16                    temp_v1_10;
    s16                    temp_v1_11;
    s16                    temp_v1_8;
    s16                    temp_v1_9;
    s32                    temp_a0_4;
    s32                    temp_v1_13;
    s32                    temp_v1_6;
    s32                    var_a0;
    GfxCoord*              var_a1_2;
    s32                    var_fp;
    s32                    var_s4;
    s32                    temp_s0_3;
    s32                    temp_s0_5;
    s8                     temp_v1_12;
    u16                    temp_v1_3;
    s32                    historyDst, historySrc;
    SVECTOR*               historyOut;
    s32                    direction;
    u8                     temp_v0_5;
    u8                     temp_v1_4;
    u8                     temp_v1_5;
    u8                     temp_v1_7;
    GfxCoord*              ownerCoord;
    Task*                  motionParent;
    Task*                  temp_a0_3;
    WorldCollisionBody*    obj;
    WorldCollisionContact* recs;
    SVECTOR*               temp_s0_4;
    SVECTOR*               temp_s1;
    WorldCollisionCapsule* newShape;
    GfxCoord*              target;
    GfxCoord*              view;
    /* The setup and draw phases reuse this pointer; steering has its own counter. */
    void*                          shared;
    s32                            steeringPass;
    Task*                          owner;
    Actor403600ProjectileWork*     work;
    GfxCoord*                      coord;
    _Actor403600ProjectileScratch* scratch;
    Actor403600ProjectileWork*     newWork;
    SVECTOR*                       temp_v0_4;
    SVECTOR*                       temp_v1;
    SVECTOR*                       point;

    coord  = arg0->extra.coordBody->coord;
    sp10   = D_actor_403600_80131E24;
    player = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    if (player == NULL) {
        taskCallExit(arg0);
        return;
    }
    if (((Actor403600Work*)((Task*)arg0->spawnArg2.pointer)->work)->defeated == 1) {
        taskCallExit(arg0);
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(_Actor403600ProjectileScratch);
    scratch = SCRATCH_STACK_CURSOR(_Actor403600ProjectileScratch);
    if (arg0->state == 0) {
        newWork = memCalloc(sizeof(Actor403600ProjectileWork), 0);
        if (newWork == NULL) {
            taskCallExit(arg0);
            SCRATCH_STACK_RELEASE_BLOCK(_Actor403600ProjectileScratch);
            return;
        }
        arg0->work             = newWork;
        coord->parent          = &gGfxViewCoord;
        coord->coord.t[2]      = 0;
        coord->coord.t[1]      = 0;
        coord->coord.t[0]      = 0;
        coord->composeStamp    = GRAPHICS_COORD_DIRTY;
        owner                  = arg0->spawnArg2.pointer;
        arg0->killCountdown    = 1;
        arg0->status           = 1;
        arg0->extraState.value = 0;
        if (owner == NULL) {
            newWork->direction.vx = 0;
            newWork->direction.vy = -0x1000;
            newWork->direction.vz = 0;
        } else {
            shared                = owner->work;
            newWork->direction.vy = -0x1B8;
            firstVector           = &sp18;
            temp_s1               = &newWork->direction;

            newWork->direction.vx = 0;
            newWork->direction.vz = 0x4B0;
            sp18                  = newWork->direction;
            ownerCoord            = &((Actor403600Work*)shared)->worldCoord;
            shared                = &((Actor403600Work*)shared)->worldCoord.coord;
            gte_SetRotMatrix(shared);
            gte_ldv0(firstVector);
            gte_rtv0();
            gte_stsv(temp_s1);
            coord->coord.t[0]     = ownerCoord->coord.t[0] + newWork->direction.vx;
            coord->coord.t[1]     = ownerCoord->coord.t[1] + newWork->direction.vy;
            coord->coord.t[2]     = ownerCoord->coord.t[2] + newWork->direction.vz;
            newWork->direction.vx = (s16)((rand() & 0x1FF) - 0x100);
            newWork->direction.vy = (s16)((rand() & 0x1FF) - 0x100);
            newWork->direction.vz = 0x1000;
            sp18                  = newWork->direction;
            gte_SetRotMatrix(shared);
            gte_ldv0(firstVector);
            gte_rtv0();
            gte_stsv(temp_s1);

            if (arg0->spawnArg1.value == 0x1100) {
                actorRenderCopyCoordBodyTransform(arg0, &owner->extra.tmd->coords[14], &sp10);
            } else {
                actorRenderCopyCoordBodyTransform(arg0, &owner->extra.tmd->coords[18], &sp10);
            }
            if (arg0->spawnArg1.value < 0x1000) {
                effectSpawn(EFFECT_EVE_ENERGY_RING, coord, 0x20, 0);
            }
            arg0->status        = 3;
            arg0->killCountdown = 0x20;
        }
        var_s4 = 0;
        if (arg0->spawnArg1.value >= 0x1000) {
            arg0->status = 4;
        }
        do {
            newWork->trail[var_s4].vx = (u16)coord->coord.t[0];
            newWork->trail[var_s4].vy = (u16)coord->coord.t[1];
            newWork->trail[var_s4].vz = (u16)coord->coord.t[2];
            var_s4                   += 1;
        } while (var_s4 < ARRAY_SIZE(newWork->trail));
        newShape = &newWork->attackCapsule;
        if (arg0->spawnArg1.value < 0x1000) {
            obj                               = &newWork->attackBody;
            obj->coord                        = coord;
            obj->context.capsule              = newShape;
            obj->pos.vx                       = 0;
            obj->pos.vy                       = 0;
            obj->pos.vz                       = 0;
            obj->radius                       = 0;
            recs                              = newWork->attackContacts;
            obj->key                          = damagePackAttackKey(&D_actor_403600_801420F0, arg0->spawnArg1.value & 0xF);
            obj->flags                        = WORLD_COLLISION_BODY_CAPSULE;
            newShape->contacts                = recs;
            newShape->ends[1].vx              = 0;
            newShape->ends[1].vy              = 0;
            newShape->ends[1].vz              = 0;
            newWork->attackCapsule.ends[0].vx = 0;
            newShape->ends[0].vy              = 0;
            newShape->ends[0].vz              = 0;
            newShape->end0Radius              = 0xC8;
            newShape->end1Radius              = 0xC8;
            worldCollisionInitContacts(recs, ARRAY_SIZE(newWork->attackContacts), 0);
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, obj);
            obj->flags         = obj->flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            arg0->exitCallback = actor403600ProjectileExit;
        }
        newWork->life = 0x12C;
        arg0->state   = arg0->state + 1;
    }
    work   = arg0->work;
    target = &player->extra.tmd->coords[1];
    actorRenderComposeCoord(target);
    TransposeMatrix(&gGfxViewCoord.workm, &scratch->inverseViewRot);
    view            = &gGfxViewCoord;
    var_s4          = target->workm.t[0] - view->workm.t[0];
    scratch->dir.vx = (s16)var_s4;
    var_s4          = target->workm.t[1] - view->workm.t[1];
    scratch->dir.vy = (s16)var_s4;
    newShape        = &work->attackCapsule;
    cameraVector    = &sp18;
    temp_v1         = &scratch->dir;
    var_s4          = target->workm.t[2] - view->workm.t[2];
    scratch->dir.vz = (s16)var_s4;
    *cameraVector   = scratch->dir;
    gteValue1       = &scratch->inverseViewRot;
    gte_SetRotMatrix(gteValue1);
    gte_ldv0(cameraVector);
    gte_rtv0();
    gte_stsv(temp_v1);
    scratch->target.vx = scratch->dir.vx;
    scratch->target.vy = scratch->dir.vy;
    scratch->target.vz = scratch->dir.vz;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        if (arg0->spawnArg1.value < 0x1000) {
            work->life = work->life - 1;
        } else {
            arg0->killCountdown = (u16)arg0->killCountdown + 1;
        }
        var_s4 = 0;
        do {
            work->trail[ARRAY_SIZE(work->trail) - 1 - var_s4] = work->trail[ARRAY_SIZE(work->trail) - 2 - var_s4];
            var_s4                                           += 1;
        } while (var_s4 < ARRAY_SIZE(work->trail) - 1);
        arg0->extraState.value ^= 1;
        temp_v1_3               = (u16)arg0->killCountdown - 1;
        arg0->killCountdown     = temp_v1_3;
        if ((temp_v1_3 << 0x10) <= 0) {
            temp_v1_4 = arg0->status;
            if (temp_v1_4 == 2) {
                taskCallExit(arg0);
                SCRATCH_STACK_RELEASE_BLOCK(_Actor403600ProjectileScratch);
                return;
            }
            if (temp_v1_4 == 3) {
                temp_s0_3 = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(SOUND_SHELTER_B2_POD_BTM_PROJECTILE_LAUNCH, temp_s0_3, (s8)worldCoordGetOriginAudioDepth(coord));
                arg0->killCountdown = (rand() & 0xF) + 0x10;
                arg0->status        = 1;
            } else if (temp_v1_4 == 0) {
                arg0->killCountdown = 0x7FFF;
                arg0->status        = 1;
            } else {
                arg0->killCountdown = (u16)((rand() & 0xF) + 0x10);
                arg0->status        = (u8)(arg0->status ^ 1);
            }
        }
        if (arg0->status != 2) {
            steeringPass = 0;
            temp_s0_4    = &scratch->dir;
            for (; steeringPass < 2; steeringPass++) {
                if (arg0->status == 0) {
                    scratch->dir.vx = (s16)((scratch->target.vx - coord->coord.t[0]) >> 2);
                    scratch->dir.vy = (s16)((s32)(scratch->target.vy - coord->coord.t[1]) >> 2);
                    scratch->dir.vz = (s16)((s32)(scratch->target.vz - coord->coord.t[2]) >> 2);
                    VectorNormalSS(temp_s0_4, temp_s0_4);
                    direction       = (scratch->dir.vx + work->direction.vx * 7) >> 4;
                    scratch->dir.vx = direction;
                    direction       = (scratch->dir.vy + work->direction.vy * 7) >> 4;
                    scratch->dir.vy = direction;
                    direction       = (scratch->dir.vz + work->direction.vz * 7) >> 4;
                    scratch->dir.vz = direction;
                    VectorNormalSS(temp_s0_4, temp_s0_4);
                    work->direction.vx = (s16)(u16)scratch->dir.vx;
                    work->direction.vy = (s16)(u16)scratch->dir.vy;
                    work->direction.vz = (s16)(u16)scratch->dir.vz;
                }
                temp_v1_5 = arg0->status;
                if (temp_v1_5 < 3U) {
                    gte_lddp(100);
                    gteValue2 = &work->direction;
                    gte_ldsv(gteValue2);
                    gte_gpf12();
                    gte_stsv(temp_s0_4);
                    newShape->ends[1].vx = (s16) - (s16)(u16)scratch->dir.vx;
                    newShape->ends[1].vy = (s16) - (s16)(u16)scratch->dir.vy;
                    newShape->ends[1].vz = (s16) - (s16)(u16)scratch->dir.vz;
                    coord->coord.t[0]    = coord->coord.t[0] + scratch->dir.vx;
                    coord->coord.t[1]    = coord->coord.t[1] + scratch->dir.vy;
                    coord->coord.t[2]    = coord->coord.t[2] + scratch->dir.vz;
                } else if (temp_v1_5 == 3) {
                    motionParent = arg0->spawnArg2.pointer;
                    if ((s16)arg0->killCountdown >= 7) {
                        var_a1_2 = &motionParent->extra.tmd->coords[18];
                        actorRenderCopyCoordBodyTransform(arg0, var_a1_2, &sp10);
                        var_s4 = 0;
                        do {
                            work->trail[var_s4].vx = (u16)coord->coord.t[0];
                            work->trail[var_s4].vy = (u16)coord->coord.t[1];
                            work->trail[var_s4].vz = (u16)coord->coord.t[2];
                            var_s4                += 1;
                        } while (var_s4 < ARRAY_SIZE(work->trail));
                    }
                } else {
                    temp_a0_3 = arg0->spawnArg2.pointer;
                    if (arg0->spawnArg1.value == 0x1000) {
                        actorRenderCopyCoordBodyTransform(arg0, &temp_a0_3->extra.tmd->coords[18], &sp10);
                    } else {
                        actorRenderCopyCoordBodyTransform(arg0, &temp_a0_3->extra.tmd->coords[14], &sp10);
                    }
                }
            }
        }
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work->trail[0].vx  = (u16)coord->coord.t[0];
        work->trail[0].vy  = (u16)coord->coord.t[1];
        work->trail[0].vz  = (u16)coord->coord.t[2];
        work->trail[0].pad = rand();
    }
    if ((arg0->spawnArg1.value < 0x1000) && (worldCollisionFindContactIndex(work->attackContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0)) {
        temp_s0_5 = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(SOUND_SHELTER_B2_POD_BTM_PROJECTILE_HIT, temp_s0_5, (s8)worldCoordGetOriginAudioDepth(coord));
        work->life = -1;
    }
    if (work->life < 0) {
        if (arg0->spawnArg1.value < 0x1000) {
            worldCollisionClearContacts(work->attackContacts);
            work->attackBody.flags = work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        }
        work->life          = 0x7FFFFFFF;
        arg0->status        = 2;
        arg0->killCountdown = 0x20;
    }
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    temp_v1_6 = arg0->spawnArg1.value & 0xF;
    switch (temp_v1_6) {
        case 0:
            sp24 = 0x808000;
            break;
        case 1:
            sp24 = 0x808080;
            break;
        case 2:
            sp24 = 0x80;
            break;
        default:
            sp24 = 0x8080;
            break;
    }
    sp28      = 1;
    temp_v1_7 = arg0->status;
    var_fp    = 0;
    if (temp_v1_7 == 2) {
        var_fp = 0x20 - (s16)arg0->killCountdown;
    } else if (temp_v1_7 == 3) {
        sp28 = 4;
    }
    var_s4 = var_fp;
    if (var_s4 < ARRAY_SIZE(work->trail)) {
        ds = &gDisplayState;
        /* This loop is at the size limit (261 insns at the loop pass) below
         * which the `2` the three status tests compare with is still moved
         * out of it; one more temporary in the body changes the allocation. */
        do {
            shared                  = D_actor_403600_8016069C;
            D_actor_403600_8016069C = (u8*)shared + sizeof(POLY_FT4);
            point                   = &work->trail[var_s4];
            gte_ldv0(point);
            gte_rtps();
            gteValue3 = &scratch->sxy;
            gte_stsxy(gteValue3);
            gteValue4 = &scratch->dp;
            gte_stdp(gteValue4);
            gteValue5 = &scratch->flag;
            gte_stflg(gteValue5);
            gteValue6 = &scratch->otz;
            gte_stszotz(gteValue6);
            if (scratch->flag >= 0) {
                if (var_s4 == 0) {
                    if (arg0->status != 2) {
                        temp_a0_4 = scratch->otz;
                        if (temp_a0_4 >= 0) {
                            scratch->cornerOffset.vx = (u16)((s32)(ds->screenDistance * 0x96) / temp_a0_4);
                        } else {
                            scratch->cornerOffset.vx = 0x1000U;
                        }
                        temp_v1_8                  = (u16)scratch->sxy.vx - (u16)scratch->cornerOffset.vx;
                        ((POLY_FT4*)shared)->x2    = temp_v1_8;
                        ((POLY_FT4*)shared)->x0    = temp_v1_8;
                        temp_v1_9                  = (u16)scratch->sxy.vx + (u16)scratch->cornerOffset.vx;
                        ((POLY_FT4*)shared)->x3    = temp_v1_9;
                        ((POLY_FT4*)shared)->x1    = temp_v1_9;
                        temp_v1_10                 = (u16)scratch->sxy.vy - (u16)scratch->cornerOffset.vx;
                        ((POLY_FT4*)shared)->y1    = temp_v1_10;
                        ((POLY_FT4*)shared)->y0    = temp_v1_10;
                        temp_v1_11                 = (u16)scratch->sxy.vy + (u16)scratch->cornerOffset.vx;
                        ((POLY_FT4*)shared)->tpage = 0x29;
                        ((POLY_FT4*)shared)->y3    = temp_v1_11;
                        ((POLY_FT4*)shared)->y2    = temp_v1_11;
                        if (arg0->extraState.value != 0) {
                            ((POLY_FT4*)shared)->u2   = 0x70U;
                            ((POLY_FT4*)shared)->u0   = 0x70U;
                            ((POLY_FT4*)shared)->clut = 0x428B;
                        } else {
                            ((POLY_FT4*)shared)->u2   = 0xA8U;
                            ((POLY_FT4*)shared)->u0   = 0xA8U;
                            ((POLY_FT4*)shared)->clut = 0x428C;
                        }
                        ((POLY_FT4*)shared)->v1                          = 0xC9;
                        ((POLY_FT4*)shared)->v0                          = 0xC9;
                        ((POLY_FT4*)shared)->v3                          = 0xFF;
                        ((POLY_FT4*)shared)->v2                          = 0xFF;
                        GPU_PRIMITIVE_COLOR_WORD(((POLY_FT4*)shared), 0) = sp24;
                        temp_v1_12                                       = ((POLY_FT4*)shared)->u0 + 0x37;
                        setlen((POLY_FT4*)shared, 9);
                        ((POLY_FT4*)shared)->code = 0x2E;
                        ((POLY_FT4*)shared)->u3   = temp_v1_12;
                        ((POLY_FT4*)shared)->u1   = temp_v1_12;
                        addPrim(&gGpuCurrentOt[((u32)scratch->otz << ds->otDepthShift) >> 4 & 0x3FF],
                                (POLY_FT4*)shared);
                    }
                } else if (var_s4 >= (var_fp - 4)) {
                    temp_s0_6                = (u16)point->pad;
                    scratch->cornerOffset.vx = rsin(temp_s0_6);
                    scratch->cornerOffset.vy = rcos(temp_s0_6);
                    scratch->cornerOffset.vz = 0;
                    if (scratch->otz >= 0) {
                        var_a0 = (var_s4 * 2) + 0x78;
                        if ((var_fp >= var_s4) && (arg0->status == 2)) {
                            var_a0 = (var_s4 * 20) + 0x78;
                        } else if (arg0->status == 4) {
                            var_a0 *= 2;
                        }
                        gte_lddp((var_a0 * ds->screenDistance) / scratch->otz);
                        temp_v0_4 = &scratch->cornerOffset;
                        gte_ldsv(temp_v0_4);
                        gte_gpf12();
                        gte_stsv(temp_v0_4);
                    }
                    ((POLY_FT4*)shared)->x0    = (s16)((u16)scratch->sxy.vx + (u16)scratch->cornerOffset.vx);
                    ((POLY_FT4*)shared)->y0    = (s16)((u16)scratch->sxy.vy + (u16)scratch->cornerOffset.vy);
                    ((POLY_FT4*)shared)->x1    = (s16)((u16)scratch->sxy.vx + (u16)scratch->cornerOffset.vy);
                    ((POLY_FT4*)shared)->y1    = (s16)((u16)scratch->sxy.vy - (u16)scratch->cornerOffset.vx);
                    ((POLY_FT4*)shared)->x2    = (s16)((u16)scratch->sxy.vx - (u16)scratch->cornerOffset.vy);
                    ((POLY_FT4*)shared)->y2    = (s16)((u16)scratch->sxy.vy + (u16)scratch->cornerOffset.vx);
                    ((POLY_FT4*)shared)->x3    = (s16)((u16)scratch->sxy.vx - (u16)scratch->cornerOffset.vx);
                    ((POLY_FT4*)shared)->y3    = (s16)((u16)scratch->sxy.vy - (u16)scratch->cornerOffset.vy);
                    temp_v1_13                 = (u8)point->pad;
                    temp_v1_13                &= 0x20;
                    ((POLY_FT4*)shared)->v1    = 0x18;
                    ((POLY_FT4*)shared)->v0    = 0x18;
                    ((POLY_FT4*)shared)->v3    = 0x37;
                    ((POLY_FT4*)shared)->v2    = 0x37;
                    temp_v0_5                  = temp_v1_13 + 0x60;
                    temp_v1_13                += 0x7F;
                    ((POLY_FT4*)shared)->u2    = temp_v0_5;
                    ((POLY_FT4*)shared)->u0    = temp_v0_5;
                    ((POLY_FT4*)shared)->u3    = temp_v1_13;
                    ((POLY_FT4*)shared)->u1    = temp_v1_13;
                    ((POLY_FT4*)shared)->tpage = 0x2A;
                    ((POLY_FT4*)shared)->clut  = 0x42CC;
                    var_a0                     = var_s4;
                    if (arg0->status == 2) {
                        if (var_fp >= var_s4) {
                            var_a0 = var_fp;
                        }
                    }
                    temp_v1_13 = D_actor_403600_80142120[var_a0];
                    setlen((POLY_FT4*)shared, 9);
                    GPU_PRIMITIVE_COLOR_WORD(((POLY_FT4*)shared), 0) = temp_v1_13;
                    ((POLY_FT4*)shared)->code                        = 0x2E;
                    ACTOR_403600_LINK_PRIMITIVE(&gGpuCurrentOt[((u32)scratch->otz << ds->otDepthShift) >> 4 & 0x3FF],
                                                (POLY_FT4*)shared);
                }
            }
            var_s4 += sp28;
        } while (var_s4 < ARRAY_SIZE(work->trail));
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403600ProjectileScratch);
}

/// Clamps one radial-grid vertex's geometry and assigns its capture page/UV.
///
/// Borrows a writable quad and signed capture-image pixel coordinates.
/// projectedY retains the original vertex Y bits as an unsigned halfword.
/// Vertical edges clamp
/// both geometry and V; the left edge clamps geometry and U. At the right edge
/// geometry is clamped but U retains the displaced X, narrowed to a byte after
/// subtracting page shift 64. Width/height are 320/240; geometry stores retain
/// halfword wrap. Uses no GTE or scratch storage.
static inline void _actor403600ClampRippleVertex(_Actor403600GridQuad* quad, s32 captureWidth, s32 captureHeight, u16 projectedY, s32 captureX, s32 captureY)
{
    enum { ACTOR_403600_RIPPLE_PAGE_PIXELS = 256,
           ACTOR_403600_RIPPLE_PAGE_SHIFT  = 64 };
    if (captureY >= captureHeight) {
        quad->vertex0.y = projectedY + (captureHeight - 1) - captureY;
        captureY        = captureHeight - 1;
    } else if (captureY < 0) {
        quad->vertex0.y = projectedY - captureY;
        captureY        = 0;
    }
    if (captureX >= captureWidth) {
        quad->vertex0.x = (u16)quad->vertex0.x + (captureWidth - 1) - captureX;
    } else if (captureX < 0) {
        quad->vertex0.x = (u16)quad->vertex0.x - captureX;
        captureX        = 0;
    }
    quad->page0 = 0;
    if (captureX >= ACTOR_403600_RIPPLE_PAGE_PIXELS) {
        quad->page0 = ACTOR_403600_RIPPLE_PAGE_SHIFT;
    }
    quad->vertex0.v = captureY;
    quad->vertex0.u = captureX - quad->page0;
}

void actor403600DrawRipple(const Actor403600Ripple* ripple, GfxCoord* discCoord)
{
    enum {
        ACTOR_403600_RIPPLE_RADIAL_SAMPLE_COUNT   = ACTOR_403600_RIPPLE_SAMPLE_COUNT / 2,
        ACTOR_403600_RIPPLE_SECTOR_COUNT          = 12,
        ACTOR_403600_RIPPLE_FIRST_RADIUS          = 20,
        ACTOR_403600_RIPPLE_RADIUS_STEP           = 155,
        ACTOR_403600_RIPPLE_LIFT_SHIFT            = 3,
        ACTOR_403600_RIPPLE_SHALLOW_LIFT_SHIFT    = 8,
        ACTOR_403600_RIPPLE_DISPLACEMENT_SHIFT    = 10,
        ACTOR_403600_RIPPLE_WAVE_FRACTION_BITS    = 12,
        ACTOR_403600_RIPPLE_TURN_SHIFT            = 12,
        ACTOR_403600_RIPPLE_TEXEL_DIRECTION_SHIFT = 3,
        ACTOR_403600_RIPPLE_PAGE_X_SHIFT          = 6,
        ACTOR_403600_RIPPLE_CAPTURE_CENTER_X      = 160,
        ACTOR_403600_RIPPLE_CAPTURE_CENTER_Y      = 120,
        ACTOR_403600_RIPPLE_PAGE_PIXELS           = 256,
        ACTOR_403600_RIPPLE_PAGE_SHIFT            = 64,
        ACTOR_403600_RIPPLE_CAPTURE_X             = 448,
        ACTOR_403600_RIPPLE_CAPTURE_TPAGE_FLAGS   = 0x110,
        ACTOR_403600_RIPPLE_PACKET_WORD_COUNT     = 9,
        ACTOR_403600_RIPPLE_PACKET_CODE           = 0x2D,
        ACTOR_403600_RIPPLE_OT_DEPTH_MASK         = 0x3FFF,
        ACTOR_403600_RIPPLE_OT_QUANTIZATION_SHIFT = 4
    };
    s32                            ringLift[ACTOR_403600_RIPPLE_RADIAL_SAMPLE_COUNT];
    s32                            texelDisplacementScale[ACTOR_403600_RIPPLE_RADIAL_SAMPLE_COUNT];
    s32                            cornerU[4];
    s32                            ringIndex;
    s32                            sectorIndex;
    s32                            sampleOffset;
    s32                            sampleIndex;
    s32                            lookupIndex;
    s32                            weightedSample;
    s32                            sampleLift;
    s32                            texelDirection;
    s32                            sharedVertexXY;
    s32                            texelCenterX;
    s32                            texelCenterY;
    s32                            screenX;
    s32                            screenY;
    s32                            minU;
    s32                            maxU;
    s32                            pageShift;
    s32                            liftOffsetBytes;
    s32                            ringDistance;
    s32                            cornerIndex;
    s32*                           displacementScale;
    MATRIX*                        sectorMatrix;
    s32*                           displacementScaleBase;
    s32*                           nextDisplacementScale;
    u16                            unclampedY;
    u8*                            scratchHead;
    u8*                            scratchBlock;
    _Actor403600GridQuad*          afterQuad;
    _Actor403600GridQuad*          previousQuad;
    _Actor403600GridQuad*          neighborQuad;
    _Actor403600GridQuad*          quad;
    SVECTOR*                       texelOffset;
    _Actor403600RadialGridScratch* scratch;

    scratchHead                = SCRATCH_STACK_CURSOR(u8);
    scratchBlock               = scratchHead - sizeof(_Actor403600RadialGridScratch);
    SCRATCH_STACK_CURSOR(void) = scratchBlock;
    scratch                    = (_Actor403600RadialGridScratch*)scratchBlock;
    actorRenderComposeCoord(discCoord);
    gte_SetRotMatrix(&discCoord->workm);
    gte_SetTransMatrix(&discCoord->workm);

    scratch->probePoints[1].vz = ONE;
    scratch->probePoints[2].vx = ONE;
    scratch->probePoints[0].vx = 0;
    scratch->probePoints[0].vy = 0;
    scratch->probePoints[0].vz = 0;
    scratch->probePoints[1].vx = 0;
    scratch->probePoints[1].vy = 0;
    scratch->probePoints[2].vy = 0;
    scratch->probePoints[2].vz = 0;
    gte_ldv3(&scratch->probePoints[0], &scratch->probePoints[1], &scratch->probePoints[2]);
    gte_rtpt();
    gte_stsxy3(&scratch->probeSxy[0], &scratch->probeSxy[1], &scratch->probeSxy[2]);
    gte_stdp(&scratch->dp);
    gte_stflg(&scratch->flag);
    gte_stszotz(&scratch->otz);
    gte_nclip();
    gte_stopz(&scratch->nclip);

    // Sample wave lift and texture displacement independently at each radius.
    ringIndex = 0;
    do {
        sampleOffset   = ripple->head + ringIndex * 2;
        lookupIndex    = sampleOffset % ACTOR_403600_RIPPLE_SAMPLE_COUNT;
        sampleLift     = (rsin(ripple->phase[lookupIndex]) * ripple->strength[lookupIndex]) >> ACTOR_403600_RIPPLE_WAVE_FRACTION_BITS;
        weightedSample = (sampleLift * (ACTOR_403600_RIPPLE_RADIAL_SAMPLE_COUNT - ringIndex)) / ACTOR_403600_RIPPLE_RADIAL_SAMPLE_COUNT;
        sampleLift     = weightedSample >> ACTOR_403600_RIPPLE_LIFT_SHIFT;
        if (ripple->shallow == 1) {
            sampleLift = weightedSample >> ACTOR_403600_RIPPLE_SHALLOW_LIFT_SHIFT;
        }
        ringLift[ringIndex]               = sampleLift;
        weightedSample                    = (ripple->strength[lookupIndex] * (ACTOR_403600_RIPPLE_RADIAL_SAMPLE_COUNT - 1 - ringIndex)) >> ACTOR_403600_RIPPLE_DISPLACEMENT_SHIFT;
        texelDisplacementScale[ringIndex] = weightedSample;
        if (scratch->nclip > 0) {
            texelDisplacementScale[ringIndex] = -weightedSample;
        }
        ringIndex++;
        sectorIndex = 0;
    } while (ringIndex < ARRAY_SIZE(ringLift));

    // Project one radial edge per sector and share it with adjacent sectors.
    scratch->maxOtz       = 0;
    sectorMatrix          = &scratch->sectorMatrix;
    texelOffset           = &scratch->texelOffset;
    displacementScaleBase = texelDisplacementScale;
    do {
        scratch->sectorMatrix = discCoord->workm;
        gfxRotMatrixY(sectorMatrix, (sectorIndex << ACTOR_403600_RIPPLE_TURN_SHIFT) / ACTOR_403600_RIPPLE_SECTOR_COUNT, 0);
        gte_SetTransMatrix(&discCoord->workm);
        gte_SetRotMatrix(sectorMatrix);
        ringDistance      = ACTOR_403600_RIPPLE_FIRST_RADIUS;
        ringIndex         = 0;
        displacementScale = displacementScaleBase;
        liftOffsetBytes   = 0;
        sampleIndex       = ripple->head;
        do {
            // Clamp geometry to the displayed 320 by 240 pixel region.
            s32 screenW = 320;
            s32 screenH = 240;

            sampleIndex             %= ACTOR_403600_RIPPLE_SAMPLE_COUNT;
            quad                     = (_Actor403600GridQuad*)D_actor_403600_8016069C;
            D_actor_403600_8016069C += sizeof(_Actor403600GridQuad);
            texelDirection           = -rcos(ripple->phase[sampleIndex]) >> ACTOR_403600_RIPPLE_TEXEL_DIRECTION_SHIFT;
            scratch->texelOffset.vx  = rsin(texelDirection);
            scratch->texelOffset.vy  = rcos(texelDirection);
            scratch->texelOffset.vz  = 0;
            gte_ldv0(texelOffset);
            gte_rtv0();
            scratch->localVertex.vx = ringDistance;
            scratch->localVertex.vz = 0;
            // Advance an independent byte cursor through the 16 lift samples.
            scratch->localVertex.vy = *(s32*)((u8*)ringLift + liftOffsetBytes);
            gte_stsv(texelOffset);
            gte_ldv0(&scratch->localVertex);
            gte_rtps();
            gte_stsxy(&scratch->sxy);
            gte_stdp(&scratch->dp);
            gte_stflg(&scratch->flag);
            gte_stszotz(&scratch->otz);
            gte_lddp(*displacementScale);
            gte_ldsv(texelOffset);
            gte_gpf12();
            gte_stsv(texelOffset);

            ACTOR_403600_GRID_VERTEX_XY_WORD(quad->vertex0) = scratch->sxy;
            unclampedY                                      = quad->vertex0.y;
            texelCenterX                                    = scratch->texelOffset.vx + ACTOR_403600_RIPPLE_CAPTURE_CENTER_X;
            screenX                                         = (s16)quad->vertex0.x + texelCenterX;
            texelCenterY                                    = scratch->texelOffset.vy + ACTOR_403600_RIPPLE_CAPTURE_CENTER_Y;
            screenY                                         = (s16)quad->vertex0.y + texelCenterY;
            _actor403600ClampRippleVertex(quad, screenW, screenH, unclampedY, screenX, screenY);
            // The terminal sample supplies vertices but has no quad beyond it.
            if (scratch->flag >= 0 && ringIndex != ARRAY_SIZE(ringLift) - 1 && (*displacementScale != 0 || (lookupIndex = ringIndex + 1, nextDisplacementScale = &displacementScaleBase[lookupIndex], *nextDisplacementScale != 0))) {
                setlen(quad, ACTOR_403600_RIPPLE_PACKET_WORD_COUNT);
                quad->code   = ACTOR_403600_RIPPLE_PACKET_CODE;
                scratch->otz = (scratch->otz << gDisplayState.otDepthShift & ACTOR_403600_RIPPLE_OT_DEPTH_MASK) >> ACTOR_403600_RIPPLE_OT_QUANTIZATION_SHIFT;
                if (scratch->maxOtz < scratch->otz) {
                    scratch->maxOtz = scratch->otz;
                }
                addPrim(&gGpuCurrentOt[scratch->otz], quad);
            }
            previousQuad = quad - 1;
            if (ringIndex != 0) {
                ACTOR_403600_GRID_VERTEX_XY_WORD(previousQuad->vertex1) = ACTOR_403600_GRID_VERTEX_XY_WORD(quad->vertex0);
                previousQuad->vertex1.u                                 = quad->vertex0.u;
                do {
                    previousQuad->vertex1.v = quad->vertex0.v;
                    previousQuad->page1     = quad->page0;
                    if (sectorIndex != 0) {
                        sharedVertexXY = ACTOR_403600_GRID_VERTEX_XY_WORD(previousQuad->vertex0);
                        neighborQuad   = quad - (ACTOR_403600_RIPPLE_RADIAL_SAMPLE_COUNT + 1);
                    } else {
                        sharedVertexXY = ACTOR_403600_GRID_VERTEX_XY_WORD(previousQuad->vertex0);
                        neighborQuad   = quad + ((ACTOR_403600_RIPPLE_SECTOR_COUNT - 1) * ACTOR_403600_RIPPLE_RADIAL_SAMPLE_COUNT - 1);
                    }
                    ACTOR_403600_GRID_VERTEX_XY_WORD(neighborQuad->vertex2) = sharedVertexXY;
                    neighborQuad->vertex2.u                                 = previousQuad->vertex0.u;
                } while (0);
                neighborQuad->vertex2.v                                 = previousQuad->vertex0.v;
                neighborQuad->page2                                     = previousQuad->page0;
                ACTOR_403600_GRID_VERTEX_XY_WORD(neighborQuad->vertex3) = ACTOR_403600_GRID_VERTEX_XY_WORD(previousQuad->vertex1);
                neighborQuad->vertex3.u                                 = previousQuad->vertex1.u;
                neighborQuad->vertex3.v                                 = previousQuad->vertex1.v;
                neighborQuad->page3                                     = previousQuad->page1;
            }
            displacementScale++;
            liftOffsetBytes += sizeof(ringLift[0]);
            ringIndex++;
            sampleIndex  += 2;
            ringDistance += ACTOR_403600_RIPPLE_RADIUS_STEP;
        } while (ringIndex < ARRAY_SIZE(ringLift));
        sectorIndex++;
    } while (sectorIndex < ACTOR_403600_RIPPLE_SECTOR_COUNT);

    /// Rebases the current ripple quad and retreats the reverse traversal.
    ///
    /// Captures live one-past afterQuad and quad pointers, ringIndex, cornerU[4],
    /// minU/maxU/pageShift and cornerIndex (signed words). Reads corner page
    /// bytes without changing them; writes tpage/U, then decreases both cursors
    /// and advances ringIndex. The interleaved cursor updates preserve traversal.
    /// Uses this function's ACTOR_403600_RIPPLE_ constants; no arguments or calls.
#define ACTOR_403600_REBASE_AND_RETREAT_RIPPLE_QUAD()                                                                                                               \
    {                                                                                                                                                               \
        cornerU[0] = afterQuad[-1].vertex0.u + afterQuad[-1].page0;                                                                                                 \
        cornerU[1] = afterQuad[-1].vertex1.u + afterQuad[-1].page1;                                                                                                 \
        cornerU[2] = afterQuad[-1].vertex2.u + afterQuad[-1].page2;                                                                                                 \
        cornerU[3] = afterQuad[-1].vertex3.u + afterQuad[-1].page3;                                                                                                 \
        minU       = cornerU[0];                                                                                                                                    \
        maxU       = cornerU[0];                                                                                                                                    \
        for (cornerIndex = 1; cornerIndex < ARRAY_SIZE(cornerU); cornerIndex++) {                                                                                   \
            if (cornerU[cornerIndex] < minU) {                                                                                                                      \
                minU = cornerU[cornerIndex];                                                                                                                        \
            } else if (maxU < cornerU[cornerIndex]) {                                                                                                               \
                maxU = cornerU[cornerIndex];                                                                                                                        \
            }                                                                                                                                                       \
        }                                                                                                                                                           \
        if (maxU >= ACTOR_403600_RIPPLE_PAGE_PIXELS || minU >= ACTOR_403600_RIPPLE_PAGE_SHIFT) {                                                                    \
            pageShift = ACTOR_403600_RIPPLE_PAGE_SHIFT;                                                                                                             \
        } else {                                                                                                                                                    \
            pageShift = 0;                                                                                                                                          \
        }                                                                                                                                                           \
        afterQuad[-1].tpage     = ((u32)(pageShift + ACTOR_403600_RIPPLE_CAPTURE_X) >> ACTOR_403600_RIPPLE_PAGE_X_SHIFT) | ACTOR_403600_RIPPLE_CAPTURE_TPAGE_FLAGS; \
        afterQuad[-1].vertex0.u = cornerU[0] - pageShift;                                                                                                           \
        afterQuad[-1].vertex1.u = cornerU[1] - pageShift;                                                                                                           \
        quad--;                                                                                                                                                     \
        afterQuad[-1].vertex2.u = cornerU[2] - pageShift;                                                                                                           \
        ringIndex++;                                                                                                                                                \
        afterQuad[-1].vertex3.u = cornerU[3] - pageShift;                                                                                                           \
        afterQuad--;                                                                                                                                                \
    }

    // Rebase each completed quad onto one captured-frame texture page.
    sectorIndex = 0;
    do {
        ringIndex = 0;
        // Retreat from one-past the last quad through this sector's records.
        afterQuad = quad + 1;
        do {
            ACTOR_403600_REBASE_AND_RETREAT_RIPPLE_QUAD();
        } while (ringIndex < ARRAY_SIZE(ringLift));
        sectorIndex++;
    } while (sectorIndex < ACTOR_403600_RIPPLE_SECTOR_COUNT);
#undef ACTOR_403600_REBASE_AND_RETREAT_RIPPLE_QUAD
    frameCaptureQueue(scratch->maxOtz + 1);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(_Actor403600RadialGridScratch));
}

static const SVECTOR D_actor_403600_80131E2C = { 0, 0x578, 0, 0 };

/// Records one ripple-source sample and advances its outward history.
///
/// Requires a live initialized ripple and head in 0..31. Source strength uses
/// 4096 as unity: emission adds 512 while below unity, release subtracts 128
/// while positive, and an emission edge restarts phase. The new sample is cleared
/// first, then recorded only at nonzero strength. Source phase is in 4096 units
/// per turn; stored samples retain its signed low halfword. Owns no storage.
static inline void _actor403600TickRipple(Actor403600Ripple* state)
{
    enum {
        ACTOR_403600_RIPPLE_FULL_STRENGTH      = ONE,
        ACTOR_403600_RIPPLE_STRENGTH_RISE      = 512,
        ACTOR_403600_RIPPLE_STRENGTH_FALL      = 128,
        ACTOR_403600_RIPPLE_PHASE_STEP         = 384,
        ACTOR_403600_RIPPLE_SHALLOW_PHASE_STEP = 256
    };
    s32 head;

    state->head          += ACTOR_403600_RIPPLE_SAMPLE_COUNT - 1;
    state->head          %= ACTOR_403600_RIPPLE_SAMPLE_COUNT;
    head                  = state->head;
    state->phase[head]    = 0;
    state->strength[head] = 0;
    if (state->emitting != 0) {
        if (state->wasEmitting == 0) {
            state->sourcePhase = 0;
        }
        if (state->sourceStrength < ACTOR_403600_RIPPLE_FULL_STRENGTH) {
            state->sourceStrength += ACTOR_403600_RIPPLE_STRENGTH_RISE;
        }
    } else if (state->sourceStrength > 0) {
        state->sourceStrength -= ACTOR_403600_RIPPLE_STRENGTH_FALL;
    }
    state->wasEmitting = state->emitting;
    if (state->sourceStrength != 0) {
        state->phase[head]    = state->sourcePhase;
        state->strength[head] = state->sourceStrength;
        if (state->shallow == 0) {
            state->sourcePhase += ACTOR_403600_RIPPLE_PHASE_STEP;
        } else {
            state->sourcePhase += ACTOR_403600_RIPPLE_SHALLOW_PHASE_STEP;
        }
    }
}

/// Tests whether all stored ripple phases are zero.
///
/// Returns 1 only when all 32 phases are zero, otherwise 0. Strength and
/// emission are ignored; task expiry also requires its emission timer to end.
/// Borrows a live ripple and changes no state.
static inline s32 _actor403600RippleHasDiedOut(const Actor403600Ripple* state)
{
    s32 sampleIndex;

    for (sampleIndex = 0; sampleIndex < ARRAY_SIZE(state->phase); sampleIndex++) {
        if (state->phase[sampleIndex] != 0) {
            return 0;
        }
    }
    return 1;
}

/// Runs a boss ripple and the model's timed crossing of its clipping plane.
///
/// Start in state zero with the live boss task in spawnArg2.pointer and its
/// initialized effect task. Spawn mode 1 sinks then hides the model; 2 waits,
/// shows it and clips its rise through the reversed plane; other modes emit
/// a ripple without clipping. Owns its zeroed ripple work, borrowing the boss
/// and coordinate parents. Running updates advance timers/history; every update
/// draws. Exit follows boss defeat or ended emission plus zero sample phases.
/// The package-global plane borrows this work until the crossing interval ends,
/// including the rising model's hidden wait; it is cleared before work release.
static void _actor403600RippleTask(Task* task)
{
    enum {
        ACTOR_403600_RIPPLE_SINK_MODEL          = 1,
        ACTOR_403600_RIPPLE_RISE_MODEL          = 2,
        ACTOR_403600_RIPPLE_EMISSION_TICKS      = 16,
        ACTOR_403600_RIPPLE_WARMUP_TICKS        = 16,
        ACTOR_403600_RIPPLE_SINK_CLIP_TICKS     = 8,
        ACTOR_403600_RIPPLE_RISE_EMISSION_TICKS = 46,
        ACTOR_403600_RIPPLE_RISE_WAIT_TICKS     = 31,
        ACTOR_403600_RIPPLE_RISE_CLIP_LAST_TICK = -7,
        ACTOR_403600_RIPPLE_RISE_CLIP_END_TICK  = -8,
        ACTOR_403600_RIPPLE_REVERSE_PLANE_ANGLE = ONE / 2
    };
    SVECTOR            discOffset;
    Actor403600Ripple* ripple;
    Actor403600Ripple* newRipple;
    GfxCoord*          discCoord;
    s32                spawnMode;
    s32                sinkClipTicks;
    s32                riseClipTicks;
    s32                warmupTick;
    Task*              bossTask;
    TmdObject*         sinkingModel;
    TmdObject*         risingModel;
    Task*              fxTask;
    TmdObject*         defeatedModel;
    Actor403600Work*   ownerWork;

    bossTask  = task->spawnArg2.pointer;
    ownerWork = bossTask->work;
    discCoord = task->extra.coordBody->coord;
    fxTask    = ownerWork->fxTask;
    if (ownerWork->defeated == 1) {
        defeatedModel                = bossTask->extra.tmd;
        gActor403600RipplePlaneCoord = NULL;
        defeatedModel->flags         = (u16)(defeatedModel->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW);
        taskCallExit(task);
        return;
    }
    // Allocate history and attach the disc and its clipping plane to the boss.
    if (task->state == 0) {
        newRipple = memCalloc(sizeof(Actor403600Ripple), false);
        if (newRipple != NULL) {
            task->work         = newRipple;
            newRipple->shallow = 0;
            discOffset         = D_actor_403600_80131E2C;
            actorRenderCopyCoordBodyTransform(task, &fxTask->parent->extra.tmd->coords[1], &discOffset);
            gfxSetRotIdentity(&newRipple->clipCoord.coord);
            newRipple->clipCoord.coord.t[0]   = 0;
            newRipple->clipCoord.coord.t[1]   = 0;
            newRipple->clipCoord.coord.t[2]   = 0;
            newRipple->clipCoord.composeStamp = GRAPHICS_COORD_DIRTY;
            newRipple->clipCoord.parent       = discCoord;
            spawnMode                         = task->spawnArg1.value;
            task->killCountdown               = ACTOR_403600_RIPPLE_EMISSION_TICKS;
            switch (spawnMode) {
                case ACTOR_403600_RIPPLE_SINK_MODEL:
                    newRipple->emitting = spawnMode;
                    warmupTick          = 0;
                    do {
                        _actor403600TickRipple(newRipple);
                        warmupTick += 1;
                    } while (warmupTick < ACTOR_403600_RIPPLE_WARMUP_TICKS);
                    newRipple->clipCountdown = ACTOR_403600_RIPPLE_SINK_CLIP_TICKS;
                    break;
                case ACTOR_403600_RIPPLE_RISE_MODEL:
                    task->killCountdown      = ACTOR_403600_RIPPLE_RISE_EMISSION_TICKS;
                    newRipple->clipCountdown = ACTOR_403600_RIPPLE_RISE_WAIT_TICKS;
                    gfxRotMatrixZ(&newRipple->clipCoord.coord, ACTOR_403600_RIPPLE_REVERSE_PLANE_ANGLE, GRAPHICS_ROTATION_COMPOSE);
                    discCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                    break;
                default:
                    newRipple->emitting = 1;
                    warmupTick          = 0;
                    do {
                        _actor403600TickRipple(newRipple);
                        warmupTick += 1;
                    } while (warmupTick < ACTOR_403600_RIPPLE_WARMUP_TICKS);
                    break;
            }
            task->state = task->state + 1;
        } else {
            taskCallExit(task);
            return;
        }
    }
    ripple = task->work;
    // Select the crossing plane; the wave keeps spreading after emission ends.
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        switch (task->spawnArg1.value) {
            case ACTOR_403600_RIPPLE_SINK_MODEL:
                sinkClipTicks         = ripple->clipCountdown - 1;
                ripple->clipCountdown = sinkClipTicks;
                if (sinkClipTicks == 0) {
                    sinkingModel                 = ((Task*)task->spawnArg2.pointer)->extra.tmd;
                    gActor403600RipplePlaneCoord = NULL;
                    sinkingModel->flags          = (u16)(sinkingModel->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
                } else if (sinkClipTicks > 0) {
                    gActor403600RipplePlaneCoord = &ripple->clipCoord;
                    actorRenderComposeCoord(&ripple->clipCoord);
                }
                break;
            case ACTOR_403600_RIPPLE_RISE_MODEL:
                riseClipTicks         = ripple->clipCountdown - 1;
                ripple->clipCountdown = riseClipTicks;
                if (riseClipTicks == 0) {
                    risingModel                  = ((Task*)task->spawnArg2.pointer)->extra.tmd;
                    gActor403600RipplePlaneCoord = &ripple->clipCoord;
                    risingModel->flags           = (u16)(risingModel->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW);
                    actorRenderComposeCoord(&ripple->clipCoord);
                } else if (riseClipTicks >= ACTOR_403600_RIPPLE_RISE_CLIP_LAST_TICK) {
                    gActor403600RipplePlaneCoord = &ripple->clipCoord;
                    actorRenderComposeCoord(&ripple->clipCoord);
                } else if (riseClipTicks == ACTOR_403600_RIPPLE_RISE_CLIP_END_TICK) {
                    gActor403600RipplePlaneCoord = NULL;
                }
                break;
        }
        if (task->killCountdown > 0) {
            ripple->emitting = 1;
        } else {
            ripple->emitting = 0;
        }
        task->killCountdown = (s16)((u16)task->killCountdown - 1);
        _actor403600TickRipple(ripple);
    }
    actor403600DrawRipple(ripple, discCoord);
    if (task->killCountdown <= 0 && _actor403600RippleHasDiedOut(ripple)) {
        taskCallExit(task);
    }
}

u32* actor403600DrawStreamGt3BottomFade(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    CVECTOR       lightColor;
    s16           fadeAnchorY;
    s16           displaceAnchorY;
    POLY_GT3*     packet;
    s32           fadeAmount;
    s32           fadeAnchorFromOrigin;
    s32           yDisplacement;
    s32           fadeDistance;
    s32           fadeStartY;
    s32*          gteResult;
    const u16*    offsets;
    const u8*     vertexBytes;
    const u8*     normalBytes;
    DisplayState* display;

    packet       = (POLY_GT3*)workspace->primWrite;
    lightColor   = _gActor403600NeutralLightColor;
    fadeDistance = workspace->obj->shading.screenFadeDistance;
    if (workspace->elemCount-- > 0) {
        gteResult  = &workspace->gteResult;
        fadeStartY = ACTOR_403600_SCREEN_FADE_ORIGIN_Y - fadeDistance;
        display    = &gDisplayState;
        do {
            offsets     = (const u16*)elements;
            vertexBytes = (const u8*)workspace->verts;
            gte_ldv3(vertexBytes + (offsets[0] & ACTOR_403600_GEOMETRY_OFFSET_MASK), vertexBytes + (offsets[1] & ACTOR_403600_GEOMETRY_OFFSET_MASK),
                     vertexBytes + (offsets[2] & ACTOR_403600_GEOMETRY_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(&workspace->gteFlag);
            if (workspace->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(gteResult);
                if (workspace->gteResult > 0) {
                    gte_stsxy3_gt3(packet);
                    gte_avsz3();
                    // Fade from corner 0; every corner uses this same colour weight.
                    fadeAmount = 0;
                    if (fadeDistance != 0) {
                        fadeAnchorY = packet->y0;
                        if (fadeStartY < fadeAnchorY) {
                            fadeAnchorFromOrigin = fadeAnchorY - ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                            fadeAmount           = (fadeAnchorFromOrigin + fadeDistance) * 2;
                        }
                    }
                    if (fadeAmount >= (ACTOR_403600_SCREEN_FADE_COLOR_ONE + 1)) {
                        GPU_PRIMITIVE_COLOR_WORD(packet, 0) = 0;
                        GPU_PRIMITIVE_COLOR_WORD(packet, 1) = 0;
                        GPU_PRIMITIVE_COLOR_WORD(packet, 2) = 0;
                    } else {
                        ACTOR_403600_LIGHT_GT3_SCREEN_FADE(workspace, lightColor, fadeAmount, offsets, packet, normalBytes);
                    }
                    // Reflect corner 0 about the fade boundary by translating every Y.
                    if (fadeDistance != 0) {
                        displaceAnchorY = packet->y0;
                        if (fadeStartY < displaceAnchorY) {
                            yDisplacement  = displaceAnchorY;
                            yDisplacement -= ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                            yDisplacement += fadeDistance;
                            yDisplacement *= 2;
                            packet->y1    -= yDisplacement;
                            packet->y2    -= yDisplacement;
                            packet->y0    -= yDisplacement;
                        }
                    }
                    setPolyGT3(packet);
                    setSemiTrans(packet, 1);
                    gte_stotz(gteResult);
                    addPrim(&workspace->ot[((u32)workspace->gteResult << display->otDepthShift) >> ACTOR_403600_OT_DEPTH_SHIFT & ACTOR_403600_OT_INDEX_MASK], packet);
                }
            }
            packet++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)packet;
    return elements;
}

u32* actor403600DrawStreamGt3PreXformBottomFade(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_GT3*     packet;
    s32*          gteResult;
    DisplayState* display;
    u32           invalidDepthMask;
    const u16*    offsets;
    s32           fadeDistance;
    s32           fadeStartY;
    s32           fadeAmount;
    s32           fadeAnchorFromOrigin;
    s32           yDisplacement;
    s32           vertexDepth;
    s32           depthOffsetBytes;
    s32*          depths;
    s16           fadeAnchorY;
    s16           displaceAnchorY;

    packet       = (POLY_GT3*)workspace->preXformWrite;
    fadeDistance = workspace->obj->shading.screenFadeDistance;
    if (workspace->elemCount-- > 0) {
        gteResult        = &workspace->gteResult;
        invalidDepthMask = TMD_VERTEX_DEPTH_INVALID;
        fadeStartY       = ACTOR_403600_SCREEN_FADE_ORIGIN_Y - fadeDistance;
        display          = &gDisplayState;
        do {
            offsets = (const u16*)elements;
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(packet, 0));
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(packet, 1));
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(packet, 2));
            gte_nclip();
            gte_stopz(gteResult);
            if (workspace->gteResult > 0) {
                depths           = workspace->szTable;
                depthOffsetBytes = offsets[0] & ACTOR_403600_DEPTH_OFFSET_MASK;
                vertexDepth      = depths[(u32)depthOffsetBytes / sizeof(*depths)];
                if (!(vertexDepth & invalidDepthMask)) {
                    gte_ldSZ1(vertexDepth);
                    depthOffsetBytes = offsets[1] & ACTOR_403600_DEPTH_OFFSET_MASK;
                    vertexDepth      = depths[(u32)depthOffsetBytes / sizeof(*depths)];
                    if (!(vertexDepth & invalidDepthMask)) {
                        gte_ldSZ2(vertexDepth);
                        depthOffsetBytes = offsets[2] & ACTOR_403600_DEPTH_OFFSET_MASK;
                        vertexDepth      = depths[(u32)depthOffsetBytes / sizeof(*depths)];
                        if (!(vertexDepth & invalidDepthMask)) {
                            gte_ldSZ3(vertexDepth);
                            gte_avsz3();
                            // Fade from corner 0; every corner uses this same colour weight.
                            fadeAmount = 0;
                            if (fadeDistance != 0) {
                                fadeAnchorY = packet->y0;
                                if (fadeStartY < fadeAnchorY) {
                                    fadeAnchorFromOrigin = fadeAnchorY - ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                                    fadeAmount           = (fadeAnchorFromOrigin + fadeDistance) * 2;
                                }
                                if (fadeAmount >= (ACTOR_403600_SCREEN_FADE_COLOR_ONE + 1)) {
                                    GPU_PRIMITIVE_COLOR_WORD(packet, 0) = 0;
                                    GPU_PRIMITIVE_COLOR_WORD(packet, 1) = 0;
                                    GPU_PRIMITIVE_COLOR_WORD(packet, 2) = 0;
                                } else {
                                    ACTOR_403600_FADE_PRE_XFORM_GT3_COLORS(packet, fadeAmount);
                                }
                            }
                            if (fadeDistance != 0) {
                                displaceAnchorY = packet->y0;
                                if (fadeStartY < displaceAnchorY) {
                                    yDisplacement  = displaceAnchorY;
                                    yDisplacement -= ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                                    yDisplacement += fadeDistance;
                                    yDisplacement *= 2;
                                    packet->y1    -= yDisplacement;
                                    packet->y2    -= yDisplacement;
                                    packet->y0    -= yDisplacement;
                                }
                            }
                            setPolyGT3(packet);
                            setSemiTrans(packet, 1);
                            gte_stotz(gteResult);
                            addPrim(&workspace->ot[((u32)workspace->gteResult << display->otDepthShift) >> ACTOR_403600_OT_DEPTH_SHIFT & ACTOR_403600_OT_INDEX_MASK], packet);
                        }
                    }
                }
            }
            packet++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->preXformWrite = (u8*)packet;
    return elements;
}

/// Tests whether a quad's second triangle faces the viewer.
///
/// The caller must leave projected corners 1, 2 and 3 in the GTE screen FIFO,
/// in that order. `signedAreaDestination` must be the cached address of the
/// writable `workspace->gteResult` word. Stores the signed screen-space area
/// there and returns 1 for a negative area, 0 otherwise. The corner order
/// reverses the second triangle's winding relative to the first triangle's
/// positive NCLIP(0,1,2) test. A zero area is rejected. The caller uses this
/// fallback only when the first triangle's area is nonpositive.
static inline s32 _actor403600SecondHalfFacesViewer(TmdStreamWorkspace* workspace, s32* signedAreaDestination)
{
    gte_nclip();
    gte_stopz(signedAreaDestination);
    return workspace->gteResult < 0;
}

/// Tests a pre-transformed quad's second triangle for facing the viewer.
///
/// The GTE screen FIFO must hold this packet's projected corners 0, 1 and 2,
/// in that order. Pushes its packed signed pixel coordinates for corner 3,
/// leaving corners 1, 2 and 3 for `_actor403600SecondHalfFacesViewer`.
/// `packet` must provide a readable, word-aligned POLY_GT4; it is not changed.
/// `signedAreaDestination` must be the cached address of the writable
/// `workspace->gteResult` word. Returns 1 when the stored signed area is
/// negative, 0 otherwise, including a degenerate triangle. Call only after
/// the first triangle's NCLIP(0,1,2) result is nonpositive.
static inline s32 _actor403600PreXformSecondHalfFacesViewer(const POLY_GT4* packet, TmdStreamWorkspace* workspace, s32* signedAreaDestination)
{
    gte_ldSXYP(*(const u32*)&packet->x3);
    return _actor403600SecondHalfFacesViewer(workspace, signedAreaDestination);
}

u32* actor403600DrawStreamGt4BottomFade(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    CVECTOR       lightColor;
    s16           fadeAnchorY;
    s16           displaceAnchorY;
    POLY_GT4*     packet;
    s32           fadeAmount;
    s32           fadeAnchorFromOrigin;
    s32           yDisplacement;
    s32           fadeDistance;
    s32           fadeStartY;
    s32*          gteResult;
    s32*          gteFlag;
    u32           gteErrorMask;
    const u16*    offsets;
    const u8*     vertexBytes;
    const u8*     normalBytes;
    DisplayState* display;

    packet       = (POLY_GT4*)workspace->primWrite;
    lightColor   = _gActor403600NeutralLightColor;
    fadeDistance = workspace->obj->shading.screenFadeDistance;
    gte_ldrgb(&lightColor);
    if (workspace->elemCount-- > 0) {
        gteFlag      = &workspace->gteFlag;
        gteErrorMask = TMD_GTE_ERROR_FLAG;
        gteResult    = &workspace->gteResult;
        fadeStartY   = ACTOR_403600_SCREEN_FADE_ORIGIN_Y - fadeDistance;
        display      = &gDisplayState;
        do {
            offsets     = (const u16*)elements;
            vertexBytes = (const u8*)workspace->verts;
            gte_ldv3(vertexBytes + (offsets[0] & ACTOR_403600_GEOMETRY_OFFSET_MASK), vertexBytes + (offsets[1] & ACTOR_403600_GEOMETRY_OFFSET_MASK),
                     vertexBytes + (offsets[2] & ACTOR_403600_GEOMETRY_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(gteFlag);
            if (!(workspace->gteFlag & gteErrorMask)) {
                gte_nclip();
                gte_stopz(gteResult);
                gte_stsxy3_gt4(packet);
                gte_ldv0((const u8*)workspace->verts + (offsets[3] & ACTOR_403600_GEOMETRY_OFFSET_MASK));
                gte_rtps();
                gte_stflg(gteFlag);
                if (!(workspace->gteFlag & gteErrorMask)) {
                    if (workspace->gteResult > 0 || _actor403600SecondHalfFacesViewer(workspace, gteResult)) {
                        gte_stsxy2(&packet->x3);
                        gte_avsz4();
                        // Fade from corner 0; every corner uses this same colour weight.
                        fadeAmount = 0;
                        if (fadeDistance != 0) {
                            fadeAnchorY = packet->y0;
                            if (fadeStartY < fadeAnchorY) {
                                fadeAnchorFromOrigin = fadeAnchorY - ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                                fadeAmount           = (fadeAnchorFromOrigin + fadeDistance) * 2;
                            }
                        }
                        if (fadeAmount >= (ACTOR_403600_SCREEN_FADE_COLOR_ONE + 1)) {
                            GPU_PRIMITIVE_COLOR_WORD(packet, 0) = 0;
                            GPU_PRIMITIVE_COLOR_WORD(packet, 1) = 0;
                            GPU_PRIMITIVE_COLOR_WORD(packet, 2) = 0;
                            GPU_PRIMITIVE_COLOR_WORD(packet, 3) = 0;
                        } else {
                            ACTOR_403600_LIGHT_GT4_SCREEN_FADE(workspace, lightColor, fadeAmount, offsets, packet, normalBytes);
                        }
                        if (fadeDistance != 0) {
                            displaceAnchorY = packet->y0;
                            if (fadeStartY < displaceAnchorY) {
                                yDisplacement  = displaceAnchorY;
                                yDisplacement -= ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                                yDisplacement += fadeDistance;
                                yDisplacement *= 2;
                                packet->y3    -= yDisplacement;
                                packet->y2    -= yDisplacement;
                                packet->y1    -= yDisplacement;
                                packet->y0    -= yDisplacement;
                            }
                        }
                        setPolyGT4(packet);
                        setSemiTrans(packet, 1);
                        gte_stotz(gteResult);
                        addPrim(&workspace->ot[((u32)workspace->gteResult << display->otDepthShift) >> ACTOR_403600_OT_DEPTH_SHIFT & ACTOR_403600_OT_INDEX_MASK], packet);
                    }
                }
            }
            packet++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)packet;
    return elements;
}

u32* actor403600DrawStreamGt4PreXformBottomFade(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_GT4*     packet;
    s32*          gteResult;
    DisplayState* display;
    u32           invalidDepthMask;
    const u16*    offsets;
    s32           fadeDistance;
    s32           fadeStartY;
    s32           fadeAmount;
    s32           fadeAnchorFromOrigin;
    s32           yDisplacement;
    s32           vertexDepth;
    s32           depthOffsetBytes;
    s32*          depths;
    s16           fadeAnchorY;
    s16           displaceAnchorY;

    packet       = (POLY_GT4*)workspace->preXformWrite;
    fadeDistance = workspace->obj->shading.screenFadeDistance;
    if (workspace->elemCount-- > 0) {
        gteResult        = &workspace->gteResult;
        invalidDepthMask = TMD_VERTEX_DEPTH_INVALID;
        fadeStartY       = ACTOR_403600_SCREEN_FADE_ORIGIN_Y - fadeDistance;
        display          = &gDisplayState;
        do {
            offsets = (const u16*)elements;
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(packet, 0));
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(packet, 1));
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(packet, 2));
            gte_nclip();
            gte_stopz(gteResult);
            if (workspace->gteResult > 0 || _actor403600PreXformSecondHalfFacesViewer(packet, workspace, gteResult)) {
                depths           = workspace->szTable;
                depthOffsetBytes = offsets[0] & ACTOR_403600_DEPTH_OFFSET_MASK;
                vertexDepth      = depths[(u32)depthOffsetBytes / sizeof(*depths)];
                if (!(vertexDepth & invalidDepthMask)) {
                    gte_ldSZ0(vertexDepth);
                    depthOffsetBytes = offsets[1] & ACTOR_403600_DEPTH_OFFSET_MASK;
                    vertexDepth      = depths[(u32)depthOffsetBytes / sizeof(*depths)];
                    if (!(vertexDepth & invalidDepthMask)) {
                        gte_ldSZ1(vertexDepth);
                        depthOffsetBytes = offsets[2] & ACTOR_403600_DEPTH_OFFSET_MASK;
                        vertexDepth      = depths[(u32)depthOffsetBytes / sizeof(*depths)];
                        if (!(vertexDepth & invalidDepthMask)) {
                            gte_ldSZ2(vertexDepth);
                            depthOffsetBytes = offsets[3] & ACTOR_403600_DEPTH_OFFSET_MASK;
                            vertexDepth      = depths[(u32)depthOffsetBytes / sizeof(*depths)];
                            if (!(vertexDepth & invalidDepthMask)) {
                                gte_ldSZ3(vertexDepth);
                                gte_avsz4();
                                // Fade from corner 0; every corner uses this same colour weight.
                                fadeAmount = 0;
                                if (fadeDistance != 0) {
                                    fadeAnchorY = packet->y0;
                                    if (fadeStartY < fadeAnchorY) {
                                        fadeAnchorFromOrigin = fadeAnchorY - ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                                        fadeAmount           = (fadeAnchorFromOrigin + fadeDistance) * 2;
                                    }
                                    if (fadeAmount >= (ACTOR_403600_SCREEN_FADE_COLOR_ONE + 1)) {
                                        GPU_PRIMITIVE_COLOR_WORD(packet, 0) = 0;
                                        GPU_PRIMITIVE_COLOR_WORD(packet, 1) = 0;
                                        GPU_PRIMITIVE_COLOR_WORD(packet, 2) = 0;
                                        GPU_PRIMITIVE_COLOR_WORD(packet, 3) = 0;
                                    } else {
                                        ACTOR_403600_FADE_PRE_XFORM_GT4_COLORS(packet, fadeAmount);
                                    }
                                }
                                if (fadeDistance != 0) {
                                    displaceAnchorY = packet->y0;
                                    if (fadeStartY < displaceAnchorY) {
                                        yDisplacement  = displaceAnchorY;
                                        yDisplacement -= ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                                        yDisplacement += fadeDistance;
                                        yDisplacement *= 2;
                                        packet->y3    -= yDisplacement;
                                        packet->y2    -= yDisplacement;
                                        packet->y1    -= yDisplacement;
                                        packet->y0    -= yDisplacement;
                                    }
                                }
                                setPolyGT4(packet);
                                setSemiTrans(packet, 1);
                                gte_stotz(gteResult);
                                gte_stotz(gteResult);
                                addPrim(&workspace->ot[((u32)workspace->gteResult << display->otDepthShift) >> ACTOR_403600_OT_DEPTH_SHIFT & ACTOR_403600_OT_INDEX_MASK], packet);
                            }
                        }
                    }
                }
            }
            packet++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->preXformWrite = (u8*)packet;
    return elements;
}

u32* actor403600DrawStreamGt3TopDisplace(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    CVECTOR       lightColor;
    s16           fadeAnchorY;
    s16           displaceAnchorY;
    POLY_GT3*     packet;
    s32           fadeAmount;
    s32           fadeAnchorFromOrigin;
    s32           yDisplacement;
    s32           fadeDistance;
    s32           fadeStartY;
    s32*          gteResult;
    const u16*    offsets;
    const u8*     vertexBytes;
    const u8*     normalBytes;
    DisplayState* display;

    packet       = (POLY_GT3*)workspace->primWrite;
    lightColor   = _gActor403600NeutralLightColor;
    fadeDistance = workspace->obj->shading.screenFadeDistance;
    if (workspace->elemCount-- > 0) {
        gteResult  = &workspace->gteResult;
        fadeStartY = ACTOR_403600_SCREEN_FADE_ORIGIN_Y - fadeDistance;
        display    = &gDisplayState;
        do {
            offsets     = (const u16*)elements;
            vertexBytes = (const u8*)workspace->verts;
            gte_ldv3(vertexBytes + (offsets[0] & ACTOR_403600_GEOMETRY_OFFSET_MASK), vertexBytes + (offsets[1] & ACTOR_403600_GEOMETRY_OFFSET_MASK),
                     vertexBytes + (offsets[2] & ACTOR_403600_GEOMETRY_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(&workspace->gteFlag);
            if (workspace->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(gteResult);
                if (workspace->gteResult > 0) {
                    gte_stsxy3_gt3(packet);
                    gte_avsz3();
                    // Fade from corner 0; every corner uses this same colour weight.
                    fadeAmount = 0;
                    if (fadeDistance != 0) {
                        fadeAnchorY = packet->y0;
                        if (fadeStartY < fadeAnchorY) {
                            fadeAnchorFromOrigin = fadeAnchorY - ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                            fadeAmount           = (fadeAnchorFromOrigin + fadeDistance) * 2;
                        }
                    }
                    if (fadeAmount >= (ACTOR_403600_SCREEN_FADE_COLOR_ONE + 1)) {
                        GPU_PRIMITIVE_COLOR_WORD(packet, 0) = 0;
                        GPU_PRIMITIVE_COLOR_WORD(packet, 1) = 0;
                        GPU_PRIMITIVE_COLOR_WORD(packet, 2) = 0;
                    } else {
                        ACTOR_403600_LIGHT_GT3_SCREEN_FADE(workspace, lightColor, fadeAmount, offsets, packet, normalBytes);
                    }
                    setPolyGT3(packet);
                    // Translate the whole packet upward beyond the top threshold.
                    if (fadeDistance != 0) {
                        displaceAnchorY = packet->y0;
                        if (displaceAnchorY < (fadeDistance - ACTOR_403600_SCREEN_FADE_ORIGIN_Y)) {
                            yDisplacement  = displaceAnchorY;
                            yDisplacement += ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                            yDisplacement -= fadeDistance;
                            yDisplacement *= 2;
                            packet->y1    += yDisplacement;
                            packet->y2    += yDisplacement;
                            packet->y0    += yDisplacement;
                            setSemiTrans(packet, 1);
                        }
                    }
                    setShadeTex(packet, 0);
                    gte_stotz(gteResult);
                    addPrim(&workspace->ot[((u32)workspace->gteResult << display->otDepthShift) >> ACTOR_403600_OT_DEPTH_SHIFT & ACTOR_403600_OT_INDEX_MASK], packet);
                }
            }
            packet++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)packet;
    return elements;
}

u32* actor403600DrawStreamGt3TopDisplaceSemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    CVECTOR       lightColor;
    s16           fadeAnchorY;
    s16           displaceAnchorY;
    POLY_GT3*     packet;
    s32           fadeAmount;
    s32           fadeAnchorFromOrigin;
    s32           yDisplacement;
    s32           fadeDistance;
    s32           fadeStartY;
    s32*          gteResult;
    const u16*    offsets;
    const u8*     vertexBytes;
    const u8*     normalBytes;
    DisplayState* display;

    packet       = (POLY_GT3*)workspace->primWrite;
    lightColor   = _gActor403600NeutralLightColor;
    fadeDistance = workspace->obj->shading.screenFadeDistance;
    if (workspace->elemCount-- > 0) {
        gteResult  = &workspace->gteResult;
        fadeStartY = ACTOR_403600_SCREEN_FADE_ORIGIN_Y - fadeDistance;
        display    = &gDisplayState;
        do {
            offsets     = (const u16*)elements;
            vertexBytes = (const u8*)workspace->verts;
            gte_ldv3(vertexBytes + (offsets[0] & ACTOR_403600_GEOMETRY_OFFSET_MASK), vertexBytes + (offsets[1] & ACTOR_403600_GEOMETRY_OFFSET_MASK),
                     vertexBytes + (offsets[2] & ACTOR_403600_GEOMETRY_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(&workspace->gteFlag);
            if (workspace->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(gteResult);
                if (workspace->gteResult > 0) {
                    gte_stsxy3_gt3(packet);
                    gte_avsz3();
                    // Fade from corner 0; every corner uses this same colour weight.
                    fadeAmount = 0;
                    if (fadeDistance != 0) {
                        fadeAnchorY = packet->y0;
                        if (fadeStartY < fadeAnchorY) {
                            fadeAnchorFromOrigin = fadeAnchorY - ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                            fadeAmount           = (fadeAnchorFromOrigin + fadeDistance) * 2;
                        }
                    }
                    if (fadeAmount >= (ACTOR_403600_SCREEN_FADE_COLOR_ONE + 1)) {
                        GPU_PRIMITIVE_COLOR_WORD(packet, 0) = 0;
                        GPU_PRIMITIVE_COLOR_WORD(packet, 1) = 0;
                        GPU_PRIMITIVE_COLOR_WORD(packet, 2) = 0;
                    } else {
                        ACTOR_403600_LIGHT_GT3_SCREEN_FADE(workspace, lightColor, fadeAmount, offsets, packet, normalBytes);
                    }
                    setPolyGT3(packet);
                    // Translate the whole packet upward beyond the top threshold.
                    if (fadeDistance != 0) {
                        displaceAnchorY = packet->y0;
                        if (displaceAnchorY < (fadeDistance - ACTOR_403600_SCREEN_FADE_ORIGIN_Y)) {
                            yDisplacement  = displaceAnchorY;
                            yDisplacement += ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                            yDisplacement -= fadeDistance;
                            yDisplacement *= 2;
                            packet->y1    += yDisplacement;
                            packet->y2    += yDisplacement;
                            packet->y0    += yDisplacement;
                            setSemiTrans(packet, 1);
                        }
                    }
                    setShadeTex(packet, 0);
                    setSemiTrans(packet, 1);
                    gte_stotz(gteResult);
                    addPrim(&workspace->ot[((u32)workspace->gteResult << display->otDepthShift) >> ACTOR_403600_OT_DEPTH_SHIFT & ACTOR_403600_OT_INDEX_MASK], packet);
                }
            }
            packet++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)packet;
    return elements;
}

u32* actor403600DrawStreamGt4TopDisplace(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    CVECTOR       lightColor;
    s16           fadeAnchorY;
    s16           displaceAnchorY;
    POLY_GT4*     packet;
    s32           fadeAmount;
    s32           fadeAnchorFromOrigin;
    s32           yDisplacement;
    s32           fadeDistance;
    s32           fadeStartY;
    s32*          gteResult;
    s32*          gteFlag;
    u32           gteErrorMask;
    const u16*    offsets;
    const u8*     vertexBytes;
    const u8*     normalBytes;
    DisplayState* display;

    packet       = (POLY_GT4*)workspace->primWrite;
    lightColor   = _gActor403600NeutralLightColor;
    fadeDistance = workspace->obj->shading.screenFadeDistance;
    gte_ldrgb(&lightColor);
    if (workspace->elemCount-- > 0) {
        gteFlag      = &workspace->gteFlag;
        gteErrorMask = TMD_GTE_ERROR_FLAG;
        gteResult    = &workspace->gteResult;
        fadeStartY   = ACTOR_403600_SCREEN_FADE_ORIGIN_Y - fadeDistance;
        display      = &gDisplayState;
        do {
            offsets     = (const u16*)elements;
            vertexBytes = (const u8*)workspace->verts;
            gte_ldv3(vertexBytes + (offsets[0] & ACTOR_403600_GEOMETRY_OFFSET_MASK), vertexBytes + (offsets[1] & ACTOR_403600_GEOMETRY_OFFSET_MASK),
                     vertexBytes + (offsets[2] & ACTOR_403600_GEOMETRY_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(gteFlag);
            if (!(workspace->gteFlag & gteErrorMask)) {
                gte_nclip();
                gte_stopz(gteResult);
                gte_stsxy3_gt4(packet);
                gte_ldv0((const u8*)workspace->verts + (offsets[3] & ACTOR_403600_GEOMETRY_OFFSET_MASK));
                gte_rtps();
                gte_stflg(gteFlag);
                if (!(workspace->gteFlag & gteErrorMask)) {
                    if (workspace->gteResult > 0 || _actor403600SecondHalfFacesViewer(workspace, gteResult)) {
                        gte_stsxy2(&packet->x3);
                        gte_avsz4();
                        // Fade from corner 0; every corner uses this same colour weight.
                        fadeAmount = 0;
                        if (fadeDistance != 0) {
                            fadeAnchorY = packet->y0;
                            if (fadeStartY < fadeAnchorY) {
                                fadeAnchorFromOrigin = fadeAnchorY - ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                                fadeAmount           = (fadeAnchorFromOrigin + fadeDistance) * 2;
                            }
                        }
                        if (fadeAmount >= (ACTOR_403600_SCREEN_FADE_COLOR_ONE + 1)) {
                            GPU_PRIMITIVE_COLOR_WORD(packet, 0) = 0;
                            GPU_PRIMITIVE_COLOR_WORD(packet, 1) = 0;
                            GPU_PRIMITIVE_COLOR_WORD(packet, 2) = 0;
                            GPU_PRIMITIVE_COLOR_WORD(packet, 3) = 0;
                        } else {
                            ACTOR_403600_LIGHT_GT4_SCREEN_FADE(workspace, lightColor, fadeAmount, offsets, packet, normalBytes);
                        }
                        setPolyGT4(packet);
                        if (fadeDistance != 0) {
                            displaceAnchorY = packet->y0;
                            if (displaceAnchorY < (fadeDistance - ACTOR_403600_SCREEN_FADE_ORIGIN_Y)) {
                                yDisplacement  = displaceAnchorY;
                                yDisplacement += ACTOR_403600_SCREEN_FADE_ORIGIN_Y;
                                yDisplacement -= fadeDistance;
                                yDisplacement *= 2;
                                packet->y3    += yDisplacement;
                                packet->y2    += yDisplacement;
                                packet->y1    += yDisplacement;
                                packet->y0    += yDisplacement;
                                setSemiTrans(packet, 1);
                            }
                        }
                        setShadeTex(packet, 0);
                        gte_stotz(gteResult);
                        addPrim(&workspace->ot[((u32)workspace->gteResult << display->otDepthShift) >> ACTOR_403600_OT_DEPTH_SHIFT & ACTOR_403600_OT_INDEX_MASK], packet);
                    }
                }
            }
            packet++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)packet;
    return elements;
}

#undef ACTOR_403600_LIGHT_GT3_SCREEN_FADE
#undef ACTOR_403600_LIGHT_GT4_SCREEN_FADE
#undef ACTOR_403600_FADE_PRE_XFORM_GT3_COLORS
#undef ACTOR_403600_FADE_PRE_XFORM_GT4_COLORS

u32* actor403600DrawStreamGt3PlaneClamp(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    CVECTOR                 color;
    u8*                     savedScratchHead;
    _Actor403600TriScratch* scratch;
    GfxCoord*               planeCoord;
    POLY_GT3*               packet;
    const u16*              offsets;
    s32                     cornerIndex;

    if (gActor403600RipplePlaneCoord != NULL) {
        packet           = (POLY_GT3*)workspace->primWrite;
        color            = _gActor403600NeutralLightColor;
        savedScratchHead = SCRATCH_STACK_CURSOR(u8);
        scratch          = SCRATCH_STACK_CURSOR(_Actor403600TriScratch) =
            (_Actor403600TriScratch*)(savedScratchHead - sizeof(_Actor403600TriScratch));
        // Save the part transform and compose it into the ripple plane frame.
        gte_sttr(&scratch->trans);
        gte_ReadRotMatrix(&scratch->savedRot);
        TransposeMatrix(&gActor403600RipplePlaneCoord->workm, &scratch->local);
        planeCoord         = gActor403600RipplePlaneCoord;
        scratch->offset.vx = scratch->trans.vx - planeCoord->workm.t[0];
        scratch->offset.vy = scratch->trans.vy - planeCoord->workm.t[1];
        scratch->offset.vz = scratch->trans.vz - planeCoord->workm.t[2];
        _gfxRotateSv(&scratch->local, &scratch->offset);
        gte_MulMatrix0(&scratch->local, &scratch->savedRot, &scratch->local);
        scratch->local.t[0] = scratch->offset.vx;
        scratch->local.t[1] = scratch->offset.vy;
        scratch->local.t[2] = scratch->offset.vz;
        gte_ldrgb(&color);
        for (; workspace->elemCount-- > 0; packet++, elements += workspace->elemStride) {
            offsets = (const u16*)elements;
            // Flatten the positive-Y side, then project back through the plane.
            gte_SetTransMatrix(&scratch->local);
            gte_SetRotMatrix(&scratch->local);
            scratch->index[0] = offsets[0] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT;
            scratch->index[1] = offsets[1] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT;
            scratch->index[2] = offsets[2] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT;
            for (cornerIndex = 0; cornerIndex < (s32)ARRAY_SIZE(scratch->verts); cornerIndex++) {
                ACTOR_403600_TRANSFORM_AND_CLAMP_PLANE_VERTEX(&workspace->verts[scratch->index[cornerIndex]], scratch->verts[cornerIndex]);
            }
            gte_SetRotMatrix(&gActor403600RipplePlaneCoord->workm);
            gte_SetTransMatrix(&gActor403600RipplePlaneCoord->workm);
            gte_ldv3(&scratch->verts[0], &scratch->verts[1], &scratch->verts[2]);
            gte_rtpt();
            gte_stflg(&workspace->gteFlag);
            if (workspace->gteFlag & TMD_GTE_ERROR_FLAG) {
                continue;
            }
            gte_nclip();
            gte_stopz(&workspace->gteResult);
            if (workspace->gteResult <= 0) {
                continue;
            }
            gte_stsxy3_gt3(packet);
            gte_avsz3();
            gte_ldv3(&workspace->normals[offsets[3] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT], &workspace->normals[offsets[4] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT], &workspace->normals[offsets[5] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT]);
            gte_ncct();
            gte_strgb3_gt3(packet);
            setPolyGT3(packet);
            gte_stotz(&workspace->gteResult);
            addPrim(&workspace->ot[((u32)workspace->gteResult << gDisplayState.otDepthShift) >> ACTOR_403600_OT_DEPTH_SHIFT & ACTOR_403600_OT_INDEX_MASK], packet);
        }
        workspace->primWrite = (u8*)packet;
        // The following stream records still require the original part transform.
        gte_SetTransVector(&scratch->trans);
        gte_SetRotMatrix(&scratch->savedRot);
        SCRATCH_STACK_RELEASE_BYTES(sizeof(_Actor403600TriScratch));
        return elements;
    }
    return tmdDrawStreamGt3(workspace, objectFlags, elements);
}

u32* actor403600DrawStreamGt4PlaneClamp(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    CVECTOR                  color;
    u8*                      savedScratchHead;
    _Actor403600QuadScratch* scratch;
    GfxCoord*                planeCoord;
    POLY_GT4*                packet;
    const u16*               offsets;
    s32                      cornerIndex;

    if (gActor403600RipplePlaneCoord != NULL) {
        packet           = (POLY_GT4*)workspace->primWrite;
        color            = _gActor403600NeutralLightColor;
        savedScratchHead = SCRATCH_STACK_CURSOR(u8);
        scratch          = SCRATCH_STACK_CURSOR(_Actor403600QuadScratch) =
            (_Actor403600QuadScratch*)(savedScratchHead - sizeof(_Actor403600QuadScratch));
        // Save the part transform and compose it into the ripple plane frame.
        gte_sttr(&scratch->trans);
        gte_ReadRotMatrix(&scratch->savedRot);
        TransposeMatrix(&gActor403600RipplePlaneCoord->workm, &scratch->local);
        planeCoord         = gActor403600RipplePlaneCoord;
        scratch->offset.vx = scratch->trans.vx - planeCoord->workm.t[0];
        scratch->offset.vy = scratch->trans.vy - planeCoord->workm.t[1];
        scratch->offset.vz = scratch->trans.vz - planeCoord->workm.t[2];
        _gfxRotateSv(&scratch->local, &scratch->offset);
        gte_MulMatrix0(&scratch->local, &scratch->savedRot, &scratch->local);
        scratch->local.t[0] = scratch->offset.vx;
        scratch->local.t[1] = scratch->offset.vy;
        scratch->local.t[2] = scratch->offset.vz;
        gte_ldrgb(&color);
        for (; workspace->elemCount-- > 0; packet++, elements += workspace->elemStride) {
            offsets = (const u16*)elements;
            // Flatten the positive-Y side, then project back through the plane.
            gte_SetTransMatrix(&scratch->local);
            gte_SetRotMatrix(&scratch->local);
            scratch->index[0] = offsets[0] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT;
            scratch->index[1] = offsets[1] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT;
            scratch->index[2] = offsets[2] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT;
            scratch->index[3] = offsets[3] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT;
            for (cornerIndex = 0; cornerIndex < (s32)ARRAY_SIZE(scratch->verts); cornerIndex++) {
                ACTOR_403600_TRANSFORM_AND_CLAMP_PLANE_VERTEX(&workspace->verts[scratch->index[cornerIndex]], scratch->verts[cornerIndex]);
            }
            gte_SetRotMatrix(&gActor403600RipplePlaneCoord->workm);
            gte_SetTransMatrix(&gActor403600RipplePlaneCoord->workm);
            gte_ldv3(&scratch->verts[0], &scratch->verts[1], &scratch->verts[2]);
            gte_rtpt();
            gte_stflg(&workspace->gteFlag);
            if (workspace->gteFlag & TMD_GTE_ERROR_FLAG) {
                continue;
            }
            gte_nclip();
            gte_stopz(&workspace->gteResult);
            gte_stsxy3_gt4(packet);
            gte_ldv0(&scratch->verts[3]);
            gte_rtps();
            gte_stflg(&workspace->gteFlag);
            if (workspace->gteFlag & TMD_GTE_ERROR_FLAG) {
                continue;
            }
            // Either triangle can establish the quad's facing.
            if (workspace->gteResult <= 0) {
                gte_nclip();
                gte_stopz(&workspace->gteResult);
                if (workspace->gteResult >= 0) {
                    continue;
                }
            }
            gte_stsxy2(&packet->x3);
            gte_avsz4();
            gte_ldv3(&workspace->normals[offsets[4] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT], &workspace->normals[offsets[5] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT], &workspace->normals[offsets[6] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT]);
            gte_ncct();
            gte_strgb3_gt4(packet);
            gte_ldv0(&workspace->normals[offsets[7] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT]);
            gte_nccs();
            gte_strgb(&packet->r3);
            setPolyGT4(packet);
            gte_stotz(&workspace->gteResult);
            addPrim(&workspace->ot[((u32)workspace->gteResult << gDisplayState.otDepthShift) >> ACTOR_403600_OT_DEPTH_SHIFT & ACTOR_403600_OT_INDEX_MASK], packet);
        }
        workspace->primWrite = (u8*)packet;
        // The following stream records still require the original part transform.
        gte_SetTransVector(&scratch->trans);
        gte_SetRotMatrix(&scratch->savedRot);
        SCRATCH_STACK_RELEASE_BYTES(sizeof(_Actor403600QuadScratch));
        return elements;
    }
    return tmdDrawStreamGt4(workspace, objectFlags, elements);
}

u32* actor403600XformStreamVertsPlaneClamp(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    CVECTOR                 color;
    u8*                     savedScratchHead;
    _Actor403600TriScratch* scratch;
    GfxCoord*               planeCoord;
    const u16*              offsets;
    s32                     previousVertexOffset;

    if (gActor403600RipplePlaneCoord != NULL) {
        enum { ACTOR_403600_NO_PREVIOUS_VERTEX = -1 };

        previousVertexOffset = ACTOR_403600_NO_PREVIOUS_VERTEX;
        color                = _gActor403600NeutralLightColor;
        if (workspace->elemCount == 0) {
            return elements;
        }
        savedScratchHead = SCRATCH_STACK_CURSOR(u8);
        scratch          = SCRATCH_STACK_CURSOR(_Actor403600TriScratch) =
            (_Actor403600TriScratch*)(savedScratchHead - sizeof(_Actor403600TriScratch));
        // Save the part transform and compose it into the ripple plane frame.
        gte_sttr(&scratch->trans);
        gte_ReadRotMatrix(&scratch->savedRot);
        TransposeMatrix(&gActor403600RipplePlaneCoord->workm, &scratch->local);
        planeCoord         = gActor403600RipplePlaneCoord;
        scratch->offset.vx = scratch->trans.vx - planeCoord->workm.t[0];
        scratch->offset.vy = scratch->trans.vy - planeCoord->workm.t[1];
        scratch->offset.vz = scratch->trans.vz - planeCoord->workm.t[2];
        _gfxRotateSv(&scratch->local, &scratch->offset);
        gte_MulMatrix0(&scratch->local, &scratch->savedRot, &scratch->local);
        scratch->local.t[0] = scratch->offset.vx;
        scratch->local.t[1] = scratch->offset.vy;
        scratch->local.t[2] = scratch->offset.vz;
        gte_ldrgb(&color);
        while (workspace->elemCount-- > 0) {
            offsets = (const u16*)elements;
            // Consecutive references reuse projection and depth, even on GTE failure.
            if (offsets[0] != previousVertexOffset) {
                gte_SetTransMatrix(&scratch->local);
                gte_SetRotMatrix(&scratch->local);
                ACTOR_403600_TRANSFORM_AND_CLAMP_PLANE_VERTEX(&workspace->verts[offsets[0] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT], scratch->verts[0]);
                gte_SetRotMatrix(&gActor403600RipplePlaneCoord->workm);
                gte_SetTransMatrix(&gActor403600RipplePlaneCoord->workm);
                gte_ldv0(&scratch->verts[0]);
                gte_rtps();
                gte_stsz(&workspace->gteResult);
                gte_stflg(&workspace->gteFlag);
                if (workspace->gteFlag & TMD_GTE_ERROR_FLAG) {
                    workspace->gteResult |= TMD_VERTEX_DEPTH_INVALID;
                }
                workspace->szTable[offsets[0] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT] = workspace->gteResult;
            }
            gte_stsxy(workspace->preXformWrite + offsets[2]);
            gte_ldv0(&workspace->normals[offsets[1] >> ACTOR_403600_GEOMETRY_OFFSET_SHIFT]);
            gte_nccs();
            elements += workspace->elemStride;
            gte_strgb(workspace->preXformWrite + offsets[3]);
            previousVertexOffset = offsets[0];
        }
        // Restore the part transform without advancing either packet-region cursor.
        gte_SetTransVector(&scratch->trans);
        gte_SetRotMatrix(&scratch->savedRot);
        SCRATCH_STACK_RELEASE_BYTES(sizeof(_Actor403600TriScratch));
        return elements;
    }
    return tmdXformStreamVerts(workspace, objectFlags, elements);
}

#undef ACTOR_403600_TRANSFORM_AND_CLAMP_PLANE_VERTEX

/// Neutral Q7 RGB lighting seed for the model-stream draw and transform callbacks.
///
/// Each component is one (128); the zero GTE colour-command byte survives when
/// fade callbacks overwrite RGB.
static const CVECTOR _gActor403600NeutralLightColor = {
    ACTOR_403600_SCREEN_FADE_COLOR_ONE, ACTOR_403600_SCREEN_FADE_COLOR_ONE, ACTOR_403600_SCREEN_FADE_COLOR_ONE, 0
};
