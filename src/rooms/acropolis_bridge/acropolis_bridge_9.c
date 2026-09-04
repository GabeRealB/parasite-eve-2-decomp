#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern GpEnemyTaskFuncTable3 D_acropolis_bridge_8017D6E8;

/// Work block the bridge enemy's task keeps at `Task::idMap`. `field_4` is the
/// live flag every state handler in this unit gates on, and `field_12E` /
/// `field_196` are the two flag halfwords whose bit 15 the handlers toggle to
/// enable one behaviour and disable the other.
typedef struct AcropolisBridgeEnemyWork {
    /* 0x000 */ byte pad_0[0x4];
    /* 0x004 */ s16  field_4;
    /* 0x006 */ byte pad_6[0x128];
    /* 0x12E */ u16  field_12E;
    /* 0x130 */ byte pad_130[0x66];
    /* 0x196 */ u16  field_196;
} AcropolisBridgeEnemyWork;

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184024);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184208);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_801843A0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_8018450C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184638);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184908);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80184B94);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80185104);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_8018532C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_801856E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_8018581C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80185988);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80185F28);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_801861A0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_801863A8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80186618);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80186BBC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80187078);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80187310);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_801874DC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_801876A8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_9", func_acropolis_bridge_80187850);

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
        work->field_12E    &= 0x7FFF;
        work->field_196    &= 0x7FFF;
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
