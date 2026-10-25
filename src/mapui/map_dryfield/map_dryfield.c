#include "mapui/map_dryfield.h"

#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/battle_reward.h"
#include "gameplay/direction.h"
#include "gameplay/item_menu.h"
#include "gameplay/item_placement.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/map.h"
#include "gameplay/object_task.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
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
    MAP_DRYFIELD_GRAY_STALKER_ENTRY           = 5,
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
};

/// Room resource keys used by daytime actor layouts.
///
/// Equal keys in different rooms need not select the same actor package.
enum {
    MAP_DRYFIELD_BRAIN_STINGER_ENTRY      = 3,
    MAP_DRYFIELD_RAT_ENTRY                = 7,
    MAP_DRYFIELD_AMOEBA_ENTRY             = 24,
    MAP_DRYFIELD_BAT_ENTRY                = 37,
    MAP_DRYFIELD_BALCONY_ROOK_GOLEM_ENTRY = 57,
    MAP_DRYFIELD_MOTEL_ROOM_6_SCENE_ENTRY = 101,
    MAP_DRYFIELD_GARY_DOUGLAS_ENTRY       = 106,
    MAP_DRYFIELD_KYLE_MADIGAN_ENTRY       = 140,
};

/// Actor-specific modes and parameter rows used by these placement lists.
///
/// Bat wave modes activate at scene counter mode - 9; mode 10 also directs
/// the encounter. Bat turn rows select angular steps in 4096ths of a turn.
/// Brain Stinger modes select the room's two patrol routes.
enum {
    MAP_DRYFIELD_AMOEBA_STRONG_TUNING        = 1,
    MAP_DRYFIELD_SCORPION_ROAM               = 0,
    MAP_DRYFIELD_WATER_TOWER_CHASER_HIDDEN   = 1,
    MAP_DRYFIELD_BAT_PERCH_DROP              = 1,
    MAP_DRYFIELD_BAT_PERCH_BACK              = 2,
    MAP_DRYFIELD_BAT_WAVE_1_DIRECTOR         = 10,
    MAP_DRYFIELD_BAT_WAVE_2                  = 11,
    MAP_DRYFIELD_BAT_WAVE_3                  = 12,
    MAP_DRYFIELD_BAT_WAVE_4                  = 13,
    MAP_DRYFIELD_BAT_WAVE_5                  = 14,
    MAP_DRYFIELD_BAT_TURN_RATE_16_ROW        = 0,
    MAP_DRYFIELD_BAT_TURN_RATE_20_ROW        = 1,
    MAP_DRYFIELD_BAT_TURN_RATE_24_ROW        = 2,
    MAP_DRYFIELD_BAT_TURN_RATE_28_ROW        = 3,
    MAP_DRYFIELD_RAT_TUNING_THIRD            = 2,
    MAP_DRYFIELD_BRAIN_STINGER_PATROL_FIRST  = 0,
    MAP_DRYFIELD_BRAIN_STINGER_PATROL_SECOND = 1,
};

/// Flag-nibble IDs used by map-page availability and the contiguous marker run.
enum {
    MAP_DRYFIELD_PAGE_UNLOCK_FLAG = 0x16,
    MAP_DRYFIELD_MARKER_FLAG_BASE = 0x1CE,
};

/// Page value closing this map's marker and icon lists.
enum { MAP_DRYFIELD_MAP_LIST_END_PAGE = 0 };

/// Objective-byte values whose destination icons appear on this map.
enum {
    MAP_DRYFIELD_OBJECTIVE_GARAGE       = 0xA,
    MAP_DRYFIELD_OBJECTIVE_MOTEL_ROOM_6 = 0xB,
    MAP_DRYFIELD_OBJECTIVE_WATER_TANK   = 0xD,
    MAP_DRYFIELD_OBJECTIVE_BREEZEWAY    = 0xE,
    MAP_DRYFIELD_OBJECTIVE_DRIVEWAY     = 0xF,
    MAP_DRYFIELD_ICON_FIXED_PICTURE     = 0,
};

/// Catalog item IDs awarded by the daytime battle lists, one pack per slot.
enum {
    MAP_DRYFIELD_REWARD_RECOVERY2       = 2,
    MAP_DRYFIELD_REWARD_RECOVERY3       = 3,
    MAP_DRYFIELD_REWARD_MP_BOOST1       = 6,
    MAP_DRYFIELD_REWARD_PENICILLIN      = 8,
    MAP_DRYFIELD_REWARD_LIPSTICK        = 0xB,
    MAP_DRYFIELD_REWARD_FLARE           = 0x3A,
    MAP_DRYFIELD_REWARD_PROTEIN_CAPSULE = 0x3C,
    MAP_DRYFIELD_REWARD_EAU_DE_TOILETTE = 0x3E,
    MAP_DRYFIELD_REWARD_COMBAT_LIGHT    = 0x41,
    MAP_DRYFIELD_REWARD_HAMMER          = 0x42,
    MAP_DRYFIELD_REWARD_GUNBLADE        = 0x96,
    MAP_DRYFIELD_REWARD_9MM_HYDRA       = 0xA1,
    MAP_DRYFIELD_REWARD_9MM_SPARTAN     = 0xA2,
    MAP_DRYFIELD_REWARD_AIRBURST        = 0xAA,
    MAP_DRYFIELD_REWARD_BUCKSHOT        = 0xAC,
    MAP_DRYFIELD_REWARD_FIREFLY         = 0xAD,
    MAP_DRYFIELD_REWARD_R_SLUG          = 0xAE,
};

void mapDryfieldSetupMovieBuffers(const GameLocationKey* location)
{
    s32 pixelCount;

    gCdCmdQueue.movieVramStaging = 0;
    if (location->area == GAME_AREA_DRYFIELD_GAS_STATION) {
        pixelCount                   = gStreamMovieWidthPixels * gStreamMovieHeightRows;
        gMdecMovieExpandedBuffers[0] = (u_long*)((u8*)gStreamMovieRingBuffer + MAP_DRYFIELD_MOVIE_RING_BYTES);
        gMdecMovieExpandedBuffers[1] = gStreamMovieWorkspace;
        gMdecMovieOutputBuffers[0]   = (u_long*)((u8*)gStreamMovieWorkspace + pixelCount * 3);
        gMdecMovieOutputBuffers[1]   = (u_long*)((u8*)gStreamMovieWorkspace + pixelCount * 3 + pixelCount * 2);
    }
    gStreamMovieBufferLayoutEnd = (u8*)gMdecMovieOutputBuffers[1] + gStreamMovieWidthPixels * gStreamMovieHeightRows * 2;
    gGameSession->field_7C      = 0;
    gGameSession->field_7E      = 0;
}

