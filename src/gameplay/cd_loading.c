#include "gameplay/loading.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/area.h"
#include "companion_load.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "loading.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "stages/akropolis/rooms/bridge.h"

#include "stages/akropolis/rooms/cafeteria.h"

#include "stages/akropolis/rooms/east_elevator_hall.h"

#include "stages/akropolis/rooms/fire_escape.h"

#include "stages/akropolis/rooms/forked_road.h"

#include "stages/akropolis/rooms/fountain.h"

#include "stages/akropolis/rooms/hallway.h"

#include "stages/akropolis/rooms/helicopter_landing_pad.h"

#include "stages/akropolis/rooms/observatory.h"

#include "stages/akropolis/rooms/patio.h"

#include "stages/akropolis/rooms/plaza.h"

#include "stages/akropolis/rooms/promenade.h"

#include "stages/akropolis/rooms/roof_garden.h"

#include "stages/akropolis/rooms/sanctuary.h"

#include "stages/akropolis/rooms/security_room.h"

#include "stages/akropolis/rooms/square.h"

#include "rooms/dryfield_back_street.h"

#include "rooms/dryfield_breezeway.h"

#include "rooms/dryfield_cellar.h"

#include "rooms/dryfield_dilapidated_house.h"

#include "rooms/dryfield_driveway.h"

#include "rooms/dryfield_g_r_kitchen.h"

#include "rooms/dryfield_garage.h"

#include "rooms/dryfield_gas_station.h"

#include "rooms/dryfield_general_store.h"

#include "rooms/dryfield_junk_yard.h"

#include "rooms/dryfield_main_street.h"

#include "rooms/dryfield_motel_balcony.h"

#include "rooms/dryfield_motel_room_1.h"

#include "rooms/dryfield_motel_room_2.h"

#include "rooms/dryfield_motel_room_6.h"

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

#include "rooms/dryfield_parking_lot.h"

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

#include "rooms/mine_cavern.h"

#include "rooms/mine_forked_tunnel.h"

#include "rooms/mine_gorge.h"

#include "rooms/mine_mesa.h"

#include "rooms/mine_refuge.h"

#include "rooms/mine_secret_passage.h"

#include "rooms/mine_tunnel.h"

#include "rooms/mine_tunnel_entrance.h"

#include "stages/akropolis/rooms/mist_parking.h"

#include "stages/akropolis/rooms/mist_r18.h"

#include "stages/akropolis/rooms/mist_shooting_gallery.h"

#include "rooms/neo_ark_bridge.h"

#include "rooms/neo_ark_eve_access_tunnel.h"

#include "rooms/neo_ark_forest_zone.h"

#include "rooms/neo_ark_garden.h"

#include "rooms/neo_ark_island.h"

#include "rooms/neo_ark_north_promenade.h"

#include "rooms/neo_ark_observatory.h"

#include "rooms/neo_ark_pavilion.h"

#include "rooms/neo_ark_power_plant_1.h"

#include "rooms/neo_ark_power_plant_2.h"

#include "rooms/neo_ark_pyramid.h"

#include "rooms/neo_ark_r26.h"

#include "rooms/neo_ark_r31.h"

#include "rooms/neo_ark_savanna_zone.h"

#include "rooms/neo_ark_shrine.h"

#include "rooms/neo_ark_south_promenade.h"

#include "rooms/neo_ark_submarine_gallery.h"

#include "rooms/neo_ark_submarine_tunnel.h"

#include "rooms/neo_ark_woodland_path.h"

#include "rooms/shelter_1f_airlock.h"

#include "rooms/shelter_1f_bulwark.h"

#include "rooms/shelter_1f_heliport.h"

#include "rooms/shelter_1f_parking_garage.h"

#include "rooms/shelter_1f_tent.h"

#include "rooms/shelter_1f_vehicular_airlock.h"

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

#include "rooms/shelter_b6_corridor.h"

#include "rooms/shelter_b6_growth_room.h"

#include "rooms/shelter_b6_nursery.h"

#include "rooms/shelter_b6_training_room.h"

#include "rooms/shelter_r36.h"

#include "rooms/shelter_r47.h"

#include "rooms/shelter_r48.h"

#include "rooms/shelter_r49.h"

/// Stage-zero file-id hundreds digits for the character model package, one per
/// resource variant.
///
/// Indexed by `gPlayerStatus.resourceVariant` minus one. With file group 1 and
/// file index 0, the digits 4, 3, 2, 5 and 6 select files 10400, 10300, 10200,
/// 10500 and 10600; the last two load the same package. Wrapping the bytes in
/// a struct lets the table be copied by assignment.
typedef struct {
    u8 fileIdHundreds[5]; // Hundreds component for resource variants 1..5
} _LoadingConfigFileHundreds;
STATIC_ASSERT_SIZEOF(_LoadingConfigFileHundreds, 5);

/// Global-library selectors and the request to seek back to the current view.
enum {
    LOADING_GLOBAL_CDF         = 0,
    LOADING_PLAYER_FILE_GROUP  = 1,
    LOADING_VIEW_FOLDER_SUFFIX = 1,
    LOADING_AMMO_FILE_NONE     = 0,
    LOADING_VIEW_SEEK_PENDING  = 1,
};

/// View-load state slots, completion markers and the bank-0 task's presentation policies.
enum {
    LOADING_VIEW_STATE_BEGIN             = 0,
    LOADING_VIEW_STATE_ENQUEUE_RESOURCES = 1,
    LOADING_VIEW_STATE_UPLOAD_IMAGE      = 2,
    LOADING_VIEW_STATE_WAIT_MOVIE_READY  = 3,
    LOADING_VIEW_STATE_WAIT_CD_IDLE      = 4,
    LOADING_VIEW_STATE_WAIT_SCENE_IMAGE  = 5,
    LOADING_VIEW_FINISH_READY            = -1,
    LOADING_VIEW_FINISH_CD_IDLE          = -2,
    LOADING_VIEW_MOVIE_READY_TICKS       = 3,
    LOADING_VIEW_INPUT_PORT              = 0,
    LOADING_VIEW_TASK_BANK               = 0,
    LOADING_VIEW_TASK_TYPE               = 0x1E,
    LOADING_VIEW_SPRITE_TASK_TYPE        = 0x17,
    LOADING_VIEW_RESUME_GAME_LOOP        = 0,
    LOADING_VIEW_RESUME_TASK_FLIPS       = 1,
    LOADING_VIEW_NOT_READY               = 0,
    LOADING_VIEW_READY                   = 1,
    LOADING_VIEW_RELOAD_CLEAR            = 0,
};

static const AreaRecord _gAreaAcropolisRecords[GAME_AREA_MIST_SHOOTING_GALLERY + 1];
static const AreaRecord _gAreaDryfieldRecords[GAME_AREA_DRYFIELD_UNDERPASS + 1];
static const AreaRecord _gAreaDryfieldNightRecords[GAME_AREA_DRYFIELD_NIGHT_UNDERPASS + 1];
static const AreaRecord _gAreaMineShelterRecords[GAME_AREA_SHELTER_R49 + 1];
static const AreaRecord _gAreaShelterNeoArkRecords[GAME_AREA_NEO_ARK_SUBSTATION + 1];

static void _loadingEnqueueWeaponAmmoResources(void);

static void _loadingWaitForMovieReadyTask(Task* viewLoadTask);

static void _loadingWaitForViewCdIdleTask(Task* viewLoadTask);

static void _loadingWaitForSceneImageTask(Task* viewLoadTask);

static void _loadingReloadSavedView(void);

static void _loadingReloadView(s32 logicalViewIndex);

