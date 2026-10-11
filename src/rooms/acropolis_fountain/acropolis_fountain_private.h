#ifndef SRC_ROOMS_ACROPOLIS_FOUNTAIN_ACROPOLIS_FOUNTAIN_PRIVATE_H
#define SRC_ROOMS_ACROPOLIS_FOUNTAIN_ACROPOLIS_FOUNTAIN_PRIVATE_H

#include "types.h"

/// Arrival warp latched for the deferred departure to patio room 3.
///
/// An executing transition request stores its full unsigned warp byte before
/// spawning departure; the reload copies it into the live save. The pending
/// departure requires one writer and a valid patio endpoint (1..4). The latch
/// is retained after consumption and belongs to the loaded fountain overlay.
extern u8 gAcropolisFountainPatioArrivalWarp;

/// Requested patio room byte retained when a deferred departure is accepted.
///
/// The executing request replaces this singleton latch before task spawn. No
/// recovered code reads it: departure selects patio room 3 independently.
/// Kept separately from the arrival warp and reset by loading the room image.
extern u8 gAcropolisFountainPatioRequestedRoom;

/// Replaces the fountain's inspection trigger with its enabled climb interaction.
///
/// Requires the loaded room's initialized action-trigger list and view coordinate.
/// Unlinks the CAP-command trigger, then parents and links the climb callback
/// trigger. Repeated linking is supported. The room records remain live while
/// linked; the fountain progress flag is managed by the caller.
void acropolisFountainEnableClimbTrigger(void);

#endif // SRC_ROOMS_ACROPOLIS_FOUNTAIN_ACROPOLIS_FOUNTAIN_PRIVATE_H
