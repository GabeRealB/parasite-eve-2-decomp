#include "common.h"

#include "actors/actor_450900.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/unknown_syms.h"

extern s16 D_80071076;
extern u8  D_801153F4;
extern s32 D_8017A99C;
extern s8  D_actor_450900_80135E70;
extern s32 D_actor_450900_80135E74;
extern s32 D_actor_450900_80135F00;
extern s32 D_actor_450900_80135F24;
extern s32 D_actor_450900_801360B4;
extern s32 D_actor_450900_80136470;
extern s32 D_actor_450900_80136680;
extern s32 D_actor_450900_80136890;
extern s32 D_actor_450900_80136B00;
extern s32 D_actor_450900_80136BD8;

/// The save-point capture task spawned by `func_actor_450900_80131E38`, kept
/// alive until `func_actor_450900_80132548` kills it. Script opcode 0xD reaches
/// both this and `func_actor_450900_80132678`, so its one argument is the
/// opcode's immediate.
extern Task* D_actor_450900_80136C9C;

/// This overlay's own spawn table, six `TaskDesc` entries. Index 0 is the exit
/// handler `taskKill`; 1..5 are the overlay's state handlers, and the "next
/// stage" of each is the next entry: `func_actor_450900_80131E38` spawns 5 on
/// its way through, and `func_actor_450900_80132834` spawns 4
/// (`func_actor_450900_8013235C`, the save-data teardown) when the ally has
/// walked past the trigger line.
extern TaskDesc D_actor_450900_80135E78;

/// State handler that runs the save-point capture. State 0 spawns the capture
/// task `func_actor_450900_80132548` into `D_actor_450900_80136C9C`; state 1
/// waits for `D_8017A99C`, the AI tick counter, to pass 0x30C with save data in
/// the slot, then arms the flag `func_actor_450900_80132518` toggles and, on
/// every 210th tick, plays the ally's voice cue at its own pan and depth and
/// posts the `0x3F7` / `0x3E8` / `0x3F9` messages to the slot-0xA task; 0x3C
/// ticks later it posts `0x3E8` alone, with the capture-indicator animation.
/// The one-shot `D_actor_450900_80135E74` retires the handler after one pass.
void func_actor_450900_80131E38(Task* task)
{
    GsCOORDINATE2* coord;
    s32            state;
    s32            t;
    s8             pan;
    s8             depth;
    void*          slot;

    slot  = gameGetPtrSlot(0xA);
    state = task->state;
    switch (state) {
        case 0:
            D_actor_450900_80135E70 = 0;
            D_actor_450900_80136C9C = Task_SpawnFromTable(&D_actor_450900_80135E78, 5, 0, 0);
            task->state             = task->state + 1;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (gGameSession->eventState != 0) {
                break;
            }
            if (D_801153F4 != 0) {
                break;
            }
            if (Mc_SaveData.demoScene != 0xB) {
                D_8017A99C = D_8017A99C + 1;
            }
            t = D_8017A99C - 0x30C;
            if (D_actor_450900_80135E74 == 0 && (s16)Mc_SaveData.companionHp > 0 && t >= 0) {
                D_actor_450900_80135E70 = state;
                if (t % 210 == 0) {
                    coord = ((TmdObject*)(gameGetPtrSlot(0xA))->extra)->coords;
                    pan   = (s8)Gp_GetObjPan(coord);
                    depth = (s8)gpGetObjDepth(coord);
                    if (rand() & 1) {
                        SndEvt_EnqueueType6(0x55170005, pan, depth);
                    } else {
                        SndEvt_EnqueueType6(0x55170006, pan, depth);
                    }
                    Gp_AllyAnimId(&D_actor_450900_80135F24);
                    Gp_DispatchMsg(slot, 0x3F7, &D_actor_450900_80135F00, 0);
                    Gp_DispatchMsg(slot, 0x3E8, &D_actor_450900_80135F24, 0);
                    Gp_DispatchMsg(slot, 0x3F9, (s32*)0x40010, 0);
                } else if (t % 210 == 0x3C) {
                    Gp_AllyAnimId(&D_actor_450900_801360B4);
                    Gp_DispatchMsg(slot, 0x3E8, &D_actor_450900_801360B4, 0);
                }
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_8013207C);

void func_actor_450900_8013223C(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_RunCapCmd(1, 0);
            task->state = task->state + 1;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state = task->state + 1;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() != 0xB) {
                goto kill;
            }
            GameFlag_SetNibble(0xD8, 1);
            D_801153F4 = 1;
            Gp_RunCapCmd(2, 0);
            func_800E8614((s32)&D_actor_450900_80136B00, 0);
            task->state = task->state + 1;
            break;
        case 3:
            if (gGameSession->eventState == 0) {
            kill:
                Gp_MsgPlayerWeapon(1);
                D_801153F4 = 0;
                taskKill(task);
            }
            break;
    }
}

void func_actor_450900_8013235C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StartCapSlot(0xB, 1, 1);
            task->state = task->state + 1;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state = task->state + 1;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() != 0xB) {
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
            } else {
                func_800E8614((s32)&D_actor_450900_80136BD8, 0);
                task->state = task->state + 1;
            }
            break;
        case 3:
            if (gGameSession->eventState == 2) {
                task->state = task->state + 1;
            }
            break;
        case 4:
            GameFlag_SetNibble(0x4D, 0);
            GameFlag_SetNibble(0xFC, 1);
            GameFlag_SetNibble(0xA5, 0);
            GameFlag_SetNibble(0xD9, 0);
            GameFlag_SetNibble(0xAB, 1);
            GameFlag_SetNibble(0x1C7, 0);
            GameFlag_SetNibble(0xD2, 0);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 8);
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.area  = 0xF;
            Mc_SaveData.at4.loc.warp  = 3;
            Mc_SaveData.companionType = 0;
            Mc_SaveData.at4.loc.room  = 1;
            D_80071076                = 1;
            Task_Spawn(0, 0x11, 0, 0);
            Gp_RestoreStreamRng();
            taskKill(task);
            break;
    }
}

