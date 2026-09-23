#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_cafeteria.h"

#include <psyq/libgs.h>

extern s8  D_8007106B;
extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

extern GpMsgEntry D_acropolis_cafeteria_80184CEC[];

extern TaskDesc D_acropolis_cafeteria_80184178;

/// Copies the third column of a rotation matrix into an `SVECTOR`.
#define COPY_MATRIX_COLUMN2(src, dst)     \
    __asm__ volatile("lhu $12, 4(%0);"    \
                     "lhu $13, 10(%0);"   \
                     "lhu $14, 16(%0);"   \
                     "sh $12, 0(%1);"     \
                     "sh $13, 2(%1);"     \
                     "sh $14, 4(%1)"      \
                     :                    \
                     : "r"(src), "r"(dst) \
                     : "$12", "$13", "$14", "memory")
extern s32 D_acropolis_cafeteria_80184CFC;

void func_acropolis_cafeteria_8017E6B8(Task* arg0)
{
    Display_SpawnWithOt(&D_acropolis_cafeteria_80184178, 2, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

void func_acropolis_cafeteria_8017E708(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;

    work  = (GpEffWork*)task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (task->state != 0) {
        return;
    }
    task->msgTable = D_acropolis_cafeteria_80184CEC;
    Game_SetPtrSlot(task, 5);
    vec                            = &work->move;
    D_acropolis_cafeteria_80184CFC = 0;
    work->move.vx                  = 0x220;
    work->move.vy                  = -0x12C;
    work->move.vz                  = -0x6A0;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    work->move.vx = 0x400;
    work->move.vy = -0x12C;
    work->move.vz = -0x260;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    work->move.vx = 0x370;
    work->move.vy = -0x12C;
    work->move.vz = -0x860;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    task->state   = task->state + 1;
    work->move.vx = 0xBB8;
    work->move.vy = -0x834;
    work->move.vz = -0x7D0;
    Gp_SpawnEff(0x60064, coord, 1, vec);
    work->move.vx = 0xB22;
    work->move.vy = -0x834;
    work->move.vz = -0x900;
    Gp_SpawnEff(0x60064, coord, 1, vec);
    D_80115758 = 0x6028D;
    D_8011572C = 0x6028E;
    D_80115750 = 0x6028F;
}
void func_acropolis_cafeteria_8017E89C(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    s32            i;
    u16            count;
    s32            flags;
    s32            spawnArg;
    u8             mode;
    u16            rnd;

    work  = (GpEffWork*)task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (D_acropolis_cafeteria_80184CFC == 0) {
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    mode = gGameSession->at4.loc.view;
    if (mode == 9) {
        count = 0x28;
        if (work->scale != mode) {
            flags = 0x1000;
        } else {
            count = 2;
            flags = 0;
        }
        for (i = 0; i < count; i++) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->move.vx = (u32)rnd % 2620 + 0x230;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->move.vy = -0x12C - (u16)((u32)rnd % 5) * 0x190;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->move.vz = (rnd & 0x3FF) + 0xB00;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            spawnArg      = flags + 0x180;
            Gp_SpawnEff(0x60061, coord, (rnd & 0xFF) + spawnArg, &work->move);
        }
    }
    work->scale = gGameSession->at4.loc.view;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_7", func_acropolis_cafeteria_8017EA90);

void func_acropolis_cafeteria_8017F390(Task* task)
{
    TmdObject*     obj;
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    s16            state;
    s32            v;
    s32            w;
    s32            n;
    s32            k; // one variable for both branches' LCG addend; literal constants allocate differently
    s32            pan;

    obj   = (TmdObject*)task->extra;
    work  = (GpEffWork*)task->spawnArg2;
    state = Gp_State1C->eventState;
    coord = obj->coords;
    if (state >= 4) {
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    if (state != 0) {
        return;
    }
    Gp_UpdateCoord(coord);
    work->age++;
    if (task->state == 0) {
        obj->flags &= ~0x80;
        if (task->spawnArg1 != 0) {
            work->period = 0xD90;
            work->angle  = 0;
            work->index  = 2;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->scale  = ((u32)Gp_LcgState >> 16) & 0xF00;
        } else {
            work->scale  = 0x400;
            work->angle  = 0;
            work->period = 0xB00;
        }
        Gfx_RotMatrixY(&coord->coord, work->scale, 0);
        task->state++;
        return;
    }
    switch (work->index) {
        case 0:
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->angle  = 0;
            work->scale -= (((u32)Gp_LcgState >> 16) & 0xFF) - 0x80;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 30) == 0) {
                work->index = 1;
            }
            if (work->age >= 0x79) {
                work->index = 4;
            }
            break;
        case 1:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (((u32)Gp_LcgState >> 16) & 1) {
                v = work->scale;
                if (v > 0x400) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    n           = v - 0x10;
                    n          -= ((u32)Gp_LcgState >> 16) & 0x3F;
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    n           = v + 0x10;
                    n          += ((u32)Gp_LcgState >> 16) & 0x3F;
                }
                work->scale = n;
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = 0x200;
            if ((u16)(((u32)Gp_LcgState >> 16) % 30) == 0) {
                work->index = 0;
            }
            if (work->age >= 0x79) {
                work->index = 4;
            }
            break;
        case 2:
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->scale -= (((u32)Gp_LcgState >> 16) & 0xFF) - 0x80;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 240) == 0) {
                work->scale = 0x400;
                work->angle = 0x200;
                work->index = 3;
            }
            break;
        case 3:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 7) == 0) {
                work->angle = 0;
                work->index = 2;
            }
            break;
        case 4:
            w = work->scale;
            if (w > 0x400) {
                k           = 0x71357911;
                Gp_LcgState = Gp_LcgState * 5 + k;
                w          -= 0x10;
                w          -= ((u32)Gp_LcgState >> 16) & 0x3F;
            } else {
                k           = 0x71357911;
                Gp_LcgState = Gp_LcgState * 5 + k;
                w          += 0x10;
                w          += ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            work->scale = w;
            work->angle = 0x300;
            if ((Gp_GetViewIndex() & 0xFF) == 7 && work->step == 0) {
                pan = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(0x51040006, pan, (s8)gpGetObjDepth(coord));
                work->step = 1;
            }
            break;
    }
    rot     = (GpMtxWords*)&coord->coord;
    rot->w0 = 0x1000;
    rot->w1 = 0;
    rot->w2 = 0x1000;
    rot->w3 = 0;
    rot->h4 = 0x1000;
    Gfx_RotMatrixY(&coord->coord, work->scale, 0);
    COPY_MATRIX_COLUMN2(&coord->coord, &work->move);
    work->move.vx      = (work->move.vx * work->angle) >> 16;
    work->move.vy      = (work->move.vy * work->angle) >> 16;
    work->move.vz      = (work->move.vz * work->angle) >> 16;
    coord->coord.t[0] += work->move.vx;
    coord->coord.t[1] += work->move.vy;
    coord->coord.t[2] += work->move.vz;
    coord->flg         = 0;
    if (work->period < coord->coord.t[0]) {
        Gp_ReleaseState1CMem(work, task);
    }
}

s32 func_acropolis_cafeteria_8017F908(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    GsCOORDINATE2* coord;

    coord                          = ((TmdObject*)task->extra)->coords;
    D_acropolis_cafeteria_80184CFC = arg2;
    if (arg2 != 0) {
        Gp_SpawnEff(0x6009D, coord, 0, NULL);
    }
    return 0;
}
INCLUDE_RODATA("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_7", RoomsShared80181e70Table);
