#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/acropolis_security_room.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix with no translation
/// vector added. Same reason as above for spelling out the word.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
/// `mvmva 1, 0, 0, 0, 0`: rotate V0 by the rotation matrix and add the
/// translation vector. Same reason as above for spelling out the word.
#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")
/// `gpf 1`: scale IR1..3 by IR0. Same reason as above for spelling out the word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
void func_acropolis_security_room_8017FD64(s32 flags);
void func_acropolis_security_room_8017F480(Task* task);
void func_acropolis_security_room_80180308(Task* task);
/// The two `TaskDesc`s this room's script spawns from: index 0 is
/// `func_acropolis_security_room_80180368`, index 1 is
/// `func_acropolis_security_room_801804CC`.
extern TaskDesc D_acropolis_security_room_80182700[];
/// The single-entry `TaskDesc` table the script spawns its child task from:
/// `func_acropolis_security_room_8017F9C8`.
extern TaskDesc D_acropolis_security_room_801826C0[];
/// The script's message table, parked in `Task::field_24`.
extern GpMsgEntry D_acropolis_security_room_801826CC[];
extern s8         D_8007216C;
extern s16        D_80114D08;
/// 0xFF-terminated area-record lists applied as the script ends.
extern GpAreaApplyRec D_acropolis_security_room_80184F50[];
extern GpAreaApplyRec D_acropolis_security_room_80184F78[];
extern GpAreaApplyRec D_acropolis_security_room_80184F7C[];
extern GpAreaApplyRec D_acropolis_security_room_80184F80[];
/// Scratch state of the security-room ambience task, stored at `Task::idMap`.
/// `func_acropolis_security_room_80180368` allocates it with `Mem_Calloc(4, 0)`,
/// so the size below is the allocation and not a guess.
typedef struct {
    /* 0x0 */ u16  fadeStarted; // the looping ambience has already been faded out
    /* 0x2 */ byte pad_2[0x2];
} AsrAmbienceState;
/// The 0x100-entry RGB555 palette every monitor-screen CLUT is blended
/// towards: the "off" colours of the four security-camera feeds.
extern u16 D_acropolis_security_room_80182718[];
/// The four lit palettes, one per camera feed, blended against
/// `D_acropolis_security_room_80182718` by the feed's own brightness.
extern u16 D_acropolis_security_room_80182918[];
extern u16 D_acropolis_security_room_80182B18[];
extern u16 D_acropolis_security_room_80182D18[];
extern u16 D_acropolis_security_room_80182F18[];
/// The four blend results, uploaded to VRAM by
/// `D_acropolis_security_room_80183918`.
extern u16 D_acropolis_security_room_80183118[];
extern u16 D_acropolis_security_room_80183318[];
extern u16 D_acropolis_security_room_80183518[];
extern u16 D_acropolis_security_room_80183718[];
/// The upload records for the four blended CLUTs above.
extern GpImgRec D_acropolis_security_room_80183918[];
/// Camera-lit bitmask for each value of `GameFlag_GetNibble(9)`; bit N is set
/// while feed N is showing something.
extern u16 D_acropolis_security_room_80183968[];
/// Spawn position and effect id of the flash each newly lit feed plays,
/// indexed by feed.
extern SVECTOR D_acropolis_security_room_80183998[];
extern s16     D_acropolis_security_room_801839B8[];

/// Resets both action-prompt slots before the script's first cursor scan and
/// steps the caller on one state: clears each slot's leading words and its two
/// trailing shorts, parks the target id at 0x100 with `field_E` at 0xF, and
/// marks the slot as highlighted (`mode` 1). Same body as the shared
/// `Room_Util04`, which this overlay cannot link because it carries a second
/// copy at `func_acropolis_security_room_8017EDE4`.
void func_acropolis_security_room_80180308(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0               = 0;
        prompt->field_4               = 0;
        prompt->targetId              = 0x100;
        prompt->field_E               = 0xF;
        prompt->buttons[0].heldFrames = 0;
        prompt->buttons[1].heldFrames = 0;
        prompt->mode                  = 1;
    }
    task->state = task->state + 1;
}

