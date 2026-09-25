#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017e4f8.h"

extern s32 D_80070F70;
extern s16 D_80071076;
extern u8  D_80115690;
extern s32 D_8011572C;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115750;
extern s32 D_80115754;
extern s32 D_80115758;
extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Descriptor of the event task the message handler spawns.
extern TaskDesc D_shelter_b1_south_maintenance_walkway_801822FC;

/// The room's message table.
extern GpMsgEntry D_shelter_b1_south_maintenance_walkway_80182308[];

/// Points the room task draws its glows and discs at, depending on the view.
extern SVECTOR D_shelter_b1_south_maintenance_walkway_80182330[];
extern SVECTOR D_shelter_b1_south_maintenance_walkway_801823D0[];

/// The two points of the twin trail, as offsets from its anchor frame. The
/// second is also reached under its own name.
extern SVECTOR D_shelter_b1_south_maintenance_walkway_801823D8[];
extern SVECTOR D_shelter_b1_south_maintenance_walkway_801823E0;

/// Per-colour channel shifts for the glowing disc, indexed by the spawn
/// argument.
extern RoomHaloShade D_shelter_b1_south_maintenance_walkway_801823E8[];

/// Spawn payload of the task 0x31 the event task may start.
extern GpStateBD8       D_shelter_b1_south_maintenance_walkway_80183634;
extern RoomEventMsg     D_shelter_b1_south_maintenance_walkway_8018363C;
extern s8               D_shelter_b1_south_maintenance_walkway_80183644;
extern RoomLatchedEvent D_shelter_b1_south_maintenance_walkway_80183648;

