#include "common.h"

#include "decomp/common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"

extern TaskDesc D_acropolis_square_80183808;
extern s32      D_acropolis_square_80183B98;
extern s32      D_acropolis_square_80183B58;
extern s32      D_acropolis_square_80183830;
extern s32      D_acropolis_square_8018399C;
extern s32      D_acropolis_square_80183A5C;

s32 func_acropolis_square_80182360(void)
{
    GpAreaKey key;

    if (GameFlag_GetNibble(0x1F) == 0) {
        GameFlag_SetNibble(0x1F, 1);
        key.field_3 = 1;
        key.field_2 = 1;
        Gp_SetAreaObjId(&key, 2, 1);
        Game_Session->field_1 = 1;
        Task_SpawnFromTable(&D_acropolis_square_80183808, 0, 0, 0);
        return 0;
    }
    return 1;
}

void func_acropolis_square_801823DC(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = task->spawnArg2;
    switch (task->state) { /* irregular */
        case 0:
            task->field_24 = &D_acropolis_square_80183B58;
            Game_SetPtrSlot(task, 5);
            D_acropolis_square_80183B98 = 0;
            Task_Spawn(1, 0x25, 0, 0);
            Task_Spawn(1, 0x25, 1, 0);
            task->state++;
            return;
        case 1:
            if ((0x268 >> ((u8)Game_Session->field_4 - 1)) & 1) {
                work->field_10.vx = 0x19AA;
                work->field_10.vy = -0xF96;
                work->field_10.vz = 0x8DE;
                Gp_SpawnEff(0x60047, coord, D_acropolis_square_80183B98 * 0x10000218 + 0x10E08,
                            &work->field_10);
            }
            if ((u8)Game_Session->field_4 == 0xE) {
                work->field_10.vx = 0x18D2;
                work->field_10.vy = -0x100B;
                work->field_10.vz = 0x8AB;
                Gp_SpawnEff(0x60047, coord, D_acropolis_square_80183B98 * 0x218 + 0x10010608,
                            &work->field_10);
            }
            if ((u8)Game_Session->field_4 == 9) {
                work->field_10.vx = 0x19AA;
                work->field_10.vy = -0xF96;
                work->field_10.vz = 0x8E8;
                Gp_SpawnEff(0x60047, coord, D_acropolis_square_80183B98 * 0x118 + 0x80010308,
                            &work->field_10);
            }
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_7", func_acropolis_square_801825DC);

s32 func_acropolis_square_8018344C(s32 arg0, s32 arg1, s32 arg2)
{
    D_acropolis_square_80183B98 = arg2;
    return 0;
}

void func_acropolis_square_8018345C(void)
{
}
