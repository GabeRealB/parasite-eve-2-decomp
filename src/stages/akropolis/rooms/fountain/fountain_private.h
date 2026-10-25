#ifndef STAGES_AKROPOLIS_ROOMS_FOUNTAIN_FOUNTAIN_PRIVATE_H
#define STAGES_AKROPOLIS_ROOMS_FOUNTAIN_FOUNTAIN_PRIVATE_H

#include "types.h"

/// Arrival warp latched for the deferred departure to patio room 3.
extern u8 gAkropolisFountainPatioArrivalWarp;

/// Requested patio room byte retained when a deferred departure is accepted.
extern u8 gAkropolisFountainPatioRequestedRoom;

/// Replaces the fountain's inspection trigger with its enabled climb interaction.
void akropolisFountainEnableClimbTrigger(void);

#endif // STAGES_AKROPOLIS_ROOMS_FOUNTAIN_FOUNTAIN_PRIVATE_H
