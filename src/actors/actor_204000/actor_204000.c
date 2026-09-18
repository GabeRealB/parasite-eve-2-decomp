#include "common.h"
#include "actors/actor_104000.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "psyq/abs.h"
#include <psyq/inline_c.h>

extern u8 D_80072729;

/// Bearing of `p` from `eye` in the XZ plane, staged in a scratch block of its
/// own that is released before `ratan2` runs.
static __inline__ s16 Actor204000_BearingXZ(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                    head;
    Actor104000AvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (Actor104000AvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vz);
}

/// Bearing of `p` from `eye` in the XY plane; used when the facing column is
/// close to vertical.
static __inline__ s16 Actor204000_BearingXY(SVECTOR3* p, SVECTOR3* eye)
{
    u8*                    head;
    Actor104000AvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (Actor104000AvoidDelta*)(head - 0x10);
    d->vx                 = p->vx - eye->vx;
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = p->vy - eye->vy;
    d->vz                 = p->vz - eye->vz;
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vy);
}

/// Pushes `coord` away from the obstacles in `recs`. Records of kind 0x10000
/// (which also raises the returned `blocked` flag) or 0x30000 each give a
/// bearing, at most eight; bearings more than 0x400 apart cancel each other.
/// Each survivor becomes a 10-unit step added to `push` and to the translation.
s32 func_actor_204000_8014A06C(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* push)
{
    u8*                      head;
    Actor104000AvoidScratch* s;
    s16                      diff;
    s16                      t;
    s32                      mag;

    if (gGameSession->viewReady == 1 || D_80072729 == 1) {
        return 0;
    }

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(Actor104000AvoidScratch);
    s                     = (Actor104000AvoidScratch*)*(u8**)G_SCRATCH_HEAD;
    s->blocked            = 0;
    push->vz              = 0;
    push->vy              = 0;
    push->vx              = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = *(u16*)&coord->workm.t[0];
    s->eye.vy = *(u16*)&coord->workm.t[1];
    s->eye.vz = *(u16*)&coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].field_4 == 0) {
            break;
        }
        s->kind = recs[s->i].field_4 & 0xFFFF0000;
        switch (s->kind) {
            case 0x10000:
                s->blocked = 1;
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = Actor204000_BearingXZ((SVECTOR3*)&recs[s->i].field_8, &s->eye);
        } else {
            s->angle[s->count] = Actor204000_BearingXY((SVECTOR3*)&recs[s->i].field_8, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            diff = (u16)s->angle[s->i] - (u16)s->angle[s->j];
            t    = diff;
            if (diff < 0) {
            wrapUp:
                if (t < -0x800) {
                    t += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (t > 0x800) {
                    t -= 0x1000;
                    goto wrapDown;
                }
            }
            mag     = t;
            s->diff = mag;
            SOFT_BARRIER();
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x401) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            __asm__ volatile("nop; nop; .word 0x4B98003D");
            gte_stsv(&s->dir);
            push->vx          += s->dir.vx;
            push->vz          += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + sizeof(Actor104000AvoidScratch);
    return s->blocked != 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000", func_actor_204000_8014A5B8);

extern Actor104000* D_actor_204000_80156538[6];

/// Message handler: 0x1003/1 registers the actor in its lead slot and places
/// it at that slot's start point; 0x1203 and 0x302 move it to the scripted
/// positions for its slot and pick the next state.
s32 func_actor_204000_8014A75C(Actor104000* arg0, s32 arg1, Actor104000Event* event)
{
    Actor104000Work* work;
    Actor104000Ctx*  ctx;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (event->words[0] == 0x1003 && event->words[1] == 1) {
        work->field_0                               = 0xE;
        D_actor_204000_80156538[ctx->field_8 >> 12] = arg0;
        ctx->field_14                               = 1;
        switch (ctx->field_8 >> 12) {
            case 0:
                arg0->field_2C->field_8->coord.t[0] = 0x116;
                arg0->field_2C->field_8->coord.t[1] = -0xBB8;
                arg0->field_2C->field_8->coord.t[2] = 0x6A4;
                arg0->field_2C->field_8->flg        = 0;
                break;
            case 1:
                arg0->field_2C->field_8->coord.t[0] = 0x2BC;
                arg0->field_2C->field_8->coord.t[1] = -0xBB8;
                arg0->field_2C->field_8->coord.t[2] = 0x56A;
                arg0->field_2C->field_8->flg        = 0;
                break;
            case 2:
                arg0->field_2C->field_8->coord.t[0] = -0x1E;
                arg0->field_2C->field_8->coord.t[1] = -0xBB8;
                arg0->field_2C->field_8->coord.t[2] = 0x500;
                arg0->field_2C->field_8->flg        = 0;
                break;
            case 3:
                arg0->field_2C->field_8->coord.t[0] = 0xB2;
                arg0->field_2C->field_8->coord.t[1] = -0xBB8;
                arg0->field_2C->field_8->coord.t[2] = 0x22E;
                arg0->field_2C->field_8->flg        = 0;
                break;
            case 4:
                arg0->field_2C->field_8->coord.t[0] = 0x21E;
                arg0->field_2C->field_8->coord.t[1] = -0xBB8;
                arg0->field_2C->field_8->coord.t[2] = -0xF2;
                arg0->field_2C->field_8->flg        = 0;
                break;
            case 5:
                arg0->field_2C->field_8->coord.t[0] = -0x46;
                arg0->field_2C->field_8->coord.t[1] = -0xBB8;
                arg0->field_2C->field_8->coord.t[2] = -0x20B;
                arg0->field_2C->field_8->flg        = 0;
                break;
        }
    }
    if (event->words[0] == 0x1203) {
        switch (event->words[1]) {
            case 0:
                work->field_0 = 0;
                break;
            case 1:
                switch (ctx->field_8 >> 12) {
                    case 0:
                        work->field_0                       = 0x10;
                        arg0->field_2C->field_8->coord.t[0] = -0x3AC;
                        arg0->field_2C->field_8->coord.t[1] = -0xF0;
                        arg0->field_2C->field_8->coord.t[2] = 0x166C;
                        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, 0x3E8, 1);
                        Gp_ArmStateF0(1);
                        break;
                    case 1:
                        work->field_0                       = 0x11;
                        arg0->field_2C->field_8->coord.t[0] = 0x2A8;
                        arg0->field_2C->field_8->coord.t[1] = -0x7D0;
                        arg0->field_2C->field_8->coord.t[2] = 0x189C;
                        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, 0x800, 1);
                        break;
                }
                break;
        }
    }
    if (event->words[0] == 0x302) {
        switch (event->words[1]) {
            case 0:
                work->field_0 = 9;
                break;
            case 9:
                work->field_0 = 0;
                break;
            case 1:
                switch (ctx->field_8 >> 12) {
                    case 0:
                        arg0->field_2C->field_8->coord.t[0] = 0xF1E;
                        arg0->field_2C->field_8->coord.t[1] = -0x384;
                        arg0->field_2C->field_8->coord.t[2] = 0xFE6;
                        arg0->field_2C->field_8->flg        = 0;
                        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, -0x400, 1);
                        work->field_0 = 0x11;
                        break;
                    case 1:
                        arg0->field_2C->field_8->coord.t[0] = 0xA1E;
                        arg0->field_2C->field_8->coord.t[1] = -0x384;
                        arg0->field_2C->field_8->coord.t[2] = 0x1590;
                        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, 0x7D0, 1);
                        arg0->field_2C->field_8->flg = 0;
                        work->field_0                = 0x12;
                        break;
                    case 2:
                        arg0->field_2C->field_8->coord.t[0] = 0x1A4;
                        arg0->field_2C->field_8->coord.t[1] = -0x4C4;
                        arg0->field_2C->field_8->coord.t[2] = 0x1194;
                        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, 0x3E8, 1);
                        arg0->field_2C->field_8->flg = 0;
                        work->field_0                = 0x12;
                        break;
                    case 3:
                    case 4:
                    default:
                        work->field_0 = 0x11;
                        break;
                }
                break;
        }
    }
    return 0;
}