// The overlay stores its immutable area records in the fixed data segment.
const AreaRecord* gAreaRecordsByStage[GAME_STAGE_SHELTER_NEO_ARK + 1] = {
    [GAME_STAGE_NONE]            = NULL,
    [GAME_STAGE_AKROPOLIS]       = _gAreaAcropolisRecords,
    [GAME_STAGE_DRYFIELD]        = _gAreaDryfieldRecords,
    [GAME_STAGE_DRYFIELD_NIGHT]  = _gAreaDryfieldNightRecords,
    [GAME_STAGE_MINE_SHELTER]    = _gAreaMineShelterRecords,
    [GAME_STAGE_SHELTER_NEO_ARK] = _gAreaShelterNeoArkRecords,
};

/// Immutable area-ID directory for Akropolis Tower and MIST layouts and live saved state.
///
/// IDs 0..20 index this table directly; zero and unpublished areas are empty.
/// Room layout pointers require the selected room overlay to remain loaded.
/// Save slots are explicit: the shooting gallery uses the live Dryfield bank.
static const AreaRecord _gAreaAcropolisRecords[GAME_AREA_MIST_SHOOTING_GALLERY + 1] SECTION(".data") = {
    [GAME_AREA_AKROPOLIS_SQUARE]                 = { gAkropolisSquareAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[0].state },
    [GAME_AREA_AKROPOLIS_EAST_ELEVATOR_HALL]     = { gAkropolisEastElevatorHallAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[1].state },
    [GAME_AREA_AKROPOLIS_PATIO]                  = { gAkropolisPatioAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[2].state },
    [GAME_AREA_AKROPOLIS_CAFETERIA]              = { gAkropolisCafeteriaAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[3].state },
    [GAME_AREA_AKROPOLIS_PLAZA]                  = { gAkropolisPlazaAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[4].state },
    [GAME_AREA_AKROPOLIS_SECURITY_ROOM]          = { gAkropolisSecurityRoomAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[5].state },
    [GAME_AREA_AKROPOLIS_HALLWAY]                = { gAkropolisHallwayAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[6].state },
    [GAME_AREA_AKROPOLIS_FOUNTAIN]               = { gAkropolisFountainAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[7].state },
    [GAME_AREA_AKROPOLIS_FORKED_ROAD]            = { gAkropolisForkedRoadAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[8].state },
    [GAME_AREA_AKROPOLIS_OBSERVATORY]            = { gAkropolisObservatoryAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[9].state },
    [GAME_AREA_AKROPOLIS_PROMENADE]              = { gAkropolisPromenadeAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[10].state },
    [GAME_AREA_AKROPOLIS_SANCTUARY]              = { gAkropolisSanctuaryAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[11].state },
    [GAME_AREA_AKROPOLIS_ROOF_GARDEN]            = { gAkropolisRoofGardenAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[12].state },
    [GAME_AREA_AKROPOLIS_BRIDGE]                 = { gAkropolisBridgePlacementVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[13].state },
    [GAME_AREA_AKROPOLIS_FIRE_ESCAPE]            = { gAkropolisFireEscapeAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[14].state },
    [GAME_AREA_AKROPOLIS_HELICOPTER_LANDING_PAD] = { gAkropolisHelicopterLandingPadAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[15].state },
    [GAME_AREA_AKROPOLIS_WEST_ELEVATOR_HALL]     = { NULL, NULL },
    [GAME_AREA_MIST_R18]                         = { gMistR18AreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[16].state },
    [GAME_AREA_MIST_PARKING]                     = { gMistParkingAreaVariants, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[17].state },
    [GAME_AREA_MIST_SHOOTING_GALLERY]            = { gMistShootingGalleryAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[29].state },
};
/// Two words after the Akropolis/MIST directory; their purpose is unproven.
static u32 D_8010CC8C[2] = { 0xFFFF, 0 };

/// Immutable area-ID directory for daytime Dryfield layouts and live saved state.
///
/// IDs 0..38 index this table directly; zero and unpublished areas are empty.
/// Layouts borrow the loaded room overlays; save slots refer to the live
/// Dryfield bank shared with nighttime Dryfield and are independent of area ID.
static const AreaRecord _gAreaDryfieldRecords[GAME_AREA_DRYFIELD_UNDERPASS + 1] SECTION(".data") = {
    [GAME_AREA_DRYFIELD_GAS_STATION]       = { gDryfieldGasStationPlacementVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[0].state },
    [GAME_AREA_DRYFIELD_MAIN_STREET]       = { gDryfieldMainStreetAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[1].state },
    [GAME_AREA_DRYFIELD_GENERAL_STORE]     = { gDryfieldGeneralStorePlacementVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[2].state },
    [GAME_AREA_DRYFIELD_R04]               = { NULL, NULL },
    [GAME_AREA_DRYFIELD_BACK_STREET]       = { gDryfieldBackStreetAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[3].state },
    [GAME_AREA_DRYFIELD_SOUVENIR_SHOP]     = { gDryfieldSouvenirShopAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[4].state },
    [GAME_AREA_DRYFIELD_WAREHOUSE]         = { gDryfieldWarehousePlacementVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[5].state },
    [GAME_AREA_DRYFIELD_R08]               = { gDryfieldR08AreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[33].state },
    [GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE] = { gDryfieldDilapidatedHouseAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[6].state },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_1]      = { gDryfieldMotelRoom1PlacementVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[7].state },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_2]      = { gDryfieldMotelRoom2AreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[8].state },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_3]      = { NULL, NULL },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_4]      = { NULL, NULL },
    [GAME_AREA_DRYFIELD_PARKING_LOT]       = { gDryfieldParkingLotAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[11].state },
    [GAME_AREA_DRYFIELD_TOILET]            = { gDryfieldToiletAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[12].state },
    [GAME_AREA_DRYFIELD_MOTEL_LOBBY]       = { NULL, NULL },
    [GAME_AREA_DRYFIELD_SALOON_G_R]        = { gDryfieldSaloonGRAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[13].state },
    [GAME_AREA_DRYFIELD_G_R_KITCHEN]       = { gDryfieldGRKitchenAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[14].state },
    [GAME_AREA_DRYFIELD_WATER_TOWER]       = { gDryfieldWaterTowerAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[15].state },
    [GAME_AREA_DRYFIELD_WATER_TANK]        = { gDryfieldWaterTankAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[16].state },
    [GAME_AREA_DRYFIELD_BREEZEWAY]         = { gDryfieldBreezewayAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[17].state },
    [GAME_AREA_DRYFIELD_FACTORY]           = { NULL, NULL },
    [GAME_AREA_DRYFIELD_GARAGE]            = { gDryfieldGarageAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[18].state },
    [GAME_AREA_DRYFIELD_DRIVEWAY]          = { gDryfieldDrivewayAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[19].state },
    [GAME_AREA_DRYFIELD_JUNK_YARD]         = { gDryfieldJunkYardAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[20].state },
    [GAME_AREA_DRYFIELD_TRAILER_COACH]     = { gDryfieldTrailerCoachAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[21].state },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_5]      = { NULL, NULL },
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY]     = { gDryfieldMotelBalconyAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[23].state },
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_6]      = { gDryfieldMotelRoom6AreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[24].state },
    [GAME_AREA_DRYFIELD_MOTEL_LOFT]        = { NULL, NULL },
    [GAME_AREA_DRYFIELD_WATER_HOLE]        = { gDryfieldWaterHoleAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[26].state },
    [GAME_AREA_DRYFIELD_CELLAR]            = { gDryfieldCellarAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[27].state },
    [GAME_AREA_DRYFIELD_UNDERPASS]         = { gDryfieldUnderpassAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[28].state },
};
/// Two words after the daytime Dryfield directory; their purpose is unproven.
static u32 D_8010CDCC[2] = { 0xFFFF, 0 };

