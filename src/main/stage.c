#include "main/stage.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libpress.h>

#include "common.h"

#include "main/coord.h"
#include "main/display.h"
#include "display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "fs.h"
#include "main/fs_types.h"
#include "fs_types.h"
#include "main/game_debug_types.h"
#include "gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "stage.h"
#include "main/stream.h"
#include "stream.h"
#include "main/task.h"
#include "task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "gameplay/actor_render.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/player_actor.h"
#include "gameplay/world_collision.h"

/// Stage / flow context (Stage_Ctx → bss Stage_Context, size 0x38).
typedef struct _StageCtx {
    /* 0x00 */ TaskDesc*    field_0; // task desc table for spawn
    /* 0x04 */ s32          field_4; // spawn arg
    /* 0x08 */ TaskSpawnArg field_8; // second task spawn payload
    /* 0x0C */ u32          field_C;
    /* 0x10 */ byte         unknown_10;
    /* 0x11 */ u8           field_11;
    /* 0x12 */ u8           field_12; // flow gate
    /* 0x13 */ u8           field_13;
    /* 0x14 */ u8           field_14;
    /* 0x15 */ u8           field_15;
    /* 0x16 */ byte         unknown_16;
    /* 0x17 */ u8           field_17; // flow gate
    /* 0x18 */ u8           field_18;
    /* 0x19 */ u8           field_19; // flag bits (bit0/1)
    /* 0x1A */ u8           field_1a;
    /* 0x1B */ byte         unknown_1b;
    /* 0x1C */ u32          field_1c;    // flag word
    /* 0x20 */ s32          field_20;
    /* 0x24 */ s32          field_24;    // last gDisplayState.frameBuffer
    /* 0x28 */ s32          field_28;    // step counter
    /* 0x2C */ u8           field_2C[8]; // CDF load param block
    /* 0x34 */ u8           field_34[4]; // CDF load param block
} StageCtx;
STATIC_ASSERT_SIZEOF(StageCtx, 0x38);

static StageCtx Stage_Context;

static s32 D_8007A358;

static u16 D_8007A35C;

static u16 D_8007A35E;

static void* D_8007A360;

static u8* Mdec_DecodeBase;

static StreamSceneImageHeader* Stage_CdEntry;

static GameDebugState Pad_DefaultRemapState;

/// Active stage/flow context pointer.
static StageCtx* Stage_Ctx;

static TaskDesc Display_ModeTaskDesc;

static const TaskFuncTable6 Display_TaskStates;

void func_80701470(Task* arg0);

static void Display_StepFadeOverlay(void);

static s32 Display_TransitionLoad(Task* unused);

static Task* Display_SpawnFromMode(void);

static void Display_TransitionTask(Task* task);

static void Display_FlipOtAndDispatch(s32 unused);

static void Display_InvertFramebufferGray(void);

/// Transition kinds 3 and 7: same field_1c 0x40000000 handshake as
/// Stage_BeginTransition, with StageCtx::field_11 fixed to 3 and 7.
static s32 Stage_BeginTransitionKind3(void);

static void Stage_SetModeAndFlip(u8 arg0);

static void Stage_WaitCdActivate(Task* task);

static void Stage_WaitCdAndSpawn(Task* task);

static void Display_TaskLoadStep(Task* task);

static void Stage_WaitCdEntry(Task* task);

static void Stage_FinishCdFollowUp(Task* task);

static void Display_DispatchTaskTable(Task* task);

static __inline__ void mdecFinishDecode(void);

/// imageDecodeStep state machine: start DCT, apply work-lists / image chunks, complete.
static void Mdec_ProcessDecode(void);

static void Mdec_DecodeToVram(void);

static void Mdec_StripCallback(void);

// resolved decode base (Mdec_ResolveStreamBuffer)
// matched gCdCmdQueue.sceneImageHeaders entry

/// Active stage/flow context pointer.
static StageCtx* Stage_Ctx            = &Stage_Context;
static TaskDesc  Display_ModeTaskDesc = { { { TASK_BODY_NONE, 0 } }, Display_DispatchTaskTable };
GameDebugState*  Pad_RemapState       = &Pad_DefaultRemapState;
TaskDesc         D_800626AC[]         = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, Task_KillMaybeSpawn },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80701470 },
};

static const TaskFuncTable6 Display_TaskStates = { {
    Stage_WaitCdActivate,
    Stage_WaitCdAndSpawn,
    Display_TransitionTask,
    Display_TaskLoadStep,
    Stage_WaitCdEntry,
    Stage_FinishCdFollowUp,
} };

