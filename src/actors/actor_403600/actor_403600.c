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
static const CVECTOR D_actor_403600_80131E34;

void        func_actor_403600_801353D0(Actor403600Ripple* arg0, GfxCoord* arg1);
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

void func_actor_403600_80134288(Task*);
void func_actor_403600_80134398(Task*);
void func_actor_403600_80135C28(Task*);

TaskDesc D_actor_403600_801421A0[4] = {
    { { { TASK_BODY_NONE, 95 } }, func_actor_403600_80134288, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_403600_80134398, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_actor_403600_80138C34, { .value = 0 } },
    { { { TASK_BODY_COORD, 112 } }, func_actor_403600_80135C28, { .value = 0 } },
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
static void        func_actor_403600_8013289C(s32 x, s32 corner, SVECTOR* arg2, s32 fade);
static inline void _actor403600RotateSv(const MATRIX* rotationMatrix, const SVECTOR* input, SVECTOR* output);
static inline void _actor403600TrailTick(Actor403600Ripple* state);
static inline s32  _actor403600TrailEmpty(Actor403600Ripple* state);

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

/* Places vertex `corner` of a grid quad on screen: the vertex is moved to
 * screen space, jittered by a few pixels with probability rising with `fade`,
 * clamped to the 320x240 frame (the clamp is folded back into the vertex), and
 * given the texture coordinate of the frame copy beneath it.
 * The quad arrives as an integer because the same variable then holds the
 * vertex's screen x: only one variable keeps both in the register the build
 * requires. The third argument's value is never read. */
static void func_actor_403600_8013289C(s32 x, s32 corner, SVECTOR* arg2, s32 fade)
{
    _Actor403600GridVertex* vtx;
    s16                     vx;
    s16                     vy;
    s32                     y;
    s32                     top;
    s32                     seed;
    s32                     seed2;
    s32                     seed3;
    u8*                     page;

    switch (corner) {
        case 0:
            vtx  = &((_Actor403600GridQuad*)x)->vertex0;
            page = &((_Actor403600GridQuad*)x)->page0;
            break;
        case 1:
            vtx  = &((_Actor403600GridQuad*)x)->vertex1;
            page = &((_Actor403600GridQuad*)x)->page1;
            break;
        case 2:
            vtx  = &((_Actor403600GridQuad*)x)->vertex2;
            page = &((_Actor403600GridQuad*)x)->page2;
            break;
        default:
            vtx  = &((_Actor403600GridQuad*)x)->vertex3;
            page = &((_Actor403600GridQuad*)x)->page3;
            break;
    }
    vx                      = vtx->x;
    vy                      = vtx->y;
    x                       = vx + 0xA0;
    y                       = vy + 0x78;
    seed                    = D_actor_403600_80160698 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    D_actor_403600_80160698 = seed;
    if (((seed >> 16) & 0xFFF) < fade + 0x400) {
        seed2                   = seed * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        x                       = vx + 0x9C;
        x                      += (seed2 >> 16) & 7;
        seed3                   = seed2 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        D_actor_403600_80160698 = seed3;
        top                     = vy + 0x74;
        y                       = top + ((seed3 >> 16) & 7);
    }
    if (y >= 0xF0) {
        vtx->y += 0xEF - y;
        y       = 0xEF;
    } else if (y < 0) {
        vtx->y -= y;
        y       = 0;
    }
    if (x >= 0x140) {
        vtx->x += 0x13F - x;
        x       = 0x13F;
    } else if (x < 0) {
        vtx->x -= x;
        x       = 0;
    }
    *page = 0;
    if (x >= 0x100) {
        *page = 0x40;
    }
    vtx->u = x - *page;
    vtx->v = y;
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
                func_actor_403600_8013289C((s32)poly, 2, &scratch->field_14, fade);
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
                    func_actor_403600_8013289C((s32)poly, 0, &scratch->field_14, fade);
                } else {
                    previous                                        = poly - 1;
                    ACTOR_403600_GRID_VERTEX_XY_WORD(poly->vertex0) = ACTOR_403600_GRID_VERTEX_XY_WORD(previous->vertex1);
                    poly->vertex0.u                                 = previous->vertex1.u;
                    poly->vertex0.v                                 = previous->vertex1.v;
                    poly->page0                                     = previous->page1;
                }
                poly->vertex1.x = x + 0x10;
                poly->vertex1.y = y;
                func_actor_403600_8013289C((s32)poly, 1, &scratch->field_14, fade);
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
            func_actor_403600_8013289C((s32)poly, 3, &scratch->field_14, fade);
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

void func_actor_403600_80132E40(Task* arg0, Actor403600Work* work, Actor403600FxWork* fx)
{
    Task*                     actor;
    GfxCoord*                 center;
    u8*                       head;
    _Actor403600ChainScratch* scratch;
    s32                       i;

    actor  = arg0->parent;
    center = &actor->extra.tmd->coords[8];
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        head    = SCRATCH_STACK_CURSOR(u8);
        scratch = (_Actor403600ChainScratch*)(SCRATCH_STACK_CURSOR(u8) = head - sizeof(_Actor403600ChainScratch));
        actorRenderComposeCoord(&actor->extra.tmd->coords[11]);
        if (fx->chainsPlaced == 0) {
            TransposeMatrix(&gGfxViewCoord.workm, &scratch->basis);
            scratch->aux.vx = center->workm.t[0] - gGfxViewCoord.workm.t[0];
            scratch->aux.vy = center->workm.t[1] - gGfxViewCoord.workm.t[1];
            scratch->aux.vz = center->workm.t[2] - gGfxViewCoord.workm.t[2];

            _gfxRotateSv(&scratch->basis, &scratch->aux);

            scratch->segment.vx = 0;
            scratch->segment.vy = 0;
            scratch->segment.vz = -0x485;
            _gfxRotateSv(&center->workm, &scratch->segment);

            _gfxRotateSv(&scratch->basis, &scratch->segment);

            i = 0;
            do {
                fx->chain[i]     = scratch->aux;
                fx->chain[i].vx += scratch->segment.vx * i;
                fx->chain[i].vy += scratch->segment.vy * i;
                fx->chain[i].vz += scratch->segment.vz * i;
                i++;
            } while (i < 4);

            i = 0;
            do {
                GfxCoord* limb = &actor->extra.tmd->coords[i * 4 + 15];
                actorRenderComposeCoord(limb);
                scratch->aux.vx = limb->workm.t[0] - gGfxViewCoord.workm.t[0];
                scratch->aux.vy = limb->workm.t[1] - gGfxViewCoord.workm.t[1];
                scratch->aux.vz = limb->workm.t[2] - gGfxViewCoord.workm.t[2];
                _gfxRotateSv(&scratch->basis, &scratch->aux);

                scratch->segment.vx = 0;
                scratch->segment.vy = 0x898;
                scratch->segment.vz = 0;
                _gfxRotateSv(&center->workm, &scratch->segment);

                _gfxRotateSv(&scratch->basis, &scratch->segment);
                fx->limbTips[i].vx = scratch->aux.vx + scratch->segment.vx;
                fx->limbTips[i].vy = scratch->aux.vy + scratch->segment.vy;
                fx->limbTips[i].vz = scratch->aux.vz + scratch->segment.vz;
                i++;
            } while (i < 2);
            fx->chainsPlaced += 1;
        } else {
            TransposeMatrix(&gGfxViewCoord.workm, &scratch->basis);
            scratch->aux.vx = center->workm.t[0] - gGfxViewCoord.workm.t[0];
            scratch->aux.vy = center->workm.t[1] - gGfxViewCoord.workm.t[1];
            scratch->aux.vz = center->workm.t[2] - gGfxViewCoord.workm.t[2];

            _gfxRotateSv(&scratch->basis, &scratch->aux);
            fx->chain[0] = scratch->aux;

            scratch->aux.vx = 0;
            scratch->aux.vy = 0;
            scratch->aux.vz = -(work->chainPullExtra + 0x200);
            _gfxRotateSv(&center->workm, &scratch->aux);
            _gfxRotateSv(&scratch->basis, &scratch->aux);

            if (work->chainSweep != 0) {
                gte_lddp(work->chainSweep);
                gte_ldsv(&scratch->aux);
                gte_gpf12();
                gte_stsv(&scratch->sweepPull);
            }

            i = 0;
            do {
                scratch->segment.vx  = fx->chain[i + 1].vx - fx->chain[i].vx;
                scratch->segment.vy  = fx->chain[i + 1].vy - fx->chain[i].vy;
                scratch->segment.vz  = fx->chain[i + 1].vz - fx->chain[i].vz;
                scratch->segment.vx += scratch->aux.vx;
                scratch->segment.vy += scratch->aux.vy;
                scratch->segment.vz += scratch->aux.vz;
                scratch->aux.vx    >>= 1;
                scratch->aux.vy    >>= 1;
                scratch->aux.vz    >>= 1;
                VectorNormalSS(&scratch->segment, &scratch->segment);
                scratch->dirs[i] = scratch->segment;
                gte_lddp(0x485);
                gte_ldsv(&scratch->segment);
                gte_gpf12();
                gte_stsv(&scratch->segment);
                fx->chain[i + 1].vx = fx->chain[i].vx + scratch->segment.vx;
                fx->chain[i + 1].vy = fx->chain[i].vy + scratch->segment.vy;
                fx->chain[i + 1].vz = fx->chain[i].vz + scratch->segment.vz;
                i++;
            } while (i < 3);

            i = 0;
            do {
                GfxCoord* segment = &actor->extra.tmd->coords[i + 9];
                _actor403600RotateSv(&gGfxViewCoord.workm, &scratch->dirs[i], &scratch->segment);
                TransposeMatrix(&center->workm, &scratch->rot);
                _gfxRotateSv(&scratch->rot, &scratch->segment);
                scratch->aux.vx     = 0;
                scratch->aux.vy     = 0x1000;
                scratch->aux.vz     = 0;
                scratch->segment.vx = -scratch->segment.vx;
                scratch->segment.vy = -scratch->segment.vy;
                scratch->segment.vz = -scratch->segment.vz;
                Gfx_OrthonormalBasis(&scratch->basis, &scratch->segment, &scratch->aux);
                gte_MulMatrix0(&scratch->rot, &segment->workm, &scratch->rot);
                gte_MulMatrix0(&scratch->basis, &scratch->rot, &scratch->rot);
                gte_MulMatrix0(&center->workm, &scratch->rot, &scratch->rot);
                TransposeMatrix(&segment->parent->workm, &scratch->basis);
                gte_MulMatrix0(&scratch->basis, &scratch->rot, &segment->coord);
                segment->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(segment);
                i++;
            } while (i < 3);

            i = 0;
            do {
                GfxCoord* limb = &actor->extra.tmd->coords[i * 4 + 15];
                TransposeMatrix(&gGfxViewCoord.workm, &scratch->basis);
                actorRenderComposeCoord(limb);
                scratch->aux.vx = limb->workm.t[0] - gGfxViewCoord.workm.t[0];
                scratch->aux.vy = limb->workm.t[1] - gGfxViewCoord.workm.t[1];
                scratch->aux.vz = limb->workm.t[2] - gGfxViewCoord.workm.t[2];
                _gfxRotateSv(&scratch->basis, &scratch->aux);

                scratch->segment.vx = 0;
                scratch->segment.vy = work->limbPullExtra + 0x200;
                scratch->segment.vz = 0;
                _gfxRotateSv(&limb->workm, &scratch->segment);
                _gfxRotateSv(&scratch->basis, &scratch->segment);

                scratch->segment.vx += fx->limbTips[i].vx - scratch->aux.vx;
                scratch->segment.vy += fx->limbTips[i].vy - scratch->aux.vy;
                scratch->segment.vz += fx->limbTips[i].vz - scratch->aux.vz;
                if (work->chainSweep != 0) {
                    scratch->segment.vx += scratch->sweepPull.vx;
                    scratch->segment.vy += scratch->sweepPull.vy;
                    scratch->segment.vz += scratch->sweepPull.vz;
                }
                VectorNormalSS(&scratch->segment, &scratch->segment);
                scratch->dirs[i] = scratch->segment;
                gte_lddp(0x898);
                gte_ldsv(&scratch->segment);
                gte_gpf12();
                gte_stsv(&scratch->segment);
                fx->limbTips[i].vx = scratch->aux.vx + scratch->segment.vx;
                fx->limbTips[i].vy = scratch->aux.vy + scratch->segment.vy;
                fx->limbTips[i].vz = scratch->aux.vz + scratch->segment.vz;

                _actor403600RotateSv(&gGfxViewCoord.workm, &scratch->dirs[i], &scratch->segment);
                TransposeMatrix(&limb->workm, &scratch->rot);
                _gfxRotateSv(&scratch->rot, &scratch->segment);
                scratch->aux.vx = 0;
                scratch->aux.vy = 0;
                scratch->aux.vz = 0x1000;
                Gfx_OrthonormalBasis(&scratch->basis, &scratch->segment, &scratch->aux);
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
                i++;
            } while (i < 2);
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
        work                    = arg0->parent->work;
        work->fxTask            = arg0;
        D_actor_403600_801606A0 = 0;
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
    s32                    var_v0;
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
                Gp_CopyCoordOffset(arg0, &owner->extra.tmd->coords[14], &sp10);
            } else {
                Gp_CopyCoordOffset(arg0, &owner->extra.tmd->coords[18], &sp10);
            }
            if (arg0->spawnArg1.value < 0x1000) {
                Gp_SpawnEff(EFFECT_EVE_ENERGY_RING, coord, 0x20, 0);
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
            obj->key                          = Gp_PackPair(&D_actor_403600_801420F0, arg0->spawnArg1.value & 0xF);
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
            arg0->exitCallback = func_actor_403600_80138C68;
        }
        newWork->life = 0x12C;
        arg0->state   = arg0->state + 1;
        goto block_22;
    }
block_22:
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
            } else {
                if (temp_v1_4 == 3) {
                    temp_s0_3 = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(SOUND_SHELTER_B2_POD_BTM_PROJECTILE_LAUNCH, temp_s0_3, (s8)worldCoordGetOriginAudioDepth(coord));
                    var_v0 = (rand() & 0xF) + 0x10;
                    goto block_34;
                }
                var_v0 = 0x7FFF;
                if (temp_v1_4 == 0) {
                block_34:
                    arg0->killCountdown = var_v0;
                    arg0->status        = 1;
                } else {
                    arg0->killCountdown = (u16)((rand() & 0xF) + 0x10);
                    arg0->status        = (u8)(arg0->status ^ 1);
                }
                goto block_36;
            }
        } else {
        block_36:
            if (arg0->status != 2) {
                steeringPass = 0;
                temp_s0_4    = &scratch->dir;
                do {
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
                        goto block_51;
                    }
                    if (temp_v1_5 == 3) {
                        motionParent = arg0->spawnArg2.pointer;
                        if ((s16)arg0->killCountdown >= 7) {
                            var_a1_2 = &motionParent->extra.tmd->coords[18];
                            Gp_CopyCoordOffset(arg0, var_a1_2, &sp10);
                            var_s4 = 0;
                            do {
                                work->trail[var_s4].vx = (u16)coord->coord.t[0];
                                work->trail[var_s4].vy = (u16)coord->coord.t[1];
                                work->trail[var_s4].vz = (u16)coord->coord.t[2];
                                var_s4                += 1;
                            } while (var_s4 < ARRAY_SIZE(work->trail));
                            steeringPass += 1;
                        } else {
                            goto block_51;
                        }
                    } else {
                        temp_a0_3 = arg0->spawnArg2.pointer;
                        if (arg0->spawnArg1.value == 0x1000) {
                            Gp_CopyCoordOffset(arg0, &temp_a0_3->extra.tmd->coords[18], &sp10);
                        } else {
                            Gp_CopyCoordOffset(arg0, &temp_a0_3->extra.tmd->coords[14], &sp10);
                        }
                    block_51:
                        steeringPass += 1;
                    }
                } while (steeringPass < 2);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            work->trail[0].vx  = (u16)coord->coord.t[0];
            work->trail[0].vy  = (u16)coord->coord.t[1];
            work->trail[0].vz  = (u16)coord->coord.t[2];
            work->trail[0].pad = rand();
            goto block_54;
        }
    } else {
    block_54:
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
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403600ProjectileScratch);
}

