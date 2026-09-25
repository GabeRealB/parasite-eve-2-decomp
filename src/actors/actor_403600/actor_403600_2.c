#include "common.h"

#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "psyq/libgs.h"
#include "psyq/inline_c.h"
#include "gte.h"

#include "actors/actor.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "actors/actor_403600.h"

typedef struct Actor403600DamageRow {
    /* 0x0 */ s16 threshold;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 pad_6;
} Actor403600DamageRow;
STATIC_ASSERT_SIZEOF(Actor403600DamageRow, 0x8);

typedef struct Actor403600DamageScratch {
    /* 0x00 */ u8             pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ u8             pad_30[0x18];
} Actor403600DamageScratch;
STATIC_ASSERT_SIZEOF(Actor403600DamageScratch, 0x48);

typedef struct Actor403600MotionState {
    /* 0x00 */ s16  field_0[0x40];
    /* 0x80 */ s32  field_80;
    /* 0x84 */ s32  field_84;
    /* 0x88 */ s32  field_88;
    /* 0x8C */ s16  field_8C;
    /* 0x8E */ s16  field_8E;
    /* 0x90 */ byte pad_90[0x50];
    /* 0xE0 */ s32  field_E0;
} Actor403600MotionState;
STATIC_ASSERT_SIZEOF(Actor403600MotionState, 0xE4);

typedef struct Actor403600Pattern {
    u8 values[9];
} __attribute__((packed)) Actor403600Pattern;
STATIC_ASSERT_SIZEOF(Actor403600Pattern, 9);

/// Coordinate frame with a word view of its rotation matrix.
typedef union Actor403600ViewFrame {
    GpCoord view;
    struct {
        /* 0x00 */ u32        flags;
        /* 0x04 */ GpMtxWords words;
    } matrix;
} Actor403600ViewFrame;
STATIC_ASSERT_SIZEOF(Actor403600ViewFrame, 0x50);

/// 0x24-byte scratch block used to hold seven planar distances while selecting
/// the nearest point from D_actor_403600_801605F4.
typedef struct Actor403600DistanceScratch {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 z;
    /* 0x08 */ s32 distances[7];
} Actor403600DistanceScratch;
STATIC_ASSERT_SIZEOF(Actor403600DistanceScratch, 0x24);

typedef struct Actor403600TurnMatrix {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s16 field_10;
    /* 0x12 */ s16 pad_12;
    /* 0x14 */ s32 field_14;
    /* 0x18 */ s32 field_18;
    /* 0x1C */ s32 field_1C;
} Actor403600TurnMatrix;
STATIC_ASSERT_SIZEOF(Actor403600TurnMatrix, 0x20);

typedef struct Actor403600TurnScratch {
    /* 0x00 */ u16                   angles[4];
    /* 0x08 */ s16                   vector[4];
    /* 0x10 */ Actor403600TurnMatrix matrix;
} Actor403600TurnScratch;
STATIC_ASSERT_SIZEOF(Actor403600TurnScratch, 0x30);

/// Scratch vector, rotation matrix and yaw used by func_actor_403600_8013C864.
typedef struct Actor403600TargetScratch {
    /* 0x00 */ SVECTOR               vector;
    /* 0x08 */ Actor403600TurnMatrix matrix;
    /* 0x28 */ s32                   angle;
} Actor403600TargetScratch;
STATIC_ASSERT_SIZEOF(Actor403600TargetScratch, 0x2C);

/// Scratch block the bearing helpers work in: the vector from one coordinate
/// to another, turned into the second one's frame, and the rotation it is
/// turned by.
/// The helpers reach it as `head[-1]`, the block just below the scratch head,
/// before and after they claim it.
typedef struct Actor403600BearingScratch {
    byte    pad_0[0x20];
    VECTOR  toPlayer; // from the coordinate to the player, in world space
    byte    pad_30[0xC];
    SVECTOR rel;      // between the two coordinates, then in the target's frame
    byte    pad_44[0x18];
    MATRIX  rot;      // the target's rotation, transposed
} Actor403600BearingScratch;
STATIC_ASSERT_SIZEOF(Actor403600BearingScratch, 0x7C);

typedef struct Actor403600Point {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 pad_2;
    /* 0x4 */ s16 z;
    /* 0x6 */ s16 pad_6;
} Actor403600Point;
STATIC_ASSERT_SIZEOF(Actor403600Point, 0x8);

/// The two `s16` halves of `D_actor_403600_801606B8`, zeroed together when the
/// actor is spawned. The indexed view records the two most recent action choices.
typedef union {
    struct {
        /* 0x0 */ s16 field_0;
        /* 0x2 */ s16 field_2;
    } fields;
    u16 values[2];
} Actor403600Pair;
STATIC_ASSERT_SIZEOF(Actor403600Pair, 0x4);

typedef struct Actor403600TargetPair {
    /* 0x00 */ s16 x0;
    /* 0x02 */ s16 y0;
    /* 0x04 */ s16 z0;
    /* 0x06 */ s16 pad_6;
    /* 0x08 */ s16 x1;
    /* 0x0A */ s16 y1;
    /* 0x0C */ s16 z1;
    /* 0x0E */ s16 pad_E;
} Actor403600TargetPair;
STATIC_ASSERT_SIZEOF(Actor403600TargetPair, 0x10);

extern Actor403600TargetPair D_actor_403600_8016063C;
extern Actor403600TargetPair D_actor_403600_8016064C;
extern u8                    D_actor_403600_80160694;
extern u8                    D_actor_403600_80160695;

extern SVECTOR    D_actor_403600_801605D4;
extern SVECTOR    D_actor_403600_801605DC;
extern SVECTOR    D_actor_403600_801605E4;
extern SVECTOR    D_actor_403600_801605EC;
extern TaskDesc   D_actor_403600_80160514;
extern Task*      D_actor_403600_801606B0;
extern GpXformArg D_actor_403600_801606E0;
extern GpU16Pair  D_8016A408[];
extern u16        D_8016AEF8[];
extern u16        D_8016E450;
extern SVECTOR    D_actor_403600_8016065C;
extern SVECTOR    D_actor_403600_80160664;
extern u16        D_actor_403600_80150EA4;
extern u16        D_actor_403600_80150EAC;
extern s32        D_actor_403600_80160700[9];

void func_actor_403600_80138EF8(struct GpEnemy* enemy, Task* task);
void func_actor_403600_8013938C(GpEnemy* arg0, Task* arg1);
void func_8004BFF8(s32 angle, MATRIX* matrix);
void func_actor_403600_8013C864(Task* arg0);
void func_actor_403600_80138C9C(Actor403600MotionState* arg0);
u8*  func_actor_403600_80138DCC(Task* arg0);
void func_actor_403600_8013CCEC(Task* arg0, s32 arg1);
s32  func_actor_403600_8013D9A8(Task* arg0);
void func_actor_403600_8013DAF4(Task* arg0, s32 arg1);
s32  func_actor_403600_8013DDF4(Task* arg0, s16 arg1);
s32  func_actor_403600_8013DFE0(Task* arg0);
void func_actor_403600_8013E470(GpCoord* arg0, s32* arg1, s32* arg2);
s16  func_actor_403600_8013E66C(GpCoord* arg0);
s32  func_actor_403600_8013E7D4(s32 arg0, s32 arg1);
void func_actor_403600_8013EA04(Task* arg0);
void func_actor_403600_8013F608(Task* arg0);
void func_actor_403600_801417A8(Task* arg0, s32 arg1);
s32  func_actor_403600_80141840(Task* arg0);
void func_actor_403600_80141B60(Task* arg0);
void D_80181A48(Task* arg0);
s32  func_actor_403600_801406A4(Task* arg0, s32 arg1, GpCmdArg* arg2);
void func_actor_403600_80140B4C(struct GpEnemy* arg0, Task* arg1);
void func_actor_403600_80141F58(GpCoord* arg0, s32 arg1);

/* `Gp_LcgState`, read through a register holding only the upper half of its
 * address, which the damage handler keeps live across its blocks; naming the
 * global rebuilds the address at every use instead. */
#define ACTOR403600_RNG_VALUE(base) (*(u32*)((u8*)(base) + 0xF60))

#define ACTOR_COPY_MATRIX_COLUMN_TO_SV(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, %2(%0);"                                 \
        "lhu $13, %3(%0);"                                 \
        "lhu $14, %4(%0);"                                 \
        "sh $12, 0(%1);"                                   \
        "sh $13, 2(%1);"                                   \
        "sh $14, 4(%1)"                                    \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

#define actor_403600_d_x(out, hi) \
    __asm__("lh %0, %%lo(D_actor_403600_801605D4)(%1)" : "=r"(out) : "r"(hi))
#define actor_403600_load_scratch_head(out) \
    __asm__ volatile("lui %0, 0x1F80; lw %0, 0x3FC(%0)" : "=r"(out))
#define actor_403600_load_scratch_head_nop(out) \
    __asm__ volatile("lui %0, 0x1F80; lw %0, 0x3FC(%0); nop" : "=r"(out))
#define actor_403600_store_scratch_head(value) \
    __asm__ volatile("lui $1, 0x1F80; sw %0, 0x3FC($1)" : : "r"(value) : "$1", "memory")
#define actor_403600_restore_scratch_head(value) \
    __asm__("addiu %0, %0, 24; lui $1, 0x1F80; sw %0, 0x3FC($1)" : "+r"(value) : : "$1")
#define actor_403600_color_tail(z, a3, block, work, zero)                                   \
    __asm__ volatile("lw %0, 1272(%3); addu %1, %4, $zero; lui $1, 0x1F80; sw %2, 1020($1)" \
                     : "=r"(z), "=r"(a3), "+r"(block) : "r"(work), "r"(zero) : "$1")
#define actor_403600_color_tail_in_place(work, a3, block, zero)                             \
    __asm__ volatile("lw %0, 1272(%0); addu %1, %3, $zero; lui $1, 0x1F80; sw %2, 1020($1)" \
                     : "+r"(work), "=r"(a3), "+r"(block) : "r"(zero) : "$1")
#define actor_403600_cutscene_color_tail(work, a3, block, zero)                             \
    __asm__ volatile("lw %0, 1272(%0); addu %1, %3, $zero; lui $1, 0x1F80; sw %2, 1020($1)" \
                     : "=r"(work), "=r"(a3), "=r"(block) : "r"(zero), "0"(work), "2"(block) : "$1")
#define actor_403600_rcos(angle)                                \
    ({                                                          \
        s32 result = rcos(angle);                               \
        __asm__ volatile("sll $s0, $s0, 16; sra $s0, $s0, 16"); \
        result;                                                 \
    })
#define actor_403600_rot_setup(object, rot, matrix, coord, actor, work, head)             \
    __asm__ volatile("lw %0, 44(%4); addiu %1, %5, 1792; addiu %2, %6, -32; lw %3, 8(%0)" \
                     : "=r"(object), "=r"(rot), "=r"(matrix), "=r"(coord)                 \
                     : "r"(actor), "r"(work), "r"(head))
#define actor_403600_coord_advance(head, work) \
    __asm__ volatile("sw $zero, 1208(%1); addiu %0, %0, 32" : "+r"(head) : "r"(work))

extern TaskDesc D_80162E98;
/// Models effect 0x80005 spawns, set in `D_800626EC[5].arg.model`.
extern TmdSource D_80187B10;
extern TmdSource D_80187E14;
extern TmdSource D_80188264;
extern TmdSource D_8018864C;
extern Task*     D_actor_403600_801606B4;

extern TaskDesc             D_8016E468;
extern u8                   D_actor_403600_80150ED4;
extern TaskDesc             D_actor_403600_801421A0;
extern s32                  D_actor_403600_8016056C;
extern GpAnimArg            D_actor_403600_80160568;
extern s32                  D_actor_403600_8016057C[];
extern Actor403600Point     D_actor_403600_801605F4[];
extern GpU16Pair            D_actor_403600_801606A4;
extern Task*                D_actor_403600_801606A8;
extern Actor403600DamageRow D_actor_403600_8016066C[];
extern GpU16Pair            D_actor_403600_80150EB0;
extern GpPairSrcE           D_actor_403600_80150EC8;
extern GpPairSrcE           D_actor_403600_80150ED8;
extern GpU16Pair            D_actor_403600_80150E9C;
extern s32                  D_actor_403600_80160504[4];
extern Actor403600Pair      D_actor_403600_801606B8;
extern Task*                D_actor_403600_801606AC;
extern s32                  D_actor_403600_801606BC;

void func_actor_403600_80141598(Task* arg0);
void func_actor_403600_8014174C(Task* arg0);
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_actor_403600_80132E40(Task* arg0, Actor403600Work* arg1, Actor403600FxWork* arg2);
void func_80181940(Task* arg0);
void func_actor_403600_8013A444(Task* arg0);
void func_actor_403600_801419E8(Task* arg0);
void func_actor_403600_8013955C(Task* arg0);
void func_actor_403600_801396F8(Task* arg0);
void func_actor_403600_8013D15C(Task* arg0);
void func_actor_403600_80141C7C(Task* arg0, s32 arg1);
void func_actor_403600_8013DC7C(Task* arg0);
void func_actor_403600_8013F0C0(Task* arg0);
void func_actor_403600_801411D4(Task* arg0, s32 arg1);
void func_actor_403600_801412D0(GpEnemy* arg0, Task* arg1);
void func_actor_403600_80141338(Task* arg0);
void func_actor_403600_801414FC(Task* arg0);
void func_actor_403600_8014161C(Task* arg0);
void func_actor_403600_80141954(s32 arg0);
void func_actor_403600_80141A34(Task* arg0);
void func_actor_403600_80141B24(Task* arg0);
void func_actor_403600_80141C3C(Task* arg0);

#define ACTOR_COPY_SV_TO_MATRIX_COLUMN(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, 0(%0);"                                  \
        "lhu $13, 2(%0);"                                  \
        "lhu $14, 4(%0);"                                  \
        "sh $12, %2(%1);"                                  \
        "sh $13, %3(%1);"                                  \
        "sh $14, %4(%1)"                                   \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

void func_actor_403600_801400BC(Task* arg0);
void func_actor_403600_80141F28(Task* arg0);
void func_actor_403600_80140488(GpEnemy* arg0, Task* arg1);
void func_actor_403600_80141D30(GpEnemy* arg0, Task* arg1);
void func_actor_403600_80141E78(GpEnemy* arg0, Task* arg1);

void func_actor_403600_80138C34(Task* arg0)
{
    Task* parent;

    parent = arg0->parent;
    func_actor_403600_80132E40(parent, parent->parent->work, parent->work);
}

void func_actor_403600_80138C68(Task* arg0)
{
    Gp_UnlinkObj(&((Actor403600ProjectileWork*)arg0->work)->obj);
    taskKill(arg0);
}

void func_actor_403600_80138C9C(Actor403600MotionState* arg0)
{
    s32  temp_a1;
    s32  temp_a1_2;
    s32  temp_v0;
    s32  temp_v0_3;
    s32  temp_v1;
    s32  temp_v1_2;
    s32  var_v0;
    s16* temp_v0_2;
    s16* temp_v0_4;

    temp_a1        = arg0->field_80;
    temp_v1        = temp_a1 + 0x1F;
    var_v0         = temp_v1;
    arg0->field_80 = temp_v1;
    if (temp_v1 < 0) {
        var_v0 = temp_a1 + 0x3E;
    }
    temp_v0 = temp_v1 - ((var_v0 >> 5) << 5);
    __asm__("move %0,%1" : "=r"(temp_a1_2) : "r"(temp_v0));
    arg0->field_80  = temp_v0;
    temp_v0_2       = &arg0->field_0[temp_a1_2];
    temp_v0_2[0]    = 0;
    temp_v0_2[0x20] = 0;
    if (arg0->field_8E != 0) {
        if (arg0->field_8C == 0) {
            arg0->field_84 = 0;
        }
        temp_v1_2 = arg0->field_88;
        if (temp_v1_2 < 0x1000) {
            arg0->field_88 = temp_v1_2 + 0x200;
        }
    } else {
        temp_v0_3 = arg0->field_88;
        if (temp_v0_3 > 0) {
            arg0->field_88 = temp_v0_3 - 0x80;
        }
    }
    arg0->field_8C = (u16)arg0->field_8E;
    if (arg0->field_88 != 0) {
        temp_v0_4       = &arg0->field_0[temp_a1_2];
        temp_v0_4[0]    = (u16)arg0->field_84;
        temp_v0_4[0x20] = (u16)arg0->field_88;
        if (arg0->field_E0 == 0) {
            arg0->field_84 += 0x180;
            return;
        }
        arg0->field_84 += 0x100;
    }
}

s32 func_actor_403600_80138D9C(s16* arg0)
{
    s32 i;

    i = 0;
loop:
    i++;
    if (*arg0 == 0) {
        arg0++;
        if (i < 0x20) {
            goto loop;
        }
        return 1;
    }
    return 0;
}

u8* func_actor_403600_80138DCC(Task* arg0)
{
    u8*                  head;
    u8*                  restore;
    ActorProjectScratch* block;
    TmdObject*           object;
    GpCoord*             coord;
    SVECTOR*             vec;

    object = arg0->extra.tmd;
    actor_403600_load_scratch_head(head);
    coord = object->coords;
    SOFT_BARRIER();
    block = (ActorProjectScratch*)(head - sizeof(ActorProjectScratch));
    actor_403600_store_scratch_head(block);
    block->vec.vx = 0;
    block->vec.vy = 0;
    block->vec.vz = 0;
    Gp_UpdateCoord(&coord[1]);
    vec = &block->vec;
    gte_SetRotMatrix(&coord[1].workm);
    gte_SetTransMatrix(&coord[1].workm);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&block->sxy);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->flag < 0) {
        block->otz = 0;
    }
    block->otz = (block->otz >> 4) + 0x1E;
    func_actor_403600_801320F8(block->otz);
    actor_403600_load_scratch_head(restore);
    actor_403600_restore_scratch_head(restore);
    return restore;
}

