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

static void _cdCmdHandleMoviePlayback(void);

static void CdCmd_HandleFileLoad(void);

static void _cdCmdHandleStageMount(void);

static void CdCmd_ProcessPhase1(void);

static void _cdCmdHandleRequestSuspension(void);

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

/// Resets request-handler progress and retires the head of a nonempty ring.
///
/// Requires borrowed queue storage with both ring indices in 0..7. An empty
/// ring still receives the progress resets. The caller releases busy ownership
/// first; the saved request, replacement and playback storage remain intact.
static inline void _cdCmdRetireHeadRequest(CdCmdQueue* completionQueue)
{
    enum { CD_COMMAND_HANDLER_BEGIN = 0 };
    completionQueue->step               = CD_COMMAND_HANDLER_BEGIN;
    completionQueue->cancelStep         = CD_COMMAND_CANCEL_BEGIN;
    completionQueue->pausePlayClock     = 0;
    completionQueue->cdOperationPending = 0;
    if (completionQueue->readIdx != completionQueue->writeIdx) {
        completionQueue->entries[completionQueue->readIdx].cmd = CD_COMMAND_EMPTY;
        completionQueue->readIdx                               = completionQueue->readIdx + 1;
        completionQueue->readIdx                               = completionQueue->readIdx % ARRAY_SIZE(completionQueue->entries);
    }
}

/// Advances the head play/continue movie request through setup and playback.
///
/// Requires a valid movie slot (0..14), reserved workspaces and serialized CD
/// access. Fresh requests reset the two decoder buffer selectors; continue
/// requests retain them and become play requests. Setup failure publishes the
/// ready/frame-change latches and retires the head, then still polls playback.
/// Completion releases CD-busy ownership and handler progress without freeing
/// retained movie storage. Calls during cancellation keep playback progressing.
static void _cdCmdHandleMoviePlayback(void)
{
    enum {
        CD_COMMAND_MOVIE_HANDLER_BEGIN     = 0,
        CD_COMMAND_MOVIE_HANDLER_PLAY      = 1,
        CD_COMMAND_MOVIE_SECTORS_PER_FRAME = 10,
    };
    CdCmdQueue* queue;
    CdCmdQueue* completionQueue;
    CdCmdEntry* entry;
    s32         slotIndex;
    s32         command;
    s32         wasBusy;

    // Keep the unsigned slot-byte load separate from its signed interpretation.
    queue     = &gCdCmdQueue;
    slotIndex = *(volatile u8*)&queue->entries[queue->readIdx].args.stream.slotIndex;
    command   = queue->entries[queue->readIdx].cmd;
    slotIndex = (s8)slotIndex;
    if (command == CD_COMMAND_EMPTY) {
        return;
    }
    if (command < 0) {
        return;
    }
    if (command >= CD_COMMAND_CONTINUE_STREAM + 1) {
        return;
    }
    if (command < CD_COMMAND_PLAY_STREAM) {
        return;
    }
    switch (queue->step) {
        case CD_COMMAND_MOVIE_HANDLER_BEGIN:
            if (queue->busy == 0) {
                queue->busy          = 1;
                gDisplayState.cdBusy = DISPLAY_CD_BUSY;
            }
            switch (cdSyncPollCommand(0, 0)) {
                case CD_SYNC_PENDING:
                    return;
                case CD_SYNC_RETRY:
                    CdFlush();
                    /* fallthrough */
                case CD_SYNC_COMPLETE:
                    entry = &queue->entries[queue->readIdx];
                    if (entry->cmd == CD_COMMAND_PLAY_STREAM) {
                        D_8005EAEC = 0;
                        D_8005EAEE = 0;
                    } else if (entry->cmd == CD_COMMAND_CONTINUE_STREAM) {
                        entry->cmd = CD_COMMAND_PLAY_STREAM;
                    }
                    if ((s16)streamPrepareMoviePlayback(slotIndex & 0xFFFF) != 0) {
                        completionQueue          = &gCdCmdQueue;
                        wasBusy                  = completionQueue->busy;
                        queue->movieReady        = 1;
                        queue->movieFrameChanged = 1;
                        if (wasBusy != 0) {
                            completionQueue->busy = 0;
                            gDisplayState.cdBusy  = DISPLAY_CD_IDLE;
                        }
                        _cdCmdRetireHeadRequest(completionQueue);
                        break;
                    }
                    queue->continueMovie = 1;
                    streamPollMoviePlayback(0, ((u16)queue->movieFrame - 1) * CD_COMMAND_MOVIE_SECTORS_PER_FRAME);
                    queue->step = queue->step + 1;
                    break;
            }
            break;
        case CD_COMMAND_MOVIE_HANDLER_PLAY:
            break;
        default:
            return;
    }
    // Resume from the one-based published frame, at ten sectors per frame.
    if ((s16)streamPollMoviePlayback(0, ((u16)queue->movieFrame - 1) * CD_COMMAND_MOVIE_SECTORS_PER_FRAME) != 0) {
        completionQueue = &gCdCmdQueue;
        if (completionQueue->busy != 0) {
            completionQueue->busy = 0;
            gDisplayState.cdBusy  = DISPLAY_CD_IDLE;
        }
        _cdCmdRetireHeadRequest(completionQueue);
    }
}

