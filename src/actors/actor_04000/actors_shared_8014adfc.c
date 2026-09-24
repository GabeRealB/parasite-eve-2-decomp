#include "common.h"

#include "actors/actor_104000.h"

/// Reaction check keyed on the requested animation in `field_174`: for 2 and
/// 3, answers 0x40280001 the first time the playing animation id in `field_4A`
/// reaches one of that animation's trigger ids (latched in `field_474`, which
/// clears on any other id); for 5, answers 0x400C0005 while bit 2 of
/// `field_58` is set. Answers 0 otherwise.
s32 Actor04000_Fn00FDC(Actor104000Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->field_174) {
        case 2:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->field_474 == v) {
                goto same;
            }
            arg0->field_474 = id;
            return 0x40280001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_474 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_474 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}