/// Spawns this actor: parks its work block in `task->work`, destroying the
/// enemy if there is none, hangs the model's root coordinate under the block's
/// world coordinate, links the enemy and its three collision bodies, sets its
/// hit points from the kind's `hpMax` raised by the session's
/// `bossPartsHpSum`, builds the animation rig, turns the actor to the heading
/// its model already had, and spawns its display task above it.
void func_actor_403600_80138EF8(GpEnemy* enemy, Task* task)
{
    SVECTOR          rot;
    s16              temp_a0_2;
    s16              temp_s0_5;
    GpCoord*         temp_s5;
    Task*            temp_v0_4;
    s32              var_s0;
    GpCoord*         temp_a0;
    GpCoord*         temp_s0;
    GpRec18*         temp_s0_2;
    GpRec18*         temp_s0_3;
    GpRec18*         temp_s0_4;
    TmdObject*       temp_s2;
    Actor403600Work* temp_v0;
    GpMtxWords*      temp_v0_2;
    GpMtxWords*      temp_v0_3;
    GameSession*     gpSess;

    temp_s2 = task->extra.tmd;
    temp_s0 = temp_s2->coords;
    temp_v0 = memCalloc(0x7B8, 0);
    temp_s5 = &temp_s0[1];
    if (temp_v0 == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work                                        = temp_v0;
    temp_v0->field_4B8.sub                            = &gGfxViewCoord;
    temp_v0_2                                         = (GpMtxWords*)&temp_v0->field_4B8.coord;
    ((GpMtxWords*)&temp_v0->field_4B8.coord)->m00_m01 = 0x1000;
    temp_v0_2->m02_m10                                = 0;
    temp_v0_2->m11_m12                                = 0x1000;
    temp_v0_2->m20_m21                                = 0;
    temp_v0_2->m22                                    = 0x1000;
    temp_v0->field_4B8.coord.t[0]                     = temp_s0->coord.t[0];
    temp_v0->field_4B8.coord.t[1]                     = temp_s0->coord.t[1];
    temp_a0                                           = &temp_v0->field_4B8;
    temp_v0->field_4B8.coord.t[2]                     = temp_s0->coord.t[2];
    temp_v0_3                                         = (GpMtxWords*)&temp_s0->coord;
    temp_s0->sub                                      = temp_a0;
    ((GpMtxWords*)&temp_s0->coord)->m00_m01           = 0x1000;
    temp_v0_3->m02_m10                                = 0;
    temp_v0_3->m11_m12                                = 0x1000;
    temp_v0_3->m20_m21                                = 0;
    temp_v0_3->m22                                    = 0x1000;
    temp_s0->coord.t[0]                               = 0;
    temp_s0->coord.t[1]                               = 0x744;
    temp_s0->coord.t[2]                               = 0;
    temp_v0->field_4B8.flg                            = 0;
    Gp_UpdateCoord(temp_a0);
    temp_s0->flg = 0;
    Gp_UpdateCoord(temp_s0);
    temp_s2->flags    = 0x80;
    temp_s0->flg      = 0;
    temp_s2->lightMtx = &temp_v0->field_494;
    temp_s2->colorMtx = &temp_v0->field_474;
    enemy->field_4    = &temp_s0[1].coord;
    enemy->field_48   = 0;
    Gp_LinkNode(&enemy->node);
    enemy->node.state.b.flags = 1;
    enemy->bodyPos.vy         = -0x1F4;
    gpSess                    = gGameSession;
    enemy->coord              = temp_s5;
    enemy->bodyPos.vx         = 0;
    enemy->bodyPos.vz         = 0;
    enemy->param              = &D_actor_403600_80150EC8;
    enemy->recs               = temp_v0->field_528;
    temp_a0_2                 = D_actor_403600_80150EC8.hpMax + (((u16)gpSess->bossPartsHpSum * 0x4B) / 100);
    enemy->hp                 = temp_a0_2;
    temp_v0->field_78A        = temp_a0_2;
    var_s0                    = 1;
    temp_v0->field_798        = (s16)((temp_a0_2 * 0x3C) / 100);
    temp_v0->field_79A        = (s16)(((s16)temp_v0->field_78A * 0x23) / 100);
    func_800B3F84(&temp_v0->rig.anim, D_actor_403600_8016057C, temp_s2, temp_v0->rig.poses, temp_v0->rig.slots);
    do {
        Gp_AnimResetSlot(&temp_v0->rig.anim, var_s0, 1);
        var_s0 += 1;
    } while (var_s0 < 0x14);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    temp_v0->field_736            = 1;
    temp_v0->field_658.coord      = &temp_v0->field_4B8;
    temp_v0->field_658.spawnArgLo = 0x600;
    temp_v0->field_658.spawnArgHi = 2;
    temp_v0->field_6E8.vy         = -0x1F4;
    temp_s0_2                     = temp_v0->field_528;
    temp_v0->field_738            = 0;
    temp_v0->field_744            = 0;
    temp_v0->field_6E8.vx         = 0;
    temp_v0->field_6E8.vz         = 0xC8;
    temp_v0->field_508.coord      = temp_s5;
    temp_v0->field_508.ctx.recs   = temp_s0_2;
    temp_v0->field_508.pos.vx     = 0;
    temp_v0->field_508.pos.vy     = 0;
    temp_v0->field_508.pos.vz     = 0;
    temp_v0->field_508.key        = 0x30024;
    temp_v0->field_508.radius     = 0x3E8;
    temp_v0->field_508.flags      = 1U;
    Gp_LinkObj(2, &temp_v0->field_508);
    Gp_InitRec18Table(temp_s0_2, 4, 0);
    temp_s0_3                   = temp_v0->field_5A8;
    temp_v0->field_588.coord    = temp_s5;
    temp_v0->field_588.ctx.recs = temp_s0_3;
    temp_v0->field_588.pos.vx   = 0;
    temp_v0->field_588.pos.vy   = 0;
    temp_v0->field_588.pos.vz   = 0x3E8;
    temp_v0->field_508.flags    = temp_v0->field_508.flags | 0xC200;
    temp_v0->field_588.key      = Gp_PackPair(&D_actor_403600_80150E9C, 1);
    temp_v0->field_588.radius   = 0x5DC;
    temp_v0->field_588.flags    = 1U;
    Gp_LinkObj(3, &temp_v0->field_588);
    Gp_InitRec18Table(temp_s0_3, 1, 0);
    temp_s0_4                   = temp_v0->field_5F8;
    temp_v0->field_5C0.coord    = temp_s5;
    temp_v0->field_5C0.ctx.recs = temp_s0_4;
    temp_v0->field_5C0.pos.vx   = 0;
    temp_v0->field_5C0.pos.vz   = 0;
    temp_v0->field_5C0.pos.vy   = 0x7D0;
    temp_v0->field_5C0.key      = 0;
    temp_v0->field_5C0.radius   = 0x64;
    temp_v0->field_5C0.flags    = 1U;
    temp_v0->field_588.flags    = temp_v0->field_588.flags & 0x7FFF;
    Gp_LinkObj(3, &temp_v0->field_5C0);
    Gp_InitRec18Table(temp_s0_4, 4, 0);
    temp_v0->field_5C0.flags = temp_v0->field_5C0.flags & 0x3FFF;
    Gfx_MatrixCol2(&task->extra.tmd->coords->coord, &rot);
    temp_s0_5 = ratan2(rot.vx, rot.vz);
    rot.vx    = 0;
    rot.vy    = temp_s0_5;
    rot.vz    = 0;
    RotMatrix(&rot, &temp_v0->field_4B8.coord);
    temp_v0->field_748      = temp_s0_5;
    temp_v0_4               = Task_SpawnFromTable(&D_actor_403600_801421A0, 0, 0, 0);
    D_actor_403600_801606AC = temp_v0_4;
    if (temp_v0_4 != 0) {
        Task_Reparent(task, temp_v0_4);
    }
    temp_v0->field_4B4      = 0;
    D_actor_403600_801606A8 = task;
    temp_v0->field_78C      = 0;
    temp_v0->field_796      = 0;
    temp_v0->field_7A0      = 0;
    temp_v0->field_7AE      = 0;
    func_actor_403600_8014174C(task);
    D_actor_403600_8016056C                = 0;
    D_actor_403600_801606B8.fields.field_2 = 0;
    D_actor_403600_801606B8.fields.field_0 = 0;
    task->msgTable                         = D_actor_403600_80160504;
    task->exitCallback                     = func_actor_403600_80141598;
    temp_v0->field_730                     = 0;
    D_actor_403600_801606BC                = 0;
    task->state                            = (s32)(task->state + 1);
}

void func_actor_403600_8013938C(GpEnemy* arg0, Task* arg1)
{
    s16              temp_a1;
    s16              temp_v0;
    s16              temp_v0_2;
    s32              state;
    Actor403600Work* work;
    GpCoord*         var_a0;

    state = Gp_StateF0.field_4;
    work  = arg1->work;
    if (state == 1) {
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
    if (work->field_7AC != 0) {
        work->field_7AC = 0;
        SndEvt_EnqueueType9(0x50000000);
    }
    goto default_body;
case1:
    func_actor_403600_801412D0(arg0, arg1);
    if (work->field_7AC == 0) {
        work->field_7AC = state;
        SndEvt_EnqueueType8(0x50000000);
    }
    return;
case2:
    arg1->extra.tmd->flags   = 0x80;
    arg0->node.state.b.flags = 9;
    return;
default_body:
    if (((gDisplayState.pendingMode & 0xF0) == 0x40) && (work->field_7AC == 0)) {
        work->field_7AC = 1;
        SndEvt_EnqueueType8(0x50000000);
    }
    func_actor_403600_801396F8(arg1);
    temp_v0 = work->field_730;
    if (temp_v0 != 0) {
        if (temp_v0 < 0xA) {
            func_actor_403600_8013DC7C(arg1);
            func_actor_403600_8013955C(arg1);
            func_actor_403600_8013D15C(arg1);
        }
    }
    func_actor_403600_801411D4(arg1, 0x14);
    temp_v0_2 = work->field_730;
    if (temp_v0_2 != 0) {
        var_a0 = &work->field_4B8;
        if (temp_v0_2 < 0xA) {
            func_actor_403600_80141338(arg1);
            func_actor_403600_8014161C(arg1);
            func_actor_403600_80141A34(arg1);
            goto block_21;
        }
    } else {
    block_21:
        var_a0 = &work->field_4B8;
    }
    work->field_4B8.flg = 0;
    Gp_UpdateCoord(var_a0);
    func_actor_403600_801412D0(arg0, arg1);
    temp_a1 = work->field_77A;
    if (temp_a1 != 0) {
        Gp_SetObjTrans(arg1->extra.tmd, temp_a1, temp_a1, temp_a1);
    }
    func_actor_403600_801414FC(arg1);
    func_actor_403600_8013F0C0(arg1);
}

void func_actor_403600_8013955C(Task* arg0)
{
    Actor403600Work* temp_s1;
    GpEnemy*         temp_s0;
    s32              temp_ret;
    u32              temp_v0;
    u32              temp_v0_2;
    u32              temp_v1_2;
    u8               temp_v1;

    temp_s0 = arg0->spawnArg2;
    temp_v1 = temp_s0->reactionFlags;
    temp_s1 = arg0->work;
    if (temp_v1 != 0) {
        if (temp_v1 & 1) {
            temp_s0->reactionFlags = temp_v1 & 0xFE;
            temp_s1->field_730     = 2;
        }
        if (temp_s0->reactionFlags & 2) {
            temp_s0->reactionFlags &= 0xFD;
            temp_s1->field_730      = 3;
            temp_s1->field_736      = 0xE;
            SOFT_BARRIER();
            temp_s1->field_790 = D_actor_403600_80150ED4 * 0x1E;
        }
        if (temp_s0->reactionFlags & 0xC) {
            if (temp_s1->field_73E != 0x28) {
                temp_ret = Gp_TickObjFlag4(temp_s0);
                if (temp_ret != 0) {
                    temp_v1_2   = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState = temp_v1_2;
                    if ((temp_v1_2 >> 0x10) & 1) {
                        temp_v0            = (temp_v1_2 * 5) + 0x71357911;
                        Gp_LcgState        = temp_v0;
                        temp_s1->field_700 = ((temp_v0 >> 0xB) & 0x60) + 0x80;
                    } else {
                        temp_v0_2          = (temp_v1_2 * 5) + 0x71357911;
                        Gp_LcgState        = temp_v0_2;
                        temp_s1->field_700 = -(((temp_v0_2 >> 0xB) & 0x60) + 0x80);
                    }
                    func_actor_403600_8013DAF4(arg0, temp_ret / 5);
                }
            }
            if ((Gp_ObjFlag4Expired(temp_s0) != 0) || (temp_s0->hp < 0x1F4)) {
                temp_s0->reactionFlags &= 0xF3;
            }
        }
    }
}

const Actor403600Pattern D_actor_403600_80131E38 = { { 1, 12, 13, 14, 15, 16, 17, 18, 19 } };

void func_actor_403600_801396F8(Task* arg0)
{
    SVECTOR            sp10;
    SVECTOR            sp18;
    Actor403600Pattern sp20;
    u8                 sp30[4];
    s32                temp_a0_4;
    s32                temp_a0_6;
    s32                temp_a2_2;
    SVECTOR*           temp_a3_2;
    s16                temp_v0_13;
    s32                temp_v0_14;
    s16                temp_v0_16;
    s16                temp_v0_18;
    s32                temp_v0_19;
    s16                temp_v0_29;
    s16                temp_v0_9;
    s16                temp_v1;
    s16                temp_v1_2;
    s16                temp_v1_3;
    s16                temp_v1_6;
    s16                var_v0_3;
    GpEffWork*         temp_v0_5;
    GpEffWork*         temp_v0_6;
    GpEffWork*         temp_v0_7;
    GpEffWork*         temp_v0_8;
    s32                temp_s2;
    s32                temp_v0_17;
    s32                var_a0;
    s32                patternIndex;
    s32                var_s2;
    s32                var_s2_2;
    s32                var_v0;
    s32                temp_s0_2;
    s32                temp_s0_4;
    u16                temp_v0_22;
    u16                temp_v0_23;
    u16                temp_v0_27;
    u16                temp_v0_28;
    u16                temp_v0_2;
    u16                temp_v0_4;
    s32                temp_v1_4;
    u16                var_v0_4;
    u32                temp_v0_10;
    u32                temp_v0_11;
    u8*                temp_s0_5;
    u8*                temp_s0_6;
    u8                 temp_a0;
    u8                 temp_a0_2;
    u8                 temp_a0_3;
    u8                 temp_a0_5;
    GpCoord*           temp_s0;
    GpCoord*           temp_s0_3;
    Actor403600Work*   temp_s1;
    Actor403600Work*   temp_v0;
    Actor403600Work*   temp_v0_12;
    Actor403600Work*   temp_v0_15;
    Actor403600Work*   temp_v0_3;

    temp_s1 = arg0->work;
    temp_v1 = temp_s1->field_730;
    switch (temp_v1) {
        case 0:
            temp_s1->field_4B8.coord.t[0] = 0x196E;
            temp_s1->field_4B8.coord.t[1] = 0x1AE;
            temp_s1->field_4B8.coord.t[2] = 0x1630;
            return;
        case 1:
            func_actor_403600_8013A444(arg0);
            return;
        case 2:
            func_actor_403600_80141B24(arg0);
            temp_s1->field_774 = 0;
            temp_s1->field_784 = 1;
            temp_s1->field_74A = 0;
            temp_s1->field_736 = 0xB;
            if (temp_s1->field_73A < 0xF) {
                temp_s1->field_73C = -0xA;
            }
            if (temp_s1->field_73A >= 0x27) {
                temp_v0            = arg0->work;
                temp_v0->field_756 = 8;
                temp_v0->field_778 = 0x10;
                temp_v0->field_776 = 0xA;
                temp_v0->field_742 = 0;
                temp_v0->field_746 = 0;
                temp_v0->field_774 = 0;
                temp_v0->field_77A = 0U;
                temp_v0->field_784 = 0;
                temp_v0->field_73C = 0;
                temp_v0->field_73E = 0;
                temp_v0->field_74A = 0;
                temp_v0->field_73A = 0;
                temp_v0->field_76E = 0x40;
                temp_v0->field_75E = 0;
                temp_v0->field_7A4 = 0;
                temp_v0->field_7A6 = 0;
                temp_v0->field_7AC = 0;
                temp_s1->field_730 = 1;
                return;
            }
        default:
            return;
        case 3:
            func_actor_403600_80141B24(arg0);
            temp_s1->field_73C = 0;
            temp_s1->field_74A = 0;
            if (temp_s1->field_736 == 0xE) {
                temp_v0_2          = temp_s1->field_790 - 1;
                temp_s1->field_790 = temp_v0_2;
                if ((temp_v0_2 << 0x10) == 0) {
                    temp_s1->field_736 = 0xF;
                    return;
                }
            } else {
                var_v0 = temp_s1->field_73A < 0x11;
                goto block_30;
            }
            break;
        case 4:
            func_actor_403600_80141B24(arg0);
            temp_s1->field_73C = 0;
            temp_s1->field_74A = 0;
            temp_v0_4          = temp_s1->field_790 - 1;
            temp_s1->field_790 = temp_v0_4;
            if ((temp_v0_4 << 0x10) != 0) {
                *(volatile s16*)&temp_s1->field_778 = 0;
                temp_s1->field_4B8.coord.t[1]       = (s32)(temp_s1->field_4B8.coord.t[1] + (rsin(*(volatile s32*)&gDisplayState.animFrame << 9) >> 8));
                return;
            }
            goto block_31;
        case 5:
            func_actor_403600_80141B24(arg0);
            temp_s1->field_78C = 1;
            temp_s1->field_784 = 1;
            temp_s1->field_736 = 0xB;
            temp_s1->field_774 = 0;
            temp_s1->field_74A = 0;
            temp_s1->field_70A = 0;
            if (temp_s1->field_73A == 1) {
                memset(&sp10, 0, 8);
                sp10.vy = 0x64;
                Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[1], 3, &sp10);
                func_actor_403600_80141954(1);
                D_800626EC[5].arg.model = &D_80187B10;
                temp_v0_5               = Gp_SpawnEff(0x80005, &arg0->extra.tmd->coords[1], 0, NULL);
                if (temp_v0_5 != NULL) {
                    func_actor_403600_801419E8(temp_v0_5->task);
                }
                D_800626EC[5].arg.model = &D_80187E14;
                temp_v0_6               = Gp_SpawnEff(0x80005, &arg0->extra.tmd->coords[1], 0, NULL);
                if (temp_v0_6 != NULL) {
                    func_actor_403600_801419E8(temp_v0_6->task);
                }
                D_800626EC[5].arg.model = &D_80188264;
                temp_v0_7               = Gp_SpawnEff(0x80005, &arg0->extra.tmd->coords[1], 0, NULL);
                if (temp_v0_7 != NULL) {
                    func_actor_403600_801419E8(temp_v0_7->task);
                }
                D_800626EC[5].arg.model = &D_8018864C;
                temp_v0_8               = Gp_SpawnEff(0x80005, &arg0->extra.tmd->coords[1], 0, NULL);
                if (temp_v0_8 != NULL) {
                    func_actor_403600_801419E8(temp_v0_8->task);
                }
                Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[1], 0x800, NULL);
            }
            temp_v1_2 = temp_s1->field_73A;
            if ((temp_v1_2 == 4) || (temp_v1_2 == 6)) {
                memset(&sp18, 0, 8);
                sp18.vy = 0x64;
                Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[1], 3, &sp18);
            }
            if (temp_s1->field_73A < 0xF) {
                temp_s1->field_73C = -0xA;
            }
            var_v0 = temp_s1->field_73A < 0x27;
        block_30:
            if (var_v0 != 0) {
                return;
            }
        block_31:
            temp_v0_3            = arg0->work;
            temp_v0_3->field_756 = 8;
            temp_v0_3->field_778 = 0x10;
            temp_v0_3->field_776 = 0xA;
            temp_v0_3->field_742 = 0;
            temp_v0_3->field_746 = 0;
            temp_v0_3->field_774 = 0;
            temp_v0_3->field_77A = 0U;
            temp_v0_3->field_784 = 0;
            temp_v0_3->field_73C = 0;
            temp_v0_3->field_73E = 0;
            temp_v0_3->field_74A = 0;
            temp_v0_3->field_73A = 0;
            temp_v0_3->field_76E = 0x40;
            temp_v0_3->field_75E = 0;
            temp_v0_3->field_7A4 = 0;
            temp_v0_3->field_7A6 = 0;
            temp_v0_3->field_7AC = 0;
            temp_s1->field_730   = 1;
            return;
        case 6:
            if (temp_s1->field_78C == 1) {
                if (temp_s1->field_73A >= 0x32) {
                    temp_a0 = temp_s1->field_7A2;
                    Fade_DrawOverlay(temp_a0, temp_a0, temp_a0, 1);
                    temp_v0_9          = (u16)temp_s1->field_7A2 + 0xF;
                    temp_s1->field_7A2 = temp_v0_9;
                    if (temp_v0_9 >= 0xFF) {
                        temp_s1->field_7A2 = 0xFF;
                    }
                }
                if (((u16)temp_s1->field_73A & 3) == 3) {
                    Gp_SpawnEff(0x60055, &arg0->extra.tmd->coords[1], 0x12800, NULL);
                }
                temp_v1_3 = temp_s1->field_73A;
                if (temp_v1_3 < 0x2F) {
                    if (temp_v1_3 == 0x2E) {
                        temp_s0    = &temp_s1->field_4B8;
                        temp_s2    = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160013;
                        temp_s0_2  = (s8)Gp_GetObjPan(temp_s0);
                        temp_v0_10 = gpGetObjDepth(temp_s0);
                        SndEvt_EnqueueType6(temp_s2, temp_s0_2, (s32)(((temp_v0_10 >> 0x1F) + temp_v0_10) << 0x17) >> 0x18);
                    }
                    if (((u16)temp_s1->field_73A & 0xF) == 0xF) {
                        func_80181940(arg0);
                    }
                } else {
                    func_80181940(arg0);
                    if (temp_s1->field_73A == 0x32) {
                        temp_s0_3  = &temp_s1->field_4B8;
                        temp_s2    = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160014;
                        temp_s0_4  = (s8)Gp_GetObjPan(temp_s0_3);
                        temp_v0_11 = gpGetObjDepth(temp_s0_3);
                        SndEvt_EnqueueType6(temp_s2, temp_s0_4, (s32)(((temp_v0_11 >> 0x1F) + temp_v0_11) << 0x17) >> 0x18);
                        Gp_SpawnEff(0x601BC, &arg0->extra.tmd->coords[1], 0x1E, NULL);
                    }
                }
                if (temp_s1->field_73A == 0x3C) {
                    func_actor_403600_80141954(0);
                }
                temp_s1->field_73C = 0;
                temp_s1->field_74A = 0;
                temp_s1->field_736 = 4;
                if (temp_s1->field_73A >= 0x46) {
                    SndEvt_EnqueueType7(0x54160013, 0x14);
                    temp_v0_12            = arg0->work;
                    temp_v0_12->field_756 = 8;
                    temp_v0_12->field_778 = 0x10;
                    temp_v0_12->field_776 = 0xA;
                    temp_v0_12->field_742 = 0;
                    temp_v0_12->field_746 = 0;
                    temp_v0_12->field_774 = 0;
                    temp_v0_12->field_77A = 0U;
                    temp_v0_12->field_784 = 0;
                    temp_v0_12->field_73C = 0;
                    temp_v0_12->field_73E = 0;
                    temp_v0_12->field_74A = 0;
                    temp_v0_12->field_73A = 0;
                    temp_v0_12->field_76E = 0x40;
                    temp_v0_12->field_75E = 0;
                    temp_v0_12->field_7A4 = 0;
                    temp_v0_12->field_7A6 = 0;
                    temp_v0_12->field_7AC = 0;
                    temp_s1->field_736    = 5;
                    temp_s1->field_78C    = 2;
                    return;
                }
            } else {
                temp_a0_2 = temp_s1->field_7A2;
                Fade_DrawOverlay(temp_a0_2, temp_a0_2, temp_a0_2, 1);
                temp_v0_13         = (u16)temp_s1->field_7A2 - 0x28;
                temp_s1->field_7A2 = temp_v0_13;
                if ((temp_v0_13 << 0x10) <= 0) {
                    temp_s1->field_7A2 = 0;
                }
                temp_v0_14 = temp_s1->field_73A;
                if (temp_v0_14 == 0xA) {
                    SndEvt_EnqueueType7(0x54160013, 0x14);
                    temp_v0_14 = temp_s1->field_73A;
                }
                if (temp_v0_14 >= 0x1E) {
                    temp_v0_15            = arg0->work;
                    temp_v0_15->field_756 = 8;
                    temp_v0_15->field_778 = 0x10;
                    temp_v0_15->field_742 = 0;
                    temp_v0_15->field_746 = 0;
                    temp_v0_15->field_774 = 0;
                    temp_v0_15->field_77A = 0U;
                    temp_v0_15->field_784 = 0;
                    temp_v0_15->field_73C = 0;
                    temp_v0_15->field_73E = 0;
                    temp_v0_15->field_74A = 0;
                    temp_v0_15->field_73A = 0;
                    temp_v0_15->field_776 = 0xA;
                    temp_v0_15->field_76E = 0x40;
                    temp_v0_15->field_75E = 0;
                    temp_v0_15->field_7A4 = 0;
                    temp_v0_15->field_7A6 = 0;
                    temp_v0_15->field_7AC = 0;
                    temp_s1->field_78C    = 0;
                    temp_s1->field_730    = 1;
                    return;
                }
            }
            break;
        case 10:
            temp_a0_3 = temp_s1->field_7A2;
            Fade_DrawOverlay(temp_a0_3, temp_a0_3, temp_a0_3, 1);
            temp_v0_16         = (u16)temp_s1->field_7A2 - 0x1E;
            temp_s1->field_7A2 = temp_v0_16;
            if ((temp_v0_16 << 0x10) <= 0) {
                temp_s1->field_7A2 = 0;
            }
            temp_s1->field_73C = 0;
            temp_s1->field_74A = 0;
            Gp_SetObjTrans(arg0->extra.tmd, 0x1F40, 0x1F40, 0x1F40);
            return;
        case 11:
            temp_s1->field_73C = 0;
            temp_s1->field_74A = 0;
            temp_s1->field_77A = (u16)(temp_s1->field_77A + 0x64);
            return;
        case 12:
            temp_a0_4 = temp_s1->field_732;
            switch (temp_a0_4) {
                case 0:
                    temp_v0_17                    = temp_s1->field_4B8.coord.t[1] + temp_s1->field_73C;
                    temp_s1->field_4B8.coord.t[1] = temp_v0_17;
                    if (temp_v0_17 < -0x1B61) {
                        *(volatile s16*)&temp_s1->field_776 = 0;
                        temp_s1->field_776                  = -0x19;
                        temp_s1->field_73C                  = 0;
                        temp_s1->field_73A                  = 0;
                        temp_s1->field_794                  = 3U;
                        temp_s1->field_732                  = (s16)((u16)temp_s1->field_732 + 1);
                    }
                    if (!((u16)temp_s1->field_73A & 1)) {
                        temp_v0_18         = (u16)temp_s1->field_73C + 2;
                        temp_s1->field_73C = temp_v0_18;
                        var_a0             = 0x601BF;
                        if (temp_v0_18 >= -0x1E) {
                            temp_s1->field_73C = -0x1E;
                            goto block_71;
                        }
                    } else {
                    block_71:
                        var_a0 = 0x601BF;
                    }
                    Gp_SpawnEff(var_a0, &arg0->extra.tmd->coords[15], 0xC00, NULL);
                    Gp_SpawnEff(0x601BF, &arg0->extra.tmd->coords[19], 0xC00, NULL);
                    return;
                case 1:
                    if ((u16)temp_s1->field_73A & 1) {
                        temp_v0_19                    = temp_s1->field_776;
                        temp_v1_4                     = temp_s1->field_4B8.coord.t[1];
                        temp_v1_4                    += temp_v0_19;
                        temp_s1->field_4B8.coord.t[1] = temp_v1_4;
                        temp_v1_4                     = (u16)temp_s1->field_776;
                        if (temp_v0_19 < 0) {
                            temp_v0_19         = temp_v1_4 + 1;
                            temp_s1->field_776 = temp_v0_19;
                            if ((temp_v0_19 << 0x10) == 0) {
                                temp_s1->field_776 = temp_a0_4;
                                temp_v0_22         = temp_s1->field_794 - 1;
                                temp_s1->field_794 = temp_v0_22;
                                if ((temp_v0_22 << 0x10) == 0) {
                                    temp_s1->field_794 = 3U;
                                    var_v0_3           = 0x19;
                                    goto block_81;
                                }
                            }
                        } else {
                            temp_v0_19         = temp_v1_4 - 1;
                            temp_s1->field_776 = temp_v0_19;
                            if ((temp_v0_19 << 0x10) == 0) {
                                temp_s1->field_776 = 0;
                                temp_v0_23         = temp_s1->field_794 - 1;
                                temp_s1->field_794 = temp_v0_23;
                                if ((temp_v0_23 << 0x10) == 0) {
                                    temp_s1->field_794 = 3U;
                                    var_v0_3           = -0x19;
                                block_81:
                                    temp_s1->field_776 = var_v0_3;
                                }
                            }
                        }
                    }
                    if (temp_s1->field_792 >= 0x14) {
                        sp20   = D_actor_403600_80131E38;
                        var_s2 = 0;
                        do {
                            temp_a0_6 = 0x60070;
                            temp_a2_2 = 0x34C00;
                            temp_a3_2 = NULL;
                            SOFT_TOUCH_REG3(temp_a0_6, temp_a2_2, temp_a3_2);
                            patternIndex = var_s2 & 0xFF;
                            temp_s0_5    = sp20.values;
                            temp_s0_5   += patternIndex;
                            var_s2      += 1;
                            Gp_SpawnEff(temp_a0_6, &arg0->extra.tmd->coords[*temp_s0_5], temp_a2_2, temp_a3_2);
                            Gp_SpawnEff(0x601BF, &arg0->extra.tmd->coords[*temp_s0_5], 0xC00, NULL);
                        } while ((u32)(var_s2 & 0xFF) < 9U);
                        temp_s1->field_792 = 0;
                    }
                    temp_s1->field_792 = (s16)((u16)temp_s1->field_792 + 1);
                    if (temp_s1->field_73A >= 0xC8) {
                        var_v0_4           = temp_s1->field_732;
                        temp_s1->field_73A = 0;
                        goto block_116;
                    }
                    break;
                case 2:
                    temp_s1->field_736 = 0x14;
                    if (temp_s1->field_792 >= 0x14) {
                        sp20     = D_actor_403600_80131E38;
                        var_s2_2 = 0;
                        do {
                            temp_a0_6 = 0x60070;
                            temp_a2_2 = 0x34C00;
                            temp_a3_2 = NULL;
                            SOFT_TOUCH_REG3(temp_a0_6, temp_a2_2, temp_a3_2);
                            patternIndex = var_s2_2 & 0xFF;
                            temp_s0_6    = sp20.values;
                            temp_s0_6   += patternIndex;
                            var_s2_2    += 1;
                            Gp_SpawnEff(temp_a0_6, &arg0->extra.tmd->coords[*temp_s0_6], temp_a2_2, temp_a3_2);
                            Gp_SpawnEff(0x601BF, &arg0->extra.tmd->coords[*temp_s0_6], 0xC00, NULL);
                        } while ((u32)(var_s2_2 & 0xFF) < 9U);
                        temp_s1->field_792 = 0;
                    }
                    temp_s1->field_792 = (s16)((u16)temp_s1->field_792 + 1);
                    if ((u16)temp_s1->field_73A & 1) {
                        temp_v0_19                    = temp_s1->field_776;
                        temp_v1_4                     = temp_s1->field_4B8.coord.t[1];
                        temp_v1_4                    += temp_v0_19;
                        temp_s1->field_4B8.coord.t[1] = temp_v1_4;
                        temp_v1_4                     = (u16)temp_s1->field_776;
                        if (temp_v0_19 < 0) {
                            temp_v0_19         = temp_v1_4 + 1;
                            temp_s1->field_776 = temp_v0_19;
                            if ((temp_v0_19 << 0x10) == 0) {
                                temp_s1->field_776 = 1;
                                temp_v0_27         = temp_s1->field_794 - 1;
                                temp_s1->field_794 = temp_v0_27;
                                if ((temp_v0_27 << 0x10) == 0) {
                                    temp_s1->field_794 = 3U;
                                    temp_s1->field_776 = 0x19;
                                    return;
                                }
                            }
                        } else {
                            temp_v0_19         = temp_v1_4 - 1;
                            temp_s1->field_776 = temp_v0_19;
                            if ((temp_v0_19 << 0x10) == 0) {
                                temp_s1->field_776 = 0;
                                temp_v0_28         = temp_s1->field_794 - 1;
                                temp_s1->field_794 = temp_v0_28;
                                if ((temp_v0_28 << 0x10) == 0) {
                                    temp_s1->field_794 = 3U;
                                    temp_s1->field_776 = -0x19;
                                    return;
                                }
                            }
                        }
                    }
                    break;
            }
            break;
        case 20:
            temp_a0_5 = temp_s1->field_7A2;
            Fade_DrawOverlay(temp_a0_5, temp_a0_5, temp_a0_5, 1);
            temp_v0_29         = (u16)temp_s1->field_7A2 + 2;
            temp_s1->field_7A2 = temp_v0_29;
            if (temp_v0_29 >= 0xFF) {
                temp_s1->field_7A2 = 0xFF;
            }
            temp_v1_6 = temp_s1->field_732;
            switch (temp_v1_6) {
                case 0:
                    func_800E9BDC(1, 0xF9FF);
                    temp_s1->field_70A         = 0;
                    Player_Status.peStateFlags = 0;
                    goto block_115;
                case 1:
                    if ((s16)temp_s1->field_7A2 == 0xFF) {
                        sp20.values[2] = 0x1E;
                        sp20.values[3] = 0;
                        sp20.values[0] = 0;
                        sp30[0]        = 0x24;
                        sp30[3]        = 0;
                        sp30[2]        = 0;
                        sp30[1]        = 0;
                        CdCmd_Enqueue(0x21, sp20.values, sp30);
                        goto block_115;
                    }
                    break;
                case 2:
                    if (CdCmd_IsIdle() & 0xFFFF) {
                        if (D_actor_403600_801606B4 != 0) {
                            Task_CallExit(D_actor_403600_801606B4);
                        }
                        Task_SpawnFromTable(&D_80162E98, 0, 0, 0);
                        func_800E9BDC(0, 0xF9FF);
                        goto block_115;
                    }
                    break;
            }
            break;
    }
    return;

block_115:
    var_v0_4 = temp_s1->field_732;
block_116:
    temp_s1->field_732 = (s16)(var_v0_4 + 1);
}

void Gp_SpawnPadLerpWide(s32 arg0, s32 arg1, s32 arg2) asm("Gp_SpawnPadLerp");

