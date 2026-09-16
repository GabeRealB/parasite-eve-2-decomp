#include "common.h"

#include "actors/actor_303600.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"

extern Task* D_actor_303600_8016E4C0;
INCLUDE_RODATA("actors/nonmatchings/actor_303600/actor_303600", D_actor_303600_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_80161F40);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_8016216C);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_801622E8);

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_801623CC);

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

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600", func_actor_303600_8016253C);

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