void func_shelter_b1_south_maintenance_walkway_8017DC88(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b1_south_maintenance_walkway_8017E404(SVECTOR* arg0, s16 arg1);
void func_shelter_b1_south_maintenance_walkway_8017EA04(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b1_south_maintenance_walkway_8017EE30(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_b1_south_maintenance_walkway_8017F6B4(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
void func_shelter_b1_south_maintenance_walkway_8017FD34(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);
void func_shelter_b1_south_maintenance_walkway_80180E70(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b1_south_maintenance_walkway_801810F4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b1_south_maintenance_walkway_80181518(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_b1_south_maintenance_walkway_80181A58(GsCOORDINATE2* coord, s16 size);
void func_shelter_b1_south_maintenance_walkway_80181F84(GsCOORDINATE2* arg0, s32 arg1);

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _shelterB1SouthMaintenanceWalkwayStartEvent(
    RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b1_south_maintenance_walkway_80183644 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b1_south_maintenance_walkway_8018363C = *dst;
            D_shelter_b1_south_maintenance_walkway_80183648 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_south_maintenance_walkway_801822FC, 0, 0, 0);
            D_shelter_b1_south_maintenance_walkway_80183644 = 1;
        }
        return 2;
    }
    return 1;
}

/// The event task the room's message handler spawns. It runs the latched
/// event's CAP command and waits for it to finish, starting task 0x31 when the
/// event asks for it; then plays the event's stage sound (if any) and waits
/// for the voice to end. Finally it commits the latched message's area, warp
/// and room as the save location, respawns the player task as type 0x11 and
/// ends.
void func_shelter_b1_south_maintenance_walkway_8017D5F8(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b1_south_maintenance_walkway_80183648.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b1_south_maintenance_walkway_80183648.fade != 0) {
                    D_shelter_b1_south_maintenance_walkway_80183634.field_0 = 0;
                    D_shelter_b1_south_maintenance_walkway_80183634.field_1 = 0;
                    D_shelter_b1_south_maintenance_walkway_80183634.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b1_south_maintenance_walkway_80183634);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b1_south_maintenance_walkway_80183648.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b1_south_maintenance_walkway_80183648.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b1_south_maintenance_walkway_80183648.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b1_south_maintenance_walkway_8018363C.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b1_south_maintenance_walkway_8018363C.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_b1_south_maintenance_walkway_8018363C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Messages 9 and 0xB start the room's event, each with its
/// own parameters and flag; any other message answers 1.
s32 func_shelter_b1_south_maintenance_walkway_8017D790(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 9) {
        event.capCmd   = 1;
        event.stageSnd = 0x540A0001;
        event.flagId   = 0x14B;
        event.fade     = 0;
        return _shelterB1SouthMaintenanceWalkwayStartEvent(out, &event);
    }
    if (in->msgId == 0xB) {
        event.capCmd   = 2;
        event.stageSnd = 0x540A0003;
        event.flagId   = 0x14C;
        event.fade     = 0;
        return _shelterB1SouthMaintenanceWalkwayStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_b1_south_maintenance_walkway_8017D9D0(void)
{
    return 0;
}

s32 func_shelter_b1_south_maintenance_walkway_8017D9D8(void)
{
    return 0;
}

s32 func_shelter_b1_south_maintenance_walkway_8017D9E0(void)
{
    return 0;
}

/// Installs the room's message table on `task`, publishes the task in pointer
/// slot 7 and steps it to its next state.
void func_shelter_b1_south_maintenance_walkway_8017D9E8(Task* task)
{
    task->msgTable = D_shelter_b1_south_maintenance_walkway_80182308;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// The room task's idle state: does nothing.
void func_shelter_b1_south_maintenance_walkway_8017DA2C(Task* task)
{
}

/// The room task's three states: set-up, idle and exit.
const TaskFuncTable3 D_shelter_b1_south_maintenance_walkway_8017D5D8 = {
    { func_shelter_b1_south_maintenance_walkway_8017D9E8, func_shelter_b1_south_maintenance_walkway_8017DA2C, taskKill },
};

/// The room task. Runs the handler for its current state from the room's
/// three-entry state table, copied to the stack first.
void func_shelter_b1_south_maintenance_walkway_8017DA34(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_south_maintenance_walkway_8017D5D8;
    sp.funcs[task->state](task);
}

void func_shelter_b1_south_maintenance_walkway_8017DA8C(Task* task)
{
    SVECTOR* p;

    if (task->state == 0) {
        D_80115758  = 0x601C9;
        D_8011572C  = 0x601E5;
        D_80115750  = 0x60201;
        D_80115734  = 0x6021C;
        D_80115730  = 0x6021B;
        D_80115754  = 0x6021D;
        task->state = 1;
    }

    switch (gGameSession->at4.loc.view) {
        case 2:
            p = D_shelter_b1_south_maintenance_walkway_80182330;
            SOFT_TOUCH_REG(p);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[0], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[6], 0x200, 0x400);
            break;
        case 3:
            p = D_shelter_b1_south_maintenance_walkway_80182330;
            SOFT_TOUCH_REG(p);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[0], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[2], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[6], 0x200, 0x400);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[8], 0x200, 0x400);
            break;
        case 4:
            p = D_shelter_b1_south_maintenance_walkway_801823D0;
            SOFT_TOUCH_REG(p);
            func_shelter_b1_south_maintenance_walkway_8017E404(&p[0], 0x200);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[-16], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[-10], 0x200, -0x400);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[-6], 0x200, 0x800);
            break;
        case 5:
            p = D_shelter_b1_south_maintenance_walkway_801823D0;
            SOFT_TOUCH_REG(p);
            func_shelter_b1_south_maintenance_walkway_8017E404(&p[0], 0x200);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[-16], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[-8], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[-6], 0x200, -0x400);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[-4], 0x200, 0);
            func_shelter_b1_south_maintenance_walkway_8017DC88(&p[-2], 0x200, -0x400);
            break;
    }
}

/// Queues a grey gouraud glow spanning the projected points `arg0[0]` and
/// `arg0[1]`: a half-disc at each end, of radius `arg1` scaled by that end's
/// depth and turned by the angle `arg2`, joined by quads. The brightness
/// alternates between 0x20 and 0x28 on successive frames. Nothing is drawn when
/// the second point lies nearer than OTZ 0x11.
void func_shelter_b1_south_maintenance_walkway_8017DC88(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        base      = (s16)arg2;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = rgb;
            p->g2    = rgb;
            prim->b2 = rgb;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            SCHED_BARRIER();
            t3             = ang - 0x1000;
            prim           = (POLY_G4*)gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Queues a red gouraud disc of four quads at the projected point `arg0`, of
/// radius `arg1` scaled by depth, its centre alternating between 0x20 and 0x28
/// on successive frames. Nothing is drawn nearer than OTZ 0x11.
void func_shelter_b1_south_maintenance_walkway_8017E404(SVECTOR* arg0, s16 arg1)
{
    u8*                head;
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                rgb;
    s32                radius;

    {
        void**       scratch;
        register u8* tmp asm("v0");

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0xC;
        block    = (RoomDraw25Scratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw25Scratch*)(head - 0xC))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - 0xC))->otz >= 0x11) {
        radius        = (arg1 * 64) / ((RoomDraw25Scratch*)(head - 0xC))->otz;
        rgb           = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        ang           = 0;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0xC;
}

