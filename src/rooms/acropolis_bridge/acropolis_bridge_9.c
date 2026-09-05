#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "rooms/acropolis_bridge.h"

extern s16                   D_acropolis_bridge_801915E4[][6];
extern GpEnemyTaskFuncTable3 D_acropolis_bridge_8017D6E8;
extern u16                   D_acropolis_bridge_80190C60;
/// State handler table the per-frame tick dispatches through on
/// `AcropolisBridgeEnemyWork::field_0`.
extern void (*D_acropolis_bridge_8019175C[])(Task*);
extern u8   D_80072729;
/// Table of 0x80-byte actor config blocks; `Wip_SysConfig` is entry 1.
extern WipSysConfig D_80073B08[];
extern u8           D_801153F4;
extern u16          D_801153F6;
extern s32          D_80070F70;
extern MATRIX*      D_80073B8C;
extern s32          Gp_LcgState;

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_800FDB18(s32 arg0, GsCOORDINATE2* arg1, SVECTOR* arg2, GpEffArg* arg3);
void func_acropolis_bridge_8018581C(Task* task);
s16  func_acropolis_bridge_8017E024(void);

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
/// own `cursor`.
typedef struct AcropolisBridgeNavData {
    /* 0x0 */ AcropolisBridgeNavNode* nodes;
    /* 0x4 */ u8*                     field_4;
    /* 0x8 */ u8                      count;
    /* 0x9 */ u8                      field_9;
    /* 0xA */ byte                    pad_A[0x2];
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
    /* 0x14 */ byte                     pad_14[0x8];
    /* 0x1C */ SVECTOR                  moveStep;
    /* 0x24 */ SVECTOR                  moveDelta;
    /* 0x2C */ byte                     pad_2C[0x8];
    /* 0x34 */ MATRIX                   scaleMtx;
    /* 0x54 */ s16                      scale;
    /* 0x56 */ s16                      field_56;
    /* 0x58 */ s16                      field_58;
    /* 0x5A */ s16                      field_5A;
    /* 0x5C */ u16                      field_5C;
    /* 0x5E */ u16                      field_5E;
    /* 0x60 */ u16                      field_60;
    /* 0x62 */ s16                      field_62;
    /* 0x64 */ s16                      field_64;
    /* 0x66 */ byte                     pad_66[0x2];
    /* 0x68 */ u8                       state;
    /* 0x69 */ u8                       field_69;
    /* 0x6A */ u8                       node;
    /* 0x6B */ u8                       field_6B;
    /* 0x6C */ u8                       field_6C;
    /* 0x6D */ u8                       field_6D;
    /* 0x6E */ u8                       field_6E;
    /* 0x6F */ u8                       field_6F;
    /* 0x70 */ u8                       field_70;
    /* 0x71 */ u8                       field_71;
    /* 0x72 */ u8                       field_72;
    /* 0x73 */ u8                       field_73;
    /* 0x74 */ byte                     pad_74[0x2];
    /* 0x76 */ u8                       cursor;
    /* 0x77 */ byte                     pad_77[0x1];
    /* 0x78 */ u8                       moving;
    /* 0x79 */ byte                     pad_79[0x7];
    /* 0x80 */ AcropolisBridgeNavData   navData;
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
    /* 0x106 */ s16                       field_106;
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

/// 0x28-byte scratchpad block the walker tick carves off `G_SCRATCH_HEAD`.
/// Only the `SVECTOR3` at +4 is used here: every state writes the position the
/// walker is steering for into it and hands it to
/// `func_acropolis_bridge_80185104`.
typedef struct AcropolisBridgeWalkScratch {
    /* 0x00 */ s32      field_0;
    /* 0x04 */ SVECTOR3 pos;
    /* 0x0A */ byte     pad_A[0x1E];
} AcropolisBridgeWalkScratch;
STATIC_ASSERT_SIZEOF(AcropolisBridgeWalkScratch, 0x28);

/// 0x14-byte scratchpad block `func_acropolis_bridge_8018450C` carves off
/// `G_SCRATCH_HEAD` to pick the patrol node nearest the walker. `dx` / `dz` are
/// the axis deltas for the node under test and `dist` their squared sum, which
/// is compared against the running `best` -- initialised to `-1` so the first
/// node always wins -- and `nearest` is the winning node's index.
typedef struct AcropolisBridgeNearScratch {
    /* 0x00 */ s16  dx;
    /* 0x02 */ byte pad_2[0x2];
    /* 0x04 */ s16  dz;
    /* 0x06 */ byte pad_6[0x2];
    /* 0x08 */ u32  best;
    /* 0x0C */ u32  dist;
    /* 0x10 */ u8   node;
    /* 0x11 */ u8   nearest;
    /* 0x12 */ byte pad_12[0x2];
} AcropolisBridgeNearScratch;
STATIC_ASSERT_SIZEOF(AcropolisBridgeNearScratch, 0x14);

/// Ticks the walker task: steps its patrol route and drives its animation.
void func_acropolis_bridge_8018532C(AcropolisBridgeWalkerWork* walker);

/// Returns the patrol node nearest the given actor's coordinate, by squared
/// distance in the XZ plane.
u8 func_acropolis_bridge_801843A0(AcropolisBridgeWalkerWork* work, s32 actor);
/// Returns the patrol node nearest the walker, by squared distance in the
/// XZ plane between the node table and the walker's coordinate translation.
u8   func_acropolis_bridge_8018450C(AcropolisBridgeWalkerWork* work);
void func_acropolis_bridge_80184638(AcropolisBridgeWalkerWork* work, s32 arg1);
void func_acropolis_bridge_80184908(AcropolisBridgeWalkerWork* work);
void func_acropolis_bridge_80184B94(AcropolisBridgeWalkerWork* work);
void func_acropolis_bridge_80185104(AcropolisBridgeWalkerWork* work, SVECTOR3* pos);

/// 8-byte scratch block the arrival test carves off `G_SCRATCH_HEAD` to stage
/// the delta between the patrol node the walker is heading for and the walker
/// itself. The node coordinates are copied over as raw halfwords and then have
/// the coordinate translation subtracted from them in place; `y` is flattened
/// to zero because the test only measures in the XZ plane.
typedef struct AcropolisBridgeDeltaScratch {
    /* 0x0 */ u16  x;
    /* 0x2 */ u16  y;
    /* 0x4 */ u16  z;
    /* 0x6 */ byte pad_6[0x2];
} AcropolisBridgeDeltaScratch;
STATIC_ASSERT_SIZEOF(AcropolisBridgeDeltaScratch, 0x8);

/// 0xC-byte scratch block the range test below squares its three operands in,
/// nested inside the delta block its caller already holds.
typedef struct AcropolisBridgeRangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} AcropolisBridgeRangeScratch;
STATIC_ASSERT_SIZEOF(AcropolisBridgeRangeScratch, 0xC);

