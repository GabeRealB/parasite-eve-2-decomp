#include "fs.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libcd.h>
#include <psyq/libpress.h>

#include "types.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream_types.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_dryfield.h"

/* Define BSS before API headers to preserve first-declaration order. */
static s32 D_8006AC08;

static u16 D_8006AC0C;

static u16 D_8006AC0E;

static u16 D_8006AC10;

static u16 D_8006AC12;

static u16 D_8006AC14;

static u16 D_8006AC16;

static u16 D_8006AC18;

static u16 D_8006AC1A;

static u16 D_8006AC1C;

static u16 D_8006AC1E;

static u16 D_8006AC20;

static s32 D_8006AC24;

static u16 D_8006AC28;

StreamInterFile D_8006AC30;

static u_short* D_8006AC38;

u16 D_8006AC3C;

void* D_8006AC40;

void* D_8006AC44;

u_long* D_8006AC48[2];

u_long* D_8006AC50[2];

u16 D_8006AC58;

u16 D_8006AC5A;

u16 D_8006AC5C;

u16* D_8006AC60;

static void* D_8006AC64;

static u_long* D_8006AC68;

u16 D_8006AC6C;

#include "main/stream.h"
#include "stream.h"

extern s32 StCdIntrFlag;

extern void func_map_akropolis_80179988(u8* arg0);

extern void func_map_neo_ark_801799BC(u8* arg0);

static void Mdec_SetupBuffers(u8* arg0);

static void _streamLoadMovieSlotState(u32 slotIndex);

static __inline__ void _streamClearDisplayBuffers(RECT* clearRect);

static void _streamCompleteDecodedFrame(void);

static void _mdecMovieOutputCallback(void);

static void _mdecStartMovieFrameOutput(void);

static void Mdec_DecodeFrame(void);

static __inline__ u16 Stream_SeekPosition(CdlLOC* loc);

/* Resets the decoder and the stream ring, routes decoded slices to the upload
 * callback and applies CD volume table entry 0 ahead of a streaming read. */
static __inline__ void _streamStartDecode(void);

static __inline__ s32 _streamStartRead(void);

static __inline__ void _streamUploadFrameStrips(RECT* uploadRect, u32 vramX, u32 vramY, u16 offsetDrawBuffer);

/// VRAM layout and byte capacities reserved for resident movie playback.
enum {
    STREAM_MOVIE_FRAMEBUFFER_STRIDE_ROWS    = 272,
    STREAM_MOVIE_RGB24_DISPLAY_WORDS        = FILE_SYSTEM_IMAGE_WIDTH * 3 / 2,
    STREAM_MOVIE_RGB24_STRIP_WORDS          = FILE_SYSTEM_IMAGE_STRIP_WIDTH * 3 / 2,
    STREAM_MOVIE_RGB16_OUTPUT_WORDS_PER_ROW = FILE_SYSTEM_IMAGE_STRIP_WIDTH / 2,
    STREAM_MOVIE_RGB24_OUTPUT_WORDS_PER_ROW = STREAM_MOVIE_RGB24_STRIP_WORDS / 2,
    STREAM_MOVIE_WORKSPACE_NORMAL_BYTES     = 0x4A800,
    STREAM_MOVIE_WORKSPACE_STREAMING_BYTES  = 0x45400,
    STREAM_MOVIE_SAVED_IMAGE_X              = 320,
    STREAM_MOVIE_SAVED_IMAGE_WORDS          = 160,
    STREAM_MOVIE_SAVED_IMAGE_ROWS           = 256,
    STREAM_MOVIE_IMAGE_BACKUP_FIRST_X       = 704,
    STREAM_MOVIE_IMAGE_BACKUP_SECOND_X      = 864,
};

/// Steps of the post-movie memory and optional sprite-image restoration.
enum {
    STREAM_GAME_RESTORE_MEMORY      = 0,
    STREAM_GAME_RESTORE_WAIT_IMAGES = 1,
    STREAM_GAME_RESTORE_COMPLETE    = 2,
};

u16 D_8005EAEC = 0;
u16 D_8005EAEE = 0;

static void Mdec_SetupBuffers(u8* arg0)
{
    s32     temp_lo;
    u16*    temp_v1;
    u16*    temp_v1_2;
    u_long* temp_v1_3;
    u_long* temp_v1_4;
    s32     temp_lo_2;
    u16*    temp_v1_5;

    D_8006AC5C                   = 0;
    D_8006AC3C                   = 1;
    gCdCmdQueue.field_24A        = 0;
    gCdCmdQueue.movieVramStaging = 0;
    D_8006AC24                   = 0x20;
    D_8006AC38                   = Fs_ActorLoadBase0;
    D_8006AC60                   = (u16*)((u8*)Fs_ActorLoadBase0 + STREAM_VLC_TABLE_BYTES);
    D_8006AC64                   = D_8006AC40;

    switch ((s8)(arg0[3] + 1)) {
        case 0:
            D_8006AC24 = 0x28;
            D_8006AC38 = (u_short*)D_8006AC40;
            temp_v1    = (D_8006AC60 = (u16*)((u8*)D_8006AC40 + STREAM_VLC_TABLE_BYTES));
            {
                u16 h         = D_8006AC6C;
                s32 stride    = h * 0x30;
                temp_lo       = D_8006AC5A * h;
                D_8006AC48[1] = (u_long*)(temp_v1_2 = (u16*)((u8*)temp_v1 + 0x14000));
                D_8006AC48[0] = (u_long*)temp_v1_2;
                D_8006AC50[0] = (temp_v1_3 = (u_long*)((u8*)temp_v1_2 + stride));
                D_8006AC50[1] = (temp_v1_4 = (u_long*)((u8*)temp_v1_3 + temp_lo));
                D_8006AC44    = (u8*)temp_v1_4 + temp_lo;
            }
            return;
        case 1: {
            u_long** p50;
            u_long** p48;
            void*    base;

            temp_lo_2 = D_8006AC5A * D_8006AC6C;
            p50       = D_8006AC50;
            temp_v1_5 = (u16*)((u8*)D_8006AC60 + 0x10000);
            base      = D_8006AC40;
            p48       = D_8006AC48;
            p50[0]    = (u_long*)temp_v1_5;
            p48[0]    = (u_long*)base;
            p50[1]    = (u_long*)((u8*)temp_v1_5 + temp_lo_2);
            p48[1]    = (u_long*)((u8*)base + (temp_lo_2 * 2));
            return;
        }
        case 2:
            func_map_akropolis_80179988(arg0);
            return;
        case 3:
            func_map_dryfield_80179954(arg0);
            return;
        case 6:
            func_map_neo_ark_801799BC(arg0);
            return;
    }
}

