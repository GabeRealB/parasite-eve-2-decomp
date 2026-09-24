#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8 D_8007217B;

/// First state of the water task: clears the session's `field_80` or
/// `field_7E`, chosen by `D_8007217B`, and advances to the next state.
void func_shelter_b4_lower_sewer_8017E33C(Task* arg0)
{
    if (D_8007217B == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}
