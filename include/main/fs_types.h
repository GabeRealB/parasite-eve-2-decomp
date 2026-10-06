#ifndef MAIN_FS_TYPES_H
#define MAIN_FS_TYPES_H

#include <psyq/sys/types.h>

#include "common.h"

#include "main/stream_types.h"

/// Request opcodes stored in `CdCmdEntry.cmd`; the high nibble selects a handler.
enum {
    CD_COMMAND_EMPTY                     = 0,
    CD_COMMAND_LOAD_FILE                 = 0x21,
    CD_COMMAND_MOUNT_STAGE               = 0x54,
    CD_COMMAND_READ_STAGE_HEADER         = 0x55,
    CD_COMMAND_CONTINUE_STREAM           = 0x62,
    CD_COMMAND_PLAY_STREAM_AT_OFFSET     = 0x71,
    CD_COMMAND_RESET_STREAM_AT_OFFSET    = 0x72,
    CD_COMMAND_RESUME_STREAM_AT_POSITION = 0x73,
    CD_COMMAND_PLAY_SCENE_AUDIO          = 0x81,
    CD_COMMAND_START_SCENE_AUDIO         = 0x82,
};

/// File-load policies stored in `CdCmdEntry.args.file.loadMode`.
enum {
    CD_COMMAND_LOAD_DEFAULT         = 0,
    CD_COMMAND_LOAD_SEEK_ONLY       = 1,
    CD_COMMAND_LOAD_INIT_SOUND      = 2,
    CD_COMMAND_LOAD_RELOCATE_IMAGES = 3,
    CD_COMMAND_LOAD_SKIP_BACKGROUND = 4,
    CD_COMMAND_LOAD_IMAGES_ONLY     = 5,
    CD_COMMAND_LOAD_SKIP_SOUND      = 6,
};

/// An eight-byte CD request, used in the ring and its active/replacement slots.
///
/// `args` preserves all four bytes of the enqueue API's second parameter block.
/// File commands interpret them as an ID component and load options; stream
/// commands interpret them as a slot and, for 0x71/0x72, a signed 16-bit sector
/// offset packed high byte first. The other bytes are copied without interpreting
/// them when a request is saved or requeued.
///
/// Stage-zero file IDs are `fileGroup*10000 + args.file.fileIdHundreds*100 +
/// fileIndex`. Stages 1..5 use folder `fileGroup*100 + args.file.fileIdHundreds`
/// and `fileIndex` within it. `stage` also selects the CDF for mount commands.
/// A zero `cmd` marks an empty or retired record. Iterator pointers borrow ring
/// storage and must be copied before queue operations can overwrite the slot.
typedef struct {
    union {
        u8 bytes[4];             // Exact second parameter block, including bytes unused by an opcode
        struct {
            u8 fileIdHundreds;   // Stage-zero hundreds component; otherwise the folder-number suffix
            s8 loadMode;         // Load policy (0 normal, 1 seek, 2 sound setup, 3 relocate images, 4 skip background, 5 images only, 6 skip sound)
            s8 imageXPageOffset; // Signed strip X shift in 64-word VRAM pages (y >= 256 or load mode 3)
            s8 imageYOffset;     // Signed vertical image shift in VRAM rows, applied to image headers with y=245..255
        } file;
        struct {
            s8 slotIndex;        // Stream-slot index (0..14); negative values are invalid
            u8 sectorOffsetHigh; // High byte of the signed sector offset for 0x71/0x72
            u8 sectorOffsetLow;  // Low byte of that offset; the fourth argument byte is unused by stream handlers
        } stream;
    } args;
    u8 cmd;       // Request opcode (0 empty); high nibble identifies the command family
    u8 stage;     // CDF index (0 global library, 1..5 room folders); mount selector for 0x54
    u8 fileGroup; // Stage-zero file category; otherwise the folder-number hundreds component
    u8 fileIndex; // Stage-zero file-ID low component; otherwise the file index within the folder
} CdCmdEntry;
STATIC_ASSERT_SIZEOF(CdCmdEntry, 0x8);

/// Layout of the decoded room image consumed by the display uploader.
enum {
    FILE_SYSTEM_IMAGE_CONTIGUOUS = 0,
    FILE_SYSTEM_IMAGE_STRIPS     = 1,
};