GfxImageSlot gMapDryfieldImageSlots[GAME_AREA_DRYFIELD_UNDERPASS + 1] = {
    [0]                                    = GRAPHICS_IMAGE_SLOT(0x3A8E0),
    [GAME_AREA_DRYFIELD_GAS_STATION]       = GRAPHICS_IMAGE_SLOT(0x4EE30),
    [GAME_AREA_DRYFIELD_MAIN_STREET]       = GRAPHICS_IMAGE_SLOT(0x4F100),
    [GAME_AREA_DRYFIELD_GENERAL_STORE]     = GRAPHICS_IMAGE_SLOT(0x4DE10),
    [GAME_AREA_DRYFIELD_R04]               = GRAPHICS_IMAGE_SLOT(0x585E0),
    [GAME_AREA_DRYFIELD_BACK_STREET]       = GRAPHICS_IMAGE_SLOT(0x54F10),
    [GAME_AREA_DRYFIELD_SOUVENIR_SHOP]     = GRAPHICS_IMAGE_SLOT(0x56780),
    [GAME_AREA_DRYFIELD_WAREHOUSE]         = GRAPHICS_IMAGE_SLOT(0x53770),
    [GAME_AREA_DRYFIELD_R08]               = GRAPHICS_IMAGE_SLOT(0x54E70),
    [GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE] = GRAPHICS_IMAGE_SLOT(0x49580),
    [10]                                   = { NULL, 0 },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_1]      = GRAPHICS_IMAGE_SLOT(0x543A0),
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_2]      = GRAPHICS_IMAGE_SLOT(0x558B0),
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_3]      = GRAPHICS_IMAGE_SLOT(0x58030),
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_4]      = GRAPHICS_IMAGE_SLOT(0x57D70),
    [GAME_AREA_DRYFIELD_PARKING_LOT]       = GRAPHICS_IMAGE_SLOT(0x560A0),
    [GAME_AREA_DRYFIELD_TOILET]            = GRAPHICS_IMAGE_SLOT(0x4B730),
    [GAME_AREA_DRYFIELD_MOTEL_LOBBY]       = GRAPHICS_IMAGE_SLOT(0x545C0),
    [GAME_AREA_DRYFIELD_SALOON_G_R]        = GRAPHICS_IMAGE_SLOT(0x523E0),
    [GAME_AREA_DRYFIELD_G_R_KITCHEN]       = GRAPHICS_IMAGE_SLOT(0x57030),
    [GAME_AREA_DRYFIELD_WATER_TOWER]       = GRAPHICS_IMAGE_SLOT(0x4C970),
    [GAME_AREA_DRYFIELD_WATER_TANK]        = GRAPHICS_IMAGE_SLOT(0x4BD80),
    [GAME_AREA_DRYFIELD_BREEZEWAY]         = GRAPHICS_IMAGE_SLOT(0x514F0),
    [GAME_AREA_DRYFIELD_FACTORY]           = GRAPHICS_IMAGE_SLOT(0x495E0),
    [GAME_AREA_DRYFIELD_GARAGE]            = GRAPHICS_IMAGE_SLOT(0x53ED0),
    [GAME_AREA_DRYFIELD_DRIVEWAY]          = GRAPHICS_IMAGE_SLOT(0x551F0),
    [GAME_AREA_DRYFIELD_JUNK_YARD]         = GRAPHICS_IMAGE_SLOT(0x532A0),
    [GAME_AREA_DRYFIELD_TRAILER_COACH]     = GRAPHICS_IMAGE_SLOT(0x49210),
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_5]      = GRAPHICS_IMAGE_SLOT(0x57F20),
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY]     = GRAPHICS_IMAGE_SLOT(0x4D6F0),
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_6]      = GRAPHICS_IMAGE_SLOT(0x4C850),
    [GAME_AREA_DRYFIELD_MOTEL_LOFT]        = GRAPHICS_IMAGE_SLOT(0x56A60),
    [GAME_AREA_DRYFIELD_WATER_HOLE]        = GRAPHICS_IMAGE_SLOT(0x52EE0),
    [33]                                   = { NULL, 0 },
    [GAME_AREA_DRYFIELD_CELLAR]            = GRAPHICS_IMAGE_SLOT(0x541F0),
    [35]                                   = { NULL, 0 },
    [36]                                   = { NULL, 0 },
    [37]                                   = { NULL, 0 },
    [GAME_AREA_DRYFIELD_UNDERPASS]         = GRAPHICS_IMAGE_SLOT(0x54820),
};

s32 gMapDryfieldMovieWorkspaceBytesByArea[32] = {
    [GAME_AREA_DRYFIELD_GAS_STATION] = 0x2A00,
};

u8 gMapDryfieldPageFlagIds[MAP_DRYFIELD_PAGE_COUNT + 1] = { 0, MAP_DRYFIELD_PAGE_UNLOCK_FLAG, MAP_DRYFIELD_PAGE_UNLOCK_FLAG, MAP_DRYFIELD_PAGE_UNLOCK_FLAG };

MenuMapArea gMapDryfieldAreas[GAME_AREA_DRYFIELD_UNDERPASS + 2] = {
    [0]                                    = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_DRYFIELD_GAS_STATION]       = { 0x126, -0xD9A, 4, 0x2F, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_MAIN_STREET]       = { -0x1C2, -0xD6B, 0xFFFD, 0x2E, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_GENERAL_STORE]     = { 0xC1C, 0x12C, 0xC, 0x23, 0x15E, 0x122, 2 },
    [GAME_AREA_DRYFIELD_R04]               = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_DRYFIELD_BACK_STREET]       = { -0x257D, 0x123B, 0x27, 0x12, 0x113, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_SOUVENIR_SHOP]     = { 0x7D0, -0xD6D, 0x2B, 0xA, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_WAREHOUSE]         = { 0xA27, -0xDCF, 0x46, 0xA, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_R08]               = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE] = { 0x63C, -0x9EC, 0x6A, 0xB, 0x12C, 0x12C, 2 },
    [10]                                   = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_1]      = { 0x10D3, 0xD3C, 0xFFE2, 0x2D, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_2]      = { 0x1194, 0xA32, 0xFFE2, 0x10, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_3]      = { 0x116E, 0x42B, 0xFFE2, 0xFFFF, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_4]      = { 0xD48, 0x2A3, 0xFFF2, 0xFFF8, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_PARKING_LOT]       = { 0x2995, 0xC05, 0xFFE2, 0x1D, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_TOILET]            = { -0x67C, 0x675, 0xFFBA, 0x2C, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_MOTEL_LOBBY]       = { 0xB7C, 0x13C, 0xFFC4, 0x14, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_SALOON_G_R]        = { 0x952, -0x1235, 0xFFA1, 0x23, 0x12C, 0x136, 2 },
    [GAME_AREA_DRYFIELD_G_R_KITCHEN]       = { -0x4B0, 0x881, 0xFFAF, 5, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_WATER_TOWER]       = { -0x985, -0x161E, 0xFFB2, 0xFFFE, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_WATER_TANK]        = { -0x917, 0x477, 0xFFDC, 0xFFDF, 0x12C, 0x12C, 1 },
    [GAME_AREA_DRYFIELD_BREEZEWAY]         = { 0x1A00, 0x620, 0xFFD6, 0xFFE6, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_FACTORY]           = { 0x143A, 0x5AE, 0x10, 0xFFFD, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_GARAGE]            = { 0x355, 0xAB5, 0x1A, 0xFFE9, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_DRIVEWAY]          = { -0xD48, 0x258, 0x18, 0xFFFE, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_JUNK_YARD]         = { 0x13A6, 0x186, 0x1E, 0xFFD5, 0x12C, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_TRAILER_COACH]     = { 0x62B, -0x320, 0x50, 0xFFD8, 0x190, 0x12C, 2 },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_5]      = { 0x10FE, 0x97A, 0xA, 0x1D, 0x12C, 0x12C, 1 },
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY]     = { 0x315E, 0x5C6, 0x4D, 0x11, 0x12C, 0x12C, 1 },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_6]      = { 0x10FE, 0x5DC, 8, 6, 0x12C, 0x12C, 1 },
    [GAME_AREA_DRYFIELD_MOTEL_LOFT]        = { 0x1082, -0x764, 0x1F, 0xFFEA, 0x12C, 0x12C, 1 },
    [GAME_AREA_DRYFIELD_WATER_HOLE]        = { 0x1CEE, -0x5AA, 0xFFE4, 0xFFE9, 0x12C, 0x12C, 3 },
    [33]                                   = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_DRYFIELD_CELLAR]            = { 0x15EB, 0xF69, 0xFFF8, 0xC, 0x12C, 0x12C, 3 },
    [35]                                   = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [36]                                   = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [37]                                   = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_DRYFIELD_UNDERPASS]         = { 0x14B7, 0x2C94, 0xFFF9, 0xFFB6, 0x12C, 0x10E, 3 },
    [GAME_AREA_DRYFIELD_UNDERPASS + 1]     = { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_END },
};

/// Picture-page IDs selected by this stage's area shapes.
enum {
    MAP_DRYFIELD_SHAPE_PAGE_MOTEL_WATER_TANK = 1,
    MAP_DRYFIELD_SHAPE_PAGE_TOWN             = 2,
    MAP_DRYFIELD_SHAPE_PAGE_UNDERGROUND      = 3,
};

