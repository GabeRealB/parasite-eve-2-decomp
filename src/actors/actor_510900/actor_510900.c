#include "common.h"
#include "main/sound.h"
#include "main/task.h"

#include "actors/actor_510900.h"
#include "actors/actors_shared_8013bbe4.h"

void Gp_UpdateCoord(Actor510900Coord* arg0);
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

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801340E8);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80134284);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801346D4);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013482C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80134C90);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801350F8);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801355B4);

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

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013AD90);

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