void func_actor_403600_8013A444(Task* arg0)
{
    u32              sp10;
    s32              sp14;
    s32              node_flag;
    s32              rumble_distance;
    s32              state16;
    s32              adjusted_y0;
    s32              adjusted_y1;
    s32              adjusted_y2;
    s32              var_a1;
    s32              var_a1_2;
    s32              var_a0_2;
    s16              temp_a0;
    s16              temp_a0_2;
    s32              temp_a2;
    s32              temp_s1_2;
    s16              temp_v0_11;
    s16              temp_v0_16;
    s16              temp_v0_25;
    s16              temp_v0_3;
    s16              temp_v1;
    s16              temp_v1_10;
    s16              temp_v1_11;
    s32              temp_v1_12;
    s16              temp_v1_4;
    s16              temp_v1_8;
    s16              temp_v1_9;
    s32              var_v0_10;
    s32              var_v0_13;
    s32              var_v0_3;
    s32              var_v0_8;
    s32              temp_lo;
    s32              temp_lo_2;
    s32              temp_lo_3;
    s32              temp_lo_4;
    s32              temp_s1;
    s32              temp_s4;
    s32              temp_v0_26;
    s32              temp_v0_27;
    s32              temp_v0_28;
    s32              temp_v1_3;
    s32              temp_v1_5;
    s32              temp_v1_6;
    s32              temp_v1_7;
    s32              var_s2;
    s32              var_v0_12;
    s32              var_v0_14;
    s32              temp_s0_11;
    s32              temp_s0_12;
    s32              temp_s0_14;
    s32              temp_s0_16;
    s32              temp_s0_17;
    s32              temp_s0_19;
    s32              temp_s0_21;
    s32              temp_s0_22;
    s32              temp_s0_23;
    s32              temp_s0_25;
    s32              temp_s0_26;
    s32              temp_s0_28;
    s32              temp_s0_2;
    s32              temp_s0_30;
    s32              temp_s0_32;
    s32              temp_pan_28;
    s32              temp_s0_4;
    s32              temp_s0_6;
    s32              temp_s0_7;
    s32              temp_s0_9;
    u16              temp_a3;
    u16              temp_v0_32;
    u16              temp_v0_34;
    u16              temp_v0_35;
    u16              temp_v0_9;
    u16              var_v0_6;
    u32              temp_v0;
    u32              temp_v0_12;
    u32              temp_v0_13;
    u32              temp_v0_14;
    u32              temp_v0_15;
    u32              temp_v0_17;
    u32              temp_v0_18;
    u32              temp_v0_19;
    u32              temp_v0_21;
    u32              temp_v0_22;
    u32              temp_v0_23;
    u32              temp_v0_24;
    u32              temp_v0_30;
    u32              temp_v0_31;
    u32              temp_v0_33;
    u32              temp_v0_36;
    u32              temp_v0_37;
    u32              temp_v0_5;
    u32              temp_v0_6;
    u32              temp_v0_7;
    u32              temp_v0_8;
    u32              temp_depth_28;
    GpCoord*         temp_s0;
    GpCoord*         temp_s0_10;
    GpCoord*         temp_s0_13;
    GpCoord*         temp_s0_15;
    GpCoord*         temp_s0_18;
    GpCoord*         temp_s0_20;
    GpCoord*         temp_s0_24;
    GpCoord*         temp_s0_27;
    GpCoord*         temp_s0_29;
    GpCoord*         temp_s0_31;
    GpCoord*         temp_s0_3;
    GpCoord*         temp_s0_5;
    GpCoord*         temp_s0_8;
    GpCoord*         temp_sound_28;
    GpAnimArg*       temp_s0_msg;
    GpAnimArg*       temp_s1_3;
    Actor403600Work* temp_s3;
    GpCoord*         temp_s4_4;
    GpCoord*         temp_s6;
    GpEnemy*         temp_s7;
    Actor403600Work* temp_v0_10;
    Actor403600Work* temp_v0_20;
    Actor403600Work* temp_v0_2;
    Actor403600Work* temp_reset_30;
    Actor403600Work* temp_v0_4;
    GameActor*       temp_v1_2;
    GpCoord*         var_a0;
    GpCoord*         var_s0;
    PlayerStatus*    temp_wip;

    temp_wip = &Player_Status;
    temp_s3  = (Actor403600Work*)arg0->work;
    temp_s7  = arg0->spawnArg2;
    temp_a0  = temp_s3->field_73E;
    temp_s6  = Gp_ActorSlots[0]->extra.tmd->coords;
    switch (temp_a0) {
        case 0x0:
            temp_s3->field_736 = 1U;
            temp_s3->field_73C = 0U;
            temp_s3->field_74A = 0;
            if (temp_s3->field_73A >= temp_s3->field_776) {
                temp_s3->field_776 = 0xA;
                temp_s3->field_73A = 0;
                func_actor_403600_8013E470(&temp_s3->field_4B8, &sp10, &sp14);
                if ((sp10 < 0x835U) && (temp_s3->field_7B4 == 0)) {
                    temp_s3->field_588.pos.vz = 0x3E8;
                    temp_s3->field_588.key    = Gp_PackPair(&D_actor_403600_80150E9C, 1);
                    temp_s3->field_588.radius = 0x5DC;
                    temp_s3->field_736        = 2U;
                    temp_s3->field_73E        = 0x46;
                    temp_s3->field_73C        = 0U;
                    temp_s3->field_73A        = 0;
                    temp_s3->field_7B2        = 0U;
                    return;
                }
                func_actor_403600_8013EA04(arg0);
                if (temp_s3->field_73E == 0) {
                    temp_s3->field_776 = 0;
                    return;
                }
                temp_s3->field_7B4 = 0;
                return;
            }
        default:
            return;
        case 0x1:
            if (temp_s3->field_73A == 0x14) {
                temp_s0   = &temp_s3->field_4B8;
                temp_s4   = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160013;
                temp_s0_2 = (s8)Gp_GetObjPan(temp_s0);
                temp_v0   = gpGetObjDepth(temp_s0);
                SndEvt_EnqueueType6(temp_s4, temp_s0_2, (s32)(((temp_v0 >> 0x1F) + temp_v0) << 0x17) >> 0x18);
            }
            if (temp_s3->field_73A >= 0x14) {
                func_80181940(arg0);
            }
            temp_s3->field_736 = 4U;
            temp_s3->field_73C = 0U;
            temp_s3->field_74A = 0;
            if (temp_s3->field_73A >= 0x2D) {
                temp_v0_2            = ((Actor403600Work*)arg0->work);
                temp_v0_2->field_756 = 8;
                temp_v0_2->field_778 = 0x10;
                temp_v0_2->field_776 = 0xA;
                temp_v0_2->field_742 = 0;
                temp_v0_2->field_746 = 0;
                temp_v0_2->field_774 = 0;
                temp_v0_2->field_77A = 0;
                temp_v0_2->field_784 = 0;
                temp_v0_2->field_73C = 0U;
                temp_v0_2->field_73E = 0;
                temp_v0_2->field_74A = 0;
                temp_v0_2->field_73A = 0;
                temp_v0_2->field_76E = 0x40;
                temp_v0_2->field_75E = 0;
                temp_v0_2->field_7A4 = 0;
                temp_v0_2->field_7A6 = 0;
                temp_v0_2->field_7AC = 0;
                temp_s3->field_73E   = 2;
                temp_s3->field_7AE   = 0;
                return;
            }
            break;
        case 0x2:
            if (temp_s3->field_73A < 0xC) {
                func_80181940(arg0);
            }
            temp_s3->field_736 = 5U;
            temp_v0_3          = temp_s3->field_73A;
            if (temp_v0_3 == 0xA) {
                SndEvt_EnqueueType7(0x54160013, 0x14);
            }
            if (temp_s3->field_73A >= 0x1E) {
                temp_v0_4            = ((Actor403600Work*)arg0->work);
                temp_v0_4->field_756 = 8;
                temp_v0_4->field_778 = 0x10;
                temp_v0_4->field_742 = 0;
                temp_v0_4->field_746 = 0;
                temp_v0_4->field_774 = 0;
                temp_v0_4->field_77A = 0;
                temp_v0_4->field_784 = 0;
                temp_v0_4->field_73C = 0U;
                temp_v0_4->field_73E = 0;
                temp_v0_4->field_74A = 0;
                temp_v0_4->field_73A = 0;
                temp_v0_4->field_776 = 0xA;
                temp_v0_4->field_76E = 0x40;
                temp_v0_4->field_75E = 0;
                temp_v0_4->field_7A4 = 0;
                temp_v0_4->field_7A6 = 0;
                temp_v0_4->field_7AC = 0;
                return;
            }
            break;
        case 0xA:
            temp_v1 = temp_s3->field_732;
            switch (temp_v1) {
                case 0:
                    temp_s3->field_784    = 1;
                    temp_s3->field_778    = 0x20;
                    temp_s3->field_736    = 0x10U;
                    temp_s3->field_73C    = 0x14U;
                    temp_s3->field_74A    = 0;
                    temp_s3->field_6F0.vx = (s32)D_actor_403600_801605D4.vx;
                    temp_s3->field_6F0.vy = (s32)D_actor_403600_801605D4.vy;
                    temp_s3->field_6F0.vz = (s32)D_actor_403600_801605D4.vz;
                    temp_s3->field_76E    = 0x40;
                    temp_s3->field_746    = 1;
                    func_actor_403600_8013DFE0(arg0);
                    if (temp_s3->field_73A >= 0x13) {
                        temp_s0_3 = &temp_s3->field_4B8;
                        temp_s4   = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160004;
                        temp_s0_4 = (s8)Gp_GetObjPan(temp_s0_3);
                        temp_v0_5 = gpGetObjDepth(temp_s0_3);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_4, (s32)(((temp_v0_5 >> 0x1F) + temp_v0_5) << 0x17) >> 0x18);
                        temp_s3->field_778 = 0x10;
                        temp_s3->field_76E = 0x80;
                        temp_s3->field_73A = 0;
                        temp_s3->field_7A4 = 1;
                        temp_s3->field_732 = (s16)((u16)temp_s3->field_732 + 1);
                        return;
                    }
                    break;
                case 1:
                    temp_s3->field_736 = 0x12U;
                    temp_s3->field_73C = 0x4B0U;
                    temp_s3->field_74A = 0;
                    func_actor_403600_8013DFE0(arg0);
                    if (temp_s3->field_4B8.coord.t[1] < D_actor_403600_801605D4.vy) {
                        temp_s3->field_4B8.coord.t[0] = (s32)D_actor_403600_801605D4.vx;
                        temp_s3->field_4B8.coord.t[1] = (s32)D_actor_403600_801605D4.vy;
                        temp_s3->field_4B8.coord.t[2] = (s32)D_actor_403600_801605D4.vz;
                        temp_s3->field_6F0.vx         = (s32)D_actor_403600_801605DC.vx;
                        temp_s3->field_6F0.vy         = (s32)D_actor_403600_801605DC.vy;
                        temp_s3->field_6F0.vz         = (s32)D_actor_403600_801605DC.vz;
                        temp_s3->field_746            = 3;
                        func_actor_403600_8013DFE0(arg0);
                        temp_s3->field_774 = 1;
                        temp_s3->field_73A = 0;
                        temp_s3->field_734 = 0;
                        temp_s3->field_73C = 0U;
                        temp_s3->field_792 = 0x28;
                        temp_s3->field_732 = (s16)((u16)temp_s3->field_732 + 1);
                        return;
                    }
                    break;
                case 2:
                    if (temp_s3->field_792 != 0) {
                        temp_s3->field_792 = (s16)((u16)temp_s3->field_792 - 1);
                        return;
                    }
                    if (!((u16)temp_s3->field_73A & 1)) {
                        Gp_SpawnEff(0x601B9, &arg0->extra.tmd->coords[1], 0, &D_actor_403600_80160664);
                        Gp_SpawnEff(0x601B9, &arg0->extra.tmd->coords[15], 0x800, NULL);
                        Gp_SpawnEff(0x601B9, &arg0->extra.tmd->coords[19], 0x800, NULL);
                    }
                    temp_s3->field_736 = 0x12U;
                    temp_s3->field_73C = 0x320U;
                    temp_s3->field_74A = 0;
                    func_actor_403600_801417A8(arg0, 0x14);
                    if (((D_actor_403600_801605DC.vy - 0x1388) < temp_s3->field_4B8.coord.t[1]) && (temp_s3->field_734 == 0)) {
                        temp_s3->field_734 = 1;
                        temp_s0_5          = &temp_s3->field_4B8;
                        temp_s4            = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160005;
                        temp_s0_6          = (s8)Gp_GetObjPan(temp_s0_5);
                        temp_v0_6          = gpGetObjDepth(temp_s0_5);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_6, (s32)(((temp_v0_6 >> 0x1F) + temp_v0_6) << 0x17) >> 0x18);
                    }
                    if ((D_actor_403600_801605DC.vy - 0x3E8) < temp_s3->field_4B8.coord.t[1]) {
                        Gp_SpawnPadLerp(0xA, 0xFF, 0x50);
                        var_s2 = 0;
                        if (Player_Status.coordMtx->t[1] < -0xF3B) {
                            temp_s4_4                       = &temp_s3->field_4B8;
                            D_actor_403600_801606A4.field_0 = (u16)D_actor_403600_80150EA4;
                            func_actor_403600_8013E470(temp_s4_4, &sp10, &sp14);
                            if ((u32)(sp10 - 0xFA0) < 0x7D1U) {
                                temp_v1_2                       = gameGetPtrSlot(3)->work;
                                D_actor_403600_801606A4.field_2 = 0;
                                D_actor_403600_801606A4.field_0 = (u16)((u16)D_actor_403600_801606A4.field_0 >> 2);
                                temp_v1_2->field_96C            = 2;
                                temp_v1_2->field_96E            = (u16)D_actor_403600_801606A4.field_0;
                                temp_v1_2->field_972            = 0;
                            } else {
                                if ((u32)(sp10 - 0x9C4) < 0x5DCU) {
                                    D_actor_403600_801606A4.field_2 = 0;
                                    D_actor_403600_801606A4.field_0 = (u16)((u16)D_actor_403600_801606A4.field_0 >> 1);
                                    var_v0_3                        = (s16)func_actor_403600_8013E66C(temp_s4_4);
                                    if (var_v0_3 < 0) {
                                        var_v0_3 = -var_v0_3;
                                    }
                                    if (var_v0_3 >= 0x401) {
                                        D_actor_403600_8016056C = 4;
                                        func_actor_403600_8013E7D4((s32)arg0, 0);
                                        temp_s3->field_762 = 0x64;
                                    } else {
                                        D_actor_403600_8016056C = 3;
                                        func_actor_403600_8013E7D4((s32)arg0, 1);
                                        temp_s3->field_762 = -0x64;
                                    }
                                    Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, &D_actor_403600_80160568, 0);
                                    var_s2             = 2;
                                    temp_s3->field_760 = 0;
                                } else if (sp10 < 0x9C4U) {
                                    D_actor_403600_801606A4.field_2 = 0;
                                    D_actor_403600_80160568.field_4 = 1;
                                    func_actor_403600_8013E7D4((s32)arg0, 1);
                                    temp_s3->field_762 = -0x190;
                                    Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, &D_actor_403600_80160568, 0);
                                    var_s2             = 3;
                                    temp_s3->field_760 = 0;
                                }
                            }
                            if (var_s2 != 0) {
                                temp_s4   = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 6;
                                temp_s0_7 = (s8)Gp_GetObjPan(temp_s6);
                                temp_v0_7 = gpGetObjDepth(temp_s6);
                                SndEvt_EnqueueType6(temp_s4, temp_s0_7, (s32)(((temp_v0_7 >> 0x1F) + temp_v0_7) << 0x17) >> 0x18);
                                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F9, Gp_PackPair(&D_actor_403600_801606A4, 0), 0);
                            }
                        }
                        temp_s0_8 = &temp_s3->field_4B8;
                        temp_s4   = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160006;
                        temp_s0_9 = (s8)Gp_GetObjPan(temp_s0_8);
                        temp_v0_8 = gpGetObjDepth(temp_s0_8);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_9, (s32)(((temp_v0_8 >> 0x1F) + temp_v0_8) << 0x17) >> 0x18);
                        Gp_SpawnEff(0x601BA, &arg0->extra.tmd->coords[1], 0, &D_actor_403600_80160664);
                        temp_s3->field_764 = 0x32;
                        temp_s3->field_766 = 0x10;
                        temp_s3->field_73A = 0;
                        temp_s3->field_73C = -0x64U;
                        temp_s3->field_75E = 0;
                        temp_s3->field_7A4 = 0;
                        temp_s3->field_732 = (s16)((u16)temp_s3->field_732 + 1);
                        return;
                    }
                    break;
                case 3:
                    temp_s3->field_736 = 0x12U;
                    if ((u16)temp_s3->field_73A & 8) {
                        temp_v0_9          = temp_s3->field_73C + 5;
                        temp_s3->field_73C = temp_v0_9;
                        if ((temp_v0_9 << 0x10) > 0) {
                            temp_s3->field_73C = 0U;
                        }
                    }
                    temp_s3->field_74A = 0;
                    if ((s16)temp_s3->field_73C == 0) {
                        temp_s3->field_7AE = 1;
                        temp_s3->field_758 = 0;
                        temp_s3->field_73A = 0;
                        temp_s3->field_732 = (s16)((u16)temp_s3->field_732 + 1);
                        return;
                    }
                    break;
                case 4:
                    temp_s3->field_736 = 0x11U;
                    temp_s3->field_74A = 0;
                    if (temp_s3->field_73A >= 0xA) {
                        temp_s3->field_746 = 2;
                    block_172:
                        func_actor_403600_8013DFE0(arg0);
                    }
                    if (temp_s3->field_73A >= 0x26) {
                        temp_v0_10            = ((Actor403600Work*)arg0->work);
                        temp_v0_10->field_756 = 8;
                        temp_v0_10->field_778 = 0x10;
                        temp_v0_10->field_776 = 0xA;
                        temp_v0_10->field_742 = 0;
                        temp_v0_10->field_746 = 0;
                        temp_v0_10->field_774 = 0;
                        temp_v0_10->field_77A = 0;
                        temp_v0_10->field_784 = 0;
                        temp_v0_10->field_73C = 0U;
                        temp_v0_10->field_73E = 0;
                        temp_v0_10->field_74A = 0;
                        temp_v0_10->field_73A = 0;
                        temp_v0_10->field_76E = 0x40;
                        temp_v0_10->field_75E = 0;
                        temp_v0_10->field_7A4 = 0;
                        temp_v0_10->field_7A6 = 0;
                        temp_v0_10->field_7AC = 0;
                        temp_s3->field_73E    = 1;
                        return;
                    }
                    break;
            }
            break;
        case 0x14:
            temp_v0_11 = temp_s3->field_736 - 6;
            switch (temp_v0_11) {
                case 10:
                    temp_s3->field_784    = 1;
                    temp_s3->field_778    = 0x20;
                    temp_s3->field_736    = 0x10U;
                    temp_s3->field_73C    = 0x14U;
                    temp_s3->field_74A    = 0;
                    temp_s3->field_6F0.vx = (s32)D_actor_403600_801605D4.vx;
                    temp_s3->field_6F0.vy = (s32)D_actor_403600_801605D4.vy;
                    temp_s3->field_6F0.vz = (s32)D_actor_403600_801605D4.vz;
                    temp_s3->field_76E    = 0x40;
                    temp_s3->field_746    = 1;
                    func_actor_403600_8013DFE0(arg0);
                    if (temp_s3->field_73A >= 0x13) {
                        temp_s0_10 = &temp_s3->field_4B8;
                        temp_s4    = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160004;
                        temp_s0_11 = (s8)Gp_GetObjPan(temp_s0_10);
                        temp_v0_12 = gpGetObjDepth(temp_s0_10);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_11, (s32)(((temp_v0_12 >> 0x1F) + temp_v0_12) << 0x17) >> 0x18);
                        temp_s3->field_76E = 0x80;
                        temp_s3->field_732 = 0;
                        temp_s3->field_778 = 0x10;
                        temp_s3->field_73A = 0;
                        temp_s3->field_774 = 1;
                        temp_s3->field_736 = 0x12U;
                        return;
                    }
                    break;
                case 12:
                    temp_s3->field_736 = 0x12U;
                    temp_s3->field_73C = 0x4B0U;
                    temp_s3->field_74A = 0;
                    if (temp_s3->field_732 == 1) {
                        temp_s3->field_6F0.vy = (s32)(Player_Status.coordMtx->t[1] - 0x258);
                    }
                    temp_s3->field_746 = 1;
                    if (func_actor_403600_8013DFE0(arg0) < 0x7D1) {
                        if (temp_s3->field_732 == 0) {
                            temp_s3->field_732 = 1;
                            func_actor_403600_8013CCEC(arg0, 0);
                            return;
                        }
                        temp_s3->field_73C = 0U;
                        temp_s3->field_74A = 0;
                        temp_s3->field_73A = 0;
                        temp_s3->field_736 = 0x11U;
                        var_s0             = &temp_s3->field_4B8;
                        var_a0             = var_s0;
                        temp_s4            = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160004;
                    block_134:
                        temp_s0_12 = (s8)Gp_GetObjPan(var_a0);
                        temp_v0_13 = gpGetObjDepth(var_s0);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_12, (s32)(((temp_v0_13 >> 0x1F) + temp_v0_13) << 0x17) >> 0x18);
                        return;
                    }
                    break;
                case 11:
                    temp_s3->field_736 = 0x11U;
                    temp_s3->field_76E = 0xA0;
                    temp_s3->field_73C = 0U;
                    temp_s3->field_74A = 0;
                    temp_s3->field_746 = 2;
                    func_actor_403600_8013DFE0(arg0);
                    if (temp_s3->field_73A >= 0x26) {
                        temp_s3->field_76E = 0x80;
                        temp_s3->field_784 = 0;
                        temp_s3->field_73C = 0U;
                        temp_s3->field_74A = 0;
                        temp_s3->field_73A = 0;
                        temp_s3->field_774 = 0;
                        temp_s3->field_736 = 6U;
                        return;
                    }
                    break;
                case 0:
                    temp_a2 = temp_s3->field_73A;
                    var_a1  = 0x71350000;
                    if (temp_a2 == 1) {
                        var_a1      = 0x71357911;
                        temp_v0_14  = (Gp_LcgState * 5) + 0x71357911;
                        Gp_LcgState = temp_v0_14;
                        temp_v1_3   = (temp_v0_14 >> 0x10) & 0xF;
                        if (temp_v1_3 < 2) {
                            temp_s3->field_75A = 0;
                        } else if (temp_v1_3 < 5) {
                            temp_s3->field_75A = 2;
                        } else if (temp_v1_3 < 0xA) {
                            temp_s3->field_75A = temp_a2;
                        } else {
                            temp_s3->field_75A = 3;
                        }
                    }
                    if (temp_s3->field_73A == 0x27) {
                        temp_s0_13 = &temp_s3->field_4B8;
                        temp_s4    = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160008;
                        temp_s0_14 = (s8)Gp_GetObjPan(temp_s0_13);
                        temp_v0_15 = gpGetObjDepth(temp_s0_13);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_14, (s32)(((temp_v0_15 >> 0x1F) + temp_v0_15) << 0x17) >> 0x18);
                        Task_SpawnFromTable(&D_actor_403600_801421A0, 1, (s16)((u16)temp_s3->field_75A | 0x10), arg0);
                    }
                    if ((temp_s3->field_73A == 0x2C) || (temp_s3->field_73A == 0x31)) {
                        Task_SpawnFromTable(&D_actor_403600_801421A0, 1, temp_s3->field_75A, arg0);
                    }
                    if (temp_s3->field_73A >= temp_s3->field_734) {
                        SndEvt_EnqueueType7(0x54160008, 1);
                        var_v0_6 = 7;
                    block_189:
                        temp_s3->field_73A = 0;
                        temp_s3->field_736 = var_v0_6;
                        return;
                    }
                    break;
                case 1:
                    if (temp_s3->field_73A >= 0x33) {
                        temp_s3->field_746 = 0;
                        goto block_230;
                    }
                    break;
            }
            break;
        case 0x1E:
            temp_v1_4 = temp_s3->field_732;
            switch (temp_v1_4) {
                case 0:
                    temp_s3->field_736 = 2U;
                    if ((func_actor_403600_80141840(arg0) & 0xFF) == 3) {
                        temp_s3->field_73C = 0U;
                        temp_s3->field_74A = 0;
                        temp_s3->field_73A = 0;
                        if (temp_s3->field_772 == 1) {
                            temp_s3->field_6F0.vx = (s32)((SVECTOR*)&D_actor_403600_8016063C)[(s16)temp_s3->field_770].vx;
                            temp_s3->field_6F0.vy = (s32)(((SVECTOR*)&D_actor_403600_8016063C)[(s16)temp_s3->field_770].vy + 0xFA0);
                            temp_s3->field_6F0.vz = (s32)((SVECTOR*)&D_actor_403600_8016063C)[(s16)temp_s3->field_770].vz;
                        } else if ((u16)temp_s3->field_770 & 2) {
                            temp_s3->field_6F0.vx = (s32)D_actor_403600_8016064C.x1;
                            temp_s3->field_6F0.vy = (s32)D_actor_403600_8016064C.y1;
                            temp_s3->field_6F0.vz = (s32)D_actor_403600_8016064C.z1;
                        } else {
                            temp_s3->field_6F0.vx = (s32)D_actor_403600_8016064C.x0;
                            temp_s3->field_6F0.vy = (s32)D_actor_403600_8016064C.y0;
                            temp_s3->field_6F0.vz = (s32)D_actor_403600_8016064C.z0;
                        }
                        temp_s3->field_732 = (s16)((u16)temp_s3->field_732 + 1);
                        return;
                    }
                    break;
                case 1:
                    temp_s3->field_736 = 1U;
                    temp_v0_16         = (-0x1770 - temp_s3->field_4B8.coord.t[1]) / 25;
                    temp_s3->field_74A = temp_v0_16;
                    if (temp_v0_16 < 0xA) {
                        temp_s3->field_74A = 0;
                        temp_s3->field_73A = 0;
                        temp_s3->field_732 = (s16)((u16)temp_s3->field_732 + 1);
                        return;
                    }
                    break;
                case 2:
                    temp_s3->field_746 = 1;
                    func_actor_403600_8013DDF4(arg0, 0x20);
                    if (temp_s3->field_73A >= 0x28) {
                        temp_s3->field_778 = 0x20;
                        temp_s3->field_74A = 0;
                        temp_s3->field_73A = 0;
                        temp_s3->field_76E = 0xA0;
                        temp_s3->field_732 = (s16)((u16)temp_s3->field_732 + 1);
                        return;
                    }
                    break;
                case 3:
                    temp_s3->field_736 = 0x10U;
                    temp_s3->field_784 = 1;
                    temp_s3->field_73C = 0U;
                    temp_s3->field_74A = 0;
                    func_actor_403600_8013DFE0(arg0);
                    if (temp_s3->field_73A >= 0x13) {
                        temp_s0_15 = &temp_s3->field_4B8;
                        temp_s4    = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x5416000B;
                        temp_s0_16 = (s8)Gp_GetObjPan(temp_s0_15);
                        temp_v0_17 = gpGetObjDepth(temp_s0_15);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_16, (s32)(((temp_v0_17 >> 0x1F) + temp_v0_17) << 0x17) >> 0x18);
                        temp_s3->field_73C = 0x320U;
                        temp_s3->field_778 = 0x10;
                        temp_s3->field_774 = 0;
                        temp_s3->field_734 = 0;
                        temp_s3->field_73A = 0;
                        temp_s3->field_76E = 0x80;
                        temp_s3->field_7A4 = 1;
                        temp_s3->field_732 = (s16)((u16)temp_s3->field_732 + 1);
                        return;
                    }
                    break;
                case 4:
                    temp_s3->field_736 = 0x12U;
                    temp_s3->field_73C = 0x320U;
                    temp_s3->field_74A = 0;
                    temp_s3->field_76E = 0xA0;
                    if (!((u16)temp_s3->field_73A & 1)) {
                        Gp_SpawnEff(0x601B9, &arg0->extra.tmd->coords[1], 0, &D_actor_403600_80160664);
                        Gp_SpawnEff(0x601B9, &arg0->extra.tmd->coords[15], 0x800, NULL);
                        Gp_SpawnEff(0x601B9, &arg0->extra.tmd->coords[19], 0x800, NULL);
                    }
                    temp_s3->field_746 = 1;
                    temp_s1            = func_actor_403600_8013DFE0(arg0);
                    temp_v1_5          = Player_Status.coordMtx->t[0] - temp_s3->field_4B8.coord.t[0];
                    temp_lo            = temp_v1_5 * temp_v1_5;
                    temp_v1_6          = Player_Status.coordMtx->t[1] - temp_s3->field_4B8.coord.t[1];
                    temp_lo_2          = temp_v1_6 * temp_v1_6;
                    temp_v1_7          = Player_Status.coordMtx->t[2] - temp_s3->field_4B8.coord.t[2];
                    temp_v0_18         = SquareRoot0(temp_lo + temp_lo_2 + (temp_v1_7 * temp_v1_7));
                    sp10               = temp_v0_18;
                    if (temp_v0_18 < 0x76DU) {
                        temp_s0_msg = &D_actor_403600_80160568;
                        if (temp_s0_msg->field_4 == 0) {
                            Gp_SpawnPadLerp(0x14, 0xFF, 0x50);
                            temp_s3->field_760 = 0;
                            var_v0_8           = (s16)func_actor_403600_8013E66C(&temp_s3->field_4B8);
                            if (var_v0_8 < 0) {
                                var_v0_8 = -var_v0_8;
                            }
                            if (var_v0_8 >= 0x401) {
                                temp_s3->field_762   = 0x28;
                                temp_s0_msg->field_4 = 4;
                                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, temp_s0_msg, 0);
                            } else {
                                temp_s3->field_762   = -0x28;
                                temp_s0_msg->field_4 = 3;
                                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, temp_s0_msg, 0);
                            }
                            temp_s4    = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 6;
                            temp_s0_17 = (s8)Gp_GetObjPan(temp_s6);
                            temp_v0_19 = gpGetObjDepth(temp_s6);
                            SndEvt_EnqueueType6(temp_s4, temp_s0_17, (s32)(((temp_v0_19 >> 0x1F) + temp_v0_19) << 0x17) >> 0x18);
                            Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F9, Gp_PackPair(&D_actor_403600_80150E9C, 0), 0);
                        }
                    }
                    if (temp_s1 < 0x3E9) {
                        temp_s3->field_778 = 0x10;
                        temp_s3->field_73C = 0U;
                        temp_s3->field_74A = 0;
                        temp_s3->field_746 = 0;
                        temp_s3->field_734 = 0;
                        temp_s3->field_73A = 0;
                        temp_s3->field_7A4 = 0;
                        temp_s3->field_7AE = 1;
                        temp_s3->field_758 = 0;
                        temp_s3->field_732 = (s16)((u16)temp_s3->field_732 + 1);
                        return;
                    }
                    break;
                case 5:
                    temp_s3->field_736 = 0x11U;
                    temp_s3->field_74A = 0;
                    temp_s3->field_746 = 2;
                    temp_s3->field_76E = 0x40;
                    func_actor_403600_8013DFE0(arg0);
                    if (temp_s3->field_73A >= 0x26) {
                        temp_v0_20            = ((Actor403600Work*)arg0->work);
                        temp_v0_20->field_756 = 8;
                        temp_v0_20->field_778 = 0x10;
                        temp_v0_20->field_742 = 0;
                        temp_v0_20->field_746 = 0;
                        temp_v0_20->field_774 = 0;
                        temp_v0_20->field_77A = 0;
                        temp_v0_20->field_784 = 0;
                        temp_v0_20->field_73C = 0U;
                        temp_v0_20->field_73E = 0;
                        temp_v0_20->field_74A = 0;
                        temp_v0_20->field_73A = 0;
                        temp_v0_20->field_776 = 0xA;
                        temp_v0_20->field_76E = 0x40;
                        temp_v0_20->field_75E = 0;
                        temp_v0_20->field_7A4 = 0;
                        temp_v0_20->field_7A6 = 0;
                        temp_v0_20->field_7AC = 0;
                        temp_s3->field_73E    = 1;
                        return;
                    }
                    break;
            }
            break;
        case 0x28:
            temp_v1_8 = temp_s3->field_732;
            switch (temp_v1_8) {
                case 0:
                    temp_s3->field_746 = 1;
                    temp_s3->field_73C = 0xC8U;
                    adjusted_y0        = Player_Status.coordMtx->t[1] + 0x1F4;
                    temp_s3->field_74A = (s16)((adjusted_y0 - temp_s3->field_4B8.coord.t[1]) / 25);
                    if (func_actor_403600_8013DDF4(arg0, 0xB0) < 0x3E9) {
                        temp_s3->field_73C = 0U;
                        temp_s3->field_74A = 0;
                        temp_s3->field_73A = 0;
                        temp_s3->field_732 = (s16)((u16)temp_s3->field_732 + 1);
                        return;
                    }
                    break;
                case 1:
                    temp_s3->field_746 = 0;
                    func_actor_403600_8013DDF4(arg0, 0x20);
                    temp_s3->field_73C = 0U;
                    temp_s3->field_74A = 0;
                    func_actor_403600_8013E470(&temp_s3->field_4B8, &sp10, &sp14);
                    if (temp_s3->field_73A >= 0x32) {
                        temp_s3->field_73A = 0;
                        temp_s3->field_732 = (s16)((u16)temp_s3->field_732 + 1);
                        return;
                    }
                    break;
                case 2:
                    D_80181A48(arg0);
                    temp_s3->field_736 = 0x13U;
                    if (temp_s3->field_73A == 0x19) {
                        temp_s0_18 = &temp_s3->field_4B8;
                        temp_s4    = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160015;
                        temp_s0_19 = (s8)Gp_GetObjPan(temp_s0_18);
                        temp_v0_21 = gpGetObjDepth(temp_s0_18);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_19, (s32)(((temp_v0_21 >> 0x1F) + temp_v0_21) << 0x17) >> 0x18);
                    }
                    if (temp_s3->field_73A < 0x14) {
                        temp_s3->field_6F0.vx = (s32)Player_Status.coordMtx->t[0];
                        temp_s3->field_6F0.vy = (s32)(Player_Status.coordMtx->t[1] - 0x3E8);
                        temp_s3->field_6F0.vz = (s32)Player_Status.coordMtx->t[2];
                    }
                    if (temp_s3->field_73A >= 0x32) {
                        temp_s3->field_736 = 0x10U;
                        temp_s3->field_746 = 1;
                        temp_s3->field_778 = 0x20;
                        temp_s3->field_73A = 0;
                        temp_s3->field_76E = 0xA0;
                        temp_s3->field_732 = (s16)((u16)temp_s3->field_732 + 1);
                        return;
                    }
                    break;
                case 3:
                    state16 = 0x10;
                    SOFT_TOUCH_REG(state16);
                    temp_s3->field_784 = 1;
                    temp_s3->field_736 = state16;
                    temp_s3->field_73C = 0U;
                    temp_s3->field_74A = 0;
                    func_actor_403600_8013DFE0(arg0);
                    if (temp_s3->field_73A >= 0x13) {
                        temp_s3->field_73C = 0x320U;
                        temp_s3->field_76E = 0xA0;
                        temp_s3->field_70A = 0x7000;
                        temp_s3->field_736 = 0x12U;
                        temp_s3->field_778 = state16;
                        temp_s3->field_746 = 1;
                        temp_s3->field_734 = 0;
                        temp_s3->field_73A = 0;
                        temp_s3->field_75E = 0;
                        temp_s3->field_786 = 0;
                        temp_s3->field_792 = 0x96;
                        temp_s3->field_7A4 = 1;
                        temp_s3->field_732 = (s16)((u16)temp_s3->field_732 + 1);
                        temp_sound_28      = &temp_s3->field_4B8;
                        temp_s4            = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x5416000B;
                        temp_pan_28        = (s8)Gp_GetObjPan(temp_sound_28);
                        temp_depth_28      = gpGetObjDepth(temp_sound_28);
                        SndEvt_EnqueueType6(temp_s4, temp_pan_28, (s32)(((temp_depth_28 >> 0x1F) + temp_depth_28) << 0x17) >> 0x18);
                        return;
                    }
                    break;
                case 4:
                    if (!((u16)temp_s3->field_73A & 1)) {
                        if (temp_s3->field_734 != 0xFF) {
                            Gp_SpawnEff(0x601B9, &arg0->extra.tmd->coords[1], 0, &D_actor_403600_80160664);
                            Gp_SpawnEff(0x601B9, &arg0->extra.tmd->coords[15], 0x800, NULL);
                            Gp_SpawnEff(0x601B9, &arg0->extra.tmd->coords[19], 0x800, NULL);
                        }
                    }
                    temp_s7->node.state.b.flags = 8;
                    temp_s3->field_736          = 0x12U;
                    func_actor_403600_801417A8(arg0, 0xA);
                    temp_s0_20 = &temp_s3->field_4B8;
                    func_actor_403600_8013E470(temp_s0_20, &sp10, &sp14);
                    if (sp10 < 0x5DDU) {
                        temp_s1_3 = &D_actor_403600_80160568;
                        if (temp_s1_3->field_4 == 0) {
                            if (temp_s3->field_734 == 0xFF) {
                                Gp_SpawnPadLerp(0xA, 0xFF, 0x50);
                                D_actor_403600_801606A4.field_2 = 0xA;
                                temp_s1_3->field_4              = 1;
                                D_actor_403600_801606A4.field_0 = (u16)D_actor_403600_80150EAC;
                                func_actor_403600_8013E7D4((s32)arg0, 1);
                                temp_s3->field_762 = -0x190;
                                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, temp_s1_3, 0);
                                temp_s3->field_760 = 0;
                                temp_s4            = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 6;
                                temp_s0_21         = (s8)Gp_GetObjPan(temp_s6);
                                temp_v0_22         = gpGetObjDepth(temp_s6);
                                SndEvt_EnqueueType6(temp_s4, temp_s0_21, (s32)(((temp_v0_22 >> 0x1F) + temp_v0_22) << 0x17) >> 0x18);
                                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F9, Gp_PackPair(&D_actor_403600_801606A4, 0), 0);
                            } else {
                                Gp_SpawnPadLerp(0x14, 0xB0, 0x50);
                                temp_s3->field_760 = 0;
                                var_v0_10          = (s16)func_actor_403600_8013E66C(temp_s0_20);
                                if (var_v0_10 < 0) {
                                    var_v0_10 = -var_v0_10;
                                }
                                if (var_v0_10 >= 0x401) {
                                    temp_s3->field_762 = 0x28;
                                    temp_s1_3->field_4 = 4;
                                    Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, temp_s1_3, 0);
                                } else {
                                    temp_s3->field_762 = -0x28;
                                    temp_s1_3->field_4 = 3;
                                    Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, temp_s1_3, 0);
                                }
                                temp_s4    = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 6;
                                temp_s0_22 = (s8)Gp_GetObjPan(temp_s6);
                                temp_v0_23 = gpGetObjDepth(temp_s6);
                                SndEvt_EnqueueType6(temp_s4, temp_s0_22, (s32)(((temp_v0_23 >> 0x1F) + temp_v0_23) << 0x17) >> 0x18);
                                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F9, Gp_PackPair(&D_actor_403600_80150E9C, 4), 0);
                                temp_s3->field_734 = (s16)temp_s3->field_782;
                            }
                            temp_s3->field_792 = 0x96;
                        }
                    }
                    temp_s1_2 = func_actor_403600_8013D9A8(arg0) & 0xFF;
                    if (temp_s1_2 == 2) {
                        Task_SpawnFromTable(&D_actor_403600_801421A0, 3, 0, arg0);
                    }
                    if ((temp_s1_2 == 3) && (temp_s3->field_734 == 0xFF)) {
                        temp_s3->field_732 = 6;
                        temp_s3->field_7A4 = 0;
                        temp_s3->field_75E = 0;
                        temp_s3->field_7AE = 1;
                        temp_s3->field_758 = 0;
                    }
                    if (temp_s1_2 == 1) {
                        temp_s3->field_7A4 = 0;
                        temp_s3->field_7A6 = temp_s1_2;
                        temp_s4            = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x5416000E;
                        temp_s0_23         = (s8)Gp_GetObjPan(temp_s6);
                        temp_v0_24         = gpGetObjDepth(temp_s6);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_23, (s32)(((temp_v0_24 >> 0x1F) + temp_v0_24) << 0x17) >> 0x18);
                        Task_SpawnFromTable(&D_actor_403600_801421A0, 3, 1, arg0);
                        temp_s3->field_776       = 0;
                        temp_s3->field_5C0.flags = (u16)(temp_s3->field_5C0.flags & 0xBFFF);
                        __asm__("addiu %0,$zero,1" : "=r"(node_flag) : "r"(temp_s1_2));
                        temp_s7->node.state.b.flags = node_flag;
                        Gp_ClearNodeSlots(&temp_s7->node);
                        temp_s3->field_73A = 0;
                        temp_s3->field_732 = 5;
                        temp_s3->field_794 = (u16)temp_s3->field_792;
                        return;
                    }
                    break;
                case 5:
                    func_actor_403600_801417A8(arg0, 0xA);
                    temp_v0_25         = (u16)temp_s3->field_776 + 1;
                    temp_s3->field_776 = temp_v0_25;
                    if (temp_v0_25 == 8) {
                        temp_s3->field_73C = 0U;
                    }
                    if (temp_s3->field_776 == (s16)temp_s3->field_794) {
                        SndEvt_EnqueueType6(0x5416000F, 0, 0);
                        func_actor_403600_8013C864(arg0);
                        temp_s3->field_746 = 3;
                        func_actor_403600_8013DFE0(arg0);
                        Task_SpawnFromTable(&D_actor_403600_801421A0, 3, 2, arg0);
                    }
                    temp_v0_26 = Player_Status.coordMtx->t[0] - temp_s3->field_4B8.coord.t[0];
                    temp_lo_3  = temp_v0_26 * temp_v0_26;
                    temp_v0_27 = Player_Status.coordMtx->t[1] - temp_s3->field_4B8.coord.t[1];
                    temp_lo_4  = temp_v0_27 * temp_v0_27;
                    temp_v0_28 = Player_Status.coordMtx->t[2] - temp_s3->field_4B8.coord.t[2];
                    var_a0_2   = SquareRoot0(temp_lo_3 + temp_lo_4 + (temp_v0_28 * temp_v0_28));
                    sp10       = var_a0_2;
                    if (temp_s3->field_73A >= 8) {
                        rumble_distance = var_a0_2;
                        TOUCH_REG_USE(rumble_distance, var_a0_2);
                        temp_s3->field_73A = 0;
                        if (rumble_distance < 0x3E9U) {
                            var_a0_2 = 5;
                            var_a1_2 = 0xB0;
                            goto block_168_call;
                        }
                        if (rumble_distance < 0x7D1U) {
                            var_a1_2 = 0x80;
                            var_a0_2 = 5;
                            goto block_168_call;
                        }
                        if (var_a0_2 < 0xBB9U) {
                            var_a0_2 = 5;
                            var_a1_2 = 0x50;
                        block_168_call:
                            Gp_SpawnPadLerpWide(var_a0_2, var_a1_2, var_a1_2);
                        }
                    }
                block_168:
                    if (temp_s3->field_776 >= ((s16)temp_s3->field_794 + 0x1E)) {
                        SndEvt_EnqueueType7(0x5416000F, 1);
                        SndEvt_EnqueueType6(0x54160010, 0, 0);
                        temp_s3->field_73C = 0x320U;
                        temp_s3->field_76E = 0xA0;
                        temp_s3->field_732 = 4;
                        temp_s3->field_73A = 0;
                        temp_s3->field_786 = 0;
                        temp_s3->field_7A4 = 1;
                        temp_s3->field_7A6 = 0;
                        return;
                    }
                    break;
                case 6:
                    temp_s3->field_736 = 0x11U;
                    temp_s3->field_70A = 0;
                    temp_s3->field_73C = 0U;
                    temp_s3->field_746 = 2;
                    func_actor_403600_8013DFE0(arg0);
                    if (temp_s3->field_73A >= 0x26) {
                        temp_reset_30            = ((Actor403600Work*)arg0->work);
                        temp_reset_30->field_756 = 8;
                        temp_reset_30->field_778 = 0x10;
                        temp_reset_30->field_742 = 0;
                        temp_reset_30->field_746 = 0;
                        temp_reset_30->field_774 = 0;
                        temp_reset_30->field_77A = 0;
                        temp_reset_30->field_784 = 0;
                        temp_reset_30->field_73C = 0U;
                        temp_reset_30->field_73E = 0;
                        temp_reset_30->field_74A = 0;
                        temp_reset_30->field_73A = 0;
                        temp_reset_30->field_776 = 0xA;
                        temp_reset_30->field_76E = 0x40;
                        temp_reset_30->field_75E = 0;
                        temp_reset_30->field_7A4 = 0;
                        temp_reset_30->field_7A6 = 0;
                        temp_reset_30->field_7AC = 0;
                        temp_s3->field_73E       = 1;
                        return;
                    }
                    break;
            }
            break;
        case 0x32:
            temp_v1_9 = (s16)temp_s3->field_736;
            switch (temp_v1_9) {
                case 2:
                    temp_s3->field_746 = 0;
                    func_actor_403600_8013DDF4(arg0, 0xA0);
                    temp_s3->field_73C = 0x12CU;
                    adjusted_y1        = Player_Status.coordMtx->t[1] + 0x1F4;
                    temp_s3->field_74A = (s16)((adjusted_y1 - temp_s3->field_4B8.coord.t[1]) / 25);
                    func_actor_403600_8013E470(&temp_s3->field_4B8, &sp10, &sp14);
                    if ((sp10 < 0x1389U) && (temp_s3->field_74A < 0x12D)) {
                        temp_s3->field_73C = 0U;
                        temp_s3->field_74A = 0;
                        temp_s3->field_73A = 0;
                        temp_s3->field_736 = 6U;
                        Gp_SpawnEff(0x601BC, &arg0->extra.tmd->coords[1], temp_s3->field_734, NULL);
                        return;
                    }
                    break;
                case 6:
                    temp_s3->field_73C = 0U;
                    temp_s3->field_74A = 0;
                    if (temp_s3->field_73A == 1) {
                        temp_s0_24 = &temp_s3->field_4B8;
                        temp_s4    = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160014;
                        temp_s0_25 = (s8)Gp_GetObjPan(temp_s0_24);
                        temp_v0_30 = gpGetObjDepth(temp_s0_24);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_25, (s32)(((temp_v0_30 >> 0x1F) + temp_v0_30) << 0x17) >> 0x18);
                    }
                    if (temp_s3->field_73A >= temp_s3->field_734) {
                        temp_s3->field_73A = 0;
                        temp_s3->field_736 = 8;
                        return;
                    }
                    break;
                case 8:
                    if ((temp_s3->field_73A == 1) && (temp_s3->field_4B4 == 0)) {
                        temp_s3->field_4B4 = Gp_SpawnEnemyFromTable(&D_actor_403600_80160514, 1, Gp_NodeSlotMask(&temp_s7->node), 0);
                    }
                    if (temp_s3->field_73A >= 0x1E) {
                        temp_s3->field_73C = -0xAU;
                        temp_s3->field_74A = -0x14;
                    }
                    if (temp_s3->field_73A >= 0x42) {
                        temp_s3->field_73A = 0;
                        temp_s3->field_736 = 3U;
                        case 3:
                    }
                    temp_s3->field_73C = -0x14U;
                    temp_s3->field_74A = -0x1E;
                    if (temp_s3->field_73A >= 0x1E) {
                        temp_s3->field_73C = 0U;
                        temp_s3->field_74A = 0;
                        temp_s3->field_73E = 0;
                        temp_s3->field_73A = 0;
                        temp_s3->field_776 = 5;
                        return;
                    }
                    break;
            }
            break;
        case 0x3C:
            temp_v1_10 = (s16)temp_s3->field_736;
            switch (temp_v1_10) {
                case 2:
                    temp_s3->field_6F0.vx = 0x1F40;
                    temp_s3->field_6F0.vy = -0x1B58;
                    temp_s3->field_6F0.vz = 0x1900;
                    if ((func_actor_403600_80141840(arg0) & 0xFF) == 3) {
                        temp_s3->field_76A = 0x100;
                        temp_s3->field_768 = 0;
                        temp_s3->field_76C = 0x32;
                        temp_s3->field_758 = 0;
                        temp_s3->field_73C = 0U;
                        temp_s3->field_74A = 0;
                        temp_s3->field_73A = 0;
                        temp_s3->field_736 = 6U;
                        temp_s3->field_79C = (u16)temp_wip->mp;
                        return;
                    }
                    break;
                case 6:
                    if (temp_s3->field_73A >= 0x2D) {
                        func_actor_403600_80141B60(arg0);
                        temp_a3            = (u16)temp_s3->field_73A;
                        temp_s3->field_708 = (u16)(temp_s3->field_708 + (0xC00 / (s16)temp_s3->field_734));
                        if (temp_s3->field_73A == 0x30) {
                            Gp_SpawnPadLerp((s16)(((u16)temp_s3->field_734 - temp_a3) + 0x23), 0x40, 0xFF);
                            SndEvt_EnqueueType6(0x54160001, 0, 0);
                        }
                    }
                    func_actor_403600_8013F608(arg0);
                    if (temp_s3->field_73A >= temp_s3->field_734) {
                        temp_s3->field_73A = 0;
                        temp_s3->field_736 = 0xAU;
                    }
                    if (temp_s3->field_758 < 0xC8) {
                        return;
                    }
                    goto block_229;
                case 10:
                    func_actor_403600_8013F608(arg0);
                    temp_v1_11 = temp_s3->field_73A;
                    if (temp_v1_11 == 0x23) {
                        Gp_SpawnPadLerp(0xA, 0xFF, 0xFF);
                        temp_s3->field_708 = 0x1000U;
                        temp_s4            = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 6;
                        temp_s0_26         = (s8)Gp_GetObjPan(temp_s6);
                        temp_v0_31         = gpGetObjDepth(temp_s6);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_26, (s32)(((temp_v0_31 >> 0x1F) + temp_v0_31) << 0x17) >> 0x18);
                        Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F9, Gp_PackPair(&D_actor_403600_80150E9C, 3), 0);
                        D_actor_403600_80160568.field_4 = 4;
                        Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, &D_actor_403600_80160568, 0);
                        temp_v0_32   = temp_wip->mp - 0xFB;
                        temp_wip->mp = temp_v0_32;
                        if ((temp_v0_32 << 0x10) <= 0) {
                            temp_wip->mp = 0U;
                        }
                        temp_s3->field_76C = -1;
                        temp_s3->field_760 = 0;
                        temp_s3->field_762 = 0x28;
                        temp_s0_27         = Gp_ActorSlots[0]->extra.tmd->coords;
                        temp_s4            = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160003;
                        temp_s0_28         = (s8)Gp_GetObjPan(temp_s0_27);
                        temp_v0_33         = gpGetObjDepth(temp_s0_27);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_28, (s32)(((temp_v0_33 >> 0x1F) + temp_v0_33) << 0x17) >> 0x18);
                        SndEvt_EnqueueType7(0x54160001, 1);
                    } else if (temp_v1_11 < 0x23) {
                        func_actor_403600_80141B60(arg0);
                    }
                    temp_v0_34         = temp_s3->field_708 + 0xE;
                    temp_s3->field_708 = temp_v0_34;
                    if ((s16)temp_v0_34 >= 0x1000) {
                        temp_s3->field_708 = 0x1000U;
                    }
                    if (temp_s3->field_73A >= 0x45) {
                        if ((s16)temp_wip->mp <= 0) {
                            temp_wip->mp = 0U;
                        }
                        temp_s3->field_708 = 0U;
                        temp_s3->field_73E = 0;
                        temp_s3->field_73A = 0;
                    }
                    if ((temp_s3->field_758 >= 0xC8) && (temp_s3->field_73A < 0x32)) {
                        goto block_229;
                    }
                    break;
                block_229:
                    func_actor_403600_80141B24(arg0);
                    temp_s3->field_73C = 0U;
                    temp_s3->field_74A = 0;
                block_230:
                    temp_s3->field_73E = 0;
                    temp_s3->field_73A = 0;
                    return;
            }
            break;
        case 0x46:
            temp_v1_12 = (s16)temp_s3->field_736;
            switch (temp_v1_12) {
                case 2:
                    temp_s3->field_746 = 0;
                    func_actor_403600_8013DDF4(arg0, 0x20);
                    func_actor_403600_8013E470(&temp_s3->field_4B8, &sp10, &sp14);
                    if (sp10 < 0x7D1U) {
                        temp_s3->field_73C = 0U;
                    } else {
                        temp_s3->field_73C = 0x50U;
                    }
                    adjusted_y2        = temp_s3->field_4B8.coord.t[1] + 0x3E8;
                    temp_a0_2          = (Player_Status.coordMtx->t[1] - adjusted_y2) / 25;
                    temp_s3->field_74A = temp_a0_2;
                    if (sp10 < 0x7D1U) {
                        var_v0_12 = sp14;
                        if (var_v0_12 < 0) {
                            var_v0_12 = -var_v0_12;
                        }
                        if (var_v0_12 < 0x200) {
                            var_v0_13 = temp_a0_2;
                            if (var_v0_13 < 0) {
                                var_v0_13 = -var_v0_13;
                            }
                            if (var_v0_13 < 0x28) {
                                temp_s3->field_73C = 0U;
                                temp_s3->field_74A = 0;
                                temp_s3->field_73A = 0;
                                temp_s3->field_736 = 0xCU;
                                temp_s3->field_756 = 0;
                            }
                        }
                    }
                    temp_v0_35         = temp_s3->field_7B2 + 1;
                    temp_s3->field_7B2 = temp_v0_35;
                    if (((s16)temp_v0_35 >= 0x5A) || (sp10 >= 0xFA0U)) {
                        temp_s3->field_756 = 8;
                        temp_s3->field_73C = 0U;
                        temp_s3->field_73A = 0;
                        temp_s3->field_73E = 0;
                        temp_s3->field_7B4 = 1;
                        return;
                    }
                    break;
                case 12:
                    temp_s3->field_736 = (u16)temp_v1_12;
                    if (temp_s3->field_73A == 0xE) {
                        temp_s0_29 = &temp_s3->field_4B8;
                        temp_s4    = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x5416000D;
                        temp_s0_30 = (s8)Gp_GetObjPan(temp_s0_29);
                        temp_v0_36 = gpGetObjDepth(temp_s0_29);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_30, (s32)(((temp_v0_36 >> 0x1F) + temp_v0_36) << 0x17) >> 0x18);
                    }
                    if (temp_s3->field_73A == 0x11) {
                        temp_s3->field_588.flags = (u16)(temp_s3->field_588.flags | 0x8000);
                    }
                    if (temp_s3->field_73A == 0x15) {
                        temp_s3->field_588.flags = (u16)(temp_s3->field_588.flags & 0x7FFF);
                    }
                    if (temp_s3->field_73A >= 0x1E) {
                        temp_s3->field_73C = 0U;
                        temp_s3->field_73A = 0;
                        temp_s3->field_736 = 0xDU;
                        func_actor_403600_8013E470(&temp_s3->field_4B8, &sp10, &sp14);
                        var_v0_14 = sp10 < 0x7D0U;
                        goto block_266;
                    }
                    break;
                case 13:
                    temp_s3->field_736 = (u16)temp_v1_12;
                    if (temp_s3->field_73A == 9) {
                        temp_s0_31 = &temp_s3->field_4B8;
                        temp_s4    = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x5416000D;
                        temp_s0_32 = (s8)Gp_GetObjPan(temp_s0_31);
                        temp_v0_37 = gpGetObjDepth(temp_s0_31);
                        SndEvt_EnqueueType6(temp_s4, temp_s0_32, (s32)(((temp_v0_37 >> 0x1F) + temp_v0_37) << 0x17) >> 0x18);
                    }
                    if (temp_s3->field_73A == 0xA) {
                        temp_s3->field_588.flags = (u16)(temp_s3->field_588.flags | 0x8000);
                    }
                    if (temp_s3->field_73A == 0xE) {
                        temp_s3->field_588.flags = (u16)(temp_s3->field_588.flags & 0x7FFF);
                    }
                    var_v0_14 = temp_s3->field_73A < 0x23;
                block_266:
                    if (var_v0_14 == 0) {
                        temp_s3->field_756 = 8;
                        temp_s3->field_73C = 0U;
                        temp_s3->field_73A = 0;
                        temp_s3->field_73E = 0;
                    }
            }
            break;
    }
}

