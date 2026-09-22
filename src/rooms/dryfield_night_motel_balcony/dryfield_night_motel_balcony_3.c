#include "common.h"

#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/wipsys.h"
#include "psyq/libgpu.h"

extern s8       D_8007106B;
extern TaskDesc D_dryfield_night_motel_balcony_80182834;

extern u8** D_dryfield_night_motel_balcony_80182C3C[];

extern void func_dryfield_night_motel_balcony_8017E250(s16 arg0, s16 arg1);

void func_dryfield_night_motel_balcony_8017E0C8(Task* arg0)
{
    Display_SpawnWithOt(&D_dryfield_night_motel_balcony_80182834, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    SndEvt_EnqueueType7(0x60010001, 1);
    taskKill(arg0);
}

void func_dryfield_night_motel_balcony_8017E128(u8 arg0)
{
    GameSession* g    = gGameSession;
    GpAreaKey*   sess = &g->at4.loc;
    GpSprtRec*   rec;
    GpSprtCmd*   cmd;

    rec = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1];

    switch (sess->view) {
        case 17:
            cmd = rec[16].field_4;
            if (arg0 == 0) {
                cmd[2].field_4 = 1;
            } else {
                cmd[3].field_4 = 1;
            }
            break;
        case 18:
            cmd             = rec[17].field_4;
            cmd[6].field_4  = 0;
            cmd[7].field_4  = 0;
            cmd[8].field_4  = 0;
            cmd[9].field_4  = 0;
            cmd[10].field_4 = 0;
            break;
        case 19:
            cmd = rec[18].field_4;
            if (arg0 == 0) {
                cmd[2].field_4 = 0;
            } else {
                cmd[1].field_4 = 0;
            }
        case 22:
            cmd = rec[21].field_4;
            if (arg0 == 1) {
                cmd[1].field_4 = 0;
                cmd[2].field_4 = 1;
            } else if (arg0 == 2) {
                cmd[1].field_4 = 1;
                cmd[2].field_4 = 0;
            } else {
                cmd[2].field_4 = 1;
                cmd[1].field_4 = 1;
            }
            break;
    }
}

void func_dryfield_night_motel_balcony_8017E250(s16 arg0, s16 arg1)
{
    GpAreaKey* sess;
    GpSprtRec* rec;
    GpSprtCmd* cmd;
    u8*        p;

    p    = D_dryfield_night_motel_balcony_80182C3C[arg0][arg1];
    sess = &gGameSession->at4.loc;
    rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    cmd  = rec[p[0]].field_4;
    if (p[0] != 0xFF) {
        do {
            if (p[1] == 0xFF) {
                cmd = rec[p[0]].field_4;
                p  += 2;
            }
            cmd[p[0]].field_4 = p[1];
            p                += 2;
        } while (p[0] != 0xFF);
    }
    switch (arg0) {
        case 0:
            GameFlag_SetNibble(0x85, arg1);
            break;
        case 1:
            GameFlag_SetNibble(0x86, arg1);
            break;
        case 2:
            GameFlag_SetNibble(0x87, arg1);
            break;
        case 3:
            GameFlag_SetNibble(0x88, arg1);
            break;
        case 4:
            GameFlag_SetNibble(0x89, arg1);
            break;
        case 5:
            GameFlag_SetNibble(0x8A, arg1);
            break;
        case 6:
            GameFlag_SetNibble(0x8B, arg1);
            break;
        case 7:
            GameFlag_SetNibble(0x8C, arg1);
            break;
        case 8:
            GameFlag_SetNibble(0x8D, arg1);
            break;
    }
}

void func_dryfield_night_motel_balcony_8017E3C8(void)
{
    func_dryfield_night_motel_balcony_8017E250(0, GameFlag_GetNibble(0x85));
    func_dryfield_night_motel_balcony_8017E250(1, GameFlag_GetNibble(0x86));
    func_dryfield_night_motel_balcony_8017E250(2, GameFlag_GetNibble(0x87));
    func_dryfield_night_motel_balcony_8017E250(3, GameFlag_GetNibble(0x88));
    func_dryfield_night_motel_balcony_8017E250(4, GameFlag_GetNibble(0x89));
    func_dryfield_night_motel_balcony_8017E250(5, GameFlag_GetNibble(0x8A));
    func_dryfield_night_motel_balcony_8017E250(6, GameFlag_GetNibble(0x8B));
    func_dryfield_night_motel_balcony_8017E250(7, GameFlag_GetNibble(0x8C));
    func_dryfield_night_motel_balcony_8017E250(8, GameFlag_GetNibble(0x8D));
}

void func_dryfield_night_motel_balcony_8017E4B8(void)
{
    GameSession* g    = gGameSession;
    GpAreaKey*   sess = &g->at4.loc;
    GpSprtRec*   rec;
    GpSprtCmd*   cmd;

    rec = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1];

    cmd            = rec[16].field_4;
    cmd[2].field_4 = 0;
    cmd[3].field_4 = 0;

    cmd             = rec[17].field_4;
    cmd[6].field_4  = 1;
    cmd[7].field_4  = 1;
    cmd[8].field_4  = 1;
    cmd[9].field_4  = 1;
    cmd[10].field_4 = 1;

    cmd            = rec[18].field_4;
    cmd[1].field_4 = 1;
    cmd[2].field_4 = 1;

    cmd            = rec[21].field_4;
    cmd[1].field_4 = 1;
    cmd[2].field_4 = 1;
}
