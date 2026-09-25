#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"

extern u8         D_8007216D;
extern TaskDesc   D_dryfield_underpass_8017E818[];
extern GpMsgEntry D_dryfield_underpass_8017E830[];
extern s32        D_dryfield_underpass_8017E89C;
extern s32        D_dryfield_underpass_8017E8D8;
extern SVECTOR    D_dryfield_underpass_8017EAD0[8];
extern s16        D_dryfield_underpass_8017EB10[8];

/// Switch task the room's 0x13F0 handler spawns: plays cap command `spawnArg2`,
/// waits for it to finish, and once its event key reaches 0xA toggles game
/// nibble `spawnArg1`. When that nibble is 0x51 it also picks the room variant
/// to load next from nibbles 0xC9, 0x53 and 0x51 and writes it to both the
/// session and the save data. The last state flags the view dirty when the
/// chosen room is 5 or above, then kills the task.
void func_dryfield_underpass_8017D5D0(Task* task)
{
    GpSaveLoc    src;
    GpSaveLoc    dst;
    GpSaveLoc*   s;
    GpSaveLoc*   d;
    GameSession* session;
    s32          flag;
    s32          state;
    s32          arg;
    u8           room;

    flag  = task->spawnArg1;
    state = task->state;
    arg   = (s32)task->spawnArg2;
    switch (state) {
        case 0:
            Gp_RunCapCmd1(arg);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 2:
            if (Gp_GetCapEventKey() >= 0xA) {
                GameFlag_SetNibble(flag, GameFlag_GetNibble(flag) == 0);
                if (flag == 0x51) {
                    d           = &dst;
                    s           = &src;
                    *(u16*)&src = 0x26;
                    src.field_5 = 0;
                    if (s->field_5 == 0) {
                        if (GameFlag_GetNibble(0xC9) != 0) {
                            if (GameFlag_GetNibble(0x53) != 0) {
                                d->field_3 = 2;
                            } else {
                                d->field_3 = 1;
                            }
                            if (GameFlag_GetNibble(0x51) == 0) {
                                dst.field_3 = dst.field_3 + 2;
                            }
                        } else {
                            if (GameFlag_GetNibble(0x51) != 0) {
                                d->field_3 = 5;
                            } else {
                                d->field_3 = 6;
                            }
                        }
                    }
                    session                  = gGameSession;
                    room                     = dst.field_3;
                    session->at4.loc.room    = room;
                    Mc_SaveData.at4.loc.room = room;
                }
            }
            task->state = task->state + 1;
            return;
        case 3:
            if (gGameSession->at4.loc.room >= 5) {
                gGameSession->viewDirty = 1;
            }
            taskKill(task);
            return;
    }
}

/// Handler for message 0x13EE: copies the incoming record onto the outgoing one
/// and, unless the query is report-only (`field_5` set), answers record id 0x20
/// with 1 or 2 from nibble 0x51, raised by 2 while nibble 0x53 is set, and
/// record id 0x22 with 1 or 2 from nibble 0x52. Always returns 1.
s32 func_dryfield_underpass_8017D788(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 val;

    *out = *in;
    USE_REG(out);
    USE_REG(out);
    if (in->msgId == 0x20 && in->field_5 == 0) {
        val = GameFlag_GetNibble(0x51);
        if (val == 0) {
            val = 2;
        } else {
            val = 1;
        }
        out->field_3 = val;
        if (GameFlag_GetNibble(0x53) != 0) {
            out->field_3 = (u8)out->field_3 + 2;
        }
    }
    if (in->msgId == 0x22 && in->field_5 == 0) {
        val = GameFlag_GetNibble(0x52);
        if (val == 0) {
            val = 2;
        } else {
            val = 1;
        }
        out->field_3 = val;
    }
    return 1;
}

