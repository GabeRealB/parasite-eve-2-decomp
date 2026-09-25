#ifndef ROOMS_DRYFIELD_MOTEL_ROOM_1_H
#define ROOMS_DRYFIELD_MOTEL_ROOM_1_H

#include "common.h"

#include "gameplay/3CD8.h"

#include "main/task.h"

#include "rooms/room.h"
#include "rooms/room_common.h"

/// Work block hung off `Task::work` (0x1C) of the room task parked in
/// `D_dryfield_motel_room_1_8018159C`, which every entry point in this overlay
/// reaches the room state through.
///
/// `func_dryfield_motel_room_1_8017DC2C` allocates it (`Mem_Malloc(0x38)`) and
/// fills `field_0` from `gameGetPtrSlot(3)` and `field_4` .. `field_10` from
/// `Gp_FindWorkById(session id | index)`, which makes `field_C` / `field_10`
/// the two placed objects `func_dryfield_motel_room_1_8017DF08` addresses its
/// 0x7D4 placements to. `field_2C` is an action index the room's script driver
/// consumes and `field_2E` the sub-state counter reset alongside it.
///
/// `func_dryfield_motel_room_1_8017DFD0` copies the player matrix translation
/// (`Player_Status.coordMtx->t[0..2]`) into `field_14` .. `field_1C` and hands
/// `&field_14` to the slot-4 task as the three-word payload of message 0x3E9;
/// `field_24` / `field_26` / `field_28` are the halfwords it stages next to that
/// payload, still as `0` / `0x500` / `0`. `field_20` stays unidentified.
///
/// Action 6 of `func_dryfield_motel_room_1_8017D7AC` turns an angle in
/// `field_34`: it starts from the slot-3 actor's facing plus 0xC00, wrapped to
/// 12 bits, and steps by 0x96 a frame, with `field_26` kept 0x400 ahead of it.
/// `field_30` counts the frames of that action's closing step.
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
    /* 0x2C */ u16   field_2C;
    /* 0x2E */ u16   field_2E;
    /* 0x30 */ u16   field_30;
    /* 0x32 */ byte  pad_32[0x2];
    /* 0x34 */ s16   field_34;
    /* 0x36 */ byte  pad_36[0x2];
} Dmr1Work;
STATIC_ASSERT_SIZEOF(Dmr1Work, 0x38);

/// The one scratch buffer `func_dryfield_motel_room_1_8017DD3C` builds both of
/// its payloads in, which is why they share a frame slot: `rec` is the 0x14-byte
/// slot-3 record message 0x3E8 takes (`GpAnimArg`, `animBlock.index` the equipped weapon's
/// animation id, `field_4` / `field_8` 1, `field_C` 5, `field_10` 0) and `msg` the
/// `GpCmdArg` the 0x7DA poke takes in states 1 and 2. Same shape as the
/// breezeway's `DbwMsgBuf`.
typedef union Dmr1MsgBuf {
    /* 0x0 */ GpAnimArg rec;
    /* 0x0 */ GpCmdArg  msg;
} Dmr1MsgBuf;
STATIC_ASSERT_SIZEOF(Dmr1MsgBuf, 0x14);

/// The script driver's scratch buffer. `rec` and `msg` share its start, as in
/// `Dmr1MsgBuf`; the first step of action 6 builds its 0x3E8 record in
/// `shifted.rec`, eight bytes further in, for no reason the code shows.
typedef union Dmr1DriverBuf {
    /* 0x0 */ GpAnimArg rec;
    /* 0x0 */ GpCmdArg  msg;
    struct {
        /* 0x0 */ s32       pad[2];
        /* 0x8 */ GpAnimArg rec;
    } shifted;
} Dmr1DriverBuf;
STATIC_ASSERT_SIZEOF(Dmr1DriverBuf, 0x1C);

/// The room's script-driver task, whose `work` holds a `Dmr1Work`.
extern Task* D_dryfield_motel_room_1_8018159C;

/// The two objects the room task places, passed as `Gp_DispatchMsg`'s `arg2`
/// for message 0x7D4 - `[0]` to `Dmr1Work::field_C`, `[1]` to `field_10`.
extern GpXformArg D_dryfield_motel_room_1_8017E130[2];

/// The placements the script driver's actions 1 and 2 send as message 0x7D4:
/// `[0]` to `Dmr1Work::field_4`, `[1]` to `field_8`.
extern GpXformArg D_dryfield_motel_room_1_8017E0D0[2];
extern GpXformArg D_dryfield_motel_room_1_8017E100[2];

/// Room entry point: allocate the `Dmr1Work` the room task hangs off
/// `Task::work` (killing the task if the allocation fails), zero it, park the
/// slot-3 task in `field_0` and the room task itself in
/// `D_dryfield_motel_room_1_8018159C`, then resolve the four placed objects
/// `field_4` .. `field_10` from the session id: the base id, then the id with
/// the 0x1000 / 0x2000 / 0x3000 index of `Gp_FindWorkById`'s search key.
void func_dryfield_motel_room_1_8017DC2C(Task* arg0);

/// The room's script driver: runs the action `func_dryfield_motel_room_1_8017DFB0`
/// left in `Dmr1Work::field_2C`. Actions 1 and 2 send the 0x7DA message to the
/// slot-4 task and one of the two placement pairs as message 0x7D4 (action 2
/// also sends 0x3F3 to the slot-3 task); 3, 4 and 5 play a sound. Each of these
/// runs once and clears the action. Action 6 runs over several frames with
/// `field_2E` as its step: it sends a slot-3 weapon record, turns the
/// `field_34` angle one way or the other each frame while passing the stored
/// player position back as message 0x3E9, and ends four frames after the turn
/// completes, when it clears the action itself. Every path through
/// `func_dryfield_motel_room_1_8017DD3C` except its early return and its kill
/// ends here.
void func_dryfield_motel_room_1_8017D7AC(Task* arg0);

/// Main loop of the room's cutscene task. State 0 arms it once -- a `D_80114C12`
/// of 1 or a live `D_80071075` both mean a cutscene is already up, so the task
/// only steps the script. Otherwise it builds the work block, sends the slot-3
/// weapon record as message 0x3E8 and hands the cutscene's two script blocks to
/// `func_800E8634`. States 0 and 1 then advance the state and step the driver;
/// state 1 does that only while the session is still up, and state 2 only once
/// the session's `at4.loc.view` has reached 2, which is where the task kills itself.
void func_dryfield_motel_room_1_8017DD3C(Task* arg0);

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
