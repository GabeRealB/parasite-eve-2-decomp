#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b4_reservoir.h"

#include <psyq/abs.h>
#include <psyq/libgte.h>

#define RAND() ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)

extern s32 Gp_GetViewIndex(void);

extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115738;
extern s32 D_8011574C;
extern s32 D_80115754;
extern u32 Gp_LcgState;

extern s16     D_shelter_b4_reservoir_80184F82;
extern SVECTOR D_shelter_b4_reservoir_80185024[];
extern SVECTOR D_shelter_b4_reservoir_80185034;
extern SVECTOR D_shelter_b4_reservoir_80185044;
extern SVECTOR D_shelter_b4_reservoir_80185074;
extern SVECTOR D_shelter_b4_reservoir_80185094;
extern SVECTOR D_shelter_b4_reservoir_8018509C[];
extern SVECTOR D_shelter_b4_reservoir_801850AC[];
extern SVECTOR D_shelter_b4_reservoir_80187634[];

void func_shelter_b4_reservoir_8017FB84(Task* task)
{
    ShelterB4ReservoirWork* work;
    Task*                   player;
    GsCOORDINATE2*          root;
    GsCOORDINATE2*          c;
    GsCOORDINATE2           coord;
    MATRIX*                 view;
    GsCOORDINATE2*          parent;
    s32                     i;
    s32                     offset;
    s32                     roll;

    work   = task->spawnArg2;
    player = gameGetPtrSlot(3);
    root   = ((TmdObject*)player->extra)->coords;
    if (task->state == 0) {
        D_8011574C  = 0x60172;
        D_80115738  = 0x60173;
        D_80115734  = 0x60225;
        D_80115730  = 0x60230;
        D_80115754  = 0x6023B;
        task->state = 1;
        for (i = 0; i < 10; i++) {
            work->field_24                        = RAND() & 0x1C0;
            work->field_26                        = (RAND() & 0x1FF) + (i << 9);
            D_shelter_b4_reservoir_80187634[i].vx = D_shelter_b4_reservoir_80185094.vx + 0x800;
            D_shelter_b4_reservoir_80187634[i].vy =
                D_shelter_b4_reservoir_80185094.vy + ((work->field_24 * rsin(work->field_26)) >> 12);
            D_shelter_b4_reservoir_80187634[i].vz =
                D_shelter_b4_reservoir_80185094.vz + ((work->field_24 * rcos(work->field_26)) >> 12);
        }
        D_shelter_b4_reservoir_80187684.field_0 = 0;
        D_shelter_b4_reservoir_80187684.field_2 = 0;
        D_shelter_b4_reservoir_80187684.field_4 = 0;
        for (i = 0; i < 2; i++) {
            c                                     = &((TmdObject*)player->extra)->coords[i * 3 + 14];
            D_shelter_b4_reservoir_801850AC[i].vx = c->workm.t[0];
            D_shelter_b4_reservoir_801850AC[i].vy = c->workm.t[1];
            D_shelter_b4_reservoir_801850AC[i].vz = c->workm.t[2];
        }
    }
    if (Gp_State1C->eventState == 0) {
        if (GameFlag_GetNibble(0xB7) != 0) {
            if (gGameSession->waterY < root->coord.t[1] && work->field_22 != 0) {
                for (i = 0; i < 2; i++) {
                    parent = &gGfxViewCoord;
                    view   = &Gfx_ViewWorldMtx;
                    c      = &((TmdObject*)player->extra)->coords[i * 3 + 14];
                    Gp_UpdateCoord(c);
                    work->field_26 = ABS(D_shelter_b4_reservoir_801850AC[i].vx - c->workm.t[0]) +
                                     ABS(D_shelter_b4_reservoir_801850AC[i].vy - c->workm.t[1]) +
                                     ABS(D_shelter_b4_reservoir_801850AC[i].vz - c->workm.t[2]) + 0x20;
                    Gp_WorldToLocal(view, &c->workm, &coord.coord);
                    coord.sub        = parent;
                    coord.coord.t[1] = gGameSession->waterY;
                    coord.flg        = 0;
                    Gp_UpdateCoord(&coord);
                    if ((s32)(RAND() & 0x1FF) < work->field_26) {
                        Gp_SpawnEff(D_8011574C, &coord, 0x40, NULL);
                    }
                    work->field_26 -= 0x20;
                    if ((s32)(RAND() & 0x1FF) < work->field_26) {
                        Gp_SpawnEff(D_80115738, &coord, 0x1202180, NULL);
                    }
                    D_shelter_b4_reservoir_801850AC[i].vx = c->workm.t[0];
                    D_shelter_b4_reservoir_801850AC[i].vy = c->workm.t[1];
                    D_shelter_b4_reservoir_801850AC[i].vz = c->workm.t[2];
                }
            }
            work->field_22++;
        }
        if (D_shelter_b4_reservoir_80187684.field_0 != 0 && D_shelter_b4_reservoir_80187684.field_2 != 0) {
            for (i = 0; i < D_shelter_b4_reservoir_80187684.field_0; i++) {
                if ((RAND() & 0x1F) == 0) {
                    work->field_24                        = RAND() & 0x1C0;
                    work->field_26                        = (RAND() & 0x1FF) + (i << 9);
                    D_shelter_b4_reservoir_80187634[i].vx = D_shelter_b4_reservoir_80185094.vx + 0x800;
                    D_shelter_b4_reservoir_80187634[i].vy =
                        D_shelter_b4_reservoir_80185094.vy + ((work->field_24 * rsin(work->field_26)) >> 12);
                    D_shelter_b4_reservoir_80187634[i].vz =
                        D_shelter_b4_reservoir_80185094.vz + ((work->field_24 * rcos(work->field_26)) >> 12);
                }
                if ((u16)(RAND() % 100) < D_shelter_b4_reservoir_80187684.field_2) {
                    offset = D_shelter_b4_reservoir_80187684.field_4;
                    roll   = (RAND() & 0x10FF) + 0x502000;
                    Gp_SpawnEff(0x600AA, NULL, offset + roll, &D_shelter_b4_reservoir_80187634[i]);
                }
            }
        }
    }
    if ((u8)Gp_GetViewIndex() == 10) {
        D_shelter_b4_reservoir_8018509C[1].vy = D_shelter_b4_reservoir_80184F82;
        if ((RAND() & 1) == 0) {
            Gp_SpawnEff(D_80115738, NULL, (RAND() & 0x1000) + 0x4A03600, &D_shelter_b4_reservoir_8018509C[0]);
        }
        if ((RAND() & 1) == 0) {
            Gp_SpawnEff(D_80115738, NULL, (RAND() & 0x10FF) | 0x11602300, &D_shelter_b4_reservoir_8018509C[1]);
        }
        if ((RAND() & 3) == 0) {
            Gp_SpawnEff(D_8011574C, NULL, (RAND() & 0x7F) | 0x80, &D_shelter_b4_reservoir_8018509C[1]);
        }
    }
    switch ((u8)Gp_GetViewIndex()) {
        case 2:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185074, 0x200, 0x444);
            break;
        case 4:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[0], 0x200, 0x222);
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[6], 0x200, 0x444);
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[8], 0x200, 0x333);
            if (GameFlag_GetNibble(0xB7) != 0) {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 5:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[0], 0x200, 0x444);
            if (GameFlag_GetNibble(0xB7) != 0) {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 6:
            if (GameFlag_GetNibble(0xB7) != 0) {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 7:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185034, 0x200, 0x444);
            break;
        case 3:
        case 9:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185044, 0x200, 0x444);
            break;
    }
}
