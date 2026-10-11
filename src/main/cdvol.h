#ifndef MAIN_PRIVATE_CDVOL_H
#define MAIN_PRIVATE_CDVOL_H

#include "types.h"

/// Movie CD-input gain presets in seven-bit levels (0 silent, 127 full).
///
/// Index 0 is silent; valid indexes are 0..39. Each level is applied equally to
/// the left and right CD inputs at 256 SPU register units per level, and also
/// replaces the CD fade cursor. Storage remains in the resident audio data block.
extern u8 gCdVolMoviePresetLevels[40];

#endif // MAIN_PRIVATE_CDVOL_H
