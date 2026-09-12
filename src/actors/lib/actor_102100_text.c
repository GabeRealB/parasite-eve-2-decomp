#include "common.h"

#include "actors/actor_102100.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include <psyq/inline_c.h>

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn00048);

GpEffWork* Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, SVECTOR* arg3);

void Actor02100_Fn004C4(Actor02100* arg0)
{
    Actor02100Fn014E4Scratch* scratch;
    Actor02100Work*           work;
    Actor02100Spawn*          enemy;
    GsCOORDINATE2*            coord;
    GsCOORDINATE2*            src;
    GpRoomParamRec*           param;
    u8*                       head;
    s32                       damage;
    s32                       stun;
    s32                       sound;
    s32                       pan;
    s32                       depth;
    s32                       index;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x18;
    scratch               = (Actor02100Fn014E4Scratch*)*(u8**)G_SCRATCH_HEAD;
    coord                 = arg0->field_2C->field_8;
    work                  = arg0->field_1C;
    enemy                 = arg0->field_20;

    if (work->field_170 != 0) {
        work->field_170--;
        if (work->field_170 <= 0) {
            work->field_170 = 0;
        }
    }
    work->field_17E = 0;
    if (work->field_18A != 0) {
        work->field_18A--;
    }

    if (work->field_170 == 0) {
        if ((work->field_60.field_4 & 0xFFFF0000) == 0x20000) {
            if (work->field_60.field_4 & 0x8000) {
                func_800DA6E8(&enemy->field_10, 0, 0);
            } else if ((((u32)work->field_60.field_4 >> 8) & 0x3F) < 0x21U) {
                src             = Gp_ActorSlots[((u32)work->field_60.field_4 >> 7) & 1]->extra->field_8;
                scratch->vec.vx = src->coord.t[0] - coord->coord.t[0];
                scratch->vec.vy = src->coord.t[1] - coord->coord.t[1];
                scratch->vec.vz = src->coord.t[2] - coord->coord.t[2];
                damage          = Gp_ComputeDamage(work->field_60.field_4,
                                                   SquareRoot0(scratch->vec.vx * scratch->vec.vx +
                                                               scratch->vec.vy * scratch->vec.vy +
                                                               scratch->vec.vz * scratch->vec.vz),
                                                   0, 0);
                if (Gp_RollEnemyChance((struct _GpEnemy*)arg0->field_20,
                                       work->field_60.field_4, 0) != 0) {
                    damage *= 4;
                    Gp_SpawnEff(0x6009C, coord, 0, 0);
                }
                enemy->field_40 -= damage;
                func_800DA6E8(&enemy->field_10, damage, 0);
                work->field_17E = 1;
                if (enemy->field_40 <= 0) {
                    Gp_SpawnEff(0x6005C, coord, 0x10002400, 0);
                    Gp_SpawnEff(0x60070, coord, 0x32FF1400, 0);
                    work->field_172 = 4;
                    work->field_174 = 0;
                    work->field_96 &= 0x7FFF;
                    work->field_E6 &= 0x7FFF;
                    work->field_96 &= 0xBFFF;
                    work->field_E6 &= 0xBFFF;
                    arg0->field_30  = 2;
                    sound           = ((arg0->field_20->field_8 >> 12) << 8) | 0x4015000A;
                    SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan((GpObj38*)coord),
                                        (s8)Gp_GetObjDepth((GpObj38*)coord));
                } else if (damage > 0) {
                    if (work->field_18A == 0) {
                        scratch->shortVec.vx = 0;
                        scratch->shortVec.vy = 0;
                        scratch->shortVec.vz = 0xC8;
                        if ((Gp_GetIdParam0(work->field_60.field_4) & 0xFFFF) == 7) {
                            Gp_SpawnEff(0x6007F, coord,
                                        work->field_100.field_4 | (work->field_100.field_6 << 16),
                                        &scratch->shortVec);
                        }
                        func_800FDB18(7, coord, &scratch->shortVec, &work->field_100);
                        work->field_18A = 10;
                    }
                    sound = ((arg0->field_20->field_8 >> 12) << 8) | 0x40150009;
                    pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
                    depth = (s8)Gp_GetObjDepth((GpObj38*)coord);
                    SndEvt_EnqueueType6(sound, pan, depth);
                    stun = Gp_GetIdParam2(work->field_60.field_4);
                    if (stun > 0) {
                        work->field_170 = stun;
                    }
                }
            }
        }
    }

    Gp_ClearRec18Occupied(&work->field_60);
    work->field_184 = 0;
    if (Gp_CountRec18Hi(&work->field_98, 0x100000) != 0) {
        index = func_800E1B24(work->field_98.field_4);
        param = Gp_RoomParamTables[Game_Session->field_7 - 1]
                                  [Game_Session->field_6 - 1][index];
        if (param->field_1 == 0) {
            work->field_184 = 1;
        }
    }

    if ((work->field_98.field_4 & 0xFFFF0000) == 0x10000 ||
        (work->field_98.field_4 & 0xFFFF0000) == 0x30000 || work->field_184 == 1) {
        scratch->shortVec.vx = 0;
        scratch->shortVec.vy = 0;
        scratch->shortVec.vz = 0x12C;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->shortVec);
        __asm__ volatile("nop; nop; .word 0x4A486012");
        gte_stlvnl(&scratch->vec);
        scratch->vec.vx += coord->workm.t[0];
        scratch->vec.vy += coord->workm.t[1];
        scratch->vec.vz += coord->workm.t[2];
        scratch->vec.vx  = work->field_98.field_8 - scratch->vec.vx;
        scratch->vec.vy  = work->field_98.field_A - scratch->vec.vy;
        scratch->vec.vz  = work->field_98.field_C - scratch->vec.vz;
        work->field_182  = SquareRoot0(scratch->vec.vx * scratch->vec.vx +
                                       scratch->vec.vy * scratch->vec.vy +
                                       scratch->vec.vz * scratch->vec.vz);
        if (work->field_174 >= 2) {
            scratch->shortVec.vx = 0;
            scratch->shortVec.vy = 0;
            scratch->shortVec.vz = work->field_182;
            gte_SetRotMatrix(&work->field_144);
            gte_ldv0(&scratch->shortVec);
            __asm__ volatile("nop; nop; .word 0x4A486012");
            gte_stsv(&scratch->shortVec);
            scratch->shortVec.vz += 0x12C;
            Gp_SpawnEff(0x6003B, coord, 0, &scratch->shortVec);
        }
    }

    Gp_ClearRec18Occupied(&work->field_98);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn00ADC);

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Gp_ArmStateF0(s32 arg0);

