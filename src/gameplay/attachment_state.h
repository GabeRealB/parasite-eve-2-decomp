#ifndef GAMEPLAY_PRIVATE_ATTACHMENT_STATE_H
#define GAMEPLAY_PRIVATE_ATTACHMENT_STATE_H

#include "common.h"

#include "hud.h"

/// +0xC overlay of the 0x30-byte record `Gp_InitPlayClock` allocates with
/// `memCalloc(0x30, 0)` and stores at `Task::work`. `Gp_ResetHudFx` is
/// called with that pointer + 0xC; it writes `field_16 = -1` and clears
/// `field_18`. `Gp_UseItemTask` clears `field_10` (word) and `field_E` (`sb`)
/// on entry and reads `field_15` (`lb`) as a gate on the pad poll.
typedef struct _GpIdMapC {
    /* 0x00 */ s32        field_0;
    /* 0x04 */ s32        field_4;
    /* 0x08 */ s32        field_8;
    /* 0x0C */ byte       pad_C;
    /* 0x0D */ s8         field_D;
    /* 0x0E */ s8         field_E;
    /* 0x0F */ byte       pad_F;
    /* 0x10 */ s32        field_10;
    /* 0x14 */ u8         field_14;
    /* 0x15 */ s8         field_15;
    /* 0x16 */ s8         field_16;
    /* 0x17 */ byte       pad_17;
    /* 0x18 */ s16        field_18;
    /* 0x1A */ byte       pad_1A[2];
    /* 0x1C */ GpHudTrack field_1C;
} GpIdMapC;
STATIC_ASSERT_SIZEOF(GpIdMapC, 0x24);

#endif // GAMEPLAY_PRIVATE_ATTACHMENT_STATE_H