void func_actor_403600_801353D0(Actor403600Ripple* arg0, GfxCoord* arg1)
{
    s32                            radii[16];
    s32                            heights[16];
    s32                            corner[4];
    s32                            i;
    s32                            j;
    s32                            firstAngle;
    s32                            angle;
    s32                            index;
    s32                            value;
    s32                            firstRadius;
    s32                            rotation;
    s32                            mirrorXY;
    s32                            projectedX;
    s32                            projectedY;
    s32                            screenX;
    s32                            screenY;
    s32                            min;
    s32                            max;
    s32                            adjust;
    s32                            radiusOffset;
    s32                            scale;
    s32                            scanCount;
    s32*                           height;
    MATRIX*                        matrix;
    s32*                           heightBase;
    s32*                           nextHeight;
    u16                            oldY;
    u8*                            head;
    u8*                            newHead;
    _Actor403600GridQuad*          after;
    _Actor403600GridQuad*          previous;
    _Actor403600GridQuad*          mirror;
    _Actor403600GridQuad*          poly;
    SVECTOR*                       vec;
    _Actor403600RadialGridScratch* scratch;

    head                       = SCRATCH_STACK_CURSOR(u8);
    newHead                    = head - sizeof(_Actor403600RadialGridScratch);
    SCRATCH_STACK_CURSOR(void) = newHead;
    scratch                    = (_Actor403600RadialGridScratch*)newHead;
    actorRenderComposeCoord(arg1);
    gte_SetRotMatrix(&arg1->workm);
    gte_SetTransMatrix(&arg1->workm);

    scratch->probePoints[1].vz = 0x1000;
    scratch->probePoints[2].vx = 0x1000;
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

    i = 0;
    do {
        firstAngle  = arg0->head + i * 2;
        index       = firstAngle % 32;
        firstRadius = (rsin(arg0->phase[index]) * arg0->strength[index]) >> 12;
        value       = (firstRadius * (16 - i)) / 16;
        firstRadius = value >> 3;
        if (arg0->shallow == 1) {
            firstRadius = value >> 8;
        }
        radii[i]   = firstRadius;
        value      = (arg0->strength[index] * (15 - i)) >> 10;
        heights[i] = value;
        if (scratch->nclip > 0) {
            heights[i] = -value;
        }
        i++;
        j = 0;
    } while (i < 16);

    scratch->maxOtz = 0;
    matrix          = &scratch->sectorMatrix;
    vec             = &scratch->texelOffset;
    heightBase      = heights;
    do {
        scratch->sectorMatrix = arg1->workm;
        gfxRotMatrixY(matrix, (j << 12) / 12, 0);
        gte_SetTransMatrix(&arg1->workm);
        gte_SetRotMatrix(matrix);
        scale        = 0x14;
        i            = 0;
        height       = heightBase;
        radiusOffset = 0;
        angle        = arg0->head;
        do {
            /* The screen the quad corners are clamped to. */
            s32 screenW = 320;
            s32 screenH = 240;

            angle                   %= 32;
            poly                     = (_Actor403600GridQuad*)D_actor_403600_8016069C;
            D_actor_403600_8016069C += sizeof(_Actor403600GridQuad);
            rotation                 = -rcos(arg0->phase[angle]) >> 3;
            scratch->texelOffset.vx  = rsin(rotation);
            scratch->texelOffset.vy  = rcos(rotation);
            scratch->texelOffset.vz  = 0;
            gte_ldv0(vec);
            gte_rtv0();
            scratch->localVertex.vx = scale;
            scratch->localVertex.vz = 0;
            /* A byte offset stepped beside `i`: indexing `radii` by `i` frees
             * that register and moves the allocation of the whole loop. */
            scratch->localVertex.vy = *(s32*)((u8*)radii + radiusOffset);
            gte_stsv(vec);
            gte_ldv0(&scratch->localVertex);
            gte_rtps();
            gte_stsxy(&scratch->sxy);
            gte_stdp(&scratch->dp);
            gte_stflg(&scratch->flag);
            gte_stszotz(&scratch->otz);
            gte_lddp(*height);
            gte_ldsv(vec);
            gte_gpf12();
            gte_stsv(vec);

            ACTOR_403600_GRID_VERTEX_XY_WORD(poly->vertex0) = scratch->sxy;
            oldY                                            = poly->vertex0.y;
            projectedX                                      = scratch->texelOffset.vx + 0xA0;
            screenX                                         = (s16)poly->vertex0.x + projectedX;
            projectedY                                      = scratch->texelOffset.vy + 0x78;
            screenY                                         = (s16)poly->vertex0.y + projectedY;
            if (screenY >= screenH) {
                poly->vertex0.y = oldY + (screenH - 1) - screenY;
                screenY         = screenH - 1;
            } else if (screenY < 0) {
                poly->vertex0.y = oldY - screenY;
                screenY         = 0;
            }
            if (screenX >= screenW) {
                poly->vertex0.x = (u16)poly->vertex0.x + (screenW - 1) - screenX;
            } else if (screenX < 0) {
                poly->vertex0.x = (u16)poly->vertex0.x - screenX;
                screenX         = 0;
            }
            poly->page0 = 0;
            if (screenX >= 0x100) {
                poly->page0 = 0x40;
            }
            poly->vertex0.v = screenY;
            poly->vertex0.u = screenX - poly->page0;
            /* The next height is reached through its own index and pointer:
             * `heightBase[i + 1]` folds the +1 into the load's displacement,
             * and an inline `heightBase[index]` adds the base second. */
            if (scratch->flag >= 0 && i != 15 && (*height != 0 || (index = i + 1, nextHeight = &heightBase[index], *nextHeight != 0))) {
                setlen(poly, 9);
                poly->code   = 0x2D;
                scratch->otz = (scratch->otz << gDisplayState.otDepthShift & 0x3FFF) >> 4;
                if (scratch->maxOtz < scratch->otz) {
                    scratch->maxOtz = scratch->otz;
                }
                addPrim(&gGpuCurrentOt[scratch->otz], poly);
            }
            previous = poly - 1;
            if (i != 0) {
                ACTOR_403600_GRID_VERTEX_XY_WORD(previous->vertex1) = ACTOR_403600_GRID_VERTEX_XY_WORD(poly->vertex0);
                previous->vertex1.u                                 = poly->vertex0.u;
                do {
                    previous->vertex1.v = poly->vertex0.v;
                    previous->page1     = poly->page0;
                    if (j != 0) {
                        mirrorXY = ACTOR_403600_GRID_VERTEX_XY_WORD(previous->vertex0);
                        mirror   = poly - 17;
                    } else {
                        mirrorXY = ACTOR_403600_GRID_VERTEX_XY_WORD(previous->vertex0);
                        mirror   = poly + 175;
                    }
                    ACTOR_403600_GRID_VERTEX_XY_WORD(mirror->vertex2) = mirrorXY;
                    mirror->vertex2.u                                 = previous->vertex0.u;
                } while (0);
                mirror->vertex2.v                                 = previous->vertex0.v;
                mirror->page2                                     = previous->page0;
                ACTOR_403600_GRID_VERTEX_XY_WORD(mirror->vertex3) = ACTOR_403600_GRID_VERTEX_XY_WORD(previous->vertex1);
                mirror->vertex3.u                                 = previous->vertex1.u;
                mirror->vertex3.v                                 = previous->vertex1.v;
                mirror->page3                                     = previous->page1;
            }
            height++;
            radiusOffset += 4;
            i++;
            angle += 2;
            scale += 0x9B;
        } while (i < 16);
        j++;
    } while (j < 12);

    j = 0;
    do {
        i = 0;
        /* Walked one quad ahead of the quad it adjusts, so every field is
         * reached at a negative displacement, as the build requires. */
        after = poly + 1;
        do {
            corner[0] = after[-1].vertex0.u + after[-1].page0;
            corner[1] = after[-1].vertex1.u + after[-1].page1;
            corner[2] = after[-1].vertex2.u + after[-1].page2;
            corner[3] = after[-1].vertex3.u + after[-1].page3;
            min       = corner[0];
            max       = corner[0];
            for (scanCount = 1; scanCount < 4; scanCount++) {
                if (corner[scanCount] < min) {
                    min = corner[scanCount];
                } else if (max < corner[scanCount]) {
                    max = corner[scanCount];
                }
            }
            if (max >= 0x100 || min >= 0x40) {
                adjust = 0x40;
            } else {
                adjust = 0;
            }
            after[-1].tpage     = ((u32)(adjust + 0x1C0) >> 6) | 0x110;
            after[-1].vertex0.u = corner[0] - adjust;
            after[-1].vertex1.u = corner[1] - adjust;
            poly--;
            after[-1].vertex2.u = corner[2] - adjust;
            i++;
            after[-1].vertex3.u = corner[3] - adjust;
            after--;
        } while (i < 16);
        j++;
    } while (j < 12);
    frameCaptureQueue(scratch->maxOtz + 1);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(_Actor403600RadialGridScratch));
}