/// Caches a movie descriptor and resets presentation suppression and VLC slicing.
///
/// The low halfword of `slotIndex` must select a movie slot in 0..14. Dimensions
/// are pixels/rows, the VRAM origin is words/rows, and the frame limit is one-based
/// (1..32767 for the signed completion clamp). No validation or allocation occurs.
static void _streamLoadMovieSlotState(u32 slotIndex)
{
    const StreamSlot* slotTable;
    const StreamSlot* movieSlot;

    gCdCmdQueue.suppressMoviePresentation = false;
    slotTable                             = Stream_Slots;
    D_8006AC12                            = 0;
    movieSlot                             = &slotTable[slotIndex & 0xFFFF];
    D_8006AC08                            = movieSlot->startSector;
    D_8006AC0C                            = movieSlot->data.movie.frameLimit;
    D_8006AC5A                            = movieSlot->data.movie.width;
    D_8006AC6C                            = movieSlot->data.movie.height;
    D_8006AC0E                            = movieSlot->data.movie.vramX;
    D_8006AC10                            = movieSlot->data.movie.vramY;
    D_8006AC16                            = movieSlot->data.movie.loopMode;
    D_8006AC14                            = movieSlot->data.movie.displayMode;
    D_8006AC58                            = movieSlot->data.movie.volumeTableIndex;
    D_8006AC18                            = movieSlot->data.movie.uploadMode;
}

s16 streamFindMovieSlot(const GameLocationKey* location, s32 subId, s32 requireViewStream)
{
    s32         slotIndex;
    s32         hasMatch;
    s32         matchedSlotIndex;
    StreamSlot* slotTable;
    s32         movieKind;

    matchedSlotIndex   = 0;
    slotIndex          = matchedSlotIndex;
    hasMatch           = matchedSlotIndex;
    slotTable          = Stream_Slots;
    movieKind          = STREAM_KIND_MOVIE;
    requireViewStream &= 0xFFFF;

loop:
    if (slotTable[slotIndex & 0xFFFF].kind == movieKind) {
        if (slotTable[slotIndex & 0xFFFF].startSector != 0) {
            if (slotTable[slotIndex & 0xFFFF].key.parts.id == location->view) {
                if (slotTable[slotIndex & 0xFFFF].subId == (subId & 0xFFFF)) {
                    if (slotTable[slotIndex & 0xFFFF].key.parts.group == STREAM_KEY_ANY_GROUP) {
                        if (requireViewStream == 0) {
                            goto matched;
                        }
                        if (slotTable[slotIndex & 0xFFFF].data.movie.viewStream != 0) {
                            hasMatch = 1;
                            goto matched_result;
                        }
                    } else if (slotTable[slotIndex & 0xFFFF].key.parts.group == location->room) {
                        if (requireViewStream == 0) {
                            goto matched;
                        }
                        // A room-specific rejection suppresses later matches.
                        if (slotTable[slotIndex & 0xFFFF].data.movie.viewStream == 0) {
                            goto done;
                        }
                        hasMatch = 1;
                        goto matched_result;
                    }
                }
            }
        }
    }
    slotIndex = slotIndex + 1;
    if ((u32)(slotIndex & 0xFFFF) < ARRAY_SIZE(Stream_Slots)) {
        goto loop;
    }
done:
    if ((hasMatch & 0xFFFF) == 0) {
        goto ret_neg;
    }
    return matchedSlotIndex;

matched:
    hasMatch = 1;
matched_result:
    matchedSlotIndex = slotIndex;
    goto done;

ret_neg:
    return STREAM_SLOT_NOT_FOUND;
}

