#ifndef MAIN_PRIVATE_CD_TYPES_H
#define MAIN_PRIVATE_CD_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libcd.h>

#include "common.h"

/// Twelve-byte drive header read before a sector's payload in CdlModeSize1.
///
/// Consume all three words before reading the payload. Only the BCD minute,
/// second and sector are interpreted, through `CdPosToInt`; the fourth
/// location byte and the following eight bytes are not interpreted here.
typedef struct {
    CdlLOC location;       // BCD disc position; the fourth byte is unused by position conversion
    u8     unreadBytes[8]; // Transferred with the position; contents remain uninterpreted
} CdSectorHeader;
STATIC_ASSERT_SIZEOF(CdSectorHeader, 3 * sizeof(u32));

#endif // MAIN_PRIVATE_CD_TYPES_H
