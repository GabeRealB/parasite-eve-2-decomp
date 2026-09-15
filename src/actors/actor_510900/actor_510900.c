#include "common.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "actors/actor_510900.h"
#include "actors/actors_shared_8013bbe4.h"

void Gp_UpdateCoord(Actor510900Coord* arg0);
void Gp_WorldToLocal(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2);
void func_actor_510900_80135744(Actor510900* arg0);
void func_actor_510900_8013864C(Actor510900* arg0);
void func_actor_510900_801387F4(Actor510900* arg0);
void func_actor_510900_80138978(Actor510900* arg0);
void func_actor_510900_80138A9C(Actor510900* arg0);
void func_actor_510900_80138BF0(Actor510900* arg0);
void func_actor_510900_80138D38(Actor510900* arg0);
void func_actor_510900_80138F44(Actor510900* arg0);
void func_actor_510900_8013B804(Actor510900* arg0);
void func_actor_510900_8013BB20(Actor510900* arg0);
void func_actor_510900_8013BC38(Actor510900* arg0, Actor510900Coord* arg1);
void func_actor_510900_8013BC80(Actor510900* arg0);

extern u8 D_801153F4;

INCLUDE_RODATA("actors/nonmatchings/actor_510900/actor_510900", D_actor_510900_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80131F24);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80132D4C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801332EC);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013371C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80133C84);

void func_actor_510900_801340E8(Task* arg0)
{
    Actor510900Cam*         base;
    GsCOORDINATE2*          cam;
    Actor510900CamCoord*    ext;
    GpEffWork*              eff;
    Actor510900Coord*       coord;
    Actor510900MatrixWords* mat;
    s32                     i;

    base  = &D_8011505C;
    cam   = &base->cam.coord;
    eff   = arg0->spawnArg2;
    coord = ((Actor510900Obj2C*)arg0->extra)->field_8;
    ext   = (Actor510900CamCoord*)cam;
    if (Gp_State1C->field_4 != 0) {
        Gp_ReleaseState1CMem(eff, arg0);
        return;
    }
    mat                       = (Actor510900MatrixWords*)&coord->field_0.coord;
    coord->field_0.sub        = eff->field_8;
    mat->m00_m01              = 0x1000;
    mat->m02_m10              = 0;
    mat->m11_m12              = 0x1000;
    mat->m20_m21              = 0;
    mat->m22                  = 0x1000;
    coord->field_0.coord.t[0] = eff->field_18;
    coord->field_0.coord.t[1] = eff->field_1A;
    coord->field_0.coord.t[2] = eff->field_1C;
    coord->field_0.flg        = 0;
    Gp_UpdateCoord(coord);
    eff->field_10 = -0x200;
    eff->field_12 = 0x40;
    eff->field_14 = 0;
    Gp_SpawnEff(0x6003B, &coord->field_0, 0x180, (SVECTOR*)&eff->field_10);
    for (i = 0; i < 6; i++) {
        Gp_SpawnEff(0x60065, &coord->field_0, 0, (SVECTOR*)&eff->field_10);
        Gp_SpawnEff(0x600A4, &coord->field_0, 1, NULL);
    }
    base->field_0 = 4;
    ext->field_58 = 0xFA0;
    ext->field_5C = 0x12C0;
    ext->rot.vx   = 0xC00;
    ext->rot.vy   = 0x800;
    ext->rot.vz   = 0x400;
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->field_0.workm, &cam->coord);
    cam->flg = 0;
    Gp_ReleaseState1CMem(eff, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80134284);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801346D4);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013482C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80134C90);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801350F8);