void func_actor_403600_8013C864(Task* arg0)
{
    SVECTOR*                  var_s2;
    s16                       temp_a1;
    s16                       temp_v1;
    s32                       var_v0_3;
    u16                       var_v0_4;
    s32                       temp_a0;
    s32                       temp_d_x;
    s32                       temp_early_z;
    s32                       temp_z0;
    s32                       temp_z1;
    s32                       temp_v0;
    s32                       var_v0;
    s32                       var_v0_2;
    u16                       temp_s0_2;
    Actor403600TurnMatrix*    temp_s0;
    Actor403600TurnMatrix*    temp_s0_3;
    Actor403600Work*          temp_s3;
    Actor403600TargetScratch* temp_s4;
    Actor403600TargetScratch* temp_s5;
    Actor403600TargetScratch* temp_s6;
    u8*                       temp_s1;

    temp_s3 = arg0->work;
    if (temp_s3->field_734 == temp_s3->field_782) {
        __asm__ volatile(
            "lui %0, %%hi(D_actor_403600_801605D4);"
            "lh $4, %%lo(D_actor_403600_801605D4)(%0);"
            "li $2, -0x960;"
            "sw $2, 0x4D4(%1);"
            "sw $4, 0x4D0(%1);"
            "addiu $4, %0, %%lo(D_actor_403600_801605D4);"
            "lh $2, 4($4);"
            "nop;"
            "sw $2, 0x4D8(%1);"
            "lh %0, %%lo(D_actor_403600_801605D4)(%0);"
            "li $2, -0x1F40;"
            "sw $2, 0x6F4(%1);"
            "sw %0, 0x6F0(%1);"
            "lh %0, 4($4);"
            "li $2, 0xFF;"
            "sh $2, 0x734(%1)"
            : "=&r"(temp_early_z)
            : "r"(temp_s3)
            : "$2", "$4", "memory");
        temp_s3->field_6F0.vz = temp_early_z;
        return;
    }
    temp_s4 = SCRATCH_HEAD(Actor403600TargetScratch);
    SOFT_USE_REG(temp_s4);
    temp_s6                                = (temp_s4 - 1);
    SCRATCH_HEAD(Actor403600TargetScratch) = temp_s6;
    temp_s5                                = temp_s6;
    if (!((u16)temp_s3->field_734 & 1)) {
        (temp_s4 - 1)->vector.vx = (s16)(Player_Status.coordMtx->t[0] - (u16)D_actor_403600_801605D4.vx);
        temp_a1                  = Player_Status.coordMtx->t[2] - (u16)D_actor_403600_801605D4.vz;
        temp_s6->vector.vz       = temp_a1;
        temp_v0                  = ratan2((temp_s4 - 1)->vector.vx, temp_a1);
        SOFT_TOUCH_REG(temp_v0);
        var_v0 = temp_v0;
        if (temp_v0 < 0) {
            SOFT_TOUCH_REG(var_v0);
            var_v0 = -var_v0;
        }
        temp_s6->angle = temp_v0;
        if (var_v0 >= 0x801) {
            var_v0_2 = temp_v0 - 0x1000;
            if (temp_v0 <= 0) {
                var_v0_2 = 0x1000 - temp_v0;
            }
            __asm__ volatile("sw %0, 40(%1)" : : "r"(var_v0_2), "r"(temp_s5) : "memory");
        }
        __asm__ volatile(
            "lui $2, %%hi(Player_Status + 4)\n\t"
            "lw $3, %%lo(Player_Status + 4)($2)\n\t"
            "nop\n\t"
            "lw $2, 20($3)\n\t"
            "nop\n\t"
            "addiu $2, $2, -3500\n\t"
            "sltiu $2, $2, 8501\n\t"
            ".word 0x1040000C\n\t"
            "lui %0, %%hi(D_actor_403600_801605D4)\n\t"
            "lw $2, 28($3)\n\t"
            "nop\n\t"
            "addiu $2, $2, -2000\n\t"
            "sltiu $2, $2, 10001\n\t"
            ".word 0x10400007\n\t"
            "addiu %1, %0, %%lo(D_actor_403600_801605D4)\n\t"
            "lw $2, 40(%2)\n\t"
            "nop\n\t"
            "subu $2, $0, $2\n\t"
            "sw $2, 40(%2)\n\t"
            "lui %0, %%hi(D_actor_403600_801605D4)\n\t"
            "addiu %1, %0, %%lo(D_actor_403600_801605D4)"
            : "=&r"(temp_s1), "=&r"(var_s2)
            : "r"(temp_s5)
            : "$2", "$3", "memory");
        temp_s5->vector.vx      = 0;
        temp_s5->vector.vy      = 0;
        temp_s0                 = &temp_s5->matrix;
        temp_s5->vector.vz      = (s16)(0x3A98 - var_s2->vz);
        temp_s5->matrix.field_0 = 0x1000;
        temp_s0->field_4        = 0;
        temp_s0->field_8        = 0x1000;
        temp_s0->field_C        = 0;
        temp_s0->field_10       = 0x1000;
        func_8004BFF8(temp_s5->angle, (MATRIX*)temp_s0);
        gte_SetRotMatrix(temp_s0);
        gte_ldv0(temp_s5);
        SOFT_USE_REG(temp_s1);
        gte_rtv0();
        gte_stsv(temp_s5);
        actor_403600_d_x(temp_d_x, temp_s1);
        temp_s3->field_4B8.coord.t[0] = (s32)(temp_s5->vector.vx + temp_d_x);
        temp_s3->field_4B8.coord.t[1] = (s32)(Player_Status.coordMtx->t[1] - 0x3E8);
        temp_s3->field_4B8.coord.t[2] = (s32)(temp_s5->vector.vz + (s16)var_s2->vz);
        temp_a0                       = (s32)Player_Status.coordMtx->t[0];
        if ((u32)(temp_a0 - 0xDAC) < 0x2135U) {
            if ((u32)((s32)Player_Status.coordMtx->t[2] - 0x7D0) < 0x2711U) {
                temp_s3->field_6F0.vx = temp_a0;
                temp_s3->field_6F0.vy = (s32)(Player_Status.coordMtx->t[1] - 0x3E8);
                temp_s3->field_6F0.vz = (s32)Player_Status.coordMtx->t[2];
            } else {
                goto block_17;
            }
        } else {
        block_17:
            temp_s3->field_6F0.vx = (s32)D_actor_403600_801605D4.vx;
            temp_s3->field_6F0.vy = (s32)(Player_Status.coordMtx->t[1] - 0x3E8);
            temp_s3->field_6F0.vz = (s32)D_actor_403600_801605D4.vz;
        }
        temp_s0_2 = (u16)temp_s5->angle;
        temp_v1   = temp_s0_2 + 0x800;
        SOFT_TOUCH_REG(temp_v1);
        SOFT_TOUCH_REG(temp_v1);
        var_v0_3 = temp_v1;
        if (temp_v1 < 0) {
            SOFT_TOUCH_REG(var_v0_3);
            var_v0_3 = -var_v0_3;
        }
        temp_s3->field_780 = temp_v1;
        if (var_v0_3 >= 0x801) {
            var_v0_4 = temp_s0_2 - 0x800;
            if (temp_v1 <= 0) {
                var_v0_4 = 0x1000 - temp_v1;
            }
            temp_s3->field_780 = var_v0_4;
        }
        temp_s3->field_792 = 0xA;
        temp_s3->field_734 = (s16)((u16)temp_s3->field_734 + 1);
    } else {
        (temp_s4 - 1)->vector.vx = 0;
        temp_s6->vector.vy       = 0;
        temp_s0_3                = &(temp_s4 - 1)->matrix;
        temp_s6->vector.vz       = (s16)(0x3A98 - (u16)D_actor_403600_801605D4.vz);
        temp_s0_3->field_0       = 0x1000;
        temp_s0_3->field_4       = 0;
        temp_s0_3->field_8       = 0x1000;
        temp_s0_3->field_C       = 0;
        temp_s0_3->field_10      = 0x1000;
        func_8004BFF8((s32)temp_s3->field_780, (MATRIX*)temp_s0_3);
        gte_SetRotMatrix(temp_s0_3);
        gte_ldv0(temp_s5);
        gte_rtv0();
        gte_stsv(temp_s5);
        temp_s3->field_4B8.coord.t[0] = (s32)((temp_s4 - 1)->vector.vx + D_actor_403600_801605D4.vx);
        temp_s3->field_4B8.coord.t[1] = (s32)(Player_Status.coordMtx->t[1] - 0x3E8);
        temp_z0                       = (s32)temp_s6->vector.vz;
        temp_z1                       = (s32)D_actor_403600_801605D4.vz;
        temp_s3->field_792            = 0x96;
        temp_s3->field_734            = (s16)((u16)temp_s3->field_734 + 1);
        temp_s3->field_4B8.coord.t[2] = temp_z0 + temp_z1;
    }
    SCRATCH_POP_BYTES(0x2C);
}

void func_actor_403600_8013CCEC(Task* arg0, s32 arg1)
{
    s32                         temp_a0;
    s32                         temp_v0;
    s32                         temp_v0_10;
    s32                         temp_v0_2;
    s32                         temp_v0_3;
    s32                         temp_v0_4;
    s32                         temp_v0_5;
    s32                         temp_v0_6;
    s32                         temp_v0_7;
    s32                         temp_v0_8;
    s32                         temp_v0_9;
    s32                         temp_v1;
    s32                         temp_v1_2;
    s32                         temp_v1_3;
    s32                         temp_v1_4;
    s32                         temp_v1_5;
    s32                         temp_v1_6;
    s32                         temp_v1_7;
    s32                         temp_v1_8;
    s32                         temp_v1_9;
    s32                         var_a1;
    s32                         var_a2;
    s32                         var_v1;
    Actor403600DistanceScratch* temp_s2;
    Actor403600DistanceScratch* temp_s3;
    Actor403600DistanceScratch* temp_s5;
    Actor403600Work*            temp_s4;
    Actor403600Point*           temp_v0_11;
    Actor403600Point*           temp_v1_10;

    temp_s2            = SCRATCH_HEAD(void);
    temp_s3            = temp_s2 - 1;
    SCRATCH_HEAD(void) = temp_s3;
    temp_s4            = arg0->work;
    temp_s5            = temp_s3;
    if (arg1 == 0) {
        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[0].x;
        temp_v1               = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[0].z;
        temp_s3->z            = temp_v1;
        temp_v0               = temp_s3->x;
        temp_s3->distances[0] = SquareRoot0((temp_v0 * temp_v0) + (temp_v1 * temp_v1));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[1].x;
        temp_v1_2             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[1].z;
        temp_s3->z            = temp_v1_2;
        temp_v0_2             = temp_s3->x;
        temp_s3->distances[1] = SquareRoot0((temp_v0_2 * temp_v0_2) + (temp_v1_2 * temp_v1_2));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[2].x;
        temp_v1_3             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[2].z;
        temp_s3->z            = temp_v1_3;
        temp_v0_3             = temp_s3->x;
        temp_s3->distances[2] = SquareRoot0((temp_v0_3 * temp_v0_3) + (temp_v1_3 * temp_v1_3));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[3].x;
        temp_v1_4             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[3].z;
        temp_s3->z            = temp_v1_4;
        temp_v0_4             = temp_s3->x;
        temp_s3->distances[3] = SquareRoot0((temp_v0_4 * temp_v0_4) + (temp_v1_4 * temp_v1_4));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[4].x;
        temp_v1_5             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[4].z;
        temp_s3->z            = temp_v1_5;
        temp_v0_5             = temp_s3->x;
        temp_s3->distances[4] = SquareRoot0((temp_v0_5 * temp_v0_5) + (temp_v1_5 * temp_v1_5));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[5].x;
        temp_v1_6             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[5].z;
        temp_s3->z            = temp_v1_6;
        temp_v0_6             = temp_s3->x;
        temp_s3->distances[5] = SquareRoot0((temp_v0_6 * temp_v0_6) + (temp_v1_6 * temp_v1_6));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[6].x;
        temp_v1_7             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[6].z;
        temp_s3->z            = temp_v1_7;
        temp_v0_7             = temp_s3->x;
        temp_s3->distances[6] = SquareRoot0((temp_v0_7 * temp_v0_7) + (temp_v1_7 * temp_v1_7));
        var_a2                = 0xFFFFFF;
        var_v1                = 0;
        var_a1                = 0;
        do {
            temp_a0 = temp_s5->distances[var_a1 & 0xFF];
            if (temp_a0 < var_a2) {
                var_v1 = var_a1;
                var_a2 = temp_a0;
            }
            var_a1 += 1;
        } while ((u32)(var_a1 & 0xFF) < 7U);
    } else {
        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[7].x;
        temp_v1_8             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[7].z;
        temp_s3->z            = temp_v1_8;
        temp_v0_8             = temp_s3->x;
        temp_s3->distances[0] = SquareRoot0((temp_v0_8 * temp_v0_8) + (temp_v1_8 * temp_v1_8));

        temp_s3->x            = Player_Status.coordMtx->t[0] - D_actor_403600_801605F4[8].x;
        temp_v1_9             = Player_Status.coordMtx->t[2] - D_actor_403600_801605F4[8].z;
        temp_s3->z            = temp_v1_9;
        temp_v0_9             = temp_s3->x;
        temp_v0_10            = SquareRoot0((temp_v0_9 * temp_v0_9) + (temp_v1_9 * temp_v1_9));
        temp_s3->distances[1] = temp_v0_10;
        var_v1                = temp_v0_10 >= temp_s3->distances[0];
    }
    temp_v0_11 = D_actor_403600_801605F4;
    temp_v1_10 = (var_v1 & 0xFF) + temp_v0_11;
    do {
        temp_s4->field_6F0.vx = temp_v1_10->x;
    } while (0);
    temp_s4->field_6F0.vy = Player_Status.coordMtx->t[1] - 0x258;
    SCRATCH_POP_BYTES(0x24);
    temp_s4->field_6F0.vz = temp_v1_10->z;
}

