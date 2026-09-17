#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_water_tank.h"

extern TaskDesc       D_dryfield_water_tank_8017F34C;
extern s32            D_dryfield_water_tank_8017F114;
extern s32            D_dryfield_water_tank_8017F21C;
extern GpMsgEntry     D_dryfield_water_tank_8017FD90[];
extern s32            D_dryfield_water_tank_80184E0C;
extern s32            D_dryfield_water_tank_801859DC;
extern GpAreaApplyRec D_dryfield_water_tank_80188D1C[];
extern Task*          D_dryfield_water_tank_80188D4C;

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

/// Drives the model task the room's script spawns: state 0 allocates the
/// light/colour matrix pair for the task's `TmdObject` and reparents the task
/// to the script driver, state 1 idles, and state 2 waits for
/// `func_dryfield_water_tank_8017DB98` to report the model finished. Every
/// frame it hands the model part's translation to `func_800D7A9C`, which turns
/// it into the light/colour matrices.
void func_dryfield_water_tank_8017DD20(Task* arg0)
{
    TmdObject*     extra;
    GsCOORDINATE2* coord;
    DwtColorMtx*   mtx;
    TmdObject*     mdl;
    VECTOR         pos;

    switch (arg0->state) {
        case 0:
            extra       = (TmdObject*)arg0->extra;
            coord       = extra->field_8;
            mtx         = (DwtColorMtx*)Mem_Malloc(0x58, 0);
            arg0->idMap = (TaskIdMap*)mtx;
            if (mtx == NULL) {
                Task_Kill(arg0);
            } else {
                Mem_Set(mtx, 0, 0x58);
                mtx->owner     = (Task*)Game_GetPtrSlot(3);
                coord->sub     = &Gfx_ViewCoord;
                extra->field_C = 0x80;
                Tmd_AllocBuffers(extra);
                extra->field_1C = &mtx->light;
                extra->field_20 = &mtx->color;
                arg0->field_24  = D_dryfield_water_tank_8017FD90;
                Task_Reparent(D_dryfield_water_tank_80188D4C, arg0);
            }
            arg0->state += 1;
            break;
        case 1:
            break;
        case 2:
            if (func_dryfield_water_tank_8017DB98(arg0) & 0xFFFF) {
                arg0->state = 1;
            }
            break;
    }

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->field_8->workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

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
