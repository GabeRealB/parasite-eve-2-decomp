#include "common.h"

#include "actors/actor_202600.h"

/// Runs the handler of the work's current behaviour state (`field_39A`, 0-8);
/// state 9, entered when the hit points run out, runs nothing.
void Actor02600_Fn03784(Actor202600* arg0)
{
    switch (arg0->field_1C->field_39A) {
        case 0:
            Actor02600_Fn00754(arg0);
            break;
        case 1:
            Actor02600_Fn00914(arg0);
            break;
        case 2:
            Actor02600_Fn00A94(arg0);
            break;
        case 3:
            Actor02600_Fn00FA0(arg0);
            break;
        case 4:
            Actor02600_Fn012E8(arg0);
            break;
        case 5:
            Actor02600_Fn0143C(arg0);
            break;
        case 6:
            Actor02600_Fn01A0C(arg0);
            break;
        case 7:
            Actor02600_Fn0385C(arg0);
            break;
        case 8:
            Actor02600_Fn01B30(arg0);
            break;
        case 9:
            break;
    }
}
