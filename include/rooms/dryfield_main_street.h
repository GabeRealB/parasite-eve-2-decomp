#ifndef ROOMS_DRYFIELD_MAIN_STREET_H
#define ROOMS_DRYFIELD_MAIN_STREET_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// An event the room's message handler latches for its own event task. The
/// handler builds it on the stack and copies it whole. `field_0` is the CAP
/// command the task runs and `field_4` the stage sound it then plays;
/// `flagId` is the game-flag nibble set once the event has fired (zero: none);
/// a non-zero `field_A` makes the task start helper task 0x31.
typedef struct {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} DryfieldMainStreetLatchedEvent;

/// The message and event the message handler latched for the room's event
/// task.
extern RoomEventMsg                   D_dryfield_main_street_80185614;
extern DryfieldMainStreetLatchedEvent D_dryfield_main_street_80185634;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 D_dryfield_main_street_8018562C;

/// The room entry task's three states: set the room up, idle, end.
extern const TaskFuncTable3 D_dryfield_main_street_8017D5F4;

/// The task the room's spawn helper starts; cleared or killed by the room's
/// other entry points.
extern Task* D_dryfield_main_street_80185630;

s32  func_dryfield_main_street_8017D798(RoomEventReq* req, RoomEventMsg* msg);
void func_dryfield_main_street_8017F18C(GsCOORDINATE2* coord, s32 arg1, s32 arg2, u8* rgb);
void func_dryfield_main_street_8017EA88(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_dryfield_main_street_8017F5B8(GsCOORDINATE2* coord, s32 arg1, u8* rgb);
void func_dryfield_main_street_801804BC(GsCOORDINATE2* coord, s16 arg1, u8* rgb);

#endif
