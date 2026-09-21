#include "common.h"

#include "actors/actor_104900.h"
#include "actors/actors_shared_801384ac.h"
#include "actors/actors_shared_801388e8.h"
#include "actors/actors_shared_80138efc.h"
#include "actors/actors_shared_801511c8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"

#include <psyq/inline_c.h>
#include <psyq/rand.h>

extern u32 Gp_LcgState;
/// Nonzero suppresses the step along the model's facing. This handler tests
/// it against zero.
extern u8        D_80072729;
extern u8        D_actor_104900_80147480[];
extern GpU16Pair D_actor_104900_801392F0[];
extern TaskDesc  D_actor_104900_80147400[];

/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix with no translation
/// vector added. The `inline_c.h` macro of that name assembles to a different
/// word, so spell the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
/// `gpf 1`: scale IR1..3 by IR0. Same reason as above for spelling out the word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_actor_104900_80137498(GpEnemy*, Task*, ActorsShared80138efcWork*, void*);
s32  func_actor_104900_80132D78(GpEnemy*, Task*, ActorsShared80138efcWork*, void*);
void ActorsShared801357f0(GpEnemy*, Task*, ActorsShared80138efcWork*, ActorsShared80138efcArg*);

/// First-frame distance handler: while the latch at 0xBA8 is still clear it
/// sets motion 6, zeroes the countdown at 0xB8C and steps the latch. If actor
/// slot 3 is live it rotates `(0x12C, 0, 0)` through model part 10's `workm`,
/// adds the player-part-1 versus part-10 translation, and maps
/// `SquareRoot0(Gfx_ApplyMatrixNoSf)` into `field_B9E` — 0 inside 0x384, 0x2000
/// past 0xA8C, otherwise `((dist - 0x384) << 9) / 100`.
///
/// Every later frame increments the countdown, asks `ActorsShared801357f0` for
/// the yaw at 0xB90 and turns the model's `field_46` toward it by at most 0x10,
/// then rebuilds the Y rotation. Frame 0x16 packs pair 3 into the first motion
/// node and ORs the 0xC000 bits; frame 0x20 posts `0x400B0008`. While the
/// countdown sits in `[0x17, 0x2B]` and the latch is still 1, a high-bit hit on
/// the recs at 0xA70 steps the latch to 3. `field_B98` / `field_B94` ramp with
/// the countdown, the frame block's scratch byte at 0x64 takes 0xC, and frame
/// 0x2C masks those bits back out of both motion nodes. The trigger at 0xBA9
/// writes rate 0x10 onto slots `[1, 0x14]` of both animation runs and then
/// either stages state 0xE, or, while the latch is 3, a 1-in-4 draw of that
/// state versus restarting the motion through `field_BA5`.
///
/// The stack copy of the offset is what the first `lwc2` pair reads, and it is
/// written with the sibling bodies' raw asm: `gte_ldv0` of a stack local leaves
/// its `addiu` free for sched2 to hoist. `vec` is assigned only inside the
/// player-present arm so it is a local quantity and keeps `$a0` for
/// `Gfx_ApplyMatrixNoSf`.
///
/// Same body as the four twins - `func_actor_101100_80136230` at the same
/// address, `func_actor_201100_8014E230` / `func_actor_204900_8014E230` 0x18000
/// past it and `func_actor_301100_80166230` 0x30000 past. A shared span here
/// would sit inside `_3`, ahead of the rest of this unit, and insert a new
/// overlay-local run that renames `_4`..`_7`.
void func_actor_104900_80136230(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    SVECTOR        local;
    SVECTOR*       vec;
    GsCOORDINATE2* actorCoords;
    GsCOORDINATE2* playerCoords;
    GsCOORDINATE2* actorPart;
    GsCOORDINATE2* playerPart;
    GpCoordPose*   pose;
    GpObj*         obj;
    GpAnimSlot*    slotA;
    GpAnimSlot*    slotB;
    Task*          player;
    s32            dist;
    s32            i;
    s32            off;
    s32            off2;
    s32            yaw;
    s32            rate;
    u16            angle;
    u16            time;
    u16            reach;
    u32            rng;

    if (work->field_BA8 == 0) {
        work->field_BA4 = 6;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
        player          = gameGetPtrSlot(3);
        if (player == NULL) {
            work->field_B9E = 0;
        } else {
            vec                      = (SVECTOR*)&arg->pad_0[0x10];
            actorCoords              = ((TmdObject*)task->extra)->coords;
            playerCoords             = ((TmdObject*)player->extra)->coords;
            *(s16*)&arg->pad_0[0x10] = 0x12C;
            *(s16*)&arg->pad_0[0x12] = 0;
            *(s16*)&arg->pad_0[0x14] = 0;
            local                    = *(SVECTOR*)&arg->pad_0[0x10];
            actorPart                = &actorCoords[10];
            playerPart               = &playerCoords[1];
            gte_SetRotMatrix(&actorPart->workm);
            __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
            gte_rtv0_real();
            gte_stsv(vec);
            *(s16*)&arg->pad_0[0x10] += (u16)playerPart->workm.t[0] - (u16)actorPart->workm.t[0];
            *(s16*)&arg->pad_0[0x12] += (u16)playerPart->workm.t[1] - (u16)actorPart->workm.t[1];
            *(s16*)&arg->pad_0[0x14] += (u16)playerPart->workm.t[2] - (u16)actorPart->workm.t[2];
            dist                      = SquareRoot0(Gfx_ApplyMatrixNoSf(vec, vec));
            if (dist < 0x384) {
                work->field_B9E = 0;
            } else if (dist >= 0xA8D) {
                work->field_B9E = 0x2000;
            } else {
                work->field_B9E = ((dist - 0x384) << 9) / 100;
            }
        }
    }
    work->field_B8C = (u16)work->field_B8C + 1;
    ActorsShared801357f0(enemy, task, work, arg);
    yaw  = work->field_B90;
    pose = (GpCoordPose*)((TmdObject*)task->extra)->coords;
    if (yaw >= 0x11) {
        pose->field_46 = (u16)pose->field_46 + 0x10;
    } else if (yaw < -0x10) {
        pose->field_46 = (u16)pose->field_46 - 0x10;
    } else {
        pose->field_46 = (u16)pose->field_46 + yaw;
    }
    angle          = (u16)pose->field_46 & 0xFFF;
    pose->field_46 = angle;
    Gfx_RotMatrixY(&pose->coord, angle, 1);
    pose->flg = 0;
    if (work->field_B8C == 0x16) {
        obj         = &work->motion.objs[0];
        obj->key    = Gp_PackObjPair((GpObj50*)enemy, 3);
        obj->flags |= 0xC000;
    } else if (work->field_B8C == 0x20) {
        SndEvt_EnqueueType6((work->field_BB8 << 22) | ((work->field_B88 << 8) | 0x400B0008), arg->pan, arg->depth);
    }
    if (((u32)((u16)work->field_B8C - 0x17) < 0x15U) && (work->field_BA8 == 1) &&
        (Gp_CountRec18Hi((GpRec18*)((u8*)work + 0xA70), 0x10000) != 0)) {
        work->field_BA8 = 3;
    }
    time = work->field_B8C;
    if ((u32)(time - 1) < 0x1DU) {
        if (work->field_B98 < 0x800) {
            work->field_B98 = (s16)((u16)work->field_B98 + 0x40);
        }
    } else if ((s16)time >= 0x1E) {
        if (work->field_B98 >= 0x100) {
            work->field_B98 = (s16)((u16)work->field_B98 - 0x100);
        } else {
            work->field_B98 = 0;
        }
    }
    time = work->field_B8C;
    if ((u32)(time - 0x1E) < 0xEU) {
        reach = work->field_B9E;
        if (work->field_B94 < ((s32)(reach << 0x10) >> 0x10)) {
            work->field_B94 = (s16)((u16)work->field_B94 + ((s32)(reach << 0x10) >> 0x13));
        }
    } else if ((s16)time >= 0x2C) {
        if (work->field_B94 >= 0x200) {
            work->field_B94 = (s16)((u16)work->field_B94 - 0x200);
        } else {
            work->field_B94 = 0;
        }
    }
    arg->field_64 = 0xC;
    if (work->field_B8C == 0x2C) {
        i   = 0;
        off = 0x9C8;
        do {
            ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
            off                                += 0x20;
            i++;
        } while (i < 2);
    }
    if (work->field_BA9 != 0) {
        rate = 0x10;
        i    = 1;
        off  = 0x538;
        off2 = 0x8C;
        do {
            slotA       = (GpAnimSlot*)((u8*)work + off2);
            slotA->rate = rate;
            SOFT_BARRIER();
            slotB = (GpAnimSlot*)((u8*)work + off);
            off  += 0x28;
            i++;
            slotB->rate = rate;
            off2       += 0x28;
        } while (i < 0x15);
        if (work->field_BA8 != 3) {
            work->state     = 0xE;
            work->field_BA8 = 0;
            work->field_BAA = (u8)work->field_BAA + 1;
            return;
        }
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        if (!((rng >> 0x10) & 3)) {
            work->state = 0xE;
        } else {
            work->field_BA5 = 1;
        }
        work->field_BA8 = 0;
    }
}

