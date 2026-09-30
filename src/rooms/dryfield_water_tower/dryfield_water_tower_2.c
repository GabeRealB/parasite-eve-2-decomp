#include "rooms/dryfield_water_tower.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "dryfield_water_tower_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_dryfield.h"

#include "overlay.h"

extern ActorTransform D_dryfield_water_tower_80181A70[3];

extern ActorTransform D_dryfield_water_tower_80181A40[2];

extern GpObj4C D_dryfield_water_tower_80186A84[24];

/// Work block of the water tower's script task, allocated as 0x18 zeroed bytes
/// by `func_dryfield_water_tower_8017FD64` and hung off `Task::work` (0x1C) --
/// that slot is *not* a `TaskIdMap` here. Reach it with
/// `(DwtwWork*)task->work`.
///
/// The first three fields are the tasks the room's script dispatches its
/// messages to: `field_0` is the slot-3 game pointer (`gameGetPtrSlot(3)`),
/// and `field_4` / `field_8` are `Gp_FindWorkById(...)->field_0` for two ids
/// built from the session's `at4.loc.area` / `at4.loc.stage` bytes (the second id has
/// 0x1000 OR'd in). `func_dryfield_water_tower_80180220` sends the 0x7D4 pair
/// to `field_8` / `field_4` and the 0x3F3 / 0x3E9 messages to `field_0`.
///
/// `field_C` / `field_E` are written together as a pair -- `field_E` always
/// cleared -- by `func_dryfield_water_tower_80180174`, a body the breezeway
/// room carries as `func_dryfield_breezeway_8017E370`. `field_C` is the
/// per-frame command latch `func_dryfield_water_tower_8017FBE8` switches on and
/// every one of its paths clears: the states it dispatches are the room's
/// animations and the two prop placements, and its idle states are 0 and 3.
/// `field_E`'s meaning is not yet known. `field_14` is a 0/1 latch that lets
/// `func_dryfield_water_tower_80180194` dispatch its one-shot message once.
typedef struct DwtwWork {
    /* 0x00 */ Task* field_0; // gameGetPtrSlot(3), Gp_DispatchMsg target
    /* 0x04 */ Task* field_4; // Gp_FindWorkById(...)->field_0
    /* 0x08 */ Task* field_8; // Gp_FindWorkById(...)->field_0
    /* 0x0C */ s16   field_C;
    /* 0x0E */ s16   field_E;
    /* 0x10 */ byte  pad_10[0x4];
    /* 0x14 */ s16   field_14;
    /* 0x16 */ byte  pad_16[0x2];
} DwtwWork;
STATIC_ASSERT_SIZEOF(DwtwWork, 0x18);

/// One step of the room's rotation schedule, the table
/// `func_dryfield_water_tower_8017FB4C` walks: `field_0` is the step's
/// threshold and `field_2` its duration. Four of them sit at 0x8018767C --
/// `{0, 25}`, `{1, 30}`, `{2, 35}` and the `{0xFFFF, 40}` terminator, so the
/// thresholds ascend and the last entry is the cap the walk can never pass --
/// and the duration is read out as `field_2 * 30` with the low bit cleared.
///
/// `func_dryfield_water_tower_8017EB7C` counts the step up in
/// `DryfieldWaterTowerState::field_72` and walks the same table inline,
/// handing the duration to the accumulator `D_dryfield_water_tower_801876AA`
/// at 0x801876AA, the halfword its 0x801876A8 frame counter is compared
/// against.
typedef struct DwtwStep {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} DwtwStep;
STATIC_ASSERT_SIZEOF(DwtwStep, 0x4);

/// One entry of the room's per-view volume table: while the cap is being
/// rotated, `func_dryfield_water_tower_8017EB7C` looks up the view the cap
/// script last recorded and plays its running sound at `field_2` percent of
/// full volume (0x7F). A `field_0` of 0xFFFF ends the table, and a view with no
/// entry plays at full volume.
typedef struct DwtwViewVolume {
    /* 0x0 */ u16 field_0; // view index
    /* 0x2 */ u16 field_2; // volume, percent
} DwtwViewVolume;
STATIC_ASSERT_SIZEOF(DwtwViewVolume, 0x4);

/// Scratch state of the room's cap script, stored at `Task::work`: the
/// 0x7C-byte block `func_dryfield_water_tower_8017F128` allocates for its own
/// task before it runs. Every task the room spawns off `D_..._80182384`
/// allocates the same block, so the prop tasks reached through `field_44` /
/// `field_48` carry one too.
///
/// It opens with the light and colour matrices of the task's own model: setup
/// points the model's `lightMtx` and `colorMtx` at them.
///
/// `field_40` is the slot-3 game pointer (`gameGetPtrSlot(3)`), the task the
/// 0x3E9 player-placement messages go to. `field_44` / `field_48` are the two
/// prop tasks `func_dryfield_water_tower_8017F128` spawns as types 1 and 2 of
/// `D_..._80182384` -- the 0x7D4 (`func_dryfield_water_tower_8017F77C`)
/// targets -- and `field_4C` is
/// the task it spawns off a second table, `D_..._8018277C`. `field_50` is the
/// script-18 task `func_dryfield_water_tower_8017E93C` spawns when the cap
/// script reaches one of its last three states, one of the room's three
/// `Gp_SpawnScript18` pairs.
///
/// The tail is halfword slots -- the pairs are timers the instructions above
/// the state switch count down -- and the ones the decomp has named so far are
/// `field_58` / `field_5A` / `field_60`, the three
/// `func_dryfield_water_tower_8017F808` clears on message 0x7DB, and
/// `field_5C` / `field_5E`, which `func_dryfield_water_tower_8017F8E8` writes
/// together -- `field_5C` is the cap script's command index, the value
/// `func_dryfield_water_tower_8017E93C` switches on and every one of its states
/// resets to zero -- and `field_6A`, the cap props' displacement accumulator: the prop
/// task `func_dryfield_water_tower_8017DE30` advances it by 4 a frame and adds
/// the result to the cap coordinate's Y, so the cap accelerates downwards.
/// `field_64` holds the value of nibble 0x55 the cap script read while placing
/// its props, `field_68` the view index the script later restores into the
/// saved location, and `field_74` the session's view, which the script records every
/// frame it runs. `field_6C` / `field_6E` are 0/1 latches set by
/// `func_dryfield_water_tower_8017FBC8` / `8017FBD8`; `field_70` is a third,
/// set by script opcode `func_dryfield_water_tower_8017FA5C` and read back by
/// the prop task `func_dryfield_water_tower_8017E1DC`. `field_72` is the
/// rotation's step, advanced by `func_dryfield_water_tower_8017EB7C` and the
/// index `func_dryfield_water_tower_8017FB4C` reads its `DwtwStep` with.
/// `field_76` is also a
/// 0/1 latch, set once by `func_dryfield_water_tower_8017F82C`, the 0x0D
/// handler both of the room's script tables carry. `field_78` is a fourth:
/// `func_dryfield_water_tower_8017E93C` sets it as it spawns
/// `Gp_SpawnScript18(0x80187628, 0x8018763C)`, `8017EB7C` clears it, and
/// `func_dryfield_water_tower_8017F908` reads it to decide whether its event
/// 0x5214000C is due.
typedef struct DryfieldWaterTowerState {
    MATRIX           lightMtx;
    MATRIX           colorMtx;
    /* 0x40 */ Task* field_40; // gameGetPtrSlot(3)
    /* 0x44 */ Task* field_44;
    /* 0x48 */ Task* field_48;
    /* 0x4C */ Task* field_4C;
    /* 0x50 */ Task* field_50;
    /* 0x54 */ u8    pad_54[0x4];
    /* 0x58 */ u16   field_58;
    /* 0x5A */ s16   field_5A;
    /* 0x5C */ u16   field_5C;
    /* 0x5E */ s16   field_5E;
    /* 0x60 */ s16   field_60;
    /* 0x62 */ u8    pad_62[0x2];
    /* 0x64 */ u16   field_64;
    /* 0x66 */ u16   field_66;
    /* 0x68 */ s16   field_68;
    /* 0x6A */ u16   field_6A;
    /* 0x6C */ u16   field_6C;
    /* 0x6E */ u16   field_6E;
    /* 0x70 */ u16   field_70;
    /* 0x72 */ u16   field_72;
    /* 0x74 */ s16   field_74;
    /* 0x76 */ u16   field_76;
    /* 0x78 */ u16   field_78;
    /* 0x7A */ u8    pad_7A[0x2];
} DryfieldWaterTowerState;
STATIC_ASSERT_SIZEOF(DryfieldWaterTowerState, 0x7C);

/// Main-executable globals with no module header yet: `Player_Status.weapon` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on and
/// `Mc_SaveData[0].state.characterId` picks which of the two weapon-id bases that record uses; the
/// alternate block is indexed by `Player_Status.weapon` plus 1 against the base block's
/// plus 0x22.

/// Placement sent to the cap and prop tasks as message 0x7D4. The cap's
/// arrival test reads its height and Z coordinate directly from this record.
extern ActorTransform D_dryfield_water_tower_80181AB8;

/// The three effect-definition tables the cap-arrival test and
/// `func_dryfield_water_tower_8017FA5C` install into the room's live copies --
/// 0x10, 0x18 and 0x40 bytes. The same three pairs, at the same sizes, are
/// copied by `func_dryfield_water_tower_8017F128` on its state-7 path, so each
/// is one variant of a table the room selects between rather than the only
/// content. The destinations are also reached with an offset from a lower base
/// (`0x801828CC + 0x10` is `801828DC`), i.e. the live table is a region the
/// room walks. The live normal, vertex, and face arrays retain those element ranges.
extern SVECTOR    D_dryfield_water_tower_80181B10[2];
extern GpGridFace D_dryfield_water_tower_80181BA0[2];
extern SVECTOR    D_dryfield_water_tower_80181B20[8];

/// The room's two cap placements with message 0x7D4, the pair the cap props
/// publish to themselves through `func_dryfield_water_tower_8017F77C`: `[0]` is the raised position
/// (Y = -0x1F40, the one a task spawned with a non-zero `spawnArg1` publishes)
/// and `[1]` the lowered one (Y = -0xFA0) the lowering prop stops at. They are
/// the two 0x18-byte records above them in the same run --
/// `func_dryfield_water_tower_8017F908` sends `-0x20` and `8017F9AC` `-0x38` --
/// so the split names only the first.

/// The first record of that same placement run, at 0x80181A40, and the one
/// `func_dryfield_water_tower_8017E5B0` drives the cap from: its `pos.vz` is the
/// Z the cap has to sink past, its `pos.vy` the Y it is snapped to while
/// lowering and its `pos.vx` the X state 1 pulls it to.
///
/// The length is one, not two, because the run continues with the record below
/// and the original reaches that record both ways: as `[1]` -- the sibling
/// `func_dryfield_water_tower_8017E428` reads its `pos.vy` and `pos.vz` with the
/// offsets still measured from this head, which is why those loads carry the
/// run's own address and a displacement -- and by naming its own symbol where
/// the address is the record's. Declaring two elements would fold `[1]` into
/// the address constant and lose the head-based form.
///
/// `func_dryfield_water_tower_8017F9AC` also sends this record to the prop task
/// at `DryfieldWaterTowerState::field_44` with message 0x7D4.

/// The run's second record, at 0x80181A58: the lowered position
/// `func_dryfield_water_tower_8017E428` sinks the cap to (`pos.vy`), tests the
/// cap's Z against (`pos.vz`) and pulls the cap's X to (`pos.vx`), and the
/// 0x7D4 placement that same function publishes to itself once its state 1
/// counter runs out. `func_dryfield_water_tower_8017F908` sends it to the prop
/// task at `DryfieldWaterTowerState::field_44` with the same message.

/// The effect offsets `func_dryfield_water_tower_8017E5B0` spawns 0x60054
/// with, at 0x80181C60: twelve halfwords, indexed by the 0..9 `killCountdown`
/// counter the same block wraps, so only the first ten -- 0x190, 0x3E8, 0xFE70,
/// 0xFF38, 0, 0xFDA8, 0xC8, 0x320, 0xFC18 and 0xFCE0 -- are ever read.
extern u16 D_dryfield_water_tower_80181C60[];

/// The room's 4A object -- the list node `Gp_LinkObj4A` chains into
/// `Gp_Obj4ALists` and `Gp_UnlinkObj4A` takes out again. Command 3 of
/// `func_dryfield_water_tower_8017E93C` raises its `field_4A` bit 0x40, the
/// same bit `func_acropolis_fountain_8017DA1C` raises on the fountain's node.

/// The two player placements the cap script's commands 3, 4 and 7 dispatch as
/// the payload of message 0x3E9 (their handler is the slot-3 game task at
/// `field_40`): `80181AD0` from commands 4 and 7, and `80181AE8` from command
/// 3, which sends `80181AD0` -- the 0x18-byte record one step below it in the
/// same array -- with 0x3F2 straight after. `func_dryfield_water_tower_8017F9AC`
/// and `8017FA5C` send `80181AD0` with the same message.
extern ActorTransform D_dryfield_water_tower_80181AD0[2];

/// The three `Gp_SpawnScript18` pairs the cap script's last three commands
/// spawn into `field_50`, and the sound each one queues: `0x52140006` with the
/// extra `0x5214000C` for command 8, whose script `func_dryfield_water_tower_8017EB7C`
/// and `func_dryfield_water_tower_8017F908` wait on through `field_78`.
extern GpScriptCmd D_dryfield_water_tower_80187628[5];
extern GpScriptRec D_dryfield_water_tower_8018763C[4];
extern GpScriptCmd D_dryfield_water_tower_8018764C[5];
extern GpScriptRec D_dryfield_water_tower_80187660[4];
extern GpScriptCmd D_dryfield_water_tower_80187670[2];
extern GpScriptRec D_dryfield_water_tower_80187678;

/// Main-executable byte at 0x80114C11, read signed (`lb`), with no module
/// header yet: the raise prop `func_dryfield_water_tower_8017E1DC` runs its
/// state machine only while it is zero, so it is the room's "leave the cap
/// alone" gate -- the cap stops moving the moment it goes non-zero.

/// The room's script table, the `Task::msgTable` block `Gp_DispatchMsg`
/// reads: the raise prop `func_dryfield_water_tower_8017E1DC` hangs it off its
/// own task in state 0, the same slot the cap script publishes a table into.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*);
        void (*call1)(Task*, s32, ActorCommand* request);
        void (*call2)(Task*, s32, ActorTransform*);
    } handler;
} DryfieldWaterTower2MessageEntry;
STATIC_ASSERT_SIZEOF(DryfieldWaterTower2MessageEntry, 8);

extern DryfieldWaterTower2MessageEntry D_dryfield_water_tower_80181B00[2];

/// Main-executable gates the cap script checks, with no module header yet:
/// the script only runs while `Player_Status.hp` is non-zero, and its state 8 holds
/// back on `Gp_StateC08.field_A` == 1 or a non-zero `gDisplayState.pendingMode`.

/// The raised-cap sources the cap script restores the room's script-table
/// blocks from, and the lowered-cap ones it uses when the cap is already down.
/// The destinations are runs of two blocks each (0x10, 0x18 and 0x40 bytes),
/// reached by their first symbol plus the block size or by the second symbol
/// minus it.
extern SVECTOR    D_dryfield_water_tower_80181B60[8];
extern SVECTOR    D_dryfield_water_tower_80181BB8[2];
extern SVECTOR    D_dryfield_water_tower_80181BC8[8];
extern SVECTOR    D_dryfield_water_tower_80181C08[8];
extern GpGridFace D_dryfield_water_tower_80181C48[2];

/// The pair of blocks the cap script's state 8 hands to `func_800E8634`.
extern GpEvsCmd D_dryfield_water_tower_801820B0[];
extern GpEvsCmd D_dryfield_water_tower_80182248[];

/// The two `func_800E8634` pairs the rotation step hands over: `80181C78` /
/// `80181DC8` when it starts a rotation from view 7, `80181E88` / `80181FF0`
/// when it ends one.
extern GpEvsCmd D_dryfield_water_tower_80181C78[];
extern GpEvsCmd D_dryfield_water_tower_80181DC8[];
extern GpEvsCmd D_dryfield_water_tower_80181E88[];
extern GpEvsCmd D_dryfield_water_tower_80181FF0[];

