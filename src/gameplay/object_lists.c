#include "gameplay/scene_combat.h"
#include "gameplay/world_collision.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor.h"
#include "gameplay/collision.h"
#include "collision.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"

#include "gameplay/damage.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task_types.h"

/// Scratch-stack workspace for finding the contact nearest a capsule body's second endpoint.
///
/// The endpoint is the capsule's `ends[1]` offset plus the body's position,
/// each component truncated to a signed halfword, in the body's own frame and
/// in game-coordinate units. The body's cached transform rotates it and the
/// transform's translation places it in the world. The vector that held the
/// rotated endpoint then takes, for each contact measured, that contact's
/// point minus the placed endpoint; the length of that difference is the
/// distance compared. The block is reserved uninitialized for one search and
/// released before it returns. The SDK vectors' fourth components are never
/// written, and nothing uses them.
typedef struct {
    union {
        VECTOR rotatedEndpoint; // Local endpoint after the body's rotation, before its translation
        VECTOR contactDelta;    // Contact point minus the placed endpoint, for the entry being measured
    } work;
    VECTOR  worldEndpoint;      // Endpoint 1 placed in the world: rotated, then translated
    SVECTOR localEndpoint;      // Endpoint 1 in the body's frame: capsule offset plus the body's position
} _WorldCollisionNearestContactScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionNearestContactScratch, 0x28);

/// Grid-normal Y below which a face counts as a floor.
///
/// Components use 4096 for one unit. The floor query records a face only when
/// its normal Y is below this value, and pushback splits contacts on the same
/// boundary. 3546 is about the cosine of 30 degrees at this scale, so the
/// boundary sits about 30 degrees off -Y.
enum { WORLD_COLLISION_FLOOR_NORMAL_Y = -0xDDA };

/// X or Z product of two 4096-unit normals below which they count as opposed.
///
/// The value is minus half of one squared unit. A more negative product on
/// either axis selects the opposed-push result. The correction is still stored.
enum { WORLD_COLLISION_OPPOSED_NORMAL_PRODUCT = -0x800000 };

/// Shift from a 12-bit normal-times-distance product to a 16.16 correction.
enum { WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT = 4 };

/// Scratch-stack workspace for resolving grid contacts into one pushback step.
///
/// The block is reserved uninitialized for that resolution and released before
/// it returns. Only occupied grid contacts on surfaces that apply pushback are
/// accumulated. `nonFloorSum` totals `normal * distance` for contacts whose
/// normal is not a floor. `floorSum` totals the vertical part of the floor
/// contacts, clearing X and Z on every one of them, and `floorCount` is how
/// many entered that sum so it can be averaged in. `opposedProduct` holds the
/// latest pairwise X and Z products of the non-floor normals; its Y is unused.
/// Distances are world-coordinate units, so each product has twelve fractional
/// bits before `WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT`.
typedef struct {
    VECTOR nonFloorSum;    // Sum of normal*distance for pushback contacts that are not floors
    VECTOR floorSum;       // Floor contacts: Y sums normal.vy*distance; X and Z are cleared each contact
    VECTOR opposedProduct; // Latest pairwise X and Z products of non-floor normals; Y is unused
    s32    floorCount;     // Floor contacts included in floorSum; zero skips the average
} _WorldCollisionPushbackScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionPushbackScratch, 0x34);

/// Slots of `_WorldCollisionResponsePushbackScratch.corrections`.
///
/// A slot's index is the response value a grid contact carries in bits 8..11
/// of its key, so that value selects both how the contact is resolved and the
/// slot that receives it.
enum {
    WORLD_COLLISION_RESPONSE_PUSHBACK_OVERLAP = 0, // Ordinary overlap: pushed out along the face normal
    WORLD_COLLISION_RESPONSE_PUSHBACK_FLOOR   = 1, // Floor query: lifted by the distance to the floor
    WORLD_COLLISION_RESPONSE_PUSHBACK_EDGE    = 2, // Edge overlap: slid horizontally off the nearest edge
    WORLD_COLLISION_RESPONSE_PUSHBACK_COUNT   = 3
};

/// Scratch-stack workspace for resolving grid contacts by their response value.
///
/// The block is reserved for that resolution and released before it returns.
/// Only occupied grid contacts on surfaces that apply pushback contribute.
/// `corrections` is cleared on entry and holds one candidate correction for
/// each response value: the overlap slot sums `normal * distance` over every
/// ordinary contact, the floor slot keeps the last floor contact's lift
/// `-(distance << 12)` in Y, and the edge slot keeps the X and Z of
/// `normal * distance` for the edge contact with the smallest distance among
/// those whose normal is horizontal. The result is overlap plus floor when
/// any ordinary contact contributed, and floor plus edge otherwise.
///
/// `opposedProduct` is left unset on entry and holds the latest pairwise X
/// and Z products of the ordinary contacts' normals; its Y is unused.
/// Distances are world-coordinate units and normals use 4096 for one unit, so
/// every correction has twelve fractional bits before
/// `WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT`.
typedef struct {
    VECTOR corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_COUNT]; // Candidate correction per response value; see the slot enum
    VECTOR opposedProduct;                                       // Latest pairwise X and Z products of ordinary-contact normals; Y is unused
} _WorldCollisionResponsePushbackScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionResponsePushbackScratch, 0x40);

/// Temporary vectors for placing a body's floor-query segment and finding its direction.
///
/// The local endpoints lie on the body's Y axis through the local origin, so
/// their X and Z are zero. Endpoint 0's Y is the body's position plus its
/// radius and endpoint 1's is the position minus that radius, each truncated
/// to a signed 16-bit component. The radius is the floor-query half-height,
/// in game units. The body's cached transform rotates an endpoint; the
/// caller's endpoint is that result plus the translation. The same vector
/// then holds endpoint 0 minus endpoint 1 for normalization. The block lives
/// on the scratch stack for that calculation. The SDK vectors' fourth
/// components are unused and left uninitialized.
typedef struct {
    union {
        VECTOR rotatedEndpoint; // Local endpoint after the body's rotation, before its translation
        VECTOR segmentDelta;    // Placed endpoint 0 minus endpoint 1, before normalization
    } work;
    SVECTOR localEndpoints[2];  // Body-frame ends: [0] position Y plus the half-height, [1] minus it
} _WorldCollisionFloorSegmentScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionFloorSegmentScratch, 0x20);