s16 streamFindViewMovieSlot(const GameLocationKey* location)
{
    s32         slotIndex;
    StreamSlot* slotTable;
    s32         movieKind;
    s32         shiftedSlotIndex;

    slotIndex = 0;
    slotTable = Stream_Slots;
    movieKind = STREAM_KIND_MOVIE;
    while (1) {
        if (slotTable[slotIndex & 0xFFFF].kind == movieKind) {
            if (slotTable[slotIndex & 0xFFFF].key.parts.id == location->view) {
                if (slotTable[slotIndex & 0xFFFF].key.parts.group == STREAM_KEY_ANY_GROUP) {
                    if (slotTable[slotIndex & 0xFFFF].data.movie.viewStream != 0) {
                        shiftedSlotIndex = slotIndex << 16;
                        return shiftedSlotIndex >> 16;
                    }
                }
                if (slotTable[slotIndex & 0xFFFF].key.parts.group == location->room) {
                    if (slotTable[slotIndex & 0xFFFF].data.movie.viewStream != 0) {
                        shiftedSlotIndex = slotIndex << 16;
                        return shiftedSlotIndex >> 16;
                    }
                }
            }
        }
        slotIndex = slotIndex + 1;
        if ((u32)(slotIndex & 0xFFFF) >= ARRAY_SIZE(Stream_Slots)) {
            break;
        }
    }
    return STREAM_SLOT_NOT_FOUND;
}

u16 streamPollGameRestore(s32 selectConfiguredAuxHeap, s32 reloadSpriteImages)
{
    RECT         restoreRect;
    u8           fileKey[4];
    u8           loadOptions[4];
    GameSession* session;
    CdCmdQueue*  queue;
    s32          restoreStep;
    u8           stageId;
    u8           areaId;
    u8           spriteVariant;

    queue       = &gCdCmdQueue;
    restoreStep = D_8006AC28;
    switch (restoreStep) {
        case STREAM_GAME_RESTORE_MEMORY:
            // Recover displaced VRAM before reusing the movie's auxiliary storage.
            if (D_8006AC1E != 0) {
                restoreRect.x = STREAM_MOVIE_IMAGE_BACKUP_FIRST_X;
                restoreRect.y = 0;
                restoreRect.w = STREAM_MOVIE_SAVED_IMAGE_WORDS;
                restoreRect.h = STREAM_MOVIE_SAVED_IMAGE_ROWS;
                MoveImage2(&restoreRect, STREAM_MOVIE_SAVED_IMAGE_X, 0);
                restoreRect.x = STREAM_MOVIE_IMAGE_BACKUP_SECOND_X;
                restoreRect.y = 0;
                restoreRect.w = STREAM_MOVIE_SAVED_IMAGE_WORDS;
                restoreRect.h = STREAM_MOVIE_SAVED_IMAGE_ROWS;
                MoveImage2(&restoreRect, STREAM_MOVIE_SAVED_IMAGE_X, STREAM_MOVIE_SAVED_IMAGE_ROWS);
            }
            memConfigureImageMemory(gGameSession->location.loc.stage, gGameSession->location.loc.area);
            if ((selectConfiguredAuxHeap & 0xFFFF) == true) {
                memSelectAuxHeapRegion(true);
            }
            tmdResetAuxHeapAndRestoreBuffers();
            if (gDisplayState.videoMode == DISPLAY_VIDEO_STREAMING) {
                cdCmdPrepareViewMovie();
                cdCmdSelectMovieWorkspace();
            }
            D_8006AC28 = D_8006AC28 + 1;
            if (reloadSpriteImages & 0xFFFF) {
                session        = gGameSession;
                stageId        = session->location.loc.stage;
                fileKey[3]     = stageId;
                areaId         = session->location.loc.area;
                fileKey[0]     = 0;
                fileKey[2]     = areaId;
                spriteVariant  = session->spriteVariant;
                loadOptions[1] = CD_COMMAND_LOAD_IMAGES_ONLY;
                loadOptions[2] = 0;
                loadOptions[3] = 0;
                loadOptions[0] = spriteVariant;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadOptions);
                break;
            }
            return 1;
        case STREAM_GAME_RESTORE_WAIT_IMAGES:
            if (cdCmdIsIdle() & 0xFFFF) {
                queue->blockGamePause = false;
                D_8006AC28            = D_8006AC28 + 1;
                return 1;
            }
            break;
        case STREAM_GAME_RESTORE_COMPLETE:
            return 1;
    }
    return 0;
}

/// Clears both 320x240 movie framebuffers, using 480 VRAM words per row for RGB24.
///
/// Borrows `clearRect` as writable scratch; it ends describing the lower buffer.
static __inline__ void _streamClearDisplayBuffers(RECT* clearRect)
{
    clearRect->y = 0;
    clearRect->x = 0;
    if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
        clearRect->w = STREAM_MOVIE_RGB24_DISPLAY_WORDS;
    } else {
        clearRect->w = FILE_SYSTEM_IMAGE_WIDTH;
    }
    clearRect->h = FILE_SYSTEM_IMAGE_HEIGHT;
    ClearImage(clearRect, 0, 0, 0);
    clearRect->y = STREAM_MOVIE_FRAMEBUFFER_STRIDE_ROWS;
    ClearImage(clearRect, 0, 0, 0);
}

u32 Stream_InitializePlayback(u32 slotIndex)
{
    u8          params[8];
    RECT        clearRect;
    CdCmdQueue* queue;
    u32         slot;

    slot                     = slotIndex & 0xFFFF;
    queue                    = &gCdCmdQueue;
    queue->movieFrameSubstep = 0;
    queue->movieStep         = CD_COMMAND_MOVIE_WAIT_READY;
    queue->movieAtEnd        = 0;
    queue->movieFrameChanged = 0;
    _streamLoadMovieSlotState(slot);
    if (D_8006AC58 != 0) {
        if (D_8006AC30.startSector == 0) {
            return 1U;
        }
        D_8006AC08 = Stream_Slots[slot].source.interSectorOffset + D_8006AC30.startSector;
    }
    if (D_8006AC14 != STREAM_MOVIE_DISPLAY_TEXTURE) {
        params[3] = 0xFF;
        Mdec_SetupBuffers(params);
        queue->movieFrame = 1;
        _streamClearDisplayBuffers(&clearRect);
        if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
            displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_RGB24 | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
        } else {
            displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
        }
        gDisplayState.mdecActive = 1;
        DecDCTvlcBuild(D_8006AC38);
        return 0U;
    }
    Mdec_SetupBuffers((u8*)&gGameSession->location.loc);
    return 0U;
}