static void Display_StepFadeOverlay(void)
{
    StageCtx* p;
    s32       temp;
    s32       product;
    s32       otIdx;
    s32       yoff;
    u8        max;
    u8        val;
    TILE*     tile;
    DR_TPAGE* dr;

    p = Stage_Ctx;
    if ((s8)p->field_19 & 0x80) {
        p->field_19 = p->field_19 & 0x7F;
        return;
    }

    if (gDisplayState.control.flags.flipMode != DISPLAY_FLIP_HOLD) {
        temp = (s8)p->field_18;
        if (temp != 0) {
            product = temp * gDisplayState.frameTicks;
            temp    = p->field_17;
            temp    = temp + product;
            if (temp <= 0) {
                p->field_17         = 0;
                Stage_Ctx->field_18 = 0;
            } else {
                max = p->field_1a;
                if (temp >= (s32)max) {
                    p->field_17         = max;
                    Stage_Ctx->field_18 = 0;
                } else {
                    p->field_17 = (u8)temp;
                }
            }
        }
    }

    if (Stage_Ctx->field_17 != 0) {
        otIdx = 0;
        if (Stage_Ctx->field_19 & 2) {
            otIdx = 0x3FF;
            if (Stage_Ctx->field_13 == 0) {
                otIdx = 0x3F;
            }
        }

        tile           = gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        yoff           = gDisplayState.vramYOffset;
        setlen(tile, 3);
        setcode(tile, 0x62);
        tile->x0 = -0xA0;
        tile->y0 = -0x78 - yoff;
        tile->w  = 0x140;
        tile->h  = 0xF0;
        val      = Stage_Ctx->field_17;
        tile->b0 = val;
        tile->g0 = val;
        tile->r0 = val;

        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        if (!(Stage_Ctx->field_19 & 1)) {
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
        } else {
            setlen(dr, 1);
            dr->code[0] = 0xE1000220;
        }

        addPrim(&gGpuCurrentOt[otIdx], tile);
        addPrim(&gGpuCurrentOt[otIdx], dr);
    }
}

static s32 Display_TransitionLoad(Task* unused)
{
    RECT rect;
    s32  temp_v1;

    temp_v1 = Stage_Ctx->field_28;
    if (temp_v1 == 1) {
        goto case1;
    }
    if (temp_v1 == 0) {
        goto case0;
    }
    if (temp_v1 == 2) {
        goto case2;
    }
    if (temp_v1 == 3) {
        goto case3;
    }
    goto default_case;

case0:
    SetDispMask(0);
    Stage_Ctx->field_24        = gDisplayState.frameBuffer;
    gDisplayState.keepGraphics = 1;
    Gfx_LoadImageSlot(gGameSession->location.loc.stage, gGameSession->location.loc.area, gDisplayState.frameBuffer);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
    Stage_Ctx->field_28                  = Stage_Ctx->field_28 + 1;
    goto end;
case1:
    if (CdCmd_IsIdle() & 0xFFFF) {
        CdCmd_Enqueue(0x21, Stage_Ctx->field_2C, Stage_Ctx->field_34);
        Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
    }
    goto end;
case2:
    if ((CdCmd_IsIdle() & 0xFFFF) && (gDisplayState.frameBuffer != Stage_Ctx->field_24)) {
        Gfx_StoreImageSlot(gGameSession->location.loc.stage, gGameSession->location.loc.area, gDisplayState.frameBuffer, 0x10000);
        Mem_InitAux();
        rect.x = 0;
        rect.w = 0x140;
        rect.h = 0xF0;
        rect.y = (gDisplayState.frameBuffer ^ 1) * 0x110;
        ClearImage(&rect, 0, 0, 0);
        rect.x = 0;
        rect.w = 0x140;
        rect.h = 0xF0;
        rect.y = gDisplayState.frameBuffer * 0x110;
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        Stage_Ctx->field_12 = 1;
        Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
    }
    goto end;
case3:
    gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_TRANSITION_STRIPS;
    Stage_Ctx->field_28                     = Stage_Ctx->field_28 + 1;
default_case:
    SetDispMask(1);
    Stage_Ctx->field_1c = Stage_Ctx->field_1c & 0xF7FFFFFF;
end:
    return 1;
}

static Task* Display_SpawnFromMode(void)
{
    Task*            ret;
    u32              mode;
    Task*            slot;
    GameActor*       obj;
    GfxCoord*        ptr;
    GameLocationKey* ed;
    s32              flag;

    ret = Task_SpawnFromTable(Stage_Ctx->field_0, 0, Stage_Ctx->field_4, Stage_Ctx->field_8);
    if (ret != NULL) {
        mode = Stage_Ctx->field_C;
        if (mode == 4) {
            goto block_case4;
        }
        if (mode >= 5U) {
            goto block_default;
        }
        if (mode == 1) {
            goto block_case13;
        }
        if (mode == 3) {
            goto block_case13;
        }
        goto block_default;

    block_case4:
        Stage_Ctx->field_11 = 2;
        slot                = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        obj                 = (GameActor*)slot->work;
        flag                = obj->collisionEnableMask & 1;
        ptr                 = slot->extra.tmd->coords;
        if (flag) {
            func_801011D0(ptr, obj->collisionMotionContexts[0].contacts, 6, &obj->surfaceClass);
        }
        Gp_ClearRec18Occupied(obj->collisionContacts);
        ptr->composeStamp = GRAPHICS_COORD_DIRTY;
    block_case13:
        Stage_Ctx->field_15 = 1;
        if (Stage_Ctx->field_C == 3) {
            gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_HOLD;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
        } else {
            gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_FULL;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
        }
    } else {
        goto block_end;
    }
    goto block_end;

block_default:
    ed = &gGameSession->location.loc;
    Gpu_ResetGraphAndOt();
    Gfx_StoreImageSlot(ed->stage, ed->area, gDisplayState.drawBuffer, 0x10000);
    if (Stage_Ctx->field_C == 0x100) {
        Display_InvertFramebufferGray();
    }
    Mem_InitAux();
    gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_ROOM_SLOT;
    slot                                    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    obj                                     = (GameActor*)slot->work;
    flag                                    = obj->collisionEnableMask & 1;
    ptr                                     = slot->extra.tmd->coords;
    if (flag) {
        func_801011D0(ptr, obj->collisionMotionContexts[0].contacts, 6, &obj->surfaceClass);
    }
    Gp_ClearRec18Occupied(obj->collisionContacts);
    ptr->composeStamp = GRAPHICS_COORD_DIRTY;

block_end:
    return ret;
}

