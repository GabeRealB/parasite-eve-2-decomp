#include "common.h"

#include <psyq/inline_c.h>
#include "psyq/abs.h"

#include "actors/actor_401800.h"
#include "actors/actors_shared_80133eb8.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// Binds the work block's light and color matrices onto the model object.
/// Same body as `Actor01900_BindMatrices` / `Actor401300_BindMatrices`.
static __inline__ void Actor401800_BindMatrices(Actor401800* actor)
{
    Actor401800Work* work;
    TmdObject*       obj;

    work          = actor->field_1C;
    obj           = actor->field_2C;
    obj->lightMtx = &work->field_B80;
    obj->colorMtx = &work->field_BA0;
}

/// Rebuild `coord`'s Y rotation from its current yaw, uniformly scaled by
/// `scale`. Same body as `Actor01900_RescaleYaw`.
static __inline__ void Actor401800_RescaleYaw(GsCOORDINATE2* coord, s16 scale)
{
    void**                 scratch;
    void*                  head;
    Actor401800RotScratch* blk;
    s16                    ang;
    u16                    m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor401800RotScratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((Actor401800RotScratch*)((u8*)head - 0x34))->m.m[0][0];
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

/// `Actor401800_RescaleYaw` with a separate Y scale.
static __inline__ void Actor401800_RescaleYawY(GsCOORDINATE2* coord, s32 scale, s16 scaleY)
{
    void**                 scratch;
    void*                  head;
    Actor401800RotScratch* blk;
    s16                    ang;
    u16                    m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor401800RotScratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vx = scale;
    blk->scale.vy = scaleY;
    blk->scale.vz = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((Actor401800RotScratch*)((u8*)head - 0x34))->m.m[0][0];
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

/// Rebuild `coord`'s Y rotation from its current yaw at unit scale. Same body
/// as `Actor01900_ResetYaw` / `Actor401300_ResetYaw`.
static __inline__ void Actor401800_ResetYaw(GsCOORDINATE2* coord)
{
    void*                  head;
    Actor401800RotScratch* blk;
    s16                    ang;

    head                    = *(void**)G_SCRATCH_HEAD;
    blk                     = (Actor401800RotScratch*)((u8*)head - 0x34);
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

/// Enemy init: allocates the work block, binds the model matrices, sets up both
/// animation contexts and the three hit/body `GpObj` nodes, then picks the
/// starting state and tint row from the spawn flags and rescales the model.
/// Same body as `Actor01900_Fn02018` / `func_actor_401300_80134454`.
void func_actor_401800_8013423C(GpEnemy* enemy, Actor401800* actor)
{
    SVECTOR          dir;
    VECTOR           pos;
    SVECTOR*         v;
    TmdObject*       obj;
    GsCOORDINATE2*   root;
    Actor401800Work* work;
    GpObj*           body;
    GpObj*           head;
    s32              kind;

    root            = actor->field_2C->coords;
    obj             = actor->field_2C;
    work            = memCalloc(0xC78, 0);
    actor->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)actor);
        return;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    ((Task*)actor)->exitCallback = func_actor_401800_8013E0A0;
    Actor401800_BindMatrices(actor);
    enemy->field_4    = &actor->field_2C->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->field_2C->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags    = 1;
    enemy->reactionFlags = 0;
    enemy->hp            = (s16)D_actor_401800_8013E6F0.hpMax;
    enemy->param         = &D_actor_401800_8013E6F0;
    enemy->recs          = &work->field_8E8;
    func_800B3F84(&((Actor401800AnimWork*)work)->anim, &D_actor_401800_80155938, obj,
                  ((Actor401800AnimWork*)work)->pad_328, ((Actor401800AnimWork*)work)->slots);
    func_800B3F84(&((Actor401800AnimWork*)work)->blendAnim, &D_actor_401800_80155938, obj,
                  ((Actor401800AnimWork*)work)->pad_764, ((Actor401800AnimWork*)work)->blendSlots);
    work->field_898 = 2;
    work->field_89E = 2;
    work->field_89A = 0;
    work->field_8B0 = 0;
    work->field_8AE = 0;
    work->field_8A4 = 0x10;
    work->field_8A2 = 0x10;
    if ((s16)((enemy->placeKey >> 12) & 1) == 1) {
        work->field_8A4++;
    } else {
        work->field_8A4--;
    }
    func_actor_401800_80133EB8(actor);

    work->field_A08.ctx.recs = &work->field_A28;
    work->field_A08.coord    = root;
    work->field_A08.pos.vx   = 0;
    work->field_A08.pos.vy   = -0xAC;
    work->field_A08.pos.vz   = 0;
    work->field_A08.key      = 0x30012;
    work->field_A08.radius   = 0x12C;
    work->field_A08.flags    = 1;
    Gp_LinkObj(2, &work->field_A08);
    work->field_BE0        = 0;
    work->field_A08.flags |= 0x4000;
    Gp_InitRec18Table(work->field_A08.ctx.recs, 0xC, 0);

    body           = &work->field_8C8;
    body->coord    = &actor->field_2C->coords[2];
    body->ctx.recs = &work->field_8E8;
    body->pos.vx   = 0;
    body->pos.vy   = 0;
    body->pos.vz   = 0;
    body->key      = 0x30000;
    body->radius   = 0x12C;
    body->flags    = 1;
    Gp_LinkObj(2, body);
    body->flags |= 0x8000;
    Gp_InitRec18Table(body->ctx.recs, 0xC, 0);

    dir.vx         = 0;
    dir.vy         = 0;
    dir.vz         = 0;
    head           = &work->field_B48;
    head->coord    = &actor->field_2C->coords[6];
    head->ctx.recs = &work->field_B68;
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
    gte_gpf12_real();
    gte_stsv(v);
    work->field_C[1].x = actor->field_2C->coords->coord.t[0] + dir.vx;
    work->field_C[1].z = actor->field_2C->coords->coord.t[2] + dir.vz;

    ((Task*)actor)->msgTable = &D_actor_401800_80155A80;
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
    kind                       = actor->field_36;
    switch (kind & 0xF) {
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
            work->field_C08 = D_actor_401800_8013E700[0].field_0;
            work->field_C0A = D_actor_401800_8013E700[0].field_2;
            work->field_C0C = D_actor_401800_8013E700[0].field_4;
            work->field_C0E = D_actor_401800_8013E700[0].field_6;
            break;
        case 1:
            work->field_C08 = D_actor_401800_8013E700[2].field_0;
            work->field_C0A = D_actor_401800_8013E700[2].field_2;
            work->field_C0C = D_actor_401800_8013E700[2].field_4;
            work->field_C0E = D_actor_401800_8013E700[2].field_6;
            break;
        case 0:
        default:
            work->field_C08 = D_actor_401800_8013E700[1].field_0;
            work->field_C0A = D_actor_401800_8013E700[1].field_2;
            work->field_C0C = D_actor_401800_8013E700[1].field_4;
            work->field_C0E = D_actor_401800_8013E700[1].field_6;
            break;
    }

    Actor401800_RescaleYaw(actor->field_2C->coords, 0x1194);
    work->field_C74 = 0;
    ((Task*)actor)->state++;
}

/// Picks one of twelve hit positions out of `D_actor_401800_80155A20` by damage
/// magnitude `arg1`, copies it to an 8-byte scratch vector, then arms the
/// `field_8B8` spawn record with the actor's part-1 coordinate as its anchor
/// and hands it to `func_800FDB18` to spawn effect `Gp_GetIdParam1(arg2)`.
/// Scale 0x300 and count 2 are the effect's; the record's coordinate comes from
/// `TmdObject.coords[sc->pad]`, so the effect follows the part the table entry
/// names. Same body as `Actor00100_Fn03340` (actors/lib/actor_400100_damage.c).
void func_actor_401800_801348A8(Actor401800* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         sc;
    s32              mag;
    Actor401800Work* work;

    sc   = (SVECTOR*)(*(u32*)G_SCRATCH_HEAD -= 8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = (Actor401800Work*)arg0->field_1C;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_401800_80155A20[0];
                break;
            case 1:
                *sc = D_actor_401800_80155A20[1];
                break;
            case 2:
                *sc = D_actor_401800_80155A20[2];
                break;
            case 3:
                *sc = D_actor_401800_80155A20[3];
                break;
            default:
                *sc = D_actor_401800_80155A20[4];
                break;
        }
    } else if (mag > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_401800_80155A20[5];
                break;
            case 1:
                *sc = D_actor_401800_80155A20[6];
                break;
            default:
                *sc = D_actor_401800_80155A20[7];
                break;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_401800_80155A20[8];
        } else {
            *sc = D_actor_401800_80155A20[9];
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_401800_80155A20[10];
        } else {
            *sc = D_actor_401800_80155A20[11];
        }
    }
    work->field_8B8.coord      = &arg0->field_2C->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->field_2C->coords[sc->pad], sc, &work->field_8B8);
    *(u32*)G_SCRATCH_HEAD += 8;
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_80134C94);

