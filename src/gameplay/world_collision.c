#include "gameplay/world_collision.h"

#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor.h"
#include "gameplay/collision.h"
#include "collision.h"
#include "item_use.h"
#include "player_actor.h"
#include "world_collision.h"

#include "main/scratch.h"
#include "main/session.h"
#include "main/task_types.h"

/// Claims an entry and marks its receiving-body index for a pair-contact writer.
///
/// `rec` must be a modifiable pointer into `obj`'s initialized contact table.
/// Both arguments must be stable expressions: they are evaluated repeatedly.
/// This macro advances `rec`, and returns from its enclosing void function on
/// exhaustion or a missing reciprocal table. Single-contact mode replaces the
/// first entry and clears the previous body contact using its encoded address.
/// Its inline helper and fixed labels require one expansion per function.
#define WORLD_COLLISION_CLAIM_CONTACT(rec, obj)                                                                                            \
    do {                                                                                                                                   \
        WorldCollisionContact* _other;                                                                                                     \
        u16                    _recFlags;                                                                                                  \
                                                                                                                                           \
        if ((obj)->flags & WORLD_COLLISION_BODY_SINGLE_CONTACT) {                                                                          \
            _recFlags = (rec)->flags;                                                                                                      \
            if (!(_recFlags & WORLD_COLLISION_CONTACT_OCCUPIED)) {                                                                         \
                (rec)->flags = _recFlags | (((obj)->flags & WORLD_COLLISION_CONTACT_BODY_INDEX_MASK) + WORLD_COLLISION_CONTACT_OCCUPIED);  \
            } else {                                                                                                                       \
                if (((rec)->key.value & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_GRID) {                              \
                    _other = _worldCollisionGetObjectContacts(                                                                             \
                        (WorldCollisionBody*)((((rec)->response.bodyAddress.high << 16) & 0xFFFF0000) | (rec)->response.bodyAddress.low)); \
                    if (_other == NULL) {                                                                                                  \
                        return;                                                                                                            \
                    }                                                                                                                      \
                    for (;;) {                                                                                                             \
                        if (_other->key.value == (obj)->key) {                                                                             \
                            goto _found;                                                                                                   \
                        }                                                                                                                  \
                        if (_other->flags & WORLD_COLLISION_CONTACT_LAST) {                                                                \
                            return;                                                                                                        \
                        }                                                                                                                  \
                        _other++;                                                                                                          \
                    }                                                                                                                      \
                _found:                                                                                                                    \
                    _other->key.value             = 0;                                                                                     \
                    _other->distance              = 0;                                                                                     \
                    _other->point.vx              = 0;                                                                                     \
                    _other->point.vy              = 0;                                                                                     \
                    _other->point.vz              = 0;                                                                                     \
                    _other->response.direction.vx = 0;                                                                                     \
                    _other->response.direction.vy = 0;                                                                                     \
                    _other->response.direction.vz = 0;                                                                                     \
                    _other->flags                &= ~WORLD_COLLISION_CONTACT_OCCUPIED;                                                     \
                }                                                                                                                          \
                (rec)->flags |= ((obj)->flags & WORLD_COLLISION_CONTACT_BODY_INDEX_MASK) + WORLD_COLLISION_CONTACT_OCCUPIED;               \
            }                                                                                                                              \
        } else {                                                                                                                           \
            for (;;) {                                                                                                                     \
                _recFlags = (rec)->flags;                                                                                                  \
                if (!(_recFlags & WORLD_COLLISION_CONTACT_OCCUPIED)) {                                                                     \
                    goto _free;                                                                                                            \
                }                                                                                                                          \
                if (_recFlags & WORLD_COLLISION_CONTACT_LAST) {                                                                            \
                    return;                                                                                                                \
                }                                                                                                                          \
                (rec)++;                                                                                                                   \
            }                                                                                                                              \
        _free:                                                                                                                             \
            (rec)->flags = _recFlags | (((obj)->flags & WORLD_COLLISION_CONTACT_BODY_INDEX_MASK) + WORLD_COLLISION_CONTACT_OCCUPIED);      \
        }                                                                                                                                  \
    } while (0)

/// What one body of a colliding pair learns about the other, as a pair test hands it to the contact writer.
///
/// These are the fields of a `WorldCollisionContact` that depend on the shapes
/// tested. The writer copies them unchanged into the entry it claims and takes
/// the key and flags from the two bodies. A test fills this once per body,
/// because the two sides of one contact are described differently:
///
/// - Sphere against sphere, either body: `point` is the other sphere's centre,
///   `response` is zero and `distance` is the sum of both radii.
/// - Sphere against capsule, for the sphere: `point` is the capsule's second
///   endpoint and `response.direction` its unit axis towards the first endpoint.
/// - Sphere against capsule, for the capsule: `point` is the axis point nearest
///   the sphere, or the sphere's centre when the capsule tapers. `response.bodyAddress`
///   is the sphere body's address under `WORLD_COLLISION_BODY_SINGLE_CONTACT`,
///   and zero otherwise.
///
/// The capsule's first endpoint is its earlier clip contact when it has one.
typedef struct {
    SVECTOR                       point;    // World position, truncated to signed halfwords
    WorldCollisionContactResponse response; // Axis direction (4096 per unit), encoded body address, or zero
    s16                           distance; // Summed sphere radii in world units; 0 for capsule pairs
} _WorldCollisionPairContact;
STATIC_ASSERT_SIZEOF(_WorldCollisionPairContact, 0x12);

/// Scratch storage for sphere-pair intersection and its contact result.
typedef struct {
    _WorldCollisionPairContact contact;
    byte                       field_12[2]; // Role unproven
    VECTOR                     pos0;
    VECTOR                     pos1;
    VECTOR                     delta;
    s32                        rsum32;
} _WorldCollisionSphereScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionSphereScratch, 0x48);

