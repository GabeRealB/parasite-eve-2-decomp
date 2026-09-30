#ifndef MAIN_MC_H
#define MAIN_MC_H

#include "main/mc_types.h"

/// Resident memory-card save images: the live record and its backup.
///
/// Each element is one `McSaveData` image. The game reads and writes
/// `MEMORY_CARD_SAVE_LIVE`. `MEMORY_CARD_SAVE_BACKUP` is addressed only as the
/// following bytes of that same buffer: the card code compares the two images,
/// copies the live image over the backup, and transfers both when loading.
/// Initialization clears the live image and fills the backup with 0xFF. Saving
/// checksums the live image and writes it to the card twice before that copy.
/// A save-header check reads the live image through its `preview` member.
extern McSaveData gMcSaveData[MEMORY_CARD_SAVE_COUNT];

void Mc_ResetSaveFlags(void);

/// Init Mc_BufferSlots[1..8] dual-bank buffers and related save state.
void Mc_InitBufferSlots(void);

#endif // MAIN_MC_H
