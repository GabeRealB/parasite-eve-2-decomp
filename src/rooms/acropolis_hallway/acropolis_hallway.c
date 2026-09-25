#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d830.h"

extern GpMsgEntry D_acropolis_hallway_8017E238[];
extern SVECTOR    D_acropolis_hallway_8017FA4C;

void func_acropolis_hallway_8017D784(Task* task);
void func_acropolis_hallway_8017D7C8(Task* task);

/// State handlers of the room task: set-up, an idle tick and `taskKill`.
const TaskFuncTable3 D_acropolis_hallway_8017D5C4 = {
    { func_acropolis_hallway_8017D784, func_acropolis_hallway_8017D7C8, taskKill },
};

/// Message gate for the hallway's first hotspot: copies the incoming record to
/// the outgoing one, then edits the copy's `field_3` (the answer the caller
/// acts on) according to the message id and the room's progress nibbles.
/// Returning 0 means the message was consumed.
s32 func_acropolis_hallway_8017D5D0(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u16 msgId;

    *out = *in;
    if (in->msgId == 8) {
        if ((GameFlag_GetNibble(9) & 2) && in->field_5 == 0) {
            out->field_3 = 2;
        }
    }
    if (in->msgId == 4 && in->field_2 == 3 && GameFlag_GetNibble(0) < 3) {
        if (in->field_5 == 0) {
            Gp_RunCapCmd1(1);
        }
        return 0;
    }
    if (in->msgId == 8 && GameFlag_GetNibble(0) == 3) {
        return 1;
    }
    msgId = in->msgId;
    if (msgId == 4 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0) >= 3) {
            out->field_3 = msgId;
        }
        if (GameFlag_GetNibble(0) == 2) {
            out->field_3 = 3;
        }
    }
    return 1;
}

/// Message handler that accepts the message and does nothing else.
s32 func_acropolis_hallway_8017D72C(void)
{
    return 0;
}

s32 func_acropolis_hallway_8017D734(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) { /* irregular */
        case 6:
            SndEvt_EnqueueType6(0x51070006, 0, 0);
            break;
        case 7:
            SndEvt_EnqueueType6(0x51070007, 0, 0);
            break;
    }
    return 0;
}

/// State 0 of the room task: installs the room's message table, publishes the
/// task in pointer slot 7 and advances to the next state.
void func_acropolis_hallway_8017D784(Task* task)
{
    task->msgTable = D_acropolis_hallway_8017E238;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room task: does nothing.
void func_acropolis_hallway_8017D7C8(Task* task)
{
}

/// Runs the room task's current state through a stack copy of the room's
/// three-entry state table.
void func_acropolis_hallway_8017D7D0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_hallway_8017D5C4;
    sp.funcs[task->state](task);
}

void func_acropolis_hallway_8017D828(void)
{
}

/// Gets a 16.16 X/Y/Z displacement for `rec` from `func_800E0C10` and, when it
/// reports one, adds its X and Z to the coordinate's translation, rounding a
/// fractional part away from zero. The whole-unit displacement is also left in
/// `D_acropolis_hallway_8017FA4C`. Returns non-zero when the X or Z
/// displacement is non-zero.
s32 func_acropolis_hallway_8017D830(GsCOORDINATE2* coord, GpRec18* rec, s16 arg2)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    register void*    p asm("v1");
    s32               val;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    p        = head - 0x14;
    s        = p;
    *scratch = p;
    s->moved = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        coord->coord.t[0]              += ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]              += s->delta.vz.h.hi;
        D_acropolis_hallway_8017FA4C.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_acropolis_hallway_8017FA4C.vy = s->delta.vy.w >> 16;
        D_acropolis_hallway_8017FA4C.vz = s->delta.vz.w >> 16;
        val                             = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_acropolis_hallway_8017FA4C.vx++;
            } else {
                coord->coord.t[0]--;
                D_acropolis_hallway_8017FA4C.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_acropolis_hallway_8017FA4C.vz++;
            } else {
                coord->coord.t[2]--;
                D_acropolis_hallway_8017FA4C.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP_BYTES(0x14);
    return s->moved;
}

