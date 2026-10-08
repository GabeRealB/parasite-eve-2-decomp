#ifndef GAMEPLAY_PRIVATE_MODEL_LIGHTING_H
#define GAMEPLAY_PRIVATE_MODEL_LIGHTING_H

#include "main/task_types.h"

/// Initializes the play monitor's clock/HUD work and starts the current room's tasks.
///
/// Requires a fresh state-zero task, live saved/player/session state and the
/// selected room overlay. Saved play time counts minutes. Allocation failure
/// kills the task after the initial input/session writes; success gives the
/// zeroed work block to the task and advances its state. Selects two-VBlank
/// timing. Demos reset random/timing state and start input replay; keep their
/// aligned 0x18000-byte save-and-input resource loaded throughout playback.
void playClockInitializeTask(Task* task);

void Gp_TickPlayClock(Task* task);

void Gp_RestartSessionTask(Task* arg0);

#endif // GAMEPLAY_PRIVATE_MODEL_LIGHTING_H
