#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "gameplay/3E9C.h"
#include "gameplay/areaplace.h"
#include "gameplay/pairsrc.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include <psyq/abs.h>

typedef struct Actor01600Range {
    /* 0x0 */ s32 low;
    /* 0x4 */ s32 high;
} Actor01600Range;
STATIC_ASSERT_SIZEOF(Actor01600Range, 0x8);

/* Declared here with a signed id: see the note in gameplay/1BC.h. */
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

/// Packed contact record fields over the existing 0x2EC collision block.
typedef union Actor01600ContactId {
    s32 id;
    struct {
        u8  byte0;
        u8  byte1;
        u16 kind;
    } parts;
} Actor01600ContactId;

typedef union Actor01600Contacts {
    byte field_2EC[0xE0];
    struct {
        /* 0x2EC */ byte                pad_2EC[8];
        /* 0x2F4 */ GpCoord*            field_2F4;
        /* 0x2F8 */ s8*                 field_2F8;
        /* 0x2FC */ s16                 field_2FC;
        /* 0x2FE */ s16                 field_2FE;
        /* 0x300 */ s16                 field_300;
        /* 0x302 */ byte                pad_302[2];
        /* 0x304 */ s32                 field_304;
        /* 0x308 */ s16                 field_308;
        /* 0x30A */ u16                 field_30A;
        /* 0x30C */ byte                pad_30C[2];
        /* 0x30E */ s16                 field_30E;
        /* 0x310 */ Actor01600ContactId hit;
        /* 0x314 */ s16                 field_314;
        /* 0x316 */ s16                 field_316;
        /* 0x318 */ s16                 field_318;
        /* 0x31A */ byte                pad_31A[0xB2];
    } named;
} Actor01600Contacts;
STATIC_ASSERT_SIZEOF(Actor01600Contacts, 0xE0);

typedef union Actor01600HitVector {
    VECTOR v;
    struct {
        s16 lowx, highx, lowy, highy, lowz, highz;
        s32 pad;
    } half;
} Actor01600HitVector;

/// Collision displacement and normalized push vectors in the scratch arena.
typedef struct Actor01600HitScratch {
    /* 0x00 */ byte                pad[0x20];
    /* 0x20 */ Actor01600HitVector delta;
    /* 0x30 */ VECTOR              normal;
    /* 0x40 */ byte                tail[0xC];
} Actor01600HitScratch;
STATIC_ASSERT_SIZEOF(Actor01600HitScratch, 0x4C);

typedef struct Actor01600Work {
    /* 0x000 */ GpAnimCtx          anim;
    /* 0x014 */ GpAnimSlot         slots[9];
    /* 0x17C */ byte               pad_17C[0x90];
    /* 0x20C */ MATRIX             field_20C;
    /* 0x22C */ MATRIX             field_22C;
    /* 0x24C */ GpCoord            field_24C;
    /* 0x29C */ byte               field_29C[8];
    /* 0x2A4 */ GpCoord*           field_2A4;
    /* 0x2A8 */ s8*                field_2A8;
    /* 0x2AC */ s16                field_2AC;
    /* 0x2AE */ s16                field_2AE;
    /* 0x2B0 */ s16                field_2B0;
    /* 0x2B2 */ s16                field_2B2;
    /* 0x2B4 */ s32                field_2B4;
    /* 0x2B8 */ s16                field_2B8;
    /* 0x2BA */ u16                field_2BA;
    /* 0x2BC */ byte               pad_2BC[4];
    /* 0x2C0 */ s16                field_2C0;
    /* 0x2C2 */ byte               pad_2C2[0xA];
    /* 0x2CC */ s16                field_2CC;
    /* 0x2CE */ s16                field_2CE;
    /* 0x2D0 */ GpRec18*           field_2D0;
    /* 0x2D4 */ GpRec18            field_2D4;
    /* 0x2EC */ Actor01600Contacts collision;
    /* 0x3CC */ byte               field_3CC[8];
    /* 0x3D4 */ GpCoord*           field_3D4;
    /* 0x3D8 */ s8*                field_3D8;
    /* 0x3DC */ s16                field_3DC;
    /* 0x3DE */ s16                field_3DE;
    /* 0x3E0 */ s16                field_3E0;
    /* 0x3E2 */ s16                field_3E2;
    /* 0x3E4 */ s32                field_3E4;
    /* 0x3E8 */ s16                field_3E8;
    /* 0x3EA */ u16                field_3EA;
    /* 0x3EC */ byte               pad_3EC[0x18];
    /* 0x404 */ GpCoord*           field_404;
    /* 0x408 */ s16                field_408;
    /* 0x40A */ s16                field_40A;
    /* 0x40C */ byte               field_40C[8];
    /* 0x414 */ GpCoord*           field_414;
    /* 0x418 */ s8*                field_418;
    /* 0x41C */ s16                field_41C;
    /* 0x41E */ s16                field_41E;
    /* 0x420 */ s16                field_420;
    /* 0x422 */ s16                field_422;
    /* 0x424 */ s32                field_424;
    /* 0x428 */ s16                field_428;
    /* 0x42A */ u16                field_42A;
    /* 0x42C */ s16                field_42C;
    /* 0x42E */ byte               pad_42E[2];
    /* 0x430 */ s16                field_430;
    /* 0x432 */ byte               pad_432[0xA];
    /* 0x43C */ s16                field_43C;
    /* 0x43E */ s16                field_43E;
    /* 0x440 */ s8*                field_440;
    /* 0x444 */ GpRec18            field_444;
    /* 0x45C */ Actor01600Range    ranges[8];
    /* 0x49C */ MATRIX             field_49C;
    /* 0x4BC */ s32                field_4BC;
    /* 0x4C0 */ s32                field_4C0;
    /* 0x4C4 */ s32                field_4C4;
    /* 0x4C8 */ byte               pad_4C8[4];
    /* 0x4CC */ s16                field_4CC;
    /* 0x4CE */ byte               pad_4CE[6];
    /* 0x4D4 */ Task*              field_4D4;
    /* 0x4D8 */ s16                field_4D8;
    /* 0x4DA */ s16                field_4DA;
    /* 0x4DC */ s16                field_4DC;
    /* 0x4DE */ byte               pad_4DE[2];
    /* 0x4E0 */ s32                field_4E0;
    /* 0x4E4 */ s32                field_4E4;
    /* 0x4E8 */ s16                field_4E8;
    /* 0x4EA */ s16                field_4EA;
    /* 0x4EC */ s16                field_4EC;
    /* 0x4EE */ s16                field_4EE;
    /* 0x4F0 */ s16                field_4F0;
    /* 0x4F2 */ s16                field_4F2;
    /* 0x4F4 */ u16                field_4F4;
    /* 0x4F6 */ s16                field_4F6;
    /* 0x4F8 */ s16                field_4F8;
    /* 0x4FA */ s16                field_4FA;
    /* 0x4FC */ s16                field_4FC;
    /* 0x4FE */ s16                field_4FE;
    /* 0x500 */ s16                field_500;
    /* 0x502 */ s16                field_502;
    /* 0x504 */ s16                field_504;
    /* 0x506 */ s16                field_506;
    /* 0x508 */ s16                field_508;
    /* 0x50A */ s16                field_50A;
    /* 0x50C */ s16                field_50C;
    /* 0x50E */ s16                field_50E;
    /* 0x510 */ s16                field_510;
    /* 0x512 */ s16                field_512;
    /* 0x514 */ s16                field_514;
    /* 0x516 */ s16                field_516;
    /* 0x518 */ s16                field_518;
    /* 0x51A */ s16                field_51A;
    /* 0x51C */ s16                field_51C;
    /* 0x51E */ s16                field_51E;
    /* 0x520 */ s16                field_520;
    /* 0x522 */ s16                field_522;
    /* 0x524 */ s16                field_524;
    /* 0x526 */ s16                field_526;
    /* 0x528 */ s16                field_528;
    /* 0x52A */ s16                field_52A;
    /* 0x52C */ s16                field_52C;
    /* 0x52E */ s16                field_52E;
    /* 0x530 */ s16                field_530;
    /* 0x532 */ s16                field_532;
    /* 0x534 */ s16                field_534;
    /* 0x536 */ s16                field_536;
    /* 0x538 */ s16                field_538;
    /* 0x53A */ s16                field_53A;
    /* 0x53C */ s16                field_53C;
    /* 0x53E */ s16                field_53E;
    /* 0x540 */ s16                field_540;
    /* 0x542 */ u16                field_542;
    /* 0x544 */ s16                field_544;
    /// Second animation id the `variant == 2` and `variant == 4` paths of
    /// `Actor01600_Fn05F80` run their countdown against: it is stored into
    /// `field_506` and steps 7 -> 9.
    /* 0x546 */ s16 field_546;
    /* 0x548 */ u16 field_548;
    /// Copy of the spawn variant `Actor01600_Fn05F80` takes its `case 0x1A`
    /// path for.
    /* 0x54A */ s16  field_54A;
    /* 0x54C */ u16  field_54C;
    /* 0x54E */ s16  field_54E;
    /* 0x550 */ s16  field_550;
    /* 0x552 */ byte pad_552[2];
    /* 0x554 */ s16  field_554;
    /* 0x556 */ s16  field_556;
} Actor01600Work;
STATIC_ASSERT_SIZEOF(Actor01600Work, 0x558);

typedef struct Actor01600RotScratch {
    /* 0x00 */ VECTOR  position;
    /* 0x10 */ SVECTOR rotation;
} Actor01600RotScratch;
STATIC_ASSERT_SIZEOF(Actor01600RotScratch, 0x18);

/// 0x7C-byte scratch from `G_SCRATCH_HEAD` that `Actor01600_Fn045A8` aims from:
/// `delta` takes the world-space offset from the actor to the player, `dir` the
/// same offset written as an `SVECTOR` and then replaced by that offset turned
/// into the actor's own frame, and `mat` the transpose of the actor's rotation
/// the turn multiplies by. Only `delta` is reached through the allocated
/// pointer; the two above it are written at negative offsets from the scratchpad
/// head, which is what keeps the head in a register of its own.
typedef struct Actor01600AimScratch {
    /* 0x00 */ byte    pad_0[0x20];
    /* 0x20 */ VECTOR  delta;
    /* 0x30 */ byte    pad_30[0xC];
    /* 0x3C */ SVECTOR dir;
    /* 0x44 */ byte    pad_44[0x18];
    /* 0x5C */ MATRIX  mat;
} Actor01600AimScratch;
STATIC_ASSERT_SIZEOF(Actor01600AimScratch, 0x7C);

/// 0x30-byte scratch from `G_SCRATCH_HEAD` used by `Actor01600_Fn04C64`: `vec`
/// takes (0, 0, `distance`), `mat` the yaw rotation `func_8004BFF8` builds from
/// the work block's `field_4EC`, and `out` the `vec` turned by it - the
/// displacement the actor keeps in `field_42C` / `field_430`.
typedef struct Actor01600YawScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ SVECTOR out;
    /* 0x10 */ MATRIX  mat;
} Actor01600YawScratch;
STATIC_ASSERT_SIZEOF(Actor01600YawScratch, 0x30);

/// Index of the `Gp_ActorSlots` actor nearer to `arg0`, or 0 when slot 0 is
/// empty (or slot 1 is at least as far). The distance is planar: the Y
/// difference is computed into the scratch vector but never enters the sum.
s32 Actor01600_Fn052C4(Task* arg0);

extern Task*      Gp_ActorSlots[];
extern GpDelayArg Actor01600_D12878;
extern GpXformArg Actor01600_D12890;

extern s32 Actor01600_D12874;
extern s32 Actor01600_D127DC;
extern s32 Actor01600_D12870;

/// Models effect 0x80005 spawns, set in `D_800626EC[5].arg.model`.
extern TmdSource Actor01600_D0973C;
extern TmdSource Actor01600_D09CFC;
extern TmdSource Actor01600_D09EE0;
extern SVECTOR   Actor01600_D12868;

void Actor01600_Fn070AC(Task* arg0, Task* arg1);

void Actor01600_Fn01420(Task* arg0);
void Actor01600_Fn04054(GpEnemy* arg0, Task* arg1);
void Actor01600_Fn066E8(Task* arg0);

s32 Actor01600_Fn045A8(Task* arg0, s32* distance);
s32 Actor01600_Fn04C64(Task* arg0, s32 distance, s32 angle);

u8 Actor01600_Fn04EB0(Task* arg0);

/// Scratchpad storage for the actor's ground-quad position and rotated offset.
typedef struct Actor01600GroundScratch {
    /* 0x00 */ VECTOR3 pos;
    /* 0x0C */ s32     pad_C;
    /* 0x10 */ SVECTOR offset;
} Actor01600GroundScratch;
STATIC_ASSERT_SIZEOF(Actor01600GroundScratch, 0x18);

/// 0x3C-byte `G_SCRATCH_HEAD` block `Actor01600_Fn06974` steps the attachment
/// coordinate in: the step vector the coordinate's facing is rotated into, the
/// `SVECTOR` `Gfx_MatrixCol2` reads that facing into, the rotation
/// `func_8004BFF8` builds for the yaw and the yaw itself.
typedef struct Actor01600StepScratch {
    /* 0x00 */ VECTOR     move;
    /* 0x10 */ SVECTOR    dir;
    /* 0x18 */ OverlayMat mat;
    /* 0x38 */ s16        yaw;
    /* 0x3A */ byte       pad_3A[2];
} Actor01600StepScratch;
STATIC_ASSERT_SIZEOF(Actor01600StepScratch, 0x3C);

void Actor01600_Fn03A60(Task* actor);

extern GpAnimArg Actor01600_D127D8;

s32  Actor01600_Fn047A0(Task* actor);
s32  Actor01600_Fn04974(Task* actor, s32 angle, s32 distance, s32 flags);
void Actor01600_Fn06974(Task* actor, s32 distance);
s32  Actor01600_Fn06C1C(Task* actor);
s32  Actor01600_Fn06C94(Task* actor, s32 angle, s32 distance);
s32  Actor01600_Fn06D74(Task* actor, s32 angle, s32 distance);

extern GpPairSrcE Actor01600_D09F0C;
extern u8         Actor01600_D127EC[], Actor01600_D127A4[];
void              Actor01600_Fn05400(Task* actor);
void              Actor01600_Fn06EA4(Task* actor);

void                    func_8004BFF8(s16 angle, MATRIX* matrix);
extern struct GpU16Pair Actor01600_D09F04;

extern SVECTOR Actor01600_D09F1C[];
extern SVECTOR Actor01600_D09F3C[];

/// Calls `Gp_ClearRec18Occupied` as a function with a result, although the
/// gameplay definition returns nothing: this actor was compiled against a
/// declaration with one. Called as `void`, the code after the call that ends
/// `Actor01600_Fn04C64` takes different registers and the overlay no longer
/// matches.
#define CLEAR_REC18_OCCUPIED(rec) (((s32 (*)(GpRec18*))Gp_ClearRec18Occupied)(rec))
MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

void Actor01600_Fn001F4(GpEnemy* ctx, Task* actor);
void Actor01600_Fn00480(Task* arg0);
void Actor01600_Fn00674(GpEnemy* arg0, Task* arg1);
void Actor01600_Fn00A4C(Task* arg0);
void Actor01600_Fn00BAC(Task* arg0);
void Actor01600_Fn0131C(Task* arg0, s32 damage);
void Actor01600_Fn017BC(Task* arg0);
void Actor01600_Fn020F8(Task* arg0);
void Actor01600_Fn03D48(Task* arg0);
void Actor01600_Fn03EEC(Task* arg0);
void Actor01600_Fn04AD8(Task* arg0);
s32  Actor01600_Fn05558(Task* arg0);
void Actor01600_Fn05F80(Task* arg0);
void Actor01600_Fn0646C(Task* arg0);
void Actor01600_Fn06744(Task* arg0);
void Actor01600_Fn06810(GpEnemy* arg0, Task* arg1);
void Actor01600_Fn06880(Task* arg0);
void Actor01600_Fn06A84(Task* arg0);
void Actor01600_Fn06F10(Task* arg0);
u8   Actor01600_Fn06F78(Task* arg0);
void Actor01600_Fn06FDC(Task* arg0, s32 arg1);

/// Takes a 0x10-byte `VECTOR` from `G_SCRATCH_HEAD`, fills it with `attach`'s
/// world position and hands it to `Gp_UpdateActorColor`. Inlined so the
/// scratch-head address is rematerialised on every access.
static __inline__ void update_actor_color(GpEnemy* ctx, GpCoord* attach)
{
    u8*     head;
    VECTOR* block;

    head  = SCRATCH_HEAD(u8);
    block = (VECTOR*)(head - 0x10);

    SCRATCH_HEAD(VECTOR) = block;

    block->vx = attach->workm.t[0];
    block->vy = attach->workm.t[1];
    block->vz = attach->workm.t[2];
    Gp_UpdateActorColor(ctx, block, 0, 0);

    SCRATCH_HEAD(u8) = (u8*)SCRATCH_HEAD(void) + 0x10;
}