/// Scratch-stack workspace for building the rotation that faces along a direction.
///
/// The rotation is the product Ry(yaw) * Rx(-pitch) * Rz(roll), whose third
/// column - the local Z axis - is the normalized direction. Yaw is the
/// direction's heading about Y, measured from +Z toward +X, and pitch its
/// elevation above the XZ plane; the caller supplies the roll. Angles count
/// 4096 units per turn and matrix elements use `ONE` (4096) for 1.0.
///
/// The block is reserved uninitialized for one conversion and released before
/// it returns. Only the nine rotation elements of each matrix are used; their
/// translations and the vector's fourth halfword are never written.
typedef struct {
    union {
        SVECTOR direction; // Unit direction, `ONE` for 1.0, from which yaw and pitch are measured
        SVECTOR angles;    // Single-axis Euler angles for the next factor: yaw in Y, minus pitch in X, roll in Z
    } work;
    MATRIX rotation;       // Accumulated product: Ry(yaw), then Ry(yaw) * Rx(-pitch)
    MATRIX axisRotation;   // Factor right-multiplied next: Rx(-pitch), then Rz(roll)
    s16    pitch;          // Elevation of the direction above the XZ plane, 0..0xFFF
    s16    yaw;            // Heading of the direction about Y, 0..0xFFF
} _GfxDirectionRotationScratch;
STATIC_ASSERT_SIZEOF(_GfxDirectionRotationScratch, 0x4C);

/// The SDK natural logarithm uses twelve fractional bits; ln(2) is 2839 at that scale.
enum {
    WORLD_COLLISION_SURFACE_LOG_FRACTION_BITS = 12,
    WORLD_COLLISION_SURFACE_LOG_TWO           = 2839
};

/* Define BSS before API headers to preserve first-declaration order. */
WorldCollisionTrigger* Gp_PendingObj4C;

WorldCollisionBody* Gp_ObjList0;

WorldCollisionBody* Gp_ObjList1;

WorldCollisionBody* Gp_ObjList2;

WorldCollisionBody* Gp_ObjList3;

WorldCollisionBody* Gp_ObjList4;

WorldCollisionBody* Gp_ObjList5;

WorldCollisionBody* Gp_ObjList6;

WorldCollisionBody* Gp_ObjList7;

WorldCollisionBody* Gp_ObjList8;

#include "world_collision.h"
#include "attachments.h"

/// Nine-entry table of `WorldCollisionBody` list heads (`Gp_ObjList0` .. `Gp_ObjList8`).
/// `worldCollisionLinkBody` appends to `Gp_ObjLists[index]`; `worldCollisionUnlinkBody` unlinks.
extern WorldCollisionBody** Gp_ObjLists[9];

/// Two-entry table of `WorldCollisionTrigger` list heads. `Gp_LinkObj4A` appends to
/// `Gp_Obj4ALists[index]`; `Gp_ClearObj4AList` walks and clears that list.
extern WorldCollisionTrigger** Gp_Obj4ALists[2];

/// One-entry table of `WorldCollisionOccluder` list heads. `Gp_LinkObj3A` appends to
/// `Gp_Obj3ALists[index]`; `Gp_ClearObj3AList` walks and clears that list.
extern WorldCollisionOccluder** Gp_Obj3ALists[1];

static void Gp_WorldToGrid(VECTOR3* arg0, SVECTOR3* arg1);

static void Gp_UnlinkObj3A(s32 arg0, WorldCollisionOccluder* occluder);

WorldCollisionBody** Gp_ObjLists[9] = {
    &Gp_ObjList0,
    &Gp_ObjList1,
    &Gp_ObjList2,
    &Gp_ObjList3,
    &Gp_ObjList4,
    &Gp_ObjList5,
    &Gp_ObjList6,
    &Gp_ObjList7,
    &Gp_ObjList8,
};
WorldCollisionTrigger** Gp_Obj4ALists[2] = {
    &Gp_PendingObj4C,
    &Gp_Obj4CList,
};
WorldCollisionOccluder** Gp_Obj3ALists[1] = {
    &D_80115550,
};

void Gp_ClearObjHeads(void)
{
    Gp_ObjList0         = NULL;
    Gp_ObjList1         = NULL;
    Gp_ObjList2         = NULL;
    Gp_ObjList3         = NULL;
    Gp_ObjList4         = NULL;
    Gp_ObjList5         = NULL;
    Gp_ObjList6         = NULL;
    Gp_ObjList7         = NULL;
    Gp_ObjList8         = NULL;
    Gp_GridParams       = 0;
    Gp_PendingObj4C     = NULL;
    Gp_Obj4CList        = NULL;
    D_80115550          = NULL;
    Gp_PendingObj4CFlag = 0;
}

