#include "common.h"

#include "actors/actor_136100.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"
#include "psyq/libgpu.h"

extern s32   D_actor_136100_8013F1A0;
extern s32   D_actor_136100_8013F2F4;
extern Task* D_actor_136100_8014078C;

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80131EC4);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80131FBC);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_801320E0);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132284);

INCLUDE_RODATA("actors/nonmatchings/actor_136100/actor_136100", D_actor_136100_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_801323F8);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132748);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132BC0);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132E78);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133238);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133558);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133690);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_8013379C);

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
