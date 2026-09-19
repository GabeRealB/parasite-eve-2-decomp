#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/abs.h>

#include "actors/actor_401000.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/wipsys.h"

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// `Actor401300_MoveForward`: the plain forward-step helper, the shape the
/// state-8 body `func_actor_401000_801388F4` inlines. The `Nonzero` variant
/// below is the same block with an `amount != 0` test and a `gteVec` copy
/// wrapped around it.
static __inline__ void Actor401000_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12_real();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// `Actor401300_MoveForwardNonzero` and `Actor00100_MoveForwardNonzero`, down
/// to the `head[-1].vx` read-back; the step lands in `coord` directly rather
/// than being reported back through the caller's local.
static __inline__ void Actor401000_MoveForwardNonzero(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        gteVec                     = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12_real();
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801323EC);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132590);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132824);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132A84);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132BB0);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132EF0);

/// Points the model's light and color matrices at the work block's copies.
static __inline__ void Actor401000_BindMatrices(Actor401000* actor)
{
    Actor401000Work* work;
    TmdObject*       obj;

    work          = actor->field_1C;
    obj           = actor->field_2C;
    obj->lightMtx = &work->field_B88;
    obj->colorMtx = &work->field_BA8;
}

/// Rebuilds the root coordinate's scaled Y rotation and drops both obstacle
/// tables while the rotation scratch block is still held.
static __inline__ void Actor401000_InitPose(GsCOORDINATE2* coord, Actor401000Work* work)
{
    void*                  scratch_base;
    u8*                    head;
    u8*                    tail;
    Actor401000RotScratch* blk;
    GpRec18*               rec;
    s16                    ang;
    u16                    m22;

    scratch_base                             = PSX_SCRATCH;
    head                                     = scratch_base;
    head                                     = *(u8**)(head + 0x3FC);
    blk                                      = (Actor401000RotScratch*)(head - 0x34);
    *(Actor401000RotScratch**)G_SCRATCH_HEAD = blk;
    ang                                      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle                               = ang;
    Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
    blk->scale.vz = 0x1194;
    blk->scale.vy = 0x1194;
    blk->scale.vx = 0x1194;
    ScaleMatrix(&blk->m, &blk->scale);
    coord->coord.m[0][0] = *(u16*)&((Actor401000RotScratch*)(head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
    tail       = *(u8**)(tail + 0x3FC);
    m22        = *(u16*)&blk->m.m[2][2];
    rec        = (GpRec18*)work->field_A30;
    coord->flg = 0;
    tail       = tail + 0x34;
    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
    coord->coord.m[2][2] = m22;
    work->field_C7C      = 0;
    Gp_ClearRec18Occupied(rec);
    Gp_ClearRec18Occupied((GpRec18*)work->field_8F0);
}

/// Enemy init: allocates the 0xC80-byte work block, binds the model's light
/// and colour matrices to its copies, seeds both animation contexts and the
/// three `GpObj` nodes, then picks the opening clip from the low bits of
/// `GpEnemy::placeKey` and the `field_C10` parameter run from the spawn flags.
/// The tail rebuilds the root coordinate through `Actor401000_InitPose`.
void func_actor_401000_80133274(GpEnemy* enemy, Actor401000* actor)
{
    SVECTOR          dir;
    VECTOR           pos;
    SVECTOR*         v;
    TmdObject*       obj;
    GsCOORDINATE2*   root;
    Actor401000Work* work;
    GpObj*           body;
    GpObj*           head;
    u16              kind;
    s16              clip;
    s32              variant;

    root            = actor->field_2C->coords;
    obj             = actor->field_2C;
    work            = memCalloc(0xC80, 0);
    actor->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)actor);
        return;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    ((Task*)actor)->exitCallback = func_actor_401000_8013DA78;
    Actor401000_BindMatrices(actor);
    enemy->field_4    = &actor->field_2C->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->field_2C->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags    = 1;
    enemy->reactionFlags = 0;
    enemy->hp            = (s16)D_actor_401000_8013E09C.hpMax;
    enemy->param         = &D_actor_401000_8013E09C;
    enemy->recs          = (GpRec18*)&work->field_8F0;
    func_800B3F84(&((Actor401000AnimWork*)work)->anim, &D_actor_401000_80154E48, obj,
                  ((Actor401000AnimWork*)work)->pad_328, ((Actor401000AnimWork*)work)->slots);
    func_800B3F84(&((Actor401000AnimWork*)work)->blendAnim, &D_actor_401000_80154E48,
                  obj, ((Actor401000AnimWork*)work)->pad_764,
                  ((Actor401000AnimWork*)work)->blendSlots);
    work->field_898 = 2;
    work->field_89E = 2;
    work->field_89A = 0;
    work->field_8B0 = 0;
    work->field_8AE = 0;
    work->field_8A4 = 0x10;
    work->field_8A2 = 0x10;
    kind            = ((u16)enemy->placeKey >> 12) % 5;
    switch (kind) {
        case 0:
            clip = 0x11;
            break;
        case 1:
            clip = 0xF;
            break;
        case 2:
            clip = 0x10;
            break;
        case 3:
            clip = 0x12;
            break;
        case 4:
        default:
            clip = 0xE;
            break;
    }
    work->field_8A4 = clip;
    SCHED_BARRIER();
    func_actor_401000_80132EF0(actor);

    work->field_A10.ctx.recs = (GpRec18*)&work->field_A30;
    work->field_A10.coord    = root;
    work->field_A10.pos.vx   = 0;
    work->field_A10.pos.vy   = -0xAC;
    work->field_A10.pos.vz   = 0;
    work->field_A10.key      = 0x30000;
    work->field_A10.radius   = 0x12C;
    work->field_A10.flags    = 1;
    Gp_LinkObj(2, &work->field_A10);
    work->field_BE8        = 0;
    work->field_A10.flags |= 0x4000;
    Gp_InitRec18Table(work->field_A10.ctx.recs, 0xC, 0);

    body           = &work->field_8D0;
    body->coord    = &actor->field_2C->coords[2];
    body->ctx.recs = (GpRec18*)&work->field_8F0;
    body->pos.vx   = 0;
    body->pos.vy   = 0;
    body->pos.vz   = 0;
    body->key      = 0x3000A;
    body->radius   = 0x1AE;
    body->flags    = 1;
    Gp_LinkObj(2, body);
    body->flags |= 0x8000;
    Gp_InitRec18Table(body->ctx.recs, 0xC, 0);
    work->field_8D0.key = 0x30000;

    dir.vx         = 0;
    dir.vy         = 0;
    dir.vz         = 0;
    head           = &work->field_B50;
    head->coord    = &actor->field_2C->coords[6];
    head->ctx.recs = &work->field_B70;
    v              = &dir;
    head->pos.vx   = v->vx;
    head->pos.vy   = v->vy;
    head->pos.vz   = v->vz;
    head->radius   = 0x180;
    head->flags    = 1;
    Gp_LinkObj(3, head);
    Gp_InitRec18Table(head->ctx.recs, 1, 0);

    work->field_14     = 0;
    work->field_C[0].x = actor->field_2C->coords->coord.t[0];
    work->field_C[0].z = actor->field_2C->coords->coord.t[2];
    Gfx_MatrixCol2(&actor->field_2C->coords->coord, v);
    dir.vy = 0;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    __asm__ volatile("nop; nop; .word 0x4B98003D");
    gte_stsv(v);
    work->field_C[1].x = actor->field_2C->coords->coord.t[0] + dir.vx;
    work->field_C[1].z = actor->field_2C->coords->coord.t[2] + dir.vz;

    ((Task*)actor)->msgTable = &D_actor_401000_80154F90;
    root->sub                = &gGfxViewCoord;
    root->flg                = 0;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->field_8B8.coord      = &actor->field_2C->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    variant                    = actor->field_36;
    switch (variant & 0xF) {
        case 2:
            work->field_2 = -1;
            work->field_0 = 0;
            break;
        case 4:
            work->field_2 = -1;
            work->field_0 = 0x16;
            break;
        default:
            work->field_2 = -1;
            work->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (((Task*)actor)->spawnArg1 & 0xF) {
        case 2:
            work->field_C10 = D_actor_401000_8013E0AC[0].field_0;
            work->field_C12 = D_actor_401000_8013E0AC[0].field_2;
            work->field_C14 = D_actor_401000_8013E0AC[0].field_4;
            work->field_C16 = D_actor_401000_8013E0AC[0].field_6;
            break;
        case 1:
            work->field_C10 = D_actor_401000_8013E0AC[2].field_0;
            work->field_C12 = D_actor_401000_8013E0AC[2].field_2;
            work->field_C14 = D_actor_401000_8013E0AC[2].field_4;
            work->field_C16 = D_actor_401000_8013E0AC[2].field_6;
            break;
        case 0:
        default:
            work->field_C10 = D_actor_401000_8013E0AC[1].field_0;
            work->field_C12 = D_actor_401000_8013E0AC[1].field_2;
            work->field_C14 = D_actor_401000_8013E0AC[1].field_4;
            work->field_C16 = D_actor_401000_8013E0AC[1].field_6;
            break;
    }

    Actor401000_InitPose(actor->field_2C->coords, work);

    ((Task*)actor)->state++;
}

/// Spawn the effect a hit record `arg2` names at one of twelve model offsets
/// picked by the signed damage `arg1`: the `Gp_LcgState` draw's low bits
/// bucket `|arg1|` into below 0x200 / above 0x600 / positive / non-positive,
/// each selecting from its own run of `D_actor_401000_80154F30`. The chosen
/// offset goes into the work block's `field_8C0` and the `field_8B8` argument
/// record, which anchors it at the model's second coordinate part, scale
/// 0x300 and count 2 — the effect `func_800FDB18` then spawns hangs off the
/// part the vector's `pad` names. The 8-byte scratch the offset is built in is
/// carved off and given back around the call. Same body as
/// `Actor00100_Fn03340`, which keeps its record inline and scales by 0x100.
void func_actor_401000_80133940(Actor401000* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         sc;
    s32              mag;
    Actor401000Work* work;

    sc   = (SVECTOR*)(*(u32*)G_SCRATCH_HEAD -= 8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->field_1C;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_401000_80154F30[0];
                break;
            case 1:
                *sc = D_actor_401000_80154F30[1];
                break;
            case 2:
                *sc = D_actor_401000_80154F30[2];
                break;
            case 3:
                *sc = D_actor_401000_80154F30[3];
                break;
            default:
                *sc = D_actor_401000_80154F30[4];
                break;
        }
    } else if (mag > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_401000_80154F30[5];
                break;
            case 1:
                *sc = D_actor_401000_80154F30[6];
                break;
            default:
                *sc = D_actor_401000_80154F30[7];
                break;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_401000_80154F30[8];
        } else {
            *sc = D_actor_401000_80154F30[9];
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_401000_80154F30[10];
        } else {
            *sc = D_actor_401000_80154F30[11];
        }
    }
    work->field_8B8.coord      = &arg0->field_2C->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    work->field_8C0            = *sc;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->field_2C->coords[sc->pad], &work->field_8C0, &work->field_8B8);
    *(u32*)G_SCRATCH_HEAD += 8;
}

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80133D50);

