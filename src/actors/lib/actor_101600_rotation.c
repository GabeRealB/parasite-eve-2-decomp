#include "common.h"

#include "actors/actor_101600.h"
#include "gameplay/3CD8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include <psyq/abs.h>
#include <psyq/inline_c.h>

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

/// `Gp_ClearRec18Occupied` must stay undeclared here: an implicit declaration
/// is what makes `Actor01600_Fn04C64`'s call a value-returning one, the way
/// retail compiled it. Declaring it `void` (as `gameplay/3A34.h` does) drops
/// the `$v0` definition from the call's RTL and resequences the epilogue.

extern u8  D_801153F4;
extern s32 Gp_LcgState;

void Actor01600_Fn03A60(Actor01600* arg0)
{
    Actor01600PlayerSlot** slot;
    Actor01600PlayerSlot** slots;
    s16                    mode;
    s16                    diff;
    s32                    current;
    s16                    angle;
    s32                    absoluteDiff;
    s32                    randomTurn;
    s16                    turn;
    s32                    randomStep;
    s16                    wrap;
    s32                    amount;
    s32                    remaining;
    s32                    remaining2;
    u16                    wanted;
    u32                    randomState;
    u32                    randomState2;
    u32                    random;
    GsCOORDINATE2*         playerCoord;
    Actor01600Work*        work;
    GsCOORDINATE2*         coord;
    Actor01600RotScratch*  allocated;
    Actor01600RotScratch*  scratch;

    work                                = arg0->field_1C;
    coord                               = arg0->field_2C->field_8;
    slots                               = Gp_ActorSlots;
    slot                                = &slots[Actor01600_Fn052C4(arg0) & 0xFF];
    allocated                           = *(Actor01600RotScratch**)0x1F8003FC;
    allocated                          -= 1;
    *(Actor01600RotScratch**)0x1F8003FC = allocated;
    mode                                = work->field_510;
    playerCoord                         = (*slot)->field_2C->field_8;
    scratch                             = allocated;
    switch (mode) {
        case 0:
            scratch->position.vx = (s32)(playerCoord->coord.t[0] - coord->coord.t[0]);
            scratch->position.vy = 0;
            scratch->position.vz = (s32)(playerCoord->coord.t[2] - coord->coord.t[2]);
            wanted               = ratan2((s32)(s16)scratch->position.vx, (s32)(s16)scratch->position.vz) & 0xFFF;
            diff                 = wanted - (work->field_4FC & 0xFFF);
            absoluteDiff         = diff >= 0 ? diff : -diff;
            turn                 = diff;
            if (absoluteDiff < 0x21) {
                work->field_4FC = wanted;
            } else {
                if (absoluteDiff >= 0x801) {
                    wrap = diff - 0x1000;
                    if (diff <= 0) {
                        wrap = 0x1000 - diff;
                    }
                    turn = wrap;
                }
                current = (s16)work->field_4FC;
                if (turn > 0) {
                    work->field_4FC = (u16)(current + 0x20);
                } else {
                    work->field_4FC = (u16)(current - 0x20);
                }
            }
            break;
        case 1:
            randomState = (Gp_LcgState * 5) + 0x71357911;
            random      = randomState >> 0x10;
            randomTurn  = (random & 0x3FF) + 0x400;
            Gp_LcgState = (s32)randomState;
            if (random & 0x400) {
                randomTurn = -randomTurn;
            }
            work->field_512 = randomTurn;
            work->field_510 = 2;
            break;
        case 2:
            randomState2 = (Gp_LcgState * 5) + 0x71357911;
            randomStep   = (randomState2 >> 0x10) & 0x17;
            Gp_LcgState  = (s32)randomState2;
            if (work->field_512 <= 0) {
                randomStep = -randomStep;
            }
            turn            = randomStep;
            work->field_4FC = (u16)(work->field_4FC + turn);
            remaining       = work->field_512 - turn;
            if ((remaining >= 0 ? remaining : -remaining) < 0x20) {
                work->field_510 = 0;
                work->field_512 = 0;
            } else {
                work->field_512 -= turn;
            }
            break;
        case 7:
            break;
        case 5:
            work->field_510 = 6;
            work->field_512 = (s16)work->field_4E0;
            break;
        case 6:
            amount = work->field_4F0;
            if (work->field_512 <= 0) {
                amount = -amount;
            }
            turn            = amount;
            angle           = work->field_4FC + amount;
            work->field_4FC = (u16)angle;
            if (angle >= 0x801) {
                work->field_4FC = angle - 0x1000;
            } else if (angle < -0x800) {
                work->field_4FC = angle + 0x1000;
            }
            remaining2 = work->field_512 - turn;
            if ((remaining2 >= 0 ? remaining2 : -remaining2) < work->field_4F0) {
                work->field_510 = 7;
                work->field_512 = 0;
            } else {
                work->field_512 -= turn;
            }
            break;
    }
    scratch->rotation.vx = 0;
    scratch->rotation.vy = (u16)work->field_4FC;
    scratch->rotation.vz = 0;
    RotMatrix(&scratch->rotation, &coord->coord);
    *(Actor01600RotScratch**)0x1F8003FC += 1;
}

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
            Gp_UnlinkObj(work->collision.field_2EC);
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
    if (count < 5 && gGameSession->field_52 != 1) {
        return;
    }
    work->field_550 = 0;

    attach = &arg1->field_2C->field_8[1];
    update_actor_color(arg0, attach);
}

