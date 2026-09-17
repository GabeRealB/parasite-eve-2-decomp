#include "common.h"

#include "actors/actor_120300.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern s8  D_8007218A;
extern u8  D_80073BA9;
extern s32 D_actor_120300_80140910;
extern s32 D_actor_120300_80140A44;
extern s32 D_actor_120300_80140ACC;
extern s32 D_actor_120300_80140B2C;
extern s32 D_actor_120300_80140B5C;

extern TaskDesc ActorsShared80134898Desc;

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

/// Spawn tick: allocates the 0x4E4-byte `Actor120300Work` block, zeroes it and
/// parks it in `Task::idMap`, then wires the model object up -- `Tmd_AllocBuffers`,
/// the work block's light/colour matrices into `TmdObject::field_1C` / `field_20`,
/// bit 2 of `TmdObject::field_C` cleared and the animation context handed to
/// `func_800B3F84` along with the work block's slot array.
/// The texture page / CLUT row come from the placement record at the nested
/// area table's `field_0` list whose id matches neither 0xFF (end) nor 0x6A
/// (the skip marker).  Animation slots 1..19 are then re-armed with slot count
/// 0xE, and the two helper tasks the shared table's entries 2 and 3 spawn are
/// reparented under this one, which also lifts `field_4E0` to 0x1000.
void func_actor_120300_801335D8(Task* arg0)
{
    Actor120300Work* work;
    TaskIdMap*       map;
    Actor120300Work* animWork;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    u8               id;
    s32              i;

    tmd         = arg0->extra;
    coord       = tmd->field_8;
    map         = Mem_Malloc(0x4E4, 0);
    arg0->idMap = map;
    if (map == NULL) {
        Task_Kill(arg0);
        return;
    }
    work = (Actor120300Work*)map;
    Mem_Set(work, 0, 0x4E4);
    work->field_4B4         = Game_GetPtrSlot(3);
    D_actor_120300_80141BA8 = (Actor120300*)arg0;
    coord->sub              = &Gfx_ViewCoord;
    Tmd_AllocBuffers(tmd);
    tmd->field_1C = &work->field_474;
    tmd->field_20 = &work->field_494;
    tmd->field_C &= 0xFFFB;
    place         = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&Game_Session->field_4)->field_0;
    id            = place->field_0;
    while (id != 0xFF) {
        if (id == 0x6A) {
            break;
        }
        place++;
        id = place->field_0;
    }
    Gp_SetTmdBytes(tmd, ((s8*)place)[0xD], ((s8*)place)[0xE]);
    func_800B3F84(&work->anim, &D_actor_120300_80140910, (GpAnimObj*)tmd, work->pad_334, work->slots);
    animWork            = (Actor120300Work*)arg0->idMap;
    animWork->field_4D4 = 0xE;
    i                   = 1;
    do {
        animWork->slots[(u16)i].field_9 = 0x10;
        Gp_AnimResetSlot(&animWork->anim, (u16)i, 0xE);
        i++;
    } while ((u16)i < 0x14U);
    work->field_4B8 = Task_SpawnFromTable(&ActorsShared80134898Desc, 2, 0, (s32)arg0);
    work->field_4BC = Task_SpawnFromTable(&ActorsShared80134898Desc, 3, 0, (s32)arg0);
    arg0->field_24  = &D_actor_120300_80140A44;
    work->field_4E0 = 0x1000;
    Task_Reparent(arg0, work->field_4B8);
    Task_Reparent(arg0, work->field_4BC);
}

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_801337C4);
