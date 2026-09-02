#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_motel_balcony.h"

extern GpMsgEntry           D_dryfield_motel_balcony_8018227C[];
extern TaskDesc             D_dryfield_motel_balcony_80182270;
extern RoomEventMsg         D_dryfield_motel_balcony_80186724;
extern u8                   D_dryfield_motel_balcony_8018672C;
extern RoomEventReq         D_dryfield_motel_balcony_80186730;
extern const TaskFuncTable3 D_dryfield_motel_balcony_8017D5DC;
void                        func_dryfield_motel_balcony_8017DB84(Task*);
void                        func_dryfield_motel_balcony_8017DBC8(void);
extern s32                  D_80115720;
extern s32                  D_80115728;
extern s32                  D_8011572C;
extern s32                  D_8011573C;
extern s32                  D_80115744;
extern s32                  D_80115750;
extern s32                  D_80115758;

s32 func_dryfield_motel_balcony_8017D5E8(RoomEventReq* arg0, RoomEventMsg* arg1)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                              = arg0->flagId;
    D_dryfield_motel_balcony_8018672C = 0;
    neg                               = flag < 0;
    got                               = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(arg0->itemId) != 0 || arg0->itemId == 0) {
            ret = 2;
            if (arg1->field_5 == 0) {
                D_dryfield_motel_balcony_80186724 = *arg1;
                D_dryfield_motel_balcony_80186730 = *arg0;
                id                                = arg0->flagId;
                mode                              = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_motel_balcony_80182270, 0, 0, 0);
                D_dryfield_motel_balcony_8018672C = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (arg1->field_5 == 0) {
            Gp_RunCapCmd1(arg0->field_4);
            Gp_SetNibbleIf(arg1->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}
INCLUDE_RODATA("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", D_dryfield_motel_balcony_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017D74C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017D8BC);

s32 func_dryfield_motel_balcony_8017DB1C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x8:
            Gp_EnqueueStageSnd6(0x521D0008, 0, 0);
            break;
        case 0x9:
            Gp_EnqueueStageSnd6(0x521D0009, 0, 0);
            break;
    }
    return 0;
}
s32 func_dryfield_motel_balcony_8017DB6C(void)
{
    return 0;
}

s32 func_dryfield_motel_balcony_8017DB74(void)
{
    return 0;
}

s32 func_dryfield_motel_balcony_8017DB7C(void)
{
    return 0;
}

void func_dryfield_motel_balcony_8017DB84(Task* arg0)
{
    arg0->field_24 = D_dryfield_motel_balcony_8018227C;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
}
void func_dryfield_motel_balcony_8017DBC8(void)
{
}

void func_dryfield_motel_balcony_8017DBD0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_balcony_8017D5DC;
    sp.funcs[task->state](task);
}
void func_dryfield_motel_balcony_8017DC28(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x60282;
        D_80115744  = 0x60283;
        D_8011573C  = 0x60284;
        D_80115720  = 0x60285;
        D_80115758  = 0x60005;
        D_8011572C  = 0x60073;
        D_80115750  = 0x60074;
        arg0->state = 1;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017DCB8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017DF84);

const TaskFuncTable3 D_dryfield_motel_balcony_8017D5DC = {
    func_dryfield_motel_balcony_8017DB84,
    (TaskFunc)func_dryfield_motel_balcony_8017DBC8,
    Task_Kill,
};