void func_actor_403600_8013D15C(Task* arg0)
{
    s32                       sp10;
    s32                       sp14;
    Actor403600DamageRow*     var_v1_2;
    s32                       var_a2;
    s16                       temp_v0_2;
    s16                       temp_v0_7;
    s32                       temp_lo;
    s32                       temp_v0;
    s32                       temp_v0_3;
    s32                       temp_v0_4;
    s32                       temp_v1_4;
    s32                       temp_v1_6;
    s32                       temp_v1_8;
    s32                       one;
    s32                       var_a0;
    s32                       var_s1;
    s32                       var_s3;
    s32                       var_v0;
    s32                       var_v1;
    u16                       temp_v1_3;
    u32                       temp_v0_5;
    u32                       temp_v0_6;
    u32                       temp_v0_8;
    u32                       temp_v0_9;
    u32                       temp_v1_5;
    u32                       random_value;
    s32                       negated_speed;
    Actor403600Work*          temp_s0;
    GpPairSrcE*               data_ec8;
    TmdObject*                effect_obj;
    GpRec18*                  temp_s1;
    GpEnemy*                  temp_s4;
    u32*                      rng;
    GpDeltaScratch*           delta_scratch;
    Actor403600DamageScratch* temp_v1_2;
    u8*                       var_s2;
    PlayerStatus*             player_status;
    GpRec18*                  records;

    temp_s0 = arg0->work;
    records = temp_s0->field_528;
    temp_v0 = (s32)SCRATCH_HEAD(u8);
    SOFT_TOUCH_REG_USE(temp_v0, records);
    delta_scratch      = (GpDeltaScratch*)(temp_v0 - 0x28);
    temp_v0           -= 0x48;
    SCRATCH_HEAD(void) = (void*)temp_v0;
    temp_v1_2          = (Actor403600DamageScratch*)temp_v0;
    temp_s4            = arg0->spawnArg2;
    temp_v0            = func_800E0C10(records, delta_scratch, 4, 0);
    if (temp_v0 == 1) {
        goto block_collision;
    }
    if (temp_v0 < 2) {
        goto block_after_collision;
    }
    if (temp_v0 == 2) {
        goto block_collision;
    }
    goto block_after_collision;

block_fatal:
    Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[1], 3, 0);
    func_800DA6E8(&temp_s4->node, 0x3E7, 0);
    temp_s0->field_742 = 1;
    return;

block_collision:
    if ((arg0 == D_actor_403600_801606A8) && (temp_s0->field_774 == 0)) {
        temp_s0->field_4B8.coord.t[0] += temp_v1_2->delta.vx.h.hi;
        temp_s0->field_4B8.coord.t[1] += temp_v1_2->delta.vy.h.hi;
        temp_s0->field_4B8.coord.t[2] += temp_v1_2->delta.vz.h.hi;
        SOFT_USE_REG(temp_v1_2);
    }
block_after_collision:
    if ((s16)temp_s0->field_744 != 0) {
        temp_v0_2          = (u16)temp_s0->field_744 - 1;
        temp_s0->field_744 = temp_v0_2;
        if ((temp_v0_2 << 0x10) <= 0) {
            temp_s0->field_744 = 0;
        }
    }
    player_status = &Player_Status;
    data_ec8      = &D_actor_403600_80150EC8;
    one           = 1;
    rng           = (u32*)0x80070000;
    SOFT_USE_REG(rng);
    /* A byte cursor steps through the four `field_528` contact records from
     * the start of the work block, reaching each at the table's offset and
     * stopping at the table's size; indexing the table instead rebuilds the
     * whole loop. */
    var_s2 = (u8*)temp_s0;
loop_14:
    temp_v1_3 = *(u16*)(var_s2 + 0x52E);
    temp_v1_4 = temp_v1_3 == one;
    if (temp_v1_4) {
        goto block_83;
    }
    if (temp_v1_3 != 2) {
        goto block_83;
    }
    if ((s16)temp_s0->field_744 != 0) {
        goto block_83;
    }
    temp_v0_3             = player_status->coordMtx->t[0] - temp_s0->field_4B8.coord.t[0];
    temp_lo               = temp_v0_3 * temp_v0_3;
    temp_v1_2->delta.vx.w = temp_v0_3;
    temp_v0_4             = player_status->coordMtx->t[1] - 0x7D0;
    temp_v0_4            -= temp_s0->field_4B8.coord.t[1];
    temp_v1_2->delta.vy.w = temp_v0_4;
    temp_v1_4             = player_status->coordMtx->t[2] - temp_s0->field_4B8.coord.t[2];
    var_s1                = 0;
    SOFT_TOUCH_REG(var_s1);
    temp_v1_2->delta.vz.w = temp_v1_4;
    var_s3                = Gp_ComputeDamage(((GpRec18*)(var_s2 + 0x528))->key, SquareRoot0(temp_lo + (temp_v0_4 * temp_v0_4) + (temp_v1_4 * temp_v1_4)), 0, 0);
    if (Gp_RollEnemyChance(arg0->spawnArg2, ((GpRec18*)(var_s2 + 0x528))->key, 0) != 0) {
        var_s1  = 1;
        var_s3 *= 4;
    }
    var_v1 = var_s1;
    if (temp_s0->field_7AE != 0) {
        var_s1  = 2;
        var_s3 *= 2;
        if (temp_s0->field_758 >= 0xC9) {
            temp_s0->field_7AE = 0;
        }
    }
    var_v1 = var_s1;
    if (var_v1 == one) {
        var_a0     = 0x6009C;
        effect_obj = arg0->extra.tmd;
        var_a2     = 0;
        goto block_27;
    }
    if (var_v1 == 2) {
        var_a0     = 0x6009C;
        effect_obj = arg0->extra.tmd;
        var_a2     = 3;
    block_27:
        Gp_SpawnEff(var_a0, &effect_obj->coords[1], var_a2, 0);
    }
    temp_v1_5 = Gp_GetIdParam0(((GpRec18*)(var_s2 + 0x528))->key) & 0xFFFF;
    switch (temp_v1_5) {
        case 0:
            break;
        case 2:
            if (temp_s0->field_784 == 0) {
                temp_s0->field_7AE = 0;
                Gp_SetObjFlag2(arg0->spawnArg2, ((GpRec18*)(var_s2 + 0x528))->key, 0);
                if (((u32)((u16)temp_s0->field_736 - 0x10) < 2U) && ((u32)((u16)temp_s0->field_73A - 6) < 0x18U)) {
                    temp_s0->field_790 = data_ec8->flag2Ticks;
                }
                if ((temp_s0->field_736 == one) && (temp_s0->field_73A < 0x1E)) {
                    temp_s0->field_790 = data_ec8->flag2Ticks;
                }
            }
            break;
        case 3:
            if ((temp_s0->field_784 == 0) && (temp_s4->hp >= 0x1F5)) {
                Gp_SetObjFlag4(arg0->spawnArg2, ((GpRec18*)(var_s2 + 0x528))->key, 0);
            }
            if (((GpRec18*)(var_s2 + 0x528))->key & 8) {
                temp_v0_5                  = (ACTOR403600_RNG_VALUE(rng) * 5) + 0x71357911;
                ACTOR403600_RNG_VALUE(rng) = temp_v0_5;
                if ((temp_v0_5 >> 0x10) & 1) {
                    temp_s0->field_758 = 0xC8;
                }
                if ((u32)((u16)temp_s0->field_736 - 0x10) < 2U) {
                    temp_s0->field_7AE = 0;
                    if (temp_s0->field_73A < 0x1E) {
                        temp_s0->field_730 = 4;
                        temp_s0->field_790 = data_ec8->flag2Ticks * 0xA;
                    }
                }
            }
            break;
        case 1:
        case 4:
            if (temp_s0->field_7AE != 0) {
                Gp_SetObjFlag1(temp_s4);
                var_a0                     = 0x71350000;
                var_a0                    |= 0x7911;
                random_value               = ACTOR403600_RNG_VALUE(rng);
                temp_v1_5                  = random_value * 4;
                temp_v1_5                 += random_value;
                temp_v1_5                 += var_a0;
                temp_s0->field_7AE         = 0;
                ACTOR403600_RNG_VALUE(rng) = temp_v1_5;
                if (!(((temp_v1_5 >> 0x10) - (((temp_v1_5 >> 0x10) / 10U) * 10)) & 0xFFFF) && (temp_s0->field_78C == 0) && (temp_s0->field_7A6 == 0)) {
                    temp_s0->field_79E     = 0;
                    temp_s0->field_73A     = 0U;
                    temp_s0->field_730     = 5;
                    temp_s4->reactionFlags = (u8)(temp_s4->reactionFlags & 0xFE);
                    case 6:
                    case 7:
                    case 9:
                }
            }
            temp_v1_6 = (s16)temp_s0->field_78A;
            temp_v1_6 = temp_v1_6 / 10;
            if (temp_s4->hp < temp_v1_6) {
                temp_s0->field_7AE = 0;
                if ((temp_s0->field_78C == 0) && (temp_s0->field_7A6 == 0)) {
                    temp_s0->field_79E     = 0;
                    temp_s0->field_73A     = 0U;
                    temp_s0->field_730     = 5;
                    temp_s4->reactionFlags = (u8)(temp_s4->reactionFlags & 0xFE);
                }
            }
            break;
    }
    if (temp_s0->field_7A4 != 0) {
        temp_v1_8 = ((GpRec18*)(var_s2 + 0x528))->key;
        if ((temp_v1_8 == 0x28003) || (temp_v1_8 == 0x28006) || (temp_v1_8 == 0x2800F)) {
            temp_s0->field_7AE = 0;
            if (temp_s0->field_78C == 0) {
                if (temp_s0->field_7A6 == 0) {
                    temp_s0->field_79E     = 0;
                    temp_s0->field_73A     = 0U;
                    temp_s0->field_730     = 5;
                    temp_s4->reactionFlags = (u8)(temp_s4->reactionFlags & 0xFE);
                }
            }
        }
    }
    if (arg0 != D_actor_403600_801606A8) {
        func_800E2C78(temp_s4, ((GpRec18*)(var_s2 + 0x528))->key, var_s3, 0);
        if (((GpRec18*)(var_s2 + 0x528))->key & 8) {
            temp_v0_6                  = (ACTOR403600_RNG_VALUE(rng) * 5) + 0x71357911;
            ACTOR403600_RNG_VALUE(rng) = temp_v0_6;
            if (((temp_v0_6 >> 0x10) & 3) == 0) {
                goto block_fatal;
            }
        }
        func_actor_403600_80141C7C(arg0, var_s3);
        goto block_73;
    }
    func_800E2C78(temp_s4, ((GpRec18*)(var_s2 + 0x528))->key, var_s3, 0);
    func_actor_403600_8013DAF4(arg0, var_s3);
    if (temp_s0->field_730 == one) {
        var_a0   = 0;
        var_v1_2 = D_actor_403600_8016066C;
        do {
            if (var_s3 >= var_v1_2->threshold) {
                temp_s0->field_796 = var_v1_2->field_2;
                temp_s0->field_7A0 = var_v1_2->field_4;
            }
            var_a0 += 1;
            SOFT_TOUCH_REG(var_v1_2);
            var_v1_2 += 1;
        } while (var_a0 < 5);
    }
block_73:
    temp_s0->field_758 = (u16)temp_s0->field_758 + var_s3;
    if ((temp_s4->hp > 0) && (arg0 == D_actor_403600_801606A8)) {
        func_800FDB18(Gp_GetIdParam1(((GpRec18*)(var_s2 + 0x528))->key) & 0xFFFF, &temp_s0->field_4B8, &temp_s0->field_6E8, &temp_s0->field_658);
    }
    temp_v0_7 = Gp_GetIdParam2(((GpRec18*)(var_s2 + 0x528))->key);
    if ((temp_v0_7 << 0x10) > 0) {
        temp_s0->field_744 = temp_v0_7;
    }
    func_actor_403600_8013E470(&temp_s0->field_4B8, &sp10, &sp14);
    var_v0 = sp14;
    if (var_v0 < 0) {
        var_v0 = -var_v0;
    }
    if (var_v0 < 0x401) {
        temp_v0_8                  = (ACTOR403600_RNG_VALUE(rng) * 5) + 0x71357911;
        ACTOR403600_RNG_VALUE(rng) = temp_v0_8;
        temp_s0->field_700         = (s16)(((temp_v0_8 >> 0xB) & 0x60) + 0x80);
    } else {
        temp_v0_9                  = (ACTOR403600_RNG_VALUE(rng) * 5) + 0x71357911;
        ACTOR403600_RNG_VALUE(rng) = temp_v0_9;
        temp_s0->field_700         = (s16) - (((temp_v0_9 >> 0xB) & 0x60) + 0x80);
        negated_speed              = -temp_s0->field_796;
        temp_s0->field_796         = negated_speed;
    }
block_83:
    var_s2 += 0x18;
    if ((s32)var_s2 >= (s32)((u8*)temp_s0 + 0x60)) {
        Gp_ClearRec18Occupied(temp_s0->field_528);
        temp_s1 = temp_s0->field_5A8;
        if (Gp_FindRec18(temp_s1, 0) != 0) {
            Gp_ClearRec18Occupied(temp_s1);
            temp_s0->field_588.flags &= 0x7FFF;
        }
        SCRATCH_POP_BYTES(0x48);
        return;
    }
    goto loop_14;
}

s32 func_actor_403600_8013D9A8(Task* arg0)
{
    s32              i;
    s32              mask;
    s32              kind;
    s32              callResult;
    s32              three;
    s32              x;
    s32              y;
    s32              z;
    Actor403600Work* work;
    u8*              entry;

    i     = 0;
    mask  = 0xFFFF0000;
    kind  = 0x10;
    work  = arg0->work;
    entry = (u8*)work;
    do {
        if ((((u32)(((volatile GpRec18*)(entry + 0x5F8))->key & mask)) >> 16) == kind) {
            if (work->field_786 == 0) {
                work->field_786++;
                callResult = func_800E1B24(((volatile GpRec18*)(entry + 0x5F8))->key);
                three      = 3;
                if (callResult == three) {
                    return 2;
                }
                SOFT_USE_REG(three);
            }
        }
        i++;
        entry += sizeof(GpRec18);
    } while (i < 4);

    Gp_ClearRec18Occupied(work->field_5F8);
    x = work->field_4B8.coord.t[0] + ((work->field_4B8.coord.m[0][2] * 0x177) >> 9);
    y = work->field_4B8.coord.t[2] + ((work->field_4B8.coord.m[2][2] * 0x177) >> 9);
    if (x < 0x101) {
        return 1;
    }
    if ((x >= 0x3D00) || (y >= 0x3800)) {
        return 1;
    }
    if (y < -0x7F) {
        return 1;
    }
    z = work->field_4B8.coord.t[1];
    if (z >= 0) {
        return 1;
    }
    if (z < -0x176F) {
        return 3;
    }
    return 0;
}

void func_actor_403600_8013DAF4(Task* arg0, s32 arg1)
{
    GpEnemy*         temp_v0;
    GpEnemy*         temp_s0;
    Actor403600Work* temp_s1;
    Task*            temp_v0_2;
    Actor403600Work* temp_v0_3;

    temp_s0     = arg0->spawnArg2;
    temp_s1     = arg0->work;
    temp_s0->hp = (u16)temp_s0->hp - arg1;
    func_800DA6E8(&temp_s0->node, arg1, 0);
    if (temp_s0->hp <= 0) {
        if (Player_Status.hp <= 0) {
            temp_s0->hp = 0xA;
            return;
        }
        temp_v0 = temp_s1->field_4B4;
        if (temp_v0 != NULL) {
            temp_v0_2                                      = temp_v0->task;
            temp_v0_2->state                               = 2;
            temp_v0_2->killCountdown                       = 0;
            ((Actor403600Work*)temp_v0_2->work)->field_732 = 1;
        }
        temp_s1->field_588.flags &= 0x7FFF;
        Gp_PulseState1C80();
        gGameSession->eventState = 1;
        D_actor_403600_8016056C  = 0;
        Gp_DispatchMsg(*Gp_ActorSlots, 0x3F1, 0, 0);
        temp_v0_3            = arg0->work;
        temp_v0_3->field_756 = 8;
        temp_v0_3->field_776 = 0xA;
        temp_v0_3->field_742 = 0;
        temp_v0_3->field_746 = 0;
        temp_v0_3->field_774 = 0;
        temp_v0_3->field_778 = 0x10;
        temp_v0_3->field_77A = 0;
        temp_v0_3->field_784 = 0;
        temp_v0_3->field_73C = 0;
        temp_v0_3->field_73E = 0;
        temp_v0_3->field_74A = 0;
        temp_v0_3->field_73A = 0;
        temp_v0_3->field_76E = 0x40;
        temp_v0_3->field_75E = 0;
        temp_v0_3->field_7A4 = 0;
        temp_v0_3->field_7A6 = 0;
        temp_v0_3->field_7AC = 0;
        func_actor_403600_80141954(0);
        func_actor_403600_80141B24(arg0);
        temp_s1->field_742 = 1;
        temp_s1->field_736 = 1;
        temp_s1->field_756 = 0;
        temp_s1->field_778 = 0x10;
        temp_s1->field_730 = 0x14;
        temp_s1->field_732 = 0;
        temp_s1->field_7A2 = 0;
        Gp_HaltPadScripts();
        SndEvt_EnqueueType7(0x54160001, 1);
    }
}

void func_actor_403600_8013DC7C(Task* arg0)
{
    s16              temp_v0_2;
    s16              temp_v0;
    u16              var_a3;
    s16              var_a3_signed;
    Actor403600Work* temp_a2;

    temp_a2               = arg0->work;
    var_a3                = temp_a2->field_73C;
    temp_a2->field_6B0.vx = temp_a2->field_4B8.coord.t[0];
    temp_a2->field_6B0.vy = temp_a2->field_4B8.coord.t[1];
    temp_a2->field_6B0.vz = temp_a2->field_4B8.coord.t[2];
    var_a3_signed         = var_a3;
    if ((temp_a2->field_78C != 0) && (var_a3_signed != 0)) {
        var_a3 = (var_a3_signed * 0x3C) / 100;
    }
    if (temp_a2->field_784 != 0) {
        temp_a2->field_4B8.coord.t[0] +=
            (temp_a2->field_4B8.coord.m[0][2] * ((s16)var_a3 - temp_a2->field_796)) >> 0xC;
        temp_a2->field_4B8.coord.t[1] +=
            (temp_a2->field_4B8.coord.m[1][2] * ((s16)var_a3 - temp_a2->field_796)) >> 0xC;
        temp_a2->field_4B8.coord.t[2] +=
            (temp_a2->field_4B8.coord.m[2][2] * ((s16)var_a3 - temp_a2->field_796)) >> 0xC;
    } else {
        temp_a2->field_4B8.coord.t[1] += temp_a2->field_74A;
        temp_a2->field_4B8.coord.t[0] +=
            (temp_a2->field_4B8.coord.m[0][2] * ((s16)var_a3 - temp_a2->field_796)) >> 0xC;
        temp_a2->field_4B8.coord.t[2] +=
            (temp_a2->field_4B8.coord.m[2][2] * ((s16)var_a3 - temp_a2->field_796)) >> 0xC;
    }
    temp_v0            = temp_a2->field_7A0 - 1;
    temp_a2->field_7A0 = temp_v0;
    if (temp_v0 < 0) {
        temp_v0_2          = (u16)temp_a2->field_796 - 1;
        temp_a2->field_796 = temp_v0_2;
        if (temp_v0_2 < 0) {
            temp_a2->field_796 = 0;
        }
        temp_a2->field_7A0 = 0;
    }
}

s32 func_actor_403600_8013DDF4(Task* arg0, s16 arg1)
{
    Actor403600Work*  work;
    ActorFaceScratch* scratch;
    ActorFaceScratch* oldHead;
    s16               step;
    s32               distance;
    u16               angle;
    s32               rawDiff;
    s32               adiff;
    s32               turnDiff;
    s32               next;

    step    = arg1;
    oldHead = SCRATCH_HEAD(ActorFaceScratch);
    scratch = (SCRATCH_HEAD(ActorFaceScratch) =
                   oldHead - 1);
    work    = arg0->work;
    if ((arg1 << 0x10) == 0) {
        step = 0x20;
    }

    switch (work->field_746) {
        case 0:
            oldHead[-1].delta.vx = Player_Status.coordMtx->t[0] - work->field_4B8.coord.t[0];
            scratch->delta.vy    = 0;
            scratch->delta.vz    = Player_Status.coordMtx->t[2] - work->field_4B8.coord.t[2];
            break;
        case 1:
            oldHead[-1].delta.vx = work->field_6F0.vx - work->field_4B8.coord.t[0];
            scratch->delta.vy    = 0;
            scratch->delta.vz    = work->field_6F0.vz - work->field_4B8.coord.t[2];
            break;
    }

    distance = SquareRoot0((scratch->delta.vx * scratch->delta.vx) +
                           (scratch->delta.vy * scratch->delta.vy) +
                           (scratch->delta.vz * scratch->delta.vz));
    angle    = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & 0xFFF;
    rawDiff  = angle - (work->field_748 & 0xFFF);
    adiff    = __builtin_abs((s16)rawDiff);
    turnDiff = rawDiff;
    if (step >= adiff) {
        work->field_748 = angle;
    } else {
        if (adiff >= 0x801) {
            next = rawDiff - 0x1000;
            if ((s16)rawDiff <= 0) {
                next = 0x1000 - rawDiff;
            }
            turnDiff = next;
        }
        rawDiff = (s16)work->field_748;
        if ((turnDiff << 0x10) > 0) {
            next = rawDiff + step;
        } else {
            next = (s16)work->field_748 - step;
        }
        work->field_748 = next;
    }

    scratch->rot.vx = 0;
    scratch->rot.vy = work->field_748;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &work->field_4B8.coord);
    SCRATCH_POP(ActorFaceScratch);
    return distance;
}

s32 func_actor_403600_8013DFE0(Task* arg0)
{
    s16                     temp_v0;
    s16                     temp_v0_2;
    s16                     temp_v0_3;
    s16                     temp_v1_3;
    s16                     temp_v1_4;
    s32                     temp_v1_5;
    s16                     temp_v1_6;
    s32                     temp_v1_7;
    s16                     temp_v1_8;
    s32                     temp_v1_9;
    s16                     var_a0_2;
    s16                     var_a0_3;
    s16                     var_a0_4;
    s32                     var_a1;
    s32                     var_a1_2;
    s32                     var_a1_3;
    s16                     var_v0;
    s16                     var_v0_2;
    s32                     var_v0_3;
    s16                     var_v0_4;
    s32                     var_v0_5;
    s16                     var_v0_6;
    s32                     var_v0_7;
    s32                     temp_lo;
    s32                     temp_s5;
    s32                     temp_step;
    s32                     temp_step_2;
    s32                     temp_step_3;
    u16                     temp_a3;
    u16                     temp_a3_2;
    u16                     temp_a3_3;
    Actor403600TurnMatrix*  temp_s0;
    MATRIX*                 temp_s0_2;
    Actor403600TurnMatrix*  temp_s0_3;
    MATRIX*                 temp_s0_4;
    SVECTOR*                temp_s1;
    SVECTOR*                temp_s2;
    Actor403600Work*        temp_s4;
    Actor403600TurnScratch* temp_v1;
    Actor403600TurnScratch* temp_v1_2;

    temp_v1            = SCRATCH_HEAD(Actor403600TurnScratch);
    temp_v1            = temp_v1 - 1;
    SCRATCH_HEAD(void) = temp_v1;
    temp_v1_2          = temp_v1;
    temp_s4            = arg0->work;
    temp_v1_3          = temp_s4->field_746;
    switch (temp_v1_3) {
        case 0:
            temp_v1_2->vector[0] =
                (s16)(Player_Status.coordMtx->t[0] - temp_s4->field_4B8.coord.t[0]);
            temp_v1_2->vector[1] =
                (s16)(Player_Status.coordMtx->t[1] - temp_s4->field_4B8.coord.t[1]);
            temp_v1_2->vector[2] =
                (s16)(Player_Status.coordMtx->t[2] - temp_s4->field_4B8.coord.t[2]);
            break;
        case 1:
        case 3:
            temp_v1_2->vector[0] = (s16)(temp_s4->field_6F0.vx - temp_s4->field_4B8.coord.t[0]);
            temp_v1_2->vector[1] = (s16)(temp_s4->field_6F0.vy - temp_s4->field_4B8.coord.t[1]);
            temp_v1_2->vector[2] = (s16)(temp_s4->field_6F0.vz - temp_s4->field_4B8.coord.t[2]);
            break;
        case 2:
            temp_s5              = (s16)(Player_Status.coordMtx->t[0] - temp_s4->field_4B8.coord.t[0]);
            temp_v1_2->vector[1] = 0;
            temp_v1_2->vector[0] = temp_s5;
            temp_v1_2->vector[2] =
                (s16)(Player_Status.coordMtx->t[2] - temp_s4->field_4B8.coord.t[2]);
            break;
    }
    temp_v0   = temp_v1_2->vector[0];
    temp_v0_2 = temp_v1_2->vector[1];
    temp_v0_3 = temp_v1_2->vector[2];
    temp_lo   = temp_v0_3 * temp_v0_3;
    temp_s5   = SquareRoot0((temp_v0 * temp_v0) + (temp_v0_2 * temp_v0_2) + temp_lo);
    if (temp_s4->field_746 == 3) {
        temp_s2                   = (SVECTOR*)temp_v1_2->vector;
        temp_s0                   = &temp_v1_2->matrix;
        temp_v1_2->matrix.field_0 = 0x1000;
        temp_s0->field_4          = 0;
        temp_s0->field_8          = 0x1000;
        temp_s0->field_C          = 0;
        temp_s0->field_10         = 0x1000;
        VectorNormalSS(temp_s2, temp_s2);
        temp_v1_2->angles[0] = 0;
        temp_v1_2->angles[1] = 0x1000;
        temp_v1_2->angles[2] = 0;
        Gfx_OrthonormalBasis((MATRIX*)temp_s0, temp_s2, (SVECTOR*)temp_v1_2);
        Gfx_MatrixToEuler((MATRIX*)temp_s0, (SVECTOR*)temp_v1_2);
        temp_s0_2            = &temp_s4->field_4B8.coord;
        temp_v1_2->angles[2] = (u16)(temp_v1_2->angles[2] + temp_s4->field_75E);
        Gfx_RotMatrixXYZ(temp_s0_2, (SVECTOR*)temp_v1_2, 1);
        Gfx_MatrixCol2(temp_s0_2, (SVECTOR*)temp_v1_2);
    } else {
        temp_s1                   = (SVECTOR*)temp_v1_2->vector;
        temp_s0_3                 = &temp_v1_2->matrix;
        temp_v1_2->matrix.field_0 = 0x1000;
        temp_s0_3->field_4        = 0;
        temp_s0_3->field_8        = 0x1000;
        temp_s0_3->field_C        = 0;
        temp_s0_3->field_10       = 0x1000;
        VectorNormalSS(temp_s1, temp_s1);
        temp_v1_2->angles[0] = 0;
        temp_v1_2->angles[1] = 0x1000;
        temp_v1_2->angles[2] = 0;
        Gfx_OrthonormalBasis((MATRIX*)temp_s0_3, temp_s1, (SVECTOR*)temp_v1_2);
        Gfx_MatrixToEuler((MATRIX*)temp_s0_3, (SVECTOR*)temp_v1_2);
        Gfx_MatrixToEuler(&temp_s4->field_4B8.coord, temp_s1);
        temp_a3   = temp_v1_2->angles[0];
        temp_v1_4 = (temp_a3 & 0xFFF) - ((u16)temp_v1_2->vector[0] & 0xFFF);
        temp_step = temp_s4->field_76E;
        var_a1    = temp_v1_4;
        if (temp_v1_4 < 0) {
            SOFT_TOUCH_REG(var_a1);
            var_a1 = -var_a1;
        }
        var_a0_2 = temp_v1_4;
        if (temp_step >= var_a1) {
            temp_v1_2->vector[0] = (s16)temp_a3;
        } else {
            SOFT_TOUCH_REG(var_a0_2);
            SOFT_TOUCH_REG(var_a0_2);
            if (var_a1 >= 0x801) {
                var_v0_2 = temp_v1_4 - 0x1000;
                if (temp_v1_4 <= 0) {
                    var_v0_2 = 0x1000 - temp_v1_4;
                }
                var_a0_2 = var_v0_2;
            }
            temp_v1_5 = temp_v1_2->vector[0];
            if ((var_a0_2 << 0x10) > 0) {
                var_v0_3 = temp_v1_5 + temp_s4->field_76E;
            } else {
                var_v0_3 = temp_v1_5 - temp_s4->field_76E;
            }
            temp_v1_2->vector[0] = var_v0_3;
        }
        temp_a3_2   = temp_v1_2->angles[1];
        temp_v1_6   = (temp_a3_2 & 0xFFF) - ((u16)temp_v1_2->vector[1] & 0xFFF);
        temp_step_2 = temp_s4->field_76E;
        var_a1_2    = temp_v1_6;
        if (temp_v1_6 < 0) {
            SOFT_TOUCH_REG(var_a1_2);
            var_a1_2 = -var_a1_2;
        }
        var_a0_3 = temp_v1_6;
        if (temp_step_2 >= var_a1_2) {
            temp_v1_2->vector[1] = (s16)temp_a3_2;
        } else {
            SOFT_TOUCH_REG(var_a0_3);
            SOFT_TOUCH_REG(var_a0_3);
            if (var_a1_2 >= 0x801) {
                var_v0_4 = temp_v1_6 - 0x1000;
                if (temp_v1_6 <= 0) {
                    var_v0_4 = 0x1000 - temp_v1_6;
                }
                var_a0_3 = var_v0_4;
            }
            temp_v1_7 = temp_v1_2->vector[1];
            if ((var_a0_3 << 0x10) > 0) {
                var_v0_5 = temp_v1_7 + temp_s4->field_76E;
            } else {
                var_v0_5 = temp_v1_7 - temp_s4->field_76E;
            }
            temp_v1_2->vector[1] = var_v0_5;
        }
        temp_a3_3   = temp_v1_2->angles[2];
        temp_v1_8   = (temp_a3_3 & 0xFFF) - ((u16)temp_v1_2->vector[2] & 0xFFF);
        temp_step_3 = temp_s4->field_76E;
        var_a1_3    = temp_v1_8;
        if (temp_v1_8 < 0) {
            SOFT_TOUCH_REG(var_a1_3);
            var_a1_3 = -var_a1_3;
        }
        var_a0_4 = temp_v1_8;
        if (temp_step_3 >= var_a1_3) {
            temp_v1_2->vector[2] = (s16)temp_a3_3;
        } else {
            SOFT_TOUCH_REG(var_a0_4);
            SOFT_TOUCH_REG(var_a0_4);
            if (var_a1_3 >= 0x801) {
                var_v0_6 = temp_v1_8 - 0x1000;
                if (temp_v1_8 <= 0) {
                    var_v0_6 = 0x1000 - temp_v1_8;
                }
                var_a0_4 = var_v0_6;
            }
            temp_v1_9 = temp_v1_2->vector[2];
            if ((var_a0_4 << 0x10) > 0) {
                var_v0_7 = temp_v1_9 + temp_s4->field_76E;
            } else {
                var_v0_7 = temp_v1_9 - temp_s4->field_76E;
            }
            temp_v1_2->vector[2] = var_v0_7;
        }
        temp_s0_4            = &temp_s4->field_4B8.coord;
        temp_v1_2->vector[2] = (s16)((u16)temp_v1_2->vector[2] + temp_s4->field_75E);
        Gfx_RotMatrixXYZ(temp_s0_4, (SVECTOR*)temp_v1_2->vector, 1);
        temp_s4->field_748 = (u16)temp_v1_2->vector[1];
        Gfx_MatrixCol2(temp_s0_4, (SVECTOR*)temp_v1_2);
    }
    temp_s4->field_748 = ratan2((s16)temp_v1_2->angles[0], (s16)temp_v1_2->angles[2]);
    SCRATCH_POP_BYTES(0x30);
    return temp_s5;
}

