#include "fs.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libcd.h>
#include <psyq/libpress.h>

#include "types.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/areas.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream_types.h"
#include "main/tmd.h"
#include "main/gamemain.h"

#include "mapui/map_akropolis.h"
#include "mapui/map_dryfield.h"
#include "mapui/map_neo_ark.h"

/// Absolute 2048-byte CD sector used as the movie seek/recovery base.
///
/// Starts at the descriptor's CDF sector; INTER movies replace it with the
/// disc container's first sector plus the descriptor's relative sector offset.
static s32 _gStreamMovieSeekSector;

/// One-based movie stop frame; 1..32767 is required by the signed publication clamp.
///
/// Stored unsigned for the STR-header stop test and reverse scene-frame count.
static u16 _gStreamMovieFrameLimit;

/// Movie presentation origin X in 16-bit VRAM words.
static u16 _gStreamMovieVramX;

/// Movie presentation origin Y in VRAM rows, before any framebuffer offset.
static u16 _gStreamMovieVramY;

/// Nonzero removes the VLC expansion budget; movie setup always selects zero.
///
/// Zero selects half the compressed header's expanded-payload word count plus
/// two words per poll. The SDK budget is in 32-bit words, not halfwords.
static u16 _gMdecMovieVlcUnlimited;

/// Cached movie format (0 texture RGB16, 1 display RGB24, 2 display RGB16).
static u16 _gStreamMovieDisplayMode;

/// Cached completion policy (1 restart playback; all other values stop).
static u16 _gStreamMovieLoopMode;

/// Cached texture destination policy (1 fixed VRAM; otherwise draw-buffer Y offset).
static u16 _gStreamMovieUploadMode;

/// Nonzero while the borrowed compressed frame has an unfinished VLC expansion.
///
/// Resumed on later decode polls; cleared when expanded input is submitted or
/// the decoder is restarted. Retains the frame's ring storage until submission.
static u16 _gMdecMovieVlcPending;

/// Display columns uploaded so far in the current frame, reset on frame completion.
///
/// Columns are 16 pixels wide; the final callback sees width/16 - 1 here.
static u16 _gMdecMovieUploadedStripCount;

/// Nonzero when movie preparation saved the two displaced VRAM image regions.
///
/// Restoration copies those backups back before reconfiguring image memory.
static u16 _gStreamMovieVramImagesSaved;

/// Movie step restored after a read-start retry (1 initialize, 8 seek/resume).
static u16 _gStreamMovieRetryResumeStep;

/// STR ring capacity in 2048-byte sectors (32 texture, 40 display).
static s32 _gStreamMovieRingSectorCount;

/// Post-movie restore step (0 memory, 1 wait for images/CD idle, 2 complete).
static u16 _gStreamGameRestoreStep;

/* Define BSS before API headers to preserve first-declaration order. */

StreamInterFile gStreamInterFile;

/// Borrowed 0x11000-byte halfword VLC lookup table in the selected movie layout.
///
/// Uses actor buffer zero for texture movies and the movie workspace for display
/// movies. Must be built before expansion and remain intact throughout playback.
u_short* _gMdecMovieVlcTable;

u16 gStreamMovieReleasePauseOnStop;

void* gStreamMovieWorkspace;

u8* gStreamMovieBufferLayoutEnd;

u_long* gMdecMovieOutputBuffers[2];

u_long* gMdecMovieExpandedBuffers[2];

u16 gStreamMovieVolumeTableIndex;

u16 gStreamMovieWidthPixels;

u16 gStreamMovieBackgroundPlayback;

u_long* gStreamMovieRingBuffer;

/// Receives the active workspace base during buffer assignment; purpose unproven.
///
/// No code reads this copy. Retained to preserve its storage and setup write.
void* D_8006AC64;

/// Borrowed compressed movie frame retained while its VLC expansion is in progress.
///
/// The STR ring owns the storage; output submission releases it before feeding
/// expanded commands to MDEC. This pointer is stale after that release.
u_long* _gMdecMovieCompressedFrame;

u16 gStreamMovieHeightRows;

#include "main/stream.h"
#include "stream.h"

extern s32 StCdIntrFlag;

static void _streamAssignMovieBuffers(const GameLocationKey* location);

static void _streamLoadMovieSlotState(u32 slotIndex);

static inline void _streamClearDisplayBuffers(RECT* clearRect);

static void _streamCompleteDecodedFrame(void);

static void _mdecMovieOutputCallback(void);

static void _mdecStartMovieFrameOutput(void);

static void _mdecStepMovieDecode(void);

static inline u16 _streamPollMovieSeek(CdlLOC* location);

static inline void _streamStartDecode(void);

static inline s32 _streamStartRead(void);

static inline void _streamUploadFrameStrips(RECT* uploadRect, u32 vramX, u32 vramY, u16 offsetDrawBuffer);

