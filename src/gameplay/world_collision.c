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
#define WORLD_COLLISION_CLAIM_CONTACT(rec, obj)                                                                                           \
    do {                                                                                                                                  \
        WorldCollisionContact* _other;                                                                                                    \
        u16                    _recFlags;                                                                                                 \
                                                                                                                                          \
        if ((obj)->flags & WORLD_COLLISION_BODY_SINGLE_CONTACT) {                                                                         \
            _recFlags = (rec)->flags;                                                                                                     \
            if (!(_recFlags & WORLD_COLLISION_CONTACT_OCCUPIED)) {                                                                        \
                (rec)->flags = _recFlags | (((obj)->flags & WORLD_COLLISION_CONTACT_BODY_INDEX_MASK) + WORLD_COLLISION_CONTACT_OCCUPIED); \
            } else {                                                                                                                      \
                if (((rec)->key.value & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_GRID) {                             \
                    _other = _worldCollisionGetObjectContacts(                                                                            \
                        (GpObj*)((((rec)->response.node.high << 16) & 0xFFFF0000) | (rec)->response.node.low));                           \
                    if (_other == NULL) {                                                                                                 \
                        return;                                                                                                           \
                    }                                                                                                                     \
                    for (;;) {                                                                                                            \
                        if (_other->key.value == (obj)->key) {                                                                            \
                            goto _found;                                                                                                  \
                        }                                                                                                                 \
                        if (_other->flags & WORLD_COLLISION_CONTACT_LAST) {                                                               \
                            return;                                                                                                       \
                        }                                                                                                                 \
                        _other++;                                                                                                         \
                    }                                                                                                                     \
                _found:                                                                                                                   \
                    _other->key.value          = 0;                                                                                       \
                    _other->distance           = 0;                                                                                       \
                    _other->point.vx           = 0;                                                                                       \
                    _other->point.vy           = 0;                                                                                       \
                    _other->point.vz           = 0;                                                                                       \
                    _other->response.normal.vx = 0;                                                                                       \
                    _other->response.normal.vy = 0;                                                                                       \
                    _other->response.normal.vz = 0;                                                                                       \
                    _other->flags             &= ~WORLD_COLLISION_CONTACT_OCCUPIED;                                                       \
                }                                                                                                                         \
                (rec)->flags |= ((obj)->flags & WORLD_COLLISION_CONTACT_BODY_INDEX_MASK) + WORLD_COLLISION_CONTACT_OCCUPIED;              \
            }                                                                                                                             \
        } else {                                                                                                                          \
            for (;;) {                                                                                                                    \
                _recFlags = (rec)->flags;                                                                                                 \
                if (!(_recFlags & WORLD_COLLISION_CONTACT_OCCUPIED)) {                                                                    \
                    goto _free;                                                                                                           \
                }                                                                                                                         \
                if (_recFlags & WORLD_COLLISION_CONTACT_LAST) {                                                                           \
                    return;                                                                                                               \
                }                                                                                                                         \
                (rec)++;                                                                                                                  \
            }                                                                                                                             \
        _free:                                                                                                                            \
            (rec)->flags = _recFlags | (((obj)->flags & WORLD_COLLISION_CONTACT_BODY_INDEX_MASK) + WORLD_COLLISION_CONTACT_OCCUPIED);     \
        }                                                                                                                                 \
    } while (0)

/// Position, response vector and distance passed from pair tests to their contact writer.
typedef struct {
    SVECTOR point;    // Other centre or capsule intersection, in world units
    SVECTOR response; // Capsule axis, encoded body address, or zero for spheres
    s16     distance; // Summed sphere radii, or zero for capsule hits
} _WorldCollisionPairContact;

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

/// Scratch storage for sphere/capsule intersection and its contact result.
typedef struct {
    _WorldCollisionPairContact contact;
    byte                       field_12[2]; // Role unproven
    VECTOR3                    sphere;
    s32                        pad_20;
    VECTOR                     end0;
    VECTOR                     end1;
    VECTOR                     planeA;
    VECTOR                     planeB;
    VECTOR                     delta;
    SVECTOR                    normal;
    SVECTOR                    hit;
    SVECTOR                    scaled;
} _WorldCollisionCapsuleScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionCapsuleScratch, 0x8C);

