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
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "gameplay/damage.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task_types.h"

/// 0x28-byte scratch from `G_SCRATCH_HEAD` used by `Gp_FindNearestSlot`.
/// `local` is the collider's `end1` plus `GpObj.pos`,
/// rotated by `coord->workm`. `vec` is that GTE output (then overwritten
/// with per-slot XYZ deltas). `world` is `vec + workm.t`.
typedef struct _GpNearScratch {
    /* 0x00 */ VECTOR3 vec;
    /* 0x0C */ s32     pad_C;
    /* 0x10 */ VECTOR3 world;
    /* 0x1C */ s32     pad_1C;
    /* 0x20 */ SVECTOR local;
} GpNearScratch;
STATIC_ASSERT_SIZEOF(GpNearScratch, 0x28);

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by `func_800E0FEC`.
/// Each `GpRec18` whose `key` high halfword is `0x10` contributes to
/// one accumulator, selected by `key` bits `0xF00`: kind 0 sums
/// `depth * at10.normal` into `acc[0]`, kind 1 writes the lift
/// `-(depth << 12)` into `acc[1].vy`, and kind 2 writes the slide
/// `depth * at10.normal.vx` / `.vz` into `acc[2]` for the record with the
/// smallest `depth`. `acc[3]` holds the pairwise XZ products of the kind-0
/// records used to detect opposing pushes.
typedef struct _GpPushScratch {
    /* 0x00 */ VECTOR acc[4];
} GpPushScratch;
STATIC_ASSERT_SIZEOF(GpPushScratch, 0x40);

/// 0x34-byte scratch from `G_SCRATCH_HEAD` used by `func_800E0C10`.
/// `acc[0]` sums `at10.normal * depth` for every contributing
/// `GpRec18` that sits at or above the floor cutoff (`at10.normal.vy >=
/// -0xDDA`); records below it instead accumulate into `acc[1].vy` and
/// bump `count`, so the average of that column can be folded in at the
/// end. `acc[2]` holds the pairwise XZ products used to detect two
/// records pushing in opposing directions.
typedef struct _GpSlideScratch {
    /* 0x00 */ VECTOR acc[3];
    /* 0x30 */ s32    count;
} GpSlideScratch;
STATIC_ASSERT_SIZEOF(GpSlideScratch, 0x34);

/// 0x20-byte scratch from `G_SCRATCH_HEAD` used by `func_800E0994`.
/// `local[0]` / `local[1]` are `(0, pos.vy +/- radius, 0)` in the
/// object's local space, rotated by `coord->workm` into `vec` then added
/// to `workm.t` to give the two world points `arg1[0]` / `arg1[1]`.
/// `vec` is reused as `arg1[0] - arg1[1]` for `VectorNormalS`.
typedef struct _GpAxisScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR local[2];
} GpAxisScratch;
STATIC_ASSERT_SIZEOF(GpAxisScratch, 0x20);

/// 0x4C-byte scratch from `G_SCRATCH_HEAD` used by `Gp_OrientAlong`.
/// `vec` is the `VectorNormalS` result, reused as the `RotMatrix` angle
/// vector. `mat1` is RotY(yaw), then RotY * RotX(-pitch). `mat2` is
/// RotX(-pitch), then RotZ(roll). `pitch` / `yaw` are `ratan2` angles
/// in `0..0xFFF`.
typedef struct _GpDirMatScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ MATRIX  mat1;
    /* 0x28 */ MATRIX  mat2;
    /* 0x48 */ s16     pitch;
    /* 0x4A */ s16     yaw;
} GpDirMatScratch;
STATIC_ASSERT_SIZEOF(GpDirMatScratch, 0x4C);

/* Define BSS before API headers to preserve first-declaration order. */
GpObj4C* Gp_PendingObj4C;

GpObj* Gp_ObjList0;

GpObj* Gp_ObjList1;

GpObj* Gp_ObjList2;

GpObj* Gp_ObjList3;

GpObj* Gp_ObjList4;

GpObj* Gp_ObjList5;

