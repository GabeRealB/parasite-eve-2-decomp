/* A dialogue that repeats until the player leaves it: a room task that runs a
 * caption command, waits for it, and runs it again unless the caption reports
 * event key 0xF. The room message that spawns it hides the player's weapon
 * first; the task shows it again when it ends.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_CAP_DIALOGUE_H
#define SRC_SHARED_CAP_DIALOGUE_H

#include "common.h"

#include "main/task_types.h"

void capDialogueLoopTask(Task* task);

#endif /* SRC_SHARED_CAP_DIALOGUE_H */
