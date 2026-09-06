#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"

extern SVECTOR D_neo_ark_observatory_80181434[];
extern SVECTOR D_neo_ark_observatory_801814E4[];
extern SVECTOR D_neo_ark_observatory_801814F4[];
extern SVECTOR D_neo_ark_observatory_801814FC[];
extern SVECTOR D_neo_ark_observatory_8018150C[];
extern SVECTOR D_neo_ark_observatory_8018151C[];
extern SVECTOR D_neo_ark_observatory_80181524[];
extern SVECTOR D_neo_ark_observatory_80181564[];
extern SVECTOR D_neo_ark_observatory_80181574[];
extern SVECTOR D_neo_ark_observatory_8018157C[];
extern s16     D_neo_ark_observatory_80187A3C;

void Room_Draw13(SVECTOR* v, s32 arg1, s32 arg2);
void func_neo_ark_observatory_80180534(SVECTOR* v, s32 arg1, s16 arg2, s32 arg3);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_4", func_neo_ark_observatory_8017FE34);

void func_neo_ark_observatory_80180124(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_neo_ark_observatory_80187A3C = 0;
        task->state                    = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            Room_Draw13(&D_neo_ark_observatory_801814E4[0], 0x280, 0x444);
            break;
        case 3: {
            SVECTOR* p;
            p = D_neo_ark_observatory_801814F4;
            Room_Draw13(&p[0], 0x280, 0x444);
            Room_Draw13(&p[1], 0x280, 0x444);
            break;
        }
        case 4:
        case 16: {
            SVECTOR* p;
            p = D_neo_ark_observatory_801814FC;
            Room_Draw13(&p[0], 0x280, 0x444);
            Room_Draw13(&p[1], 0x280, 0x444);
            Room_Draw13(&p[2], 0x280, 0x333);
            Room_Draw13(&p[3], 0x280, 0x222);
            break;
        }
        case 5:
        case 17: {
            SVECTOR* p;
            p = D_neo_ark_observatory_8018150C;
            Room_Draw13(&p[0], 0x280, 0x444);
            Room_Draw13(&p[1], 0x280, 0x444);
            break;
        }
        case 6:
        case 18:
            Room_Draw13(&D_neo_ark_observatory_8018151C[0], 0x200, 0x444);
            break;
        case 7: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181434;
            func_neo_ark_observatory_80180534(&p[0], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[2], 0x400, D_neo_ark_observatory_80187A3C, 0xC);
            func_neo_ark_observatory_80180534(&p[4], 0x400, D_neo_ark_observatory_80187A3C, 8);
        }
            /* fallthrough */
        case 19: {
            SVECTOR* p;
            p = D_neo_ark_observatory_8018151C;
            Room_Draw13(&p[0], 0x200, 0x444);
            Room_Draw13(&p[2], 0x200, 0x222);
            Room_Draw13(&p[6], 0x200, 0x222);
            Room_Draw13(&p[7], 0x200, 0x333);
            Room_Draw13(&p[8], 0x200, 0x444);
            Room_Draw13(&p[9], 0x200, 0x444);
            break;
        }
        case 8:
        case 20: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181564;
            Room_Draw13(&p[0], 0x200, 0x444);
            Room_Draw13(&p[1], 0x200, 0x444);
            Room_Draw13(&p[2], 0x200, 0x444);
            func_neo_ark_observatory_80180534(&p[-32], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[-30], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[-28], 0x400, D_neo_ark_observatory_80187A3C, 8);
            break;
        }
        case 9: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181574;
            Room_Draw13(&p[0], 0x200, 0x444);
            Room_Draw13(&p[1], 0x200, 0x444);
            Room_Draw13(&p[2], 0x200, 0x333);
            Room_Draw13(&p[3], 0x200, 0x222);
            Room_Draw13(&p[-6], 0x200, 0x222);
            Room_Draw13(&p[-8], 0x200, 0x333);
            func_neo_ark_observatory_80180534(&p[-28], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[-26], 0x400, D_neo_ark_observatory_80187A3C, 0xC);
            func_neo_ark_observatory_80180534(&p[-24], 0x400, D_neo_ark_observatory_80187A3C, 8);
            break;
        }
        case 10: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181524;
            Room_Draw13(&p[0], 0x200, 0x444);
            Room_Draw13(&p[1], 0x200, 0x444);
            Room_Draw13(&p[5], 0x200, 0x444);
            Room_Draw13(&p[6], 0x200, 0x333);
            Room_Draw13(&p[7], 0x200, 0x222);
            func_neo_ark_observatory_80180534(&p[-12], 0x600, D_neo_ark_observatory_80187A3C, 0x10);
            break;
        }
        case 11: {
            SVECTOR* p;
            p = D_neo_ark_observatory_8018157C;
            Room_Draw13(&p[0], 0x200, 0x222);
            Room_Draw13(&p[1], 0x200, 0x333);
            Room_Draw13(&p[2], 0x200, 0x444);
            Room_Draw13(&p[-7], 0x200, 0x444);
            Room_Draw13(&p[-8], 0x200, 0x444);
            func_neo_ark_observatory_80180534(&p[-21], 0x600, D_neo_ark_observatory_80187A3C, 0x10);
            break;
        }
        case 12:
        case 14: {
            SVECTOR* p;
            p = D_neo_ark_observatory_801814E4;
            Room_Draw13(&p[0], 0x280, 0x444);
            Room_Draw13(&p[2], 0x280, 0x444);
            break;
        }
        case 21: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181564;
            Room_Draw13(&p[0], 0x200, 0x444);
            Room_Draw13(&p[1], 0x200, 0x444);
            break;
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_4", func_neo_ark_observatory_80180534);
