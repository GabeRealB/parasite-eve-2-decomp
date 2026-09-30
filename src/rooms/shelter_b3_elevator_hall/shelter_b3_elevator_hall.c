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

// Preserve the nonzero halfword after the three effect records.
// Its role is unresolved; it may be retained exporter padding.
typedef struct {
    RoomHaloShade entries[3];
    u16           retained;
} ShelterB3ElevatorHallHaloStorage;
STATIC_ASSERT_SIZEOF(ShelterB3ElevatorHallHaloStorage, 20);
extern ShelterB3ElevatorHallHaloStorage D_shelter_b3_elevator_hall_80182B34;

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
static void func_shelter_b3_elevator_hall_8017EAC0(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3);
static void func_shelter_b3_elevator_hall_8017ED84(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b3_elevator_hall_8017FA80(GfxCoord* coord, s16 size);
static void func_shelter_b3_elevator_hall_8017FFAC(GfxCoord* arg0, s32 arg1);
static void func_shelter_b3_elevator_hall_80180324(GfxCoord* arg0, s16 arg1, u8* arg2);

void func_shelter_b3_elevator_hall_8017D790(Task*);
void func_shelter_b3_elevator_hall_8017D900(Task*);
void func_shelter_b3_elevator_hall_8017DAF0(Task*);
s32  func_shelter_b3_elevator_hall_8017DC78(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b3_elevator_hall_8017DC80(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b3_elevator_hall_8017DD88(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b3_elevator_hall_8017DD90(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b3_elevator_hall_8017DD98(Task*, s32, s32, GpMessageArg);

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

ShelterB3ElevatorHallHaloStorage D_shelter_b3_elevator_hall_80182B34 = { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 1685 };

static s32  func_shelter_b3_elevator_hall_8017D62C(RoomEventReq* req, RoomEventMsg* msg);
static void func_shelter_b3_elevator_hall_8017F1A8(GfxCoord* arg0, s16 arg1, u8* rgb);

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

s32 func_shelter_b3_elevator_hall_8017DC78(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
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

s32 func_shelter_b3_elevator_hall_8017DD88(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b3_elevator_hall_8017DD90(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b3_elevator_hall_8017DD98(Task* task, s32 msgId, s32 arg2, GpMessageArg arg3)
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
    SCRATCH_POP_BYTES(0x1C);
}

/// A drifting mote: moves the effect's coordinate vertically by the speed
/// packed in the spawn argument and draws it every other tick. A mote whose
/// spawn argument has either low mode bit set starts at full brightness and
/// holds it, moving the other way when bit 1 is set; otherwise it starts dim,
/// rises with a random extra speed and brightens. It fades over the last
/// ticks of its lifetime and releases its work block when dark, or when the
/// room's event state reaches 4.
void func_shelter_b3_elevator_hall_8017E7F4(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    s32        lifetime;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                if (task->spawnArg1.value & 3) {
                    work->scale   = 0x80;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = ((RoomMoteArg*)&task->spawnArg1.value)->speed;
                    work->move.vz = 0;
                    if (task->spawnArg1.value & 2) {
                        work->move.vy = -work->move.vy;
                    }
                    task->state = 2;
                } else {
                    work->scale   = 0x20;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = -((RoomMoteArg*)&task->spawnArg1.value)->speed - (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F);
                    work->move.vz = 0;
                    task->state   = (task->spawnArg1.value & 1) + 1;
                }
                break;
            case 1:
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_shelter_b3_elevator_hall_8017EAC0(coord, work->index, work->angle | 0x1000, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    } else if (work->scale < 0x80) {
                        work->scale += 0x20;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
            case 2:
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_shelter_b3_elevator_hall_8017EAC0(coord, work->index, work->angle, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws one mote: projects the coordinate's world position through
/// `GsWSMATRIX` and, unless the GTE flags the projection, queues one
/// semi-transparent textured square centred on it. `arg1`'s low two bits and
/// `arg2`'s top nibble pick the 24-texel texture cell, `arg2`'s low twelve
/// bits are the half-extent (scaled by 23 / (otz + 1)), `arg3`'s low byte is
/// the grey level and its top nibble picks the palette.
static void func_shelter_b3_elevator_hall_8017EAC0(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    GpRingScratch* block;
    POLY_FT4*      prim;
    DisplayState*  ds;
    u16            row;
    u16            pal;
    s32            u0;
    s32            u1;
    s16            xy;

    row           = arg2 >> 12;
    arg2         &= 0xFFF;
    pal           = arg3 >> 12;
    arg3         &= 0xFF;
    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        setRGB0(prim, arg3, arg3, arg3);
        if (pal != 0) {
            prim->clut = getClut(pal * 16 + 0xF0, 0x10B);
        } else {
            prim->clut = getClut(0xB0, 0x10B);
        }
        u0 = row * 0x60 + (arg1 & 3) * 24;
        u1 = u0 + 0x17;
        setUV4(prim, u0, 0, u1, 0, u0, 0x17, u1, 0x17);
        block->step = arg2 * 23 / block->otz;
        xy          = block->sx - block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = block->sx + block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = block->sy - block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = block->sy + block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpRingScratch);
}

/// Draws a gouraud ring of sixteen `POLY_G4` segments around the projected
/// origin of `arg0`'s world matrix, when it projects. The ring runs between
/// the radii `arg1` and `arg1 + arg2`, both scaled by depth; its edge at
/// `arg1 + arg2` takes the colour `rgb` and its edge at `arg1` is black.
static void func_shelter_b3_elevator_hall_8017ED84(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   next;
    s32                   inner;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    inner         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = ((s16)arg1 * 64) / block->otz;
        block->rInner = ((s16)inner * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(next)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomBillboardScratch);
}

/// Draws a gouraud disc of eight `POLY_G4` wedges around the projected origin
/// of `arg0`'s world matrix, when it projects: radius `arg1` scaled by depth,
/// the colour `rgb` at the centre fading to black at the rim.
static void func_shelter_b3_elevator_hall_8017F1A8(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// An expanding halo. State 0 anchors the effect's frame to its parent at the
/// spawn position and derives the per-tick step from the spawn argument's
/// duration; state 1 ramps the level up while drawing a growing disc, a
/// half-bright wider disc on odd ticks and a shrinking ring; state 2 fades the
/// level out through a two-ring flare before the work block is released.
void func_shelter_b3_elevator_hall_8017F53C(Task* arg0)
{
    u8          rgb[3];
    GpEffWork*  mem;
    GfxCoord*   coord;
    GpMtxWords* rot;
    s16         flag;
    s32         shift;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        switch (arg0->state) {
            case 0:
                rot                 = (GpMtxWords*)&coord->coord;
                coord->parent       = mem->parent;
                rot->m00_m01        = 0x1000;
                rot->m02_m10        = 0;
                rot->m11_m12        = 0x1000;
                rot->m20_m21        = 0;
                rot->m22            = 0x1000;
                coord->coord.t[0]   = mem->pos.vx;
                coord->coord.t[1]   = mem->pos.vy;
                coord->coord.t[2]   = mem->pos.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                shift                 = arg0->spawnArg1.halves.high;
                mem->index            = shift;
                arg0->spawnArg1.value = arg0->spawnArg1.halves.low;
                arg0->state           = 1;
                mem->step             = 0x100 / arg0->spawnArg1.value;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                mem->scale            += mem->step;
                mem->angle            += mem->step;
                arg0->spawnArg1.value -= 1;
                rgb[0]                 = mem->scale >> D_shelter_b3_elevator_hall_80182B34.entries[mem->index].r;
                rgb[1]                 = mem->scale >> D_shelter_b3_elevator_hall_80182B34.entries[mem->index].g;
                rgb[2]                 = mem->scale >> D_shelter_b3_elevator_hall_80182B34.entries[mem->index].b;
                func_shelter_b3_elevator_hall_8017F1A8(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_shelter_b3_elevator_hall_8017F1A8(coord, (s16)(mem->angle + 0x100), rgb);
                }
                func_shelter_b3_elevator_hall_8017ED84(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1.value == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_shelter_b3_elevator_hall_80182B34.entries[mem->index].r;
                    rgb[1] = mem->scale >> D_shelter_b3_elevator_hall_80182B34.entries[mem->index].g;
                    rgb[2] = mem->scale >> D_shelter_b3_elevator_hall_80182B34.entries[mem->index].b;
                    func_shelter_b3_elevator_hall_80180324(coord, (u16)mem->angle * 4, rgb);
                    mem->scale -= 0x10;
                    mem->angle += 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// A burst: each tick it draws a disc growing with the effect's angle and the
/// glow at that size, and while its echo level lasts a widening ring fading
/// out around them. The work block is released once the disc's level runs
/// down, or when the room's event state reaches 4.
void func_shelter_b3_elevator_hall_8017F8D4(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
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
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_shelter_b3_elevator_hall_8017F1A8(coord, (s16)(step * 2), rgb);
        func_shelter_b3_elevator_hall_8017FA80(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_shelter_b3_elevator_hall_8017ED84(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
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

/// Draws a glow at the coordinate: two camera-facing textured squares, an
/// inner one of half-extent `size` and an outer one of `size * 3 / 2`
/// (each scaled by 0x37 / otz), plus a flat quad on the ground beneath it.
/// It also points the `Gp_RoomCoords[2]` light at the
/// coordinate with a randomly flickering intensity. Nothing is drawn when the
/// GTE flags the projection.
static void func_shelter_b3_elevator_hall_8017FA80(GfxCoord* coord, s16 size)
{
    GfxCoord       ground;
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

    slot                                  = &Gp_RoomCoords[2];
    slot->framesLeft                      = 2;
    light                                 = &slot->light;
    light->inner                          = 0x300;
    light->outer                          = 0x3000;
    random                                = (Gp_LcgState * 5) + 0x71357911;
    intensity                             = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r                         = intensity;
    shifted                               = intensity << 0x10;
    light->head.g                         = shifted >> 0x11;
    light->head.b                         = shifted >> 0x12;
    light->head.u.at.local.t[0]           = coord->coord.t[0];
    light->head.u.at.local.t[1]           = coord->coord.t[1];
    light->head.u.at.local.t[2]           = coord->coord.t[2];
    slot->light.head.u.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_LcgState                           = random;
    block                                 = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx                         = coord->workm.t[0];
    block->vec.vy                         = coord->workm.t[1];
    block->vec.vz                         = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code |= 1;
        }
        block->step = size * 0x37 / block->otz;
        left        = block->sx - block->step;
        prim->x2    = left;
        prim->x0    = left;
        right       = block->sx + block->step;
        prim->x3    = right;
        prim->x1    = right;
        top         = block->sy - block->step;
        prim->y1    = top;
        prim->y0    = top;
        bottom      = block->sy + block->step;
        prim->y3    = bottom;
        prim->y2    = bottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (((gDisplayState.animFrame & 1) * 0x10 + 0x120) >> 4) | 0x4300;
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)(size * 3 / 2);
        block->step = outerSize * 0x37 / block->otz;
        outerLeft   = block->sx - block->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = block->sx + block->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = block->sy - block->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = block->sy + block->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_shelter_b3_elevator_hall_8017FFAC(&ground, outerSize);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_shelter_b3_elevator_hall_8017FFAC(GfxCoord* arg0, s32 arg1)
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

    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        v     = &block->vec[i];
        tbl   = &D_80111E38[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += arg0->workm.t[0];
        v->vy += arg0->workm.t[1];
        v->vz += arg0->workm.t[2];
    }

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
        prim           = gGpuPrimCursor;
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
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Draws a flare around the projected origin of `arg0`'s world matrix, when
/// it projects: a disc of radius `arg1` scaled by depth at half the colour
/// `arg2`, a full-colour disc of half that radius over it, and a ring of
/// half-colour spikes built from a second, eight-times smaller radius. Every
/// wedge is `POLY_G4`, lit at the centre and black at the rim.
static void func_shelter_b3_elevator_hall_80180324(GfxCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomBillboardScratch);
}

/// Each tick advances the effect's angle by a random 0x200-0x3FF, sets its
/// velocity to 3/16 of that direction's unit vector in X and Z with a Y that
/// grows with age, and spawns the child effect `D_80115728` at the task's
/// coordinate frame. Releases the work block after 0x15 ticks, or when the
/// room's event state reaches 4.
void func_shelter_b3_elevator_hall_80180CE4(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        ang;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        if (mem->age >= 0x15) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        }
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        ang          = mem->scale + ((((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200);
        mem->scale   = ang;
        mem->move.vx = (u32)(rcos(ang) * 3) >> 4;
        mem->move.vy = -mem->age * 128;
        mem->move.vz = (u32)(rsin(mem->scale) * 3) >> 4;
        Gp_SpawnEff(D_80115728, coord, 0x30080201, &mem->move);
    }
}
