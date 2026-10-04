#include "actor_503500_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"

extern AnimationSet* D_actor_503500_8016EA54[20];

extern AnimationSet* D_actor_503500_8016EAA4[5];

/// The 0x44 block `func_actor_503500_801455A4` allocates: an attack sphere and
/// its one-entry contact table, plus the effect task it reparents itself under.
typedef struct Actor503500Work44 {
    WorldCollisionBody    body;        // Attack sphere placed on the player's coordinate; collides only during the strike phase
    WorldCollisionContact contacts[1]; // Contact table of `body`, emptied every frame
    /* 0x38 */ Task*      field_38;
    /* 0x3C */ s16        field_3C;    // frame counter within `field_40`'s phase
    /* 0x3E */ byte       pad_3E[0x2];
    /* 0x40 */ s8         field_40;    // phase, advanced by `func_actor_503500_80145754`
    /* 0x41 */ byte       pad_41[0x3];
} Actor503500Work44;
STATIC_ASSERT_SIZEOF(Actor503500Work44, 0x44);

/// The 0xD0 block `func_actor_503500_80144E8C` allocates: an attack capsule
/// with its shape and four-entry contact table, then this task's own payload:
/// the effect task it reparents itself under, a rotation it seeds to identity
/// next to the one in its `GfxCoord`, and the pair of words plus the halfword
/// that `func_actor_503500_801450A0` reads and writes every frame.
typedef struct Actor503500WorkD0 {
    WorldCollisionBody    body;        // Attack capsule on the task's own coordinate; pair-tested from the strike until it touches the player or its radii have shrunk to a quarter
    WorldCollisionCapsule capsule;     // Shape of `body`: from the coordinate's origin to a far end turned about Y each frame of the sweep, both radii shrinking with it
    WorldCollisionContact contacts[4]; // Contact table of `capsule`, emptied every frame
    /* 0x98 */ Task*      field_98;
    /* 0x9C */ MATRIX     field_9C;
    /* 0xBC */ Fixed16    field_BC; // angle; the high half turns field_9C
    /* 0xC0 */ s32        field_C0; // per-frame angle step
    /* 0xC4 */ s16        field_C4;
    /* 0xC6 */ s16        field_C6; // sub-state frame counter
    /* 0xC8 */ byte       pad_C8[0x4];
    /* 0xCC */ s8         field_CC; // sub-state index
    /* 0xCD */ byte       pad_CD[0x3];
} Actor503500WorkD0;
STATIC_ASSERT_SIZEOF(Actor503500WorkD0, 0xD0);

/// The 0xAC block `func_actor_503500_80145A2C` allocates: an attack capsule
/// with its shape and four-entry contact table, then the effect task it
/// reparents itself under and its phase counters.
typedef struct Actor503500WorkAC {
    WorldCollisionBody    body;        // Attack capsule on the task's own coordinate; pair-tested during the strike phase until it touches the player
    WorldCollisionCapsule capsule;     // Shape of `body`: fixed, from the coordinate's origin (radius 2000) to (0, 500, 6000) (radius 3000)
    WorldCollisionContact contacts[4]; // Contact table of `capsule`, emptied every frame
    /* 0x98 */ Task*      field_98;
    /* 0x9C */ byte       pad_9C[0x8];
    /* 0xA4 */ s16        field_A4; // sub-state frame counter
    /* 0xA6 */ byte       pad_A6[0x2];
    /* 0xA8 */ s8         field_A8; // sub-state index
    /* 0xA9 */ byte       pad_A9[0x3];
} Actor503500WorkAC;
STATIC_ASSERT_SIZEOF(Actor503500WorkAC, 0xAC);

/// The 0x4CC effect work block, allocated by `func_actor_503500_8014642C`
/// (`memCalloc(0x4CC)`) and parked in that task's `Task::work` slot. Unlike the
/// package's other allocated work blocks, whose tasks unlink a collision body
/// on exit, this one exits through `func_actor_503500_801464E8`, which
/// only calls `enemyTaskExit`, so the block does not open with a `WorldCollisionBody`.
/// `func_actor_503500_80146508` republishes the two matrices onto
/// `TmdObject::lightMtx` / `colorMtx`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`,
/// exactly as `func_actor_503500_801324EC` does for `_Actor503500SliderWork`.
///
/// The size is the allocation, and the fields below are the ones the init
/// seeds: the three `sb` bytes at 0x43D/0x43E/0x4C8 are set to -1, and the
/// three words at 0x4A0..0x4A8 are cleared. This is the same layout as
/// `Actor317000Work` and its siblings in the other actor overlays, except that
/// those write 0x4C8 as a halfword.
typedef struct Actor503500Effect4CC {
    ActorAnimRig19   rig;
    ActorModelState  model;
    /* 0x480 */ s32  field_480[4]; // saved `coord.m` words 0..3
    /* 0x490 */ s16  field_490;    // saved `coord.m[2][2]`
    /* 0x492 */ byte pad_492[0xE];
    /* 0x4A0 */ s32  field_4A0;
    /* 0x4A4 */ s32  field_4A4;
    /* 0x4A8 */ s32  field_4A8;
    /* 0x4AC */ byte pad_4AC[0x4];
    /* 0x4B0 */ s32  field_4B0;
    /* 0x4B4 */ s32  field_4B4;
    /* 0x4B8 */ s32  field_4B8;
    /* 0x4BC */ byte pad_4BC[0x4];
    /* 0x4C0 */ s16  field_4C0;
    /* 0x4C2 */ s16  field_4C2;
    /* 0x4C4 */ s16  field_4C4;
    /* 0x4C6 */ s16  field_4C6;
    /* 0x4C8 */ s8   field_4C8;
    /* 0x4C9 */ byte pad_4C9[0x3];
} Actor503500Effect4CC;
STATIC_ASSERT_SIZEOF(Actor503500Effect4CC, 0x4CC);

static void func_actor_503500_801464E8(Task* arg0);
static void func_actor_503500_80146508(Task* arg0);

/// `taskMessageDispatch` handler table installed at `Task::msgTable` by
/// `func_actor_503500_8014642C`; terminator id `TASK_MESSAGE_TABLE_END`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_503500_80176530[];

/// Local offset of the display node `func_actor_503500_80144E8C` links, and the
/// offsets it seeds its `WorldCollisionCapsule` with.
extern SVECTOR D_actor_503500_801715C4;
extern SVECTOR D_actor_503500_801715CC;
/// Local offset of the display node `func_actor_503500_801455A4` links.
extern SVECTOR D_actor_503500_801715D4;
static void    func_actor_503500_80145480(Task* arg0);
static void    func_actor_503500_801450A0(Task* arg0);
static void    func_actor_503500_801454E0(Task* arg0);
static void    func_actor_503500_80145754(Task* arg0);
static void    func_actor_503500_80145950(Task* arg0);
static void    func_actor_503500_801459B0(Task* arg0);
static void    func_actor_503500_80145C50(Task* arg0);
static void    func_actor_503500_80145F18(Task* arg0);