const GpEnemyTaskFuncTable3 Actor01600_D00004 = {
    { Actor01600_Fn001F4, Actor01600_Fn00674, Actor01600_Fn04054 },
};

void Actor01600_Fn001F4(GpEnemy* ctx, Task* actor)
{
    SVECTOR         sp18;
    GpCoord*        next_coord;
    s32             i;
    u32             random;
    GpCoord*        coord;
    TmdObject*      obj;
    Actor01600Work* work;
    MATRIX*         matrix;

    obj        = actor->extra.tmd;
    coord      = obj->coords;
    work       = memCalloc(0x558U, false);
    next_coord = coord + 1;
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work                           = work;
    obj->flags                            = 0;
    coord->flg                            = 0;
    obj->lightMtx                         = &work->field_22C;
    obj->colorMtx                         = &work->field_20C;
    work->field_24C.sub                   = &gGfxViewCoord;
    matrix                                = &work->field_24C.coord;
    *(s32*)&work->field_24C.coord.m[0][0] = 0x1000;
    *(s32*)&matrix->m[0][2]               = 0;
    *(s32*)&matrix->m[1][1]               = 0x1000;
    *(s32*)&matrix->m[2][0]               = 0;
    matrix->m[2][2]                       = 0x1000;
    work->field_24C.coord.t[0]            = (s32)coord->coord.t[0];
    work->field_24C.coord.t[1]            = (s32)coord->coord.t[1];
    work->field_24C.coord.t[2]            = (s32)coord->coord.t[2];
    work->field_24C.flg                   = 0;
    work->field_4F2                       = 0;
    ctx->field_4                          = &coord->coord;
    ctx->field_48                         = 0;
    Gp_LinkNode(&ctx->node);
    ctx->bodyPos.vy         = -0x190;
    ctx->node.state.b.flags = 0;
    ctx->coord              = coord;
    ctx->bodyPos.vx         = 0;
    ctx->bodyPos.vz         = 0;
    ctx->param              = &Actor01600_D09F0C;
    ctx->recs               = (GpRec18*)&work->collision.named.pad_30C;
    ctx->hp                 = (u16)Actor01600_D09F0C.hpMax;
    work->field_408         = 0x280;
    work->field_40A         = 2;
    work->field_404         = next_coord;
    func_800B3F84(&work->anim, Actor01600_D127EC, obj, work->pad_17C, work->slots);
    for (i = 1; i < 9; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    Gp_IncStateF0Ref(0);
    work->field_506 = 1;
    work->field_508 = 1;
    work->field_542 = 0x14;
    work->field_4EA = 0;
    work->field_52A = 0;
    work->field_52C = 0;
    work->field_4FA = 0;
    work->field_50C = 0;
    work->field_526 = 0;
    work->field_528 = 0;
    work->field_51E = 0;
    work->field_520 = 0;
    work->field_510 = 0;
    work->field_530 = 0;
    work->field_532 = 0;
    work->field_534 = 0;
    work->field_53A = 0;
    work->field_53C = 0;
    work->field_540 = 0;
    work->field_53E = 0;
    work->field_524 = 0;
    work->field_544 = 0;
    work->field_538 = 0x10;
    random          = (Gp_LcgState * 5) + 0x71357911;
    work->field_536 = (s16)(((random >> 0x10) & 0x1F) + 1);
    Gp_LcgState     = (s32)random;
    Gfx_MatrixCol2(&actor->extra.tmd->coords->coord, &sp18);
    work->field_4FC = ratan2((s32)sp18.vx, (s32)sp18.vz);
    Actor01600_Fn05400(actor);
    actor->exitCallback = &Actor01600_Fn06EA4;
    actor->msgTable     = &Actor01600_D127A4;
    actor->state        = (s32)(actor->state + 1);
}

void Actor01600_Fn00480(Task* actor)
{
    Actor01600Work* work;
    GpRec18*        table1;
    GpCoord*        coord;
    s8*             table2;
    s8*             table3;
    s8*             table4;

    work            = actor->work;
    coord           = actor->extra.tmd->coords;
    work->field_2C0 = 0xFA0;
    work->field_2CC = 0x384;
    table1          = &work->field_2D4;
    work->field_2CE = 0x64;
    work->field_2D0 = table1;
    work->field_2A8 = work->pad_2BC;
    work->field_2AC = 0;
    work->field_2AE = -0x190;
    work->field_2B0 = 0;
    work->field_2B4 = 0;
    work->field_2B8 = 0;
    work->field_2BA = 3;
    work->field_2A4 = coord;
    Gp_LinkObj(3, (struct _GpObj*)work->field_29C);
    Gp_InitRec18Table(table1, 1, 0);
    table2                          = &work->collision.field_2EC[0x20];
    work->collision.named.field_2F4 = coord;
    work->collision.named.field_2F8 = table2;
    work->collision.named.field_304 = 0x30010;
    work->collision.named.field_308 = 0x190;
    work->collision.named.field_2FC = 0;
    work->collision.named.field_2FE = -0x190;
    work->collision.named.field_300 = 0;
    work->collision.named.field_30A = 1;
    work->field_2BA                |= 0xC000;
    Gp_LinkObj(2, (struct _GpObj*)work->collision.field_2EC);
    Gp_InitRec18Table((GpRec18*)table2, 8, 0);
    table3                           = (s8*)&work->field_444;
    work->field_430                  = 0x1F4;
    work->field_42C                  = 0x1F4;
    work->field_43C                  = 1;
    work->field_43E                  = 1;
    work->field_440                  = table3;
    work->field_414                  = coord;
    work->field_418                  = (s8*)&work->field_42C;
    work->field_41C                  = 0;
    work->field_41E                  = -0x190;
    work->field_420                  = 0;
    work->field_424                  = 0;
    work->field_428                  = 0;
    work->field_42A                  = 3;
    work->collision.named.field_30A |= 0xC200;
    Gp_LinkObj(2, (struct _GpObj*)work->field_40C);
    Gp_InitRec18Table((GpRec18*)table3, 1, 0);
    table4           = work->pad_3EC;
    work->field_3D4  = coord;
    work->field_3D8  = table4;
    work->field_3DC  = 0;
    work->field_3DE  = -0x186;
    work->field_3E0  = 0;
    work->field_42A &= 0x3FFF;
    work->field_3E4  = Gp_PackPair(&Actor01600_D09F04, 1);
    work->field_3E8  = 0x12C;
    work->field_3EA  = 1;
    Gp_LinkObj(3, (struct _GpObj*)work->field_3CC);
    Gp_InitRec18Table((GpRec18*)table4, 1, 0);
    work->field_3EA &= 0x7FFF;
}

void Actor01600_Fn00674(GpEnemy* arg0, Task* arg1)
{
    Actor01600Work* work;
    GpCoord*        coord;
    TmdObject*      obj;
    s32             id;
    s32             map;
    u16             count;

    work  = arg1->work;
    coord = arg1->extra.tmd->coords;
    if (!(Actor01600_Fn05558(arg1) & 0xFF)) {
        switch (Gp_StateF0.field_4) {
            case 0:
                arg1->extra.tmd->flags   = 0;
                arg0->node.state.b.flags = 0;
                break;
            case 1:
                Actor01600_Fn06810(arg0, arg1);
                goto update;
            case 2:
                obj                      = arg1->extra.tmd;
                obj->flags              |= 0x80;
                arg0->node.state.b.flags = 1;
                return;
            default:
                break;
        }
        Actor01600_Fn04AD8(arg1);
        if (arg0->reactionFlags != 0) {
            Actor01600_Fn00A4C(arg1);
        }
        Actor01600_Fn00BAC(arg1);
        if (work->field_528 == 0) {
            if (work->field_500 != 0) {
                arg1->state = 2;
            }
        }
        map = GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA;
        if (map != 0x3260000 && map != 0x4070000 && map != 0x4010000) {
            if (coord->coord.t[1] >= 0x65) {
                coord->coord.t[1] = -0xA;
            }
        }
        if (((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(3, 29, 0, 0)) && (coord->coord.t[1] >= -0x3E7)) {
            id = (((u16)((GpEnemy*)arg1->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100005;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            id = (((u16)((GpEnemy*)arg1->spawnArg2)->placeKey >> 0xC) << 8) | 0x4010000A;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            Actor01600_Fn06F10(arg1);
            Actor01600_Fn06FDC(arg1, 0);
        }
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(4, 1, 0, 0)) {
            if (coord->coord.t[1] > 0) {
                work->field_532 = 1;
            }
            if (coord->coord.t[1] >= 0x3E9) {
                id = (((u16)((GpEnemy*)arg1->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100005;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                id = (((u16)((GpEnemy*)arg1->spawnArg2)->placeKey >> 0xC) << 8) | 0x4010000A;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                Actor01600_Fn06F10(arg1);
                Actor01600_Fn06FDC(arg1, 0);
            }
        }
        Actor01600_Fn01420(arg1);
        Actor01600_Fn03D48(arg1);
        Actor01600_Fn06A84(arg1);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        count           = work->field_550 + 1;
        work->field_550 = count;
        if (((s16)count >= 5) || (gGameSession->viewDirty == 1)) {
            work->field_550 = 0;
            Actor01600_Fn06810(arg0, arg1);
        }
        if (work->field_532 == 0) {
        update:
            Actor01600_Fn03EEC(arg1);
        }
    }
}

void Actor01600_Fn00A4C(Task* arg0)
{
    GpEnemy*        ctx;
    Actor01600Work* work;
    s16             state;
    s32             damage;
    u8              flags2;
    u8              flags1;

    ctx    = arg0->spawnArg2;
    flags1 = ctx->reactionFlags;
    work   = arg0->work;
    if ((flags1 & 1) && (work->field_528 == 0)) {
        ctx->reactionFlags = flags1 & 0xFE;
        work->field_4FE    = 2;
        work->field_538    = 0x14;
        work->field_504    = 0;
        work->field_506    = 0xE;
    }
    flags2 = ctx->reactionFlags;
    if ((flags2 & 2) && (work->field_4FE != 2) && (work->field_528 == 0)) {
        ctx->reactionFlags = flags2 & 0xFD;
        work->field_4FE    = 3;
        if (work->field_556 != 0) {
            work->field_506 = 0xE;
            work->field_4FE = 5;
        }
        work->field_504 = 0;
    }
    if ((ctx->reactionFlags & 0xC) && (work->field_528 == 0)) {
        Actor01600_Fn06F10(arg0);
        damage = Gp_TickObjFlag4(ctx);
        if (damage != 0) {
            state = work->field_4FE;
            if ((state != 2) && (state != 5)) {
                work->field_4FE = 0;
                work->field_506 = 0xA;
            }
            Actor01600_Fn0131C(arg0, damage);
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->reactionFlags &= 0xF3;
        }
    }
}

void Actor01600_Fn00BAC(Task* actor)
{
    s32                   distance;
    Task**                slots;
    void*                 world;
    Actor01600Work*       work;
    GpEnemy*              ctx;
    GpCoord*              coord;
    Actor01600Work*       rec;
    Actor01600HitScratch* scratch;
    void*                 old;
    GpCoord*              other;
    s32                   x, y, z;
    s32                   damage;
    s32                   amount;
    s32                   product;
    s32                   push;
    s32                   clamped;
    s32                   cx, cz;
    s16                   count;
    s32                   mode;
    VECTOR *              v1, *v2;
    work    = actor->work;
    old     = SCRATCH_HEAD(void);
    scratch = (SCRATCH_HEAD(void) = old - 0x4C);
    ctx     = actor->spawnArg2;
    coord   = actor->extra.tmd->coords;
    mode    = func_800E0C10((GpRec18*)&work->collision.field_2EC[0x20], old - 0x2C, 8, old - 4);
    world   = coord + 1;
    if (mode == 1)
        goto mode1;
    if (mode < 2)
        goto mode_end;
    if (mode == 2)
        goto mode2;
    goto mode_end;
    {
    mode1:
        coord->coord.t[0] += scratch->delta.half.highx;
        coord->coord.t[1] += scratch->delta.half.highy;
        coord->coord.t[2] += scratch->delta.half.highz;
        goto mode_end;
    mode2:
        coord->coord.t[0] = work->field_4BC;
        coord->coord.t[1] = work->field_4C0;
        coord->coord.t[2] = work->field_4C4;
    }
mode_end:
    slots = Gp_ActorSlots;
    if (work->field_51C != 0) {
        if (--work->field_51C <= 0)
            work->field_51C = 0;
    }
    /* The record walk steps a work pointer one contact record at a time and
       reads the record through it, keeping the record's offset in the
       displacement as the ROM does. */
    rec = work;
next_record: {
    switch (rec->collision.named.hit.parts.kind) {
        case 2:
            if (work->field_51C == 0) {
                other               = slots[rec->collision.named.hit.parts.byte0 >> 7]->extra.tmd->coords;
                x                   = other->coord.t[0] - coord->coord.t[0];
                scratch->delta.v.vx = x;
                y                   = other->coord.t[1] - coord->coord.t[1];
                scratch->delta.v.vy = y;
                z                   = other->coord.t[2] - coord->coord.t[2];
                scratch->delta.v.vz = z;
                damage              = Gp_ComputeDamage(rec->collision.named.hit.id, SquareRoot0(x * x + y * y + z * z), 0, 0);
                if (Gp_RollEnemyChance(actor->spawnArg2, rec->collision.named.hit.id, 0)) {
                    damage *= 4;
                    Gp_SpawnEff(0x6009C, actor->extra.tmd->coords + 1, 0, 0);
                }
                if (work->field_4FE == 1 && work->field_528 != 0 && work->field_51E < 0) {
                    damage *= 2;
                    Gp_SpawnEff(0x6009C, actor->extra.tmd->coords + 1, 3, 0);
                }
                func_800E2C78(ctx, rec->collision.named.hit.id, damage, 0);
                Actor01600_Fn0131C(actor, damage);
                count = Gp_GetIdParam2(rec->collision.named.hit.id);
                if (count > 0)
                    work->field_51C = count;
                switch (Gp_GetIdParam0(rec->collision.named.hit.id) & 0xFFFF) {
                    case 4:
                    case 6:
                        if ((s16)ctx->hp <= 0)
                            goto dead;
                        Gp_SetObjFlag1(actor->spawnArg2);
                        break;
                    case 2:
                    case 9:
                        if (work->field_4FE != 3 && work->field_4FE != 5) {
                            Gp_SetObjFlag2(actor->spawnArg2, rec->collision.named.hit.id, 0);
                            work->field_556 = 1;
                        }
                        break;
                    case 8:
                        if (work->field_4FE != 3 && work->field_4FE != 5) {
                            Gp_SetObjFlag2(actor->spawnArg2, rec->collision.named.hit.id, 0);
                            work->field_556 = 0;
                        }
                        break;
                    case 1:
                    case 5:
                        if (work->field_4FE != 3 && work->field_4FE != 5)
                            Gp_SetObjFlag1(actor->spawnArg2);
                        break;
                    case 0:
                        break;
                    case 3:
                        Gp_SetObjFlag4(actor->spawnArg2, rec->collision.named.hit.id, 0);
                        break;
                }
                if (damage >= 40 && work->field_556 == 0) {
                    Gp_SetObjFlag1(ctx);
                    if (work->field_4FE == 0 && work->field_528 != 0) {
                        work->field_528 = 0;
                        work->field_51E = 0;
                        work->field_520 = 0;
                    }
                } else {
                    work->field_522 = 1;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_4CC = (((u32)Gp_LcgState >> 11) & 0x60) + 0x100;
                }
                if (work->field_4FE != 0 && work->field_528 != 0) {
                    work->field_3EA &= 0x7FFF;
                    CLEAR_REC18_OCCUPIED((GpRec18*)work->pad_3EC);
                    work->field_4FA  = 0;
                    work->field_51E += 20;
                    amount           = Actor01600_Fn045A8(actor, &distance);
                    if (amount < 0)
                        amount = -amount;
                    if (amount < 0x400) {
                        work->field_506 = 14;
                        work->field_50E = -40;
                    } else {
                        work->field_506 = 11;
                        work->field_50E = 40;
                    }
                    work->field_516 = 8;
                }
                func_800FDB18(Gp_GetIdParam1(rec->collision.named.hit.id) & 0xFFFF, world, 0, (GpEffArg*)&work->pad_3EC[0x18]);
            }
            break;
        case 3:
            cx                  = coord->workm.t[0] - rec->collision.named.field_314;
            scratch->delta.v.vy = 0;
            scratch->delta.v.vx = cx;
            cz                  = coord->workm.t[2] - rec->collision.named.field_318;
            scratch->delta.v.vz = cz;
            push                = rec->collision.named.field_30E - SquareRoot0(cx * cx + cz * cz);
            clamped             = push;
            if (push <= 0)
                clamped = 0;
            push = clamped;
            SOFT_TOUCH_REG_USE(push, rec);
            SOFT_TOUCH_REG_USE(push, rec);
            SOFT_TOUCH_REG_USE(push, rec);
            v1                  = &scratch->delta.v;
            scratch->delta.v.vx = coord->workm.t[0] - rec->collision.named.field_314;
            SOFT_TOUCH_REG_USE(push, clamped);
            v2                  = &scratch->normal;
            scratch->delta.v.vy = coord->workm.t[1] - rec->collision.named.field_316;
            scratch->delta.v.vz = coord->workm.t[2] - rec->collision.named.field_318;
            VectorNormal(v1, v2);
            ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, v2, v1);
            if (work->field_506 == 23 || work->field_506 == 5 || work->field_506 == 6) {
                coord->coord.t[0] += (push * scratch->delta.v.vx) >> 12;
                product            = push * scratch->delta.v.vy;
                if (product < 0)
                    coord->coord.t[1] += product >> 12;
                coord->coord.t[2] += (push * scratch->delta.v.vz) >> 12;
            }
            break;
        case 0:
        case 1:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
            break;
    }
}
    rec = (void*)rec + 0x18;
    if ((s32)rec < (s32)work + 0xC0)
        goto next_record;
    CLEAR_REC18_OCCUPIED((GpRec18*)&work->collision.field_2EC[0x20]);
    if (work->field_516 && Gp_FindRec18((GpRec18*)work->pad_3EC, 0)) {
        work->collision.named.field_2FE = -400;
        work->collision.named.field_308 = 400;
        work->field_52A                 = 1;
        work->field_3EA                &= 0x7FFF;
        CLEAR_REC18_OCCUPIED((GpRec18*)work->pad_3EC);
        if (work->field_50A < 15) {
            work->field_516  = 8;
            work->field_4FA  = 0;
            work->field_51E += 20;
            amount           = Actor01600_Fn045A8(actor, &distance);
            if (amount < 0)
                amount = -amount;
            if (amount >= 0x400)
                goto far_angle;
            work->field_506 = 14;
            work->field_50E = -40;
            goto release;
        dead:
            work->field_540 = 0;
            Actor01600_Fn0646C(actor);
            work->field_540 = 2;
            work->field_528 = 0;
            return;
        far_angle:
            work->field_506 = 11;
            work->field_50E = 40;
        }
    }
release:
    SCRATCH_POP_BYTES(0x4C);
    return;
}

void Actor01600_Fn0131C(Task* arg0, s32 damage)
{
    s32             id;
    s32             pan;
    GpEnemy*        ctx;
    Actor01600Work* work;
    GpCoord*        coord;

    ctx     = arg0->spawnArg2;
    work    = arg0->work;
    coord   = arg0->extra.tmd->coords;
    ctx->hp = (u16)(ctx->hp - damage);
    func_800DA6E8(&ctx->node, damage, 0);
    work->field_536 = 1;
    if ((s16)ctx->hp <= 0) {
        work->field_500  = 1;
        work->field_502  = 0;
        work->field_3EA &= 0x7FFF;
        id               = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4010000A;
        SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    } else {
        if (work->field_506 == 1) {
            work->field_506 = 0xA;
            work->field_50A = 0;
        }
        work->field_50E = 0;
        work->field_51A = 0;
        id              = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100009;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(coord));
    }
}

/// Per-frame tick for the actor's cornered/pursuit cycle, dispatched on
/// `field_4FE`. States 0 and 1 hand the frame to `Actor01600_Fn017BC` /
/// `Actor01600_Fn020F8` and then run the shared post-step
/// `Actor01600_Fn06744`. State 2 advances `field_504`, nudges `field_50E` back
/// by 0x3C while the animation is still 0xE and under 0x11 frames in, arms the
/// 0x16 animation at frame 0x28 and, past frame 0x5B, resets to animation 0x19
/// with the 0x8000 bit set in `field_2BA`. State 3 plays animation 0x13 until
/// `Gp_TickObjFlag2` fires. State 4 only selects animation 0x11. State 5 rolls
/// the 0x4CC swerve offset once per approach (animation 0xE, past frame 0x2C,
/// `field_522` still clear and bit 1 of `field_50A` set) and, on animation
/// 0x16 past frame 0x32, ends the cycle the same way state 2 does.
///
/// Whatever the state, animations 1/9/0x10/0x13/0x15/0x16/0x1B..0x1E are
/// silent; the rest count `field_542` down and, on expiry, play one of three
/// growls (`0x4010_0006..8`) picked by a `Gp_LcgState` draw modulo 5 - two of
/// the five outcomes stay quiet - panned and attenuated for the actor's
/// coordinate, then rearm the counter at 0x14.
void Actor01600_Fn01420(Task* arg0)
{
    Actor01600Work* work;
    GpCoord*        coord;
    s32             id;
    s32             state;
    s32             one;
    u16             sel;
    s16             count;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;

    switch (work->field_4FE) {
        case 0:
            Actor01600_Fn017BC(arg0);
            goto tick;
        case 1:
            Actor01600_Fn020F8(arg0);
            goto tick;
        case 2:
            Actor01600_Fn06F10(arg0);
            work->field_504 = work->field_504 + 1;
            if (work->field_506 == 0xE && work->field_50A < 0x11) {
                work->field_50E = -0x3C;
                Actor01600_Fn06744(arg0);
            } else {
                work->field_50E = 0;
            }
            if (work->field_504 == 0x28) {
                work->field_538 = 0x10;
                work->field_506 = 0x16;
            }
            if (work->field_504 >= 0x5B) {
                work->field_506  = 0x19;
                work->field_4FE  = 0;
                work->field_504  = 0;
                work->field_2BA |= 0x8000;
            }
            goto clear;
        case 3:
            Actor01600_Fn06F10(arg0);
            work->field_506 = 0x13;
            if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
                work->field_4FE  = 0;
                work->field_506  = 0x19;
                work->field_2BA |= 0x8000;
            }
            goto clear;
        case 4:
            work->field_506 = 0x11;
            break;
        case 5:
            Actor01600_Fn06F10(arg0);
            if (work->field_506 == 0xE) {
                if (work->field_50A >= 0x2C && work->field_522 == 0 &&
                    ((u16)work->field_50A & 2)) {
                    work->field_522 = 1;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_4CC = (((u32)Gp_LcgState >> 11) & 0x60) + 0x20;
                }
            } else if (work->field_506 == 0x16) {
                if (work->field_50A >= 0x32) {
                    work->field_4FE  = 0;
                    work->field_506  = 0x19;
                    work->field_2BA |= 0x8000;
                }
            }
            if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
                work->field_506 = 0x16;
            }
            goto clear;
    }
    goto tail;

clear:
    work->field_50E = 0;
tick:
    Actor01600_Fn06744(arg0);

tail:
    state = work->field_506;
    one   = 1;
    if (state == one || state == 0x16 || state == 0x15 || state == 0x10 ||
        state == 0x13 || state == 0x1C || state == 0x1D || state == 0x1E ||
        state == 0x1B || state == 9) {
        return;
    }
    count           = work->field_542 - 1;
    work->field_542 = count;
    if (count != 0) {
        return;
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    sel         = ((u32)Gp_LcgState >> 16) % 5;
    switch (sel) {
        case 0:
            id = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40100006;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            break;
        case 1:
            id = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40100007;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            break;
        case 2:
            id = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40100008;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            break;
    }
    work->field_542 = 0x14;
}

void Actor01600_Fn017BC(Task* actor)
{
    GpEnemy*        ctx;
    Actor01600Work* work;
    GpCoord*        coord;
    TmdObject*      model;
    s16             frameOffset;
    s16             count;
    s16             tick;
    s16             state;
    s16             height;
    s16             repeatHeight;
    s16             frame;
    s32             contact;
    s32             id;
    void*           old;
    s32             pan8;
    s32             distance;
    GpRec18*        rec;
    s32             pan1;
    s32             pan2;
    s32             pan3;
    s32             pan4;
    s32             pan5;
    s32             pan6;
    s32             pan7;
    u16             flags;
    u16             attackFrame;

    work               = actor->work;
    old                = SCRATCH_HEAD(void);
    rec                = &work->field_2D4;
    SCRATCH_HEAD(void) = old - 8;
    model              = actor->extra.tmd;
    coord              = model->coords;
    ctx                = actor->spawnArg2;
    if (Gp_CountRec18Hi(rec, 0x10000) != 0) {
        work->field_51A = 1;
    }
    if (work->field_51A != 0) {
        work->field_4FE  = 1;
        work->field_516  = 0;
        work->field_510  = 0;
        work->field_4FA  = 0;
        work->field_528  = 0;
        work->field_51E  = 0;
        work->field_520  = 0;
        work->field_50A  = 0;
        work->field_4EA  = 0;
        work->field_2BA &= 0x3FFF;
        work->field_42A &= 0x3FFF;
        Gp_ArmStateF0(1);
    }
    CLEAR_REC18_OCCUPIED(rec);
    if (work->field_53A == 1) {
        count           = (u16)work->field_536 - 1;
        work->field_536 = count;
        if (count == 0) {
            work->field_50A = 0;
            work->field_506 = 2;
            work->field_53A = 0;
        }
    }
    tick            = (u16)work->field_504 + 1;
    work->field_504 = tick;
    if (tick >= 0x1F) {
        work->field_504 = 0;
    }
    state = (u16)work->field_506 - 1;
    switch (state) {
        case 0:
            work->field_2C0 = 0x3E8;
            work->field_50E = 0;
            work->field_538 = 0x10;
            if (work->field_50A >= 0x3E) {
                work->field_50A = 0;
            }
            if (ctx->place->mode == 0) {
                if ((Gp_StateF0.field_2 & 1) || (*(u32*)&Gp_StateF0 & 0xFF140000)) {
                    work->field_53A = 1;
                }
            }
            break;
        case 1:
            work->field_2C0 = 0xFA0;
            work->field_50E = 0;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            if (work->field_50A >= 0x36) {
                work->field_506  = 0x19;
                work->field_50A  = 0;
                work->field_4EA  = 0;
                work->field_514  = 1;
                work->field_42A |= 0xC000;
            }
            break;
        case 24:
            work->field_50E = 0;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            contact         = Actor01600_Fn04EB0(actor) & 0xFF;
            if (contact != 0) {
                flags           = work->field_42A & 0x3FFF;
                work->field_42A = flags;
                if (contact != 0xFF) {
                    distance        = work->field_4E0;
                    work->field_548 = 0U;
                    distance        = abs(distance);
                    work->field_50A = 0;
                    if ((distance >= 0x201) || ((contact & 0xF) == 2)) {
                        work->field_510 = 5;
                        if ((contact & 0xF0) == 0x80) {
                            work->field_506 = 7;
                        } else {
                            work->field_506 = 8;
                        }
                    } else {
                        work->field_510 = 0;
                        work->field_506 = 4;
                    }
                } else {
                    work->field_42A = flags | 0xC000;
                    work->field_4EA = 0;
                }
            }
            if (work->field_50A >= 0x57) {
                work->field_50A = 0;
                work->field_508 = 0;
                work->field_506 = 0x19;
            }
            break;
        case 2:
            work->field_4FA = 0;
            work->field_538 = 0x10;
            work->field_50E = 0;
            if (work->field_50A >= 0x3D) {
                work->field_50A = 0;
                work->field_506 = 3;
            }
            break;
        case 3:
            work->field_538 = 0x10;
            work->field_4FA = 4;
            if (work->field_528 != 0) {
                if (work->field_50A >= 0xC) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0xA;
                }
                height          = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = height;
                if (height >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            if (work->field_50A == 6) {
                work->field_528 = 1;
                work->field_51E = -0x50;
                work->field_520 = (u16)work->field_520 - 0x50;
            }
            attackFrame = (u16)work->field_50A;
            if ((u32)(attackFrame - 5) < 0x10U) {
                if ((s16)attackFrame >= 0xC) {
                    work->field_50E = 0x5A;
                } else {
                    work->field_50E = 0x3C;
                }
                Actor01600_Fn03A60(actor);
            } else {
                work->field_50E = 0;
            }
            if (work->field_50A == 0x14) {
                id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100002;
                pan1 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan1, (s8)gpGetObjDepth(coord));
            }
            if (work->field_50A >= 0x15) {
                id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100001;
                pan2 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan2, (s8)gpGetObjDepth(coord));
                work->field_506 = 0x17;
                work->field_528 = 0;
                work->field_51E = 0;
                work->field_520 = 0;
                work->field_50C = 0;
                work->field_50A = 0;
                work->field_548 = (u16)(work->field_548 + 1);
                work->field_2BA = (work->field_2BA | 0x8000) & 0xBFFF;
            }
            break;
        case 22:
            work->field_538 = 0x10;
            if (work->field_528 != 0) {
                if (work->field_50A >= (work->field_50C + 0xC)) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0xA;
                }
                repeatHeight    = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = repeatHeight;
                if (repeatHeight >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            if (work->field_50A == (work->field_50C + 6)) {
                work->field_528 = 1;
                work->field_51E = -0x50;
                work->field_520 = (u16)work->field_520 - 0x50;
            }
            frameOffset = work->field_50C;
            frame       = work->field_50A;
            if ((frame >= (frameOffset + 6)) && ((frameOffset + 0x15) >= frame)) {
                if ((frameOffset + 0xA) >= frame) {
                    work->field_50E = 0x5A;
                } else {
                    work->field_50E = 0x3C;
                }
                Actor01600_Fn03A60(actor);
            } else {
                work->field_50E = 0;
            }
            if (work->field_50A == (work->field_50C + 0x14)) {
                id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100002;
                pan3 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan3, (s8)gpGetObjDepth(coord));
            }
            if (work->field_50A >= (work->field_50C + 0x17)) {
                id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100001;
                pan4 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan4, (s8)gpGetObjDepth(coord));
                work->field_548 = (u16)(work->field_548 + 1);
                work->field_4FA = 0;
                work->field_528 = 0;
                work->field_51E = 0;
                work->field_520 = 0;
                work->field_514 = 0;
                work->field_50A = 0;
                work->field_50C = -3;
                if ((s16)work->field_548 >= 3) {
                    work->field_506 = 0x18;
                } else {
                    work->field_506 = 0x17;
                }
                work->field_2BA = (work->field_2BA | 0x8000) & 0xBFFF;
            }
            break;
        case 23:
            work->field_538 = 0x10;
            work->field_4FA = 4;
            work->field_50E = 0;
            if (work->field_50A >= 0xA) {
                work->field_506  = 0x19;
                work->field_514  = 1;
                work->field_51A  = 0;
                work->field_4EA  = 0;
                work->field_42A |= 0xC000;
                work->field_2BA  = (work->field_2BA | 0x8000) & 0xBFFF;
            }
            break;
        case 6:
        case 7:
            work->field_538 = 0x10;
            work->field_4FA = 0;
            if (work->field_506 == 7) {
                if (work->field_50A == 0xF) {
                    id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100003;
                    pan5 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan5, (s8)gpGetObjDepth(coord));
                }
                if (work->field_50A == 0x11) {
                    id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100004;
                    pan8 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan8, (s8)gpGetObjDepth(coord));
                }
            } else {
                if (work->field_50A == 0xF) {
                    id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100004;
                    pan6 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan6, (s8)gpGetObjDepth(coord));
                }
                if (work->field_50A == 0x12) {
                    id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100003;
                    pan7 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan7, (s8)gpGetObjDepth(coord));
                }
            }
            work->field_50E = 0;
            if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                Actor01600_Fn03A60(actor);
            }
            if (work->field_50A >= 0x1C) {
                work->field_508  = 0;
                work->field_50A  = 0;
                work->field_2BA |= 0x8000;
                if (work->field_510 == 7) {
                    work->field_514 = 0;
                    work->field_50A = 0;
                    work->field_506 = 4;
                }
            }
            break;
        case 9:
            work->field_4FA = 0;
            work->field_538 = 0x10;
            work->field_50E = 0;
            if (work->field_50A >= 0x28) {
                work->field_51A = 1;
                work->field_50A = 0;
                work->field_506 = 4;
            }
            break;
        default:
            break;
    }
    SCRATCH_POP_BYTES(8);
}

