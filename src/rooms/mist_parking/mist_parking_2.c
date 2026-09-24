#include "common.h"
#include <psyq/libgte.h>
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "rooms/room_common.h"
#include "rooms/mist_parking.h"

extern s16 D_80071076;

extern s32 D_mist_parking_8018F374;
extern s32 D_mist_parking_8018F4AC;
extern s32 D_mist_parking_8018F5E4;
extern s32 D_mist_parking_8018F824;
extern s32 D_mist_parking_8018F9A4;
extern s32 D_mist_parking_8018FA4C;
extern s32 D_mist_parking_8018FB3C;
extern s32 D_mist_parking_8018FBFC[];
extern s32 D_mist_parking_8018FC10[];

typedef struct {
    /* 0x0 */ s16 timer;
    /* 0x2 */ s16 index;
} MistParkingScanState;

extern MistParkingScanState D_mist_parking_80195328;

void func_mist_parking_80182A44(Task* task)
{
    s32                   i;
    s32                   flag;
    s16                   idx;
    MistParkingScanState* st = &D_mist_parking_80195328;

    switch (task->state) {
        case 0:
            Mem_Set(st, 0, 4);
            func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
            Gp_RunCapCmd(2, 0);
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
            func_mist_parking_80183708(2);
            func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
            Gp_RunCapCmd(6, 0);
            task->state++;
            break;
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            switch (Gp_GetCapEventKey()) {
                case 1:
                    st->timer = 10;
                    Gp_RunCapCmd(7, 0);
                    task->state = 4;
                    break;
                case 4:
                    for (i = 0; i < 5; i++) {
                        flag = i + 0x125;
                        if (GameFlag_GetNibble(flag) == 2) {
                            GameFlag_SetNibble(flag, 3);
                        }
                    }
                    func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
                    Gp_RunCapCmd(8, 0);
                    task->state = 6;
                    break;
                case 3:
                    for (i = 0; i < 4; i++) {
                        flag = i + 0x125;
                        if (GameFlag_GetNibble(flag) == 2 && Gp_GiveItem(Gp_ScanPtrs[3], D_mist_parking_8018FBFC[i], D_mist_parking_8018FC10[i]) != 0) {
                            GameFlag_SetNibble(flag, 3);
                            Gp_SetCurBit2Flag(i + 0x20, 2);
                        }
                    }
                    if (GameFlag_GetNibble(0x129) == 2 && func_800B7420(0x6C) == 0) {
                        if (Gp_GiveItem(D_8010D55C, 0x6C, 1) != 0) {
                            GameFlag_SetNibble(0x129, 3);
                            Gp_SetCurBit2Flag(0x24, 2);
                        }
                    }
                    func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
                    Gp_RunCapCmd(4, 0);
                    task->state = 6;
                    break;
            }
            break;
        case 4:
            if (Gp_CapBusy() != 0) {
                return;
            }
            st->timer--;
            if (st->timer == 5) {
                idx = st->index;
                if (GameFlag_GetNibble(idx + 0x125) == 2) {
                    Gp_StartCapSlot(5, 0, idx);
                }
                return;
            }
            if (st->timer != 0) {
                return;
            }
            idx = st->index;
            if (Gp_GetCurBit2Flag(idx + 0x20) != 1) {
                GameFlag_SetNibble(idx + 0x125, 3);
            }
            st->timer = 10;
            st->index++;
            if (st->index >= 5) {
                task->state++;
            }
            break;
        case 5:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() == 0) {
                func_800E8614((s32)&D_mist_parking_8018F9A4, 1);
                Gp_RunCapCmd(2, 0);
                task->state++;
            }
            /* fallthrough */
        case 6:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            task->state++;
            break;
        case 7:
            task->killCountdown++;
            if (task->killCountdown >= 0xB) {
                func_mist_parking_80183708(0);
                Gp_RunCapCmd(4, 0);
                task->killCountdown = 0;
                task->state++;
            }
            break;
        case 8:
            if (Gp_CapBusy() != 0) {
                return;
            }
            if (Gp_GetCapEventKey() == 1) {
                func_800E8614((s32)&D_mist_parking_8018FA4C, 1);
            } else {
                func_800E8614((s32)&D_mist_parking_8018FB3C, 1);
            }
            task->state++;
            break;
        case 9:
            task->killCountdown++;
            if (task->killCountdown == 0xA) {
                Gp_RunCapCmd(3, 0);
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state++;
            break;
        case 10:
            Gp_MsgPlayerWeapon(1);
            taskKill(task);
            break;
    }
}

