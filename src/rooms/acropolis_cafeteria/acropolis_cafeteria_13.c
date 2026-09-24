#include "common.h"

#include "main/task.h"

/// Fades the screen to white, four steps of the kill countdown per frame, and
/// kills the task once the countdown reaches 0x100.
void func_acropolis_cafeteria_8017E658(Task* arg0)
{
    u16 temp_v0;

    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
    temp_v0             = arg0->killCountdown + 4;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}