s32  Actor02100_Fn0337C(SVECTOR* arg0, SVECTOR* arg1);
void Actor02100_Fn011C4(Actor02100* arg0);

/// Line-of-sight scan. Takes a 0x20-byte block from `G_SCRATCH_HEAD`, builds
/// the world-space delta from this actor's coordinate to the player's (entry 0
/// of the player's coordinate array in mode 4, entry 3 otherwise) and, when the
/// player is in front of the actor, checks the distance against the sight range
/// in `Actor02100_D03E00` and asks `Actor02100_Fn0337C` whether the segment is
/// clear. A hit latches the player onto `field_140` and switches the state
/// machine to 2 (or 3 in mode 4). `field_186` throttles the scan to one run
/// every 5 frames while the session is in state 1.
void Actor02100_Fn00DCC(Actor02100* arg0)
{
    Actor02100Work*  work;
    Actor02100Sight* blk;
    GsCOORDINATE2*   self;
    GsCOORDINATE2*   target;
    u8*              head;
    u32              dist;
    s32              mode;

    self = arg0->field_2C->field_8;
    work = arg0->field_1C;

    if (Game_Session->field_4D == 1) {
        work->field_186 = 5;
    }
    if (work->field_186 != 0) {
        work->field_186--;
        return;
    }

    work->field_140 = NULL;
    work->field_164 = 0;
    work->field_180 = 0;
    self->flg       = 0;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x20;
    blk                   = (Actor02100Sight*)(head - 0x20);

    if (work->field_178 == 4) {
        target = &((Actor02100*)Game_GetPtrSlot(3))->field_2C->field_8[0];
    } else {
        target = &((Actor02100*)Game_GetPtrSlot(3))->field_2C->field_8[3];
    }
    target->flg = 0;
    Gp_UpdateCoord(target);

    if (work->field_17E == 0) {
        blk->delta.vx = target->workm.t[0] - self->workm.t[0];
        blk->delta.vy = target->workm.t[1] - self->workm.t[1];
        blk->delta.vz = target->workm.t[2] - self->workm.t[2];

        if (((blk->delta.vx * self->workm.m[0][2]) + (blk->delta.vy * self->workm.m[1][2]) +
             (blk->delta.vz * self->workm.m[2][2])) > 0) {
            dist = SquareRoot0((blk->delta.vx * blk->delta.vx) + (blk->delta.vy * blk->delta.vy) +
                               (blk->delta.vz * blk->delta.vz));
            if (dist < Actor02100_D03E00[arg0->field_20->field_3C->field_F & 7]) {
                blk->from.vx = target->workm.t[0];
                blk->from.vy = target->workm.t[1];
                blk->from.vz = target->workm.t[2];
                blk->to.vx   = self->workm.t[0];
                blk->to.vy   = self->workm.t[1];
                blk->to.vz   = self->workm.t[2];
                if (Actor02100_Fn0337C(&blk->from, &blk->to) == 0) {
                    work->field_140 = Game_GetPtrSlot(3);
                    work->field_164 = dist;
                    work->field_180 = 1;
                }
            }
        }
        Actor02100_Fn011C4(arg0);
    } else {
        blk->delta.vx = target->workm.t[0] - self->workm.t[0];
        blk->delta.vy = target->workm.t[1] - self->workm.t[1];
        blk->delta.vz = target->workm.t[2] - self->workm.t[2];

        if (((blk->delta.vx * self->workm.m[0][2]) + (blk->delta.vy * self->workm.m[1][2]) +
             (blk->delta.vz * self->workm.m[2][2])) > 0) {
            blk->from.vx = target->workm.t[0];
            blk->from.vy = target->workm.t[1];
            blk->from.vz = target->workm.t[2];
            blk->to.vx   = self->workm.t[0];
            blk->to.vy   = self->workm.t[1];
            blk->to.vz   = self->workm.t[2];
            if (Actor02100_Fn0337C(&blk->from, &blk->to) == 0) {
                work->field_140 = Game_GetPtrSlot(3);
                work->field_164 = 1;
                work->field_180 = 1;
            }
        }
    }

    if (work->field_180 != 0) {
        Gp_ArmStateF0(1);
        if (work->field_178 == 4) {
            mode = 3;
        } else {
            mode = 2;
        }
        work->field_120 = work->field_118;
        work->field_122 = work->field_11A;
        work->field_124 = work->field_11C;
        work->field_172 = mode;
        work->field_174 = 0;
        work->field_17A = 0;
        work->field_17C = 0;
        work->field_118 = 0;
        work->field_11A = 0;
        work->field_11C = 0;
        if (work->field_188 == 1) {
            SndEvt_EnqueueType7(work->field_168, 1);
            work->field_188 = 0;
        }
    }

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + 0x20;
}