/// Walk-state body, split on the live flag. Live: hand the model back to
/// `Tmd_AllocBuffers`, restart the 0x898 slot, ramp `field_8A2` to 0x10, remap
/// the state at 0x89E (11 -> 0x17, 12/25 -> 0x18, anything else -> 0x17) and
/// hold the two `field_5A` countdowns open until the step helper has run its
/// course, then drop `field_8A2` to 0x20. Dead: clear the model's coordinate
/// flag, halve `field_8A2` with the 1 / -1 wrap, and once `Gp_TickObjFlag2`
/// reports 1 clear the enemy's node bit 1 and move to state 0x11.
/// Same body as `func_actor_401300_80135DDC`.
void func_actor_401800_80135DAC(Actor401800* arg0)
{
    Actor401800Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;
    TmdObject*       tmd;

    if (work->field_4 != 0) {
        tmd               = arg0->field_2C;
        enemy->node.flags = 0;
        tmd->flags        = 0;
        Tmd_AllocBuffers(tmd);
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_A08.flags |= 0x4000;
        if (work->field_89E == 11) {
            work->field_89E = 0x17;
        } else if (work->field_89E == 12 || work->field_89E == 25) {
            work->field_89E = 0x18;
        }
        if ((u16)(work->field_89E - 0x17) >= 2) {
            work->field_89E = 0x17;
        }
        do {
            func_actor_401800_80133EB8(arg0);
        } while (!(work->field_89E == 0x17 && (work->field_5A & 0x3FF) >= 6) &&
                 !(work->field_89E == 0x18 && (work->field_5A & 0x3FF) >= 9));
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
    func_actor_401800_80133EB8(arg0);
    if (Gp_TickObjFlag2((GpObj5D*)enemy) == 1) {
        enemy->reactionFlags &= ~2;
        work->field_0         = 0x11;
    }
}

/// Inline bodies behind `func_actor_401800_80135F58`, which is the same
/// aim-then-rescale shape as `Actor01900_Fn080A8` / `func_actor_401300_80136238`
/// with these inlined rather than called. Splitting the `G_SCRATCH_HEAD` loads
/// out this way is what keeps each access out of a register CSE would share.
static __inline__ void Actor401800_ConfigPositionDelta(PlayerStatus* config, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = config->coordMtx->t[0] - coord->coord.t[0];
    pos->vy = config->coordMtx->t[1] - coord->coord.t[1];
    pos->vz = config->coordMtx->t[2] - coord->coord.t[2];
}

/// Wraps a 12-bit angle difference into `[-0x800, 0x800]`.
static __inline__ s16 Actor401800_NormalizeYaw(s16 input)
{
    s16 value = input;
    if (input < 0) {
        while (1) {
            if (value >= -0x800)
                break;
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800)
                break;
            value -= 0x1000;
        }
    }
    return value;
}

/// Yaw from the actor's facing to the player, wrapped; `pos` receives the offset.
static __inline__ s16 Actor401800_PositionYaw(Actor401800* actor, SVECTOR* pos, PlayerStatus* config)
{
    GsCOORDINATE2* coord;
    s32            angle;

    Actor401800_ConfigPositionDelta(config, actor->field_2C->coords, pos);
    coord = actor->field_2C->coords;
    angle = ratan2(pos->vx, pos->vz);
    return Actor401800_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Aim the actor at the player and rescale its root coordinate. On the live
/// flag it resets the model buffers and hands back the pose the actor was
/// running; otherwise it takes a 0x10 scratch for the player offset and the
/// clamped turn, folds the turn into the coordinate's Y rotation and rebuilds
/// the matrix from the new yaw at scale 0x1194.
/// Same body as `Actor01900_Fn080A8`, with the aim and rescale helpers inlined.
void func_actor_401800_80135F58(Actor401800* arg0)
{
    Actor401800Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401800AimScratch* aim;

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
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        func_actor_401800_80133EB8(arg0);
        work->field_8C8.radius = 0x12C;
        Gp_ArmStateF0(1);
        return;
    }
    *(Actor401800AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401800AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->coords->flg               = 0;
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
    aim->angle      = Actor401800_PositionYaw(arg0, &aim->delta, &Player_Status);
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
    Actor401800_RescaleYaw(arg0->field_2C->coords, 0x1194);
    func_actor_401800_80133EB8(arg0);
    *(Actor401800AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Push the root coordinate out of a `GpRec18` table: take a 0x34 scratch, seed
/// its position from the second coordinate, then walk the records until `count`
/// or a zero `key`. Each kind 0x10000 / 0x30000 record contributes half its
/// offset along X and Z, normalised to length 0x96 first when it is longer than
/// that; `hit` reports whether one was seen.
/// Same body as `Actor01900_Fn03FF8` / `func_actor_401300_80132910`, with the
/// coordinate update written out in both arms of the length test.
s32 func_actor_401800_8013629C(Actor401800* arg0, GpRec18* recs, s16 count)
{
    Actor401800PushScratch* head;
    Actor401800PushScratch* s;
    Actor401800PushScratch* blk;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    arg0->field_2C->coords[1].flg             = 0;
    head                                      = *(Actor401800PushScratch**)G_SCRATCH_HEAD;
    blk                                       = head - 1;
    *(Actor401800PushScratch**)G_SCRATCH_HEAD = blk;
    s                                         = blk;
    Gp_UpdateCoord(&arg0->field_2C->coords[1]);
    s->pos.vx = arg0->field_2C->coords[1].workm.t[0];
    s->pos.vy = arg0->field_2C->coords[1].workm.t[1];
    s->pos.vz = arg0->field_2C->coords[1].workm.t[2];
    s->hit    = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            Gp_MakeDirOffset(&s->pos, (GpDirSrc*)&recs[s->i], &s->offset);
            s->len = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->len = SquareRoot0(s->len);
            if (s->len >= 0x96) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0x96);
                gte_ldsv(&s->offset);
                __asm__ volatile("nop; nop; .word 0x4B98003D");
                gte_stsv(&s->offset);
                arg0->field_2C->coords->coord.t[0] += s->offset.vx / 2;
                arg0->field_2C->coords->coord.t[2] += s->offset.vz / 2;
            } else {
                arg0->field_2C->coords->coord.t[0] += s->offset.vx / 2;
                arg0->field_2C->coords->coord.t[2] += s->offset.vz / 2;
            }
            arg0->field_2C->coords->flg = 0;
        }
    }
    *(Actor401800PushScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}

