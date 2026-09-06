#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/rooms_shared_80181f58.h"

#include <psyq/libgte.h>

extern s32 D_80115728;
extern s32 Gp_LcgState;

void RoomsShared80181f58(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            ang;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->field_22++;
        if (mem->field_22 >= 0x15) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        }
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        ang           = mem->field_24 + ((((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200);
        mem->field_24 = ang;
        mem->field_10 = (u32)(rcos(ang) * 3) >> 4;
        mem->field_12 = -mem->field_22 * 128;
        mem->field_14 = (u32)(rsin(mem->field_24) * 3) >> 4;
        Gp_SpawnEff(D_80115728, coord, 0x30080201, (SVECTOR*)&mem->field_10);
    }
}
