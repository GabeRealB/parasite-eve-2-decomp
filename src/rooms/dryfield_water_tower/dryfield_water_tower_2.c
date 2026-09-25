#include "common.h"

#include <psyq/libgte.h>

#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "rooms/dryfield_water_tower.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

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

/// Scratch the raise-prop body `func_dryfield_water_tower_8017E1DC` stages its
/// two vectors in. The spawn tick's `vec` -- the model translation it hands to
/// `func_800D7A9C` -- is overwritten before the cap is drawn, so it and the
/// `rot` the floor quad takes never overlap and the two share one frame slot.
typedef union DwtwVec {
    /* 0x0 */ VECTOR  vec; // cap translation, handed to func_800D7A9C
    /* 0x0 */ SVECTOR rot; // floor-quad offset, handed to Gp_DrawFloorQuad
} DwtwVec;

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
    /* 0x00 */ u8    pad_0[0x40];
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

/// The water tower's script task, published by its state-0 init
/// `func_dryfield_water_tower_8017FD64`. `DwtwWork` hangs off its `work`.
extern Task* D_dryfield_water_tower_801876AC;

/// The water tower's cap-script task, published by
/// `func_dryfield_water_tower_8017F128`, which allocates the cap script's
/// 0x7C-byte scratch block into the task's `work` first.
extern Task* D_dryfield_water_tower_801876A4;

/// `Mc_SaveData.at4.loc.view`, the saved location's view byte, spelled by
/// address because that is the name the room imports. Two stores below name the
/// field instead, where its struct spelling is needed for aliasing.
extern u8 D_8007216C;

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on and
/// `D_8007218A` picks which of the two weapon-id bases that record uses; the
/// alternate block is indexed by `D_80073BA9` plus 1 against the base block's
/// plus 0x22.
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// The two scalars the cap-arrival test below reads out of the room's data.
/// Both are reached through their own symbol but as record members rather than
/// as bare scalars, and that shape is load-bearing: `sched_analyze` orders a
/// store ahead of a later load only when the load is not a plain scalar opposed
/// to a struct-member store (`true_dependence`'s aliasing test), and both blocks
/// of the target's body depend on that order. `D_..._80181AA4` is `pos.vy` of
/// the placement record at 0x80181AA0 and `D_80070F6C` is the flag word the
/// case-2 `t[2]` offset is keyed on; each is one element of its own run here.
extern s32 D_dryfield_water_tower_80181AA4[];
extern s32 D_80070F6C[];

/// The payloads of the cap-arrival test: the 0x7D4 records it publishes to
/// itself. `80181AA0` is the placement record whose `pos.vy` is compared above,
/// and the one `func_dryfield_water_tower_8017FA5C` sends to the prop task at
/// `DryfieldWaterTowerState::field_48`; splat splits it into three chunks, the
/// other two (`80181AA4` / `80181AA8`) being the rest of its 0x18 bytes.
/// `80181AB8` is the record case 0 sends.
extern GpXformArg D_dryfield_water_tower_80181AA0;
extern s32        D_dryfield_water_tower_80181AA8;
extern u8         D_dryfield_water_tower_80181AB8;

/// The three effect-definition tables the cap-arrival test and
/// `func_dryfield_water_tower_8017FA5C` install into the room's live copies --
/// 0x10, 0x18 and 0x40 bytes. The same three pairs, at the same sizes, are
/// copied by `func_dryfield_water_tower_8017F128` on its state-7 path, so each
/// is one variant of a table the room selects between rather than the only
/// content. The destinations are also reached with an offset from a lower base
/// (`0x801828CC + 0x10` is `801828DC`), i.e. the live table is a region the
/// room walks. The record types are not pinned yet, so the blobs stay bytes.
extern u8 D_dryfield_water_tower_80181B10[];
extern u8 D_dryfield_water_tower_801828DC[];
extern u8 D_dryfield_water_tower_80181BA0[];
extern u8 D_dryfield_water_tower_80182F44[];
extern u8 D_dryfield_water_tower_80181B20[];
extern u8 D_dryfield_water_tower_801829F4[];

/// The room's two cap placements with message 0x7D4, the pair the cap props
/// publish to themselves through `func_dryfield_water_tower_8017F77C`: `[0]` is the raised position
/// (Y = -0x1F40, the one a task spawned with a non-zero `spawnArg1` publishes)
/// and `[1]` the lowered one (Y = -0xFA0) the lowering prop stops at. They are
/// the two 0x18-byte records above them in the same run --
/// `func_dryfield_water_tower_8017F908` sends `-0x20` and `8017F9AC` `-0x38` --
/// so the split names only the first.
extern GpXformArg D_dryfield_water_tower_80181A70[];

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
extern GpXformArg D_dryfield_water_tower_80181A40[1];

/// The run's second record, at 0x80181A58: the lowered position
/// `func_dryfield_water_tower_8017E428` sinks the cap to (`pos.vy`), tests the
/// cap's Z against (`pos.vz`) and pulls the cap's X to (`pos.vx`), and the
/// 0x7D4 placement that same function publishes to itself once its state 1
/// counter runs out. `func_dryfield_water_tower_8017F908` sends it to the prop
/// task at `DryfieldWaterTowerState::field_44` with the same message.
extern GpXformArg D_dryfield_water_tower_80181A58;

/// The effect offsets `func_dryfield_water_tower_8017E5B0` spawns 0x60054
/// with, at 0x80181C60: twelve halfwords, indexed by the 0..9 `killCountdown`
/// counter the same block wraps, so only the first ten -- 0x190, 0x3E8, 0xFE70,
/// 0xFF38, 0, 0xFDA8, 0xC8, 0x320, 0xFC18 and 0xFCE0 -- are ever read.
extern u16 D_dryfield_water_tower_80181C60[];

/// The room's 4A object -- the list node `Gp_LinkObj4A` chains into
/// `Gp_Obj4ALists` and `Gp_UnlinkObj4A` takes out again. Command 3 of
/// `func_dryfield_water_tower_8017E93C` raises its `field_4A` bit 0x40, the
/// same bit `func_acropolis_fountain_8017DA1C` raises on the fountain's node.
extern GpObj4A D_dryfield_water_tower_80187074;

