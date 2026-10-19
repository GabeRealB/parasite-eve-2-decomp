#include "mapui/map_shelter.h"

#include <psyq/sys/types.h>

#include "types.h"

#include "actors/actor_503500.h"

#include "gameplay/area_flags.h"
#include "gameplay/battle_reward.h"
#include "gameplay/direction.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/map.h"
#include "gameplay/object_task.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gfx_types.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/task_types.h"

#include "mappic/mappic.h"

#include "rooms/mine_cavern.h"

#include "rooms/mine_forked_tunnel.h"

#include "rooms/mine_gorge.h"

#include "rooms/mine_mesa.h"

#include "rooms/mine_refuge.h"

#include "rooms/mine_secret_passage.h"

#include "rooms/mine_tunnel.h"

#include "rooms/mine_tunnel_entrance.h"

#include "rooms/shelter_1f_heliport_s4.h"

#include "rooms/shelter_b1_access_tunnel.h"

#include "rooms/shelter_b1_armory.h"

#include "rooms/shelter_b1_control_room.h"

#include "rooms/shelter_b1_control_room_access_tunnel.h"

#include "rooms/shelter_b1_elevator_hall.h"

#include "rooms/shelter_b1_golem_freezer_1.h"

#include "rooms/shelter_b1_main_corridor.h"

#include "rooms/shelter_b1_north_maintenance_walkway.h"

#include "rooms/shelter_b1_pod_access_tunnel.h"

#include "rooms/shelter_b1_pod_service_gantry.h"

#include "rooms/shelter_b1_sleeping_quarters.h"

#include "rooms/shelter_b1_south_maintenance_walkway.h"

#include "rooms/shelter_b1_sterilization_room.h"

#include "rooms/shelter_b1_storeroom.h"

#include "rooms/shelter_b1_transfer_tunnel.h"

#include "rooms/shelter_b1_underground_parking.h"

#include "rooms/shelter_b2_breeding_room.h"

#include "rooms/shelter_b2_elevator.h"

#include "rooms/shelter_b2_elevator_hall.h"

#include "rooms/shelter_b2_laboratory.h"

#include "rooms/shelter_b2_main_corridor.h"

#include "rooms/shelter_b2_north_maintenance_walkway.h"

#include "rooms/shelter_b2_operating_room.h"

#include "rooms/shelter_b2_pod_access_tunnel.h"

#include "rooms/shelter_b2_pod_bottom.h"

#include "rooms/shelter_b2_septic_tank.h"

#include "rooms/shelter_b2_south_maintenance_walkway.h"

#include "rooms/shelter_b3_dumping_hole.h"

#include "rooms/shelter_b3_elevator_hall.h"

#include "rooms/shelter_b3_garbage_incinerator.h"

#include "rooms/shelter_b3_incinerator_control_room.h"

#include "rooms/shelter_b4_lower_sewer.h"

#include "rooms/shelter_b4_reservoir.h"

#include "rooms/shelter_b4_upper_sewer.h"

#include "rooms/shelter_b4_water_supply.h"

#include "rooms/shelter_r36.h"

#include "rooms/shelter_r37.h"

#include "rooms/shelter_r47.h"

#include "rooms/shelter_r48.h"

#include "rooms/shelter_r49.h"

/// Binds the shared Mine/Shelter resolver to this overlay's public export.
///
/// Keep this s32 (RoomEventMsg*, RoomEventMsg*) identifier binding defined through
/// the shared header and fragment; it evaluates nothing and is undefined below.
#define ROOM_VARIANT_RESOLVE_SHELTER mapShelterRoomVariantResolve
#include "../../shared/room/room_variants.h"

/* The Mesa, mine and Shelter stage's map UI overlay: a hook the stage's rooms
 * call at this map's slot address, and the per-stage tables gameplay and main
 * index by stage. Most point into the stage's room packages or at the map
 * pictures' marker models; for some rooms the overlay carries the room's
 * coordinate and object records itself, and the flagged item and enemy
 * placement lists.
 */

// Forward declarations of the map-owned place lists referenced by the area directory.
static AreaObjectPlace _gMapShelterMineMesaObjectPlaces[2];
static AreaObjectPlace _gMapShelterMineCavernObjectPlaces[3];
static AreaObjectPlace _gMapShelterMineRefugeObjectPlaces[5];
static AreaObjectPlace _gMapShelterMineForkedTunnelObjectPlaces[3];
static AreaObjectPlace _gMapShelterB1StoreroomObjectPlaces[3];
static AreaObjectPlace _gMapShelterB1NorthMaintenanceWalkwayObjectPlaces[2];
static AreaObjectPlace _gMapShelterB1ArmoryObjectPlaces[5];
static AreaObjectPlace _gMapShelterB1SleepingQuartersObjectPlaces[2];
static AreaObjectPlace _gMapShelterB1SterilizationRoomObjectPlaces[3];
static AreaObjectPlace _gMapShelterB1UndergroundParkingObjectPlaces[15];
static AreaObjectPlace _gMapShelterB1GolemFreezer1ObjectPlaces[1];
static AreaObjectPlace _gMapShelterB2ElevatorObjectPlaces[2];
static AreaObjectPlace _gMapShelterB2OperatingRoomObjectPlaces[3];
static AreaObjectPlace _gMapShelterB2LaboratoryObjectPlaces[3];
static AreaObjectPlace _gMapShelterB2BreedingRoomObjectPlaces[5];
static AreaObjectPlace _gMapShelterB3DumpingHoleObjectPlaces[4];
static AreaObjectPlace _gMapShelterB3IncineratorControlRoomObjectPlaces[2];
static AreaObjectPlace _gMapShelterB4ReservoirObjectPlaces[2];
static AreaObjectPlace _gMapShelterR47ObjectPlaces[4];
static AreaObjectPlace _gMapShelterR48ObjectPlaces[3];

#include "../../shared/room/room_variants_shelter.inc.c"
#undef ROOM_VARIANT_RESOLVE_SHELTER

GfxImageSlot gMapShelterImageSlotsByArea[50] = {
    [0]                                               = GRAPHICS_IMAGE_SLOT(0x3A8E0),
    [GAME_AREA_MINE_MESA]                             = GRAPHICS_IMAGE_SLOT(0x4B5F0),
    [GAME_AREA_MINE_CAVERN]                           = GRAPHICS_IMAGE_SLOT(0x444B0),
    [GAME_AREA_MINE_TUNNEL_ENTRANCE]                  = GRAPHICS_IMAGE_SLOT(0x57100),
    [GAME_AREA_MINE_TUNNEL]                           = GRAPHICS_IMAGE_SLOT(0x56010),
    [GAME_AREA_MINE_GORGE]                            = GRAPHICS_IMAGE_SLOT(0x520D0),
    [GAME_AREA_MINE_REFUGE]                           = GRAPHICS_IMAGE_SLOT(0x4F6C0),
    [GAME_AREA_MINE_FORKED_TUNNEL]                    = GRAPHICS_IMAGE_SLOT(0x50950),
    [GAME_AREA_MINE_SECRET_PASSAGE]                   = GRAPHICS_IMAGE_SLOT(0x529D0),
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL]              = GRAPHICS_IMAGE_SLOT(0x518A0),
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY]  = GRAPHICS_IMAGE_SLOT(0x53440),
    [GAME_AREA_SHELTER_B1_STOREROOM]                  = GRAPHICS_IMAGE_SLOT(0x4E150),
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY]  = GRAPHICS_IMAGE_SLOT(0x50CB0),
    [GAME_AREA_SHELTER_B1_ARMORY]                     = GRAPHICS_IMAGE_SLOT(0x4ED60),
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS]          = GRAPHICS_IMAGE_SLOT(0x4EB50),
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR]              = GRAPHICS_IMAGE_SLOT(0x4FC60),
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM]         = GRAPHICS_IMAGE_SLOT(0x43FE0),
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL]          = GRAPHICS_IMAGE_SLOT(0x50B80),
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM]               = GRAPHICS_IMAGE_SLOT(0x518D0),
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL]              = GRAPHICS_IMAGE_SLOT(0x56480),
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING]        = GRAPHICS_IMAGE_SLOT(0x41CA0),
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1]            = GRAPHICS_IMAGE_SLOT(0x567F0),
    [GAME_AREA_SHELTER_B2_POD_BOTTOM]                 = GRAPHICS_IMAGE_SLOT(0x4C350),
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY]         = GRAPHICS_IMAGE_SLOT(0x50060),
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL]            = GRAPHICS_IMAGE_SLOT(0x53DC0),
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL] = GRAPHICS_IMAGE_SLOT(0x548D0),
    [GAME_AREA_SHELTER_B2_ELEVATOR]                   = GRAPHICS_IMAGE_SLOT(0x578A0),
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL]              = GRAPHICS_IMAGE_SLOT(0x518C0),
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY]  = GRAPHICS_IMAGE_SLOT(0x53240),
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM]             = GRAPHICS_IMAGE_SLOT(0x50B50),
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY]  = GRAPHICS_IMAGE_SLOT(0x4FC40),
    [GAME_AREA_SHELTER_B2_LABORATORY]                 = GRAPHICS_IMAGE_SLOT(0x478B0),
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM]              = GRAPHICS_IMAGE_SLOT(0x4D930),
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR]              = GRAPHICS_IMAGE_SLOT(0x4B480),
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK]                = GRAPHICS_IMAGE_SLOT(0x4F300),
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL]          = GRAPHICS_IMAGE_SLOT(0x50E70),
    [GAME_AREA_SHELTER_R36]                           = GRAPHICS_IMAGE_SLOT(0x53830),
    [GAME_AREA_SHELTER_R37]                           = GRAPHICS_IMAGE_SLOT(0x59070),
    [GAME_AREA_SHELTER_1F_HELIPORT_S4]                = GRAPHICS_IMAGE_SLOT(0x58EF0),
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE]               = GRAPHICS_IMAGE_SLOT(0x44E20),
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR]        = GRAPHICS_IMAGE_SLOT(0x417F0),
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM]   = GRAPHICS_IMAGE_SLOT(0x4FEF0),
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL]              = GRAPHICS_IMAGE_SLOT(0x513B0),
    [GAME_AREA_SHELTER_B4_LOWER_SEWER]                = GRAPHICS_IMAGE_SLOT(0x52420),
    [GAME_AREA_SHELTER_B4_UPPER_SEWER]                = GRAPHICS_IMAGE_SLOT(0x4BF20),
    [GAME_AREA_SHELTER_B4_RESERVOIR]                  = GRAPHICS_IMAGE_SLOT(0x4DA70),
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY]               = GRAPHICS_IMAGE_SLOT(0x51640),
    [GAME_AREA_SHELTER_R47]                           = GRAPHICS_IMAGE_SLOT(0x422F0),
    [GAME_AREA_SHELTER_R48]                           = GRAPHICS_IMAGE_SLOT(0x46F00),
    [GAME_AREA_SHELTER_R49]                           = GRAPHICS_IMAGE_SLOT(0x58970),
};

/// Picture-page IDs used by this overlay's map records.
enum {
    MAP_SHELTER_PAGE_MINE_UNDERGROUND = 1,
    MAP_SHELTER_PAGE_MINE             = 2,
    MAP_SHELTER_PAGE_B1               = 3,
    MAP_SHELTER_PAGE_B2               = 4,
    MAP_SHELTER_PAGE_B3               = 5,
    MAP_SHELTER_PAGE_B4               = 6,
};

/// Marker/icon list terminator and the fixed icon's ungated condition.
enum {
    MAP_SHELTER_LIST_PAGE_END      = 0,
    MAP_SHELTER_ICON_FIXED_PICTURE = 0,
    MAP_SHELTER_ICON_ALWAYS        = 0,
};

u8 gMapShelterPageFlagIds[MAP_SHELTER_PAGE_COUNT + 1] = {
    [0]                                 = 0,
    [MAP_SHELTER_PAGE_MINE_UNDERGROUND] = GAME_FLAG_SHELTER_B2_LABORATORY_017,
    [MAP_SHELTER_PAGE_MINE]             = GAME_FLAG_SHELTER_B2_LABORATORY_017,
    [MAP_SHELTER_PAGE_B1]               = GAME_FLAG_SHELTER_B2_LABORATORY_017,
    [MAP_SHELTER_PAGE_B2]               = GAME_FLAG_SHELTER_B2_LABORATORY_018,
    [MAP_SHELTER_PAGE_B3]               = GAME_FLAG_SHELTER_B2_LABORATORY_018,
    [MAP_SHELTER_PAGE_B4]               = GAME_FLAG_SHELTER_B2_LABORATORY_018,
};

MenuMapArea gMapShelterAreas[GAME_AREA_SHELTER_R49 + 2] = {
    [0]                                               = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_MINE_MESA]                             = { 0x13B, 0xDEA, 0x4B, 0xFFF5, 0x12C, 0x12C, MAP_SHELTER_PAGE_MINE },
    [GAME_AREA_MINE_CAVERN]                           = { 0x4522, 0x9C4, 0x15, 0xFFEB, 0x12C, 0x12C, MAP_SHELTER_PAGE_MINE_UNDERGROUND },
    [GAME_AREA_MINE_TUNNEL_ENTRANCE]                  = { 0x4204, 0x4B0, 0x3F, 0xFFF3, 0x12C, 0x12C, MAP_SHELTER_PAGE_MINE },
    [GAME_AREA_MINE_TUNNEL]                           = { 0x3CF0, 0x4B0, 2, 0xFFF5, 0x12C, 0x12C, MAP_SHELTER_PAGE_MINE },
    [GAME_AREA_MINE_GORGE]                            = { 0x43F8, 0x640, 0xFFC8, 0xFFF4, 0x12C, 0x12C, MAP_SHELTER_PAGE_MINE },
    [GAME_AREA_MINE_REFUGE]                           = { 0x5C0, 0x12B, 0xFF90, 0xFFEB, 0x12C, 0x12C, MAP_SHELTER_PAGE_MINE },
    [GAME_AREA_MINE_FORKED_TUNNEL]                    = { 0x92E, 0x2904, 0x11, 2, 0x12C, 0x12C, MAP_SHELTER_PAGE_MINE },
    [GAME_AREA_MINE_SECRET_PASSAGE]                   = { 0x514, 0x36B0, 0xFFE0, 0xFFFC, 0x12C, 0x12C, MAP_SHELTER_PAGE_MINE_UNDERGROUND },
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL]              = { -0x2EE0, 0, 0xFFF1, 0x5F, 0x10E, 0x12C, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY]  = { -0x935, -0xF96, 0x4D, 0x4F, 0x12C, 0x12C, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B1_STOREROOM]                  = { 0x1388, -0x820, 0x5A, 0x2A, 0xF0, 0x12C, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY]  = { -0x7D0, 0xE10, 0x4D, 0xFFF9, 0x12C, 0x12C, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B1_ARMORY]                     = { 0, 0x834, 0x21, 0x34, 0x104, 0x104, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS]          = { 0, -0x35C, 0x23, 0x18, 0x12C, 0x118, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR]              = { 0, -0x4A38, 0xE, 0x55, 0x12C, 0x118, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM]         = { 0xD80, 0x3C0, 0xF, 0xB, 0xDC, 0x118, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL]          = { 0x6A2, -0x178C, 0xE, 0xFFD5, 0x12C, 0x104, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM]               = { 0x2774, 0x520, 0xFFE1, 0x24, 0x12C, 0x12C, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL]              = { 0x26E1, 0x5C2, 0xFFDF, 0x26, 0x10E, 0x12C, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING]        = { 0x779, -0x1C0C, 0xFFBA, 8, 0x12C, 0x12C, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1]            = { 0x1A6B, 0x3E8, 0xFFD2, 0x21, 0x12C, 0x12C, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B2_POD_BOTTOM]                 = { 0x606, 0x1CCE, 0xFFFA, 0xFFB2, 0x12C, 0x12C, MAP_SHELTER_PAGE_B2 },
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY]         = { 0, 0, 0, 0, 0x12C, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL]            = { 0x1846, 0x3C, 0xFFF8, 0x18, 0x12C, 0x12C, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL] = { 0x150D, 0xC2, 0xFFF8, 0x35, 0x12C, 0x12C, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_B2_ELEVATOR]                   = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL]              = { -0x1559, 0x410, 0xFFE2, 0x59, 0x11B, 0x12C, MAP_SHELTER_PAGE_B2 },
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY]  = { -0x96C, -0xF6A, 0x1F, 0x4D, 0x12C, 0x12C, MAP_SHELTER_PAGE_B2 },
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM]             = { 0x261B, 0x409, 0x2B, 0x27, 0x118, 0x12C, MAP_SHELTER_PAGE_B2 },
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY]  = { 0x7D0, -0x1194, 0x2D, 0x11, 0x12C, 0x12C, MAP_SHELTER_PAGE_B2 },
    [GAME_AREA_SHELTER_B2_LABORATORY]                 = { 0, 0x2BC, 0xFFF3, 0x38, 0x104, 0x12C, MAP_SHELTER_PAGE_B2 },
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM]              = { 0, 0, 0xFFF3, 0x16, 0x118, 0x118, MAP_SHELTER_PAGE_B2 },
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR]              = { -0x43, -0x4909, 0xFFE0, 0x53, 0x12C, 0x10E, MAP_SHELTER_PAGE_B2 },
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK]                = { -0x59, -0x3395, 0xFFE0, 7, 0x12C, 0x12C, MAP_SHELTER_PAGE_B2 },
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL]          = { 0x761, -0x251C, 0xFFE0, 0xFFCF, 0x10E, 0x12C, MAP_SHELTER_PAGE_B2 },
    [GAME_AREA_SHELTER_R36]                           = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_SHELTER_R37]                           = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_SHELTER_1F_HELIPORT_S4]                = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE]               = { 0x4CF4, -0x1162, 0xFFEB, 0xFFC5, 0x12C, 0x12C, MAP_SHELTER_PAGE_B3 },
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR]        = { 0x280, -0xA60, 0xFFF7, 0xFFCC, 0x12C, 0x12C, MAP_SHELTER_PAGE_B3 },
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM]   = { -0x14D5, 0xBA8, 0x23, 0x23, 0x12C, 0x12C, MAP_SHELTER_PAGE_B3 },
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL]              = { -0x179F, 0x5DC, 0x22, 0x3E, 0x12C, 0x12C, MAP_SHELTER_PAGE_B3 },
    [GAME_AREA_SHELTER_B4_LOWER_SEWER]                = { -0x1E8D, 0x995, 0xFFB6, 0xFFEA, 0x12C, 0x12C, MAP_SHELTER_PAGE_B4 },
    [GAME_AREA_SHELTER_B4_UPPER_SEWER]                = { -0x23F3, 0x193A, 3, 0xFFEC, 0x122, 0x12C, MAP_SHELTER_PAGE_B4 },
    [GAME_AREA_SHELTER_B4_RESERVOIR]                  = { 0x148A, -0xB74, 0x4D, 0xE, 0x12C, 0x12C, MAP_SHELTER_PAGE_B4 },
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY]               = { 0x190, -0x400, 0x36, 0xFFE3, 0x12C, 0x12C, MAP_SHELTER_PAGE_B4 },
    [GAME_AREA_SHELTER_R47]                           = { 0x9E7, 0xC94, 0x2D, 0xFFC5, 0x12C, 0x12C, MAP_SHELTER_PAGE_B1 },
    [GAME_AREA_SHELTER_R48]                           = { 0x606, 0x1CCE, 0xFFFA, 0xFFB2, 0x12C, 0x12C, MAP_SHELTER_PAGE_B2 },
    [GAME_AREA_SHELTER_R49]                           = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_SHELTER_R49 + 1]                       = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_END },
};