void Actor01600_Fn020F8(Task* actor)
{
    PlayerStatus*   config = &Player_Status;
    s32             neg_velocity;
    s32             reset_y;
    s32             reset_y2;
    SVECTOR         offset;
    s32             distance;
    GpEnemy*        ctx;
    Actor01600Work* work;
    GpCoord*        effectCoord;
    GpCoord*        coord;
    s16             targetKind;
    s16             temp_v0_3;
    s16             temp_v0_4;
    s16             temp_v0_7;
    s16             temp_v0_8;
    s16             temp_v1;
    s16             temp_v1_10;
    s16             temp_v1_5;
    s16             temp_v1_6;
    s16             temp_v1_7;
    s32             id;
    s32             flags;
    s32             angle;
    s32             angle2;
    s32             var_v1;
    s32             pan10;
    s32             pan11;
    s32             pan12;
    s32             pan13;
    s32             pan14;
    s32             pan_case3;
    s32             pan15;
    s32             pan16;
    s32             pan17;
    s32             pan18;
    s32             pan19;
    s32             pan20;
    s32             pan21;
    s32             pan23;
    s32             pan24;
    s32             pan_msg_zero;
    s32             pan25;
    s32             pan26;
    s32             pan27;
    s32             pan28;
    s32             pan29;
    s32             pan2;
    s32             pan30;
    s32             pan31;
    s32             pan32;
    s32             pan33;
    s32             pan34;
    s32             pan35;
    s32             pan36;
    s32             pan3;
    s32             pan4;
    s32             pan5;
    s32             pan6;
    s32             pan7;
    s32             pan8;
    s32             pan9;
    u16             temp_v0_6;
    u16             temp_v1_3;
    u16             temp_v1_8;
    GpCoord*        attachedCoord;
    GpCoord*        attachedCoord2;
    GpCoord*        attachedOffset;
    GpCoord*        attachedOffset2;

    ctx         = actor->spawnArg2;
    work        = actor->work;
    coord       = actor->extra.tmd->coords;
    flags       = Actor01600_Fn052C4(actor) & 0xFF;
    effectCoord = &actor->extra.tmd->coords[2];
    memset(&offset, 0, 8);
    offset.vy = 0x32;
    temp_v1   = work->field_516;
    switch (temp_v1) {
        case 0:
            work->field_4FA = 4;
            work->field_506 = 5;
            work->field_50E = 0;
            work->field_538 = 0x10;
            if ((u32)((u16)work->field_50A - 9) < 9U) {
                work->field_50E = 0x3C;
                Actor01600_Fn03A60(actor);
                Actor01600_Fn06974(actor, 0x46);
                if (work->field_4F2 == 0) {
                    work->field_4F2 = 1;
                }
            }
            if (work->field_50A >= 0x12) {
                id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100004;
                pan2 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan2, (s8)gpGetObjDepth(coord));
                SOFT_USE_REG(work);
                if (!(Actor01600_Fn06C1C(actor) & 0xFF)) {
                    work->field_516 = 1;
                    work->field_50A = 0;
                    work->field_50E = 0;
                    work->field_506 = 0x17;
                    goto block_8;
                }
            } else {
            block_8:
                angle = Actor01600_Fn045A8(actor, &distance);
                SOFT_USE_REG(angle);
                if (!(Actor01600_Fn06C94(actor, angle, distance) & 0xFF)) {
                    if (Actor01600_Fn04974(actor, angle, distance, flags) & 0xFF) {
                        if (work->field_50A >= 0xA) {
                            id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100003;
                            pan3 = (s8)Gp_GetObjPan(coord);
                            SndEvt_EnqueueType6(id, (s32)pan3, (s8)gpGetObjDepth(coord));
                            SOFT_USE_REG(work);
                            id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100004;
                            pan4 = (s8)Gp_GetObjPan(coord);
                            SndEvt_EnqueueType6(id, (s32)pan4, (s8)gpGetObjDepth(coord));
                            SOFT_USE_REG(work);
                            work->field_50A = 0;
                            return;
                        }
                    } else if ((Actor01600_Fn06D74(actor, angle, distance) & 0xFF) && (work->field_50A >= 0xA)) {
                        id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100003;
                        pan5 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan5, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                        id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100004;
                        pan6 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan6, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                        work->field_50A = 0;
                        return;
                    }
                } else {
                    default:
                        return;
                }
            }
            break;
        case 1:
            work->field_4FA = 4;
            work->field_538 = 0x10;
            work->field_50E = 0x1E;
            work->field_506 = 6;
            if ((u32)((u16)work->field_50A - 8) < 8U) {
                work->field_50E = 0x3C;
                Actor01600_Fn03A60(actor);
                Actor01600_Fn06974(actor, -0x5A);
                if (work->field_4F2 == 0) {
                    work->field_4F2 = 1;
                }
            }
            if (work->field_50A == 0x12) {
                id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100003;
                pan7 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan7, (s8)gpGetObjDepth(coord));
                SOFT_USE_REG(work);
            }
            if ((u32)((u16)work->field_50A - 0x1A) < 9U) {
                work->field_50E = 0x3C;
                Actor01600_Fn03A60(actor);
                Actor01600_Fn06974(actor, 0x46);
                if (work->field_4F2 == 0) {
                    work->field_4F2 = 1;
                }
            }
            if (work->field_50A >= 0x22) {
                id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100004;
                pan8 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan8, (s8)gpGetObjDepth(coord));
                work->field_50A = 0;
                if (Actor01600_Fn06C1C(actor) & 0xFF) {
                    return;
                }
            }
            angle2 = Actor01600_Fn045A8(actor, &distance);
            SOFT_USE_REG(angle2);
            if (!(Actor01600_Fn06C94(actor, angle2, distance) & 0xFF)) {
                if (Actor01600_Fn04974(actor, angle2, distance, flags) & 0xFF) {
                    if (work->field_50A >= 8) {
                        id   = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100003;
                        pan9 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan9, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100004;
                        pan10 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan10, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                        work->field_50A = 0;
                        return;
                    }
                } else if ((Actor01600_Fn06D74(actor, angle2, distance) & 0xFF) && (work->field_50A >= 8)) {
                    id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100003;
                    pan11 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan11, (s8)gpGetObjDepth(coord));
                    SOFT_USE_REG(work);
                    id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100004;
                    pan12 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan12, (s8)gpGetObjDepth(coord));
                    SOFT_USE_REG(work);
                    work->field_50A = 0;
                    return;
                }
            }
            break;
        case 2:
            work->field_506 = 7;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            work->field_50E = 0;
            if (work->field_50A == 0xF) {
                id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100003;
                pan13 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan13, (s8)gpGetObjDepth(coord));
            }
            if (work->field_50A == 0x11) {
                var_v1 = 0x40100004;
                id     = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | var_v1;
                pan14  = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan14, (s8)gpGetObjDepth(coord));
            }
            if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                Actor01600_Fn03A60(actor);
            }
            if (work->field_50A >= 0x1C) {
                work->field_508 = 0;
                work->field_50A = 0;
                if (work->field_510 == 7) {
                    work->field_50A = 0;
                    work->field_516 = 0;
                    return;
                }
            }
            break;
        case 3:
            work->field_506 = 8;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            work->field_50E = 0;
            if (work->field_50A == 0xF) {
                id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100004;
                pan15 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan15, (s8)gpGetObjDepth(coord));
            }
            if (work->field_50A == 0x12) {
                var_v1    = 0x40100003;
                id        = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | var_v1;
                pan_case3 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan_case3, (s8)gpGetObjDepth(coord));
            }
            if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                Actor01600_Fn03A60(actor);
            }
            if (work->field_50A >= 0x1C) {
                work->field_508 = 0;
                work->field_50A = 0;
                if (work->field_510 == 7) {
                    work->field_50A = 0;
                    work->field_516 = 0;
                    return;
                }
            }
            break;
        case 4:
            work->field_538 = 0x10;
            work->field_506 = 9;
            work->field_50E = 0;
            work->field_4FA = 0;
            if (work->field_50A == 1) {
                id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x4010000E;
                pan16 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan16, (s8)gpGetObjDepth(coord));
                SOFT_USE_REG(work);
            }
            if (work->field_50A >= 0x25) {
                work->field_516 = 0;
                work->field_50A = 0;
                work->field_526 = 0;
                return;
            }
            break;
        case 5:
            work->field_538 = 0x10;
            work->field_50E = 0;
            work->field_4FA = 0;
            work->field_506 = 0x1A;
            if (work->field_528 != 0) {
                work->field_538 = 0x14;
                if (work->field_51E < 0) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0x14;
                }
                temp_v0_3       = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = temp_v0_3;
                if (temp_v0_3 >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            if (work->field_50A == 8) {
                work->field_528 = 1;
                work->field_530 = 0;
                work->field_51E = -0x96;
                work->field_520 = (u16)work->field_520 - 0x96;
            }
            if (work->field_50A == 0x1E) {
                id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100001;
                pan17 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan17, (s8)gpGetObjDepth(coord));
                SOFT_USE_REG(work);
                id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100002;
                pan18 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan18, (s8)gpGetObjDepth(coord));
                SOFT_USE_REG(work);
                work->field_528 = 0;
                work->field_51E = 0;
                work->field_520 = 0;
            }
            temp_v1_3 = (u16)work->field_50A;
            if ((u32)(temp_v1_3 - 8) < 0x17U) {
                if (work->field_52A == 0) {
                    if ((s16)temp_v1_3 < 0xC) {
                        work->collision.named.field_2FE = -0x258;
                        work->collision.named.field_308 = 0x258;
                        work->field_50E                 = 0x12C;
                    } else if ((s16)temp_v1_3 < 0x18) {
                        work->collision.named.field_2FE = -0x190;
                        work->collision.named.field_308 = 0x190;
                        work->field_50E                 = 0x32;
                    } else {
                        work->field_50E = 0x19;
                    }
                }
                Actor01600_Fn03A60(actor);
            }
            if ((work->field_50A == 9) && (work->field_53C == 0)) {
                work->field_3EA |= 0x8000;
            }
            if (work->field_50A == 0x17) {
                work->collision.named.field_30A = (u16)(work->collision.named.field_30A | 0x4000);
                work->field_3EA                &= 0x7FFF;
            }
            if (work->field_50A >= 0x34) {
                if (work->field_53C != 0) {
                    work->field_516 = 4;
                } else {
                    work->field_516 = 0;
                }
                work->field_53C = 0;
                work->field_50A = 0;
                work->field_532 = 0;
                return;
            }
            break;
        case 6:
            work->field_538 = 0x10;
            work->field_4FA = 0;
            if (work->field_528 != 0) {
                work->field_538 = 0x14;
                if (work->field_51E < 0) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0x14;
                }
                temp_v0_4       = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = temp_v0_4;
                if (temp_v0_4 >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            temp_v1_5 = work->field_506;
            switch (temp_v1_5) {
                case 26:
                    temp_v1_6 = work->field_50A;
                    if (temp_v1_6 == 8) {
                        work->field_530 = 0;
                        work->field_528 = 1;
                        if (work->field_544 == 0) {
                            work->field_51E = -0x96;
                        } else {
                            work->field_51E = -0xC8;
                        }
                        work->field_520 = (u16)work->field_520 + (u16)work->field_51E;
                    }
                    work->field_50E = 0;
                    if (work->field_50A >= 8) {
                        if (work->field_544 == 0) {
                            if (work->field_50A < 0xC) {
                                work->collision.named.field_2FE = -0x258;
                                work->collision.named.field_308 = 0x258;
                                work->field_50E                 = 0x12C;
                            } else {
                                if (work->field_50A < 0x18) {
                                    work->collision.named.field_2FE = -0x190;
                                    work->collision.named.field_308 = 0x190;
                                    work->field_50E                 = 0x32;
                                } else {
                                    work->field_50E = 0x19;
                                }
                            }
                        } else {
                            if (work->field_50A < 0xC) {
                                work->field_50E = 0x1F4;
                            } else if (work->field_50A < 0x18) {
                                work->field_50E = 0x46;
                            } else {
                                work->field_50E = 0x32;
                            }
                        }
                    }
                    if ((work->field_50A == 9) && (work->field_53C == 0)) {
                        work->field_3EA |= 0x8000;
                    }
                    if (work->field_50A == 0x17) {
                        work->collision.named.field_30A = (u16)(work->collision.named.field_30A | 0x4000);
                        work->field_3EA                &= 0x7FFF;
                    }
                    if (work->field_50A >= 0x19) {
                        work->field_506 = 0x1B;
                        work->field_50A = 0;
                        work->field_544 = 0;
                        work->field_53C = 0;
                        return;
                    }
                    break;
                case 27:
                    work->field_50E = 0x3C;
                    if (work->field_50A == 0xB) {
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100005;
                        pan19 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan19, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A >= 0xD) {
                        work->field_538 = 0x10;
                        work->field_50E = 0;
                    }
                    if (work->field_50A >= 0x28) {
                        work->field_506 = 0x15;
                        work->field_50A = 0;
                    }
                    break;
                case 21:
                    if (work->field_50A == 0x1F) {
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100001;
                        pan20 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan20, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100002;
                        pan21 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan21, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A >= 0x2F) {
                        work->field_516 = 0;
                        work->field_50A = 0;
                        work->field_532 = 0;
                        return;
                    }
                    break;
            }
            break;
        case 7:
            work->field_526 = 1;
            work->field_50E = 0;
            work->field_4FA = 0;
            work->field_538 = 0x10;
            work->field_528 = 0;
            work->field_51E = 0;
            temp_v1_7       = (u16)work->field_506 - 9;
            work->field_520 = 0;
            switch (temp_v1_7) {
                case 19:
                    Actor01600_Fn03A60(actor);
                    Actor01600_Fn06974(actor, 0xA);
                    if (work->field_50A >= 0xD) {
                        work->field_50E = 0x5A;
                    }
                    if (work->field_50A >= 0x12) {
                        if (Actor01600_Fn047A0(actor) & 0xFF) {
                            targetKind = work->field_53E;
                            if (targetKind == 1) {
                                if (Mc_SaveData.companionHp > 0) {
                                    if (Gp_DispatchMsg(work->field_4D4, 0x3F9, Gp_PackObjPair(ctx, 0), 0) == targetKind) {
                                        work->field_506 = 9;
                                        work->field_50E = 0;
                                        work->field_50A = 0;
                                        work->field_4E4 = (s32)coord->coord.t[1];
                                        return;
                                    }
                                    goto block_136;
                                }
                                work->field_506 = 9;
                                work->field_50E = 0;
                                work->field_50A = 0;
                                work->field_4E4 = (s32)coord->coord.t[1];
                                return;
                            }
                            if (config->hp <= 0) {
                                work->field_506 = 9;
                                work->field_50E = 0;
                                work->field_50A = 0;
                                work->field_4E4 = (s32)coord->coord.t[1];
                                return;
                            }
                        block_136:
                            work->field_534 = 1;
                            work->field_4FA = 0;
                            work->field_506 = 0x1D;
                            id              = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x4010000D;
                            pan23           = (s8)Gp_GetObjPan(coord);
                            SndEvt_EnqueueType6(id, (s32)pan23, (s8)gpGetObjDepth(coord));
                            SOFT_USE_REG(work);
                            if (work->field_53E != 0) {
                                Actor01600_D127D8.field_4 = 1;
                                Gp_DispatchMsg(work->field_4D4, 0x3F4, (s32)&Actor01600_D127D8, 0);
                                id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x4065000A;
                                pan24 = (s8)Gp_GetObjPan(coord);
                                SndEvt_EnqueueType6(id, (s32)pan24, (s8)gpGetObjDepth(coord));
                            } else {
                                Actor01600_D127D8.field_4 = 2;
                                Gp_DispatchMsg(work->field_4D4, 0x3FF, (s32)&Actor01600_D127D8, 0);
                                id           = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 6;
                                pan_msg_zero = (s8)Gp_GetObjPan(coord);
                                SndEvt_EnqueueType6(id, (s32)pan_msg_zero, (s8)gpGetObjDepth(coord));
                            }
                            work->field_554 = 2;
                            goto block_141;
                        }
                        work->field_4FA = 4;
                        work->field_526 = 0;
                        work->field_506 = 0x1B;
                    block_141:
                        work->field_50E = 0;
                        work->field_50A = 0;
                        work->field_54C = 0U;
                        return;
                    }
                    break;
                case 20:
                    attachedCoord     = work->field_4D4->extra.tmd->coords;
                    attachedOffset    = &attachedCoord[17];
                    coord->coord.t[0] = attachedCoord->coord.t[0] + attachedOffset->coord.t[0];
                    coord->coord.t[2] = attachedCoord->coord.t[2] + attachedOffset->coord.t[2];
                    temp_v0_6         = work->field_54C + 1;
                    work->field_54C   = temp_v0_6;
                    if ((s16)temp_v0_6 == 0x14) {
                        work->field_54C = 0U;
                        work->field_554 = (u16)work->field_554 + 1;
                        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, effectCoord, &offset, (GpEffArg*)&work->pad_3EC[0x18]);
                        if (work->field_53E == 0) {
                            Gp_SpawnPadLerp(0xA, 0x80U, 0x80U);
                        }
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x4010000D;
                        pan25 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan25, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                        if (work->field_53E == 0) {
                            Gp_DispatchMsg(work->field_4D4, 0x3F9, Gp_PackObjPair(ctx, 0), 0);
                        }
                        if (config->hp <= 0) {
                            if (work->field_53E == 0) {
                                work->field_506           = 9;
                                work->field_50E           = 0;
                                work->field_50A           = 0;
                                reset_y                   = coord->coord.t[1];
                                work->field_534           = 0;
                                work->field_4E4           = reset_y;
                                Actor01600_D127D8.field_4 = 0;
                                Actor01600_D127D8.field_8 = 0;
                                Actor01600_D127D8.field_C = 0;
                                Gp_DispatchMsg(work->field_4D4, 0x3F1, 0, 0);
                                return;
                            }
                            goto block_156;
                        }
                        goto block_150;
                    }
                block_150:
                    if (work->field_53E == 0) {
                        if (work->field_50A >= 0x31) {
                            if (work->field_554 >= 5) {
                                Actor01600_D127D8.field_4 = 3;
                                Actor01600_D127D8.field_8 = 1;
                                Actor01600_D127D8.field_C = 1;
                                Gp_DispatchMsg(work->field_4D4, 0x3FF, (s32)&Actor01600_D127D8, 0);
                                work->field_508 = 0;
                                work->field_50A = 0;
                                work->field_506 = 0x1E;
                            }
                            if (work->field_50A >= 0x31) {
                                work->field_50A = 0;
                                return;
                            }
                        }
                    } else {
                    block_156:
                        if (Mc_SaveData.companionHp <= 0) {
                            work->field_506           = 9;
                            work->field_50E           = 0;
                            work->field_50A           = 0;
                            reset_y2                  = coord->coord.t[1];
                            work->field_534           = 0;
                            work->field_4E4           = reset_y2;
                            Actor01600_D127D8.field_4 = 0;
                            Actor01600_D127D8.field_8 = 0;
                            Actor01600_D127D8.field_C = 0;
                            Gp_DispatchMsg(work->field_4D4, 0x3F1, 0, 0);
                            return;
                        }
                        if (work->field_50A >= 0x31) {
                            work->field_50A = 0;
                            work->field_506 = 0x1E;
                            return;
                        }
                    }
                    break;
                case 21:
                    if (work->field_50A >= 0x10) {
                        work->field_50E = 0;
                        temp_v1_8       = (u16)work->field_50A;
                        if ((u32)(temp_v1_8 - 0x10) < 2U) {
                            work->field_50E = -0x12C;
                        } else if ((s16)temp_v1_8 < 0x1D) {
                            work->field_50E = -0x64;
                        } else if ((s16)temp_v1_8 < 0x24) {
                            work->field_50E = -0x28;
                        }
                    } else {
                        attachedCoord2    = work->field_4D4->extra.tmd->coords;
                        attachedOffset2   = &attachedCoord2[17];
                        coord->coord.t[0] = attachedCoord2->coord.t[0] + attachedOffset2->coord.t[0];
                        coord->coord.t[2] = attachedCoord2->coord.t[2] + attachedOffset2->coord.t[2];
                    }
                    if (work->field_50A >= 0x2D) {
                        work->field_534           = 0;
                        Actor01600_D127D8.field_4 = 0;
                        Actor01600_D127D8.field_8 = 0;
                        Actor01600_D127D8.field_C = 0;
                        Gp_DispatchMsg(work->field_4D4, 0x3F1, 0, 0);
                    }
                    if (work->field_50A >= 0x38) {
                        work->field_506   = 9;
                        work->field_50E   = 0;
                        Actor01600_D12870 = 0;
                    }
                    if (work->field_50A == 0x13) {
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x4010000B;
                        pan26 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan26, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                        if (work->field_53E == 0) {
                            Gp_SpawnPadLerp(0xA, 0xD0U, 0xD0U);
                        }
                    }
                    if (work->field_50A == 0x1F) {
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x4010000C;
                        pan27 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan27, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100001;
                        pan28 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan28, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A == 0x21) {
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100002;
                        pan29 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan29, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                        return;
                    }
                    break;
                case 0:
                    if (work->field_50A == 1) {
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x4010000E;
                        pan30 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan30, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A >= 0x25) {
                        work->field_516 = 0;
                        work->field_50A = 0;
                        work->field_526 = 0;
                        return;
                    }
                    break;
                case 18:
                    work->field_50E = 0x28;
                    work->field_526 = 0;
                    work->field_4FA = 4;
                    if (work->field_50A >= 0xD) {
                        work->field_50E = 0;
                    }
                    if (work->field_50A == 0xD) {
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100005;
                        pan31 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan31, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A >= 0x30) {
                        work->field_506 = 0x15;
                        work->field_50E = 0;
                        work->field_50A = 0;
                    }
                    break;
                case 12:
                    work->field_526 = 0;
                    if (work->field_50A == 0x1F) {
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100001;
                        pan32 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan32, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100002;
                        pan33 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan33, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A >= 0x2F) {
                        work->field_50E = 0;
                        work->field_516 = 0;
                        work->field_50A = 0;
                        work->field_526 = 0;
                        return;
                    }
                    break;
            }
            break;
        case 8:
            work->field_538 = 0x14;
            work->field_4FA = 0;
            switch (work->field_506) {
                case 11:
                case 14:
                    temp_v0_7 = work->field_50A;
                    if (temp_v0_7 <= 0) {
                        work->collision.named.field_2FE = -0x258;
                        work->collision.named.field_308 = 0x258;
                        work->field_50E                 = 0x1F4;
                    } else if (temp_v0_7 < 0x13) {
                        work->collision.named.field_2FE = -0x190;
                        work->collision.named.field_308 = 0x190;
                        work->field_50E                 = 0x4B;
                    } else {
                        work->field_50E = 0;
                    }
                    if (work->field_506 == 0xE) {
                        neg_velocity    = -work->field_50E;
                        work->field_50E = neg_velocity;
                    }
                    if (work->field_528 != 0) {
                        temp_v0_8 = (u16)work->field_51E + 0xF;
                        SOFT_TOUCH_REG(temp_v0_8);
                        temp_v1_10      = (u16)work->field_520 + temp_v0_8;
                        work->field_520 = temp_v1_10;
                        work->field_51E = temp_v0_8;
                        if (temp_v1_10 >= 0) {
                            id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100005;
                            pan36 = (s8)Gp_GetObjPan(coord);
                            SndEvt_EnqueueType6(id, (s32)pan36, (s8)gpGetObjDepth(coord));
                            SOFT_USE_REG(work);
                            work->field_528 = 0;
                            work->field_520 = 0;
                        }
                    }
                    if (work->field_50A >= 0x28) {
                        if (work->field_506 == 0xE) {
                            work->field_506 = 0x16;
                        } else {
                            work->field_506 = 0x15;
                        }
                        work->field_50A = 0;
                        work->field_50E = 0;
                        work->field_528 = 0;
                        work->field_51E = 0;
                        work->field_520 = 0;
                        work->field_532 = 0;
                        return;
                    }
                    break;
                case 21:
                case 22:
                    work->field_538 = 0x10;
                    work->field_50E = 0;
                    if (work->field_50A == 0x19) {
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100002;
                        pan34 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan34, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A == 0x1B) {
                        id    = (((u16)((GpEnemy*)actor->spawnArg2)->placeKey >> 0xC) << 8) | 0x40100001;
                        pan35 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(id, (s32)pan35, (s8)gpGetObjDepth(coord));
                        SOFT_USE_REG(work);
                    }
                    if (work->field_50A >= 0x2F) {
                        work->field_516 = 0;
                        work->field_50A = 0;
                    }
                    break;
                default:
                    break;
            }
            break;
    }
}