/// A flash that swells and then fades. For as many ticks as the spawn argument
/// it brightens and grows two discs and a ring at its frame, all tinted
/// (level, level / 4, level / 2); at full brightness it draws a fade quad, then
/// draws a two-ring billboard that dims by 0x10 a tick and releases its work
/// block once the level falls to 0x10. It pauses while the room's event state
/// is set and releases the block when that state reaches 4.
void func_shelter_b1_south_maintenance_walkway_8017E760(Task* task)
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
                func_shelter_b1_south_maintenance_walkway_8017EE30(coord, (s16)work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b1_south_maintenance_walkway_8017EE30(coord, (s16)((u16)work->angle * 2), rgb);
                func_shelter_b1_south_maintenance_walkway_8017EA04(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
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
                    func_shelter_b1_south_maintenance_walkway_8017FD34(coord, (s16)((s16)work->angle * 3), rgb);
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

/// Queues a gouraud ring of sixteen quads around the projected world position
/// of `arg0`: black at radius `arg1` and shaded `rgb` at radius `arg1 + arg2`,
/// both scaled by depth. Nothing is drawn when the projection overflows.
void func_shelter_b1_south_maintenance_walkway_8017EA04(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
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
    gte_rtps();
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

/// Queues a gouraud disc of eight wedges around the projected world position
/// of `arg0`, shaded `rgb` at the centre and black at the rim, of radius
/// `arg1` scaled by depth. Nothing is drawn when the projection overflows.
void func_shelter_b1_south_maintenance_walkway_8017EE30(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
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
    gte_rtps();
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

/// A twin trail. The first tick allocates sixteen coordinate frames, eight for
/// each trail, and seeds them all from the two points offset from the anchor,
/// so both trails start collapsed. Each later tick re-places the two points,
/// records them in the next slot of each ring of eight and draws the trails
/// between the rings as a beam. The work block is released once the tick count
/// reaches the spawn argument. It idles while the room's event state is 2 or
/// more.
void func_shelter_b1_south_maintenance_walkway_8017F1C4(Task* task)
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
                objCoord->coord.t[0] = D_shelter_b1_south_maintenance_walkway_801823D8[0].vx;
                objCoord->coord.t[1] = D_shelter_b1_south_maintenance_walkway_801823D8[0].vy;
                objCoord->coord.t[2] = D_shelter_b1_south_maintenance_walkway_801823D8[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_shelter_b1_south_maintenance_walkway_801823D8[1];
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
                coord.coord.t[0] = D_shelter_b1_south_maintenance_walkway_801823E0.vx;
                coord.coord.t[1] = D_shelter_b1_south_maintenance_walkway_801823E0.vy;
                coord.coord.t[2] = D_shelter_b1_south_maintenance_walkway_801823E0.vz;
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
                func_shelter_b1_south_maintenance_walkway_8017F6B4(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the beam between two rings of eight coordinate frames as seven
/// gouraud quads, walking back from slot `arg2`, each quad joining two adjacent
/// slots of both rings and dimmer the older it is. `arg3` packs the colour as
/// three multipliers, at bits 8, 4 and 0. A quad whose projection overflows is
/// skipped.
void func_shelter_b1_south_maintenance_walkway_8017F6B4(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3)
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
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
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

/// A spark burst. The first tick spawns its flash effect; then, for a non-zero
/// spawn argument, it sprays randomly jittered sparks each tick, and for zero
/// it draws a fixed ring and one widening by 0x30 a tick, both dimming by 0x20
/// a tick. Either way it releases its work block after seven ticks. It pauses
/// while the room's event state is set and releases the block when that state
/// reaches 4.
void func_shelter_b1_south_maintenance_walkway_8017FAAC(Task* task)
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
            func_shelter_b1_south_maintenance_walkway_8017EA04(objCoord, 0x100, 0x100, rgb);
            func_shelter_b1_south_maintenance_walkway_8017EA04(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Queues a star-shaped glow at the projected world position of `arg0`: a
/// disc of radius `arg1` scaled by depth, shaded half `arg2` at the centre, an
/// inner disc of half that radius at full `arg2`, and four thin rays at right
/// angles, alternately reaching the radius and twice it, all fading to black
/// at the rim. Nothing is drawn when the projection overflows.
void func_shelter_b1_south_maintenance_walkway_8017FD34(GsCOORDINATE2* arg0, s16 arg1, u8* arg2)
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
    gte_rtps();
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

/// A glowing disc attached to its parent at the work block's position. In
/// state 1 the disc grows, and every fourth tick the task spawns the effect
/// `D_80115730` names at a random joint of the player's model and adopts it
/// as a child task; state 2 adds a flickering half-bright second disc; state 3
/// drifts the disc away while it fades inside an expanding ring, then releases
/// the work block. The spawn argument picks the disc's colour shifts. It
/// pauses while the room's event state is set and releases the block when that
/// state reaches 4.
void func_shelter_b1_south_maintenance_walkway_801806F4(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpEffWork*     spawned;
    MATRIX*        mtx;
    u8             col[4];

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            coord->sub                   = mem->parent;
            mtx                          = &coord->coord;
            *(s32*)&coord->coord.m[0][0] = 0x1000;
            *(s32*)&mtx->m[0][2]         = 0;
            *(s32*)&mtx->m[1][1]         = 0x1000;
            *(s32*)&mtx->m[2][0]         = 0;
            mtx->m[2][2]                 = 0x1000;
            coord->coord.t[0]            = mem->pos.vx;
            coord->coord.t[1]            = mem->pos.vy;
            coord->coord.t[2]            = mem->pos.vz;
            coord->flg                   = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &((TmdObject*)player->extra)->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], (s32)coord, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1].r;
            col[1] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1].g;
            col[2] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1].b;
            func_shelter_b1_south_maintenance_walkway_80181518(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1].r;
            col[1] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1].g;
            col[2] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1].b;
            func_shelter_b1_south_maintenance_walkway_80181518(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b1_south_maintenance_walkway_80181518(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1].r;
            col[1] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1].g;
            col[2] = mem->scale >> D_shelter_b1_south_maintenance_walkway_801823E8[arg0->spawnArg1].b;
            func_shelter_b1_south_maintenance_walkway_80181518(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = (u16)mem->scale >> 1;
            col[2] = (u16)mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            func_shelter_b1_south_maintenance_walkway_801810F4(coord, (s16)(mem->period + 0x80), 0x100, col);
            mem->angle -= 0x10;
            if (mem->scale > 0x10) {
                mem->scale -= 0x10;
                break;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// A spark that flies to another frame. The first tick takes the offset from
/// its own frame to the target frame the spawn argument names, in its own
/// axes, and keeps 0xCC/0x1000 of it as its step. Each later tick moves it by
/// that step and, every other tick, draws it as a textured square at the next
/// animation frame; it releases its work block after 20 ticks. It pauses while
/// the room's event state is set and releases the block when that state
/// reaches 4.
void func_shelter_b1_south_maintenance_walkway_80180C4C(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    VECTOR         delta;

    work   = task->spawnArg2;
    coord  = ((TmdObject*)task->extra)->coords;
    target = (GsCOORDINATE2*)task->spawnArg1;
    if (Gp_State1C->eventState == 0) {
        work->field_22++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->field_18 = delta.vx;
                work->field_1A = delta.vy;
                work->field_1C = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->field_18);
                gte_rtv0();
                gte_stsv(&work->field_18);
                gte_lddp(0xCC);
                gte_ldsv(&work->field_18);
                gte_gpf12();
                gte_stsv(&work->field_18);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0] += (s16)work->field_18;
                coord->coord.t[1] += (s16)work->field_1A;
                coord->coord.t[2] += (s16)work->field_1C;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->field_22 & 1) {
                    func_shelter_b1_south_maintenance_walkway_80180E70(coord, (s16)++work->field_20, 0x200, 0x80);
                }
                if ((s16)work->field_22 >= 20) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    } else if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Queues a semi-transparent textured square centred on the projected world
/// position of `arg0`, of half-size `arg2` scaled by depth. `arg1 & 3` picks
/// the animation frame from a row of four 24-texel frames and `arg3` is the
/// grey level. Nothing is drawn when the projection overflows.
void func_shelter_b1_south_maintenance_walkway_80180E70(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw14Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    DisplayState*      ds;
    s32                tex;
    s32                u0;
    s32                u1;
    s32                sarg;
    s32                t;
    s16                xy;
    u16                vz;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw14Scratch*)(head - 0x18);
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42CB;
        t           = (tex & 3) * 24;
        u0          = t + 0x60;
        u1          = t + 0x77;
        SOFT_USE_REG(u1);
        prim->u0 = u0;
        prim->u2 = u0;
        prim->v2 = 0x17;
        prim->v3 = 0x17;
        sarg     = (s16)arg2;
        prim->u1 = u1;
        prim->u3 = u1;
        t        = sarg * 24;
        setRGB0(prim, arg3, arg3, arg3);
        prim->v0      = 0;
        prim->v1      = 0;
        block->radius = (t - sarg) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = *(u16*)&block->sy - *(u16*)&block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = *(u16*)&block->sy + *(u16*)&block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        ds            = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

/// Queues a gouraud ring of sixteen quads around the projected world position
/// of `arg0`: black at radius `arg1` and shaded `rgb` at radius `arg1 + arg2`,
/// both scaled by depth. Nothing is drawn when the projection overflows. The
/// same drawing as `func_shelter_b1_south_maintenance_walkway_8017EA04`, with
/// its scratch block laid out differently.
void func_shelter_b1_south_maintenance_walkway_801810F4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw07Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    register s32       sum asm("a1");
    register s32       otz asm("v0");
    register s32       rOuter asm("a0");
    register s32       rInner asm("v1");
    register u8*       color asm("s4");
    s32                t;
    u16                vz;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw07Scratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw07Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw07Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw07Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw07Scratch*)(head - 0x1C))->otz);
        USE_REG(head);
        otz        = block->otz + 1;
        rOuter     = ((s16)saved * 64) / otz;
        rInner     = (s16)sum * 64;
        block->otz = otz;
        SOFT_BARRIER();
        rInner        = rInner / otz;
        ang           = 0;
        block->rOuter = rOuter;
        block->rInner = rInner;

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