MenuMapAreaShape gMapDryfieldAreaShapes[GAME_AREA_DRYFIELD_UNDERPASS + 1] = {
    [0]                                    = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_GAS_STATION]       = { .model = &gMappicS202GasStationShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_MAIN_STREET]       = { .model = &gMappicS202MainStreetShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_GENERAL_STORE]     = { .model = &gMappicS202GeneralStoreShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_R04]               = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_BACK_STREET]       = { .model = &gMappicS202BackStreetShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_SOUVENIR_SHOP]     = { .model = &gMappicS202SouvenirShopShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_WAREHOUSE]         = { .model = &gMappicS202WarehouseShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_R08]               = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE] = { .model = &gMappicS202DilapidatedHouseShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [10]                                   = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_1]      = { .model = &gMappicS202MotelRoom1Shape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_2]      = { .model = &gMappicS202MotelRoom2Shape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_3]      = { .model = &gMappicS202MotelRoom3Shape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_4]      = { .model = &gMappicS202MotelRoom4Shape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_PARKING_LOT]       = { .model = &gMappicS202ParkingLotShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_TOILET]            = { .model = &gMappicS202ToiletShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_MOTEL_LOBBY]       = { .model = &gMappicS202MotelLobbyShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_SALOON_G_R]        = { .model = &gMappicS202SaloonGRShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_G_R_KITCHEN]       = { .model = &gMappicS202GRKitchenShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_WATER_TOWER]       = { .model = &gMappicS202WaterTowerShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_WATER_TANK]        = { .model = &gMappicS200WaterTankShape, .page = MAP_DRYFIELD_SHAPE_PAGE_MOTEL_WATER_TANK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_BREEZEWAY]         = { .model = &gMappicS202BreezewayShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_FACTORY]           = { .model = &gMappicS202FactoryShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_GARAGE]            = { .model = &gMappicS202GarageShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_DRIVEWAY]          = { .model = &gMappicS202DrivewayShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_JUNK_YARD]         = { .model = &gMappicS202JunkYardShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_TRAILER_COACH]     = { .model = &gMappicS202TrailerCoachShape, .page = MAP_DRYFIELD_SHAPE_PAGE_TOWN, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_5]      = { .model = &gMappicS200MotelRoom5Shape, .page = MAP_DRYFIELD_SHAPE_PAGE_MOTEL_WATER_TANK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY]     = { .model = &gMappicS200MotelBalconyShape, .page = MAP_DRYFIELD_SHAPE_PAGE_MOTEL_WATER_TANK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_6]      = { .model = &gMappicS200MotelRoom6Shape, .page = MAP_DRYFIELD_SHAPE_PAGE_MOTEL_WATER_TANK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_MOTEL_LOFT]        = { .model = &gMappicS200MotelLoftShape, .page = MAP_DRYFIELD_SHAPE_PAGE_MOTEL_WATER_TANK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_WATER_HOLE]        = { .model = &gMappicS203WaterHoleShape, .page = MAP_DRYFIELD_SHAPE_PAGE_UNDERGROUND, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [33]                                   = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_CELLAR]            = { .model = &gMappicS203CellarShape, .page = MAP_DRYFIELD_SHAPE_PAGE_UNDERGROUND, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [35]                                   = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [36]                                   = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [37]                                   = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_DRYFIELD_UNDERPASS]         = { .model = &gMappicS203UnderpassShape, .page = MAP_DRYFIELD_SHAPE_PAGE_UNDERGROUND, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
};

MenuMapMarker gMapDryfieldMarkers[MAP_DRYFIELD_MARKER_COUNT + 1] = {
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
    { .page = MAP_DRYFIELD_MAP_LIST_END_PAGE },
};

MenuMapIcon gMapDryfieldIcons[11] = {
    { .page = 2, .area = GAME_AREA_DRYFIELD_GAS_STATION, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = 0, .x = 19, .y = 38 },
    { .page = 2, .area = GAME_AREA_DRYFIELD_GAS_STATION, .kind = MAP_DRYFIELD_ICON_FIXED_PICTURE, .condition = 0, .x = 53, .y = 54 },
    { .page = 2, .area = GAME_AREA_DRYFIELD_TRAILER_COACH, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = 0, .x = 79, .y = -38 },
    { .page = 1, .area = GAME_AREA_DRYFIELD_MOTEL_ROOM_6, .kind = MAP_DRYFIELD_ICON_FIXED_PICTURE, .condition = 0, .x = 0, .y = 7 },
    { .page = 1, .area = GAME_AREA_DRYFIELD_MOTEL_ROOM_6, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = 0, .x = 0, .y = -5 },
    { .page = 2, .area = GAME_AREA_DRYFIELD_GARAGE, .kind = MENU_MAP_ICON_KIND_OBJECTIVE, .condition = MAP_DRYFIELD_OBJECTIVE_GARAGE, .x = 41, .y = -33 },
    { .page = 1, .area = GAME_AREA_DRYFIELD_MOTEL_ROOM_6, .kind = MENU_MAP_ICON_KIND_OBJECTIVE, .condition = MAP_DRYFIELD_OBJECTIVE_MOTEL_ROOM_6, .x = 3, .y = -3 },
    { .page = 1, .area = GAME_AREA_DRYFIELD_WATER_TANK, .kind = MENU_MAP_ICON_KIND_OBJECTIVE, .condition = MAP_DRYFIELD_OBJECTIVE_WATER_TANK, .x = -27, .y = -31 },
    { .page = 2, .area = GAME_AREA_DRYFIELD_BREEZEWAY, .kind = MENU_MAP_ICON_KIND_OBJECTIVE, .condition = MAP_DRYFIELD_OBJECTIVE_BREEZEWAY, .x = -8, .y = -25 },
    { .page = 2, .area = GAME_AREA_DRYFIELD_DRIVEWAY, .kind = MENU_MAP_ICON_KIND_OBJECTIVE, .condition = MAP_DRYFIELD_OBJECTIVE_DRIVEWAY, .x = 33, .y = -6 },
    { .page = MAP_DRYFIELD_MAP_LIST_END_PAGE },
};

MenuMapAreaName gMapDryfieldAreaNames[GAME_AREA_DRYFIELD_UNDERPASS] = {
    [GAME_AREA_DRYFIELD_GAS_STATION - 1]       = { "Gas station" },
    [GAME_AREA_DRYFIELD_MAIN_STREET - 1]       = { "Main street" },
    [GAME_AREA_DRYFIELD_GENERAL_STORE - 1]     = { "General store" },
    [GAME_AREA_DRYFIELD_R04 - 1]               = { "" },
    [GAME_AREA_DRYFIELD_BACK_STREET - 1]       = { "Back street" },
    [GAME_AREA_DRYFIELD_SOUVENIR_SHOP - 1]     = { "Souvenir shop" },
    [GAME_AREA_DRYFIELD_WAREHOUSE - 1]         = { "Warehouse" },
    [GAME_AREA_DRYFIELD_R08 - 1]               = { "" },
    [GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE - 1] = { "Dilapidated house" },
    [10 - 1]                                   = { "" },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_1 - 1]      = { "Motel room 1" },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_2 - 1]      = { "Motel room 2" },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_3 - 1]      = { "" },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_4 - 1]      = { "" },
    [GAME_AREA_DRYFIELD_PARKING_LOT - 1]       = { "Parking lot" },
    [GAME_AREA_DRYFIELD_TOILET - 1]            = { "Toilet" },
    [GAME_AREA_DRYFIELD_MOTEL_LOBBY - 1]       = { "" },
    [GAME_AREA_DRYFIELD_SALOON_G_R - 1]        = { "Saloon G & R" },
    [GAME_AREA_DRYFIELD_G_R_KITCHEN - 1]       = { "G & R Kitchen" },
    [GAME_AREA_DRYFIELD_WATER_TOWER - 1]       = { "Water tower" },
    [GAME_AREA_DRYFIELD_WATER_TANK - 1]        = { "Water tank" },
    [GAME_AREA_DRYFIELD_BREEZEWAY - 1]         = { "Breezeway" },
    [GAME_AREA_DRYFIELD_FACTORY - 1]           = { "Factory" },
    [GAME_AREA_DRYFIELD_GARAGE - 1]            = { "Garage" },
    [GAME_AREA_DRYFIELD_DRIVEWAY - 1]          = { "Driveway" },
    [GAME_AREA_DRYFIELD_JUNK_YARD - 1]         = { "Junk yard" },
    [GAME_AREA_DRYFIELD_TRAILER_COACH - 1]     = { "Trailer coach" },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_5 - 1]      = { "" },
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY - 1]     = { "Motel balcony" },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_6 - 1]      = { "Motel room 6" },
    [GAME_AREA_DRYFIELD_MOTEL_LOFT - 1]        = { "" },
    [GAME_AREA_DRYFIELD_WATER_HOLE - 1]        = { "Water hole" },
    [33 - 1]                                   = { "" },
    [GAME_AREA_DRYFIELD_CELLAR - 1]            = { "Cellar" },
    [35 - 1]                                   = { "" },
    [36 - 1]                                   = { "" },
    [37 - 1]                                   = { "" },
    [GAME_AREA_DRYFIELD_UNDERPASS - 1]         = { "Underpass" },
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
    { .kind = INVENTORY_COLLECTION_ID_WIRE_ROPE, .taskDesc = { .header = { .fields = { TASK_BODY_TMD, TASK_PRIORITY_REDRAW } }, .callback = dryfieldJunkYardWireRopeTask, .data = { .model = &gDryfieldJunkYardModel01378 } } },
    { .kind = AREA_OBJECT_SPAWN_END },
};

