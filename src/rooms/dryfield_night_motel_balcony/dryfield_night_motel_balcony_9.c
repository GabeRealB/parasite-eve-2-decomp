#include "common.h"

#include "main/gameflow.h"
#include "main/task.h"

/// Draws a white fade overlay (mode 2) each tick while `killCountdown`
/// climbs by 4, and kills the task once it reaches 0x100.
void func_dryfield_night_motel_balcony_8017E068(Task* arg0)
{
    u16 temp_v0;

    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
    temp_v0             = arg0->killCountdown + 4;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}