/// Dispatch modes for the saved active request.
enum {
    CD_COMMAND_PHASE_DISPATCH = 0,
    CD_COMMAND_PHASE_CANCEL   = 1,
    CD_COMMAND_PHASE_SUSPEND  = 2,
};

/// Completion and header-wait markers for image loads.
enum {
    CD_COMMAND_IMAGE_PENDING     = 0,
    CD_COMMAND_IMAGE_COMPLETE    = 0xFF,
    CD_COMMAND_IMAGE_WAIT_HEADER = -1,
    CD_COMMAND_IMAGE_START       = 0,
    CD_COMMAND_IMAGE_WAIT_OUTPUT = 1,
};

/// Movie playback steps, including disk recovery and read-start retries.
enum {
    CD_COMMAND_MOVIE_WAIT_READY  = 0,
    CD_COMMAND_MOVIE_INIT        = 1,
    CD_COMMAND_MOVIE_SEEK_START  = 2,
    CD_COMMAND_MOVIE_DECODE      = 3,
    CD_COMMAND_MOVIE_PAUSE       = 4,
    CD_COMMAND_MOVIE_FINISH      = 5,
    CD_COMMAND_MOVIE_RECOVER     = 6,
    CD_COMMAND_MOVIE_STOP_RETRY  = 7,
    CD_COMMAND_MOVIE_SEEK_RESUME = 8,
    CD_COMMAND_MOVIE_RETRY_READ  = 9,
};

/// Drive seek and pause steps stored independently of the request handler step.
enum {
    CD_COMMAND_SEEK_SET_LOCATION = 0,
    CD_COMMAND_SEEK_ISSUE        = 1,
    CD_COMMAND_SEEK_WAIT         = 2,
    CD_COMMAND_PAUSE_ISSUE       = 0,
    CD_COMMAND_PAUSE_WAIT        = 1,
};

/// Saved-request cancellation steps, shared by movie and scene/audio cancellation.
enum {
    CD_COMMAND_CANCEL_BEGIN  = 0,
    CD_COMMAND_CANCEL_WAIT   = 1,
    CD_COMMAND_CANCEL_FINISH = 2,
};

/// Scene/audio modes and the failed scene-slot lookup marker.
enum {
    CD_COMMAND_SCENE_INACTIVE       = 0,
    CD_COMMAND_SCENE_PLAYING        = 1,
    CD_COMMAND_SCENE_STARTING_AUDIO = 2,
    CD_COMMAND_NO_SCENE_SLOT        = -1,
};

/// Timing words are cumulative horizontal-scanline deadlines, with two markers.
enum {
    CD_COMMAND_TIMING_END  = 0,
    CD_COMMAND_TIMING_SKIP = -1,
};

/// Image MDEC input modes; bit 1 asks the decoder to set output pixel bit 15.
enum {
    MDEC_IMAGE_MODE_RGB16          = 0,
    MDEC_IMAGE_MODE_RGB16_MASK_BIT = 2,
};

