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

/* Appends a command to the ring and returns the slot it was written to. */
static inline s32 _cdCmdEnqueue(s32 cmd, u8* paramA, u8* paramB);

/* True when no transfer is in progress and the ring is empty. */
static inline u16 _cdCmdIsIdle(void);

static s32 CdCmd_GetOverlayStatus(void);

static s16 CdCmd_GetStreamMode(void);

/// Unused command-module entry point; retained for the original image layout.
static void CdCmd_UnusedStub1(void);

/// Unused command-module entry point; retained for the original image layout.
static void CdCmd_UnusedStub2(void);

static void CdCmd_EnqueueReplaceOverlay81(void);

static void CdCmd_ResetRing(void);

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

void* CdCmd_SetupMdecBuffers(void)
{
    CdCmdQueue* p;
    u16         vlcBufferKind;
    u8          timingBufferKind;
    s32*        sizeRow;
    s32         size;

    p = &gCdCmdQueue;
    if (p->sceneBuffersNeeded != 0) {
        if (p->sceneVlcTableMode == STREAM_SCENE_VLC_RESERVED_TABLE) {
            vlcBufferKind = p->sceneStream->data.scene.vlcBufferKind;
            switch (vlcBufferKind) {
                case STREAM_VLC_BUFFER_ALLOCATE:
                    p->vlcTable = memMalloc(STREAM_VLC_TABLE_BYTES, true);
                    break;
                case STREAM_VLC_BUFFER_ACTOR_0:
                    gGameSession->field_7C = 0;
                    p->vlcTable            = Fs_ActorLoadBase0;
                    break;
                case STREAM_VLC_BUFFER_ACTOR_1:
                    gGameSession->field_7E = 0;
                    p->vlcTable            = Fs_ActorLoadBase1;
                    break;
                case STREAM_VLC_BUFFER_ACTOR_2:
                    gGameSession->field_80 = 0;
                    p->vlcTable            = Fs_ActorLoadBase2;
                    break;
            }
            if (p->vlcTableBuilt == 0) {
                DecDCTvlcBuild(p->vlcTable);
                p->vlcTableBuilt = 1;
            }
        } else {
            p->vlcTable = NULL;
        }

        p->timingBuffer  = NULL;
        timingBufferKind = p->sceneStream->control.scene.timingBufferKind;
        switch (timingBufferKind) {
            case STREAM_TIMING_BUFFER_ALLOCATE:
                p->timingBuffer = memMalloc(p->sceneStream->data.scene.timingBufferBytes, true);
                break;
            case STREAM_TIMING_BUFFER_ACTOR_0:
                gGameSession->field_7C = 0;
                p->timingBuffer        = Fs_ActorLoadBase0;
                if (p->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_0) {
                    p->timingBuffer = (u32*)((u8*)Fs_ActorLoadBase0 + STREAM_VLC_TABLE_BYTES);
                }
                break;
            case STREAM_TIMING_BUFFER_ACTOR_1:
                gGameSession->field_7E = 0;
                p->timingBuffer        = Fs_ActorLoadBase1;
                if (p->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_1) {
                    p->timingBuffer = (u32*)((u8*)Fs_ActorLoadBase1 + STREAM_VLC_TABLE_BYTES);
                }
                break;
            case STREAM_TIMING_BUFFER_ACTOR_2:
                gGameSession->field_80 = 0;
                p->timingBuffer        = Fs_ActorLoadBase2;
                if (p->sceneStream->data.scene.vlcBufferKind == STREAM_VLC_BUFFER_ACTOR_2) {
                    p->timingBuffer = (u32*)((u8*)Fs_ActorLoadBase2 + STREAM_VLC_TABLE_BYTES);
                }
                break;
        }

        p->timingCursor = p->timingBuffer;
        if (p->decodeBufferBytes != 0) {
            p->decodeBuffer = memMalloc(p->decodeBufferBytes, true);
        }
    }

    D_8006AC00 = NULL;
    if (gGameSession->location.loc.stage == GAME_STAGE_NONE) {
        D_8006AC00 = memMalloc(0x4B000, true);
    } else if (Stream_FindSlot((u8*)&gGameSession->location.loc, 0, 0) < 0) {
        return NULL;
    } else {
        sizeRow = CdCmd_MapHeapSizes[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage];
        if (sizeRow != NULL) {
            size = sizeRow[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area];
            if (size != 0) {
                D_8006AC00 = memMalloc(size, true);
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
    s16         ret;
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
            ret = CdCmd_PollStatus(0, 0);
            if (ret != 1) {
                if (ret < 2) {
                    if (ret == 0) {
                        return;
                    }
                    goto end_check;
                }
                if (ret != 2) {
                    goto end_check;
                }
                CdFlush();
            }
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
                goto end_check;
            }
            state->continueMovie = 1;
            Stream_PollPlayback(0, ((u16)state->movieFrame - 1) * 0xA);
            state->step = state->step + 1;
            /* fallthrough */
        case 1:
        end_check:
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
            return;
    }
}

static void CdCmd_HandleFileLoad(void)
{
    CdCmdQueue* state;
    CdCmdQueue* p;
    s16         ret;
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
            ret = CdCmd_PollStatus(0, 0);
            if (ret != 1) {
                if (ret < 2) {
                    if (ret == 0) {
                        return;
                    }
                    goto end_check;
                }
                if (ret != 2) {
                    goto end_check;
                }
                CdFlush();
            }
            mode = 0xA0;
            CdControlB(CdlSetmode, &mode, NULL);
            if (state->entries[state->readIdx].args.file.loadMode != CD_COMMAND_LOAD_DEFAULT) {
                state->step = 4;
                goto do_load;
            }
            if (req[3] == 0) {
                state->step = 4;
                goto do_load;
            }
            if (req[0] != 0) {
                state->step = 4;
                goto do_load;
            }
            state->step = state->step + 1;
            /* fallthrough */
        case 1:
            Fs_PrepareFolderLoad(req[3], req[2], req[1]);
            goto increment_step;
        case 2: {
            s32 sync;
            s32 diskErr;

            sync    = CdSync(1, NULL);
            diskErr = CdlDiskError;
            if (sync == diskErr) {
                CdSyncCallback(NULL);
                CdReadyCallback(NULL);
                goto wait_reset_step1;
            }
            Fs_CheckReadTimeout();
            status = Fs_CdOpStatus;
            switch (status) {
                case 0x80:
                    ret = CdCmd_PollStatus(0, 0);
                    if (ret != 1) {
                        if (ret < 2) {
                            if (ret == 0) {
                                return;
                            }
                            goto end_check;
                        }
                        if (ret != 2) {
                            goto end_check;
                        }
                        CdFlush();
                    }
                    sync    = CdSync(1, NULL);
                    diskErr = CdlDiskError;
                    if (sync == diskErr) {
                    wait_reset_step1:
                        Fs_WaitDiskReset(1);
                    }
                    state->step = 1;
                    goto end_check;
                case 0xFF:
                    CdSyncCallback(NULL);
                    CdReadyCallback(NULL);
                    goto increment_step;
                case 0x10:
                case 0x20:
                case 0x40:
                    ret = CdCmd_PollStatus(0, 0);
                    if (ret != 1) {
                        if (ret < 2) {
                            if (ret == 0) {
                                return;
                            }
                            goto end_check;
                        }
                        if (ret != 2) {
                            goto end_check;
                        }
                        CdFlush();
                    }
                    Fs_RetryReadN();
                    goto end_check;
            }
            goto end_check;
        }
        case 3:
            ret = CdCmd_PollStatus(0, 0);
            if (ret != 1) {
                if (ret < 2) {
                    if (ret == 0) {
                        return;
                    }
                    goto do_load;
                }
                if (ret != 2) {
                    goto do_load;
                }
                CdFlush();
            }
            Fs_BuildFolderTables(req[3], req[2], req[1]);
            state->step = state->step + 1;
            /* fallthrough */
        case 4:
        do_load:
            Fs_LoadFile(
                req,
                (u8)state->entries[state->readIdx].args.file.loadMode,
                state->entries[state->readIdx].args.file.imageXPageOffset,
                state->entries[state->readIdx].args.file.imageYOffset);
        increment_step:
            state->step = state->step + 1;
            goto end_check;
        case 5: {
            s32 sync;
            s32 diskErr;

            sync    = CdSync(1, NULL);
            diskErr = CdlDiskError;
            if (sync == diskErr) {
                CdSyncCallback(NULL);
                CdReadyCallback(NULL);
                goto wait_reset_step4;
            }
            Fs_CheckReadTimeout();
            status = Fs_CdOpStatus;
            switch (status) {
                case 0x80:
                    ret = CdCmd_PollStatus(0, 0);
                    if (ret != 1) {
                        if (ret < 2) {
                            if (ret == 0) {
                                return;
                            }
                            goto end_check;
                        }
                        if (ret != 2) {
                            goto end_check;
                        }
                        CdFlush();
                    }
                    sync    = CdSync(1, NULL);
                    diskErr = CdlDiskError;
                    if (sync == diskErr) {
                    wait_reset_step4:
                        Fs_WaitDiskReset(1);
                    }
                    state->step = 4;
                    goto end_check;
                case 0xFF:
                    if (state->imageLoadStatus != status) {
                        goto end_check;
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
                    goto end_check;
                case 0x10:
                case 0x20:
                case 0x40:
                    ret = CdCmd_PollStatus(0, 0);
                    if (ret == 1) {
                        Fs_RetryReadN();
                        goto end_check;
                    }
                    if (ret < 2) {
                        if (ret == 0) {
                            return;
                        }
                        goto end_check;
                    }
                    if (ret == 2) {
                        CdFlush();
                        Fs_RetryReadN();
                    }
                    goto end_check;
            }
            goto end_check;
        }
    }

end_check:
    if (state->imageDecodePending != 0) {
        CdCmd_StepVlcRebuild();
    }
}

static void CdCmd_HandleMount(void)
{
    CdCmdQueue* state;
    s32         cmd;
    s32         status;
    s16         ret;

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
                    Fs_SelectStage(stageIndex & 0xFF);
                    goto increment_step;
                case 1:
                    if (CdSync(1, NULL) == CdlDiskError) {
                        CdSyncCallback(NULL);
                        CdReadyCallback(NULL);
                        goto wait_reset_clear_step;
                    }
                    Fs_CheckReadTimeout();
                    status = Fs_CdOpStatus;
                    switch (status) {
                        case 0x80:
                            ret = CdCmd_PollStatus(0, 0);
                            if (ret != step) {
                                if (ret < 2) {
                                    return;
                                }
                                if (ret != 2) {
                                    return;
                                }
                                CdFlush();
                            }
                            if (CdSync(1, NULL) == CdlDiskError) {
                            wait_reset_clear_step:
                                Fs_WaitDiskReset(1);
                            }
                            state->step = 0;
                            return;
                        case 0xFF:
                            CdSyncCallback(NULL);
                            CdReadyCallback(NULL);
                        increment_step:
                            state->step = state->step + 1;
                            return;
                        case 0x10:
                        case 0x20:
                        case 0x40:
                            ret = CdCmd_PollStatus(0, 0);
                            if (ret != 1) {
                                if (ret < 2) {
                                    return;
                                }
                                if (ret != 2) {
                                    return;
                                }
                                CdFlush();
                            }
                            Fs_RetryReadN();
                            return;
                    }
                    return;
                case 2:
                    ret = CdCmd_PollStatus(0, 0);
                    if (ret != 1) {
                        if (ret < 2) {
                            return;
                        }
                        if (ret != step) {
                            return;
                        }
                        CdFlush();
                    }
                    Fs_InitFolderTable(stageIndex & 0xFF);
                    goto cleanup;
            }
            return;
        }
        case CD_COMMAND_READ_STAGE_HEADER:
            status = Fs_CdOpStatus;
            if (status != 0xFF) {
                goto case55_cont;
            }
        cleanup:
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
        case55_cont:
            if (status != 0x80) {
                return;
            }
            ret = CdCmd_PollStatus(0, 0);
            if (ret != 1) {
                if (ret < 2) {
                    return;
                }
                if (ret != 2) {
                    return;
                }
                CdFlush();
            }
            if (CdSync(1, NULL) == CdlDiskError) {
                Fs_WaitDiskReset(1);
            }
            Fs_InitStage0Tables();
            return;
    }
}

