#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_bridge.h"

extern s16                   D_acropolis_bridge_801915E4[][6];
extern GpEnemyTaskFuncTable3 D_acropolis_bridge_8017D6E8;
extern u16                   D_acropolis_bridge_80190C60;
extern u16                   D_801153F6;
extern s32                   Gp_LcgState;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_800FDB18(s32 arg0, GsCOORDINATE2* arg1, SVECTOR* arg2, GpEffArg* arg3);
void func_acropolis_bridge_8018581C(Task* task);

/// Work block the bridge enemy's task keeps at `Task::idMap`. `field_4` is the
/// live flag every state handler in this unit gates on, and `field_12E` /
/// `field_196` are the two flag halfwords whose bit 15 the handlers toggle to
/// enable one behaviour and disable the other. `field_1F0` is the `GpEffArg`
/// the death effect is spawned with and `field_290` the death-sequence frame
/// counter.
typedef struct AcropolisBridgeEnemyWork {
    /* 0x000 */ s16        field_0;
    /* 0x002 */ byte       pad_2[0x2];
    /* 0x004 */ s16        field_4;
    /* 0x006 */ byte       pad_6[0x6];
    /* 0x00C */ GpAnimCtx  anim;
    /* 0x020 */ GpAnimSlot slots[4];
    /* 0x0C0 */ byte       pad_C0[0x40];
    /* 0x100 */ s16        field_100;
    /* 0x102 */ s16        field_102;
    /* 0x104 */ s16        field_104;
    /* 0x106 */ u16        field_106;
    /* 0x108 */ s16        field_108;
    /* 0x10A */ byte       pad_10A[0x2];
    /* 0x10C */ s16        field_10C;
    /* 0x10E */ byte       pad_10E[0x20];
    /* 0x12E */ u16        field_12E;
    /* 0x130 */ byte       pad_130[0x66];
    /* 0x196 */ u16        field_196;
    /* 0x198 */ byte       pad_198[0x58];
    /* 0x1F0 */ GpEffArg   field_1F0;
    /* 0x1F8 */ byte       pad_1F8[0x98];
    /* 0x290 */ u16        field_290;
} AcropolisBridgeEnemyWork;

/// One entry of the patrol node table the walker task steers by. The three
/// packed coordinates are copied straight into the scratch `SVECTOR3` the
/// caller hands down, so they are read back as raw halfwords.
typedef struct AcropolisBridgeNavNode {
    /* 0x0 */ u16  x;
    /* 0x2 */ u16  y;
    /* 0x4 */ u16  z;
    /* 0x6 */ byte pad_6[0x2];
} AcropolisBridgeNavNode;

/// The room's patrol data for the walker: the node table every route indexes
/// and the second byte table `func_acropolis_bridge_8018532C` walks with its
/// own cursor at `field_76`.
typedef struct AcropolisBridgeNavData {
    /* 0x0 */ AcropolisBridgeNavNode* nodes;
    /* 0x4 */ u8*                     field_4;
} AcropolisBridgeNavData;

/// One patrol route: a 0xFF-terminated list of node indices plus the cursor
/// into it, which wraps back to the first node at the terminator. `arrived` is
/// the flag `func_acropolis_bridge_80184208` raises on the frame the walker
/// reaches its current node and the cursor steps on.
typedef struct AcropolisBridgeNavRoute {
    /* 0x0 */ u8*  nodes;
    /* 0x4 */ byte pad_4[0x1];
    /* 0x5 */ u8   cursor;
    /* 0x6 */ s8   arrived;
} AcropolisBridgeNavRoute;

/// Work block of the walker task this unit's second half drives (the task
/// `func_acropolis_bridge_8018532C` ticks). `node` is the patrol node it is
/// currently heading for and `field_62` / `field_64` the movement deltas that
/// are cleared whenever it arrives.
typedef struct AcropolisBridgeWalkerWork {
    /* 0x00 */ AcropolisBridgeNavData*  nav;
    /* 0x04 */ AcropolisBridgeNavRoute* route;
    /* 0x08 */ byte                     pad_8[0x5A];
    /* 0x62 */ s16                      field_62;
    /* 0x64 */ s16                      field_64;
    /* 0x66 */ byte                     pad_66[0x4];
    /* 0x6A */ u8                       node;
} AcropolisBridgeWalkerWork;

/// Reports whether the walker has reached the patrol node at `work->node`.
s16 func_acropolis_bridge_80184024(AcropolisBridgeWalkerWork* work);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184024);

