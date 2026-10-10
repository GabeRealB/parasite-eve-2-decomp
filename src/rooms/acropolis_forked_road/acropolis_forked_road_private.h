#ifndef SRC_ROOMS_ACROPOLIS_FORKED_ROAD_ACROPOLIS_FORKED_ROAD_PRIVATE_H
#define SRC_ROOMS_ACROPOLIS_FORKED_ROAD_ACROPOLIS_FORKED_ROAD_PRIVATE_H

#include "gameplay/evs.h"

#include "main/task_types.h"

/// Indices in `gAcropolisForkedRoadMovieTaskDescriptors`; slot 1 has no callback.
enum {
    ACROPOLIS_FORKED_ROAD_TASK_OUTBOUND_MOVIE = 0,
    ACROPOLIS_FORKED_ROAD_TASK_UNUSED_MOVIE   = 1,
    ACROPOLIS_FORKED_ROAD_TASK_RETURN_MOVIE   = 2,
    ACROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_OUT  = 3,
    ACROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_IN   = 4,
    ACROPOLIS_FORKED_ROAD_MOVIE_TASK_COUNT    = 5,
};

/// Bodyless movie-path and return-skip fade controllers for the forked road.
///
/// Indexed by the constants above, with no terminator. Slot 1 is retained but
/// cannot be spawned. The descriptors and callbacks are borrowed from this
/// overlay and must remain loaded for each spawned task's lifetime.
extern TaskDesc gAcropolisForkedRoadMovieTaskDescriptors[ACROPOLIS_FORKED_ROAD_MOVIE_TASK_COUNT];

/// Once-only encounter sequence after the second security lock is released.
///
/// Eight instructions, ending at `EVENT_SCRIPT_OPCODE_END`: extends the equipped
/// weapon's animation bank, plays clip 48, releases the scripted Maggot/Caterpillar
/// entrances, then engages battle and resumes player control with the root offset
/// retained. Wait counts are interpreter updates. Borrow the script until it ends;
/// its copied animation-set pointers require this overlay to stay loaded afterwards.
extern EvsCommand gAcropolisForkedRoadLockReleaseScript[8];

#endif // SRC_ROOMS_ACROPOLIS_FORKED_ROAD_ACROPOLIS_FORKED_ROAD_PRIVATE_H
