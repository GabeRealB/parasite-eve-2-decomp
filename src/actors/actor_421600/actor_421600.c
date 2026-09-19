#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actor_421600.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80132310);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013285C);

s32 func_actor_421600_80132A00(Actor421600* arg0, s32 arg1, Actor421600Msg* arg2)
{
    Actor421600Work* work;
    GpEnemy*         enemy;
    s32              angle;
    s16              mode;

    work  = arg0->field_1C;
    enemy = arg0->field_20;

    if (arg2->field_0.word == 0x109) {
        switch (arg2->field_2.word) {
            case 1:
                work->field_EAA = work->field_EA8;
                break;
            case 2:
                if (work->field_0 == 0x26) {
                    work->field_0 = 0x26;
                }
                break;
            case 3:
                if (work->field_0 == 1) {
                    work->field_0 = 2;
                }
                break;
        }
        return 1;
    }

    work->field_E90.bytes[0] = arg2->field_0.bytes[0];
    work->field_E90.bytes[1] = arg2->field_0.bytes[1];
    work->field_E90.bytes[2] = arg2->field_2.bytes[0];

    if (arg2->field_0.word != 0x1402) {
        return 0;
    }

    switch (arg2->field_2.word) {
        case 0:
            enemy->hp = D_actor_421600_8013EF3C;
            if ((enemy->placeKey >> 12) == 0) {
                work->field_0 = 2;
            }
            return 1;

        case 1:
            mode      = enemy->placeKey >> 12;
            enemy->hp = D_actor_421600_8013EF3C;
            switch (mode) {
                case 0:
                    if (D_actor_421600_80151268 < 4) {
                        goto negstate;
                    }
                    if (work->field_0 != 0) {
                        goto tail;
                    }
                    arg0->field_2C->coords->coord.t[0] = 0x1057;
                    arg0->field_2C->coords->coord.t[2] = -0x11A3;
                    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, -0x400, 1);
                    arg0->field_2C->coords->flg = 0;
                    Gp_UpdateCoord(arg0->field_2C->coords);
                    work->field_0 = 0x20;
                    work->field_2 = -1;
                    goto tail;
                case 1:
                    if (D_actor_421600_80151268 < 5) {
                        goto negstate;
                    }
                    if (work->field_0 != 0) {
                        goto tail;
                    }
                    arg0->field_2C->coords->coord.t[0] = 0x1467;
                    arg0->field_2C->coords->coord.t[2] = 0x4B9;
                    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0x7BC, 1);
                    arg0->field_2C->coords->flg = 0;
                    Gp_UpdateCoord(arg0->field_2C->coords);
                    work->field_0 = 0x20;
                    work->field_2 = -1;
                    goto tail;
            }
            goto tail;
        negstate:
            work->field_0 = 0;
            work->field_2 = -1;
        tail:
            Gp_SetLightMode(enemy, 0);
            enemy->reactionFlags = 0;
            enemy->hp            = D_actor_421600_8013EF3C;
            return 1;

        case 2:
            switch (enemy->placeKey >> 12) {
                case 0:
                    if (D_actor_421600_80151268 <= 0) {
                        goto blockDE0;
                    }
                    arg0->field_2C->coords->coord.t[0] = -0xD40;
                    arg0->field_2C->coords->coord.t[2] = 0x104F;
                    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, -0x76C, 1);
                    arg0->field_2C->coords->flg = 0;
                    Gp_UpdateCoord(arg0->field_2C->coords);
                    Gp_SetLightMode(enemy, 0);
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_421600_8013EF3C;
                    work->field_0        = 6;
                    goto blockDE0;
                case 1:
                    if (D_actor_421600_80151268 < 2) {
                        goto blockDE0;
                    }
                    arg0->field_2C->coords->coord.t[0] = 0x138C;
                    arg0->field_2C->coords->coord.t[2] = 0x4B2;
                    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0x7BC, 1);
                    arg0->field_2C->coords->flg = 0;
                    Gp_UpdateCoord(arg0->field_2C->coords);
                    Gp_SetLightMode(enemy, 0);
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_421600_8013EF3C;
                    work->field_0        = 6;
                    goto blockDE0;
                default:
                    goto blockDE0;
            }
        blockDE0:
            if (D_actor_421600_80151268 == 0) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 0, 0);
            }
            return 1;

        case 3:
            if (work->field_E9C == 1) {
                work->field_E9C = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
            }
            if (work->field_0 != 0x14 && work->field_0 != 0x11 && work->field_0 != 0x15 &&
                work->field_0 != 0x16 && work->field_0 != 0 && work->field_0 != 8) {
                work->field_0 = 5;
                work->field_2 = -1;
            }
            return 1;

        case 9:
            work->field_0 = 0;
            work->field_2 = -1;
            return 1;

        default:
            return 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80132EC0);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80133334);

