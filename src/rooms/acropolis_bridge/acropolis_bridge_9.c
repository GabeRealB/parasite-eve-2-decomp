#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern GpEnemyTaskFuncTable3 D_acropolis_bridge_8017D6E8;
extern u16                   D_801153F6;
extern s32                   Gp_LcgState;

void func_800FDB18(s32 arg0, GsCOORDINATE2* arg1, SVECTOR* arg2, GpEffArg* arg3);
void func_acropolis_bridge_8018581C(Task* task);

/// Work block the bridge enemy's task keeps at `Task::idMap`. `field_4` is the
/// live flag every state handler in this unit gates on, and `field_12E` /
/// `field_196` are the two flag halfwords whose bit 15 the handlers toggle to
/// enable one behaviour and disable the other. `field_1F0` is the `GpEffArg`
/// the death effect is spawned with and `field_290` the death-sequence frame
/// counter.
typedef struct AcropolisBridgeEnemyWork {
    /* 0x000 */ s16      field_0;
    /* 0x002 */ byte     pad_2[0x2];
    /* 0x004 */ s16      field_4;
    /* 0x006 */ byte     pad_6[0xFA];
    /* 0x100 */ s16      field_100;
    /* 0x102 */ byte     pad_102[0x2];
    /* 0x104 */ s16      field_104;
    /* 0x106 */ byte     pad_106[0x2];
    /* 0x108 */ s16      field_108;
    /* 0x10A */ byte     pad_10A[0x2];
    /* 0x10C */ s16      field_10C;
    /* 0x10E */ byte     pad_10E[0x20];
    /* 0x12E */ u16      field_12E;
    /* 0x130 */ byte     pad_130[0x66];
    /* 0x196 */ u16      field_196;
    /* 0x198 */ byte     pad_198[0x58];
    /* 0x1F0 */ GpEffArg field_1F0;
    /* 0x1F8 */ byte     pad_1F8[0x98];
    /* 0x290 */ u16      field_290;
} AcropolisBridgeEnemyWork;

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184024);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184208);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_801843A0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_8018450C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184638);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184908);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184B94);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80185104);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_8018532C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_801856E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_8018581C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80185988);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80185F28);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_801861A0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_801863A8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80186618);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80186BBC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80187078);

/// Runs the bridge enemy's collapse sequence. On the first frame (work block
/// still live) it allocates the model's aux buffers, clears bit 15 of both
/// behaviour flag words, tags the link node, seeds the three behaviour
/// parameters, gives the model root coordinate a random yaw from the shared
/// LCG, drops the enemy's actor slots, arms the pending `Gp_StateF0` request
/// and restarts the frame counter. Every frame after that it ticks the counter
/// up to 100, runs `func_acropolis_bridge_8018581C` and, on frames 10, 22, 28
/// and 34, steps the light mode and model flags through the fade-out.
void func_acropolis_bridge_80187310(Task* task)
{
    AcropolisBridgeEnemyWork* work  = (AcropolisBridgeEnemyWork*)task->idMap;
    GpEnemy*                  enemy = (GpEnemy*)task->spawnArg2;
    s32                       step;

    if (work->field_4 != 0) {
        Tmd_AllocBuffers((TmdObject*)task->extra);
        work->field_196    &= 0x7FFF;
        work->field_12E    &= 0x7FFF;
        enemy->node.field_4 = 1;
        work->field_100     = 1;
        work->field_104     = 5;
        work->field_108     = 0x10;
        Gp_LcgState         = Gp_LcgState * 5 + 0x71357911;
        Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, (u32)Gp_LcgState >> 16, 1);
        ((TmdObject*)task->extra)->field_8->flg = 0;
        Gp_ClearNodeSlots(&enemy->node);
        if (Gp_StateF0.field_0 == 0 && Gp_StateF0.field_6 != 0) {
            Gp_ArmStateF0(1);
        }
        work->field_290 = 0;
    }
    if (work->field_290 < 0x65) {
        work->field_290++;
        func_acropolis_bridge_8018581C(task);
        step = work->field_290;
        switch (step) {
            case 10:
                Gp_SetLightMode((GpObj4C*)enemy, 1);
                Gp_SpawnEff(0x600A5, &((TmdObject*)task->extra)->field_8[2], 1, NULL);
                break;
            case 28:
                ((TmdObject*)task->extra)->field_C = 2;
                break;
            case 22:
                Gp_SetLightMode((GpObj4C*)enemy, 2);
                break;
            case 34:
                ((TmdObject*)task->extra)->field_C = 0x80;
                break;
        }
    }
}

