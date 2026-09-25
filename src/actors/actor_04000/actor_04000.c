#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include "actors/actor.h"
#include "actors/actors_shared_80135990.h"
#include "actors/actors_shared_80138548.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "psyq/abs.h"

/// Event packet handed to the message handlers: the same four bytes read as
/// two `u16` words, a command word (0x1003, 0x1203, 0x302) and a sub-command.
typedef union Actor104000Event {
    /* 0x0 */ u8  bytes[4];
    /* 0x0 */ u16 words[2];
} Actor104000Event;

/// The actor's per-instance work block (`Task::work`),
/// allocated and filled by `Actor04000_Fn010B8`. It embeds four
/// collision objects linked with `Gp_LinkObj`, each followed by the `GpRec18`
/// table its `field_C` points at; the high bit of their flag words gates one
/// behaviour and bit 0x4000 another.
typedef struct Actor104000Work {
    /* 0x000 */ s16        field_0;
    /* 0x002 */ s16        field_2;
    /* 0x004 */ s16        field_4;
    /* 0x006 */ u16        field_6;
    /* 0x008 */ s16        field_8;
    /* 0x00A */ s16        field_A;
    /* 0x00C */ GpAnimCtx  anim;
    /* 0x020 */ GpAnimSlot slots[1]; // slots 1..5 continue past here, overlapping the fields below
    /* 0x048 */ byte       pad_48[2];
    /* 0x04A */ u16        field_4A; // low ten bits: animation id (`slots[1].field_2`)
    /* 0x04C */ byte       pad_4C[0xC];
    /* 0x058 */ u16        field_58;
    /* 0x05A */ byte       pad_5A[0xB6];
    /* 0x110 */ byte       poses[0x60]; // `func_800B3F84` arg3
    /* 0x170 */ s16        field_170;
    /* 0x172 */ s16        field_172;
    /* 0x174 */ s16        field_174;
    /* 0x176 */ s16        field_176;
    /* 0x178 */ s16        field_178;
    /* 0x17A */ s16        field_17A;
    /* 0x17C */ s16        field_17C;
    /* 0x17E */ s16        field_17E;
    /* 0x180 */ s32        field_180;
    /* 0x184 */ s32        field_184;
    /* 0x188 */ s32        field_188;
    /* 0x18C */ s32        field_18C;
    /* 0x190 */ s32        field_190;
    /* 0x194 */ u16        field_194;
    /* 0x196 */ byte       pad_196[2];
    /* 0x198 */ u16        field_198;
    /* 0x19A */ u16        field_19A;
    /* 0x19C */ s16        field_19C;
    /* 0x19E */ byte       pad_19E[2];
    /* 0x1A0 */ s16        field_1A0;
    /* 0x1A2 */ s16        field_1A2;
    /* 0x1A4 */ byte       pad_1A4[0xC];
    /* 0x1B0 */ GpRec18    rec1B0[8];
    /* 0x270 */ GpObj      obj270;
    /* 0x290 */ GpRec18    hits[8]; // this frame's collision records, ended by a zero id
    /* 0x350 */ GpObj      obj350;
    /* 0x370 */ GpRec18    rec370;
    /* 0x388 */ GpObj      obj388;
    /* 0x3A8 */ GpRec18    rec3A8;
    /* 0x3C0 */ GpObj      obj3C0;
    /* 0x3E0 */ GpEffArg   eff;           // `func_800FDB18` argument record
    /* 0x3E8 */ SVECTOR    effOfs;        // offset handed to `func_800FDB18`; `pad` picks the coordinate
    /* 0x3F0 */ SVECTOR    origin;        // model position at spawn
    /* 0x3F8 */ SVECTOR    dir;           // facing direction captured on restart
    /* 0x400 */ SVECTOR    patrol[2];     // spawn position plus (0) / minus (1) 1000 units along the facing (XZ)
    /* 0x410 */ s16        patrolIdx;     // `patrol` point currently walked toward
    /* 0x412 */ byte       pad_412[2];
    /* 0x414 */ MATRIX     lightMtx;      // installed at `TmdObject::lightMtx`
    /* 0x434 */ MATRIX     colorMtx;      // installed at `TmdObject::colorMtx`
    /* 0x454 */ MATRIX     savedColorMtx; // `colorMtx` before the death fade scales it
    /* 0x474 */ u16        field_474;     // animation id that last raised the reaction
    /* 0x476 */ byte       pad_476[3];
    /* 0x479 */ u8         field_479;
    /* 0x47A */ u8         field_47A;
    /* 0x47B */ byte       pad_47B[1];
    /* 0x47C */ byte       field_47C[0x14];
    /* 0x490 */ s32        field_490;
    /* 0x494 */ s16        field_494;
    /* 0x496 */ s16        field_496;
} Actor104000Work;
STATIC_ASSERT_SIZEOF(Actor104000Work, 0x498);

/// 0x18-byte scratch taken from `0x1F8003FC` while applying a hit: the first
/// type-2 record's position, its offset from the model origin, the attack id,
/// the computed damage and the hit's yaw relative to the model's facing.
typedef struct Actor104000HitScratch {
    /* 0x00 */ SVECTOR d;
    /* 0x08 */ SVECTOR pos;
    /* 0x10 */ s32     id;
    /* 0x14 */ u16     dmg;
    /* 0x16 */ s16     angle;
} Actor104000HitScratch;
STATIC_ASSERT_SIZEOF(Actor104000HitScratch, 0x18);

/// 0xC-byte scratch taken from `0x1F8003FC` by the walking state: the offset
/// to the spawn point (later the camera target) and the clamped new yaw.
typedef struct Actor104000TurnScratch {
    /* 0x0 */ SVECTOR d;
    /* 0x8 */ s16     angle;
    /* 0xA */ s16     pad;
} Actor104000TurnScratch;
STATIC_ASSERT_SIZEOF(Actor104000TurnScratch, 0xC);

/// 0x14-byte scratch taken from `0x1F8003FC` by the lunge state: the offset to
/// the player (later the snap direction), the final yaw and the relative yaw.
typedef struct Actor104000AimScratch {
    /* 0x00 */ SVECTOR d;
    /* 0x08 */ byte    pad_8[8];
    /* 0x10 */ s16     yaw;
    /* 0x12 */ s16     angle;
} Actor104000AimScratch;
STATIC_ASSERT_SIZEOF(Actor104000AimScratch, 0x14);

/// 0x34-byte scratch from `G_SCRATCH_HEAD` for rebuilding a coordinate as a
/// scaled yaw: the rotation, the uniform scale applied to it and the yaw.
typedef struct Actor104000FaceScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
} Actor104000FaceScratch;
STATIC_ASSERT_SIZEOF(Actor104000FaceScratch, 0x34);

/// The nineteen handlers the tick copies onto its stack before dispatching.
typedef struct Actor104000StateTable {
    /* 0x00 */ GpEnemyTaskFunc fn[19];
} Actor104000StateTable;
STATIC_ASSERT_SIZEOF(Actor104000StateTable, 0x4C);

extern u8      D_80072729;
extern u8      D_8007216C;
extern MATRIX* D_80073B8C;

extern Task* Actor04000_D0C710[2];
extern Task* Actor04000_D0C718[6];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

void Actor04000_Fn06A5C(GpEnemy* enemy, Task* task);
void Actor04000_Fn06878(GpEnemy* arg0, Task* arg1);
void Actor04000_Fn06994(GpEnemy* arg0, Task* arg1);
void Actor04000_Fn06AC4(GpEnemy* arg0, Task* arg1);
void Actor04000_Fn06BC8(GpEnemy* arg0, Task* arg1);
void Actor04000_Fn06C80(GpEnemy* arg0, Task* arg1);
void Actor04000_Fn06D38(GpEnemy* arg0, Task* arg1);

/// Pushes `coord` away from the obstacles in `recs`. Records of kind 0x10000
/// (which also raises the returned `blocked` flag) or 0x30000 each give a
/// bearing, at most eight; bearings more than 0x400 apart cancel each other.
/// Each survivor becomes a 10-unit step added to `push` and to the translation.
s32 Actor04000_Fn0024C(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* push)
{
    u8*                  head;
    OverlayAvoidScratch* s;
    s16                  diff;
    s16                  t;
    s32                  mag;

    if (gGameSession->viewReady == 1 || D_80072729 == 1) {
        return 0;
    }

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(OverlayAvoidScratch);
    s                     = (OverlayAvoidScratch*)*(u8**)G_SCRATCH_HEAD;
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
        if (recs[s->i].key == 0) {
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        switch (s->kind) {
            case 0x10000:
                s->blocked = 1;
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = overlayBearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = overlayBearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
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
            gte_gpf12();
            gte_stsv(&s->dir);
            push->vx          += s->dir.vx;
            push->vz          += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + sizeof(OverlayAvoidScratch);
    return s->blocked != 0;
}

/// Whole-unit part of the last step `Actor04000_Fn00798` applied.
extern SVECTOR Actor04000_D0C708;

/// Steps `coord` by the movement the first `arg2` `GpRec18` records of
/// `movement` resolve to, and keeps the whole-unit part of that step in
/// `Actor04000_D0C708`. Returns 1 when the X or Z step is nonzero; a step with
/// a fractional part moves the coordinate and the kept step one unit further
/// from zero.
s32 Actor04000_Fn00798(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
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
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]    = coord->coord.t[0] + ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]    = coord->coord.t[2] + s->delta.vz.h.hi;
        Actor04000_D0C708.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        Actor04000_D0C708.vy = s->delta.vy.w >> 16;
        Actor04000_D0C708.vz = s->delta.vz.w >> 16;
        val                  = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                Actor04000_D0C708.vx++;
            } else {
                coord->coord.t[0]--;
                Actor04000_D0C708.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                Actor04000_D0C708.vz++;
            } else {
                coord->coord.t[2]--;
                Actor04000_D0C708.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->moved;
}

