#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/gameflow.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_water_tower.h"
#include "rooms/room_common.h"

/// Placement `func_dryfield_water_tower_8017F908` sends to the prop task at
/// `DryfieldWaterTowerState::field_44` with message 0x7D4, whose handler is
/// `Room_Util08` -- the same message and handler the table's other 0x0D entry
/// `func_dryfield_water_tower_8017FA5C` uses below. One whole 0x18-byte run,
/// unlike the placement that one sends, which splat splits in three.
extern RoomPlacement D_dryfield_water_tower_80181A58;

/// Placement `func_dryfield_water_tower_8017F9AC` sends to the prop task at
/// `DryfieldWaterTowerState::field_44` with the same message 0x7D4, whose
/// handler is `Room_Util08`. The 0x18 bytes immediately below the `80181A58`
/// run `func_dryfield_water_tower_8017F908` sends with that message, so the two
/// streams differ in the placement they hand the task and in nothing else this
/// record carries.
extern RoomPlacement D_dryfield_water_tower_80181A40;

/// Placement `func_dryfield_water_tower_8017FA5C` sends to the prop task at
/// `DryfieldWaterTowerState::field_48` with message 0x7D4, whose handler is
/// `Room_Util08`. The first of three chunks splat splits it into; the other two
/// (`80181AA4` / `80181AA8`) are the rest of the 0x18 bytes.
extern RoomPlacement D_dryfield_water_tower_80181AA0;

/// Placement the same function sends to the slot-3 game task at `field_40` with
/// message 0x3E9, the player move `func_dryfield_water_tower_80180220` also
/// uses. 0x18 bytes, one word run in the split.
extern RoomPlacement D_dryfield_water_tower_80181AD0;

/// `func_dryfield_water_tower_8017F908` writes the room's view index here and
/// `func_dryfield_water_tower_8017FA5C` writes view 9's; the byte is
/// `Mc_SaveData.at4.loc.view`, the saved location `Gp_CommitSpawnLoc` sets and
/// `Gp_ReloadAtLoc` restores. Spelled by address because that is the name the
/// room imports: the struct spelling prints `%hi(Mc_SaveData)` /
/// `%lo(Mc_SaveData+4)` and costs the scratch scorer 0.24% on an object whose
/// words are identical, while the two entries below need it for aliasing. Both
/// spellings are deliberate.
extern s8 D_8007216C;

/// The three effect-definition tables `func_dryfield_water_tower_8017FA5C`
/// installs into the room's live copies -- 0x10, 0x18 and 0x40 bytes. The same
/// three pairs, at the same sizes, are copied by
/// `func_dryfield_water_tower_8017F128` on its state-7 path, so each is one
/// variant of a table the room selects between rather than the only content.
/// The destinations are also reached with an offset from a lower base
/// (`0x801828CC + 0x10` is `801828DC`), i.e. the live table is a region the
/// room walks. The record types are not pinned yet, so the blobs stay bytes.
extern u8 D_dryfield_water_tower_80181B10[];
extern u8 D_dryfield_water_tower_801828DC[];
extern u8 D_dryfield_water_tower_80181BA0[];
extern u8 D_dryfield_water_tower_80182F44[];
extern u8 D_dryfield_water_tower_80181B20[];
extern u8 D_dryfield_water_tower_801829F4[];

/// The room's four-step rotation schedule: `{0,25}`, `{1,30}`, `{2,35}` and the
/// `{0xFFFF,40}` terminator `func_dryfield_water_tower_8017FB4C` selects from by
/// `DryfieldWaterTowerState::field_72` and returns the step's 30-fold. The
/// thresholds are the step numbers themselves, so the entry the walk lands on
/// is the counter's own step, and the terminator is what holds the last step
/// once the counter runs past it. `func_dryfield_water_tower_8017EB7C` advances
/// that counter and walks the same table inline, storing the duration into the
/// halfword at 0x801876AA its 0x801876A8 frame counter is compared against.
extern DwtwStep D_dryfield_water_tower_8018767C[];

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
/// location byte `D_8007216C`, sends its 0x7D4 (`Room_Util08`) placement
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

