#include "common.h"
#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/session.h"
#include "main/task.h"

extern s32      D_acropolis_cafeteria_80184164;
extern TaskDesc D_acropolis_cafeteria_80182AD8[];
extern RECT     D_acropolis_cafeteria_80184168;
extern RECT     D_acropolis_cafeteria_80184170;

s32 func_acropolis_cafeteria_8017E0DC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        if (GameFlag_GetNibble(0) >= 2 || D_acropolis_cafeteria_80184164 >= 2) {
            if (Gp_GetCurBit2Flag(4) == 1 || Gp_GetCurBit2Flag(4) == 0) {
                Gp_StartCapSlot(7, 1, 0);
            }
        }
    }
    return 0;
}
/// Handler for slot-7 msg `0x13EF`: the directed action selected by `field_2`.
INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_4", func_acropolis_cafeteria_8017E154);
INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_4", func_acropolis_cafeteria_8017E22C);
void func_acropolis_cafeteria_8017E27C(s32 arg0)
{
    ((GameActor*)((Task*)Game_GetPtrSlot(3))->idMap)->field_930 = arg0;
}

void func_acropolis_cafeteria_8017E2B0(void)
{
    Gp_PulseState1C80();
}

void func_acropolis_cafeteria_8017E2D0(void)
{
    MoveImage(&D_acropolis_cafeteria_80184168, 0x180, 0x100);
    MoveImage(&D_acropolis_cafeteria_80184170, 0, 0xF7);
}