static void Display_TransitionTask(Task* task)
{
    u32          flags;
    s32          state;
    GameSession* ed;
    StageCtx*    g;
    s32          flag;
    s32          f11;
    s32          disp;

    flags = Stage_Ctx->field_1c;
    if (flags & 0x40000000) {
        Pad_SetCooldown(0);
        Stage_Ctx->field_15 = 0;
        state               = Stage_Ctx->field_28;
        switch (state) {
            case 0:
                Stage_Ctx->field_24                  = gDisplayState.frameBuffer;
                gGameSession->location.loc.view      = Stage_Ctx->field_20;
                Stage_Ctx->field_C                   = 0;
                gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
                Mem_ConfigureAuxHeap(gGameSession->location.loc.stage, gGameSession->location.loc.area);
                if (!(Stage_Ctx->field_1c & 0x10000000)) {
                    (gameGetTaskSlot(GAME_TASK_SLOT_VIEW_GATE))->spawnArg1.value = gGameSession->location.loc.view;
                    ResetGraph(1);
                    Gpu_ClearOTag(0);
                    Gpu_ClearOTag(1);
                    Mem_InitAux();
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = gGameSession->location.loc.view;
                    Pad_SetCooldown(0);
                    Gp_SpawnCurView(2);
                    gGameSession->viewReady = 0;
                    Task_Spawn(0, 0x1E, 2, 0);
                } else {
                    Tmd_AllocMissingBuffers();
                    gGameSession->viewReady = 1;
                }
                Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
                break;
            case 1:
                ed   = gGameSession;
                flag = ed->viewReady;
                if (flag == 1) {
                    disp = gDisplayState.frameBuffer;
                    g    = Stage_Ctx;
                    if (disp == g->field_24) {
                        f11           = g->field_11;
                        ed->viewReady = 0;
                        if (f11 == 0) {
                            task->killCountdown = flag;
                            Stage_Ctx->field_28 = Stage_Ctx->field_28 + 2;
                        } else {
                            Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
                        }
                    }
                    CdCmd_ActivatePhase2();
                }
                break;
            case 2:
                gDisplayState.otBuffer = gDisplayState.frameBuffer;
                Display_FlipOtAndDispatch(0);
                Stage_Ctx->field_19                  = Stage_Ctx->field_19 | 0x80;
                gDisplayState.control.flags.flipMode = gDisplayState.control.flags.flipMode | DISPLAY_FLIP_SKIP_TASK_OT;
                task->killCountdown                  = 3;
                Stage_Ctx->field_28                  = Stage_Ctx->field_28 + 1;
                break;
            case 3:
                gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
                task->killCountdown                  = task->killCountdown - 1;
                if (task->killCountdown == 0) {
                    Gpu_ResetGraphAndOt();
                    Gfx_StoreImageSlot(gGameSession->location.loc.stage, gGameSession->location.loc.area,
                                       gDisplayState.frameBuffer, 0x10000);
                    Mem_InitAux();
                    Stage_Ctx->field_12 = 0;
                    if ((s32)Stage_Ctx->field_1c < 0) {
                        Pad_ClearCooldown(0);
                        task->state = task->state + 1;
                        Display_TaskLoadStep(task);
                        return;
                    }
                    Stage_Ctx->field_28 = Stage_Ctx->field_28 + 1;
                }
                break;
            case 4:
                gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_ROOM_SLOT;
                Stage_Ctx->field_28                     = Stage_Ctx->field_28 + 1;
                break;
            case 5:
                Pad_ClearCooldown(0);
                Stage_Ctx->field_1c = Stage_Ctx->field_1c & 0xBFFFFFFF;
                break;
        }
    } else if (flags & 0x08000000) {
        Display_TransitionLoad(task);
    } else if ((s32)flags < 0) {
        task->state = task->state + 1;
        Display_TaskLoadStep(task);
    } else if (flags & 0x20000000) {
        Gfx_StoreImageSlot(gGameSession->location.loc.stage, gGameSession->location.loc.area, gDisplayState.frameBuffer,
                           0x10000);
        Stage_Ctx->field_1c = Stage_Ctx->field_1c & 0xDFFFFFFF;
    }

    if (Stage_Ctx->field_C == 4) {
        Display_FlipOtAndDispatch(0);
    }
}

