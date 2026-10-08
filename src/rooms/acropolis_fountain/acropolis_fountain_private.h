#ifndef SRC_ROOMS_ACROPOLIS_FOUNTAIN_ACROPOLIS_FOUNTAIN_PRIVATE_H
#define SRC_ROOMS_ACROPOLIS_FOUNTAIN_ACROPOLIS_FOUNTAIN_PRIVATE_H

#include "types.h"

extern u8 D_acropolis_fountain_80183BB0;

extern u8 D_acropolis_fountain_80183BB1;

/// Replaces the fountain's inspection trigger with its enabled climb interaction.
///
/// Requires the loaded room's initialized action-trigger list and view coordinate.
/// Unlinks the CAP-command trigger, then parents and links the climb callback
/// trigger. Repeated linking is supported. The room records remain live while
/// linked; the fountain progress flag is managed by the caller.
void acropolisFountainEnableClimbTrigger(void);

#endif // SRC_ROOMS_ACROPOLIS_FOUNTAIN_ACROPOLIS_FOUNTAIN_PRIVATE_H
