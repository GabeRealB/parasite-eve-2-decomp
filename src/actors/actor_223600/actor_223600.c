#include "common.h"

#include "actors/actor_223600.h"
#include "actors/actors_shared_80134178.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014A170);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014A4B8);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014AA04);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014ABA8);

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014B2F4);

/// In motion states 2 and 3, reports 0x400C0001 the first time the animation id
/// in `field_4A` reaches one of that state's trigger ids (latched in
/// `field_208`); in state 5, 0x400C0005 while bit 2 of `field_58` is set.
/// Returns 0 otherwise.
s32 func_actor_223600_8014B464(Actor223600Work* arg0)
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
            if (arg0->field_208 == v) {
                goto same;
            }
            arg0->field_208 = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_208 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_208 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

/// Normalises `dir` in place and scales it to `amount`/0x1000 of unit length on
/// the GTE. The pointer stays in one register across `VectorNormalSS` because
/// the GTE loads read it back afterwards.
static __inline__ void Actor223600_ScaleForward(SVECTOR* dir, s16 amount)
{
    VectorNormalSS(dir, dir);
    gte_lddp(amount);
    gte_ldsv(dir);
    gte_gpf12_real();
    gte_stsv(dir);
}

/// Steps the model `amount` units along its facing -- the coordinate matrix's z
/// column, normalised and GTE-scaled in a scratch-pad vector -- and invalidates
/// the coordinate. Skipped entirely while `D_80072729` is 1.
static __inline__ void Actor223600_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        Actor223600_ScaleForward(vec, amount);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// Folds a yaw difference back into +/-0x800, a twelfth of a turn either way.
static __inline__ s16 Actor223600_NormalizeYaw(s16 input)
{
    s16 value = input;
    if (input < 0) {
        while (1) {
            if (value >= -0x800) {
                break;
            }
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800) {
                break;
            }
            value -= 0x1000;
        }
    }
    return value;
}

/// Spawn state of this enemy: allocates the 0x214 work block, publishes it as
/// `Task::work`, reparents the model to `gGfxViewCoord`, seeds its animation
/// slots from `D_actor_223600_801509C0` and hangs the enemy's display node off
/// part 2 of the model's coordinate array. HP and max HP both come from
/// `D_actor_223600_8014CFCC`, which also picks the opening motion through
/// `func_actor_223600_8014B2F4`. The context's top `field_8` nibble biases the
/// three timers in `field_176`, `field_184` and `field_186` -- up by the nibble
/// when its low bit is set, down by half of it otherwise. The model's world
/// position is sampled into `field_194`..`field_198` and its facing is
/// normalised and scaled on the GTE, and the instance is published as the
/// overlay's anchor `D_actor_223600_80150B5C`.
void func_actor_223600_8014B540(GpEnemy* enemy, Task* task)
{
    SVECTOR          dir;
    Actor223600Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    u32              scale;
    u32              flag;
    s32              hp;

    obj        = (TmdObject*)task->extra;
    coord      = obj->coords;
    work       = memCalloc(sizeof(Actor223600Work), false);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    coord->sub     = &gGfxViewCoord;
    task->msgTable = D_actor_223600_80150B28;
    obj->flags     = 0;
    func_800B3F84(&work->anim, D_actor_223600_801509C0, obj, work->poses, work->slots);

    enemy->field_4     = &coord->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &((TmdObject*)task->extra)->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags = 1;
    enemy->field_42   = 1;
    enemy->field_40   = 1;
    enemy->field_4C   = 0;
    hp                = D_actor_223600_8014CFCC.hpMax;
    enemy->field_50   = &D_actor_223600_8014CFCC;
    enemy->field_54   = 0;
    enemy->field_42   = hp;
    enemy->field_40   = hp;

    work->field_170 = 2;
    work->field_174 = 1;
    work->field_176 = 0x10;
    work->field_178 = 0;
    func_actor_223600_8014B2F4(task, hp);
    work->field_17E = 0;
    work->field_8   = 0;
    obj->lightMtx   = &work->field_1A8;
    obj->colorMtx   = &work->field_1C8;
    coord->flg      = 0;
    work->field_184 = 5;
    work->field_186 = 0x14;

    scale = (u16)(enemy->field_8 >> 12);
    flag  = scale & 1;
    if (flag == 1) {
        work->field_176 += enemy->field_8 >> 12;
        work->field_186 += enemy->field_8 >> 12;
        work->field_184 += enemy->field_8 >> 12;
    } else {
        work->field_176 -= scale >> 1;
        work->field_186 -= enemy->field_8 >> 13;
        work->field_184 -= enemy->field_8 >> 13;
    }

    work->field_194 = ((Actor223600CoordPos*)((TmdObject*)task->extra)->coords)->x;
    work->field_196 = ((Actor223600CoordPos*)((TmdObject*)task->extra)->coords)->y;
    work->field_198 = ((Actor223600CoordPos*)((TmdObject*)task->extra)->coords)->z;

    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, &dir);
    dir.vy = 0;
    Actor223600_ScaleForward(&dir, 0x3E8);

    work->field_0 = 0;
    work->field_2 = -1;

    D_actor_223600_80150B5C.coord      = ((TmdObject*)task->extra)->coords;
    D_actor_223600_80150B5C.spawnArgLo = 0x100;
    D_actor_223600_80150B5C.spawnArgHi = 1;
    task->state++;
}

