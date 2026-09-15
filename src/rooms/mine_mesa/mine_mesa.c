#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"

INCLUDE_RODATA("rooms/nonmatchings/mine_mesa/mine_mesa", D_mine_mesa_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017D670);

extern s8 D_80114C12;

extern s32 D_mine_mesa_80184664;
extern s32 D_mine_mesa_80184BA4;
extern s32 D_mine_mesa_8018578C;
extern s32 D_mine_mesa_801861DC;
extern s32 D_mine_mesa_80189B50;

void func_mine_mesa_8017D808(void)
{
    u8  field9;
    s32 nibble;

    if ((Game_Session->field_1 == 0) && (D_80114C12 != 1) && (field9 = Game_Session->field_9, field9 == 1)) {
        if (GameFlag_GetNibble(0x90) == 0) {
            if (Game_GetPtrSlot(0xA) != NULL) {
                func_800E8634((s32)&D_mine_mesa_8018578C, 0, (s32)&D_mine_mesa_801861DC);
            }
            func_800E3FAC(0xA2, 0x1B);
            GameFlag_SetNibble(0x90, 1);
            return;
        }
        nibble = GameFlag_GetNibble(0xCD);
        if ((nibble == field9) && (D_mine_mesa_80189B50 == nibble)) {
            func_800E8634((s32)&D_mine_mesa_80184664, 0, (s32)&D_mine_mesa_80184BA4);
            D_mine_mesa_80189B50 = 2;
        }
    }
}

INCLUDE_RODATA("rooms/nonmatchings/mine_mesa/mine_mesa", RoomsShared8017d878Table);