void Actor01600_Fn03A60(Task* arg0)
{
    Task**                slot;
    Task**                slots;
    s16                   mode;
    s16                   diff;
    s32                   current;
    s16                   angle;
    s32                   absoluteDiff;
    s32                   randomTurn;
    s16                   turn;
    s32                   randomStep;
    s16                   wrap;
    s32                   amount;
    s32                   remaining;
    s32                   remaining2;
    u16                   wanted;
    u32                   randomState;
    u32                   randomState2;
    u32                   random;
    GpCoord*              playerCoord;
    Actor01600Work*       work;
    GpCoord*              coord;
    Actor01600RotScratch* allocated;
    Actor01600RotScratch* scratch;

    work                               = arg0->work;
    coord                              = arg0->extra.tmd->coords;
    slots                              = Gp_ActorSlots;
    slot                               = &slots[Actor01600_Fn052C4(arg0) & 0xFF];
    allocated                          = SCRATCH_HEAD(Actor01600RotScratch);
    allocated                         -= 1;
    SCRATCH_HEAD(Actor01600RotScratch) = allocated;
    mode                               = work->field_510;
    playerCoord                        = (*slot)->extra.tmd->coords;
    scratch                            = allocated;
    switch (mode) {
        case 0:
            scratch->position.vx = (s32)(playerCoord->coord.t[0] - coord->coord.t[0]);
            scratch->position.vy = 0;
            scratch->position.vz = (s32)(playerCoord->coord.t[2] - coord->coord.t[2]);
            wanted               = ratan2((s32)(s16)scratch->position.vx, (s32)(s16)scratch->position.vz) & 0xFFF;
            diff                 = wanted - (work->field_4FC & 0xFFF);
            absoluteDiff         = diff >= 0 ? diff : -diff;
            turn                 = diff;
            if (absoluteDiff < 0x21) {
                work->field_4FC = wanted;
            } else {
                if (absoluteDiff >= 0x801) {
                    wrap = diff - 0x1000;
                    if (diff <= 0) {
                        wrap = 0x1000 - diff;
                    }
                    turn = wrap;
                }
                current = (s16)work->field_4FC;
                if (turn > 0) {
                    work->field_4FC = (u16)(current + 0x20);
                } else {
                    work->field_4FC = (u16)(current - 0x20);
                }
            }
            break;
        case 1:
            randomState = (Gp_LcgState * 5) + 0x71357911;
            random      = randomState >> 0x10;
            randomTurn  = (random & 0x3FF) + 0x400;
            Gp_LcgState = (s32)randomState;
            if (random & 0x400) {
                randomTurn = -randomTurn;
            }
            work->field_512 = randomTurn;
            work->field_510 = 2;
            break;
        case 2:
            randomState2 = (Gp_LcgState * 5) + 0x71357911;
            randomStep   = (randomState2 >> 0x10) & 0x17;
            Gp_LcgState  = (s32)randomState2;
            if (work->field_512 <= 0) {
                randomStep = -randomStep;
            }
            turn            = randomStep;
            work->field_4FC = (u16)(work->field_4FC + turn);
            remaining       = work->field_512 - turn;
            if ((remaining >= 0 ? remaining : -remaining) < 0x20) {
                work->field_510 = 0;
                work->field_512 = 0;
            } else {
                work->field_512 -= turn;
            }
            break;
        case 7:
            break;
        case 5:
            work->field_510 = 6;
            work->field_512 = (s16)work->field_4E0;
            break;
        case 6:
            amount = work->field_4F0;
            if (work->field_512 <= 0) {
                amount = -amount;
            }
            turn            = amount;
            angle           = work->field_4FC + amount;
            work->field_4FC = (u16)angle;
            if (angle >= 0x801) {
                work->field_4FC = angle - 0x1000;
            } else if (angle < -0x800) {
                work->field_4FC = angle + 0x1000;
            }
            remaining2 = work->field_512 - turn;
            if ((remaining2 >= 0 ? remaining2 : -remaining2) < work->field_4F0) {
                work->field_510 = 7;
                work->field_512 = 0;
            } else {
                work->field_512 -= turn;
            }
            break;
    }
    scratch->rotation.vx = 0;
    scratch->rotation.vy = (u16)work->field_4FC;
    scratch->rotation.vz = 0;
    RotMatrix(&scratch->rotation, &coord->coord);
    SCRATCH_POP(Actor01600RotScratch);
}

