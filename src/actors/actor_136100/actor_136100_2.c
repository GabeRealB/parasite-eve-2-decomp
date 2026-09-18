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
extern Task* D_actor_136100_8014078C;

/// Display-fade task: on its first tick it allocates the 8-byte `r`/`g`/`b`
/// block, then every frame draws the fade overlay and steps all three channels
/// up by `spawnArg1`.  The fade ends once `r` reaches 0x100, at which point the
/// task blanks the display and kills itself; `D_actor_136100_8013F17C` makes it
/// kill itself immediately instead (the cutscene wants the display back).
void func_actor_136100_80134588(Task* arg0)
{
    Actor136100FadeWork* fade;
    Actor136100FadeWork* alloc;

    fade = (Actor136100FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor136100FadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
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