/// Queues a gouraud disc of eight wedges around the projected world position
/// of `arg0`, shaded `rgb` at the centre and black at the rim, of radius
/// `arg1` scaled by depth. Nothing is drawn when the projection overflows. The
/// same drawing as `func_shelter_b1_south_maintenance_walkway_8017EE30`, with
/// its scratch block laid out differently.
void func_shelter_b1_south_maintenance_walkway_80181518(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
{
    void**             scratch;
    u8*                head;
    RoomDraw10Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register s32       ang2 asm("s1");
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw10Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw10Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw10Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw10Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw10Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
        ang         = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->step * rcos(ang)) >> 12);
            ang2     = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            ang2     = ang + 0x200;
            prim->x3 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            ang      = ang2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// A burst in orange. Each tick draws a disc and a glow at a growing size
/// while a wider, dimmer ring expands and fades behind them; once that ring is
/// gone the main level falls 0x18 a tick and the work block is released. It
/// pauses while the room's event state is set and releases the block when that
/// state reaches 4.
void func_shelter_b1_south_maintenance_walkway_801818AC(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            step;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x80;
            mem->period = 0xE0;
            mem->step   = 0x80;
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]     = mem->scale;
        rgb[1]     = (u16)mem->scale >> 1;
        rgb[2]     = (u16)mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_shelter_b1_south_maintenance_walkway_80181518(coord, (s16)(step * 2), rgb);
        func_shelter_b1_south_maintenance_walkway_80181A58(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = (u16)mem->period >> 1;
            rgb[2] = (u16)mem->period >> 2;
            func_shelter_b1_south_maintenance_walkway_801810F4(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x30;
            return;
        }
        mem->scale -= 0x18;
        if (mem->scale < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

/// A glow at the world position of `coord`: two camera-facing textured
/// squares, an inner one of half-size `size` whose texture follows bit 0 of
/// `D_80070F70` and an outer one of `size * 3 / 2` whose palette alternates
/// each frame, plus a mark on the ground beneath it when the ground is found.
/// It also sets the `Gp_RoomCoords[2]` light at the glow's position with a
/// flickering orange intensity. Nothing is drawn when the projection
/// overflows.
void func_shelter_b1_south_maintenance_walkway_80181A58(GsCOORDINATE2* coord, s16 size)
{
    GsCOORDINATE2  ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;
    void**         scratch;
    GpRingScratch* alias;
    u16            vy;
    GpRingScratch* sc;

    slot                        = &Gp_RoomCoords[2];
    slot->framesLeft            = 2;
    light                       = &slot->data.light;
    light->inner                = 0x300;
    light->outer                = 0x3000;
    random                      = (Gp_LcgState * 5) + 0x71357911;
    intensity                   = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r               = intensity;
    shifted                     = intensity << 0x10;
    light->head.g               = (s16)(shifted >> 0x11);
    light->head.b               = (s16)(shifted >> 0x12);
    light->head.u.at.local.t[0] = (s32)coord->coord.t[0];
    light->head.u.at.local.t[1] = (s32)coord->coord.t[1];
    light->head.u.at.local.t[2] = coord->coord.t[2];
    slot->data.coord.flg        = 0;
    scratch                     = (void**)G_SCRATCH_HEAD;
    block                       = (GpRingScratch*)*scratch - 1;
    block->vec.vx               = *(u16*)&coord->workm.t[0];
    alias                       = block;
    vy                          = *(u16*)&coord->workm.t[1];
    __asm__("move %0,%1" : "=r"(alias) : "r"(alias), "r"(vy), "r"(alias));
    sc          = alias;
    sc->vec.vy  = vy;
    sc->vec.vz  = *(u16*)&coord->workm.t[2];
    Gp_LcgState = random;
    *scratch    = sc;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (sc->flag >= 0) {
        gte_stszotz(&block->otz);
        prim                 = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor       = prim + 1;
        ((u8*)&prim->tag)[3] = 9;
        prim->code           = 0x2EU;
        *(u16*)&prim->tpage  = 0x29;
        if (D_80070F70 & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code = (u8)(prim->code | 1);
        }
        sc->step = (s32)((s32)((s16)size * 0x37) / (s32)sc->otz);
        left     = *(u16*)&sc->sx - (u16)sc->step;
        prim->x2 = left;
        prim->x0 = left;
        right    = *(u16*)&sc->sx + (u16)sc->step;
        prim->x3 = right;
        prim->x1 = right;
        top      = *(u16*)&sc->sy - (u16)sc->step;
        prim->y1 = top;
        prim->y0 = top;
        bottom   = *(u16*)&sc->sy + (u16)sc->step;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        prim                 = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor       = prim + 1;
        ((u8*)&prim->tag)[3] = 9;
        prim->code           = 0x2F;
        prim->tpage          = 0x29;
        prim->clut =
            (s16)(((u32)(((gDisplayState.animFrame & 1) * 0x10) + 0x120) >> 4) |
                  0x4300);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)((s16)size * 3 / 2);
        sc->step    = (s32)((s32)(outerSize * 0x37) / (s32)sc->otz);
        outerLeft   = *(u16*)&sc->sx - (u16)sc->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = *(u16*)&sc->sx + (u16)sc->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = *(u16*)&sc->sy - (u16)sc->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = *(u16*)&sc->sy + (u16)sc->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_shelter_b1_south_maintenance_walkway_80181F84(&ground, outerSize);
        }
    }
    *(void**)G_SCRATCH_HEAD =
        (u8*)*(void**)G_SCRATCH_HEAD + sizeof(GpRingScratch);
}

/// Queues a dim red, semi-transparent textured quad at the world position of
/// `arg0`: the unit square `D_80111E38`, taken in the x-z plane, scaled by
/// `arg1`, turned by the view rotation and moved to that position. Its texture alternates
/// between two frames on successive frames. Nothing is drawn when the
/// projection overflows.
void func_shelter_b1_south_maintenance_walkway_80181F84(GsCOORDINATE2* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = block->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = *(u16*)&block->sxy0.vx;
        prim->y0    = *(u16*)&block->sxy0.vy;
        prim->x1    = *(u16*)&block->sxy1.vx;
        prim->y1    = *(u16*)&block->sxy1.vy;
        prim->x2    = *(u16*)&block->sxy2.vx;
        prim->y2    = *(u16*)&block->sxy2.vy;
        prim->x3    = *(u16*)&block->sxy3.vx;
        prim->y3    = *(u16*)&block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}