/// Reports whether the XZ delta staged in `d` is at least `r` long, squaring
/// both sides in a 0xC-byte scratch block of its own so no comparison is done
/// on a square root.
static __inline__ s32 acropolisBridgeOutOfRange(AcropolisBridgeDeltaScratch* d, s16 r)
{
    AcropolisBridgeRangeScratch* b;
    u8*                          head;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0xC;
    b                     = (AcropolisBridgeRangeScratch*)*(u8**)G_SCRATCH_HEAD;

    b->dx                 = (s16)d->x;
    b->dz                 = (s16)d->z;
    b->r                  = r;
    b->dx                 = b->dx * b->dx;
    b->dz                 = b->dz * b->dz;
    b->r                  = b->r * b->r;
    *(u8**)G_SCRATCH_HEAD = head;
    return b->dx + b->dz >= b->r;
}

/// Reports whether the walker has reached the patrol node at `work->node`. It
/// stages the XZ delta between the node and the walker's coordinate
/// translation in an 8-byte scratch block, then accepts the node if the walker
/// is inside either of two radii: its own `field_5C * 4`, or a flat 300.
s16 func_acropolis_bridge_80184024(AcropolisBridgeWalkerWork* work)
{
    AcropolisBridgeDeltaScratch* d;
    u8*                          head;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x8;
    d                     = (AcropolisBridgeDeltaScratch*)(head - 0x8);

    d->x = work->nav->nodes[work->node].x;
    d->y = work->nav->nodes[work->node].y;
    d->z = work->nav->nodes[work->node].z;
    d->x = d->x - *(u16*)&work->coord->coord.t[0];
    d->y = 0;
    d->z = d->z - *(u16*)&work->coord->coord.t[2];

    if (!acropolisBridgeOutOfRange(d, work->field_5C * 4) ||
        !acropolisBridgeOutOfRange(d, 300)) {
        *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x8;
        return 1;
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x8;
    return 0;
}

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

/// 0x18-byte scratch block the actor-relative nearest-node scan carves off
/// `G_SCRATCH_HEAD`. `cfg` is the actor config entry whose coordinate the
/// scan measures from, the three deltas are the per-axis distance to the node
/// under test -- `dy` is staged but never enters the distance -- and `dist`
/// is the squared XZ distance compared against the running `best`, which
/// starts at `-1` so the first node always wins.
typedef struct AcropolisBridgeNearCfgScratch {
    /* 0x00 */ s16           dx;
    /* 0x02 */ s16           dy;
    /* 0x04 */ s16           dz;
    /* 0x06 */ byte          pad_6[0x2];
    /* 0x08 */ WipSysConfig* cfg;
    /* 0x0C */ u32           best;
    /* 0x10 */ u32           dist;
    /* 0x14 */ u8            node;
    /* 0x15 */ u8            nearest;
    /* 0x16 */ byte          pad_16[0x2];
} AcropolisBridgeNearCfgScratch;
STATIC_ASSERT_SIZEOF(AcropolisBridgeNearCfgScratch, 0x18);

/// Scans the room's patrol node table for the node nearest actor `actor` and
/// returns its index. Same scan as `func_acropolis_bridge_8018450C`, but
/// measured from the translation of the actor config's matrix rather than
/// from the walker's own coordinate; the walker uses it with the player
/// (entry 1) to pick the node it retreats to.
u8 func_acropolis_bridge_801843A0(AcropolisBridgeWalkerWork* work, s32 actor)
{
    AcropolisBridgeNearCfgScratch* block;
    u8*                            head;
    s16                            dz;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x18;
    block                 = (AcropolisBridgeNearCfgScratch*)*(u8**)G_SCRATCH_HEAD;

    block->cfg  = &D_80073B08[(s16)actor];
    block->best = -1;
    for (block->node = 0; block->node < work->nav->count; block->node++) {
        block->dx   = *(u16*)&block->cfg->field_4->t[0] - work->nav->nodes[block->node].x;
        block->dy   = *(u16*)&block->cfg->field_4->t[1] - work->nav->nodes[block->node].y;
        dz          = *(u16*)&block->cfg->field_4->t[2] - work->nav->nodes[block->node].z;
        block->dz   = dz;
        block->dist = block->dx * block->dx + dz * dz;
        if (block->dist < block->best || block->best == -1) {
            block->best    = block->dist;
            block->nearest = block->node;
        }
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x18;
    return block->nearest;
}

/// Scans the room's patrol node table for the node nearest the walker and
/// returns its index. Distance is the squared XZ distance between the node and
/// the low halfword of the walker coordinate's translation, staged in a 0x14
/// byte scratch block along with the cursor and the running best.
u8 func_acropolis_bridge_8018450C(AcropolisBridgeWalkerWork* work)
{
    AcropolisBridgeNearScratch* block;
    u8*                         head;
    s16                         dz;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x14;
    block                 = (AcropolisBridgeNearScratch*)*(u8**)G_SCRATCH_HEAD;

    block->best = -1;
    for (block->node = 0; block->node < work->nav->count; block->node++) {
        block->dx   = *(u16*)&work->coord->coord.t[0] - work->nav->nodes[block->node].x;
        dz          = *(u16*)&work->coord->coord.t[2] - work->nav->nodes[block->node].z;
        block->dz   = dz;
        block->dist = block->dx * block->dx + dz * dz;
        if (block->dist < block->best || block->best == -1) {
            block->best    = block->dist;
            block->nearest = block->node;
        }
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x14;
    return block->nearest;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184638);

/// 0x18-byte scratch the walker's per-frame move carves off `G_SCRATCH_HEAD`:
/// the `GpDeltaScratch` `func_800E0C10` fills with the 16.16 step toward the
/// current patrol node, followed by the whole-unit step actually applied to
/// the walker's coordinate this frame.
typedef struct AcropolisBridgeMoveScratch {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ SVECTOR        move;
} AcropolisBridgeMoveScratch;
STATIC_ASSERT_SIZEOF(AcropolisBridgeMoveScratch, 0x18);

/// Steps the walker toward its current patrol node. `func_800E0C10` produces
/// the 16.16 delta; the high half of each component becomes the whole-unit
/// step, rounded away from zero whenever a fraction is left over. While
/// `field_6B` is set the walker is pinned vertically, otherwise Y also carries
/// a constant 0x10 fall. Y is applied in three bands: a +8 hop above 0x20, a
/// -0x20 drop below -0x20, and the plain step in between. `moving` records
/// whether the frame produced any XZ motion at all.
void func_acropolis_bridge_80184908(AcropolisBridgeWalkerWork* work)
{
    u8*                         head;
    AcropolisBridgeMoveScratch* s;
    s32                         valx;
    s32                         valy;
    s32                         valz;
    s32                         dx;
    s32                         dy;
    s32                         dz;
    s32                         y;
    s32                         mag;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x18;
    s                     = (AcropolisBridgeMoveScratch*)(head - 0x18);
    if (func_800E0C10((GpRec18*)work->field_C, &s->delta, work->field_56, NULL) != 0) {
        dx         = ((AcropolisBridgeMoveScratch*)(head - 0x18))->delta.vx.h.hi;
        dz         = s->delta.vz.h.hi;
        s->move.vx = dx;
        s->move.vz = dz;
        valx       = ((AcropolisBridgeMoveScratch*)(head - 0x18))->delta.vx.w;
        if ((valx & 0xFFFF) != 0) {
            if (valx > 0) {
                s->move.vx++;
            } else {
                s->move.vx--;
            }
        }
        valz = s->delta.vz.w;
        if ((valz & 0xFFFF) != 0) {
            if (valz > 0) {
                s->move.vz++;
            } else {
                s->move.vz--;
            }
        }
        if (work->field_6B == 0) {
            dy         = s->delta.vy.h.hi;
            valy       = s->delta.vy.w;
            s->move.vy = s->move.vy + dy;
            if ((valy & 0xFFFF) != 0) {
                if (valy > 0) {
                    s->move.vy++;
                } else {
                    s->move.vy--;
                }
            }
        } else {
            s->move.vy = 0;
        }
    } else {
        s->move.vx = 0;
        s->move.vy = 0;
        s->move.vz = 0;
    }
    if (work->field_6B == 0) {
        s->move.vy += 0x10;
    }
    work->moveDelta          = s->move;
    work->coord->coord.t[0] += s->move.vx;
    if (s->move.vy >= 0x21) {
        work->coord->coord.t[1] += 8;
    }
    if (s->move.vy < -0x20) {
        work->coord->coord.t[1] -= 0x20;
    }
    y   = s->move.vy;
    mag = y;
    if (y < 0) {
        SOFT_TOUCH_REG(mag);
        mag = -mag;
    }
    if (mag < 0x20) {
        work->coord->coord.t[1] += y;
    }
    work->coord->coord.t[2] += s->move.vz;
    if (work->coord->coord.t[0] != 0 || work->coord->coord.t[2] != 0) {
        work->moving = 1;
    } else {
        work->moving = 0;
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x18;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184B94);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80185104);

/// Runs the walker's per-frame step inside the 0x28-byte scratch frame
/// `func_acropolis_bridge_8018532C` opened for it. `head` is the scratch head
/// as it was before the frame was carved off, so the `SVECTOR3` the states
/// steer towards is `head - 0x24` == `&block->pos`.
///
/// State 1 heads straight for the actor selected by the walker's spawn
/// variant -- the low halfword of each translation component of that actor's
/// coordinate matrix -- state 2 re-runs the patrol steering and re-reads the
/// route's byte table at `cursor` whenever the step or the state changed, and
/// state 3 follows the patrol route proper. The scalar at `field_5E` then
/// ramps towards `field_5C` by `field_60` a frame; while it is non-zero it
/// scales (`GPF`) the normalised facing column of the model matrix into the
/// per-frame world step, which is added to the coordinate's translation and
/// kept in `moveStep`. `D_80072729` (a global freeze flag) zeroes the step
/// instead.
static __inline__ void walkerStep(AcropolisBridgeWalkerWork* walker, u8* head,
                                  AcropolisBridgeWalkScratch* block)
{
    u8*            head2;
    SVECTOR3*      pos;
    WipSysConfig*  cfg;
    SVECTOR*       sv;
    SVECTOR*       gsv;
    SVECTOR*       step;
    GsCOORDINATE2* coord;
    s16            sdiff;
    s32            diff;
    s16            speed;
    s32            cur;
    s32            target;
    s32            result;

    switch (walker->state) {
        case 0:
            break;
        case 1:
            cfg                            = &D_80073B08[walker->field_6E];
            pos                            = (SVECTOR3*)(head - 0x24);
            ((SVECTOR3*)(head - 0x24))->vx = *(u16*)&cfg->field_4->t[0];
            pos->vy                        = *(u16*)&cfg->field_4->t[1];
            pos->vz                        = *(u16*)&cfg->field_4->t[2];
            break;
        case 2:
            *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD - 4;
            walker->field_6F      = func_acropolis_bridge_801843A0(walker, 1);
            walker->field_70      = func_acropolis_bridge_8018450C(walker);
            if (walker->field_69 != walker->state || walker->field_70 != walker->field_72 ||
                walker->field_6F != walker->field_71) {
                func_acropolis_bridge_80184638(walker, 1);
                walker->node = walker->nav->field_4[walker->cursor];
            }
            walker->field_69 = walker->state;
            walker->field_72 = walker->field_70;
            walker->field_71 = walker->field_6F;
            if (func_acropolis_bridge_80184024(walker) != 0) {
                walker->cursor       += walker->field_73;
                walker->node          = walker->nav->field_4[walker->cursor];
                *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 4;
            }
            break;
        case 3:
            func_acropolis_bridge_80184208(walker, (SVECTOR3*)(head - 0x24));
            break;
    }
    func_acropolis_bridge_80185104(walker, &block->pos);

    cur    = walker->field_5C;
    target = walker->field_5E;
    if (cur != target) {
        diff  = cur - target;
        sdiff = diff;
        if (sdiff > walker->field_60) {
            result = target + walker->field_60;
        } else if (sdiff < -walker->field_60) {
            result = target - walker->field_60;
        } else {
            result = target + diff;
        }
        walker->field_5E = result;
    }

    coord = walker->coord;
    speed = walker->field_5E;
    step  = &walker->moveStep;
    if (D_80072729 == 1) {
        step->vz            = 0;
        step->vy            = 0;
        walker->moveStep.vx = 0;
    } else {
        head2                 = *(u8**)G_SCRATCH_HEAD;
        sv                    = (SVECTOR*)(head2 - 8);
        *(u8**)G_SCRATCH_HEAD = (u8*)sv;
        /* The ROM keeps a second copy of the block address for the GTE
           transfers; without it `sv` and the copy share one register. */
        gsv = sv;
        if (speed != 0) {
            Gfx_MatrixCol2(&coord->coord, sv);
            VectorNormalSS(sv, sv);
            gte_lddp(speed);
            gte_ldsv(gsv);
            gte_gpf12_real();
            gte_stsv(gsv);
            coord->coord.t[0] += ((SVECTOR*)(head2 - 8))->vx;
            coord->coord.t[1] += sv->vy;
            coord->coord.t[2] += sv->vz;
            walker->moveStep   = *(SVECTOR*)(head2 - 8);
            coord->flg         = 0;
        }
        *(u8**)G_SCRATCH_HEAD += 8;
    }
    if (walker->field_6C == 0) {
        func_acropolis_bridge_80184908(walker);
    }
    if (walker->field_6D == 0) {
        func_acropolis_bridge_80184B94(walker);
    }
}

void func_acropolis_bridge_8018532C(AcropolisBridgeWalkerWork* walker)
{
    u8*                         head;
    AcropolisBridgeWalkScratch* block;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x28;
    block                 = (AcropolisBridgeWalkScratch*)*(u8**)G_SCRATCH_HEAD;
    walkerStep(walker, head, block);
    walker->coord->flg    = 0;
    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + 0x28;
}

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
    work->walker.navData.count    = 0xA;
    work->walker.navData.field_4  = D_acropolis_bridge_801916CC;
    work->walker.navData.field_9  = 0xA;
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

/// Plays one of the bridge enemy's positional sounds. The spawn variant in the
/// enemy's `field_8` high nibble picks the bank, so the event id is that nibble
/// shifted into byte 1 of `base`, and the pan and depth come from the model's
/// root coordinate.
static __inline__ void bridge_play_snd(Task* task, GpEnemy* enemy, s32 base)
{
    s32 snd;
    s32 pan;

    snd = ((enemy->field_8 >> 12) << 8) | base;
    pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->field_8);
    SndEvt_EnqueueType6(snd, pan,
                        (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->field_8));
}

/// Runs the bridge enemy's plunge into the gorge. On the first frame (work
/// block still live) it disables the hit box, clears the enemy's link node tag,
/// seeds the three behaviour parameters and moves the model root out over the
/// gorge -- the X and Z it drops to depend on which of the three spawn variants
/// this is -- then loads the light-blend colour matrix and gives the root a
/// random yaw. Every frame after that the fall height comes from
/// `func_acropolis_bridge_8017E024`, and the model is spun on its own yaw and
/// scaled down once it is past 0x1F4, four units of scale per unit of depth.
/// Three chances to restart the scream animation are rolled on the way down --
/// on crossing 0x1F4, then one in sixteen frames while the animation has run
/// long enough, then one in thirty-two frames below 0x320 with the second
/// animation slot finished -- and the yaw is re-rolled while animation 4 is in
/// its fifth playback step. Landing on a kind-1 surface plays the impact sound
/// and hands over to state 7.
void func_acropolis_bridge_80186618(Task* task)
{
    AcropolisBridgeEnemyWork* work;
    AcropolisBridgeEnemyWork* anim;
    GpEnemy*                  enemy;
    VECTOR                    scale;
    s32                       amount;
    s32                       height;

    work  = (AcropolisBridgeEnemyWork*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work->field_4 != 0) {
        work->hit.flags    &= 0x7FFF;
        enemy->node.field_4 = 0;
        work->field_108     = 0x20;
        work->field_100     = 2;
        work->field_104     = 1;
        switch (enemy->field_8 >> 12) {
            case 0:
                ((TmdObject*)task->extra)->field_8->coord.t[0] = -0x22C4;
                ((TmdObject*)task->extra)->field_8->coord.t[2] = -0x640;
                break;
            case 1:
                ((TmdObject*)task->extra)->field_8->coord.t[0] = -0x270F;
                ((TmdObject*)task->extra)->field_8->coord.t[2] = -0x7D0;
                break;
            case 2:
                ((TmdObject*)task->extra)->field_8->coord.t[0] = -0x2EE0;
                ((TmdObject*)task->extra)->field_8->coord.t[2] = -0x5DC;
                break;
        }
        work->colorMtx.t[1]    = 0x80;
        work->colorMtx.t[0]    = 0x80;
        work->colorMtx.t[2]    = 0x5A0;
        work->colorMtx.m[2][1] = 0xC0;
        work->colorMtx.m[2][0] = 0xC0;
        work->colorMtx.m[2][2] = 0x5A0;
        work->colorMtx.m[1][1] = 0xC0;
        work->colorMtx.m[1][0] = 0xC0;
        work->colorMtx.m[1][2] = 0x5A0;
        work->colorMtx.m[0][1] = 0xC0;
        work->colorMtx.m[0][0] = 0xC0;
        work->colorMtx.m[0][2] = 0x5A0;
        Gp_LcgState            = Gp_LcgState * 5 + 0x71357911;
        work->yaw              = (u32)Gp_LcgState >> 16;
    }
    ((TmdObject*)task->extra)->field_8->coord.t[1] =
        func_acropolis_bridge_8017E024() - 0xC8;
    ((TmdObject*)task->extra)->field_8->flg = 0;
    height                                  = ((TmdObject*)task->extra)->field_8->coord.t[1];
    if (height < 0x1F4) {
        amount   = 0x1000;
        scale.vx = scale.vy = scale.vz = amount;
    } else {
        amount   = 0x1000;
        height  -= 0x1F4;
        height  *= 4;
        amount  -= height;
        scale.vx = scale.vy = scale.vz = amount;
    }
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, work->yaw, 1);
    ScaleMatrix(&((TmdObject*)task->extra)->field_8->coord, &scale);
    if (((TmdObject*)task->extra)->field_8->coord.t[1] < 0x1F4 &&
        work->field_104 != 4) {
        work->field_100 = 2;
        work->field_104 = 4;
        bridge_play_snd(task, enemy, 0x40290003);
    }
    if (work->field_104 == 4) {
        if (((TmdObject*)task->extra)->field_8->coord.t[1] >= -0x3DD &&
            (s32)((enemy->field_8 >> 12) + 8) < work->field_106) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 0xF) == 0) {
                work->field_108 = ((enemy->field_8 >> 12) * 2) + 0x10;
                work->field_100 = 2;
                work->field_104 = 4;
                bridge_play_snd(task, enemy, 0x40290003);
            }
        }
    }
    if (((TmdObject*)task->extra)->field_8->coord.t[1] < 0x320) {
        anim = (AcropolisBridgeEnemyWork*)task->idMap;
        if (anim->slots[1].field_2 == anim->slots[1].field_6) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 0x1F) == 0) {
                work->field_108 = 0x10;
                work->field_100 = 2;
                work->field_104 = 4;
                bridge_play_snd(task, enemy, 0x40290003);
            }
        }
    }
    if (*(s32*)&work->field_104 == 0x50004) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        work->yaw   = (u32)Gp_LcgState >> 16;
    }
    if (bridge_rec_kind1(work->recs) != 0) {
        if (work->field_10C > 0) {
            bridge_play_snd(task, enemy, 0x40290002);
        }
        work->field_0 = 7;
    }
    func_acropolis_bridge_8018581C(task);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80186BBC);

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