void Actor01600_Fn03D48(Task* arg0)
{
    Actor01600Work* work;
    GpCoord*        coord;
    s16             anim;
    s32             i;

    work = arg0->work;

    if (work->field_506 != 0xFF) {
        if (work->field_506 != work->field_508) {
            work->field_508 = (s16)(u16)work->field_506;
            work->field_50A = 0;
            for (i = 1; i < 9; i++) {
                func_800B4114(&work->anim, i, work->field_506, 0, (s32)work->field_4FA);
            }
        } else {
            work->field_50A = (u16)work->field_50A + 1;
            for (i = 1; i < 9; i++) {
                work->slots[i].rate = (u8)work->field_538;
                Gp_AnimTickIndex(&work->anim, i);
            }
        }
        anim = work->field_506;
        if (anim == 28 || anim == 30 || anim == 9 || anim == 21 || anim == 22 || anim == 5 || anim == 6 || anim == 27) {
            arg0->extra.tmd->coords[1].coord.t[0] = 0;
            arg0->extra.tmd->coords[1].coord.t[2] = 0;
            arg0->extra.tmd->coords[1].coord.t[0] = 0;
            arg0->extra.tmd->coords[1].coord.t[2] = 0;
            return;
        } else if (anim == 29) {
            arg0->extra.tmd->coords[1].coord.t[0] = 0;
            coord                                 = arg0->extra.tmd->coords;
            coord[1].coord.t[2]                   = (s32)(coord[1].coord.t[2] - 0x2BC);
        }
    }
}

void Actor01600_Fn03EEC(Task* arg0)
{
    VECTOR3                  pos;
    Actor01600GroundScratch* head;
    Actor01600GroundScratch* scratch;
    Actor01600Work*          work;
    GpCoord*                 coord;
    s32                      worldZ;
    s32                      height;
    s32                      mode;
    s32                      z;
    Actor01600GroundScratch* allocated;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_526 == 0) {
        head                                  = SCRATCH_HEAD(Actor01600GroundScratch);
        allocated                             = head - 1;
        SCRATCH_HEAD(Actor01600GroundScratch) = allocated;
        /* Not `work->field_528`: spelled as a member access, the read is
           scheduled above the scratch-head store, and the ROM reads it after. */
        mode = *(s16*)((u8*)work + OFFSET_OF(Actor01600Work, field_528));
        do {
            scratch = allocated;
            if (mode != 0) {
                gte_SetRotMatrix(&coord->workm);
                scratch->offset.vx = 0;
                height             = work->field_520 - 0x80;
                scratch->offset.vz = 0;
                scratch->offset.vy = -height;
                gte_ldv0(&scratch->offset);
                gte_rtv0();
                gte_stlvnl(&scratch->pos);
                head[-1].pos.vx = (s32)(head[-1].pos.vx + coord->workm.t[0]);
                scratch->pos.vy = (s32)(scratch->pos.vy + coord->workm.t[1]);
                SCHED_BARRIER();
                z = scratch->pos.vz;
                TOUCH_REG(z);
                worldZ = z + coord->workm.t[2];
            } else {
                head[-1].pos.vx = (s32)coord->workm.t[0];
                scratch->pos.vy = (s32)coord->workm.t[1];
                worldZ          = coord->workm.t[2];
            }
            scratch->pos.vz = worldZ;
        } while (0);
        Gp_DrawEffGroundQuad(&scratch->pos, 0x1C0, 0);
        SCRATCH_POP(Actor01600GroundScratch);
        return;
    }
    if (func_800EA1A8((VECTOR3*)coord[1].workm.t, &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x1C0, Gp_State1C->groundShade);
    }
}

void Actor01600_Fn04054(GpEnemy* arg0, Task* arg1)
{
    Actor01600Work* work;
    TmdObject*      obj;
    GpCoord*        coords;
    GpCoord*        attach;
    GpCoord*        body;
    Actor01600Work* w;
    GpStateF0*      state;
    GpStateC08*     pad;
    SVECTOR         aim;
    s32             dist;
    s32             anim;
    s16             phase;
    s16             timer;
    s16             count;
    s32             mode;
    /* Both calls below take the task; pinning it to the argument register
       emits the one shared `move $a0, $s2` the branch delay slot uses. */
    register Task* task asm("a0");

    obj    = arg1->extra.tmd;
    work   = arg1->work;
    coords = obj->coords;
    mode   = Gp_StateF0.field_4;
    if (mode == 1) {
        return;
    }
    if (mode > 1) {
        if (mode == 2) {
            obj->flags              |= 0x80;
            arg0->node.state.b.flags = 1;
            return;
        }
    }
    switch (work->field_502) {
        case 0:
            Actor01600_Fn06F10(arg1);
            if (work->field_540 != 2) {
                anim            = work->field_4FE;
                work->field_4FA = 0;
                work->field_538 = 0x14;
                if (anim != 2 && anim != 5) {
                    dist = Actor01600_Fn045A8(arg1, (s32*)&aim);
                    if (dist < 0) {
                        dist = -dist;
                    }
                    work->field_506 = (dist < 0x400) ? 0xE : 0xB;
                }
                work->field_504 = 0;
                work->field_518 = 0x1000;
                work->field_49C = coords[0].coord;
                Gp_SetLightMode(arg0, 1);
            }
            arg0->node.state.b.flags = 1;
            arg0->recs               = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj((GpObj*)work->field_40C);
            Gp_UnlinkObj((GpObj*)work->field_29C);
            Gp_UnlinkObj((GpObj*)work->collision.field_2EC);
            Gp_UnlinkObj((GpObj*)work->field_3CC);
            state = &Gp_StateF0;
            task  = arg1;
            if (state->field_1C >= 3) {
                if (Actor01600_Fn06F78(task) == 1) {
                    state->field_1C = state->field_1C + 1;
                }
            } else {
                Gp_ReleaseStateF0Add(task, 0x10);
            }
            work->field_502 = 1;
            break;
        case 1:
            Actor01600_Fn06880(arg1);
            phase           = work->field_504 + 1;
            work->field_504 = phase;
            if (work->field_540 != 2) {
                if (phase == 0xA) {
                    obj->flags = 2;
                }
                if (work->field_504 == 0xF) {
                    Gp_SpawnEff(0x600A5, coords, 1, NULL);
                }
                if (work->field_504 < 0x10) {
                    body = arg1->extra.tmd->coords;
                    w    = arg1->work;

                    w->field_4BC = body->coord.t[0];
                    w->field_4C0 = body->coord.t[1];
                    w->field_4C4 = body->coord.t[2];

                    body->coord.t[0] += (body->coord.m[0][2] * w->field_50E) >> 12;
                    body->coord.t[2] += (body->coord.m[2][2] * w->field_50E) >> 12;
                    if (w->field_530 == 0) {
                        if (w->field_528 != 0) {
                            body->coord.t[1] += w->field_51E;
                        } else {
                            body->coord.t[1] += 0x80;
                        }
                    }
                }
            }
            if (work->field_504 >= 0x3C) {
                work->field_502 = 2;
            }
            break;
        case 2:
            if (Gp_StateF0.field_1C >= 3) {
                if (Actor01600_D12874 == 1) {
                    pad = &Gp_StateC08;
                    if (pad->field_A == 1) {
                        break;
                    }
                    if (gDisplayState.pendingMode != 0) {
                        break;
                    }
                    pad->field_6 |= 1;
                    Gp_PulseState1C();
                    Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, (s32)arg1, 0);
                    work->field_502         = 0xFF;
                    arg1->extra.tmd->flags |= 0x80;
                    arg1->extra.tmd->flags |= 4;
                    break;
                }
                Gp_ReleaseStateF0Add(arg1, 0x10);
            }
            work->field_54E         = 0x3C;
            arg1->extra.tmd->flags |= 0x80;
            Actor01600_D12874--;
            arg1->extra.tmd->flags |= 4;
            work->field_502         = 3;
            break;
        case 3:
            timer           = work->field_54E - 1;
            work->field_54E = timer;
            if (timer == 0) {
                Actor01600_Fn06EA4(arg1);
            }
            return;
        default:
            return;
    }

    Actor01600_Fn03D48(arg1);
    coords->flg = 0;
    Gp_UpdateCoord(coords);
    count           = work->field_550 + 1;
    work->field_550 = count;
    if (count < 5 && gGameSession->viewDirty != 1) {
        return;
    }
    work->field_550 = 0;

    attach = &arg1->extra.tmd->coords[1];
    update_actor_color(arg0, attach);
}

