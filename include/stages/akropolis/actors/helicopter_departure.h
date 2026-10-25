#ifndef INCLUDE_STAGES_AKROPOLIS_ACTORS_HELICOPTER_DEPARTURE_H
#define INCLUDE_STAGES_AKROPOLIS_ACTORS_HELICOPTER_DEPARTURE_H

#include "gameplay/animation.h"

#include "main/task_types.h"

// Native animation sets referenced by room script banks.
extern AnimationSet gAkropolisHelicopterLandingPadPlayerHelipadReportEscapeClip;

extern AnimationSet gAkropolisHelicopterLandingPadPlayerHelipadDetonatorTalkClip;

/// Helicopter TMD recipes: 0 hull, 1 main rotor, 2 tail rotor, 3 searchlight.
extern TaskDesc gHelicopterDepartureHelicopterTaskDescriptors[4];

/// Rupert Broderick TMD recipes: 0 body, 1 Mongoose revolver, 2 second held prop.
extern TaskDesc gHelicopterDepartureRupertTaskDescriptors[3];

/// No. 9 GOLEM TMD recipes: 0 body, 1 part-8 model, 2 part-3 model, 3 part-12 prop.
extern TaskDesc gHelicopterDepartureNo9GolemTaskDescriptors[4];

#endif // INCLUDE_STAGES_AKROPOLIS_ACTORS_HELICOPTER_DEPARTURE_H
