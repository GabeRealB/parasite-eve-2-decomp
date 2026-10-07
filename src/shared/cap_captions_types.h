#ifndef SRC_SHARED_CAP_CAPTIONS_TYPES_H
#define SRC_SHARED_CAP_CAPTIONS_TYPES_H

#include "common.h"
#include "types.h"

/// Continuation-caret countdown storage, including three uninterpreted retained bytes.
///
/// Only the first byte is accessed by the caption implementation. The remaining
/// bytes retain the instance's original initializer; their role is unproven.
/// The countdown is reset on selection and decreases on eligible caret calls,
/// suppressing the triangle until the call after it reaches zero.
typedef struct {
    u8 drawsLeft;    // Remaining eligible caret calls before drawing (0 visible).
    u8 unknown_1[3]; // Retained bytes with no identified accesses; role unproven.
} CapCaptionCaretDelayStorage;
STATIC_ASSERT_SIZEOF(CapCaptionCaretDelayStorage, 4);

#endif // SRC_SHARED_CAP_CAPTIONS_TYPES_H