MenuMapAreaShape gMapShelterAreaShapes[GAME_AREA_SHELTER_R49 + 1] = {
    [0]                                               = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_MINE_MESA]                             = { .model = &gMappicS402MesaShape, .page = MAP_SHELTER_PAGE_MINE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_MINE_CAVERN]                           = { .model = &gMappicS400CavernShape, .page = MAP_SHELTER_PAGE_MINE_UNDERGROUND, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_MINE_TUNNEL_ENTRANCE]                  = { .model = &gMappicS402TunnelEntranceShape, .page = MAP_SHELTER_PAGE_MINE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_MINE_TUNNEL]                           = { .model = &gMappicS402TunnelShape, .page = MAP_SHELTER_PAGE_MINE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_MINE_GORGE]                            = { .model = &gMappicS402GorgeShape, .page = MAP_SHELTER_PAGE_MINE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_MINE_REFUGE]                           = { .model = &gMappicS402RefugeShape, .page = MAP_SHELTER_PAGE_MINE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_MINE_FORKED_TUNNEL]                    = { .model = &gMappicS402ForkedTunnelShape, .page = MAP_SHELTER_PAGE_MINE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_MINE_SECRET_PASSAGE]                   = { .model = &gMappicS400SecretPassageShape, .page = MAP_SHELTER_PAGE_MINE_UNDERGROUND, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL]              = { .model = &gMappicS403B1ElevatorHallShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY]  = { .model = &gMappicS403B1SouthMaintenanceWalkwayShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_STOREROOM]                  = { .model = &gMappicS403B1StoreroomShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY]  = { .model = &gMappicS403B1NorthMaintenanceWalkwayShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_ARMORY]                     = { .model = &gMappicS403B1ArmoryShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS]          = { .model = &gMappicS403B1SleepingQuartersShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR]              = { .model = &gMappicS403B1MainCorridorShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM]         = { .model = &gMappicS403B1SterilizationRoomShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL]          = { .model = &gMappicS403B1PodAccessTunnelShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM]               = { .model = &gMappicS403B1ControlRoomShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL]              = { .model = &gMappicS403B1AccessTunnelShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING]        = { .model = &gMappicS403B1UndergroundParkingShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1]            = { .model = &gMappicS403B1GolemFreezer1Shape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B2_POD_BOTTOM]                 = { .model = &gMappicS404B2PodBottomShape, .page = MAP_SHELTER_PAGE_B2, .pairedArea = GAME_AREA_SHELTER_R48 },
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY]         = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL]            = { .model = &gMappicS403B1TransferTunnelShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL] = { .model = &gMappicS403B1ControlRoomAccessTunnelShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B2_ELEVATOR]                   = { .model = NULL, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL]              = { .model = &gMappicS404B2ElevatorHallShape, .page = MAP_SHELTER_PAGE_B2, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY]  = { .model = &gMappicS404B2SouthMaintenanceWalkwayShape, .page = MAP_SHELTER_PAGE_B2, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM]             = { .model = &gMappicS404B2OperatingRoomShape, .page = MAP_SHELTER_PAGE_B2, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY]  = { .model = &gMappicS404B2NorthMaintenanceWalkwayShape, .page = MAP_SHELTER_PAGE_B2, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B2_LABORATORY]                 = { .model = &gMappicS404B2LaboratoryShape, .page = MAP_SHELTER_PAGE_B2, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM]              = { .model = &gMappicS404B2BreedingRoomShape, .page = MAP_SHELTER_PAGE_B2, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR]              = { .model = &gMappicS404B2MainCorridorShape, .page = MAP_SHELTER_PAGE_B2, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK]                = { .model = &gMappicS404B2SepticTankShape, .page = MAP_SHELTER_PAGE_B2, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL]          = { .model = &gMappicS404B2PodAccessTunnelShape, .page = MAP_SHELTER_PAGE_B2, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_R36]                           = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_R37]                           = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_1F_HELIPORT_S4]                = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE]               = { .model = &gMappicS405DumpingHoleShape, .page = MAP_SHELTER_PAGE_B3, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR]        = { .model = &gMappicS405GarbageIncineratorShape, .page = MAP_SHELTER_PAGE_B3, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM]   = { .model = &gMappicS405IncineratorControlRoomShape, .page = MAP_SHELTER_PAGE_B3, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL]              = { .model = &gMappicS405ElevatorHallShape, .page = MAP_SHELTER_PAGE_B3, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B4_LOWER_SEWER]                = { .model = &gMappicS406LowerSewerShape, .page = MAP_SHELTER_PAGE_B4, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B4_UPPER_SEWER]                = { .model = &gMappicS406UpperSewerShape, .page = MAP_SHELTER_PAGE_B4, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B4_RESERVOIR]                  = { .model = &gMappicS406ReservoirShape, .page = MAP_SHELTER_PAGE_B4, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY]               = { .model = &gMappicS406WaterSupplyShape, .page = MAP_SHELTER_PAGE_B4, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_R47]                           = { .model = &gMappicS403B1PodDeckShape, .page = MAP_SHELTER_PAGE_B1, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_R48]                           = { .model = &gMappicS404B2PodBottomShape, .page = MAP_SHELTER_PAGE_B2, .pairedArea = GAME_AREA_SHELTER_B2_POD_BOTTOM },
    [GAME_AREA_SHELTER_R49]                           = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
};

MenuMapMarker gMapShelterMarkers[MAP_SHELTER_MARKER_COUNT + 1] = {
    { .page = MAP_SHELTER_PAGE_B1, .area = MENU_MAP_MARKER_AREA_ANY, .x = 14, .y = 90 },
    { .page = MAP_SHELTER_PAGE_B1, .area = MENU_MAP_MARKER_AREA_ANY, .x = 58, .y = 44 },
    { .page = MAP_SHELTER_PAGE_B3, .area = MENU_MAP_MARKER_AREA_ANY, .x = 35, .y = 57 },
    { .page = MAP_SHELTER_PAGE_B2, .area = MENU_MAP_MARKER_AREA_ANY, .x = 45, .y = 18 },
    { .page = MAP_SHELTER_PAGE_B2, .area = MENU_MAP_MARKER_AREA_ANY, .x = 28, .y = 77 },
    { .page = MAP_SHELTER_PAGE_B2, .area = MENU_MAP_MARKER_AREA_ANY, .x = 44, .y = 44 },
    { .page = MAP_SHELTER_PAGE_B2, .area = MENU_MAP_MARKER_AREA_ANY, .x = -32, .y = 85 },
    { .page = MAP_SHELTER_PAGE_B1, .area = MENU_MAP_MARKER_AREA_ANY, .x = 0, .y = 24 },
    { .page = MAP_SHELTER_PAGE_B1, .area = MENU_MAP_MARKER_AREA_ANY, .x = -34, .y = 45 },
    { .page = MAP_SHELTER_PAGE_B2, .area = MENU_MAP_MARKER_AREA_ANY, .x = -46, .y = 22 },
    { .page = MAP_SHELTER_PAGE_B2, .area = MENU_MAP_MARKER_AREA_ANY, .x = -19, .y = 51 },
    { .page = MAP_SHELTER_PAGE_B1, .area = MENU_MAP_MARKER_AREA_ANY, .x = 36, .y = -58 },
    { .page = MAP_SHELTER_PAGE_B2, .area = MENU_MAP_MARKER_AREA_ANY, .x = -12, .y = -76 },
    { .page = MAP_SHELTER_PAGE_MINE, .area = MENU_MAP_MARKER_AREA_ANY, .x = -116, .y = -12 },
    { .page = MAP_SHELTER_PAGE_B3, .area = MENU_MAP_MARKER_AREA_ANY, .x = 64, .y = 55 },
    { .page = MAP_SHELTER_PAGE_B4, .area = MENU_MAP_MARKER_AREA_ANY, .x = 49, .y = 14 },
    { .page = MAP_SHELTER_PAGE_B4, .area = MENU_MAP_MARKER_AREA_ANY, .x = 102, .y = -29 },
    { .page = MAP_SHELTER_PAGE_B1, .area = GAME_AREA_SHELTER_B1_ELEVATOR_HALL, .x = 70, .y = 96 },
    { .page = MAP_SHELTER_PAGE_B2, .area = GAME_AREA_SHELTER_B2_ELEVATOR_HALL, .x = 26, .y = 95 },
    { .page = MAP_SHELTER_PAGE_MINE_UNDERGROUND, .area = MENU_MAP_MARKER_AREA_ANY, .x = -31, .y = -11 },
    { .page = MAP_SHELTER_PAGE_B1, .area = GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, .x = 25, .y = -68 },
    { .page = MAP_SHELTER_PAGE_B1, .area = MENU_MAP_MARKER_AREA_ANY, .x = -94, .y = -16 },
    { .page = MAP_SHELTER_PAGE_B1, .area = MENU_MAP_MARKER_AREA_ANY, .x = 77, .y = -4 },
    { .page = MAP_SHELTER_PAGE_B1, .area = MENU_MAP_MARKER_AREA_ANY, .x = -37, .y = 34 },
    { .page = MAP_SHELTER_PAGE_B1, .area = MENU_MAP_MARKER_AREA_ANY, .x = 90, .y = 25 },
    { .page = MAP_SHELTER_PAGE_B2, .area = MENU_MAP_MARKER_AREA_ANY, .x = -49, .y = 51 },
    { .page = MAP_SHELTER_PAGE_B1, .area = MENU_MAP_MARKER_AREA_ANY, .x = 90, .y = 45 },
    { .page = MAP_SHELTER_PAGE_B1, .area = MENU_MAP_MARKER_AREA_ANY, .x = 52, .y = 70 },
    { .page = MAP_SHELTER_PAGE_B2, .area = GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, .x = -20, .y = -69 },
    { .page = MAP_SHELTER_PAGE_B3, .area = GAME_AREA_SHELTER_B3_ELEVATOR_HALL, .x = 39, .y = 64 },
    { .page = MAP_SHELTER_LIST_PAGE_END, .area = 0, .x = 0, .y = 0 },
};

/// Objective-byte values selecting this map's four objective-icon records.
///
/// The laboratory chooses 40/41 from its route flag; the walkway scene sets 32.
/// Value 46 marks the B2 main corridor; these are objective IDs, not nibble IDs.
enum {
    MAP_SHELTER_OBJECTIVE_AFTER_WALKWAY_SCENE = 0x20,
    MAP_SHELTER_OBJECTIVE_LAB_ROUTE_SET       = 0x28,
    MAP_SHELTER_OBJECTIVE_LAB_ROUTE_CLEAR     = 0x29,
    MAP_SHELTER_OBJECTIVE_B2_MAIN_CORRIDOR    = 0x2E,
};

MenuMapIcon gMapShelterIcons[15] = {
    { .page = MAP_SHELTER_PAGE_MINE, .area = GAME_AREA_MINE_MESA, .kind = MAP_SHELTER_ICON_FIXED_PICTURE, .condition = GAME_FLAG_09A, .x = 97, .y = -13 },
    { .page = MAP_SHELTER_PAGE_MINE, .area = GAME_AREA_MINE_REFUGE, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = MAP_SHELTER_ICON_ALWAYS, .x = -114, .y = -27 },
    { .page = MAP_SHELTER_PAGE_B1, .area = GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = MAP_SHELTER_ICON_ALWAYS, .x = 3, .y = -36 },
    { .page = MAP_SHELTER_PAGE_B1, .area = GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, .kind = MAP_SHELTER_ICON_FIXED_PICTURE, .condition = MAP_SHELTER_ICON_ALWAYS, .x = 26, .y = -36 },
    { .page = MAP_SHELTER_PAGE_B1, .area = GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = MAP_SHELTER_ICON_ALWAYS, .x = -76, .y = 0 },
    { .page = MAP_SHELTER_PAGE_B1, .area = GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, .kind = MAP_SHELTER_ICON_FIXED_PICTURE, .condition = GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE, .x = -77, .y = -16 },
    { .page = MAP_SHELTER_PAGE_B2, .area = GAME_AREA_SHELTER_B2_LABORATORY, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = MAP_SHELTER_ICON_ALWAYS, .x = -2, .y = 68 },
    { .page = MAP_SHELTER_PAGE_B2, .area = GAME_AREA_SHELTER_B2_LABORATORY, .kind = MAP_SHELTER_ICON_FIXED_PICTURE, .condition = MAP_SHELTER_ICON_ALWAYS, .x = 0, .y = 43 },
    { .page = MAP_SHELTER_PAGE_B3, .area = GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = MAP_SHELTER_ICON_ALWAYS, .x = 38, .y = 50 },
    { .page = MAP_SHELTER_PAGE_B1, .area = GAME_AREA_SHELTER_R47, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = MAP_SHELTER_ICON_ALWAYS, .x = 82, .y = -80 },
    { .page = MAP_SHELTER_PAGE_B1, .area = GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, .kind = MENU_MAP_ICON_KIND_OBJECTIVE, .condition = MAP_SHELTER_OBJECTIVE_AFTER_WALKWAY_SCENE, .x = 90, .y = 0 },
    { .page = MAP_SHELTER_PAGE_B1, .area = GAME_AREA_SHELTER_R47, .kind = MENU_MAP_ICON_KIND_OBJECTIVE, .condition = MAP_SHELTER_OBJECTIVE_LAB_ROUTE_SET, .x = 85, .y = -73 },
    { .page = MAP_SHELTER_PAGE_B1, .area = GAME_AREA_SHELTER_R47, .kind = MENU_MAP_ICON_KIND_OBJECTIVE, .condition = MAP_SHELTER_OBJECTIVE_LAB_ROUTE_CLEAR, .x = 85, .y = -73 },
    { .page = MAP_SHELTER_PAGE_B2, .area = GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, .kind = MENU_MAP_ICON_KIND_OBJECTIVE, .condition = MAP_SHELTER_OBJECTIVE_B2_MAIN_CORRIDOR, .x = -48, .y = 52 },
    { .page = MAP_SHELTER_LIST_PAGE_END },
};

MenuMapAreaName gMapShelterAreaNames[GAME_AREA_SHELTER_R49] = {
    [GAME_AREA_MINE_MESA - 1]                             = { "Mesa" },
    [GAME_AREA_MINE_CAVERN - 1]                           = { "Cavern" },
    [GAME_AREA_MINE_TUNNEL_ENTRANCE - 1]                  = { "Tunnel entrance" },
    [GAME_AREA_MINE_TUNNEL - 1]                           = { "Tunnel" },
    [GAME_AREA_MINE_GORGE - 1]                            = { "Gorge" },
    [GAME_AREA_MINE_REFUGE - 1]                           = { "Refuge" },
    [GAME_AREA_MINE_FORKED_TUNNEL - 1]                    = { "Forked tunnel" },
    [GAME_AREA_MINE_SECRET_PASSAGE - 1]                   = { "Secret passage" },
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL - 1]              = { "Elevator hall" },
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY - 1]  = { "South maintenance walkway" },
    [GAME_AREA_SHELTER_B1_STOREROOM - 1]                  = { "Storeroom" },
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY - 1]  = { "North maintenance walkway" },
    [GAME_AREA_SHELTER_B1_ARMORY - 1]                     = { "Armory" },
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS - 1]          = { "Sleeping quarters" },
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR - 1]              = { "Main corridor" },
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM - 1]         = { "Sterilization room" },
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL - 1]          = { "Pod access tunnel" },
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM - 1]               = { "Control room" },
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL - 1]              = { "Access tunnel" },
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING - 1]        = { "Underground parking" },
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 - 1]            = { "Golem freezer 1" },
    [GAME_AREA_SHELTER_B2_POD_BOTTOM - 1]                 = { "Pod bottom" },
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY - 1]         = { "Pod service gantry" },
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL - 1]            = { "Transfer tunnel" },
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL - 1] = { "Control room access tunnel" },
    [GAME_AREA_SHELTER_B2_ELEVATOR - 1]                   = { "Elevator" },
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL - 1]              = { "Elevator hall" },
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY - 1]  = { "South maintenance walkway" },
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM - 1]             = { "Operating room" },
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY - 1]  = { "North maintenance walkway" },
    [GAME_AREA_SHELTER_B2_LABORATORY - 1]                 = { "Laboratory" },
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM - 1]              = { "Breeding room" },
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR - 1]              = { "Main corridor" },
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK - 1]                = { "Septic tank" },
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL - 1]          = { "Pod access tunnel" },
    [GAME_AREA_SHELTER_R36 - 1]                           = { "Oval Office\201inight\201j" },
    [GAME_AREA_SHELTER_R37 - 1]                           = { "" },
    [GAME_AREA_SHELTER_1F_HELIPORT_S4 - 1]                = { "" },
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE - 1]               = { "Dumping hole" },
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR - 1]        = { "Garbage incinerator" },
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM - 1]   = { "Incinerator control room" },
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL - 1]              = { "Elevator hall" },
    [GAME_AREA_SHELTER_B4_LOWER_SEWER - 1]                = { "Lower sewer" },
    [GAME_AREA_SHELTER_B4_UPPER_SEWER - 1]                = { "Upper sewer" },
    [GAME_AREA_SHELTER_B4_RESERVOIR - 1]                  = { "Reservoir" },
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY - 1]               = { "Water supply" },
    [GAME_AREA_SHELTER_R47 - 1]                           = { "Pod deck" },
    [GAME_AREA_SHELTER_R48 - 1]                           = { "Pod bottom" },
    [GAME_AREA_SHELTER_R49 - 1]                           = { "Oval Office\201imorning\201j" },
};

