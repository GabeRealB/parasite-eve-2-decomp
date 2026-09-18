#include "common.h"

#include "rooms/dryfield_water_tower.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "main/session.h"

/// Message 0x7DB handler, the room script table `D_dryfield_water_tower_80181B00`
/// lists beside its `Room_Util08` 0x7D4 entry. It rests the cap script: the
/// three halfword slots it keeps its timers in are cleared along with the
/// task's kill countdown, and the payload's halfword becomes the task's state,
/// so the 0x7DB sender picks the state the cap script resumes in. The opcode
/// itself is never read, hence the named-but-unused `msgId`.
void func_dryfield_water_tower_8017F808(Task* task, s32 msgId, DwtwMsg7DB* msg)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)task->work;

    state->field_58     = 0;
    state->field_60     = 0;
    state->field_5A     = 0;
    task->state         = msg->field_2;
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
/// raises bit 0x80 of `Game_Session->field_69` and drops bit 0x40, releases one
/// ref of the slot-4 game object, and sets the latch.
/// `func_shelter_b3_dumping_hole_801818E0` runs the same latch / release /
/// `|= 0x80` sequence for its room.
void func_dryfield_water_tower_8017F82C(void)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)D_dryfield_water_tower_801876A4->work;

    if (state->field_76 == 0) {
        Game_Session->field_69 |= 0x80;
        Game_Session->field_69 &= 0xBF;
        Gp_ReleaseStateF0Add((GpObj20E*)Gp_LookupSlot4(0), 1);
        state->field_76 = 1;
    }
}
