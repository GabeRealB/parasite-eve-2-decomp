#include "common.h"

#include "actors/actor_121300.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80131EB0);

void func_actor_121300_801326EC(Task* arg0)
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
            fade    = alloc;
            fade->r = 0xFF;
            fade->g = 0xFF;
            fade->b = 0xFF;
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->b, 2);
            goto state_inc;
        case 2:
            SetDispMask(1);
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->b, 2);
        state_inc:
            arg0->state += 1;
            break;
        case 3:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->b, 2);
            fade->r = (s16)((u16)fade->r - (u16)arg0->spawnArg1);
            fade->g = (s16)((u16)fade->g - (u16)arg0->spawnArg1);
            fade->b = (s16)((u16)fade->b - (u16)arg0->spawnArg1);
            if ((s16)fade->r < 0) {
                Task_Kill(arg0);
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80132818);

INCLUDE_RODATA("actors/nonmatchings/actor_121300/actor_121300", D_actor_121300_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013293C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133064);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013322C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013343C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133580);

extern void     func_8017F334(s32 arg0);
extern void     func_8017F340(u8 arg0, u8 arg1);
extern TaskDesc ActorsShared80136280Desc;
extern s16      D_actor_121300_8013D41C;

/// Waypoint walker: while the current `Actor121300Work::field_49E` waypoint of
/// `D_actor_121300_8013CC20` is live, counts three frames on it, then retunes
/// the view through `func_8017F340`, bumps the value `func_8017F334` passes on
/// and spawns the `ActorsShared80136280Desc[3]` child seeded with the new
/// waypoint index.
void func_actor_121300_80133730(Task* arg0)
{
    Actor121300Work* work = (Actor121300Work*)arg0->idMap;

    switch (work->field_4A2) {
        case 0:
            D_actor_121300_8013D41C = 1;
            work->field_4A4         = 0;
            work->field_4A6         = 0;
            work->field_4A2        += 1;
            break;
        case 1:
            if (D_actor_121300_8013CC20[work->field_49E].field_6 != -1) {
                if ((s16)++work->field_4A4 >= 3) {
                    if (work->field_4A6 < 6) {
                        func_8017F340((u8)work->field_4A6, 1);
                    } else if (work->field_4A6 >= 7) {
                        func_8017F340((u8)(work->field_4A6 - 1), 1);
                    }
                    func_8017F334(work->field_4A6 + 1);
                    Task_SpawnFromTable(&ActorsShared80136280Desc, 3, work->field_4A6, 0);
                    work->field_4A4 = 0;
                    work->field_4A6 = (s16)((u16)work->field_4A6 + 1);
                }
            }
            break;
    }
}

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
