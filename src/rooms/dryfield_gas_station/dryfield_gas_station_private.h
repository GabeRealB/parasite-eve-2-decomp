#ifndef SRC_ROOMS_DRYFIELD_GAS_STATION_DRYFIELD_GAS_STATION_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_GAS_STATION_DRYFIELD_GAS_STATION_PRIVATE_H

#include "main/task_types.h"

#include "rooms/room.h"

/// Two bodyless arrival-cutscene recipes at priority `TASK_PRIORITY_CONTROL`, without an end record.
///
/// Entry 0 starts the in-room controller with ignored spawn arguments; entry 1
/// starts its fade-in using the unsigned low halfword of spawnArg1 as intensity
/// units removed per update (zero holds). Spawning copies the recipe; keep the
/// callbacks and borrowed scene resources loaded until exit.
extern TaskDesc gDryfieldGasStationArrivalCutsceneTaskDescs[2];

/// Bodyless warp-1 arrival recipes at priority `TASK_PRIORITY_CONTROL`, without an end record.
///
/// Entry 0 owns the movie/cutscene sequence; entry 1 runs the arrival movie on
/// the display list; both ignore initial spawn arguments. Entry 2 removes
/// spawnArg1's unsigned low-halfword intensity step per update (zero holds).
/// Spawning copies the recipe; callbacks and their room/movie resources must
/// remain loaded through completion.
extern TaskDesc gDryfieldGasStationArrivalTaskDescs[3];

/// Arrival-sequence child handle retained by the room's arrival supervisor.
///
/// NULL until spawn; successful polling releases the child without clearing
/// this borrowed handle. Use it only while that child remains live.
extern Task* gDryfieldGasStationArrivalTask;

extern Task* gRoomCutsceneSoundTask;

/// Mutable record borrowed by the room-command cutscene after the first interaction.
///
/// Command 1 stages view 8, CAP slot/file 1 and start/end/scene/follow-up sounds,
/// with scene playback enabled. Zero-initialized texture coordinates choose
/// the default CAP page at (960, 0). Another request overwrites this record;
/// leave it intact and keep the overlay loaded until its runner finishes.
extern RoomCutsceneRec gDryfieldGasStationRoomCutscene;

// Callbacks referenced by the overlay's shared data tables.
/// Plays the warp-1 arrival movie, then restores game resources and the display loop.
///
/// Starts in state 0 as the display callback. Uses resource view 100 and saves
/// VRAM images; Start cancels playback. Music starts at movie tick 390 or on
/// an earlier skip. `killCountdown` counts playback ticks, and `spawnArg1.value`
/// becomes the music-started latch. Restores sprite images, clears the complete
/// resident frame workspace, starts an eight-frame fade and releases itself.
/// Keep room/movie resources available until restoration finishes.
void dryfieldGasStationArrivalMovieTask(Task* task);

/// Owns the gas-station arrival movie and its in-room cutscene child.
///
/// Start in state 0. Allocates work, transfers presentation to the movie task
/// and queues the current view; two intervening ticks precede cutscene spawn.
/// Allocation failure kills immediately, child-spawn failure requests removal,
/// and a child's kill request is collected before this owner requests removal.
/// The child result is ignored. Keep room and scene resources loaded throughout.
void dryfieldGasStationArrivalTask(Task* task);

#endif // SRC_ROOMS_DRYFIELD_GAS_STATION_DRYFIELD_GAS_STATION_PRIVATE_H
