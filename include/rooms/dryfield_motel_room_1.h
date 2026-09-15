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
/// consumes and `field_2E` the sub-state counter reset alongside it.
///
/// `func_dryfield_motel_room_1_8017DFD0` copies the player matrix translation
/// (`Wip_SysConfig.field_4->t[0..2]`) into `field_14` .. `field_1C` and hands
/// `&field_14` to the slot-4 task as the three-word payload of message 0x3E9;
/// `field_24` / `field_26` / `field_28` are the halfwords it stages next to that
/// payload, still as `0` / `0x500` / `0`. `field_20` stays unidentified.
typedef struct Dmr1Work {
    /* 0x00 */ Task* field_0;
    /* 0x04 */ Task* field_4;
    /* 0x08 */ Task* field_8;
    /* 0x0C */ Task* field_C;
    /* 0x10 */ Task* field_10;
    /* 0x14 */ s32   field_14;
    /* 0x18 */ s32   field_18;
    /* 0x1C */ s32   field_1C;
    /* 0x20 */ byte  pad_20[0x4];
    /* 0x24 */ s16   field_24;
    /* 0x26 */ s16   field_26;
    /* 0x28 */ s16   field_28;
    /* 0x2A */ byte  pad_2A[0x2];
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

/// Room entry point: allocate the `Dmr1Work` the room task hangs off
/// `Task::idMap` (killing the task if the allocation fails), zero it, park the
/// slot-3 task in `field_0` and the room task itself in
/// `D_dryfield_motel_room_1_8018159C`, then resolve the four placed objects
/// `field_4` .. `field_10` from the session id: the base id, then the id with
/// the 0x1000 / 0x2000 / 0x3000 index of `Gp_FindWorkById`'s search key.
void func_dryfield_motel_room_1_8017DC2C(Task* arg0);

/// Install the player's weapon animation set on slot 3 (message 0x3E8: the
/// equip-slot id `D_80073BA9` plus 1 in the alternate weapon block, plus 0x22
/// in the base one, `field_4` 9, the rest of the frame zero), then copy the
/// player matrix translation into `Dmr1Work::field_14` .. `field_1C` and send
/// them back to slot 4 as message 0x3E9. Same slot-3 record the actors'
/// `func_actor_341900_801635A4` builds.
void func_dryfield_motel_room_1_8017DFD0(void);

/// Set the room's action index, resetting the sub-state counter that goes with
/// it - the same body as `func_actor_444000_801327E8`.
void func_dryfield_motel_room_1_8017DFB0(s16 arg0);

/// Arm the player's weapon, then re-issue the room task's messages: the 0x7DA
/// poke at the slot-4 task and both 0x7D4 placements.
void func_dryfield_motel_room_1_8017DF08(void);

#endif // ROOMS_DRYFIELD_MOTEL_ROOM_1_H
