#ifndef MAIN_LOADUI_H
#define MAIN_LOADUI_H

#include "types.h"

/// Phases of the disk-swap presentation, stored in a byte.
enum {
    LOAD_UI_DISK_SWAP_CHECK_REQUIRED_DISC = 0,
    LOAD_UI_DISK_SWAP_WAIT_PROMPT_LOAD    = 1,
    LOAD_UI_DISK_SWAP_DELAY_BEFORE_PROBE  = 2,
    LOAD_UI_DISK_SWAP_PROBE_DISC          = 3,
    LOAD_UI_DISK_SWAP_VALIDATE_DISC       = 4,
    LOAD_UI_DISK_SWAP_DISMISS_PROMPT      = 5,
};

/// Resident disk-swap phase shared by session loading and prompt polling.
///
/// Set to LOAD_UI_DISK_SWAP_CHECK_REQUIRED_DISC before a new check. Polling
/// advances through the named phases; retries return to WAIT_PROMPT_LOAD.
/// Completion leaves the phase unchanged, so a later check needs another reset.
extern u8 gLoadUiDiskSwapState;

/// Results of advancing the disk-swap prompt, retained as a byte.
enum {
    LOAD_UI_DISK_SWAP_COMPLETE = 0,
    LOAD_UI_DISK_SWAP_PENDING  = 0xFF,
};

/// Advances the disk-swap prompt until the current stage's required disc is present.
///
/// Returns LOAD_UI_DISK_SWAP_PENDING while the prompt is active, or
/// LOAD_UI_DISK_SWAP_COMPLETE when no swap is needed or dismissal has finished.
/// Reset `gLoadUiDiskSwapState` to LOAD_UI_DISK_SWAP_CHECK_REQUIRED_DISC before
/// starting, then call from the loading task until completion. Requires a valid
/// session stage and available prompt resource/drawing storage. Drive probing
/// and directory scanning can block
/// without a timeout. Read failures and rejected discs repeat the prompt.
/// Blocks game pause and selects modal presentation until successful dismissal;
/// completion does not reset the state byte for another check.
u8 loadUiPollDiskSwap(void);

#endif // MAIN_LOADUI_H
