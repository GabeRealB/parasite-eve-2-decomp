#include "common.h"

#include "actors/actor_143000.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

#include <psyq/memory.h>
#include <psyq/rand.h>

extern char             D_actor_143000_80131EB0[];
extern Actor143000Rect  D_actor_143000_80134580[];
extern char*            D_actor_143000_801345F8[];
extern TaskDesc         D_actor_143000_801350B0;
extern Actor143000Spawn D_actor_143000_80135C08;
extern u8               D_actor_143000_80135C0C;
extern char             D_actor_143000_80135C20[];
extern s16              D_80114D08;
extern s8               D_8007216C;
extern u8               D_801153F4;

void func_actor_143000_801336E8(Actor143000* arg0)
{
    Actor143000Work*  work   = arg0->field_1C;
    RoomActionPrompt* prompt = &D_80114D28;
    s32               cmd;

    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        switch ((s16)(work->field_2 - 1)) {
            case 0:
                cmd = 8;
                goto run;
            case 1:
                cmd = 7;
                goto run;
            case 3:
                cmd = 9;
            run:
                Gp_RunCapCmd(cmd, 0);
                arg0->field_30 = 2;
                break;
            case 2:
                SndEvt_EnqueueType6(0x541F0010, 0, 0);
                arg0->field_30 = 7;
                arg0->field_2A = 0;
                break;
            case 4:
                work->field_7 = 1;
                Gp_RunCapCmd(0xA, 0);
                if (GameFlag_GetNibble(0xD0) == 1) {
                    arg0->field_2A = 0xA;
                    arg0->field_30 = 9;
                } else {
                    arg0->field_30 = 2;
                }
                break;
            default:
                arg0->field_30 = 2;
                break;
        }
    } else if (work->field_4 != 0) {
        arg0->field_30 = 6;
    } else {
        arg0->field_30 = 2;
    }
}

void func_actor_143000_80133800(Task* arg0)
{
    Actor143000Work* work = (Actor143000Work*)arg0->work;

    Display_ReleaseRef();
    gGameSession->cutsceneHold = 0;
    if (work->field_C == 0) {
        D_80114D08               = 0xA;
        gGameSession->eventState = 0;
        gGameSession->hideHud    = 0;
        D_801153F4               = 0;
        D_8007216C               = D_actor_143000_80135C0C;
        Gp_MsgPlayer3F3(1);
    } else {
        Task_SpawnFromTable(&D_actor_143000_801350B0, 1, 0, (s32)&D_actor_143000_80135C08);
    }
    taskKill((Task*)arg0->spawnArg2);
    Task_RequestKill(arg0, work->field_C);
}

void func_actor_143000_801338C8(Actor143000* arg0)
{
    arg0->field_1C->field_4 = 0;
    arg0->field_30          = 2;
}

void func_actor_143000_801338E0(Actor143000* arg0)
{
    Actor143000Work* work = arg0->field_1C;
    s32              col  = (work->field_8 + 0x80) / 16;
    s32              row  = (work->field_A - 0x20) / 16;
    char*            key;

    if ((u32)col < 13) {
        if (row >= 0) {
            if (row < 3) {
                key = D_actor_143000_801345F8[row] + col;
                if ((s8)*key == '#') {
                    work->field_10 = 0;
                } else if ((s8)*key == '-') {
                    if (work->field_10 > 0) {
                        work->field_10--;
                    }
                } else if (work->field_10 < 20) {
                    D_actor_143000_80135C20[work->field_10] = *key;
                    work->field_10++;
                }
                work->field_18 = 0x30;
            }
        }
    }
    arg0->field_30 = 2;
}

void func_actor_143000_801339CC(Actor143000* arg0)
{
    Actor143000Work* work = arg0->field_1C;
    u32              count;

    if (Gp_CapBusy() == 0) {
        count          = (u16)arg0->field_2A - 1;
        arg0->field_2A = count;
        if ((s16)count <= 0) {
            arg0->field_2A = (rand() * 8 >> 15) + 8;
            work->field_10++;
            SndEvt_EnqueueType6(0x541F0013, 0, 0);
            memcpy(D_actor_143000_80135C20, D_actor_143000_80131EB0, 11);
            count = work->field_10;
            if (count >= 0xA) {
                arg0->field_30 = 2;
            }
        }
    }
}

void func_actor_143000_80133AC0(Actor143000* arg0)
{
    u16 count = (u16)arg0->field_2A - 1;

    arg0->field_2A = count;
    if ((s16)count <= 0) {
        arg0->field_30 = 5;
    }
}

s32 func_actor_143000_80133AE8(Actor143000Rect* p, s16 x, s16 y)
{
    s32 result = 0;

    if (p->field_8 != -1) {
        do {
            if (x >= p->x && x < p->x + p->w && y >= p->y && y < p->y + p->h) {
                if (Mc_SaveData.demoScene == 9) {
                    func_actor_143000_80133334(p, 0, 0, 0);
                }
                p->field_B = 1;
                if (result == 0) {
                    result = p->field_8;
                }
            } else {
                if (Mc_SaveData.demoScene == 9) {
                    func_actor_143000_80133334(p, 0xFF, 0, 0);
                }
                p->field_B = 0;
            }
            p++;
        } while (p->field_8 != -1);
    }
    return result;
}

void func_actor_143000_80133C2C(void)
{
    Actor143000Rect* p = D_actor_143000_80134580;

    if (p->field_8 != -1) {
        do {
            func_actor_143000_80133334(p, 0, 0xFF, 0);
            p++;
        } while (p->field_8 != -1);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", ActorsShared8013845cSub0);

void func_actor_143000_80133CF0(Task* arg0)
{
    Actor143000CaptureArgs* p = arg0->spawnArg2;
    RECT                    r;
    RECT                    r2;
    s32                     n;
    s32                     offset;
    RECT*                   rp;
    s32                     bottom;
    Task*                   killTask = arg0;

    SOFT_TOUCH_REG(killTask);
    if (gGameSession->at4.loc.view == 0xE) {
        switch (arg0->state) {
            case 0:
                arg0->killCountdown = 6;
                p->count            = 0;
                arg0->state++;
                return;
            case 1:
                if (--arg0->killCountdown > 0) {
                    return;
                }
                arg0->killCountdown = 6;
                r.x                 = p->x;
                r.w                 = p->w;
                r.y                 = p->y + p->h * p->count / p->total;
                n                   = p->count + 1;
                rp                  = &r2;
                SOFT_TOUCH_REG_USE(rp, n);
                p->count = n;
                bottom   = p->y + p->h * n / p->total;
                offset   = r.y * 640;
                r.h      = bottom - r.y;
                SOFT_USE_REG(offset);
                r2    = r;
                r2.x  = 0x1C0;
                rp->w = 0x140;
                r2.y += 0x100;
                StoreImage(rp, (u32*)((u8*)Fs_ImgBuffers + offset));
                killTask = arg0;
                if (p->count >= p->total) {
                    goto kill;
                }
                break;
        }
    } else {
        goto kill;
    }
    return;
kill:
    taskKill(killTask);
}
