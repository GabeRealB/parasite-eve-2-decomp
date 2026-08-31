#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"

/// Task table spawned by `func_shelter_b2_laboratory_801801D0` the first time
/// the laboratory console is used.
extern TaskDesc D_shelter_b2_laboratory_80182A6C[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_5", func_shelter_b2_laboratory_801800FC);

/// Handler for slot-7 msg `0x13EF` in `D_shelter_b2_laboratory_80182A38`: the
/// directed action on the laboratory console (`field_2` 1). Runs the scripted
/// scene once, then replays cap script `6` on later visits.
s32 func_shelter_b2_laboratory_801801D0(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 1) {
        if (GameFlag_GetNibble(0x13D) != 0) {
            if (GameFlag_GetNibble(0xD0) < 2) {
                Task_SpawnFromTable(D_shelter_b2_laboratory_80182A6C, 0, 0, 0);
            } else {
                Gp_RunCapCmd1(6);
            }
        } else {
            Gp_RunCapCmd1(0x1E);
            GameFlag_SetNibble(0x13D, 1);
        }
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_5", func_shelter_b2_laboratory_8018025C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_5", func_shelter_b2_laboratory_80180290);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_5", func_shelter_b2_laboratory_80180350);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_5", func_shelter_b2_laboratory_80180450);
