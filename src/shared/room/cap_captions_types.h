#ifndef SRC_SHARED_CAP_CAPTIONS_TYPES_H
#define SRC_SHARED_CAP_CAPTIONS_TYPES_H

#include "common.h"
#include "types.h"

/// Per-caption-instance continuation-caret delay and three retained image bytes.
///
/// Selecting a text record resets the unsigned-byte countdown to thirty eligible
/// caret calls. Each call with a nonzero countdown decrements it without drawing;
/// the next call at zero draws. Suppressed captions and instant text consume no
/// calls. The other three bytes have no recovered accesses: their meaning and
/// membership in the original countdown object are unproven, so this declaration
/// retains their initializers without treating them as padding.
typedef struct {
    u8 drawsLeft;    // Remaining eligible caret calls before drawing (0 visible).
    u8 unknown_1[3]; // Retained bytes with no identified accesses; role unproven.
} CapCaptionCaretDelayStorage;
STATIC_ASSERT_SIZEOF(CapCaptionCaretDelayStorage, 4);

#endif // SRC_SHARED_CAP_CAPTIONS_TYPES_H
