#ifndef MAIN_PRIVATE_BOOT_H
#define MAIN_PRIVATE_BOOT_H

#include "types.h"

#include "main/task_types.h"

/// Initializes the CD-audio player for a fresh game session.
///
/// Uses `cdAudioInit`'s software-reset contract: previous stream activity and
/// callbacks must have ended before its buffers and voice state are reused.
void bootInitCdAudio(void);

/// Initializes the CD library, double-speed reads, audio player and request queue.
///
/// Call after sound initialization, before issuing CD requests. Selects
/// 2048-byte sectors without headers; library/command failures are not reported.
void bootInitCd(void);

/// SDK reset policies accepted by `bootResetCd`.
enum {
    BOOT_CD_RESET_DRIVE            = 0,
    BOOT_CD_RESET_DRIVE_AND_VOLUME = 1,
    BOOT_CD_RESET_INTERRUPTS       = 2,
};

/// Pauses the drive, detaches an active movie ring and resets CD request state.
///
/// Cancel CD audio before calling; this leaves the audio player's state intact.
/// `resetMode` is a BOOT_CD_RESET_* policy passed unchanged to the SDK. Uses
/// three-VBlank SDK pacing before pausing, relative to the preceding paced
/// VSync call. Restores double-speed, 2048-byte reads without headers. Discards
/// queued requests without freeing their buffers; drive reset and mode-command
/// failures are not reported.
void bootResetCd(s32 resetMode);

void Boot_LoadTask(Task* task);

#endif // MAIN_PRIVATE_BOOT_H
