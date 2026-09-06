#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1A8.h"
#include "gameplay/gameplay.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/unknown_syms.h"
#include "rooms/acropolis_fire_escape.h"

extern TaskFuncTable3 RoomsShared8017d878Table;
extern s8             D_8007272D;
extern GpMsgEntry     D_acropolis_fire_escape_80181D3C[];
extern TaskDesc       D_acropolis_fire_escape_80181D64;
extern GpObj4A        D_acropolis_fire_escape_801826A8;
extern s32            D_acropolis_fire_escape_80183040;

void func_acropolis_fire_escape_8017FF7C(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    switch (task->state) {
        case 0:
            work->field_10.vx = 0xB58;
            work->field_10.vy = -0x822;
            work->field_10.vz = -0xE5;
            Gp_SpawnEff(0x6008C, coord, 0x42000, &work->field_10);
            task->state = task->state + 1;
            break;
        case 1:
            if (Gp_State1C->field_4 < 4) {
                if ((u8)Game_Session->field_4 == 3) {
                    work->field_10.vx = 0x48F;
                    work->field_10.vy = -0x391;
                    work->field_10.vz = 0x686;
                    Gp_SpawnEff(0x6004F, coord, 0x60E, &work->field_10);
                }
                if ((u8)Game_Session->field_4 == 8) {
                    work->field_10.vx = 0x48F;
                    work->field_10.vy = -0x391;
                    work->field_10.vz = 0x686;
                    Gp_SpawnEff(0x6004F, coord, 0x8000030E, &work->field_10);
                }
                if ((u8)Game_Session->field_4 == 6) {
                    work->field_10.vx = -0xC1F;
                    work->field_10.vy = -0xD10;
                    work->field_10.vz = 0x8E0;
                    Gp_SpawnEff(0x6004F, coord, 0x10408, &work->field_10);
                }
                if ((u8)Game_Session->field_4 == 9) {
                    work->field_10.vx = -0xC1F;
                    work->field_10.vy = -0xD10;
                    work->field_10.vz = 0x8E0;
                    Gp_SpawnEff(0x6004F, coord, 0x80010208, &work->field_10);
                }
            }
            break;
    }
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape_7", func_acropolis_fire_escape_80180154);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape_7", func_acropolis_fire_escape_80180B20);
