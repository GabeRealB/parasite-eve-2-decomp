#include "common.h"

#include "actors/actor_101600.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include <psyq/abs.h>

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn001F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn00480);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn00674);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn00A4C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn00BAC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn0131C);

void Actor01600_Fn017BC(Actor01600* arg0);
void Actor01600_Fn020F8(Actor01600* arg0);
void Actor01600_Fn06744(Actor01600* arg0);
void Actor01600_Fn06F10(Actor01600* arg0);
s32  Gp_TickObjFlag2(void* arg0);
s32  Gp_GetObjPan(void* arg0);
s32  Gp_GetObjDepth(void* arg0);

extern s32 Gp_LcgState;

/// Per-frame tick for the actor's cornered/pursuit cycle, dispatched on
/// `field_4FE`. States 0 and 1 hand the frame to `Actor01600_Fn017BC` /
/// `Actor01600_Fn020F8` and then run the shared post-step
/// `Actor01600_Fn06744`. State 2 advances `field_504`, nudges `field_50E` back
/// by 0x3C while the animation is still 0xE and under 0x11 frames in, arms the
/// 0x16 animation at frame 0x28 and, past frame 0x5B, resets to animation 0x19
/// with the 0x8000 bit set in `field_2BA`. State 3 plays animation 0x13 until
/// `Gp_TickObjFlag2` fires. State 4 only selects animation 0x11. State 5 rolls
/// the 0x4CC swerve offset once per approach (animation 0xE, past frame 0x2C,
/// `field_522` still clear and bit 1 of `field_50A` set) and, on animation
/// 0x16 past frame 0x32, ends the cycle the same way state 2 does.
///
/// Whatever the state, animations 1/9/0x10/0x13/0x15/0x16/0x1B..0x1E are
/// silent; the rest count `field_542` down and, on expiry, play one of three
/// growls (`0x4010_0006..8`) picked by a `Gp_LcgState` draw modulo 5 - two of
/// the five outcomes stay quiet - panned and attenuated for the actor's
/// coordinate, then rearm the counter at 0x14.
void Actor01600_Fn01420(Actor01600* arg0)
{
    Actor01600Work* work;
    GsCOORDINATE2*  coord;
    s32             id;
    s32             state;
    s32             one;
    u16             sel;
    s16             count;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;

    switch (work->field_4FE) {
        case 0:
            Actor01600_Fn017BC(arg0);
            goto tick;
        case 1:
            Actor01600_Fn020F8(arg0);
            goto tick;
        case 2:
            Actor01600_Fn06F10(arg0);
            work->field_504 = work->field_504 + 1;
            if (work->field_506 == 0xE && work->field_50A < 0x11) {
                work->field_50E = -0x3C;
                Actor01600_Fn06744(arg0);
            } else {
                work->field_50E = 0;
            }
            if (work->field_504 == 0x28) {
                work->field_538 = 0x10;
                work->field_506 = 0x16;
            }
            if (work->field_504 >= 0x5B) {
                work->field_506  = 0x19;
                work->field_4FE  = 0;
                work->field_504  = 0;
                work->field_2BA |= 0x8000;
            }
            goto clear;
        case 3:
            Actor01600_Fn06F10(arg0);
            work->field_506 = 0x13;
            if (Gp_TickObjFlag2(arg0->field_20) != 0) {
                work->field_4FE  = 0;
                work->field_506  = 0x19;
                work->field_2BA |= 0x8000;
            }
            goto clear;
        case 4:
            work->field_506 = 0x11;
            break;
        case 5:
            Actor01600_Fn06F10(arg0);
            if (work->field_506 == 0xE) {
                if (work->field_50A >= 0x2C && work->field_522 == 0 &&
                    ((u16)work->field_50A & 2)) {
                    work->field_522 = 1;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_4CC = (((u32)Gp_LcgState >> 11) & 0x60) + 0x20;
                }
            } else if (work->field_506 == 0x16) {
                if (work->field_50A >= 0x32) {
                    work->field_4FE  = 0;
                    work->field_506  = 0x19;
                    work->field_2BA |= 0x8000;
                }
            }
            if (Gp_TickObjFlag2(arg0->field_20) != 0) {
                work->field_506 = 0x16;
            }
            goto clear;
    }
    goto tail;

clear:
    work->field_50E = 0;
tick:
    Actor01600_Fn06744(arg0);

tail:
    state = work->field_506;
    one   = 1;
    if (state == one || state == 0x16 || state == 0x15 || state == 0x10 ||
        state == 0x13 || state == 0x1C || state == 0x1D || state == 0x1E ||
        state == 0x1B || state == 9) {
        return;
    }
    count           = work->field_542 - 1;
    work->field_542 = count;
    if (count != 0) {
        return;
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    sel         = ((u32)Gp_LcgState >> 16) % 5;
    switch (sel) {
        case 0:
            id = ((arg0->field_20->field_8 >> 12) << 8) | 0x40100006;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
            break;
        case 1:
            id = ((arg0->field_20->field_8 >> 12) << 8) | 0x40100007;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
            break;
        case 2:
            id = ((arg0->field_20->field_8 >> 12) << 8) | 0x40100008;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
            break;
    }
    work->field_542 = 0x14;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn017BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn020F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn03A60);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn03D48);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn03EEC);

