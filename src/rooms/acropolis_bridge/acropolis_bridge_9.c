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
/// State handler table the per-frame tick dispatches through on
/// `AcropolisBridgeEnemyWork::field_0`.
extern void    (*D_acropolis_bridge_8019175C[])(Task*);
extern u8      D_801153F4;
extern u16     D_801153F6;
extern s32     D_80070F70;
extern MATRIX* D_80073B8C;
extern s32     Gp_LcgState;

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
    /* 0x6 */ u8   arrived;
} AcropolisBridgeNavRoute;

/// Work block of the walker task this unit's second half drives (the task
/// `func_acropolis_bridge_8018532C` ticks). `node` is the patrol node it is
/// currently heading for and `field_62` / `field_64` the movement deltas that
/// are cleared whenever it arrives. `scaleMtx` is the model matrix the spawn
/// scale-up in `func_acropolis_bridge_801861A0` rebuilds every frame from
/// `scale`, which ramps to 0x1000, and `state` is the step
/// `func_acropolis_bridge_8018532C` dispatches on. `nav` and `route` point at
/// the `navData` / `routeData` copies embedded further down the same block,
/// which `func_acropolis_bridge_80185988` seeds; that is why the retreat state
/// in `func_acropolis_bridge_801863A8` reaches the route cursor and its
/// arrived flag as plain walker fields.
typedef struct AcropolisBridgeWalkerWork {
    /* 0x00 */ AcropolisBridgeNavData*  nav;
    /* 0x04 */ AcropolisBridgeNavRoute* route;
    /* 0x08 */ GsCOORDINATE2*           coord;
    /* 0x0C */ s32                      field_C;
    /* 0x10 */ GpRec18*                 recs;
    /* 0x14 */ byte                     pad_14[0x20];
    /* 0x34 */ MATRIX                   scaleMtx;
    /* 0x54 */ s16                      scale;
    /* 0x56 */ s16                      field_56;
    /* 0x58 */ s16                      field_58;
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
    /* 0x6B */ u8                       field_6B;
    /* 0x6C */ u8                       field_6C;
    /* 0x6D */ byte                     pad_6D[0x1];
    /* 0x6E */ u8                       field_6E;
    /* 0x6F */ byte                     pad_6F[0x11];
    /* 0x80 */ AcropolisBridgeNavData   navData;
    /* 0x88 */ u8                       field_88;
    /* 0x89 */ u8                       field_89;
    /* 0x8A */ byte                     pad_8A[0x2];
    /* 0x8C */ AcropolisBridgeNavRoute  routeData;
} AcropolisBridgeWalkerWork;
STATIC_ASSERT_SIZEOF(AcropolisBridgeWalkerWork, 0x94);

/// Work block the bridge enemy's task keeps at `Task::idMap`. `field_4` is the
/// live flag every state handler in this unit gates on. `body` and `hit` are
/// the two linked `GpObj`s -- kind 2 for the model and kind 3 for the hit box
/// -- whose `flags` bit 15 the handlers toggle to enable one and disable the
/// other, and `recs` / `hitRecs` are their collision record tables.
/// `lightMtx` / `colorMtx` are the matrices the model's `TmdObject` is pointed
/// at, `field_1F0` is the `GpEffArg` the death effect is spawned with and
/// `field_290` the death-sequence frame counter.
typedef struct AcropolisBridgeEnemyWork {
    /* 0x000 */ s16                       field_0;
    /* 0x002 */ s16                       field_2;
    /* 0x004 */ s16                       field_4;
    /* 0x006 */ byte                      pad_6[0x2];
    /* 0x008 */ s16                       yaw;
    /* 0x00A */ byte                      pad_A[0x2];
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
    /* 0x10E */ s16                       field_10E;
    /* 0x110 */ GpObj                     body;
    /* 0x130 */ GpRec18                   recs[3];
    /* 0x178 */ GpObj                     hit;
    /* 0x198 */ GpRec18                   hitRecs[1];
    /* 0x1B0 */ MATRIX                    lightMtx;
    /* 0x1D0 */ MATRIX                    colorMtx;
    /* 0x1F0 */ GpEffArg                  field_1F0;
    /* 0x1F8 */ s16                       field_1F8;
    /* 0x1FA */ s16                       field_1FA;
    /* 0x1FC */ AcropolisBridgeWalkerWork walker;
    /* 0x290 */ u16                       field_290;
    /* 0x292 */ u16                       field_292;
} AcropolisBridgeEnemyWork;
STATIC_ASSERT_SIZEOF(AcropolisBridgeEnemyWork, 0x294);