/// Enter the live-actor state: reinstate the model buffers, seed the
/// `field_898` / `field_8A2` animation pair, fold the current `field_89E`
/// state onto the 0x17/0x18 pair, then hold in `func_actor_401000_80132EF0`
/// until the clip's `field_5A` frame count passes 6 (state 0x17) or 9 (state
/// 0x18), or the `flags_68` word reports the actor gone. The un-flagged path
/// halves `field_8A2` down to the +-0x10 turntable step and retires the actor
/// once the enemy is spent. Same body as `func_actor_401300_80135DDC`, which
/// drops the frame-count loop's `flags_68` guard and its own 0x36 test.
void func_actor_401000_80134DB4(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        tmd               = arg0->field_2C;
        enemy->node.flags = 0;
        tmd->flags        = 0;
        Tmd_AllocBuffers(tmd);
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_A10.flags |= 0x4000;
        if (work->field_89E == 11 || work->field_89E == 23) {
            work->field_89E = 0x17;
        } else if (work->field_89E == 12 || work->field_89E == 25 || work->field_89E == 24) {
            work->field_89E = 0x18;
        }
        if ((u16)(work->field_89E - 0x17) >= 2) {
            work->field_89E = 0x17;
        }
        do {
            func_actor_401000_80132EF0(arg0);
            if (work->field_89E == 0x17 && (work->field_5A & 0x3FF) >= 6) {
                break;
            }
            if (work->field_89E == 0x18 && (work->field_5A & 0x3FF) >= 9) {
                break;
            }
        } while (!(work->flags_68.word & 0x102));
        work->field_8A2 = 0x20;
        return;
    }
    arg0->field_2C->coords->flg = 0;
    work->field_8A2             = work->field_8A2 / 2;
    if (work->field_8A2 == 1) {
        work->field_8A2 = -0x10;
    }
    if (work->field_8A2 == -1) {
        work->field_8A2 = 0x10;
    }
    func_actor_401000_80132EF0(arg0);
    if (Gp_TickObjFlag2((GpObj5D*)enemy) == 1) {
        enemy->reactionFlags &= ~2;
        work->field_0         = 0x11;
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x11;
    }
}

/// Wrap `input` into the -0x800..0x800 turn range. Same body as
/// `Actor01900_NormalizeYaw` / `Actor401300_NormalizeYaw`.
static __inline__ s16 Actor401000_NormalizeYaw(s16 input)
{
    s16 value = input;

    if (input < 0) {
        while (1) {
            if (value >= -0x800) {
                break;
            }
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800) {
                break;
            }
            value -= 0x1000;
        }
    }
    return value;
}

/// Offset from `config`'s player position to `coord`'s, halfwords only. Same
/// body as `Actor401300_ConfigPositionDelta`.
static __inline__ void Actor401000_ConfigPositionDelta(PlayerStatus* config, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = config->coordMtx->t[0] - coord->coord.t[0];
    pos->vy = config->coordMtx->t[1] - coord->coord.t[1];
    pos->vz = config->coordMtx->t[2] - coord->coord.t[2];
}