s32 CdCmd_StopMdec(s32 arg0)
{
    RECT        rect;
    s32         ac14;
    s32         f12a;
    CdCmdQueue* p;

    if (cdSyncPollPause() != 0) {
        p = &gCdCmdQueue;
        DecDCToutCallback(0);
        DecDCTReset(0);
        StClearRing();
        StUnSetRing();
        ac14                           = D_8006AC14;
        Wip_SysFlags.movieStreamActive = 0;
        p->field_24A                   = 0;
        p->movieReady                  = 0;
        p->movieFrameChanged           = 0;
        p->field_1E2                   = 0;
        p->movieStep                   = CD_COMMAND_MOVIE_WAIT_READY;
        if (ac14 != STREAM_MOVIE_DISPLAY_TEXTURE) {
            f12a = gDisplayState.videoMode;
            if (f12a == 1) {
                if (arg0 & 0xFFFF) {
                    rect.y = 0;
                    rect.x = 0;
                    if (ac14 == f12a) {
                        rect.w = 0x1E0;
                    } else {
                        rect.w = 0x140;
                    }
                    rect.h = 0xF0;
                    ClearImage(&rect, 0, 0, 0);
                    rect.y = 0x110;
                    ClearImage(&rect, 0, 0, 0);
                }
                displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
            }
            p->movieFrameAvailable   = 0;
            gDisplayState.mdecActive = 0;
        } else if (D_8006AC3C != 0) {
            p->blockGamePause = 0;
        }
        return 1;
    }
    return 0;
}

/// Publishes the clamped STR movie position and its forward or reverse scene frame.
static __inline__ void _streamPublishMovieFrame(CdCmdQueue* queue, CdlLOC* backLocation)
{
    enum { STREAM_MOVIE_FIRST_FRAME = 1 };
    s16 receivedFrameMinusOne;
    s16 movieFrame;

    receivedFrameMinusOne = StGetBackloc(backLocation) - 1;
    movieFrame            = receivedFrameMinusOne;
    if (receivedFrameMinusOne <= 0) {
        movieFrame = STREAM_MOVIE_FIRST_FRAME;
    }
    if ((s16)D_8006AC0C < movieFrame) {
        movieFrame = (s16)D_8006AC0C;
    }
    if (queue->movieFrame != movieFrame) {
        queue->movieFrame        = (u16)movieFrame;
        queue->movieFrameChanged = true;
        queue->movieReady        = true;
    }
    if (queue->reverseSceneFrames == 0) {
        queue->sceneFrame = (u16)movieFrame;
    } else {
        queue->sceneFrame = (D_8006AC0C - movieFrame) + 1;
    }
}

/// Completes a movie frame, publishing texture output or uploading the final display column.
///
/// Updates one-based movie/scene frame counters from the STR ring and clears
/// the pending-output latch. Display movies switch framebuffer after the final
/// column; staging uploads use the current output-buffer selector before it flips.
static void _streamCompleteDecodedFrame(void)
{
    CdlLOC      backLocation;
    RECT        uploadRect;
    s32         stripWords;
    s16         stagingStripWords;
    s32         stripIndex;
    s32         stripBytes;
    u16         stagingStripIndex;
    s32         stripX;
    s32         nextStripCount;
    u16         vramX;
    u16         stagingX, stagingY;
    RECT*       stagingRect;
    CdCmdQueue* queue;
    u16         stripY;
    u_long*     stripPixels;
    u32         widthPixels;

    // Clamp the ring's frame position and publish the scene traversal frame.
    queue = &gCdCmdQueue;
    _streamPublishMovieFrame(queue, &backLocation);
    queue->mdecOutputPending = false;
    if (D_8006AC14 == STREAM_MOVIE_DISPLAY_TEXTURE) {
        queue->movieFrameAvailable = true;
    } else {
        nextStripCount = D_8006AC1C + 1;
        D_8006AC1C     = nextStripCount;
        stripIndex     = (nextStripCount & 0xFFFF) - 1;
        vramX          = D_8006AC0E;
        if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
            stripX = vramX + stripIndex * STREAM_MOVIE_RGB24_STRIP_WORDS;
        } else {
            stripX = vramX + stripIndex * FILE_SYSTEM_IMAGE_STRIP_WIDTH;
        }
        stripY       = D_8006AC10;
        uploadRect.x = stripX;
        if (gDisplayState.frameBuffer != 0) {
            stripY += STREAM_MOVIE_FRAMEBUFFER_STRIDE_ROWS;
        }
        uploadRect.y = stripY;
        stripWords   = FILE_SYSTEM_IMAGE_STRIP_WIDTH;
        if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
            stripWords = STREAM_MOVIE_RGB24_STRIP_WORDS;
        }
        uploadRect.h = D_8006AC6C;
        uploadRect.w = stripWords;
        LoadImage(&uploadRect, D_8006AC48[D_8005EAEE ^ 1]);
        gDisplayState.frameBuffer ^= 1;
    }
    stagingStripWords = FILE_SYSTEM_IMAGE_STRIP_WIDTH;
    // A staged texture frame is laid out as successive full-height columns.
    if (queue->movieVramStaging != 0) {
        stagingX    = queue->movieStagingX;
        stagingY    = queue->movieStagingY;
        stagingRect = &uploadRect;
        if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
            stagingStripWords = STREAM_MOVIE_RGB24_STRIP_WORDS;
        }
        stagingRect->y = stagingY;
        stagingRect->w = stagingStripWords;
        stagingRect->h = D_8006AC6C;
        uploadRect.x   = stagingX;
        stripPixels    = D_8006AC48[D_8005EAEE];
        stripBytes     = stagingStripWords * D_8006AC6C * 2;
        widthPixels    = D_8006AC5A;
        for (stagingStripIndex = 0; (u32)stagingStripIndex < (widthPixels / FILE_SYSTEM_IMAGE_STRIP_WIDTH); stagingStripIndex++) {
            LoadImage(stagingRect, stripPixels);
            stagingRect->x += stagingStripWords;
            stripPixels     = (u_long*)((u8*)stripPixels + stripBytes);
        }
    }
    D_8006AC1C  = 0;
    D_8005EAEE ^= 1;
}

