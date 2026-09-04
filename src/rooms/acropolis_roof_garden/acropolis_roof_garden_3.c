#include "common.h"

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_3", func_acropolis_roof_garden_8017DCDC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_3", func_acropolis_roof_garden_8017DE90);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_3", func_acropolis_roof_garden_8017E29C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_3", func_acropolis_roof_garden_8017F10C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_3", func_acropolis_roof_garden_8017F560);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_3", func_acropolis_roof_garden_8017F870);

INCLUDE_ASM("rooms/nonmatchings/acropolis_roof_garden/acropolis_roof_garden_3", func_acropolis_roof_garden_8017FA14);

/// Item-pickup model task step: the item's mesh is only visible from views 5
/// through 7, and stays hidden once the item's 2-bit flag reads 2 (already
/// taken). The three hidden cases are written as separate tests so the two view
/// comparisons are not folded into one unsigned range check.
void func_acropolis_roof_garden_80180160(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;
    s32         view;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = (TmdObject*)task->extra;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    view = Gp_GetViewIndex();
    if (view >= 8) {
        tmd->field_C = 0x80;
    } else if (view < 5) {
        tmd->field_C = 0x80;
    } else if (flag == 2) {
        tmd->field_C = 0x80;
    } else {
        tmd->field_C = 8;
        tmd->field_E = 0;
    }
}