/// Resident CD-request ring and shared file, movie and scene/audio load state.
///
/// Ring indices wrap over eight entries; enqueue does not check for a full ring,
/// so producers must avoid overtaking the consumer. `activeRequest` preserves a
/// request while it is cancelled, suspended or requeued. Its complete 16-byte
/// extent is cleared together, including the saved sector, phase and unknown bytes.
///
/// `sceneStream` borrows a descriptor until its stream table is reloaded. Scene
/// headers are indexed by payload destination (0..4), and remain valid with the
/// associated buffers until those buffers are reused. Decode storage comes from
/// the auxiliary heap; VLC and timing storage may instead borrow actor buffers.
/// The external payload buffer is caller-owned. Transfers and header offsets must
/// fit the selected storage; this object does not record every buffer's capacity.
/// Timing storage contains aligned u32 deadlines in horizontal scanlines, ending
/// at 0 and skipping 0xFFFFFFFF; its extent comes from the scene descriptor/payload.
/// Movie frame limits must fit positive s16 values for the decoder's frame clamp.
/// Unknown storage keeps its observed extent without claiming a padding role.
typedef struct {
    // clang-format off
    CdCmdEntry entries[8];                        // Pending requests; readIdx/writeIdx are in 0..7
    struct {
        CdCmdEntry entry;                         // Saved request, independent of the ring slot's lifetime
        s32        resumeSector;                  // Absolute CD sector captured when suspending a movie
        s8         phase;                         // Dispatch mode (0 normal, 1 cancel, 2 suspend)
        byte       unknown_4d[3];                 // Role unproven; cleared with the complete active request
    } activeRequest;
    CdCmdEntry replacementEntry;                  // Deferred request committed to the ring; cmd=0 means empty
    StreamSceneImageHeader sceneImageHeaders[5];  // Cached headers for decode, actor 0/1/2 and external storage
    u8*         decodeBuffer;                     // Auxiliary-heap scene payload storage
    s32         decodeBufferBytes;                // Byte count for buffer setup; advanced after a kind-0 payload is decoded
    u16*        vlcTable;                         // SDK VLC table; allocated or borrowed from actor/image storage
    StreamSlot* sceneStream;                      // Borrowed scene/audio descriptor, not a movie descriptor
    s32         nextDecodeBufferBytes;            // Byte capacity requested by the latest kind-0 payload header
    u8*         externalScenePayloadBuffer;       // Caller-owned storage for payload destination 4
    u32*        timingCursor;                     // Current timing word; 0 ends advancement, 0xFFFFFFFF skips one word
    u32         timingElapsedLines;               // Accumulated horizontal scanlines since scene timing began
    u32*        timingBuffer;                     // Timing-table base, allocated or borrowed from an actor buffer
    s32         savedLcgState;                    // Game LCG state restored after deterministic scene playback
    u32         savedRandSeed;                    // Next SDK rand() result used to reseed after scene playback
    byte        unknown_1B0[0x18];
    u16         writeIdx;                         // Next ring slot to fill (0..7)
    u16         readIdx;                          // Ring slot being consumed (0..7)
    byte        unknown_1cc[4];
    u16         step;                             // Current request handler's step; interpretation depends on opcode
    u16         suspendResumeStep;                // Suspend/follow-up step (0 begin, 1 wait, 2 stop when suspending)
    u16         diskError;                        // Most recent CD sync result was a disk error (0 no, 1 yes)
    u16         seekStep;                         // Seek step (0 wait/setloc, 1 seek, 2 wait for completion)
    byte        unknown_1d8[6];
    u16         pauseStep;                        // Pause step (0 issue command, 1 wait/retry)
    u16         pauseRetryCount;                  // Retries through the pause handler's status-3 path
    u16         field_1E2;                        // Only cleared on movie shutdown; role unproven
    u16         movieStep;                        // Playback step (0 ready, 1 init, 2 seek, 3 decode, 4 pause, 5 finish, 6..9 recovery)
    u16         movieFrameAvailable;              // Texture movie has a decoded frame available for presentation
    u16         continueMovie;                    // Nonzero continues decoding; zero requests stopping after a frame
    u16         movieFrame;                       // Encoded movie frame, numbered from 1
    u16         mdecOutputPending;                // Decoder output is in progress; completion callback clears it
    u16         sceneFrame;                       // Scene traversal frame: movieFrame, or frameLimit-movieFrame+1 in reverse
    u16         reverseSceneFrames;               // Scene traversal direction (0 forward, 1 reverse)
    u16         movieFrameSubstep;                // Presentation substep (0 newly decoded/terminal frame, 1 repeated frame)
    u16         movieFrameChanged;                // Latched frame change, consumed by the next texture presentation
    u16         movieAtEnd;                       // Latched when the encoded frame reaches the movie's frame limit
    u16         plazaStreamSubId;                 // Plaza movie selector (0..5), passed as the stream's subId
    u16         movieReady;                       // Movie wait gate (0 awaiting a frame, 1 started or skipped)
    u16         cancelStep;                       // Cancel step (0 begin, 1 fade/wait, 2 stop/resume trailing audio)
    u8          imageLoadStatus;                  // Image completion (0 pending, 0xFF complete)
    u8          field_1FF;                        // Set to 1 when selecting scene/audio; subsequent role unproven
    u16         imageDecodePending;               // Nonzero while an image decode request remains outstanding
    s16         imageDecodeStep;                  // -1 wait for header, 0 start, 1 wait for output
    u16         suspendNormalDispatch;            // Nonzero suppresses normal ring dispatch; setter is unproven
    byte        unknown_206[4];
    u16         suppressMoviePresentation;        // Nonzero suppresses movie presentation during a session change
    byte        unknown_20C[2];
    s16         sceneAudioMode;                   // Scene/audio mode (0 inactive, 1 scene playback, 2 starting audio)
    u16         viewMovieSelected;                // View movie selected after releasing slot-4 model buffers
    u16         sceneAudioStarted;                // Audio has reached its started phase; next request can reuse it
    u16         scenePayloadAvailable;            // Completed scene payload enables cached-header image decoding
    u16         sceneBuffersNeeded;               // Scene selection requires VLC/timing/decode buffer setup
    u16         scenePayloadLoading;              // Payload transfer in progress; cached-header resolution must wait
    s16         sceneSlotIndex;                   // Selected scene/audio table index (-1 no matching slot)
    u16         imageLayout;                      // Room image layout (0 contiguous 320x240, 1 twenty 16x240 strips)
    u16         vlcTableBuilt;                    // Reserved scene VLC table has been initialized (0 no, 1 yes)
    byte        unknown_220[2];
    u16         pausePlayClock;                   // Nonzero pauses the play clock during CD file loads
    u16         bootLoadActive;                   // Boot image/text load needs servicing by the CD dispatcher
    u16         diskRecoveryStep;                 // Disk validation (0 test read, 1 wait after validation failure)
    u16         syncRecoveryStep;                 // Sync recovery (0 poll command, 1 recover after shell-open error)
    u16         imageMdecMode;                    // One-image input mode (0 RGB16, 2 RGB16 with pixel bit 15 set); then reset
    u16         movieVramStaging;                 // Nonzero uploads to a fixed staging rectangle before presentation
    u16         holdBootImage;                    // Nonzero holds the boot image/text before its closing fade
    u16         movieStagingX;                    // Staging rectangle X in VRAM words
    u16         movieStagingY;                    // Staging rectangle Y in VRAM rows
    s16         rebuildImageVlcTable;             // Rebuild the image-workspace VLC table before a standalone decode
    s16         field_236;                        // Negative fallback status after failed selection; role unproven
    s16         sceneVlcTableMode;                // Descriptor policy (0 reserved VLC storage, 1 image-workspace VLC storage)
    s16         sceneEnded;                       // Scene restored/ended; later payload arrivals do not enable scene decoding
    byte        unknown_23C[2];
    s16         preserveDisplayAfterDecode;       // One-shot copy of the displayed image instead of clearing the other buffer
    s16         paceToSceneTiming;                // Nonzero enables main-loop pacing against the timing table
    s16         cdOperationPending;               // CD seek/read/pause must finish before cancellation or suspension
    s16         blockGamePause;                   // Nonzero blocks game pause unless the scene halt flag permits it
    u16         scenePayloadReusable;             // A zero-forceReload payload may be reused until decode completion
    u16         releasePauseBlockAfterFade;       // Session-start fade releases blockGamePause on completion
    u16         field_24A;                        // Set by particular map movie-buffer layouts; role unproven
    u16         movieDiskRecoveryActive;          // Latched shell-open movie recovery, cleared after restarting its read
    u16         movieReadPending;                 // Initial movie seek/read has not yet started successfully
    byte        unknown_250[2];
    s16         busy;                             // Blocking CD request is active; mirrored by the display's CD-busy latch
    // clang-format on
} CdCmdQueue;
STATIC_ASSERT_SIZEOF(CdCmdQueue, 0x254);
STATIC_ASSERT(sizeof(((CdCmdQueue*)0)->activeRequest) == 0x10, cd_command_active_request_size);