/// Approach state of this enemy. On the frame it is entered (`field_4` set) it
/// allocates the model's draw buffers, seeds the target position in
/// `field_19C`/`field_1A0`, writes the starting world position for this
/// context's top `field_8` nibble -- two spawn points, a third leaving the
/// coordinate alone -- faces the model down +Z and restarts its motion. On
/// every later frame it counts the frame in `field_6`, turns the model by up to
/// 0x10 towards the target (the clamped yaw kept in the scratch block) and
/// walks it 5 units forward.
void func_actor_223600_8014B840(GpEnemy* enemy, Task* task)
{
    Actor223600Work* work;
    Actor223600Turn* head;
    Actor223600Turn* turn;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    u32              mode;

    work = (Actor223600Work*)task->work;
    if (work->field_4 != 0) {
        obj               = (TmdObject*)task->extra;
        enemy->node.flags = 1;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_19C = 0x115D;
        work->field_19E = 1;
        work->field_1A0 = 0x12D5;

        mode = enemy->field_8 >> 12;
        switch (mode) {
            case 0:
                ((TmdObject*)task->extra)->coords->coord.t[0] = 0xA8C;
                ((TmdObject*)task->extra)->coords->coord.t[1] = 1;
                ((TmdObject*)task->extra)->coords->coord.t[2] = 0xA28;
                break;
            case 1:
                ((TmdObject*)task->extra)->coords->coord.t[0] = 0x384;
                ((TmdObject*)task->extra)->coords->coord.t[1] = mode;
                ((TmdObject*)task->extra)->coords->coord.t[2] = 0x960;
                break;
        }
        Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, 0, 1);
        work->field_174 = 2;
        work->field_170 = 2;
        func_actor_223600_8014B2F4(task);
        ((TmdObject*)task->extra)->coords->flg = 0;
        work->field_6                          = 0;
        return;
    }

    work->field_6++;
    head                               = *(Actor223600Turn**)G_SCRATCH_HEAD;
    head[-1].dx                        = work->field_19C - ((Actor223600CoordPos*)((TmdObject*)task->extra)->coords)->x;
    *(Actor223600Turn**)G_SCRATCH_HEAD = head - 1;
    turn                               = head - 1;
    turn->dy                           = 0;
    turn->dz                           = work->field_1A0 - ((Actor223600CoordPos*)((TmdObject*)task->extra)->coords)->z;

    coord     = ((TmdObject*)task->extra)->coords;
    turn->yaw = Actor223600_NormalizeYaw(ratan2(head[-1].dx, turn->dz) -
                                         ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    if (turn->yaw > 0x10) {
        turn->yaw = 0x10;
    }
    if (turn->yaw < -0x10) {
        turn->yaw = -0x10;
    }
    turn->yaw += ratan2(-((TmdObject*)task->extra)->coords->coord.m[2][0],
                        ((TmdObject*)task->extra)->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, turn->yaw, 1);
    Actor223600_MoveForward(((TmdObject*)task->extra)->coords, 5);
    func_actor_223600_8014B2F4(task);
    *(Actor223600Turn**)G_SCRATCH_HEAD    += 1;
    ((TmdObject*)task->extra)->coords->flg = 0;
}

