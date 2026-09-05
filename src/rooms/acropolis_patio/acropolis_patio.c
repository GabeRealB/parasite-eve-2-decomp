#include "common.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"

typedef struct {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4;
    /* 0x5 */ u8  field_5;
    /* 0x6 */ u16 field_6;
} AcropolisPatioMsg8;

/// Four-byte payload this room hands `Gp_DispatchMsg` as `arg2` for message
/// 0x7DB once the session is on its second visit.
typedef struct {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} AcropolisPatioSlotMsg;

extern s8       D_8007272D;
extern s16      D_80071076;
extern u8       D_8007216D;
extern TaskDesc D_acropolis_patio_801802BC;
extern s32      D_acropolis_patio_8018028C;
extern s32      D_acropolis_patio_80180428;
extern s32      D_acropolis_patio_80180440;
extern s32      D_acropolis_patio_8018044C;
extern s32      D_acropolis_patio_8018046C;
extern Task*    D_acropolis_patio_80187060;
extern s32      D_acropolis_patio_80180DEC;
extern s32      D_acropolis_patio_80180EDC;
extern u8       D_acropolis_patio_80187064;
extern u8       D_acropolis_patio_80187065;
extern s32      D_acropolis_patio_80180484;
extern s32      D_acropolis_patio_801806AC;
extern s32      D_acropolis_patio_8018082C;
extern s32      D_acropolis_patio_80180C64;
extern s32      D_acropolis_patio_8018280C;
extern s32      D_acropolis_patio_80182BE4;

/// Room entry task tick. Publishes the room's own record at
/// `Task::field_24` / pointer slot 7, then re-issues the messages the room's
/// actors need for the current point in the story: the first visit
/// (`GameFlag_GetNibble(0) < 2`) arms the two hotspots and spawns the arrival
/// cutscene, and the second-visit branches replace them according to
/// `Game_Session::field_9`.
void func_acropolis_patio_8017D5EC(Task* arg0)
{
    AcropolisPatioSlotMsg msg;
    s32                   temp;

    arg0->field_24 = &D_acropolis_patio_8018028C;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0) < 2) {
        if (D_8007216D == 1) {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D4, (s32)&D_acropolis_patio_80180428, 0);
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_acropolis_patio_8018044C, 0);
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D5, 1, 0);
            D_acropolis_patio_80187060 = Task_SpawnFromTable(&D_acropolis_patio_801802BC, 2, 0, 0);
        }
        temp = Gp_LookupSlot4(1);
        if (temp != 0) {
            Gp_DispatchMsg((Task*)temp, 0x7D4, (s32)&D_acropolis_patio_8018046C, 0);
        }
    }
    if ((Game_Session->field_9 == 1) && (GameFlag_GetNibble(0x21) < 2) && (GameFlag_GetNibble(0x21) < 2)) {
        temp = Gp_LookupSlot4(1);
        if (temp != 0) {
            Gp_DispatchMsg((Task*)temp, 0x7DB, (s32)&D_acropolis_patio_80180440, 0);
        }
    }
    if ((Game_Session->field_9 == 2) && (GameFlag_GetNibble(0x26) == 0)) {
        msg.field_0 = 1;
        msg.field_1 = 3;
        msg.field_2 = 0;
        Gp_DispatchMsg((Task*)Gp_LookupSlot4(2), 0x7DB, (s32)&msg, 0);
        Gp_DispatchMsg((Task*)Gp_LookupSlot4(3), 0x7DB, (s32)&msg, 0);
    }
    arg0->state = arg0->state + 1;
}

