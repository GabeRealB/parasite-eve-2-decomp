#include "common.h"

#include "actors/actor_101500.h"

/// Runs the behaviour state `field_35A` selects. State 8 has no handler.
void Actor01500_Fn026D8(Actor101500* arg0)
{
    switch (arg0->field_1C->field_35A) {
        case 0:
            Actor01500_Fn00CA4(arg0);
            break;
        case 1:
            Actor01500_Fn027B0(arg0);
            break;
        case 2:
            Actor01500_Fn00FC4(arg0);
            break;
        case 3:
            Actor01500_Fn011B0(arg0);
            break;
        case 4:
            Actor01500_Fn0288C(arg0);
            break;
        case 5:
            Actor01500_Fn015DC(arg0);
            break;
        case 6:
            Actor01500_Fn028B0(arg0);
            break;
        case 7:
            Actor01500_Fn01708(arg0);
            break;
        case 9:
            Actor01500_Fn020D8(arg0);
            break;
    }
}