/// First `TaskDesc` of `D_acropolis_security_room_80182700`: starts the room's
/// looping ambience, then rides alongside the cutscene task
/// (`func_acropolis_security_room_801804CC`) until the CD queue reaches its cue
/// or the player skips, fading the loop out exactly once either way, and asks
/// the task system to kill itself.
void func_acropolis_security_room_80180368(Task* task)
{
    CdCmdQueue*       queue;
    s32               state;
    AsrAmbienceState* st;
    AsrAmbienceState* alloc;

    queue = &CdCmd_Queue;
    state = task->state;
    st    = (AsrAmbienceState*)task->idMap;

    switch (state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
    }
    return;

L_case0:
    alloc       = (AsrAmbienceState*)Mem_Calloc(sizeof(AsrAmbienceState), 0);
    task->idMap = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        Task_Kill(task);
        return;
    }
    Mem_Set(alloc, 0, sizeof(AsrAmbienceState));
    SndEvt_EnqueueType6(0x51060008, 0, 0);
    goto advance;

L_case1:
    if (queue->field_1EA >= 0x46 && st->fadeStarted == 0) {
        SndEvt_EnqueueType7(0x51060008, 0x14);
        st->fadeStarted = state;
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        task->state = task->state + 1;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    if (st->fadeStarted == 0) {
        SndEvt_EnqueueType7(0x51060008, 0x14);
    }
advance:
    task->state = task->state + 1;
    return;

L_case2:
    Task_RequestKill(task, 0);
}

/// Second `TaskDesc` of `D_acropolis_security_room_80182700`: kicks off the
/// streamed cutscene for the security room, waits for the CD queue to go idle
/// (or for the player to skip it), then asks the task system to kill itself.
void func_acropolis_security_room_801804CC(Task* arg0)
{
    u8          slotParam[4];
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
    }
    return;

L_case0:
    queue->field_1EA = 1;
    slotParam[0]     = Stream_FindSlot(&Game_Session->field_4, 0, 0);
    CdCmd_Enqueue(0x61, 0, slotParam);
    goto advance;

L_case1:
    if (CdCmd_IsIdle() & 0xFFFF) {
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
advance:
    task->state = task->state + 1;
    return;

L_case2:
    Task_RequestKill(task, 0);
}