/// First-frame distance handler: while the latch at 0xBA8 is still clear it
/// sets motion 7, zeroes the countdown at 0xB8C and steps the latch. If actor
/// slot 3 is live it rotates `(0x12C, 0, 0)` through model part 6's `workm`,
/// adds the player-part-1 versus part-6 translation, and maps
/// `SquareRoot0(Gfx_ApplyMatrixNoSf)` into `field_B9E` — 0 inside 0x384, 0x2000
/// past 0xA8C, otherwise `((dist - 0x384) << 9) / 100`.
///
/// Every frame then asks `ActorsShared801357f0` for the yaw at 0xB90 and turns
/// the model's `field_46` toward it by at most 0x10, rebuilds the Y rotation,
/// increments the countdown and asks again. Frame 0x23 packs pair 4 into the
/// second motion node and ORs the 0xC000 bits; frame 0x2D posts `0x400B0008`.
/// While the countdown sits in `[0x24, 0x3B]` and the latch is still 1, a
/// high-bit hit on the recs at 0xAB8 steps the latch to 3. `field_B9A` /
/// `field_B96` ramp with the countdown, the frame block's scratch byte at 0x64
/// takes 8 (with a same-value write on frame 0x2F), and frame 0x3C masks those
/// bits back out of both motion nodes. The trigger at 0xBA9 writes rate 0x10
/// onto slots `[1, 0x14]` of both animation runs, zeroes `field_B96`, and then
/// either stages state 0xE, or, while the latch is 3, a 1-in-4 draw of that
/// state versus restarting the motion through `field_BA5`.
///
/// The stack copy of the offset is what the first `lwc2` pair reads, and it is
/// written with the sibling bodies' raw asm: `gte_ldv0` of a stack local leaves
/// its `addiu` free for sched2 to hoist. `vec` is assigned only inside the
/// player-present arm so it is a local quantity and keeps `$a0` for
/// `Gfx_ApplyMatrixNoSf`.
///
/// Same body as the four twins - `func_actor_101100_801366E8` at the same
/// address, `func_actor_201100_8014E6E8` / `func_actor_204900_8014E6E8` 0x18000
/// past it and `func_actor_301100_801666E8` 0x30000 past. A shared span here
/// would sit inside `_3`, ahead of the rest of this unit, and insert a new
/// overlay-local run that renames `_4`..`_7`.
void func_actor_104900_801366E8(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    SVECTOR        local;
    SVECTOR*       vec;
    GsCOORDINATE2* actorCoords;
    GsCOORDINATE2* playerCoords;
    GsCOORDINATE2* actorPart;
    GsCOORDINATE2* playerPart;
    GpCoordPose*   pose;
    GpObj*         obj;
    GpAnimSlot*    slotA;
    GpAnimSlot*    slotB;
    Task*          player;
    s32            dist;
    s32            i;
    s32            off;
    s32            off2;
    s32            yaw;
    s32            rate;
    u16            angle;
    u16            time;
    u16            reach;
    u32            rng;

    if (work->field_BA8 == 0) {
        work->field_BA4 = 7;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
        player          = gameGetPtrSlot(3);
        if (player == NULL) {
            work->field_B9E = 0;
        } else {
            vec                      = (SVECTOR*)&arg->pad_0[0x10];
            actorCoords              = ((TmdObject*)task->extra)->coords;
            playerCoords             = ((TmdObject*)player->extra)->coords;
            *(s16*)&arg->pad_0[0x10] = 0x12C;
            *(s16*)&arg->pad_0[0x12] = 0;
            *(s16*)&arg->pad_0[0x14] = 0;
            local                    = *(SVECTOR*)&arg->pad_0[0x10];
            actorPart                = &actorCoords[6];
            playerPart               = &playerCoords[1];
            gte_SetRotMatrix(&actorPart->workm);
            __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
            gte_rtv0_real();
            gte_stsv(vec);
            *(s16*)&arg->pad_0[0x10] += (u16)playerPart->workm.t[0] - (u16)actorPart->workm.t[0];
            *(s16*)&arg->pad_0[0x12] += (u16)playerPart->workm.t[1] - (u16)actorPart->workm.t[1];
            *(s16*)&arg->pad_0[0x14] += (u16)playerPart->workm.t[2] - (u16)actorPart->workm.t[2];
            dist                      = SquareRoot0(Gfx_ApplyMatrixNoSf(vec, vec));
            if (dist < 0x384) {
                work->field_B9E = 0;
            } else if (dist >= 0xA8D) {
                work->field_B9E = 0x2000;
            } else {
                work->field_B9E = ((dist - 0x384) << 9) / 100;
            }
        }
    }
    ActorsShared801357f0(enemy, task, work, arg);
    yaw  = work->field_B90;
    pose = (GpCoordPose*)((TmdObject*)task->extra)->coords;
    if (yaw >= 0x11) {
        pose->field_46 = (u16)pose->field_46 + 0x10;
    } else if (yaw < -0x10) {
        pose->field_46 = (u16)pose->field_46 - 0x10;
    } else {
        pose->field_46 = (u16)pose->field_46 + yaw;
    }
    angle          = (u16)pose->field_46 & 0xFFF;
    pose->field_46 = angle;
    Gfx_RotMatrixY(&pose->coord, angle, 1);
    pose->flg       = 0;
    work->field_B8C = (u16)work->field_B8C + 1;
    ActorsShared801357f0(enemy, task, work, arg);
    if (work->field_B8C == 0x23) {
        obj         = &work->motion.objs[1];
        obj->key    = Gp_PackObjPair((GpObj50*)enemy, 4);
        obj->flags |= 0xC000;
    } else if (work->field_B8C == 0x2D) {
        SndEvt_EnqueueType6((work->field_BB8 << 22) | ((work->field_B88 << 8) | 0x400B0008), arg->pan, arg->depth);
    }
    if (((u32)((u16)work->field_B8C - 0x24) < 0x18U) && (work->field_BA8 == 1) &&
        (Gp_CountRec18Hi((GpRec18*)((u8*)work + 0xAB8), 0x10000) != 0)) {
        work->field_BA8 = 3;
    }
    time = work->field_B8C;
    if ((u32)(time - 1) < 0x28U) {
        if (work->field_B9A < 0x800) {
            work->field_B9A = (s16)((u16)work->field_B9A + 0x40);
        }
    } else if ((s16)time >= 0x29) {
        if (work->field_B9A >= 0x100) {
            work->field_B9A = (s16)((u16)work->field_B9A - 0x100);
        } else {
            work->field_B9A = 0;
        }
    }
    time = work->field_B8C;
    if ((u32)(time - 0x29) < 0x13U) {
        reach = work->field_B9E;
        if (work->field_B96 < ((s32)(reach << 0x10) >> 0x10)) {
            work->field_B96 = (s16)((u16)work->field_B96 + ((s32)(reach << 0x10) >> 0x13));
        }
    } else if ((s16)time >= 0x3C) {
        if (work->field_B96 >= 0x200) {
            work->field_B96 = (s16)((u16)work->field_B96 - 0x200);
        } else {
            work->field_B96 = 0;
        }
    }
    if (work->field_B8C == 0x2F) {
        arg->field_64 = 8;
    }
    arg->field_64 = 8;
    if (work->field_B8C == 0x3C) {
        i   = 0;
        off = 0x9C8;
        do {
            ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
            off                                += 0x20;
            i++;
        } while (i < 2);
    }
    if (work->field_BA9 != 0) {
        rate = 0x10;
        i    = 1;
        off  = 0x538;
        off2 = 0x8C;
        do {
            slotA       = (GpAnimSlot*)((u8*)work + off2);
            slotA->rate = rate;
            SOFT_BARRIER();
            slotB = (GpAnimSlot*)((u8*)work + off);
            off  += 0x28;
            i++;
            slotB->rate = rate;
            off2       += 0x28;
        } while (i < 0x15);
        work->field_B96 = 0;
        if (work->field_BA8 != 3) {
            work->state     = 0xE;
            work->field_BA8 = 0;
            work->field_BAA = (u8)work->field_BAA + 1;
            return;
        }
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        if (!((rng >> 0x10) & 3)) {
            work->state = 0xE;
        } else {
            work->field_BA5 = 1;
        }
        work->field_BA8 = 0;
    }
}

