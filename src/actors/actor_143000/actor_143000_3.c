#include "common.h"

#include "actors/actor_143000.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/display.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

#include <psyq/memory.h>
#include <psyq/rand.h>

extern char             D_actor_143000_80131EB0[];
extern Actor143000Rect  D_actor_143000_80134580[];
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
    Actor143000Work* work = (Actor143000Work*)arg0->idMap;

    Display_ReleaseRef();
    Game_Session->field_66 = 0;
    if (work->field_C == 0) {
        D_80114D08             = 0xA;
        Game_Session->field_1  = 0;
        Game_Session->field_68 = 0;
        D_801153F4             = 0;
        D_8007216C             = D_actor_143000_80135C0C;
        Gp_MsgPlayer3F3(1);
    } else {
        Task_SpawnFromTable(&D_actor_143000_801350B0, 1, 0, (s32)&D_actor_143000_80135C08);
    }
    Task_Kill((Task*)arg0->spawnArg2);
    Task_RequestKill(arg0, work->field_C);
}

void func_actor_143000_801338C8(Actor143000* arg0)
{
    arg0->field_1C->field_4 = 0;
    arg0->field_30          = 2;
}

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_801338E0);

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

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_80133AE8);

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

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_80133CF0);
