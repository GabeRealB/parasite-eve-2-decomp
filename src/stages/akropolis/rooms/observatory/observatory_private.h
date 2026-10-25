#ifndef STAGES_AKROPOLIS_ROOMS_OBSERVATORY_OBSERVATORY_PRIVATE_H
#define STAGES_AKROPOLIS_ROOMS_OBSERVATORY_OBSERVATORY_PRIVATE_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/pad_script.h"

#include "main/task_types.h"

/// Four bodyless priority-192 recipes for the observatory arrival movies.
extern TaskDesc gAkropolisObservatoryArrivalTaskDescs[4];

extern AnimationSet gDryfieldTrailerCoachPlayerEvent58Clip;

/// Direct player animation-set directory for the scripted battle handoff.
extern AnimationSet* gAkropolisObservatoryPlayerAnimationSets[2];

/// Bodyless priority-192 recipe for the room-2 scripted player/battle handoff.
extern TaskDesc gAkropolisObservatoryScriptedBattleTaskDesc;

/// Extents of each arrival's separate command and vibration-segment arrays.
enum {
    AKROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_COMMAND_COUNT = 6,
    AKROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_SEGMENT_COUNT = 2,
};

/// Forked-road arrival's port-0 vibration program: twenty cycles of two binary-motor holds.
extern PadScriptCmd gAkropolisObservatoryForkedRoadArrivalVibrationCommands[AKROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_COMMAND_COUNT];

/// Two binary-motor hold segments for the forked-road arrival: two frames and one frame.
extern PadScriptVibrationSegment gAkropolisObservatoryForkedRoadArrivalVibrationSegments[AKROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_SEGMENT_COUNT];

/// Promenade arrival's port-0 vibration program: twenty cycles of two binary-motor holds.
extern PadScriptCmd gAkropolisObservatoryPromenadeArrivalVibrationCommands[AKROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_COMMAND_COUNT];

/// Two binary-motor hold segments for the promenade arrival: two frames and one frame.
extern PadScriptVibrationSegment gAkropolisObservatoryPromenadeArrivalVibrationSegments[AKROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_SEGMENT_COUNT];

#endif // STAGES_AKROPOLIS_ROOMS_OBSERVATORY_OBSERVATORY_PRIVATE_H