void func_actor_421600_80133444(GsCOORDINATE2* arg0)
{
    SVECTOR                  vec;
    SVECTOR*                 dir;
    Actor421600ArenaScratch* blk;
    u8*                      head;
    s32                      outside;
    u32                      spad_a;
    u32                      spad_b;

    if ((u32)(arg0->coord.t[0] - 0x1F5) < 0x3E7) {
        if (arg0->coord.t[2] < 0x1F4) {
            if (arg0->coord.t[2] < -0x1F4) {
                head                           = *(void**)G_SCRATCH_HEAD;
                blk                            = (Actor421600ArenaScratch*)(head - 0xC);
                spad_a                         = (u32)PSX_SCRATCH;
                *(void**)((u8*)spad_a + 0x3FC) = blk;
                vec.vx                         = (u16)arg0->coord.t[0] - 0x3E8;
                vec.vy                         = 0;
                vec.vz                         = (u16)arg0->coord.t[2] + 1;
                blk->field_0                   = vec.vx;
                dir                            = &vec;
                blk->field_4                   = dir->vz;
                blk->field_8                   = 0x2D0;
                blk->field_0                   = blk->field_0 * blk->field_0;
                blk->field_4                   = blk->field_4 * blk->field_4;
                blk->field_8                   = blk->field_8 * blk->field_8;
                spad_b                         = (u32)PSX_SCRATCH + 0x3F8;
                *(void**)((u8*)spad_b + 0x4)   = head;
                outside                        = blk->field_0 + blk->field_4 >= blk->field_8;
                if (outside != 0) {
                    return;
                }
                VectorNormalSS(dir, dir);
                gte_lddp(0x2BC);
                gte_ldsv(dir);
                gte_gpf12_real();
                gte_stsv(dir);
                arg0->coord.t[0] = vec.vx + 0x3E8;
                arg0->coord.t[2] = vec.vz;
                arg0->flg        = 0;
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801335BC);

void func_actor_421600_80133B30(Actor421600* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    Actor421600AnimWork* work;
    s32                  blend;
    s32                  invBlend;
    s16                  index;
    s16                  next;

    index = 1;
    work  = (Actor421600AnimWork*)arg0->field_1C;
    do {
        switch (index) {
            case 1:
                blend = 0xC00;
                break;
            case 2:
                blend = 0x800;
                break;
            case 3:
            case 4:
            case 5:
                blend = 0x5DE;
                break;
            default:
                blend = 0xBD0;
                break;
        }
        invBlend = 0x1000 - blend;
        if (index < 0xB) {
            work->blendSlots[index].rate = (u8)work->field_83A;
            work->slots[index].rate      = (s8)(work->field_832 - 3);
            func_800B3448(&work->anim, index, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, index, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(&work->anim, index, &pose, &blendPose, blend, invBlend);
        } else {
            work->slots[index].rate = (s8)(work->field_832 - 3);
            Gp_AnimTickIndex(&work->anim, index);
        }
        next  = index + 1;
        index = next;
    } while (next < 0x12);
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80133CAC);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80134604);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80134AD4);

/// Picks one of twelve hit positions out of `D_actor_421600_801510B8` by
/// damage magnitude `arg1`, then spawns effect `Gp_GetIdParam1(arg2)` on the
/// model part that entry names. Same body as the shared
/// `Actor00100_Fn03340` / `func_actor_401300_80134BA4` pair, but it reads this
/// overlay's own table, so it stays a per-overlay copy.
void func_actor_421600_801350BC(Actor421600* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         sc;
    s32              mag;
    Actor421600Work* work;

    sc   = (SVECTOR*)(*(u32*)G_SCRATCH_HEAD -= 8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->field_1C;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_421600_801510B8[0];
                break;
            case 1:
                *sc = D_actor_421600_801510B8[1];
                break;
            case 2:
                *sc = D_actor_421600_801510B8[2];
                break;
            case 3:
                *sc = D_actor_421600_801510B8[3];
                break;
            default:
                *sc = D_actor_421600_801510B8[4];
                break;
        }
    } else if (mag >= 0x601) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_421600_801510B8[5];
                break;
            case 1:
                *sc = D_actor_421600_801510B8[6];
                break;
            default:
                *sc = D_actor_421600_801510B8[7];
                break;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_421600_801510B8[8];
        } else {
            *sc = D_actor_421600_801510B8[9];
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_421600_801510B8[10];
        } else {
            *sc = D_actor_421600_801510B8[11];
        }
    }
    work->field_890.coord      = &arg0->field_2C->coords[sc->pad];
    work->field_890.spawnArgLo = 0x100;
    work->field_890.spawnArgHi = 2;
    work->field_898            = *sc;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->field_2C->coords[sc->pad], &work->field_898, &work->field_890);
    *(u32*)G_SCRATCH_HEAD += 8;
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801354D8);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80135F6C);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80136138);

/// Rebuild `coord`'s Y rotation from its current yaw (`ratan2` of
/// `-m[2][0], m[2][2]`), scaled by `y` on Y and left at 1.0 on X and Z, through
/// a 0x34-byte block borrowed from the scratchpad. Marks the coordinate dirty.
/// Same body as `Actor401300_RescaleYaw` / `ActorsShared80135a60`, per-axis
/// instead of uniform, which is why `y` arrives already narrowed to `s16`.
static __inline__ void Actor421600_ShrinkCoord(GsCOORDINATE2* coord, s16 y)
{
    void*                     head;
    Actor421600ShrinkScratch* blk;
    s16                       ang;
    u16                       m22;

    head                    = *(void**)G_SCRATCH_HEAD;
    blk                     = (Actor421600ShrinkScratch*)((u8*)head - 0x34);
    *(void**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vx = 0x1000;
    blk->scale.vy = y;
    blk->scale.vz = 0x1000;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] =
        *(u16*)&((Actor421600ShrinkScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1]    = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]    = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]    = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]    = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]    = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]    = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]    = *(u16*)&blk->m.m[2][1];
    m22                     = *(u16*)&blk->m.m[2][2];
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x34;
    coord->flg              = 0;
    coord->coord.m[2][2]    = m22;
}