/// Immutable area-ID directory for nighttime Dryfield layouts and live saved state.
///
/// IDs 0..38 index this table directly; zero and unpublished areas are empty.
/// Layouts borrow the loaded room overlays. Saved placement state is shared
/// with the daytime directory through explicit live Dryfield bank slots.
static const AreaRecord _gAreaDryfieldNightRecords[GAME_AREA_DRYFIELD_NIGHT_UNDERPASS + 1] SECTION(".data") = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION]       = { gDryfieldNightGasStationAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[0].state },
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET]       = { gDryfieldNightMainStreetAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[1].state },
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE]     = { gDryfieldNightGeneralStoreAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[2].state },
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET]       = { gDryfieldNightBackStreetAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[3].state },
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP]     = { gDryfieldNightSouvenirShopAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[4].state },
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE]         = { gDryfieldNightWarehouseAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[5].state },
    [GAME_AREA_DRYFIELD_NIGHT_R08]               = { gDryfieldNightR08AreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[33].state },
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE] = { gDryfieldNightDilapidatedHouseAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[6].state },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1]      = { gDryfieldNightMotelRoom1AreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[7].state },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2]      = { gDryfieldNightMotelRoom2AreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[8].state },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3]      = { gDryfieldNightMotelRoom3AreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[9].state },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4]      = { gDryfieldNightMotelRoom4AreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[10].state },
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT]       = { gDryfieldNightParkingLotAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[11].state },
    [GAME_AREA_DRYFIELD_NIGHT_TOILET]            = { gDryfieldNightToiletAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[12].state },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY]       = { gDryfieldNightMotelLobbyAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[34].state },
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R]        = { gDryfieldNightSaloonGRAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[13].state },
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN]       = { gDryfieldNightGRKitchenAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[14].state },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER]       = { gDryfieldNightWaterTowerAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[15].state },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK]        = { gDryfieldNightWaterTankAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[16].state },
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY]         = { gDryfieldNightBreezewayAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[17].state },
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY]           = { gDryfieldNightFactoryAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[35].state },
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE]            = { gDryfieldNightGarageAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[18].state },
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY]          = { gDryfieldNightDrivewayAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[19].state },
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD]         = { gDryfieldNightJunkYardAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[20].state },
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH]     = { gDryfieldNightTrailerCoachAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[21].state },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5]      = { gDryfieldNightMotelRoom5AreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[22].state },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY]     = { gDryfieldNightMotelBalconyAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[23].state },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6]      = { gDryfieldNightMotelRoom6AreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[24].state },
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT]        = { gDryfieldNightMotelLoftAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[25].state },
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE]        = { gDryfieldNightWaterHoleAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[26].state },
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR]            = { gDryfieldNightCellarAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[27].state },
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS]         = { gDryfieldNightUnderpassAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[28].state },
};
/// Two words after the nighttime Dryfield directory; their purpose is unproven.
static u32 D_8010CF0C[2] = { 0xFFFF, 0 };

/// Immutable area-ID directory for the mine and Shelter B1..B4 layouts and live saved state.
///
/// IDs 0..49 index this table directly; zero and unpublished areas are empty.
/// Layouts borrow loaded room overlays. Save-bank slots are explicit; Shelter
/// R49 and the mine refuge share live saved placement state in slot 5.
static const AreaRecord _gAreaMineShelterRecords[GAME_AREA_SHELTER_R49 + 1] SECTION(".data") = {
    [GAME_AREA_MINE_MESA]                             = { gMineMesaAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[0].state },
    [GAME_AREA_MINE_CAVERN]                           = { gMineCavernAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[1].state },
    [GAME_AREA_MINE_TUNNEL_ENTRANCE]                  = { gMineTunnelEntranceAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[2].state },
    [GAME_AREA_MINE_TUNNEL]                           = { gMineTunnelAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[3].state },
    [GAME_AREA_MINE_GORGE]                            = { gMineGorgeAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[4].state },
    [GAME_AREA_MINE_REFUGE]                           = { gMineRefugeAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[5].state },
    [GAME_AREA_MINE_FORKED_TUNNEL]                    = { gMineForkedTunnelAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[6].state },
    [GAME_AREA_MINE_SECRET_PASSAGE]                   = { gMineSecretPassageAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[7].state },
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL]              = { gShelterB1ElevatorHallAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[8].state },
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY]  = { gShelterB1SouthMaintenanceWalkwayAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[9].state },
    [GAME_AREA_SHELTER_B1_STOREROOM]                  = { gShelterB1StoreroomAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[10].state },
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY]  = { gShelterB1NorthMaintenanceWalkwayAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[11].state },
    [GAME_AREA_SHELTER_B1_ARMORY]                     = { gShelterB1ArmoryAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[12].state },
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS]          = { gShelterB1SleepingQuartersAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[13].state },
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR]              = { gShelterB1MainCorridorAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[14].state },
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM]         = { gShelterB1SterilizationRoomAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[15].state },
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL]          = { gShelterB1PodAccessTunnelAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[16].state },
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM]               = { gShelterB1ControlRoomAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[17].state },
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL]              = { gShelterB1AccessTunnelAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[18].state },
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING]        = { gShelterB1UndergroundParkingAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[19].state },
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1]            = { gShelterB1GolemFreezer1AreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[20].state },
    [GAME_AREA_SHELTER_B2_POD_BOTTOM]                 = { gShelterB2PodBottomAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[21].state },
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY]         = { gShelterB1PodServiceGantryAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[22].state },
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL]            = { gShelterB1TransferTunnelAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[23].state },
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL] = { gShelterB1ControlRoomAccessTunnelAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[24].state },
    [GAME_AREA_SHELTER_B2_ELEVATOR]                   = { gShelterB2ElevatorAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[25].state },
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL]              = { gShelterB2ElevatorHallAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[26].state },
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY]  = { gShelterB2SouthMaintenanceWalkwayAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[27].state },
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM]             = { gShelterB2OperatingRoomAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[28].state },
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY]  = { gShelterB2NorthMaintenanceWalkwayAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[29].state },
    [GAME_AREA_SHELTER_B2_LABORATORY]                 = { gShelterB2LaboratoryAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[30].state },
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM]              = { gShelterB2BreedingRoomAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[31].state },
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR]              = { gShelterB2MainCorridorAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[32].state },
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK]                = { gShelterB2SepticTankAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[33].state },
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL]          = { gShelterB2PodAccessTunnelAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[34].state },
    [GAME_AREA_SHELTER_R36]                           = { gShelterR36AreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[35].state },
    [GAME_AREA_SHELTER_R37]                           = { NULL, NULL },
    [GAME_AREA_SHELTER_1F_HELIPORT_S4]                = { NULL, NULL },
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE]               = { gShelterB3DumpingHoleAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[38].state },
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR]        = { gShelterB3GarbageIncineratorAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[39].state },
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM]   = { gShelterB3IncineratorControlRoomAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[40].state },
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL]              = { gShelterB3ElevatorHallAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[41].state },
    [GAME_AREA_SHELTER_B4_LOWER_SEWER]                = { gShelterB4LowerSewerAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[42].state },
    [GAME_AREA_SHELTER_B4_UPPER_SEWER]                = { gShelterB4UpperSewerAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[43].state },
    [GAME_AREA_SHELTER_B4_RESERVOIR]                  = { gShelterB4ReservoirAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[44].state },
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY]               = { gShelterB4WaterSupplyPlacementVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[45].state },
    [GAME_AREA_SHELTER_R47]                           = { gShelterR47AreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[46].state },
    [GAME_AREA_SHELTER_R48]                           = { gShelterR48AreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[47].state },
    [GAME_AREA_SHELTER_R49]                           = { gShelterR49AreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[5].state },
};
/// Two words after the mine/Shelter directory; their purpose is unproven.
static u32 D_8010D0A4[2] = { 0xFFFF, 0 };

