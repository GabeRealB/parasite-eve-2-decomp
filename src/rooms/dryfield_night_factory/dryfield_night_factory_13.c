#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

extern TaskDesc* D_dryfield_night_factory_8018A7E0;
extern TaskDesc* D_dryfield_night_factory_8018A7E4;
extern Task**    D_dryfield_night_factory_8018A7E8;

extern TaskDesc   D_dryfield_night_factory_80186DE0[];
extern TaskDesc   D_dryfield_night_factory_80186E28[];
extern GpMsgEntry D_dryfield_night_factory_80186E64[];
extern TaskDesc   D_dryfield_night_factory_80186E94[];
extern TaskDesc   D_dryfield_night_factory_80186EA0[];

extern void Room_Util16(s32);

void func_dryfield_night_factory_8018076C(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            *D_dryfield_night_factory_8018A7E8 = Task_SpawnFromTable(D_dryfield_night_factory_8018A7E0, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(*D_dryfield_night_factory_8018A7E8, &poll) != 0) {
                taskKill(task);
            }
            return;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_13", RoomsShared8017fc38Table);
