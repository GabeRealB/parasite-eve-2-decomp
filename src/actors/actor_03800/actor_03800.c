#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/object_fields.h"
#include "gameplay/pairsrc.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

typedef struct Actor103800Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[6];
    /* 0x104 */ byte       field_104[0x60];
    /* 0x164 */ MATRIX     field_164;
    /* 0x184 */ MATRIX     field_184;
    /* 0x1A4 */ byte       field_1A4[8];
    /* 0x1AC */ void*      field_1AC;
    /* 0x1B0 */ GpRec18*   field_1B0;
    /* 0x1B4 */ u16        field_1B4;
    /* 0x1B6 */ s16        field_1B6;
    /* 0x1B8 */ u16        field_1B8;
    /* 0x1BA */ byte       pad_1BA[2];
    /* 0x1BC */ u32        field_1BC;
    /* 0x1C0 */ u16        field_1C0;
    /* 0x1C2 */ u16        field_1C2;
    /* 0x1C4 */ GpRec18    field_1C4[3];
    /* 0x20C */ byte       field_20C[8];
    /* 0x214 */ void*      field_214;
    /* 0x218 */ GpRec18*   field_218;
    /* 0x21C */ u16        field_21C;
    /* 0x21E */ s16        field_21E;
    /* 0x220 */ u16        field_220;
    /* 0x222 */ byte       pad_222[2];
    /* 0x224 */ u32        field_224;
    /* 0x228 */ s16        field_228;
    /* 0x22A */ u16        field_22A;
    /* 0x22C */ GpRec18    field_22C[4];
    /* 0x28C */ byte       field_28C[8];
    /* 0x294 */ void*      field_294;
    /* 0x298 */ GpRec18*   field_298;
    /* 0x29C */ u16        field_29C;
    /* 0x29E */ s16        field_29E;
    /* 0x2A0 */ u16        field_2A0;
    /* 0x2A2 */ byte       pad_2A2[2];
    /* 0x2A4 */ u32        field_2A4;
    /* 0x2A8 */ u16        field_2A8;
    /* 0x2AA */ u16        field_2AA;
    /* 0x2AC */ GpRec18    field_2AC[1];
    /* 0x2C4 */ GpEffArg   field_2C4; // record the death effect is spawned with
    /* 0x2CC */ MATRIX     field_2CC;
    /* 0x2EC */ s16        field_2EC;
    /* 0x2EE */ s16        field_2EE;
    /* 0x2F0 */ s16        field_2F0;
    /* 0x2F2 */ byte       pad_2F2[2];
    /// Coordinate node `Actor03800_Fn003B8` publishes on `field_344` for the
    /// detached modes (spawn kinds 1 and 2): it is seeded from the model's own
    /// `TmdObject::coords`, parented to `gGfxViewCoord` and then turned
    /// by 0x400 / 0x800 about X. `coord.coord.t` is the saved world translation
    /// the idle and detach ticks restore after rebuilding the rotation.
    /* 0x2F4 */ GpCoord  coord;
    /* 0x344 */ GpCoord* field_344;
    /* 0x348 */ u16      field_348;
    /* 0x34A */ s16      field_34A;
    /* 0x34C */ u16      field_34C;
    /* 0x34E */ s16      field_34E;
    /* 0x350 */ s16      field_350;
    /* 0x352 */ s16      field_352;
    /* 0x354 */ s16      field_354;
    /* 0x356 */ s16      field_356;
    /* 0x358 */ s16      field_358;
    /* 0x35A */ s16      field_35A;
    /* 0x35C */ s16      field_35C;
    /* 0x35E */ s16      field_35E;
    /* 0x360 */ s16      field_360;
    /* 0x362 */ s16      field_362;
    /* 0x364 */ s16      field_364;
    /* 0x366 */ s16      field_366;
    /* 0x368 */ s16      field_368;
    /* 0x36A */ s16      field_36A;
    /* 0x36C */ s16      field_36C;
    /* 0x36E */ s16      field_36E;
    /* 0x370 */ s16      field_370;
    /* 0x372 */ s16      field_372;
    /* 0x374 */ s16      field_374;
    /* 0x376 */ byte     pad_376[2];
    /* 0x378 */ s16      field_378;
    /* 0x37A */ s16      field_37A;
    /* 0x37C */ s16      field_37C;
    /* 0x37E */ s16      field_37E;
} Actor103800Work;
STATIC_ASSERT_SIZEOF(Actor103800Work, 0x380);

typedef struct Actor03800TurnScratch {
    /* 0x00 */ SVECTOR rotation;
    /* 0x08 */ MATRIX  matrix;
} Actor03800TurnScratch;
STATIC_ASSERT_SIZEOF(Actor03800TurnScratch, 0x28);

typedef struct Actor03800MoveScratch {
    VECTOR  delta;
    SVECTOR normal;
} Actor03800MoveScratch;
STATIC_ASSERT_SIZEOF(Actor03800MoveScratch, 0x18);

extern void*     D_80067704[1];
extern TmdSource Actor03800_D0459C;
extern TmdSource Actor03800_D046A0;
extern TmdSource Actor03800_D047A4;
extern TmdSource Actor03800_D04868;
extern TmdSource Actor03800_D0492C;

extern s16        Actor03800_D05F90[];
extern s16        Actor03800_D05FA8[];
extern GpU16Pair  Actor03800_D05F40[1];
extern GpPairSrcE Actor03800_D05F44;
extern GpAnimSet* Actor03800_D05F60[12];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void Actor03800_Fn000B8(GpEnemy* arg0, Task* arg1);
static void Actor03800_Fn003B8(Task* arg0);
static void Actor03800_Fn00974(Task* arg0);
static void Actor03800_Fn00A98(Task* arg0);
static void Actor03800_Fn026F8(Task* arg0);
static void Actor03800_Fn02848(Task* arg0);
static void Actor03800_Fn02998(GpEnemy* arg0, Task* arg1);
static void Actor03800_Fn02E50(Task* arg0);
static void Actor03800_Fn03008(Task* actor, u32 variant);
static void Actor03800_Fn031B8(GpEnemy* arg0, Task* arg1);
static void Actor03800_Fn032D8(Task* arg0);
static void Actor03800_Fn03420(Task* arg0);
static void Actor03800_Fn034B0(Task* arg0);
static void Actor03800_Fn03594(Task* arg0);
static void Actor03800_Fn03628(Task* arg0);
static void Actor03800_Fn036EC(Task* arg0);
static void Actor03800_Fn03744(Task* arg0);
static void Actor03800_Fn037E0(Task* arg0);

/// State handlers `Actor03800_Fn0315C` dispatches, indexed by the task's state
/// (`Task::state`): the spawn state that allocates the work block and
/// moves to state 1, the per-frame tick, and the state-2 handler the tick hands
/// over to, which carries the death sequence.
static const GpEnemyTaskFuncTable3 Actor03800_D00004 = {
    {
        Actor03800_Fn000B8,
        Actor03800_Fn031B8,
        Actor03800_Fn02998,
    },
};

extern GpAnimSet Actor03800_D04B24;
extern GpAnimSet Actor03800_D04D20;
extern GpAnimSet Actor03800_D04FE0;
extern GpAnimSet Actor03800_D05534;
extern GpAnimSet Actor03800_D055F0;
extern GpAnimSet Actor03800_D05734;
extern GpAnimSet Actor03800_D05824;
extern GpAnimSet Actor03800_D05988;
extern GpAnimSet Actor03800_D05B84;
extern GpAnimSet Actor03800_D05E14;
extern GpAnimSet Actor03800_D05F18;
extern TmdSource Actor03800_D043A0;
static void      Actor03800_Fn0315C(Task*);

TmdBone Actor03800_D038D0[6] = {
#include "assets/actor_103800_model_043A0_skeleton.inc"
};

u32 Actor03800_D039A8[6] = {
#include "assets/actor_103800_model_043A0_partVerts.inc"
};

SVECTOR Actor03800_D039C0[33] = {
#include "assets/actor_103800_model_043A0_verts.inc"
};

SVECTOR Actor03800_D03AC8[42] = {
#include "assets/actor_103800_model_043A0_normals.inc"
};

u32 Actor03800_D03C18[482] = {
#include "assets/actor_103800_model_043A0_stream.inc"
};

TmdSource Actor03800_D043A0 = {
    0,
    2120,
    1072,
    6,
    Actor03800_D039A8,
    Actor03800_D039C0,
    Actor03800_D03AC8,
    Actor03800_D038D0,
    Actor03800_D03C18,
};

TmdBone Actor03800_D043C4[1] = {
#include "assets/actor_103800_model_0459C_skeleton.inc"
};

u32 Actor03800_D043E8[1] = {
#include "assets/actor_103800_model_0459C_partVerts.inc"
};

SVECTOR Actor03800_D043EC[8] = {
#include "assets/actor_103800_model_0459C_verts.inc"
};

SVECTOR Actor03800_D0442C[8] = {
#include "assets/actor_103800_model_0459C_normals.inc"
};

u32 Actor03800_D0446C[76] = {
#include "assets/actor_103800_model_0459C_stream.inc"
};