/// Scratch for one sphere-against-capsule pair test.
///
/// The scratch stack reserves this block for the test and releases it on
/// every exit. Positions use game units. `segmentDirection` uses 4096 per
/// unit and points from endpoint 1 toward endpoint 0. Endpoint 0 is an
/// earlier clip contact when the capsule has one, and the geometric end
/// otherwise. The sphere centre must lie between `extendedEnd0` and
/// `extendedEnd1`, those endpoints moved apart by the sphere radius.
/// `axisPoint` is the centre's projection onto that axis. A taper measures
/// the segment with `segmentDelta` and interpolates the capsule radius,
/// restoring geometric endpoint 0 from the capsule when a contact replaced
/// it. `work` reuses one vector for the radius displacement, the restored
/// local endpoint, the projection offset and the centre-to-axis vector.
/// Each SDK vector's fourth component is unused and left uninitialized.
/// `contact` is filled once for each body of the pair.
typedef struct {
    _WorldCollisionPairContact contact;          // Per-body result copied into the contact table
    VECTOR                     sphereCenter;     // Sphere body's world centre
    VECTOR                     ends[2];          // World-space capsule segment, [0] then [1]
    VECTOR                     extendedEnd0;     // Endpoint 0 moved away from endpoint 1 by the sphere radius
    VECTOR                     extendedEnd1;     // Endpoint 1 moved away from endpoint 0 by the sphere radius
    VECTOR                     segmentDelta;     // Endpoint 0 minus endpoint 1, game units; tapered length only
    SVECTOR                    segmentDirection; // From endpoint 1 toward endpoint 0; 4096 per unit
    SVECTOR                    axisPoint;        // Sphere centre projected onto the axis, truncated to signed 16 bits
    union {
        SVECTOR radiusAlongSegment;              // Sphere radius as a displacement along segmentDirection
        SVECTOR localEndpoint;                   // Local endpoint 0 plus the body's position, before rotation
        SVECTOR projectionOffset;                // segmentDirection times the distance from extendedEnd1
        SVECTOR centreToAxis;                    // axisPoint minus the sphere centre
    } work;
} _WorldCollisionCapsuleScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionCapsuleScratch, 0x8C);

/// 0x88-byte scratch from the scratch stack used by `Gp_CollideObjGrid`.
/// `pos` is the object's world position (`Gp_ObjWorldPos`) and `grid` its cell
/// (`Gp_LocalToGrid`). `verts` holds the face corners rotated by
/// `Gp_GridParams->viewCoord->workm` and translated by that matrix, `normal` the
/// rotated face normal. Per edge, `delta` is the corner difference, `unit` its
/// `VectorNormal`, and `delta` is then reused for the `normal x unit` inward
/// edge plane.
typedef struct _GpGridHitScratch {
    /* 0x00 */ SVECTOR3 grid;
    /* 0x06 */ s16      pad_6;
    /* 0x08 */ VECTOR3  pos;
    /* 0x14 */ s32      pad_14;
    /* 0x18 */ VECTOR   verts[4];
    /* 0x58 */ VECTOR   normal;
    /* 0x68 */ VECTOR   unit;
    /* 0x78 */ VECTOR   delta;
} GpGridHitScratch;
STATIC_ASSERT_SIZEOF(GpGridHitScratch, 0x88);

/// Transformed grid-face geometry and edge work for a segment intersection test.
///
/// Corners and directions use the query space given by the grid's cached
/// transform; only corners receive its translation. Triangle tests initialize
/// three corners, quad tests all four. The scratch stack reserves this entire
/// block for one face test and releases it on every exit; its contents are not
/// retained. `edgeWork` changes units when the edge-plane normal replaces the
/// displacement, without changing its signed 32-bit component representation.
typedef struct {
    VECTOR corners[4];    // Face corners in query space, in game-coordinate units
    VECTOR faceNormal;    // Rotated face normal, with 4096 representing one unit
    VECTOR edgeDirection; // Normalized end-minus-start edge direction, with 4096 per unit
    VECTOR edgeWork;      // Edge displacement in game units, then outward edge-plane normal with 4096 per unit
} _WorldCollisionGridRayScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionGridRayScratch, 0x70);

s32 Gp_PendingObj4CFlag;

s32 Gp_PairHandler1(WorldCollisionBody* arg0, WorldCollisionBody* arg1, s32 kind);

s32 Gp_PairHandler3(WorldCollisionBody* arg0, WorldCollisionBody* arg1, s32 kind);

static void Gp_RunPairHandler(WorldCollisionBody* node);

static void _worldCollisionRecordPairContact(WorldCollisionBody* receivingBody, WorldCollisionBody* contactedBody, _WorldCollisionPairContact* contact);

/// Contact table selected by the body's shape, or NULL for a body without one.
static inline WorldCollisionContact* _worldCollisionGetObjectContacts(WorldCollisionBody* obj)
{
    WorldCollisionContact* recs = NULL;

    switch (obj->flags & WORLD_COLLISION_BODY_KIND_MASK) {
        case WORLD_COLLISION_BODY_NONE:
            break;
        case WORLD_COLLISION_BODY_SPHERE:
            recs = obj->context.contacts;
            break;
        case WORLD_COLLISION_BODY_CONTACT_PROXY:
            recs = obj->context.contactOwner->context.contacts;
            break;
        case WORLD_COLLISION_BODY_CAPSULE:
            recs = obj->context.capsule->contacts;
            break;
        case WORLD_COLLISION_BODY_MOTION_SPHERE:
            recs = obj->context.motion->contacts;
            break;
    }
    return recs;
}