/// Countdown handler for the latch at 0xBA8. While the previous count is
/// below 0x1F, `field_B98` climbs by 0x40 toward 0x800; from 0x20 it falls by
/// 0x80, and each frame is mirrored into `field_B9A`.
///
/// Frame 0x20 aims a yaw at actor slot 0 — scratchpad delta, transpose,
/// `ratan2`, wrapped into [-0x800, 0x800) — then spawns from
/// `D_actor_104900_80147400`. Placement `field_BBB` 0x31 is a pair of shots;
/// otherwise one fan of three, each at ±0x12C on model part 4, with cue
/// `0x400B000A`. A set `field_BA9` stages state 0xE.
void func_actor_104900_80136BD4(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, Actor104900ShotArg* arg)
{
    SVECTOR        local;
    GsCOORDINATE2* self;
    GsCOORDINATE2* other;
    GsCOORDINATE2* part;
    MATRIX*        selfWorkm;
    Task*          spawned;
    s32            angle;
    s32            nOuter;
    s32            yaw;
    s32            kind;
    s32            nInner;
    s32            i;
    s32            j;
    s32            dir;
    u16            prev;
    u16            time;
    void*          head;
    void*          head2;
    s8*            deltaX;
    s32            otherY;
    s32            selfY;
    void*          vec;
    void*          matrix;
    s32            bridge;

    if (work->field_BA8 == 0) {
        work->field_BA4 = 8;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    prev            = work->field_B8C;
    time            = prev + 1;
    work->field_B8C = time;
    if ((u32)(prev & 0xFFFF) < 0x1F) {
        if (work->field_B98 < 0x800) {
            work->field_B98 = (u16)work->field_B98 + 0x40;
        }
    } else if ((s16)time >= 0x20) {
        if (work->field_B98 >= 0x80) {
            work->field_B98 = (u16)work->field_B98 - 0x80;
        } else {
            work->field_B98 = 0;
        }
    }
    work->field_B9A = work->field_B98;
    if (work->field_B8C == 0x20) {
        self = ((TmdObject*)task->extra)->coords;
        if (Gp_ActorSlots[0] == NULL) {
            bridge = 0;
        } else {
            other     = Gp_ActorSlots[0]->extra->coords;
            selfWorkm = &self->workm;
            __asm__("lui %0, 0x1F80" : "=r"(head) : "r"(other));
            head   = *(void**)(head + 0x3FC);
            deltaX = (s8*)head - 0x40;
            vec    = head - 0x40;

            *(s16*)deltaX = (s16)(other->workm.t[0] - self->workm.t[0]);
            otherY        = (u16)other->workm.t[1];
            selfY         = (u16)self->workm.t[1];
            __asm__("addiu %0, %2, -0x20" : "=r"(matrix), "+r"(otherY) : "r"(head), "r"(selfY));
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(vec) : "memory");
            *(s16*)((s8*)vec + 2) = (s16)(otherY - selfY);
            *(s16*)((s8*)vec + 4) = (s16)(other->workm.t[2] - self->workm.t[2]);
            TransposeMatrix(selfWorkm, matrix);

            local = *(SVECTOR*)vec;
            gte_SetRotMatrix(matrix);
            __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
            gte_rtv0_real();
            gte_stsv(vec);

            angle = ratan2(*(s16*)deltaX, *(s16*)((s8*)vec + 4));
            if (angle >= 0x801) {
                angle -= 0x1000;
            } else if (angle < -0x800) {
                angle += 0x1000;
            }
            __asm__("lui %0, 0x1F80" : "=r"(head2));
            head2  = *(void**)(head2 + 0x3FC);
            bridge = angle;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"((void*)((u8*)head2 + 0x40)), "r"(angle) : "memory");
        }
        yaw = bridge;

        kind = 1;
        if ((s8)((Actor104900SpawnWork*)work)->field_BBB == 0x31) {
            kind = 2;
        }
        if (kind == 1) {
            nOuter = 1;
            nInner = 3;
        } else {
            nOuter = 2;
            nInner = 1;
        }
        i = 0;
        if (nOuter != 0) {
            do {
                dir = i;
                if (kind == 1) {
                    dir = (u32)~yaw >> 31;
                }
                j    = 0;
                part = &((TmdObject*)task->extra)->coords[4];
                if (nInner != 0) {
                    do {
                        arg->offset.vx = (dir != 0) ? 0x12C : -0x12C;
                        arg->offset.vy = 0;
                        arg->offset.vz = 0;
                        spawned        = Task_SpawnFromTable(&D_actor_104900_80147400, kind, yaw, 0);
                        if (spawned != NULL) {
                            Gp_CopyCoordOffset(spawned, part, &arg->offset);
                            Task_Reparent(task, spawned);
                        }
                        selfY = 0x400B000A;
                        j    += 1;
                        SndEvt_EnqueueType6(((work->field_BB8 << 22) | selfY) | (work->field_B88 << 8), arg->pan, arg->depth);
                    } while (j < nInner);
                }
                i += 1;
            } while (i < nOuter);
        }
    }
    ActorsShared801357f0(enemy, task, work, (ActorsShared80138efcArg*)arg);
    if (work->field_BA9 != 0) {
        work->state     = 0xE;
        work->field_BA8 = 0;
        work->field_BAA = work->field_BAA + 1;
    }
}

