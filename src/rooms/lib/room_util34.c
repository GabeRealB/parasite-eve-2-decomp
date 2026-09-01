#include "common.h"

#include "main/task.h"

/// Scratch state of the room's cap script, stored at `Task::idMap`. Shared by
/// `dryfield_factory` and `dryfield_night_factory`.
typedef struct {
    /* 0x0 */ u8  pad_0[0xA];
    /* 0xA */ s16 field_A;
} RoomUtil34State;

/// Room script callback: raise the `field_A` flag in the script's scratch state.
void Room_Util34(Task* task)
{
    ((RoomUtil34State*)task->idMap)->field_A = 1;
}