GpPairFn Gp_PairHandlers[5] = {
    Gp_PairNop,
    Gp_PairHandler1,
    Gp_PairNop,
    Gp_PairHandler3,
    Gp_PairNop,
};
GpPairRule D_8010FA4C[4][4] = {
    { { 1, 0 }, { 2, 0 }, { 3, 0 }, { 1, 0 } },
    { { 2, 1 }, { 0, 0 }, { 4, 1 }, { 2, 1 } },
    { { 3, 1 }, { 4, 0 }, { 0, 0 }, { 3, 1 } },
    { { 1, 0 }, { 2, 0 }, { 3, 0 }, { 1, 0 } },
};

void Gp_TickWorldCollision(Task* unused)
{
    if (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER) != NULL) {
        Gp_UpdatePlayerMove();
        Gp_CollideListGrid(Gp_ObjList0);
        Gp_CollideListGrid(Gp_ObjList1);
        Gp_CollideListGrid(Gp_ObjList2);
        Gp_CollideListGrid(Gp_ObjList3);
        Gp_CollideListGrid(Gp_ObjList4);
        Gp_CollideListGrid(Gp_ObjList7);
        Gp_CollideListGrid(Gp_ObjList8);
        Gp_CollideLists(Gp_ObjList0, Gp_ObjList2);
        Gp_CollideLists(Gp_ObjList0, Gp_ObjList3);
        Gp_CollideLists(Gp_ObjList0, Gp_ObjList4);
        Gp_CollideLists(Gp_ObjList0, Gp_ObjList8);
        Gp_RunPairHandler(Gp_ObjList0);
        Gp_CollideLists(Gp_ObjList1, Gp_ObjList2);
        Gp_CollideLists(Gp_ObjList1, Gp_ObjList4);
        Gp_CollideLists(Gp_ObjList1, Gp_ObjList6);
        Gp_CollideLists(Gp_ObjList2, Gp_ObjList4);
        Gp_CollideLists(Gp_ObjList2, Gp_ObjList8);
        Gp_RunPairHandler(Gp_ObjList2);
        Gp_CollideLists(Gp_ObjList3, Gp_ObjList4);
        Gp_CollideLists(Gp_ObjList4, Gp_ObjList8);
        if (Gp_PendingObj4CFlag != 0) {
            Gp_ClearPendingObj4C();
        }
        func_800E0608(Gp_ObjList0, WORLD_COLLISION_BODY_PAIR_ENABLED | WORLD_COLLISION_BODY_ROOM_TRIGGER_ENABLED | WORLD_COLLISION_BODY_KIND_MASK,
                      WORLD_COLLISION_BODY_PAIR_ENABLED | WORLD_COLLISION_BODY_ROOM_TRIGGER_ENABLED | WORLD_COLLISION_BODY_MOTION_SPHERE);
        if (gGameSession->suppressViewTriggers == 0) {
            func_800E06AC(Gp_ObjList0, WORLD_COLLISION_BODY_PAIR_ENABLED | WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED | WORLD_COLLISION_BODY_KIND_MASK,
                          WORLD_COLLISION_BODY_PAIR_ENABLED | WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED | WORLD_COLLISION_BODY_MOTION_SPHERE);
        }
    }
}

static void Gp_RunPairHandler(WorldCollisionBody* node)
{
    WorldCollisionBody* other;
    GpPairRule*         rec;
    s32                 rowOff;
    s32                 temp;
    u16                 flags;
    u16                 handler;
    u16                 swap;
    u8                  kind;
    u8                  otherKind;

    for (; node != NULL; node = node->next) {
        flags = node->flags;
        other = node->next;
        if (flags & WORLD_COLLISION_BODY_PAIR_ENABLED) {
            kind = (node->flags & WORLD_COLLISION_BODY_KIND_MASK) - 1;
            if (other != NULL) {
                rowOff = kind << 4;
                for (; other != NULL; other = other->next) {
                    if (other->flags & WORLD_COLLISION_BODY_PAIR_ENABLED) {
                        otherKind = (other->flags & WORLD_COLLISION_BODY_KIND_MASK) - 1;
                        temp      = (otherKind << 2) + rowOff;
                        rec       = &D_8010FA4C[0][0] + (temp >> 2);
                        swap      = rec->swap;
                        handler   = rec->handler;
                        if (swap == 0) {
                            Gp_PairHandlers[handler](node, other, handler);
                        } else {
                            Gp_PairHandlers[handler](other, node, handler);
                        }
                    }
                }
            }
        }
    }
}

/// Records an identified pair contact in the receiving body's initialized table.
static void _worldCollisionRecordPairContact(WorldCollisionBody* receivingBody, WorldCollisionBody* contactedBody, _WorldCollisionPairContact* contact)
{
    WorldCollisionContact* rec;

    if (contactedBody->key == 0) {
        return;
    }

    rec = _worldCollisionGetObjectContacts(receivingBody);
    if (rec == NULL) {
        return;
    }

    WORLD_COLLISION_CLAIM_CONTACT(rec, receivingBody);

    rec->key.value = contactedBody->key;
    rec->distance  = contact->distance;
    rec->point     = contact->point;
    rec->response  = contact->response;
}