/// Distance to actor slot 3, squared, through the scratch pool. Each access of
/// `G_SCRATCH_HEAD` is its own inline so the address stays a rematerialized
/// `lui`/`lw` of `0x1F8003FC`, and the macro writes the caller's variable so
/// the distance is one pseudo.
static __inline__ u8* Actor104900_ScratchRead(void)
{
    return *(u8**)G_SCRATCH_HEAD;
}

static __inline__ void Actor104900_ScratchWrite(u8* p)
{
    *(u8**)G_SCRATCH_HEAD = p;
}

#define Actor104900_DistToPlayer(arg0, out)                           \
    {                                                                 \
        u8*            head;                                          \
        SVECTOR*       vec;                                           \
        GsCOORDINATE2* coord;                                         \
        Task*          slot;                                          \
                                                                      \
        slot = gameGetPtrSlot(3);                                     \
        if (slot == NULL) {                                           \
            out = 0x7FFFFFFF;                                         \
        } else {                                                      \
            coord   = ((TmdObject*)slot->extra)->coords;              \
            head    = Actor104900_ScratchRead();                      \
            vec     = (SVECTOR*)(head - 8);                           \
            vec->vx = (u16)coord->workm.t[0] - (u16)arg0->workm.t[0]; \
            vec->vy = (u16)coord->workm.t[1] - (u16)arg0->workm.t[1]; \
            Actor104900_ScratchWrite((u8*)vec);                       \
            vec->vz = (u16)coord->workm.t[2] - (u16)arg0->workm.t[2]; \
            out     = Gfx_ApplyMatrixNoSf(vec, vec);                  \
            Actor104900_ScratchWrite(Actor104900_ScratchRead() + 8);  \
        }                                                             \
    }