void func_actor_403600_8013E470(GpCoord* arg0, s32* arg1, s32* arg2)
{
    SVECTOR                    local;
    GpCoord*                   coord;
    s32                        angle;
    s32                        x;
    s32                        z;
    Actor403600BearingScratch* head;
    SVECTOR*                   vec;
    MATRIX*                    matrix;
    Actor403600BearingScratch* scratch;

    head            = SCRATCH_HEAD(void);
    coord           = (*Gp_ActorSlots)->extra.tmd->coords;
    head[-1].rel.vx = (s16)(coord->workm.t[0] - arg0->workm.t[0]);
    vec             = &head[-1].rel;
    vec->vy         = (s16)(coord->workm.t[1] - arg0->workm.t[1]);
    scratch         = (SCRATCH_HEAD(void) = &head[-1]);
    vec->vz         = (s16)(coord->workm.t[2] - arg0->workm.t[2]);
    matrix          = &head[-1].rot;
    TransposeMatrix(&arg0->workm, matrix);
    local = *vec;
    gte_SetRotMatrix(matrix);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    gte_rtv0();
    gte_stsv(vec);
    angle = ratan2(head[-1].rel.vx, vec->vz);
    *arg2 = angle;
    if (angle >= 0x801) {
        *arg2 = angle - 0x1000;
    } else if (angle < -0x800) {
        *arg2 = angle + 0x1000;
    }
    x                    = Player_Status.coordMtx->t[0] - arg0->coord.t[0];
    scratch->toPlayer.vx = x;
    scratch->toPlayer.vy = Player_Status.coordMtx->t[1] - arg0->coord.t[1];
    z                    = Player_Status.coordMtx->t[2] - arg0->coord.t[2];
    scratch->toPlayer.vz = z;
    *arg1                = SquareRoot0((x * x) + (z * z));
    SCRATCH_POP_BYTES(sizeof(Actor403600BearingScratch));
}

s16 func_actor_403600_8013E66C(GpCoord* arg0)
{
    SVECTOR                    local;
    GpCoord*                   coord;
    s16                        angle;
    s16                        result;
    SVECTOR*                   vec;
    Actor403600BearingScratch* head;

    head               = SCRATCH_HEAD(void);
    coord              = (*Gp_ActorSlots)->extra.tmd->coords;
    SCRATCH_HEAD(void) = &head[-1];
    head[-1].rel.vx    = (s16)(arg0->workm.t[0] - coord->workm.t[0]);
    vec                = &head[-1].rel;
    vec->vy            = (s16)(arg0->workm.t[1] - coord->workm.t[1]);
    vec->vz            = (s16)(arg0->workm.t[2] - coord->workm.t[2]);
    TransposeMatrix(&coord->workm, &head[-1].rot);
    local = *vec;
    gte_SetRotMatrix(&head[-1].rot);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    gte_rtv0();
    gte_stsv(vec);
    angle  = ratan2(head[-1].rel.vx, vec->vz);
    result = angle;
    if (angle >= 0x801) {
        result = angle - 0x1000;
    } else if (angle < -0x800) {
        result = angle + 0x1000;
    }
    SCRATCH_POP_BYTES(sizeof(Actor403600BearingScratch));
    return result;
}

s32 func_actor_403600_8013E7D4(s32 arg0, s32 arg1)
{
    Task*    temp_s7;
    GpCoord* temp_s3;
    s32      temp_s0;
    s32      temp_s0_3;
    s32      temp_s1;
    s32      temp_s5;
    s32      var_s2;
    s32      var_s4;
    s32      var_v1;

    temp_s7 = *Gp_ActorSlots;
    temp_s3 = temp_s7->extra.tmd->coords;
    temp_s1 = D_actor_403600_801605E4.vx - temp_s3->coord.t[0];
    temp_s0 = D_actor_403600_801605E4.vz - temp_s3->coord.t[2];
    var_s4  = 0;
    temp_s5 = SquareRoot0((temp_s1 * temp_s1) + (temp_s0 * temp_s0));
    var_s2  = ratan2(temp_s1, temp_s0);
    if (var_s2 >= 0x801) {
        var_s2 -= 0x1000;
    } else if (var_s2 < -0x800) {
        var_s2 += 0x1000;
    }
    temp_s1   = D_actor_403600_801605EC.vx - temp_s3->coord.t[0];
    temp_s0   = D_actor_403600_801605EC.vz - temp_s3->coord.t[2];
    temp_s0_3 = SquareRoot0((temp_s1 * temp_s1) + (temp_s0 * temp_s0));
    var_v1    = ratan2(temp_s1, temp_s0);
    if (var_v1 >= 0x801) {
        var_v1 -= 0x1000;
    } else if (var_v1 < -0x800) {
        var_v1 += 0x1000;
    }
    if (arg1 & 1) {
        if (temp_s0_3 >= temp_s5) {
            D_actor_403600_801606E0.rot.vy = (s16)var_v1;
        } else {
            goto block_14;
        }
    } else if (temp_s5 < temp_s0_3) {
    block_14:
        D_actor_403600_801606E0.rot.vy = (s16)var_s2;
    } else {
        D_actor_403600_801606E0.rot.vy = (s16)var_v1;
    }
    SOFT_USE_REG(arg1);
    __asm__ volatile(
        ".set\tnoreorder\n\t"
        ".set\tnomacro\n\t"
        "lui $a1, %%hi(D_actor_403600_801606E0)\n\t"
        "addiu $a0, $a1, %%lo(D_actor_403600_801606E0)\n\t"
        "andi $v0, %1, 2\n\t"
        "sw $zero, 0(%2)\n\t"
        "sh $zero, 16($a0)\n\t"
        "beqz $v0, 3f\n\t"
        "sh $zero, 20($a0)\n\t"
        "lw $v0, 28(%2)\n\t"
        "lh $v1, 18($a0)\n\t"
        "nop\n\t"
        "bne $v1, %4, 1f\n\t"
        "sw $v0, 4($a0)\n\t"
        "lui $v0, %%hi(D_actor_403600_801605EC)\n\t"
        "addiu $v1, $v0, %%lo(D_actor_403600_801605EC)\n\t"
        "lh $v0, %%lo(D_actor_403600_801605EC)($v0)\n\t"
        "lh $v1, 4($v1)\n\t"
        "j 2f\n\t"
        "move %0, $zero\n"
        "1:\n\t"
        "lui $v0, %%hi(D_actor_403600_801605E4)\n\t"
        "addiu $v1, $v0, %%lo(D_actor_403600_801605E4)\n\t"
        "lh $v0, %%lo(D_actor_403600_801605E4)($v0)\n\t"
        "lh $v1, 4($v1)\n\t"
        "li %0, 1\n"
        "2:\n\t"
        "sw $v0, %%lo(D_actor_403600_801606E0)($a1)\n\t"
        "j 4f\n\t"
        "sw $v1, 8($a0)\n"
        "3:\n\t"
        "lw $v0, 24(%2)\n\t"
        "nop\n\t"
        "sw $v0, %%lo(D_actor_403600_801606E0)($a1)\n\t"
        "lw $v0, 28(%2)\n\t"
        "nop\n\t"
        "sw $v0, 4($a0)\n\t"
        "lw $v0, 32(%2)\n\t"
        "nop\n\t"
        "sw $v0, 8($a0)\n"
        "4:\n\t"
        "move $a0, %3\n\t"
        "li $a1, 0x3e9\n\t"
        "lui $a2, %%hi(D_actor_403600_801606E0)\n\t"
        "addiu $a2, $a2, %%lo(D_actor_403600_801606E0)\n\t"
        "jal Gp_DispatchMsg\n\t"
        "move $a3, $zero\n\t"
        ".set\tmacro\n\t"
        ".set\treorder"
        : "+r"(var_s4)
        : "r"(arg1), "r"(temp_s3), "r"(temp_s7), "r"(var_s2)
        : "v0", "a0", "a1", "a2", "a3", "memory");
    return var_s4;
}

void func_actor_403600_8013EA04(Task* arg0)
{
    s16              temp_a3;
    u32              temp_a0_3;
    u32              temp_a2;
    s32              temp_lo;
    s32              temp_lo_3;
    s32              temp_s0;
    s32              temp_s0_2;
    register s32     temp_threshold asm("a0");
    s32              temp_work_limit;
    s32              temp_v0_3;
    s32              temp_v1;
    s32              temp_v1_2;
    s32              temp_v1_3;
    s32              temp_v1_4;
    s32              delta;
    u32              temp_a0;
    u32              temp_a0_2;
    u32              temp_a0_4;
    u32              temp_a1;
    u32              temp_a2_2;
    u32              temp_v0;
    u32              temp_v0_2;
    u32              temp_v0_4;
    u32              var_v0;
    u32              var_v1;
    s32              temp_lo_2;
    s32              temp_lo_4;
    register s32     case_value asm("a1");
    register Task*   call_actor asm("a0");
    s32              var_a2;
    Actor403600Work* temp_s2;
    GpEnemy*         temp_t0;

    temp_s2            = arg0->work;
    temp_t0            = arg0->spawnArg2;
    var_a2             = 0;
    temp_s2->field_772 = 0;
    if (temp_s2->field_4B4 != 0) {
        var_v0      = (Gp_LcgState * 5) + 0x71357911;
        var_a2      = 3;
        Gp_LcgState = var_v0;
        if ((var_v0 >> 0x10) & 1) {
            var_a2 = 2;
        }
    } else {
        temp_v1 = Player_Status.coordMtx->t[1];
        if (temp_v1 >= -0x7D0) {
            temp_a0            = (Gp_LcgState * 5) + 0x71357911;
            temp_s2->field_772 = 2;
            temp_v1_2          = (temp_a0 >> 0x10) & 0xF;
            Gp_LcgState        = temp_a0;
            if (temp_v1_2 < 3) {
                var_a2 = 2;
            } else if (temp_v1_2 < 6) {
                var_a2 = 3;
            } else if (temp_v1_2 < 0xB) {
                var_a2 = 5;
            } else {
                temp_v0     = (temp_a0 * 5) + 0x71357911;
                Gp_LcgState = temp_v0;
                var_a2      = 4;
                if ((temp_v0 >> 0x10) & 1) {
                    var_a2 = 6;
                }
            }
        } else if (temp_v1 >= -0x1004) {
            if (((u32)(Player_Status.coordMtx->t[0] - 0xFA0) < 0x1F41U) &&
                ((u32)(Player_Status.coordMtx->t[2] - 0xBB8) < 0x1F41U)) {
                temp_a1            = (Gp_LcgState * 5) + 0x71357911;
                temp_s2->field_772 = 1;
                temp_threshold     = temp_t0->hp;
                temp_v1_3          = temp_a1 >> 0x10;
                temp_v1_3         &= 0xF;
                temp_work_limit    = temp_s2->field_798;
                Gp_LcgState        = temp_a1;
                if (temp_work_limit < temp_threshold) {
                    var_a2 = 1;
                    if (temp_v1_3 & 1) {
                        var_a2 = 4;
                    }
                } else {
                    if (temp_v1_3 < 2) {
                        var_a2 = 4;
                    } else if (temp_v1_3 < 6) {
                        var_a2 = 3;
                    } else {
                        temp_v0_2   = (temp_a1 * 5) + 0x71357911;
                        Gp_LcgState = temp_v0_2;
                        var_a2      = 1;
                        if ((temp_v0_2 >> 0x10) & 1) {
                            var_a2 = 2;
                        }
                    }
                }
            } else {
                temp_a0_2          = (Gp_LcgState * 5) + 0x71357911;
                temp_s2->field_772 = 3;
                temp_v1_4          = (temp_a0_2 >> 0x10) & 0xF;
                Gp_LcgState        = temp_a0_2;
                if (temp_v1_4 < 2) {
                    var_a2 = 3;
                } else if (temp_v1_4 < 5) {
                    var_a2 = 6;
                } else if (temp_v1_4 < 8) {
                    var_a2 = 4;
                } else {
                    var_v0      = (temp_a0_2 * 5) + 0x71357911;
                    Gp_LcgState = var_v0;
                    var_a2      = 5;
                    if ((var_v0 >> 0x10) & 1) {
                        var_a2 = 2;
                    }
                }
            }
        }
    }
    if (D_actor_403600_80160695 != 0) {
        var_a2 = D_actor_403600_80160694;
    } else {
        if (!((((u32)(var_a2 - 2) >= 2U) || (temp_t0->hp <= temp_s2->field_798)) &&
              ((var_a2 != 5) ||
               ((temp_t0->hp <= temp_s2->field_79A) && (temp_s2->field_7B0 < 0xA))) &&
              ((D_actor_403600_801606B8.values[0] != var_a2) ||
               (D_actor_403600_801606B8.values[1] != var_a2)))) {
            return;
        }
        D_actor_403600_801606B8.values[D_actor_403600_801606BC] = (u16)var_a2;
        D_actor_403600_801606BC                                ^= 1;
    }
    var_v1 = var_a2 - 1;
    switch (var_v1) {
        case 0:
            temp_s2->field_746 = 1;
            temp_s2->field_75E = 0;
            temp_s2->field_732 = 0;
            temp_s2->field_73E = 0xA;
            return;
        case 1:
            temp_s2->field_736 = 0x10;
            temp_s2->field_734 = 0x32;
            temp_s2->field_746 = 0;
            temp_s2->field_73E = 0x14;
            return;
        case 2:
            delta     = Player_Status.coordMtx->t[0] - D_actor_403600_8016063C.x0;
            temp_lo   = delta * delta;
            delta     = Player_Status.coordMtx->t[2] - D_actor_403600_8016063C.z0;
            temp_s0   = SquareRoot0(temp_lo + (delta * delta));
            delta     = Player_Status.coordMtx->t[0] - D_actor_403600_8016063C.x1;
            temp_lo_2 = delta * delta;
            delta     = Player_Status.coordMtx->t[2] - D_actor_403600_8016063C.z1;
            temp_v0_3 = SquareRoot0(temp_lo_2 + (delta * delta));
            if (temp_s2->field_772 == 1) {
                if (temp_s0 < temp_v0_3) {
                    temp_s2->field_770    = 0U;
                    temp_s2->field_6F0.vx = (s32)D_actor_403600_8016063C.x1;
                    temp_s2->field_6F0.vy = (s32)D_actor_403600_8016063C.y1;
                    temp_s2->field_6F0.vz = (s32)D_actor_403600_8016063C.z1;
                } else {
                    temp_s2->field_770    = 1U;
                    temp_s2->field_6F0.vx = (s32)D_actor_403600_8016063C.x0;
                    temp_s2->field_6F0.vy = (s32)D_actor_403600_8016063C.y0;
                    temp_s2->field_6F0.vz = (s32)D_actor_403600_8016063C.z0;
                }
            } else {
                if (temp_s0 < temp_v0_3) {
                    temp_s2->field_770    = 0U;
                    temp_s2->field_6F0.vx = (s32)D_actor_403600_8016063C.x0;
                    temp_s2->field_6F0.vy = (s32)D_actor_403600_8016063C.y0;
                    temp_s2->field_6F0.vz = (s32)D_actor_403600_8016063C.z0;
                } else {
                    temp_s2->field_770    = 1U;
                    temp_s2->field_6F0.vx = (s32)D_actor_403600_8016063C.x1;
                    temp_s2->field_6F0.vy = (s32)D_actor_403600_8016063C.y1;
                    temp_s2->field_6F0.vz = (s32)D_actor_403600_8016063C.z1;
                }
                delta     = Player_Status.coordMtx->t[0] - D_actor_403600_8016064C.x0;
                temp_lo_3 = delta * delta;
                delta     = Player_Status.coordMtx->t[2] - D_actor_403600_8016064C.z0;
                temp_s0_2 = SquareRoot0(temp_lo_3 + (delta * delta));
                delta     = Player_Status.coordMtx->t[0] - D_actor_403600_8016064C.x1;
                temp_lo_4 = delta * delta;
                delta     = Player_Status.coordMtx->t[2] - D_actor_403600_8016064C.z1;
                if (SquareRoot0(temp_lo_4 + (delta * delta)) < temp_s0_2) {
                    temp_s2->field_770 = (u16)(temp_s2->field_770 | 2);
                }
            }
            temp_s2->field_774 = 1;
            temp_s2->field_732 = 0;
            temp_s2->field_746 = 0;
            temp_s2->field_734 = 0;
            temp_s2->field_73E = 0x1E;
            return;
        case 3:
            temp_v0_4                = (Gp_LcgState * 5) + 0x71357911;
            call_actor               = arg0;
            case_value               = 1;
            temp_s2->field_774       = case_value;
            temp_s2->field_746       = case_value;
            temp_a2                  = temp_v0_4 * 5;
            temp_a2_2                = temp_a2 + 0x71357911;
            case_value               = 0x28;
            temp_s2->field_73E       = case_value;
            case_value               = 1;
            temp_s2->field_732       = 0;
            Gp_LcgState              = temp_v0_4;
            temp_s2->field_782       = (s16)((temp_v0_4 >> 0xF) & 6);
            Gp_LcgState              = temp_a2_2;
            temp_s2->field_5C0.flags = (u16)(temp_s2->field_5C0.flags | 0x4000);
            temp_a3                  = ((temp_a2_2 >> 0x10) % 0x14) + 0x28;
            temp_s2->field_734       = temp_a3;
            func_actor_403600_8013CCEC(call_actor, case_value);
            return;
        case 4:
            temp_a0_3          = Gp_LcgState * 5;
            temp_a0_4          = temp_a0_3 + 0x71357911;
            temp_s2->field_746 = 0;
            temp_s2->field_736 = 2;
            temp_s2->field_73E = 0x32;
            temp_lo_2          = (s16)((u16)temp_s2->field_7B0 + 1);
            Gp_LcgState        = temp_a0_4;
            temp_s2->field_7B0 = temp_lo_2;
            temp_s2->field_734 = (s16)(((temp_a0_4 >> 0x10) % 0x14) + 0x28);
            return;
        case 5:
            temp_s2->field_736 = 2;
            temp_s2->field_734 = 0xD2;
            temp_s2->field_73E = 0x3C;
            temp_s2->field_792 = 0x14;
            temp_s2->field_794 = 0x13;
            break;
    }
}

void func_actor_403600_8013F0C0(Task* arg0)
{
    s16              temp_v1;
    s16              temp_v1_2;
    s16              temp_v1_3;
    s16              temp_v1_4;
    s32              temp_s2;
    s32              temp_s4;
    s32              var_v0_2;
    s32              temp_s0;
    s32              temp_s0_2;
    s32              temp_s0_3;
    s32              temp_s0_4;
    s32              var_v0;
    u16              temp_v0;
    register u16     temp_v0_2 asm("v0");
    GpCoord*         temp_a0;
    GpCoord*         temp_a0_2;
    GpCoord*         temp_a0_3;
    Actor403600Work* temp_a1;
    Actor403600Work* temp_a1_2;
    Actor403600Work* temp_a1_3;
    Actor403600Work* temp_s3;

    temp_s4 = (s32)Gp_ActorSlots[0]->extra.tmd->coords;
    temp_s3 = arg0->work;
    switch (D_actor_403600_8016056C) {
        case 1:
            temp_s3->field_760 = (u16)(temp_s3->field_760 + 1);
            temp_a1            = arg0->work;
            temp_a0            = Gp_ActorSlots[0]->extra.tmd->coords;
            temp_a0->coord.t[0] =
                (s32)(temp_a0->coord.t[0] +
                      ((s32)(temp_a0->coord.m[0][2] * temp_a1->field_762) >> 0xC));
            temp_a0->coord.t[2] =
                (s32)(temp_a0->coord.t[2] +
                      ((s32)(temp_a0->coord.m[2][2] * temp_a1->field_762) >> 0xC));
            if (((s16)temp_s3->field_760 >= 0xC) || (gGameSession->viewReady != 0)) {
                temp_s3->field_760 = 0;
                if (func_actor_403600_8013E7D4((s32)arg0, 3) == 0) {
                    Mc_SaveData.at4.loc.view = 7;
                } else {
                    Mc_SaveData.at4.loc.view = 3;
                }
                D_actor_403600_80160568.field_4 = 2;
                Gp_StateC08.field_6            |= 1;
                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, (s32)&D_actor_403600_80160568, 0);
                goto end;
            }
            break;

        case 2:
            temp_v0            = temp_s3->field_760 + 1;
            temp_s3->field_760 = temp_v0;
            if ((s16)temp_v0 == 0xC) {
                Gp_SpawnPadLerp(0xA, 0xFF, 0xFF);
                D_actor_403600_801606A4.field_0 = 0x14;
                D_actor_403600_801606A4.field_2 = 0;
                temp_s2                         = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 6;
                temp_s0                         = (s8)Gp_GetObjPan(temp_s4);
                SndEvt_EnqueueType6(temp_s2, temp_s0,
                                    (s8)gpGetObjDepth(temp_s4));
                temp_s2 =
                    (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160011;
                temp_s0_2 = (s8)Gp_GetObjPan(temp_s4);
                SndEvt_EnqueueType6(temp_s2, temp_s0_2,
                                    (s8)gpGetObjDepth(temp_s4));
                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F9,
                               Gp_PackPair(&D_actor_403600_801606A4, 0), 0);
            }
            var_v0_2 = (s16)temp_s3->field_760 < 0x66;
            goto check_timeout;

        case 3:
            temp_s3->field_760 = (u16)(temp_s3->field_760 + 1);
            temp_v1            = (u16)temp_s3->field_762 + 2;
            temp_s3->field_762 = temp_v1;
            if ((temp_v1 << 0x10) > 0) {
                temp_s3->field_762 = 0;
            }
            temp_a1_2 = arg0->work;
            temp_a0_2 = Gp_ActorSlots[0]->extra.tmd->coords;
            temp_a0_2->coord.t[0] =
                (s32)(temp_a0_2->coord.t[0] +
                      ((s32)(temp_a0_2->coord.m[0][2] * temp_a1_2->field_762) >> 0xC));
            temp_a0_2->coord.t[2] =
                (s32)(temp_a0_2->coord.t[2] +
                      ((s32)(temp_a0_2->coord.m[2][2] * temp_a1_2->field_762) >> 0xC));
            temp_v1_2 = temp_s3->field_73E;
            if ((temp_v1_2 != 0x3C) && (temp_v1_2 != 0x28) &&
                ((s16)temp_s3->field_760 == 0xC)) {
                temp_s2 =
                    (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160012;
                temp_s0_3 = (s8)Gp_GetObjPan(temp_s4);
                SndEvt_EnqueueType6(temp_s2, temp_s0_3,
                                    (s8)gpGetObjDepth(temp_s4));
            }
            if ((s16)temp_s3->field_760 >= 0x24) {
                Gp_StateC08.field_6            |= 1;
                temp_s3->field_760              = 0;
                D_actor_403600_80160568.field_4 = 5;
                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, (s32)&D_actor_403600_80160568, 0);
                goto end;
            }
            break;

        case 4:
            temp_s3->field_760 = (u16)(temp_s3->field_760 + 1);
            temp_v1_3          = (u16)temp_s3->field_762 - 2;
            temp_s3->field_762 = temp_v1_3;
            if (temp_v1_3 < 0) {
                temp_s3->field_762 = 0;
            }
            temp_a1_3 = arg0->work;
            temp_a0_3 = Gp_ActorSlots[0]->extra.tmd->coords;
            temp_a0_3->coord.t[0] =
                (s32)(temp_a0_3->coord.t[0] +
                      ((s32)(temp_a0_3->coord.m[0][2] * temp_a1_3->field_762) >> 0xC));
            temp_a0_3->coord.t[2] =
                (s32)(temp_a0_3->coord.t[2] +
                      ((s32)(temp_a0_3->coord.m[2][2] * temp_a1_3->field_762) >> 0xC));
            temp_v1_4 = temp_s3->field_73E;
            if ((temp_v1_4 != 0x3C) && (temp_v1_4 != 0x28) &&
                ((s16)temp_s3->field_760 == 0xC)) {
                temp_s2 =
                    (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54160012;
                temp_s0_4 = (s8)Gp_GetObjPan(temp_s4);
                SndEvt_EnqueueType6(temp_s2, temp_s0_4,
                                    (s8)gpGetObjDepth(temp_s4));
            }
            if ((s16)temp_s3->field_760 >= 0x24) {
                Gp_StateC08.field_6            |= 1;
                temp_s3->field_760              = 0;
                D_actor_403600_80160568.field_4 = 6;
                Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F4, (s32)&D_actor_403600_80160568, 0);
                goto end;
            }
            break;

        case 5:
        case 6:
            temp_v0_2          = temp_s3->field_760 + 1;
            temp_s3->field_760 = temp_v0_2;
            SOFT_TOUCH_REG(temp_v0_2);
            var_v0_2 = (s16)temp_v0_2 < 0x28;
            goto check_timeout;
    }
    goto end;

check_timeout:
    if (var_v0_2 != 0) {
        goto end;
    }
    temp_s3->field_760      = 0;
    D_actor_403600_8016056C = 0;
    Gp_DispatchMsg(Gp_ActorSlots[0], 0x3F1, 0, 0);

end:
    return;
}

void func_actor_403600_8013F608(Task* arg0)
{
    Actor403600Work* work;
    s16              temp_v0_3;
    s16              temp_v1;
    s32              temp_arg2;
    s32              var_check;
    s32              var_s1;
    u16              temp_field;
    u16              temp_v0;
    u16              temp_v0_2;
    u16              temp_v0_4;
    u32              temp_t0;
    u32              temp_v0_5;

    work    = arg0->work;
    temp_v1 = work->field_76C;
    if (temp_v1 == -1) {
        temp_v0         = (u16)work->field_768 + 1;
        work->field_768 = temp_v0;
        if ((s16)temp_v0 >= 3) {
            var_s1          = 1;
            work->field_768 = 0;
            do {
                Gp_SpawnEff(0x60080, (*Gp_ActorSlots)->extra.tmd->coords + var_s1, 0x400, NULL);
                var_s1 += 1;
            } while (var_s1 < 0x13);
        }
    } else {
        temp_v0_2       = (u16)work->field_768 + 1;
        work->field_768 = temp_v0_2;
        var_check       = (s16)temp_v0_2 < temp_v1;
        temp_field      = (u16)work->field_76C;
        if (!var_check) {
            temp_v0_3       = temp_field - 8;
            work->field_76C = temp_v0_3;
            if (temp_v0_3 < 3) {
                work->field_76C = 2;
            }
            temp_v0_4       = (u16)work->field_76A + 1;
            work->field_76A = temp_v0_4;
            if ((s16)temp_v0_4 >= 0x400) {
                work->field_76A = 0x400;
            }
            temp_v0_5   = (Gp_LcgState * 5) + 0x71357911;
            temp_t0     = temp_v0_5 >> 0x10;
            temp_arg2   = work->field_76A;
            Gp_LcgState = temp_v0_5;
            Gp_SpawnEff(0x60080,
                        (u8*)(*Gp_ActorSlots)->extra.tmd->coords + (((temp_t0 % 19) & 0xFFFF) * 0x50),
                        temp_arg2, NULL);
            work->field_768 = 0;
        }
    }
}

