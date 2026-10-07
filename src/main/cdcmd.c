#include "main/fs.h"

#include <psyq/sys/types.h>
#include <psyq/libcd.h>
#include <psyq/libpress.h>

#include "types.h"

#include "main/cdaudio.h"
#include "cdaudio.h"
#include "main/cdaudio_types.h"
#include "main/display.h"
#include "main/display_types.h"
#include "fs.h"
#include "main/fs_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "stream.h"
#include "main/stream_types.h"

#include "gameplay/scene_runtime.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_neo_ark.h"

#include "rooms/acropolis_plaza.h"

static void* D_8006AC00;

static u16 CdCmd_EntryIter;

static s32* CdCmd_MapHeapSizes[];

static void CdCmd_HandleStreamDecode(void);

static void CdCmd_HandleFileLoad(void);

static void CdCmd_HandleMount(void);

static void CdCmd_ProcessPhase1(void);

static void CdCmd_ProcessPhase2(void);

static inline s32 _cdCmdEnqueue(s32 command, const void* fileKey, const void* commandArgs);

static inline u16 _cdCmdIsIdle(void);

static s32 _cdCmdGetSceneSelectionStatus(void);

static s16 _cdCmdGetSceneAudioMode(void);

static void CdCmd_UnusedStub1(void);

static void CdCmd_UnusedStub2(void);

static void _cdCmdStageScenePlayback(void);

static void _cdCmdResetRing(void);

static s32* CdCmd_MapHeapSizes[] = {
    NULL,
    D_map_akropolis_8017A0F8,
    D_map_dryfield_80179B4C,
    NULL,
    NULL,
    D_map_neo_ark_80179EC8,
    NULL,
    NULL,
    NULL,
    NULL,
};

void* cdCmdReservePlaybackBuffers(void)
{
    enum { CD_COMMAND_TITLE_MOVIE_WORKSPACE_BYTES = 0x4B000 };
    CdCmdQueue* queue;
    u16         vlcBufferKind;
    u8          timingBufferKind;
    s32*        areaWorkspaceBytes;
    s32         workspaceBytes;

    queue = &gCdCmdQueue;
    if (queue->sceneBuffersNeeded != 0) {
        // Reserve scene storage first so later movie/model allocations cannot consume it.
        if (queue->sceneVlcTableMode == STREAM_SCENE_VLC_RESERVED_TABLE) {
            vlcBufferKind = queue->sceneStream->data.scene.vlcBufferKind;
            switch (vlcBufferKind) {
                case STREAM_VLC_BUFFER_ALLOCATE:
                    queue->vlcTable = memMalloc(STREAM_VLC_TABLE_BYTES, true);
                    break;
                case STREAM_VLC_BUFFER_ACTOR_0:
                    gGameSession->field_7C = 0;
                    queue->vlcTable        = Fs_ActorLoadBase0;
                    break;
                case STREAM_VLC_BUFFER_ACTOR_1:
                    gGameSession->field_7E = 0;
                    queue->vlcTable        = Fs_ActorLoadBase1;
                    break;
                case STREAM_VLC_BUFFER_ACTOR_2:
                    gGameSession->field_80 = 0;
                    queue->vlcTable        = Fs_ActorLoadBase2;
                    break;
            }
            if (queue->vlcTableBuilt == 0) {
                DecDCTvlcBuild(queue->vlcTable);
                queue->vlcTableBuilt = 1;
            }
        } else {
            queue->vlcTable = NULL;
        }

        queue->timingBuffer = NULL;
        timingBufferKind    = queue->sceneStream->control.scene.timingBufferKind;
        // Matching VLC/actor selectors reserve the table prefix before timing words.
        switch (timingBufferKind) {
            case STREAM_TIMING_BUFFER_ALLOCATE:
                queue->timingBuffer = memMalloc(queue->sceneStream->data.scene.timingBufferBytes, true);
                break;
            case STREAM_TIMING_BUFFER_ACTOR_0:
                gGameSession->field_7C = 0;
                queue->timingBuffer    = Fs_ActorLoadBase0;
                if (queue->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_0) {
                    queue->timingBuffer = (u32*)((u8*)Fs_ActorLoadBase0 + STREAM_VLC_TABLE_BYTES);
                }
                break;
            case STREAM_TIMING_BUFFER_ACTOR_1:
                gGameSession->field_7E = 0;
                queue->timingBuffer    = Fs_ActorLoadBase1;
                if (queue->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_1) {
                    queue->timingBuffer = (u32*)((u8*)Fs_ActorLoadBase1 + STREAM_VLC_TABLE_BYTES);
                }
                break;
            case STREAM_TIMING_BUFFER_ACTOR_2:
                gGameSession->field_80 = 0;
                queue->timingBuffer    = Fs_ActorLoadBase2;
                if (queue->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_2) {
                    queue->timingBuffer = (u32*)((u8*)Fs_ActorLoadBase2 + STREAM_VLC_TABLE_BYTES);
                }
                break;
        }

        queue->timingCursor = queue->timingBuffer;
        if (queue->decodeBufferBytes != 0) {
            queue->decodeBuffer = memMalloc(queue->decodeBufferBytes, true);
        }
    }

    D_8006AC00 = NULL;
    if (gGameSession->location.loc.stage == GAME_STAGE_NONE) {
        D_8006AC00 = memMalloc(CD_COMMAND_TITLE_MOVIE_WORKSPACE_BYTES, true);
    } else if (streamFindMovieSlot(&gGameSession->location.loc, 0, 0) < 0) {
        return NULL;
    } else {
        areaWorkspaceBytes = CdCmd_MapHeapSizes[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage];
        if (areaWorkspaceBytes != NULL) {
            workspaceBytes = areaWorkspaceBytes[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area];
            if (workspaceBytes != 0) {
                D_8006AC00 = memMalloc(workspaceBytes, true);
            }
        }
    }
    return D_8006AC00;
}