/// Yaw from the actor's facing to the player, wrapped; `pos` receives the
/// offset. Same body as `Actor01900_PositionYaw` / `Actor401300_PositionYaw`.
static __inline__ s16 Actor401000_PositionYaw(Actor401000* actor, SVECTOR* pos, PlayerStatus* config)
{
    GsCOORDINATE2* coord;
    s32            angle;

    Actor401000_ConfigPositionDelta(config, actor->field_2C->coords, pos);
    coord = actor->field_2C->coords;
    angle = ratan2(pos->vx, pos->vz);
    return Actor401000_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Rebuild `coord`'s Y rotation from its current yaw, uniformly scaled by
/// `scale`. Same body as `Actor01900_RescaleYaw` / `Actor401300_RescaleYaw`.
static __inline__ void Actor401000_RescaleYaw(GsCOORDINATE2* coord, s16 scale)
{
    void**                 scratch;
    void*                  head;
    Actor401000RotScratch* blk;
    s16                    ang;
    u16                    m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor401000RotScratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((Actor401000RotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    *scratch             = (u8*)*scratch + 0x34;
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

/// Turn the actor toward the player, clamped to +-0x10 past its current facing,
/// then rebuild the root coordinate scaled by 0x1194. Same body as
/// `func_actor_401300_80136238`, minus that one's `field_8B6` state pair and
/// its message-id gate, which the 401000 sibling keeps in `field_C10`.
void func_actor_401000_80134F98(Actor401000* arg0)
{
    Actor401000Work*       work;
    Actor401000AimScratch* aim;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 9;
        work->field_89A        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        work->field_8D0.radius = 0x1AE;
        Gp_ArmStateF0(1);
        return;
    }
    *(Actor401000AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401000AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->coords->flg               = 0;
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
    aim->angle      = Actor401000_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE = aim->angle;
    if (aim->angle > 0x10) {
        aim->angle = 0x10;
    }
    if (aim->angle < -0x10) {
        aim->angle = -0x10;
    }
    coord       = arg0->field_2C->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor401000_RescaleYaw(arg0->field_2C->coords, 0x1194);
    func_actor_401000_80132EF0(arg0);
    *(Actor401000AimScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401000_801352DC(GpAreaKey* session, GsCOORDINATE2* coord)
{
    Actor401000HeightClamp* row;
    s32                     offset;
    s32                     lo;
    s16                     i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401000_80154FD0[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            lo     = row->lo;
            offset = coord->coord.t[1];
            if (offset < lo) {
                coord->coord.t[1] = lo;
            } else if (row->hi < offset) {
                coord->coord.t[1] = row->hi;
            }
            coord->flg = 0;
            return;
        }
    }
}

/// Whether `D_actor_401000_80154FD0` has a row matching the session's
/// `GpAreaKey::stage` / `area` pair. The helper behind both
/// height-clamp probes of `func_actor_401000_80135374`; the second probe is
/// followed by the `func_actor_401000_801352DC` call itself, which walks the
/// same rows to clamp the root Y. Same helper as `Actor401300_HasHeightClamp`.
static __inline__ s32 Actor401000_HasHeightClamp(GpAreaKey* session)
{
    Actor401000HeightClamp* row;
    s16                     i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401000_80154FD0[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            return 1;
        }
    }
    return 0;
}

/// Root-coordinate step, the 401000 twin of `func_actor_401300_80132C78`:
/// carve the 0x20-byte `Actor401000Delta` off `G_SCRATCH_HEAD`, fill its delta
/// from the `rec` obstacle record, clamp the Y step to ±0x12C while a
/// height-clamp row matches, hand the XZ step to the GTE normalisation once it
/// passes 0x96, and step the root coordinate by each component. Reports
/// whether anything moved.
///
/// Both the repeated clamp and the `clamped` temporary are load-bearing for
/// register allocation, not style. The first clamp only runs while a clamp row
/// matches and its in-range arm skips the second copy entirely, so folding the
/// two (or letting the add re-read `s->step.vy`) swaps `$s0`/`$s1`: the block
/// pointer against the `step` local. The temporary keeps one reference to the
/// block pointer out of the RTL, which is what tips that fight the other way.
s32 func_actor_401000_80135374(GsCOORDINATE2* coord, GpRec18* rec, s16 arg2, s16 arg3)
{
    Actor401000Delta* head;
    Actor401000Delta* s;
    s16               vy;
    s16               clamped;
    SVECTOR*          step;

    if (D_80072729 == 1) {
        return 0;
    }
    head                                = *(Actor401000Delta**)G_SCRATCH_HEAD;
    *(Actor401000Delta**)G_SCRATCH_HEAD = head - 1;
    s                                   = head - 1;
    s->moved                            = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        s->step.vx = head[-1].delta.vx.w >> 16;
        s->step.vy = s->delta.vy.w >> 16;
        s->step.vz = s->delta.vz.w >> 16;
        if (Actor401000_HasHeightClamp(&gGameSession->at4.loc.view)) {
            vy = s->step.vy;
            if (((vy >= 0) ? vy : -vy) <= 0x12C) {
                goto addStep;
            }
            clamped    = (vy <= 0) ? -0x12C : 0x12C;
            s->step.vy = clamped;
        }
        vy = s->step.vy;
        if (((vy >= 0) ? vy : -vy) <= 0x12C) {
            goto addStep;
        }
        s->step.vy = (vy <= 0) ? -0x12C : 0x12C;
    addStep:
        coord->coord.t[1] += s->step.vy;
        s->len             = s->step.vx * s->step.vx + s->step.vz * s->step.vz;
        s->len             = SquareRoot0(s->len);
        step               = &s->step;
        if (s->len >= 0x96) {
            s->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(0x96);
            gte_ldsv(step);
            gte_gpf12_real();
            gte_stsv(step);
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        } else {
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        }
        if (s->delta.vx.w & 0xFFFF) {
            if (s->delta.vx.w > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        if (s->delta.vz.w & 0xFFFF) {
            if (s->delta.vz.w > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    if (Actor401000_HasHeightClamp(&gGameSession->at4.loc.view)) {
        func_actor_401000_801352DC(&gGameSession->at4.loc.view, coord);
        coord->coord.t[1] += arg3;
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(Actor401000Delta**)G_SCRATCH_HEAD += 1;
    return s->moved;
}

void func_actor_401000_80135704(Actor401000* arg0, GpRec18* rec, s32 arg2);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80135704);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80135AA4);

/// Turn-aim state body, the 401000 twin of `Actor01900_Fn04D14`: take a 0x10
/// chase scratch off `G_SCRATCH_HEAD` and, on the live-actor flag, key the
/// two animation nodes, the frame counter and the `field_C06` clip phase.
/// Once `field_8` has counted 7 frames the arm aims at the player - the yaw
/// toward `gameGetPtrSlot(3)` goes in `pad_8`, the wrapped yaw toward
/// `Player_Status.coordMtx` in `pad_A` - and the root is turned by the facing
/// yaw plus a +-0x60 clamp of the turn's 1000 bias. The forward draw
/// `field_C04` is the doubled frame parameter (halved while `field_89A` is
/// up, forced to 2 while the frame counter runs), and the actor slides along
/// it when the `0x12C` probe reports the step is clear. `field_C06` walks 8 ->
/// -1 -> 0 as `field_8A2` passes 0x18 and 0x12, and the 0 arm runs the
/// five-frame exit window that re-aims once more and picks state 0xB when the
/// actor faces away from the player, else state 0x1A.
void func_actor_401000_801365C8(Actor401000* arg0)
{
    Actor401000ChaseScratch* chase;
    Actor401000ChaseScratch* head;
    Actor401000Work*         work;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           facing;
    s32                      turn;
    s32                      diffPos;
    s32                      diffNeg;
    s32                      yaw;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0xD7;
        work->field_898        = 1;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        work->field_C06         = 8;
        work->field_6           = 0;
        work->field_8           = 0;
        D_actor_401000_80155000 = 0;
        work->field_C24++;
        return;
    }
    head                                       = *(Actor401000ChaseScratch**)G_SCRATCH_HEAD;
    *(Actor401000ChaseScratch**)G_SCRATCH_HEAD = head - 1;
    chase                                      = head - 1;
    arg0->field_2C->coords->flg                = 0;
    func_actor_401000_80132EF0(arg0);
    if (func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC) == 1) {
        work->field_8++;
    } else {
        func_actor_401000_80135704(arg0, (GpRec18*)work->field_8F0, 0xC);
    }
    Actor401000_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &chase->delta);
    if (work->field_8 >= 7) {
        chase->pad_8  = ratan2(-((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                               ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
        chase->pad_A  = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
        chase->pad_A  = Actor401000_NormalizeYaw(chase->pad_A);
        work->field_0 = 0x1A;
    }
    coord       = arg0->field_2C->coords;
    chase->turn = Actor401000_NormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    turn        = chase->turn;
    if (turn >= 0) {
        diffPos = turn - 1000;
        if (ABS(diffPos) < 0x60) {
            chase->angle = chase->turn - 1000;
        } else if (diffPos > 0) {
            chase->angle = 0x60;
        } else {
            chase->angle = -0x60;
        }
    } else {
        diffNeg = turn + 1000;
        if (ABS(diffNeg) < 0x60) {
            chase->angle = chase->turn + 1000;
        } else if (diffNeg > 0) {
            chase->angle = 0x60;
        } else {
            chase->angle = -0x60;
        }
    }
    facing        = arg0->field_2C->coords;
    chase->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, chase->angle, 1);
    Actor401000_RescaleYaw(arg0->field_2C->coords, 0x1194);
    coord                       = arg0->field_2C->coords;
    work->field_8AE             = Actor401000_NormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    arg0->field_2C->coords->flg = 0;
    work->field_C04             = work->field_8A2 * 8;
    if (work->field_89A != 0) {
        work->field_C04 = work->field_C04 >> 1;
    }
    if (work->field_8 != 0) {
        work->field_C04 = 2;
    }
    if ((func_actor_401000_80132590(arg0->field_2C->coords, 0x12C, work->field_C04) << 0x10) != 0) {
        Actor401000_MoveForwardNonzero(arg0->field_2C->coords, (u16)work->field_C04);
    }
    D_actor_401000_80155000 += (u16)work->field_C04;
    if (work->field_C06 == 8 && work->field_8A2 >= 0x18) {
        work->field_C06 = -1;
    }
    if (work->field_C06 == -1 && work->field_8A2 == 0x12) {
        work->field_C06 = 0;
        work->field_6   = 0;
    }
    if (work->field_C06 == 0) {
        if (++work->field_6 == 5) {
            chase->pad_8 = ratan2(-((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                                  ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
            Actor401000_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &chase->delta);
            chase->pad_A = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
            chase->pad_A = Actor401000_NormalizeYaw(chase->pad_A);
            yaw          = chase->pad_A - chase->pad_8;
            if (yaw < 0) {
                yaw = -yaw;
            }
            if (yaw > 0x400 && work->field_C1B == 0) {
                work->field_0 = 0xB;
            } else {
                work->field_0 = 0x1A;
                work->field_2 = -1;
            }
        }
    }
    work->field_8A2 += (u16)work->field_C06;
    if (work->field_C1B != 0) {
        work->field_C1B--;
    }
    *(Actor401000ChaseScratch**)G_SCRATCH_HEAD += 1;
}

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor401000_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor401000RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    ((Actor401000RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor401000RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor401000RangeScratch*)(head - 0xC))->dx *= ((Actor401000RangeScratch*)(head - 0xC))->dx;
    *(Actor401000RangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)G_SCRATCH_HEAD                         = head;
    ret                                           = ((Actor401000RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// Turn-entry body, the 401000 twin of `func_actor_401300_801376E4`: carve the
/// chase scratch off `G_SCRATCH_HEAD`, and while the live-actor flag is up
/// reset the display nodes and rebuild the actor's facing. The turn direction
/// comes off the wrapped yaw toward the player, the yaw itself out of the
/// root's own rotation, and the pair (`field_C00` / `field_C02`) is what the
/// per-frame arm then walks: the turn swings the facing 0x89 a frame until it
/// reaches the seeded yaw, `Gfx_RotMatrixY` rebuilds the rotation from it and
/// `Actor401000_RescaleYaw` re-scales the root by 0x1194. When the two have
/// met the actor re-arms (`field_0` 8 or 0xB) off `field_C24`, the obstacle
/// range and the `field_C1B` cooldown, and the arm is then slid forward along
/// its obstacle table. `field_C1B` counts down once per entry.
void func_actor_401000_80136E20(Actor401000* arg0)
{
    Actor401000Work*         work;
    Actor401000ChaseScratch* aim;
    Actor401000ChaseScratch* head;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           facing;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        head                                       = *(Actor401000ChaseScratch**)G_SCRATCH_HEAD;
        obj                                        = arg0->field_2C;
        *(Actor401000ChaseScratch**)G_SCRATCH_HEAD = head - 1;
        aim                                        = head - 1;
        arg0->field_20->node.flags                 = 0;
        obj->flags                                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_8AE        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        Actor401000_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &aim->delta);
        coord                                       = arg0->field_2C->coords;
        aim->turn                                   = Actor401000_NormalizeYaw(ratan2(head[-1].delta.vx, aim->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing                                      = arg0->field_2C->coords;
        aim->angle                                  = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_C00                             = aim->angle;
        work->field_C02                             = aim->angle + (u16)aim->turn * 2;
        *(Actor401000ChaseScratch**)G_SCRATCH_HEAD += 1;
        return;
    }
    head                                       = *(Actor401000ChaseScratch**)G_SCRATCH_HEAD;
    *(Actor401000ChaseScratch**)G_SCRATCH_HEAD = head - 1;
    aim                                        = head - 1;
    func_actor_401000_80132EF0(arg0);
    Actor401000_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &aim->delta);
    if (work->field_C00 == work->field_C02) {
        if (work->field_C24 < 2 || Actor401000_OutOfRange(&aim->delta, 0x384) || work->field_C1B != 0) {
            work->field_0 = 8;
        } else {
            work->field_0 = 0xB;
        }
    }
    if (work->field_C00 > work->field_C02) {
        work->field_C00 -= 0x89;
        if (work->field_C00 < work->field_C02) {
            work->field_C00 = work->field_C02;
        }
    }
    if (work->field_C00 < work->field_C02) {
        work->field_C00 += 0x89;
        if (work->field_C00 > work->field_C02) {
            work->field_C00 = work->field_C02;
        }
    }
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, work->field_C00, 1);
    Actor401000_RescaleYaw(arg0->field_2C->coords, 0x1194);
    arg0->field_2C->coords->flg = 0;
    if (work->field_89A == 0) {
        if ((s16)func_actor_401000_80132590(arg0->field_2C->coords, 0x12C, 0x28) != 0) {
            Actor401000_MoveForward(arg0->field_2C->coords, 0x28);
        }
    } else {
        if ((s16)func_actor_401000_80132590(arg0->field_2C->coords, 0x12C, 0x14) != 0) {
            Actor401000_MoveForward(arg0->field_2C->coords, 0x14);
        }
    }
    if (func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC) != 1) {
        func_actor_401000_80135704(arg0, (GpRec18*)work->field_8F0, 0xC);
    }
    if (work->field_C1B != 0) {
        work->field_C1B--;
    }
    *(Actor401000ChaseScratch**)G_SCRATCH_HEAD += 1;
}

/// Turn-entry body, the 401000 twin of `func_actor_401300_80137D78`: carve the
/// aim scratch off `G_SCRATCH_HEAD`, and while the live-actor flag is up reset
/// the display nodes and rebuild the actor's facing. The turn direction
/// (`field_C08`) is drawn from `Gp_LcgState` on the first entry, and each entry
/// swings the facing toward the player by `field_C12` plus a 0x171 bias until
/// `field_C26` has been counted once. The forward direction `field_BF0` comes
/// out of the turn angle through `Gfx_RotMatrixY`, and the `field_C0A` draw
/// scales it onto the scratch vector; the actor is then slid along its obstacle
/// table, halving that draw while it overlaps. Counts the entry in `field_6`
/// and keys state 7 once 0x1E of them have run.
void func_actor_401000_801374D4(Actor401000* arg0)
{
    Actor401000Work*       work;
    Actor401000AimScratch* aim;
    Actor401000AimScratch* head;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    SVECTOR*               dir;
    MATRIX                 mat;
    u16                    angle;
    s32                    kind;

    kind = arg0->field_36;
    work = arg0->field_1C;
    if ((kind & 0xF0) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    head                                     = *(Actor401000AimScratch**)G_SCRATCH_HEAD;
    *(Actor401000AimScratch**)G_SCRATCH_HEAD = head - 1;
    aim                                      = head - 1;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0xD7;
        work->field_6          = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        Actor401000_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &aim->delta);
        aim->angle = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_C08 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C08 = 1;
            } else {
                work->field_C08 = -1;
            }
        }
        if (work->field_C08 == 1) {
            work->field_89E = 0x15;
            if (work->field_C26 == 0) {
                angle      = aim->angle + 0x171;
                aim->angle = work->field_C12 + angle;
            } else {
                aim->angle += work->field_C12;
            }
            work->field_C08 = -1;
        } else {
            work->field_89E = 0x14;
            if (work->field_C26 == 0) {
                angle      = aim->angle - 0x171;
                aim->angle = angle - work->field_C12;
            } else {
                aim->angle -= work->field_C12;
            }
            work->field_C08 = 1;
        }
        work->field_898 = 1;
        work->field_8A2 = 0xC;
        work->field_89A = 0;
        func_actor_401000_80132EF0(arg0);
        Gfx_RotMatrixY(&mat, aim->angle, 1);
        dir = &work->field_BF0;
        Gfx_MatrixCol2(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_C0A = 0xDE;
        work->field_C26++;
    }
    arg0->field_2C->coords->flg = 0;
    func_actor_401000_80132EF0(arg0);
    arg0->field_2C->coords->flg = 0;
    if (work->field_89A == 0) {
        gte_lddp(work->field_C0A);
        gte_ldsv(&work->field_BF0);
        gte_gpf12_real();
        gte_stsv(aim);
    } else {
        gte_lddp(work->field_C0A >> 1);
        gte_ldsv(&work->field_BF0);
        gte_gpf12_real();
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = arg0->field_2C->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->field_2C->coords;
        coord->coord.t[2] += aim->delta.vz;
        if (func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC) == 1) {
            work->field_C0A = (u16)(work->field_C0A >> 1);
        }
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
    }
    *(Actor401000AimScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801378DC);

void func_actor_401000_801380B8(Actor401000* arg0)
{
    SVECTOR          dir;
    Actor401000Work* work;
    GpEnemy*         enemy;
    Task*            player;
    SVECTOR*         pdir;
    GpMsg3EE*        msg;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        player                                   = gameGetPtrSlot(3);
        work->field_8D0.radius                   = 0x1AE;
        work->field_B50.flags                   &= 0x7FFF;
        work->field_A10.flags                   |= 0x4000;
        enemy->node.flags                        = 0;
        work->field_898                          = 1;
        work->field_8A2                          = 0x10;
        work->field_89E                          = 5;
        ((TmdObject*)player->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)player->extra)->coords);
        msg          = &D_actor_401000_80155018;
        msg->field_0 = ((TmdObject*)player->extra)->coords->coord.t[0];
        msg->field_4 = ((TmdObject*)player->extra)->coords->coord.t[1];
        msg->field_8 = ((TmdObject*)player->extra)->coords->coord.t[2];
        pdir         = &dir;
        dir.vx       = ((GpCoordXZ*)arg0->field_2C->coords)->field_18 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_18;
        dir.vy       = 0;
        dir.vz       = ((GpCoordXZ*)arg0->field_2C->coords)->field_20 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_20;
        VectorNormalSS(pdir, pdir);
        gte_lddp(0x3E8);
        gte_ldsv(pdir);
        gte_gpf12_real();
        gte_stsv(pdir);
        arg0->field_2C->coords->coord.t[0] = ((TmdObject*)player->extra)->coords->coord.t[0] + dir.vx;
        arg0->field_2C->coords->coord.t[2] = ((TmdObject*)player->extra)->coords->coord.t[2] + dir.vz;
        arg0->field_2C->coords->flg        = 0;
        msg->field_10                      = 0;
        msg->field_12                      = ratan2(dir.vx, dir.vz);
        msg->field_14                      = 0;
        Gp_DispatchMsg(player, 0x3E9, (s32)msg, 0);
        Gp_SpawnPadLerp(0xC, 8, 0x8F);
    }
    func_actor_401000_80132EF0(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->coords[2].coord, -0x80, 0);
    arg0->field_2C->coords[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->coords[2]);
    Gfx_RotMatrixX(&arg0->field_2C->coords[3].coord, -0x80, 0);
    arg0->field_2C->coords[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->coords[3]);
    if (work->field_89E == 5 && (work->flags_68.half & 1)) {
        work->field_8B8.coord      = arg0->field_2C->coords + 5;
        work->field_8B8.spawnArgLo = 0x200;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, arg0->field_2C->coords + 5, NULL, &work->field_8B8);
        work->field_0 = 0xD;
    }
}

void func_actor_401000_801383F0(Actor401000* arg0)
{
    Actor401000Work* work;
    GpAnimArg*       msg;
    GpEnemy*         enemy;
    Task*            player;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 6;
        work->field_898 = 2;
        msg             = &D_actor_401000_80154F1C;
        msg->field_4    = 2;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)msg, 0);
        player = gameGetPtrSlot(3);
        Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 0), 0);
        Gp_SpawnPadLerp(5, 0xFF, 8);
    }
    if (work->flags_68.half & 1) {
        work->field_8B8.coord      = arg0->field_2C->coords + 1;
        work->field_8B8.spawnArgLo = 0x100;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, arg0->field_2C->coords + 5, NULL, &work->field_8B8);
        work->field_0 = 0xE;
    }
    work->field_894 = work->field_5A & 0x3FF;
    func_actor_401000_80132EF0(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->coords[2].coord, -0x80, 0);
    arg0->field_2C->coords[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->coords[3]);
    Gfx_RotMatrixX(&arg0->field_2C->coords[3].coord, -0x80, 0);
    arg0->field_2C->coords[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->coords[2]);
}