GpObj* Gp_ObjList6;

GpObj* Gp_ObjList7;

GpObj* Gp_ObjList8;

#include "world_collision.h"

/// Nine-entry table of `GpObj` list heads (`Gp_ObjList0` .. `Gp_ObjList8`).
/// `Gp_LinkObj` appends to `Gp_ObjLists[index]`; `Gp_UnlinkObj` unlinks.
extern GpObj** Gp_ObjLists[9];

/// Two-entry table of `GpObj4A` list heads. `Gp_LinkObj4A` appends to
/// `Gp_Obj4ALists[index]`; `Gp_ClearObj4AList` walks and clears that list.
extern GpObj4A** Gp_Obj4ALists[2];

/// One-entry table of `GpObj3A` list heads. `Gp_LinkObj3A` appends to
/// `Gp_Obj3ALists[index]`; `Gp_ClearObj3AList` walks and clears that list.
extern GpObj3A** Gp_Obj3ALists[1];

static void Gp_WorldToGrid(VECTOR3* arg0, SVECTOR3* arg1);

static s32 Gp_FindNearestSlot(GpObj* arg0, s32 arg1);

static void Gp_UnlinkObj3A(s32 arg0, GpObj3A* arg1);

GpObj** Gp_ObjLists[9] = {
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
GpObj4A** Gp_Obj4ALists[2] = {
    &Gp_PendingObj4C,
    &Gp_Obj4CList,
};
GpObj3A** Gp_Obj3ALists[1] = {
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
    VECTOR*  vec;
    GpObj3A* node;
    s32      ret;

    ret     = 0;
    node    = D_80115550;
    vec     = SCRATCH_PUSH(VECTOR);
    vec->vx = arg1->vx - arg0->vx;
    vec->vy = arg1->vy - arg0->vy;
    vec->vz = arg1->vz - arg0->vz;
    VectorNormal(vec, vec);
    for (; node != NULL; node = node->next) {
        if (node->field_3A & 0x40) {
            ret = func_800DFCCC(node, arg0, arg1, vec);
            if (ret == 1) {
                break;
            }
        }
    }
    SCRATCH_POP(VECTOR);
    return ret;
}

void Gp_CollideLists(GpObj* a, GpObj* b)
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

    for (; a != NULL; a = a->next) {
        flags = a->flags;
        if (flags & 0x8000) {
            kind  = (a->flags & 7) - 1;
            other = b;
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
                            Gp_PairHandlers[handler](a, other, handler);
                        } else {
                            Gp_PairHandlers[handler](other, a, handler);
                        }
                    }
                }
            }
        }
    }
}

void Gp_CollideListGrid(GpObj* node)
{
    u16 flags;

    if (Gp_GridParams != 0) {
        for (; node != NULL; node = node->next) {
            flags = node->flags;
            if (flags & 0x4000) {
                switch (flags & 7) {
                    case 0:
                        break;
                    case 1:
                        Gp_CollideObjGrid(node);
                        break;
                    case 2:
                        break;
                    case 3:
                        func_800DDDF8(node);
                        break;
                    case 4:
                        if (node->flags & 0x200) {
                            func_800DD940(node);
                        }
                        Gp_CollideObjGridDir(node);
                        break;
                }
            }
        }
    }
}

void func_800E0608(GpObj* node, s32 mask, s32 match)
{
    GpObj4C* other;

    other = Gp_PendingObj4C;
    for (; node != NULL; node = node->next) {
        if ((node->flags & mask) == (u16)match) {
            for (; other != NULL; other = other->next) {
                if (other->field_4A & 0x40) {
                    func_800DEF80(node, other);
                }
            }
        }
    }
}

void func_800E06AC(GpObj* node, s32 mask, s32 match)
{
    GpObj4C*   other;
    GameActor* actor;
    s32        idx;
    s32        msk;
    u16        mch;

    other = Gp_Obj4CList;
    idx   = 3;
    msk   = mask;
    mch   = match;
    actor = gameGetPtrSlot(idx)->work;
    for (; node != NULL; node = node->next) {
        if ((node->flags & msk) == mch) {
            for (; other != NULL; other = other->next) {
                if (other->field_4A & 0x40) {
                    func_800DF6AC(node, other, (VECTOR3*)&actor->field_10);
                }
            }
        }
    }
}