static void CdCmd_HandleStreamDecode(void)
{
    CdCmdQueue* state;
    CdCmdQueue* p;
    CdCmdEntry* entry;
    s32         slotIndex;
    s32         cmd;
    s32         busy;

    // Keep the unsigned slot-byte load separate from its signed interpretation.
    state     = &gCdCmdQueue;
    slotIndex = *(volatile u8*)&state->entries[state->readIdx].args.bytes[0];
    cmd       = state->entries[state->readIdx].cmd;
    slotIndex = (s8)slotIndex;
    if (cmd == CD_COMMAND_EMPTY) {
        return;
    }
    if (cmd < 0) {
        return;
    }
    if (cmd >= CD_COMMAND_CONTINUE_STREAM + 1) {
        return;
    }
    if (cmd < CD_COMMAND_PLAY_STREAM) {
        return;
    }
    switch (state->step) {
        case 0:
            if (state->busy == 0) {
                state->busy          = 1;
                gDisplayState.cdBusy = DISPLAY_CD_BUSY;
            }
            switch (cdSyncPollCommand(0, 0)) {
                case CD_SYNC_PENDING:
                    return;
                case CD_SYNC_RETRY:
                    CdFlush();
                    /* fallthrough */
                case CD_SYNC_COMPLETE:
                    entry = &state->entries[state->readIdx];
                    if (entry->cmd == CD_COMMAND_PLAY_STREAM) {
                        D_8005EAEC = 0;
                        D_8005EAEE = 0;
                    } else if (entry->cmd == CD_COMMAND_CONTINUE_STREAM) {
                        entry->cmd = CD_COMMAND_PLAY_STREAM;
                    }
                    if ((s16)Stream_InitializePlayback(slotIndex & 0xFFFF) != 0) {
                        p                        = &gCdCmdQueue;
                        busy                     = p->busy;
                        state->movieReady        = 1;
                        state->movieFrameChanged = 1;
                        if (busy != 0) {
                            p->busy              = 0;
                            gDisplayState.cdBusy = DISPLAY_CD_IDLE;
                        }
                        p->step               = 0;
                        p->cancelStep         = CD_COMMAND_CANCEL_BEGIN;
                        p->pausePlayClock     = 0;
                        p->cdOperationPending = 0;
                        if (p->readIdx != p->writeIdx) {
                            p->entries[p->readIdx].cmd = CD_COMMAND_EMPTY;
                            p->readIdx                 = p->readIdx + 1;
                            p->readIdx                 = p->readIdx % ARRAY_SIZE(p->entries);
                        }
                        break;
                    }
                    state->continueMovie = 1;
                    Stream_PollPlayback(0, ((u16)state->movieFrame - 1) * 0xA);
                    state->step = state->step + 1;
                    break;
            }
            break;
        case 1:
            break;
        default:
            return;
    }
    if ((s16)Stream_PollPlayback(0, ((u16)state->movieFrame - 1) * 0xA) != 0) {
        p = &gCdCmdQueue;
        if (p->busy != 0) {
            p->busy              = 0;
            gDisplayState.cdBusy = DISPLAY_CD_IDLE;
        }
        p->step               = 0;
        p->cancelStep         = CD_COMMAND_CANCEL_BEGIN;
        p->pausePlayClock     = 0;
        p->cdOperationPending = 0;
        if (p->readIdx != p->writeIdx) {
            p->entries[p->readIdx].cmd = CD_COMMAND_EMPTY;
            p->readIdx                 = p->readIdx + 1;
            p->readIdx                 = p->readIdx % ARRAY_SIZE(p->entries);
        }
    }
}

static void CdCmd_HandleFileLoad(void)
{
    CdCmdQueue* state;
    CdCmdQueue* p;
    s32         status;
    u8          req[4];
    u8          mode;

    // The file key's hundreds/folder-suffix byte is stored with the load options.
    state  = &gCdCmdQueue;
    req[3] = state->entries[state->readIdx].stage;
    req[2] = state->entries[state->readIdx].fileGroup;
    req[0] = state->entries[state->readIdx].fileIndex;
    req[1] = state->entries[state->readIdx].args.file.fileIdHundreds;

    switch (state->step) {
        case 0:
            p = &gCdCmdQueue;
            {
                s32 busy;
                busy                  = p->busy;
                state->pausePlayClock = 1;
                if (busy == 0) {
                    p->busy              = 1;
                    gDisplayState.cdBusy = DISPLAY_CD_BUSY;
                }
            }
            switch (cdSyncPollCommand(0, 0)) {
                case CD_SYNC_PENDING:
                    return;
                case CD_SYNC_RETRY:
                    CdFlush();
                    /* fallthrough */
                case CD_SYNC_COMPLETE:
                    break;
                default:
                    goto end_check;
            }
            mode = 0xA0;
            CdControlB(CdlSetmode, &mode, NULL);
            if (state->entries[state->readIdx].args.file.loadMode != CD_COMMAND_LOAD_DEFAULT || req[3] == 0 || req[0] != 0) {
                state->step = 4;
                goto do_load;
            }
            state->step = state->step + 1;
            /* fallthrough */
        case 1:
            fsStartFolderDirectoryRead(req[3], req[2], req[1]);
            state->step = state->step + 1;
            break;
        case 2:
            if (CdSync(1, NULL) == CdlDiskError) {
                CdSyncCallback(NULL);
                CdReadyCallback(NULL);
                cdSyncWaitForReadableDisc(1);
                state->step = 1;
                break;
            }
            fsAbortTimedOutOperation();
            status = Fs_CdOpStatus;
            switch (status) {
                case 0x80:
                    switch (cdSyncPollCommand(0, 0)) {
                        case CD_SYNC_PENDING:
                            return;
                        case CD_SYNC_RETRY:
                            CdFlush();
                            /* fallthrough */
                        case CD_SYNC_COMPLETE:
                            if (CdSync(1, NULL) == CdlDiskError) {
                                cdSyncWaitForReadableDisc(1);
                            }
                            state->step = 1;
                            break;
                    }
                    break;
                case 0xFF:
                    CdSyncCallback(NULL);
                    CdReadyCallback(NULL);
                    state->step = state->step + 1;
                    break;
                case 0x10:
                case 0x20:
                case 0x40:
                    switch (cdSyncPollCommand(0, 0)) {
                        case CD_SYNC_PENDING:
                            return;
                        case CD_SYNC_RETRY:
                            CdFlush();
                            /* fallthrough */
                        case CD_SYNC_COMPLETE:
                            Fs_RetryReadN();
                            break;
                    }
                    break;
            }
            break;
        case 3:
            switch (cdSyncPollCommand(0, 0)) {
                case CD_SYNC_PENDING:
                    return;
                case CD_SYNC_RETRY:
                    CdFlush();
                    /* fallthrough */
                case CD_SYNC_COMPLETE:
                    fsBuildFolderTables(req[3], req[2], req[1]);
                    state->step = state->step + 1;
                    break;
            }
            /* fallthrough */
        case 4:
        do_load:
            Fs_LoadFile(
                req,
                (u8)state->entries[state->readIdx].args.file.loadMode,
                state->entries[state->readIdx].args.file.imageXPageOffset,
                state->entries[state->readIdx].args.file.imageYOffset);
            state->step = state->step + 1;
            break;
        case 5:
            if (CdSync(1, NULL) == CdlDiskError) {
                CdSyncCallback(NULL);
                CdReadyCallback(NULL);
                cdSyncWaitForReadableDisc(1);
                state->step = 4;
                break;
            }
            fsAbortTimedOutOperation();
            status = Fs_CdOpStatus;
            switch (status) {
                case 0x80:
                    switch (cdSyncPollCommand(0, 0)) {
                        case CD_SYNC_PENDING:
                            return;
                        case CD_SYNC_RETRY:
                            CdFlush();
                            /* fallthrough */
                        case CD_SYNC_COMPLETE:
                            if (CdSync(1, NULL) == CdlDiskError) {
                                cdSyncWaitForReadableDisc(1);
                            }
                            state->step = 4;
                            break;
                    }
                    break;
                case 0xFF:
                    if (state->imageLoadStatus != status) {
                        break;
                    }
                    CdSyncCallback(NULL);
                    CdReadyCallback(NULL);
                    p = &gCdCmdQueue;
                    if (p->busy != 0) {
                        p->busy              = 0;
                        gDisplayState.cdBusy = DISPLAY_CD_IDLE;
                    }
                    p->step               = 0;
                    p->cancelStep         = CD_COMMAND_CANCEL_BEGIN;
                    p->pausePlayClock     = 0;
                    p->cdOperationPending = 0;
                    if (p->readIdx != p->writeIdx) {
                        p->entries[p->readIdx].cmd = CD_COMMAND_EMPTY;
                        p->readIdx                 = p->readIdx + 1;
                        p->readIdx                 = p->readIdx % ARRAY_SIZE(p->entries);
                    }
                    break;
                case 0x10:
                case 0x20:
                case 0x40:
                    switch (cdSyncPollCommand(0, 0)) {
                        case CD_SYNC_PENDING:
                            return;
                        case CD_SYNC_COMPLETE:
                            Fs_RetryReadN();
                            break;
                        case CD_SYNC_RETRY:
                            CdFlush();
                            Fs_RetryReadN();
                            break;
                    }
                    break;
            }
            break;
    }

end_check:
    if (state->imageDecodePending != 0) {
        mdecStepImageDecode();
    }
}

