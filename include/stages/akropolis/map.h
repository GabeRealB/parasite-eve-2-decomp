#ifndef INCLUDE_STAGES_AKROPOLIS_MAP_H
#define INCLUDE_STAGES_AKROPOLIS_MAP_H

#include "main/tmd_types.h"

// Each shape is an area's flat outline in the model X/Z plane, white and without normals. The map scales a
// page's outlines by 2107/4096 and maps Z to inverted screen Y, and borrows their vertices and first stream
// group: the page's package stays loaded while the map shows it.

// akropolis_map: pages 0 and 1 of the Akropolis map
/// Square's three-quad area fill on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapSquareShape;

/// East elevator hall's area-fill mesh on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapEastElevatorHallShape;

/// Patio's area-fill mesh on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapPatioShape;

/// Cafeteria's two-quad area fill on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapCafeteriaShape;

/// Security room's area-fill quad on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapSecurityRoomShape;

/// Hallway's area-fill quad on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapHallwayShape;

/// Fountain's area-fill mesh on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapFountainShape;

/// Forked road's area-fill mesh on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapForkedRoadShape;

/// Observatory's area-fill mesh on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapObservatoryShape;

/// Promenade's area-fill mesh on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapPromenadeShape;

/// Sanctuary's area-fill quad on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapSanctuaryShape;

/// Roof garden's area-fill quad on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapRoofGardenShape;

/// Bridge's area-fill quad on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapBridgeShape;

/// Fire escape's area-fill quad on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapFireEscapeShape;

/// West elevator hall's area-fill mesh on page 1 of the Akropolis map.
extern TmdSource gAkropolisMapWestElevatorHallShape;

// akropolis_helicopter_landing_pad_map
/// Helicopter landing pad's area-fill quad on page 2 of the Akropolis map.
extern TmdSource gAkropolisHelicopterLandingPadMapShape;

#endif // INCLUDE_STAGES_AKROPOLIS_MAP_H