/// Initial two-bit object states used by this stage's placements.
///
/// State 3 publishes a full ammunition stack and is retained after collection.
/// The separate prompt bit selects immediate pickup. Stored bit 8 has no reader;
/// retain it without assigning a purpose.
enum {
    MAP_SHELTER_STATE_INACTIVE            = 0,
    MAP_SHELTER_STATE_AVAILABLE           = 1,
    MAP_SHELTER_STATE_REPLENISHABLE       = 3,
    MAP_SHELTER_OBJECT_STATE_UNREAD_BIT_8 = 0x100,
};

/// Catalog kinds and room-specific prop/UI kinds in the object lists.
///
/// Banks 0 and 1 publish catalog ids, bank 2 selects scripted models, bank 7
/// selects an item-box inventory and bank 8 selects save-point UI. The breeding
/// room model uses kind 0x129, whose catalog entry is empty; its identity is
/// unproven beyond the model hidden when its object state is 2.
enum {
    MAP_SHELTER_KIND_STIM                                = 0x4,
    MAP_SHELTER_KIND_COLA                                = 0x5,
    MAP_SHELTER_KIND_MP_BOOST_2                          = 0x7,
    MAP_SHELTER_KIND_BELT_POUCH                          = 0xD,
    MAP_SHELTER_KIND_MEDICINE_WHEEL                      = 0x37,
    MAP_SHELTER_KIND_RINGERS_SOLUTION                    = 0x3D,
    MAP_SHELTER_KIND_EAU_DE_TOILETTE                     = 0x3E,
    MAP_SHELTER_KIND_P229                                = 0x84,
    MAP_SHELTER_KIND_SP12                                = 0x8D,
    MAP_SHELTER_KIND_9MM_PARABELLUM                      = 0xA0,
    MAP_SHELTER_KIND_9MM_HYDRA                           = 0xA1,
    MAP_SHELTER_KIND_GRENADE                             = 0xA9,
    MAP_SHELTER_KIND_BUCKSHOT                            = 0xAC,
    MAP_SHELTER_KIND_556_RIFLE_ROUND                     = 0xAF,
    MAP_SHELTER_KIND_JUMPER_PLUG_120                     = 0x120,
    MAP_SHELTER_KIND_YOSHIDAS_CARD                       = 0x122,
    MAP_SHELTER_KIND_ELECTRIC_CAR_KEY                    = 0x123,
    MAP_SHELTER_KIND_PIERCE_MEMO                         = 0x125,
    MAP_SHELTER_KIND_AERIS_MAGAZINE                      = 0x127,
    MAP_SHELTER_KIND_BREEDING_ROOM_MODEL                 = 0x129,
    MAP_SHELTER_KIND_JUMPER_PLUG_12C                     = 0x12C,
    MAP_SHELTER_KIND_MICRO_DEVICE                        = 0x12D,
    MAP_SHELTER_KIND_R48_FIRST_SLIDER                    = 0x20B,
    MAP_SHELTER_KIND_R48_SECOND_SLIDER                   = 0x20C,
    MAP_SHELTER_KIND_FORKED_TUNNEL_PROP                  = 0x20D,
    MAP_SHELTER_KIND_ITEM_BOX_3                          = 0x703,
    MAP_SHELTER_KIND_ITEM_BOX_4                          = 0x704,
    MAP_SHELTER_KIND_ITEM_BOX_5                          = 0x705,
    MAP_SHELTER_KIND_ITEM_BOX_6                          = 0x706,
    MAP_SHELTER_KIND_STERILIZATION_ROOM_ITEM_BOX         = 0x708,
    MAP_SHELTER_KIND_LABORATORY_ITEM_BOX                 = 0x70A,
    MAP_SHELTER_KIND_REFUGE_SAVE_POINT                   = 0x807,
    MAP_SHELTER_KIND_STERILIZATION_ROOM_SAVE_POINT       = 0x808,
    MAP_SHELTER_KIND_UNDERGROUND_PARKING_SAVE_POINT      = 0x809,
    MAP_SHELTER_KIND_LABORATORY_SAVE_POINT               = 0x80A,
    MAP_SHELTER_KIND_INCINERATOR_CONTROL_ROOM_SAVE_POINT = 0x80B,
    MAP_SHELTER_KIND_R47_SAVE_POINT                      = 0x80C,
};

/// Stage-local indices of the 52 object states seeded by these place lists.
///
/// Each selects one of 64 two-bit slots in the resident Mine/Shelter bank.
enum {
    MAP_SHELTER_OBJECT_SLOT_MINE_FORKED_TUNNEL_OAK_BOARD              = 0x1,
    MAP_SHELTER_OBJECT_SLOT_B1_NORTH_MAINTENANCE_WALKWAY_BOWMANS_CARD = 0x2,
    MAP_SHELTER_OBJECT_SLOT_B2_ELEVATOR_YOSHIDAS_CARD                 = 0x3,
    MAP_SHELTER_OBJECT_SLOT_MINE_REFUGE_JUMPER_PLUG_120               = 0x4,
    MAP_SHELTER_OBJECT_SLOT_MINE_REFUGE_JUMPER_PLUG_12C               = 0x5,
    MAP_SHELTER_OBJECT_SLOT_MINE_CAVERN_JUMPER_PLUG_12C               = 0x6,
    MAP_SHELTER_OBJECT_SLOT_R47_MICRO_DEVICE                          = 0x7,
    MAP_SHELTER_OBJECT_SLOT_MINE_CAVERN_P229                          = 0x8,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_BOWMANS_CARD       = 0x9,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_YOSHIDAS_CARD      = 0xA,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_ELECTRIC_CAR_KEY   = 0xB,
    MAP_SHELTER_OBJECT_SLOT_MINE_REFUGE_9MM_PARABELLUM                = 0xC,
    MAP_SHELTER_OBJECT_SLOT_B1_STOREROOM_9MM_PARABELLUM               = 0xD,
    MAP_SHELTER_OBJECT_SLOT_B1_ARMORY_SP12                            = 0xF,
    MAP_SHELTER_OBJECT_SLOT_B1_ARMORY_9MM_PARABELLUM                  = 0x11,
    MAP_SHELTER_OBJECT_SLOT_B1_ARMORY_9MM_HYDRA                       = 0x12,
    MAP_SHELTER_OBJECT_SLOT_B1_ARMORY_BUCKSHOT                        = 0x13,
    MAP_SHELTER_OBJECT_SLOT_B1_SLEEPING_QUARTERS_AERIS_MAGAZINE       = 0x14,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_556_RIFLE_ROUND    = 0x15,
    MAP_SHELTER_OBJECT_SLOT_B2_OPERATING_ROOM_RINGERS_SOLUTION        = 0x16,
    MAP_SHELTER_OBJECT_SLOT_B2_BREEDING_ROOM_STIM                     = 0x17,
    MAP_SHELTER_OBJECT_SLOT_B2_BREEDING_ROOM_MODEL                    = 0x18,
    MAP_SHELTER_OBJECT_SLOT_B2_BREEDING_ROOM_9MM_PARABELLUM           = 0x19,
    MAP_SHELTER_OBJECT_SLOT_B3_DUMPING_HOLE_MEDICINE_WHEEL            = 0x1A,
    MAP_SHELTER_OBJECT_SLOT_MINE_MESA_ITEM_BOX_5                      = 0x1B,
    MAP_SHELTER_OBJECT_SLOT_B1_STERILIZATION_ROOM_ITEM_BOX            = 0x1C,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_GRENADE            = 0x1D,
    MAP_SHELTER_OBJECT_SLOT_B2_LABORATORY_ITEM_BOX                    = 0x1E,
    MAP_SHELTER_OBJECT_SLOT_B3_DUMPING_HOLE_9MM_HYDRA                 = 0x1F,
    MAP_SHELTER_OBJECT_SLOT_R48_FIRST_SLIDER                          = 0x20,
    MAP_SHELTER_OBJECT_SLOT_R48_SECOND_SLIDER                         = 0x21,
    MAP_SHELTER_OBJECT_SLOT_R47_PIERCE_MEMO                           = 0x22,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_ITEM_BOX_3         = 0x23,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_ITEM_BOX_4         = 0x24,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_ITEM_BOX_5         = 0x25,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_ITEM_BOX_6         = 0x26,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_9MM_PARABELLUM     = 0x27,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_9MM_HYDRA          = 0x28,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_BUCKSHOT           = 0x29,
    MAP_SHELTER_OBJECT_SLOT_R47_SAVE_POINT                            = 0x2A,
    MAP_SHELTER_OBJECT_SLOT_B3_INCINERATOR_CONTROL_ROOM_SAVE_POINT    = 0x2B,
    MAP_SHELTER_OBJECT_SLOT_B2_LABORATORY_SAVE_POINT                  = 0x2C,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_SAVE_POINT         = 0x2D,
    MAP_SHELTER_OBJECT_SLOT_B1_STERILIZATION_ROOM_SAVE_POINT          = 0x2E,
    MAP_SHELTER_OBJECT_SLOT_MINE_REFUGE_SAVE_POINT                    = 0x2F,
    MAP_SHELTER_OBJECT_SLOT_B3_DUMPING_HOLE_9MM_PARABELLUM            = 0x30,
    MAP_SHELTER_OBJECT_SLOT_B4_RESERVOIR_MP_BOOST_2                   = 0x31,
    MAP_SHELTER_OBJECT_SLOT_B1_STOREROOM_RINGERS_SOLUTION             = 0x32,
    MAP_SHELTER_OBJECT_SLOT_B2_OPERATING_ROOM_EAU_DE_TOILETTE         = 0x33,
    MAP_SHELTER_OBJECT_SLOT_B2_BREEDING_ROOM_COLA                     = 0x34,
    MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_BELT_POUCH         = 0x35,
    MAP_SHELTER_OBJECT_SLOT_MINE_FORKED_TUNNEL_PROP                   = 0x37,
};

/// Execution priority of this stage's generic placed-model tasks.
enum { MAP_SHELTER_OBJECT_TASK_PRIORITY = 0x62 };

/// Empty generic spawn list shared by areas with flag-only object placements.
///
/// The single terminator keeps the spawn pointer non-NULL and stops every kind
/// search before a task descriptor is read. The placement lists still seed
/// and expose their two-bit object flags.
static AreaObjectSpawn _gMapShelterEmptyObjectSpawns[1] = {
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// Spawn recipe for the Forked tunnel's scripted prop.
///
/// Matches the paired place list by kind and ends with `AREA_OBJECT_SPAWN_END`.
/// Tasks own their enemy/model allocations; borrowed callbacks and model sources
/// must remain loaded until the spawned tasks finish.
static AreaObjectSpawn _gMapShelterMineForkedTunnelObjectSpawns[2] = {
    { .kind = MAP_SHELTER_KIND_FORKED_TUNNEL_PROP, .taskDesc = { { { TASK_BODY_TMD, MAP_SHELTER_OBJECT_TASK_PRIORITY } }, mineForkedTunnelAreaObjectTask, { &gMineForkedTunnelModel01B48 } } },
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// Spawn recipe for the Sleeping quarters' collectable magazine model.
///
/// Matches the paired place list by kind and ends with `AREA_OBJECT_SPAWN_END`.
/// Tasks own their enemy/model allocations; borrowed callbacks and model sources
/// must remain loaded until the spawned tasks finish.
static AreaObjectSpawn _gMapShelterB1SleepingQuartersObjectSpawns[2] = {
    { .kind = MAP_SHELTER_KIND_AERIS_MAGAZINE, .taskDesc = { { { TASK_BODY_TMD, MAP_SHELTER_OBJECT_TASK_PRIORITY } }, shelterB1SleepingQuartersAreaObjectTask, { &gShelterB1SleepingQuartersModel02DFC } } },
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// Spawn recipe for the Laboratory's item-box lid.
///
/// Matches the paired place list by kind and ends with `AREA_OBJECT_SPAWN_END`.
/// Tasks own their enemy/model allocations; borrowed callbacks and model sources
/// must remain loaded until the spawned tasks finish.
static AreaObjectSpawn _gMapShelterB2LaboratoryObjectSpawns[2] = {
    { .kind = MAP_SHELTER_KIND_LABORATORY_ITEM_BOX, .taskDesc = { { { TASK_BODY_TMD, MAP_SHELTER_OBJECT_TASK_PRIORITY } }, itemPickupContainerLidTask, { &gShelterB2LaboratoryAcropolisSanctuaryModel090F0 } } },
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// Spawn recipe for the Breeding room's state-controlled model.
///
/// Matches the paired place list by kind and ends with `AREA_OBJECT_SPAWN_END`.
/// Tasks own their enemy/model allocations; borrowed callbacks and model sources
/// must remain loaded until the spawned tasks finish.
static AreaObjectSpawn _gMapShelterB2BreedingRoomObjectSpawns[2] = {
    { .kind = MAP_SHELTER_KIND_BREEDING_ROOM_MODEL, .taskDesc = { { { TASK_BODY_TMD, MAP_SHELTER_OBJECT_TASK_PRIORITY } }, shelterB2BreedingRoomAreaObjectTask, { &gShelterB2BreedingRoomModel02E04 } } },
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// Spawn recipe for the Dumping hole's Medicine Wheel container lid.
///
/// Matches the paired place list by kind and ends with `AREA_OBJECT_SPAWN_END`.
/// Tasks own their enemy/model allocations; borrowed callbacks and model sources
/// must remain loaded until the spawned tasks finish.
static AreaObjectSpawn _gMapShelterB3DumpingHoleObjectSpawns[2] = {
    { .kind = MAP_SHELTER_KIND_MEDICINE_WHEEL, .taskDesc = { { { TASK_BODY_TMD, MAP_SHELTER_OBJECT_TASK_PRIORITY } }, itemPickupContainerLidTask, { &gShelterB3DumpingHoleAcropolisSanctuaryModel090F0 } } },
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// Spawn recipes for the two R48 intro slider models.
///
/// Matches the paired place list by kind and ends with `AREA_OBJECT_SPAWN_END`.
/// Tasks own their enemy/model allocations; borrowed callbacks and model sources
/// must remain loaded until the spawned tasks finish.
static AreaObjectSpawn _gMapShelterR48ObjectSpawns[3] = {
    { .kind = MAP_SHELTER_KIND_R48_FIRST_SLIDER, .taskDesc = { { { TASK_BODY_TMD, MAP_SHELTER_OBJECT_TASK_PRIORITY } }, actor503500SliderTask, { &gActor503500Model14DA0 } } },
    { .kind = MAP_SHELTER_KIND_R48_SECOND_SLIDER, .taskDesc = { { { TASK_BODY_TMD, MAP_SHELTER_OBJECT_TASK_PRIORITY } }, actor503500SliderTask, { &gActor503500Model15820 } } },
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// Empty model-spawn list for the Underground parking placements.
///
/// Contains only `AREA_OBJECT_SPAWN_END`; these placements still seed flags
/// and supply pickup metadata, but this table creates no models.
static AreaObjectSpawn _gMapShelterB1UndergroundParkingObjectSpawns[1] = {
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// Spawn recipe for the Sterilization room's item-box lid.
///
/// Matches the paired place list by kind and ends with `AREA_OBJECT_SPAWN_END`.
/// Tasks own their enemy/model allocations; borrowed callbacks and model sources
/// must remain loaded until the spawned tasks finish.
static AreaObjectSpawn _gMapShelterB1SterilizationRoomObjectSpawns[2] = {
    { .kind = MAP_SHELTER_KIND_STERILIZATION_ROOM_ITEM_BOX, .taskDesc = { { { TASK_BODY_TMD, MAP_SHELTER_OBJECT_TASK_PRIORITY } }, itemPickupContainerLidTask, { &gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0 } } },
    { .kind = AREA_OBJECT_SPAWN_END },
};

AreaObjectRoom gMapShelterObjectRooms[GAME_AREA_SHELTER_R49 + 2] = {
    [0]                                               = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_MINE_MESA]                             = { .places = { .list = _gMapShelterMineMesaObjectPlaces }, .spawns = _gMapShelterEmptyObjectSpawns },
    [GAME_AREA_MINE_CAVERN]                           = { .places = { .list = _gMapShelterMineCavernObjectPlaces }, .spawns = _gMapShelterEmptyObjectSpawns },
    [GAME_AREA_MINE_TUNNEL_ENTRANCE]                  = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_MINE_TUNNEL]                           = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_MINE_GORGE]                            = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_MINE_REFUGE]                           = { .places = { .list = _gMapShelterMineRefugeObjectPlaces }, .spawns = _gMapShelterEmptyObjectSpawns },
    [GAME_AREA_MINE_FORKED_TUNNEL]                    = { .places = { .list = _gMapShelterMineForkedTunnelObjectPlaces }, .spawns = _gMapShelterMineForkedTunnelObjectSpawns },
    [GAME_AREA_MINE_SECRET_PASSAGE]                   = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL]              = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY]  = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B1_STOREROOM]                  = { .places = { .list = _gMapShelterB1StoreroomObjectPlaces }, .spawns = _gMapShelterEmptyObjectSpawns },
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY]  = { .places = { .list = _gMapShelterB1NorthMaintenanceWalkwayObjectPlaces }, .spawns = _gMapShelterEmptyObjectSpawns },
    [GAME_AREA_SHELTER_B1_ARMORY]                     = { .places = { .list = _gMapShelterB1ArmoryObjectPlaces }, .spawns = _gMapShelterEmptyObjectSpawns },
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS]          = { .places = { .list = _gMapShelterB1SleepingQuartersObjectPlaces }, .spawns = _gMapShelterB1SleepingQuartersObjectSpawns },
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR]              = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM]         = { .places = { .list = _gMapShelterB1SterilizationRoomObjectPlaces }, .spawns = _gMapShelterB1SterilizationRoomObjectSpawns },
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL]          = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM]               = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL]              = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING]        = { .places = { .list = _gMapShelterB1UndergroundParkingObjectPlaces }, .spawns = _gMapShelterB1UndergroundParkingObjectSpawns },
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1]            = { .places = { .list = _gMapShelterB1GolemFreezer1ObjectPlaces }, .spawns = _gMapShelterEmptyObjectSpawns },
    [GAME_AREA_SHELTER_B2_POD_BOTTOM]                 = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY]         = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL]            = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL] = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B2_ELEVATOR]                   = { .places = { .list = _gMapShelterB2ElevatorObjectPlaces }, .spawns = _gMapShelterEmptyObjectSpawns },
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL]              = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY]  = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM]             = { .places = { .list = _gMapShelterB2OperatingRoomObjectPlaces }, .spawns = _gMapShelterEmptyObjectSpawns },
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY]  = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B2_LABORATORY]                 = { .places = { .list = _gMapShelterB2LaboratoryObjectPlaces }, .spawns = _gMapShelterB2LaboratoryObjectSpawns },
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM]              = { .places = { .list = _gMapShelterB2BreedingRoomObjectPlaces }, .spawns = _gMapShelterB2BreedingRoomObjectSpawns },
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR]              = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK]                = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL]          = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_R36]                           = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_R37]                           = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_1F_HELIPORT_S4]                = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE]               = { .places = { .list = _gMapShelterB3DumpingHoleObjectPlaces }, .spawns = _gMapShelterB3DumpingHoleObjectSpawns },
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR]        = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM]   = { .places = { .list = _gMapShelterB3IncineratorControlRoomObjectPlaces }, .spawns = NULL },
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL]              = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B4_LOWER_SEWER]                = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B4_UPPER_SEWER]                = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_B4_RESERVOIR]                  = { .places = { .list = _gMapShelterB4ReservoirObjectPlaces }, .spawns = NULL },
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY]               = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_R47]                           = { .places = { .list = _gMapShelterR47ObjectPlaces }, .spawns = _gMapShelterEmptyObjectSpawns },
    [GAME_AREA_SHELTER_R48]                           = { .places = { .list = _gMapShelterR48ObjectPlaces }, .spawns = _gMapShelterR48ObjectSpawns },
    [GAME_AREA_SHELTER_R49]                           = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_SHELTER_R49 + 1]                       = { .places = { .sentinel = AREA_OBJECT_ROOM_END }, .spawns = NULL },
};