/// Immutable area-ID directory for Shelter 1F/B6 and Neo Ark layouts and live saved state.
///
/// IDs 0..33 index this table directly; zero and unpublished areas are empty.
/// Layouts borrow loaded room overlays. Save-bank slots are explicit; the B6
/// nursery and growth room share live saved placement state in slot 21.
static const AreaRecord _gAreaShelterNeoArkRecords[GAME_AREA_NEO_ARK_SUBSTATION + 1] SECTION(".data") = {
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE]    = { gShelter1fParkingGarageAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[0].state },
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK] = { gShelter1fVehicularAirlockAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[1].state },
    [GAME_AREA_SHELTER_1F_BULWARK]           = { gShelter1fBulwarkAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[2].state },
    [GAME_AREA_SHELTER_1F_HELIPORT]          = { gShelter1fHeliportAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[3].state },
    [GAME_AREA_SHELTER_1F_AIRLOCK]           = { gShelter1fAirlockAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[4].state },
    [GAME_AREA_SHELTER_1F_GUARDROOM]         = { NULL, NULL },
    [GAME_AREA_NEO_ARK_OBSERVATORY]          = { gNeoArkObservatoryAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[6].state },
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL]    = { gNeoArkEveAccessTunnelAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[7].state },
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR]         = { NULL, NULL },
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE]      = { gNeoArkNorthPromenadeAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[9].state },
    [GAME_AREA_NEO_ARK_FOREST_ZONE]          = { gNeoArkForestZoneAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[10].state },
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL]     = { gNeoArkSubmarineTunnelAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[11].state },
    [GAME_AREA_NEO_ARK_PAVILION]             = { gNeoArkPavilionAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[12].state },
    [GAME_AREA_NEO_ARK_ISLAND]               = { gNeoArkIslandAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[13].state },
    [GAME_AREA_NEO_ARK_GARDEN]               = { gNeoArkGardenAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[14].state },
    [GAME_AREA_NEO_ARK_POWER_PLANT_2]        = { gNeoArkPowerPlant2AreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[15].state },
    [GAME_AREA_NEO_ARK_POWER_PLANT_1]        = { gNeoArkPowerPlant1AreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[16].state },
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE]         = { gNeoArkSavannaZoneAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[17].state },
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE]      = { gNeoArkSouthPromenadeAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[18].state },
    [GAME_AREA_NEO_ARK_ALTAR]                = { NULL, NULL },
    [GAME_AREA_NEO_ARK_SHRINE]               = { gNeoArkShrineAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[20].state },
    [GAME_AREA_SHELTER_B6_NURSERY]           = { gShelterB6NurseryAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[21].state },
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM]       = { gShelterB6GrowthRoomAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[21].state },
    [GAME_AREA_SHELTER_B6_CORRIDOR]          = { gShelterB6CorridorAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[23].state },
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM]     = { gShelterB6TrainingRoomAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[24].state },
    [GAME_AREA_NEO_ARK_R26]                  = { gNeoArkR26AreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[25].state },
    [GAME_AREA_NEO_ARK_BRIDGE]               = { gNeoArkBridgePlacementVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[26].state },
    [GAME_AREA_SHELTER_1F_TENT]              = { gShelter1fTentAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[27].state },
    [GAME_AREA_NEO_ARK_WOODLAND_PATH]        = { gNeoArkWoodlandPathAreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[28].state },
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY]    = { gNeoArkSubmarineGalleryPlacementVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[29].state },
    [GAME_AREA_NEO_ARK_R31]                  = { gNeoArkR31AreaVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[30].state },
    [GAME_AREA_NEO_ARK_PYRAMID]              = { gNeoArkPyramidPlacementVariants, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[31].state },
    [GAME_AREA_NEO_ARK_SUBSTATION]           = { NULL, NULL },
};
/// Two words after the Shelter/Neo Ark directory; their purpose is unproven.
static u32 D_8010D1BC[2] = { 0xFFFF, 0 };

/// Six phase handlers copied by value before each current-view loading update.
///
/// Task states 0..5 select begin, enqueue, upload/movie selection, movie-ready
/// wait, CD-idle wait and scene-image wait. Negative finish markers are handled
/// before indexing. There is no upper-bound check or terminator; callbacks
/// borrow live gameplay resources and may release the task on completion.
static const TaskFuncTable6 _gLoadingViewStateHandlers = { {
    [LOADING_VIEW_STATE_BEGIN]             = loadingBeginViewLoadTask,
    [LOADING_VIEW_STATE_ENQUEUE_RESOURCES] = loadingEnqueueViewResourcesTask,
    [LOADING_VIEW_STATE_UPLOAD_IMAGE]      = loadingUploadViewImageTask,
    [LOADING_VIEW_STATE_WAIT_MOVIE_READY]  = _loadingWaitForMovieReadyTask,
    [LOADING_VIEW_STATE_WAIT_CD_IDLE]      = _loadingWaitForViewCdIdleTask,
    [LOADING_VIEW_STATE_WAIT_SCENE_IMAGE]  = _loadingWaitForSceneImageTask,
} };

/// Global-library file-ID hundreds components for player resource variants 1..5.
///
/// Copied as a five-byte value before selecting variant minus one. With group
/// 1 and low index 0, selects files 10400, 10300, 10200, 10500 and 10600.
/// The variant must be in range; the table contains no terminator.
static const _LoadingConfigFileHundreds _gLoadingCharacterFileHundreds = { { 4, 3, 2, 5, 6 } };

/// Restores discarded model storage and initializes the current view's cached sprite packets.
///
/// `keepGraphics` is the display preservation flag's snapshot: zero resets
/// queued GPU work, invalidates attached-model buffers, resets the auxiliary
/// heap and restores eligible model buffers. Any nonzero value preserves that
/// state. Both paths allocate and initialize new cached view-sprite packets.
///
/// Requires loaded view resources and a configured auxiliary heap. With zero,
/// all prior uses of its allocations must have ended; with nonzero, the heap
/// must already be initialized. Previous cached sprite storage must be retired
/// in either path. The source and storage contracts of
/// `tmdResetAuxHeapAndRestoreBuffers` and `spriteAllocateViewCachedPackets` apply.
/// Allocation failure does not abort the remaining work or report a status.
static inline void _loadingRestoreViewGraphics(s32 keepGraphics)
{
    if (keepGraphics == 0) {
        // Invalidate model pointers before resetting their backing storage.
        gpuResetAndInvalidateModelBuffers();
        tmdResetAuxHeapAndRestoreBuffers();
    }
    spriteAllocateViewCachedPackets();
}