/// Shrink tick: on the live-actor edge it drops the model's dirty flag, clears
/// the 0x4000 bit on the 0xB6C node, marks the enemy's list node and resets
/// `field_6` / `field_8A0`. Then it counts frames in `field_6` and, from frame
/// 0xB on, scales the model's coordinate Y by `0x1000 - (frame - 0xA) * 0x6B`
/// until that factor runs out at 0, through `Actor421600_ShrinkCoord`. The
/// frame counter also drives the light state: 1 sets modes 0 and 1, 20 (and
/// the fall-through from 1) sets mode 2, 38 sets `field_C` 0x80 and the
/// `field_0` state 0x16. Counting stops at 0x401.
void func_actor_421600_801366F4(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    s32              t;
    u16              tick;

    work = arg0->field_1C;
    obj  = arg0->field_2C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj->flags             = 0;
        work->field_B6C.flags &= 0xBFFF;
        ctx->node.flags        = 1;
        work->field_6          = 0;
        work->field_8A0        = 0;
    }
    if ((s16)work->field_6 < 0x401) {
        tick          = work->field_6 + 1;
        work->field_6 = tick;
        switch ((s16)tick) {
            case 1:
                Gp_SetLightMode((GpObj4C*)ctx, 0);
                Gp_SetLightMode((GpObj4C*)ctx, 1);
                /* fallthrough */
            case 20:
                arg0->field_2C->flags = 2;
                Gp_SetLightMode((GpObj4C*)ctx, 2);
                break;
            case 22:
                break;
            case 38:
                arg0->field_2C->flags = 0x80;
                work->field_0         = 0x16;
                break;
        }
        if ((s16)work->field_6 >= 0xB) {
            t = ((s16)work->field_6 - 10) * 0x6B;
            if (t < 0x1000) {
                Actor421600_ShrinkCoord(arg0->field_2C->coords, 0x1000 - t);
            } else {
                Actor421600_ShrinkCoord(arg0->field_2C->coords, 0);
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801369A0);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80136C88);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801373D4);

/// Spawn/aim tick: on the live-actor edge it resets the display objects, runs
/// the arena vector through the gte rotation, stores the world X (`field_8A4`)
/// and Z (`field_8AC`) it produces, plays the spawn sound, then bumps the
/// `field_6` frame counter. On frame 0xF of the 7 pose it enqueues the second
/// sound and, outside a live-game state, spawns the effect on the player's
/// coordinate. The `field_E90` id picks `field_0` on the 0x100 flag, 5 for
/// 0x11402 and 0x1F otherwise -- the same pair `func_actor_421600_8013E9D8`
/// writes.
void func_actor_421600_8013848C(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    SVECTOR*         head;
    SVECTOR*         vec;
    SVECTOR*         gteVec;
    TmdObject*       obj;
    Task*            player;
    s32              x;
    s32              z;
    s32              sound;
    s32              pan;
    s32              eventPan;
    s32              state;
    u16              tick;

    work                       = arg0->field_1C;
    player                     = gameGetPtrSlot(3);
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    vec                        = head - 2;
    *(SVECTOR**)G_SCRATCH_HEAD = vec;
    ctx                        = arg0->field_20;
    gteVec                     = vec;
    if (work->field_4 != 0) {
        TOUCH_REG(gteVec);
        obj             = arg0->field_2C;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_82E        = 5;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->field_83E        = 0;
        work->field_B6C.flags |= 0x4000;
        work->field_832        = work->field_834;
        func_actor_421600_80134604(arg0);
        Gfx_MatrixCol2(&arg0->field_2C->coords->coord, vec);
        work->field_6 = 0;
        VectorNormalSS(vec, vec);
        if (work->field_E9E >= 0xFA1) {
            work->field_E9E = 0xFA0;
        }
        gte_lddp(0x85);
        gte_ldsv(gteVec);
        gte_gpf12_real();
        gte_stsv(gteVec);
        x               = head[-2].vx;
        work->field_8A8 = 0;
        work->field_8A4 = x;
        z               = vec->vz;
        work->field_8B4 = 7;
        work->field_8B6 = 1;
        work->field_8AC = z;
        pan             = (s8)Gp_GetObjPan(arg0->field_2C->coords);
        SndEvt_EnqueueType6(7, (s32)pan, (s32)(s8)Gp_GetObjDepth(arg0->field_2C->coords));
    }
    tick          = work->field_6 + 1;
    work->field_6 = tick;
    if (((s16)tick == 0xF) && (work->field_8B4 == 7)) {
        sound    = (((u16)ctx->placeKey >> 0xC) << 8) | 0x4001000A;
        eventPan = (s8)Gp_GetObjPan(arg0->field_2C->coords);
        SndEvt_EnqueueType6(sound, (s32)eventPan, (s32)(s8)Gp_GetObjDepth(arg0->field_2C->coords));
        if (Gp_State1C->roomEffectMode == 2) {
            Gp_SpawnEff(0x60054, ((TmdObject*)player->extra)->coords + 1, 0x80003A00, NULL);
        }
    }
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        state = work->field_E90.word & 0xFFFFFF;
        if (state == 0x11402) {
            state = 5;
        } else {
            state = 0x1F;
        }
        work->field_0 = state;
    }
    *(SVECTOR**)G_SCRATCH_HEAD += 2;
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80138750);