s32 func_800E0308(SVECTOR* arg0, SVECTOR* arg1)
{
    VECTOR*                 vec;
    WorldCollisionOccluder* node;
    s32                     ret;

    ret     = 0;
    node    = D_80115550;
    vec     = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    vec->vx = arg1->vx - arg0->vx;
    vec->vy = arg1->vy - arg0->vy;
    vec->vz = arg1->vz - arg0->vz;
    VectorNormal(vec, vec);
    for (; node != NULL; node = node->next) {
        if (node->flags & WORLD_COLLISION_OCCLUDER_ENABLED) {
            ret = worldCollisionTestOccluderSegment(node, arg0, arg1, vec);
            if (ret == 1) {
                break;
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
    return ret;
}

void Gp_CollideLists(WorldCollisionBody* a, WorldCollisionBody* b)
{
    WorldCollisionBody*           other;
    const WorldCollisionPairRule* rule;
    s32                           rowOffsetBytes;
    s32                           ruleOffsetBytes;
    u16                           flags;
    u16                           handlerIndex;
    u16                           swapBodies;
    u8                            rowIndex;
    u8                            columnIndex;

    for (; a != NULL; a = a->next) {
        flags = a->flags;
        if (flags & WORLD_COLLISION_BODY_PAIR_ENABLED) {
            rowIndex = (a->flags & WORLD_COLLISION_BODY_KIND_MASK) - WORLD_COLLISION_BODY_SPHERE;
            other    = b;
            if (other != NULL) {
                rowOffsetBytes = rowIndex * (s32)sizeof(D_8010FA4C[0]);
                for (; other != NULL; other = other->next) {
                    if (other->flags & WORLD_COLLISION_BODY_PAIR_ENABLED) {
                        columnIndex     = (other->flags & WORLD_COLLISION_BODY_KIND_MASK) - WORLD_COLLISION_BODY_SPHERE;
                        ruleOffsetBytes = columnIndex * (s32)sizeof(WorldCollisionPairRule) + rowOffsetBytes;
                        // Complete the byte offset before adding the matrix base.
                        rule         = (const WorldCollisionPairRule*)((const u8*)&D_8010FA4C + ruleOffsetBytes);
                        swapBodies   = rule->swapBodies;
                        handlerIndex = rule->handlerIndex;
                        if (swapBodies == false) {
                            Gp_PairHandlers[handlerIndex](a, other, handlerIndex);
                        } else {
                            Gp_PairHandlers[handlerIndex](other, a, handlerIndex);
                        }
                    }
                }
            }
        }
    }
}

void Gp_CollideListGrid(WorldCollisionBody* node)
{
    u16 flags;

    if (Gp_GridParams != 0) {
        for (; node != NULL; node = node->next) {
            flags = node->flags;
            if (flags & WORLD_COLLISION_BODY_GRID_ENABLED) {
                switch (flags & WORLD_COLLISION_BODY_KIND_MASK) {
                    case WORLD_COLLISION_BODY_NONE:
                        break;
                    case WORLD_COLLISION_BODY_SPHERE:
                        Gp_CollideObjGrid(node);
                        break;
                    case WORLD_COLLISION_BODY_CONTACT_PROXY:
                        break;
                    case WORLD_COLLISION_BODY_CAPSULE:
                        func_800DDDF8(node);
                        break;
                    case WORLD_COLLISION_BODY_MOTION_SPHERE:
                        if (node->flags & WORLD_COLLISION_BODY_FLOOR_QUERY) {
                            func_800DD940(node);
                        }
                        Gp_CollideObjGridDir(node);
                        break;
                }
            }
        }
    }
}

void func_800E0608(WorldCollisionBody* node, s32 mask, s32 match)
{
    WorldCollisionTrigger* other;

    other = Gp_PendingObj4C;
    for (; node != NULL; node = node->next) {
        if ((node->flags & mask) == (u16)match) {
            for (; other != NULL; other = other->next) {
                if (other->flags & WORLD_COLLISION_TRIGGER_ENABLED) {
                    func_800DEF80(node, other);
                }
            }
        }
    }
}

void func_800E06AC(WorldCollisionBody* node, s32 mask, s32 match)
{
    WorldCollisionTrigger* other;
    GameActor*             actor;
    s32                    idx;
    s32                    msk;
    u16                    mch;

    other = Gp_Obj4CList;
    idx   = GAME_TASK_SLOT_PLAYER;
    msk   = mask;
    mch   = match;
    actor = gameGetTaskSlot(idx)->work;
    for (; node != NULL; node = node->next) {
        if ((node->flags & msk) == mch) {
            for (; other != NULL; other = other->next) {
                if (other->flags & WORLD_COLLISION_TRIGGER_ENABLED) {
                    worldCollisionTestViewBoundarySphere(node, other, &actor->previousPosition);
                }
            }
        }
    }
}

s32 worldCollisionPairNop(WorldCollisionBody* firstBody, WorldCollisionBody* secondBody, s32 handlerIndex)
{
    return 0;
}

void Gp_LocalToGrid(VECTOR3* arg0, SVECTOR3* arg1)
{
    u8*                 head;
    VECTOR*             vec;
    WorldCollisionGrid* grid;
    s32                 val;

    head = SCRATCH_STACK_CURSOR(u8);
    vec = SCRATCH_STACK_CURSOR(VECTOR) = (VECTOR*)(head - 0x10);
    // Recover room XZ coordinates before applying the grid-origin biases.
    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, (VECTOR*)arg0, vec);
    grid = Gp_GridParams;
    val  = ((VECTOR*)(head - 0x10))->vx + grid->xBias - grid->viewCoord->coord.t[0];
    if (val >= 0) {
        arg1->vx = val / grid->cellSize;
    } else {
        arg1->vx = WORLD_COLLISION_GRID_INVALID_CELL;
    }
    grid     = Gp_GridParams;
    arg1->vy = 0;
    val      = vec->vz + grid->zBias - grid->viewCoord->coord.t[2];
    if (val >= 0) {
        arg1->vz = val / grid->cellSize;
    } else {
        arg1->vz = WORLD_COLLISION_GRID_INVALID_CELL;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

void worldCollisionGetBodyComposedPosition(const WorldCollisionBody* body, VECTOR* position)
{
    // Only XYZ is accessed; the remainder of the original reservation is unproven.
    enum { WORLD_COLLISION_BODY_POSITION_SCRATCH_BYTES = 0x30 };
    VECTOR3* rotatedOffset;

    rotatedOffset = SCRATCH_STACK_RESERVE_BYTES(WORLD_COLLISION_BODY_POSITION_SCRATCH_BYTES);
    gte_SetRotMatrix(&body->coord->workm);
    gte_ldv0(&body->pos);
    gte_rtv0();
    gte_stlvnl(rotatedOffset);
    position->vx = body->coord->workm.t[0] + rotatedOffset->vx;
    position->vy = body->coord->workm.t[1] + rotatedOffset->vy;
    position->vz = body->coord->workm.t[2] + rotatedOffset->vz;
    SCRATCH_STACK_RELEASE_BYTES(WORLD_COLLISION_BODY_POSITION_SCRATCH_BYTES);
}

void worldCollisionPlaceFloorSegment(const WorldCollisionBody* body, VECTOR endpoints[2], SVECTOR* direction)
{
    _WorldCollisionFloorSegmentScratch* scratch;
    s32                                 endpointIndex;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionFloorSegmentScratch);
    // Local Y segment through the origin: X and Z stay zero, one half-height either side of the body's Y.
    scratch->localEndpoints[0].vx = 0;
    scratch->localEndpoints[0].vy = (u16)body->pos.vy + body->radius;
    scratch->localEndpoints[0].vz = 0;
    scratch->localEndpoints[1].vx = 0;
    scratch->localEndpoints[1].vy = (u16)body->pos.vy - body->radius;
    scratch->localEndpoints[1].vz = 0;
    gte_SetRotMatrix(&body->coord->workm);
    for (endpointIndex = 0; endpointIndex < ARRAY_SIZE(scratch->localEndpoints); endpointIndex++) {
        gte_ldv0(&scratch->localEndpoints[endpointIndex]);
        gte_rtv0();
        gte_stlvnl(&scratch->work.rotatedEndpoint);
        endpoints[endpointIndex].vx = scratch->work.rotatedEndpoint.vx + body->coord->workm.t[0];
        endpoints[endpointIndex].vy = scratch->work.rotatedEndpoint.vy + body->coord->workm.t[1];
        endpoints[endpointIndex].vz = scratch->work.rotatedEndpoint.vz + body->coord->workm.t[2];
    }
    scratch->work.segmentDelta.vx = endpoints[0].vx - endpoints[1].vx;
    scratch->work.segmentDelta.vy = endpoints[0].vy - endpoints[1].vy;
    scratch->work.segmentDelta.vz = endpoints[0].vz - endpoints[1].vz;
    VectorNormalS(&scratch->work.segmentDelta, direction);
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionFloorSegmentScratch);
}

void Gp_ClearPendingObj4C(void)
{
    WorldCollisionTrigger* node;

    for (node = Gp_PendingObj4C; node != NULL; node = node->next) {
        if (node->hit != 0) {
            node->hit = 0;
        }
    }
}

static void Gp_WorldToGrid(VECTOR3* arg0, SVECTOR3* arg1)
{
    s32                 val;
    WorldCollisionGrid* grid;

    grid = Gp_GridParams;
    val  = arg0->vx + grid->xBias;
    if (val >= 0) {
        arg1->vx = val / grid->cellSize;
    } else {
        arg1->vx = WORLD_COLLISION_GRID_INVALID_CELL;
    }
    grid     = Gp_GridParams;
    arg1->vy = 0;
    val      = arg0->vz + grid->zBias;
    if (val >= 0) {
        arg1->vz = val / grid->cellSize;
    } else {
        arg1->vz = WORLD_COLLISION_GRID_INVALID_CELL;
    }
}

s32 func_800E0C10(WorldCollisionContact* arg0, WorldCollisionDelta* delta, s32 arg2, s32* arg3)
{
    u8*                             head;
    _WorldCollisionPushbackScratch* scratch;
    WorldCollisionContact*          rec;
    s32                             i;
    s32                             j;
    s32                             count;
    s32                             mask;
    s32                             ret;

    count = 0;
    ret   = 0;
    mask  = 0;
    /* `list` is a VLA, so its alloca has to be emitted after the three
     * initializations above; the inner block is what pins that order. */
    {
        s16 list[arg2];

        if (arg2 == 0) {
            return count;
        }

        head                       = SCRATCH_STACK_CURSOR(u8);
        SCRATCH_STACK_CURSOR(void) = head - sizeof(_WorldCollisionPushbackScratch);
        scratch                    = (_WorldCollisionPushbackScratch*)(head - sizeof(_WorldCollisionPushbackScratch));

        scratch->nonFloorSum.vx = 0;
        scratch->nonFloorSum.vy = 0;
        scratch->nonFloorSum.vz = 0;
        scratch->floorSum.vx    = 0;
        scratch->floorSum.vy    = 0;
        scratch->floorSum.vz    = 0;
        scratch->floorCount     = 0;

        // Separate floor contacts from the rest of the pushback.
        for (i = 0; i < arg2; i++) {
            rec = &arg0[i];
            if ((rec->flags & WORLD_COLLISION_CONTACT_OCCUPIED) && (rec->key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_GRID) {
                mask |= 1 << rec->key.value;
                if (Gp_RoomParams[rec->key.value & 7] == WORLD_COLLISION_SURFACE_APPLY_PUSHBACK) {
                    if (rec->response.direction.vy >= WORLD_COLLISION_FLOOR_NORMAL_Y) {
                        scratch->nonFloorSum.vx += rec->response.direction.vx * rec->distance;
                        scratch->nonFloorSum.vy += rec->response.direction.vy * rec->distance;
                        scratch->nonFloorSum.vz += rec->response.direction.vz * rec->distance;
                        list[count++]            = i;
                    } else {
                        scratch->floorSum.vx  = 0;
                        scratch->floorSum.vy += rec->response.direction.vy * rec->distance;
                        scratch->floorSum.vz  = 0;
                        scratch->floorCount++;
                    }
                }
                ret = 1;
            }
        }

        if (arg3 != NULL) {
            *arg3 = mask;
        }

        // Record when two non-floor normals oppose on X or Z.
        for (i = 0; i < count; i++) {
            for (j = 1; j < count; j++) {
                scratch->opposedProduct.vx = arg0[list[i]].response.direction.vx * arg0[list[j]].response.direction.vx;
                scratch->opposedProduct.vz = arg0[list[i]].response.direction.vz * arg0[list[j]].response.direction.vz;
                if (scratch->opposedProduct.vx < WORLD_COLLISION_OPPOSED_NORMAL_PRODUCT || scratch->opposedProduct.vz < WORLD_COLLISION_OPPOSED_NORMAL_PRODUCT) {
                    ret = 2;
                }
            }
        }

        // Convert the 12-bit products to a 16.16 correction, averaging the floors.
        delta->fixed.vx.word = scratch->nonFloorSum.vx << WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT;
        delta->fixed.vy.word = scratch->nonFloorSum.vy << WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT;
        delta->fixed.vz.word = scratch->nonFloorSum.vz << WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT;
        if (scratch->floorCount != 0) {
            delta->fixed.vx.word += (scratch->floorSum.vx / scratch->floorCount) << WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT;
            delta->fixed.vy.word += (scratch->floorSum.vy / scratch->floorCount) << WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT;
            delta->fixed.vz.word += (scratch->floorSum.vz / scratch->floorCount) << WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT;
        }

        SCRATCH_STACK_RELEASE_BYTES(sizeof(_WorldCollisionPushbackScratch));
        return ret;
    }
}

s32 func_800E0FEC(WorldCollisionContact* arg0, WorldCollisionDelta* delta, s32 arg2, s32* arg3)
{
    u8*                                     head;
    _WorldCollisionResponsePushbackScratch* scratch;
    WorldCollisionContact*                  rec;
    s32                                     i;
    s32                                     j;
    s32                                     count;
    s32                                     mask;
    s32                                     ret;
    s32                                     prev;
    u8                                      list[0x20];

    ret   = 0;
    count = 0;
    mask  = 0;
    prev  = 0;
    if (arg2 == 0) {
        return ret;
    }

    head                       = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(void) = head - sizeof(_WorldCollisionResponsePushbackScratch);
    scratch                    = (_WorldCollisionResponsePushbackScratch*)(head - sizeof(_WorldCollisionResponsePushbackScratch));

    // The opposed product needs no clearing: it is written before each test.
    for (i = 0; i < WORLD_COLLISION_RESPONSE_PUSHBACK_COUNT; i++) {
        scratch->corrections[i].vx = 0;
        scratch->corrections[i].vy = 0;
        scratch->corrections[i].vz = 0;
    }

    // File each grid contact's correction under its response value.
    for (i = 0; i < arg2; i++) {
        rec = &arg0[i];
        if ((rec->flags & WORLD_COLLISION_CONTACT_OCCUPIED) && (rec->key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_GRID) {
            mask |= 1 << rec->key.value;
            if (Gp_RoomParams[rec->key.value & 7] == WORLD_COLLISION_SURFACE_APPLY_PUSHBACK) {
                switch ((u32)(rec->key.value & 0xF00) >> 8) {
                    case WORLD_COLLISION_RESPONSE_PUSHBACK_OVERLAP:
                        scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_OVERLAP].vx += rec->distance * rec->response.direction.vx;
                        scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_OVERLAP].vy += rec->distance * rec->response.direction.vy;
                        scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_OVERLAP].vz += rec->distance * rec->response.direction.vz;
                        list[count++]                                                       = i;
                        break;
                    case WORLD_COLLISION_RESPONSE_PUSHBACK_FLOOR:
                        scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_FLOOR].vx = 0;
                        scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_FLOOR].vy = -(rec->distance << 12);
                        scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_FLOOR].vz = 0;
                        break;
                    case WORLD_COLLISION_RESPONSE_PUSHBACK_EDGE:
                        if (rec->response.direction.vy == 0 && ((s16)prev == 0 || rec->distance < (s16)prev)) {
                            scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_EDGE].vx = rec->distance * rec->response.direction.vx;
                            scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_EDGE].vy = 0;
                            scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_EDGE].vz = rec->distance * rec->response.direction.vz;
                            prev                                                            = (u16)rec->distance;
                        }
                        break;
                }
            }
            ret = 1;
        }
    }

    // Record when two ordinary-contact normals oppose on X or Z.
    for (i = 0; i < count; i++) {
        for (j = 1; j < count; j++) {
            scratch->opposedProduct.vx = arg0[list[i]].response.direction.vx * arg0[list[j]].response.direction.vx;
            scratch->opposedProduct.vz = arg0[list[i]].response.direction.vz * arg0[list[j]].response.direction.vz;
            if (scratch->opposedProduct.vx < WORLD_COLLISION_OPPOSED_NORMAL_PRODUCT || scratch->opposedProduct.vz < WORLD_COLLISION_OPPOSED_NORMAL_PRODUCT) {
                ret = 2;
            }
        }
    }

    if (arg3 != NULL) {
        *arg3 = mask;
    }

    // Convert the 12-bit products to a 16.16 correction; the edge slide applies only without an ordinary overlap.
    if (count != 0) {
        delta->fixed.vx.word = (scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_OVERLAP].vx + scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_FLOOR].vx) << WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT;
        delta->fixed.vy.word = (scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_OVERLAP].vy + scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_FLOOR].vy) << WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT;
        delta->fixed.vz.word = (scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_OVERLAP].vz + scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_FLOOR].vz) << WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT;
    } else {
        delta->fixed.vx.word = (scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_FLOOR].vx + scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_EDGE].vx) << WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT;
        delta->fixed.vy.word = (scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_FLOOR].vy + scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_EDGE].vy) << WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT;
        delta->fixed.vz.word = (scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_FLOOR].vz + scratch->corrections[WORLD_COLLISION_RESPONSE_PUSHBACK_EDGE].vz) << WORLD_COLLISION_PUSHBACK_FRACTION_SHIFT;
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(_WorldCollisionResponsePushbackScratch));
    return ret;
}