/// Queues the equipped weapon's ammunition-dependent global resource package.
///
/// Requires live player/save data, valid file destinations and one free CD ring
/// slot when a package is selected. An unequipped or unsupported weapon queues
/// nothing. Request sources are copied immediately; loading is asynchronous.
/// Also requests the drive's later seek back to the current view.
static void _loadingEnqueueWeaponAmmoResources(void)
{
    /// Selects a weapon's ammunition-dependent file index, or zero when absent.
    ///
    /// Both arguments must be simple local lvalues without side effects: the key
    /// is written repeatedly, and the weapon index can be read again for its load.
    /// Reads `gPlayerStatus` and, for M4A1 Grenade, the live save's secondary load.
    /// Unsupported or empty ammunition selects that weapon's default package.
    /// Only `fileIndex` is written. The local binding is undefined after its use.
#define LOADING_SELECT_WEAPON_AMMO_FILE(fileKey, weaponIndex)                                                                                      \
    do {                                                                                                                                           \
        enum {                                                                                                                                     \
            LOADING_WEAPON_GRENADE_PISTOL             = 0xB,                                                                                       \
            LOADING_WEAPON_MM1                        = 0xC,                                                                                       \
            LOADING_WEAPON_PA3                        = 0xD,                                                                                       \
            LOADING_WEAPON_SP12                       = 0xE,                                                                                       \
            LOADING_WEAPON_AS12                       = 0xF,                                                                                       \
            LOADING_WEAPON_GUNBLADE                   = 0x17,                                                                                      \
            LOADING_WEAPON_M4A1_GRENADE               = 0x1B,                                                                                      \
            LOADING_AMMO_AIRBURST                     = 0xB,                                                                                       \
            LOADING_AMMO_RIOT                         = 0xC,                                                                                       \
            LOADING_AMMO_FIREFLY                      = 0xE,                                                                                       \
            LOADING_AMMO_SLUG                         = 0xF,                                                                                       \
            LOADING_AMMO_ITEM_INDEX_BASE              = 0x9F,                                                                                      \
            LOADING_AMMO_FILE_GRENADE_PISTOL_DEFAULT  = 1,                                                                                         \
            LOADING_AMMO_FILE_GRENADE_PISTOL_AIRBURST = 2,                                                                                         \
            LOADING_AMMO_FILE_GRENADE_PISTOL_RIOT     = 3,                                                                                         \
            LOADING_AMMO_FILE_MM1_DEFAULT             = 4,                                                                                         \
            LOADING_AMMO_FILE_MM1_AIRBURST            = 5,                                                                                         \
            LOADING_AMMO_FILE_MM1_RIOT                = 6,                                                                                         \
            LOADING_AMMO_FILE_PA3_DEFAULT             = 7,                                                                                         \
            LOADING_AMMO_FILE_PA3_FIREFLY             = 8,                                                                                         \
            LOADING_AMMO_FILE_PA3_SLUG                = 9,                                                                                         \
            LOADING_AMMO_FILE_SP12_DEFAULT            = 10,                                                                                        \
            LOADING_AMMO_FILE_SP12_FIREFLY            = 11,                                                                                        \
            LOADING_AMMO_FILE_SP12_SLUG               = 12,                                                                                        \
            LOADING_AMMO_FILE_AS12_DEFAULT            = 13,                                                                                        \
            LOADING_AMMO_FILE_M4A1_GRENADE_DEFAULT    = 16,                                                                                        \
            LOADING_AMMO_FILE_M4A1_GRENADE_AIRBURST   = 17,                                                                                        \
            LOADING_AMMO_FILE_M4A1_GRENADE_RIOT       = 18,                                                                                        \
            LOADING_AMMO_FILE_GUNBLADE_DEFAULT        = 19,                                                                                        \
            LOADING_AMMO_FILE_GUNBLADE_FIREFLY        = 20,                                                                                        \
            LOADING_AMMO_FILE_GUNBLADE_SLUG           = 21,                                                                                        \
        };                                                                                                                                         \
        s32 primaryAmmoIndex;                                                                                                                      \
        s32 secondaryAmmoIndex;                                                                                                                    \
                                                                                                                                                   \
        (fileKey).fileIndex = LOADING_AMMO_FILE_NONE;                                                                                              \
        switch ((weaponIndex)) {                                                                                                                   \
            case LOADING_WEAPON_GRENADE_PISTOL:                                                                                                    \
                (fileKey).fileIndex = LOADING_AMMO_FILE_GRENADE_PISTOL_DEFAULT;                                                                    \
                if (gPlayerStatus.weaponSlotItem == LOADING_AMMO_AIRBURST) {                                                                       \
                    (fileKey).fileIndex = LOADING_AMMO_FILE_GRENADE_PISTOL_AIRBURST;                                                               \
                }                                                                                                                                  \
                if (gPlayerStatus.weaponSlotItem == LOADING_AMMO_RIOT) {                                                                           \
                    (fileKey).fileIndex = LOADING_AMMO_FILE_GRENADE_PISTOL_RIOT;                                                                   \
                }                                                                                                                                  \
                break;                                                                                                                             \
            case LOADING_WEAPON_MM1:                                                                                                               \
                (fileKey).fileIndex = LOADING_AMMO_FILE_MM1_DEFAULT;                                                                               \
                if (gPlayerStatus.weaponSlotItem == LOADING_AMMO_AIRBURST) {                                                                       \
                    (fileKey).fileIndex = LOADING_AMMO_FILE_MM1_AIRBURST;                                                                          \
                }                                                                                                                                  \
                if (gPlayerStatus.weaponSlotItem == LOADING_AMMO_RIOT) {                                                                           \
                    (fileKey).fileIndex = LOADING_AMMO_FILE_MM1_RIOT;                                                                              \
                }                                                                                                                                  \
                break;                                                                                                                             \
            case LOADING_WEAPON_PA3:                                                                                                               \
                (fileKey).fileIndex = LOADING_AMMO_FILE_PA3_DEFAULT;                                                                               \
                if (gPlayerStatus.weaponSlotItem == LOADING_AMMO_FIREFLY) {                                                                        \
                    (fileKey).fileIndex = LOADING_AMMO_FILE_PA3_FIREFLY;                                                                           \
                }                                                                                                                                  \
                if (gPlayerStatus.weaponSlotItem == LOADING_AMMO_SLUG) {                                                                           \
                    (fileKey).fileIndex = LOADING_AMMO_FILE_PA3_SLUG;                                                                              \
                }                                                                                                                                  \
                break;                                                                                                                             \
            case LOADING_WEAPON_SP12:                                                                                                              \
                (fileKey).fileIndex = LOADING_AMMO_FILE_SP12_DEFAULT;                                                                              \
                if (gPlayerStatus.weaponSlotItem == LOADING_AMMO_FIREFLY) {                                                                        \
                    (fileKey).fileIndex = LOADING_AMMO_FILE_SP12_FIREFLY;                                                                          \
                }                                                                                                                                  \
                if (gPlayerStatus.weaponSlotItem == LOADING_AMMO_SLUG) {                                                                           \
                    (fileKey).fileIndex = LOADING_AMMO_FILE_SP12_SLUG;                                                                             \
                }                                                                                                                                  \
                break;                                                                                                                             \
            case LOADING_WEAPON_AS12:                                                                                                              \
                (fileKey).fileIndex = LOADING_AMMO_FILE_AS12_DEFAULT;                                                                              \
                primaryAmmoIndex    = gPlayerStatus.weaponSlotItem;                                                                                \
                if (primaryAmmoIndex == LOADING_AMMO_FIREFLY) {                                                                                    \
                    (fileKey).fileIndex = primaryAmmoIndex;                                                                                        \
                }                                                                                                                                  \
                if (primaryAmmoIndex == LOADING_AMMO_SLUG) {                                                                                       \
                    (fileKey).fileIndex = primaryAmmoIndex;                                                                                        \
                }                                                                                                                                  \
                break;                                                                                                                             \
            case LOADING_WEAPON_GUNBLADE:                                                                                                          \
                (fileKey).fileIndex = LOADING_AMMO_FILE_GUNBLADE_DEFAULT;                                                                          \
                if (gPlayerStatus.weaponSlotItem == LOADING_AMMO_FIREFLY) {                                                                        \
                    (fileKey).fileIndex = LOADING_AMMO_FILE_GUNBLADE_FIREFLY;                                                                      \
                }                                                                                                                                  \
                if (gPlayerStatus.weaponSlotItem == LOADING_AMMO_SLUG) {                                                                           \
                    (fileKey).fileIndex = LOADING_AMMO_FILE_GUNBLADE_SLUG;                                                                         \
                }                                                                                                                                  \
                break;                                                                                                                             \
                /* This attachment selects its package from the secondary ammunition load. */                                                      \
            case LOADING_WEAPON_M4A1_GRENADE: {                                                                                                    \
                EquipmentWeaponLoad* weaponLoad;                                                                                                   \
                                                                                                                                                   \
                (fileKey).fileIndex = LOADING_AMMO_FILE_M4A1_GRENADE_DEFAULT;                                                                      \
                weaponLoad          = equipmentGetWeaponLoad((weaponIndex) + (EQUIPMENT_WEAPON_ITEM_FIRST - 1));                                   \
                if (weaponLoad->secondaryItemId != INVENTORY_ITEM_NONE && weaponLoad->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) { \
                    secondaryAmmoIndex = weaponLoad->secondaryItemId - LOADING_AMMO_ITEM_INDEX_BASE;                                               \
                    if (secondaryAmmoIndex == LOADING_AMMO_AIRBURST) {                                                                             \
                        (fileKey).fileIndex = LOADING_AMMO_FILE_M4A1_GRENADE_AIRBURST;                                                             \
                    }                                                                                                                              \
                    if (secondaryAmmoIndex == LOADING_AMMO_RIOT) {                                                                                 \
                        (fileKey).fileIndex = LOADING_AMMO_FILE_M4A1_GRENADE_RIOT;                                                                 \
                    }                                                                                                                              \
                }                                                                                                                                  \
                break;                                                                                                                             \
            }                                                                                                                                      \
        }                                                                                                                                          \
    } while (0)
    enum { LOADING_AMMO_FILE_HUNDREDS = 10 };
    CdCmdFileKey      fileKey;
    CdCmdFileLoadArgs loadArgs;
    u16               weaponIndex;

    weaponIndex = gPlayerStatus.weapon;
    if (weaponIndex == PLAYER_STATUS_EQUIPMENT_NONE) {
        return;
    }

    LOADING_SELECT_WEAPON_AMMO_FILE(fileKey, weaponIndex);
#undef LOADING_SELECT_WEAPON_AMMO_FILE

    if (fileKey.fileIndex == LOADING_AMMO_FILE_NONE) {
        return;
    }

    fileKey.stage             = LOADING_GLOBAL_CDF;
    fileKey.fileGroup         = LOADING_PLAYER_FILE_GROUP;
    loadArgs.fileIdHundreds   = LOADING_AMMO_FILE_HUNDREDS;
    loadArgs.imageYOffset     = 0;
    loadArgs.imageXPageOffset = 0;
    loadArgs.loadMode         = CD_COMMAND_LOAD_DEFAULT;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, &fileKey, &loadArgs);
    gCdCmdViewSeekPending = LOADING_VIEW_SEEK_PENDING;
}

