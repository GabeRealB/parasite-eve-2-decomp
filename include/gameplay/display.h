#ifndef GAMEPLAY_DISPLAY_H
#define GAMEPLAY_DISPLAY_H

#include "common.h"

#include "main/coord.h"
#include "main/tmd_types.h"

/// The spawn argument and work record of `Gp_FadeWorkTask` (task 0x31), which
/// fades the screen out through a semi-transparent full-screen quad. `field_0`
/// selects the semi-transparency rate of the trailing `DR_TPAGE`; `field_1` is
/// the handshake flag the owner sets to 1 to start the fade-out and the task
/// sets to 2 once it is done; `field_2` is the fade length in frames
/// (defaulted to 0x20), which also divides the ramp.
typedef struct GpFadeWork {
    u8  field_0;
    u8  field_1;
    s16 field_2;
} GpFadeWork;
STATIC_ASSERT_SIZEOF(GpFadeWork, 4);

/// A task-owned coordinate body refreshed by the model draw passes.
///
/// `Task::extra.coordBody` owns this body when `bodyKind` is `TASK_BODY_COORD`.
/// It supplies one transform for effects and other tasks that draw their own
/// primitives. Attachment initializes an identity transform beneath
/// `gGfxViewCoord`; tasks may change its local matrix and borrowed parent.
/// Matrix units and cache invalidation follow `GfxCoord`.
///
/// `coord` points to `ownedCoord` for the body's lifetime. Keep the body at its
/// allocated address, unlink it from `gModelObjectCoordBodyList` before releasing it, and
/// keep any borrowed parent alive while its transform is composed.
typedef struct ModelObjectCoordBody {
    TmdListNode link;       // Intrusive link on `gModelObjectCoordBodyList`; forward traversal ends at NULL
    GfxCoord*   coord;      // Single coordinate node, pointing to `ownedCoord`
    s32         field_C;    // Initialized to 1; meaning unproven
    GfxCoord    ownedCoord; // Owned local transform and composed-matrix cache
} ModelObjectCoordBody;
STATIC_ASSERT_SIZEOF(ModelObjectCoordBody, 0x60);

#endif // GAMEPLAY_DISPLAY_H