/// Inlined `Gfx_MatrixCol2` plus the GPF that scales the copied column. The
/// matrix is pinned to `$v1` and the destination is born next; the empty `+r`
/// keeps that `addiu` ahead of the loads, and the GTE ops stay on the same
/// pointer. `$t4`..`$t6` are the column, as in the out-of-line copy.
static __inline__ void Actor104900_MatrixCol2(MATRIX* arg0, volatile SVECTOR* arg1, s32 scale)
{
    register MATRIX*  src asm("v1");
    register short    t4 asm("t4");
    register short    t5 asm("t5");
    register short    t6 asm("t6");
    volatile SVECTOR* out;

    src = arg0;
    out = arg1;
    asm("" : "+r"(out));
    t4      = src->m[0][2];
    t5      = src->m[1][2];
    t6      = src->m[2][2];
    out->vx = t4;
    out->vy = t5;
    out->vz = t6;
    gte_lddp(scale);
    gte_ldsv((SVECTOR*)out);
    gte_gpf12_real();
    gte_stsv((SVECTOR*)out);
}

/// Lunge. The first frame, while the latch at 0xBA8 is clear, measures the
/// squared distance to actor slot 3. No spawn argument and a target inside
/// 0xA62B0F, or any target inside 0x1DE83F, consumes one `rand` in the first
/// of those cases and stages state 0xF. Otherwise motion 9 is armed, a nibble
/// of `Gp_LcgState` picks a 1/2/3 countdown at 0xB8C (under 5, under 0xC,
/// else), the frame at 0xBAD is armed to -1 and the latch is stepped. The
/// empty asm before the 3 is not a single set, so that arm stays a fallthrough
/// `li`.
///
/// Later frames step 0xBAD while the motion id still matches and the clip has
/// not finished, then `ActorsShared801357f0` supplies the yaw at 0xB90. While
/// the frame sits in [1, 0x2E) the model's `field_46` turns toward that yaw by
/// at most 0x10 and the Y rotation is rebuilt. The same window steps
/// `((frame - 13) * 900) / 33` and, while `D_80072729` is clear, adds the
/// scaled facing column's X/Z onto the translation through the frame block's
/// vector at 0x10. Frame 1 cues `0x400B0002` and frame 0x2E cues `0x400B0001`.
///
/// Frame 0x2E remeasures the distance. Seven draws in eight leave the lunge:
/// `field_BAC` below 0xB and a yaw inside ±0x300 stage 0xC or 0xD from the
/// sign, flipped by a further one draw in eight, and only while the new
/// distance is inside 0xA62B0F. Anything else stages 0xB inside 0x89543F and
/// 0xF beyond it.
///
/// Same body as the four twins — `func_actor_101100_80136F8C` at the same
/// address, `func_actor_201100_8014EF8C` / `func_actor_204900_8014EF8C` 0x18000
/// past it and `func_actor_301100_80166F8C` 0x30000 past. A shared span here
/// would sit inside `_3`, ahead of `func_actor_104900_80137498`, and insert a
/// new overlay-local run that renames `_4`..`_7`.
void func_actor_104900_80136F8C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    GsCOORDINATE2*              actorCoords;
    GsCOORDINATE2*              coords;
    GpCoordPose*                pose;
    ActorsShared80138efcMotion* motion;
    s32                         dist;
    s32                         dist2;
    s32                         turn;
    s32                         yaw;
    s32                         yaw2;
    s32                         scale;
    s32                         frame;
    s32                         n;
    s32                         snd;
    s16                         count;
    u16                         angle;
    u32                         rng;

    if (work->field_BA8 == 0) {
        actorCoords = ((TmdObject*)task->extra)->coords;
        Actor104900_DistToPlayer(actorCoords, dist);
        if ((task->spawnArg1 == 0) && (dist <= 0xA62B0F)) {
            rand();
            work->state     = 0xF;
            work->field_BA8 = 0;
            return;
        }
        if (dist <= 0x1DE83F) {
            work->state     = 0xF;
            work->field_BA8 = 0;
            return;
        }
        rng             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState     = rng;
        work->field_BA4 = 9;
        n               = (rng >> 16) & 0xF;
        if (n < 5) {
            count = 1;
        } else if (n < 0xC) {
            count = 2;
        } else {
            asm("");
            count = 3;
        }
        work->field_B8C = count;
        work->field_BAD = -1;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    } else {
        motion = &work->motion;
        if ((work->motion.field_0 != work->field_BA4) ||
            (work->field_BAD = (u8)work->field_BAD + 1, ((u16)motion->field_2 > (u16)motion->field_6))) {
            work->field_BAD = -1;
        }
    }

    ActorsShared801357f0(enemy, task, work, arg);
    if ((u32)((u8)work->field_BAD - 1) < 0x2EU) {
        turn = work->field_B90;
        pose = (GpCoordPose*)((TmdObject*)task->extra)->coords;
        if (turn >= 0x11) {
            pose->field_46 = (u16)pose->field_46 + 0x10;
        } else if (turn < -0x10) {
            pose->field_46 = (u16)pose->field_46 - 0x10;
        } else {
            pose->field_46 = (u16)pose->field_46 + turn;
        }
        angle          = (u16)pose->field_46 & 0xFFF;
        pose->field_46 = angle;
        Gfx_RotMatrixY(&pose->coord, angle, 1);
        pose->flg = 0;
        frame     = work->field_BAD;
        scale     = ((frame - 13) * 900) / 33 - ((frame - 14) * 900) / 33;
        coords    = ((TmdObject*)task->extra)->coords;
        if (D_80072729 == 0) {
            Actor104900_MatrixCol2(&coords->coord, (volatile SVECTOR*)&arg->pad_0[0x10], scale);
            coords->coord.t[0] += ((SVECTOR*)&arg->pad_0[0x10])->vx;
            coords->coord.t[2] += ((SVECTOR*)&arg->pad_0[0x10])->vz;
            coords->flg         = 0;
        }
    }
    if (work->field_BAD == 0x2E) {
        snd = 0x400B0001;
        goto do_sound;
    }
    if (work->field_BAD == 1) {
        snd = 0x400B0002;
    do_sound:
        SndEvt_EnqueueType6((work->field_BB8 << 22) | snd | (work->field_B88 << 8), arg->pan, arg->depth);
    }
    if (work->field_BAD == 0x2E) {
        actorCoords = ((TmdObject*)task->extra)->coords;
        Actor104900_DistToPlayer(actorCoords, dist2);
        if (rand() & 7) {
            if (work->field_BAC < 0xBU) {
                yaw = work->field_B90;
                if (yaw < -0x300) {
                    goto far_state;
                }
                if (yaw < 0x301) {
                    goto close_state;
                }
            }
        far_state:
            if (dist2 <= 0x89543F) {
                work->state = 0xB;
            } else {
                work->state = 0xF;
            }
            work->field_BA8 = 0;
            return;
        close_state:
            if (dist2 <= 0xA62B0F) {
                yaw2 = yaw;
                if (!(rand() & 7)) {
                    yaw2 = -yaw2;
                }
                if (yaw2 < 0) {
                    work->state = 0xC;
                } else {
                    work->state = 0xD;
                }
                work->field_BA8 = 0;
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_3", func_actor_104900_80137498);

/// Countdown handler built around the halfword at 0xB8C.
///
/// The first frame arms the motion pair: `field_BA4` takes 0x13, or 0x14 while
/// the flag at 0xBAE is set, `field_BA5` and `field_BAF` both take 1 and the
/// countdown is zeroed, with the run-once latch at 0xBA8 stepped either way.
/// Every later frame moves the countdown up by one and, on the frame it reaches
/// 5, cues the 0x400B0003 event - the actor's id byte at 0xB88 in bits 8..15
/// and the variant at 0xBB8 in bit 22, pan and depth from the frame block -
/// then parks the countdown at -0x7FFF so it fires only once. The frame block's
/// scratch byte at 0x64 takes 3 either way, and the trigger at 0xBA9 ends the
/// sub-state: while `field_B92` still counts it keeps the state on the 0x17
/// motion with the 0x10 pair when the enemy is not carrying flag 0x2 in
/// `field_4C`, and stages the 0x14 motion through `field_BA6` when it is; once
/// that count has run out it hands the frame to the shared routine at
/// 0x80137498 on state 0x18 instead.
///
/// Same body as the four twins - `func_actor_101100_80137B1C` at the same
/// address, `func_actor_201100_8014FB1C` / `func_actor_204900_8014FB1C` 0x18000
/// past it and `func_actor_301100_80167B1C` 0x30000 past - but the last call
/// reaches this overlay's own `func_actor_104900_80137498`, so the body cannot
/// move into `src/actors/lib/`.
void func_actor_104900_80137B1C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    u16 time;

    if (work->field_BA8 == 0) {
        if (work->field_BAE == 0) {
            work->field_BA4 = 0x13;
        } else {
            work->field_BA4 = 0x14;
        }
        work->field_BA5 = 1;
        work->field_BAF = 1;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    time            = (u16)work->field_B8C + 1;
    work->field_B8C = time;
    if ((s16)time >= 5) {
        SndEvt_EnqueueType6((work->field_BB8 << 22) | ((work->field_B88 << 8) | 0x400B0003), arg->pan, arg->depth);
        work->field_B8C = -0x7FFF;
    }
    arg->field_64 = 3;
    if (work->field_BA9 != 0) {
        work->field_B9C = 0;
        if (work->field_B92 > 0) {
            if (!(enemy->reactionFlags & 2)) {
                work->field_BAB = 0x10;
                work->field_BAF = 2;
                work->state     = 0x17;
            } else {
                work->field_BA6 = 2;
                work->field_BAB = 5;
                work->state     = 0x14;
            }
            work->field_BA8 = 0;
            return;
        }
        work->state     = 0x18;
        work->field_BA8 = 0;
        func_actor_104900_80137498(enemy, task, work, arg);
    }
}

/// Spawns the effect this actor's next state rides on and re-homes the actor.
///
/// The 0x58-byte work block goes in `Task::work` and the effect task comes back
/// from `Gp_SpawnEff` as `0x60081` parented to the model's trailing coordinate;
/// that task becomes `Task::spawnArg2` and the actor's parent, and the actor arms
/// its own 0x5A kill countdown.
///
/// The effect's velocity is a random direction in the actor's frame: an SVECTOR
/// is built 8 bytes into the scratchpad pool below its published head, with X
/// and Z from `rsin` / `rcos` of the spawn argument and Y a 9-bit draw hung below
/// 0xE000, rotated through the actor's current `coord` and then scaled by
/// `((Gp_LcgState >> 16) & 0x1F) + 0x28` of 0x1000, which the work block keeps.
/// The coordinate is reset to the identity first - a 0x1000 diagonal, the
/// off-diagonal pairs written as zeroed words - then the velocity's X and Z are
/// added to its translation and a 7-bit draw to the Y, and `flg` is cleared.
///
/// The display node is linked as kind 3 pointing at the coordinate and at the
/// 0x28 record, which takes 0x96 for `end0Radius` / `end1Radius` and points
/// `recs` at the one-entry collision table `Gp_InitRec18Table` zeroes, and
/// its `0xC000` flag pair is ORed in on top of `Gp_LinkObj`'s `flags = 3`. The
/// actor takes `ActorsShared801511c8` as its exit callback and steps on to the
/// next state, which it also runs immediately.
///
/// The stack copy of the vector is what the first `lwc2` pair reads, and it is
/// written with the sibling bodies' raw asm: `gte_ldv0` of a stack local leaves
/// its `addiu` free for sched2 to hoist, which this body's schedule does not.
void func_actor_104900_80137C88(Task* task)
{
    Actor104900Work* work;
    GpActorD4Rec*    rec;
    GsCOORDINATE2*   coord;
    GpEffWork*       eff;
    GpObj*           obj;
    MATRIX*          mtx;
    SVECTOR          local;
    SVECTOR*         vec;
    u8*              head;
    s32              angle;

    coord = ((TmdObject*)task->extra)->coords;
    work  = memCalloc(0x58, 0);
    if (work == NULL) {
        Task_CallExit(task);
        return;
    }
    task->work = (TaskIdMap*)work;
    eff        = Gp_SpawnEff(0x60081, coord, 0, 0);
    if (eff == NULL) {
        Task_CallExit(task);
        return;
    }
    task->spawnArg2 = eff->task;
    Task_Reparent(task, eff->task);
    angle               = task->spawnArg1;
    task->killCountdown = 0x5A;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 8;
    vec                   = (SVECTOR*)(head - 8);
    Gp_LcgState           = Gp_LcgState * 5 + 0x71357911;
    vec->vy               = 0xE000 - ((Gp_LcgState >> 16) & 0x1FF);
    vec->vx               = rsin(angle);
    vec->vz               = rcos(angle);

    mtx   = &coord->coord;
    local = *(SVECTOR*)(head - 8);
    gte_SetRotMatrix(mtx);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    gte_rtv0_real();
    gte_stsv(vec);

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    gte_lddp(((Gp_LcgState >> 16) & 0x1F) + 0x28);
    gte_ldsv(vec);
    gte_gpf12_real();
    gte_stsv(&work->vel);

    *(s32*)&mtx->m[0][0] = 0x1000;
    *(s32*)&mtx->m[0][2] = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    *(s32*)&mtx->m[2][0] = 0;
    mtx->m[2][2]         = 0x1000;

    coord->coord.t[0] += work->vel.vx;
    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
    coord->coord.t[1] += (Gp_LcgState >> 16) & 0x7F;
    coord->coord.t[2] += work->vel.vz;
    coord->flg         = 0;

    obj            = &work->obj;
    rec            = &work->rec;
    obj->coord     = coord;
    obj->ctx.d4rec = rec;
    obj->pos.vx    = 0;
    obj->pos.vy    = 0;
    obj->pos.vz    = 0;
    obj->radius    = 0;
    obj->key       = Gp_PackPair(&D_actor_104900_801392F0[0], 5);
    obj->flags     = 3;

    rec->recs       = work->rec18;
    rec->end1.vx    = 0;
    rec->end1.vy    = 0;
    rec->end1.vz    = 0;
    rec->end0.vx    = 0;
    rec->end0.vy    = 0;
    rec->end0.vz    = 0;
    rec->end0Radius = 0x96;
    rec->end1Radius = 0x96;
    Gp_InitRec18Table(work->rec18, 1, 0);
    Gp_LinkObj(3, obj);
    obj->flags |= 0xC000;

    task->exitCallback     = ActorsShared801511c8;
    *(u8**)G_SCRATCH_HEAD += 8;
    task->state           += 1;
    ActorsShared80137fb8(task);
}
