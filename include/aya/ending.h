#ifndef INCLUDE_AYA_ENDING_H
#define INCLUDE_AYA_ENDING_H

#include "main/task_types.h"

/// Bodyless controller for replay awards, cleared-save confirmation and restart.
///
/// Spawn at index 0 with zero arguments after credits restore the normal
/// display. The controller loads the award UI, then borrows each panel until
/// closing it. Keep the replay package, gameplay item catalog and live save
/// available through the sequence; only one controller may use the totals.
extern TaskDesc gEndingAwardScreenTaskDescriptor;

/// Indices of the ending credits controller and its bodyless workers.
enum {
    ENDING_CREDITS_CONTROLLER_TASK_INDEX = 0,
    ENDING_CREDITS_FADE_IN_TASK_INDEX    = 1,
    ENDING_CREDITS_FADE_OUT_TASK_INDEX   = 2,
    ENDING_CREDITS_PICTURE_TASK_INDEX    = 3,
    ENDING_CREDITS_TASK_COUNT            = 4,
};

/// Priority-32 task recipes for credits playback, fades and picture requests.
///
/// Gameplay starts the controller with zero arguments after loading the ending
/// resources. Fade workers take a positive duration up to 32767 callback ticks
/// in argument 1; the picture worker takes a loaded bitstream resource-slot index
/// (0..49). All entries are bodyless. Keep this package and its resources loaded
/// until the controller restores presentation and begins replay awards.
extern const TaskDesc gEndingCreditsTaskDescriptors[ENDING_CREDITS_TASK_COUNT];

#endif // INCLUDE_AYA_ENDING_H
