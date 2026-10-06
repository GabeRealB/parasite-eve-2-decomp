/* Shared channel step; include after overlay.h and main/task_types.h. */
#ifndef SRC_SHARED_SCREEN_FADE_STEP_DOWN_INC_C
#define SRC_SHARED_SCREEN_FADE_STEP_DOWN_INC_C

/// Subtracts the task's low-halfword intensity rate from all three fade channels.
///
/// `fade` must be writable and `task` live. The rate is unsigned, in intensity
/// units per update; zero leaves the channels unchanged. Each ordered subtraction
/// promotes to int and narrows back to signed 16 bits without clamping. The caller
/// draws before stepping and decides completion from the stored red channel.
static inline void _screenFadeStepDown(ScreenFadeWork* fade, const Task* task)
{
    fade->r -= task->spawnArg1.halves.low;
    fade->g -= task->spawnArg1.halves.low;
    fade->b -= task->spawnArg1.halves.low;
}

#endif /* SRC_SHARED_SCREEN_FADE_STEP_DOWN_INC_C */
