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
void                        func_dryfield_motel_balcony_8017DBC8(Task* arg0);

/// Task entry points the shared room-event body dispatches through. It is a
/// unit of its own because nothing it could join owns rodata beside it: the run
/// before belongs to a shared body compiled once for every room carrying it,
/// which cannot hold one room's function pointers, and no room-local rodata
/// follows.
const TaskFuncTable3 RoomsShared8017d878Table = {
    RoomsShared8017db84,
    func_dryfield_motel_balcony_8017DBC8,
    taskKill,
};