/// Per-frame update of the security-room's four monitor feeds: state 0 seeds
/// the four screen CLUTs from the unlit palette, state 1 re-blends each of
/// them towards its lit palette by that feed's brightness and spawns the
/// flash effects. `Task::spawnArg2` is the `GpEffWork` holding the lit-feed
/// bitmask (`field_20`) and the four per-feed brightnesses
/// (`field_24` .. `field_2A`).
void func_acropolis_security_room_801805A4(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    s32            i;

    work  = (GpEffWork*)task->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;

    switch (task->state) {
        case 0: {
            u16* base = D_acropolis_security_room_80182718;
            u16* pal  = D_acropolis_security_room_80182918;
            u16* out  = D_acropolis_security_room_80183118;

            for (i = 0; i < 0x100; i += 0x10) {
                Gp_BlendRgb555Clut(&pal[i], &base[i], 0, &out[i]);
            }
            pal = D_acropolis_security_room_80182B18;
            out = D_acropolis_security_room_80183318;
            for (i = 0; i < 0x100; i += 0x10) {
                Gp_BlendRgb555Clut(&pal[i], &base[i], 0, &out[i]);
            }
            pal = D_acropolis_security_room_80182D18;
            out = D_acropolis_security_room_80183518;
            for (i = 0; i < 0x100; i += 0x10) {
                Gp_BlendRgb555Clut(&pal[i], &base[i], 0, &out[i]);
            }
            pal = D_acropolis_security_room_80182F18;
            out = D_acropolis_security_room_80183718;
            for (i = 0; i < 0x100; i += 0x10) {
                Gp_BlendRgb555Clut(&pal[i], &base[i], 0, &out[i]);
            }
            Gp_LoadImages(D_acropolis_security_room_80183918);
            task->state = task->state + 1;
            break;
        }

        case 1:
            work->field_20 = D_acropolis_security_room_80183968[GameFlag_GetNibble(9)];
            if ((Gp_GetViewIndex() & 0xFF) == 6) {
                u16* pal  = D_acropolis_security_room_80182918;
                u16* base = D_acropolis_security_room_80182718;
                u16* out  = D_acropolis_security_room_80183118;
                s32  limit;

                // The cap flickers by one step every other frame.
                limit          = 0x1000 - ((Display_State.field_8 & 1) << 9);
                work->field_24 = (work->field_20 & 1) ? ((work->field_24 < limit) ? work->field_24 + 0x200 : limit) : 0;
                work->field_26 = (work->field_20 & 2) ? ((work->field_26 < limit) ? work->field_26 + 0x200 : limit) : 0;
                work->field_28 = (work->field_20 & 4) ? ((work->field_28 < limit) ? work->field_28 + 0x200 : limit) : 0;
                work->field_2A = (work->field_20 & 8) ? ((work->field_2A < limit) ? work->field_2A + 0x200 : limit) : 0;

                for (i = 0; i < 0x100; i += 0x10) {
                    Gp_BlendRgb555Clut(&pal[i], &base[i], work->field_24, &out[i]);
                }
                pal = D_acropolis_security_room_80182B18;
                out = D_acropolis_security_room_80183318;
                for (i = 0; i < 0x100; i += 0x10) {
                    Gp_BlendRgb555Clut(&pal[i], &base[i], work->field_26, &out[i]);
                }
                pal = D_acropolis_security_room_80182D18;
                out = D_acropolis_security_room_80183518;
                for (i = 0; i < 0x100; i += 0x10) {
                    Gp_BlendRgb555Clut(&pal[i], &base[i], work->field_28, &out[i]);
                }
                pal = D_acropolis_security_room_80182F18;
                out = D_acropolis_security_room_80183718;
                for (i = 0; i < 0x100; i += 0x10) {
                    Gp_BlendRgb555Clut(&pal[i], &base[i], work->field_2A, &out[i]);
                }
                Gp_LoadImages(D_acropolis_security_room_80183918);

                for (i = 0; i < 4; i++) {
                    Gp_SpawnEff(0x60049, coord, i, NULL);
                }
            } else if (((Gp_GetViewIndex() & 0xFF) != 8) && ((Gp_GetViewIndex() & 0xFF) != 0x10)) {
                for (i = 0; i < 4; i++) {
                    if ((work->field_20 >> i) & 1) {
                        Gp_SpawnEff(0x600A0, coord, D_acropolis_security_room_801839B8[i],
                                    &D_acropolis_security_room_80183998[i]);
                    }
                }
            }
            break;
    }

    if (GameFlag_GetNibble(1) < 3) {
        func_acropolis_security_room_80180A78(task);
    }
}

