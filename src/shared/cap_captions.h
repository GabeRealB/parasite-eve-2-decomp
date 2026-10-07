/* CAP caption relocation, rendering, timing and schedule playback.
 *
 * CapCaption_ShowTimed and _capCaptionLoadResource are static-inline
 * implementations behind ordinary overlay entry points. Helpers and common settings are private. The including
 * overlay defines its own schedule descriptor and caption work at their data
 * positions. Cross-TU work declarations belong in the overlay's private header;
 * exported schedule declarations belong in its public header. Retained caret
 * bytes stay with that instance's storage. CAP_CAPTION_DRAW_CURRENT selects the
 * current-caption drawer; its default is private to each carrier. A carrier
 * exporting its drawer binds the name and leaves its linkage binding empty.
 *
 * Include this header in the prologue and the _settings and _schedule storage
 * fragments at their data positions. At the function run, include
 * cap_captions.inc.c, define the ShowTimed wrapper, include
 * cap_captions_resource.inc.c, then define the resource-selection wrapper.
 */

#ifndef SRC_SHARED_CAP_CAPTIONS_H
#define SRC_SHARED_CAP_CAPTIONS_H

#include "common.h"
#include "types.h"

#include "cap_captions_types.h"

// Selection resets the continuation-caret delay in eligible drawer calls.
enum {
    CAP_CAPTION_CARET_DELAY_DRAWS = 30
};

// Schedule states, the terminating upper bound, and the window-to-frame scale.
enum {
    CAP_CAPTION_SCHEDULE_INIT            = 0,
    CAP_CAPTION_SCHEDULE_RUNNING         = 1,
    CAP_CAPTION_SCHEDULE_END             = -1,
    CAP_CAPTION_SCHEDULE_FRAMES_PER_UNIT = 30
};

/// One window of a caption schedule: the caption to show while the session's
/// scene clock lies inside it.
///
/// Bounds count `CAP_CAPTION_SCHEDULE_FRAMES_PER_UNIT` clock frames each, so
/// the window holds the clock values above `lower` units and up to `upper`
/// units. The clock is signed and counts down past zero, so a bound may be
/// negative. A schedule is scanned from its first entry every frame and the
/// first window holding the clock is the one played; an entry whose `upper` is
/// `CAP_CAPTION_SCHEDULE_END` ends the table, so no window can use that bound.
/// `commandIndex` picks a sequence out of the loaded caption file and `key`
/// the line within it; the sequence's own command record is not consulted. A
/// window with command index 0 plays nothing.
typedef struct {
    s32 upper;        // Last clock value inside the window, in schedule units (inclusive).
    s32 lower;        // Clock value just below the window, in schedule units (exclusive).
    s32 commandIndex; // Entry of the caption file's `CapCommandTable` whose sequence is played (0 none).
    s32 key;          // Variant key of the line shown, matched against `CapSequenceRecord.key`.
} CapCaptionScheduleWindow;
STATIC_ASSERT_SIZEOF(CapCaptionScheduleWindow, 0x10);

/// Selects per-carrier caption storage for the included implementation.
///
/// Bind before this header and retain through all caption fragments. Defaults
/// name TU-local storage; the incinerator supplies its overlay-private globals.
/// The glyph binding is a writable pointer lvalue to readable TextGlyphCell
/// storage, accepting const cells. The sequence binding is a writable
/// CapSequenceRecord* lvalue. Both borrow the loaded CAP file. Metric and
/// record-index bindings are s16 lvalues; caret positions are u16, and
/// CAP_CAPTION_CARET_DRAWS_LEFT is a writable u8 lvalue.
/// Bindings have no side effects or token construction and may be read repeatedly.
#ifndef CAP_CAPTION_GLYPH_CELLS
#define CAP_CAPTION_GLYPH_CELLS _gCapCaptionGlyphCells
#endif
#ifndef CAP_CAPTION_SEQUENCE
#define CAP_CAPTION_SEQUENCE _gCapCaptionSequence
#endif
#ifndef CAP_CAPTION_BLOCK_LEFT_X
#define CAP_CAPTION_BLOCK_LEFT_X _gCapCaptionBlockLeftX
#endif
#ifndef CAP_CAPTION_FIRST_BASELINE_Y
#define CAP_CAPTION_FIRST_BASELINE_Y _gCapCaptionFirstBaselineY
#endif
#ifndef CAP_CAPTION_BOTTOM_BASELINE_Y
#define CAP_CAPTION_BOTTOM_BASELINE_Y _gCapCaptionBottomBaselineY
#endif
#ifndef CAP_CAPTION_RECORD_INDEX
#define CAP_CAPTION_RECORD_INDEX _gCapCaptionRecordIndex
#endif
#ifndef CAP_CAPTION_BLOCK_HEIGHT
#define CAP_CAPTION_BLOCK_HEIGHT _gCapCaptionBlockHeight
#endif
#ifndef CAP_CAPTION_CARET_LEFT_X
#define CAP_CAPTION_CARET_LEFT_X _gCapCaptionCaretLeftX
#endif
#ifndef CAP_CAPTION_CARET_TIP_Y
#define CAP_CAPTION_CARET_TIP_Y _gCapCaptionCaretTipY
#endif
#ifndef CAP_CAPTION_CARET_DRAWS_LEFT
#define CAP_CAPTION_CARET_DRAWS_LEFT (_gCapCaptionCaretDelayStorage.drawsLeft)
#endif

/// Selects the void(void) function that queues the selected caption and its caret.
///
/// Bind a function identifier before this header and retain it through
/// `cap_captions.inc.c`. Room carriers use the default static instance; actor_215100
/// binds its public export and an empty `CAP_CAPTION_DRAW_CURRENT_LINKAGE`.
/// All selected definitions take no arguments and return no value; the name
/// binding does not determine linkage. Each carrier owns independent state.
/// The alias has no arguments, captures or token construction.
#ifndef CAP_CAPTION_DRAW_CURRENT
#define CAP_CAPTION_DRAW_CURRENT _capCaptionDrawCurrent
#endif
#ifndef CAP_CAPTION_DRAW_CURRENT_LINKAGE
#define CAP_CAPTION_DRAW_CURRENT_LINKAGE static
#endif
CAP_CAPTION_DRAW_CURRENT_LINKAGE void CAP_CAPTION_DRAW_CURRENT(void);

static inline void CapCaption_ShowTimed(s16 arg0, s16 arg1, s16 arg2);
static inline void _capCaptionLoadResource(s16 texturePageX, s16 texturePageY, s16 dataResourceIndex);

#endif /* SRC_SHARED_CAP_CAPTIONS_H */
