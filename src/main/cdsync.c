#include "fs.h"

#include <psyq/sys/types.h>
#include <psyq/libetc.h>

#include "main/fs.h"

/// Shared command-recovery states, separate from seek/pause request progress.
enum {
    CD_SYNC_COMMAND_POLLING    = 0,
    CD_SYNC_COMMAND_RECOVERING = 1,
};

/// Recovery probe parameters and the response-byte-1 mask that rejects it.
///
/// The probe's first byte is preserved raw; its intended location and the
/// device meaning of the rejection bit are unproven.
enum {
    CD_SYNC_DISC_PROBE                 = 0,
    CD_SYNC_DISC_WAIT_SHELL_OPEN       = 1,
    CD_SYNC_RECOVERY_MODE_VBLANKS      = 3,
    CD_SYNC_RECOVERY_PROBE_MINUTE_BYTE = 0x0A,
    CD_SYNC_RECOVERY_READ_REJECTED     = 0x40,
    CD_SYNC_POLL_WITHOUT_WAIT          = 1,
};

/// Polls the pending command with shell-open recovery and no error-value override.
///
/// Returns CD_SYNC_PENDING, CD_SYNC_COMPLETE or CD_SYNC_RETRY. Shares state
/// with `cdSyncPollCommand`; never flushes or reissues the caller's command.
static inline s16 _cdSyncPollPendingCommand(void)
{
    CdCmdQueue* state;

    state = &gCdCmdQueue;
    switch (state->syncRecoveryStep) {
        case CD_SYNC_COMMAND_POLLING:
            switch (CdSync(CD_SYNC_POLL_WITHOUT_WAIT, NULL)) {
                case CdlComplete:
                    state->diskError = 0;
                    return CD_SYNC_COMPLETE;
                case CdlNoIntr:
                    break;
                case CdlDiskError:
                    state->diskError = 1;
                    if (cdSyncHasShellOpenStatus() != 0) {
                        state->syncRecoveryStep += 1;
                        break;
                    }
                    return CD_SYNC_RETRY;
                default:
                    return CD_SYNC_RETRY;
            }
            break;
        case CD_SYNC_COMMAND_RECOVERING:
            if (cdSyncPollDiscRecovery() != 0) {
                state->syncRecoveryStep = CD_SYNC_COMMAND_POLLING;
                return CD_SYNC_RETRY;
            }
            break;
        default:
            return CD_SYNC_PENDING;
    }
    return CD_SYNC_PENDING;
}

u16 cdSyncPollLogicalSeek(CdlLOC* location, s32 unused)
{
    CdCmdQueue* state;
    u8          unusedStackBytes[8]; // Unreferenced local retained for the matching stack frame.

    state = &gCdCmdQueue;
    switch (state->seekStep) {
        case CD_COMMAND_SEEK_SET_LOCATION:
            switch (_cdSyncPollPendingCommand()) {
                case CD_SYNC_PENDING:
                    return CD_SYNC_PENDING;
                case CD_SYNC_RETRY:
                    CdFlush();
                    /* fallthrough */
                case CD_SYNC_COMPLETE:
                    CdControlF(CdlSetloc, &location->minute);
                    state->seekStep++;
                    break;
            }
            /* fallthrough */
        case CD_COMMAND_SEEK_ISSUE:
            switch (_cdSyncPollPendingCommand()) {
                case CD_SYNC_PENDING:
                    return CD_SYNC_PENDING;
                case CD_SYNC_COMPLETE:
                    // Setloc must finish before the logical seek is issued.
                    CdControlF(CdlSeekL, NULL);
                    state->seekStep++;
                    break;
                case CD_SYNC_RETRY:
                    CdFlush();
                    state->seekStep = CD_COMMAND_SEEK_SET_LOCATION;
                    return CD_SYNC_PENDING;
            }
            /* fallthrough */
        case CD_COMMAND_SEEK_WAIT:
            switch (_cdSyncPollPendingCommand()) {
                case CD_SYNC_PENDING:
                    return CD_SYNC_PENDING;
                case CD_SYNC_COMPLETE:
                    state->seekStep  = CD_COMMAND_SEEK_SET_LOCATION;
                    state->diskError = 0;
                    return CD_SYNC_COMPLETE;
                case CD_SYNC_RETRY:
                    CdFlush();
                    state->seekStep = CD_COMMAND_SEEK_SET_LOCATION;
                    break;
            }
            return CD_SYNC_PENDING;
        default:
            return CD_SYNC_PENDING;
    }
}

u16 cdSyncPollPause(void)
{
    CdCmdQueue* state;

    state = &gCdCmdQueue;
    switch (state->pauseStep) {
        case CD_COMMAND_PAUSE_ISSUE:
            switch (_cdSyncPollPendingCommand()) {
                case CD_SYNC_PENDING:
                    return CD_SYNC_PENDING;
                case CD_SYNC_RETRY:
                    CdFlush();
                    /* fallthrough */
                case CD_SYNC_COMPLETE:
                    state->pauseRetryCount = 0;
                    CdControlF(CdlPause, NULL);
                    state->pauseStep++;
                    break;
            }
            /* fallthrough */
        case CD_COMMAND_PAUSE_WAIT:
            switch (_cdSyncPollPendingCommand()) {
                case CD_SYNC_PENDING:
                    return CD_SYNC_PENDING;
                case CD_SYNC_COMPLETE:
                    state->pauseStep = CD_COMMAND_PAUSE_ISSUE;
                    state->diskError = 0;
                    return CD_SYNC_COMPLETE;
                case CD_SYNC_RETRY:
                    state->pauseStep = CD_COMMAND_PAUSE_WAIT;
                    CdFlush();
                    CdControlF(CdlPause, NULL);
                    return CD_SYNC_PENDING;
                case CD_SYNC_ERROR_VALUE_MISMATCH:
                    // Retained branch: the pending-command poll never returns 3.
                    CdFlush();
                    CdControlF(CdlPause, NULL);
                    state->pauseStep = CD_COMMAND_PAUSE_WAIT;
                    state->pauseRetryCount++;
                    break;
            }
            return CD_SYNC_PENDING;
        default:
            return CD_SYNC_PENDING;
    }
}

