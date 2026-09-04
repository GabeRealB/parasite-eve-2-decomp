#include "common.h"
#include "main/task.h"

void Room_Script08(Task* arg0)
{
    u8  fade;
    s16 temp_v0;

    fade = (u8)arg0->killCountdown;
    Fade_DrawOverlay(fade, fade, fade, 2);
    temp_v0             = (u16)arg0->killCountdown + 0x20;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x100) {
        Task_RequestKill(arg0, 0);
    }
}
