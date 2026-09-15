#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_water_tank.h"

extern TaskDesc       D_dryfield_water_tank_8017F34C;
extern s32            D_dryfield_water_tank_8017F114;
extern s32            D_dryfield_water_tank_8017F21C;
extern s32            D_dryfield_water_tank_80184E0C;
extern s32            D_dryfield_water_tank_801859DC;
extern GpAreaApplyRec D_dryfield_water_tank_80188D1C[];

void func_dryfield_water_tank_8017DB48(void)
{
    switch (GameFlag_GetNibble(0x55)) {
        case 0:
        case 1:
        case 2:
            func_dryfield_water_tank_8017EFF4(1);
            break;
        case 3:
            func_dryfield_water_tank_8017EFF4(0);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017DB98);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017DD20);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017DEA4);

/// Hides the task's `TmdObject` (bit 0x80 of `field_C`) while `arg2` is zero,
/// and clears that bit otherwise. `arg1` is unused; the flag is the *third*
/// argument, so the second slot is only there to place it in `$a2`. Byte for
/// byte the actors library's `ActorsShared801346ec`, which toggles the same bit
/// of the same field for the model of the task it is handed.
void func_dryfield_water_tank_8017E0B4(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)task->extra;
    if (arg2 != 0) {
        obj->field_C = obj->field_C & 0xFF7F;
        return;
    }
    obj->field_C = obj->field_C | 0x80;
}
