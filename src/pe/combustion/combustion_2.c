#include "common.h"

#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("pe/nonmatchings/combustion/combustion_2", func_combustion_8012FF0C);

INCLUDE_ASM("pe/nonmatchings/combustion/combustion_2", func_combustion_80130184);

INCLUDE_ASM("pe/nonmatchings/combustion/combustion_2", func_combustion_801305F8);

void func_combustion_801308E0(Task* arg0)
{
    GsCOORDINATE2* coord;

    if (arg0->state != 0) {
        Gp_ReleaseState1CMem(arg0->spawnArg2, arg0);
        return;
    }
    coord = ((TmdObject*)arg0->extra)->field_8;
    Gp_UpdateCoord(coord);
    Gp_SpawnEff(0x8006001B, coord, 1, 0);
    Gp_SpawnEff(0x8006001B, coord, -1, 0);
    arg0->state = arg0->state + 1;
}
