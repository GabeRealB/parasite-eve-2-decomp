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

extern s32 D_80070F70;
extern s16 D_80071076;
extern u8  D_80115690;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Spawn descriptor of the exit gate's transition task.
extern TaskDesc D_shelter_b2_operating_room_80180904;

/// Descriptor of the room's own event task, which the message handler spawns.
extern TaskDesc D_shelter_b2_operating_room_80180910;

/// The room's message table, which the cap scripts index.
extern GpMsgEntry D_shelter_b2_operating_room_8018091C[];

/// Point pairs and points the view task draws its glows at, per view.
extern SVECTOR D_shelter_b2_operating_room_801809BC[];
extern SVECTOR D_shelter_b2_operating_room_80180ABC[];
extern SVECTOR D_shelter_b2_operating_room_80180ADC[];
extern SVECTOR D_shelter_b2_operating_room_80180B44[];
extern SVECTOR D_shelter_b2_operating_room_80180B5C[];
extern SVECTOR D_shelter_b2_operating_room_80180B6C[];

/// Per-colour channel shifts for the halo task, indexed by the colour its
/// spawn argument selects.
extern s16 D_shelter_b2_operating_room_80180BBC[][3];

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern GpFadeWork D_shelter_b2_operating_room_80184214;

/// The message and request of the exit the gate last accepted, latched for
/// the transition task, and the flag saying the gate spawned it.
extern RoomEventMsg D_shelter_b2_operating_room_8018421C;
extern u8           D_shelter_b2_operating_room_80184224;
extern RoomEventReq D_shelter_b2_operating_room_80184238;

/// The message and event the message handler latched for the room's event
/// task, and the flag saying the handler spawned it.
extern RoomEventMsg     D_shelter_b2_operating_room_8018422C;
extern u8               D_shelter_b2_operating_room_80184234;
extern RoomLatchedEvent D_shelter_b2_operating_room_80184258;

void func_shelter_b2_operating_room_8017E118(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b2_operating_room_8017E95C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b2_operating_room_8017F478(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b2_operating_room_8017F6FC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b2_operating_room_8017FB20(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_b2_operating_room_80180060(GsCOORDINATE2* coord, s16 size);
void func_shelter_b2_operating_room_8018058C(GsCOORDINATE2* arg0, s32 arg1);

/// Handles a request to leave through a flag-gated exit. When the flag named
/// by `req->flagId` (negated: must be clear) is already in the wanted state,
/// returns 1. Otherwise, if the item `req->itemId` has been collected (or none
/// is needed), sets the flag, records `msg` and `req`, spawns the transition
/// task and returns 2; if the item is missing, runs cap command `req->field_4`
/// and returns 0. The spawn and the cap command happen only when
/// `msg->field_5` is 0.
s32 func_shelter_b2_operating_room_8017D628(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                 = req->flagId;
    D_shelter_b2_operating_room_80184224 = 0;
    neg                                  = flag < 0;
    got                                  = (s16)flag;
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
            if (msg->field_5 == 0) {
                D_shelter_b2_operating_room_8018421C = *msg;
                D_shelter_b2_operating_room_80184238 = *req;
                id                                   = req->flagId;
                mode                                 = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b2_operating_room_80180904, 0, 0, 0);
                D_shelter_b2_operating_room_80184224 = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// Transition task for an exit the gate accepted: runs the latched request's
/// cap command `field_0`, plays its sounds `field_8` and then `field_C` (each
/// only when non-zero), waiting for each voice to finish, then records the
/// latched message's area, warp and room in the save data's location, spawns
/// task 0x11 of bank 0 and kills itself.
void func_shelter_b2_operating_room_8017D78C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b2_operating_room_80184238.field_0);
            if (D_shelter_b2_operating_room_80184238.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_operating_room_80184238.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b2_operating_room_80184238.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b2_operating_room_80184238.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_operating_room_80184238.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b2_operating_room_80184238.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_operating_room_8018421C.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_operating_room_8018421C.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b2_operating_room_8018421C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room's own event task, spawned by its message handler. State 0 runs
/// the latched event's CAP command; state 1 waits for it to finish and, when
/// the event asks for it, starts helper task 0x31; states 2 and 3 play the
/// event's stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void func_shelter_b2_operating_room_8017D8FC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b2_operating_room_80184258.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b2_operating_room_80184258.fade != 0) {
                    D_shelter_b2_operating_room_80184214.field_0 = 0;
                    D_shelter_b2_operating_room_80184214.field_1 = 0;
                    D_shelter_b2_operating_room_80184214.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b2_operating_room_80184214);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b2_operating_room_80184258.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b2_operating_room_80184258.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b2_operating_room_80184258.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_operating_room_8018422C.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_operating_room_8018422C.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_b2_operating_room_8018422C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _operatingRoomStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b2_operating_room_80184234 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b2_operating_room_8018422C = *dst;
            D_shelter_b2_operating_room_80184258 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b2_operating_room_80180910, 0, 0, 0);
            D_shelter_b2_operating_room_80184234 = 1;
        }
        return 2;
    }
    return 1;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Message 0x1E goes through the exit gate