void func_actor_403600_8013F7B8(GpEnemy* enemy, Task* task)
{
    SVECTOR                rot;
    TmdObject*             model;
    GpCoord*               worldCoord;
    GpCoord*               modelCoord;
    GpCoord*               bodyCoord;
    GpRec18*               bodyRecs;
    GpRec18*               attackRecs;
    Actor403600Work*       animWork;
    Actor403600Work*       ownerWork;
    Actor403600Work*       work;
    u8*                    anim;
    s16                    animId;
    u32                    randomProduct;
    s32                    angle;
    s32                    i;
    s32                    animIndex;
    u32                    randomState;
    Actor403600TurnMatrix* worldMatrix;
    Actor403600TurnMatrix* modelMatrix;

    model      = task->extra.tmd;
    modelCoord = model->coords;
    ownerWork  = D_actor_403600_801606A8->work;
    work       = memCalloc(sizeof(Actor403600Work), false);
    bodyCoord  = &modelCoord[1];
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    worldCoord                 = &work->field_4B8;
    task->work                 = work;
    model->flags               = 2;
    model->lightLevel          = 0;
    work->field_4B8.sub        = &gGfxViewCoord;
    worldMatrix                = (Actor403600TurnMatrix*)&work->field_4B8.coord;
    worldMatrix->field_0       = 0x1000;
    worldMatrix->field_4       = 0;
    worldMatrix->field_8       = 0x1000;
    worldMatrix->field_C       = 0;
    worldMatrix->field_10      = 0x1000;
    modelMatrix                = (Actor403600TurnMatrix*)&modelCoord->coord;
    work->field_4B8.coord.t[0] = 0;
    work->field_4B8.coord.t[1] = 0;
    work->field_4B8.coord.t[2] = 0;
    modelCoord->sub            = worldCoord;
    modelMatrix->field_0       = 0x1000;
    modelMatrix->field_4       = 0;
    modelMatrix->field_8       = 0x1000;
    modelMatrix->field_C       = 0;
    modelMatrix->field_10      = 0x1000;
    modelCoord->coord.t[0]     = 0;
    modelCoord->coord.t[1]     = 0x744;
    modelCoord->coord.t[2]     = 0;
    work->field_4B8.flg        = 0;
    Gp_UpdateCoord(worldCoord);
    modelCoord->flg = 0;
    Gp_UpdateCoord(modelCoord);
    model->lightMtx = &work->field_494;
    model->colorMtx = &work->field_474;
    enemy->field_4  = &modelCoord[1].coord;
    enemy->field_48 = 0;
    Gp_LinkNode(&enemy->node);
    enemy->node.state.b.flags = 8;
    enemy->coord              = bodyCoord;
    enemy->bodyPos.vx         = 0;
    enemy->bodyPos.vy         = 0;
    enemy->bodyPos.vz         = 0;
    enemy->param              = &D_actor_403600_80150ED8;
    enemy->recs               = (s32)work->field_528;
    enemy->hp                 = (s16)D_actor_403600_80150ED8.hpMax;
    func_800B3F84(&work->rig.anim, D_actor_403600_8016057C, model, work->rig.poses, work->rig.slots);
    i = 1;
    do {
        Gp_AnimResetSlot(&work->rig.anim, i, 1);
        i += 1;
    } while (i < 0x14);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    bodyRecs        = work->field_528;
    work->field_736 = 9;
    work->field_738 = 0;
    work->field_744 = 0;
    work->field_6C0 =
        &task->extra.tmd->coords[1];
    work->field_6C4          = 0x100;
    work->field_6C6          = 1;
    work->field_6E8.vx       = 0;
    work->field_6E8.vy       = 0;
    work->field_6E8.vz       = 0;
    work->field_73E          = 0;
    work->field_508.coord    = bodyCoord;
    work->field_508.ctx.recs = bodyRecs;
    work->field_508.pos.vx   = 0;
    work->field_508.pos.vy   = 0;
    work->field_508.pos.vz   = 0;
    work->field_508.key      = 0x30024;
    work->field_508.radius   = 0x3E8;
    work->field_508.flags    = 1;
    Gp_LinkObj(2, &work->field_508);
    Gp_InitRec18Table(bodyRecs, 4, 0);
    attackRecs               = work->field_5A8;
    work->field_588.coord    = bodyCoord;
    work->field_588.ctx.recs = attackRecs;
    work->field_588.pos.vx   = 0;
    work->field_588.pos.vy   = 0;
    work->field_588.pos.vz   = 0x3E8;
    work->field_508.flags   |= 0xC200;
    work->field_588.key      = Gp_PackPair(&D_actor_403600_80150EB0, 0);
    work->field_588.radius   = 0x5DC;
    work->field_588.flags    = 1;
    Gp_LinkObj(3, &work->field_588);
    Gp_InitRec18Table(attackRecs, 1, 0);
    work->field_588.flags     &= 0x7FFF;
    work->field_4B8.coord.t[0] = (s32)ownerWork->field_4B8.coord.t[0];
    work->field_4B8.coord.t[1] = (s32)ownerWork->field_4B8.coord.t[1];
    work->field_4B8.coord.t[2] = (s32)ownerWork->field_4B8.coord.t[2];
    Gfx_MatrixCol2(&ownerWork->field_4B8.coord, &rot);
    angle  = ratan2((s32)rot.vx, (s32)rot.vz);
    rot.vx = 0;
    rot.vy = (s16)angle;
    rot.vz = 0;
    RotMatrix(&rot, &work->field_4B8.coord);
    randomProduct   = Gp_LcgState * 5;
    randomState     = randomProduct + 0x71357911;
    work->field_748 = (s16)angle;
    work->field_756 = 0;
    work->field_742 = 0;
    work->field_74E = 0;
    work->field_73E = 5;
    work->field_74C = 0xA;
    work->field_750 = 0x14;
    work->field_754 = (s16)(((randomState >> 0x10) % 0x32) + 0xBB8);
    Gp_LcgState     = randomState;
    if (task->spawnArg1 != 0) {
        Gp_AssignNodeSlot0(&enemy->node);
    }
    animWork = task->work;
    animId   = animWork->field_736;
    if (D_actor_403600_8016057C[animId] != 0) {
        animIndex = 1;
        if (animId != animWork->field_738) {
            animWork->field_738 = (s16)(u16)animWork->field_736;
            animWork->field_73A = 0U;
            do {
                func_800B4114(&animWork->rig.anim, animIndex,
                              (s32)animWork->field_736, 0,
                              (s32)animWork->field_756);
                animIndex += 1;
            } while (animIndex < 0x14);
        } else {
            TOUCH_REG(animIndex);
            animWork->field_73A += animIndex;
            /* A byte cursor 0x28 bytes into the block reaches each slot's `rate` at
             * +0x1D. The slot index is hidden from the compiler for the update
             * above, so indexing `rig.slots` would derive the cursor from it with
             * a multiply instead of this constant start. */
            anim = (u8*)animWork + 0x28;
            do {
                anim[0x1D] = (u8)animWork->field_778;
                Gp_AnimTickIndex(&animWork->rig.anim, animIndex);
                animIndex += 1;
                anim      += sizeof(GpAnimSlot);
            } while (animIndex < 0x14);
        }
    }
    work->field_778    = 0x10;
    task->exitCallback = func_actor_403600_80141598;
    task->state       += 1;
}

void func_actor_403600_8013FC2C(GpEnemy* arg0, Task* arg1)
{
    s16                       temp_a0;
    s16                       temp_v0_3;
    s16                       temp_v0_4;
    s16                       temp_v0_5;
    s32                       state;
    s32                       var_s0;
    u16                       temp_v0_6;
    u16                       temp_v0_8;
    u16                       temp_v0_9;
    VECTOR*                   temp_a1_2;
    MATRIX*                   temp_a1_5;
    register MATRIX*          matrix_head asm("s1");
    register MATRIX*          matrix_arg asm("a1");
    register MATRIX*          gte_block asm("s2");
    register SVECTOR*         rot_arg asm("a0");
    register TmdObject*       coord_object asm("v0");
    VECTOR*                   temp_a1_7;
    register GpCoord*         temp_s0 asm("s0");
    s16*                      temp_s0_2;
    Actor403600Work*          temp_s1;
    Actor403600Work*          temp_s3;
    Actor403600Work*          temp_s4;
    TmdObject*                temp_s7;
    s16*                      temp_v0;
    s16*                      temp_v0_2;
    u8*                       restore1;
    u8*                       restore2;
    u8*                       var_s2;
    register GpEnemy*         color_actor1 asm("a0");
    register s32              color_zero1 asm("a2");
    register GpEnemy*         color_actor2 asm("a0");
    register s32              color_zero2 asm("a2");
    register Actor403600Work* color_work2 asm("v0");
    register s32              color_z1 asm("v0");
    register s32              color_arg3_1 asm("a3");
    register s32              color_arg3_2 asm("a3");

    temp_s7 = arg1->extra.tmd;
    temp_s4 = arg1->work;
    state   = Gp_StateF0.field_4;
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
    color_actor1 = arg0;
    actor_403600_load_scratch_head(temp_a1_2);
    temp_a1_2[-1].vx = temp_s4->field_4B8.workm.t[0];
    color_zero1      = 0;
    temp_a1_2        = temp_a1_2 - 1;
    temp_a1_2->vy    = temp_s4->field_4B8.workm.t[1];
    actor_403600_color_tail(color_z1, color_arg3_1, temp_a1_2, temp_s4, color_zero1);
    temp_a1_2->vz = color_z1;
    Gp_UpdateActorColor(color_actor1, temp_a1_2, color_zero1, color_arg3_1);
    actor_403600_load_scratch_head_nop(restore1);
    restore1 += 0x10;
    actor_403600_store_scratch_head(restore1);
    goto end;

case2:
    temp_s7->flags           = 0x80;
    arg0->node.state.b.flags = 9;
    goto end;

default_body:
    func_actor_403600_80141C3C(arg1);
    func_actor_403600_8013DC7C(arg1);
    func_actor_403600_8013D15C(arg1);
    temp_s1 = arg1->work;
    temp_a0 = temp_s1->field_736;
    if (D_actor_403600_8016057C[temp_a0] != 0) {
        var_s0 = 1;
        if (temp_a0 != temp_s1->field_738) {
            temp_s1->field_738 = (s16)(u16)temp_s1->field_736;
            temp_s1->field_73A = 0U;
            do {
                func_800B4114(&temp_s1->rig.anim, var_s0, temp_s1->field_736, 0,
                              (s32)temp_s1->field_756);
                var_s0 += 1;
            } while (var_s0 < 0x14);
        } else {
            TOUCH_REG(var_s0);
            temp_s1->field_73A += var_s0;
            /* A byte cursor 0x28 bytes into the block reaches each slot's `rate` at
             * +0x1D. The slot index is hidden from the compiler for the update
             * above, so indexing `rig.slots` would derive the cursor from it with
             * a multiply instead of this constant start. */
            var_s2 = (u8*)temp_s1 + 0x28;
            do {
                var_s2[0x1D] = (u8)temp_s1->field_778;
                Gp_AnimTickIndex(&temp_s1->rig.anim, var_s0);
                var_s0 += 1;
                var_s2 += 0x28;
            } while (var_s0 < 0x14);
        }
    }
    temp_s3 = arg1->work;
    actor_403600_load_scratch_head(matrix_head);
    actor_403600_rot_setup(coord_object, rot_arg, matrix_arg, temp_s0, arg1, temp_s3, matrix_head);
    actor_403600_store_scratch_head(matrix_arg);
    gte_block = matrix_arg;
    RotMatrix(rot_arg, matrix_arg);
    temp_v0 = &temp_s0[2].coord.m[0][0];
    gte_SetRotMatrix(temp_v0);
    gte_ldclmv(gte_block);
    gte_rtir();
    gte_stclmv(temp_v0);
    gte_ldclmv(&matrix_head[-1].m[0][1]);
    gte_rtir();
    temp_v0_2 = &temp_s0[2].coord.m[0][1];
    gte_stclmv(temp_v0_2);
    matrix_head = (MATRIX*)&matrix_head[-1].m[0][2];
    gte_ldclmv(matrix_head);
    gte_rtir();
    temp_s0_2 = &temp_s0[2].coord.m[0][2];
    gte_stclmv(temp_s0_2);
    temp_v0_3 = temp_s3->field_700;
    if (temp_v0_3 != 0) {
        if (temp_v0_3 >= 0x20) {
            temp_v0_4          = (u16)temp_s3->field_700 - 0x20;
            temp_s3->field_700 = temp_v0_4;
            if ((temp_v0_4 << 0x10) <= 0) {
                temp_s3->field_700 = 0;
            }
        }
        if (temp_s3->field_700 < 0x21) {
            temp_v0_5          = (u16)temp_s3->field_700 + 0x20;
            temp_s3->field_700 = temp_v0_5;
            if ((temp_v0_5 << 0x10) >= 0) {
                temp_s3->field_700 = 0;
            }
        }
    }
    actor_403600_load_scratch_head(temp_a1_5);
    actor_403600_coord_advance(temp_a1_5, temp_s4);
    actor_403600_store_scratch_head(temp_a1_5);
    Gp_UpdateCoord(&temp_s4->field_4B8);
    temp_v0_6          = temp_s4->field_74C + 1;
    temp_s4->field_74C = temp_v0_6;
    if ((s16)temp_v0_6 >= 0xA) {
        color_actor2       = arg0;
        temp_s4->field_74C = 0U;
        color_work2        = arg1->work;
        actor_403600_load_scratch_head(temp_a1_7);
        temp_a1_7[-1].vx = color_work2->field_4B8.workm.t[0];
        color_zero2      = 0;
        temp_a1_7        = temp_a1_7 - 1;
        temp_a1_7->vy    = color_work2->field_4B8.workm.t[1];
        actor_403600_color_tail_in_place(color_work2, color_arg3_2, temp_a1_7, color_zero2);
        temp_a1_7->vz = (s32)color_work2;
        Gp_UpdateActorColor(color_actor2, temp_a1_7, color_zero2, color_arg3_2);
        actor_403600_load_scratch_head_nop(restore2);
        restore2 += 0x10;
        actor_403600_store_scratch_head(restore2);
    }
    temp_v0_8          = temp_s4->field_74E + 1;
    temp_s4->field_74E = temp_v0_8;
    if ((((s16)temp_v0_8 % 42) << 0x10) == 0) {
        temp_v0_9          = temp_s4->field_750 + 1;
        temp_s4->field_750 = temp_v0_9;
        if ((s16)temp_v0_9 >= 0x64) {
            temp_s4->field_750 = 0x64U;
        }
    }
    if (((s16)temp_s4->field_74E >= temp_s4->field_754) || (temp_s4->field_742 != 0)) {
        arg0->node.state.b.flags = 1;
        temp_s7->lightLevel      = 0x12C;
        arg1->killCountdown      = 0x3C;
        arg1->state              = (s32)(arg1->state + 1);
    }

end:
    return;
}

/// Handlers for states 0-2 of the task `func_actor_403600_80141BE0` dispatches,
/// indexed by `Task::state`. The state-0 handler sets the task up and
/// advances it.
const GpEnemyTaskFuncTable3 D_actor_403600_801320A0 = { {
    func_actor_403600_8013F7B8,
    func_actor_403600_8013FC2C,
    func_actor_403600_80140488,
} };

void func_actor_403600_801400BC(Task* arg0)
{
    u32              sp10;
    s32              sp14;
    s16              temp_v1;
    s32              temp_s2;
    s32              temp_v0;
    s32              temp_v1_2;
    s32              var_v0;
    s32              var_v0_2;
    s32              var_v0_3;
    s32              temp_s0_2;
    s32              temp_s0_4;
    u32              temp_v0_2;
    u32              temp_v0_3;
    GpCoord*         temp_s0;
    GpCoord*         temp_s0_3;
    Actor403600Work* temp_s1;

    temp_s1 = arg0->work;
    temp_v1 = temp_s1->field_73E;
    switch (temp_v1) {
        case 0:
            temp_s1->field_736 = 1;
            temp_s1->field_73C = 0U;
            temp_s1->field_74A = 0;
            if (temp_s1->field_73A >= 0x1E) {
                temp_s1->field_73E = 1;
                temp_s1->field_73A = 0;
                return;
            }
        default:
            return;
        case 1:
            func_actor_403600_8013DDF4(arg0, 0);
            temp_s1->field_736 = 2;
            temp_s1->field_73C = temp_s1->field_750;
            temp_v1_2          = Player_Status.coordMtx->t[1];
            temp_v0            = temp_s1->field_4B8.coord.t[1] + 0x3E8;
            temp_s1->field_74A = (s16)((temp_v1_2 - temp_v0) / 25);
            func_actor_403600_8013E470(&temp_s1->field_4B8, (s32*)&sp10, &sp14);
            if (sp10 < 0x835U) {
                var_v0 = sp14;
                if (var_v0 < 0) {
                    var_v0 = -var_v0;
                }
                if (var_v0 < 0x400) {
                    temp_s1->field_756 = 0;
                    temp_s1->field_73C = 0U;
                    temp_s1->field_73A = 0;
                    temp_s1->field_73E = 4;
                    return;
                }
            }
            break;
        case 3:
            temp_s1->field_588.key = Gp_PackPair(&D_actor_403600_80150EB0, 0);
            temp_s1->field_736     = 0xC;
            temp_s1->field_73C     = 0U;
            temp_s1->field_74A     = 0;
            if (temp_s1->field_73A == 0xE) {
                temp_s0   = &temp_s1->field_4B8;
                temp_s2   = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x5416000D;
                temp_s0_2 = (s8)Gp_GetObjPan(temp_s0);
                temp_v0_2 = gpGetObjDepth(temp_s0);
                SndEvt_EnqueueType6(temp_s2, temp_s0_2,
                                    (s32)(((temp_v0_2 >> 0x1F) + temp_v0_2) << 0x17) >> 0x18);
            }
            if (temp_s1->field_73A == 0x11) {
                temp_s1->field_588.flags = (u16)(temp_s1->field_588.flags | 0x8000);
            }
            if (temp_s1->field_73A == 0x15) {
                temp_s1->field_588.flags = (u16)(temp_s1->field_588.flags & 0x7FFF);
            }
            if (temp_s1->field_73A >= 0x1E) {
                temp_s1->field_73C = 0U;
                temp_s1->field_73A = 0;
                func_actor_403600_8013E470(&temp_s1->field_4B8, (s32*)&sp10, &sp14);
                if (sp10 < 0x7D1U) {
                    var_v0_2 = sp14;
                    if (var_v0_2 < 0) {
                        var_v0_2 = -var_v0_2;
                    }
                    if (var_v0_2 < 0x400) {
                        temp_s1->field_73E = 4;
                        return;
                    }
                }
                goto block_33;
            }
            break;
        case 4:
            temp_s1->field_588.key = Gp_PackPair(&D_actor_403600_80150EB0, 1);
            temp_s1->field_736     = 0xD;
            temp_s1->field_73C     = 0U;
            temp_s1->field_74A     = 0;
            if (temp_s1->field_73A == 9) {
                temp_s0_3 = &temp_s1->field_4B8;
                temp_s2   = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x5416000D;
                temp_s0_4 = (s8)Gp_GetObjPan(temp_s0_3);
                temp_v0_3 = gpGetObjDepth(temp_s0_3);
                SndEvt_EnqueueType6(temp_s2, temp_s0_4,
                                    (s32)(((temp_v0_3 >> 0x1F) + temp_v0_3) << 0x17) >> 0x18);
            }
            if (temp_s1->field_73A == 0xA) {
                temp_s1->field_588.flags = (u16)(temp_s1->field_588.flags | 0x8000);
            }
            if (temp_s1->field_73A == 0xE) {
                temp_s1->field_588.flags = (u16)(temp_s1->field_588.flags & 0x7FFF);
            }
            if (temp_s1->field_73A >= 0x23) {
                temp_s1->field_756 = 8;
                temp_s1->field_73C = 0U;
                temp_s1->field_73A = 0;
                func_actor_403600_8013E470(&temp_s1->field_4B8, (s32*)&sp10, &sp14);
                if (sp10 < 0x7D1U) {
                    var_v0_3 = sp14;
                    if (var_v0_3 < 0) {
                        var_v0_3 = -var_v0_3;
                    }
                    if (var_v0_3 < 0x400) {
                        temp_s1->field_73E = 3;
                        return;
                    }
                    goto block_33;
                }
                goto block_33;
            }
            break;
        block_33:
            temp_s1->field_73E = 0;
            return;
        case 5:
            temp_s1->field_736 = 9;
            temp_s1->field_73C = 0U;
            temp_s1->field_74A = 0;
            if (temp_s1->field_73A >= 0x46) {
                temp_s1->field_73E = 1;
                temp_s1->field_73A = 0;
                temp_s1->field_756 = 8;
            }
            break;
    }
}

void func_actor_403600_80140488(GpEnemy* arg0, Task* arg1)
{
    s32              state;
    s32              i;
    s16              countdown;
    TmdObject*       object;
    Actor403600Work* initialWork;
    Actor403600Work* globalWork;
    Actor403600Work* cleanupWork;
    Actor403600Work* commonWork;
    GpEnemy*         enemy;
    u8*              anim;

    object      = arg1->extra.tmd;
    initialWork = arg1->work;
    globalWork  = D_actor_403600_801606A8->work;
    state       = Gp_StateF0.field_4;
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
    if (globalWork->field_742 != 1) {
        return;
    }
    goto default_body;
case2:
    object->flags           |= 0x80;
    arg0->node.state.b.flags = 1;
    return;
default_body:
    if (initialWork->field_732 == 0) {
        goto inner0;
    }
    if (initialWork->field_732 == 1) {
        goto inner1;
    }
    goto common;
inner0:
    object->lightLevel    += 3;
    arg1->extra.tmd->flags = 0;
    countdown              = (u16)arg1->killCountdown - 1;
    arg1->killCountdown    = countdown;
    if ((countdown << 0x10) <= 0) {
        initialWork->field_732 = 1;
        initialWork->field_734 = 0;
    }
    goto common;
inner1:
    Gp_ReleaseStateF0Add(arg1, 0x24);
    globalWork->field_4B4        = NULL;
    enemy                        = arg1->spawnArg2;
    cleanupWork                  = arg1->work;
    arg1->extra.tmd->coords->sub = &gGfxViewCoord;
    enemy->recs                  = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&cleanupWork->field_508);
    Gp_UnlinkObj(&cleanupWork->field_588);
    if (arg1 == D_actor_403600_801606A8) {
        Gp_UnlinkObj(&cleanupWork->field_5C0);
    }
    Gp_EnemyTaskExit(arg1);
    return;
common:
    commonWork = arg1->work;
    if (D_actor_403600_8016057C[(s16)commonWork->field_736] != 0) {
        i = 1;
        if ((s16)commonWork->field_736 != commonWork->field_738) {
            commonWork->field_738 = commonWork->field_736;
            commonWork->field_73A = 0;
            do {
                func_800B4114(&commonWork->rig.anim, i, commonWork->field_736, 0, commonWork->field_756);
                i++;
            } while (i < 0x14);
        } else {
            TOUCH_REG(i);
            commonWork->field_73A += i;
            /* A byte cursor 0x28 bytes into the block reaches each slot's `rate` at
             * +0x1D. The slot index is hidden from the compiler for the update
             * above, so indexing `rig.slots` would derive the cursor from it with
             * a multiply instead of this constant start. */
            anim = (u8*)commonWork + 0x28;
            do {
                anim[0x1D] = (u8)commonWork->field_778;
                Gp_AnimTickIndex(&commonWork->rig.anim, i);
                i++;
                anim += sizeof(GpAnimSlot);
            } while (i < 0x14);
        }
    }
}

s32 func_actor_403600_801406A4(Task* arg0, s32 arg1, GpCmdArg* arg2)
{
    SVECTOR          angles;
    s32              messageZero;
    s32              messageId;
    GpXformArg*      position;
    Task*            actor;
    s16              nodeFlags;
    SVECTOR*         rotation;
    u16              message;
    GpEnemy*         enemy;
    Actor403600Work* work;
    Actor403600Work* initialWork;
    Actor403600Work* resetWork;
    TmdObject*       hiddenObject;
    TmdObject*       bufferedObject;
    TmdObject*       resetBuffers;
    TmdObject*       shownObject;
    TmdObject*       restartBuffers;
    TmdObject*       restartedObject;
    TmdObject*       stoppedObject;
    TmdObject*       stoppedBuffers;
    Actor403600Work* childWork;
    TmdObject*       childObject;

    message = arg2->command;
    work    = arg0->work;
    enemy   = arg0->spawnArg2;
    switch (message) {
        case 1:
            initialWork               = arg0->work;
            initialWork->field_756    = 8;
            initialWork->field_778    = 0x10;
            initialWork->field_742    = 0;
            initialWork->field_746    = 0;
            initialWork->field_774    = 0;
            initialWork->field_77A    = 0;
            initialWork->field_784    = 0;
            initialWork->field_73C    = 0;
            initialWork->field_73E    = 0;
            initialWork->field_74A    = 0;
            initialWork->field_73A    = 0;
            initialWork->field_776    = 0xA;
            initialWork->field_76E    = 0x40;
            initialWork->field_75E    = 0;
            initialWork->field_7A4    = 0;
            initialWork->field_7A6    = 0;
            initialWork->field_7AC    = 0;
            enemy->node.state.b.flags = 1;
            Gp_ClearNodeSlots(&enemy->node);
            work->field_736            = 1;
            work->field_4B8.coord.t[0] = 0x1D7A;
            work->field_4B8.coord.t[1] = -0x145A;
            work->field_4B8.coord.t[2] = 0x19AE;
            work->field_730            = 0xA;
            work->field_738            = 0;
            angles.vx                  = 0;
            angles.vy                  = 0x200;
            angles.vz                  = 0;
            RotMatrix(&angles, &work->field_4B8.coord);
            messageId = 0x3E9;
            position  = &D_actor_403600_801606E0;
            __asm__("addu %1,$zero,$zero; lw %0,%4"
                    : "=r"(actor), "=&r"(messageZero), "+r"(messageId), "+r"(position)
                    : "m"(Gp_ActorSlots[0]));
            position->rot.vy               = -0x600;
            position->rot.vx               = 0;
            position->rot.vz               = 0;
            D_actor_403600_801606E0.pos.vx = 0x1E8D;
            position->pos.vy               = -0xF9F;
            position->pos.vz               = 0x1AC6;
            Gp_DispatchMsg(actor, messageId, (s32)position, messageZero);
            D_actor_403600_80160568.field_4 = 9;
            Gp_DispatchMsg((Task*)Gp_ActorSlots[0], 0x3F4, (s32)&D_actor_403600_80160568, 0);
            break;
        case 2:
            work->field_736                 = 0x15;
            work->field_738                 = 0;
            D_actor_403600_80160568.field_4 = 0xA;
            Gp_DispatchMsg((Task*)Gp_ActorSlots[0], 0x3F4, (s32)&D_actor_403600_80160568, 0);
            break;
        case 3:
            work->field_77A = 0x3E8;
            work->field_730 = 0xB;
            break;
        case 4:
            D_actor_403600_801606B0 = Task_SpawnFromTable(&D_8016E468, 0, 0, 0);
            Gp_DispatchMsg((Task*)Gp_ActorSlots[0], 0x3F3, 0, 0);
            hiddenObject          = arg0->extra.tmd;
            hiddenObject->flags   = (u16)(hiddenObject->flags | 0x80);
            bufferedObject        = arg0->extra.tmd;
            bufferedObject->flags = (u16)(bufferedObject->flags | 4);
            work->field_4B4       = Gp_SpawnEnemyFromTable(&D_actor_403600_80160514, 2, 0, 0);
            break;
        case 5:
            D_actor_403600_801606E0.rot.vx = 0;
            D_actor_403600_801606E0.rot.vy = 0;
            D_actor_403600_801606E0.rot.vz = 0;
            D_actor_403600_801606E0.pos.vx = 0;
            D_actor_403600_801606E0.pos.vy = 0;
            D_actor_403600_801606E0.pos.vz = 0;
            Gp_DispatchMsg((Task*)Gp_ActorSlots[0], 0x3E9, (s32)&D_actor_403600_801606E0, 0);
            D_actor_403600_80160568.field_4 = 0xB;
            Gp_DispatchMsg((Task*)Gp_ActorSlots[0], 0x3F4, (s32)&D_actor_403600_80160568, 0);
            childWork               = work->field_4B4->task->work;
            childObject             = work->field_4B4->task->extra.tmd;
            childWork->field_730    = 0xD;
            childObject->lightLevel = 0;
            break;
        case 6:
            work->field_73C            = -0x50;
            work->field_730            = 0xC;
            work->field_736            = 1;
            work->field_4B8.coord.t[0] = 0x196E;
            work->field_4B8.coord.t[1] = -0x7D0;
            work->field_4B8.coord.t[2] = 0x1630;
            work->field_738            = 0;
            angles.vx                  = 0;
            angles.vy                  = 0x200;
            angles.vz                  = 0;
            RotMatrix(&angles, &work->field_4B8.coord);
            work->field_4B8.flg = 0;
            Gp_UpdateCoord(&work->field_4B8);
            Tmd_AllocBuffers(arg0->extra.tmd);
            resetBuffers        = arg0->extra.tmd;
            resetBuffers->flags = (u16)(resetBuffers->flags & 0xFFFB);
            shownObject         = arg0->extra.tmd;
            shownObject->flags  = (u16)(shownObject->flags & 0xFF7F);
            work->field_732     = 0;
            break;
        case 7:
            rotation = &angles;
            __asm__ volatile("li %0,8" : "=r"(nodeFlags) : "r"(rotation), "r"(&work->field_4B8.coord));
            resetWork                  = arg0->work;
            resetWork->field_778       = 0x10;
            resetWork->field_776       = 0xA;
            resetWork->field_742       = 0;
            resetWork->field_756       = nodeFlags;
            resetWork->field_746       = 0;
            resetWork->field_774       = 0;
            resetWork->field_77A       = 0;
            resetWork->field_784       = 0;
            resetWork->field_73C       = 0;
            resetWork->field_73E       = 0;
            resetWork->field_74A       = 0;
            resetWork->field_73A       = 0;
            resetWork->field_76E       = 0x40;
            resetWork->field_75E       = 0;
            resetWork->field_7A4       = 0;
            resetWork->field_7A6       = 0;
            resetWork->field_7AC       = 0;
            work->field_730            = 1;
            work->field_732            = 0;
            enemy->node.state.b.flags  = nodeFlags;
            work->field_736            = 1;
            work->field_4B8.coord.t[0] = 0x196E;
            work->field_4B8.coord.t[1] = -0x1B62;
            work->field_4B8.coord.t[2] = 0x1630;
            work->field_738            = 0;
            angles.vx                  = 0;
            angles.vy                  = 0x200;
            angles.vz                  = 0;
            RotMatrix(rotation, &work->field_4B8.coord);
            Tmd_AllocBuffers(arg0->extra.tmd);
            restartBuffers         = arg0->extra.tmd;
            restartBuffers->flags  = (u16)(restartBuffers->flags & 0xFFFB);
            restartedObject        = arg0->extra.tmd;
            restartedObject->flags = (u16)(restartedObject->flags & 0xFF7F);
            break;
        case 8:
            work->field_730           = 0;
            stoppedObject             = arg0->extra.tmd;
            stoppedObject->flags      = (u16)(stoppedObject->flags | 0x80);
            stoppedBuffers            = arg0->extra.tmd;
            stoppedBuffers->flags     = (u16)(stoppedBuffers->flags | 4);
            enemy->node.state.b.flags = 1;
            break;
        case 9:
            Gp_ReleaseStateF0Add(arg0, 0x24);
            gGameSession->flowFlags = (u8)(gGameSession->flowFlags | 0x80);
            Gp_StateF0.field_1      = 5;
            break;
    }
    return 0;
}