/// Message handler: 0x1003/1 registers the actor in its lead slot and places
/// it at that slot's start point; 0x1203 and 0x302 move it to the scripted
/// positions for its slot and pick the next state.
s32 Actor04000_Fn0093C(Task* arg0, s32 arg1, Actor104000Event* event)
{
    Actor104000Work* work;
    GpEnemy*         ctx;

    work = arg0->work;
    ctx  = arg0->spawnArg2;
    if (event->words[0] == 0x1003 && event->words[1] == 1) {
        work->field_0                          = 0xE;
        Actor04000_D0C718[ctx->placeKey >> 12] = arg0;
        ctx->node.flags                        = 1;
        switch (ctx->placeKey >> 12) {
            case 0:
                ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0x116;
                ((TmdObject*)arg0->extra)->coords->coord.t[1] = -0xBB8;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0x6A4;
                ((TmdObject*)arg0->extra)->coords->flg        = 0;
                break;
            case 1:
                ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0x2BC;
                ((TmdObject*)arg0->extra)->coords->coord.t[1] = -0xBB8;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0x56A;
                ((TmdObject*)arg0->extra)->coords->flg        = 0;
                break;
            case 2:
                ((TmdObject*)arg0->extra)->coords->coord.t[0] = -0x1E;
                ((TmdObject*)arg0->extra)->coords->coord.t[1] = -0xBB8;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0x500;
                ((TmdObject*)arg0->extra)->coords->flg        = 0;
                break;
            case 3:
                ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0xB2;
                ((TmdObject*)arg0->extra)->coords->coord.t[1] = -0xBB8;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0x22E;
                ((TmdObject*)arg0->extra)->coords->flg        = 0;
                break;
            case 4:
                ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0x21E;
                ((TmdObject*)arg0->extra)->coords->coord.t[1] = -0xBB8;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] = -0xF2;
                ((TmdObject*)arg0->extra)->coords->flg        = 0;
                break;
            case 5:
                ((TmdObject*)arg0->extra)->coords->coord.t[0] = -0x46;
                ((TmdObject*)arg0->extra)->coords->coord.t[1] = -0xBB8;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] = -0x20B;
                ((TmdObject*)arg0->extra)->coords->flg        = 0;
                break;
        }
    }
    if (event->words[0] == 0x1203) {
        switch (event->words[1]) {
            case 0:
                work->field_0 = 0;
                break;
            case 1:
                switch (ctx->placeKey >> 12) {
                    case 0:
                        work->field_0                                 = 0x10;
                        ((TmdObject*)arg0->extra)->coords->coord.t[0] = -0x3AC;
                        ((TmdObject*)arg0->extra)->coords->coord.t[1] = -0xF0;
                        ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0x166C;
                        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, 0x3E8, 1);
                        Gp_ArmStateF0(1);
                        break;
                    case 1:
                        work->field_0                                 = 0x11;
                        ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0x2A8;
                        ((TmdObject*)arg0->extra)->coords->coord.t[1] = -0x7D0;
                        ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0x189C;
                        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, 0x800, 1);
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
                switch (ctx->placeKey >> 12) {
                    case 0:
                        ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0xF1E;
                        ((TmdObject*)arg0->extra)->coords->coord.t[1] = -0x384;
                        ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0xFE6;
                        ((TmdObject*)arg0->extra)->coords->flg        = 0;
                        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, -0x400, 1);
                        work->field_0 = 0x11;
                        break;
                    case 1:
                        ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0xA1E;
                        ((TmdObject*)arg0->extra)->coords->coord.t[1] = -0x384;
                        ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0x1590;
                        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, 0x7D0, 1);
                        ((TmdObject*)arg0->extra)->coords->flg = 0;
                        work->field_0                          = 0x12;
                        break;
                    case 2:
                        ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0x1A4;
                        ((TmdObject*)arg0->extra)->coords->coord.t[1] = -0x4C4;
                        ((TmdObject*)arg0->extra)->coords->coord.t[2] = 0x1194;
                        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, 0x3E8, 1);
                        ((TmdObject*)arg0->extra)->coords->flg = 0;
                        work->field_0                          = 0x12;
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

/// Seeds animation slots 1..5 with the requested animation id and blend speed.
static __inline__ void Actor204000_ResetSlots(Actor104000Work* arg0)
{
    Actor104000Work* work = arg0;
    s32              i;

    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimResetSlot(&work->anim, i, work->field_174);
    }
    work->field_172 = work->field_174;
}

/// Advances animation slots 1..5 by one tick.
static __inline__ void Actor204000_TickSlots(Task* arg0)
{
    Actor104000Work* work;
    s32              i;

    work = arg0->work;
    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimTickIndex(&work->anim, i);
    }
}

/// Steps the motion state in `field_170`: states 1 and 2 restart slots 1..5 on
/// the animation in `field_174`, clear the frame and loop counters and move to
/// state 3; state 3 ticks the slots, counting frames in `field_17A` and, while
/// bit 2 of `field_58` is set, loops in `field_17C`.
void Actor04000_Fn00E6C(Task* arg0)
{
    Actor104000Work* work;

    work = arg0->work;
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

/// Reaction check keyed on the requested animation in `field_174`: for 2 and
/// 3, answers 0x40280001 the first time the playing animation id in `field_4A`
/// reaches one of that animation's trigger ids (latched in `field_474`, which
/// clears on any other id); for 5, answers 0x400C0005 while bit 2 of
/// `field_58` is set. Answers 0 otherwise.
s32 Actor04000_Fn00FDC(Actor104000Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->field_174) {
        case 2:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->field_474 == v) {
                goto same;
            }
            arg0->field_474 = id;
            return 0x40280001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_474 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_474 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

extern GpPairSrcE Actor04000_D07084;
extern u8         Actor04000_D0C4C4[]; // animation bank handed to `func_800B3F84`
extern u8         Actor04000_D0C6B0[];

/// Spawn state: allocates the work block, links the four collision objects and
/// the enemy node, seeds the size and HP from the enemy's level nibble, records
/// the spawn position and the points 1000 units ahead and behind it, then
/// starts in state 2 when the high half of `Task::spawnArg1` is 1 and state 7 otherwise.
void Actor04000_Fn010B8(GpEnemy* arg0, Task* arg1)
{
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    Actor104000Work* work;
    GpRec18*         hits;
    SVECTOR          sv;
    VECTOR           pos;
    SVECTOR*         p;
    SVECTOR*         q;
    GpObj*           o1;
    GpObj*           o2;
    GpObj*           o3;
    GpObj*           o4;

    obj        = arg1->extra;
    coord      = obj->coords;
    work       = memCalloc(sizeof(Actor104000Work), 0);
    arg1->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    coord->sub      = &gGfxViewCoord;
    arg1->msgTable  = Actor04000_D0C6B0;
    work->field_180 = 0;
    work->field_184 = 1;
    work->field_18C = 3;
    work->field_188 = 0;
    work->field_190 = 1;
    obj->flags      = 0;
    func_800B3F84(&work->anim, Actor04000_D0C4C4, (TmdObject*)obj, work->poses, work->slots);

    o1           = &work->obj270;
    o1->coord    = ((TmdObject*)arg1->extra)->coords + 1;
    o1->ctx.recs = work->rec1B0;
    o1->pos.vy   = -0x110;
    o1->pos.vx   = 0;
    o1->pos.vz   = 0;
    o1->key      = 0x3000C;
    o1->radius   = 0x190;
    o1->flags    = 1;
    Gp_LinkObj(2, o1);
    o1->flags |= 0x4000;
    Gp_InitRec18Table(o1->ctx.recs, 8, 0);

    o2           = &work->obj350;
    sv.vx        = 0;
    sv.vy        = -0x168;
    sv.vz        = 0;
    p            = &sv;
    hits         = work->hits;
    o2->coord    = ((TmdObject*)arg1->extra)->coords + 2;
    o2->ctx.recs = hits;
    o2->pos.vx   = p->vx;
    o2->pos.vy   = p->vy;
    o2->pos.vz   = p->vz;
    o2->key      = 0x3000C;
    o2->radius   = 0x168;
    o2->flags    = 1;
    Gp_LinkObj(2, o2);
    o2->flags |= 0x8000;
    Gp_InitRec18Table(o2->ctx.recs, 8, 0);

    sv.vx        = 0;
    sv.vy        = 0;
    sv.vz        = 0;
    o3           = &work->obj388;
    o3->coord    = &gGfxViewCoord;
    o3->ctx.recs = &work->rec370;
    o3->pos.vx   = p->vx;
    o3->pos.vy   = p->vy;
    o3->pos.vz   = p->vz;
    o3->radius   = 0x500;
    o3->flags    = 1;
    Gp_LinkObj(3, o3);
    Gp_InitRec18Table(o3->ctx.recs, 1, 0);

    o4           = &work->obj3C0;
    o4->coord    = &gGfxViewCoord;
    o4->ctx.recs = &work->rec3A8;
    o4->pos.vx   = p->vx;
    o4->pos.vy   = p->vy;
    o4->pos.vz   = p->vz;
    o4->radius   = 0x80;
    o4->flags    = 1;
    Gp_LinkObj(8, o4);
    Gp_InitRec18Table(o4->ctx.recs, 1, 0);

    arg0->field_4    = &coord->coord;
    arg0->field_48   = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->coord      = ((TmdObject*)arg1->extra)->coords + 2;
    Gp_LinkNode(&arg0->node);
    arg0->node.flags    = 1;
    arg0->reactionFlags = 0;
    arg0->hp = arg0->hpMax = Actor04000_D07084.hpMax;
    arg0->param            = &Actor04000_D07084;
    arg0->recs             = hits;
    work->field_170        = 2;
    work->field_174        = 1;
    work->field_176        = 0x10;
    work->field_178        = 0;
    Actor04000_Fn00E6C(arg1);
    work->field_17E = 0;
    work->field_A   = 0;
    obj->lightMtx   = &work->lightMtx;
    obj->colorMtx   = &work->colorMtx;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    work->field_1A0 = 5;
    work->field_1A2 = 0x14;
    if ((u16)(arg0->placeKey >> 12) % 2 == 1) {
        work->field_176 += arg0->placeKey >> 12;
        work->field_1A2 += arg0->placeKey >> 12;
        work->field_1A0 += arg0->placeKey >> 12;
    } else {
        work->field_176 -= (u16)(arg0->placeKey >> 12) / 2;
        work->field_1A2 -= arg0->placeKey >> 13;
        work->field_1A0 -= arg0->placeKey >> 13;
    }
    work->origin.vx = ((TmdObject*)arg1->extra)->coords->coord.t[0];
    work->origin.vy = ((TmdObject*)arg1->extra)->coords->coord.t[1];
    work->origin.vz = ((TmdObject*)arg1->extra)->coords->coord.t[2];
    Gfx_MatrixCol2(&((TmdObject*)arg1->extra)->coords->coord, &sv);
    sv.vy = 0;
    q     = &sv;
    VectorNormalSS(q, q);
    gte_lddp(1000);
    gte_ldsv(q);
    gte_gpf12();
    gte_stsv(q);
    work->patrol[0].vx = ((TmdObject*)arg1->extra)->coords->coord.t[0] + sv.vx;
    work->patrol[0].vy = ((TmdObject*)arg1->extra)->coords->coord.t[1];
    work->patrol[0].vz = ((TmdObject*)arg1->extra)->coords->coord.t[2] + sv.vz;
    work->patrol[1].vx = ((TmdObject*)arg1->extra)->coords->coord.t[0] - sv.vx;
    work->patrol[1].vy = ((TmdObject*)arg1->extra)->coords->coord.t[1];
    work->patrol[1].vz = ((TmdObject*)arg1->extra)->coords->coord.t[2] - sv.vz;
    /* the gameplay prototype takes no argument, but this call site passes 0 */
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    if ((arg1->spawnArg1 >> 16) == 0) {
        work->field_0 = 7;
    } else if ((arg1->spawnArg1 >> 16) == 1) {
        work->field_0 = 2;
    } else {
        work->field_0 = 7;
    }
    work->field_2   = -1;
    work->field_479 = 0;
    work->field_47A = 0;
    arg1->state++;
}

#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern GpAnimArg Actor04000_D0C530;
extern s32       Actor04000_D07094;
extern s32       Actor04000_D070A0;
extern byte      Actor04000_D0C510[];
extern byte      Actor04000_D0C520[];

/// Lunge state: steps forward on frames 8 and 9, then from frame 9 on grabs
/// the player when within 600 units and a quarter turn of the facing, dispatches
/// the side-dependent grab message and snaps the model beside and facing them.
void Actor04000_Fn0168C(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work*       work;
    Task*                  player;
    GameActor*             actor;
    TmdObject*             obj;
    Actor104000AimScratch* head;
    Actor104000AimScratch* sc;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         pos;
    s16                    angle;
    s32                    mag;

    work   = arg1->work;
    player = gameGetPtrSlot(3);
    actor  = player->work;
    if (work->field_4 != 0) {
        obj                                     = arg1->extra;
        ((GpEnemy*)arg1->spawnArg2)->node.flags = 0;
        Gp_ArmStateF0(1);
        obj->flags          = 0;
        work->field_170     = 1;
        work->field_176     = 0x10;
        work->field_178     = 0;
        work->field_174     = 0xD;
        work->obj270.flags |= 0x4000;
        Actor04000_Fn00E6C(arg1);
        work->field_6 = 0;
        return;
    }
    Actor04000_Fn00E6C(arg1);
    work->field_6++;
    if ((s16)work->field_6 < 8) {
        return;
    }
    if ((s16)work->field_6 == 8) {
        actorStepForward(((TmdObject*)arg1->extra)->coords, 0x32);
        return;
    }
    if ((s16)work->field_6 == 9) {
        actorStepForward(((TmdObject*)arg1->extra)->coords, 0x32);
    }
    work->field_0 = 0xC;
    head          = (Actor104000AimScratch*)SCRATCH_SP;
    sc            = (Actor104000AimScratch*)(SCRATCH_SP -= sizeof(Actor104000AimScratch));
    pos           = ((TmdObject*)arg1->extra)->coords;
    head[-1].d.vx = Player_Status.coordMtx->t[0] - pos->coord.t[0];
    sc->d.vy      = Player_Status.coordMtx->t[1] - pos->coord.t[1];
    sc->d.vz      = Player_Status.coordMtx->t[2] - pos->coord.t[2];
    coord         = ((TmdObject*)arg1->extra)->coords;
    angle         = ratan2(head[-1].d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle     = actorWrapAngle(angle);
    if (!overlayOutOfRange(&sc->d, 600)) {
        mag = (sc->angle >= 0) ? sc->angle : -sc->angle;
        if (mag < 0x200) {
            if (actor->field_954 != 2) {
                work->field_490 = 0xC;
                if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)work->field_47C, 0) == 0) {
                    coord     = ((TmdObject*)player->extra)->coords;
                    angle     = ratan2(sc->d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
                    sc->angle = actorWrapAngle(angle);
                    if (sc->angle < 0) {
                        Actor04000_D0C530.animBlock.ptr = Actor04000_D0C510;
                    } else {
                        Actor04000_D0C530.animBlock.ptr = Actor04000_D0C520;
                    }
                    Actor04000_D0C530.field_4 = 1;
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&Actor04000_D0C530, 0);
                    work->field_0   = 0xB;
                    work->field_496 = 1;
                    Gfx_MatrixCol0(&((TmdObject*)player->extra)->coords->coord, &sc->d);
                    sc->d.vy = 0;
                    VectorNormalSS(&sc->d, &sc->d);
                    if (sc->angle < 0) {
                        gte_lddp(-0x3C);
                        gte_ldsv(&sc->d);
                        gte_gpf12();
                        gte_stsv(&sc->d);
                    } else {
                        gte_lddp(0x3C);
                        gte_ldsv(&sc->d);
                        gte_gpf12();
                        gte_stsv(&sc->d);
                    }
                    ((TmdObject*)arg1->extra)->coords->coord.t[0] = ((TmdObject*)player->extra)->coords->coord.t[0] + sc->d.vx;
                    ((TmdObject*)arg1->extra)->coords->coord.t[1] = ((TmdObject*)player->extra)->coords->coord.t[1];
                    ((TmdObject*)arg1->extra)->coords->coord.t[2] = ((TmdObject*)player->extra)->coords->coord.t[2] + sc->d.vz;
                    sc->d.vy                                      = 0;
                    VectorNormalSS(&sc->d, &sc->d);
                    gte_lddp(-0x258);
                    gte_ldsv(&sc->d);
                    gte_gpf12();
                    gte_stsv(&sc->d);
                    ((TmdObject*)arg1->extra)->coords->coord.t[0] += sc->d.vx;
                    ((TmdObject*)arg1->extra)->coords->coord.t[2] += sc->d.vz;
                    sc->d.vx                                       = -sc->d.vx;
                    sc->d.vy                                       = -sc->d.vy;
                    sc->d.vz                                       = -sc->d.vz;
                    sc->yaw                                        = ratan2(sc->d.vx, sc->d.vz);
                    Gfx_RotMatrixY(&((TmdObject*)arg1->extra)->coords->coord, sc->yaw, 1);
                    ((TmdObject*)arg1->extra)->coords->flg = 0;
                }
            }
        }
    }
    SCRATCH_SP += sizeof(Actor104000AimScratch);
}