void func_actor_401000_801385B0(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;
    GpAnimArg*       msg;
    PlayerStatus*    cfg;
    u8               kind;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    cfg   = &Player_Status;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 7;
        work->field_898 = 2;
        func_actor_401000_80132EF0(arg0);
        msg          = &D_actor_401000_80154F1C;
        msg->field_4 = 3;
        if (cfg->hp > 0) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)msg, 0);
        }
        work->field_C0C        = -0x78;
        work->field_6          = 0;
        work->field_A10.flags |= 0x4000;
        return;
    }
    if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0 && cfg->hp > 0 && work->field_C28 == 1) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
        work->field_C28 = 0;
    }
    if ((u32)((work->field_5A & 0x3FF) - 0x10) < 7U) {
        if ((s16)func_actor_401000_80132590(arg0->field_2C->coords, 0x12C, work->field_C0C) != 0) {
            Actor401000_MoveForwardNonzero(arg0->field_2C->coords, (u16)work->field_C0C);
        }
        if (func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC) == 1) {
            work->field_C0C = (s16)(u16)work->field_C0C / 2;
        }
        arg0->field_2C->coords->flg = 0;
    }
    func_actor_401000_80132EF0(arg0);
    if (work->flags_68.half & 1) {
        kind = enemy->node.targeted;
        if (kind == 1) {
            if (func_actor_401000_80132824(arg0) == kind) {
                work->field_0 = 6;
            } else {
                work->field_0 = 0xA;
            }
        } else {
            work->field_0 = 6;
        }
        if (cfg->hp > 0 && work->field_C28 == 1) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            work->field_C28 = 0;
        }
    }
}

/// State 8 body, the 401000 twin of `func_actor_401300_80138CF8`: on the
/// live-actor flag, reset the two animation nodes, the root coordinate and the
/// model's facing, then slide the root along both obstacle tables and take one
/// forward step while the 0x12C probe is still in range. The tail keys the
/// actor's next state (`field_0`) off `GpEnemy.hp` / `.reactionFlags` whenever
/// the work block's pending-request bit is up.
void func_actor_401000_801388F4(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0;
        work->field_8D0.radius = 0x1AE;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_898        = 1;
        work->field_89E        = 0xA;
        work->field_89A        = 0;
        work->field_8A2        = 0x10;
        work->field_8B0        = 0;
        work->field_8AE        = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= 0x4000;
    }
    func_actor_401000_80132EF0(arg0);
    func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_8F0, 0xC);
    func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC);
    if (work->field_89E == 0xA && (s16)func_actor_401000_80132590(arg0->field_2C->coords, 0x12C, -0x57) != 0) {
        Actor401000_MoveForward(arg0->field_2C->coords, -0x57);
    }
    arg0->field_2C->coords->flg = 0;
    if (work->flags_68.half & 1) {
        if (work->field_89E == 0xA) {
            work->field_89E = 0xB;
            work->field_898 = 2;
            func_actor_401000_80132EF0(arg0);
        }
        if ((work->flags_68.half & 1) && work->field_89E == 0xB) {
            work->field_8D0.flags |= 0x4000;
            if (enemy->hp > 0) {
                if (enemy->reactionFlags & 2) {
                    work->field_0 = 4;
                } else {
                    work->field_0 = 0x11;
                }
            } else {
                work->field_0 = 0x15;
            }
        }
    }
}

/// State 9 body, the 401000 twin of `func_actor_401300_80140300` and
/// `Actor01900_Fn09BE8`: on the live-actor flag, reset the two animation nodes,
/// the root coordinate and the model's facing, then hand the root to the
/// obstacle helper once per record table. The tail keys the actor's next state
/// (`field_0`) off `GpEnemy.hp` / `.reactionFlags` whenever the work block's
/// pending-request bit is up.
void func_actor_401000_80138BB4(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0;
        work->field_8D0.radius = 0x1AE;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_898        = 1;
        work->field_89E        = 0xC;
        work->field_8A2        = 0x10;
        work->field_8B0        = 0;
        work->field_8AE        = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= 0x4000;
    }
    func_actor_401000_80132EF0(arg0);
    func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_8F0, 0xC);
    func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC);
    arg0->field_2C->coords->flg = 0;
    if (work->flags_68.half & 1) {
        work->field_8D0.flags |= 0x4000;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Emergence tick: `field_6` counts up from the state's 0x18 seed and the
/// deltas dispatch the one-shot actions — release state F0 at 0x18, the
/// light-mode pair at 0x1D / 0x29, the 0x600A5 spawn at 0x1D, and the
/// `TmdObject.flags` writes at 0x2F / 0x3F. From 0x1A on the tail rebuilds
/// the actor's root coordinate: a Y rotation taken from the model root's
/// facing, scaled by 0x1194 less 0xB per tick past 0x14, written back through
/// `coord.m` with `flg` cleared so the local matrix is recomputed.
/// Same body as `Actor01900_Fn06904`, minus that one's 0x13 release argument
/// and with `field_A10.flags |= 0x4000` in place of its `&= 0xBFFF`.
void func_actor_401000_80138D08(Actor401000* arg0)
{
    Actor401000Work*       work;
    GpEnemy*               enemy;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401000RotScratch* blk;
    u8*                    head;
    u8*                    tail;
    void*                  scratch_base;
    s16                    temp_v0;
    s16                    ang;
    s16                    cur;
    s32                    k;
    s32                    sy;
    u16                    temp_v1;
    u16                    m22;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->flags            = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
        enemy->node.flags     = 1;
        work->field_6         = 0;
    }
    temp_v1      = (u16)work->field_6;
    scratch_base = PSX_SCRATCH;
    if (work->field_6 < 0x401) {
        work->field_6 = (s16)(temp_v1 + 1);
        temp_v0       = temp_v1 - 0x18;
        switch (temp_v0) {
            case 0:
                Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0xA);
                break;
            case 5:
                Gp_SetLightMode((GpObj4C*)enemy, 1);
                Gp_SpawnEff(0x600A5, arg0->field_2C->coords + 2, 3, NULL);
                break;
            case 23:
                arg0->field_2C->flags = 2;
                break;
            case 17:
                Gp_SetLightMode((GpObj4C*)enemy, 2);
                break;
            case 39:
                arg0->field_2C->flags = 0x80;
                break;
        }
        cur = work->field_6;
        if (cur >= 0x1A) {
            k                                        = 0x1194;
            head                                     = scratch_base;
            head                                     = *(u8**)(head + 0x3FC);
            coord                                    = arg0->field_2C->coords;
            blk                                      = (Actor401000RotScratch*)(head - 0x34);
            sy                                       = k - (cur - 0x14) * 0xB;
            *(Actor401000RotScratch**)G_SCRATCH_HEAD = blk;
            ang                                      = ratan2((s32)-coord->coord.m[2][0], (s32)coord->coord.m[2][2]);
            blk->angle                               = ang;
            Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
            blk->scale.vx = k;
            blk->scale.vy = (s32)(s16)sy;
            blk->scale.vz = k;
            ScaleMatrix(&blk->m, &((Actor401000RotScratch*)(head - 0x34))->scale);
            coord->coord.m[0][0] = *(u16*)&((Actor401000RotScratch*)(head - 0x34))->m.m[0][0];
            coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
            coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
            coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
            coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
            coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
            coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
            coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
            __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
            tail       = *(u8**)(tail + 0x3FC);
            m22        = *(u16*)&blk->m.m[2][2];
            coord->flg = 0;
            tail       = tail + 0x34;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
            coord->coord.m[2][2] = m22;
        }
    }
}