/// The third of the unit's 0x0D script handlers, between
/// `func_dryfield_water_tower_8017F908` and `func_dryfield_water_tower_8017FA5C`
/// and the one that does not recompute the view: it records the state byte
/// `DryfieldWaterTowerState::field_68` in the saved location, starts the prop
/// task at `field_44` on state 1 with its 0x7D4 (`Room_Util08`) placement
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

/// The `RoomPlacement` run the room's 0x7D4 messages step the props through:
/// the 0x18-byte records from 0x801823A8 up to 0x80182408. `D_..._801823C0`,
/// the second of them, is case 1's pair -- `[0]` to `field_4` and `[3]`
/// (0x80182408) to `field_8`; `D_..._801823F0`, the run's element 2, is case
/// 2's, and the same record the script opcode
/// `func_dryfield_water_tower_80180220` sends as element 1 of the pair it
/// declares `D_..._801823D8[]`; and `D_..._801823A8`, the first, is the player
/// move both that opcode and case 4 send with 0x3E9.
extern RoomPlacement D_dryfield_water_tower_801823C0[];
extern RoomPlacement D_dryfield_water_tower_801823F0;
extern RoomPlacement D_dryfield_water_tower_801823A8;

/// The `GpAnimArg` (0x14-byte) run the 0x7D3 animation messages send: `field_4`
/// carries the animation index -- 0x0D / 0x0E / 0x0F for the three records --
/// and every other field, `field_8` included, is zero. Element 1 and element 2
/// are also named by address, and the code below uses both spellings: element 1
/// is `D_..._80182420[1]` in case 1 and `D_..._80182434` in case 6.
extern GpAnimArg D_dryfield_water_tower_80182420[];
extern GpAnimArg D_dryfield_water_tower_80182434[];
extern GpAnimArg D_dryfield_water_tower_80182448[];

/// The task table `Task_SpawnFromTable`'s `2` below indexes: entry 0 is the
/// room task `func_dryfield_water_tower_8017FD64` itself, entry 1 the private
/// fade-out task `func_dryfield_water_tower_80180038` and entry 2 the shared
/// fade-*up* body `RoomsShared8017ff5c`, the one state 1 starts here with the
/// fade rate 8.
extern TaskDesc D_dryfield_water_tower_8018277C[];

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

/// Main-executable globals with no room-side header: `D_80073BA9` is the
/// equipped-weapon index the slot-3 message 0x3E8 record is keyed on and
/// `D_8007218A` picks which of the two weapon-id bases that record uses; the
/// alternate block is indexed by `D_80073BA9` plus 1 against the base block's
/// plus 0x22. `D_80071075` is the flag that, with `Gp_StateC08.field_A`, holds
/// this room's state 0 back.
extern u8 D_80073BA9;
extern s8 D_8007218A;
extern u8 D_80071075;

/// The pair of cutscene blocks `func_800E8634` hands to `Task_Spawn` (bank 9,
/// type 7): the one the running scene starts and the one it parks in
/// `D_801156D0` for the task that follows it.
extern s32 D_dryfield_water_tower_80182464;
extern s32 D_dryfield_water_tower_80182674;

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
            weaponId     = D_80073BA9;
            anim         = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.field_0  = (void*)anim;
            msg.field_4  = 1;
            msg.field_8  = 1;
            msg.field_C  = 0xA;
            msg.field_10 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&msg, 0);
            work       = (DwtwWork*)Mem_Malloc(0x18, 0);
            task->work = (TaskIdMap*)work;
            if (work == NULL) {
                Task_Kill(task);
            } else {
                Mem_Set(work, 0, 0x18);
                work->field_0                   = (Task*)Game_GetPtrSlot(3);
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