s32 Gp_PairHandler1(WorldCollisionBody* arg0, WorldCollisionBody* arg1, s32 kind)
{
    u8*                           head;
    _WorldCollisionSphereScratch* block;
    s32                           ret;

    head                                               = SCRATCH_STACK_CURSOR(u8);
    block                                              = (_WorldCollisionSphereScratch*)(head - sizeof(_WorldCollisionSphereScratch));
    SCRATCH_STACK_CURSOR(_WorldCollisionSphereScratch) = block;
    Gp_ObjWorldPos(arg0, (VECTOR3*)&block->pos0);
    Gp_ObjWorldPos(arg1, (VECTOR3*)&block->pos1);

    ret             = 0;
    block->delta.vx = block->pos0.vx - block->pos1.vx;
    block->delta.vy = block->pos0.vy - block->pos1.vy;
    block->delta.vz = block->pos0.vz - block->pos1.vz;
    if ((ABS(block->delta.vx) > 0x7FFF) || (ABS(block->delta.vz) > 0x7FFF)) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionSphereScratch);
        return 0;
    }

    block->rsum32 = arg0->radius + arg1->radius;
    if (block->delta.vx * block->delta.vx + block->delta.vy * block->delta.vy + block->delta.vz * block->delta.vz < block->rsum32 * block->rsum32) {
        block->contact.point.vx              = block->pos1.vx;
        block->contact.point.vy              = block->pos1.vy;
        block->contact.point.vz              = block->pos1.vz;
        block->contact.response.direction.vx = 0;
        block->contact.response.direction.vy = 0;
        block->contact.response.direction.vz = 0;
        block->contact.distance              = block->rsum32;
        _worldCollisionRecordPairContact(arg0, arg1, &block->contact);
        ret = 1;

        block->contact.point.vx              = block->pos0.vx;
        block->contact.point.vy              = block->pos0.vy;
        block->contact.point.vz              = block->pos0.vz;
        block->contact.response.direction.vx = 0;
        block->contact.response.direction.vy = 0;
        block->contact.response.direction.vz = 0;
        block->contact.distance              = block->rsum32;
        _worldCollisionRecordPairContact(arg1, arg0, &block->contact);
    }

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionSphereScratch);
    return ret;
}

s32 Gp_PairHandler3(WorldCollisionBody* arg0, WorldCollisionBody* arg1, s32 kind)
{
    // Collision records encode the source address as two halfwords.
    union {
        WorldCollisionBody* object;
        s32                 address;
    } sourceAddress;
    u8*                            head;
    _WorldCollisionCapsuleScratch* block;
    WorldCollisionCapsule*         rec;
    s32                            proj;
    s32                            ret;
    s32                            tapered;
    VECTOR3*                       sphereCenter;
    s32                            radiusSquared;
    s32                            dx0;
    s32                            dy0;
    s32                            dz0;
    s32                            dx1;
    s32                            dy1;
    s32                            dz1;
    s32                            dx2;
    s32                            dy2;
    s32                            dz2;
    s32                            dx3;
    s32                            dy3;
    s32                            dz3;
    s32                            dx4;
    s32                            dy4;
    s32                            dz4;
    s32                            len;
    s32                            plen;
    s32                            r0;
    s32                            r1;
    s32                            tmp; // combined radius on a straight capsule, taper ratio less 1.0 on a tapered one

    // Address the centre from the cursor before the reservation is stored.
    // Taking &block->sphereCenter after that store does not keep this order.
    head                       = SCRATCH_STACK_CURSOR(u8);
    sphereCenter               = (VECTOR3*)&((_WorldCollisionCapsuleScratch*)(head - sizeof(_WorldCollisionCapsuleScratch)))->sphereCenter;
    SCRATCH_STACK_CURSOR(void) = head - sizeof(_WorldCollisionCapsuleScratch);
    rec                        = arg1->context.capsule;
    block                      = (_WorldCollisionCapsuleScratch*)(head - sizeof(_WorldCollisionCapsuleScratch));
    // Place the sphere centre and the capsule segment in world space.
    Gp_ObjWorldPos(arg0, sphereCenter);
    func_800DEC80(arg1, block->ends, &block->segmentDirection, 0);

    block->work.radiusAlongSegment.vx = (block->segmentDirection.vx * arg0->radius) >> 12;
    block->work.radiusAlongSegment.vy = (block->segmentDirection.vy * arg0->radius) >> 12;
    block->work.radiusAlongSegment.vz = (block->segmentDirection.vz * arg0->radius) >> 12;

    block->extendedEnd0.vx = block->ends[0].vx + block->work.radiusAlongSegment.vx;
    block->extendedEnd0.vy = block->ends[0].vy + block->work.radiusAlongSegment.vy;
    block->extendedEnd0.vz = block->ends[0].vz + block->work.radiusAlongSegment.vz;
    block->extendedEnd1.vx = block->ends[1].vx - block->work.radiusAlongSegment.vx;
    block->extendedEnd1.vy = block->ends[1].vy - block->work.radiusAlongSegment.vy;
    block->extendedEnd1.vz = block->ends[1].vz - block->work.radiusAlongSegment.vz;

    // Reject a centre outside the segment extended by the sphere radius.
    dx0 = (block->sphereCenter.vx - block->extendedEnd0.vx) * block->segmentDirection.vx;
    dy0 = (block->sphereCenter.vy - block->extendedEnd0.vy) * block->segmentDirection.vy;
    dz0 = (block->sphereCenter.vz - block->extendedEnd0.vz) * block->segmentDirection.vz;
    ret = 0;
    if (dx0 + dy0 + dz0 > 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionCapsuleScratch);
        return 0;
    }

    dx1  = (block->sphereCenter.vx - block->extendedEnd1.vx) * block->segmentDirection.vx;
    dy1  = (block->sphereCenter.vy - block->extendedEnd1.vy) * block->segmentDirection.vy;
    dz1  = (block->sphereCenter.vz - block->extendedEnd1.vz) * block->segmentDirection.vz;
    proj = (dx1 + dy1 + dz1) >> 12;
    if (proj <= 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionCapsuleScratch);
        return 0;
    }

    r1      = rec->end0Radius;
    tapered = r1 != rec->end1Radius;
    // Equal radii share one combined radius. A taper restores geometric
    // endpoint 0 when a contact replaced it, then interpolates the radius.
    if (!tapered) {
        tmp                 = arg0->radius + r1;
        block->axisPoint.vx = (u16)block->extendedEnd1.vx + ((block->segmentDirection.vx * proj) >> 12);
        block->axisPoint.vy = (u16)block->extendedEnd1.vy + ((block->segmentDirection.vy * proj) >> 12);
        block->axisPoint.vz = (u16)block->extendedEnd1.vz + ((block->segmentDirection.vz * proj) >> 12);
        proj                = tmp;
        goto check;
    }

    if (arg1->flags & (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT)) {
        gte_SetRotMatrix(&arg1->coord->workm);
        block->work.localEndpoint.vx = (u16)rec->ends[0].vx + (u16)arg1->pos.vx;
        block->work.localEndpoint.vy = (u16)rec->ends[0].vy + (u16)arg1->pos.vy;
        block->work.localEndpoint.vz = (u16)rec->ends[0].vz + (u16)arg1->pos.vz;
        gte_ldv0(&block->work.localEndpoint);
        gte_rtv0();
        gte_stlvnl(&block->ends[0]);
        block->ends[0].vx += (arg1->coord)->workm.t[0];
        block->ends[0].vy += (arg1->coord)->workm.t[1];
        block->ends[0].vz += (arg1->coord)->workm.t[2];
    }

    block->segmentDelta.vx = block->ends[0].vx - block->ends[1].vx;
    block->segmentDelta.vy = block->ends[0].vy - block->ends[1].vy;
    block->segmentDelta.vz = block->ends[0].vz - block->ends[1].vz;
    dx2                    = block->segmentDelta.vx * block->segmentDelta.vx;
    dy2                    = block->segmentDelta.vy * block->segmentDelta.vy;
    dz2                    = block->segmentDelta.vz * block->segmentDelta.vz;
    len                    = SquareRoot0(dx2 + dy2 + dz2);

    block->work.projectionOffset.vx = (block->segmentDirection.vx * proj) >> 12;
    block->work.projectionOffset.vy = (block->segmentDirection.vy * proj) >> 12;
    block->work.projectionOffset.vz = (block->segmentDirection.vz * proj) >> 12;
    dx3                             = block->work.projectionOffset.vx * block->work.projectionOffset.vx;
    dy3                             = block->work.projectionOffset.vy * block->work.projectionOffset.vy;
    dz3                             = block->work.projectionOffset.vz * block->work.projectionOffset.vz;
    proj                            = len;
    plen                            = SquareRoot0(dx3 + dy3 + dz3);

    r0                  = (rec->end0Radius << 12) / rec->end1Radius;
    proj                = (plen << 12) / proj;
    tmp                 = r0 - 0x1000;
    r1                  = arg0->radius;
    proj                = r1 + ((((tmp * proj) >> 12) * rec->end1Radius >> 12) + rec->end1Radius);
    block->axisPoint.vx = (u16)block->work.projectionOffset.vx + (u16)block->extendedEnd1.vx;
    block->axisPoint.vy = (u16)block->work.projectionOffset.vy + (u16)block->extendedEnd1.vy;
    block->axisPoint.vz = (u16)block->work.projectionOffset.vz + (u16)block->extendedEnd1.vz;

