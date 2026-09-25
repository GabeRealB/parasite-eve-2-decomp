#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>
#include "actors/actor.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"

/// Script pair handed to `Gp_SpawnScript18`. Both live in gameplay's image, so
/// the overlay imports them by absolute address and passes them as `s32`.
extern s32 D_80114A24;
extern s32 D_80114A34;

extern TaskDesc D_actor_136300_8013B134;
extern TaskDesc D_80183380;
extern s8       D_8007272D;

extern s32 D_actor_136300_8013B208;
extern s32 D_actor_136300_8013B230;

/// Script pair handed to `func_800E8614` -- the first while the ending is being
/// armed, the second when the capture event is cancelled.
extern s32 D_actor_136300_8013C5C8;
extern s32 D_actor_136300_8013C6C0;

/// Distortion amplitude of the screen wave, `frame * scale / span` of the
/// running context, recomputed every frame.
extern s32 D_actor_136300_80132ADC;

/// The ramp context the running wave task was spawned with, parked at spawn
/// so the tick reads the ramp through it.
extern OverlayWaveCtx* D_actor_136300_8013C888;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed every frame.
extern OverlayWaveRec6 D_actor_136300_8013C88C[11];
extern OverlayWaveRec6 D_actor_136300_8013C8DC[30];

/// The ramp context the message handler seeds and hands to the screen-wave
/// task.
extern OverlayWaveCtx D_actor_136300_8013C99C;

/// Spawn table of the screen-wave task.
extern TaskDesc D_actor_136300_80132AC4;

