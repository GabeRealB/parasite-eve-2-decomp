#include "mapui/map_dryfield_full.h"

#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/battle_reward.h"
#include "gameplay/direction.h"
#include "gameplay/item_menu.h"
#include "gameplay/item_placement.h"
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
#include "main/session.h"
#include "main/task_types.h"

#include "mappic/mappic.h"

#include "rooms/dryfield_night_back_street.h"

#include "rooms/dryfield_night_breezeway.h"

#include "rooms/dryfield_night_cellar.h"

#include "rooms/dryfield_night_dilapidated_house.h"

#include "rooms/dryfield_night_driveway.h"

#include "rooms/dryfield_night_factory.h"

#include "rooms/dryfield_night_g_r_kitchen.h"

#include "rooms/dryfield_night_garage.h"

#include "rooms/dryfield_night_gas_station.h"

#include "rooms/dryfield_night_general_store.h"

#include "rooms/dryfield_night_junk_yard.h"

#include "rooms/dryfield_night_main_street.h"

#include "rooms/dryfield_night_motel_balcony.h"

#include "rooms/dryfield_night_motel_lobby.h"

#include "rooms/dryfield_night_motel_loft.h"

#include "rooms/dryfield_night_motel_room_1.h"

#include "rooms/dryfield_night_motel_room_2.h"

#include "rooms/dryfield_night_motel_room_3.h"

#include "rooms/dryfield_night_motel_room_4.h"

#include "rooms/dryfield_night_motel_room_5.h"

#include "rooms/dryfield_night_motel_room_6.h"

#include "rooms/dryfield_night_parking_lot.h"

#include "rooms/dryfield_night_r08.h"

#include "rooms/dryfield_night_saloon_g_r.h"

#include "rooms/dryfield_night_souvenir_shop.h"

#include "rooms/dryfield_night_toilet.h"

#include "rooms/dryfield_night_trailer_coach.h"

#include "rooms/dryfield_night_underpass.h"

#include "rooms/dryfield_night_warehouse.h"

#include "rooms/dryfield_night_water_hole.h"

#include "rooms/dryfield_night_water_tank.h"

#include "rooms/dryfield_night_water_tower.h"

/* The Dryfield-at-night stage's map UI overlay: a hook the stage's rooms call
 * at this map's slot address, and the per-stage tables gameplay and main index
 * by stage, most of which point into the stage's room packages or at the map
 * pictures' marker models.
 */

/// Flag-bank kind and task execution priority of this map's model-backed objects.
enum {
    MAP_DRYFIELD_FULL_OBJECT_KIND_M950 = 0x82,
    MAP_DRYFIELD_FULL_OBJECT_PRIORITY  = 98,
};

/// Placement keys matched against each room variant's resource list, not file numbers.
enum {
    MAP_DRYFIELD_FULL_RESOURCE_DESERT_CHASER         = 1,
    MAP_DRYFIELD_FULL_RESOURCE_BRAIN_STINGER         = 3,
    MAP_DRYFIELD_FULL_RESOURCE_GRAY_STALKER          = 5,
    MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER         = 6,
    MAP_DRYFIELD_FULL_RESOURCE_RAT                   = 7,
    MAP_DRYFIELD_FULL_RESOURCE_MOTH                  = 8,
    MAP_DRYFIELD_FULL_RESOURCE_MOSSBACK              = 11,
    MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER          = 15,
    MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER             = 16,
    MAP_DRYFIELD_FULL_RESOURCE_ODD_STRANGER          = 18,
    MAP_DRYFIELD_FULL_RESOURCE_PAWN_GOLEM            = 20,
    MAP_DRYFIELD_FULL_RESOURCE_KNIGHT_GOLEM          = 22,
    MAP_DRYFIELD_FULL_RESOURCE_ROOK_BEAM_SWORD       = 23,
    MAP_DRYFIELD_FULL_RESOURCE_SCORPION              = 25,
    MAP_DRYFIELD_FULL_RESOURCE_BAT                   = 37,
    MAP_DRYFIELD_FULL_RESOURCE_BISHOP_GOLEM          = 39,
    MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER         = 40,
    MAP_DRYFIELD_FULL_RESOURCE_ROOK_GRENADE_LAUNCHER = 57,
    MAP_DRYFIELD_FULL_RESOURCE_GARY_DOUGLAS          = 106,
    MAP_DRYFIELD_FULL_RESOURCE_AYA_WALKER            = 132,
    MAP_DRYFIELD_FULL_RESOURCE_KYLE_MADIGAN          = 140,
};

/// Actor-specific placement modes used by these night-stage layouts.
enum {
    MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE                     = 0,
    MAP_DRYFIELD_FULL_SCAVENGER_WAVE_WAIT_IMMEDIATE        = 1,
    MAP_DRYFIELD_FULL_SCAVENGER_WAVE_WAIT_RANDOM_DELAY     = 2,
    MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED                = 0x80,
    MAP_DRYFIELD_FULL_SCAVENGER_SCRIPTED                   = 3,
    MAP_DRYFIELD_FULL_DESERT_CHASER_TUNING_ROW_2           = 1,
    MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH    = 2,
    MAP_DRYFIELD_FULL_SCORPION_ROAM                        = 0,
    MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER             = 2,
    MAP_DRYFIELD_FULL_ROOK_PATROL                          = 1,
    MAP_DRYFIELD_FULL_BAT_PERCH_BACK                       = 2,
    MAP_DRYFIELD_FULL_BAT_WANDER                           = 0,
    MAP_DRYFIELD_FULL_BAT_PERCH_DROP                       = 1,
    MAP_DRYFIELD_FULL_MOSSBACK_MOVEMENT_AWARENESS          = 1,
    MAP_DRYFIELD_FULL_ZEBRA_STALKER_WATER_ENTRANCE         = 0x10,
    MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING             = 1,
    MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH = 3,
    MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE                   = 1,
    MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL                 = 0,
};

/// Additional placement resource keys, distinct from the selected actor file numbers.
enum {
    MAP_DRYFIELD_FULL_RESOURCE_BURNER            = 31,
    MAP_DRYFIELD_FULL_RESOURCE_MAD_CHASER        = 44,
    MAP_DRYFIELD_FULL_RESOURCE_PAWN_GOLEM_105600 = 56,
    MAP_DRYFIELD_FULL_RESOURCE_FLINT             = 114,
    MAP_DRYFIELD_FULL_RESOURCE_HANDOVER_FIGURE   = 143,
};

/// Spawn selectors and parameter-table rows used by the night layouts below.
enum {
    MAP_DRYFIELD_FULL_MAD_CHASER_DANGLE             = 1,
    MAP_DRYFIELD_FULL_MIND_SUCKLER_SCRIPTED         = 1,
    MAP_DRYFIELD_FULL_ODD_STRANGER_STALK            = 0x10,
    MAP_DRYFIELD_FULL_PAWN_PATROL                   = 1,
    MAP_DRYFIELD_FULL_PAWN_PATROL_LENGTH_THOUSANDS  = 4,
    MAP_DRYFIELD_FULL_ZEBRA_STALKER_FIRST_DROP      = 0x20,
    MAP_DRYFIELD_FULL_ZEBRA_STALKER_JUNK_YARD_ENTRY = 0x30,
    MAP_DRYFIELD_FULL_ZEBRA_STALKER_SECOND_DROP     = 0x40,
    MAP_DRYFIELD_FULL_ZEBRA_STALKER_GROUP_WAIT      = 0x50,
    MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2            = 2,
    MAP_DRYFIELD_FULL_SCORPION_TUNING_ROW_3         = 3,
    MAP_DRYFIELD_FULL_BAT_TURN_RATE_28_ROW          = 3,
    MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW          = 4,
    MAP_DRYFIELD_FULL_BAT_TURN_RATE_44_ROW          = 5,
};

/// Map-page gating, fixed icon kind and room-task priority used by these tables.
enum {
    MAP_DRYFIELD_FULL_PAGE_UNLOCK_FLAG_ID   = 0x16,
    MAP_DRYFIELD_FULL_ICON_FIXED_PICTURE    = 0,
    MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY    = 32,
    MAP_DRYFIELD_FULL_OBJECTIVE_GAS_STATION = 0x11,
    MAP_DRYFIELD_FULL_OBJECTIVE_MOTEL_LOBBY = 0x13,
};

/// Nibble IDs shared by the two endpoints of each night-stage map-marked passage.
enum {
    MAP_DRYFIELD_FULL_MARKER_GAS_STATION_GENERAL_STORE     = 0x1E9,
    MAP_DRYFIELD_FULL_MARKER_GAS_STATION_MAIN_STREET       = 0x1E8,
    MAP_DRYFIELD_FULL_MARKER_MAIN_STREET_MOTEL_ROOM_1      = 0x1E7,
    MAP_DRYFIELD_FULL_MARKER_MAIN_STREET_PARKING_LOT       = 0x1E6,
    MAP_DRYFIELD_FULL_MARKER_MAIN_STREET_MOTEL_ROOM_2      = 0x1E5,
    MAP_DRYFIELD_FULL_MARKER_MAIN_STREET_MOTEL_ROOM_3      = 0x1E4,
    MAP_DRYFIELD_FULL_MARKER_MAIN_STREET_MOTEL_ROOM_4      = 0x1E3,
    MAP_DRYFIELD_FULL_MARKER_MAIN_STREET_DRIVEWAY          = 0x1E2,
    MAP_DRYFIELD_FULL_MARKER_PARKING_LOT_MOTEL_LOBBY       = 0x1E1,
    MAP_DRYFIELD_FULL_MARKER_PARKING_LOT_TOILET            = 0x1E0,
    MAP_DRYFIELD_FULL_MARKER_PARKING_LOT_SALOON            = 0x1DF,
    MAP_DRYFIELD_FULL_MARKER_SALOON_KITCHEN                = 0x1DE,
    MAP_DRYFIELD_FULL_MARKER_KITCHEN_WATER_TOWER           = 0x1DD,
    MAP_DRYFIELD_FULL_MARKER_WATER_TOWER_BREEZEWAY         = 0x1DC,
    MAP_DRYFIELD_FULL_MARKER_BREEZEWAY_FACTORY             = 0x1DB,
    MAP_DRYFIELD_FULL_MARKER_FACTORY_DRIVEWAY              = 0x1DA,
    MAP_DRYFIELD_FULL_MARKER_FACTORY_GARAGE                = 0x1D9,
    MAP_DRYFIELD_FULL_MARKER_GARAGE_JUNK_YARD              = 0x1D8,
    MAP_DRYFIELD_FULL_MARKER_JUNK_YARD_TRAILER_COACH       = 0x1D7,
    MAP_DRYFIELD_FULL_MARKER_GENERAL_STORE_BACK_STREET     = 0x1D6,
    MAP_DRYFIELD_FULL_MARKER_BACK_STREET_SOUVENIR_SHOP     = 0x1D5,
    MAP_DRYFIELD_FULL_MARKER_BACK_STREET_WAREHOUSE         = 0x1D4,
    MAP_DRYFIELD_FULL_MARKER_BACK_STREET_DILAPIDATED_HOUSE = 0x1D3,
    MAP_DRYFIELD_FULL_MARKER_MOTEL_BALCONY_LOFT            = 0x1D2,
    MAP_DRYFIELD_FULL_MARKER_MOTEL_BALCONY_ROOM_6          = 0x1D1,
    MAP_DRYFIELD_FULL_MARKER_MOTEL_BALCONY_ROOM_5          = 0x1D0,
    MAP_DRYFIELD_FULL_MARKER_CELLAR_UNDERPASS              = 0x1CF,
    MAP_DRYFIELD_FULL_MARKER_WATER_HOLE_UNDERPASS          = 0x1CE,
};

/// Catalog item IDs awarded by the night-stage battle lists; zero uses INVENTORY_ITEM_NONE.
enum {
    MAP_DRYFIELD_FULL_REWARD_RECOVERY_2       = 0x02,
    MAP_DRYFIELD_FULL_REWARD_RECOVERY_3       = 0x03,
    MAP_DRYFIELD_FULL_REWARD_MP_BOOST_1       = 0x06,
    MAP_DRYFIELD_FULL_REWARD_MP_BOOST_2       = 0x07,
    MAP_DRYFIELD_FULL_REWARD_LIPSTICK         = 0x0B,
    MAP_DRYFIELD_FULL_REWARD_BELT_POUCH       = 0x0D,
    MAP_DRYFIELD_FULL_REWARD_FLARE            = 0x3A,
    MAP_DRYFIELD_FULL_REWARD_PROTEIN_CAPSULE  = 0x3C,
    MAP_DRYFIELD_FULL_REWARD_RINGERS_SOLUTION = 0x3D,
    MAP_DRYFIELD_FULL_REWARD_EAU_DE_TOILETTE  = 0x3E,
    MAP_DRYFIELD_FULL_REWARD_HUNTER_GOGGLES   = 0x3F,
    MAP_DRYFIELD_FULL_REWARD_COMBAT_LIGHT     = 0x41,
    MAP_DRYFIELD_FULL_REWARD_PYKE             = 0x43,
    MAP_DRYFIELD_FULL_REWARD_9MM_HYDRA        = 0xA1,
    MAP_DRYFIELD_FULL_REWARD_9MM_SPARTAN      = 0xA2,
    MAP_DRYFIELD_FULL_REWARD_GRENADE          = 0xA9,
    MAP_DRYFIELD_FULL_REWARD_AIRBURST         = 0xAA,
    MAP_DRYFIELD_FULL_REWARD_RIOT             = 0xAB,
    MAP_DRYFIELD_FULL_REWARD_FIREFLY          = 0xAD,
    MAP_DRYFIELD_FULL_REWARD_RIFLED_SLUG      = 0xAE,
};

