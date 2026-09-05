#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_motel_balcony.h"
#include "rooms/rooms_shared_8017db84.h"

extern TaskDesc             RoomsShared8017d638Desc;
extern RoomEventMsg         RoomsShared8017d638Msg;
extern u8                   RoomsShared8017d638Flag;
extern RoomEventReq         RoomsShared8017d638Req;
extern const TaskFuncTable3 RoomsShared8017d878Table;
void                        func_dryfield_motel_balcony_8017DBC8(void);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", D_dryfield_motel_balcony_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017D74C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017D8BC);

s32 func_dryfield_motel_balcony_8017DB1C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x8:
            Gp_EnqueueStageSnd6(0x521D0008, 0, 0);
            break;
        case 0x9:
            Gp_EnqueueStageSnd6(0x521D0009, 0, 0);
            break;
    }
    return 0;
}

s32 func_dryfield_motel_balcony_8017DB6C(void)
{
    return 0;
}

s32 func_dryfield_motel_balcony_8017DB74(void)
{
    return 0;
}

s32 func_dryfield_motel_balcony_8017DB7C(void)
{
    return 0;
}

const TaskFuncTable3 RoomsShared8017d878Table = {
    RoomsShared8017db84,
    (TaskFunc)func_dryfield_motel_balcony_8017DBC8,
    Task_Kill,
};
