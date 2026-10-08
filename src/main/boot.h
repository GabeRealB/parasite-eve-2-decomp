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

/// Discovers the startup files and shows the cold-boot image before starting the title.
///
/// Resident bank 0, slot 31 starts at state zero. States 0..4 decode INIT.BS,
/// fade it in, hold it for at least 90 callbacks while stage-zero file 1 loads,
/// fade it out, then clear the image workspace and queue title initialization.
/// `killCountdown` holds byte brightness or callback counts, independent of
/// frameTicks. Requires initialized CD, display and decoder state; directory
/// discovery and the initial decode block until completion. Spawn payloads are
/// ignored. The final state kills this task and disables external debug hooks.
void bootColdStartTask(Task* task);

/// Reloads stage-zero file 1 and hands off to the title's startup descriptor.
///
/// Resident bank 0, slot 32 starts at state zero with the stage-zero file table
/// already mounted. State 0 hides the display and queues normal loading; state
/// 1 waits for an idle CD queue, restores the display, reconfigures image memory
/// and kills this task after spawning the title. Spawn payloads are ignored.
void bootReloadTitleTask(Task* task);

#endif // MAIN_PRIVATE_BOOT_H
