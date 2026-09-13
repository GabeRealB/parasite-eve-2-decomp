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

extern s32  Gp_LcgState;
extern s32* Gp_GridParams;
s16         Gp_GetIdParam2(s32);
s32         Gp_GetIdParam0(s32);
s32         Gp_GetIdParam1(s32);
s32         func_800E0C10(void*, void*, s32, void*);
u32         Gp_ComputeDamage(u32, u32, s32, s32);
s32         Gp_RollEnemyChance(void*, s32, s32);
void        func_800E2C78(void*, s32, s32, s32);
void        Gp_SetObjFlag1(void*);
void        Gp_SetObjFlag2(void*, s32, s32);
void        Gp_SetObjFlag4(void*, s32, s32);
void        Gp_ClearRec18Occupied(void*);
s32         Gp_FindRec18(void*, s32);
void        func_800FDB18(s32, void*, void*, void*);
void        Actor01600_Fn0646C(Actor01600*);

void Actor01600_Fn00BAC(Actor01600* actor)
{
    s32                    distance;
    Actor01600PlayerSlot** slots;
    void*                  world;
    Actor01600Work*        work;
    Actor01600Ctx*         ctx;
    GsCOORDINATE2*         coord;
    Actor01600Work*        rec;
    Actor01600HitScratch*  scratch;
    void*                  old;
    GsCOORDINATE2*         other;
    s32                    x, y, z;
    s32                    damage;
    s32                    amount;
    s32                    product;
    s32                    push;
    s32                    clamped;
    s32                    cx, cz;
    s16                    count;
    s32                    mode;
    VECTOR *               v1, *v2;
    work    = actor->field_1C;
    old     = *(void**)0x1F8003FC;
    scratch = (*(void**)0x1F8003FC = old - 0x4C);
    ctx     = actor->field_20;
    coord   = actor->field_2C->field_8;
    mode    = func_800E0C10(&work->collision.field_2EC[0x20], old - 0x2C, 8, old - 4);
    world   = (void*)coord + 0x50;
    if (mode == 1)
        goto mode1;
    if (mode < 2)
        goto mode_end;
    if (mode == 2)
        goto mode2;
    goto mode_end;
    {
    mode1:
        coord->coord.t[0] += scratch->delta.half.highx;
        coord->coord.t[1] += scratch->delta.half.highy;
        coord->coord.t[2] += scratch->delta.half.highz;
        goto mode_end;
    mode2:
        coord->coord.t[0] = work->field_4BC;
        coord->coord.t[1] = work->field_4C0;
        coord->coord.t[2] = work->field_4C4;
    }
mode_end:
    slots = Gp_ActorSlots;
    if (work->field_51C != 0) {
        if (--work->field_51C <= 0)
            work->field_51C = 0;
    }
    rec = work;
next_record: {
    switch (rec->collision.named.hit.parts.kind) {
        case 2:
            if (work->field_51C == 0) {
                other               = slots[rec->collision.named.hit.parts.byte0 >> 7]->field_2C->field_8;
                x                   = other->coord.t[0] - coord->coord.t[0];
                scratch->delta.v.vx = x;
                y                   = other->coord.t[1] - coord->coord.t[1];
                scratch->delta.v.vy = y;
                z                   = other->coord.t[2] - coord->coord.t[2];
                scratch->delta.v.vz = z;
                damage              = Gp_ComputeDamage(rec->collision.named.hit.id, SquareRoot0(x * x + y * y + z * z), 0, 0);
                if (Gp_RollEnemyChance(actor->field_20, rec->collision.named.hit.id, 0)) {
                    damage *= 4;
                    Gp_SpawnEff(0x6009C, (void*)actor->field_2C->field_8 + 0x50, 0, 0);
                }
                if (work->field_4FE == 1 && work->field_528 != 0 && work->field_51E < 0) {
                    damage *= 2;
                    Gp_SpawnEff(0x6009C, (void*)actor->field_2C->field_8 + 0x50, 3, 0);
                }
                func_800E2C78(ctx, rec->collision.named.hit.id, damage, 0);
                Actor01600_Fn0131C(actor, damage);
                count = Gp_GetIdParam2(rec->collision.named.hit.id);
                if (count > 0)
                    work->field_51C = count;
                switch (Gp_GetIdParam0(rec->collision.named.hit.id) & 0xFFFF) {
                    case 4:
                    case 6:
                        if ((s16)ctx->field_40 <= 0)
                            goto dead;
                        Gp_SetObjFlag1(actor->field_20);
                        break;
                    case 2:
                    case 9:
                        if (work->field_4FE != 3 && work->field_4FE != 5) {
                            Gp_SetObjFlag2(actor->field_20, rec->collision.named.hit.id, 0);
                            work->field_556 = 1;
                        }
                        break;
                    case 8:
                        if (work->field_4FE != 3 && work->field_4FE != 5) {
                            Gp_SetObjFlag2(actor->field_20, rec->collision.named.hit.id, 0);
                            work->field_556 = 0;
                        }
                        break;
                    case 1:
                    case 5:
                        if (work->field_4FE != 3 && work->field_4FE != 5)
                            Gp_SetObjFlag1(actor->field_20);
                        break;
                    case 0:
                        break;
                    case 3:
                        Gp_SetObjFlag4(actor->field_20, rec->collision.named.hit.id, 0);
                        break;
                }
                if (damage >= 40 && work->field_556 == 0) {
                    Gp_SetObjFlag1(ctx);
                    if (work->field_4FE == 0 && work->field_528 != 0) {
                        work->field_528 = 0;
                        work->field_51E = 0;
                        work->field_520 = 0;
                    }
                } else {
                    work->field_522 = 1;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_4CC = (((u32)Gp_LcgState >> 11) & 0x60) + 0x100;
                }
                if (work->field_4FE != 0 && work->field_528 != 0) {
                    work->field_3EA &= 0x7FFF;
                    Gp_ClearRec18Occupied(work->pad_3EC);
                    work->field_4FA  = 0;
                    work->field_51E += 20;
                    amount           = Actor01600_Fn045A8(actor, &distance);
                    if (amount < 0)
                        amount = -amount;
                    if (amount < 0x400) {
                        work->field_506 = 14;
                        work->field_50E = -40;
                    } else {
                        work->field_506 = 11;
                        work->field_50E = 40;
                    }
                    work->field_516 = 8;
                }
                func_800FDB18(Gp_GetIdParam1(rec->collision.named.hit.id) & 0xFFFF, world, 0, &work->pad_3EC[0x18]);
            }
            break;
        case 3:
            cx                  = coord->workm.t[0] - rec->collision.named.field_314;
            scratch->delta.v.vy = 0;
            scratch->delta.v.vx = cx;
            cz                  = coord->workm.t[2] - rec->collision.named.field_318;
            scratch->delta.v.vz = cz;
            push                = rec->collision.named.field_30E - SquareRoot0(cx * cx + cz * cz);
            clamped             = push;
            if (push <= 0)
                clamped = 0;
            push = clamped;
            SOFT_TOUCH_REG_USE(push, rec);
            SOFT_TOUCH_REG_USE(push, rec);
            SOFT_TOUCH_REG_USE(push, rec);
            v1                  = &scratch->delta.v;
            scratch->delta.v.vx = coord->workm.t[0] - rec->collision.named.field_314;
            SOFT_TOUCH_REG_USE(push, clamped);
            v2                  = &scratch->normal;
            scratch->delta.v.vy = coord->workm.t[1] - rec->collision.named.field_316;
            scratch->delta.v.vz = coord->workm.t[2] - rec->collision.named.field_318;
            VectorNormal(v1, v2);
            ApplyTransposeMatrixLV(*Gp_GridParams + 0x24, v2, v1);
            if (work->field_506 == 23 || work->field_506 == 5 || work->field_506 == 6) {
                coord->coord.t[0] += (push * scratch->delta.v.vx) >> 12;
                product            = push * scratch->delta.v.vy;
                if (product < 0)
                    coord->coord.t[1] += product >> 12;
                coord->coord.t[2] += (push * scratch->delta.v.vz) >> 12;
            }
            break;
        case 0:
        case 1:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
            break;
    }
}
    rec = (void*)rec + 0x18;
    if ((s32)rec < (s32)work + 0xC0)
        goto next_record;
    Gp_ClearRec18Occupied(&work->collision.field_2EC[0x20]);
    if (work->field_516 && Gp_FindRec18(work->pad_3EC, 0)) {
        work->collision.named.field_2FE = -400;
        work->collision.named.field_308 = 400;
        work->field_52A                 = 1;
        work->field_3EA                &= 0x7FFF;
        Gp_ClearRec18Occupied(work->pad_3EC);
        if (work->field_50A < 15) {
            work->field_516  = 8;
            work->field_4FA  = 0;
            work->field_51E += 20;
            amount           = Actor01600_Fn045A8(actor, &distance);
            if (amount < 0)
                amount = -amount;
            if (amount >= 0x400)
                goto far_angle;
            work->field_506 = 14;
            work->field_50E = -40;
            goto release;
        dead:
            work->field_540 = 0;
            Actor01600_Fn0646C(actor);
            work->field_540 = 2;
            work->field_528 = 0;
            return;
        far_angle:
            work->field_506 = 11;
            work->field_50E = 40;
        }
    }
release:
    *(void**)0x1F8003FC += 0x4C;
    return;
}

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