/// Handles one MDEC output completion, requesting the next column or completing the frame.
///
/// Display output alternates output slots as finished columns reach the GPU;
/// the configured slots may alias. Texture output completes in one transfer.
/// Display width must be a positive multiple of 16 pixels; output sizes are 32-bit words.
static void _mdecMovieOutputCallback(void)
{
    RECT     uploadRect;
    s32      nextStripCount;
    s32      stripIndex;
    s32      stripX;
    u16      stripY;
    u16      vramX;
    s32      stripWords;
    u16      heightRows;
    s32      outputWords;
    u_long** outputSlot;

    if (D_8006AC14 != STREAM_MOVIE_DISPLAY_TEXTURE) {
        // Service the deferred CD interrupt before chaining an RGB24 output.
        if ((D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) && (StCdIntrFlag != 0)) {
            StCdInterrupt();
            StCdIntrFlag = 0;
        }
        if (D_8006AC1C != ((D_8006AC5A / FILE_SYSTEM_IMAGE_STRIP_WIDTH) - 1)) {
            nextStripCount = D_8006AC1C + 1;
            D_8006AC1C     = nextStripCount;
            stripIndex     = (nextStripCount & 0xFFFF) - 1;
            vramX          = D_8006AC0E;
            if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
                stripX = vramX + stripIndex * STREAM_MOVIE_RGB24_STRIP_WORDS;
            } else {
                stripX = vramX + stripIndex * FILE_SYSTEM_IMAGE_STRIP_WIDTH;
            }
            stripY       = D_8006AC10;
            uploadRect.x = stripX;
            if (gDisplayState.frameBuffer != 0) {
                stripY += STREAM_MOVIE_FRAMEBUFFER_STRIDE_ROWS;
            }
            stripWords   = FILE_SYSTEM_IMAGE_STRIP_WIDTH;
            uploadRect.y = stripY;
            if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
                stripWords = STREAM_MOVIE_RGB24_STRIP_WORDS;
            }
            uploadRect.h = D_8006AC6C;
            uploadRect.w = stripWords;
            LoadImage(&uploadRect, D_8006AC48[D_8005EAEE ^ 1]);
            D_8005EAEE ^= 1;
            outputSlot  = &D_8006AC48[D_8005EAEE ^ 1];
            heightRows  = D_8006AC6C;
            if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
                outputWords = heightRows * STREAM_MOVIE_RGB24_OUTPUT_WORDS_PER_ROW;
            } else {
                outputWords = heightRows * STREAM_MOVIE_RGB16_OUTPUT_WORDS_PER_ROW;
            }
            DecDCTout(*outputSlot, outputWords);
            return;
        }
    }
    _streamCompleteDecodedFrame();
}

/// Submits a VLC-expanded movie frame to MDEC and starts its first output transfer.
///
/// Releases the compressed frame's ring storage before feeding the expanded
/// buffer. Display movies output one 16-pixel column; texture movies output the
/// full RGB16 frame. Output sizes are 32-bit words, not bytes. Buffer selectors
/// must be 0 or 1 and their workspaces must outlive the pending DMA transfers.
static void _mdecStartMovieFrameOutput(void)
{
    CdCmdQueue* queue;
    s32         outputWords;
    u_long**    outputBuffers;
    u_long**    outputSlot;
    u16         stripHeightRows;
    s32         frameHeightRows;

    queue = &gCdCmdQueue;
    StFreeRing(D_8006AC68);
    DecDCTin(D_8006AC50[D_8005EAEC], D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB16 ? MDEC_IMAGE_MODE_RGB16 : D_8006AC14);
    if (D_8006AC14 != STREAM_MOVIE_DISPLAY_TEXTURE) {
        outputBuffers   = D_8006AC48;
        outputSlot      = &outputBuffers[D_8005EAEE ^ 1];
        stripHeightRows = D_8006AC6C;
        if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
            outputWords = stripHeightRows * STREAM_MOVIE_RGB24_OUTPUT_WORDS_PER_ROW;
        } else {
            outputWords = stripHeightRows * STREAM_MOVIE_RGB16_OUTPUT_WORDS_PER_ROW;
        }
        DecDCTout(*outputSlot, outputWords);
    } else {
        frameHeightRows = D_8006AC6C;
        outputWords     = (D_8006AC5A * frameHeightRows) / 2;
        outputBuffers   = D_8006AC48;
        DecDCTout(outputBuffers[D_8005EAEE ^ 1], outputWords);
    }
    queue->mdecOutputPending = true;
    D_8006AC1A               = false;
    D_8005EAEC              ^= 1;
}

