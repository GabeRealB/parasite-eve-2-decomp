#include "common.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "rooms/mine_refuge.h"

extern u8         D_8007216C;
extern u8         D_80062735;
extern u8         D_80115598;
extern s32        D_mine_refuge_80182AD8;
extern u8         D_mine_refuge_80182ADC;
extern GpMsgEntry D_mine_refuge_80181884[];
extern TaskDesc   D_mine_refuge_80181860;
extern TaskDesc   D_mine_refuge_801818B4;

INCLUDE_RODATA("rooms/nonmatchings/mine_refuge/mine_refuge_4", RoomsShared8017ef20Title);
INCLUDE_RODATA("rooms/nonmatchings/mine_refuge/mine_refuge_4", RoomsShared8017de9cHundred);
INCLUDE_RODATA("rooms/nonmatchings/mine_refuge/mine_refuge_4", RoomsShared8017e8b4WeaponTitle);
INCLUDE_RODATA("rooms/nonmatchings/mine_refuge/mine_refuge_4", RoomsShared8017e8b4PeTitle);
INCLUDE_RODATA("rooms/nonmatchings/mine_refuge/mine_refuge_4", RoomsShared8017ea68Title);

void func_mine_refuge_8017FE78(s32 arg0);

s32 func_mine_refuge_8017FC2C(Task* task, s32 msgId, s32 arg2)
{
    u8 temp_a3;

    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x12B) != 0) {
            func_mine_refuge_8017FE78(0U);
        } else {
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            temp_a3                  = Mc_SaveData.at4.loc.view;
            Mc_SaveData.at4.loc.view = 6U;
            D_mine_refuge_80182ADC   = temp_a3;
            SndEvt_EnqueueType6(0x54060003, 0, 0);
            Gp_RunCapCmd(0xD, 0);
            Task_SpawnFromTable(&D_mine_refuge_801818B4, 1, 0, 0);
        }
    }
    return 0;
}

s32 func_mine_refuge_8017FCD0(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    u8 temp_s0 = arg2->field_2;

    if (temp_s0 == 1) {
        if (GameFlag_GetNibble(0xBB) != temp_s0) {
            GameFlag_SetNibble(0xC4, 0);
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_mine_refuge_801818B4, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(0xA);
        }
    }
    return 0;
}

/// The `0x13F2` message handler of `D_mine_refuge_80181884`: arguments 0xC, 0x63
/// and 0x67 each cue a sound (ids 0x5406000C, 0x5406000F and 0x5406000D),
/// centred and at zero depth. Any other argument is ignored.
s32 func_mine_refuge_8017FD48(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0xC:
            SndEvt_EnqueueType6(0x5406000C, 0, 0);
            break;
        case 0x63:
            SndEvt_EnqueueType6(0x5406000F, 0, 0);
            break;
        case 0x67:
            SndEvt_EnqueueType6(0x5406000D, 0, 0);
            break;
    }
    return 0;
}

void func_mine_refuge_8017FDBC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if (Gp_CapBusy() != 0) {
                return;
            }
            if (Gp_GetCapEventKey() == 5) {
                arg0->state = arg0->state + 1;
                return;
            }
            D_8007216C = D_mine_refuge_80182ADC;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            break;
        case 1:
            GameFlag_SetNibble(0x12B, 1);
            func_mine_refuge_8017FE78(D_mine_refuge_80182ADC);
            break;
        default:
            return;
    }
    Task_Kill(arg0);
}

/// Fills `D_mine_refuge_80182AE0` with the script for the mine's epilogue cap
/// and spawns it. `arg0` is the area id to force (negative in `field_0` means
/// "keep the current area"); zero instead pins area 6 and cues its sound. The
/// `0x155` progress flag picks between the long and the short version of the
/// scene: slot 1 / file 1 / no sound when it is `0xF`, slot 5 / file 0 and the
/// `0x54060006` cue otherwise.
void func_mine_refuge_8017FE78(s32 arg0)
{
    s32 slot;

    if (arg0 != 0) {
        D_mine_refuge_80182AE0.field_4 = 0;
        D_mine_refuge_80182AE0.field_0 = -arg0;
    } else {
        D_mine_refuge_80182AE0.field_0 = 6;
        D_mine_refuge_80182AE0.field_4 = 0x54060003;
    }
    if (GameFlag_GetNibble(0x155) == 0xF) {
        slot                           = 1;
        D_mine_refuge_80182AE0.field_1 = 0xE;
        D_mine_refuge_80182AE0.field_3 = 0;
    } else {
        slot                           = 5;
        D_mine_refuge_80182AE0.field_1 = 1;
        D_mine_refuge_80182AE0.field_3 = 1;
    }
    D_mine_refuge_80182AE0.field_2  = 0;
    D_mine_refuge_80182AE0.field_8  = 0x54060006;
    D_mine_refuge_80182AE0.field_10 = 0x54060004;
    D_mine_refuge_80182AE0.field_C  = 0x54060005;
    Task_SpawnFromTable(&D_mine_refuge_80181860, 0, slot, (s32)&D_mine_refuge_80182AE0);
}

void func_mine_refuge_8017FF4C(Task* arg0)
{
    arg0->msgTable = D_mine_refuge_80181884;
    Game_SetPtrSlot(arg0, 7);
    D_mine_refuge_80182AD8 = 0;
    D_80062735             = 1;
    arg0->state            = arg0->state + 1;
    D_80115598             = 1;
}