void Actor02100_Fn011C4(Actor02100* arg0)
{
    Actor02100Fn011C4Scratch* scratch;
    Task*                     list;
    Actor02100*               head;
    Actor02100*               current;
    Actor02100Spawn*          enemy;
    Actor02100Work*           work;
    GsCOORDINATE2*            coord;
    u8**                      scratchSlot;
    u8*                       scratchHead;
    s32                       index;
    s32                       dist;

    list  = (Task*)Game_GetPtrSlot(4);
    coord = arg0->field_2C->field_8;
    head  = (Actor02100*)list->firstChild;
    work  = arg0->field_1C;
    if (head != NULL) {
        scratchSlot  = (u8**)G_SCRATCH_HEAD;
        current      = head;
        scratchHead  = *(u8**)G_SCRATCH_HEAD;
        *scratchSlot = scratchHead - 0x40;
        scratch      = (Actor02100Fn011C4Scratch*)*scratchSlot;
        SOFT_TOUCH_REG(head);
        do {
            enemy = current->field_20;
            index = *(u8*)enemy->field_3C;
            if (index >= 0x50U) {
                index = 0;
            }
            if (Actor02100_D03E2C[index] == 0 && enemy->field_40 > 0) {
                Gp_GetLockPos((GpLockPos*)&enemy->field_10, (VECTOR3*)&scratch->lock);
                scratch->delta.vx = scratch->lock.vx - coord->coord.t[0];
                scratch->delta.vy = scratch->lock.vy - coord->coord.t[1];
                scratch->delta.vz = scratch->lock.vz - coord->coord.t[2];
                if ((scratch->delta.vx * coord->coord.m[0][2]) +
                        (scratch->delta.vy * coord->coord.m[1][2]) +
                        (scratch->delta.vz * coord->coord.m[2][2]) >
                    0) {
                    dist = SquareRoot0((scratch->delta.vx * scratch->delta.vx) +
                                       (scratch->delta.vy * scratch->delta.vy) +
                                       (scratch->delta.vz * scratch->delta.vz));
                    if ((work->field_164 == 0 || (u32)dist < (u32)work->field_164) &&
                        (u32)dist <
                            (u32)Actor02100_D03E00[arg0->field_20->field_3C->field_F & 7]) {
                        scratch->from.vx = (u16)scratch->lock.vx;
                        scratch->from.vy = (u16)scratch->lock.vy;
                        scratch->from.vz = (u16)scratch->lock.vz;
                        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
                        gte_ldv0(&scratch->from);
                        __asm__ volatile("nop; nop; .word 0x4A486012");
                        gte_stlvnl(&scratch->transformed);
                        scratch->transformed.vx += Gfx_ViewCoord.workm.t[0];
                        scratch->transformed.vy += Gfx_ViewCoord.workm.t[1];
                        scratch->transformed.vz += Gfx_ViewCoord.workm.t[2];
                        scratch->from.vx         = (u16)scratch->transformed.vx;
                        scratch->from.vy         = (u16)scratch->transformed.vy;
                        scratch->from.vz         = (u16)scratch->transformed.vz;
                        scratch->to.vx           = (u16)coord->workm.t[0];
                        scratch->to.vy           = (u16)coord->workm.t[1];
                        scratch->to.vz           = (u16)coord->workm.t[2];
                        if (Actor02100_Fn0337C(&scratch->from, &scratch->to) == 0) {
                            work->field_140 = current;
                            work->field_164 = dist;
                            work->field_180 = 2;
                        }
                    }
                }
            }
            current = (Actor02100*)((Task*)current)->nextSibling;
        } while (current != head);
        *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + 0x40;
    }
}