void  Gp_UnlinkNode(void* node);
void  Gp_UnlinkObj(void* node);
void  Gp_EnemyTaskExit(Actor01600* arg0);
void  Gp_SetLightMode(void* arg0, s32 arg1);
void  Gp_ReleaseStateF0Add(void* arg0, s32 arg1);
void  Gp_PulseState1C(void);
void  Gp_UpdateCoord(GsCOORDINATE2* arg0);
void  Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void* Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, void* arg3);
void  Gp_DispatchMsg(void* arg0, s32 arg1, void* arg2, s32 arg3);

void Actor01600_Fn03D48(Actor01600* arg0);
s32  Actor01600_Fn045A8(Actor01600* arg0, SVECTOR* arg1);
void Actor01600_Fn06880(Actor01600* arg0);
void Actor01600_Fn06EA4(Actor01600* arg0);
void Actor01600_Fn06F10(Actor01600* arg0);
u8   Actor01600_Fn06F78(Actor01600* arg0);

extern u8 D_80071075;

/// Takes a 0x10-byte `VECTOR` from `G_SCRATCH_HEAD`, fills it with `attach`'s
/// world position and hands it to `Gp_UpdateActorColor`. Inlined so the
/// scratch-head address is rematerialised on every access.
static __inline__ void update_actor_color(Actor01600Ctx* ctx, GsCOORDINATE2* attach)
{
    u8*     head;
    VECTOR* block;

    head  = *(u8**)G_SCRATCH_HEAD;
    block = (VECTOR*)(head - 0x10);

    *(VECTOR**)G_SCRATCH_HEAD = block;

    block->vx = attach->workm.t[0];
    block->vy = attach->workm.t[1];
    block->vz = attach->workm.t[2];
    Gp_UpdateActorColor(ctx, block, 0, 0);

    *(u8**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void Actor01600_Fn04054(Actor01600Ctx* arg0, Actor01600* arg1)
{
    Actor01600Work*     work;
    Actor01600Obj2C*    obj;
    GsCOORDINATE2*      coords;
    GsCOORDINATE2*      attach;
    GsCOORDINATE2*      body;
    Actor01600Work*     w;
    Actor01600StateF0*  state;
    Actor01600StateC08* pad;
    SVECTOR             aim;
    s32                 dist;
    s32                 anim;
    s16                 phase;
    s16                 timer;
    s16                 count;
    s32                 mode;
    /* Both calls below take the task; pinning it to the argument register
       emits the one shared `move $a0, $s2` the branch delay slot uses. */
    register Actor01600* task asm("a0");

    obj    = arg1->field_2C;
    work   = arg1->field_1C;
    coords = obj->field_8;
    mode   = Gp_StateF0.field_4;
    if (mode == 1) {
        return;
    }
    if (mode > 1) {
        if (mode == 2) {
            obj->field_C      |= 0x80;
            arg0->node.field_4 = 1;
            return;
        }
    }
    switch (work->field_502) {
        case 0:
            Actor01600_Fn06F10(arg1);
            if (work->field_540 != 2) {
                anim            = work->field_4FE;
                work->field_4FA = 0;
                work->field_538 = 0x14;
                if (anim != 2 && anim != 5) {
                    dist = Actor01600_Fn045A8(arg1, &aim);
                    if (dist < 0) {
                        dist = -dist;
                    }
                    work->field_506 = (dist < 0x400) ? 0xE : 0xB;
                }
                work->field_504 = 0;
                work->field_518 = 0x1000;
                work->field_49C = coords[0].coord;
                Gp_SetLightMode(arg0, 1);
            }
            arg0->node.field_4 = 1;
            arg0->field_54     = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(work->field_40C);
            Gp_UnlinkObj(work->field_29C);
            Gp_UnlinkObj(work->field_2EC);
            Gp_UnlinkObj(work->field_3CC);
            state = &Gp_StateF0;
            task  = arg1;
            if (state->field_1C >= 3) {
                if (Actor01600_Fn06F78(task) == 1) {
                    state->field_1C = state->field_1C + 1;
                }
            } else {
                Gp_ReleaseStateF0Add(task, 0x10);
            }
            work->field_502 = 1;
            break;
        case 1:
            Actor01600_Fn06880(arg1);
            phase           = work->field_504 + 1;
            work->field_504 = phase;
            if (work->field_540 != 2) {
                if (phase == 0xA) {
                    obj->field_C = 2;
                }
                if (work->field_504 == 0xF) {
                    Gp_SpawnEff(0x600A5, coords, 1, NULL);
                }
                if (work->field_504 < 0x10) {
                    body = arg1->field_2C->field_8;
                    w    = arg1->field_1C;

                    w->field_4BC = body->coord.t[0];
                    w->field_4C0 = body->coord.t[1];
                    w->field_4C4 = body->coord.t[2];

                    body->coord.t[0] += (body->coord.m[0][2] * w->field_50E) >> 12;
                    body->coord.t[2] += (body->coord.m[2][2] * w->field_50E) >> 12;
                    if (w->field_530 == 0) {
                        if (w->field_528 != 0) {
                            body->coord.t[1] += w->field_51E;
                        } else {
                            body->coord.t[1] += 0x80;
                        }
                    }
                }
            }
            if (work->field_504 >= 0x3C) {
                work->field_502 = 2;
            }
            break;
        case 2:
            if (Gp_StateF0.field_1C >= 3) {
                if (Actor01600_D12874 == 1) {
                    pad = &Gp_StateC08;
                    if (pad->field_A == 1) {
                        break;
                    }
                    if (D_80071075 != 0) {
                        break;
                    }
                    pad->field_6 |= 1;
                    Gp_PulseState1C();
                    Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F4, arg1, 0);
                    work->field_502          = 0xFF;
                    arg1->field_2C->field_C |= 0x80;
                    arg1->field_2C->field_C |= 4;
                    break;
                }
                Gp_ReleaseStateF0Add(arg1, 0x10);
            }
            work->field_54E          = 0x3C;
            arg1->field_2C->field_C |= 0x80;
            Actor01600_D12874--;
            arg1->field_2C->field_C |= 4;
            work->field_502          = 3;
            break;
        case 3:
            timer           = work->field_54E - 1;
            work->field_54E = timer;
            if (timer == 0) {
                Actor01600_Fn06EA4(arg1);
            }
            return;
        default:
            return;
    }

    Actor01600_Fn03D48(arg1);
    coords->flg = 0;
    Gp_UpdateCoord(coords);
    count           = work->field_550 + 1;
    work->field_550 = count;
    if (count < 5 && Game_Session->field_52 != 1) {
        return;
    }
    work->field_550 = 0;

    attach = &arg1->field_2C->field_8[1];
    update_actor_color(arg0, attach);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn045A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn047A0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn04974);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn04AD8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn04C64);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn04EB0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn052C4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn05400);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn05558);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn05B08);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn05F80);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn0646C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn066E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn06744);

