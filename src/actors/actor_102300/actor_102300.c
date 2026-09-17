#include "common.h"

#include "decomp/common.h"

#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/tmd.h"
#include "main/session.h"
#include "main/fs.h"
#include "gameplay/D4.h"
#include "gameplay/3FB8.h"

#include <psyq/abs.h>

#include "actors/actor_102300.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_102300_80131E80;

/// First frame of each animation, indexed by `Actor102300Work::field_694`;
/// the state handlers offset it to get the frames their cues fire on.
extern s16 D_actor_102300_80135D64[];
/// The `Gp_PackPair` entry the lunge parks in the work block's 0x5E4 node.
extern GpU16Pair D_actor_102300_801477E4;
/// Base sound id of the lunge cue, ORed with the enemy's id nibble.
extern s32 D_actor_102300_80147918;

/// The `GpPairSrcE` the enemy parks in its own `field_50` slot.
extern GpPairSrcE D_actor_102300_801477F8;
/// Per-room voice-stream sector tables, indexed by `GameSession::field_7` then
/// `field_6`; a NULL row means this room has no cue.
extern u16* D_actor_102300_80147AA0[];
/// The overlay's own spawn table: entry 0 is this enemy, 1 and 2 the two
/// companions the setup state spawns.
extern TaskDesc D_actor_102300_80147AB8;
/// Animation bank `func_800B3F84` binds to the work block.
extern u8 D_actor_102300_80147ADC[];
/// The gameplay LCG state the setup state advances for its coin flip.
extern u32 Gp_LcgState;

INCLUDE_RODATA("actors/nonmatchings/actor_102300/actor_102300", D_actor_102300_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80131EA4);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_8013290C);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80132AF0);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80132C2C);

INCLUDE_RODATA("actors/nonmatchings/actor_102300/actor_102300", D_actor_102300_80131E80);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80133008);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80133100);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_8013332C);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_801334B8);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_801336C4);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80133840);