/// Parked state of this enemy. On the frame it is entered (`field_4` set) it
/// allocates the model's draw buffers and drops the model at the spawn point
/// the context's top `field_8` nibble selects -- two of them just face the
/// model and hand it to motion state 2, while the third steps it forward and
/// spins up motion state 0xE, waiting out the restart until `field_58` bit 0
/// comes back. Every later frame it runs one step of the motion the work
/// block's `field_174` names: 0xE raises the model by `field_212` a frame,
/// stepping it forward while the frame counter is inside the walk window, and
/// hands over to 0xF once the model's world Y goes positive; 0xF walks the
/// coordinate along its own axes on the GTE and swings part 1 through the
/// flourish, in a longer form for the nibble-0 context than for the others;
/// state 2 only widens `field_176`. The scratch block comes off
/// `G_SCRATCH_HEAD` under three names -- `head`, whose negative index the
/// world-X step reads, `vec`, which the column and normalise calls take, and
/// `gte`, which the GTE round trip reads back -- and the two `G_SCRATCH_HEAD`
/// pointers are the carve and the release, each materialised where it is used.
void func_actor_223600_8014BBF4(GpEnemy* enemy, Task* task)
{
    Actor223600Work*  work;
    Actor223600Turn** push;
    Actor223600Turn** pop;
    Actor223600Turn*  head;
    SVECTOR*          vec;
    SVECTOR*          gte;
    TmdObject*        obj;
    s32               mode;
    s32               state;
    s16               frame;

    work = (Actor223600Work*)task->work;
    if (work->field_4 != 0) {
        obj               = (TmdObject*)task->extra;
        enemy->node.flags = 1;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        mode = enemy->field_8 >> 12;
        switch (mode) {
            case 0:
                ((TmdObject*)task->extra)->coords->coord.t[0] = 0xA1E;
                ((TmdObject*)task->extra)->coords->coord.t[1] = -0x384;
                ((TmdObject*)task->extra)->coords->coord.t[2] = 0x1590;
                Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, 0x7D0, 1);
                work->field_6   = -0xA;
                work->field_212 = 0xB4;
                work->field_174 = 2;
                work->field_170 = 2;
                break;
            case 1:
                ((TmdObject*)task->extra)->coords->coord.t[0] = 0x12C;
                ((TmdObject*)task->extra)->coords->coord.t[1] = -0x4C4;
                ((TmdObject*)task->extra)->coords->coord.t[2] = 0x1194;
                Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, 0x3E8, 1);
                work->field_212 = 0xBE;
                work->field_6   = 0;
                work->field_174 = 2;
                work->field_170 = 2;
                break;
            case 2:
                ((TmdObject*)task->extra)->coords->coord.t[0] = 0x104A;
                ((TmdObject*)task->extra)->coords->coord.t[1] = -0x384;
                ((TmdObject*)task->extra)->coords->coord.t[2] = 0xFE6;
                Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, -0x400, 1);
                Actor223600_MoveForward(((TmdObject*)task->extra)->coords, 0x15E);
                work->field_6   = 0x3C;
                work->field_212 = 0x50;
                work->field_176 = 0x40;
                work->field_174 = 0xE;
                work->field_170 = mode;
                do {
                    func_actor_223600_8014B2F4(task);
                } while ((work->field_58 & 1) == 0);
                work->field_176 = 0x10;
                work->field_212 = 0x46;
                break;
        }
        func_actor_223600_8014B2F4(task);
        ((TmdObject*)task->extra)->coords->flg = 0;
        return;
    }

    if (work->field_6 == 0) {
        state = work->field_174;
        if (state == 2) {
            work->field_174 = 0xE;
            work->field_170 = state;
        }
    }
    func_actor_223600_8014B2F4(task);

    push  = (Actor223600Turn**)G_SCRATCH_HEAD;
    head  = *push;
    vec   = (SVECTOR*)(head - 1);
    gte   = (SVECTOR*)(head - 1);
    *push = head - 1;

    switch (work->field_174) {
        case 0xE:
            work->field_176 = 0x10;
            if ((enemy->field_8 >> 12) != 2) {
                if (work->field_6 < 0x32) {
                    if (work->field_6 >= 0x28) {
                        Actor223600_MoveForward(((TmdObject*)task->extra)->coords, 0x16);
                    } else if (work->field_6 >= 0x23) {
                        Actor223600_MoveForward(((TmdObject*)task->extra)->coords, 0xA);
                    } else if (work->field_6 >= 0x13) {
                        Actor223600_MoveForward(((TmdObject*)task->extra)->coords, 0xA);
                    }
                }
            }
            frame = (u16)work->field_6;
            if (frame >= 0x28) {
                if ((u16)(frame % 5) < 2) {
                    ((TmdObject*)task->extra)->coords->coord.t[1] += work->field_212 - (frame - 0x28) / 4;
                } else {
                    ((TmdObject*)task->extra)->coords->coord.t[1] += work->field_212 + (frame - 0x28) / 2;
                }
                if (((TmdObject*)task->extra)->coords->coord.t[1] >= 2) {
                    ((TmdObject*)task->extra)->coords->coord.t[1] = 1;
                }
            }
            if (((TmdObject*)task->extra)->coords->coord.t[1] > 0) {
                work->field_174 = 0xF;
                work->field_170 = 2;
                work->field_6   = 0;
            }
            break;
        case 0xF:
            work->field_176 = 0x10;
            if (work->field_6 < 0xB) {
                Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, vec);
                VectorNormalSS(vec, vec);
                gte_lddp(0x23);
                gte_ldsv(gte);
                gte_gpf12_real();
                gte_stsv(gte);
                ((TmdObject*)task->extra)->coords->coord.t[0] += head[-1].dx;
                ((TmdObject*)task->extra)->coords->coord.t[1] += vec->vy;
                ((TmdObject*)task->extra)->coords->coord.t[2] += vec->vz;
            }
            if ((u32)((u16)work->field_6 - 5) < 9) {
                Gfx_MatrixCol1(&((TmdObject*)task->extra)->coords->coord, vec);
                VectorNormalSS(vec, vec);
                gte_lddp(-0x14);
                gte_ldsv(gte);
                gte_gpf12_real();
                gte_stsv(gte);
                ((TmdObject*)task->extra)->coords->coord.t[0] += head[-1].dx;
                ((TmdObject*)task->extra)->coords->coord.t[1] += vec->vy;
                ((TmdObject*)task->extra)->coords->coord.t[2] += vec->vz;
                Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[1].coord,
                               -((work->field_6 - 4) * 0xCC), 0);
            }
            if ((enemy->field_8 >> 12) == 0) {
                if ((u32)((u16)work->field_6 - 0xE) < 0x17) {
                    Gfx_MatrixCol1(&((TmdObject*)task->extra)->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(0xB);
                    gte_ldsv(gte);
                    gte_gpf12_real();
                    gte_stsv(gte);
                    ((TmdObject*)task->extra)->coords->coord.t[0] += head[-1].dx;
                    ((TmdObject*)task->extra)->coords->coord.t[1] += vec->vy;
                    ((TmdObject*)task->extra)->coords->coord.t[2] += vec->vz;
                    Gfx_MatrixCol0(&((TmdObject*)task->extra)->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(-0x1D);
                    gte_ldsv(gte);
                    gte_gpf12_real();
                    gte_stsv(gte);
                    switch ((s16)((u16)work->field_6 - 0xF)) {
                        case 0:
                        case 1:
                        case 3:
                        case 4:
                        case 5:
                        case 7:
                        case 9:
                            ((TmdObject*)task->extra)->coords->coord.t[0] -= gte->vx;
                            ((TmdObject*)task->extra)->coords->coord.t[1] -= gte->vy;
                            ((TmdObject*)task->extra)->coords->coord.t[2] -= gte->vz;
                            Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[1].coord, -0x800, 0);
                            Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords[1].coord,
                                           (work->field_6 - 0xD) * 0x55 - 0x6E, 0);
                            break;
                        default:
                            ((TmdObject*)task->extra)->coords->coord.t[0] += gte->vx;
                            ((TmdObject*)task->extra)->coords->coord.t[1] += gte->vy;
                            ((TmdObject*)task->extra)->coords->coord.t[2] += gte->vz;
                            Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[1].coord, -0x800, 0);
                            Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords[1].coord,
                                           (work->field_6 - 0xD) * 0x55, 0);
                            break;
                    }
                }
                if (work->field_6 >= 0x25) {
                    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[1].coord, -0x800, 0);
                    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords[1].coord, 0x800, 0);
                }
            } else {
                if ((u32)((u16)work->field_6 - 0xE) < 0x10) {
                    Gfx_MatrixCol1(&((TmdObject*)task->extra)->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(0xB);
                    gte_ldsv(gte);
                    gte_gpf12_real();
                    gte_stsv(gte);
                    ((TmdObject*)task->extra)->coords->coord.t[0] += head[-1].dx;
                    ((TmdObject*)task->extra)->coords->coord.t[1] += vec->vy;
                    ((TmdObject*)task->extra)->coords->coord.t[2] += vec->vz;
                    Gfx_MatrixCol0(&((TmdObject*)task->extra)->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(-0x1D);
                    gte_ldsv(gte);
                    gte_gpf12_real();
                    gte_stsv(gte);
                    ((TmdObject*)task->extra)->coords->coord.t[0] += head[-1].dx;
                    ((TmdObject*)task->extra)->coords->coord.t[1] += vec->vy;
                    ((TmdObject*)task->extra)->coords->coord.t[2] += vec->vz;
                    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[1].coord, -0x800, 0);
                    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords[1].coord,
                                   (work->field_6 - 0xD) * 0x78, 0);
                }
                if (work->field_6 >= 0x1E) {
                    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[1].coord, -0x800, 0);
                    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords[1].coord, 0x800, 0);
                }
            }
            break;
        case 2:
            work->field_176 = 0x20;
            break;
    }
    ((TmdObject*)task->extra)->coords->flg = 0;
    ((TmdObject*)task->extra)->coords->flg = 0;
    pop                                    = (Actor223600Turn**)G_SCRATCH_HEAD;
    *pop                                  += 1;
    work->field_6++;
}