/// Places a capsule's second endpoint in the world using its body's cached transform.
///
/// The local sum wraps to signed halfwords before rotation. The caller owns
/// the reserved scratch block; this operation also replaces the GTE rotation.
static __inline__ void _worldCollisionPlaceCapsuleSecondEndpoint(const WorldCollisionBody*             body,
                                                                 const WorldCollisionCapsule*          capsule,
                                                                 _WorldCollisionNearestContactScratch* scratch)
{
    gte_SetRotMatrix(&body->coord->workm);
    scratch->localEndpoint.vx = (u16)capsule->ends[1].vx + (u16)body->pos.vx;
    scratch->localEndpoint.vy = (u16)capsule->ends[1].vy + (u16)body->pos.vy;
    scratch->localEndpoint.vz = (u16)capsule->ends[1].vz + (u16)body->pos.vz;
    gte_ldv0(&scratch->localEndpoint);
    gte_rtv0();
    gte_stlvnl(&scratch->work.rotatedEndpoint);
    scratch->worldEndpoint.vx = scratch->work.rotatedEndpoint.vx + body->coord->workm.t[0];
    scratch->worldEndpoint.vy = scratch->work.rotatedEndpoint.vy + body->coord->workm.t[1];
    scratch->worldEndpoint.vz = scratch->work.rotatedEndpoint.vz + body->coord->workm.t[2];
}