/// Measures the player against the actor: returns the yaw the actor would have
/// to turn through to face the player, wrapped into -0x800..0x800, and writes
/// the horizontal distance to `distance`.
///
/// The scratch block is addressed two ways on purpose. `head` is the scratchpad
/// top the allocation moved down from, and the direction vector and the
/// transposed rotation are written at negative offsets from it, which is what
/// keeps the head in a register of its own; only `delta`, at the foot of the
/// block, goes through the allocated pointer. The first difference is computed
/// into `dx` so that the scratchpad pointer is stored between it and its own
/// store, as the two stores are emitted in that order.
///
/// The `sw` is written out because the second `scratch` operand is what gives
/// the pointer the reference count that ranks it above `other` in the register
/// allocator; the instruction it emits is the store the C expression would have
/// emitted anyway.
s32 Actor01600_Fn045A8(Task* arg0, s32* distance)
{
    SVECTOR               local;
    Task**                slot;
    Task**                slots;
    GpCoord*              coord;
    GpCoord*              other;
    s32                   angle;
    s32                   dx;
    s32                   x;
    s32                   z;
    Actor01600AimScratch* head;
    Actor01600AimScratch* allocated;
    SVECTOR*              vec;
    MATRIX*               matrix;
    Actor01600AimScratch* scratch;

    slots              = Gp_ActorSlots;
    slot               = &slots[Actor01600_Fn052C4(arg0) & 0xFF];
    head               = SCRATCH_HEAD(Actor01600AimScratch);
    coord              = arg0->extra.tmd->coords;
    other              = (*slot)->extra.tmd->coords;
    dx                 = (u16)other->workm.t[0] - (u16)coord->workm.t[0];
    allocated          = (SCRATCH_HEAD(Actor01600AimScratch) = head - 1);
    (head - 1)->dir.vx = (s16)dx;
    vec                = &(head - 1)->dir;
    vec->vy            = (s16)((u16)other->workm.t[1] - (u16)coord->workm.t[1]);
    scratch            = allocated;
    vec->vz            = (s16)((u16)other->workm.t[2] - (u16)coord->workm.t[2]);
    matrix             = &(head - 1)->mat;
    TransposeMatrix(&coord->workm, matrix);
    local = *vec;
    gte_SetRotMatrix(matrix);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    gte_rtv0();
    gte_stsv(vec);
    angle = ratan2((head - 1)->dir.vx, vec->vz);
    if (angle >= 0x801) {
        angle -= 0x1000;
    } else if (angle < -0x800) {
        angle += 0x1000;
    }
    x = other->coord.t[0] - coord->coord.t[0];
    __asm__("sw\t%1, %0" : "=m"(scratch->delta.vx) : "r"(x), "r"(scratch));
    scratch->delta.vy = other->coord.t[1] - coord->coord.t[1];
    z                 = other->coord.t[2] - coord->coord.t[2];
    scratch->delta.vz = z;
    *distance         = SquareRoot0((x * x) + (z * z));
    SCRATCH_POP(Actor01600AimScratch);
    return angle;
}

s32 Actor01600_Fn047A0(Task* arg0)
{
    SVECTOR3        delta;
    s32             distance;
    Actor01600Work* work;
    GpCoord*        coord;
    Task*           task;
    s16             angle;
    s16             heading;
    s32             difference;
    GpCoord*        other;

    work  = arg0->work;
    task  = work->field_4D4;
    other = task->extra.tmd->coords;
    coord = arg0->extra.tmd->coords;
    if (((GameActor*)Gp_ActorSlots[work->field_53E]->work)->field_954 != 2) {
        if (Actor01600_D12870 != 1) {
            difference = Actor01600_Fn045A8(arg0, &distance);
            if (difference < 0) {
                difference = -difference;
            }
            if (difference < 0x401) {
                if (distance < 0x3E8) {
                    Actor01600_D12878.field_14 = 5;
                    if (work->field_53E != 0) {
                        Actor01600_D12878.field_4 = 1;
                    } else {
                        Actor01600_D12878.field_4 = 2;
                    }
                    if (Gp_DispatchMsg(task, 0x3F8, (s32)&Actor01600_D12878, 0) == 0) {
                        other->flg = 0;
                        delta.vx   = coord->coord.t[0] - other->coord.t[0];
                        delta.vy   = 0;
                        delta.vz   = coord->coord.t[2] - other->coord.t[2];
                        angle      = ratan2(delta.vx, delta.vz);
                        heading    = angle;
                        if (angle >= 0x801) {
                            heading = angle - 0x1000;
                        } else if (angle < -0x800) {
                            heading = angle + 0x1000;
                        }
                        Actor01600_D12890.rot.vx = 0;
                        Actor01600_D12890.rot.vy = heading;
                        Actor01600_D12890.rot.vz = 0;
                        Actor01600_D12890.pos.vx = (s32)other->coord.t[0];
                        Actor01600_D12890.pos.vy = (s32)other->coord.t[1];
                        Actor01600_D12890.pos.vz = (s32)other->coord.t[2];
                        Gp_DispatchMsg(task, 0x3E9, (s32)&Actor01600_D12890, 0);
                        Actor01600_D12870 = 1;
                        return 1;
                    }
                    return 0;
                }
                return 0;
            }
            return 0;
        }
        return 0;
    }
    return 0;
}

s32 Actor01600_Fn04974(Task* actor, s32 angle, s32 distance, s32 flags)
{
    Actor01600Work* work;
    GpCoord*        coord;
    GpCoord*        other;
    s32             difference;
    s32             angleAbs;
    s32             done;
    s32             otherY;
    s32             tmp;

    work  = actor->work;
    coord = actor->extra.tmd->coords;
    other = (*Gp_ActorSlots)->extra.tmd->coords;
    if (((GameActor*)Gp_ActorSlots[flags]->work)->field_954 != 2) {
        if (Actor01600_D12870 == 0) {
            otherY     = other->coord.t[1];
            tmp        = coord->coord.t[1];
            difference = otherY - tmp;
            if (difference < 0) {
                difference = -difference;
            }
            if (difference < 0x191) {
                if (distance < 0x3E9) {
                    tmp      = angle >= 0;
                    angleAbs = tmp ? angle : -angle;
                    if (angleAbs < 0x101) {
                        work->field_42A |= 0xC000;
                        Actor01600_Fn04EB0(actor);
                        if (work->field_4EA >= 2) {
                            work->field_506  = 0x19;
                            work->field_51A  = 0;
                            work->field_4FE  = 0;
                            work->field_4EA  = 0;
                            work->field_514  = 1;
                            work->field_2BA &= 0x3FFF;
                            done             = 1;
                        } else {
                            done = 0;
                        }
                        if ((u8)done) {
                            return 1;
                        }
                        work->field_516 = 7;
                        work->field_506 = 0x1C;
                        work->field_52C = 0;
                        work->field_53E = flags;
                        work->field_4D4 = Gp_ActorSlots[flags];
                        return 1;
                    }
                    return 0;
                }
                return 0;
            }
            return 0;
        }
        return 0;
    }
    return 0;
}

void Actor01600_Fn04AD8(Task* arg0)
{
    GpEnemy*        ctx;
    Actor01600Work* work;
    GpCoord*        body;
    s16             state;
    u16             count;
    u16             count2;

    work  = arg0->work;
    ctx   = arg0->spawnArg2;
    state = work->field_4F2;
    body  = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_24C.coord.t[0] = body->coord.t[0];
            work->field_24C.coord.t[2] = body->coord.t[2];
            ctx->coord                 = body;
            break;
        case 1:
            work->field_24C.coord.t[0] = body->coord.t[0];
            work->field_24C.coord.t[2] = body->coord.t[2];
            ctx->coord                 = &work->field_24C;
            goto advance;
        case 2:
            count           = work->field_4F4 + 1;
            work->field_4F4 = count;
            if ((s16)count < 8) {
                break;
            }
        advance:
            work->field_4F4 = 0U;
            work->field_4F2 = (u16)work->field_4F2 + 1;
            break;
        case 3:
            work->field_4F6 = (s16)((body->coord.t[0] - work->field_24C.coord.t[0]) / 5);
            work->field_4F8 = (s16)((body->coord.t[2] - work->field_24C.coord.t[2]) / 5);
            work->field_4F2 = (u16)work->field_4F2 + 1;
            break;
        case 4:
            work->field_24C.coord.t[0] += work->field_4F6;
            work->field_24C.coord.t[2] += work->field_4F8;
            count2                      = work->field_4F4 + 1;
            work->field_4F4             = count2;
            if ((s16)count2 >= 5) {
                work->field_4F4 = 0U;
                work->field_4F2 = 0;
            }
            break;
    }
    work->field_24C.coord.t[1] = body->coord.t[1];
    work->field_24C.flg        = 0;
    Gp_UpdateCoord(&work->field_24C);
}

/// Allocates the yaw scratch, updates `field_4EC` either by `angle` (clamped
/// into (-0x800, 0x801]) or, when `angle` is 0, by its own 0x71 / 0xA step -
/// returning 1 once the degree counter `field_4EE` passes a full turn - then
/// turns (0, 0, `distance`) by the resulting yaw into `field_42C` / `field_430`
/// and advances the swept-angle range `ranges[field_4E8]`.
s32 Actor01600_Fn04C64(Task* arg0, s32 distance, s32 angle)
{
    Actor01600YawScratch* allocated;
    Actor01600YawScratch* scratch;
    Actor01600Work*       work;
    MATRIX*               m;
    SVECTOR*              out;
    s16                   temp_v0_3;
    s16                   temp_v0_4;
    s16                   temp_v0_5;
    s16                   temp_v1_2;
    s16                   var_v0;
    s16                   var_v0_2;
    s32                   scaled;
    s32                   temp_a0;
    s32                   var_s4;

    var_s4                             = 0;
    allocated                          = SCRATCH_HEAD(Actor01600YawScratch) - 1;
    work                               = arg0->work;
    SCRATCH_HEAD(Actor01600YawScratch) = allocated;
    scratch                            = allocated;
    scratch->vec.vx                    = 0;
    scratch->vec.vy                    = 0;
    scratch->vec.vz                    = (s16)distance;
    if (angle == 0) {
        temp_v1_2       = (u16)work->field_4EE + 0xA;
        scaled          = temp_v1_2 << 0x10;
        var_s4          = scaled > 0x01670000;
        work->field_4EC = (u16)work->field_4EC + 0x71;
        work->field_4EE = temp_v1_2;
    } else {
        work->field_4EC = (s16)angle;
        if ((s16)angle >= 0x801) {
            var_v0 = angle - 0x1000;
            goto block_5;
        }
        if ((s16)angle >= -0x800) {
            goto block_6;
        }
        var_v0 = angle + 0x1000;
    block_5:
        work->field_4EC = var_v0;
    block_6:;
    }
    m                  = &scratch->mat;
    *(s32*)&m->m[0][0] = 0x1000;
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = 0x1000;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = 0x1000;
    func_8004BFF8(work->field_4EC, m);
    out = &scratch->out;
    gte_SetRotMatrix(m);
    gte_ldv0(&scratch->vec);
    gte_rtv0();
    gte_stsv(out);
    work->field_42C = (s16)scratch->out.vx;
    work->field_430 = (s16)scratch->out.vz;
    if ((u16)(work->field_444.key >> 16) != 0x10) {
        if (angle == 0) {
            temp_v0_3 = work->field_4E8;
            if (work->ranges[temp_v0_3].low == 0xFFFF) {
                work->ranges[temp_v0_3].low = (s32)work->field_4EC;
            } else {
                work->ranges[temp_v0_3].high = (s32)work->field_4EC;
                if (var_s4 == 1) {
                    var_v0_2 = (u16)work->field_4E8 + 1;
                    goto block_18;
                }
            }
        } else {
            var_s4 = 1;
        }
    } else if (angle == 0) {
        temp_v0_4 = work->field_4E8;
        temp_a0   = work->ranges[temp_v0_4].low;
        if (temp_a0 != 0xFFFF) {
            if (work->ranges[temp_v0_4].high == 0xFFFF) {
                work->ranges[temp_v0_4].high = temp_a0;
            }
            temp_v0_5       = (u16)work->field_4E8 + 1;
            work->field_4E8 = temp_v0_5;
            if (temp_v0_5 >= 7) {
                var_v0_2 = 7;
            block_18:
                work->field_4E8 = var_v0_2;
            }
        }
    }
    CLEAR_REC18_OCCUPIED(&work->field_444);
    SCRATCH_POP(Actor01600YawScratch);
    return var_s4;
}

u8 Actor01600_Fn04EB0(Task* arg0)
{
    SVECTOR         dir;
    s32             distance;
    Actor01600Work* work;
    s32             flags;
    s32             angle;
    s32             mag;
    s32             other;
    s32             span;
    s32             midpoint;
    s32             delta1;
    s32             best;
    s32             direction;
    s32             i;
    s32             count;
    s32             savedAngle;
    s32             callAngle;
    s32             high;
    s32             circle;

    work  = arg0->work;
    flags = 0;
    switch (work->field_4EA) {
        case 0:
            savedAngle = Actor01600_Fn045A8(arg0, &distance);
            TOUCH_REG(savedAngle);
            Actor01600_Fn04C64(arg0, distance, savedAngle);
            work->field_4E0 = savedAngle;
            work->field_4EA++;
            break;
        case 1:
            callAngle       = Actor01600_Fn045A8(arg0, &distance);
            work->field_4E0 = callAngle;
            if (Actor01600_Fn04C64(arg0, distance, callAngle) & 0xFF) {
                Gfx_MatrixCol2(&arg0->extra.tmd->coords->coord, &dir);
                ratan2(dir.vx, dir.vz);
                mag   = __builtin_abs(work->field_4E0);
                other = 0x1000 - mag;
                TOUCH_REG_USE(other, flags);
                SOFT_TOUCH_REG(other);
                SOFT_TOUCH_REG(other);
                flags = 0;
                if (other < mag)
                    angle = other;
                else {
                    flags = 0x80;
                    angle = mag;
                }
                work->field_4F0 = angle / 16;
                if (work->field_4F0 < 0x20)
                    work->field_4F0 = 0x20;
                return flags | 1;
            }
            work->field_4EA++;
            break;
        case 2:
            work->field_4EC = -0x71;
            work->field_4E8 = 0;
            work->field_4EE = -10;
            for (i = 0; i < 8; i++) {
                work->ranges[i].low  = 0xFFFF;
                work->ranges[i].high = 0xFFFF;
            }
            Actor01600_Fn04C64(arg0, 2000, 0);
            work->field_4EA++;
            break;
        case 3:
            if (Actor01600_Fn04C64(arg0, 2000, 0) & 0xFF) {
                if (!work->field_4E8)
                    return 0xFF;
                work->field_4EA++;
            } else
                return 0;
            break;
        case 4:
            angle = Actor01600_Fn045A8(arg0, &distance);
            if (angle < 0)
                angle += 0x1000;
            mag             = work->field_4E8;
            work->field_4E0 = 0;
            if (mag == 1) {
                mag   = work->ranges[0].high;
                other = work->ranges[0].low;
                span  = mag - other;
                if (span >= 0x400) {
                    delta1   = __builtin_abs(angle - mag);
                    midpoint = __builtin_abs(angle - other);
                    if (delta1 < midpoint)
                        work->field_4E0 = mag;
                    else
                        work->field_4E0 = other;
                } else
                    work->field_4E0 = other + span / 2;
            } else {
                best = 0xFFFF;
                i    = 0;
                if (mag > 0) {
                    circle = 0x1000;
                    count  = mag;
                    do {
                        high     = *(volatile s32*)&work->ranges[i].high;
                        mag      = *(volatile s32*)&work->ranges[i].low;
                        span     = high - mag;
                        midpoint = mag + span / 2;
                        if ((u32)(midpoint - 0x780) >= 0x101U) {
                            mag       = __builtin_abs(angle - midpoint);
                            other     = circle - mag;
                            direction = 0;
                            if (other < mag)
                                mag = other;
                            else
                                direction = 0x80;
                            if (mag < best) {
                                flags = direction;
                                best  = mag;
                                if (span >= 0x400) {
                                    other    = work->ranges[i].high;
                                    mag      = *(volatile s32*)&work->ranges[i].low;
                                    delta1   = __builtin_abs(angle - other);
                                    midpoint = __builtin_abs(angle - mag);
                                    if (delta1 < midpoint)
                                        midpoint = other;
                                    else
                                        midpoint = mag;
                                }
                                work->field_4E0 = midpoint;
                            }
                        }
                        i++;
                    } while (i < count);
                }
            }
            if (work->field_4E0 > 0x800)
                work->field_4E0 -= 0x1000;
            else if (work->field_4E0 < -0x800)
                work->field_4E0 += 0x1000;
            Gfx_MatrixCol2(&arg0->extra.tmd->coords->coord, &dir);
            ratan2(dir.vx, dir.vz);
            angle = __builtin_abs(work->field_4E0);
            if (angle > 0x800)
                angle = 0x1000 - angle;
            work->field_4F0 = angle / 16;
            if (work->field_4F0 < 0x20)
                work->field_4F0 = 0x20;
            return flags | 2;
    }
    return 0;
}

