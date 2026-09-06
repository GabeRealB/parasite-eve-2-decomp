#include "common.h"

#include "main/task.h"
#include "gameplay/3A34.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"

extern TaskDesc D_mine_mesa_80189B2C;
extern Task*    D_mine_mesa_80189B4C;
extern GpObj4A  D_mine_mesa_801890EC[4];

extern void func_800E8614(s32 arg0, s32 arg1);
extern void func_800E8634(s32 arg0, s32 arg1, s32 arg2);
extern s32  D_mine_mesa_80184D9C;
extern s32  D_mine_mesa_80184FF4;
extern s32  D_mine_mesa_801850E4;
extern s32  D_mine_mesa_801854BC;
extern s32  D_mine_mesa_801856B4;

void func_mine_mesa_8017DD44(void);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017D8F8);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017DA7C);

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

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017DBC4);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017DC80);

void func_mine_mesa_8017DD44(void)
{
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[0]);
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[1]);
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[2]);
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[3]);
}