static void CdCmd_HandleMount(void)
{
    CdCmdQueue* state;
    s32         cmd;
    s32         status;

    state = &gCdCmdQueue;
    cmd   = state->entries[state->readIdx].cmd;
    if (cmd < CD_COMMAND_MOUNT_STAGE) {
        return;
    }
    switch (cmd) {
        case CD_COMMAND_MOUNT_STAGE: {
            CdCmdEntry* entry;
            s32         stageIndex;
            s32         step;

            entry      = &state->entries[state->readIdx];
            status     = *(volatile u8*)&entry->stage;
            stageIndex = status;
            step       = state->step;
            stageIndex = (s8)stageIndex;
            switch (step) {
                case 0:
                    if (state->busy == 0) {
                        state->busy          = 1;
                        gDisplayState.cdBusy = DISPLAY_CD_BUSY;
                    }
                    fsStartStageFolderListRead(stageIndex & 0xFF);
                    state->step = state->step + 1;
                    return;
                case 1:
                    if (CdSync(1, NULL) == CdlDiskError) {
                        CdSyncCallback(NULL);
                        CdReadyCallback(NULL);
                        cdSyncWaitForReadableDisc(1);
                        state->step = 0;
                        return;
                    }
                    fsAbortTimedOutOperation();
                    status = Fs_CdOpStatus;
                    switch (status) {
                        case 0x80:
                            switch (cdSyncPollCommand(0, 0)) {
                                case CD_SYNC_RETRY:
                                    CdFlush();
                                    /* fallthrough */
                                case CD_SYNC_COMPLETE:
                                    if (CdSync(1, NULL) == CdlDiskError) {
                                        cdSyncWaitForReadableDisc(1);
                                    }
                                    state->step = 0;
                                    return;
                                case CD_SYNC_PENDING:
                                default:
                                    return;
                            }
                        case 0xFF:
                            CdSyncCallback(NULL);
                            CdReadyCallback(NULL);
                            state->step = state->step + 1;
                            return;
                        case 0x10:
                        case 0x20:
                        case 0x40:
                            switch (cdSyncPollCommand(0, 0)) {
                                case CD_SYNC_RETRY:
                                    CdFlush();
                                    /* fallthrough */
                                case CD_SYNC_COMPLETE:
                                    Fs_RetryReadN();
                                    return;
                                case CD_SYNC_PENDING:
                                default:
                                    return;
                            }
                    }
                    return;
                case 2:
                    switch (cdSyncPollCommand(0, 0)) {
                        case CD_SYNC_RETRY:
                            CdFlush();
                            /* fallthrough */
                        case CD_SYNC_COMPLETE:
                            fsInitFolderTable(stageIndex & 0xFF);
                            if (state->busy != 0) {
                                state->busy          = 0;
                                gDisplayState.cdBusy = DISPLAY_CD_IDLE;
                            }
                            state->step               = 0;
                            state->cancelStep         = CD_COMMAND_CANCEL_BEGIN;
                            state->pausePlayClock     = 0;
                            state->cdOperationPending = 0;
                            if (state->readIdx != state->writeIdx) {
                                (state->entries + state->readIdx)->cmd = CD_COMMAND_EMPTY;
                                state->readIdx                         = state->readIdx + 1;
                                state->readIdx                         = state->readIdx % ARRAY_SIZE(state->entries);
                            }
                            return;
                        case CD_SYNC_PENDING:
                        default:
                            return;
                    }
            }
            return;
        }
        case CD_COMMAND_READ_STAGE_HEADER:
            status = Fs_CdOpStatus;
            if (status == 0xFF) {
                if (state->busy != 0) {
                    state->busy          = 0;
                    gDisplayState.cdBusy = DISPLAY_CD_IDLE;
                }
                state->step               = 0;
                state->cancelStep         = CD_COMMAND_CANCEL_BEGIN;
                state->pausePlayClock     = 0;
                state->cdOperationPending = 0;
                if (state->readIdx != state->writeIdx) {
                    (state->entries + state->readIdx)->cmd = CD_COMMAND_EMPTY;
                    state->readIdx                         = state->readIdx + 1;
                    state->readIdx                         = state->readIdx % ARRAY_SIZE(state->entries);
                }
                return;
            }
            if (status != 0x80) {
                return;
            }
            switch (cdSyncPollCommand(0, 0)) {
                case CD_SYNC_RETRY:
                    CdFlush();
                    /* fallthrough */
                case CD_SYNC_COMPLETE:
                    if (CdSync(1, NULL) == CdlDiskError) {
                        cdSyncWaitForReadableDisc(1);
                    }
                    fsStartStage0HeaderRead();
                    return;
                case CD_SYNC_PENDING:
                default:
                    return;
            }
    }
}

