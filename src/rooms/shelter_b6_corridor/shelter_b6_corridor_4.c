#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"

#include "main/task.h"
#include "main/tmd.h"

extern s32 D_80070F70;
extern u32 Gp_LcgState;
extern s32 D_shelter_b6_corridor_801851B8;

void func_shelter_b6_corridor_8017EBA4(Task* task)
{
    GsCOORDINATE2* coord;
    u8             rgb[3];
    u32            shade;

    coord = ((TmdObject*)task->extra)->coords + 1;
    if (Gp_State1C->eventState == 0) {
        shade  = ((D_80070F70 & 1) << 4) + 0x40;
        rgb[0] = shade;
        rgb[1] = shade;
        rgb[2] = shade >> 1;
        Gp_DrawRing(coord, 0x200, rgb);
        Gp_DrawRing(coord, 0x400, rgb);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (((Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x600E0, ((TmdObject*)task->extra)->coords + (((Gp_LcgState >> 16) & 0xF) + 3), 0x10080, NULL);
        }
    }
}

void func_shelter_b6_corridor_8017ECA8(Task* task)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            eventState;
    u8             rgb[3];

    mem        = task->spawnArg2;
    eventState = Gp_State1C->eventState;
    coord      = ((TmdObject*)task->extra)->coords;
    if (eventState != 0) {
        if (eventState < 4) {
            return;
        }
        goto release;
    }
    mem->age++;
    if (task->state == 0) {
        mem->scale = 0xC0;
        mem->angle = 0x200;
        D_shelter_b6_corridor_801851B8++;
        task->state     = 1;
        task->spawnArg1 = D_shelter_b6_corridor_801851B8;
    }
    if (task->spawnArg1 != D_shelter_b6_corridor_801851B8) {
        goto release;
    }
    rgb[0]      = mem->scale;
    rgb[1]      = mem->scale;
    rgb[2]      = (u16)mem->scale >> 1;
    mem->angle += 0x18;
    Gp_DrawArc(coord, (s16)(mem->angle * 2), 0, rgb);
    Gp_DrawRing(coord, (s16)((u16)mem->angle * 4), rgb);
    if (mem->age < 9) {
        return;
    }
    mem->scale -= 0x18;
    if (mem->scale < 0x18) {
    release:
        Gp_ReleaseState1CMem(mem, task);
    }
}

void func_shelter_b6_corridor_8017EE08(s32 arg0, s32 arg1)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtRec* rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    GpSprtCmd* cmd;
    s32        run = arg0 & 0xFF;
    s32        flag;

    if (run == 0) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            cmd            = rec[1].field_4;
            cmd[1].field_4 = 1;
            return;
        }
        if (flag == 1) {
            cmd            = rec[1].field_4;
            cmd[1].field_4 = 0;
            return;
        }
    } else if (run == 1) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            cmd            = rec[1].field_4;
            cmd[2].field_4 = run;
            cmd            = rec[2].field_4;
            cmd[2].field_4 = run;
            return;
        }
        if (flag == run) {
            cmd            = rec[1].field_4;
            cmd[2].field_4 = 0;
            cmd            = rec[2].field_4;
            cmd[2].field_4 = 0;
            return;
        }
    } else if (run == 2) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            cmd            = rec[2].field_4;
            cmd[1].field_4 = 1;
            return;
        }
        if (flag == 1) {
            cmd            = rec[2].field_4;
            cmd[1].field_4 = 0;
        }
    }
}