static void Mdec_DecodeFrame(void)
{
    StHEADER*   header;
    u_long*     headerWords;
    CdCmdQueue* p;

    p = &gCdCmdQueue;
    if (D_8006AC1A != 0) {
        if (DecDCTvlc2(NULL, NULL, D_8006AC38) == 0) {
            _mdecStartMovieFrameOutput();
        }
        return;
    }

    if (StGetNext(&D_8006AC68, &headerWords) != 0) {
        return;
    }

    header = (StHEADER*)headerWords;
    if (header->frameCount >= (u32)D_8006AC0C) {
        CdVol_ApplyFromTable(0);
        p->movieAtEnd = 1;
        p->movieStep  = CD_COMMAND_MOVIE_PAUSE;
    }

    p->cdOperationPending = 0;
    if (D_8006AC12 != 0) {
        DecDCTvlcSize2(0);
    } else {
        DecDCTvlcSize2(DecDCTBufSize(D_8006AC68) / 2 + 2);
    }

    if (DecDCTvlc2(D_8006AC68, D_8006AC50[D_8005EAEC], D_8006AC38) == 0) {
        _mdecStartMovieFrameOutput();
    } else {
        D_8006AC1A = 1;
    }

    if (p->continueMovie == 0) {
        p->movieStep = CD_COMMAND_MOVIE_PAUSE;
    }
}

static __inline__ u16 Stream_SeekPosition(CdlLOC* loc)
{
    if (cdSyncPollLogicalSeek(loc, 0) != 0) {
        return 1;
    }
    return 0;
}

/* Resets the decoder and the stream ring, routes decoded slices to the upload
 * callback and applies CD volume table entry 0 ahead of a streaming read. */
static __inline__ void _streamStartDecode(void)
{
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    DecDCTReset(0);
    StSetStream(D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB16 ? STREAM_MOVIE_DISPLAY_TEXTURE : D_8006AC14, 0, -1, NULL, NULL);
    StSetRing((u_long*)D_8006AC60, D_8006AC24);
    StClearRing();
    Wip_SysFlags.movieStreamActive = 1;
    DecDCToutCallback(_mdecMovieOutputCallback);
    CdVol_ApplyFromTable(0);
    queue->mdecOutputPending = 0;
    D_8006AC1A               = 0;
}

/// Starts a double-speed STR read, enabling XA ADPCM for a nonzero volume-table index.
///
/// Requires a completed CD seek and the cached movie volume selection. Returns the
/// SDK read-start result (zero means failure); it does not wait for a frame.
static __inline__ s32 _streamStartRead(void)
{
    if (D_8006AC58 != 0) {
        return CdRead2(CdlModeStream2 | CdlModeSpeed | CdlModeRT);
    }
    return CdRead2(CdlModeStream2 | CdlModeSpeed);
}

