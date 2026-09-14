#include "common.h"
#include "actors/actor_104400.h"
#include "actors/actors_shared_801639a8.h"
#include "actors/actors_shared_8016945c.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

void Actor04400_Fn00220(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s32 arg4, u8 arg5);
void Actor04400_Fn006A8(Task* arg0);
void Actor04400_Fn022A8(Task* arg0, s16 arg1);
/* Reads the caller's Task* from $a0; the call passes no argument. */
void Actor04400_Fn02B8C();
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

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn00B24);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn022A8);

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