void loadingEnqueueViewResourcesTask(Task* task)
{
    GameLocationKey*  location;
    CdCmdFileKey      fileKey;
    CdCmdFileLoadArgs loadArgs;

    location = &gGameSession->location;
    if (cdCmdIsIdle()) {
        fileKey.stage             = location->stage;
        fileKey.fileGroup         = location->area;
        fileKey.fileIndex         = viewGetMappedIndex();
        loadArgs.fileIdHundreds   = LOADING_VIEW_FOLDER_SUFFIX;
        loadArgs.loadMode         = CD_COMMAND_LOAD_DEFAULT;
        loadArgs.imageXPageOffset = 0;
        loadArgs.imageYOffset     = 0;
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, &fileKey, &loadArgs);
        task->state++;
    }
}

/// Finishes a view load after three updates observing a started or skipped movie.
///
/// State 3 of `loadingViewLoadTask`. `killCountdown` starts at zero when the
/// movie is queued and counts ready observations, not encoded movie frames;
/// updates without readiness leave the count intact. Requires a live task and
/// the completion resources of `loadingFinishViewLoad`.
static void _loadingWaitForMovieReadyTask(Task* viewLoadTask)
{
    if (gCdCmdQueue.movieReady != 0) {
        viewLoadTask->killCountdown++;
    }
    if (viewLoadTask->killCountdown >= LOADING_VIEW_MOVIE_READY_TICKS) {
        viewLoadTask->state = LOADING_VIEW_FINISH_READY;
        loadingFinishViewLoad(viewLoadTask);
    }
}

/// Finishes a view load when normal CD dispatch has an empty request ring.
///
/// State 4 of `loadingViewLoadTask`, reached when an image reload has queued a
/// deferred replacement without a movie. Requires a live task and the resources
/// of `loadingFinishViewLoad`; queue idle does not test drive/display busy state.
static void _loadingWaitForViewCdIdleTask(Task* viewLoadTask)
{
    if (cdCmdIsIdle()) {
        viewLoadTask->state = LOADING_VIEW_FINISH_CD_IDLE;
        loadingFinishViewLoad(viewLoadTask);
    }
}

/// Finishes a view load when the cached scene-image decoder marks completion.
///
/// State 5 of `loadingViewLoadTask`. The completion latch also covers decoder
/// timeout cleanup; it is not a success status. Requires a live task and the
/// completion resources of `loadingFinishViewLoad`.
static void _loadingWaitForSceneImageTask(Task* viewLoadTask)
{
    if (gCdCmdQueue.imageLoadStatus == CD_COMMAND_IMAGE_COMPLETE) {
        viewLoadTask->state = LOADING_VIEW_FINISH_READY;
        loadingFinishViewLoad(viewLoadTask);
    }
}

void loadingUploadCachedViewImage(void)
{
    enum { LOADING_VIEW_IMAGE_IGNORE_GPU_TIME_LIMIT = 1 };
    u8 mappedViewIndex;
    u8 resourceSlotIndex;

    mappedViewIndex = viewGetMappedIndex();
    for (resourceSlotIndex = 0; resourceSlotIndex < ARRAY_SIZE(gFsResourceSlots); resourceSlotIndex++) {
        if (gFsResourceSlots[resourceSlotIndex].kind == FILE_SYSTEM_RESOURCE_IMAGE && mappedViewIndex - 1 == resourceSlotIndex) {
            // Keep retrying the retained chunk, including after timer failure.
            while (fsUploadImageChunk(gFsResourceSlots[resourceSlotIndex].data, LOADING_VIEW_IMAGE_IGNORE_GPU_TIME_LIMIT) != FILE_SYSTEM_IMAGE_UPLOAD_COMPLETE) {
            }
            break;
        }
    }
}

void loadingFinishViewLoad(Task* viewLoadTask)
{
    padClearInputBlock(LOADING_VIEW_INPUT_PORT);
    if (viewLoadTask->spawnArg1.value == LOADING_VIEW_RESUME_GAME_LOOP) {
        stageMusicUpdateAreaAmbient(1);
        gGameSession->viewDirty = LOADING_VIEW_RELOAD_CLEAR;
        taskKill(viewLoadTask);
        displayResumeGameLoop();
    } else {
        // Scene-owned loads publish readiness without returning presentation.
        if (viewLoadTask->spawnArg1.value == LOADING_VIEW_RESUME_TASK_FLIPS) {
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
        }
        gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_TRANSITION_STRIPS;
        taskSpawn(LOADING_VIEW_TASK_BANK, LOADING_VIEW_SPRITE_TASK_TYPE, 0, 0);
        gGameSession->viewReady = LOADING_VIEW_READY;
        taskKill(viewLoadTask);
    }
}