check:
    block->work.centreToAxis.vx = (u16)block->axisPoint.vx - (u16)block->sphereCenter.vx;
    block->work.centreToAxis.vy = (u16)block->axisPoint.vy - (u16)block->sphereCenter.vy;
    block->work.centreToAxis.vz = (u16)block->axisPoint.vz - (u16)block->sphereCenter.vz;
    dx4                         = block->work.centreToAxis.vx * block->work.centreToAxis.vx;
    dy4                         = block->work.centreToAxis.vy * block->work.centreToAxis.vy;
    dz4                         = block->work.centreToAxis.vz * block->work.centreToAxis.vz;
    radiusSquared               = proj * proj;
    if (dx4 + dy4 + dz4 < radiusSquared) {
        // The sphere records endpoint 1 and the segment direction. The capsule
        // records the axis point, or the sphere centre when the capsule tapers.
        block->contact.distance              = 0;
        block->contact.point.vx              = (u16)block->ends[1].vx;
        block->contact.point.vy              = (u16)block->ends[1].vy;
        block->contact.point.vz              = (u16)block->ends[1].vz;
        block->contact.response.direction.vx = (u16)block->segmentDirection.vx;
        block->contact.response.direction.vy = (u16)block->segmentDirection.vy;
        block->contact.response.direction.vz = (u16)block->segmentDirection.vz;
        _worldCollisionRecordPairContact(arg0, arg1, &block->contact);
        if (!tapered) {
            block->contact.point = block->axisPoint;
        } else {
            block->contact.point.vx = (u16)block->sphereCenter.vx;
            block->contact.point.vy = (u16)block->sphereCenter.vy;
            block->contact.point.vz = (u16)block->sphereCenter.vz;
        }
        if (arg1->flags & WORLD_COLLISION_BODY_SINGLE_CONTACT) {
            sourceAddress.object                     = arg0;
            block->contact.response.bodyAddress.low  = sourceAddress.address;
            block->contact.response.bodyAddress.high = sourceAddress.address >> 16;
        } else {
            block->contact.response.direction.vx = 0;
            block->contact.response.direction.vy = 0;
        }
        block->contact.response.direction.vz = 0;
        block->contact.distance              = 0;
        _worldCollisionRecordPairContact(arg1, arg0, &block->contact);
        ret = 1;
    }

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionCapsuleScratch);
    return ret;
}

