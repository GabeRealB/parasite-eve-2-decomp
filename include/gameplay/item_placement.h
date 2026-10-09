#ifndef GAMEPLAY_ITEM_PLACEMENT_H
#define GAMEPLAY_ITEM_PLACEMENT_H

#include "gameplay/area_flags.h"

/// Dryfield gas station object placements, shared by day and night.
///
/// Contains 6 objects followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
/// Includes the replenishable 9mm ammunition supply, gasoline, item box and
/// save point.
extern AreaObjectPlace gAreaDryfieldGasStationObjectPlaces[7];

/// Dryfield main street object placements, shared by day and night.
///
/// Contains 2 objects followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
/// Includes the truck key and Ice Bag.
extern AreaObjectPlace gAreaDryfieldMainStreetObjectPlaces[3];

/// Dryfield general store object placements, shared by day and night.
///
/// Contains 2 objects followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
extern AreaObjectPlace gAreaDryfieldGeneralStoreObjectPlaces[3];

/// Dryfield back street object placements, shared by day and night.
///
/// Contains 1 object followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
extern AreaObjectPlace gAreaDryfieldBackStreetObjectPlaces[2];

/// Dryfield souvenir shop object placements, shared by day and night.
///
/// Contains 1 object followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
extern AreaObjectPlace gAreaDryfieldSouvenirShopObjectPlaces[2];

/// Empty Dryfield warehouse placement list, shared by day and night.
///
/// The sole `AREA_OBJECT_PLACE_END` record seeds no states and spawns no objects.
extern AreaObjectPlace gAreaDryfieldWarehouseObjectPlaces[1];

/// Dryfield motel room 2 object placements, shared by day and night.
///
/// Contains 2 objects followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
extern AreaObjectPlace gAreaDryfieldMotelRoom2ObjectPlaces[3];

/// Dryfield motel room 3 object placements, shared by day and night.
///
/// Contains 1 object followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
extern AreaObjectPlace gAreaDryfieldMotelRoom3ObjectPlaces[2];

/// Dryfield parking lot object placements, shared by day and night.
///
/// Contains 1 object followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
extern AreaObjectPlace gAreaDryfieldParkingLotObjectPlaces[2];

/// Dryfield motel lobby object placements, shared by day and night.
///
/// Contains 2 objects followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
/// Includes the Bronco Masterkey and save point.
/// State bit 8 is preserved; its purpose is unproven.
extern AreaObjectPlace gAreaDryfieldMotelLobbyObjectPlaces[3];

/// Dryfield Saloon G & R object placements, shared by day and night.
///
/// Contains 1 object followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
extern AreaObjectPlace gAreaDryfieldSaloonGRObjectPlaces[2];

/// Dryfield G & R kitchen object placements, shared by day and night.
///
/// Contains 2 objects followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
/// Includes the Bottlecap Magnet.
extern AreaObjectPlace gAreaDryfieldGRKitchenObjectPlaces[3];

/// Dryfield water tank object placements, shared by day and night.
///
/// Contains 6 objects followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
/// Includes the Saloon Key and SUV Key.
extern AreaObjectPlace gAreaDryfieldWaterTankObjectPlaces[7];

/// Dryfield breezeway object placements, shared by day and night.
///
/// Contains 1 object followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
/// Includes the Factory Key.
extern AreaObjectPlace gAreaDryfieldBreezewayObjectPlaces[2];

/// Dryfield factory object placements, shared by day and night.
///
/// Contains 1 object followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
extern AreaObjectPlace gAreaDryfieldFactoryObjectPlaces[2];

/// Dryfield garage object placements, shared by day and night.
///
/// Contains 3 objects followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
/// Includes the Motel Key No.6, Monkey Wrench and item box.
extern AreaObjectPlace gAreaDryfieldGarageObjectPlaces[4];

/// Dryfield driveway object placements, shared by day and night.
///
/// Contains 1 object followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
extern AreaObjectPlace gAreaDryfieldDrivewayObjectPlaces[2];

/// Dryfield junk yard object placements, shared by day and night.
///
/// Contains 2 objects followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
/// The wire rope has parent-frame XYZ (1745, 0, 280); its model spawns only by
/// day.
/// State bit 8 is preserved; its purpose is unproven.
extern AreaObjectPlace gAreaDryfieldJunkYardObjectPlaces[3];

/// Dryfield trailer coach object placements, shared by day and night.
///
/// Contains 3 objects followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
/// Includes the Lobby Key, save point and initially collected container lid.
/// The lid uses the M950 kind at parent-frame XYZ (4128, 0, -256), yaw half a
/// turn.
/// State bit 8 is preserved; its purpose is unproven.
extern AreaObjectPlace gAreaDryfieldTrailerCoachObjectPlaces[4];

/// Dryfield motel room 5 object placements, shared by day and night.
///
/// Contains 1 object followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
extern AreaObjectPlace gAreaDryfieldMotelRoom5ObjectPlaces[2];

/// Dryfield motel room 6 object placements, shared by day and night.
///
/// Contains 2 objects followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
/// Includes an item box and save point.
/// State bit 8 is preserved; its purpose is unproven.
extern AreaObjectPlace gAreaDryfieldMotelRoom6ObjectPlaces[3];

/// Dryfield motel loft object placements, shared by day and night.
///
/// Contains 2 objects followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
/// The Jerry Can model spawns only at night, at parent-frame XYZ (-5950, -500,
/// -20).
extern AreaObjectPlace gAreaDryfieldMotelLoftObjectPlaces[3];

/// Dryfield water hole object placements, shared by day and night.
///
/// Contains 1 object followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
extern AreaObjectPlace gAreaDryfieldWaterHoleObjectPlaces[2];

/// Dryfield cellar object placements, shared by day and night.
///
/// Contains 2 objects followed by `AREA_OBJECT_PLACE_END`. Both stage maps borrow
/// this gameplay-owned list for saved-state seeding, pickup lookup and spawning.
extern AreaObjectPlace gAreaDryfieldCellarObjectPlaces[3];

#endif // GAMEPLAY_ITEM_PLACEMENT_H