/// `Actor401800_MoveForwardNonzero` with the step applied through `vec`
/// rather than a second name for it. Same body as `Actor01900_StepForwardHead`.
static __inline__ void Actor401800_StepForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(vec);
            gte_gpf12_real();
            gte_stsv(vec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor401800_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor401800RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    blk                                           = (Actor401800RangeScratch*)(head - 0xC);
    ((Actor401800RangeScratch*)(head - 0xC))->dx  = d->vx;
    *(Actor401800RangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor401800RangeScratch*)(head - 0xC))->dx *= ((Actor401800RangeScratch*)(head - 0xC))->dx;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)G_SCRATCH_HEAD                         = head;
    ret                                           = ((Actor401800RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

static __inline__ s32 Actor401800_ChaseOutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor401800RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    ((Actor401800RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor401800RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor401800RangeScratch*)(head - 0xC))->dx *= ((Actor401800RangeScratch*)(head - 0xC))->dx;
    *(Actor401800RangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)G_SCRATCH_HEAD                         = head;
    ret                                           = ((Actor401800RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// Step `coord` `amount` units along its local Z axis unless movement is
/// frozen. Same body as `Actor01900_MoveForward`.
static __inline__ void Actor401800_MoveForwardNonzero(GsCOORDINATE2* coord, s16 amount)
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

void func_actor_401800_80136560(Actor401800* arg0)
{
    Actor401800Work*         work;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           turnCoord;
    GsCOORDINATE2*           facing;
    void**                   scratch;
    u8*                      head;
    u8*                      block;
    s16                      animRate;
    Actor401800ChaseScratch* s;
    s32                      kind;

    kind = arg0->field_36;
    work = arg0->field_1C;
    if ((kind & 0xF0) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 1;
        work->field_89E        = 3;
        work->field_B48.flags &= 0x7FFF;
        animRate               = work->field_8A4;
        TOUCH_REG(work);
        work->field_89A        = 0;
        work->field_8A2        = animRate;
        work->field_A08.flags |= 0x4000;
        ActorsShared80133eb8((ActorsShared80133eb8Actor*)arg0);
        work->field_C1C = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        return;
    }
    work->field_6++;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = head - 0x10;
    *scratch = block;
    s        = (Actor401800ChaseScratch*)block;
    if (func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC) != 1) {
        func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
    }
    coord                                               = arg0->field_2C->coords;
    ((Actor401800ChaseScratch*)(head - 0x10))->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    s->delta.vy                                         = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    s->delta.vz                                         = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    arg0->field_2C->coords->flg                         = 0;
    ActorsShared80133eb8((ActorsShared80133eb8Actor*)arg0);
    s->playerYaw                                        = ratan2(-((TmdObject*)((Task*)gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                                                                 ((TmdObject*)((Task*)gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
    coord                                               = arg0->field_2C->coords;
    ((Actor401800ChaseScratch*)(head - 0x10))->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    s->delta.vy                                         = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    s->delta.vz                                         = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    s->yaw                                              = ratan2(s->delta.vx, s->delta.vz) + 0x800;
    s->yaw                                              = Actor401800_NormalizeYaw(s->yaw);
    turnCoord                                           = arg0->field_2C->coords;
    s->turn                                             = Actor401800_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-turnCoord->coord.m[2][0], turnCoord->coord.m[2][2]));
    work->field_8AE                                     = s->turn;
    if (abs(s->yaw - s->playerYaw) < 0x44) {
        if (((s16)work->field_C0C + work->field_C1E / 2) < work->field_6) {
            if (abs(s->turn) < 0x80) {
                if (Actor401800_ChaseOutOfRange(&s->delta, 0x708) && func_actor_401800_80133918(arg0) != 1) {
                    work->field_0 = 0xA;
                }
            }
        }
    }
    if (s->turn < 0x200) {
        if (!Actor401800_ChaseOutOfRange(&s->delta, 0x44C) && func_actor_401800_80133918(arg0) != 1 && work->field_8C2 == 0) {
            work->field_0 = 0xB;
        }
    }
    if (s->turn > 0x30) {
        s->turn = 0x30;
    }
    if (s->turn < -0x30) {
        s->turn = -0x30;
    }
    facing   = arg0->field_2C->coords;
    s->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, s->turn, 1);
    Actor401800_RescaleYaw(arg0->field_2C->coords, 0x1194);
    arg0->field_2C->coords->flg = 0;
    if (work->field_89E == 3) {
        if (work->field_89A == 0) {
            if ((s16)func_actor_401800_80133558(arg0->field_2C->coords, 0x12C, ((work->field_8A4 + 2) * 0x42) / 18) != 0) {
                Actor401800_MoveForwardNonzero(arg0->field_2C->coords, ((work->field_8A4 + 2) * 0x42) / 18);
            }
        } else if ((s16)func_actor_401800_80133558(arg0->field_2C->coords, 0x12C, (((work->field_8A4 + 2) * 0x42) / 18) >> 2) != 0) {
            Actor401800_MoveForwardNonzero(arg0->field_2C->coords, (((work->field_8A4 + 2) * 0x42) / 18) >> 2);
        }
    } else if (work->field_68 & 1) {
        work->field_89E = 3;
        work->field_898 = 1;
    }
    if (work->field_8C2 != 0) {
        work->field_8C2--;
    }
    *(u8**)G_SCRATCH_HEAD += 0x10;
}

/// Chase body that steers the actor along its own local Z while the step
/// countdown runs: takes a 0x10 scratch for the player offset and the yaws, and
/// on the live flag resets the model buffers, arms the walk state and seeds
/// `field_BFE` to 8. Otherwise it re-aims the actor at the player once
/// `field_8` has run 7 frames, clamps the turn toward the player into
/// `s->angle`, rebuilds the root coordinate at scale 0x1194 and steps the actor
/// by `field_BFC` (the step countdown, halved while `field_89A` is set, forced
/// to 2 while `field_8` is live) while `func_actor_401800_80133558` reports the
/// path clear. `field_BFE` then walks 8 -> -1 -> 0 against `field_8A2`, and at
/// 0 the fifth `field_6` frame picks `field_0` from the yaw offset to the
/// player. Same step ramp as `Actor01900_Fn04D14`, with the aim and the step
/// helper inlined.
void func_actor_401800_80136EAC(Actor401800* arg0)
{
    Actor401800Work*         work;
    Actor401800ChaseScratch* head;
    Actor401800ChaseScratch* s;
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
        work->field_8C8.radius = 0x96;
        work->field_898        = 1;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        func_actor_401800_80133EB8(arg0);
        work->field_BFE         = 8;
        work->field_6           = 0;
        work->field_8           = 0;
        D_actor_401800_80155AC0 = 0;
        work->field_C1C++;
        return;
    }
    head                                       = *(Actor401800ChaseScratch**)G_SCRATCH_HEAD;
    *(Actor401800ChaseScratch**)G_SCRATCH_HEAD = head - 1;
    s                                          = head - 1;
    arg0->field_2C->coords->flg                = 0;
    func_actor_401800_80133EB8(arg0);
    if (func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC) != 0) {
        work->field_8++;
    } else {
        func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
    }
    Actor401800_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &s->delta);
    if (work->field_8 >= 7) {
        s->playerYaw  = ratan2(-((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                               ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
        s->yaw        = ratan2(s->delta.vx, s->delta.vz) + 0x800;
        s->yaw        = Actor401800_NormalizeYaw(s->yaw);
        work->field_0 = 0x1A;
    }
    coord   = arg0->field_2C->coords;
    s->turn = Actor401800_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    turn    = s->turn;
    if (turn >= 0) {
        diffPos = turn - 1000;
        if (((diffPos < 0) ? -diffPos : diffPos) < 0x60) {
            s->angle = s->turn - 1000;
        } else if (diffPos > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    } else {
        diffNeg = turn + 1000;
        if (((diffNeg < 0) ? -diffNeg : diffNeg) < 0x60) {
            s->angle = s->turn + 1000;
        } else if (diffNeg > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    }
    facing    = arg0->field_2C->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, s->angle, 1);
    Actor401800_RescaleYaw(arg0->field_2C->coords, 0x1194);
    coord                       = arg0->field_2C->coords;
    work->field_8AE             = Actor401800_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    arg0->field_2C->coords->flg = 0;
    work->field_BFC             = work->field_8A2 * 8;
    if (work->field_89A != 0) {
        work->field_BFC = work->field_BFC >> 1;
    }
    if (work->field_8 != 0) {
        work->field_BFC = 2;
    }
    if ((s16)func_actor_401800_80133558(arg0->field_2C->coords, 0x12C, work->field_BFC) != 0) {
        Actor401800_MoveForwardNonzero(arg0->field_2C->coords, work->field_BFC);
    }
    D_actor_401800_80155AC0 += work->field_BFC;
    if (work->field_BFE == 8 && work->field_8A2 >= 0x18) {
        work->field_BFE = -1;
    }
    if (work->field_BFE == -1 && work->field_8A2 == 0x12) {
        work->field_BFE = 0;
        work->field_6   = 0;
    }
    if (work->field_BFE == 0) {
        if (++work->field_6 == 5) {
            s->playerYaw = ratan2(-((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                                  ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
            Actor401800_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &s->delta);
            s->yaw = ratan2(s->delta.vx, s->delta.vz) + 0x800;
            yaw    = Actor401800_NormalizeYaw(s->yaw);
            s->yaw = yaw;
            yaw    = yaw - s->playerYaw;
            if (yaw < 0) {
                yaw = -yaw;
            }
            if (yaw >= 0x401 && func_actor_401800_80133918(arg0) != 1 && work->field_8C2 == 0) {
                work->field_0 = 0xB;
            } else {
                work->field_0 = 0x1A;
                work->field_2 = -1;
            }
        }
    }
    work->field_8A2 += work->field_BFE;
    if (work->field_8C2 != 0) {
        work->field_8C2--;
    }
    *(Actor401800ChaseScratch**)G_SCRATCH_HEAD += 1;
}

/// Chase body: takes a 0x10 scratch for the player offset and the heading it
/// folds into the root coordinate. On the live flag it resets the model
/// buffers, arms the walk state and stores the facing yaw `field_BF8` along
/// with the target `field_BFA` — the facing plus twice the wrapped turn toward
/// the player. Otherwise it picks `field_0` from the `field_C1C` contact range
/// and the `field_8C2` countdown, steers `field_BF8` 0x89 a frame toward
/// `field_BFA`, rebuilds the root coordinate at scale 0x1194 and steps the
/// actor 0x28 / 0x14 along its local Z while `func_actor_401800_80133558`
/// reports the path clear. Same body as `func_actor_401300_801376E4` /
/// `Actor01900_Fn0551C`, with the step helper's clear-path test added.
void func_actor_401800_80137714(Actor401800* arg0)
{
    Actor401800Work*         work;
    Actor401800ChaseScratch* head;
    Actor401800ChaseScratch* s;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           facing;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        head                                       = *(Actor401800ChaseScratch**)G_SCRATCH_HEAD;
        obj                                        = arg0->field_2C;
        *(Actor401800ChaseScratch**)G_SCRATCH_HEAD = head - 1;
        s                                          = head - 1;
        arg0->field_20->node.flags                 = 0;
        obj->flags                                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_8AE        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        func_actor_401800_80133EB8(arg0);
        Actor401800_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &s->delta);
        coord                                       = arg0->field_2C->coords;
        s->turn                                     = Actor401800_NormalizeYaw(ratan2(head[-1].delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing                                      = arg0->field_2C->coords;
        s->angle                                    = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_BF8                             = s->angle;
        work->field_BFA                             = s->angle + (u16)s->turn * 2;
        *(Actor401800ChaseScratch**)G_SCRATCH_HEAD += 1;
        return;
    }
    head                                       = *(Actor401800ChaseScratch**)G_SCRATCH_HEAD;
    *(Actor401800ChaseScratch**)G_SCRATCH_HEAD = head - 1;
    s                                          = head - 1;
    func_actor_401800_80133EB8(arg0);
    Actor401800_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &s->delta);
    if (work->field_BF8 == work->field_BFA) {
        if (work->field_C1C < 2 || Actor401800_OutOfRange(&s->delta, 0x384)) {
            work->field_0 = 8;
        } else if (func_actor_401800_80133918(arg0) != 1 && work->field_8C2 == 0) {
            work->field_0 = 0xB;
        } else {
            work->field_0 = 8;
        }
    }
    if (work->field_BF8 > work->field_BFA) {
        work->field_BF8 -= 0x89;
        if (work->field_BF8 < work->field_BFA) {
            work->field_BF8 = work->field_BFA;
        }
    }
    if (work->field_BF8 < work->field_BFA) {
        work->field_BF8 += 0x89;
        if (work->field_BF8 > work->field_BFA) {
            work->field_BF8 = work->field_BFA;
        }
    }
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, work->field_BF8, 1);
    Actor401800_RescaleYaw(arg0->field_2C->coords, 0x1194);
    arg0->field_2C->coords->flg = 0;
    if (work->field_89A == 0) {
        if ((s16)func_actor_401800_80133558(arg0->field_2C->coords, 0x12C, 0x28) != 0) {
            Actor401800_StepForward(arg0->field_2C->coords, 0x28);
        }
    } else {
        if ((s16)func_actor_401800_80133558(arg0->field_2C->coords, 0x12C, 0x14) != 0) {
            Actor401800_StepForward(arg0->field_2C->coords, 0x14);
        }
    }
    if (func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC) != 1) {
        func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
    }
    if (work->field_8C2 != 0) {
        work->field_8C2--;
    }
    *(Actor401800ChaseScratch**)G_SCRATCH_HEAD += 1;
}

/// Live-actor swing. On the live flag it resets the model buffers, takes the
/// swing side `field_C00` from `Gp_LcgState`, offsets the player bearing in
/// `field_BE8` by +-0x171 on the first frame and stores the 0x15 / 0x14 state
/// the walk body runs. The `field_BE8` yaw is then rebuilt into a direction and
/// GPF-scaled by `field_C02` into the offset added to the root coordinate while
/// `field_6` sits in 0xC..0x15 — halving the scale once the `field_A28` contact
/// test fires. The step counter moves the actor to state 7 at 0x1E; a kind
/// 0x10 actor reloads 0x1E instead. Same body as `Actor01900_Fn05B4C`, with the
/// position delta inlined.
void func_actor_401800_80137DDC(Actor401800* arg0)
{
    Actor401800Work*       work;
    Actor401800AimScratch* head;
    Actor401800AimScratch* aim;
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
    head                                     = *(Actor401800AimScratch**)G_SCRATCH_HEAD;
    *(Actor401800AimScratch**)G_SCRATCH_HEAD = head - 1;
    aim                                      = head - 1;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x96;
        work->field_6          = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        Actor401800_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &aim->delta);
        aim->angle = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_C00 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C00 = 1;
            } else {
                work->field_C00 = -1;
            }
        }
        if (work->field_C00 == 1) {
            work->field_89E = 0x15;
            if (work->field_C1E == 0) {
                angle      = aim->angle + 0x171;
                aim->angle = work->field_C0A + angle;
            } else {
                aim->angle += work->field_C0A;
            }
            work->field_C00 = -1;
        } else {
            work->field_89E = 0x14;
            if (work->field_C1E == 0) {
                angle      = aim->angle - 0x171;
                aim->angle = angle - work->field_C0A;
            } else {
                aim->angle -= work->field_C0A;
            }
            work->field_C00 = 1;
        }
        work->field_898 = 1;
        work->field_8A2 = 0xC;
        work->field_89A = 0;
        func_actor_401800_80133EB8(arg0);
        Gfx_RotMatrixY(&mat, aim->angle, 1);
        dir = &work->field_BE8;
        Gfx_MatrixCol2(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_C02 = 0xDE;
        work->field_C1E++;
    }
    arg0->field_2C->coords->flg = 0;
    func_actor_401800_80133EB8(arg0);
    arg0->field_2C->coords->flg = 0;
    if (work->field_89A == 0) {
        gte_lddp(work->field_C02);
        gte_ldsv(&work->field_BE8);
        gte_gpf12_real();
        gte_stsv(aim);
    } else {
        gte_lddp(work->field_C02 >> 1);
        gte_ldsv(&work->field_BE8);
        gte_gpf12_real();
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = arg0->field_2C->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->field_2C->coords;
        coord->coord.t[2] += aim->delta.vz;
        if (func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC) == 1) {
            work->field_C02 >>= 1;
        }
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
    }
    *(Actor401800AimScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_801381E4);

/// Live-actor body: arms the animation slots and the two `field_8C8` /
/// `field_A08` nodes, then aims the actor at the `gameGetPtrSlot(3)` task's
/// root position — the XZ offset normalized by `VectorNormalSS` and GPF-scaled
/// by 0x3E8, the heading taken through `ratan2` — sends it as message 0x3E9
/// and spawns the 0xC/8/0x8F pad-lerp. On work flag bit 0 while `field_89E` is
/// 5, restarts the actor's model (`field_0 = 0xD`, the 0x8B8 effect record for
/// the second coordinate). Same shape as `func_actor_401300_80138800`.
void func_actor_401800_80138C28(Actor401800* arg0)
{
    SVECTOR          dir;
    Actor401800Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;
    Task*            player;
    SVECTOR*         pdir;

    if (work->field_4 != 0) {
        player                                   = gameGetPtrSlot(3);
        work->field_8C8.radius                   = 0x12C;
        work->field_B48.flags                   &= 0x7FFF;
        work->field_A08.flags                   |= 0x4000;
        enemy->node.flags                        = 0;
        work->field_898                          = 1;
        work->field_8A2                          = 0x10;
        work->field_89E                          = 5;
        ((TmdObject*)player->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)player->extra)->coords);
        D_actor_401800_80155AD8.field_0.vx = ((TmdObject*)player->extra)->coords->coord.t[0];
        D_actor_401800_80155AD8.field_0.vy = ((TmdObject*)player->extra)->coords->coord.t[1];
        D_actor_401800_80155AD8.field_0.vz = ((TmdObject*)player->extra)->coords->coord.t[2];
        pdir                               = &dir;
        dir.vx                             = ((GpCoordXZ*)arg0->field_2C->coords)->field_18 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_18;
        dir.vy                             = 0;
        dir.vz                             = ((GpCoordXZ*)arg0->field_2C->coords)->field_20 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_20;
        VectorNormalSS(pdir, pdir);
        gte_lddp(0x3E8);
        gte_ldsv(pdir);
        gte_gpf12_real();
        gte_stsv(pdir);
        arg0->field_2C->coords->coord.t[0]  = ((TmdObject*)player->extra)->coords->coord.t[0] + dir.vx;
        arg0->field_2C->coords->coord.t[2]  = ((TmdObject*)player->extra)->coords->coord.t[2] + dir.vz;
        arg0->field_2C->coords->flg         = 0;
        D_actor_401800_80155AD8.field_10.vx = 0;
        D_actor_401800_80155AD8.field_10.vy = ratan2(dir.vx, dir.vz);
        D_actor_401800_80155AD8.field_10.vz = 0;
        Gp_DispatchMsg(player, 0x3E9, (s32)&D_actor_401800_80155AD8, 0);
        Gp_SpawnPadLerp(0xC, 8, 0x8F);
    }
    func_actor_401800_80133EB8(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->coords[2].coord, -0x80, 0);
    arg0->field_2C->coords[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->coords[2]);
    Gfx_RotMatrixX(&arg0->field_2C->coords[3].coord, -0x80, 0);
    arg0->field_2C->coords[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->coords[3]);
    if (work->field_89E == 5 && (work->field_68 & 1)) {
        work->field_0              = 0xD;
        work->field_8B8.coord      = &arg0->field_2C->coords[1];
        work->field_8B8.spawnArgLo = 0x200;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->field_2C->coords[5], 0, &work->field_8B8);
    }
}

