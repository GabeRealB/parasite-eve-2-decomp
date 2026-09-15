#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "main/tmd.h"

extern SVECTOR D_neo_ark_observatory_80181368;
extern s16     D_neo_ark_observatory_8018136A; // D_neo_ark_observatory_80181368.vy

void func_neo_ark_observatory_8017FE34(GsCOORDINATE2* coord, SVECTOR* offset);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_2", func_neo_ark_observatory_8017F44C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_2", func_neo_ark_observatory_8017F588);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_2", func_neo_ark_observatory_8017F6F8);

/// Aims the observatory's model at slot 0xA when one is resident, otherwise at
/// the slot-3 game pointer, and publishes the y of the vertex offset
/// `func_neo_ark_observatory_8017FE34` adds to the transformed model: 0 while
/// flag 0xD7 is set, 10000 while it is clear or no slot-0xA model is resident.
void func_neo_ark_observatory_8017FA98(void)
{
    Task* task;
    Task* slotA;

    task  = Game_GetPtrSlot(0xA);
    slotA = task;
    if (task == NULL) {
        task = Game_GetPtrSlot(3);
    }
    if (slotA != NULL && GameFlag_GetNibble(0xD7) != 0) {
        D_neo_ark_observatory_8018136A = 0;
    } else {
        D_neo_ark_observatory_8018136A = 0x2710;
    }
    func_neo_ark_observatory_8017FE34(((TmdObject*)task->extra)->field_8, &D_neo_ark_observatory_80181368);
}

void func_neo_ark_observatory_8017FB1C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x300, 0);
            Gp_SpawnIfCapIdle(task->spawnArg1, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            task->state++;
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            Task_Kill(task);
            break;
    }
}
