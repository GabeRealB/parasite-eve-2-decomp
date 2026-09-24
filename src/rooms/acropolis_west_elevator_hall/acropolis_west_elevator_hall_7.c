#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3688.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/acropolis_west_elevator_hall.h"
#include "rooms/room_common.h"

/// Per-frame callback of a held-object reflection. `spawnArg2` is the mirror
/// task that spawned it and the parent is the held-object task it reflects;
/// with no parent it exits. On the first frame it clones the parent's TMD
/// source, hangs the clone's root coordinate off the mirror model's part that
/// `D_acropolis_west_elevator_hall_801802A4` names for its slot, points the
/// clone at the mirror's light and color matrices and negates the X
/// translation, scaling slots 2 and up by the rodata vector; every frame it
/// copies the mirror model's draw flags onto the clone.
void func_acropolis_west_elevator_hall_8017F134(Task* task)
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
    mirrorPart  = &((TmdObject*)mirror->extra)->coords[D_acropolis_west_elevator_hall_801802A4[task->spawnArg1]];
    work        = (RoomMirrorWork*)mirror->work;
    mirrorExtra = mirror->extra;
    if (task->state == 0) {
        src      = task->parent->extra;
        srcParts = src->coords;
        if (Gp_AttachTmd(task, src->source) == NULL) {
            Task_CallExit(task);
            return;
        }
        extra        = task->extra;
        parts        = extra->coords;
        extra->tpage = src->tpage;
        tmdProcessStream(extra);
        tmdProcessStream(extra);
        extra->flags    = 0x10;
        extra->otOffset = 0x1F;
        parts->sub      = mirrorPart;
        extra->lightMtx = &work->light;
        extra->colorMtx = &work->color;
        if (task->spawnArg1 >= 2) {
            scale = D_acropolis_west_elevator_hall_8017D5C4;
            ScaleMatrix(&parts->coord, &scale);
        }
        parts->coord.t[0] = -srcParts->coord.t[0];
        parts->coord.t[1] = srcParts->coord.t[1];
        parts->coord.t[2] = srcParts->coord.t[2];
        parts->flg        = 0;
        task->state++;
    }
    extra        = task->extra;
    flags        = mirrorExtra->flags;
    extra->flags = flags;
    if (task->spawnArg1 >= 2) {
        extra->flags = flags & 0xFFEF;
    }
}

/// The mirror task's per-frame entry: state 0 sets the mirror up, state 1
/// runs its update.
void func_acropolis_west_elevator_hall_8017F304(Task* task)
{
    TaskFunc states[2] = {
        func_acropolis_west_elevator_hall_8017D5FC,
        func_acropolis_west_elevator_hall_8017D7B0,
    };

    states[task->state](task);
}