static void Display_FlipOtAndDispatch(s32 unused)
{
    DisplayState* temp;
    u_long*       saved;
    s32           buf;
    u32           mode;

    temp           = &gDisplayState;
    saved          = gGpuCurrentOt;
    buf            = temp->otBuffer ^ 1;
    temp->otBuffer = buf;
    gpuBeginOt(buf);
    temp->control.flags.flipMode = DISPLAY_FLIP_FULL;
    temp->drawBuffer             = temp->frameBuffer;
    mode                         = Stage_Ctx->field_11;
    switch (mode) {
        case 3:
        case 0x20:
            Task_ExecDefaultList(&gTaskDefaultList);
            break;
        case 2:
            Gp_LinkViewSprts();
            Gp_DrawActorTmdActive(&Gpu_OtBuffers[temp->otBuffer]);
            break;
        case 1:
            Task_ExecListFiltered(&gTaskDefaultList, 0x62);
            Gp_LinkViewSprts();
            Gp_DrawActorTmdFlagged(&Gpu_OtBuffers[temp->otBuffer]);
            break;
    }
    gGpuCurrentOt = saved;
}

static void Display_InvertFramebufferGray(void)
{
    s32  i;
    u32* p0;
    u32* p1;
    u32  hi;
    u32  lo;
    u32  gray;
    u32  t;

    /* The 320x240 15-bit screen capture sits directly below the primitive
     * heap. Each pass converts four pixels (two words) at once: the red,
     * green and blue channels of all four are packed into the bytes of one
     * word, weighted 3:4:1 into a grey level, inverted, and written back as
     * grey pixels. */
    p0 = (u32*)(Gpu_PrimHeapBase - 0x25800);
    p1 = p0 + 1;
    for (i = 0; i < 0x4B00; i++) {
        hi    = *p1;
        lo    = *p0;
        t     = hi & 0x001F001F;
        t   <<= 8;
        t    |= lo & 0x001F001F;
        gray  = t * 3;
        t     = hi & 0x03E003E0;
        t   <<= 3;
        lo  >>= 5;
        t    |= lo & 0x001F001F;
        gray += t * 4;
        hi  >>= 2;
        t     = hi & 0x1F001F00;
        lo  >>= 5;
        t    |= lo & 0x001F001F;
        gray += t;
        gray  = (gray >> 3) & 0x1F1F1F1F;
        gray  = 0x1F1F1F1F - gray;

        lo   = gray & 0x001F001F;
        lo  |= (lo << 10) | (lo << 5);
        hi   = gray & 0x1F001F00;
        hi >>= 8;
        hi  |= (hi << 10) | (hi << 5);
        *p0  = lo;
        *p1  = hi;
        p1  += 2;
        p0  += 2;
    }
}

void Stage_InitOtAndSpawn(void)
{
    DisplayState* temp;

    Gpu_InitOtSmall();
    temp                         = &gDisplayState;
    temp->displayOwner           = DISPLAY_OWNER_TRANSITION;
    temp->control.flags.flipMode = DISPLAY_FLIP_HOLD;
    temp->frameBuffer            = temp->otBuffer ^ 1;
    Task_InitList(&gTaskDisplayList);
    Task_SpawnFromTable(&Display_ModeTaskDesc, 0, 0, 0);
}

s32 Stage_SetEndingFlag(void)
{
    Stage_Ctx->field_1c |= 0x80000000;
    return 0;
}

s32 Stage_BeginTransition(s32 arg0, s32 arg1)
{
    StageCtx* temp;
    s32       mask;

    mask = 0x40000000;
    if (!(Stage_Ctx->field_1c & mask)) {
        Pad_SetCooldown(0);
        temp            = Stage_Ctx;
        temp->field_20  = arg0;
        temp->field_24  = 0;
        temp->field_28  = 0;
        temp->field_11  = arg1;
        temp->field_1c |= mask;
    }
    return gGameSession->location.loc.view;
}

s32 Stage_BeginTransitionKind7(s32 arg0)
{
    StageCtx* temp;
    s32       mask;
    s32       ret;

    mask = 0x40000000;
    ret  = -1;
    if (!(Stage_Ctx->field_1c & mask)) {
        Pad_SetCooldown(0);
        temp                 = Stage_Ctx;
        temp->field_20       = arg0;
        temp->field_24       = 0;
        temp->field_28       = 0;
        temp->field_11       = 7;
        temp->field_1c      |= mask;
        ret                  = gGameSession->location.loc.view;
        Stage_Ctx->field_1c |= 0x80000000;
    }
    return ret;
}

s32 Stage_RequestImageCapture(void)
{
    Stage_Ctx->field_1c |= 0x20000000;
    return 0;
}