/// On the live-actor flag, raises the three animation slots, sends the `0x3FF`
/// animation record and the `0x3F9` object pair to the `gameGetPtrSlot(3)`
/// task, then spawns the 5/0xFF/8 pad-lerp. On work flag bit 0, restarts the
/// actor's model (`field_0 = 0xE`, the 0x8B8 effect record for the second
/// coordinate) and finally copies the `field_5A` clip id into `field_894` and
/// rebuilds the four coordinate parts the actor draws from.
void func_actor_401800_80138F5C(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;
    GpAnimArg*       msg;
    void*            player;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 6;
        work->field_898 = 2;
        msg             = &D_actor_401800_80155A0C;
        msg->field_4    = 2;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)msg, 0);
        player = gameGetPtrSlot(3);
        Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 0), 0);
        Gp_SpawnPadLerp(5, 0xFF, 8);
    }
    if (work->field_68 & 1) {
        work->field_0              = 0xE;
        work->field_8B8.coord      = &arg0->field_2C->coords[1];
        work->field_8B8.spawnArgLo = 0x200;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->field_2C->coords[5], 0, &work->field_8B8);
    }
    work->field_894 = work->field_5A & 0x3FF;
    func_actor_401800_80133EB8(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->coords[2].coord, -0x80, 0);
    arg0->field_2C->coords[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->coords[3]);
    Gfx_RotMatrixX(&arg0->field_2C->coords[3].coord, -0x80, 0);
    arg0->field_2C->coords[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->coords[2]);
}

