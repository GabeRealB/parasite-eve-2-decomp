#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8 D_8007217B;

/// First state of the water task: clears the session counter the current
/// display mode selects (`field_80` when `D_8007217B` is zero, `field_7E`
/// otherwise) and moves on to the per-frame state.
void func_shelter_b4_reservoir_8017FB44(Task* arg0)
{
    if (D_8007217B == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}