s32 CdCmd_ActivatePhase1(void)
{
    CdCmdQueue* p;
    u8          cmd;

    p   = &gCdCmdQueue;
    cmd = p->entries[p->readIdx].cmd;
    if (cmd != CD_COMMAND_EMPTY) {
        p->activeRequest.entry.cmd           = cmd;
        p->activeRequest.entry.stage         = p->entries[p->readIdx].stage;
        p->activeRequest.entry.fileGroup     = p->entries[p->readIdx].fileGroup;
        p->activeRequest.entry.fileIndex     = p->entries[p->readIdx].fileIndex;
        p->activeRequest.entry.args.bytes[0] = p->entries[p->readIdx].args.bytes[0];
        p->activeRequest.entry.args.bytes[1] = p->entries[p->readIdx].args.bytes[1];
        p->activeRequest.entry.args.bytes[2] = p->entries[p->readIdx].args.bytes[2];
        p->activeRequest.entry.args.bytes[3] = p->entries[p->readIdx].args.bytes[3];
        p->activeRequest.phase               = CD_COMMAND_PHASE_CANCEL;
        return 1;
    }
    if ((u16)p->sceneAudioMode != CD_COMMAND_SCENE_INACTIVE) {
        p->activeRequest.phase = CD_COMMAND_PHASE_CANCEL;
        return 1;
    }
    return 0;
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
                            goto case8_cleanup;
                        }
                        return;
                    case CD_COMMAND_CANCEL_FINISH:
                        if (CdAudio_Phase.waveLoadStep == CD_AUDIO_WAVE_LOAD_STEP_DONE) {
                            goto case8_cleanup;
                        }
                        return;
                    default:
                        return;
                }
            } else {
            case8_cleanup:
                p = &gCdCmdQueue;
                memFillBytes(&p->activeRequest, 0, sizeof(p->activeRequest));
                p->replacementEntry.cmd = CD_COMMAND_EMPTY;
                p->blockGamePause       = 0;
                p->sceneAudioMode       = CD_COMMAND_SCENE_INACTIVE;
                Gp_ApplySndBankMasks(p->sceneStream->data.scene.soundBankMask);
                Gp_RestoreStreamRng();
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
            return;
        case 5:
        default:
            return;
    }
}