TmdSource Actor03800_D0459C = {
    0,
    452,
    0,
    1,
    Actor03800_D043E8,
    Actor03800_D043EC,
    Actor03800_D0442C,
    Actor03800_D043C4,
    Actor03800_D0446C,
};

TmdBone Actor03800_D045C0[1] = {
#include "assets/actor_103800_model_046A0_skeleton.inc"
};

u32 Actor03800_D045E4[1] = {
#include "assets/actor_103800_model_046A0_partVerts.inc"
};

SVECTOR Actor03800_D045E8[4] = {
#include "assets/actor_103800_model_046A0_verts.inc"
};

SVECTOR Actor03800_D04608[4] = {
#include "assets/actor_103800_model_046A0_normals.inc"
};

u32 Actor03800_D04628[30] = {
#include "assets/actor_103800_model_046A0_stream.inc"
};

TmdSource Actor03800_D046A0 = {
    0,
    160,
    0,
    1,
    Actor03800_D045E4,
    Actor03800_D045E8,
    Actor03800_D04608,
    Actor03800_D045C0,
    Actor03800_D04628,
};

TmdBone Actor03800_D046C4[1] = {
#include "assets/actor_103800_model_047A4_skeleton.inc"
};

u32 Actor03800_D046E8[1] = {
#include "assets/actor_103800_model_047A4_partVerts.inc"
};

SVECTOR Actor03800_D046EC[4] = {
#include "assets/actor_103800_model_047A4_verts.inc"
};

SVECTOR Actor03800_D0470C[4] = {
#include "assets/actor_103800_model_047A4_normals.inc"
};

u32 Actor03800_D0472C[30] = {
#include "assets/actor_103800_model_047A4_stream.inc"
};

TmdSource Actor03800_D047A4 = {
    0,
    160,
    0,
    1,
    Actor03800_D046E8,
    Actor03800_D046EC,
    Actor03800_D0470C,
    Actor03800_D046C4,
    Actor03800_D0472C,
};

TmdBone Actor03800_D047C8[1] = {
#include "assets/actor_103800_model_04868_skeleton.inc"
};

u32 Actor03800_D047EC[1] = {
#include "assets/actor_103800_model_04868_partVerts.inc"
};

SVECTOR Actor03800_D047F0[4] = {
#include "assets/actor_103800_model_04868_verts.inc"
};

SVECTOR Actor03800_D04810[2] = {
#include "assets/actor_103800_model_04868_normals.inc"
};

u32 Actor03800_D04820[18] = {
#include "assets/actor_103800_model_04868_stream.inc"
};

TmdSource Actor03800_D04868 = {
    0,
    104,
    0,
    1,
    Actor03800_D047EC,
    Actor03800_D047F0,
    Actor03800_D04810,
    Actor03800_D047C8,
    Actor03800_D04820,
};

TmdBone Actor03800_D0488C[1] = {
#include "assets/actor_103800_model_0492C_skeleton.inc"
};

u32 Actor03800_D048B0[1] = {
#include "assets/actor_103800_model_0492C_partVerts.inc"
};

SVECTOR Actor03800_D048B4[4] = {
#include "assets/actor_103800_model_0492C_verts.inc"
};

SVECTOR Actor03800_D048D4[2] = {
#include "assets/actor_103800_model_0492C_normals.inc"
};

u32 Actor03800_D048E4[18] = {
#include "assets/actor_103800_model_0492C_stream.inc"
};

TmdSource Actor03800_D0492C = {
    0,
    104,
    0,
    1,
    Actor03800_D048B0,
    Actor03800_D048B4,
    Actor03800_D048D4,
    Actor03800_D0488C,
    Actor03800_D048E4,
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[7];
    AnimationPackedRotation        words[21];
} Actor03800PoseBank4950;

