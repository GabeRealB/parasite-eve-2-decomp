/* Shared channel step; include after overlay.h and main/task_types.h. */
#ifndef SRC_SHARED_SCREEN_FADE_STEP_UP_INC_C
#define SRC_SHARED_SCREEN_FADE_STEP_UP_INC_C

/// Adds the task's low-halfword intensity rate to all three fade channels.
///
/// The rate is unsigned, in intensity units per update; zero leaves the channels unchanged. Each ordered addition
/// narrows back to signed 16 bits without clamping. The caller draws before stepping and decides completion from the
/// stored red channel. The reverse step is `_screenFadeStepDown`.
static inline void _screenFadeStepUp(ScreenFadeWork* fade, const Task* task)
{
    fade->r += task->spawnArg1.halves.low;
    fade->g += task->spawnArg1.halves.low;
    fade->b += task->spawnArg1.halves.low;
}

#endif /* SRC_SHARED_SCREEN_FADE_STEP_UP_INC_C */
