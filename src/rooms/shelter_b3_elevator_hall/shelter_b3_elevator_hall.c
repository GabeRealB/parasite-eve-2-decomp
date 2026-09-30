#include "rooms/shelter_b3_elevator_hall.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b3_elevator_hall_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "rooms/rooms_shared_8017dcb8.h"
#include "../../shared/room_visual_effects.h"

/// Set when the gate latched a request and spawned the task that runs it.
/// Descriptor of the task that runs a latched request.
extern TaskDesc D_shelter_b3_elevator_hall_80182A20;
extern TaskDesc D_shelter_b3_elevator_hall_80182A2C[];
extern TaskDesc D_shelter_b3_elevator_hall_80182A68[];
/// The room's message table, which its CAP scripts index.
extern GpMsgEntry D_shelter_b3_elevator_hall_80182A38[];
extern SVECTOR    D_shelter_b3_elevator_hall_80182A74[];
extern SVECTOR    D_shelter_b3_elevator_hall_80182AB4[];
extern SVECTOR    D_shelter_b3_elevator_hall_80182AF4[];
/// Per-palette right shifts applied to the halo's level for red, green and
/// blue, selected by the palette index in the spawn argument.

static void func_shelter_b3_elevator_hall_8017DDCC(Task* task);
static void func_shelter_b3_elevator_hall_8017DE10(Task* task);
static void func_shelter_b3_elevator_hall_8017DFB0(SVECTOR* arg0, s32 arg1, s32 arg2);