/// Per-frame tick for the enemy's charge, sharing `field_6A8` with the rest of
/// the overlay and measuring the offset to the player through a 0x10-byte
/// `G_SCRATCH_HEAD` block. State 0 waits out the wind-up: from frame 0x47 it
/// mirrors `field_6D0` into `field_6CE` and commits to the charge
/// (`field_69C` = 0x84) unless the player is already 1000 units away, zeroes
/// the cycle counter `field_6B6` on frame 0x46, and hands over to state 1 on
/// animation 6 once the animation is past its start frame plus 0x52. State 1
/// drives the charge, aiming `field_6A4` at the player each frame, switching to
/// state 2 on animation 7 within 1500 units and on animation 4 once the heading
/// has drifted more than 0x100 from `field_6A2`. State 2 raises the 0x5E4
/// node's 0x8000 flag and queues the cue on frame 0xD, drops the flag on frame
/// 0x1E, and from frame 0x3B picks animation 8 (back to state 0) within 3000
/// units or animation 4 otherwise. A `field_6B6` of 0x4C at any point aborts
/// the whole cycle back to animation 8.
void func_actor_102300_80133C10(Actor102300* arg0)
{
    s16              state;
    s16              diff;
    s16              turn;
    s32*             scratch;
    s32              dx;
    s32              dz;
    s32              dxAim;
    s32              dzAim;
    s32              dxHold;
    s32              dzHold;
    s32              dist;
    s32              sound;
    s32              pan;
    u8*              head;
    Actor102300Work* work;
    GsCOORDINATE2*   self;
    VECTOR*          delta;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x10;
    delta                 = (VECTOR*)(head - 0x10);
    work                  = arg0->field_1C;
    state                 = work->field_6A8;
    self                  = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            if ((work->field_698 >= 0x47) && (work->field_6CE              = (s16)(work->field_6D0 > 0),
                                              ((VECTOR*)(head - 0x10))->vx = (s32)(Wip_SysConfig.field_4->t[0] - self->coord.t[0]),
                                              dz                           = Wip_SysConfig.field_4->t[2] - self->coord.t[2],
                                              delta->vz                    = dz,
                                              dx                           = ((VECTOR*)(head - 0x10))->vx,
                                              ((SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) == 0))) {
                work->field_69C = 0x84;
            } else {
                work->field_69C = 0;
            }
            work->field_69E = 0;
            if (work->field_698 == 0x46) {
                work->field_6B6 = 0;
                work->field_6CC = 1;
            }
            if ((work->field_698 >= 0x47) && (work->field_6B6 >= 0x4C)) {
                work->field_6A6       = 8;
                work->field_6A8       = 0;
                work->field_69C       = 0;
                work->field_69E       = 0;
                work->field_6CC       = 0;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
                break;
            }
            if (work->field_698 >= (D_actor_102300_80135D64[work->field_694] + 0x52)) {
                work->field_6A8 = 1;
                work->field_694 = 6;
            }
            break;
        case 1:
            work->field_69C              = 0x84;
            work->field_69E              = 0;
            ((VECTOR*)(head - 0x10))->vx = (s32)(Wip_SysConfig.field_4->t[0] - self->coord.t[0]);
            delta->vz                    = (s32)(Wip_SysConfig.field_4->t[2] - self->coord.t[2]);
            work->field_6A4              = (s16)(ratan2((s32)(s16)((VECTOR*)(head - 0x10))->vx, (s32)(s16)delta->vz) & 0xFFF);
            dxAim                        = ((VECTOR*)(head - 0x10))->vx;
            dzAim                        = delta->vz;
            dist                         = SquareRoot0((dxAim * dxAim) + (dzAim * dzAim));
            if (work->field_6B6 >= 0x4C) {
                goto reset;
            }
            if (dist < 0x5DC) {
                work->field_6A8 = 2;
                work->field_694 = 7;
                work->field_69C = 0;
            } else {
                diff = (ratan2((s32)(s16)((VECTOR*)(head - 0x10))->vx, (s32)(s16)delta->vz) & 0xFFF) - work->field_6A2;
                turn = (abs(diff) >= 0x800) ? ((diff > 0) ? 0x1000 - diff : diff + 0x1000) : abs(diff);
                if (turn > 0x100) {
                    work->field_6A6 = 2;
                    work->field_6A8 = 2;
                    work->field_694 = 4;
                    work->field_6CC = 0;
                    work->field_6CE = 0;
                }
            }
            break;
        case 2:
            work->field_69C = 0;
            work->field_69E = 0;
            if ((work->field_698 < 0xD) && (work->field_6B6 >= 0x4C)) {
            reset:
                work->field_6A6       = 8;
                work->field_6A8       = 0;
                work->field_69C       = 0;
                work->field_69E       = 0;
                work->field_6CC       = 0;
                work->field_6CE       = 0;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
                break;
            }
            if (work->field_698 == 0xD) {
                work->field_5E4.flags    = (u16)(work->field_5E4.flags | 0x8000);
                work->field_5E4.field_18 = Gp_PackPair(&D_actor_102300_801477E4, 1);
                sound                    = D_actor_102300_80147918 | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan                      = (s8)Gp_GetObjPan((GpObj38*)self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)self));
            }
            if (work->field_698 == 0x1E) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
            }
            if (work->field_698 >= 0x3B) {
                work->field_6CE = 0;
                dxHold          = Wip_SysConfig.field_4->t[0] - self->coord.t[0];
                delta->vx       = dxHold;
                dzHold          = Wip_SysConfig.field_4->t[2] - self->coord.t[2];
                delta->vz       = dzHold;
                if (SquareRoot0((dxHold * dxHold) + (dzHold * dzHold)) < 0xBB8) {
                    work->field_6A6 = 4;
                    work->field_6A8 = 0;
                    work->field_694 = 8;
                } else {
                    work->field_6A6 = 2;
                    work->field_6A8 = 2;
                    work->field_694 = 4;
                }
            }
            break;
    }
    scratch   = (s32*)G_SCRATCH_HEAD;
    *scratch += 0x10;
}