/// Handler for message 0x13F0: for `arg2` 1 or 2, spawns the room's switch
/// task `func_dryfield_underpass_8017D5D0` from the task table, toggling
/// nibble 0x51 with cap command 1 or nibble 0x52 with cap command 2. Any other
/// value spawns nothing. Always returns 0.
s32 func_dryfield_underpass_8017D868(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 1:
            Task_SpawnFromTable(D_dryfield_underpass_8017E818, 0, 0x51, 1);
            break;
        case 2:
            Task_SpawnFromTable(D_dryfield_underpass_8017E818, 0, 0x52, 2);
            break;
    }
    return 0;
}

/// Handler for message 0x13F2: when `arg2` is 2, queues stage sound 0x52260002.
/// Always returns 0.
s32 func_dryfield_underpass_8017D8CC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        Gp_EnqueueStageSnd6(0x52260000 | 2, 0, 0);
    }
    return 0;
}

/// Handler for message 0x13F1: does nothing and returns 0.
s32 func_dryfield_underpass_8017D900(void)
{
    return 0;
}

/// Handler for message 0x13EF: the first time record `field_2` 1 arrives while
/// the session's place is 1 and nibble 0xC9 is clear, sets that nibble and
/// runs the room's script `D_dryfield_underpass_8017E8D8`. Always returns 0.
s32 func_dryfield_underpass_8017D908(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp_v1;

    temp_v1 = in->field_2;
    if ((temp_v1 == 1) && (gGameSession->at4.loc.place == temp_v1) && (GameFlag_GetNibble(0xC9) == 0)) {
        GameFlag_SetNibble(0xC9, 1);
        func_800E8614((s32)&D_dryfield_underpass_8017E8D8, 0);
    }
    return 0;
}

/// First state of the room task: parks the room's message table in
/// `Task::msgTable` and publishes the task in pointer slot 7. While the
/// session's place is 1 and nibble 0xC9 is clear it also sends message 0x7DA,
/// with `D_dryfield_underpass_8017E89C`, to the task in slot 4. Then advances.
void func_dryfield_underpass_8017D970(Task* arg0)
{
    arg0->msgTable = D_dryfield_underpass_8017E830;
    Game_SetPtrSlot(arg0, 7);
    if ((gGameSession->at4.loc.place == 1) && (GameFlag_GetNibble(0xC9) == 0)) {
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_dryfield_underpass_8017E89C, 0x7DB);
    }
    arg0->state = arg0->state + 1;
}

/// Second state of the room task: idles.
void func_dryfield_underpass_8017DA00(Task* task)
{
}

/// Picks the room variant to load next from nibbles 0xC9, 0x53 and 0x51, the
/// same choice the switch task `func_dryfield_underpass_8017D5D0` makes when it
/// toggles nibble 0x51, and writes it to the session's room and to
/// `D_8007216D`, then flags the room objects dirty. Reached from the room's
/// script data.
void func_dryfield_underpass_8017DA08(void)
{
    RoomEventMsg  src;
    RoomEventMsg  dst;
    RoomEventMsg* s;
    RoomEventMsg* d;
    GameSession*  session;
    u8            room;

    d           = &dst;
    s           = &src;
    *(u16*)&src = 0x26;
    src.field_5 = 0;
    if (s->field_5 == 0) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) != 0) {
                d->field_3 = 2;
            } else {
                d->field_3 = 1;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                dst.field_3 = dst.field_3 + 2;
            }
        } else {
            if (GameFlag_GetNibble(0x51) != 0) {
                d->field_3 = 5;
            } else {
                d->field_3 = 6;
            }
        }
    }
    session                     = gGameSession;
    room                        = dst.field_3;
    session->at4.loc.room       = room;
    D_8007216D                  = room;
    gGameSession->roomObjsDirty = 1;
}

/// State handlers of the room task `func_dryfield_underpass_8017DAC8`, indexed
/// by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
const TaskFuncTable3 D_dryfield_underpass_8017D5C4 = {
    { func_dryfield_underpass_8017D970, func_dryfield_underpass_8017DA00, taskKill },
};

