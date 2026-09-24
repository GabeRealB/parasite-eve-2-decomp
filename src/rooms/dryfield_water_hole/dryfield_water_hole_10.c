#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8 D_8007217B;

/// The water task's first state: clears the session halfword `field_80`, or
/// `field_7E` while `D_8007217B` is set, then advances to the drawing state.
void func_dryfield_water_hole_8017E000(Task* arg0)
{
    if (D_8007217B == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}