s32 Actor01600_Fn052C4(Task* arg0)
{
    GpCoordPos* coord;
    GpCoordPos* other;
    SVECTOR     d;
    s32         dist;

    coord = (GpCoordPos*)arg0->extra.tmd->coords;
    if (Gp_ActorSlots[0] != NULL) {
        other = (GpCoordPos*)Gp_ActorSlots[0]->extra.tmd->coords;
        d.vx  = other->x - coord->x;
        d.vy  = other->y - coord->y;
        d.vz  = other->z - coord->z;
        dist  = SquareRoot0((d.vx * d.vx) + (d.vz * d.vz));
        if (Gp_ActorSlots[1] != NULL) {
            other = (GpCoordPos*)Gp_ActorSlots[1]->extra.tmd->coords;
            d.vx  = other->x - coord->x;
            d.vy  = other->y - coord->y;
            d.vz  = other->z - coord->z;
            return SquareRoot0((d.vx * d.vx) + (d.vz * d.vz)) < dist;
        }
    }
    return 0;
}

void Actor01600_Fn05400(Task* arg0)
{
    TmdObject*      obj;
    TmdObject*      obj2;
    TmdObject*      obj3;
    TmdObject*      obj4;
    s32             kind;
    GpEnemy*        ctx;
    Actor01600Work* work;

    ctx  = arg0->spawnArg2;
    kind = ctx->place->mode;
    work = arg0->work;
    switch (kind) {
        case 0:
            Actor01600_Fn00480(arg0);
            Tmd_AllocBuffers(arg0->extra.tmd);
            obj             = arg0->extra.tmd;
            obj->flags     &= 0xFFFB;
            obj2            = arg0->extra.tmd;
            obj2->flags    &= 0xFF7F;
            work->field_52E = 1;
            return;
        case 3:
            work->field_53C         = 1;
            ctx->node.state.b.flags = 1;
            work->field_52E         = 0;
            work->field_530         = 1;
            work->field_4D8         = 0;
            work->field_4DA         = (u16)ctx->place->yaw;
            work->field_4DC         = 0;
            Actor01600_D12874      += 1;
            return;
        case 1:
            work->field_536 = kind;

        default:
            obj3                    = arg0->extra.tmd;
            obj3->flags            |= 0x80;
            obj4                    = arg0->extra.tmd;
            obj4->flags            |= 4;
            work->field_53C         = 1;
            ctx->node.state.b.flags = 1;
            work->field_532         = 1;
            work->field_52E         = 0;
            work->field_530         = 1;
            Actor01600_D12874      += 1;
            return;
    }
}
s32 Actor01600_Fn05558(Task* arg0)
{
    SVECTOR         rot;
    GpEnemy*        ctx;
    GpAreaPlace*    params;
    Actor01600Work* work;
    GpCoord*        rootCoord;
    GpCoord*        coord;
    TmdObject*      obj;
    TmdObject*      obj2;
    TmdObject*      obj3;
    TmdObject*      obj4;
    SVECTOR*        pos;
    SVECTOR*        tableA;
    SVECTOR*        tableB;
    SVECTOR*        pos2;
    s16             countdown;
    s16             countdown2;
    s16             countdown3;
    s32             event;
    s32             pan;
    s32             scriptArg;
    u16             kind;
    u8              mode;

    coord = arg0->extra.tmd->coords;
    ctx   = arg0->spawnArg2;
    work  = arg0->work;
    if ((u32)(gGameSession->at4.loc.stage - 2) < 2U) {
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_KEY(0, 255, 255, 0)) == GP_LOC_KEY(0, 34, 1, 0)) {
            if ((u16)work->field_4FE < 2U) {
                work->field_4FE = 4;
            }
        }
        if (((u32)(gGameSession->at4.loc.stage - 2) < 2U) && (gGameSession->at4.loc.area == 0x26) &&
            ((mode = gGameSession->at4.loc.room, (mode == 1)) || (mode == 3)) && ((u16)work->field_4FE < 2U)) {
            work->field_4FE = 4;
        }
    }
    if (((u8)ctx->place->variant & 0x80) && ((u16)work->field_4FE < 2U)) {
        work->field_4FE = 4;
    }
    if (work->field_52E != 0) {
        return 0;
    }

    params = ctx->place;
    kind   = params->mode;
    switch (kind) {
        case 1:
        case 2:
            Actor01600_Fn03A60(arg0);
            if ((u32)((u8)Gp_StateF0.field_1C - 1) >= 2U) {
                goto running;
            }
            scriptArg = (s8)(u8)Gp_StateF0.field_1C;
            if (scriptArg == 1) {
                event = (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4010000E;
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(event, pan, (s8)gpGetObjDepth(coord));
                countdown       = (u16)work->field_536 - 1;
                work->field_536 = countdown;
                if ((countdown << 0x10) != 0) {
                    goto running;
                }
                if ((u8)ctx->place->variant == 0) {
                    work->field_516 = 6;
                    work->field_506 = 0x1A;
                } else {
                    work->field_53C = (s16)scriptArg;
                    work->field_516 = 5;
                }
                work->field_532 = 1;
            } else if (scriptArg == 2) {
                if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(3, 15, 0, 0)) {
                    tableA            = Actor01600_D09F1C;
                    pos               = &Actor01600_D09F1C[(u16)ctx->placeKey >> 0xC];
                    coord->coord.t[0] = pos->vx;
                    coord->coord.t[1] = pos->vy;
                    coord->coord.t[2] = pos->vz;
                }
                if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(4, 4, 0, 0)) {
                    tableB            = Actor01600_D09F3C;
                    pos2              = &Actor01600_D09F3C[(u16)ctx->placeKey >> 0xC];
                    coord->coord.t[0] = pos2->vx;
                    coord->coord.t[1] = pos2->vy;
                    coord->coord.t[2] = pos2->vz;
                }
                Gp_ArmStateF0(1);
            }
            ctx->node.state.b.flags = 0;
            work->field_52E         = 1;
            Actor01600_Fn00480(arg0);
            if (Gp_StateF0.field_1C == 1) {
                work->collision.named.field_30A &= 0xBFFF;
            }
            Tmd_AllocBuffers(arg0->extra.tmd);
            obj             = arg0->extra.tmd;
            obj->flags     &= 0xFFFB;
            obj2            = arg0->extra.tmd;
            obj2->flags    &= 0xFF7F;
            work->field_4FE = 1;
            work->field_4FA = 0;
            work->field_508 = 0;
            work->field_50A = 0;
            work->field_52A = 0;
            return 0;
        case 3:
            if (Gp_StateF0.field_1C == kind) {
                if ((u8)params->variant == 1) {
                    Actor01600_Fn06FDC(arg0, 1);
                running:
                    return 1;
                }
                countdown2      = (u16)work->field_536 - 1;
                work->field_536 = countdown2;
                if ((countdown2 << 0x10) == 0) {
                    ctx->node.state.b.flags = 0;
                    Actor01600_Fn00480(arg0);
                    work->field_516 = 6;
                    work->field_52E = 1;
                    work->field_4FE = 1;
                    work->field_4FA = 0;
                    work->field_508 = 0;
                    work->field_50A = 0;
                    work->field_52A = 0;
                    work->field_506 = 0x1A;
                    return 0;
                }
                goto running;
            }
            if (arg0->extra.tmd->flags & 0x80) {
                goto running;
            }
            Actor01600_Fn03D48(arg0);
            Actor01600_Fn05F80(arg0);
            if (work->field_54A != 0) {
                update_actor_color(ctx, arg0->extra.tmd->coords + 1);
                work->field_54A = 0;
            }
            coord->flg = 0;
            goto running;
        default:
            if (Gp_StateF0.field_1C < (s32)ctx->place->mode) {
                goto running;
            }
            countdown3      = (u16)work->field_536 - 1;
            work->field_536 = countdown3;
            if ((countdown3 << 0x10) != 0) {
                goto running;
            }
            ctx->node.state.b.flags = 0;
            work->field_52E         = 1;
            Actor01600_Fn00480(arg0);
            Tmd_AllocBuffers(arg0->extra.tmd);
            obj3            = arg0->extra.tmd;
            obj3->flags    &= 0xFFFB;
            obj4            = arg0->extra.tmd;
            obj4->flags    &= 0xFF7F;
            work->field_516 = 6;
            work->field_4FE = 1;
            work->field_4FA = 0;
            work->field_508 = 0;
            work->field_50A = 0;
            work->field_52A = 0;
            work->field_544 = 1;
            work->field_506 = 0x1A;
            rootCoord       = arg0->extra.tmd->coords;
            memset(&rot, 0, 8);
            rot.vy = -0x400;
            RotMatrix(&rot, &rootCoord->coord);
            work->field_532 = 1;
            return 0;
    }
}

s32 Actor01600_Fn05B08(Task* arg0, s32 arg1, GpCmdArg* arg2)
{
    SVECTOR         rot;
    GpEnemy*        ctx;
    Actor01600Work* work;
    GpCoord*        coord;
    TmdObject*      obj;
    u32             variant;

    ctx     = arg0->spawnArg2;
    coord   = arg0->extra.tmd->coords;
    work    = arg0->work;
    variant = ctx->place->variant;

    switch (arg2->command) {
        case 1:
            work->field_54A = 1;
            if (variant == 2) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_50A = 0;
                work->field_508 = 0;
                work->field_506 = 3;
                work->field_546 = 9;
                work->field_524 = ((u32)Gp_LcgState >> 16) % 20;
            }
            if (variant == 4) {
                work->field_506 = 0x11;
                work->field_508 = 0;
            }
            if (variant == 3) {
                work->field_506   = 0x11;
                work->field_508   = 0;
                coord->coord.t[0] = 0x4021;
                coord->coord.t[1] = -0x190;
                coord->coord.t[2] = -0x6C0;
                rot.vx            = 0;
                rot.vy            = 0x600;
                rot.vz            = 0;
                RotMatrix(&rot, &coord->coord);
                goto setFlags;
            }
            break;
        case 2:
            work->field_54A = 1;
            if (variant == 1) {
                work->field_506 = 0x1A;
                work->field_508 = 0;
            }
            if (variant == 2) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_50A = 0;
                work->field_508 = 0;
                work->field_506 = 3;
                work->field_546 = 9;
                work->field_524 = ((u32)Gp_LcgState >> 16) % 20;
            }
            if (variant == 3) {
                obj = arg0->extra.tmd;
                do {
                    obj->flags &= 0xFFFB;
                    obj         = arg0->extra.tmd;
                } while (0);
                obj->flags &= 0xFF7F;
            }
            break;
        case 3:
            work->field_54A = 1;
            if ((u32)(variant - 1) < 4) {
                obj         = arg0->extra.tmd;
                obj->flags &= 0xFFFB;
                obj         = arg0->extra.tmd;
                obj->flags &= 0xFF7F;
            }
            if (variant == 1) {
                work->field_506 = 9;
                work->field_508 = 0;
            }
            if (variant == 2) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_50A = 0;
                work->field_508 = 0;
                work->field_506 = 3;
                work->field_546 = 9;
                work->field_524 = ((u32)Gp_LcgState >> 16) % 20;
            }
            break;
        case 4:
            work->field_54A = 1;
            if (variant == 1) {
                work->field_506 = 0xFF;
                work->field_508 = 0;
                work->field_50A = 0;
            }
            if (variant == 2) {
                work->field_506 = 3;
                work->field_524 = 0xA;
                work->field_50A = 0;
                work->field_508 = 0;
                work->field_546 = 7;
            }
            if (variant == 4) {
                work->field_50A = 0;
                work->field_524 = 7;
                work->field_546 = 7;
            }
            break;
        case 5:
            work->field_54A = 1;
            break;
        case 6:
            work->field_54A = 1;
            if (variant == 3) {
                Actor01600_Fn06FDC(arg0, 0);
            }
            if (variant == 2 || variant == 4) {
                obj         = arg0->extra.tmd;
                obj->flags &= 0xFFFB;
                obj         = arg0->extra.tmd;
                obj->flags &= 0xFF7F;
            }
            Gp_StateF0.field_1C = 3;
            break;
        case 7:
            work->field_54A = 1;
            if ((u32)(variant - 1) < 4) {
            setFlags:
                obj         = arg0->extra.tmd;
                obj->flags |= 0x80;
                obj         = arg0->extra.tmd;
                obj->flags |= 4;
            }
            break;
        case 8:
            Gp_ReleaseStateF0Add(arg0, 0x10);
            Gp_DestroyEnemy(ctx, arg0);
            Actor01600_D12874 -= 1;
            break;
        default:
            return -1;
    }
    return 0;
}