/// Per-frame tick for the enemy's lunge cycle, sharing the `field_6A8` state
/// with the rest of the overlay. State 0 measures the offset to the player
/// through a 0x10-byte `G_SCRATCH_HEAD` block: over the window from frame 0x22
/// to 0x26 of the current animation the enemy commits to the lunge
/// (`field_69C` = 0x84) unless the player is already 1000 units away, aims
/// `field_6A4` at them every frame, raises the 0x5E4 node's 0x8000 flag on
/// frame 0x20 and queues the cue on frame 0x21, then hands over to state 1 on
/// animation 9 once the animation is past frame 0x27. State 1 waits for frame
/// 0x5E and moves on to state 2 on animation 4.
void func_actor_102300_801340B0(Actor102300* arg0)
{
    s16              startFrame;
    s16              state;
    s16              frame;
    s32*             scratch;
    s32              dz;
    s32              sound;
    s32              dx;
    s32              pan;
    u8*              head;
    Actor102300Work* work;
    GsCOORDINATE2*   self;
    VECTOR*          delta;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x10;
    delta                 = (VECTOR*)(head - 0x10);
    work                  = arg0->field_1C;
    state                 = work->field_6A8;
    self                  = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            ((VECTOR*)(head - 0x10))->vx = (s32)(Wip_SysConfig.field_4->t[0] - self->coord.t[0]);
            dz                           = Wip_SysConfig.field_4->t[2] - self->coord.t[2];
            delta->vz                    = dz;
            startFrame                   = D_actor_102300_80135D64[work->field_694];
            frame                        = work->field_698;
            if ((frame >= (startFrame + 0x22)) && ((startFrame + 0x26) >= frame) && (dx = ((VECTOR*)(head - 0x10))->vx, ((SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) == 0))) {
                work->field_69C = 0x84;
            } else {
                work->field_69C = 0;
            }
            work->field_69E = 0x14;
            work->field_6A4 = (s16)(ratan2((s32)(s16)delta->vx, (s32)(s16)delta->vz) & 0xFFF);
            if (work->field_698 == (D_actor_102300_80135D64[work->field_694] + 0x20)) {
                work->field_5E4.flags    = (u16)(work->field_5E4.flags | 0x8000);
                work->field_5E4.field_18 = Gp_PackPair(&D_actor_102300_801477E4, 0);
            }
            if (work->field_698 == (D_actor_102300_80135D64[work->field_694] + 0x21)) {
                sound = D_actor_102300_80147918 | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan   = (s8)Gp_GetObjPan((GpObj38*)self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)self));
            }
            if (work->field_698 >= (D_actor_102300_80135D64[work->field_694] + 0x27)) {
                work->field_6A8       = 1;
                work->field_694       = 9;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            if (work->field_698 >= 0x5E) {
                work->field_6A6 = 2;
                work->field_6A8 = 2;
                work->field_694 = 4;
            }
            break;
    }
    scratch   = (s32*)G_SCRATCH_HEAD;
    *scratch += 0x10;
}

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80134338);

