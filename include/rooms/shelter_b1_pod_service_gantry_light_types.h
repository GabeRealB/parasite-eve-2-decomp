#ifndef INCLUDE_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_LIGHT_TYPES_H
#define INCLUDE_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_LIGHT_TYPES_H

#include "common.h"

#include "gameplay/light.h"

/// The pod-service gantry's cone-light array and adjacent uninterpreted data.
///
/// The `shelter_b1_pod_service_gantry` and `shelter_r47` room-light collections
/// borrow only `coneLights`. Its transforms and attenuation remain writable
/// while the owning overlay is loaded. The remaining bytes are outside the
/// light count; their original type, boundaries and relation to the lights
/// are unproven.
typedef struct {
    WorldCoordSpotLight coneLights[2];   // Mutable cone lights, both contributing in every view
    u8                  unknown_D8[756]; // Uninterpreted bytes; purpose and internal boundaries unproven
} ShelterB1PodServiceGantrySpotLightStorage;
STATIC_ASSERT_SIZEOF(ShelterB1PodServiceGantrySpotLightStorage, 972);

#endif // INCLUDE_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_LIGHT_TYPES_H
