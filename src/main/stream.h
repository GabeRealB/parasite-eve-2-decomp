#ifndef MAIN_PRIVATE_STREAM_H
#define MAIN_PRIVATE_STREAM_H

#include <psyq/sys/types.h>

#include "types.h"

extern u16 D_8006AC58;

/// Pauses the movie drive and releases its decoder and STR ring state.
///
/// Returns 0 while pause is pending and 1 after shutdown. A nonzero low
/// halfword of `clearFramebuffers` clears both display-movie buffers before
/// restoring the game display. Texture movies instead release their pause
/// block when configured to do so. Movie workspaces are retained for restart;
/// serialize calls with playback and the resident CD command channel.
s16 streamPollMovieStop(s32 clearFramebuffers);

/// Schedules a standalone background bitstream for asynchronous image decoding.
///
/// `bitstream` starts a word-aligned PSX BS command stream and remains readable
/// until VLC expansion. The filesystem borrows the resident image workspace
/// for this input; decoded strips subsequently overwrite it. Requires standalone
/// image loading (no available scene payload), a valid VLC table or its pending
/// rebuild, and auxiliary storage large enough for the complete expanded stream.
/// The request replaces decoder bookkeeping without allocating or cancelling
/// an existing decode, so the previous operation must have ended.
void mdecRequestImageDecode(u_long* bitstream);

/// Requests a rebuild of the VLC lookup table in the shared image workspace.
///
/// Call when a background load reuses the image storage. Sets a pending flag
/// without building the table or accessing the buffer; repeated requests coalesce.
/// `mdecStepImageDecode` rebuilds it before the next standalone image decode,
/// once no scene payload is available. The image workspace must then be writable.
/// A scene decode using that workspace can also satisfy the request.
void mdecRequestImageVlcRebuild(void);

/// Results from preparing a movie's retained playback buffers.
enum {
    STREAM_MOVIE_SETUP_COMPLETE          = 0,
    STREAM_MOVIE_SETUP_INTER_UNAVAILABLE = 1,
};

/// Prepares a loaded movie's state and retained buffer layout before playback.
///
/// Uses the low halfword of `slotIndex` (0..14), without kind/bounds checks.
/// Returns `STREAM_MOVIE_SETUP_COMPLETE`, or `STREAM_MOVIE_SETUP_INTER_UNAVAILABLE`
/// when the descriptor selects INTER but that file has no loaded start sector.
/// Failure still resets slot and playback bookkeeping.
/// Requires reserved movie/actor workspaces with room for this descriptor's
/// buffers, and no active decoder/STR-ring use of them. Display movies take
/// framebuffer ownership, clear both buffers and build the VLC table. Texture
/// movies use the current stage/area layout. Neither path seeks or starts DMA.
u32 streamPrepareMoviePlayback(u32 slotIndex);

/// Copies the available texture-movie frame into its VRAM presentation destination.
///
/// Does nothing while movie presentation is suppressed or no frame is available.
/// Uploads the completed RAM buffer's columns, or copies the staging rectangle,
/// at the movie origin. Upload mode 1 uses fixed VRAM; every other mode adds the
/// current draw-buffer Y offset. The available frame and staging pixels must
/// remain valid through GPU transfer, and the destination must fit in VRAM.
/// Updates the presentation substep (0 new/terminal frame, 1 repeated frame)
/// without clearing availability or changing the display environments.
void streamPresentMovieFrame(void);

/// Returns 1 if any resident movie with an ID below 100 has a loaded sector, else 0.
///
/// `unusedLocation` is ignored: this examines the entire table, without a room,
/// sub-ID or `viewStream` filter. The argument is retained by the resident ABI.
s16 streamHasLoadedViewMovie(void* unusedLocation);

#endif // MAIN_PRIVATE_STREAM_H