/// Returns the occupied contact nearest a capsule's second endpoint, as a one-based index, or 0.
///
/// `body` must be a capsule with a live cached world transform and a readable,
/// non-NULL contact table ending in `WORLD_COLLISION_CONTACT_LAST`. The final
/// entry is included, holes are allowed and equal distances retain the first
/// match. `contactKind` is a packed-key category already shifted into the high
/// halfword, with zero in the low halfword. Distances use game-coordinate units;
/// the local endpoint sum truncates to signed halfwords before world placement.
/// The body and table are unchanged. Uses a temporary scratch-stack block and
/// changes GTE state; no pointer is retained.
static s32 _worldCollisionFindNearestCapsuleEndContactIndex(const WorldCollisionBody* body, s32 contactKind)
{
    // The unsigned all-ones value is above every candidate distance.
    enum { WORLD_COLLISION_NEAREST_DISTANCE_NONE = -1 };
    _WorldCollisionNearestContactScratch* scratch;
    const WorldCollisionCapsule*          capsule;
    const WorldCollisionContact*          contact;
    u32                                   nearestDistance;
    s32                                   contactIndex;
    s32                                   nearestContactIndex;
    s32                                   deltaX;
    s32                                   deltaY;
    s32                                   deltaZ;
    u32                                   distance;

    nearestDistance     = WORLD_COLLISION_NEAREST_DISTANCE_NONE;
    contactIndex        = 0;
    nearestContactIndex = contactIndex;
    capsule             = body->context.capsule;
    scratch             = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionNearestContactScratch);
    contact             = capsule->contacts;
    // Place the capsule's second endpoint in the world: rotate it, then add the translation.
    _worldCollisionPlaceCapsuleSecondEndpoint(body, capsule, scratch);

    // Measure every occupied contact of the requested kind from that endpoint.
    for (;;) {
        if ((contact->flags & WORLD_COLLISION_CONTACT_OCCUPIED) && ((contact->key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == contactKind)) {
            deltaX                        = contact->point.vx - scratch->worldEndpoint.vx;
            scratch->work.contactDelta.vx = deltaX;
            deltaY                        = contact->point.vy - scratch->worldEndpoint.vy;
            scratch->work.contactDelta.vy = deltaY;
            deltaZ                        = contact->point.vz - scratch->worldEndpoint.vz;
            scratch->work.contactDelta.vz = deltaZ;
            distance                      = SquareRoot0((deltaX * deltaX) + (deltaY * deltaY) + (deltaZ * deltaZ));
            if (distance < nearestDistance) {
                nearestDistance     = distance;
                nearestContactIndex = contactIndex + 1;
            }
        }
        if (contact->flags & WORLD_COLLISION_CONTACT_LAST) {
            break;
        }
        contact++;
        contactIndex++;
    }

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionNearestContactScratch);
    return nearestContactIndex;
}