void Gp_CollideObjGrid(WorldCollisionBody* arg0)
{
    u8*                     head;
    GpGridHitScratch*       block;
    VECTOR3*                pos;
    WorldCollisionGridFace* face;
    WorldCollisionContact*  slot;
    s16*                    cell;
    s32                     id;
    s32                     i;
    s32                     n;
    s32                     outside;
    s32                     val;
    s32                     faceDot;
    s32                     edgeDot;
    u16                     dist;
    u16                     flags;

    head                       = SCRATCH_STACK_CURSOR(u8);
    pos                        = (VECTOR3*)(head - 0x80);
    SCRATCH_STACK_CURSOR(void) = head - 0x88;
    block                      = (GpGridHitScratch*)(head - 0x88);
    Gp_ObjWorldPos(arg0, pos);
    Gp_LocalToGrid(pos, &block->grid);

    if ((u16)block->grid.vx < Gp_GridParams->cellCountX && (u16)block->grid.vz < Gp_GridParams->cellCountZ) {
        cell = Gp_GridParams->cellFaceIds[block->grid.vx * Gp_GridParams->cellCountZ + block->grid.vz];
        if (cell != NULL) {
            for (;;) {
                id = *cell;
                if (id == WORLD_COLLISION_GRID_CELL_END) {
                    goto done;
                }
                face = &Gp_GridParams->faces[id];
                if (face->vertexIndices[0] == 0 && face->vertexIndices[1] == 0) {
                    cell++;
                    continue;
                }

                gte_SetRotMatrix(&Gp_GridParams->viewCoord->workm);
                gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[0]]);
                gte_rtv0();
                gte_stlvnl(&block->verts[0]);
                block->verts[0].vx += Gp_GridParams->viewCoord->workm.t[0];
                block->verts[0].vy += Gp_GridParams->viewCoord->workm.t[1];
                block->verts[0].vz += Gp_GridParams->viewCoord->workm.t[2];

                gte_ldv0(&Gp_GridParams->normals[face->normalIndex]);
                gte_rtv0();
                gte_stlvnl(&block->normal);

                faceDot = (block->normal.vx * block->verts[0].vx + block->normal.vy * block->verts[0].vy +
                           block->normal.vz * block->verts[0].vz) >>
                          12;
                dist = ((block->normal.vx * block->pos.vx + block->normal.vy * block->pos.vy +
                         block->normal.vz * block->pos.vz) >>
                        12) -
                       faceDot;
                if (arg0->radius >= ABS((s16)dist)) {
                    goto edges;
                }
                goto next_face;

            mark_outside:
                outside = 1;
                goto edges_done;

            fill:
                slot->flags              = flags | WORLD_COLLISION_CONTACT_OCCUPIED;
                slot->distance           = arg0->radius - dist;
                slot->key.value          = face->surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                slot->point.vx           = 0;
                slot->point.vy           = 0;
                slot->point.vz           = 0;
                slot->response.direction = Gp_GridParams->normals[face->normalIndex];
                goto next_face;

            edges:
                n = (face->vertexIndices[3] != WORLD_COLLISION_GRID_FACE_NO_VERTEX) ? 4 : 3;
                for (i = 1; i < n; i++) {
                    gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[i]]);
                    gte_rtv0();
                    gte_stlvnl(&block->verts[i]);
                    block->verts[i].vx += Gp_GridParams->viewCoord->workm.t[0];
                    block->verts[i].vy += Gp_GridParams->viewCoord->workm.t[1];
                    block->verts[i].vz += Gp_GridParams->viewCoord->workm.t[2];
                }

                outside = 0;
                for (i = n - 3; i < n * 2 - 3; i++) {
                    block->delta.vx =
                        block->verts[Gp_FaceEdgePairs[i].endCornerIndex].vx - block->verts[Gp_FaceEdgePairs[i].startCornerIndex].vx;
                    block->delta.vy =
                        block->verts[Gp_FaceEdgePairs[i].endCornerIndex].vy - block->verts[Gp_FaceEdgePairs[i].startCornerIndex].vy;
                    block->delta.vz =
                        block->verts[Gp_FaceEdgePairs[i].endCornerIndex].vz - block->verts[Gp_FaceEdgePairs[i].startCornerIndex].vz;
                    VectorNormal(&block->delta, &block->unit);
                    gte_ldopv1(&block->normal);
                    gte_ldopv2(&block->unit);
                    gte_op12();
                    gte_stlvnl(&block->delta);

                    edgeDot = (block->delta.vx * block->verts[Gp_FaceEdgePairs[i].endCornerIndex].vx +
                               block->delta.vy * block->verts[Gp_FaceEdgePairs[i].endCornerIndex].vy +
                               block->delta.vz * block->verts[Gp_FaceEdgePairs[i].endCornerIndex].vz) >>
                              12;
                    val = (s16)(((block->delta.vx * block->pos.vx + block->delta.vy * block->pos.vy +
                                  block->delta.vz * block->pos.vz) >>
                                 12) -
                                edgeDot);
                    if (val - 10 > 0) {
                        goto mark_outside;
                    }
                }
            edges_done:
                if (outside) {
                    cell++;
                    continue;
                }

                slot = arg0->context.contacts;
                for (;;) {
                    flags = slot->flags;
                    if (!(flags & WORLD_COLLISION_CONTACT_OCCUPIED)) {
                        goto fill;
                    }
                    if (flags & WORLD_COLLISION_CONTACT_LAST) {
                        goto done;
                    }
                    slot++;
                }

            next_face:
                cell++;
            }
        }
    }

done:
    SCRATCH_STACK_RELEASE_BYTES(0x88);
}

