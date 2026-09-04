#include "common.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "rooms/room_common.h"

extern TaskDesc D_acropolis_east_elevator_hall_8017FC90;

/// Part index in the mirrored player model each held-object reflection hangs
/// off, indexed by `Task::spawnArg1` (the held-object slot).
extern u8 D_acropolis_east_elevator_hall_8017FC8C[];

void func_acropolis_east_elevator_hall_8017D7A4(Task* task);

/// Sets up the elevator hall's mirror: re-attaches the player's own TMD source
/// to this task so the reflection draws the same model, allocates the
/// `RoomMirrorWork` block the reflection's coordinate frame and matrices live
/// in, and hangs the task off the player task so it dies with it. `spawnArg1`
/// selects which of the hall's two mirrors this is; mirror 0 also raises
/// `GameSession::field_4E`. The two child tasks mirror the player's held-object
/// tasks (`GameActor::field_920` / `field_924`).
void func_acropolis_east_elevator_hall_8017D5F0(Task* task)
{
    Task*           owner;
    GameActor*      actor;
    TmdObject*      extra;
    GsCOORDINATE2*  parts;
    RoomMirrorWork* work;
    Task*           child;
    Task*           spawned;
    s32             i;

    owner = Game_GetPtrSlot(3);
    if (Gp_AttachTmd(task, ((TmdObject*)owner->extra)->field_10) == NULL) {
        Task_Kill(task);
        return;
    }
    extra = task->extra;
    parts = extra->field_8;
    if ((u32)task->spawnArg1 >= 2U) {
        Task_Kill(task);
        return;
    }
    work = Mem_Calloc(sizeof(RoomMirrorWork), 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->idMap     = (TaskIdMap*)work;
    extra->field_24 = 6;
    Tmd_ProcessStream(extra);
    Tmd_ProcessStream(extra);
    extra->field_C = 0x10;
    extra->field_E = 0x1F;
    if (task->spawnArg1 == 0) {
        Game_Session->field_4E = 1;
    }
    parts->sub      = &work->coord;
    extra->field_1C = &work->light;
    extra->field_20 = &work->color;
    Task_Reparent(owner, task);
    task->state++;
    work->viewFlg   = Gfx_ViewCoord.flg & 0x7FFFFFFF;
    work->field_4   = 1;
    work->configRev = -1;
    extra->field_C |= 0x80;
    work->field_4   = 0;
    work->viewFlg   = -1;
    actor           = (GameActor*)owner->idMap;
    for (i = 0; i < 2; i++) {
        child = (&actor->field_920)[i];
        if (child != NULL) {
            spawned = Task_SpawnFromTable(&D_acropolis_east_elevator_hall_8017FC90, 1, i, (s32)task);
            if (spawned != NULL) {
                Task_Reparent(child, spawned);
            }
        }
    }
    func_acropolis_east_elevator_hall_8017D7A4(task);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", func_acropolis_east_elevator_hall_8017D7A4);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", D_acropolis_east_elevator_hall_8017D5C0);

/// Scale handed to `ScaleMatrix` to flip the reflection across X.
const VECTOR D_acropolis_east_elevator_hall_8017D5C4 = { -0x1000, 0x1000, 0x1000, 0 };

/// Per-frame callback of a held-object reflection. `Task::spawnArg2` is the
/// mirror task `func_acropolis_east_elevator_hall_8017D5F0` set up and the
/// parent is the held-object task being reflected. On the first frame it
/// clones the parent's TMD source, parents the clone's root coordinate to the
/// mirrored player's corresponding part, points the clone at the mirror's
/// light and color matrices and negates the X translation; every frame it
/// republishes the mirror model's draw flags onto the clone.
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
    mirrorPart  = &((TmdObject*)mirror->extra)->field_8[D_acropolis_east_elevator_hall_8017FC8C[task->spawnArg1]];
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
            scale = D_acropolis_east_elevator_hall_8017D5C4;
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

INCLUDE_ASM("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", func_acropolis_east_elevator_hall_8017F2F8);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", D_acropolis_east_elevator_hall_8017D5D4);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", D_acropolis_east_elevator_hall_8017D5E0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", D_acropolis_east_elevator_hall_8017D5E8);
