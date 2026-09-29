#include "rooms/mist_parking.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "mist_parking_private.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

/// Scratch state of the parking-lot cap script driven by
/// `func_mist_parking_80183EAC`, cleared with `Mem_Set` when the task starts.

void            func_mist_parking_801846A4(s32 arg0);
extern GpEvsCmd D_mist_parking_80190C74[];
extern GpEvsCmd D_mist_parking_80190D64[];
extern GpEvsCmd D_mist_parking_80190E84[];
extern GpEvsCmd D_mist_parking_80191034[];

extern s8 D_mist_parking_801908C8[];

void func_mist_parking_80183D58(Task*);
void func_mist_parking_80183EAC(Task*);
void func_mist_parking_801842DC(Task*);
void func_mist_parking_8018451C(Task*);
void func_mist_parking_80184668(Task*);

extern GpAnimArg D_mist_parking_801908A0;
extern GpAnimArg D_mist_parking_801908B4;
extern GpAnimArg D_mist_parking_80190944;
extern GpAnimArg D_mist_parking_801909F8;
extern GpCmdArg  D_mist_parking_80190BA4;
extern GpCmdArg  D_mist_parking_80190BA8;

void func_mist_parking_80184408(s32);
void func_mist_parking_80184428(s32);
void func_mist_parking_80184468(s32);
void func_mist_parking_801844EC(void);
void func_mist_parking_8018459C(void);
void func_mist_parking_801845D0(s32);
void func_mist_parking_80184624(s32);
void func_mist_parking_801846A4(s32);

TaskDesc D_mist_parking_80190824[5] = {
    { 0, 192, func_mist_parking_8018451C, { .model = NULL } },
    { 0, 97, func_mist_parking_80183D58, { .model = NULL } },
    { 0, 192, func_mist_parking_80184668, { .model = NULL } },
    { 0, 192, func_mist_parking_80183EAC, { .model = NULL } },
    { 0, 192, func_mist_parking_801842DC, { .model = NULL } },
};

GpAnimSet* D_mist_parking_80190860[4] = {
    NULL,
    &D_mist_parking_8018FFB8,
    &D_mist_parking_8019038C,
    &D_mist_parking_801907FC,
};

GpCopyArg D_mist_parking_80190870 = { { .sets = D_mist_parking_80190860 }, 4 };

GpAnimArg D_mist_parking_80190878 = { { .index = 1 }, 1, 0, 0, 0 };

GpAnimArg D_mist_parking_8019088C = { { .index = 1 }, 48, 0, 0, 0 };

GpAnimArg D_mist_parking_801908A0 = { { .index = 1 }, 49, 1, 6, 0 };

GpAnimArg D_mist_parking_801908B4 = { { .index = 1 }, 50, 1, 6, 0 };

s8 D_mist_parking_801908C8[124] = {
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
};

GpAnimArg D_mist_parking_80190944 = { { .ptr = NULL }, 6, 1, 20, 1 };

GpAnimArg D_mist_parking_80190958[8] = {
    { { .index = 0 }, 7, 1, 20, 1 },
    { { .index = 0 }, 8, 1, 20, 1 },
    { { .index = 0 }, 9, 1, 20, 1 },
    { { .index = 0 }, 10, 1, 20, 1 },
    { { .index = 0 }, 11, 1, 20, 1 },
    { { .index = 0 }, 12, 1, 20, 1 },
    { { .index = 0 }, 13, 1, 20, 1 },
    { { .index = 0 }, 14, 1, 20, 1 },
};

GpAnimArg D_mist_parking_801909F8 = { { .index = 0 }, 15, 1, 20, 1 };

