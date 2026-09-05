#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"
#include "rooms/rooms_shared_8017e690.h"

extern s8 D_8007217B;

void RoomsShared8017e690(Task* arg0)
{
    if (D_8007217B == 0) {
        Game_Session->field_80 = 0;
    } else {
        Game_Session->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}