s32 mapDryfieldFullResolveRoomVariant(const RoomEventMsg* request, RoomEventMsg* reply)
{
    if ((request->areaId == GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
        reply->roomState = gameFlagGetNibble(GAME_FLAG_NIGHT_JUNK_YARD_EVENT_SEEN) + 1;
    }
    return 1;
}

GfxImageSlot gMapDryfieldFullImageSlots[MAP_DRYFIELD_FULL_AREA_ID_LIMIT + 1] = {
    [0]                                          = GRAPHICS_IMAGE_SLOT(0x3A8E0),
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION]       = GRAPHICS_IMAGE_SLOT(0x41C20),
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET]       = GRAPHICS_IMAGE_SLOT(0x4A180),
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE]     = GRAPHICS_IMAGE_SLOT(0x4DB70),
    [4]                                          = { NULL, 0 },
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET]       = GRAPHICS_IMAGE_SLOT(0x54AE0),
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP]     = GRAPHICS_IMAGE_SLOT(0x56830),
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE]         = GRAPHICS_IMAGE_SLOT(0x56250),
    [GAME_AREA_DRYFIELD_NIGHT_R08]               = GRAPHICS_IMAGE_SLOT(0x54DA0),
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE] = GRAPHICS_IMAGE_SLOT(0x4A0B0),
    [10]                                         = { NULL, 0 },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1]      = GRAPHICS_IMAGE_SLOT(0x553D0),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2]      = GRAPHICS_IMAGE_SLOT(0x55240),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3]      = GRAPHICS_IMAGE_SLOT(0x54A60),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4]      = GRAPHICS_IMAGE_SLOT(0x55300),
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT]       = GRAPHICS_IMAGE_SLOT(0x53FA0),
    [GAME_AREA_DRYFIELD_NIGHT_TOILET]            = GRAPHICS_IMAGE_SLOT(0x566C0),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY]       = GRAPHICS_IMAGE_SLOT(0x4CB50),
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R]        = GRAPHICS_IMAGE_SLOT(0x4A7F0),
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN]       = GRAPHICS_IMAGE_SLOT(0x57960),
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER]       = GRAPHICS_IMAGE_SLOT(0x52820),
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK]        = GRAPHICS_IMAGE_SLOT(0x54720),
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY]         = GRAPHICS_IMAGE_SLOT(0x554B0),
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY]           = GRAPHICS_IMAGE_SLOT(0x495E0),
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE]            = GRAPHICS_IMAGE_SLOT(0x4B2D0),
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY]          = GRAPHICS_IMAGE_SLOT(0x53740),
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD]         = GRAPHICS_IMAGE_SLOT(0x50350),
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH]     = GRAPHICS_IMAGE_SLOT(0x44320),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5]      = GRAPHICS_IMAGE_SLOT(0x53F20),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY]     = GRAPHICS_IMAGE_SLOT(0x3F550),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6]      = GRAPHICS_IMAGE_SLOT(0x4A170),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT]        = GRAPHICS_IMAGE_SLOT(0x53970),
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE]        = GRAPHICS_IMAGE_SLOT(0x52180),
    [33]                                         = { NULL, 0 },
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR]            = GRAPHICS_IMAGE_SLOT(0x54570),
    [35]                                         = { NULL, 0 },
    [36]                                         = { NULL, 0 },
    [37]                                         = { NULL, 0 },
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS]         = GRAPHICS_IMAGE_SLOT(0x55610),
};

u8 gMapDryfieldFullPageFlagIds[MAP_DRYFIELD_FULL_PAGE_COUNT + 1] = {
    0,
    MAP_DRYFIELD_FULL_PAGE_UNLOCK_FLAG_ID,
    MAP_DRYFIELD_FULL_PAGE_UNLOCK_FLAG_ID,
    MAP_DRYFIELD_FULL_PAGE_UNLOCK_FLAG_ID,
};

MenuMapArea gMapDryfieldFullAreas[MAP_DRYFIELD_FULL_AREA_ID_LIMIT + 2] = {
    [0]                                          = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION]       = { 0x126, -0xD9A, 4, 0x2F, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET]       = { -0x1C2, -0xD6B, 0xFFFD, 0x2E, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE]     = { 0xC1C, 0x12C, 0xC, 0x23, 0x15E, 0x122, 2 },
    [4]                                          = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET]       = { -0x257D, 0x123B, 0x27, 0x12, 0x113, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP]     = { 0x7D0, -0xD6D, 0x2B, 0xA, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE]         = { 0xA27, -0xDCF, 0x46, 0xA, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_R08]               = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE] = { 0x63C, -0x9EC, 0x6A, 0xB, 0x12C, 0x12C, 2 },
    [10]                                         = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1]      = { 0x10D3, 0xD3C, 0xFFE2, 0x2D, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2]      = { 0x1194, 0xA32, 0xFFE2, 0x10, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3]      = { 0x116E, 0x42B, 0xFFE2, 0xFFFF, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4]      = { 0xD48, 0x2A3, 0xFFF2, 0xFFF8, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT]       = { 0x2995, 0xC05, 0xFFE2, 0x1D, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_TOILET]            = { -0x67C, 0x675, 0xFFBA, 0x2C, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY]       = { 0xB7C, 0x13C, 0xFFC4, 0x14, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R]        = { 0x952, -0x1235, 0xFFA1, 0x23, 0x12C, 0x136, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN]       = { -0x4B0, 0x881, 0xFFAF, 5, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER]       = { -0x985, -0x161E, 0xFFB2, 0xFFFE, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK]        = { -0x917, 0x477, 0xFFDC, 0xFFDF, 0x12C, 0x12C, 1 },
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY]         = { 0x1A00, 0x620, 0xFFD6, 0xFFE6, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY]           = { 0x143A, 0x5AE, 0x10, 0xFFFD, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE]            = { 0x355, 0xAB5, 0x1A, 0xFFE9, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY]          = { -0xD48, 0x258, 0x18, 0xFFFE, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD]         = { 0x13A6, 0x186, 0x1E, 0xFFD5, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH]     = { 0x62B, -0x320, 0x50, 0xFFD8, 0x190, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5]      = { 0x10FE, 0x97A, 0xA, 0x1D, 0x12C, 0x12C, 1 },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY]     = { 0x315E, 0x5C6, 0x4D, 0x11, 0x12C, 0x12C, 1 },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6]      = { 0x10FE, 0x5DC, 8, 6, 0x12C, 0x12C, 1 },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT]        = { 0x1082, -0x764, 0x1F, 0xFFEA, 0x12C, 0x12C, 1 },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE]        = { 0x1CEE, -0x5AA, 0xFFE4, 0xFFE9, 0x12C, 0x12C, 3 },
    [33]                                         = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR]            = { 0x15EB, 0xF69, 0xFFF8, 0xC, 0x12C, 0x12C, 3 },
    [35]                                         = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [36]                                         = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [37]                                         = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS]         = { 0x14B7, 0x2C94, 0xFFF9, 0xFFB6, 0x12C, 0x10E, 3 },
    [MAP_DRYFIELD_FULL_AREA_ID_LIMIT + 1]        = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
};

/// Picture-page IDs selected by this stage's area shapes.
enum {
    MAP_DRYFIELD_FULL_SHAPE_PAGE_MOTEL_WATER_TANK = 1,
    MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN             = 2,
    MAP_DRYFIELD_FULL_SHAPE_PAGE_UNDERGROUND      = 3,
};

MenuMapAreaShape gMapDryfieldFullAreaShapes[MAP_DRYFIELD_FULL_AREA_ID_LIMIT + 1] = {
    [0]                                          = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION]       = { .model = &gMappicS302GasStationShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET]       = { .model = &gMappicS302MainStreetShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE]     = { .model = &gMappicS302GeneralStoreShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [4]                                          = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET]       = { .model = &gMappicS302BackStreetShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP]     = { .model = &gMappicS302SouvenirShopShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE]         = { .model = &gMappicS302WarehouseShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_R08]               = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE] = { .model = &gMappicS302DilapidatedHouseShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [10]                                         = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1]      = { .model = &gMappicS302MotelRoom1Shape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2]      = { .model = &gMappicS302MotelRoom2Shape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3]      = { .model = &gMappicS302MotelRoom3Shape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4]      = { .model = &gMappicS302MotelRoom4Shape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT]       = { .model = &gMappicS302ParkingLotShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_TOILET]            = { .model = &gMappicS302ToiletShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY]       = { .model = &gMappicS302MotelLobbyShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R]        = { .model = &gMappicS302SaloonGRShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN]       = { .model = &gMappicS302GRKitchenShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER]       = { .model = &gMappicS302WaterTowerShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK]        = { .model = &gMappicS300WaterTankShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_MOTEL_WATER_TANK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY]         = { .model = &gMappicS302BreezewayShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY]           = { .model = &gMappicS302FactoryShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE]            = { .model = &gMappicS302GarageShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY]          = { .model = &gMappicS302DrivewayShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD]         = { .model = &gMappicS302JunkYardShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH]     = { .model = &gMappicS302TrailerCoachShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5]      = { .model = &gMappicS300MotelRoom5Shape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_MOTEL_WATER_TANK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY]     = { .model = &gMappicS300MotelBalconyShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_MOTEL_WATER_TANK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6]      = { .model = &gMappicS300MotelRoom6Shape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_MOTEL_WATER_TANK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT]        = { .model = &gMappicS300MotelLoftShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_MOTEL_WATER_TANK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE]        = { .model = &gMappicS303WaterHoleShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_UNDERGROUND, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [33]                                         = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR]            = { .model = &gMappicS303CellarShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_UNDERGROUND, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [35]                                         = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [36]                                         = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [37]                                         = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS]         = { .model = &gMappicS303UnderpassShape, .page = MAP_DRYFIELD_FULL_SHAPE_PAGE_UNDERGROUND, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
};

MenuMapMarker gMapDryfieldFullMarkers[MAP_DRYFIELD_FULL_MARKER_COUNT + 1] = {
    { 2, MENU_MAP_MARKER_AREA_ANY, 12, 38 },
    { 2, MENU_MAP_MARKER_AREA_ANY, 1, 47 },
    { 2, MENU_MAP_MARKER_AREA_ANY, -26, 43 },
    { 2, MENU_MAP_MARKER_AREA_ANY, -26, 29 },
    { 2, MENU_MAP_MARKER_AREA_ANY, -26, 14 },
    { 2, MENU_MAP_MARKER_AREA_ANY, -26, 1 },
    { 2, MENU_MAP_MARKER_AREA_ANY, -13, -3 },
    { 2, MENU_MAP_MARKER_AREA_ANY, 1, 6 },
    { 2, MENU_MAP_MARKER_AREA_ANY, -59, 25 },
    { 2, MENU_MAP_MARKER_AREA_ANY, -67, 41 },
    { 2, MENU_MAP_MARKER_AREA_ANY, -91, 40 },
    { 2, MENU_MAP_MARKER_AREA_ANY, -84, 7 },
    { 2, MENU_MAP_MARKER_AREA_ANY, -79, 3 },
    { 2, MENU_MAP_MARKER_AREA_ANY, -43, -25 },
    { 2, MENU_MAP_MARKER_AREA_ANY, 1, -25 },
    { 2, MENU_MAP_MARKER_AREA_ANY, 21, -3 },
    { 2, MENU_MAP_MARKER_AREA_ANY, 21, -25 },
    { 2, MENU_MAP_MARKER_AREA_ANY, 27, -39 },
    { 2, MENU_MAP_MARKER_AREA_ANY, 87, -43 },
    { 2, MENU_MAP_MARKER_AREA_ANY, 35, 18 },
    { 2, MENU_MAP_MARKER_AREA_ANY, 45, 15 },
    { 2, MENU_MAP_MARKER_AREA_ANY, 68, 15 },
    { 2, MENU_MAP_MARKER_AREA_ANY, 105, 15 },
    { 1, MENU_MAP_MARKER_AREA_ANY, 31, -17 },
    { 1, MENU_MAP_MARKER_AREA_ANY, 12, 5 },
    { 1, MENU_MAP_MARKER_AREA_ANY, 12, 31 },
    { 3, MENU_MAP_MARKER_AREA_ANY, 27, -16 },
    { 3, MENU_MAP_MARKER_AREA_ANY, -7, 12 },
    { 3, MENU_MAP_MARKER_AREA_ANY, -39, -23 },
    { 1, MENU_MAP_MARKER_AREA_ANY, 16, 12 },
    { 0, 0, 0, 0 },
};

MenuMapIcon gMapDryfieldFullIcons[9] = {
    { .page = 2, .area = GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, .kind = MAP_DRYFIELD_FULL_ICON_FIXED_PICTURE, .condition = 0, .x = 53, .y = 54 },
    { .page = 2, .area = GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = 0, .x = -51, .y = 20 },
    { .page = 2, .area = GAME_AREA_DRYFIELD_NIGHT_GARAGE, .kind = MAP_DRYFIELD_FULL_ICON_FIXED_PICTURE, .condition = GAME_FLAG_097, .x = 37, .y = -17 },
    { .page = 2, .area = GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = 0, .x = 79, .y = -38 },
    { .page = 1, .area = GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6, .kind = MAP_DRYFIELD_FULL_ICON_FIXED_PICTURE, .condition = 0, .x = 0, .y = 7 },
    { .page = 1, .area = GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = 0, .x = 0, .y = -5 },
    { .page = 2, .area = GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, .kind = MENU_MAP_ICON_KIND_OBJECTIVE, .condition = MAP_DRYFIELD_FULL_OBJECTIVE_GAS_STATION, .x = 31, .y = 46 },
    { .page = 2, .area = GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY, .kind = MENU_MAP_ICON_KIND_OBJECTIVE, .condition = MAP_DRYFIELD_FULL_OBJECTIVE_MOTEL_LOBBY, .x = -60, .y = 13 },
    { .page = 0 },
};

