#include "rooms/dryfield_motel_room_1.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "dryfield_motel_room_1_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_dryfield.h"

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

/// Main loop of the room's cutscene task. State 0 arms it once -- a `Gp_StateC08.field_A`
/// of 1 or a live `gDisplayState.pendingMode` both mean a cutscene is already up, so the task
/// only steps the script. Otherwise it builds the work block, sends the slot-3
/// weapon record as message 0x3E8 and hands the cutscene's two script blocks to
/// `func_800E8634`. States 0 and 1 then advance the state and step the driver;
/// state 1 does that only while the session is still up, and state 2 only once
/// the session's `at4.loc.view` has reached 2, which is where the task kills itself.
void func_dryfield_motel_room_1_8017DD3C(Task* arg0);

/// Install the player's weapon animation set on slot 3 (message 0x3E8: the
/// equip-slot id `Player_Status.weapon` plus 1 in the alternate weapon block, plus 0x22
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

/// Main-executable globals with no module header yet: `Player_Status.weapon` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on,
/// `gDisplayState.pendingMode` and `Gp_StateC08.field_A` (the cutscene mode flag) gate the room task's
/// setup, and `Mc_SaveData[0].state.characterId` picks which of the two weapon-id bases that record
/// uses.

/// The cutscene script's two blocks, handed to `func_800E8634` by the room
/// task's state 0.
extern GpEvsCmd D_dryfield_motel_room_1_8017E160[];
extern GpEvsCmd D_dryfield_motel_room_1_8017E340[];

extern GpGridParams   D_dryfield_motel_room_1_8017EABC[1];
extern GpObj3A        D_dryfield_motel_room_1_801811BC[1];
extern GpObj4C        D_dryfield_motel_room_1_80180CFC[8];
extern GpObj4C        D_dryfield_motel_room_1_80180F5C[8];
extern GpRoomCoordSet D_dryfield_motel_room_1_801813D8[1];

extern GpSprtCmd  D_dryfield_motel_room_1_8017EC24[2];
extern GpSprtCmd  D_dryfield_motel_room_1_8017EC34[2];
extern GpSprtCmd  D_dryfield_motel_room_1_8017EC44[2];
extern GpSprtCmd  D_dryfield_motel_room_1_8017EF9C[6];
extern GpSprtCmd  D_dryfield_motel_room_1_8017F670[7];
extern GpSprtCmd  D_dryfield_motel_room_1_8017FB6C[5];
extern GpSprtCmd  D_dryfield_motel_room_1_8017FDD8[6];
extern GpSprtCmd  D_dryfield_motel_room_1_801806B4[10];
extern GpSprtElem D_dryfield_motel_room_1_8017EC54[42];
extern GpSprtElem D_dryfield_motel_room_1_8017EFCC[85];
extern GpSprtElem D_dryfield_motel_room_1_8017F6A8[61];
extern GpSprtElem D_dryfield_motel_room_1_8017FB94[29];
extern GpSprtElem D_dryfield_motel_room_1_8017FE08[111];

GpXformArg D_dryfield_motel_room_1_8017E0D0[2] = {
    { { 500, 0, 1400, 0 }, { 0, 0, 0, 0 } },
    { { 1200, 0, 2450, 0 }, { 0, 0, 0, 0 } },
};

GpXformArg D_dryfield_motel_room_1_8017E100[2] = {
    { { 500, 0, 2000, 0 }, { 0, 0, 0, 0 } },
    { { 1200, 0, 2400, 0 }, { 0, 0, 0, 0 } },
};

GpXformArg D_dryfield_motel_room_1_8017E130[2] = {
    { { 500, 0, 2800, 0 }, { 0, 0, 0, 0 } },
    { { 1000, 0, 3200, 0 }, { 0, 0, 0, 0 } },
};

GpEvsCmd D_dryfield_motel_room_1_8017E160[20] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_motel_room_1_8017DFB0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_motel_room_1_8017DFB0 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_motel_room_1_8017DFB0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_motel_room_1_8017DFB0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_motel_room_1_8017DFB0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_motel_room_1_8017DFB0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_motel_room_1_8017DF08 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_motel_room_1_8017E340[13] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_motel_room_1_8017DFD0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_motel_room_1_8017DF08 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_motel_room_1_8017DF08 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_dryfield_motel_room_1_8017E478 = { 0, 192, func_dryfield_motel_room_1_8017DD3C, { .model = NULL } };

GpRoomObjRec D_dryfield_motel_room_1_8017E484[2] = {
    { D_dryfield_motel_room_1_8017EABC, D_dryfield_motel_room_1_80180CFC, D_dryfield_motel_room_1_80180F5C, D_dryfield_motel_room_1_801811BC },
    { D_dryfield_motel_room_1_8017EABC, D_dryfield_motel_room_1_80180CFC, D_dryfield_motel_room_1_80180F5C, D_dryfield_motel_room_1_801811BC },
};

u8 D_dryfield_motel_room_1_8017E4A4[12] = {
    1,
    9,
    8,
    4,
    5,
    6,
    7,
    3,
    2,
    0,
    0,
    0,
};

u8* D_dryfield_motel_room_1_8017E4B0[2] = {
    D_dryfield_motel_room_1_8017E4A4,
    D_dryfield_motel_room_1_8017E4A4,
};

GpViewCountRec D_dryfield_motel_room_1_8017E4B8[2] = {
    { { .bytes = { 9, 0 } } },
    { { .bytes = { 9, 0 } } },
};

GpRoomCoordRec D_dryfield_motel_room_1_8017E4BC[2] = {
    { D_dryfield_motel_room_1_801813D8, NULL },
    { D_dryfield_motel_room_1_801813D8, NULL },
};

