#ifndef ROOMS_SHARED_8017E4F8_H
#define ROOMS_SHARED_8017E4F8_H

#include "common.h"

#include "main/task.h"

/// One row of `RoomsShared8017e4f8Shades`, indexed by
/// `GpEffWork.field_20` (the palette selector packed into the spawn arg).
/// Each field is the right-shift applied to the effect's fade level to get
/// that colour channel, so a row picks the tint of the halo.
typedef struct _RoomsShared8017e4f8Shade {
    /* 0x0 */ s16 r;
    /* 0x2 */ s16 g;
    /* 0x4 */ s16 b;
} RoomsShared8017e4f8Shade;
STATIC_ASSERT_SIZEOF(RoomsShared8017e4f8Shade, 0x6);

extern RoomsShared8017e4f8Shade RoomsShared8017e4f8Shades[];

/// The two draw helpers the halo task calls: the per-frame halo itself and the
/// fading afterglow. Each carrying room names its own pair at its own address,
/// and they need not be the same routine in every room - the shared object only
/// needs a name to relocate against.
void RoomsShared8017e4f8Halo(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void RoomsShared8017e4f8Fade(GsCOORDINATE2* coord, s16 frame, u8* rgb);

/// The rooms' expanding halo: state 0 parks the effect frame on its anchor and
/// works the fade step out of the spawn argument, state 1 draws the halo (plus
/// a half-bright echo on odd ticks) while the level ramps up, and state 2 fades
/// it back out through the afterglow helper before releasing the work block.
/// Eleven rooms carry this body.
void RoomsShared8017e4f8(Task* task);

#endif // ROOMS_SHARED_8017E4F8_H
