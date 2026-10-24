#ifndef SRC_ROOMS_AKROPOLIS_FORKED_ROAD_AKROPOLIS_FORKED_ROAD_PRIVATE_H
#define SRC_ROOMS_AKROPOLIS_FORKED_ROAD_AKROPOLIS_FORKED_ROAD_PRIVATE_H

#include "gameplay/evs.h"

#include "main/task_types.h"

/// Indices in `gAkropolisForkedRoadMovieTaskDescriptors`; slot 1 has no callback.
enum {
    AKROPOLIS_FORKED_ROAD_TASK_OUTBOUND_MOVIE = 0,
    AKROPOLIS_FORKED_ROAD_TASK_UNUSED_MOVIE   = 1,
    AKROPOLIS_FORKED_ROAD_TASK_RETURN_MOVIE   = 2,
    AKROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_OUT  = 3,
    AKROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_IN   = 4,
    AKROPOLIS_FORKED_ROAD_MOVIE_TASK_COUNT    = 5,
};

/// Bodyless movie-path and return-skip fade controllers for the forked road.
extern TaskDesc gAkropolisForkedRoadMovieTaskDescriptors[AKROPOLIS_FORKED_ROAD_MOVIE_TASK_COUNT];

/// Once-only encounter sequence after the second security lock is released.
extern EvsCommand gAkropolisForkedRoadLockReleaseScript[8];

#endif // SRC_ROOMS_AKROPOLIS_FORKED_ROAD_AKROPOLIS_FORKED_ROAD_PRIVATE_H