/// Measures the player against the actor: returns the yaw the actor would have
/// to turn through to face the player, wrapped into -0x800..0x800, and writes
/// the horizontal distance to `distance`.
///
/// The scratch block is addressed two ways on purpose. `head` is the scratchpad
/// top the allocation moved down from, and the direction vector and the
/// transposed rotation are written at negative offsets from it, which is what
/// keeps the head in a register of its own; only `delta`, at the foot of the
/// block, goes through the allocated pointer. The first difference is computed
/// into `dx` so that the scratchpad pointer is stored between it and its own
/// store, as the two stores are emitted in that order.
///
/// The `sw` is written out because the second `scratch` operand is what gives
/// the pointer the reference count that ranks it above `other` in the register
/// allocator; the instruction it emits is the store the C expression would have
/// emitted anyway.
s32 Actor01600_Fn045A8(Actor01600* arg0, s32* distance)
{
    SVECTOR                local;
    Actor01600PlayerSlot** slot;
    Actor01600PlayerSlot** slots;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         other;
    s32                    angle;
    s32                    dx;
    s32                    x;
    s32                    z;
    void*                  head;
    void*                  allocated;
    void*                  vec;
    void*                  matrix;
    Actor01600AimScratch*  scratch;

    slots                     = Gp_ActorSlots;
    slot                      = &slots[Actor01600_Fn052C4(arg0) & 0xFF];
    head                      = *(void**)0x1F8003FC;
    coord                     = arg0->field_2C->field_8;
    other                     = (*slot)->field_2C->field_8;
    dx                        = *(u16*)&other->workm.t[0] - *(u16*)&coord->workm.t[0];
    allocated                 = (*(void**)0x1F8003FC = head - 0x7C);
    *(s16*)((s8*)head - 0x40) = (s16)dx;
    vec                       = head - 0x40;
    *(s16*)((s8*)vec + 2)     = (s16)(*(u16*)&other->workm.t[1] - *(u16*)&coord->workm.t[1]);
    scratch                   = allocated;
    *(s16*)((s8*)vec + 4)     = (s16)(*(u16*)&other->workm.t[2] - *(u16*)&coord->workm.t[2]);
    matrix                    = head - 0x20;
    TransposeMatrix(&coord->workm, matrix);
    local = *(SVECTOR*)vec;
    gte_SetRotMatrix(matrix);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stsv(vec);
    angle = ratan2(*(s16*)((s8*)head - 0x40), *(s16*)((s8*)vec + 4));
    if (angle >= 0x801) {
        angle -= 0x1000;
    } else if (angle < -0x800) {
        angle += 0x1000;
    }
    x = other->coord.t[0] - coord->coord.t[0];
    __asm__("sw\t%1, %0" : "=m"(scratch->delta.vx) : "r"(x), "r"(scratch));
    scratch->delta.vy   = other->coord.t[1] - coord->coord.t[1];
    z                   = other->coord.t[2] - coord->coord.t[2];
    scratch->delta.vz   = z;
    *distance           = SquareRoot0((x * x) + (z * z));
    *(void**)0x1F8003FC = *(void**)0x1F8003FC + 0x7C;
    return angle;
}

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

