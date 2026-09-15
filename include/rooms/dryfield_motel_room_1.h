#ifndef ROOMS_DRYFIELD_MOTEL_ROOM_1_H
#define ROOMS_DRYFIELD_MOTEL_ROOM_1_H

#include "common.h"

#include "main/task.h"

#include "rooms/room_common.h"

/// Work block hung off `Task::idMap` (0x1C) of the room task parked in
/// `D_dryfield_motel_room_1_8018159C`, which every entry point in this overlay
/// reaches the room state through.
///
/// `func_dryfield_motel_room_1_8017DC2C` allocates it (`Mem_Malloc(0x38)`) and
/// fills `field_0` from `Game_GetPtrSlot(3)` and `field_4` .. `field_10` from
/// `Gp_FindWorkById(session id | index)`, which makes `field_C` / `field_10`
/// the two placed objects `func_dryfield_motel_room_1_8017DF08` addresses its
/// 0x7D4 placements to. `field_2C` is an action index the room's script driver
/// consumes and `field_2E` the sub-state counter reset alongside it; the span
/// between 0x14 and 0x2C is not identified yet, so the struct carries it as
/// padding.
typedef struct Dmr1Work {
    /* 0x00 */ byte  pad_0[0xC];
    /* 0x0C */ Task* field_C;
    /* 0x10 */ Task* field_10;
    /* 0x14 */ byte  pad_14[0x18];
    /* 0x2C */ s16   field_2C;
    /* 0x2E */ s16   field_2E;
} Dmr1Work;
STATIC_ASSERT_SIZEOF(Dmr1Work, 0x30);

/// Four-byte payload `func_dryfield_motel_room_1_8017DF08` hands
/// `Gp_DispatchMsg` as `arg2` for message 0x7DA: the session's two id bytes
/// followed by the halfword the 0x7DB receiver switches on. Same record the
/// actors' `ActorsShared80132724` sends.
typedef struct Dmr1Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Dmr1Msg7DA;
STATIC_ASSERT_SIZEOF(Dmr1Msg7DA, 0x4);

/// The room's script-driver task, whose `idMap` holds a `Dmr1Work`.
extern Task* D_dryfield_motel_room_1_8018159C;

/// The two objects the room task places, passed as `Gp_DispatchMsg`'s `arg2`
/// for message 0x7D4 - `[0]` to `Dmr1Work::field_C`, `[1]` to `field_10`.
extern RoomPlacement D_dryfield_motel_room_1_8017E130[2];

/// Set the room's action index, resetting the sub-state counter that goes with
/// it - the same body as `func_actor_444000_801327E8`.
void func_dryfield_motel_room_1_8017DFB0(s16 arg0);

/// Arm the player's weapon, then re-issue the room task's messages: the 0x7DA
/// poke at the slot-4 task and both 0x7D4 placements.
void func_dryfield_motel_room_1_8017DF08(void);

#endif // ROOMS_DRYFIELD_MOTEL_ROOM_1_H
