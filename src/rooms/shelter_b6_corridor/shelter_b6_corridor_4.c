#include "common.h"

#include "gameplay/3CD8.h"

#include "main/task.h"
#include "main/tmd.h"

extern s32 D_80070F70;
extern u32 Gp_LcgState;

void func_shelter_b6_corridor_8017EBA4(Task* task)
{
    GsCOORDINATE2* coord;
    u8             rgb[3];
    u32            shade;

    coord = ((TmdObject*)task->extra)->coords + 1;
    if (Gp_State1C->eventState == 0) {
        shade  = ((D_80070F70 & 1) << 4) + 0x40;
        rgb[0] = shade;
        rgb[1] = shade;
        rgb[2] = shade >> 1;
        Gp_DrawRing(coord, 0x200, rgb);
        Gp_DrawRing(coord, 0x400, rgb);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (((Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x600E0, ((TmdObject*)task->extra)->coords + (((Gp_LcgState >> 16) & 0xF) + 3), 0x10080, NULL);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_corridor/shelter_b6_corridor_4", func_shelter_b6_corridor_8017ECA8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_corridor/shelter_b6_corridor_4", func_shelter_b6_corridor_8017EE08);
