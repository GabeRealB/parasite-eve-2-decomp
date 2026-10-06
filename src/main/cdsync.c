#include "main/fs.h"

#include <psyq/sys/types.h>
#include <psyq/libcd.h>
#include <psyq/libetc.h>

#include "types.h"

#include "fs.h"
#include "main/fs_types.h"

static bool CdSync_CanIssueCommand(void);

/// One poll of the drive's pending command: 0 while it is still running (or
/// the disc is being recovered), 1 when it completed, 2 when it has to be
/// issued again.
static inline s16 CdCmd_SyncPoll(void)
{
    CdCmdQueue* state;

    state = &gCdCmdQueue;
    switch (state->syncRecoveryStep) {
        case 0:
            switch (CdSync(1, NULL)) {
                case CdlComplete:
                    state->diskError = 0;
                    return 1;
                case CdlNoIntr:
                    break;
                case CdlDiskError:
                    state->diskError = 1;
                    if (CdSync_IsShellOpenBitSet() != 0) {
                        state->syncRecoveryStep += 1;
                        break;
                    }
                    return 2;
                default:
                    return 2;
            }
            break;
        case 1:
            if (CdCmd_RecoverDisk() != 0) {
                state->syncRecoveryStep = 0;
                return 2;
            }
            break;
        default:
            return 0;
    }
    return 0;
}

s32 CdCmd_SeekL(u8* loc, s32 unused)
{
    CdCmdQueue* state;
    u8          pad[8];

    state = &gCdCmdQueue;
    switch (state->seekStep) {
        case CD_COMMAND_SEEK_SET_LOCATION:
            switch (CdCmd_SyncPoll()) {
                case 0:
                    return 0;
                case 2:
                    CdFlush();
                    /* fallthrough */
                case 1:
                    CdControlF(CdlSetloc, loc);
                    state->seekStep++;
                    break;
            }
            /* fallthrough */
        case CD_COMMAND_SEEK_ISSUE:
            switch (CdCmd_SyncPoll()) {
                case 0:
                    return 0;
                case 1:
                    CdControlF(CdlSeekL, NULL);
                    state->seekStep++;
                    break;
                case 2:
                    CdFlush();
                    state->seekStep = CD_COMMAND_SEEK_SET_LOCATION;
                    return 0;
            }
            /* fallthrough */
        case CD_COMMAND_SEEK_WAIT:
            switch (CdCmd_SyncPoll()) {
                case 0:
                    return 0;
                case 1:
                    state->seekStep  = CD_COMMAND_SEEK_SET_LOCATION;
                    state->diskError = 0;
                    return 1;
                case 2:
                    CdFlush();
                    state->seekStep = CD_COMMAND_SEEK_SET_LOCATION;
                    break;
            }
            return 0;
        default:
            return 0;
    }
}

s32 CdCmd_PausePoll(void)
{
    CdCmdQueue* state;

    state = &gCdCmdQueue;
    switch (state->pauseStep) {
        case CD_COMMAND_PAUSE_ISSUE:
            switch (CdCmd_SyncPoll()) {
                case 0:
                    return 0;
                case 2:
                    CdFlush();
                    /* fallthrough */
                case 1:
                    state->pauseRetryCount = 0;
                    CdControlF(CdlPause, NULL);
                    state->pauseStep++;
                    break;
            }
            /* fallthrough */
        case CD_COMMAND_PAUSE_WAIT:
            switch (CdCmd_SyncPoll()) {
                case 0:
                    return 0;
                case 1:
                    state->pauseStep = CD_COMMAND_PAUSE_ISSUE;
                    state->diskError = 0;
                    return 1;
                case 2:
                    state->pauseStep = CD_COMMAND_PAUSE_WAIT;
                    CdFlush();
                    CdControlF(CdlPause, NULL);
                    return 0;
                case 3:
                    CdFlush();
                    CdControlF(CdlPause, NULL);
                    state->pauseStep = CD_COMMAND_PAUSE_WAIT;
                    state->pauseRetryCount++;
                    break;
            }
            return 0;
        default:
            return 0;
    }
}

s16 CdCmd_RecoverDisk(void)
{
    u8          mode[8];
    u8          result[8];
    u8          loc[8];
    CdCmdQueue* state;
    s32         temp;

    state = &gCdCmdQueue;
    switch (state->diskRecoveryStep) {
        case 0:
            temp = CdDiskReady(1);
            if (temp == CdlComplete) {
                temp = 1;
            } else {
                temp = 0;
            }
            if (temp != 0) {
                CdControlB(CdlGetTN, NULL, NULL);
                mode[0] = CdlModeSpeed | CdlModeSize1;
                CdControlB(CdlSetmode, mode, NULL);
                VSync(3);
                loc[0] = 0xA;
                loc[1] = 0;
                loc[2] = 0;
                CdControlB(CdlReadN, loc, result);
                if ((result[0] & CdlStatError) && (result[1] & 0x40)) {
                    state->diskRecoveryStep += 1;
                } else {
                    state->diskRecoveryStep = 0;
                    return 1;
                }
            }
            break;
        case 1:
            CdControlB(CdlNop, NULL, mode);
            temp = mode[0] & CdlStatShellOpen;
            if (temp == CdlStatShellOpen) {
                temp = 1;
            } else {
                temp = 0;
            }
            if (temp != 0) {
                state->movieStep = CD_COMMAND_MOVIE_WAIT_READY;
            }
            break;
        default:
            break;
    }
    return 0;
}

s32 CdCmd_PollStatus(s32 arg0, s32 arg1)
{
    CdCmdQueue* state;
    s32         status;
    u16         a1;

    state = &gCdCmdQueue;
    switch (state->syncRecoveryStep) {
        case 0:
            status = CdSync(1, NULL);
            switch (status) {
                case CdlNoIntr:
                    break;
                case CdlComplete:
                    state->diskError = 0;
                    return 1;
                case CdlDiskError:
                    state->diskError = 1;
                    if (CdSync_IsShellOpenBitSet() != 0) {
                        state->syncRecoveryStep++;
                        break;
                    }
                    a1 = arg1 & 0xFFFF;
                    if (a1 == 0) {
                        return 2;
                    }
                    if ((arg0 & 0xFFFF) == a1) {
                        return 1;
                    }
                    return 3;
                default:
                    return 2;
            }
            break;
        case 1:
            if (CdCmd_RecoverDisk() != 0) {
                state->syncRecoveryStep = 0;
                return 2;
            }
            break;
        default:
            return 0;
    }
    return 0;
}

s16 CdSync_IsShellOpenBitSet(void)
{
    s16 tmp;
    u8  result[8];

    // Writing it as (result[0] & CdlStatShellOpen) != 0 produces the wrong code
    CdControlB(CdlNop, NULL, result);
    tmp = result[0] & CdlStatShellOpen;
    return tmp != 0;
}

static bool CdSync_CanIssueCommand(void)
{
    return CdDiskReady(1) == CdlComplete;
}