/// Scans the 12 0x18-byte records at 0xCE4 for one whose `field_4` carries the
/// 0x100000 kind, stopping at the first empty record. The 5-record twin of
/// this body is `Actor00100_HasRecord10`, which reads the same halves of
/// `field_0` instead; `func_actor_421600_80138D24` picks its aim scale with it.
static __inline__ s16 Actor421600_HasRecord10(Actor421600* arg0)
{
    Actor421600Work* work  = arg0->field_1C;
    s16              found = 0;
    s16              i;

    for (i = 0; i < 0xC; i++) {
        if (!work->field_CE4[i].key) {
            break;
        }
        if ((work->field_CE4[i].key & 0xFFFF0000) == 0x100000) {
            found = 1;
        }
    }
    return found;
}

/// Takes an `SVECTOR` off `G_SCRATCH_HEAD`, runs `coord` through
/// `Gfx_MatrixCol2` and `VectorNormalSS`, scales the resulting direction by
/// `amount` with the gte `gpf 12` (`-0x55` / `-0xC8` are the two aims
/// `func_actor_421600_80138D24` picks) and folds the scaled X/Y/Z back into
/// the coordinate's translation before handing the slot back. Same body and
/// same gte op as `Actor00100_MoveForward`, sized for one `SVECTOR` of scratch.
///
/// Keeping the alloc/use/free block whole matters: `G_SCRATCH_HEAD` is the bare
/// constant 0x1F8003FC, so an access written straight into a caller that touches
/// it twice CSEs the address into a callee-saved register, while the inliner
/// re-expands this body and each access keeps the `lw`/`sw` against the constant
/// that the assembler turns into the `lui`/`lw` (and `lui $at`/`sw`) pair the
/// ROM has.
static __inline__ void Actor421600_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12_real();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// Aim tick: on the live-actor edge it re-arms the model the way
/// `func_actor_421600_8013848C` does -- buffers reallocated, clip 0x10,
/// `field_82E` 6, the 0xB6C node's 0x4000 flag up -- with `field_8` and the
/// 0xCD8 offset it owns reseeded, then, while `field_6` is inside 9..0x18 and
/// `field_8` below 5, walks the 0xB8C table and counts a retry for every hit.
/// The 0xCE4 records decide which way the model is aimed: one carrying the
/// 0x100000 kind turns it by `-0x55`, none by `-0xC8`, through
/// `Actor421600_MoveForward`. Outside that frame window, and in both aim arms,
/// the 0xB8C walk is what runs.
void func_actor_421600_80138D24(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    s16              found;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        ctx             = arg0->field_20;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 6;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        work->field_6   = 0;
        work->field_8   = 0;
        work->field_CD8 = -0x320;
    }
    work->field_6++;
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 2;
    }
    if (((u32)(work->field_6 - 9) < 0x10) && ((s16)work->field_8 < 5)) {
        if (func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC) != 0) {
            work->field_8++;
        }
        found = Actor421600_HasRecord10(arg0);
        if (found != 0) {
            Actor421600_MoveForward(arg0->field_2C->coords, -0x55);
        } else {
            Actor421600_MoveForward(arg0->field_2C->coords, -0xC8);
        }
    } else {
        func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC);
    }
    arg0->field_2C->coords->flg = 0;
}

/// Re-arms the model the way `func_actor_421600_8013848C` does -- buffers
/// reallocated, clip 0x10, `field_82E` 2, the 0xB6C node's 0x4000 flag up --
/// then walks the 0xB8C `GpRec18` table through `func_actor_421600_8013285C`.
/// Takes two `SVECTOR`s off `G_SCRATCH_HEAD` and fills the XZ offset of the
/// model coordinate from `Player_Status.coordMtx` (the camera target matrix),
/// forms the yaw difference against the model's own facing (row 2 of its
/// matrix), wraps it into `[-0x800, 0x800]` into `field_840` and re-aims the
/// coordinate with `Gfx_RotMatrixY`. Ends by writing the view index into
/// `field_0` on the two view transitions.
///
/// The coordinate is read twice into two locals: `coord` only feeds the offset
/// and dies before the first `ratan2`, while `coord2` is live across it, so GCC
/// 2.8.1 keeps them in a caller-saved and a callee-saved register respectively.
/// One local assigned twice is one pseudo with one live range and costs a sixth
/// saved register.
void func_actor_421600_8013903C(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    SVECTOR*         head;
    SVECTOR*         vec;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   coord2;
    s16              angle;
    s32              view;

    head                        = *(SVECTOR**)G_SCRATCH_HEAD;
    *(SVECTOR**)G_SCRATCH_HEAD -= 2;
    vec                         = head - 2;
    work                        = arg0->field_1C;
    ctx                         = arg0->field_20;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 1;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 2;
        work->field_83E        = 0;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        work->field_6 = 0;
    }
    func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC);
    arg0->field_2C->coords->flg = 0;
    coord                       = arg0->field_2C->coords;
    head[-2].vx                 = (u16)Player_Status.coordMtx->t[0] - (u16)coord->coord.t[0];
    vec->vy                     = (u16)Player_Status.coordMtx->t[1] - (u16)coord->coord.t[1];
    vec->vz                     = (u16)Player_Status.coordMtx->t[2] - (u16)coord->coord.t[2];
    coord2                      = arg0->field_2C->coords;
    angle                       = ratan2(head[-2].vx, vec->vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    work->field_840 = angle;
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, (s16)ratan2(vec->vx, vec->vz), 1);
    arg0->field_2C->coords->flg = 0;
    func_actor_421600_80134604(arg0);
    if (((u16)ctx->placeKey >> 0xC) == 0) {
        view = Gp_GetViewIndex() & 0xFF;
        if (view == 3) {
            work->field_0 = view;
        }
    }
    if ((((u16)ctx->placeKey >> 0xC) == 1) && ((Gp_GetViewIndex() & 0xFF) == 8)) {
        work->field_0 = 3;
    }
    *(SVECTOR**)G_SCRATCH_HEAD += 2;
}

