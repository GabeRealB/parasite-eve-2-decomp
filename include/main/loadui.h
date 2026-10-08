#ifndef MAIN_LOADUI_H
#define MAIN_LOADUI_H

#include "types.h"

extern u8 D_800626E8;

extern u8 D_8007A394;

extern s16 D_8007A396;

/// Results of advancing the disk-swap prompt, retained as a byte.
enum {
    LOAD_UI_DISK_SWAP_COMPLETE = 0,
    LOAD_UI_DISK_SWAP_PENDING  = 0xFF,
};

/// Advances the disk-swap prompt until the current stage's required disc is present.
///
/// Returns LOAD_UI_DISK_SWAP_PENDING while the prompt is active, or
/// LOAD_UI_DISK_SWAP_COMPLETE when no swap is needed or dismissal has finished.
/// Reset `D_8007A394` to zero before starting a new check, then call from the
/// loading task until completion. Requires a valid session stage and available
/// prompt resource/drawing storage. Drive probing and directory scanning can block
/// without a timeout. Read failures and rejected discs repeat the prompt.
/// Blocks game pause and selects modal presentation until successful dismissal;
/// completion does not reset the state byte for another check.
u8 loadUiPollDiskSwap(void);

#endif // MAIN_LOADUI_H