static void CdCmd_HandleFileLoad(void)
{
    CdCmdQueue*   state;
    CdCmdQueue*   p;
    s32           status;
    FsFileLoadKey fileKey;
    u8            mode;

    // The file key's hundreds/folder-suffix byte is stored with the load options.
    state                  = &gCdCmdQueue;
    fileKey.stage          = state->entries[state->readIdx].stage;
    fileKey.fileGroup      = state->entries[state->readIdx].fileGroup;
    fileKey.fileIndex      = state->entries[state->readIdx].fileIndex;
    fileKey.fileIdHundreds = state->entries[state->readIdx].args.file.fileIdHundreds;

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
            if (state->entries[state->readIdx].args.file.loadMode != CD_COMMAND_LOAD_DEFAULT || fileKey.stage == 0 || fileKey.fileIndex != 0) {
                state->step = 4;
                goto do_load;
            }
            state->step = state->step + 1;
            /* fallthrough */
        case 1:
            fsStartFolderDirectoryRead(fileKey.stage, fileKey.fileGroup, fileKey.fileIdHundreds);
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
                            fsResumeRequestedRead();
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
                    fsBuildFolderTables(fileKey.stage, fileKey.fileGroup, fileKey.fileIdHundreds);
                    state->step = state->step + 1;
                    break;
            }
            /* fallthrough */
        case 4:
        do_load:
            fsLoadFile(
                &fileKey,
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
                            fsResumeRequestedRead();
                            break;
                        case CD_SYNC_RETRY:
                            CdFlush();
                            fsResumeRequestedRead();
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

/// Advances a stage folder-list mount or polls the initial STAGE0.HED read.
///
/// A mount's stage byte must select a CDF on the current disc in 1..5. Reads use the shared
/// sector buffer, so drive access must remain serialized until the folder table
/// is built. The HED read is already started by ISO scanning; this handler only
/// restarts or completes it. Completion releases busy ownership and the head.
static void _cdCmdHandleStageMount(void)
{
    enum {
        CD_COMMAND_STAGE_MOUNT_READ    = 0,
        CD_COMMAND_STAGE_MOUNT_WAIT    = 1,
        CD_COMMAND_STAGE_MOUNT_BUILD   = 2,
        CD_COMMAND_STAGE_READ_RESUME   = 0x40,
        CD_COMMAND_STAGE_READ_RESTART  = 0x80,
        CD_COMMAND_STAGE_READ_COMPLETE = 0xFF,
    };
    CdCmdQueue* queue;
    s32         command;
    s32         readStatus;

    queue   = &gCdCmdQueue;
    command = queue->entries[queue->readIdx].cmd;
    if (command < CD_COMMAND_MOUNT_STAGE) {
        return;
    }
    switch (command) {
        case CD_COMMAND_MOUNT_STAGE: {
            CdCmdEntry* entry;
            s32         stageIndex;
            s32         mountStep;

            // Retain the unsigned byte load before the signed stage interpretation.
            entry      = &queue->entries[queue->readIdx];
            readStatus = *(volatile u8*)&entry->stage;
            stageIndex = readStatus;
            mountStep  = queue->step;
            stageIndex = (s8)stageIndex;
            switch (mountStep) {
                case CD_COMMAND_STAGE_MOUNT_READ:
                    if (queue->busy == 0) {
                        queue->busy          = 1;
                        gDisplayState.cdBusy = DISPLAY_CD_BUSY;
                    }
                    fsStartStageFolderListRead(stageIndex & 0xFF);
                    queue->step = queue->step + 1;
                    return;
                case CD_COMMAND_STAGE_MOUNT_WAIT:
                    if (CdSync(1, NULL) == CdlDiskError) {
                        CdSyncCallback(NULL);
                        CdReadyCallback(NULL);
                        cdSyncWaitForReadableDisc(1);
                        queue->step = CD_COMMAND_STAGE_MOUNT_READ;
                        return;
                    }
                    fsAbortTimedOutOperation();
                    readStatus = Fs_CdOpStatus;
                    switch (readStatus) {
                        case CD_COMMAND_STAGE_READ_RESTART:
                            switch (cdSyncPollCommand(0, 0)) {
                                case CD_SYNC_RETRY:
                                    CdFlush();
                                    /* fallthrough */
                                case CD_SYNC_COMPLETE:
                                    if (CdSync(1, NULL) == CdlDiskError) {
                                        cdSyncWaitForReadableDisc(1);
                                    }
                                    queue->step = CD_COMMAND_STAGE_MOUNT_READ;
                                    return;
                                case CD_SYNC_PENDING:
                                default:
                                    return;
                            }
                        case CD_COMMAND_STAGE_READ_COMPLETE:
                            CdSyncCallback(NULL);
                            CdReadyCallback(NULL);
                            queue->step = queue->step + 1;
                            return;
                        // The writers of these two additional resumption statuses are unproven.
                        case 0x10:
                        case 0x20:
                        case CD_COMMAND_STAGE_READ_RESUME:
                            switch (cdSyncPollCommand(0, 0)) {
                                case CD_SYNC_RETRY:
                                    CdFlush();
                                    /* fallthrough */
                                case CD_SYNC_COMPLETE:
                                    fsResumeRequestedRead();
                                    return;
                                case CD_SYNC_PENDING:
                                default:
                                    return;
                            }
                    }
                    return;
                case CD_COMMAND_STAGE_MOUNT_BUILD:
                    // Build offsets only after the read callbacks and drive have stopped.
                    switch (cdSyncPollCommand(0, 0)) {
                        case CD_SYNC_RETRY:
                            CdFlush();
                            /* fallthrough */
                        case CD_SYNC_COMPLETE:
                            fsInitFolderTable(stageIndex & 0xFF);
                            if (queue->busy != 0) {
                                queue->busy          = 0;
                                gDisplayState.cdBusy = DISPLAY_CD_IDLE;
                            }
                            _cdCmdRetireHeadRequest(queue);
                            return;
                        case CD_SYNC_PENDING:
                        default:
                            return;
                    }
            }
            return;
        }
        case CD_COMMAND_READ_STAGE_HEADER:
            readStatus = Fs_CdOpStatus;
            if (readStatus == CD_COMMAND_STAGE_READ_COMPLETE) {
                if (queue->busy != 0) {
                    queue->busy          = 0;
                    gDisplayState.cdBusy = DISPLAY_CD_IDLE;
                }
                _cdCmdRetireHeadRequest(queue);
                return;
            }
            if (readStatus != CD_COMMAND_STAGE_READ_RESTART) {
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

/// Restores scene state and sound-request gates after scene-audio cancellation.
///
/// The selected scene descriptor and gameplay scene interface must remain live.
/// Audio stop and any trailing wave load must have completed before this call.
/// Clears the saved/replacement requests, releases the pause/busy gates and
/// retires the head if present; scene buffers remain allocated for reuse.
static inline void _cdCmdFinishSceneAudio(void)
{
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    memFillBytes(&queue->activeRequest, 0, sizeof(queue->activeRequest));
    queue->replacementEntry.cmd = CD_COMMAND_EMPTY;
    queue->blockGamePause       = 0;
    queue->sceneAudioMode       = CD_COMMAND_SCENE_INACTIVE;
    // Restore sound-request gates and scene state before publishing queue completion.
    cdCmdResumeSceneSoundRequests(queue->sceneStream->data.scene.soundBankMask);
    streamFinishScene();
    if (queue->busy != 0) {
        queue->busy          = 0;
        gDisplayState.cdBusy = DISPLAY_CD_IDLE;
    }
    _cdCmdRetireHeadRequest(queue);
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
                    acropolisPlazaPollStreamCommands();
                    return;
                }
                _cdCmdHandleMoviePlayback();
                return;
            }
            statePtr = &p->cancelStep;
            switch (*statePtr) {
                case CD_COMMAND_CANCEL_BEGIN:
                    if (D_8006AC58 != 0) {
                        cdVolBeginFadeOut();
                        p->cancelStep = p->cancelStep + 1;
                    } else {
                        p->cancelStep = CD_COMMAND_CANCEL_FINISH;
                        goto case_2;
                    }
                    /* fallthrough */
                case CD_COMMAND_CANCEL_WAIT:
                    if (cdVolStepFadeOut() == 0) {
                        *statePtr = *statePtr + 1;
                    }
                    _cdCmdHandleMoviePlayback();
                    ret = 0;
                    break;
                case CD_COMMAND_CANCEL_FINISH:
                case_2:
                    if (streamPollMovieStop(1)) {
                        ret = 1;
                    } else {
                        _cdCmdHandleMoviePlayback();
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
                        cdAudioCancel();
                        p->cancelStep = p->cancelStep + 1;
                        return;
                    case CD_COMMAND_CANCEL_WAIT:
                        if (CdAudio_Phase.stopStep == CD_AUDIO_STOP_STEP_DONE) {
                            sceneStream = p->sceneStream;
                            temp        = sceneStream->data.scene.resumeSectorOffset;
                            if (temp) {
                                cdAudioLoadWaves(sceneStream->startSector + temp);
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

/// Advances suspension of the saved head request for an intervening load.
///
/// File requests return to dispatch; movie requests finish any pending drive
/// operation, fade CD audio when selected and stop playback before recording an
/// absolute resume sector. The saved request and movie buffers survive the ring
/// clear. Unsupported command families retain suspension without progressing.
static void _cdCmdHandleRequestSuspension(void)
{
    enum {
        CD_COMMAND_SUSPEND_BEGIN       = 0,
        CD_COMMAND_SUSPEND_FADE        = 1,
        CD_COMMAND_SUSPEND_STOP        = 2,
        CD_COMMAND_MOVIE_FAMILY        = CD_COMMAND_CONTINUE_STREAM >> 4,
        CD_COMMAND_OFFSET_MOVIE_FAMILY = CD_COMMAND_RESUME_STREAM_AT_POSITION >> 4,
        CD_COMMAND_SCENE_AUDIO_FAMILY  = CD_COMMAND_PLAY_SCENE_AUDIO >> 4,
        CD_COMMAND_HANDLER_BEGIN       = 0,
    };
    CdCmdQueue* queue;
    CdCmdQueue* resetQueue;
    u16*        suspendStep;
    u16         movieStopped;
    u8          positionResult[8];
    CdlLOC      resumeLocation;
    s32         absoluteResumeSector;

    /// Polls shutdown, progressing playback while the drive pause is pending.
    ///
    /// Requires serialized resident playback state. `stopped` must be a writable
    /// lvalue; it is evaluated once and assigned 0 or 1. Successful shutdown
    /// clears display-movie framebuffers and retains playback workspaces.
#define CD_COMMAND_POLL_SUSPENDED_MOVIE_STOP(stopped) \
    {                                                 \
        if (streamPollMovieStop(true)) {              \
            (stopped) = 1;                            \
        } else {                                      \
            _cdCmdHandleMoviePlayback();              \
            (stopped) = 0;                            \
        }                                             \
    }

    queue = &gCdCmdQueue;
    switch (queue->activeRequest.entry.cmd >> 4) {
        case 0:
        case 1:
        case 2:
            queue->activeRequest.phase = CD_COMMAND_PHASE_DISPATCH;
            queue->sceneAudioStarted   = 0;
            break;
        case 4:
        case CD_COMMAND_MOVIE_FAMILY:
        case CD_COMMAND_OFFSET_MOVIE_FAMILY:
            // Keep playback moving until its current seek/read/pause can be stopped.
            if (queue->cdOperationPending != 0) {
                if ((queue->activeRequest.entry.cmd >> 4) == CD_COMMAND_OFFSET_MOVIE_FAMILY) {
                    acropolisPlazaPollStreamCommands();
                } else {
                    _cdCmdHandleMoviePlayback();
                }
                break;
            }
            suspendStep = &queue->suspendResumeStep;
            switch (*suspendStep) {
                case CD_COMMAND_SUSPEND_BEGIN:
                    if (D_8006AC58 != 0) {
                        cdVolBeginFadeOut();
                        queue->suspendResumeStep = queue->suspendResumeStep + 1;
                    } else {
                        queue->suspendResumeStep = CD_COMMAND_SUSPEND_STOP;
                        goto stopMovie;
                    }
                    /* fallthrough */
                case CD_COMMAND_SUSPEND_FADE:
                    if (cdVolStepFadeOut() == 0) {
                        *suspendStep = *suspendStep + 1;
                    }
                    _cdCmdHandleMoviePlayback();
                    movieStopped = 0;
                    break;
                case CD_COMMAND_SUSPEND_STOP:
                stopMovie:
                    CD_COMMAND_POLL_SUSPENDED_MOVIE_STOP(movieStopped);
                    break;
                default:
                    movieStopped = 0;
                    break;
            }
            if (movieStopped != 0) {
                // Preserve the paused sector before clearing requests for intervening loads.
                CdControlB(CdlGetlocL, NULL, positionResult);
                resumeLocation.minute             = positionResult[0];
                resumeLocation.second             = positionResult[1];
                resumeLocation.sector             = positionResult[2];
                resumeLocation.track              = 0;
                absoluteResumeSector              = CdPosToInt(&resumeLocation);
                resetQueue                        = &gCdCmdQueue;
                queue->activeRequest.resumeSector = absoluteResumeSector;
                queue->activeRequest.phase        = CD_COMMAND_PHASE_DISPATCH;
                queue->suspendResumeStep          = CD_COMMAND_SUSPEND_BEGIN;
                if (resetQueue->busy != 0) {
                    resetQueue->busy     = 0;
                    gDisplayState.cdBusy = DISPLAY_CD_IDLE;
                }
                memFillBytes(resetQueue->entries, 0, sizeof(resetQueue->entries));
                resetQueue->writeIdx          = 0;
                resetQueue->readIdx           = 0;
                resetQueue->step              = CD_COMMAND_HANDLER_BEGIN;
                resetQueue->cancelStep        = CD_COMMAND_CANCEL_BEGIN;
                resetQueue->suspendResumeStep = CD_COMMAND_SUSPEND_BEGIN;
            }
            break;
        case 3:
        case 5:
        case CD_COMMAND_SCENE_AUDIO_FAMILY:
            break;
    }
#undef CD_COMMAND_POLL_SUSPENDED_MOVIE_STOP
}

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
                        _cdCmdHandleMoviePlayback();
                        break;
                    case 7:
                        acropolisPlazaPollStreamCommands();
                        break;
                    case 5:
                        _cdCmdHandleStageMount();
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
            _cdCmdHandleRequestSuspension();
            break;
    }

    if (state->bootLoadActive != 0) {
        Fs_StepBootImage();
    }
}