Actor03800PoseBank4950 Actor03800_D04950 = { .poses = {
#include "assets/actor_103800_animation_04B24_bank1.inc"
};

AnimationPackedRotation Actor03800_D049A4[24] = {
#include "assets/actor_103800_animation_04B24_bank4.inc"
};

GpAnimRec Actor03800_D04A04[69] = {
#include "assets/actor_103800_animation_04B24_records.inc"
};

u16 Actor03800_D04B18[6] = {
#include "assets/actor_103800_animation_04B24_indices.inc"
};

GpAnimSet Actor03800_D04B24 = {
    Actor03800_D04A04,
    Actor03800_D04B18,
    { NULL, Actor03800_D04950, NULL, NULL, Actor03800_D049A4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[12];
    AnimationPackedRotation        words[36];
} Actor03800PoseBank4B4C;

Actor03800PoseBank4B4C Actor03800_D04B4C = { .poses = {
#include "assets/actor_103800_animation_04D20_bank1.inc"
};

AnimationPackedRotation Actor03800_D04BDC[12] = {
#include "assets/actor_103800_animation_04D20_bank4.inc"
};

GpAnimRec Actor03800_D04C0C[66] = {
#include "assets/actor_103800_animation_04D20_records.inc"
};

u16 Actor03800_D04D14[6] = {
#include "assets/actor_103800_animation_04D20_indices.inc"
};

GpAnimSet Actor03800_D04D20 = {
    Actor03800_D04C0C,
    Actor03800_D04D14,
    { NULL, Actor03800_D04B4C, NULL, NULL, Actor03800_D04BDC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[17];
    AnimationPackedRotation        words[51];
} Actor03800PoseBank4D48;

Actor03800PoseBank4D48 Actor03800_D04D48 = { .poses = {
#include "assets/actor_103800_animation_04FE0_bank1.inc"
};

AnimationPackedRotation Actor03800_D04E14[43] = {
#include "assets/actor_103800_animation_04FE0_bank4.inc"
};

GpAnimRec Actor03800_D04EC0[69] = {
#include "assets/actor_103800_animation_04FE0_records.inc"
};

u16 Actor03800_D04FD4[6] = {
#include "assets/actor_103800_animation_04FE0_indices.inc"
};

GpAnimSet Actor03800_D04FE0 = {
    Actor03800_D04EC0,
    Actor03800_D04FD4,
    { NULL, Actor03800_D04D48, NULL, NULL, Actor03800_D04E14, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[31];
    AnimationPackedRotation        words[93];
} Actor03800PoseBank5008;

Actor03800PoseBank5008 Actor03800_D05008 = { .poses = {
#include "assets/actor_103800_animation_05534_bank1.inc"
};

AnimationPackedRotation Actor03800_D0517C[95] = {
#include "assets/actor_103800_animation_05534_bank4.inc"
};

GpAnimRec Actor03800_D052F8[140] = {
#include "assets/actor_103800_animation_05534_records.inc"
};

u16 Actor03800_D05528[6] = {
#include "assets/actor_103800_animation_05534_indices.inc"
};

GpAnimSet Actor03800_D05534 = {
    Actor03800_D052F8,
    Actor03800_D05528,
    { NULL, Actor03800_D05008, NULL, NULL, Actor03800_D0517C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[2];
    AnimationPackedRotation        words[6];
} Actor03800PoseBank555C;

Actor03800PoseBank555C Actor03800_D0555C = { .poses = {
#include "assets/actor_103800_animation_055F0_bank1.inc"
};

AnimationPackedRotation Actor03800_D05574[4] = {
#include "assets/actor_103800_animation_055F0_bank4.inc"
};

GpAnimRec Actor03800_D05584[24] = {
#include "assets/actor_103800_animation_055F0_records.inc"
};

u16 Actor03800_D055E4[6] = {
#include "assets/actor_103800_animation_055F0_indices.inc"
};

GpAnimSet Actor03800_D055F0 = {
    Actor03800_D05584,
    Actor03800_D055E4,
    { NULL, Actor03800_D0555C, NULL, NULL, Actor03800_D05574, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[7];
    AnimationPackedRotation        words[21];
} Actor03800PoseBank5618;

Actor03800PoseBank5618 Actor03800_D05618 = { .poses = {
#include "assets/actor_103800_animation_05734_bank1.inc"
};

AnimationPackedRotation Actor03800_D0566C[14] = {
#include "assets/actor_103800_animation_05734_bank4.inc"
};

GpAnimRec Actor03800_D056A4[33] = {
#include "assets/actor_103800_animation_05734_records.inc"
};

u16 Actor03800_D05728[6] = {
#include "assets/actor_103800_animation_05734_indices.inc"
};

GpAnimSet Actor03800_D05734 = {
    Actor03800_D056A4,
    Actor03800_D05728,
    { NULL, Actor03800_D05618, NULL, NULL, Actor03800_D0566C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[4];
    AnimationPackedRotation        words[12];
} Actor03800PoseBank575C;

Actor03800PoseBank575C Actor03800_D0575C = { .poses = {
#include "assets/actor_103800_animation_05824_bank1.inc"
};

AnimationPackedRotation Actor03800_D0578C[12] = {
#include "assets/actor_103800_animation_05824_bank4.inc"
};

GpAnimRec Actor03800_D057BC[23] = {
#include "assets/actor_103800_animation_05824_records.inc"
};

u16 Actor03800_D05818[6] = {
#include "assets/actor_103800_animation_05824_indices.inc"
};

GpAnimSet Actor03800_D05824 = {
    Actor03800_D057BC,
    Actor03800_D05818,
    { NULL, Actor03800_D0575C, NULL, NULL, Actor03800_D0578C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[7];
    AnimationPackedRotation        words[21];
} Actor03800PoseBank584C;

Actor03800PoseBank584C Actor03800_D0584C = { .poses = {
#include "assets/actor_103800_animation_05988_bank1.inc"
};

AnimationPackedRotation Actor03800_D058A0[19] = {
#include "assets/actor_103800_animation_05988_bank4.inc"
};

GpAnimRec Actor03800_D058EC[36] = {
#include "assets/actor_103800_animation_05988_records.inc"
};

u16 Actor03800_D0597C[6] = {
#include "assets/actor_103800_animation_05988_indices.inc"
};

GpAnimSet Actor03800_D05988 = {
    Actor03800_D058EC,
    Actor03800_D0597C,
    { NULL, Actor03800_D0584C, NULL, NULL, Actor03800_D058A0, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[12];
    AnimationPackedRotation        words[36];
} Actor03800PoseBank59B0;

Actor03800PoseBank59B0 Actor03800_D059B0 = { .poses = {
#include "assets/actor_103800_animation_05B84_bank1.inc"
};

AnimationPackedRotation Actor03800_D05A40[12] = {
#include "assets/actor_103800_animation_05B84_bank4.inc"
};

GpAnimRec Actor03800_D05A70[66] = {
#include "assets/actor_103800_animation_05B84_records.inc"
};

u16 Actor03800_D05B78[6] = {
#include "assets/actor_103800_animation_05B84_indices.inc"
};

GpAnimSet Actor03800_D05B84 = {
    Actor03800_D05A70,
    Actor03800_D05B78,
    { NULL, Actor03800_D059B0, NULL, NULL, Actor03800_D05A40, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[16];
    AnimationPackedRotation        words[48];
} Actor03800PoseBank5BAC;

Actor03800PoseBank5BAC Actor03800_D05BAC = { .poses = {
#include "assets/actor_103800_animation_05E14_bank1.inc"
};

AnimationPackedRotation Actor03800_D05C6C[39] = {
#include "assets/actor_103800_animation_05E14_bank4.inc"
};

GpAnimRec Actor03800_D05D08[64] = {
#include "assets/actor_103800_animation_05E14_records.inc"
};

u16 Actor03800_D05E08[6] = {
#include "assets/actor_103800_animation_05E14_indices.inc"
};

GpAnimSet Actor03800_D05E14 = {
    Actor03800_D05D08,
    Actor03800_D05E08,
    { NULL, Actor03800_D05BAC, NULL, NULL, Actor03800_D05C6C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[4];
    AnimationPackedRotation        words[12];
} Actor03800PoseBank5E3C;

Actor03800PoseBank5E3C Actor03800_D05E3C = { .poses = {
#include "assets/actor_103800_animation_05F18_bank1.inc"
};

AnimationPackedRotation Actor03800_D05E6C[12] = {
#include "assets/actor_103800_animation_05F18_bank4.inc"
};

GpAnimRec Actor03800_D05E9C[28] = {
#include "assets/actor_103800_animation_05F18_records.inc"
};

u16 Actor03800_D05F0C[6] = {
#include "assets/actor_103800_animation_05F18_indices.inc"
};

GpAnimSet Actor03800_D05F18 = {
    Actor03800_D05E9C,
    Actor03800_D05F0C,
    { NULL, Actor03800_D05E3C, NULL, NULL, Actor03800_D05E6C, NULL, NULL, NULL },
};

GpU16Pair Actor03800_D05F40[1] = {
    { 6, 7 },
};

GpPairSrcE Actor03800_D05F44 = { Actor03800_D05F40, 280, 15, 53, 1, 0, 10, 100, 0, 0 };

TaskDesc Actor03800_D05F54 = { 1, 96, Actor03800_Fn0315C, { .model = &Actor03800_D043A0 } };

GpAnimSet* Actor03800_D05F60[12] = {
    NULL,
    &Actor03800_D04B24,
    &Actor03800_D04D20,
    &Actor03800_D04FE0,
    &Actor03800_D05534,
    &Actor03800_D055F0,
    &Actor03800_D05734,
    &Actor03800_D05824,
    &Actor03800_D05988,
    &Actor03800_D05B84,
    &Actor03800_D05E14,
    &Actor03800_D05F18,
};

s16 Actor03800_D05F90[12] = {
    0,
    8,
    3,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
};

s16 Actor03800_D05FA8[2] = {
    2,
    1,
};

static void        Actor03800_Fn01150(Task* arg0);
static void        Actor03800_Fn012B4(Task* arg0);
static void        Actor03800_Fn01520(Task* arg0);
static void        Actor03800_Fn0166C(Task* arg0);
static void        Actor03800_Fn01948(Task* arg0);
static void        Actor03800_Fn01AD0(Task* arg0);
static void        Actor03800_Fn01C50(Task* arg0);
static void        Actor03800_Fn01EEC(Task* arg0);
static void        Actor03800_Fn02068(Task* arg0);
static void        Actor03800_Fn021E4(Task* arg0);
static void        Actor03800_Fn02584(Task* arg0);
static inline void _actor03800TickAnim(Task* task);

static void Actor03800_Fn000B8(GpEnemy* arg0, Task* arg1)
{
    GpObj*           obj;
    GpRec18*         records1;
    GpRec18*         records2;
    GpRec18*         records3;
    Actor103800Work* work;
    s32              i;
    TmdObject*       extra;

    extra = arg1->extra.tmd;
    work  = memCalloc(0x384, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work      = (TaskIdMap*)work;
    extra->lightMtx = &work->field_184;
    extra->flags    = 0;
    extra->colorMtx = &work->field_164;
    Gp_IncStateF0Ref(0);
    Actor03800_Fn003B8(arg1);
    arg0->field_4  = &work->field_344->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = &arg1->extra.tmd->coords[3];
    arg0->node.state.b.flags   = 0;
    arg0->bodyPos.vx           = 0;
    arg0->recs                 = work->field_1C4;
    arg0->bodyPos.vy           = 0;
    arg0->bodyPos.vz           = 0;
    arg0->param                = &Actor03800_D05F44;
    arg0->hp                   = (s16)Actor03800_D05F44.hpMax;
    work->field_2C4.coord      = &arg1->extra.tmd->coords[3];
    work->field_2C4.spawnArgLo = 0x200;
    work->field_2C4.spawnArgHi = 1;
    func_800B3F84(&work->anim, Actor03800_D05F60, extra, work->field_104, work->slots);
    for (i = 1; i < 6; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    work->field_1AC = arg1->extra.tmd->coords;
    records1        = work->field_1C4;
    work->field_1B6 = -0xFA;
    work->field_1B0 = records1;
    work->field_1B4 = 0;
    work->field_1B8 = 0;
    work->field_1BC = 0x30026;
    work->field_1C0 = 0xFA;
    work->field_1C2 = 1U;
    Gp_LinkObj(2, (GpObj*)work->field_1A4);
    Gp_InitRec18Table(records1, 3, 0);
    work->field_214 = arg1->extra.tmd->coords;
    records2        = work->field_22C;
    work->field_21E = -0x12C;
    work->field_218 = records2;
    work->field_21C = 0;
    work->field_220 = 0;
    work->field_224 = 0x30026;
    work->field_228 = 0x12C;
    work->field_22A = 1U;
    Gp_LinkObj(2, (GpObj*)work->field_20C);
    Gp_InitRec18Table(records2, 4, 0);
    switch (work->field_350) {
        case 0:
            work->field_1C2 |= 0x8000;
            work->field_22A |= 0x4200;
            break;
        case 1:
            work->field_1C2 |= 0x8000;
            work->field_22A &= ~0x4200;
            break;
        case 2:
            work->field_1C2 |= 0x8000;
            work->field_22A &= ~0x4200;
            break;
        case 3:
            work->field_1C2 &= ~0x8000;
            work->field_22A &= ~0x4200;
            break;
    }
    obj             = (GpObj*)work->field_28C;
    work->field_294 = arg1->extra.tmd->coords;
    records3        = work->field_2AC;
    work->field_29E = -0xFA;
    work->field_2A0 = 0xFA;
    work->field_2A8 = 0xC8;
    work->field_298 = records3;
    work->field_29C = 0;
    work->field_2A4 = 0;
    work->field_2AA = 1U;
    Gp_LinkObj(3, obj);
    Gp_InitRec18Table(records3, 1, 0);
    work->field_2AA = (u16)(work->field_2AA | 0x8000);
    arg1->state     = 1;
}

/// Applies the spawn variant (`GpAreaPlace::mode`) to the freshly allocated
/// work block: the tens digit picks the mode (`field_350`) and the units digit
/// of mode 0 the idle pose, seeding the look-around countdown from the LCG.
/// Modes 1 and 2 instead detach the model: the work block's own coordinate is
/// seeded from the model's, parented to `gGfxViewCoord` and published on
/// `field_344`, while the model's coordinate is reset to an identity rotation
/// at the origin and re-parented under it. `Gp_MulMatrix0`-style GTE column
/// products then turn the detached coordinate by 0x400 / 0x800 about X.
static void Actor03800_Fn003B8(Task* arg0)
{
    Actor103800Work* work;
    GpEnemy*         ctx;
    GpCoord*         src;
    OverlayMat*      mtx;
    OverlayMat*      srcmtx;
    OverlayMat*      mtx2;
    OverlayMat*      srcmtx2;
    SVECTOR          rot;
    MATRIX           mat;
    s16              mode;
    s16              kind;

    ctx  = (GpEnemy*)arg0->spawnArg2.pointer;
    work = (Actor103800Work*)arg0->work;
    src  = arg0->extra.tmd->coords;
    mode = ctx->place->mode / 10;

    work->field_350 = mode;
    switch (mode) {
        case 0:
            kind            = ctx->place->mode % 10;
            work->field_352 = kind;
            switch (kind) {
                case 0:
                    work->field_348 = 1;
                    work->field_37A = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_356 = (((u32)Gp_LcgState >> 0x10) & 0xFF) + 0x5A;
                    break;
                case 1:
                    work->field_348 = 2;
                    work->field_356 = 0;
                    work->field_37A = 1;
                    break;
            }
            work->field_366 = 0x80;
            work->field_372 = 0x80;
            work->field_344 = arg0->extra.tmd->coords;
            break;
        case 1:
            work->field_352 = 8;
            work->field_372 = -1;
            work->field_366 = 0;
            work->field_344 = &work->coord;
            work->field_36E = 1;
            work->field_37A = 0;
            work->field_2CC = src->coord;

            mtx                = (OverlayMat*)&work->coord.coord;
            mtx->ident.m00_m01 = 0x1000;
            mtx->ident.m02_m10 = 0;
            mtx->ident.m11_m12 = 0x1000;
            mtx->ident.m20_m21 = 0;
            mtx->ident.m22     = 0x1000;

            work->coord.sub        = &gGfxViewCoord;
            work->coord.coord      = src->coord;
            work->coord.coord.t[0] = src->coord.t[0];
            work->coord.coord.t[1] = src->coord.t[1];
            work->coord.coord.t[2] = src->coord.t[2];

            srcmtx                = (OverlayMat*)&src->coord;
            srcmtx->ident.m00_m01 = 0x1000;
            srcmtx->ident.m02_m10 = 0;
            srcmtx->ident.m11_m12 = 0x1000;
            srcmtx->ident.m20_m21 = 0;
            srcmtx->ident.m22     = 0x1000;

            src->sub        = &work->coord;
            src->coord.t[0] = 0;
            src->coord.t[1] = 0;
            src->coord.t[2] = 0;

            rot.vx = 0x400;
            rot.vy = 0;
            rot.vz = 0;
            RotMatrix(&rot, &mat);

            gte_SetRotMatrix(&work->coord.coord);
            gte_ldclmv(&mat.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][0]);
            gte_ldclmv(&mat.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][1]);
            gte_ldclmv(&mat.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][2]);
            break;
        case 2:
            work->field_352 = 9;
            work->field_372 = -1;
            work->field_366 = 0;
            work->field_344 = &work->coord;
            work->field_36E = 1;
            work->field_37A = 0;
            work->field_2CC = src->coord;

            mtx2                = (OverlayMat*)&work->coord.coord;
            mtx2->ident.m00_m01 = 0x1000;
            mtx2->ident.m02_m10 = 0;
            mtx2->ident.m11_m12 = 0x1000;
            mtx2->ident.m20_m21 = 0;
            mtx2->ident.m22     = 0x1000;

            work->coord.sub        = &gGfxViewCoord;
            work->coord.coord      = src->coord;
            work->coord.coord.t[0] = src->coord.t[0];
            work->coord.coord.t[1] = src->coord.t[1];
            work->coord.coord.t[2] = src->coord.t[2];

            srcmtx2                = (OverlayMat*)&src->coord;
            srcmtx2->ident.m00_m01 = 0x1000;
            srcmtx2->ident.m02_m10 = 0;
            srcmtx2->ident.m11_m12 = 0x1000;
            srcmtx2->ident.m20_m21 = 0;
            srcmtx2->ident.m22     = 0x1000;

            src->sub        = &work->coord;
            src->coord.t[0] = 0;
            src->coord.t[1] = 0;
            src->coord.t[2] = 0;

            rot.vx = 0x800;
            rot.vy = 0;
            rot.vz = 0;
            RotMatrix(&rot, &mat);

            gte_SetRotMatrix(&work->coord.coord);
            gte_ldclmv(&mat.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][0]);
            gte_ldclmv(&mat.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][1]);
            gte_ldclmv(&mat.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->coord.coord.m[0][2]);
            break;
        case 3:
            work->field_352 = 0xB;
            work->field_366 = 0;
            work->field_372 = -1;
            work->field_344 = arg0->extra.tmd->coords;
            break;
    }
}

static void Actor03800_Fn00974(Task* arg0)
{
    GpEnemy*         ctx;
    Actor103800Work* work;
    s16              damage;
    u16              remaining;
    u8               flags;

    ctx   = arg0->spawnArg2.pointer;
    flags = ctx->reactionFlags;
    work  = arg0->work;
    if (flags & 2) {
        if (work->field_350 == 0) {
            ctx->reactionFlags = flags & 0xFD;
            work->field_352    = 6;
            work->field_354    = 0;
            work->field_356    = 0;
            work->field_37E    = 1;
        } else if ((work->field_352 != 0xA) || (work->field_354 >= 4)) {
            work->field_352 = 0xA;
            work->field_354 = 0;
        }
    }
    if (ctx->reactionFlags & 0xC) {
        damage = Gp_TickObjFlag4(ctx);
        if (damage != 0) {
            func_800DA6E8(&ctx->node, (s32)damage, 0);
            remaining = ctx->hp - damage;
            ctx->hp   = remaining;
            if ((s16)remaining <= 0) {
                work->field_352 = 7;
            } else {
                work->field_352 = 5;
            }
            work->field_354 = 0;
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags &= 0xF3;
        }
    }
}

/// Keeps the deepest contact seen so far: when `depth` beats `best`, it
/// becomes the new `best`, and `frame->dir` the direction to push out along -
/// the offset in `frame->delta` normalised and carried into the collision
/// grid's frame.
#define _ACTOR03800_KEEP_DEEPEST(best, depth, frame)                                                 \
    do {                                                                                             \
        if ((best) < (depth)) {                                                                      \
            (best) = (depth);                                                                        \
            VectorNormal((VECTOR*)&(frame)->delta, &(frame)->normal);                                \
            ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &(frame)->normal, &(frame)->dir); \
        }                                                                                            \
    } while (0)

static void Actor03800_Fn00A98(Task* arg0)
{
    Actor103800Work*    work;
    ActorWallPushFrame* frame;
    GpEnemy*            ctx;
    GpCoord*            coord;
    GpCoord*            sourceCoord;
    s32                 push;
    s32                 reaction;
    s32                 result;
    s32                 i;
    s32                 depth;
    s32                 boundedDepth;
    s32                 dx;
    s32                 dy;
    s32                 dz;
    u32                 lastId;
    u32                 id;
    u32                 hitId;
    u32                 damage;

    push     = 0;
    reaction = 0;
    lastId   = 0;
    work     = arg0->work;
    SCRATCH_PUSH(ActorWallPushFrame);
    frame  = SCRATCH_HEAD(ActorWallPushFrame);
    coord  = work->field_344;
    ctx    = arg0->spawnArg2.pointer;
    result = func_800E0C10(work->field_22C, &frame->delta, 4, NULL);
    if (result != 0) {
        for (i = 0; i < 4; i++) {
            if ((work->field_22C[i].key & 0xFFFF0000) == 0x100000) {
                if (work->field_22C[i].at10.normal.vy >= -0xDDA) {
                    if (work->field_36E == 0) {
                        work->field_370 = 1;
                        break;
                    }
                } else {
                    work->field_374 = 1;
                }
            }
        }
        switch (result) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += frame->delta.vx.h.hi;
                coord->coord.t[1] += frame->delta.vy.h.hi;
                coord->coord.t[2] += frame->delta.vz.h.hi;
                break;
            case 2:
                coord->coord.t[0] = work->field_2EC;
                coord->coord.t[1] = work->field_2EE;
                coord->coord.t[2] = work->field_2F0;
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_22C);
    if (work->field_34E != 0) {
        if (--work->field_34E <= 0) {
            work->field_34E = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        id = work->field_1C4[i].key;
        switch (id >> 0x10) {
            case 0:
                break;
            case 2:
                if (work->field_34E == 0) {
                    sourceCoord       = Gp_ActorSlots[(id >> 7) & 1]->extra.tmd->coords;
                    frame->delta.vx.w = sourceCoord->coord.t[0] - coord->coord.t[0];
                    frame->delta.vy.w = sourceCoord->coord.t[1] - coord->coord.t[1];
                    frame->delta.vz.w = sourceCoord->coord.t[2] - coord->coord.t[2];
                    damage            = Gp_ComputeDamage(work->field_1C4[i].key, SquareRoot0((frame->delta.vx.w * frame->delta.vx.w) + (frame->delta.vy.w * frame->delta.vy.w) + (frame->delta.vz.w * frame->delta.vz.w)), 0, 0);
                    if (work->field_36E == 0) {
                        if (Gp_RollEnemyChance(ctx, work->field_1C4[i].key, 0) != 0) {
                            damage *= 4;
                            Gp_SpawnEff(0x6009C, coord, 0, NULL);
                        }
                    } else if (!(work->field_1C4[i].key & 0x8000) && (damage != 0)) {
                        damage *= 3;
                        Gp_SpawnEff(0x6009C, coord, 4, NULL);
                    }
                    func_800DA6E8(&ctx->node, damage, 0);
                    func_800E2C78(ctx, work->field_1C4[i].key, damage, 0);
                    ctx->hp -= damage;
                    if (ctx->hp <= 0) {
                        reaction = 2;
                    }
                    switch (Gp_GetIdParam0(work->field_1C4[i].key) & 0xFFFF) {
                        case 0:
                        default:
                            break;
                        case 3:
                            Gp_SetObjFlag4(ctx, work->field_1C4[i].key, 0);
                            break;
                        case 4:
                            if (ctx->hp > 0) {
                                if (work->field_36E == 0) {
                                    reaction = 1;
                                }
                            } else {
                                work->field_368 = 1;
                            }
                            break;
                        case 6:
                            if (ctx->hp <= 0) {
                                work->field_368 = 1;
                            } else if (work->field_36E == 0) {
                                reaction = 1;
                            }
                            break;
                        case 8:
                            if (work->field_36E == 0 && reaction == 0) {
                                Gp_SetObjFlag2(ctx, work->field_1C4[i].key, 0);
                            }
                            break;
                        case 1:
                        case 2:
                        case 5:
                        case 9:
                            if (work->field_36E == 0 && reaction == 0) {
                                reaction = 1;
                            }
                            break;
                    }
                    switch (reaction) {
                        case 0:
                            if (work->field_352 != 0xA && damage != 0) {
                                work->field_352 = 5;
                                work->field_354 = 0;
                            }
                            break;
                        case 1:
                            work->field_352 = 3;
                            work->field_354 = 0;
                            break;
                        case 2:
                            work->field_352 = 7;
                            work->field_354 = 0;
                            break;
                    }
                    hitId = work->field_1C4[i].key;
                    if (lastId != hitId) {
                        lastId = hitId;
                        func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, arg0->extra.tmd->coords + 3, NULL, &work->field_2C4);
                    }
                    result = Gp_GetIdParam2(work->field_1C4[i].key);
                    if (result > 0) {
                        work->field_34E = result;
                    }
                }
                break;
            case 1:
                dx                = coord->workm.t[0] - work->field_1C4[i].point.vx;
                frame->delta.vx.w = dx;
                dy                = coord->workm.t[1] - work->field_1C4[i].point.vy;
                frame->delta.vy.w = dy;
                dz                = coord->workm.t[2] - work->field_1C4[i].point.vz;
                frame->delta.vz.w = dz;
                depth             = work->field_1C4[i].depth - SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
                boundedDepth      = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                _ACTOR03800_KEEP_DEEPEST(push, depth, frame);
                break;
            case 3:
                dx                = coord->workm.t[0] - work->field_1C4[i].point.vx;
                frame->delta.vx.w = dx;
                dy                = coord->workm.t[1] - work->field_1C4[i].point.vy;
                frame->delta.vy.w = dy;
                dz                = coord->workm.t[2] - work->field_1C4[i].point.vz;
                frame->delta.vz.w = dz;
                depth             = work->field_1C4[i].depth - SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
                boundedDepth      = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                _ACTOR03800_KEEP_DEEPEST(push, depth, frame);
                break;
        }
    }
    if (push > 0 && work->field_350 == 0) {
        coord->coord.t[0] += (push * frame->dir.vx) >> 0xC;
        coord->coord.t[2] += (push * frame->dir.vz) >> 0xC;
    }
    Gp_ClearRec18Occupied(work->field_1C4);
    work->field_36C = 0;
    result          = Gp_CountRec18Hi(work->field_2AC, 0x10000);
    if (result != 0) {
        Gp_ArmStateF0(1);
        work->field_36C = 1;
        work->field_37A = 1;
        if ((work->field_350 == 0) && (work->field_36E == 0) && (work->field_352 != 0xC)) {
            work->field_352 = 0xC;
            work->field_354 = 0;
        }
    }
    Gp_ClearRec18Occupied(work->field_2AC);
    SCRATCH_POP(ActorWallPushFrame);
}

static void Actor03800_Fn01150(Task* arg0)
{
    Actor103800Work* work;
    s32              turn;

    work = arg0->work;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_354 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                turn            = ((u32)Gp_LcgState >> 16) & 0x3FF;
                if ((((u32)Gp_LcgState >> 16) & 0x400) == 0) {
                    turn = -turn;
                }
                work->field_348 = 2;
                work->field_36A = 1;
                work->field_364 = (work->field_362 + turn) & 0xFFF;
            }
            break;
        case 1:
            work->field_360 = 0x1E;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                if (work->field_37A == 0) {
                    work->field_354 = 0;
                    work->field_348 = 1;
                    work->field_36A = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_356 = (((u32)Gp_LcgState >> 16) & 0xFF) + 0x5A;
                } else {
                    work->field_352 = 1;
                    work->field_354 = 0;
                    if (work->field_36A == 0) {
                        work->field_36A = 1;
                    }
                }
            }
            break;
    }

    if (Gp_StateF0.prefix.bytes.field_2 & 5) {
        work->field_352 = 2;
        work->field_354 = 0;
        if (work->field_36A == 0) {
            work->field_36A = 1;
        }
        work->field_37A = 1;
    }
}

static void Actor03800_Fn012B4(Task* arg0)
{
    Actor103800Work* work;
    s32              turn;
    s32              delta;
    s32              turn2;

    work = arg0->work;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35E = 2;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            turn            = ((u32)Gp_LcgState >> 16) & 0x1FF;
            if ((((u32)Gp_LcgState >> 16) & 0x400) == 0) {
                turn = -turn;
            }
            delta = turn;
            if (work->field_370 != 0) {
                delta           = turn + 0x800;
                work->field_370 = 0;
            }
            work->field_348 = 2;
            work->field_354 = 1;
            work->field_364 = (work->field_362 + delta) & 0xFFF;
            turn            = 0; /* dead store: keeps `turn` cse-canonical over `delta` */
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_356 = (((u32)Gp_LcgState >> 16) & 0xF) + 0x19;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_358 = (((u32)Gp_LcgState >> 16) & 0x1F) + 0x1E;
            break;
        case 1:
            work->field_360 = 0x1E;
            work->field_35E = 2;
            work->field_356--;
            if (work->field_356 <= 0) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_356 = (((u32)Gp_LcgState >> 16) & 0xF) + 0x19;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                turn2           = ((u32)Gp_LcgState >> 16) & 0x1FF;
                if ((((u32)Gp_LcgState >> 16) & 0x400) == 0) {
                    turn2 = -turn2;
                }
                work->field_364 = (work->field_362 + turn2) & 0xFFF;
            }
            if (work->field_370 != 0) {
                if (work->field_35C < 0x1E) {
                    work->field_352 = 0;
                    work->field_354 = 0;
                    work->field_348 = 1;
                    work->field_36A = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_356 = (((u32)Gp_LcgState >> 16) & 0x1F) + 0x1E;
                } else {
                    work->field_352 = 0xC;
                    work->field_354 = 0;
                }
            }
            work->field_358--;
            if (work->field_358 <= 0) {
                work->field_352 = 0;
                work->field_354 = 0;
                work->field_348 = 1;
                work->field_36A = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_356 = (((u32)Gp_LcgState >> 16) & 0x1F) + 0x1E;
            }
            break;
    }

    if (Gp_StateF0.prefix.bytes.field_2 & 5) {
        work->field_352 = 2;
        work->field_354 = 0;
    }
}

static void Actor03800_Fn01520(Task* arg0)
{
    Actor103800Work* work;
    GpCoord*         coord;
    VECTOR           vec;

    work  = arg0->work;
    coord = work->field_344;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            vec.vx          = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec.vy          = 0;
            vec.vz          = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_364 = ratan2((s16)vec.vx, (s16)vec.vz) & 0xFFF;
            work->field_348 = 9;
            if (work->field_36A == 0) {
                work->field_36A = 1;
            }
            work->field_354 = 1;
            break;
        case 1:
            work->field_360 = 0x28;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                work->field_354 = 2;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_356 = (((u32)Gp_LcgState >> 16) & 0x1F) + 0x78;
            }
            break;
        case 2:
            work->field_360 = 0;
            work->field_35E = 2;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_352 = 1;
                work->field_354 = 0;
            }
            break;
    }
}

static void Actor03800_Fn0166C(Task* arg0)
{
    Actor03800MoveScratch* scratch;
    Actor103800Work*       work;
    GpEnemy*               ctx;
    GpCoord*               coord;
    s16                    state;
    s32                    snd;
    s32                    pan;
    s32                    pan2;

    scratch = (Actor03800MoveScratch*)SCRATCH_PUSH_BYTES(0x18);
    work    = arg0->work;
    ctx     = arg0->spawnArg2.pointer;
    state   = work->field_354;
    coord   = work->field_344;
    switch (state) {
        case 0:
            work->field_348  = 3;
            work->field_354  = 1;
            work->field_36A  = 0;
            work->field_36E  = 1;
            work->field_2AA &= 0x7FFF;
            snd              = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40260003;
            pan              = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            break;
        case 1:
            if ((u32)(work->field_34C - 2) < 12) {
                scratch->delta.vx = coord->coord.t[0] - Player_Status.coordMtx->t[0];
                scratch->delta.vy = coord->coord.t[1] - Player_Status.coordMtx->t[1];
                scratch->delta.vz = coord->coord.t[2] - Player_Status.coordMtx->t[2];
                VectorNormalS(&scratch->delta, &scratch->normal);
                coord->coord.t[0] += (scratch->normal.vx * 17) >> 9;
                coord->coord.t[2] += (scratch->normal.vz * 17) >> 9;
            } else {
                work->field_35C = 0;
                work->field_35E = 0;
            }
            if ((s16)work->field_34C == 12) {
                snd  = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40260002;
                pan2 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(snd, pan2, (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_34C >= 29) {
                work->field_354 = 2;
                work->field_37C = ((Actor03800_D05F44.hpMax - ctx->hp) * 100 / Actor03800_D05F44.hpMax) * 10 + 240;
            }
            break;
        case 2:
            work->field_37C--;
            if (work->field_37C <= 0) {
                work->field_352 = 4;
                work->field_354 = 0;
            }
            break;
    }
    SCRATCH_POP_BYTES(0x18);
}

static void Actor03800_Fn01948(Task* arg0)
{
    Actor103800Work* work = arg0->work;
    GpCoord*         coord;
    s16              state;
    s32              snd;
    s32              pan;

    state = work->field_354;
    coord = work->field_344;
    switch (state) {
        case 0:
            if (work->field_350 == 0) {
                if (work->field_36E == 0) {
                    work->field_348 = 0xB;
                    work->field_356 = 0xC;
                } else {
                    work->field_348 = 6;
                    work->field_356 = 0x13;
                }
            } else {
                work->field_348 = 7;
                work->field_356 = 0;
                work->field_352 = 0xA;
            }
            work->field_354 = 1;
            work->field_34A = 1;
            work->field_36A = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_360 = 0;
            snd             = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | 0x40260003;
            pan             = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            break;
        case 1:
            work->field_356 -= 1;
            if (work->field_356 > 0) {
                break;
            }
            if (work->field_36E == 0) {
                if (work->field_37E == 0) {
                    work->field_352 = 2;
                    work->field_354 = 0;
                } else {
                    work->field_352 = 6;
                    work->field_354 = 0;
                    work->field_356 = 0;
                }
                if (work->field_36A == 0) {
                    work->field_36A = 1;
                }
            } else {
                work->field_352 = 3;
                work->field_354 = 2;
            }
            break;
    }
}

static void Actor03800_Fn01AD0(Task* arg0)
{
    GpEnemy*         ctx;
    Actor103800Work* work;

    work            = arg0->work;
    ctx             = arg0->spawnArg2.pointer;
    work->field_360 = 0;
    work->field_35C = 0;
    work->field_35E = 0;
    work->field_356--;
    if (work->field_356 <= 0) {
        if (work->field_36E == 0) {
            work->field_348 = 0xB;
        } else {
            work->field_348 = 6;
        }
        work->field_34A = 1;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_356 = (((u32)Gp_LcgState >> 16) & 7) + 3;
    }
    if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
        work->field_37E = 0;
        if (work->field_350 == 0) {
            if (work->field_36E == 0) {
                work->field_352 = 2;
                work->field_354 = 0;
                if (work->field_36A == 0) {
                    work->field_36A = 1;
                }
            } else {
                work->field_352 = 3;
                work->field_354 = 2;
                work->field_37C = ((Actor03800_D05F44.hpMax - ctx->hp) * 100 / Actor03800_D05F44.hpMax) * 10 + 240;
            }
        } else {
            work->field_352 = 0xA;
            work->field_354 = 0;
        }
    }
}

static void Actor03800_Fn01C50(Task* arg0)
{
    SVECTOR          rotation;
    MATRIX           matrix;
    Actor103800Work* work;
    GpCoord*         coord;
    s16              state;

    work  = arg0->work;
    state = work->field_354;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if (work->field_350 == 0) {
                arg0->state     = 2;
                work->field_354 = 0;
                return;
            }
            work->field_366  = 0x80;
            work->field_372  = 0x80;
            work->field_354  = 1;
            work->field_22A |= 0x4200;
            return;
        case 1:
            if (work->field_374 != 0) {
                work->field_354 = 2;
                return;
            }
        default:
            return;
        case 2:
            work->field_348 = 5;
            work->field_354 = 3;
            return;
        case 3:
            rotation.vx = 0;
            rotation.vy = (u16)work->field_362;
            rotation.vz = 0;
            RotMatrix(&rotation, &matrix);
            gte_SetRotMatrix(&work->field_2CC);
            gte_ldclmv(&matrix.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][0]);
            gte_ldclmv(&matrix.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][1]);
            gte_ldclmv(&matrix.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][2]);
            coord->sub        = &gGfxViewCoord;
            coord->coord      = work->field_2CC;
            coord->coord.t[0] = work->coord.coord.t[0];
            coord->coord.t[1] = work->coord.coord.t[1];
            coord->coord.t[2] = work->coord.coord.t[2];
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            work->field_356 = 0xF;
            work->field_344 = coord;
            work->field_354 = 4;
            return;
        case 4:
            work->field_350 = 0;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_378 = 1;
                work->field_354 = 0;
                arg0->state     = 2;
            }
            break;
    }
}

/// Second copy of the idle "look around" tick; identical body to
/// `Actor03800_Fn02068`, which the overlay carries twice.
static void Actor03800_Fn01EEC(Task* arg0)
{
    Actor103800Work* work;
    s32              rand;
    s32              delta;

    work = arg0->work;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_356--;
            if (work->field_356 > 0) {
                break;
            }

            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_354 = 1;
            rand            = (u32)Gp_LcgState >> 16;
            delta           = rand & 0x3FF;
            if (!(rand & 0x400)) {
                delta = -delta;
            }

            work->field_348 = 2;
            work->field_36A = 1;
            work->field_364 = (work->field_362 + delta) & 0xFFF;
            break;

        case 1:
            work->field_360 = 0x1E;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_354 = 0;
                work->field_348 = 1;
                work->field_36A = 0;
                work->field_356 = ((u32)Gp_LcgState >> 16 & 0xFF) + 0x5A;
            }
            break;
    }

    if ((Gp_StateF0.prefix.bytes.field_2 & 5) || work->field_36C != 0) {
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_352 = 0xA;
        work->field_354 = 0;
        work->field_37A = 1;
        work->field_360 = 0;
        work->field_35C = 0;
        work->field_35E = 0;
        work->field_356 = ((u32)Gp_LcgState >> 16 & 0xF) + 0xF;
    }
}

/// Idle "look around" tick. State 0 counts `field_356` down and, on expiry,
/// picks a new facing `field_364` within +/-0x3FF of the current one; state 1
/// waits for the turn to finish and re-arms the countdown. Either way, an
/// active `Gp_StateF0.prefix.bytes.field_2` bit (1 or 4) or a non-zero `field_36C` aborts
/// back to state 0 with a short delay.
static void Actor03800_Fn02068(Task* arg0)
{
    Actor103800Work* work;
    s32              rand;
    s32              delta;

    work = arg0->work;

    switch (work->field_354) {
        case 0:
            work->field_360 = 0;
            work->field_35C = 0;
            work->field_35E = 0;
            work->field_356--;
            if (work->field_356 > 0) {
                break;
            }

            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_354 = 1;
            rand            = (u32)Gp_LcgState >> 16;
            delta           = rand & 0x3FF;
            if (!(rand & 0x400)) {
                delta = -delta;
            }

            work->field_348 = 2;
            work->field_36A = 1;
            work->field_364 = (work->field_362 + delta) & 0xFFF;
            break;

        case 1:
            work->field_360 = 0x1E;
            work->field_35C = 0;
            work->field_35E = 0;
            if (work->field_362 == work->field_364) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_354 = 0;
                work->field_348 = 1;
                work->field_36A = 0;
                work->field_356 = ((u32)Gp_LcgState >> 16 & 0xFF) + 0x5A;
            }
            break;
    }

    if ((Gp_StateF0.prefix.bytes.field_2 & 5) || work->field_36C != 0) {
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_352 = 0xA;
        work->field_354 = 0;
        work->field_37A = 1;
        work->field_360 = 0;
        work->field_35C = 0;
        work->field_35E = 0;
        work->field_356 = ((u32)Gp_LcgState >> 16 & 0xF) + 0xF;
    }
}

static void Actor03800_Fn021E4(Task* arg0)
{
    GpEnemy*               ctx;
    Actor103800Work*       work;
    GpCoord*               coord;
    Actor03800TurnScratch* scratch;
    s32                    sound;
    s32                    pan;

    scratch = (Actor03800TurnScratch*)SCRATCH_PUSH_BYTES(sizeof(*scratch));
    work    = arg0->work;
    coord   = arg0->extra.tmd->coords;
    ctx     = arg0->spawnArg2.pointer;
    switch (work->field_354) {
        case 0:
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_354 = 1;
            }
            break;
        case 1:
            work->field_366  = 0x100;
            work->field_372  = 0x80;
            work->field_22A |= 0x4200;
            if (work->field_374 != 0) {
                work->field_374  = 0;
                work->field_354  = 2;
                work->field_366  = 0x80;
                work->field_2AA &= 0x7FFF;
                sound            = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x40260002;
                pan              = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)gpGetObjDepth(coord));
            }
            break;
        case 2:
            work->field_348 = 8;
            work->field_354 = 3;
            break;
        case 3:
            scratch->rotation.vx = 0;
            scratch->rotation.vy = (u16)work->field_362;
            scratch->rotation.vz = 0;
            RotMatrix(&scratch->rotation, &scratch->matrix);
            gte_SetRotMatrix(&work->field_2CC);
            gte_ldclmv(&scratch->matrix.m[0][0]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][0]);
            gte_ldclmv(&scratch->matrix.m[0][1]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][1]);
            gte_ldclmv(&scratch->matrix.m[0][2]);
            gte_rtir();
            gte_stclmv(&work->field_2CC.m[0][2]);
            coord->coord      = work->field_2CC;
            coord->coord.t[0] = work->coord.coord.t[0];
            coord->coord.t[1] = work->coord.coord.t[1];
            coord->coord.t[2] = work->coord.coord.t[2];
            coord->sub        = &gGfxViewCoord;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            work->field_354 = 4;
            work->field_344 = coord;
            work->field_37C = ((Actor03800_D05F44.hpMax - ctx->hp) * 100 / Actor03800_D05F44.hpMax) * 10 + 240;
            break;
        case 4:
            work->field_350 = 0;
            work->field_356--;
            if (work->field_356 <= 0) {
                work->field_352 = 4;
                work->field_354 = 0;
            }
            break;
    }
    SCRATCH_POP_BYTES(0x28);
}