void func_mist_parking_80182F60(Task* task)
{
    s32 key;

    switch (task->state) {
        case 0:
            func_800E8614((s32)&D_mist_parking_8018F374, 1);
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
            key             = Gp_GetCapEventKey();
            task->spawnArg1 = key;
            switch (key) {
                case 4:
                    func_800E8614((s32)&D_mist_parking_8018F4AC, 1);
                    break;
                case 5:
                    func_800E8614((s32)&D_mist_parking_8018F5E4, 1);
                    break;
                case 6:
                    func_800E8614((s32)&D_mist_parking_8018F824, 1);
                    break;
            }
            task->state++;
            break;
        case 4:
            if (task->spawnArg1 == 4) {
                Gp_MsgPlayerWeapon(1);
            }
            taskKill(task);
            break;
    }
}

/// Attaches the task's model to the coordinate frame of part `spawnArg1` of
/// the model of the task in `spawnArg2`, sharing its light and colour
/// matrices, reparents the task under that one and steps it on.
void func_mist_parking_8018307C(Task* task)
{
    Task*          parent;
    s32            part;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = (TmdObject*)task->extra;
    parentExtra     = (TmdObject*)parent->extra;
    coord           = extra->coords;
    dest            = &parentExtra->coords[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// The empty per-frame state of `D_mist_parking_8017D7E8`.
void func_mist_parking_801830F8(Task* task)
{
}

void func_mist_parking_80183100(s32 arg0)
{
    Gp_StartCapSlot(arg0 >> 16, 0, arg0);
}

void func_mist_parking_8018312C(s32 arg0)
{
    Task_SpawnFromTable(&D_mist_parking_8018FC24, 0, arg0, 0);
    gGameSession->freezeRoomObjs = 1;
}

void func_mist_parking_8018316C(s32 arg0)
{
    Mc_SaveData.at4.loc.stage = 1;
    Mc_SaveData.at4.loc.warp  = 1;
    Mc_SaveData.at4.loc.room  = 1;
    Mc_SaveData.at4.loc.area  = arg0;
    D_80071076                = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    if (arg0 == 5) {
        Fs_BeginBootLoad(&Mc_SaveData.at4.loc.view, 0);
    }
}

void func_mist_parking_801831F0(s32 arg0)
{
    Task**     slot;
    Task*      task;
    TmdObject* obj;

    if (arg0 == 0) {
        slot = &D_mist_parking_80195320;
    } else {
        slot = NULL;
    }

    if ((slot != NULL) && (*slot == NULL)) {
        task  = Task_SpawnFromTable(&D_mist_parking_8018D75C, arg0, 0, 0);
        *slot = task;
        if (task != NULL) {
            obj         = (TmdObject*)task->extra;
            obj->flags &= ~0x80;
        }
    }
}

void func_mist_parking_8018326C(s32 arg0)
{
    if (arg0 == 0) {
        if (D_mist_parking_80195320 != NULL) {
            taskKill(D_mist_parking_80195320);
        }
        D_mist_parking_80195320 = NULL;
    }
}

/// Runs the handler for the task's state from a stack copy of
/// `D_mist_parking_8017D7F4`: the text block's setup, its wait and its exit.
void func_mist_parking_801832AC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_parking_8017D7F4;
    sp.funcs[task->state](task);
}

/// The two text lines of the block `func_mist_parking_80183304` shows, and
/// the alternative pair it uses when the task's `spawnArg1` is 1.
extern u8* D_mist_parking_8018DF24[4];

/// Allocates a two-line text block, parks it at `Task::work`, spawns it and
/// steps the task on; `func_mist_parking_80183434` is set as the exit
/// callback.
void func_mist_parking_80183304(Task* task)
{
    RoomTextBlock* block;
    TextLineNode*  node;
    u8**           line;
    s32            table;
    s32            off;
    s32            mode;
    s32            i;

    block = memCalloc(sizeof(RoomTextBlock), 0);
    node  = block->lines;
    if (block == NULL) {
        taskKill(task);
        return;
    }

    i                  = 0;
    mode               = 1;
    line               = D_mist_parking_8018DF24;
    table              = (s32)D_mist_parking_8018DF24;
    off                = 8;
    task->work         = (TaskIdMap*)block;
    task->exitCallback = func_mist_parking_80183434;

    for (; i < 2; i++) {
        if (task->spawnArg1 == mode) {
            node->text = *(u8**)(off + table);
        } else {
            node->text = *line;
        }
        node->next = node + 1;
        node++;
        line++;
        off += 4;
    }
    node[-1].next = NULL;

    block->desc.count   = 2;
    block->desc.lines   = block->lines;
    block->desc.field_8 = 0;
    block->field_C      = 0;
    Ui_SpawnTextBlock(&block->desc, 0, 0, 0);
    task->state++;
}

/// Waits for the text block parked at `Task::work` to report a non-zero
/// `TextBlockDesc::field_2`, stores it through `Task::spawnArg2` and steps
/// the task on.
void func_mist_parking_801833F8(Task* task)
{
    s16 result;

    result = ((RoomTextBlock*)task->work)->desc.field_2;
    if (result != 0) {
        *(s32*)task->spawnArg2 = result;
        task->state            = task->state + 1;
    }
}

/// Exit callback of the text-block task: kills it and calls
/// `Stage_SetEndingFlag`.
void func_mist_parking_80183434(Task* arg0)
{
    taskKill(arg0);
    Stage_SetEndingFlag();
}

extern s32 D_mist_parking_8019531C;

void func_mist_parking_8018345C(Task* arg0)
{
    if (gGameSession->eventState == 0 && Gp_CapBusy() == 0) {
        if (D_mist_parking_8019531C == 2) {
            func_800E8614((s32)&D_mist_parking_8018F5E4, 1);
        } else {
            func_800E8614((s32)&D_mist_parking_8018F4AC, 1);
        }
        taskKill(arg0);
    }
}

void func_mist_parking_801834D4(Task* arg0)
{
    if (gGameSession->eventState == 0 && Gp_CapBusy() == 0) {
        if (D_mist_parking_8019531C == 2) {
            func_800E8614((s32)&D_mist_parking_8018FB3C, 1);
        } else {
            func_800E8614((s32)&D_mist_parking_8018FA4C, 1);
        }
        taskKill(arg0);
    }
}

/// Spawns entry 3 of `D_mist_parking_8018D75C`.
void func_mist_parking_8018354C(void)
{
    Task_SpawnFromTable(&D_mist_parking_8018D75C, 3, 0, 0);
}

void func_mist_parking_8018357C(Task* arg0)
{
    func_800BC4E4();
    Mc_SaveData.at4.loc.stage = 2;
    Mc_SaveData.at4.loc.area  = 1;
    Mc_SaveData.at4.loc.warp  = 1;
    Mc_SaveData.at4.loc.room  = 1;
    D_80071076                = 1;
    Fs_BeginBootLoad(&Mc_SaveData.at4.loc.view, 1);
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    taskKill(arg0);
}

/// Spawns entry 4 of `D_mist_parking_8018D75C` and keeps its handle in
/// `D_mist_parking_80195324`.
void func_mist_parking_80183600(void)
{
    D_mist_parking_80195324 = Task_SpawnFromTable(&D_mist_parking_8018D75C, 4, 0, 0);
}