s32 Actor02100_Fn014E4(Actor02100* arg0)
{
    Actor02100Fn014E4Scratch* scratch;
    Actor02100Work*           work;
    GsCOORDINATE2*            coord;
    GsCOORDINATE2*            targetCoord;
    VECTOR*                   vec;
    GpLockPos*                lock;
    u8*                       head;
    s32                       result;
    s32                       state;

    work   = arg0->field_1C;
    coord  = arg0->field_2C->field_8;
    result = 0;
    if (work->field_140 == NULL) {
        return result;
    }

    head                  = *(u8**)G_SCRATCH_HEAD;
    scratch               = (Actor02100Fn014E4Scratch*)(head - 0x18);
    *(u8**)G_SCRATCH_HEAD = (u8*)scratch;
    vec                   = &scratch->vec;
    state                 = work->field_180;
    if (state == 1) {
        goto case1;
    }
    if (state < 2) {
        goto cleanup;
    }
    if (state == 2) {
        goto case2;
    }
    goto cleanup;

case1:
    if (work->field_178 == 4) {
        targetCoord = work->field_140->field_2C->field_8;
    } else {
        targetCoord = &work->field_140->field_2C->field_8[3];
    }
    vec->vx = targetCoord->workm.t[0];
    vec->vy = targetCoord->workm.t[1];
    vec->vz = targetCoord->workm.t[2];
    ApplyTransposeMatrixLV(&coord->workm, vec, &work->field_108);
    result = 1;
    goto cleanup;

case2:
    if (work->field_140->field_20->field_40 <= 0) {
        goto cleanup;
    }
    lock = (GpLockPos*)&work->field_140->field_20->field_10;
    Gp_GetLockPos(lock, (VECTOR3*)&scratch->vec);
    scratch->shortVec.vx = *(u16*)&scratch->vec.vx;
    scratch->shortVec.vy = *(u16*)&scratch->vec.vy;
    scratch->shortVec.vz = *(u16*)&scratch->vec.vz;
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(&scratch->shortVec);
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stlvnl(vec);
    scratch->vec.vx += Gfx_ViewCoord.workm.t[0];
    scratch->vec.vy += Gfx_ViewCoord.workm.t[1];
    scratch->vec.vz += Gfx_ViewCoord.workm.t[2];
    ApplyTransposeMatrixLV(&coord->workm, &scratch->vec, &work->field_108);
    result = 1;

cleanup:
    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + 0x18;
    return result;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn016EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn01FF0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn02924);

void Actor02100_Fn03168(Actor02100* arg0)
{
    Actor02100StateFuncTable3 sp;

    sp = Actor02100_D00004;
    sp.funcs[arg0->field_30]((Actor02100Ctx*)arg0->field_20, arg0);
}

void Actor02100_Fn004C4(Actor02100* arg0);
void Actor02100_Fn03488(Actor02100* arg0);

extern u8 D_801153F4;
extern s8 D_80115416;

