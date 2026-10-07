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

/// Selects the current-caption void(void) function defined by the shared source.
///
/// Bind a function identifier before this header and retain it through
/// cap_captions.inc.c. Room carriers use the default static instance; actor_215100
/// binds its public export and an empty CAP_CAPTION_DRAW_CURRENT_LINKAGE.
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
