#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"

extern void func_80724608(void* owner, s32 arg1, s32 arg2, void* name);

extern u8  D_80071075;
extern s16 D_8007107A;
extern s8  D_80114C12;
extern s16 D_80071076;
extern s8  D_801153F1;

extern s32            D_dryfield_dilapidated_house_80189B6C;
extern s32            D_dryfield_dilapidated_house_80183EFC;
extern s32            D_dryfield_dilapidated_house_80184408;
extern s32            D_dryfield_dilapidated_house_80184C60;
extern TaskDesc       D_dryfield_dilapidated_house_80183EB4;
extern s32            D_dryfield_dilapidated_house_80184EA0;
extern s32            D_dryfield_dilapidated_house_801855F0;
extern GpAreaApplyRec D_dryfield_dilapidated_house_80189AA0;
extern GpAreaApplyRec D_dryfield_dilapidated_house_80189B24;

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017D64C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017DE88);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D5C4);

/// The `"AUNT"` and `"Player"` name words `func_dryfield_dilapidated_house_8017E014`
/// hands to `func_80724608`, the two calls this room makes to it. The pair is
/// the 20-byte run that follows the room's task table at the head of the
/// overlay's leading rodata, and that run was carried by this function's own
/// assembly file until the body moved into C, so the bytes are written here -
/// between the `INCLUDE_RODATA` above and the unit's next emission below, which
/// is where the address order puts them. `Player` spans 12 bytes because it
/// also covers the four zero bytes that pad the run out to the switch table of
/// `func_dryfield_dilapidated_house_8017E144`.
///
/// The section attribute is load-bearing: at 8 and 12 bytes these fall under
/// the compiler's small-data threshold and would otherwise be emitted into
/// `.sdata`, which the overlay's linker script does not lay out.
const char D_dryfield_dilapidated_house_8017D5D0[8] __attribute__((section(".rodata")))  = "AUNT";
const char D_dryfield_dilapidated_house_8017D5D8[12] __attribute__((section(".rodata"))) = "Player";

/// Room gate task. While the session is in the room (`Game_Session->field_1`
/// is 0) it walks `D_dryfield_dilapidated_house_80183EFC` from 1 to 2 and then
/// to 3: the 1 -> 2 step is unconditional, the 2 -> 3 step waits for the room's
/// message (0x7D6) to be dispatched and answered with 0 by the slot-0 object,
/// and for no sound to be playing; reaching 3 spawns entry 3 of the room's task
/// table. Independently, once the stream file is open it starts the named
/// sequences `"AUNT"` and `"Player"` on the two slot objects.
void func_dryfield_dilapidated_house_8017E014(void)
{
    if (Game_Session->field_1 == 0) {
        if (D_dryfield_dilapidated_house_80183EFC == 1) {
            D_dryfield_dilapidated_house_80183EFC = 2;
        } else if ((D_dryfield_dilapidated_house_80183EFC == 2) &&
                   (Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D6, 0, 0) == 0)) {
            if (D_80114C12 != 1) {
                if (D_80071075 == 0) {
                    D_dryfield_dilapidated_house_80183EFC += 1;
                    Task_SpawnFromTable(&D_dryfield_dilapidated_house_80183EB4, 3, 0, 0);
                }
            }
        }
    }
    if ((D_8007107A != 0) && (Gp_LookupSlot4(1) != 0)) {
        func_80724608(Gp_LookupSlot4(1), -0x8C, 0xA, &D_dryfield_dilapidated_house_8017D5D0);
        func_80724608(Game_GetPtrSlot(3), -0x8C, 0x14, &D_dryfield_dilapidated_house_8017D5D8);
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017E144);