s32 Gp_PairNop(GpObj* arg0, GpObj* arg1, s32 kind)
{
    return 0;
}

void Gp_LocalToGrid(VECTOR3* arg0, SVECTOR3* arg1)
{
    u8*           head;
    VECTOR*       vec;
    GpGridParams* p;
    s32           val;

    head = SCRATCH_HEAD(u8);
    vec = SCRATCH_HEAD(VECTOR) = (VECTOR*)(head - 0x10);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, (VECTOR*)arg0, vec);
    p   = Gp_GridParams;
    val = ((VECTOR*)(head - 0x10))->vx + p->field_14 - p->field_0->coord.t[0];
    if (val >= 0) {
        arg1->vx = val / p->field_20;
    } else {
        arg1->vx = -1;
    }
    p        = Gp_GridParams;
    arg1->vy = 0;
    val      = vec->vz + p->field_18 - p->field_0->coord.t[2];
    if (val >= 0) {
        arg1->vz = val / p->field_20;
    } else {
        arg1->vz = -1;
    }
    SCRATCH_POP_BYTES(0x10);
}

void Gp_ObjWorldPos(GpObj* arg0, VECTOR3* arg1)
{
    VECTOR3* vec;

    SCRATCH_PUSH_BYTES(0x30);
    vec = SCRATCH_HEAD(VECTOR3);
    gte_SetRotMatrix(&arg0->coord->workm);
    gte_ldv0(&arg0->pos.vx);
    gte_rtv0();
    gte_stlvnl(vec);
    arg1->vx = arg0->coord->workm.t[0] + vec->vx;
    arg1->vy = arg0->coord->workm.t[1] + vec->vy;
    arg1->vz = arg0->coord->workm.t[2] + vec->vz;
    SCRATCH_POP_BYTES(0x30);
}

void func_800E0994(GpObj* arg0, VECTOR* arg1, SVECTOR* arg2)
{
    GpAxisScratch* block;
    s32            i;

    SCRATCH_PUSH_BYTES(0x20);
    block              = SCRATCH_HEAD(GpAxisScratch);
    block->local[0].vx = 0;
    block->local[0].vy = (u16)arg0->pos.vy + (u16)arg0->radius;
    block->local[0].vz = 0;
    block->local[1].vx = 0;
    block->local[1].vy = (u16)arg0->pos.vy - (u16)arg0->radius;
    block->local[1].vz = 0;
    gte_SetRotMatrix(&arg0->coord->workm);
    for (i = 0; i < 2; i++) {
        gte_ldv0(&block->local[i]);
        gte_rtv0();
        gte_stlvnl(&block->vec);
        arg1[i].vx = block->vec.vx + (arg0->coord)->workm.t[0];
        arg1[i].vy = block->vec.vy + (arg0->coord)->workm.t[1];
        arg1[i].vz = block->vec.vz + (arg0->coord)->workm.t[2];
    }
    block->vec.vx = arg1[0].vx - arg1[1].vx;
    block->vec.vy = arg1[0].vy - arg1[1].vy;
    block->vec.vz = arg1[0].vz - arg1[1].vz;
    VectorNormalS(&block->vec, arg2);
    SCRATCH_POP_BYTES(0x20);
}

void Gp_ClearPendingObj4C(void)
{
    GpObj4C* node;

    for (node = Gp_PendingObj4C; node != NULL; node = node->next) {
        if (node->field_4B != 0) {
            node->field_4B = 0;
        }
    }
}

