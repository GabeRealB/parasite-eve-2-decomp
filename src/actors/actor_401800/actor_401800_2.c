#include "common.h"

#include "actors/actor_401800.h"

/// The `0x7D3` handler of the `D_actor_401800_80155A80` table: maps the
/// requested state onto the work block's `field_89E` animation slot (5 selects
/// nothing), then resets the actor to state `0x11` with `field_2` cleared.
///
/// The table GCC emits for this switch is what puts the function in this unit:
/// it owns the rodata run at 0x254, so the table starts this object's `.rodata`
/// and its `.align 3` is a no-op there, and the byte of padding that follows it
/// (`D_actor_401800_80132088`, the original object's pad before the next unit's
/// table) is defined below as the data it is.
s32 func_actor_401800_8013DCBC(Actor401800* arg0, s32 arg1, Actor401800Msg7D3* arg2)
{
    Actor401800Work* work = arg0->field_1C;

    switch (arg2->field_4) {
        case 0:
            work->field_89E = 0x22;
            break;
        case 1:
            work->field_89E = 0x23;
            break;
        case 2:
            work->field_89E = 0x24;
            break;
        case 3:
            work->field_89E = 0x25;
            break;
        case 4:
            work->field_89E = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/// The `.align 3` pad that closes this unit's rodata: the switch table above
/// ends at a 4-mod-8 offset, so the original object's next table carried four
/// bytes of padding, and splat grouped them into the table's own symbol. The
/// compiler regenerates the table here, so the pad is written out as the word
/// of data it is; it is what keeps the next unit's `.rodata` at 0x26C.
const s32 D_actor_401800_80132088 = 0;