static const SVECTOR D_actor_403600_80131E2C = { 0, 0x578, 0, 0 };

/// Advances the ripple by one step: moves `head` back one slot in the two
/// sample rings, clears it, ramps the source's strength up while `emitting` is
/// set (restarting its phase on a rising edge) or down otherwise, and records
/// the source's phase and strength in the new head while the strength is
/// non-zero.
static inline void _actor403600TrailTick(Actor403600Ripple* state)
{
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
        if (state->sourceStrength < 0x1000) {
            state->sourceStrength += 0x200;
        }
    } else if (state->sourceStrength > 0) {
        state->sourceStrength -= 0x80;
    }
    state->wasEmitting = state->emitting;
    if (state->sourceStrength != 0) {
        state->phase[head]    = state->sourcePhase;
        state->strength[head] = state->sourceStrength;
        if (state->shallow == 0) {
            state->sourcePhase += 0x180;
        } else {
            state->sourcePhase += 0x100;
        }
    }
}

/// Whether every sample of the ripple has phase zero.
static inline s32 _actor403600TrailEmpty(Actor403600Ripple* state)
{
    s32 i;

    for (i = 0; i < ACTOR_403600_RIPPLE_SAMPLE_COUNT; i++) {
        if (state->phase[i] != 0) {
            return 0;
        }
    }
    return 1;
}