GpAnimArg D_mist_parking_80190A0C[20] = {
    { { .index = 0 }, 16, 1, 20, 1 },
    { { .index = 0 }, 17, 1, 20, 1 },
    { { .index = 0 }, 18, 1, 20, 1 },
    { { .index = 0 }, 19, 1, 20, 1 },
    { { .index = 0 }, 20, 1, 20, 1 },
    { { .index = 0 }, 21, 1, 20, 1 },
    { { .index = 0 }, 22, 1, 20, 1 },
    { { .index = 0 }, 23, 1, 20, 1 },
    { { .index = 0 }, 24, 1, 20, 1 },
    { { .index = 0 }, 25, 1, 20, 1 },
    { { .index = 0 }, 26, 1, 20, 1 },
    { { .index = 0 }, 27, 1, 20, 1 },
    { { .index = 0 }, 28, 1, 20, 1 },
    { { .index = 0 }, 29, 1, 20, 1 },
    { { .index = 0 }, 30, 1, 20, 1 },
    { { .index = 0 }, 31, 1, 20, 1 },
    { { .index = 0 }, 32, 1, 20, 1 },
    { { .index = 0 }, 33, 1, 20, 1 },
    { { .index = 0 }, 34, 1, 20, 1 },
    { { .index = 0 }, 35, 1, 20, 1 },
};

GpOverrideArg D_mist_parking_80190B9C = { 0, 0x10000 };

GpCmdArg D_mist_parking_80190BA4 = { { .loc = { 0, 0 } }, 2 };

GpCmdArg D_mist_parking_80190BA8 = { { .loc = { 0, 0 } }, 3 };

GpAnimArg D_mist_parking_80190BAC = { { .index = 0 }, 1, 0, 0, 0 };

GpAnimArg D_mist_parking_80190BC0 = { { .index = 0 }, 1, 1, 20, 1 };

GpAnimArg D_mist_parking_80190BD4[3] = {
    { { .index = 0 }, 2, 1, 20, 1 },
    { { .index = 0 }, 3, 0, 0, 1 },
    { { .index = 0 }, 4, 1, 10, 1 },
};

GpAnimArg D_mist_parking_80190C10 = { { .index = 0 }, 5, 1, 20, 1 };

GpAnimArg D_mist_parking_80190C24 = { { .index = 0 }, 6, 0, 0, 1 };

GpAnimArg D_mist_parking_80190C38 = { { .index = 0 }, 7, 0, 0, 1 };

GpAnimArg D_mist_parking_80190C4C = { { .index = 0 }, 8, 1, 20, 1 };

GpAnimArg D_mist_parking_80190C60 = { { .index = 0 }, 9, 1, 15, 1 };