s32 Stage_SetFadeRate(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0) {
        Stage_Ctx->field_18 = 0x20;
    } else {
        Stage_Ctx->field_18 = arg2;
    }
    if (arg0 != 0) {
        Stage_Ctx->field_18 = -Stage_Ctx->field_18;
    }
    Stage_Ctx->field_19 = 0;
    if (arg1 != 0) {
        Stage_Ctx->field_19 |= 1;
    }
    if (arg3 != 0) {
        Stage_Ctx->field_19 |= 2;
    }
    return 0;
}

s32 Stage_GetFadeStatus(void)
{
    StageCtx* temp;
    u8        temp_a0;

    temp    = Stage_Ctx;
    temp_a0 = temp->field_17;
    if (temp_a0 == 0) {
        return 0;
    }
    if (temp_a0 >= temp->field_1a) {
        return 1;
    }
    return -1;
}

s32 Stage_HasTransitionFlags(void)
{
    return (Stage_Ctx->field_1c & 0x48000000) != 0;
}

void Stage_InitOtOnce(void)
{
    if (Stage_Ctx->field_13 == 0) {
        Gpu_InitOt();
        Stage_Ctx->field_13 = 1;
    }
}

void Stage_InitPrimBufOnce(void)
{
    if (Stage_Ctx->field_14 == 0) {
        Display_SetPrimBufLarge();
        Stage_Ctx->field_14 = 1;
    }
}

void Stage_ReleasePrimBuf(void)
{
    if (Stage_Ctx->field_14 == 1) {
        Display_SetPrimBufSmall();
        Stage_Ctx->field_14 = 0;
    }
}

void Stage_SetFadeMax(u8 arg0)
{
    Stage_Ctx->field_1a = arg0;
}

void Display_SetDrawMode(s32 arg0)
{
    switch (arg0) {
        case 0:
            gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            Display_SetAutoClear(0, 0, 0);
            return;
        case 1:
            gDisplayState.control.flags.flipMode    = (u8)arg0;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_ROOM_SLOT;
            Display_SetAutoClear(-1, 0, 0);
            return;
        case 2:
            gDisplayState.control.flags.flipMode    = DISPLAY_FLIP_TASK_ONLY;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_TRANSITION_STRIPS;
            Display_SetAutoClear(-1, 0, 0);
            return;
        case 3:
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            return;
    }
}

/// Transition kinds 3 and 7: same field_1c 0x40000000 handshake as
/// Stage_BeginTransition, with StageCtx::field_11 fixed to 3 and 7.
static s32 Stage_BeginTransitionKind3(void)
{
    StageCtx* temp;
    u32       flags;
    s32       val;

    temp  = Stage_Ctx;
    flags = temp->field_1c;
    if (!(flags & 0x40000000)) {
        temp->field_1c = flags | 0x50000000;
        val            = gGameSession->location.loc.view;
        temp->field_24 = 0;
        temp->field_28 = 0;
        temp->field_11 = 3;
        temp->field_20 = val;
    }
    return 0;
}

Task* Display_InitModeObj(TaskDesc* descriptor, s32 arg1, TaskSpawnArg arg2, s32 arg3)
{
    StageCtx* temp;

    if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
        return 0;
    }

    MEM_CLEAR(Stage_Ctx, sizeof(StageCtx));

    temp          = Stage_Ctx;
    temp->field_0 = descriptor;
    temp->field_4 = arg1;
    temp->field_8 = arg2;
    temp->field_C = arg3;
    if (arg3 == 0) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 5, 0, 0)) {
            temp->field_C = 1;
        }
    }
    Stage_Ctx->field_1a       = 0xFF;
    gDisplayState.pendingMode = DISPLAY_MODE_DESCRIPTOR;
    return 0;
}

s32 Stage_GetModeByte12(void)
{
    return Stage_Ctx->field_12;
}

static void Stage_SetModeAndFlip(u8 arg0)
{
    StageCtx* temp;

    temp = Stage_Ctx;
    if (temp->field_15 == 1) {
        temp->field_11 = arg0;
        Display_FlipOtAndDispatch(0);
    }
}

void Stage_ResetFade(void)
{
    Stage_Ctx->field_17 = 0;
    Stage_Ctx->field_1a = 0xFF;
}

static void Stage_WaitCdActivate(Task* task)
{
    Pad_SetCooldown(0);
    if (CdCmd_ActivatePhase2() != 0) {
        task->state += 1;
    } else {
        gPadStates[0].inputBlockPolls = 1;
        Display_SpawnFromMode();
        task->state += 2;
    }
}

static void Stage_WaitCdAndSpawn(Task* task)
{
    Pad_SetCooldown(0);
    if (CdCmd_IsIdleOrOverlayPending() != 0) {
        gPadStates[0].inputBlockPolls = 1;
        Display_SpawnFromMode();
        task->state += 1;
    }
}

static void Display_TaskLoadStep(Task* task)
{
    u32 temp_v1;

    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
    temp_v1                              = Stage_Ctx->field_C;
    if (temp_v1 < 5U) {
        if (temp_v1 < 3U) {
            if (temp_v1 != 1) {
                goto block_3;
            }
        }
    } else {
    block_3:
        Mem_ConfigureAuxHeap(gGameSession->location.loc.stage, gGameSession->location.loc.area);
        Tmd_AllocMissingBuffers();
        Gp_AllocSprtLists();
    }
    CdCmd_EnqueueLoadFile(0, 0, 4);
    task->state = (s32)(task->state + 1);
    Stage_WaitCdEntry(task);
}

