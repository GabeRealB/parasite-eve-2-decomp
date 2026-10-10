#include "mapui/map_dryfield.h"

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
#include "gameplay/message.h"
#include "gameplay/map.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gfx_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/task_types.h"

#include "mappic/mappic.h"

#include "rooms/dryfield_back_street.h"

#include "rooms/dryfield_breezeway.h"

#include "rooms/dryfield_cellar.h"

#include "rooms/dryfield_dilapidated_house.h"

#include "rooms/dryfield_driveway.h"

#include "rooms/dryfield_factory.h"

#include "rooms/dryfield_g_r_kitchen.h"

#include "rooms/dryfield_garage.h"

#include "rooms/dryfield_gas_station.h"

#include "rooms/dryfield_general_store.h"

#include "rooms/dryfield_junk_yard.h"

#include "rooms/dryfield_main_street.h"

#include "rooms/dryfield_motel_balcony.h"

#include "rooms/dryfield_motel_lobby.h"

#include "rooms/dryfield_motel_loft.h"

#include "rooms/dryfield_motel_room_1.h"

#include "rooms/dryfield_motel_room_2.h"

#include "rooms/dryfield_motel_room_3.h"

#include "rooms/dryfield_motel_room_4.h"

#include "rooms/dryfield_motel_room_5.h"

#include "rooms/dryfield_motel_room_6.h"

#include "rooms/dryfield_parking_lot.h"

#include "rooms/dryfield_r04.h"

#include "rooms/dryfield_r08.h"

#include "rooms/dryfield_saloon_g_r.h"

#include "rooms/dryfield_souvenir_shop.h"

#include "rooms/dryfield_toilet.h"

#include "rooms/dryfield_trailer_coach.h"

#include "rooms/dryfield_underpass.h"

#include "rooms/dryfield_warehouse.h"

#include "rooms/dryfield_water_hole.h"

#include "rooms/dryfield_water_tank.h"

#include "rooms/dryfield_water_tower.h"

/* The Dryfield stage's map UI overlay: the stage's stream setup and the
 * per-stage tables gameplay and main index by stage, most of which point into
 * the stage's room packages or at the map pictures' marker models.
 */

/// Byte extent of the 32-sector movie ring before the first VLC output buffer.
enum { MAP_DRYFIELD_MOVIE_RING_BYTES = 0x10000 };

/// Resource-entry keys used by the daytime map's actor placements.
///
/// These select room-owned resource recipes, rather than global actor IDs.
enum {
    MAP_DRYFIELD_DESERT_CHASER_ENTRY          = 1,
    MAP_DRYFIELD_WOMAN_STRANGER_ENTRY         = 9,
    MAP_DRYFIELD_TOILET_ODD_STRANGER_ENTRY    = 10,
    MAP_DRYFIELD_BONE_SUCKLER_ENTRY           = 12,
    MAP_DRYFIELD_MIND_SUCKLER_ENTRY           = 15,
    MAP_DRYFIELD_ODD_STRANGER_ENTRY           = 18,
    MAP_DRYFIELD_PAWN_GOLEM_ENTRY             = 20,
    MAP_DRYFIELD_KNIGHT_GOLEM_ENTRY           = 22,
    MAP_DRYFIELD_ROOK_GOLEM_ENTRY             = 23,
    MAP_DRYFIELD_SCORPION_ENTRY               = 25,
    MAP_DRYFIELD_ANMC_WOMAN_ENTRY             = 29,
    MAP_DRYFIELD_NO9_GOLEM_ENTRY              = 34,
    MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY          = 40,
    MAP_DRYFIELD_LAUNCHER_PAWN_GOLEM_ENTRY    = 56,
    MAP_DRYFIELD_CUTSCENE_DESERT_CHASER_ENTRY = 101,
    MAP_DRYFIELD_CUTSCENE_BONE_SUCKLER_ENTRY  = 112,
    MAP_DRYFIELD_AYA_DOUBLE_SCENE_ENTRY       = 132,
};

/// Actor-specific selectors stored in the map's placement records.
///
/// The area spawner packs variant in the high half and mode in the low half.
/// GOLEM patrol variants count thousands of world units; other selectors are
/// independent actor states or tuning rows, not distances.
enum {
    MAP_DRYFIELD_CHASER_APPROACH                = 0,
    MAP_DRYFIELD_CHASER_STEER                   = 2,
    MAP_DRYFIELD_CHASER_TUNING_SECOND           = 0,
    MAP_DRYFIELD_CHASER_TUNING_FIRST            = 2,
    MAP_DRYFIELD_SCORPION_AMBUSH_LEADER         = 1,
    MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER       = 2,
    MAP_DRYFIELD_GOLEM_PATROLS                  = 1,
    MAP_DRYFIELD_GOLEM_IDLE                     = 0,
    MAP_DRYFIELD_SUCKLER_PATROL                 = 0,
    MAP_DRYFIELD_SUCKLER_IDLE                   = 1,
    MAP_DRYFIELD_MIND_SUCKLER_PERCHED           = 0,
    MAP_DRYFIELD_MIND_SUCKLER_WALL_NO_SETTLE    = 2,
    MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE = 3,
    MAP_DRYFIELD_ODD_STRANGER_STALK             = 0x10,
    MAP_DRYFIELD_ODD_STRANGER_TUNING_SECOND     = 0,
    MAP_DRYFIELD_ODD_STRANGER_TUNING_THIRD      = 1,
    MAP_DRYFIELD_M950_OBJECT_KIND               = 0x82,
    MAP_DRYFIELD_OBJECT_TASK_PRIORITY           = 98,
};

void mapDryfieldSetupMovieBuffers(const GameLocationKey* location)
{
    s32 pixelCount;

    gCdCmdQueue.movieVramStaging = 0;
    if (location->area == GAME_AREA_DRYFIELD_GAS_STATION) {
        pixelCount    = D_8006AC5A * D_8006AC6C;
        D_8006AC50[0] = (u_long*)((u8*)D_8006AC60 + MAP_DRYFIELD_MOVIE_RING_BYTES);
        D_8006AC50[1] = D_8006AC40;
        D_8006AC48[0] = (u_long*)((u8*)D_8006AC40 + pixelCount * 3);
        D_8006AC48[1] = (u_long*)((u8*)D_8006AC40 + pixelCount * 3 + pixelCount * 2);
    }
    D_8006AC44             = (u8*)D_8006AC48[1] + D_8006AC5A * D_8006AC6C * 2;
    gGameSession->field_7C = 0;
    gGameSession->field_7E = 0;
}

