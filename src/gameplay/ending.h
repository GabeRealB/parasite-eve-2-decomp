#ifndef GAMEPLAY_PRIVATE_ENDING_H
#define GAMEPLAY_PRIVATE_ENDING_H

#include "main/task_types.h"

/// Six phases of the play-time/HUD controller and its death/restart sequence.
///
/// Task state 0 initializes, 1 updates play, 2 starts the death fade, 3 waits
/// for it, 4 restarts the session and 5 tears down. `playClockTask` copies the
/// complete table before dispatch; the state must be in 0..5.
extern const TaskFuncTable6 gPlayClockStates;

/// Encoded byte-string HP label shared by the HUD and battle-result readout.
extern const u8 gHudHpLabelText[];

/// Encoded byte-string MP label shared by the HUD and battle-result readout.
extern const u8 gHudMpLabelText[];

/// Encoded title of the item-pickup panel, before its confirmation closes it.
extern const char gItemPickupTitleText[];

/// Applies the battle result once and draws its gains and updated player totals.
///
/// spawnArg2 borrows the live panel object. spawnArg1 == 0 awards the combat
/// EXP/BP/MP totals and attached recovery effects; any nonzero value selects an
/// escape result (-10 BP, +1 MP, no EXP or recovery). Rewards narrow to signed
/// halfwords before application; BP is clamped to 0..999999, EXP only above
/// 999999, and HP/MP only above their maxima, after their stored-width addition.
/// The MP recovery bonus retains the unsigned quarter-round-up followed by
/// halfword narrowing; the displayed gains need not equal the capped increase.
///
/// Runs once per UI task update. killCountdown is elapsed updates here, capped
/// at 500; totals appear at updates 51/81/111/141 for EXP/BP/MP/optional HP.
/// An active panel accepts either confirm or cancel as CONFIRM, even before
/// every total appears. The caller closes the panel; this task releases nothing.
/// Result scratch is shared, so only one result panel may be active at a time.
void itemMenuBattleResultTask(Task* task);

/// Starts combat presentation and waits for its delay and countdown-table music.
///
/// spawnArg2 borrows the live HUD on state 0, changing its battle step to FIGHT.
/// The delay is 30 task updates, or 90 in stage 4 area 48, including setup.
/// Requires loaded vibration/map/music resources and an active stage mode task;
/// after the delay and music load, kills this task and requests that mode's exit.
void sceneBattleStartTransitionTask(Task* task);

#endif // GAMEPLAY_PRIVATE_ENDING_H