/// The three state handlers the tick below picks between by the work block's
/// state word, copied onto the stack before the call. The copy is a three-word
/// block move out of the unit's `.rodata`, which is why the table is a rodata
/// object rather than a local initialiser.
const GpEnemyTaskFuncTable3 D_actor_223600_80149E4C = {
    {
        ActorsShared80134178,
        func_actor_223600_8014B840,
        func_actor_223600_8014BBF4,
    },
};

/// Per-frame tick of the park/unpark machine `ActorsShared80135df4` dispatches
/// to. The game mode word selects a one-shot arm first: mode 0 clears the
/// model's `field_C` when the work block is parked and then falls through to
/// the shared body, mode 1 does the same and returns, and mode 2 forces
/// `field_C` to 0x80 and returns. The shared body records the state change in
/// `field_4` and the dispatched state in `field_2`, runs the state handler,
/// turns the animation latch `func_actor_223600_8014B464` raises into a
/// `SndEvt_EnqueueType6` cue -- the work id from the enemy's `field_8` in its
/// bits 8-11, with the model's pan and depth -- and finally re-parks the model
/// through `func_800D7A9C` while `field_20C` is set, latching it once the
/// non-resident mode or an empty coordinate arrives.
void func_actor_223600_8014CA00(GpEnemy* enemy, Task* task)
{
    Actor223600Work*      work;
    GpEnemyTaskFuncTable3 fns;
    s32                   reaction;
    s32                   cue;
    s32                   pan;

    work = (Actor223600Work*)task->work;
    fns  = D_actor_223600_80149E4C;

    switch (D_801153F4) {
        case 0:
            if (work->field_0 != 0) {
                ((TmdObject*)task->extra)->flags = 0;
            }
            break;
        case 1:
            if (work->field_0 != 0) {
                ((TmdObject*)task->extra)->flags = 0;
            }
            return;
        case 2:
            ((TmdObject*)task->extra)->flags = 0x80;
            return;
    }

    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    fns.funcs[work->field_0](enemy, task);

    reaction = func_actor_223600_8014B464(work);
    if (reaction != 0) {
        cue = reaction | (((u16)enemy->field_8 >> 12) << 8);
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(
            cue, pan,
            (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->coords));
    }
    if (work->field_20C != 0) {
        func_800D7A9C((TmdObject*)task->extra,
                      (VECTOR*)((TmdObject*)task->extra)->coords->workm.t, 0, 3);
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)task->extra)->coords->flg = 0;
    }
    if (((TmdObject*)task->extra)->coords->flg == 0) {
        work->field_20C = 1;
        return;
    }
    work->field_20C = 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_223600/actor_223600", ActorsShared80135df4Table);