/// Restores data-read mode and issues the recovery probe into SDK response storage.
///
/// `commandBuffer` needs one parameter byte, `probeParameters` needs three,
/// and `readResult` holds the SDK response (up to eight bytes). Calls block.
static inline void _cdSyncIssueRecoveryReadProbe(u8* commandBuffer, u8* probeParameters, u8* readResult)
{
    CdControlB(CdlGetTN, NULL, NULL);
    commandBuffer[0] = CdlModeSpeed | CdlModeSize1;
    CdControlB(CdlSetmode, commandBuffer, NULL);
    VSync(CD_SYNC_RECOVERY_MODE_VBLANKS);
    probeParameters[0] = CD_SYNC_RECOVERY_PROBE_MINUTE_BYTE;
    probeParameters[1] = 0;
    probeParameters[2] = 0;
    CdControlB(CdlReadN, probeParameters, readResult);
}

s16 cdSyncPollDiscRecovery(void)
{
    u8          commandBuffer[8];
    u8          readResult[8];
    u8          probeParameters[8];
    CdCmdQueue* state;
    s32         discReady;
    s32         shellOpened;

    state = &gCdCmdQueue;
    switch (state->diskRecoveryStep) {
        case CD_SYNC_DISC_PROBE:
            discReady = CdDiskReady(CD_SYNC_POLL_WITHOUT_WAIT);
            if (discReady == CdlComplete) {
                discReady = 1;
            } else {
                discReady = 0;
            }
            if (discReady != 0) {
                // Probe only after TOC readiness and the data-mode settling delay.
                _cdSyncIssueRecoveryReadProbe(commandBuffer, probeParameters, readResult);
                if ((readResult[0] & CdlStatError) && (readResult[1] & CD_SYNC_RECOVERY_READ_REJECTED)) {
                    state->diskRecoveryStep += 1;
                } else {
                    state->diskRecoveryStep = CD_SYNC_DISC_PROBE;
                    return CD_SYNC_COMPLETE;
                }
            }
            break;
        case CD_SYNC_DISC_WAIT_SHELL_OPEN:
            CdControlB(CdlNop, NULL, commandBuffer);
            shellOpened = commandBuffer[0] & CdlStatShellOpen;
            if (shellOpened == CdlStatShellOpen) {
                shellOpened = 1;
            } else {
                shellOpened = 0;
            }
            if (shellOpened != 0) {
                // The binary restarts movie progress but keeps recovery in this state.
                state->movieStep = CD_COMMAND_MOVIE_WAIT_READY;
            }
            break;
        default:
            break;
    }
    return CD_SYNC_PENDING;
}

s16 cdSyncPollCommand(s32 diskErrorValue, s32 acceptedDiskErrorValue)
{
    CdCmdQueue* state;
    s32         interrupt;
    u16         acceptedValue;

    state = &gCdCmdQueue;
    switch (state->syncRecoveryStep) {
        case CD_SYNC_COMMAND_POLLING:
            interrupt = CdSync(CD_SYNC_POLL_WITHOUT_WAIT, NULL);
            switch (interrupt) {
                case CdlNoIntr:
                    break;
                case CdlComplete:
                    state->diskError = 0;
                    return CD_SYNC_COMPLETE;
                case CdlDiskError:
                    state->diskError = 1;
                    if (cdSyncHasShellOpenStatus() != 0) {
                        state->syncRecoveryStep++;
                        break;
                    }
                    acceptedValue = acceptedDiskErrorValue & 0xFFFF;
                    if (acceptedValue == 0) {
                        return CD_SYNC_RETRY;
                    }
                    if ((diskErrorValue & 0xFFFF) == acceptedValue) {
                        return CD_SYNC_COMPLETE;
                    }
                    return CD_SYNC_ERROR_VALUE_MISMATCH;
                default:
                    return CD_SYNC_RETRY;
            }
            break;
        case CD_SYNC_COMMAND_RECOVERING:
            if (cdSyncPollDiscRecovery() != 0) {
                state->syncRecoveryStep = CD_SYNC_COMMAND_POLLING;
                return CD_SYNC_RETRY;
            }
            break;
        default:
            return CD_SYNC_PENDING;
    }
    return CD_SYNC_PENDING;
}

s16 cdSyncHasShellOpenStatus(void)
{
    s16 shellOpenStatus;
    u8  commandResult[8];

    CdControlB(CdlNop, NULL, commandResult);
    shellOpenStatus = commandResult[0] & CdlStatShellOpen;
    return shellOpenStatus != 0;
}

/// Tests whether the nonwaiting TOC-readiness check reports completion.
static bool _cdSyncIsDiscReady(void)
{
    return CdDiskReady(CD_SYNC_POLL_WITHOUT_WAIT) == CdlComplete;
}