/// Runs the bridge enemy's death sequence. On the first frame (work block still
/// live) it clears bit 15 of both behaviour flag words, tags the link node and
/// drops its actor slots, arms the pending `Gp_StateF0` request, credits the
/// kill if the enemy still had HP, spawns the death effect on the model's
/// second part coordinate, switches the model to light mode 1, shakes the pad
/// and restarts the frame counter. Every frame after that it ticks the counter
/// up to 100, runs `func_acropolis_bridge_8018581C` and, on frames 2, 30 and
/// 44, steps the model flags / light mode through the fade-out.
void func_acropolis_bridge_801874DC(Task* task)
{
    AcropolisBridgeEnemyWork* work  = (AcropolisBridgeEnemyWork*)task->idMap;
    GpEnemy*                  enemy = (GpEnemy*)task->spawnArg2;
    s32                       step;

    if (work->field_4 != 0) {
        work->field_196    &= 0x7FFF;
        work->field_12E    &= 0x7FFF;
        enemy->node.field_4 = 1;
        Gp_ClearNodeSlots(&enemy->node);
        if (Gp_StateF0.field_0 == 0 && Gp_StateF0.field_6 != 0) {
            Gp_ArmStateF0(1);
        }
        if (enemy->field_40 > 0) {
            Gp_ReleaseStateF0Add((GpObj20E*)task, 0x29);
        }
        work->field_1F0.field_0 = &((TmdObject*)task->extra)->field_8[1];
        work->field_1F0.field_4 = 0xA0;
        work->field_1F0.field_6 = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &((TmdObject*)task->extra)->field_8[1], NULL,
                      &work->field_1F0);
        Gp_SetLightMode((GpObj4C*)enemy, 1);
        Gp_SpawnEff(0x600A5, &((TmdObject*)task->extra)->field_8[1], 1, NULL);
        ((TmdObject*)task->extra)->field_C = 2;
        work->field_290                    = 0;
        Gp_SpawnPadLerp(3, 0xFF, 8);
    }
    if (work->field_290 < 0x65) {
        work->field_290++;
        func_acropolis_bridge_8018581C(task);
        step = work->field_290;
        switch (step) {
            case 2:
                ((TmdObject*)task->extra)->field_C = step;
                break;
            case 30:
                Gp_SetLightMode((GpObj4C*)enemy, 2);
                break;
            case 44:
                ((TmdObject*)task->extra)->field_C = 0x80;
                break;
        }
    }
}