static void Gp_WorldToGrid(VECTOR3* arg0, SVECTOR3* arg1)
{
    s32           val;
    GpGridParams* p;

    p   = Gp_GridParams;
    val = arg0->vx + p->field_14;
    if (val >= 0) {
        arg1->vx = val / p->field_20;
    } else {
        arg1->vx = -1;
    }
    p        = Gp_GridParams;
    arg1->vy = 0;
    val      = arg0->vz + p->field_18;
    if (val >= 0) {
        arg1->vz = val / p->field_20;
    } else {
        arg1->vz = -1;
    }
}

s32 func_800E0C10(GpRec18* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3)
{
    u8*             head;
    GpSlideScratch* s;
    GpRec18*        rec;
    s32             i;
    s32             j;
    s32             count;
    s32             mask;
    s32             ret;

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

        head               = SCRATCH_HEAD(u8);
        SCRATCH_HEAD(void) = head - 0x34;
        s                  = (GpSlideScratch*)(head - 0x34);

        s->acc[0].vx = 0;
        s->acc[0].vy = 0;
        s->acc[0].vz = 0;
        s->acc[1].vx = 0;
        s->acc[1].vy = 0;
        s->acc[1].vz = 0;
        s->count     = 0;

        for (i = 0; i < arg2; i++) {
            rec = &arg0[i];
            if ((rec->flags & 1) && (rec->key & 0xFFFF0000) == 0x100000) {
                mask |= 1 << rec->key;
                if (Gp_RoomParams[rec->key & 7] == 0) {
                    if (rec->at10.normal.vy >= -0xDDA) {
                        s->acc[0].vx += rec->at10.normal.vx * rec->depth;
                        s->acc[0].vy += rec->at10.normal.vy * rec->depth;
                        s->acc[0].vz += rec->at10.normal.vz * rec->depth;
                        list[count++] = i;
                    } else {
                        s->acc[1].vx  = 0;
                        s->acc[1].vy += rec->at10.normal.vy * rec->depth;
                        s->acc[1].vz  = 0;
                        s->count++;
                    }
                }
                ret = 1;
            }
        }

        if (arg3 != NULL) {
            *arg3 = mask;
        }

        for (i = 0; i < count; i++) {
            for (j = 1; j < count; j++) {
                s->acc[2].vx = arg0[list[i]].at10.normal.vx * arg0[list[j]].at10.normal.vx;
                s->acc[2].vz = arg0[list[i]].at10.normal.vz * arg0[list[j]].at10.normal.vz;
                if (s->acc[2].vx < -0x800000 || s->acc[2].vz < -0x800000) {
                    ret = 2;
                }
            }
        }

        arg1->vx.w = s->acc[0].vx << 4;
        arg1->vy.w = s->acc[0].vy << 4;
        arg1->vz.w = s->acc[0].vz << 4;
        if (s->count != 0) {
            arg1->vx.w += (s->acc[1].vx / s->count) << 4;
            arg1->vy.w += (s->acc[1].vy / s->count) << 4;
            arg1->vz.w += (s->acc[1].vz / s->count) << 4;
        }

        SCRATCH_POP_BYTES(0x34);
        return ret;
    }
}

