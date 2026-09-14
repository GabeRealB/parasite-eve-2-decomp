#include "common.h"
#include "actors/actor_104400.h"
#include "actors/actors_shared_801639a8.h"
#include "actors/actors_shared_8016945c.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

void Actor04400_Fn006A8(Task* arg0);
/* Reads the caller's Task* from $a0; the call passes no argument. */
void Actor04400_Fn02B8C();
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

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn00220);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn006A8);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn01B70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn01CA0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn01E08);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn02E8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn0304C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text", Actor04400_Fn031B8);
