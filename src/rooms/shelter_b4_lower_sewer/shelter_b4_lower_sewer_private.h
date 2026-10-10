#ifndef SRC_ROOMS_SHELTER_B4_LOWER_SEWER_SHELTER_B4_LOWER_SEWER_PRIVATE_H
#define SRC_ROOMS_SHELTER_B4_LOWER_SEWER_SHELTER_B4_LOWER_SEWER_PRIVATE_H

#include "types.h"

/// Next free byte for the lower sewer's mixed water-quad and draw-mode packets.
///
/// Reset each frame to the selected actor-load arena's display half (0 or 1),
/// each 0xC000 bytes. Both water lists append at most 0xD80 bytes together.
/// The cursor stays word-aligned; the borrowed arena must remain reserved
/// through GPU consumption. NULL until the water drawer first resets it.
extern u8* gShelterB4LowerSewerWaterPacketCursor;

#endif // SRC_ROOMS_SHELTER_B4_LOWER_SEWER_SHELTER_B4_LOWER_SEWER_PRIVATE_H