s32 func_800E0FEC(GpRec18* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3)
{
    u8*            head;
    GpPushScratch* s;
    GpRec18*       rec;
    s32            i;
    s32            j;
    s32            count;
    s32            mask;
    s32            ret;
    s32            prev;
    u8             list[0x20];

    ret   = 0;
    count = 0;
    mask  = 0;
    prev  = 0;
    if (arg2 == 0) {
        return ret;
    }

    head               = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(void) = head - 0x40;
    s                  = (GpPushScratch*)(head - 0x40);

    for (i = 0; i < 3; i++) {
        s->acc[i].vx = 0;
        s->acc[i].vy = 0;
        s->acc[i].vz = 0;
    }

    for (i = 0; i < arg2; i++) {
        rec = &arg0[i];
        if ((rec->flags & 1) && (rec->key & 0xFFFF0000) == 0x100000) {
            mask |= 1 << rec->key;
            if (Gp_RoomParams[rec->key & 7] == 0) {
                switch ((u32)(rec->key & 0xF00) >> 8) {
                    case 0:
                        s->acc[0].vx += rec->depth * rec->at10.normal.vx;
                        s->acc[0].vy += rec->depth * rec->at10.normal.vy;
                        s->acc[0].vz += rec->depth * rec->at10.normal.vz;
                        list[count++] = i;
                        break;
                    case 1:
                        s->acc[1].vx = 0;
                        s->acc[1].vy = -(rec->depth << 12);
                        s->acc[1].vz = 0;
                        break;
                    case 2:
                        if (rec->at10.normal.vy == 0 && ((s16)prev == 0 || rec->depth < (s16)prev)) {
                            s->acc[2].vx = rec->depth * rec->at10.normal.vx;
                            s->acc[2].vy = 0;
                            s->acc[2].vz = rec->depth * rec->at10.normal.vz;
                            prev         = (u16)rec->depth;
                        }
                        break;
                }
            }
            ret = 1;
        }
    }

    for (i = 0; i < count; i++) {
        for (j = 1; j < count; j++) {
            s->acc[3].vx = arg0[list[i]].at10.normal.vx * arg0[list[j]].at10.normal.vx;
            s->acc[3].vz = arg0[list[i]].at10.normal.vz * arg0[list[j]].at10.normal.vz;
            if (s->acc[3].vx < -0x800000 || s->acc[3].vz < -0x800000) {
                ret = 2;
            }
        }
    }

    if (arg3 != NULL) {
        *arg3 = mask;
    }

    if (count != 0) {
        arg1->vx.w = (s->acc[0].vx + s->acc[1].vx) << 4;
        arg1->vy.w = (s->acc[0].vy + s->acc[1].vy) << 4;
        arg1->vz.w = (s->acc[0].vz + s->acc[1].vz) << 4;
    } else {
        arg1->vx.w = (s->acc[1].vx + s->acc[2].vx) << 4;
        arg1->vy.w = (s->acc[1].vy + s->acc[2].vy) << 4;
        arg1->vz.w = (s->acc[1].vz + s->acc[2].vz) << 4;
    }

    SCRATCH_POP_BYTES(0x40);
    return ret;
}

static s32 Gp_FindNearestSlot(GpObj* arg0, s32 arg1)
{
    u8*            head;
    GpNearScratch* block;
    GpActorD4Rec*  rec;
    GpRec18*       slot;
    s32            minDist;
    s32            index;
    s32            best;
    s32            dx;
    s32            dy;
    s32            dz;
    s32            dist;

    minDist            = -1;
    index              = 0;
    best               = index;
    rec                = arg0->ctx.d4rec;
    head               = SCRATCH_HEAD(u8);
    slot               = rec->recs;
    SCRATCH_HEAD(void) = (void*)(head - 0x28);
    block              = (GpNearScratch*)(head - 0x28);
    gte_SetRotMatrix(&arg0->coord->workm);
    block->local.vx = (u16)rec->end1.vx + (u16)arg0->pos.vx;
    block->local.vy = (u16)rec->end1.vy + (u16)arg0->pos.vy;
    block->local.vz = (u16)rec->end1.vz + (u16)arg0->pos.vz;
    gte_ldv0((SVECTOR*)(head - 8));
    gte_rtv0();
    gte_stlvnl(block);
    block->world.vx = ((VECTOR3*)(head - 0x28))->vx + (arg0->coord)->workm.t[0];
    block->world.vy = block->vec.vy + (arg0->coord)->workm.t[1];
    block->world.vz = block->vec.vz + (arg0->coord)->workm.t[2];

    for (;;) {
        if ((slot->flags & 1) && ((slot->key & 0xFFFF0000) == arg1)) {
            dx            = slot->point.vx - block->world.vx;
            block->vec.vx = dx;
            dy            = slot->point.vy - block->world.vy;
            block->vec.vy = dy;
            dz            = slot->point.vz - block->world.vz;
            block->vec.vz = dz;
            dist          = SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
            if ((u32)dist < (u32)minDist) {
                minDist = dist;
                best    = index + 1;
            }
        }
        if (slot->flags & 2) {
            break;
        }
        slot++;
        index++;
    }

    SCRATCH_POP_BYTES(0x28);
    return best;
}

