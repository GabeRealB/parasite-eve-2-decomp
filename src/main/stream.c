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

static void Stream_InitFromSlot(u32 arg0);

/* Clears both display buffers to black, at the width of the current MDEC mode. */
static __inline__ void _streamClearDisplayBuffers(RECT* rect);

/// Initializes the MDEC ring, output callback and frame decode state.
static void Stream_StartDecoder(void);

static void Mdec_UploadSlice(void);

static void Mdec_KickStrip(void);

static void Mdec_DecodeFrame(void);

static __inline__ u16 Stream_SeekPosition(u8* loc);

/* Resets the decoder and the stream ring, routes decoded slices to the upload
 * callback and applies CD volume table entry 0 ahead of a streaming read. */
static __inline__ void _streamStartDecode(void);

/* Starts the streaming read; ADPCM playback is enabled when the stream's
 * volume table entry is nonzero. */
static __inline__ s32 _streamStartRead(void);

static __inline__ void Stream_UploadFrameStrips(RECT* rect, u32 x, u32 y, u16 useDisplayBuffer);

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

static void Stream_InitFromSlot(u32 arg0)
{
    StreamSlot* slots;
    StreamSlot* slot;

    gCdCmdQueue.suppressMoviePresentation = 0;
    slots                                 = Stream_Slots;
    D_8006AC12                            = 0;
    slot                                  = &slots[arg0 & 0xFFFF];
    D_8006AC08                            = slot->startSector;
    D_8006AC0C                            = slot->data.movie.frameLimit;
    D_8006AC5A                            = slot->data.movie.width;
    D_8006AC6C                            = slot->data.movie.height;
    D_8006AC0E                            = slot->data.movie.vramX;
    D_8006AC10                            = slot->data.movie.vramY;
    D_8006AC16                            = slot->data.movie.loopMode;
    D_8006AC14                            = slot->data.movie.displayMode;
    D_8006AC58                            = slot->data.movie.volumeTableIndex;
    D_8006AC18                            = slot->data.movie.uploadMode;
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

u16 Stream_RestoreAfterLoad(s32 arg0, s32 arg1)
{
    RECT         rect;
    u8           param1[8];
    u8           param2[8];
    GameSession* g;
    CdCmdQueue*  p;
    s32          state;
    u8           f7;
    u8           f6;
    u8           f74;

    p     = &gCdCmdQueue;
    state = D_8006AC28;
    if (state != 1) {
        if (state < 2) {
            if (state == 0) {
                goto case0;
            }
            goto ret_zero;
        }
        if (state == 2) {
            goto ret_one;
        }
        goto ret_zero;
    case0:
        if (D_8006AC1E != 0) {
            rect.x = 0x2C0;
            rect.y = 0;
            rect.w = 0xA0;
            rect.h = 0x100;
            MoveImage2(&rect, 0x140, 0);
            rect.x = 0x360;
            rect.y = 0;
            rect.w = 0xA0;
            rect.h = 0x100;
            MoveImage2(&rect, 0x140, 0x100);
        }
        memConfigureImageMemory(gGameSession->location.loc.stage, gGameSession->location.loc.area);
        if ((arg0 & 0xFFFF) == 1) {
            memSelectAuxHeapRegion(true);
        }
        Tmd_AllocMissingBuffers();
        if (gDisplayState.videoMode == DISPLAY_VIDEO_STREAMING) {
            cdCmdPrepareViewMovie();
            CdCmd_SelectMdecBuffer();
        }
        D_8006AC28 = D_8006AC28 + 1;
        if (arg1 & 0xFFFF) {
            g         = gGameSession;
            f7        = g->location.loc.stage;
            param1[3] = f7;
            f6        = g->location.loc.area;
            param1[0] = 0;
            param1[2] = f6;
            f74       = g->spriteVariant;
            param2[1] = 5;
            param2[2] = 0;
            param2[3] = 0;
            param2[0] = f74;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            goto ret_zero;
        }
        goto ret_one;
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        p->blockGamePause = 0;
        D_8006AC28        = D_8006AC28 + 1;
        return 1;
    }
    goto ret_zero;

ret_one:
    return 1;
ret_zero:
    return 0;
}

/* Clears both display buffers to black, at the width of the current MDEC mode. */
static __inline__ void _streamClearDisplayBuffers(RECT* rect)
{
    rect->y = 0;
    rect->x = 0;
    if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
        rect->w = 0x1E0;
    } else {
        rect->w = 0x140;
    }
    rect->h = 0xF0;
    ClearImage(rect, 0, 0, 0);
    rect->y = 0x110;
    ClearImage(rect, 0, 0, 0);
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
    Stream_InitFromSlot(slot);
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
            Display_SetMode(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_RGB24 | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
        } else {
            Display_SetMode(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
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

    if (CdCmd_PausePoll() & 0xFFFF) {
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
                Display_SetMode(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
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

static void Stream_StartDecoder(void)
{
    CdlLOC      loc;
    RECT        rect;
    s16         backFrame;
    s32         width;
    s16         frame;
    s16         stripWidth;
    s32         index;
    s32         stride;
    u16         i;
    s32         imageX;
    s32         nextStrip;
    u16         originX;
    u16         x, y;
    RECT*       stripRect;
    CdCmdQueue* queue;
    u16         imageY;
    u_long*     data;
    u32         frameWidth;

    queue     = &gCdCmdQueue;
    backFrame = StGetBackloc(&loc) - 1;
    frame     = backFrame;
    if (backFrame <= 0) {
        frame = 1;
    }
    if ((s16)D_8006AC0C < frame) {
        frame = (s16)D_8006AC0C;
    }
    if (queue->movieFrame != frame) {
        queue->movieFrame        = (u16)frame;
        queue->movieFrameChanged = 1;
        queue->movieReady        = 1;
    }
    if (queue->reverseSceneFrames == 0) {
        queue->sceneFrame = (u16)frame;
    } else {
        queue->sceneFrame = (D_8006AC0C - frame) + 1;
    }
    queue->mdecOutputPending = 0;
    if (D_8006AC14 == STREAM_MOVIE_DISPLAY_TEXTURE) {
        queue->movieFrameAvailable = 1;
    } else {
        nextStrip  = D_8006AC1C + 1;
        D_8006AC1C = nextStrip;
        index      = (nextStrip & 0xFFFF) - 1;
        originX    = D_8006AC0E;
        if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
            imageX = originX + index * 0x18;
        } else {
            imageX = originX + index * 0x10;
        }
        imageY = D_8006AC10;
        rect.x = imageX;
        if (gDisplayState.frameBuffer != 0) {
            imageY += 0x110;
        }
        rect.y = imageY;
        width  = 0x10;
        if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
            width = 0x18;
        }
        rect.h = D_8006AC6C;
        rect.w = width;
        LoadImage(&rect, D_8006AC48[D_8005EAEE ^ 1]);
        gDisplayState.frameBuffer ^= 1;
    }
    stripWidth = 0x10;
    if (queue->movieVramStaging != 0) {
        x         = queue->movieStagingX;
        y         = queue->movieStagingY;
        stripRect = &rect;
        if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
            stripWidth = 0x18;
        }
        stripRect->y = y;
        stripRect->w = stripWidth;
        stripRect->h = D_8006AC6C;
        rect.x       = x;
        data         = D_8006AC48[D_8005EAEE];
        stride       = stripWidth * D_8006AC6C * 2;
        frameWidth   = D_8006AC5A;
        for (i = 0; (u32)(i & 0xFFFF) < (frameWidth >> 4); i++) {
            LoadImage(stripRect, data);
            stripRect->x += stripWidth;
            data          = (u_long*)((u8*)data + stride);
        }
    }
    D_8006AC1C  = 0;
    D_8005EAEE ^= 1;
}

static void Mdec_UploadSlice(void)
{
    RECT     rect;
    s32      nextStrip;
    s32      index;
    s32      imageX;
    u16      imageY;
    u16      originX;
    s32      width;
    u16      height;
    s32      size;
    u_long** out;

    if (D_8006AC14 != STREAM_MOVIE_DISPLAY_TEXTURE) {
        if ((D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) && (StCdIntrFlag != 0)) {
            StCdInterrupt();
            StCdIntrFlag = 0;
        }
        if (D_8006AC1C != ((D_8006AC5A >> 4) - 1)) {
            nextStrip  = D_8006AC1C + 1;
            D_8006AC1C = nextStrip;
            index      = (nextStrip & 0xFFFF) - 1;
            originX    = D_8006AC0E;
            if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
                imageX = originX + index * 0x18;
            } else {
                imageX = originX + index * 0x10;
            }
            imageY = D_8006AC10;
            rect.x = imageX;
            if (gDisplayState.frameBuffer != 0) {
                imageY += 0x110;
            }
            width  = 0x10;
            rect.y = imageY;
            if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
                width = 0x18;
            }
            rect.h = D_8006AC6C;
            rect.w = width;
            LoadImage(&rect, D_8006AC48[D_8005EAEE ^ 1]);
            D_8005EAEE ^= 1;
            out         = &D_8006AC48[D_8005EAEE ^ 1];
            height      = D_8006AC6C;
            if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
                size = height * 12;
            } else {
                size = height * 8;
            }
            DecDCTout(*out, size);
            return;
        }
    }
    Stream_StartDecoder();
}