/// 0x88-byte scratch from `G_SCRATCH_HEAD` used by `Gp_CollideObjGrid`.
/// `pos` is the object's world position (`Gp_ObjWorldPos`) and `grid` its cell
/// (`Gp_LocalToGrid`). `verts` holds the face corners rotated by
/// `Gp_GridParams->field_0->workm` and translated by that matrix, `normal` the
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

/// 0x70-byte scratch from `G_SCRATCH_HEAD` used by `func_800DD324`, the
/// ray / face intersection test. Same tail layout as `GpGridHitScratch`
/// without the object position and cell: `verts` are the face corners rotated
/// by `Gp_GridParams->field_0->workm` and translated by that matrix, `normal`
/// the rotated face normal, and per edge `delta` is the corner difference,
/// `unit` its `VectorNormal`, then `delta` is reused for the `normal x unit`
/// inward edge plane.
typedef struct _GpGridRayScratch {
    /* 0x00 */ VECTOR verts[4];
    /* 0x40 */ VECTOR normal;
    /* 0x50 */ VECTOR unit;
    /* 0x60 */ VECTOR delta;
} GpGridRayScratch;
STATIC_ASSERT_SIZEOF(GpGridRayScratch, 0x70);

s32 Gp_PendingObj4CFlag;

s32 Gp_PairHandler1(GpObj* arg0, GpObj* arg1, s32 kind);

s32 Gp_PairHandler3(GpObj* arg0, GpObj* arg1, s32 kind);

static void Gp_RunPairHandler(GpObj* node);

static void _worldCollisionRecordPairContact(GpObj* receivingBody, GpObj* contactedBody, _WorldCollisionPairContact* contact);