s32 Stream_PollPlayback(u16 resume, s32 sectorOffset)
{
    union {
        RECT   rect;
        CdlLOC location;
    } scratch;
    CdCmdQueue* state;
    CdCmdQueue* stop;
    s32         sector;
    u16         ready;
    s32         videoMode;
    s32         displayMode;

    state = &gCdCmdQueue;
    switch (state->movieStep) {
        case CD_COMMAND_MOVIE_WAIT_READY:
            if (D_8006AC5C == 0) {
                state->releasePauseBlockAfterFade = 0;
                state->blockGamePause             = 1;
            }
            state->movieDiskRecoveryActive = 0;
            switch (cdSyncPollCommand(0, 0)) {
                case CD_SYNC_PENDING:
                    break;
                case CD_SYNC_RETRY:
                    CdFlush();
                case CD_SYNC_COMPLETE:
                    state->movieStep++;
                    break;
            }
            break;
        case CD_COMMAND_MOVIE_INIT:
            _streamStartDecode();
            state->seekStep = CD_COMMAND_SEEK_SET_LOCATION;
            state->movieStep++;
        case CD_COMMAND_MOVIE_SEEK_START:
            if ((resume & 0xFFFF) == 0) {
                sector = D_8006AC08 + sectorOffset;
            } else {
                sector = sectorOffset;
            }
            state->cdOperationPending = 1;
            state->movieReadPending   = 1;
            CdIntToPos(sector, &scratch.location);
            ready = Stream_SeekPosition(&scratch.location);
            if (ready & 0xFFFF) {
                CdVol_ApplyFromTable((u8)D_8006AC58);
                if (!(_streamStartRead() & 0xFFFF)) {
                    D_8006AC20       = 1;
                    state->movieStep = CD_COMMAND_MOVIE_RETRY_READ;
                    return 0;
                }
                state->cdOperationPending = 0;
                state->movieReadPending   = 0;
                state->movieStep++;
                if (D_8006AC5C != 0) {
                    cdCmdClearBusy();
                }
            }
            break;
        case CD_COMMAND_MOVIE_DECODE:
            Mdec_DecodeFrame();
            if (cdSyncHasShellOpenStatus() != 0) {
                state->movieDiskRecoveryActive = 1;
                cdCmdSetBusy();
                state->movieStep = CD_COMMAND_MOVIE_RECOVER;
            }
            break;
        case CD_COMMAND_MOVIE_PAUSE:
            Mdec_DecodeFrame();
            state->cdOperationPending = 1;
            if (cdSyncPollPause() != 0) {
                state->movieStep++;
            }
            break;
        case CD_COMMAND_MOVIE_FINISH:
            if (D_8006AC16 == STREAM_MOVIE_REPEAT) {
                cdCmdSetBusy();
                state->movieFrame = 1;
                state->movieStep  = CD_COMMAND_MOVIE_INIT;
                return 0;
            }
            state->cdOperationPending = 0;
            stop                      = &gCdCmdQueue;
            DecDCToutCallback(NULL);
            DecDCTReset(0);
            StClearRing();
            StUnSetRing();
            videoMode                      = D_8006AC14;
            Wip_SysFlags.movieStreamActive = 0;
            stop->field_24A                = 0;
            stop->movieReady               = 0;
            stop->movieFrameChanged        = 0;
            stop->field_1E2                = 0;
            stop->movieStep                = CD_COMMAND_MOVIE_WAIT_READY;
            if (videoMode != 0) {
                displayMode = gDisplayState.videoMode;
                if (displayMode == 1) {
                    scratch.rect.y = 0;
                    scratch.rect.x = 0;
                    if (videoMode == displayMode) {
                        scratch.rect.w = 0x1E0;
                    } else {
                        scratch.rect.w = 0x140;
                    }
                    scratch.rect.h = 0xF0;
                    ClearImage(&scratch.rect, 0, 0, 0);
                    scratch.rect.y = 0x110;
                    ClearImage(&scratch.rect, 0, 0, 0);
                    displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
                }
                stop->movieFrameAvailable = 0;
                gDisplayState.mdecActive  = 0;
            } else if (D_8006AC3C != 0) {
                stop->blockGamePause = 0;
            }
            return 1;
        case CD_COMMAND_MOVIE_RECOVER:
            if (cdSyncPollDiscRecovery() != 0) {
                state->movieStep++;
            }
            break;
        case CD_COMMAND_MOVIE_STOP_RETRY:
            if ((s16)CdCmd_StopMdec(0) != 0) {
                if (D_8006AC14 != STREAM_MOVIE_DISPLAY_TEXTURE) {
                    if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
                        displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_RGB24 | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
                    }
                    gDisplayState.mdecActive = 1;
                }
                state->cdOperationPending = 1;
                state->movieStep          = CD_COMMAND_MOVIE_SEEK_RESUME;
            }
            break;
        case CD_COMMAND_MOVIE_SEEK_RESUME:
            CdIntToPos(D_8006AC08 + (state->movieFrame - 1) * 10, &scratch.location);
            ready = Stream_SeekPosition(&scratch.location);
            if (ready & 0xFFFF) {
                if (!(_streamStartRead() & 0xFFFF)) {
                    D_8006AC20       = 8;
                    state->movieStep = CD_COMMAND_MOVIE_RETRY_READ;
                    return 0;
                }
                _streamStartDecode();
                CdVol_ApplyFromTable((u8)D_8006AC58);
                state->movieDiskRecoveryActive = 0;
                state->cdOperationPending      = 0;
                state->movieStep               = CD_COMMAND_MOVIE_DECODE;
            }
            break;
        case CD_COMMAND_MOVIE_RETRY_READ:
            if ((s16)CdCmd_StopMdec(0) != 0) {
                state->movieStep = D_8006AC20;
            }
            break;
    }
    return 0;
}

/// Uploads a completed RAM movie frame's columns to a fixed or draw-buffer VRAM origin.
///
/// Borrows `uploadRect` as mutable scratch. Only low halfwords of coordinates
/// reach VRAM; nonzero `offsetDrawBuffer` adds the current draw-buffer Y offset.
/// The pixel width must be a positive multiple of 16 and all columns must fit
/// the buffer and destination. Each column is 16/24 VRAM words wide in RGB16/24,
/// so its byte stride is width-in-words times height-in-rows times two.
static __inline__ void _streamUploadFrameStrips(RECT* uploadRect, u32 vramX, u32 vramY, u16 offsetDrawBuffer)
{
    s16 stripWords;
    s32 drawBufferY;
    s32 stripBytes;
    u16 stripIndex;
    u8* stripPixels;
    u32 widthPixels;

    stripWords = FILE_SYSTEM_IMAGE_STRIP_WIDTH;
    if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
        stripWords = STREAM_MOVIE_RGB24_STRIP_WORDS;
    }
    if (offsetDrawBuffer) {
        drawBufferY = vramY & 0xFFFF;
        if (gDisplayState.drawBuffer != 0) {
            drawBufferY += STREAM_MOVIE_FRAMEBUFFER_STRIDE_ROWS;
        }
        uploadRect->y = drawBufferY;
    } else {
        uploadRect->y = vramY;
    }
    uploadRect->w = stripWords;
    uploadRect->x = vramX;
    uploadRect->h = D_8006AC6C;
    stripBytes    = stripWords * D_8006AC6C * 2;
    stripPixels   = (u8*)D_8006AC48[D_8005EAEE];
    widthPixels   = D_8006AC5A;
    for (stripIndex = 0; (u32)stripIndex < (widthPixels / FILE_SYSTEM_IMAGE_STRIP_WIDTH); stripIndex++) {
        LoadImage(uploadRect, (u_long*)stripPixels);
        uploadRect->x = (u16)uploadRect->x + stripWords;
        stripPixels  += stripBytes;
    }
}