/// Scene-clear task: the room's hand-off to the rest of the game. State 0
/// starts the streamed scene named by the two blocks `func_800E8634` takes,
/// state 1 fires when the session is back in play (`Game_Session->field_1`
/// is 2) and hands slot 0 the release event 0x1B, state 6 waits for the room
/// message (`Game_Session->field_126`), and state 7 -- reached once the save
/// has not already banked this clear (`Mc_SaveData.field_23`) -- applies the
/// room's two area records, raises the progression flags, refills the party
/// and hands off to the results screen with `Task_Spawn(0, 0x11, 0, 0)`.
/// States 0..6 share the `advance` tail that walks the task one state on;
/// `goto advance` from state 1 is the `acropolis_patio` idiom, and the
/// `do/while (0)` around the shared increment is this project's allocation
/// lever, not a loop: it weights the task pointer's references by loop depth
/// so it outranks the `Mc_SaveData` base and takes `$s0` instead of `$s1`.
void func_dryfield_dilapidated_house_8017E2B0(Task* task)
{
    switch (task->state) {
        case 0:
            func_800E8634((s32)&D_dryfield_dilapidated_house_80184EA0, 0, (s32)&D_dryfield_dilapidated_house_801855F0);
            task->state += 1;
            return;
        case 1:
            if (Game_Session->field_1 == 2) {
                Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0x1B);
                D_801153F1 = 3;
                goto advance;
            }
            return;
        case 2:
        case 3:
        case 4:
        case 5:
            task->state += 1;
            return;
        case 6:
            if (Game_Session->field_126 == 0) {
                return;
            }
        advance:
            do {
                task->state += 1;
            } while (0);
            return;
        case 7:
            if (Mc_SaveData.field_23 != 9) {
                Gp_ApplyAreaRecs(&D_dryfield_dilapidated_house_80189AA0);
                if (GameFlag_GetNibble(0xCE) != 0) {
                    Gp_ApplyAreaRecs(&D_dryfield_dilapidated_house_80189B24);
                }
                GameFlag_SetNibble(0x4B, 6);
                GameFlag_SetNibble(0x4C, 1);
                GameFlag_SetNibble(0x45, 1);
                GameFlag_SetNibble(0x62, 1);
                GameFlag_SetNibble(0x59, 1);
                GameFlag_SetNibble(0x5A, 2);
                GameFlag_SetNibble(3, 0);
                GameFlag_SetNibble(0x155, 0);
                Gp_FillPlayerHpMp();
                Gp_FillAllyHp();
                Mc_SaveData.field_5C5 = 1;
                Mc_SaveData.field_7   = 2;
                Mc_SaveData.field_8   = 1;
                Mc_SaveData.field_5   = 1;
                Mc_SaveData.field_6   = 8;
                D_80071076            = 1;
                Task_Spawn(0, 0x11, 0, 0);
            }
            Task_Kill(task);
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017E48C);

s32 func_dryfield_dilapidated_house_8017E56C(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017E574);

s32 func_dryfield_dilapidated_house_8017E684(void)
{
    return 0;
}

s32 func_dryfield_dilapidated_house_8017E68C(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    u8 temp_v1;

    temp_v1 = arg2->field_2;
    if ((temp_v1 == 1) && (D_dryfield_dilapidated_house_80183EFC == 0)) {
        D_dryfield_dilapidated_house_80183EFC = (s32)temp_v1;
        func_800E8634((s32)&D_dryfield_dilapidated_house_80184408, 0, (s32)&D_dryfield_dilapidated_house_80184C60);
    }
    return 0;
}

void func_dryfield_dilapidated_house_8017E6DC(Task* arg0)
{
    Task* temp_s1;
    s32   temp_a1;
    s32   temp_v1;

    temp_s1 = Game_GetPtrSlot(3);
    temp_a1 = Gp_LookupSlot4(1);
    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            arg0->spawnArg1 = 0;
            arg0->state    += 1;
            return;
        case 2:
            func_800B0928(temp_s1, (Task*)temp_a1, 0x200, 0x180, 0x1000);
            /* fallthrough */
        case 1:
            return;
    }
}

void func_dryfield_dilapidated_house_8017E780(Task* arg0)
{
    s32 temp_v0;
    s32 temp_v1;
    s32 var_a0;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            D_dryfield_dilapidated_house_80189B6C = arg0->spawnArg1;
            arg0->state                          += 1;
            return;
        case 1:
            var_a0 = (s32)(D_dryfield_dilapidated_house_80189B6C * 3) / (s32)arg0->spawnArg1;
            if (D_dryfield_dilapidated_house_80189B6C & 1) {
                var_a0 = -var_a0;
            }
            Display_ClampField126((s8)var_a0);
            temp_v0                               = D_dryfield_dilapidated_house_80189B6C - 1;
            D_dryfield_dilapidated_house_80189B6C = temp_v0;
            if (temp_v0 == 0) {
                Task_Kill(arg0);
            }
            return;
    }
}

void func_dryfield_dilapidated_house_8017E858(Task* arg0)
{
    s32 var_v0;

    var_v0 = arg0->spawnArg1;
    if (var_v0 < 0) {
        Stage_SetEndingFlag();
        Task_Kill(arg0);
        var_v0 = arg0->spawnArg1;
    }
    var_v0          = var_v0 - 1;
    arg0->spawnArg1 = var_v0;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D61C);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D628);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D634);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D640);