/// Priority and default-room selector for the Mine/Shelter room recipes.
enum {
    MAP_SHELTER_ROOM_TASK_PRIORITY     = 32,
    MAP_SHELTER_ROOM_TASK_AREA_DEFAULT = 0,
};

TaskDesc gMapShelterRoomTaskDescriptors[GAME_AREA_SHELTER_R49 + 1] = {
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = mineMesaRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_MESA, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = mineCavernRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = mineTunnelEntranceRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_TUNNEL_ENTRANCE, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = mineTunnelRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_TUNNEL, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = mineGorgeRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = mineRefugeRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = mineForkedTunnelRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_FORKED_TUNNEL, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = mineSecretPassageRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_SECRET_PASSAGE, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1ElevatorHallRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1SouthMaintenanceWalkwayRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1StoreroomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STOREROOM, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1NorthMaintenanceWalkwayRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1ArmoryRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ARMORY, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1SleepingQuartersRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1MainCorridorRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1SterilizationRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1PodAccessTunnelRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1AccessTunnelRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1UndergroundParkingRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1GolemFreezer1RoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1TransferTunnelRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1ControlRoomAccessTunnelRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB2ElevatorRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB2ElevatorHallRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB2SouthMaintenanceWalkwayRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB2OperatingRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_OPERATING_ROOM, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB2NorthMaintenanceWalkwayRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB2LaboratoryTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB2BreedingRoomRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB2MainCorridorRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB2SepticTankRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB2PodAccessTunnelRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterR36RoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R36, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterR37RoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R37, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelter1fHeliportS4RoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_1F_HELIPORT_S4, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB3DumpingHoleRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB3GarbageIncineratorRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB3IncineratorControlRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB3ElevatorHallRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_ELEVATOR_HALL, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB4LowerSewerRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_LOWER_SEWER, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB4UpperSewerRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB4ReservoirRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_RESERVOIR, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB4WaterSupplyRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_WATER_SUPPLY, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterR48RoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterR47RoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterR49RoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R49, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1ControlRoomRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_CONTROL_ROOM, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB1PodServiceGantryRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_BODY_NONE, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = shelterB2PodBottomRoomTask, .data = { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, MAP_SHELTER_ROOM_TASK_AREA_DEFAULT) } },
    { .header = { .fields = { TASK_DESC_END, MAP_SHELTER_ROOM_TASK_PRIORITY } }, .callback = NULL, .data = { .value = 0 } },
};

/// Game-flag nibble IDs for marker words without a resident named ID.
enum {
    MAP_SHELTER_MARKER_FLAG_1AD = 0x1AD,
    MAP_SHELTER_MARKER_FLAG_1AF = 0x1AF,
    MAP_SHELTER_MARKER_FLAG_1B1 = 0x1B1,
    MAP_SHELTER_MARKER_FLAG_1C0 = 0x1C0,
    MAP_SHELTER_MARKER_FLAG_1C3 = 0x1C3,
    MAP_SHELTER_MARKER_FLAG_1C5 = 0x1C5,
    MAP_SHELTER_MARKER_FLAG_1C8 = 0x1C8,
    MAP_SHELTER_MARKER_FLAG_1C9 = 0x1C9,
    MAP_SHELTER_MARKER_FLAG_1CA = 0x1CA,
    MAP_SHELTER_MARKER_FLAG_1CC = 0x1CC,
};

u16 gMapShelterMarkerFlags[MAP_SHELTER_MARKER_COUNT] = {
    GAME_FLAG_MAP_MARK_SHELTER_1CD,
    MAP_SHELTER_MARKER_FLAG_1CC,
    GAME_FLAG_MAP_MARK_B3_INCINERATOR_CONTROL_DOOR,
    MAP_SHELTER_MARKER_FLAG_1CA,
    MAP_SHELTER_MARKER_FLAG_1C9,
    MAP_SHELTER_MARKER_FLAG_1C8,
    GAME_FLAG_MAP_MARK_SHELTER_1C7,
    GAME_FLAG_MAP_MARK_SHELTER_R47_1C6,
    MAP_SHELTER_MARKER_FLAG_1C5,
    GAME_FLAG_MAP_MARK_B2_MAIN_CORRIDOR,
    MAP_SHELTER_MARKER_FLAG_1C3,
    GAME_FLAG_MAP_MARK_B2_LABORATORY,
    GAME_FLAG_MAP_MARK_POD_SERVICE_GANTRY,
    MAP_SHELTER_MARKER_FLAG_1C0,
    GAME_FLAG_MAP_MARK_RESERVOIR_1BF | MENU_MAP_MARKER_ALTERNATE_PICTURE,
    GAME_FLAG_MAP_MARK_RESERVOIR_1BE | MENU_MAP_MARKER_ALTERNATE_PICTURE,
    GAME_FLAG_MAP_MARK_WATER,
    GAME_FLAG_MAP_MARK_SHELTER_1BB,
    GAME_FLAG_MAP_MARK_SHELTER_1BB,
    GAME_FLAG_MAP_MARK_MINE_CAVERN,
    GAME_FLAG_MAP_MARK_POD,
    GAME_FLAG_MAP_MARK_UNDERGROUND_PARKING,
    GAME_FLAG_MAP_MARK_SHELTER_1B3 | MENU_MAP_MARKER_ALTERNATE_PICTURE,
    MAP_SHELTER_MARKER_FLAG_1B1,
    GAME_FLAG_MAP_MARK_SHELTER_1B0 | MENU_MAP_MARKER_ALTERNATE_PICTURE,
    MAP_SHELTER_MARKER_FLAG_1AF | MENU_MAP_MARKER_ALTERNATE_PICTURE,
    GAME_FLAG_MAP_MARK_SHELTER_1AE | MENU_MAP_MARKER_ALTERNATE_PICTURE,
    MAP_SHELTER_MARKER_FLAG_1AD,
    GAME_FLAG_MAP_MARK_POD,
    GAME_FLAG_MAP_MARK_SHELTER_1BB,
};