/// Per-frame tick of the 0xE/0xF animation pair, the same shape as
/// `func_actor_401000_8013922C`. The live-actor arm allocates the model
/// buffers, restores the saved pose matrix `field_BA8` over the live
/// `field_BC8`, and restarts the 0xE / 0x898 animation slots; the body is then
/// gated on the `field_6` countdown and a 0-15 `Gp_LcgState` draw. The XZ
/// offset to `D_80073B8C` is probed against `field_C16`, and an armed
/// `Gp_StateF0` bit 0x50000, each dropping the actor to state 6. The tail runs
/// `func_actor_401000_80132EF0` and swaps `field_89E` between 0xE and 0xF on
/// `flags_68` bits 1 and 2, re-running the tick after each swap.
/// Same body as `func_actor_401300_80139520`, with the pose matrix in place of
/// that one's `field_C48` / `field_C68` pair and a `field_C16` radius in place
/// of its literal 3000.
void func_actor_401000_80138F50(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj        = arg0->field_2C;
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_6          = 0;
        work->field_BC8        = work->field_BA8;
        work->field_89E        = 0xE;
        work->field_898        = 1;
        work->field_8A2        = work->field_8A4;
    }
    if (work->field_6 > 0x960) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 0xF)) {
            return;
        }
    } else {
        work->field_6 = (u16)work->field_6 + 1;
    }
    coord    = arg0->field_2C->coords;
    d        = &delta;
    delta.vx = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy    = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz    = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401000_OutOfRange(d, work->field_C16)) {
        work->field_0 = 6;
    }
    if (*(u32*)&Gp_StateF0 & 0x50000) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    func_actor_401000_80132EF0(arg0);
    if (work->field_89E == 0xE && (work->flags_68.half & 2)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            work->field_89E = 0xF;
            work->field_898 = 1;
            func_actor_401000_80132EF0(arg0);
        }
    }
    if (work->field_89E == 0xF && (work->flags_68.half & 1)) {
        work->field_89E = 0xE;
        work->field_898 = 1;
        func_actor_401000_80132EF0(arg0);
    }
}

void func_actor_401000_8013922C(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj                     = arg0->field_2C;
        D_actor_401000_80154E88 = &D_actor_401000_80154634;
        work->field_89E         = 0x10;
        work->field_898         = 2;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_8B0        = 0;
        work->field_8A2        = 0x10;
        work->field_8AE        = 0;
        work->field_6          = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->placeKey >> 0xC) << 8) | 0x51030008;
        pan   = (s8)Gp_GetObjPan(arg0->field_2C->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->field_2C->coords));
        work->field_6 = 1;
    }
    func_actor_401000_80132EF0(arg0);
    if ((work->field_5A & 0x3FF) == 4 && work->field_8B4 != (work->field_5A & 0x3FF)) {
        work->field_8B8.coord      = arg0->field_2C->coords + 1;
        work->field_8B8.spawnArgLo = 0x100;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->field_2C->coords + 5, NULL, &work->field_8B8);
    }
    work->field_8B4 = work->field_5A & 0x3FF;
    coord           = arg0->field_2C->coords;
    d               = &delta;
    delta.vx        = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy           = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz           = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401000_OutOfRange(d, work->field_C16)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    if (*(u32*)&Gp_StateF0 & 0x50000) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
}

/// Walk the actor along its `field_C` waypoint pair: `field_14` picks the
/// waypoint the offset is taken from and flips once the actor closes inside
/// 0xA0 of it, or after 0x15 frames in `field_6`, and the wrapped yaw toward
/// that waypoint is clamped to +-0x20, added back to the facing yaw and the
/// root rotation rescaled by 0x1194. The `func_actor_401000_80132590` probe
/// takes one 0xA step forward, the obstacle walk runs against `field_A30`
/// (plus `field_8F0` through `func_actor_401000_80135704` when `field_36` is
/// 0x10), and each arm counts `field_6` up while the yaw stays inside 0x80.
/// The tail drops the actor to state 6 on the `Player_Status` range checks and
/// the `Gp_StateF0` bits, and the live-actor arm restarts the 0x1AE clip.
void func_actor_401000_801394EC(Actor401000* arg0)
{
    Actor401000Work*        work;
    Actor401000TurnScratch* turn;
    TmdObject*              obj;
    GpRec18*                rec;
    GsCOORDINATE2*          coord;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        work->field_6 = 0;
        if (arg0->field_36 == 0x10) {
            work->field_8D0.flags |= 0x4000;
        }
    } else {
        *(Actor401000TurnScratch**)G_SCRATCH_HEAD -= 1;
        turn                                       = *(Actor401000TurnScratch**)G_SCRATCH_HEAD;
        turn->delta.vx                             = work->field_C[work->field_14].x - arg0->field_2C->coords->coord.t[0];
        turn->delta.vy                             = 0;
        turn->delta.vz                             = work->field_C[work->field_14].z - arg0->field_2C->coords->coord.t[2];
        if (!Actor401000_OutOfRange(&turn->delta, 0xA0) || work->field_6 >= 0x15) {
            if (work->field_14 == 0) {
                work->field_14 = 1;
            } else {
                work->field_14 = 0;
            }
            work->field_6 = 0;
        }
        func_actor_401000_80132EF0(arg0);
        coord           = arg0->field_2C->coords;
        turn->angle     = Actor401000_NormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = turn->angle;
        if (turn->angle > 0x20) {
            turn->angle = 0x20;
        }
        if (turn->angle < -0x20) {
            turn->angle = -0x20;
        }
        turn->angle += ratan2(-arg0->field_2C->coords->coord.m[2][0], arg0->field_2C->coords->coord.m[2][2]);
        Gfx_RotMatrixY(&arg0->field_2C->coords->coord, turn->angle, 1);
        Actor401000_RescaleYaw(arg0->field_2C->coords, 0x1194);
        if (work->field_89A == 0 && (func_actor_401000_80132590(arg0->field_2C->coords, 0x12C, 0xA) << 16) != 0) {
            Actor401000_MoveForward(arg0->field_2C->coords, 0xA);
        }
        if (arg0->field_36 != 0x10) {
            if (func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC) == 1) {
                if (ABS(work->field_8AE) < 0x80) {
                    work->field_6 = (u16)work->field_6 + 1;
                }
            }
        } else {
            rec = (GpRec18*)work->field_8F0;
            if (func_actor_401000_801323EC(arg0->field_2C->coords, rec, 0xC) != 1 && func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC) != 1) {
                func_actor_401000_80135704(arg0, rec, 0xC);
            } else {
                if (ABS(work->field_8AE) < 0x80) {
                    work->field_6 = (u16)work->field_6 + 1;
                }
            }
        }
        arg0->field_2C->coords->flg = 0;
        if (func_actor_401000_80132824(arg0) != 1) {
            Actor401000_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &turn->delta);
            if (!Actor401000_OutOfRange(&turn->delta, work->field_C16)) {
                work->field_0 = 6;
            } else if (!Actor401000_OutOfRange(&turn->delta, 0xFA0)) {
                coord       = arg0->field_2C->coords;
                turn->angle = Actor401000_NormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
                if (ABS(turn->angle) < 0x300) {
                    work->field_0 = 6;
                }
            }
        }
        if (*(u32*)&Gp_StateF0 & 0xD0000) {
            work->field_0 = 6;
        }
        *(Actor401000TurnScratch**)G_SCRATCH_HEAD += 1;
    }
}

/// Walk the actor at the player: on the live-actor flag it restarts the
/// 0x12 clip and clears the spawn pose, then takes a 0xC-byte `G_SCRATCH_HEAD`
/// turn block, aims it at `Player_Status.coordMtx` through
/// `Actor401000_PositionYaw`, clamps the turn to +-0x40 and adds the facing
/// yaw back in before rebuilding the root coordinate. The obstacle walk
/// `func_actor_401000_801323EC` runs against `field_A30` and hands
/// `field_8F0` to `func_actor_401000_80135704` when it reports a hit, the
/// `func_actor_401000_80132590` probe takes one forward step out of
/// `field_C04`, and that same countdown then runs down by 0xA a frame. The
/// tail drops the actor to state 9 on the `flags_68` bit or once the
/// countdown is spent.
void func_actor_401000_80139D10(Actor401000* arg0)
{
    Actor401000Work*        work;
    GpEnemy*                enemy;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    Actor401000TurnScratch* turn;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy           = arg0->field_20;
        obj             = arg0->field_2C;
        work->field_89E = 0x12;
        work->field_898 = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_8B0        = 0;
        work->field_8A2        = 0x1E;
    }
    *(Actor401000TurnScratch**)G_SCRATCH_HEAD -= 1;
    turn                                       = *(Actor401000TurnScratch**)G_SCRATCH_HEAD;
    turn->angle                                = Actor401000_PositionYaw(arg0, &turn->delta, &Player_Status);
    work->field_8AE                            = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->field_2C->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, turn->angle, 1);
    if (func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC) != 1) {
        func_actor_401000_80135704(arg0, (GpRec18*)work->field_8F0, 0xC);
    }
    if ((func_actor_401000_80132590(arg0->field_2C->coords, 0x12C, work->field_C04) << 0x10) != 0) {
        Actor401000_MoveForwardNonzero(arg0->field_2C->coords, (u16)work->field_C04);
    }
    if (work->field_C04 > 0) {
        work->field_C04 = (u16)work->field_C04 - 0xA;
        if (work->field_C04 < 0) {
            work->field_C04 = 0;
        }
    }
    func_actor_401000_80132EF0(arg0);
    if ((work->flags_68.half & 1) || work->field_C04 == 0) {
        work->field_0 = 9;
    }
    *(Actor401000TurnScratch**)G_SCRATCH_HEAD += 1;
}