/// Contact table selected by the body's shape, or NULL for a body without one.
static inline WorldCollisionContact* _worldCollisionGetObjectContacts(GpObj* obj)
{
    WorldCollisionContact* recs = NULL;

    switch (obj->flags & 7) {
        case 0:
            break;
        case 1:
            recs = obj->ctx.recs;
            break;
        case 2:
            recs = obj->ctx.node->ctx.recs;
            break;
        case 3:
            recs = obj->ctx.d4rec->recs;
            break;
        case 4:
            recs = obj->ctx.dir->field_8;
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
    if (gameGetPtrSlot(3) != NULL) {
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
        func_800E0608(Gp_ObjList0, 0x9007, 0x9004);
        if (gGameSession->field_12C == 0) {
            func_800E06AC(Gp_ObjList0, 0xA007, 0xA004);
        }
    }
}

static void Gp_RunPairHandler(GpObj* node)
{
    GpObj*      other;
    GpPairRule* rec;
    s32         rowOff;
    s32         temp;
    u16         flags;
    u16         handler;
    u16         swap;
    u8          kind;
    u8          otherKind;

    for (; node != NULL; node = node->next) {
        flags = node->flags;
        other = node->next;
        if (flags & 0x8000) {
            kind = (node->flags & 7) - 1;
            if (other != NULL) {
                rowOff = kind << 4;
                for (; other != NULL; other = other->next) {
                    if (other->flags & 0x8000) {
                        otherKind = (other->flags & 7) - 1;
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
static void _worldCollisionRecordPairContact(GpObj* receivingBody, GpObj* contactedBody, _WorldCollisionPairContact* contact)
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

    rec->key.value       = contactedBody->key;
    rec->distance        = contact->distance;
    rec->point           = contact->point;
    rec->response.normal = contact->response;
}

s32 Gp_PairHandler1(GpObj* arg0, GpObj* arg1, s32 kind)
{
    u8*                           head;
    _WorldCollisionSphereScratch* block;
    s32                           ret;

    head                                       = SCRATCH_HEAD(u8);
    block                                      = (_WorldCollisionSphereScratch*)(head - sizeof(_WorldCollisionSphereScratch));
    SCRATCH_HEAD(_WorldCollisionSphereScratch) = block;
    Gp_ObjWorldPos(arg0, (VECTOR3*)&block->pos0);
    Gp_ObjWorldPos(arg1, (VECTOR3*)&block->pos1);

    ret             = 0;
    block->delta.vx = block->pos0.vx - block->pos1.vx;
    block->delta.vy = block->pos0.vy - block->pos1.vy;
    block->delta.vz = block->pos0.vz - block->pos1.vz;
    if ((ABS(block->delta.vx) > 0x7FFF) || (ABS(block->delta.vz) > 0x7FFF)) {
        SCRATCH_POP(_WorldCollisionSphereScratch);
        return 0;
    }

    block->rsum32 = arg0->radius + arg1->radius;
    if (block->delta.vx * block->delta.vx + block->delta.vy * block->delta.vy + block->delta.vz * block->delta.vz < block->rsum32 * block->rsum32) {
        block->contact.point.vx    = block->pos1.vx;
        block->contact.point.vy    = block->pos1.vy;
        block->contact.point.vz    = block->pos1.vz;
        block->contact.response.vx = 0;
        block->contact.response.vy = 0;
        block->contact.response.vz = 0;
        block->contact.distance    = block->rsum32;
        _worldCollisionRecordPairContact(arg0, arg1, &block->contact);
        ret = 1;

        block->contact.point.vx    = block->pos0.vx;
        block->contact.point.vy    = block->pos0.vy;
        block->contact.point.vz    = block->pos0.vz;
        block->contact.response.vx = 0;
        block->contact.response.vy = 0;
        block->contact.response.vz = 0;
        block->contact.distance    = block->rsum32;
        _worldCollisionRecordPairContact(arg1, arg0, &block->contact);
    }

    SCRATCH_POP(_WorldCollisionSphereScratch);
    return ret;
}

s32 Gp_PairHandler3(GpObj* arg0, GpObj* arg1, s32 kind)
{
    // Collision records encode the source address as two halfwords.
    union {
        GpObj* object;
        s32    address;
    } sourceAddress;
    u8*                            head;
    _WorldCollisionCapsuleScratch* block;
    VECTOR*                        ends;
    GpActorD4Rec*                  rec;
    s32                            proj;
    s32                            ret;
    s32                            tapered;
    VECTOR3*                       pos;
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

    head               = SCRATCH_HEAD(u8);
    pos                = (VECTOR3*)(head - 0x78);
    SCRATCH_HEAD(void) = head - sizeof(_WorldCollisionCapsuleScratch);
    rec                = arg1->ctx.d4rec;
    block              = (_WorldCollisionCapsuleScratch*)(head - sizeof(_WorldCollisionCapsuleScratch));
    Gp_ObjWorldPos(arg0, pos);
    ends = (VECTOR*)(head - 0x68);
    func_800DEC80(arg1, ends, (SVECTOR*)(head - 0x18), 0);

    block->scaled.vx = (block->normal.vx * (u16)arg0->radius) >> 12;
    block->scaled.vy = (block->normal.vy * (u16)arg0->radius) >> 12;
    block->scaled.vz = (block->normal.vz * (u16)arg0->radius) >> 12;

    block->planeA.vx = block->end0.vx + block->scaled.vx;
    block->planeA.vy = block->end0.vy + block->scaled.vy;
    block->planeA.vz = block->end0.vz + block->scaled.vz;
    block->planeB.vx = block->end1.vx - block->scaled.vx;
    block->planeB.vy = block->end1.vy - block->scaled.vy;
    block->planeB.vz = block->end1.vz - block->scaled.vz;

    dx0 = (block->sphere.vx - block->planeA.vx) * block->normal.vx;
    dy0 = (block->sphere.vy - block->planeA.vy) * block->normal.vy;
    dz0 = (block->sphere.vz - block->planeA.vz) * block->normal.vz;
    ret = 0;
    if (dx0 + dy0 + dz0 > 0) {
        SCRATCH_POP(_WorldCollisionCapsuleScratch);
        return 0;
    }

    dx1  = (block->sphere.vx - block->planeB.vx) * block->normal.vx;
    dy1  = (block->sphere.vy - block->planeB.vy) * block->normal.vy;
    dz1  = (block->sphere.vz - block->planeB.vz) * block->normal.vz;
    proj = (dx1 + dy1 + dz1) >> 12;
    if (proj <= 0) {
        SCRATCH_POP(_WorldCollisionCapsuleScratch);
        return 0;
    }

    r1      = rec->end0Radius;
    tapered = r1 != rec->end1Radius;
    if (!tapered) {
        tmp           = (u16)arg0->radius + r1;
        block->hit.vx = (u16)block->planeB.vx + ((block->normal.vx * proj) >> 12);
        block->hit.vy = (u16)block->planeB.vy + ((block->normal.vy * proj) >> 12);
        block->hit.vz = (u16)block->planeB.vz + ((block->normal.vz * proj) >> 12);
        proj          = tmp;
        goto check;
    }

    if (arg1->flags & 0xC00) {
        gte_SetRotMatrix(&arg1->coord->workm);
        block->scaled.vx = (u16)rec->end0.vx + (u16)arg1->pos.vx;
        block->scaled.vy = (u16)rec->end0.vy + (u16)arg1->pos.vy;
        block->scaled.vz = (u16)rec->end0.vz + (u16)arg1->pos.vz;
        gte_ldv0((SVECTOR*)(head - 8));
        gte_rtv0();
        gte_stlvnl(ends);
        block->end0.vx += (arg1->coord)->workm.t[0];
        block->end0.vy += (arg1->coord)->workm.t[1];
        block->end0.vz += (arg1->coord)->workm.t[2];
    }

    block->delta.vx = block->end0.vx - block->end1.vx;
    block->delta.vy = block->end0.vy - block->end1.vy;
    block->delta.vz = block->end0.vz - block->end1.vz;
    dx2             = block->delta.vx * block->delta.vx;
    dy2             = block->delta.vy * block->delta.vy;
    dz2             = block->delta.vz * block->delta.vz;
    len             = SquareRoot0(dx2 + dy2 + dz2);

    block->scaled.vx = (block->normal.vx * proj) >> 12;
    block->scaled.vy = (block->normal.vy * proj) >> 12;
    block->scaled.vz = (block->normal.vz * proj) >> 12;
    dx3              = block->scaled.vx * block->scaled.vx;
    dy3              = block->scaled.vy * block->scaled.vy;
    dz3              = block->scaled.vz * block->scaled.vz;
    proj             = len;
    plen             = SquareRoot0(dx3 + dy3 + dz3);

    r0            = (rec->end0Radius << 12) / rec->end1Radius;
    proj          = (plen << 12) / proj;
    tmp           = r0 - 0x1000;
    r1            = (u16)arg0->radius;
    proj          = r1 + ((((tmp * proj) >> 12) * rec->end1Radius >> 12) + rec->end1Radius);
    block->hit.vx = (u16)block->scaled.vx + (u16)block->planeB.vx;
    block->hit.vy = (u16)block->scaled.vy + (u16)block->planeB.vy;
    block->hit.vz = (u16)block->scaled.vz + (u16)block->planeB.vz;

check:
    block->scaled.vx = (u16)block->hit.vx - (u16)block->sphere.vx;
    block->scaled.vy = (u16)block->hit.vy - (u16)block->sphere.vy;
    block->scaled.vz = (u16)block->hit.vz - (u16)block->sphere.vz;
    dx4              = block->scaled.vx * block->scaled.vx;
    dy4              = block->scaled.vy * block->scaled.vy;
    dz4              = block->scaled.vz * block->scaled.vz;
    radiusSquared    = proj * proj;
    if (dx4 + dy4 + dz4 < radiusSquared) {
        block->contact.distance    = 0;
        block->contact.point.vx    = (u16)block->end1.vx;
        block->contact.point.vy    = (u16)block->end1.vy;
        block->contact.point.vz    = (u16)block->end1.vz;
        block->contact.response.vx = (u16)block->normal.vx;
        block->contact.response.vy = (u16)block->normal.vy;
        block->contact.response.vz = (u16)block->normal.vz;
        _worldCollisionRecordPairContact(arg0, arg1, &block->contact);
        if (!tapered) {
            block->contact.point = block->hit;
        } else {
            block->contact.point.vx = (u16)block->sphere.vx;
            block->contact.point.vy = (u16)block->sphere.vy;
            block->contact.point.vz = (u16)block->sphere.vz;
        }
        if (arg1->flags & WORLD_COLLISION_BODY_SINGLE_CONTACT) {
            sourceAddress.object       = arg0;
            block->contact.response.vx = sourceAddress.address;
            block->contact.response.vy = sourceAddress.address >> 16;
        } else {
            block->contact.response.vx = 0;
            block->contact.response.vy = 0;
        }
        block->contact.response.vz = 0;
        block->contact.distance    = 0;
        _worldCollisionRecordPairContact(arg1, arg0, &block->contact);
        ret = 1;
    }

    SCRATCH_POP(_WorldCollisionCapsuleScratch);
    return ret;
}

void Gp_CollideObjGrid(GpObj* arg0)
{
    u8*                    head;
    GpGridHitScratch*      block;
    VECTOR3*               pos;
    GpGridFace*            face;
    WorldCollisionContact* slot;
    s16*                   cell;
    s32                    id;
    s32                    i;
    s32                    n;
    s32                    outside;
    s32                    val;
    s32                    faceDot;
    s32                    edgeDot;
    u16                    dist;
    u16                    flags;

    head               = SCRATCH_HEAD(u8);
    pos                = (VECTOR3*)(head - 0x80);
    SCRATCH_HEAD(void) = head - 0x88;
    block              = (GpGridHitScratch*)(head - 0x88);
    Gp_ObjWorldPos(arg0, pos);
    Gp_LocalToGrid(pos, &block->grid);

    if ((u16)block->grid.vx < Gp_GridParams->field_1C && (u16)block->grid.vz < Gp_GridParams->field_1E) {
        cell = Gp_GridParams->field_10[block->grid.vx * Gp_GridParams->field_1E + block->grid.vz];
        if (cell != NULL) {
            for (;;) {
                id = *cell;
                if (id == -1) {
                    goto done;
                }
                face = &Gp_GridParams->field_C[id];
                if (face->verts[0] == 0 && face->verts[1] == 0) {
                    cell++;
                    continue;
                }

                gte_SetRotMatrix(&Gp_GridParams->field_0->workm);
                gte_ldv0(&Gp_GridParams->field_8[face->verts[0]]);
                gte_rtv0();
                gte_stlvnl(&block->verts[0]);
                block->verts[0].vx += Gp_GridParams->field_0->workm.t[0];
                block->verts[0].vy += Gp_GridParams->field_0->workm.t[1];
                block->verts[0].vz += Gp_GridParams->field_0->workm.t[2];

                gte_ldv0(&Gp_GridParams->field_4[face->normalIndex]);
                gte_rtv0();
                gte_stlvnl(&block->normal);

                faceDot = (block->normal.vx * block->verts[0].vx + block->normal.vy * block->verts[0].vy +
                           block->normal.vz * block->verts[0].vz) >>
                          12;
                dist = ((block->normal.vx * block->pos.vx + block->normal.vy * block->pos.vy +
                         block->normal.vz * block->pos.vz) >>
                        12) -
                       faceDot;
                if ((u16)arg0->radius >= ABS((s16)dist)) {
                    goto edges;
                }
                goto next_face;

            mark_outside:
                outside = 1;
                goto edges_done;

            fill:
                slot->flags           = flags | WORLD_COLLISION_CONTACT_OCCUPIED;
                slot->distance        = (u16)arg0->radius - dist;
                slot->key.value       = face->surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                slot->point.vx        = 0;
                slot->point.vy        = 0;
                slot->point.vz        = 0;
                slot->response.normal = Gp_GridParams->field_4[face->normalIndex];
                goto next_face;

            edges:
                n = (face->verts[3] != 0xFFFF) ? 4 : 3;
                for (i = 1; i < n; i++) {
                    gte_ldv0(&Gp_GridParams->field_8[face->verts[i]]);
                    gte_rtv0();
                    gte_stlvnl(&block->verts[i]);
                    block->verts[i].vx += Gp_GridParams->field_0->workm.t[0];
                    block->verts[i].vy += Gp_GridParams->field_0->workm.t[1];
                    block->verts[i].vz += Gp_GridParams->field_0->workm.t[2];
                }

                outside = 0;
                for (i = n - 3; i < n * 2 - 3; i++) {
                    block->delta.vx =
                        block->verts[Gp_FaceEdgePairs[i].field_0].vx - block->verts[Gp_FaceEdgePairs[i].field_2].vx;
                    block->delta.vy =
                        block->verts[Gp_FaceEdgePairs[i].field_0].vy - block->verts[Gp_FaceEdgePairs[i].field_2].vy;
                    block->delta.vz =
                        block->verts[Gp_FaceEdgePairs[i].field_0].vz - block->verts[Gp_FaceEdgePairs[i].field_2].vz;
                    VectorNormal(&block->delta, &block->unit);
                    gte_ldopv1(&block->normal);
                    gte_ldopv2(&block->unit);
                    gte_op12();
                    gte_stlvnl(&block->delta);

                    edgeDot = (block->delta.vx * block->verts[Gp_FaceEdgePairs[i].field_0].vx +
                               block->delta.vy * block->verts[Gp_FaceEdgePairs[i].field_0].vy +
                               block->delta.vz * block->verts[Gp_FaceEdgePairs[i].field_0].vz) >>
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

                slot = arg0->ctx.recs;
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
    SCRATCH_POP_BYTES(0x88);
}

void Gp_CollideObjGridDir(GpObj* arg0)
{
    u8*                    head;
    GpGridHitScratch*      block;
    VECTOR3*               pos;
    GpGridFace*            face;
    WorldCollisionContact* slot;
    GpObjDirRec*           rec;
    s16*                   cell;
    s32                    id;
    s32                    i;
    s32                    n;
    s32                    outside;
    s32                    val;
    s32                    faceDot;
    s32                    edgeDot;
    u16                    dist;
    s32                    extra;
    s32                    faceKind;
    u16                    flags;

    head               = SCRATCH_HEAD(u8);
    pos                = (VECTOR3*)(head - 0x80);
    SCRATCH_HEAD(void) = head - 0x88;
    block              = (GpGridHitScratch*)(head - 0x88);
    rec                = arg0->ctx.dir;
    Gp_ObjWorldPos(arg0, pos);
    Gp_LocalToGrid(pos, &block->grid);

    if ((u16)block->grid.vx < Gp_GridParams->field_1C && (u16)block->grid.vz < Gp_GridParams->field_1E) {
        cell = Gp_GridParams->field_10[block->grid.vx * Gp_GridParams->field_1E + block->grid.vz];
        if (cell != NULL) {
            for (;;) {
                id = *cell;
                if (id == -1) {
                    goto done;
                }
                face = &Gp_GridParams->field_C[id];
                if (face->verts[0] == 0 && face->verts[1] == 0) {
                    cell++;
                    continue;
                }
                if (Gp_GridParams->field_4[face->normalIndex].vy < -0xDDA) {
                    cell++;
                    continue;
                }

                gte_SetRotMatrix(&Gp_GridParams->field_0->workm);
                gte_ldv0(&Gp_GridParams->field_8[face->verts[0]]);
                gte_rtv0();
                gte_stlvnl(&block->verts[0]);
                block->verts[0].vx += Gp_GridParams->field_0->workm.t[0];
                block->verts[0].vy += Gp_GridParams->field_0->workm.t[1];
                block->verts[0].vz += Gp_GridParams->field_0->workm.t[2];

                gte_ldv0(&Gp_GridParams->field_4[face->normalIndex]);
                gte_rtv0();
                gte_stlvnl(&block->normal);

                if (rec->dir.vx * block->normal.vx + rec->dir.vy * block->normal.vy +
                        rec->dir.vz * block->normal.vz >
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
                if ((u16)arg0->radius >= ABS((s16)dist)) {
                    goto edges;
                }
                goto next_face;

            mark_outside:
                outside = 1;
                goto edges_done;

            edges:
                n = (face->verts[3] != 0xFFFF) ? 4 : 3;
                for (i = 1; i < n; i++) {
                    gte_ldv0(&Gp_GridParams->field_8[face->verts[i]]);
                    gte_rtv0();
                    gte_stlvnl(&block->verts[i]);
                    block->verts[i].vx += Gp_GridParams->field_0->workm.t[0];
                    block->verts[i].vy += Gp_GridParams->field_0->workm.t[1];
                    block->verts[i].vz += Gp_GridParams->field_0->workm.t[2];
                }

                extra   = 0;
                outside = 0;
                for (i = n - 3; i < n * 2 - 3; i++) {
                    block->delta.vx =
                        block->verts[Gp_FaceEdgePairs[i].field_0].vx - block->verts[Gp_FaceEdgePairs[i].field_2].vx;
                    block->delta.vy =
                        block->verts[Gp_FaceEdgePairs[i].field_0].vy - block->verts[Gp_FaceEdgePairs[i].field_2].vy;
                    block->delta.vz =
                        block->verts[Gp_FaceEdgePairs[i].field_0].vz - block->verts[Gp_FaceEdgePairs[i].field_2].vz;
                    VectorNormal(&block->delta, &block->unit);
                    gte_ldopv1(&block->normal);
                    gte_ldopv2(&block->unit);
                    gte_op12();
                    gte_stlvnl(&block->delta);

                    edgeDot = (block->delta.vx * block->verts[Gp_FaceEdgePairs[i].field_0].vx +
                               block->delta.vy * block->verts[Gp_FaceEdgePairs[i].field_0].vy +
                               block->delta.vz * block->verts[Gp_FaceEdgePairs[i].field_0].vz) >>
                              12;
                    val = (s16)(((block->delta.vx * block->pos.vx + block->delta.vy * block->pos.vy +
                                  block->delta.vz * block->pos.vz) >>
                                 12) -
                                edgeDot);
                    if (val - (u16)arg0->radius > 0) {
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

                slot = arg0->ctx.dir->field_8;
                for (;;) {
                    flags = slot->flags;
                    if (flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
                        if ((slot->key.value & -0x100) == (extra | WORLD_COLLISION_CONTACT_GRID)) {
                            if (slot->response.normal.vx == Gp_GridParams->field_4[face->normalIndex].vx &&
                                slot->response.normal.vy == Gp_GridParams->field_4[face->normalIndex].vy &&
                                slot->response.normal.vz == Gp_GridParams->field_4[face->normalIndex].vz) {
                                if (slot->distance < (s32)(u16)arg0->radius - (s16)dist) {
                                    slot->distance = (u16)arg0->radius - dist;
                                }
                                goto next_face;
                            }
                        }
                    } else {
                        slot->flags           = flags | WORLD_COLLISION_CONTACT_OCCUPIED;
                        slot->distance        = (u16)arg0->radius - dist;
                        faceKind              = face->surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                        slot->key.value       = extra | faceKind;
                        slot->point.vx        = 0;
                        slot->point.vy        = 0;
                        slot->point.vz        = 0;
                        slot->response.normal = Gp_GridParams->field_4[face->normalIndex];
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
    SCRATCH_POP_BYTES(0x88);
}

s32 func_800DD324(s32 faceId, VECTOR* seg, SVECTOR* ray, GpObj* arg3)
{
    u8*               head;
    GpGridRayScratch* block;
    GpGridFace*       face;
    s32               i;
    s32               n;
    s16               faceDot;
    s32               denom;
    s32               t;
    s32               edgeDot;
    s32               val;
    s32               limit;

    head               = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(void) = head - 0x70;
    face               = &Gp_GridParams->field_C[faceId];
    block              = (GpGridRayScratch*)(head - 0x70);

    gte_SetRotMatrix(&Gp_GridParams->field_0->workm);
    gte_ldv0(&Gp_GridParams->field_8[face->verts[0]]);
    gte_rtv0();
    gte_stlvnl(&block->verts[0]);
    block->verts[0].vx += Gp_GridParams->field_0->workm.t[0];
    block->verts[0].vy += Gp_GridParams->field_0->workm.t[1];
    block->verts[0].vz += Gp_GridParams->field_0->workm.t[2];

    gte_ldv0(&Gp_GridParams->field_4[face->normalIndex]);
    gte_rtv0();
    gte_stlvnl(&block->normal);

    faceDot = (block->normal.vx * block->verts[0].vx + block->normal.vy * block->verts[0].vy +
               block->normal.vz * block->verts[0].vz) >>
              12;
    denom = (block->normal.vx * ray[0].vx + block->normal.vy * ray[0].vy + block->normal.vz * ray[0].vz) >> 12;

    if (denom >= 0) {
        SCRATCH_POP_BYTES(0x70);
        return 0;
    }
    if ((((block->normal.vx * seg[1].vx + block->normal.vy * seg[1].vy + block->normal.vz * seg[1].vz) >> 12) -
         faceDot) <= 0) {
        SCRATCH_POP_BYTES(0x70);
        return 0;
    }
    t = -(((((block->normal.vx * seg[0].vx + block->normal.vy * seg[0].vy + block->normal.vz * seg[0].vz) >> 12) -
            faceDot)
           << 12)) /
        denom;
    if (t >= 0) {
        SCRATCH_POP_BYTES(0x70);
        return 0;
    }

    ray[1].vx = seg[0].vx + ((ray[0].vx * t) >> 12);
    ray[1].vy = seg[0].vy + ((ray[0].vy * t) >> 12);
    ray[1].vz = seg[0].vz + ((ray[0].vz * t) >> 12);

    n = (face->verts[3] == 0xFFFF) ? 3 : 4;

    gte_SetRotMatrix(&Gp_GridParams->field_0->workm);
    for (i = 1; i < n; i++) {
        gte_ldv0(&Gp_GridParams->field_8[face->verts[i]]);
        gte_rtv0();
        gte_stlvnl(&block->verts[i]);
        block->verts[i].vx += Gp_GridParams->field_0->workm.t[0];
        block->verts[i].vy += Gp_GridParams->field_0->workm.t[1];
        block->verts[i].vz += Gp_GridParams->field_0->workm.t[2];
    }

    for (i = n - 3; i < n * 2 - 3; i++) {
        block->delta.vx = block->verts[Gp_FaceEdgePairs[i].field_0].vx - block->verts[Gp_FaceEdgePairs[i].field_2].vx;
        block->delta.vy = block->verts[Gp_FaceEdgePairs[i].field_0].vy - block->verts[Gp_FaceEdgePairs[i].field_2].vy;
        block->delta.vz = block->verts[Gp_FaceEdgePairs[i].field_0].vz - block->verts[Gp_FaceEdgePairs[i].field_2].vz;
        VectorNormal(&block->delta, &block->unit);
        gte_ldopv1(&block->normal);
        gte_ldopv2(&block->unit);
        gte_op12();
        gte_stlvnl(&block->delta);

        edgeDot = (block->delta.vx * block->verts[Gp_FaceEdgePairs[i].field_0].vx +
                   block->delta.vy * block->verts[Gp_FaceEdgePairs[i].field_0].vy +
                   block->delta.vz * block->verts[Gp_FaceEdgePairs[i].field_0].vz) >>
                  12;
        limit = 5;
        val   = ((block->delta.vx * ray[1].vx + block->delta.vy * ray[1].vy + block->delta.vz * ray[1].vz) >> 12) -
              edgeDot;
        if (arg3 != 0) {
            limit = 10;
        }
        if ((s16)val - limit > 0) {
            SCRATCH_POP_BYTES(0x70);
            return 0;
        }
    }
    SCRATCH_POP_BYTES(0x70);
    return 1;
}