/// Model-lighting descriptors for Shelter B1 north maintenance walkway, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1NorthMaintenanceWalkwayRoomLighting[1] = {
    { .lights = &gShelterB1NorthMaintenanceWalkwayRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 armory, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1ArmoryRoomLighting[1] = {
    { .lights = &gShelterB1ArmoryRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 sleeping quarters, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1SleepingQuartersRoomLighting[1] = {
    { .lights = &gShelterB1SleepingQuartersRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 main corridor, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1MainCorridorRoomLighting[1] = {
    { .lights = &gShelterB1MainCorridorRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 pod access tunnel, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1PodAccessTunnelRoomLighting[1] = {
    { .lights = &gShelterB1PodAccessTunnelRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 control room, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights and
/// per-view ambient minima. Both inputs require that overlay to remain loaded.
static WorldCoordRoomLighting _gMapShelterB1ControlRoomLighting[1] = {
    { .lights = &gShelterB1ControlRoomRoomLights, .ambientTable = gShelterB1ControlRoomAmbientByView },
};

/// Model-lighting descriptors for Shelter B1 access tunnel, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1AccessTunnelRoomLighting[1] = {
    { .lights = &gShelterB1AccessTunnelRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 golem freezer 1, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights and
/// per-view ambient minima. Both inputs require that overlay to remain loaded.
static WorldCoordRoomLighting _gMapShelterB1GolemFreezer1RoomLighting[1] = {
    { .lights = &gShelterB1GolemFreezer1RoomLights, .ambientTable = gShelterB1GolemFreezer1AmbientByView },
};

/// Model-lighting descriptors for Shelter B1 pod service gantry, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1PodServiceGantryRoomLighting[1] = {
    { .lights = &gShelterB1PodServiceGantryRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 transfer tunnel, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1TransferTunnelRoomLighting[1] = {
    { .lights = &gShelterB1TransferTunnelRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 control room access tunnel, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1ControlRoomAccessTunnelRoomLighting[1] = {
    { .lights = &gShelterB1ControlRoomAccessTunnelRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B2 elevator, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB2ElevatorRoomLighting[1] = {
    { .lights = &gShelterB2ElevatorRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B2 elevator hall, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB2ElevatorHallRoomLighting[1] = {
    { .lights = &gShelterB2ElevatorHallRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B2 south maintenance walkway, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB2SouthMaintenanceWalkwayRoomLighting[1] = {
    { .lights = &gShelterB2SouthMaintenanceWalkwayRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B2 operating room, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights and
/// per-view ambient minima. Both inputs require that overlay to remain loaded.
static WorldCoordRoomLighting _gMapShelterB2OperatingRoomLighting[1] = {
    { .lights = &gShelterB2OperatingRoomRoomLights, .ambientTable = gShelterB2OperatingRoomRoomAmbient },
};

/// Model-lighting descriptors for Shelter B2 north maintenance walkway, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB2NorthMaintenanceWalkwayRoomLighting[1] = {
    { .lights = &gShelterB2NorthMaintenanceWalkwayLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B2 laboratory, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights and
/// per-view ambient minima. Both inputs require that overlay to remain loaded.
static WorldCoordRoomLighting _gMapShelterB2LaboratoryRoomLighting[1] = {
    { .lights = &gShelterB2LaboratoryRoomLights, .ambientTable = gShelterB2LaboratoryViewAmbientTable },
};

/// Model-lighting descriptors for Shelter B2 breeding room, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights and
/// per-view ambient minima. Both inputs require that overlay to remain loaded.
static WorldCoordRoomLighting _gMapShelterB2BreedingRoomLighting[1] = {
    { .lights = &gShelterB2BreedingRoomRoomLights, .ambientTable = gShelterB2BreedingRoomAmbientByView },
};

/// Model-lighting descriptors for Shelter B2 main corridor, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB2MainCorridorRoomLighting[1] = {
    { .lights = &gShelterB2MainCorridorRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B2 septic tank, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB2SepticTankRoomLighting[1] = {
    { .lights = &gShelterB2SepticTankRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter area 36, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterR36RoomLighting[1] = {
    { .lights = &gShelterR36RoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter area 37, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterR37RoomLighting[1] = {
    { .lights = &gShelterR37RoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter 1F heliport (stage 4), room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelter1fHeliportS4RoomLighting[1] = {
    { .lights = &gShelter1fHeliportS4RoomLights, .ambientTable = NULL },
};

/// Dumping-hole lighting descriptors for Mine/Shelter area 39, rooms 1 and 2.
///
/// The map overlay owns these descriptors; their light and per-view ambient
/// inputs belong to the loaded dumping-hole overlay and require its lifetime.
static WorldCoordRoomLighting _gMapShelterDumpingHoleRoomLighting[2] = {
    { &gShelterB3DumpingHoleRoom1Lights, gShelterB3DumpingHoleRoom1AmbientByView },
    { &gShelterB3DumpingHoleRoom2Lights, gShelterB3DumpingHoleRoom2AmbientByView },
};

/// Model-lighting descriptors for Shelter B3 incinerator control room, rooms 1 and 2.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
/// Both room variants share the same light collection and default ambient.
static WorldCoordRoomLighting _gMapShelterB3IncineratorControlRoomLighting[SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_COUNT] = {
    { .lights = &gShelterB3IncineratorControlRoomLights, .ambientTable = NULL },
    { .lights = &gShelterB3IncineratorControlRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B3 elevator hall, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB3ElevatorHallRoomLighting[1] = {
    { .lights = &gShelterB3ElevatorHallRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B4 lower sewer, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB4LowerSewerRoomLighting[1] = {
    { .lights = &gShelterB4LowerSewerRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B4 upper sewer, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB4UpperSewerRoomLighting[1] = {
    { .lights = &gShelterB4UpperSewerRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B4 water supply, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights and
/// per-view ambient minima. Both inputs require that overlay to remain loaded.
static WorldCoordRoomLighting _gMapShelterB4WaterSupplyRoomLighting[1] = {
    { .lights = &gShelterB4WaterSupplyRoomLights, .ambientTable = gShelterB4WaterSupplyRoomAmbient },
};

/// Model-lighting descriptors for Shelter area 47, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterR47RoomLighting[1] = {
    { .lights = &gShelterR47RoomLights, .ambientTable = NULL },
};

WorldCoordRoomLighting* gMapShelterRoomLightingTables[49] = {
    [GAME_AREA_MINE_MESA - 1]                             = gMineMesaRoomLighting,
    [GAME_AREA_MINE_CAVERN - 1]                           = gMineCavernRoomLighting,
    [GAME_AREA_MINE_TUNNEL_ENTRANCE - 1]                  = gMineTunnelEntranceRoomLighting,
    [GAME_AREA_MINE_TUNNEL - 1]                           = gMineTunnelRoomLighting,
    [GAME_AREA_MINE_GORGE - 1]                            = gMineGorgeRoomLighting,
    [GAME_AREA_MINE_REFUGE - 1]                           = gMineRefugeRoomLighting,
    [GAME_AREA_MINE_FORKED_TUNNEL - 1]                    = gMineForkedTunnelRoomLighting,
    [GAME_AREA_MINE_SECRET_PASSAGE - 1]                   = gMineSecretPassageRoomLighting,
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL - 1]              = gShelterB1ElevatorHallRoomLighting,
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY - 1]  = gShelterB1SouthMaintenanceWalkwayRoomLighting,
    [GAME_AREA_SHELTER_B1_STOREROOM - 1]                  = gShelterB1StoreroomRoomLighting,
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY - 1]  = _gMapShelterB1NorthMaintenanceWalkwayRoomLighting,
    [GAME_AREA_SHELTER_B1_ARMORY - 1]                     = _gMapShelterB1ArmoryRoomLighting,
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS - 1]          = _gMapShelterB1SleepingQuartersRoomLighting,
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR - 1]              = _gMapShelterB1MainCorridorRoomLighting,
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM - 1]         = gShelterB1SterilizationRoomLighting,
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL - 1]          = _gMapShelterB1PodAccessTunnelRoomLighting,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM - 1]               = _gMapShelterB1ControlRoomLighting,
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL - 1]              = _gMapShelterB1AccessTunnelRoomLighting,
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING - 1]        = gShelterB1UndergroundParkingRoomLighting,
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 - 1]            = _gMapShelterB1GolemFreezer1RoomLighting,
    [GAME_AREA_SHELTER_B2_POD_BOTTOM - 1]                 = gShelterB2PodBottomRoomLighting,
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY - 1]         = _gMapShelterB1PodServiceGantryRoomLighting,
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL - 1]            = _gMapShelterB1TransferTunnelRoomLighting,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL - 1] = _gMapShelterB1ControlRoomAccessTunnelRoomLighting,
    [GAME_AREA_SHELTER_B2_ELEVATOR - 1]                   = _gMapShelterB2ElevatorRoomLighting,
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL - 1]              = _gMapShelterB2ElevatorHallRoomLighting,
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY - 1]  = _gMapShelterB2SouthMaintenanceWalkwayRoomLighting,
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM - 1]             = _gMapShelterB2OperatingRoomLighting,
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY - 1]  = _gMapShelterB2NorthMaintenanceWalkwayRoomLighting,
    [GAME_AREA_SHELTER_B2_LABORATORY - 1]                 = _gMapShelterB2LaboratoryRoomLighting,
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM - 1]              = _gMapShelterB2BreedingRoomLighting,
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR - 1]              = _gMapShelterB2MainCorridorRoomLighting,
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK - 1]                = _gMapShelterB2SepticTankRoomLighting,
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL - 1]          = gShelterB2PodAccessTunnelRoomLighting,
    [GAME_AREA_SHELTER_R36 - 1]                           = _gMapShelterR36RoomLighting,
    [GAME_AREA_SHELTER_R37 - 1]                           = _gMapShelterR37RoomLighting,
    [GAME_AREA_SHELTER_1F_HELIPORT_S4 - 1]                = _gMapShelter1fHeliportS4RoomLighting,
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE - 1]               = _gMapShelterDumpingHoleRoomLighting,
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR - 1]        = gShelterB3GarbageIncineratorRoomLighting,
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM - 1]   = _gMapShelterB3IncineratorControlRoomLighting,
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL - 1]              = _gMapShelterB3ElevatorHallRoomLighting,
    [GAME_AREA_SHELTER_B4_LOWER_SEWER - 1]                = _gMapShelterB4LowerSewerRoomLighting,
    [GAME_AREA_SHELTER_B4_UPPER_SEWER - 1]                = _gMapShelterB4UpperSewerRoomLighting,
    [GAME_AREA_SHELTER_B4_RESERVOIR - 1]                  = gShelterB4ReservoirRoomLighting,
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY - 1]               = _gMapShelterB4WaterSupplyRoomLighting,
    [GAME_AREA_SHELTER_R47 - 1]                           = _gMapShelterR47RoomLighting,
    [GAME_AREA_SHELTER_R48 - 1]                           = gShelterR48RoomLighting,
    [GAME_AREA_SHELTER_R49 - 1]                           = gShelterR49RoomLighting,
};

DirectionWarpEntry* gMapShelterAreaWarpEntries[GAME_AREA_SHELTER_R49] = {
    [GAME_AREA_MINE_MESA - 1]                             = gMineMesaWarpEntries,
    [GAME_AREA_MINE_CAVERN - 1]                           = gMineCavernWarpEntries,
    [GAME_AREA_MINE_TUNNEL_ENTRANCE - 1]                  = gMineTunnelEntranceWarpEntries,
    [GAME_AREA_MINE_TUNNEL - 1]                           = gMineTunnelWarpEntries,
    [GAME_AREA_MINE_GORGE - 1]                            = gMineGorgeWarpEntries,
    [GAME_AREA_MINE_REFUGE - 1]                           = gMineRefugeWarpEntries,
    [GAME_AREA_MINE_FORKED_TUNNEL - 1]                    = gMineForkedTunnelWarpEntries,
    [GAME_AREA_MINE_SECRET_PASSAGE - 1]                   = gMineSecretPassageWarpEntries,
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL - 1]              = gShelterB1ElevatorHallWarpEntries,
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY - 1]  = gShelterB1SouthMaintenanceWalkwayWarpEntries,
    [GAME_AREA_SHELTER_B1_STOREROOM - 1]                  = gShelterB1StoreroomWarpEntries,
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY - 1]  = gShelterB1NorthMaintenanceWalkwayWarpEntries,
    [GAME_AREA_SHELTER_B1_ARMORY - 1]                     = gShelterB1ArmoryWarpEntries,
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS - 1]          = gShelterB1SleepingQuartersWarpEntries,
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR - 1]              = gShelterB1MainCorridorWarpEntries,
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM - 1]         = gShelterB1SterilizationRoomWarpEntries,
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL - 1]          = gShelterB1PodAccessTunnelWarpEntries,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM - 1]               = gShelterB1ControlRoomWarpEntries,
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL - 1]              = gShelterB1AccessTunnelWarpEntries,
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING - 1]        = gShelterB1UndergroundParkingWarpEntries,
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 - 1]            = gShelterB1GolemFreezer1WarpEntries,
    [GAME_AREA_SHELTER_B2_POD_BOTTOM - 1]                 = gShelterB2PodBottomWarpEntries,
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY - 1]         = gShelterB1PodServiceGantryWarpEntries,
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL - 1]            = gShelterB1TransferTunnelWarpEntries,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL - 1] = gShelterB1ControlRoomAccessTunnelWarpEntries,
    [GAME_AREA_SHELTER_B2_ELEVATOR - 1]                   = gShelterB2ElevatorWarps,
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL - 1]              = gShelterB2ElevatorHallWarpEntries,
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY - 1]  = gShelterB2SouthMaintenanceWalkwayWarpEntries,
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM - 1]             = gShelterB2OperatingRoomWarpEntries,
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY - 1]  = gShelterB2NorthMaintenanceWalkwayWarpEntries,
    [GAME_AREA_SHELTER_B2_LABORATORY - 1]                 = gShelterB2LaboratoryWarpEntries,
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM - 1]              = gShelterB2BreedingRoomWarpEntries,
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR - 1]              = gShelterB2MainCorridorWarpEntries,
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK - 1]                = gShelterB2SepticTankWarpEntries,
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL - 1]          = gShelterB2PodAccessTunnelWarpEntries,
    [GAME_AREA_SHELTER_R36 - 1]                           = gShelterR36WarpEntries,
    [GAME_AREA_SHELTER_R37 - 1]                           = gShelterR37WarpEntries,
    [GAME_AREA_SHELTER_1F_HELIPORT_S4 - 1]                = gShelter1fHeliportS4WarpEntries,
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE - 1]               = gShelterB3DumpingHoleWarpEntries,
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR - 1]        = gShelterB3GarbageIncineratorWarpEntries,
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM - 1]   = gShelterB3IncineratorControlRoomWarpEntries,
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL - 1]              = gShelterB3ElevatorHallWarpEntries,
    [GAME_AREA_SHELTER_B4_LOWER_SEWER - 1]                = gShelterB4LowerSewerWarpEntries,
    [GAME_AREA_SHELTER_B4_UPPER_SEWER - 1]                = gShelterB4UpperSewerWarpEntries,
    [GAME_AREA_SHELTER_B4_RESERVOIR - 1]                  = gShelterB4ReservoirWarpEntries,
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY - 1]               = gShelterB4WaterSupplyWarpEntries,
    [GAME_AREA_SHELTER_R47 - 1]                           = gShelterR47WarpEntries,
    [GAME_AREA_SHELTER_R48 - 1]                           = gShelterR48WarpEntries,
    [GAME_AREA_SHELTER_R49 - 1]                           = gShelterR49WarpEntries,
};

/// Borrowed per-room logical-view count arrays for all 49 Mine/Shelter areas.
///
/// Area ID minus one selects a populated pointer; room ID minus one selects a
/// signed halfword count within that room overlay's own extent. Neither level
/// terminates. Keep this map and the selected room overlay loaded for lookup.
static ViewCount* _gMapShelterAreaViewCounts[GAME_AREA_SHELTER_R49] = {
    [GAME_AREA_MINE_MESA - 1]                             = gMineMesaRoomViewCounts,
    [GAME_AREA_MINE_CAVERN - 1]                           = gMineCavernRoomViewCounts,
    [GAME_AREA_MINE_TUNNEL_ENTRANCE - 1]                  = gMineTunnelEntranceRoomViewCounts,
    [GAME_AREA_MINE_TUNNEL - 1]                           = gMineTunnelRoomViewCounts,
    [GAME_AREA_MINE_GORGE - 1]                            = gMineGorgeRoomViewCounts,
    [GAME_AREA_MINE_REFUGE - 1]                           = gMineRefugeRoomViewCounts,
    [GAME_AREA_MINE_FORKED_TUNNEL - 1]                    = gMineForkedTunnelRoomViewCounts,
    [GAME_AREA_MINE_SECRET_PASSAGE - 1]                   = gMineSecretPassageRoomViewCounts,
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL - 1]              = gShelterB1ElevatorHallRoomViewCounts,
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY - 1]  = gShelterB1SouthMaintenanceWalkwayRoomViewCounts,
    [GAME_AREA_SHELTER_B1_STOREROOM - 1]                  = gShelterB1StoreroomRoomViewCounts,
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY - 1]  = gShelterB1NorthMaintenanceWalkwayRoomViewCounts,
    [GAME_AREA_SHELTER_B1_ARMORY - 1]                     = gShelterB1ArmoryRoomViewCounts,
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS - 1]          = gShelterB1SleepingQuartersRoomViewCounts,
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR - 1]              = gShelterB1MainCorridorRoomViewCounts,
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM - 1]         = gShelterB1SterilizationRoomViewCounts,
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL - 1]          = gShelterB1PodAccessTunnelRoomViewCounts,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM - 1]               = gShelterB1ControlRoomRoomViewCounts,
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL - 1]              = gShelterB1AccessTunnelRoomViewCounts,
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING - 1]        = gShelterB1UndergroundParkingRoomViewCounts,
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 - 1]            = gShelterB1GolemFreezer1ViewCounts,
    [GAME_AREA_SHELTER_B2_POD_BOTTOM - 1]                 = gShelterB2PodBottomRoomViewCounts,
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY - 1]         = gShelterB1PodServiceGantryRoomViewCounts,
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL - 1]            = gShelterB1TransferTunnelRoomViewCounts,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL - 1] = gShelterB1ControlRoomAccessTunnelRoomViewCounts,
    [GAME_AREA_SHELTER_B2_ELEVATOR - 1]                   = gShelterB2ElevatorViewCounts,
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL - 1]              = gShelterB2ElevatorHallRoomViewCounts,
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY - 1]  = gShelterB2SouthMaintenanceWalkwayRoomViewCounts,
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM - 1]             = gShelterB2OperatingRoomRoomViewCounts,
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY - 1]  = gShelterB2NorthMaintenanceWalkwayRoomViewCounts,
    [GAME_AREA_SHELTER_B2_LABORATORY - 1]                 = gShelterB2LaboratoryRoomViewCounts,
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM - 1]              = gShelterB2BreedingRoomRoomViewCounts,
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR - 1]              = gShelterB2MainCorridorViewCounts,
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK - 1]                = gShelterB2SepticTankRoomViewCounts,
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL - 1]          = gShelterB2PodAccessTunnelRoomViewCounts,
    [GAME_AREA_SHELTER_R36 - 1]                           = gShelterR36ViewCounts,
    [GAME_AREA_SHELTER_R37 - 1]                           = gShelterR37ViewCounts,
    [GAME_AREA_SHELTER_1F_HELIPORT_S4 - 1]                = gShelter1fHeliportS4RoomViewCounts,
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE - 1]               = gShelterB3DumpingHoleViewCounts,
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR - 1]        = gShelterB3GarbageIncineratorRoomViewCounts,
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM - 1]   = gShelterB3IncineratorControlRoomViewCounts,
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL - 1]              = gShelterB3ElevatorHallRoomViewCounts,
    [GAME_AREA_SHELTER_B4_LOWER_SEWER - 1]                = gShelterB4LowerSewerRoomViewCounts,
    [GAME_AREA_SHELTER_B4_UPPER_SEWER - 1]                = gShelterB4UpperSewerViewCounts,
    [GAME_AREA_SHELTER_B4_RESERVOIR - 1]                  = gShelterB4ReservoirRoomViewCounts,
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY - 1]               = gShelterB4WaterSupplyViewCounts,
    [GAME_AREA_SHELTER_R47 - 1]                           = gShelterR47ViewCounts,
    [GAME_AREA_SHELTER_R48 - 1]                           = gShelterR48ViewCounts,
    [GAME_AREA_SHELTER_R49 - 1]                           = gShelterR49RoomViewCounts,
};

ViewCountTable gMapShelterViewCountTable = { .viewCounts = _gMapShelterAreaViewCounts };

/// Room-1 collision resources for Shelter B1 north maintenance walkway.
///
/// Area 12 selects this single descriptor; only room ID 1 is valid, with
/// no terminator. Borrows the mutable room grid, view boundaries and actions,
/// plus sight occluders. Keep the room resources loaded until the
/// active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapShelterB1NorthMaintenanceWalkwayRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB1NorthMaintenanceWalkwayCollisionGrid,
        .viewBoundaryTriggers = gShelterB1NorthMaintenanceWalkwayViewBoundaryTriggers,
        .actionTriggers       = gShelterB1NorthMaintenanceWalkwayActionTriggers,
        .occluders            = gShelterB1NorthMaintenanceWalkwaySightOccluders,
    },
};

/// Room-1 collision resources for Shelter B1 armory.
///
/// Area 13 selects this single descriptor; only room ID 1 is valid, with
/// no terminator. Borrows the mutable room grid, view boundaries and actions;
/// there are no sight occluders. Keep the room resources loaded until the
/// active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapShelterB1ArmoryRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB1ArmoryCollisionGrid,
        .viewBoundaryTriggers = gShelterB1ArmoryViewBoundaryTriggers,
        .actionTriggers       = gShelterB1ArmoryActionTriggers,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter B1 sleeping quarters.
///
/// Area 14 selects this array; room ID minus one selects room 1.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB1SleepingQuartersRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB1SleepingQuartersCollisionGrid,
        .viewBoundaryTriggers = gShelterB1SleepingQuartersViewBoundaryTriggers,
        .actionTriggers       = gShelterB1SleepingQuartersActionTriggers,
        .occluders            = gShelterB1SleepingQuartersOccluders,
    },
};

/// Collision-resource descriptors for Shelter B1 main corridor.
///
/// Area 15 selects this array; room ID minus one selects room 1.
/// No sight occluders.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB1MainCorridorRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB1MainCorridorCollisionGrid,
        .viewBoundaryTriggers = gShelterB1MainCorridorViewBoundaries,
        .actionTriggers       = gShelterB1MainCorridorWarpTriggers,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter B1 pod access tunnel.
///
/// Area 17 selects this array; room ID minus one selects room 1.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB1PodAccessTunnelRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB1PodAccessTunnelCollisionGrid,
        .viewBoundaryTriggers = gShelterB1PodAccessTunnelViewBoundaryTriggers,
        .actionTriggers       = gShelterB1PodAccessTunnelActionTriggers,
        .occluders            = gShelterB1PodAccessTunnelSightOccluders,
    },
};

/// Room-1 collision resources for Shelter B1 control room.
///
/// Area 18 selects this single descriptor; only room ID 1 is valid, with
/// no terminator. Borrows the mutable room grid, view boundaries and actions;
/// there are no sight occluders. Keep the room resources loaded until the
/// active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapShelterB1ControlRoomRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB1ControlRoomRoomCollision,
        .viewBoundaryTriggers = gShelterB1ControlRoomViewBoundaryTriggers,
        .actionTriggers       = gShelterB1ControlRoomActionTriggers,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter B1 access tunnel.
///
/// Area 19 selects this array; room ID minus one selects room 1.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB1AccessTunnelRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB1AccessTunnelCollisionGrid,
        .viewBoundaryTriggers = gShelterB1AccessTunnelViewBoundaryTriggers,
        .actionTriggers       = gShelterB1AccessTunnelWarpTriggers,
        .occluders            = gShelterB1AccessTunnelSightOccluders,
    },
};

/// Collision-resource descriptors for Shelter B1 golem freezer 1.
///
/// Area 21 selects this array; room ID minus one selects room 1.
/// No sight occluders.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB1GolemFreezer1RoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB1GolemFreezer1RoomCollision,
        .viewBoundaryTriggers = gShelterB1GolemFreezer1ViewBoundaryTriggers,
        .actionTriggers       = gShelterB1GolemFreezer1ActionTriggers,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter B1 pod service gantry.
///
/// Area 23 selects this array; room ID minus one selects room 1.
/// No view boundaries, action triggers, sight occluders.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB1PodServiceGantryRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB1PodServiceGantryRoomCollision,
        .viewBoundaryTriggers = NULL,
        .actionTriggers       = NULL,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter B1 transfer tunnel.
///
/// Area 24 selects this array; room ID minus one selects room 1.
/// No sight occluders.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB1TransferTunnelRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB1TransferTunnelRoomCollision,
        .viewBoundaryTriggers = gShelterB1TransferTunnelViewBoundaryTriggers,
        .actionTriggers       = gShelterB1TransferTunnelWarpTriggers,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter B1 control room access tunnel.