void loadingViewLoadTask(Task* viewLoadTask)
{
    TaskFuncTable6 stateHandlers;

    stateHandlers = _gLoadingViewStateHandlers;
    padStartInputBlock(LOADING_VIEW_INPUT_PORT);
    // Finish markers must be intercepted before indexing the six handlers.
    if (viewLoadTask->state < 0) {
        loadingFinishViewLoad(viewLoadTask);
    } else {
        stateHandlers.funcs[viewLoadTask->state](viewLoadTask);
    }
}

/// Resets queued GPU drawing and discards both resident frame ordering tables.
///
/// Prior GPU users of both tables must have finished. No heap or model storage
/// is released; subsequent view setup supplies new packets. Resets the GPU
/// command queue before clearing frame buffer 0's table, then frame buffer 1's.
/// Leaves the selected ordering table and primitive allocation cursor intact.
static inline void _loadingResetViewDrawing(void)
{
    enum { LOADING_VIEW_GPU_RESET_QUEUE = 1 };
    ResetGraph(LOADING_VIEW_GPU_RESET_QUEUE);
    gpuClearFrameOrderingTable(0);
    gpuClearFrameOrderingTable(1);
}

/// Starts reloading the live save's logical view under task-owned presentation.
///
/// Requires a live view-gate task, session/save, valid loaded view resources,
/// finished GPU uses of both frame ordering tables, and task allocation capacity.
/// Camera resources must remain live until the queued camera task runs.
/// Copies only the saved view byte, clears queued drawing, queues its camera
/// without cached-sprite setup, clears readiness and spawns a mode-1 view load.
/// Does not wait for the load or handle task-allocation failure.
static void _loadingReloadSavedView(void)
{
    Task*       viewGateTask;
    McSaveData* liveSave;

    viewGateTask                  = gameGetTaskSlot(GAME_TASK_SLOT_VIEW_GATE);
    liveSave                      = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    viewGateTask->spawnArg1.value = liveSave->state.location.view;
    _loadingResetViewDrawing();
    gGameSession->location.view = liveSave->state.location.view;
    padStartInputBlock(LOADING_VIEW_INPUT_PORT);
    viewQueueCurrentCamera(VIEW_PACKET_LIST_NONE);
    gGameSession->viewReady = LOADING_VIEW_NOT_READY;
    taskSpawn(LOADING_VIEW_TASK_BANK, LOADING_VIEW_TASK_TYPE, LOADING_VIEW_RESUME_TASK_FLIPS, 0);
}

/// Starts reloading a logical view and returns presentation to the game loop on completion.
///
/// Uses the low byte of `logicalViewIndex` as the one-based view in the live
/// save/session and view-gate cache. Requires that byte to select loaded view
/// resources, a live gate task and task allocation capacity. Camera resources
/// must remain live until the queued camera task runs. Queues the camera on the
/// selected list and cached-sprite setup on the default list, selects image
/// strips and spawns a mode-0 view load; does not wait or handle allocation failure.
static void _loadingReloadView(s32 logicalViewIndex)
{
    Task* viewGateTask;

    viewGateTask                                           = gameGetTaskSlot(GAME_TASK_SLOT_VIEW_GATE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = logicalViewIndex;
    gGameSession->location.view                            = logicalViewIndex;
    viewGateTask->spawnArg1.value                          = (u8)logicalViewIndex;
    padStartInputBlock(LOADING_VIEW_INPUT_PORT);
    viewQueueCurrentCamera(VIEW_PACKET_LIST_DEFAULT);
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
    taskSpawn(LOADING_VIEW_TASK_BANK, LOADING_VIEW_TASK_TYPE, LOADING_VIEW_RESUME_GAME_LOOP, 0);
}

void viewCommitIndexTask(Task* task)
{
    u8 viewIndex;

    viewIndex                                              = (u8)task->spawnArg1.value;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = viewIndex;
    gGameSession->location.view                            = viewIndex;
    taskKill(task);
}

void loadingRequestViewGraphicsRestore(void)
{
    enum {
        LOADING_VIEW_GRAPHICS_RESTORE_TASK_BANK = 0,
        LOADING_VIEW_GRAPHICS_RESTORE_TASK_SLOT = 0x26,
    };
    displaySpawnTask(LOADING_VIEW_GRAPHICS_RESTORE_TASK_BANK, LOADING_VIEW_GRAPHICS_RESTORE_TASK_SLOT, 0, 0);
}

void loadingRestoreViewGraphicsTask(Task* task)
{
    DisplayState* display;
    s32           keepGraphics;

    display                         = &gDisplayState;
    keepGraphics                    = display->keepGraphics;
    display->control.flags.flipMode = DISPLAY_FLIP_HOLD;
    // Finish rebuilding graphics before returning presentation to the game loop.
    _loadingRestoreViewGraphics(keepGraphics);
    taskKill(task);
    displayResumeGameLoop();
}

void loadingRestoreViewImageAndEnqueueResources(u8 skipBackground)
{
    u8                mappedViewIndex;
    u8                resourceSlotIndex;
    GameSession*      session;
    CdCmdFileLoadArgs loadArgs;
    CdCmdFileKey      fileKey;

    // Restore the retained image before the queued file reload can replace resources.
    mappedViewIndex = viewGetMappedIndex();
    for (resourceSlotIndex = 0; resourceSlotIndex < ARRAY_SIZE(gFsResourceSlots); resourceSlotIndex++) {
        if (gFsResourceSlots[resourceSlotIndex].kind == FILE_SYSTEM_RESOURCE_IMAGE) {
            if (mappedViewIndex - 1 == resourceSlotIndex) {
                while (fsUploadImageChunk(gFsResourceSlots[resourceSlotIndex].data, 1) != FILE_SYSTEM_IMAGE_UPLOAD_COMPLETE) {
                }
                break;
            }
        }
    }
    session                 = gGameSession;
    fileKey.stage           = session->location.stage;
    fileKey.fileGroup       = session->location.area;
    fileKey.fileIndex       = viewGetMappedIndex();
    loadArgs.fileIdHundreds = LOADING_VIEW_FOLDER_SUFFIX;
    if (skipBackground != 0) {
        loadArgs.loadMode = CD_COMMAND_LOAD_SKIP_BACKGROUND;
    } else {
        loadArgs.loadMode = CD_COMMAND_LOAD_DEFAULT;
    }
    loadArgs.imageYOffset     = 0;
    loadArgs.imageXPageOffset = 0;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, &fileKey, &loadArgs);
}

void loadingEnqueueCharacterResources(s32 imagesOnly)
{
    enum {
        LOADING_CHARACTER_FILE_INDEX          = 0,
        LOADING_CHARACTER_IMAGE_X_PAGE_OFFSET = 6,
    };
    CdCmdFileKey               fileKey;
    CdCmdFileLoadArgs          loadArgs;
    _LoadingConfigFileHundreds fileHundredsByVariant;

    fileHundredsByVariant = _gLoadingCharacterFileHundreds;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId != 0) {
        fileKey.stage           = LOADING_GLOBAL_CDF;
        fileKey.fileGroup       = LOADING_PLAYER_FILE_GROUP;
        fileKey.fileIndex       = LOADING_CHARACTER_FILE_INDEX;
        loadArgs.fileIdHundreds = fileHundredsByVariant.fileIdHundreds[gPlayerStatus.resourceVariant - 1];
        if ((u8)imagesOnly == 0) {
            loadArgs.loadMode = CD_COMMAND_LOAD_DEFAULT;
        } else {
            loadArgs.loadMode = CD_COMMAND_LOAD_IMAGES_ONLY;
        }
        loadArgs.imageXPageOffset = LOADING_CHARACTER_IMAGE_X_PAGE_OFFSET;
        loadArgs.imageYOffset     = 0;
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, &fileKey, &loadArgs);
    }
}