void func_actor_403600_80140B4C(GpEnemy* enemy, Task* actor)
{
    SVECTOR              effectOffset;
    Actor403600ViewFrame viewFrame;
    GpMtxWords*          matrixWords;
    GpCmdArg             startMsg;
    GpCmdArg             stopMsg;
    s16                  viewIndex;
    s32                  transparency;
    s32                  upperRadius;
    s32                  lowerRadius;
    s32                  viewPosition;
    s16                  nextViewIndex;
    s16                  nextScale;
    s16                  effectY;
    s32                  upperX;
    s32                  lowerX;
    s32                  upperAngle;
    s32                  lowerAngle;
    s32                  screenDistance;
    s32                  lightLevel;
    s32                  rotationIndex;
    s32                  positionIndex;
    s32                  colorX;
    s32                  colorArg3;
    s32                  colorZero;
    s16*                 rotationSource;
    s32*                 positionDest;
    u16*                 rotationDest;
    s16*                 viewSource;
    u16                  viewRotation;
    u16                  frame;
    u32                  sideState;
    u32                  upperRadiusState;
    u32                  lowerRadiusState;
    u32                  upperAngleState;
    u32                  positiveYState;
    u32                  negativeYState;
    u32                  lowerAngleState;
    u32                  positiveXState;
    u32                  negativeXState;
    VECTOR*              scratchHead;
    VECTOR*              colorPosition;
    VECTOR*              restore;
    TmdObject*           object;
    GpEnemy*             colorActor;
    Actor403600Work*     work;
    Actor403600Work*     colorWork;

    work                           = ((volatile Task*)actor)->work;
    object                         = ((volatile Task*)actor)->extra.tmd;
    viewFrame.view.sub             = &gGfxViewCoord;
    matrixWords                    = (GpMtxWords*)&viewFrame.view.coord;
    matrixWords->m00_m01           = 0x1000;
    viewFrame.matrix.words.m02_m10 = 0;
    matrixWords->m11_m12           = 0x1000;
    viewFrame.matrix.words.m20_m21 = 0;
    matrixWords->m22               = 0x1000;
    viewFrame.view.coord.t[0]      = 0;
    viewFrame.view.coord.t[1]      = 0;
    viewFrame.view.coord.t[2]      = 0;
    D_actor_403600_8016065C.vz     = D_8016A408[work->field_77C].field_0;
    D_actor_403600_8016065C.vy     = D_8016A408[work->field_77C].field_2;
    RotMatrix(&D_actor_403600_8016065C, &work->field_4B8.coord);
    SOFT_TOUCH_REG(object);
    object->otOffset = -0x1F;
    viewIndex        = work->field_77C;
    if (viewIndex >= 0x239) {
        viewSource = (s16*)&D_8016E450;
    } else {
        viewSource = (s16*)&D_8016AEF8[viewIndex * 12];
    }
    rotationIndex  = 0;
    rotationDest   = (u16*)D_actor_403600_80160700;
    rotationSource = viewSource;
    do {
        SOFT_TOUCH_REG(rotationIndex);
        viewRotation    = (u16)*rotationSource;
        rotationSource += 1;
        rotationIndex  += 1;
        *rotationDest   = viewRotation;
        rotationDest   += 1;
    } while (rotationIndex < 9);
    positionIndex = 0;
    positionDest  = D_actor_403600_80160700;
    do {
        SOFT_TOUCH_REG(positionIndex);
        viewPosition    = viewSource[9];
        viewSource     += 1;
        positionIndex  += 1;
        positionDest[5] = viewPosition;
        positionDest   += 1;
    } while (positionIndex < 3);
    screenDistance             = 0x149;
    D_actor_403600_80160700[8] = screenDistance;
    Gp_TrySpawnViewTask((s32)D_actor_403600_80160700);
    func_actor_403600_80141F58(&work->field_4B8, (s16)work->field_744);
    nextViewIndex   = (u16)work->field_77C + 1;
    work->field_77C = nextViewIndex;
    if (nextViewIndex >= 0x2BC) {
        work->field_77C = 0x2BB;
    }
    if (work->field_730 != 0xD) {
        if (work->field_77C == 1) {
            DisplayState* display;

            startMsg.from.loc.stage = 4;
            startMsg.from.loc.area  = 0x16;
            startMsg.command        = 0;
            Gp_DispatchMsg(D_actor_403600_801606B0, 0x7DB, (s32)&startMsg, 0);
            display = &gDisplayState;
            SOFT_TOUCH_REG(display);
            display->screenDistance = screenDistance;
            gte_SetGeomScreen(screenDistance);
            gte_SetGeomOffset(0, 0);
        }
        if (work->field_77C >= 0x100) {
            nextScale       = (u16)work->field_744 + 0x20;
            work->field_744 = nextScale;
            if (nextScale >= 0x1200) {
                work->field_744 = 0x1200;
            }
            work->field_77A = (u16)(work->field_77A - 0x2D);
        }
        if ((u32)((u16)work->field_73A - 0x32) < 0x15FU) {
            upperAngleState  = (Gp_LcgState * 5) + 0x71357911;
            upperAngle       = (upperAngleState >> 0x10) & 0xF80;
            upperRadiusState = (upperAngleState * 5) + 0x71357911;
            Gp_LcgState      = upperRadiusState;
            upperRadius      = ((upperRadiusState >> 0x10) & 0xF00) + 0x200;
            upperX           = (s16)upperRadius * actor_403600_rcos(upperAngle);
            effectOffset.vy  = -0x1800;
            effectOffset.vx  = (s16)(upperX >> 0xC);
            effectOffset.vz  = (s16)((s32)((s16)upperRadius * rsin(upperAngle)) >> 0xC);
            Gp_SpawnEff(0x601C0, &viewFrame.view, 0x300, &effectOffset);
        }
        if ((s16)work->field_73A == 0x15E) {
            stopMsg.from.loc.stage = 4;
            stopMsg.from.loc.area  = 0x16;
            stopMsg.command        = 1;
            Gp_DispatchMsg(D_actor_403600_801606B0, 0x7DB, (s32)&stopMsg, 0);
        }
    } else {
        if ((s16)work->field_73A >= 0x258) {
            lightLevel         = object->lightLevel + 3;
            object->lightLevel = lightLevel;
            if (lightLevel >= 0x259) {
                object->lightLevel = 0x258;
            }
            if (((s16)work->field_73A < 0x2EF) && (work->field_73A & 2)) {
                sideState   = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState = sideState;
                if ((sideState >> 0x10) & 1) {
                    positiveXState                   = (sideState * 5) + 0x71357911;
                    *(volatile s16*)&effectOffset.vx = (positiveXState >> 0x10) & 0x7FF;
                    TOUCH_REG_MEM(positiveXState);
                    positiveYState               = (positiveXState * 5) + 0x71357911;
                    *(volatile u32*)&Gp_LcgState = positiveXState;
                    Gp_LcgState                  = positiveYState;
                    SOFT_TOUCH_REG(positiveYState);
                    effectY = (positiveYState >> 0x10) & 0x7FF;
                } else {
                    negativeXState                   = (sideState * 5) + 0x71357911;
                    *(volatile s16*)&effectOffset.vx = -((negativeXState >> 0x10) & 0x7FF);
                    TOUCH_REG_MEM(negativeXState);
                    negativeYState               = (negativeXState * 5) + 0x71357911;
                    *(volatile u32*)&Gp_LcgState = negativeXState;
                    Gp_LcgState                  = negativeYState;
                    SOFT_TOUCH_REG(negativeYState);
                    effectY = -((negativeYState >> 0x10) & 0x7FF);
                }
                effectOffset.vy = effectY;
                Gp_SpawnEff(0x601BF, &work->field_4B8, 0x10800, &effectOffset);
            }
        }
        if ((s16)work->field_73A == 0x2A8) {
            Gp_DispatchMsg((Task*)Gp_ActorSlots[0], 0x3F3, 1, 0);
        }
        frame = work->field_73A;
        if ((u32)(frame - 0x2A8) < 0xDDU) {
            if ((frame & 3) == 3) {
                Gp_SpawnEff(0x601BF, &work->field_4B8, 0x10800, NULL);
            }
        }
        D_actor_403600_801606E0.rot.vx = 0;
        D_actor_403600_801606E0.rot.vz = 0;
        D_actor_403600_801606E0.pos.vx = -0x1F4;
        D_actor_403600_801606E0.pos.vy = 0x3E8;
        D_actor_403600_801606E0.pos.vz = -0x1F4;
        D_actor_403600_801606E0.rot.vy = (u16)(D_actor_403600_801606E0.rot.vy + 0x38);
        Gp_DispatchMsg((Task*)Gp_ActorSlots[0], 0x3E9, (s32)&D_actor_403600_801606E0, 0);
        if ((u32)((u16)work->field_73A - 0x2BC) < 0xC9U) {
            lowerAngleState  = (Gp_LcgState * 5) + 0x71357911;
            lowerAngle       = (lowerAngleState >> 0x10) & 0xF80;
            lowerRadiusState = (lowerAngleState * 5) + 0x71357911;
            Gp_LcgState      = lowerRadiusState;
            lowerRadius      = ((lowerRadiusState >> 0x10) & 0xF00) + 0x200;
            lowerX           = (s16)lowerRadius * actor_403600_rcos(lowerAngle);
            effectOffset.vy  = 0x1800;
            effectOffset.vx  = (s16)(lowerX >> 0xC);
            effectOffset.vz  = (s16)((s32)((s16)lowerRadius * rsin(lowerAngle)) >> 0xC);
            Gp_SpawnEff(0x601C0, &viewFrame.view, -0x300, &effectOffset);
        }
    }
    work->field_4B8.flg = 0;
    Gp_UpdateCoord(&work->field_4B8);
    colorActor = enemy;
    SOFT_TOUCH_REG(colorActor);
    colorWork = actor->work;
    actor_403600_load_scratch_head(scratchHead);
    colorX = colorWork->field_4B8.workm.t[0];
    SCHED_BARRIER();
    colorZero = 0;
    SOFT_TOUCH_REG_USE(colorZero, scratchHead);
    scratchHead[-1].vx = colorX;
    SOFT_TOUCH_REG(scratchHead);
    colorPosition     = scratchHead - 1;
    colorPosition->vy = colorWork->field_4B8.workm.t[1];
    actor_403600_cutscene_color_tail(colorWork, colorArg3, colorPosition, colorZero);
    colorPosition->vz = (s32)colorWork;
    Gp_UpdateActorColor(colorActor, colorPosition, colorZero, colorArg3);
    actor_403600_load_scratch_head(restore);
    transparency = work->field_77A;
    SOFT_TOUCH_REG_USE(restore, transparency);
    restore += 1;
    actor_403600_store_scratch_head(restore);
    if (transparency != 0) {
        Gp_SetObjTrans(actor->extra.tmd, transparency, transparency, transparency);
    }
    work->field_73A = (u16)(work->field_73A + 1);
}

/// The actor's task entry: runs the handler for `task->state` from a two-entry
/// table built on the stack, passing the enemy the task was spawned for and
/// the task. State 0 is the spawn (`func_actor_403600_80138EF8`, which
/// advances the state), state 1 the per-frame update.
void func_actor_403600_80141180(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_403600_80138EF8,
        func_actor_403600_8013938C,
    };

    fns[task->state](task->spawnArg2, task);
}

void func_actor_403600_801411D4(Task* arg0, s32 arg1)
{
    Actor403600Work* work;
    s32              i;
    s32              masked1;
    s32              masked2;
    s32              limit1;
    s32              limit2;
    u8*              anim;

    work = arg0->work;
    if (D_actor_403600_8016057C[(s16)work->field_736] != 0) {
        i = 1;
        if ((s16)work->field_736 != work->field_738) {
            masked1         = arg1 & 0xFF;
            work->field_738 = work->field_736;
            work->field_73A = 0;
            if (i < masked1) {
                limit1 = masked1;
                do {
                    func_800B4114(&work->rig.anim, i, work->field_736, 0, work->field_756);
                    i++;
                } while (i < limit1);
            }
        } else {
            TOUCH_REG(i);
            masked2          = arg1 & 0xFF;
            work->field_73A += i;
            if (i < masked2) {
                limit2 = masked2;
                /* A byte cursor 0x28 bytes into the block reaches each slot's `rate` at
                 * +0x1D. The slot index is hidden from the compiler for the update
                 * above, so indexing `rig.slots` would derive the cursor from it with
                 * a multiply instead of this constant start. */
                anim = (u8*)work + 0x28;
                do {
                    anim[0x1D] = (u8)work->field_778;
                    Gp_AnimTickIndex(&work->rig.anim, i);
                    i++;
                    anim += 0x28;
                } while (i < limit2);
            }
        }
    }
}

void func_actor_403600_801412D0(GpEnemy* arg0, Task* arg1)
{
    Actor403600Work* work;
    VECTOR*          head;
    VECTOR*          block;

    work                 = arg1->work;
    head                 = SCRATCH_HEAD(VECTOR);
    head[-1].vx          = work->field_4B8.workm.t[0];
    block                = head - 1;
    block->vy            = work->field_4B8.workm.t[1];
    SCRATCH_HEAD(VECTOR) = block;
    block->vz            = work->field_4B8.workm.t[2];
    Gp_UpdateActorColor(arg0, block, 0, 0);
    SCRATCH_POP_BYTES(0x10);
}

void func_actor_403600_80141338(Task* arg0)
{
    Actor403600Work* work;
    GpCoord*         coord;
    MATRIX*          block;
    MATRIX*          head;
    s16              value;
    s16              decrement;
    s16              increment;
    register Task*   actor asm("v1");
    register MATRIX* matrixArg asm("a1");

    head                 = SCRATCH_HEAD(MATRIX);
    block                = head - 1;
    SCRATCH_HEAD(MATRIX) = block;
    matrixArg            = block;
    actor                = arg0;
    SOFT_USE_REG2(block, block);
    work  = actor->work;
    coord = actor->extra.tmd->coords;
    RotMatrix((SVECTOR*)&work->field_700, matrixArg);

    gte_SetRotMatrix(&coord[2].coord.m[0][0]);
    gte_ldclmv(block);
    gte_rtir();
    gte_stclmv(&coord[2].coord.m[0][0]);

    gte_ldclmv(&block->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[2].coord.m[0][1]);

    gte_ldclmv(&block->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[2].coord.m[0][2]);

    value = work->field_700;
    if (value != 0) {
        if (value >= 0x20) {
            decrement       = (u16)work->field_700 - 0x20;
            work->field_700 = decrement;
            if ((decrement << 0x10) <= 0) {
                work->field_700 = 0;
            }
        }
        if (work->field_700 < 0x21) {
            increment       = (u16)work->field_700 + 0x20;
            work->field_700 = increment;
            if ((increment << 0x10) >= 0) {
                work->field_700 = 0;
            }
        }
    }

    SCRATCH_POP_BYTES(0x20);
}

void func_actor_403600_801414FC(Task* arg0)
{
    Actor403600Work* work;
    s16              value;
    s16              countdown;
    s32              brightness;

    work  = arg0->work;
    value = work->field_766;
    if (value != 0) {
        if (value < work->field_764) {
            brightness = rsin(gDisplayState.animFrame << 9) << 0xD;
        } else {
            brightness = rsin(gDisplayState.animFrame << 9) << 0xC;
        }
        Display_ClampField126((s8)(brightness >> 0x18));
        countdown       = (u16)work->field_764 - 1;
        work->field_764 = countdown;
        if ((countdown << 0x10) <= 0) {
            work->field_766 = 0;
            Display_ClampField126(0);
        }
    }
}

void func_actor_403600_80141598(Task* task)
{
    Actor403600Work* work;
    GpEnemy*         enemy;

    enemy                        = task->spawnArg2;
    work                         = (Actor403600Work*)task->work;
    task->extra.tmd->coords->sub = &gGfxViewCoord;
    enemy->recs                  = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_508);
    Gp_UnlinkObj(&work->field_588);
    if (task == D_actor_403600_801606A8) {
        Gp_UnlinkObj(&work->field_5C0);
    }
    Gp_EnemyTaskExit(task);
}

void func_actor_403600_8014161C(Task* arg0)
{
    Actor403600Work* work;
    GpEnemy*         enemy;
    u16*             ticks;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_78C == 1) {
        if (((work->field_79E & 3) == 3) &&
            ((work->field_73E != 0x28) || (work->field_732 != 5))) {
            Gp_SpawnEff(0x60055, arg0->extra.tmd->coords + 1, 0x12800, NULL);
        }
        /* Stored through a plain halfword pointer: as a structure store it
         * makes the compiler read `gDisplayState.animFrame` again after it. */
        ticks                       = &work->field_79E;
        *ticks                      = (u16)(work->field_79E + 1);
        work->field_4B8.coord.t[1] += rsin(gDisplayState.animFrame << 8) >> 6;
        if (((s16)work->field_78A / 10 < enemy->hp) &&
            (work->field_79E >= 0x385) && (work->field_73E == 0)) {
            work->field_730 = 6;
        }
    }
}

void func_actor_403600_8014174C(Task* arg0)
{
    Actor403600Work* work;

    work            = arg0->work;
    work->field_756 = 8;
    work->field_778 = 0x10;
    work->field_776 = 0xA;
    work->field_742 = 0;
    work->field_746 = 0;
    work->field_774 = 0;
    work->field_77A = 0;
    work->field_784 = 0;
    work->field_73C = 0;
    work->field_73E = 0;
    work->field_74A = 0;
    work->field_73A = 0;
    work->field_76E = 0x40;
    work->field_75E = 0;
    work->field_7A4 = 0;
    work->field_7A6 = 0;
    work->field_7AC = 0;
}

void func_actor_403600_801417A8(Task* arg0, s32 arg1)
{
    SVECTOR          rotation;
    MATRIX*          matrix;
    Actor403600Work* work;
    s16              angle;
    s32              absAngle;
    u16              wrapped;

    work  = arg0->work;
    angle = work->field_75E + (arg1 & 0xFF);
    SOFT_TOUCH_REG(angle);
    SOFT_TOUCH_REG(angle);
    absAngle = angle;
    if (angle < 0) {
        SOFT_TOUCH_REG(absAngle);
        absAngle = -absAngle;
    }
    work->field_75E = angle;
    if (absAngle >= 0x801) {
        wrapped = angle - 0x1000;
        if (angle <= 0) {
            wrapped = 0x1000 - angle;
        }
        work->field_75E = wrapped;
    }
    matrix = &work->field_4B8.coord;
    Gfx_MatrixToEuler(matrix, &rotation);
    rotation.vz += work->field_75E;
    RotMatrix(&rotation, matrix);
}

s32 func_actor_403600_80141840(Task* arg0)
{
    s32              targetY;
    s32              currentY;
    s32              count;
    s32              deltaX;
    s32              deltaY;
    s32              deltaZ;
    register s32     left asm("v0");
    register s32     right asm("v1");
    s16              amount;
    Actor403600Work* work;
    s16*             rise;

    count = 0;
    left  = 1;
    SOFT_BARRIER();
    work            = arg0->work;
    work->field_746 = left;
    func_actor_403600_8013DDF4(arg0, 0xA0);

    left   = work->field_6F0.vx;
    right  = work->field_4B8.coord.t[0];
    amount = 0x12C;
    deltaX = left - right;
    SOFT_BARRIER();
    if (deltaX < 0) {
        deltaX = -deltaX;
    }
    work->field_73C = amount;
    if (deltaX < 0x1F5) {
        count                      = 1;
        work->field_4B8.coord.t[0] = work->field_6B0.vx;
    }

    targetY  = work->field_6F0.vy;
    currentY = work->field_4B8.coord.t[1];
    deltaY   = targetY - currentY;
    if (deltaY < 0) {
        deltaY = -deltaY;
    }
    if (deltaY < 0x1F5) {
        count                     += 1;
        work->field_4B8.coord.t[1] = work->field_6B0.vy;
    } else if (targetY < currentY) {
        /* Stored through a plain halfword pointer: as a structure store it
         * lets the compiler read `gDisplayState.animFrame` ahead of it. */
        rise                        = &work->field_74A;
        *rise                       = -0x12C;
        work->field_4B8.coord.t[1] += rsin(gDisplayState.animFrame << 8) >> 6;
    } else {
        work->field_74A = amount;
    }

    left   = work->field_6F0.vz;
    right  = work->field_4B8.coord.t[2];
    deltaZ = left - right;
    if (deltaZ < 0) {
        deltaZ = -deltaZ;
    }
    if (deltaZ < 0x1F5) {
        work->field_4B8.coord.t[2] = work->field_6B0.vz;
        count                     += 1;
    }
    return count & 0xFF;
}

void func_actor_403600_80141954(s32 arg0)
{
    RECT rect;

    rect.y = 0x80;
    rect.h = 0x80;
    rect.w = 0x80;
    if (arg0 == 1) {
        rect.x = 0x180;
    } else {
        rect.x = 0x1C0;
    }
    MoveImage(&rect, 0x180, 0x180);
    rect.w = 0x100;
    rect.h = 1;
    rect.x = 0;
    if (arg0 == 1) {
        rect.y = 0xFD;
    } else {
        rect.y = 0xFE;
    }
    MoveImage(&rect, 0, 0xF9);
}

void func_actor_403600_801419E8(Task* arg0)
{
    TmdObject* obj;

    obj               = arg0->extra.tmd;
    *(s8*)&obj->tpage = -0xF;
    obj->clut         = 2;
    if (obj->buffer != NULL) {
        tmdProcessStream(obj);
        tmdProcessStream(obj);
    }
}

void func_actor_403600_80141A34(Task* arg0)
{
    RECT             rect;
    Actor403600Work* work;
    s16              value;

    work  = arg0->work;
    value = work->field_784;
    if (work->field_7A8 != value) {
        if (value == 1) {
            s16 width;

            rect.x = 0x162;
            rect.y = 0x162;
            width  = 0x15;
            rect.w = width;
            rect.h = 0xA;
            MoveImage(&rect, 0x141, 0x152);
            rect.x = 0x16F;
            rect.y = 0x102;
            rect.w = 0x17;
            rect.h = width;
            MoveImage(&rect, 0x141, 0x164);
        } else {
            s16 width;

            rect.x = 0x141;
            rect.y = 0x1F3;
            width  = 0x15;
            rect.w = width;
            rect.h = 0xA;
            MoveImage(&rect, 0x141, 0x152);
            rect.x = 0x17F;
            rect.y = 0x1AB;
            rect.w = 0x17;
            rect.h = width;
            MoveImage(&rect, 0x141, 0x164);
        }
        work->field_7A8 = (u16)work->field_784;
    }
}

void func_actor_403600_80141B24(Task* arg0)
{
    Actor403600Work* work = arg0->work;

    Gp_HaltPadScripts();
    SndEvt_EnqueueType7(0x54160001, 1);
    work->field_708 = 0;
}

void func_actor_403600_80141B60(Task* arg0)
{
    s16              nextCountdown;
    u16              countdown;
    u16              currentMp;
    Actor403600Work* work;
    PlayerStatus*    config;

    work            = arg0->work;
    countdown       = (u16)work->field_792 - 1;
    work->field_792 = countdown;
    if ((countdown << 0x10) <= 0) {
        config     = &Player_Status;
        currentMp  = config->mp + 1;
        config->mp = currentMp;
        if ((s16)currentMp >= config->mpMax) {
            config->mp = config->mpMax;
        }
        if (work->field_794 <= 0) {
            work->field_792 = 1;
            return;
        }
        nextCountdown   = (u16)work->field_794 - 1;
        work->field_794 = nextCountdown;
        work->field_792 = nextCountdown;
    }
}

void func_actor_403600_80141BE0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403600_801320A0;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_403600_80141C3C(Task* arg0)
{
    s16 value;

    value = ((Actor403600Work*)arg0->work)->field_730;
    if (value < 0) {
        return;
    }
    if (value < 2) {
        func_actor_403600_801400BC(arg0);
    }
}

void func_actor_403600_80141C7C(Task* arg0, s32 arg1)
{
    GpEnemy*         enemy;
    Actor403600Work* work;

    enemy     = arg0->spawnArg2;
    work      = arg0->work;
    enemy->hp = (u16)enemy->hp - arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->hp <= 0) {
        work->field_742 = 1;
    }
}

/// Handlers for states 0-2 of the task `func_actor_403600_80141CD4` dispatches,
/// indexed by `Task::state`. The state-0 handler sets the task up and
/// advances it.
const GpEnemyTaskFuncTable3 D_actor_403600_801320EC = { {
    func_actor_403600_80141D30,
    func_actor_403600_80141E78,
    func_actor_403600_80140B4C,
} };

void func_actor_403600_80141CD4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403600_801320EC;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_403600_80141D30(GpEnemy* arg0, Task* arg1)
{
    GpCoord*         workCoord;
    GpCoord*         coord;
    Actor403600Work* work;
    MATRIX*          matrix;
    MATRIX*          matrix2;

    coord = arg1->extra.tmd->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }

    arg1->work                 = (TaskIdMap*)work;
    work->field_4B8.sub        = &gGfxViewCoord;
    matrix                     = &work->field_4B8.coord;
    MATRIX_PAIR(matrix, 0, 0)  = 0x1000;
    MATRIX_PAIR(matrix, 0, 2)  = 0;
    MATRIX_PAIR(matrix, 1, 1)  = 0x1000;
    MATRIX_PAIR(matrix, 2, 0)  = 0;
    matrix->m[2][2]            = 0x1000;
    work->field_4B8.coord.t[0] = coord->coord.t[0];
    work->field_4B8.coord.t[1] = coord->coord.t[1];
    workCoord                  = &work->field_4B8;
    work->field_4B8.coord.t[2] = coord->coord.t[2];
    matrix2                    = &coord->coord;
    coord->sub                 = workCoord;
    MATRIX_PAIR(matrix2, 0, 0) = 0x1000;
    MATRIX_PAIR(matrix2, 0, 2) = 0;
    MATRIX_PAIR(matrix2, 1, 1) = 0x1000;
    MATRIX_PAIR(matrix2, 2, 0) = 0;
    matrix2->m[2][2]           = 0x1000;
    coord->coord.t[1]          = 0x690;
    coord->coord.t[0]          = 0;
    coord->coord.t[2]          = 0x5DC;
    work->field_4B8.flg        = 0;
    Gp_UpdateCoord(workCoord);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    work->field_4B8.coord.t[0] = 0;
    work->field_4B8.coord.t[1] = 0;
    work->field_4B8.coord.t[2] = 0;
    work->field_730            = 0;
    arg1->msgTable             = D_actor_403600_80160504;
    arg1->exitCallback         = func_actor_403600_80141F28;
    work->field_77A            = 0x2328;
    work->field_744            = 0;
    arg1->state               += 1;
}

void func_actor_403600_80141E78(GpEnemy* arg0, Task* arg1)
{
    TmdObject*       obj;
    TmdObject*       obj2;
    Actor403600Work* work;
    u16              value;

    work            = arg1->work;
    value           = work->field_744 + 1;
    work->field_744 = value;
    if ((s16)value >= 2) {
        Tmd_AllocBuffers(arg1->extra.tmd);
        obj          = arg1->extra.tmd;
        obj->flags  &= 0xFFFB;
        obj2         = arg1->extra.tmd;
        obj2->flags &= 0xFF7F;
        arg1->state++;
        work->field_73A = 0;
        work->field_77C = 0;
        work->field_744 = 0x1000;
    }
}

void func_actor_403600_80141F28(Task* arg0)
{
    arg0->extra.tmd->coords->sub = &gGfxViewCoord;
    Gp_EnemyTaskExit(arg0);
}

void func_actor_403600_80141F58(GpCoord* arg0, s32 arg1)
{
    void**   scratch;
    SVECTOR* head;
    SVECTOR* vec;
    MATRIX*  matrix;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    vec                            = head - 1;
    SCRATCH_HEAD_AT(scratch, void) = vec;
    matrix                         = &arg0->coord;

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 0, 6, 12);
    gte_lddp(arg1);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 0, 6, 12);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 2, 8, 14);
    gte_lddp(arg1);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 2, 8, 14);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 4, 10, 16);
    gte_lddp(arg1);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 4, 10, 16);

    head                           = SCRATCH_HEAD_AT(scratch, void);
    arg0->flg                      = 0;
    SCRATCH_HEAD_AT(scratch, void) = head + 1;
}