/// Re-arms the model buffers and the 0x828 motion block the way
/// `func_actor_421600_8013848C` does, with clip 0x10 and pose 7, then walks the
/// two `GpRec18` movement tables 0x90C and 0xA4C through
/// `func_actor_421600_80132310`. `field_0` becomes 0x22 when either walk
/// reports a hit, and again when the squared XZ offset from `D_80073B8C` is
/// under the squared 0x5DC radius, so the actor only takes the state while the
/// camera target is close. Ends by clearing the model's `flg`.
void func_actor_421600_801392A8(Actor421600* arg0)
{
    Actor421600Work*         work;
    GpEnemy*                 ctx;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    MATRIX*                  target;
    void*                    head;
    Actor421600ArenaScratch* blk;
    SVECTOR                  vec;
    SVECTOR*                 dir;
    u32                      spad_a;
    u32                      spad_b;
    s32                      outside;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        ctx             = arg0->field_20;
        obj             = arg0->field_2C;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_828        = 2;
        work->field_832        = 0x10;
        work->field_82A        = 0;
        work->field_82E        = 7;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
    }
    func_actor_421600_80134604(arg0);
    if (((func_actor_421600_80132310(arg0->field_2C->coords, &work->field_90C, 0xC, &vec) << 0x10) != 0) || ((func_actor_421600_80132310(arg0->field_2C->coords, &work->field_A4C, 0xC, &vec) << 0x10) != 0)) {
        work->field_0 = 0x22;
    }
    target                         = D_80073B8C;
    coord                          = arg0->field_2C->coords;
    vec.vx                         = (u16)target->t[0] - (u16)coord->coord.t[0];
    dir                            = &vec;
    dir->vy                        = (u16)target->t[1] - (u16)coord->coord.t[1];
    dir->vz                        = (u16)target->t[2] - (u16)coord->coord.t[2];
    head                           = *(void**)G_SCRATCH_HEAD;
    blk                            = (Actor421600ArenaScratch*)((u8*)head - 0xC);
    spad_a                         = (u32)PSX_SCRATCH;
    *(void**)((u8*)spad_a + 0x3FC) = blk;
    blk->field_0                   = vec.vx;
    blk->field_4                   = dir->vz;
    blk->field_8                   = 0x5DC;
    blk->field_0                   = blk->field_0 * blk->field_0;
    blk->field_4                   = blk->field_4 * blk->field_4;
    blk->field_8                   = blk->field_8 * blk->field_8;
    spad_b                         = (u32)PSX_SCRATCH + 0x3F8;
    *(void**)((u8*)spad_b + 0x4)   = head;
    outside                        = blk->field_0 + blk->field_4 >= blk->field_8;
    if (outside == 0) {
        work->field_0 = 0x22;
    }
    arg0->field_2C->coords->flg = 0;
}