GpWarpRec D_dryfield_motel_room_1_8017E4CC[1] = {
    { { .words = { 3072, 4369, 0, 3388 } }, { 0, 0, 0, 0 }, { .words = { 3072, 4369, 0, 3388 } }, { 0, 0, 0, 0 }, 0x520B0002, 0x520B0001, 0, 2, 0, 487 },
};

SVECTOR D_dryfield_motel_room_1_8017E504[9] = {
    { 2740, 0, -3045, 0 },
    { 227, 0, -4090, 0 },
    { 4096, 0, 0, 0 },
    { -4096, 0, 0, 0 },
    { 0, 4096, 0, 0 },
    { 0, 0, 4096, 0 },
    { 0, 0, -4096, 0 },
    { 0, -4096, 0, 0 },
    { -3731, 0, -1691, 0 },
};

SVECTOR D_dryfield_motel_room_1_8017E54C[85] = {
    { 2180, 0, 3850, 0 },
    { 2180, -1050, 3850, 0 },
    { 2580, -1050, 4210, 0 },
    { 2580, 0, 4210, 0 },
    { 200, 0, 3740, 0 },
    { 200, -1050, 3740, 0 },
    { 3500, -2000, 4210, 0 },
    { 3500, -3000, 4210, 0 },
    { 3500, -3000, 6400, 0 },
    { 3500, -2000, 6400, 0 },
    { 4800, 0, 6400, 0 },
    { 4800, -3000, 6400, 0 },
    { 4800, -3000, 200, 0 },
    { 4800, 0, 200, 0 },
    { 200, -3000, 6400, 0 },
    { 200, -3000, 200, 0 },
    { 200, -3000, 200, 0 },
    { 200, 0, 200, 0 },
    { 200, 0, 6400, 0 },
    { 200, 0, 200, 0 },
    { 3100, 0, 5760, 0 },
    { 3100, -2000, 5760, 0 },
    { 3500, -2000, 5760, 0 },
    { 3500, 0, 5760, 0 },
    { 2300, 0, 1270, 0 },
    { 2300, -1000, 1270, 0 },
    { 2300, -1000, 200, 0 },
    { 2300, 0, 200, 0 },
    { 4800, 0, 1270, 0 },
    { 4800, -1000, 1270, 0 },
    { 2300, 0, 900, 0 },
    { 2300, -1000, 900, 0 },
    { 1350, -1000, 900, 0 },
    { 1350, 0, 900, 0 },
    { 3100, -2000, 6400, 0 },
    { 3100, -3000, 6400, 0 },
    { 3100, -3000, 4700, 0 },
    { 3100, -2000, 4700, 0 },
    { 3450, 0, 200, 0 },
    { 3450, -560, 200, 0 },
    { 3450, -560, 2450, 0 },
    { 3450, 0, 2450, 0 },
    { 2300, -560, 2450, 0 },
    { 2300, 0, 2450, 0 },
    { 2300, -560, 200, 0 },
    { 2300, 0, 200, 0 },
    { 3500, 0, 6400, 0 },
    { 1350, 0, 200, 0 },
    { 1350, -560, 200, 0 },
    { 1350, -560, 2450, 0 },
    { 1350, 0, 2450, 0 },
    { 200, -560, 2450, 0 },
    { 200, 0, 2450, 0 },
    { 3100, 0, 4770, 0 },
    { 3100, -2000, 4770, 0 },
    { 3100, 0, 4700, 0 },
    { 3500, 0, 4770, 0 },
    { 3500, -2000, 4770, 0 },
    { 3500, 0, 4210, 0 },
    { 200, -3000, 4700, 0 },
    { 200, 0, 4700, 0 },
    { 200, 0, 4210, 0 },
    { 200, -3000, 4210, 0 },
    { 3100, 0, 6400, 0 },
    { 4800, 0, 4560, 0 },
    { 3130, 0, 200, 0 },
    { 3130, 0, 4560, 0 },
    { 3130, 0, 6400, 0 },
    { 200, 0, 4560, 0 },
    { 3450, -560, 200, 0 },
    { 2300, -560, 200, 0 },
    { 1350, -560, 200, 0 },
    { 200, -560, 200, 0 },
    { 1140, 0, 6400, 0 },
    { 1140, -2000, 6400, 0 },
    { 1430, -2000, 5760, 0 },
    { 1430, 0, 5760, 0 },
    { 4800, -2000, 5760, 0 },
    { 4800, 0, 5760, 0 },
    { 2300, -1000, 2450, 0 },
    { 3450, -1000, 2450, 0 },
    { 3450, -1000, 200, 0 },
    { 1350, -1000, 2450, 0 },
    { 200, -1000, 2450, 0 },
    { 1350, -1000, 200, 0 },
};

GpGridFace D_dryfield_motel_room_1_8017E7F4[38] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 5, 1, 4, 0 }, 1, 0 },
    { { 7, 8, 6, 9 }, 2, 0 },
    { { 11, 12, 10, 13 }, 3, 0 },
    { { 14, 15, 11, 12 }, 4, 0 },
    { { 12, 16, 13, 17 }, 5, 0 },
    { { 14, 11, 18, 10 }, 6, 0 },
    { { 15, 14, 19, 18 }, 2, 0 },
    { { 21, 22, 20, 23 }, 6, 0 },
    { { 25, 26, 24, 27 }, 3, 0 },
    { { 29, 25, 28, 24 }, 5, 0 },
    { { 31, 32, 30, 33 }, 5, 0 },
    { { 35, 36, 34, 37 }, 3, 0 },
    { { 39, 40, 38, 41 }, 2, 0 },
    { { 40, 42, 41, 43 }, 5, 0 },
    { { 42, 44, 43, 45 }, 3, 0 },
    { { 22, 9, 23, 46 }, 2, 0 },
    { { 48, 49, 47, 50 }, 2, 0 },
    { { 49, 51, 50, 52 }, 5, 0 },
    { { 54, 37, 53, 55 }, 3, 0 },
    { { 57, 54, 56, 53 }, 5, 0 },
    { { 6, 57, 58, 56 }, 2, 0 },
    { { 36, 59, 55, 60 }, 5, 0 },
    { { 62, 7, 61, 58 }, 6, 0 },
    { { 34, 21, 63, 20 }, 3, 0 },
    { { 13, 65, 64, 66 }, 7, 1 },
    { { 67, 10, 66, 64 }, 7, 1 },
    { { 65, 19, 66, 68 }, 7, 1 },
    { { 18, 67, 68, 66 }, 7, 2 },
    { { 40, 69, 42, 70 }, 7, 0 },
    { { 49, 71, 51, 72 }, 7, 0 },
    { { 74, 75, 73, 76 }, 8, 0 },
    { { 75, 77, 76, 78 }, 6, 0 },
    { { 79, 26, 42, 70 }, 3, 3 },
    { { 80, 79, 40, 42 }, 5, 3 },
    { { 81, 80, 69, 40 }, 2, 3 },
    { { 82, 83, 49, 51 }, 5, 3 },
    { { 84, 82, 71, 49 }, 2, 3 },
};

