#ifndef INCLUDE_STAGES_AKROPOLIS_ACTORS_HELICOPTER_DEPARTURE_H
#define INCLUDE_STAGES_AKROPOLIS_ACTORS_HELICOPTER_DEPARTURE_H

#include "gameplay/animation.h"

#include "main/task_types.h"

// Native animation sets referenced by room script banks.
extern AnimationSet gAkropolisHelicopterLandingPadPlayerHelipadReportEscapeClip;

extern AnimationSet gAkropolisHelicopterLandingPadPlayerHelipadDetonatorTalkClip;

/// Helicopter TMD recipes: 0 hull, 1 main rotor, 2 tail rotor, 3 searchlight.
///
/// All run at priority `TASK_PRIORITY_CONTROL` and defer primitive-buffer allocation. The hull's
/// first show message spawns entries 1..3 with their index in spawn argument 1
/// and a borrowed hull `Task*` in argument 2. Children borrow its coordinates
/// and lighting through teardown. The table has no terminator; keep the actor
/// package and model data loaded while these tasks live.
extern TaskDesc gHelicopterDepartureHelicopterTaskDescriptors[4];

/// Rupert Broderick TMD recipes: 0 body, 1 Mongoose revolver, 2 second held prop.
///
/// All run at priority `TASK_PRIORITY_CONTROL`; only the body defers primitive-buffer allocation.
/// Entry 0 borrows its owning enemy in spawn argument 2. Its initialization
/// spawns entries 1/2 with body-part indices 8/12 in argument 1 and a borrowed
/// body `Task*` in argument 2. Children borrow the body's coordinates and
/// lighting through teardown. The table has no terminator; keep the actor
/// package and model data loaded while these tasks live.
extern TaskDesc gHelicopterDepartureRupertTaskDescriptors[3];

/// No. 9 GOLEM TMD recipes: 0 body, 1 part-8 model, 2 part-3 model, 3 part-12 prop.
///
/// All run at priority `TASK_PRIORITY_ACTOR` with automatic model buffers. Spawn entry 0 through
/// the enemy system with its owning enemy in spawn argument 2. It creates
/// entries 1..3 as child enemies whose tasks borrow the body's coordinates and
/// work-owned lighting. The table has no terminator; keep the actor package
/// and model data loaded through enemy teardown.
extern TaskDesc gHelicopterDepartureNo9GolemTaskDescriptors[4];

#endif // INCLUDE_STAGES_AKROPOLIS_ACTORS_HELICOPTER_DEPARTURE_H