/// Saves the ring head for cancellation or suspension.
///
/// `headCommand` is the opcode captured at the start of the caller's transition.
/// The other seven bytes come from the current head; its index and contents must
/// stay valid throughout the copy. The saved resume sector and phase stay intact.
static inline void _cdCmdSnapshotHeadRequest(CdCmdQueue* queue, u8 headCommand)
{
    queue->activeRequest.entry.cmd           = headCommand;
    queue->activeRequest.entry.stage         = queue->entries[queue->readIdx].stage;
    queue->activeRequest.entry.fileGroup     = queue->entries[queue->readIdx].fileGroup;
    queue->activeRequest.entry.fileIndex     = queue->entries[queue->readIdx].fileIndex;
    queue->activeRequest.entry.args.bytes[0] = queue->entries[queue->readIdx].args.bytes[0];
    queue->activeRequest.entry.args.bytes[1] = queue->entries[queue->readIdx].args.bytes[1];
    queue->activeRequest.entry.args.bytes[2] = queue->entries[queue->readIdx].args.bytes[2];
    queue->activeRequest.entry.args.bytes[3] = queue->entries[queue->readIdx].args.bytes[3];
}

s32 cdCmdRequestCancel(void)
{
    CdCmdQueue* queue;
    u8          command;

    queue   = &gCdCmdQueue;
    command = queue->entries[queue->readIdx].cmd;
    if (command != CD_COMMAND_EMPTY) {
        _cdCmdSnapshotHeadRequest(queue, command);
        queue->activeRequest.phase = CD_COMMAND_PHASE_CANCEL;
        return 1;
    }
    if ((u16)queue->sceneAudioMode != CD_COMMAND_SCENE_INACTIVE) {
        queue->activeRequest.phase = CD_COMMAND_PHASE_CANCEL;
        return 1;
    }
    return 0;
}

static inline void _cdCmdFinishSceneAudio(void)
{
    CdCmdQueue* p;

    p = &gCdCmdQueue;
    memFillBytes(&p->activeRequest, 0, sizeof(p->activeRequest));
    p->replacementEntry.cmd = CD_COMMAND_EMPTY;
    p->blockGamePause       = 0;
    p->sceneAudioMode       = CD_COMMAND_SCENE_INACTIVE;
    cdCmdResumeSceneSoundRequests(p->sceneStream->data.scene.soundBankMask);
    streamFinishScene();
    if (p->busy != 0) {
        p->busy              = 0;
        gDisplayState.cdBusy = DISPLAY_CD_IDLE;
    }
    p->step               = 0;
    p->cancelStep         = CD_COMMAND_CANCEL_BEGIN;
    p->pausePlayClock     = 0;
    p->cdOperationPending = 0;
    if (p->readIdx != p->writeIdx) {
        p->entries[p->readIdx].cmd = CD_COMMAND_EMPTY;
        p->readIdx                 = p->readIdx + 1;
        p->readIdx                 = p->readIdx % ARRAY_SIZE(p->entries);
    }
}

static void CdCmd_ProcessPhase1(void)
{
    CdCmdQueue* p;
    CdCmdQueue* q;
    u16*        statePtr;
    u16         ret;
    s32         temp;
    StreamSlot* sceneStream;

    p = &gCdCmdQueue;
    switch (p->activeRequest.entry.cmd >> 4) {
        case 0:
        case 1:
        case 2:
            p->activeRequest.phase = CD_COMMAND_PHASE_DISPATCH;
            return;
        case 3:
        case 4:
        case 6:
        case 7:
            if (p->cdOperationPending != 0) {
                if ((p->activeRequest.entry.cmd >> 4) == 7) {
                    func_acropolis_plaza_8017D6D4();
                    return;
                }
                CdCmd_HandleStreamDecode();
                return;
            }
            statePtr = &p->cancelStep;
            switch (*statePtr) {
                case CD_COMMAND_CANCEL_BEGIN:
                    if (D_8006AC58 != 0) {
                        CdVol_CacheFromSpu();
                        p->cancelStep = p->cancelStep + 1;
                    } else {
                        p->cancelStep = CD_COMMAND_CANCEL_FINISH;
                        goto case_2;
                    }
                    /* fallthrough */
                case CD_COMMAND_CANCEL_WAIT:
                    if (CdVol_StepDown() == 0) {
                        *statePtr = *statePtr + 1;
                    }
                    CdCmd_HandleStreamDecode();
                    ret = 0;
                    break;
                case CD_COMMAND_CANCEL_FINISH:
                case_2:
                    if ((s16)CdCmd_StopMdec(1)) {
                        ret = 1;
                    } else {
                        CdCmd_HandleStreamDecode();
                        ret = 0;
                    }
                    break;
                default:
                    ret = 0;
                    break;
            }
            if (ret != 0) {
                p->movieFrameAvailable = 0;
                memFillBytes(&p->activeRequest, 0, sizeof(p->activeRequest));
                q = &gCdCmdQueue;
                if (q->busy != 0) {
                    q->busy              = 0;
                    gDisplayState.cdBusy = DISPLAY_CD_IDLE;
                }
                q->step               = 0;
                q->cancelStep         = CD_COMMAND_CANCEL_BEGIN;
                q->pausePlayClock     = 0;
                q->cdOperationPending = 0;
                if (q->readIdx != q->writeIdx) {
                    q->entries[q->readIdx].cmd = CD_COMMAND_EMPTY;
                    q->readIdx                 = q->readIdx + 1;
                    q->readIdx                 = q->readIdx % ARRAY_SIZE(q->entries);
                }
            }
            return;
        case 8:
            if (p->cdOperationPending != 0) {
                Gp_StepCdAudioCmd();
                return;
            }
            if ((u16)p->sceneAudioMode != CD_COMMAND_SCENE_INACTIVE) {
                switch (p->cancelStep) {
                    case CD_COMMAND_CANCEL_BEGIN:
                        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0) {
                            sndEvtRequestScriptStart(SOUND_SCRIPT_REQUEST_NO_OP, 0, 0);
                        }
                        CdAudio_Begin();
                        p->cancelStep = p->cancelStep + 1;
                        return;
                    case CD_COMMAND_CANCEL_WAIT:
                        if (CdAudio_Phase.stopStep == CD_AUDIO_STOP_STEP_DONE) {
                            sceneStream = p->sceneStream;
                            temp        = sceneStream->data.scene.resumeSectorOffset;
                            if (temp) {
                                CdAudio_JumpToSector(sceneStream->startSector + temp);
                                p->cancelStep = p->cancelStep + 1;
                                return;
                            }
                            _cdCmdFinishSceneAudio();
                            return;
                        }
                        return;
                    case CD_COMMAND_CANCEL_FINISH:
                        if (CdAudio_Phase.waveLoadStep == CD_AUDIO_WAVE_LOAD_STEP_DONE) {
                            _cdCmdFinishSceneAudio();
                            return;
                        }
                        return;
                    default:
                        return;
                }
            } else {
                _cdCmdFinishSceneAudio();
            }
            return;
        case 5:
        default:
            return;
    }
}