///
/// Area 25 selects this array; room ID minus one selects room 1.
/// No sight occluders.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB1ControlRoomAccessTunnelRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB1ControlRoomAccessTunnelCollisionGrid,
        .viewBoundaryTriggers = gShelterB1ControlRoomAccessTunnelViewBoundaryTriggers,
        .actionTriggers       = gShelterB1ControlRoomAccessTunnelWarpTriggers,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter B2 elevator.
///
/// Area 26 selects this array; room ID minus one selects room 1.
/// No sight occluders.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB2ElevatorRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB2ElevatorCollisionGrid,
        .viewBoundaryTriggers = gShelterB2ElevatorViewBoundaryTriggers,
        .actionTriggers       = gShelterB2ElevatorActionTriggers,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter B2 elevator hall.
///
/// Area 27 selects this array; room ID minus one selects room 1.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB2ElevatorHallRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB2ElevatorHallRoomCollision,
        .viewBoundaryTriggers = gShelterB2ElevatorHallViewBoundaryTriggers,
        .actionTriggers       = gShelterB2ElevatorHallWarpTriggers,
        .occluders            = gShelterB2ElevatorHallSightOccluders,
    },
};

/// Room-1 collision resources for Shelter B2 south maintenance walkway.
///
/// Area 28 selects this single descriptor; only room ID 1 is valid, with
/// no terminator. Borrows the mutable room grid, view boundaries and actions,
/// plus sight occluders. Keep the room resources loaded until the
/// active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapShelterB2SouthMaintenanceWalkwayRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB2SouthMaintenanceWalkwayCollisionGrid,
        .viewBoundaryTriggers = gShelterB2SouthMaintenanceWalkwayViewBoundaryTriggers,
        .actionTriggers       = gShelterB2SouthMaintenanceWalkwayActionTriggers,
        .occluders            = gShelterB2SouthMaintenanceWalkwaySightOccluders,
    },
};

/// Collision-resource descriptors for Shelter B2 operating room.
///
/// Area 29 selects this array; room ID minus one selects room 1.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB2OperatingRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB2OperatingRoomCollisionGrid,
        .viewBoundaryTriggers = gShelterB2OperatingRoomViewBoundaryTriggers,
        .actionTriggers       = gShelterB2OperatingRoomActionTriggers,
        .occluders            = gShelterB2OperatingRoomOccluders,
    },
};

/// Room-1 collision resources for Shelter B2 north maintenance walkway.
///
/// Area 30 selects this single descriptor; only room ID 1 is valid, with
/// no terminator. Borrows the mutable room grid, view boundaries and actions,
/// plus sight occluders. Keep the room resources loaded until the
/// active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapShelterB2NorthMaintenanceWalkwayRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB2NorthMaintenanceWalkwayCollisionGrid,
        .viewBoundaryTriggers = gShelterB2NorthMaintenanceWalkwayViewBoundaryTriggers,
        .actionTriggers       = gShelterB2NorthMaintenanceWalkwayActionTriggers,
        .occluders            = gShelterB2NorthMaintenanceWalkwaySightOccluders,
    },
};

/// Collision-resource descriptors for Shelter B2 laboratory.
///
/// Area 31 selects this array; room ID minus one selects room 1.
/// No sight occluders.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB2LaboratoryRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB2LaboratoryCollisionGrid,
        .viewBoundaryTriggers = gShelterB2LaboratoryViewBoundaryTriggers,
        .actionTriggers       = gShelterB2LaboratoryActionTriggers,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter B2 breeding room.
///
/// Area 32 selects this array; room ID minus one selects room 1.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB2BreedingRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB2BreedingRoomCollisionGrid,
        .viewBoundaryTriggers = gShelterB2BreedingRoomViewBoundaryTriggers,
        .actionTriggers       = gShelterB2BreedingRoomActionTriggers,
        .occluders            = gShelterB2BreedingRoomOccluders,
    },
};

/// Collision-resource descriptors for Shelter B2 main corridor.
///
/// Area 33 selects this array; room ID minus one selects room 1.
/// No sight occluders.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB2MainCorridorRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB2MainCorridorCollisionGrid,
        .viewBoundaryTriggers = gShelterB2MainCorridorViewBoundaryTriggers,
        .actionTriggers       = gShelterB2MainCorridorActionTriggers,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter B2 septic tank.
///
/// Area 34 selects this array; room ID minus one selects room 1.
/// No sight occluders.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB2SepticTankRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB2SepticTankCollisionGrid,
        .viewBoundaryTriggers = gShelterB2SepticTankViewBoundaryTriggers,
        .actionTriggers       = gShelterB2SepticTankActionTriggers,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter area 36.
///
/// Area 36 selects this array; room ID minus one selects room 1.
/// No action triggers, sight occluders.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterR36RoomCollisionResources[1] = {
    {
        .grid                 = &gShelterR36FloorCollisionGrid,
        .viewBoundaryTriggers = gShelterR36ViewBoundaryTriggers,
        .actionTriggers       = NULL,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter area 37.
///
/// Area 37 selects this array; room ID minus one selects room 1.
/// No action triggers, sight occluders.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterR37RoomCollisionResources[1] = {
    {
        .grid                 = &gShelterR37RoomCollision,
        .viewBoundaryTriggers = gShelterR37ViewBoundaryTriggers,
        .actionTriggers       = NULL,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter 1F heliport.
///
/// Area 38 selects this array; room ID minus one selects room 1.
/// No action triggers, sight occluders.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelter1fHeliportS4RoomCollisionResources[1] = {
    {
        .grid                 = &gShelter1fHeliportS4CollisionGrid,
        .viewBoundaryTriggers = gShelter1fHeliportS4ViewBoundaryTriggers,
        .actionTriggers       = NULL,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter B3 incinerator control room.
///
/// Area 41 selects this array; room ID minus one selects rooms 1 and 2.
/// Both rooms share the grid, view boundaries and occluders, with separate actions.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB3IncineratorControlRoomCollisionResources[SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_COUNT] = {
    {
        .grid                 = &gShelterB3IncineratorControlRoomCollisionGrid,
        .viewBoundaryTriggers = gShelterB3IncineratorControlRoomViewBoundaryTriggers,
        .actionTriggers       = gShelterB3IncineratorControlRoomRoom1ActionTriggers,
        .occluders            = gShelterB3IncineratorControlRoomOccluders,
    },
    {
        .grid                 = &gShelterB3IncineratorControlRoomCollisionGrid,
        .viewBoundaryTriggers = gShelterB3IncineratorControlRoomViewBoundaryTriggers,
        .actionTriggers       = gShelterB3IncineratorControlRoomRoom2ActionTriggers,
        .occluders            = gShelterB3IncineratorControlRoomOccluders,
    },
};

/// Collision-resource descriptors for Shelter B3 elevator hall.
///
/// Area 42 selects this array; room ID minus one selects room 1.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB3ElevatorHallRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB3ElevatorHallCollisionGrid,
        .viewBoundaryTriggers = gShelterB3ElevatorHallViewBoundaryTriggers,
        .actionTriggers       = gShelterB3ElevatorHallActionTriggers,
        .occluders            = gShelterB3ElevatorHallOccluders,
    },
};

/// Collision-resource descriptors for Shelter B4 lower sewer.
///
/// Area 43 selects this array; room ID minus one selects room 1.
/// No sight occluders.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB4LowerSewerRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB4LowerSewerCollisionGrid,
        .viewBoundaryTriggers = gShelterB4LowerSewerViewBoundaryTriggers,
        .actionTriggers       = gShelterB4LowerSewerActionTriggers,
        .occluders            = NULL,
    },
};

/// Collision-resource descriptors for Shelter B4 upper sewer.
///
/// Area 44 selects this array; room ID minus one selects room 1.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB4UpperSewerRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB4UpperSewerCollisionGrid,
        .viewBoundaryTriggers = gShelterB4UpperSewerViewBoundaryTriggers,
        .actionTriggers       = gShelterB4UpperSewerActionTriggers,
        .occluders            = gShelterB4UpperSewerOccluders,
    },
};

/// Collision-resource descriptors for Shelter B4 water supply.
///
/// Area 46 selects this array; room ID minus one selects room 1.
/// Borrows mutable room-overlay resources: keep the grid and its pools loaded
/// while active, and trigger/occluder storage loaded until unlinked.
static WorldCollisionRoomResources _gMapShelterB4WaterSupplyRoomCollisionResources[1] = {
    {
        .grid                 = &gShelterB4WaterSupplyCollisionGrid,
        .viewBoundaryTriggers = gShelterB4WaterSupplyViewBoundaryTriggers,
        .actionTriggers       = gShelterB4WaterSupplyActionTriggers,
        .occluders            = gShelterB4WaterSupplyOccluders,
    },
};

/// Room-1 collision resources for Shelter area 47.
///
/// Area 47 selects this single descriptor; only room ID 1 is valid, with
/// no terminator. Borrows the mutable room grid, view boundaries and actions;
/// there are no sight occluders. Keep the room resources loaded until the
/// active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapShelterR47RoomCollisionResources[1] = {
    {
        .grid                 = &gShelterR47CollisionGrid,
        .viewBoundaryTriggers = gShelterR47ViewBoundaryTriggers,
        .actionTriggers       = gShelterR47ActionTriggers,
        .occluders            = NULL,
    },
};

/// Per-area collision room arrays for Mine/Shelter.
///
/// Select area - 1 for IDs 1..49. Every area entry is populated.
/// Each array uses room - 1 within its own extent, without a terminator.
/// Descriptors may be map-owned or borrowed; their mutable room resources must
/// remain loaded until the active grid and linked lists are retired.
static WorldCollisionRoomResources* _gMapShelterWorldCollisionAreaRooms[GAME_AREA_SHELTER_R49] = {
    [GAME_AREA_MINE_MESA - 1]                             = gMineMesaRoomCollisionResources,
    [GAME_AREA_MINE_CAVERN - 1]                           = gMineCavernRoomCollisionResources,
    [GAME_AREA_MINE_TUNNEL_ENTRANCE - 1]                  = gMineTunnelEntranceRoomCollisionResources,
    [GAME_AREA_MINE_TUNNEL - 1]                           = gMineTunnelRoomCollisionResources,
    [GAME_AREA_MINE_GORGE - 1]                            = gMineGorgeRoomCollisionResources,
    [GAME_AREA_MINE_REFUGE - 1]                           = gMineRefugeRoomCollisionResources,
    [GAME_AREA_MINE_FORKED_TUNNEL - 1]                    = gMineForkedTunnelRoomCollisionResources,
    [GAME_AREA_MINE_SECRET_PASSAGE - 1]                   = gMineSecretPassageRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL - 1]              = gShelterB1ElevatorHallRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY - 1]  = gShelterB1SouthMaintenanceWalkwayRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_STOREROOM - 1]                  = gShelterB1StoreroomRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY - 1]  = _gMapShelterB1NorthMaintenanceWalkwayRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_ARMORY - 1]                     = _gMapShelterB1ArmoryRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS - 1]          = _gMapShelterB1SleepingQuartersRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR - 1]              = _gMapShelterB1MainCorridorRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM - 1]         = gShelterB1SterilizationRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL - 1]          = _gMapShelterB1PodAccessTunnelRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM - 1]               = _gMapShelterB1ControlRoomRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL - 1]              = _gMapShelterB1AccessTunnelRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING - 1]        = gShelterB1UndergroundParkingRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 - 1]            = _gMapShelterB1GolemFreezer1RoomCollisionResources,
    [GAME_AREA_SHELTER_B2_POD_BOTTOM - 1]                 = gShelterB2PodBottomRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY - 1]         = _gMapShelterB1PodServiceGantryRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL - 1]            = _gMapShelterB1TransferTunnelRoomCollisionResources,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL - 1] = _gMapShelterB1ControlRoomAccessTunnelRoomCollisionResources,
    [GAME_AREA_SHELTER_B2_ELEVATOR - 1]                   = _gMapShelterB2ElevatorRoomCollisionResources,
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL - 1]              = _gMapShelterB2ElevatorHallRoomCollisionResources,
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY - 1]  = _gMapShelterB2SouthMaintenanceWalkwayRoomCollisionResources,
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM - 1]             = _gMapShelterB2OperatingRoomCollisionResources,
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY - 1]  = _gMapShelterB2NorthMaintenanceWalkwayRoomCollisionResources,
    [GAME_AREA_SHELTER_B2_LABORATORY - 1]                 = _gMapShelterB2LaboratoryRoomCollisionResources,
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM - 1]              = _gMapShelterB2BreedingRoomCollisionResources,
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR - 1]              = _gMapShelterB2MainCorridorRoomCollisionResources,
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK - 1]                = _gMapShelterB2SepticTankRoomCollisionResources,
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL - 1]          = gShelterB2PodAccessTunnelRoomCollisionResources,
    [GAME_AREA_SHELTER_R36 - 1]                           = _gMapShelterR36RoomCollisionResources,
    [GAME_AREA_SHELTER_R37 - 1]                           = _gMapShelterR37RoomCollisionResources,
    [GAME_AREA_SHELTER_1F_HELIPORT_S4 - 1]                = _gMapShelter1fHeliportS4RoomCollisionResources,
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE - 1]               = gShelterB3DumpingHoleRoomCollisionResources,
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR - 1]        = gShelterB3GarbageIncineratorRoomCollisionResources,
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM - 1]   = _gMapShelterB3IncineratorControlRoomCollisionResources,
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL - 1]              = _gMapShelterB3ElevatorHallRoomCollisionResources,
    [GAME_AREA_SHELTER_B4_LOWER_SEWER - 1]                = _gMapShelterB4LowerSewerRoomCollisionResources,
    [GAME_AREA_SHELTER_B4_UPPER_SEWER - 1]                = _gMapShelterB4UpperSewerRoomCollisionResources,
    [GAME_AREA_SHELTER_B4_RESERVOIR - 1]                  = gShelterB4ReservoirRoomCollisionResources,
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY - 1]               = _gMapShelterB4WaterSupplyRoomCollisionResources,
    [GAME_AREA_SHELTER_R47 - 1]                           = _gMapShelterR47RoomCollisionResources,
    [GAME_AREA_SHELTER_R48 - 1]                           = gShelterR48RoomCollisionResources,
    [GAME_AREA_SHELTER_R49 - 1]                           = gShelterR49RoomCollisionResources,
};

WorldCollisionStageResources gMapShelterWorldCollisionResources = { .areaRooms = _gMapShelterWorldCollisionAreaRooms };

/// Borrowed camera arrays for all 49 Mine/Shelter areas.
///
/// Indexed by area - 1, then mapped camera index - 1; neither level has a
/// terminator. Each room's array supplies its own camera extent, independent of
/// logical-view counts. Keep the map directory and selected room overlay loaded;
/// queued camera tasks borrow the selected record until they apply it.
static ViewCamera* _gMapShelterAreaViewCameras[GAME_AREA_SHELTER_R49] = {
    [GAME_AREA_MINE_MESA - 1]                             = gMineMesaViewCameras,
    [GAME_AREA_MINE_CAVERN - 1]                           = gMineCavernViewCameras,
    [GAME_AREA_MINE_TUNNEL_ENTRANCE - 1]                  = gMineTunnelEntranceViewCameras,
    [GAME_AREA_MINE_TUNNEL - 1]                           = gMineTunnelViewCameras,
    [GAME_AREA_MINE_GORGE - 1]                            = gMineGorgeViewCameras,
    [GAME_AREA_MINE_REFUGE - 1]                           = gMineRefugeViewCameras,
    [GAME_AREA_MINE_FORKED_TUNNEL - 1]                    = gMineForkedTunnelViewCameras,
    [GAME_AREA_MINE_SECRET_PASSAGE - 1]                   = gMineSecretPassageViewCameras,
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL - 1]              = gShelterB1ElevatorHallViewCameras,
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY - 1]  = gShelterB1SouthMaintenanceWalkwayViewCameras,
    [GAME_AREA_SHELTER_B1_STOREROOM - 1]                  = gShelterB1StoreroomViewCameras,
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY - 1]  = gShelterB1NorthMaintenanceWalkwayViewCameras,
    [GAME_AREA_SHELTER_B1_ARMORY - 1]                     = gShelterB1ArmoryViewCameras,
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS - 1]          = gShelterB1SleepingQuartersViewCameras,
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR - 1]              = gShelterB1MainCorridorViewCameras,
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM - 1]         = gShelterB1SterilizationRoomViewCameras,
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL - 1]          = gShelterB1PodAccessTunnelViewCameras,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM - 1]               = gShelterB1ControlRoomViewCameras,
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL - 1]              = gShelterB1AccessTunnelViewCameras,
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING - 1]        = gShelterB1UndergroundParkingViewCameras,
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 - 1]            = gShelterB1GolemFreezer1ViewCameras,
    [GAME_AREA_SHELTER_B2_POD_BOTTOM - 1]                 = gShelterB2PodBottomViewCameras,
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY - 1]         = gShelterB1PodServiceGantryViewCameras,
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL - 1]            = gShelterB1TransferTunnelViewCameras,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL - 1] = gShelterB1ControlRoomAccessTunnelViewCameras,
    [GAME_AREA_SHELTER_B2_ELEVATOR - 1]                   = gShelterB2ElevatorViewCameras,
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL - 1]              = gShelterB2ElevatorHallViewCameras,
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY - 1]  = gShelterB2SouthMaintenanceWalkwayViewCameras,
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM - 1]             = gShelterB2OperatingRoomViewCameras,
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY - 1]  = gShelterB2NorthMaintenanceWalkwayViewCameras,
    [GAME_AREA_SHELTER_B2_LABORATORY - 1]                 = gShelterB2LaboratoryViewCameras,
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM - 1]              = gShelterB2BreedingRoomViewCameras,
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR - 1]              = gShelterB2MainCorridorViewCameras,
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK - 1]                = gShelterB2SepticTankViewCameras,
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL - 1]          = gShelterB2PodAccessTunnelViewCameras,
    [GAME_AREA_SHELTER_R36 - 1]                           = gShelterR36ViewCameras,
    [GAME_AREA_SHELTER_R37 - 1]                           = gShelterR37ViewCameras,
    [GAME_AREA_SHELTER_1F_HELIPORT_S4 - 1]                = gShelter1fHeliportS4ViewCameras,
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE - 1]               = gShelterB3DumpingHoleViewCameras,
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR - 1]        = gShelterB3GarbageIncineratorCameras,
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM - 1]   = gShelterB3IncineratorControlRoomViewCameras,
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL - 1]              = gShelterB3ElevatorHallViewCameras,
    [GAME_AREA_SHELTER_B4_LOWER_SEWER - 1]                = gShelterB4LowerSewerViewCameras,
    [GAME_AREA_SHELTER_B4_UPPER_SEWER - 1]                = gShelterB4UpperSewerViewCameras,
    [GAME_AREA_SHELTER_B4_RESERVOIR - 1]                  = gShelterB4ReservoirViewCameras,
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY - 1]               = gShelterB4WaterSupplyViewCameras,
    [GAME_AREA_SHELTER_R47 - 1]                           = gShelterR47ViewCameras,
    [GAME_AREA_SHELTER_R48 - 1]                           = gShelterR48ViewCameras,
    [GAME_AREA_SHELTER_R49 - 1]                           = gShelterR49ViewCameras,
};

