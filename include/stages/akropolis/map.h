#ifndef INCLUDE_STAGES_AKROPOLIS_MAP_H
#define INCLUDE_STAGES_AKROPOLIS_MAP_H

#include "main/tmd_types.h"

// akropolis_map: pages 0 and 1 of the Akropolis map
/// Square's three-quad area fill on page 1 of the Acropolis map.
///
/// Eight vertices in the model X/Z plane are scaled by 2107/4096, with Z
/// mapped to inverted screen Y. Has no normal data. The map borrows the
/// vertices and first stream group; keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapSquareShape;

/// East elevator hall's area-fill mesh on page 1 of the Acropolis map.
///
/// Sixteen vertices in the model X/Z plane form two triangles and six quads;
/// the map scales them by 2107/4096 and maps Z to inverted screen Y.
/// Has no normal data. The map borrows the vertices and first stream group;
/// keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapEastElevatorHallShape;

/// Patio's area-fill mesh on page 1 of the Acropolis map.
///
/// Ten vertices in the model X/Z plane form two triangles and three quads;
/// the map scales them by 2107/4096 and maps Z to inverted screen Y.
/// Has no normal data. The map borrows the vertices and first stream group;
/// keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapPatioShape;

/// Cafeteria's two-quad area fill on page 1 of the Acropolis map.
///
/// Six vertices in the model X/Z plane are scaled by 2107/4096, with Z
/// mapped to inverted screen Y. Has no normal data. The map borrows the
/// vertices and first stream group; keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapCafeteriaShape;

/// Security room's area-fill quad on page 1 of the Acropolis map.
///
/// Four vertices in the model X/Z plane are scaled by 2107/4096, with Z
/// mapped to inverted screen Y. Has no normal data. The map borrows the
/// vertices and first stream group; keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapSecurityRoomShape;

/// Hallway's area-fill quad on page 1 of the Acropolis map.
///
/// Four vertices in the model X/Z plane are scaled by 2107/4096, with Z
/// mapped to inverted screen Y. Has no normal data. The map borrows the
/// vertices and first stream group; keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapHallwayShape;

/// Fountain's area-fill mesh on page 1 of the Acropolis map.
///
/// Eleven vertices in the model X/Z plane form one triangle and four quads;
/// the map scales them by 2107/4096 and maps Z to inverted screen Y.
/// Has no normal data. The map borrows the vertices and first stream group;
/// keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapFountainShape;

/// Forked road's area-fill mesh on page 1 of the Acropolis map.
///
/// Eight vertices in the model X/Z plane form two triangles and two quads;
/// the map scales them by 2107/4096 and maps Z to inverted screen Y.
/// Has no normal data. The map borrows the vertices and first stream group;
/// keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapForkedRoadShape;

/// Observatory's area-fill mesh on page 1 of the Acropolis map.
///
/// Five vertices in the model X/Z plane form one triangle and one quad;
/// the map scales them by 2107/4096 and maps Z to inverted screen Y.
/// Has no normal data. The map borrows the vertices and first stream group;
/// keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapObservatoryShape;

/// Promenade's area-fill mesh on page 1 of the Acropolis map.
///
/// Nine vertices in the model X/Z plane form one triangle and three quads;
/// the map scales them by 2107/4096 and maps Z to inverted screen Y.
/// Has no normal data. The map borrows the vertices and first stream group;
/// keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapPromenadeShape;

/// Sanctuary's area-fill quad on page 1 of the Acropolis map.
///
/// Four vertices in the model X/Z plane are scaled by 2107/4096, with Z
/// mapped to inverted screen Y. Has no normal data. The map borrows the
/// vertices and first stream group; keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapSanctuaryShape;

/// Roof garden's area-fill quad on page 1 of the Acropolis map.
///
/// Four vertices in the model X/Z plane are scaled by 2107/4096, with Z
/// mapped to inverted screen Y. Has no normal data. The map borrows the
/// vertices and first stream group; keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapRoofGardenShape;

/// Bridge's area-fill quad on page 1 of the Acropolis map.
///
/// Four vertices in the model X/Z plane are scaled by 2107/4096, with Z
/// mapped to inverted screen Y. Has no normal data. The map borrows the
/// vertices and first stream group; keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapBridgeShape;

/// Fire escape's area-fill quad on page 1 of the Acropolis map.
///
/// Four vertices in the model X/Z plane are scaled by 2107/4096, with Z
/// mapped to inverted screen Y. Has no normal data. The map borrows the
/// vertices and first stream group; keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapFireEscapeShape;

/// West elevator hall's area-fill mesh on page 1 of the Acropolis map.
///
/// Sixteen vertices in the model X/Z plane form two triangles and six quads;
/// the map scales them by 2107/4096 and maps Z to inverted screen Y.
/// Has no normal data. The map borrows the vertices and first stream group;
/// keep akropolis_map loaded during use.
extern TmdSource gAkropolisMapWestElevatorHallShape;

// akropolis_helicopter_landing_pad_map
/// Helicopter landing pad's area-fill quad on page 2 of the Acropolis map.
///
/// Four vertices in the model X/Z plane are scaled by 2107/4096, with Z
/// mapped to inverted screen Y. Has no normal data. The map borrows the
/// vertices and first stream group; keep akropolis_helicopter_landing_pad_map loaded during use.
extern TmdSource gAkropolisHelicopterLandingPadMapShape;

#endif // INCLUDE_STAGES_AKROPOLIS_MAP_H
