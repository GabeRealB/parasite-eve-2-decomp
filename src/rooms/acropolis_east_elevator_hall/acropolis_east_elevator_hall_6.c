#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3688.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/acropolis_east_elevator_hall.h"

/// Part of the mirrored player model each held-object reflection hangs off,
/// indexed by `Task::spawnArg1`.
extern u8 D_acropolis_east_elevator_hall_8017FC8C[];

/// Per-frame callback of a held-object reflection. `Task::spawnArg2` is the
/// hall's mirror task and the parent is the held-object task being reflected.
/// On the first frame it clones the parent's TMD source, parents the clone's
/// root coordinate to the mirrored player's corresponding part, points the
/// clone at the mirror's light and color matrices and negates the X
/// translation, flipping the clone across X when `spawnArg1` is 2 or more;
/// every frame it copies the mirror model's draw flags onto the clone.
void func_acropolis_east_elevator_hall_8017F128(Task* task)
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
    mirrorPart  = &((TmdObject*)mirror->extra)->coords[D_acropolis_east_elevator_hall_8017FC8C[task->spawnArg1]];
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
            scale = D_acropolis_east_elevator_hall_8017D5C4;
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

/// Runs the hall's mirror task: state 0 sets the mirror up, state 1 is its
/// per-frame update.
void func_acropolis_east_elevator_hall_8017F2F8(Task* task)
{
    TaskFunc states[2] = {
        func_acropolis_east_elevator_hall_8017D5F0,
        func_acropolis_east_elevator_hall_8017D7A4,
    };

    states[task->state](task);
}