/// The room's rotation schedule (see `DwtwStep`) and the per-view volume table
/// its running sound is scaled by.
extern DwtwStep       D_dryfield_water_tower_8018767C[];
extern DwtwViewVolume D_dryfield_water_tower_80182350[];

/// The cap script's message table, published into its own `Task::msgTable`.
extern DryfieldWaterTower2MessageEntry D_dryfield_water_tower_80182374[2];

/// The room script's task table: entry 0 is the room task
/// `func_dryfield_water_tower_8017FD64` itself, which the cap script spawns in
/// its state 3, entry 1 the fade-out task `func_dryfield_water_tower_80180038`
/// and entry 2 the fade-in task `func_dryfield_water_tower_8017FF5C`, which the
/// room task's state 1 starts with the fade rate 8.
extern TaskDesc D_dryfield_water_tower_8018277C[];

/// The room's run of 4A objects; element 14 is `D_dryfield_water_tower_80186A84[20]`.

/// The `ActorTransform` run the room's 0x7D4 messages step the props through:
/// the 0x18-byte records from 0x801823A8 up to 0x80182408. `D_..._801823C0`,
/// the second of them, is case 1's pair -- `[0]` to `field_4` and `[3]`
/// (0x80182408) to `field_8`; `D_..._801823F0`, the run's element 2, is case
/// 2's, and the same record the script opcode
/// `func_dryfield_water_tower_80180220` sends as element 1 of the pair it
/// declares `D_..._801823D8[]`; and `D_..._801823A8`, the first, is the player
/// move both that opcode and case 4 send with 0x3E9.
extern ActorTransform D_dryfield_water_tower_801823A8;

/// The `AnimationPlayRequest` (0x14-byte) run the 0x7D3 animation messages send: `field_4`
/// carries the animation index -- 0x0D / 0x0E / 0x0F for the three records --
/// and every other field, `field_8` included, is zero. Element 1 and element 2
/// are also named by address, and the code below uses both spellings: element 1
/// is `D_..._80182420[1]` in case 1 and `D_..._80182434` in case 6.

/// The pair of placements `func_dryfield_water_tower_80180220` sends with
/// message 0x7D4, one to each of `field_8` and `field_4`; the second is the
/// element at 0x18, so the run is declared as an array. Both are payloads of
/// `func_dryfield_water_tower_8017F77C`, the handler the room's script table
/// pairs with 0x7D4.

/// The pair of cutscene blocks `func_800E8634` hands to `Task_Spawn` (bank 9,
/// type 7): the one the running scene starts and the one it parks in
/// `D_801156D0` for the task that follows it.
extern GpEvsCmd D_dryfield_water_tower_80182464[];
extern GpEvsCmd D_dryfield_water_tower_80182674[];

/// The per-view `roomEffectMode` table `func_dryfield_water_tower_80180348`
/// reads, indexed by the 1-based view.
extern u16 D_dryfield_water_tower_801827A0[];

static u16 func_dryfield_water_tower_8017EB7C(Task* arg0);
static s32 func_dryfield_water_tower_8017DFAC(Task* arg0);

extern GpRoomCoordSet D_dryfield_water_tower_801874E4[1];

extern GpGridParams D_dryfield_water_tower_801835C4[1];
extern GpObj4C      D_dryfield_water_tower_8018665C[14];
void                func_dryfield_water_tower_8017FD64(Task*);
void                func_dryfield_water_tower_8017FF5C(Task*);
void                func_dryfield_water_tower_80180038(Task*);
void                func_dryfield_water_tower_80180114(void);
void                func_dryfield_water_tower_80180134(void);
void                func_dryfield_water_tower_80180154(void);
void                func_dryfield_water_tower_80180174(s16);
void                func_dryfield_water_tower_80180194(void);
void                func_dryfield_water_tower_80180220(void);

extern TmdSource D_dryfield_water_tower_80180DC8;
extern TmdSource D_dryfield_water_tower_80181A1C;
void             func_dryfield_water_tower_8017E1DC(Task*);
void             func_dryfield_water_tower_8017E764(Task*);
void             func_dryfield_water_tower_8017F128(Task*);
void             func_dryfield_water_tower_8017F700(s32);
void             func_dryfield_water_tower_8017F82C(void);
void             func_dryfield_water_tower_8017F8B0(void);
void             func_dryfield_water_tower_8017F8E8(s16);
void             func_dryfield_water_tower_8017F908(void);
void             func_dryfield_water_tower_8017F9AC(void);
void             func_dryfield_water_tower_8017FA5C(void);
void             func_dryfield_water_tower_8017FBC8(Task*);
void             func_dryfield_water_tower_8017FBD8(Task*);

extern GpGridFace D_dryfield_water_tower_80182F2C[73];
extern SVECTOR    D_dryfield_water_tower_801828CC[29];
extern SVECTOR    D_dryfield_water_tower_801829B4[175];
extern TaskDesc   D_8014D8A4;
extern s16*       D_dryfield_water_tower_80183584[16];
void              func_dryfield_water_tower_8017F77C(Task*, s32, ActorTransform* placement);
void              func_dryfield_water_tower_8017F808(Task*, s32, ActorCommand* msg);

TaskDesc D_dryfield_water_tower_80180394 = { 0, 32, func_dryfield_water_tower_8017D7D8, { .model = NULL } };