/// Screen-wave task, spawned by the message handler with the ramp context
/// `D_actor_136300_8013C99C` as its argument. State 0 seeds the column and row
/// phases, parks the argument and clears its ramp; state 1 ramps the frame up
/// to the span (ramp state 0) or back down to zero (ramp state 1, then 2,
/// which kills the task and restores the display field), and redraws the
/// frame buffer as a 10 by 30 mesh of textured quads displaced by sine waves
/// of that amplitude, tinted when the context's tint flag is set.
///
/// `Task::state` is read as a scalar through a cast: that keeps the load
/// behind the `D_800691CA` store, which a member read lets GCC hoist above it.
void func_actor_136300_80131E40(Task* arg0)
{
    OverlayWaveCtx* ctx;
    POLY_FT4*       p;
    DR_STP*         stp;
    s32             i, j, k;
    s32             drawY;
    s32             tpage0, tpage1;
    s32             u0, u1, v0, v1;
    s32             waveX0, waveY0, waveX1, waveY1;
    s32             waveX2, waveY2, waveX3, waveY3;

    CdCmd_Queue.field_22A = 2;
    /* `Task::state` read as a scalar through a cast: that keeps the load
       behind the `CdCmd_Queue.field_22A` store, which a member read lets GCC hoist
       above it. */
    switch (*(s32*)((u8*)arg0 + OFFSET_OF(Task, state))) {
        case 0:
            for (i = 0; i < 11; i++) {
                D_actor_136300_8013C88C[i].phase  = 0;
                D_actor_136300_8013C88C[i].offset = (u32)rand() >> 3;
                D_actor_136300_8013C88C[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                D_actor_136300_8013C8DC[i].phase  = 0;
                D_actor_136300_8013C8DC[i].offset = (u32)rand() >> 3;
                D_actor_136300_8013C8DC[i].speed  = (rand() * 100 + 20) >> 15;
            }
            D_actor_136300_80132ADC        = 0;
            D_actor_136300_8013C888        = arg0->spawnArg2;
            D_actor_136300_8013C888->frame = 0;
            D_actor_136300_8013C888->state = 0;
            Display_ClampField126(-8);
            arg0->state++;
            break;
        case 1:
            ctx = D_actor_136300_8013C888;
            switch (ctx->state) {
                case 0:
                    if (ctx->frame < ctx->span) {
                        ctx->frame++;
                    }
                    break;
                case 1:
                    if (ctx->frame > 0) {
                        ctx->frame--;
                    } else {
                        ctx->state = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    Display_ClampField126(0);
                    break;
            }
            D_actor_136300_80132ADC = D_actor_136300_8013C888->frame * D_actor_136300_8013C888->scale / D_actor_136300_8013C888->span;
            for (i = 0; i < 11; i++) {
                D_actor_136300_8013C88C[i].phase += D_actor_136300_8013C88C[i].speed;
            }
            for (i = 0; i < 30; i++) {
                D_actor_136300_8013C8DC[i].phase += D_actor_136300_8013C8DC[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)(p + 1);
                    setPolyFT4(p);
                    if (D_actor_136300_8013C888->blend == 0) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = D_actor_136300_8013C888->r;
                        p->g0 = D_actor_136300_8013C888->g;
                        p->b0 = D_actor_136300_8013C888->b;
                    }
                    u0 = k * 32;
                    u1 = (k + 1) * 32;
                    if (u1 == 320)
                        u1 = 319;
                    if (u0 < 128) {
                        p->tpage = tpage0;
                    } else {
                        p->tpage = tpage1;
                        u0      -= 128;
                        u1      -= 128;
                    }
                    if (j != -1) {
                        v1     = (j + 1) * 8 + gDisplayState.drawBuffer * 16;
                        v0     = j * 8 + gDisplayState.drawBuffer * 16;
                        waveX0 = D_actor_136300_80132ADC * (rsin((j << 9) + D_actor_136300_8013C88C[k].phase + D_actor_136300_8013C88C[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_actor_136300_80132ADC * (rsin((k << 10) + D_actor_136300_8013C8DC[j].phase + D_actor_136300_8013C8DC[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_actor_136300_80132ADC * (rsin((j << 9) + D_actor_136300_8013C88C[k + 1].phase + D_actor_136300_8013C88C[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_actor_136300_80132ADC * (rsin(((k + 1) << 10) + D_actor_136300_8013C8DC[j].phase + D_actor_136300_8013C8DC[j].offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        drawY = gDisplayState.drawBuffer * 16;
                        p->x0 = k * 32 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 32 - 160;
                        p->y1 = -112;
                        v0    = drawY + 8;
                        v1    = drawY;
                    }
                    {

                        waveX2 = D_actor_136300_80132ADC * (rsin(((j + 1) << 9) + D_actor_136300_8013C88C[k].phase + D_actor_136300_8013C88C[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = D_actor_136300_80132ADC * (rsin((k << 10) + D_actor_136300_8013C8DC[j + 1].phase + D_actor_136300_8013C8DC[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = D_actor_136300_80132ADC * (rsin(((j + 1) << 9) + D_actor_136300_8013C88C[k + 1].phase + D_actor_136300_8013C88C[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = D_actor_136300_80132ADC * (rsin(((k + 1) << 10) + D_actor_136300_8013C8DC[j + 1].phase + D_actor_136300_8013C8DC[j + 1].offset) << 3);
                        p->y3  = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    p->u0 = u0;
                    p->v0 = v0;
                    p->u1 = u1;
                    p->v1 = v0;
                    p->u2 = u0;
                    p->v2 = v1;
                    p->u3 = u1;
                    p->v3 = v1;
                    addPrim(&gGpuCurrentOt[3], p);
                }
            }
            break;
    }
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
}

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
            gDisplayState.roomVariant = 1;
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

/// Message handler driving the screen wave. A positive argument is written
/// into the wave's ramp state (1 ramps the wave back down, after which the
/// task ends). Otherwise the CD command queue's `field_22A` is set to 2 and,
/// except for the -2 message, the ramp context is seeded (span 0x64 for 0,
/// 5 otherwise, scale 0x100) and the screen-wave task
/// `D_actor_136300_80132AC4` is spawned with it.
///
/// Both halves of the context are written in *each* arm of the span test so
/// that each arm is a complete two-store address session: jump optimization
/// then merges the identical tails and the span collapses to one `li` per
/// arm, which is what puts the block's `lui` in the delay slot of the entry
/// test. Hoisting the scale store out of the arms compiles to a different
/// allocation.
void func_actor_136300_80132910(s32 arg0)
{
    CdCmdQueue* queue;

    queue = &CdCmd_Queue;
    if (arg0 <= 0) {
        queue->field_22A = 2;
        if (arg0 != -2) {
            if (arg0 == 0) {
                D_actor_136300_8013C99C.span  = 0x64;
                D_actor_136300_8013C99C.scale = 0x100;
            } else {
                D_actor_136300_8013C99C.span  = 5;
                D_actor_136300_8013C99C.scale = 0x100;
            }
            Task_SpawnFromTable(&D_actor_136300_80132AC4, 0, 0, (s32)&D_actor_136300_8013C99C);
        }
    } else {
        D_actor_136300_8013C99C.state = arg0;
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
