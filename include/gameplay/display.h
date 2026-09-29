#ifndef GAMEPLAY_DISPLAY_H
#define GAMEPLAY_DISPLAY_H

#include "common.h"

#include "main/coord.h"
#include "main/tmd_types.h"

/// The spawn argument and work record of `Gp_FadeWorkTask` (task 0x31), which
/// fades the screen out through a semi-transparent full-screen quad. `field_0`
/// selects the semi-transparency rate of the trailing `DR_TPAGE`; `field_1` is
/// the handshake flag the owner sets to 1 to start the fade-out and the task
/// sets to 2 once it is done; `field_2` is the fade length in frames
/// (defaulted to 0x20), which also divides the ramp.
typedef struct GpFadeWork {
    u8  field_0;
    u8  field_1;
    s16 field_2;
} GpFadeWork;
STATIC_ASSERT_SIZEOF(GpFadeWork, 4);

/// A 2D-display body: the node a spawnType-2 task carries and hangs on
/// `gTmdDisp2dList`, holding one coordinate of its own instead of a model.
///
/// Nothing is drawn from the body — the model passes compose its coordinate once
/// a frame and walk on — so what it is for is the task that owns it, which
/// places the coordinate and reads back the world matrix composed from it. Code
/// outside this overlay reaches the body as a `TmdObject`, so the head is laid
/// out like that type's and, as on a model body, `coords` is where the
/// coordinate is found. A 2D-display body has a single one, so that field points
/// at the node's own `coord` rather than at an array of them.
typedef struct GpDisp2d {
    TmdListHead link;    // Its place on `gTmdDisp2dList`
    GfxCoord*   coords;  // The body's coordinate, i.e. `&coord`
    s32         field_C; // Set to 1 when the body is attached; no reader found, so the role is unproven
    GfxCoord    coord;   // Coordinate the body occupies: its task places it, the passes compose `workm` from it
} GpDisp2d;
STATIC_ASSERT_SIZEOF(GpDisp2d, 0x60);

#endif // GAMEPLAY_DISPLAY_H