/// Measures the bearing of each type-1 or type-3 record in `recs` (up to
/// `count`, or the first zero key) from the coordinate's world position,
/// relative to the direction it faces. For a record that has every other such
/// record within a quarter turn of it, moves the coordinate `push` units back
/// along that record's bearing, in X and Z. Returns non-zero if it moved the
/// coordinate; returns 0 at once while `gGameSession->viewReady` is 1.
s32 func_acropolis_hallway_8017D9D4(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push)
{
    void**                  scratch;
    void**                  tail;
    u8*                     head;
    OverlayBisectorScratch* st;
    u16                     vz;
    s16                     d;
    s16                     dz;
    s32                     t;
    s32                     hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp = head - sizeof(OverlayBisectorScratch);
        st  = (OverlayBisectorScratch*)tmp;
    }
    st->eye.vx = *(u16*)&coord->coord.t[0];
    st->eye.vy = *(u16*)&coord->coord.t[1];
    vz         = *(u16*)&coord->coord.t[2];
    *scratch   = st;
    st->eye.vz = vz;

    overlayToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    overlayToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = *(u16*)&recs[st->i].point.vx - *(u16*)&st->eye.vx;
            st->delta.vy     = *(u16*)&recs[st->i].point.vy - *(u16*)&st->eye.vy;
            dz               = *(u16*)&recs[st->i].point.vz - *(u16*)&st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = ratan2(st->delta.vx, dz);

            st->delta.vx     = *(u16*)&st->aim.vx - *(u16*)&st->eye.vx;
            st->delta.vy     = *(u16*)&st->aim.vy - *(u16*)&st->eye.vy;
            dz               = *(u16*)&st->aim.vz - *(u16*)&st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = *(u16*)&st->angle[st->i] - ratan2(st->delta.vx, dz);

            d = st->angle[st->i];
            if (st->angle[st->i] < 0) {
            wrapUp1:
                if (d < -0x800) {
                    d += 0x1000;
                    goto wrapUp1;
                }
            } else {
            wrapDown1:
                if (d > 0x800) {
                    d -= 0x1000;
                    goto wrapDown1;
                }
            }
            st->angle[st->i] = d;
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                d        = st->diff;
                if (st->diff < 0) {
                wrapUp2:
                    if (d < -0x800) {
                        d += 0x1000;
                        goto wrapUp2;
                    }
                } else {
                wrapDown2:
                    if (d > 0x800) {
                        d -= 0x1000;
                        goto wrapDown2;
                    }
                }
                t        = d;
                st->diff = t;
                SOFT_BARRIER();
                if (t < 0) {
                    t = -t;
                }
                if (t >= 0x401) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    tail = (void**)G_SCRATCH_HEAD;
    hit  = st->hit;
    SCRATCH_POP_BYTES_AT(tail, sizeof(OverlayBisectorScratch));
    return hit;
}

/// Item-pickup model task step: on the first run resets the mesh flags and
/// arms the task, then hides the mesh with flag 0x80 unless the room is being
/// drawn from view 5, and always hides it once the item's 2-bit flag reads 2
/// (already taken).
void func_acropolis_hallway_8017E120(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = (TmdObject*)task->extra;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    if (task->state == 0) {
        tmd->flags    = 8;
        tmd->otOffset = 0;
        task->state++;
    }
    if (Gp_GetViewIndex() == 5) {
        tmd->flags = 8;
    } else {
        tmd->flags = 0x80;
    }
    if (flag == 2) {
        tmd->flags = 0x80;
    }
}

/// Model task step for a pickup's mesh: when the pickup's 2-bit flag reads 2
/// it sets mesh flag 4, otherwise it resets the mesh flags and draw offset and
/// allocates the mesh's TMD buffers. The view index is fetched but unused.
void func_acropolis_hallway_8017E1C0(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = (TmdObject*)task->extra;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    Gp_GetViewIndex();
    if (flag == 2) {
        tmd->flags |= 4;
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
}
