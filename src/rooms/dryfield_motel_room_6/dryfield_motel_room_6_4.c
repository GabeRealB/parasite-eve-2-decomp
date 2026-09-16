#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern TaskDesc D_8013843C;
extern TaskDesc D_dryfield_motel_room_6_80182D78;
extern Task*    D_dryfield_motel_room_6_80186828;

void func_dryfield_motel_room_6_80181910(void)
{
}

s32 func_dryfield_motel_room_6_80181918(void)
{
    return 0;
}

/// Handler for a message `0x14` request: copies the incoming record to the
/// outgoing one, then answers 1 while the request is not the one this room
/// waits for or the 0x54 nibble is already latched. Otherwise, with no
/// sub-state pending, it latches nibble 0x54 and runs cap command 7, and
/// answers 0 either way.
/// Same gate as `func_neo_ark_shrine_8017D6AC` and
/// `func_shelter_b3_incinerator_control_room_8017FA8C`, which also latch a
/// nibble and run a cap command.
s32 func_dryfield_motel_room_6_80181920(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId != 0x14) {
        return 1;
    }
    if (GameFlag_GetNibble(0x54) != 0) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    GameFlag_SetNibble(0x54, 1);
    Gp_RunCapCmd1(7);
    return 0;
}

/// Handler for a slot-7 msg `0x13EF` request (`GpMsg13EF`) whose sub-id
/// (`field_2`) is clear: the first time it runs it latches nibble 0x31 and
/// arms the room's script task from `D_dryfield_motel_room_6_80182D78`.
/// Where the sibling gates of this shape (`func_acropolis_security_room_8017D740`,
/// `func_acropolis_sanctuary_8017D848`) answer 0, this one answers 1.
s32 func_dryfield_motel_room_6_801819A8(Task* arg0, s32 arg1, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 0 && GameFlag_GetNibble(0x31) == 0) {
        GameFlag_SetNibble(0x31, 1);
        Task_SpawnFromTable(&D_dryfield_motel_room_6_80182D78, 0, 0, 0);
    }
    return 1;
}

s32 func_dryfield_motel_room_6_80181A00(void)
{
    return 0;
}

/// Spawns this room's event task from entry 1 of `D_8013843C`, keeps it in
/// `D_dryfield_motel_room_6_80186828`, waits for it to be killed and then kills
/// this task. Same shape as `func_dryfield_gas_station_8017FE20`.
void func_dryfield_motel_room_6_80181A08(Task* arg0)
{
    s32 out;

    switch (arg0->state) {
        case 0:
            D_dryfield_motel_room_6_80186828 = Task_SpawnFromTable(&D_8013843C, 1, 0, 0);
            arg0->state++;
            break;
        case 1:
            if (Task_PollKill(D_dryfield_motel_room_6_80186828, &out) != 0) {
                arg0->state++;
            }
            break;
        case 2:
            Task_Kill(arg0);
            break;
    }
}