/// The two player placements the cap script's commands 3, 4 and 7 dispatch as
/// the payload of message 0x3E9 (their handler is the slot-3 game task at
/// `field_40`): `80181AD0` from commands 4 and 7, and `80181AE8` from command
/// 3, which sends `80181AD0` -- the 0x18-byte record one step below it in the
/// same run -- with 0x3F2 straight after. `func_dryfield_water_tower_8017F9AC`
/// and `8017FA5C` send `80181AD0` with the same message.
extern GpXformArg D_dryfield_water_tower_80181AD0;
extern GpXformArg D_dryfield_water_tower_80181AE8;

/// The three `Gp_SpawnScript18` pairs the cap script's last three commands
/// spawn into `field_50`, and the sound each one queues: `0x52140006` with the
/// extra `0x5214000C` for command 8, whose script `func_dryfield_water_tower_8017EB7C`
/// and `func_dryfield_water_tower_8017F908` wait on through `field_78`.
extern s32 D_dryfield_water_tower_80187628;
extern s32 D_dryfield_water_tower_8018763C;
extern s32 D_dryfield_water_tower_8018764C;
extern s32 D_dryfield_water_tower_80187660;
extern s32 D_dryfield_water_tower_80187670;
extern s32 D_dryfield_water_tower_80187678;

/// Main-executable byte at 0x80114C11, read signed (`lb`), with no module
/// header yet: the raise prop `func_dryfield_water_tower_8017E1DC` runs its
/// state machine only while it is zero, so it is the room's "leave the cap
/// alone" gate -- the cap stops moving the moment it goes non-zero.
extern s8 D_80114C11;

/// The room's script table, the `Task::msgTable` block `Gp_DispatchMsg`
/// reads: the raise prop `func_dryfield_water_tower_8017E1DC` hangs it off its
/// own task in state 0, the same slot the cap script publishes a table into.
extern u32 D_dryfield_water_tower_80181B00;

/// Main-executable gates the cap script checks, with no module header yet:
/// the script only runs while `D_80073BA0` is non-zero, and its state 8 holds
/// back on `D_80114C12` == 1 or a non-zero `D_80071075`.
extern s16 D_80073BA0;
extern s8  D_80114C12;
extern u8  D_80071075;

/// The raised-cap sources the cap script restores the room's script-table
/// blocks from, and the lowered-cap ones it uses when the cap is already down.
/// The destinations are runs of two blocks each (0x10, 0x18 and 0x40 bytes),
/// reached by their first symbol plus the block size or by the second symbol
/// minus it.
extern u8 D_dryfield_water_tower_80181B60;
extern u8 D_dryfield_water_tower_80181BB8;
extern u8 D_dryfield_water_tower_80181BC8;
extern u8 D_dryfield_water_tower_80181C08;
extern u8 D_dryfield_water_tower_80181C48;
extern u8 D_dryfield_water_tower_801828CC[];
extern u8 D_dryfield_water_tower_801829B4[];
extern u8 D_dryfield_water_tower_80182F2C[];

/// The pair of blocks the cap script's state 8 hands to `func_800E8634`.
extern u8 D_dryfield_water_tower_801820B0;
extern u8 D_dryfield_water_tower_80182248;

/// The two `func_800E8634` pairs the rotation step hands over: `80181C78` /
/// `80181DC8` when it starts a rotation from view 7, `80181E88` / `80181FF0`
/// when it ends one.
extern u8 D_dryfield_water_tower_80181C78;
extern u8 D_dryfield_water_tower_80181DC8;
extern u8 D_dryfield_water_tower_80181E88;
extern u8 D_dryfield_water_tower_80181FF0;

/// The room's rotation schedule (see `DwtwStep`) and the per-view volume table
/// its running sound is scaled by.
extern DwtwStep       D_dryfield_water_tower_8018767C[];
extern DwtwViewVolume D_dryfield_water_tower_80182350[];

/// The rotation's frame counter and the limit it is compared against, the
/// current step's duration in frames.
extern u16 D_dryfield_water_tower_801876A8;
extern u16 D_dryfield_water_tower_801876AA;

/// Main-executable byte with no module header yet; while it is non-zero the
/// rotation step neither times out nor updates its running sound.
extern u8 D_80114CF8;

/// The cap script's message table, published into its own `Task::msgTable`.
extern s32 D_dryfield_water_tower_80182374;

/// The room script's task table: entry 0 is the room task
/// `func_dryfield_water_tower_8017FD64` itself, which the cap script spawns in
/// its state 3, entry 1 the fade-out task `func_dryfield_water_tower_80180038`
/// and entry 2 the fade-in task `func_dryfield_water_tower_8017FF5C`, which the
/// room task's state 1 starts with the fade rate 8.
extern TaskDesc D_dryfield_water_tower_8018277C[];

/// The room's run of 4A objects; element 14 is `D_dryfield_water_tower_80187074`.
extern GpObj4A D_dryfield_water_tower_80186C4C[];

/// The `GpXformArg` run the room's 0x7D4 messages step the props through:
/// the 0x18-byte records from 0x801823A8 up to 0x80182408. `D_..._801823C0`,
/// the second of them, is case 1's pair -- `[0]` to `field_4` and `[3]`
/// (0x80182408) to `field_8`; `D_..._801823F0`, the run's element 2, is case
/// 2's, and the same record the script opcode
/// `func_dryfield_water_tower_80180220` sends as element 1 of the pair it
/// declares `D_..._801823D8[]`; and `D_..._801823A8`, the first, is the player
/// move both that opcode and case 4 send with 0x3E9.
extern GpXformArg D_dryfield_water_tower_801823C0[];
extern GpXformArg D_dryfield_water_tower_801823F0;
extern GpXformArg D_dryfield_water_tower_801823A8;

/// The `GpAnimArg` (0x14-byte) run the 0x7D3 animation messages send: `field_4`
/// carries the animation index -- 0x0D / 0x0E / 0x0F for the three records --
/// and every other field, `field_8` included, is zero. Element 1 and element 2
/// are also named by address, and the code below uses both spellings: element 1
/// is `D_..._80182420[1]` in case 1 and `D_..._80182434` in case 6.
extern GpAnimArg D_dryfield_water_tower_80182420[];
extern GpAnimArg D_dryfield_water_tower_80182434[];
extern GpAnimArg D_dryfield_water_tower_80182448[];

