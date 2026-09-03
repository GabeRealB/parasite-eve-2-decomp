#include "common.h"

#include <psyq/stdio.h>

#include "gameplay/1A8.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/acropolis_security_room.h"

extern Task* D_acropolis_security_room_801855A8;
extern Task* D_acropolis_security_room_801855AC;

extern GpMsgEntry D_acropolis_security_room_801825DC[];
extern TaskDesc   D_acropolis_security_room_80182618;

/// The five camera ids the security monitor can display, in the order the
/// `GameFlag_GetNibble(0x2A)` nibble indexes them (see
/// `func_acropolis_security_room_8017D9DC`, which seeds `AsrMonitorWork::cameraId`
/// from this table).
extern s16 D_acropolis_security_room_801826B4[];

extern s8  D_8007216C;
extern s16 D_80114D08;

void func_acropolis_security_room_8017E490(Task* task);
void func_acropolis_security_room_8017EDE4(Task* task);

s32 func_acropolis_security_room_8017D6DC(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 ret;

    if (arg0 == NULL) {
        ret = 0;
    } else {
        ret = Gp_DispatchMsg(arg0, arg1, arg2, arg3);
    }
    return ret;
}

s32 func_acropolis_security_room_8017D708(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        Task_SpawnFromTable(&D_acropolis_security_room_80182618, 1, 0, 0);
    }
    return 0;
}

void func_acropolis_security_room_8017D740(Task* arg0, s32 arg1, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 0) {
        Task_SpawnFromTable(&D_acropolis_security_room_80182618, 0, 0, 0);
    }
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", D_acropolis_security_room_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", D_acropolis_security_room_8017D5C4);

void func_acropolis_security_room_8017D77C(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            printf("monitor\n");
            D_acropolis_security_room_801855A8 = Task_Spawn(2, 9, 0, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Task_PollKill(D_acropolis_security_room_801855A8, &sp10) != 0) {
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                Task_Kill(arg0);
            }
            return;
    }
}

/* "power supply\n" plus the two bytes of alignment padding the original
 * object left in .rodata, so the block stays 16 bytes of the same content. */
static const char PowerSupplyMsg[16] = "power supply\n\0@\021";

void func_acropolis_security_room_8017D834(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            printf(PowerSupplyMsg);
            D_acropolis_security_room_801855AC = Task_Spawn(2, 0xA, 0, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(2);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Task_PollKill(D_acropolis_security_room_801855AC, &sp10) != 0) {
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F3, 1, 0);
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                D_acropolis_security_room_801855AC = NULL;
                Task_Kill(arg0);
            }
            return;
    }
}

void func_acropolis_security_room_8017D930(Task* arg0)
{
    arg0->field_24 = D_acropolis_security_room_801825DC;
    Game_SetPtrSlot(arg0, 7);
    arg0->state                        = arg0->state + 1;
    D_acropolis_security_room_801855AC = NULL;
}

void func_acropolis_security_room_8017D97C(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017D984);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017D9DC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017DB30);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017DC7C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017DE80);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017E0C4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017E37C);

void func_acropolis_security_room_8017D9DC(Task* task);
void func_acropolis_security_room_8017DB30(Task* task);
void func_acropolis_security_room_8017DC7C(Task* task);
void func_acropolis_security_room_8017EA28(Task* task);
void func_acropolis_security_room_8017EA5C(Task* task);
void func_acropolis_security_room_8017EADC(Task* task);
void func_acropolis_security_room_8017EB9C(Task* task);

/// States of the security-monitor task, dispatched by
/// `func_acropolis_security_room_8017ED68`: set up the work block, run the
/// camera list, redraw the panel, confirm a camera, and leave the monitor.
static const AsrMonitorStateTable AsrMonitorStates = {
    { {
        func_acropolis_security_room_8017D9DC,
        func_acropolis_security_room_8017EA28,
        func_acropolis_security_room_8017DB30,
        func_acropolis_security_room_8017EA5C,
        func_acropolis_security_room_8017DC7C,
        func_acropolis_security_room_8017EADC,
        func_acropolis_security_room_8017EB9C,
    } },
    NULL,
};

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017E490);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017E8F0);

/// Two-state dispatcher whose handler table is built on the stack rather than
/// read from `.data`: state 0 runs `func_acropolis_security_room_8017EDE4` and
/// state 1 runs `func_acropolis_security_room_8017E490`.
void func_acropolis_security_room_8017E9D8(Task* task)
{
    TaskFunc funcs[2] = {
        func_acropolis_security_room_8017EDE4,
        func_acropolis_security_room_8017E490,
    };

    funcs[task->state](task);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017EA28);

/// Confirms the camera the player picked on the security monitor: clears the
/// action prompt, redraws the panel for the selected camera plus its cursor
/// overlay, spawns the prompt at the panel's coordinates and advances the task.
void func_acropolis_security_room_8017EA5C(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    AsrMonitorWork*   work   = (AsrMonitorWork*)task->idMap;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_acropolis_security_room_8017E0C4(work->cameraId - 0x7F);
    func_acropolis_security_room_8017E37C(task);
    func_800D4E78(prompt->screenX, prompt->screenY, work->promptKind);
    task->state = 4;
}

/// Leaves the security monitor: records which camera was on screen as the
/// `0x2A` nibble (index into `D_acropolis_security_room_801826B4`, 0 if the id
/// is not in the table), restores the room's normal display state and kills the
/// monitor task along with the child task it spawned.
void func_acropolis_security_room_8017EADC(Task* task)
{
    AsrMonitorWork* work;
    s16*            camera;
    s32             index;
    s32             cameraId;

    index      = 0;
    camera     = D_acropolis_security_room_801826B4;
    work       = (AsrMonitorWork*)task->idMap;
    D_80114D08 = 0xA;
    cameraId   = (s16)work->cameraId;
loop:
    if (cameraId != *camera) {
        index  += 1;
        camera += 1;
        if (index >= 5) {
            GameFlag_SetNibble(0x2A, 0);
            goto done;
        }
        goto loop;
    }
    GameFlag_SetNibble(0x2A, index);
done:
    D_8007216C = 4;
    Display_ReleaseRef();
    Game_Session->field_66 = 0;
    Game_Session->field_68 = 0;
    Game_Session->field_1  = 0;
    Task_Kill((Task*)task->spawnArg2);
    Task_RequestKill(task, 0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017EB9C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017ECB4);

/// Runs the security monitor's current state. The seven handlers are copied
/// onto the stack first, so the call goes through a local table rather than
/// through `.rodata`.
void func_acropolis_security_room_8017ED68(Task* task)
{
    TaskFuncTable7 sp;

    sp = AsrMonitorStates.states;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017EDE4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", func_acropolis_security_room_8017EE44);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_security_room/acropolis_security_room", D_acropolis_security_room_8017D63C);
