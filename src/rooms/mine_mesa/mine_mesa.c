#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern void func_800E8614(s32 arg0, s32 arg1);
extern void func_800E8634(s32 arg0, s32 arg1, s32 arg2);

extern s32 D_mine_mesa_80184D9C;
extern s32 D_mine_mesa_80184FF4;
extern s32 D_mine_mesa_801850E4;
extern s32 D_mine_mesa_801854BC;
extern s32 D_mine_mesa_801856B4;

void func_mine_mesa_8017DD44(void);

INCLUDE_RODATA("rooms/nonmatchings/mine_mesa/mine_mesa", D_mine_mesa_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017D670);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017D808);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017D8F0);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017D8F8);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017DA7C);

s32 func_mine_mesa_8017DABC(Task* task, s32 msgId, GpMsg13EF* msg, s32 arg3)
{
    switch (msg->field_2) {
        case 1:
            if (GameFlag_GetNibble(0x71) == 0) {
                if (Game_GetPtrSlot(0xA) != NULL) {
                    func_800E8614((s32)&D_mine_mesa_801850E4, 0);
                }
                func_800E3FAC(0xA2, 0x1C);
                GameFlag_SetNibble(0x71, 1);
                func_mine_mesa_8017DD44();
            }
            break;
        case 2:
            if (GameFlag_GetNibble(0x71) <= 0) {
                if (GameFlag_GetNibble(0x91) == 0) {
                    if (Game_GetPtrSlot(0xA) != NULL) {
                        func_800E8634((s32)&D_mine_mesa_80184D9C, 1, (s32)&D_mine_mesa_80184FF4);
                    }
                    GameFlag_SetNibble(0x91, 1);
                } else if (Game_GetPtrSlot(0xA) != NULL) {
                    func_800E8634((s32)&D_mine_mesa_801854BC, 1, (s32)&D_mine_mesa_801856B4);
                }
            }
            break;
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017DBC4);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017DC80);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017DD44);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017DD98);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017DDF0);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017DE38);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017DFC4);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E024);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E074);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E15C);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E2A4);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E3E0);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E5A0);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E5C0);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E5E0);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E600);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E620);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E650);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E684);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E6D8);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E70C);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E760);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E7B0);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E8B0);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E8FC);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E91C);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E93C);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E948);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017E978);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017EA24);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017EA78);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017EAAC);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017EAC0);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017EB18);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017EB38);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017EB54);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017ED08);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017EFA8);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017F230);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017F4D4);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017F900);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8017FC94);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_80180184);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa", func_mine_mesa_8018057C);

INCLUDE_RODATA("rooms/nonmatchings/mine_mesa/mine_mesa", D_mine_mesa_8017D624);

INCLUDE_RODATA("rooms/nonmatchings/mine_mesa/mine_mesa", jtbl_mine_mesa_8017D644);

INCLUDE_RODATA("rooms/nonmatchings/mine_mesa/mine_mesa", D_mine_mesa_8017D660);