void func_actor_510900_801355B4(Actor510900Ctx* arg0, Actor510900* arg1)
{
    Actor510900Coord* coord;
    Actor510900Work*  work;
    s32               snd;
    s32               pan;
    s32               pan2;
    s32               i;

    work           = arg1->field_1C;
    coord          = arg1->field_2C->field_8;
    arg0->field_14 = 1;
    if (work->field_586 == 0x20 && work->field_58A == 0xD2) {
        work->field_594 = 1;
        work->field_598 = 0xFF;
        snd             = (((u16)arg0->field_8 >> 0xC) << 8) | 0x4078000E;
        pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        work->field_580 = (((u16)arg0->field_8 >> 0xC) << 8) | 0x40780011;
        pan2            = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(work->field_580, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
    work->field_58A++;
    for (i = 1; i < 0x13; i++) {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
    }
    coord->field_0.flg = 0;
    Gp_UpdateCoord(coord);
    func_actor_510900_8013BC38(arg1, coord);
    if (work->field_594 != work->field_596) {
        if (work->field_564 != NULL) {
            work->field_564[0xD] = work->field_594;
        }
        work->field_596 = work->field_594;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80135744);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80135E90);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80136184);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013691C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80136B70);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80137008);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801373B8);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801375D8);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80137868);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80137E20);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80137FBC);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80138250);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801384C4);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013864C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801387F4);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80138978);

/// Fires the actor's step sounds: while the current animation record carries
/// `field_3` bit 0x20 or 0x10, a sound is queued on the frame that bit has just
/// dropped from `Actor510900Work::field_59A`, panned and depth-attenuated from
/// the actor's attach coordinate. The record's two bits are latched for the
/// next frame at the end.
void func_actor_510900_80138A9C(Actor510900* arg0)
{
    s32               snd;
    s32               pan;
    s32               pan2;
    Actor510900Work*  work;
    Actor510900Coord* coord;
    GpAnimRec*        rec;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    rec   = Gp_AnimGetRec((GpAnimCtx*)work, (GpAnimSlot*)&work->obj38.prev);
    if (rec != NULL) {
        if (!(rec->field_3 & 0x20) && (work->field_59A & 0x20)) {
            snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780001;
            pan = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
        if (!(rec->field_3 & 0x10) && (work->field_59A & 0x10)) {
            snd  = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780002;
            pan2 = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
        work->field_59A = (u16)(rec->field_3 & 0x30);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80138BF0);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80138D38);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80138F44);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801391B8);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801395AC);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801397F0);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80139C10);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013A100);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013A310);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013A5B8);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013A85C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013A9BC);

void func_actor_510900_8013AD90(GpEnemy* enemy, Task* task)
{
    GsCOORDINATE2*          coord;
    Actor510900MatrixWords* mat;
    Actor510900ChildWork*   work;

    coord = ((Actor510900Obj2C*)task->extra)->field_8;
    work  = Mem_Calloc(sizeof(Actor510900ChildWork), false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    mat               = (Actor510900MatrixWords*)&coord->coord;
    task->idMap       = (TaskIdMap*)work;
    mat->m00_m01      = 0x1000;
    mat->m11_m12      = 0x1000;
    mat->m22          = 0x1000;
    mat->m02_m10      = 0;
    mat->m20_m21      = 0;
    coord->coord.t[0] = -0x17D4;
    coord->coord.t[1] = -0x456;
    coord->coord.t[2] = 0x17C;
    coord->sub        = &Gfx_ViewCoord;
    coord->flg        = 0;
    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    Gp_LinkNode(&enemy->node);
    enemy->field_18     = coord;
    enemy->node.field_4 = 1;
    enemy->field_1C.vx  = 0;
    enemy->field_1C.vy  = 0;
    enemy->field_1C.vz  = 0;
    work->obj0.field_8  = coord;
    work->obj0.field_10 = 0;
    work->obj0.field_12 = 0;
    work->obj0.field_14 = 0;
    work->obj0.field_C  = &work->rec20;
    work->obj0.field_18 = 0;
    work->obj0.field_1C = 0x12C;
    work->obj0.flags    = 1;
    Gp_LinkObj(2, &work->obj0);
    Gp_InitRec18Table(&work->rec20, 1, 0);
    work->obj38.field_12 = -0x200;
    work->obj38.field_8  = coord;
    work->obj38.field_10 = 0;
    work->obj38.field_14 = 0;
    work->obj38.field_C  = &work->rec58;
    work->obj38.field_18 = 0;
    work->obj38.field_1C = 0x200;
    work->obj38.flags    = 1;
    work->obj0.flags    &= 0x7FFF;
    Gp_LinkObj(8, &work->obj38);
    Gp_InitRec18Table(&work->rec58, 1, 0);
    work->obj38.flags &= 0x7FFF;
    task->exitCallback = (TaskFunc)func_actor_510900_8013C430;
    task->state        = 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013AF38);

INCLUDE_RODATA("actors/nonmatchings/actor_510900/actor_510900", ActorsShared80135df4Table);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013B0D8);