/// Turns `coord` to face along its own Z axis in the XZ plane and scales the
/// rotation uniformly by `s`, working on a scratch block.
static __inline__ void Actor204000_FaceScale(GsCOORDINATE2* coord, s16 s)
{
    Actor104000FaceScratch* head;
    Actor104000FaceScratch* sc;

    head                                      = *(Actor104000FaceScratch**)G_SCRATCH_HEAD;
    sc                                        = head - 1;
    *(Actor104000FaceScratch**)G_SCRATCH_HEAD = sc;
    sc->angle                                 = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&sc->m, sc->angle, 1);
    sc->scale.vx = sc->scale.vy = sc->scale.vz = s;
    ScaleMatrix(&sc->m, &head[-1].scale);
    coord->coord.m[0][0]                       = head[-1].m.m[0][0];
    coord->coord.m[0][1]                       = sc->m.m[0][1];
    coord->coord.m[0][2]                       = sc->m.m[0][2];
    coord->coord.m[1][0]                       = sc->m.m[1][0];
    coord->coord.m[1][1]                       = sc->m.m[1][1];
    coord->coord.m[1][2]                       = sc->m.m[1][2];
    coord->coord.m[2][0]                       = sc->m.m[2][0];
    coord->coord.m[2][1]                       = sc->m.m[2][1];
    coord->coord.m[2][2]                       = sc->m.m[2][2];
    coord->flg                                 = 0;
    *(Actor104000FaceScratch**)G_SCRATCH_HEAD += 1;
}

/// Frames 0x5B onward of the collapse: drifts the model along its facing for the
/// first 0x13 frames, steps the effects keyed on `field_6`, then fades the colour
/// matrix out and grows the model over frames 0x5C-0x64.
void Actor04000_Fn01E1C(GpEnemy* arg0, Task* arg1)
{
    SVECTOR          dir;
    SVECTOR*         d;
    VECTOR           scale;
    Actor104000Work* work;
    TmdObject*       obj;
    s16              s;
    s32              pan;
    s32              id;

    work = arg1->work;
    obj  = arg1->extra;
    if (work->field_4 != 0) {
        arg0->node.flags    = 1;
        obj->flags          = 0;
        work->obj350.flags &= 0x7FFF;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj388.key    = Gp_PackObjPair(arg0, 1);
        work->obj3C0.key    = 0x22222;
        work->field_6       = 0;
        work->obj270.flags |= 0x4000;
        work->savedColorMtx = work->colorMtx;
        work->field_174     = 0xE;
        work->field_170     = 1;
        work->field_178     = 0;
        Actor04000_Fn00E6C(arg1);
        work->obj3C0.pos.vx       = ((TmdObject*)arg1->extra)->coords->coord.t[0];
        work->obj3C0.pos.vy       = ((TmdObject*)arg1->extra)->coords->coord.t[1] - 0x1F4;
        work->obj3C0.pos.vz       = ((TmdObject*)arg1->extra)->coords->coord.t[2];
        work->obj388.pos.vx       = ((TmdObject*)arg1->extra)->coords->coord.t[0];
        work->obj388.pos.vy       = ((TmdObject*)arg1->extra)->coords->coord.t[1];
        work->obj388.pos.vz       = ((TmdObject*)arg1->extra)->coords->coord.t[2];
        Actor04000_D0C530.field_4 = 2;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&Actor04000_D0C530, 0);
        work->field_6 = 0;
    }
    if ((s16)work->field_6 < 0x13) {
        Gfx_MatrixCol0(&((TmdObject*)arg1->extra)->coords->coord, &dir);
        d      = &dir;
        dir.vy = 0;
        VectorNormalSS(d, d);
        gte_lddp(0x15);
        gte_ldsv(d);
        gte_gpf12();
        gte_stsv(d);
        ((TmdObject*)arg1->extra)->coords->coord.t[0] += dir.vx;
        ((TmdObject*)arg1->extra)->coords->coord.t[2] += dir.vz;
        ((TmdObject*)arg1->extra)->coords->flg         = 0;
    }
    Actor04000_Fn00E6C(arg1);
    switch ((s16)(work->field_6 - 0x5B)) {
        case 0:
            if (work->field_496 == 1) {
                if (((GameActor*)(gameGetPtrSlot(3))->work)->field_954 == 2) {
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
                }
                work->field_496 = 0;
            }
            ((TmdObject*)arg1->extra)->flags = 2;
            break;
        case 1:
            Gp_SpawnScript18Ex((s32)&Actor04000_D07094, (s32)&Actor04000_D070A0,
                               (s16)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords));
            work->obj388.radius = 0x3E8;
            work->obj3C0.radius = 0xFA;
            work->obj388.flags |= 0x8000;
            work->obj3C0.flags |= 0x8000;
            Gp_SpawnEff(0x6009C, &((TmdObject*)arg1->extra)->coords[2], 1, NULL);
            break;
        case 2:
            work->obj3C0.radius = 0x1F4;
            break;
        case 3:
            work->obj3C0.radius = 0x3E8;
            work->obj388.flags &= 0x7FFF;
            break;
        case 5:
            Gp_ReleaseStateF0Add(arg1, 0xC);
            work->obj3C0.flags &= 0x7FFF;
            break;
        case 7:
            if ((s8)work->field_479 == 0) {
                Gp_SpawnEff(0x6009E, ((TmdObject*)arg1->extra)->coords, 0, NULL);
            }
            obj->flags = 0x80;
            id         = ((arg0->placeKey >> 12) << 8) | 0x40280004;
            pan        = (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords));
            break;
        case 28:
            work->field_0 = 0;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    work->colorMtx = work->savedColorMtx;
    if ((u16)(work->field_6 - 0x17) < 0x44) {
        work->colorMtx.t[0] += ((s16)work->field_6 - 0x16) * 0x60;
    }
    if ((u16)(work->field_6 - 0x5C) < 9) {
        s = 0xBB8 - ((s16)work->field_6 - 0x5C) * 600;
        if (s < 0x4B0) {
            scale.vx = scale.vy = scale.vz = 0;
            Actor204000_FaceScale(((TmdObject*)arg1->extra)->coords, 0x1000);
            ScaleMatrix(&work->colorMtx, &scale);
            work->colorMtx.t[0] = work->colorMtx.t[1] = work->colorMtx.t[2] = 0;
            Actor204000_FaceScale(((TmdObject*)arg1->extra)->coords, 0x1000);
        } else {
            scale.vx = scale.vy = scale.vz = s;
            work->colorMtx                 = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(s);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            s = ((s16)work->field_6 - 0x5A) * 0x400 + 0x1000;
            if (s > 0x2000) {
                s = 0x2000;
            }
            Actor204000_FaceScale(((TmdObject*)arg1->extra)->coords, s);
        }
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

/// Restarts the actor when `field_4` is set; otherwise steps it, occasionally
/// switches to state 3 on a random roll, and arms the player state when the
/// camera target comes within 2000 units.
void Actor04000_Fn026FC(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    TmdObject*       obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                 = arg1->extra;
        arg0->node.flags    = 0;
        obj->flags          = 0;
        work->field_174     = 5;
        work->field_170     = 1;
        work->field_178     = 0;
        work->obj350.flags |= 0x8000;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj270.flags |= 0x4000;
        Actor04000_Fn00E6C(arg1);
        return;
    }
    Actor04000_Fn00E6C(arg1);
    if ((work->field_58 & 2) && work->field_17C >= 0x19) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 0x10) & 7)) {
            work->field_0 = 3;
        }
    }
    coord    = ((TmdObject*)arg1->extra)->coords;
    d        = &delta;
    delta.vx = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy    = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz    = D_80073B8C->t[2] - coord->coord.t[2];
    if (!overlayOutOfRange(d, 2000)) {
        Gp_ArmStateF0(1);
        work->field_0 = 3;
    }
}