/// Per-frame tick, entry 1 of `Actor02100_D00004`. `D_801153F4` is the global
/// gameplay mode: mode 1 only refreshes the actor colour, mode 2 parks the
/// actor (`field_C` 0x80, node flag 1) and returns, and mode 0 re-shows it
/// (`field_C` 0, node flag 8) before falling into the normal body. The body
/// drains the pending translation delta at `field_118` into the actor's
/// coordinate, runs the state machine, and switches to state 4 - handing the
/// task over to `Actor02100_Fn035D4` - once `D_80115416` reports the kill.
void Actor02100_Fn031C4(Actor02100Ctx* arg0, Actor02100* arg1)
{
    Actor02100Obj2C* obj;
    Actor02100Work*  work;
    GsCOORDINATE2*   coord;
    s32              mode;
    s32              one;

    obj   = arg1->field_2C;
    mode  = D_801153F4;
    work  = arg1->field_1C;
    coord = obj->field_8;
    one   = 1;
    if (mode == one) {
        goto case1;
    }
    if (mode >= 2) {
        goto ge2;
    }
    if (mode == 0) {
        goto case0;
    }
    goto body;
ge2:
    if (mode == 2) {
        goto case2;
    }
    goto body;
case0:
    obj->field_C       = 0;
    arg0->node.field_4 = 8;
    goto body;
case1:
    Actor02100_Fn03488(arg1);
    return;
case2:
    obj->field_C       = 0x80;
    arg0->node.field_4 = one;
    return;
body:
    Actor02100_Fn004C4(arg1);
    coord->coord.t[0] += work->field_118;
    coord->coord.t[1] += work->field_11A;
    coord->coord.t[2] += work->field_11C;
    coord->flg         = 0;
    Gp_UpdateCoord(coord);
    Actor02100_Fn032E4(arg1);
    Actor02100_Fn03488(arg1);
    if (D_80115416 == 1) {
        work->field_172 = 4;
        work->field_174 = 0;
        arg1->field_30  = 2;
    }
}

void Actor02100_Fn00ADC(Actor02100* arg0);
void Actor02100_Fn016EC(Actor02100* arg0);
void Actor02100_Fn01FF0(Actor02100* arg0);

void Actor02100_Fn032E4(Actor02100* arg0)
{
    s16 state;

    state = arg0->field_1C->field_172;
    switch (state) {
        case 1:
            Actor02100_Fn00ADC(arg0);
        case 0:
            if (GameFlag_GetNibble(0xD2) == 0) {
                Actor02100_Fn00DCC(arg0);
            }
            break;
        case 2:
            Actor02100_Fn016EC(arg0);
            break;
        case 3:
            Actor02100_Fn01FF0(arg0);
            break;
        case 4:
            break;
    }
}

s32 Actor02100_Fn0337C(SVECTOR* arg0, SVECTOR* arg1)
{
    void**           scratch;
    u8*              head;
    register VECTOR* vec asm("s1");
    GpObj3A*         node;
    s32              ret;

    ret                          = 0;
    scratch                      = (void**)G_SCRATCH_HEAD;
    node                         = D_80115550;
    head                         = *scratch;
    ((VECTOR*)(head - 0x10))->vx = arg1->vx - arg0->vx;
    head                         = head - 0x10;
    vec                          = (VECTOR*)head;
    TOUCH_REG_USE(vec, head);
    vec->vy  = arg1->vy - arg0->vy;
    *scratch = vec;
    vec->vz  = arg1->vz - arg0->vz;
    VectorNormal(vec, vec);
    for (; node != NULL; node = node->next) {
        if (node->field_3A & 0x40) {
            ret = func_800DFCCC(node, arg0, arg1, vec);
            if (ret == 1) {
                break;
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
    return ret;
}

void Actor02100_Fn03488(Actor02100* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn034E0);

void Actor02100_Fn035D4(Actor02100Ctx* arg0, Actor02100* arg1)
{
    Actor02100Work* work;
    s16             state;
    u16             timer;

    work  = arg1->field_1C;
    state = work->field_174;
    if (state == 0) {
        goto case0;
    }
    if (state == 1) {
        goto case1;
    }
    goto epilogue;
case0:
    arg1->field_2C->field_C = 0x80;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(work->field_40);
    Gp_UnlinkObj(work->field_78);
    Gp_UnlinkObj(work->field_C8);
    arg0->field_54 = 0;
    Gp_ReleaseStateF0Add(arg1, 0x15);
    work->field_174 = 1;
    work->field_17A = 0x3C;
    if (work->field_188 != 0) {
        SndEvt_EnqueueType7(work->field_168, 1);
    }
    goto epilogue;
case1:
    timer = work->field_17A;
    timer--;
    work->field_17A = timer;
    if ((s16)timer > 0) {
        goto epilogue;
    }
    Gp_DestroyEnemy(arg0, arg1);
epilogue:
    return;
}