/// Draws the security room's sweeping laser beam: two points in the emitter's
/// local frame are rotated into world space by the emitter coordinate's
/// `workm`, projected through `GsWSMATRIX`, and linked into the current OT as
/// one semi-transparent flat `LINE_F2`. The beam only exists in the two camera
/// views selected by the `0xC` bitmask over `GameSession::field_4`, its far
/// endpoint sweeps with the frame counter (`Display_State.field_8 * 6` folded
/// into a 406-step range), and nothing is queued when the near endpoint
/// projects closer than an OTZ of 0x11.
void func_acropolis_security_room_80180A78(Task* task)
{
    void**          scratch;
    u8*             head;
    AsrBeamScratch* blk;
    GsCOORDINATE2*  coord;
    LINE_F2*        prim;

    coord = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    if ((0xC >> ((u8)Game_Session->field_4 - 1)) & 1) {
        scratch   = (void**)G_SCRATCH_HEAD;
        head      = *scratch;
        blk       = (AsrBeamScratch*)(head - 0x14);
        blk->a.vx = -0x427;
        blk->a.vy = ((u32)Display_State.field_8 * 6) % 406 + 0xF633;
        *scratch  = blk;
        blk->a.vz = 0x9AF;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((AsrBeamScratch*)(head - 0x14))->a);
        gte_rtv0_real();
        gte_stsv(&((AsrBeamScratch*)(head - 0x14))->a);
        blk->a.vx = *(u16*)&blk->a.vx + *(u16*)&coord->workm.t[0];
        blk->a.vy = *(u16*)&blk->a.vy + *(u16*)&coord->workm.t[1];
        blk->a.vz = *(u16*)&blk->a.vz + *(u16*)&coord->workm.t[2];
        blk->b.vx = -0x1F0;
        blk->b.vy = ((u32)Display_State.field_8 * 6) % 406 + 0xF633;
        blk->b.vz = 0x9AF;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((AsrBeamScratch*)(head - 0x14))->b);
        gte_rtv0_real();
        gte_stsv(&((AsrBeamScratch*)(head - 0x14))->b);
        blk->b.vx = *(u16*)&blk->b.vx + *(u16*)&coord->workm.t[0];
        blk->b.vy = *(u16*)&blk->b.vy + *(u16*)&coord->workm.t[1];
        blk->b.vz = *(u16*)&blk->b.vz + *(u16*)&coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&((AsrBeamScratch*)(head - 0x14))->a);
        gte_rtps_real();
        prim           = (LINE_F2*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setLineF2(prim);
        gte_stsxy(&prim->x0);
        gte_ldv0(&((AsrBeamScratch*)(head - 0x14))->b);
        gte_rtps_real();
        prim->code |= 2;
        gte_stsxy(&prim->x1);
        gte_stszotz(&blk->otz);
        if (((AsrBeamScratch*)(head - 0x14))->otz > 0x10) {
            setRGB0(prim, 0x10, 0x10, 0x10);
            addPrim((u_long*)(((((u32)((AsrBeamScratch*)(head - 0x14))->otz << Display_State.field_128) >> 2) &
                               0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 2, ((AsrBeamScratch*)(head - 0x14))->otz);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    }
}

/// Per-frame draw for the security-room's flash sprite: refreshes the task's
/// coordinate frame, loads it into the GTE, then queues one 128x128 textured
/// quad from `D_acropolis_security_room_80183970` -- picked by the low two bits
/// of `Task::spawnArg1` -- into the current OT before releasing the effect's
/// `Gp_State1C` work block.
void func_acropolis_security_room_80180E34(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    POLY_FT4*      prim;
    s16            x;
    s16            y;
    u16            cx;
    u16            cy;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    mem   = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);

    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setPolyFT4(prim);
    mem->field_24 = *(u16*)&arg0->spawnArg1 & 3;
    prim->tpage   = 0xAB;
    prim->code   |= 3;
    prim->clut    = D_acropolis_security_room_80183970[mem->field_24].clut << 6;
    cx            = D_acropolis_security_room_80183970[mem->field_24].x;
    cy            = D_acropolis_security_room_80183970[mem->field_24].y;
    prim->u0      = D_acropolis_security_room_80183970[mem->field_24].u;
    prim->v0      = D_acropolis_security_room_80183970[mem->field_24].v;
    prim->u1      = D_acropolis_security_room_80183970[mem->field_24].u + 0x7F;
    prim->v1      = D_acropolis_security_room_80183970[mem->field_24].v;
    prim->u2      = D_acropolis_security_room_80183970[mem->field_24].u;
    prim->v2      = D_acropolis_security_room_80183970[mem->field_24].v + 0x7F;
    prim->u3      = D_acropolis_security_room_80183970[mem->field_24].u + 0x7F;
    prim->v3      = D_acropolis_security_room_80183970[mem->field_24].v + 0x7F;
    x             = cx - 0x40;
    prim->x2      = x;
    prim->x0      = x;
    x             = cx + 0x3F;
    prim->x3      = x;
    prim->x1      = x;
    y             = cy - 0x40;
    prim->y1      = y;
    prim->y0      = y;
    y             = cy + 0x3F;
    prim->y3      = y;
    prim->y2      = y;
    addPrim((u_long*)(((((u32)0x30 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), prim);
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_security_room/acropolis_security_room_6", func_acropolis_security_room_80181108);

void func_acropolis_security_room_801817A4(Task* taskArg)
{
    Task* task;
    void* mem;

    POLY_G4*         quad;
    LINE_G3*         line;
    s32              nextIndex;
    s32              greenBit;
    s32              quadLum;
    s32              lineLum;
    u32              tagMask;
    u32              addrMask;
    s32              tpageMode;
    s32              savedLum;
    s32              rawLum;
    u16              vz;
    s32              direction;
    s32              i;
    s32              redBit;
    s32              product;
    s32              greenProduct;
    u8               lineRed;
    u8               lineGreen;
    s32              redProduct;
    u16              redHalf;
    u16              x;
    u16              y;
    u32              rng;
    u8*              head;
    GsCOORDINATE2*   coord;
    AsrFlashScratch* scratch;

    task  = taskArg;
    coord = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    mem   = task->spawnArg2;
    Gp_UpdateCoord(coord);
    head = *(u8**)G_SCRATCH_HEAD;
    {
        u8* tmp;
        tmp = head - 0x14;
        SOFT_TOUCH_REG(tmp);
        scratch = (AsrFlashScratch*)tmp;
    }
    scratch->v.vx           = *(u16*)&coord->workm.t[0];
    scratch->v.vy           = *(u16*)&coord->workm.t[1];
    vz                      = *(u16*)&coord->workm.t[2];
    *(void**)G_SCRATCH_HEAD = scratch;
    scratch->v.vz           = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AsrFlashScratch*)(head - 0x14))->v);
    gte_rtps_real();
    gte_stsxy(&((AsrFlashScratch*)(head - 0x14))->x);
    gte_stszotz(&scratch->otz);
    if (((AsrFlashScratch*)(head - 0x14))->otz >= 0x11) {
        redBit  = ((s32)task->spawnArg1 >> 1) & 1;
        redHalf = redBit;

        rng      = (Gp_LcgState * 5) + 0x71357911;
        greenBit = (u16)task->spawnArg1 & 1;
        rawLum   = ((rng >> 0x10) & 0x70) + 0x40;
        __asm__("move %0,%1" : "=r"(quadLum) : "r"(rawLum));
        redProduct = quadLum * redBit;
        i          = 0;
        addrMask   = 0xFFFFFF;
        SOFT_TOUCH_REG(addrMask);
        Gp_LcgState = rng;
        __asm__("move %0,%1" : "=r"(savedLum) : "r"(rawLum));
        scratch->step = 0xC00 / ((AsrFlashScratch*)(head - 0x14))->otz;
        do {
            quad           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(quad + 1);
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, redProduct, greenBit * quadLum, 0);
            setRGB3(quad, 0, 0, 0);
            quad->x0  = scratch->x - *(u16*)&scratch->step;
            x         = scratch->x;
            quad->x2  = x;
            quad->x1  = x;
            quad->x3  = scratch->x + *(u16*)&scratch->step;
            y         = scratch->y;
            quad->y3  = y;
            quad->y2  = y;
            quad->y0  = y;
            quad->y1  = (scratch->y - *(u16*)&scratch->step) + (scratch->step * (i << 1));
            tagMask   = 0xFF000000;
            quad->tag = (quad->tag & tagMask) | (*((u_long*)(((((u32)scratch->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt)) & addrMask);
            TOUCH_REG(tagMask);
            __asm__("addiu %0,$0,1" : "=r"(tpageMode) : "r"(i));
            *((u_long*)(((((u32)scratch->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt)) = (*((u_long*)(((((u32)scratch->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt)) & tagMask) | ((u32)quad & addrMask);
            i                                                                                                 += tpageMode;
            Gp_AddTpageShift((P_TAG*)quad, tpageMode, scratch->otz);
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
        } while (i < 2);
        i       = 0;
        lineLum = savedLum;
        do {
            line           = (LINE_G3*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            product   = redHalf * lineLum;
            lineRed   = product;
            product   = (greenProduct = greenBit * lineLum);
            lineGreen = product;
            setRGB1(line, lineRed, lineGreen, 0);
            setRGB2(line, 0, 0, 0);
            direction = (i << 1) - 1;
            product   = scratch->step * direction;
            line->x0  = scratch->x + product;
            nextIndex = i + 1;
            line->y0  = scratch->y - (scratch->step * nextIndex);
            line->x1  = scratch->x;
            line->y1  = scratch->y;
            line->x2  = scratch->x - (scratch->step * direction);
            line->y2  = scratch->y + (scratch->step * nextIndex);
            addPrim((u_long*)(((((u32)scratch->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), line);
            Gp_AddTpageShift((P_TAG*)line, 1, scratch->otz);
            i = nextIndex;
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
        } while (i < 2);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    Gp_ReleaseState1CMem(mem, task);
}
