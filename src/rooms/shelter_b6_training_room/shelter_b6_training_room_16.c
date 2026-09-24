#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8       D_8007106B;
extern TaskDesc D_shelter_b6_training_room_8018431C;

/// One-shot task: spawns the stream player, the second entry of the room's
/// descriptor pair, as the display's owning task, sets `D_8007106B`, spawns
/// the view tasks and kills itself.
void func_shelter_b6_training_room_8017DD98(Task* arg0)
{
    Display_SpawnWithOt(&D_shelter_b6_training_room_8018431C, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}