u16 CdCmd_ActivatePhase2(void)
{
    CdCmdQueue* p;
    u8          cmd;

    p = &gCdCmdQueue;
    if (p->activeRequest.phase != CD_COMMAND_PHASE_DISPATCH) {
        return 1;
    }
    cmd = p->entries[p->readIdx].cmd;
    if ((cmd >> 4) != 8) {
        if (cmd != CD_COMMAND_EMPTY) {
            goto do_work;
        }
    }
    return 0;
do_work:
    p->activeRequest.entry.cmd           = cmd;
    p->activeRequest.entry.stage         = p->entries[p->readIdx].stage;
    p->activeRequest.entry.fileGroup     = p->entries[p->readIdx].fileGroup;
    p->activeRequest.entry.fileIndex     = p->entries[p->readIdx].fileIndex;
    p->activeRequest.entry.args.bytes[0] = p->entries[p->readIdx].args.bytes[0];
    p->activeRequest.entry.args.bytes[1] = p->entries[p->readIdx].args.bytes[1];
    p->activeRequest.entry.args.bytes[2] = p->entries[p->readIdx].args.bytes[2];
    p->activeRequest.entry.args.bytes[3] = p->entries[p->readIdx].args.bytes[3];
    p->activeRequest.phase               = CD_COMMAND_PHASE_SUSPEND;
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

/* Appends a command to the ring and returns the slot it was written to. */
static inline s32 _cdCmdEnqueue(s32 cmd, u8* paramA, u8* paramB)
{
    CdCmdQueue* p;
    CdCmdEntry* entry;
    u16         writeIdx;
    u16         next;

    p                    = &gCdCmdQueue;
    entry                = &p->entries[p->writeIdx];
    entry->cmd           = cmd;
    entry->stage         = paramA[3];
    entry->fileGroup     = paramA[2];
    entry->fileIndex     = paramA[0];
    entry->args.bytes[0] = paramB[0];
    entry->args.bytes[1] = paramB[1];
    entry->args.bytes[2] = paramB[2];
    entry->args.bytes[3] = paramB[3];
    writeIdx             = p->writeIdx;
    next                 = writeIdx + 1;
    p->writeIdx          = next;
    next                 = p->writeIdx % ARRAY_SIZE(p->entries);
    p->writeIdx          = next;
    return writeIdx;
}

/* True when no transfer is in progress and the ring is empty. */
static inline u16 _cdCmdIsIdle(void)
{
    CdCmdQueue* p;

    p = &gCdCmdQueue;
    if (p->activeRequest.phase != CD_COMMAND_PHASE_DISPATCH) {
        return 0;
    }
    if (p->writeIdx != p->readIdx) {
        return 0;
    }
    return 1;
}

u16 CdCmd_EnqueueFollowUp(void)
{
    CdCmdQueue* p;
    u8          params[4];

    p = &gCdCmdQueue;
    switch (p->activeRequest.entry.cmd >> 4) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            break;
        case 6:
        case 7:
            switch (p->suspendResumeStep) {
                case 0:
                    params[3] = p->activeRequest.entry.stage;
                    params[2] = p->activeRequest.entry.fileGroup;
                    params[0] = p->activeRequest.entry.fileIndex;
                    if ((p->activeRequest.entry.cmd >> 4) == 6) {
                        _cdCmdEnqueue(CD_COMMAND_CONTINUE_STREAM, params, p->activeRequest.entry.args.bytes);
                    } else if ((p->activeRequest.entry.cmd >> 4) == 7) {
                        _cdCmdEnqueue(CD_COMMAND_RESUME_STREAM_AT_POSITION, params, p->activeRequest.entry.args.bytes);
                    }
                    p->suspendResumeStep++;
                    return 0;
                case 1:
                    if (p->movieReady != 0 || _cdCmdIsIdle()) {
                        memFillBytes(&p->activeRequest, 0, sizeof(p->activeRequest));
                        p->suspendResumeStep = 0;
                        break;
                    }
                    return 0;
            }
            break;
        case 8:
            break;
    }
    return 1;
}

