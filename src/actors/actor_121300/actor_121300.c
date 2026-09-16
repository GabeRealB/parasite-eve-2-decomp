#include "common.h"

#include "actors/actor_121300.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80131EB0);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_801326EC);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80132818);

INCLUDE_RODATA("actors/nonmatchings/actor_121300/actor_121300", D_actor_121300_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013293C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133064);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013322C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013343C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133580);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133730);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133854);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133BFC);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133D98);

void func_actor_121300_8013400C(Task* arg0)
{
    Actor121300FadeWork* fade;
    Actor121300FadeWork* alloc;

    fade = (Actor121300FadeWork*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            alloc       = (Actor121300FadeWork*)Mem_Malloc(8, 0);
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
            if ((s16)fade->r < 0x100) {
                return;
            }
            SetDispMask(0);
            Task_Kill(arg0);
            break;
    }
}

void func_actor_121300_801340F0(void)
{
    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
}
