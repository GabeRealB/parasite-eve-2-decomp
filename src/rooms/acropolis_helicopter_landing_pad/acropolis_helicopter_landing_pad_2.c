#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"
#include "main/unknown_syms.h"

extern s32      D_acropolis_helicopter_landing_pad_80183A34;
extern s32      D_acropolis_helicopter_landing_pad_80183FA4;
extern s32      D_acropolis_helicopter_landing_pad_80184D9C;
extern TaskDesc RoomsShared8017e5b8Desc;
extern s32      D_acropolis_helicopter_landing_pad_80184E0C;
extern GpObj4A  D_acropolis_helicopter_landing_pad_80185FAC[];
extern s32      D_acropolis_helicopter_landing_pad_80187F84;

/// Slot-3 msg `0x3EF` handler. On kind 0, once the room session flag
/// `D_acropolis_helicopter_landing_pad_80184E0C` is up and the phase is
/// still 0, starts the helicopter sequence: flags the session, loads the
/// bank pair, moves to phase 1 and swaps the visible `GpObj4A` from element
/// 4 to element 0. Kind 1 latches `D_acropolis_helicopter_landing_pad_80187F84`.
s32 func_acropolis_helicopter_landing_pad_8017E4A4(Task* task, s32 msgId, GpMsg13EF* msg)
{
    u8       kind;
    GpObj4A* obj;
    GpObj4A* obj2;

    if ((msg->field_2 == 0) && (D_acropolis_helicopter_landing_pad_80184D9C == 0) && (D_acropolis_helicopter_landing_pad_80184E0C != 0)) {
        Game_Session->field_69 = 0x82;
        D_80062735             = 1;
        func_800E8634((s32)&D_acropolis_helicopter_landing_pad_80183A34, 0, (s32)&D_acropolis_helicopter_landing_pad_80183FA4);
        D_acropolis_helicopter_landing_pad_80184D9C = 1;
        obj                                         = D_acropolis_helicopter_landing_pad_80185FAC;
        obj2                                        = obj + 4;
        obj->field_4A                              |= 0x40;
        obj2->field_4A                             &= 0xBF;
    }
    kind = msg->field_2;
    if (kind == 1) {
        D_acropolis_helicopter_landing_pad_80187F84 = kind;
    }
    return 0;
}

s32 func_acropolis_helicopter_landing_pad_8017E570(s32 arg0, s32 arg1, s32 arg2)
{
    if ((arg2 == 4) && (D_acropolis_helicopter_landing_pad_80184D9C == 2)) {
        Task_SpawnFromTable(&RoomsShared8017e5b8Desc, 4, 0, 0);
    }
    return 0;
}
