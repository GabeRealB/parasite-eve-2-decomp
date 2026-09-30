#include "rooms/shelter_b1_control_room_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"

/// The room's message table, installed on its task by state 0.
extern GpMsgEntry D_shelter_b1_control_room_access_tunnel_80181E74[];

extern SVECTOR D_shelter_b1_control_room_access_tunnel_80181E9C[];
extern SVECTOR D_shelter_b1_control_room_access_tunnel_80181EAC[];

/// The smoke trail's two spawn offsets: `[0]` places the effect's own
/// coordinate and `[1]` the second trail's origin. State 1 reads `[1]` again
/// under its own name.

s32 func_shelter_b1_control_room_access_tunnel_8017D5E4(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_shelter_b1_control_room_access_tunnel_8017D5EC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b1_control_room_access_tunnel_8017D630(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_shelter_b1_control_room_access_tunnel_8017D638(Task*, s32, TaskMessageArg, TaskMessageArg);

GpMsgEntry D_shelter_b1_control_room_access_tunnel_80181E74[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_control_room_access_tunnel_8017D5EC },
    { 5105, func_shelter_b1_control_room_access_tunnel_8017D5E4 },
    { 5103, func_shelter_b1_control_room_access_tunnel_8017D638 },
    { 5104, func_shelter_b1_control_room_access_tunnel_8017D630 },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_shelter_b1_control_room_access_tunnel_80181E9C[2] = {
    { 5017, -196, 1184, 0 },
    { 4172, -196, 1184, 0 },
};

SVECTOR D_shelter_b1_control_room_access_tunnel_80181EAC[7] = {
    { 2313, -196, 1184, 0 },
    { 1527, -196, 1184, 0 },
    { 5017, -196, -1028, 0 },
    { 4172, -196, -1028, 0 },
    { 2313, -196, -1028, 0 },
    { 1527, -196, -1028, 0 },
    { 4266, -1272, -1160, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

static void func_shelter_b1_control_room_access_tunnel_8017D640(Task* task);
static void func_shelter_b1_control_room_access_tunnel_8017D684(Task* task);
static void func_shelter_b1_control_room_access_tunnel_8017D6E4(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b1_control_room_access_tunnel_8017DE60(SVECTOR* arg0, s16 arg1);

s32 func_shelter_b1_control_room_access_tunnel_8017D5E4(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one,
/// passes both to `func_map_shelter_80179A04` and returns 1.
s32 func_shelter_b1_control_room_access_tunnel_8017D5EC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

s32 func_shelter_b1_control_room_access_tunnel_8017D630(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b1_control_room_access_tunnel_8017D638(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// State 0 of the room's task: installs the room's message table, publishes
/// the task in pointer slot 7 and advances to state 1.
static void func_shelter_b1_control_room_access_tunnel_8017D640(Task* task)
{
    task->msgTable = D_shelter_b1_control_room_access_tunnel_80181E74;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's task: does nothing until the task is killed.
static void func_shelter_b1_control_room_access_tunnel_8017D684(Task* task)
{
}

/// State handlers of the task `func_shelter_b1_control_room_access_tunnel_8017D68C`
/// runs, which copies the table to the stack and calls the entry for the
/// task's state: the room's setup, an idle state, and `taskKill`.
static const TaskFuncTable3 D_shelter_b1_control_room_access_tunnel_8017D5C4 = {
    { func_shelter_b1_control_room_access_tunnel_8017D640, func_shelter_b1_control_room_access_tunnel_8017D684, taskKill }
};

/// Runs the room's task through its three-state handler table, copied onto
/// the stack before the call.
void func_shelter_b1_control_room_access_tunnel_8017D68C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_control_room_access_tunnel_8017D5C4;
    sp.funcs[task->state](task);
}

/// Projects `arg0[0]` and `arg0[1]` through `gGfxViewCoord.workm` and, when the
/// second point's OTZ is above 0x10, queues gouraud `POLY_G4` quads for a cone
/// between them: wedges at the first point, bands joining the two ends and
/// wedges at the second point, starting from the angle `arg2`. The radius at
/// each end is `arg1 * 64 / otz`, with the first point's OTZ clamped to at
/// least 0x10, and the grey level alternates between 0x20 and 0x28 on odd and
/// even frames.
static void func_shelter_b1_control_room_access_tunnel_8017D6E4(SVECTOR* arg0, s32 arg1, s32 arg2)
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

        scratch  = SCRATCH_STACK_CURSOR_SLOT;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
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
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            t3             = ang - 0x1000;
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Projects `arg0` through `gGfxViewCoord.workm` and, when its OTZ is above 0x10,
/// queues four gouraud `POLY_G4` wedges forming a red disc around it, of radius
/// `arg1 * 64 / otz`. The centre's red level alternates between 0x20 and 0x28
/// on odd and even frames.
static void func_shelter_b1_control_room_access_tunnel_8017DE60(SVECTOR* arg0, s16 arg1)
{
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                rgb;
    s32                radius;

    block = SCRATCH_STACK_RESERVE_BLOCK(RoomDraw25Scratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stszotz(&block->otz);
    if (block->otz >= 0x11) {
        radius        = (arg1 * 64) / block->otz;
        rgb           = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        ang           = 0;
        block->radius = radius;
        do {
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomDraw25Scratch);
}

/// On its first tick stores six effect ids in gameplay's `D_801157xx` slots;
/// every tick then draws the room's gouraud cones and disc for the current
/// camera view (views 2 and 3).
void func_shelter_b1_control_room_access_tunnel_8017E1BC(Task* arg0)
{
    u8 view;

    if (arg0->state == 0) {
        D_80115758  = 0x601CF;
        D_8011572C  = 0x601EB;
        D_80115750  = 0x60207;
        D_80115734  = 0x60279;
        D_80115730  = 0x6027A;
        D_80115754  = 0x6027B;
        arg0->state = 1;
    }
    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_b1_control_room_access_tunnel_80181E9C;
            func_shelter_b1_control_room_access_tunnel_8017D6E4(&p[0], 0x200, 0x400);
            func_shelter_b1_control_room_access_tunnel_8017D6E4(&p[4], 0x200, 0x400);
            func_shelter_b1_control_room_access_tunnel_8017DE60(&p[8], 0x200);
        } break;
        case 3: {
            SVECTOR* p = D_shelter_b1_control_room_access_tunnel_80181EAC;
            func_shelter_b1_control_room_access_tunnel_8017D6E4(&p[0], 0x200, -0x400);
            func_shelter_b1_control_room_access_tunnel_8017D6E4(&p[4], 0x200, -0x400);
        } break;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b1_control_room_access_tunnel_8017E2D8(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b1_control_room_access_tunnel_8017ED3C(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b1_control_room_access_tunnel_8017F624(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