static void Mdec_KickStrip(void)
{
    CdCmdQueue* p;
    s32         size;
    u_long**    base;
    u_long**    outs;
    u16         ac6c;
    s32         temp;

    p = &gCdCmdQueue;
    StFreeRing(D_8006AC68);
    DecDCTin(D_8006AC50[D_8005EAEC], D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB16 ? STREAM_MOVIE_DISPLAY_TEXTURE : D_8006AC14);
    if (D_8006AC14 != STREAM_MOVIE_DISPLAY_TEXTURE) {
        base = D_8006AC48;
        outs = &base[D_8005EAEE ^ 1];
        ac6c = D_8006AC6C;
        if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
            size = ac6c * 12;
        } else {
            size = ac6c * 8;
        }
        DecDCTout(*outs, size);
    } else {
        temp = D_8006AC6C;
        size = (D_8006AC5A * temp) / 2;
        base = D_8006AC48;
        DecDCTout(base[D_8005EAEE ^ 1], size);
    }
    p->mdecOutputPending = 1;
    D_8006AC1A           = 0;
    D_8005EAEC          ^= 1;
}

static void Mdec_DecodeFrame(void)
{
    StHEADER*   header;
    u_long*     headerWords;
    CdCmdQueue* p;

    p = &gCdCmdQueue;
    if (D_8006AC1A != 0) {
        if (DecDCTvlc2(NULL, NULL, D_8006AC38) == 0) {
            Mdec_KickStrip();
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
        Mdec_KickStrip();
    } else {
        D_8006AC1A = 1;
    }

    if (p->continueMovie == 0) {
        p->movieStep = CD_COMMAND_MOVIE_PAUSE;
    }
}

static __inline__ u16 Stream_SeekPosition(u8* loc)
{
    if (CdCmd_SeekL(loc, 0) & 0xFFFF) {
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
    DecDCToutCallback(Mdec_UploadSlice);
    CdVol_ApplyFromTable(0);
    queue->mdecOutputPending = 0;
    D_8006AC1A               = 0;
}

/* Starts the streaming read; ADPCM playback is enabled when the stream's
 * volume table entry is nonzero. */
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
            switch ((s16)CdCmd_PollStatus(0, 0)) {
                case 0:
                    break;
                case 2:
                    CdFlush();
                case 1:
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
            ready = Stream_SeekPosition((u8*)&scratch.location);
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
                    CdCmd_ClearBusy();
                }
            }
            break;
        case CD_COMMAND_MOVIE_DECODE:
            Mdec_DecodeFrame();
            if (CdSync_IsShellOpenBitSet() != 0) {
                state->movieDiskRecoveryActive = 1;
                CdCmd_SetBusy();
                state->movieStep = CD_COMMAND_MOVIE_RECOVER;
            }
            break;
        case CD_COMMAND_MOVIE_PAUSE:
            Mdec_DecodeFrame();
            state->cdOperationPending = 1;
            if (CdCmd_PausePoll() & 0xFFFF) {
                state->movieStep++;
            }
            break;
        case CD_COMMAND_MOVIE_FINISH:
            if (D_8006AC16 == STREAM_MOVIE_REPEAT) {
                CdCmd_SetBusy();
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
                    Display_SetMode(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
                }
                stop->movieFrameAvailable = 0;
                gDisplayState.mdecActive  = 0;
            } else if (D_8006AC3C != 0) {
                stop->blockGamePause = 0;
            }
            return 1;
        case CD_COMMAND_MOVIE_RECOVER:
            if (CdCmd_RecoverDisk() != 0) {
                state->movieStep++;
            }
            break;
        case CD_COMMAND_MOVIE_STOP_RETRY:
            if ((s16)CdCmd_StopMdec(0) != 0) {
                if (D_8006AC14 != STREAM_MOVIE_DISPLAY_TEXTURE) {
                    if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
                        Display_SetMode(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_RGB24 | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
                    }
                    gDisplayState.mdecActive = 1;
                }
                state->cdOperationPending = 1;
                state->movieStep          = CD_COMMAND_MOVIE_SEEK_RESUME;
            }
            break;
        case CD_COMMAND_MOVIE_SEEK_RESUME:
            CdIntToPos(D_8006AC08 + (state->movieFrame - 1) * 10, &scratch.location);
            ready = Stream_SeekPosition((u8*)&scratch.location);
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

static __inline__ void Stream_UploadFrameStrips(RECT* rect, u32 x, u32 y, u16 useDisplayBuffer)
{
    s16 stripWidth;
    s32 bufferY;
    s32 stride;
    u16 i;
    u8* data;
    u32 frameWidth;

    stripWidth = 0x10;
    if (D_8006AC14 == STREAM_MOVIE_DISPLAY_RGB24) {
        stripWidth = 0x18;
    }
    if (useDisplayBuffer & 0xFFFF) {
        bufferY = y & 0xFFFF;
        if (gDisplayState.drawBuffer != 0) {
            bufferY += 0x110;
        }
        rect->y = bufferY;
    } else {
        rect->y = y;
    }
    rect->w    = stripWidth;
    rect->x    = x;
    rect->h    = (s16)D_8006AC6C;
    stride     = stripWidth * D_8006AC6C * 2;
    data       = (u8*)D_8006AC48[D_8005EAEE];
    frameWidth = D_8006AC5A;
    for (i = 0; (u32)(i & 0xFFFF) < (frameWidth >> 4); i++) {
        LoadImage(rect, (u_long*)data);
        rect->x = (u16)rect->x + stripWidth;
        data   += stride;
    }
}

void Stream_PresentFrame(void)
{
    RECT        rect;
    s32         yOffset;
    CdCmdQueue* queue;
    s32         useDisplayBuffer;
    s32         x;
    s32         y;

    queue = &gCdCmdQueue;
    if ((queue->suppressMoviePresentation == 0) && (queue->movieFrameAvailable != 0)) {
        if (queue->movieFrame == D_8006AC0C) {
            queue->movieFrameSubstep = 0;
        } else if (queue->movieFrameChanged != 0) {
            queue->movieFrameChanged = 0;
            queue->movieFrameSubstep = 0;
        } else {
            queue->movieFrameSubstep = 1;
        }
        if (queue->movieVramStaging == 0) {
            useDisplayBuffer = D_8006AC18 != STREAM_MOVIE_UPLOAD_FIXED_VRAM;
            x                = D_8006AC0E;
            y                = D_8006AC10;
            Stream_UploadFrameStrips(&rect, x, y, useDisplayBuffer);
        } else {
            rect.x = queue->movieStagingX;
            rect.y = queue->movieStagingY;
            rect.w = (s16)D_8006AC5A;
            rect.h = (s16)D_8006AC6C;
            if (D_8006AC18 == STREAM_MOVIE_UPLOAD_FIXED_VRAM) {
                yOffset = 0;
            } else {
                yOffset = gDisplayState.drawBuffer != 0 ? 0x110 : 0;
            }
            MoveImage(&rect, (s32)D_8006AC0E, yOffset + D_8006AC10);
        }
    }
}

StreamSlot* streamGetSlot(u16 slotIndex)
{
    return &Stream_Slots[slotIndex];
}

void Mem_AllocAuxWithImages(s16 arg0)
{
    RECT        rect;
    CdCmdQueue* p;

    p = &gCdCmdQueue;
    Gpu_ResetGraphAndOt();
    memSelectAuxHeapRegion(false);
    memInitAuxHeap();
    if (gDisplayState.videoMode == DISPLAY_VIDEO_NORMAL) {
        D_8006AC40 = memMalloc(0x4A800, true);
    } else {
        D_8006AC40 = memMalloc(0x45400, true);
    }
    if ((arg0 & 0xFFFF) != 0) {
        rect.x = 0x140;
        rect.y = 0;
        rect.w = 0xA0;
        rect.h = 0x100;
        MoveImage2(&rect, 0x2C0, 0);
        rect.x = 0x140;
        rect.y = 0x100;
        rect.w = 0xA0;
        rect.h = 0x100;
        MoveImage2(&rect, 0x360, 0);
    }
    D_8006AC1E        = arg0;
    p->blockGamePause = 1;
}

void Stream_ResetRestoreState(void)
{
    D_8006AC28 = 0;
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
