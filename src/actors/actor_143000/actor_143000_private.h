#ifndef SRC_ACTORS_ACTOR_143000_ACTOR_143000_PRIVATE_H
#define SRC_ACTORS_ACTOR_143000_ACTOR_143000_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor_143000.h"

#include "gameplay/animation.h"
#include "gameplay/display.h"

#include "main/task_types.h"

/// Persistent fade to black for an accepted keypad code and back into the terminal scene.
///
/// The keypad task starts a fade to black with it and, once the screen is
/// dark, hands the same record to the event task it spawns; that task requests
/// the fade's return as it starts, so the scene it sets up fades in.
extern ScreenFade gActor143000CodeAcceptedFade;

/// A band of the background image for the overlay's strip-capture task, and
/// that task's progress through it.
///
/// The task divides the band's rows into `stripCount` strips and, one strip
/// per interval from the top down, stores those rows from VRAM into the same
/// rows of the resident image workspace (`gFsImageWorkspacePtr`). The source is the
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

extern AnimationSet gShelterB2LaboratoryPlayerAtTerminalClip;

extern AnimationSet gShelterB2LaboratoryPlayerTypeAtTerminalClip;

extern AnimationSet gShelterB2LaboratoryPlayerShockedAtTerminalClip;

extern AnimationSet gShelterB2LaboratoryPlayerStopTypingClip;

extern AnimationSet gShelterB2LaboratoryPlayerResultsCue16Clip;

/// Keypad draw calls since overlay load; bit 4 controls its insertion-caret blink.
extern s32 gActor143000KeypadCaretDrawCalls;

/// Dog animation phase in sixteenths of a sequence step; each draw adds the Q4 marquee speed.
///
/// Initialized on overlay load and retained across keypad sessions.
extern s32 gActor143000KeypadDogPhaseQ4;

/// Byte capacity of the keypad entry storage, including its terminator.
enum { ACTOR_143000_KEYPAD_CODE_BUFFER_BYTES = 24 };

/// Keypad entry string; holds at most twenty characters plus a terminating zero.
///
/// Typing writes characters and drawing terminates at the current length;
/// automatic entry copies the eleven-byte accepted string. The storage persists
/// for the overlay lifetime, while each new keypad task starts with length zero.
extern char gActor143000KeypadCode[ACTOR_143000_KEYPAD_CODE_BUFFER_BYTES];

/// Room view saved before the keypad selects view 11, restored on a rejected or cancelled login.
extern u8 gActor143000KeypadPreviousView;

/// Completion-scene capture of background rows 39..128 in ten strips.
///
/// The capture task resets and writes `stripsCaptured`. Only the band's Y and
/// height select capture rows; every strip stores the full 320-pixel image width.
/// This writable record must stay loaded until its one running capture ends.
extern Actor143000CaptureArgs gActor143000CompletionUpperCaptureBand;

/// Completion-scene capture of background rows 138..212 in eight strips.
///
/// Follows the script's seventy-update wait after starting the upper band. The capture task
/// resets and writes `stripsCaptured`; it stores full-width rows and borrows
/// this writable record until that capture ends.
extern Actor143000CaptureArgs gActor143000CompletionLowerCaptureBand;

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