/// Per-frame body of the live actor while it walks: on work flag bit 0 it
/// raises the `0x10`/7/2 render slots, re-sends the `0x3FF` animation record
/// with clip 3 to the `gameGetPtrSlot(3)` task and seeds the walk step
/// `field_C04` to -0x78; otherwise, while the `field_5A` clip is one of
/// `0x10..0x16`, it advances the actor along its own local Z by `field_C04`
/// once `func_actor_401800_80133558` says the path is still clear and halves
/// that step each time the `field_A28` contact fires. Both paths then tick the
/// animation, and — on work bit 0 — pick `field_0` from the enemy's state byte
/// (`6`, or `0xA` when the enemy is not the one `func_actor_401800_80133918`
/// reports) and release the `0x3F1` message once.
void func_actor_401800_80139118(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;
    PlayerStatus*    config;
    u8               kind;

    work   = arg0->field_1C;
    enemy  = arg0->field_20;
    config = &Player_Status;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 7;
        work->field_898 = 2;
        func_actor_401800_80133EB8(arg0);
        D_actor_401800_80155A0C.field_4 = 3;
        if (config->hp > 0) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&D_actor_401800_80155A0C, 0);
        }
        work->field_C04        = -0x78;
        work->field_6          = 0;
        work->field_A08.flags |= 0x4000;
        return;
    }
    if ((Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) && (config->hp > 0) && (work->field_C20 == 1)) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
        work->field_C20 = 0;
    }
    if ((u32)((work->field_5A & 0x3FF) - 0x10) < 7) {
        if ((s16)func_actor_401800_80133558(arg0->field_2C->coords, 0x12C, work->field_C04) != 0) {
            Actor401800_MoveForwardNonzero(arg0->field_2C->coords, work->field_C04);
        }
        if (func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC) == 1) {
            work->field_C04 = work->field_C04 / 2;
        }
        arg0->field_2C->coords->flg = 0;
    }
    func_actor_401800_80133EB8(arg0);
    if (work->field_68 & 1) {
        kind = enemy->node.targeted;
        if (kind == 1) {
            if (func_actor_401800_80133918(arg0) == kind) {
                work->field_0 = 6;
            } else {
                work->field_0 = 0xA;
            }
        } else {
            work->field_0 = 6;
        }
        if ((config->hp > 0) && (work->field_C20 == 1)) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            work->field_C20 = 0;
        }
    }
}

/// Per-frame body of the live actor armed into state 1: raises the same
/// animation slots as `func_actor_401800_8013971C` but leaves `field_89E = 0xA`
/// (with `field_898 = 1` and `field_89A` cleared), then, while that slot is
/// still `0xA`, advances the actor along its own local Z by a fixed `-0x57`
/// once `func_actor_401800_80133558` says the path is clear. The `0xA` branch
/// then flips the slots to `0xB`/2 and ticks the animation a second time before
/// the two contact records are rebuilt, after which work bit 0 picks `field_0`
/// from the enemy's HP sign and its `field_4C` bit 1.
void func_actor_401800_8013945C(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0;
        work->field_8C8.radius = 0x12C;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
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
        work->field_8C8.flags |= 0x4000;
    }
    if ((work->field_89E == 0xA) && ((s16)func_actor_401800_80133558(arg0->field_2C->coords, 0x12C, -0x57) != 0)) {
        Actor401800_StepForward(arg0->field_2C->coords, -0x57);
    }
    func_actor_401800_80133EB8(arg0);
    if ((work->field_68 & 1) && (work->field_89E == 0xA)) {
        work->field_89E = 0xB;
        work->field_898 = 2;
        func_actor_401800_80133EB8(arg0);
    }
    func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_8E8, 0xC);
    func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC);
    arg0->field_2C->coords->flg = 0;
    if ((work->field_68 & 1) && (work->field_89E == 0xB)) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Per-frame body of the live actor: arms the animation slots and the two
/// `field_8C8` / `field_A08` nodes, re-seeds the 0x8E8 and 0xA28 contact
/// records, then — while work bit 0x100 is set — picks `field_0` from the
/// enemy's HP sign and its `field_4C` bit 1. Same body as `Actor01900_Fn09BE8`.
void func_actor_401800_8013971C(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0;
        work->field_8C8.radius = 0x12C;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_898        = 2;
        work->field_89E        = 0xB;
        work->field_8A2        = 0x10;
        work->field_8B0        = 0;
        work->field_8AE        = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    func_actor_401800_80133EB8(arg0);
    func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_8E8, 0xC);
    func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC);
    arg0->field_2C->coords->flg = 0;
    if (work->field_68 & 0x100) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Second per-frame body of the live actor: as `func_actor_401800_8013971C`,
/// but it arms the animation slots with `field_89E = 0x19` and skips the
/// `field_5A` clip rebuild.
void func_actor_401800_80139870(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0;
        work->field_8C8.radius = 0x12C;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_898        = 2;
        work->field_89E        = 0x19;
        work->field_8A2        = 0x10;
        work->field_8B0        = 0;
        work->field_8AE        = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    func_actor_401800_80133EB8(arg0);
    func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_8E8, 0xC);
    func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC);
    arg0->field_2C->coords->flg = 0;
    if (work->field_68 & 0x100) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Third per-frame body of the live actor: `func_actor_401800_8013971C` with
/// the animation slots armed at 1 / 0xC, and its `field_0` selector driven by
/// work bit 0 instead of bit 8. Same body as `func_actor_401800_8013971C`
/// apart from those three constants.
void func_actor_401800_801399C4(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0;
        work->field_8C8.radius = 0x12C;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_898        = 1;
        work->field_89E        = 0xC;
        work->field_8A2        = 0x10;
        work->field_8B0        = 0;
        work->field_8AE        = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    func_actor_401800_80133EB8(arg0);
    func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_8E8, 0xC);
    func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC);
    arg0->field_2C->coords->flg = 0;
    if (work->field_68 & 1) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Grow-and-settle body of the live actor: while its flag is set it clears the