/// Chasing state: restarts the actor when `field_4` is set; otherwise turns
/// toward the camera target by at most 0x10 a frame and steps forward, counting
/// frames spent more than 1000 units away (state 8 after 240), and switches to
/// state 10 within 600 units and an eighth turn of the facing.
void Actor04000_Fn028F0(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work*        work;
    Actor104000TurnScratch* head;
    Actor104000TurnScratch* sc;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          target;
    GsCOORDINATE2*          pos;
    TmdObject*              obj;
    s16                     angle;
    s32                     mag;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                 = arg1->extra;
        arg0->node.flags    = 0;
        obj->flags          = 0;
        work->field_174     = 3;
        work->field_170     = 1;
        work->field_178     = 0x10;
        work->obj350.flags |= 0x8000;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj270.flags |= 0x4000;
        Gp_ArmStateF0(1);
        Actor04000_Fn00E6C(arg1);
        work->field_494 = 0;
        return;
    }
    head = (Actor104000TurnScratch*)SCRATCH_SP;
    sc   = (Actor104000TurnScratch*)(SCRATCH_SP -= sizeof(Actor104000TurnScratch));
    Actor04000_Fn00E6C(arg1);
    pos           = ((TmdObject*)arg1->extra)->coords;
    head[-1].d.vx = Player_Status.coordMtx->t[0] - pos->coord.t[0];
    sc->d.vy      = Player_Status.coordMtx->t[1] - pos->coord.t[1];
    sc->d.vz      = Player_Status.coordMtx->t[2] - pos->coord.t[2];
    coord         = ((TmdObject*)arg1->extra)->coords;
    angle         = ratan2(head[-1].d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle     = actorWrapAngle(angle);
    if (sc->angle > 0x10) {
        sc->angle = 0x10;
    }
    if (sc->angle < -0x10) {
        sc->angle = -0x10;
    }
    sc->angle += ratan2(-((TmdObject*)arg1->extra)->coords->coord.m[2][0], ((TmdObject*)arg1->extra)->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg1->extra)->coords->coord, sc->angle, 1);
    actorStepForward(((TmdObject*)arg1->extra)->coords, 0x14);
    Actor04000_Fn00798(((TmdObject*)arg1->extra)->coords, work->rec1B0, 8);
    if (overlayOutOfRange(&sc->d, 1000)) {
        work->field_494++;
    } else {
        work->field_494 = 0;
    }
    Actor04000_Fn0024C(((TmdObject*)arg1->extra)->coords, work->hits, 8, &sc->d);
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    sc->d.vx                               = work->origin.vx - ((TmdObject*)arg1->extra)->coords->coord.t[0];
    sc->d.vy                               = 0;
    sc->d.vz                               = work->origin.vz - ((TmdObject*)arg1->extra)->coords->coord.t[2];
    overlayOutOfRange(&sc->d, 3000);
    if (work->field_494 > 0xF0) {
        work->field_0 = 8;
    }
    target    = ((TmdObject*)arg1->extra)->coords;
    sc->d.vx  = Player_Status.coordMtx->t[0] - target->coord.t[0];
    sc->d.vy  = Player_Status.coordMtx->t[1] - target->coord.t[1];
    sc->d.vz  = Player_Status.coordMtx->t[2] - target->coord.t[2];
    coord     = ((TmdObject*)arg1->extra)->coords;
    angle     = ratan2(sc->d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = actorWrapAngle(angle);
    if (!overlayOutOfRange(&sc->d, 600)) {
        mag = (sc->angle >= 0) ? sc->angle : -sc->angle;
        if (mag < 0x200) {
            work->field_0 = 0xA;
        }
    }
    SCRATCH_SP += sizeof(Actor104000TurnScratch);
}

