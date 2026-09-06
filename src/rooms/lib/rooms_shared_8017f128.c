#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3688.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017f128.h"

/// Per-frame callback of a held-object reflection. `Task::spawnArg2` is the
/// mirror task the room set up and the parent is the held-object task being reflected. On the first frame it
/// clones the parent's TMD source, parents the clone's root coordinate to the
/// mirrored player's corresponding part, points the clone at the mirror's
/// light and color matrices and negates the X translation; every frame it
/// republishes the mirror model's draw flags onto the clone.
void RoomsShared8017f128(Task* task)
{
    Task*           mirror;
    TmdObject*      mirrorExtra;
    RoomMirrorWork* work;
    GsCOORDINATE2*  mirrorPart;
    TmdObject*      src;
    GsCOORDINATE2*  srcParts;
    TmdObject*      extra;
    GsCOORDINATE2*  parts;
    VECTOR          scale;
    u16             flags;

    if (task->parent == NULL) {
        Task_CallExit(task);
    }
    mirror      = (Task*)task->spawnArg2;
    mirrorPart  = &((TmdObject*)mirror->extra)->field_8[RoomsShared8017f128Parts[task->spawnArg1]];
    work        = (RoomMirrorWork*)mirror->idMap;
    mirrorExtra = mirror->extra;
    if (task->state == 0) {
        src      = task->parent->extra;
        srcParts = src->field_8;
        if (Gp_AttachTmd(task, src->field_10) == NULL) {
            Task_CallExit(task);
            return;
        }
        extra           = task->extra;
        parts           = extra->field_8;
        extra->field_24 = src->field_24;
        Tmd_ProcessStream(extra);
        Tmd_ProcessStream(extra);
        extra->field_C  = 0x10;
        extra->field_E  = 0x1F;
        parts->sub      = mirrorPart;
        extra->field_1C = &work->light;
        extra->field_20 = &work->color;
        if (task->spawnArg1 >= 2) {
            scale = RoomsShared8017f128Scale;
            ScaleMatrix(&parts->coord, &scale);
        }
        parts->coord.t[0] = -srcParts->coord.t[0];
        parts->coord.t[1] = srcParts->coord.t[1];
        parts->coord.t[2] = srcParts->coord.t[2];
        parts->flg        = 0;
        task->state++;
    }
    extra          = task->extra;
    flags          = mirrorExtra->field_C;
    extra->field_C = flags;
    if (task->spawnArg1 >= 2) {
        extra->field_C = flags & 0xFFEF;
    }
}
