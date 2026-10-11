#ifndef INCLUDE_ACTORS_GLUTTON_DISPLAY_TYPES_H
#define INCLUDE_ACTORS_GLUTTON_DISPLAY_TYPES_H

#include "common.h"

/// Attachment of a Glutton display-model task to one body-model coordinate.
///
/// Entrance and incinerator exit scenes use the same relationship: the child's
/// root translates from a body part in that part's local game-coordinate units.
/// Tables are indexed by the task's part id in `Task::spawnArg1.value`: record 0
/// is unused by the body, 1/2 attach the legs to body coordinate 0, and 3..5
/// attach the fixed parts. The parent coordinate must remain live until detach.
typedef struct {
    s16 offsetX;    // Root translation along the parent part's local X axis
    s16 offsetY;    // Root translation along the parent part's local Y axis
    s16 offsetZ;    // Root translation along the parent part's local Z axis
    s16 parentPart; // Body-model coordinate index; nonnegative and within that model
} GluttonPartAttachment;
STATIC_ASSERT_SIZEOF(GluttonPartAttachment, 0x8);

#endif // INCLUDE_ACTORS_GLUTTON_DISPLAY_TYPES_H