/// The pair of placements `func_dryfield_water_tower_80180220` sends with
/// message 0x7D4, one to each of `field_8` and `field_4`; the second is the
/// element at 0x18, so the run is declared as an array. Both are payloads of
/// `func_dryfield_water_tower_8017F77C`, the handler the room's script table
/// pairs with 0x7D4.
extern GpXformArg D_dryfield_water_tower_801823D8[];

/// The pair of cutscene blocks `func_800E8634` hands to `Task_Spawn` (bank 9,
/// type 7): the one the running scene starts and the one it parks in
/// `D_801156D0` for the task that follows it.
extern s32 D_dryfield_water_tower_80182464;
extern s32 D_dryfield_water_tower_80182674;

/// The per-view `roomEffectMode` table `func_dryfield_water_tower_80180348`
/// reads, indexed by the 1-based view.
extern u16 D_dryfield_water_tower_801827A0[];

u16 func_dryfield_water_tower_8017EB7C(Task* arg0);
s32 func_dryfield_water_tower_8017DFAC(Task* arg0);

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
void func_dryfield_water_tower_8017DE30(Task* arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->work;
    GsCOORDINATE2*           coord = ((TmdObject*)arg0->extra)->coords;
    RoomActorMsg             msg;

    if (arg0->spawnArg1 == 0) {
        switch (state->field_58) {
            case 0:
                state->field_48 = Task_SpawnFromTable(D_dryfield_water_tower_80182384, 2, 1, 0);
                state->field_58++;
                return;

            case 1:
                msg.command = 2;
                Gp_DispatchMsg(state->field_48, 0x7DB, (s32)&msg, 0);
                state->field_58++;
                /* fallthrough */

            case 2:
                if (coord->coord.t[1] > D_dryfield_water_tower_80181A70[1].pos.vy) {
                    Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_dryfield_water_tower_80181A70[1], 0);
                    state->field_58++;
                    return;
                }
                break;

            case 3:
                return;
        }
    } else if (state->field_58 == 0) {
        Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_dryfield_water_tower_80181A70[0], 0);
        state->field_58++;
    }

    state->field_6A   += 4;
    coord->coord.t[1] += (s16)state->field_6A;
    coord->flg         = 0;
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
/// nudged by 0xA while the `D_80070F6C` flag bit 2 is raised. Only the states
/// that arrive there reach the shared 0x7D4 tail, and only state 2 reports
/// arrival -- which is why `func_dryfield_water_tower_8017E1DC` masks the result
/// with 0xFFFF to test it.
///
/// Two shapes here are the original's rather than stylistic. The unaligned copies
/// go to the low addresses and the room's data is the source, matching the
/// three `Mem_CopyUnaligned` argument pairs the target shows; and the `pos`
/// scratch is filled `vz`, `vy`, `vx` -- the reverse of its declaration order --
/// which is the store order the target's frame keeps.
s32 func_dryfield_water_tower_8017DFAC(Task* arg0)
{
    DryfieldWaterTowerState* state;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           effCoord;
    SVECTOR                  pos;
    s32                      i;

    state      = (DryfieldWaterTowerState*)arg0->work;
    coord      = ((TmdObject*)arg0->extra)->coords;
    coord->flg = 0;
    switch (state->field_58) {
        case 0:
            state->field_70 = 1;
            if (state->field_48 != NULL) {
                taskKill(state->field_48);
                state->field_48 = NULL;
            }
            Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_dryfield_water_tower_80181AB8, 0);
            state->field_58++;
            break;

        case 1:
            coord->coord.t[1] += 0x12C;
            if (D_dryfield_water_tower_80181AA4[0] < coord->coord.t[1]) {
                SndEvt_EnqueueType7(0x5214000B, 0);
                SndEvt_EnqueueType6(0x52140010, 0, 0);
                effCoord = ((TmdObject*)arg0->extra)->coords;
                pos.vz   = 0;
                pos.vy   = 0;
                pos.vx   = -0xC8;
                i        = 0;
                do {
                    Gp_SpawnEff(0x60054, effCoord, 0x80002700, &pos);
                    i++;
                    pos.vx += 0x190;
                } while ((u32)(i & 0xFFFF) < 2U);
                Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_dryfield_water_tower_80181AA0, 0);
                state->field_58++;
            }
            break;

        case 2:
            state->field_5A++;
            if ((s16)state->field_5A >= 0xB) {
                Mem_CopyUnaligned(D_dryfield_water_tower_80181B10, D_dryfield_water_tower_801828DC, 0x10);
                Mem_CopyUnaligned(D_dryfield_water_tower_80181BA0, D_dryfield_water_tower_80182F44, 0x18);
                Mem_CopyUnaligned(D_dryfield_water_tower_80181B20, D_dryfield_water_tower_801829F4, 0x40);
                Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_dryfield_water_tower_80181AA0, 0);
                return 1;
            }
            coord->coord.t[2] = D_dryfield_water_tower_80181AA8;
            if (D_80070F6C[0] & 4) {
                coord->coord.t[2] = D_dryfield_water_tower_80181AA8 + 0xA;
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
    GsCOORDINATE2*           coord;
    DwtwVec                  pos;
    MATRIX*                  mem;

    obj   = (TmdObject*)arg0->extra;
    state = (DryfieldWaterTowerState*)arg0->work;
    coord = obj->coords;
    if (gGameSession->field_65 != 0) {
        obj->flags |= 0x80;
        return;
    }
    new_var     = 0;
    obj->flags &= 0xFF7F;
    if (D_80114C11 == 0) {
        switch (arg0->state) {
            case 0: {
                TmdObject*     model;
                GsCOORDINATE2* modelCoord;

                model      = (TmdObject*)arg0->extra;
                modelCoord = model->coords;
                mem        = (MATRIX*)Mem_Malloc(0x7C, false);
                arg0->work = (TaskIdMap*)mem;
                if (mem == 0) {
                    taskKill(arg0);
                } else {
                    Mem_Set(mem, 0, 0x7C);
                    ((DryfieldWaterTowerState*)mem)->field_40 = gameGetPtrSlot(3);
                    modelCoord->sub                           = &gGfxViewCoord;
                    model->flags                              = 0;
                    Tmd_AllocBuffers(model);
                    model->colorMtx = mem;
                    model->colorMtx = model->colorMtx + 1;
                    model->lightMtx = mem;
                    arg0->msgTable  = &D_dryfield_water_tower_80181B00;
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
        model      = (TmdObject*)arg0->extra;
        pos.vec.vx = model->coords->workm.t[0];
        pos.vec.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
        pos.vec.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
        func_800D7A9C(model, &pos.vec, 0, 3);
    }
    if (state->field_70 != new_var) {
        pos.rot.vx = 0;
        pos.rot.vy = -((u16)coord->coord.t[1]) - 0xC8;
        pos.rot.vz = 0;
        Gp_DrawFloorQuad(((TmdObject*)arg0->extra)->coords, 0x300, &pos.rot);
    }
}

/// A second cap-arrival body, the sibling of `func_dryfield_water_tower_8017E5B0`
/// and `func_dryfield_water_tower_8017DFAC`: its `Task::work` is the same
/// 0x7C-byte `DryfieldWaterTowerState` the cap script allocates and its
/// `extra->field_8` the cap's own coordinate, and it reports arrival the same
/// way the cap-arrival test does, by returning 1.
///
/// State 0 is the lowering tick: it sinks the cap's Z by 0x14 a frame and snaps
/// its Y to the run's lowered record, nudged by 5 while the `D_80070F6C` flag
/// bit 2 is raised; once the cap's Z has passed that record's `pos.vz` it steps
/// to state 1. Every frame of the state also spawns effect 0x60054 at the cap,
/// offset in X by the room's per-frame table entry
/// `D_..._80181C60[killCountdown]`, and wraps that 0..9 counter. State 1 counts
/// `field_5A`; on its 0x3D-th tick it publishes the 0x7D4 record at 0x80181A58
/// and returns 1, and until then mirrors that record's `pos.vx` into the cap's
/// X, nudged by the same flag. Every path clears `coord->flg`, leaving the
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
s32 func_dryfield_water_tower_8017E428(Task* arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->work;
    GsCOORDINATE2*           coord = ((TmdObject*)arg0->extra)->coords;
    GsCOORDINATE2*           effCoord;
    SVECTOR                  pos;

    switch (state->field_58) {
        case 0:
            coord->coord.t[2] += 0x14;
            coord->coord.t[1]  = D_dryfield_water_tower_80181A40[1].pos.vy;
            if (D_80070F6C[0] & 4) {
                coord->coord.t[1] += 5;
            }
            if (coord->coord.t[2] > D_dryfield_water_tower_80181A40[1].pos.vz) {
                state->field_58++;
            }
            effCoord = ((TmdObject*)arg0->extra)->coords;
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
                Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_dryfield_water_tower_80181A58, 0);
                return 1;
            }
            coord->coord.t[0] = D_dryfield_water_tower_80181A58.pos.vx;
            if (D_80070F6C[0] & 4) {
                coord->coord.t[0] += 5;
            }
            break;
    }
    coord->flg = 0;
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
/// placement record's `pos.vy`, nudged by 5 while the `D_80070F6C` flag bit 2 is
/// raised; once the cap's Z has sunk past that record's `pos.vz` it queues the
/// same event again as 0x5214000C/0xA and steps to state 1. Every frame of the
/// state also spawns effect 0x60054 at the cap, offset in X by the room's
/// per-frame table entry `D_..._80181C60[killCountdown]`, and wraps that 0..9
/// counter. State 1 counts `field_5A`; on its 0x3D-th tick it publishes the
/// 0x7D4 record at 0x80181A40 and returns 1, and until then mirrors that
/// record's `pos.vx` into the cap's X, nudged by the same flag. Every path
/// clears `coord->flg`, leaving the coordinate dirty for the next
/// `Gp_UpdateCoord` pass.
///
/// Two shapes here are the original's rather than stylistic, and folding either
/// away moves the two loads `Gp_SpawnEff` takes as its coordinate argument:
/// `effCoord` is filled from `arg0->extra` *before* the counter update, which is
/// what puts them in that block instead of the join block, and the two flag
/// branches re-read the coordinate (`+= 5`) rather than reloading the record, so
/// CSE forwards the stored value and one load serves both uses.
s32 func_dryfield_water_tower_8017E5B0(Task* arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->work;
    GsCOORDINATE2*           coord = ((TmdObject*)arg0->extra)->coords;
    GsCOORDINATE2*           effCoord;
    SVECTOR                  pos;

    switch (state->field_58) {
        case 0:
            SndEvt_EnqueueTypeB(0x5214000C, 0x7F);
            coord->coord.t[2] -= 0x14;
            coord->coord.t[1]  = D_dryfield_water_tower_80181A40[0].pos.vy;
            if (D_80070F6C[0] & 4) {
                coord->coord.t[1] += 5;
            }
            if (coord->coord.t[2] < D_dryfield_water_tower_80181A40[0].pos.vz) {
                SndEvt_EnqueueType7(0x5214000C, 0xA);
                state->field_58++;
            }
            effCoord = ((TmdObject*)arg0->extra)->coords;
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
                Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_dryfield_water_tower_80181A40, 0);
                return 1;
            }
            coord->coord.t[0] = D_dryfield_water_tower_80181A40[0].pos.vx;
            if (D_80070F6C[0] & 4) {
                coord->coord.t[0] += 5;
            }
            break;
    }
    coord->flg = 0;
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
    GsCOORDINATE2*           coord;
    VECTOR                   vec;

    obj = (TmdObject*)arg0->extra;
    if (gGameSession->field_65 != 0) {
        obj->flags |= 0x80;
        return;
    }
    obj->flags &= 0xFF7F;
    if ((s8)Gp_StateC08.field_9 != 0) {
        return;
    }
    switch (arg0->state) {
        case 0:
            tmp        = (TmdObject*)arg0->extra;
            coord      = tmp->coords;
            state      = (DryfieldWaterTowerState*)Mem_Malloc(0x7C, false);
            arg0->work = (TaskIdMap*)state;
            if (state == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(state, 0, 0x7C);
                state->field_40 = gameGetPtrSlot(3);
                coord->sub      = &gGfxViewCoord;
                tmp->flags      = 0;
                Tmd_AllocBuffers(tmp);
                tmp->colorMtx  = (MATRIX*)state + 1;
                tmp->lightMtx  = (MATRIX*)state;
                arg0->msgTable = &D_dryfield_water_tower_80181B00;
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
    model  = (TmdObject*)arg0->extra;
    vec.vx = model->coords->workm.t[0];
    vec.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
    vec.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
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
/// `field_68` in `D_8007216C` and raise the session's `viewDirty`, the pair
/// `func_dryfield_water_tower_8017D948` undoes.
///
/// The last three commands start a script-18 pair each -- the cutscene
/// `func_dryfield_water_tower_8017F908` waits on with its 0x5214000C -- into
/// `field_50`; command 8 raises the `field_78` running flag that
/// `func_dryfield_water_tower_8017EB7C` clears and queues two sounds where 9
/// and 10 queue one.
void func_dryfield_water_tower_8017E93C(Task* arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->work;

    switch (state->field_5C) {
        case 0:
            break;

        case 1: {
            RoomActorMsg msg;

            Gp_DispatchMsg(state->field_40, 0x3F3, 1, 0);
            msg.command = 2;
            Gp_DispatchMsg(state->field_44, 0x7DB, (s32)&msg, 0);
            break;
        }

        case 4:
            if (state->field_66 == 2) {
                Gp_DispatchMsg(state->field_40, 0x3E9, (s32)&D_dryfield_water_tower_80181AD0, 0);
            }
            Gp_DispatchMsg(state->field_40, 0x3F3, 1, 0);

        case 2:
            D_8007216C              = state->field_68;
            gGameSession->viewDirty = 1;
            break;

        case 3: {
            RoomActorMsg msg;

            D_dryfield_water_tower_80187074.field_4A |= 0x40;
            if (state->field_66 == 2) {
                Gp_DispatchMsg(state->field_40, 0x3E9, (s32)&D_dryfield_water_tower_80181AE8, 0);
                Gp_DispatchMsg(state->field_40, 0x3F2, (s32)&D_dryfield_water_tower_80181AE8 - 0x18,
                               0);
            } else {
                Gp_DispatchMsg(state->field_40, 0x3F3, 0, 0);
            }
            msg.command = 3;
            Gp_DispatchMsg(state->field_44, 0x7DB, (s32)&msg, 0);
            break;
        }

        case 6: {
            RoomActorMsg msg;

            msg.command = 2;
            Gp_DispatchMsg(state->field_48, 0x7DB, (s32)&msg, 0);
            break;
        }

        case 7: {
            RoomActorMsg msg;

            Gp_DispatchMsg(state->field_40, 0x3E9, (s32)&D_dryfield_water_tower_80181AD0, 0);
            msg.command = 3;
            Gp_DispatchMsg(state->field_48, 0x7DB, (s32)&msg, 0);
            break;
        }

        case 8:
            state->field_78 = 1;
            state->field_50 = Gp_SpawnScript18((s32)&D_dryfield_water_tower_80187628,
                                               (s32)&D_dryfield_water_tower_8018763C);
            SndEvt_EnqueueType6(0x52140006, 0, 0);
            SndEvt_EnqueueType6(0x5214000C, 0, 0);
            break;

        case 9:
            state->field_50 = Gp_SpawnScript18((s32)&D_dryfield_water_tower_8018764C,
                                               (s32)&D_dryfield_water_tower_80187660);
            SndEvt_EnqueueType6(0x52140007, 0, 0);
            break;

        case 10:
            state->field_50 = Gp_SpawnScript18((s32)&D_dryfield_water_tower_80187670,
                                               (s32)&D_dryfield_water_tower_80187678);
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
u16 func_dryfield_water_tower_8017EB7C(Task* arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->work;
    DryfieldWaterTowerState* work;
    GameSession*             session;
    RoomActorMsg             msg0;
    RoomActorMsg             msg2;
    RoomActorMsg             msg4;
    u16                      objId;
    u8                       objA;
    u8                       objB;
    s32                      reason;
    u16                      i;
    s32                      volume;

    switch (state->field_58) {
        case 0:
            if (state->field_64 == 0) {
                msg0.from.loc.stage = gGameSession->at4.loc.stage;
                msg0.from.loc.area  = gGameSession->at4.loc.area;
                msg0.command        = 9;
                Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg0, 0x7DB);
                state->field_68 = Gp_FindViewIndex(7);
                state->field_78 = 0;
                func_800E8634((s32)&D_dryfield_water_tower_80181C78, 0, (s32)&D_dryfield_water_tower_80181DC8);
                state->field_58++;
            } else {
                Gp_DispatchMsg(state->field_44, 0x7D4, (s32)&D_dryfield_water_tower_80181A58, 0);
                Gp_DispatchMsg(state->field_40, 0x3F3, 1, 0);
                Gp_DispatchMsg(state->field_40, 0x3F1, 0, 0);
                D_8007216C          = Gp_FindViewIndex(7);
                session             = gGameSession;
                session->viewDirty  = 1;
                session->hideHud    = 0;
                session->eventState = 0;
                SndEvt_EnqueueType6(0x52140006, 0, 0x20);
                SndEvt_EnqueueType6(0x5214000C, 0, 0);
                state->field_58 = 2;
            }
            D_dryfield_water_tower_80187074.field_4A &= 0xBF;
            D_dryfield_water_tower_801876AA           = _dryfieldWaterTowerStepFrames(arg0);
            Mem_CopyUnaligned(&D_dryfield_water_tower_80181BC8, D_dryfield_water_tower_801829B4, 0x40);
            Mem_CopyUnaligned(&D_dryfield_water_tower_80181BB8, D_dryfield_water_tower_801828CC, 0x10);
            Mem_CopyUnaligned(&D_dryfield_water_tower_80181C48, D_dryfield_water_tower_80182F2C, 0x18);
            return 0;

        case 1:
            if (gGameSession->eventState != 0) {
                return 0;
            }
            state->field_58++;
            break;

        case 2:
            msg2.from.loc.stage = gGameSession->at4.loc.stage;
            msg2.from.loc.area  = gGameSession->at4.loc.area;
            msg2.command        = 1;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg2, 0x7DB);
            D_dryfield_water_tower_801876A8 = 0;
            state->field_64                 = 2;
            GameFlag_SetNibble(0x55, 2);
            state->field_58++;

        case 3:
            if (Gp_TakePendingObj4C(&objId, &objA, &objB) != 0 && D_80114C12 != 1 && D_80071075 == 0 &&
                (objId & 0x7FFF) == 5 && (reason = (s8)objA) == 2) {
                Gp_UnlinkObj4A(0, D_dryfield_water_tower_80186C4C);
                state->field_68 = Gp_FindViewIndex(9);
                func_800E8634((s32)&D_dryfield_water_tower_80181E88, 0, (s32)&D_dryfield_water_tower_80181FF0);
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
                func_800E8634((s32)&D_dryfield_water_tower_80181E88, 0, (s32)&D_dryfield_water_tower_80181FF0);
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
                msg4.from.loc.stage = gGameSession->at4.loc.stage;
                msg4.from.loc.area  = gGameSession->at4.loc.area;
                msg4.command        = 3;
                Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg4, 0x7DB);
            }
            Mem_CopyUnaligned(&D_dryfield_water_tower_80181C08, D_dryfield_water_tower_801829B4, 0x40);
            Mem_CopyUnaligned(&D_dryfield_water_tower_80181BB8, D_dryfield_water_tower_801828CC, 0x10);
            Mem_CopyUnaligned(&D_dryfield_water_tower_80181C48, D_dryfield_water_tower_80182F2C, 0x18);
            GameFlag_SetNibble(0x55, 1);
            return state->field_66;
    }
    return 0;
}

/// State 7 of the cap script, one call per frame, returning non-zero once the
/// step is complete. On its first frame (`field_58` 0) it sends message 0x7DA to
/// the slot-4 game task with a `RoomActorMsg` record naming the current stage and
/// area and carrying 2 as the requested state, the reply message being 0x7DB;
/// after that it waits for the `field_6C` latch.
static inline u16 _dryfieldWaterTowerState7Step(Task* arg0)
{
    DryfieldWaterTowerState* work = (DryfieldWaterTowerState*)arg0->work;
    RoomActorMsg             msg;

    switch (work->field_58) {
        case 0:
            msg.from.loc.stage = gGameSession->at4.loc.stage;
            msg.from.loc.area  = gGameSession->at4.loc.area;
            msg.command        = 2;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
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
/// frames while `D_80114C12` is 1 or `D_80071075` is set; after that it waits
/// for the session's `eventState` to go idle and sets nibble 0x32 to 2.
static inline u16 _dryfieldWaterTowerState8Step(Task* arg0)
{
    DryfieldWaterTowerState* work = (DryfieldWaterTowerState*)arg0->work;

    switch (work->field_58) {
        case 0:
            if (D_80114C12 == 1) {
                break;
            }
            if (D_80071075 != 0) {
                return 0;
            }
            func_800E8634((s32)&D_dryfield_water_tower_801820B0, 0, (s32)&D_dryfield_water_tower_80182248);
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
/// runs. It does nothing while the session's `field_65` or `D_80114C11` is set
/// or `D_80073BA0` is zero. State 0 allocates the 0x7C-byte
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

    if (gGameSession->field_65 != 0 || D_80114C11 != 0 || D_80073BA0 == 0) {
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
                arg0->msgTable                  = &D_dryfield_water_tower_80182374;
            }
            Mem_CopyUnaligned(&D_dryfield_water_tower_80181C08, D_dryfield_water_tower_801829B4, 0x40);
            Mem_CopyUnaligned(&D_dryfield_water_tower_80181BB8, D_dryfield_water_tower_801828CC, 0x10);
            Mem_CopyUnaligned(&D_dryfield_water_tower_80181C48, D_dryfield_water_tower_80182F2C, 0x18);
            Mem_CopyUnaligned(D_dryfield_water_tower_80181B10, D_dryfield_water_tower_801828CC + 0x10, 0x10);
            Mem_CopyUnaligned(D_dryfield_water_tower_80181BA0, D_dryfield_water_tower_80182F2C + 0x18, 0x18);
            Mem_CopyUnaligned(&D_dryfield_water_tower_80181B60, D_dryfield_water_tower_801829B4 + 0x40, 0x40);
            state = (DryfieldWaterTowerState*)arg0->work;
            arg0->state++;
            break;

        case 1:
            state->field_44 = Task_SpawnFromTable(D_dryfield_water_tower_80182384, 1, 0, 0);
            state->field_48 = Task_SpawnFromTable(D_dryfield_water_tower_80182384, 2, 0, 0);
            arg0->state++;
            break;

        case 2:
            Gp_DispatchMsg(state->field_44, 0x7D4, (s32)D_dryfield_water_tower_80181A40, 0);
            Gp_DispatchMsg(state->field_48, 0x7D4, (s32)D_dryfield_water_tower_80181A70, 0);
            state->field_64 = GameFlag_GetNibble(0x55);
            if (GameFlag_GetNibble(0x32) == 0) {
                arg0->state++;
            } else if (GameFlag_GetNibble(0x32) == 1) {
                arg0->state = 5;
            } else if (GameFlag_GetNibble(0x32) == 2) {
                mask           = ~0x40;
                p0             = &D_dryfield_water_tower_80186C4C[0];
                p0->field_4A  &= mask;
                p14            = &D_dryfield_water_tower_80186C4C[14];
                p14->field_4A &= mask;
                Gp_DispatchMsg(state->field_48, 0x7D4, (s32)&D_dryfield_water_tower_80181A70[2], 0);
                ((DryfieldWaterTowerState*)state->field_48->work)->field_70 = 1;
                Mem_CopyUnaligned(D_dryfield_water_tower_80181B10, D_dryfield_water_tower_801828DC, 0x10);
                Mem_CopyUnaligned(D_dryfield_water_tower_80181BA0, D_dryfield_water_tower_80182F44, 0x18);
                Mem_CopyUnaligned(D_dryfield_water_tower_80181B20, D_dryfield_water_tower_801829F4, 0x40);
                if (state->field_64 == 3) {
                    p3            = &D_dryfield_water_tower_80186C4C[3];
                    p3->field_4A &= mask;
                    Gp_DispatchMsg(state->field_44, 0x7D4, (s32)&D_dryfield_water_tower_80181A40[1], 0);
                    Mem_CopyUnaligned(&D_dryfield_water_tower_80181BC8, D_dryfield_water_tower_801829F4 - 0x40, 0x40);
                    Mem_CopyUnaligned(&D_dryfield_water_tower_80181BB8, D_dryfield_water_tower_801828DC - 0x10, 0x10);
                    Mem_CopyUnaligned(&D_dryfield_water_tower_80181C48, D_dryfield_water_tower_80182F44 - 0x18, 0x18);
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
/// `func_actor_136100_8013467C` does -- `D_80073BA9` picked through
/// `D_8007218A` is the record's `field_0` -- but fills the two halfword slots
/// from that script argument: `field_8` is its "non-zero" flag and `field_C`
/// the halfword itself.
void func_dryfield_water_tower_8017F700(s32 arg0)
{
    GpAnimArg rec;
    s32       weaponId;
    s32       id;
    s32       value;

    weaponId            = D_80073BA9;
    id                  = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    value               = arg0 & 0xFFFF;
    rec.animBlock.index = id;
    rec.field_4         = 1;
    rec.field_8         = value != 0;
    rec.field_C         = value;
    rec.field_10        = 0;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&rec, 0);
}

/// The room's handler for message 0x7D4: copies `placement` onto the task's
/// `TmdObject` coordinate frame -- the three longs become the translation,
/// then yaw, pitch and roll are applied with `Gfx_RotMatrixY` / `X` / `Z` --
/// and marks the coordinate dirty.
void func_dryfield_water_tower_8017F77C(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}

/// Message 0x7DB handler, the room script table `D_dryfield_water_tower_80181B00`
/// lists beside its 0x7D4 entry `func_dryfield_water_tower_8017F77C`. It rests the cap script: the
/// three halfword slots it keeps its timers in are cleared along with the
/// task's kill countdown, and the payload's halfword becomes the task's state,
/// so the 0x7DB sender picks the state the cap script resumes in. The opcode
/// itself is never read, hence the named-but-unused `msgId`.
void func_dryfield_water_tower_8017F808(Task* task, s32 msgId, RoomActorMsg* msg)
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
        Gp_ReleaseStateF0Add((Task*)Gp_LookupSlot4(0), 1);
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
/// location byte `D_8007216C`, sends its 0x7D4 placement
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
    D_8007216C              = Gp_FindViewIndex(7);
    gGameSession->viewDirty = 1;
    Gp_DispatchMsg(state->field_44, 0x7D4, (s32)&D_dryfield_water_tower_80181A58, 0);
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
///
/// `Mc_SaveData.at4.loc.view` rather than the `D_8007216C` address the room imports:
/// as a scalar the store is fixed-address against the struct traffic below, so
/// `sched.c`'s `true_dependence` drops the output dependence between it and the
/// `gGameSession` store and the scheduler sinks the byte store past the whole
/// `gGameSession` pair. Naming the field keeps both MEMs in-struct and the store
/// where the target has it; the `%hi`/`%lo` pair it prints relocates to the same
/// two words. Measured; see `DECOMPILATION_LEARNINGS.md` on struct-typing and
/// aliasing.
void func_dryfield_water_tower_8017F9AC(void)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)D_dryfield_water_tower_801876A4->work;

    Mc_SaveData.at4.loc.view = state->field_68;
    gGameSession->viewDirty  = 1;
    Gp_DispatchMsg(state->field_44, 0x7D4, (s32)&D_dryfield_water_tower_80181A40, 0);
    state->field_44->state = 1;
    Gp_HaltPadScripts();
    SndEvt_EnqueueType7(0x52140007, 0xA);
    SndEvt_EnqueueType7(0x5214000C, 0xA);
    if (state->field_66 == 2) {
        Gp_DispatchMsg(state->field_40, 0x3E9, (s32)&D_dryfield_water_tower_80181AD0, 0);
    }
}

/// Script opcode 0x0D of the room's command table `D_dryfield_water_tower_80182248`:
/// it hands the stream to view 9, restarts the prop task at `field_48` on state 1
/// and gives it its 0x7D4 placement, moves the player to `80181AD0`, stops the pad
/// scripts, plays event 0x5214000B and installs three of the room's effect tables.
///
/// `Mc_SaveData.at4.loc.view` rather than a bare `extern` for 0x8007216C: the store is
/// a struct member, so the read of `field_48` below still conflicts with it in
/// `true_dependence`. As a scalar global the pair is fixed-address against
/// varying-struct and GCC 2.8.1 drops the dependence, which lets the scheduler
/// sink the store into `Gp_DispatchMsg`'s delay slot. Measured; see
/// `DECOMPILATION_LEARNINGS.md` on struct-typing and aliasing.
void func_dryfield_water_tower_8017FA5C(void)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)D_dryfield_water_tower_801876A4->work;

    Mc_SaveData.at4.loc.view = Gp_FindViewIndex(9);
    Gp_DispatchMsg(state->field_48, 0x7D4, (s32)&D_dryfield_water_tower_80181AA0, 0);
    state->field_48->state = 1;
    Gp_DispatchMsg(state->field_40, 0x3E9, (s32)&D_dryfield_water_tower_80181AD0, 0);
    Gp_HaltPadScripts();
    SndEvt_EnqueueType7(0x5214000B, 0xA);
    Mem_CopyUnaligned(D_dryfield_water_tower_80181B10, D_dryfield_water_tower_801828DC, 0x10);
    Mem_CopyUnaligned(D_dryfield_water_tower_80181BA0, D_dryfield_water_tower_80182F44, 0x18);
    Mem_CopyUnaligned(D_dryfield_water_tower_80181B20, D_dryfield_water_tower_801829F4, 0x40);
    ((DryfieldWaterTowerState*)state->field_48->work)->field_70 = 1;
}

/// The `DwtwStep` the rotation's step counter is on: the last entry of
/// `D_dryfield_water_tower_8018767C` whose threshold is below
/// `DryfieldWaterTowerState::field_72`, walked from the second entry -- a
/// counter of 0 fails the entry-0 test and takes the first entry without
/// walking -- and capped by the `0xFFFF` terminator. The step's duration comes
/// back 30-fold with its low bit cleared.
s32 func_dryfield_water_tower_8017FB4C(Task* task)
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
void func_dryfield_water_tower_8017FBE8(Task* task)
{
    DwtwWork* work  = (DwtwWork*)task->work;
    u16       state = work->field_C;

    switch (state) {
        case 0:
            break;
        case 1:
            Gp_DispatchMsg(work->field_4, 0x7D3, (s32)&D_dryfield_water_tower_80182420[0], 0);
            Gp_DispatchMsg(work->field_4, 0x7D4, (s32)&D_dryfield_water_tower_801823C0[0], 0);
            Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&D_dryfield_water_tower_801823C0[3], 0);
            Gp_DispatchMsg(work->field_8, 0x7D3, (s32)&D_dryfield_water_tower_80182420[1], 0);
            Gp_DispatchMsg(work->field_0, 0x3F3, 2, 0);
            Task_SpawnFromTable(D_dryfield_water_tower_8018277C, 2, 8, 0);
            break;
        case 2:
            Gp_DispatchMsg(work->field_4, 0x7D4, (s32)&D_dryfield_water_tower_801823F0, 0);
            Gp_DispatchMsg(work->field_4, 0x7D3, (s32)&D_dryfield_water_tower_80182448, 0);
            break;
        case 4:
            Gp_DispatchMsg(work->field_0, 0x3F3, 1, 0);
            Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&D_dryfield_water_tower_801823A8, 0);
            break;
        case 5:
            Gp_DispatchMsg(work->field_4, 0x7D3, (s32)&D_dryfield_water_tower_80182420[0], 0);
            break;
        case 6:
            Gp_DispatchMsg(work->field_8, 0x7D3, (s32)&D_dryfield_water_tower_80182434, 0);
            break;
    }
    work->field_C = 0;
}

/// Room entry point: install the player's weapon animation set on slot 3
/// (message 0x3E8) unless `Gp_StateC08.field_A` says a battle is running or
/// `D_80071075` says one has just ended, then allocate the `DwtwWork` the room
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
    GpAnimArg msg;
    DwtwWork* work;
    s32       id;
    s32       weaponId;
    s32       anim;

    if (gGameSession->field_65 != 0) {
        return;
    }
    if ((s8)Gp_StateC08.field_9 != 0) {
        return;
    }
    switch (task->state) {
        case 0:
            if (Gp_StateC08.field_A == 1 || D_80071075 != 0) {
                return;
            }
            weaponId            = D_80073BA9;
            anim                = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.animBlock.index = anim;
            msg.field_4         = 1;
            msg.field_8         = 1;
            msg.field_C         = 0xA;
            msg.field_10        = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&msg, 0);
            work       = (DwtwWork*)Mem_Malloc(0x18, 0);
            task->work = (TaskIdMap*)work;
            if (work == NULL) {
                taskKill(task);
            } else {
                Mem_Set(work, 0, 0x18);
                work->field_0                   = gameGetPtrSlot(3);
                D_dryfield_water_tower_801876AC = task;
                id                              = gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8);
                work->field_4                   = (Task*)Gp_FindWorkById(id)->field_0;
                id                              = ((gGameSession->at4.loc.stage << 8) | 0x1000) | gGameSession->at4.loc.area;
                work->field_8                   = (Task*)Gp_FindWorkById(id)->field_0;
            }
            task->state++;
            break;
        case 1:
            func_800E8634((s32)&D_dryfield_water_tower_80182464, 0, (s32)&D_dryfield_water_tower_80182674);
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
/// allocates the `RoomFadeWork` block into `Task::work` and saturates its
/// three channels at 0xFF; a failed allocation kills the task. Every state-1
/// frame draws the overlay tinted `r`/`g`/`r` with `Fade_DrawOverlay` and
/// lowers each channel by `Task::spawnArg1`, the fade rate; once `r` falls
/// below zero the task kills itself.
void func_dryfield_water_tower_8017FF5C(Task* arg0)
{
    RoomFadeWork* work;
    RoomFadeWork* alloc;

    work = (RoomFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (RoomFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
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
            work->r -= (u16)arg0->spawnArg1;
            work->g -= (u16)arg0->spawnArg1;
            work->b -= (u16)arg0->spawnArg1;
            if ((s16)work->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

/// The room's fade-out task: state 0 allocates the 8-byte `RoomFadeWork` block
/// into `Task::work` and clears its three channels, and every state-1 frame
/// draws them with `Fade_DrawOverlay` and raises each by `Task::spawnArg1`, the
/// fade rate. The red channel is the one watched: once it passes 0x100 the fade
/// has run its course and the task kills itself. The task is the second
/// descriptor of `D_dryfield_water_tower_8018277C`, the table whose entry 0 is
/// the room script task. The table's entry 2, `func_dryfield_water_tower_8017FF5C`,
/// runs the same body backwards, from saturated channels falling past zero.
void func_dryfield_water_tower_80180038(Task* arg0)
{
    RoomFadeWork* work;
    RoomFadeWork* alloc;

    work = (RoomFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (RoomFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
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
            work->r += (u16)arg0->spawnArg1;
            work->g += (u16)arg0->spawnArg1;
            work->b += (u16)arg0->spawnArg1;
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
    RoomActorMsg msg;

    if (work->field_14 == 0) {
        Gp_ArmStateF0(1);
        msg.from.loc.stage = gGameSession->at4.loc.stage;
        msg.from.loc.area  = gGameSession->at4.loc.area;
        msg.command        = 0;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
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

    Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&D_dryfield_water_tower_801823D8[0], 0);
    Gp_DispatchMsg(work->field_4, 0x7D4, (s32)&D_dryfield_water_tower_801823D8[1], 0);
    Gp_DispatchMsg(work->field_0, 0x3F3, 1, 0);
    Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&D_dryfield_water_tower_801823A8, 0);
    D_8007216C              = Gp_FindViewIndex(4);
    gGameSession->viewDirty = 1;
    CdCmd_CancelReplaceAndActivate();
    Gp_RestoreStreamRng();
}

/// Hides or shows one of the area's sprite commands from the nibble-0x55 band
/// by setting its `GpSprtCmd::field_4`: the argument's low byte zero keeps the
/// command's sprites out of the ordering table, non-zero draws them. Only stage
/// 2 has a record to write.
void func_dryfield_water_tower_801802D8(u8 arg0)
{
    GpAreaKey* sess;
    GpSprtCmd* vs;

    sess = &gGameSession->at4.loc;
    if (sess->stage == 2) {
        vs = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1][18].field_4;
        if (!(arg0 & 0xFF)) {
            vs[1].field_4 = 1;
            return;
        }
        vs[1].field_4 = 0;
    }
}

void func_dryfield_water_tower_80180348(void)
{
    Gp_State1C->roomEffectMode = D_dryfield_water_tower_801827A0[(Gp_GetViewIndex() & 0xFF) - 1];
}
