#ifndef ROOMS_SHELTER_B2_MAIN_CORRIDOR_H
#define ROOMS_SHELTER_B2_MAIN_CORRIDOR_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// The twelve bytes the room's message handler stages before spawning a task
/// from `D_shelter_b2_main_corridor_80182C44`. `field_1` takes either the
/// triggering message's `field_3` or a fixed value, chosen together with
/// `field_0`.
///
/// The layout matches the event blocks other rooms stage the same way
/// (`ShelterB4WaterSupplyEventDesc`, `NeoArkObservatoryEventDesc`); whether they
/// are one type is open. `pad_6` is never touched here.
typedef struct ShelterB2MainCorridorEventDesc {
    /* 0x0 */ u8   field_0;
    /* 0x1 */ u8   field_1;
    /* 0x2 */ u8   field_2;
    /* 0x3 */ u8   field_3;
    /* 0x4 */ s16  field_4;
    /* 0x6 */ byte pad_6[0x2];
    /* 0x8 */ s32  field_8;
} ShelterB2MainCorridorEventDesc;
STATIC_ASSERT_SIZEOF(ShelterB2MainCorridorEventDesc, 0xC);

/// Descriptor of the tasks the room's message handler spawns.
extern TaskDesc D_shelter_b2_main_corridor_80182C44;

/// Passed by address to `func_800E8614` when the room's one-shot flag event
/// fires; its contents are not read here.
extern s32 D_shelter_b2_main_corridor_80182CA8;

/// The staged event block, read by the task spawned from the descriptor above.
extern ShelterB2MainCorridorEventDesc D_shelter_b2_main_corridor_80189684;

/// A second copy of the staged event block, taken whole once the block has been
/// passed through `func_shelter_b2_main_corridor_8017E0FC`.
extern ShelterB2MainCorridorEventDesc D_shelter_b2_main_corridor_80189664;

/// Descriptor of the task spawned after the staged block has been copied.
extern TaskDesc D_shelter_b2_main_corridor_801828E4;

/// Parameters of a room exit, staged by the room's message handler for the task
/// spawned from `D_shelter_b2_main_corridor_80182C08`, which runs `capCmd`,
/// plays `sndId` and then moves the player. `flag` names a game-flag nibble that
/// is set once the exit has been taken; once set, the handler no longer claims
/// the message.
typedef struct ShelterB2MainCorridorExit {
    s32 capCmd;
    s32 sndId;
    s16 flag;
    u8  field_A;
} ShelterB2MainCorridorExit;
STATIC_ASSERT_SIZEOF(ShelterB2MainCorridorExit, 0xC);

/// Descriptor of the task that carries out a staged exit.
extern TaskDesc D_shelter_b2_main_corridor_80182C08;

/// The outgoing message of the exit being taken; the exit task copies its
/// destination into the save location.
extern RoomEventMsg D_shelter_b2_main_corridor_80189654;

/// Cleared whenever the handler considers an exit, set once the exit task has
/// been spawned. Nothing else in the room reads it.
extern u8 D_shelter_b2_main_corridor_8018965C;

/// The exit being taken, read by the exit task.
extern ShelterB2MainCorridorExit D_shelter_b2_main_corridor_80189674;

s32  func_shelter_b2_main_corridor_8017E0FC(RoomEventMsg* in, RoomEventMsg* out);
void func_shelter_b2_main_corridor_8017E264(RoomEventMsg* msg);

#endif