MenuMapAreaName gMapDryfieldFullAreaNames[MAP_DRYFIELD_FULL_AREA_ID_LIMIT] = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION - 1]       = { "Gas station" },
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET - 1]       = { "Main street" },
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE - 1]     = { "General store" },
    [4 - 1]                                          = { "" },
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET - 1]       = { "Back street" },
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP - 1]     = { "Souvenir shop" },
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE - 1]         = { "Warehouse" },
    [GAME_AREA_DRYFIELD_NIGHT_R08 - 1]               = { "" },
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE - 1] = { "Dilapidated house" },
    [10 - 1]                                         = { "" },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1 - 1]      = { "Motel room 1" },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2 - 1]      = { "Motel room 2" },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3 - 1]      = { "Motel room 3" },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4 - 1]      = { "Motel room 4" },
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT - 1]       = { "Parking lot" },
    [GAME_AREA_DRYFIELD_NIGHT_TOILET - 1]            = { "Toilet" },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY - 1]       = { "Motel lobby" },
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R - 1]        = { "Saloon G & R" },
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN - 1]       = { "G & R kitchen" },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER - 1]       = { "Water tower" },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK - 1]        = { "Water tank" },
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY - 1]         = { "Breezeway" },
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY - 1]           = { "Factory" },
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE - 1]            = { "Garage" },
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY - 1]          = { "Driveway" },
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD - 1]         = { "Junk yard" },
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH - 1]     = { "Trailer coach" },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5 - 1]      = { "Motel room 5" },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY - 1]     = { "Motel balcony" },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6 - 1]      = { "Motel room 6" },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT - 1]        = { "Motel loft" },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE - 1]        = { "Water hole" },
    [33 - 1]                                         = { "" },
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR - 1]            = { "Cellar" },
    [35 - 1]                                         = { "" },
    [36 - 1]                                         = { "" },
    [37 - 1]                                         = { "" },
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS - 1]         = { "Underpass" },
};

/// Shared empty spawn list for night-stage areas with no model-backed object recipe.
///
/// The sole record is `AREA_OBJECT_SPAWN_END`; a placed kind never matches it.
static AreaObjectSpawn _gMapDryfieldFullEmptyObjectSpawns[1] = {
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// Empty spawn list selected by the night Gas Station.
///
/// Contains only `AREA_OBJECT_SPAWN_END`; its placed pickups have no model recipe.
static AreaObjectSpawn _gMapDryfieldFullGasStationObjectSpawns[1] = {
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// M950 pickup container-lid recipe for the night Trailer Coach.
///
/// Matches object kind 0x82 and borrows the room model until the task ends.
/// `AREA_OBJECT_SPAWN_END` terminates the list.
static AreaObjectSpawn _gMapDryfieldFullTrailerCoachObjectSpawns[2] = {
    {
        .kind     = MAP_DRYFIELD_FULL_OBJECT_KIND_M950,
        .taskDesc = {
            .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = MAP_DRYFIELD_FULL_OBJECT_PRIORITY } },
            .callback = itemPickupContainerLidTask,
            .data     = { .model = &gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0 },
        },
    },
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// Jerry Can model recipe for the night Motel Loft.
///
/// Matches `INVENTORY_COLLECTION_ID_JERRY_CAN` and borrows the room model
/// until the task ends. `AREA_OBJECT_SPAWN_END` terminates the list.
static AreaObjectSpawn _gMapDryfieldFullMotelLoftObjectSpawns[2] = {
    {
        .kind     = INVENTORY_COLLECTION_ID_JERRY_CAN,
        .taskDesc = {
            .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = MAP_DRYFIELD_FULL_OBJECT_PRIORITY } },
            .callback = areaObjectModelTask,
            .data     = { .model = &gDryfieldNightMotelLoftActor135400Model071AC },
        },
    },
    { .kind = AREA_OBJECT_SPAWN_END },
};

AreaObjectRoom gMapDryfieldFullObjectRooms[MAP_DRYFIELD_FULL_AREA_ID_LIMIT + 2] = {
    [0]                                          = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION]       = { .places = { .list = gAreaDryfieldGasStationObjectPlaces }, .spawns = _gMapDryfieldFullGasStationObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET]       = { .places = { .list = gAreaDryfieldMainStreetObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE]     = { .places = { .list = gAreaDryfieldGeneralStoreObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [4]                                          = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET]       = { .places = { .list = gAreaDryfieldBackStreetObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP]     = { .places = { .list = gAreaDryfieldSouvenirShopObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE]         = { .places = { .list = gAreaDryfieldWarehouseObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_R08]               = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE] = { .places = { .list = NULL }, .spawns = NULL },
    [10]                                         = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1]      = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2]      = { .places = { .list = gAreaDryfieldMotelRoom2ObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3]      = { .places = { .list = gAreaDryfieldMotelRoom3ObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4]      = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT]       = { .places = { .list = gAreaDryfieldParkingLotObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_TOILET]            = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY]       = { .places = { .list = gAreaDryfieldMotelLobbyObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R]        = { .places = { .list = gAreaDryfieldSaloonGRObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN]       = { .places = { .list = gAreaDryfieldGRKitchenObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER]       = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK]        = { .places = { .list = gAreaDryfieldWaterTankObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY]         = { .places = { .list = gAreaDryfieldBreezewayObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY]           = { .places = { .list = gAreaDryfieldFactoryObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE]            = { .places = { .list = gAreaDryfieldGarageObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY]          = { .places = { .list = gAreaDryfieldDrivewayObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD]         = { .places = { .list = gAreaDryfieldJunkYardObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH]     = { .places = { .list = gAreaDryfieldTrailerCoachObjectPlaces }, .spawns = _gMapDryfieldFullTrailerCoachObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5]      = { .places = { .list = gAreaDryfieldMotelRoom5ObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY]     = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6]      = { .places = { .list = gAreaDryfieldMotelRoom6ObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT]        = { .places = { .list = gAreaDryfieldMotelLoftObjectPlaces }, .spawns = _gMapDryfieldFullMotelLoftObjectSpawns },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE]        = { .places = { .list = gAreaDryfieldWaterHoleObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [33]                                         = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR]            = { .places = { .list = gAreaDryfieldCellarObjectPlaces }, .spawns = _gMapDryfieldFullEmptyObjectSpawns },
    [35]                                         = { .places = { .list = NULL }, .spawns = NULL },
    [36]                                         = { .places = { .list = NULL }, .spawns = NULL },
    [37]                                         = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS]         = { .places = { .list = NULL }, .spawns = NULL },
    [MAP_DRYFIELD_FULL_AREA_ID_LIMIT + 1]        = { .places = { .sentinel = AREA_OBJECT_ROOM_END }, .spawns = NULL },
};

TaskDesc gMapDryfieldFullRoomTaskDescriptors[33] = {
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightGasStationRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightMainStreetRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightGeneralStoreRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightBackStreetRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightSouvenirShopRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightWarehouseRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightDilapidatedHouseRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightMotelRoom1RoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightMotelRoom2RoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightMotelRoom3RoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightMotelRoom4RoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightParkingLotRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightToiletRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TOILET, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightMotelLobbyRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightSaloonGRRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightGRKitchenRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightWaterTowerRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightWaterTankRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TANK, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightBreezewayRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightFactoryEntryTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_FACTORY, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightGarageRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GARAGE, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightDrivewayRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightJunkYardRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightTrailerCoachRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightMotelRoom5RoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightMotelBalconyRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightMotelRoom6RoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightMotelLoftRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightWaterHoleRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightCellarRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_CELLAR, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightUnderpassRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 0) } },
    { { { TASK_BODY_NONE, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, dryfieldNightR08RoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_R08, 0) } },
    { { { TASK_DESC_END, MAP_DRYFIELD_FULL_ROOM_TASK_PRIORITY } }, NULL, { 0 } },
};

u16 gMapDryfieldFullMarkerFlags[MAP_DRYFIELD_FULL_MARKER_COUNT] = {
    MAP_DRYFIELD_FULL_MARKER_GAS_STATION_GENERAL_STORE,
    MAP_DRYFIELD_FULL_MARKER_GAS_STATION_MAIN_STREET,
    MAP_DRYFIELD_FULL_MARKER_MAIN_STREET_MOTEL_ROOM_1,
    MAP_DRYFIELD_FULL_MARKER_MAIN_STREET_PARKING_LOT,
    MAP_DRYFIELD_FULL_MARKER_MAIN_STREET_MOTEL_ROOM_2,
    MAP_DRYFIELD_FULL_MARKER_MAIN_STREET_MOTEL_ROOM_3,
    MAP_DRYFIELD_FULL_MARKER_MAIN_STREET_MOTEL_ROOM_4,
    MAP_DRYFIELD_FULL_MARKER_MAIN_STREET_DRIVEWAY | MENU_MAP_MARKER_ALTERNATE_PICTURE,
    MAP_DRYFIELD_FULL_MARKER_PARKING_LOT_MOTEL_LOBBY,
    MAP_DRYFIELD_FULL_MARKER_PARKING_LOT_TOILET,
    MAP_DRYFIELD_FULL_MARKER_PARKING_LOT_SALOON,
    MAP_DRYFIELD_FULL_MARKER_SALOON_KITCHEN,
    MAP_DRYFIELD_FULL_MARKER_KITCHEN_WATER_TOWER,
    MAP_DRYFIELD_FULL_MARKER_WATER_TOWER_BREEZEWAY,
    MAP_DRYFIELD_FULL_MARKER_BREEZEWAY_FACTORY,
    MAP_DRYFIELD_FULL_MARKER_FACTORY_DRIVEWAY,
    MAP_DRYFIELD_FULL_MARKER_FACTORY_GARAGE,
    MAP_DRYFIELD_FULL_MARKER_GARAGE_JUNK_YARD,
    MAP_DRYFIELD_FULL_MARKER_JUNK_YARD_TRAILER_COACH,
    MAP_DRYFIELD_FULL_MARKER_GENERAL_STORE_BACK_STREET,
    MAP_DRYFIELD_FULL_MARKER_BACK_STREET_SOUVENIR_SHOP,
    MAP_DRYFIELD_FULL_MARKER_BACK_STREET_WAREHOUSE,
    MAP_DRYFIELD_FULL_MARKER_BACK_STREET_DILAPIDATED_HOUSE,
    MAP_DRYFIELD_FULL_MARKER_MOTEL_BALCONY_LOFT,
    MAP_DRYFIELD_FULL_MARKER_MOTEL_BALCONY_ROOM_6,
    MAP_DRYFIELD_FULL_MARKER_MOTEL_BALCONY_ROOM_5,
    MAP_DRYFIELD_FULL_MARKER_CELLAR_UNDERPASS,
    MAP_DRYFIELD_FULL_MARKER_WATER_HOLE_UNDERPASS,
    GAME_FLAG_MAP_MARK_WATER,
    0,
};

WorldCoordRoomLighting* gMapDryfieldFullRoomLightingTables[MAP_DRYFIELD_FULL_AREA_ID_LIMIT] = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION - 1]       = gDryfieldNightGasStationRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET - 1]       = gDryfieldNightMainStreetRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE - 1]     = gDryfieldNightGeneralStoreRoomLighting,
    [3]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET - 1]       = gDryfieldNightBackStreetRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP - 1]     = gDryfieldNightSouvenirShopRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE - 1]         = gDryfieldNightWarehouseRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_R08 - 1]               = gDryfieldNightR08RoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE - 1] = gDryfieldNightDilapidatedHouseRoomLighting,
    [9]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1 - 1]      = gDryfieldNightMotelRoom1RoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2 - 1]      = gDryfieldNightMotelRoom2RoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3 - 1]      = gDryfieldNightMotelRoom3RoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4 - 1]      = gDryfieldNightMotelRoom4RoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT - 1]       = gDryfieldNightParkingLotRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_TOILET - 1]            = gDryfieldNightToiletRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY - 1]       = gDryfieldNightMotelLobbyRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R - 1]        = gDryfieldNightSaloonGRRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN - 1]       = gDryfieldNightGRKitchenRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER - 1]       = gDryfieldNightWaterTowerRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK - 1]        = gDryfieldNightWaterTankRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY - 1]         = gDryfieldNightBreezewayRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY - 1]           = gDryfieldNightFactoryRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE - 1]            = gDryfieldNightGarageRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY - 1]          = gDryfieldNightDrivewayRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD - 1]         = gDryfieldNightJunkYardRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH - 1]     = gDryfieldNightTrailerCoachRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5 - 1]      = gDryfieldNightMotelRoom5RoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY - 1]     = gDryfieldNightMotelBalconyRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6 - 1]      = gDryfieldNightMotelRoom6RoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT - 1]        = gDryfieldNightMotelLoftRoomLighting,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE - 1]        = gDryfieldNightWaterHoleRoomLighting,
    [32]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR - 1]            = gDryfieldNightCellarRoomLighting,
    [34]                                             = NULL,
    [35]                                             = NULL,
    [36]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS - 1]         = gDryfieldNightUnderpassRoomLighting,
};