/// Spawn/setup state for this enemy. Allocates the 0x6E4 work block, wires the
/// model object to the block's own light/colour matrices, primes the nineteen
/// animation slots, then spawns the two companion enemies from the overlay's
/// table (entries 2 and 1) and points each one's model at the texture page and
/// CLUT row its room's `GpCdRec10` names.
///
/// `GpEnemy::field_4B` then picks how the enemy starts: 0 builds the full
/// object set -- the four `GpObj` nodes with their `GpRec18` tables, the voice
/// cue looked up per room in `D_actor_102300_80147AA0`, and the coin-flip in
/// `field_6C4` drawn from `Gp_LcgState` -- while 1 and 2 only prime the
/// animation state and hand straight on to the next task state.
void func_actor_102300_801346CC(GpEnemy* enemy, Actor102300* actor)
{
    Actor102300Work* work;
    TmdObject*       obj;
    TmdObject*       model;
    TmdObject*       model2;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parts;
    GsCOORDINATE2*   partsA;
    GsCOORDINATE2*   partsB;
    GsCOORDINATE2*   partsC;
    GsCOORDINATE2*   effParts;
    GpAreaKey*       sessionKey;
    GpAreaKey*       sessionKey2;
    GpAreaKey*       keyPtr;
    GpAreaKey*       keyPtr2;
    u8               areaByte0;
    u8               areaByte02;
    GpAreaRec*       rec;
    GpAreaRec*       rec2;
    GpCdRec10*       entry;
    GpCdRec10*       entry2;
    GpEnemy*         eff;
    GpEnemy*         eff2;
    u16*             tbl;
    u8               param1[8];
    u8               param2[8];
    GpAreaKey        key;
    s32              i;
    s32              one;
    s32              kind;
    s32              idx;
    s32              idx2;
    s32              param;
    u32              lcg;

    obj   = actor->field_2C;
    coord = obj->field_8;
    work  = Mem_Calloc(0x6E4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)actor);
        return;
    }
    actor->field_1C = work;
    obj->field_C    = 0;
    coord->flg      = 0;
    obj->field_1C   = &work->field_45C;
    obj->field_20   = &work->field_43C;
    work->field_6CA = 0x17;
    work->field_66C = &D_actor_102300_80147AB8;
    work->field_670 = &actor->field_2C->field_8[3];
    work->field_674 = 0x500;
    work->field_676 = 2;
    func_800B3F84(&work->anim, D_actor_102300_80147ADC, (GpAnimObj*)obj, work->poses, work->slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }

    eff         = Gp_SpawnEnemyFromTable(&D_actor_102300_80147AB8, 2, 0, enemy);
    sessionKey  = (GpAreaKey*)&Game_Session->field_4;
    model       = eff->task->extra;
    idx         = enemy->field_8 >> 12;
    key.field_3 = sessionKey->field_3;
    key.field_2 = sessionKey->field_2;
    key.field_1 = sessionKey->field_1;
    areaByte0   = sessionKey->field_0;
    /* Both calls take `&key`. Left alone, GCC 2.8.1 CSEs that address into one
       pseudo that is live across the first call, costing a callee-saved
       register; the ROM rematerializes `addiu a0, sp, key` for each call. The
       barrier keeps the address materialization next to the call and the
       `+r` touch makes the second one a fresh computation. */
    keyPtr      = &key;
    key.field_0 = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    SOFT_DEF_REG(keyPtr);
    keyPtr          = &key;
    rec             = Gp_GetNestedAreaRec(keyPtr);
    entry           = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
    model->field_24 = entry->field_D;
    model->field_25 = entry->field_E;
    if (model->field_18 != NULL) {
        Tmd_ProcessStream(model);
        Tmd_ProcessStream(model);
    }
    SOFT_DEF_REG(keyPtr);

    eff2        = Gp_SpawnEnemyFromTable(&D_actor_102300_80147AB8, 1, 0, enemy);
    sessionKey2 = (GpAreaKey*)&Game_Session->field_4;
    model2      = eff2->task->extra;
    idx2        = enemy->field_8 >> 12;
    key.field_3 = sessionKey2->field_3;
    key.field_2 = sessionKey2->field_2;
    key.field_1 = sessionKey2->field_1;
    areaByte02  = sessionKey2->field_0;
    keyPtr2     = &key;
    key.field_0 = areaByte02;
    Gp_SyncAreaKeyIndex(keyPtr2);
    SOFT_DEF_REG(keyPtr2);
    keyPtr2          = &key;
    rec2             = Gp_GetNestedAreaRec(keyPtr2);
    entry2           = (GpCdRec10*)((idx2 << 4) + (s32)rec2->field_0);
    model2->field_24 = entry2->field_D;
    model2->field_25 = entry2->field_E;
    if (model2->field_18 != NULL) {
        Tmd_ProcessStream(model2);
        Tmd_ProcessStream(model2);
    }

    one  = 1;
    kind = enemy->field_4B;
    if (kind == one) {
        goto case1;
    }
    if (kind >= 2) {
        goto ge2;
    }
    if (kind == 0) {
        goto case0;
    }
    return;
ge2:
    if (kind == 2) {
        goto case2;
    }
    return;

