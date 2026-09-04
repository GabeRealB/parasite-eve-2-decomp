#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern SVECTOR       D_acropolis_sanctuary_8017D5D0;
extern GpMsgEntry    D_acropolis_sanctuary_8018081C[];
extern RoomPlacement D_acropolis_sanctuary_801808BC;
extern s32           D_acropolis_sanctuary_80180AE8;
extern u8            D_acropolis_sanctuary_80181814[];
extern TaskDesc      D_acropolis_sanctuary_80182240;

extern void func_acropolis_sanctuary_8017DD78(void);
extern void func_acropolis_sanctuary_8017DF88(s32 arg0, s32 arg1);

s32 func_acropolis_sanctuary_8017D810(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0 && GameFlag_GetNibble(6) == 0) {
        func_800E8614((s32)&D_acropolis_sanctuary_80181814, 0);
    }
    return 0;
}

/// Message gate for the sanctuary hotspot registered under id 0x13EF: sub-id 1
/// arms the room's own task the first time it is seen, latching nibble 7 so a
/// second visit does nothing. The record is not copied to the outgoing one -
/// this handler only ever consumes the message (returns 0).
s32 func_acropolis_sanctuary_8017D848(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->field_2 == 1 && GameFlag_GetNibble(7) == 0) {
        GameFlag_SetNibble(7, 1);
        Task_SpawnFromTable(&D_acropolis_sanctuary_80182240, 0, 0, 0);
    }
    return 0;
}

void func_acropolis_sanctuary_8017D8A0(u32 arg0)
{
    func_acropolis_sanctuary_8017DF88((arg0 >> 8) & 0xFF, arg0 & 0xFF);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017D8CC);

/// State 0 of the sanctuary room task: publishes the room's message-handler
/// table under pointer slot 7 and advances to the next state. Unless nibble 6
/// has already reached 1 it also chains slot-4 message list 1 onto itself and,
/// when nibble 2 is set and that slot holds a task, places the actor by sending
/// it the 0x7D3 animation record followed by the 0x7D4 placement.
void func_acropolis_sanctuary_8017D930(Task* arg0)
{
    Task* slot;

    arg0->field_24 = D_acropolis_sanctuary_8018081C;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = arg0->state + 1;
    if (GameFlag_GetNibble(6) != 1) {
        slot = (Task*)Gp_LookupSlot4(1);
        Gp_MsgSlot4Chain(1, 1);
        if (GameFlag_GetNibble(2) != 0 && slot != NULL) {
            Gp_DispatchMsg(slot, 0x7D3, (s32)&D_acropolis_sanctuary_80180AE8, 0);
            Gp_DispatchMsg(slot, 0x7D4, (s32)&D_acropolis_sanctuary_801808BC, 0);
        }
    }
    func_acropolis_sanctuary_8017DD78();
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017D9E8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017DA40);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017DCE0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017DD78);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017DF88);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017E00C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017E134);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017E338);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017EC90);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017F4E8);

/// Spawns effect 0x60078 on the room task's model coordinate, seeded with the
/// fixed offset vector held in this unit's rodata. Always consumes the event
/// (returns 0).
s32 func_acropolis_sanctuary_8017F918(Task* task)
{
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->field_8;
    SVECTOR        vec   = D_acropolis_sanctuary_8017D5D0;

    Gp_SpawnEff(0x60078, coord, 0, &vec);
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017F974);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017FB18);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_80180264);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_801802E0);