static void Stage_WaitCdEntry(Task* task)
{
    if (CdCmd_IsIdleOrOverlayPending() != 0) {
        task->state += 1;
    }
}

static void Stage_FinishCdFollowUp(Task* task)
{
    if (CdCmd_EnqueueFollowUp() != 0) {
        gDisplayState.displayOwner              = DISPLAY_OWNER_GAME_LOOP;
        gDisplayState.pendingMode               = DISPLAY_MODE_NONE;
        gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
        Display_SetAutoClear(-1, 0, 0);
        Task_CallExit(task);
    }
}

static void Display_DispatchTaskTable(Task* task)
{
    TaskFuncTable6 sp;

    sp = Display_TaskStates;
    sp.funcs[task->state](task);
    Display_StepFadeOverlay();
}

void Mdec_ResolveStreamBuffer(u8* arg0)
{
    u16         i;
    u16         found;
    s16         bufferKind;
    s16         neg;
    s32         key;
    s32         imageDataOffset;
    CdCmdQueue* p;
    u8*         base;

    p     = &gCdCmdQueue;
    i     = 0;
    found = 0;
    key   = *arg0;
loop:
    if (key == p->sceneImageHeaders[i].viewId) {
        goto matched;
    }
    i++;
    if (i < ARRAY_SIZE(p->sceneImageHeaders)) {
        goto loop;
    }
done:
    if ((found & 0xFFFF) != 0) {
        if (p->scenePayloadLoading == 0) {
            goto success;
        }
    }
    p->imageDecodePending = 1;
    p->imageLoadStatus    = CD_COMMAND_IMAGE_PENDING;
    neg                   = CD_COMMAND_IMAGE_WAIT_HEADER;
    p->imageDecodeStep    = neg;
    return;

matched:
    found = 1;
    goto done;

success:
    Stage_CdEntry = &p->sceneImageHeaders[i];
    bufferKind    = Stage_CdEntry->bufferKind;
    switch (bufferKind) {
        case STREAM_SCENE_BUFFER_DECODE:
            base = p->decodeBuffer;
            goto store_base;
        case STREAM_SCENE_BUFFER_ACTOR_0:
            Mdec_DecodeBase = (u8*)Fs_ActorLoadBase0;
            if (p->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_0) {
                Mdec_DecodeBase = (u8*)Fs_ActorLoadBase0 + STREAM_VLC_TABLE_BYTES;
            }
            if (p->sceneStream->control.scene.timingBufferKind == STREAM_TIMING_BUFFER_ACTOR_0) {
                Mdec_DecodeBase = Mdec_DecodeBase + p->sceneStream->data.scene.timingBufferBytes;
            }
            gGameSession->field_7C = 0;
            break;
        case STREAM_SCENE_BUFFER_ACTOR_1:
            Mdec_DecodeBase = (u8*)Fs_ActorLoadBase1;
            if (p->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_1) {
                Mdec_DecodeBase = (u8*)Fs_ActorLoadBase1 + STREAM_VLC_TABLE_BYTES;
            }
            if (p->sceneStream->control.scene.timingBufferKind == STREAM_TIMING_BUFFER_ACTOR_1) {
                Mdec_DecodeBase = Mdec_DecodeBase + p->sceneStream->data.scene.timingBufferBytes;
            }
            gGameSession->field_7E = 0;
            break;
        case STREAM_SCENE_BUFFER_ACTOR_2:
            Mdec_DecodeBase = (u8*)Fs_ActorLoadBase2;
            if (p->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_2) {
                Mdec_DecodeBase = (u8*)Fs_ActorLoadBase2 + STREAM_VLC_TABLE_BYTES;
            }
            if (p->sceneStream->control.scene.timingBufferKind == STREAM_TIMING_BUFFER_ACTOR_2) {
                Mdec_DecodeBase = Mdec_DecodeBase + p->sceneStream->data.scene.timingBufferBytes;
            }
            gGameSession->field_80 = 0;
            break;
        case STREAM_SCENE_BUFFER_EXTERNAL:
            base = p->externalScenePayloadBuffer;
        store_base:
            Mdec_DecodeBase = base;
            break;
    }
    imageDataOffset       = Stage_CdEntry->imageDataOffset;
    D_8007A35C            = 0;
    p->imageDecodePending = 1;
    p->imageLoadStatus    = CD_COMMAND_IMAGE_PENDING;
    p->imageDecodeStep    = CD_COMMAND_IMAGE_START;
    D_8007A360            = Mdec_DecodeBase + imageDataOffset;
}

