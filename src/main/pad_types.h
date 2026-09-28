#ifndef MAIN_PRIVATE_PAD_TYPES_H
#define MAIN_PRIVATE_PAD_TYPES_H

#include "common.h"

/// Raw libpad port buffer (PadInitDirect targets). Indexed with stride 0x24.
/// field_2/field_3 are combined as a big-endian halfword by Pad_ReadButtonsInv.
typedef struct _PadRawPort {
    /* 0x00 */ byte unknown_0[0x2];
    /* 0x02 */ u8   field_2;
    /* 0x03 */ u8   field_3;
    /* 0x04 */ byte unknown_4[0x20];
} PadRawPort;
STATIC_ASSERT_SIZEOF(PadRawPort, 0x24);

#endif // MAIN_PRIVATE_PAD_TYPES_H