void Actor01600_Fn05F80(Task* arg0)
{
    SVECTOR         rot;
    GpCoord*        coord;
    Actor01600Work* work;
    GpCoord*        part1;
    u32             variant;
    s32             anim;
    u16             timer;
    s32             value;

    coord   = arg0->extra.tmd->coords;
    work    = arg0->work;
    variant = ((GpEnemy*)arg0->spawnArg2)->place->variant;
    part1   = &arg0->extra.tmd->coords[1];

    if (variant == 1) {
        switch (work->field_506) {
            case 0x1A:
                work->field_54A = variant;
                if ((u32)((u16)work->field_50A - 6) < 0xEU) {
                    coord->coord.t[1] -= 0xC8;
                }
                if ((u32)((u16)work->field_50A - 0x14) < 0xFU) {
                    value             = coord->coord.t[1] + 0x96;
                    coord->coord.t[1] = value;
                    if (value >= -0x497) {
                        coord->coord.t[1] = -0x498;
                    }
                }
                if ((u32)((u16)work->field_50A - 0xB) < 0x14U) {
                    value             = coord->coord.t[0] + ((coord->coord.m[0][2] * 0x4B) >> 0xB);
                    coord->coord.t[0] = value;
                    if (value < 0x3B23) {
                        coord->coord.t[0] = 0x3B23;
                    }
                    value             = coord->coord.t[2] + ((coord->coord.m[2][2] * 0x4B) >> 0xB);
                    coord->coord.t[2] = value;
                    if (value >= -0xD11) {
                        coord->coord.t[2] = -0xD12;
                    }
                }
                if ((s16)work->field_50A >= 0x31) {
                    work->field_506 = 8;
                    work->field_508 = 0;
                    work->field_50A = 0;
                }
                break;
            case 8:
                if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                    work->field_4D8  = 0;
                    work->field_4DC  = 0;
                    work->field_4DA += 0x32;
                    RotMatrix((SVECTOR*)&work->field_4D8, &coord->coord);
                }
                if ((s16)work->field_50A >= 0x1C) {
                    work->field_506 = 9;
                    work->field_508 = 0;
                    work->field_50A = 0;
                }
                break;
            case 0xFF:
                anim = work->field_50A;
                if (anim == 2) {
                    work->field_404 = part1;
                    work->field_408 = 0x100;
                    work->field_40A = anim;
                    func_800FDB18(1, part1, 0, (GpEffArg*)&work->field_404);
                    Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[1], 0, NULL);
                }
                if ((u32)((u16)work->field_50A - 0xF) < 8U) {
                    coord->coord.t[1] += 0x80;
                }
                if ((s16)work->field_50A >= 3) {
                    coord->coord.t[0] += 0x10E;
                    coord->coord.t[2] -= 0xC8;
                    coord              = &arg0->extra.tmd->coords[6];
                    rot.vx             = -0x400;
                    rot.vy             = 0;
                    rot.vz             = 0;
                    RotMatrix(&rot, &coord->coord);
                    coord  = &arg0->extra.tmd->coords[8];
                    rot.vx = -0x400;
                    rot.vy = 0;
                    rot.vz = 0;
                    RotMatrix(&rot, &coord->coord);
                    if ((s16)work->field_50A >= 3) {
                        work->field_4D8  = 0x384;
                        work->field_4DA += 0x96;
                        timer            = work->field_4DC + 0x64;
                        work->field_4DC  = timer;
                        if ((s16)timer >= 0x384) {
                            work->field_4DC = 0x384;
                        }
                        coord = &arg0->extra.tmd->coords[1];
                        RotMatrix((SVECTOR*)&work->field_4D8, &coord->coord);
                    }
                }
                if ((s16)work->field_50A >= 0x12) {
                    Actor01600_Fn06FDC(arg0, 0);
                }
                work->field_50A += 1;
                break;
        }
    }
    if (variant == 2) {
        timer           = work->field_524 - 1;
        work->field_524 = timer;
        if ((s16)timer < 0) {
            anim            = work->field_546;
            work->field_524 = 1;
            switch (anim) {
                case 7:
                    work->field_506 = anim;
                    if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                        work->field_4D8  = 0;
                        work->field_4DC  = 0;
                        work->field_4DA += 0x32;
                        RotMatrix((SVECTOR*)&work->field_4D8, &coord->coord);
                    }
                    if ((s16)work->field_50A >= 0x1C) {
                        work->field_546 = 9;
                        work->field_506 = 9;
                        work->field_508 = 0;
                        work->field_50A = 0;
                    }
                    break;
                case 9:
                    work->field_506 = anim;
                    if ((s16)work->field_50A >= 0x32) {
                        work->field_50A = 0;
                        work->field_508 = 0;
                        work->field_506 = 3;
                        work->field_546 = 0;
                    }
                    break;
            }
        }
    }
    if (variant == 4) {
        timer           = work->field_524 - 1;
        work->field_524 = timer;
        if ((s16)timer < 0) {
            anim            = work->field_546;
            work->field_524 = 1;
            if (anim == 7) {
                work->field_506 = anim;
                if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                    work->field_4D8  = 0;
                    work->field_4DC  = 0;
                    work->field_4DA -= 0xFA;
                    RotMatrix((SVECTOR*)&work->field_4D8, &coord->coord);
                }
                if ((s16)work->field_50A >= 0x1C) {
                    work->field_546 = 9;
                    work->field_506 = 9;
                    work->field_508 = 0;
                    work->field_50A = 0;
                }
            }
        }
    }
}

void Actor01600_Fn0646C(Task* arg0)
{
    GpEffWork* effect;
    TmdObject* obj;
    TmdObject* obj2;
    s32        randomState;
    s32        choice;

    if (((Actor01600Work*)arg0->work)->field_540 != 0) {
        D_800626EC[5].arg.model = &Actor01600_D0973C;
        effect                  = Gp_SpawnEff(0x80005, arg0->extra.tmd->coords + 1, 0, NULL);
        if (effect != NULL) {
            Actor01600_Fn070AC(effect->task, arg0);
        }
        D_800626EC[5].arg.model = &Actor01600_D09EE0;
        effect                  = Gp_SpawnEff(0x80005, arg0->extra.tmd->coords + 2, 0, NULL);
        if (effect != NULL) {
            Actor01600_Fn070AC(effect->task, arg0);
        }
        D_800626EC[5].arg.model = &Actor01600_D09EE0;
        effect                  = Gp_SpawnEff(0x80005, arg0->extra.tmd->coords + 3, 0, NULL);
        if (effect != NULL) {
            Actor01600_Fn070AC(effect->task, arg0);
        }
        Gp_SpawnEff(0x60030, arg0->extra.tmd->coords + 1, 0x300, &Actor01600_D12868);
        return;
    }
    randomState = (Gp_LcgState * 5) + 0x71357911;
    Gp_LcgState = randomState;
    choice      = ((u32)randomState >> 0x10) & 3;
    switch (choice) {
        case 0:
        case 1:
            D_800626EC[5].arg.model = &Actor01600_D0973C;
            effect                  = Gp_SpawnEff(0x80005, arg0->extra.tmd->coords + 1, 0, NULL);
            if (effect != NULL) {
                Actor01600_Fn070AC(effect->task, arg0);
            }
            break;
        case 2:
            D_800626EC[5].arg.model = &Actor01600_D09EE0;
            effect                  = Gp_SpawnEff(0x80005, arg0->extra.tmd->coords + 2, 0, NULL);
            if (effect != NULL) {
                Actor01600_Fn070AC(effect->task, arg0);
            }
            break;
        case 3:
            D_800626EC[5].arg.model = &Actor01600_D09CFC;
            effect                  = Gp_SpawnEff(0x80005, arg0->extra.tmd->coords + 6, 0, NULL);
            if (effect != NULL) {
                Actor01600_Fn070AC(effect->task, arg0);
            }
            break;
    }
    Gp_SpawnEff(0x60030, arg0->extra.tmd->coords + 1, 0x50, &Actor01600_D12868);
    obj          = arg0->extra.tmd;
    obj->flags  |= 0x80;
    obj2         = arg0->extra.tmd;
    obj2->flags |= 4;
}

void Actor01600_Fn066E8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor01600_D00004;
    sp.funcs[arg0->state]((GpEnemy*)((GpEnemy*)arg0->spawnArg2), arg0);
}

/// Snapshots the attachment coordinate's translation into the work block's
/// `field_4BC`/`field_4C0`/`field_4C4`, then steps that coordinate along the
/// model's own facing: `field_50E` units of the coordinate's column 2 added to
/// its X/Z translation. Vertical follow-up only runs while `field_530` is
/// clear: with `field_528` set the frame counter `field_51E` is added to
/// `t[1]`, otherwise the coordinate is snapped down by 0x80.
void Actor01600_Fn06744(Task* arg0)
{
    Actor01600Work* work;
    GpCoord*        coord;

    coord              = arg0->extra.tmd->coords;
    work               = arg0->work;
    work->field_4BC    = coord->coord.t[0];
    work->field_4C0    = coord->coord.t[1];
    work->field_4C4    = coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_50E) >> 0xC;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_50E) >> 0xC;
    if (work->field_530 == 0) {
        if (work->field_528 != 0) {
            coord->coord.t[1] += work->field_51E;
            return;
        }
        coord->coord.t[1] += 0x80;
    }
}

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters.
void Actor01600_Fn06810(GpEnemy* arg0, Task* arg1)
{
    GpCoord* coord;
    void**   scratch;
    u8*      head;
    VECTOR*  block;

    coord                          = &arg1->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Squashes the actor's attachment coordinate: the work block's `field_49C`
/// rotation is copied into the coordinate, an identity is splatted into a
/// 0x30-byte scratch block and scaled per axis by 1.0 / the decaying
/// `field_518` / 1.0, and the product replaces the coordinate's rotation.
/// `flg` is cleared so its own work matrix is rebuilt from `coord` next frame.
void Actor01600_Fn06880(Task* arg0)
{
    GpCoord*           coord;
    u8*                head;
    ActorScaleScratch* scratch;
    Actor01600Work*    work;

    head               = SCRATCH_HEAD(u8);
    work               = arg0->work;
    scratch            = (ActorScaleScratch*)(head - 0x30);
    SCRATCH_HEAD(void) = scratch;
    coord              = arg0->extra.tmd->coords;
    if (work->field_518 >= 0x201) {
        work->field_518 = (u16)work->field_518 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_518;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_49C;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg = 0;
    SCRATCH_POP_BYTES(0x30);
}

/// Steps the attachment coordinate `distance` units along the model's facing:
/// `Gfx_MatrixCol2` reads that coordinate's column into `dir`, `ratan2` turns it
/// into a yaw, `func_8004BFF8` builds the rotation for the yaw and
/// `ApplyMatrixLV` rotates the step vector `(distance, 0, 0)` by it before the
/// result is added to `coord.t`.
void Actor01600_Fn06974(Task* actor, s32 distance)
{
    OverlayMat*            mat;
    Actor01600StepScratch* work;
    GpCoord*               coord;
    Actor01600StepScratch* head;
    void**                 scratch;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, Actor01600StepScratch);
    coord                          = actor->extra.tmd->coords;
    work                           = head - 1;
    work->move.vx                  = (s16)distance;
    work->move.vy                  = 0;
    work->move.vz                  = 0;
    SCRATCH_HEAD_AT(scratch, void) = work;
    Gfx_MatrixCol2(&actor->extra.tmd->coords->coord, &(head - 1)->dir);
    mat                = &(head - 1)->mat;
    work->yaw          = ratan2(work->dir.vx, work->dir.vz);
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    func_8004BFF8(work->yaw, &mat->mat);
    ApplyMatrixLV(&mat->mat, &work->move, &work->move);
    coord->coord.t[0] += work->move.vx;
    coord->coord.t[1] += work->move.vy;
    coord->coord.t[2] += work->move.vz;
    SCRATCH_POP_AT(scratch, Actor01600StepScratch);
}

void Actor01600_Fn06A84(Task* arg0)
{
    Actor01600Work* work;
    GpCoord*        coord;
    MATRIX*         scratch;
    u8*             head;
    s16             value;

    head                 = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(MATRIX) = (MATRIX*)(head - 0x20);
    scratch              = (MATRIX*)(head - 0x20);
    work                 = arg0->work;
    coord                = arg0->extra.tmd->coords;
    RotMatrix((SVECTOR*)&work->field_4CC, scratch);
    gte_SetRotMatrix(&coord[1].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[1].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[1].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[1].coord.m[0][2]);
    value = work->field_4CC;
    if (value != 0) {
        if (value < 0x21) {
            work->field_4CC = 0;
            work->field_522 = 0;
        } else {
            work->field_4CC = (u16)work->field_4CC - 0x20;
        }
    }
    SCRATCH_POP_BYTES(0x20);
}

s32 Actor01600_Fn06C1C(Task* arg0)
{
    Actor01600Work* work;

    work            = arg0->work;
    work->field_42A = (u16)(work->field_42A | 0xC000);
    Actor01600_Fn04EB0(arg0);
    if (work->field_4EA >= 2) {
        work->field_506 = 0x19;
        work->field_51A = 0;
        work->field_4FE = 0;
        work->field_4EA = 0;
        work->field_514 = 1;
        work->field_2BA = (u16)(work->field_2BA & 0x3FFF);
        return 1;
    }
    return 0;
}

/// Column 2 of the attachment matrix goes to `dir`; the trailing `SVECTOR`
/// is never read but owns the second half of the stack local block.
s32 Actor01600_Fn06C94(Task* arg0, s32 arg1, s32 unusedDistance)
{
    SVECTOR         dir;
    SVECTOR         unused;
    Actor01600Work* work;
    s32             ang;
    s32             half;
    s32             res;

    work = arg0->work;
    if (ABS(arg1) < 0x301) {
        return 0;
    }
    work->field_4E0 = arg1;
    Gfx_MatrixCol2(&arg0->extra.tmd->coords->coord, &dir);
    ratan2(dir.vx, dir.vz);
    ang  = ABS(work->field_4E0);
    half = 0x1000 - ang;
    if (half < ang) {
        work->field_516 = 3;
        res             = half;
    } else {
        work->field_516 = 2;
        res             = ang;
    }
    work->field_4F0 = res / 16;
    if (work->field_4F0 < 0x20) {
        work->field_4F0 = 0x20;
    }
    work->field_50A = 0;
    work->field_510 = 5;
    return 1;
}

s32 Actor01600_Fn06D74(Task* arg0, s32 arg1, s32 arg2)
{
    Actor01600Work* work;
    s32             scaledState;
    s32             handled;
    s32             angle;
    u32             state;

    work = arg0->work;
    if (arg2 < 0x7D1) {
        angle = (arg1 >= 0 ? arg1 : -arg1);
        if (angle < 0x201) {
            work->field_42A |= 0xC000;
            Actor01600_Fn04EB0(arg0);
            handled = 0;
            if (work->field_4EA >= 2) {
                work->field_506  = 0x19;
                work->field_51A  = 0;
                work->field_4FE  = 0;
                work->field_4EA  = 0;
                work->field_514  = 1;
                work->field_2BA &= 0x3FFF;
                handled          = 1;
            }
            if ((u8)handled) {
                return 1;
            } else {
                scaledState     = Gp_LcgState * 5;
                state           = scaledState + 0x71357911;
                work->field_4FA = 0;
                work->field_508 = 0;
                work->field_52A = 0;
                Gp_LcgState     = (s32)state;
                if ((u32)(((state >> 16) % 100) & 0xFFFF) < 0x14U) {
                    work->field_516 = 6;
                    work->field_506 = 0x1A;
                } else {
                    work->field_516 = 5;
                }
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

/// Tears the actor down: flags its context node dead (`field_4` = 1), clears
/// `field_54`, unlinks the node from its list and releases the four display
/// objects held in the work block, then hands the task to `Gp_EnemyTaskExit`.
void Actor01600_Fn06EA4(Task* arg0)
{
    GpEnemy*        ctx;
    Actor01600Work* work;

    ctx  = arg0->spawnArg2;
    work = arg0->work;

    ctx->node.state.b.flags = 1;
    ctx->recs               = 0;
    Gp_UnlinkNode(&ctx->node);
    Gp_UnlinkObj((GpObj*)work->field_40C);
    Gp_UnlinkObj((GpObj*)work->field_29C);
    Gp_UnlinkObj((GpObj*)work->collision.field_2EC);
    Gp_UnlinkObj((GpObj*)work->field_3CC);
    Gp_EnemyTaskExit(arg0);
}

void Actor01600_Fn06F10(Task* arg0)
{
    Actor01600Work* work;

    work = arg0->work;
    if (work->field_534 != 0) {
        Gp_DispatchMsg(work->field_4D4, 0x3F1, 0, 0);
        Actor01600_D127DC = 0;
        work->field_534   = 0;
        Actor01600_D12870 = 0;
        work->field_50E   = 0;
        work->field_516   = 0;
        work->field_50A   = 0;
        work->field_526   = 0;
    }
}

/// Walks the sibling ring of task slot 4's children and reports whether any of
/// them has already been flagged `0x80` in its `field_2C` object. Returns 0xFF
/// when the slot has no children at all, 1 on the first flagged sibling and 0
/// when the whole ring is clean. The task argument is unused.
u8 Actor01600_Fn06F78(Task* arg0)
{
    Task* head;
    Task* iter;

    head = gameGetPtrSlot(4)->firstChild;
    if (head == NULL) {
        return 0xFF;
    }
    iter = head;
    do {
        if (iter->extra.tmd->flags & 0x80) {
            return 1;
        }
        iter = iter->nextSibling;
    } while (iter != head);
    return 0;
}

void Actor01600_Fn06FDC(Task* arg0, s32 arg1)
{
    Actor01600Work* work;
    GpEnemy*        ctx;
    TmdObject*      obj;
    TmdObject*      obj2;

    obj         = arg0->extra.tmd;
    ctx         = arg0->spawnArg2;
    work        = arg0->work;
    obj->flags  = (u16)(obj->flags | 0x80);
    obj2        = arg0->extra.tmd;
    obj2->flags = (u16)(obj2->flags | 4);
    ctx->recs   = 0;
    Gp_UnlinkNode(&ctx->node);
    if (!(arg1 & 0xFF)) {
        Gp_UnlinkObj((GpObj*)work->field_40C);
        Gp_UnlinkObj((GpObj*)work->field_29C);
        Gp_UnlinkObj((GpObj*)work->collision.field_2EC);
        Gp_UnlinkObj((GpObj*)work->field_3CC);
    }
    Gp_SetLightMode(ctx, 1);
    Gp_ReleaseStateF0Add(arg0, 0x10);
    Gp_DestroyEnemy(ctx, arg0);
    Actor01600_D12874 -= 1;
}

/// Copies the source actor's texture page and CLUT row (`field_24` /
/// `field_25`) onto this actor's model object, then re-runs the model stream
/// twice so the new page/CLUT is baked into both of the object's primitive
/// buffers. Objects without an aux buffer (`field_18` NULL) have nothing to
/// rebuild and are left alone.
void Actor01600_Fn070AC(Task* arg0, Task* arg1)
{
    TmdObject* src;
    TmdObject* dst;

    src        = arg1->extra.tmd;
    dst        = arg0->extra.tmd;
    dst->tpage = src->tpage;
    dst->clut  = src->clut;
    if (dst->buffer != NULL) {
        tmdProcessStream(dst);
        tmdProcessStream(dst);
    }
}

s32 Actor01600_Fn07100(Task* arg0)
{
    ((Actor01600Work*)arg0->work)->field_554 = 6;
    return 0;
}