s32 CdCmd_Enqueue(s32 cmd, u8* paramA, u8* paramB)
{
    return _cdCmdEnqueue(cmd, paramA, paramB);
}

u16 CdCmd_IsIdle(void)
{
    CdCmdQueue* p;

    p = &gCdCmdQueue;
    if (p->activeRequest.phase != CD_COMMAND_PHASE_DISPATCH) {
        return 0;
    }
    return p->writeIdx == p->readIdx;
}

u16 CdCmd_IsSlotEmpty(s16 arg0)
{
    return gCdCmdQueue.entries[arg0].cmd == CD_COMMAND_EMPTY;
}

void CdCmd_BuildVlcIfStream(void)
{
    gCdCmdQueue.movieFrame = 1;
    if (Stream_HasActiveLowId(&gGameSession->location.loc.view) != 0) {
        DecDCTvlcBuild(Fs_ActorLoadBase0);
        gGameSession->field_7C = 0;
    }
}

void CdCmd_ClearQueue(void)
{
    memFillBytes(&gCdCmdQueue, 0, sizeof(gCdCmdQueue));
}

s32 CdCmd_DropPending(void)
{
    CdCmdQueue* p;
    u16         i;
    u16         writeIdx;

    p        = &gCdCmdQueue;
    writeIdx = p->writeIdx;
    if (writeIdx == p->readIdx) {
        return 1;
    }

    i = p->readIdx + 1;
    i = i % ARRAY_SIZE(p->entries);
    if (i != writeIdx) {
        do {
            p->entries[i].cmd = CD_COMMAND_EMPTY;
            i                 = i + 1;
            i                 = i % ARRAY_SIZE(p->entries);
        } while (i != p->writeIdx);
    }

    p->writeIdx = p->readIdx + 1;
    p->writeIdx = p->writeIdx % ARRAY_SIZE(p->entries);
    return 0;
}

