#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/task.h"

void func_dryfield_night_gas_station_80180D1C(void);
void func_dryfield_night_gas_station_80180DC8(s16 arg0);

extern TaskFuncTable3 D_dryfield_night_gas_station_8017D644;

/// Gates the room's two sprite records on nibble 0x8D, then dispatches the task
/// through the room's own three-state table, copied onto the stack first.
void func_dryfield_night_gas_station_8017FB70(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_gas_station_8017D644;
    func_dryfield_night_gas_station_80180D1C();
    sp.funcs[arg0->state](arg0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_3", func_dryfield_night_gas_station_8017FBD4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_3", func_dryfield_night_gas_station_8017FD80);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_3", func_dryfield_night_gas_station_801802EC);

/// Bursts the room's lamp effect: `arg0` picks one of three spawn-argument
/// triples and the effect's `arg2`, the spark is spawned at the lamp task's own
/// coordinate, and the lamp is then told to light up. Any other `arg0` only
/// switches the lamp back to dark.
void func_dryfield_night_gas_station_80180604(s32 arg0)
{
    GpWorkObj*     work;
    GsCOORDINATE2* coord;
    SVECTOR        offset;

    work = Gp_FindWorkById(Game_Session->field_6 | ((Game_Session->field_7 << 8) | 0x2000));
    if (work != NULL) {
        coord = ((TmdObject*)((Task*)work->field_0)->extra)->field_8;
        switch (arg0) {
            case 0:
                offset.vx = 0;
                offset.vy = -0x64;
                offset.vz = -0x12C;
                Gp_SpawnEff(0x600E0, coord, 0x300, &offset);
                func_dryfield_night_gas_station_80180DC8(1);
                break;

            case 1:
                offset.vx = 0xC8;
                offset.vy = -0x64;
                offset.vz = -0xC8;
                Gp_SpawnEff(0x600E0, coord, 0x200, &offset);
                func_dryfield_night_gas_station_80180DC8(1);
                break;

            case 2:
                offset.vx = -0x64;
                offset.vy = -0x64;
                offset.vz = -0xC8;
                Gp_SpawnEff(0x600E0, coord, 0x200, &offset);
                func_dryfield_night_gas_station_80180DC8(1);
                break;

            default:
                func_dryfield_night_gas_station_80180DC8(0);
                break;
        }
    }
}