DirectionWarpEntry* gMapDryfieldFullAreaWarpEntries[MAP_DRYFIELD_FULL_AREA_ID_LIMIT] = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION - 1]       = gDryfieldNightGasStationWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET - 1]       = gDryfieldNightMainStreetWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE - 1]     = gDryfieldNightGeneralStoreWarpEntries,
    [3]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET - 1]       = gDryfieldNightBackStreetWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP - 1]     = gDryfieldNightSouvenirShopWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE - 1]         = gDryfieldNightWarehouseWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_R08 - 1]               = gDryfieldNightR08WarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE - 1] = gDryfieldNightDilapidatedHouseWarpEntries,
    [9]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1 - 1]      = gDryfieldNightMotelRoom1Warps,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2 - 1]      = gDryfieldNightMotelRoom2WarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3 - 1]      = gDryfieldNightMotelRoom3WarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4 - 1]      = gDryfieldNightMotelRoom4WarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT - 1]       = gDryfieldNightParkingLotWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_TOILET - 1]            = gDryfieldNightToiletWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY - 1]       = gDryfieldNightMotelLobbyWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R - 1]        = gDryfieldNightSaloonGRWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN - 1]       = gDryfieldNightGRKitchenWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER - 1]       = gDryfieldNightWaterTowerWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK - 1]        = gDryfieldNightWaterTankWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY - 1]         = gDryfieldNightBreezewayWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY - 1]           = gDryfieldNightFactoryWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE - 1]            = gDryfieldNightGarageWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY - 1]          = gDryfieldNightDrivewayWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD - 1]         = gDryfieldNightJunkYardWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH - 1]     = gDryfieldNightTrailerCoachWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5 - 1]      = gDryfieldNightMotelRoom5WarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY - 1]     = gDryfieldNightMotelBalconyWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6 - 1]      = gDryfieldNightMotelRoom6WarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT - 1]        = gDryfieldNightMotelLoftWarpEntries,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE - 1]        = gDryfieldNightWaterHoleWarpEntries,
    [32]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR - 1]            = gDryfieldNightCellarWarpEntries,
    [34]                                             = NULL,
    [35]                                             = NULL,
    [36]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS - 1]         = gDryfieldNightUnderpassWarpEntries,
};

/// Borrowed per-room logical-view counts for Dryfield at night.
///
/// Indexed by area - 1 for areas 1..38; areas 4, 10, 33 and 35..37 are NULL.
/// Populated arrays use room - 1 and their own extents, without a terminator.
/// The selected room overlay owns the signed-halfword element counts and must
/// remain loaded during lookup; counts do not measure mapped resource capacity.
static ViewCount* _gMapDryfieldFullRoomViewCountsByArea[MAP_DRYFIELD_FULL_AREA_ID_LIMIT] = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION - 1]       = gDryfieldNightGasStationViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET - 1]       = gDryfieldNightMainStreetViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE - 1]     = gDryfieldNightGeneralStoreViewCounts,
    [4 - 1]                                          = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET - 1]       = gDryfieldNightBackStreetRoomViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP - 1]     = gDryfieldNightSouvenirShopViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE - 1]         = gDryfieldNightWarehouseRoomViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_R08 - 1]               = gDryfieldNightR08ViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE - 1] = gDryfieldNightDilapidatedHouseViewCounts,
    [10 - 1]                                         = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1 - 1]      = gDryfieldNightMotelRoom1ViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2 - 1]      = gDryfieldNightMotelRoom2RoomViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3 - 1]      = gDryfieldNightMotelRoom3ViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4 - 1]      = gDryfieldNightMotelRoom4ViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT - 1]       = gDryfieldNightParkingLotViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_TOILET - 1]            = gDryfieldNightToiletViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY - 1]       = gDryfieldNightMotelLobbyViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R - 1]        = gDryfieldNightSaloonGRViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN - 1]       = gDryfieldNightGRKitchenViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER - 1]       = gDryfieldNightWaterTowerViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK - 1]        = gDryfieldNightWaterTankViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY - 1]         = gDryfieldNightBreezewayViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY - 1]           = gDryfieldNightFactoryViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE - 1]            = gDryfieldNightGarageViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY - 1]          = gDryfieldNightDrivewayViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD - 1]         = gDryfieldNightJunkYardViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH - 1]     = gDryfieldNightTrailerCoachViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5 - 1]      = gDryfieldNightMotelRoom5RoomViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY - 1]     = gDryfieldNightMotelBalconyRoomViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6 - 1]      = gDryfieldNightMotelRoom6ViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT - 1]        = gDryfieldNightMotelLoftViewCounts,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE - 1]        = gDryfieldNightWaterHoleViewCounts,
    [33 - 1]                                         = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR - 1]            = gDryfieldNightCellarViewCounts,
    [35 - 1]                                         = NULL,
    [36 - 1]                                         = NULL,
    [37 - 1]                                         = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS - 1]         = gDryfieldNightUnderpassViewCounts,
};

ViewCountTable gMapDryfieldFullViewCountTable = { .viewCounts = _gMapDryfieldFullRoomViewCountsByArea };

/// Per-area collision room arrays for Dryfield at night.
///
/// Select area - 1 for IDs 1..38. Areas 4, 10, 33, 35, 36, 37 are NULL.
/// Each array uses room - 1 within its own extent, without a terminator.
/// Descriptors may be map-owned or borrowed; their mutable room resources must
/// remain loaded until the active grid and linked lists are retired.
static WorldCollisionRoomResources* _gMapDryfieldFullWorldCollisionAreaRooms[MAP_DRYFIELD_FULL_AREA_ID_LIMIT] = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION - 1]       = gDryfieldNightGasStationRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET - 1]       = gDryfieldNightMainStreetRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE - 1]     = gDryfieldNightGeneralStoreRoomCollisionResources,
    [3]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET - 1]       = gDryfieldNightBackStreetRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP - 1]     = gDryfieldNightSouvenirShopRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE - 1]         = gDryfieldNightWarehouseRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_R08 - 1]               = gDryfieldNightR08RoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE - 1] = gDryfieldNightDilapidatedHouseRoomCollisionResources,
    [9]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1 - 1]      = gDryfieldNightMotelRoom1RoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2 - 1]      = gDryfieldNightMotelRoom2RoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3 - 1]      = gDryfieldNightMotelRoom3RoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4 - 1]      = gDryfieldNightMotelRoom4RoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT - 1]       = gDryfieldNightParkingLotRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_TOILET - 1]            = gDryfieldNightToiletRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY - 1]       = gDryfieldNightMotelLobbyRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R - 1]        = gDryfieldNightSaloonGRRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN - 1]       = gDryfieldNightGRKitchenRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER - 1]       = gDryfieldNightWaterTowerCollisionRooms,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK - 1]        = gDryfieldNightWaterTankCollisionRooms,
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY - 1]         = gDryfieldNightBreezewayRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY - 1]           = gDryfieldNightFactoryRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE - 1]            = gDryfieldNightGarageRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY - 1]          = gDryfieldNightDrivewayRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD - 1]         = gDryfieldNightJunkYardRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH - 1]     = gDryfieldNightTrailerCoachRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5 - 1]      = gDryfieldNightMotelRoom5RoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY - 1]     = gDryfieldNightMotelBalconyRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6 - 1]      = gDryfieldNightMotelRoom6RoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT - 1]        = gDryfieldNightMotelLoftRoomCollisionResources,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE - 1]        = gDryfieldNightWaterHoleRoomCollisionResources,
    [32]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR - 1]            = gDryfieldNightCellarRoomCollisionResources,
    [34]                                             = NULL,
    [35]                                             = NULL,
    [36]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS - 1]         = gDryfieldNightUnderpassRoomCollisionResources,
};

WorldCollisionStageResources gMapDryfieldFullWorldCollisionResources = { .areaRooms = _gMapDryfieldFullWorldCollisionAreaRooms };

/// Night-stage area directory of borrowed room camera arrays.
///
/// Slots use area ID minus one (0..37), with NULL for 4, 10, 33 and 35..37.
/// A pool uses mapped camera index minus one and has its own extent; neither
/// level has a terminator. Keep the selected room overlay loaded until any
/// queued camera application has finished borrowing its record.
static ViewCamera* _gMapDryfieldFullViewCamerasByArea[MAP_DRYFIELD_FULL_AREA_ID_LIMIT] = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION - 1]       = gDryfieldNightGasStationViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET - 1]       = gDryfieldNightMainStreetViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE - 1]     = gDryfieldNightGeneralStoreViewCameras,
    [3]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET - 1]       = gDryfieldNightBackStreetViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP - 1]     = gDryfieldNightSouvenirShopViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE - 1]         = gDryfieldNightWarehouseViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_R08 - 1]               = gDryfieldNightR08ViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE - 1] = gDryfieldNightDilapidatedHouseViewCameras,
    [9]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1 - 1]      = gDryfieldNightMotelRoom1ViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2 - 1]      = gDryfieldNightMotelRoom2ViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3 - 1]      = gDryfieldNightMotelRoom3ViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4 - 1]      = gDryfieldNightMotelRoom4ViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT - 1]       = gDryfieldNightParkingLotViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_TOILET - 1]            = gDryfieldNightToiletViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY - 1]       = gDryfieldNightMotelLobbyViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R - 1]        = gDryfieldNightSaloonGRViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN - 1]       = gDryfieldNightGRKitchenViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER - 1]       = gDryfieldNightWaterTowerViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK - 1]        = gDryfieldNightWaterTankViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY - 1]         = gDryfieldNightBreezewayViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY - 1]           = gDryfieldNightFactoryViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE - 1]            = gDryfieldNightGarageViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY - 1]          = gDryfieldNightDrivewayViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD - 1]         = gDryfieldNightJunkYardViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH - 1]     = gDryfieldNightTrailerCoachViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5 - 1]      = gDryfieldNightMotelRoom5ViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY - 1]     = gDryfieldNightMotelBalconyViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6 - 1]      = gDryfieldNightMotelRoom6ViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT - 1]        = gDryfieldNightMotelLoftViewCameras,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE - 1]        = gDryfieldNightWaterHoleViewCameras,
    [32]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR - 1]            = gDryfieldNightCellarViewCameras,
    [34]                                             = NULL,
    [35]                                             = NULL,
    [36]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS - 1]         = gDryfieldNightUnderpassViewCameras,
};

ViewCameraTable gMapDryfieldFullViewCameraTable = { .cameras = _gMapDryfieldFullViewCamerasByArea };

/// Night-stage area directory of borrowed per-room logical-view byte maps.
///
/// Slots use area ID minus one (0..37), with NULL for missing areas. Each
/// room pointer and map stays valid only while its room overlay is loaded.
static u8** _gMapDryfieldFullAreaViewMaps[MAP_DRYFIELD_FULL_AREA_ID_LIMIT] = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION - 1]       = gDryfieldNightGasStationViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET - 1]       = gDryfieldNightMainStreetViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE - 1]     = gDryfieldNightGeneralStoreViewMaps,
    [3]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET - 1]       = gDryfieldNightBackStreetViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP - 1]     = gDryfieldNightSouvenirShopViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE - 1]         = gDryfieldNightWarehouseViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_R08 - 1]               = gDryfieldNightR08ViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE - 1] = gDryfieldNightDilapidatedHouseViewMaps,
    [9]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1 - 1]      = gDryfieldNightMotelRoom1ViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2 - 1]      = gDryfieldNightMotelRoom2RoomViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3 - 1]      = gDryfieldNightMotelRoom3ViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4 - 1]      = gDryfieldNightMotelRoom4ViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT - 1]       = gDryfieldNightParkingLotViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_TOILET - 1]            = gDryfieldNightToiletViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY - 1]       = gDryfieldNightMotelLobbyViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R - 1]        = gDryfieldNightSaloonGRViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN - 1]       = gDryfieldNightGRKitchenViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER - 1]       = gDryfieldNightWaterTowerViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK - 1]        = gDryfieldNightWaterTankViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY - 1]         = gDryfieldNightBreezewayViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY - 1]           = gDryfieldNightFactoryViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE - 1]            = gDryfieldNightGarageViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY - 1]          = gDryfieldNightDrivewayViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD - 1]         = gDryfieldNightJunkYardViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH - 1]     = gDryfieldNightTrailerCoachViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5 - 1]      = gDryfieldNightMotelRoom5RoomViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY - 1]     = gDryfieldNightMotelBalconyViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6 - 1]      = gDryfieldNightMotelRoom6ViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT - 1]        = gDryfieldNightMotelLoftViewMaps,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE - 1]        = gDryfieldNightWaterHoleViewMaps,
    [32]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR - 1]            = gDryfieldNightCellarViewMaps,
    [34]                                             = NULL,
    [35]                                             = NULL,
    [36]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS - 1]         = gDryfieldNightUnderpassViewMaps,
};

ViewIndexTable gMapDryfieldFullViewIndexTable = { .viewMaps = _gMapDryfieldFullAreaViewMaps };

/// Borrowed room sprite-view arrays for the 38 night-Dryfield area slots.
///
/// Indexed by area - 1; areas 4, 10, 33 and 35..37 are NULL. Populated slots
/// borrow writable room-overlay arrays, selected by mapped view - 1. There
/// is no terminator; keep this map and the selected room loaded during use.
static SpriteView* _gMapDryfieldFullSpriteViewsByArea[MAP_DRYFIELD_FULL_AREA_ID_LIMIT] = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION - 1]       = gDryfieldNightGasStationSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET - 1]       = gDryfieldNightMainStreetSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE - 1]     = gDryfieldNightGeneralStoreSpriteViews,
    [3]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET - 1]       = gDryfieldNightBackStreetSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP - 1]     = gDryfieldNightSouvenirShopSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE - 1]         = gDryfieldNightWarehouseSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_R08 - 1]               = gDryfieldNightR08SpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE - 1] = gDryfieldNightDilapidatedHouseSpriteViews,
    [9]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1 - 1]      = gDryfieldNightMotelRoom1SpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2 - 1]      = gDryfieldNightMotelRoom2SpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3 - 1]      = gDryfieldNightMotelRoom3SpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4 - 1]      = gDryfieldNightMotelRoom4SpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT - 1]       = gDryfieldNightParkingLotSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_TOILET - 1]            = gDryfieldNightToiletSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY - 1]       = gDryfieldNightMotelLobbySpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R - 1]        = gDryfieldNightSaloonGRSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN - 1]       = gDryfieldNightGRKitchenSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER - 1]       = gDryfieldNightWaterTowerSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK - 1]        = gDryfieldNightWaterTankSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY - 1]         = gDryfieldNightBreezewaySpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY - 1]           = gDryfieldNightFactorySpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE - 1]            = gDryfieldNightGarageSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY - 1]          = gDryfieldNightDrivewaySpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD - 1]         = gDryfieldNightJunkYardSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH - 1]     = gDryfieldNightTrailerCoachSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5 - 1]      = gDryfieldNightMotelRoom5SpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY - 1]     = gDryfieldNightMotelBalconySpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6 - 1]      = gDryfieldNightMotelRoom6SpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT - 1]        = gDryfieldNightMotelLoftSpriteViews,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE - 1]        = gDryfieldNightWaterHoleSpriteViews,
    [32]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR - 1]            = gDryfieldNightCellarSpriteViews,
    [34]                                             = NULL,
    [35]                                             = NULL,
    [36]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS - 1]         = gDryfieldNightUnderpassSpriteViews,
};