void loadingEnqueueEquippedWeaponResources(void)
{
    enum { LOADING_WEAPON_FILE_HUNDREDS      = 3,
           LOADING_UNARMED_WEAPON_FILE_INDEX = 1 };
    CdCmdFileKey      fileKey;
    CdCmdFileLoadArgs loadArgs;
    u8                weaponIndex;

    weaponIndex = gPlayerStatus.weapon;
    if (weaponIndex == PLAYER_STATUS_EQUIPMENT_NONE) {
        weaponIndex = LOADING_UNARMED_WEAPON_FILE_INDEX;
    }
    fileKey.fileIndex         = weaponIndex;
    fileKey.stage             = LOADING_GLOBAL_CDF;
    fileKey.fileGroup         = LOADING_PLAYER_FILE_GROUP;
    loadArgs.fileIdHundreds   = LOADING_WEAPON_FILE_HUNDREDS;
    loadArgs.imageYOffset     = 0;
    loadArgs.imageXPageOffset = 0;
    loadArgs.loadMode         = CD_COMMAND_LOAD_DEFAULT;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, &fileKey, &loadArgs);
    // Loading this library file moves the drive away from the current view.
    gCdCmdViewSeekPending = LOADING_VIEW_SEEK_PENDING;
    _loadingEnqueueWeaponAmmoResources();
}

void loadingEnqueueStageResources(void)
{
    enum { LOADING_STAGE_MAP_FILE_GROUP = 90 };
    CdCmdFileKey      fileKey;
    CdCmdFileLoadArgs loadArgs;

    // Mount the new CDF before loading its stage-specific map and room names.
    cdCmdEnqueue(CD_COMMAND_MOUNT_STAGE, &gGameSession->location, NULL);
    fileKey.stage             = LOADING_GLOBAL_CDF;
    fileKey.fileGroup         = LOADING_STAGE_MAP_FILE_GROUP;
    fileKey.fileIndex         = gGameSession->location.stage;
    loadArgs.imageYOffset     = 0;
    loadArgs.imageXPageOffset = 0;
    loadArgs.loadMode         = CD_COMMAND_LOAD_DEFAULT;
    loadArgs.fileIdHundreds   = 0;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, &fileKey, &loadArgs);
}

void companionEnqueueResources(u8 companionType, u8 resourceVariant)
{
    enum {
        COMPANION_TYPE_NONE                     = 0,
        COMPANION_RESOURCE_BASE_FILE_INDEX      = 0,
        COMPANION_RESOURCE_FILE_GROUP           = 80,
        COMPANION_REQUEST_SCRATCH_BYTES         = 8,
        COMPANION_RESOURCE_IMAGE_X_PAGE_OFFSET  = 4,
        COMPANION_RESOURCE_IMAGE_Y_OFFSET       = 6,
        COMPANION_RESOURCE_VARIANT_REMAP_SOURCE = 5,
        COMPANION_RESOURCE_VARIANT_REMAP_TARGET = 3,
    };
    CdCmdFileLoadArgs loadArgs;
    CdCmdFileKey*     fileKey;

    if (companionType == COMPANION_TYPE_NONE) {
        return;
    }

    // Only the first four bytes of this eight-byte scratch reservation form the key.
    fileKey                   = SCRATCH_STACK_RESERVE_BYTES(COMPANION_REQUEST_SCRATCH_BYTES);
    gGameSession->field_80    = 0;
    fileKey->stage            = LOADING_GLOBAL_CDF;
    fileKey->fileGroup        = COMPANION_RESOURCE_FILE_GROUP;
    fileKey->fileIndex        = COMPANION_RESOURCE_BASE_FILE_INDEX;
    loadArgs.fileIdHundreds   = companionType;
    loadArgs.loadMode         = CD_COMMAND_LOAD_DEFAULT;
    loadArgs.imageXPageOffset = COMPANION_RESOURCE_IMAGE_X_PAGE_OFFSET;
    loadArgs.imageYOffset     = COMPANION_RESOURCE_IMAGE_Y_OFFSET;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, &loadArgs);

    if (resourceVariant != COMPANION_RESOURCE_BASE_FILE_INDEX) {
        fileKey->stage            = LOADING_GLOBAL_CDF;
        fileKey->fileGroup        = COMPANION_RESOURCE_FILE_GROUP;
        fileKey->fileIndex        = resourceVariant;
        loadArgs.fileIdHundreds   = companionType;
        loadArgs.loadMode         = CD_COMMAND_LOAD_DEFAULT;
        loadArgs.imageXPageOffset = COMPANION_RESOURCE_IMAGE_X_PAGE_OFFSET;
        loadArgs.imageYOffset     = COMPANION_RESOURCE_IMAGE_Y_OFFSET;
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, &loadArgs);
        // Variant 5 loads a special package but subsequently uses variant 3 actor data.
        if (resourceVariant == COMPANION_RESOURCE_VARIANT_REMAP_SOURCE) {
            gGameSession->companionVariant                            = COMPANION_RESOURCE_VARIANT_REMAP_TARGET;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant = COMPANION_RESOURCE_VARIANT_REMAP_TARGET;
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(COMPANION_REQUEST_SCRATCH_BYTES);
}

void companionRelocateModelTextures(Task* companionTask)
{
    enum {
        COMPANION_MODEL_TEXTURE_PAGE_OFFSET = 4,
        COMPANION_MODEL_CLUT_ROW_OFFSET     = 6
    };
    TmdObject* model;

    model = companionTask->extra.tmd;
    if (companionTask->bodyKind == TASK_BODY_TMD) {
        model->texturePageOffset = COMPANION_MODEL_TEXTURE_PAGE_OFFSET;
        model->clutRowOffset     = COMPANION_MODEL_CLUT_ROW_OFFSET;
        if (model->buffer != NULL) {
            // Each build toggles the half selector; two refresh both and restore it.
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
    }
}

const TaskFuncTable3 gGameFlowReloadStateHandlers      = { {
    [GAME_FLOW_RELOAD_STATE_PREPARE]      = gameFlowPrepareSessionReloadTask,
    [GAME_FLOW_RELOAD_STATE_HOLD_DISPLAY] = gameFlowHoldSessionDisplayTask,
    [GAME_FLOW_RELOAD_STATE_REBUILD]      = gameFlowRebuildSessionTask,
} };
const TaskFuncTable8 gLoadingSessionStateHandlers      = { {
    [LOADING_SESSION_STATE_PREPARE_CHARACTER]  = loadingPrepareCharacterResourcesTask,
    [LOADING_SESSION_STATE_ENQUEUE_STAGE]      = loadingEnqueueStageResourcesTask,
    [LOADING_SESSION_STATE_INITIALIZE_AREA]    = loadingInitializeAreaMemoryAndAudioTask,
    [LOADING_SESSION_STATE_ENQUEUE_AREA]       = loadingEnqueueAreaAndCompanionResourcesTask,
    [LOADING_SESSION_STATE_PREPARE_AREA]       = loadingPrepareAreaStateTask,
    [LOADING_SESSION_STATE_POLL_AREA]          = loadingPollAreaResourcesTask,
    [LOADING_SESSION_STATE_RELEASE_BOOT_IMAGE] = loadingHoldFadeAndReleaseBootImageTask,
    [LOADING_SESSION_STATE_START_ROOM]         = loadingFinishSessionLoadTask,
} };
const TaskFuncTable3 gLoadingRoomResourceStateHandlers = { {
    [LOADING_ROOM_RESOURCE_STATE_INITIALIZE] = loadingInitRoomResourcesTask,
    [LOADING_ROOM_RESOURCE_STATE_UPDATE]     = loadingUpdateRoomResourcesTask,
    [LOADING_ROOM_RESOURCE_STATE_TEARDOWN]   = taskKill,
} };
