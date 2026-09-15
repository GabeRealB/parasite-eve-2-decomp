#include "common.h"

#include "gameplay/3688.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern void func_dryfield_night_factory_80180DE8(Task*, s16);

/// Work block `func_dryfield_night_factory_8018182C` allocates (Mem_Calloc(0x10))
/// and hangs off the `Task::idMap` slot (0x1C) -- that slot is *not* a
/// `TaskIdMap` here, it is the block the task's init state allocated. Reach it
/// with `(NightFactoryWork*)task->idMap`.
///
/// `field_C` is the cap step `func_dryfield_night_factory_80180DE8` switches on
/// (0..4) to pick the sound, the game flag and the cap slot for the step, and
/// `field_8` is the short this state arms with 0xA.
typedef struct NightFactoryWork {
    /* 0x0 */ byte pad_0[0x8];
    /* 0x8 */ s16  field_8;
    /* 0xA */ s16  field_A;
    /* 0xC */ s16  field_C;
    /* 0xE */ byte pad_E[0x2];
} NightFactoryWork;
STATIC_ASSERT_SIZEOF(NightFactoryWork, 0x10);

/// Runs the prompt state of the night factory script: drops the highlight the
/// previous state left in `D_80114D28` and, while `func_800D4EC0` still reports
/// a prompt on screen, hands the task to the cap step `field_C` names. Once the
/// prompt is gone the task advances to state 2 instead, and either way the work
/// block's `field_8` is set to 0xA.
void func_dryfield_night_factory_801819BC(Task* task)
{
    NightFactoryWork* work = (NightFactoryWork*)task->idMap;

    D_80114D28.mode     = 0;
    D_80114D28.targetId = 0;
    if (func_800D4EC0() != 0) {
        func_dryfield_night_factory_80180DE8(task, work->field_C);
    } else {
        task->state = 2;
    }
    work->field_8 = 0xA;
}