/// State 12 of `Actor03800_Fn032D8`: pick and hold a turn direction while the
/// actor is aiming at the player. State 0 chooses the side to turn towards -
/// clockwise (1) when the player is in front of the actor's local +Z axis,
/// anticlockwise (2) otherwise - and a pending `field_370` request forces the
/// clockwise side. States 1 and 2 drive `field_35C` by -/+0x7D while the yaw
/// error `field_34C` is inside 8..0x10 and hand over to state 3 once it grows
/// past 0x10; state 3 finishes the move and hands back to `field_352` 1.
static void Actor03800_Fn02584(Task* arg0)
{
    Actor103800Work* work;
    GpCoord*         coord;
    VECTOR           vec;

    work  = arg0->work;
    coord = work->field_344;

    switch (work->field_354) {
        case 0:
            vec.vx          = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec.vy          = 0;
            vec.vz          = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_348 = 0xA;
            if (work->field_370 != 0) {
                work->field_370 = 0;
                work->field_354 = 1;
            } else {
                work->field_354 =
                    ((vec.vx * coord->coord.m[0][2]) + (vec.vz * coord->coord.m[2][2]) > 0) ? 1 : 2;
            }
            work->field_2AA &= 0x7FFF;
            break;

        case 1:
            if (work->field_34C >= 8 && work->field_34C <= 0x10) {
                work->field_35C = -0x7D;
                break;
            }
            goto turn_done;

        case 2:
            if (work->field_34C >= 8 && work->field_34C <= 0x10) {
                work->field_35C = 0x7D;
                break;
            }
        turn_done:
            work->field_35C = 0;
            if ((s16)work->field_34C >= 0x11) {
                work->field_354 = 3;
            }
            break;

        case 3:
            if ((s16)work->field_34C >= 0x14) {
                work->field_352  = 1;
                work->field_354  = 0;
                work->field_372  = 0x80;
                work->field_2AA |= 0x8000;
            }
            break;
    }
}