/// Frames 0x28 onward of the collapse: steps the effects keyed on `field_6`,
/// then fades the colour matrix out and grows the model over frames 0x2A-0x32.
void Actor04000_Fn02F48(GpEnemy* arg0, Task* arg1)
{
    VECTOR           scale;
    Actor104000Work* work;
    TmdObject*       obj;
    s16              s;
    s32              pan;
    s32              id;

    work = arg1->work;
    obj  = arg1->extra;
    if (work->field_4 != 0) {
        arg0->node.flags    = 1;
        obj->flags          = 0;
        work->obj350.flags |= 0x8000;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj388.key    = Gp_PackObjPair(arg0, 1);
        work->obj3C0.key    = 0x22222;
        work->field_6       = 0;
        work->obj270.flags |= 0x4000;
        work->savedColorMtx = work->colorMtx;
        work->field_178     = 0;
        Actor04000_Fn00E6C(arg1);
        work->obj3C0.pos.vx = ((TmdObject*)arg1->extra)->coords->coord.t[0];
        work->obj3C0.pos.vy = ((TmdObject*)arg1->extra)->coords->coord.t[1] - 0x1F4;
        work->obj3C0.pos.vz = ((TmdObject*)arg1->extra)->coords->coord.t[2];
        work->obj388.pos.vx = ((TmdObject*)arg1->extra)->coords->coord.t[0];
        work->obj388.pos.vy = ((TmdObject*)arg1->extra)->coords->coord.t[1];
        work->obj388.pos.vz = ((TmdObject*)arg1->extra)->coords->coord.t[2];
        if (work->field_194 == 0x1003) {
            Actor04000_D0C718[arg0->placeKey >> 12] = NULL;
        }
        return;
    }
    Actor04000_Fn00E6C(arg1);
    switch ((s16)(work->field_6 - 0x28)) {
        case 0:
            work->obj350.flags &= 0x7FFF;
            break;
        case 1:
            if (work->field_496 == 1) {
                if (((GameActor*)(gameGetPtrSlot(3))->work)->field_954 == 2) {
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
                }
                work->field_496 = 0;
            }
            ((TmdObject*)arg1->extra)->flags = 2;
            break;
        case 2:
            Gp_SpawnScript18Ex((s32)&Actor04000_D07094, (s32)&Actor04000_D070A0,
                               (s16)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords));
            work->obj388.radius = 0x3E8;
            work->obj3C0.radius = 0xFA;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, 0, 0x7DE);
            work->obj388.flags |= 0x8000;
            work->obj3C0.flags |= 0x8000;
            Gp_SpawnEff(0x6009C, &((TmdObject*)arg1->extra)->coords[2], 1, NULL);
            break;
        case 3:
            work->obj3C0.radius = 0x1F4;
            break;
        case 4:
            work->obj3C0.radius = 0x3E8;
            break;
        case 6:
            Gp_ReleaseStateF0Add(arg1, 0xC);
            work->obj3C0.flags &= 0x7FFF;
            break;
        case 8:
            if ((s8)work->field_479 == 0) {
                Gp_SpawnEff(0x6009E, ((TmdObject*)arg1->extra)->coords, 0, NULL);
            }
            id  = ((arg0->placeKey >> 12) << 8) | 0x40280004;
            pan = (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords));
            break;
        case 10:
            work->obj388.flags &= 0x7FFF;
            obj->flags          = 0x80;
            break;
        case 12:
            obj->flags |= 4;
            break;
        case 29:
            work->field_0 = 0;
            arg0->hp      = 0;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    work->colorMtx = work->savedColorMtx;
    if ((u16)(work->field_6 - 0x17) < 0x12) {
        work->colorMtx.t[0] += ((s16)work->field_6 - 0x16) * 0x60;
    }
    if ((u16)(work->field_6 - 0x2A) < 9) {
        s = 0xBB8 - ((s16)work->field_6 - 0x2A) * 600;
        if (s < 0x4B0) {
            scale.vx = scale.vy = scale.vz = 0;
            Actor204000_FaceScale(((TmdObject*)arg1->extra)->coords, 0x1000);
            ScaleMatrix(&work->colorMtx, &scale);
            work->colorMtx.t[0] = work->colorMtx.t[1] = work->colorMtx.t[2] = 0;
            Actor204000_FaceScale(((TmdObject*)arg1->extra)->coords, 0x1000);
        } else {
            scale.vx = scale.vy = scale.vz = s;
            work->colorMtx                 = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(s);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            s = ((s16)work->field_6 - 0x28) * 0x400 + 0x1000;
            if (s > 0x2000) {
                s = 0x2000;
            }
            Actor204000_FaceScale(((TmdObject*)arg1->extra)->coords, s);
        }
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

/// Death state: saves the colour matrix, then fades it and grows the model
/// over frames 13-21 while stepping through the collapse effects.
void Actor04000_Fn03798(GpEnemy* arg0, Task* arg1)
{
    VECTOR           scale;
    Actor104000Work* work;
    TmdObject*       obj;
    s16              s;
    s32              pan;
    s32              id;

    work = arg1->work;
    obj  = arg1->extra;
    if (work->field_4 != 0) {
        arg0->node.flags    = 1;
        obj->flags          = 0;
        work->obj350.flags &= 0x7FFF;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj388.key    = Gp_PackObjPair(arg0, 1);
        work->obj3C0.key    = 0x22222;
        work->field_6       = 0;
        work->obj270.flags &= 0xBFFF;
        work->savedColorMtx = work->colorMtx;
        work->field_174     = 0xA;
        work->field_170     = 1;
        work->field_178     = 0;
        work->field_176     = 0x2C;
        Actor04000_Fn00E6C(arg1);
        work->obj3C0.pos.vx = ((TmdObject*)arg1->extra)->coords->coord.t[0];
        work->obj3C0.pos.vy = ((TmdObject*)arg1->extra)->coords->coord.t[1] - 0x1F4;
        work->obj3C0.pos.vz = ((TmdObject*)arg1->extra)->coords->coord.t[2];
        work->obj388.pos.vx = ((TmdObject*)arg1->extra)->coords->coord.t[0];
        work->obj388.pos.vy = ((TmdObject*)arg1->extra)->coords->coord.t[1];
        work->obj388.pos.vz = ((TmdObject*)arg1->extra)->coords->coord.t[2];
        Gp_ArmStateF0(1);
        if (work->field_194 == 0x1003) {
            Actor04000_D0C718[arg0->placeKey >> 12] = NULL;
        }
    }
    Actor04000_Fn00E6C(arg1);
    switch ((s16)(work->field_6 - 0xD)) {
        case 0:
            id  = ((arg0->placeKey >> 12) << 8) | 0x40280004;
            pan = (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords));
            ((TmdObject*)arg1->extra)->flags = 2;
            break;
        case 1:
            work->obj388.radius = 0x3E8;
            work->obj388.flags |= 0x8000;
            Gp_SpawnEff(0x6009C, &((TmdObject*)arg1->extra)->coords[2], 1, NULL);
            Gp_SpawnScript18((s32)&Actor04000_D07094, (s32)&Actor04000_D070A0);
            break;
        case 2:
            Gp_ReleaseStateF0Add(arg1, 0xC);
            work->obj3C0.radius = 0xFA;
            work->obj388.flags &= 0x7FFF;
            work->obj3C0.flags |= 0x8000;
            break;
        case 3:
            work->obj3C0.radius = 0x1F4;
            break;
        case 4:
            work->obj3C0.radius = 0x3E8;
            break;
        case 6:
            work->obj3C0.flags &= 0x7FFF;
            if ((s8)work->field_479 == 0) {
                Gp_SpawnEff(0x6009E, ((TmdObject*)arg1->extra)->coords, 0, NULL);
            }
            break;
        case 8:
            obj->flags = 0x80;
            break;
        case 10:
            obj->flags |= 4;
            break;
        case 25:
            work->field_0 = 0;
            break;
    }
    if ((u16)(work->field_6 - 0xD) < 9) {
        s = 0xBB8 - ((s16)work->field_6 - 0xB) * 0x320;
        if (s < 0) {
            s = 0;
        }
        scale.vx = scale.vy = scale.vz = s;
        work->colorMtx                 = work->savedColorMtx;
        ScaleMatrix(&work->colorMtx, &scale);
        gte_lddp(s);
        gte_ldlvl(work->colorMtx.t);
        gte_gpf12();
        gte_stlvl(work->colorMtx.t);
        s = (s16)work->field_6 * 0xB4 + 0x1000;
        if (s > 0x2000) {
            s = 0x2000;
        }
        Actor204000_FaceScale(((TmdObject*)arg1->extra)->coords, s);
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

/// Picks a random offset and coordinate index for an effect from the hit
/// angle `arg1` (front, back, right or left), copies it into `work->eff` and
/// spawns the effect for hit id `arg2`.
void Actor04000_Fn03D30(Task* arg0, s16 arg1, u32 arg2)
{
    SVECTOR*         sc;
    Actor104000Work* work;
    s32              mag;
    GsCOORDINATE2*   coord;

    sc   = (SVECTOR*)(SCRATCH_SP -= sizeof(SVECTOR));
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->work;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 2;
            sc->vx  = 80;
            sc->vy  = -180;
            sc->vz  = 330;
        } else {
            sc->pad = 2;
            sc->vx  = -60;
            sc->vy  = -150;
            sc->vz  = 300;
        }
    } else if (mag > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 1;
            sc->vx  = 0;
            sc->vy  = 0;
            sc->vz  = -180;
        } else {
            sc->pad = 2;
            sc->vx  = 2;
            sc->vy  = -50;
            sc->vz  = -50;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 5;
            sc->vx  = 100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 5;
            sc->vx  = 120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 4;
            sc->vx  = -100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 4;
            sc->vx  = -120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    }
    work->effOfs         = *sc;
    coord                = &((TmdObject*)arg0->extra)->coords[sc->pad];
    work->eff.spawnArgLo = 0x100;
    work->eff.spawnArgHi = 1;
    work->eff.coord      = coord;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &((TmdObject*)arg0->extra)->coords[sc->pad], &work->effOfs, &work->eff);
    SCRATCH_SP += sizeof(SVECTOR);
}

/// Applies the first type-2 hit in `work->hits`: computes its damage, turns the
/// model toward the hit, plays the impact sound and subtracts the damage from
/// `arg0->field_40`, switching to state 6 once it runs out.
void Actor04000_Fn03FB4(GpEnemy* arg0, Task* arg1)
{
    Actor104000HitScratch* sc;
    Actor104000Work*       work;
    GpRec18*               recs;
    SVECTOR*               pos;
    s32                    mask;
    s32                    kind;
    s32                    id;
    s16                    angle;
    s32                    snd;
    s32                    pan;
    s16                    i;

    work = arg1->work;
    sc   = (Actor104000HitScratch*)(SCRATCH_SP -= sizeof(Actor104000HitScratch));
    pos  = &sc->pos;
    recs = work->hits;
    i    = 0;
    mask = 0xFFFF0000;
    kind = 0x20000;
scan:
    if (recs[i].key == 0) {
        goto missed;
    }
    if ((recs[i].key & mask) == kind) {
        pos->vx = recs[i].point.vx;
        pos->vy = recs[i].point.vy;
        pos->vz = recs[i].point.vz;
        id      = recs[i].key;
        goto found;
    }
    i++;
    if (i < 8) {
        goto scan;
    }
missed:
    id = 0;
found:
    sc->id = id;

    if (id != 0) {
        sc->dmg                                = Gp_ComputeDamage(sc->id, 0, 0, 0x1000);
        ((TmdObject*)arg1->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
        sc->d.vx = ((TmdObject*)arg1->extra)->coords->workm.t[0];
        sc->d.vy = ((TmdObject*)arg1->extra)->coords->workm.t[1];
        sc->d.vz = ((TmdObject*)arg1->extra)->coords->workm.t[2];
        sc->d.vx = sc->pos.vx - ((TmdObject*)arg1->extra)->coords->workm.t[0];
        sc->d.vy = sc->pos.vy - ((TmdObject*)arg1->extra)->coords->workm.t[1];
        sc->d.vz = sc->pos.vz - ((TmdObject*)arg1->extra)->coords->workm.t[2];
        angle    = ratan2(sc->d.vx, sc->d.vz) -
                ratan2(-((TmdObject*)arg1->extra)->coords->workm.m[2][0], ((TmdObject*)arg1->extra)->coords->workm.m[2][2]);
        sc->angle = angle;
        sc->angle = actorWrapAngle(angle);
        Actor04000_Fn03D30(arg1, sc->angle, sc->id);
        snd = ((arg0->placeKey >> 12) << 8) | 0x40280003;
        pan = (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords));
        func_800E2C78(arg0, sc->id, sc->dmg, 0);
        func_800DA6E8(&arg0->node, sc->dmg, 0);
        arg0->hp -= sc->dmg;
        if (arg0->hp <= 0) {
            work->field_0 = 6;
        }
        if (work->field_496 == 1) {
            if (((GameActor*)gameGetPtrSlot(3)->work)->field_954 == 2) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            }
            work->field_496 = 0;
        }
    }
    SCRATCH_SP += sizeof(Actor104000HitScratch);
}

/// Patrol state: restarts the actor when `field_4` is set; otherwise turns the
/// model toward the current patrol point by at most 0x20 a frame and steps it
/// forward, swapping patrol points within 400 units or after 97 blocked frames,
/// switching to state 4 when the camera target is within 2000 units and either
/// inside a quarter turn of the facing or within 1000 units, and occasionally to
/// state 1 once `field_17C` passes 20.
void Actor04000_Fn0432C(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work*        work;
    Actor104000TurnScratch* head;
    Actor104000TurnScratch* sc;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          target;
    TmdObject*              obj;
    s16                     angle;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                 = arg1->extra;
        arg0->node.flags    = 0;
        obj->flags          = 0;
        work->field_174     = 2;
        work->field_170     = 1;
        work->field_178     = 0;
        work->patrolIdx     = 0;
        work->obj350.flags |= 0x8000;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj270.flags |= 0x4000;
        Actor04000_Fn00E6C(arg1);
        work->field_6 = 0;
        return;
    }
    head          = (Actor104000TurnScratch*)SCRATCH_SP;
    sc            = (Actor104000TurnScratch*)(SCRATCH_SP -= sizeof(Actor104000TurnScratch));
    head[-1].d.vx = work->patrol[work->patrolIdx].vx - ((TmdObject*)arg1->extra)->coords->coord.t[0];
    sc->d.vy      = 0;
    sc->d.vz      = work->patrol[work->patrolIdx].vz - ((TmdObject*)arg1->extra)->coords->coord.t[2];
    coord         = ((TmdObject*)arg1->extra)->coords;
    angle         = ratan2(head[-1].d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle     = actorWrapAngle(angle);
    if (sc->angle > 0x20) {
        sc->angle = 0x20;
    }
    if (sc->angle < -0x20) {
        sc->angle = -0x20;
    }
    sc->angle += ratan2(-((TmdObject*)arg1->extra)->coords->coord.m[2][0], ((TmdObject*)arg1->extra)->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg1->extra)->coords->coord, sc->angle, 1);
    actorStepForward(((TmdObject*)arg1->extra)->coords, 5);
    if (Actor04000_Fn00798(((TmdObject*)arg1->extra)->coords, work->rec1B0, 8)) {
        work->field_6++;
    }
    if (!overlayOutOfRange(&sc->d, 400) || (s16)work->field_6 > 0x60) {
        if (work->patrolIdx == 0) {
            work->patrolIdx = 1;
        } else {
            work->patrolIdx = 0;
        }
        work->field_6 = 0;
    }
    Actor04000_Fn0024C(((TmdObject*)arg1->extra)->coords, work->hits, 8, &sc->d);
    target   = ((TmdObject*)arg1->extra)->coords;
    sc->d.vx = Player_Status.coordMtx->t[0] - target->coord.t[0];
    sc->d.vy = Player_Status.coordMtx->t[1] - target->coord.t[1];
    sc->d.vz = Player_Status.coordMtx->t[2] - target->coord.t[2];
    if (!overlayOutOfRange(&sc->d, 2000)) {
        coord = ((TmdObject*)arg1->extra)->coords;
        angle = ratan2(sc->d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (actorWrapAngle(angle) < 0x400 || !overlayOutOfRange(&sc->d, 1000)) {
            work->field_0 = 4;
        }
    }
    Actor04000_Fn00E6C(arg1);
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    if ((work->field_58 & 2) && work->field_17C > 0x14) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 0x10) & 7)) {
            work->field_0 = 1;
        }
    }
    SCRATCH_SP += sizeof(Actor104000TurnScratch);
}