void Actor01600_Fn017BC(Actor01600* actor)
{
    Actor01600Ctx*  ctx;
    Actor01600Work* work;
    GsCOORDINATE2*  coord;
    TmdObject*      model;
    s16             frameOffset;
    s16             count;
    s16             tick;
    s16             state;
    s16             height;
    s16             repeatHeight;
    s16             frame;
    s32             contact;
    s32             id;
    void*           old;
    s32             pan8;
    s32             distance;
    GpRec18*        rec;
    s32             pan1;
    s32             pan2;
    s32             pan3;
    s32             pan4;
    s32             pan5;
    s32             pan6;
    s32             pan7;
    u16             flags;
    u16             attackFrame;

    work                = actor->field_1C;
    old                 = *(void**)0x1F8003FC;
    rec                 = &work->field_2D4;
    *(void**)0x1F8003FC = old - 8;
    model               = actor->field_2C;
    coord               = model->field_8;
    ctx                 = actor->field_20;
    if (Gp_CountRec18Hi(rec, 0x10000) != 0) {
        work->field_51A = 1;
    }
    if (work->field_51A != 0) {
        work->field_4FE  = 1;
        work->field_516  = 0;
        work->field_510  = 0;
        work->field_4FA  = 0;
        work->field_528  = 0;
        work->field_51E  = 0;
        work->field_520  = 0;
        work->field_50A  = 0;
        work->field_4EA  = 0;
        work->field_2BA &= 0x3FFF;
        work->field_42A &= 0x3FFF;
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(rec);
    if (work->field_53A == 1) {
        count           = (u16)work->field_536 - 1;
        work->field_536 = count;
        if (count == 0) {
            work->field_50A = 0;
            work->field_506 = 2;
            work->field_53A = 0;
        }
    }
    tick            = (u16)work->field_504 + 1;
    work->field_504 = tick;
    if (tick >= 0x1F) {
        work->field_504 = 0;
    }
    state = (u16)work->field_506 - 1;
    switch (state) {
        case 0:
            work->field_2C0 = 0x3E8;
            work->field_50E = 0;
            work->field_538 = 0x10;
            if (work->field_50A >= 0x3E) {
                work->field_50A = 0;
            }
            if (ctx->field_3C->field_2 == 0) {
                if (((u8)Gp_StateF0.flags.bytes[2] & 1) || (Gp_StateF0.flags.word & 0xFF140000)) {
                    work->field_53A = 1;
                }
            }
            break;
        case 1:
            work->field_2C0 = 0xFA0;
            work->field_50E = 0;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            if (work->field_50A >= 0x36) {
                work->field_506  = 0x19;
                work->field_50A  = 0;
                work->field_4EA  = 0;
                work->field_514  = 1;
                work->field_42A |= 0xC000;
            }
            break;
        case 24:
            work->field_50E = 0;
            work->field_538 = 0x10;
            work->field_4FA = 0;
            contact         = Actor01600_Fn04EB0(actor) & 0xFF;
            if (contact != 0) {
                flags           = work->field_42A & 0x3FFF;
                work->field_42A = flags;
                if (contact != 0xFF) {
                    distance        = work->field_4E0;
                    work->field_548 = 0U;
                    distance        = abs(distance);
                    work->field_50A = 0;
                    if ((distance >= 0x201) || ((contact & 0xF) == 2)) {
                        work->field_510 = 5;
                        if ((contact & 0xF0) == 0x80) {
                            work->field_506 = 7;
                        } else {
                            work->field_506 = 8;
                        }
                    } else {
                        work->field_510 = 0;
                        work->field_506 = 4;
                    }
                } else {
                    work->field_42A = flags | 0xC000;
                    work->field_4EA = 0;
                }
            }
            if (work->field_50A >= 0x57) {
                work->field_50A = 0;
                work->field_508 = 0;
                work->field_506 = 0x19;
            }
            break;
        case 2:
            work->field_4FA = 0;
            work->field_538 = 0x10;
            work->field_50E = 0;
            if (work->field_50A >= 0x3D) {
                work->field_50A = 0;
                work->field_506 = 3;
            }
            break;
        case 3:
            work->field_538 = 0x10;
            work->field_4FA = 4;
            if (work->field_528 != 0) {
                if (work->field_50A >= 0xC) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0xA;
                }
                height          = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = height;
                if (height >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            if (work->field_50A == 6) {
                work->field_528 = 1;
                work->field_51E = -0x50;
                work->field_520 = (u16)work->field_520 - 0x50;
            }
            attackFrame = (u16)work->field_50A;
            if ((u32)(attackFrame - 5) < 0x10U) {
                if ((s16)attackFrame >= 0xC) {
                    work->field_50E = 0x5A;
                } else {
                    work->field_50E = 0x3C;
                }
                Actor01600_Fn03A60(actor);
            } else {
                work->field_50E = 0;
            }
            if (work->field_50A == 0x14) {
                id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100002;
                pan1 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan1, (s8)Gp_GetObjDepth(coord));
            }
            if (work->field_50A >= 0x15) {
                id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100001;
                pan2 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan2, (s8)Gp_GetObjDepth(coord));
                work->field_506 = 0x17;
                work->field_528 = 0;
                work->field_51E = 0;
                work->field_520 = 0;
                work->field_50C = 0;
                work->field_50A = 0;
                work->field_548 = (u16)(work->field_548 + 1);
                work->field_2BA = (work->field_2BA | 0x8000) & 0xBFFF;
            }
            break;
        case 22:
            work->field_538 = 0x10;
            if (work->field_528 != 0) {
                if (work->field_50A >= (work->field_50C + 0xC)) {
                    work->field_51E = (u16)work->field_51E + 0xF;
                } else {
                    work->field_51E = (u16)work->field_51E + 0xA;
                }
                repeatHeight    = (u16)work->field_520 + (u16)work->field_51E;
                work->field_520 = repeatHeight;
                if (repeatHeight >= 0) {
                    work->field_528 = 0;
                    work->field_51E = 0;
                    work->field_520 = 0;
                }
            }
            if (work->field_50A == (work->field_50C + 6)) {
                work->field_528 = 1;
                work->field_51E = -0x50;
                work->field_520 = (u16)work->field_520 - 0x50;
            }
            frameOffset = work->field_50C;
            frame       = work->field_50A;
            if ((frame >= (frameOffset + 6)) && ((frameOffset + 0x15) >= frame)) {
                if ((frameOffset + 0xA) >= frame) {
                    work->field_50E = 0x5A;
                } else {
                    work->field_50E = 0x3C;
                }
                Actor01600_Fn03A60(actor);
            } else {
                work->field_50E = 0;
            }
            if (work->field_50A == (work->field_50C + 0x14)) {
                id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100002;
                pan3 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan3, (s8)Gp_GetObjDepth(coord));
            }
            if (work->field_50A >= (work->field_50C + 0x17)) {
                id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100001;
                pan4 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(id, (s32)pan4, (s8)Gp_GetObjDepth(coord));
                work->field_548 = (u16)(work->field_548 + 1);
                work->field_4FA = 0;
                work->field_528 = 0;
                work->field_51E = 0;
                work->field_520 = 0;
                work->field_514 = 0;
                work->field_50A = 0;
                work->field_50C = -3;
                if ((s16)work->field_548 >= 3) {
                    work->field_506 = 0x18;
                } else {
                    work->field_506 = 0x17;
                }
                work->field_2BA = (work->field_2BA | 0x8000) & 0xBFFF;
            }
            break;
        case 23:
            work->field_538 = 0x10;
            work->field_4FA = 4;
            work->field_50E = 0;
            if (work->field_50A >= 0xA) {
                work->field_506  = 0x19;
                work->field_514  = 1;
                work->field_51A  = 0;
                work->field_4EA  = 0;
                work->field_42A |= 0xC000;
                work->field_2BA  = (work->field_2BA | 0x8000) & 0xBFFF;
            }
            break;
        case 6:
        case 7:
            work->field_538 = 0x10;
            work->field_4FA = 0;
            if (work->field_506 == 7) {
                if (work->field_50A == 0xF) {
                    id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100003;
                    pan5 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan5, (s8)Gp_GetObjDepth(coord));
                }
                if (work->field_50A == 0x11) {
                    id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100004;
                    pan8 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan8, (s8)Gp_GetObjDepth(coord));
                }
            } else {
                if (work->field_50A == 0xF) {
                    id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100004;
                    pan6 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan6, (s8)Gp_GetObjDepth(coord));
                }
                if (work->field_50A == 0x12) {
                    id   = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40100003;
                    pan7 = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, (s32)pan7, (s8)Gp_GetObjDepth(coord));
                }
            }
            work->field_50E = 0;
            if ((u32)((u16)work->field_50A - 4) < 0x11U) {
                Actor01600_Fn03A60(actor);
            }
            if (work->field_50A >= 0x1C) {
                work->field_508  = 0;
                work->field_50A  = 0;
                work->field_2BA |= 0x8000;
                if (work->field_510 == 7) {
                    work->field_514 = 0;
                    work->field_50A = 0;
                    work->field_506 = 4;
                }
            }
            break;
        case 9:
            work->field_4FA = 0;
            work->field_538 = 0x10;
            work->field_50E = 0;
            if (work->field_50A >= 0x28) {
                work->field_51A = 1;
                work->field_50A = 0;
                work->field_506 = 4;
            }
            break;
        default:
            break;
    }
    *(void**)0x1F8003FC += 8;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101600_text", Actor01600_Fn020F8);