/// `field_C` overlay, drops the two `GpObj` flag bits the previous body raised,
/// marks the enemy node live and restarts the step counter. The counter then
/// runs to 0x401, firing the light-mode and `0x600A5` effect cues as it crosses
/// steps 0x18, 0x1D, 0x29, 0x2F and 0x3F, and from step 0x1A on rebuilds the
/// root coordinate's Y rotation from its current yaw at scale 0x1194 with the
/// Y component shedding 0xB a step. Same body as `Actor01900_Fn06904` with the
/// `Gp_ReleaseStateF0Add` argument and the actor types changed.
void func_actor_401800_80139B18(Actor401800* arg0)
{
    Actor401800Work*       work;
    GpEnemy*               enemy;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401800RotScratch* blk;
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
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
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
            blk                                      = (Actor401800RotScratch*)(head - 0x34);
            sy                                       = k - (cur - 0x14) * 0xB;
            *(Actor401800RotScratch**)G_SCRATCH_HEAD = blk;
            ang                                      = ratan2((s32)-coord->coord.m[2][0], (s32)coord->coord.m[2][2]);
            blk->angle                               = ang;
            Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
            blk->scale.vx = k;
            blk->scale.vy = (s32)(s16)sy;
            blk->scale.vz = k;
            ScaleMatrix(&blk->m, &((Actor401800RotScratch*)(head - 0x34))->scale);
            coord->coord.m[0][0] = *(u16*)&((Actor401800RotScratch*)(head - 0x34))->m.m[0][0];
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

/// Countdown body: on the live-actor flag re-allocates the model's buffers,
/// copies the root coordinate over its `field_BC0` home and restarts the step
/// counter in state 0xE. The counter then runs to 0x961, rerolling the LCG each
/// frame past it and bailing for that frame on every 0xF-th draw; the surviving
/// frames re-test the squared XZ offset to the camera target against
/// `field_C0E` and arm `Gp_StateF0` state 6 on a miss — bit 0x50000 there arms
/// it the same way. After the shared per-frame tick the body flips between
/// states 0xE and 0xF, one LCG draw per attempt, on the two `field_68` mask
/// bits. Same shape as `func_actor_401800_8013A034`.
void func_actor_401800_80139D60(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    u16              step;
    u32              lcg;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj        = arg0->field_2C;
        enemy      = arg0->field_20;
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags &= 0xBFFF;
        enemy->node.flags      = 0;
        work->field_6          = 0;
        work->field_BC0        = work->field_BA0;
        work->field_89E        = 0xE;
        work->field_898        = 1;
        work->field_8A2        = work->field_8A4;
    }
    step = (u16)work->field_6;
    if (work->field_6 >= 0x961) {
        lcg         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = lcg;
        if (!((lcg >> 16) & 0xF)) {
            return;
        }
    } else {
        work->field_6 = (s16)(step + 1);
    }
    coord    = arg0->field_2C->coords;
    d        = &delta;
    delta.vx = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy    = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz    = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401800_OutOfRange(d, work->field_C0E)) {
        work->field_0 = 6;
    }
    if (*(u32*)&Gp_StateF0 & 0x50000) {
        work->field_0 = 6;
    }
    func_actor_401800_80133EB8(arg0);
    if (work->field_89E == 0xE) {
        if (work->field_68 & 2) {
            lcg         = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = lcg;
            if ((lcg >> 16) & 1) {
                work->field_89E = 0xF;
                work->field_898 = 1;
                func_actor_401800_80133EB8(arg0);
            }
        }
    }
    if (work->field_89E == 0xF && (work->field_68 & 1)) {
        work->field_89E = 0xE;
        work->field_898 = 1;
        func_actor_401800_80133EB8(arg0);
    }
}

/// Walking body: on the live-actor flag re-allocates the model's buffers,
/// hands the actor the `D_actor_401800_80155124` animation block and zeroes the
/// step counter and the 0x8A2..0x8B0 pose slots, otherwise plays the actor's
/// 0x51030008 spawn sound once on the first frame. After the shared per-frame
/// tick, a `field_5A` state of 4 that differs from the last handled one
/// (`field_8B4`) sends the 0x200-scale effect for the second coordinate part.
/// Then, if the squared XZ offset to the camera target fits inside
/// `field_C0E`, the actor plays 0x51030008 and arms `Gp_StateF0` in state 6 —
/// bit 0x50000 of `Gp_StateF0` arms it the same way. Same shape as
/// `Actor01900_Fn06B4C` and `func_actor_401300_801397F8`.
void func_actor_401800_8013A034(Actor401800* arg0)
{
    Actor401800Work* work;
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
        D_actor_401800_80155978 = &D_actor_401800_80155124;
        work->field_89E         = 0x10;
        work->field_898         = 2;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
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
    func_actor_401800_80133EB8(arg0);
    if ((work->field_5A & 0x3FF) == 4 && work->field_8B4 != (work->field_5A & 0x3FF)) {
        work->field_8B8.coord      = arg0->field_2C->coords + 1;
        work->field_8B8.spawnArgLo = 0x200;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->field_2C->coords + 5, NULL, &work->field_8B8);
    }
    work->field_8B4 = work->field_5A & 0x3FF;
    coord           = arg0->field_2C->coords;
    d               = &delta;
    delta.vx        = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy           = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz           = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401800_OutOfRange(d, work->field_C0E)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    if (*(u32*)&Gp_StateF0 & 0x50000) {
        work->field_0 = 6;
    }
}

/// Patrol state: walks toward the waypoint `field_14` selects, turning at most
/// 0x18 per step and swapping waypoints once the waypoint is inside 0xA0 or
/// `field_6` has run 0x15 frames. The turn is folded into the root coordinate,
/// which is rebuilt at scale 0x1194, and the actor steps 7 units along its own
/// local Z while `func_actor_401800_80133558` reports the path clear. The
/// `field_A28` / `field_8E8` contact records then decide whether `field_6`
/// counts up or `func_actor_401800_8013629C` re-seeds them. In the tail the
/// `Player_Status` offset arms state 6 within `field_C0E`, or within 0xFA0 when
/// the aim toward the player is under 0x300. Same body as
/// `Actor01900_Fn06F40` / `func_actor_401300_80139AB0`, with the aim and step
/// helpers inlined. The waypoint delta is written twice; the retail build keeps
/// both sets of stores.
void func_actor_401800_8013A2E8(Actor401800* arg0)
{
    Actor401800Work*        work;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          facing;
    Actor401800TurnScratch* s;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        func_actor_401800_80133EB8(arg0);
        work->field_6 = 0;
        if (arg0->field_36 == 0x10) {
            work->field_8C8.flags |= 0x4000;
        }
        return;
    }
    *(Actor401800TurnScratch**)G_SCRATCH_HEAD -= 1;
    s                                          = *(Actor401800TurnScratch**)G_SCRATCH_HEAD;
    s->delta.vx                                = work->field_C[work->field_14].x - arg0->field_2C->coords->coord.t[0];
    s->delta.vy                                = 0;
    s->delta.vz                                = work->field_C[work->field_14].z - arg0->field_2C->coords->coord.t[2];
    s->delta.vx                                = work->field_C[work->field_14].x - arg0->field_2C->coords->coord.t[0];
    s->delta.vy                                = 0;
    s->delta.vz                                = work->field_C[work->field_14].z - arg0->field_2C->coords->coord.t[2];
    if (!Actor401800_OutOfRange(&s->delta, 0xA0) || work->field_6 >= 0x15) {
        if (work->field_14 == 0) {
            work->field_14 = 1;
        } else {
            work->field_14 = 0;
        }
        work->field_6 = 0;
    }
    func_actor_401800_80133EB8(arg0);
    coord           = arg0->field_2C->coords;
    s->angle        = Actor401800_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = s->angle;
    if (s->angle > 0x18) {
        s->angle = 0x18;
    }
    if (s->angle < -0x18) {
        s->angle = -0x18;
    }
    facing    = arg0->field_2C->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, s->angle, 1);
    Actor401800_RescaleYaw(arg0->field_2C->coords, 0x1194);
    if (work->field_89A == 0 && (s16)func_actor_401800_80133558(arg0->field_2C->coords, 0x12C, 7) != 0) {
        Actor401800_StepForward(arg0->field_2C->coords, 7);
    }
    if (arg0->field_36 != 0x10) {
        if (func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC) == 1 &&
            ABS(work->field_8AE) < 0x80) {
            work->field_6++;
        } else {
            func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
        }
    } else {
        if ((func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC) == 1 ||
             func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_8E8, 0xC) == 1) &&
            ABS(work->field_8AE) < 0x80) {
            work->field_6++;
        } else {
            func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
        }
    }
    arg0->field_2C->coords->flg = 0;
    if (func_actor_401800_80133918(arg0) != 1) {
        Actor401800_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &s->delta);
        if (!Actor401800_OutOfRange(&s->delta, work->field_C0E)) {
            work->field_0 = 6;
        } else if (!Actor401800_OutOfRange(&s->delta, 0xFA0)) {
            coord    = arg0->field_2C->coords;
            s->angle = Actor401800_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
            if (ABS(s->angle) < 0x300) {
                work->field_0 = 6;
            }
        }
    }
    if (*(u32*)&Gp_StateF0 & 0xD0000) {
        work->field_0 = 6;
    }
    *(Actor401800TurnScratch**)G_SCRATCH_HEAD += 1;
}