/// `func_shelter_b2_operating_room_8017D628` on flag 0xA8. Message 0x1C, while nibble 0xAA is clear, answers 0 and - unless
/// `in->field_5` asks for a dry run - passes `in->field_6` to `Gp_SetNibbleIf`
/// and runs cap command 3; once the nibble is set it starts the room event on
/// flag 0x13A instead. Message 0x1F starts the event on flag 0x13B; any other
/// message answers 1.
s32 func_shelter_b2_operating_room_8017DA94(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq     req;
    RoomLatchedEvent event;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x1E) {
        req.field_0 = 2;
        req.field_4 = 1;
        req.field_8 = 0x541D0007;
        req.field_C = 0x541D0003;
        req.flagId  = 0xA8;
        req.itemId  = 0;
        return func_shelter_b2_operating_room_8017D628(&req, out);
    }
    if (in->msgId == 0x1C && GameFlag_GetNibble(0xAA) == 0) {
        if (in->field_5 == 0) {
            Gp_SetNibbleIf(in->field_6, 2);
            Gp_RunCapCmd1(3);
        }
        return 0;
    }
    if (in->msgId == 0x1C) {
        event.capCmd   = 0xE;
        event.stageSnd = 0x541D0001;
        event.flagId   = 0x13A;
        event.fade     = 0;
        return _operatingRoomStartEvent(out, &event);
    }
    if (in->msgId == 0x1F) {
        event.capCmd   = 0xD;
        event.stageSnd = 0x541D0005;
        event.flagId   = 0x13B;
        event.fade     = 0;
        return _operatingRoomStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_b2_operating_room_8017DC9C(void)
{
    return 0;
}

s32 func_shelter_b2_operating_room_8017DCA4(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 4:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) == 0 ? 4 : 0x10, 0);
            break;
        case 5:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) != 0 ? 0xF : 5, 0);
            break;
    }
    return 0;
}

s32 func_shelter_b2_operating_room_8017DD0C(void)
{
    return 0;
}

/// Installs `D_shelter_b2_operating_room_8018091C` as the task's message
/// table, registers the task in pointer slot 7 and steps it on one state.
void func_shelter_b2_operating_room_8017DD14(Task* task)
{
    task->msgTable = D_shelter_b2_operating_room_8018091C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// The room task's idle state, which does nothing.
void func_shelter_b2_operating_room_8017DD58(Task* task)
{
}

/// The room task's three states, dispatched by
/// `func_shelter_b2_operating_room_8017DD60`: install the message table, idle,
/// end.
const TaskFuncTable3 D_shelter_b2_operating_room_8017D5F0 = {
    { func_shelter_b2_operating_room_8017DD14, func_shelter_b2_operating_room_8017DD58, taskKill }
};

/// Runs the handler for the task's current state, from a local copy of
/// `D_shelter_b2_operating_room_8017D5F0`.
void func_shelter_b2_operating_room_8017DD60(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_operating_room_8017D5F0;
    sp.funcs[task->state](task);
}

/// Per-frame view task. On its first frame it stores three ids in the gameplay
/// words `D_80115734`, `D_80115730` (the effect the halo task spawns) and
/// `D_80115754`; every frame it
/// draws the glows of the current view (views 2 to 7) at that view's points,
/// as capsules through `func_shelter_b2_operating_room_8017E118` and discs
/// through `func_shelter_b2_operating_room_8017E95C`.
void func_shelter_b2_operating_room_8017DDB8(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115734  = 0x60222;
        D_80115730  = 0x6022D;
        D_80115754  = 0x60238;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B6C[0], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B6C[4], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B6C[8], 0x100, 0x444);
            break;
        case 3:
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B5C[0], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B5C[8], 0x100, 0x444);
            break;
        case 4:
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[0], 0x380, 0x444);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-48], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-44], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-41], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-38], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-37], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-36], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-35], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-34], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-31], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-30], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-27], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-26], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-23], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-22], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-21], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B44[-7], 0x100, 0x400);
            break;
        case 5:
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[0], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[6], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[8], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[9], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[10], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[13], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[14], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[15], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[16], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[17], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[20], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[21], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[24], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[25], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[29], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[30], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[31], 0x200, 0x400);
            break;
        case 6:
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ABC[0], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ABC[2], 0x100, 0x444);
            break;
        case 7:
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ADC[0], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ADC[2], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ADC[4], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ADC[8], 0x100, 0x444);
            break;
    }
}