/// Appends a borrowed collision body in insertion order and records its incoming link.
///
/// `listHead` must address a live, writable head slot for an acyclic list;
/// `body` must be non-NULL and absent from every list. The incoming link is
/// the head slot for an empty list, or the previous tail's `next` otherwise.
/// The head slot must remain alive while the list is nonempty, and each body
/// until it is unlinked. The body becomes the new tail; its shape, contacts
/// and flags are unchanged. The caller is responsible for validating the
/// shape kind and setting the LINKED flag.
static __inline__ void _worldCollisionAppendBody(WorldCollisionBody** listHead, WorldCollisionBody* body)
{
    WorldCollisionBody*       tail;
    WorldCollisionBody* const first = *listHead;

    if (first != NULL) {
        tail = first;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = body;
        body->prev = &tail->next;
    } else {
        *listHead  = body;
        body->prev = listHead;
    }
    body->next = NULL;
}

void worldCollisionLinkBody(s32 listIndex, WorldCollisionBody* body)
{
    u16                  flags;
    WorldCollisionBody** head;

    head  = Gp_ObjLists[listIndex];
    flags = body->flags;
    if (!(flags & WORLD_COLLISION_BODY_LINKED)) {
        if ((flags & WORLD_COLLISION_BODY_KIND_MASK) <= WORLD_COLLISION_BODY_MOTION_SPHERE) {
            body->flags = flags | WORLD_COLLISION_BODY_LINKED;
            _worldCollisionAppendBody(head, body);
        }
    }
}

void worldCollisionUnlinkBody(WorldCollisionBody* body)
{
    u16                  flags;
    WorldCollisionBody*  next;
    WorldCollisionBody** previousLink;

    flags = body->flags;
    if (flags & WORLD_COLLISION_BODY_LINKED) {
        next         = body->next;
        body->flags  = flags & WORLD_COLLISION_BODY_KIND_MASK;
        previousLink = body->prev;
        if (next != NULL) {
            *previousLink = next;
            next->prev    = body->prev;
            body->next    = NULL;
        } else {
            *previousLink = NULL;
        }
        body->prev = NULL;
    }
}

void Gp_LinkObj4A(s32 arg0, WorldCollisionTrigger* arg1)
{
    u8                      flags;
    WorldCollisionTrigger** head;
    WorldCollisionTrigger*  node;
    WorldCollisionTrigger*  temp;

    head  = Gp_Obj4ALists[arg0];
    flags = arg1->flags;
    if (!(flags & WORLD_COLLISION_TRIGGER_LINKED)) {
        arg1->flags = flags | WORLD_COLLISION_TRIGGER_LINKED;
        temp        = *head;
        if (temp != NULL) {
            node = temp;
            while (node->next != NULL) {
                node = node->next;
            }
            node->next     = arg1;
            arg1->prevLink = &node->next;
        } else {
            *head          = arg1;
            arg1->prevLink = head;
        }
        arg1->next = NULL;
    }
}

void Gp_UnlinkObj4A(s32 arg0, WorldCollisionTrigger* arg1)
{
    u8                      flags;
    WorldCollisionTrigger*  next;
    WorldCollisionTrigger** prev;

    flags = arg1->flags;
    if (flags & WORLD_COLLISION_TRIGGER_LINKED) {
        next        = arg1->next;
        arg1->flags = flags & WORLD_COLLISION_TRIGGER_PERSISTENT_FLAGS;
        prev        = arg1->prevLink;
        if (next != NULL) {
            *prev          = next;
            next->prevLink = arg1->prevLink;
            arg1->next     = NULL;
        } else {
            *prev = NULL;
        }
        arg1->prevLink = NULL;
    }
}

void Gp_ClearObj4AList(s32 arg0)
{
    WorldCollisionTrigger** head;
    WorldCollisionTrigger*  node;
    WorldCollisionTrigger*  next;
    WorldCollisionTrigger*  temp;
    s32                     flags;

    head = Gp_Obj4ALists[arg0];
    temp = *head;
    if (temp != NULL) {
        node  = temp;
        *head = NULL;
        for (;;) {
            flags          = node->flags;
            next           = node->next;
            node->prevLink = NULL;
            flags         &= ~(0xFF ^ WORLD_COLLISION_TRIGGER_PERSISTENT_FLAGS);
            node->flags    = flags;
            if (next != NULL) {
                node->next = NULL;
                node       = next;
            } else {
                break;
            }
        }
    }
}