void Actor01600_L06808(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn06810);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn06880);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn06974);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn06A84);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn06C1C);

/// Column 2 of the attachment matrix goes to `dir`; the trailing `SVECTOR`
/// is never read but owns the second half of the stack local block.
s32 Actor01600_Fn06C94(Actor01600* arg0, s32 arg1)
{
    SVECTOR         dir;
    SVECTOR         unused;
    Actor01600Work* work;
    s32             ang;
    s32             half;
    s32             res;

    work = arg0->field_1C;
    if (ABS(arg1) < 0x301) {
        return 0;
    }
    work->field_4E0 = arg1;
    Gfx_MatrixCol2(&arg0->field_2C->field_8->coord, &dir);
    ratan2(dir.vx, dir.vz);
    ang  = ABS(work->field_4E0);
    half = 0x1000 - ang;
    if (half < ang) {
        work->field_516 = 3;
        res             = half;
    } else {
        work->field_516 = 2;
        res             = ang;
    }
    work->field_4F0 = res / 16;
    if (work->field_4F0 < 0x20) {
        work->field_4F0 = 0x20;
    }
    work->field_50A = 0;
    work->field_510 = 5;
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn06D74);

/// Tears the actor down: flags its context node dead (`field_4` = 1), clears
/// `field_54`, unlinks the node from its list and releases the four display
/// objects held in the work block, then hands the task to `Gp_EnemyTaskExit`.
void Actor01600_Fn06EA4(Actor01600* arg0)
{
    Actor01600Ctx*  ctx;
    Actor01600Work* work;

    ctx  = arg0->field_20;
    work = arg0->field_1C;

    ctx->node.field_4 = 1;
    ctx->field_54     = 0;
    Gp_UnlinkNode(&ctx->node);
    Gp_UnlinkObj(work->field_40C);
    Gp_UnlinkObj(work->field_29C);
    Gp_UnlinkObj(work->field_2EC);
    Gp_UnlinkObj(work->field_3CC);
    Gp_EnemyTaskExit(arg0);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn06F10);

/// Walks the sibling ring of task slot 4's children and reports whether any of
/// them has already been flagged `0x80` in its `field_2C` object. Returns 0xFF
/// when the slot has no children at all, 1 on the first flagged sibling and 0
/// when the whole ring is clean. The task argument is unused.
u8 Actor01600_Fn06F78(Actor01600* arg0)
{
    Actor01600* head;
    Actor01600* iter;

    head = ((Actor01600*)Game_GetPtrSlot(4))->field_C;
    if (head == NULL) {
        return 0xFF;
    }
    iter = head;
    do {
        if (iter->field_2C->field_C & 0x80) {
            return 1;
        }
        iter = iter->field_10;
    } while (iter != head);
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn06FDC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn070AC);

s32 Actor01600_Fn07100(Actor01600* arg0)
{
    arg0->field_1C->field_554 = 6;
    return 0;
}