/// Seeds animation slots 1..5 with the step's animation id and blend speed.
static __inline__ void Actor204000_ResetSlots(Actor104000Work* arg0)
{
    Actor104000Work* work = arg0;
    s32              i;

    for (i = 1; i < 6; i++) {
        work->slots[i].field_9 = work->field_176 + work->field_178;
        Gp_AnimResetSlot(&work->anim, i, work->field_174);
    }
    work->field_172 = work->field_174;
}

/// Advances animation slots 1..5 by one tick.
static __inline__ void Actor204000_TickSlots(Actor104000* arg0)
{
    Actor104000Work* work;
    s32              i;

    work = arg0->field_1C;
    for (i = 1; i < 6; i++) {
        work->slots[i].field_9 = work->field_176 + work->field_178;
        Gp_AnimTickIndex(&work->anim, i);
    }
}

/// Animation step: states 1 and 2 reset the slots and move to 3, which ticks
/// them and counts frames (`field_17A`) and loop ends (`field_17C`).
void func_actor_204000_8014AC8C(Actor104000* arg0)
{
    Actor104000Work* work;

    work = arg0->field_1C;
    if (work->field_170 == 1) {
        Actor204000_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 2) {
        Actor204000_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 3) {
        work->field_17A++;
        Actor204000_TickSlots(arg0);
        if (work->field_58 & 2) {
            work->field_17C++;
        }
    }
}

/// Closes this unit's `.rodata` after the message handler's jump tables so
/// `actor_204000_2`'s start at 0x80149E54. Nothing reads it.
const u32 D_actor_204000_80149E50 = 0;