void Gp_LinkObj3A(s32 arg0, WorldCollisionOccluder* occluder)
{
    u8                       flags;
    WorldCollisionOccluder** head;
    WorldCollisionOccluder*  tail;
    WorldCollisionOccluder*  first;

    head  = Gp_Obj3ALists[arg0];
    flags = occluder->flags;
    if (!(flags & WORLD_COLLISION_OCCLUDER_LINKED)) {
        occluder->flags = flags | WORLD_COLLISION_OCCLUDER_LINKED;
        first           = *head;
        if (first != NULL) {
            tail = first;
            while (tail->next != NULL) {
                tail = tail->next;
            }
            tail->next         = occluder;
            occluder->prevLink = &tail->next;
        } else {
            *head              = occluder;
            occluder->prevLink = head;
        }
        occluder->next = NULL;
    }
}

static void Gp_UnlinkObj3A(s32 arg0, WorldCollisionOccluder* occluder)
{
    u8                       flags;
    WorldCollisionOccluder*  next;
    WorldCollisionOccluder** prevLink;

    flags = occluder->flags;
    if (flags & WORLD_COLLISION_OCCLUDER_LINKED) {
        next            = occluder->next;
        occluder->flags = flags & WORLD_COLLISION_OCCLUDER_PERSISTENT_FLAGS;
        prevLink        = occluder->prevLink;
        if (next != NULL) {
            *prevLink      = next;
            next->prevLink = occluder->prevLink;
            occluder->next = NULL;
        } else {
            *prevLink = NULL;
        }
        occluder->prevLink = NULL;
    }
}

void Gp_ClearObj3AList(s32 arg0)
{
    WorldCollisionOccluder** head;
    WorldCollisionOccluder*  node;
    WorldCollisionOccluder*  next;
    WorldCollisionOccluder*  first;
    s32                      flags;

    head  = Gp_Obj3ALists[arg0];
    first = *head;
    if (first != NULL) {
        node  = first;
        *head = NULL;
        for (;;) {
            flags          = node->flags;
            next           = node->next;
            node->prevLink = NULL;
            flags         &= ~(0xFF ^ WORLD_COLLISION_OCCLUDER_PERSISTENT_FLAGS);
            node->flags    = flags;
            if (next != NULL) {
                node->next = NULL;
                node       = next;
            } else {
                break;
            }
        }
    }
}

void worldCollisionInitContacts(WorldCollisionContact* contacts, s32 count, s32 unused)
{
    memFillBytes(contacts, 0, count * sizeof(*contacts));
    contacts[count - 1].flags = WORLD_COLLISION_CONTACT_LAST;
}

void Gp_LoadRoomParams(void)
{
    s32                               i;
    GameSession*                      session;
    WorldCollisionSurfaceProperties** surfaceProperties;

    for (i = ARRAY_SIZE(Gp_RoomParams) - 1; i >= 0; i--) {
        Gp_RoomParams[i] = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK;
    }

    session           = gGameSession;
    surfaceProperties = Gp_RoomParamTables[session->location.loc.stage - 1][session->location.loc.area - 1];
    for (i = 0; i < ARRAY_SIZE(Gp_RoomParams); i++) {
        Gp_RoomParams[i] = surfaceProperties[i]->suppressPushback;
    }
}

s32 worldCollisionFindContactIndex(const WorldCollisionContact* contacts, s32 searchKey)
{
    s32 matchedIndex;
    s32 contactIndex;

    matchedIndex = 0;
    for (contactIndex = 1;; contactIndex++) {
        if (contacts->flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
            if (searchKey == WORLD_COLLISION_FIND_ANY_KEY) {
                return 1;
            }
            if (contacts->key.value == searchKey) {
                matchedIndex = contactIndex;
            }
        }
        if ((contacts++)->flags & WORLD_COLLISION_CONTACT_LAST) {
            break;
        }
    }
    return matchedIndex;
}

s32 worldCollisionCountContactsByKind(const WorldCollisionContact* contacts, s32 contactKind)
{
    s32 count;

    count = 0;
    do {
        if ((contacts->flags & WORLD_COLLISION_CONTACT_OCCUPIED) && ((contacts->key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == contactKind)) {
            count += 1;
        }
    } while (!((contacts++)->flags & WORLD_COLLISION_CONTACT_LAST));
    return count;
}

void worldCollisionClearContacts(WorldCollisionContact* contacts)
{
    for (;;) {
        if (contacts->flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
            contacts->flags                &= WORLD_COLLISION_CONTACT_LAST;
            contacts->distance              = 0;
            contacts->key.value             = 0;
            contacts->point.vx              = 0;
            contacts->point.vy              = 0;
            contacts->point.vz              = 0;
            contacts->response.direction.vx = 0;
            contacts->response.direction.vy = 0;
            contacts->response.direction.vz = 0;
        }
        if (contacts->flags & WORLD_COLLISION_CONTACT_LAST) {
            break;
        }
        contacts++;
    }
}

s32 worldCollisionSurfaceClassFromMask(const u8* mask)
{
    s32 fixedMask;
    s32 surfaceClass;

    fixedMask = *mask << WORLD_COLLISION_SURFACE_LOG_FRACTION_BITS;
    if (fixedMask != 0) {
        surfaceClass = cln(fixedMask) / WORLD_COLLISION_SURFACE_LOG_TWO;
    } else {
        surfaceClass = 0;
    }
    return surfaceClass;
}

s32 worldCollisionSurfaceClassFromKey(s32 key)
{
    // Only the first stack word is written; the second word's role is unproven.
    s32 storedMask[2];
    s32 classMask;
    s32 fixedMask;
    s32 surfaceClass;

    // Packed category and response bits do not participate in the target's variable shift.
    classMask     = 1U << key;
    storedMask[0] = classMask;
    fixedMask     = (u8)classMask << WORLD_COLLISION_SURFACE_LOG_FRACTION_BITS;
    if (fixedMask != 0) {
        surfaceClass = cln(fixedMask) / WORLD_COLLISION_SURFACE_LOG_TWO;
    } else {
        surfaceClass = 0;
    }
    return surfaceClass;
}

void Gp_CommitObj4CSave(void)
{
    WorldCollisionTrigger* node;

    for (node = Gp_Obj4CList; node != NULL; node = node->next) {
        if (node->hit != 0) {
            node->hit = 0;
            if (gGameSession->location.loc.view == node->parameter0) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = node->parameter1;
            }
        }
    }
}