/// Aim step toward the player, the `func_actor_401300_8013A5C0` twin. Unlike
/// `func_actor_401000_80134F98` the wrapped turn is halved and clamped to
/// +-0x80 instead of +-0x10, so the actor turns at half speed and only the
/// 0x16/2 -> 0x16/0x11 spawn pair keys the follow-up; the spawn arm writes
/// `field_8D0.field_1C` first and leaves `field_6` alone, and the exit turn
/// reads the sign of `field_8AE` with the `0x4B0` arm first.
void func_actor_401000_8013A0C8(Actor401000* arg0)
{
    Actor401000Work*       work;
    Actor401000AimScratch* aim;
    TmdObject*             obj;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 1;
        work->field_8A2        = 0x16;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        return;
    }
    func_actor_401000_80132EF0(arg0);
    *(Actor401000AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401000AimScratch**)G_SCRATCH_HEAD;
    aim->angle                                = Actor401000_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE                           = aim->angle;
    if (ABS(aim->angle) < 0x81 && work->field_89E == 2) {
        work->field_8A2 = 0x16;
        work->field_89E = 0x11;
        work->field_898 = 1;
        work->field_6   = 0;
        func_actor_401000_80132EF0(arg0);
    }
    if (aim->angle >= 0x81) {
        aim->angle = 0x80;
    }
    if (aim->angle < -0x80) {
        aim->angle = -0x80;
    } else {
        aim->angle = aim->angle >> 1;
    }
    aim->angle += ratan2(-arg0->field_2C->coords->coord.m[2][0], arg0->field_2C->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor401000_RescaleYaw(arg0->field_2C->coords, 0x1194);
    arg0->field_2C->coords->flg = 0;
    if (work->field_89E == 0x11) {
        work->field_6++;
        if ((func_actor_401000_80132590(arg0->field_2C->coords, 0x12C, -0x10) << 16) != 0) {
            Actor401000_MoveForward(arg0->field_2C->coords, -0x10);
        }
        if (func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC) != 1) {
            func_actor_401000_80135704(arg0, (GpRec18*)work->field_8F0, 0xC);
        }
        arg0->field_2C->coords->flg = 0;
        if ((s16)work->field_6 >= 0x13) {
            if (work->field_8AE <= 0) {
                Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0x4B0, 0);
            } else {
                Gfx_RotMatrixY(&arg0->field_2C->coords->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    *(Actor401000AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Aim step toward the player: the same body as `func_actor_401000_80134F98`
/// with three differences. The wrapped turn is clamped to zero-or-negative
/// rather than +-0x10, so the actor only ever rotates one way; the animation
/// slot is 0x13 instead of 9 and `field_8D0.field_1C` is written before the
/// other state words; and the spawn arm clears the `field_6` latch on its way
/// out instead of arming state F0.
void func_actor_401000_8013A5F0(Actor401000* arg0)
{
    Actor401000Work*       work;
    Actor401000AimScratch* aim;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 0x13;
        work->field_89A        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6                            += 1;
    *(Actor401000AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401000AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->coords->flg               = 0;
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
    aim->angle      = Actor401000_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE = aim->angle;
    if (aim->angle > 0) {
        aim->angle = 0;
    }
    if (aim->angle < 0) {
        aim->angle = 0;
    }
    coord       = arg0->field_2C->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor401000_RescaleYaw(arg0->field_2C->coords, 0x1194);
    func_actor_401000_80132EF0(arg0);
    *(Actor401000AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Turn the actor toward the player in two stages: while the `field_6`
/// countdown is under 0x32 the five part coordinates are reset to fixed
/// pitches, and afterwards each one unwinds by shifting its pitch down a step
/// every four frames; the turn itself is clamped to +-0x24 and drops the actor
/// to state 7 once it lines up. The `field_8AE` slot it drives is what
/// `func_actor_401000_8013A5F0` writes whole; here it slides toward the target
/// by at most 0x28 a frame. Same body as `func_actor_401300_8013AE48`, with the
/// spawn arm arming the 0x13 clip, `field_8D0.field_1C` written before the other
/// state words, and `field_A10.flags |= 0x4000` in place of the sibling's
/// `&= 0xBFFF`.
void func_actor_401000_8013A930(Actor401000* arg0)
{
    Actor401000Work*       work;
    Actor401000AimScratch* aim;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 2;
        work->field_8A2        = 8;
        work->field_89E        = 0x13;
        work->field_89A        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        func_actor_401000_80132EF0(arg0);
        func_actor_401000_80132EF0(arg0);
        work->field_6   = 0;
        work->field_8B0 = 0;
        return;
    }
    work->field_6++;
    *(Actor401000AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401000AimScratch**)G_SCRATCH_HEAD;
    aim->angle                                = Actor401000_PositionYaw(arg0, &aim->delta, &Player_Status);
    if (work->field_8AE < aim->angle) {
        if (aim->angle - work->field_8AE > 0x28) {
            work->field_8AE += 0x28;
        } else {
            work->field_8AE = aim->angle;
        }
    } else if (work->field_8AE - aim->angle > 0x28) {
        work->field_8AE -= 0x28;
    } else {
        work->field_8AE = aim->angle;
    }
    coord      = arg0->field_2C->coords;
    aim->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor401000_RescaleYaw(arg0->field_2C->coords, 0x1194);
    func_actor_401000_80132EF0(arg0);
    if (work->field_6 < 0x32) {
        Gfx_RotMatrixX(&arg0->field_2C->coords[1].coord, 0x40, 0);
        arg0->field_2C->coords[1].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[1]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[2].coord, 0x80, 0);
        arg0->field_2C->coords[2].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[2]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[3].coord, 0x80, 0);
        arg0->field_2C->coords[3].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[3]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[4].coord, 0x80, 0);
        arg0->field_2C->coords[4].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[4]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[5].coord, 0x100, 0);
        arg0->field_2C->coords[4].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[4]);
    } else {
        Gfx_RotMatrixX(&arg0->field_2C->coords[1].coord, 0x40 >> ((work->field_6 - 0x31) / 4), 0);
        arg0->field_2C->coords[1].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[1]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[2].coord, 0x80 >> ((work->field_6 - 0x30) / 4), 0);
        arg0->field_2C->coords[2].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[2]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[3].coord, 0x80 >> ((work->field_6 - 0x2F) / 4), 0);
        arg0->field_2C->coords[3].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[3]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[4].coord, 0x80 >> ((work->field_6 - 0x2E) / 4), 0);
        arg0->field_2C->coords[4].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[4]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[5].coord, 0x100 >> ((work->field_6 - 0x31) / 4), 0);
        arg0->field_2C->coords[4].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[4]);
        aim->angle = Actor401000_PositionYaw(arg0, &aim->delta, &Player_Status);
        if (aim->angle > 0x24) {
            aim->angle = 0x24;
        } else if (aim->angle < -0x24) {
            aim->angle = -0x24;
        }
        if (ABS(aim->angle) < 0x24 || work->field_6 >= 0x4F) {
            work->field_0 = 7;
        }
        coord       = arg0->field_2C->coords;
        aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
        Actor401000_RescaleYaw(arg0->field_2C->coords, 0x1194);
        arg0->field_2C->coords->flg = 0;
    }
    *(Actor401000AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Tint a freshly spawned effect model from the enemy's area record: read the
/// session's location key, let `Gp_SyncAreaKeyIndex` fill its table index, and
/// copy the room's texture page / CLUT row into the model's `TmdObject`. The
/// same body `func_actor_302600_80165A6C` writes out inline and
/// `Actor401300_TintEffect` keeps as a helper; it has to be a helper here too
/// rather than four copies in the caller, because each inline expansion gets
/// its own `raw` / `model` / `idx` pseudos and the copies then share `$s0`
/// block by block, while four uses of one variable rank `model` (96 refs/insn
/// over 104) above `idx` (24 over 28) in `global.c` and hand it `$s0`.
static __inline__ void Actor401000_TintEffect(GpEffWork* eff, GpEnemy* enemy)
{
    GpAreaKey    key;
    GpAreaKey*   sessionKey;
    u8           areaByte0;
    GpAreaRec*   rec;
    GpAreaPlace* entry;
    TmdObject*   model;
    s32          idx;
    u32          raw;

    if (eff != NULL) {
        sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
        raw        = enemy->placeKey;
        model      = (TmdObject*)eff->task->extra;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        areaByte0  = gGameSession->at4.loc.view;
        idx        = raw >> 12;
        key.view   = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        rec = Gp_GetNestedAreaRec(&key);
        /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled
           index onto the table (`addu s0, s0, v0`). */
        entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = entry->tpage;
        model->clut  = entry->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
}

/// Clip-0x2D body: on the live-actor flag it resets the effect node and the
/// spawn offset, then walks the animation latch `field_6` from 0 to 0x3D and
/// spawns one effect per key frame, each tinted by `Actor401000_TintEffect`.
/// At 0x3D the actor returns to state 0. The 401000 twin of
/// `func_actor_401300_8013B6E8`: same five clips, three of them at the same
/// node offsets (`+1`, `+9`, `+12`, `+1`, `+3` off the root coordinate) and the
/// same 0x64/0/0 spawn vector, but it reads the offset from the work block
/// rather than a stack `SVECTOR` and has no `field_D20` guard on the tail.
void func_actor_401000_8013B1E4(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;
    u16              next;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0x80;
        work->field_8D0.radius = 0x1AE;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 1;
        work->field_8AE        = 0;
        work->field_6          = 0;
        work->field_8C0.vx     = 0x64;
        work->field_8C0.vz     = 0;
        work->field_8C0.vy     = 0;
        Gp_SpawnEff(0x60030, arg0->field_2C->coords + 1, 0x10300, &work->field_8C0);
        Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0xA);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 3) {
        D_80114B78[0]      = &D_actor_401000_80143EB4;
        work->field_8C0.vz = 0x64;
        work->field_8C0.vy = 0;
        work->field_8C0.vx = 0;
        Actor401000_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 9, 0x200, &work->field_8C0), enemy);
    }
    if ((s16)work->field_6 == 5) {
        D_80114B78[0]      = &D_actor_401000_80144830;
        work->field_8C0.vy = 0;
        work->field_8C0.vx = 0;
        Actor401000_TintEffect(Gp_SpawnEff(0xA0000 | 5, arg0->field_2C->coords + 12, 0x200, &work->field_8C0), enemy);
    }
    if ((s16)work->field_6 == 7) {
        D_80114B78[0] = &D_actor_401000_80146190;
        Actor401000_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 1, 0x200, NULL), enemy);
    }
    if ((s16)work->field_6 == 8) {
        D_80114B78[0] = &D_actor_401000_8014599C;
        Actor401000_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 3, 0x200, NULL), enemy);
    }
    if ((s16)work->field_6 >= 0x3D) {
        work->field_0 = 0;
    }
}