/// Applies one hit to the bridge enemy. While the work block still has HP it
/// rolls the damage for the incoming attack id, spawns the hit effect on the
/// model's second part coordinate, quadruples the damage on a critical roll,
/// credits it to the kill tally and the link node, and subtracts it from both
/// the work block's and the enemy's HP; the pending `Gp_StateF0` request is
/// armed once the HP runs out. When there is no HP left to take (before or
/// after the hit) it steps the behaviour state instead: 5 and 6 are already
/// reaction states and stay put, 4 and 8 advance to 6, everything else resets
/// to 5.
void func_acropolis_bridge_801876A8(Task* task, u32 attackId)
{
    AcropolisBridgeEnemyWork* work  = (AcropolisBridgeEnemyWork*)task->idMap;
    GpEnemy*                  enemy = (GpEnemy*)task->spawnArg2;
    s32                       damage;
    s16                       state;

    if (work->field_10C > 0) {
        damage                  = Gp_ComputeDamage(attackId, 0, 0, 0x1000);
        work->field_1F0.field_0 = &((TmdObject*)task->extra)->field_8[1];
        work->field_1F0.field_4 = 0x80;
        work->field_1F0.field_6 = 2;
        func_800FDB18(Gp_GetIdParam1(attackId) & 0xFFFF, &((TmdObject*)task->extra)->field_8[1],
                      NULL, &work->field_1F0);
        if (Gp_RollEnemyChance(enemy, attackId, 0) != 0) {
            damage *= 4;
            Gp_SpawnEff(0x6009C, &((TmdObject*)task->extra)->field_8[1], 0, NULL);
        }
        func_800E2C78((GpObj40*)enemy, attackId, damage, 0);
        enemy->field_40 -= damage;
        func_800DA6E8(&enemy->node, damage, 0);
        work->field_10C -= damage;
        enemy->field_40  = work->field_10C;
        if (work->field_10C > 0) {
            return;
        }
        if (D_801153F6 != 0) {
            Gp_ReleaseStateF0Add((GpObj20E*)task, 0x29);
        }
        if (work->field_10C > 0) {
            return;
        }
    }
    state = work->field_0;
    if (state < 7) {
        if (state >= 5) {
            return;
        }
        if (state != 4) {
            work->field_0 = 5;
            return;
        }
        work->field_0 = 6;
    } else {
        if (state != 8) {
            work->field_0 = 5;
            return;
        }
        work->field_0 = 6;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80187850);

/// Applies a visibility request to the bridge task's model flags: no request
/// restores the default flag set, bit 0 hides the mesh outright and bit 1 adds
/// the "skip drawing" bit to whatever flags are already set. Always reports
/// success.
s32 func_acropolis_bridge_80187BD0(Task* task, s32 arg1, s32 flags)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    if (flags == 0) {
        extra->field_C = 0x80;
    } else if (flags & 1) {
        extra->field_C = 0;
    } else if (flags & 2) {
        extra->field_C |= 4;
    }
    return 1;
}

/// Relights the bridge enemy's model. Borrows a `VECTOR` from the scratchpad
/// arena, optionally refreshes the TMD's root coordinate first (`arg1 == 1`),
/// then feeds that part's world translation to `func_800D7A9C` so the object's
/// colour matrix is rebuilt for its current position, and releases the scratch.
void func_acropolis_bridge_80187C10(Task* task, s16 arg1)
{
    void**  scratch;
    u8*     head;
    VECTOR* pos;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    pos      = (VECTOR*)(head - 0x10);
    *scratch = pos;
    if (arg1 == 1) {
        ((TmdObject*)task->extra)->field_8->flg = 0;
        Gp_UpdateCoord(((TmdObject*)task->extra)->field_8);
    }
    ((VECTOR*)(head - 0x10))->vx = ((TmdObject*)task->extra)->field_8->workm.t[0];
    pos->vy                      = ((TmdObject*)task->extra)->field_8->workm.t[1];
    pos->vz                      = ((TmdObject*)task->extra)->field_8->workm.t[2];
    func_800D7A9C((TmdObject*)task->extra, pos, 0, 3);
    *scratch = (u8*)*scratch + 0x10;
}

/// Shuts the bridge enemy's animation down. While the work block is still live
/// it hides the mesh behind the default flag set, tags the enemy's link node
/// and clears bit 15 of both behaviour flag words; once the work block has been
/// cleared it instead adds the "skip drawing" bit and releases the TMD's aux
/// buffers.
void func_acropolis_bridge_80187D04(Task* task)
{
    AcropolisBridgeEnemyWork* work  = (AcropolisBridgeEnemyWork*)task->idMap;
    TmdObject*                extra = (TmdObject*)task->extra;

    if (work->field_4 != 0) {
        GpEnemy* enemy = (GpEnemy*)task->spawnArg2;

        extra->field_C      = 0x80;
        enemy->node.field_4 = 1;
        work->field_12E    &= 0x7FFF;
        work->field_196    &= 0x7FFF;
        return;
    }
    extra->field_C |= 4;
    if (extra->field_18 != NULL) {
        Tmd_FreeBuffers(extra);
    }
}

/// Runs the bridge enemy's current state handler: spawn/setup
/// (`func_acropolis_bridge_80185988`), per-frame tick
/// (`func_acropolis_bridge_80187850`) or teardown (`Gp_DestroyEnemy`). The
/// table is copied onto the stack before the call, as everywhere else this
/// dispatch shape appears.
void func_acropolis_bridge_80187D80(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_acropolis_bridge_8017D6E8;
    sp.funcs[task->state](task->spawnArg2, task);
}