/// M950 pickup container-lid recipe for the daytime Trailer Coach.
///
/// Matches the corresponding placed-object kind, then `AREA_OBJECT_SPAWN_END`.
/// Borrows the room-owned model geometry until the spawned task ends.
static AreaObjectSpawn _gMapDryfieldTrailerCoachObjectSpawns[2] = {
    { .kind = MAP_DRYFIELD_M950_OBJECT_KIND, .taskDesc = { .header = { .fields = { TASK_BODY_TMD, TASK_PRIORITY_REDRAW } }, .callback = itemPickupContainerLidTask, .data = { .model = &gDryfieldTrailerCoachItemBoxModel } } },
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

TaskDesc gMapDryfieldRoomTaskDescriptors[32] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldGasStationRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldMainStreetTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldGeneralStoreRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GENERAL_STORE, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldBackStreetRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BACK_STREET, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldSouvenirShopRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_SOUVENIR_SHOP, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldWarehouseRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WAREHOUSE, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldDilapidatedHouseRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldMotelRoom1Task, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_1, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldMotelRoom2Task, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_2, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldMotelRoom3Task, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_3, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldMotelRoom4Task, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_4, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldParkingLotRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldToiletRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldMotelLobbyTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_LOBBY, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldSaloonGRRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_SALOON_G_R, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldGRKitchenRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_G_R_KITCHEN, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldWaterTowerRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldWaterTankRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldBreezewayMessageTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldFactoryEntryTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldGarageRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GARAGE, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldDrivewayRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldJunkYardRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_JUNK_YARD, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldTrailerCoachRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TRAILER_COACH, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldMotelRoom5Task, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_5, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldMotelBalconyRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_BALCONY, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldMotelRoom6Task, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_6, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldMotelLoftTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_LOFT, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldWaterHoleRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_HOLE, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldCellarRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_CELLAR, 0) } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, dryfieldUnderpassRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_UNDERPASS, 0) } },
    { { { TASK_DESC_END, TASK_PRIORITY_SCENE } }, NULL, { 0 } },
};

u16 gMapDryfieldMarkerFlags[MAP_DRYFIELD_MARKER_COUNT] = {
    MAP_DRYFIELD_MARKER_FLAG_BASE + 27,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 26,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 25,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 24,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 23,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 22,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 21,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 20,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 19,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 18,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 17,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 16,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 15,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 14,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 13,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 12,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 11,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 10,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 9,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 8,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 7,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 6,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 5,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 4,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 3,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 2,
    MAP_DRYFIELD_MARKER_FLAG_BASE + 1,
    MAP_DRYFIELD_MARKER_FLAG_BASE,
    GAME_FLAG_MAP_MARK_WATER,
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

DirectionWarpEntry* gMapDryfieldAreaWarpEntries[GAME_AREA_DRYFIELD_UNDERPASS] = {
    [GAME_AREA_DRYFIELD_GAS_STATION - 1]       = gDryfieldGasStationWarpEntries,
    [GAME_AREA_DRYFIELD_MAIN_STREET - 1]       = gDryfieldMainStreetWarpEntries,
    [GAME_AREA_DRYFIELD_GENERAL_STORE - 1]     = gDryfieldGeneralStoreWarpEntries,
    [GAME_AREA_DRYFIELD_R04 - 1]               = gDryfieldR04WarpEntries,
    [GAME_AREA_DRYFIELD_BACK_STREET - 1]       = gDryfieldBackStreetWarpEntries,
    [GAME_AREA_DRYFIELD_SOUVENIR_SHOP - 1]     = gDryfieldSouvenirShopWarpEntries,
    [GAME_AREA_DRYFIELD_WAREHOUSE - 1]         = gDryfieldWarehouseWarpEntries,
    [GAME_AREA_DRYFIELD_R08 - 1]               = gDryfieldR08WarpEntries,
    [GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE - 1] = gDryfieldDilapidatedHouseWarpEntries,
    [9]                                        = NULL,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_1 - 1]      = gDryfieldMotelRoom1WarpEntries,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_2 - 1]      = gDryfieldMotelRoom2WarpEntries,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_3 - 1]      = gDryfieldMotelRoom3WarpEntries,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_4 - 1]      = gDryfieldMotelRoom4WarpEntries,
    [GAME_AREA_DRYFIELD_PARKING_LOT - 1]       = gDryfieldParkingLotWarpEntries,
    [GAME_AREA_DRYFIELD_TOILET - 1]            = gDryfieldToiletWarpEntries,
    [GAME_AREA_DRYFIELD_MOTEL_LOBBY - 1]       = gDryfieldMotelLobbyWarpEntries,
    [GAME_AREA_DRYFIELD_SALOON_G_R - 1]        = gDryfieldSaloonGRWarpEntries,
    [GAME_AREA_DRYFIELD_G_R_KITCHEN - 1]       = gDryfieldGRKitchenWarpEntries,
    [GAME_AREA_DRYFIELD_WATER_TOWER - 1]       = gDryfieldWaterTowerWarpEntries,
    [GAME_AREA_DRYFIELD_WATER_TANK - 1]        = gDryfieldWaterTankWarpEntries,
    [GAME_AREA_DRYFIELD_BREEZEWAY - 1]         = gDryfieldBreezewayWarpEntries,
    [GAME_AREA_DRYFIELD_FACTORY - 1]           = gDryfieldFactoryWarpEntries,
    [GAME_AREA_DRYFIELD_GARAGE - 1]            = gDryfieldGarageWarpEntries,
    [GAME_AREA_DRYFIELD_DRIVEWAY - 1]          = gDryfieldDrivewayWarpEntries,
    [GAME_AREA_DRYFIELD_JUNK_YARD - 1]         = gDryfieldJunkYardWarpEntries,
    [GAME_AREA_DRYFIELD_TRAILER_COACH - 1]     = gDryfieldTrailerCoachWarpEntries,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_5 - 1]      = gDryfieldMotelRoom5WarpEntries,
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY - 1]     = gDryfieldMotelBalconyWarpEntries,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_6 - 1]      = gDryfieldMotelRoom6WarpEntries,
    [GAME_AREA_DRYFIELD_MOTEL_LOFT - 1]        = gDryfieldMotelLoftWarpEntries,
    [GAME_AREA_DRYFIELD_WATER_HOLE - 1]        = gDryfieldWaterHoleWarpEntries,
    [32]                                       = NULL,
    [GAME_AREA_DRYFIELD_CELLAR - 1]            = gDryfieldCellarWarpEntries,
    [34]                                       = NULL,
    [35]                                       = NULL,
    [36]                                       = NULL,
    [GAME_AREA_DRYFIELD_UNDERPASS - 1]         = gDryfieldUnderpassWarpEntries,
};