/// Rebuild `coord`'s Y rotation from its current yaw at unit scale, the same
/// body as `Actor401300_ResetYaw` / `Actor01900_ResetYaw` (`coord->coord.m` is
/// splatted back from the rotation scratch block and `flg` cleared so the local
/// matrix is recomputed). The state-0x1A body below walks it down the model
/// root's node run from `+2` to `+10`.
static __inline__ void Actor401000_ResetYaw(GsCOORDINATE2* coord)
{
    void*                  head;
    Actor401000RotScratch* blk;
    s16                    ang;

    head                    = *(void**)G_SCRATCH_HEAD;
    blk                     = (Actor401000RotScratch*)((u8*)head - 0x34);
    *(void**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = 1;
    blk->scale.vy = 1;
    blk->scale.vx = 1;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]    = *(u16*)&blk->m.m[0][0];
    coord->coord.m[0][1]    = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]    = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]    = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]    = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]    = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]    = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]    = *(u16*)&blk->m.m[2][1];
    coord->coord.m[2][2]    = *(u16*)&blk->m.m[2][2];
    coord->flg              = 0;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x34;
}

/// State-2 clip body and its 0x1A successor, the 401000 twin of
/// `func_actor_401300_8013BB30` and `Actor01900_Fn0892C`. On the live-actor flag
/// it arms the effect node, seeds the 0x8C0 spawn offset and the animation
/// slots, and spawns clip 0x60030. `field_6` then counts up under `field_89E`:
/// the state-2 arm waits 0x10 frames on `flags_68` bit 2 before switching to
/// 0x1A, runs the `0x12C`/0xA range probe and the `field_A30` obstacle slide,
/// and spawns the three tinted key-frame effects at counts 3, 5 and 6; the
/// state-0x1A arm gates on `flags_68` bit 0x100, dispatches the one-shot actions
/// off `field_6 - 0x19`, and from 0x1A on rebuilds the root coordinate through
/// `ratan2` at scale `0x1194 - (field_6 - 0x14) * 0xB`. Both arms end in
/// `func_actor_401000_80132EF0` and `Actor401000_ResetYaw` on nodes 2..10.
void func_actor_401000_8013B61C(Actor401000* arg0)
{
    Actor401000Work*       work;
    GpEnemy*               enemy;
    GsCOORDINATE2*         coord;
    Actor401000RotScratch* blk;
    u8*                    head;
    u8*                    tail;
    void*                  scratch_base;
    u16                    next;
    s16                    ang;
    s16                    cur;
    s32                    k;
    s32                    sy;
    u16                    m22;

    work         = arg0->field_1C;
    enemy        = arg0->field_20;
    scratch_base = PSX_SCRATCH;
    if (work->field_4 != 0) {
        work->field_8D0.radius = 0x1AE;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 1;
        work->field_8C0.vx     = 0x64;
        work->field_89E        = 2;
        work->field_898        = 1;
        work->field_8AE        = 0;
        work->field_6          = 0;
        work->field_8C0.vz     = 0;
        work->field_8C0.vy     = 0;
        work->field_8A2        = 0x10;
        Gp_SpawnEff(0x60030, arg0->field_2C->coords + 1, 0x10300, &work->field_8C0);
        work->field_6 = 0;
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    switch (work->field_89E) {
        case 2:
            if ((s16)next >= 0x10 && (work->flags_68.half & 2)) {
                work->field_89E = 0x1A;
                work->field_898 = 2;
                work->field_8A2 = 0x10;
                work->field_89A = 0;
            }
            if ((s16)func_actor_401000_80132590(arg0->field_2C->coords, 0x12C, 0xA) != 0) {
                Actor401000_MoveForward(arg0->field_2C->coords, 0xA);
            }
            func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC);
            if ((s16)work->field_6 == 3) {
                D_80114B78[0]      = &D_actor_401000_80143EB4;
                work->field_8C0.vz = 0x64;
                work->field_8C0.vy = 0;
                work->field_8C0.vx = 0;
                Actor401000_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 9, 0x200, &work->field_8C0), enemy);
            }
            if ((s16)work->field_6 == 5) {
                D_80114B78[0] = &D_actor_401000_80146190;
                Actor401000_TintEffect(Gp_SpawnEff(0xA0000 | 5, arg0->field_2C->coords + 1, 0x200, NULL), enemy);
            }
            if ((s16)work->field_6 == 6) {
                D_80114B78[0] = &D_actor_401000_8014599C;
                Actor401000_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 3, 0x200, NULL), enemy);
            }
            break;
        case 0x1A:
            if (!(work->flags_68.half & 0x100)) {
                work->field_6 = 0;
            }
            switch ((s16)(work->field_6 - 0x19)) {
                case 0:
                    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0xA);
                    break;
                case 5:
                    Gp_SetLightMode((GpObj4C*)enemy, 1);
                    Gp_SpawnEff(0x600A5, arg0->field_2C->coords + 2, 2, NULL);
                    break;
                case 23:
                    arg0->field_2C->flags = 2;
                    break;
                case 17:
                    Gp_SetLightMode((GpObj4C*)enemy, 2);
                    break;
                case 39:
                    arg0->field_2C->flags = 0x80;
                    work->field_0         = 0;
                    break;
            }
            cur = work->field_6;
            if (cur >= 0x1A) {
                k                                        = 0x1194;
                head                                     = scratch_base;
                head                                     = *(u8**)(head + 0x3FC);
                coord                                    = arg0->field_2C->coords;
                blk                                      = (Actor401000RotScratch*)(head - 0x34);
                sy                                       = k - (cur - 0x14) * 0xB;
                *(Actor401000RotScratch**)G_SCRATCH_HEAD = blk;
                ang                                      = ratan2((s32)-coord->coord.m[2][0], (s32)coord->coord.m[2][2]);
                blk->angle                               = ang;
                Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
                blk->scale.vx = k;
                blk->scale.vy = (s32)(s16)sy;
                blk->scale.vz = k;
                ScaleMatrix(&blk->m, &((Actor401000RotScratch*)(head - 0x34))->scale);
                coord->coord.m[0][0] = *(u16*)&((Actor401000RotScratch*)(head - 0x34))->m.m[0][0];
                coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
                coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
                coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
                coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
                coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
                coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
                coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
                __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
                tail       = *(u8**)(tail + 0x3FC);
                m22        = *(u16*)&blk->m.m[2][2];
                coord->flg = 0;
                tail       = tail + 0x34;
                __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
                coord->coord.m[2][2] = m22;
            }
            break;
    }
    func_actor_401000_80132EF0(arg0);
    Actor401000_ResetYaw(arg0->field_2C->coords + 2);
    Actor401000_ResetYaw(arg0->field_2C->coords + 3);
    Actor401000_ResetYaw(arg0->field_2C->coords + 4);
    Actor401000_ResetYaw(arg0->field_2C->coords + 5);
    Actor401000_ResetYaw(arg0->field_2C->coords + 6);
    Actor401000_ResetYaw(arg0->field_2C->coords + 7);
    Actor401000_ResetYaw(arg0->field_2C->coords + 8);
    Actor401000_ResetYaw(arg0->field_2C->coords + 9);
    Actor401000_ResetYaw(arg0->field_2C->coords + 10);
}

/// State-2 aim body, the 401000 twin of `func_actor_401300_8013CBAC` and
/// `Actor01900_Fn042BC`: on the live-actor flag it resets the effect node, forks
/// the first clip and seeds the animation slots, then walks both obstacle tables
/// and aims the actor at the player with `Gfx_RotMatrixY` / `Actor401000_RescaleYaw`.
/// `field_6` and `field_8` then count up under the `func_actor_401000_80132824`
/// clip test: the still-aiming arm re-wraps the turn, drops the actor to state
/// 0xB once the 0x44C range check fails inside 0x200 and re-arms at 0x1B past
/// 0x5B frames, while the settled arm draws a turn direction from `Gp_LcgState`
/// and flips it every 0xF1 frames. The tail takes one forward step off the
/// 0x12C probe, or re-arms the clip on the `flags_68` bit. `field_C1B` counts
/// down once per entry.
void func_actor_401000_8013C46C(Actor401000* arg0)
{
    Actor401000Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         facing;
    Actor401000AimScratch* head;
    Actor401000AimScratch* s;
    s16                    yaw;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = 0x1AE;
        work->field_898        = 1;
        work->field_8A2        = 0x24;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_C24        = 0;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        Gp_ArmStateF0(1);
        work->field_6 = 0;
        work->field_8 = 0;
        if (arg0->field_36 == 0x10) {
            work->field_8D0.flags |= 0x4000;
        }
    }
    work->field_6++;
    work->field_8++;
    head                                     = *(Actor401000AimScratch**)G_SCRATCH_HEAD;
    *(Actor401000AimScratch**)G_SCRATCH_HEAD = head - 1;
    s                                        = head - 1;
    if (func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC) != 1) {
        if (func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_8F0, 0xC) != 1) {
            func_actor_401000_80135704(arg0, (GpRec18*)work->field_8F0, 0xC);
        }
    }
    Actor401000_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &s->delta);
    arg0->field_2C->coords->flg = 0;
    func_actor_401000_80132EF0(arg0);
    s->pad_8 = ratan2(-((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                      ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
    Actor401000_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &s->delta);
    yaw             = ratan2(s->delta.vx, s->delta.vz) + 0x800;
    s->pad_A        = yaw;
    s->pad_A        = Actor401000_NormalizeYaw(yaw);
    coord           = arg0->field_2C->coords;
    s->angle        = Actor401000_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = s->angle;
    if (func_actor_401000_80132824(arg0) != 1) {
        work->field_6   = 0;
        coord           = arg0->field_2C->coords;
        s->angle        = Actor401000_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->angle;
        if (s->angle < 0x200) {
            if (!Actor401000_OutOfRange(&s->delta, 0x44C) && work->field_C1B == 0) {
                work->field_0 = 0xB;
            }
        }
        if (work->field_8 >= 0x5B) {
            work->field_0 = 0x1B;
        }
    } else {
        work->field_8   = 0;
        coord           = arg0->field_2C->coords;
        s->angle        = Actor401000_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->angle;
        if (work->field_C08 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C08 = -1;
            } else {
                work->field_C08 = 1;
            }
        }
        if (work->field_C08 == 1) {
            s->angle += 0x300;
        } else {
            s->angle -= 0x300;
        }
        if (work->field_6 >= 0xF1) {
            work->field_6   = 0;
            work->field_C08 = -work->field_C08;
        }
    }
    if (s->angle > 0x20) {
        s->angle = 0x20;
    }
    if (s->angle < -0x20) {
        s->angle = -0x20;
    }
    facing    = arg0->field_2C->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, s->angle, 1);
    Actor401000_RescaleYaw(arg0->field_2C->coords, 0x1194);
    arg0->field_2C->coords->flg = 0;
    if (work->field_89E == 2) {
        if (work->field_89A == 0) {
            if ((s16)func_actor_401000_80132590(arg0->field_2C->coords, 0x12C, 0x16) != 0) {
                Actor401000_MoveForward(arg0->field_2C->coords, 0x16);
            }
        } else {
            if ((s16)func_actor_401000_80132590(arg0->field_2C->coords, 0x12C, 5) != 0) {
                Actor401000_MoveForward(arg0->field_2C->coords, 5);
            }
        }
    } else if (work->flags_68.half & 1) {
        work->field_89E = 2;
        work->field_898 = 1;
    }
    if (work->field_C1B != 0) {
        work->field_C1B--;
    }
    *(Actor401000AimScratch**)G_SCRATCH_HEAD += 1;
}

