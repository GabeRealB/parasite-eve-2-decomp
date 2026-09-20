#include "common.h"
#include "actors/actor_136300.h"
#include "gameplay/3CD8.h"
#include "gameplay/268.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "psyq/libgpu.h"

/// Script pair handed to `Gp_SpawnScript18`. Both live in gameplay's image, so
/// the overlay imports them by absolute address and passes them as `s32`.
extern s32 D_80114A24;
extern s32 D_80114A34;

extern TaskDesc D_actor_136300_8013B134;
extern TaskDesc D_80183380;
extern s8       D_8007272D;
extern s16      D_80071076;

extern s32 D_actor_136300_8013B208;
extern s32 D_actor_136300_8013B230;

/// Script pair handed to `func_800E8614` -- the first while the ending is being
/// armed, the second when the capture event is cancelled.
extern s32 D_actor_136300_8013C5C8;
extern s32 D_actor_136300_8013C6C0;

/// State machine for the capture-event actor: arms the ending, waits for the
/// capture key, then hands control to the boot loader and spawns the drop-in
/// task. The two `func_800E8614` calls and the `arg0->state += 1` blocks are
/// written out in every arm that needs them; jump optimization merges the
/// identical tails, so one copy of the increment lands between case 3 and case
/// 6 and one copy of the call lands after case 0. Hoisting either tail into a
/// shared `goto` target compiles to a different allocation - the call's address
/// then reaches `$a0` through `$v0` instead of being built there directly.
void func_actor_136300_8013267C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            gGameSession->hideHud = 1;
            Gp_MsgPlayerWeapon(0);
            func_800E8614((s32)&D_actor_136300_8013C5C8, 1);
            arg0->state += 1;
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                arg0->state += 1;
            }
            return;
        case 2:
            if (Gp_GetCapEventKey() == 2) {
                gGameSession->hideHud = 0;
                Gp_MsgPlayerWeapon(1);
                taskKill(arg0);
                return;
            }
            func_800E8614((s32)&D_actor_136300_8013C6C0, 1);
            arg0->state += 1;
            return;
        case 3:
            if (gGameSession->eventState != 1) {
                arg0->state += 1;
            }
            return;
        case 4:
        case 5:
            arg0->state += 1;
            return;
        case 6:
            SetDispMask(1);
            SndEvt_EnqueueType7(0x80000000, 0);
            GameFlag_SetNibble(0x7A, 4);
            GameFlag_SetNibble(0x97, 0);
            GameFlag_SetNibble(0x98, 1);
            GameFlag_SetNibble(0x9A, 1);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 0xF);
            GameFlag_SetNibble(0x4C, 4);
            Mc_SaveData.sceneEvent    = 9;
            Mc_SaveData.at4.loc.stage = 4;
            Mc_SaveData.at4.loc.area  = 1;
            Mc_SaveData.at4.loc.warp  = 1;
            Mc_SaveData.at4.loc.room  = 1;
            Fs_BeginBootLoad(&Mc_SaveData.at4.loc.view, 0);
            Gp_ClearCollectedBit(0x116);
            D_80071076 = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            return;
    }
}

/// Runs once on spawn, then counts `spawnArg1` down; when it goes negative the
/// ending flag is set and the task kills itself. The decrement is one reused
/// local: m2c's temp plus per-arm subtract splits the value into three
/// quantities and the store lands in `$v1` instead of `$v0`.
void func_actor_136300_80132854(Task* arg0)
{
    s32 var_v0;

    if (arg0->state == 0) {
        Gp_SpawnScript18((s32)&D_80114A24, (s32)&D_80114A34);
        arg0->state += 1;
    }
    var_v0 = arg0->spawnArg1;
    if (var_v0 < 0) {
        Stage_SetEndingFlag();
        taskKill(arg0);
        var_v0 = arg0->spawnArg1;
    }
    var_v0          = var_v0 - 1;
    arg0->spawnArg1 = var_v0;
}

void func_actor_136300_801328D4(s8 arg0)
{
    D_8007272D = arg0;
}

void func_actor_136300_801328E0(s32 arg0)
{
    Task_SpawnFromTable(&D_80183380, 0, arg0, 0);
}

/// Message handler for the start-countdown cue. A positive argument is latched
/// and nothing else happens; otherwise the CD command queue is dropped into
/// Mdec_DecodeToVram mode 2 and -- except for the -2 "already ran" message --
/// the spawn block is filled and the `D_actor_136300_80132AC4` entry started.
///
/// Both halves of the block are written in *each* arm of the countdown test so
/// that each arm is a complete two-store address session: jump optimization
/// then merges the identical tails and the countdown collapses to one `li` per
/// arm, which is what puts the block's `lui` in the delay slot of the entry
/// test. Hoisting `unk2` out of the arms compiles to a different allocation.
void func_actor_136300_80132910(s32 arg0)
{
    CdCmdQueue* queue;

    queue = &CdCmd_Queue;
    if (arg0 <= 0) {
        queue->field_22A = 2;
        if (arg0 != -2) {
            if (arg0 == 0) {
                D_actor_136300_8013C99C.field_0 = 0x64;
                D_actor_136300_8013C99C.field_2 = 0x100;
            } else {
                D_actor_136300_8013C99C.field_0 = 5;
                D_actor_136300_8013C99C.field_2 = 0x100;
            }
            Task_SpawnFromTable(&D_actor_136300_80132AC4, 0, 0, (s32)&D_actor_136300_8013C99C);
        }
    } else {
        D_actor_136300_8013C9A0 = arg0;
    }
}

void func_actor_136300_80132998(void)
{
    s32 temp_v0;

    temp_v0 = GameFlag_GetNibble(0x72);
    Gp_StartCapSlot((s16)(temp_v0 + 0x10), 0, 0);
    if (temp_v0 < 2) {
        GameFlag_SetNibble(0x72, temp_v0 + 1);
    }
}

void func_actor_136300_801329EC(void)
{
    s32* var_s0;

    if (Gp_GetCapEventKey() == 1) {
        var_s0 = &D_actor_136300_8013B208;
    } else {
        var_s0 = &D_actor_136300_8013B230;
    }
    Gp_AllyAnimId(var_s0);
    Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3E8, (s32)var_s0, 0);
}

void func_actor_136300_80132A4C(s32 arg0)
{
    Display_InitModeObj(&D_actor_136300_8013B134, arg0, 0, 0x100);
}

void func_actor_136300_80132A7C(s32 arg0)
{
    if (arg0 == 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x180, 0x100);
        return;
    }
    Gp_ResetCap();
}