u16 cdCmdRequestSuspend(void)
{
    enum { CD_COMMAND_SCENE_AUDIO_FAMILY = CD_COMMAND_PLAY_SCENE_AUDIO >> 4 };
    CdCmdQueue* queue;
    u8          command;

    queue = &gCdCmdQueue;
    if (queue->activeRequest.phase != CD_COMMAND_PHASE_DISPATCH) {
        return 1;
    }
    command = queue->entries[queue->readIdx].cmd;
    if ((command >> 4) == CD_COMMAND_SCENE_AUDIO_FAMILY || command == CD_COMMAND_EMPTY) {
        return 0;
    }
    _cdCmdSnapshotHeadRequest(queue, command);
    queue->activeRequest.phase = CD_COMMAND_PHASE_SUSPEND;
    return 1;
}

static void CdCmd_ProcessPhase2(void)
{
    CdCmdQueue* p;
    CdCmdQueue* p2;
    u16*        statePtr;
    u16         ret;
    u8          result[8];
    CdlLOC      loc;
    s32         pos;

    p = &gCdCmdQueue;
    switch (p->activeRequest.entry.cmd >> 4) {
        case 0:
        case 1:
        case 2:
            p->activeRequest.phase = CD_COMMAND_PHASE_DISPATCH;
            p->sceneAudioStarted   = 0;
            break;
        case 4:
        case 6:
        case 7:
            if (p->cdOperationPending != 0) {
                if ((p->activeRequest.entry.cmd >> 4) == 7) {
                    func_acropolis_plaza_8017D6D4();
                } else {
                    CdCmd_HandleStreamDecode();
                }
                break;
            }
            statePtr = &p->suspendResumeStep;
            switch (*statePtr) {
                case 0:
                    if (D_8006AC58 != 0) {
                        CdVol_CacheFromSpu();
                        p->suspendResumeStep = p->suspendResumeStep + 1;
                    } else {
                        p->suspendResumeStep = 2;
                        goto case_2;
                    }
                    /* fallthrough */
                case 1:
                    if (CdVol_StepDown() == 0) {
                        *statePtr = *statePtr + 1;
                    }
                    CdCmd_HandleStreamDecode();
                    ret = 0;
                    break;
                case 2:
                case_2:
                    if ((s16)CdCmd_StopMdec(1)) {
                        ret = 1;
                    } else {
                        CdCmd_HandleStreamDecode();
                        ret = 0;
                    }
                    break;
                default:
                    ret = 0;
                    break;
            }
            if (ret != 0) {
                CdControlB(CdlGetlocL, NULL, result);
                loc.minute                    = result[0];
                loc.second                    = result[1];
                loc.sector                    = result[2];
                loc.track                     = 0;
                pos                           = CdPosToInt(&loc);
                p2                            = &gCdCmdQueue;
                p->activeRequest.resumeSector = pos;
                p->activeRequest.phase        = CD_COMMAND_PHASE_DISPATCH;
                p->suspendResumeStep          = 0;
                if (p2->busy != 0) {
                    p2->busy             = 0;
                    gDisplayState.cdBusy = DISPLAY_CD_IDLE;
                }
                memFillBytes(p2->entries, 0, sizeof(p2->entries));
                p2->writeIdx          = 0;
                p2->readIdx           = 0;
                p2->step              = 0;
                p2->cancelStep        = CD_COMMAND_CANCEL_BEGIN;
                p2->suspendResumeStep = 0;
            }
            break;
        case 3:
        case 5:
        case 8:
            break;
    }
}

/* Alignment pad after the 9-entry CdCmd_ProcessPhase2 jump table. */

/// Stores the low command byte, key bytes 3/2/0 and all four argument bytes.
///
/// Uses unsigned byte views in that order, with no alignment requirement or
/// retained source pointers. Both source addresses must remain readable through
/// their highest accessed byte, including opcode-unused bytes. A zero address
/// is read from low RAM, as required by the resident raw-byte API.
static inline void _cdCmdStoreRequest(CdCmdEntry* entry, s32 command, const void* fileKey, const void* commandArgs)
{
    const u8* fileKeyBytes;
    const u8* argumentBytes;

    fileKeyBytes         = fileKey;
    argumentBytes        = commandArgs;
    entry->cmd           = command;
    entry->stage         = fileKeyBytes[3];
    entry->fileGroup     = fileKeyBytes[2];
    entry->fileIndex     = fileKeyBytes[0];
    entry->args.bytes[0] = argumentBytes[0];
    entry->args.bytes[1] = argumentBytes[1];
    entry->args.bytes[2] = argumentBytes[2];
    entry->args.bytes[3] = argumentBytes[3];
}

