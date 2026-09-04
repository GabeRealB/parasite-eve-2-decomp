#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

extern s32  Gp_LcgState;
extern void func_acropolis_bridge_801827EC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_8017F868);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_80180320);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_8018063C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_8018099C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_80180CC0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_80180FF0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_801812F4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_801819C8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_80181D28);

s32 func_acropolis_bridge_801820A0(Task* task)
{
    GsCOORDINATE2* coord;
    SVECTOR        pos;
    s32            i;

    coord = ((TmdObject*)task->extra)->field_8;

    i = 0;
    do {
        pos.vx      = -0x3E58;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vy      = ((u32)Gp_LcgState >> 16) % 1536 + 0xF830;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = (((u32)Gp_LcgState >> 16) & 0xF) + 0xF63C;
        Gp_SpawnEff(0x600BC, coord, 0, &pos);

        pos.vx      = -0x3E58;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vy      = ((u32)Gp_LcgState >> 16) % 1536 + 0xF830;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = 0xFA06 - (((u32)Gp_LcgState >> 16) & 0xF);
        Gp_SpawnEff(0x600BC, coord, 0, &pos);

        pos.vx      = -0x3E58;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vy      = (((u32)Gp_LcgState >> 16) & 0xF) + 0xF830;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = (u16)((u32)Gp_LcgState >> 16) % 970 + 0xF63C;
        Gp_SpawnEff(0x600BC, coord, 0, &pos);
        i++;
    } while (i < 0x20);

    i = 0;
    do {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vx      = -0x3E58;
        pos.vy      = ((u32)Gp_LcgState >> 16) % 1536 - 0x7D0;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = (u16)((u32)Gp_LcgState >> 16) % 970 - 0x9C4;
        Gp_SpawnEff(0x600BC, coord, 0, &pos);
        i++;
    } while (i < 8);

    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_80182394);

void func_acropolis_bridge_80182694(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    if (Gp_State1C->field_4 != 0) {
        func_acropolis_bridge_801827EC(coord, (s16)work->field_26, (s16)work->field_24);
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->field_22++;
        switch (task->state) {
            case 0:
                work->field_24 = 0x40;
                work->field_26 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
                coord->flg  = 0;
                task->state = 1;
                /* fallthrough */
            case 1:
                work->field_26 += 0x20;
                func_acropolis_bridge_801827EC(coord, (s16)work->field_26, (s16)work->field_24);
                if ((s16)work->field_24 >= 3) {
                    work->field_24 -= 2;
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_801827EC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_80182AF8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_80182F8C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", func_acropolis_bridge_801833A0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", D_acropolis_bridge_8017D6CC);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_8", D_acropolis_bridge_8017D6E8);
