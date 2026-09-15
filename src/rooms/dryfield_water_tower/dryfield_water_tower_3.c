#include "common.h"

#include "rooms/dryfield_water_tower.h"

/// Message 0x7DB handler, the room script table `D_dryfield_water_tower_80181B00`
/// lists beside its `Room_Util08` 0x7D4 entry. It rests the cap script: the
/// three halfword slots it keeps its timers in are cleared along with the
/// task's kill countdown, and the payload's halfword becomes the task's state,
/// so the 0x7DB sender picks the state the cap script resumes in. The opcode
/// itself is never read, hence the named-but-unused `msgId`.
void func_dryfield_water_tower_8017F808(Task* task, s32 msgId, DwtwMsg7DB* msg)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)task->idMap;

    state->field_58     = 0;
    state->field_60     = 0;
    state->field_5A     = 0;
    task->state         = msg->field_2;
    task->killCountdown = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_3", func_dryfield_water_tower_8017F82C);
