/* The Akropolis bridge as a separate model, shown in the rooms that see it: a
 * room task parks the model at a fixed world position under the room's view
 * coordinate system and, each frame, hides it on the room's own camera views.
 * The setup state is shared; each room keeps its per-frame state, which tests
 * its own views.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_BRIDGE_MODEL_H
#define SRC_SHARED_BRIDGE_MODEL_H

#include "common.h"

#include "main/task_types.h"

/// Work block of the bridge model task.
///
/// The setup state allocates it from the primary heap and hands it to
/// `Task::work`, so the task's default teardown releases it. Neither room's
/// per-frame state reads it.
typedef struct {
    s32 field_0; // Cleared at setup and never read; role unproven
} BridgeModelWork;
STATIC_ASSERT_SIZEOF(BridgeModelWork, 0x4);

static void bridgeModelSetup(Task* task);

#endif /* SRC_SHARED_BRIDGE_MODEL_H */