static __inline__ void mdecFinishDecode(void)
{
    CdCmdQueue* q = &gCdCmdQueue;

    if (gDisplayState.keepGraphics == 0) {
        Tmd_AllocMissingBuffers();
    }
    q->imageLoadStatus      = CD_COMMAND_IMAGE_COMPLETE;
    q->imageDecodePending   = 0;
    D_8007A35C              = 0;
    q->imageDecodeStep      = CD_COMMAND_IMAGE_START;
    q->scenePayloadReusable = 0;
}

/// imageDecodeStep state machine: start DCT, apply work-lists / image chunks, complete.
static void Mdec_ProcessDecode(void)
{
    enum {
        STREAM_SCENE_STRIP_X_SHIFT_PAGES = -8,
        STREAM_SCENE_CHUNK_Y_SHIFT_ROWS  = -3,
    };
    CdCmdQueue* p;
    u16         i;
    s32         r;

    p = &gCdCmdQueue;
    switch ((s16)p->imageDecodeStep) {
        case CD_COMMAND_IMAGE_WAIT_HEADER:
            Mdec_ResolveStreamBuffer(&gGameSession->location.loc.view);
            if ((u32)++D_8007A358 >= 0x5B) {
                D_8007A358 = 0;
                Gpu_ResetGraphAndOt();
                if (Stage_CdEntry->bufferKind == STREAM_SCENE_BUFFER_DECODE) {
                    p->decodeBufferBytes = p->nextDecodeBufferBytes;
                }
                mdecFinishDecode();
            }
            break;
        case CD_COMMAND_IMAGE_START:
            Gpu_ResetGraphAndOt();
            p->mdecOutputPending = 1;
            if (p->sceneVlcTableMode == STREAM_SCENE_VLC_IMAGE_BUFFER) {
                DecDCTvlcBuild((u16*)((u8*)Fs_ImgBuffers + 0x8800));
                p->rebuildImageVlcTable = 0;
                p->vlcTable             = (u16*)((u8*)Fs_ImgBuffers + 0x8800);
            }
            DecDCTReset(0);
            DecDCTvlcSize2(0);
            DecDCTvlc2((u_long*)D_8007A360, gMemActiveAuxHeap,
                       p->vlcTable);
            D_8007A35E = 1;
            DecDCToutCallback(Mdec_StripCallback);
            DecDCTin(gMemActiveAuxHeap, p->imageMdecMode);
            p->imageMdecMode = MDEC_IMAGE_MODE_RGB16;
            DecDCTout((u_long*)Fs_ImgBuffers, 0x780);
            D_8007A358 = 0;
            p->imageDecodeStep++;
            /* fallthrough */
        case CD_COMMAND_IMAGE_WAIT_OUTPUT:
            if (p->mdecOutputPending == 0) {
                // Upload the payload's optional strip lists and image chunks.
                for (i = 0; i < ARRAY_SIZE(Stage_CdEntry->stripListOffsets); i++) {
                    if (Stage_CdEntry->stripListOffsets[i] != 0) {
                        if (Stage_CdEntry->relocateStripLists[i] != 0) {
                            Fs_ChunkMode    = 2;
                            D5B498_8006C233 = STREAM_SCENE_STRIP_X_SHIFT_PAGES;
                        }
                        Fs_CopyWorkEntries((FsWorkEntry*)(Mdec_DecodeBase + Stage_CdEntry->stripListOffsets[i]));
                        while (Fs_LoadImageStrip(1) != 1) {
                            r = Fs_LoadImageStrip(1);
                            if (r == 1) {
                                break;
                            }
                            if (r == 0x7F) {
                                Fs_CopyWorkEntries((FsWorkEntry*)(Mdec_DecodeBase + Stage_CdEntry->stripListOffsets[i]));
                            }
                        }
                        Fs_ChunkMode    = 0;
                        D5B498_8006C233 = 0;
                    }
                }
                for (i = 0; i < ARRAY_SIZE(Stage_CdEntry->imageChunkOffsets); i++) {
                    if (Stage_CdEntry->imageChunkOffsets[i] != 0) {
                        if (Stage_CdEntry->relocateImageChunks[i] != 0) {
                            Fs_ChunkMode    = 2;
                            D5B498_8006C234 = STREAM_SCENE_CHUNK_Y_SHIFT_ROWS;
                        }
                        while (Fs_LoadImageChunk((FsImageChunk*)(Mdec_DecodeBase + Stage_CdEntry->imageChunkOffsets[i]), 1)) {
                        }
                        Fs_ChunkMode    = 0;
                        D5B498_8006C234 = 0;
                    }
                }
                p->imageLayout = FILE_SYSTEM_IMAGE_STRIPS;
                if (Stage_CdEntry->bufferKind == STREAM_SCENE_BUFFER_DECODE) {
                    p->decodeBufferBytes = p->nextDecodeBufferBytes;
                }
                // Refresh timing data before releasing the completed decode operation.
                if (p->sceneStream->control.scene.timingBufferKind != STREAM_TIMING_BUFFER_NONE) {
                    Mem_CopyUnaligned(&Mdec_DecodeBase[Stage_CdEntry->timingDataOffset], p->timingBuffer,
                                      Stage_CdEntry->timingDataBytes);
                }
                mdecFinishDecode();
            } else if ((u32)++D_8007A358 >= 0x5B) {
                D_8007A358 = 0;
                Gpu_ResetGraphAndOt();
                if (Stage_CdEntry->bufferKind == STREAM_SCENE_BUFFER_DECODE) {
                    p->decodeBufferBytes = p->nextDecodeBufferBytes;
                }
                mdecFinishDecode();
            }
            break;
    }
}