s32 Gp_TakePendingObj4C(u16* arg0, u8* arg1, u8* arg2)
{
    WorldCollisionTrigger* node;

    for (node = Gp_PendingObj4C; node != NULL; node = node->next) {
        if (node->hit != 0) {
            Gp_PendingObj4CFlag = 1;
            *arg0               = node->control;
            *arg1               = node->parameter0;
            *arg2               = node->parameter1;
            return 1;
        }
    }
    return 0;
}

/// Writes a Parasite Energy attack into one enemy contact slot.
///
/// `contact` must be non-NULL writable storage in the target's initialized
/// contact table; either an empty or an occupied slot may be supplied.
/// `attackKey` is the complete packed attack identity and is copied unchanged.
/// Distance and the point/response XYZ components are zero because targeting
/// supplies no collision geometry. Existing flags, including LAST and body-index
/// bits, and both vector pad halfwords survive. OCCUPIED is set after writing
/// the payload. The caller selects the slot and counts the target; no pointer
/// is retained.
static __inline__ void _attachmentWriteTargetContact(WorldCollisionContact* contact, s32 attackKey)
{
    contact->key.value             = attackKey;
    contact->distance              = 0;
    contact->point.vx              = 0;
    contact->point.vy              = 0;
    contact->point.vz              = 0;
    contact->response.direction.vx = 0;
    contact->response.direction.vy = 0;
    contact->response.direction.vz = 0;
    contact->flags                |= WORLD_COLLISION_CONTACT_OCCUPIED;
}

void attachmentAddTargetContact(const Enemy* enemy, s32 attackKey)
{
    WorldCollisionContact* contact;
    WorldCollisionContact* contacts;
    s32                    occupied;
    SceneCombatState*      combat;

    contacts = enemy->recs;
    if (contacts != NULL) {
        contact  = contacts;
        occupied = WORLD_COLLISION_CONTACT_OCCUPIED;
        // A full table replaces its final entry, so every selected target receives the attack.
        while (1) {
            // Read the aligned flags/distance word; only the low flag bits participate.
            if ((*(const s32*)&contact->flags & (WORLD_COLLISION_CONTACT_OCCUPIED | WORLD_COLLISION_CONTACT_LAST)) != occupied) {
                break;
            }
            contact++;
        }
        _attachmentWriteTargetContact(contact, attackKey);
        combat = &gSceneCombatState;
        combat->peTargetCount++;
    }
}

/// Multiplies direction-facing rotations with the GTE, leaving translation and padding untouched.
///
/// Inputs use ONE (4096) for 1.0. Each column product is shifted by twelve bits
/// and saturated to signed halfwords by the GTE. Matrices must be word-aligned;
/// `out` may equal either input, otherwise it must be disjoint from both.
/// Loading the complete left rotation before writing any column makes that
/// in-place composition safe. Changes GTE rotation and arithmetic state and
/// retains no pointers.
static __inline__ void _gfxMultiplyDirectionRotations(const MATRIX* left, const MATRIX* right, MATRIX* out)
{
    gte_SetRotMatrix(left);
    gte_ldclmv(&right->m[0][0]);
    gte_rtir();
    gte_stclmv(&out->m[0][0]);
    gte_ldclmv(&right->m[0][1]);
    gte_rtir();
    gte_stclmv(&out->m[0][1]);
    gte_ldclmv(&right->m[0][2]);
    gte_rtir();
    gte_stclmv(&out->m[0][2]);
}

void gfxBuildDirectionRotation(const VECTOR* direction, MATRIX* out, s32 roll)
{
    enum {
        GRAPHICS_DIRECTION_ANGLE_MASK    = ONE - 1,
        GRAPHICS_DIRECTION_FRACTION_BITS = 12
    };
    _GfxDirectionRotationScratch* scratch;
    MATRIX*                       rotation;
    MATRIX*                       axisRotation;
    s32                           yawSine;
    s32                           yaw;
    s32                           pitch;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GfxDirectionRotationScratch);
    // The SDK's unqualified normalization input is only read.
    VectorNormalS((VECTOR*)direction, &scratch->work.direction);

    // Measure the heading, then the elevation against the horizontal length.
    rotation     = &scratch->rotation;
    yaw          = ratan2(scratch->work.direction.vx, scratch->work.direction.vz) & GRAPHICS_DIRECTION_ANGLE_MASK;
    scratch->yaw = yaw;
    yawSine      = rsin(yaw);
    scratch->pitch =
        ratan2(scratch->work.direction.vy,
               (scratch->work.direction.vx * yawSine + scratch->work.direction.vz * rcos(scratch->yaw)) >> GRAPHICS_DIRECTION_FRACTION_BITS) &
        GRAPHICS_DIRECTION_ANGLE_MASK;

    scratch->work.angles.vx = 0;
    scratch->work.angles.vz = 0;
    scratch->work.angles.vy = scratch->yaw;
    RotMatrix(&scratch->work.angles, rotation);

    axisRotation            = &scratch->axisRotation;
    pitch                   = scratch->pitch;
    scratch->work.angles.vx = -pitch;
    scratch->work.angles.vy = 0;
    scratch->work.angles.vz = 0;
    RotMatrix(&scratch->work.angles, axisRotation);

    // Compose yaw and negative pitch before applying the caller's roll.
    _gfxMultiplyDirectionRotations(rotation, axisRotation, rotation);

    scratch->work.angles.vx = 0;
    scratch->work.angles.vy = 0;
    scratch->work.angles.vz = roll;
    RotMatrix(&scratch->work.angles, axisRotation);

    // The caller's rotation is that product times Rz(roll); its translation is untouched.
    _gfxMultiplyDirectionRotations(rotation, axisRotation, out);

    SCRATCH_STACK_RELEASE_BLOCK(_GfxDirectionRotationScratch);
}
