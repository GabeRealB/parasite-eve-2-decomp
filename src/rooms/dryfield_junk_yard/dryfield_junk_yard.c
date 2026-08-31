#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_dryfield_junk_yard_8017DD20;
extern s32 D_dryfield_junk_yard_8017DD88;
extern s32 D_dryfield_junk_yard_8017DE00;
extern s32 D_dryfield_junk_yard_8017DE30;
extern s32 D_dryfield_junk_yard_8017E490;
extern s32 D_dryfield_junk_yard_8017E658;

INCLUDE_ASM("rooms/nonmatchings/dryfield_junk_yard/dryfield_junk_yard", func_dryfield_junk_yard_8017D5F4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_junk_yard/dryfield_junk_yard", func_dryfield_junk_yard_8017D658);

void func_dryfield_junk_yard_8017D708(Task* arg0)
{
    arg0->field_24 = &D_dryfield_junk_yard_8017DD20;
    Game_SetPtrSlot(arg0, 7);
    if (Game_GetPtrSlot(0xA) != 0) {
        if (GameFlag_GetNibble(0x38) == 0) {
            Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_junk_yard_8017DE00, 0);
            Gp_AllyAnimId(&D_dryfield_junk_yard_8017DD88);
            Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E8, (s32)&D_dryfield_junk_yard_8017DD88, 0);
        }
        if ((GameFlag_GetNibble(0x39) == 0) && (GameFlag_GetNibble(0x28) >= 2)) {
            GameFlag_SetNibble(0x39, 1);
            func_800E8634((s32)&D_dryfield_junk_yard_8017E490, 0, (s32)&D_dryfield_junk_yard_8017E658);
        } else if (Game_Session->field_8 == 2) {
            Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_junk_yard_8017DE30, 0);
        }
    }
    arg0->state = (s32)(arg0->state + 1);
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_junk_yard/dryfield_junk_yard", D_dryfield_junk_yard_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_junk_yard/dryfield_junk_yard", D_dryfield_junk_yard_8017D5C4);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_junk_yard/dryfield_junk_yard", D_dryfield_junk_yard_8017D5D0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_junk_yard/dryfield_junk_yard", func_dryfield_junk_yard_8017D848);
