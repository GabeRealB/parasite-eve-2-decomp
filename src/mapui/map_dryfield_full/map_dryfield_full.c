#include "mapui/map_dryfield_full.h"

#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/battle_reward.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/item_menu.h"
#include "gameplay/item_placement.h"
#include "gameplay/items.h"
#include "gameplay/map.h"
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
    MAP_DRYFIELD_FULL_RESOURCE_ZEBRA_STALKER         = 6,
    MAP_DRYFIELD_FULL_RESOURCE_RAT                   = 7,
    MAP_DRYFIELD_FULL_RESOURCE_MOTH                  = 8,
    MAP_DRYFIELD_FULL_RESOURCE_MIND_SUCKLER          = 15,
    MAP_DRYFIELD_FULL_RESOURCE_SCAVENGER             = 16,
    MAP_DRYFIELD_FULL_RESOURCE_ROOK_BEAM_SWORD       = 23,
    MAP_DRYFIELD_FULL_RESOURCE_SCORPION              = 25,
    MAP_DRYFIELD_FULL_RESOURCE_BAT                   = 37,
    MAP_DRYFIELD_FULL_RESOURCE_BLOOD_SUCKLER         = 40,
    MAP_DRYFIELD_FULL_RESOURCE_ROOK_GRENADE_LAUNCHER = 57,
    MAP_DRYFIELD_FULL_RESOURCE_GARY_DOUGLAS          = 106,
};

/// Actor-specific placement modes used by these night-stage layouts.
enum {
    MAP_DRYFIELD_FULL_SCAVENGER_SCRIPTED                = 3,
    MAP_DRYFIELD_FULL_DESERT_CHASER_TUNING_ROW_2        = 1,
    MAP_DRYFIELD_FULL_MIND_SUCKLER_WALL_NO_WALL_REPERCH = 2,
    MAP_DRYFIELD_FULL_SCORPION_ROAM                     = 0,
    MAP_DRYFIELD_FULL_SCORPION_AMBUSH_FOLLOWER          = 2,
    MAP_DRYFIELD_FULL_ROOK_PATROL                       = 1,
    MAP_DRYFIELD_FULL_BAT_PERCH_BACK                    = 2,
    MAP_DRYFIELD_FULL_ZEBRA_STALKER_NO_CEILING          = 1,
    MAP_DRYFIELD_FULL_BLOOD_SUCKLER_IDLE                = 1,
};

s32 mapDryfieldFullResolveRoomVariant(const RoomEventMsg* request, RoomEventMsg* reply)
{
    if ((request->areaId == GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
        reply->room = gameFlagGetNibble(GAME_FLAG_NIGHT_JUNK_YARD_EVENT_SEEN) + 1;
    }
    return 1;
}

GfxImageSlot gMapDryfieldFullImageSlots[MAP_DRYFIELD_FULL_AREA_ID_LIMIT + 1] = {
    [0]                                          = GFX_IMAGE_SLOT(0x3A8E0),
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION]       = GFX_IMAGE_SLOT(0x41C20),
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET]       = GFX_IMAGE_SLOT(0x4A180),
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE]     = GFX_IMAGE_SLOT(0x4DB70),
    [4]                                          = { NULL, 0 },
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET]       = GFX_IMAGE_SLOT(0x54AE0),
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP]     = GFX_IMAGE_SLOT(0x56830),
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE]         = GFX_IMAGE_SLOT(0x56250),
    [GAME_AREA_DRYFIELD_NIGHT_R08]               = GFX_IMAGE_SLOT(0x54DA0),
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE] = GFX_IMAGE_SLOT(0x4A0B0),
    [10]                                         = { NULL, 0 },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1]      = GFX_IMAGE_SLOT(0x553D0),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2]      = GFX_IMAGE_SLOT(0x55240),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3]      = GFX_IMAGE_SLOT(0x54A60),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4]      = GFX_IMAGE_SLOT(0x55300),
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT]       = GFX_IMAGE_SLOT(0x53FA0),
    [GAME_AREA_DRYFIELD_NIGHT_TOILET]            = GFX_IMAGE_SLOT(0x566C0),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY]       = GFX_IMAGE_SLOT(0x4CB50),
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R]        = GFX_IMAGE_SLOT(0x4A7F0),
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN]       = GFX_IMAGE_SLOT(0x57960),
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER]       = GFX_IMAGE_SLOT(0x52820),
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK]        = GFX_IMAGE_SLOT(0x54720),
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY]         = GFX_IMAGE_SLOT(0x554B0),
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY]           = GFX_IMAGE_SLOT(0x495E0),
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE]            = GFX_IMAGE_SLOT(0x4B2D0),
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY]          = GFX_IMAGE_SLOT(0x53740),
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD]         = GFX_IMAGE_SLOT(0x50350),
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH]     = GFX_IMAGE_SLOT(0x44320),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5]      = GFX_IMAGE_SLOT(0x53F20),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY]     = GFX_IMAGE_SLOT(0x3F550),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6]      = GFX_IMAGE_SLOT(0x4A170),
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT]        = GFX_IMAGE_SLOT(0x53970),
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE]        = GFX_IMAGE_SLOT(0x52180),
    [33]                                         = { NULL, 0 },
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR]            = GFX_IMAGE_SLOT(0x54570),
    [35]                                         = { NULL, 0 },
    [36]                                         = { NULL, 0 },
    [37]                                         = { NULL, 0 },
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS]         = GFX_IMAGE_SLOT(0x55610),
};

u8 D_map_dryfield_full_80179ADC[4] = { 0, 0x16, 0x16, 0x16 };

