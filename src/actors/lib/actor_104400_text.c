#include "common.h"
#include "actors/actor_104400.h"
#include "actors/actors_shared_801639a8.h"
#include "actors/actors_shared_8016945c.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "psyq/abs.h"

void Actor04400_Fn00220(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s32 arg4, u8 arg5);
void Actor04400_Fn006A8(Task* arg0);
void Actor04400_Fn022A8(Task* arg0, s16 arg1);
/* Reads the caller's Task* from $a0; the call passes no argument. */
void Actor04400_Fn02B8C();
void Actor04400_Fn02D18(Task* arg0);
void Actor04400_Fn061B4(void);
void Actor04400_Fn06520(Task* arg0, s16 arg1, u16* arg2);
void Actor04400_Fn07360(Task* arg0);
/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
s16  Actor04400_Fn06328(Task* arg0);
void Actor04400_Fn06374(Task* arg0, s32 arg1);
s16  Actor04400_Fn065F4(Task* arg0, s32 arg1);
s16  Actor04400_Fn06618(Task* arg0);
void Actor04400_Fn067A0(Task* arg0, s32 step);
void Actor04400_Fn08208(Task* arg0);
void Actor04400_Fn0823C(Task* arg0);

extern u32 Gp_LcgState;

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` copies into
 * its effect's `TmdObject`. Declared as a one-element array so GCC 2.8.1
 * cannot treat the store as a non-aliasing scalar and sink it past the
 * `TmdObject` loads. */
extern void*          D_800678F0[1];
extern TaskFuncTable9 Actor04400_D000EC;
extern u8             Actor04400_D098FC[];
extern u8             Actor04400_D09FA0[];
extern u8             Actor04400_D0A510[];

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn00220);

void Actor04400_Fn006A8(Task* arg0)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* src;
    TmdObject* src2;

    D_800678F0[0] = Actor04400_D098FC;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[6], 0x200, NULL);
    if (eff != NULL) {
        src           = (TmdObject*)arg0->extra;
        dst           = (TmdObject*)eff->field_0->extra;
        dst->field_24 = src->field_24;
        dst->field_25 = src->field_25;
        if (dst->field_18 != NULL) {
            Tmd_ProcessStream(dst);
            Tmd_ProcessStream(dst);
        }
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((Gp_LcgState >> 16) & 1) {
        D_800678F0[0] = Actor04400_D09FA0;
        eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[8], 0x200, NULL);
    } else {
        D_800678F0[0] = Actor04400_D0A510;
        eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[2], 0x200, NULL);
    }
    if (eff2 != NULL) {
        src2           = (TmdObject*)arg0->extra;
        dst2           = (TmdObject*)eff2->field_0->extra;
        dst2->field_24 = src2->field_24;
        dst2->field_25 = src2->field_25;
        if (dst2->field_18 != NULL) {
            Tmd_ProcessStream(dst2);
            Tmd_ProcessStream(dst2);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[3], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[4], 0x200, NULL);
}

/// Same body as `ActorsShared801639a8`.
void Actor04400_Fn00874(Task* arg0)
{
    SVECTOR                       rot;
    ActorsShared801639a8Mat       mtx;
    ActorsShared801639a8MatWords* ident;
    Actor104400Work*              work;
    GsCOORDINATE2*                coords;
    MATRIX*                       m5;
    MATRIX*                       m4;
    MATRIX*                       m3;

    work   = (Actor104400Work*)arg0->idMap;
    ident  = &mtx.ident;
    coords = ((TmdObject*)arg0->extra)->field_8;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m5                = &coords[5].coord;
    Gp_MtxToEuler(m5, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m5->m[0][0]   = (u16)mtx.mat.m[0][0];
    m5->m[0][1]   = (u16)mtx.mat.m[0][1];
    m5->m[0][2]   = (u16)mtx.mat.m[0][2];
    m5->m[1][0]   = (u16)mtx.mat.m[1][0];
    m5->m[1][1]   = (u16)mtx.mat.m[1][1];
    m5->m[1][2]   = (u16)mtx.mat.m[1][2];
    m5->m[2][0]   = (u16)mtx.mat.m[2][0];
    m5->m[2][1]   = (u16)mtx.mat.m[2][1];
    m5->m[2][2]   = (u16)mtx.mat.m[2][2];
    coords[5].flg = 0;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m4                = &coords[4].coord;
    Gp_MtxToEuler(m4, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m4->m[0][0]   = (u16)mtx.mat.m[0][0];
    m4->m[0][1]   = (u16)mtx.mat.m[0][1];
    m4->m[0][2]   = (u16)mtx.mat.m[0][2];
    m4->m[1][0]   = (u16)mtx.mat.m[1][0];
    m4->m[1][1]   = (u16)mtx.mat.m[1][1];
    m4->m[1][2]   = (u16)mtx.mat.m[1][2];
    m4->m[2][0]   = (u16)mtx.mat.m[2][0];
    m4->m[2][1]   = (u16)mtx.mat.m[2][1];
    m4->m[2][2]   = (u16)mtx.mat.m[2][2];
    coords[4].flg = 0;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m3                = &coords[3].coord;
    Gp_MtxToEuler(m3, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m3->m[0][0]   = (u16)mtx.mat.m[0][0];
    m3->m[0][1]   = (u16)mtx.mat.m[0][1];
    m3->m[0][2]   = (u16)mtx.mat.m[0][2];
    m3->m[1][0]   = (u16)mtx.mat.m[1][0];
    m3->m[1][1]   = (u16)mtx.mat.m[1][1];
    m3->m[1][2]   = (u16)mtx.mat.m[1][2];
    m3->m[2][0]   = (u16)mtx.mat.m[2][0];
    m3->m[2][1]   = (u16)mtx.mat.m[2][1];
    m3->m[2][2]   = (u16)mtx.mat.m[2][2];
    coords[3].flg = 0;
}

/// Main enemy init. Allocates the 0x454-byte `Actor104400Work`, points the
/// model at the light / color matrices inside it, runs the animation context,
/// links the collision objects and the enemy's list node, and enters state 2
/// for spawn kind 1 (low nibble of `spawnArg1`), state 1 otherwise. The root
/// coord is lifted by 0x3C and its translation kept as the spawn position.
///
/// Same body as `func_actor_342400_80163C58`.
///
/// `one` is a separate variable set before `Gp_IncStateF0Ref`: the ROM holds
/// the constant in `$s0`, which GCC only picks for a pseudo that crosses a
/// call (sched2 then sinks the `li` below the `jal`).
void Actor04400_Fn00B24(Task* arg0)
{
    GpEnemy*         enemy;
    GsCOORDINATE2*   root;
    Actor104400Work* work;
    TmdObject*       obj;
    Actor104400Work* w;
    GpEnemy*         e;
    GsCOORDINATE2*   coord;
    Actor104400Work* w2;
    Actor104400Work* w3;
    Actor104400Work* w4;
    s32              one;

    enemy       = arg0->spawnArg2;
    root        = ((TmdObject*)arg0->extra)->field_8;
    arg0->idMap = Mem_Calloc(0x454, 0);
    work        = (Actor104400Work*)arg0->idMap;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    Actor04400_Fn061B4();
    obj                = arg0->extra;
    w                  = (Actor104400Work*)arg0->idMap;
    e                  = arg0->spawnArg2;
    coord              = obj->field_8;
    arg0->field_24     = Actor04400_D107CC;
    obj->field_1C      = &w->lightMtx;
    obj->field_20      = &w->colorMtx;
    e->field_50        = &Actor04400_D0D318;
    e->field_54        = (s32)w->rec_2EC;
    w->eff_3FC.field_0 = &((TmdObject*)arg0->extra)->field_8[1];
    w->eff_3FC.field_4 = 0x140;
    w->eff_3FC.field_6 = 2;
    e->field_40 = e->field_42 = Actor04400_D0D318.field_4;
    func_800B3F84(&w->anim, Actor04400_D10778, (GpAnimObj*)obj, w->field_21C, &w->slot_B4);
    w2            = (Actor104400Work*)arg0->idMap;
    w2->field_41C = 0x10;
    w2->field_418 = 7;
    w2->field_414 = 2;
    Actor04400_Fn02B8C(arg0);
    coord->sub = &Gfx_ViewCoord;
    Actor04400_Fn02D18(arg0);
    w->field_7A = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0x800;
    enemy       = arg0->spawnArg2;
    Gp_LinkNode(&enemy->node);
    enemy->field_4      = &((TmdObject*)arg0->extra)->field_8->coord;
    enemy->field_48     = 0;
    enemy->field_1C.vx  = 0;
    enemy->field_1C.vy  = 0;
    enemy->field_1C.vz  = 0;
    enemy->field_18     = &((TmdObject*)arg0->extra)->field_8[1];
    enemy->node.field_4 = 4;
    one                 = 1;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    if ((arg0->spawnArg1 & 0xF) == one) {
        w3            = (Actor104400Work*)arg0->idMap;
        arg0->state   = 2;
        w3->field_420 = 0;
        w3->field_422 = 0;
    } else {
        w4            = (Actor104400Work*)arg0->idMap;
        arg0->state   = one;
        w4->field_420 = 0;
        w4->field_422 = 0;
    }
    work->field_80    = root->coord.t[0];
    root->coord.t[1] -= 0x3C;
    work->field_82    = root->coord.t[1];
    work->field_84    = root->coord.t[2];
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn00D3C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn00F7C);

/// Same body as `ActorsShared8016454c`.
void Actor04400_Fn01418(Task* arg0)
{
    Actor104400Work* work;
    s32              soundId;
    s32              pan;
    s16              step;

    work            = (Actor104400Work*)arg0->idMap;
    work->field_426 = 8;
    work->field_418 = 7;
    work->field_41C = 0x10;
    work->field_414 = 1;
    work->field_422++;
    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0001;
    pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_410 = (Gp_LcgState >> 0x10) & 0x7FF;
    if (work->field_43A < 1000) {
        work->field_41C = 0x10;
        work->field_436 = 0x10;
        return;
    }
    if (work->field_43A < 2000) {
        work->field_41C = 0x14;
        step            = 0x12;
    } else if (work->field_43A < 3000) {
        work->field_41C = 0x18;
        step            = 0x14;
    } else if (work->field_43A < 4000) {
        work->field_41C = 0x1C;
        step            = 0x16;
    } else if (work->field_43A < 5000) {
        work->field_41C = 0x20;
        step            = 0x18;
    } else {
        work->field_41C = 0x40;
        step            = 0x20;
    }
    work->field_436 = step;
}

void Actor04400_Fn01584(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;
    s16              dist;
    s16              limit;
    s16              step;
    s16              angle;
    s16              speed;
    s32              soundId;
    s32              pan;

    dist = work->field_43A;
    if (dist < 1000) {
        limit = 0x10;
        step  = 0x10;
    } else if (dist < 2000) {
        step  = 0x12;
        limit = 0x14;
    } else if (dist < 3000) {
        step  = 0x14;
        limit = 0x18;
    } else if (dist < 4000) {
        step  = 0x16;
        limit = 0x1C;
    } else if (dist < 5000) {
        limit = 0x20;
        step  = 0x18;
    } else {
        limit = 0x40;
        step  = 0x20;
    }
    if (work->field_41C < limit) {
        work->field_41C = limit;
        work->field_436 = step;
    }
    Actor04400_Fn067A0(arg0, work->field_436);
    speed                                           = Actor04400_Fn065F4(arg0, -0x10);
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0001;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if (work->field_43A < work->field_410 + 2000 && (work->field_43A < 1500 || work->field_44A == 0) &&
        (u16)(((work->field_444 + 0x800) & 0xFFF) - 0x200) > 0xC00) {
        work->field_422++;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn017B0);

void Actor04400_Fn01B70(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    s32              soundId;
    s32              pan;
    u32              rand;

    work = (Actor104400Work*)arg0->idMap;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0004;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        work->field_438  = 0;
        rand             = Gp_LcgState * 5 + 0x71357911;
        work->field_44A  = ((rand >> 16) & 0x7F) + 0x5A;
        work->field_7A  += 0x800;
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_41C = 0x10;
        work2->field_418 = 0xD;
        work2->field_414 = 2;
        work3            = (Actor104400Work*)arg0->idMap;
        Gp_LcgState      = rand;
        arg0->state      = 1;
        work3->field_420 = 0;
        work3->field_422 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn01CA0);

/// Colours `enemy` from `coord`'s world position through a 0x10-byte `VECTOR`
/// taken off `G_SCRATCH_HEAD`; the same helper as `func_actor_342400_80164F3C`.
static __inline__ void Actor04400_UpdateColor(void* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block = (VECTOR*)(*(u8**)G_SCRATCH_HEAD - 0x10);

    block->vx                 = coord->workm.t[0];
    block->vy                 = coord->workm.t[1];
    *(VECTOR**)G_SCRATCH_HEAD = block;
    block->vz                 = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// Same body as `func_actor_342400_80164F3C`. Per-frame callback with a
/// one-entry handler table. `D_801153F4` 2 hides the model; 0 runs the state
/// handler and the follow-up steps, then moves the task to state 4 when
/// `field_448` requests it and the enemy is out of HP; 0 and 1 both colour
/// it, run `Actor04400_Fn00220` for three part pairs and unhide it. The work
/// block is reloaded through its own local for the state reset, as the
/// original does.
void Actor04400_Fn01E08(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->idMap;
    GpEnemy*         enemy = arg0->spawnArg2;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFunc         sp[1] = { Actor04400_Fn07360 };

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp[(s16)work->field_420](arg0);
            Actor04400_Fn022A8(arg0, 1);
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->field_40 <= 0) {
                Actor104400Work* w = (Actor104400Work*)arg0->idMap;

                arg0->state  = work->field_448;
                w->field_420 = 0;
                w->field_422 = 0;
            }
            Actor04400_Fn02B8C(arg0);
            if (work->field_432 == 1) {
                Actor04400_Fn06520(arg0, 6, &work->field_80);
            }
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
            Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
            Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->field_C &= ~0x80;
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn02008);

/// Same body as `ActorsShared801652a0`.
void Actor04400_Fn0216C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* next;
    Actor104400Work* next2;
    u32              soundId;
    s32              pan;

    work = (Actor104400Work*)arg0->idMap;
    if ((s16)++work->field_412 == 1) {
        soundId   = (u16)((GpEnemy*)arg0->spawnArg2)->field_8;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0004;
        pan       = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if ((s16)work->field_412 == 2) {
        soundId   = (u16)((GpEnemy*)arg0->spawnArg2)->field_8;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0003;
        pan       = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if (Actor04400_Fn06618(arg0)) {
        next             = (Actor104400Work*)arg0->idMap;
        arg0->state      = 3;
        next->field_420  = 0;
        next->field_422  = 0;
        next2            = (Actor104400Work*)arg0->idMap;
        next2->field_420 = 3;
        next2->field_422 = 0;
    }
}

/// Picks the per-axis step: the collision `step` when there is one and the
/// push-out opposes it, otherwise whichever of the two is larger in the
/// direction of `step`.
static __inline__ s16 Actor04400_PickStep(s16 step, s16 push)
{
    if (step == 0) {
        return push;
    }
    if ((step > 0 && push < 0) || (step < 0 && push > 0)) {
        return step;
    }
    if (step > 0) {
        if (push < step) {
            return step;
        }
        return push;
    }
    if (push < step) {
        return push;
    }
    return step;
}

/// Push-out of the model from contact record `rec`: how far `coord` sits
/// inside the record's radius (`field_2`), along the direction from the
/// record's centre to the root part, carried into grid space.
///
/// `rec` must stay an inline argument: `integrate.c` expands it with
/// `EXPAND_SUM`, giving `(i * 0x18 + work) + 0x2EC` rather than a loop giv.
static __inline__ void Actor04400_CalcPush(Task* arg0, GsCOORDINATE2* coord, GpRec18* rec, SVECTOR* out)
{
    SVECTOR        pos;
    VECTOR         d;
    VECTOR         n;
    GsCOORDINATE2* c2;
    s32            t;
    s32            pen;

    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    c2     = ((TmdObject*)arg0->extra)->field_8;
    d.vx   = pos.vx - rec->field_8;
    d.vy   = 0;
    d.vz   = pos.vz - rec->field_C;
    pen    = SquareRoot0(d.vx * d.vx + d.vz * d.vz);
    pen    = rec->field_2 - pen;
    if (pen <= 0) {
        t = 0;
    } else {
        t = pen;
    }
    pen  = t;
    d.vx = c2->workm.t[0] - rec->field_8;
    d.vy = c2->workm.t[1] - rec->field_A;
    d.vz = c2->workm.t[2] - rec->field_C;
    VectorNormal(&d, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &n, &d);
    out->vx = (pen * d.vx) >> 12;
    out->vy = 0;
    out->vz = (pen * d.vz) >> 12;
}

/// Same body as `func_actor_342400_801653DC`. Per-frame contact handling for
/// the overlay's enemy. Walks the eight contact records: kind 1 (skipped when
/// `arg1` is set) and kind 3 push the model out, kind 2 applies a hit -
/// damage, status effects and the pending state request in `field_448` -
/// unless `field_40E` is still cooling down. Then ticks the status flags,
/// applies `func_800E0C10`'s collision step (snapping back to `field_60` when
/// it reports a conflict) and moves the root by the combined step and
/// push-out.
void Actor04400_Fn022A8(Task* arg0, s16 arg1)
{
    GpDeltaScratch   delta;
    SVECTOR          push;
    s16              maxX;
    s16              maxZ;
    s16              stepX;
    s16              stepZ;
    u8               blocked;
    Actor104400Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    s16              amount;
    s32              dmg;
    s32              tmp;
    s16              tick;
    s32              i;

    stepZ                  = 0;
    maxX                   = 0;
    maxZ                   = 0;
    stepX                  = 0;
    blocked                = 0;
    work                   = (Actor104400Work*)arg0->idMap;
    coord                  = ((TmdObject*)arg0->extra)->field_8;
    enemy                  = arg0->spawnArg2;
    *(u8**)G_SCRATCH_HEAD -= 8;
    work->field_41E        = 0;
    for (i = 0; i < 8; i++) {
        switch (work->rec_2EC[i].field_4 & 0xFFFF0000) {
            case 0x10000:
                if (arg1 != 0) {
                    break;
                }
            case 0x30000:
                Actor04400_CalcPush(arg0, coord, &work->rec_2EC[i], &push);
                if (ABS(maxX) < ABS(push.vx)) {
                    maxX = push.vx;
                }
                if (ABS(maxZ) < ABS(push.vz)) {
                    maxZ = push.vz;
                }
                break;
            case 0x20000:
                if (work->field_40E == 0) {
                    work->field_41E = 1;
                    dmg             = Gp_ComputeDamage(work->rec_2EC[i].field_4, work->field_43A, 0, 0);
                    amount          = dmg;
                    work->field_40E = Gp_GetIdParam2(work->rec_2EC[i].field_4);
                    if (Gp_RollEnemyChance(enemy, work->rec_2EC[i].field_4, 0) != 0) {
                        amount = ((u32)dmg << 16) >> 14;
                        Gp_SpawnEff(0x6009C, &((TmdObject*)arg0->extra)->field_8[3], 0, NULL);
                    }
                    func_800E2C78((GpObj40*)enemy, work->rec_2EC[i].field_4, amount, 0);
                    func_800DA6E8(&enemy->node, amount, 0);
                    enemy->field_40 -= amount;
                    if (enemy->field_40 < 0) {
                        enemy->field_40 = 0;
                    }
                    func_800FDB18(Gp_GetIdParam1(work->rec_2EC[i].field_4) & 0xFFFF,
                                  &((TmdObject*)arg0->extra)->field_8[1], NULL, &work->eff_3FC);
                    if (amount >= 0x28) {
                        work->field_448 = 2;
                    } else {
                        work->field_448 = 1;
                    }
                    switch (Gp_GetIdParam0(work->rec_2EC[i].field_4) & 0xFFFF) {
                        case 0:
                            break;
                        case 1:
                            Gp_SetObjFlag1((GpObj4C*)enemy);
                            break;
                        case 2:
                            Gp_SetObjFlag2((GpObj5D*)enemy, work->rec_2EC[i].field_4, 0);
                            break;
                        case 3:
                            Gp_SetObjFlag4((GpObj5C*)enemy, work->rec_2EC[i].field_4, 0);
                            break;
                        case 4:
                            work->field_448 = 4;
                            break;
                        case 5:
                            work->field_448 = 2;
                            break;
                        case 6:
                            work->field_448 = 4;
                            break;
                        case 7:
                            work->field_448 = 2;
                            break;
                        case 8:
                            work->field_448 = 3;
                            break;
                        case 9:
                            work->field_448 = 3;
                            break;
                    }
                } else if ((Gp_GetIdParam1(work->rec_2EC[i].field_4) & 0xFFFF) == 0xD) {
                    func_800FDB18(0xD, &((TmdObject*)arg0->extra)->field_8[1], NULL, &work->eff_3FC);
                }
                break;
        }
    }

    if (enemy->field_4C & 1) {
        enemy->field_4C &= 0xFE;
        work->field_448  = 5;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= 0xFD;
        work->field_448  = 3;
    }
    if (enemy->field_4C & 0xC) {
        work->field_44E = 1;
        tmp             = Gp_TickObjFlag4((GpObj5C*)enemy);
        tick            = tmp;
        if (tick != 0) {
            enemy->field_40 -= tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if (enemy->field_40 < 0) {
                enemy->field_40 = 0;
            }
            work->field_41E = 1;
            work->field_448 = 2;
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->field_4C &= 0xF3;
        }
    }

    switch (func_800E0C10(work->rec_2EC, &delta, 8, NULL)) {
        case 0:
            break;
        case 1:
            stepZ = delta.vz.h.hi;
            stepX = delta.vx.w >> 16;
            if (delta.vx.w & 0xFFFF) {
                if (delta.vx.w > 0) {
                    stepX++;
                } else {
                    stepX--;
                }
            }
            if (delta.vz.w & 0xFFFF) {
                if (delta.vz.w > 0) {
                    stepZ++;
                } else {
                    stepZ--;
                }
            }
            break;
        case 2:
            coord->coord.t[0] = work->field_60.vx;
            coord->coord.t[2] = work->field_60.vz;
            coord->flg        = 0;
            blocked           = 1;
            break;
    }

    Gp_ClearRec18Occupied(work->rec_2EC);
    if (work->field_43E != 0) {
        work->field_43E--;
    }
    if (work->field_40E > 0) {
        work->field_40E--;
    }
    if (blocked == 0) {
        work->field_80    += Actor04400_PickStep(stepX, maxX >> 3);
        work->field_84    += Actor04400_PickStep(stepZ, maxZ >> 3);
        coord->coord.t[0] += Actor04400_PickStep(stepX, maxX >> 3);
        coord->coord.t[2] += Actor04400_PickStep(stepZ, maxZ >> 3);
        coord->flg         = 0;
    }
    *(u8**)G_SCRATCH_HEAD += 8;
}

/// Same body as `ActorsShared80165cc0`.
void Actor04400_Fn02B8C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* start;
    s32              i;
    s32              j;
    s32              k;

    work = (Actor104400Work*)arg0->idMap;
    if (work->field_414 == 1) {
        start = work;
        if (start->field_416 == start->field_418) {
            for (i = 1; i < 9; i++) {
                (&start->slot_B4)[i].field_9 = start->field_41C;
                func_800B4114(&start->anim, i, start->field_418, 0, start->field_426);
            }
        } else {
            for (i = 1; i < 9; i++) {
                (&start->slot_B4)[i].field_9 = start->field_41C;
                func_800B4114(&start->anim, i, start->field_418, 0, start->field_426);
            }
            start->field_426 = 0;
        }
        goto advance;
    }
    if (work->field_414 == 2) {
        start = work;
        for (j = 1; j < 9; j++) {
            Gp_AnimResetSlot(&start->anim, j, start->field_418);
            (&start->slot_B4)[j].field_9 = start->field_41C;
        }
    advance:
        start->field_416 = start->field_418;
        work->field_414  = 3;
        work->field_41A  = 0;
    } else if (work->field_414 == 3) {
        work->field_41A++;
    }
    for (k = 1; k < 9; k++) {
        (&work->slot_B4)[k].field_9 = work->field_41C;
        Gp_AnimTickIndex(&work->anim, k);
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn02D18);

/// Same body as `func_actor_342400_80165FC0`. Per-frame callback of the main
/// enemy. `D_801153F4` 2 hides the model, 0 runs the current state handler
/// (then colours it), 1 only colours it. Unless `field_451` is set, it then
/// runs `Actor04400_Fn00220` for three part pairs.
void Actor04400_Fn02E8C(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->idMap;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable9   sp    = Actor04400_D000EC;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            if (work->field_451 == 0) {
                Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
                Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
                Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0304C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn031B8);
