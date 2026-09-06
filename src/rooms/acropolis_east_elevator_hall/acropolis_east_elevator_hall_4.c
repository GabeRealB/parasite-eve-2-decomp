#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_east_elevator_hall.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

extern void func_807245E4(void*);
extern void func_80724608(void*, s32, s32, void*);

extern s32     D_acropolis_east_elevator_hall_8017D5E0;
extern SVECTOR D_acropolis_east_elevator_hall_8017D5E8;
extern s32     D_acropolis_east_elevator_hall_80185C8C;
extern s32     D_acropolis_east_elevator_hall_80185D54;
extern s32     D_acropolis_east_elevator_hall_801860B4;
extern s32     D_acropolis_east_elevator_hall_8018621C;
extern s32     D_acropolis_east_elevator_hall_801862F4;
extern s32     D_acropolis_east_elevator_hall_8018631C;

void func_acropolis_east_elevator_hall_8017F5B4(Task* task)
{
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)task->extra)->field_8;
    switch (task->state) {
        case 0:
            Task_Spawn(1, 0x25, 0, 0);
            Task_Spawn(1, 0x25, 1, 0);
            task->state++;
            /* fallthrough */
        case 1:
            if ((u8)Game_Session->field_4 == 2) {
                SVECTOR vec = D_acropolis_east_elevator_hall_8017D5E8;

                Gp_SpawnEff(0x60022, coord, 0xC03, &vec);
                vec.vx = 0x1600;
                vec.vy = -0x985;
                vec.vz = 0x55;
                Gp_SpawnEff(0x60022, coord, 0xC03, &vec);
                vec.vx = 0x1600;
                vec.vy = -0xA81;
                vec.vz = -0x1CA;
                Gp_SpawnEff(0x60022, coord, 0x1204, &vec);
                vec.vx = 0x1600;
                vec.vy = -0xA93;
                vec.vz = -0x61C;
                Gp_SpawnEff(0x60022, coord, 0x1204, &vec);
                vec.vx = 0x1600;
                vec.vy = -0x460;
                vec.vz = -0x1A1;
                Gp_SpawnEff(0x60022, coord, 0x1204, &vec);
                vec.vx = 0x1600;
                vec.vy = -0x449;
                vec.vz = -0x635;
                Gp_SpawnEff(0x60022, coord, 0x1204, &vec);
            }
            break;
    }
}