/// Script callback: arms or disarms the save-point capture task's flag
/// (`Task::spawnArg1`, the value `func_actor_450900_80132548` tests to decide
/// which way the capture cursor sweeps).
void func_actor_450900_80132518(s32 arg0)
{
    if (D_actor_450900_80136C9C != NULL) {
        if (arg0 == 1) {
            D_actor_450900_80136C9C->spawnArg1 = 0;
            return;
        }
        D_actor_450900_80136C9C->spawnArg1 = 1;
    }
}

/// State handler of the save-point capture task `func_actor_450900_80131E38`
/// spawns. State 0 allocates the head-aim record the capture cursor sweeps with
/// (`memCalloc(0xC, false)` into `Task::work`); state 1 ramps its `rate` one
/// 0x200 step per frame, up or down according to `Task::spawnArg1` (the flag
/// `func_actor_450900_80132518` arms), and hands the record to `func_800B17D4`
/// between the slot-3 task and the ally's own slot-0xA task. Any other state
/// kills the task and drops the overlay's handle to it.
void func_actor_450900_80132548(Task* task)
{
    Actor450900HeadAim* aim;
    void*               slot;
    u16                 rate;

    slot = gameGetPtrSlot(3);
    switch (task->state) {
        case 0:
            aim = memCalloc(sizeof(Actor450900HeadAim), false);
            if (aim == NULL) {
                taskKill(task);
                return;
            }
            task->work      = (TaskIdMap*)aim;
            aim->yawLimit   = 0x100;
            aim->pitchLimit = 0x200;
            task->state++;
            /* fallthrough */
        case 1:
            aim = (Actor450900HeadAim*)task->work;
            if (task->spawnArg1 != 0) {
                rate      = aim->rate + 0x200;
                aim->rate = rate;
                if ((s16)rate >= 0x1001) {
                    aim->rate = 0x1000;
                }
            } else {
                rate      = aim->rate - 0x200;
                aim->rate = rate;
                if ((s16)rate < 0) {
                    aim->rate = 0;
                }
            }
            func_800B17D4(slot, gameGetPtrSlot(0xA), (GpHeadAim*)aim);
            return;
        default:
            taskKill(task);
            D_actor_450900_80136C9C = NULL;
            return;
    }
}

void func_actor_450900_80132678(u8 arg0)
{
    D_801153F4 = arg0;
}

/// Plays the ally's voice cue at its own pan and depth: `arg0` picks the
/// non-random id, otherwise one of the two `0x55170005/6` takes is chosen.
void func_actor_450900_80132684(s32 arg0)
{
    GsCOORDINATE2* coord;
    s8             pan;
    s8             depth;

    coord = ((TmdObject*)(gameGetPtrSlot(0xA))->extra)->coords;
    pan   = (s8)Gp_GetObjPan(coord);
    depth = (s8)gpGetObjDepth(coord);
    if (arg0 != 0) {
        SndEvt_EnqueueType6(0x55170007, pan, depth);
    } else if (rand() & 1) {
        SndEvt_EnqueueType6(0x55170005, pan, depth);
    } else {
        SndEvt_EnqueueType6(0x55170006, pan, depth);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_450900/actor_450900", func_actor_450900_80132724);

/// Spawns the ally's save-point state handler. Once flag 0xD8 is set (the
/// capture ran) the one-shot `D_actor_450900_80135E74` swaps the ally onto the
/// `D_actor_450900_80136890` handler the first time through, and every later
/// call just re-arms the idle capture. Before the flag is set the handler is
/// picked by the AI tick counter `D_8017A99C`: the low-traffic
/// `D_actor_450900_80136470` below 0x30C, `D_actor_450900_80136680` at or above
/// it. The three calls are written out at each site - the `jal` is shared only
/// because `jump.c` cross-jumps the identical tails.
void func_actor_450900_801327A8(void)
{
    if (GameFlag_GetNibble(0xD8) != 0) {
        if (D_actor_450900_80135E74 == 0) {
            D_actor_450900_80135E74 = 1;
            func_800E8614((s32)&D_actor_450900_80136890, 0);
        } else {
            Gp_SpawnIfCapIdle(0xC, 1);
        }
    } else if (D_8017A99C < 0x30C) {
        func_800E8614((s32)&D_actor_450900_80136470, 0);
    } else {
        func_800E8614((s32)&D_actor_450900_80136680, 0);
    }
}

/// Save-point gate: reads the ally actor's root coordinate and, once it has
/// walked past `Z < -0x76C`, hands over to the save-data teardown task instead
/// of starting the save-point capture. The chain is the one
/// `func_actor_450900_80132684` and `func_actor_161500_80132210` use: the ally
/// task's `Task::extra` is its `TmdObject`, whose `field_8` is the root
/// `GsCOORDINATE2`, so `coord.t[2]` is that coordinate's world Z.
void func_actor_450900_80132834(void)
{
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)(gameGetPtrSlot(0xA))->extra)->coords;
    if (coord->coord.t[2] < -0x76C) {
        Task_SpawnFromTable(&D_actor_450900_80135E78, 4, 0, 0);
    } else {
        Gp_StartCapSlot(0xB, 1, 0);
    }
}
