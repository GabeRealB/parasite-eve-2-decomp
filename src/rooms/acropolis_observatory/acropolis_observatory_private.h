#ifndef SRC_ROOMS_ACROPOLIS_OBSERVATORY_ACROPOLIS_OBSERVATORY_PRIVATE_H
#define SRC_ROOMS_ACROPOLIS_OBSERVATORY_ACROPOLIS_OBSERVATORY_PRIVATE_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/pad_script.h"

#include "main/task_types.h"

/// Four bodyless priority-192 recipes for the observatory arrival movies.
///
/// Entries 0/1 run forked-road/promenade arrival paths; 2/3 fade out/in on
/// movie skip. All spawns pass zero payloads. This is an indexed table without
/// a terminator; keep the room code and movie resources loaded through teardown.
extern TaskDesc gAcropolisObservatoryArrivalTaskDescs[4];

extern AnimationSet gDryfieldTrailerCoachPlayerEvent58Clip;

/// Direct player animation-set directory for the scripted battle handoff.
///
/// Entry 0 is absent; animation ID 1 selects the observatory's set in entry 1.
/// The scene installs this table into the player and may request the same set
/// for its idle follow-up. Playback borrows the table, records and pose banks;
/// keep this overlay loaded until the player's ordinary animation bank is restored.
/// The table has exactly two entries and no terminator.
extern AnimationSet* gAcropolisObservatoryPlayerAnimationSets[2];

/// Bodyless priority-192 recipe for the room-2 scripted player/battle handoff.
///
/// Direct spawning uses entry 0 and zero payloads; the callback owns its scene
/// work and captures the player. Keep this room overlay loaded until task teardown.
extern TaskDesc gAcropolisObservatoryScriptedBattleTaskDesc;

/// Extents of each arrival's separate command and vibration-segment arrays.
enum {
    ACROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_COMMAND_COUNT = 6,
    ACROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_SEGMENT_COUNT = 2,
};

/// Forked-road arrival's port-0 vibration program: twenty cycles of two binary-motor holds.
///
/// The binary lane plays segment 0, waits two script frames, plays segment 1
/// and repeats from step 0 using a loop operand of 19, then stops at step 5.
/// The variable lane stops at step 0 (its operand 1 is ignored). Both the
/// commands and paired segments are borrowed until the arrival's child task ends.
extern PadScriptCmd gAcropolisObservatoryForkedRoadArrivalVibrationCommands[ACROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_COMMAND_COUNT];

/// Two binary-motor hold segments for the forked-road arrival: two frames and one frame.
///
/// PLAY operands 0 and 1 index these entries. Intensity endpoints 0 and 60 are
/// stored but unused by the binary lane. Keep this overlay loaded through playback.
extern PadScriptVibrationSegment gAcropolisObservatoryForkedRoadArrivalVibrationSegments[ACROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_SEGMENT_COUNT];

/// Promenade arrival's port-0 vibration program: twenty cycles of two binary-motor holds.
///
/// Plays segment 0, waits two script frames, plays segment 1, then loops to step
/// 0 with operand 19 before stopping at step 5. The variable lane stops at step
/// 0 with an ignored operand 1. Borrows this program and its segment table until
/// the arrival's child script task ends.
extern PadScriptCmd gAcropolisObservatoryPromenadeArrivalVibrationCommands[ACROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_COMMAND_COUNT];

/// Two binary-motor hold segments for the promenade arrival: two frames and one frame.
///
/// PLAY operands 0 and 1 index these entries. Intensity endpoints 0 and 60 are
/// stored but unused by the binary lane. Keep this overlay loaded through playback.
extern PadScriptVibrationSegment gAcropolisObservatoryPromenadeArrivalVibrationSegments[ACROPOLIS_OBSERVATORY_ARRIVAL_VIBRATION_SEGMENT_COUNT];

#endif // SRC_ROOMS_ACROPOLIS_OBSERVATORY_ACROPOLIS_OBSERVATORY_PRIVATE_H