/// VRAM layout and byte capacities reserved for resident movie playback.
enum {
    STREAM_MOVIE_SECTORS_PER_FRAME          = 10,
    STREAM_MOVIE_FIRST_FRAME                = 1,
    STREAM_MOVIE_TEXTURE_RING_SECTORS       = 32,
    STREAM_MOVIE_DISPLAY_RING_SECTORS       = 40,
    STREAM_MOVIE_TEXTURE_RING_BYTES         = STREAM_MOVIE_TEXTURE_RING_SECTORS * 2048,
    STREAM_MOVIE_DISPLAY_RING_BYTES         = STREAM_MOVIE_DISPLAY_RING_SECTORS * 2048,
    STREAM_MOVIE_RGB24_STRIP_BYTES_PER_ROW  = FILE_SYSTEM_IMAGE_STRIP_WIDTH * 3,
    STREAM_MOVIE_DISPLAY_LAYOUT_STAGE       = 0xFF,
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

/// Clears both 320x240 display-movie buffers using a caller-supplied RGB24 choice.
///
/// `clearRect` must be a writable, side-effect-free RECT lvalue: it is evaluated
/// repeatedly and ends describing the lower buffer. `rgb24` is tested once;
/// true uses 480 VRAM words per row, false uses 320. Both buffers must be owned
/// by movie playback until the clears are queued. X starts at zero and Y ends
/// at row 272; height is 240 rows. The two ClearImage calls queue GPU work,
/// without waiting for completion. Captures the TU's movie-layout constants
/// but no caller locals or configuration bindings.
#define STREAM_MOVIE_CLEAR_DISPLAY_FRAMEBUFFERS(clearRect, rgb24) \
    do {                                                          \
        (clearRect).y = 0;                                        \
        (clearRect).x = 0;                                        \
        if (rgb24) {                                              \
            (clearRect).w = STREAM_MOVIE_RGB24_DISPLAY_WORDS;     \
        } else {                                                  \
            (clearRect).w = FILE_SYSTEM_IMAGE_WIDTH;              \
        }                                                         \
        (clearRect).h = FILE_SYSTEM_IMAGE_HEIGHT;                 \
        ClearImage(&(clearRect), 0, 0, 0);                        \
        (clearRect).y = DISPLAY_BUFFER_STRIDE_ROWS;               \
        ClearImage(&(clearRect), 0, 0, 0);                        \
    } while (0)

/// Steps of the post-movie memory and optional sprite-image restoration.
enum {
    STREAM_GAME_RESTORE_MEMORY      = 0,
    STREAM_GAME_RESTORE_WAIT_IMAGES = 1,
    STREAM_GAME_RESTORE_COMPLETE    = 2,
};

u16 gMdecMovieInputSlot  = 0;
u16 gMdecMovieOutputSlot = 0;

/// Assigns borrowed STR, VLC-output and decoded-pixel buffers for a movie.
///
/// Movie dimensions must already be cached and the workspace reserved/aligned.
/// Reads only stage, plus area in the loaded map hook. Stage 255 selects display
/// storage: VLC table, 40-sector ring, one RGB24 strip and two width*height-byte
/// VLC buffers. Stage zero uses actor buffer 0 for VLC/ring/output and two
/// workspace pixel buffers separated by twice width*height bytes. Stages 1, 2
/// and 5 delegate placement to their loaded map overlay. Other stages retain
/// previous output pointers after the common reset; only supported layouts may
/// be used for decoding. Does not allocate, clear pixels or start the decoder.
/// Display storage must fit 0x11000 + 40*2048 + 48*height + 2*width*height bytes.
/// Stage-zero actor storage must fit 0x11000 + 32*2048 + 2*width*height bytes,
/// and its pixel workspace must fit 4*width*height bytes. Products must fit s32;
/// map-selected layouts require their own larger reservations where applicable.
/// Each VLC expansion must fit the reserved interval before the next buffer.
static void _streamAssignMovieBuffers(const GameLocationKey* location)
{
    s32     vlcBufferBytes;
    u_long* ringBuffer;
    u_long* decodedStrip;
    u_long* firstVlcBuffer;
    u_long* secondVlcBuffer;
    s32     textureVlcBufferBytes;
    u_long* textureVlcBuffer;

    // Default input storage comes from actor buffer zero; map hooks can replace the pixel layout.
    gStreamMovieBackgroundPlayback = false;
    gStreamMovieReleasePauseOnStop = true;
    gCdCmdQueue.field_24A          = 0;
    gCdCmdQueue.movieVramStaging   = 0;
    _gStreamMovieRingSectorCount   = STREAM_MOVIE_TEXTURE_RING_SECTORS;
    _gMdecMovieVlcTable            = (u16*)gFsActorBuffer0Base;
    gStreamMovieRingBuffer         = (u_long*)(gFsActorBuffer0Base + STREAM_VLC_TABLE_BYTES);
    D_8006AC64                     = gStreamMovieWorkspace;

    switch ((s8)(location->stage + 1)) {
        case (s8)(STREAM_MOVIE_DISPLAY_LAYOUT_STAGE + 1):
            // Pack table, ring, shared strip and both VLC outputs into the resident arena.
            _gStreamMovieRingSectorCount = STREAM_MOVIE_DISPLAY_RING_SECTORS;
            _gMdecMovieVlcTable          = gStreamMovieWorkspace;
            ringBuffer                   = (gStreamMovieRingBuffer = (u_long*)((u8*)gStreamMovieWorkspace + STREAM_VLC_TABLE_BYTES));
            {
                u16 heightRows               = gStreamMovieHeightRows;
                s32 stripBufferBytes         = heightRows * STREAM_MOVIE_RGB24_STRIP_BYTES_PER_ROW;
                vlcBufferBytes               = gStreamMovieWidthPixels * heightRows;
                gMdecMovieOutputBuffers[1]   = (decodedStrip = (u_long*)((u8*)ringBuffer + STREAM_MOVIE_DISPLAY_RING_BYTES));
                gMdecMovieOutputBuffers[0]   = decodedStrip;
                gMdecMovieExpandedBuffers[0] = (firstVlcBuffer = (u_long*)((u8*)decodedStrip + stripBufferBytes));
                gMdecMovieExpandedBuffers[1] = (secondVlcBuffer = (u_long*)((u8*)firstVlcBuffer + vlcBufferBytes));
                gStreamMovieBufferLayoutEnd  = (u8*)secondVlcBuffer + vlcBufferBytes;
            }
            return;
        case GAME_STAGE_NONE + 1: {
            u_long** vlcBuffers;
            u_long** decodedBuffers;
            void*    workspace;

            // Keep actor-based input separate from the two full-frame pixel buffers.
            textureVlcBufferBytes = gStreamMovieWidthPixels * gStreamMovieHeightRows;
            vlcBuffers            = gMdecMovieExpandedBuffers;
            textureVlcBuffer      = (u_long*)((u8*)gStreamMovieRingBuffer + STREAM_MOVIE_TEXTURE_RING_BYTES);
            workspace             = gStreamMovieWorkspace;
            decodedBuffers        = gMdecMovieOutputBuffers;
            vlcBuffers[0]         = textureVlcBuffer;
            decodedBuffers[0]     = workspace;
            vlcBuffers[1]         = (u_long*)((u8*)textureVlcBuffer + textureVlcBufferBytes);
            decodedBuffers[1]     = (u_long*)((u8*)workspace + (textureVlcBufferBytes * 2));
            return;
        }
        case GAME_STAGE_AKROPOLIS + 1:
            mapAkropolisSetupMovieBuffers(location);
            return;
        case GAME_STAGE_DRYFIELD + 1:
            mapDryfieldSetupMovieBuffers(location);
            return;
        case GAME_STAGE_SHELTER_NEO_ARK + 1:
            mapNeoArkSetupMovieBuffers(location);
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
    slotTable                             = gStreamSlots;
    _gMdecMovieVlcUnlimited               = false;
    movieSlot                             = &slotTable[slotIndex & 0xFFFF];
    _gStreamMovieSeekSector               = movieSlot->startSector;
    _gStreamMovieFrameLimit               = movieSlot->data.movie.frameLimit;
    gStreamMovieWidthPixels               = movieSlot->data.movie.width;
    gStreamMovieHeightRows                = movieSlot->data.movie.height;
    _gStreamMovieVramX                    = movieSlot->data.movie.vramX;
    _gStreamMovieVramY                    = movieSlot->data.movie.vramY;
    _gStreamMovieLoopMode                 = movieSlot->data.movie.loopMode;
    _gStreamMovieDisplayMode              = movieSlot->data.movie.displayMode;
    gStreamMovieVolumeTableIndex          = movieSlot->data.movie.volumeTableIndex;
    _gStreamMovieUploadMode               = movieSlot->data.movie.uploadMode;
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
    slotTable          = gStreamSlots;
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
                    } else if (slotTable[slotIndex & 0xFFFF].key.parts.group == location->roomState) {
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
    if ((u32)(slotIndex & 0xFFFF) < ARRAY_SIZE(gStreamSlots)) {
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
    slotTable = gStreamSlots;
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
                if (slotTable[slotIndex & 0xFFFF].key.parts.group == location->roomState) {
                    if (slotTable[slotIndex & 0xFFFF].data.movie.viewStream != 0) {
                        shiftedSlotIndex = slotIndex << 16;
                        return shiftedSlotIndex >> 16;
                    }
                }
            }
        }
        slotIndex = slotIndex + 1;
        if ((u32)(slotIndex & 0xFFFF) >= ARRAY_SIZE(gStreamSlots)) {
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
    restoreStep = _gStreamGameRestoreStep;
    switch (restoreStep) {
        case STREAM_GAME_RESTORE_MEMORY:
            // Recover displaced VRAM before reusing the movie's auxiliary storage.
            if (_gStreamMovieVramImagesSaved != 0) {
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
            memConfigureImageMemory(gGameSession->location.stage, gGameSession->location.area);
            if ((selectConfiguredAuxHeap & 0xFFFF) == true) {
                memSelectAuxHeapRegion(true);
            }
            tmdResetAuxHeapAndRestoreBuffers();
            if (gDisplayState.videoMode == DISPLAY_VIDEO_STREAMING) {
                cdCmdPrepareViewMovie();
                cdCmdSelectMovieWorkspace();
            }
            _gStreamGameRestoreStep = _gStreamGameRestoreStep + 1;
            if (reloadSpriteImages & 0xFFFF) {
                session        = gGameSession;
                stageId        = session->location.stage;
                fileKey[3]     = stageId;
                areaId         = session->location.area;
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
                queue->blockGamePause   = false;
                _gStreamGameRestoreStep = _gStreamGameRestoreStep + 1;
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
static inline void _streamClearDisplayBuffers(RECT* clearRect)
{
    clearRect->y = 0;
    clearRect->x = 0;
    if (_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB24) {
        clearRect->w = STREAM_MOVIE_RGB24_DISPLAY_WORDS;
    } else {
        clearRect->w = FILE_SYSTEM_IMAGE_WIDTH;
    }
    clearRect->h = FILE_SYSTEM_IMAGE_HEIGHT;
    ClearImage(clearRect, 0, 0, 0);
    clearRect->y = DISPLAY_BUFFER_STRIDE_ROWS;
    ClearImage(clearRect, 0, 0, 0);
}

u32 streamPrepareMoviePlayback(u32 slotIndex)
{
    GameLocationKey displayLocation;
    RECT            clearRect;
    CdCmdQueue*     queue;
    u32             movieSlotIndex;

    movieSlotIndex           = slotIndex & 0xFFFF;
    queue                    = &gCdCmdQueue;
    queue->movieFrameSubstep = 0;
    queue->movieStep         = CD_COMMAND_MOVIE_WAIT_READY;
    queue->movieAtEnd        = false;
    queue->movieFrameChanged = false;
    _streamLoadMovieSlotState(movieSlotIndex);
    if (gStreamMovieVolumeTableIndex != 0) {
        if (gStreamInterFile.startSector == 0) {
            return STREAM_MOVIE_SETUP_INTER_UNAVAILABLE;
        }
        _gStreamMovieSeekSector = gStreamSlots[movieSlotIndex].source.interSectorOffset + gStreamInterFile.startSector;
    }
    if (_gStreamMovieDisplayMode != STREAM_MOVIE_DISPLAY_TEXTURE) {
        // Display movies use a resident layout independently of the current map.
        displayLocation.stage = STREAM_MOVIE_DISPLAY_LAYOUT_STAGE;
        _streamAssignMovieBuffers(&displayLocation);
        queue->movieFrame = STREAM_MOVIE_FIRST_FRAME;
        _streamClearDisplayBuffers(&clearRect);
        if (_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB24) {
            displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_RGB24 | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
        } else {
            displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
        }
        gDisplayState.mdecActive = true;
        DecDCTvlcBuild(_gMdecMovieVlcTable);
        return STREAM_MOVIE_SETUP_COMPLETE;
    }
    _streamAssignMovieBuffers(&gGameSession->location);
    return STREAM_MOVIE_SETUP_COMPLETE;
}

s16 streamPollMovieStop(s32 clearFramebuffers)
{
    RECT        clearRect;
    s32         movieDisplayMode;
    s32         displayVideoMode;
    CdCmdQueue* queue;

    if (cdSyncPollPause() != 0) {
        // Stop DMA callbacks and detach the ring only after the drive has paused.
        queue = &gCdCmdQueue;
        DecDCToutCallback(NULL);
        DecDCTReset(0);
        StClearRing();
        StUnSetRing();
        movieDisplayMode                           = _gStreamMovieDisplayMode;
        gGameMainPersistentState.movieStreamActive = false;
        queue->field_24A                           = 0;
        queue->movieReady                          = false;
        queue->movieFrameChanged                   = false;
        queue->field_1E2                           = 0;
        queue->movieStep                           = CD_COMMAND_MOVIE_WAIT_READY;
        if (movieDisplayMode != STREAM_MOVIE_DISPLAY_TEXTURE) {
            displayVideoMode = gDisplayState.videoMode;
            if (displayVideoMode == DISPLAY_VIDEO_STREAMING) {
                if (clearFramebuffers & 0xFFFF) {
                    STREAM_MOVIE_CLEAR_DISPLAY_FRAMEBUFFERS(clearRect, movieDisplayMode == displayVideoMode);
                }
                displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
            }
            queue->movieFrameAvailable = false;
            gDisplayState.mdecActive   = false;
        } else if (gStreamMovieReleasePauseOnStop != 0) {
            queue->blockGamePause = false;
        }
        return 1;
    }
    return 0;
}

/// Publishes the STR ring's movie position and forward or reverse scene frame.
///
/// Borrows writable `backLocation` scratch for the SDK's returned CD position.
/// Subtracts one from the ring frame and narrows to s16 before clamping to
/// 1..frameLimit; the cached limit must be 1..32767. A changed movie frame sets
/// ready/change latches; an unchanged frame leaves them intact. The one-based
/// scene frame is refreshed on every call, reversing against the same limit.
static inline void _streamPublishMovieFrame(CdCmdQueue* queue, CdlLOC* backLocation)
{
    s16 receivedFrameMinusOne;
    s16 movieFrame;

    receivedFrameMinusOne = StGetBackloc(backLocation) - 1;
    movieFrame            = receivedFrameMinusOne;
    if (receivedFrameMinusOne <= 0) {
        movieFrame = STREAM_MOVIE_FIRST_FRAME;
    }
    if ((s16)_gStreamMovieFrameLimit < movieFrame) {
        movieFrame = (s16)_gStreamMovieFrameLimit;
    }
    if (queue->movieFrame != movieFrame) {
        queue->movieFrame        = (u16)movieFrame;
        queue->movieFrameChanged = true;
        queue->movieReady        = true;
    }
    if (queue->reverseSceneFrames == 0) {
        queue->sceneFrame = (u16)movieFrame;
    } else {
        queue->sceneFrame = (_gStreamMovieFrameLimit - movieFrame) + 1;
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
    if (_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_TEXTURE) {
        queue->movieFrameAvailable = true;
    } else {
        nextStripCount                = _gMdecMovieUploadedStripCount + 1;
        _gMdecMovieUploadedStripCount = nextStripCount;
        stripIndex                    = (nextStripCount & 0xFFFF) - 1;
        vramX                         = _gStreamMovieVramX;
        if (_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB24) {
            stripX = vramX + stripIndex * STREAM_MOVIE_RGB24_STRIP_WORDS;
        } else {
            stripX = vramX + stripIndex * FILE_SYSTEM_IMAGE_STRIP_WIDTH;
        }
        stripY       = _gStreamMovieVramY;
        uploadRect.x = stripX;
        if (gDisplayState.frameBuffer != 0) {
            stripY += DISPLAY_BUFFER_STRIDE_ROWS;
        }
        uploadRect.y = stripY;
        stripWords   = FILE_SYSTEM_IMAGE_STRIP_WIDTH;
        if (_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB24) {
            stripWords = STREAM_MOVIE_RGB24_STRIP_WORDS;
        }
        uploadRect.h = gStreamMovieHeightRows;
        uploadRect.w = stripWords;
        LoadImage(&uploadRect, gMdecMovieOutputBuffers[gMdecMovieOutputSlot ^ 1]);
        gDisplayState.frameBuffer ^= 1;
    }
    stagingStripWords = FILE_SYSTEM_IMAGE_STRIP_WIDTH;
    // A staged texture frame is laid out as successive full-height columns.
    if (queue->movieVramStaging != 0) {
        stagingX    = queue->movieStagingX;
        stagingY    = queue->movieStagingY;
        stagingRect = &uploadRect;
        if (_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB24) {
            stagingStripWords = STREAM_MOVIE_RGB24_STRIP_WORDS;
        }
        stagingRect->y = stagingY;
        stagingRect->w = stagingStripWords;
        stagingRect->h = gStreamMovieHeightRows;
        uploadRect.x   = stagingX;
        stripPixels    = gMdecMovieOutputBuffers[gMdecMovieOutputSlot];
        stripBytes     = stagingStripWords * gStreamMovieHeightRows * 2;
        widthPixels    = gStreamMovieWidthPixels;
        for (stagingStripIndex = 0; (u32)stagingStripIndex < (widthPixels / FILE_SYSTEM_IMAGE_STRIP_WIDTH); stagingStripIndex++) {
            LoadImage(stagingRect, stripPixels);
            stagingRect->x += stagingStripWords;
            stripPixels     = (u_long*)((u8*)stripPixels + stripBytes);
        }
    }
    _gMdecMovieUploadedStripCount = 0;
    gMdecMovieOutputSlot         ^= 1;
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

    if (_gStreamMovieDisplayMode != STREAM_MOVIE_DISPLAY_TEXTURE) {
        // Service the deferred CD interrupt before chaining an RGB24 output.
        if ((_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB24) && (StCdIntrFlag != 0)) {
            StCdInterrupt();
            StCdIntrFlag = 0;
        }
        if (_gMdecMovieUploadedStripCount != ((gStreamMovieWidthPixels / FILE_SYSTEM_IMAGE_STRIP_WIDTH) - 1)) {
            nextStripCount                = _gMdecMovieUploadedStripCount + 1;
            _gMdecMovieUploadedStripCount = nextStripCount;
            stripIndex                    = (nextStripCount & 0xFFFF) - 1;
            vramX                         = _gStreamMovieVramX;
            if (_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB24) {
                stripX = vramX + stripIndex * STREAM_MOVIE_RGB24_STRIP_WORDS;
            } else {
                stripX = vramX + stripIndex * FILE_SYSTEM_IMAGE_STRIP_WIDTH;
            }
            stripY       = _gStreamMovieVramY;
            uploadRect.x = stripX;
            if (gDisplayState.frameBuffer != 0) {
                stripY += DISPLAY_BUFFER_STRIDE_ROWS;
            }
            stripWords   = FILE_SYSTEM_IMAGE_STRIP_WIDTH;
            uploadRect.y = stripY;
            if (_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB24) {
                stripWords = STREAM_MOVIE_RGB24_STRIP_WORDS;
            }
            uploadRect.h = gStreamMovieHeightRows;
            uploadRect.w = stripWords;
            LoadImage(&uploadRect, gMdecMovieOutputBuffers[gMdecMovieOutputSlot ^ 1]);
            gMdecMovieOutputSlot ^= 1;
            outputSlot            = &gMdecMovieOutputBuffers[gMdecMovieOutputSlot ^ 1];
            heightRows            = gStreamMovieHeightRows;
            if (_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB24) {
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
    StFreeRing(_gMdecMovieCompressedFrame);
    DecDCTin(gMdecMovieExpandedBuffers[gMdecMovieInputSlot], _gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB16 ? MDEC_IMAGE_MODE_RGB16 : _gStreamMovieDisplayMode);
    if (_gStreamMovieDisplayMode != STREAM_MOVIE_DISPLAY_TEXTURE) {
        outputBuffers   = gMdecMovieOutputBuffers;
        outputSlot      = &outputBuffers[gMdecMovieOutputSlot ^ 1];
        stripHeightRows = gStreamMovieHeightRows;
        if (_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB24) {
            outputWords = stripHeightRows * STREAM_MOVIE_RGB24_OUTPUT_WORDS_PER_ROW;
        } else {
            outputWords = stripHeightRows * STREAM_MOVIE_RGB16_OUTPUT_WORDS_PER_ROW;
        }
        DecDCTout(*outputSlot, outputWords);
    } else {
        frameHeightRows = gStreamMovieHeightRows;
        outputWords     = (gStreamMovieWidthPixels * frameHeightRows) / 2;
        outputBuffers   = gMdecMovieOutputBuffers;
        DecDCTout(outputBuffers[gMdecMovieOutputSlot ^ 1], outputWords);
    }
    queue->mdecOutputPending = true;
    _gMdecMovieVlcPending    = false;
    gMdecMovieInputSlot     ^= 1;
}

/// Advances a retained VLC expansion or starts decoding the next complete STR frame.
///
/// Requires initialized ring, VLC and double-buffer workspaces, with selectors
/// in 0..1. The ring owns compressed storage until VLC expansion completes and
/// output submission releases it. A stop frame or withdrawn continue request
/// schedules pause while allowing the current expansion/output to finish.
static void _mdecStepMovieDecode(void)
{
    enum {
        MDEC_VLC_UNLIMITED_OUTPUT         = 0,
        MDEC_VLC_COMMAND_HEADER_HALFWORDS = 2,
    };
    StHEADER*   frameHeader;
    u_long*     frameHeaderWords;
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    // NULL inputs resume the SDK's sliced VLC expansion of the retained frame.
    if (_gMdecMovieVlcPending != 0) {
        if (DecDCTvlc2(NULL, NULL, _gMdecMovieVlcTable) == 0) {
            _mdecStartMovieFrameOutput();
        }
        return;
    }

    if (StGetNext(&_gMdecMovieCompressedFrame, &frameHeaderWords) != 0) {
        return;
    }

    // The SDK returns generic words for the STR header; its frame number sets the stop point.
    frameHeader = (StHEADER*)frameHeaderWords;
    if (frameHeader->frameCount >= _gStreamMovieFrameLimit) {
        cdVolApplyMoviePreset(0);
        queue->movieAtEnd = true;
        queue->movieStep  = CD_COMMAND_MOVIE_PAUSE;
    }

    queue->cdOperationPending = false;
    if (_gMdecMovieVlcUnlimited != 0) {
        DecDCTvlcSize2(MDEC_VLC_UNLIMITED_OUTPUT);
    } else {
        // Budget half the expanded-payload word count plus two 32-bit words.
        DecDCTvlcSize2(DecDCTBufSize(_gMdecMovieCompressedFrame) / 2 + MDEC_VLC_COMMAND_HEADER_HALFWORDS);
    }

    if (DecDCTvlc2(_gMdecMovieCompressedFrame, gMdecMovieExpandedBuffers[gMdecMovieInputSlot], _gMdecMovieVlcTable) == 0) {
        _mdecStartMovieFrameOutput();
    } else {
        _gMdecMovieVlcPending = true;
    }

    if (queue->continueMovie == 0) {
        queue->movieStep = CD_COMMAND_MOVIE_PAUSE;
    }
}

/// Polls the serialized movie seek to a borrowed BCD CD location.
///
/// Returns 0 while pending and 1 when complete; the seek state starts at
/// CD_COMMAND_SEEK_SET_LOCATION. Shares the resident drive recovery channel.
static inline u16 _streamPollMovieSeek(CdlLOC* location)
{
    if (cdSyncPollLogicalSeek(location, 0) != 0) {
        return 1;
    }
    return 0;
}

/// Initializes the movie decoder, ring delivery and output callback for a new read.
///
/// Requires a configured movie and word-aligned ring/VLC/output workspaces that
/// remain live through playback. RGB16 display output uses the SDK's RGB16
/// stream mode. Resets ring contents and pending VLC/output flags and applies
/// the silent CD volume preset; it neither seeks nor starts the CD read.
static inline void _streamStartDecode(void)
{
    enum {
        STREAM_MOVIE_RING_RGB16          = 0,
        STREAM_MOVIE_RING_NO_START_FRAME = 0,
        STREAM_MOVIE_RING_NO_END_FRAME   = -1,
    };
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    DecDCTReset(0);
    StSetStream(_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB16 ? STREAM_MOVIE_RING_RGB16 : _gStreamMovieDisplayMode, STREAM_MOVIE_RING_NO_START_FRAME, STREAM_MOVIE_RING_NO_END_FRAME, NULL, NULL);
    StSetRing(gStreamMovieRingBuffer, _gStreamMovieRingSectorCount);
    StClearRing();
    gGameMainPersistentState.movieStreamActive = true;
    DecDCToutCallback(_mdecMovieOutputCallback);
    cdVolApplyMoviePreset(0);
    queue->mdecOutputPending = false;
    _gMdecMovieVlcPending    = false;
}

/// Starts a double-speed STR read, enabling XA ADPCM for a nonzero volume-table index.
///
/// Requires a completed CD seek and the cached movie volume selection. Returns the
/// SDK read-start result (zero means failure); it does not wait for a frame.
static inline s32 _streamStartRead(void)
{
    if (gStreamMovieVolumeTableIndex != 0) {
        return CdRead2(CdlModeStream2 | CdlModeSpeed | CdlModeRT);
    }
    return CdRead2(CdlModeStream2 | CdlModeSpeed);
}

s32 streamPollMoviePlayback(u16 sectorIsAbsolute, s32 sectorOffset)
{
    union {
        RECT   rect;
        CdlLOC location;
    } scratch;
    CdCmdQueue* queue;
    CdCmdQueue* stopQueue;
    s32         absoluteSector;
    u16         seekComplete;
    s32         movieDisplayMode;
    s32         displayVideoMode;

    queue = &gCdCmdQueue;
    switch (queue->movieStep) {
        case CD_COMMAND_MOVIE_WAIT_READY:
            if (gStreamMovieBackgroundPlayback == 0) {
                queue->releasePauseBlockAfterFade = false;
                queue->blockGamePause             = true;
            }
            queue->movieDiskRecoveryActive = false;
            switch (cdSyncPollCommand(0, 0)) {
                case CD_SYNC_PENDING:
                    break;
                case CD_SYNC_RETRY:
                    CdFlush();
                case CD_SYNC_COMPLETE:
                    queue->movieStep++;
                    break;
            }
            break;
        case CD_COMMAND_MOVIE_INIT:
            _streamStartDecode();
            queue->seekStep = CD_COMMAND_SEEK_SET_LOCATION;
            queue->movieStep++;
        case CD_COMMAND_MOVIE_SEEK_START:
            if (sectorIsAbsolute == 0) {
                absoluteSector = _gStreamMovieSeekSector + sectorOffset;
            } else {
                absoluteSector = sectorOffset;
            }
            queue->cdOperationPending = true;
            queue->movieReadPending   = true;
            CdIntToPos(absoluteSector, &scratch.location);
            seekComplete = _streamPollMovieSeek(&scratch.location);
            if (seekComplete) {
                cdVolApplyMoviePreset((u8)gStreamMovieVolumeTableIndex);
                if (!(_streamStartRead() & 0xFFFF)) {
                    _gStreamMovieRetryResumeStep = CD_COMMAND_MOVIE_INIT;
                    queue->movieStep             = CD_COMMAND_MOVIE_RETRY_READ;
                    return 0;
                }
                queue->cdOperationPending = false;
                queue->movieReadPending   = false;
                queue->movieStep++;
                if (gStreamMovieBackgroundPlayback != 0) {
                    cdCmdClearBusy();
                }
            }
            break;
        case CD_COMMAND_MOVIE_DECODE:
            _mdecStepMovieDecode();
            if (cdSyncHasShellOpenStatus() != 0) {
                queue->movieDiskRecoveryActive = true;
                cdCmdSetBusy();
                queue->movieStep = CD_COMMAND_MOVIE_RECOVER;
            }
            break;
        case CD_COMMAND_MOVIE_PAUSE:
            _mdecStepMovieDecode();
            queue->cdOperationPending = true;
            if (cdSyncPollPause() != 0) {
                queue->movieStep++;
            }
            break;
        case CD_COMMAND_MOVIE_FINISH:
            if (_gStreamMovieLoopMode == STREAM_MOVIE_REPEAT) {
                cdCmdSetBusy();
                queue->movieFrame = STREAM_MOVIE_FIRST_FRAME;
                queue->movieStep  = CD_COMMAND_MOVIE_INIT;
                return 0;
            }
            // A final pause precedes decoder shutdown and restoration of the game display.
            queue->cdOperationPending = false;
            stopQueue                 = &gCdCmdQueue;
            DecDCToutCallback(NULL);
            DecDCTReset(0);
            StClearRing();
            StUnSetRing();
            movieDisplayMode                           = _gStreamMovieDisplayMode;
            gGameMainPersistentState.movieStreamActive = false;
            stopQueue->field_24A                       = 0;
            stopQueue->movieReady                      = false;
            stopQueue->movieFrameChanged               = false;
            stopQueue->field_1E2                       = 0;
            stopQueue->movieStep                       = CD_COMMAND_MOVIE_WAIT_READY;
            if (movieDisplayMode != STREAM_MOVIE_DISPLAY_TEXTURE) {
                displayVideoMode = gDisplayState.videoMode;
                if (displayVideoMode == DISPLAY_VIDEO_STREAMING) {
                    STREAM_MOVIE_CLEAR_DISPLAY_FRAMEBUFFERS(scratch.rect, movieDisplayMode == displayVideoMode);
                    displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
                }
                stopQueue->movieFrameAvailable = false;
                gDisplayState.mdecActive       = false;
            } else if (gStreamMovieReleasePauseOnStop != 0) {
                stopQueue->blockGamePause = false;
            }
            return 1;
        case CD_COMMAND_MOVIE_RECOVER:
            if (cdSyncPollDiscRecovery() != 0) {
                queue->movieStep++;
            }
            break;
        case CD_COMMAND_MOVIE_STOP_RETRY:
            if (streamPollMovieStop(0) != 0) {
                if (_gStreamMovieDisplayMode != STREAM_MOVIE_DISPLAY_TEXTURE) {
                    if (_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB24) {
                        displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_RGB24 | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
                    }
                    gDisplayState.mdecActive = true;
                }
                queue->cdOperationPending = true;
                queue->movieStep          = CD_COMMAND_MOVIE_SEEK_RESUME;
            }
            break;
        case CD_COMMAND_MOVIE_SEEK_RESUME:
            // Resume from the last published frame after recovering the disc.
            CdIntToPos(_gStreamMovieSeekSector + (queue->movieFrame - 1) * STREAM_MOVIE_SECTORS_PER_FRAME, &scratch.location);
            seekComplete = _streamPollMovieSeek(&scratch.location);
            if (seekComplete) {
                if (!(_streamStartRead() & 0xFFFF)) {
                    _gStreamMovieRetryResumeStep = CD_COMMAND_MOVIE_SEEK_RESUME;
                    queue->movieStep             = CD_COMMAND_MOVIE_RETRY_READ;
                    return 0;
                }
                _streamStartDecode();
                cdVolApplyMoviePreset((u8)gStreamMovieVolumeTableIndex);
                queue->movieDiskRecoveryActive = false;
                queue->cdOperationPending      = false;
                queue->movieStep               = CD_COMMAND_MOVIE_DECODE;
            }
            break;
        case CD_COMMAND_MOVIE_RETRY_READ:
            if (streamPollMovieStop(0) != 0) {
                queue->movieStep = _gStreamMovieRetryResumeStep;
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
static inline void _streamUploadFrameStrips(RECT* uploadRect, u32 vramX, u32 vramY, u16 offsetDrawBuffer)
{
    s16 stripWords;
    s32 drawBufferY;
    s32 stripBytes;
    u16 stripIndex;
    u8* stripPixels;
    u32 widthPixels;

    stripWords = FILE_SYSTEM_IMAGE_STRIP_WIDTH;
    if (_gStreamMovieDisplayMode == STREAM_MOVIE_DISPLAY_RGB24) {
        stripWords = STREAM_MOVIE_RGB24_STRIP_WORDS;
    }
    if (offsetDrawBuffer) {
        drawBufferY = vramY & 0xFFFF;
        if (gDisplayState.drawBuffer != 0) {
            drawBufferY += DISPLAY_BUFFER_STRIDE_ROWS;
        }
        uploadRect->y = drawBufferY;
    } else {
        uploadRect->y = vramY;
    }
    uploadRect->w = stripWords;
    uploadRect->x = vramX;
    uploadRect->h = gStreamMovieHeightRows;
    stripBytes    = stripWords * gStreamMovieHeightRows * 2;
    stripPixels   = (u8*)gMdecMovieOutputBuffers[gMdecMovieOutputSlot];
    widthPixels   = gStreamMovieWidthPixels;
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
        if (queue->movieFrame == _gStreamMovieFrameLimit) {
            queue->movieFrameSubstep = STREAM_MOVIE_PRESENT_NEW_FRAME;
        } else if (queue->movieFrameChanged != 0) {
            queue->movieFrameChanged = false;
            queue->movieFrameSubstep = STREAM_MOVIE_PRESENT_NEW_FRAME;
        } else {
            queue->movieFrameSubstep = STREAM_MOVIE_PRESENT_REPEAT_FRAME;
        }
        // Present from RAM columns or from the already uploaded staging rectangle.
        if (queue->movieVramStaging == 0) {
            offsetDrawBuffer = _gStreamMovieUploadMode != STREAM_MOVIE_UPLOAD_FIXED_VRAM;
            vramX            = _gStreamMovieVramX;
            vramY            = _gStreamMovieVramY;
            _streamUploadFrameStrips(&presentRect, vramX, vramY, offsetDrawBuffer);
        } else {
            presentRect.x = queue->movieStagingX;
            presentRect.y = queue->movieStagingY;
            presentRect.w = gStreamMovieWidthPixels;
            presentRect.h = gStreamMovieHeightRows;
            if (_gStreamMovieUploadMode == STREAM_MOVIE_UPLOAD_FIXED_VRAM) {
                drawBufferY = 0;
            } else {
                drawBufferY = gDisplayState.drawBuffer != 0 ? DISPLAY_BUFFER_STRIDE_ROWS : 0;
            }
            MoveImage(&presentRect, _gStreamMovieVramX, drawBufferY + _gStreamMovieVramY);
        }
    }
}

StreamSlot* streamGetSlot(u16 slotIndex)
{
    return &gStreamSlots[slotIndex];
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
        gStreamMovieWorkspace = memMalloc(STREAM_MOVIE_WORKSPACE_NORMAL_BYTES, true);
    } else {
        gStreamMovieWorkspace = memMalloc(STREAM_MOVIE_WORKSPACE_STREAMING_BYTES, true);
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
    _gStreamMovieVramImagesSaved = preserveVramImages;
    queue->blockGamePause        = true;
}

void streamResetGameRestore(void)
{
    _gStreamGameRestoreStep = STREAM_GAME_RESTORE_MEMORY;
}

s16 streamHasLoadedViewMovie(void* unusedLocation)
{
    enum { STREAM_VIEW_MOVIE_ID_LIMIT = 100U };
    s32 slotIndex;
    s32 hasLoadedMovie;

    slotIndex      = 0;
    hasLoadedMovie = 0;
    while (1) {
        if (gStreamSlots[slotIndex & 0xFFFF].kind == STREAM_KIND_MOVIE) {
            if (gStreamSlots[slotIndex & 0xFFFF].key.parts.id < (u32)STREAM_VIEW_MOVIE_ID_LIMIT) {
                if (gStreamSlots[slotIndex & 0xFFFF].startSector != 0) {
                    hasLoadedMovie = 1;
                    break;
                }
            }
        }
        slotIndex = slotIndex + 1;
        if ((u32)(slotIndex & 0xFFFF) >= ARRAY_SIZE(gStreamSlots)) {
            break;
        }
    }
    return hasLoadedMovie;
}

u16 streamGetFrameLimit(u16 slotIndex)
{
    return gStreamSlots[slotIndex].data.movie.frameLimit;
}

void streamInitMoviePlayback(u32 slotIndex)
{
    streamPrepareMoviePlayback(slotIndex & 0xFFFF);
}