/// Appends a request under `cdCmdEnqueue`'s byte-source and ring-capacity contract.
static inline s32 _cdCmdEnqueue(s32 command, const void* fileKey, const void* commandArgs)
{
    CdCmdQueue* queue;
    CdCmdEntry* entry;
    u16         writtenSlot;
    u16         nextWriteSlot;

    queue = &gCdCmdQueue;
    entry = &queue->entries[queue->writeIdx];
    _cdCmdStoreRequest(entry, command, fileKey, commandArgs);
    writtenSlot     = queue->writeIdx;
    nextWriteSlot   = writtenSlot + 1;
    queue->writeIdx = nextWriteSlot;
    nextWriteSlot   = queue->writeIdx % ARRAY_SIZE(queue->entries);
    queue->writeIdx = nextWriteSlot;
    return writtenSlot;
}

/// Returns 1 when normal dispatch is selected and the request ring is empty.
static inline u16 _cdCmdIsIdle(void)
{
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if (queue->activeRequest.phase != CD_COMMAND_PHASE_DISPATCH) {
        return 0;
    }
    if (queue->writeIdx != queue->readIdx) {
        return 0;
    }
    return 1;
}

u16 cdCmdResumeSuspendedMovie(void)
{
    enum {
        CD_COMMAND_SCENE_AUDIO_FAMILY  = CD_COMMAND_PLAY_SCENE_AUDIO >> 4,
        CD_COMMAND_MOVIE_FAMILY        = CD_COMMAND_CONTINUE_STREAM >> 4,
        CD_COMMAND_OFFSET_MOVIE_FAMILY = CD_COMMAND_RESUME_STREAM_AT_POSITION >> 4,
        CD_COMMAND_RESUME_ENQUEUE      = 0,
        CD_COMMAND_RESUME_WAIT         = 1,
    };
    CdCmdQueue* queue;
    u8          fileKeyBytes[4];

    queue = &gCdCmdQueue;
    switch (queue->activeRequest.entry.cmd >> 4) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            break;
        case CD_COMMAND_MOVIE_FAMILY:
        case CD_COMMAND_OFFSET_MOVIE_FAMILY:
            switch (queue->suspendResumeStep) {
                case CD_COMMAND_RESUME_ENQUEUE:
                    // Requeue the saved movie before waiting for its first frame.
                    fileKeyBytes[3] = queue->activeRequest.entry.stage;
                    fileKeyBytes[2] = queue->activeRequest.entry.fileGroup;
                    fileKeyBytes[0] = queue->activeRequest.entry.fileIndex;
                    if ((queue->activeRequest.entry.cmd >> 4) == CD_COMMAND_MOVIE_FAMILY) {
                        _cdCmdEnqueue(CD_COMMAND_CONTINUE_STREAM, fileKeyBytes, queue->activeRequest.entry.args.bytes);
                    } else if ((queue->activeRequest.entry.cmd >> 4) == CD_COMMAND_OFFSET_MOVIE_FAMILY) {
                        _cdCmdEnqueue(CD_COMMAND_RESUME_STREAM_AT_POSITION, fileKeyBytes, queue->activeRequest.entry.args.bytes);
                    }
                    queue->suspendResumeStep++;
                    return 0;
                case CD_COMMAND_RESUME_WAIT:
                    // A skipped movie can retire without ever publishing a frame.
                    if (queue->movieReady != 0 || _cdCmdIsIdle()) {
                        memFillBytes(&queue->activeRequest, 0, sizeof(queue->activeRequest));
                        queue->suspendResumeStep = CD_COMMAND_RESUME_ENQUEUE;
                        break;
                    }
                    return 0;
            }
            break;
        case CD_COMMAND_SCENE_AUDIO_FAMILY:
            break;
    }
    return 1;
}

s32 cdCmdEnqueue(s32 command, const void* fileKey, const void* commandArgs)
{
    return _cdCmdEnqueue(command, fileKey, commandArgs);
}

u16 cdCmdIsIdle(void)
{
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if (queue->activeRequest.phase != CD_COMMAND_PHASE_DISPATCH) {
        return 0;
    }
    return queue->writeIdx == queue->readIdx;
}

u16 cdCmdIsSlotEmpty(s16 slot)
{
    return gCdCmdQueue.entries[slot].cmd == CD_COMMAND_EMPTY;
}

void cdCmdPrepareViewMovie(void)
{
    gCdCmdQueue.movieFrame = 1;
    if (streamHasLoadedViewMovie(&gGameSession->location.loc.view) != 0) {
        DecDCTvlcBuild(Fs_ActorLoadBase0);
        gGameSession->field_7C = 0;
    }
}

void cdCmdResetState(void)
{
    memFillBytes(&gCdCmdQueue, 0, sizeof(gCdCmdQueue));
}

s32 cdCmdDropQueuedTail(void)
{
    CdCmdQueue* queue;
    u16         slot;
    u16         tailSlot;

    queue    = &gCdCmdQueue;
    tailSlot = queue->writeIdx;
    if (tailSlot == queue->readIdx) {
        return 1;
    }

    // Preserve the head; discard only requests queued behind it.
    slot = queue->readIdx + 1;
    slot = slot % ARRAY_SIZE(queue->entries);
    if (slot != tailSlot) {
        do {
            queue->entries[slot].cmd = CD_COMMAND_EMPTY;
            slot                     = slot + 1;
            slot                     = slot % ARRAY_SIZE(queue->entries);
        } while (slot != queue->writeIdx);
    }

    queue->writeIdx = queue->readIdx + 1;
    queue->writeIdx = queue->writeIdx % ARRAY_SIZE(queue->entries);
    return 0;
}

void cdCmdSelectMovieWorkspace(void)
{
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if (streamFindMovieSlot(&gGameSession->location.loc, 0, 0) >= 0) {
        D_8006AC40 = D_8006AC00;
    }
    queue->movieFrameAvailable = 0;
}