/// Borrowed per-room logical-view counts for daytime Dryfield.
///
/// Indexed by area - 1 for areas 1..38; areas 10, 33 and 35..37 are NULL.
/// Populated arrays use room - 1 and their own extents, without a terminator.
/// The selected room overlay owns the signed-halfword element counts and must
/// remain loaded during lookup; counts do not measure mapped resource capacity.
static ViewCount* _gMapDryfieldRoomViewCountsByArea[GAME_AREA_DRYFIELD_UNDERPASS] = {
    [GAME_AREA_DRYFIELD_GAS_STATION - 1]       = gDryfieldGasStationRoomViewCounts,
    [GAME_AREA_DRYFIELD_MAIN_STREET - 1]       = gDryfieldMainStreetRoomViewCounts,
    [GAME_AREA_DRYFIELD_GENERAL_STORE - 1]     = gDryfieldGeneralStoreRoomViewCounts,
    [GAME_AREA_DRYFIELD_R04 - 1]               = gDryfieldR04RoomViewCounts,
    [GAME_AREA_DRYFIELD_BACK_STREET - 1]       = gDryfieldBackStreetRoomViewCounts,
    [GAME_AREA_DRYFIELD_SOUVENIR_SHOP - 1]     = gDryfieldSouvenirShopRoomViewCounts,
    [GAME_AREA_DRYFIELD_WAREHOUSE - 1]         = gDryfieldWarehouseRoomViewCounts,
    [GAME_AREA_DRYFIELD_R08 - 1]               = gDryfieldR08RoomViewCounts,
    [GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE - 1] = gDryfieldDilapidatedHouseRoomViewCounts,
    [10 - 1]                                   = NULL,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_1 - 1]      = gDryfieldMotelRoom1RoomViewCounts,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_2 - 1]      = gDryfieldMotelRoom2RoomViewCounts,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_3 - 1]      = gDryfieldMotelRoom3RoomViewCounts,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_4 - 1]      = gDryfieldMotelRoom4RoomViewCounts,
    [GAME_AREA_DRYFIELD_PARKING_LOT - 1]       = gDryfieldParkingLotRoomViewCounts,
    [GAME_AREA_DRYFIELD_TOILET - 1]            = gDryfieldToiletRoomViewCounts,
    [GAME_AREA_DRYFIELD_MOTEL_LOBBY - 1]       = gDryfieldMotelLobbyRoomViewCounts,
    [GAME_AREA_DRYFIELD_SALOON_G_R - 1]        = gDryfieldSaloonGRRoomViewCounts,
    [GAME_AREA_DRYFIELD_G_R_KITCHEN - 1]       = gDryfieldGRKitchenViewCounts,
    [GAME_AREA_DRYFIELD_WATER_TOWER - 1]       = gDryfieldWaterTowerRoomViewCounts,
    [GAME_AREA_DRYFIELD_WATER_TANK - 1]        = gDryfieldWaterTankRoomViewCounts,
    [GAME_AREA_DRYFIELD_BREEZEWAY - 1]         = gDryfieldBreezewayViewCounts,
    [GAME_AREA_DRYFIELD_FACTORY - 1]           = gDryfieldFactoryRoomViewCounts,
    [GAME_AREA_DRYFIELD_GARAGE - 1]            = gDryfieldGarageRoomViewCounts,
    [GAME_AREA_DRYFIELD_DRIVEWAY - 1]          = gDryfieldDrivewayRoomViewCounts,
    [GAME_AREA_DRYFIELD_JUNK_YARD - 1]         = gDryfieldJunkYardRoomViewCounts,
    [GAME_AREA_DRYFIELD_TRAILER_COACH - 1]     = gDryfieldTrailerCoachViewCounts,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_5 - 1]      = gDryfieldMotelRoom5RoomViewCounts,
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY - 1]     = gDryfieldMotelBalconyRoomViewCounts,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_6 - 1]      = gDryfieldMotelRoom6RoomViewCounts,
    [GAME_AREA_DRYFIELD_MOTEL_LOFT - 1]        = gDryfieldMotelLoftRoomViewCounts,
    [GAME_AREA_DRYFIELD_WATER_HOLE - 1]        = gDryfieldWaterHoleRoomViewCounts,
    [33 - 1]                                   = NULL,
    [GAME_AREA_DRYFIELD_CELLAR - 1]            = gDryfieldCellarRoomViewCounts,
    [35 - 1]                                   = NULL,
    [36 - 1]                                   = NULL,
    [37 - 1]                                   = NULL,
    [GAME_AREA_DRYFIELD_UNDERPASS - 1]         = gDryfieldUnderpassRoomViewCounts,
};

ViewCountTable gMapDryfieldViewCountTable = { .viewCounts = _gMapDryfieldRoomViewCountsByArea };

/// Per-area collision room arrays for daytime Dryfield.
///
/// Select area - 1 for IDs 1..38. Areas 10, 33, 35, 36, 37 are NULL.
/// Each array uses room - 1 within its own extent, without a terminator.
/// Descriptors may be map-owned or borrowed; their mutable room resources must
/// remain loaded until the active grid and linked lists are retired.
static WorldCollisionRoomResources* _gMapDryfieldWorldCollisionAreaRooms[GAME_AREA_DRYFIELD_UNDERPASS] = {
    [GAME_AREA_DRYFIELD_GAS_STATION - 1]       = gDryfieldGasStationRoomCollisionResources,
    [GAME_AREA_DRYFIELD_MAIN_STREET - 1]       = gDryfieldMainStreetRoomCollisionResources,
    [GAME_AREA_DRYFIELD_GENERAL_STORE - 1]     = gDryfieldGeneralStoreRoomCollisionResources,
    [GAME_AREA_DRYFIELD_R04 - 1]               = gDryfieldR04RoomCollisionResources,
    [GAME_AREA_DRYFIELD_BACK_STREET - 1]       = gDryfieldBackStreetRoomCollisionResources,
    [GAME_AREA_DRYFIELD_SOUVENIR_SHOP - 1]     = gDryfieldSouvenirShopRoomCollisionResources,
    [GAME_AREA_DRYFIELD_WAREHOUSE - 1]         = gDryfieldWarehouseRoomCollisionResources,
    [GAME_AREA_DRYFIELD_R08 - 1]               = gDryfieldR08RoomCollisionResources,
    [GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE - 1] = gDryfieldDilapidatedHouseRoomCollisionResources,
    [9]                                        = NULL,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_1 - 1]      = gDryfieldMotelRoom1RoomCollisionResources,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_2 - 1]      = gDryfieldMotelRoom2RoomCollisionResources,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_3 - 1]      = gDryfieldMotelRoom3RoomCollisionResources,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_4 - 1]      = gDryfieldMotelRoom4RoomCollisionResources,
    [GAME_AREA_DRYFIELD_PARKING_LOT - 1]       = gDryfieldParkingLotRoomCollisionResources,
    [GAME_AREA_DRYFIELD_TOILET - 1]            = gDryfieldToiletRoomCollisionResources,
    [GAME_AREA_DRYFIELD_MOTEL_LOBBY - 1]       = gDryfieldMotelLobbyRoomCollisionResources,
    [GAME_AREA_DRYFIELD_SALOON_G_R - 1]        = gDryfieldSaloonGRRoomCollisionResources,
    [GAME_AREA_DRYFIELD_G_R_KITCHEN - 1]       = gDryfieldGRKitchenRoomCollisionResources,
    [GAME_AREA_DRYFIELD_WATER_TOWER - 1]       = gDryfieldWaterTowerRoomCollisionResources,
    [GAME_AREA_DRYFIELD_WATER_TANK - 1]        = gDryfieldWaterTankRoomCollisionResources,
    [GAME_AREA_DRYFIELD_BREEZEWAY - 1]         = gDryfieldBreezewayRoomCollisionResources,
    [GAME_AREA_DRYFIELD_FACTORY - 1]           = gDryfieldFactoryRoomCollisionResources,
    [GAME_AREA_DRYFIELD_GARAGE - 1]            = gDryfieldGarageRoomCollisionResources,
    [GAME_AREA_DRYFIELD_DRIVEWAY - 1]          = gDryfieldDrivewayRoomCollisionResources,
    [GAME_AREA_DRYFIELD_JUNK_YARD - 1]         = gDryfieldJunkYardRoomCollisionResources,
    [GAME_AREA_DRYFIELD_TRAILER_COACH - 1]     = gDryfieldTrailerCoachRoomCollisionResources,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_5 - 1]      = gDryfieldMotelRoom5RoomCollisionResources,
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY - 1]     = gDryfieldMotelBalconyRoomCollisionResources,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_6 - 1]      = gDryfieldMotelRoom6RoomCollisionResources,
    [GAME_AREA_DRYFIELD_MOTEL_LOFT - 1]        = gDryfieldMotelLoftRoomCollisionResources,
    [GAME_AREA_DRYFIELD_WATER_HOLE - 1]        = gDryfieldWaterHoleRoomCollisionResources,
    [32]                                       = NULL,
    [GAME_AREA_DRYFIELD_CELLAR - 1]            = gDryfieldCellarRoomCollisionResources,
    [34]                                       = NULL,
    [35]                                       = NULL,
    [36]                                       = NULL,
    [GAME_AREA_DRYFIELD_UNDERPASS - 1]         = gDryfieldUnderpassRoomCollisionResources,
};

