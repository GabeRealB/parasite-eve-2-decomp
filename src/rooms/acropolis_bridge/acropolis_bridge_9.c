#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "rooms/acropolis_bridge.h"

extern s16                   D_acropolis_bridge_801915E4[][6];
extern GpEnemyTaskFuncTable3 D_acropolis_bridge_8017D6E8;
extern u16                   D_acropolis_bridge_80190C60;
extern u16                   D_801153F6;
extern s32                   D_80070F70;
extern MATRIX*               D_80073B8C;
extern s32                   Gp_LcgState;

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_800FDB18(s32 arg0, GsCOORDINATE2* arg1, SVECTOR* arg2, GpEffArg* arg3);
void func_acropolis_bridge_8018581C(Task* task);

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
/// are cleared whenever it arrives. `scaleMtx` is the model matrix the spawn
/// scale-up in `func_acropolis_bridge_801861A0` rebuilds every frame from
/// `scale`, which ramps to 0x1000, and `state` is the step
/// `func_acropolis_bridge_8018532C` dispatches on. `field_91` is the flag the
/// retreat state in `func_acropolis_bridge_801863A8` clears on its first frame
/// and tests at the end of every frame to decide which behaviour state to hand
/// over to; the death handlers clear it as well.
typedef struct AcropolisBridgeWalkerWork {
    /* 0x00 */ AcropolisBridgeNavData*  nav;
    /* 0x04 */ AcropolisBridgeNavRoute* route;
    /* 0x08 */ byte                     pad_8[0x2C];
    /* 0x34 */ MATRIX                   scaleMtx;
    /* 0x54 */ s16                      scale;
    /* 0x56 */ byte                     pad_56[0x4];
    /* 0x5A */ s16                      field_5A;
    /* 0x5C */ s16                      field_5C;
    /* 0x5E */ u16                      field_5E;
    /* 0x60 */ s16                      field_60;
    /* 0x62 */ s16                      field_62;
    /* 0x64 */ s16                      field_64;
    /* 0x66 */ byte                     pad_66[0x2];
    /* 0x68 */ u8                       state;
    /* 0x69 */ byte                     pad_69[0x1];
    /* 0x6A */ u8                       node;
    /* 0x6B */ byte                     pad_6B[0x26];
    /* 0x91 */ u8                       field_91;
    /* 0x92 */ u8                       field_92;
    /* 0x93 */ byte                     pad_93[0x1];
} AcropolisBridgeWalkerWork;
STATIC_ASSERT_SIZEOF(AcropolisBridgeWalkerWork, 0x94);

/// Work block the bridge enemy's task keeps at `Task::idMap`. `field_4` is the
/// live flag every state handler in this unit gates on, and `field_12E` /
/// `field_196` are the two flag halfwords whose bit 15 the handlers toggle to
/// enable one behaviour and disable the other. `recs` is the collision record
/// table `Gp_ClearRec18Occupied` wipes, `field_1F0` is the `GpEffArg` the death
/// effect is spawned with and `field_290` the death-sequence frame counter.
typedef struct AcropolisBridgeEnemyWork {
    /* 0x000 */ s16                       field_0;
    /* 0x002 */ byte                      pad_2[0x2];
    /* 0x004 */ s16                       field_4;
    /* 0x006 */ byte                      pad_6[0x6];
    /* 0x00C */ GpAnimCtx                 anim;
    /* 0x020 */ GpAnimSlot                slots[4];
    /* 0x0C0 */ byte                      pad_C0[0x40];
    /* 0x100 */ s16                       field_100;
    /* 0x102 */ s16                       field_102;
    /* 0x104 */ s16                       field_104;
    /* 0x106 */ u16                       field_106;
    /* 0x108 */ s16                       field_108;
    /* 0x10A */ byte                      pad_10A[0x2];
    /* 0x10C */ s16                       field_10C;
    /* 0x10E */ byte                      pad_10E[0x20];
    /* 0x12E */ u16                       field_12E;
    /* 0x130 */ GpRec18                   recs[4];
    /* 0x190 */ s32                       field_190;
    /* 0x194 */ byte                      pad_194[0x2];
    /* 0x196 */ u16                       field_196;
    /* 0x198 */ byte                      pad_198[0x4];
    /* 0x19C */ s32                       field_19C;
    /* 0x1A0 */ byte                      pad_1A0[0x50];
    /* 0x1F0 */ GpEffArg                  field_1F0;
    /* 0x1F8 */ s16                       field_1F8;
    /* 0x1FA */ s16                       field_1FA;
    /* 0x1FC */ AcropolisBridgeWalkerWork walker;
    /* 0x290 */ u16                       field_290;
} AcropolisBridgeEnemyWork;
STATIC_ASSERT_SIZEOF(AcropolisBridgeEnemyWork, 0x294);

