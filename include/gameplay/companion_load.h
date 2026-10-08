#ifndef GAMEPLAY_COMPANION_LOAD_H
#define GAMEPLAY_COMPANION_LOAD_H

#include "main/task_types.h"

/// Empty entry point called first in a pair before Acropolis scene transitions.
///
/// Its intended role is unproven.
void func_800ABFF8(void);

/// Empty entry point called second in a pair before Acropolis scene transitions.
///
/// Its intended role is unproven.
void func_800AC000(void);

/// Resident task-bank entry for the gameplay session-reload dispatcher.
enum {
    GAME_FLOW_RELOAD_TASK_BANK = 0,
    GAME_FLOW_RELOAD_TASK_SLOT = 17,
};

/// First spawn-argument options for `gameFlowReloadSessionTask`.
///
/// The low nibble is forwarded to the loading task: 0 captures the current
/// framebuffer before loading; 1 clears the image source before rebuilding.
/// Bit 4 skips battle-escape handling, for callers that already requested it.
enum {
    GAME_FLOW_RELOAD_CAPTURE_FRAME      = 0,
    GAME_FLOW_RELOAD_BLANK_DISPLAY      = 1,
    GAME_FLOW_RELOAD_DISPLAY_MODE_MASK  = 0xF,
    GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE = 0x10,
};

/// Reloads the session from the live save through combat exit, display hold and resource rebuild.
///
/// Spawn via `GAME_FLOW_RELOAD_TASK_BANK` / `GAME_FLOW_RELOAD_TASK_SLOT` while
/// gameplay is loaded. The first argument combines a display-mode nibble with
/// `GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE`; the second argument is unused.
/// Requires valid live save/session state and dispatch state 0..2. Every tick
/// renews pad 0's 61-update input block and pauses actor updates before one
/// phase runs. Death presentation cancels the first phase; the final phase
/// replaces the task lists and heaps and starts loading, invalidating the task.
void gameFlowReloadSessionTask(Task* task);

#endif // GAMEPLAY_COMPANION_LOAD_H