/// 0xC-byte scratchpad block the per-frame tick carves off `G_SCRATCH_HEAD` to
/// stage the collision record it hands to the damage path: the record's three
/// packed coordinates followed by its `field_4` attack id, which is also the
/// "was there a hit" flag.
typedef struct AcropolisBridgeHitScratch {
    /* 0x0 */ s16  x;
    /* 0x2 */ s16  y;
    /* 0x4 */ s16  z;
    /* 0x6 */ byte pad_6[0x2];
    /* 0x8 */ s32  hit;
} AcropolisBridgeHitScratch;
STATIC_ASSERT_SIZEOF(AcropolisBridgeHitScratch, 0xC);

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

extern u8                     D_8007218A;
extern GpPairSrcE             D_acropolis_bridge_80190C5C;
extern s32                    D_acropolis_bridge_801915C8;
extern AcropolisBridgeNavNode D_acropolis_bridge_8019162C[];
extern u8                     D_acropolis_bridge_801916CC[];
extern u8*                    D_acropolis_bridge_80191720[];
extern s32                    D_acropolis_bridge_80191744;

/// Copies a scratch `SVECTOR3` onto a `GpObj`'s three position halfwords.
static __inline__ void bridge_set_obj_pos(GpObj* obj, SVECTOR3* pos)
{
    obj->field_10 = pos->vx;
    obj->field_12 = pos->vy;
    obj->field_14 = pos->vz;
}