void Gp_LinkObj(s32 arg0, GpObj* arg1)
{
    u16     flags;
    GpObj** head;
    GpObj*  node;
    GpObj*  temp;

    head  = Gp_ObjLists[arg0];
    flags = arg1->flags;
    if (!(flags & 0x8)) {
        if ((flags & 0x7) < 5) {
            arg1->flags = flags | 0x8;
            temp        = *head;
            if (temp != NULL) {
                node = temp;
                while (node->next != NULL) {
                    node = node->next;
                }
                node->next = arg1;
                arg1->prev = &node->next;
            } else {
                *head      = arg1;
                arg1->prev = head;
            }
            arg1->next = NULL;
        }
    }
}

void Gp_UnlinkObj(GpObj* node)
{
    u16     flags;
    GpObj*  next;
    GpObj** prev;

    flags = node->flags;
    if (flags & 0x8) {
        next        = node->next;
        node->flags = flags & 0x7;
        prev        = node->prev;
        if (next != NULL) {
            *prev      = next;
            next->prev = node->prev;
            node->next = NULL;
        } else {
            *prev = NULL;
        }
        node->prev = NULL;
    }
}

void Gp_LinkObj4A(s32 arg0, GpObj4A* arg1)
{
    u8        flags;
    GpObj4A** head;
    GpObj4A*  node;
    GpObj4A*  temp;

    head  = Gp_Obj4ALists[arg0];
    flags = arg1->field_4A;
    if (!(flags & 0x20)) {
        arg1->field_4A = flags | 0x20;
        temp           = *head;
        if (temp != NULL) {
            node = temp;
            while (node->next != NULL) {
                node = node->next;
            }
            node->next = arg1;
            arg1->prev = &node->next;
        } else {
            *head      = arg1;
            arg1->prev = head;
        }
        arg1->next = NULL;
    }
}

void Gp_UnlinkObj4A(s32 arg0, GpObj4A* arg1)
{
    u8        flags;
    GpObj4A*  next;
    GpObj4A** prev;

    flags = arg1->field_4A;
    if (flags & 0x20) {
        next           = arg1->next;
        arg1->field_4A = flags & 0x87;
        prev           = arg1->prev;
        if (next != NULL) {
            *prev      = next;
            next->prev = arg1->prev;
            arg1->next = NULL;
        } else {
            *prev = NULL;
        }
        arg1->prev = NULL;
    }
}

void Gp_ClearObj4AList(s32 arg0)
{
    GpObj4A** head;
    GpObj4A*  node;
    GpObj4A*  next;
    GpObj4A*  temp;
    s32       flags;
    s32       mask;

    head = Gp_Obj4ALists[arg0];
    temp = *head;
    if (temp != NULL) {
        node  = temp;
        *head = NULL;
        mask  = ~0x78;
    loop:
        flags          = node->field_4A;
        next           = node->next;
        node->prev     = NULL;
        flags         &= mask;
        node->field_4A = flags;
        if (next != NULL) {
            node->next = NULL;
            node       = next;
            goto loop;
        }
    }
}

void Gp_LinkObj3A(s32 arg0, GpObj3A* arg1)
{
    u8        flags;
    GpObj3A** head;
    GpObj3A*  node;
    GpObj3A*  temp;

    head  = Gp_Obj3ALists[arg0];
    flags = arg1->field_3A;
    if (!(flags & 0x20)) {
        arg1->field_3A = flags | 0x20;
        temp           = *head;
        if (temp != NULL) {
            node = temp;
            while (node->next != NULL) {
                node = node->next;
            }
            node->next = arg1;
            arg1->prev = &node->next;
        } else {
            *head      = arg1;
            arg1->prev = head;
        }
        arg1->next = NULL;
    }
}

