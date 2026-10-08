#ifndef SRC_ACTORS_ACTOR_143000_ACTOR_143000_PRIVATE_H
#define SRC_ACTORS_ACTOR_143000_ACTOR_143000_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/display.h"

#include "main/task_types.h"

/// Control record of the screen fade that follows an accepted code.
///
/// The keypad task starts a fade to black with it and, once the screen is
/// dark, hands the same record to the event task it spawns; that task requests
/// the fade's return as it starts, so the scene it sets up fades in.
extern ScreenFade D_actor_143000_80135C08;

/// A band of the background image for the overlay's strip-capture task, and
/// that task's progress through it.
///
/// The task divides the band's rows into `stripCount` strips and, one strip
/// per interval from the top down, stores those rows from VRAM into the same
/// rows of the resident image workspace (`Fs_ImgBuffers`). The source is the
/// 320-pixel-wide VRAM region whose top-left corner is (448,256), read at the
/// band's own row offsets. Every strip spans the full image width: the task
/// copies `band.x` and `band.w` and then replaces them, so only the rows
/// select what is captured.
///
/// The record is the task's second spawn argument and the task writes its
/// progress back into it, so one record serves one running task at a time.
typedef struct {
    RECT band;           // Rows to capture, in image pixels; only `y` and `h` take effect
    s32  stripCount;     // Strips the band is divided into; must be positive
    s32  stripsCaptured; // Strips stored so far; the task clears it on start and ends at `stripCount`
} Actor143000CaptureArgs;
STATIC_ASSERT_SIZEOF(Actor143000CaptureArgs, 0x10);

extern AnimationSet gActor143000Animation02A20;

extern AnimationSet gActor143000Animation02CCC;

extern AnimationSet gActor143000Animation02EE8;

extern AnimationSet gActor143000Animation03090;

extern AnimationSet gActor143000Animation03248;

extern TaskDesc D_actor_143000_801350B0[2];

extern s32 D_actor_143000_80135C00;

extern s32 D_actor_143000_80135C04;

extern char D_actor_143000_80135C20[24];

extern Actor143000CaptureArgs D_actor_143000_80135090;

extern Actor143000CaptureArgs D_actor_143000_801350A0;

/// Captures a background band into the resident image workspace, one strip per six ticks.
///
/// spawnArg2 must point to a writable Actor143000CaptureArgs for the task's
/// lifetime. stripCount must be positive and band rows must stay within the
/// 240-row image. The task resets stripsCaptured, kills itself on completion
/// or outside view 14, and borrows the workspace without allocating it.
/// The caller must keep the background loaded at VRAM (448,256), avoid workspace
/// reuse until capture finishes and synchronize GPU access to the stored image.
void actor143000CaptureStripTask(Task* task);

#endif // SRC_ACTORS_ACTOR_143000_ACTOR_143000_PRIVATE_H