/// One-time setup for the bridge enemy: allocates the 0x294-byte work block,
/// points the model's light and colour matrices at it, hangs the enemy off the
/// room's stat block, starts the animation context on three slots, and links
/// the model and hit-box `GpObj`s (kinds 2 and 3) onto the part coordinates at
/// `field_8[3]` and `field_8[1]`. The walker half is seeded next: its patrol
/// tables are the copies embedded in the walker itself, the route is the entry
/// `D_acropolis_bridge_80191720` holds for this spawn variant, and the scale
/// matrix is rebuilt from `scale` through a `VECTOR` borrowed from the scratch
/// arena -- the same block is then reused for the world position handed to
/// `func_800D7A9C` before it is released. `field_1F8` is the 0x5DC entry
/// offset the model root is raised by, remembered in `field_1FA`. In the
/// third visit (`Game_Session->field_5 == 2`) the three known variants start
/// in state 8 at a fixed position instead of state 1.
void func_acropolis_bridge_80185988(GpEnemy* enemy, Task* task)
{
    TmdObject*                 obj;
    TmdObject*                 obj2;
    GsCOORDINATE2*             coord;
    GsCOORDINATE2*             coord2;
    GsCOORDINATE2*             coord3;
    AcropolisBridgeEnemyWork*  work;
    AcropolisBridgeWalkerWork* walker;
    GpObj*                     link;
    GpObj*                     link2;
    register u8*               head;
    register u8*               head2;
    register u8*               head3;
    VECTOR*                    scale;
    VECTOR*                    vec;
    s32                        amount;
    s32                        variant;
    s32                        step;
    u16                        hp;
    s32                        axisY;
    SVECTOR3                   pos;

    obj         = (TmdObject*)task->extra;
    coord       = obj->field_8;
    work        = (AcropolisBridgeEnemyWork*)Mem_Calloc(sizeof(AcropolisBridgeEnemyWork), 0);
    task->idMap = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    obj2            = (TmdObject*)task->extra;
    obj2->field_1C  = &work->lightMtx;
    obj2->field_20  = &work->colorMtx;
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    enemy->field_50 = &D_acropolis_bridge_80190C5C;
    hp              = D_acropolis_bridge_80190C5C.field_4;
    enemy->field_54 = (s32)work->recs;
    enemy->field_40 = hp;
    work->field_10C = 1;
    work->field_10E = 1;
    enemy->field_42 = work->field_10C;
    enemy->field_40 = enemy->field_42;
    func_800B3F84(&work->anim, &D_acropolis_bridge_801915C8, (GpAnimObj*)obj, work->pad_C0,
                  work->slots);
    work->field_108 = 0x10;
    link            = &work->body;
    link->field_8   = &((TmdObject*)task->extra)->field_8[3];
    link->field_C   = work->recs;
    link->field_10  = 0;
    link->field_12  = 0;
    link->field_14  = 0;
    link->field_18  = 0x30029;
    link->field_1C  = 0x100;
    link->flags     = 1;
    Gp_LinkObj(2, link);
    link->flags |= 0x8000;
    Gp_InitRec18Table(link->field_C, 3, 0);
    pos.vx         = 0;
    pos.vy         = 0;
    pos.vz         = 0;
    link2          = &work->hit;
    link2->field_8 = &((TmdObject*)task->extra)->field_8[1];
    link2->field_C = work->hitRecs;
    bridge_set_obj_pos(link2, &pos);
    link2->field_1C = 0x100;
    link2->flags    = 1;
    Gp_LinkObj(3, link2);
    Gp_InitRec18Table(link2->field_C, 1, 0);
    work->hit.field_18 = Gp_PackObjPair((GpObj50*)enemy, 0);
    coord->sub         = &Gfx_ViewCoord;
    work->yaw          = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    work->field_100    = 2;
    work->field_104    = 2;
    func_acropolis_bridge_8018581C(task);
    enemy->field_18    = &((TmdObject*)task->extra)->field_8[3];
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4 = 1;
    task->field_24      = &D_acropolis_bridge_80191744;
    work->field_2       = -1;
    work->field_0       = 1;
    work->field_1F8     = 0x5DC;
    work->field_1FA     = coord->coord.t[1];
    coord->coord.t[1]  += work->field_1F8;

    work->walker.navData.nodes    = D_acropolis_bridge_8019162C;
    work->walker.field_88         = 0xA;
    work->walker.navData.field_4  = D_acropolis_bridge_801916CC;
    work->walker.field_89         = 0xA;
    work->walker.routeData.nodes  = D_acropolis_bridge_80191720[enemy->field_8 >> 12];
    work->walker.nav              = &work->walker.navData;
    work->walker.routeData.cursor = 0;
    work->walker.route            = &work->walker.routeData;
    coord2                        = ((TmdObject*)task->extra)->field_8;
    work->walker.field_58         = 3;
    walker                        = &work->walker;
    work->walker.field_C          = 0;
    work->walker.recs             = work->recs;
    work->walker.scale            = 0x1000;
    work->walker.field_56         = 0;
    work->walker.field_5A         = 0x30;
    work->walker.coord            = coord2;
    walker->field_5C              = 0x30;
    walker->field_5E              = 0;
    walker->field_60              = 1;
    step                          = 3;
    work->walker.state            = step;
    work->walker.field_6B         = 1;
    work->walker.field_6C         = 1;
    work->walker.field_6E         = D_8007218A;

    __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
    head                      = *(u8**)(head + 0x3FC);
    amount                    = walker->scale;
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
    scale                     = (VECTOR*)(head - 0x10);
    *(VECTOR**)G_SCRATCH_HEAD = scale;
    if (amount != 0 && amount != 0x1000) {
        scale->vz                    = amount;
        scale->vy                    = amount;
        ((VECTOR*)(head - 0x10))->vx = amount;
        ScaleMatrix(&work->walker.scaleMtx, scale);
    }
    ((TmdObject*)task->extra)->field_8->flg = 0;
    coord3                                  = ((TmdObject*)task->extra)->field_8;
    __asm__("lui %0, 0x1F80" : "=r"(head2) : "r"(coord3));
    head2 = *(u8**)(head2 + 0x3FC);
    vec   = (VECTOR*)head2;
    Gp_UpdateCoord(coord3);
    vec->vx = ((TmdObject*)task->extra)->field_8->workm.t[0];
    vec->vy = ((TmdObject*)task->extra)->field_8->workm.t[1];
    vec->vz = ((TmdObject*)task->extra)->field_8->workm.t[2];
    func_800D7A9C((TmdObject*)task->extra, vec, 0, 3);
    __asm__ volatile("lui %0, 0x1F80" : "=r"(head3));
    head3                 = *(u8**)(head3 + 0x3FC);
    *(u8**)G_SCRATCH_HEAD = head3 + 0x10;
    axisY                 = 1;
    if (D_801153F6 < 3) {
        ((void (*)(s32))Gp_IncStateF0Ref)(0);
    }
    if (Game_Session->field_5 == 2) {
        variant = enemy->field_8 >> 12;
        switch (variant) {
            case 0:
                work->field_0                                  = 8;
                ((TmdObject*)task->extra)->field_8->coord.t[0] = -0x22C4;
                ((TmdObject*)task->extra)->field_8->coord.t[1] = -0x3E8;
                ((TmdObject*)task->extra)->field_8->coord.t[2] = -0x640;
                break;
            case 1:
                work->field_0                                      = 8;
                ((TmdObject*)task->extra)->field_8->coord.t[0]     = -0x270F;
                ((TmdObject*)task->extra)->field_8->coord.t[axisY] = -0x3E8;
                ((TmdObject*)task->extra)->field_8->coord.t[2]     = -0x7D0;
                break;
            case 2:
                work->field_0                                  = 8;
                ((TmdObject*)task->extra)->field_8->coord.t[0] = -0x2EE0;
                ((TmdObject*)task->extra)->field_8->coord.t[1] = -0x3E8;
                ((TmdObject*)task->extra)->field_8->coord.t[2] = -0x5DC;
                break;
            default:
                work->field_0 = 0;
                break;
        }
    }
    task->state++;
}

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
        work->walker.state            = 3;
        work->walker.routeData.cursor = 0;
        work->hit.flags              &= 0x7FFF;
        work->body.flags             |= 0x8000;
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
    if (work->walker.routeData.arrived == 1) {
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
        work->hit.flags      |= 0x8000;
        work->body.flags     |= 0x8000;
        work->hit.field_18    = Gp_PackObjPair((GpObj50*)enemy, 0);
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
    if (((AcropolisBridgeEnemyWork*)task->idMap)->hitRecs[0].field_4 == 0) {
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
/// of the hit box's flags, rebuilds the scale matrix and restarts the animation slots
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
        work->walker.state            = 3;
        work->walker.routeData.cursor = 0;
        work->hit.flags              &= 0x7FFF;
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
    if (work->walker.routeData.cursor != 0) {
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
/// clears bit 15 of the hit box's flags and sets it in the model's, tags the link node,
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
        work->hit.flags    &= 0x7FFF;
        work->body.flags   |= 0x8000;
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
        work->hit.flags    &= 0x7FFF;
        work->body.flags   &= 0x7FFF;
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
        work->hit.flags    &= 0x7FFF;
        work->body.flags   &= 0x7FFF;
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

/// Ticks the bridge enemy once per frame. It refreshes the model's root
/// coordinate and relights it, then branches on the global pause mode
/// `D_801153F4`: mode 1 only releases the collision records, mode 2 also hides
/// the mesh, and mode 0 keeps the model's visibility in step with the camera
/// -- re-allocating or releasing the TMD's aux buffers when the view changes,
/// and remembering the view it last synced to in `field_292`. Outside the
/// death and cleanup states it then borrows a 0xC-byte scratch block, scans
/// the body's three collision records for a hit (high halfword 0x2), applies
/// it through `func_acropolis_bridge_801876A8`, raises `field_4` on the frame
/// the behaviour state changes, runs the state's handler from
/// `D_acropolis_bridge_8019175C`, clears both record tables and -- while no
/// `Gp_StateF0` request is pending -- resets any state other than 5 or 6 back
/// to 0.
void func_acropolis_bridge_80187850(GpEnemy* enemy, Task* task)
{
    AcropolisBridgeEnemyWork*  work;
    AcropolisBridgeEnemyWork*  cur;
    AcropolisBridgeHitScratch* block;
    TmdObject*                 extra;
    GpRec18*                   recs;
    VECTOR                     pos;
    s32                        mode;
    s32                        view;
    s32                        hit;
    u16                        state;
    s16                        i;

    work                                    = (AcropolisBridgeEnemyWork*)task->idMap;
    ((TmdObject*)task->extra)->field_8->flg = 0;
    Gp_UpdateCoord(((TmdObject*)task->extra)->field_8);
    pos.vx = ((TmdObject*)task->extra)->field_8->workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->field_8->workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->field_8->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    mode = D_801153F4;
    if (mode == 1) {
        goto paused;
    }
    if (mode >= 2) {
        goto ge2;
    }
    if (mode == 0) {
        goto running;
    }
    goto body;
ge2:
    if (mode == 2) {
        goto hidden;
    }
    goto body;

running:
    state = (u16)work->field_0;
    if ((u32)(state - 6) >= 2U) {
        if (state != 0) {
            view = Gp_GetViewIndex() & 0xFF;
            switch (view) {
                case 8:
                    if ((s32)work->field_292 == view) {
                        goto drop;
                    }
                    ((TmdObject*)task->extra)->field_C = 0x80;
                    goto resync;
                case 22:
                    if (work->field_0 == 4) {
                        goto draw;
                    }
                drop:
                    ((TmdObject*)task->extra)->field_C |= 4;
                    extra                               = (TmdObject*)task->extra;
                    if (extra->field_18 != NULL) {
                        Tmd_FreeBuffers(extra);
                    }
                    goto resync;
                default:
                    Tmd_AllocBuffers((TmdObject*)task->extra);
                draw:
                    ((TmdObject*)task->extra)->field_C = 0;
                    break;
            }
        resync:
            work->field_292 = Gp_GetViewIndex() & 0xFF;
        }
    }
    goto body;

paused:
    state = (u16)work->field_0;
    if ((u32)(state - 6) >= 2U && state != 0) {
        Gp_ClearRec18Occupied(&work->recs[0]);
        Gp_ClearRec18Occupied(&work->hitRecs[0]);
    }
    return;

hidden:
    ((TmdObject*)task->extra)->field_C = 0x80;
    Gp_ClearRec18Occupied(&work->recs[0]);
    Gp_ClearRec18Occupied(&work->hitRecs[0]);
    return;

body:
    *(u8**)G_SCRATCH_HEAD -= 0xC;
    block                  = (AcropolisBridgeHitScratch*)*(u8**)G_SCRATCH_HEAD;
    recs                   = work->recs;
    i                      = 0;
    do {
        if (recs[i].field_4 == 0) {
            goto missed;
        }
        if ((recs[i].field_4 & 0xFFFF0000) == 0x20000) {
            block->x = recs[i].field_8;
            block->y = recs[i].field_A;
            block->z = recs[i].field_C;
            hit      = recs[i].field_4;
            goto hitTaken;
        }
        i++;
    } while (i < 3);
missed:
    hit = 0;
hitTaken:
    block->hit = hit;
    if (hit != 0) {
        func_acropolis_bridge_801876A8(task, hit);
    }

    cur = (AcropolisBridgeEnemyWork*)task->idMap;
    if (cur->field_2 != cur->field_0) {
        cur->field_4 = 1;
    } else {
        cur->field_4 = 0;
    }
    cur->field_2 = cur->field_0;
    D_acropolis_bridge_8019175C[work->field_0](task);
    Gp_ClearRec18Occupied(&work->recs[0]);
    Gp_ClearRec18Occupied(&work->hitRecs[0]);
    if (D_801153F6 == 0) {
        if ((u32)((u16)work->field_0 - 5) >= 2U) {
            work->field_0 = 0;
        }
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0xC;
}

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
        work->body.flags   &= 0x7FFF;
        work->hit.flags    &= 0x7FFF;
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
