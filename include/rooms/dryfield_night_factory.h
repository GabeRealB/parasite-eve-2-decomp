#ifndef ROOMS_DRYFIELD_NIGHT_FACTORY_H
#define ROOMS_DRYFIELD_NIGHT_FACTORY_H

#include "common.h"

#include "main/task.h"

/// Cutscene work block the room's factory task allocates as 0xC zeroed bytes in
/// its state 0 and parks at `Task::idMap` (0x1C) -- that slot is *not* a
/// `TaskIdMap` here.
///
/// `state` selects the handler out of `D_dryfield_night_factory_8017D5DC`,
/// `step` is the counter the room's own handlers advance, and `prevFlag` is the
/// nibble of game flag 0x4E the shared state-0 handler `RoomsShared8017fdc8`
/// last saw. That handler reads the same three bytes.
typedef struct NightFactoryCutsceneWork {
    /* 0x0 */ byte pad_0[0x8];
    /* 0x8 */ u8   state;
    /* 0x9 */ u8   step;
    /* 0xA */ u8   prevFlag;
    /* 0xB */ byte pad_B[0x1];
} NightFactoryCutsceneWork;
STATIC_ASSERT_SIZEOF(NightFactoryCutsceneWork, 0xC);

/// A handler of the cutscene sequence. Unlike `TaskFunc` these report back: a
/// non-zero return means the handler has finished its part of the scene, and
/// the sequence drops back to the shared state 0.
typedef s32 (*NightFactoryCutsceneFunc)(Task*);

/// The room's three handler slots, as the dispatcher's local copy sees them.
/// The `.rodata` table they are copied out of ends with a NULL slot.
typedef struct NightFactoryCutsceneTable3 {
    /* 0x0 */ NightFactoryCutsceneFunc funcs[3];
} NightFactoryCutsceneTable3;
STATIC_ASSERT_SIZEOF(NightFactoryCutsceneTable3, 0xC);

/// The night factory cutscene sequence's handler table: the shared state-0
/// handler, the room's own two states, and the NULL the original `.rodata`
/// table ends with. Only the three handlers are copied by the dispatcher
/// `func_dryfield_night_factory_8017FD5C`, which is why the terminator is not
/// part of `NightFactoryCutsceneTable3`.
extern const NightFactoryCutsceneTable3 D_dryfield_night_factory_8017D5DC;

/// Work block the room's factory task allocates as 0x58 zeroed bytes in its
/// state 0 and parks at `Task::idMap`. It is the same block the shared
/// `Room_Util20` body (src/rooms/lib/room_util20.c) reads its model light and
/// color matrices out of at 0x18 / 0x38, so the tail is left unreferenced here.
///
/// `field_0` is the nibble of game flag 0x49 the task last saw, `field_14`
/// counts the frames since that nibble changed, and `field_16` / `field_17`
/// latch its two low bits -- `func_dryfield_night_factory_8017D6F8` seeds both
/// to -1 when it allocates the block.
typedef struct NightFactoryWork {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte pad_4[0x10];
    /* 0x14 */ u16  field_14;
    /* 0x16 */ u8   field_16;
    /* 0x17 */ u8   field_17;
    /* 0x18 */ byte pad_18[0x40];
} NightFactoryWork;
STATIC_ASSERT_SIZEOF(NightFactoryWork, 0x58);

/// Runs the factory model for the bit of game flag 0x49 the task last saw: bit
/// 1 picks the first handler pair and bit 0 the second of the pair, the frame
/// counter at `NightFactoryWork::field_14` is bumped, and the model's coordinate
/// is rebuilt and handed to `func_800D7A9C` together with its translation.
void func_dryfield_night_factory_8017FA08(Task* task);

/// Called by the model's state 0 with arg1 set and arg2 clear, and by the
/// per-frame handler above with arg1 clear and the low bit of the nibble in
/// arg2.
void func_dryfield_night_factory_8017D858(Task* task, s32 arg1, s32 arg2);

/// The handler the model runs while bit 1 of game flag 0x49 is set, and -- when
/// bit 0 is set with it -- the handler that follows.
void func_dryfield_night_factory_8017E13C(Task* task);
void func_dryfield_night_factory_8017DA54(Task* task);

/// The handler that follows `func_dryfield_night_factory_8017E13C` when bit 0 of
/// game flag 0x49 is clear.
void func_dryfield_night_factory_8017DDD4(Task* task);

/// The handler the model runs while bit 1 of game flag 0x49 is clear, and --
/// when bit 0 is set with it -- the handler that follows.
void func_dryfield_night_factory_8017E480(Task* task);
void func_dryfield_night_factory_8017E7A4(Task* task);

/// The handler that follows `func_dryfield_night_factory_8017E480` when bit 0 of
/// game flag 0x49 is clear.
void func_dryfield_night_factory_8017EBD4(Task* task);

#endif // ROOMS_DRYFIELD_NIGHT_FACTORY_H