ViewCameraTable gMapShelterViewCameraTable = { .cameras = _gMapShelterAreaViewCameras };

/// 49 area entries borrowing room-local logical-view map directories.
///
/// Indexed by area minus one; rooms and logical views use their own bounds.
/// NULL entries have no maps. Room pointers stay valid only while that room is loaded.
static u8** _gMapShelterAreaViewMaps[49] = {
    gMineMesaRoomViewMaps,
    gMineCavernRoomViewMaps,
    gMineTunnelEntranceRoomViewMaps,
    gMineTunnelRoomViewMaps,
    gMineGorgeRoomViewMaps,
    gMineRefugeRoomViewMaps,
    gMineForkedTunnelRoomViewMaps,
    gMineSecretPassageRoomViewMaps,
    gShelterB1ElevatorHallRoomViewMaps,
    gShelterB1SouthMaintenanceWalkwayRoomViewMaps,
    gShelterB1StoreroomRoomViewMaps,
    gShelterB1NorthMaintenanceWalkwayViewMaps,
    gShelterB1ArmoryRoomViewMaps,
    gShelterB1SleepingQuartersRoomViewMaps,
    gShelterB1MainCorridorRoomViewMaps,
    gShelterB1SterilizationRoomViewMaps,
    gShelterB1PodAccessTunnelRoomViewMaps,
    gShelterB1ControlRoomRoomViewMaps,
    gShelterB1AccessTunnelRoomViewMaps,
    gShelterB1UndergroundParkingRoomViewMaps,
    gShelterB1GolemFreezer1ViewMaps,
    gShelterB2PodBottomRoomViewMaps,
    gShelterB1PodServiceGantryRoomViewMaps,
    gShelterB1TransferTunnelRoomViewMaps,
    gShelterB1ControlRoomAccessTunnelRoomViewMaps,
    gShelterB2ElevatorViewMaps,
    gShelterB2ElevatorHallViewMaps,
    gShelterB2SouthMaintenanceWalkwayRoomViewMaps,
    gShelterB2OperatingRoomRoomViewMaps,
    gShelterB2NorthMaintenanceWalkwayRoomViewMaps,
    gShelterB2LaboratoryRoomViewMaps,
    gShelterB2BreedingRoomRoomViewMaps,
    gShelterB2MainCorridorViewMaps,
    gShelterB2SepticTankViewMaps,
    gShelterB2PodAccessTunnelRoomViewMaps,
    gShelterR36RoomViewMaps,
    gShelterR37ViewMaps,
    gShelter1fHeliportS4RoomViewMaps,
    gShelterB3DumpingHoleViewMaps,
    gShelterB3GarbageIncineratorViewMaps,
    gShelterB3IncineratorControlRoomViewMaps,
    gShelterB3ElevatorHallViewMaps,
    gShelterB4LowerSewerViewMaps,
    gShelterB4UpperSewerViewMaps,
    gShelterB4ReservoirViewMaps,
    gShelterB4WaterSupplyViewMaps,
    gShelterR47ViewMaps,
    gShelterR48ViewMaps,
    gShelterR49RoomViewMaps,
};

ViewIndexTable gMapShelterViewIndexTable = { _gMapShelterAreaViewMaps };

/// 49 area entries borrowing mapped-view sprite arrays from room overlays.
///
/// Indexed by area minus one, then the mapped view minus one. Room pointers
/// must stay loaded; an area entry supplies no sprite-view count or sentinel.
static SpriteView* _gMapShelterAreaSpriteViews[49] = {
    gMineMesaSpriteViews,
    gMineCavernSpriteViews,
    gMineTunnelEntranceSpriteViews,
    gMineTunnelSpriteViews,
    gMineGorgeSpriteViews,
    gMineRefugeSpriteViews,
    gMineForkedTunnelSpriteViews,
    gMineSecretPassageSpriteViews,
    gShelterB1ElevatorHallSpriteViews,
    gShelterB1SouthMaintenanceWalkwaySpriteViews,
    gShelterB1StoreroomSpriteViews,
    gShelterB1NorthMaintenanceWalkwaySpriteViews,
    gShelterB1ArmorySpriteViews,
    gShelterB1SleepingQuartersSpriteViews,
    gShelterB1MainCorridorSpriteViews,
    gShelterB1SterilizationRoomSpriteViews,
    gShelterB1PodAccessTunnelSpriteViews,
    gShelterB1ControlRoomSpriteViews,
    gShelterB1AccessTunnelSpriteViews,
    gShelterB1UndergroundParkingSpriteViews,
    gShelterB1GolemFreezer1SpriteViews,
    gShelterB2PodBottomSpriteViews,
    gShelterB1PodServiceGantrySpriteViews,
    gShelterB1TransferTunnelSpriteViews,
    gShelterB1ControlRoomAccessTunnelSpriteViews,
    gShelterB2ElevatorSpriteViews,
    gShelterB2ElevatorHallSpriteViews,
    gShelterB2SouthMaintenanceWalkwaySpriteViews,
    gShelterB2OperatingRoomSpriteViews,
    gShelterB2NorthMaintenanceWalkwaySpriteViews,
    gShelterB2LaboratorySpriteViews,
    gShelterB2BreedingRoomSpriteViews,
    gShelterB2MainCorridorSpriteViews,
    gShelterB2SepticTankSpriteViews,
    gShelterB2PodAccessTunnelSpriteViews,
    gShelterR36SpriteViews,
    gShelterR37SpriteViews,
    gShelter1fHeliportS4SpriteViews,
    gShelterB3DumpingHoleSpriteViews,
    gShelterB3GarbageIncineratorSpriteViews,
    gShelterB3IncineratorControlRoomSpriteViews,
    gShelterB3ElevatorHallSpriteViews,
    gShelterB4LowerSewerSpriteViews,
    gShelterB4UpperSewerSpriteViews,
    gShelterB4ReservoirSpriteViews,
    gShelterB4WaterSupplySpriteViews,
    gShelterR47SpriteViews,
    gShelterR48SpriteViews,
    gShelterR49SpriteViews,
};

SpriteAreaTable gMapShelterSpriteAreaTable = { _gMapShelterAreaSpriteViews };

WorldCollisionSurfaceProperties** gMapShelterSurfacePropertiesByArea[49] = {
    [GAME_AREA_MINE_MESA - 1]                             = gMineMesaSurfacePropertiesByClass,
    [GAME_AREA_MINE_CAVERN - 1]                           = gMineCavernSurfacePropertiesByClass,
    [GAME_AREA_MINE_TUNNEL_ENTRANCE - 1]                  = gMineTunnelEntranceSurfacePropertiesByClass,
    [GAME_AREA_MINE_TUNNEL - 1]                           = gMineTunnelSurfaceProperties,
    [GAME_AREA_MINE_GORGE - 1]                            = gMineGorgeSurfacePropertiesByClass,
    [GAME_AREA_MINE_REFUGE - 1]                           = gMineRefugeSurfacePropertiesByClass,
    [GAME_AREA_MINE_FORKED_TUNNEL - 1]                    = gMineForkedTunnelSurfacePropertiesByClass,
    [GAME_AREA_MINE_SECRET_PASSAGE - 1]                   = gMineSecretPassageSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL - 1]              = gShelterB1ElevatorHallSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY - 1]  = gShelterB1SouthMaintenanceWalkwaySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_STOREROOM - 1]                  = gShelterB1StoreroomSurfaceClassProperties,
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY - 1]  = gShelterB1NorthMaintenanceWalkwaySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_ARMORY - 1]                     = gShelterB1ArmorySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS - 1]          = gShelterB1SleepingQuartersSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR - 1]              = gShelterB1MainCorridorSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM - 1]         = gShelterB1SterilizationRoomSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL - 1]          = gShelterB1PodAccessTunnelSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM - 1]               = gShelterB1ControlRoomSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL - 1]              = gShelterB1AccessTunnelSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING - 1]        = gShelterB1UndergroundParkingSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 - 1]            = gShelterB1GolemFreezer1SurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_POD_BOTTOM - 1]                 = gShelterB2PodBottomSurfaceProperties,
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY - 1]         = gShelterB1PodServiceGantrySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL - 1]            = gShelterB1TransferTunnelSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL - 1] = gShelterB1ControlRoomAccessTunnelSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_ELEVATOR - 1]                   = gShelterB2ElevatorSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL - 1]              = gShelterB2ElevatorHallSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY - 1]  = gShelterB2SouthMaintenanceWalkwaySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM - 1]             = gShelterB2OperatingRoomSurfaceProperties,
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY - 1]  = gShelterB2NorthMaintenanceWalkwaySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_LABORATORY - 1]                 = gShelterB2LaboratorySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM - 1]              = gShelterB2BreedingRoomSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR - 1]              = gShelterB2MainCorridorSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK - 1]                = gShelterB2SepticTankSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL - 1]          = gShelterB2PodAccessTunnelSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_R36 - 1]                           = gShelterR36SurfacePropertiesByClass,
    [GAME_AREA_SHELTER_R37 - 1]                           = gShelterR37SurfacePropertiesByClass,
    [GAME_AREA_SHELTER_1F_HELIPORT_S4 - 1]                = gShelter1fHeliportS4SurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE - 1]               = gShelterB3DumpingHoleSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR - 1]        = gShelterB3GarbageIncineratorSurfaceProperties,
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM - 1]   = gShelterB3IncineratorControlRoomSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL - 1]              = gShelterB3ElevatorHallSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B4_LOWER_SEWER - 1]                = gShelterB4LowerSewerSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B4_UPPER_SEWER - 1]                = gShelterB4UpperSewerSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B4_RESERVOIR - 1]                  = gShelterB4ReservoirSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY - 1]               = gShelterB4WaterSupplySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_R47 - 1]                           = gShelterR47SurfaceProperties,
    [GAME_AREA_SHELTER_R48 - 1]                           = gShelterR48SurfacePropertiesByClass,
    [GAME_AREA_SHELTER_R49 - 1]                           = gShelterR49SurfacePropertiesByClass,
};

