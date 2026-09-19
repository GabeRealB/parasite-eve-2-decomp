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

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

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
        if ((work->field_60.key & 0xFFFF0000) == 0x20000) {
            if (work->field_60.key & 0x8000) {
                func_800DA6E8(&enemy->field_10, 0, 0);
            } else if ((((u32)work->field_60.key >> 8) & 0x3F) < 0x21U) {
                src             = Gp_ActorSlots[((u32)work->field_60.key >> 7) & 1]->extra->coords;
                scratch->vec.vx = src->coord.t[0] - coord->coord.t[0];
                scratch->vec.vy = src->coord.t[1] - coord->coord.t[1];
                scratch->vec.vz = src->coord.t[2] - coord->coord.t[2];
                damage          = Gp_ComputeDamage(work->field_60.key,
                                                   SquareRoot0(scratch->vec.vx * scratch->vec.vx +
                                                               scratch->vec.vy * scratch->vec.vy +
                                                               scratch->vec.vz * scratch->vec.vz),
                                                   0, 0);
                if (Gp_RollEnemyChance((struct _GpEnemy*)arg0->field_20,
                                       work->field_60.key, 0) != 0) {
                    damage *= 4;
                    Gp_SpawnEff(0x6009C, coord, 0, 0);
                }
                enemy->field_40 -= damage;
                func_800DA6E8(&enemy->field_10, damage, 0);
                work->field_17E = 1;
                if (enemy->field_40 <= 0) {
                    Gp_SpawnEff(0x6005C, coord, 0x10002400, 0);
                    Gp_SpawnEff(0x60070, coord, 0x32FF1400, 0);
                    work->field_172       = 4;
                    work->field_174       = 0;
                    work->field_78.flags &= 0x7FFF;
                    work->field_C8.flags &= 0x7FFF;
                    work->field_78.flags &= 0xBFFF;
                    work->field_C8.flags &= 0xBFFF;
                    arg0->field_30        = 2;
                    sound                 = ((arg0->field_20->field_8 >> 12) << 8) | 0x4015000A;
                    SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan((GpObj38*)coord),
                                        (s8)Gp_GetObjDepth((GpObj38*)coord));
                } else if (damage > 0) {
                    if (work->field_18A == 0) {
                        scratch->shortVec.vx = 0;
                        scratch->shortVec.vy = 0;
                        scratch->shortVec.vz = 0xC8;
                        if ((Gp_GetIdParam0(work->field_60.key) & 0xFFFF) == 7) {
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
                    stun = Gp_GetIdParam2(work->field_60.key);
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
        index = func_800E1B24(work->field_98.key);
        param = Gp_RoomParamTables[gGameSession->at4.loc.stage - 1]
                                  [gGameSession->at4.loc.area - 1][index];
        if (param->field_1 == 0) {
            work->field_184 = 1;
        }
    }

    if ((work->field_98.key & 0xFFFF0000) == 0x10000 ||
        (work->field_98.key & 0xFFFF0000) == 0x30000 || work->field_184 == 1) {
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
        scratch->vec.vx  = work->field_98.point.vx - scratch->vec.vx;
        scratch->vec.vy  = work->field_98.point.vy - scratch->vec.vy;
        scratch->vec.vz  = work->field_98.point.vz - scratch->vec.vz;
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

/// Stops the looping effect, clears the offset vector and starts the tail
/// sound. Expanded at the end of both active steps, which the compiler emits
/// as one shared tail; `sound` is deliberately a caller-scope variable, since
/// both expansions must name the same object.
#define STOP_SOUND                                                           \
    work->field_118 = 0;                                                     \
    work->field_11A = 0;                                                     \
    work->field_11C = 0;                                                     \
    SndEvt_EnqueueType7(work->field_168, 1);                                 \
    work->field_188 = 0;                                                     \
    sound           = (((arg0->field_20->field_8 >> 12) << 8) | 0x40150008); \
    {                                                                        \
        s32 pan;                                                             \
        s32 depth;                                                           \
                                                                             \
        pan   = (s8)Gp_GetObjPan((GpObj38*)coord);                           \
        depth = (s8)Gp_GetObjDepth((GpObj38*)coord);                         \
        SndEvt_EnqueueType6(sound, pan, depth);                              \
    }

/// Per-frame handler for the four-step sound cycle at `field_16C`: two active
/// steps that keep a looping effect playing while the stored offset vector is
/// applied, separated by 60-frame gaps. Steps 0 and 2 start the effect on their
/// first frame, retrigger it each frame, and run for `field_176 * 40` frames;
/// each then stops the effect, clears the offset and starts the tail sound.
/// Step 1 installs the negated offset, step 3 restores it and returns to 0.
void Actor02100_Fn00ADC(Actor02100* arg0)
{
    Actor02100Work* work;
    GsCOORDINATE2*  coord;
    s32             negX;
    s32             negY;
    s32             negZ;
    s16             state;
    s32             one;
    s32             sound;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    one   = 1;
    work->field_16E++;
    state = work->field_16C;

    switch (state) {
        case 0:
            if (work->field_16E == one) {
                work->field_168 = (((arg0->field_20->field_8 >> 12) << 8) | 0x40150007);
                {
                    s32 pan;
                    s32 depth;

                    pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
                    depth = (s8)Gp_GetObjDepth((GpObj38*)coord);
                    SndEvt_EnqueueType6(work->field_168, pan, depth);
                }
                work->field_188 = one;
            }
            {
                s32 pan;
                s32 depth;

                pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
                depth = (s8)Gp_GetObjDepth((GpObj38*)coord);
                SndEvt_EnqueueTypeA(work->field_168, pan, depth);
            }
            if (work->field_16E < (work->field_176 * 40)) {
                break;
            }
            work->field_16E = 0;
            work->field_16C = one;
            STOP_SOUND;
            break;

        case 1:
            if (work->field_16E < 60) {
                break;
            }
            work->field_16C = (state = 2);
            work->field_16E = 0;
            work->field_118 = (negX = -work->field_138);
            work->field_11A = (negY = -work->field_13A);
            work->field_11C = (negZ = -work->field_13C);
            break;

        case 2: {
            s32 timer;

            timer = work->field_16E;
            if (timer == one) {
                work->field_168 = (((arg0->field_20->field_8 >> 12) << 8) | 0x40150007);
                {
                    s32 pan;
                    s32 depth;

                    pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
                    depth = (s8)Gp_GetObjDepth((GpObj38*)coord);
                    SndEvt_EnqueueType6(work->field_168, pan, depth);
                }
                work->field_188 = timer;
            }
        }
            {
                s32 pan;
                s32 depth;

                pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
                depth = (s8)Gp_GetObjDepth((GpObj38*)coord);
                SndEvt_EnqueueTypeA(work->field_168, pan, depth);
            }
            if (work->field_16E < (work->field_176 * 40)) {
                break;
            }
            work->field_16E = 0;
            work->field_16C = 3;
            STOP_SOUND;
            break;

        case 3:
            if (work->field_16E < 60) {
                break;
            }
            work->field_16E = 0;
            work->field_16C = 0;
            work->field_118 = (u16)work->field_138;
            work->field_11A = (u16)work->field_13A;
            work->field_11C = (u16)work->field_13C;
            break;
    }
}

#undef STOP_SOUND

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

    if (gGameSession->viewReady == 1) {
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
                        scratch->transformed.vx += gGfxViewCoord.workm.t[0];
                        scratch->transformed.vy += gGfxViewCoord.workm.t[1];
                        scratch->transformed.vz += gGfxViewCoord.workm.t[2];
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
    scratch->vec.vx += gGfxViewCoord.workm.t[0];
    scratch->vec.vy += gGfxViewCoord.workm.t[1];
    scratch->vec.vz += gGfxViewCoord.workm.t[2];
    ApplyTransposeMatrixLV(&coord->workm, &scratch->vec, &work->field_108);
    result = 1;

cleanup:
    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + 0x18;
    return result;
}

void Actor02100_Fn02924(Actor02100* arg0, s32 arg1);
void Actor02100_Fn034E0(Actor02100* arg0);

/// Aims the actor at its stored target and rebuilds both direction vectors from
/// the new facing. A fixed forward offset is rotated by the coordinate's matrix,
/// translated into the coordinate's own frame and subtracted from the target
/// position; `Gp_OrientAlong` turns the vector that remains into the facing
/// matrix at `field_144`. The near vector at `field_128[1]` and the far vector
/// at `field_B0`, mirrored into `field_E8`, are then rotated through that
/// matrix. Each step borrows scratch from `G_SCRATCH_HEAD` and releases it.
///
/// `Actor02100_OrientScratch`, `Actor02100_UpdateVectors` and
/// `Actor02100_SetVector` do the same three steps for their own callers. This
/// copy is not interchangeable with them: the statement order here is what
/// reproduces this function's schedule and register allocation.
static __inline__ void Actor02100_AimAndBuildVectors(Actor02100* arg0)
{
    Actor02100Fn01FF0Scratch* scratch;
    Actor02100Fn014E4Scratch* shortScratch;
    Actor02100Work*           work;
    Actor02100Work*           nextWork;
    Actor02100Work*           nextWork2;
    GsCOORDINATE2*            coord;
    SVECTOR*                  shortVec;
    u8*                       head0;
    u8*                       head1;
    u8*                       head2;

    coord                 = arg0->field_2C->field_8;
    head0                 = *(u8**)G_SCRATCH_HEAD;
    scratch               = (Actor02100Fn01FF0Scratch*)(head0 - 0x28);
    *(u8**)G_SCRATCH_HEAD = (u8*)scratch;
    work                  = arg0->field_1C;

    scratch->shortVec.vx = 0;
    scratch->shortVec.vy = 0;
    scratch->shortVec.vz = 0x12C;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&scratch->shortVec);
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stlvnl(&scratch->transformed);
    scratch->transformed.vx += coord->workm.t[0];
    scratch->transformed.vy += coord->workm.t[1];
    scratch->transformed.vz += coord->workm.t[2];
    ApplyTransposeMatrixLV(&coord->workm, &scratch->transformed, &scratch->delta);
    scratch->transformed.vx = work->field_108.vx - scratch->delta.vx;
    scratch->transformed.vy = work->field_108.vy - scratch->delta.vy;
    scratch->transformed.vz = work->field_108.vz - scratch->delta.vz;
    Gp_OrientAlong(&scratch->transformed, &work->field_144, 0);

    head1                     = *(u8**)G_SCRATCH_HEAD;
    nextWork                  = arg0->field_1C;
    shortScratch              = (Actor02100Fn014E4Scratch*)(head1 + 0x10);
    nextWork->field_128[0].vx = 0;
    nextWork->field_128[0].vy = 0;
    nextWork->field_128[0].vz = 0x12C;
    shortScratch->shortVec.vx = 0;
    shortScratch->shortVec.vy = 0;
    *(u8**)G_SCRATCH_HEAD     = head1 + 0x28;
    shortScratch->shortVec.vz = nextWork->field_182;
    *(u8**)G_SCRATCH_HEAD     = (u8*)shortScratch;
    gte_SetRotMatrix(&nextWork->field_144);
    gte_ldv0(&shortScratch->shortVec);
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stsv(&nextWork->field_128[1]);
    nextWork->field_128[1].vz += 0x12C;

    head2                 = *(u8**)G_SCRATCH_HEAD;
    shortVec              = (SVECTOR*)(head2 + 0x10);
    *(u8**)G_SCRATCH_HEAD = head2 + 0x18;
    nextWork2             = arg0->field_1C;
    *(u8**)G_SCRATCH_HEAD = (u8*)shortVec;
    shortVec->vx          = 0;
    shortVec->vy          = 0;
    shortVec->vz          = 0x2710;
    gte_SetRotMatrix(&nextWork2->field_144);
    gte_ldv0(shortVec);
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stsv(&nextWork2->field_B0);
    nextWork2->field_E8    = nextWork2->field_B0;
    *(u8**)G_SCRATCH_HEAD += 8;
}

/// Rebuilds the same two direction vectors as `Actor02100_AimAndBuildVectors`
/// from the facing matrix the actor already holds, without re-aiming. The two
/// arms of the frame test hold the same code: the original duplicates this step
/// rather than sharing it, and the branch has no effect on what is written.
static __inline__ void Actor02100_BuildVectors(Actor02100* arg0, Actor02100Work* currentWork)
{
    Actor02100Fn014E4Scratch* scratch;
    Actor02100Fn014E4Scratch* scratch2;
    Actor02100Work*           work;
    Actor02100Work*           nextWork;
    SVECTOR*                  shortVec;
    u8*                       head;
    u8*                       headTail;

    if (currentWork->field_17A == 0xE) {
        work                  = arg0->field_1C;
        work->field_128[0].vz = 0x12C;
        head                  = *(u8**)G_SCRATCH_HEAD;
        scratch               = (Actor02100Fn014E4Scratch*)(head - 0x18);
        work->field_128[0].vx = 0;
        work->field_128[0].vy = 0;
        scratch->shortVec.vx  = 0;
        scratch->shortVec.vy  = 0;
        *(u8**)G_SCRATCH_HEAD = (u8*)scratch;
        scratch->shortVec.vz  = work->field_182;
        gte_SetRotMatrix(&work->field_144);
        head -= 8;
        gte_ldv0((SVECTOR*)head);
        __asm__ volatile("nop; nop; .word 0x4A486012");
    } else {
        work                  = arg0->field_1C;
        work->field_128[0].vz = 0x12C;
        head                  = *(u8**)G_SCRATCH_HEAD;
        scratch2              = (Actor02100Fn014E4Scratch*)(head - 0x18);
        work->field_128[0].vx = 0;
        work->field_128[0].vy = 0;
        scratch2->shortVec.vx = 0;
        scratch2->shortVec.vy = 0;
        *(u8**)G_SCRATCH_HEAD = (u8*)scratch2;
        scratch2->shortVec.vz = work->field_182;
        gte_SetRotMatrix(&work->field_144);
        head -= 8;
        gte_ldv0((SVECTOR*)head);
        __asm__ volatile("nop; nop; .word 0x4A486012");
    }
    gte_stsv(&work->field_128[1]);
    work->field_128[1].vz += 0x12C;
    *(u8**)G_SCRATCH_HEAD += 0x18;
    SOFT_COMPILER_BARRIER();

    headTail              = *(u8**)G_SCRATCH_HEAD;
    shortVec              = (SVECTOR*)(headTail - 8);
    nextWork              = arg0->field_1C;
    *(u8**)G_SCRATCH_HEAD = (u8*)shortVec;
    shortVec->vx          = 0;
    shortVec->vy          = 0;
    shortVec->vz          = 0x2710;
    gte_SetRotMatrix(&nextWork->field_144);
    gte_ldv0(shortVec);
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stsv(&nextWork->field_B0);
    nextWork->field_E8     = nextWork->field_B0;
    *(u8**)G_SCRATCH_HEAD += 8;
}

/// Four-state sweep with a charge-up, a strike and a recovery wait. State 0 aims
/// at the target every frame until `Actor02100_Fn014E4` loses it - which drops
/// straight to the recovery state - starts the loop sound on the first frame and
/// draws the beam from the second, and advances to state 1 once the frame count
/// reaches the per-variant limit in `Actor02100_D03D88`. State 1 stops the loop
/// sound on its first frame, rebuilds the vectors without re-aiming and draws
/// for fifteen frames, then fires the impact sound and enters state 2. State 2
/// shows the two hit objects for one frame, picks the impact sound from the
/// variant index, hides them again after four frames and recovers. State 3 waits
/// out the per-variant recovery count, restores the actor's stored position and
/// returns to state 0.
void Actor02100_Fn016EC(Actor02100* arg0)
{
    Actor02100Work* work;
    GsCOORDINATE2*  coord;
    s32             sound;
    s32             soundId;
    s32             packed;
    s32             flagBit;
    s16             state;
    s16             frame;

    *(u8**)G_SCRATCH_HEAD -= 0x48;
    work                   = arg0->field_1C;
    state                  = work->field_174;
    coord                  = arg0->field_2C->field_8;
    flagBit                = 0x20000;
    sound                  = 0;

    switch (state) {
        case 0:
            if (work->field_17A != 0) {
                if (Actor02100_Fn014E4(arg0) == 0) {
                    work->field_174 = 3;
                    work->field_17A = 0;
                    if (work->field_188 == 2) {
                        SndEvt_EnqueueType7(work->field_168, 1);
                        work->field_188 = 0;
                    }
                    break;
                }

                Actor02100_AimAndBuildVectors(arg0);
            }

            if (work->field_17A == 1) {
                work->field_168 = ((arg0->field_20->field_8 >> 12) << 8) | 0x40150001;
                SndEvt_EnqueueType6(work->field_168, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
                work->field_188 = 2;
            }
            if (work->field_17A >= 2) {
                Actor02100_Fn034E0(arg0);
                Actor02100_Fn02924(arg0, 0);
            }
            work->field_78.flags |= 0x8000;
            frame                 = (u16)work->field_17A + 1;
            work->field_17A       = frame;
            work->field_C8.flags |= 0x8000;
            work->field_78.flags |= 0x4000;
            work->field_C8.flags |= 0x4000;
            if (frame >= Actor02100_D03D88[work->field_178].bounds.field_0) {
                work->field_17A = 0;
                work->field_174 = 1;
            }
            break;

        case 1:
            if (work->field_17A == state) {
                SndEvt_EnqueueType7(work->field_168, 1);
                work->field_188 = 0;
            }
            Actor02100_BuildVectors(arg0, work);
            Actor02100_Fn034E0(arg0);
            Actor02100_Fn02924(arg0, 0);
            frame           = (u16)work->field_17A + 1;
            work->field_17A = frame;
            if (frame >= 0xF) {
                work->field_17A = 0;
                work->field_174 = 2;
                soundId         = ((arg0->field_20->field_8 >> 12) << 8) | 0x40150002;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            break;

        case 2:
            Actor02100_Fn034E0(arg0);
            Actor02100_Fn02924(arg0, 1);
            frame = work->field_17A;
            if (frame == 1) {
                work->field_78.field_18 = Gp_PackPair(&Actor02100_D03D64, work->field_178);
                packed                  = work->field_178 + 0x26;
                work->field_C8.field_18 = flagBit;
                work->field_C8.field_18 = (packed << 8) | (packed | work->field_C8.field_18);
                switch (work->field_178) {
                    case 0:
                        sound = 0x40150003;
                        break;
                    case 1:
                        sound = 0x40150004;
                        break;
                    case 2:
                        sound = 0x40150005;
                        break;
                    case 3:
                        sound = 0x40150006;
                        break;
                }
                soundId = sound | ((arg0->field_20->field_8 >> 12) << 8);
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)Gp_GetObjDepth((GpObj38*)coord));
            } else if (frame == state) {
                work->field_78.field_18 = 0;
                work->field_C8.field_18 = 0;
            }
            frame           = (u16)work->field_17A + 1;
            work->field_17A = frame;
            if (frame >= 4) {
                work->field_174       = 3;
                work->field_17A       = 0;
                work->field_78.flags &= 0x7FFF;
                work->field_C8.flags &= 0x7FFF;
                work->field_78.flags &= 0xBFFF;
                work->field_C8.flags &= 0xBFFF;
            }
            break;

        case 3:
            frame           = (u16)work->field_17A + 1;
            work->field_17A = frame;
            if (frame >= Actor02100_D03D88[work->field_178].bounds.field_2) {
                work->field_17A = 0;
                work->field_174 = 0;
                work->field_172 = work->field_176 != 0;
                work->field_118 = (u16)work->field_120;
                work->field_11A = (u16)work->field_122;
                work->field_11C = (u16)work->field_124;
            }
            break;
    }

    *(u8* volatile*)G_SCRATCH_HEAD += 0x48;
}

/// Points the actor at its stored target. Rotates a fixed forward offset by the
/// coordinate's matrix, maps it back into the coordinate's own frame, and hands
/// the vector from there to the target position to `Gp_OrientAlong`, which
/// writes the facing matrix at `field_144`.
static __inline__ void Actor02100_OrientScratch(Actor02100* arg0)
{
    Actor02100Fn01FF0Scratch* scratch;
    Actor02100Work*           work;
    GsCOORDINATE2*            coord;
    u8*                       head;

    coord                 = arg0->field_2C->field_8;
    head                  = *(u8**)G_SCRATCH_HEAD;
    work                  = arg0->field_1C;
    scratch               = (Actor02100Fn01FF0Scratch*)(head - 0x28);
    *(u8**)G_SCRATCH_HEAD = (u8*)scratch;

    scratch->shortVec.vx = 0;
    scratch->shortVec.vy = 0;
    scratch->shortVec.vz = 0x12C;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&scratch->shortVec);
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stlvnl(&scratch->transformed);
    scratch->transformed.vx += coord->workm.t[0];
    scratch->transformed.vy += coord->workm.t[1];
    scratch->transformed.vz += coord->workm.t[2];
    ApplyTransposeMatrixLV(&coord->workm, &scratch->transformed, &scratch->delta);
    scratch->transformed.vx = work->field_108.vx - scratch->delta.vx;
    scratch->transformed.vy = work->field_108.vy - scratch->delta.vy;
    scratch->transformed.vz = work->field_108.vz - scratch->delta.vz;
    Gp_OrientAlong(&scratch->transformed, &work->field_144, 0);
}

/// Refreshes the two vectors the actor's facing matrix defines: the near one at
/// `field_128[1]`, rotated from the length in `field_182` and advanced by 0x12C
/// each frame, and the far one at `field_B0`, a fixed 0x2710 ahead, which is
/// mirrored into `field_E8`. Both rotations borrow an `SVECTOR` from
/// `G_SCRATCH_HEAD`; the first reuses the block `Actor02100_OrientScratch` just
/// released.
static __inline__ void Actor02100_UpdateVectors(Actor02100* arg0)
{
    Actor02100Fn01FF0Scratch* scratch;
    Actor02100Fn014E4Scratch* inner;
    Actor02100Work*           work;
    Actor02100Work*           work2;
    SVECTOR*                  shortVec;
    u8*                       head;
    u8*                       head2;
    u16                       vz;

    head    = *(u8**)G_SCRATCH_HEAD;
    work    = arg0->field_1C;
    scratch = (Actor02100Fn01FF0Scratch*)head;
    inner   = (Actor02100Fn014E4Scratch*)(head + 0x10);

    work->field_128[0].vx = 0;
    work->field_128[0].vy = 0;
    work->field_128[0].vz = 0x12C;
    inner->shortVec.vx    = 0;
    inner->shortVec.vy    = 0;
    vz                    = work->field_182;
    SOFT_COMPILER_BARRIER();
    *(u8**)G_SCRATCH_HEAD = head + 0x28;
    SOFT_COMPILER_BARRIER();
    *(u8**)G_SCRATCH_HEAD = (u8*)inner;
    inner->shortVec.vz    = vz;
    gte_SetRotMatrix(&work->field_144);
    gte_ldv0(&scratch->shortVec);
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stsv(&work->field_128[1]);
    head2                  = *(u8**)G_SCRATCH_HEAD;
    work->field_128[1].vz += 0x12C;

    work2                 = arg0->field_1C;
    shortVec              = (SVECTOR*)(head2 + 0x10);
    *(u8**)G_SCRATCH_HEAD = head2 + 0x18;
    SOFT_COMPILER_BARRIER();
    *(u8**)G_SCRATCH_HEAD = (u8*)shortVec;
    shortVec->vx          = 0;
    shortVec->vy          = 0;
    shortVec->vz          = 0x2710;
    gte_SetRotMatrix(&work2->field_144);
    gte_ldv0(shortVec);
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stsv(&work2->field_B0);
    work2->field_E8        = work2->field_B0;
    *(u8**)G_SCRATCH_HEAD += 8;
}

/// Rewrites the far vector alone, without the per-frame near vector: the same
/// 0x2710 offset through the facing matrix into `field_B0`, mirrored into
/// `field_E8`.
static __inline__ void Actor02100_SetVector(Actor02100* arg0)
{
    Actor02100Work* work;
    SVECTOR*        shortVec;
    u8*             head;

    head     = *(u8**)G_SCRATCH_HEAD;
    work     = arg0->field_1C;
    shortVec = (SVECTOR*)(head - 8);
    SOFT_TOUCH_REG(head);
    ((SVECTOR*)(head - 8))->vx = 0;
    shortVec->vz               = 0x2710;
    *(u8**)G_SCRATCH_HEAD      = (u8*)shortVec;
    shortVec->vy               = 0;
    gte_SetRotMatrix(&work->field_144);
    gte_ldv0(shortVec);
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stsv(&work->field_B0);
    work->field_E8         = work->field_B0;
    *(u8**)G_SCRATCH_HEAD += 8;
}

static __inline__ void Actor02100_ReleaseScratch28(void)
{
    *(u8**)G_SCRATCH_HEAD += 0x28;
}

/// Seven-state attack cycle, run from `Actor02100_Fn031C4`. State 0 holds the
/// wind-up: it re-aims each frame, refreshes both direction vectors, starts the
/// looping sound on the second frame and shows the two objects, until the
/// wind-up frame count in `Actor02100_D03D88` runs out. State 1 stops that
/// sound and waits four frames; state 2 emits the two effects with a randomised
/// parameter, plays the strike sound and re-aims once; states 3 and 4 set and
/// clear the pair table entry and the colour word that make the strike hit,
/// counting one hit in `field_17C`; state 5 loops back to state 2 until ten
/// hits, then hides both objects; state 6 waits out the recovery frame count
/// and returns to state 0. `Actor02100_Fn014E4` failing at any aim point drops
/// straight to state 6.
void Actor02100_Fn01FF0(Actor02100* arg0)
{
    Actor02100Fn01FF0Block* root;
    Actor02100Work*         work;
    GsCOORDINATE2*          coord;
    u8*                     rootHead;
    s32                     pan0;
    s32                     pan2;
    s32                     sound2;
    u32                     random;
    s32                     packed2;
    s32                     packed3;
    s32                     shifted;
    s32                     orTmp;
    s32                     result3;
    s16                     state;
    s16                     frame0;
    s16                     frame1;
    s16                     frame6;
    u16                     flags96;
    u16                     flagsE6;

    rootHead              = *(u8**)G_SCRATCH_HEAD - 0x48;
    *(u8**)G_SCRATCH_HEAD = rootHead;
    root                  = (Actor02100Fn01FF0Block*)rootHead;
    work                  = arg0->field_1C;
    state                 = work->field_174;
    coord                 = arg0->field_2C->field_8;

    switch (state) {
        case 0:
            if (work->field_17A != 0) {
                if (Actor02100_Fn014E4(arg0) == 0) {
                    work->field_174 = 6;
                    work->field_17A = 0;
                    if (work->field_188 == 2) {
                        SndEvt_EnqueueType7(work->field_168, 1);
                        work->field_188 = 0;
                    }
                    break;
                }

                Actor02100_OrientScratch(arg0);
                Actor02100_UpdateVectors(arg0);
            }

            if (work->field_17A == 1) {
                work->field_168 = (((u16)arg0->field_20->field_8 >> 12) << 8) | 0x40150001;
                pan0            = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(work->field_168, pan0, (s8)Gp_GetObjDepth((GpObj38*)coord));
                work->field_188 = 2;
            }
            if (work->field_17A >= 2) {
                Actor02100_Fn034E0(arg0);
                Actor02100_Fn02924(arg0, 0);
            }
            work->field_78.flags |= 0x8000;
            frame0                = (u16)work->field_17A + 1;
            work->field_17A       = frame0;
            work->field_C8.flags |= 0x8000;
            work->field_78.flags |= 0x4000;
            work->field_C8.flags |= 0x4000;
            if (frame0 >= Actor02100_D03D88[work->field_178].bounds.field_0) {
                work->field_17A = 0;
                work->field_174 = 1;
            }
            break;

        case 1:
            if (work->field_17A == 1) {
                SndEvt_EnqueueType7(work->field_168, 1);
                work->field_188 = 0;
            }
            frame1          = (u16)work->field_17A + 1;
            work->field_17A = frame1;
            if (frame1 >= 4) {
                work->field_17A = 0;
                Actor02100_SetVector(arg0);
                work->field_174 = 2;
            }
            break;

        case 2:
            root->shortVec.vx = 0;
            root->shortVec.vy = 0;
            root->shortVec.vz = 0x12C;
            random            = Gp_LcgState * 5 + 0x71357911;
            packed2           = ((random >> 16) & 0x1FF) | 0x200;
            Gp_LcgState       = random;
            Gp_SpawnEff(0x60034, coord, packed2, &root->shortVec);
            Gp_SpawnEff(0x60072, coord, packed2, &root->shortVec);
            sound2 = (((u16)arg0->field_20->field_8 >> 12) << 8) | 0x4015000B;
            pan2   = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(sound2, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
            work->field_174 = 3;
            if (Actor02100_Fn014E4(arg0) == 0) {
                work->field_174 = 6;
                work->field_17A = 0;
            } else {
                Actor02100_OrientScratch(arg0);
                Actor02100_ReleaseScratch28();
            }
            break;

        case 3:
            result3 = Gp_PackPair(&Actor02100_D03D64, work->field_178);
            packed3 = work->field_178;
            SOFT_TOUCH_REG_USE(result3, packed3);
            work->field_78.field_18 = result3;
            work->field_174         = 4;
            packed3                += 0x26;
            shifted                 = packed3 << 8;
            orTmp                   = packed3 | 0x20000;
            work->field_C8.field_18 = shifted | orTmp;
            work->field_17C         = (u16)work->field_17C + 1;
            break;

        case 4:
            work->field_78.field_18 = 0;
            work->field_C8.field_18 = 0;
            work->field_174         = 5;
            break;

        case 5:
            if (work->field_17C < 10) {
                work->field_174 = 2;
                SOFT_COMPILER_BARRIER();
                Actor02100_SetVector(arg0);
                break;
            }
            work->field_174 = 6;
            work->field_17A = 0;
            flags96         = work->field_78.flags & 0x7FFF;
            flagsE6         = work->field_C8.flags & 0x7FFF;
            SOFT_TOUCH_REG2(flags96, flagsE6);
            work->field_78.flags = flags96;
            work->field_C8.flags = flagsE6;
            work->field_78.flags = flags96 & 0xBFFF;
            work->field_C8.flags = flagsE6 & 0xBFFF;
            break;

        case 6:
            frame6          = (u16)work->field_17A + 1;
            work->field_17A = frame6;
            if (frame6 >= Actor02100_D03D88[work->field_178].bounds.field_2) {
                work->field_17A = 0;
                work->field_174 = 0;
                work->field_172 = work->field_176 != 0;
                work->field_118 = (u16)work->field_120;
                work->field_11A = (u16)work->field_122;
                work->field_11C = (u16)work->field_124;
            }
            break;
    }

    *(u8**)G_SCRATCH_HEAD += 0x48;
}

/// Draws one beam between the two screen points held in `Actor02100Work`
/// (`field_18C`/`field_190` and their depths in `field_194`). The span is
/// normalised once, its y component negated, and the beam then emitted as eight
/// segments of rising depth; a segment nearer than 30 is dropped. Each segment
/// is a bright centre line plus two gouraud quads that fade from the beam
/// colour on that line to black at the edges, all linked into the ordering
/// table at the segment's own depth and followed by a draw-mode packet. `arg1`
/// selects the style: it picks the edge offsets out of `Actor02100_D03DD8` and
/// the colour triplet out of `Actor02100_D03D88`, and style 1 draws its centre
/// line in flat grey instead of the table colour.
void Actor02100_Fn02924(Actor02100* arg0, s32 arg1)
{
    Actor02100Fn02924Corners* corners;
    POLY_G4*                  quad;
    DR_TPAGE*                 mode;
    LINE_F2*                  line;
    Actor02100Fn02924Scratch* scratch;
    u8*                       head;
    u8*                       newHead;
    s32                       stepX;
    s32                       stepY;
    s32*                      quadSlot;
    s32*                      modeSlot;
    s32*                      lineSlot;
    s32                       next;
    s32                       offsetX0;
    s32                       offsetX1;
    s32                       normalY;
    u8                        blue;
    s32                       offsetY0;
    s32                       offsetY1;
    s32                       depth;
    s32                       corner;
    s32                       segment;
    s32                       spanX;
    s32                       spanY;
    s32                       spanZ;
    Actor02100Work*           work;

    head                  = *(u8**)G_SCRATCH_HEAD;
    work                  = arg0->field_1C;
    newHead               = head - 0x3C;
    *(u8**)G_SCRATCH_HEAD = newHead;
    scratch               = (Actor02100Fn02924Scratch*)newHead;

    ((Actor02100Fn02924Scratch*)(head - 0x3C))->delta.vx = work->field_18C[1] - work->field_18C[0];
    scratch->delta.vy                                    = work->field_190[1] - work->field_190[0];
    scratch->delta.vz                                    = 0;
    VectorNormalS((VECTOR*)newHead, &scratch->normal);
    normalY            = -scratch->normal.vy;
    scratch->normal.vy = normalY;

    spanX = work->field_18C[1] - work->field_18C[0];
    if (spanX < 0) {
        spanX += 7;
    }
    scratch->stepX = (s16)(spanX >> 3);
    spanY          = work->field_190[1] - work->field_190[0];
    if (spanY < 0) {
        spanY += 7;
    }
    scratch->stepY = (s16)(spanY >> 3);
    spanZ          = work->field_194[1] - work->field_194[0];
    if (spanZ < 0) {
        spanZ += 7;
    }
    scratch->depthStep = spanZ >> 3;
    segment            = 0;

    do {
        next           = segment + 1;
        depth          = (scratch->depthStep * next) + work->field_194[0];
        scratch->depth = depth;
        if (depth >= 0x1E) {
            stepX         = scratch->stepX;
            scratch->x[0] = (u16)((u16)work->field_18C[0] + (stepX * segment));
            scratch->x[1] = (u16)((u16)work->field_18C[0] + (stepX * next));
            corner        = 0;
            offsetX0 =
                (s32)((s32)(scratch->normal.vy * Actor02100_D03DD8[work->field_178].styles[arg1].first * 0x300) >>
                      0xC) /
                (s32)scratch->depth;
            scratch->x[2] = (s16)(scratch->x[0] + offsetX0);
            scratch->x[3] = (s16)(scratch->x[1] + offsetX0);
            offsetX1 =
                (s32)((s32)(scratch->normal.vy * Actor02100_D03DD8[work->field_178].styles[arg1].second * 0x300) >>
                      0xC) /
                (s32)scratch->depth;
            stepY         = scratch->stepY;
            scratch->x[4] = (s16)(scratch->x[0] + offsetX1);
            scratch->x[5] = (s16)(scratch->x[1] + offsetX1);
            scratch->y[0] = (u16)((u16)work->field_190[0] + (stepY * segment));
            scratch->y[1] = (u16)((u16)work->field_190[0] + (stepY * next));
            offsetY0 =
                (s32)((s32)(scratch->normal.vx * Actor02100_D03DD8[work->field_178].styles[arg1].first * 0x300) >>
                      0xC) /
                (s32)scratch->depth;
            scratch->y[2] = (s16)(scratch->y[0] + offsetY0);
            COMPILER_BARRIER();
            scratch->y[3] = (s16)(scratch->y[1] + offsetY0);
            offsetY1 =
                (s32)((s32)(scratch->normal.vx * Actor02100_D03DD8[work->field_178].styles[arg1].second * 0x300) >>
                      0xC) /
                (s32)scratch->depth;
            scratch->y[4] = (s16)(scratch->y[0] + offsetY1);
            scratch->y[5] = (s16)(scratch->y[1] + offsetY1);

            do {
                quad           = (POLY_G4*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)((u8*)quad + 0x24);
                setlen(quad, 8);
                setcode(quad, 0x3A);
                corners  = &Actor02100_D03E1C[corner];
                quad->x0 = (u16)scratch->x[corners->corners[0]];
                quad->y0 = (u16)scratch->y[corners->corners[0]];
                quad->x1 = (u16)scratch->x[corners->corners[1]];
                quad->y1 = (u16)scratch->y[corners->corners[1]];
                quad->x2 = (u16)scratch->x[corners->corners[2]];
                quad->y2 = (u16)scratch->y[corners->corners[2]];
                quad->x3 = (u16)scratch->x[corners->corners[3]];
                quad->y3 = (u16)scratch->y[corners->corners[3]];
                quad->r0 = (u8)Actor02100_D03D88[work->field_178].shorts[(arg1 * 3) + 2];
                quad->g0 = (u8)Actor02100_D03D88[work->field_178].shorts[(arg1 * 3) + 3];
                quad->b0 = (u8)Actor02100_D03D88[work->field_178].shorts[(arg1 * 3) + 4];
                quad->r1 = (u8)Actor02100_D03D88[work->field_178].shorts[(arg1 * 3) + 2];
                quad->g1 = (u8)Actor02100_D03D88[work->field_178].shorts[(arg1 * 3) + 3];
                blue     = (u8)Actor02100_D03D88[work->field_178].shorts[(arg1 * 3) + 4];
                quad->r2 = 0;
                quad->g2 = 0;
                quad->b2 = 0;
                quad->r3 = 0;
                quad->g3 = 0;
                quad->b3 = 0;
                quad->b1 = blue;
                corner  += 1;
                setaddr(quad,
                        getaddr((((u32)(scratch->depth << Display_State.field_128) >> 2) & 0xFFC) +
                                (u32)Gpu_CurrentOt));
                quadSlot = (s32*)((((u32)(scratch->depth << Display_State.field_128) >> 2) & 0xFFC) +
                                  (u32)Gpu_CurrentOt);
                setaddr(quadSlot, quad);
            } while (corner < 2);

            line           = (LINE_F2*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)((u8*)line + 0x10);
            setlen(line, 3);
            setcode(line, 0x42);
            line->x0 = (u16)scratch->x[0];
            line->y0 = (u16)scratch->y[0];
            line->x1 = (u16)scratch->x[1];
            line->y1 = (u16)scratch->y[1];
            if (arg1 == 1) {
                line->r0 = 0x80U;
                line->g0 = 0x80U;
                line->b0 = 0x80U;
            } else {
                line->r0 = (u8)Actor02100_D03D88[work->field_178].shorts[(arg1 * 3) + 2];
                line->g0 = (u8)Actor02100_D03D88[work->field_178].shorts[(arg1 * 3) + 3];
                line->b0 = (u8)Actor02100_D03D88[work->field_178].shorts[(arg1 * 3) + 4];
            }
            setaddr(line,
                    getaddr((((u32)(scratch->depth << Display_State.field_128) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt));
            mode           = Gpu_PrimCursor;
            lineSlot       = (s32*)((((u32)(scratch->depth << Display_State.field_128) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt);
            Gpu_PrimCursor = (DR_TPAGE*)((u8*)mode + 8);
            setaddr(lineSlot, line);
            setlen(mode, 1);
            mode->code[0] = 0xE1000620;
            setaddr(mode,
                    getaddr((((u32)(scratch->depth << Display_State.field_128) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt));
            modeSlot = (s32*)((((u32)(scratch->depth << Display_State.field_128) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt);
            setaddr(modeSlot, mode);
        }
        segment += 1;
    } while (segment < 8);

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + 0x3C;
}

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
    obj->field_C     = 0;
    arg0->node.flags = 8;
    goto body;
case1:
    Actor02100_Fn03488(arg1);
    return;
case2:
    obj->field_C     = 0x80;
    arg0->node.flags = one;
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

/// Projects the two `field_128` points through the actor's own coordinate,
/// storing screen x/y in `field_18C`/`field_190` and depth in `field_194`.
void Actor02100_Fn034E0(Actor02100* arg0)
{
    Actor02100Screen* scratch;
    GsCOORDINATE2*    coord;
    Actor02100Work*   work;
    s32               i;
    u8*               head;
    s32               y;

    head                  = *(u8**)G_SCRATCH_HEAD;
    work                  = arg0->field_1C;
    *(u8**)G_SCRATCH_HEAD = head - 8;
    scratch               = (Actor02100Screen*)*(u8**)G_SCRATCH_HEAD;
    coord                 = arg0->field_2C->field_8;
    for (i = 0; i < 2; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_SetTransMatrix(&coord->workm);
        gte_ldv0(&work->field_128[i]);
        gte_rtps_real();
        gte_stsxy(&scratch->sxy);
        gte_stszotz(&scratch->sz);
        work->field_18C[i] = scratch->sxy.vx;
        y                  = scratch->sxy.vy;
        work->field_190[i] = y;
        work->field_194[i] = scratch->sz;
    }
    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + 8;
}

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
    Gp_UnlinkObj(&work->field_40);
    Gp_UnlinkObj(&work->field_78);
    Gp_UnlinkObj(&work->field_C8);
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