/// Steers the walker along its patrol route. `pos` receives the position of the
/// node it is heading for; while it is still short of that node the route's
/// `arrived` flag stays clear, and on the frame it gets there the flag is
/// raised, the movement deltas are cleared and the cursor steps to the next
/// node -- wrapping back to the first when it hits the 0xFF terminator -- so
/// `pos` already describes the new node.
void func_acropolis_bridge_80184208(AcropolisBridgeWalkerWork* work, SVECTOR3* pos)
{
    AcropolisBridgeNavRoute* route;
    AcropolisBridgeNavRoute* step;
    AcropolisBridgeNavRoute* wrap;
    AcropolisBridgeNavRoute* next;
    u8                       node;

    route      = work->route;
    work->node = route->nodes[route->cursor];
    if (func_acropolis_bridge_80184024(work) == 0) {
        pos->vx              = work->nav->nodes[work->node].x;
        pos->vy              = work->nav->nodes[work->node].y;
        pos->vz              = work->nav->nodes[work->node].z;
        work->route->arrived = 0;
        return;
    }

    work->route->arrived = 1;
    step                 = work->route;
    work->field_62       = 0;
    work->field_64       = 0;
    step->cursor++;

    wrap = work->route;
    if (wrap->nodes[wrap->cursor] == 0xFF) {
        wrap->cursor = 0;
    }

    next       = work->route;
    node       = next->nodes[next->cursor];
    work->node = node;
    pos->vx    = work->nav->nodes[node].x;
    pos->vy    = work->nav->nodes[work->node].y;
    pos->vz    = work->nav->nodes[work->node].z;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_801843A0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_8018450C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184638);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184908);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184B94);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80185104);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_8018532C);

/// Handles the room's 0x7DB broadcast for the bridge enemy. Message 0x0B01/1
/// (the bridge is being lowered) restores the model's default flag set while
/// the enemy is still in one of its first three spawn variants, and message
/// 0x0E01/2 (the bridge run has ended) decides whether the enemy is armed for
/// this variant: variant 0 needs `D_801153F6` to be set at all, variant 1 needs
/// it to be at least 2 and variant 2 at least 3. When it is, the enemy and the
/// work block are given the stat block's starting HP and the behaviour state
/// advances to 4; otherwise the state resets to 0 and the mesh is hidden behind
/// the default flag set. Always reports success.
s32 func_acropolis_bridge_801856E0(Task* task, s32 msgId, AcropolisBridgeMsg7DB* msg)
{
    AcropolisBridgeEnemyWork* work  = (AcropolisBridgeEnemyWork*)task->idMap;
    GpEnemy*                  enemy = (GpEnemy*)task->spawnArg2;
    TmdObject*                extra = (TmdObject*)task->extra;
    s32                       variant;
    u16                       sub;

    if (msg->field_0 == 0xB01 && msg->field_2 == 1) {
        variant = enemy->field_8 >> 12;
        switch (variant) {
            case 0:
            case 1:
            case 2:
                extra->field_C = 0;
                break;
        }
    }
    if (msg->field_0 == 0xE01) {
        sub = msg->field_2;
        if (sub == 2) {
            variant = enemy->field_8 >> 12;
            switch (variant) {
                case 0:
                    if (D_801153F6 != 0) {
                        break;
                    }
                    work->field_0 = 0;
                    goto hide;
                case 1:
                    if (D_801153F6 >= 2) {
                        break;
                    }
                    work->field_0 = 0;
                    goto hide;
                case 2:
                    if (D_801153F6 < 3) {
                        goto reset;
                    }
                    break;
                default:
                    work->field_0 = 0;
                    goto hide;
            }
            enemy->field_40 = D_acropolis_bridge_80190C60;
            work->field_10C = D_acropolis_bridge_80190C60;
            work->field_0   = 4;
            goto done;
        reset:
            work->field_0 = 0;
        hide:
            ((TmdObject*)task->extra)->field_C = 0x80;
        }
    }
done:
    return 1;
}

/// Drives the bridge enemy's three animation slots from the state word at
/// `field_100`. State 1 restarts every slot on animation `field_104` with the
/// blend value the room's `D_acropolis_bridge_801915E4` table holds for the
/// (previous, next) animation pair, state 2 resets them without a blend, and
/// both then latch `field_104` as the previous animation and hand over to
/// state 3, which just ticks the slots once per frame and counts frames in
/// `field_106`. Every path first copies `field_108` into each slot's
/// `field_9` playback-rate byte.
void func_acropolis_bridge_8018581C(Task* task)
{
    AcropolisBridgeEnemyWork* work;
    AcropolisBridgeEnemyWork* start;
    AcropolisBridgeEnemyWork* reset;
    AcropolisBridgeEnemyWork* tick;
    s32                       i;
    s32                       j;
    s32                       k;

    work = (AcropolisBridgeEnemyWork*)task->idMap;
    if (work->field_100 == 1) {
        start = (AcropolisBridgeEnemyWork*)task->idMap;
        for (i = 1; i < 4; i++) {
            start->slots[i].field_9 = start->field_108;
            func_800B4114(&start->anim, i, start->field_104, 0,
                          D_acropolis_bridge_801915E4[start->field_102][start->field_104]);
        }
        start->field_102 = start->field_104;
        goto advance;
    }
    if (work->field_100 == 2) {
        reset = (AcropolisBridgeEnemyWork*)task->idMap;
        for (j = 1; j < 4; j++) {
            reset->slots[j].field_9 = reset->field_108;
            Gp_AnimResetSlot(&reset->anim, j, reset->field_104);
        }
        reset->field_102 = reset->field_104;
    advance:
        work->field_100 = 3;
        work->field_106 = 0;
        return;
    }
    if (work->field_100 == 3) {
        work->field_106++;
        tick = (AcropolisBridgeEnemyWork*)task->idMap;
        for (k = 1; k < 4; k++) {
            tick->slots[k].field_9 = tick->field_108;
            Gp_AnimTickIndex(&tick->anim, k);
        }
    }
}

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