SpriteAreaTable gMapDryfieldFullSpriteAreaTable = { .areaViews = _gMapDryfieldFullSpriteViewsByArea };

WorldCollisionSurfaceProperties** gMapDryfieldFullSurfacePropertiesByArea[MAP_DRYFIELD_FULL_AREA_ID_LIMIT] = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION - 1]       = gDryfieldNightGasStationSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET - 1]       = gDryfieldNightMainStreetSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE - 1]     = gDryfieldNightGeneralStoreSurfacePropertiesByClass,
    [3]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET - 1]       = gDryfieldNightBackStreetSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP - 1]     = gDryfieldNightSouvenirShopSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE - 1]         = gDryfieldNightWarehouseSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_R08 - 1]               = gDryfieldNightR08SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE - 1] = gDryfieldNightDilapidatedHouseSurfacePropertiesByClass,
    [9]                                              = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1 - 1]      = gDryfieldNightMotelRoom1SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2 - 1]      = gDryfieldNightMotelRoom2SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3 - 1]      = gDryfieldNightMotelRoom3SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4 - 1]      = gDryfieldNightMotelRoom4SurfaceProperties,
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT - 1]       = gDryfieldNightParkingLotSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_TOILET - 1]            = gDryfieldNightToiletSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY - 1]       = gDryfieldNightMotelLobbySurfacePropertyTable,
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R - 1]        = gDryfieldNightSaloonGRSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN - 1]       = gDryfieldNightGRKitchenSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER - 1]       = gDryfieldNightWaterTowerSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK - 1]        = gDryfieldNightWaterTankSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY - 1]         = gDryfieldNightBreezewaySurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY - 1]           = gDryfieldNightFactorySurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE - 1]            = gDryfieldNightGarageSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY - 1]          = gDryfieldNightDrivewaySurfacePropertyTable,
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD - 1]         = gDryfieldNightJunkYardSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH - 1]     = gDryfieldNightTrailerCoachSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5 - 1]      = gDryfieldNightMotelRoom5SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY - 1]     = gDryfieldNightMotelBalconySurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6 - 1]      = gDryfieldNightMotelRoom6SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT - 1]        = gDryfieldNightMotelLoftSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE - 1]        = gDryfieldNightWaterHoleSurfacePropertiesByClass,
    [32]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR - 1]            = gDryfieldNightCellarSurfacePropertiesByClass,
    [34]                                             = NULL,
    [35]                                             = NULL,
    [36]                                             = NULL,
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS - 1]         = gDryfieldNightUnderpassSurfacePropertiesByClass,
};

