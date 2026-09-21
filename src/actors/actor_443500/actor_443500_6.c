#include "common.h"

#include "actors/actor_443500.h"

#include "main/task.h"
#include "main/tmd.h"

/// The four-way 0x7D5 switch documented on the prototype in `actor_443500.h`;
/// `ActorsShared80162bc4` carries the same body against its carriers' work
/// blocks. Only the mode-2 latch and the trailing mirror differ: the mode goes
/// into `Actor443500Work::field_4BC` -- the word the spawn handler seeds to -1
/// -- and the resulting `field_C` onto `field_4C0`.
s32 func_actor_443500_8013297C(Task* task, s32 anim, s32 mode, s32 arg3)
{
    TmdObject*       obj;
    s32              ret;
    Actor443500Work* work;

    obj  = task->extra;
    work = (Actor443500Work*)task->work;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags     |= 0x80;
            work->field_4BC = mode;
            obj->flags     |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    work->field_4C0 = obj->flags;
    return ret;
}

/// Position triple in this actor's layout table; 8 bytes with a trailing pad.
typedef struct {
    s16 x;
    s16 y;
    s16 z;
    s16 pad;
} Actor443500Vec;
STATIC_ASSERT_SIZEOF(Actor443500Vec, 8);

/// 12-byte layout record copied verbatim by `func_actor_443500_80132A68`.
typedef struct {
    s8 b[12];
} Actor443500Blob;
STATIC_ASSERT_SIZEOF(Actor443500Blob, 0xC);

/// Table of pointers into the layout data: the two-entry vector list at
/// `field_4`, the six-entry list at `field_8` and the two 12-byte records at
/// `field_C`. `D_actor_443500_801587D8` is the template, `D_8018828C` the live
/// copy in the room overlay slot.
typedef struct {
    s32              field_0;
    Actor443500Vec*  field_4;
    Actor443500Vec*  field_8;
    Actor443500Blob* field_C;
    void*            field_10;
} Actor443500Layout;
STATIC_ASSERT_SIZEOF(Actor443500Layout, 0x14);

extern Actor443500Layout D_8018828C;
extern Actor443500Layout D_actor_443500_801587D8;

void func_actor_443500_80132A68(s32 arg0)
{
    Actor443500Layout* dst;
    Actor443500Layout* src;
    Actor443500Vec     d;
    s32                i;

    dst = &D_8018828C;
    src = &D_actor_443500_801587D8;

    for (i = 0; i < 2; i++) {
        dst->field_4[i].x = src->field_4[i].x;
        dst->field_4[i].y = src->field_4[i].y;
        dst->field_4[i].z = src->field_4[i].z;
        dst->field_C[i]   = src->field_C[i];
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].x = src->field_8[i].x;
        dst->field_8[i].y = src->field_8[i].y;
        dst->field_8[i].z = src->field_8[i].z;
    }

    if (arg0 == 0) {
        d.x = 0;
        d.y = 0;
    } else {
        d.x = 0;
        d.y = 0x7D0;
    }
    d.z = 0;

    for (i = 0; i < 6; i++) {
        dst->field_8[i].x += d.x;
        dst->field_8[i].y += d.y;
        dst->field_8[i].z += d.z;
    }
}