/// Draws a glowing capsule between the points `arg0[0]` and `arg0[1]`,
/// projected through `Gfx_ViewWorldMtx`; nothing is drawn unless both project.
/// Each end is a half-disc of screen radius `arg1 * 64 / otz` and the two are
/// joined by a band, built from gouraud quads lit at the centre line and black
/// at the rim, in two 0x400 steps around the angle between the projected
/// points. `arg2` is the colour as three 4-bit channels (0xRGB), brightened
/// slightly on odd frames.
void func_shelter_b2_operating_room_8017E118(SVECTOR* arg0, s32 arg1, s32 arg2)
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
    s32                      sum;
    u8                       r;
    u8                       g;
    u8                       b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (OverlayPointPairScratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
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
            SCHED_BARRIER();
            ang    = (s16)ang;
            blend  = (*(u8*)&ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
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
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)gGpuPrimCursor;
                    USE_REG(prim);
                    t   = ang - angStart;
                    t <<= 1;
                    TOUCH_REG(t);
                    sum            = angStart + t;
                    t              = sum;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();
                    t3   = ang + 0x800;
                    prim = (POLY_G4*)gGpuPrimCursor;
                    SOFT_BARRIER();
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
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Draws a glowing disc around the point `arg0`, projected through
/// `Gfx_ViewWorldMtx`, unless the projection flags an error: four gouraud
/// wedges lit at the projected centre and black at the rim, of screen radius
/// `arg1 * 64 / otz`. `arg2` is the colour as three 4-bit channels (0xRGB),
/// brightened slightly on odd frames.
void func_shelter_b2_operating_room_8017E95C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    register u8*       tmp asm("v0");
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw13Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1 = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ang  = 0;
        tmp  = (u8*)&gDisplayState;
        SOFT_TOUCH_REG(tmp);
        ds            = (DisplayState*)tmp;
        blend         = (*(u8*)&ds->animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
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
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// Halo effect task attached to a parent coordinate. State 0 places it at the
/// work block's position; states 1 and 2 grow a glowing disc tinted by the
/// channel shifts its spawn argument selects, state 1 also spawning effect
/// `D_80115730` on a random bone of the player every fourth tick and state 2
/// adding a fainter, wider disc on odd ticks. State 3 drifts away while
/// drawing a widening ring and fading, and releases the work block once faded;
/// state 4 releases it at once. While `Gp_State1C->eventState` is non-zero it
/// does nothing but release once that reaches 4.
void func_shelter_b2_operating_room_8017ECFC(Task* arg0)
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
            col[0] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1][2];
            func_shelter_b2_operating_room_8017FB20(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1][2];
            func_shelter_b2_operating_room_8017FB20(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b2_operating_room_8017FB20(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1][2];
            func_shelter_b2_operating_room_8017FB20(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = mem->scale >> 1;
            col[2] = mem->scale >> 2;
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
            func_shelter_b2_operating_room_8017F6FC(coord, (s16)(mem->period + 0x80), 0x100, col);
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

/// Effect task that drifts toward the coordinate in `spawnArg1`. Its first
/// frame turns the world-space offset to that coordinate into the effect's
/// own frame and keeps 0xCC/0x1000 of it as the per-frame step. Every frame
/// after that moves by the step and, on odd ticks, draws the next frame of
/// `func_shelter_b2_operating_room_8017F478`, releasing the effect at tick 20.
/// While `Gp_State1C->eventState` is non-zero it does nothing but release from
/// state 4.
void func_shelter_b2_operating_room_8017F254(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    VECTOR         delta;

    work   = task->spawnArg2;
    coord  = ((TmdObject*)task->extra)->coords;
    target = (GsCOORDINATE2*)task->spawnArg1;
    if (Gp_State1C->eventState == 0) {
        work->age++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->pos.vx = delta.vx;
                work->pos.vy = delta.vy;
                work->pos.vz = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->pos);
                gte_rtv0();
                gte_stsv(&work->pos);
                gte_lddp(0xCC);
                gte_ldsv(&work->pos);
                gte_gpf12();
                gte_stsv(&work->pos);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0] += work->pos.vx;
                coord->coord.t[1] += work->pos.vy;
                coord->coord.t[2] += work->pos.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    func_shelter_b2_operating_room_8017F478(coord, ++work->index, 0x200, 0x80);
                }
                if (work->age >= 20) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    } else if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Draws a sprite at the coordinate's world position: one semi-transparent
/// textured quad (tpage 0x2A, clut 0x42CB) centred on the projected point,
/// unless the projection flags an error. `arg1` picks one of four 24-texel
/// frames from U 0x60, `arg2` is the size (a screen half-extent of
/// `arg2 * 23 / (otz + 1)`) and `arg3` the grey level it is shaded with.
void func_shelter_b2_operating_room_8017F478(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    s32            u0;
    s32            u1;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = *(u16*)&arg0->workm.t[1];
    vz                                      = *(u16*)&arg0->workm.t[2];
    *scratch                                = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
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
        prim->v0    = 0;
        prim->v1    = 0;
        block->step = (t - sarg) / block->otz;
        xy          = *(u16*)&block->sx - *(u16*)&block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = *(u16*)&block->sx + *(u16*)&block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = *(u16*)&block->sy - *(u16*)&block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = *(u16*)&block->sy + *(u16*)&block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// Draws a glowing ring around the coordinate's projected position, unless the
/// projection flags an error: sixteen gouraud quads, black at screen radius
/// `arg1 * 64 / (otz + 1)` and coloured `rgb` at
/// `(arg1 + arg2) * 64 / (otz + 1)`.
void func_shelter_b2_operating_room_8017F6FC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    GpArcScratch*   block;
    POLY_G4*        prim;
    s32             ang;
    register void** scratch asm("a1");
    register s32    saved asm("t1");
    register u8*    head asm("t0");
    register s32    sum asm("a1");
    register s32    otz asm("v0");
    register s32    rOuter asm("a0");
    register s32    rInner asm("v1");
    register u8*    color asm("s4");
    s32             t;
    u16             vz;
    u32             maskLo;
    u32             maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                     = *(u16*)&arg0->workm.t[0];
        ((GpArcScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (GpArcScratch*)tmp;
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
    gte_stsxy(&((GpArcScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpArcScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpArcScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        otz        = block->otz + 1;
        rOuter     = ((s16)saved * 64) / otz;
        rInner     = (s16)sum * 64;
        block->otz = otz;
        SOFT_BARRIER();
        rInner       = rInner / otz;
        ang          = 0;
        block->inner = rOuter;
        block->outer = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = *(u16*)&block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->inner * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->inner * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->inner * rcos(t)) >> 12);
            prim->x2 = *(u16*)&block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = *(u16*)&block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = *(u16*)&block->sx + ((block->outer * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->outer * rcos(t)) >> 12);
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
    SCRATCH_POP_BYTES(0x1C);
}

/// Draws a glowing disc at the coordinate's world position, unless the
/// projection flags an error: eight gouraud wedges coloured `rgb` at the
/// projected centre and black at the rim, of screen radius
/// `arg1 * 64 / (otz + 1)`.
void func_shelter_b2_operating_room_8017FB20(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;
    register s32   ang2 asm("s1");
    u16            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                      = *(u16*)&arg0->workm.t[0];
        ((GpRingScratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (GpRingScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
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
    SCRATCH_POP_BYTES(0x18);
}

/// Burst effect task. Every frame it draws a glowing disc and the glow of
/// `func_shelter_b2_operating_room_80180060` at a growing size. While its echo
/// level lasts it also draws a widening ring that fades out; once the echo is
/// spent the main level runs down, and the work block is released when it
/// does. While `Gp_State1C->eventState` is non-zero it does nothing but
/// release once that reaches 4.
void func_shelter_b2_operating_room_8017FEB4(Task* arg0)
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
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_shelter_b2_operating_room_8017FB20(coord, (s16)(step * 2), rgb);
        func_shelter_b2_operating_room_80180060(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_shelter_b2_operating_room_8017F6FC(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
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
/// (each scaled by 0x37 / otz), plus the flat quad of
/// `func_shelter_b2_operating_room_8018058C` on the ground beneath it. It also
/// points the `Gp_RoomCoords[2]` light at the coordinate with a randomly flickering
/// intensity. Nothing is drawn when the GTE flags the projection.
void func_shelter_b2_operating_room_80180060(GsCOORDINATE2* coord, s16 size)
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
            func_shelter_b2_operating_room_8018058C(&ground, outerSize);
        }
    }
    SCRATCH_POP_BYTES(sizeof(GpRingScratch));
}

/// Draws a flat quad facing the camera around the coordinate's world
/// position: the four unit corners of `D_80111E38` scaled by `arg1`, turned by
/// `Gfx_ViewWorldMtx` and moved to that position, then projected through
/// `GsWSMATRIX`. Unless the projection flags an error it queues one
/// semi-transparent textured quad (tpage 0x28, clut 0x428C) tinted
/// (0x30, 0x20, 0x20), alternating between two 32-texel frames from U 0xC0 on
/// odd and even frames.
void func_shelter_b2_operating_room_8018058C(GsCOORDINATE2* arg0, s32 arg1)
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
    SCRATCH_POP_BYTES(0x38);
}
