#ifndef ACTORS_SHARED_80138EFC_H
#define ACTORS_SHARED_80138EFC_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block the body at 0x80138EFC is handed in `$a2`, as five actor slots
/// lay it out.
///
/// The dispatcher `func_actor_104900_80134780` copies its 26-entry handler
/// table onto the stack and then calls `table[work->state](enemy, task, work)`,
/// reading the index with `lb` from 0xBA7 - which is why that field is named
/// rather than numbered. The same slot allocates the block with
/// `Mem_Calloc(0xBCC, 0)` in `func_actor_104900_8013279C` and parks it in
/// `Task::idMap` (0x1C), so the size below is that allocation rather than a
/// guess; other slots carry the body with a differently sized block, and only
/// the fields this handler touches are laid out here.
///
/// The siblings in the same unit reach into the same run: `func_actor_104900_80138B5C`
/// reads 0xBA0 as a byte and 0xB94 / 0xB96 as halfwords, `func_actor_104900_80138E34`
/// and `func_actor_104900_80138F68` use 0xB9C..0xBAE the way this one does, and
/// `func_actor_104900_80138D58` stores a halfword at 0xB8C.
typedef struct ActorsShared80138efcWork {
    /* 0x000 */ byte pad_0[0xB9C];
    /* 0xB9C */ s16  field_B9C;
    /* 0xB9E */ byte pad_B9E[0x6];
    /// Motion id armed for the frame; every sibling writes a different pair
    /// here (0xB/0xE here, 0x15/0x16 next door, 5 in the setup handler).
    /* 0xBA4 */ s8 field_BA4;
    /// Set alongside `field_BA4` to ask for the motion to be restarted.
    /* 0xBA5 */ s8 field_BA5;
    /// Set when the trigger at `field_BA9` fires.
    /* 0xBA6 */ s8 field_BA6;
    /// Index into the dispatcher's 26-entry handler table, read there with
    /// `lb` and multiplied by 4.
    /* 0xBA7 */ s8 state;
    /// Run-once latch: 0 means the state has not started yet. Read signed for
    /// the test but re-read unsigned for the increment, hence the `(u8)` cast
    /// at the one place it is stepped.
    /* 0xBA8 */ s8 field_BA8;
    /// Trigger this handler consumes: nonzero sets `field_BA6`, resets the
    /// countdown at `field_B9C` and selects state 0xF.
    /* 0xBA9 */ s8   field_BA9;
    /* 0xBAA */ byte pad_BAA[0x1];
    /* 0xBAB */ s8   field_BAB;
    /* 0xBAC */ byte pad_BAC[0x2];
    /* 0xBAE */ u8   field_BAE;
    /* 0xBAF */ byte pad_BAF[0x1D];
} ActorsShared80138efcWork;
STATIC_ASSERT_SIZEOF(ActorsShared80138efcWork, 0xBCC);

/// Arms the motion pair for the current sub-state when `field_BA8` is still
/// clear, and switches to state 0xF when `field_BA9` is set.
void ActorsShared80138efc(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work);

#endif // ACTORS_SHARED_80138EFC_H
