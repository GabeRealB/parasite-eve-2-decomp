#include "common.h"

#include "actors/actor_136100.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "psyq/libgpu.h"

extern s8    D_8007218A;
extern u8    D_80073BA9;
extern s32   D_actor_136100_8013F1A0;
extern s32   D_actor_136100_8013F2F4;
extern s32   D_actor_136100_8013F31C;
extern s32   D_actor_136100_8013F334;
extern s32   D_actor_136100_8013F3AC;
extern s32   D_actor_136100_8013F40C;
extern Task* D_actor_136100_8014078C;

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80131EC4);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80131FBC);

/// Spawn tick of the cutscene actor's second phase: allocates the 0x4F0-byte
/// `Actor136100Work` block, zeroes it and parks it in `Task::idMap`, then wires
/// the model object up -- `Tmd_AllocBuffers`, `TmdObject::field_C` cleared, the
/// work block's light/colour matrices into `TmdObject::field_1C` / `field_20`
/// and the animation-context task reparented under `D_actor_136100_8014078C`.
/// The texture page / CLUT row come from the placement record at the nested
/// area table's `field_0` list whose id matches neither 0xFF (end) nor 0x6A
/// (the skip marker), and this all runs even on the `Mem_Malloc` failure path,
/// which still advances the state after killing the task.
///
/// The dead `VECTOR` is read back through `arg0->extra` rather than the local
/// `obj`, which is what makes the original reload `Task::extra` for each of the
/// three coordinate reads (see `func_actor_136100_80132284`).
void func_actor_136100_801320E0(Task* arg0)
{
    Actor136100Work* work;
    VECTOR           vec;
    GpAreaPlace*     place;
    u8               id;

    if (arg0->state == 0) {
        TmdObject*     tmd   = arg0->extra;
        GsCOORDINATE2* coord = tmd->field_8;

        work        = Mem_Malloc(0x4F0, 0);
        arg0->idMap = (TaskIdMap*)work;
        if (work == NULL) {
            Task_Kill(arg0);
        } else {
            Mem_Set(work, 0, 0x4F0);
            coord->sub                         = (GsCOORDINATE2*)arg0->spawnArg2;
            ((TmdObject*)arg0->extra)->field_C = 0;
            Tmd_AllocBuffers(tmd);
            tmd->field_1C  = &work->field_474;
            tmd->field_20  = &work->field_494;
            arg0->field_24 = &D_actor_136100_8013F2F4;
            Task_Reparent(D_actor_136100_8014078C, arg0);
        }
        place = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&Game_Session->field_4)->field_0;
        id    = place->field_0;
        while (id != 0xFF) {
            if (id == 0x6A) {
                break;
            }
            place++;
            id = place->field_0;
        }
        Gp_SetTmdBytes((TmdObject*)arg0->extra, ((s8*)place)[0xD], ((s8*)place)[0xE]);
        arg0->state += 1;
    }
    {
        TmdObject* obj = arg0->extra;

        Gp_UpdateCoord(obj->field_8);
        vec.vx = ((TmdObject*)arg0->extra)->field_8->workm.t[0];
        vec.vy = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
        vec.vz = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
        func_800D7A9C(obj, &vec, 0, 3);
    }
}

