#include "common.h"

#include "actors/actor_120300.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/task.h"
#include "main/tmd.h"

extern s8  D_8007218A;
extern u8  D_80073BA9;
extern s32 D_actor_120300_80140ACC;
extern s32 D_actor_120300_80140B2C;
extern s32 D_actor_120300_80140B5C;

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80131EE0);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80132004);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_801321C8);

INCLUDE_RODATA("actors/nonmatchings/actor_120300/actor_120300", D_actor_120300_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80132338);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80132C60);

/// Sets the actor up for play: clears the model's `field_C`, sends message
/// 0x7D5 to the actor and to the two slots at 0x4B8/0x4BC, and resets animation
/// slots 1..19 to the 8 it first parks in `field_4D4`.
/// `func_actor_120300_801337C4` calls it with 1 once flag nibble 0x2D is set; a
/// zero argument additionally hands the task at 0x4B4 the player-weapon record
/// (`GpRec14`, built from the equip-slot addend `D_80073BA9`), lifts
/// `field_4E0` to 0x1000 and drops the pending overlay replacement.  The
/// request codes at 0x4C0 and 0x4C8 are cleared either way, so any phase
/// counter armed alongside them restarts from the top.
void func_actor_120300_80133330(s32 arg0)
{
    Task*            task;
    Actor120300Work* work;
    Actor120300Work* animWork;
    SVECTOR          unused;
    GpRec14          rec;
    s32              i;
    s32              weaponId;
    s32              id;

    task                               = (Task*)D_actor_120300_80141BA8;
    work                               = (Actor120300Work*)task->idMap;
    ((TmdObject*)task->extra)->field_C = 0;
    Gp_DispatchMsg(task, 0x7D4, (s32)&D_actor_120300_80140B2C, 0);

    animWork            = (Actor120300Work*)task->idMap;
    animWork->field_4D4 = 8;
    i                   = 1;
    do {
        animWork->slots[(u16)i].field_9 = 0x10;
        Gp_AnimResetSlot(&animWork->anim, (u16)i, 8);
        i++;
    } while ((u16)i < 0x14U);

    Gp_DispatchMsg(work->field_4B8, 0x7D5, 1, 0);
    Gp_DispatchMsg(work->field_4BC, 0x7D5, 1, 0);
    Gp_DispatchMsg(work->field_4BC, 0x7D4, (s32)&D_actor_120300_80140B5C, 0);
    if (arg0 == 0) {
        weaponId     = D_80073BA9;
        id           = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
        rec.field_0  = id;
        rec.field_4  = 1;
        rec.field_8  = 0;
        rec.field_C  = 0;
        rec.field_10 = 1;
        Gp_DispatchMsg(work->field_4B4, 0x3E8, (s32)&rec, 0);
        Gp_DispatchMsg(work->field_4B4, 0x3E9, (s32)&D_actor_120300_80140ACC, 0);
        work->field_4E0 = 0x1000;
        CdCmd_CancelReplaceAndActivate();
    }
    work->field_4C0 = 0;
    work->field_4C8 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_801334A4);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_801335D8);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_801337C4);
