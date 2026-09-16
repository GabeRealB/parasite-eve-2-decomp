#include "common.h"

#include "actors/actor_303600.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

extern Task*    D_actor_303600_8016E4C0;
extern Task*    D_actor_303600_8016E4C4;
extern TaskDesc D_actor_303600_80162E98;
INCLUDE_RODATA("actors/nonmatchings/actor_303600/actor_303600", D_actor_303600_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_80161F40);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_8016216C);

/// Fade-out driver: the same eight-byte channel block `func_actor_303600_801623CC`
/// walks up, walked the other way.  State 0 allocates it and fills all three
/// channels with 0xFF; a failed allocation kills the task outright.  State 1
/// draws the overlay tinted `r`/`g`/`r` in mode 1, steps all three channels down
/// by `Task::spawnArg1` -- the fade rate, not a colour -- and once `r` has gone
/// below zero clears `D_actor_303600_8016E4C4` before killing the task.
void func_actor_303600_801622E8(Task* arg0)
{
    Actor303600FadeWork* work;
    Actor303600FadeWork* alloc;

    work = (Actor303600FadeWork*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            alloc       = (Actor303600FadeWork*)Mem_Malloc(8, 0);
            arg0->idMap = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0xFF;
            work->g      = 0xFF;
            work->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 1);
            work->r -= (u16)arg0->spawnArg1;
            work->g -= (u16)arg0->spawnArg1;
            work->b -= (u16)arg0->spawnArg1;
            if ((s16)work->r < 0) {
                D_actor_303600_8016E4C4 = NULL;
                Task_Kill(arg0);
            }
            break;
    }
}

/// Fade-in driver: state 0 allocates the eight-byte channel block and clears
/// all three channels; a failed allocation kills the task outright.  State 1
/// runs every frame: it draws the overlay tinted `r`/`g`/`r` in mode 1, steps
/// all three channels by `Task::spawnArg1` -- the fade rate, not a colour -- and
/// once `r` has passed 0x100 clears `D_actor_303600_8016E4C4` before killing the
/// task.  The fade-out counterpart that walks the same block the other way, from
/// 0xFF down past zero, is `func_actor_303600_801622E8`.
void func_actor_303600_801623CC(Task* arg0)
{
    Actor303600FadeWork* work;
    Actor303600FadeWork* alloc;

    work = (Actor303600FadeWork*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            alloc       = (Actor303600FadeWork*)Mem_Malloc(8, 0);
            arg0->idMap = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0;
            work->g      = 0;
            work->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 1);
            work->r += (u16)arg0->spawnArg1;
            work->g += (u16)arg0->spawnArg1;
            work->b += (u16)arg0->spawnArg1;
            if ((s16)work->r >= 0x100) {
                D_actor_303600_8016E4C4 = NULL;
                Task_Kill(arg0);
            }
            break;
    }
}

/// One-shot announcement of the cutscene: while the work block's "message
/// outstanding" flag is still clear, hand the slot-4 task the session's two id
/// bytes plus selector 9 as message 0x7DA, record 9 in the work block and raise
/// the flag so the message goes out only once.
void func_actor_303600_801624B0(void)
{
    Actor303600Work*  work = (Actor303600Work*)D_actor_303600_8016E4C0->idMap;
    Actor303600Msg7DA msg;

    if (work->field_E == 0) {
        msg.field_0 = Game_Session->field_7;
        msg.field_1 = Game_Session->field_6;
        msg.field_2 = 9;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
        work->field_C = 9;
        work->field_E = 1;
    }
}

/// Cutscene teardown: kill the task a previous cutscene left in
/// `D_actor_303600_8016E4C4`, then, while the work block's message flag is
/// still clear, send the same 0x7DA announcement
/// `func_actor_303600_801624B0` sends and latch selector 9.  Finishes by
/// spawning the overlay's own continuation task -- `D_actor_303600_80162E98`
/// entry 3 -- so this runs exactly once per cutscene.
void func_actor_303600_8016253C(void)
{
    Actor303600Work*  work;
    Actor303600Msg7DA msg;

    if (D_actor_303600_8016E4C4 != NULL) {
        Task_Kill(D_actor_303600_8016E4C4);
        D_actor_303600_8016E4C4 = NULL;
    }

    work = (Actor303600Work*)D_actor_303600_8016E4C0->idMap;
    if (work->field_E == 0) {
        msg.field_0 = Game_Session->field_7;
        msg.field_1 = Game_Session->field_6;
        msg.field_2 = 9;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
        work->field_C = 9;
        work->field_E = 1;
    }

    Task_SpawnFromTable(&D_actor_303600_80162E98, 3, 0, 0);
}

void func_actor_303600_80162600(s16 arg0)
{
    Actor303600Work* work = (Actor303600Work*)D_actor_303600_8016E4C0->idMap;

    work->command = arg0;
    work->field_6 = 0;
}

void func_actor_303600_80162620(void)
{
    Gp_PulseState1C80();
    Gp_StateC08.field_6 |= 1;
}
