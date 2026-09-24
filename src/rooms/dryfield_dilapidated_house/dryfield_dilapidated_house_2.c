#include "common.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/task.h"
#include "main/fs.h"
#include "actors/actor_136300.h"

/// Script pair handed to `Gp_SpawnScript18`. Both live in gameplay's image, so
/// the room overlay imports them by absolute address and passes them as `s32`.
extern s32 D_80114A24;
extern s32 D_80114A34;

extern TaskDesc D_dryfield_dilapidated_house_80183E64;
extern Task*    D_dryfield_dilapidated_house_801857E8;
extern TaskDesc D_dryfield_dilapidated_house_80186854;
extern Task*    D_dryfield_dilapidated_house_80189B7C;
/// Spawn argument the spawned task reads back; its address is also the
/// `Task_SpawnFromTable` arg, so the store and the call must stay ordered.
extern s16 D_dryfield_dilapidated_house_80189B80;
/// Declared as a one-element array rather than a bare `s16`: measured. With a
/// bare scalar, GCC 2.8.1's `fixed_scalar_and_varying_struct_p` concludes it
/// cannot alias the `Task*` store beside it in
/// `func_dryfield_dilapidated_house_8017E970` and swaps the two stores.
extern s16 D_dryfield_dilapidated_house_80189B82[1];

/// Shared in source with actor 136300: the spawn block, the latched countdown
/// and the entry the handler starts.
extern Actor136300Spawn D_dryfield_dilapidated_house_80189C94;
extern s16              D_dryfield_dilapidated_house_80189C98;
extern TaskDesc         D_dryfield_dilapidated_house_80183E48;

/// Message handler for the start-countdown cue; actor 136300 carries the same
/// body. A positive argument is latched
/// and nothing else happens; otherwise the CD command queue is dropped into
/// Mdec_DecodeToVram mode 2 and -- except for the -2 "already ran" message --
/// the spawn block is filled and the `D_dryfield_dilapidated_house_80183E48` entry started.
///
/// Both halves of the block are written in *each* arm of the countdown test so
/// that each arm is a complete two-store address session: jump optimization
/// then merges the identical tails and the countdown collapses to one `li` per
/// arm, which is what puts the block's `lui` in the delay slot of the entry
/// test. Hoisting `unk2` out of the arms compiles to a different allocation.
void func_dryfield_dilapidated_house_8017E8E8(s32 arg0)
{
    CdCmdQueue* queue;

    queue = &CdCmd_Queue;
    if (arg0 <= 0) {
        queue->field_22A = 2;
        if (arg0 != -2) {
            if (arg0 == 0) {
                D_dryfield_dilapidated_house_80189C94.field_0 = 0x64;
                D_dryfield_dilapidated_house_80189C94.field_2 = 0x100;
            } else {
                D_dryfield_dilapidated_house_80189C94.field_0 = 5;
                D_dryfield_dilapidated_house_80189C94.field_2 = 0x100;
            }
            Task_SpawnFromTable(&D_dryfield_dilapidated_house_80183E48, 0, 0, (s32)&D_dryfield_dilapidated_house_80189C94);
        }
    } else {
        D_dryfield_dilapidated_house_80189C98 = arg0;
    }
}

void func_dryfield_dilapidated_house_8017E970(s32 arg0)
{
    if (arg0 == 0) {
        D_dryfield_dilapidated_house_80189B7C->state = 0;
        D_dryfield_dilapidated_house_80189B82[0]     = 1;
        return;
    }
    D_dryfield_dilapidated_house_80189B7C->state     = 2;
    D_dryfield_dilapidated_house_80189B7C->spawnArg1 = arg0;
}

void func_dryfield_dilapidated_house_8017E9A4(s32 arg0)
{
    if (arg0 != 0) {
        Gp_SpawnScript18((s32)&D_80114A24, (s32)&D_80114A34);
        D_dryfield_dilapidated_house_80189B80 = arg0;
        Task_SpawnFromTable(&D_dryfield_dilapidated_house_80183E64, 0, 0,
                            (s32)&D_dryfield_dilapidated_house_80189B80);
        return;
    }
    D_dryfield_dilapidated_house_80189B82[0] = 1;
}

void func_dryfield_dilapidated_house_8017EA10(s32 arg0)
{
    if (arg0 != 0) {
        D_dryfield_dilapidated_house_801857E8 =
            Task_SpawnFromTable(&D_dryfield_dilapidated_house_80186854, 0, 3, (s32)gameGetPtrSlot(3));
        return;
    }
    if (D_dryfield_dilapidated_house_801857E8 != NULL) {
        taskKill(D_dryfield_dilapidated_house_801857E8);
        D_dryfield_dilapidated_house_801857E8 = NULL;
    }
}