s16 D_dryfield_motel_room_1_8017E9BC[38] = {
    0,
    1,
    2,
    3,
    4,
    5,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    -1,
};

s16 D_dryfield_motel_room_1_8017EA08[35] = {
    0,
    1,
    2,
    3,
    4,
    6,
    7,
    8,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    -1,
};

s16 D_dryfield_motel_room_1_8017EA50[25] = {
    2,
    3,
    4,
    5,
    9,
    10,
    12,
    13,
    14,
    15,
    19,
    20,
    21,
    22,
    23,
    25,
    26,
    27,
    28,
    29,
    32,
    33,
    34,
    35,
    -1,
};

s16 D_dryfield_motel_room_1_8017EA84[19] = {
    2,
    3,
    4,
    6,
    8,
    12,
    16,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    32,
    -1,
};

s16* D_dryfield_motel_room_1_8017EAAC[4] = {
    D_dryfield_motel_room_1_8017E9BC,
    D_dryfield_motel_room_1_8017EA08,
    D_dryfield_motel_room_1_8017EA50,
    D_dryfield_motel_room_1_8017EA84,
};

GpGridParams D_dryfield_motel_room_1_8017EABC[1] = {
    { NULL, D_dryfield_motel_room_1_8017E504, D_dryfield_motel_room_1_8017E54C, D_dryfield_motel_room_1_8017E7F4, D_dryfield_motel_room_1_8017EAAC, -200, -200, 2, 2, 4000, 38 },
};

GpViewRec D_dryfield_motel_room_1_8017EAE0[9] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -2500, 0x2968, -3000 } }, 257 },
    { { { { 1367, 0, -3861 }, { -708, 4026, -250 }, { 3795, 751, 1344 } }, { -200, 1600, -940 } }, 212 },
    { { { { 1557, 0, 3788 }, { 539, 4054, -221 }, { -3749, 583, 1541 } }, { -4800, 1450, -840 } }, 216 },
    { { { { 4033, 0, 714 }, { 53, 4084, -300 }, { -712, 305, 4021 } }, { -4400, 1350, -1640 } }, 269 },
    { { { { -508, 0, -4064 }, { -1596, 3766, 199 }, { 3737, 1609, -467 } }, { -200, 1850, -5690 } }, 246 },
    { { { { -459, 0, 4070 }, { 2448, 3271, 276 }, { -3250, 2464, -367 } }, { -3200, 2600, -5740 } }, 246 },
    { { { { -242, 0, 4088 }, { 396, 4076, 23 }, { -4069, 397, -240 } }, { -3361, 275, -2562 } }, 329 },
    { { { { 1271, 0, 3893 }, { 1803, 3630, -588 }, { -3450, 1897, 1126 } }, { -4561, 2558, -1173 } }, 257 },
    { { { { 1324, 0, -3876 }, { -1721, 3669, -588 }, { 3472, 1819, 1186 } }, { -239, 2334, -1269 } }, 225 },
};