void CdCmd_SelectMdecBuffer(void)
{
    CdCmdQueue* p;

    p = &gCdCmdQueue;
    if (Stream_FindSlot((u8*)&gGameSession->location.loc, 0, 0) >= 0) {
        D_8006AC40 = D_8006AC00;
    }
    p->movieFrameAvailable = 0;
}

static s32 CdCmd_GetOverlayStatus(void)
{
    CdCmdQueue* p;
    s32         ret;

    p = &gCdCmdQueue;
    if (p->sceneSlotIndex > CD_COMMAND_NO_SCENE_SLOT) {
        return 2;
    }
    if (p->field_236 < 0) {
        return -1;
    }
    ret = 3;
    return ret;
}

static s16 CdCmd_GetStreamMode(void)
{
    return gCdCmdQueue.sceneAudioMode;
}

void CdCmd_StartOverlay(u16 arg0, u16 arg1, u16 arg2)
{
    CdCmdQueue* p;

    p                 = &gCdCmdQueue;
    p->field_1FF      = 1;
    p->field_236      = -1;
    p->sceneSlotIndex = Gp_FindStreamSlot(arg0, arg1, arg2, 0);
}

void CdCmd_UnusedStub0(void)
{
}

void CdCmd_CancelReplaceAndActivate(void)
{
    gCdCmdQueue.replacementEntry.cmd = CD_COMMAND_EMPTY;
    CdCmd_ActivatePhase1();
    Gp_RestoreStreamRng();
}

