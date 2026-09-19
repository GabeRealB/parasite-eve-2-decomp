#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "rooms/mine_mesa.h"

extern u8         D_801153F4;
extern u8         D_80115690;
extern s16        D_80071076;
extern GpStateBD8 D_mine_mesa_80189B38;
extern u32        D_mine_mesa_80189B64;
extern u8         D_mine_mesa_80189B6A;

/// Runs this room's pending event once the request for it has been accepted.
/// State 0 plays the caption command recorded in `D_mine_mesa_80189B60` and
/// saves a point; state 1 spawns the helper task 0x31 the request asked for;
/// state 2 queues the stage sound and state 3 waits for that voice to end,
/// either of which falls through to state 4 - the commit, which plays the
/// event's sound, copies the saved location into `Mc_SaveData` and loads it.
void func_mine_mesa_8017D670(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_mine_mesa_80189B60.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_mine_mesa_80189B6A != 0) {
                    D_mine_mesa_80189B38.field_0 = 0;
                    D_mine_mesa_80189B38.field_1 = 0;
                    D_mine_mesa_80189B38.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_mine_mesa_80189B38);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_mine_mesa_80189B64 != 0) {
                Gp_EnqueueStageSnd6(D_mine_mesa_80189B64, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_mine_mesa_80189B64)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_mine_mesa_80189B40.field_0;
            Mc_SaveData.at4.loc.warp = D_mine_mesa_80189B40.field_2;
            Mc_SaveData.at4.loc.room = D_mine_mesa_80189B40.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

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

    if ((gGameSession->eventState == 0) && (D_80114C12 != 1) && (field9 = gGameSession->at4.loc.place, field9 == 1)) {
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