/// Walking state: restarts the actor when `field_4` is set; otherwise turns the
/// model toward its spawn point by at most 0x10 a frame and steps it forward,
/// switching to state 1 within 80 units of the spawn point and to state 4 when
/// the camera target is within 2000 units and either inside a quarter turn of
/// the facing or within 1000 units.
void Actor04000_Fn049C0(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work*        work;
    Actor104000TurnScratch* head;
    Actor104000TurnScratch* sc;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          target;
    TmdObject*              obj;
    s16                     angle;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                 = arg1->extra;
        arg0->node.flags    = 0;
        obj->flags          = 0;
        work->field_174     = 2;
        work->field_170     = 1;
        work->field_178     = 0;
        work->obj350.flags |= 0x8000;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj270.flags |= 0x4000;
        Actor04000_Fn00E6C(arg1);
        work->field_494 = 0;
        return;
    }
    head = (Actor104000TurnScratch*)SCRATCH_SP;
    sc   = (Actor104000TurnScratch*)(SCRATCH_SP -= sizeof(Actor104000TurnScratch));
    Actor04000_Fn00E6C(arg1);
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    head[-1].d.vx                          = work->origin.vx - ((TmdObject*)arg1->extra)->coords->coord.t[0];
    sc->d.vy                               = 0;
    sc->d.vz                               = work->origin.vz - ((TmdObject*)arg1->extra)->coords->coord.t[2];
    coord                                  = ((TmdObject*)arg1->extra)->coords;
    angle                                  = ratan2(head[-1].d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle                              = actorWrapAngle(angle);
    if (sc->angle > 0x10) {
        sc->angle = 0x10;
    }
    if (sc->angle < -0x10) {
        sc->angle = -0x10;
    }
    sc->angle += ratan2(-((TmdObject*)arg1->extra)->coords->coord.m[2][0], ((TmdObject*)arg1->extra)->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg1->extra)->coords->coord, sc->angle, 1);
    actorStepForward(((TmdObject*)arg1->extra)->coords, 8);
    Actor04000_Fn00798(((TmdObject*)arg1->extra)->coords, work->rec1B0, 8);
    if (!overlayOutOfRange(&sc->d, 80)) {
        work->field_0 = 1;
    }
    Actor04000_Fn0024C(((TmdObject*)arg1->extra)->coords, work->hits, 8, &sc->d);
    target   = ((TmdObject*)arg1->extra)->coords;
    sc->d.vx = Player_Status.coordMtx->t[0] - target->coord.t[0];
    sc->d.vy = Player_Status.coordMtx->t[1] - target->coord.t[1];
    sc->d.vz = Player_Status.coordMtx->t[2] - target->coord.t[2];
    if (!overlayOutOfRange(&sc->d, 2000)) {
        coord = ((TmdObject*)arg1->extra)->coords;
        angle = ratan2(sc->d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (actorWrapAngle(angle) < 0x400 || !overlayOutOfRange(&sc->d, 1000)) {
            work->field_0 = 4;
        }
    }
    SCRATCH_SP += sizeof(Actor104000TurnScratch);
}

/// Restarts the actor when `field_4` is set; otherwise waits 50 frames, then
/// drops the model with growing speed, unwinding its Z roll by at most 0x92 a
/// frame, and on landing plays the impact sound and switches to state 3.
void Actor04000_Fn04FA4(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    s32              id;
    s32              pan;
    s32              mag;
    s32              rot;
    s32              step;

    work = arg1->work;
    if (work->field_4 != 0) {
        ((TmdObject*)arg1->extra)->flags = 0;
        work->field_174                  = 5;
        work->field_170                  = 1;
        work->field_178                  = 0;
        work->obj350.flags              |= 0x8000;
        work->obj388.flags              &= 0x7FFF;
        work->obj3C0.flags              &= 0x7FFF;
        work->obj270.flags              |= 0x4000;
        Actor04000_Fn00E6C(arg1);
        ratan2(-((TmdObject*)arg1->extra)->coords->coord.m[2][0], ((TmdObject*)arg1->extra)->coords->coord.m[2][2]);
        ((TmdObject*)arg1->extra)->coords->flg = 0;
        work->field_19A                        = 10;
        work->field_198                        = 0;
        work->field_6                          = 0;
        work->field_19C                        = 0x800;
        work->field_479                        = 1;
        return;
    }
    if ((s16)work->field_6 < 0x32) {
        work->field_6++;
        return;
    }
    work->field_19A                               += 4;
    work->field_198                               += work->field_19A;
    ((TmdObject*)arg1->extra)->coords->coord.t[1] += (s16)work->field_198;
    if (((TmdObject*)arg1->extra)->coords->coord.t[1] >= -0x12B) {
        if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x03100000) {
            id  = ((arg0->placeKey >> 12) << 8) | 0x53100006;
            pan = (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords));
        }
        ((TmdObject*)arg1->extra)->coords->coord.t[1] = 0;
        work->field_0                                 = 3;
        work->field_479                               = 0;
        Gfx_RotMatrixZ(&((TmdObject*)arg1->extra)->coords->coord, -work->field_19C, 0);
    } else {
        rot = work->field_19C;
        if (rot != 0) {
            mag  = __builtin_abs(rot);
            step = rot;
            SOFT_TOUCH_REG(step);
            step = -step;
            if (mag >= 0x93) {
                step = -0x92;
                if (rot < 0) {
                    step = 0x92;
                }
            }
            Gfx_RotMatrixZ(&((TmdObject*)arg1->extra)->coords->coord, (s16)step, 0);
            work->field_19C += step;
        }
    }
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Actor04000_Fn00E6C(arg1);
}

/// Restarts the actor when `field_4` is set; otherwise cycles `field_6` through
/// a 32-frame loop that resets the model position, steps it back and forth
/// and changes `field_176`, then spins it and raises `field_14` in view 5.
void Actor04000_Fn0522C(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work* work;

    work = arg1->work;
    if (work->field_4 != 0) {
        ((TmdObject*)arg1->extra)->flags = 0;
        work->obj350.flags              |= 0x8000;
        work->obj388.flags              &= 0x7FFF;
        work->obj3C0.flags              &= 0x7FFF;
        work->obj270.flags              |= 0x4000;
        arg0->node.flags                 = 0;
        work->field_174                  = 3;
        work->field_170                  = 2;
        work->field_178                  = 0;
        Actor04000_Fn00E6C(arg1);
        Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords->coord, 0x400, 0);
        ((TmdObject*)arg1->extra)->coords->flg = 0;
        work->field_479                        = 1;
        work->obj350.coord                     = &gGfxViewCoord;
        work->obj350.pos.vx                    = -0x3AC;
        work->obj350.pos.vy                    = -0xF0;
        work->field_6                          = 0;
        work->obj350.pos.vz                    = 0x166C;
        return;
    }
    switch ((s16)++work->field_6 % 32) {
        case 0:
            work->field_176                               = 0x40;
            ((TmdObject*)arg1->extra)->coords->coord.t[0] = -0x3AC;
            ((TmdObject*)arg1->extra)->coords->coord.t[1] = -0xF0;
            ((TmdObject*)arg1->extra)->coords->coord.t[2] = 0x166C;
            break;
        case 1:
        case 2:
        case 4:
        case 5:
        case 7:
        case 8:
            actorStepForward(((TmdObject*)arg1->extra)->coords, -0x78);
            break;
        case 11:
        case 12:
        case 14:
            actorStepForward(((TmdObject*)arg1->extra)->coords, 0xC8);
            break;
        case 17:
            work->field_176 = 0x10;
            break;
        case 25:
            work->field_176 = 8;
            break;
    }
    Gfx_RotMatrixY(&((TmdObject*)arg1->extra)->coords->coord, 0x44C, 1);
    Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords->coord, 0x190, 0);
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Actor04000_Fn00E6C(arg1);
    if ((u8)Gp_GetViewIndex() == 5) {
        arg0->node.flags = 1;
        Gp_ClearNodeSlots(&((GpEnemy*)arg0)->node);
        return;
    }
    arg0->node.flags = 0;
}

