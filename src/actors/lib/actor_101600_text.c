#include "common.h"

#include "actors/actor_101600.h"
#include "gameplay/3CD8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include <psyq/abs.h>
#include <psyq/inline_c.h>

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn001F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn00480);

void Actor01600_Fn00480(Actor01600* arg0);
void Actor01600_Fn00A4C(Actor01600* arg0);
void Actor01600_Fn0131C(Actor01600* arg0, s32 damage);
void Actor01600_Fn00BAC(Actor01600* arg0);
void Actor01600_Fn03D48(Actor01600* arg0);
void Actor01600_Fn03EEC(Actor01600* arg0);
void Actor01600_Fn04AD8(Actor01600* arg0);
s32  Actor01600_Fn05558(Actor01600* arg0);
void Actor01600_Fn06810(Actor01600Ctx* arg0, Actor01600* arg1);
void Actor01600_Fn06A84(Actor01600* arg0);
void Actor01600_Fn06F10(Actor01600* arg0);
void Actor01600_Fn06FDC(Actor01600* arg0, s32 arg1);
s32  Gp_TickObjFlag4(Actor01600Ctx* arg0);
s32  Gp_ObjFlag4Expired(Actor01600Ctx* arg0);
s32  Gp_GetObjPan(void* arg0);
s32  Gp_GetObjDepth(void* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

extern u8 D_801153F4;

void Actor01600_Fn00674(Actor01600Ctx* arg0, Actor01600* arg1)
{
    Actor01600Work* work;
    GsCOORDINATE2*  coord;
    TmdObject*      obj;
    s32             id;
    s32             map;
    u16             count;

    work  = arg1->field_1C;
    coord = arg1->field_2C->field_8;
    if (!(Actor01600_Fn05558(arg1) & 0xFF)) {
        switch (D_801153F4) {
            case 0:
                arg1->field_2C->field_C = 0;
                arg0->node.field_4      = 0;
                break;
            case 1:
                Actor01600_Fn06810(arg0, arg1);
                goto update;
            case 2:
                obj                = arg1->field_2C;
                obj->field_C      |= 0x80;
                arg0->node.field_4 = 1;
                return;
            default:
                break;
        }
        Actor01600_Fn04AD8(arg1);
        if (arg0->field_4C != 0) {
            Actor01600_Fn00A4C(arg1);
        }
        Actor01600_Fn00BAC(arg1);
        if (work->field_528 == 0) {
            if (work->field_500 != 0) {
                arg1->field_30 = 2;
            }
        }
        map = *(u32*)&Game_Session->field_4 & 0xFFFF0000;
        if (map != 0x3260000 && map != 0x4070000 && map != 0x4010000) {
            if (coord->coord.t[1] >= 0x65) {
                coord->coord.t[1] = -0xA;
            }
        }
        if (((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x031D0000) && (coord->coord.t[1] >= -0x3E7)) {
            id = (((u16)arg1->field_20->field_8 >> 0xC) << 8) | 0x40100005;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
            id = (((u16)arg1->field_20->field_8 >> 0xC) << 8) | 0x4010000A;
            SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
            Actor01600_Fn06F10(arg1);
            Actor01600_Fn06FDC(arg1, 0);
        }
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x04010000) {
            if (coord->coord.t[1] > 0) {
                work->field_532 = 1;
            }
            if (coord->coord.t[1] >= 0x3E9) {
                id = (((u16)arg1->field_20->field_8 >> 0xC) << 8) | 0x40100005;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
                id = (((u16)arg1->field_20->field_8 >> 0xC) << 8) | 0x4010000A;
                SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
                Actor01600_Fn06F10(arg1);
                Actor01600_Fn06FDC(arg1, 0);
            }
        }
        Actor01600_Fn01420(arg1);
        Actor01600_Fn03D48(arg1);
        Actor01600_Fn06A84(arg1);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        count           = work->field_550 + 1;
        work->field_550 = count;
        if (((s16)count >= 5) || (Game_Session->field_52 == 1)) {
            work->field_550 = 0;
            Actor01600_Fn06810(arg0, arg1);
        }
        if (work->field_532 == 0) {
        update:
            Actor01600_Fn03EEC(arg1);
        }
    }
}

void Actor01600_Fn00A4C(Actor01600* arg0)
{
    Actor01600Ctx*  ctx;
    Actor01600Work* work;
    s16             state;
    s32             damage;
    u8              flags2;
    u8              flags1;

    ctx    = arg0->field_20;
    flags1 = ctx->field_4C;
    work   = arg0->field_1C;
    if ((flags1 & 1) && (work->field_528 == 0)) {
        ctx->field_4C   = flags1 & 0xFE;
        work->field_4FE = 2;
        work->field_538 = 0x14;
        work->field_504 = 0;
        work->field_506 = 0xE;
    }
    flags2 = ctx->field_4C;
    if ((flags2 & 2) && (work->field_4FE != 2) && (work->field_528 == 0)) {
        ctx->field_4C   = flags2 & 0xFD;
        work->field_4FE = 3;
        if (work->field_556 != 0) {
            work->field_506 = 0xE;
            work->field_4FE = 5;
        }
        work->field_504 = 0;
    }
    if ((ctx->field_4C & 0xC) && (work->field_528 == 0)) {
        Actor01600_Fn06F10(arg0);
        damage = Gp_TickObjFlag4(ctx);
        if (damage != 0) {
            state = work->field_4FE;
            if ((state != 2) && (state != 5)) {
                work->field_4FE = 0;
                work->field_506 = 0xA;
            }
            Actor01600_Fn0131C(arg0, damage);
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->field_4C &= 0xF3;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn00BAC);

void func_800DA6E8(void* arg0, s32 arg1, s32 arg2);

void Actor01600_Fn0131C(Actor01600* arg0, s32 damage)
{
    s32             id;
    s32             pan;
    Actor01600Ctx*  ctx;
    Actor01600Work* work;
    GsCOORDINATE2*  coord;

    ctx           = arg0->field_20;
    work          = arg0->field_1C;
    coord         = arg0->field_2C->field_8;
    ctx->field_40 = (u16)(ctx->field_40 - damage);
    func_800DA6E8(&ctx->node, damage, 0);
    work->field_536 = 1;
    if ((s16)ctx->field_40 <= 0) {
        work->field_500  = 1;
        work->field_502  = 0;
        work->field_3EA &= 0x7FFF;
        id               = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x4010000A;
        SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan(coord), (s8)Gp_GetObjDepth(coord));
    } else {
        if (work->field_506 == 1) {
            work->field_506 = 0xA;
            work->field_50A = 0;
        }
        work->field_50E = 0;
        work->field_51A = 0;
        id              = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40100009;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth(coord));
    }
}

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

void Actor01600_Fn03D48(Actor01600* arg0)
{
    Actor01600Work* work;
    GsCOORDINATE2*  coord;
    s16             anim;
    s32             i;

    work = arg0->field_1C;

    if (work->field_506 != 0xFF) {
        if (work->field_506 != work->field_508) {
            work->field_508 = (s16)(u16)work->field_506;
            work->field_50A = 0;
            for (i = 1; i < 9; i++) {
                func_800B4114(&work->anim, i, work->field_506, 0, (s32)work->field_4FA);
            }
        } else {
            work->field_50A = (u16)work->field_50A + 1;
            for (i = 1; i < 9; i++) {
                work->slots[i].field_9 = (u8)work->field_538;
                Gp_AnimTickIndex(&work->anim, i);
            }
        }
        anim = work->field_506;
        if (anim == 28 || anim == 30 || anim == 9 || anim == 21 || anim == 22 || anim == 5 || anim == 6 || anim == 27) {
            arg0->field_2C->field_8[1].coord.t[0] = 0;
            arg0->field_2C->field_8[1].coord.t[2] = 0;
            arg0->field_2C->field_8[1].coord.t[0] = 0;
            arg0->field_2C->field_8[1].coord.t[2] = 0;
            return;
        } else if (anim == 29) {
            arg0->field_2C->field_8[1].coord.t[0] = 0;
            coord                                 = arg0->field_2C->field_8;
            coord[1].coord.t[2]                   = (s32)(coord[1].coord.t[2] - 0x2BC);
        }
    }
}

void Actor01600_Fn03EEC(Actor01600* arg0)
{
    VECTOR3                  pos;
    Actor01600GroundScratch* head;
    Actor01600GroundScratch* scratch;
    Actor01600Work*          work;
    GsCOORDINATE2*           coord;
    s32                      worldZ;
    s32                      height;
    s32                      mode;
    s32                      z;
    Actor01600GroundScratch* allocated;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    if (work->field_526 == 0) {
        head                                   = *(Actor01600GroundScratch**)0x1F8003FC;
        allocated                              = head - 1;
        *(Actor01600GroundScratch**)0x1F8003FC = allocated;
        mode                                   = *(s16*)((u8*)work + OFFSET_OF(Actor01600Work, field_528));
        do {
            scratch = allocated;
            if (mode != 0) {
                gte_SetRotMatrix(&coord->workm);
                scratch->offset.vx = 0;
                height             = work->field_520 - 0x80;
                scratch->offset.vz = 0;
                scratch->offset.vy = -height;
                gte_ldv0(&scratch->offset);
                __asm__ volatile("nop; nop; .word 0x4A486012");
                gte_stlvnl(&scratch->pos);
                head[-1].pos.vx = (s32)(head[-1].pos.vx + coord->workm.t[0]);
                scratch->pos.vy = (s32)(scratch->pos.vy + coord->workm.t[1]);
                SCHED_BARRIER();
                z = scratch->pos.vz;
                TOUCH_REG(z);
                worldZ = z + coord->workm.t[2];
            } else {
                head[-1].pos.vx = (s32)coord->workm.t[0];
                scratch->pos.vy = (s32)coord->workm.t[1];
                worldZ          = coord->workm.t[2];
            }
            scratch->pos.vz = worldZ;
        } while (0);
        Gp_DrawEffGroundQuad(&scratch->pos, 0x1C0, 0);
        *(Actor01600GroundScratch**)0x1F8003FC += 1;
        return;
    }
    if (func_800EA1A8((VECTOR3*)coord[1].workm.t, &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x1C0, Gp_State1C->field_8);
    }
}

void Gp_UnlinkNode(void* node);
void Gp_UnlinkObj(void* node);
void Gp_EnemyTaskExit(Actor01600* arg0);
void Gp_SetLightMode(void* arg0, s32 arg1);
void Gp_ReleaseStateF0Add(void* arg0, s32 arg1);
void Gp_PulseState1C(void);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
s32  Gp_DispatchMsg(void* arg0, s32 arg1, void* arg2, s32 arg3);

void Actor01600_Fn03D48(Actor01600* arg0);
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
    TmdObject*          obj;
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
                    dist = Actor01600_Fn045A8(arg1, (s32*)&aim);
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

s32 Actor01600_Fn047A0(Actor01600* arg0)
{
    SVECTOR3        delta;
    s32             distance;
    Actor01600Work* work;
    GsCOORDINATE2*  coord;
    Task*           task;
    s16             angle;
    s16             heading;
    s32             difference;
    GsCOORDINATE2*  other;

    work  = arg0->field_1C;
    task  = work->field_4D4;
    other = ((TmdObject*)task->extra)->field_8;
    coord = arg0->field_2C->field_8;
    if (Gp_ActorSlots[work->field_53E]->actor->field_954 != 2) {
        if (Actor01600_D12870 != 1) {
            difference = Actor01600_Fn045A8(arg0, &distance);
            if (difference < 0) {
                difference = -difference;
            }
            if (difference < 0x401) {
                if (distance < 0x3E8) {
                    Actor01600_D12878.field_14 = 5;
                    if (work->field_53E != 0) {
                        Actor01600_D12878.field_4 = 1;
                    } else {
                        Actor01600_D12878.field_4 = 2;
                    }
                    if (Gp_DispatchMsg(task, 0x3F8, &Actor01600_D12878, 0) == 0) {
                        other->flg = 0;
                        delta.vx   = coord->coord.t[0] - other->coord.t[0];
                        delta.vy   = 0;
                        delta.vz   = coord->coord.t[2] - other->coord.t[2];
                        angle      = ratan2(delta.vx, delta.vz);
                        heading    = angle;
                        if (angle >= 0x801) {
                            heading = angle - 0x1000;
                        } else if (angle < -0x800) {
                            heading = angle + 0x1000;
                        }
                        Actor01600_D12890.rotation.vx = 0;
                        Actor01600_D12890.rotation.vy = heading;
                        Actor01600_D12890.rotation.vz = 0;
                        Actor01600_D12890.position.vx = (s32)other->coord.t[0];
                        Actor01600_D12890.position.vy = (s32)other->coord.t[1];
                        Actor01600_D12890.position.vz = (s32)other->coord.t[2];
                        Gp_DispatchMsg(task, 0x3E9, &Actor01600_D12890, 0);
                        Actor01600_D12870 = 1;
                        return 1;
                    }
                    return 0;
                }
                return 0;
            }
            return 0;
        }
        return 0;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn04974);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn04AD8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn04C64);