DryfieldWaterTowerMessageEntry D_dryfield_water_tower_801803A0[7] = {
    { 5102, { .call1 = func_dryfield_water_tower_8017DAF8 } },
    { 5105, { .call0 = func_dryfield_water_tower_8017DCFC } },
    { 5103, { .call0 = func_dryfield_water_tower_8017DD3C } },
    { 5104, { .call2 = func_dryfield_water_tower_8017DD04 } },
    { 5106, { .call3 = func_dryfield_water_tower_8017DC64 } },
    { 5108, { .call2 = func_dryfield_water_tower_8017DD44 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_dryfield_water_tower_801803D8[2] = {
    { 0, 32, func_dryfield_water_tower_8017D948, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

TmdBone D_dryfield_water_tower_801803F0[1] = {
#include "assets/dryfield_water_tower_model_03808_skeleton.inc"
};

u32 D_dryfield_water_tower_80180414[1] = {
#include "assets/dryfield_water_tower_model_03808_partVerts.inc"
};

SVECTOR D_dryfield_water_tower_80180418[97] = {
#include "assets/dryfield_water_tower_model_03808_verts.inc"
};

u32 D_dryfield_water_tower_80180720[426] = {
#include "assets/dryfield_water_tower_model_03808_stream.inc"
};

TmdSource D_dryfield_water_tower_80180DC8 = {
    0,
    3360,
    0,
    1,
    D_dryfield_water_tower_80180414,
    D_dryfield_water_tower_80180418,
    &D_dryfield_water_tower_80180418[97],
    D_dryfield_water_tower_801803F0,
    D_dryfield_water_tower_80180720,
};

TmdBone D_dryfield_water_tower_80180DEC[1] = {
#include "assets/dryfield_water_tower_model_0445C_skeleton.inc"
};

u32 D_dryfield_water_tower_80180E10[1] = {
#include "assets/dryfield_water_tower_model_0445C_partVerts.inc"
};

SVECTOR D_dryfield_water_tower_80180E14[152] = {
#include "assets/dryfield_water_tower_model_0445C_verts.inc"
};

u32 D_dryfield_water_tower_801812D4[466] = {
#include "assets/dryfield_water_tower_model_0445C_stream.inc"
};

TmdSource D_dryfield_water_tower_80181A1C = {
    0,
    3680,
    0,
    1,
    D_dryfield_water_tower_80180E10,
    D_dryfield_water_tower_80180E14,
    &D_dryfield_water_tower_80180E14[152],
    D_dryfield_water_tower_80180DEC,
    D_dryfield_water_tower_801812D4,
};

// Placement task reads and dispatches entry 1.
ActorTransform D_dryfield_water_tower_80181A40[2] = {
    { { 3100, 0, 0, 0 }, { 0, 1024, 0, 0 } },
    { { 3100, 0, 1800, 0 }, { 0, 1024, 0, 0 } },
};

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
ActorTransform D_dryfield_water_tower_80181A70[3] = {
    { { -2900, -8000, 0, 0 }, { 0, 3072, 0, 0 } },
    { { -2900, -4000, 0, 0 }, { 0, 3072, 0, 0 } },
    { { -2500, -200, 0, 0 }, { 0, 3072, 0, 0 } },
};

ActorTransform D_dryfield_water_tower_80181AB8 = { { -2500, -4000, 0, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_dryfield_water_tower_80181AD0[2] = {
    { { 1900, 0, -270, 0 }, { 0, 3072, 0, 0 } },
    { { 2500, 0, -270, 0 }, { 0, 3072, 0, 0 } },
};

DryfieldWaterTower2MessageEntry D_dryfield_water_tower_80181B00[2] = {
    { 2004, { .call2 = func_dryfield_water_tower_8017F77C } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_dryfield_water_tower_8017F808 } },
};

SVECTOR D_dryfield_water_tower_80181B10[2] = {
#include "assets/dryfield_water_tower_collision_04550.inc"
};

SVECTOR D_dryfield_water_tower_80181B20[8] = {
#include "assets/dryfield_water_tower_collision_04560.inc"
};

SVECTOR D_dryfield_water_tower_80181B60[8] = {
#include "assets/dryfield_water_tower_collision_045A0.inc"
};

GpGridFace D_dryfield_water_tower_80181BA0[2] = {
#include "assets/dryfield_water_tower_collision_045E0.inc"
};

SVECTOR D_dryfield_water_tower_80181BB8[2] = {
#include "assets/dryfield_water_tower_collision_045F8.inc"
};

SVECTOR D_dryfield_water_tower_80181BC8[8] = {
#include "assets/dryfield_water_tower_collision_04608.inc"
};

SVECTOR D_dryfield_water_tower_80181C08[8] = {
#include "assets/dryfield_water_tower_collision_04648.inc"
};

GpGridFace D_dryfield_water_tower_80181C48[2] = {
#include "assets/dryfield_water_tower_collision_04688.inc"
};

u16 D_dryfield_water_tower_80181C60[12] = {
    400,
    1000,
    0xFE70,
    0xFF38,
    0,
    0xFDA8,
    200,
    800,
    0xFC18,
    0xFCE0,
    600,
    0,
};

GpEvsCmd D_dryfield_water_tower_80181C78[14] = {
    { 47, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_water_tower_8017F700 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_8017F8E8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_8017F8E8 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_8017F8E8 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_water_tower_80181DC8[8] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_water_tower_8017F908 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_water_tower_80181E88[15] = {
    { 47, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_8017F8E8 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_water_tower_8017F8B0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_water_tower_8017F700 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_8017F8E8 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_8017F8E8 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_water_tower_80181FF0[8] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_water_tower_8017F9AC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_water_tower_801820B0[17] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_water_tower_8017F8B0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_water_tower_8017F700 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_8017F8E8 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_8017F8E8 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_8017F8E8 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_8017F8E8 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_water_tower_8017F82C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_water_tower_80182248[11] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_water_tower_8017FA5C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_water_tower_8017F82C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

DwtwViewVolume D_dryfield_water_tower_80182350[9] = {
    { 3, 60 },
    { 4, 75 },
    { 5, 50 },
    { 6, 50 },
    { 7, 75 },
    { 8, 90 },
    { 19, 75 },
    { 20, 100 },
    { 0xFFFF, 0xFFFF },
};

DryfieldWaterTower2MessageEntry D_dryfield_water_tower_80182374[2] = {
    { 5108, { .call0 = func_dryfield_water_tower_8017FBC8 } },
    { 5100, { .call0 = func_dryfield_water_tower_8017FBD8 } },
};

TaskDesc D_dryfield_water_tower_80182384[3] = {
    { 0, 192, func_dryfield_water_tower_8017F128, { .model = NULL } },
    { 257, 192, func_dryfield_water_tower_8017E764, { .model = &D_dryfield_water_tower_80180DC8 } },
    { 257, 192, func_dryfield_water_tower_8017E1DC, { .model = &D_dryfield_water_tower_80181A1C } },
};

ActorTransform D_dryfield_water_tower_801823A8 = { { -700, -1, -4500, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_dryfield_water_tower_801823C0[4] = {
    { { -4465, -1, 0, 0 }, { 0, 1024, 0, 0 } },
    { { -4000, 100, -1200, 0 }, { 3840, 1024, 0, 0 } },
    { { -4465, -1, 1000, 0 }, { 0, 1024, 0, 0 } },
    { { -4000, 100, -1200, 0 }, { 3968, 1024, 0, 0 } },
};

// Retained parameter record; layout follows the adjacent script arguments.

AnimationPlayRequest D_dryfield_water_tower_80182420[3] = {
    { { .index = 0 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

GpOverlayIds D_dryfield_water_tower_8018245C = { 2, 16, 11 };

GpEvsCmd D_dryfield_water_tower_80182464[22] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 12, { .overlays = &D_dryfield_water_tower_8018245C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_water_tower_80180114 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_80180174 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_water_tower_80180134 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_80180174 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_80180174 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_80180174 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_water_tower_80180154 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_water_tower_80180194 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_water_tower_80180174 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_water_tower_80182674[11] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_water_tower_80180220 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_water_tower_80180194 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_dryfield_water_tower_8018277C[3] = {
    { 0, 192, func_dryfield_water_tower_8017FD64, { .model = NULL } },
    { 0, 192, func_dryfield_water_tower_80180038, { .model = NULL } },
    { 0, 192, func_dryfield_water_tower_8017FF5C, { .model = NULL } },
};

u16 D_dryfield_water_tower_801827A0[22] = {
    0,
    0,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    0,
    2,
    2,
    2,
    2,
    2,
    2,
    0,
    0,
    0,
    2,
    2,
    3838,
};

GpRoomObjRec D_dryfield_water_tower_801827CC[1] = {
    { D_dryfield_water_tower_801835C4, D_dryfield_water_tower_8018665C, &D_dryfield_water_tower_8018665C[14], NULL },
};

u8* D_dryfield_water_tower_801827DC[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_water_tower_801827E0[1] = {
    { { .bytes = { 21, 0 } } },
};

GpRoomCoordRec D_dryfield_water_tower_801827E4[1] = {
    { D_dryfield_water_tower_801874E4, NULL },
};

GpWarpRec D_dryfield_water_tower_801827EC[4] = {
    { { .words = { 0, -2437, 0, -5662 } }, { 0, 0, 0, 0 }, { .words = { 0, -2437, 0, -5662 } }, { 0, 0, 0, 0 }, 0x52140004, 0x52140003, 0, 4, 0, 477 },
    { { .words = { 3072, 5646, 0, 1800 } }, { 0, 0, 0, 0 }, { .words = { 3072, 5646, 0, 1800 } }, { 0, 0, 0, 0 }, 0x52140002, 0x52140001, 0x52140008, 20, 0, 476 },
    { { .words = { 1024, -2186, 1, 170 } }, { 0, 0, 0, 0 }, { .words = { 1024, -2186, 1, 170 } }, { 0, 0, 0, 0 }, 0x52140005, 0x52140005, 0, 9, 2, 0 },
    { { .words = { 3072, 5500, 1, -5310 } }, { 0, 0, 0, 0 }, { .words = { 3072, 5500, 1, -5310 } }, { 0, 0, 0, 0 }, 0, 0x52140005, 0, 3, 2, 0 },
};

SVECTOR D_dryfield_water_tower_801828CC[29] = {
#include "assets/dryfield_water_tower_collision_06004_normals.inc"
};

SVECTOR D_dryfield_water_tower_801829B4[175] = {
#include "assets/dryfield_water_tower_collision_06004_verts.inc"
};

GpGridFace D_dryfield_water_tower_80182F2C[73] = {
#include "assets/dryfield_water_tower_collision_06004_faces.inc"
};

s16 D_dryfield_water_tower_80183298[374] = {
#include "assets/dryfield_water_tower_collision_06004_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_water_tower_80183298[i])
s16* D_dryfield_water_tower_80183584[16] = {
#include "assets/dryfield_water_tower_collision_06004_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_water_tower_801835C4[1] = {
    { NULL, D_dryfield_water_tower_801828CC, D_dryfield_water_tower_801829B4, D_dryfield_water_tower_80182F2C, D_dryfield_water_tower_80183584, 7000, 7000, 4, 4, 4000, 73 },
};

GpViewRec D_dryfield_water_tower_801835E8[20] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x5DC0, 0 } }, 322 },
    { { { { 2851, 0, -2940 }, { -2508, 2135, -2433 }, { 1532, 3495, 1486 } }, { -4690, 2700, 5660 } }, 257 },
    { { { { 0, 0, -4096 }, { 23, 4095, 0 }, { 4095, -23, 0 } }, { 2600, 1169, 4500 } }, 257 },
    { { { { -119, 0, 4094 }, { -19, 4095, 0 }, { -4094, -19, -119 } }, { -2900, 1170, 4250 } }, 257 },
    { { { { -3811, 0, -1500 }, { -121, 4082, 307 }, { 1495, 330, -3798 } }, { 5810, 1779, -4040 } }, 257 },
    { { { { 4000, 0, -878 }, { 74, 4081, 338 }, { 875, -346, 3986 } }, { 5550, 1170, 2970 } }, 257 },
    { { { { 1168, 0, 3925 }, { 3159, 2431, -939 }, { -2330, 3296, 693 } }, { -4290, 6370, -3260 } }, 257 },
    { { { { 4001, 0, 876 }, { 118, 4058, -539 }, { -868, 552, 3964 } }, { -5510, 1500, 5900 } }, 257 },
    { { { { 4002, 0, -870 }, { -738, 2168, -3395 }, { 460, 3475, 2118 } }, { 700, 5770, 3420 } }, 230 },
    { { { { 4044, 0, 649 }, { 244, 3795, -1519 }, { -602, 1539, 3747 } }, { -190, 2050, 3580 } }, 257 },
    { { { { 4095, 0, -54 }, { 3, 4087, 255 }, { 54, -256, 4087 } }, { 4140, 1170, 5700 } }, 230 },
    { { { { 3988, 0, -930 }, { -333, 3823, -1430 }, { 868, 1469, 3723 } }, { 4400, 1470, 3720 } }, 271 },
    { { { { 4094, 0, -112 }, { 4, 4091, 181 }, { 112, -181, 4090 } }, { 4140, 1570, 700 } }, 230 },
    { { { { 3261, 0, 2478 }, { 0, 4096, 0 }, { -2478, 0, 3261 } }, { -4900, 1150, 2500 } }, 257 },
    { { { { 0, 0, 4096 }, { -1074, 3952, 0 }, { -3952, -1074, 0 } }, { -5900, 750, 1000 } }, 257 },
    { { { { -3562, 0, 2021 }, { 462, 3987, 815 }, { -1968, 937, -3467 } }, { -4750, 1750, 40 } }, 289 },
    { { { { 3429, 0, 2240 }, { -689, 3896, 1056 }, { -2131, -1261, 3262 } }, { -4790, 1110, 1250 } }, 289 },
    { { { { 2272, 0, -3408 }, { 2302, 3019, 1535 }, { 2512, -2767, 1674 } }, { 6000, 1890, 4000 } }, 221 },
    { { { { -3657, 0, 1843 }, { 0, 4096, 0 }, { -1843, 0, -3657 } }, { -1430, 1250, -4770 } }, 257 },
    { { { { 1485, 0, -3816 }, { -3580, 1417, -1394 }, { 1321, 3842, 514 } }, { -3517, 4818, -1078 } }, 257 },
};

SpriteBatch D_dryfield_water_tower_801838B8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_tower_801838C8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_water_tower_801838D8[38] = {
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -80, 24, 1250, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -88, -40, 1250, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, 24, 1250, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -96, 0, 1250, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -96, -104, 1250, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -120, 1250, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -104, -64, 1250, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, 24, 1250, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -56, 1375, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 0, 1375, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -144, -48, 1375, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 0, 1375, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, -56, 1375, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 0, 1375, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, -48, 1375, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -112, 0, 1375, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -96, -56, 1375, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, 0, 1375, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, -80, -56, 1375, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, -80, 0, 1375, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -56, 1375, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, 0, 1375, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, 0, 1250, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -64, 1250, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -88, -72, 1125, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -88, 0, 1125, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -96, 0, 1000, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -96, -80, 1000, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, -88, 875, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, 0, 875, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -128, 0, 750, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -128, -104, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -144, -120, 687, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -144, 0, 687, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -160, -120, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -160, 0, 625, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, -120, 1375, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -80, -40, 1375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tower_80183BD0[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 12, 0, 0, { 0, 0 } },
    { 20, 16, 0, 0, { 2, 0 } },
    { 36, 2, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_water_tower_80183C00[38] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 24, 1250, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -40, 1250, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 72, 8, 1250, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -120, 1250, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 80, -72, 1250, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, 0, 1250, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 32, 1250, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, -120, 1250, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -48, 1500, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 8, 1500, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 72, -48, 1500, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 72, 0, 1500, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -56, 1500, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 88, 0, 1500, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -56, 1500, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 104, 0, 1500, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -56, 1500, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 120, 0, 1500, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 136, -56, 1500, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 136, 0, 1500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, -56, 1375, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, 0, 1375, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -64, 1250, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, 0, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 72, -64, 1125, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 72, 0, 1125, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 80, -88, 1000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, 0, 1000, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 96, -104, 875, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 96, 0, 875, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 136, -120, 625, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 136, -64, 625, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 136, 0, 625, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 136, 64, 625, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 112, 0, 750, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 112, 56, 750, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 112, -120, 750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 112, -64, 750, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tower_80183EF8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 12, 0, 0, { 2, 0 } },
    { 20, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_water_tower_80183F20[47] = {
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -8, -40, 2000, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -24, -40, 2000, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -40, -40, 2000, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -56, -40, 2000, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -72, -40, 2000, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -88, -40, 2000, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -104, -40, 2000, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -120, -40, 2000, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -136, -40, 2000, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -160, -40, 2000, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -40, -120, 2000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -32, -96, 2000, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -8, 16, 2000, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -24, -32, 2000, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -32, 16, 2000, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 0, 2000, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -160, -120, 2000, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, -48, 2000, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 64, 1250, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 88, 48, 1250, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, 40, 1250, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 104, 24, 1250, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -16, 1250, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, 24, 1250, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, 56, 1250, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 88, 1250, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 128, -40, 1250, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, 8, 1250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, 48, 1250, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 128, 88, 1250, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -8, -40, 1844, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -16, -48, 1705, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -16, 0, 1693, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -32, 0, 1599, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -32, -48, 1556, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -48, -48, 1334, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -48, 0, 1339, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -64, -56, 1208, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -64, 8, 1248, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, -88, -56, 1100, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, -88, 16, 1102, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -112, -64, 963, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -112, 16, 975, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -136, -64, 857, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -136, 24, 864, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -160, -72, 774, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -160, 24, 790, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tower_801842CC[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 3, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { 10, 5, 0, 0, { 5, 0 } },
    { 15, 3, 0, 0, { 1, 0 } },
    { 18, 12, 0, 0, { 4, 0 } },
    { 30, 17, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_water_tower_8018430C[30] = {
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 48, -24, 2000, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 64, -24, 2000, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 80, -24, 2000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 96, -24, 2000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 112, -24, 2000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 128, -24, 2000, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 144, -24, 2000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -32, 1500, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 48, 16, 1500, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, -24, 2500, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, 16, 2500, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -40, 1375, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 56, 16, 1375, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, -40, 1250, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, 24, 1250, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, 88, -48, 1125, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, 88, 24, 1125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 104, -56, 1000, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 104, 24, 1000, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 120, -64, 937, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 120, 24, 937, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 136, -80, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 136, 24, 875, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, 40, 1500, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 56, 8, 1500, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 64, -72, 1500, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 64, 8, 1500, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 72, -120, 1500, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 72, -72, 1500, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 72, 8, 1500, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tower_80184564[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 16, 0, 0, { 2, 0 } },
    { 23, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_water_tower_8018458C[63] = {
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, 24, 409, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 40, 455, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 56, 518, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 72, 594, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -96, 88, 699, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 104, 825, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -120, 1125, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -8, -120, 1125, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -96, 1125, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, -72, 1125, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -32, -48, 1125, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -40, -24, 1125, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -48, 0, 1125, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -80, 88, 1125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -72, 72, 1125, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 48, 1125, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -56, 24, 1125, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, 48, 1250, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 48, 1250, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, -64, 1250, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, -48, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -128, -24, 1250, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 48, 1250, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, 48, 1250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, 48, 1250, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -80, 48, 983, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -64, 72, 1183, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -112, -24, 1250, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, -24, 1250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, 8, 1250, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -120, 1250, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -120, 1250, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -112, -48, 1250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -120, 1250, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -96, 2000, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, -72, 2250, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, -80, 1250, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -120, -80, 1250, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -96, -80, 1250, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -88, -80, 2250, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -48, 1250, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -48, 2000, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 112, 1013, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -104, 2602, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -80, 1544, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -48, 1526, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -8, 1391, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -48, 24, 1326, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -40, 64, 1484, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, -104, 1917, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -144, -104, 1979, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -128, -104, 2035, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -112, -104, 2058, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, -112, 2008, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, -112, 2044, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, -56, 2500, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -96, -64, 2500, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, -8, 1500, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, 0, 1500, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -40, -8, 1500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -32, -16, 1625, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, -24, 1625, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -32, 24, 1875, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tower_80184A78[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 3, 0 } },
    { 6, 11, 0, 0, { 4, 0 } },
    { 17, 25, 0, 0, { 1, 0 } },
    { 42, 7, 0, 0, { 5, 0 } },
    { 49, 6, 0, 0, { 0, 0 } },
    { 55, 2, 0, 0, { 6, 0 } },
    { 57, 6, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_water_tower_80184AC0[111] = {
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -160, -48, 2375, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -144, -56, 2375, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -128, -56, 2375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -112, -56, 2375, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, -64, 2375, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, -64, 2375, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -64, -64, 2375, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, -24, 2375, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -32, -24, 2375, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, -56, 2375, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -64, 2375, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -16, -48, 2375, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -32, -72, 1794, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -24, -64, 1978, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -16, -64, 2140, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 0, 831, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -144, 56, 884, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -136, 56, 876, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 56, 869, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 64, 918, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -152, 64, 910, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -144, 64, 884, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 64, 878, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 64, 860, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, -104, 829, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -160, -24, 871, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -144, -104, 819, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -144, -24, 857, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 40, 866, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 40, 868, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 40, 887, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 48, 887, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 48, 871, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 48, 889, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 56, 905, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 56, 874, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 56, 896, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 64, 894, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 64, 878, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 64, 897, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 72, 870, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 72, 877, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 72, 892, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -128, -120, 812, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -120, -120, 812, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -104, 0, 875, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -120, -48, 875, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 0, 875, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 40, 889, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -152, 40, 882, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -144, 40, 874, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -136, 40, 853, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 40, 847, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 40, 864, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 40, 869, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 48, 893, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -152, 48, 886, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -144, 48, 879, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -136, 48, 872, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 48, 854, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 48, 893, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 48, 871, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 56, 898, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -152, 56, 890, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -144, 56, 883, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -136, 56, 876, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 56, 859, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 56, 888, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 56, 874, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 64, 918, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -152, 64, 911, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -144, 64, 904, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -136, 64, 878, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 64, 879, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 64, 866, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -144, 72, 901, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -136, 72, 908, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, -120, 875, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -160, -48, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -144, -120, 875, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, -48, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 0, 875, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -136, 48, 869, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 48, 854, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 48, 883, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 48, 870, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 48, 894, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -96, 48, 1036, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 48, 1143, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -136, 56, 868, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 56, 859, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 56, 886, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 56, 875, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 56, 890, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 64, 860, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 64, 891, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 64, 879, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 72, 866, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, 72, 865, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -144, -112, 812, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -136, -112, 812, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -136, -24, 812, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -120, -104, 875, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -120, -24, 875, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -104, -96, 1000, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -104, -24, 1000, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -88, -96, 1125, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -88, -24, 1125, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -72, -88, 1250, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -72, -16, 1250, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, 8, 1250, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tower_8018536C[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 3, 0 } },
    { 12, 3, 0, 0, { 0, 0 } },
    { 15, 13, 0, 0, { 5, 0 } },
    { 28, 20, 0, 0, { 1, 0 } },
    { 48, 34, 0, 0, { 4, 0 } },
    { 82, 29, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_water_tower_801853AC[102] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 64, 947, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 64, 874, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, 72, 976, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, 72, 1129, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 80, 1384, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 80, 1359, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -48, 112, 836, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -16, 104, 916, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 16, 96, 930, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 40, 88, 903, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 72, 80, 997, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 104, 72, 960, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 120, 64, 973, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -80, 750, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, -88, 750, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, -104, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -112, -120, 750, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, -120, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -120, 750, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -64, -120, 750, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, 88, 750, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -128, 80, 750, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -88, 72, 750, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -56, 64, 750, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -16, 56, 750, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 24, 48, 750, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 56, 40, 750, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 96, 32, 750, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, 24, 750, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -120, 1337, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -128, -112, 1750, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -80, 1875, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -64, 1750, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -40, 1125, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -40, 1375, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -40, 1875, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, 0, 1125, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 0, 1337, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, 0, 1875, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 48, 1125, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 48, 1337, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 48, 1350, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 1125, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 88, 1337, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 88, 1750, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -80, 1337, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, -80, 1125, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -144, -120, 1750, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, -120, 1750, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, -120, 1750, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -104, -120, 1750, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -24, -88, 2000, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -120, 1875, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, -120, 1937, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -88, 2000, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 8, -120, 1937, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -88, 2000, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -88, 2000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, -104, 1937, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -120, 1922, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -96, 1060, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, -96, 2072, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, -120, 1912, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -40, -120, 1717, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -40, -88, 2039, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -56, -120, 1718, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -56, -88, 2058, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -72, -120, 215, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -72, -88, 2005, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -120, 226, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -88, 1874, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -64, 1215, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -24, 1512, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, -8, 1675, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, -16, 1600, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 32, -32, 1450, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 32, -40, 1381, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 32, -48, 1320, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 32, -56, 1273, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 40, -104, 1017, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 48, -112, 983, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 48, -120, 953, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 40, -96, 1055, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 40, -88, 1087, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 40, -80, 1127, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 40, -72, 1175, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, -88, 1875, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 72, -120, 1875, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 80, -120, 1875, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 88, -120, 1875, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -120, 1875, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 0, 1500, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, -16, 1500, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -32, 1500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 136, 16, 1500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 152, 64, 1500, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 152, 16, 1500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, -48, 1500, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, -120, 2000, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -128, -120, 2000, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -112, -104, 2000, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, -72, 2000, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tower_80185BA4[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 3, 0 } },
    { 13, 16, 0, 0, { 4, 0 } },
    { 29, 18, 0, 0, { 1, 0 } },
    { 47, 24, 0, 0, { 5, 0 } },
    { 71, 15, 0, 0, { 0, 0 } },
    { 86, 12, 0, 0, { 6, 0 } },
    { 98, 4, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_tower_80185BEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_water_tower_80185BFC[17] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 80, 750, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 80, 750, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 80, 750, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 80, 750, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -16, 80, 750, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 80, 750, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 80, 750, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, 88, 750, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 88, 750, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 88, 750, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, 88, 750, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 88, 750, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, 104, 750, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 104, 750, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 104, 750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, 104, 750, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 104, 750, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tower_80185D50[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_water_tower_80185D68[20] = {
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -48, 40, 500, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -32, 56, 500, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, 64, 500, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, 64, 500, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, 64, 500, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 64, 500, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, 64, 500, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, 64, 500, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 64, 500, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, 56, 500, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -48, 80, 500, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -32, 80, 500, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -16, 80, 500, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, 80, 500, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 16, 80, 500, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, 80, 500, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, 80, 500, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, 80, 500, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 80, 80, 500, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, 80, 500, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tower_80185EF8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_tower_80185F10[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_water_tower_80185F20[47] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 24, 875, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 40, -88, 625, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 40, 625, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -16, 750, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, 32, 750, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 56, -80, 750, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 0, 750, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 32, 750, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, 40, 875, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 72, 0, 875, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -72, 875, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 80, -24, 875, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -48, 1250, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, -48, 1250, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -16, 1250, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, -16, 1250, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, 16, 1250, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, 16, 1250, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 48, 1222, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 88, 48, 1250, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, 0, 1625, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 8, 0, 1625, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 24, 0, 1625, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 0, 1625, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -24, 1625, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 24, -24, 1625, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 8, -24, 1625, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -8, -24, 1625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, -56, -120, 250, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -56, 8, 250, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, -72, -120, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -72, 8, 250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -88, 8, 475, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -104, 8, 450, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -120, 8, 425, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -136, 8, 412, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -152, 8, 400, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, 8, 387, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, -64, 387, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -152, -64, 400, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -136, -64, 412, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -120, -64, 425, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -104, -64, 450, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -88, -64, 475, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -56, 1125, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -16, 1125, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 32, 1125, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tower_801862CC[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { 12, 8, 0, 0, { 3, 0 } },
    { 20, 8, 0, 0, { 2, 0 } },
    { 28, 16, 0, 0, { 4, 0 } },
    { 44, 3, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_tower_80186304[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_tower_80186314[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_tower_80186324[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_water_tower_80186334[16] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, -56, 2500, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, -88, 250, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -40, -104, 250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, -104, 250, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, -120, 250, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, -120, 250, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -112, 250, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -72, -96, 250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -56, -80, 250, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, -64, 250, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, -24, 250, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -48, 250, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -16, 250, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, -40, 250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -8, 250, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -16, 250, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tower_80186474[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_water_tower_80186494[1] = {
    { 143, 0x3FC0, { .fields = { 80, 136 } }, -64, -88, 367, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tower_801864A8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_dryfield_water_tower_801864C0[2] = {
    { { 316, 1, 3, 2 }, 500 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtElem D_dryfield_water_tower_801864D4[5] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, -48, 375, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -56, 375, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -72, -80, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -56, -72, 375, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, -64, 375, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tower_80186538[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_tower_80186550[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_water_tower_80186560[21] = {
    { { .empty = D_dryfield_water_tower_801838B8 }, D_dryfield_water_tower_801838B8, NULL },
    { { .empty = D_dryfield_water_tower_801838C8 }, D_dryfield_water_tower_801838C8, NULL },
    { { .elements = D_dryfield_water_tower_801838D8 }, D_dryfield_water_tower_80183BD0, NULL },
    { { .elements = D_dryfield_water_tower_80183C00 }, D_dryfield_water_tower_80183EF8, NULL },
    { { .elements = D_dryfield_water_tower_80183F20 }, D_dryfield_water_tower_801842CC, NULL },
    { { .elements = D_dryfield_water_tower_8018430C }, D_dryfield_water_tower_80184564, NULL },
    { { .elements = D_dryfield_water_tower_8018458C }, D_dryfield_water_tower_80184A78, NULL },
    { { .elements = D_dryfield_water_tower_80184AC0 }, D_dryfield_water_tower_8018536C, NULL },
    { { .elements = D_dryfield_water_tower_801853AC }, D_dryfield_water_tower_80185BA4, NULL },
    { { .empty = D_dryfield_water_tower_80185BEC }, D_dryfield_water_tower_80185BEC, NULL },
    { { .elements = D_dryfield_water_tower_80185BFC }, D_dryfield_water_tower_80185D50, NULL },
    { { .elements = D_dryfield_water_tower_80185D68 }, D_dryfield_water_tower_80185EF8, NULL },
    { { .empty = D_dryfield_water_tower_80185F10 }, D_dryfield_water_tower_80185F10, NULL },
    { { .elements = D_dryfield_water_tower_80185F20 }, D_dryfield_water_tower_801862CC, NULL },
    { { .empty = D_dryfield_water_tower_80186304 }, D_dryfield_water_tower_80186304, NULL },
    { { .empty = D_dryfield_water_tower_80186314 }, D_dryfield_water_tower_80186314, NULL },
    { { .empty = D_dryfield_water_tower_80186324 }, D_dryfield_water_tower_80186324, NULL },
    { { .elements = D_dryfield_water_tower_80186334 }, D_dryfield_water_tower_80186474, NULL },
    { { .elements = D_dryfield_water_tower_80186494 }, D_dryfield_water_tower_801864A8, D_dryfield_water_tower_801864C0 },
    { { .elements = D_dryfield_water_tower_801864D4 }, D_dryfield_water_tower_80186538, NULL },
    { { .empty = D_dryfield_water_tower_80186550 }, D_dryfield_water_tower_80186550, NULL },
};

GpObj4C D_dryfield_water_tower_8018665C[14] = {
    { NULL, NULL, NULL, { 447, -1888, -4529, 0 }, { { -602, -2912, -2368, 0 }, { 595, -2912, 2364, 0 }, { -602, 2912, -2368, 0 }, { 595, 2912, 2364, 0 } }, { 3978, 0, -1007, 0 }, { 0, 0, 4096, 0 }, 3797, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 607, -1984, -4417, 0 }, { { 594, -3008, 2362, 0 }, { -603, -3008, -2369, 0 }, { 594, 3008, 2362, 0 }, { -603, 3008, -2369, 0 } }, { -3981, 0, 1006, 0 }, { 0, 0, 4096, 0 }, 3865, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -4449, -1712, -4673, 0 }, { { 1730, -2736, 1722, 0 }, { -1730, -2736, -1722, 0 }, { 1730, 2736, 1722, 0 }, { -1730, 2736, -1722, 0 } }, { -2892, 0, 2904, 0 }, { 0, 0, 4096, 0 }, 3665, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -4640, -1632, -4544, 0 }, { { -1730, -2656, -1722, 0 }, { 1730, -2656, 1722, 0 }, { -1730, 2656, -1722, 0 }, { 1730, 2656, 1722, 0 } }, { 2897, 0, -2912, 0 }, { 0, 0, 4096, 0 }, 3602, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -4865, -1680, 575, 0 }, { { -2441, -2704, 6, 0 }, { 2441, -2704, -6, 0 }, { -2441, 2704, 6, 0 }, { 2441, 2704, -6, 0 } }, { -11, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 3638, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { -4770, -1776, 446, 0 }, { { 2441, -2800, -17, 0 }, { -2440, -2800, 17, 0 }, { 2441, 2800, -17, 0 }, { -2440, 2800, 17, 0 } }, { 28, 0, 4105, 0 }, { 0, 0, 4096, 0 }, 3709, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { -3138, -1696, 3709, 0 }, { { 279, -2720, -1510, 0 }, { -278, -2720, 1511, 0 }, { 279, 2720, -1510, 0 }, { -278, 2720, 1511, 0 } }, { 4035, 0, 743, 0 }, { 0, 0, 4096, 0 }, 3114, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { -2816, -1792, 3518, 0 }, { { -238, -2816, 1330, 0 }, { 238, -2816, -1330, 0 }, { -238, 2816, 1330, 0 }, { 238, 2816, -1330, 0 } }, { -4039, 0, -724, 0 }, { 0, 0, 4096, 0 }, 3114, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { 5343, -1824, -2755, 0 }, { { 2712, -2848, 15, 0 }, { -2711, -2848, -14, 0 }, { 2712, 2848, 15, 0 }, { -2711, 2848, -14, 0 } }, { -23, 0, 4103, 0 }, { 0, 0, 4096, 0 }, 3924, 0, 8, 3, 1, 0 },
    { NULL, NULL, NULL, { 5280, -1904, -2561, 0 }, { { -2757, -2928, -12, 0 }, { 2756, -2928, 11, 0 }, { -2757, 2928, -12, 0 }, { 2756, 2928, 11, 0 } }, { 16, 0, -4105, 0 }, { 0, 0, 4096, 0 }, 4015, 0, 3, 8, 1, 0 },
    { NULL, NULL, NULL, { 3023, -2016, -65, 0 }, { { 4, -3040, 1207, 0 }, { -3, -3040, -1206, 0 }, { 4, 3040, 1207, 0 }, { -3, 3040, -1206, 0 } }, { -4104, 0, 11, 0 }, { 0, 0, 4096, 0 }, 3268, 0, 9, 8, 1, 0 },
    { NULL, NULL, NULL, { 2847, -2128, -33, 0 }, { { -3, -3152, -1206, 0 }, { 4, -3152, 1207, 0 }, { -3, 3152, -1206, 0 }, { 4, 3152, 1207, 0 } }, { 4099, 0, -13, 0 }, { 0, 0, 4096, 0 }, 3367, 0, 8, 9, 1, 0 },
    { NULL, NULL, NULL, { 4431, -2048, 511, 0 }, { { -1504, -2928, 554, 0 }, { 1490, -2928, -570, 0 }, { -1504, 2928, 554, 0 }, { 1490, 2928, -570, 0 } }, { -1447, 0, -3853, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 8, 20, 1, 0 },
    { NULL, NULL, NULL, { 4383, -1888, 479, 0 }, { { 1495, -2928, -564, 0 }, { -1499, -2928, 560, 0 }, { 1495, 2928, -564, 0 }, { -1499, 2928, 560, 0 } }, { 1445, 0, 3851, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 20, 8, 129, 0 },
};

GpObj4C D_dryfield_water_tower_80186A84[24] = {
    { NULL, NULL, NULL, { -2832, -48, -5760, 0 }, { { -624, 0, -416, 0 }, { 624, 0, -416, 0 }, { -624, 0, 416, 0 }, { 624, 0, 416, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 749, 0, 19, 18, 2, 0 },
    { NULL, NULL, NULL, { 5760, -48, 1696, 0 }, { { 416, 0, -736, 0 }, { 416, 0, 736, 0 }, { -416, 0, -736, 0 }, { -416, 0, 736, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 844, 0, 22, 33, 2, 0 },
    { NULL, NULL, NULL, { -2400, -48, 112, 0 }, { { -768, 0, -624, 0 }, { 769, 0, -624, 0 }, { -768, 0, 624, 0 }, { 769, 0, 624, 0 } }, { 0, 4104, 0, 0 }, { 4090, 0, -201, 0 }, 989, 0, 21, 49, 2, 0 },
    { NULL, NULL, NULL, { 1008, -64, 3664, 0 }, { { -400, 0, -336, 0 }, { 528, 0, -592, 0 }, { -848, 0, 464, 0 }, { 720, 0, 464, 0 } }, { 0, 4105, 0, 0 }, { 201, 0, 4091, 0 }, 966, 2, 7, 255, 2, 0 },
    { NULL, NULL, NULL, { 688, -64, -4416, 0 }, { { -435, 0, -2947, 0 }, { 862, 0, 2233, 0 }, { -863, 0, -2234, 0 }, { 434, 0, 2946, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 2974, 0x8005, 1, 0, 3, 0 },
    { NULL, NULL, NULL, { 5568, -48, -5248, 0 }, { { 416, 0, -624, 0 }, { 416, 0, 624, 0 }, { -416, 0, -624, 0 }, { -416, 0, 624, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 749, 0, 30, 66, 2, 0 },
    { NULL, NULL, NULL, { 2842, -64, -128, 0 }, { { 611, 0, -1392, 0 }, { 611, 0, 1392, 0 }, { -611, 0, -1392, 0 }, { -611, 0, 1392, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 1519, 0x8005, 2, 0, 3, 0 },
    { NULL, NULL, NULL, { -3456, -64, -1280, 0 }, { { -1616, 0, -1504, 0 }, { 464, 0, -1504, 0 }, { -1616, 0, 1536, 0 }, { 464, 0, 1536, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 2217, 2, 12, 0, 4, 0 },
    { NULL, NULL, NULL, { -3456, -192, -1280, 0 }, { { -1616, 0, -1504, 0 }, { 464, 0, -1504, 0 }, { -1616, 0, 1536, 0 }, { 464, 0, 1536, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 2217, 2, 12, 0, 4, 0 },
    { NULL, NULL, NULL, { 2816, -64, -128, 0 }, { { 611, 0, -1392, 0 }, { 611, 0, 1392, 0 }, { -611, 0, -1392, 0 }, { -611, 0, 1392, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1519, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { 64, -64, -3296, 0 }, { { -3728, 0, -416, 0 }, { 3728, 0, -416, 0 }, { -3728, 0, 416, 0 }, { 3728, 0, 416, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, -4096, 0 }, 3744, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { -1856, -64, 3232, 0 }, { { -1872, 0, -416, 0 }, { 1872, 0, -416, 0 }, { -1872, 0, 416, 0 }, { 1872, 0, 416, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1915, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { 112, -64, -2528, 0 }, { { -3136, 0, -416, 0 }, { 3136, 0, -416, 0 }, { -3136, 0, 416, 0 }, { 3136, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 3156, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { 0, -64, 2432, 0 }, { { -2832, 0, -416, 0 }, { 2832, 0, -416, 0 }, { -2832, 0, 416, 0 }, { 2832, 0, 416, 0 } }, { 0, 4113, 0, 0 }, { 0, 0, -4096, 0 }, 2862, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { -3552, -64, 32, 0 }, { { -416, 0, 3312, 0 }, { -416, 0, -3312, 0 }, { 416, 0, 3312, 0 }, { 416, 0, -3312, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 3337, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { 2688, -64, -2224, 0 }, { { -416, 0, 704, 0 }, { -416, 0, -704, 0 }, { 416, 0, 704, 0 }, { 416, 0, -704, 0 } }, { 0, 4117, 0, 0 }, { -4096, 0, 0, 0 }, 817, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { 2624, -64, 2048, 0 }, { { -416, 0, 704, 0 }, { -416, 0, -704, 0 }, { 416, 0, 704, 0 }, { 416, 0, -704, 0 } }, { 0, 4117, 0, 0 }, { -4096, 0, 0, 0 }, 817, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { 3712, -64, 2208, 0 }, { { -416, 0, 1104, 0 }, { -416, 0, -1104, 0 }, { 416, 0, 1104, 0 }, { 416, 0, -1104, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1173, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { 3776, -64, -2368, 0 }, { { -416, 0, 1104, 0 }, { -416, 0, -1104, 0 }, { 416, 0, 1104, 0 }, { 416, 0, -1104, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1173, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { -2560, -64, 1856, 0 }, { { -416, 0, 1104, 0 }, { -416, 0, -1104, 0 }, { 416, 0, 1104, 0 }, { 416, 0, -1104, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1173, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { 3840, -64, 0, 0 }, { { 355, 0, -1392, 0 }, { 355, 0, 1392, 0 }, { -355, 0, -1392, 0 }, { -355, 0, 1392, 0 } }, { 0, 4102, 0, 0 }, { 4052, 0, -601, 0 }, 1431, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { -1679, 0, -1440, 0 }, { { 594, 0, -1104, 0 }, { 594, 0, 688, 0 }, { -594, 0, -1103, 0 }, { -594, 0, 1521, 0 } }, { 0, 4119, 0, 0 }, { 4091, 0, 201, 0 }, 1629, 2, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { -2240, -64, -2272, 0 }, { { 722, 0, -496, 0 }, { 722, 0, 496, 0 }, { -722, 0, -495, 0 }, { -722, 0, 497, 0 } }, { 0, 4111, 0, 0 }, { 201, 0, -4091, 0 }, 875, 2, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { -2304, -64, -352, 0 }, { { 722, 0, -496, 0 }, { 722, 0, 496, 0 }, { -722, 0, -495, 0 }, { -722, 0, 497, 0 } }, { 0, 4111, 0, 0 }, { -201, 0, 4091, 0 }, 875, 2, 12, 0, 130, 0 },
};

GpLight D_dryfield_water_tower_801871A4[4] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 208, 208, 208, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2052, 2052, 2052, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 208, 208, 208, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 208, 208, 208, { 0, 0 } },
};

GpPointLight D_dryfield_water_tower_80187304[5] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4500, -2620, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2048, 2048, 2048, { 0, 0 } }, 1339, 2360 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3200, -2159, -922 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3076, 3076, 3076, { 0, 0 } }, 2220, 3442 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -766, -2000, -4618 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3076, 3076, 3076, { 0, 0 } }, 1319, 2059 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1240, -5082, 2801 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3076, 3076, 3076, { 0, 0 } }, 7000, 7001 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3966, -2581, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1800, 3001 },
};

GpRoomCoordSet D_dryfield_water_tower_801874E4[1] = {
    { 4, D_dryfield_water_tower_801871A4, 5, D_dryfield_water_tower_80187304, 0, NULL },
};

GpAreaTmdRec D_dryfield_water_tower_801874FC[2] = {
    { 1, 216, 3, 0, { 0, 0 }, D_80151254 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_water_tower_80187514[2] = {
    { 25, 25, 0, 0, { 0, 0 }, D_801379A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_water_tower_8018752C[2] = {
    { 25, 25, 0, 0, { 0, 0 }, D_801379A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_water_tower_80187544[2] = {
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_water_tower_8018755C[2] = {
    { 1, 0, 0, -1568, 0, 2432, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_dryfield_water_tower_8018757C[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017B534, D_dryfield_water_tower_801874FC },
    { D_map_dryfield_8017B564, D_dryfield_water_tower_80187514 },
    { D_map_dryfield_8017B5E4, D_dryfield_water_tower_8018752C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_water_tower_8018755C, D_dryfield_water_tower_80187544 },
    { NULL, NULL },
    { NULL, NULL },
};

s32 D_dryfield_water_tower_801875E4[3] = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

GpRoomParamRec D_dryfield_water_tower_801875F0[1] = {
    { 0, 0, 1, 0, D_dryfield_water_tower_801875E4 },
};

GpRoomParamRec D_dryfield_water_tower_801875F8[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_dryfield_water_tower_80187600[1] = {
    { 0, 0, 1, 0, D_dryfield_water_tower_801875E4 },
};

GpRoomParamRec* D_dryfield_water_tower_80187608[8] = {
    D_dryfield_water_tower_801875F0,
    D_dryfield_water_tower_801875F8,
    D_dryfield_water_tower_80187600,
    D_dryfield_water_tower_801875F0,
    D_dryfield_water_tower_801875F0,
    D_dryfield_water_tower_801875F0,
    D_dryfield_water_tower_801875F0,
    D_dryfield_water_tower_801875F0,
};

GpScriptCmd D_dryfield_water_tower_80187628[5] = { { 1, 257 }, { 258, 513 }, { 7427, 769 }, { 4, 0 }, { 0, 0 } };

GpScriptRec D_dryfield_water_tower_8018763C[4] = {
    { 0, 0, 1, 0 },
    { 90, 120, 20, 1 },
    { 120, 120, 58, 1 },
    { 90, 190, 15, 1 },
};

GpScriptCmd D_dryfield_water_tower_8018764C[5] = {
    { 1, 257 },
    { 258, 513 },
    { 7939, 769 },
    { 4, 0 },
    { 0, 0 },
};

GpScriptRec D_dryfield_water_tower_80187660[4] = {
    { 0, 0, 1, 0 },
    { 90, 120, 20, 1 },
    { 120, 120, 65, 1 },
    { 90, 190, 15, 1 },
};

GpScriptCmd D_dryfield_water_tower_80187670[2] = {
    { 0, 1 },
    { 0, 1280 },
};

GpScriptRec D_dryfield_water_tower_80187678 = { 90, 180, 60, 1 };

DwtwStep D_dryfield_water_tower_8018767C[4] = {
    { 0, 25 },
    { 1, 30 },
    { 2, 35 },
    { 0xFFFF, 40 },
};

DryfieldWaterTowerStorage768C D_dryfield_water_tower_8018768C = { 0 };

static void       func_dryfield_water_tower_8017DE30(Task* arg0);
static s32        func_dryfield_water_tower_8017E428(Task* arg0);
static s32        func_dryfield_water_tower_8017E5B0(Task* arg0);
static void       func_dryfield_water_tower_8017E93C(Task* arg0);
static inline u16 _dryfieldWaterTowerStepFrames(Task* task);
static inline u16 _dryfieldWaterTowerState7Step(Task* arg0);
static inline u16 _dryfieldWaterTowerState8Step(Task* arg0);
static s32        func_dryfield_water_tower_8017FB4C(Task* task);
static void       func_dryfield_water_tower_8017FBE8(Task* task);

/// Cap-prop task body, in two variants picked by `spawnArg1`. With it zero the
/// task lowers the cap, driven by `DryfieldWaterTowerState::field_58`: state 0
/// spawns the table's entry-2 prop into `field_48` and returns without moving
/// the cap, state 1 hands that prop a 0x7DB record whose `field_2` asks it for
/// state 2, and state 2 publishes the lowered placement above once the cap's
/// coordinate has sunk past it, i.e. once the cap has arrived -- state 3 does
/// nothing and only the states that arrive there reach the shared tail.
///
/// A task spawned with a non-zero `spawnArg1` has no state machine: it
/// publishes the raised placement on its first frame and then only runs the
/// tail, which advances the halfword `field_6A` by 4 and moves the cap's
/// coordinate down by it -- so the cap accelerates by 4 a frame -- leaving the
/// coordinate marked dirty for the next `Gp_UpdateCoord` pass.
static void func_dryfield_water_tower_8017DE30(Task* arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->work;
    GfxCoord*                coord = arg0->extra.tmd->coords;
    ActorCommand             msg;

    if (arg0->spawnArg1.value == 0) {
        switch (state->field_58) {
            case 0:
                state->field_48 = Task_SpawnFromTable(D_dryfield_water_tower_80182384, 2, 1, 0);
                state->field_58++;
                return;

            case 1:
                msg.command = 2;
                Gp_DispatchMsgPtr(state->field_48, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
                state->field_58++;
                /* fallthrough */

            case 2:
                if (coord->coord.t[1] > D_dryfield_water_tower_80181A70[1].pos.vy) {
                    Gp_DispatchMsgPtr(arg0, 0x7D4, &D_dryfield_water_tower_80181A70[1], 0);
                    state->field_58++;
                    return;
                }
                break;

            case 3:
                return;
        }
    } else if (state->field_58 == 0) {
        Gp_DispatchMsgPtr(arg0, 0x7D4, &D_dryfield_water_tower_80181A70[0], 0);
        state->field_58++;
    }

    state->field_6A    += 4;
    coord->coord.t[1]  += (s16)state->field_6A;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// The cap-arrival test the raise prop `func_dryfield_water_tower_8017E1DC` runs
/// as its state 3 and the lower prop's state machine drives: it is the lenient
/// sibling of the lowering prop `func_dryfield_water_tower_8017DE30` above, a
/// three-state machine over the same `DryfieldWaterTowerState::field_58` that
/// reports 1 once the cap has arrived.
///
/// State 0 is the spawn tick: it raises the `field_70` shadow latch, kills the
/// prop parked in `field_48` and publishes the raised placement (the 0x7D4
/// record at 0x80181AB8, the pair's second entry). State 1 sinks the cap's
/// coordinate by 0x12C a frame and, once it passes `pos.vy` of the record at
/// 0x80181AA0 -- the height the cap has to reach -- fires this room's two arrival
/// sounds, spawns the two effects 0x190 apart around the cap and publishes the
/// placed record with 0x7D4 before advancing. State 2 counts `field_5A`; on the
/// eleventh tick it restores the three script tables from the room's data and
/// returns 1, and until then it mirrors the record's `pos.vz` into the cap's Z,
/// nudged by 0xA while the `gDisplayState.gameTick` flag bit 2 is raised. Only the states
/// that arrive there reach the shared 0x7D4 tail, and only state 2 reports
/// arrival -- which is why `func_dryfield_water_tower_8017E1DC` masks the result
/// with 0xFFFF to test it.
///
/// Two shapes here are the original's rather than stylistic. The unaligned copies
/// go to the low addresses and the room's data is the source, matching the
/// three `Mem_CopyUnaligned` argument pairs the target shows; and the `pos`
/// scratch is filled `vz`, `vy`, `vx` -- the reverse of its declaration order --
/// which is the store order the target's frame keeps.
static s32 func_dryfield_water_tower_8017DFAC(Task* arg0)
{
    DryfieldWaterTowerState* state;
    GfxCoord*                coord;
    GfxCoord*                effCoord;
    SVECTOR                  pos;
    s32                      i;

    state               = (DryfieldWaterTowerState*)arg0->work;
    coord               = arg0->extra.tmd->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    switch (state->field_58) {
        case 0:
            state->field_70 = 1;
            if (state->field_48 != NULL) {
                taskKill(state->field_48);
                state->field_48 = NULL;
            }
            Gp_DispatchMsgPtr(arg0, 0x7D4, &D_dryfield_water_tower_80181AB8, 0);
            state->field_58++;
            break;

        case 1:
            coord->coord.t[1] += 0x12C;
            if (D_dryfield_water_tower_80181A70[2].pos.vy < coord->coord.t[1]) {
                SndEvt_EnqueueType7(0x5214000B, 0);
                SndEvt_EnqueueType6(0x52140010, 0, 0);
                effCoord = arg0->extra.tmd->coords;
                pos.vz   = 0;
                pos.vy   = 0;
                pos.vx   = -0xC8;
                i        = 0;
                do {
                    Gp_SpawnEff(0x60054, effCoord, 0x80002700, &pos);
                    i++;
                    pos.vx += 0x190;
                } while ((u32)(i & 0xFFFF) < 2U);
                Gp_DispatchMsgPtr(arg0, 0x7D4, &D_dryfield_water_tower_80181A70[2], 0);
                state->field_58++;
            }
            break;

        case 2:
            state->field_5A++;
            if ((s16)state->field_5A >= 0xB) {
                Mem_CopyUnaligned(D_dryfield_water_tower_80181B10, (D_dryfield_water_tower_801828CC + 2), 0x10);
                Mem_CopyUnaligned(D_dryfield_water_tower_80181BA0, (D_dryfield_water_tower_80182F2C + 2), 0x18);
                Mem_CopyUnaligned(D_dryfield_water_tower_80181B20, (D_dryfield_water_tower_801829B4 + 8), 0x40);
                Gp_DispatchMsgPtr(arg0, 0x7D4, &D_dryfield_water_tower_80181A70[2], 0);
                return 1;
            }
            {
                s32 z             = D_dryfield_water_tower_80181A70[2].pos.vz;
                coord->coord.t[2] = z;
                if (gDisplayState.gameTick & 4) {
                    coord->coord.t[2] = z + 0xA;
                }
            }
            break;
    }
    return 0;
}

/// The table's entry-2 cap prop, the one that raises the cap: it runs the same
/// four states the lowering prop does, but reads them from `Task::state` and
/// takes no `spawnArg1` variant, and it is the task `func_dryfield_water_tower_8017F128`
/// points at the script table's entry 2.
///
/// State 0 is the spawn tick: it allocates the cap script's 0x7C-byte
/// `DryfieldWaterTowerState` block into `Task::work`, parks the slot-3 game
/// task at its `field_40`, parents the model's coordinate to `gGfxViewCoord`,
/// rebuilds the model's buffers and points its light and colour matrices
/// (`field_1C` / `field_20`) at the block, so the cap is lit by the room's own
/// state rather than by the default pair a `Tmd_Create` model starts with. It
/// then hangs the room's script table off `Task::msgTable`.
///
/// State 1 kills the prop `field_48` holds -- the lowering prop the script
/// spawned -- and states 2 and 3 hand the frame to that prop's body,
/// `func_dryfield_water_tower_8017DE30`, and then wait on
/// `func_dryfield_water_tower_8017DFAC` until it reports the cap has arrived,
/// which drops back to state 1. Every state that falls through, and the whole
/// body when the machine is skipped, stages the cap's `workm` translation and
/// hands it to `func_800D7A9C`.
///
/// `DryfieldWaterTowerState::field_70` gates the shadow: the script opcode
/// `func_dryfield_water_tower_8017FA5C` raises it, and it is read here as the
/// latch that puts a floor quad under the cap, mirrored to `-y - 0xC8` of the
/// cap's own coordinate.
///
/// `gGameSession->field_65` is the session's overlay-wait gate: while it is set
/// the prop only raises the model's skip-draw bit 0x80 and returns, leaving the
/// script's states alone.
///
/// Two shapes in the body are what the original compiled from rather than
/// stylistic choices, and folding either away re-schedules the blocks around
/// them: `new_var` is a dead zero the shadow latch is tested against instead of
/// `if (state->field_70)`, and `field_20` is stored twice -- `mem`, then the
/// field plus one -- instead of being assigned `mem + 1` outright.
void func_dryfield_water_tower_8017E1DC(Task* arg0)
{
    int                      new_var;
    DryfieldWaterTowerState* state;
    TmdObject*               obj;
    TmdObject*               model;
    GfxCoord*                coord;
    OverlayVecSlot           pos;
    MATRIX*                  mem;

    obj   = arg0->extra.tmd;
    state = (DryfieldWaterTowerState*)arg0->work;
    coord = obj->coords;
    if (gGameSession->field_65 != 0) {
        obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    new_var     = 0;
    obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    if (Gp_StateC08.field_9 == 0) {
        switch (arg0->state) {
            case 0: {
                TmdObject* model;
                GfxCoord*  modelCoord;

                model      = arg0->extra.tmd;
                modelCoord = model->coords;
                mem        = (MATRIX*)Mem_Malloc(0x7C, false);
                arg0->work = mem;
                if (mem == 0) {
                    taskKill(arg0);
                } else {
                    Mem_Set(mem, 0, 0x7C);
                    ((DryfieldWaterTowerState*)mem)->field_40 = gameGetPtrSlot(3);
                    modelCoord->parent                        = &gGfxViewCoord;
                    model->flags                              = 0;
                    Tmd_AllocBuffers(model);
                    model->colorMtx = mem;
                    model->colorMtx = model->colorMtx + 1;
                    model->lightMtx = mem;
                    arg0->msgTable  = D_dryfield_water_tower_80181B00;
                }
                arg0->state++;
                break;
            }

            case 1:
                if (state->field_48 != 0) {
                    taskKill(state->field_48);
                    state->field_48 = 0;
                }
                break;

            case 2:
                func_dryfield_water_tower_8017DE30(arg0);
                break;

            case 3:
                if (func_dryfield_water_tower_8017DFAC(arg0) & 0xFFFF) {
                    arg0->state = 1;
                }
                break;
        }
        model      = arg0->extra.tmd;
        pos.vec.vx = model->coords->workm.t[0];
        pos.vec.vy = arg0->extra.tmd->coords->workm.t[1];
        pos.vec.vz = arg0->extra.tmd->coords->workm.t[2];
        func_800D7A9C(model, &pos.vec, 0, 3);
    }
    if (state->field_70 != new_var) {
        pos.rot.vx = 0;
        pos.rot.vy = -((u16)coord->coord.t[1]) - 0xC8;
        pos.rot.vz = 0;
        Gp_DrawFloorQuad(arg0->extra.tmd->coords, 0x300, &pos.rot);
    }
}

/// A second cap-arrival body, the sibling of `func_dryfield_water_tower_8017E5B0`
/// and `func_dryfield_water_tower_8017DFAC`: its `Task::work` is the same
/// 0x7C-byte `DryfieldWaterTowerState` the cap script allocates and its
/// `extra->field_8` the cap's own coordinate, and it reports arrival the same
/// way the cap-arrival test does, by returning 1.
///
/// State 0 is the lowering tick: it sinks the cap's Z by 0x14 a frame and snaps
/// its Y to the run's lowered record, nudged by 5 while the `gDisplayState.gameTick` flag
/// bit 2 is raised; once the cap's Z has passed that record's `pos.vz` it steps
/// to state 1. Every frame of the state also spawns effect 0x60054 at the cap,
/// offset in X by the room's per-frame table entry
/// `D_..._80181C60[killCountdown]`, and wraps that 0..9 counter. State 1 counts
/// `field_5A`; on its 0x3D-th tick it publishes the 0x7D4 record at 0x80181A58
/// and returns 1, and until then mirrors that record's `pos.vx` into the cap's
/// X, nudged by the same flag. Every path clears `coord->composeStamp`, leaving the
/// coordinate dirty for the next `Gp_UpdateCoord` pass.
///
/// Where the sibling `func_dryfield_water_tower_8017E5B0` drives the run's head
/// and queues two `SndEvt_EnqueueType*` calls per lowering, this one drives the
/// record below it and queues nothing, and that is the whole of the difference
/// between the two bodies.
///
/// Two shapes here are the original's rather than stylistic. `effCoord` is
/// filled from `arg0->extra` *before* the counter update, which is what puts the
/// coordinate load `Gp_SpawnEff` takes into that block instead of the join
/// block; and the Z test is written `coord->coord.t[2] > record.pos.vz` rather
/// than the mirrored `<`, which is what makes `sgt_si` load the coordinate first
/// and emit `slt` with its operands swapped.
static s32 func_dryfield_water_tower_8017E428(Task* arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->work;
    GfxCoord*                coord = arg0->extra.tmd->coords;
    GfxCoord*                effCoord;
    SVECTOR                  pos;

    switch (state->field_58) {
        case 0:
            coord->coord.t[2] += 0x14;
            coord->coord.t[1]  = D_dryfield_water_tower_80181A40[1].pos.vy;
            if (gDisplayState.gameTick & 4) {
                coord->coord.t[1] += 5;
            }
            if (coord->coord.t[2] > D_dryfield_water_tower_80181A40[1].pos.vz) {
                state->field_58++;
            }
            effCoord = arg0->extra.tmd->coords;
            if (arg0->killCountdown >= 0xA) {
                arg0->killCountdown = 0;
            } else {
                arg0->killCountdown = (u16)arg0->killCountdown + 1;
            }
            pos.vy = 0;
            pos.vz = 0;
            pos.vx = D_dryfield_water_tower_80181C60[arg0->killCountdown];
            Gp_SpawnEff(0x60054, effCoord, 0x80002300, &pos);
            break;

        case 1:
            state->field_5A++;
            if ((s16)state->field_5A >= 0x3D) {
                Gp_DispatchMsgPtr(arg0, 0x7D4, &D_dryfield_water_tower_80181A40[1], 0);
                return 1;
            }
            coord->coord.t[0] = D_dryfield_water_tower_80181A40[1].pos.vx;
            if (gDisplayState.gameTick & 4) {
                coord->coord.t[0] += 5;
            }
            break;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// A third cap-arrival body, the sibling of `func_dryfield_water_tower_8017DFAC`
/// and `func_dryfield_water_tower_8017E428`: its `Task::work` is the same
/// 0x7C-byte `DryfieldWaterTowerState` the cap script allocates and its
/// `extra->field_8` the cap's own coordinate, and it reports arrival the same
/// way the cap-arrival test does, by returning 1.
///
/// State 0 is the lowering tick: it queues `SndEvt_EnqueueTypeB(0x5214000C,
/// 0x7F)` once, sinks the cap's Z by 0x14 a frame and snaps its Y to the
/// placement record's `pos.vy`, nudged by 5 while the `gDisplayState.gameTick` flag bit 2 is
/// raised; once the cap's Z has sunk past that record's `pos.vz` it queues the
/// same event again as 0x5214000C/0xA and steps to state 1. Every frame of the
/// state also spawns effect 0x60054 at the cap, offset in X by the room's
/// per-frame table entry `D_..._80181C60[killCountdown]`, and wraps that 0..9
/// counter. State 1 counts `field_5A`; on its 0x3D-th tick it publishes the
/// 0x7D4 record at 0x80181A40 and returns 1, and until then mirrors that
/// record's `pos.vx` into the cap's X, nudged by the same flag. Every path
/// clears `coord->composeStamp`, leaving the coordinate dirty for the next
/// `Gp_UpdateCoord` pass.
///
/// Two shapes here are the original's rather than stylistic, and folding either
/// away moves the two loads `Gp_SpawnEff` takes as its coordinate argument:
/// `effCoord` is filled from `arg0->extra` *before* the counter update, which is
/// what puts them in that block instead of the join block, and the two flag
/// branches re-read the coordinate (`+= 5`) rather than reloading the record, so
/// CSE forwards the stored value and one load serves both uses.
static s32 func_dryfield_water_tower_8017E5B0(Task* arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->work;
    GfxCoord*                coord = arg0->extra.tmd->coords;
    GfxCoord*                effCoord;
    SVECTOR                  pos;

    switch (state->field_58) {
        case 0:
            SndEvt_EnqueueTypeB(0x5214000C, 0x7F);
            coord->coord.t[2] -= 0x14;
            coord->coord.t[1]  = D_dryfield_water_tower_80181A40[0].pos.vy;
            if (gDisplayState.gameTick & 4) {
                coord->coord.t[1] += 5;
            }
            if (coord->coord.t[2] < D_dryfield_water_tower_80181A40[0].pos.vz) {
                SndEvt_EnqueueType7(0x5214000C, 0xA);
                state->field_58++;
            }
            effCoord = arg0->extra.tmd->coords;
            if (arg0->killCountdown >= 0xA) {
                arg0->killCountdown = 0;
            } else {
                arg0->killCountdown = (u16)arg0->killCountdown + 1;
            }
            pos.vy = 0;
            pos.vz = 0;
            pos.vx = D_dryfield_water_tower_80181C60[arg0->killCountdown];
            Gp_SpawnEff(0x60054, effCoord, 0x80002300, &pos);
            break;

        case 1:
            state->field_5A++;
            if ((s16)state->field_5A >= 0x3D) {
                Gp_DispatchMsgPtr(arg0, 0x7D4, &D_dryfield_water_tower_80181A40, 0);
                return 1;
            }
            coord->coord.t[0] = D_dryfield_water_tower_80181A40[0].pos.vx;
            if (gDisplayState.gameTick & 4) {
                coord->coord.t[0] += 5;
            }
            break;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// The other cap prop's task, entry 1 of the room's task table and the
/// counterpart of `func_dryfield_water_tower_8017E1DC`. It shares that prop's
/// gating -- while `gGameSession->field_65` is set it only raises the model's
/// skip-draw bit 0x80, and while `Gp_StateC08.field_9` is non-zero it clears
/// the bit and does nothing else -- and its spawn tick is the same: allocate
/// the 0x7C-byte `DryfieldWaterTowerState` into `Task::work`, park the slot-3
/// game task at `field_40`, parent the model to `gGfxViewCoord`, rebuild its
/// buffers with the block as its light and colour matrices, and hang the
/// room's script table off `Task::msgTable`.
///
/// State 1 idles. States 2 and 3 run the two cap-arrival bodies,
/// `func_dryfield_water_tower_8017E428` and `func_dryfield_water_tower_8017E5B0`,
/// each dropping back to state 1 once its body reports arrival. Every frame
/// that gets past the gates then hands the cap's `workm` translation to
/// `func_800D7A9C`.
///
/// The gate byte is read as a member of `Gp_StateC08`, not as a bare scalar:
/// `true_dependence` lets the scheduler lift a scalar load above the preceding
/// struct-member store to `flags`, and the original keeps the two in source
/// order.
void func_dryfield_water_tower_8017E764(Task* arg0)
{
    DryfieldWaterTowerState* state;
    TmdObject*               obj;
    TmdObject*               tmp;
    TmdObject*               model;
    GfxCoord*                coord;
    VECTOR                   vec;

    obj = arg0->extra.tmd;
    if (gGameSession->field_65 != 0) {
        obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    if ((s8)Gp_StateC08.field_9 != 0) {
        return;
    }
    switch (arg0->state) {
        case 0:
            tmp        = arg0->extra.tmd;
            coord      = tmp->coords;
            state      = (DryfieldWaterTowerState*)Mem_Malloc(0x7C, false);
            arg0->work = state;
            if (state == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(state, 0, 0x7C);
                state->field_40 = gameGetPtrSlot(3);
                coord->parent   = &gGfxViewCoord;
                tmp->flags      = 0;
                Tmd_AllocBuffers(tmp);
                tmp->colorMtx  = &state->colorMtx;
                tmp->lightMtx  = &state->lightMtx;
                arg0->msgTable = D_dryfield_water_tower_80181B00;
            }
            arg0->state++;
            break;

        case 1:
            break;

        case 2:
            if ((func_dryfield_water_tower_8017E428(arg0) & 0xFFFF) != 0) {
                arg0->state = 1;
            }
            break;

        case 3:
            if ((func_dryfield_water_tower_8017E5B0(arg0) & 0xFFFF) != 0) {
                arg0->state = 1;
            }
            break;
    }
    model  = arg0->extra.tmd;
    vec.vx = model->coords->workm.t[0];
    vec.vy = arg0->extra.tmd->coords->workm.t[1];
    vec.vz = arg0->extra.tmd->coords->workm.t[2];
    func_800D7A9C(model, &vec, 0, 3);
}

/// The cap script's command dispatcher, run once per frame on the task that
/// `func_dryfield_water_tower_8017F128` allocates the state block for: the
/// command in `DryfieldWaterTowerState::field_5C` is switched on and cleared at
/// the end of every path, so each one runs exactly once.
///
/// Commands 1, 3, 6 and 7 each end by handing the prop task at `field_44`
/// (commands 1 and 3) or `field_48` (6 and 7) a 0x7DB record whose `field_2` is
/// the prop's next state -- 2 for the first pair, 3 for the second -- after
/// telling the slot-3 game task (commands 1 and 4) 0x3F3/1 or (3, 4 and 7)
/// 0x3E9. Command 3 sends the two player placements the state's `field_66`
/// picks between: with it 2, `80181AE8` with 0x3E9 and then its 0x18-byte
/// neighbour `80181AD0` with 0x3F2, otherwise 0x3F3 with a null payload; it
/// also raises bit 0x40 of the room's 4A object, as `func_acropolis_fountain_8017DA1C`
/// does for the fountain's. Commands 4 and 2 share their tail: 4 sends 0x3E9
/// (with `80181AD0`) only when `field_66` is 2, then both stash
/// `field_68` in `Mc_SaveData[0].state.at4.loc.view` and raise the session's `viewDirty`, the pair
/// `func_dryfield_water_tower_8017D948` undoes.
///
/// The last three commands start a script-18 pair each -- the cutscene
/// `func_dryfield_water_tower_8017F908` waits on with its 0x5214000C -- into
/// `field_50`; command 8 raises the `field_78` running flag that
/// `func_dryfield_water_tower_8017EB7C` clears and queues two sounds where 9
/// and 10 queue one.
static void func_dryfield_water_tower_8017E93C(Task* arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->work;

    switch (state->field_5C) {
        case 0:
            break;

        case 1: {
            ActorCommand msg;

            Gp_DispatchMsg(state->field_40, 0x3F3, 1, 0);
            msg.command = 2;
            Gp_DispatchMsgPtr(state->field_44, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
            break;
        }

        case 4:
            if (state->field_66 == 2) {
                Gp_DispatchMsgPtr(state->field_40, 0x3E9, &D_dryfield_water_tower_80181AD0[0], 0);
            }
            Gp_DispatchMsg(state->field_40, 0x3F3, 1, 0);

        case 2:
            Mc_SaveData[0].state.at4.loc.view = state->field_68;
            gGameSession->viewDirty           = 1;
            break;

        case 3: {
            ActorCommand msg;

            {
                GpObj4C* object   = &D_dryfield_water_tower_80186A84[20];
                object->field_4A |= 0x40;
            }
            if (state->field_66 == 2) {
                Gp_DispatchMsgPtr(state->field_40, 0x3E9, &D_dryfield_water_tower_80181AD0[1], 0);
                Gp_DispatchMsgPtr(state->field_40, 0x3F2, &D_dryfield_water_tower_80181AD0[1] - 1, 0);
            } else {
                Gp_DispatchMsg(state->field_40, 0x3F3, 0, 0);
            }
            msg.command = 3;
            Gp_DispatchMsgPtr(state->field_44, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
            break;
        }

        case 6: {
            ActorCommand msg;

            msg.command = 2;
            Gp_DispatchMsgPtr(state->field_48, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
            break;
        }

        case 7: {
            ActorCommand msg;

            Gp_DispatchMsgPtr(state->field_40, 0x3E9, &D_dryfield_water_tower_80181AD0[0], 0);
            msg.command = 3;
            Gp_DispatchMsgPtr(state->field_48, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
            break;
        }

        case 8:
            state->field_78 = 1;
            state->field_50 = Gp_SpawnScript18(D_dryfield_water_tower_80187628,
                                               D_dryfield_water_tower_8018763C);
            SndEvt_EnqueueType6(0x52140006, 0, 0);
            SndEvt_EnqueueType6(0x5214000C, 0, 0);
            break;

        case 9:
            state->field_50 = Gp_SpawnScript18(D_dryfield_water_tower_8018764C,
                                               D_dryfield_water_tower_80187660);
            SndEvt_EnqueueType6(0x52140007, 0, 0);
            break;

        case 10:
            state->field_50 = Gp_SpawnScript18(D_dryfield_water_tower_80187670,
                                               &D_dryfield_water_tower_80187678);
            SndEvt_EnqueueType6(0x5214000B, 0, 0);
            break;
    }
    state->field_5C = 0;
}

/// The duration of the rotation step `DryfieldWaterTowerState::field_72` is on,
/// in frames: the same walk of `D_dryfield_water_tower_8018767C` as
/// `func_dryfield_water_tower_8017FB4C`, without that function's low-bit mask.
static inline u16 _dryfieldWaterTowerStepFrames(Task* task)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)task->work;
    u16                      i;

    i = 0;
    if (D_dryfield_water_tower_8018767C[0].field_0 < state->field_72) {
        do {
            i += 1;
        } while (D_dryfield_water_tower_8018767C[i].field_0 < state->field_72);
    }
    return D_dryfield_water_tower_8018767C[i].field_2 * 30;
}

/// State 6 of the cap script, one call per frame: returns 0 while the step is
/// running, otherwise the value the script switches on -- 1 to go back to
/// state 5, 2 to go on to state 7.
///
/// State 0 either starts a rotation (`field_64` 0: message 0x7DA with 9 to the
/// slot-4 game task, view 7 recorded in `field_68`, the `field_78` latch
/// cleared and the first `func_800E8634` pair handed over), or, when the cap
/// has already been placed, restores view 7 and the lowered cap directly and
/// goes to state 2. Either way it clears bit 0x40 of the room's 4A object, loads
/// the current step's duration and restores the first block of each script
/// table pair. State 1 waits for the session's `eventState` to go idle; state 2
/// sends 0x7DA with 1, restarts the frame counter and sets nibble 0x55 to 2.
///
/// State 3 ends the rotation either on a pending 4C record with id 5 and a
/// second byte of 2 (`field_66` then takes that byte) or once the frame counter
/// passes the step's duration (`field_66` 1, the step advanced and the current
/// view kept); until then it plays the running sound at the volume the recorded
/// view's `DwtwViewVolume` entry gives. State 4 waits for `eventState`, sends
/// 0x7DA with 3 unless `field_66` is 2, restores the blocks again, sets nibble
/// 0x55 to 1 and returns `field_66`.
static u16 func_dryfield_water_tower_8017EB7C(Task* arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->work;
    DryfieldWaterTowerState* work;
    GameSession*             session;
    ActorCommand             msg0;
    ActorCommand             msg2;
    ActorCommand             msg4;
    u16                      objId;
    u8                       objA;
    u8                       objB;
    s32                      reason;
    u16                      i;
    s32                      volume;

    switch (state->field_58) {
        case 0:
            if (state->field_64 == 0) {
                msg0.context.loc.stage = gGameSession->at4.loc.stage;
                msg0.context.loc.area  = gGameSession->at4.loc.area;
                msg0.command           = 9;
                Gp_DispatchMsgPtr(gameGetPtrSlot(4), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg0, ACTOR_COMMAND_MESSAGE_APPLY);
                state->field_68 = Gp_FindViewIndex(7);
                state->field_78 = 0;
                func_800E8634(D_dryfield_water_tower_80181C78, 0, D_dryfield_water_tower_80181DC8);
                state->field_58++;
            } else {
                Gp_DispatchMsgPtr(state->field_44, 0x7D4, &D_dryfield_water_tower_80181A40[1], 0);
                Gp_DispatchMsg(state->field_40, 0x3F3, 1, 0);
                Gp_DispatchMsg(state->field_40, 0x3F1, 0, 0);
                Mc_SaveData[0].state.at4.loc.view = Gp_FindViewIndex(7);
                session                           = gGameSession;
                session->viewDirty                = 1;
                session->hideHud                  = 0;
                session->eventState               = 0;
                SndEvt_EnqueueType6(0x52140006, 0, 0x20);
                SndEvt_EnqueueType6(0x5214000C, 0, 0);
                state->field_58 = 2;
            }
            {
                GpObj4C* object   = &D_dryfield_water_tower_80186A84[20];
                object->field_4A &= 0xBF;
            }
            D_dryfield_water_tower_801876AA = _dryfieldWaterTowerStepFrames(arg0);
            Mem_CopyUnaligned(D_dryfield_water_tower_80181BC8, D_dryfield_water_tower_801829B4, 0x40);
            Mem_CopyUnaligned(D_dryfield_water_tower_80181BB8, D_dryfield_water_tower_801828CC, 0x10);
            Mem_CopyUnaligned(D_dryfield_water_tower_80181C48, D_dryfield_water_tower_80182F2C, 0x18);
            return 0;

        case 1:
            if (gGameSession->eventState != 0) {
                return 0;
            }
            state->field_58++;
            break;

        case 2:
            msg2.context.loc.stage = gGameSession->at4.loc.stage;
            msg2.context.loc.area  = gGameSession->at4.loc.area;
            msg2.command           = 1;
            Gp_DispatchMsgPtr(gameGetPtrSlot(4), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg2, ACTOR_COMMAND_MESSAGE_APPLY);
            D_dryfield_water_tower_801876A8 = 0;
            state->field_64                 = 2;
            GameFlag_SetNibble(0x55, 2);
            state->field_58++;

        case 3:
            if (Gp_TakePendingObj4C(&objId, &objA, &objB) != 0 && Gp_StateC08.field_A != 1 && gDisplayState.pendingMode == DISPLAY_MODE_NONE &&
                (objId & 0x7FFF) == 5 && (reason = (s8)objA) == 2) {
                Gp_UnlinkObj4A(0, (D_dryfield_water_tower_80186A84 + 6));
                state->field_68 = Gp_FindViewIndex(9);
                func_800E8634(D_dryfield_water_tower_80181E88, 0, D_dryfield_water_tower_80181FF0);
                state->field_66 = reason;
                state->field_58++;
                break;
            }
            D_dryfield_water_tower_801876A8++;
            if (D_80114CF8 != 0) {
                break;
            }
            if (D_dryfield_water_tower_801876AA < D_dryfield_water_tower_801876A8) {
                state->field_72++;
                state->field_68 = gGameSession->at4.loc.view;
                func_800E8634(D_dryfield_water_tower_80181E88, 0, D_dryfield_water_tower_80181FF0);
                state->field_66 = 1;
                state->field_58++;
            }
            work = (DryfieldWaterTowerState*)arg0->work;
            for (i = 0; D_dryfield_water_tower_80182350[i].field_0 != 0xFFFF; i++) {
                if (D_dryfield_water_tower_80182350[i].field_0 == Gp_FindViewIndex((u8)work->field_74)) {
                    volume = D_dryfield_water_tower_80182350[i].field_2 * 127 / 100;
                    goto play;
                }
            }
            volume = 0x7F;
        play:
            SndEvt_EnqueueTypeB(0x5214000C, volume & 0xFF);
            return 0;

        case 4:
            if (gGameSession->eventState != 0) {
                return 0;
            }
            if (state->field_66 != 2) {
                msg4.context.loc.stage = gGameSession->at4.loc.stage;
                msg4.context.loc.area  = gGameSession->at4.loc.area;
                msg4.command           = 3;
                Gp_DispatchMsgPtr(gameGetPtrSlot(4), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg4, ACTOR_COMMAND_MESSAGE_APPLY);
            }
            Mem_CopyUnaligned(D_dryfield_water_tower_80181C08, D_dryfield_water_tower_801829B4, 0x40);
            Mem_CopyUnaligned(D_dryfield_water_tower_80181BB8, D_dryfield_water_tower_801828CC, 0x10);
            Mem_CopyUnaligned(D_dryfield_water_tower_80181C48, D_dryfield_water_tower_80182F2C, 0x18);
            GameFlag_SetNibble(0x55, 1);
            return state->field_66;
    }
    return 0;
}

/// State 7 of the cap script, one call per frame, returning non-zero once the
/// step is complete. On its first frame (`field_58` 0) it sends message 0x7DA to
/// the slot-4 game task with an `ActorCommand` record naming the current stage and
/// area and carrying 2 as the requested state, the reply message being 0x7DB;
/// after that it waits for the `field_6C` latch.
static inline u16 _dryfieldWaterTowerState7Step(Task* arg0)
{
    DryfieldWaterTowerState* work = (DryfieldWaterTowerState*)arg0->work;
    ActorCommand             msg;

    switch (work->field_58) {
        case 0:
            msg.context.loc.stage = gGameSession->at4.loc.stage;
            msg.context.loc.area  = gGameSession->at4.loc.area;
            msg.command           = 2;
            Gp_DispatchMsgPtr(gameGetPtrSlot(4), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            work->field_58++;
            break;
        case 1:
            if (work->field_6C != 0) {
                return 1;
            }
            break;
        default:
            return 0;
    }
    return 0;
}

/// State 8 of the cap script, one call per frame, returning non-zero once the
/// step is complete. On its first frame (`field_58` 0) it hands the room's two
/// blocks at 0x801820B0 / 0x80182248 to `func_800E8634`, retrying on later
/// frames while `Gp_StateC08.field_A` is 1 or `gDisplayState.pendingMode` is set; after that it waits
/// for the session's `eventState` to go idle and sets nibble 0x32 to 2.
static inline u16 _dryfieldWaterTowerState8Step(Task* arg0)
{
    DryfieldWaterTowerState* work = (DryfieldWaterTowerState*)arg0->work;

    switch (work->field_58) {
        case 0:
            if (Gp_StateC08.field_A == 1) {
                break;
            }
            if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return 0;
            }
            func_800E8634(D_dryfield_water_tower_801820B0, 0, D_dryfield_water_tower_80182248);
            work->field_58++;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                return 0;
            }
            GameFlag_SetNibble(0x32, 2);
            return 1;
        default:
            return 0;
    }
    return 0;
}

/// The cap script, the task entry 0 of `D_dryfield_water_tower_80182384`
/// runs. It does nothing while the session's `field_65` or `Gp_StateC08.field_9` is set
/// or `Player_Status.hp` is zero. State 0 allocates the 0x7C-byte
/// `DryfieldWaterTowerState`, publishes the task and its message table, and
/// restores the room's three pairs of script-table blocks; state 1 spawns
/// entries 1 and 2 of the same table into `field_44` / `field_48` and state 2
/// places them.
///
/// Nibble 0x32 then picks where the script resumes. At 0 it waits (state 3) for
/// a pending 4C record with id 5 (low 15 bits) and a second byte of 1, spawns
/// entry 0 of `D_dryfield_water_tower_8018277C` into `field_4C` and sets the
/// nibble to 1, then waits for that task to end (state 4). At 1 it goes
/// straight to state 5, which waits on the `field_6E` latch. At 2 it clears
/// bit 0x40 of 4A objects 0 and 14, moves the `field_48` prop to
/// `D_dryfield_water_tower_80181A70[2]` and raises its `field_70`, and
/// overwrites the second block of each pair; when nibble 0x55 is also 3 it
/// clears object 3 too, moves the `field_44` prop to its second record,
/// overwrites the first block of each pair and parks in state 9, which does
/// nothing.
///
/// State 6 runs `func_dryfield_water_tower_8017EB7C`, going back to state 5
/// when it returns 1 and on to state 7 when it returns 2. Every frame the
/// script runs, it records the session's view in `field_74` and executes the
/// queued command.
void func_dryfield_water_tower_8017F128(Task* arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->work;
    DryfieldWaterTowerState* work;
    u16                      objId;
    u8                       objA;
    u8                       objB;
    s32                      out;
    s32                      mask;
    GpObj4A*                 p0;
    GpObj4A*                 p3;
    GpObj4A*                 p14;

    if (gGameSession->field_65 != 0 || Gp_StateC08.field_9 != 0 || Player_Status.hp == 0) {
        return;
    }

    switch (arg0->state) {
        case 0:
            work       = Mem_Malloc(0x7C, 0);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x7C);
                work->field_40                  = gameGetPtrSlot(3);
                D_dryfield_water_tower_801876A4 = arg0;
                arg0->msgTable                  = D_dryfield_water_tower_80182374;
            }
            Mem_CopyUnaligned(D_dryfield_water_tower_80181C08, D_dryfield_water_tower_801829B4, 0x40);
            Mem_CopyUnaligned(D_dryfield_water_tower_80181BB8, D_dryfield_water_tower_801828CC, 0x10);
            Mem_CopyUnaligned(D_dryfield_water_tower_80181C48, D_dryfield_water_tower_80182F2C, 0x18);
            Mem_CopyUnaligned(D_dryfield_water_tower_80181B10, D_dryfield_water_tower_801828CC + 2, 0x10);
            Mem_CopyUnaligned(D_dryfield_water_tower_80181BA0, D_dryfield_water_tower_80182F2C + 2, 0x18);
            Mem_CopyUnaligned(D_dryfield_water_tower_80181B60, D_dryfield_water_tower_801829B4 + 8, 0x40);
            state = (DryfieldWaterTowerState*)arg0->work;
            arg0->state++;
            break;

        case 1:
            state->field_44 = Task_SpawnFromTable(D_dryfield_water_tower_80182384, 1, 0, 0);
            state->field_48 = Task_SpawnFromTable(D_dryfield_water_tower_80182384, 2, 0, 0);
            arg0->state++;
            break;

        case 2:
            Gp_DispatchMsgPtr(state->field_44, 0x7D4, D_dryfield_water_tower_80181A40, 0);
            Gp_DispatchMsgPtr(state->field_48, 0x7D4, D_dryfield_water_tower_80181A70, 0);
            state->field_64 = GameFlag_GetNibble(0x55);
            if (GameFlag_GetNibble(0x32) == 0) {
                arg0->state++;
            } else if (GameFlag_GetNibble(0x32) == 1) {
                arg0->state = 5;
            } else if (GameFlag_GetNibble(0x32) == 2) {
                mask           = ~0x40;
                p0             = &(D_dryfield_water_tower_80186A84 + 6)[0];
                p0->field_4A  &= mask;
                p14            = &(D_dryfield_water_tower_80186A84 + 6)[14];
                p14->field_4A &= mask;
                Gp_DispatchMsgPtr(state->field_48, 0x7D4, &D_dryfield_water_tower_80181A70[2], 0);
                ((DryfieldWaterTowerState*)state->field_48->work)->field_70 = 1;
                Mem_CopyUnaligned(D_dryfield_water_tower_80181B10, (D_dryfield_water_tower_801828CC + 2), 0x10);
                Mem_CopyUnaligned(D_dryfield_water_tower_80181BA0, (D_dryfield_water_tower_80182F2C + 2), 0x18);
                Mem_CopyUnaligned(D_dryfield_water_tower_80181B20, (D_dryfield_water_tower_801829B4 + 8), 0x40);
                if (state->field_64 == 3) {
                    p3            = &(D_dryfield_water_tower_80186A84 + 6)[3];
                    p3->field_4A &= mask;
                    Gp_DispatchMsgPtr(state->field_44, 0x7D4, &D_dryfield_water_tower_80181A40[1], 0);
                    Mem_CopyUnaligned(D_dryfield_water_tower_80181BC8, (D_dryfield_water_tower_801829B4 + 8) - 8, 0x40);
                    Mem_CopyUnaligned(D_dryfield_water_tower_80181BB8, (D_dryfield_water_tower_801828CC + 2) - 2, 0x10);
                    Mem_CopyUnaligned(D_dryfield_water_tower_80181C48, (D_dryfield_water_tower_80182F2C + 2) - 2, 0x18);
                    arg0->state = 9;
                }
            }
            break;

        case 3:
            if (Gp_TakePendingObj4C(&objId, &objA, &objB) != 0 && (objId & 0x7FFF) == 5 && (s8)objA == 1) {
                state->field_4C = Task_SpawnFromTable(D_dryfield_water_tower_8018277C, 0, 0, 0);
                GameFlag_SetNibble(0x32, 1);
                arg0->state++;
            }
            break;

        case 4:
            if (Task_PollKill(state->field_4C, &out) != 0) {
                arg0->state++;
            }
            break;

        case 5:
            if (state->field_6E != 0) {
                state->field_6E = 0;
                state->field_58 = 0;
                arg0->state++;
            }
            break;

        case 6:
            switch (func_dryfield_water_tower_8017EB7C(arg0)) {
                case 0:
                    break;
                case 1:
                    state->field_58 = 0;
                    arg0->state--;
                    break;
                case 2:
                    state->field_58 = 0;
                    arg0->state++;
                    break;
            }
            break;

        case 7:
            if (_dryfieldWaterTowerState7Step(arg0)) {
                state->field_58 = 0;
                arg0->state++;
            }
            break;

        case 8:
            if (_dryfieldWaterTowerState8Step(arg0)) {
                state->field_58 = 0;
                arg0->state++;
            }
            break;

        case 9:
            break;
    }
    state->field_74 = gGameSession->at4.loc.view;
    func_dryfield_water_tower_8017E93C(arg0);
}

/// The 0x0D entry of three of the room's script tables -- at 0x80181C94,
/// 0x80181EEC and 0x801820E4, each one word above its `.word 0x0D` opcode. The
/// interpreter runs that opcode as `((void (*)(s32))arg0)(arg1)`, so the
/// script's `arg1` arrives here: 0 on the first two records and 0xA on the one
/// at 0x801820E4.
///
/// Republishes the player's weapon to slot 3 (msg 0x3E8) the way
/// `func_actor_136100_8013467C` does -- `Player_Status.weapon` picked through
/// `Mc_SaveData[0].state.characterId` is the record's `field_0` -- but fills the two halfword slots
/// from that script argument: `field_8` is its "non-zero" flag and `field_C`
/// the halfword itself.
void func_dryfield_water_tower_8017F700(s32 arg0)
{
    AnimationPlayRequest rec;
    s32                  weaponId;
    s32                  id;
    s32                  value;

    weaponId                 = Player_Status.weapon;
    id                       = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    value                    = arg0 & 0xFFFF;
    rec.source.index         = id;
    rec.animationId          = 1;
    rec.blend                = value != 0;
    rec.blendFrames          = value;
    rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, &rec, 0);
}

/// The room's handler for message 0x7D4: copies `placement` onto the task's
/// `TmdObject` coordinate frame -- the three longs become the translation,
/// then yaw, pitch and roll are applied with `Gfx_RotMatrixY` / `X` / `Z` --
/// and marks the coordinate dirty.
void func_dryfield_water_tower_8017F77C(Task* task, s32 arg1, ActorTransform* placement)
{
    GfxCoord* coord;
    MATRIX*   mtx;

    coord             = task->extra.tmd->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Message 0x7DB handler, the room script table `D_dryfield_water_tower_80181B00`
/// lists beside its 0x7D4 entry `func_dryfield_water_tower_8017F77C`. It rests the cap script: the
/// three halfword slots it keeps its timers in are cleared along with the
/// task's kill countdown, and the payload's halfword becomes the task's state,
/// so the 0x7DB sender picks the state the cap script resumes in. The opcode
/// itself is never read, hence the named-but-unused `msgId`.
void func_dryfield_water_tower_8017F808(Task* task, s32 msgId, ActorCommand* msg)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)task->work;

    state->field_58     = 0;
    state->field_60     = 0;
    state->field_5A     = 0;
    task->state         = msg->command;
    task->killCountdown = 0;
}

/// The 0x0D entry of two of the room's script tables -- `D_..._801820B0` and
/// `D_..._80182248`, at 0x801821EC and 0x801822DC, each one word below its
/// `.word 0x0D` opcode (0x801821E8 / 0x801822D8). The opcode is the room's
/// per-record handler slot, and the other records filling it are
/// `func_dryfield_water_tower_8017F908` on `D_..._80181DC8` and
/// `func_dryfield_water_tower_8017FA5C` on the 0x8018227C record.
///
/// One-shot, latched by `DryfieldWaterTowerState::field_76`: the first call
/// raises bit 0x80 of `gGameSession->flowFlags` and drops bit 0x40, releases one
/// ref of the slot-4 game object, and sets the latch.
/// `func_shelter_b3_dumping_hole_801818E0` runs the same latch / release /
/// `|= 0x80` sequence for its room.
void func_dryfield_water_tower_8017F82C(void)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)D_dryfield_water_tower_801876A4->work;

    if (state->field_76 == 0) {
        gGameSession->flowFlags |= 0x80;
        gGameSession->flowFlags &= 0xBF;
        Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 1);
        state->field_76 = 1;
    }
}

/// Record handler (opcode 0x0D) of two of the room's script tables: calls
/// `Gp_PulseState1C` and raises bit 0 of `Gp_StateC08.field_6`.
void func_dryfield_water_tower_8017F8B0(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}

void func_dryfield_water_tower_8017F8E8(s16 arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)D_dryfield_water_tower_801876A4->work;

    state->field_5C = arg0;
    state->field_5E = 0;
}

/// The 0x0D entry of the room's other script table,
/// `D_dryfield_water_tower_80181DC8` -- the word at 0x80181DF8 is the opcode
/// and 0x80181DFC this function's address. It is the sibling of
/// `func_dryfield_water_tower_8017FA5C` on `D_dryfield_water_tower_80182248`
/// and differs in the view it selects: 7 here against that one's 9.
///
/// It plays event 0x5214000C unless the latch `DryfieldWaterTowerState::field_78`
/// says the view has already been announced, records the view in the saved
/// location byte `Mc_SaveData[0].state.at4.loc.view`, sends its 0x7D4 placement
/// `80181A58` to the prop task at `field_44` and restarts that task on state 1,
/// then stops the pad scripts and queues event 0x52140006. The latch is what
/// separates it from that sibling: this one is the re-entry the 0x5214000C
/// announcement is gated on, and `func_dryfield_water_tower_8017EB7C` clears
/// the latch when it re-arms the room.
void func_dryfield_water_tower_8017F908(void)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)D_dryfield_water_tower_801876A4->work;

    if (state->field_78 == 0) {
        SndEvt_EnqueueType6(0x5214000C, 0, 0);
    }
    Mc_SaveData[0].state.at4.loc.view = Gp_FindViewIndex(7);
    gGameSession->viewDirty           = 1;
    Gp_DispatchMsgPtr(state->field_44, 0x7D4, &D_dryfield_water_tower_80181A40[1], 0);
    state->field_44->state = 1;
    Gp_HaltPadScripts();
    SndEvt_EnqueueType7(0x52140006, 0x1E);
}

/// The third of the room's 0x0D script handlers in this run, between
/// `func_dryfield_water_tower_8017F908` and `func_dryfield_water_tower_8017FA5C`
/// and the one that does not recompute the view: it records the view
/// `DryfieldWaterTowerState::field_68` in the saved location, starts the prop
/// task at `field_44` on state 1 with its 0x7D4 placement
/// `80181A40` and stops the pad scripts, the same three-step restart the other
/// two perform on their own prop tasks.
///
/// It plays both event ids of that restart -- 0x52140007 and 0x5214000C -- and
/// is the only one of the three that moves the player: the 0x3E9 placement
/// `80181AD0` `func_dryfield_water_tower_8017FA5C` sends unconditionally goes
/// to the slot-3 game task at `field_40` here, but only while `field_66` reads
/// 2, the room's "the cap is following" state.
void func_dryfield_water_tower_8017F9AC(void)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)D_dryfield_water_tower_801876A4->work;

    Mc_SaveData[0].state.at4.loc.view = state->field_68;
    gGameSession->viewDirty           = 1;
    Gp_DispatchMsgPtr(state->field_44, 0x7D4, &D_dryfield_water_tower_80181A40, 0);
    state->field_44->state = 1;
    Gp_HaltPadScripts();
    SndEvt_EnqueueType7(0x52140007, 0xA);
    SndEvt_EnqueueType7(0x5214000C, 0xA);
    if (state->field_66 == 2) {
        Gp_DispatchMsgPtr(state->field_40, 0x3E9, &D_dryfield_water_tower_80181AD0[0], 0);
    }
}

/// Script opcode 0x0D of the room's command table `D_dryfield_water_tower_80182248`:
/// it hands the stream to view 9, restarts the prop task at `field_48` on state 1
/// and gives it its 0x7D4 placement, moves the player to `80181AD0`, stops the pad
/// scripts, plays event 0x5214000B and installs three of the room's effect tables.
void func_dryfield_water_tower_8017FA5C(void)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)D_dryfield_water_tower_801876A4->work;

    Mc_SaveData[0].state.at4.loc.view = Gp_FindViewIndex(9);
    Gp_DispatchMsgPtr(state->field_48, 0x7D4, &D_dryfield_water_tower_80181A70[2], 0);
    state->field_48->state = 1;
    Gp_DispatchMsgPtr(state->field_40, 0x3E9, &D_dryfield_water_tower_80181AD0[0], 0);
    Gp_HaltPadScripts();
    SndEvt_EnqueueType7(0x5214000B, 0xA);
    Mem_CopyUnaligned(D_dryfield_water_tower_80181B10, (D_dryfield_water_tower_801828CC + 2), 0x10);
    Mem_CopyUnaligned(D_dryfield_water_tower_80181BA0, (D_dryfield_water_tower_80182F2C + 2), 0x18);
    Mem_CopyUnaligned(D_dryfield_water_tower_80181B20, (D_dryfield_water_tower_801829B4 + 8), 0x40);
    ((DryfieldWaterTowerState*)state->field_48->work)->field_70 = 1;
}

/// The `DwtwStep` the rotation's step counter is on: the last entry of
/// `D_dryfield_water_tower_8018767C` whose threshold is below
/// `DryfieldWaterTowerState::field_72`, walked from the second entry -- a
/// counter of 0 fails the entry-0 test and takes the first entry without
/// walking -- and capped by the `0xFFFF` terminator. The step's duration comes
/// back 30-fold with its low bit cleared.
static s32 func_dryfield_water_tower_8017FB4C(Task* task)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)task->work;
    u16                      i;

    i = 0;
    if (D_dryfield_water_tower_8018767C[0].field_0 < state->field_72) {
        do {
            i += 1;
        } while (D_dryfield_water_tower_8018767C[i].field_0 < state->field_72);
    }
    return (D_dryfield_water_tower_8018767C[i].field_2 * 30) & 0xFFFE;
}

void func_dryfield_water_tower_8017FBC8(Task* task)
{
    ((DryfieldWaterTowerState*)task->work)->field_6C = 1;
}

void func_dryfield_water_tower_8017FBD8(Task* task)
{
    ((DryfieldWaterTowerState*)task->work)->field_6E = 1;
}

/// The room's per-frame body, run by `func_dryfield_water_tower_8017FD64`
/// after its state machine has stepped the task on. It dispatches on the
/// `DwtwWork::field_C` the room's script writes through
/// `func_dryfield_water_tower_80180174` and clears it again on every path, so
/// each command runs for the single frame the latch holds.
///
/// State 1 installs the room's machinery: animation 0x0D to the prop task at
/// `field_4`, the two 0x7D4 placements (base and base+0x48) to `field_4` and
/// `field_8`, animation 0x0E to `field_8`, the slot-3 command 0x3F3 with its
/// `2`, and the fade-up. The cap script sends that same command as `1`
/// (`func_dryfield_water_tower_8017E93C` commands 1 and 4) -- the value state 4
/// below pairs with the 0x3E9 player move, as that script's command 4 does.
/// States 2, 5 and 6 are the single messages the props receive when the script
/// moves them: animation 0x0F with case 2's placement, and animations 0x0D and
/// 0x0E to `field_4` and `field_8` on their own. States 0 and 3 are the idle
/// ones -- every path, including theirs, clears the latch.
///
/// The halfword is read unsigned, so the state arrives as `lhu`; state 0 is a
/// real case, which is why the switch spans 0..6 and indexes its jump table
/// with the state itself rather than with `state - 1`.
static void func_dryfield_water_tower_8017FBE8(Task* task)
{
    DwtwWork* work  = (DwtwWork*)task->work;
    u16       state = work->field_C;

    switch (state) {
        case 0:
            break;
        case 1:
            Gp_DispatchMsgPtr(work->field_4, 0x7D3, &D_dryfield_water_tower_80182420[0], 0);
            Gp_DispatchMsgPtr(work->field_4, 0x7D4, &D_dryfield_water_tower_801823C0[0], 0);
            Gp_DispatchMsgPtr(work->field_8, 0x7D4, &D_dryfield_water_tower_801823C0[3], 0);
            Gp_DispatchMsgPtr(work->field_8, 0x7D3, &D_dryfield_water_tower_80182420[1], 0);
            Gp_DispatchMsg(work->field_0, 0x3F3, 2, 0);
            Task_SpawnFromTable(D_dryfield_water_tower_8018277C, 2, 8, 0);
            break;
        case 2:
            Gp_DispatchMsgPtr(work->field_4, 0x7D4, &D_dryfield_water_tower_801823C0[2], 0);
            Gp_DispatchMsgPtr(work->field_4, 0x7D3, (D_dryfield_water_tower_80182420 + 2), 0);
            break;
        case 4:
            Gp_DispatchMsg(work->field_0, 0x3F3, 1, 0);
            Gp_DispatchMsgPtr(work->field_0, 0x3E9, &D_dryfield_water_tower_801823A8, 0);
            break;
        case 5:
            Gp_DispatchMsgPtr(work->field_4, 0x7D3, &D_dryfield_water_tower_80182420[0], 0);
            break;
        case 6:
            Gp_DispatchMsgPtr(work->field_8, 0x7D3, (D_dryfield_water_tower_80182420 + 1), 0);
            break;
    }
    work->field_C = 0;
}

/// Room entry point: install the player's weapon animation set on slot 3
/// (message 0x3E8) unless `Gp_StateC08.field_A` says a battle is running or
/// `gDisplayState.pendingMode` says one has just ended, then allocate the `DwtwWork` the room
/// task hangs off `Task::work` (killing the task if the allocation fails),
/// zero it, park the slot-3 task in `field_0` and the room task itself in
/// `D_dryfield_water_tower_801876AC`, and resolve `field_4` / `field_8` from
/// the session id: the base id, then the id with the 0x1000 index of
/// `Gp_FindWorkById`'s search key.
///
/// State 1 starts the room's cutscene pair and state 2 kills the task once the
/// scene is over, exactly as the actors' `func_actor_560800_80135D54` pairs
/// them; the task runs only while the session is not paused
/// (`GameSession::field_65`) and no cutscene is active (`Gp_StateC08.field_9`,
/// a signed byte), and every path that is not a kill ends in the room's
/// per-frame body `func_dryfield_water_tower_8017FBE8`.
void func_dryfield_water_tower_8017FD64(Task* task)
{
    AnimationPlayRequest msg;
    DwtwWork*            work;
    s32                  id;
    s32                  weaponId;
    s32                  anim;

    if (gGameSession->field_65 != 0) {
        return;
    }
    if ((s8)Gp_StateC08.field_9 != 0) {
        return;
    }
    switch (task->state) {
        case 0:
            if (Gp_StateC08.field_A == 1 || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            weaponId                 = Player_Status.weapon;
            anim                     = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.source.index         = anim;
            msg.animationId          = 1;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 0xA;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, &msg, 0);
            work       = (DwtwWork*)Mem_Malloc(0x18, 0);
            task->work = work;
            if (work == NULL) {
                taskKill(task);
            } else {
                Mem_Set(work, 0, 0x18);
                work->field_0                   = gameGetPtrSlot(3);
                D_dryfield_water_tower_801876AC = task;
                id                              = gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8);
                work->field_4                   = Gp_FindWorkById(id)->field_0;
                id                              = ((gGameSession->at4.loc.stage << 8) | 0x1000) | gGameSession->at4.loc.area;
                work->field_8                   = Gp_FindWorkById(id)->field_0;
            }
            task->state++;
            break;
        case 1:
            func_800E8634(D_dryfield_water_tower_80182464, 0, D_dryfield_water_tower_80182674);
            task->state++;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(task, 0);
                return;
            }
            break;
    }
    func_dryfield_water_tower_8017FBE8(task);
}

/// The room's fade-in task, entry 2 of `D_dryfield_water_tower_8018277C`: the
/// reverse of the fade-out `func_dryfield_water_tower_80180038`. State 0
/// allocates the `OverlayFadeWork` block into `Task::work` and saturates its
/// three channels at 0xFF; a failed allocation kills the task. Every state-1
/// frame draws the overlay tinted `r`/`g`/`r` with `Fade_DrawOverlay` and
/// lowers each channel by `Task::spawnArg1`, the fade rate; once `r` falls
/// below zero the task kills itself.
void func_dryfield_water_tower_8017FF5C(Task* arg0)
{
    OverlayFadeWork* work;
    OverlayFadeWork* alloc;

    work = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0xFF;
            work->g      = 0xFF;
            work->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r -= (u16)arg0->spawnArg1.value;
            work->g -= (u16)arg0->spawnArg1.value;
            work->b -= (u16)arg0->spawnArg1.value;
            if ((s16)work->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

/// The room's fade-out task: state 0 allocates the 8-byte `OverlayFadeWork` block
/// into `Task::work` and clears its three channels, and every state-1 frame
/// draws them with `Fade_DrawOverlay` and raises each by `Task::spawnArg1`, the
/// fade rate. The red channel is the one watched: once it passes 0x100 the fade
/// has run its course and the task kills itself. The task is the second
/// descriptor of `D_dryfield_water_tower_8018277C`, the table whose entry 0 is
/// the room script task. The table's entry 2, `func_dryfield_water_tower_8017FF5C`,
/// runs the same body backwards, from saturated channels falling past zero.
void func_dryfield_water_tower_80180038(Task* arg0)
{
    OverlayFadeWork* work;
    OverlayFadeWork* alloc;

    work = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0;
            work->g      = 0;
            work->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r += (u16)arg0->spawnArg1.value;
            work->g += (u16)arg0->spawnArg1.value;
            work->b += (u16)arg0->spawnArg1.value;
            if ((s16)work->r >= 0x100) {
                taskKill(arg0);
            }
            break;
    }
}

/// Record handler (opcode 0x0D) of one of the room's script tables: queues
/// CD command 0x82.
void func_dryfield_water_tower_80180114(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Record handler (opcode 0x0D) of one of the room's script tables: queues
/// CD command 0x81.
void func_dryfield_water_tower_80180134(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Record handler (opcode 0x0D) of one of the room's script tables: calls
/// `Gp_RestoreStreamRng`.
void func_dryfield_water_tower_80180154(void)
{
    Gp_RestoreStreamRng();
}

void func_dryfield_water_tower_80180174(s16 arg0)
{
    DwtwWork* work = (DwtwWork*)D_dryfield_water_tower_801876AC->work;

    work->field_C = arg0;
    work->field_E = 0;
}

/// Armed once per room: hands the slot-4 task the session's two id bytes as
/// message 0x7DA's payload, then sets `field_14` so the message goes out only
/// the first time. The halfword it zeroes is the state the 0x7DB handler reads.
void func_dryfield_water_tower_80180194(void)
{
    DwtwWork*    work = (DwtwWork*)D_dryfield_water_tower_801876AC->work;
    ActorCommand msg;

    if (work->field_14 == 0) {
        Gp_ArmStateF0(1);
        msg.context.loc.stage = gGameSession->at4.loc.stage;
        msg.context.loc.area  = gGameSession->at4.loc.area;
        msg.command           = 0;
        Gp_DispatchMsgPtr(gameGetPtrSlot(4), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
        work->field_14 = 1;
    }
}

/// Places the room's two prop tasks and the slot-3 game task: the first two
/// 0x7D4 placements go to `field_8` / `field_4`, then `field_0` gets the 0x3F3
/// (1) and 0x3E9 commands that move the player to `D_..._801823A8`. The area
/// record takes view 4 and the session is dropped back to state 1 before the
/// stream RNG is restored.
void func_dryfield_water_tower_80180220(void)
{
    DwtwWork* work = (DwtwWork*)D_dryfield_water_tower_801876AC->work;

    Gp_DispatchMsgPtr(work->field_8, 0x7D4, &(D_dryfield_water_tower_801823C0 + 1)[0], 0);
    Gp_DispatchMsgPtr(work->field_4, 0x7D4, &(D_dryfield_water_tower_801823C0 + 1)[1], 0);
    Gp_DispatchMsg(work->field_0, 0x3F3, 1, 0);
    Gp_DispatchMsgPtr(work->field_0, 0x3E9, &D_dryfield_water_tower_801823A8, 0);
    Mc_SaveData[0].state.at4.loc.view = Gp_FindViewIndex(4);
    gGameSession->viewDirty           = 1;
    CdCmd_CancelReplaceAndActivate();
    Gp_RestoreStreamRng();
}

/// Hides or shows one of the area's sprite commands from the nibble-0x55 band
/// by setting its `SpriteBatch::hidden`: the argument's low byte zero keeps the
/// command's sprites out of the ordering table, non-zero draws them. Only stage
/// 2 has a record to write.
void func_dryfield_water_tower_801802D8(u8 arg0)
{
    GameLocationKey* sess;
    SpriteBatch*     batches;

    sess = &gGameSession->at4.loc;
    if (sess->stage == 2) {
        batches = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1][18].field_4;
        if (!(arg0 & 0xFF)) {
            batches[1].hidden = 1;
            return;
        }
        batches[1].hidden = 0;
    }
}

void func_dryfield_water_tower_80180348(Task* unused)
{
    Gp_State1C->roomEffectMode = D_dryfield_water_tower_801827A0[(Gp_GetViewIndex() & 0xFF) - 1];
}