static void CdCmd_UnusedStub1(void)
{
}

static void CdCmd_UnusedStub2(void)
{
}

void CdCmd_UnusedStub3(void)
{
}

void CdCmd_EnqueueOverlay81(void)
{
    CdCmdQueue* p;
    u8          sp10;

    p = &gCdCmdQueue;
    if (p->sceneSlotIndex > CD_COMMAND_NO_SCENE_SLOT) {
        sp10              = p->sceneSlotIndex;
        p->sceneAudioMode = CD_COMMAND_SCENE_STARTING_AUDIO;
        CdCmd_Enqueue(CD_COMMAND_PLAY_SCENE_AUDIO, 0, &sp10);
    } else {
        p->sceneAudioMode = CD_COMMAND_SCENE_PLAYING;
    }
}

static void CdCmd_EnqueueReplaceOverlay81(void)
{
    CdCmdQueue* p;
    u8          sp10;

    p = &gCdCmdQueue;
    if (p->sceneSlotIndex > CD_COMMAND_NO_SCENE_SLOT) {
        sp10 = p->sceneSlotIndex;
        CdCmd_EnqueueReplace(CD_COMMAND_PLAY_SCENE_AUDIO, 0, &sp10);
    }
}

void CdCmd_EnqueueOverlay82(void)
{
    CdCmdQueue* p;
    u8          sp10;

    p = &gCdCmdQueue;
    if (p->sceneSlotIndex > CD_COMMAND_NO_SCENE_SLOT) {
        sp10              = p->sceneSlotIndex;
        p->sceneAudioMode = CD_COMMAND_SCENE_STARTING_AUDIO;
        CdCmd_Enqueue(CD_COMMAND_START_SCENE_AUDIO, 0, &sp10);
    }
}

