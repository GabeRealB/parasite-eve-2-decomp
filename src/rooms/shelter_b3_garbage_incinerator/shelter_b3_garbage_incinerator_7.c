#include "common.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"

extern s8       D_8007218A;
extern u8       D_80073BA9;
extern TaskDesc D_80164190;

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_7", func_shelter_b3_garbage_incinerator_80183E78);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_7", func_shelter_b3_garbage_incinerator_801842A4);

void func_shelter_b3_garbage_incinerator_80184D7C(void)
{
}

void func_shelter_b3_garbage_incinerator_80184D84(Task* arg0)
{
    union {
        s32 msg[5];
        struct {
            u8 param1[8];
            u8 param2[8];
        } cd;
    } buf;
    s32 out;
    s32 v;

    switch (arg0->state) {
        case 0:
            v = D_80073BA9;
            if (D_8007218A == 1) {
                v = v + 1;
            } else {
                v = v + 0x22;
            }
            buf.msg[0] = v;
            buf.msg[1] = 1;
            buf.msg[2] = 0;
            buf.msg[3] = 0;
            buf.msg[4] = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)buf.msg, 0);
            arg0->spawnArg2 = Task_SpawnFromTable(&D_80164190, 0, 0, 0);
            arg0->state++;
            return;
        case 1:
            if (Task_PollKill(arg0->spawnArg2, &out) != 0) {
                arg0->state++;
            }
            return;
        case 2:
            arg0->state = 3;
            return;
        case 3:
            buf.cd.param1[2] = 0x22;
            buf.cd.param1[3] = 0;
            buf.cd.param1[0] = 0;
            buf.cd.param2[0] = 0x16;
            buf.cd.param2[1] = 0;
            buf.cd.param2[2] = 0;
            buf.cd.param2[3] = 0;
            CdCmd_Enqueue(0x21, buf.cd.param1, buf.cd.param2);
            taskKill(arg0);
            break;
    }
}
