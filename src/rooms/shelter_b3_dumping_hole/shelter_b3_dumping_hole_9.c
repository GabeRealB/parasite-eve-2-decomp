#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "rooms/room_common.h"
#include <psyq/libgs.h>

typedef struct {
    u8             pad0[8];
    GsCOORDINATE2* field_8;
    u8             pad_C[0x16];
    u16            field_22;
} ClumpMem;

void func_shelter_b3_dumping_hole_80186D4C(Task* arg0)
{
    ClumpMem*      mem;
    GsCOORDINATE2* coord;
    MATRIX*        m;
    s32            i;

    mem   = (ClumpMem*)arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_4 != 0) {
        Room_Draw41(coord, ((s16)mem->field_22 / 2) & 0xFFFF, 0x380);
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        m                            = &coord->coord;
        coord->sub                   = mem->field_8;
        *(s32*)&coord->coord.m[0][0] = 0x1000;
        *(s32*)&m->m[0][2]           = 0;
        *(s32*)&m->m[1][1]           = 0x1000;
        *(s32*)&m->m[2][0]           = 0;
        m->m[2][2]                   = 0x1000;
        coord->coord.t[2]            = 0;
        coord->coord.t[1]            = 0;
        coord->coord.t[0]            = 0;
        coord->flg                   = 0;
        Gp_UpdateCoord(coord);
        arg0->state = 1;
    }
    mem->field_22 += 1;
    switch (arg0->spawnArg1) {
        case 0:
            Gp_SpawnEff(0x6019A, coord, 0x14002400, NULL);
            arg0->spawnArg1 = 1;
            return;
        case 1:
            Room_Draw41(coord, ((s16)mem->field_22 / 2) & 0xFFFF, 0x380);
            if (!(mem->field_22 & 1)) {
                Gp_SpawnEff(0x6019A, coord, 0x1001400, NULL);
            }
            mem->field_22 += 1;
            return;
        case 2:
            Gp_SpawnEff(0x6019A, coord, 0x10002380, NULL);
            for (i = 0; i < 4; i++) {
                Gp_SpawnEff(0x6019A, coord, 0x2002400, NULL);
                Gp_SpawnEff(0x60199, coord, 0x2202300, NULL);
            }
            arg0->spawnArg1 = 3;
            return;
        case 3:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}