void streamPresentMovieFrame(void)
{
    enum {
        STREAM_MOVIE_PRESENT_NEW_FRAME    = 0,
        STREAM_MOVIE_PRESENT_REPEAT_FRAME = 1,
    };
    RECT        presentRect;
    s32         drawBufferY;
    CdCmdQueue* queue;
    s32         offsetDrawBuffer;
    s32         vramX;
    s32         vramY;

    queue = &gCdCmdQueue;
    if ((queue->suppressMoviePresentation == 0) && (queue->movieFrameAvailable != 0)) {
        // Repeated presentations advance the scene substep but retain the frame.
        if (queue->movieFrame == D_8006AC0C) {
            queue->movieFrameSubstep = STREAM_MOVIE_PRESENT_NEW_FRAME;
        } else if (queue->movieFrameChanged != 0) {
            queue->movieFrameChanged = false;
            queue->movieFrameSubstep = STREAM_MOVIE_PRESENT_NEW_FRAME;
        } else {
            queue->movieFrameSubstep = STREAM_MOVIE_PRESENT_REPEAT_FRAME;
        }
        // Present from RAM columns or from the already uploaded staging rectangle.
        if (queue->movieVramStaging == 0) {
            offsetDrawBuffer = D_8006AC18 != STREAM_MOVIE_UPLOAD_FIXED_VRAM;
            vramX            = D_8006AC0E;
            vramY            = D_8006AC10;
            _streamUploadFrameStrips(&presentRect, vramX, vramY, offsetDrawBuffer);
        } else {
            presentRect.x = queue->movieStagingX;
            presentRect.y = queue->movieStagingY;
            presentRect.w = D_8006AC5A;
            presentRect.h = D_8006AC6C;
            if (D_8006AC18 == STREAM_MOVIE_UPLOAD_FIXED_VRAM) {
                drawBufferY = 0;
            } else {
                drawBufferY = gDisplayState.drawBuffer != 0 ? STREAM_MOVIE_FRAMEBUFFER_STRIDE_ROWS : 0;
            }
            MoveImage(&presentRect, D_8006AC0E, drawBufferY + D_8006AC10);
        }
    }
}

StreamSlot* streamGetSlot(u16 slotIndex)
{
    return &Stream_Slots[slotIndex];
}

void streamPrepareMovieWorkspace(s16 preserveVramImages)
{
    RECT        backupRect;
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    // Retire graphics allocations before turning the whole region into decoder storage.
    gpuResetAndInvalidateModelBuffers();
    memSelectAuxHeapRegion(false);
    memInitAuxHeap();
    if (gDisplayState.videoMode == DISPLAY_VIDEO_NORMAL) {
        D_8006AC40 = memMalloc(STREAM_MOVIE_WORKSPACE_NORMAL_BYTES, true);
    } else {
        D_8006AC40 = memMalloc(STREAM_MOVIE_WORKSPACE_STREAMING_BYTES, true);
    }
    if ((preserveVramImages & 0xFFFF) != 0) {
        backupRect.x = STREAM_MOVIE_SAVED_IMAGE_X;
        backupRect.y = 0;
        backupRect.w = STREAM_MOVIE_SAVED_IMAGE_WORDS;
        backupRect.h = STREAM_MOVIE_SAVED_IMAGE_ROWS;
        MoveImage2(&backupRect, STREAM_MOVIE_IMAGE_BACKUP_FIRST_X, 0);
        backupRect.x = STREAM_MOVIE_SAVED_IMAGE_X;
        backupRect.y = STREAM_MOVIE_SAVED_IMAGE_ROWS;
        backupRect.w = STREAM_MOVIE_SAVED_IMAGE_WORDS;
        backupRect.h = STREAM_MOVIE_SAVED_IMAGE_ROWS;
        MoveImage2(&backupRect, STREAM_MOVIE_IMAGE_BACKUP_SECOND_X, 0);
    }
    D_8006AC1E            = preserveVramImages;
    queue->blockGamePause = true;
}

void streamResetGameRestore(void)
{
    D_8006AC28 = STREAM_GAME_RESTORE_MEMORY;
}

s16 streamHasLoadedViewMovie(void* unusedLocation)
{
    enum { STREAM_VIEW_MOVIE_ID_LIMIT = 100U };
    s32 slotIndex;
    s32 hasLoadedMovie;

    slotIndex      = 0;
    hasLoadedMovie = 0;
    while (1) {
        if (Stream_Slots[slotIndex & 0xFFFF].kind == STREAM_KIND_MOVIE) {
            if (Stream_Slots[slotIndex & 0xFFFF].key.parts.id < (u32)STREAM_VIEW_MOVIE_ID_LIMIT) {
                if (Stream_Slots[slotIndex & 0xFFFF].startSector != 0) {
                    hasLoadedMovie = 1;
                    break;
                }
            }
        }
        slotIndex = slotIndex + 1;
        if ((u32)(slotIndex & 0xFFFF) >= ARRAY_SIZE(Stream_Slots)) {
            break;
        }
    }
    return hasLoadedMovie;
}

u16 streamGetFrameLimit(u16 slotIndex)
{
    return Stream_Slots[slotIndex].data.movie.frameLimit;
}

void Stream_KickDecode(u32 arg0)
{
    Stream_InitializePlayback(arg0 & 0xFFFF);
}