void Gp_CollideObjGridDir(WorldCollisionBody* arg0)
{
    u8*                          head;
    GpGridHitScratch*            block;
    VECTOR3*                     pos;
    WorldCollisionGridFace*      face;
    WorldCollisionContact*       slot;
    WorldCollisionMotionContext* motionContext;
    s16*                         cell;
    s32                          id;
    s32                          i;
    s32                          n;
    s32                          outside;
    s32                          val;
    s32                          faceDot;
    s32                          edgeDot;
    u16                          dist;
    s32                          extra;
    s32                          faceKind;
    u16                          flags;

    head                       = SCRATCH_STACK_CURSOR(u8);
    pos                        = (VECTOR3*)(head - 0x80);
    SCRATCH_STACK_CURSOR(void) = head - 0x88;
    block                      = (GpGridHitScratch*)(head - 0x88);
    motionContext              = arg0->context.motion;
    Gp_ObjWorldPos(arg0, pos);
    Gp_LocalToGrid(pos, &block->grid);

    if ((u16)block->grid.vx < Gp_GridParams->cellCountX && (u16)block->grid.vz < Gp_GridParams->cellCountZ) {
        cell = Gp_GridParams->cellFaceIds[block->grid.vx * Gp_GridParams->cellCountZ + block->grid.vz];
        if (cell != NULL) {
            for (;;) {
                id = *cell;
                if (id == WORLD_COLLISION_GRID_CELL_END) {
                    goto done;
                }
                face = &Gp_GridParams->faces[id];
                if (face->vertexIndices[0] == 0 && face->vertexIndices[1] == 0) {
                    cell++;
                    continue;
                }
                if (Gp_GridParams->normals[face->normalIndex].vy < -0xDDA) {
                    cell++;
                    continue;
                }

                gte_SetRotMatrix(&Gp_GridParams->viewCoord->workm);
                gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[0]]);
                gte_rtv0();
                gte_stlvnl(&block->verts[0]);
                block->verts[0].vx += Gp_GridParams->viewCoord->workm.t[0];
                block->verts[0].vy += Gp_GridParams->viewCoord->workm.t[1];
                block->verts[0].vz += Gp_GridParams->viewCoord->workm.t[2];

                gte_ldv0(&Gp_GridParams->normals[face->normalIndex]);
                gte_rtv0();
                gte_stlvnl(&block->normal);

                if (motionContext->motionDirection.vx * block->normal.vx + motionContext->motionDirection.vy * block->normal.vy +
                        motionContext->motionDirection.vz * block->normal.vz >
                    0x280000) {
                    cell++;
                    continue;
                }

                faceDot = (block->normal.vx * block->verts[0].vx + block->normal.vy * block->verts[0].vy +
                           block->normal.vz * block->verts[0].vz) >>
                          12;
                dist = ((block->normal.vx * block->pos.vx + block->normal.vy * block->pos.vy +
                         block->normal.vz * block->pos.vz) >>
                        12) -
                       faceDot;
                if (arg0->radius >= ABS((s16)dist)) {
                    goto edges;
                }
                goto next_face;

            mark_outside:
                outside = 1;
                goto edges_done;

            edges:
                n = (face->vertexIndices[3] != WORLD_COLLISION_GRID_FACE_NO_VERTEX) ? 4 : 3;
                for (i = 1; i < n; i++) {
                    gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[i]]);
                    gte_rtv0();
                    gte_stlvnl(&block->verts[i]);
                    block->verts[i].vx += Gp_GridParams->viewCoord->workm.t[0];
                    block->verts[i].vy += Gp_GridParams->viewCoord->workm.t[1];
                    block->verts[i].vz += Gp_GridParams->viewCoord->workm.t[2];
                }

                extra   = 0;
                outside = 0;
                for (i = n - 3; i < n * 2 - 3; i++) {
                    block->delta.vx =
                        block->verts[Gp_FaceEdgePairs[i].endCornerIndex].vx - block->verts[Gp_FaceEdgePairs[i].startCornerIndex].vx;
                    block->delta.vy =
                        block->verts[Gp_FaceEdgePairs[i].endCornerIndex].vy - block->verts[Gp_FaceEdgePairs[i].startCornerIndex].vy;
                    block->delta.vz =
                        block->verts[Gp_FaceEdgePairs[i].endCornerIndex].vz - block->verts[Gp_FaceEdgePairs[i].startCornerIndex].vz;
                    VectorNormal(&block->delta, &block->unit);
                    gte_ldopv1(&block->normal);
                    gte_ldopv2(&block->unit);
                    gte_op12();
                    gte_stlvnl(&block->delta);

                    edgeDot = (block->delta.vx * block->verts[Gp_FaceEdgePairs[i].endCornerIndex].vx +
                               block->delta.vy * block->verts[Gp_FaceEdgePairs[i].endCornerIndex].vy +
                               block->delta.vz * block->verts[Gp_FaceEdgePairs[i].endCornerIndex].vz) >>
                              12;
                    val = (s16)(((block->delta.vx * block->pos.vx + block->delta.vy * block->pos.vy +
                                  block->delta.vz * block->pos.vz) >>
                                 12) -
                                edgeDot);
                    if (val - arg0->radius > 0) {
                        outside = 1;
                        goto edges_done;
                    }
                    if (val > 0) {
                        if ((s16)dist < 0) {
                            goto mark_outside;
                        }
                        extra = WORLD_COLLISION_CONTACT_GRID_EDGE;
                    }
                }
            edges_done:
                if (outside) {
                    cell++;
                    continue;
                }

                slot = arg0->context.motion->contacts;
                for (;;) {
                    flags = slot->flags;
                    if (flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
                        if ((slot->key.value & -0x100) == (extra | WORLD_COLLISION_CONTACT_GRID)) {
                            if (slot->response.direction.vx == Gp_GridParams->normals[face->normalIndex].vx &&
                                slot->response.direction.vy == Gp_GridParams->normals[face->normalIndex].vy &&
                                slot->response.direction.vz == Gp_GridParams->normals[face->normalIndex].vz) {
                                if (slot->distance < (s32)arg0->radius - (s16)dist) {
                                    slot->distance = arg0->radius - dist;
                                }
                                goto next_face;
                            }
                        }
                    } else {
                        slot->flags              = flags | WORLD_COLLISION_CONTACT_OCCUPIED;
                        slot->distance           = arg0->radius - dist;
                        faceKind                 = face->surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                        slot->key.value          = extra | faceKind;
                        slot->point.vx           = 0;
                        slot->point.vy           = 0;
                        slot->point.vz           = 0;
                        slot->response.direction = Gp_GridParams->normals[face->normalIndex];
                        goto next_face;
                    }
                    if (slot->flags & WORLD_COLLISION_CONTACT_LAST) {
                        goto done;
                    }
                    slot++;
                }

            next_face:
                cell++;
            }
        }
    }

