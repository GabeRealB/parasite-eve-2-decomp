#include "common.h"

#include "gameplay/3688.h"
#include "main/task.h"
#include "rooms/dryfield_night_factory.h"
#include "rooms/room_common.h"

/// Script state: drops the prompt's highlight and spawns the action prompt at
/// the cursor position with the display mode of the confirmed hotspot, then
/// moves the script to state 4.
void func_dryfield_night_factory_8018196C(Task* task)
{
    RoomActionPrompt*       prompt = &D_80114D28;
    NightFactoryScriptWork* work   = (NightFactoryScriptWork*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->field_E);
    task->state = 4;
}
