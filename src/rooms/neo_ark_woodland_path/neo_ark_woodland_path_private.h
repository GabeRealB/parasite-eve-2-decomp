#ifndef SRC_ROOMS_NEO_ARK_WOODLAND_PATH_NEO_ARK_WOODLAND_PATH_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_WOODLAND_PATH_NEO_ARK_WOODLAND_PATH_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "main/task_types.h"

/// Last sampled positions of player model parts 15 and 18 for water emission.
///
/// Entries 0 and 1 retain cached composed XYZ in signed halfword game units.
/// The splash task seeds both on its first tick, then advances them only while
/// emitting; suspension preserves the previous samples. Storage is shared by
/// the room's singleton splash task for the overlay's lifetime.
extern SVECTOR gNeoArkWoodlandPathPlayerWaterPartHistory[2];

extern TaskDesc D_neo_ark_woodland_path_80184A44[2];

#endif // SRC_ROOMS_NEO_ARK_WOODLAND_PATH_NEO_ARK_WOODLAND_PATH_PRIVATE_H