/// Death / respawn tick: re-arms the model buffers and the 0x828 motion block,
/// fires the 0x40010009 spawn sound and the 0x40010007 tick sound (draining
/// `field_40` by 0xF and flooring it at 1), then walks the two `GpRec18`
/// movement tables. While the id word's third byte reads 2 the actor is held
/// in the arena by clamping X -- and Z only when X was already inside -- and
/// otherwise `func_actor_421600_80133334` drags it back. Picks the state
/// `field_0` out of `field_40` / `field_4C`.
void func_actor_421600_8013947C(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    s32              sound;
    s32              pan;
    s32              eventSound;
    s32              eventPan;
    s32              x;
    s32              z;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        ctx->node.flags = 0;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.radius = 0x19C;
        work->field_832        = 0x10;
        work->field_82E        = 0xA;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->field_B6C.flags |= 0x4000;
        work->field_8EC.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        sound = (((u16)ctx->placeKey >> 0xC) << 8) | 0x40010009;
        pan   = (s8)Gp_GetObjPan(arg0->field_2C->coords);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)Gp_GetObjDepth(arg0->field_2C->coords));
        ctx->hp -= 0xF;
        func_800DA6E8(&ctx->node, 0xF, 0);
        if (ctx->hp <= 0) {
            ctx->hp = 1;
        }
        eventSound = (((u16)ctx->placeKey >> 0xC) << 8) | 0x40010007;
        eventPan   = (s8)Gp_GetObjPan(arg0->field_2C->coords);
        SndEvt_EnqueueType6(eventSound, eventPan,
                            (s32)(s8)Gp_GetObjDepth(arg0->field_2C->coords));
    }
    func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_B8C, 0xC);
    func_actor_421600_8013285C(arg0->field_2C->coords, &work->field_90C, 0xC);
    if (work->field_E90.bytes[2] == 2) {
        coord = arg0->field_2C->coords;
        x     = coord->coord.t[0];
        if (x > 0) {
            if (x >= 0xBEB) {
                coord->coord.t[0] = 0xB54;
            } else {
                goto block_10;
            }
        } else if (x < -0xB22) {
            coord->coord.t[0] = -0xA8C;
        } else {
        block_10:
            z = coord->coord.t[2];
            if (z > 0) {
                if (z >= 0xB23) {
                    coord->coord.t[2] = 0xA8C;
                }
            } else if (z < -0xB22) {
                coord->coord.t[2] = -0xA8C;
            }
        }
    } else {
        func_actor_421600_80133334(arg0->field_2C->coords);
    }
    arg0->field_2C->coords->flg = 0;
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        if (ctx->hp > 0) {
            if (ctx->reactionFlags & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x11;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80139718);

void func_actor_421600_8013A404(Actor421600* arg0)
{
    Actor421600Work* temp_s0;
    GsCOORDINATE2*   temp_v0_2;
    s32              temp_a0;
    s32              temp_a1;
    s32              var_a0;
    s32              var_v1;
    u32              temp_v0;
    u8               temp_v1;

    temp_s0 = arg0->field_1C;
    if (temp_s0->field_4 != 0) {
        temp_v0          = (Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = temp_v0;
        temp_s0->field_6 = temp_s0->field_EA4 + ((temp_v0 >> 0x10) & 0xF);
    }
    temp_s0->field_6 -= 1;
    func_actor_421600_80134604(arg0);
    if ((s16)temp_s0->field_6 < 0) {
        temp_v1 = temp_s0->field_E90.bytes[2];
        if ((temp_v1 == 1) || (temp_v1 == 3)) {
            temp_s0->field_0 = 5;
        } else if (temp_v1 == 2) {
            temp_v0_2 = arg0->field_2C->coords;
            temp_a0   = temp_v0_2->coord.t[0];
            temp_a1   = temp_v0_2->coord.t[2];
            if (temp_a0 >= 0xD49) {
                var_a0 = 3;
            } else if (temp_a0 > 0) {
                var_a0 = 2;
            } else {
                var_a0 = temp_a0 >= -0xC7F;
            }
            var_v1 = 0;
            if (temp_a1 < 0xBB9) {
                var_v1 = 1;
                if (temp_a1 <= 0) {
                    var_v1 = 3;
                    if (temp_a1 >= -0xBB7) {
                        var_v1 = 2;
                    }
                }
            }
            if (D_actor_421600_801511C0[var_a0 | (var_v1 * 4)] >= 0xB) {
                temp_s0->field_0 = 0x24;
            } else {
                temp_s0->field_0 = 6;
            }
        } else {
            temp_s0->field_0 = 0x24;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013A554);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013B00C);

/// Zone-aim tick: the live-actor edge re-arms the model the way
/// `func_actor_421600_80138D24` does -- buffers reallocated, clip 0x10, pose 3,
/// motion 1, the 0xB6C node's 0x4000 flag up.
///
/// Otherwise the X and Z of the actor's coordinate are bucketed into the 4x4
/// zone table `D_actor_421600_801511C0` exactly as `func_actor_421600_8013A404`
/// does, and zone 5 abandons the tick into state 7. Any other zone picks the
/// neighbouring entry of the 8-byte pose table `D_actor_421600_80151158` --
/// `zone - 1` above the table's midpoint `mode`, `zone + 1` at or below it --
/// and copies all three halfwords into a 0xC block taken off `G_SCRATCH_HEAD`,
/// which becomes the XZ direction from the actor to that pose.
///
/// `mode` and the `(s8)` casts on `zone` are load-bearing, and so is the
/// `blk->vec.vy = 0` between the two coordinate subtractions. A plain `5`
/// literal lets expand fold `zone > 5` into `zone < 6`, which drops the two
/// register copies and the `slt` the ROM has; keeping the limit in a
/// declaration-initialised `s8` leaves it a register operand so the fold never
/// runs. The midpoint store then lands in the load-delay slot the subtractions
/// leave open.
void func_actor_421600_8013B4C4(Actor421600* arg0)
{
    Actor421600Work*        work;
    Actor421600SeekScratch* head;
    Actor421600SeekScratch* blk;
    GpEnemy*                ctx;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          coord2;
    GsCOORDINATE2*          coord3;
    GsCOORDINATE2*          coord4;
    s32                     zone;
    s8                      mode = 5;
    s32                     v;
    s32                     x;
    s32                     z;
    s16                     angle;
    s32                     wrapped;
    s32                     var_a0;
    s32                     var_v1;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        ctx             = arg0->field_20;
        ctx->node.flags = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_832        = 0x10;
        work->field_82E        = 3;
        work->field_828        = 1;
        work->field_B6C.flags |= 0x4000;
        func_actor_421600_80134604(arg0);
        return;
    }
    coord = arg0->field_2C->coords;
    x     = coord->coord.t[0];
    z     = coord->coord.t[2];
    if (x >= 0xD49) {
        var_a0 = 3;
    } else if (x > 0) {
        var_a0 = 2;
    } else {
        var_a0 = x >= -0xC7F;
    }
    var_v1 = 0;
    if (z < 0xBB9) {
        var_v1 = 1;
        if (z <= 0) {
            var_v1 = 3;
            if (z >= -0xBB7) {
                var_v1 = 2;
            }
        }
    }
    zone = D_actor_421600_801511C0[var_a0 | (var_v1 * 4)];
    if ((s8)zone == mode) {
        work->field_0 = 7;
        return;
    }
    head                                       = *(Actor421600SeekScratch**)G_SCRATCH_HEAD;
    *(Actor421600SeekScratch**)G_SCRATCH_HEAD -= 1;
    blk                                        = head - 1;
    if ((s8)zone > mode) {
        head[-1].vec.vx = D_actor_421600_80151158[zone - 1].vx;
        blk->vec.vy     = D_actor_421600_80151158[zone - 1].vy;
        blk->vec.vz     = D_actor_421600_80151158[zone - 1].vz;
    } else {
        head[-1].vec.vx = D_actor_421600_80151158[zone + 1].vx;
        blk->vec.vy     = D_actor_421600_80151158[zone + 1].vy;
        blk->vec.vz     = D_actor_421600_80151158[zone + 1].vz;
    }
    blk->vec.vx = blk->vec.vx - (u16)arg0->field_2C->coords->coord.t[0];
    blk->vec.vy = 0;
    blk->vec.vz = blk->vec.vz - (u16)arg0->field_2C->coords->coord.t[2];
    func_actor_421600_80134604(arg0);
    coord2 = arg0->field_2C->coords;
    angle  = ratan2(blk->vec.vx, blk->vec.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    wrapped         = angle;
    blk->angle      = wrapped;
    work->field_840 = wrapped;
    if (blk->angle >= 0x81) {
        blk->angle = 0x80;
    }
    if (blk->angle < -0x80) {
        blk->angle = -0x80;
    }
    work->field_83E = blk->angle;
    coord3          = arg0->field_2C->coords;
    blk->angle      = blk->angle + ratan2(-coord3->coord.m[2][0], coord3->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, blk->angle, 1);
    if (work->field_82A == 0) {
        coord4 = arg0->field_2C->coords;
        Actor421600_MoveForward(coord4, 0xC8);
    }
    func_actor_421600_80132310(arg0->field_2C->coords, &work->field_90C, 0xC, &blk->vec);
    *(Actor421600SeekScratch**)G_SCRATCH_HEAD += 1;
    arg0->field_2C->coords->flg                = 0;
}

void func_actor_421600_8013B8E0(Actor421600* arg0)
{
    Actor421600Work* temp_s1;
    TmdObject*       temp_a0;

    temp_s1 = arg0->field_1C;
    if (temp_s1->field_4 != 0) {
        temp_a0                    = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        temp_a0->flags             = 0;
        Tmd_AllocBuffers(temp_a0);
        temp_s1->field_832                 = 0x10;
        temp_s1->field_82E                 = 0x11;
        temp_s1->field_828                 = 2;
        temp_s1->field_B6C.flags          |= 0x4000;
        arg0->field_2C->coords->coord.t[0] = 0;
        arg0->field_2C->coords->coord.t[1] = 0;
        arg0->field_2C->coords->coord.t[2] = 0;
        arg0->field_2C->coords->flg        = 0;
        Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0, 1);
        func_actor_421600_80134604(arg0);
    }
    func_actor_421600_80134604(arg0);
    if (temp_s1->field_68 & 0x100) {
        arg0->field_2C->coords->coord.t[0] = -0x334;
        arg0->field_2C->coords->coord.t[1] = 0;
        arg0->field_2C->coords->coord.t[2] = -0x4C4;
        arg0->field_2C->coords->flg        = 0;
        Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0x400, 1);
        temp_s1->field_828 = 2;
        temp_s1->field_82E = 0;
        func_actor_421600_80134604(arg0);
        func_actor_421600_80134604(arg0);
        temp_s1->field_0 = 0x27;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013BA70);

/// Death tick: the live-actor edge arms the model (dirty 0x80, clip 0x19C, the
/// 0xB6C node's 0x4000 flag down, the enemy's list node marked, the 0x83E /
/// 0x840 / 0x844 triple and `field_6` cleared) and spawns the 0x60030 effect on
/// the second coordinate. Frames 2, 3, 5, 7 and 8 then free the model buffers
/// and spawn one effect each -- 0xA0005 on coordinate 9, 12, 1 and 3 -- whose
/// model is tinted from the enemy's area record (`field_24` / `field_25`) and
/// re-streamed. Frame 0xA writes the 0x16 state. The counter stops at 0x400.
void func_actor_421600_8013C8E0(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    SVECTOR          vec;
    GpAreaKey        key;
    GpAreaKey*       keyPtr;
    GpAreaKey*       sessionKey1;
    GpAreaKey*       sessionKey2;
    GpAreaKey*       sessionKey3;
    GpAreaKey*       sessionKey4;
    u32              raw1;
    u32              raw2;
    u32              raw3;
    u32              raw4;
    u32              index1;
    u32              index2;
    u32              index3;
    u32              index4;
    GpEffWork*       effect1;
    GpEffWork*       effect2;
    GpEffWork*       effect3;
    GpEffWork*       effect4;
    TmdObject*       model1;
    TmdObject*       model2;
    TmdObject*       model3;
    TmdObject*       model4;
    GpAreaRec*       rec1;
    GpAreaRec*       rec2;
    GpAreaRec*       rec3;
    GpAreaRec*       rec4;
    GpAreaPlace*     entry1;
    GpAreaPlace*     entry2;
    GpAreaPlace*     entry3;
    GpAreaPlace*     entry4;
    u8               areaByte0;
    u16              tick;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    obj  = arg0->field_2C;
    if (work->field_4 != 0) {
        obj->flags             = 0x80;
        work->field_8EC.radius = 0x19C;
        work->field_B6C.flags  = (u16)(work->field_B6C.flags & 0xBFFF);
        ctx->node.flags        = 1;
        work->field_844        = 0;
        work->field_840        = 0;
        work->field_83E        = 0;
        work->field_6          = 0;
        vec.vx                 = 0x64;
        vec.vz                 = 0;
        vec.vy                 = 0;
        Gp_SpawnEff(0x60030, arg0->field_2C->coords + 1, 0x10300, &vec);
    }
    if ((s16)work->field_6 == 2) {
        obj->flags |= 4;
        Tmd_FreeBuffers(obj);
    }
    if ((s16)work->field_6 == 3) {
        D_80114B78[0] = &D_actor_421600_80143EF4;
        vec.vz        = 0x64;
        vec.vy        = 0;
        vec.vx        = 0;
        effect1       = Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 9, 0x200, &vec);
        if (effect1 != NULL) {
            sessionKey1 = (GpAreaKey*)&gGameSession->at4.loc;
            raw1        = ctx->placeKey;
            model1      = (TmdObject*)effect1->task->extra;
            key.stage   = sessionKey1->stage;
            key.area    = sessionKey1->area;
            key.room    = sessionKey1->room;
            areaByte0   = gGameSession->at4.loc.view;
            index1      = raw1 >> 12;
            SOFT_BARRIER();
            keyPtr = &key;
            TOUCH_REG(keyPtr);
            key.view = areaByte0;
            Gp_SyncAreaKeyIndex(keyPtr);
            rec1          = Gp_GetNestedAreaRec(&key);
            entry1        = (GpAreaPlace*)((index1 * 0x10) + (s32)rec1->field_0);
            model1->tpage = entry1->tpage;
            model1->clut  = entry1->clut;
            if (model1->buffer != NULL) {
                tmdProcessStream(model1);
                tmdProcessStream(model1);
            }
        }
    }
    if ((s16)work->field_6 == 5) {
        D_80114B78[0] = &D_actor_421600_801443C8;
        vec.vy        = 0;
        vec.vx        = 0;
        effect2       = Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 12, 0x200, &vec);
        if (effect2 != NULL) {
            sessionKey2 = (GpAreaKey*)&gGameSession->at4.loc;
            raw2        = ctx->placeKey;
            model2      = (TmdObject*)effect2->task->extra;
            key.stage   = sessionKey2->stage;
            key.area    = sessionKey2->area;
            key.room    = sessionKey2->room;
            areaByte0   = gGameSession->at4.loc.view;
            index2      = raw2 >> 12;
            SOFT_BARRIER();
            keyPtr = &key;
            TOUCH_REG(keyPtr);
            key.view = areaByte0;
            Gp_SyncAreaKeyIndex(keyPtr);
            rec2          = Gp_GetNestedAreaRec(&key);
            entry2        = (GpAreaPlace*)((index2 * 0x10) + (s32)rec2->field_0);
            model2->tpage = entry2->tpage;
            model2->clut  = entry2->clut;
            if (model2->buffer != NULL) {
                tmdProcessStream(model2);
                tmdProcessStream(model2);
            }
        }
    }
    if ((s16)work->field_6 == 7) {
        D_80114B78[0] = &D_actor_421600_80145604;
        effect3       = Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 1, 0x200, NULL);
        if (effect3 != NULL) {
            sessionKey3 = (GpAreaKey*)&gGameSession->at4.loc;
            raw3        = ctx->placeKey;
            model3      = (TmdObject*)effect3->task->extra;
            key.stage   = sessionKey3->stage;
            key.area    = sessionKey3->area;
            key.room    = sessionKey3->room;
            areaByte0   = gGameSession->at4.loc.view;
            index3      = raw3 >> 12;
            SOFT_BARRIER();
            keyPtr = &key;
            TOUCH_REG(keyPtr);
            key.view = areaByte0;
            Gp_SyncAreaKeyIndex(keyPtr);
            rec3          = Gp_GetNestedAreaRec(&key);
            entry3        = (GpAreaPlace*)((index3 * 0x10) + (s32)rec3->field_0);
            model3->tpage = entry3->tpage;
            model3->clut  = entry3->clut;
            if (model3->buffer != NULL) {
                tmdProcessStream(model3);
                tmdProcessStream(model3);
            }
        }
    }
    if ((s16)work->field_6 == 8) {
        D_80114B78[0] = &D_actor_421600_80145124;
        effect4       = Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 3, 0x200, NULL);
        if (effect4 != NULL) {
            sessionKey4 = (GpAreaKey*)&gGameSession->at4.loc;
            raw4        = ctx->placeKey;
            model4      = (TmdObject*)effect4->task->extra;
            key.stage   = sessionKey4->stage;
            key.area    = sessionKey4->area;
            key.room    = sessionKey4->room;
            areaByte0   = gGameSession->at4.loc.view;
            index4      = raw4 >> 12;
            SOFT_BARRIER();
            keyPtr = &key;
            TOUCH_REG(keyPtr);
            key.view = areaByte0;
            Gp_SyncAreaKeyIndex(keyPtr);
            rec4          = Gp_GetNestedAreaRec(&key);
            entry4        = (GpAreaPlace*)((index4 * 0x10) + (s32)rec4->field_0);
            model4->tpage = entry4->tpage;
            model4->clut  = entry4->clut;
            if (model4->buffer != NULL) {
                tmdProcessStream(model4);
                tmdProcessStream(model4);
            }
        }
    }
    if ((s16)work->field_6 == 0xA) {
        work->field_0 = 0x16;
    }
    if ((s16)work->field_6 < 0x400) {
        tick          = work->field_6 + 1;
        work->field_6 = tick;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013CD3C);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013D1DC);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013D658);

void func_actor_421600_8013E424(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_421600/actor_421600", ActorsShared80135df4Table);
