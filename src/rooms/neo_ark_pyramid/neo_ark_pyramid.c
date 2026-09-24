#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

/// `Gp_StateF0.field_4` (0x801153F4), the byte the room's cutscene driver sets on
/// enter and clears on exit. Declared as a one-element array so the store keeps the
/// in-struct memory attribute a real struct-member store would have: that is what makes
/// the store alias the `task->state` load for the scheduler, which leaves the state
/// load where the source put it and lets jump.c cross-jump case 0's `task->state++`
/// into the shared tail. A plain `u8` declaration assembles to the same bytes but is
/// scheduled one statement early.
extern u8 D_801153F4[1];
extern u8 D_80115690;
extern u8 D_8007216C;

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;
extern s32 Gp_LcgState;

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// Angle of the room's rotating quad, set by `func_neo_ark_pyramid_8017DAC0`.
extern s32      D_neo_ark_pyramid_801818A4;
extern TaskDesc D_neo_ark_pyramid_8017FC0C;

/// The room's message table: handlers for messages 0x13EE, 0x13F1, 0x13EF
/// and 0x13F0, closed by a 0x7FFFFFFF entry.
extern GpMsgEntry D_neo_ark_pyramid_8017FBE4[];

/// The two points on the spawner's parent coordinate that the ribbon task
/// trails: the first entry places the effect, the second (reached here both
/// as `[1]` and under its own label) is the ribbon's other edge.
extern SVECTOR D_neo_ark_pyramid_8017FC18[];
extern SVECTOR D_neo_ark_pyramid_8017FC20;

