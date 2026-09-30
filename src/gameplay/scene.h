#ifndef GAMEPLAY_PRIVATE_SCENE_H
#define GAMEPLAY_PRIVATE_SCENE_H

#include "common.h"

#include "gameplay/pad_script.h"
#include "pad_script.h"

/// Complete 0x18-byte pad-script work allocation, also the prefix of GpState34.
typedef struct _GpState18 {
    /* 0x00 */ PadScriptCmd*  field_0;
    /* 0x04 */ GpScriptRec*   field_4;
    /* 0x08 */ s16            field_8;
    /* 0x0A */ GpScriptOpcode field_A;
    /* 0x0C */ GpScriptOpcode field_C;
    /* 0x0E */ u8             field_E;
    /* 0x0F */ u8             field_F;
    /* 0x10 */ u8             field_10;
    /* 0x11 */ u8             field_11;
    /* 0x12 */ u8             field_12;
    /* 0x13 */ u8             field_13;
    /* 0x14 */ u8             field_14;
    /* 0x15 */ u8             field_15;
    /* 0x16 */ byte           pad_16[2];
} GpState18;
STATIC_ASSERT_SIZEOF(GpState18, 0x18);

#endif // GAMEPLAY_PRIVATE_SCENE_H