/// State 9 clip-0xB body, the 401000 twin of `func_actor_401000_80138BB4` and
/// `func_actor_401000_8013CEF0`: on the live-actor flag it resets the two
/// animation nodes and the root coordinate like the state 9 body, but keys the
/// node pair off clip 0xB / slot 2 and tests the request bit `0x100` rather
/// than bit 0. Same tail: `GpEnemy.hp` / `.reactionFlags` pick the next
/// `field_0` whenever the request bit is up.
void func_actor_401000_8013CD9C(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0;
        work->field_8D0.radius = 0x1AE;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_898        = 2;
        work->field_89E        = 0xB;
        work->field_8A2        = 0x10;
        work->field_8B0        = 0;
        work->field_8AE        = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= 0x4000;
    }
    func_actor_401000_80132EF0(arg0);
    func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_8F0, 0xC);
    func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC);
    arg0->field_2C->coords->flg = 0;
    if (work->flags_68.half & 0x100) {
        work->field_8D0.flags |= 0x4000;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// State 10 body, the 401000 twin of `func_actor_401000_80138BB4` and
/// `func_actor_401300_8014046C`: on the live-actor flag it resets the two
/// animation nodes and the root coordinate like the state 9 body, but keys the
/// node pair off clip 0x19 / slot 2 and tests the request bit `0x100` rather
/// than bit 0. Same tail: `GpEnemy.hp` / `.reactionFlags` pick the next
/// `field_0` whenever the request bit is up.
void func_actor_401000_8013CEF0(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0;
        work->field_8D0.radius = 0x1AE;
        work->field_B50.flags &= 0x7FFF;
        work->field_A10.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_898        = 2;
        work->field_89E        = 0x19;
        work->field_8A2        = 0x10;
        work->field_8B0        = 0;
        work->field_8AE        = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= 0x4000;
    }
    func_actor_401000_80132EF0(arg0);
    func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_8F0, 0xC);
    func_actor_401000_801323EC(arg0->field_2C->coords, (GpRec18*)work->field_A30, 0xC);
    arg0->field_2C->coords->flg = 0;
    if (work->flags_68.half & 0x100) {
        work->field_8D0.flags |= 0x4000;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Walks `p` up its parent chain to `gGfxViewCoord`, transforming `out` by each
/// coordinate; `out` is left unchanged if the chain ends before the view. Same
/// body as `Actor401300_TransformToView` / `Actor01900_TransformToView`.
static __inline__ void Actor401000_TransformToView(GsCOORDINATE2* p, SVECTOR* out)
{
    SVECTOR        sv;
    VECTOR         vec;
    s32            flag;
    SVECTOR*       svp   = &sv;
    GsCOORDINATE2* view  = &gGfxViewCoord;
    VECTOR*        vecp  = &vec;
    s32*           flagp = &flag;
    sv.vx                = out->vx;
    sv.vy                = out->vy;
    sv.vz                = out->vz;
loop:
    if (p->sub != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            __asm__ volatile("nop; nop; .word 0x4A480012");
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->sub;
            goto loop;
        }
        out->vx = sv.vx;
        out->vy = sv.vy;
        out->vz = sv.vz;
    }
}

/// The actor's state handlers, indexed by `Actor401000Work::field_0`. Copied to
/// the frame by `func_actor_401000_8013D044` before the dispatch, so the
/// handler may overwrite the live table entry.
static const Actor401000StateTable D_actor_401000_80131FF4 = { {
    func_actor_401000_8013DB10,
    func_actor_401000_8013DB6C,
    func_actor_401000_8013DC14,
    func_actor_401000_8013DCC0,
    func_actor_401000_80134DB4,
    func_actor_401000_8013DD6C,
    func_actor_401000_80134F98,
    func_actor_401000_80135AA4,
    func_actor_401000_801365C8,
    func_actor_401000_80136E20,
    func_actor_401000_801374D4,
    func_actor_401000_801378DC,
    func_actor_401000_801380B8,
    func_actor_401000_801383F0,
    func_actor_401000_801385B0,
    func_actor_401000_8013DE24,
    func_actor_401000_8013DEC8,
    func_actor_401000_8013DF6C,
    NULL,
    func_actor_401000_801388F4,
    func_actor_401000_80138BB4,
    func_actor_401000_80138D08,
    func_actor_401000_80138F50,
    func_actor_401000_8013922C,
    func_actor_401000_801394EC,
    func_actor_401000_8013A0C8,
    func_actor_401000_80139D10,
    func_actor_401000_8013A5F0,
    func_actor_401000_8013A930,
    func_actor_401000_8013B1E4,
    func_actor_401000_8013C46C,
    func_actor_401000_8013CD9C,
    func_actor_401000_8013CEF0,
    func_actor_401000_8013B61C,
} };

/// The actor's per-frame tick, the 401000 twin of `func_actor_401300_801405DC`:
/// copy the state table to the frame, advance the root coordinate and hand it
/// to `Gp_UpdateActorColor`, then run the `D_801153F4` arm. Arms 1 and 2 only
/// drop the two obstacle records (2 also opening the `field_C` draw to 0x80)
/// and return; arm 0 falls through into the common tail, which counts
/// `field_BE8` down into `func_actor_401000_80133D50`, carries a new
/// `field_0` into `field_2`/`field_4` (snapping the root to `field_BF8` on a
/// 0xB/0xD transition), dispatches through the table, re-flags `field_8D0`,
/// then appends the view-space position to the `field_C2C` ring and publishes
/// it as the enemy's `field_1C` while the `field_89E` clip is 0x14/0x15.
void func_actor_401000_8013D044(GpEnemy* enemy, Actor401000* actor)
{
    VECTOR                  pos;
    Actor401000StateTable   states;
    Actor401000Work*        work;
    Actor401000ViewScratch* scratch;
    Actor401000ViewScratch* head;
    s32                     state;

    work   = actor->field_1C;
    states = D_actor_401000_80131FF4;

    actor->field_2C->coords->flg = 0;
    Gp_UpdateCoord(actor->field_2C->coords);
    pos.vx = actor->field_2C->coords->workm.t[0];
    pos.vy = actor->field_2C->coords->workm.t[1];
    pos.vz = actor->field_2C->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    switch (D_801153F4) {
        case 0:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x21)) {
                actor->field_2C->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->coords->workm.t, 0x180, Gp_State1C->groundShade);
                state = work->field_0;
            }
            if ((state == 0x21) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            break;
        case 1:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x21)) {
                actor->field_2C->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->coords->workm.t, 0x180, Gp_State1C->groundShade);
                state = work->field_0;
            }
            if ((state == 0x21) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            Gp_ClearRec18Occupied((GpRec18*)work->field_A30);
            Gp_ClearRec18Occupied((GpRec18*)work->field_8F0);
            return;
        case 2:
            actor->field_2C->flags = 0x80;
            Gp_ClearRec18Occupied((GpRec18*)work->field_A30);
            Gp_ClearRec18Occupied((GpRec18*)work->field_8F0);
            return;
    }

    head                                      = *(Actor401000ViewScratch**)G_SCRATCH_HEAD;
    *(Actor401000ViewScratch**)G_SCRATCH_HEAD = head - 1;
    scratch                                   = head - 1;

    if (work->field_BE8 > 0) {
        work->field_BE8 = (s16)((u16)work->field_BE8 - 1);
    } else {
        func_actor_401000_80133D50(actor);
    }
    if (work->field_2 != work->field_0) {
        if ((work->field_2 == 0xB) || (work->field_2 == 0xD)) {
            actor->field_2C->coords->coord.t[0] = work->field_BF8.vx;
            actor->field_2C->coords->coord.t[1] = work->field_BF8.vy;
            actor->field_2C->coords->coord.t[2] = work->field_BF8.vz;
            actor->field_2C->coords->flg        = 0;
            Gp_UpdateCoord(actor->field_2C->coords);
        }
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    states.fn[work->field_0](actor);

    state = work->field_0;
    if ((state == 0x15) || (state == 0) || (state == 0x1D) || (state == 0x21)) {
        work->field_8D0.flags &= 0x7FFF;
    } else {
        work->field_8D0.flags |= 0x8000;
    }
    Gp_ClearRec18Occupied((GpRec18*)work->field_A30);
    Gp_ClearRec18Occupied((GpRec18*)work->field_8F0);

    if ((D_801153F2[1] == 1) && (work->field_0 == 0x18)) {
        work->field_0 = 6;
    }

    scratch->pos.vx = 0;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    Actor401000_TransformToView(actor->field_2C->coords + 2, &scratch->pos);

    work->field_C2C[work->field_C7C].vx = scratch->pos.vx;
    work->field_C2C[work->field_C7C].vy = scratch->pos.vy;
    work->field_C2C[work->field_C7C].vz = scratch->pos.vz;

    *(u8**)G_SCRATCH_HEAD += 0x18;
    work->field_C7C        = (u16)work->field_C7C + 1;
    if (work->field_C7C == 7) {
        work->field_C7C = 0;
    }
    if ((u32)((u16)work->field_89E - 0x14) < 2U) {
        enemy->bodyPos.vx = work->field_C2C[work->field_C7C].vx;
        enemy->bodyPos.vy = work->field_C2C[work->field_C7C].vy;
        enemy->bodyPos.vz = work->field_C2C[work->field_C7C].vz;
    } else {
        enemy->bodyPos.vx = scratch->pos.vx;
        enemy->bodyPos.vy = scratch->pos.vy;
        enemy->bodyPos.vz = scratch->pos.vz;
    }
    enemy->coord = &gGfxViewCoord;
}

void func_actor_401000_8013D68C(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_401000/actor_401000", ActorsShared80135df4Table);

s32 func_actor_401000_8013D694(Actor401000* arg0, s32 arg1, Actor401000Msg* arg2)
{
    Actor401000Work* work = arg0->field_1C;

    switch (arg2->field_4) {
        case 0:
            work->field_89E = 0x22;
            break;
        case 1:
            work->field_89E = 0x23;
            break;
        case 2:
            work->field_89E = 0x24;
            break;
        case 3:
            work->field_89E = 0x25;
            break;
        case 4:
            work->field_89E = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/// The four bytes of `.rodata` that sit between this function's jump table and
/// `func_actor_401000_8013DF6C`'s. Both tables came out of one translation unit,
/// where GCC's `.align 3` ahead of the second left this gap; the split puts the
/// second table in `actor_401000_2`, so the gap has to be materialised here to
/// keep the leading rodata the length the package says it is.
const u32 D_actor_401000_801320A0 SECTION(".rodata") = 0;