void func_shelter_b3_elevator_hall_8017D790(Task*);
void func_shelter_b3_elevator_hall_8017D900(Task*);
void func_shelter_b3_elevator_hall_8017DAF0(Task*);
s32  func_shelter_b3_elevator_hall_8017DC78(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_b3_elevator_hall_8017DC80(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b3_elevator_hall_8017DD88(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_b3_elevator_hall_8017DD90(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_b3_elevator_hall_8017DD98(Task*, s32, s32, TaskMessageArg);

TaskDesc D_shelter_b3_elevator_hall_80182A20 = { 0, 32, func_shelter_b3_elevator_hall_8017D790, { .model = NULL } };

TaskDesc D_shelter_b3_elevator_hall_80182A2C[1] = {
    { 0, 32, func_shelter_b3_elevator_hall_8017D900, { .model = NULL } },
};

GpMsgEntry D_shelter_b3_elevator_hall_80182A38[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b3_elevator_hall_8017DC80 },
    { 5105, func_shelter_b3_elevator_hall_8017DC78 },
    { 5103, func_shelter_b3_elevator_hall_8017DD90 },
    { 5104, func_shelter_b3_elevator_hall_8017DD88 },
    { 5106, func_shelter_b3_elevator_hall_8017DD98 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_shelter_b3_elevator_hall_80182A68[1] = {
    { 0, 32, func_shelter_b3_elevator_hall_8017DAF0, { .model = NULL } },
};

SVECTOR D_shelter_b3_elevator_hall_80182A74[8] = {
    { -5771, -3058, 151, 0 },
    { -5771, -3058, -1003, 0 },
    { -5653, -3058, 151, 0 },
    { -5653, -3058, -1003, 0 },
    { -3341, -3058, -1525, 0 },
    { -2185, -3058, -1525, 0 },
    { -3341, -3058, -1644, 0 },
    { -2185, -3058, -1644, 0 },
};

SVECTOR D_shelter_b3_elevator_hall_80182AB4[8] = {
    { -357, -3058, -1525, 0 },
    { 798, -3058, -1525, 0 },
    { -357, -3058, -1644, 0 },
    { 798, -3058, -1644, 0 },
    { 2609, -3058, -1525, 0 },
    { 3763, -3058, -1525, 0 },
    { 2609, -3058, -1644, 0 },
    { 3763, -3058, -1644, 0 },
};

SVECTOR D_shelter_b3_elevator_hall_80182AF4[8] = {
    { 3863, -2122, 3666, 0 },
    { 5022, -2122, 3666, 0 },
    { 3863, -2032, 3742, 0 },
    { 5022, -2032, 3742, 0 },
    { 7230, -2032, 3742, 0 },
    { 8394, -2032, 3742, 0 },
    { 7230, -2122, 3666, 0 },
    { 8394, -2122, 3666, 0 },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 1685 }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"

static inline RoomHaloShade* RoomFx_GetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

static s32 func_shelter_b3_elevator_hall_8017D62C(RoomEventReq* req, RoomEventMsg* msg);

/// Decides whether the event `req` describes fires for message `msg`. A set
/// flag nibble (a clear one for a negative `flagId`) means it already has, and
/// the answer is 1. Without the prerequisite collected item the request's CAP
/// command runs and the answer is 0. Otherwise the request and message are
/// latched, the flag nibble is written, the task that runs the request is
/// spawned, and the answer is 2. A non-zero `queryOnly` on the message only asks
/// for the answer and changes nothing.
static s32 func_shelter_b3_elevator_hall_8017D62C(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                   = req->flagId;
    D_shelter_b3_elevator_hall_80184A08[0] = 0;
    neg                                    = flag < 0;
    got                                    = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->queryOnly == ROOM_EVENT_EXECUTE) {
                D_shelter_b3_elevator_hall_80184A00 = *msg;
                D_shelter_b3_elevator_hall_80184A0C = *req;
                id                                  = req->flagId;
                mode                                = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b3_elevator_hall_80182A20, 0, 0, 0);
                D_shelter_b3_elevator_hall_80184A08[0] = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->flagId, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// Runs a latched event request: plays its CAP command and its two voice
/// cues in turn, waiting for each to finish, then queues a type-7 sound event
/// and loads the area, warp and room the latched message names.
void func_shelter_b3_elevator_hall_8017D790(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b3_elevator_hall_80184A0C.field_0);
            if (D_shelter_b3_elevator_hall_80184A0C.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b3_elevator_hall_80184A0C.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b3_elevator_hall_80184A0C.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b3_elevator_hall_80184A0C.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b3_elevator_hall_80184A0C.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b3_elevator_hall_80184A0C.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b3_elevator_hall_80184A00.areaId;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b3_elevator_hall_80184A00.warp;
            Mc_SaveData[0].state.at4.loc.room = (u8)D_shelter_b3_elevator_hall_80184A00.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Destination choice: waits for the CAP prompt to close, then maps the
/// chosen event key 0xB, 0xC or 0xD to area 9 warp 3, area 0x1B warp 2 or
/// area 0x2A warp 3; any other key hands control back and ends the task.
/// After the voice cue in the spawn argument finishes, it resolves the room
/// through `func_map_shelter_80179A04` and starts the load.
void func_shelter_b3_elevator_hall_8017D900(Task* task)
{
    RoomEventMsg msg;
    RoomEventMsg msg2;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_StateF0.field_4 = 1;
            goto next;
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_StateF0.field_4 = 0;
                goto next;
            }
            break;
        case 2:
            Gp_StateF0.field_4 = 1;
            switch (Gp_GetCapEventKey()) {
                case 0xB:
                    Mc_SaveData[0].state.at4.loc.area = 9;
                    Mc_SaveData[0].state.at4.loc.warp = 3;
                    break;
                case 0xC:
                    Mc_SaveData[0].state.at4.loc.area = 0x1B;
                    Mc_SaveData[0].state.at4.loc.warp = 2;
                    break;
                case 0xD:
                    Mc_SaveData[0].state.at4.loc.area = 0x2A;
                    Mc_SaveData[0].state.at4.loc.warp = 3;
                    break;
                default:
                    Gp_MsgPlayerWeapon(1);
                    Gp_StateF0.field_4 = 0;
                    taskKill(task);
                    break;
            }
            goto next;
        case 3:
            if (SndVoice_HasActiveId(task->spawnArg1.value) != 0) {
                break;
            }
        next:
            task->state++;
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            msg.room      = 1;
            msg.queryOnly = ROOM_EVENT_EXECUTE;
            msg.areaId    = Mc_SaveData[0].state.at4.loc.area;
            msg.warp      = Mc_SaveData[0].state.at4.loc.warp;
            msg2          = msg;
            func_map_shelter_80179A04(&msg, &msg2);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.warp = msg2.warp;
            Mc_SaveData[0].state.at4.loc.room = msg2.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// State table of the room's message-driven task: publish the message table,
/// idle, then kill the task.
static const TaskFuncTable3 D_shelter_b3_elevator_hall_8017D5F0 = {
    {
        func_shelter_b3_elevator_hall_8017DDCC,
        func_shelter_b3_elevator_hall_8017DE10,
        taskKill,
    },
};

void func_shelter_b3_elevator_hall_8017DAF0(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_StateF0.field_4 = 1;
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            if (GameFlag_GetNibble(0xCF) != 0) {
                Gp_RunCapCmd(4, 0);
                Task_SpawnFromTable(D_shelter_b3_elevator_hall_80182A2C, 0, 0x542A0001, 0);
                taskKill(task);
            } else {
                Gp_RunCapCmd1(3);
            }
            task->state++;
            break;
        case 3:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 4:
            if (Gp_GetCapEventKey() == 0x15) {
                Mc_SaveData[0].state.at4.loc.area = 0x1A;
                Mc_SaveData[0].state.at4.loc.warp = 1;
                Mc_SaveData[0].state.at4.loc.room = 1;
            } else {
                Gp_MsgPlayerWeapon(1);
                Gp_StateF0.field_4 = 0;
                taskKill(task);
            }
            task->state++;
            break;
        case 5:
            if (SndVoice_HasActiveId(0x542A0001) == 0) {
                task->state++;
            }
            break;
        case 6:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_shelter_b3_elevator_hall_8017DC78(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b3_elevator_hall_8017DC80(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId == 0x29) {
        req.field_0 = 1;
        req.field_4 = 1;
        req.field_8 = 0x542A0005;
        req.field_C = 0x542A0003;
        req.flagId  = 0xA7;
        req.itemId  = 0;
        return func_shelter_b3_elevator_hall_8017D62C(&req, out);
    }
    if (in->areaId != 0x1A) {
        return 1;
    }
    if (in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (GameFlag_GetNibble(0xBA) == 0) {
            Gp_RunCapCmd1(2);
            GameFlag_SetNibble(0xBA, 1);
        }
        Task_SpawnFromTable(D_shelter_b3_elevator_hall_80182A68, 0, 0x542A0001, 0);
    }
    return 0;
}

s32 func_shelter_b3_elevator_hall_8017DD88(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b3_elevator_hall_8017DD90(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b3_elevator_hall_8017DD98(Task* task, s32 msgId, s32 arg2, TaskMessageArg arg3)
{
    if (arg2 == 1) {
        SndEvt_EnqueueType6(0x542A0000 | 1, 0, 0);
    }
    return 0;
}

/// First state of the room's message-driven task: points the task at the
/// room's message table, publishes it in pointer slot 7 and advances.
static void func_shelter_b3_elevator_hall_8017DDCC(Task* task)
{
    task->msgTable = D_shelter_b3_elevator_hall_80182A38;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// The message-driven task's idle state.
static void func_shelter_b3_elevator_hall_8017DE10(Task* task)
{
}

/// Runs the message-driven task's current state through a stack copy of its
/// state table.
void func_shelter_b3_elevator_hall_8017DE18(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_elevator_hall_8017D5F0;
    sp.funcs[task->state](task);
}

void func_shelter_b3_elevator_hall_8017DE70(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x6024D;
        D_80115744  = 0x60259;
        D_8011573C  = 0x60264;
        D_80115720  = 0x60270;
        D_80115734  = 0x60223;
        D_80115730  = 0x6022E;
        D_80115754  = 0x60239;
        arg0->state = 1;
    }
    switch ((u8)Gp_GetViewIndex()) {
        case 3: {
            SVECTOR* p = D_shelter_b3_elevator_hall_80182A74;
            func_shelter_b3_elevator_hall_8017DFB0(&p[0], 0x180, 0x222);
            func_shelter_b3_elevator_hall_8017DFB0(&p[2], 0x180, 0x222);
            func_shelter_b3_elevator_hall_8017DFB0(&p[4], 0x180, 0x222);
            func_shelter_b3_elevator_hall_8017DFB0(&p[6], 0x180, 0x222);
        } break;
        case 4: {
            SVECTOR* p = D_shelter_b3_elevator_hall_80182AB4;
            func_shelter_b3_elevator_hall_8017DFB0(&p[0], 0x180, 0x222);
            func_shelter_b3_elevator_hall_8017DFB0(&p[2], 0x180, 0x222);
            func_shelter_b3_elevator_hall_8017DFB0(&p[4], 0x180, 0x222);
            func_shelter_b3_elevator_hall_8017DFB0(&p[6], 0x180, 0x222);
        } break;
        case 6: {
            SVECTOR* p = D_shelter_b3_elevator_hall_80182AF4;
            func_shelter_b3_elevator_hall_8017DFB0(&p[0], 0x180, 0x222);
            func_shelter_b3_elevator_hall_8017DFB0(&p[2], 0x180, 0x222);
            func_shelter_b3_elevator_hall_8017DFB0(&p[4], 0x180, 0x222);
            func_shelter_b3_elevator_hall_8017DFB0(&p[6], 0x180, 0x222);
        } break;
    }
}

/// Draws a gouraud band between the two points `arg0[0]` and `arg0[1]`,
/// projected through `gGfxViewCoord.workm`: a half-disc fan at each end and a
/// quad strip joining them, three `POLY_G4`s per 0x400 step of the screen
/// angle between the two centres. `arg1` scales the radii by depth. The lit
/// vertices take the colour packed 4 bits per channel in `arg2`, with the
/// frame counter's low bit blended in so the band flickers.
static void func_shelter_b3_elevator_hall_8017DFB0(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      conn;
    u8                       r;
    u8                       g;
    u8                       b;

    p1       = arg0 + 1;
    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (OverlayPointPairScratch*)(head - 0x1C);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)ang;
            blend     = ((u8)ds->animFrame & 1) * 8;
            packed    = arg2 << 16;
            tr        = (packed >> 20) & 0xF0;
            tg        = (packed >> 16) & 0xF0;
            r         = blend | tr;
            g         = blend | tg;
            b         = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    t3             = ang + 0x800;
                    prim           = gGpuPrimCursor;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

#include "../../shared/room_visual_effects.inc.c"

void func_shelter_b3_elevator_hall_8017E7F4(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_shelter_b3_elevator_hall_8017F53C(Task* arg0)
{
    RoomFx_HaloTask(arg0);
}

void func_shelter_b3_elevator_hall_8017F8D4(Task* arg0)
{
    RoomFx_OrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b3_elevator_hall_80180CE4(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}