/// 1 object-state placement for Mine Mesa.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
static AreaObjectPlace _gMapShelterMineMesaObjectPlaces[2] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_MINE_MESA_ITEM_BOX_5, .kind = MAP_SHELTER_KIND_ITEM_BOX_5, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 2 object-state placements for Mine Cavern.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
/// Prompt-marked entries collect immediately when their pickup panel opens.
static AreaObjectPlace _gMapShelterMineCavernObjectPlaces[3] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_MINE_CAVERN_JUMPER_PLUG_12C, .kind = MAP_SHELTER_KIND_JUMPER_PLUG_12C, .state = MAP_SHELTER_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_MINE_CAVERN_P229, .kind = MAP_SHELTER_KIND_P229, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 4 object-state placements for Mine Refuge.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
static AreaObjectPlace _gMapShelterMineRefugeObjectPlaces[5] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_MINE_REFUGE_9MM_PARABELLUM, .kind = MAP_SHELTER_KIND_9MM_PARABELLUM, .state = MAP_SHELTER_STATE_REPLENISHABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_MINE_REFUGE_SAVE_POINT, .kind = MAP_SHELTER_KIND_REFUGE_SAVE_POINT, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_MINE_REFUGE_JUMPER_PLUG_120, .kind = MAP_SHELTER_KIND_JUMPER_PLUG_120, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_MINE_REFUGE_JUMPER_PLUG_12C, .kind = MAP_SHELTER_KIND_JUMPER_PLUG_12C, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 2 object-state placements for Mine Forked Tunnel.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
/// Prompt-marked entries collect immediately when their pickup panel opens.
static AreaObjectPlace _gMapShelterMineForkedTunnelObjectPlaces[3] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_MINE_FORKED_TUNNEL_OAK_BOARD, .kind = INVENTORY_COLLECTION_ID_OAK_BOARD, .state = MAP_SHELTER_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_MINE_FORKED_TUNNEL_PROP, .kind = MAP_SHELTER_KIND_FORKED_TUNNEL_PROP, .state = MAP_SHELTER_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 2 object-state placements for Shelter B1 Storeroom.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
static AreaObjectPlace _gMapShelterB1StoreroomObjectPlaces[3] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_STOREROOM_9MM_PARABELLUM, .kind = MAP_SHELTER_KIND_9MM_PARABELLUM, .state = MAP_SHELTER_STATE_REPLENISHABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_STOREROOM_RINGERS_SOLUTION, .kind = MAP_SHELTER_KIND_RINGERS_SOLUTION, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 1 object-state placement for Shelter B1 North Maintenance Walkway.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
/// Prompt-marked entries collect immediately when their pickup panel opens.
static AreaObjectPlace _gMapShelterB1NorthMaintenanceWalkwayObjectPlaces[2] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_NORTH_MAINTENANCE_WALKWAY_BOWMANS_CARD, .kind = INVENTORY_COLLECTION_ID_BOWMANS_CARD, .state = MAP_SHELTER_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 4 object-state placements for Shelter B1 Armory.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
static AreaObjectPlace _gMapShelterB1ArmoryObjectPlaces[5] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_ARMORY_SP12, .kind = MAP_SHELTER_KIND_SP12, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_ARMORY_9MM_PARABELLUM, .kind = MAP_SHELTER_KIND_9MM_PARABELLUM, .state = MAP_SHELTER_STATE_REPLENISHABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_ARMORY_9MM_HYDRA, .kind = MAP_SHELTER_KIND_9MM_HYDRA, .state = MAP_SHELTER_STATE_REPLENISHABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_ARMORY_BUCKSHOT, .kind = MAP_SHELTER_KIND_BUCKSHOT, .state = MAP_SHELTER_STATE_REPLENISHABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 1 object-state placement for Shelter B1 Sleeping Quarters.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
static AreaObjectPlace _gMapShelterB1SleepingQuartersObjectPlaces[2] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_SLEEPING_QUARTERS_AERIS_MAGAZINE, .kind = MAP_SHELTER_KIND_AERIS_MAGAZINE, .state = MAP_SHELTER_STATE_AVAILABLE, .x = 300, .y = -600, .z = 3200, .yaw = 0xD10 },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 2 object-state placements for Shelter B1 Sterilization Room.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
static AreaObjectPlace _gMapShelterB1SterilizationRoomObjectPlaces[3] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_STERILIZATION_ROOM_ITEM_BOX, .kind = MAP_SHELTER_KIND_STERILIZATION_ROOM_ITEM_BOX, .state = MAP_SHELTER_STATE_AVAILABLE, .x = 5144, .y = 0, .z = 13021, .yaw = 0x800 },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_STERILIZATION_ROOM_SAVE_POINT, .kind = MAP_SHELTER_KIND_STERILIZATION_ROOM_SAVE_POINT, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 14 object-state placements for Shelter B1 Underground Parking.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
/// Prompt-marked entries collect immediately when their pickup panel opens.
static AreaObjectPlace _gMapShelterB1UndergroundParkingObjectPlaces[15] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_BOWMANS_CARD, .kind = INVENTORY_COLLECTION_ID_BOWMANS_CARD, .state = MAP_SHELTER_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_YOSHIDAS_CARD, .kind = MAP_SHELTER_KIND_YOSHIDAS_CARD, .state = MAP_SHELTER_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_ELECTRIC_CAR_KEY, .kind = MAP_SHELTER_KIND_ELECTRIC_CAR_KEY, .state = MAP_SHELTER_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_SAVE_POINT, .kind = MAP_SHELTER_KIND_UNDERGROUND_PARKING_SAVE_POINT, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_ITEM_BOX_3, .kind = MAP_SHELTER_KIND_ITEM_BOX_3, .state = MAP_SHELTER_STATE_INACTIVE, .x = -1600, .y = 0, .z = -4479, .yaw = 0x400 },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_ITEM_BOX_4, .kind = MAP_SHELTER_KIND_ITEM_BOX_4, .state = MAP_SHELTER_STATE_INACTIVE, .x = -1600, .y = 4, .z = -3126, .yaw = 0x400 },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_ITEM_BOX_5, .kind = MAP_SHELTER_KIND_ITEM_BOX_5, .state = MAP_SHELTER_STATE_INACTIVE, .x = -1600, .y = 3, .z = -1561, .yaw = 0x400 },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_ITEM_BOX_6, .kind = MAP_SHELTER_KIND_ITEM_BOX_6, .state = MAP_SHELTER_STATE_INACTIVE, .x = 3415, .y = 0, .z = -3245, .yaw = 0xC58 },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_9MM_PARABELLUM, .kind = MAP_SHELTER_KIND_9MM_PARABELLUM, .state = MAP_SHELTER_STATE_REPLENISHABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_9MM_HYDRA, .kind = MAP_SHELTER_KIND_9MM_HYDRA, .state = MAP_SHELTER_STATE_REPLENISHABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_BUCKSHOT, .kind = MAP_SHELTER_KIND_BUCKSHOT, .state = MAP_SHELTER_STATE_REPLENISHABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_GRENADE, .kind = MAP_SHELTER_KIND_GRENADE, .state = MAP_SHELTER_STATE_REPLENISHABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_556_RIFLE_ROUND, .kind = MAP_SHELTER_KIND_556_RIFLE_ROUND, .state = MAP_SHELTER_STATE_REPLENISHABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B1_UNDERGROUND_PARKING_BELT_POUCH, .kind = MAP_SHELTER_KIND_BELT_POUCH, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// Empty place list for Shelter B1 Golem Freezer 1.
///
/// Contains only `AREA_OBJECT_PLACE_END`; seeds no flags and spawns no objects.
static AreaObjectPlace _gMapShelterB1GolemFreezer1ObjectPlaces[1] = {
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 1 object-state placement for Shelter B2 Elevator.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
/// Prompt-marked entries collect immediately when their pickup panel opens.
static AreaObjectPlace _gMapShelterB2ElevatorObjectPlaces[2] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B2_ELEVATOR_YOSHIDAS_CARD, .kind = MAP_SHELTER_KIND_YOSHIDAS_CARD, .state = MAP_SHELTER_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 2 object-state placements for Shelter B2 Operating Room.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
static AreaObjectPlace _gMapShelterB2OperatingRoomObjectPlaces[3] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B2_OPERATING_ROOM_RINGERS_SOLUTION, .kind = MAP_SHELTER_KIND_RINGERS_SOLUTION, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B2_OPERATING_ROOM_EAU_DE_TOILETTE, .kind = MAP_SHELTER_KIND_EAU_DE_TOILETTE, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 2 object-state placements for Shelter B2 Laboratory.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
static AreaObjectPlace _gMapShelterB2LaboratoryObjectPlaces[3] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B2_LABORATORY_ITEM_BOX, .kind = MAP_SHELTER_KIND_LABORATORY_ITEM_BOX, .state = MAP_SHELTER_STATE_AVAILABLE, .x = 3206, .y = 0, .z = 4631, .yaw = 0x988 },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B2_LABORATORY_SAVE_POINT, .kind = MAP_SHELTER_KIND_LABORATORY_SAVE_POINT, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 4 object-state placements for Shelter B2 Breeding Room.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
static AreaObjectPlace _gMapShelterB2BreedingRoomObjectPlaces[5] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B2_BREEDING_ROOM_MODEL, .kind = MAP_SHELTER_KIND_BREEDING_ROOM_MODEL, .state = MAP_SHELTER_STATE_AVAILABLE, .x = 2100, .y = -850, .z = -2600, .yaw = 0x600 },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B2_BREEDING_ROOM_9MM_PARABELLUM, .kind = MAP_SHELTER_KIND_9MM_PARABELLUM, .state = MAP_SHELTER_STATE_REPLENISHABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B2_BREEDING_ROOM_STIM, .kind = MAP_SHELTER_KIND_STIM, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B2_BREEDING_ROOM_COLA, .kind = MAP_SHELTER_KIND_COLA, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 3 object-state placements for Shelter B3 Dumping Hole.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
/// Stored state bit 8 is retained with no established reader or purpose.
static AreaObjectPlace _gMapShelterB3DumpingHoleObjectPlaces[4] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B3_DUMPING_HOLE_MEDICINE_WHEEL, .kind = MAP_SHELTER_KIND_MEDICINE_WHEEL, .state = MAP_SHELTER_STATE_AVAILABLE | MAP_SHELTER_OBJECT_STATE_UNREAD_BIT_8, .x = 3450, .y = 0, .z = -6080, .yaw = 0x3C0 },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B3_DUMPING_HOLE_9MM_HYDRA, .kind = MAP_SHELTER_KIND_9MM_HYDRA, .state = MAP_SHELTER_STATE_REPLENISHABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B3_DUMPING_HOLE_9MM_PARABELLUM, .kind = MAP_SHELTER_KIND_9MM_PARABELLUM, .state = MAP_SHELTER_STATE_REPLENISHABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 1 object-state placement for Shelter B3 Incinerator Control Room.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
/// The area directory retains a NULL spawn pointer for this list.
static AreaObjectPlace _gMapShelterB3IncineratorControlRoomObjectPlaces[2] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B3_INCINERATOR_CONTROL_ROOM_SAVE_POINT, .kind = MAP_SHELTER_KIND_INCINERATOR_CONTROL_ROOM_SAVE_POINT, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 1 object-state placement for Shelter B4 Reservoir.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
/// The area directory retains a NULL spawn pointer for this list.
static AreaObjectPlace _gMapShelterB4ReservoirObjectPlaces[2] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_B4_RESERVOIR_MP_BOOST_2, .kind = MAP_SHELTER_KIND_MP_BOOST_2, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 3 object-state placements for Shelter R47.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
/// Prompt-marked entries collect immediately when their pickup panel opens.
static AreaObjectPlace _gMapShelterR47ObjectPlaces[4] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_R47_MICRO_DEVICE, .kind = MAP_SHELTER_KIND_MICRO_DEVICE, .state = MAP_SHELTER_STATE_AVAILABLE | AREA_OBJECT_PLACE_PROMPT },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_R47_PIERCE_MEMO, .kind = MAP_SHELTER_KIND_PIERCE_MEMO, .state = MAP_SHELTER_STATE_INACTIVE | AREA_OBJECT_PLACE_PROMPT },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_R47_SAVE_POINT, .kind = MAP_SHELTER_KIND_R47_SAVE_POINT, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// 2 object-state placements for Shelter R48.
///
/// Ended by `AREA_OBJECT_PLACE_END`; seeds saved two-bit flags and supplies
/// pickup identity. A model is created only when the paired spawn table has
/// its kind. XYZ use room-coordinate units; yaw uses 4096 units per turn.
static AreaObjectPlace _gMapShelterR48ObjectPlaces[3] = {
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_R48_FIRST_SLIDER, .kind = MAP_SHELTER_KIND_R48_FIRST_SLIDER, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = MAP_SHELTER_OBJECT_SLOT_R48_SECOND_SLIDER, .kind = MAP_SHELTER_KIND_R48_SECOND_SLIDER, .state = MAP_SHELTER_STATE_AVAILABLE },
    { .flagIndex = AREA_OBJECT_PLACE_END },
};

/// Catalog item IDs awarded by these battle lists, one pack per nonzero slot.
enum {
    MAP_SHELTER_REWARD_RECOVERY_2      = 0x2,
    MAP_SHELTER_REWARD_RECOVERY_3      = 0x3,
    MAP_SHELTER_REWARD_MP_BOOST_1      = 0x6,
    MAP_SHELTER_REWARD_PENICILLIN      = 0x8,
    MAP_SHELTER_REWARD_MD_PLAYER       = 0xe,
    MAP_SHELTER_REWARD_PROTEIN_CAPSULE = 0x3c,
    MAP_SHELTER_REWARD_PYKE            = 0x43,
    MAP_SHELTER_REWARD_JAVELIN         = 0x44,
    MAP_SHELTER_REWARD_M203            = 0x45,
    MAP_SHELTER_REWARD_MONGOOSE        = 0x88,
    MAP_SHELTER_REWARD_9MM_SPARTAN     = 0xa2,
    MAP_SHELTER_REWARD_44_MAGNUM       = 0xa6,
    MAP_SHELTER_REWARD_AIRBURST        = 0xaa,
    MAP_SHELTER_REWARD_RIOT            = 0xab,
    MAP_SHELTER_REWARD_FIREFLY         = 0xad,
    MAP_SHELTER_REWARD_R_SLUG          = 0xae,
};

InventoryBattleReward gMapShelterBattleRewards[47] = {
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_MESA, 1, 0), .items = { MAP_SHELTER_REWARD_RECOVERY_3, MAP_SHELTER_KIND_9MM_HYDRA, MAP_SHELTER_KIND_556_RIFLE_ROUND, MAP_SHELTER_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 4, 0), .items = { MAP_SHELTER_REWARD_RECOVERY_2, MAP_SHELTER_KIND_P229, MAP_SHELTER_KIND_GRENADE, MAP_SHELTER_REWARD_M203 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_TUNNEL, 1, 0), .items = { MAP_SHELTER_REWARD_FIREFLY, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_REWARD_R_SLUG } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, 1, 0), .items = { MAP_SHELTER_REWARD_MP_BOOST_1, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_SECRET_PASSAGE, 1, 0), .items = { MAP_SHELTER_KIND_BELT_POUCH, MAP_SHELTER_REWARD_9MM_SPARTAN, INVENTORY_ITEM_NONE, MAP_SHELTER_KIND_MP_BOOST_2 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, 1, 0), .items = { MAP_SHELTER_KIND_STIM, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 4, 0), .items = { MAP_SHELTER_KIND_556_RIFLE_ROUND, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, 3, 0), .items = { MAP_SHELTER_REWARD_R_SLUG, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_REWARD_FIREFLY } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, 2, 0), .items = { MAP_SHELTER_REWARD_PROTEIN_CAPSULE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 1, 0), .items = { MAP_SHELTER_KIND_MP_BOOST_2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 2, 0), .items = { MAP_SHELTER_REWARD_PENICILLIN, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, 2, 0), .items = { MAP_SHELTER_KIND_556_RIFLE_ROUND, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, 3, 0), .items = { MAP_SHELTER_REWARD_FIREFLY, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, 1, 0), .items = { MAP_SHELTER_REWARD_FIREFLY, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, 2, 0), .items = { MAP_SHELTER_REWARD_RECOVERY_2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 3, 0), .items = { MAP_SHELTER_REWARD_FIREFLY, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, 1, 0), .items = { MAP_SHELTER_KIND_GRENADE, MAP_SHELTER_KIND_556_RIFLE_ROUND, MAP_SHELTER_REWARD_RECOVERY_3, MAP_SHELTER_REWARD_AIRBURST } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, 2, 0), .items = { MAP_SHELTER_REWARD_PROTEIN_CAPSULE, MAP_SHELTER_KIND_556_RIFLE_ROUND, INVENTORY_ITEM_NONE, MAP_SHELTER_REWARD_AIRBURST } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 1, 0), .items = { MAP_SHELTER_REWARD_RECOVERY_3, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_KIND_MP_BOOST_2 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 2, 0), .items = { MAP_SHELTER_REWARD_FIREFLY, MAP_SHELTER_REWARD_9MM_SPARTAN, INVENTORY_ITEM_NONE, MAP_SHELTER_KIND_BELT_POUCH } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 3, 0), .items = { MAP_SHELTER_REWARD_RECOVERY_3, MAP_SHELTER_REWARD_AIRBURST, MAP_SHELTER_KIND_EAU_DE_TOILETTE, MAP_SHELTER_KIND_MP_BOOST_2 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 3, 0), .items = { MAP_SHELTER_KIND_EAU_DE_TOILETTE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 4, 0), .items = { MAP_SHELTER_REWARD_FIREFLY, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 11, 0), .items = { MAP_SHELTER_REWARD_RIOT, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_KIND_556_RIFLE_ROUND } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY, 11, 0), .items = { MAP_SHELTER_REWARD_9MM_SPARTAN, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STOREROOM, 11, 0), .items = { MAP_SHELTER_REWARD_RIOT, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, 11, 0), .items = { MAP_SHELTER_REWARD_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, 11, 0), .items = { MAP_SHELTER_KIND_MP_BOOST_2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_REWARD_PENICILLIN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, 11, 0), .items = { MAP_SHELTER_REWARD_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_REWARD_RIOT } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 11, 0), .items = { MAP_SHELTER_KIND_556_RIFLE_ROUND, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, 11, 0), .items = { MAP_SHELTER_REWARD_PENICILLIN, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_KIND_MP_BOOST_2 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL, 11, 0), .items = { MAP_SHELTER_KIND_556_RIFLE_ROUND, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL, 11, 0), .items = { MAP_SHELTER_REWARD_9MM_SPARTAN, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, 11, 0), .items = { MAP_SHELTER_REWARD_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, 11, 0), .items = { MAP_SHELTER_KIND_BELT_POUCH, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, 11, 0), .items = { MAP_SHELTER_KIND_556_RIFLE_ROUND, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 11, 0), .items = { MAP_SHELTER_KIND_556_RIFLE_ROUND, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, 21, 0), .items = { MAP_SHELTER_REWARD_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_KIND_GRENADE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, 21, 0), .items = { MAP_SHELTER_KIND_556_RIFLE_ROUND, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_REWARD_JAVELIN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY, 21, 0), .items = { MAP_SHELTER_REWARD_9MM_SPARTAN, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_KIND_556_RIFLE_ROUND } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 21, 0), .items = { MAP_SHELTER_REWARD_PROTEIN_CAPSULE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, 21, 0), .items = { MAP_SHELTER_REWARD_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, 21, 0), .items = { MAP_SHELTER_REWARD_9MM_SPARTAN, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 21, 0), .items = { MAP_SHELTER_REWARD_RIOT, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 22, 0), .items = { MAP_SHELTER_KIND_RINGERS_SOLUTION, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48, 1, 0), .items = { MAP_SHELTER_KIND_RINGERS_SOLUTION, MAP_SHELTER_KIND_GRENADE, MAP_SHELTER_REWARD_R_SLUG, MAP_SHELTER_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = INVENTORY_BATTLE_REWARD_LIST_END },
};

InventoryBattleReward gMapShelterHardBattleRewards[13] = {
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_MESA, 7, 0), .items = { MAP_SHELTER_REWARD_PROTEIN_CAPSULE, MAP_SHELTER_REWARD_MD_PLAYER, INVENTORY_ITEM_NONE, MAP_SHELTER_REWARD_MP_BOOST_1 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 4, 0), .items = { MAP_SHELTER_KIND_P229, MAP_SHELTER_REWARD_9MM_SPARTAN, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_SECRET_PASSAGE, 1, 0), .items = { MAP_SHELTER_REWARD_R_SLUG, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, 7, 0), .items = { MAP_SHELTER_REWARD_MONGOOSE, MAP_SHELTER_REWARD_PENICILLIN, INVENTORY_ITEM_NONE, MAP_SHELTER_KIND_EAU_DE_TOILETTE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, 11, 0), .items = { MAP_SHELTER_REWARD_44_MAGNUM, MAP_SHELTER_KIND_STIM, INVENTORY_ITEM_NONE, MAP_SHELTER_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 7, 0), .items = { MAP_SHELTER_REWARD_PYKE, MAP_SHELTER_KIND_556_RIFLE_ROUND, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, 1, 0), .items = { MAP_SHELTER_KIND_GRENADE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 3, 0), .items = { MAP_SHELTER_KIND_GRENADE, MAP_SHELTER_REWARD_RECOVERY_2, MAP_SHELTER_REWARD_MP_BOOST_1, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, 17, 0), .items = { MAP_SHELTER_REWARD_PROTEIN_CAPSULE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, 17, 0), .items = { MAP_SHELTER_REWARD_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_KIND_STIM } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 22, 0), .items = { MAP_SHELTER_REWARD_RECOVERY_2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48, 1, 0), .items = { MAP_SHELTER_REWARD_RECOVERY_3, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_SHELTER_KIND_MP_BOOST_2 } },
    { .areaLayoutKey = INVENTORY_BATTLE_REWARD_LIST_END },
};

StageMusicEntry gMapShelterAreaMusicEntries[(GAME_AREA_SHELTER_R49 + 1) * MAP_SHELTER_MUSIC_COLUMN_COUNT] = {
    // Area 0.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_MINE_MESA.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3C, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_MINE_CAVERN.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3E, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_MINE_TUNNEL_ENTRANCE.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_MINE_TUNNEL.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_MINE_GORGE.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_MINE_REFUGE.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_MINE_FORKED_TUNNEL.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_MINE_SECRET_PASSAGE.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_ELEVATOR_HALL.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_STOREROOM.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_ARMORY.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_MAIN_CORRIDOR.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_STERILIZATION_ROOM.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x43, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_DEFERRED },
    // Area GAME_AREA_SHELTER_B1_CONTROL_ROOM.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_ACCESS_TUNNEL.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B2_POD_BOTTOM.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5F, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5A, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B2_ELEVATOR.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B2_ELEVATOR_HALL.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B2_OPERATING_ROOM.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B2_LABORATORY.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B2_BREEDING_ROOM.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B2_MAIN_CORRIDOR.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B2_SEPTIC_TANK.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x41, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x47, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x26, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_R36.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x60, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x61, .startMode = STAGE_MUSIC_START_DEFERRED },
    // Area GAME_AREA_SHELTER_R37.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_1F_HELIPORT_S4.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B3_DUMPING_HOLE.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x42, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4A, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x50, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B3_ELEVATOR_HALL.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B4_LOWER_SEWER.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B4_UPPER_SEWER.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B4_RESERVOIR.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x48, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_B4_WATER_SUPPLY.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_R47.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3B, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x3B, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_R48.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5E, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_SHELTER_R49.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x51, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
};

StageMusicEntry gMapShelterCountdownMusicEntries[20] = {
    { .sequenceId = 0x44, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0x30, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0x3C, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0x42, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0x4A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0x45, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0x46, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
};