static void Actor03800_Fn026F8(Task* arg0)
{
    Actor103800Work* work;
    GpCoord*         coord;
    SVECTOR*         rot;
    s32              ang;
    u16              want;
    s16              diff;
    s32              adiff;
    s32              step;
    s32              cur;
    s32              next;
    s32              wrapStep;

    rot   = (SVECTOR*)SCRATCH_PUSH_BYTES(8);
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_364;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_362 = ang;
    if (adiff < 0x800) {
        step = work->field_360;
        if (step >= adiff) {
            work->field_362 = want;
        } else {
            next = work->field_362;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_362 = next;
        }
    } else {
        step = work->field_360;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_362 = work->field_364;
        goto done;
    turn:
        wrapStep = work->field_360;
        cur      = work->field_362;
        if (diff > 0) {
            work->field_362 = cur - wrapStep;
        } else {
            work->field_362 = cur + wrapStep;
        }
    }
done:
    rot->vx = 0;
    rot->vy = work->field_362;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    SCRATCH_POP_BYTES(8);
}

static void Actor03800_Fn02848(Task* arg0)
{
    Actor103800Work* work;
    GpCoord*         coord;
    s16              next;
    s16              speed;
    u16              value;
    s32              scale;
    Actor103800Work* work2;

    work  = arg0->work;
    coord = work->field_344;
    work2 = work;
    if (work->field_35E != 0) {
        next            = (u16)work->field_35C + (u16)work->field_35E;
        work->field_35C = next;
        if (next >= 0x33) {
            work->field_35C = 0x32;
        }
    }
    speed = work2->field_35C;
    if (speed > 0) {
        scale = speed * 0x190;
        value = Actor03800_D05F40[0].field_0 + ((Actor03800_D05F40[0].field_0 * scale) / 10000);
    } else {
        value = Actor03800_D05F40[0].field_0;
    }
    work2->field_2A4   = (((s16)value | (Actor03800_D05F40[0].field_2 << 0xC)) & 0xFFFF) | 0x40000;
    work->field_2EC    = (s16)coord->coord.t[0];
    work->field_2EE    = (s16)coord->coord.t[1];
    work->field_2F0    = (s16)coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_35C) >> 0xC;
    coord->coord.t[1] += work->field_366;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_35C) >> 0xC;
}