void func_actor_403600_80135C28(Task* arg0)
{
    SVECTOR            sp10;
    MATRIX*            mtx;
    Actor403600Ripple* temp_s0;
    Actor403600Ripple* temp_v0_2;
    GfxCoord*          temp_s4;
    s32                temp_v1_2;
    s32                temp_v0_9;
    s32                temp_v1_10;
    s32                var_a1;
    Task*              temp_a0;
    TmdObject*         temp_a0_5;
    TmdObject*         temp_a1;
    Task*              temp_s2;
    TmdObject*         temp_v0;
    Actor403600Work*   ownerWork;

    temp_a0   = arg0->spawnArg2.pointer;
    ownerWork = temp_a0->work;
    temp_s4   = arg0->extra.coordBody->coord;
    temp_s2   = ownerWork->fxTask;
    if (ownerWork->defeated == 1) {
        temp_v0                 = temp_a0->extra.tmd;
        D_actor_403600_801606A0 = NULL;
        temp_v0->flags          = (u16)(temp_v0->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW);
        taskCallExit(arg0);
        return;
    }
    if (arg0->state == 0) {
        temp_v0_2 = memCalloc(sizeof(Actor403600Ripple), false);
        if (temp_v0_2 != NULL) {
            arg0->work         = temp_v0_2;
            temp_v0_2->shallow = 0;
            sp10               = D_actor_403600_80131E2C;
            Gp_CopyCoordOffset(arg0, &temp_s2->parent->extra.tmd->coords[1], &sp10);
            mtx                               = &temp_v0_2->clipCoord.coord;
            MATRIX_PAIR(mtx, 0, 0)            = 0x1000;
            MATRIX_PAIR(mtx, 0, 2)            = 0;
            MATRIX_PAIR(mtx, 1, 1)            = 0x1000;
            MATRIX_PAIR(mtx, 2, 0)            = 0;
            mtx->m[2][2]                      = 0x1000;
            temp_v0_2->clipCoord.coord.t[0]   = 0;
            temp_v0_2->clipCoord.coord.t[1]   = 0;
            temp_v0_2->clipCoord.coord.t[2]   = 0;
            temp_v0_2->clipCoord.composeStamp = GRAPHICS_COORD_DIRTY;
            temp_v0_2->clipCoord.parent       = temp_s4;
            temp_v1_2                         = arg0->spawnArg1.value;
            arg0->killCountdown               = 0x10;
            switch (temp_v1_2) {
                case 1:
                    temp_v0_2->emitting = temp_v1_2;
                    var_a1              = 0;
                    do {
                        _actor403600TrailTick(temp_v0_2);
                        var_a1 += 1;
                    } while (var_a1 < 0x10);
                    temp_v0_2->clipCountdown = 8;
                    break;
                case 2:
                    arg0->killCountdown      = 0x2E;
                    temp_v0_2->clipCountdown = 0x1F;
                    gfxRotMatrixZ(mtx, 0x800, GRAPHICS_ROTATION_COMPOSE);
                    temp_s4->composeStamp = GRAPHICS_COORD_DIRTY;
                    break;
                default:
                    temp_v0_2->emitting = 1;
                    var_a1              = 0;
                    do {
                        _actor403600TrailTick(temp_v0_2);
                        var_a1 += 1;
                    } while (var_a1 < 0x10);
                    break;
            }
            arg0->state = (s32)(arg0->state + 1);
        } else {
            taskCallExit(arg0);
            return;
        }
    }
    temp_s0 = arg0->work;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        switch (arg0->spawnArg1.value) {
            case 1:
                temp_v0_9              = temp_s0->clipCountdown - 1;
                temp_s0->clipCountdown = temp_v0_9;
                if (temp_v0_9 == 0) {
                    temp_a0_5               = ((Task*)arg0->spawnArg2.pointer)->extra.tmd;
                    D_actor_403600_801606A0 = NULL;
                    temp_a0_5->flags        = (u16)(temp_a0_5->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
                } else if (temp_v0_9 > 0) {
                    D_actor_403600_801606A0 = &temp_s0->clipCoord;
                    actorRenderComposeCoord(&temp_s0->clipCoord);
                }
                break;
            case 2:
                temp_v1_10             = temp_s0->clipCountdown - 1;
                temp_s0->clipCountdown = temp_v1_10;
                if (temp_v1_10 == 0) {
                    temp_a1                 = ((Task*)arg0->spawnArg2.pointer)->extra.tmd;
                    D_actor_403600_801606A0 = &temp_s0->clipCoord;
                    temp_a1->flags          = (u16)(temp_a1->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW);
                    actorRenderComposeCoord(&temp_s0->clipCoord);
                } else if (temp_v1_10 >= -7) {
                    D_actor_403600_801606A0 = &temp_s0->clipCoord;
                    actorRenderComposeCoord(&temp_s0->clipCoord);
                } else if (temp_v1_10 == -8) {
                    D_actor_403600_801606A0 = NULL;
                }
                break;
        }
        if (arg0->killCountdown > 0) {
            temp_s0->emitting = 1;
        } else {
            temp_s0->emitting = 0;
        }
        arg0->killCountdown = (s16)((u16)arg0->killCountdown - 1);
        _actor403600TrailTick(temp_s0);
    }
    func_actor_403600_801353D0(temp_s0, temp_s4);
    if (arg0->killCountdown <= 0 && _actor403600TrailEmpty(temp_s0)) {
        taskCallExit(arg0);
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
    lightColor   = D_actor_403600_80131E34;
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
    lightColor   = D_actor_403600_80131E34;
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
                    if (workspace->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(gteResult);
                    if (workspace->gteResult < 0) {
                    draw:
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
            if (workspace->gteResult > 0) {
                goto draw;
            }
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(packet, 3));
            gte_nclip();
            gte_stopz(gteResult);
            if (workspace->gteResult < 0) {
            draw:
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
    lightColor   = D_actor_403600_80131E34;
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
    lightColor   = D_actor_403600_80131E34;
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
    lightColor   = D_actor_403600_80131E34;
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
                    if (workspace->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(gteResult);
                    if (workspace->gteResult < 0) {
                    draw:
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

    if (D_actor_403600_801606A0 != NULL) {
        packet           = (POLY_GT3*)workspace->primWrite;
        color            = D_actor_403600_80131E34;
        savedScratchHead = SCRATCH_STACK_CURSOR(u8);
        scratch          = SCRATCH_STACK_CURSOR(_Actor403600TriScratch) =
            (_Actor403600TriScratch*)(savedScratchHead - sizeof(_Actor403600TriScratch));
        // Save the part transform and compose it into the ripple plane frame.
        gte_sttr(&scratch->trans);
        gte_ReadRotMatrix(&scratch->savedRot);
        TransposeMatrix(&D_actor_403600_801606A0->workm, &scratch->local);
        planeCoord         = D_actor_403600_801606A0;
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
            gte_SetRotMatrix(&D_actor_403600_801606A0->workm);
            gte_SetTransMatrix(&D_actor_403600_801606A0->workm);
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

    if (D_actor_403600_801606A0 != NULL) {
        packet           = (POLY_GT4*)workspace->primWrite;
        color            = D_actor_403600_80131E34;
        savedScratchHead = SCRATCH_STACK_CURSOR(u8);
        scratch          = SCRATCH_STACK_CURSOR(_Actor403600QuadScratch) =
            (_Actor403600QuadScratch*)(savedScratchHead - sizeof(_Actor403600QuadScratch));
        // Save the part transform and compose it into the ripple plane frame.
        gte_sttr(&scratch->trans);
        gte_ReadRotMatrix(&scratch->savedRot);
        TransposeMatrix(&D_actor_403600_801606A0->workm, &scratch->local);
        planeCoord         = D_actor_403600_801606A0;
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
            gte_SetRotMatrix(&D_actor_403600_801606A0->workm);
            gte_SetTransMatrix(&D_actor_403600_801606A0->workm);
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

    if (D_actor_403600_801606A0 != NULL) {
        enum { ACTOR_403600_NO_PREVIOUS_VERTEX = -1 };

        previousVertexOffset = ACTOR_403600_NO_PREVIOUS_VERTEX;
        color                = D_actor_403600_80131E34;
        if (workspace->elemCount == 0) {
            return elements;
        }
        savedScratchHead = SCRATCH_STACK_CURSOR(u8);
        scratch          = SCRATCH_STACK_CURSOR(_Actor403600TriScratch) =
            (_Actor403600TriScratch*)(savedScratchHead - sizeof(_Actor403600TriScratch));
        // Save the part transform and compose it into the ripple plane frame.
        gte_sttr(&scratch->trans);
        gte_ReadRotMatrix(&scratch->savedRot);
        TransposeMatrix(&D_actor_403600_801606A0->workm, &scratch->local);
        planeCoord         = D_actor_403600_801606A0;
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
                gte_SetRotMatrix(&D_actor_403600_801606A0->workm);
                gte_SetTransMatrix(&D_actor_403600_801606A0->workm);
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

static const CVECTOR D_actor_403600_80131E34 = { 0x80, 0x80, 0x80, 0 };