static void Mdec_DecodeToVram(void)
{
    RECT          rect;
    s32           i;
    s32           temp;
    CdCmdQueue*   p;
    CdCmdQueue*   q;
    DisplayState* d;

    p = &gCdCmdQueue;
    switch ((s16)p->imageDecodeStep) {
        case CD_COMMAND_IMAGE_START:
            Gpu_ResetGraphAndOt();
            p->mdecOutputPending = 1;
            DecDCTReset(0);
            DecDCTvlcSize2(0);
            DecDCTvlc2((u_long*)D_8007A360, gMemActiveAuxHeap,
                       (u_short*)((u8*)Fs_ImgBuffers + 0x8800));
            D_8007A35E = 1;
            DecDCToutCallback(Mdec_StripCallback);
            DecDCTin(gMemActiveAuxHeap, p->imageMdecMode);
            p->imageMdecMode = MDEC_IMAGE_MODE_RGB16;
            DecDCTout((u_long*)Fs_ImgBuffers, 0x780);
            p->imageDecodeStep += 1;
            /* fallthrough */
        case CD_COMMAND_IMAGE_WAIT_OUTPUT:
            i = 0;
            if (p->mdecOutputPending == 0) {
                rect.w = 0x10;
                rect.h = 0xF0;
                rect.y = (gDisplayState.drawBuffer ^ 1) * 0x110;
                do {
                    temp   = i & 0xFFFF;
                    rect.x = temp * 0x10;
                    LoadImage(&rect, &Fs_ImgBuffers->words[temp * 1920]);
                    i++;
                } while ((u32)(i & 0xFFFF) < 0x14U);
                rect.w = 0x140;
                rect.x = 0;
                rect.h = 0xF0;
                d      = &gDisplayState;
                rect.y = (d->drawBuffer ^ 1) * 0x110;
                StoreImage(&rect, (u_long*)Fs_ImgBuffers);
                if (p->preserveDisplayAfterDecode != 0) {
                    rect.x = 0;
                    rect.w = 0x1E0;
                    rect.h = 0xF0;
                    rect.y = d->drawBuffer * 0x110;
                    MoveImage(&rect, 0, (d->drawBuffer ^ 1) * 0x110);
                    p->preserveDisplayAfterDecode = 0;
                } else {
                    ClearImage(&rect, 0, 0, 0);
                }
                p->imageLayout = FILE_SYSTEM_IMAGE_CONTIGUOUS;
                q              = &gCdCmdQueue;
                if (gDisplayState.keepGraphics == 0) {
                    Tmd_AllocMissingBuffers();
                }
                q->imageLoadStatus      = CD_COMMAND_IMAGE_COMPLETE;
                q->imageDecodePending   = 0;
                D_8007A35C              = 0;
                q->imageDecodeStep      = CD_COMMAND_IMAGE_START;
                q->scenePayloadReusable = 0;
            }
            return;
    }
}

void CdCmd_StepVlcRebuild(void)
{
    CdCmdQueue* p;

    p = &gCdCmdQueue;
    if (p->scenePayloadAvailable == 0) {
        if (p->rebuildImageVlcTable != 0) {
            DecDCTvlcBuild((u16*)((u8*)Fs_ImgBuffers + 0x8800));
            p->rebuildImageVlcTable = 0;
        }
        if ((p->imageDecodePending != 0) && (p->rebuildImageVlcTable == 0)) {
            Mdec_DecodeToVram();
        }
    } else if (p->imageDecodePending != 0) {
        Mdec_ProcessDecode();
    }
}

void Mdec_BeginDecode(void* arg0)
{
    CdCmdQueue* p;

    D_8007A35C            = 0;
    p                     = &gCdCmdQueue;
    p->imageDecodePending = 1;
    p->imageLoadStatus    = CD_COMMAND_IMAGE_PENDING;
    p->imageDecodeStep    = CD_COMMAND_IMAGE_START;
    D_8007A360            = arg0;
    D_8007A358            = 0;
}

void CdCmd_RequestVlcRebuild(void)
{
    gCdCmdQueue.rebuildImageVlcTable = 1;
}

static void Mdec_StripCallback(void)
{
    s32         temp;
    CdCmdQueue* p;

    temp = 0x140 / (D_8007A35E * 16);
    p    = &gCdCmdQueue;
    if (D_8007A35C == temp - 1) {
        p->mdecOutputPending = 0;
        DecDCToutCallback(0);
    } else {
        D_8007A35C = D_8007A35C + 1;
        DecDCTout(&Fs_ImgBuffers->words[D_8007A35C * 1920], 0x780);
    }
}

void Stage_TaskExit(Task* task)
{
    Task_CallExit(task);
}