/// Aim the actor at the player, fold the clamped turn into the root
/// coordinate's Y rotation, then step it along its own local Z while
/// `func_actor_401800_80133558` says the path is clear — reloading
/// `field_0 = 9` once the `field_BFC` step countdown runs out. Same body as
/// `func_actor_401300_8013A208`, with the aim and step helpers inlined.
void func_actor_401800_8013AB64(Actor401800* arg0)
{
    Actor401800Work*        work;
    GpEnemy*                enemy;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    Actor401800TurnScratch* turn;
    u16                     next;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy           = arg0->field_20;
        obj             = arg0->field_2C;
        work->field_89E = 0x12;
        work->field_898 = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_8B0        = 0;
        work->field_8A2        = 0x1E;
    }
    *(Actor401800TurnScratch**)G_SCRATCH_HEAD -= 1;
    turn                                       = *(Actor401800TurnScratch**)G_SCRATCH_HEAD;
    turn->angle                                = Actor401800_PositionYaw(arg0, &turn->delta, &Player_Status);
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
    if (func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC) != 1) {
        func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
    }
    if ((s16)func_actor_401800_80133558(arg0->field_2C->coords, 0x12C, work->field_BFC) != 0) {
        Actor401800_MoveForwardNonzero(arg0->field_2C->coords, work->field_BFC);
    }
    if (work->field_BFC > 0) {
        next            = work->field_BFC - 0xA;
        work->field_BFC = next;
        if ((s16)next < 0) {
            work->field_BFC = 0;
        }
    }
    func_actor_401800_80133EB8(arg0);
    if ((work->field_68 & 1) || work->field_BFC == 0) {
        work->field_0 = 9;
    }
    *(Actor401800TurnScratch**)G_SCRATCH_HEAD += 1;
}

/// Aim the actor at the player and rebuild its root coordinate from the new
/// yaw at scale 0x1194, clamping the turn it adds to +-0x80 and halving it
/// when it is not below -0x80. Once the aim state reaches 0x11 it counts
/// frames in `field_6`, steps the actor along its own local Z while
/// `func_actor_401800_80133558` says the path is clear, re-seeds the
/// `field_A28` contact record and past 0x13 frames turns the actor away from
/// the side the player is on by +-0x4B0. On the live flag it resets the model
/// buffers and arms the state 2 the aim test promotes to 0x11 within 0x80.
/// Same body as `Actor01900_Fn080A8` / `func_actor_401300_8013A5C0`, with the
/// aim, rescale and step helpers inlined.
void func_actor_401800_8013AF1C(Actor401800* arg0)
{
    Actor401800Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401800AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x16;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        func_actor_401800_80133EB8(arg0);
        return;
    }
    func_actor_401800_80133EB8(arg0);
    *(Actor401800AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401800AimScratch**)G_SCRATCH_HEAD;
    aim->angle                                = Actor401800_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE                           = aim->angle;
    if (ABS(aim->angle) <= 0x80 && work->field_89E == 2) {
        work->field_8A2 = 0x16;
        work->field_89E = 0x11;
        work->field_898 = 1;
        work->field_6   = 0;
        func_actor_401800_80133EB8(arg0);
    }
    if (aim->angle > 0x80) {
        aim->angle = 0x80;
    }
    if (aim->angle < -0x80) {
        aim->angle = -0x80;
    } else {
        aim->angle = aim->angle >> 1;
    }
    coord       = arg0->field_2C->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor401800_RescaleYaw(arg0->field_2C->coords, 0x1194);
    arg0->field_2C->coords->flg = 0;
    if (work->field_89E == 0x11) {
        work->field_6++;
        if ((s16)func_actor_401800_80133558(arg0->field_2C->coords, 0x12C, -0x10) != 0) {
            Actor401800_StepForward(arg0->field_2C->coords, -0x10);
        }
        if (func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC) != 1) {
            func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
        }
        arg0->field_2C->coords->flg = 0;
        if (work->field_6 >= 0x13) {
            if (work->field_8AE <= 0) {
                Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0x4B0, 0);
            } else {
                Gfx_RotMatrixY(&arg0->field_2C->coords->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    *(Actor401800AimScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401800_8013B444(Actor401800* arg0)
{
    Actor401800Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401800AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 9;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags &= 0xBFFF;
        func_actor_401800_80133EB8(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6++;
    *(Actor401800AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401800AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->coords->flg               = 0;
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
    aim->angle      = Actor401800_PositionYaw(arg0, &aim->delta, &Player_Status);
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
    Actor401800_RescaleYaw(arg0->field_2C->coords, 0x1194);
    func_actor_401800_80133EB8(arg0);
    *(Actor401800AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Aim the actor at the player and rescale its root coordinate, turning the
/// stored yaw toward the target by at most 0x28 a frame instead of the hard
/// clamp `func_actor_401800_80135F58` uses. On the live flag it resets the
/// model buffers and re-arms the step countdown; otherwise it hands the
/// player offset and the new yaw to the actor's state body and rebuilds the
/// matrix at scale 0x1194.
void func_actor_401800_8013B784(Actor401800* arg0)
{
    Actor401800Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401800AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 1;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_89E        = 0x13;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags &= 0xBFFF;
        func_actor_401800_80133EB8(arg0);
        func_actor_401800_80133EB8(arg0);
        work->field_6   = 0;
        work->field_8B0 = 0;
        return;
    }
    *(Actor401800AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401800AimScratch**)G_SCRATCH_HEAD;
    aim->angle                                = Actor401800_PositionYaw(arg0, &aim->delta, &Player_Status);
    if (work->field_8AE < aim->angle) {
        if (aim->angle - work->field_8AE >= 0x29) {
            work->field_8AE = (u16)work->field_8AE + 0x28;
        } else {
            work->field_8AE = aim->angle;
        }
    } else if (work->field_8AE - aim->angle >= 0x29) {
        work->field_8AE = (u16)work->field_8AE - 0x28;
    } else {
        work->field_8AE = aim->angle;
    }
    if (work->field_8AE == aim->angle && func_actor_401800_80133918(arg0) != 1 && work->field_8C2 == 0) {
        work->field_0 = 0xB;
    }
    coord      = arg0->field_2C->coords;
    aim->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor401800_RescaleYaw(arg0->field_2C->coords, 0x1194);
    work->field_898 = 2;
    func_actor_401800_80133EB8(arg0);
    if (work->field_8C2 != 0) {
        work->field_8C2--;
    }
    *(Actor401800AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Tint a freshly spawned effect model from the enemy's area record. Same body
/// as `Actor401300_TintEffect`, inlined at each of the four spawn sites below.
static __inline__ void Actor401800_TintEffect(GpEffWork* eff, GpEnemy* enemy)
{
    GpAreaKey    key;
    GpAreaKey*   sessionKey;
    GpAreaKey*   keyPtr;
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
        SOFT_BARRIER();
        keyPtr = &key;
        TOUCH_REG(keyPtr);
        key.view = areaByte0;
        Gp_SyncAreaKeyIndex(keyPtr);
        rec          = Gp_GetNestedAreaRec(&key);
        entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = entry->tpage;
        model->clut  = entry->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
}

/// Step-driven effect spawner for the actor's live ramp: while the spawn flag
/// is set the actor crouches (0x8C8 node pitched to 0x12C, 0xA08 flags bit
/// 0x4000 cleared), plays the 0x60030 debris burst and hands the task to the
/// state-F0 list; the step counter then fires the 0xA0005 effects at 3, 5, 7
/// and 9, each tinted from the enemy's area record, and parks the actor at 0x3D.
void func_actor_401800_8013BB10(Actor401800* arg0)
{
    SVECTOR          vec;
    Actor401800Work* work;
    GpEnemy*         enemy;
    u16              next;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0x80;
        work->field_8C8.radius = 0x12C;
        work->field_A08.flags  = (u16)(work->field_A08.flags & 0xBFFF);
        enemy->node.flags      = 1;
        work->field_8AE        = 0;
        work->field_6          = 0U;
        vec.vx                 = 0x64;
        vec.vz                 = 0;
        vec.vy                 = 0;
        Gp_SpawnEff(0x60030, arg0->field_2C->coords + 1, 0x10300, &vec);
        Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0xA);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 3) {
        D_80114B78[0] = &D_actor_401800_80143E9C;
        vec.vz        = 0x64;
        vec.vy        = 0;
        vec.vx        = 0;
        Actor401800_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 9, 0x200, &vec), enemy);
    }
    if (work->field_6 == 5) {
        D_80114B78[0] = &D_actor_401800_80144434;
        vec.vy        = 0;
        vec.vx        = 0;
        Actor401800_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 12, 0x200, &vec), enemy);
    }
    if (work->field_6 == 7) {
        D_80114B78[0] = &D_actor_401800_80143E9C;
        Actor401800_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 1, 0x200, NULL), enemy);
    }
    if (work->field_6 == 9) {
        D_80114B78[0] = &D_actor_401800_80144F24;
        Actor401800_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 3, 0x200, NULL), enemy);
    }
    if (work->field_6 >= 0x3D) {
        work->field_0 = 0;
    }
}