void CdCmd_EnqueueReplaceOverlay82(void)
{
    CdCmdQueue* p;
    u8          sp10;

    p = &gCdCmdQueue;
    if (p->sceneSlotIndex > CD_COMMAND_NO_SCENE_SLOT) {
        sp10 = p->sceneSlotIndex;
        CdCmd_EnqueueReplace(CD_COMMAND_START_SCENE_AUDIO, 0, &sp10);
    }
}

void CdCmd_EnqueueReplace(s32 cmd, u8* paramA, u8* paramB)
{
    CdCmdQueue* p;
    CdCmdEntry* entry;

    p                    = &gCdCmdQueue;
    entry                = &p->replacementEntry;
    entry->cmd           = cmd;
    entry->stage         = paramA[3];
    entry->fileGroup     = paramA[2];
    entry->fileIndex     = paramA[0];
    entry->args.bytes[0] = paramB[0];
    entry->args.bytes[1] = paramB[1];
    entry->args.bytes[2] = paramB[2];
    entry->args.bytes[3] = paramB[3];
}

s32 CdCmd_CommitReplace(void)
{
    CdCmdQueue* p;
    CdCmdEntry* entry;
    u16         writeIdx;
    u16         next;
    u8          paramA[4];
    u8*         paramB;

    p = &gCdCmdQueue;
    if (*(volatile u8*)&p->replacementEntry.cmd == CD_COMMAND_EMPTY) {
        return -1;
    }

    paramA[3] = p->replacementEntry.stage;
    paramA[2] = p->replacementEntry.fileGroup;
    paramA[0] = p->replacementEntry.fileIndex;

    writeIdx         = p->writeIdx;
    entry            = &p->entries[writeIdx];
    entry->cmd       = p->replacementEntry.cmd;
    entry->stage     = paramA[3];
    entry->fileGroup = paramA[2];
    entry->fileIndex = paramA[0];

    paramB               = p->replacementEntry.args.bytes;
    entry->args.bytes[0] = paramB[0];
    entry->args.bytes[1] = paramB[1];
    entry->args.bytes[2] = paramB[2];
    entry->args.bytes[3] = paramB[3];

    p->replacementEntry.cmd = CD_COMMAND_EMPTY;

    writeIdx    = p->writeIdx;
    next        = writeIdx + 1;
    p->writeIdx = next;
    next        = p->writeIdx % ARRAY_SIZE(p->entries);
    p->writeIdx = next;
    return (s16)writeIdx;
}

/**
 * Non-zero while the queue must not accept new work: either the ring is
 * completely idle (no error latched and nothing pending) or the entry the
 * consumer is about to run is a 0x8_ command.
 */
u16 CdCmd_IsIdleOrOverlayPending(void)
{
    CdCmdQueue* p;

    p = &gCdCmdQueue;
    if (_cdCmdIsIdle() == 0) {
        if ((p->entries[p->readIdx].cmd >> 4) != 8) {
            return 0;
        }
    }
    return 1;
}

CdCmdEntry* CdCmd_NextEntry(void)
{
    CdCmdQueue* p;
    s32         index;
    CdCmdEntry* entry;

    p     = &gCdCmdQueue;
    index = CdCmd_EntryIter;
    entry = &p->entries[index];
    if (index == p->writeIdx) {
        return NULL;
    }
    CdCmd_EntryIter = index + 1;
    CdCmd_EntryIter = CdCmd_EntryIter % ARRAY_SIZE(p->entries);
    return entry;
}