static void Gp_UnlinkObj3A(s32 arg0, GpObj3A* arg1)
{
    u8        flags;
    GpObj3A*  next;
    GpObj3A** prev;

    flags = arg1->field_3A;
    if (flags & 0x20) {
        next           = arg1->next;
        arg1->field_3A = flags & 0x87;
        prev           = arg1->prev;
        if (next != NULL) {
            *prev      = next;
            next->prev = arg1->prev;
            arg1->next = NULL;
        } else {
            *prev = NULL;
        }
        arg1->prev = NULL;
    }
}

void Gp_ClearObj3AList(s32 arg0)
{
    GpObj3A** head;
    GpObj3A*  node;
    GpObj3A*  next;
    GpObj3A*  temp;
    s32       flags;
    s32       mask;

    head = Gp_Obj3ALists[arg0];
    temp = *head;
    if (temp != NULL) {
        node  = temp;
        *head = NULL;
        mask  = ~0x78;
    loop:
        flags          = node->field_3A;
        next           = node->next;
        node->prev     = NULL;
        flags         &= mask;
        node->field_3A = flags;
        if (next != NULL) {
            node->next = NULL;
            node       = next;
            goto loop;
        }
    }
}

void Gp_InitRec18Table(GpRec18* arg0, s32 arg1, s32 arg2)
{
    Mem_Set(arg0, 0, arg1 * 0x18);
    arg0[arg1 - 1].flags = 2;
}

void Gp_LoadRoomParams(void)
{
    s32              i;
    GameSession*     session;
    GpRoomParamRec** recs;

    for (i = 7; i >= 0; i--) {
        Gp_RoomParams[i] = 0;
    }

    session = gGameSession;
    recs    = Gp_RoomParamTables[session->at4.loc.stage - 1][session->at4.loc.area - 1];
    for (i = 0; i < 8; i++) {
        Gp_RoomParams[i] = recs[i]->field_3;
    }
}

s32 Gp_FindRec18(GpRec18* arg0, s32 arg1)
{
    s32 result;
    s32 index;

    result = 0;
    for (index = 1;; index++) {
        if (arg0->flags & 1) {
            if (arg1 == 0) {
                return 1;
            }
            if (arg0->key == arg1) {
                result = index;
            }
        }
        if ((arg0++)->flags & 2) {
            break;
        }
    }
    return result;
}

s32 Gp_CountRec18Hi(GpRec18* arg0, s32 arg1)
{
    s32 count;

    count = 0;
    do {
        if ((arg0->flags & 1) && ((arg0->key & 0xFFFF0000) == arg1)) {
            count += 1;
        }
    } while (!((arg0++)->flags & 2));
    return count;
}

void Gp_ClearRec18Occupied(GpRec18* arg0)
{
    for (;;) {
        if (arg0->flags & 1) {
            arg0->flags         &= 2;
            arg0->depth          = 0;
            arg0->key            = 0;
            arg0->point.vx       = 0;
            arg0->point.vy       = 0;
            arg0->point.vz       = 0;
            arg0->at10.normal.vx = 0;
            arg0->at10.normal.vy = 0;
            arg0->at10.normal.vz = 0;
        }
        if (arg0->flags & 2) {
            break;
        }
        arg0++;
    }
}

s32 func_800E1ACC(u8* arg0)
{
    s32 val;
    s32 ret;

    val = *arg0 << 12;
    if (val != 0) {
        ret = cln(val) / 2839;
    } else {
        ret = 0;
    }
    return ret;
}

s32 func_800E1B24(s32 arg0)
{
    s32 mask[2];
    s32 val;
    s32 tmp;
    s32 ret;

    val     = 1 << arg0;
    mask[0] = val;
    tmp     = (u8)val << 12;
    if (tmp != 0) {
        ret = cln(tmp) / 2839;
    } else {
        ret = 0;
    }
    return ret;
}

void Gp_CommitObj4CSave(void)
{
    GpObj4C* node;

    for (node = Gp_Obj4CList; node != NULL; node = node->next) {
        if (node->field_4B != 0) {
            node->field_4B = 0;
            if ((u8)gGameSession->at4.loc.view == node->field_48) {
                Mc_SaveData[0].state.at4.loc.view = node->field_49;
            }
        }
    }
}

