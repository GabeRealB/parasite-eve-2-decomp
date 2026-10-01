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

/// Work block of the bridge model task, at `Task::work`: one word, cleared by
/// the setup state and read by nothing else.
typedef struct BridgeModelWork {
    s32 field_0;
} BridgeModelWork;
STATIC_ASSERT_SIZEOF(BridgeModelWork, 0x4);

static void bridgeModelSetup(Task* task);

#endif /* SRC_SHARED_BRIDGE_MODEL_H */
