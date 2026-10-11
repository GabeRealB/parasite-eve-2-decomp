#ifndef SRC_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_SHELTER_B1_POD_SERVICE_GANTRY_PRIVATE_H
#define SRC_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_SHELTER_B1_POD_SERVICE_GANTRY_PRIVATE_H

#include "gameplay/area_flags.h"

/// Saved-area changes installed when the gantry's second scene is spawned.
///
/// Ten updates and AREA_APPLY_END. Sets both pod-access tunnels to variant 22
/// and the eight listed intervening rooms to variant 1 in every save mode,
/// clearing their map marks and saved enemy poses. The scene sequence borrows
/// this overlay-owned list for the synchronous update call.
extern AreaApplyRec gShelterB1PodServiceGantrySecondSceneAreaUpdates[11];

#endif // SRC_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_SHELTER_B1_POD_SERVICE_GANTRY_PRIVATE_H