case0:
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    Gp_LinkNode(&enemy->node);
    parts              = actor->field_2C->field_8;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_50    = &D_actor_102300_801477F8;
    enemy->field_54    = (s32)work->field_4EC;
    enemy->field_18    = &parts[3];
    enemy->field_40    = D_actor_102300_801477F8.field_4;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_6AC = ((GpAreaPlace*)enemy->field_3C)->field_2 & 1;
    if (work->field_6AC == 0) {
        work->field_694 = one;
        work->field_6A6 = 0;
    } else {
        work->field_694 = 2;
        work->field_6A6 = one;
        param           = ((GpAreaPlace*)enemy->field_3C)->field_1;
        work->field_6DA = param * 1000;
    }

    tbl = D_actor_102300_80147AA0[Game_Session->field_7];
    if (tbl != NULL) {
        work->field_6D6 = tbl[Game_Session->field_6];
    }
    if (work->field_6D6 != 0) {
        param1[3] = 0;
        param1[2] = 0xA;
        param1[0] = work->field_6D6;
        param2[0] = 0x17;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
    }

    work->field_6D0          = 0xFA;
    work->field_49C.field_4  = 0x1F40;
    work->field_49C.field_10 = 0x3E8;
    work->field_49C.field_12 = 0x5DC;
    work->field_49C.field_0  = 0;
    work->field_49C.field_2  = 0;
    work->field_49C.field_8  = 0;
    work->field_49C.field_A  = 0;
    work->field_49C.field_C  = 0;
    work->field_49C.field_14 = work->field_4B4;
    lcg                      = (Gp_LcgState * 5) + 0x71357911;
    work->field_6C4          = ((lcg >> 16) & 1) + 1;
    Gp_LcgState              = lcg;
    partsA                   = actor->field_2C->field_8;
    work->field_47C.field_C  = (GpRec18*)&work->field_49C;
    work->field_47C.field_10 = 0;
    work->field_47C.field_12 = 0;
    work->field_47C.field_14 = 0;
    work->field_47C.field_18 = 0;
    work->field_47C.field_1C = 0;
    work->field_47C.flags    = 3;
    work->field_47C.field_8  = &partsA[4];
    Gp_LinkObj(3, &work->field_47C);
    Gp_InitRec18Table(work->field_4B4, 1, 0);
    work->field_47C.flags |= 0xCC00;

    partsB                   = actor->field_2C->field_8;
    work->field_4CC.field_C  = work->field_4EC;
    work->field_4CC.field_10 = 0;
    work->field_4CC.field_12 = 0;
    work->field_4CC.field_14 = 0;
    work->field_4CC.field_18 = 0x30017;
    work->field_4CC.field_1C = 0x190;
    work->field_4CC.flags    = 1;
    work->field_4CC.field_8  = &partsB[3];
    Gp_LinkObj(2, &work->field_4CC);
    Gp_InitRec18Table(work->field_4EC, 5, 0);
    work->field_4CC.flags |= 0x8000;

    partsC                   = actor->field_2C->field_8;
    work->field_564.field_12 = -0x226;
    work->field_564.field_C  = work->field_584;
    work->field_564.field_10 = 0;
    work->field_564.field_14 = 0;
    work->field_564.field_18 = 0;
    work->field_564.field_1C = 0x226;
    work->field_564.flags    = 1;
    work->field_564.field_8  = partsC;
    Gp_LinkObj(2, &work->field_564);
    Gp_InitRec18Table(work->field_584, 4, 0);
    work->field_564.flags |= 0x4200;

    effParts                 = ((TmdObject*)eff2->task->extra)->field_8;
    work->field_5E4.field_C  = work->field_604;
    work->field_5E4.field_10 = 0;
    work->field_5E4.field_12 = 0x1F4;
    work->field_5E4.field_14 = 0;
    work->field_5E4.field_18 = 0;
    work->field_5E4.field_1C = 0x1F4;
    work->field_5E4.flags    = 1;
    work->field_5E4.field_8  = effParts;
    Gp_LinkObj(3, &work->field_5E4);
    Gp_InitRec18Table(work->field_604, 1, 0);
    work->field_5E4.flags &= 0x7FFF;
    actor->field_30        = 1;
    return;

case1:
    work->field_694 = 0x19;
    work->field_6A8 = 2;
    actor->field_30 = 2;
    return;

case2:
    work->field_694 = 0x1D;
    work->field_6A8 = kind;
    actor->field_30 = kind;
}

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300", func_actor_102300_80134CC0);