WorldCollisionStageResources gMapDryfieldWorldCollisionResources = { .areaRooms = _gMapDryfieldWorldCollisionAreaRooms };

/// Borrowed room camera arrays, indexed by area minus one.
///
/// Each populated area requires its loaded room and a mapped-camera index
/// within that room's camera array. Absent area slots remain NULL.
static ViewCamera* _gMapDryfieldAreaViewCameras[GAME_AREA_DRYFIELD_UNDERPASS] = {
    [GAME_AREA_DRYFIELD_GAS_STATION - 1]       = gDryfieldGasStationViewCameras,
    [GAME_AREA_DRYFIELD_MAIN_STREET - 1]       = gDryfieldMainStreetViewCameras,
    [GAME_AREA_DRYFIELD_GENERAL_STORE - 1]     = gDryfieldGeneralStoreViewCameras,
    [GAME_AREA_DRYFIELD_R04 - 1]               = gDryfieldR04ViewCameras,
    [GAME_AREA_DRYFIELD_BACK_STREET - 1]       = gDryfieldBackStreetViewCameras,
    [GAME_AREA_DRYFIELD_SOUVENIR_SHOP - 1]     = gDryfieldSouvenirShopViewCameras,
    [GAME_AREA_DRYFIELD_WAREHOUSE - 1]         = gDryfieldWarehouseViewCameras,
    [GAME_AREA_DRYFIELD_R08 - 1]               = gDryfieldR08ViewCameras,
    [GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE - 1] = gDryfieldDilapidatedHouseViewCameras,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_1 - 1]      = gDryfieldMotelRoom1ViewCameras,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_2 - 1]      = gDryfieldMotelRoom2ViewCameras,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_3 - 1]      = gDryfieldMotelRoom3ViewCameras,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_4 - 1]      = gDryfieldMotelRoom4ViewCameras,
    [GAME_AREA_DRYFIELD_PARKING_LOT - 1]       = gDryfieldParkingLotViewCameras,
    [GAME_AREA_DRYFIELD_TOILET - 1]            = gDryfieldToiletViewCameras,
    [GAME_AREA_DRYFIELD_MOTEL_LOBBY - 1]       = gDryfieldMotelLobbyViewCameras,
    [GAME_AREA_DRYFIELD_SALOON_G_R - 1]        = gDryfieldSaloonGRViewCameras,
    [GAME_AREA_DRYFIELD_G_R_KITCHEN - 1]       = gDryfieldGRKitchenViewCameras,
    [GAME_AREA_DRYFIELD_WATER_TOWER - 1]       = gDryfieldWaterTowerViewCameras,
    [GAME_AREA_DRYFIELD_WATER_TANK - 1]        = gDryfieldWaterTankViewCameras,
    [GAME_AREA_DRYFIELD_BREEZEWAY - 1]         = gDryfieldBreezewayViewCameras,
    [GAME_AREA_DRYFIELD_FACTORY - 1]           = gDryfieldFactoryViewCameras,
    [GAME_AREA_DRYFIELD_GARAGE - 1]            = gDryfieldGarageViewCameras,
    [GAME_AREA_DRYFIELD_DRIVEWAY - 1]          = gDryfieldDrivewayViewCameras,
    [GAME_AREA_DRYFIELD_JUNK_YARD - 1]         = gDryfieldJunkYardViewCameras,
    [GAME_AREA_DRYFIELD_TRAILER_COACH - 1]     = gDryfieldTrailerCoachViewCameras,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_5 - 1]      = gDryfieldMotelRoom5ViewCameras,
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY - 1]     = gDryfieldMotelBalconyViewCameras,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_6 - 1]      = gDryfieldMotelRoom6ViewCameras,
    [GAME_AREA_DRYFIELD_MOTEL_LOFT - 1]        = gDryfieldMotelLoftViewCameras,
    [GAME_AREA_DRYFIELD_WATER_HOLE - 1]        = gDryfieldWaterHoleViewCameras,
    [GAME_AREA_DRYFIELD_CELLAR - 1]            = gDryfieldCellarViewCameras,
    [GAME_AREA_DRYFIELD_UNDERPASS - 1]         = gDryfieldUnderpassViewCameras,
};

ViewCameraTable gMapDryfieldViewCameraTable = { .cameras = _gMapDryfieldAreaViewCameras };

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
    gDryfieldGasStationSpriteViews,
    gDryfieldMainStreetSpriteViews,
    gDryfieldGeneralStoreSpriteViews,
    gDryfieldR04SpriteViews,
    gDryfieldBackStreetSpriteViews,
    gDryfieldSouvenirShopSpriteViews,
    gDryfieldWarehouseSpriteViews,
    gDryfieldR08SpriteViews,
    gDryfieldDilapidatedHouseSpriteViews,
    NULL,
    gDryfieldMotelRoom1SpriteViews,
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

