#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"

extern s8  D_8007218B;
extern s32 D_80133898;
extern s32 D_801341E0;
extern s32 D_dryfield_night_r08_80180544;

void func_dryfield_night_r08_8017D630(Task* arg0)
{
    arg0->msgTable = &D_dryfield_night_r08_80180544;
    Game_SetPtrSlot(arg0, 7);
    Gp_SetStreamBuf((u8*)D_8005C370 + 0x20000);
    if (D_8007218B != 9) {
        func_800E8634((s32)&D_80133898, 0, (s32)&D_801341E0);
    }
    arg0->state = (s32)(arg0->state + 1);
}
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_r08/dryfield_night_r08", RoomsShared8017d878Table);