/// Switches the work's animation id, resetting the slots to the blend value the
/// table gives for the new id; otherwise ticks every slot one frame.
static inline void _actor03800TickAnim(Task* task)
{
    Actor103800Work* work;
    s32              i;
    s32              value;

    work = task->work;
    if ((s16)work->field_348 != work->field_34A) {
        work->field_34A = work->field_348;
        work->field_34C = 0;
        value           = Actor03800_D05F90[(s16)work->field_348];
        for (i = 1; i < 6; i++) {
            func_800B4114(&work->anim, i, (s16)work->field_348, 0, value);
        }
    } else {
        work->field_34C++;
        for (i = 1; i < 6; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
    }
}

static void Actor03800_Fn02998(GpEnemy* arg0, Task* arg1)
{
    Actor103800Work* work;
    TmdObject*       obj;
    GpCoord*         coord;
    GpCoord*         c;
    VECTOR           vec;
    s32              state;
    s16              st;
    s16              phase;
    s16              anim;
    s32              snd;
    s32              pan;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    state = Gp_StateF0.field_4;
    coord = work->field_344;
    if (state == 1) {
        goto case1;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
case2:
    obj->flags = 0x80;
    return;
default_body:
    st = work->field_354;
    if (st == 1) {
        goto dying;
    }
    if (st >= 2) {
        goto ge2;
    }
    if (st == 0) {
        goto death;
    }
    return;
ge2:
    if (st == 2) {
        goto destroy;
    }
    if (st == 3) {
        goto case3;
    }
    return;
death:
    if (work->field_378 == 0) {
        anim = 1;
        if (work->field_36E != 0) {
            anim = 5;
        }
        work->field_348 = anim;
    }
    work->field_356 = 0;
    work->field_35A = 0x1000;
    work->field_2CC = coord->coord;
    arg0->recs      = 0;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj((GpObj*)work->field_1A4);
    Gp_UnlinkObj((GpObj*)work->field_20C);
    Gp_UnlinkObj((GpObj*)work->field_28C);
    Gp_SetLightMode(arg0, 1);
    Gp_ReleaseStateF0Add(arg1, 0x26);
    work->field_354 = 1;
    if (work->field_368 != 0) {
        obj->flags      = 0x80;
        work->field_354 = 3;
    }
    _actor03800TickAnim(arg1);
    c      = ((Actor103800Work*)arg1->work)->field_344;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    snd = ((arg0->placeKey >> 12) << 8) | 0x40260004;
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
    return;
dying:
    Actor03800_Fn037E0(arg1);
    phase           = work->field_356 + 1;
    work->field_356 = phase;
    if (phase == 10) {
        obj->flags = 2;
    }
    if (work->field_356 == 15) {
        Gp_SpawnEff(0x600A5, coord, 2, NULL);
    }
    if (work->field_356 >= 0x3C) {
        work->field_354 = 2;
        obj->flags      = 0x80;
    }
    _actor03800TickAnim(arg1);
    c      = ((Actor103800Work*)arg1->work)->field_344;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
destroy:
    Gp_DestroyEnemy(arg0, arg1);
    return;
case3:
    if (work->field_368 == 0) {
        goto timer;
    }
    if (work->field_368 < 2) {
        goto inc368;
    }
    work->field_368 = 0;
    Tmd_FreeBuffers(obj);
    obj->flags |= 4;
    Actor03800_Fn02E50(arg1);
    goto timer;
inc368:
    work->field_368++;
timer:
    phase           = work->field_356 + 1;
    work->field_356 = phase;
    if (phase < 0x3C) {
        return;
    }
    work->field_354 = 2;
}

static void Actor03800_Fn02E50(Task* actor)
{
    s16  variants[4];
    s16* out;
    s32  i;
    s32  count;
    u32  first;
    s32  second;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    first       = (u16)((Gp_LcgState >> 16) % 5);
    Actor03800_Fn03008(actor, first);
    if (Gp_StateF0.field_6 < 2) {
        count = Actor03800_D05FA8[Gp_StateF0.field_6];
        out   = variants;
        for (i = 0; i < 5; i++) {
            if (i != first) {
                *out++ = i;
            }
        }
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        second      = (Gp_LcgState >> 16) & 3;
        Actor03800_Fn03008(actor, variants[second]);
        if (--count > 0) {
            s16* next = variants;

            for (i = 0; i < 5; i++) {
                if (i != first && i != second) {
                    *next++ = i;
                }
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Actor03800_Fn03008(actor, variants[(u16)((Gp_LcgState >> 16) % 3)]);
        }
    }
}

static void Actor03800_Fn03008(Task* actor, u32 variant)
{
    GpAreaKey      key;
    GpAreaKey*     sessionKey;
    u8             areaByte0;
    GpAreaVariant* rec;
    GpAreaPlace*   entry;
    GpEffWork*     eff;
    TmdObject*     model;
    s32            idx;
    u32            raw;

    switch (variant) {
        case 0:
            D_80067704[0] = &Actor03800_D0459C;
            break;
        case 1:
            D_80067704[0] = &Actor03800_D046A0;
            break;
        case 2:
            D_80067704[0] = &Actor03800_D047A4;
            break;
        case 3:
            D_80067704[0] = &Actor03800_D04868;
            break;
        case 4:
            D_80067704[0] = &Actor03800_D0492C;
            break;
    }
    eff = Gp_SpawnEff(0x40007, actor->extra.tmd->coords + 3, 0x100, NULL);
    if (eff == NULL) {
        return;
    }
    sessionKey = &gGameSession->at4.loc;
    raw        = ((GpEnemy*)actor->spawnArg2.pointer)->placeKey;
    model      = eff->task->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    idx        = raw >> 12;
    key.view   = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    rec = Gp_GetNestedAreaRec(&key);

    entry        = gpAreaPlaceAt(rec->field_0, idx);
    model->tpage = entry->tpage;
    model->clut  = entry->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

static void Actor03800_Fn0315C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor03800_D00004;
    sp.funcs[arg0->state](((GpEnemy*)arg0->spawnArg2.pointer), arg0);
}

static void Actor03800_Fn031B8(GpEnemy* arg0, Task* arg1)
{
    Actor103800Work* work;
    s32              state;
    s32              one;

    state = Gp_StateF0.field_4;
    one   = 1;
    work  = arg1->work;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    arg1->extra.tmd->flags   = 0;
    arg0->node.state.b.flags = 0;
    goto default_body;
case2:
    arg1->extra.tmd->flags   = 0x80;
    arg0->node.state.b.flags = one;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        Actor03800_Fn00974(arg1);
    }
    Actor03800_Fn00A98(arg1);
    Actor03800_Fn032D8(arg1);
    if (work->field_360 != 0) {
        Actor03800_Fn026F8(arg1);
    }
    Actor03800_Fn02848(arg1);
    if (work->field_36A != 0) {
        Actor03800_Fn03594(arg1);
    }
    Actor03800_Fn03628(arg1);
    work->field_344->flg = 0;
    Gp_UpdateCoord(work->field_344);
case1:
    Actor03800_Fn036EC(arg1);
    Actor03800_Fn03744(arg1);
}

static void Actor03800_Fn032D8(Task* arg0)
{
    Actor103800Work* work;
    s16              state;
    s16              mag;

    work  = arg0->work;
    state = work->field_352;
    switch (state) {
        case 0:
            Actor03800_Fn01150(arg0);
            break;
        case 1:
            Actor03800_Fn012B4(arg0);
            break;
        case 2:
            Actor03800_Fn01520(arg0);
            break;
        case 3:
            Actor03800_Fn0166C(arg0);
            break;
        case 4:
            Actor03800_Fn03420(arg0);
            break;
        case 5:
            Actor03800_Fn01948(arg0);
            break;
        case 6:
            Actor03800_Fn01AD0(arg0);
            break;
        case 7:
            Actor03800_Fn01C50(arg0);
            break;
        case 8:
            Actor03800_Fn01EEC(arg0);
            break;
        case 9:
            Actor03800_Fn02068(arg0);
            break;
        case 10:
            Actor03800_Fn021E4(arg0);
            break;
        case 11:
            Actor03800_Fn034B0(arg0);
            break;
        case 12:
            Actor03800_Fn02584(arg0);
            break;
    }
    if (work->field_36E == 0) {
        work->field_21E = -0xFA;
        mag             = 0xFA;
    } else {
        work->field_21E = -0x15E;
        mag             = 0x15E;
    }
    work->field_228 = mag;
}

static void Actor03800_Fn03420(Task* arg0)
{
    Actor103800Work* work = arg0->work;
    s32              state;

    state = work->field_354;
    switch (state) {
        case 0:
            work->field_348 = 4;
            work->field_354 = 1;
            break;
        case 1:
            if ((s16)work->field_34C == 0x1E) {
                work->field_36E = 0;
            }
            if ((s16)work->field_34C >= 0x3A) {
                work->field_352 = 2;
                work->field_354 = 0;
                if (work->field_36A == 0) {
                    work->field_36A = state;
                }
                work->field_2AA |= 0x8000;
            }
            break;
    }
}

static void Actor03800_Fn034B0(Task* arg0)
{
    TmdObject*       obj;
    GpEnemy*         ctx;
    Actor103800Work* work;

    work = arg0->work;
    obj  = arg0->extra.tmd;
    ctx  = arg0->spawnArg2.pointer;
    switch (Gp_StateF0.field_20) {
        case 0:
            obj->flags              = 0x84;
            ctx->node.state.b.flags = 1;
            return;
        case 1:
            obj->flags       = 0;
            work->field_1C2 |= 0x8000;
            work->field_22A |= 0x4200;
            work->field_2AA |= 0x8000;
            Gp_ArmStateF0(1);
            work->field_366 = 0x80;
            work->field_356 = 0x5A;
            work->field_350 = 0;
            work->field_372 = 0x80;
            return;
        case 2:
            if (--work->field_356 <= 0) {
                work->field_352 = 1;
                work->field_354 = 0;
            }
            return;
    }
}

static void Actor03800_Fn03594(Task* arg0)
{
    Actor103800Work* work;
    GpCoord*         coord;
    s32              soundId;
    s32              pan;

    work  = arg0->work;
    coord = work->field_344;
    if (--work->field_36A <= 0) {
        work->field_36A = 0xC;
        soundId         = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x40260001;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(coord));
    }
}

static void Actor03800_Fn03628(Task* arg0)
{
    _actor03800TickAnim(arg0);
}

static void Actor03800_Fn036EC(Task* arg0)
{
    GpCoord* coord;
    VECTOR   vec;

    coord  = ((Actor103800Work*)arg0->work)->field_344;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

static void Actor03800_Fn03744(Task* arg0)
{
    Actor103800Work* work;
    GpCoord*         coord;
    VECTOR3          vec;
    s16              hit;

    work  = arg0->work;
    coord = work->field_344;
    if (work->field_350 == 0) {
        vec.vx = coord->workm.t[0];
        vec.vy = coord->workm.t[1];
        vec.vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(&vec, 0x1F4, work->field_372);
        return;
    }
    hit = func_800EA1A8(MATRIX_TRANS(&coord->workm), &vec);
    if (hit != 0) {
        Gp_DrawEffGroundQuad(&vec, 0x200, func_800EA318(0x200, 0x80, hit));
    }
}

static void Actor03800_Fn037E0(Task* arg0)
{
    Actor103800Work*   work;
    GpCoord*           coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;

    work                            = arg0->work;
    head                            = SCRATCH_HEAD(ActorScaleScratch);
    scratch                         = head - 1;
    SCRATCH_HEAD(ActorScaleScratch) = scratch;
    coord                           = work->field_344;
    if (work->field_35A >= 0x201) {
        work->field_35A = (u16)work->field_35A - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_35A;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_2CC;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg = 0;
    SCRATCH_POP(ActorScaleScratch);
}
