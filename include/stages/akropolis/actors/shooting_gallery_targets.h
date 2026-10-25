#ifndef INCLUDE_STAGES_AKROPOLIS_ACTORS_SHOOTING_GALLERY_TARGETS_H
#define INCLUDE_STAGES_AKROPOLIS_ACTORS_SHOOTING_GALLERY_TARGETS_H

#include "main/task_types.h"

/// Descriptor slots for the gallery mount and its carried target models.
enum {
    SHOOTING_GALLERY_TARGETS_TASK_MOUNT       = 0,
    SHOOTING_GALLERY_TARGETS_TASK_TARGET_BASE = 1,
    SHOOTING_GALLERY_TARGETS_TASK_COUNT       = 19,
};

/// Shooting-gallery mount and target spawn recipes, with no terminator.
extern TaskDesc gShootingGalleryTargetsTaskDescriptors[SHOOTING_GALLERY_TARGETS_TASK_COUNT];

#endif // INCLUDE_STAGES_AKROPOLIS_ACTORS_SHOOTING_GALLERY_TARGETS_H