/// Restarts the actor when `field_4` is set; otherwise runs state 0xC (drop the
/// model to the ground, then hop forward and tip it over), state 0x10 (wait for
/// the flag or 80 frames) and state 0x11 (slide along `dir` while rolling).
void Actor04000_Fn055C8(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          sv;
    s32              y;
    u16              h;
    s16              t;

    work = arg1->work;
    if (work->field_4 != 0) {
        ((TmdObject*)arg1->extra)->flags = 0;
        work->obj350.flags              |= 0x8000;
        work->obj388.flags              &= 0x7FFF;
        work->obj3C0.flags              &= 0x7FFF;
        work->obj270.flags              &= 0xBFFF;
        arg0->node.flags                 = 0;
        work->field_174                  = 0xC;
        work->field_170                  = 2;
        work->field_178                  = 0;
        work->field_176                  = 1;
        Actor04000_Fn00E6C(arg1);
        Actor04000_Fn00E6C(arg1);
        work->field_176                        = 0;
        ((TmdObject*)arg1->extra)->coords->flg = 0;
        work->field_6                          = 0;
        work->field_479                        = 1;
        work->field_19A                        = 0xA;
        work->field_198                        = 0;
        work->field_6                          = 0;
        work->field_8                          = 0;
        Gfx_MatrixCol0(&((TmdObject*)arg1->extra)->coords->coord, &work->dir);
        VectorNormalSS(&work->dir, &work->dir);
    }
    work->field_6++;
    switch (work->field_174) {
        case 12:
            if (work->field_176 == 0) {
                work->field_19A += 2;
                work->field_198 += work->field_19A;
                h                = work->field_198;
                coord            = ((TmdObject*)arg1->extra)->coords;
                y                = coord->coord.t[1];
                if (y >= 0 || (y < 0 ? -y : y) < (s16)h) {
                    coord->coord.t[1] = 0;
                    work->field_176   = 0x10;
                    work->field_6     = 0;
                } else {
                    coord->coord.t[1] = y + (s16)h;
                }
                ((TmdObject*)arg1->extra)->coords->flg = 0;
                return;
            }
            Actor04000_Fn00E6C(arg1);
            if ((s16)work->field_6 < 0xA) {
                actorStepForward(((TmdObject*)arg1->extra)->coords, 0x23);
            }
            if ((s16)work->field_6 < 4) {
                ((TmdObject*)arg1->extra)->coords->coord.t[1] -= 0x67;
            }
            if ((u32)(work->field_6 - 4) < 8) {
                ((TmdObject*)arg1->extra)->coords->coord.t[1] -= 0xB;
                ((TmdObject*)arg1->extra)->coords->flg         = 0;
                Gfx_RotMatrixX(&((TmdObject*)arg1->extra)->coords->coord, -0x100, 0);
            }
            if ((s16)work->field_6 == 0xC) {
                work->field_170 = 2;
                work->field_174 = 0x10;
                work->field_6   = 0;
                work->field_176 = 0x10;
            }
            break;
        case 16:
            Actor04000_Fn00E6C(arg1);
            if (!((arg0->placeKey >> 0xC) & 1)) {
                t = work->field_6;
                if (t < 0x14) {
                    break;
                }
                if (t < 0x50) {
                    break;
                }
            }
            work->field_174 = 0x11;
            work->field_170 = 2;
            work->field_6   = 0;
            break;
        case 17:
            Actor04000_Fn00E6C(arg1);
            if ((u32)(work->field_6 - 0xD) < 0x10) {
                ((TmdObject*)arg1->extra)->coords->coord.t[1] += 0xD;
                sv                                             = work->dir;
                gte_lddp(0x14);
                gte_ldsv(&sv);
                gte_gpf12();
                gte_stsv(&sv);
                ((TmdObject*)arg1->extra)->coords->coord.t[0] += sv.vx;
                ((TmdObject*)arg1->extra)->coords->coord.t[2] += sv.vz;
                Gfx_RotMatrixZ(&((TmdObject*)arg1->extra)->coords->coord, -0x88, 0);
                ((TmdObject*)arg1->extra)->coords->flg = 0;
            }
            if (work->field_58 & 1) {
                work->field_0   = 4;
                work->field_479 = 0;
            }
            break;
    }
}

/// Resets the actor when `field_4` is set; otherwise advances the `field_6`
/// timer, stepping the model forward in three speed bands and switching to
/// state 0x11 once it passes 48.
void Actor04000_Fn05AE8(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work* work;

    work = arg1->work;
    if (work->field_4 != 0) {
        ((TmdObject*)arg1->extra)->flags = 0;
        work->obj350.flags              |= 0x8000;
        work->obj388.flags              &= 0x7FFF;
        work->obj3C0.flags              &= 0x7FFF;
        work->obj270.flags              &= 0xBFFF;
        arg0->node.flags                 = 0;
        work->field_174                  = 0xB;
        work->field_170                  = 2;
        work->field_178                  = 0;
        work->field_176                  = 1;
        Actor04000_Fn00E6C(arg1);
        work->field_176                        = 0x10;
        ((TmdObject*)arg1->extra)->coords->flg = 0;
        work->field_6                          = 0;
        work->field_479                        = 1;
        work->field_19A                        = 0xA;
        work->field_198                        = 0;
        work->field_6                          = 0;
        work->field_8                          = 0;
        return;
    }
    work->field_6++;
    Actor04000_Fn00E6C(arg1);
    if (work->field_6 >= 0x13 && work->field_6 < 0x23) {
        actorStepForward(((TmdObject*)arg1->extra)->coords, 4);
    }
    if (work->field_6 >= 0x23 && work->field_6 < 0x28) {
        actorStepForward(((TmdObject*)arg1->extra)->coords, 0xC);
    }
    if (work->field_6 >= 0x28 && work->field_6 < 0x31) {
        actorStepForward(((TmdObject*)arg1->extra)->coords, 0x18);
        ((TmdObject*)arg1->extra)->coords->coord.t[1] += 0x28;
    }
    if ((s16)work->field_6 > 0x30) {
        work->field_0 = 0x11;
    }
    ((TmdObject*)arg1->extra)->coords->flg = 0;
}

const Actor104000StateTable Actor04000_D001F4 = {
    {
        Actor04000_Fn06A5C,
        Actor04000_Fn06BC8,
        Actor04000_Fn026FC,
        Actor04000_Fn06C80,
        Actor04000_Fn028F0,
        Actor04000_Fn02F48,
        Actor04000_Fn03798,
        Actor04000_Fn0432C,
        Actor04000_Fn049C0,
        Actor04000_Fn06AC4,
        Actor04000_Fn0168C,
        Actor04000_Fn06878,
        Actor04000_Fn06994,
        Actor04000_Fn01E1C,
        Actor04000_Fn06D38,
        Actor04000_Fn04FA4,
        Actor04000_Fn0522C,
        Actor04000_Fn055C8,
        Actor04000_Fn05AE8,
    }
};

/// Per-frame tick: tints the model from its position, draws the ground shadow
/// for the current light mode, runs the state handler (flagging a state change
/// in `field_4`), applies pending hits and plays the queued sound.
void Actor04000_Fn05F0C(GpEnemy* arg0, Task* arg1)
{
    VECTOR                pos;
    SVECTOR               unused; // never written; retail's frame keeps 8 bytes here
    Actor104000StateTable table;
    GsCOORDINATE2         coord;
    Actor104000Work*      work;
    OverlayMatWords*      mw;
    s32                   snd;
    s32                   pan;
    s32                   id;

    work                                   = arg1->work;
    table                                  = Actor04000_D001F4;
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
    pos.vx = ((TmdObject*)arg1->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)arg1->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)arg1->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    switch (Gp_StateF0.field_4) {
        case 0:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 5 && work->field_0 != 0xD &&
                work->field_0 != 0xF && work->field_0 != 0x10 && work->field_0 != 0x11) {
                ((TmdObject*)arg1->extra)->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)arg1->extra)->coords->workm.t, 0x100, Gp_State1C->groundShade);
            }
            if (work->field_0 == 0xF) {
                mw                                        = (OverlayMatWords*)&coord.coord;
                mw->m00_m01                               = 0x1000;
                ((OverlayMatWords*)&coord.coord)->m02_m10 = 0;
                mw->m11_m12                               = 0x1000;
                ((OverlayMatWords*)&coord.coord)->m20_m21 = 0;
                mw->m22                                   = 0x1000;
                coord.coord.t[0]                          = ((TmdObject*)arg1->extra)->coords->coord.t[0];
                coord.coord.t[1]                          = 0;
                coord.coord.t[2]                          = ((TmdObject*)arg1->extra)->coords->coord.t[2];
                coord.sub                                 = &gGfxViewCoord;
                coord.flg                                 = 0;
                Gp_UpdateCoord(&coord);
                Gp_DrawEffGroundQuad((VECTOR3*)coord.workm.t, 0x60, Gp_State1C->groundShade);
            }
            break;
        case 1:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 0xD && work->field_0 != 5 &&
                work->field_0 != 0xF && work->field_0 != 0x10 && work->field_0 != 0x11) {
                ((TmdObject*)arg1->extra)->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)arg1->extra)->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            Gp_ClearRec18Occupied(work->rec1B0);
            Gp_ClearRec18Occupied(work->hits);
            Gp_ClearRec18Occupied(&work->rec370);
            return;
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            Gp_ClearRec18Occupied(work->rec1B0);
            Gp_ClearRec18Occupied(work->hits);
            Gp_ClearRec18Occupied(&work->rec370);
            return;
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    table.fn[work->field_0]((GpEnemy*)arg0, arg1);
    if (work->field_496 == 1) {
        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0 || arg0->hp < 0) {
            if (((GameActor*)gameGetPtrSlot(3)->work)->field_954 == 2) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            }
            work->field_496 = 0;
        }
    }
    if (arg0->hp > 0) {
        Actor04000_Fn03FB4(arg0, arg1);
        if (arg0->hp <= 0) {
            work->field_0 = 6;
        }
    }
    Gp_ClearRec18Occupied(work->rec1B0);
    Gp_ClearRec18Occupied(work->hits);
    Gp_ClearRec18Occupied(&work->rec370);
    id = Actor04000_Fn00FDC(work);
    if (id != 0) {
        snd = id | ((arg0->placeKey >> 12) << 8);
        pan = (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg1->extra)->coords));
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)arg1->extra)->coords->flg = 0;
    }
}

/// Refills the two lead slots: a slot whose actor's `field_40` has run out is
/// cleared, and an empty one takes the pooled actor in state 0xE farthest from
/// the player, moving it to state 0xF. With no candidate left the controller
/// task moves on to its next state.
void Actor04000_Fn06380(Task* arg0)
{
    s32            dist[8];
    SVECTOR        d;
    GsCOORDINATE2* coord;
    MATRIX*        m;
    PlayerStatus*  cfg;
    s32            best;
    s16            i;
    s16            j;
    s16            bi;

    bi = 0;
    for (i = 0; i < 2; i++) {
        if (Actor04000_D0C710[i] != NULL) {
            if (((GpEnemy*)Actor04000_D0C710[i]->spawnArg2)->hp <= 0) {
                Actor04000_D0C710[i] = NULL;
            }
            if (Actor04000_D0C710[i] != NULL) {
                continue;
            }
        }
        j   = 0;
        cfg = &Player_Status;
        for (; j < 6; j++) {
            if (Actor04000_D0C718[j] != NULL && ((Actor104000Work*)Actor04000_D0C718[j]->work)->field_0 == 0xE) {
                coord    = ((TmdObject*)Actor04000_D0C718[j]->extra)->coords;
                m        = cfg->coordMtx;
                d.vx     = m->t[0] - coord->coord.t[0];
                d.vy     = m->t[1] - coord->coord.t[1];
                d.vz     = m->t[2] - coord->coord.t[2];
                dist[j]  = d.vx * d.vx;
                dist[j] += d.vz * d.vz;
            } else {
                dist[j] = -1;
            }
        }
        best = -1;
        for (j = 0; j < 6; j++) {
            if (best < dist[j]) {
                best = dist[j];
                bi   = j;
            }
        }
        if (best == -1) {
            arg0->state++;
            return;
        }
        ((Actor104000Work*)Actor04000_D0C718[bi]->work)->field_0 = 0xF;
        Actor04000_D0C710[i]                                     = Actor04000_D0C718[bi];
        Actor04000_D0C718[bi]                                    = NULL;
    }
}