MenuMapArea D_map_dryfield_full_80179AE0[40] = {
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0x126, -0xD9A, 4, 0x2F, 0x12C, 0x12C, 2 },
    { -0x1C2, -0xD6B, 0xFFFD, 0x2E, 0x12C, 0x12C, 2 },
    { 0xC1C, 0x12C, 0xC, 0x23, 0x15E, 0x122, 2 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { -0x257D, 0x123B, 0x27, 0x12, 0x113, 0x12C, 2 },
    { 0x7D0, -0xD6D, 0x2B, 0xA, 0x12C, 0x12C, 2 },
    { 0xA27, -0xDCF, 0x46, 0xA, 0x12C, 0x12C, 2 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0x63C, -0x9EC, 0x6A, 0xB, 0x12C, 0x12C, 2 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0x10D3, 0xD3C, 0xFFE2, 0x2D, 0x12C, 0x12C, 2 },
    { 0x1194, 0xA32, 0xFFE2, 0x10, 0x12C, 0x12C, 2 },
    { 0x116E, 0x42B, 0xFFE2, 0xFFFF, 0x12C, 0x12C, 2 },
    { 0xD48, 0x2A3, 0xFFF2, 0xFFF8, 0x12C, 0x12C, 2 },
    { 0x2995, 0xC05, 0xFFE2, 0x1D, 0x12C, 0x12C, 2 },
    { -0x67C, 0x675, 0xFFBA, 0x2C, 0x12C, 0x12C, 2 },
    { 0xB7C, 0x13C, 0xFFC4, 0x14, 0x12C, 0x12C, 2 },
    { 0x952, -0x1235, 0xFFA1, 0x23, 0x12C, 0x136, 2 },
    { -0x4B0, 0x881, 0xFFAF, 5, 0x12C, 0x12C, 2 },
    { -0x985, -0x161E, 0xFFB2, 0xFFFE, 0x12C, 0x12C, 2 },
    { -0x917, 0x477, 0xFFDC, 0xFFDF, 0x12C, 0x12C, 1 },
    { 0x1A00, 0x620, 0xFFD6, 0xFFE6, 0x12C, 0x12C, 2 },
    { 0x143A, 0x5AE, 0x10, 0xFFFD, 0x12C, 0x12C, 2 },
    { 0x355, 0xAB5, 0x1A, 0xFFE9, 0x12C, 0x12C, 2 },
    { -0xD48, 0x258, 0x18, 0xFFFE, 0x12C, 0x12C, 2 },
    { 0x13A6, 0x186, 0x1E, 0xFFD5, 0x12C, 0x12C, 2 },
    { 0x62B, -0x320, 0x50, 0xFFD8, 0x190, 0x12C, 2 },
    { 0x10FE, 0x97A, 0xA, 0x1D, 0x12C, 0x12C, 1 },
    { 0x315E, 0x5C6, 0x4D, 0x11, 0x12C, 0x12C, 1 },
    { 0x10FE, 0x5DC, 8, 6, 0x12C, 0x12C, 1 },
    { 0x1082, -0x764, 0x1F, 0xFFEA, 0x12C, 0x12C, 1 },
    { 0x1CEE, -0x5AA, 0xFFE4, 0xFFE9, 0x12C, 0x12C, 3 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0x15EB, 0xF69, 0xFFF8, 0xC, 0x12C, 0x12C, 3 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0x14B7, 0x2C94, 0xFFF9, 0xFFB6, 0x12C, 0x10E, 3 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
};

MenuMapAreaShape D_map_dryfield_full_80179D10[39] = {
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS302GasStationShape, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS302MainStreetShape, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS302GeneralStoreShape, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F188, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F218, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F2A8, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F36C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F3FC, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F48C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F51C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F5AC, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F6AC, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F73C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F7CC, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F8AC, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F93C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F9E8, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_00_8012EFF4, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012FA94, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012FB24, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012FBB4, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012FC7C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012FD78, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012FE08, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS300MotelRoom5Shape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS300MotelBalconyShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS300MotelRoom6Shape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS300MotelLoftShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS303WaterHoleShape, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS303CellarShape, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_03_8012F180, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
};

MenuMapMarker D_map_dryfield_full_80179E48[31] = {
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

MenuMapIcon D_map_dryfield_full_80179F04[9] = {
    { 2, 1, 0, 0, 53, 54 },
    { 2, 0x11, MENU_MAP_ICON_KIND_TELEPHONE, 0, -51, 20 },
    { 2, 0x18, 0, GAME_FLAG_097, 37, -17 },
    { 2, 0x1B, MENU_MAP_ICON_KIND_TELEPHONE, 0, 79, -38 },
    { 1, 0x1E, 0, 0, 0, 7 },
    { 1, 0x1E, MENU_MAP_ICON_KIND_TELEPHONE, 0, 0, -5 },
    { 2, 1, MENU_MAP_ICON_KIND_OBJECTIVE, 0x11, 31, 46 },
    { 2, 0x11, MENU_MAP_ICON_KIND_OBJECTIVE, 0x13, -60, 13 },
    { 0, 0, 0, 0, 0, 0 },
};

MenuMapAreaName D_map_dryfield_full_80179F4C[38] = {
    { "Gas station" },
    { "Main street" },
    { "General store" },
    { "" },
    { "Back street" },
    { "Souvenir shop" },
    { "Warehouse" },
    { "" },
    { "Dilapidated house" },
    { "" },
    { "Motel room 1" },
    { "Motel room 2" },
    { "Motel room 3" },
    { "Motel room 4" },
    { "Parking lot" },
    { "Toilet" },
    { "Motel lobby" },
    { "Saloon G & R" },
    { "G & R kitchen" },
    { "Water tower" },
    { "Water tank" },
    { "Breezeway" },
    { "Factory" },
    { "Garage" },
    { "Driveway" },
    { "Junk yard" },
    { "Trailer coach" },
    { "Motel room 5" },
    { "Motel balcony" },
    { "Motel room 6" },
    { "Motel loft" },
    { "Water hole" },
    { "" },
    { "Cellar" },
    { "" },
    { "" },
    { "" },
    { "Underpass" },
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

TaskDesc D_map_dryfield_full_8017A5AC[] = {
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightGasStationRoomTask, { .value = GP_TASK_LOC_KEY(3, 1, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMainStreetRoomTask, { .value = GP_TASK_LOC_KEY(3, 2, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightGeneralStoreRoomTask, { .value = GP_TASK_LOC_KEY(3, 3, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightBackStreetRoomTask, { .value = GP_TASK_LOC_KEY(3, 5, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightSouvenirShopRoomTask, { .value = GP_TASK_LOC_KEY(3, 6, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightWarehouseRoomTask, { .value = GP_TASK_LOC_KEY(3, 7, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightDilapidatedHouseRoomTask, { .value = GP_TASK_LOC_KEY(3, 9, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelRoom1RoomTask, { .value = GP_TASK_LOC_KEY(3, 11, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelRoom2RoomTask, { .value = GP_TASK_LOC_KEY(3, 12, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelRoom3RoomTask, { .value = GP_TASK_LOC_KEY(3, 13, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelRoom4RoomTask, { .value = GP_TASK_LOC_KEY(3, 14, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightParkingLotRoomTask, { .value = GP_TASK_LOC_KEY(3, 15, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightToiletRoomTask, { .value = GP_TASK_LOC_KEY(3, 16, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelLobbyRoomTask, { .value = GP_TASK_LOC_KEY(3, 17, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightSaloonGRRoomTask, { .value = GP_TASK_LOC_KEY(3, 18, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightGRKitchenRoomTask, { .value = GP_TASK_LOC_KEY(3, 19, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightWaterTowerRoomTask, { .value = GP_TASK_LOC_KEY(3, 20, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightWaterTankRoomTask, { .value = GP_TASK_LOC_KEY(3, 21, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightBreezewayRoomTask, { .value = GP_TASK_LOC_KEY(3, 22, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightFactoryEntryTask, { .value = GP_TASK_LOC_KEY(3, 23, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightGarageRoomTask, { .value = GP_TASK_LOC_KEY(3, 24, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightDrivewayRoomTask, { .value = GP_TASK_LOC_KEY(3, 25, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightJunkYardRoomTask, { .value = GP_TASK_LOC_KEY(3, 26, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightTrailerCoachRoomTask, { .value = GP_TASK_LOC_KEY(3, 27, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelRoom5RoomTask, { .value = GP_TASK_LOC_KEY(3, 28, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelBalconyRoomTask, { .value = GP_TASK_LOC_KEY(3, 29, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelRoom6RoomTask, { .value = GP_TASK_LOC_KEY(3, 30, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelLoftRoomTask, { .value = GP_TASK_LOC_KEY(3, 31, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightWaterHoleRoomTask, { .value = GP_TASK_LOC_KEY(3, 32, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightCellarRoomTask, { .value = GP_TASK_LOC_KEY(3, 34, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightUnderpassRoomTask, { .value = GP_TASK_LOC_KEY(3, 38, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightR08RoomTask, { .value = GP_TASK_LOC_KEY(3, 8, 0) } },
    { { { TASK_DESC_END, 0x20 } }, NULL, { 0 } },
};

u16 D_map_dryfield_full_8017A738[30] = {
    0x1E9,
    0x1E8,
    0x1E7,
    0x1E6,
    0x1E5,
    0x1E4,
    0x1E3,
    0x9E2,
    0x1E1,
    0x1E0,
    0x1DF,
    0x1DE,
    0x1DD,
    0x1DC,
    0x1DB,
    0x1DA,
    0x1D9,
    0x1D8,
    0x1D7,
    0x1D6,
    0x1D5,
    0x1D4,
    0x1D3,
    0x1D2,
    0x1D1,
    0x1D0,
    0x1CF,
    0x1CE,
    0x1BD,
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

DirectionWarpEntry* D_map_dryfield_full_8017A80C[38] = {
    gDryfieldNightGasStationWarpEntries,
    D_dryfield_night_main_street_80182310,
    gDryfieldNightGeneralStoreWarpEntries,
    NULL,
    D_dryfield_night_back_street_801803CC,
    D_dryfield_night_souvenir_shop_8017E104,
    D_dryfield_night_warehouse_8017E944,
    D_dryfield_night_r08_8018069C,
    gDryfieldNightDilapidatedHouseWarpEntries,
    NULL,
    D_dryfield_night_motel_room_1_8017DAA8,
    gDryfieldNightMotelRoom2WarpEntries,
    D_dryfield_night_motel_room_3_8017DABC,
    D_dryfield_night_motel_room_4_8017DAB0,
    gDryfieldNightParkingLotWarpEntries,
    D_dryfield_night_toilet_8017DAD0,
    gDryfieldNightMotelLobbyWarpEntries,
    gDryfieldNightSaloonGRWarpEntries,
    gDryfieldNightGRKitchenWarpEntries,
    gDryfieldNightWaterTowerWarpEntries,
    gDryfieldNightWaterTankWarpEntries,
    D_dryfield_night_breezeway_8017E704,
    D_dryfield_night_factory_80186F58,
    gDryfieldNightGarageWarpEntries,
    gDryfieldNightDrivewayWarpEntries,
    D_dryfield_night_junk_yard_801807CC,
    D_dryfield_night_trailer_coach_80189520,
    gDryfieldNightMotelRoom5WarpEntries,
    gDryfieldNightMotelBalconyWarpEntries,
    D_dryfield_night_motel_room_6_80182F20,
    gDryfieldNightMotelLoftWarpEntries,
    gDryfieldNightWaterHoleWarpEntries,
    NULL,
    D_dryfield_night_cellar_8017DB34,
    NULL,
    NULL,
    NULL,
    D_dryfield_night_underpass_8017DE60,
};

static ViewCount* D_map_dryfield_full_8017A8A4[38] = {
    gDryfieldNightGasStationViewCounts,
    D_dryfield_night_main_street_80182308,
    gDryfieldNightGeneralStoreViewCounts,
    NULL,
    gDryfieldNightBackStreetRoomViewCounts,
    gDryfieldNightSouvenirShopViewCounts,
    gDryfieldNightWarehouseRoomViewCounts,
    gDryfieldNightR08ViewCounts,
    gDryfieldNightDilapidatedHouseViewCounts,
    NULL,
    D_dryfield_night_motel_room_1_8017DAA4,
    gDryfieldNightMotelRoom2RoomViewCounts,
    D_dryfield_night_motel_room_3_8017DAB8,
    D_dryfield_night_motel_room_4_8017DAAC,
    gDryfieldNightParkingLotViewCounts,
    gDryfieldNightToiletViewCounts,
    gDryfieldNightMotelLobbyViewCounts,
    gDryfieldNightSaloonGRViewCounts,
    gDryfieldNightGRKitchenViewCounts,
    gDryfieldNightWaterTowerViewCounts,
    gDryfieldNightWaterTankViewCounts,
    gDryfieldNightBreezewayViewCounts,
    D_dryfield_night_factory_80186F54,
    gDryfieldNightGarageViewCounts,
    gDryfieldNightDrivewayViewCounts,
    D_dryfield_night_junk_yard_801807C8,
    D_dryfield_night_trailer_coach_8018951C,
    gDryfieldNightMotelRoom5RoomViewCounts,
    gDryfieldNightMotelBalconyRoomViewCounts,
    D_dryfield_night_motel_room_6_80182F1C,
    gDryfieldNightMotelLoftViewCounts,
    gDryfieldNightWaterHoleViewCounts,
    NULL,
    D_dryfield_night_cellar_8017DB30,
    NULL,
    NULL,
    NULL,
    D_dryfield_night_underpass_8017DE54,
};

ViewCountTable D_map_dryfield_full_8017A93C = { D_map_dryfield_full_8017A8A4 };

static WorldCollisionRoomResources* D_map_dryfield_full_8017A940[38] = {
    gDryfieldNightGasStationRoomCollisionResources,
    D_dryfield_night_main_street_80182284,
    gDryfieldNightGeneralStoreRoomCollisionResources,
    NULL,
    D_dryfield_night_back_street_801803AC,
    D_dryfield_night_souvenir_shop_8017E0EC,
    D_dryfield_night_warehouse_8017E900,
    gDryfieldNightR08RoomCollisionResources,
    D_dryfield_night_dilapidated_house_80187394,
    NULL,
    D_dryfield_night_motel_room_1_8017DA74,
    gDryfieldNightMotelRoom2RoomCollisionResources,
    D_dryfield_night_motel_room_3_8017DAA4,
    D_dryfield_night_motel_room_4_8017DA98,
    gDryfieldNightParkingLotRoomCollisionResources,
    D_dryfield_night_toilet_8017DAB8,
    gDryfieldNightMotelLobbyRoomCollisionResources,
    gDryfieldNightSaloonGRRoomCollisionResources,
    gDryfieldNightGRKitchenRoomCollisionResources,
    gDryfieldNightWaterTowerCollisionRooms,
    gDryfieldNightWaterTankCollisionRooms,
    D_dryfield_night_breezeway_8017E6EC,
    D_dryfield_night_factory_80186F34,
    gDryfieldNightGarageRoomCollisionResources,
    gDryfieldNightDrivewayRoomCollisionResources,
    D_dryfield_night_junk_yard_80180794,
    D_dryfield_night_trailer_coach_80189508,
    gDryfieldNightMotelRoom5RoomCollisionResources,
    gDryfieldNightMotelBalconyRoomCollisionResources,
    D_dryfield_night_motel_room_6_80182F08,
    D_dryfield_night_motel_loft_8017EDC0,
    gDryfieldNightWaterHoleRoomCollisionResources,
    NULL,
    D_dryfield_night_cellar_8017DB00,
    NULL,
    NULL,
    NULL,
    D_dryfield_night_underpass_8017DD70,
};

WorldCollisionStageResources D_map_dryfield_full_8017A9D8 = { D_map_dryfield_full_8017A940 };

static ViewCamera* D_map_dryfield_full_8017A9DC[38] = {
    gDryfieldNightGasStationViewCameras,
    D_dryfield_night_main_street_80184564,
    gDryfieldNightGeneralStoreViewCameras,
    NULL,
    gDryfieldNightBackStreetViewCameras,
    gDryfieldNightSouvenirShopViewCameras,
    gDryfieldNightWarehouseViewCameras,
    gDryfieldNightR08ViewCameras,
    gDryfieldNightDilapidatedHouseViewCameras,
    NULL,
    D_dryfield_night_motel_room_1_8017E0BC,
    gDryfieldNightMotelRoom2ViewCameras,
    D_dryfield_night_motel_room_3_8017E1A4,
    D_dryfield_night_motel_room_4_8017E1F4,
    gDryfieldNightParkingLotViewCameras,
    gDryfieldNightToiletViewCameras,
    gDryfieldNightMotelLobbyViewCameras,
    gDryfieldNightSaloonGRViewCameras,
    gDryfieldNightGRKitchenViewCameras,
    gDryfieldNightWaterTowerViewCameras,
    gDryfieldNightWaterTankViewCameras,
    gDryfieldNightBreezewayViewCameras,
    D_dryfield_night_factory_80187C14,
    gDryfieldNightGarageViewCameras,
    gDryfieldNightDrivewayViewCameras,
    D_dryfield_night_junk_yard_801811DC,
    D_dryfield_night_trailer_coach_80189A44,
    gDryfieldNightMotelRoom5ViewCameras,
    gDryfieldNightMotelBalconyViewCameras,
    D_dryfield_night_motel_room_6_801839A8,
    gDryfieldNightMotelLoftViewCameras,
    gDryfieldNightWaterHoleViewCameras,
    NULL,
    D_dryfield_night_cellar_8017DE84,
    NULL,
    NULL,
    NULL,
    gDryfieldNightUnderpassViewCameras,
};

ViewCameraTable D_map_dryfield_full_8017AA74 = { D_map_dryfield_full_8017A9DC };

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

static SpriteView* D_map_dryfield_full_8017AB14[38] = {
    gDryfieldNightGasStationSpriteViews,
    D_dryfield_night_main_street_801875E4,
    gDryfieldNightGeneralStoreSpriteViews,
    NULL,
    gDryfieldNightBackStreetSpriteViews,
    gDryfieldNightSouvenirShopSpriteViews,
    gDryfieldNightWarehouseSpriteViews,
    gDryfieldNightR08SpriteViews,
    gDryfieldNightDilapidatedHouseSpriteViews,
    NULL,
    gDryfieldNightMotelRoom1SpriteViews,
    gDryfieldNightMotelRoom2SpriteViews,
    gDryfieldNightMotelRoom3SpriteViews,
    gDryfieldNightMotelRoom4SpriteViews,
    gDryfieldNightParkingLotSpriteViews,
    gDryfieldNightToiletSpriteViews,
    gDryfieldNightMotelLobbySpriteViews,
    gDryfieldNightSaloonGRSpriteViews,
    gDryfieldNightGRKitchenSpriteViews,
    gDryfieldNightWaterTowerSpriteViews,
    gDryfieldNightWaterTankSpriteViews,
    gDryfieldNightBreezewaySpriteViews,
    gDryfieldNightFactorySpriteViews,
    gDryfieldNightGarageSpriteViews,
    gDryfieldNightDrivewaySpriteViews,
    gDryfieldNightJunkYardSpriteViews,
    gDryfieldNightTrailerCoachSpriteViews,
    gDryfieldNightMotelRoom5SpriteViews,
    gDryfieldNightMotelBalconySpriteViews,
    D_dryfield_night_motel_room_6_801857C0,
    gDryfieldNightMotelLoftSpriteViews,
    gDryfieldNightWaterHoleSpriteViews,
    NULL,
    gDryfieldNightCellarSpriteViews,
    NULL,
    NULL,
    NULL,
    gDryfieldNightUnderpassSpriteViews,
};

SpriteAreaTable D_map_dryfield_full_8017ABAC = { D_map_dryfield_full_8017AB14 };

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

AreaPlacement D_map_dryfield_full_8017B4A8[6] = {
    { 0x10, 0, 0, 0xAF0, 0, -0x578, 0x384, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0xB86, 0, -0xA8C, 0xDAC, 0, 0, 2, 0 },
    { 0x19, 0, 0, 0x190, 0, -0xBEA, 0x384, 0, 2, 4, 2 },
    { 0x19, 0, 0, 0x672, 0, -0x190, 0x7D0, 0, 2, 4, 2 },
    { 0x19, 0, 0, 0x1388, 0, -0xE10, 0xE42, 0, 2, 4, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B508[5] = {
    { 0x84, 0, 0, 0x2D, 0, -0x2710, 0xC00, 0, 0, 2, 0 },
    { 0x14, 0, 0, -0x3F, 0, -0x39A3, 0, 0, 2, 4, 0 },
    { 0x14, 0, 0, -0x898, 0, -0x3E8, 0xC00, 0, 2, 4, 0 },
    { 0x14, 0, 0, -0x97, 0, -0x9A7, 0x800, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B558[8] = {
    { 0x28, 0, 0, -0xC8, 0, -0x258, 0xED8, 0, 2, 4, 0 },
    { 0x28, 0, 0, -0x76C, 0, -0x5DC, 0x44C, 0, 2, 4, 0 },
    { 0x19, 0, 0, 0x9C4, 0, 0xA8C, 0xA5A, 0, 0, 2, 1 },
    { 0x19, 0, 0, 0xE74, 0, -0x76C, 0xCE4, 0, 0, 2, 1 },
    { 0x19, 0, 0, -0x9C4, 0, 0xA8C, 0x6A4, 0, 0, 2, 1 },
    { 0x19, 0, 0, 0x96, 0, 0xA8C, 0x898, 0, 0, 2, 1 },
    { 0x19, 0, 0, -0xBB8, 0, 0, 0x400, 0, 0, 2, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B5D8[7] = {
    { 0x28, 1, 0, 0x12C, 0, 0, 0x898, 0, 2, 4, 0 },
    { 0x28, 1, 0, -0x5DC, 0, -0xC8, 0x4B0, 0, 2, 4, 0 },
    { 0x10, 0, 0, 0x1F4, 0, -0x384, 0xC1C, 0, 0, 2, 0 },
    { 0x10, 0, 0, -0x258, 0, -0x12C, 0x76C, 0, 0, 2, 0 },
    { 0x10, 0, 0, -0x3E8, 0, -0x47E, 0x258, 0, 0, 2, 0 },
    { 0x10, 0, 0, -0xC8, 0, -0x578, 0x64, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B648[6] = {
    { 0x10, 0, 0, 0x258, 0, 0x898, 0x200, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x3E8, 0, 0x5DC, 0x400, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x708, 0, 0x44C, 0, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0xAF0, 0, 0x834, 0xA00, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0xBB8, 0, 0x44C, 0xE00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B6A8[6] = {
    { 0x28, 1, 0, 0x320, 1, 0xDAC, 0xDAC, 0, 2, 4, 0 },
    { 0x28, 1, 0, 0x708, 1, 0x4B0, 0x834, 0, 2, 4, 0 },
    { 0x10, 0, 0, 0x1194, 0, 0x15E0, 0x1F4, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0xFA0, 0, 0x3E8, 0x76C, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0xA28, 0, 0xFA0, 0xDAC, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B708[9] = {
    { 0x19, 0, 0, 0x1F4, 0, 0xD7A, 0x41A, 0, 0, 2, 1 },
    { 0x19, 0, 0, 0x6A4, 0, 0x4B0, 0, 0, 0, 2, 1 },
    { 0x19, 0, 0, 0xEA6, 0, 0x41A, 0xA28, 0, 0, 2, 1 },
    { 0x19, 0, 0, 0x3E8, 0, 0x157C, 0x400, 0, 0, 2, 1 },
    { 0x19, 0, 0, 0xCE4, 0, 0x14B4, 0x4B0, 0, 0, 2, 1 },
    { 8, 0, 0, 0x7D0, -0x640, 0x3E8, 0x800, 0, 2, 4, 3 },
    { 8, 0, 0, 0x6A4, -0x76C, 0x320, 0xC00, 0, 2, 4, 3 },
    { 8, 0, 0, 0x578, -0x6A4, 0x320, 0x400, 0, 2, 4, 3 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B798[4] = {
    { 0x10, 0, 0, 0x1F4, 0, 0x1388, 0xA28, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x1F4, 0, 0x17D4, 0xDAC, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x6A4, 0, 0x4B0, 0x960, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B7D8[8] = {
    { 0x28, 0, 0, 0x960, 0, 0x12C0, 0x800, 0, 2, 4, 0 },
    { 0x28, 0, 0, 0x384, 0, 0x1324, 0x800, 0, 2, 4, 0 },
    { 0x28, 0, 0, 0x672, 0, 0x47E, 0, 0, 2, 4, 0 },
    { 0x28, 0, 0, 0xDDE, 0, 0x3B6, 0x800, 0, 2, 4, 0 },
    { 0x19, 0, 0, 0x1194, 0, 0x1130, 0x546, 0, 0, 2, 1 },
    { 0x19, 0, 0, 0x1194, 0, 0x154A, 0x1F4, 0, 0, 2, 1 },
    { 0x19, 0, 0, 0xE10, 0, 0x13EC, 0xA5A, 0, 0, 2, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B858[7] = {
    { 0x28, 1, 0, 0xE10, 1, 0x41A, 0xA8C, 0, 0, 2, 0 },
    { 0x28, 1, 0, 0x9C4, 1, 0xAF0, 0x76C, 0, 0, 2, 0 },
    { 0x28, 1, 0, 0x1194, 1, 0x1518, 0x1F4, 0, 0, 2, 0 },
    { 0x28, 1, 0, 0xBB8, 1, 0x13EC, 0xF3C, 0, 0, 2, 0 },
    { 0x28, 1, 0, 0x226, 1, 0xBB8, 0x1F4, 0, 0, 2, 0 },
    { 0x28, 1, 0, 0x1F4, 1, 0x13BA, 0xDAC, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B8C8[7] = {
    { 0x10, 0, 0, 0x1F4, 0, 0x1388, 0xE10, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x1194, 0, 0x14B4, 0x258, 0, 0, 2, 0 },
    { 8, 0, 0, 0x898, -0x960, 0x6A4, 0x320, 0, 2, 4, 4 },
    { 8, 0, 0, 0x9C4, -0x8FC, 0x960, 0x960, 0, 2, 4, 4 },
    { 8, 0, 0, 0x7D0, -0x9C4, 0x8FC, 0x514, 0, 2, 4, 4 },
    { 8, 0, 0, 0x960, -0x898, 0x76C, 0xE74, 0, 2, 4, 4 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B938[6] = {
    { 0x28, 0, 0, 0xFA0, 0, 0xC80, 0x64, 0, 2, 4, 0 },
    { 0x28, 0, 0, 0x7D0, 0, 0x4B0, 0x4B0, 0, 2, 4, 0 },
    { 0x28, 1, 0, 0x3E8, 0, 0xC80, 0xE10, 0, 2, 4, 0 },
    { 0x28, 1, 0, 0xDAC, 0, 0x1068, 0x578, 0, 2, 4, 0 },
    { 0x12, 0x10, 0, 0xE74, 1, 0x1964, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B998[7] = {
    { 0x28, 0, 0, 0xD7A, 0, 0xCB2, 0x76C, 0, 2, 4, 0 },
    { 0x28, 0, 0, 0x898, 0, 0xC4E, 0x41A, 0, 2, 4, 0 },
    { 0x28, 0, 0, 0x7D0, 0, 0x320, 0x384, 0, 2, 4, 0 },
    { 0x10, 0x80, 0, 0x1194, 0, 0x1964, 0x190, 0, 0, 2, 0 },
    { 0x10, 0x80, 0, 0xEA6, 0, 0x1964, 0xE10, 0, 0, 2, 0 },
    { 0x10, 0x80, 0, 0xFD2, 0, 0x189C, 0xC8, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BA08[3] = {
    { 0x12, 0x10, 0, 0x1770, 1, 0x834, 0xA8C, 0, 0, 2, 0 },
    { 0x12, 0x10, 0, 0x76C, 1, 0xCE4, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BA38[8] = {
    { 0x28, 0, 0, 0x16A8, 1, 0x898, 0xA8C, 0, 2, 4, 0 },
    { 0x28, 0, 0, 0x157C, 1, 0x384, 0xB54, 0, 2, 4, 0 },
    { 0x28, 0, 0, 0xC80, 1, 0xDAC, 0x800, 0, 2, 4, 0 },
    { 0x28, 0, 0, 0xABE, 1, 0xFA0, 0x76C, 0, 2, 4, 0 },
    { 8, 0, 0, 0x92E, -0x898, 0xAF0, 0x800, 0, 0, 2, 3 },
    { 8, 0, 0, 0x9C4, -0x960, 0xA28, 0xC00, 0, 0, 2, 3 },
    { 8, 0, 0, 0x898, -0x7D0, 0xA28, 0x400, 0, 0, 2, 3 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BAB8[7] = {
    { 1, 0, 1, 0xBB8, 0, 0x258, 0xDAC, 0, 0, 2, 0 },
    { 1, 0, 1, -0xDAC, 0, -0x1068, 0, 0, 0, 2, 0 },
    { 8, 0, 0, 0x1068, -0x708, 0xDAC, 0xED8, 0, 2, 4, 2 },
    { 8, 0, 0, 0xFA0, -0x640, 0xD48, 0xC8, 0, 2, 4, 2 },
    { 8, 0, 0, -0xB54, -0x960, 0x578, 0x960, 0, 2, 4, 2 },
    { 8, 0, 0, -0xAF0, -0x898, 0x320, 0xF3C, 0, 2, 4, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BB28[5] = {
    { 0x10, 0, 1, -0x9C4, -0x1F4, 0xDAC, 0, 0, 0, 2, 0 },
    { 0x10, 0, 1, 0xE10, 0, 0x2EE, 0, 0, 0, 2, 0 },
    { 0x10, 0, 2, 0x708, -0x7D0, -0x1F4, 0, 0, 0, 2, 0 },
    { 0x10, 0, 1, 0x708, -0x7D0, 0xFA0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BB78[7] = {
    { 0x19, 0, 2, 0xC1C, 0, 0x4B0, -0xC8, 0, 2, 4, 1 },
    { 0x19, 0, 2, 0x64, 0, 0xBB8, 0x578, 0, 2, 4, 1 },
    { 0x19, 0, 0, 0x21FC, 0, 0xDAC, 0x44C, 0, 2, 4, 1 },
    { 0x19, 0, 0, 0x2260, 0, 0x960, 0xC4E, 0, 2, 4, 1 },
    { 0x19, 0, 0, 0x1E78, 0, 0xD16, 0xB22, 0, 2, 4, 1 },
    { 1, 0, 1, -0x898, 0, 0x898, 0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BBE8[2] = {
    { 0x27, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BC08[12] = {
    { 0x25, 0, 0, 0x320, -0x640, 0x960, 0x190, 0, 0, 2, 6 },
    { 0x25, 0, 0, 0x5DC, -0x708, 0x320, 0xC8, 0, 0, 2, 6 },
    { 0x25, 0, 0, -0x44C, -0x708, 0x834, -0xC8, 0, 0, 2, 6 },
    { 0x25, 0, 0, -0x76C, -0x578, 0x898, 0, 0, 0, 2, 6 },
    { 0x25, 0, 0, -0xA28, -0x640, 0x7D0, 0x64, 0, 0, 2, 6 },
    { 0x25, 0, 0, -0x3E8, -0x6A4, 0x6A4, 0x2BC, 0, 0, 2, 6 },
    { 0x25, 0, 0, -0x6A4, -0x514, 0x640, 0x320, 0, 0, 2, 6 },
    { 0x25, 0, 0, -0xA8C, -0x640, 0x5DC, 0x384, 0, 0, 2, 6 },
    { 0x25, 0, 0, -0x4B0, -0x578, 0x384, 0x578, 0, 0, 2, 6 },
    { 0x25, 0, 0, -0x708, -0x640, 0x320, 0x4B0, 0, 0, 2, 6 },
    { 0x25, 0, 0, -0x9C4, -0x708, 0x258, 0x5DC, 0, 0, 2, 6 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BCC8[7] = {
    { 0x28, 0, 0, 0x36, 0, -0x20B, 0xC00, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0x36, 0, -0x20B, 0, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0x36, 0, -0x20B, 0x100, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0x36, 0, -0x20B, -0x780, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0x36, 0, -0x20B, 0x400, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0x36, 0, -0x20B, 0x6A4, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BD38[12] = {
    { 7, 0, 0, 0x2BC, 0, 0x6A4, 0x2BC, 0, 0, 2, 3 },
    { 7, 0, 0, 0, 0, 0x6A4, 0x834, 0, 0, 2, 3 },
    { 7, 0, 0, 0, 0, 0x2BC, 0x578, 0, 0, 2, 3 },
    { 7, 0, 0, 0, 0, 0, 0x898, 0, 0, 2, 3 },
    { 7, 0, 0, 0x1F4, 0, 0x190, 0xE74, 0, 0, 2, 3 },
    { 7, 0, 0, 0x1F4, 0, -0x258, 0x960, 0, 0, 2, 3 },
    { 8, 0, 0, 0xC8, -0x960, 0x6A4, 0xED8, 0, 2, 4, 2 },
    { 8, 0, 0, 0, -0x7D0, 0x640, 0, 0, 2, 4, 2 },
    { 8, 0, 0, -0xC8, -0x898, 0x6A4, 0xC8, 0, 2, 4, 2 },
    { 8, 0, 0, -0x64, -0x8FC, -0x6A4, 0x834, 0, 2, 4, 2 },
    { 8, 0, 0, -0x1F4, -0x9C4, -0x6A4, 0x708, 0, 2, 4, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BDF8[6] = {
    { 0x10, 0, 0, 0x2BC, 0, 0x6A4, 0x200, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0, 0, 0x6A4, 0xE10, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x1F4, 0, 0xC8, 0x708, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x2BC, 0, -0x6A4, 0x5AA, 0, 0, 2, 0 },
    { 0x10, 0, 0, -0x6A4, 0, -0x6A4, 0xA28, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BE58[2] = {
    { 0x8C, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BE78[3] = {
    { 0x28, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BEA8[6] = {
    { 0x28, 0, 0, 0xC8, 0, 0x12C0, 0x12C, 0, 2, 4, 0 },
    { 0x10, 0, 0, -0x12C, 0, 0x16DA, 0xC00, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x64, 0, 0x1838, 0xE10, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x44C, 0, 0x1838, 0x258, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x1004, 0, 0x12C, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BF08[8] = {
    { 0x10, 0, 0, 0x258, 0, -0x7D0, 0x6A4, 0, 0, 2, 0 },
    { 0x10, 0, 0, -0x1F4, 0, -0x7D0, 0xA28, 0, 0, 2, 0 },
    { 7, 0, 0, 0x258, 0, -0x2BC, 0x2BC, 0, 2, 4, 3 },
    { 7, 0, 0, 0x258, 0, 0x64, 0x514, 0, 2, 4, 3 },
    { 7, 0, 0, 0x258, 0, 0x1F4, 0x640, 0, 2, 4, 3 },
    { 7, 0, 0, -0x1F4, 0, 0x64, 0xAF0, 0, 2, 4, 3 },
    { 7, 0, 0, -0x1F4, 0, -0x12C, 0xC80, 0, 2, 4, 3 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017BF88[11] = {
    { 0x28, 1, 0, -0x1F4, 0, -0x7D0, 0x9C4, 0, 0, 2, 0 },
    { 7, 0, 0, -0x1F4, 0, -0x640, 0xCE4, 0, 2, 4, 3 },
    { 7, 0, 0, -0x1F4, 0, -0x384, 0xAF0, 0, 2, 4, 3 },
    { 7, 0, 0, 0, 0, -0x7D0, 0x9C4, 0, 2, 4, 3 },
    { 7, 0, 0, 0x190, 0, -0x7D0, 0x640, 0, 2, 4, 3 },
    { 7, 0, 0, 0x258, 0, -0x3E8, 0x3E8, 0, 2, 4, 3 },
    { 7, 0, 0, -0x1F4, 0, 0xC8, 0x898, 0, 2, 4, 3 },
    { 7, 0, 0, 0, 0, -0x384, 0x834, 0, 2, 4, 3 },
    { 8, 0, 0, -0x64, -0x960, -0x1F4, 0, 0, 4, 6, 2 },
    { 8, 0, 0, 0, -0x960, 0x12C, 0x800, 0, 4, 6, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C038[8] = {
    { 0x28, 1, 0, -0x1F4, 0, -0x578, 0x960, 0, 2, 4, 0 },
    { 0x28, 1, 0, -0xC8, 0, -0x578, 0x708, 0, 2, 4, 0 },
    { 0x28, 1, 0, 0, 0, -0x708, 0x800, 0, 2, 4, 0 },
    { 0x28, 1, 0, 0x1F4, 0, -0x640, 0x578, 0, 2, 4, 0 },
    { 0x10, 0, 0, -0x1F4, 0, 0x2BC, 0x898, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x258, 0, -0x834, 0x6D6, 0, 0, 2, 0 },
    { 0x10, 0, 0, -0x1F4, 0, -0x834, 0x9F6, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C0B8[10] = {
    { 8, 0, 0, 0x1004, -0x9C4, 0x708, 0x320, 0, 0, 2, 2 },
    { 8, 0, 0, 0x11F8, -0xA8C, 0x898, 0x9C4, 0, 0, 2, 2 },
    { 8, 0, 0, 0x10CC, -0x8FC, 0x640, 0xED8, 0, 0, 2, 2 },
    { 8, 0, 0, -0x157C, -0xA28, -0xAF0, 0, 0, 0, 2, 2 },
    { 8, 0, 0, -0x1388, -0x960, -0xB54, 0xC00, 0, 0, 2, 2 },
    { 8, 0, 0, -0x157C, -0x9C4, -0xC1C, 0x800, 0, 0, 2, 2 },
    { 0x10, 0, 0, -0x1F4, 0, -0x1644, 0x834, 0, 2, 4, 0 },
    { 0x10, 0, 0, -0xC8, 0, -0xD48, 0xF3C, 0, 2, 4, 0 },
    { 0x10, 0, 0, 0x1F4, 0, -0x1644, 0x640, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C158[4] = {
    { 6, 0, 1, 0x14B4, 0, -0xDAC, -0xC8, 0, 0, 2, 0 },
    { 6, 0, 1, 0x12C, 0, -0xFA0, 0xAF0, 0, 0, 2, 0 },
    { 6, 0, 1, 0x2BC, 0, -0x1388, 0x3E8, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C198[2] = {
    { 0x2C, 0, 1, 0x1C2, -0xE74, 0x1130, 0xA28, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C1B8[7] = {
    { 0x19, 0, 2, 0x10CC, 0, 0x12C, 0x8CA, 0, 2, 4, 3 },
    { 0x19, 0, 2, 0, 0, -0x1068, 0x400, 0, 2, 4, 3 },
    { 0x19, 0, 2, 0x12C0, 0, -0xF6E, -0x64, 0, 2, 4, 3 },
    { 0x19, 0, 2, 0xD7A, 0, -0xC8, 0x44C, 0, 2, 4, 3 },
    { 6, 0, 1, -0x3E8, 0, -0x11F8, 0x384, 0, 0, 2, 0 },
    { 6, 0, 1, 0xA28, 0, -0xC8, 0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C228[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C238[5] = {
    { 0x8F, 0, 0, 0x456, -0x2EE0, -0x9C4, 0x71C, 0, 0, 2, 0 },
    { 0xF, 1, 3, -0x640, -0x3200, -0x9C4, 0, 0, 2, 4, 0 },
    { 0xF, 1, 3, 0x44C, -0x3200, -0xA28, 0, 0, 2, 4, 0 },
    { 0xF, 1, 3, 0xFA0, -0x3200, -0xA8C, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C288[2] = {
    { 0x8F, 0, 0, 0x456, -0x2EE0, -0x9C4, 0x71C, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C2A8[13] = {
    { 0x25, 0, 2, 0x32C8, -0x898, 0x898, 0, 0, 0, 2, 4 },
    { 0x25, 0, 2, 0x332C, -0x708, 0x898, 0, 0, 0, 2, 4 },
    { 0x25, 0, 2, 0x319C, -0x834, 0x898, 0, 0, 0, 2, 4 },
    { 0x25, 0, 2, 0x3138, -0x76C, 0x898, 0, 0, 0, 2, 4 },
    { 0x25, 0, 2, 0x30D4, -0x8FC, 0x898, 0, 0, 0, 2, 4 },
    { 0x25, 0, 2, 0x3070, -0x898, 0x898, 0, 0, 0, 2, 4 },
    { 0x25, 0, 1, 0x3200, -0x640, 0x8FC, 0x800, 0, 0, 2, 4 },
    { 0x25, 0, 1, 0x319C, -0x4B0, 0x960, 0x800, 0, 0, 2, 4 },
    { 0x25, 0, 1, 0x3070, -0x5DC, 0x8FC, 0x800, 0, 0, 2, 4 },
    { 0x25, 0, 1, 0x300C, -0x514, 0x960, 0x800, 0, 0, 2, 4 },
    { 0x25, 0, 1, 0x2F44, -0x578, 0x8FC, 0x800, 0, 0, 2, 4 },
    { 0x25, 0, 1, 0x2FA8, -0x44C, 0x960, 0x800, 0, 0, 2, 4 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C378[13] = {
    { 1, 0, 0, 0x396C, 1, 0x640, 0xC00, 0, 0, 2, 0 },
    { 8, 0, 0, 0x445C, -0xA8C, 0xAF0, 0x4B0, 0, 2, 4, 2 },
    { 8, 0, 0, 0x4524, -0x9C4, 0x9C4, 0x320, 0, 2, 4, 2 },
    { 8, 0, 0, 0x44C0, -0xAF0, 0xA28, 0x3E8, 0, 2, 4, 2 },
    { 8, 0, 0, 0x445C, -0xA28, 0xA8C, 0x384, 0, 2, 4, 2 },
    { 8, 0, 0, 0x3778, -0xA28, 0x514, 0x76C, 0, 2, 4, 2 },
    { 8, 0, 0, 0x36B0, -0xAF0, 0x578, 0x898, 0, 2, 4, 2 },
    { 8, 0, 0, 0x38A4, -0xA8C, 0x5DC, 0x800, 0, 2, 4, 2 },
    { 8, 0, 0, 0x2328, -0xA28, 0x514, 0x834, 0, 2, 4, 2 },
    { 8, 0, 0, 0x2260, -0x960, 0x578, 0x7D0, 0, 2, 4, 2 },
    { 8, 0, 0, 0x238C, -0xAF0, 0x578, 0x898, 0, 2, 4, 2 },
    { 8, 0, 0, 0x2328, -0x9C4, 0x578, 0x834, 0, 2, 4, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C448[10] = {
    { 0x19, 0, 2, 0x445C, 0, 0x5DC, 0xC1C, 0, 0, 2, 3 },
    { 0x19, 0, 2, 0x2710, 0, 0x708, 0x400, 0, 0, 2, 3 },
    { 0x19, 0, 2, 0x445C, 0, 0xB54, 0xAF0, 0, 0, 2, 3 },
    { 0xF, 0, 3, 0x2580, -0x1388, 0x640, 0x9C4, 0, 2, 4, 0 },
    { 0xF, 0, 3, 0x28A0, -0x1388, 0x4B0, 0x5DC, 0, 2, 4, 0 },
    { 0xF, 0, 3, 0x2BC0, -0x1388, 0x3E8, 0xC8, 0, 2, 4, 0 },
    { 0xF, 0, 3, 0x2EE0, -0x1388, 0x5DC, 0x320, 0, 2, 4, 0 },
    { 0xF, 0, 3, 0x3200, -0x1388, 0x44C, 0xC80, 0, 2, 4, 0 },
    { 0xF, 0, 3, 0x3520, -0x1388, 0x514, 0x960, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C4E8[5] = {
    { 0x10, 0, 0, 0x1F40, 0, 0x1130, 0x5DC, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x238C, 0, 0xA28, 0x1F4, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x2710, 0, 0xDAC, 0x708, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x2648, 0, 0xB54, 0xE10, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C538[3] = {
    { 0x6A, 0, 0, 0x1644, -0x96, 0x170C, 0, 0, 0, 2, 0 },
    { 0x72, 0, 0, 0x14B4, 0, 0x1B58, 0x7FF, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C568[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C578[4] = {
    { 6, 0, 1, 0xBB8, 0, 0x640, 0xDAC, 0, 0, 2, 0 },
    { 6, 0, 1, 0x22C4, 0, 0x1A90, 0xA8C, 0, 0, 2, 0 },
    { 6, 0, 1, 0x24B8, 0, 0xA28, 0xC1C, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C5B8[14] = {
    { 0x25, 0, 2, -0x3E8, -0x708, -0x76C, 0x800, 0, 0, 2, 4 },
    { 0x25, 0, 2, -0x258, -0x578, -0x76C, 0x800, 0, 0, 2, 4 },
    { 0x25, 0, 2, -0xC8, -0x640, -0x76C, 0x800, 0, 0, 2, 4 },
    { 0x25, 0, 0, -0x708, -0x6A4, -0x708, 0xC80, 0, 0, 2, 4 },
    { 0x25, 0, 0, -0x960, -0x514, -0x384, 0xA28, 0, 0, 2, 4 },
    { 0x25, 0, 0, -0x7D0, -0x7D0, -0x44C, 0x898, 0, 0, 2, 4 },
    { 0x25, 0, 0, -0x7D0, -0x640, -0x4B0, 0xC1C, 0, 0, 2, 4 },
    { 0x25, 0, 0, 0x514, -0x834, 0x708, 0xC1C, 0, 0, 2, 4 },
    { 0x25, 0, 0, 0x834, -0x898, 0xA8C, 0x960, 0, 0, 2, 4 },
    { 0x25, 0, 0, 0x898, -0x7D0, 0x4B0, 0x898, 0, 0, 2, 4 },
    { 0x25, 0, 0, 0x4B0, -0x640, 0x44C, 0xBB8, 0, 0, 2, 4 },
    { 0x25, 0, 0, 0x320, -0x708, 0x640, 0x6A4, 0, 0, 2, 4 },
    { 0x25, 0, 0, 0x1F4, -0x76C, 0x7D0, 0xA8C, 0, 0, 2, 4 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C698[13] = {
    { 0x25, 0, 2, -0x514, -0x1194, 0x7D0, 0x578, 0, 0, 2, 5 },
    { 0x25, 0, 2, -0x4B0, -0x10CC, 0x6A4, 0x400, 0, 0, 2, 5 },
    { 0x25, 0, 2, -0x640, -0x1130, 0x578, 0x320, 0, 0, 2, 5 },
    { 0x25, 0, 2, -0x578, -0x1194, 0x2BC, 0x258, 0, 0, 2, 5 },
    { 0x25, 0, 2, -0xA8C, -0xFA0, 0x898, 0x578, 0, 0, 2, 4 },
    { 0x25, 0, 2, -0xAF0, -0x1004, 0x708, 0x400, 0, 0, 2, 4 },
    { 0x25, 0, 2, -0xBB8, -0x1068, 0x514, 0x320, 0, 0, 2, 4 },
    { 0x25, 0, 2, -0xB54, -0x10CC, 0x384, 0x258, 0, 0, 2, 4 },
    { 0x25, 0, 2, -0xC80, -0xED8, 0x834, 0x578, 0, 0, 2, 3 },
    { 0x25, 0, 2, -0xD48, -0xE10, 0x640, 0x400, 0, 0, 2, 3 },
    { 0x25, 0, 2, -0xC1C, -0xDAC, 0x4B0, 0x320, 0, 0, 2, 3 },
    { 0x25, 0, 2, -0xCE4, -0xE74, 0x320, 0x258, 0, 0, 2, 3 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C768[9] = {
    { 0x19, 0, 2, -0xA8C, 0, 0x320, 0x400, 0, 2, 4, 3 },
    { 0x19, 0, 2, 0x12C, 0, 0x384, 0xCE4, 0, 2, 4, 3 },
    { 0x19, 0, 2, 0xC8, 0, 0x866, 0xA8C, 0, 2, 4, 3 },
    { 0xF, 0, 2, 0x79E, -0x6A4, 0xBB8, 0, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0xD48, -0x76C, 0xBB8, 0, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0xC80, -0x898, 0, 0x800, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0x320, -0x708, 0, 0x800, 0, 0, 2, 0 },
    { 0xF, 0, 2, -0xFA0, -0xAF0, -0x1F4, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C7F8[3] = {
    { 6, 0, 0x31, 0x1082, 0, -0x1180, 0, 0, 0, 2, 0 },
    { 6, 0, 0x41, 0x1C6D, 0, -0xDE1, 0x200, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C828[5] = {
    { 0xF, 0, 2, 0x1F40, -0x9C4, 0, 0x800, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0x1F40, -0x708, 0x12F2, 0, 0, 0, 2, 0 },
    { 1, 0, 1, 0x36B0, -1, 0xE74, 0x400, 0, 2, 4, 0 },
    { 1, 0, 1, 0x2AF8, -1, 0x258, 0xED8, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C878[4] = {
    { 6, 0, 1, 0x34BC, 0, 0x578, 0xED8, 0, 0, 2, 0 },
    { 6, 0, 1, 0x1AF4, 0, 0xAF0, 0x4B0, 0, 0, 2, 0 },
    { 6, 0, 1, 0x898, 0, 0x960, 0x4B0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C8B8[3] = {
    { 0x38, 4, 1, 0x1B58, 0, 0x7D0, 0x400, 0, 0, 2, 0 },
    { 0x17, 0, 0, 0x32C8, 0, 0xED8, 0xC00, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C8E8[6] = {
    { 0xF, 0, 2, 0x3200, -0x76C, 0x12F2, 0, 0, 2, 4, 0 },
    { 0xF, 0, 2, 0x2BC0, -0x640, 0x12F2, 0, 0, 2, 4, 0 },
    { 0xF, 0, 2, 0x1F40, -0x960, 0, 0x800, 0, 2, 4, 0 },
    { 0xF, 0, 2, 0x2AF8, -0x898, 0, 0x800, 0, 2, 4, 0 },
    { 6, 0, 1, 0x5334, 0, 0xE74, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C948[2] = {
    { 0x6A, 0, 0, 0xEE2, 0, -0x852, 0x200, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C968[6] = {
    { 0x12, 0x10, 0, 0x509, 1, 0x69D, 0x400, 0, 0, 2, 0 },
    { 0x12, 0x10, 0, 0x708, 1, 0x1388, 0x400, 0, 0, 2, 0 },
    { 8, 0, 0, 0xC1C, -0x8FC, 0x898, 0x640, 0, 2, 4, 2 },
    { 8, 0, 0, 0xE10, -0x834, 0x960, 0x960, 0, 2, 4, 2 },
    { 8, 0, 0, 0xC80, -0x898, 0x834, 0x258, 0, 2, 4, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017C9C8[6] = {
    { 0x10, 0x80, 0, 0x1F4, 0, 0xC80, 0x8FC, 0, 0, 2, 0 },
    { 0x10, 0x80, 0, 0x5AA, 0, 0xC80, 0x6A4, 0, 0, 2, 0 },
    { 0x10, 0x80, 0, 0x41A, 0, 0xC80, 0x800, 0, 0, 2, 0 },
    { 0x10, 0x80, 0, 0x190, 0, 0xE42, 0x708, 0, 0, 2, 0 },
    { 0x10, 0x80, 0, 0x5DC, 0, 0xE74, 0x92E, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CA28[6] = {
    { 0x10, 0, 0, -0x1FA4, -0xC80, 0xA8C, 0x4B0, 0, 0, 2, 0 },
    { 0x10, 0, 0, -0x1A90, -0xC80, 0x5DC, 0x708, 0, 0, 2, 0 },
    { 0x10, 0, 0, -0x1518, -0xC80, 0x29CC, 0x1F4, 0, 0, 2, 0 },
    { 0x10, 0, 0, -0x1518, -0xC80, 0x1F4, 0x640, 0, 0, 2, 0 },
    { 0x10, 0, 0, -0x15E0, -0xC80, -0xE10, 0x514, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CA88[4] = {
    { 0x1F, 0, 0, -0x620, 0, 0x980, 0xC00, 0, 0, 2, 0 },
    { 0x6A, 0, 0, 0, 0, 0, 0, 0, 4, 6, 0 },
    { 0x72, 0, 0, 0, 0, 0, 0, 0, 4, 6, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CAC8[3] = {
    { 6, 0, 1, -0x1838, -0xC80, 0x1644, 0x64, 0, 0, 2, 0 },
    { 6, 0, 1, -0x1644, -0xC80, 0x76C, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CAF8[6] = {
    { 0xF, 0, 2, -0x2008, -0x1450, 0xBB8, 0, 0, 0, 2, 0 },
    { 0xF, 0, 2, -0x1CE8, -0x157C, 0x3E8, 0x800, 0, 0, 2, 0 },
    { 0xF, 0, 2, -0x1B58, -0x1388, 0x12C, 0xC00, 0, 0, 2, 0 },
    { 0xF, 0, 2, -0x12C, -0x15E0, 0x2AF8, 0, 0, 0, 2, 0 },
    { 0xF, 0, 2, -0x1388, -0x13EC, 0x2AF8, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CB58[4] = {
    { 6, 0, 0x21, 0x898, 0, 0, 0xC00, 0, 0, 2, 0 },
    { 6, 0, 0x51, 0xDDE, 0, 0x186, 0xC00, 0, 0, 2, 0 },
    { 6, 0, 0x51, 0x258, 0, -0x578, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CB98[6] = {
    { 6, 0, 1, 0x44C, 0, -0x44C, 0x2BC, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x125C, 0, 0x5DC, 0x44C, 0, 2, 4, 0 },
    { 0x10, 0, 0, 0x128E, 0, -0x258, 0x384, 0, 2, 4, 0 },
    { 0x10, 0, 0, 0xCE4, 0, 0x5DC, 0x190, 0, 2, 4, 0 },
    { 0x10, 0, 0, -0x708, 0, -0x5DC, 0x4B0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CBF8[8] = {
    { 0x10, 0x80, 0, 0x12C0, 0, 0x60E, 0x1F4, 0, 0, 2, 0 },
    { 0x10, 0x80, 0, 0x12C0, 0, 0x320, 0x190, 0, 0, 2, 0 },
    { 0x10, 0x80, 0, 0x12C0, 0, 0xC8, 0xC8, 0, 0, 2, 0 },
    { 0x10, 0x80, 0, 0xFA0, 0, 0x60E, 0x384, 0, 0, 2, 0 },
    { 0x10, 0x80, 0, 0xF3C, 0, 0x320, 0x1F4, 0, 0, 2, 0 },
    { 0x10, 0x80, 0, 0xD48, 0, 0x41A, 0x320, 0, 0, 2, 0 },
    { 0x10, 0x80, 0, 0x1130, 0, 0x1C2, 0x64, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CC78[7] = {
    { 0x10, 0, 0, -0x1644, 0, 0x640, 0xE74, 0, 0, 2, 0 },
    { 0x10, 0, 0, -0x1644, 0, -0x640, 0x898, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x4E2, 0, 0, 0xBB8, 0, 0, 2, 0 },
    { 0x10, 0, 0, -0xA28, 0, 0x320, 0xAF0, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x898, 0, 0x60E, 0xED8, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x128E, 0, 0x60E, 0xC8, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CCE8[2] = {
    { 6, 0, 0x11, 0x14B4, 0, -0x3E8, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CD08[3] = {
    { 0xB, 0, 0, 0x12C0, 0, -0x640, 0xDDE, 0, 0, 2, 0 },
    { 0xB, 0, 0, 0x3E80, 0, -0xD48, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CD38[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CD48[4] = {
    { 6, 0, 1, 0x1C20, 0, -0x190, 0xB54, 0, 0, 2, 0 },
    { 6, 0, 1, 0x2968, 0, -0xBB8, 0x12C, 0, 0, 2, 0 },
    { 6, 0, 1, 0x3C8C, 0, -0xA8C, 0x4B0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CD88[6] = {
    { 0x10, 0, 0, 0x514, 0, 0x12C, 0xA28, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x514, 0, 0xCE4, 0xE10, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x898, 0, 0xCE4, 0x190, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x25E4, 0, 0x12C, 0x5DC, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x27D8, 0, 0x514, 0x578, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CDE8[12] = {
    { 0x25, 0, 0, 0x1644, -0x5DC, 0x6A4, -0x64, 0, 0, 2, 4 },
    { 0x25, 0, 0, 0x22C4, -0x76C, 0x4B0, -0x12C, 0, 0, 2, 4 },
    { 0x25, 0, 0, 0x2134, -0x6A4, 0x5DC, -0x2BC, 0, 0, 2, 4 },
    { 0x25, 0, 0, 0x2328, -0x898, 0x578, -0x190, 0, 0, 2, 4 },
    { 0x25, 0, 0, 0x2328, -0x514, 0x384, -0x3E8, 0, 0, 2, 4 },
    { 0x25, 0, 0, 0x1C84, -0x76C, 0x640, -0x1F4, 0, 0, 2, 4 },
    { 7, 0, 0, 0xED8, 0, 0x1068, 0xD48, 0, 2, 4, 3 },
    { 7, 0, 0, 0x1CE8, 0, 0xEA6, 0x834, 0, 2, 4, 3 },
    { 7, 0, 0, 0x25E4, 0, 0x12C, 0x578, 0, 2, 4, 3 },
    { 7, 0, 0, 0x125C, 0, 0x12C, 0x60E, 0, 2, 4, 3 },
    { 7, 0, 0, 0x898, 0, 0xCE4, 0x64, 0, 2, 4, 3 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CEA8[11] = {
    { 0x10, 0, 0, 0x1AF4, 0, 0x1F4, 0x8CA, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x546, 0, 0x898, -0xC8, 0, 0, 2, 0 },
    { 0x25, 0, 1, 0xAF0, -0x9C4, 0x1F4, 0x12C, 0, 2, 4, 5 },
    { 0x25, 0, 1, 0x1194, -0x9C4, 0x3E8, 0x190, 0, 2, 4, 5 },
    { 0x25, 0, 1, 0x10CC, -0x9C4, 0x2BC, 0x64, 0, 2, 4, 5 },
    { 0x25, 0, 1, 0x1450, -0x9C4, 0x1F4, 0, 0, 2, 4, 5 },
    { 0x25, 0, 1, 0x1900, -0x9C4, 0x3E8, -0x64, 0, 2, 4, 5 },
    { 0x25, 0, 1, 0x1996, -0x9C4, 0x1F4, -0xC8, 0, 2, 4, 5 },
    { 0x25, 0, 1, 0x1A90, -0x9C4, 0x258, -0x12C, 0, 2, 4, 5 },
    { 0x25, 0, 1, 0x1C52, -0x9C4, 0x258, -0x190, 0, 2, 4, 5 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CF58[2] = {
    { 5, 0, 0, 0x4074, -0xFA0, -0x209E, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CF78[6] = {
    { 0x10, 0, 0, -0x578, -0x3E8, -0x1C84, 0xD48, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x1F40, -0x3E8, -0x1C84, 0xFA0, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x32C8, -0x3E8, -0x25E4, 0x7D0, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x3BC4, -0x3E8, -0x2BC, 0xED8, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x4524, -0x3E8, -0x11F8, 0x514, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017CFD8[4] = {
    { 0xB, 1, 0, 0x43F8, -0x3E8, -0x1676, 0xA28, 0, 0, 2, 0 },
    { 0xB, 1, 0, 0x251C, -0x3E8, -0x206C, 0xA28, 0, 0, 2, 0 },
    { 0xB, 1, 0, 0xBB8, -0x3E8, -0x2422, 0x320, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017D018[2] = {
    { 0x16, 0, 0, 0, -0x3E8, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017D038[8] = {
    { 0xF, 0, 3, 0x34BC, -0xFA0, -0x251C, 0x44C, 0, 0, 2, 0 },
    { 0xF, 0, 3, 0x3264, -0xFA0, -0x2198, 0x384, 0, 0, 2, 0 },
    { 0xF, 0, 3, 0x319C, -0xFA0, -0x1C84, 0x640, 0, 0, 2, 0 },
    { 0xF, 0, 3, 0x2D50, -0xFA0, -0x2454, 0, 0, 0, 2, 0 },
    { 0xF, 0, 3, 0x2904, -0xFA0, -0x1FA4, 0xB54, 0, 0, 2, 0 },
    { 0xF, 0, 3, 0x2774, -0xFA0, -0x238C, 0xC80, 0, 0, 2, 0 },
    { 0xF, 0, 3, 0x22C4, -0xFA0, -0x20D0, 0xBB8, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

InventoryBattleReward D_map_dryfield_full_8017D0B8[23] = {
    { GAME_LOCATION_KEY(3, 1, 1, 0), { 2, 0, 0, 6 } },
    { GAME_LOCATION_KEY(3, 5, 3, 0), { 0xA1, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(3, 13, 2, 0), { 0xA1, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(3, 15, 3, 0), { 0x41, 0, 0, 0x3A } },
    { GAME_LOCATION_KEY(3, 15, 4, 0), { 0x3A, 0, 0, 0 } },
    { GAME_LOCATION_KEY(3, 16, 1, 0), { 2, 0, 0, 0 } },
    { GAME_LOCATION_KEY(3, 18, 2, 0), { 7, 0, 0, 0 } },
    { GAME_LOCATION_KEY(3, 22, 3, 0), { 2, 0, 0, 0 } },
    { GAME_LOCATION_KEY(3, 26, 1, 0), { 0xAD, 0, 0, 0xAE } },
    { GAME_LOCATION_KEY(3, 26, 2, 0), { 0x3A, 0, 0, 0 } },
    { GAME_LOCATION_KEY(3, 28, 2, 0), { 0xAB, 0, 0, 0xA9 } },
    { GAME_LOCATION_KEY(3, 29, 2, 0), { 0x3C, 0xAA, 0, 0x43 } },
    { GAME_LOCATION_KEY(3, 29, 4, 0), { 2, 6, 0, 0 } },
    { GAME_LOCATION_KEY(3, 31, 1, 0), { 3, 0, 0, 7 } },
    { GAME_LOCATION_KEY(3, 31, 4, 0), { 0xAD, 0, 0, 0xAE } },
    { GAME_LOCATION_KEY(3, 32, 1, 0), { 0xAD, 0, 0, 0xAE } },
    { GAME_LOCATION_KEY(3, 32, 2, 0), { 0xA1, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(3, 38, 1, 0), { 0x3C, 0xAE, 0, 0xAD } },
    { GAME_LOCATION_KEY(3, 38, 3, 0), { 2, 0, 0, 7 } },
    { GAME_LOCATION_KEY(3, 20, 11, 0), { 0xD, 0, 0, 0x3E } },
    { GAME_LOCATION_KEY(3, 21, 10, 0), { 0xA1, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(3, 22, 21, 0), { 0xAA, 0, 0, 0 } },
    { INVENTORY_BATTLE_REWARD_LIST_END },
};

InventoryBattleReward D_map_dryfield_full_8017D1CC[9] = {
    { GAME_LOCATION_KEY(3, 1, 7, 0), { 0x3C, 0xAA, 7, 0 } },
    { GAME_LOCATION_KEY(3, 2, 7, 0), { 0xAD, 0, 0, 0 } },
    { GAME_LOCATION_KEY(3, 5, 7, 0), { 0xAA, 0, 0, 0 } },
    { GAME_LOCATION_KEY(3, 15, 7, 0), { 0xB, 2, 0xD, 0 } },
    { GAME_LOCATION_KEY(3, 26, 7, 0), { 0x3C, 7, 0xAD, 0 } },
    { GAME_LOCATION_KEY(3, 29, 2, 0), { 0x3D, 0xAA, 0, 0 } },
    { GAME_LOCATION_KEY(3, 38, 1, 0), { 0xAE, 0, 0, 0xAD } },
    { GAME_LOCATION_KEY(3, 38, 7, 0), { 0x3F, 2, 0x3A, 0 } },
    { INVENTORY_BATTLE_REWARD_LIST_END },
};

StageMusicEntry D_map_dryfield_full_8017D238[430] = {
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x80, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0x80, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x2A, 1 },
    { 0x3A, 1 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0x80, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0x80, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x2B, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0x80, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x2A, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x2A, 0 },
    { 0x2A, 0 },
    { 0x3B, 1 },
    { 0x37, 3 },
    { 0x3B, 1 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0x80, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x2D, 3 },
    { 0x2D, 3 },
    { 0x2D, 0 },
    { 0x2D, 0 },
    { 0x2D, 0 },
    { 0x2D, 0 },
    { 0x2D, 0 },
    { 0x3A, 0 },
    { 0x2D, 0 },
    { 0x2D, 3 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0x80, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x38, 1 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0xFF, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x37, 0 },
    { 0x37, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x2F, 0 },
    { 0x2F, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x2F, 0 },
    { 0x2F, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x37, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x39, 0 },
    { 0x37, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x3A, 0 },
    { 0x2F, 0 },
    { 0x2F, 0 },
    { 0, 0 },
};

StageMusicEntry D_map_dryfield_full_8017D594[20] = {
    { 0x30, 2 },
    { 0x17, 2 },
    { 0x30, 2 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x52, 2 },
    { 0x45, 2 },
    { 0x46, 2 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
};