GpSprtCmd D_dryfield_motel_room_1_8017EC24[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_1_8017EC34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_motel_room_1_8017EC44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_motel_room_1_8017EC54[42] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 64, 625, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 48, 625, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -160, 16, 625, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 32, 725, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 712, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 712, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 712, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 712, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 712, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -160, 80, 712, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -120, 80, 712, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, 80, 712, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 40, 712, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, 40, 712, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, 0, 712, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, -40, 712, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, -80, 712, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 712, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, -120, 712, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -80, 712, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -40, 712, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 0, 712, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, 40, 725, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 80, 725, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, 64, 725, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, 24, 725, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -16, 725, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -56, 725, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -96, 725, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, -96, 712, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, -120, 712, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -56, -120, 725, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, -120, 1025, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, -80, 1025, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, -40, 1025, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, 0, 1025, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -32, 40, 1025, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 40, 1056, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, 0, 1056, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, -40, 1056, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, -80, 1056, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, -96, 1056, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_motel_room_1_8017EF9C[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 21, 0, 0, { 0, 0 } },
    { 24, 8, 0, 0, { 2, 0 } },
    { 32, 10, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_motel_room_1_8017EFCC[85] = {
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 725, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 725, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 725, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 725, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -160, 40, 725, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -120, 40, 725, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 0, 700, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -40, 700, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -80, 700, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 700, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, -120, 700, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -80, 700, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 0, 731, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -40, 728, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 0, 731, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, -40, 725, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, -80, 700, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, -120, 700, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 32, 0, 836, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 32, -40, 836, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, -80, 800, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, -120, 800, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -32, -120, 750, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 8, -120, 750, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 537, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -80, 537, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -40, 537, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 0, 537, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 40, 537, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 80, 537, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, 80, 656, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, 40, 656, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 88, 0, 653, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, -40, 637, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, -80, 631, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, -120, 631, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 64, -120, 700, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 64, -80, 700, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, 64, -40, 736, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 64, 0, 750, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 64, 40, 756, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 72, 80, 762, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, 40, 837, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, 0, 831, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, -40, 831, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, -80, 800, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, -120, 800, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, -120, 831, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, -80, 831, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, -40, 840, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, 0, 846, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 40, 846, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 32, 0, 866, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 32, -40, 837, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, 32, -64, 831, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -80, 56, 675, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 40, 687, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 24, 700, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -96, 16, 750, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -80, 24, 700, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -80, 40, 687, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, 40, 687, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 24, 700, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -112, 56, 575, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -120, 40, 600, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -120, 32, 625, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -112, 24, 637, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -96, 72, 437, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -88, 112, 437, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -160, -120, 412, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -160, -80, 412, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -40, 412, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 0, 412, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -160, 40, 412, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, -144, 80, 412, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -136, -120, 412, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -136, -80, 412, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -136, -40, 412, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -136, 0, 412, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -136, 40, 412, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -136, 80, 412, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -112, 0, 437, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, 40, 437, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -112, 80, 437, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, 80, 437, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_motel_room_1_8017F670[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { 24, 31, 0, 0, { 3, 0 } },
    { 55, 8, 0, 0, { 2, 0 } },
    { 63, 4, 0, 0, { 4, 0 } },
    { 67, 18, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_motel_room_1_8017F6A8[61] = {
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 400, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -80, 407, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -40, 431, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 0, 456, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 40, 456, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 80, 456, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -120, 425, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -80, 437, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -40, 450, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, 0, 570, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, 40, 575, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, 80, 575, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, -120, 750, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, -80, 750, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, -40, 800, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, 0, 830, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, 40, 830, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, 80, 830, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, -120, 867, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, -80, 868, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -80, -40, 878, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, 0, 900, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, 40, 906, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 80, 881, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -120, 915, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -80, 921, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -40, 937, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, 0, 962, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, 40, 475, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 40, 80, 500, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 56, 80, 50, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 80, 500, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -120, 475, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -120, 475, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, -120, 475, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 104, -80, 475, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -80, 475, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -80, 475, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -80, 475, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -40, 475, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -40, 475, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -40, 475, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -40, 475, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 0, 500, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 0, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 0, 500, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 0, 500, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 0, 500, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 40, 500, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 40, 500, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 40, 500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, 40, 500, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 80, 50, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 80, 562, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 104, 500, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 104, 500, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 96, 500, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 96, 500, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 88, 525, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 56, 88, 525, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 80, 562, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_motel_room_1_8017FB6C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 28, 0, 0, { 1, 0 } },
    { 28, 25, 0, 0, { 2, 0 } },
    { 53, 8, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_motel_room_1_8017FB94[29] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, -104, 500, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -120, 500, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -96, 500, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -120, 500, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -96, 500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, -120, 500, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, -96, 500, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -88, -120, 500, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, -96, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -64, -120, 500, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -64, -96, 500, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, -96, 500, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -104, 375, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, -64, 375, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -120, -64, 375, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -104, 375, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -104, 375, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -80, -64, 375, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, -104, 375, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -40, -64, 375, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -64, 375, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, -40, 375, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -16, 375, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -64, -96, 330, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -56, 330, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, -16, 330, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 16, 330, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, -56, 812, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -24, 812, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_motel_room_1_8017FDD8[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 3, 0 } },
    { 12, 11, 0, 0, { 0, 0 } },
    { 23, 4, 0, 0, { 2, 0 } },
    { 27, 2, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_motel_room_1_8017FE08[111] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 40, 56, 500, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 64, 487, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 72, 475, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, 112, 425, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -160, 96, 425, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -120, 96, 425, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, 96, 425, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -40, 96, 425, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, 96, 425, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 40, 96, 425, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 80, 462, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -120, 80, 462, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -80, 80, 462, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -40, 80, 462, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 0, 80, 462, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 40, 80, 462, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 64, 475, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -120, 72, 475, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 72, 475, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 72, 475, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 72, 475, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 72, 475, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 64, 487, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 64, 487, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 64, 487, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 40, 64, 487, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 56, 500, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 56, 500, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -16, 48, 537, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 56, 500, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 48, 537, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, -16, 1075, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, -16, 1075, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, -8, 1042, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 0, -8, 1042, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -144, 48, 1000, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 48, 1000, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -96, 48, 1006, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 48, 1012, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -144, 24, 1050, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, 24, 1050, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, 24, 1050, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 24, 1050, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 24, 1025, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -144, 0, 1050, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, 0, 1050, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, 0, 1050, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -72, 0, 1045, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 0, 1045, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, -8, 1045, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, -16, 1075, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, -8, 1050, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, -8, 1062, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, -8, 1062, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 0, 1045, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, -8, 1045, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, -16, 1075, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 0, 1037, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, 0, 1037, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 8, 1037, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -8, 24, 1025, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, 24, 1025, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 24, 1025, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 24, 1025, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -152, 16, 887, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -120, 24, 900, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 48, 875, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -120, 48, 887, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -152, 80, 862, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -120, 80, 875, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 40, 1125, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, 0, 1200, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 96, -120, 1152, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -120, 1375, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -40, 1311, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 0, 1327, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -24, 1450, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 120, -16, 1025, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -16, 950, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -80, 1292, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -80, 1376, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, -40, 1126, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -16, 1206, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -40, 1203, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, -80, 1125, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, -64, 1099, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -80, 1064, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, -80, 960, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -80, 944, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -40, 898, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -40, 931, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 120, -120, 985, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -120, 888, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, -40, 1030, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, -40, 992, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 0, 1050, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, 0, 950, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 0, 950, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, 16, 1050, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -24, 1075, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -16, 1075, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, 16, 1050, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -16, 1150, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, 8, 1125, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, 8, 1125, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, -24, 1150, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, -24, 1150, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 80, -48, 1227, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, -40, 1230, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, -24, 1267, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 48, -24, 1250, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_motel_room_1_801806B4[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 31, 0, 0, { 6, 0 } },
    { 31, 33, 0, 0, { 1, 0 } },
    { 64, 6, 0, 0, { 4, 0 } },
    { 70, 28, 0, 0, { 0, 0 } },
    { 98, 4, 0, 0, { 5, 0 } },
    { 102, 5, 0, 0, { 3, 0 } },
    { 107, 3, 0, 0, { 7, 0 } },
    { 110, 1, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_motel_room_1_80180704[67] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 8, 837, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 16, 800, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 96, 800, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 96, 96, 800, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 96, 800, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 72, 800, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 48, 825, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 24, 800, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 96, 72, 800, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 96, 48, 825, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 24, 800, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 72, 800, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 48, 825, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 24, 800, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 16, 800, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 32, 72, 800, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 0, 72, 800, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -32, 72, 800, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 32, 48, 825, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 32, 24, 800, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 32, 8, 800, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 0, 8, 800, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 0, 24, 800, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 0, 48, 825, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -32, 48, 812, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 56, 825, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -32, 32, 812, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -24, 24, 825, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -16, 16, 831, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 24, 1025, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 0, 1025, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -24, 1037, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 112, 24, 1025, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 0, 1025, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -24, 1050, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 112, -24, 1050, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 112, 0, 1025, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 24, 1025, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 112, 587, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 96, 562, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 104, 96, 575, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 104, 72, 575, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, 80, 562, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 40, 1046, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -120, 963, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -80, 963, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -40, 963, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 0, 963, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 40, 963, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -120, 1046, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -80, 1046, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -40, 1046, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 0, 1046, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, -104, 1112, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -80, 1112, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -40, 1112, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 0, 1112, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 0, 1175, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, -32, 1175, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 0, 787, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 8, 825, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 32, 787, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -144, 32, 825, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 64, 787, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 64, 825, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -88, 104, 237, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -56, 112, 237, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_motel_room_1_80180C40[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 4, 0 } },
    { 0, 29, 0, 0, { 5, 0 } },
    { 29, 3, 0, 0, { 2, 0 } },
    { 32, 6, 0, 0, { 6, 0 } },
    { 38, 5, 0, 0, { 1, 0 } },
    { 43, 16, 0, 0, { 7, 0 } },
    { 59, 6, 0, 0, { 3, 0 } },
    { 65, 2, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_motel_room_1_80180C90[9] = {
    { { .empty = D_dryfield_motel_room_1_8017EC24 }, D_dryfield_motel_room_1_8017EC24, NULL },
    { { .empty = D_dryfield_motel_room_1_8017EC34 }, D_dryfield_motel_room_1_8017EC34, NULL },
    { { .empty = D_dryfield_motel_room_1_8017EC44 }, D_dryfield_motel_room_1_8017EC44, NULL },
    { { .elements = D_dryfield_motel_room_1_8017EC54 }, D_dryfield_motel_room_1_8017EF9C, NULL },
    { { .elements = D_dryfield_motel_room_1_8017EFCC }, D_dryfield_motel_room_1_8017F670, NULL },
    { { .elements = D_dryfield_motel_room_1_8017F6A8 }, D_dryfield_motel_room_1_8017FB6C, NULL },
    { { .elements = D_dryfield_motel_room_1_8017FB94 }, D_dryfield_motel_room_1_8017FDD8, NULL },
    { { .elements = D_dryfield_motel_room_1_8017FE08 }, D_dryfield_motel_room_1_801806B4, NULL },
    { { .elements = D_dryfield_motel_room_1_80180704 }, D_dryfield_motel_room_1_80180C40, NULL },
};

GpObj4C D_dryfield_motel_room_1_80180CFC[8] = {
    { NULL, NULL, NULL, { 2527, -1344, 2927, 0 }, { { 22, 1888, 1715, 0 }, { 22, -1888, 1715, 0 }, { -23, 1888, -1716, 0 }, { -23, -1888, -1716, 0 } }, { -4104, 0, 53, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 2368, -1232, 2880, 0 }, { { -23, 1808, -1716, 0 }, { -23, -1808, -1716, 0 }, { 22, 1808, 1715, 0 }, { 22, -1808, 1715, 0 } }, { 4108, 0, -55, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 4047, -1264, 4559, 0 }, { { -995, 1776, -5, 0 }, { -995, -1776, -5, 0 }, { 995, 1776, 5, 0 }, { 995, -1776, 5, 0 } }, { 19, 0, -4105, 0 }, { 0, 0, 4096, 0 }, 2031, 0, 2, 4, 1, 0 },
    { NULL, NULL, NULL, { 4095, -1248, 4463, 0 }, { { 1011, 1792, -11, 0 }, { 1011, -1792, -11, 0 }, { -1011, 1792, 11, 0 }, { -1011, -1792, 11, 0 } }, { 43, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 2048, 0, 4, 2, 1, 0 },
    { NULL, NULL, NULL, { 3200, -1280, 5632, 0 }, { { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 }, { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 } }, { 4101, 0, 19, 0 }, { 0, 0, 4096, 0 }, 2031, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 3296, -1344, 5824, 0 }, { { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 }, { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 } }, { -4105, 0, -22, 0 }, { 0, 0, 4096, 0 }, 2031, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 1920, -1312, 5408, 0 }, { { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 }, { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 } }, { -4105, 0, -22, 0 }, { 0, 0, 4096, 0 }, 2031, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 1760, -1344, 5408, 0 }, { { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 }, { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 } }, { 4101, 0, 19, 0 }, { 0, 0, 4096, 0 }, 2031, 0, 5, 6, 129, 0 },
};

GpObj4C D_dryfield_motel_room_1_80180F5C[8] = {
    { NULL, NULL, NULL, { 4544, -50, 3488, 0 }, { { -256, 0, -480, 0 }, { 256, 0, -480, 0 }, { -256, 0, 480, 0 }, { 256, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 543, 0, 2, 18, 2, 0 },
    { NULL, NULL, NULL, { 384, -64, 3216, 0 }, { { -224, 0, -368, 0 }, { 224, 0, -368, 0 }, { -224, 0, 368, 0 }, { 224, 0, 368, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 430, 2, 4, 0, 2, 0 },
    { NULL, NULL, NULL, { 2880, -64, 5504, 0 }, { { -160, 0, -1024, 0 }, { 160, 0, -1024, 0 }, { -160, 0, 1024, 0 }, { 160, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1031, 0x8005, 1, 0, 3, 0 },
    { NULL, NULL, NULL, { 2168, -64, 3760, 0 }, { { -568, 0, -336, 0 }, { 232, 0, -336, 0 }, { -568, 0, 336, 0 }, { 904, 0, 336, 0 } }, { 0, 4096, 0, 0 }, { 799, 0, -4017, 0 }, 964, 2, 8, 0, 2, 0 },
    { NULL, NULL, NULL, { 720, -64, 3600, 0 }, { { -432, 0, -256, 0 }, { 432, 0, -256, 0 }, { -432, 0, 256, 0 }, { 432, 0, 256, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 501, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { 1728, -64, 5632, 0 }, { { -368, 0, -256, 0 }, { 368, 0, -256, 0 }, { -368, 0, 256, 0 }, { 368, 0, 256, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 448, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { 2689, -64, 5600, 0 }, { { -432, 0, -256, 0 }, { 432, 0, -256, 0 }, { -432, 0, 256, 0 }, { 432, 0, 256, 0 } }, { 0, 4097, 0, 0 }, { 201, 0, -4091, 0 }, 501, 2, 7, 0, 2, 0 },
    { NULL, NULL, NULL, { 4176, -64, 5600, 0 }, { { -736, 0, -256, 0 }, { 736, 0, -256, 0 }, { -736, 0, 256, 0 }, { 736, 0, 256, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 778, 2, 1, 0, 130, 0 },
};

GpObj3A D_dryfield_motel_room_1_801811BC[1] = {
    { NULL, NULL, { 1760, -1408, 4512, 0 }, { { -1632, -2432, 0, 0 }, { 1632, -2432, 0, 0 }, { -1632, 2432, 0, 0 }, { 1632, 2432, 0, 0 } }, { 0, 0, -4098, 0 }, { 102, 11 }, 129, 0 },
};

GpPointLight D_dryfield_motel_room_1_801811F8[5] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1234, -1685, 5738 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3300, 3300, 3300, { 0, 0 } }, 1222, 4450 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3463, -1645, 1027 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4250, 4250, 4250, { 0, 0 } }, 2300, 4100 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 926, -1645, 83 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3800, 3800, 3800, { 0, 0 } }, 2153, 4701 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4433, -1645, 1308 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2800, 2800, 2800, { 0, 0 } }, 852, 3970 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4671, -1479, 4462 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4000, 4000, 4000, { 0, 0 } }, 1487, 3789 },
};

GpRoomCoordSet D_dryfield_motel_room_1_801813D8[1] = {
    { 0, NULL, 5, D_dryfield_motel_room_1_801811F8, 0, NULL },
};

GpAreaTmdRec D_dryfield_motel_room_1_801813F0[1] = {
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_motel_room_1_801813FC[3] = {
    { 18, 18, 3, 0, { 0, 0 }, D_80155AC4 },
    { 12, 12, 2, 0, { 0, 0 }, D_80168E98 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_motel_room_1_80181420[3] = {
    { 112, 232, 0, 0, { 0, 0 }, D_80137234 },
    { 12, 12, 1, 0, { 0, 0 }, D_80150E98 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_motel_room_1_80181444[3] = {
    { 18, 18, 3, 0, { 0, 0 }, D_80155AC4 },
    { 40, 40, 2, 0, { 0, 0 }, D_8016E500 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_motel_room_1_80181468[3] = {
    { 112, 232, 0, 0, { 0, 0 }, D_80137234 },
    { 12, 12, 1, 0, { 0, 0 }, D_80150E98 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_dryfield_motel_room_1_8018148C[5] = {
    { 112, 0, 0, -3473, 0, 993, 4096, 0, 0, 2, 0 },
    { 112, 0, 0, -3473, 0, 993, 4096, 0, 0, 2, 0 },
    { 12, 0, 0, -3473, 0, 993, 4096, 0, 0, 2, 0 },
    { 12, 0, 0, -3473, 0, 993, 4096, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_dryfield_motel_room_1_801814DC[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017AF94, D_dryfield_motel_room_1_801813F0 },
    { D_map_dryfield_8017AFA4, D_dryfield_motel_room_1_801813FC },
    { D_map_dryfield_8017B004, D_dryfield_motel_room_1_80181420 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_8017B054, D_dryfield_motel_room_1_80181444 },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_motel_room_1_8018148C, D_dryfield_motel_room_1_80181468 },
    { NULL, NULL },
    { NULL, NULL },
};

s32 D_dryfield_motel_room_1_80181544[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

s32 D_dryfield_motel_room_1_80181550[3] = {
    0x10000001,
    0x10000003,
    0x10000001,
};

GpRoomParamRec D_dryfield_motel_room_1_8018155C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_motel_room_1_80181564[1] = {
    { 0, 0, 1, 0, D_dryfield_motel_room_1_80181544 },
};

GpRoomParamRec D_dryfield_motel_room_1_8018156C[1] = {
    { 0, 0, 1, 0, D_dryfield_motel_room_1_80181550 },
};

GpRoomParamRec D_dryfield_motel_room_1_80181574[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec* D_dryfield_motel_room_1_8018157C[8] = {
    D_dryfield_motel_room_1_8018155C,
    D_dryfield_motel_room_1_80181564,
    D_dryfield_motel_room_1_8018156C,
    D_dryfield_motel_room_1_80181574,
    D_dryfield_motel_room_1_8018155C,
    D_dryfield_motel_room_1_8018155C,
    D_dryfield_motel_room_1_8018155C,
    D_dryfield_motel_room_1_8018155C,
};

Task* D_dryfield_motel_room_1_8018159C = NULL;

static void func_dryfield_motel_room_1_8017D7AC(Task* arg0);
static void func_dryfield_motel_room_1_8017DC2C(Task* arg0);

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
static void func_dryfield_motel_room_1_8017D7AC(Task* arg0)
{
    Dmr1Work*     work = (Dmr1Work*)arg0->work;
    PlayerStatus* cfg;
    s32           anim;
    s32           weaponId;
    Dmr1DriverBuf buf;
    GpAnimArg*    rec;

    switch (work->field_2C) {
        case 1:
            buf.msg.from.loc.stage = gGameSession->at4.loc.stage;
            buf.msg.from.loc.area  = gGameSession->at4.loc.area;
            buf.msg.command        = 1;
            Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &buf.msg, 0x7DB);
            Gp_DispatchMsgPtr(work->field_4, 0x7D4, &D_dryfield_motel_room_1_8017E0D0[0], 0);
            Gp_DispatchMsgPtr(work->field_8, 0x7D4, &D_dryfield_motel_room_1_8017E0D0[1], 0);
            break;
        case 2:
            buf.msg.from.loc.stage = gGameSession->at4.loc.stage;
            buf.msg.from.loc.area  = gGameSession->at4.loc.area;
            buf.msg.command        = 2;
            Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &buf.msg, 0x7DB);
            Gp_DispatchMsg(work->field_0, 0x3F3, 1, 0);
            Gp_DispatchMsgPtr(work->field_4, 0x7D4, &D_dryfield_motel_room_1_8017E100[0], 0);
            Gp_DispatchMsgPtr(work->field_8, 0x7D4, &D_dryfield_motel_room_1_8017E100[1], 0);
            break;
        case 3:
        case 5:
            SndEvt_EnqueueType6(0x400C0005, 0, 0);
            break;
        case 4:
            SndEvt_EnqueueType6(0x400C0002, 0, 0);
            break;
        case 6:
            switch (work->field_2E) {
                case 0:
                    cfg            = &Player_Status;
                    work->field_14 = cfg->coordMtx->t[0];
                    work->field_18 = cfg->coordMtx->t[1];
                    work->field_1C = cfg->coordMtx->t[2];
                    work->field_24 = 0;
                    work->field_26 = 0;
                    work->field_28 = 0;
                    work->field_34 =
                        (((GameActor*)(work->field_0)->work)->field_52 + 0xC00) % 0x1000;
                    if (work->field_34 > 0x800) {
                        s32 weapon;

                        rec    = &buf.shifted.rec;
                        weapon = cfg->weapon;
                        if (Mc_SaveData[0].state.characterId == 1) {
                            anim = weapon + 1;
                        } else {
                            anim = weapon + 0x22;
                        }
                        buf.shifted.rec.animBlock.index = anim;
                        rec->field_4                    = 5;
                        rec->field_8                    = 1;
                        rec->field_C                    = 5;
                        buf.shifted.rec.field_10        = 0;
                        Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &buf.shifted.rec, 0);
                        Gp_DispatchMsg(work->field_0, 0x3FD, 0x30, 0);
                        work->field_2E += 1;
                    } else {
                        weaponId = cfg->weapon;
                        if (Mc_SaveData[0].state.characterId == 1) {
                            anim = weaponId + 1;
                        } else {
                            anim = weaponId + 0x22;
                        }
                        buf.rec.animBlock.index = anim;
                        buf.rec.field_4         = 6;
                        buf.rec.field_8         = 1;
                        buf.rec.field_C         = 5;
                        buf.rec.field_10        = 0;
                        Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &buf.rec, 0);
                        Gp_DispatchMsg(work->field_0, 0x3FD, 0x30, 0);
                        work->field_2E += 2;
                    }
                    return;
                case 1:
                    work->field_34 += 0x96;
                    work->field_26  = work->field_34 + 0x400;
                    if (work->field_34 > 0x1000) {
                        anim = Player_Status.weapon;
                        if (Mc_SaveData[0].state.characterId == 1) {
                            anim += 1;
                        } else {
                            anim += 0x22;
                        }
                        buf.rec.animBlock.index = anim;
                        buf.rec.field_4         = 1;
                        buf.rec.field_8         = 1;
                        buf.rec.field_C         = 3;
                        buf.rec.field_10        = 0;
                        Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &buf.rec, 0);
                        work->field_30 = 0;
                        work->field_2E = 3;
                        return;
                    }
                    Gp_DispatchMsgPtr(work->field_0, 0x3E9, &work->field_14, 0);
                    return;
                case 2:
                    work->field_34 -= 0x96;
                    work->field_26  = work->field_34 + 0x400;
                    if (work->field_34 < 0) {
                        anim = Player_Status.weapon;
                        if (Mc_SaveData[0].state.characterId == 1) {
                            anim += 1;
                        } else {
                            anim += 0x22;
                        }
                        buf.rec.animBlock.index = anim;
                        buf.rec.field_4         = 1;
                        buf.rec.field_8         = 1;
                        buf.rec.field_C         = 3;
                        buf.rec.field_10        = 0;
                        Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &buf.rec, 0);
                        work->field_30 = 0;
                        work->field_2E = 3;
                        return;
                    }
                    Gp_DispatchMsgPtr(work->field_0, 0x3E9, &work->field_14, 0);
                    return;
                case 3:
                    work->field_30 += 1;
                    if (work->field_30 >= 4) {
                        anim = Player_Status.weapon;
                        if (Mc_SaveData[0].state.characterId == 1) {
                            anim += 1;
                        } else {
                            anim += 0x22;
                        }
                        buf.rec.animBlock.index = anim;
                        buf.rec.field_4         = 9;
                        buf.rec.field_8         = 1;
                        buf.rec.field_C         = 10;
                        buf.rec.field_10        = 0;
                        Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &buf.rec, 0);
                        work->field_2C = 0;
                    }
                    return;
                default:
                    return;
            }
            return;
        case 0:
        default:
            break;
    }
    work->field_2C = 0;
}

/// Room entry point: allocate the `Dmr1Work` the room task hangs off
/// `Task::work` (killing the task if the allocation fails), zero it, park the
/// slot-3 task in `field_0` and the room task itself in
/// `D_dryfield_motel_room_1_8018159C`, then resolve the four placed objects
/// `field_4` .. `field_10` from the session id: the base id, then the id with
/// the 0x1000 / 0x2000 / 0x3000 index of `Gp_FindWorkById`'s search key.
static void func_dryfield_motel_room_1_8017DC2C(Task* arg0)
{
    Dmr1Work* work;
    s32       id;

    work       = (Dmr1Work*)Mem_Malloc(0x38, 0);
    arg0->work = (TaskIdMap*)work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    Mem_Set(work, 0, 0x38);
    work->field_0                    = gameGetPtrSlot(3);
    D_dryfield_motel_room_1_8018159C = arg0;
    id                               = gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8);
    work->field_4                    = Gp_FindWorkById(id)->field_0;
    id                               = ((gGameSession->at4.loc.stage << 8) | 0x1000) | gGameSession->at4.loc.area;
    work->field_8                    = Gp_FindWorkById(id)->field_0;
    id                               = ((gGameSession->at4.loc.stage << 8) | 0x2000) | gGameSession->at4.loc.area;
    work->field_C                    = Gp_FindWorkById(id)->field_0;
    id                               = ((gGameSession->at4.loc.stage << 8) | 0x3000) | gGameSession->at4.loc.area;
    work->field_10                   = Gp_FindWorkById(id)->field_0;
}
void func_dryfield_motel_room_1_8017DD3C(Task* arg0)
{
    Dmr1MsgBuf buf;
    s32        weaponId;
    s32        anim;

    switch (arg0->state) {
        case 0:
            if ((Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == 0)) {
                func_dryfield_motel_room_1_8017DC2C(arg0);
                weaponId                = Player_Status.weapon;
                anim                    = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                buf.rec.animBlock.index = anim;
                buf.rec.field_4         = 1;
                buf.rec.field_8         = 1;
                buf.rec.field_C         = 5;
                buf.rec.field_10        = 0;
                Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &buf.rec, 0);
                func_800E8634(D_dryfield_motel_room_1_8017E160, 0,
                              D_dryfield_motel_room_1_8017E340);
                arg0->state = arg0->state + 1;
                break;
            }
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                buf.msg.from.loc.stage = gGameSession->at4.loc.stage;
                buf.msg.from.loc.area  = gGameSession->at4.loc.area;
                buf.msg.command        = 4;
                Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &buf.msg, 0x7DB);
                arg0->state = arg0->state + 1;
                break;
            }
            break;
        case 2:
            if (gGameSession->at4.loc.view == arg0->state) {
                buf.msg.from.loc.stage = gGameSession->at4.loc.stage;
                buf.msg.from.loc.area  = gGameSession->at4.loc.area;
                buf.msg.command        = 3;
                Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &buf.msg, 0x7DB);
                taskKill(arg0);
                return;
            }
            break;
    }
    func_dryfield_motel_room_1_8017D7AC(arg0);
}

void func_dryfield_motel_room_1_8017DF08(void)
{
    Dmr1Work* work = (Dmr1Work*)D_dryfield_motel_room_1_8018159C->work;
    GpCmdArg  msg;

    Gp_ArmStateF0(1);
    msg.from.loc.stage = gGameSession->at4.loc.stage;
    msg.from.loc.area  = gGameSession->at4.loc.area;
    msg.command        = 3;
    Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &msg, 0x7DB);
    Gp_DispatchMsgPtr(work->field_C, 0x7D4, &D_dryfield_motel_room_1_8017E130[0], 0);
    Gp_DispatchMsgPtr(work->field_10, 0x7D4, &D_dryfield_motel_room_1_8017E130[1], 0);
}

void func_dryfield_motel_room_1_8017DFB0(s16 arg0)
{
    Dmr1Work* work = (Dmr1Work*)D_dryfield_motel_room_1_8018159C->work;

    work->field_2C = arg0;
    work->field_2E = 0;
}

void func_dryfield_motel_room_1_8017DFD0(void)
{
    Dmr1Work*     work;
    GpAnimArg     msg;
    PlayerStatus* cfg;
    s32           weaponId;
    s32           anim;

    work                = (Dmr1Work*)D_dryfield_motel_room_1_8018159C->work;
    weaponId            = Player_Status.weapon;
    anim                = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.animBlock.index = anim;
    msg.field_4         = 9;
    msg.field_8         = 0;
    msg.field_C         = 0;
    msg.field_10        = 0;
    Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &msg, 0);
    cfg            = &Player_Status;
    work->field_14 = cfg->coordMtx->t[0];
    work->field_18 = cfg->coordMtx->t[1];
    work->field_1C = cfg->coordMtx->t[2];
    work->field_24 = 0;
    work->field_26 = 0x500;
    work->field_28 = 0;
    Gp_DispatchMsgPtr(work->field_0, 0x3E9, &work->field_14, 0);
}
void func_dryfield_motel_room_1_8017E0A0(Task* unused)
{
}
