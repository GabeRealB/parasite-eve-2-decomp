/* A bank of task descriptors with no code of its own. Its data lies between
 * libc's sprintf and libgte's cor_04, and only library code is linked between
 * those two, so it is an object on its own in the link. Reached through
 * gTaskDescBanks.
 */
#include "common.h"

#include "main/task.h"
#include "main/devkit.h"

#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/world_collision.h"

TaskDesc D_800676A8[] = {
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, func_80722624 },
    { 0x0, 0xF0, Gp_TickWorldCollision },
    { 0x0, 0xF0, func_8071E24C },
    { 0x1, 0x70, Gp_EffAttachTask37 },
    { 0x0, 0xC0, func_80723944 },
};