/// Handler for message 0x7D5: sets the display object's visibility bits and
/// the work block's state from a mode. 0 shows the object (0x80) and sets state
/// 7, 1 hides it and sets state 7, 2 raises the lost-model flag (4) and clears
/// the state, 3 hides it, clears the state and then raises the flag, and any
/// other mode leaves both alone. Always answers 0.
s32 Actor04000_Fn06590(Task* task, s32 arg1, s32 arg2)
{
    Actor104000Work* work;
    TmdObject*       obj;

    obj  = task->extra;
    work = (Actor104000Work*)task->work;

    switch (arg2) {
        case 0:
            obj->flags    = 0x80;
            work->field_0 = 7;
            break;
        case 1:
            obj->flags    = 0;
            work->field_0 = 7;
            break;
        case 2:
            obj->flags    = (u16)(obj->flags | 4);
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags    = (u16)(obj->flags | 4);
            break;
        default:
            return 0;
    }
    return 0;
}

/// Handler for message 0x7D4: places the actor's model from `placement`. The
/// three longs become the coordinate's translation, then the X, Y and Z angles
/// are applied in that order and the coordinate is marked dirty. Always
/// answers 1.
s32 Actor04000_Fn06634(Task* task, s32 arg1, GpXformArg* placement)
{
    ((TmdObject*)task->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->coords->flg = 0;
    return 1;
}

/// Message handler for 0x7DE: advances the work block's state from 0xB to 0xD
/// and leaves any other state alone.
s32 Actor04000_Fn06704(Task* arg0, s32 arg1, void* arg2)
{
    Actor104000Work* work;

    work = arg0->work;
    if (work->field_0 == 0xB) {
        work->field_0 = 0xD;
    }
    return 1;
}

/// Handler for message 0x7D3: latches the animation id the sender asks for and
/// picks motion state 2 when its flag is clear, 1 otherwise. Always answers 1.
s32 Actor04000_Fn06728(Task* task, s32 arg1, GpAnimArg* msg, s32 arg3)
{
    Actor104000Work* work = (Actor104000Work*)task->work;

    work->field_174 = msg->field_4;
    if (msg->field_8 == 0) {
        work->field_170 = 2;
    } else {
        work->field_170 = 1;
    }
    return 1;
}

/// Rebuilds `coord`'s rotation as a pure yaw - the angle its Z row already
/// faces in the XZ plane - scaled uniformly by `scale`, working in a block
/// borrowed from the scratchpad, and marks the coordinate dirty.
void Actor04000_Fn06760(GsCOORDINATE2* coord, s16 scale)
{
    void**                  scratch;
    void*                   head;
    Actor104000FaceScratch* blk;
    s16                     ang;
    u16                     m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor104000FaceScratch*)head - 1;
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((Actor104000FaceScratch*)head - 1)->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    *scratch             = (u8*)*scratch + sizeof(Actor104000FaceScratch);
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

void Actor04000_Fn06878(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                = arg1->extra;
        arg0->node.flags   = 0;
        obj->flags         = 0;
        work->field_176    = 0x10;
        work->field_178    = 0;
        work->obj270.flags = (u16)(work->obj270.flags | 0x4000);
        Actor04000_Fn00E6C(arg1);
        work->field_6 = 0;
        return;
    }
    Actor04000_Fn00E6C(arg1);
    if (!(work->field_6 & 7)) {
        Gp_SpawnPadLerp(3, 0xFF, 8);
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair(arg0, 0), 0);
    }
    work->field_6++;
    if ((s16)work->field_6 > 0x28) {
        work->field_490 = 0x270F;
        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)work->field_47C, 0) == 0) {
            work->field_0 = 5;
        } else {
            work->field_0 = 0xD;
        }
    }
}

void Actor04000_Fn06994(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                                     = arg1->extra;
        ((GpEnemy*)arg1->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        work->field_170                         = 2;
        work->field_176                         = 0x10;
        work->field_178                         = 0;
        work->field_174                         = 0xF;
        work->obj270.flags                      = (u16)(work->obj270.flags | 0x4000);
        Actor04000_Fn00E6C(arg1);
        work->field_6   = 0;
        work->field_47A = (u8)(work->field_47A + 1);
    }
    Actor04000_Fn00E6C(arg1);
    if (work->field_58 & 1) {
        work->field_0 = 4;
    }
    if ((s8)work->field_47A >= 4) {
        work->field_0 = 5;
    }
}

/// State 0 of the actor's per-frame dispatch: on the frame the state is
/// entered (`field_4` latch) it raises the enemy's list-node flag and the
/// display object's 0x80 bit, and clears the gate bits of the work block's
/// four collision objects (the high bit on three, 0x4000 on `obj270`).
void Actor04000_Fn06A5C(GpEnemy* enemy, Task* task)
{
    Actor104000Work* work;
    TmdObject*       obj;

    work = (Actor104000Work*)task->work;
    if (work->field_4 != 0) {
        obj                = task->extra;
        enemy->node.flags  = 1;
        obj->flags         = (u16)(obj->flags | 0x80);
        work->obj350.flags = (u16)(work->obj350.flags & 0x7FFF);
        work->obj388.flags = (u16)(work->obj388.flags & 0x7FFF);
        work->obj3C0.flags = (u16)(work->obj3C0.flags & 0x7FFF);
        work->obj270.flags = (u16)(work->obj270.flags & 0xBFFF);
    }
}

void Actor04000_Fn06AC4(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    obj  = arg1->extra;
    if (work->field_4 != 0) {
        arg0->node.flags    = 1;
        obj->flags          = 0x80;
        work->field_174     = 1;
        work->field_170     = 2;
        work->obj350.flags &= 0x7FFF;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj270.flags &= 0xBFFF;
        return;
    }
    Actor04000_Fn00E6C(arg1);
    switch (arg0->placeKey >> 12) {
        case 6:
        case 7:
            arg0->node.flags = 0;
            obj->flags       = 0;
            break;
        case 3:
        case 4:
        case 5:
        default:
            arg0->node.flags = 1;
            obj->flags       = 0x80;
            break;
    }
    if (Gp_StateF0.field_0 == 1) {
        work->field_0 = 7;
    }
}

void Actor04000_Fn06BC8(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                 = arg1->extra;
        arg0->node.flags    = 0;
        obj->flags          = 0;
        work->field_174     = 4;
        work->field_170     = 1;
        work->field_178     = 0;
        work->obj350.flags |= 0x8000;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj270.flags |= 0x4000;
        Actor04000_Fn00E6C(arg1);
        return;
    }
    Actor04000_Fn00E6C(arg1);
    if (work->field_58 & 1) {
        work->field_0 = 2;
    }
}

void Actor04000_Fn06C80(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                 = arg1->extra;
        arg0->node.flags    = 0;
        obj->flags          = 0;
        work->field_174     = 6;
        work->field_170     = 1;
        work->field_178     = 0;
        work->obj350.flags |= 0x8000;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj270.flags |= 0x4000;
        Actor04000_Fn00E6C(arg1);
        return;
    }
    Actor04000_Fn00E6C(arg1);
    if (work->field_58 & 1) {
        work->field_0 = 7;
    }
}

void Actor04000_Fn06D38(GpEnemy* arg0, Task* arg1)
{
    Actor104000Work* work;
    s16              angle;

    work = arg1->work;
    if (work->field_4 != 0) {
        ((TmdObject*)arg1->extra)->flags = 0;
        arg0->node.flags                 = 5;
        work->field_174                  = 1;
        work->field_170                  = 2;
        work->field_178                  = 0;
        work->obj350.flags              |= 0x8000;
        work->obj388.flags              &= 0x7FFF;
        work->obj3C0.flags              &= 0x7FFF;
        work->obj270.flags              |= 0x4000;
        Actor04000_Fn00E6C(arg1);
        angle = ratan2(-((TmdObject*)arg1->extra)->coords->coord.m[2][0], ((TmdObject*)arg1->extra)->coords->coord.m[2][2]);
        Gfx_RotMatrixZ(&((TmdObject*)arg1->extra)->coords->coord, 0x800, 1);
        Gfx_RotMatrixY(&((TmdObject*)arg1->extra)->coords->coord, angle, 0);
        ((TmdObject*)arg1->extra)->coords->flg = 0;
        return;
    }
    Actor04000_Fn00E6C(arg1);
}

/// The enemy's spawn, per-frame and teardown handlers, indexed by the task's
/// state.
const GpEnemyTaskFuncTable3 Actor04000_D00240 = {
    Actor04000_Fn010B8,
    Actor04000_Fn05F0C,
    Gp_DestroyEnemy,
};

/// The enemy task's callback: runs the handler for the task's current state,
/// copying the table onto the stack before the call.
void Actor04000_Fn06E4C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor04000_D00240;
    sp.funcs[task->state](task->spawnArg2, task);
}

/// The controller task's state handlers, indexed by its `state`.
extern TaskFunc Actor04000_D0C6EC[];

void Actor04000_Fn06EA8(Task* arg0)
{
    GpCmdArg msg;
    s16      i;

    for (i = 0; i < 6; i++) {
        Actor04000_D0C718[i] = NULL;
    }
    Actor04000_D0C710[1] = NULL;
    msg.from.loc.stage   = 3;
    msg.from.loc.area    = 0x10;
    Actor04000_D0C710[0] = NULL;
    msg.command          = 1;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
    arg0->state++;
}

void Actor04000_Fn06F54(Task* arg0)
{
    s16 i;

    if (Gp_StateF0.field_0 == 1) {
        for (i = 0; i < 6; i++) {
            if (Actor04000_D0C718[i] != NULL) {
                ((GpEnemy*)Actor04000_D0C718[i]->spawnArg2)->node.flags = 0;
            }
        }
        arg0->state++;
    }
    if (D_8007216C == 5) {
        for (i = 0; i < 6; i++) {
            if (Actor04000_D0C718[i] != NULL) {
                Gp_ArmStateF0(1);
                return;
            }
        }
    }
}

void Actor04000_Fn0703C(Task* arg0)
{
    Actor04000_D0C6EC[arg0->state](arg0);
}
