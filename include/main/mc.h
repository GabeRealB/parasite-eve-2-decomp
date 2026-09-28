#ifndef MAIN_MC_H
#define MAIN_MC_H

#include "main/mc_types.h"

extern McSaveData Mc_SaveData[2];

void Mc_ResetSaveFlags(void);

/// Init Mc_BufferSlots[1..8] dual-bank buffers and related save state.
void Mc_InitBufferSlots(void);

#endif // MAIN_MC_H
