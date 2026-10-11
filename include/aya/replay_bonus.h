#ifndef INCLUDE_AYA_REPLAY_BONUS_H
#define INCLUDE_AYA_REPLAY_BONUS_H

#include "main/task_types.h"

/// Bodyless controller for replay awards, cleared-save confirmation and restart.
///
/// Spawn at index 0 with zero arguments after credits restore the normal
/// display. The controller loads the award UI, then borrows each panel until
/// closing it. Keep the replay package, gameplay item catalogue and live save
/// available through the sequence; only one controller may use the totals.
extern TaskDesc gReplayBonusAwardScreenTaskDescriptor;

#endif // INCLUDE_AYA_REPLAY_BONUS_H
