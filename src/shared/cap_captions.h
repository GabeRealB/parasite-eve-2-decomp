/* CAP caption relocation, rendering, timing and schedule playback.
 *
 * _capCaptionShowTimed and _capCaptionLoadResource are static-inline
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
 * cap_captions.inc.c, define the timed-caption wrapper, include
 * cap_captions_resource.inc.c, then define the resource-selection wrapper.
 */

#ifndef SRC_SHARED_CAP_CAPTIONS_H
#define SRC_SHARED_CAP_CAPTIONS_H

#include "common.h"
#include "types.h"

#include "cap_captions_types.h"

/// Selects the function that chooses a keyed record and caches its text layout.
///
/// Bind a function identifier before this header and retain through both caption
/// source fragments. It takes (s16 commandIndex, s16 key, s32 bottomBaselineY)
/// and returns s32: 0 selected, 1 null command entry. Room carriers keep the
/// default static instance; actor_215100 binds its public export and an empty
/// `CAP_CAPTION_SELECT_SCRIPT_LINKAGE`. Each carrier owns independent CAP state.
/// The object-like alias has no arguments, captures, side effects or token
/// construction; linkage is selected separately. commandIndex must be a valid
/// relocated command-table index; a non-NULL sequence requires an existing
/// nonterminal key in 0..255 and readable text/glyph cells owned by the live CAP
/// file. bottomBaselineY is in screen pixels and is narrowed to s16.
#ifndef CAP_CAPTION_SELECT_RECORD
#define CAP_CAPTION_SELECT_RECORD _capCaptionSelectRecord
#endif
#ifndef CAP_CAPTION_SELECT_SCRIPT_LINKAGE
#define CAP_CAPTION_SELECT_SCRIPT_LINKAGE static
#endif
CAP_CAPTION_SELECT_SCRIPT_LINKAGE s32 CAP_CAPTION_SELECT_RECORD(s16 commandIndex, s16 key, s32 bottomBaselineY);

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

// Bind stable per-carrier storage before this header and retain the bindings
// through all caption fragments. Defaults name TU-local storage; the incinerator
// supplies overlay-private globals. Bindings may be read repeatedly and must
// have no side effects or token construction.
/// Writable pointer lvalue borrowing readable glyph cells from the loaded CAP file.
///
/// Nonnegative text uses low-ten-bit cell indices; title selectors use eight bits.
#ifndef CAP_CAPTION_GLYPH_CELLS
#define CAP_CAPTION_GLYPH_CELLS _gCapCaptionGlyphCells
#endif
/// Writable pointer lvalue borrowing relocated command references from the CAP file.
///
/// Entries are read by command index through this view; relocation writes
/// them through the loaded file before publishing the pointer.
#ifndef CAP_CAPTION_COMMAND_REFS
#define CAP_CAPTION_COMMAND_REFS _gCapCaptionCommandRefs
#endif
/// Signed-halfword key selected for the current sequence's nonterminal text record.
#ifndef CAP_CAPTION_SELECTED_KEY
#define CAP_CAPTION_SELECTED_KEY _gCapCaptionSelectedKey
#endif
/// Writable pointer lvalue borrowing the selected CAP sequence, or NULL.
///
/// Text records follow its command header and end at CAP_TEXT_REF_END.
#ifndef CAP_CAPTION_SEQUENCE
#define CAP_CAPTION_SEQUENCE _gCapCaptionSequence
#endif
/// Writable s16 left X in biased screen pixels: (320 - widest closed line)/2 - 5.
#ifndef CAP_CAPTION_BLOCK_LEFT_X
#define CAP_CAPTION_BLOCK_LEFT_X _gCapCaptionBlockLeftX
#endif
/// Writable s16 first baseline in screen pixels, derived from the bottom baseline.
#ifndef CAP_CAPTION_FIRST_BASELINE_Y
#define CAP_CAPTION_FIRST_BASELINE_Y _gCapCaptionFirstBaselineY
#endif
/// Writable s16 final baseline in screen pixels, narrowed from the selector input.
#ifndef CAP_CAPTION_BOTTOM_BASELINE_Y
#define CAP_CAPTION_BOTTOM_BASELINE_Y _gCapCaptionBottomBaselineY
#endif
/// Writable s16 record slot in the selected sequence; text begins at slot one.
#ifndef CAP_CAPTION_RECORD_INDEX
#define CAP_CAPTION_RECORD_INDEX _gCapCaptionRecordIndex
#endif
/// Writable s16 closed-line block height in pixels.
///
/// Each closed line contributes its tallest glyph height plus the two-pixel gap.
#ifndef CAP_CAPTION_BLOCK_HEIGHT
#define CAP_CAPTION_BLOCK_HEIGHT _gCapCaptionBlockHeight
#endif
/// Writable u16 continuation-triangle left X in draw pixels, with unsigned wrapping.
#ifndef CAP_CAPTION_CARET_LEFT_X
#define CAP_CAPTION_CARET_LEFT_X _gCapCaptionCaretLeftX
#endif
/// Writable u16 continuation-triangle tip Y in draw pixels, with unsigned wrapping.
#ifndef CAP_CAPTION_CARET_TIP_Y
#define CAP_CAPTION_CARET_TIP_Y _gCapCaptionCaretTipY
#endif
/// Writable u8 delay before the continuation caret, counted in eligible draw calls.
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

static inline void _capCaptionLoadResource(s16 texturePageX, s16 texturePageY, s16 dataResourceIndex);

#endif /* SRC_SHARED_CAP_CAPTIONS_H */