AreaPlacement gMapDryfieldFullGasStationVariant1Placements[13] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 1, .mode = MAP_DRYFIELD_FULL_SCAVENGER_SCRIPTED, .x = 0x4240, .y = 0, .z = -0x10C0, .yaw = -0x262, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 2, .mode = MAP_DRYFIELD_FULL_SCAVENGER_SCRIPTED, .x = 0x3C01, .y = -0x2BC, .z = -0x940, .yaw = -0x600, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 3, .mode = MAP_DRYFIELD_FULL_SCAVENGER_SCRIPTED, .x = 0x3780, .y = -0x498, .z = -0x1280, .yaw = 0x262, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 4, .mode = MAP_DRYFIELD_FULL_SCAVENGER_SCRIPTED, .x = 0x3701, .y = 0, .z = -0x9C0, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 5, .mode = 4, .x = 0x3D40, .y = 0, .z = -0x1520, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 6, .mode = 5, .x = 0x41C0, .y = 0, .z = -0xFA0, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 7, .mode = 6, .x = 0x3D40, .y = 0, .z = -0x1520, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 8, .mode = 7, .x = 0x41C0, .y = 0, .z = -0xFA0, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 9, .mode = 8, .x = 0x3D40, .y = 0, .z = -0x1520, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0xA, .mode = 9, .x = 0x41C0, .y = 0, .z = -0xFA0, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0xB, .mode = 0xA, .x = 0x3D40, .y = 0, .z = -0x1520, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0xC, .mode = 0xB, .x = 0x41C0, .y = 0, .z = -0xFA0, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGasStationVariant2Placements[3] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x2008, .y = 0, .z = -0x125C, .yaw = 0xCE4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x3264, .y = 0, .z = -0x640, .yaw = 0xA8C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGasStationVariant3Placements[3] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_DESERT_CHASER, .variant = 0, .mode = MAP_DRYFIELD_FULL_DESERT_CHASER_TUNING_ROW_2, .x = 0x1DB0, .y = 0, .z = -0x1518, .yaw = 0x190, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_DESERT_CHASER, .variant = 0, .mode = MAP_DRYFIELD_FULL_DESERT_CHASER_TUNING_ROW_2, .x = 0x32C8, .y = 0, .z = -0x514, .yaw = 0x9C4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGasStationVariant7Placements[3] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ROOK_BEAM_SWORD, .variant = 5, .mode = MAP_DRYFIELD_FULL_ROOK_PATROL, .x = 0x1388, .y = 0, .z = -0x1388, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ROOK_GRENADE_LAUNCHER, .variant = 0, .mode = 0, .x = 0x189C, .y = 0, .z = -0x6A4, .yaw = 0x834, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGasStationVariant11Placements[5] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x2904, .y = -0xAF0, .z = -0x1770, .yaw = 0x640, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x157C, .y = -0x9C4, .z = -0x4B0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x1D4C, .y = -0x8FC, .z = -0x4B0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x2C24, .y = 0, .z = -0xC4E, .yaw = 0xABE, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMainStreetVariant1Placements[8] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x1770, .y = -0x960, .z = -0xED8, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x1644, .y = -0x898, .z = -0xF3C, .yaw = 0xC80, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x1644, .y = -0x9C4, .z = 0x76C, .yaw = 0xCE4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x898, .y = -0xAF0, .z = 0x25E4, .yaw = 0x384, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x5DC, .y = -0x898, .z = 0x2454, .yaw = 0xED8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x320, .y = -0xA28, .z = 0x28A0, .yaw = 0xA8C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_DESERT_CHASER, .variant = 0, .mode = MAP_DRYFIELD_FULL_DESERT_CHASER_TUNING_ROW_2, .x = -0x3E8, .y = 0, .z = 0xCE4, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMainStreetVariant2Placements[2] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_GARY_DOUGLAS, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMainStreetVariant7Placements[3] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BRAIN_STINGER, .variant = 0, .mode = 0, .x = -0xA28, .y = 0, .z = 0x3E8, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BRAIN_STINGER, .variant = 0, .mode = 1, .x = -0x6A4, .y = 0, .z = 0x189C, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGeneralStoreVariant1Placements[5] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0xA28, .y = -0xB54, .z = 0x6A4, .yaw = 0xE74, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x898, .y = -0xAF0, .z = 0x640, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x834, .y = -0xB54, .z = 0x1068, .yaw = 0x4B0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0xAF0, .y = -0xAF0, .z = 0x1004, .yaw = 0xC80, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGeneralStoreVariant2Placements[4] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = 0, .x = 0x708, .y = -0x3E8, .z = 0xA28, .yaw = 0x384, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = 0, .x = 0xA8C, .y = -0x3E8, .z = 0x15E0, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = 0, .x = 0x514, .y = 0, .z = 0xBB8, .yaw = 0x64, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGeneralStoreVariant3Placements[9] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0xBB8, .y = -0x9C4, .z = 0x1B58, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x1B58, .y = -0x9C4, .z = 0x1AF4, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x1D4C, .y = -0x960, .z = 0x1BBC, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x1C84, .y = -0x8FC, .z = 0x1B58, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x1C20, .y = 0, .z = 0x258, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x173E, .y = 0, .z = 0xAF0, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x19C8, .y = 0, .z = 0x1324, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x13BA, .y = 0, .z = 0x1BBC, .yaw = 0x384, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGeneralStoreVariant21Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x17D4, .y = 0, .z = 0x44C, .yaw = 0x1F4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 5 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x1A90, .y = 0, .z = 0x384, .yaw = 0xE10, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 5 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x1644, .y = 0, .z = 0x4B0, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 5 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = 0, .x = 0x12C, .y = 0, .z = 0xDAC, .yaw = 0x3E8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = 0, .x = 0x20D0, .y = 0, .z = 0x898, .yaw = 0xE10, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullBackStreetVariant1Placements[7] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = -0x1838, .y = 0, .z = 0x1194, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = -0x1F4, .y = 0, .z = 0xF3C, .yaw = 0x12C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = 0x12C0, .y = 0, .z = 0xD48, .yaw = 0x190, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = -0x1900, .y = 0, .z = 0x1004, .yaw = 0xDAC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = -0x1F4, .y = 0, .z = 0x1068, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x12C0, .y = 0, .z = 0x12C0, .yaw = 0x4B0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullBackStreetVariant3Placements[7] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = -0xBB8, .y = -0x898, .z = 0x1770, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x7D0, .y = -0x8FC, .z = 0x1770, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x1964, .y = -0x7D0, .z = 0x1770, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x20D0, .y = -0x5DC, .z = 0x7D0, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x1C20, .y = -0x6A4, .z = 0x7D0, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_DESERT_CHASER, .variant = 0, .mode = MAP_DRYFIELD_FULL_DESERT_CHASER_TUNING_ROW_2, .x = 0x170C, .y = 1, .z = 0xED8, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullBackStreetVariant4Placements[8] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_DESERT_CHASER, .variant = 0, .mode = MAP_DRYFIELD_FULL_DESERT_CHASER_TUNING_ROW_2, .x = 0x2710, .y = 0, .z = 0xAF0, .yaw = 0xC80, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_DESERT_CHASER, .variant = 0, .mode = MAP_DRYFIELD_FULL_DESERT_CHASER_TUNING_ROW_2, .x = 0xAF0, .y = 0, .z = 0xED8, .yaw = 0xD48, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x2580, .y = -0x960, .z = 0x13EC, .yaw = 0xA8C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x25E4, .y = -0xA28, .z = 0x1194, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x2580, .y = -0x9C4, .z = 0xED8, .yaw = 0xD48, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x2BC, .y = -0x7D0, .z = 0x16A8, .yaw = 0xF3C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x258, .y = -0x708, .z = 0x1644, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullBackStreetVariant7Placements[2] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ROOK_GRENADE_LAUNCHER, .variant = 0xC, .mode = MAP_DRYFIELD_FULL_ROOK_PATROL, .x = 0x1770, .y = 0, .z = 0x12C0, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullBackStreetVariant21Placements[15] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x7D0, .y = -0x9C4, .z = 0x1770, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x834, .y = -0x7D0, .z = 0x1770, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x9C4, .y = -0xA8C, .z = 0x1770, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0xB54, .y = -0x960, .z = 0x1770, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0xDAC, .y = -0x9C4, .z = 0x1770, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0xF3C, .y = -0xA8C, .z = 0x1770, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x1964, .y = -0x708, .z = 0x7D0, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x1AF4, .y = -0x578, .z = 0x7D0, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x1C20, .y = -0x640, .z = 0x7D0, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x1CE8, .y = -0x4B0, .z = 0x7D0, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x1DB0, .y = -0x6A4, .z = 0x7D0, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x1E78, .y = -0x4B0, .z = 0x7D0, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x1FA4, .y = -0x5DC, .z = 0x7D0, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x20D0, .y = -0x6A4, .z = 0x7D0, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullSouvenirShopVariant1Placements[5] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x1324, .y = -0x320, .z = -0xED8, .yaw = 0x960, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x11F8, .y = -0x4B0, .z = -0xE10, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x1004, .y = -0x1F4, .z = -0xE74, .yaw = 0x6A4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0xED8, .y = -0x640, .z = -0xDAC, .yaw = 0x578, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullSouvenirShopVariant3Placements[5] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = 0, .x = 0x15E0, .y = 0, .z = -0xC1C, .yaw = 0x578, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = 0, .x = 0x4B0, .y = 0, .z = -0x320, .yaw = 0xDAC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = 0, .x = 0x1450, .y = 0, .z = -0x4E2, .yaw = 0x708, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = 0, .x = 0x13EC, .y = 0, .z = -0x190, .yaw = 0x2BC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullSouvenirShopVariant11Placements[11] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x4B0, .y = -0x7D0, .z = -0x1F4, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x578, .y = -0x898, .z = -0x1F4, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x514, .y = -0x708, .z = -0x1F4, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x1324, .y = -0x320, .z = -0xED8, .yaw = 0x960, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x11F8, .y = -0x4B0, .z = -0xE10, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x1004, .y = -0x1F4, .z = -0xE74, .yaw = 0x6A4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0xED8, .y = -0x640, .z = -0xDAC, .yaw = 0x578, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x141E, .y = 0, .z = -0x190, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x1518, .y = 0, .z = -0xC4E, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x4B0, .y = 0, .z = -0x320, .yaw = 0x47E, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWarehouseVariant1Placements[3] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x6A4, .y = 0, .z = -0x4B0, .yaw = 0x5DC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x11F8, .y = 0, .z = -0xC80, .yaw = 0xDAC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWarehouseVariant3Placements[7] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0x5DC, .y = 0, .z = -0x190, .yaw = 0xFA0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0x190, .y = 0, .z = -0xB22, .yaw = 0xBB8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0xA8C, .y = 0, .z = -0x47E, .yaw = 0xF3C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = 0, .mode = 0, .x = 0xC80, .y = 0, .z = -0x514, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = 0, .mode = 0, .x = 0x1194, .y = 0, .z = -0x6A4, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = 0, .mode = 0, .x = 0x1388, .y = 0, .z = -0x9C4, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWarehouseVariant11Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0xAF0, .y = 0, .z = -0x578, .yaw = 0x384, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0xB86, .y = 0, .z = -0xA8C, .yaw = 0xDAC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x190, .y = 0, .z = -0xBEA, .yaw = 0x384, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x672, .y = 0, .z = -0x190, .yaw = 0x7D0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x1388, .y = 0, .z = -0xE10, .yaw = 0xE42, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullR08Variant1Placements[5] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_AYA_WALKER, .variant = 0, .mode = 0, .x = 0x2D, .y = 0, .z = -0x2710, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_PAWN_GOLEM, .variant = 0, .mode = 0, .x = -0x3F, .y = 0, .z = -0x39A3, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_PAWN_GOLEM, .variant = 0, .mode = 0, .x = -0x898, .y = 0, .z = -0x3E8, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_PAWN_GOLEM, .variant = 0, .mode = 0, .x = -0x97, .y = 0, .z = -0x9A7, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullDilapidatedHouseVariant3Placements[8] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = -0xC8, .y = 0, .z = -0x258, .yaw = 0xED8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = -0x76C, .y = 0, .z = -0x5DC, .yaw = 0x44C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x9C4, .y = 0, .z = 0xA8C, .yaw = 0xA5A, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0xE74, .y = 0, .z = -0x76C, .yaw = 0xCE4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = -0x9C4, .y = 0, .z = 0xA8C, .yaw = 0x6A4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x96, .y = 0, .z = 0xA8C, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = -0xBB8, .y = 0, .z = 0, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullDilapidatedHouseVariant4Placements[7] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0x12C, .y = 0, .z = 0, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = -0x5DC, .y = 0, .z = -0xC8, .yaw = 0x4B0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1F4, .y = 0, .z = -0x384, .yaw = 0xC1C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x258, .y = 0, .z = -0x12C, .yaw = 0x76C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x3E8, .y = 0, .z = -0x47E, .yaw = 0x258, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0xC8, .y = 0, .z = -0x578, .yaw = 0x64, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullDilapidatedHouseVariant21Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x258, .y = 0, .z = 0x898, .yaw = 0x200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x3E8, .y = 0, .z = 0x5DC, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x708, .y = 0, .z = 0x44C, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0xAF0, .y = 0, .z = 0x834, .yaw = 0xA00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0xBB8, .y = 0, .z = 0x44C, .yaw = 0xE00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelRoom1Variant1Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0x320, .y = 1, .z = 0xDAC, .yaw = 0xDAC, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0x708, .y = 1, .z = 0x4B0, .yaw = 0x834, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1194, .y = 0, .z = 0x15E0, .yaw = 0x1F4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0xFA0, .y = 0, .z = 0x3E8, .yaw = 0x76C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0xA28, .y = 0, .z = 0xFA0, .yaw = 0xDAC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelRoom1Variant2Placements[9] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x1F4, .y = 0, .z = 0xD7A, .yaw = 0x41A, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x6A4, .y = 0, .z = 0x4B0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0xEA6, .y = 0, .z = 0x41A, .yaw = 0xA28, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x3E8, .y = 0, .z = 0x157C, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0xCE4, .y = 0, .z = 0x14B4, .yaw = 0x4B0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x7D0, .y = -0x640, .z = 0x3E8, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x6A4, .y = -0x76C, .z = 0x320, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x578, .y = -0x6A4, .z = 0x320, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelRoom1Variant21Placements[4] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1F4, .y = 0, .z = 0x1388, .yaw = 0xA28, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1F4, .y = 0, .z = 0x17D4, .yaw = 0xDAC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x6A4, .y = 0, .z = 0x4B0, .yaw = 0x960, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelRoom2Variant2Placements[8] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x960, .y = 0, .z = 0x12C0, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x384, .y = 0, .z = 0x1324, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x672, .y = 0, .z = 0x47E, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0xDDE, .y = 0, .z = 0x3B6, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x1194, .y = 0, .z = 0x1130, .yaw = 0x546, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x1194, .y = 0, .z = 0x154A, .yaw = 0x1F4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0xE10, .y = 0, .z = 0x13EC, .yaw = 0xA5A, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelRoom2Variant3Placements[7] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0xE10, .y = 1, .z = 0x41A, .yaw = 0xA8C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0x9C4, .y = 1, .z = 0xAF0, .yaw = 0x76C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0x1194, .y = 1, .z = 0x1518, .yaw = 0x1F4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0xBB8, .y = 1, .z = 0x13EC, .yaw = 0xF3C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0x226, .y = 1, .z = 0xBB8, .yaw = 0x1F4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0x1F4, .y = 1, .z = 0x13BA, .yaw = 0xDAC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelRoom2Variant21Placements[7] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1F4, .y = 0, .z = 0x1388, .yaw = 0xE10, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1194, .y = 0, .z = 0x14B4, .yaw = 0x258, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x898, .y = -0x960, .z = 0x6A4, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 4 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x9C4, .y = -0x8FC, .z = 0x960, .yaw = 0x960, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 4 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x7D0, .y = -0x9C4, .z = 0x8FC, .yaw = 0x514, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 4 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x960, .y = -0x898, .z = 0x76C, .yaw = 0xE74, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 4 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelRoom3Variant1Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0xFA0, .y = 0, .z = 0xC80, .yaw = 0x64, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x7D0, .y = 0, .z = 0x4B0, .yaw = 0x4B0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0x3E8, .y = 0, .z = 0xC80, .yaw = 0xE10, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0xDAC, .y = 0, .z = 0x1068, .yaw = 0x578, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ODD_STRANGER, .variant = 0x10, .mode = 0, .x = 0xE74, .y = 1, .z = 0x1964, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelRoom3Variant2Placements[7] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0xD7A, .y = 0, .z = 0xCB2, .yaw = 0x76C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x898, .y = 0, .z = 0xC4E, .yaw = 0x41A, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x7D0, .y = 0, .z = 0x320, .yaw = 0x384, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1194, .y = 0, .z = 0x1964, .yaw = 0x190, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0xEA6, .y = 0, .z = 0x1964, .yaw = 0xE10, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0xFD2, .y = 0, .z = 0x189C, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelRoom4Variant1Placements[3] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ODD_STRANGER, .variant = 0x10, .mode = 0, .x = 0x1770, .y = 1, .z = 0x834, .yaw = 0xA8C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ODD_STRANGER, .variant = 0x10, .mode = 0, .x = 0x76C, .y = 1, .z = 0xCE4, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelRoom4Variant2Placements[8] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x16A8, .y = 1, .z = 0x898, .yaw = 0xA8C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x157C, .y = 1, .z = 0x384, .yaw = 0xB54, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0xC80, .y = 1, .z = 0xDAC, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0xABE, .y = 1, .z = 0xFA0, .yaw = 0x76C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x92E, .y = -0x898, .z = 0xAF0, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x9C4, .y = -0x960, .z = 0xA28, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x898, .y = -0x7D0, .z = 0xA28, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullParkingLotVariant2Placements[7] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_DESERT_CHASER, .variant = 0, .mode = MAP_DRYFIELD_FULL_DESERT_CHASER_TUNING_ROW_2, .x = 0xBB8, .y = 0, .z = 0x258, .yaw = 0xDAC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_DESERT_CHASER, .variant = 0, .mode = MAP_DRYFIELD_FULL_DESERT_CHASER_TUNING_ROW_2, .x = -0xDAC, .y = 0, .z = -0x1068, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x1068, .y = -0x708, .z = 0xDAC, .yaw = 0xED8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0xFA0, .y = -0x640, .z = 0xD48, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0xB54, .y = -0x960, .z = 0x578, .yaw = 0x960, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0xAF0, .y = -0x898, .z = 0x320, .yaw = 0xF3C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullParkingLotVariant3Placements[5] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_WAVE_WAIT_IMMEDIATE, .x = -0x9C4, .y = -0x1F4, .z = 0xDAC, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_WAVE_WAIT_IMMEDIATE, .x = 0xE10, .y = 0, .z = 0x2EE, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_WAVE_WAIT_RANDOM_DELAY, .x = 0x708, .y = -0x7D0, .z = -0x1F4, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_WAVE_WAIT_IMMEDIATE, .x = 0x708, .y = -0x7D0, .z = 0xFA0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullParkingLotVariant4Placements[7] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = 0xC1C, .y = 0, .z = 0x4B0, .yaw = -0xC8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = 0x64, .y = 0, .z = 0xBB8, .yaw = 0x578, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x21FC, .y = 0, .z = 0xDAC, .yaw = 0x44C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x2260, .y = 0, .z = 0x960, .yaw = 0xC4E, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_ROAM, .x = 0x1E78, .y = 0, .z = 0xD16, .yaw = 0xB22, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_DESERT_CHASER, .variant = 0, .mode = MAP_DRYFIELD_FULL_DESERT_CHASER_TUNING_ROW_2, .x = -0x898, .y = 0, .z = 0x898, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullParkingLotVariant7Placements[2] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BISHOP_GOLEM, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullParkingLotVariant21Placements[12] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x320, .y = -0x640, .z = 0x960, .yaw = 0x190, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x5DC, .y = -0x708, .z = 0x320, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = -0x44C, .y = -0x708, .z = 0x834, .yaw = -0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = -0x76C, .y = -0x578, .z = 0x898, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = -0xA28, .y = -0x640, .z = 0x7D0, .yaw = 0x64, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = -0x3E8, .y = -0x6A4, .z = 0x6A4, .yaw = 0x2BC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = -0x6A4, .y = -0x514, .z = 0x640, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = -0xA8C, .y = -0x640, .z = 0x5DC, .yaw = 0x384, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = -0x4B0, .y = -0x578, .z = 0x384, .yaw = 0x578, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = -0x708, .y = -0x640, .z = 0x320, .yaw = 0x4B0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = -0x9C4, .y = -0x708, .z = 0x258, .yaw = 0x5DC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullToiletVariant1Placements[7] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x36, .y = 0, .z = -0x20B, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x36, .y = 0, .z = -0x20B, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x36, .y = 0, .z = -0x20B, .yaw = 0x100, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x36, .y = 0, .z = -0x20B, .yaw = -0x780, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x36, .y = 0, .z = -0x20B, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0x36, .y = 0, .z = -0x20B, .yaw = 0x6A4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullToiletVariant3Placements[12] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x2BC, .y = 0, .z = 0x6A4, .yaw = 0x2BC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0x6A4, .yaw = 0x834, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0x2BC, .yaw = 0x578, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x1F4, .y = 0, .z = 0x190, .yaw = 0xE74, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x1F4, .y = 0, .z = -0x258, .yaw = 0x960, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0xC8, .y = -0x960, .z = 0x6A4, .yaw = 0xED8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0, .y = -0x7D0, .z = 0x640, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0xC8, .y = -0x898, .z = 0x6A4, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x64, .y = -0x8FC, .z = -0x6A4, .yaw = 0x834, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x1F4, .y = -0x9C4, .z = -0x6A4, .yaw = 0x708, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullToiletVariant11Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x2BC, .y = 0, .z = 0x6A4, .yaw = 0x200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0, .y = 0, .z = 0x6A4, .yaw = 0xE10, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1F4, .y = 0, .z = 0xC8, .yaw = 0x708, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x2BC, .y = 0, .z = -0x6A4, .yaw = 0x5AA, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x6A4, .y = 0, .z = -0x6A4, .yaw = 0xA28, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullSaloonGRVariant1Placements[2] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_KYLE_MADIGAN, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullSaloonGRVariant2Placements[3] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullSaloonGRVariant3Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_PATROL, .mode = 0, .x = 0xC8, .y = 0, .z = 0x12C0, .yaw = 0x12C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x12C, .y = 0, .z = 0x16DA, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x64, .y = 0, .z = 0x1838, .yaw = 0xE10, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x44C, .y = 0, .z = 0x1838, .yaw = 0x258, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1004, .y = 0, .z = 0x12C, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGRKitchenVariant1Placements[8] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x258, .y = 0, .z = -0x7D0, .yaw = 0x6A4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x1F4, .y = 0, .z = -0x7D0, .yaw = 0xA28, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x258, .y = 0, .z = -0x2BC, .yaw = 0x2BC, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x258, .y = 0, .z = 0x64, .yaw = 0x514, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x258, .y = 0, .z = 0x1F4, .yaw = 0x640, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = -0x1F4, .y = 0, .z = 0x64, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = -0x1F4, .y = 0, .z = -0x12C, .yaw = 0xC80, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGRKitchenVariant3Placements[11] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = -0x1F4, .y = 0, .z = -0x7D0, .yaw = 0x9C4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = -0x1F4, .y = 0, .z = -0x640, .yaw = 0xCE4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = -0x1F4, .y = 0, .z = -0x384, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = -0x7D0, .yaw = 0x9C4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x190, .y = 0, .z = -0x7D0, .yaw = 0x640, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x258, .y = 0, .z = -0x3E8, .yaw = 0x3E8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = -0x1F4, .y = 0, .z = 0xC8, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = -0x384, .yaw = 0x834, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x64, .y = -0x960, .z = -0x1F4, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 6, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0, .y = -0x960, .z = 0x12C, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 6, .rowIndex = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGRKitchenVariant11Placements[8] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = -0x1F4, .y = 0, .z = -0x578, .yaw = 0x960, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = -0xC8, .y = 0, .z = -0x578, .yaw = 0x708, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0, .y = 0, .z = -0x708, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER, .variant = MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE, .mode = 0, .x = 0x1F4, .y = 0, .z = -0x640, .yaw = 0x578, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x1F4, .y = 0, .z = 0x2BC, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x258, .y = 0, .z = -0x834, .yaw = 0x6D6, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x1F4, .y = 0, .z = -0x834, .yaw = 0x9F6, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWaterTowerVariant1Placements[10] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x1004, .y = -0x9C4, .z = 0x708, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x11F8, .y = -0xA8C, .z = 0x898, .yaw = 0x9C4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x10CC, .y = -0x8FC, .z = 0x640, .yaw = 0xED8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x157C, .y = -0xA28, .z = -0xAF0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x1388, .y = -0x960, .z = -0xB54, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = -0x157C, .y = -0x9C4, .z = -0xC1C, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x1F4, .y = 0, .z = -0x1644, .yaw = 0x834, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0xC8, .y = 0, .z = -0xD48, .yaw = 0xF3C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1F4, .y = 0, .z = -0x1644, .yaw = 0x640, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWaterTowerVariant2Placements[4] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x14B4, .y = 0, .z = -0xDAC, .yaw = -0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x12C, .y = 0, .z = -0xFA0, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x2BC, .y = 0, .z = -0x1388, .yaw = 0x3E8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWaterTowerVariant11Placements[2] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MAD_CHASER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MAD_CHASER_DANGLE, .x = 0x1C2, .y = -0xE74, .z = 0x1130, .yaw = 0xA28, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWaterTowerVariant21Placements[7] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = 0x10CC, .y = 0, .z = 0x12C, .yaw = 0x8CA, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_SCORPION_TUNING_ROW_3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = 0, .y = 0, .z = -0x1068, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_SCORPION_TUNING_ROW_3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = 0x12C0, .y = 0, .z = -0xF6E, .yaw = -0x64, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_SCORPION_TUNING_ROW_3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = 0xD7A, .y = 0, .z = -0xC8, .yaw = 0x44C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_SCORPION_TUNING_ROW_3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = -0x3E8, .y = 0, .z = -0x11F8, .yaw = 0x384, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0xA28, .y = 0, .z = -0xC8, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWaterTankVariant1Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWaterTankVariant10Placements[5] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_HANDOVER_FIGURE, .variant = 0, .mode = 0, .x = 0x456, .y = -0x2EE0, .z = -0x9C4, .yaw = 0x71C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = MAP_DRYFIELD_FULL_MIND_SUCKLER_SCRIPTED, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = -0x640, .y = -0x3200, .z = -0x9C4, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = MAP_DRYFIELD_FULL_MIND_SUCKLER_SCRIPTED, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x44C, .y = -0x3200, .z = -0xA28, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = MAP_DRYFIELD_FULL_MIND_SUCKLER_SCRIPTED, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0xFA0, .y = -0x3200, .z = -0xA8C, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWaterTankVariant11Placements[2] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_HANDOVER_FIGURE, .variant = 0, .mode = 0, .x = 0x456, .y = -0x2EE0, .z = -0x9C4, .yaw = 0x71C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullBreezewayVariant1Placements[13] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x32C8, .y = -0x898, .z = 0x898, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x332C, .y = -0x708, .z = 0x898, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x319C, .y = -0x834, .z = 0x898, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x3138, .y = -0x76C, .z = 0x898, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x30D4, .y = -0x8FC, .z = 0x898, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = 0x3070, .y = -0x898, .z = 0x898, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0x3200, .y = -0x640, .z = 0x8FC, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0x319C, .y = -0x4B0, .z = 0x960, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0x3070, .y = -0x5DC, .z = 0x8FC, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0x300C, .y = -0x514, .z = 0x960, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0x2F44, .y = -0x578, .z = 0x8FC, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0x2FA8, .y = -0x44C, .z = 0x960, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullBreezewayVariant3Placements[13] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_DESERT_CHASER, .variant = 0, .mode = 0, .x = 0x396C, .y = 1, .z = 0x640, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x445C, .y = -0xA8C, .z = 0xAF0, .yaw = 0x4B0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x4524, .y = -0x9C4, .z = 0x9C4, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x44C0, .y = -0xAF0, .z = 0xA28, .yaw = 0x3E8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x445C, .y = -0xA28, .z = 0xA8C, .yaw = 0x384, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x3778, .y = -0xA28, .z = 0x514, .yaw = 0x76C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x36B0, .y = -0xAF0, .z = 0x578, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x38A4, .y = -0xA8C, .z = 0x5DC, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x2328, .y = -0xA28, .z = 0x514, .yaw = 0x834, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x2260, .y = -0x960, .z = 0x578, .yaw = 0x7D0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x238C, .y = -0xAF0, .z = 0x578, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0x2328, .y = -0x9C4, .z = 0x578, .yaw = 0x834, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullBreezewayVariant21Placements[10] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = 0x445C, .y = 0, .z = 0x5DC, .yaw = 0xC1C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_SCORPION_TUNING_ROW_3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = 0x2710, .y = 0, .z = 0x708, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_SCORPION_TUNING_ROW_3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = 0x445C, .y = 0, .z = 0xB54, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_SCORPION_TUNING_ROW_3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x2580, .y = -0x1388, .z = 0x640, .yaw = 0x9C4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x28A0, .y = -0x1388, .z = 0x4B0, .yaw = 0x5DC, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x2BC0, .y = -0x1388, .z = 0x3E8, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x2EE0, .y = -0x1388, .z = 0x5DC, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x3200, .y = -0x1388, .z = 0x44C, .yaw = 0xC80, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x3520, .y = -0x1388, .z = 0x514, .yaw = 0x960, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGarageVariant1Placements[5] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1F40, .y = 0, .z = 0x1130, .yaw = 0x5DC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x238C, .y = 0, .z = 0xA28, .yaw = 0x1F4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x2710, .y = 0, .z = 0xDAC, .yaw = 0x708, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x2648, .y = 0, .z = 0xB54, .yaw = 0xE10, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGarageVariant2Placements[3] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_GARY_DOUGLAS, .variant = 0, .mode = 0, .x = 0x1644, .y = -0x96, .z = 0x170C, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_FLINT, .variant = 0, .mode = 0, .x = 0x14B4, .y = 0, .z = 0x1B58, .yaw = 0x7FF, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGarageVariant3Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullGarageVariant4Placements[4] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0xBB8, .y = 0, .z = 0x640, .yaw = 0xDAC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x22C4, .y = 0, .z = 0x1A90, .yaw = 0xA8C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x24B8, .y = 0, .z = 0xA28, .yaw = 0xC1C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullDrivewayVariant1Placements[14] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0x3E8, .y = -0x708, .z = -0x76C, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0x258, .y = -0x578, .z = -0x76C, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0xC8, .y = -0x640, .z = -0x76C, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = -0x708, .y = -0x6A4, .z = -0x708, .yaw = 0xC80, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = -0x960, .y = -0x514, .z = -0x384, .yaw = 0xA28, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = -0x7D0, .y = -0x7D0, .z = -0x44C, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = -0x7D0, .y = -0x640, .z = -0x4B0, .yaw = 0xC1C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x514, .y = -0x834, .z = 0x708, .yaw = 0xC1C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x834, .y = -0x898, .z = 0xA8C, .yaw = 0x960, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x898, .y = -0x7D0, .z = 0x4B0, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x4B0, .y = -0x640, .z = 0x44C, .yaw = 0xBB8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x320, .y = -0x708, .z = 0x640, .yaw = 0x6A4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x1F4, .y = -0x76C, .z = 0x7D0, .yaw = 0xA8C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullDrivewayVariant3Placements[13] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0x514, .y = -0x1194, .z = 0x7D0, .yaw = 0x578, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_44_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0x4B0, .y = -0x10CC, .z = 0x6A4, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_44_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0x640, .y = -0x1130, .z = 0x578, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_44_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0x578, .y = -0x1194, .z = 0x2BC, .yaw = 0x258, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_44_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0xA8C, .y = -0xFA0, .z = 0x898, .yaw = 0x578, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0xAF0, .y = -0x1004, .z = 0x708, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0xBB8, .y = -0x1068, .z = 0x514, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0xB54, .y = -0x10CC, .z = 0x384, .yaw = 0x258, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_36_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0xC80, .y = -0xED8, .z = 0x834, .yaw = 0x578, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_28_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0xD48, .y = -0xE10, .z = 0x640, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_28_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0xC1C, .y = -0xDAC, .z = 0x4B0, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_28_ROW },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_BACK, .x = -0xCE4, .y = -0xE74, .z = 0x320, .yaw = 0x258, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_FULL_BAT_TURN_RATE_28_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullDrivewayVariant21Placements[9] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = -0xA8C, .y = 0, .z = 0x320, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_SCORPION_TUNING_ROW_3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = 0x12C, .y = 0, .z = 0x384, .yaw = 0xCE4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_SCORPION_TUNING_ROW_3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCORPION, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER, .x = 0xC8, .y = 0, .z = 0x866, .yaw = 0xA8C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_SCORPION_TUNING_ROW_3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x79E, .y = -0x6A4, .z = 0xBB8, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0xD48, .y = -0x76C, .z = 0xBB8, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0xC80, .y = -0x898, .z = 0, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x320, .y = -0x708, .z = 0, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = -0xFA0, .y = -0xAF0, .z = -0x1F4, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullJunkYardVariant1Placements[3] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_JUNK_YARD_ENTRY | MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x1082, .y = 0, .z = -0x1180, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_SECOND_DROP | MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x1C6D, .y = 0, .z = -0xDE1, .yaw = 0x200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullJunkYardVariant2Placements[5] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x1F40, .y = -0x9C4, .z = 0, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x1F40, .y = -0x708, .z = 0x12F2, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_DESERT_CHASER, .variant = 0, .mode = MAP_DRYFIELD_FULL_DESERT_CHASER_TUNING_ROW_2, .x = 0x36B0, .y = -1, .z = 0xE74, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_DESERT_CHASER, .variant = 0, .mode = MAP_DRYFIELD_FULL_DESERT_CHASER_TUNING_ROW_2, .x = 0x2AF8, .y = -1, .z = 0x258, .yaw = 0xED8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullJunkYardVariant4Placements[4] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x34BC, .y = 0, .z = 0x578, .yaw = 0xED8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x1AF4, .y = 0, .z = 0xAF0, .yaw = 0x4B0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x898, .y = 0, .z = 0x960, .yaw = 0x4B0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullJunkYardVariant7Placements[3] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_PAWN_GOLEM_105600, .variant = MAP_DRYFIELD_FULL_PAWN_PATROL_LENGTH_THOUSANDS, .mode = MAP_DRYFIELD_FULL_PAWN_PATROL, .x = 0x1B58, .y = 0, .z = 0x7D0, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ROOK_BEAM_SWORD, .variant = 0, .mode = 0, .x = 0x32C8, .y = 0, .z = 0xED8, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullJunkYardVariant21Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x3200, .y = -0x76C, .z = 0x12F2, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x2BC0, .y = -0x640, .z = 0x12F2, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x1F40, .y = -0x960, .z = 0, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = 0x2AF8, .y = -0x898, .z = 0, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x5334, .y = 0, .z = 0xE74, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullTrailerCoachVariant1Placements[2] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_GARY_DOUGLAS, .variant = 0, .mode = 0, .x = 0xEE2, .y = 0, .z = -0x852, .yaw = 0x200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelRoom5Variant1Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ODD_STRANGER, .variant = MAP_DRYFIELD_FULL_ODD_STRANGER_STALK, .mode = 0, .x = 0x509, .y = 1, .z = 0x69D, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ODD_STRANGER, .variant = MAP_DRYFIELD_FULL_ODD_STRANGER_STALK, .mode = 0, .x = 0x708, .y = 1, .z = 0x1388, .yaw = 0x400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0xC1C, .y = -0x8FC, .z = 0x898, .yaw = 0x640, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0xE10, .y = -0x834, .z = 0x960, .yaw = 0x960, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOTH, .variant = 0, .mode = 0, .x = 0xC80, .y = -0x898, .z = 0x834, .yaw = 0x258, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_FULL_MOTH_PURSUIT_ROW_2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelRoom5Variant2Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1F4, .y = 0, .z = 0xC80, .yaw = 0x8FC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x5AA, .y = 0, .z = 0xC80, .yaw = 0x6A4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x41A, .y = 0, .z = 0xC80, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x190, .y = 0, .z = 0xE42, .yaw = 0x708, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x5DC, .y = 0, .z = 0xE74, .yaw = 0x92E, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelBalconyVariant1Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x1FA4, .y = -0xC80, .z = 0xA8C, .yaw = 0x4B0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x1A90, .y = -0xC80, .z = 0x5DC, .yaw = 0x708, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x1518, .y = -0xC80, .z = 0x29CC, .yaw = 0x1F4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x1518, .y = -0xC80, .z = 0x1F4, .yaw = 0x640, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x15E0, .y = -0xC80, .z = -0xE10, .yaw = 0x514, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelBalconyVariant2Placements[4] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BURNER, .variant = 0, .mode = 0, .x = -0x620, .y = 0, .z = 0x980, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_GARY_DOUGLAS, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 6, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_FLINT, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 6, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelBalconyVariant3Placements[3] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = -0x1838, .y = -0xC80, .z = 0x1644, .yaw = 0x64, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = -0x1644, .y = -0xC80, .z = 0x76C, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelBalconyVariant11Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = -0x2008, .y = -0x1450, .z = 0xBB8, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = -0x1CE8, .y = -0x157C, .z = 0x3E8, .yaw = 0x800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = -0x1B58, .y = -0x1388, .z = 0x12C, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = -0x12C, .y = -0x15E0, .z = 0x2AF8, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH, .x = -0x1388, .y = -0x13EC, .z = 0x2AF8, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelLoftVariant1Placements[4] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_FIRST_DROP | MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x898, .y = 0, .z = 0, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_GROUP_WAIT | MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0xDDE, .y = 0, .z = 0x186, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_GROUP_WAIT | MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x258, .y = 0, .z = -0x578, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelLoftVariant3Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x44C, .y = 0, .z = -0x44C, .yaw = 0x2BC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x125C, .y = 0, .z = 0x5DC, .yaw = 0x44C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x128E, .y = 0, .z = -0x258, .yaw = 0x384, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0xCE4, .y = 0, .z = 0x5DC, .yaw = 0x190, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x708, .y = 0, .z = -0x5DC, .yaw = 0x4B0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelLoftVariant4Placements[8] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x12C0, .y = 0, .z = 0x60E, .yaw = 0x1F4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x12C0, .y = 0, .z = 0x320, .yaw = 0x190, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x12C0, .y = 0, .z = 0xC8, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0xFA0, .y = 0, .z = 0x60E, .yaw = 0x384, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0xF3C, .y = 0, .z = 0x320, .yaw = 0x1F4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0xD48, .y = 0, .z = 0x41A, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = MAP_DRYFIELD_FULL_SCAVENGER_FORCE_POSED, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1130, .y = 0, .z = 0x1C2, .yaw = 0x64, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullMotelLoftVariant11Placements[7] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x1644, .y = 0, .z = 0x640, .yaw = 0xE74, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x1644, .y = 0, .z = -0x640, .yaw = 0x898, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x4E2, .y = 0, .z = 0, .yaw = 0xBB8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0xA28, .y = 0, .z = 0x320, .yaw = 0xAF0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x898, .y = 0, .z = 0x60E, .yaw = 0xED8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x128E, .y = 0, .z = 0x60E, .yaw = 0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWaterHoleVariant1Placements[2] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_WATER_ENTRANCE | MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x14B4, .y = 0, .z = -0x3E8, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWaterHoleVariant2Placements[3] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOSSBACK, .variant = 0, .mode = 0, .x = 0x12C0, .y = 0, .z = -0x640, .yaw = 0xDDE, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOSSBACK, .variant = 0, .mode = 0, .x = 0x3E80, .y = 0, .z = -0xD48, .yaw = 0xC00, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWaterHoleVariant10Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullWaterHoleVariant21Placements[4] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x1C20, .y = 0, .z = -0x190, .yaw = 0xB54, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x2968, .y = 0, .z = -0xBB8, .yaw = 0x12C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING, .x = 0x3C8C, .y = 0, .z = -0xA8C, .yaw = 0x4B0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullCellarVariant1Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x514, .y = 0, .z = 0x12C, .yaw = 0xA28, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x514, .y = 0, .z = 0xCE4, .yaw = 0xE10, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x898, .y = 0, .z = 0xCE4, .yaw = 0x190, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x25E4, .y = 0, .z = 0x12C, .yaw = 0x5DC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x27D8, .y = 0, .z = 0x514, .yaw = 0x578, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullCellarVariant3Placements[12] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x1644, .y = -0x5DC, .z = 0x6A4, .yaw = -0x64, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 4 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x22C4, .y = -0x76C, .z = 0x4B0, .yaw = -0x12C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 4 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x2134, .y = -0x6A4, .z = 0x5DC, .yaw = -0x2BC, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 4 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x2328, .y = -0x898, .z = 0x578, .yaw = -0x190, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 4 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x2328, .y = -0x514, .z = 0x384, .yaw = -0x3E8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 4 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_WANDER, .x = 0x1C84, .y = -0x76C, .z = 0x640, .yaw = -0x1F4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 4 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0xED8, .y = 0, .z = 0x1068, .yaw = 0xD48, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x1CE8, .y = 0, .z = 0xEA6, .yaw = 0x834, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x25E4, .y = 0, .z = 0x12C, .yaw = 0x578, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x125C, .y = 0, .z = 0x12C, .yaw = 0x60E, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_RAT, .variant = 0, .mode = 0, .x = 0x898, .y = 0, .z = 0xCE4, .yaw = 0x64, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullCellarVariant11Placements[11] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1AF4, .y = 0, .z = 0x1F4, .yaw = 0x8CA, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x546, .y = 0, .z = 0x898, .yaw = -0xC8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0xAF0, .y = -0x9C4, .z = 0x1F4, .yaw = 0x12C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 5 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0x1194, .y = -0x9C4, .z = 0x3E8, .yaw = 0x190, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 5 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0x10CC, .y = -0x9C4, .z = 0x2BC, .yaw = 0x64, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 5 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0x1450, .y = -0x9C4, .z = 0x1F4, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 5 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0x1900, .y = -0x9C4, .z = 0x3E8, .yaw = -0x64, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 5 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0x1996, .y = -0x9C4, .z = 0x1F4, .yaw = -0xC8, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 5 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0x1A90, .y = -0x9C4, .z = 0x258, .yaw = -0x12C, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 5 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_BAT, .variant = 0, .mode = MAP_DRYFIELD_FULL_BAT_PERCH_DROP, .x = 0x1C52, .y = -0x9C4, .z = 0x258, .yaw = -0x190, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 5 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullUnderpassVariant1Placements[2] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_GRAY_STALKER, .variant = 0, .mode = 0, .x = 0x4074, .y = -0xFA0, .z = -0x209E, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullUnderpassVariant2Placements[6] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = -0x578, .y = -0x3E8, .z = -0x1C84, .yaw = 0xD48, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x1F40, .y = -0x3E8, .z = -0x1C84, .yaw = 0xFA0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x32C8, .y = -0x3E8, .z = -0x25E4, .yaw = 0x7D0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x3BC4, .y = -0x3E8, .z = -0x2BC, .yaw = 0xED8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER, .variant = 0, .mode = MAP_DRYFIELD_FULL_SCAVENGER_ACTIVE, .x = 0x4524, .y = -0x3E8, .z = -0x11F8, .yaw = 0x514, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullUnderpassVariant3Placements[4] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOSSBACK, .variant = MAP_DRYFIELD_FULL_MOSSBACK_MOVEMENT_AWARENESS, .mode = 0, .x = 0x43F8, .y = -0x3E8, .z = -0x1676, .yaw = 0xA28, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOSSBACK, .variant = MAP_DRYFIELD_FULL_MOSSBACK_MOVEMENT_AWARENESS, .mode = 0, .x = 0x251C, .y = -0x3E8, .z = -0x206C, .yaw = 0xA28, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MOSSBACK, .variant = MAP_DRYFIELD_FULL_MOSSBACK_MOVEMENT_AWARENESS, .mode = 0, .x = 0xBB8, .y = -0x3E8, .z = -0x2422, .yaw = 0x320, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullUnderpassVariant7Placements[2] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_KNIGHT_GOLEM, .variant = 0, .mode = 0, .x = 0, .y = -0x3E8, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldFullUnderpassVariant11Placements[8] = {
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x34BC, .y = -0xFA0, .z = -0x251C, .yaw = 0x44C, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x3264, .y = -0xFA0, .z = -0x2198, .yaw = 0x384, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x319C, .y = -0xFA0, .z = -0x1C84, .yaw = 0x640, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x2D50, .y = -0xFA0, .z = -0x2454, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x2904, .y = -0xFA0, .z = -0x1FA4, .yaw = 0xB54, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x2774, .y = -0xFA0, .z = -0x238C, .yaw = 0xC80, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER, .variant = 0, .mode = MAP_DRYFIELD_FULL_MIND_SUCKLER_CEILING_NO_WALL_REPERCH, .x = 0x22C4, .y = -0xFA0, .z = -0x20D0, .yaw = 0xBB8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

InventoryBattleReward gMapDryfieldFullBattleRewards[23] = {
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, 1, 0), .items = { MAP_DRYFIELD_FULL_REWARD_RECOVERY_2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_MP_BOOST_1 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 3, 0), .items = { MAP_DRYFIELD_FULL_REWARD_9MM_HYDRA, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3, 2, 0), .items = { MAP_DRYFIELD_FULL_REWARD_9MM_HYDRA, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT, 3, 0), .items = { MAP_DRYFIELD_FULL_REWARD_COMBAT_LIGHT, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_FLARE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT, 4, 0), .items = { MAP_DRYFIELD_FULL_REWARD_FLARE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TOILET, 1, 0), .items = { MAP_DRYFIELD_FULL_REWARD_RECOVERY_2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R, 2, 0), .items = { MAP_DRYFIELD_FULL_REWARD_MP_BOOST_2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY, 3, 0), .items = { MAP_DRYFIELD_FULL_REWARD_RECOVERY_2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 1, 0), .items = { MAP_DRYFIELD_FULL_REWARD_FIREFLY, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_RIFLED_SLUG } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 2, 0), .items = { MAP_DRYFIELD_FULL_REWARD_FLARE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5, 2, 0), .items = { MAP_DRYFIELD_FULL_REWARD_RIOT, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_GRENADE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY, 2, 0), .items = { MAP_DRYFIELD_FULL_REWARD_PROTEIN_CAPSULE, MAP_DRYFIELD_FULL_REWARD_AIRBURST, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_PYKE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY, 4, 0), .items = { MAP_DRYFIELD_FULL_REWARD_RECOVERY_2, MAP_DRYFIELD_FULL_REWARD_MP_BOOST_1, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT, 1, 0), .items = { MAP_DRYFIELD_FULL_REWARD_RECOVERY_3, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_MP_BOOST_2 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT, 4, 0), .items = { MAP_DRYFIELD_FULL_REWARD_FIREFLY, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_RIFLED_SLUG } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 1, 0), .items = { MAP_DRYFIELD_FULL_REWARD_FIREFLY, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_RIFLED_SLUG } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 2, 0), .items = { MAP_DRYFIELD_FULL_REWARD_9MM_HYDRA, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 1, 0), .items = { MAP_DRYFIELD_FULL_REWARD_PROTEIN_CAPSULE, MAP_DRYFIELD_FULL_REWARD_RIFLED_SLUG, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_FIREFLY } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 3, 0), .items = { MAP_DRYFIELD_FULL_REWARD_RECOVERY_2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_MP_BOOST_2 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER, 11, 0), .items = { MAP_DRYFIELD_FULL_REWARD_BELT_POUCH, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_EAU_DE_TOILETTE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TANK, 10, 0), .items = { MAP_DRYFIELD_FULL_REWARD_9MM_HYDRA, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY, 21, 0), .items = { MAP_DRYFIELD_FULL_REWARD_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = INVENTORY_BATTLE_REWARD_LIST_END },
};