/// The lunge's own tick, run out of a 0x10-byte `G_SCRATCH_HEAD` block. State
/// 0 is the wind-up: it holds `field_69C` at 0 until the animation reaches its
/// start frame, aims `field_6A4` at the player and compares it with the
/// enemy's own facing `field_6A2` - past 0x581 apart it gives up and turns
/// (animation 3, or animation 4 when `field_6DC` says it has already turned
/// once), within 0x80 it raises the body node's 0xC000 flags and commits as
/// soon as `field_6B2` reports contact. State 1 picks what to do next: inside
/// 0x8CA of the player it lunges (animation 8), otherwise it draws from
/// `Gp_LcgState` through a mask that widens by a bit each cycle and either
/// circles (animation 0xA) or walks in (animation 5). State 2 waits out the
/// recovery and state 3 the turn.
void func_actor_102300_8013509C(Actor102300* actor)
{
    s16              yaw;
    s16              yaw2;
    s16              state;
    s16              deltaYaw;
    s16              deltaYaw2;
    s16              speed;
    s32              magnitude;
    s32              magnitude2;
    s16              wrapped;
    s16              wrapped2;
    s16              angle;
    s32              dx;
    s32              dz;
    u32              random;
    u16              flags;
    u16              flags2;
    u8*              head;
    VECTOR*          delta;
    Actor102300Work* work;
    GsCOORDINATE2*   coord;

    head                  = *(u8**)G_SCRATCH_HEAD;
    delta                 = (VECTOR*)(head - 0x10);
    *(u8**)G_SCRATCH_HEAD = (u8*)delta;
    work                  = actor->field_1C;
    state                 = work->field_6A8;
    coord                 = actor->field_2C->field_8;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= D_actor_102300_80135D64[work->field_694]) {
                speed = 0x14;
            }
            work->field_69C = speed;
            work->field_69E = 0x3C;
            delta->vx       = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
            delta->vz       = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
            work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
            yaw             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
            work->field_6A2 = yaw;
            deltaYaw        = (u16)work->field_6A4 - yaw;
            magnitude       = __builtin_abs(deltaYaw);
            if (magnitude < 0x800) {
                angle = magnitude;
            } else {
                if (deltaYaw > 0) {
                    wrapped = 0x1000 - deltaYaw;
                } else {
                    wrapped = deltaYaw + 0x1000;
                }
                angle = wrapped;
            }
            if (angle >= 0x581) {
                if (work->field_6DC == 0) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 4;
                    work->field_6A8 = 1;
                    work->field_6DC = 0;
                }
            }
            if (angle < 0x80) {
                flags                 = work->field_47C.flags | 0xC000;
                work->field_47C.flags = flags;
                if (work->field_6B2 != 0) {
                    work->field_47C.flags = (u16)(flags & 0x3FFF);
                    work->field_6A8       = 1;
                    work->field_6DC       = 0;
                }
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            delta->vx       = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
            dz              = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
            delta->vz       = dz;
            dx              = delta->vx;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x8CA) {
                work->field_6A6 = 4;
                work->field_6A8 = 0;
                work->field_694 = 8;
            } else {
                random      = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState = random;
                if (!((random >> 0x10) & ((1 << (work->field_6C0 + 1)) - 1)) && !(Wip_SysConfig.field_25 & 0x10) &&
                    work->field_6C4 != 0) {
                    work->field_6A6 = 5;
                    work->field_6A8 = 0;
                    work->field_694 = 0xA;
                    work->field_6AE = 0;
                    work->field_6C2 = 1;
                    work->field_6B6 = 0;
                    work->field_6C0++;
                } else {
                    work->field_6A6 = 3;
                    work->field_6A8 = 0;
                    work->field_694 = 5;
                    work->field_6AE = 0;
                }
            }
            break;
        case 2:
            work->field_69C       = 0;
            work->field_69E       = 0;
            flags2                = work->field_47C.flags | 0xC000;
            work->field_47C.flags = flags2;
            if (work->field_6B2 != 0) {
                work->field_47C.flags = (u16)(flags2 & 0x3FFF);
                work->field_694       = 2;
                work->field_6A8       = 0;
            } else if (work->field_698 >= 0x60) {
                delta->vx       = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
                delta->vz       = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
                work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
                yaw2            = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                work->field_6A2 = yaw2;
                deltaYaw2       = (u16)work->field_6A4 - yaw2;
                magnitude2      = __builtin_abs(deltaYaw2);
                if (magnitude2 < 0x800) {
                    angle = magnitude2;
                } else {
                    if (deltaYaw2 > 0) {
                        wrapped2 = 0x1000 - deltaYaw2;
                    } else {
                        wrapped2 = deltaYaw2 + 0x1000;
                    }
                    angle = wrapped2;
                }
                if (angle >= 0x581) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 2;
                    work->field_6A8 = 0;
                }
            }
            break;
        case 3:
            work->field_69C = 0;
            work->field_69E = 0x3B;
            if (work->field_698 >= 0x23) {
                work->field_694 = 2;
                work->field_6A8 = 0;
                work->field_6DC = 1;
            }
            break;
    }
    *(s32*)G_SCRATCH_HEAD += 0x10;
}