/// Resource kinds in a CDF bundle directory, separate from outer chunk opcodes.
enum {
    FILE_SYSTEM_RESOURCE_NONE  = 0,
    FILE_SYSTEM_RESOURCE_IMAGE = 2,
    FILE_SYSTEM_RESOURCE_DATA  = 3,
};

/// A borrowed payload address and kind from a CDF resource bundle.
///
/// The resident table preserves the bundle's fifty directory slots (indices
/// 0..49). Addresses are published before payload loading finishes; consumers
/// must wait for that load and keep its destination storage alive. Images use
/// `FsImageChunk`; data resources can contain CAP/STF files or MDEC bitstreams.
/// Clearing `kind` invalidates a slot without clearing or releasing `data`.
typedef struct {
    u8    kind; // Resource kind (0 none, 2 image, 3 untyped data)
    void* data; // Borrowed RAM destination; its payload format depends on the resource
} FsResourceSlot;
STATIC_ASSERT_SIZEOF(FsResourceSlot, 0x8);

/// Pixel layout of the resident image workspace.
///
/// One frame is 320×240 16-bit pixels in twenty strips of 16×240. Each GPU
/// word holds two pixels, a contiguous row is 640 bytes, and pixel bit 15 is
/// the mask bit. Image decode builds its VLC table at
/// `FILE_SYSTEM_IMAGE_VLC_OFFSET`; that table is `STREAM_VLC_TABLE_BYTES`
/// long and still ends inside the frame.
enum {
    FILE_SYSTEM_IMAGE_WIDTH       = 320,
    FILE_SYSTEM_IMAGE_HEIGHT      = 240,
    FILE_SYSTEM_IMAGE_STRIP_WIDTH = 16,
    FILE_SYSTEM_IMAGE_STRIP_COUNT = FILE_SYSTEM_IMAGE_WIDTH / FILE_SYSTEM_IMAGE_STRIP_WIDTH,
    FILE_SYSTEM_IMAGE_STRIP_WORDS = (FILE_SYSTEM_IMAGE_STRIP_WIDTH * FILE_SYSTEM_IMAGE_HEIGHT) / 2,
    FILE_SYSTEM_IMAGE_ROW_BYTES   = FILE_SYSTEM_IMAGE_WIDTH * 2,
    FILE_SYSTEM_IMAGE_PIXEL_MASK  = 0x8000,
    FILE_SYSTEM_IMAGE_VLC_OFFSET  = 0x8800,
};