InventoryBattleReward gMapDryfieldFullHardBattleRewards[9] = {
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_GAS_STATION, 7, 0), .items = { MAP_DRYFIELD_FULL_REWARD_PROTEIN_CAPSULE, MAP_DRYFIELD_FULL_REWARD_AIRBURST, MAP_DRYFIELD_FULL_REWARD_MP_BOOST_2, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, 7, 0), .items = { MAP_DRYFIELD_FULL_REWARD_FIREFLY, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_BACK_STREET, 7, 0), .items = { MAP_DRYFIELD_FULL_REWARD_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT, 7, 0), .items = { MAP_DRYFIELD_FULL_REWARD_LIPSTICK, MAP_DRYFIELD_FULL_REWARD_RECOVERY_2, MAP_DRYFIELD_FULL_REWARD_BELT_POUCH, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, 7, 0), .items = { MAP_DRYFIELD_FULL_REWARD_PROTEIN_CAPSULE, MAP_DRYFIELD_FULL_REWARD_MP_BOOST_2, MAP_DRYFIELD_FULL_REWARD_FIREFLY, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY, 2, 0), .items = { MAP_DRYFIELD_FULL_REWARD_RINGERS_SOLUTION, MAP_DRYFIELD_FULL_REWARD_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 1, 0), .items = { MAP_DRYFIELD_FULL_REWARD_RIFLED_SLUG, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_FULL_REWARD_FIREFLY } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 7, 0), .items = { MAP_DRYFIELD_FULL_REWARD_HUNTER_GOGGLES, MAP_DRYFIELD_FULL_REWARD_RECOVERY_2, MAP_DRYFIELD_FULL_REWARD_FLARE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = INVENTORY_BATTLE_REWARD_LIST_END },
};

