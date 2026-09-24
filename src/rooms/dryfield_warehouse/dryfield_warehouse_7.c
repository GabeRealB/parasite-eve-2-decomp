#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_warehouse.h"

/// Hands the warehouse's cutscene task the script step `arg0` to run,
/// starting from its first sub-step.
void func_dryfield_warehouse_8017E3F4(s16 arg0)
{
    DwhWork* work = (DwhWork*)D_dryfield_warehouse_801821BC->work;

    work->field_4 = arg0;
    work->field_6 = 0;
}