/// Room task: runs the state handler `D_dryfield_underpass_8017D5C4` names for
/// `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_underpass_8017DAC8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_underpass_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws one glow sprite at `arg1`, a point in the local space of the model
/// coordinate `arg0`: the point is rotated by the coordinate's `workm` and
/// offset by its translation, then projected through `GsWSMATRIX` in 0x14
/// bytes of scratch, released on exit. Anything nearer than `otz` 0x11 is
/// dropped. The sprite is a semi-transparent `POLY_FT4` on tpage 0x2B; `arg2`
/// picks the 40-texel-wide frame (u `arg2 * 40` onwards) and the clut
/// `(arg2 & 0x3F) | 0x4380`, and `arg3` is a signed half-extent, so the quad
/// reaches `(s16)arg3 * 39 / otz` from the projected centre. The grey level
/// alternates between 0x20 and 0x30 with `animFrame`.
void func_dryfield_underpass_8017DB20(GpCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw35Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                su;
    s32                sv;
    s32                u0;
    s32                u1;
    s32                flip;
    s32                rgb;
    s16                xy;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x14;
    block    = (RoomDraw35Scratch*)(head - 0x14);

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomDraw35Scratch*)(head - 0x14))->vec);
    block->vec.vx = (u16)block->vec.vx + *(u16*)&arg0->workm.t[0];
    block->vec.vy = (u16)block->vec.vy + *(u16*)&arg0->workm.t[1];
    block->vec.vz = (u16)block->vec.vz + *(u16*)&arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw35Scratch*)(head - 0x14))->vec);
    gte_rtps();

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw35Scratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw35Scratch*)(head - 0x14))->otz >= 0x11) {
        ds          = &gDisplayState;
        flip        = (u8)ds->animFrame;
        su          = (s16)arg2;
        sv          = (s16)arg3;
        prim->tpage = 0x2B;
        prim->clut  = (su & 0x3F) | 0x4380;
        u0          = su * 0x28;
        u1          = u0 + 0x27;
        prim->u1    = u1;
        prim->u3    = u1;
        prim->u0    = u0;
        prim->u2    = u0;
        prim->v0    = 0;
        prim->v1    = 0;
        prim->v2    = 0x27;
        prim->v3    = 0x27;
        rgb         = (flip & 1) << 4;
        rgb        += 0x20;
        setSemiTrans(prim, 1);
        prim->r0         = rgb;
        prim->g0         = rgb;
        prim->b0         = rgb;
        block->halfWidth = (sv * 0x27) / block->otz;
        xy               = block->sx - *(u16*)&block->halfWidth;
        prim->x2         = xy;
        prim->x0         = xy;
        xy               = block->sx + *(u16*)&block->halfWidth;
        prim->x3         = xy;
        prim->x1         = xy;
        xy               = block->sy - *(u16*)&block->halfWidth;
        prim->y1         = xy;
        prim->y0         = xy;
        xy               = block->sy + *(u16*)&block->halfWidth;
        prim->y3         = xy;
        prim->y2         = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x14);
}

/// Per-frame effect on a model task: draws the glow sprites the current visit
/// lights, one per point in `D_...EAD0` (in the model's local space) whose
/// `D_...EB10` bitmask contains the visit's bit (`gGameSession->at4.loc.view`).
/// The whole effect is skipped unless nibble 0x53 is clear.
void func_dryfield_underpass_8017DE30(Task* task)
{
    GpCoord* coord;
    s32      mask;
    s32      i;
    SVECTOR* vec;
    s16*     flags;

    coord = task->extra.tmd->coords;
    mask  = 1 << gGameSession->at4.loc.view;
    if (GameFlag_GetNibble(0x53) == 0) {
        i     = 0;
        vec   = D_dryfield_underpass_8017EAD0;
        flags = D_dryfield_underpass_8017EB10;
        do {
            if (mask & *flags) {
                func_dryfield_underpass_8017DB20(coord, vec, 0, 0x280);
            }
            vec++;
            i++;
            flags++;
        } while (i < 8);
    }
}