GpEvsCmd D_mist_parking_80190C74[10] = {
    { 13, { .callbackNoArg = func_mist_parking_8018459C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_80190870 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mist_parking_80190BA4 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_801908A0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_80190944 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_80190D64[12] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_80190870 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80184408 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_801908B4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_801909F8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_801845D0 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_801908B4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_mist_parking_80190BA8 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_80190E84[18] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_80190870 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80184408 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_801909F8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 5, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80184624 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_801846A4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80184428 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mist_parking_801844EC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_80191034[12] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_80190870 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80184408 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_801909F8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 5, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80184624 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80184468 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

void func_mist_parking_80183BAC(s32 arg0)
{
    GpGridParams* dst;
    GpGridParams* src;
    SVECTOR       d;
    s32           i;

    dst = &D_mist_parking_80192204;
    src = &D_mist_parking_8018FCB8;

    for (i = 0; i < 2; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0x7D0;
    }
    d.vz = 0;

    for (i = 0; i < 6; i++) {
        dst->field_8[i].vx += d.vx;
        dst->field_8[i].vy += d.vy;
        dst->field_8[i].vz += d.vz;
    }
}

void func_mist_parking_80183D58(Task* task)
{
    GameActor* actor;
    GpWorkObj* work;
    s32        idx;
    s32        flag;
    u16        tick;

    actor = (GameActor*)(gameGetPtrSlot(3))->work;
    if (D_801156F9 == 0) {
        idx = actor->field_438[1].nextSet - 0x2F;
        if ((idx > 0) && (idx < D_mist_parking_80190870.count)) {
            flag = D_mist_parking_801908C8[idx];
        } else {
            flag = 0;
        }
        if (task->state == 0) {
            if ((flag != 0) || (task->spawnArg1.value != 0)) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            work = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8));
            func_800B0928(gameGetPtrSlot(3), work->field_0, 0x200, 0x100, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}

void func_mist_parking_80183EAC(Task* task)
{
    MistParkingCapState* st = &D_mist_parking_80195334;
    s32                  cmd;
    s32                  i;
    s16                  slot;
    s16                  slot2;
    s32                  key;
    u16                  raw;
    s16                  count;
    u16                  tick;
    u16                  tick2;
    s16                  next;

    switch (task->state) {
        case 0:
            Mem_Set(st, 0, 8);
            func_mist_parking_801846A4(1);
            func_800E8614(D_mist_parking_80191154, 1);
            Gp_RunCapCmd(6, 0);
            task->state++;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            for (i = 0; i < 5; i++) {
                if (GameFlag_GetNibble(i + 0x125) == 2) {
                    task->state = 2;
                    return;
                }
            }
            task->state = 6;
            break;
        case 2:
            func_mist_parking_801846A4(2);
            func_800E8614(D_mist_parking_80191154, 1);
            Gp_RunCapCmd(1, 0);
            st->field_4 = 1;
            task->state++;
            break;
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            if (Gp_GetCapEventKey() == 1) {
                Gp_RunCapCmd(7, 0);
                st->timer   = 0xA;
                st->cmd     = 2;
                task->state = 4;
            } else {
                st->cmd     = 3;
                task->state = 5;
            }
            break;
        case 4:
            if (Gp_CapBusy() != 0) {
                return;
            }
            raw       = st->timer - 1;
            st->timer = raw;
            count     = raw;
            if (count == 5) {
                slot = st->slot;
                if (GameFlag_GetNibble(slot + 0x125) == 2) {
                    Gp_StartCapSlot(5, 0, slot);
                }
                return;
            }
            if (count != 0) {
                return;
            }
            slot2 = st->slot;
            if (Gp_GetCurBit2Flag(slot2 + 0x20) != 1) {
                GameFlag_SetNibble(slot2 + 0x125, 3);
            }
            st->timer = 0xA;
            next      = (u16)st->slot + 1;
            st->slot  = next;
            if (next >= 5) {
                task->state++;
            }
            break;
        case 5:
            func_800E8614(D_mist_parking_80191154, 1);
            Gp_RunCapCmd(st->cmd, 0);
            task->state++;
            break;
        case 6:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            tick                = task->killCountdown + 1;
            task->killCountdown = tick;
            if ((s16)tick == 0xA) {
                func_mist_parking_801846A4(1);
                Gp_RunCapCmd(9, 0);
                task->killCountdown = 0;
                task->state++;
            }
            break;
        case 7:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 8:
            key                   = Gp_GetCapEventKey();
            task->spawnArg1.value = key;
            if (key == 6) {
                func_800E8614(D_mist_parking_80191214, 1);
                st->field_4 = 1;
            } else if (key == 7) {
                func_800E8614(D_mist_parking_80191304, 1);
                st->field_4 = 1;
            } else {
                func_800E8614(D_mist_parking_801913C4, 1);
            }
            task->state++;
            break;
        case 9:
            tick2               = task->killCountdown + 1;
            task->killCountdown = tick2;
            if ((s16)tick2 == 0xA) {
                switch (task->spawnArg1.value) {
                    case 6:
                        Gp_RunCapCmd(7, 0);
                        break;
                    case 7:
                        Gp_RunCapCmd(8, 0);
                        break;
                    case 8:
                        if (st->field_4 != 0) {
                            Gp_RunCapCmd(7, 0);
                        } else {
                            Gp_RunCapCmd(0xA, 0);
                        }
                        break;
                }
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            cmd                 = 0xA;
            task->killCountdown = 0;
            if (task->spawnArg1.value == 7) {
                cmd = 6;
            }
            task->state = cmd;
            break;
        case 10:
            Gp_MsgPlayerWeapon(1);
            func_mist_parking_801846A4(0);
            taskKill(task);
            break;
    }
}

void func_mist_parking_801842DC(Task* task)
{
    s32 key;

    switch (task->state) {
        case 0:
            func_mist_parking_801846A4(1);
            func_800E8614(D_mist_parking_80190C74, 1);
            task->state++;
            break;
        case 1:
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state++;
            break;
        case 2:
            key                   = Gp_GetCapEventKey();
            task->spawnArg1.value = key;
            switch (key) {
                case 1:
                    func_800E8614(D_mist_parking_80190D64, 1);
                    break;
                case 2:
                    func_800E8614(D_mist_parking_80190E84, 1);
                    break;
                case 3:
                    func_800E8614(D_mist_parking_80191034, 1);
                    break;
            }
            task->state++;
            break;
        case 4:
            if (task->spawnArg1.value == 1) {
                Gp_MsgPlayerWeapon(1);
            }
            func_mist_parking_801846A4(0);
            taskKill(task);
            break;
    }
}

void func_mist_parking_80184408(s32 arg0)
{
    Gp_RunCapCmd(arg0, 0);
}

void func_mist_parking_80184428(s32 arg0)
{
    Task_SpawnFromTable(D_mist_parking_8018FC24, 0, arg0, 0);
    gGameSession->freezeRoomObjs = 1;
}

void func_mist_parking_80184468(s32 arg0)
{
    Mc_SaveData[0].state.at4.loc.stage = 1;
    Mc_SaveData[0].state.at4.loc.warp  = 1;
    Mc_SaveData[0].state.at4.loc.room  = 1;
    Mc_SaveData[0].state.at4.loc.area  = arg0;
    gDisplayState.spriteVariant        = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    if (arg0 == 5) {
        Fs_BeginBootLoad(&Mc_SaveData[0].state.at4.loc.view, 0);
    }
}

/// Spawns entry 0 of `D_mist_parking_80190824`.
void func_mist_parking_801844EC(void)
{
    Task_SpawnFromTable(D_mist_parking_80190824, 0, 0, 0);
}

void func_mist_parking_8018451C(Task* task)
{
    func_800BC4BC();
    Player_Status.field_26             = 1;
    Mc_SaveData[0].state.at4.loc.area  = 5;
    Mc_SaveData[0].state.at4.loc.stage = 1;
    Mc_SaveData[0].state.at4.loc.warp  = 1;
    Mc_SaveData[0].state.at4.loc.room  = 1;
    gDisplayState.spriteVariant        = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    Fs_BeginBootLoad(&Mc_SaveData[0].state.at4.loc.view, 0);
}

/// Spawns entry 1 of `D_mist_parking_80190824` and keeps its handle in
/// `D_mist_parking_8019532C.value`.
void func_mist_parking_8018459C(void)
{
    D_mist_parking_8019532C.value = Task_SpawnFromTable(D_mist_parking_80190824, 1, 0, 0);
}

/// Hands `phase` (0 or 1) to the task in `D_mist_parking_8019532C.value` as its
/// `spawnArg1`; any other value kills the task and drops the handle.
void func_mist_parking_801845D0(s32 phase)
{
    Task* t = D_mist_parking_8019532C.value;

    if (t == NULL) {
        return;
    }
    if (phase >= 2) {
        goto kill;
    }
    if (phase < 0) {
        goto kill;
    }
    t->spawnArg1.value = phase;
    return;
kill:
    taskKill(D_mist_parking_8019532C.value);
    D_mist_parking_8019532C.value = NULL;
}

void func_mist_parking_80184624(s32 arg0)
{
    Display_InitModeObj(Task_GetDescAt(D_mist_parking_80190824, 2U), arg0, 0, 0);
}

void func_mist_parking_80184668(Task* arg0)
{
    s32 temp_v0;

    temp_v0               = arg0->spawnArg1.value - 1;
    arg0->spawnArg1.value = temp_v0;
    if (temp_v0 < 0) {
        taskKill(arg0);
        Stage_SetEndingFlag();
    }
}

void func_mist_parking_801846A4(s32 arg0)
{
    Gp_ResetCap();
    switch (arg0) {
        case 1:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x140, 0x100);
            break;
        case 2:
            Gp_CapFile = 0;
            Gp_LoadCapFile(2);
            func_800E6D4C(0x2C0, 0);
            break;
    }
}

/// Drops the handle in `D_mist_parking_8019532C.value` without killing the task.
/// Its caller passes an argument, which is unused.
void func_mist_parking_8018471C(s32 arg0)
{
    D_mist_parking_8019532C.value = NULL;
}