void func_actor_510900_801350F8(Actor510900Ctx* arg0, Actor510900* arg1);
void func_actor_510900_8013B658(Actor510900Ctx* arg0, Actor510900* arg1);

void func_actor_510900_8013B3D0(Task* task)
{
    void (*fns[2])(Actor510900Ctx*, Actor510900*) = { func_actor_510900_801350F8, func_actor_510900_8013B658 };

    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013B424);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013B524);

void func_actor_510900_8013B608(Actor510900* arg0)
{
    Actor510900Work* work = arg0->field_1C;

    Gp_UnlinkObj(&work->obj47C);
    Gp_UnlinkObj(&work->obj4E4);
    Gp_UnlinkObj(&work->obj504);
    Gp_DestroyEnemy(arg0->field_20, (Task*)arg0);
}

void func_actor_510900_8013B658(Actor510900Ctx* arg0, Actor510900* arg1)
{
    if (Game_Session->field_1 != 0) {
        func_actor_510900_801355B4(arg0, arg1);
        return;
    }
    func_actor_510900_8013B6A0(arg0, arg1);
}

void func_actor_510900_8013B6A0(Actor510900Ctx* arg0, Actor510900* arg1)
{
    Actor510900Coord* temp_s1;
    Actor510900Obj2C* temp_a1;
    Actor510900Work*  temp_s2;
    s32               one;
    s32               sp;

    temp_s2 = arg1->field_1C;
    temp_a1 = arg1->field_2C;
    temp_s1 = temp_a1->field_8;
    if (temp_s2->field_5A4 != 0) {
        sp  = D_801153F4;
        one = 1;
        if (sp == one) {
            goto case1;
        }
        if (sp >= 2) {
            goto ge2;
        }
        if (sp == 0) {
            goto case0;
        }
        goto default_body;
    ge2:
        if (sp == 2) {
            goto case2;
        }
        goto default_body;
    case0:
        temp_a1->field_C = 0;
        arg0->field_14   = 8;
        goto default_body;
    case1:
        ActorsShared8013bbe4((ActorShared8013bbe4*)arg1);
        func_actor_510900_8013BC38(arg1, temp_s1);
        return;
    case2:
        temp_a1->field_C = 0x80;
        arg0->field_14   = one;
        return;
    default_body:
        if (arg0->field_4C != 0) {
            func_actor_510900_8013B804(arg1);
        }
        func_actor_510900_80135744(arg1);
        func_actor_510900_8013864C(arg1);
        func_actor_510900_8013B870(arg1);
        func_actor_510900_801387F4(arg1);
        func_actor_510900_80138978(arg1);
        func_actor_510900_80138A9C(arg1);
        func_actor_510900_8013BB20(arg1);
        func_actor_510900_80138BF0(arg1);
        if (temp_s2->field_584 != 0) {
            func_actor_510900_80138D38(arg1);
        }
        temp_s1->field_0.flg                  = 0;
        arg1->field_2C->field_8->field_50.flg = 0;
        Gp_UpdateCoord(temp_s1);
        ActorsShared8013bbe4((ActorShared8013bbe4*)arg1);
        func_actor_510900_8013BC38(arg1, temp_s1);
        func_actor_510900_80138F44(arg1);
        func_actor_510900_8013BC80(arg1);
    }
}

void func_actor_510900_8013B804(Actor510900* arg0)
{
    Actor510900Work* work;
    GpEnemy*         enemy;
    u8               flags;

    enemy = arg0->field_20;
    flags = enemy->field_4C;
    work  = arg0->field_1C;
    if (flags & 1) {
        enemy->field_4C = flags & 0xFE;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= 0xFD;
        work->field_58E  = 7;
        work->field_590  = 0;
        work->field_5B8  = 1;
    }
    flags = enemy->field_4C;
    if (flags & 0xC) {
        enemy->field_4C = flags & 0xF3;
    }
}
