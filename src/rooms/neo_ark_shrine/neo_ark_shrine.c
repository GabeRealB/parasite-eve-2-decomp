#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

void func_neo_ark_shrine_8017F448(void);

extern void func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

/// Message table installed at `Task::field_24` by the room task's state 0.
extern GpMsgEntry D_neo_ark_shrine_80181E34[];

extern TaskDesc D_neo_ark_shrine_80181E5C;

extern TaskDesc D_neo_ark_shrine_80182508;

/// Task spawned in state 0, polled by `Task_PollKill` and cleared in state 1.
extern Task* D_neo_ark_shrine_80186864;

s32 func_neo_ark_shrine_8017D6AC(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179B14(in, out);
    if (in->msgId != 0x11) {
        return 1;
    }
    if (GameFlag_GetNibble(0xDB) != 0) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_SetNibbleIf(in->field_6, 2);
    Gp_RunCapCmd1(4);
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", func_neo_ark_shrine_8017D740);

s32 func_neo_ark_shrine_8017D7F0(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 1) {
        if (GameFlag_GetNibble(0xDF) == 0) {
            Task_SpawnFromTable(&D_neo_ark_shrine_80181E5C, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(9);
        }
    }
    return 0;
}

void func_neo_ark_shrine_8017D84C(Task* task)
{
    s32 sp10;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            D_neo_ark_shrine_80186864 = Task_SpawnFromTable(&D_neo_ark_shrine_80182508, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(D_neo_ark_shrine_80186864, &sp10) != 0) {
                D_neo_ark_shrine_80186864 = NULL;
                Task_Kill(task);
            }
            return;
    }
}

void func_neo_ark_shrine_8017D8F4(Task* task)
{
    task->field_24 = D_neo_ark_shrine_80181E34;
    Game_SetPtrSlot(task, 7);
    func_neo_ark_shrine_8017F448();
    task->state++;
}

void func_neo_ark_shrine_8017D940(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", D_neo_ark_shrine_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", func_neo_ark_shrine_8017D948);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", func_neo_ark_shrine_8017D9A0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", func_neo_ark_shrine_8017DB10);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", RoomsShared80180294States);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", RoomsShared80181e70Table);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", D_neo_ark_shrine_8017D620);