void func_neo_ark_pyramid_8017DAC0(s32 arg0);
void func_neo_ark_pyramid_8017DB18(Task* task);
void func_neo_ark_pyramid_8017DB5C(Task* task);
void func_neo_ark_pyramid_8017DEF4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_neo_ark_pyramid_8017E320(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_neo_ark_pyramid_8017EBA4(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
void func_neo_ark_pyramid_8017F224(GsCOORDINATE2* arg0, s16 arg1, u8* rgb);

/// State handlers of the room's entry task, indexed by its state through
/// `func_neo_ark_pyramid_8017DB98`: set-up, per-frame draw, then kill.
const TaskFuncTable3 D_neo_ark_pyramid_8017D5C4 = {
    { func_neo_ark_pyramid_8017DB18, func_neo_ark_pyramid_8017DB5C, taskKill }
};

/// Event task that turns the room's rotating quad one step. It hides the HUD
/// and runs capture command 1; unless that ends on event key 0xC it plays a
/// sound and sweeps the quad's angle over 0x156 in steps of 4, then bumps
/// game-flag nibble 0xEC. Below four turns it returns to the capture command;
/// on the fourth it plays the closing sound and capture command 2. Either exit
/// restores the HUD and the player's weapon before the task kills itself.
void func_neo_ark_pyramid_8017D600(Task* task)
{
    u16 count;

    switch (task->state) {
        case 0:
            D_8007216C               = 8;
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            D_801153F4[0]            = 2;
            task->state++;
            break;
        case 1:
            Gp_RunCapCmd(1, 0);
            task->state++;
            break;
        case 2:
            D_80115690 = 1;
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 3:
            if (Gp_GetCapEventKey() == 0xC) {
                task->state = 0xA;
                break;
            }
            SndEvt_EnqueueType6(0x55200003, 0, 0);
            task->killCountdown = 0;
            task->state++;
            break;
        case 4:
            count               = task->killCountdown + 4;
            task->killCountdown = count;
            if ((s16)count >= 0x156) {
                GameFlag_SetNibble(0xEC, GameFlag_GetNibble(0xEC) + 1);
                func_neo_ark_pyramid_8017DAC0(0);
                if (GameFlag_GetNibble(0xEC) >= 4) {
                    SndEvt_EnqueueType6(0x55200005, 0, 0);
                    Gp_RunCapCmd(2, 0);
                    task->state++;
                } else {
                    SndEvt_EnqueueType6(0x55200004, 0, 0);
                    task->state = 1;
                }
            } else {
                func_neo_ark_pyramid_8017DAC0((s16)count);
            }
            break;
        case 5:
            D_80115690 = 1;
            if (Gp_CapBusy() == 0) {
                task->state = 0xA;
            }
            break;
        case 10:
            D_8007216C               = 3;
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            D_801153F4[0]            = 0;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            taskKill(task);
            break;
    }
}

/// Queues the room's rotating quad: a 0xAE-pixel textured `POLY_FT4` centred
/// on the screen origin, rotated by `arg0` (0x1000 a full turn).
void func_neo_ark_pyramid_8017D7F4(s32 arg0)
{
    POLY_FT4* prim;
    s16       src[4][2];
    s16       dst[4][2];
    s32       i;

    src[0][0] = -0x57;
    src[0][1] = -0x57;
    src[1][0] = 0x57;
    src[1][1] = -0x57;
    src[2][0] = -0x57;
    src[2][1] = 0x57;
    src[3][0] = 0x57;
    src[3][1] = 0x57;
    for (i = 0; i < 4; i++) {
        dst[i][0] = (src[i][0] * rcos(arg0) - src[i][1] * rsin(arg0)) >> 12;
        dst[i][1] = (src[i][0] * rsin(arg0) + src[i][1] * rcos(arg0)) >> 12;
    }
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2D);
    prim->x0    = dst[0][0];
    prim->y0    = dst[0][1];
    prim->x1    = dst[1][0];
    prim->y1    = dst[1][1];
    prim->x2    = dst[2][0];
    prim->y2    = dst[2][1];
    prim->x3    = dst[3][0];
    prim->y3    = dst[3][1];
    prim->u0    = 1;
    prim->v0    = 1;
    prim->u1    = 0xAF;
    prim->v1    = 1;
    prim->u2    = 1;
    prim->v2    = 0xAF;
    prim->u3    = 0xAF;
    prim->v3    = 0xAF;
    prim->clut  = 0x3FC0;
    prim->tpage = 0x8E;
    addPrim(gGpuCurrentOt + 0xC, prim);
}

/// Handler for message 0x13F1 in the room's message table; does nothing.
s32 func_neo_ark_pyramid_8017D9F0(void)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table: copies the
/// incoming save-location record onto the outgoing one and forwards both to
/// `func_80179B14`. Always answers 1.
s32 func_neo_ark_pyramid_8017D9F8(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}

/// Handler for message 0x13F0 in the room's message table; does nothing.
s32 func_neo_ark_pyramid_8017DA3C(void)
{
    return 0;
}

/// Handler for message 0x13EF in the room's message table. When the message's
/// `field_2` is 1 it resets the quad's angle; once the quad has turned four
/// times it spawns capture event 3, otherwise it has the player lower the
/// weapon and starts the task that turns the quad another step.
s32 func_neo_ark_pyramid_8017DA44(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 1) {
        func_neo_ark_pyramid_8017DAC0(0);
        if (GameFlag_GetNibble(0xEC) == 4) {
            Gp_SpawnIfCapIdle(3, 1);
        } else {
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            Task_SpawnFromTable(&D_neo_ark_pyramid_8017FC0C, 0, 0, 0);
        }
    }
    return 0;
}

/// Sets the quad's angle to twelfths of a turn counted by game-flag nibble
/// 0xEC, less four, plus the in-progress sweep `arg0`.
void func_neo_ark_pyramid_8017DAC0(s32 arg0)
{
    D_neo_ark_pyramid_801818A4 = (((GameFlag_GetNibble(0xEC) - 4) << 0xC) / 12) + arg0;
}