GfxImageSlot gMapDryfieldImageSlots[GAME_AREA_DRYFIELD_UNDERPASS + 1] = {
    [0]                                    = GFX_IMAGE_SLOT(0x3A8E0),
    [GAME_AREA_DRYFIELD_GAS_STATION]       = GFX_IMAGE_SLOT(0x4EE30),
    [GAME_AREA_DRYFIELD_MAIN_STREET]       = GFX_IMAGE_SLOT(0x4F100),
    [GAME_AREA_DRYFIELD_GENERAL_STORE]     = GFX_IMAGE_SLOT(0x4DE10),
    [GAME_AREA_DRYFIELD_R04]               = GFX_IMAGE_SLOT(0x585E0),
    [GAME_AREA_DRYFIELD_BACK_STREET]       = GFX_IMAGE_SLOT(0x54F10),
    [GAME_AREA_DRYFIELD_SOUVENIR_SHOP]     = GFX_IMAGE_SLOT(0x56780),
    [GAME_AREA_DRYFIELD_WAREHOUSE]         = GFX_IMAGE_SLOT(0x53770),
    [GAME_AREA_DRYFIELD_R08]               = GFX_IMAGE_SLOT(0x54E70),
    [GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE] = GFX_IMAGE_SLOT(0x49580),
    [10]                                   = { NULL, 0 },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_1]      = GFX_IMAGE_SLOT(0x543A0),
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_2]      = GFX_IMAGE_SLOT(0x558B0),
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_3]      = GFX_IMAGE_SLOT(0x58030),
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_4]      = GFX_IMAGE_SLOT(0x57D70),
    [GAME_AREA_DRYFIELD_PARKING_LOT]       = GFX_IMAGE_SLOT(0x560A0),
    [GAME_AREA_DRYFIELD_TOILET]            = GFX_IMAGE_SLOT(0x4B730),
    [GAME_AREA_DRYFIELD_MOTEL_LOBBY]       = GFX_IMAGE_SLOT(0x545C0),
    [GAME_AREA_DRYFIELD_SALOON_G_R]        = GFX_IMAGE_SLOT(0x523E0),
    [GAME_AREA_DRYFIELD_G_R_KITCHEN]       = GFX_IMAGE_SLOT(0x57030),
    [GAME_AREA_DRYFIELD_WATER_TOWER]       = GFX_IMAGE_SLOT(0x4C970),
    [GAME_AREA_DRYFIELD_WATER_TANK]        = GFX_IMAGE_SLOT(0x4BD80),
    [GAME_AREA_DRYFIELD_BREEZEWAY]         = GFX_IMAGE_SLOT(0x514F0),
    [GAME_AREA_DRYFIELD_FACTORY]           = GFX_IMAGE_SLOT(0x495E0),
    [GAME_AREA_DRYFIELD_GARAGE]            = GFX_IMAGE_SLOT(0x53ED0),
    [GAME_AREA_DRYFIELD_DRIVEWAY]          = GFX_IMAGE_SLOT(0x551F0),
    [GAME_AREA_DRYFIELD_JUNK_YARD]         = GFX_IMAGE_SLOT(0x532A0),
    [GAME_AREA_DRYFIELD_TRAILER_COACH]     = GFX_IMAGE_SLOT(0x49210),
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_5]      = GFX_IMAGE_SLOT(0x57F20),
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY]     = GFX_IMAGE_SLOT(0x4D6F0),
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_6]      = GFX_IMAGE_SLOT(0x4C850),
    [GAME_AREA_DRYFIELD_MOTEL_LOFT]        = GFX_IMAGE_SLOT(0x56A60),
    [GAME_AREA_DRYFIELD_WATER_HOLE]        = GFX_IMAGE_SLOT(0x52EE0),
    [33]                                   = { NULL, 0 },
    [GAME_AREA_DRYFIELD_CELLAR]            = GFX_IMAGE_SLOT(0x541F0),
    [35]                                   = { NULL, 0 },
    [36]                                   = { NULL, 0 },
    [37]                                   = { NULL, 0 },
    [GAME_AREA_DRYFIELD_UNDERPASS]         = GFX_IMAGE_SLOT(0x54820),
};

s32 gMapDryfieldMovieWorkspaceBytesByArea[32] = {
    [GAME_AREA_DRYFIELD_GAS_STATION] = 0x2A00,
};

u8 D_map_dryfield_80179BCC[4] = { 0, 0x16, 0x16, 0x16 };

MenuMapArea D_map_dryfield_80179BD0[40] = {
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
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_END },
};

MenuMapAreaShape D_map_dryfield_80179E00[39] = {
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS202GasStationShape, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS202MainStreetShape, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS202GeneralStoreShape, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS202BackStreetShape, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS202SouvenirShopShape, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F2A8, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F354, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F3E4, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F474, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F504, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F594, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F694, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F724, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F7B4, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F894, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F924, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F9D0, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS200WaterTankShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012FA7C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012FB0C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012FB9C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012FC64, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012FD60, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012FDF0, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS200MotelRoom5Shape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS200MotelBalconyShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS200MotelRoom6Shape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS200MotelLoftShape, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS203WaterHoleShape, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &gMappicS203CellarShape, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_03_8012F180, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
};

MenuMapMarker D_map_dryfield_80179F38[30] = {
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
    { 0, 0, 0, 0 },
};

MenuMapIcon D_map_dryfield_80179FEC[11] = {
    { 2, 1, MENU_MAP_ICON_KIND_TELEPHONE, 0, 19, 38 },
    { 2, 1, 0, 0, 53, 54 },
    { 2, 0x1B, MENU_MAP_ICON_KIND_TELEPHONE, 0, 79, -38 },
    { 1, 0x1E, 0, 0, 0, 7 },
    { 1, 0x1E, MENU_MAP_ICON_KIND_TELEPHONE, 0, 0, -5 },
    { 2, 0x18, MENU_MAP_ICON_KIND_OBJECTIVE, 0xA, 41, -33 },
    { 1, 0x1E, MENU_MAP_ICON_KIND_OBJECTIVE, 0xB, 3, -3 },
    { 1, 0x15, MENU_MAP_ICON_KIND_OBJECTIVE, 0xD, -27, -31 },
    { 2, 0x16, MENU_MAP_ICON_KIND_OBJECTIVE, 0xE, -8, -25 },
    { 2, 0x19, MENU_MAP_ICON_KIND_OBJECTIVE, 0xF, 33, -6 },
    { 0, 0, 0, 0, 0, 0 },
};

MenuMapAreaName D_map_dryfield_8017A044[38] = {
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
    { "" },
    { "" },
    { "Parking lot" },
    { "Toilet" },
    { "" },
    { "Saloon G & R" },
    { "G & R Kitchen" },
    { "Water tower" },
    { "Water tank" },
    { "Breezeway" },
    { "Factory" },
    { "Garage" },
    { "Driveway" },
    { "Junk yard" },
    { "Trailer coach" },
    { "" },
    { "Motel balcony" },
    { "Motel room 6" },
    { "" },
    { "Water hole" },
    { "" },
    { "Cellar" },
    { "" },
    { "" },
    { "" },
    { "Underpass" },
};

