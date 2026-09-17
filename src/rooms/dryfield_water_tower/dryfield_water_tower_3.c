#include "common.h"

#include <psyq/libgte.h>

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "rooms/dryfield_water_tower.h"
#include "rooms/room_common.h"

extern u8 D_8007216C;
extern u8 D_801153F4;

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on, and
/// `D_8007218A` picks which of the two weapon-id bases that record uses.
extern u8 D_80073BA9;
extern s8 D_8007218A;

extern u32   D_dryfield_water_tower_8018768C;
extern Task* D_dryfield_water_tower_801876A0;

/// The task table `func_dryfield_water_tower_8017F128` spawns the room's two
/// cap props from, at 0x80182384: entry 0 is the cap script itself
/// (`func_dryfield_water_tower_8017F128`), entry 1 the prop
/// `func_dryfield_water_tower_8017E764` and entry 2 the prop
/// `func_dryfield_water_tower_8017E1DC`; the entry whose callback is -1 ends
/// the table.
extern TaskDesc D_dryfield_water_tower_80182384;

/// The room's two cap placements with message 0x7D4, the pair the cap props
/// publish to themselves through `Room_Util08`: `[0]` is the raised position
/// (Y = -0x1F40, the one a task spawned with a non-zero `spawnArg1` publishes)
/// and `[1]` the lowered one (Y = -0xFA0) the lowering prop stops at. They are
/// the two 0x18-byte records above them in the same run --
/// `func_dryfield_water_tower_8017F908` sends `-0x20` and `8017F9AC` `-0x38` --
/// so the split names only the first.
extern RoomPlacement D_dryfield_water_tower_80181A70[];

/// The room's 4A object -- the list node `Gp_LinkObj4A` chains into
/// `Gp_Obj4ALists` and `Gp_UnlinkObj4A` takes out again. Command 3 of
/// `func_dryfield_water_tower_8017E93C` raises its `field_4A` bit 0x40, the
/// same bit `func_acropolis_fountain_8017DA1C` raises on the fountain's node.
extern GpObj4A D_dryfield_water_tower_80187074;

/// The two player placements the cap script's commands 3, 4 and 7 dispatch as
/// the payload of message 0x3E9 (their handler is the slot-3 game task at
/// `field_40`): `80181AD0` from commands 4 and 7, and `80181AE8` from command
/// 3, which sends `80181AD0` -- the 0x18-byte record one step below it in the
/// same run -- with 0x3F2 straight after.
extern RoomPlacement D_dryfield_water_tower_80181AD0;
extern RoomPlacement D_dryfield_water_tower_80181AE8;

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

void func_dryfield_water_tower_8017DCB4(void);

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
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->idMap;
    GsCOORDINATE2*           coord = ((TmdObject*)arg0->extra)->field_8;
    DwtwMsg7DB               msg;

    if (arg0->spawnArg1 == 0) {
        switch (state->field_58) {
            case 0:
                state->field_48 = Task_SpawnFromTable(&D_dryfield_water_tower_80182384, 2, 1, 0);
                state->field_58++;
                return;

            case 1:
                msg.field_2 = 2;
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

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017DFAC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017E1DC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017E428);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017E5B0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017E764);

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
/// `field_68` in `D_8007216C` and raise the session's `field_52`, the pair
/// `func_dryfield_water_tower_8017D948` undoes.
///
/// The last three commands start a script-18 pair each -- the cutscene
/// `func_dryfield_water_tower_8017F908` waits on with its 0x5214000C -- into
/// `field_50`; command 8 raises the `field_78` running flag that
/// `func_dryfield_water_tower_8017EB7C` clears and queues two sounds where 9
/// and 10 queue one.
void func_dryfield_water_tower_8017E93C(Task* arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)arg0->idMap;

    switch (state->field_5C) {
        case 0:
            break;

        case 1: {
            DwtwMsg7DB msg;

            Gp_DispatchMsg(state->field_40, 0x3F3, 1, 0);
            msg.field_2 = 2;
            Gp_DispatchMsg(state->field_44, 0x7DB, (s32)&msg, 0);
            break;
        }

        case 4:
            if (state->field_66 == 2) {
                Gp_DispatchMsg(state->field_40, 0x3E9, (s32)&D_dryfield_water_tower_80181AD0, 0);
            }
            Gp_DispatchMsg(state->field_40, 0x3F3, 1, 0);

        case 2:
            D_8007216C             = state->field_68;
            Game_Session->field_52 = 1;
            break;

        case 3: {
            DwtwMsg7DB msg;

            D_dryfield_water_tower_80187074.field_4A |= 0x40;
            if (state->field_66 == 2) {
                Gp_DispatchMsg(state->field_40, 0x3E9, (s32)&D_dryfield_water_tower_80181AE8, 0);
                Gp_DispatchMsg(state->field_40, 0x3F2, (s32)&D_dryfield_water_tower_80181AE8 - 0x18,
                               0);
            } else {
                Gp_DispatchMsg(state->field_40, 0x3F3, 0, 0);
            }
            msg.field_2 = 3;
            Gp_DispatchMsg(state->field_44, 0x7DB, (s32)&msg, 0);
            break;
        }

        case 6: {
            DwtwMsg7DB msg;

            msg.field_2 = 2;
            Gp_DispatchMsg(state->field_48, 0x7DB, (s32)&msg, 0);
            break;
        }

        case 7: {
            DwtwMsg7DB msg;

            Gp_DispatchMsg(state->field_40, 0x3E9, (s32)&D_dryfield_water_tower_80181AD0, 0);
            msg.field_2 = 3;
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

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017EB7C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017F128);

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
    GpRec14 rec;
    s32     weaponId;
    s32     id;
    s32     value;

    weaponId     = D_80073BA9;
    id           = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    value        = arg0 & 0xFFFF;
    rec.field_0  = id;
    rec.field_4  = 1;
    rec.field_8  = value != 0;
    rec.field_C  = value;
    rec.field_10 = 0;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&rec, 0);
}

/* The jump tables the two script commands `func_dryfield_water_tower_8017EB7C`
   and `func_dryfield_water_tower_8017F128` switch through, which follow the one
   `func_dryfield_water_tower_8017E93C` compiles in the same rodata run; the
   `.align 3` ahead of the first is the word between them. They are items of
   unit 5's run -- the two functions belong to this unit, so the split cannot
   pair them with a body of their own. */
INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_5", jtbl_dryfield_water_tower_8017D618);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_5", jtbl_dryfield_water_tower_8017D630);
