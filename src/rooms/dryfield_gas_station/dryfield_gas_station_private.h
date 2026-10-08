#ifndef SRC_ROOMS_DRYFIELD_GAS_STATION_DRYFIELD_GAS_STATION_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_GAS_STATION_DRYFIELD_GAS_STATION_PRIVATE_H

#include "main/task_types.h"

#include "rooms/room.h"

/// Descriptors of the tasks the gas station's sequencer spawns: entry 0 is the
/// cutscene task the sequencer waits on, entry 1 the fade the cutscene's last
/// script command starts.
extern TaskDesc D_dryfield_gas_station_8018312C[];

extern TaskDesc D_dryfield_gas_station_80181E7C[3];

extern Task* D_dryfield_gas_station_80184BCC;

extern Task* gRoomCutsceneSoundTask;

extern RoomCutsceneRec D_dryfield_gas_station_80184BD8;

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