s32 func_acropolis_patio_8017D7D0(s32 arg0, s32 arg1, AcropolisPatioMsg8* arg2, AcropolisPatioMsg8* arg3)
{
    s32 var_v0;
    u16 temp_s1;

    *arg3 = *arg2;
    if (arg2->field_0 == 8) {
        if ((GameFlag_GetNibble(9) & 2) && (arg2->field_5 == 0)) {
            arg3->field_3 = 2;
        }
    }
    if (arg2->field_0 == 4) {
        if (GameFlag_GetNibble(8) < 2) {
            var_v0 = 0;
            if (arg2->field_5 == 0) {
                Gp_RunCapCmd1(3);
                GameFlag_SetNibble(8, 1);
                Gp_SetNibbleIf(arg2->field_6, 2);
                return 0;
            }
            return var_v0;
        }
        if (GameFlag_GetNibble(0) == 2) {
            var_v0 = 2;
            if (arg2->field_5 == 0) {
                if (GameFlag_GetNibble(0x23) == 0) {
                    func_800E8634((s32)&D_acropolis_patio_80180DEC, 0, (s32)&D_acropolis_patio_80180EDC);
                    GameFlag_SetNibble(0x23, 1);
                    return 2;
                }
                Gp_RunCapCmd1(8);
                return 2;
            }
            return var_v0;
        }
        if (GameFlag_GetNibble(8) == 2) {
            var_v0 = 2;
            if (arg2->field_5 == 0) {
                Task_SpawnFromTable(&D_acropolis_patio_801802BC, 1, 0, 0);
                Gp_SetItemSeenBit(0x101, 1);
                D_acropolis_patio_80187064 = arg2->field_2;
                D_acropolis_patio_80187065 = arg2->field_3;
                return 2;
            }
            return var_v0;
        }
        goto block_17;
    }
block_17:
    if ((arg2->field_0 == 8) && (GameFlag_GetNibble(0) < 5)) {
        var_v0 = 0;
        if (arg2->field_5 == 0) {
            Gp_SetNibbleIf(arg2->field_6, 2);
            Gp_RunCapCmd1(4);
            return 0;
        }
        return var_v0;
    }
    if ((arg2->field_5 == 0) && (GameFlag_GetNibble(0x21) == 3)) {
        GameFlag_SetNibble(0x21, 4);
    }
    temp_s1 = arg2->field_0;
    var_v0  = 1;
    if (temp_s1 == 4) {
        var_v0 = 1;
        if (arg2->field_5 == 0) {
            if (GameFlag_GetNibble(0) >= 3) {
                arg3->field_3 = (s8)temp_s1;
            }
            var_v0 = 1;
            if (GameFlag_GetNibble(0) == 2) {
                arg3->field_3 = 3;
                var_v0        = 1;
            }
        }
    }
    return var_v0;
}

void func_acropolis_patio_8017DA5C(Task* task)
{
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(3);
            goto advance;
        case 1:
            task->state = 2;
            return;
        case 2:
            if (Gp_GetCapEventKey() == state) {
                GameFlag_SetNibble(8, 3);
                SndEvt_EnqueueType6(0x51030004, 0, 0);
            advance:
                task->state = task->state + 1;
                return;
            }
            Gp_MsgPlayerWeapon(1);
            goto kill;
        case 3:
            if (SndVoice_HasActiveId(0x51030004) != 0) {
                return;
            }
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.field_6 = 4;
            D_80071076          = 1;
            Mc_SaveData.field_8 = D_acropolis_patio_80187064;
            Mc_SaveData.field_5 = D_acropolis_patio_80187065;
            Task_Spawn(0, 0x11, 0, 0);
        kill:
            Task_Kill(task);
            return;
    }
}

void func_acropolis_patio_8017DBAC(s32 arg0, s32 arg1, AcropolisPatioMsg8* arg2)
{
    u8 state;

    if ((arg2->field_2 == 0) && (GameFlag_GetNibble(0x21) < 2)) {
        GameFlag_SetNibble(0x21, 3);
        func_800E8634((s32)&D_acropolis_patio_80180484, 0, (s32)&D_acropolis_patio_801806AC);
        D_8007272D             = 3;
        Game_Session->field_69 = 0xC1;
    }
    if ((arg2->field_2 == 1) && (GameFlag_GetNibble(0x21) == 3) &&
        (Gp_DispatchMsg((Task*)Gp_LookupSlot4(1), 0x7D6, 0, 0) == 0)) {
        GameFlag_SetNibble(0x21, 4);
        func_800E8634((s32)&D_acropolis_patio_8018082C, 0, (s32)&D_acropolis_patio_80180C64);
    }
    state = arg2->field_2;
    if ((state == 2) && (GameFlag_GetNibble(0x26) == 0) && (GameFlag_GetNibble(0) == state)) {
        GameFlag_SetNibble(0x26, 1);
        func_800E8634((s32)&D_acropolis_patio_8018280C, 0, (s32)&D_acropolis_patio_80182BE4);
    }
}
s32 func_acropolis_patio_8017DCE4(s32 arg0, s32 arg1, s32 arg2)
{
    s32 var_v0;

    if (arg2 == 1) {
        Gp_SpawnIfCapIdle(1, 0);
    }
    var_v0 = 2;
    if (arg2 == 2) {
        var_v0 = GameFlag_GetNibble(0) < 2;
        if (var_v0 != 0) {
            Gp_SpawnIfCapIdle(2, 0);
            var_v0 = 0;
        }
    }
    return var_v0;
}
INCLUDE_RODATA("rooms/nonmatchings/acropolis_patio/acropolis_patio", D_acropolis_patio_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_patio/acropolis_patio", RoomsShared8017d878Table);