/// Returns 2 for a selected scene slot, otherwise -1 or 3 from the fallback sign.
///
/// A negative `field_236` selects -1; nonnegative selects 3. The fallback field's
/// role and the meaning of status 3 are unproven; this entry point has no callers.
static s32 _cdCmdGetSceneSelectionStatus(void)
{
    enum {
        CD_COMMAND_SCENE_SELECTION_FAILED   = -1,
        CD_COMMAND_SCENE_SELECTION_SELECTED = 2,
        CD_COMMAND_SCENE_SELECTION_FALLBACK = 3,
    };
    CdCmdQueue* queue;
    s32         fallbackStatus;

    queue = &gCdCmdQueue;
    if (queue->sceneSlotIndex > CD_COMMAND_NO_SCENE_SLOT) {
        return CD_COMMAND_SCENE_SELECTION_SELECTED;
    }
    if (queue->field_236 < 0) {
        return CD_COMMAND_SCENE_SELECTION_FAILED;
    }
    fallbackStatus = CD_COMMAND_SCENE_SELECTION_FALLBACK;
    return fallbackStatus;
}

/// Returns the scene/audio mode (0 inactive, 1 scene playback, 2 starting audio).
static s16 _cdCmdGetSceneAudioMode(void)
{
    return gCdCmdQueue.sceneAudioMode;
}

void cdCmdSelectScene(u16 group, u16 streamId, u16 subId)
{
    CdCmdQueue* queue;

    queue                 = &gCdCmdQueue;
    queue->field_1FF      = 1;
    queue->field_236      = -1;
    queue->sceneSlotIndex = streamSelectScene(group, streamId, subId, 0);
}

void cdCmdSceneControlNoOp(void)
{
}

void cdCmdCancelScene(void)
{
    gCdCmdQueue.replacementEntry.cmd = CD_COMMAND_EMPTY;
    cdCmdRequestCancel();
    streamFinishScene();
}

/// Empty resident entry point with no callers; its intended role is unproven.
static void CdCmd_UnusedStub1(void)
{
}

/// Empty resident entry point with no callers; its intended role is unproven.
static void CdCmd_UnusedStub2(void)
{
}

void cdCmdSceneViewChangeNoOp(void)
{
}

void cdCmdEnqueueScenePlayback(void)
{
    CdCmdQueue* queue;
    u8          commandArgs[sizeof(queue->entries[0].args.bytes)];

    queue = &gCdCmdQueue;
    if (queue->sceneSlotIndex > CD_COMMAND_NO_SCENE_SLOT) {
        // The opcode ignores the three unwritten argument bytes and low-RAM key bytes.
        commandArgs[0]        = queue->sceneSlotIndex;
        queue->sceneAudioMode = CD_COMMAND_SCENE_STARTING_AUDIO;
        cdCmdEnqueue(CD_COMMAND_PLAY_SCENE_AUDIO, 0, commandArgs);
    } else {
        queue->sceneAudioMode = CD_COMMAND_SCENE_PLAYING;
    }
}

/// Stages a deferred playback request for the selected scene/audio session.
///
/// No selected slot leaves the previous replacement intact. Selection and
/// playback storage must survive the later commit and playback. The scene/audio
/// mode is unchanged; the playback handler changes it when the request runs.
static void _cdCmdStageScenePlayback(void)
{
    CdCmdQueue* queue;
    u8          commandArgs[sizeof(queue->entries[0].args.bytes)];

    queue = &gCdCmdQueue;
    if (queue->sceneSlotIndex > CD_COMMAND_NO_SCENE_SLOT) {
        // Preserve the unused argument bytes and the raw low-RAM key reads.
        commandArgs[0] = queue->sceneSlotIndex;
        cdCmdStageReplacement(CD_COMMAND_PLAY_SCENE_AUDIO, 0, commandArgs);
    }
}

void cdCmdEnqueueSceneAudioStart(void)
{
    CdCmdQueue* queue;
    u8          commandArgs[sizeof(queue->entries[0].args.bytes)];

    queue = &gCdCmdQueue;
    if (queue->sceneSlotIndex > CD_COMMAND_NO_SCENE_SLOT) {
        // Preserve the unused argument bytes and the raw low-RAM key reads.
        commandArgs[0]        = queue->sceneSlotIndex;
        queue->sceneAudioMode = CD_COMMAND_SCENE_STARTING_AUDIO;
        cdCmdEnqueue(CD_COMMAND_START_SCENE_AUDIO, 0, commandArgs);
    }
}

void cdCmdStageSceneAudioStart(void)
{
    CdCmdQueue* queue;
    u8          commandArgs[sizeof(queue->entries[0].args.bytes)];

    queue = &gCdCmdQueue;
    if (queue->sceneSlotIndex > CD_COMMAND_NO_SCENE_SLOT) {
        // The opcode ignores the three unwritten argument bytes and low-RAM key bytes.
        commandArgs[0] = queue->sceneSlotIndex;
        cdCmdStageReplacement(CD_COMMAND_START_SCENE_AUDIO, 0, commandArgs);
    }
}

void cdCmdStageReplacement(s32 command, const void* fileKey, const void* commandArgs)
{
    CdCmdQueue* queue;
    CdCmdEntry* entry;

    queue = &gCdCmdQueue;
    entry = &queue->replacementEntry;
    _cdCmdStoreRequest(entry, command, fileKey, commandArgs);
}

s16 cdCmdCommitReplacement(void)
{
    enum { CD_COMMAND_NO_REPLACEMENT = -1 };
    CdCmdQueue* queue;
    CdCmdEntry* entry;
    u16         writtenSlot;
    u16         nextWriteSlot;
    u8          fileKeyBytes[4];

    queue = &gCdCmdQueue;
    if (*(volatile u8*)&queue->replacementEntry.cmd == CD_COMMAND_EMPTY) {
        return CD_COMMAND_NO_REPLACEMENT;
    }

    // Snapshot the file key before packing all eight request bytes into the ring.
    fileKeyBytes[3] = queue->replacementEntry.stage;
    fileKeyBytes[2] = queue->replacementEntry.fileGroup;
    fileKeyBytes[0] = queue->replacementEntry.fileIndex;

    writtenSlot = queue->writeIdx;
    entry       = &queue->entries[writtenSlot];
    _cdCmdStoreRequest(entry, queue->replacementEntry.cmd, fileKeyBytes, queue->replacementEntry.args.bytes);

    // Retire the replacement before publishing the next ring position.
    queue->replacementEntry.cmd = CD_COMMAND_EMPTY;

    writtenSlot     = queue->writeIdx;
    nextWriteSlot   = writtenSlot + 1;
    queue->writeIdx = nextWriteSlot;
    nextWriteSlot   = queue->writeIdx % ARRAY_SIZE(queue->entries);
    queue->writeIdx = nextWriteSlot;
    return writtenSlot;
}