AreaPlacement gMapDryfieldToiletAmoebaPlacements[4] = {
    { .entryId = MAP_DRYFIELD_AMOEBA_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_AMOEBA_STRONG_TUNING, .x = 0, .y = 1, .z = 1500, .yaw = 1650, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_AMOEBA_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_AMOEBA_STRONG_TUNING, .x = 700, .y = 1, .z = 1400, .yaw = 2300, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_AMOEBA_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_AMOEBA_STRONG_TUNING, .x = 500, .y = 1, .z = 1100, .yaw = 2900, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldSaloonGRVariant1Placements[6] = {
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = 500, .y = -3000, .z = -2200, .yaw = 1900, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = -500, .y = -3650, .z = -2300, .yaw = 2000, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = 1200, .y = -3650, .z = 0, .yaw = -400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = -1000, .y = -3650, .z = 0, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = -1700, .y = -3000, .z = 1000, .yaw = 2200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldSaloonGRVariant2Placements[8] = {
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = 2400, .y = -3000, .z = -800, .yaw = 400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = 500, .y = -3000, .z = -2100, .yaw = 2200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = -700, .y = 0, .z = 3500, .yaw = 3500, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = -2200, .y = 0, .z = -1000, .yaw = 2900, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 0, .y = 0, .z = 500, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 2200, .y = 0, .z = 1600, .yaw = 3700, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BONE_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_SUCKLER_IDLE, .mode = 0, .x = 1400, .y = 0, .z = -200, .yaw = 2700, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldGRKitchenVariant1Placements[4] = {
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = 350, .y = -3000, .z = -250, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = 0, .y = -3000, .z = -500, .yaw = 2200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_WALL_NO_SETTLE, .x = 500, .y = -1600, .z = -2500, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldGRKitchenVariant2Placements[8] = {
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_WALL_NO_SETTLE, .x = -800, .y = -1600, .z = 0, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_WALL_NO_SETTLE, .x = 900, .y = -1600, .z = -1800, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_RAT_ENTRY, .variant = 0, .mode = 0, .x = 500, .y = 0, .z = -2000, .yaw = 1900, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_RAT_TUNING_THIRD },
    { .entryId = MAP_DRYFIELD_RAT_ENTRY, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = -2000, .yaw = 2700, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_RAT_TUNING_THIRD },
    { .entryId = MAP_DRYFIELD_RAT_ENTRY, .variant = 0, .mode = 0, .x = -200, .y = 0, .z = -1600, .yaw = 2200, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_RAT_TUNING_THIRD },
    { .entryId = MAP_DRYFIELD_RAT_ENTRY, .variant = 0, .mode = 0, .x = 600, .y = 0, .z = -2000, .yaw = 1300, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_RAT_TUNING_THIRD },
    { .entryId = MAP_DRYFIELD_RAT_ENTRY, .variant = 0, .mode = 0, .x = 600, .y = 0, .z = -1500, .yaw = 1600, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_RAT_TUNING_THIRD },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldWaterTowerVariant1Placements[3] = {
    { .entryId = MAP_DRYFIELD_DESERT_CHASER_ENTRY, .variant = MAP_DRYFIELD_WATER_TOWER_CHASER_HIDDEN, .mode = MAP_DRYFIELD_CHASER_TUNING_SECOND, .x = -1568, .y = 0, .z = 2432, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_DESERT_CHASER_ENTRY, .variant = MAP_DRYFIELD_WATER_TOWER_CHASER_HIDDEN, .mode = MAP_DRYFIELD_CHASER_TUNING_SECOND, .x = -568, .y = 0, .z = 1432, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldWaterTowerVariant2Placements[8] = {
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 0, .y = 0, .z = -3800, .yaw = 2600, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 2100, .y = 0, .z = 0, .yaw = 2500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 4300, .y = 0, .z = 1000, .yaw = 2500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 4600, .y = 0, .z = -2700, .yaw = 3800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_ROAM, .x = -800, .y = 0, .z = -1900, .yaw = 400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_ROAM, .x = 1000, .y = 0, .z = -2000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_ROAM, .x = -800, .y = 0, .z = 800, .yaw = 1600, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldWaterTowerVariant3Placements[6] = {
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_LEADER, .x = 2200, .y = 0, .z = -1000, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_LEADER, .x = 2000, .y = 0, .z = 800, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_LEADER, .x = 700, .y = 0, .z = -1800, .yaw = 3500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_LEADER, .x = 500, .y = 0, .z = 2000, .yaw = 2500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_LEADER, .x = 0, .y = 0, .z = 0, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldWaterTankVariant1Placements[2] = {
    { .entryId = MAP_DRYFIELD_KYLE_MADIGAN_ENTRY, .variant = 0, .mode = 0, .x = 2531, .y = -12005, .z = -644, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldWaterTankVariant2Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldBreezewayVariant1Placements[3] = {
    { .entryId = MAP_DRYFIELD_CUTSCENE_DESERT_CHASER_ENTRY, .variant = 0, .mode = 0, .x = 17200, .y = 1, .z = 2700, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_DESERT_CHASER_ENTRY, .variant = MAP_DRYFIELD_CHASER_APPROACH, .mode = MAP_DRYFIELD_CHASER_TUNING_FIRST, .x = 17200, .y = 1, .z = 2700, .yaw = -ACTOR_TRANSFORM_ANGLE_TURN * 3 / 8, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldBreezewayVariant3Placements[8] = {
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 17500, .y = 0, .z = 1500, .yaw = 3100, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 10000, .y = 0, .z = 1800, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 17500, .y = 0, .z = 2900, .yaw = 2800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 11000, .y = 0, .z = 1300, .yaw = 800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 14500, .y = 0, .z = 2200, .yaw = 2700, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 13500, .y = 0, .z = 1500, .yaw = 500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_ROAM, .x = 12100, .y = 0, .z = 1400, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldGarageVariant1Placements[2] = {
    { .entryId = MAP_DRYFIELD_GARY_DOUGLAS_ENTRY, .variant = 0, .mode = 0, .x = 4676, .y = 0, .z = 2038, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldGarageVariant2Placements[6] = {
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = 800, .y = -3700, .z = 6000, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = 2000, .y = -3700, .z = 5300, .yaw = 2200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_WALL_NO_SETTLE, .x = 10000, .y = -2500, .z = 6200, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_WALL_NO_SETTLE, .x = 10000, .y = -1900, .z = 5000, .yaw = 400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_WALL_NO_SETTLE, .x = 9300, .y = -2300, .z = 7800, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldGarageVariant3Placements[4] = {
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = 800, .y = -3700, .z = 4800, .yaw = 400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = 2300, .y = -3700, .z = 5500, .yaw = 3500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_MIND_SUCKLER_ENTRY, .variant = MAP_DRYFIELD_MIND_SUCKLER_PERCHED, .mode = MAP_DRYFIELD_MIND_SUCKLER_CEILING_NO_SETTLE, .x = 1900, .y = -3700, .z = 3800, .yaw = 3900, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldDrivewayVariant1Placements[26] = {
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_1_DIRECTOR, .x = -1600, .y = 0, .z = 2600, .yaw = 2432, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_16_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_2, .x = -1600, .y = 0, .z = 2450, .yaw = 2304, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_16_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_2, .x = -1600, .y = 0, .z = 2300, .yaw = 2176, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_16_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_2, .x = -1450, .y = 0, .z = 2600, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_16_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_2, .x = -1450, .y = 0, .z = 2450, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_16_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_2, .x = -1450, .y = 0, .z = 2300, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_16_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_2, .x = -1300, .y = 0, .z = 2600, .yaw = 1664, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_16_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_2, .x = -1300, .y = 0, .z = 2450, .yaw = 1792, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_16_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_2, .x = -1300, .y = 0, .z = 2300, .yaw = 1920, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_16_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_2, .x = -1450, .y = 0, .z = 2450, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_16_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_3, .x = -1600, .y = 0, .z = 2450, .yaw = 2304, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_3, .x = -1450, .y = 0, .z = 2600, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_3, .x = -1450, .y = 0, .z = 2450, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_4, .x = -1600, .y = 0, .z = 2450, .yaw = 2304, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_20_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_4, .x = -1450, .y = 0, .z = 2600, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_20_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_4, .x = -1450, .y = 0, .z = 2450, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_20_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_4, .x = -1450, .y = 0, .z = 2300, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_20_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_5, .x = -1600, .y = 0, .z = 2600, .yaw = 2432, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_28_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_5, .x = -1600, .y = 0, .z = 2450, .yaw = 2304, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_28_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_5, .x = -1600, .y = 0, .z = 2300, .yaw = 2176, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_28_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_5, .x = -1450, .y = 0, .z = 2450, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_28_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_5, .x = -1450, .y = 0, .z = 2300, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_28_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_5, .x = -1300, .y = 0, .z = 2600, .yaw = 1664, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_28_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_5, .x = -1300, .y = 0, .z = 2450, .yaw = 1792, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_28_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_WAVE_5, .x = -1300, .y = 0, .z = 2300, .yaw = 1920, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_28_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldDrivewayVariant2Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldDrivewayVariant3Placements[2] = {
    { .entryId = MAP_DRYFIELD_DESERT_CHASER_ENTRY, .variant = MAP_DRYFIELD_CHASER_APPROACH, .mode = MAP_DRYFIELD_CHASER_TUNING_SECOND, .x = 4676, .y = 0, .z = 2038, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldJunkYardVariant1Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldJunkYardVariant2Placements[5] = {
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 3200, .y = 0, .z = 1900, .yaw = 3200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 4000, .y = 0, .z = 1000, .yaw = 3000, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_FOLLOWER, .x = 3600, .y = 0, .z = 3000, .yaw = 2700, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_AMBUSH_LEADER, .x = 1200, .y = 0, .z = 900, .yaw = 800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldTrailerCoachGaryPlacements[2] = {
    { .entryId = MAP_DRYFIELD_GARY_DOUGLAS_ENTRY, .variant = 0, .mode = 0, .x = 3810, .y = 0, .z = -2130, .yaw = 512, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelBalconyVariant1Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelBalconyVariant2Placements[4] = {
    { .entryId = MAP_DRYFIELD_ODD_STRANGER_ENTRY, .variant = MAP_DRYFIELD_ODD_STRANGER_STALK, .mode = MAP_DRYFIELD_ODD_STRANGER_TUNING_SECOND, .x = -6000, .y = -3200, .z = 1200, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_ODD_STRANGER_ENTRY, .variant = MAP_DRYFIELD_ODD_STRANGER_STALK, .mode = MAP_DRYFIELD_ODD_STRANGER_TUNING_SECOND, .x = -5500, .y = -3200, .z = -4500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_ODD_STRANGER_ENTRY, .variant = MAP_DRYFIELD_ODD_STRANGER_STALK, .mode = MAP_DRYFIELD_ODD_STRANGER_TUNING_SECOND, .x = -5500, .y = -3200, .z = 3500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelBalconyVariant3Placements[7] = {
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_ROAM, .x = -5800, .y = -3200, .z = 10500, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_ROAM, .x = -6500, .y = -3200, .z = 7500, .yaw = 1700, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_ROAM, .x = -5400, .y = -3200, .z = 6000, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_ROAM, .x = -6500, .y = -3200, .z = -1300, .yaw = 1512, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_ROAM, .x = -5500, .y = -3200, .z = -2700, .yaw = 512, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_SCORPION_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_SCORPION_ROAM, .x = -6100, .y = -3200, .z = -4500, .yaw = ACTOR_TRANSFORM_ANGLE_TURN / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelBalconyVariant7Placements[3] = {
    { .entryId = MAP_DRYFIELD_BRAIN_STINGER_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BRAIN_STINGER_PATROL_FIRST, .x = -6000, .y = -3200, .z = 1200, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BRAIN_STINGER_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BRAIN_STINGER_PATROL_SECOND, .x = -6000, .y = -3200, .z = 8000, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelBalconyVariant8Placements[3] = {
    { .entryId = MAP_DRYFIELD_LAUNCHER_PAWN_GOLEM_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_GOLEM_IDLE, .x = -2000, .y = -3200, .z = 9700, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_DRYFIELD_BALCONY_ROOK_GOLEM_ENTRY, .variant = 6, .mode = MAP_DRYFIELD_GOLEM_PATROLS, .x = -5500, .y = -3200, .z = 2000, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelRoom6Variant1Placements[2] = {
    { .entryId = MAP_DRYFIELD_MOTEL_ROOM_6_SCENE_ENTRY, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldMotelRoom6Variant2Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldWaterHoleBatPlacements[9] = {
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_DROP, .x = 11600, .y = -2000, .z = -3200, .yaw = 600, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_DROP, .x = 11400, .y = -2000, .z = -3600, .yaw = 400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_DROP, .x = 11000, .y = -2000, .z = -3100, .yaw = 200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_DROP, .x = 10600, .y = -2000, .z = -3300, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_BACK, .x = 10000, .y = -1700, .z = -3400, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_BACK, .x = 10000, .y = -1800, .z = -3100, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_BACK, .x = 10000, .y = -1100, .z = -3500, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_BACK, .x = 10000, .y = -1200, .z = -3000, .yaw = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldCellarVariant1Placements[11] = {
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_DROP, .x = 3800, .y = -2500, .z = 3600, .yaw = 1200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_DROP, .x = 3300, .y = -2500, .z = 300, .yaw = 700, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_DROP, .x = 4700, .y = -2500, .z = 400, .yaw = 400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_DROP, .x = 6500, .y = -2500, .z = 500, .yaw = 3800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_DROP, .x = 10200, .y = -2500, .z = 2000, .yaw = 3400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_DROP, .x = 7800, .y = -2500, .z = 4200, .yaw = 3200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_BACK, .x = 4100, .y = -1500, .z = 4500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_BAT_ENTRY, .variant = 0, .mode = MAP_DRYFIELD_BAT_PERCH_BACK, .x = 3800, .y = -1700, .z = 4500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_DRYFIELD_BAT_TURN_RATE_24_ROW },
    { .entryId = MAP_DRYFIELD_RAT_ENTRY, .variant = 0, .mode = 0, .x = 4500, .y = 0, .z = 1800, .yaw = 300, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_RAT_TUNING_THIRD },
    { .entryId = MAP_DRYFIELD_RAT_ENTRY, .variant = 0, .mode = 0, .x = 7100, .y = 0, .z = 500, .yaw = 3400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_DRYFIELD_RAT_TUNING_THIRD },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapDryfieldUnderpassGrayStalkerPlacements[2] = {
    { .entryId = MAP_DRYFIELD_GRAY_STALKER_ENTRY, .variant = 0, .mode = 0, .x = 0x4074, .y = -0xFA0, .z = -0x209E, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

InventoryBattleReward gMapDryfieldBattleRewards[13] = {
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 1, 0), { MAP_DRYFIELD_REWARD_9MM_HYDRA, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_REWARD_9MM_SPARTAN } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 2, 0), { MAP_DRYFIELD_REWARD_PENICILLIN, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GENERAL_STORE, 1, 0), { MAP_DRYFIELD_REWARD_FLARE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE, 1, 0), { INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_REWARD_GUNBLADE } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_1, 3, 0), { MAP_DRYFIELD_REWARD_FLARE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 3, 0), { MAP_DRYFIELD_REWARD_9MM_HYDRA, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_REWARD_9MM_SPARTAN } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 1, 0), { MAP_DRYFIELD_REWARD_MP_BOOST1, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_REWARD_LIPSTICK } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_G_R_KITCHEN, 1, 0), { MAP_DRYFIELD_REWARD_COMBAT_LIGHT, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 1, 0), { MAP_DRYFIELD_REWARD_RECOVERY2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_DRYFIELD_REWARD_MP_BOOST1 } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BREEZEWAY, 1, 0), { MAP_DRYFIELD_REWARD_MP_BOOST1, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 1, 0), { MAP_DRYFIELD_REWARD_COMBAT_LIGHT, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_UNDERPASS, 1, 0), { MAP_DRYFIELD_REWARD_PROTEIN_CAPSULE, MAP_DRYFIELD_REWARD_R_SLUG, INVENTORY_ITEM_NONE, MAP_DRYFIELD_REWARD_FIREFLY } },
    { INVENTORY_BATTLE_REWARD_LIST_END },
};

InventoryBattleReward gMapDryfieldHardBattleRewards[8] = {
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 7, 0), { MAP_DRYFIELD_REWARD_PROTEIN_CAPSULE, MAP_DRYFIELD_REWARD_PENICILLIN, MAP_DRYFIELD_REWARD_FLARE, INVENTORY_ITEM_NONE } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_BACK_STREET, 7, 0), { MAP_DRYFIELD_REWARD_RECOVERY2, MAP_DRYFIELD_REWARD_MP_BOOST1, MAP_DRYFIELD_REWARD_9MM_SPARTAN, INVENTORY_ITEM_NONE } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 7, 0), { MAP_DRYFIELD_REWARD_HAMMER, MAP_DRYFIELD_REWARD_FIREFLY, MAP_DRYFIELD_REWARD_9MM_SPARTAN, INVENTORY_ITEM_NONE } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, 7, 0), { MAP_DRYFIELD_REWARD_EAU_DE_TOILETTE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_BALCONY, 7, 0), { MAP_DRYFIELD_REWARD_BUCKSHOT, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_BALCONY, 8, 0), { MAP_DRYFIELD_REWARD_AIRBURST, MAP_DRYFIELD_REWARD_9MM_SPARTAN, MAP_DRYFIELD_REWARD_RECOVERY3, INVENTORY_ITEM_NONE } },
    { GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_UNDERPASS, 1, 0), { MAP_DRYFIELD_REWARD_R_SLUG, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { INVENTORY_BATTLE_REWARD_LIST_END },
};

StageMusicEntry gMapDryfieldAreaMusicEntries[(GAME_AREA_DRYFIELD_UNDERPASS + 1) * MAP_DRYFIELD_MUSIC_COLUMN_COUNT] = {
    // Area 0.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_GAS_STATION.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_MAIN_STREET.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_GENERAL_STORE.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_R04.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_BACK_STREET.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_SOUVENIR_SHOP.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_WAREHOUSE.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_R08.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x31, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area 10.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_MOTEL_ROOM_1.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_MOTEL_ROOM_2.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_MOTEL_ROOM_3.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_MOTEL_ROOM_4.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_PARKING_LOT.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_TOILET.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_MOTEL_LOBBY.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_SALOON_G_R.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_G_R_KITCHEN.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_WATER_TOWER.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_WATER_TANK.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2C, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_BREEZEWAY.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_FACTORY.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_GARAGE.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2A, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x2A, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_DRIVEWAY.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_JUNK_YARD.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_DEFERRED },
    // Area GAME_AREA_DRYFIELD_TRAILER_COACH.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x2D, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x2D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_MOTEL_ROOM_5.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_MOTEL_BALCONY.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_MOTEL_ROOM_6.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = 0x29, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_MOTEL_LOFT.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_WATER_HOLE.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area 33.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_CELLAR.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area 35.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area 36.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area 37.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    // Area GAME_AREA_DRYFIELD_UNDERPASS.
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2E, .startMode = STAGE_MUSIC_START_NEVER },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x2F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
};

StageMusicEntry gMapDryfieldCountdownMusicEntries[20] = {
    { .sequenceId = 0x32, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x17, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
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

/// Uninterpreted final two bytes of the daytime Dryfield package.
///
/// Retained verbatim after the countdown table. No reader establishes a record
/// type or role for these bytes; their meaning is unproven.
static u8 _gMapDryfieldTrailingBytes[2] = { 0x20, 0x26 };
