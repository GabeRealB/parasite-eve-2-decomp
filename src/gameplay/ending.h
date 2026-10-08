#ifndef GAMEPLAY_PRIVATE_ENDING_H
#define GAMEPLAY_PRIVATE_ENDING_H

#include "main/task_types.h"

/// Colours and labels defined in `gameplay.c`, shared with `78.c`.
extern const TaskFuncTable6 Gp_PlayClockStates;

extern const char Gp_StrHP[];

extern const char Gp_StrMP[];

extern const char Gp_StrItem[];

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