/// Resident image workspace for one 320×240 frame.
///
/// `strips` is the frame in decode order. The same bytes are also one
/// contiguous image. A background bitstream occupies them until decoded
/// strips replace it, and the image-mode VLC table is built in the middle
/// of that storage first.
typedef struct {
    u_long strips[FILE_SYSTEM_IMAGE_STRIP_COUNT][FILE_SYSTEM_IMAGE_STRIP_WORDS]; // 16×240 columns, two pixels per word
} FsImgBuffers;
STATIC_ASSERT_SIZEOF(FsImgBuffers, 0x25800);

/// The fixed image buffers at the top of retail memory; no image of the game
/// holds them.
extern FsImgBuffers D_801D7000;
STATIC_ASSERT(sizeof(FsImgBuffers) == FILE_SYSTEM_IMAGE_WIDTH * FILE_SYSTEM_IMAGE_HEIGHT * 2, fs_image_frame_bytes);
STATIC_ASSERT(FILE_SYSTEM_IMAGE_VLC_OFFSET + STREAM_VLC_TABLE_BYTES <= sizeof(FsImgBuffers), fs_image_vlc_table_fits);

/// Header of one compressed VRAM rectangle.
///
/// CLUT chunk payloads, image resources in a CDF bundle, and scene-image
/// records begin with this header. `Fs_LoadImageChunk` copies the rectangle,
/// decompresses the LZSS bytes that follow the header, and uploads the result.
/// Height is stored before width.
typedef struct {
    u16 x;         // Destination X in VRAM, in halfwords
    u16 y;         // Destination row. Rows 245..255 receive the active Y shift
    u16 h;         // Height in rows
    u16 w;         // Width in halfwords
    u8  unused[8]; // Unread by the loader. Zero in every retail CLUT payload and bundle image
} FsImageChunk;
STATIC_ASSERT_SIZEOF(FsImageChunk, 0x10);

#endif // MAIN_FS_TYPES_H
