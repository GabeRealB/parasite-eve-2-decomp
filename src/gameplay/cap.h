#ifndef GAMEPLAY_PRIVATE_CAP_H
#define GAMEPLAY_PRIVATE_CAP_H

#include "common.h"

#include "gameplay/cap.h"

typedef struct _GpCapChoice {
    /* 0x0 */ s16 pos[2]; // screen x, y the choice was laid out at
    /* 0x4 */ u16 eventKey;
    /* 0x6 */ u8  sound;
    /* 0x7 */ u8  pad_7;
} GpCapChoice;
STATIC_ASSERT_SIZEOF(GpCapChoice, 8);

/// Read-only layout settings for caption text. `vertical` selects
/// top-to-bottom columns instead of left-to-right lines.
typedef struct {
    u8 vertical;
} _GpCapLayout;

#endif // GAMEPLAY_PRIVATE_CAP_H