done:
    SCRATCH_STACK_RELEASE_BYTES(0x88);
}

s32 func_800DD324(s32 faceId, VECTOR* seg, SVECTOR* ray, WorldCollisionBody* arg3)
{
    _WorldCollisionGridRayScratch* scratchEnd;
    _WorldCollisionGridRayScratch* scratch;
    WorldCollisionGridFace*        face;
    s32                            i;
    s32                            n;
    s16                            faceDot;
    s32                            denom;
    s32                            t;
    s32                            edgeDot;
    s32                            val;
    s32                            limit;

    scratchEnd                 = SCRATCH_STACK_CURSOR(_WorldCollisionGridRayScratch);
    SCRATCH_STACK_CURSOR(void) = scratchEnd - 1;
    face                       = &Gp_GridParams->faces[faceId];
    scratch                    = scratchEnd - 1;

    // Transform the face into the segment's query space.
    gte_SetRotMatrix(&Gp_GridParams->viewCoord->workm);
    gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[0]]);
    gte_rtv0();
    gte_stlvnl(&scratch->corners[0]);
    scratch->corners[0].vx += Gp_GridParams->viewCoord->workm.t[0];
    scratch->corners[0].vy += Gp_GridParams->viewCoord->workm.t[1];
    scratch->corners[0].vz += Gp_GridParams->viewCoord->workm.t[2];

    gte_ldv0(&Gp_GridParams->normals[face->normalIndex]);
    gte_rtv0();
    gte_stlvnl(&scratch->faceNormal);

    faceDot = (scratch->faceNormal.vx * scratch->corners[0].vx + scratch->faceNormal.vy * scratch->corners[0].vy +
               scratch->faceNormal.vz * scratch->corners[0].vz) >>
              12;
    denom = (scratch->faceNormal.vx * ray[0].vx + scratch->faceNormal.vy * ray[0].vy + scratch->faceNormal.vz * ray[0].vz) >> 12;

    if (denom >= 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridRayScratch);
        return 0;
    }
    if ((((scratch->faceNormal.vx * seg[1].vx + scratch->faceNormal.vy * seg[1].vy + scratch->faceNormal.vz * seg[1].vz) >> 12) -
         faceDot) <= 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridRayScratch);
        return 0;
    }
    t = -(((((scratch->faceNormal.vx * seg[0].vx + scratch->faceNormal.vy * seg[0].vy + scratch->faceNormal.vz * seg[0].vz) >> 12) -
            faceDot)
           << 12)) /
        denom;
    if (t >= 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridRayScratch);
        return 0;
    }

    ray[1].vx = seg[0].vx + ((ray[0].vx * t) >> 12);
    ray[1].vy = seg[0].vy + ((ray[0].vy * t) >> 12);
    ray[1].vz = seg[0].vz + ((ray[0].vz * t) >> 12);

    n = (face->vertexIndices[3] == WORLD_COLLISION_GRID_FACE_NO_VERTEX) ? 3 : (s32)ARRAY_SIZE(scratch->corners);

    gte_SetRotMatrix(&Gp_GridParams->viewCoord->workm);
    for (i = 1; i < n; i++) {
        gte_ldv0(&Gp_GridParams->vertices[face->vertexIndices[i]]);
        gte_rtv0();
        gte_stlvnl(&scratch->corners[i]);
        scratch->corners[i].vx += Gp_GridParams->viewCoord->workm.t[0];
        scratch->corners[i].vy += Gp_GridParams->viewCoord->workm.t[1];
        scratch->corners[i].vz += Gp_GridParams->viewCoord->workm.t[2];
    }

    // Reuse the displacement slot for each edge's outward Q12 plane normal.
    for (i = n - 3; i < n * 2 - 3; i++) {
        scratch->edgeWork.vx = scratch->corners[Gp_FaceEdgePairs[i].endCornerIndex].vx - scratch->corners[Gp_FaceEdgePairs[i].startCornerIndex].vx;
        scratch->edgeWork.vy = scratch->corners[Gp_FaceEdgePairs[i].endCornerIndex].vy - scratch->corners[Gp_FaceEdgePairs[i].startCornerIndex].vy;
        scratch->edgeWork.vz = scratch->corners[Gp_FaceEdgePairs[i].endCornerIndex].vz - scratch->corners[Gp_FaceEdgePairs[i].startCornerIndex].vz;
        VectorNormal(&scratch->edgeWork, &scratch->edgeDirection);
        gte_ldopv1(&scratch->faceNormal);
        gte_ldopv2(&scratch->edgeDirection);
        gte_op12();
        gte_stlvnl(&scratch->edgeWork);

        edgeDot = (scratch->edgeWork.vx * scratch->corners[Gp_FaceEdgePairs[i].endCornerIndex].vx +
                   scratch->edgeWork.vy * scratch->corners[Gp_FaceEdgePairs[i].endCornerIndex].vy +
                   scratch->edgeWork.vz * scratch->corners[Gp_FaceEdgePairs[i].endCornerIndex].vz) >>
                  12;
        limit = 5;
        val   = ((scratch->edgeWork.vx * ray[1].vx + scratch->edgeWork.vy * ray[1].vy + scratch->edgeWork.vz * ray[1].vz) >> 12) -
              edgeDot;
        if (arg3 != 0) {
            limit = 10;
        }
        if ((s16)val - limit > 0) {
            SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridRayScratch);
            return 0;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridRayScratch);
    return 1;
}