/// Message handler (id 0x7D5 in `D_actor_223600_80150B28`). Drives the model's
/// `field_C` flag word and the work block's state word from `arg2`: 0 sets 0x80
/// and rewrites the buffers, 1 clears it and rewrites the buffers, 2 sets bit
/// 2, and 3 clears then sets bit 2. Only case 1 keeps `arg2` as the state.
s32 func_actor_223600_8014CC04(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj  = task->extra;
    Actor223600Work* work = (Actor223600Work*)task->work;

    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = arg2;
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}

/// Message handler (id 0x7DB in `D_actor_223600_80150B28`). Copies the first
/// three bytes of the event packet into the work block, then, for command word
/// 0x302, drives the work block's state word from the packet's sub-command: 1
/// selects 2, 2 and 9 select 0, and 0 is a no-op.
s32 func_actor_223600_8014CCD4(Task* task, s32 arg1, Actor223600Event* event)
{
    Actor223600Work* work;

    work            = (Actor223600Work*)task->work;
    work->field_180 = event->bytes[0];
    work->field_181 = event->bytes[1];
    work->field_182 = event->bytes[2];
    if (event->words[0] == 0x302) {
        switch (event->words[1]) {
            case 9:
                work->field_0 = 0;
                break;
            case 1:
                work->field_0 = 2;
                break;
            case 2:
                work->field_0 = 0;
                break;
            case 0:
                break;
        }
    }
    return 0;
}
