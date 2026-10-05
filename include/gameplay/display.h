#ifndef GAMEPLAY_DISPLAY_H
#define GAMEPLAY_DISPLAY_H

#include "common.h"

#include "main/coord.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Reveals the loaded session from black and releases its display hold.
///
/// Bank 0, type 0x21 has no task body and uses no spawn arguments or work block.
/// Start with state 0: three callback ticks hold full black, then nine ticks
/// draw decreasing subtractive darkness, from 255 through 15 in steps of 30.
/// `Task::killCountdown` holds the tick count, then the signed darkness value;
/// it reaches -15 after the final overlay, without drawing a zero-darkness tick.
/// If requested, completion clears the session's game-pause block before
/// releasing one display hold and tearing down the task. Do not use the task
/// after completion. Drawing requires the frame arena and ordering table that
/// `fadeDrawOverlay` uses.
void fadeResumeSessionTask(Task* task);

/// `ScreenFade::blend`. Zero darkens the frame toward black. Any other value
/// brightens it toward white.
enum { SCREEN_FADE_SUBTRACT = 0 };

/// `ScreenFade::phase`, the handshake between the fade's owner and its task.
///
/// Running covers the ramp up and the hold. Return asks for the ramp back
/// down. Done means that ramp reached zero and the task exited.
enum {
    SCREEN_FADE_RUNNING = 0,
    SCREEN_FADE_RETURN  = 1,
    SCREEN_FADE_DONE    = 2
};

/// Frame count stored when `ScreenFade::rampFrames` is non-positive at start.
enum { SCREEN_FADE_DEFAULT_FRAMES = 32 };

/// Control record of the resident full-screen fade (task bank 1, type 0x31).
///
/// The owner keeps the record for the task's whole life and passes its address
/// in `Task::spawnArg2`. The task ramps a semi-transparent full-screen quad up
/// from black, holds it, and ramps it back down once `phase` requests the
/// return. `blend` chooses whether the quad darkens the frame toward black or
/// brightens it toward white. Both ramps take `rampFrames` frames, and the
/// owner may store a new length before requesting the return. A non-positive
/// length is replaced with 32 when the task starts.
typedef struct {
    u8  blend;      // 0 subtract toward black, nonzero add toward white
    u8  phase;      // Handshake (0 running, 1 return requested, 2 finished)
    s16 rampFrames; // Frames in one ramp; <=0 at task start selects 32
} ScreenFade;
STATIC_ASSERT_SIZEOF(ScreenFade, 4);

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