/// Shared empty spawn list for daytime areas without a model-backed object recipe.
///
/// Contains only `AREA_OBJECT_SPAWN_END`; no placed kind matches it.
static AreaObjectSpawn _gMapDryfieldEmptyObjectSpawns[1] = {
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// Empty spawn list selected by the daytime Gas Station.
///
/// Contains only `AREA_OBJECT_SPAWN_END`; no placed kind matches it.
static AreaObjectSpawn _gMapDryfieldGasStationObjectSpawns[1] = {
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// Wire Rope model recipe for the daytime Junk Yard.
///
/// Matches the corresponding placed-object kind, then `AREA_OBJECT_SPAWN_END`.
/// Borrows the room-owned model geometry until the spawned task ends.
static AreaObjectSpawn _gMapDryfieldJunkYardObjectSpawns[2] = {
    { .kind = INVENTORY_COLLECTION_ID_WIRE_ROPE, .taskDesc = { .header = { .fields = { TASK_BODY_TMD, MAP_DRYFIELD_OBJECT_TASK_PRIORITY } }, .callback = dryfieldJunkYardWireRopeTask, .data = { .model = &gDryfieldJunkYardModel01378 } } },
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// M950 pickup container-lid recipe for the daytime Trailer Coach.
///
/// Matches the corresponding placed-object kind, then `AREA_OBJECT_SPAWN_END`.
/// Borrows the room-owned model geometry until the spawned task ends.
static AreaObjectSpawn _gMapDryfieldTrailerCoachObjectSpawns[2] = {
    { .kind = MAP_DRYFIELD_M950_OBJECT_KIND, .taskDesc = { .header = { .fields = { TASK_BODY_TMD, MAP_DRYFIELD_OBJECT_TASK_PRIORITY } }, .callback = itemPickupContainerLidTask, .data = { .model = &gDryfieldTrailerCoachAcropolisSanctuaryModel090F0 } } },
    { .kind = AREA_OBJECT_SPAWN_END },
};

AreaObjectRoom gMapDryfieldObjectRooms[GAME_AREA_DRYFIELD_UNDERPASS + 2] = {
    [0]                                    = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_GAS_STATION]       = { .places = { .list = gAreaDryfieldGasStationObjectPlaces }, .spawns = _gMapDryfieldGasStationObjectSpawns },
    [GAME_AREA_DRYFIELD_MAIN_STREET]       = { .places = { .list = gAreaDryfieldMainStreetObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_GENERAL_STORE]     = { .places = { .list = gAreaDryfieldGeneralStoreObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_R04]               = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_BACK_STREET]       = { .places = { .list = gAreaDryfieldBackStreetObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_SOUVENIR_SHOP]     = { .places = { .list = gAreaDryfieldSouvenirShopObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_WAREHOUSE]         = { .places = { .list = gAreaDryfieldWarehouseObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_R08]               = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE] = { .places = { .list = NULL }, .spawns = NULL },
    [10]                                   = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_1]      = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_2]      = { .places = { .list = gAreaDryfieldMotelRoom2ObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_3]      = { .places = { .list = gAreaDryfieldMotelRoom3ObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_4]      = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_PARKING_LOT]       = { .places = { .list = gAreaDryfieldParkingLotObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_TOILET]            = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_MOTEL_LOBBY]       = { .places = { .list = gAreaDryfieldMotelLobbyObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_SALOON_G_R]        = { .places = { .list = gAreaDryfieldSaloonGRObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_G_R_KITCHEN]       = { .places = { .list = gAreaDryfieldGRKitchenObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_WATER_TOWER]       = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_WATER_TANK]        = { .places = { .list = gAreaDryfieldWaterTankObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_BREEZEWAY]         = { .places = { .list = gAreaDryfieldBreezewayObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_FACTORY]           = { .places = { .list = gAreaDryfieldFactoryObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_GARAGE]            = { .places = { .list = gAreaDryfieldGarageObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_DRIVEWAY]          = { .places = { .list = gAreaDryfieldDrivewayObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_JUNK_YARD]         = { .places = { .list = gAreaDryfieldJunkYardObjectPlaces }, .spawns = _gMapDryfieldJunkYardObjectSpawns },
    [GAME_AREA_DRYFIELD_TRAILER_COACH]     = { .places = { .list = gAreaDryfieldTrailerCoachObjectPlaces }, .spawns = _gMapDryfieldTrailerCoachObjectSpawns },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_5]      = { .places = { .list = gAreaDryfieldMotelRoom5ObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY]     = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_6]      = { .places = { .list = gAreaDryfieldMotelRoom6ObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_MOTEL_LOFT]        = { .places = { .list = gAreaDryfieldMotelLoftObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [GAME_AREA_DRYFIELD_WATER_HOLE]        = { .places = { .list = gAreaDryfieldWaterHoleObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [33]                                   = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_CELLAR]            = { .places = { .list = gAreaDryfieldCellarObjectPlaces }, .spawns = _gMapDryfieldEmptyObjectSpawns },
    [35]                                   = { .places = { .list = NULL }, .spawns = NULL },
    [36]                                   = { .places = { .list = NULL }, .spawns = NULL },
    [37]                                   = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_UNDERPASS]         = { .places = { .list = NULL }, .spawns = NULL },
    [GAME_AREA_DRYFIELD_UNDERPASS + 1]     = { .places = { .sentinel = AREA_OBJECT_ROOM_END }, .spawns = NULL },
};

TaskDesc D_map_dryfield_8017A6A4[] = {
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldGasStationRoomTask, { .value = GP_TASK_LOC_KEY(2, 1, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMainStreetTask, { .value = GP_TASK_LOC_KEY(2, 2, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldGeneralStoreRoomTask, { .value = GP_TASK_LOC_KEY(2, 3, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldBackStreetRoomTask, { .value = GP_TASK_LOC_KEY(2, 5, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldSouvenirShopRoomTask, { .value = GP_TASK_LOC_KEY(2, 6, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldWarehouseRoomTask, { .value = GP_TASK_LOC_KEY(2, 7, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldDilapidatedHouseRoomTask, { .value = GP_TASK_LOC_KEY(2, 9, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelRoom1Task, { .value = GP_TASK_LOC_KEY(2, 11, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelRoom2Task, { .value = GP_TASK_LOC_KEY(2, 12, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelRoom3Task, { .value = GP_TASK_LOC_KEY(2, 13, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelRoom4Task, { .value = GP_TASK_LOC_KEY(2, 14, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldParkingLotRoomTask, { .value = GP_TASK_LOC_KEY(2, 15, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldToiletRoomTask, { .value = GP_TASK_LOC_KEY(2, 16, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelLobbyTask, { .value = GP_TASK_LOC_KEY(2, 17, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldSaloonGRRoomTask, { .value = GP_TASK_LOC_KEY(2, 18, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldGRKitchenRoomTask, { .value = GP_TASK_LOC_KEY(2, 19, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldWaterTowerRoomTask, { .value = GP_TASK_LOC_KEY(2, 20, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldWaterTankRoomTask, { .value = GP_TASK_LOC_KEY(2, 21, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldBreezewayMessageTask, { .value = GP_TASK_LOC_KEY(2, 22, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldFactoryEntryTask, { .value = GP_TASK_LOC_KEY(2, 23, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldGarageRoomTask, { .value = GP_TASK_LOC_KEY(2, 24, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldDrivewayRoomTask, { .value = GP_TASK_LOC_KEY(2, 25, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldJunkYardRoomTask, { .value = GP_TASK_LOC_KEY(2, 26, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldTrailerCoachRoomTask, { .value = GP_TASK_LOC_KEY(2, 27, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelRoom5Task, { .value = GP_TASK_LOC_KEY(2, 28, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelBalconyRoomTask, { .value = GP_TASK_LOC_KEY(2, 29, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelRoom6Task, { .value = GP_TASK_LOC_KEY(2, 30, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelLoftTask, { .value = GP_TASK_LOC_KEY(2, 31, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldWaterHoleRoomTask, { .value = GP_TASK_LOC_KEY(2, 32, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldCellarRoomTask, { .value = GP_TASK_LOC_KEY(2, 34, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldUnderpassRoomTask, { .value = GP_TASK_LOC_KEY(2, 38, 0) } },
    { { { TASK_DESC_END, 0x20 } }, NULL, { 0 } },
};

u16 D_map_dryfield_8017A824[29] = {
    0x1E9,
    0x1E8,
    0x1E7,
    0x1E6,
    0x1E5,
    0x1E4,
    0x1E3,
    0x1E2,
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
};

WorldCoordRoomLighting* gMapDryfieldRoomLightingTables[38] = {
    gDryfieldGasStationRoomLighting,
    gDryfieldMainStreetRoomLighting,
    gDryfieldGeneralStoreRoomLighting,
    NULL,
    gDryfieldBackStreetRoomLighting,
    gDryfieldSouvenirShopRoomLighting,
    gDryfieldWarehouseRoomLighting,
    gDryfieldR08RoomLighting,
    gDryfieldDilapidatedHouseRoomLighting,
    NULL,
    gDryfieldMotelRoom1RoomLighting,
    gDryfieldMotelRoom2RoomLighting,
    gDryfieldMotelRoom3RoomLighting,
    gDryfieldMotelRoom4RoomLighting,
    gDryfieldParkingLotRoomLighting,
    gDryfieldToiletRoomLighting,
    gDryfieldMotelLobbyRoomLighting,
    gDryfieldSaloonGRRoomLighting,
    gDryfieldGRKitchenRoomLighting,
    gDryfieldWaterTowerRoomLighting,
    gDryfieldWaterTankRoomLighting,
    gDryfieldBreezewayRoomLighting,
    gDryfieldFactoryRoomLighting,
    gDryfieldGarageRoomLighting,
    gDryfieldDrivewayRoomLighting,
    gDryfieldJunkYardRoomLighting,
    gDryfieldTrailerCoachRoomLighting,
    gDryfieldMotelRoom5RoomLighting,
    gDryfieldMotelBalconyRoomLighting,
    gDryfieldMotelRoom6RoomLighting,
    gDryfieldMotelLoftRoomLighting,
    gDryfieldWaterHoleRoomLighting,
    NULL,
    gDryfieldCellarRoomLighting,
    NULL,
    NULL,
    NULL,
    gDryfieldUnderpassRoomLighting,
};

DirectionWarpEntry* D_map_dryfield_8017A8F8[38] = {
    D_dryfield_gas_station_8018316C,
    gDryfieldMainStreetWarpEntries,
    D_dryfield_general_store_8017E690,
    gDryfieldR04WarpEntries,
    gDryfieldBackStreetWarpEntries,
    gDryfieldSouvenirShopWarpEntries,
    D_dryfield_warehouse_8017FC30,
    gDryfieldR08WarpEntries,
    gDryfieldDilapidatedHouseWarpEntries,
    NULL,
    D_dryfield_motel_room_1_8017E4CC,
    gDryfieldMotelRoom2WarpEntries,
    gDryfieldMotelRoom3WarpEntries,
    gDryfieldMotelRoom4WarpEntries,
    gDryfieldParkingLotWarpEntries,
    gDryfieldToiletWarpEntries,
    gDryfieldMotelLobbyWarpEntries,
    gDryfieldSaloonGRWarpEntries,
    gDryfieldGRKitchenWarpEntries,
    D_dryfield_water_tower_801827EC,
    gDryfieldWaterTankWarpEntries,
    D_dryfield_breezeway_8018318C,
    gDryfieldFactoryWarpEntries,
    gDryfieldGarageWarpEntries,
    D_dryfield_driveway_8017E7C8,
    gDryfieldJunkYardWarpEntries,
    gDryfieldTrailerCoachWarpEntries,
    gDryfieldMotelRoom5WarpEntries,
    gDryfieldMotelBalconyWarpEntries,
    gDryfieldMotelRoom6WarpEntries,
    gDryfieldMotelLoftWarpEntries,
    gDryfieldWaterHoleWarpEntries,
    NULL,
    D_dryfield_cellar_8017DC20,
    NULL,
    NULL,
    NULL,
    D_dryfield_underpass_8017EC10,
};

static ViewCount* D_map_dryfield_8017A990[38] = {
    D_dryfield_gas_station_80183168,
    gDryfieldMainStreetRoomViewCounts,
    D_dryfield_general_store_8017E684,
    gDryfieldR04RoomViewCounts,
    gDryfieldBackStreetRoomViewCounts,
    gDryfieldSouvenirShopRoomViewCounts,
    gDryfieldWarehouseRoomViewCounts,
    gDryfieldR08RoomViewCounts,
    gDryfieldDilapidatedHouseRoomViewCounts,
    NULL,
    D_dryfield_motel_room_1_8017E4B8,
    gDryfieldMotelRoom2RoomViewCounts,
    gDryfieldMotelRoom3RoomViewCounts,
    gDryfieldMotelRoom4RoomViewCounts,
    gDryfieldParkingLotRoomViewCounts,
    gDryfieldToiletRoomViewCounts,
    gDryfieldMotelLobbyRoomViewCounts,
    gDryfieldSaloonGRRoomViewCounts,
    gDryfieldGRKitchenViewCounts,
    gDryfieldWaterTowerRoomViewCounts,
    gDryfieldWaterTankRoomViewCounts,
    gDryfieldBreezewayViewCounts,
    gDryfieldFactoryRoomViewCounts,
    gDryfieldGarageRoomViewCounts,
    D_dryfield_driveway_8017E7C4,
    gDryfieldJunkYardRoomViewCounts,
    gDryfieldTrailerCoachViewCounts,
    gDryfieldMotelRoom5RoomViewCounts,
    gDryfieldMotelBalconyRoomViewCounts,
    gDryfieldMotelRoom6RoomViewCounts,
    gDryfieldMotelLoftRoomViewCounts,
    gDryfieldWaterHoleRoomViewCounts,
    NULL,
    D_dryfield_cellar_8017DC0C,
    NULL,
    NULL,
    NULL,
    gDryfieldUnderpassRoomViewCounts,
};

ViewCountTable D_map_dryfield_8017AA28 = { D_map_dryfield_8017A990 };

static WorldCollisionRoomResources* D_map_dryfield_8017AA2C[38] = {
    D_dryfield_gas_station_8018314C,
    gDryfieldMainStreetRoomCollisionResources,
    D_dryfield_general_store_8017E670,
    gDryfieldR04RoomCollisionResources,
    gDryfieldBackStreetRoomCollisionResources,
    gDryfieldSouvenirShopRoomCollisionResources,
    D_dryfield_warehouse_8017FBBC,
    gDryfieldR08RoomCollisionResources,
    gDryfieldDilapidatedHouseRoomCollisionResources,
    NULL,
    D_dryfield_motel_room_1_8017E484,
    gDryfieldMotelRoom2RoomCollisionResources,
    gDryfieldMotelRoom3RoomCollisionResources,
    gDryfieldMotelRoom4RoomCollisionResources,
    gDryfieldParkingLotRoomCollisionResources,
    gDryfieldToiletRoomCollisionResources,
    gDryfieldMotelLobbyRoomCollisionResources,
    gDryfieldSaloonGRRoomCollisionResources,
    gDryfieldGRKitchenRoomCollisionResources,
    D_dryfield_water_tower_801827CC,
    gDryfieldWaterTankRoomCollisionResources,
    D_dryfield_breezeway_8018316C,
    D_dryfield_factory_80186F10,
    gDryfieldGarageRoomCollisionResources,
    D_dryfield_driveway_8017E784,
    gDryfieldJunkYardRoomCollisionResources,
    gDryfieldTrailerCoachRoomCollisionResources,
    gDryfieldMotelRoom5RoomCollisionResources,
    gDryfieldMotelBalconyRoomCollisionResources,
    gDryfieldMotelRoom6RoomCollisionResources,
    gDryfieldMotelLoftRoomCollisionResources,
    gDryfieldWaterHoleRoomCollisionResources,
    NULL,
    D_dryfield_cellar_8017DBDC,
    NULL,
    NULL,
    NULL,
    D_dryfield_underpass_8017EB20,
};

WorldCollisionStageResources D_map_dryfield_8017AAC4 = { D_map_dryfield_8017AA2C };

static ViewCamera* D_map_dryfield_8017AAC8[38] = {
    D_dryfield_gas_station_80183EC8,
    gDryfieldMainStreetViewCameras,
    D_dryfield_general_store_8017F25C,
    gDryfieldR04ViewCameras,
    gDryfieldBackStreetViewCameras,
    gDryfieldSouvenirShopViewCameras,
    gDryfieldWarehouseViewCameras,
    gDryfieldR08ViewCameras,
    gDryfieldDilapidatedHouseViewCameras,
    NULL,
    D_dryfield_motel_room_1_8017EAE0,
    gDryfieldMotelRoom2ViewCameras,
    gDryfieldMotelRoom3ViewCameras,
    gDryfieldMotelRoom4ViewCameras,
    gDryfieldParkingLotViewCameras,
    gDryfieldToiletViewCameras,
    gDryfieldMotelLobbyViewCameras,
    gDryfieldSaloonGRViewCameras,
    gDryfieldGRKitchenViewCameras,
    gDryfieldWaterTowerViewCameras,
    gDryfieldWaterTankViewCameras,
    gDryfieldBreezewayViewCameras,
    gDryfieldFactoryViewCameras,
    gDryfieldGarageViewCameras,
    gDryfieldDrivewayViewCameras,
    gDryfieldJunkYardViewCameras,
    gDryfieldTrailerCoachViewCameras,
    gDryfieldMotelRoom5ViewCameras,
    gDryfieldMotelBalconyViewCameras,
    gDryfieldMotelRoom6ViewCameras,
    gDryfieldMotelLoftViewCameras,
    gDryfieldWaterHoleViewCameras,
    NULL,
    D_dryfield_cellar_8017DF78,
    NULL,
    NULL,
    NULL,
    gDryfieldUnderpassViewCameras,
};

ViewCameraTable D_map_dryfield_8017AB60 = { D_map_dryfield_8017AAC8 };

/// 38 area entries borrowing room-local logical-view map directories.
///
/// Indexed by area minus one; rooms and logical views use their own bounds.
/// NULL entries have no maps. Room pointers stay valid only while that room is loaded.
static u8** _gMapDryfieldAreaViewMaps[38] = {
    gDryfieldGasStationRoomViewMaps,
    gDryfieldMainStreetRoomViewMaps,
    gDryfieldGeneralStoreRoomViewMaps,
    gDryfieldR04ViewMaps,
    gDryfieldBackStreetViewMaps,
    gDryfieldSouvenirShopRoomViewMaps,
    gDryfieldWarehouseRoomViewMaps,
    gDryfieldR08RoomViewMaps,
    gDryfieldDilapidatedHouseRoomViewMaps,
    NULL,
    gDryfieldMotelRoom1RoomViewMaps,
    gDryfieldMotelRoom2RoomViewMaps,
    gDryfieldMotelRoom3RoomViewMaps,
    gDryfieldMotelRoom4RoomViewMaps,
    gDryfieldParkingLotRoomViewMaps,
    gDryfieldToiletRoomViewMaps,
    gDryfieldMotelLobbyRoomViewMaps,
    gDryfieldSaloonGRRoomViewMaps,
    gDryfieldGRKitchenViewMaps,
    gDryfieldWaterTowerRoomViewMaps,
    gDryfieldWaterTankRoomViewMaps,
    gDryfieldBreezewayViewMaps,
    gDryfieldFactoryRoomViewMaps,
    gDryfieldGarageRoomViewMaps,
    gDryfieldDrivewayRoomViewMaps,
    gDryfieldJunkYardRoomViewMaps,
    gDryfieldTrailerCoachViewMaps,
    gDryfieldMotelRoom5RoomViewMaps,
    gDryfieldMotelBalconyViewMaps,
    gDryfieldMotelRoom6RoomViewMaps,
    gDryfieldMotelLoftRoomViewMaps,
    gDryfieldWaterHoleRoomViewMaps,
    NULL,
    gDryfieldCellarRoomViewMaps,
    NULL,
    NULL,
    NULL,
    gDryfieldUnderpassRoomViewMaps,
};

ViewIndexTable gMapDryfieldViewIndexTable = { _gMapDryfieldAreaViewMaps };

/// 38 area entries borrowing mapped-view sprite arrays from room overlays.
///
/// Indexed by area minus one, then the mapped view minus one. Room pointers
/// must stay loaded; an area entry supplies no sprite-view count or sentinel.
static SpriteView* _gMapDryfieldAreaSpriteViews[38] = {
    D_dryfield_gas_station_801842A8,
    gDryfieldMainStreetSpriteViews,
    D_dryfield_general_store_8018402C,
    gDryfieldR04SpriteViews,
    gDryfieldBackStreetSpriteViews,
    gDryfieldSouvenirShopSpriteViews,
    gDryfieldWarehouseSpriteViews,
    gDryfieldR08SpriteViews,
    gDryfieldDilapidatedHouseSpriteViews,
    NULL,
    D_dryfield_motel_room_1_80180C90,
    gDryfieldMotelRoom2SpriteViews,
    gDryfieldMotelRoom3SpriteViews,
    gDryfieldMotelRoom4SpriteViews,
    gDryfieldParkingLotSpriteViews,
    gDryfieldToiletSpriteViews,
    gDryfieldMotelLobbySpriteViews,
    gDryfieldSaloonGRSpriteViews,
    gDryfieldGRKitchenSpriteViews,
    gDryfieldWaterTowerSpriteViews,
    gDryfieldWaterTankSpriteViews,
    gDryfieldBreezewaySpriteViews,
    gDryfieldFactorySpriteViews,
    gDryfieldGarageSpriteViews,
    gDryfieldDrivewaySpriteViews,
    gDryfieldJunkYardSpriteViews,
    gDryfieldTrailerCoachSpriteViews,
    gDryfieldMotelRoom5SpriteViews,
    gDryfieldMotelBalconySpriteViews,
    gDryfieldMotelRoom6SpriteViews,
    gDryfieldMotelLoftSpriteViews,
    gDryfieldWaterHoleSpriteViews,
    NULL,
    gDryfieldCellarSpriteViews,
    NULL,
    NULL,
    NULL,
    gDryfieldUnderpassSpriteViews,
};

SpriteAreaTable gMapDryfieldSpriteAreaTable = { _gMapDryfieldAreaSpriteViews };

WorldCollisionSurfaceProperties** gMapDryfieldSurfacePropertiesByArea[GAME_AREA_DRYFIELD_UNDERPASS] = {
    [GAME_AREA_DRYFIELD_GAS_STATION - 1]       = gDryfieldGasStationSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_MAIN_STREET - 1]       = gDryfieldMainStreetSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_GENERAL_STORE - 1]     = gDryfieldGeneralStoreSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_R04 - 1]               = gDryfieldR04SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_BACK_STREET - 1]       = gDryfieldBackStreetSurfaceProperties,
    [GAME_AREA_DRYFIELD_SOUVENIR_SHOP - 1]     = gDryfieldSouvenirShopSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_WAREHOUSE - 1]         = gDryfieldWarehouseSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_R08 - 1]               = gDryfieldR08SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE - 1] = gDryfieldDilapidatedHouseSurfacePropertiesByClass,
    [10 - 1]                                   = NULL,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_1 - 1]      = gDryfieldMotelRoom1SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_2 - 1]      = gDryfieldMotelRoom2SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_3 - 1]      = gDryfieldMotelRoom3SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_4 - 1]      = gDryfieldMotelRoom4SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_PARKING_LOT - 1]       = gDryfieldParkingLotSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_TOILET - 1]            = gDryfieldToiletSurfaceClasses,
    [GAME_AREA_DRYFIELD_MOTEL_LOBBY - 1]       = gDryfieldMotelLobbySurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_SALOON_G_R - 1]        = gDryfieldSaloonGRSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_G_R_KITCHEN - 1]       = gDryfieldGRKitchenSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_WATER_TOWER - 1]       = gDryfieldWaterTowerSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_WATER_TANK - 1]        = gDryfieldWaterTankSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_BREEZEWAY - 1]         = gDryfieldBreezewaySurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_FACTORY - 1]           = gDryfieldFactorySurfaceClasses,
    [GAME_AREA_DRYFIELD_GARAGE - 1]            = gDryfieldGarageSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_DRIVEWAY - 1]          = gDryfieldDrivewaySurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_JUNK_YARD - 1]         = gDryfieldJunkYardSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_TRAILER_COACH - 1]     = gDryfieldTrailerCoachSurfaceClasses,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_5 - 1]      = gDryfieldMotelRoom5SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY - 1]     = gDryfieldMotelBalconySurfaceClasses,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_6 - 1]      = gDryfieldMotelRoom6SurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_MOTEL_LOFT - 1]        = gDryfieldMotelLoftSurfacePropertiesByClass,
    [GAME_AREA_DRYFIELD_WATER_HOLE - 1]        = gDryfieldWaterHoleSurfacePropertiesByClass,
    [33 - 1]                                   = NULL,
    [GAME_AREA_DRYFIELD_CELLAR - 1]            = gDryfieldCellarSurfacePropertiesByClass,
    [35 - 1]                                   = NULL,
    [36 - 1]                                   = NULL,
    [37 - 1]                                   = NULL,
    [GAME_AREA_DRYFIELD_UNDERPASS - 1]         = gDryfieldUnderpassSurfacePropertiesByClass,
};

AreaPlacement gMapDryfieldGasStationVariant1Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMainStreetVariant1Placements[3] = {
    { .entryId = MAP_DRYFIELD_CUTSCENE_DESERT_CHASER_ENTRY, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_DESERT_CHASER_ENTRY, .variant = MAP_DRYFIELD_CHASER_APPROACH, .mode = MAP_DRYFIELD_CHASER_TUNING_SECOND, .x = -104, .y = 0, .z = 4600, .yaw = -ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMainStreetVariant2Placements[7] = {
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_LEADER, .x = -3500, .y = 0, .z = 4500, .yaw = 500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_LEADER, .x = -1500, .y = 0, .z = 4800, .yaw = 3700, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = -2000, .y = 0, .z = 7500, .yaw = 2200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = -3500, .y = 0, .z = 6500, .yaw = 1500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = -1700, .y = 0, .z = 300, .yaw = 3700, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = -3200, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMainStreetVariant3Placements[3] = {
    { .entryId = MAP_DRYFIELD_DESERT_CHASER_ENTRY, .variant = MAP_DRYFIELD_CHASER_STEER, .mode = MAP_DRYFIELD_CHASER_TUNING_SECOND, .x = -6000, .y = 0, .z = 6900, .yaw = 1900, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_DESERT_CHASER_ENTRY, .variant = MAP_DRYFIELD_CHASER_STEER, .mode = MAP_DRYFIELD_CHASER_TUNING_SECOND, .x = -5500, .y = 0, .z = -3900, .yaw = 4050, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMainStreetVariant7Placements[3] = {
    { .entryId = MAP_DRYFIELD_PAWN_GOLEM_ENTRY, .variant = 6, .mode = MAP_DRYFIELD_GOLEM_PATROLS, .x = -2200, .y = 0, .z = 4000, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_ROOK_GOLEM_ENTRY, .variant = 4, .mode = MAP_DRYFIELD_GOLEM_PATROLS, .x = -6200, .y = 0, .z = 4000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldGeneralStoreVariant1Placements[6] = {
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_PATROL, .mode = 0, .x = 3470, .y = 0, .z = 3606, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_PATROL, .mode = 0, .x = 1048, .y = 0, .z = 4979, .yaw = -ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_PATROL, .mode = 0, .x = 3468, .y = 0, .z = 4772, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_PATROL, .mode = 0, .x = 1234, .y = 0, .z = 3641, .yaw = 128, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_PATROL, .mode = 0, .x = 3121, .y = 0, .z = 1869, .yaw = -128, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldBackStreetDesertChaserPlacements[3] = {
    { .entryId = MAP_DRYFIELD_DESERT_CHASER_ENTRY, .variant = MAP_DRYFIELD_CHASER_STEER, .mode = MAP_DRYFIELD_CHASER_TUNING_SECOND, .x = 10000, .y = 2, .z = 3000, .yaw = 600, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_DESERT_CHASER_ENTRY, .variant = MAP_DRYFIELD_CHASER_STEER, .mode = MAP_DRYFIELD_CHASER_TUNING_SECOND, .x = -500, .y = 2, .z = 4100, .yaw = 1200, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldBackStreetPawnGolemPlacements[3] = {
    { .entryId = MAP_DRYFIELD_PAWN_GOLEM_ENTRY, .variant = 6, .mode = MAP_DRYFIELD_GOLEM_PATROLS, .x = -1500, .y = 0, .z = 5000, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_LAUNCHER_PAWN_GOLEM_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_GOLEM_IDLE, .x = 9700, .y = 0, .z = 5700, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldSouvenirShopDefaultPlacements[4] = {
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_WALL_NO_SETTLE, .x = 1200, .y = -2000, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 5500, .y = 1, .z = -2800, .yaw = 2800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 3000, .y = 1, .z = -700, .yaw = -800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldR08Variant1Placements[2] = {
    { .entryId = MAP_DRYFIELD_AYA_DOUBLE_SCENE_ENTRY, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldDilapidatedHouseVariant1Placements[3] = {
    { .entryId = MAP_DRYFIELD_NO9_GOLEM_ENTRY, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_ANMC_WOMAN_ENTRY, .variant = 0, .mode = 0, .x = -2000, .y = 0, .z = -1800, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelRoom1Variant1Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelRoom1Variant2Placements[6] = {
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 2600, .y = 1, .z = 2800, .yaw = 1800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 1500, .y = 1, .z = 3200, .yaw = 3800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_PATROL, .mode = 0, .x = 500, .y = 1, .z = 3400, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 1000, .y = 1, .z = 5600, .yaw = 380, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_ODD_STRANGER_ENTRY, .variant = MAP_DRYFIELD_ODD_STRANGER_STALK, .mode = MAP_DRYFIELD_ODD_STRANGER_TUNING_SECOND, .x = 1800, .y = 1, .z = 3000, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelRoom1Variant3Placements[5] = {
    { .entryId = MAP_DRYFIELD_CUTSCENE_BONE_SUCKLER_ENTRY, .variant = 0, .mode = 0, .x = -3473, .y = 0, .z = 993, .yaw = ACTOR_TRANSFORM_ANGLE_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_CUTSCENE_BONE_SUCKLER_ENTRY, .variant = 0, .mode = 0, .x = -3473, .y = 0, .z = 993, .yaw = ACTOR_TRANSFORM_ANGLE_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_PATROL, .mode = 0, .x = 1720, .y = 0, .z = 2298, .yaw = 256, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_PATROL, .mode = 0, .x = 2309, .y = 0, .z = 3467, .yaw = -1000, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelRoom1Variant7Placements[6] = {
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 2600, .y = 1, .z = 2800, .yaw = 1800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 1500, .y = 1, .z = 3200, .yaw = 3800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_PATROL, .mode = 0, .x = 500, .y = 1, .z = 3400, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 1000, .y = 1, .z = 5600, .yaw = 380, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_ODD_STRANGER_ENTRY, .variant = MAP_DRYFIELD_ODD_STRANGER_STALK, .mode = MAP_DRYFIELD_ODD_STRANGER_TUNING_THIRD, .x = 1800, .y = 1, .z = 3000, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelRoom2Variant1Placements[3] = {
    { .entryId = MAP_DRYFIELD_ODD_STRANGER_ENTRY, .variant = MAP_DRYFIELD_ODD_STRANGER_STALK, .mode = MAP_DRYFIELD_ODD_STRANGER_TUNING_SECOND, .x = 2400, .y = 1, .z = 4000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_ODD_STRANGER_ENTRY, .variant = MAP_DRYFIELD_ODD_STRANGER_STALK, .mode = MAP_DRYFIELD_ODD_STRANGER_TUNING_SECOND, .x = 4500, .y = 1, .z = 5000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelRoom2Variant2Placements[7] = {
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = 2500, .y = -2600, .z = 1500, .yaw = 400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_WALL_NO_SETTLE, .x = 200, .y = -1800, .z = 2100, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 3000, .y = 1, .z = 4900, .yaw = 2600, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 4500, .y = 1, .z = 5300, .yaw = 2700, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 500, .y = 1, .z = 5000, .yaw = 3700, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 500, .y = 1, .z = 2900, .yaw = 800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelRoom2Variant7Placements[7] = {
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = 2500, .y = -2600, .z = 1500, .yaw = 400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_WALL_NO_SETTLE, .x = 200, .y = -1800, .z = 2100, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 3000, .y = 1, .z = 4900, .yaw = 2600, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 4500, .y = 1, .z = 5300, .yaw = 2700, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 500, .y = 1, .z = 5000, .yaw = 3700, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BLOOD_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 500, .y = 1, .z = 2900, .yaw = 800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldParkingLotVariant1Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldParkingLotVariant2Placements[3] = {
    { .entryId = MAP_DRYFIELD_DESERT_CHASER_ENTRY, .variant = MAP_DRYFIELD_CHASER_STEER, .mode = MAP_DRYFIELD_CHASER_TUNING_FIRST, .x = 1353, .y = 0, .z = 800, .yaw = 3840, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_DESERT_CHASER_ENTRY, .variant = MAP_DRYFIELD_CHASER_STEER, .mode = MAP_DRYFIELD_CHASER_TUNING_FIRST, .x = 5500, .y = 0, .z = 3500, .yaw = 3200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldParkingLotVariant3Placements[8] = {
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = -2300, .y = 0, .z = 1100, .yaw = 500, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 3100, .y = 0, .z = 1200, .yaw = 3600, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = -1500, .y = 0, .z = 1900, .yaw = 1200, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 100, .y = 0, .z = 3000, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 900, .y = 0, .z = 900, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 900, .y = 0, .z = 2100, .yaw = 1500, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_DESERT_CHASER_ENTRY, .variant = MAP_DRYFIELD_CHASER_APPROACH, .mode = MAP_DRYFIELD_CHASER_TUNING_SECOND, .x = -3000, .y = 0, .z = 1500, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldParkingLotVariant7Placements[2] = {
    { .entryId = MAP_DRYFIELD_KNIGHT_GOLEM_ENTRY, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldToiletEncounterPlacements[3] = {
    { .entryId = MAP_DRYFIELD_WOMAN_STRANGER_ENTRY, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_TOILET_ODD_STRANGER_ENTRY, .variant = MAP_DRYFIELD_ODD_STRANGER_STALK, .mode = MAP_DRYFIELD_ODD_STRANGER_TUNING_SECOND, .x = -1590, .y = 1, .z = -1260, .yaw = 380, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldToiletBoneSucklerPlacements[8] = {
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = -1800, .y = 1, .z = -500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = -200, .y = 1, .z = -1800, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 700, .y = 1, .z = 200, .yaw = 2500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 800, .y = 1, .z = 600, .yaw = 1500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 800, .y = 1, .z = 1000, .yaw = 200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 400, .y = 1, .z = 1200, .yaw = 2200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 900, .y = 1, .z = 1700, .yaw = 2500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B354[4] = {
    { 0x18, 0, 1, 0, 1, 0x5DC, 0x672, 0, 0, 2, 0 },
    { 0x18, 0, 1, 0x2BC, 1, 0x578, 0x8FC, 0, 0, 2, 0 },
    { 0x18, 0, 1, 0x1F4, 1, 0x44C, 0xB54, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B394[6] = {
    { 0xF, 0, 3, 0x1F4, -0xBB8, -0x898, 0x76C, 0, 0, 2, 0 },
    { 0xF, 0, 3, -0x1F4, -0xE42, -0x8FC, 0x7D0, 0, 0, 2, 0 },
    { 0xF, 0, 3, 0x4B0, -0xE42, 0, -0x190, 0, 0, 2, 0 },
    { 0xF, 0, 3, -0x3E8, -0xE42, 0, 0x800, 0, 0, 2, 0 },
    { 0xF, 0, 3, -0x6A4, -0xBB8, 0x3E8, 0x898, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B3F4[8] = {
    { 0xF, 0, 3, 0x960, -0xBB8, -0x320, 0x190, 0, 0, 2, 0 },
    { 0xF, 0, 3, 0x1F4, -0xBB8, -0x834, 0x898, 0, 0, 2, 0 },
    { 0xC, 1, 0, -0x2BC, 0, 0xDAC, 0xDAC, 0, 2, 4, 0 },
    { 0xC, 1, 0, -0x898, 0, -0x3E8, 0xB54, 0, 2, 4, 0 },
    { 0xC, 1, 0, 0, 0, 0x1F4, 0x400, 0, 2, 4, 0 },
    { 0xC, 1, 0, 0x898, 0, 0x640, 0xE74, 0, 2, 4, 0 },
    { 0xC, 1, 0, 0x578, 0, -0xC8, 0xA8C, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B474[4] = {
    { 0xF, 0, 3, 0x15E, -0xBB8, -0xFA, 0x800, 0, 0, 2, 0 },
    { 0xF, 0, 3, 0, -0xBB8, -0x1F4, 0x898, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0x1F4, -0x640, -0x9C4, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B4B4[8] = {
    { 0xF, 0, 2, -0x320, -0x640, 0, 0xC00, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0x384, -0x640, -0x708, 0x400, 0, 0, 2, 0 },
    { 7, 0, 0, 0x1F4, 0, -0x7D0, 0x76C, 0, 2, 4, 2 },
    { 7, 0, 0, 0, 0, -0x7D0, 0xA8C, 0, 2, 4, 2 },
    { 7, 0, 0, -0xC8, 0, -0x640, 0x898, 0, 2, 4, 2 },
    { 7, 0, 0, 0x258, 0, -0x7D0, 0x514, 0, 2, 4, 2 },
    { 7, 0, 0, 0x258, 0, -0x5DC, 0x640, 0, 2, 4, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B534[3] = {
    { 1, 1, 0, -0x620, 0, 0x980, 0xC00, 0, 0, 2, 0 },
    { 1, 1, 0, -0x238, 0, 0x598, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B564[8] = {
    { 0x19, 0, 2, 0, 0, -0xED8, 0xA28, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0x834, 0, 0, 0x9C4, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0x10CC, 0, 0x3E8, 0x9C4, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0x11F8, 0, -0xA8C, 0xED8, 0, 0, 2, 0 },
    { 0x19, 0, 0, -0x320, 0, -0x76C, 0x190, 0, 0, 2, 0 },
    { 0x19, 0, 0, 0x3E8, 0, -0x7D0, 0, 0, 0, 2, 0 },
    { 0x19, 0, 0, -0x320, 0, 0x320, 0x640, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B5E4[6] = {
    { 0x19, 0, 1, 0x898, 0, -0x3E8, 0x800, 0, 0, 2, 0 },
    { 0x19, 0, 1, 0x7D0, 0, 0x320, 0, 0, 0, 2, 0 },
    { 0x19, 0, 1, 0x2BC, 0, -0x708, 0xDAC, 0, 0, 2, 0 },
    { 0x19, 0, 1, 0x1F4, 0, 0x7D0, 0x9C4, 0, 0, 2, 0 },
    { 0x19, 0, 1, 0, 0, 0, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B644[2] = {
    { 0x8C, 0, 0, 0x9E3, -0x2EE5, -0x284, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B664[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B674[3] = {
    { 0x65, 0, 0, 0x4330, 1, 0xA8C, 0, 0, 0, 2, 0 },
    { 1, 0, 2, 0x4330, 1, 0xA8C, -0x600, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B6A4[8] = {
    { 0x19, 0, 2, 0x445C, 0, 0x5DC, 0xC1C, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0x2710, 0, 0x708, 0x400, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0x445C, 0, 0xB54, 0xAF0, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0x2AF8, 0, 0x514, 0x320, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0x38A4, 0, 0x898, 0xA8C, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0x34BC, 0, 0x5DC, 0x1F4, 0, 0, 2, 0 },
    { 0x19, 0, 0, 0x2F44, 0, 0x578, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B724[2] = {
    { 0x6A, 0, 0, 0x1244, 0, 0x7F6, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B744[6] = {
    { 0xF, 0, 3, 0x320, -0xE74, 0x1770, 0x800, 0, 0, 2, 0 },
    { 0xF, 0, 3, 0x7D0, -0xE74, 0x14B4, 0x898, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0x2710, -0x9C4, 0x1838, 0x400, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0x2710, -0x76C, 0x1388, 0x190, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0x2454, -0x8FC, 0x1E78, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B7A4[4] = {
    { 0xF, 0, 3, 0x320, -0xE74, 0x12C0, 0x190, 0, 0, 2, 0 },
    { 0xF, 0, 3, 0x8FC, -0xE74, 0x157C, 0xDAC, 0, 0, 2, 0 },
    { 0xF, 0, 3, 0x76C, -0xE74, 0xED8, 0xF3C, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B7E4[26] = {
    { 0x25, 0, 0xA, -0x640, 0, 0xA28, 0x980, 0, 0, 2, 0 },
    { 0x25, 0, 0xB, -0x640, 0, 0x992, 0x900, 0, 0, 2, 0 },
    { 0x25, 0, 0xB, -0x640, 0, 0x8FC, 0x880, 0, 0, 2, 0 },
    { 0x25, 0, 0xB, -0x5AA, 0, 0xA28, 0x800, 0, 0, 2, 0 },
    { 0x25, 0, 0xB, -0x5AA, 0, 0x992, 0x800, 0, 0, 2, 0 },
    { 0x25, 0, 0xB, -0x5AA, 0, 0x8FC, 0x800, 0, 0, 2, 0 },
    { 0x25, 0, 0xB, -0x514, 0, 0xA28, 0x680, 0, 0, 2, 0 },
    { 0x25, 0, 0xB, -0x514, 0, 0x992, 0x700, 0, 0, 2, 0 },
    { 0x25, 0, 0xB, -0x514, 0, 0x8FC, 0x780, 0, 0, 2, 0 },
    { 0x25, 0, 0xB, -0x5AA, 0, 0x992, 0x800, 0, 0, 2, 0 },
    { 0x25, 0, 0xC, -0x640, 0, 0x992, 0x900, 0, 0, 2, 2 },
    { 0x25, 0, 0xC, -0x5AA, 0, 0xA28, 0x800, 0, 0, 2, 2 },
    { 0x25, 0, 0xC, -0x5AA, 0, 0x992, 0x800, 0, 0, 2, 2 },
    { 0x25, 0, 0xD, -0x640, 0, 0x992, 0x900, 0, 0, 2, 1 },
    { 0x25, 0, 0xD, -0x5AA, 0, 0xA28, 0x800, 0, 0, 2, 1 },
    { 0x25, 0, 0xD, -0x5AA, 0, 0x992, 0x800, 0, 0, 2, 1 },
    { 0x25, 0, 0xD, -0x5AA, 0, 0x8FC, 0x800, 0, 0, 2, 1 },
    { 0x25, 0, 0xE, -0x640, 0, 0xA28, 0x980, 0, 0, 2, 3 },
    { 0x25, 0, 0xE, -0x640, 0, 0x992, 0x900, 0, 0, 2, 3 },
    { 0x25, 0, 0xE, -0x640, 0, 0x8FC, 0x880, 0, 0, 2, 3 },
    { 0x25, 0, 0xE, -0x5AA, 0, 0x992, 0x800, 0, 0, 2, 3 },
    { 0x25, 0, 0xE, -0x5AA, 0, 0x8FC, 0x800, 0, 0, 2, 3 },
    { 0x25, 0, 0xE, -0x514, 0, 0xA28, 0x680, 0, 0, 2, 3 },
    { 0x25, 0, 0xE, -0x514, 0, 0x992, 0x700, 0, 0, 2, 3 },
    { 0x25, 0, 0xE, -0x514, 0, 0x8FC, 0x780, 0, 0, 2, 3 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B984[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B994[2] = {
    { 1, 0, 0, 0x1244, 0, 0x7F6, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B9B4[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B9C4[5] = {
    { 0x19, 0, 2, 0xC80, 0, 0x76C, 0xC80, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0xFA0, 0, 0x3E8, 0xBB8, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0xE10, 0, 0xBB8, 0xA8C, 0, 0, 2, 0 },
    { 0x19, 0, 1, 0x4B0, 0, 0x384, 0x320, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017BA14[2] = {
    { 0x6A, 0, 0, 0xEE2, 0, -0x852, 0x200, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017BA34[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017BA44[4] = {
    { 0x12, 0x10, 0, -0x1770, -0xC80, 0x4B0, 0x800, 0, 0, 2, 0 },
    { 0x12, 0x10, 0, -0x157C, -0xC80, -0x1194, 0, 0, 0, 2, 0 },
    { 0x12, 0x10, 0, -0x157C, -0xC80, 0xDAC, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017BA84[7] = {
    { 0x19, 0, 0, -0x16A8, -0xC80, 0x2904, 0x800, 0, 0, 2, 0 },
    { 0x19, 0, 0, -0x1964, -0xC80, 0x1D4C, 0x6A4, 0, 0, 2, 0 },
    { 0x19, 0, 0, -0x1518, -0xC80, 0x1770, 0x800, 0, 0, 2, 0 },
    { 0x19, 0, 0, -0x1964, -0xC80, -0x514, 0x5E8, 0, 0, 2, 0 },
    { 0x19, 0, 0, -0x157C, -0xC80, -0xA8C, 0x200, 0, 0, 2, 0 },
    { 0x19, 0, 0, -0x17D4, -0xC80, -0x1194, 0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017BAF4[3] = {
    { 3, 0, 0, -0x1770, -0xC80, 0x4B0, 0x800, 0, 0, 2, 0 },
    { 3, 0, 1, -0x1770, -0xC80, 0x1F40, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017BB24[3] = {
    { 0x38, 0, 0, -0x7D0, -0xC80, 0x25E4, 0x800, 0, 0, 2, 0 },
    { 0x39, 6, 1, -0x157C, -0xC80, 0x7D0, 0x800, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017BB54[2] = {
    { 0x65, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017BB74[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017BB84[9] = {
    { 0x25, 0, 1, 0x2D50, -0x7D0, -0xC80, 0x258, 0, 0, 2, 2 },
    { 0x25, 0, 1, 0x2C88, -0x7D0, -0xE10, 0x190, 0, 0, 2, 2 },
    { 0x25, 0, 1, 0x2AF8, -0x7D0, -0xC1C, 0xC8, 0, 0, 2, 2 },
    { 0x25, 0, 1, 0x2968, -0x7D0, -0xCE4, 0, 0, 0, 2, 2 },
    { 0x25, 0, 2, 0x2710, -0x6A4, -0xD48, 0xC00, 0, 0, 2, 2 },
    { 0x25, 0, 2, 0x2710, -0x708, -0xC1C, 0xC00, 0, 0, 2, 2 },
    { 0x25, 0, 2, 0x2710, -0x44C, -0xDAC, 0xC00, 0, 0, 2, 2 },
    { 0x25, 0, 2, 0x2710, -0x4B0, -0xBB8, 0xC00, 0, 0, 2, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017BC14[11] = {
    { 0x25, 0, 1, 0xED8, -0x9C4, 0xE10, 0x4B0, 0, 0, 2, 2 },
    { 0x25, 0, 1, 0xCE4, -0x9C4, 0x12C, 0x2BC, 0, 0, 2, 2 },
    { 0x25, 0, 1, 0x125C, -0x9C4, 0x190, 0x190, 0, 0, 2, 2 },
    { 0x25, 0, 1, 0x1964, -0x9C4, 0x1F4, 0xED8, 0, 0, 2, 2 },
    { 0x25, 0, 1, 0x27D8, -0x9C4, 0x7D0, 0xD48, 0, 0, 2, 2 },
    { 0x25, 0, 1, 0x1E78, -0x9C4, 0x1068, 0xC80, 0, 0, 2, 2 },
    { 0x25, 0, 2, 0x1004, -0x5DC, 0x1194, 0, 0, 0, 2, 2 },
    { 0x25, 0, 2, 0xED8, -0x6A4, 0x1194, 0, 0, 0, 2, 2 },
    { 7, 0, 0, 0x1194, 0, 0x708, 0x12C, 0, 2, 4, 2 },
    { 7, 0, 0, 0x1BBC, 0, 0x1F4, 0xD48, 0, 2, 4, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017BCC4[2] = {
    { 5, 0, 0, 0x4074, -0xFA0, -0x209E, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

InventoryBattleReward D_map_dryfield_8017BCE4[13] = {
    { GAME_LOCATION_KEY(2, 2, 1, 0), { 0xA1, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(2, 2, 2, 0), { 8, 0, 0, 0 } },
    { GAME_LOCATION_KEY(2, 3, 1, 0), { 0x3A, 0, 0, 0 } },
    { GAME_LOCATION_KEY(2, 9, 1, 0), { 0, 0, 0, 0x96 } },
    { GAME_LOCATION_KEY(2, 11, 3, 0), { 0x3A, 0, 0, 0 } },
    { GAME_LOCATION_KEY(2, 15, 3, 0), { 0xA1, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(2, 16, 1, 0), { 6, 0, 0, 0xB } },
    { GAME_LOCATION_KEY(2, 19, 1, 0), { 0x41, 0, 0, 0 } },
    { GAME_LOCATION_KEY(2, 20, 1, 0), { 2, 0, 0, 6 } },
    { GAME_LOCATION_KEY(2, 22, 1, 0), { 6, 0, 0, 0 } },
    { GAME_LOCATION_KEY(2, 25, 1, 0), { 0x41, 0, 0, 0 } },
    { GAME_LOCATION_KEY(2, 38, 1, 0), { 0x3C, 0xAE, 0, 0xAD } },
    { INVENTORY_BATTLE_REWARD_LIST_END },
};

InventoryBattleReward D_map_dryfield_8017BD80[8] = {
    { GAME_LOCATION_KEY(2, 2, 7, 0), { 0x3C, 8, 0x3A, 0 } },
    { GAME_LOCATION_KEY(2, 5, 7, 0), { 2, 6, 0xA2, 0 } },
    { GAME_LOCATION_KEY(2, 15, 7, 0), { 0x42, 0xAD, 0xA2, 0 } },
    { GAME_LOCATION_KEY(2, 16, 7, 0), { 0x3E, 0, 0, 0 } },
    { GAME_LOCATION_KEY(2, 29, 7, 0), { 0xAC, 0, 0, 0 } },
    { GAME_LOCATION_KEY(2, 29, 8, 0), { 0xAA, 0xA2, 3, 0 } },
    { GAME_LOCATION_KEY(2, 38, 1, 0), { 0xAE, 0, 0, 0 } },
    { INVENTORY_BATTLE_REWARD_LIST_END },
};

StageMusicEntry D_map_dryfield_8017BDE0[274] = {
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x29, 1 },
    { 0x29, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x29, 0 },
    { 0x29, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0x31, 1 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x29, 0 },
    { 0x29, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x29, 0 },
    { 0x29, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x29, 0 },
    { 0x29, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x29, 0 },
    { 0x29, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2C, 1 },
    { 0x29, 2 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x29, 0 },
    { 0x2A, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2A, 1 },
    { 0x2A, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x29, 0 },
    { 0x29, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0x2E, 1 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0x2D, 1 },
    { 0x2D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x29, 0 },
    { 0x29, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0x29, 0 },
    { 0x2E, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0x2F, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0x2F, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2E, 3 },
    { 0xFF, 0 },
    { 0x2F, 0 },
    { 0xFF, 0 },
    { 0, 0 },
};

StageMusicEntry D_map_dryfield_8017C004[21] = {
    { 0x32, 2 },
    { 0xFF, 0 },
    { 0x17, 2 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
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
    { 0x20, 0x26 },
};