StageMusicEntry gMapDryfieldFullAreaMusicEntries[(MAP_DRYFIELD_FULL_AREA_ID_LIMIT + 1) * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT + 1] = {
    [(0 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
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

    [(GAME_AREA_DRYFIELD_NIGHT_GAS_STATION * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_AMBIENT_AREA, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_AMBIENT_AREA, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2A, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(4 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_BACK_STREET * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_AMBIENT_AREA, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_R08 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(10 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_AMBIENT_AREA, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_TOILET * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_AMBIENT_AREA, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_WATER_TANK * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_FACTORY * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_GARAGE * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3B, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3B, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_AMBIENT_AREA, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2D, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x2D, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x2D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2D, .startMode = STAGE_MUSIC_START_NEVER },

    [(GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_AMBIENT_AREA, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x38, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(33 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_CELLAR * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(35 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(36 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(37 * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(GAME_AREA_DRYFIELD_NIGHT_UNDERPASS * MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT) + 0] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x39, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x37, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x3A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [(MAP_DRYFIELD_FULL_AREA_ID_LIMIT + 1) *
        MAP_DRYFIELD_FULL_MUSIC_COLUMN_COUNT] = { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
};

StageMusicEntry gMapDryfieldFullCountdownMusicEntries[20] = {
    [0]  = { .sequenceId = 0x30, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    [1]  = { .sequenceId = 0x17, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    [2]  = { .sequenceId = 0x30, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    [3]  = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    [4]  = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    [5]  = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    [6]  = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    [7]  = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    [8]  = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    [9]  = { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    [10] = { .sequenceId = 0x45, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    [11] = { .sequenceId = 0x46, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    [12] = { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    [13] = { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    [14] = { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    [15] = { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    [16] = { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    [17] = { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    [18] = { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    [19] = { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
};