s32 Actor01600_Fn04974(Actor01600* actor, s32 angle, s32 distance, s32 flags)
{
    Actor01600Work* work;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  other;
    s32             difference;
    s32             angleAbs;
    s32             done;
    s32             otherY;
    s32             tmp;

    work  = actor->field_1C;
    coord = actor->field_2C->field_8;
    other = (*Gp_ActorSlots)->field_2C->field_8;
    if (Gp_ActorSlots[flags]->actor->field_954 != 2) {
        if (Actor01600_D12870 == 0) {
            otherY     = other->coord.t[1];
            tmp        = coord->coord.t[1];
            difference = otherY - tmp;
            if (difference < 0) {
                difference = -difference;
            }
            if (difference < 0x191) {
                if (distance < 0x3E9) {
                    tmp      = angle >= 0;
                    angleAbs = tmp ? angle : -angle;
                    if (angleAbs < 0x101) {
                        work->field_42A |= 0xC000;
                        Actor01600_Fn04EB0(actor);
                        if (work->field_4EA >= 2) {
                            work->field_506  = 0x19;
                            work->field_51A  = 0;
                            work->field_4FE  = 0;
                            work->field_4EA  = 0;
                            work->field_514  = 1;
                            work->field_2BA &= 0x3FFF;
                            done             = 1;
                        } else {
                            done = 0;
                        }
                        if ((u8)done) {
                            return 1;
                        }
                        work->field_516 = 7;
                        work->field_506 = 0x1C;
                        work->field_52C = 0;
                        work->field_53E = flags;
                        work->field_4D4 = (Task*)Gp_ActorSlots[flags];
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

void Actor01600_Fn04AD8(Actor01600* arg0)
{
    Actor01600Ctx*  ctx;
    Actor01600Work* work;
    GsCOORDINATE2*  body;
    s16             state;
    u16             count;
    u16             count2;

    work  = arg0->field_1C;
    ctx   = arg0->field_20;
    state = work->field_4F2;
    body  = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            work->field_24C.coord.t[0] = body->coord.t[0];
            work->field_24C.coord.t[2] = body->coord.t[2];
            ctx->field_18              = body;
            break;
        case 1:
            work->field_24C.coord.t[0] = body->coord.t[0];
            work->field_24C.coord.t[2] = body->coord.t[2];
            ctx->field_18              = &work->field_24C;
            goto advance;
        case 2:
            count           = work->field_4F4 + 1;
            work->field_4F4 = count;
            if ((s16)count < 8) {
                break;
            }
        advance:
            work->field_4F4 = 0U;
            work->field_4F2 = (u16)work->field_4F2 + 1;
            break;
        case 3:
            work->field_4F6 = (s16)((body->coord.t[0] - work->field_24C.coord.t[0]) / 5);
            work->field_4F8 = (s16)((body->coord.t[2] - work->field_24C.coord.t[2]) / 5);
            work->field_4F2 = (u16)work->field_4F2 + 1;
            break;
        case 4:
            work->field_24C.coord.t[0] += work->field_4F6;
            work->field_24C.coord.t[2] += work->field_4F8;
            count2                      = work->field_4F4 + 1;
            work->field_4F4             = count2;
            if ((s16)count2 >= 5) {
                work->field_4F4 = 0U;
                work->field_4F2 = 0;
            }
            break;
    }
    work->field_24C.coord.t[1] = body->coord.t[1];
    work->field_24C.flg        = 0;
    Gp_UpdateCoord(&work->field_24C);
}

/// Allocates the yaw scratch, updates `field_4EC` either by `angle` (clamped
/// into (-0x800, 0x801]) or, when `angle` is 0, by its own 0x71 / 0xA step -
/// returning 1 once the degree counter `field_4EE` passes a full turn - then
/// turns (0, 0, `distance`) by the resulting yaw into `field_42C` / `field_430`
/// and advances the swept-angle range `ranges[field_4E8]`.
s32 Actor01600_Fn04C64(Actor01600* arg0, s32 distance, s32 angle)
{
    Actor01600YawScratch* allocated;
    Actor01600YawScratch* scratch;
    Actor01600Work*       work;
    MATRIX*               m;
    SVECTOR*              out;
    s16                   temp_v0_3;
    s16                   temp_v0_4;
    s16                   temp_v0_5;
    s16                   temp_v1_2;
    s16                   var_v0;
    s16                   var_v0_2;
    s32                   scaled;
    s32                   temp_a0;
    s32                   var_s4;

    var_s4                              = 0;
    allocated                           = *(Actor01600YawScratch**)0x1F8003FC - 1;
    work                                = arg0->field_1C;
    *(Actor01600YawScratch**)0x1F8003FC = allocated;
    scratch                             = allocated;
    scratch->vec.vx                     = 0;
    scratch->vec.vy                     = 0;
    scratch->vec.vz                     = (s16)distance;
    if (angle == 0) {
        temp_v1_2       = (u16)work->field_4EE + 0xA;
        scaled          = temp_v1_2 << 0x10;
        var_s4          = scaled > 0x01670000;
        work->field_4EC = (u16)work->field_4EC + 0x71;
        work->field_4EE = temp_v1_2;
    } else {
        work->field_4EC = (s16)angle;
        if ((s16)angle >= 0x801) {
            var_v0 = angle - 0x1000;
            goto block_5;
        }
        if ((s16)angle >= -0x800) {
            goto block_6;
        }
        var_v0 = angle + 0x1000;
    block_5:
        work->field_4EC = var_v0;
    block_6:;
    }
    m                  = &scratch->mat;
    *(s32*)&m->m[0][0] = 0x1000;
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = 0x1000;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = 0x1000;
    func_8004BFF8(work->field_4EC, m);
    out = &scratch->out;
    gte_SetRotMatrix(m);
    gte_ldv0(&scratch->vec);
    gte_rtv0_real();
    gte_stsv(out);
    work->field_42C = (s16)scratch->out.vx;
    work->field_430 = (s16)scratch->out.vz;
    if ((u16)(work->field_444.field_4 >> 16) != 0x10) {
        if (angle == 0) {
            temp_v0_3 = work->field_4E8;
            if (work->ranges[temp_v0_3].low == 0xFFFF) {
                work->ranges[temp_v0_3].low = (s32)work->field_4EC;
            } else {
                work->ranges[temp_v0_3].high = (s32)work->field_4EC;
                if (var_s4 == 1) {
                    var_v0_2 = (u16)work->field_4E8 + 1;
                    goto block_18;
                }
            }
        } else {
            var_s4 = 1;
        }
    } else if (angle == 0) {
        temp_v0_4 = work->field_4E8;
        temp_a0   = work->ranges[temp_v0_4].low;
        if (temp_a0 != 0xFFFF) {
            if (work->ranges[temp_v0_4].high == 0xFFFF) {
                work->ranges[temp_v0_4].high = temp_a0;
            }
            temp_v0_5       = (u16)work->field_4E8 + 1;
            work->field_4E8 = temp_v0_5;
            if (temp_v0_5 >= 7) {
                var_v0_2 = 7;
            block_18:
                work->field_4E8 = var_v0_2;
            }
        }
    }
    Gp_ClearRec18Occupied(&work->field_444);
    *(Actor01600YawScratch**)0x1F8003FC += 1;
    return var_s4;
}