/// Ticks the walker task: steps its patrol route and drives its animation.
void func_acropolis_bridge_8018532C(AcropolisBridgeWalkerWork* walker);

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

/// Resets the walker's scale matrix to a uniform `walker->scale` scale, through
/// a `VECTOR` borrowed from the scratch arena and released again. Identity is
/// left in place at the two ends of the ramp (0 and full size), where scaling
/// would be a no-op anyway. `func_acropolis_bridge_80185F28` and
/// `func_acropolis_bridge_801863A8` both inline it on their first frame, where
/// the scratch block is taken and released around the whole matrix reset, and
/// the shrink variant below once per frame of the shrink, where the diagonal is
/// written before the block is taken.
static __inline__ void bridge_reset_scale_mtx_entry(AcropolisBridgeEnemyWork* work)
{
    AcropolisBridgeWalkerWork* walker;
    u8*                        head;
    VECTOR*                    scale;
    s32                        amount;

    head                      = *(u8**)G_SCRATCH_HEAD;
    walker                    = &work->walker;
    amount                    = walker->scale;
    scale                     = (VECTOR*)(head - 0x10);
    *(VECTOR**)G_SCRATCH_HEAD = scale;
    walker->scaleMtx.m[2][1]  = 0;
    walker->scaleMtx.m[2][0]  = 0;
    walker->scaleMtx.m[1][2]  = 0;
    walker->scaleMtx.m[1][0]  = 0;
    walker->scaleMtx.m[0][2]  = 0;
    walker->scaleMtx.m[0][1]  = 0;
    walker->scaleMtx.m[2][2]  = 0x1000;
    walker->scaleMtx.m[1][1]  = 0x1000;
    walker->scaleMtx.m[0][0]  = 0x1000;
    walker->scaleMtx.t[2]     = 0;
    walker->scaleMtx.t[1]     = 0;
    walker->scaleMtx.t[0]     = 0;
    if (amount != 0 && amount != 0x1000) {
        scale->vz                    = amount;
        scale->vy                    = amount;
        ((VECTOR*)(head - 0x10))->vx = amount;
        ScaleMatrix(&work->walker.scaleMtx, scale);
    }
    *(u8**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

/// The same matrix reset as `bridge_reset_scale_mtx_entry`, in the statement
/// order the shrink halves of `func_acropolis_bridge_80185F28` and
/// `func_acropolis_bridge_801863A8` use (and the one `bridge_scale_up` uses for
/// the spawn ramp).
static __inline__ void bridge_reset_scale_mtx_shrink(AcropolisBridgeEnemyWork* work)
{
    AcropolisBridgeWalkerWork* walker;
    u8*                        head;
    VECTOR*                    scale;
    s32                        amount;

    walker                   = &work->walker;
    head                     = *(u8**)G_SCRATCH_HEAD;
    walker->scaleMtx.m[2][2] = 0x1000;
    walker->scaleMtx.m[1][1] = 0x1000;
    walker->scaleMtx.m[0][0] = 0x1000;
    amount                   = walker->scale;
    walker->scaleMtx.m[2][1] = 0;
    walker->scaleMtx.m[2][0] = 0;
    walker->scaleMtx.m[1][2] = 0;
    walker->scaleMtx.m[1][0] = 0;
    walker->scaleMtx.m[0][2] = 0;
    walker->scaleMtx.m[0][1] = 0;
    walker->scaleMtx.t[2]    = 0;
    walker->scaleMtx.t[1]    = 0;
    walker->scaleMtx.t[0]    = 0;
    scale                    = (VECTOR*)(head - 0x10);

    *(VECTOR**)G_SCRATCH_HEAD = scale;
    if (amount != 0 && amount != 0x1000) {
        scale->vz                    = amount;
        scale->vy                    = amount;
        ((VECTOR*)(head - 0x10))->vx = amount;
        ScaleMatrix(&work->walker.scaleMtx, scale);
    }
    *(u8**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

/// Runs the bridge enemy's approach state. On the first frame (work block still
/// live) it parks the walker in step 3, clears the hand-over flag, swaps bit 15
/// between the two behaviour flag words, rebuilds the scale matrix and restarts
/// the animation slots on animation 1. Every frame after that it raises the
/// model root by 0x2D until the entry offset at `field_1F8` reaches 0x708, and
/// shrinks the walker by 0x33 a frame down to 0x801 -- rebuilding the scale
/// matrix as it goes -- tagging the enemy's link node on every frame it is
/// already that small, then ticks the walker and the animation slots. The
/// behaviour state becomes 2 once the player is at or above the bridge.
void func_acropolis_bridge_80185F28(Task* task)
{
    AcropolisBridgeEnemyWork*  work;
    AcropolisBridgeWalkerWork* walker;
    AcropolisBridgeWalkerWork* walker2;
    GpEnemy*                   enemy;
    WipSysConfig*              cfg;

    cfg   = &Wip_SysConfig;
    work  = (AcropolisBridgeEnemyWork*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work->field_4 != 0) {
        work->walker.state    = 3;
        work->walker.field_91 = 0;
        work->field_196      &= 0x7FFF;
        work->field_12E      |= 0x8000;
        bridge_reset_scale_mtx_entry(work);
        work->walker.field_5A = 0x60;
        walker                = &work->walker;
        walker->field_5C      = 0x20;
        walker->field_5E      = 0;
        walker->field_60      = 1;
        work->field_100       = 2;
        work->field_104       = 1;
        work->field_108       = 0x10;
    }
    if (work->walker.field_92 == 1) {
        walker2           = &work->walker;
        walker2->field_5C = 0x20;
        walker2->field_5E = 0x60;
        walker2->field_60 = 2;
    }
    if (work->field_1F8 < 0x708) {
        work->field_1F8 += 0x2D;
        ((TmdObject*)task->extra)->field_8->coord.t[1] =
            work->field_1FA + work->field_1F8;
        ((TmdObject*)task->extra)->field_8->flg = 0;
    }
    if (work->walker.scale >= 0x801) {
        work->walker.scale -= 0x33;
        bridge_reset_scale_mtx_shrink(work);
    } else {
        enemy->node.field_4 = 1;
    }
    func_acropolis_bridge_8018532C(&work->walker);
    func_acropolis_bridge_8018581C(task);
    if (cfg->field_4->t[1] >= 0x2BD) {
        work->field_0 = 2;
    }
}

/// Rebuilds the bridge enemy's model matrix for the spawn scale-up. `scale`
/// ramps 0x88 per frame until it reaches 0x1000, and until then the matrix is
/// reset to identity and scaled uniformly by it through a `VECTOR` taken from
/// the scratch stack.
static __inline__ void bridge_scale_up(AcropolisBridgeEnemyWork* work)
{
    AcropolisBridgeWalkerWork* walker;
    u8*                        head;
    VECTOR*                    scale;
    s32                        amount;

    work->walker.scale      += 0x88;
    walker                   = &work->walker;
    head                     = *(u8**)G_SCRATCH_HEAD;
    walker->scaleMtx.m[2][2] = 0x1000;
    walker->scaleMtx.m[1][1] = 0x1000;
    walker->scaleMtx.m[0][0] = 0x1000;
    amount                   = walker->scale;
    walker->scaleMtx.m[2][1] = 0;
    walker->scaleMtx.m[2][0] = 0;
    walker->scaleMtx.m[1][2] = 0;
    walker->scaleMtx.m[1][0] = 0;
    walker->scaleMtx.m[0][2] = 0;
    walker->scaleMtx.m[0][1] = 0;
    walker->scaleMtx.t[2]    = 0;
    walker->scaleMtx.t[1]    = 0;
    walker->scaleMtx.t[0]    = 0;
    scale                    = (VECTOR*)(head - 0x10);

    *(VECTOR**)G_SCRATCH_HEAD = scale;
    if (amount != 0 && amount != 0x1000) {
        scale->vz                    = amount;
        scale->vy                    = amount;
        ((VECTOR*)(head - 0x10))->vx = amount;
        ScaleMatrix(&work->walker.scaleMtx, scale);
    }
    *(u8**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

/// Runs the bridge enemy's spawn state. On the first frame (work block still
/// live) it tags the link node while `Gp_PackObjPair` rebuilds the enemy's
/// pair table, sets bit 15 of both behaviour flag words, seeds the walker's
/// first patrol step and starts the reset animation. Every frame after that it
/// counts the entry delay at `field_1F8` down 0x3C at a time -- dropping the
/// model root by it while it runs -- scales the model up until it reaches full
/// size, ticks the walker and the animation slots, and finally advances to
/// state 3 once the work block reports it is done, or resets to state 1 when
/// the player has dropped below the bridge.
void func_acropolis_bridge_801861A0(Task* task)
{
    AcropolisBridgeEnemyWork*  work;
    AcropolisBridgeWalkerWork* walker;
    GpEnemy*                   enemy;
    WipSysConfig*              cfg;
    s32                        done;
    u16                        height;

    cfg  = &Wip_SysConfig;
    work = (AcropolisBridgeEnemyWork*)task->idMap;
    if (work->field_4 != 0) {
        enemy = (GpEnemy*)task->spawnArg2;
        Gp_ArmStateF0(1);
        enemy->node.field_4   = 1;
        height                = work->walker.field_5E;
        walker                = &work->walker;
        work->walker.field_5A = 0x100;
        walker->field_5C      = 0xA0;
        walker->field_60      = 6;
        walker->field_5E      = height;
        work->walker.state    = 1;
        work->field_196      |= 0x8000;
        work->field_12E      |= 0x8000;
        work->field_190       = Gp_PackObjPair((GpObj50*)enemy, 0);
        enemy->node.field_4   = 0;
        work->field_100       = 2;
        work->field_104       = 2;
        work->field_108       = 0x50;
    }
    if (work->field_1F8 > 0) {
        work->field_1F8 -= 0x3C;
        ((TmdObject*)task->extra)->field_8->coord.t[1] =
            work->field_1FA + work->field_1F8;
        ((TmdObject*)task->extra)->field_8->flg = 0;
    }
    if (work->walker.scale < 0x1000) {
        bridge_scale_up(work);
    }
    func_acropolis_bridge_8018532C(&work->walker);
    func_acropolis_bridge_8018581C(task);
    if (((AcropolisBridgeEnemyWork*)task->idMap)->field_19C == 0) {
        done = 0;
        SOFT_BARRIER();
    } else {
        done = 1;
    }
    if (done != 0) {
        work->field_0 = 3;
    }
    if (cfg->field_4->t[1] < 0x321) {
        work->field_0 = 1;
    }
}

/// Runs the bridge enemy's retreat state. On the first frame (work block still
/// live) it parks the walker in step 3, clears the hand-over flag, drops bit 15
/// of `field_196`, rebuilds the scale matrix and restarts the animation slots
/// on animation 1. Every frame after that it raises the model root by 0x2D
/// until the entry offset at `field_1F8` reaches 0x708, shrinks the walker by
/// 0x46 a frame down to 0x500 -- rebuilding the scale matrix as it goes, and
/// once it is that small tagging the enemy's link node instead -- then ticks
/// the walker and the animation slots. When the hand-over flag is set the
/// behaviour state becomes 1 while the player is below the bridge and 2
/// otherwise.
void func_acropolis_bridge_801863A8(Task* task)
{
    AcropolisBridgeEnemyWork*  work;
    AcropolisBridgeWalkerWork* walker;
    GpEnemy*                   enemy;
    WipSysConfig*              cfg;

    cfg   = &Wip_SysConfig;
    work  = (AcropolisBridgeEnemyWork*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work->field_4 != 0) {
        work->walker.state    = 3;
        work->walker.field_91 = 0;
        work->field_196      &= 0x7FFF;
        bridge_reset_scale_mtx_entry(work);
        work->walker.field_5A = 0x200;
        walker                = &work->walker;
        walker->field_5C      = 0x20;
        walker->field_5E      = 0x80;
        walker->field_60      = 3;
        work->field_100       = 2;
        work->field_104       = 1;
        work->field_108       = 0x10;
    }
    if (work->field_1F8 < 0x708) {
        work->field_1F8 += 0x2D;
        ((TmdObject*)task->extra)->field_8->coord.t[1] =
            work->field_1FA + work->field_1F8;
        ((TmdObject*)task->extra)->field_8->flg = 0;
    }
    if (work->walker.scale >= 0x500) {
        work->walker.scale -= 0x46;
        bridge_reset_scale_mtx_shrink(work);
    } else if (enemy->node.field_4 == 0) {
        enemy->node.field_4 = 1;
    }
    func_acropolis_bridge_8018532C(&work->walker);
    func_acropolis_bridge_8018581C(task);
    if (work->walker.field_91 != 0) {
        if (cfg->field_4->t[1] < 0x321) {
            work->field_0 = 1;
        } else {
            work->field_0 = 2;
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80186618);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80186BBC);

/// Reports whether the bridge enemy is standing on a kind-1 surface: the first
/// three collision records are scanned in order and the scan stops at the first
/// empty one, so an occupied record whose `field_4` high halfword is 1 has to
/// come before any gap in the table.
static __inline__ s32 bridge_rec_kind1(GpRec18* recs)
{
    s16 i;

    for (i = 0; i < 3; i++) {
        if (recs[i].field_4 == 0) {
            return 0;
        }
        if ((recs[i].field_4 & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Runs the bridge enemy's fall. On the first frame (work block still live) it
/// clears bit 15 of `field_196` and sets it in `field_12E`, tags the link node,
/// seeds the three behaviour parameters and gives the model root coordinate a
/// random yaw from the shared LCG. Every frame after that it eases the entry
/// offset at `field_1F8` back to zero three units at a time, sets the model
/// root height from it and adds an `rsin` bob driven by the frame counter. Once
/// the enemy is resting on a kind-1 surface it also drifts the model root
/// towards the camera position by a sixteenth of the normalized direction,
/// yawing the root by 0x10 first.
void func_acropolis_bridge_80187078(Task* task)
{
    AcropolisBridgeEnemyWork* work;
    GpEnemy*                  enemy;
    GsCOORDINATE2*            coord;
    SVECTOR                   dir;
    SVECTOR*                  d;

    work  = (AcropolisBridgeEnemyWork*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work->field_4 != 0) {
        work->field_196    &= 0x7FFF;
        work->field_12E    |= 0x8000;
        enemy->node.field_4 = 1;
        work->field_100     = 1;
        work->field_104     = 3;
        work->field_108     = 0x10;
        Gp_LcgState         = Gp_LcgState * 5 + 0x71357911;
        Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, (u32)Gp_LcgState >> 16, 1);
        ((TmdObject*)task->extra)->field_8->flg = 0;
    }
    if (work->field_1F8 > 0) {
        work->field_1F8 -= 3;
    }
    ((TmdObject*)task->extra)->field_8->coord.t[1] = work->field_1FA + work->field_1F8;
    ((TmdObject*)task->extra)->field_8->coord.t[1] +=
        rsin((D_80070F70 << 5) + ((TmdObject*)task->extra)->field_8->coord.t[0]) >> 6;
    if (bridge_rec_kind1(work->recs) != 0) {
        coord  = ((TmdObject*)task->extra)->field_8;
        dir.vx = D_80073B8C->t[0] - coord->coord.t[0];
        d      = &dir;
        d->vy  = D_80073B8C->t[1] - coord->coord.t[1];
        d->vz  = D_80073B8C->t[2] - coord->coord.t[2];
        Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, 0x10, 0);
        VectorNormalSS(d, d);
        gte_lddp(-0x10);
        gte_ldsv(d);
        gte_gpf12_real();
        gte_stsv(d);
        ((TmdObject*)task->extra)->field_8->coord.t[0] += dir.vx;
        ((TmdObject*)task->extra)->field_8->coord.t[2] += dir.vz;
    }
    ((TmdObject*)task->extra)->field_8->flg = 0;
    func_acropolis_bridge_8018581C(task);
}

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