void func_actor_136100_80132284(Task* arg0)
{
    Actor136100Work* work;
    VECTOR           vec;

    if (arg0->state == 0) {
        TmdObject*     tmd   = arg0->extra;
        GsCOORDINATE2* coord = tmd->field_8;

        work        = Mem_Malloc(0x4F0, 0);
        arg0->idMap = (TaskIdMap*)work;
        if (work == NULL) {
            Task_Kill(arg0);
        } else {
            Mem_Set(work, 0, 0x4F0);
            coord->sub                         = (GsCOORDINATE2*)arg0->spawnArg2;
            ((TmdObject*)arg0->extra)->field_C = 0;
            Tmd_AllocBuffers(tmd);
            tmd->field_1C  = &work->field_474;
            tmd->field_20  = &work->field_494;
            arg0->field_24 = &D_actor_136100_8013F2F4;
            Task_Reparent(D_actor_136100_8014078C, arg0);
        }
        arg0->state += 1;
        if (arg0->spawnArg1 != 0) {
            GsCOORDINATE2* reset = ((TmdObject*)arg0->extra)->field_8;

            Gfx_RotMatrixX(&reset->coord, 0x400, 1);
            reset->coord.t[1] = 0xC8;
            reset->flg        = 0;
        }
    }
    {
        TmdObject* obj = arg0->extra;

        Gp_UpdateCoord(obj->field_8);
        vec.vx = ((TmdObject*)arg0->extra)->field_8->workm.t[0];
        vec.vy = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
        vec.vz = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
        func_800D7A9C(obj, &vec, 0, 3);
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_136100/actor_136100", D_actor_136100_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_801323F8);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132748);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132BC0);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132E78);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133238);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133558);

/// Reset the cutscene actor's animation state and re-send the weapon record.
///
/// Clears the first two value/countdown pairs, re-arms all nineteen animation
/// slots through `Gp_AnimResetSlot` with the work block's slot count at 1, then
/// sends slot 3 the 0x3E9 placement and the 0x3E8 weapon record
/// (`GpRec14`) built from the equip-slot addend (`D_80073BA9`), the pair
/// `func_actor_136100_8013467C` sends on its own.  `field_4DE` is armed on the
/// way past.
///
/// Three separate `task->idMap` loads are what the original reaches the block
/// with -- the stores to `field_4C4` / `field_4CC` invalidate the first in cse,
/// and the first is still live for the 0x3E9 send after the loop.  The dead
/// `SVECTOR` is not read; it reserves the 8-byte local the frame has between
/// the outgoing-arg area and `rec` (see `func_actor_136100_801347B8`).
void func_actor_136100_80133690(void)
{
    Task*            task;
    Actor136100Work* work;
    Actor136100Work* animWork;
    Actor136100Work* msgWork;
    SVECTOR          unused;
    GpRec14          rec;
    s32              i;
    s32              weaponId;
    s32              id;

    task            = D_actor_136100_8014078C;
    work            = (Actor136100Work*)task->idMap;
    work->field_4C4 = 0;
    work->field_4CC = 0;

    animWork            = (Actor136100Work*)task->idMap;
    animWork->field_4E0 = 1;
    i                   = 1;
    do {
        animWork->slots[(u16)i].field_9 = 0x10;
        Gp_AnimResetSlot(&animWork->anim, (u16)i, 1);
        i++;
    } while ((u16)i < 0x14U);

    Gp_DispatchMsg(work->field_4B4, 0x3E9, (s32)&D_actor_136100_8013F31C, 0);

    msgWork            = (Actor136100Work*)task->idMap;
    weaponId           = D_80073BA9;
    id                 = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.field_0        = id;
    msgWork->field_4DE = 1;
    rec.field_4        = 1;
    rec.field_8        = 0;
    rec.field_C        = 0;
    rec.field_10       = 0;
    Gp_DispatchMsg(msgWork->field_4B4, 0x3E8, (s32)&rec, 0);
}

/// Second half of the cutscene actor's re-arm: clears the first two
/// value/countdown pairs, sends the 0x7D4 cue to the host task, re-arms all
/// nineteen animation slots with the work block's slot count at 3, then re-sends
/// the two placement cues and the 0x3E8 weapon record (`GpRec14`) built from the
/// equip-slot addend (`D_80073BA9`).  `arg0 == 1` additionally resets the
/// fourth bone's rotation to zero.
///
/// Two `task->idMap` loads reach the block: the stores to `field_4C4` /
/// `field_4CC` invalidate the first in cse, and it is still live for the 0x3E9
/// and 0x3E9/`field_4C0` sends after the loop.  The dead `SVECTOR` is not read;
/// it reserves the 8-byte local the frame has between the outgoing-arg area and
/// `rec` (see `func_actor_136100_80133690`).
void func_actor_136100_8013379C(s32 arg0)
{
    Task*            task;
    Actor136100Work* work;
    Actor136100Work* animWork;
    Actor136100Work* msgWork;
    SVECTOR          unused;
    GpRec14          rec;
    s32              i;
    s32              weaponId;
    s32              id;

    task            = D_actor_136100_8014078C;
    work            = (Actor136100Work*)task->idMap;
    work->field_4C4 = 0;
    work->field_4CC = 0;

    Gp_DispatchMsg(task, 0x7D4, (s32)&D_actor_136100_8013F3AC, 0);

    animWork            = (Actor136100Work*)task->idMap;
    animWork->field_4E0 = 3;
    i                   = 1;
    do {
        animWork->slots[(u16)i].field_9 = 0x10;
        Gp_AnimResetSlot(&animWork->anim, (u16)i, 3);
        i++;
    } while ((u16)i < 0x14U);

    Gp_DispatchMsg(work->field_4B4, 0x3E9, (s32)&D_actor_136100_8013F334, 0);

    msgWork            = (Actor136100Work*)task->idMap;
    weaponId           = D_80073BA9;
    id                 = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.field_0        = id;
    msgWork->field_4DE = 1;
    rec.field_4        = 1;
    rec.field_8        = 0;
    rec.field_C        = 0;
    rec.field_10       = 0;
    Gp_DispatchMsg(msgWork->field_4B4, 0x3E8, (s32)&rec, 0);

    Gp_DispatchMsg(work->field_4C0, 0x3E9, (s32)&D_actor_136100_8013F40C, 0);

    if (arg0 == 1) {
        Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8[4].coord, 0, 1);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133904);

/// First tick of the cutscene actor: allocates the 0x4F0-byte
/// `Actor136100Work` block, zeroes it and parks it in `Task::idMap`, then wires
/// the model object up -- `Tmd_AllocBuffers`, the work block's light/colour
/// matrices into `TmdObject::field_1C` / `field_20`, bit 2 of `TmdObject::field_C`
/// cleared and the animation context handed to `func_800B3F84`.
/// The texture page / CLUT row come from the placement record at the nested
/// area table's `field_0` list whose id matches neither 0xFF (end) nor 0x6A
/// (the skip marker).
void func_actor_136100_80133A88(Task* arg0)
{
    Actor136100Work* work;
    TaskIdMap*       map;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    u8               id;

    tmd         = arg0->extra;
    coord       = tmd->field_8;
    map         = Mem_Malloc(0x4F0, 0);
    arg0->idMap = map;
    if (map == NULL) {
        Task_Kill(arg0);
        return;
    }
    work = (Actor136100Work*)map;
    Mem_Set(work, 0, 0x4F0);
    work->field_4B4         = Game_GetPtrSlot(3);
    D_actor_136100_8014078C = arg0;
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
    func_800B3F84(&work->anim, &D_actor_136100_8013F1A0, (GpAnimObj*)tmd, work->pad_334, work->slots);
    arg0->field_24 = &D_actor_136100_8013F2F4;
}

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133BC8);