/// State 0 of the room's entry task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7 and advances.
void func_neo_ark_pyramid_8017DB18(Task* task)
{
    task->msgTable = D_neo_ark_pyramid_8017FBE4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's entry task: draws the rotating quad at its current
/// angle while the session's view is 8.
void func_neo_ark_pyramid_8017DB5C(Task* task)
{
    if (gGameSession->at4.loc.view == 8) {
        func_neo_ark_pyramid_8017D7F4(D_neo_ark_pyramid_801818A4);
    }
}

/// Task tick that dispatches on the task's state through the three-entry
/// handler table `D_neo_ark_pyramid_8017D5C4`, copied to the stack first.
void func_neo_ark_pyramid_8017DB98(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_pyramid_8017D5C4;
    sp.funcs[task->state](task);
}

/// One-shot task: on its first tick stores 0x601E2, 0x601FE and 0x6021A into
/// three gameplay globals and sets `Gp_State1C` room effect mode 2.
void func_neo_ark_pyramid_8017DBF0(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758                 = 0x601E2;
        D_8011572C                 = 0x601FE;
        D_80115750                 = 0x6021A;
        Gp_State1C->roomEffectMode = 2;
        arg0->state                = 1;
    }
}

/// `Gp_State1C` effect task for a flash that builds up and bursts. Over
/// `spawnArg1` frames it ramps a brightness from zero to 0x100, drawing two
/// wedge discs and a ring in the colour `(b, b/4, b/2)`; on the last frame it
/// flashes the screen with that colour, then draws the star-shaped glow
/// while dimming by 0x10 a frame, and releases the effect once the brightness
/// is spent. Once the room's event state leaves zero it only waits for state 4
/// to release.
void func_neo_ark_pyramid_8017DC50(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = (u16)work->scale >> 2;
                rgb[2] = (u16)work->scale >> 1;
                func_neo_ark_pyramid_8017E320(coord, (s16)work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_neo_ark_pyramid_8017E320(coord, (s16)((u16)work->angle * 2), rgb);
                func_neo_ark_pyramid_8017DEF4(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = (u16)work->scale >> 2;
                    rgb[2]      = (u16)work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if ((s16)work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = (u16)work->scale >> 1;
                    func_neo_ark_pyramid_8017F224(coord, (s16)((s16)work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4`
/// segments forming a ring. The edge at `arg1` is black and the edge at
/// `arg1 + arg2` takes the colour `rgb`, both signed half-extents scaled to
/// the screen as `r * 64 / (otz + 1)`.
void func_neo_ark_pyramid_8017DEF4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    s32                sum;
    s32                otz;
    register s32       rOuter asm("v0");
    s32                rInner;
    u8*                color;
    s32                t;
    u16                vz;
    u16                vx;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    vx = *(u16*)&arg0->workm.t[0];
    USE_REG(vx);
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw02Scratch*)tmp;
    }
    block->vec.vx = vx;
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw02Scratch*)(head - 0x1C))->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw02Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw02Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        USE_REG(head);
        otz                                      = ((RoomDraw02Scratch*)(head - 0x1C))->otz + 1;
        rOuter                                   = ((s16)saved * 64) / otz;
        ((RoomDraw02Scratch*)(head - 0x1C))->otz = otz;
        rInner                                   = (s16)sum * 64;
        block->rOuter                            = rOuter;
        rInner                                   = rInner / ((RoomDraw02Scratch*)(head - 0x1C))->otz;
        ang                                      = 0;
        block->rInner                            = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues eight gouraud `POLY_G4` wedges around
/// the projected centre. `arg1` is a signed half-extent; the on-screen radius
/// is `(s16)arg1 * 64 / (otz + 1)`. Only the centre vertex takes the colour
/// `rgb`, so each wedge fades to black at the rim.
void func_neo_ark_pyramid_8017E320(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
{
    RoomDraw04Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    u8*                head;
    s32                otz;
    s32                radius;
    s32                t;
    s32                t2;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw04Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw04Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw04Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw04Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw04Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        otz           = block->otz + 1;
        radius        = ((s16)arg1 * 64) / otz;
        block->otz    = otz;
        block->radius = radius;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// `Gp_State1C` effect task drawing a ribbon between the trails of two points
/// on the spawner's parent coordinate. The first frame allocates sixteen
/// coordinates, places the effect at the first offset, and fills both
/// eight-slot trails with the two points' current view positions. Each later
/// frame records the two points into the next slot, rebuilds all sixteen,
/// draws the ribbon through `func_neo_ark_pyramid_8017EBA4` and releases the
/// effect once its age reaches `spawnArg1`. It stops advancing once the room's
/// event state reaches 2.
void func_neo_ark_pyramid_8017E6B4(Task* task)
{
    GsCOORDINATE2  coord;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* objCoord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    SVECTOR*       vec;
    s32            i;

    coords   = (GsCOORDINATE2*)task->work;
    work     = (GpEffWork*)task->spawnArg2;
    objCoord = ((TmdObject*)task->extra)->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GsCOORDINATE2*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_neo_ark_pyramid_8017FC18[0].vx;
                objCoord->coord.t[1] = D_neo_ark_pyramid_8017FC18[0].vy;
                objCoord->coord.t[2] = D_neo_ark_pyramid_8017FC18[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_neo_ark_pyramid_8017FC18[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->parent;
                coord.coord.t[0] = D_neo_ark_pyramid_8017FC20.vx;
                coord.coord.t[1] = D_neo_ark_pyramid_8017FC20.vy;
                coord.coord.t[2] = D_neo_ark_pyramid_8017FC20.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->age & 7) + 8];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_neo_ark_pyramid_8017EBA4(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws a ribbon between two eight-slot coordinate trails as seven gouraud
/// `POLY_G4` quads, walking backwards from slot `arg2`. Each quad spans
/// `workm.t` of two adjacent slots on `arg0` and `arg1`; its newer edge is
/// weighted `0x40 - 9 * i` and its older edge nine less, so the ribbon fades
/// along its length. `arg3` holds the colour as per-channel multipliers of that
/// weight: red from bits 8 up, green from bits 4-5, blue from bits 0-1. A quad
/// the GTE flags as bad is skipped.
void func_neo_ark_pyramid_8017EBA4(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GsCOORDINATE2*     a;
    GsCOORDINATE2*     b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    {
        register u8* tmp asm("v0");

        tmp                     = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomDraw03Scratch);
        blk                     = (RoomDraw03Scratch*)tmp;
        *(void**)G_SCRATCH_HEAD = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        blk->v[2].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = *(u16*)&b->workm.t[0];
        blk->v[3].vy = *(u16*)&b->workm.t[1];
        blk->v[3].vz = *(u16*)&b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps_real();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt_real();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = (POLY_G4*)gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomDraw03Scratch);
}

/// `Gp_State1C` effect task for a burst. The first frame spawns effect
/// `0x60076` at the coordinate and then, depending on `spawnArg1`, either
/// effect `0x60070` followed by seven frames of randomly aimed `0x60070`
/// sparks, or two `0x6007C` effects followed by seven frames of two expanding
/// rings in a fading `(a, a/2, a/4)` colour. The effect is then released.
/// Once the room's event state leaves zero it only waits for state 4 to
/// release.
void func_neo_ark_pyramid_8017EF9C(Task* task)
{
    GsCOORDINATE2* objCoord;
    GpEffWork*     work;
    u8             rgb[4];

    objCoord = ((TmdObject*)task->extra)->coords;
    work     = (GpEffWork*)task->spawnArg2;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1 != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = (u16)work->angle >> 1;
            rgb[2]       = (u16)work->angle >> 2;
            func_neo_ark_pyramid_8017DEF4(objCoord, 0x100, 0x100, rgb);
            func_neo_ark_pyramid_8017DEF4(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, draws a star-shaped glow from gouraud
/// `POLY_G4` wedges that are coloured only at the centre and fade to black.
/// `arg1` is a signed half-extent giving an outer radius
/// `arg1 * 64 / (otz + 1)` and an inner one `arg1 * 8 / (otz + 1)`. Eight
/// wedges span the outer radius in half the colour `arg2`, eight more span
/// half of it at full colour, and four long spikes reach twice the outer
/// radius between points on the inner one.
void func_neo_ark_pyramid_8017F224(GsCOORDINATE2* arg0, s16 arg1, u8* arg2)
{
    register RoomBillboardScratch* block asm("s3");
    register POLY_G4*              prim asm("s2");
    register s32                   ang asm("s4");
    register void**                scratch asm("a1");
    register u8*                   head asm("a2");
    s32                            t;
    s32                            t2;
    s32                            u;
    u16                            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                             = *(u16*)&arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomBillboardScratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomBillboardScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomBillboardScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz   += 1;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
