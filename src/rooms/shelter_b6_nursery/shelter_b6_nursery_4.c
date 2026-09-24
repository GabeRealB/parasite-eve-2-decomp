#include "common.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b6_nursery.h"

extern TaskDesc       D_shelter_b6_nursery_80185000;
extern s32            D_shelter_b6_nursery_8018797C;
extern TaskFuncTable3 RoomsShared8017d878Table;
extern s32            D_80070F70;
extern s32            D_8011572C;
extern s32            D_80115750;
extern s32            D_80115758;
extern u32            Gp_LcgState;
extern SVECTOR        D_shelter_b6_nursery_8018504C[];
extern SVECTOR        D_shelter_b6_nursery_80185054;
extern SVECTOR        D_shelter_b6_nursery_8018505C[];
extern SVECTOR        D_shelter_b6_nursery_80185074;

s32 rsin(s32);

void func_shelter_b6_nursery_8017FFF4(void)
{
    if (D_shelter_b6_nursery_8018797C == 0) {
        D_shelter_b6_nursery_8018797C = 1;
        Task_SpawnFromTable(&D_shelter_b6_nursery_80185000, 0, 0, 0);
    }
}

/// Sets the second sprite command's skip-link flag in view 13 for the current
/// room in the first stage table. Only low-byte values 0 and 1 change the flag.
void func_shelter_b6_nursery_80180038(s32 arg0)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtCmd* cmd;
    s32        mode;

    cmd  = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1][12].field_4;
    mode = arg0 & 0xFF;
    if (mode == 0) {
        cmd[1].field_4 = 0;
    } else if (mode == 1) {
        cmd[1].field_4 = 1;
    }
}

void func_shelter_b6_nursery_801800A0(Task* task)
{
    SVECTOR* pos;
    u32      a;
    u32      b;
    s32      angle;
    s32      r;
    s32      i;

    if (task->state == 0) {
        D_80115758                            = 0x601E0;
        D_8011572C                            = 0x601FC;
        D_80115750                            = 0x60218;
        D_shelter_b6_nursery_801879F0.field_0 = 0;
        D_shelter_b6_nursery_801879F0.field_2 = 0;
        task->state                           = 1;
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
        case 8:
            if (D_shelter_b6_nursery_801879F0.field_0 != 0) {
                Room_Draw18(D_shelter_b6_nursery_8018504C, 0x180, 0x80);
            } else {
                Room_Draw18(D_shelter_b6_nursery_8018504C, 0x60, 0x80);
            }
            break;
        case 6:
        case 10:
            if (D_shelter_b6_nursery_801879F0.field_0 != 0) {
                Room_Draw05(D_shelter_b6_nursery_8018504C, 0x180, 0x80);
            } else {
                Room_Draw05(D_shelter_b6_nursery_8018504C, 0x60, 0x80);
            }
            break;
        case 12:
            if (task->state == 1) {
                Gp_SpawnEff(0x601A3, NULL, task->spawnArg1, &D_shelter_b6_nursery_80185054);
                Gp_SpawnEff(0x601A3, NULL, task->spawnArg1, &D_shelter_b6_nursery_80185054);
                Gp_SpawnEff(0x601A3, NULL, task->spawnArg1, &D_shelter_b6_nursery_80185054);
                task->state = 2;
            }
            break;
        case 13:
            task->state = 3;
            if (D_shelter_b6_nursery_801879F0.field_2 != 0) {
                for (i = 0; i < 16; i++) {
                    a                                   = Gp_LcgState * 5 + 0x71357911;
                    b                                   = a * 5 + 0x71357911;
                    angle                               = (a >> 16) & 0xFFF;
                    Gp_LcgState                         = b;
                    r                                   = ((b >> 16) & 0xFF) * D_shelter_b6_nursery_801879F0.field_2;
                    D_shelter_b6_nursery_8018504C[6].vx = 0x1C20;
                    D_shelter_b6_nursery_8018504C[6].vy = ((r * rsin(angle)) >> 12) - 0x6D6;
                    D_shelter_b6_nursery_8018504C[6].vz = ((r * rsin(angle)) >> 12) + 0x7D0;
                    Gp_LcgState                         = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x601A5, NULL,
                                (((Gp_LcgState >> 16) & 0x1F) + 8) * D_shelter_b6_nursery_801879F0.field_2,
                                &D_shelter_b6_nursery_8018504C[6]);
                }
                D_shelter_b6_nursery_801879F0.field_0 = 0;
                D_shelter_b6_nursery_801879F0.field_2 = 0;
            }
            break;
        case 15:
            if (!(D_80070F70 & 1)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_80185074);
            }
            break;
        case 17:
            pos = D_shelter_b6_nursery_8018504C;
            Room_Draw18(pos, 0x60, 0x80);
            if (!(D_80070F70 & 1)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018504C[4]);
            }
            break;
        case 18:
            if (!(D_80070F70 & 1)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018504C[4]);
            }
            break;
    }
    if (task->state == 3 && !(D_80070F70 & 1)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018505C[0]);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018505C[1]);
    }
}