s32 Gp_TakePendingObj4C(u16* arg0, u8* arg1, u8* arg2)
{
    GpObj4C* node;

    for (node = Gp_PendingObj4C; node != NULL; node = node->next) {
        if (node->field_4B != 0) {
            Gp_PendingObj4CFlag = 1;
            *arg0               = node->field_46;
            *arg1               = node->field_48;
            *arg2               = node->field_49;
            return 1;
        }
    }
    return 0;
}

void Gp_ClaimSlot18(GpEnemy* arg0, s32 arg1)
{
    GpRec18*   slot;
    GpRec18*   temp;
    s32        one;
    GpStateF0* p;

    temp = arg0->recs;
    if (temp != NULL) {
        slot = temp;
        one  = 1;
        while (1) {
            if ((*(s32*)&slot->flags & 3) != one) {
                break;
            }
            slot++;
        }
        slot->key            = arg1;
        slot->depth          = 0;
        slot->point.vx       = 0;
        slot->point.vy       = 0;
        slot->point.vz       = 0;
        slot->at10.normal.vx = 0;
        slot->at10.normal.vy = 0;
        slot->at10.normal.vz = 0;
        slot->flags         |= 1;
        p                    = &Gp_StateF0;
        p->field_5++;
    }
}

void Gp_OrientAlong(VECTOR* arg0, MATRIX* arg1, s32 arg2)
{
    u8*              head;
    GpDirMatScratch* block;
    SVECTOR*         vec;
    MATRIX*          mat1;
    MATRIX*          mat2;
    s32              sin_yaw;
    s32              yaw;
    s32              pitch;

    head                          = SCRATCH_HEAD(u8);
    block                         = (GpDirMatScratch*)(head - 0x4C);
    vec                           = (SVECTOR*)block;
    SCRATCH_HEAD(GpDirMatScratch) = block;
    VectorNormalS(arg0, vec);

    mat1       = (MATRIX*)(head - 0x44);
    yaw        = ratan2(((SVECTOR*)(head - 0x4C))->vx, vec->vz) & 0xFFF;
    block->yaw = yaw;
    sin_yaw    = rsin(yaw);
    block->pitch =
        ratan2(vec->vy, (vec->vx * sin_yaw + vec->vz * rcos(block->yaw)) >> 12) & 0xFFF;

    ((SVECTOR*)(head - 0x4C))->vx = 0;
    vec->vz                       = 0;
    vec->vy                       = block->yaw;
    RotMatrix(vec, mat1);

    mat2                          = (MATRIX*)(head - 0x24);
    pitch                         = block->pitch;
    ((SVECTOR*)(head - 0x4C))->vx = -pitch;
    vec->vy                       = 0;
    vec->vz                       = 0;
    RotMatrix(vec, mat2);

    gte_SetRotMatrix(mat1);
    gte_ldclmv(mat2);
    gte_rtir();
    gte_stclmv(mat1);
    gte_ldclmv(&mat2->m[0][1]);
    gte_rtir();
    gte_stclmv(&mat1->m[0][1]);
    gte_ldclmv(&mat2->m[0][2]);
    gte_rtir();
    gte_stclmv(&mat1->m[0][2]);

    ((SVECTOR*)(head - 0x4C))->vx = 0;
    vec->vy                       = 0;
    vec->vz                       = arg2;
    RotMatrix(vec, mat2);

    gte_SetRotMatrix(mat1);
    gte_ldclmv(mat2);
    gte_rtir();
    gte_stclmv(arg1);
    gte_ldclmv(&mat2->m[0][1]);
    gte_rtir();
    gte_stclmv(&arg1->m[0][1]);
    gte_ldclmv(&mat2->m[0][2]);
    gte_rtir();
    gte_stclmv(&arg1->m[0][2]);

    SCRATCH_POP_BYTES(0x4C);
}