extern SVECTOR D_actor_503500_801715DC;
extern SVECTOR D_actor_503500_801715E4;
static void    func_actor_503500_80145E98(Task* arg0);
static void    func_actor_503500_8014618C(Task* arg0);
static void    func_actor_503500_80146524(Task* arg0);

extern AnimationSet*  D_actor_503500_80176514[3];
extern AnimationSet** gActorMotionAnimBanks19[1];
static void           func_actor_503500_80144E8C(Task* arg0);
static void           func_actor_503500_80145428(Task* arg0);
static void           func_actor_503500_801455A4(Task* arg0);
static void           func_actor_503500_801458F8(Task* arg0);
static void           func_actor_503500_80145A2C(Task* arg0);
static void           func_actor_503500_80145E1C(Task* arg0);
static void           func_actor_503500_8014642C(Task* arg0);
static void           func_actor_503500_80145FDC(Task* task);
static void           func_actor_503500_801464E8(Task* arg0);

/// `Task::state` handlers `func_actor_503500_8014554C` dispatches through.
static const TaskFuncTable3 D_actor_503500_801321F4 = {
    {
        func_actor_503500_80144E8C,
        func_actor_503500_80145428,
        func_actor_503500_80145480,
    },
};

static AnimationSet _gActor503500Animation444F4;
static AnimationSet _gActor503500Animation446CC;
static TmdSource    _gActor503500Actor361100Model06038;
s32                 func_actor_503500_80146664(Task* task, s32 msgId, ActorTransform* args, s32 arg3);
s32                 func_actor_503500_801466E0(Task*, s32, s32, s32);
s32                 func_actor_503500_801467C0(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
void                func_actor_503500_801463C0(Task*);

static AnimationSet _gActor503500Animation3DE60;
static AnimationSet _gActor503500Animation3E5FC;
static AnimationSet _gActor503500Animation3EE08;
static AnimationSet _gActor503500Animation3F61C;

TaskMessageEntry D_actor_503500_8016EA2C[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_503500_80135950 },
    { ACTOR_MESSAGE_PLACE, func_actor_503500_80137088 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_503500_80137158 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_503500_80135B74 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_actor_503500_8016EA54[20] = {
    NULL,
    &gActor503500Animation2DB14,
    &gActor503500Animation2E4DC,
    &gActor503500Animation2EF88,
    &gActor503500Animation2FC70,
    &gActor503500Animation306E0,
    &gActor503500Animation30F1C,
    &gActor503500Animation31788,
    &gActor503500Animation31D8C,
    &gActor503500Animation32E24,
    &gActor503500Animation333DC,
    &gActor503500Animation33C14,
    &gActor503500Animation33EBC,
    &gActor503500Animation341D8,
    &gActor503500Animation350C8,
    &gActor503500Animation35390,
    &gActor503500Animation356DC,
    &gActor503500Animation2DB14,
    &gActor503500Animation2DB14,
    &gActor503500Animation38AE0,
};

AnimationSet* D_actor_503500_8016EAA4[5] = {
    NULL,
    &gActor503500Animation38AE0,
    &gActor503500Animation3A190,
    &gActor503500Animation350C8,
    &gActor503500Animation3C968,
};

AnimationSet** D_actor_503500_8016EAB8[2] = {
    D_actor_503500_8016EA54,
    D_actor_503500_8016EAA4,
};

AnimationPlayRequest D_actor_503500_8016EAC0[1] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_503500_8016EAD4 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_503500_8016EAE8[18] = {
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 11, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 12, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 14, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 15, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 17, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 18, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

SVECTOR D_actor_503500_8016EC50 = { 0, -500, 1600, 0 };

Actor503500Step D_actor_503500_8016EC58[4] = {
    { func_actor_503500_8013667C, 127 },
    { func_actor_503500_80133BF4, 79 },
    { func_actor_503500_8013680C, 47 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016EC78[4] = {
    { func_actor_503500_80133BF4, 127 },
    { func_actor_503500_80136770, 79 },
    { func_actor_503500_8013680C, 47 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016EC98[5] = {
    { func_actor_503500_80136770, 79 },
    { func_actor_503500_80134284, 63 },
    { func_actor_503500_8013656C, 47 },
    { func_actor_503500_8013680C, 31 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016ECC0[3] = {
    { func_actor_503500_8013667C, 159 },
    { func_actor_503500_8013680C, 95 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016ECD8[3] = {
    { func_actor_503500_8013680C, 159 },
    { func_actor_503500_80136770, 95 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016ECF0[4] = {
    { func_actor_503500_8013680C, 111 },
    { func_actor_503500_80134284, 79 },
    { func_actor_503500_80136770, 63 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016ED10[4] = {
    { func_actor_503500_80134284, 159 },
    { func_actor_503500_8013656C, 63 },
    { func_actor_503500_80136770, 31 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016ED30[4] = {
    { func_actor_503500_80136948, 95 },
    { func_actor_503500_8013667C, 95 },
    { func_actor_503500_8013680C, 63 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016ED50[3] = {
    { func_actor_503500_8013680C, 159 },
    { func_actor_503500_80134284, 95 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016ED68[3] = {
    { func_actor_503500_80134284, 223 },
    { func_actor_503500_8013680C, 31 },
    { NULL, 0 },
};

Actor503500Step* D_actor_503500_8016ED80[4] = {
    D_actor_503500_8016EC58,
    D_actor_503500_8016EC78,
    D_actor_503500_8016EC78,
    D_actor_503500_8016EC98,
};

s16 D_actor_503500_8016ED90[4] = {
    700,
    1000,
    1300,
    2048,
};

Actor503500Step* D_actor_503500_8016ED98[4] = {
    D_actor_503500_8016ECC0,
    D_actor_503500_8016ECD8,
    D_actor_503500_8016ECF0,
    D_actor_503500_8016ED10,
};

s16 D_actor_503500_8016EDA8[4] = {
    700,
    1000,
    1300,
    2048,
};

Actor503500Step* D_actor_503500_8016EDB0[4] = {
    D_actor_503500_8016ED30,
    D_actor_503500_8016ED50,
    D_actor_503500_8016ED50,
    D_actor_503500_8016ED68,
};

s16 D_actor_503500_8016EDC0[4] = {
    300,
    1000,
    1300,
    2048,
};

Actor503500Step D_actor_503500_8016EDC8[4] = {
    { func_actor_503500_801364D0, 111 },
    { func_actor_503500_8013667C, 111 },
    { func_actor_503500_80133BF4, 31 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016EDE8[3] = {
    { func_actor_503500_8013667C, 159 },
    { func_actor_503500_80133BF4, 95 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016EE00[2] = {
    { func_actor_503500_8013656C, 255 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016EE10[3] = {
    { func_actor_503500_801364D0, 127 },
    { func_actor_503500_8013667C, 127 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016EE28[3] = {
    { func_actor_503500_8013680C, 159 },
    { func_actor_503500_80136770, 95 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016EE40[4] = {
    { func_actor_503500_8013680C, 95 },
    { func_actor_503500_80134284, 79 },
    { func_actor_503500_80136770, 63 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016EE60[4] = {
    { func_actor_503500_80134284, 159 },
    { func_actor_503500_8013656C, 63 },
    { func_actor_503500_80136770, 31 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016EE80[3] = {
    { func_actor_503500_801364D0, 127 },
    { func_actor_503500_80136948, 127 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016EE98[3] = {
    { func_actor_503500_8013680C, 159 },
    { func_actor_503500_80134284, 95 },
    { NULL, 0 },
};

Actor503500Step D_actor_503500_8016EEB0[3] = {
    { func_actor_503500_80134284, 127 },
    { func_actor_503500_8013656C, 127 },
    { NULL, 0 },
};

Actor503500Step* D_actor_503500_8016EEC8[4] = {
    D_actor_503500_8016EDC8,
    D_actor_503500_8016EDE8,
    D_actor_503500_8016EDE8,
    D_actor_503500_8016EE00,
};

s16 D_actor_503500_8016EED8[4] = {
    700,
    1000,
    1300,
    2048,
};

Actor503500Step* D_actor_503500_8016EEE0[4] = {
    D_actor_503500_8016EE10,
    D_actor_503500_8016EE28,
    D_actor_503500_8016EE40,
    D_actor_503500_8016EE60,
};

s16 D_actor_503500_8016EEF0[4] = {
    700,
    1000,
    1300,
    2048,
};

Actor503500Step* D_actor_503500_8016EEF8[4] = {
    D_actor_503500_8016EE80,
    D_actor_503500_8016EE98,
    D_actor_503500_8016EE98,
    D_actor_503500_8016EEB0,
};

s16 D_actor_503500_8016EF08[4] = {
    500,
    1000,
    1300,
    2048,
};

Actor503500Step** D_actor_503500_8016EF10[2][3] = {
    { D_actor_503500_8016ED80, D_actor_503500_8016ED98, D_actor_503500_8016EDB0 },
    { D_actor_503500_8016EEC8, D_actor_503500_8016EEE0, D_actor_503500_8016EEF8 },
};

s16* D_actor_503500_8016EF28[2][3] = {
    { D_actor_503500_8016ED90, D_actor_503500_8016EDA8, D_actor_503500_8016EDC0 },
    { D_actor_503500_8016EED8, D_actor_503500_8016EEF0, D_actor_503500_8016EF08 },
};

s16 D_actor_503500_8016EF40[4] = {
    1,
    1,
    1,
    100,
};

s16 D_actor_503500_8016EF48[4] = {
    2,
    13,
    14,
    7,
};

s16 D_actor_503500_8016EF50[4] = {
    3,
    16,
    15,
    8,
};

SVECTOR D_actor_503500_8016EF58[7] = {
    { 0, -1000, 2000, 0 },
    { 0, -500, 2500, 0 },
    { 0, 0, 3000, 0 },
    { 200, -700, 2500, 0 },
    { -300, -200, 2200, 0 },
    { -200, -300, 2700, 0 },
    { 300, -800, 2500, 0 },
};

static SVECTOR _gActor503500Collision3D21CNormals[4] = {
#include "assets/actor_503500_collision_3D21C_normals.inc"
};

static SVECTOR _gActor503500Collision3D21CVerts[8] = {
#include "assets/actor_503500_collision_3D21C_verts.inc"
};

static WorldCollisionGridFace _gActor503500Collision3D21CFaces[4] = {
#include "assets/actor_503500_collision_3D21C_faces.inc"
};

static s16 _gActor503500Collision3D21CCells[10] = {
#include "assets/actor_503500_collision_3D21C_cells.inc"
};

#define GRID_CELL(i) (&_gActor503500Collision3D21CCells[i])
static s16* _gActor503500Collision3D21CTable[2] = {
#include "assets/actor_503500_collision_3D21C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_actor_503500_8016F03C = { NULL, _gActor503500Collision3D21CNormals, _gActor503500Collision3D21CVerts, _gActor503500Collision3D21CFaces, _gActor503500Collision3D21CTable, 2393, 1502, 2, 1, 4000, 4 };

SVECTOR D_actor_503500_8016F060 = { 0, -80, 596, 0 };

SVECTOR D_actor_503500_8016F068 = { 0, -80, 1000, 0 };

SVECTOR D_actor_503500_8016F070 = { 0, -200, 800, 0 };

SVECTOR D_actor_503500_8016F078[3] = {
    { 0, -300, -300, 0 },
    { 0, 300, 300, 0 },
    { 0, 300, -300, 0 },
};

SVECTOR D_actor_503500_8016F090[2] = {
    { 1300, -1468, -2700, 0 },
    { -1300, -1468, -2700, 0 },
};

SVECTOR D_actor_503500_8016F0A0[1] = {
    { -364, 1479, 0, 0 },
};

SVECTOR D_actor_503500_8016F0A8[1] = {
    { -364, -1479, 0, 0 },
};

SVECTOR D_actor_503500_8016F0B0 = { 0, 0, 400, 0 };

SVECTOR D_actor_503500_8016F0B8[2] = {
    { 3500, -1500, -5200, 0 },
    { -3500, -1500, -5200, 0 },
};

SVECTOR D_actor_503500_8016F0C8 = { 0, 0, 1000, 0 };

SVECTOR D_actor_503500_8016F0D0[3] = {
    { 0, -600, 0, 0 },
    { -600, 0, 0, 0 },
    { 600, 0, 0, 0 },
};

s32 D_actor_503500_8016F0E8[2] = {
    11,
    5,
};

SVECTOR D_actor_503500_8016F0F0[2] = {
    { 300, 0, 0, 0 },
    { -300, 0, 0, 0 },
};

RECT D_actor_503500_8016F100 = { 0, 253, 256, 1 };

SVECTOR D_actor_503500_8016F108[4] = {
    { 2000, 0, 1000, 0 },
    { 2000, 0, -1000, 0 },
    { -750, 300, 0, 0 },
    { -750, 600, 0, 0 },
};

SVECTOR D_actor_503500_8016F128[1][3] = {
    { { -750, 900, 0, 0 }, { -750, 1700, 0, 0 }, { -750, 1400, 0, 0 } },
};

SVECTOR D_actor_503500_8016F140 = { -750, 1100, 0, 0 };

RECT D_actor_503500_8016F148[2][2] = {
    { { 640, 256, 64, 256 }, { 0, 252, 256, 1 } },
    { { 704, 256, 64, 256 }, { 0, 261, 256, 1 } },
};

SVECTOR D_actor_503500_8016F168[9] = {
    { 1600, 0, 1000, 0 },
    { 1200, -400, 1000, 0 },
    { 1600, 400, 1000, 0 },
    { 1400, 0, 1400, 0 },
    { 1000, -400, 1400, 0 },
    { 1400, 400, 1400, 0 },
    { 1800, 0, 600, 0 },
    { 1400, -400, 600, 0 },
    { 1800, 400, 600, 0 },
};

SVECTOR D_actor_503500_8016F1B0 = { 0, 500, -1500, 0 };

SVECTOR D_actor_503500_8016F1B8[18] = {
    { 0, -400, -2000, 0 },
    { -800, -200, -1800, 0 },
    { 800, -200, -2200, 0 },
    { -1200, -100, -1400, 0 },
    { 1200, -100, -1400, 0 },
    { -1600, 0, -1200, 0 },
    { 1600, 0, -1200, 0 },
    { 200, 100, -2800, 0 },
    { -200, 100, -3100, 0 },
    { 0, -200, -2000, 0 },
    { -1200, 0, -2200, 0 },
    { 1200, 0, -2600, 0 },
    { -1600, 100, -1800, 0 },
    { 1600, 100, -1800, 0 },
    { -2000, 200, -1600, 0 },
    { 2000, 200, -1600, 0 },
    { 600, 300, -3200, 0 },
    { -600, 300, -3500, 0 },
};

SVECTOR D_actor_503500_8016F248[2] = {
    { 1300, -1468, -2800, 0 },
    { -1300, -1468, -2800, 0 },
};

SVECTOR D_actor_503500_8016F258 = { 1300, -1468, -2700, 0 };

SVECTOR D_actor_503500_8016F260 = { 500, 1700, 0, 0 };

SVECTOR D_actor_503500_8016F268[2] = {
    { 500, 1400, 0, 0 },
    { 500, 1100, 0, 0 },
};

SVECTOR D_actor_503500_8016F278[3] = {
    { 1300, -1468, -2700, 0 },
    { 1700, -1668, -2900, 0 },
    { 2100, -1868, -3100, 0 },
};

SVECTOR D_actor_503500_8016F290[9] = {
    { -1300, -1468, -2700, 0 },
    { -1700, -1668, -2900, 0 },
    { -2100, -1868, -3100, 0 },
    { 2000, -750, -2600, 0 },
    { 2400, -800, -2800, 0 },
    { 2800, -850, -3000, 0 },
    { -2000, -750, -2600, 0 },
    { -2400, -800, -2800, 0 },
    { -2800, -850, -3000, 0 },
};

SVECTOR D_actor_503500_8016F2D8 = { 0, 800, 2000, 0 };

s16 D_actor_503500_8016F2E0[6] = {
    3584,
    3328,
    3712,
    3456,
    3584,
    3712,
};

SVECTOR D_actor_503500_8016F2EC[6] = {
    { 712, 0, 0, 0 },
    { 688, 24, 0, 0 },
    { 780, 88, 0, 0 },
    { 745, -46, 0, 0 },
    { 656, -20, 0, 0 },
    { 780, -80, 0, 0 },
};

SVECTOR D_actor_503500_8016F31C[9] = {
    { 0, 800, 3000, 0 },
    { -50, 1000, 2800, 0 },
    { 100, 1200, 2700, 0 },
    { -400, 600, 3000, 0 },
    { 450, 800, 2800, 0 },
    { -600, 1000, 2700, 0 },
    { 400, 200, 3000, 0 },
    { -50, 1200, 2800, 0 },
    { 600, 600, 2700, 0 },
};

RECT D_actor_503500_8016F364 = { 358, 480, 25, 31 };

SVECTOR D_actor_503500_8016F36C = { 0, 0, 800, 0 };

SVECTOR D_actor_503500_8016F374[6] = {
    { 0, 250, 800, 0 },
    { 400, 300, 1000, 0 },
    { -400, 350, 1100, 0 },
    { 400, -50, 1400, 0 },
    { -400, -100, 1200, 0 },
    { 0, -150, 1000, 0 },
};

RECT D_actor_503500_8016F3A4 = { 352, 256, 31, 40 };

SVECTOR D_actor_503500_8016F3AC[4] = {
    { 1300, -1468, -2700, 0 },
    { 1300, -1468, -2700, 0 },
    { -1300, -1468, -2700, 0 },
    { -1300, -1468, -2700, 0 },
};

SVECTOR D_actor_503500_8016F3CC[4] = {
    { -364, 1479, 0, 0 },
    { -364, 1479, 0, 0 },
    { -364, -1479, 0, 0 },
    { -364, -1479, 0, 0 },
};

SVECTOR D_actor_503500_8016F3EC = { 0, 0, 400, 0 };

SVECTOR D_actor_503500_8016F3F4[4] = {
    { -200, 0, 400, 0 },
    { 200, 0, 400, 0 },
    { -200, 0, 400, 0 },
    { 200, 0, 400, 0 },
};

SVECTOR D_actor_503500_8016F414[4] = {
    { 3500, -3000, -5200, 0 },
    { 1800, -2000, -5200, 0 },
    { -1800, -2000, -5200, 0 },
    { -3500, -3000, -5200, 0 },
};

s16 D_actor_503500_8016F434[10] = {
    0,
    10,
    30,
    60,
    80,
    120,
    180,
    200,
    300,
    0,
};

SVECTOR D_actor_503500_8016F448[3] = {
    { 0, -600, 0, 0 },
    { -400, 0, 0, 0 },
    { 400, 0, 0, 0 },
};

static AnimationPackedPose _gActor503500Animation3DE60Bank1[21] = {
#include "assets/actor_503500_animation_3DE60_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation3DE60Bank4[193] = {
#include "assets/actor_503500_animation_3DE60_bank4.inc"
};

static AnimationRecord _gActor503500Animation3DE60Records[254] = {
#include "assets/actor_503500_animation_3DE60_records.inc"
};

static u16 _gActor503500Animation3DE60Indices[20] = {
#include "assets/actor_503500_animation_3DE60_indices.inc"
};

static AnimationSet _gActor503500Animation3DE60 = {
    _gActor503500Animation3DE60Records,
    _gActor503500Animation3DE60Indices,
    { NULL, _gActor503500Animation3DE60Bank1, NULL, NULL, _gActor503500Animation3DE60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation3E5FCBank1[20] = {
#include "assets/actor_503500_animation_3E5FC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation3E5FCBank4[172] = {
#include "assets/actor_503500_animation_3E5FC_bank4.inc"
};

static AnimationRecord _gActor503500Animation3E5FCRecords[235] = {
#include "assets/actor_503500_animation_3E5FC_records.inc"
};

static u16 _gActor503500Animation3E5FCIndices[20] = {
#include "assets/actor_503500_animation_3E5FC_indices.inc"
};

static AnimationSet _gActor503500Animation3E5FC = {
    _gActor503500Animation3E5FCRecords,
    _gActor503500Animation3E5FCIndices,
    { NULL, _gActor503500Animation3E5FCBank1, NULL, NULL, _gActor503500Animation3E5FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation3EE08Bank1[15] = {
#include "assets/actor_503500_animation_3EE08_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation3EE08Bank4[206] = {
#include "assets/actor_503500_animation_3EE08_bank4.inc"
};

static AnimationRecord _gActor503500Animation3EE08Records[244] = {
#include "assets/actor_503500_animation_3EE08_records.inc"
};

static u16 _gActor503500Animation3EE08Indices[20] = {
#include "assets/actor_503500_animation_3EE08_indices.inc"
};

static AnimationSet _gActor503500Animation3EE08 = {
    _gActor503500Animation3EE08Records,
    _gActor503500Animation3EE08Indices,
    { NULL, _gActor503500Animation3EE08Bank1, NULL, NULL, _gActor503500Animation3EE08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation3F61CBank1[14] = {
#include "assets/actor_503500_animation_3F61C_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation3F61CBank4[207] = {
#include "assets/actor_503500_animation_3F61C_bank4.inc"
};

static AnimationRecord _gActor503500Animation3F61CRecords[248] = {
#include "assets/actor_503500_animation_3F61C_records.inc"
};

static u16 _gActor503500Animation3F61CIndices[20] = {
#include "assets/actor_503500_animation_3F61C_indices.inc"
};

static AnimationSet _gActor503500Animation3F61C = {
    _gActor503500Animation3F61CRecords,
    _gActor503500Animation3F61CIndices,
    { NULL, _gActor503500Animation3F61CBank1, NULL, NULL, _gActor503500Animation3F61CBank4, NULL, NULL, NULL },
};

s32 D_actor_503500_80171464[2] = {
    11,
    5,
};

TaskDesc D_actor_503500_8017146C = { { { TASK_BODY_NONE, 192 } }, func_actor_503500_80143AC0, { .value = 0 } };

SVECTOR D_actor_503500_80171478 = { 0 };

SVECTOR D_actor_503500_80171480[2] = {
    { 500, 0, 0, 0 },
    { -500, 0, 0, 0 },
};

s8 D_actor_503500_80171490[56] = {
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

AnimationSet* D_actor_503500_801714C8[5] = {
    NULL,
    &_gActor503500Animation3DE60,
    &_gActor503500Animation3E5FC,
    &_gActor503500Animation3EE08,
    &_gActor503500Animation3F61C,
};

s32 D_actor_503500_801714DC = 0;

AnimationPlayRequest D_actor_503500_801714E0[2] = {
    { { .sets = D_actor_503500_801714C8 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .sets = D_actor_503500_801714C8 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_actor_503500_80171508[2] = {
    { { .sets = D_actor_503500_801714C8 }, 3, ANIMATION_BLEND_INTERPOLATE, 3, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .sets = D_actor_503500_801714C8 }, 4, ANIMATION_BLEND_INTERPOLATE, 3, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_actor_503500_80171530 = { { .sets = D_actor_503500_801714C8 }, 5, ANIMATION_BLEND_INTERPOLATE, 3, ANIMATION_WORLD_COLLISION_ENABLE };

GameActorButtonPressHold D_actor_503500_80171544 = { 0 };

RECT D_actor_503500_8017155C = { 0, 261, 256, 1 };

SVECTOR D_actor_503500_80171564[5] = {
    { 0, 0, 0, 0 },
    { 0, -500, 500, 0 },
    { 0, -1000, 0, 0 },
    { 500, 0, 0, 0 },
    { -500, 0, 0, 0 },
};

SVECTOR D_actor_503500_8017158C = { -1000, 0, 0, 0 };

SVECTOR D_actor_503500_80171594 = { 1000, 0, 0, 0 };

PadScriptCmd D_actor_503500_8017159C[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_503500_801715A4[2] = { { 0, 0, 8, 0 }, { 255, 255, 8, 1 } };

SVECTOR D_actor_503500_801715AC = { 0 };

SVECTOR D_actor_503500_801715B4 = { 0 };

s32 D_actor_503500_801715BC[2] = {
    180,
    150,
};

SVECTOR D_actor_503500_801715C4 = { 0 };

SVECTOR D_actor_503500_801715CC = { 0, 2000, 6000, 0 };

SVECTOR D_actor_503500_801715D4 = { 0 };

SVECTOR D_actor_503500_801715DC = { 0 };

SVECTOR D_actor_503500_801715E4 = { 0, 500, 6000, 0 };

static TmdBone _gActor503500Actor361100Model06038Skeleton[19] = {
#include "assets/actor_361100_model_06038_skeleton.inc"
};

static u32 _gActor503500Actor361100Model06038PartVerts[19] = {
#include "assets/actor_361100_model_06038_partVerts.inc"
};

static SVECTOR _gActor503500Actor361100Model06038Verts[258] = {
#include "assets/actor_361100_model_06038_verts.inc"
};

static SVECTOR _gActor503500Actor361100Model06038Normals[270] = {
#include "assets/actor_361100_model_06038_normals.inc"
};

static u32 _gActor503500Actor361100Model06038Stream[3353] = {
#include "assets/actor_361100_model_06038_stream.inc"
};

static TmdSource _gActor503500Actor361100Model06038 = {
    0,
    15104,
    9712,
    19,
    _gActor503500Actor361100Model06038PartVerts,
    _gActor503500Actor361100Model06038Verts,
    _gActor503500Actor361100Model06038Normals,
    _gActor503500Actor361100Model06038Skeleton,
    _gActor503500Actor361100Model06038Stream,
};

static AnimationPackedPose _gActor503500Animation444F4Bank1[15] = {
#include "assets/actor_503500_animation_444F4_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation444F4Bank4[49] = {
#include "assets/actor_503500_animation_444F4_bank4.inc"
};

static AnimationRecord _gActor503500Animation444F4Records[226] = {
#include "assets/actor_503500_animation_444F4_records.inc"
};

static u16 _gActor503500Animation444F4Indices[20] = {
#include "assets/actor_503500_animation_444F4_indices.inc"
};

static AnimationSet _gActor503500Animation444F4 = {
    _gActor503500Animation444F4Records,
    _gActor503500Animation444F4Indices,
    { NULL, _gActor503500Animation444F4Bank1, NULL, NULL, _gActor503500Animation444F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation446CCBank1[6] = {
#include "assets/actor_503500_animation_446CC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation446CCBank4[18] = {
#include "assets/actor_503500_animation_446CC_bank4.inc"
};

static AnimationRecord _gActor503500Animation446CCRecords[62] = {
#include "assets/actor_503500_animation_446CC_records.inc"
};

static u16 _gActor503500Animation446CCIndices[20] = {
#include "assets/actor_503500_animation_446CC_indices.inc"
};

static AnimationSet _gActor503500Animation446CC = {
    _gActor503500Animation446CCRecords,
    _gActor503500Animation446CCIndices,
    { NULL, _gActor503500Animation446CCBank1, NULL, NULL, _gActor503500Animation446CCBank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_503500_80176514[3] = {
    NULL,
    &_gActor503500Animation444F4,
    &_gActor503500Animation446CC,
};

AnimationSet** gActorMotionAnimBanks19[1] = {
    D_actor_503500_80176514,
};

TaskDesc D_actor_503500_80176524 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_503500_801463C0, { .model = &_gActor503500Actor361100Model06038 } };

TaskMessageEntry D_actor_503500_80176530[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, func_actor_503500_80146664 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_503500_801466E0 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_503500_801467C0 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_actor_503500_80144E8C(Task* arg0)
{
    Actor503500WorkD0*     work;
    GfxCoord*              coord;
    WorldCollisionCapsule* capsule;
    WorldCollisionContact* contacts;
    EffectWork*            eff;
    Task*                  child;
    GfxRotationWords*      m1;
    GfxRotationWords*      m2;
    s32                    pan;

    coord = arg0->extra.tmd->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work     = work;
    work->field_C4 = 0x1000;

    m1         = (GfxRotationWords*)&coord->coord;
    m1->m00M01 = ONE;
    m1->m02M10 = 0;
    m1->m11M12 = ONE;
    m1->m20M21 = 0;
    m1->m22    = ONE;

    m2         = (GfxRotationWords*)&work->field_9C;
    m2->m00M01 = ONE;
    m2->m02M10 = 0;
    m2->m11M12 = ONE;
    m2->m20M21 = 0;
    m2->m22    = ONE;

    capsule  = &work->capsule;
    contacts = work->contacts;

    work->body.coord           = coord;
    work->body.context.capsule = capsule;
    work->body.pos.vx          = D_actor_503500_801715C4.vx;
    work->body.pos.vy          = D_actor_503500_801715C4.vy;
    work->body.pos.vz          = D_actor_503500_801715C4.vz;
    work->body.key             = Gp_PackPair(D_actor_503500_8016E7D4[0], 0);
    work->body.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->body.radius          = 0;

    capsule->contacts   = contacts;
    capsule->ends[1].vx = 0;
    capsule->ends[1].vy = 0;
    capsule->ends[1].vz = 0;
    capsule->ends[0].vx = D_actor_503500_801715CC.vx;
    capsule->ends[0].vy = D_actor_503500_801715CC.vy;
    capsule->ends[0].vz = D_actor_503500_801715CC.vz;
    capsule->end1Radius = 0x3E8;
    capsule->end0Radius = 0x7D0;

    Gp_LinkObj(3, &work->body);
    Gp_InitRec18Table(contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    if (arg0->spawnArg1.value == 0) {
        eff = Gp_SpawnEff(EFFECT_SHELTER_R48_RING_FLASH_PINK, coord, 0, NULL);
        if (eff == NULL) {
            func_actor_503500_80145480(arg0);
            return;
        }
        child          = eff->task;
        work->field_98 = child;
        taskReparent(arg0, child);
    }
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0A), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    func_actor_503500_80137290(6);
    arg0->exitCallback = func_actor_503500_80145480;
    arg0->state       += 1;
}

static void func_actor_503500_801450A0(Task* arg0)
{
    Actor503500WorkD0*     work;
    WorldCollisionCapsule* capsule;
    GfxCoord*              coord;
    s32                    pan;
    s32                    step;
    s32                    ang;

    work    = (Actor503500WorkD0*)arg0->work;
    capsule = &work->capsule;
    switch (work->field_CC) {
        case 0:
            if (++work->field_C6 >= 0x1F) {
                if (arg0->spawnArg1.value == 0) {
                    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
                coord = arg0->extra.tmd->coords;
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0B), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                work->field_C6 = 0;
                work->field_CC++;
            }
            break;
        case 1:
            if (++work->field_C6 >= 0x15) {
                if (arg0->spawnArg1.value != 0) {
                    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
                work->field_C6 = 0;
                work->field_CC++;
            }
            break;
        case 2:
            step = -0x20000;
            if (arg0->spawnArg1.value != 0) {
                step = 0x20000;
            }
            {
                GfxRotationWords* m;

                m                    = (GfxRotationWords*)&work->field_9C;
                m->m00M01            = ONE;
                work->field_C0      += step;
                work->field_BC.word += work->field_C0;
                m->m02M10            = 0;
                m->m11M12            = ONE;
                m->m20M21            = 0;
                m->m22               = ONE;
            }
            RotMatrixY(work->field_BC.halves.integer, &work->field_9C);
            ang = work->field_BC.halves.integer;
            if (ang < 0) {
                ang = -ang;
            }
            if (ang > 0x100) {
                work->field_CC++;
            }
            break;
        case 3:
            if (arg0->spawnArg1.value != 0) {
                work->field_C0 -= 0x20000;
                if (work->field_C0 < 0) {
                    work->field_C0 = 0;
                    work->field_CC++;
                }
            } else {
                work->field_C0 += 0x20000;
                if (work->field_C0 > 0) {
                    work->field_C0 = 0;
                    work->field_CC++;
                }
            }
            {
                GfxRotationWords* m;

                m                    = (GfxRotationWords*)&work->field_9C;
                m->m00M01            = ONE;
                work->field_BC.word += work->field_C0;
                m->m02M10            = 0;
                m->m11M12            = ONE;
                m->m20M21            = 0;
                m->m22               = ONE;
            }
            RotMatrixY(work->field_BC.halves.integer, &work->field_9C);
            break;
        case 4:
            if (work->field_C4 <= 0x400) {
                work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->field_CC++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
    if (work->field_CC >= 2) {
        work->field_C4 -= 0x50;
        if (work->field_C4 < 0x200) {
            work->field_C4 = 0x200;
        }
        capsule->end0Radius = work->field_C4 * 0x177 >> 9;
        capsule->end1Radius = work->field_C4 * 0x7D >> 9;
        gte_SetRotMatrix(&work->field_9C);
        gte_ldv0(&D_actor_503500_801715CC);
        gte_rtv0();
        gte_stsv(&capsule->ends[0]);
    }
}

static void func_actor_503500_80145428(Task* arg0)
{
    GfxCoord* coord;
    s32       state;

    state = gSceneCombatState.actorControl;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord               = arg0->extra.tmd->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_503500_801454E0(arg0);
    func_actor_503500_801450A0(arg0);
}

static void func_actor_503500_80145480(Task* arg0)
{
    TmdObject* ext;

    func_actor_503500_801372AC(6);
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0B), 1);
    ext                   = arg0->extra.tmd;
    (ext->coords)->parent = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500WorkD0*)arg0->work)->body);
    taskKill(arg0);
}

static void func_actor_503500_801454E0(Task* arg0)
{
    Actor503500WorkD0*     work;
    WorldCollisionContact* contacts;
    s32                    i;

    work     = arg0->work;
    contacts = work->contacts;
    for (i = 0; i < ARRAY_SIZE(work->contacts); i++) {
        if ((contacts[i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x10000) {
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
    Gp_ClearRec18Occupied(contacts);
}

void func_actor_503500_8014554C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801321F4;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_801459D4` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132218 = {
    {
        func_actor_503500_801455A4,
        func_actor_503500_801458F8,
        func_actor_503500_80145950,
    },
};

static void func_actor_503500_801455A4(Task* arg0)
{
    Actor503500Work44* work;
    GfxCoord*          coord;
    GfxRotationWords*  m;
    EffectWork*        eff;
    Task*              child;
    s32                pan;

    coord = arg0->extra.tmd->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = work;

    m         = (GfxRotationWords*)&coord->coord;
    m->m00M01 = ONE;
    m->m02M10 = 0;
    m->m11M12 = ONE;
    m->m20M21 = 0;
    m->m22    = ONE;

    work->body.coord            = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    work->body.context.contacts = work->contacts;
    work->body.pos.vx           = D_actor_503500_801715D4.vx;
    work->body.pos.vy           = D_actor_503500_801715D4.vy;
    work->body.pos.vz           = D_actor_503500_801715D4.vz;
    work->body.key              = Gp_PackPair(D_actor_503500_8016E7D4[1], 0);
    work->body.radius           = 0x12C;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->body);
    Gp_InitRec18Table(work->contacts, 1, 0);
    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    eff = Gp_SpawnEff(EFFECT_SHELTER_R48_RING_FLASH_YELLOW, coord, 0, NULL);
    if (eff == NULL) {
        func_actor_503500_80145950(arg0);
        return;
    }
    child          = eff->task;
    work->field_38 = child;
    taskReparent(arg0, child);
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0C), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    func_actor_503500_80137290(6);
    arg0->exitCallback = func_actor_503500_80145950;
    arg0->state       += 1;
}

static void func_actor_503500_80145754(Task* arg0)
{
    Actor503500Work44* work;
    GfxCoord*          coord;
    GfxCoord*          coord2;
    s32                pan;
    s32                pan2;

    work = (Actor503500Work44*)arg0->work;
    if (func_actor_503500_8013608C(arg0) == 0) {
        switch (work->field_40) {
            case 0:
                work->field_3C++;
                if (work->field_3C >= 0x5F) {
                    if (gPlayerStatus.coordMtx->t[1] < -1000) {
                        work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    }
                    work->field_3C = 0;
                    work->field_40++;
                } else if (work->field_3C == 0x3E) {
                    coord = arg0->extra.tmd->coords;
                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x14), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                } else if (work->field_3C == 0x5A) {
                    coord2 = arg0->extra.tmd->coords;
                    pan2   = (s8)worldCoordGetOriginAudioPan(coord2);
                    SndEvt_EnqueueType6(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0D), pan2, (s8)(worldCoordGetOriginAudioDepth(coord2) / 2));
                }
                return;
            case 1:
                work->field_3C++;
                if (work->field_3C >= 4) {
                    work->field_3C    = 0;
                    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->field_40++;
                }
                return;
        }
    }
    arg0->state += 1;
}

static void func_actor_503500_801458F8(Task* arg0)
{
    GfxCoord* coord;
    s32       state;

    coord = arg0->extra.tmd->coords;
    state = gSceneCombatState.actorControl;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_503500_801459B0(arg0);
    func_actor_503500_80145754(arg0);
}

static void func_actor_503500_80145950(Task* arg0)
{
    TmdObject* ext;

    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0C), 1);
    func_actor_503500_801372AC(6);
    ext                   = arg0->extra.tmd;
    (ext->coords)->parent = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500Work44*)arg0->work)->body);
    taskKill(arg0);
}

static void func_actor_503500_801459B0(Task* arg0)
{
    Gp_ClearRec18Occupied(((Actor503500Work44*)arg0->work)->contacts);
}

void func_actor_503500_801459D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132218;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_80145F84` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132224 = {
    {
        func_actor_503500_80145A2C,
        func_actor_503500_80145E1C,
        func_actor_503500_80145E98,
    },
};

static void func_actor_503500_80145A2C(Task* arg0)
{
    Actor503500WorkAC*     work;
    GfxCoord*              coord;
    WorldCollisionCapsule* capsule;
    WorldCollisionContact* contacts;
    EffectWork*            eff;
    Task*                  child;
    GfxRotationWords*      m;
    s32                    pan;
    s32                    pan2;

    coord = arg0->extra.tmd->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = work;

    m         = (GfxRotationWords*)&coord->coord;
    m->m00M01 = ONE;
    m->m02M10 = 0;
    m->m11M12 = ONE;
    m->m20M21 = 0;
    m->m22    = ONE;

    capsule  = &work->capsule;
    contacts = work->contacts;

    work->body.coord           = coord;
    work->body.context.capsule = capsule;
    work->body.pos.vx          = D_actor_503500_801715DC.vx;
    work->body.pos.vy          = D_actor_503500_801715DC.vy;
    work->body.pos.vz          = D_actor_503500_801715DC.vz;
    work->body.key             = Gp_PackPair(D_actor_503500_8016E7DC[0], 0);
    work->body.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->body.radius          = 0;

    capsule->contacts   = contacts;
    capsule->ends[1].vx = 0;
    capsule->ends[1].vy = 0;
    capsule->ends[1].vz = 0;
    capsule->ends[0].vx = D_actor_503500_801715E4.vx;
    capsule->ends[0].vy = D_actor_503500_801715E4.vy;
    capsule->ends[0].vz = D_actor_503500_801715E4.vz;
    capsule->end1Radius = 0x7D0;
    capsule->end0Radius = 0xBB8;

    Gp_LinkObj(3, &work->body);
    Gp_InitRec18Table(contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    eff = Gp_SpawnEff(EFFECT_SHELTER_R48_RING_FLASH, coord, arg0->spawnArg1.value, NULL);
    if (eff == NULL) {
        func_actor_503500_80145E98(arg0);
        return;
    }
    child          = eff->task;
    work->field_98 = child;
    taskReparent(arg0, child);
    if (gGameSession->eventState != 0) {
        pan = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x13), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    } else {
        pan2 = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0E), pan2, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    }
    func_actor_503500_80137290(8);
    arg0->exitCallback = func_actor_503500_80145E98;
    arg0->state       += 1;
}

static void func_actor_503500_80145C50(Task* arg0)
{
    Actor503500WorkAC* work;
    GfxCoord*          coord;
    s32                pan;

    work = (Actor503500WorkAC*)arg0->work;
    switch (work->field_A8) {
        case 0:
            if (gDisplayState.animFrame & 1) {
                Gp_SpawnPadLerp(1, 0x96, 0x96);
            }
            if (++work->field_A4 < 0x5B) {
                return;
            }
            if (gGameSession->eventState == 0) {
                work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                coord             = arg0->extra.tmd->coords;
                pan               = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0F), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
            }
            goto next;
        case 1:
            if (gGameSession->eventState == 0) {
                Gp_SpawnPadLerp(1, 0xFF, 0xFF);
            }
            if (++work->field_A4 < 0x38) {
                return;
            }
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0F), 1);
        next:
            work->field_A4 = 0;
            work->field_A8++;
            return;
        case 2:
            if (++work->field_A4 < 0x24) {
                return;
            }
        default:
            arg0->state += 1;
            return;
    }
}

static void func_actor_503500_80145E1C(Task* arg0)
{
    GfxCoord* coord;
    s32       state;

    coord = arg0->extra.tmd->coords;
    state = gSceneCombatState.actorControl;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_503500_80145F18(arg0);
    func_actor_503500_80145C50(arg0);
    if (func_actor_503500_8013608C(arg0->spawnArg2.pointer)) {
        arg0->exitCallback(arg0);
    }
}

static void func_actor_503500_80145E98(Task* arg0)
{
    TmdObject* ext;

    func_actor_503500_801372AC(8);
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0E), 1);
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x13), 1);
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x0F), 1);
    ext                   = arg0->extra.tmd;
    (ext->coords)->parent = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500WorkAC*)arg0->work)->body);
    taskKill(arg0);
}

static void func_actor_503500_80145F18(Task* arg0)
{
    Actor503500WorkAC*     work;
    WorldCollisionContact* contacts;
    s32                    i;

    work     = arg0->work;
    contacts = work->contacts;
    for (i = 0; i < ARRAY_SIZE(work->contacts); i++) {
        if ((contacts[i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x10000) {
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
    Gp_ClearRec18Occupied(contacts);
}

void func_actor_503500_80145F84(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132224;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_801463C0` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132230 = {
    {
        func_actor_503500_8014642C,
        func_actor_503500_80145FDC,
        func_actor_503500_801464E8,
    },
};

/// Per-frame tick of the `Actor503500Effect4CC` effect: runs the motion
/// handler `field_4C0` selects, adds the 16.16 velocity `field_4B0` onto the
/// accumulator `field_4A0`, moves the coordinate by the integer part and keeps
/// only the fraction, then ticks the animation slots and the actor colour.
static void func_actor_503500_80145FDC(Task* task)
{
    VECTOR                pos;
    TmdObject*            ext      = task->extra.tmd;
    Actor503500Effect4CC* work     = (Actor503500Effect4CC*)task->work;
    TaskFunc              funcs[2] = { func_actor_503500_80146524, func_actor_503500_8014618C };
    GfxCoord*             coord;
    s32                   i;

    funcs[work->field_4C0](task);
    coord               = task->extra.tmd->coords;
    work->field_4A0    += work->field_4B0;
    work->field_4A4    += work->field_4B4;
    work->field_4A8    += work->field_4B8;
    coord->coord.t[0]  += (s16)(work->field_4A0 >> 16);
    coord->coord.t[1]  += (s16)(work->field_4A4 >> 16);
    coord->coord.t[2]  += (s16)(work->field_4A8 >> 16);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_4A0     = (u16)work->field_4A0;
    work->field_4A4     = (u16)work->field_4A4;
    work->field_4A8     = (u16)work->field_4A8;
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        pos.vx = coord->workm.t[0];
        pos.vy = coord->workm.t[1];
        pos.vz = coord->workm.t[2];
        Gp_UpdateActorColor(task->spawnArg2.pointer, &pos, 0, 0);
    }
    if (work->field_4C8 >= 0) {
        if (work->field_4C8 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4C8--;
    }
}

/// Motion handler 1 of `Actor503500Effect4CC` (`field_4C0`), stepped by
/// `field_4C2`: state 0 saves the coordinate's rotation words into
/// `field_480`/`field_490`, state 1 waits 31 frames, and state 2 restores that
/// rotation every frame while squashing its Y scale `field_4C6` from 0x1000 down
/// to 0x200, firing the light and spark cues on the way before advancing the
/// task at frame 150.
static void func_actor_503500_8014618C(Task* arg0)
{
    VECTOR                scale;
    GfxCoord*             coord;
    Actor503500Effect4CC* work;
    TmdObject*            ext;
    void*                 enemy;
    s32*                  src;
    s32*                  dst;
    s32                   i;

    // `extra` is read twice on purpose: the second read is what leaves the
    // target's `move s2, v0` copy.
    coord = arg0->extra.tmd->coords;
    work  = (Actor503500Effect4CC*)arg0->work;
    enemy = arg0->spawnArg2.pointer;
    ext   = arg0->extra.tmd;
    switch (work->field_4C2) {
        case 0:
            work->field_4C4 = 0;
            work->field_4C6 = 0x1000;
            dst             = work->field_480;
            src             = (s32*)coord->coord.m;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            work->field_490 = coord->coord.m[2][2];
            work->field_4C2++;
            break;
        case 1:
            work->field_4C4++;
            if (work->field_4C4 >= 0x1F) {
                work->field_4C4 = 0;
                work->field_4C2++;
            }
            break;
        case 2:
            if (work->field_4C6 > 0x200) {
                work->field_4C6 -= 0x10;
            }
            dst = (s32*)coord->coord.m;
            src = work->field_480;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            coord->coord.m[2][2] = work->field_490;
            scale.vx             = 0x1000;
            scale.vy             = work->field_4C6;
            scale.vz             = 0x1000;
            ScaleMatrixL(&coord->coord, &scale);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->field_4C4++;
            switch (work->field_4C4) {
                case 0x14:
                    ext->flags |= TMD_OBJECT_SEMI_TRANS;
                    Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                    break;
                case 0x1E:
                    Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 2, NULL);
                    break;
                case 0x64:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case 0x96:
                    arg0->state++;
                    break;
            }
            break;
    }
}

void func_actor_503500_801463C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132230;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

static void func_actor_503500_8014642C(Task* arg0)
{
    Actor503500Effect4CC* work;
    GfxCoord*             coord;
    Enemy*                enemy;

    coord = arg0->extra.tmd->coords;
    enemy = arg0->spawnArg2.pointer;

    work = memCalloc(sizeof(Actor503500Effect4CC), false);
    if (work == NULL) {
        enemyTaskExit(arg0);
        return;
    }

    arg0->work         = work;
    work->model.animId = ACTOR_MODEL_STATE_NONE;
    work->model.bank   = ACTOR_MODEL_STATE_NONE;
    work->field_4C8    = -1;
    work->field_4A0    = 0;
    work->field_4A4    = 0;
    work->field_4A8    = 0;

    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    enemy->recs     = 0;

    func_actor_503500_80146508(arg0);

    arg0->msgTable     = D_actor_503500_80176530;
    arg0->exitCallback = func_actor_503500_801464E8;
    arg0->state++;
}
/// `Task::exitCallback` of the effect task `func_actor_503500_8014642C`
/// initialises, and the third entry of its state table: hands the task to
/// `enemyTaskExit`.
static void func_actor_503500_801464E8(Task* arg0)
{
    enemyTaskExit(arg0);
}

static void func_actor_503500_80146508(Task* arg0)
{
    TmdObject*            ext;
    Actor503500Effect4CC* work;

    work          = (Actor503500Effect4CC*)arg0->work;
    ext           = arg0->extra.tmd;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

static void func_actor_503500_80146524(Task* arg0)
{
}

#include "../../shared/actor_motion_play19.inc.c"

/// A second copy of the handler, under this file's own name.
#define actorMsgPlaceEuler func_actor_503500_80146664
#include "../../shared/actor_messages_place_euler.inc.c"
#undef actorMsgPlaceEuler

s32 func_actor_503500_801466E0(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject* ext;
    s32        ret;

    ext = task->extra.tmd;
    ret = 0;
    switch (mode) {
        case 0:
            ext->flags = (ext->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW) & ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            ext->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(ext);
            ext->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            ext->flags                                    |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Actor503500Effect4CC*)task->work)->field_4C8 = mode;
            ext->flags                                    |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            ext->flags = (ext->flags & ~TMD_OBJECT_SKIP_ACTIVE_DRAW) | TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_503500_801467C0(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    Actor503500Effect4CC* work;

    work = (Actor503500Effect4CC*)task->work;
    switch (msg->command) {
        case 0:
            work->field_4B0 = 0;
            work->field_4B4 = 0;
            work->field_4B8 = 0;
            break;
        case 1:
            work->field_4B0 = 0x0100F4DE;
            work->field_4B4 = 0xFF6DE9BE;
            work->field_4B8 = 0x68590;
            break;
        case 2:
            work->field_4B0 = 0x1371C7;
            work->field_4B4 = 0xBAAAA;
            work->field_4B8 = 0;
            work->field_4C0 = 1;
            break;
        case 3:
            task->exitCallback(task);
            break;
    }
    return 0;
}
