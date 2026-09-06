#include "common.h"

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/acropolis_bridge.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017ed38.h"

extern s16            D_80114D08;
extern TaskFuncTable9 D_acropolis_bridge_8017D614;

s32 RoomsShared8017ecb4(RoomHotspot* table, s16 x, s16 y);

/// Nine-state dispatcher of this room's script task: copies the handler table
/// out of the overlay's rodata onto the stack and tails into the entry named by
/// `Task::state`.
void func_acropolis_bridge_8017F788(Task* task)
{
    TaskFuncTable9 states;

    states = D_acropolis_bridge_8017D614;
    states.funcs[task->state](task);
}
