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

/// Ends the play session after the death/ending wait and starts restart presentation.
///
/// State 4 of the play-clock task, entered with spawnArg1.value=0 and the
/// death-sound completion latch in killCountdown. Waits 26 callback ticks,
/// then stops sound, blocks input, discards tasks/ordering tables and reuses
/// their heaps. The task becomes invalid during this call; neither caller nor
/// task walker may inspect it afterwards. Previous GPU and heap users must be
/// finished or disposable. CD cancellation is asynchronous.
/// Preserve-display mode leaves VRAM/image source intact; other modes clear
/// the 320-by-512-word display region. Clears the complete session location.
/// Ending mode selects smaller primitive/auxiliary regions and skips playback
/// buffer reservation; every mode starts the restart-presentation loader.
void playClockRestartSessionTask(Task* task);

#endif // GAMEPLAY_PRIVATE_MODEL_LIGHTING_H