void CdCmd_SetBusy(void)
{
    if (gCdCmdQueue.busy == 0) {
        gCdCmdQueue.busy     = 1;
        gDisplayState.cdBusy = DISPLAY_CD_BUSY;
    }
}

void CdCmd_ClearBusy(void)
{
    if (gCdCmdQueue.busy != 0) {
        gCdCmdQueue.busy     = 0;
        gDisplayState.cdBusy = DISPLAY_CD_IDLE;
    }
}

static void CdCmd_ResetRing(void)
{
    CdCmdQueue* state;

    state = &gCdCmdQueue;
    memFillBytes(state->entries, 0, sizeof(state->entries));
    state->writeIdx          = 0;
    state->readIdx           = 0;
    state->step              = 0;
    state->cancelStep        = CD_COMMAND_CANCEL_BEGIN;
    state->suspendResumeStep = 0;
}

void CdCmd_ResetEntryIter(void)
{
    CdCmd_EntryIter = gCdCmdQueue.readIdx;
}

void CdCmd_EnqueueUnlessStream(s32 cmd, u8* paramA, u8* paramB)
{
    CdCmdQueue* p;
    CdCmdEntry* entry;
    u16         writeIdx;
    u16         next;

    p = &gCdCmdQueue;
    if ((p->entries[p->readIdx].cmd >> 4) != 8) {
        entry                = &p->entries[p->writeIdx];
        entry->cmd           = cmd;
        entry->stage         = paramA[3];
        entry->fileGroup     = paramA[2];
        entry->fileIndex     = paramA[0];
        entry->args.bytes[0] = paramB[0];
        entry->args.bytes[1] = paramB[1];
        entry->args.bytes[2] = paramB[2];
        entry->args.bytes[3] = paramB[3];
        writeIdx             = p->writeIdx;
        next                 = writeIdx + 1;
        p->writeIdx          = next;
        next                 = p->writeIdx % ARRAY_SIZE(p->entries);
        p->writeIdx          = next;
    }
}

void CdCmd_AdvanceRead(void)
{
    CdCmdQueue* state;

    state = &gCdCmdQueue;
    if (state->busy != 0) {
        state->busy          = 0;
        gDisplayState.cdBusy = DISPLAY_CD_IDLE;
    }
    state->step               = 0;
    state->cancelStep         = CD_COMMAND_CANCEL_BEGIN;
    state->pausePlayClock     = 0;
    state->cdOperationPending = 0;
    if (state->readIdx != state->writeIdx) {
        state->entries[state->readIdx].cmd = CD_COMMAND_EMPTY;
        state->readIdx                     = state->readIdx + 1;
        state->readIdx                     = state->readIdx % ARRAY_SIZE(state->entries);
    }
}

void CdCmd_LoadActiveEntry(void)
{
    CdCmdQueue* p;

    p                                    = &gCdCmdQueue;
    p->activeRequest.entry.cmd           = p->entries[p->readIdx].cmd;
    p->activeRequest.entry.stage         = p->entries[p->readIdx].stage;
    p->activeRequest.entry.fileGroup     = p->entries[p->readIdx].fileGroup;
    p->activeRequest.entry.fileIndex     = p->entries[p->readIdx].fileIndex;
    p->activeRequest.entry.args.bytes[0] = p->entries[p->readIdx].args.bytes[0];
    p->activeRequest.entry.args.bytes[1] = p->entries[p->readIdx].args.bytes[1];
    p->activeRequest.entry.args.bytes[2] = p->entries[p->readIdx].args.bytes[2];
    p->activeRequest.entry.args.bytes[3] = p->entries[p->readIdx].args.bytes[3];
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
