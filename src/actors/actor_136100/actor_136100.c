#include "common.h"

#include "actors/actor_136100.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"
#include "psyq/libgpu.h"

extern s8    D_8007218A;
extern u8    D_80073BA9;
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

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_801344AC);

/// Display-fade task: on its first tick it allocates the 8-byte `r`/`g`/`b`
/// block, then every frame draws the fade overlay and steps all three channels
/// up by `spawnArg1`.  The fade ends once `r` reaches 0x100, at which point the
/// task blanks the display and kills itself; `D_actor_136100_8013F17C` makes it
/// kill itself immediately instead (the cutscene wants the display back).
void func_actor_136100_80134588(Task* arg0)
{
    Actor136100FadeWork* fade;
    Actor136100FadeWork* alloc;

    fade = (Actor136100FadeWork*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            alloc       = (Actor136100FadeWork*)Mem_Malloc(8, 0);
            arg0->idMap = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r = (s16)((u16)fade->r + (u16)arg0->spawnArg1);
            fade->g = (s16)((u16)fade->g + (u16)arg0->spawnArg1);
            fade->b = (s16)((u16)fade->b + (u16)arg0->spawnArg1);
            if (D_actor_136100_8013F17C != 0) {
                Task_Kill(arg0);
                return;
            }
            if ((s16)fade->r < 0x100) {
                return;
            }
            SetDispMask(0);
            Task_Kill(arg0);
            break;
    }
}

void func_actor_136100_8013467C(void)
{
    GpRec14 rec;
    s32     weaponId;
    s32     id;

    weaponId     = D_80073BA9;
    id           = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.field_0  = id;
    rec.field_4  = 1;
    rec.field_8  = 0;
    rec.field_C  = 0;
    rec.field_10 = 0;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&rec, 0);
}