/// Step-driven aim-and-rescale body: while the spawn flag is set the actor
/// pitches its 0x8C8 node to the 0x12C walk animation, sets the 0xA08 flags bit
/// 0x4000, plays the 0x60030 burst and arms the 2/0x1A state pair; the step
/// counter then steps the actor along its facing while
/// `func_actor_401800_80133558` reports the path clear, re-seeds the
/// `field_A28` contact and fires the 0xA0005 effects at 3, 5 and 6. The 0x1A
/// state runs its light/state table and folds the countdown into the root
/// coordinate's Y scale; the nine body coordinates from +2 to +10 are then
/// rebuilt at unit scale.
void func_actor_401800_8013BF48(Actor401800* arg0)
{
    SVECTOR          vec;
    Actor401800Work* work;
    GpEnemy*         enemy;
    u16              next;
    s16              cur;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_8C8.radius = 0x12C;
        work->field_A08.flags  = (u16)(work->field_A08.flags | 0x4000);
        enemy->node.flags      = 1;
        work->field_8AE        = 0;
        work->field_6          = 0U;
        vec.vx                 = 0x64;
        vec.vz                 = 0;
        vec.vy                 = 0;
        work->field_89E        = 2;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        Gp_SpawnEff(0x60030, arg0->field_2C->coords + 1, 0x10300, &vec);
        work->field_6 = 0U;
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    switch (work->field_89E) {
        case 2:
            if ((s16)next >= 0x10 && (work->field_68 & 2)) {
                work->field_89E = 0x1A;
                work->field_898 = 2;
                work->field_8A2 = 0x10;
                work->field_89A = 0;
            }
            if ((s16)func_actor_401800_80133558(arg0->field_2C->coords, 0x12C, 7) != 0) {
                Actor401800_StepForward(arg0->field_2C->coords, 7);
            }
            func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC);
            if (work->field_6 == 3) {
                D_80114B78[0] = &D_actor_401800_80143E9C;
                vec.vz        = 0x64;
                vec.vy        = 0;
                vec.vx        = 0;
                Actor401800_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 9, 0x200, &vec), enemy);
            }
            if (work->field_6 == 5) {
                D_80114B78[0] = &D_actor_401800_80143E9C;
                Actor401800_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 1, 0x200, NULL), enemy);
            }
            if (work->field_6 == 6) {
                D_80114B78[0] = &D_actor_401800_80144F24;
                Actor401800_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 3, 0x200, NULL), enemy);
            }
            break;
        case 0x1A:
            if (!(work->field_68 & 0x100)) {
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
                Actor401800_RescaleYawY(arg0->field_2C->coords, 0x1194, 0x1194 - (cur - 0x14) * 0xB);
            }
            break;
    }
    func_actor_401800_80133EB8(arg0);
    Actor401800_ResetYaw(arg0->field_2C->coords + 2);
    Actor401800_ResetYaw(arg0->field_2C->coords + 3);
    Actor401800_ResetYaw(arg0->field_2C->coords + 4);
    Actor401800_ResetYaw(arg0->field_2C->coords + 5);
    Actor401800_ResetYaw(arg0->field_2C->coords + 6);
    Actor401800_ResetYaw(arg0->field_2C->coords + 7);
    Actor401800_ResetYaw(arg0->field_2C->coords + 8);
    Actor401800_ResetYaw(arg0->field_2C->coords + 9);
    Actor401800_ResetYaw(arg0->field_2C->coords + 10);
}

/// Walk body: takes a 0x10 scratch for the player offset, the facing yaws and
/// the wrapped turn toward the player. On the live flag it resets the model
/// buffers, arms the walk state (`0x12C` animation, step 0x30) and zeroes the
/// step counters; otherwise it counts both step slots, re-seeds the `field_A28`
/// / `field_8E8` contacts, folds the turn into the root coordinate's Y rotation
/// at scale 0x1194 and steps the actor 0x15 / 5 along its own local Z while
/// `func_actor_401800_80133558` says the path is clear. On the `func_actor_
/// 401800_80133918` hit it clears the stride, picks a side from the LCG and
/// flips it every 0xF1 frames instead. Same body as `Actor01900_Fn042BC`.
void func_actor_401800_8013CD98(Actor401800* arg0)
{
    Actor401800Work*         work;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           facing;
    Actor401800ChaseScratch* s;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x30;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_C1C        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        Gp_ArmStateF0(1);
        work->field_6 = 0;
        work->field_8 = 0;
        if (arg0->field_36 == 0x10) {
            work->field_8C8.flags |= 0x4000;
        }
    }
    work->field_6++;
    work->field_8++;
    *(Actor401800ChaseScratch**)G_SCRATCH_HEAD -= 1;
    s                                           = *(Actor401800ChaseScratch**)G_SCRATCH_HEAD;
    if (func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_A28, 0xC) != 1) {
        if (func_actor_401800_80132C68(arg0->field_2C->coords, &work->field_8E8, 0xC) != 1) {
            func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
        }
    }
    Actor401800_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &s->delta);
    arg0->field_2C->coords->flg = 0;
    func_actor_401800_80133EB8(arg0);
    s->playerYaw = ratan2(-((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                          ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
    Actor401800_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &s->delta);
    s->yaw = ratan2(s->delta.vx, s->delta.vz) + 0x800;
    s->yaw = Actor401800_NormalizeYaw(s->yaw);
    if (func_actor_401800_80133918(arg0) != 1) {
        work->field_6   = 0;
        coord           = arg0->field_2C->coords;
        s->turn         = Actor401800_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) -
                                                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (s->turn < 0x200) {
            if (!Actor401800_OutOfRange(&s->delta, 0x44C) && work->field_8C2 == 0) {
                work->field_0 = 0xB;
            }
        }
        if (work->field_8 >= 0x5B) {
            work->field_0 = 0x1B;
        }
    } else {
        work->field_8 = 0;
        work->field_6++;
        coord           = arg0->field_2C->coords;
        s->turn         = Actor401800_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) -
                                                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (work->field_C00 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C00 = -1;
            } else {
                work->field_C00 = 1;
            }
        }
        if (work->field_C00 == 1) {
            s->turn += 0x400;
        } else {
            s->turn -= 0x400;
        }
        if (work->field_6 >= 0xF1) {
            work->field_6   = 0;
            work->field_C00 = -work->field_C00;
        }
    }
    if (s->turn > 0x20) {
        s->turn = 0x20;
    }
    if (s->turn < -0x20) {
        s->turn = -0x20;
    }
    facing   = arg0->field_2C->coords;
    s->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, s->turn, 1);
    Actor401800_RescaleYaw(arg0->field_2C->coords, 0x1194);
    arg0->field_2C->coords->flg = 0;
    if (work->field_89E == 2) {
        if (work->field_89A == 0) {
            if ((s16)func_actor_401800_80133558(arg0->field_2C->coords, 0x12C, 0x15) != 0) {
                Actor401800_StepForward(arg0->field_2C->coords, 0x15);
            }
        } else if ((s16)func_actor_401800_80133558(arg0->field_2C->coords, 0x12C, 5) != 0) {
            Actor401800_StepForward(arg0->field_2C->coords, 5);
        }
    } else if (work->field_68 & 1) {
        work->field_89E = 2;
        work->field_898 = 1;
    }
    if (work->field_8C2 != 0) {
        work->field_8C2--;
    }
    *(Actor401800ChaseScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013D64C);

void func_actor_401800_8013DCB4(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_401800/actor_401800_2", ActorsShared80135df4Table);

/* func_actor_401800_8013DCBC (the 0x7D3 message handler) lives in
 * actor_401800_2.c: the jump table GCC emits for its switch is what owns the
 * rodata run at 0x254, so the function belongs to the unit that starts there. */