u16 cdCmdIsIdleOrSceneAudioPending(void)
{
    enum { CD_COMMAND_SCENE_AUDIO_FAMILY = CD_COMMAND_PLAY_SCENE_AUDIO >> 4 };
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if (_cdCmdIsIdle() == 0) {
        if ((queue->entries[queue->readIdx].cmd >> 4) != CD_COMMAND_SCENE_AUDIO_FAMILY) {
            return 0;
        }
    }
    return 1;
}

CdCmdEntry* cdCmdNextQueuedEntry(void)
{
    CdCmdQueue* queue;
    s32         slot;
    CdCmdEntry* entry;

    queue = &gCdCmdQueue;
    slot  = CdCmd_EntryIter;
    entry = &queue->entries[slot];
    if (slot == queue->writeIdx) {
        return NULL;
    }
    CdCmd_EntryIter = slot + 1;
    CdCmd_EntryIter = CdCmd_EntryIter % ARRAY_SIZE(queue->entries);
    return entry;
}

void cdCmdSetBusy(void)
{
    if (gCdCmdQueue.busy == 0) {
        gCdCmdQueue.busy     = 1;
        gDisplayState.cdBusy = DISPLAY_CD_BUSY;
    }
}

void cdCmdClearBusy(void)
{
    if (gCdCmdQueue.busy != 0) {
        gCdCmdQueue.busy     = 0;
        gDisplayState.cdBusy = DISPLAY_CD_IDLE;
    }
}

/// Clears every queued request byte and resets ring and handler progress.
///
/// Retains the active snapshot, replacement, dispatch phase and busy latches.
/// Drive operations and playback storage must be quiescent before this reset.
static void _cdCmdResetRing(void)
{
    enum {
        CD_COMMAND_HANDLER_BEGIN        = 0,
        CD_COMMAND_SUSPEND_RESUME_BEGIN = 0,
    };
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    memFillBytes(queue->entries, 0, sizeof(queue->entries));
    queue->writeIdx          = 0;
    queue->readIdx           = 0;
    queue->step              = CD_COMMAND_HANDLER_BEGIN;
    queue->cancelStep        = CD_COMMAND_CANCEL_BEGIN;
    queue->suspendResumeStep = CD_COMMAND_SUSPEND_RESUME_BEGIN;
}

void cdCmdResetEntryIterator(void)
{
    CdCmd_EntryIter = gCdCmdQueue.readIdx;
}

void cdCmdEnqueueUnlessSceneAudioPending(s32 command, const void* fileKey, const void* commandArgs)
{
    enum { CD_COMMAND_SCENE_AUDIO_FAMILY = CD_COMMAND_PLAY_SCENE_AUDIO >> 4 };
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if ((queue->entries[queue->readIdx].cmd >> 4) != CD_COMMAND_SCENE_AUDIO_FAMILY) {
        _cdCmdEnqueue(command, fileKey, commandArgs);
    }
}

void cdCmdCompleteHeadRequest(void)
{
    enum { CD_COMMAND_HANDLER_BEGIN = 0 };
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if (queue->busy != 0) {
        queue->busy          = 0;
        gDisplayState.cdBusy = DISPLAY_CD_IDLE;
    }
    // Retire handler progress before releasing the head slot.
    queue->step               = CD_COMMAND_HANDLER_BEGIN;
    queue->cancelStep         = CD_COMMAND_CANCEL_BEGIN;
    queue->pausePlayClock     = 0;
    queue->cdOperationPending = 0;
    if (queue->readIdx != queue->writeIdx) {
        queue->entries[queue->readIdx].cmd = CD_COMMAND_EMPTY;
        queue->readIdx                     = queue->readIdx + 1;
        queue->readIdx                     = queue->readIdx % ARRAY_SIZE(queue->entries);
    }
}

void cdCmdSaveHeadRequest(void)
{
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    // Save only the request; its resume sector and dispatch phase stay intact.
    queue->activeRequest.entry.cmd           = queue->entries[queue->readIdx].cmd;
    queue->activeRequest.entry.stage         = queue->entries[queue->readIdx].stage;
    queue->activeRequest.entry.fileGroup     = queue->entries[queue->readIdx].fileGroup;
    queue->activeRequest.entry.fileIndex     = queue->entries[queue->readIdx].fileIndex;
    queue->activeRequest.entry.args.bytes[0] = queue->entries[queue->readIdx].args.bytes[0];
    queue->activeRequest.entry.args.bytes[1] = queue->entries[queue->readIdx].args.bytes[1];
    queue->activeRequest.entry.args.bytes[2] = queue->entries[queue->readIdx].args.bytes[2];
    queue->activeRequest.entry.args.bytes[3] = queue->entries[queue->readIdx].args.bytes[3];
}

void CdCmd_Dispatch(void)
{
    CdCmdQueue* state; // The indirection is required.

    state = &gCdCmdQueue;
    switch (state->activeRequest.phase) {
        case CD_COMMAND_PHASE_DISPATCH:
            if (state->suspendNormalDispatch == 0) {
                switch (state->entries[state->readIdx].cmd >> 4) {
                    case CD_COMMAND_PHASE_DISPATCH:
                        break;
                    case CD_COMMAND_PHASE_SUSPEND:
                        CdCmd_HandleFileLoad();
                        break;
                    case 6:
                        CdCmd_HandleStreamDecode();
                        break;
                    case 7:
                        func_acropolis_plaza_8017D6D4();
                        break;
                    case 5:
                        CdCmd_HandleMount();
                        break;
                    case 8:
                        Gp_StepCdAudioCmd();
                        break;
                }
            }
            break;
        case CD_COMMAND_PHASE_CANCEL:
            CdCmd_ProcessPhase1();
            break;
        case CD_COMMAND_PHASE_SUSPEND:
            CdCmd_ProcessPhase2();
            break;
    }

    if (state->bootLoadActive != 0) {
        Fs_StepBootImage();
    }
}
