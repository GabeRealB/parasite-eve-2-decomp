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

#include "rooms/acropolis_bridge.h"

#include "rooms/acropolis_cafeteria.h"

#include "rooms/acropolis_east_elevator_hall.h"

#include "rooms/acropolis_fire_escape.h"

#include "rooms/acropolis_forked_road.h"

#include "rooms/acropolis_fountain.h"

#include "rooms/acropolis_hallway.h"

#include "rooms/acropolis_helicopter_landing_pad.h"

#include "rooms/acropolis_observatory.h"

#include "rooms/acropolis_patio.h"

#include "rooms/acropolis_plaza.h"

#include "rooms/acropolis_promenade.h"

#include "rooms/acropolis_roof_garden.h"

#include "rooms/acropolis_sanctuary.h"

#include "rooms/acropolis_security_room.h"

#include "rooms/acropolis_square.h"

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

#include "rooms/mist_parking.h"

#include "rooms/mist_r18.h"

#include "rooms/mist_shooting_gallery.h"

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

/// View-load completion markers and the bank-0 task's presentation policies.
enum {
    LOADING_VIEW_FINISH_READY      = -1,
    LOADING_VIEW_FINISH_CD_IDLE    = -2,
    LOADING_VIEW_MOVIE_READY_TICKS = 3,
    LOADING_VIEW_INPUT_PORT        = 0,
    LOADING_VIEW_TASK_BANK         = 0,
    LOADING_VIEW_TASK_TYPE         = 0x1E,
    LOADING_VIEW_SPRITE_TASK_TYPE  = 0x17,
    LOADING_VIEW_RESUME_GAME_LOOP  = 0,
    LOADING_VIEW_RESUME_TASK_FLIPS = 1,
    LOADING_VIEW_NOT_READY         = 0,
    LOADING_VIEW_READY             = 1,
    LOADING_VIEW_RELOAD_CLEAR      = 0,
};

extern AreaRecord D_8010CBE4[21];

/// End marker following the stage 1 room table.
extern u32 D_8010CC8C[2];

extern AreaRecord D_8010CC94[39];

/// End marker following the stage 2 room table.
extern u32 D_8010CDCC[2];

extern AreaRecord D_8010CDD4[39];

/// End marker following the stage 3 room table.
extern u32 D_8010CF0C[2];

extern AreaRecord D_8010CF14[50];

/// End marker following the stage 4 room table.
extern u32 D_8010D0A4[2];

extern AreaRecord D_8010D0AC[34];

/// End marker following the stage 5 room table.
extern u32 D_8010D1BC[2];

static const TaskFuncTable6 Gp_LoadWaitFns;

static const _LoadingConfigFileHundreds Gp_ConfigCdTable;

static void _loadingEnqueueWeaponAmmoResources(void);

static void _loadingWaitForMovieReadyTask(Task* viewLoadTask);

static void _loadingWaitForViewCdIdleTask(Task* viewLoadTask);

static void _loadingWaitForSceneImageTask(Task* viewLoadTask);

static void _loadingReloadSavedView(void);

static void _loadingReloadView(s32 logicalViewIndex);

AreaRecord* Gp_AreaTables[6] = { NULL, D_8010CBE4, D_8010CC94, D_8010CDD4, D_8010CF14, D_8010D0AC };
AreaRecord  D_8010CBE4[21]   = {
    { NULL, NULL },
    { D_acropolis_square_80185E50, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[0].state },
    { D_acropolis_east_elevator_hall_80186C50, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[1].state },
    { D_acropolis_patio_80184A90, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[2].state },
    { D_acropolis_cafeteria_80189DCC, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[3].state },
    { D_acropolis_plaza_80199390, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[4].state },
    { D_acropolis_security_room_80184088, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[5].state },
    { D_acropolis_hallway_8017EA3C, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[6].state },
    { D_acropolis_fountain_8017FC9C, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[7].state },
    { D_acropolis_forked_road_801831C4, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[8].state },
    { D_acropolis_observatory_80181264, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[9].state },
    { D_acropolis_promenade_80183020, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[10].state },
    { D_acropolis_sanctuary_8018402C, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[11].state },
    { D_acropolis_roof_garden_80185934, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[12].state },
    { D_acropolis_bridge_8019004C, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[13].state },
    { D_acropolis_fire_escape_8018294C, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[14].state },
    { D_acropolis_helicopter_landing_pad_801861E8, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[15].state },
    { NULL, NULL },
    { D_mist_r18_80186BFC, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[16].state },
    { D_mist_parking_801951B4, &gGameFlagAcropolisBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[17].state },
    { D_mist_shooting_gallery_8018DF74, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[29].state },
};
/// End marker following the stage 1 room table.
u32        D_8010CC8C[2]  = { 0xFFFF, 0 };
AreaRecord D_8010CC94[39] = {
    { NULL, NULL },
    { D_dryfield_gas_station_80184A38, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[0].state },
    { D_dryfield_main_street_80184F20, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[1].state },
    { D_dryfield_general_store_80185654, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[2].state },
    { NULL, NULL },
    { D_dryfield_back_street_80180A50, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[3].state },
    { D_dryfield_souvenir_shop_8017F598, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[4].state },
    { D_dryfield_warehouse_80182100, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[5].state },
    { D_dryfield_r08_80180B88, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[33].state },
    { D_dryfield_dilapidated_house_80189938, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[6].state },
    { NULL, NULL },
    { D_dryfield_motel_room_1_801814DC, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[7].state },
    { D_dryfield_motel_room_2_80180410, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[8].state },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_parking_lot_8017FA74, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[11].state },
    { D_dryfield_toilet_80182918, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[12].state },
    { NULL, NULL },
    { D_dryfield_saloon_g_r_80181B1C, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[13].state },
    { D_dryfield_g_r_kitchen_8017F4B8, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[14].state },
    { D_dryfield_water_tower_8018757C, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[15].state },
    { D_dryfield_water_tank_80188BF0, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[16].state },
    { D_dryfield_breezeway_801842F8, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[17].state },
    { NULL, NULL },
    { D_dryfield_garage_801800E0, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[18].state },
    { D_dryfield_driveway_8017EDD4, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[19].state },
    { D_dryfield_junk_yard_8017F558, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[20].state },
    { D_dryfield_trailer_coach_801876F0, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[21].state },
    { NULL, NULL },
    { D_dryfield_motel_balcony_80186220, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[23].state },
    { D_dryfield_motel_room_6_80186764, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[24].state },
    { NULL, NULL },
    { D_dryfield_water_hole_801827BC, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[26].state },
    { NULL, NULL },
    { D_dryfield_cellar_80180ACC, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[27].state },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_underpass_8017F868, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[28].state },
};
/// End marker following the stage 2 room table.
u32        D_8010CDCC[2]  = { 0xFFFF, 0 };
AreaRecord D_8010CDD4[39] = {
    { NULL, NULL },
    { D_dryfield_night_gas_station_80190624, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[0].state },
    { D_dryfield_night_main_street_80188A08, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[1].state },
    { D_dryfield_night_general_store_8018572C, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[2].state },
    { NULL, NULL },
    { D_dryfield_night_back_street_80181518, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[3].state },
    { D_dryfield_night_souvenir_shop_8017F62C, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[4].state },
    { D_dryfield_night_warehouse_8017FB98, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[5].state },
    { D_dryfield_night_r08_801818D8, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[33].state },
    { D_dryfield_night_dilapidated_house_80189FA4, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[6].state },
    { NULL, NULL },
    { D_dryfield_night_motel_room_1_8018075C, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[7].state },
    { D_dryfield_night_motel_room_2_801809A0, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[8].state },
    { D_dryfield_night_motel_room_3_80180D24, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[9].state },
    { D_dryfield_night_motel_room_4_801802FC, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[10].state },
    { D_dryfield_night_parking_lot_80181438, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[11].state },
    { D_dryfield_night_toilet_8017F354, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[12].state },
    { gDryfieldNightMotelLobbyAreaVariants, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[34].state },
    { D_dryfield_night_saloon_g_r_80188EE4, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[13].state },
    { D_dryfield_night_g_r_kitchen_8017EB88, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[14].state },
    { D_dryfield_night_water_tower_80182B5C, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[15].state },
    { D_dryfield_night_water_tank_80180764, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[16].state },
    { D_dryfield_night_breezeway_801803E4, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[17].state },
    { D_dryfield_night_factory_8018A70C, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[35].state },
    { D_dryfield_night_garage_801874BC, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[18].state },
    { D_dryfield_night_driveway_80181F4C, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[19].state },
    { D_dryfield_night_junk_yard_801843F0, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[20].state },
    { D_dryfield_night_trailer_coach_8018C15C, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[21].state },
    { D_dryfield_night_motel_room_5_80181190, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[22].state },
    { D_dryfield_night_motel_balcony_8018EA94, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[23].state },
    { D_dryfield_night_motel_room_6_801861B4, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[24].state },
    { D_dryfield_night_motel_loft_80180888, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[25].state },
    { D_dryfield_night_water_hole_80183418, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[26].state },
    { NULL, NULL },
    { D_dryfield_night_cellar_80180780, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[27].state },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_night_underpass_801802DC, &gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[28].state },
};
/// End marker following the stage 3 room table.
u32        D_8010CF0C[2]  = { 0xFFFF, 0 };
AreaRecord D_8010CF14[50] = {
    { NULL, NULL },
    { gMineMesaAreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[0].state },
    { D_mine_cavern_8018E238, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[1].state },
    { D_mine_tunnel_entrance_8017F32C, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[2].state },
    { D_mine_tunnel_801801CC, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[3].state },
    { D_mine_gorge_80183544, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[4].state },
    { D_mine_refuge_80182A00, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[5].state },
    { D_mine_forked_tunnel_80185504, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[6].state },
    { D_mine_secret_passage_80183340, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[7].state },
    { D_shelter_b1_elevator_hall_80184940, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[8].state },
    { D_shelter_b1_south_maintenance_walkway_80183598, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[9].state },
    { D_shelter_b1_storeroom_80186C9C, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[10].state },
    { D_shelter_b1_north_maintenance_walkway_80185A38, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[11].state },
    { D_shelter_b1_armory_801854E0, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[12].state },
    { D_shelter_b1_sleeping_quarters_80183FDC, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[13].state },
    { D_shelter_b1_main_corridor_80185C30, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[14].state },
    { D_shelter_b1_sterilization_room_8018C14C, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[15].state },
    { D_shelter_b1_pod_access_tunnel_80184C00, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[16].state },
    { D_shelter_b1_control_room_80183A98, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[17].state },
    { D_shelter_b1_access_tunnel_8017FE50, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[18].state },
    { D_shelter_b1_underground_parking_8018B5C4, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[19].state },
    { gShelterB1GolemFreezer1AreaVariants, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[20].state },
    { D_shelter_b2_pod_bottom_80187678, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[21].state },
    { D_shelter_b1_pod_service_gantry_8017FBF8, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[22].state },
    { D_shelter_b1_transfer_tunnel_801830B8, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[23].state },
    { D_shelter_b1_control_room_access_tunnel_801825AC, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[24].state },
    { D_shelter_b2_elevator_8017E964, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[25].state },
    { D_shelter_b2_elevator_hall_80184C7C, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[26].state },
    { D_shelter_b2_south_maintenance_walkway_801837AC, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[27].state },
    { D_shelter_b2_operating_room_80184124, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[28].state },
    { D_shelter_b2_north_maintenance_walkway_80186258, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[29].state },
    { D_shelter_b2_laboratory_80186360, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[30].state },
    { D_shelter_b2_breeding_room_80183EEC, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[31].state },
    { D_shelter_b2_main_corridor_8018933C, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[32].state },
    { D_shelter_b2_septic_tank_80186F40, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[33].state },
    { D_shelter_b2_pod_access_tunnel_801855AC, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[34].state },
    { D_shelter_r36_8017FA40, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[35].state },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b3_dumping_hole_8018EC3C, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[38].state },
    { D_shelter_b3_garbage_incinerator_8018FA58, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[39].state },
    { D_shelter_b3_incinerator_control_room_80182610, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[40].state },
    { D_shelter_b3_elevator_hall_8018477C, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[41].state },
    { D_shelter_b4_lower_sewer_80183D48, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[42].state },
    { D_shelter_b4_upper_sewer_80188B9C, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[43].state },
    { D_shelter_b4_reservoir_80187350, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[44].state },
    { D_shelter_b4_water_supply_80184CA4, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[45].state },
    { D_shelter_r47_80187CB8, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[46].state },
    { D_shelter_r48_8018BC10, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[47].state },
    { D_shelter_r49_8017DD74, &gGameFlagMineShelterBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[5].state },
};
/// End marker following the stage 4 room table.
u32        D_8010D0A4[2]  = { 0xFFFF, 0 };
AreaRecord D_8010D0AC[34] = {
    { NULL, NULL },
    { D_shelter_1f_parking_garage_801818D8, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[0].state },
    { D_shelter_1f_vehicular_airlock_80182A04, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[1].state },
    { D_shelter_1f_bulwark_80180DA8, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[2].state },
    { D_shelter_1f_heliport_80182BF4, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[3].state },
    { D_shelter_1f_airlock_8017F758, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[4].state },
    { NULL, NULL },
    { D_neo_ark_observatory_8018786C, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[6].state },
    { D_neo_ark_eve_access_tunnel_801806B8, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[7].state },
    { NULL, NULL },
    { D_neo_ark_north_promenade_8018305C, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[9].state },
    { D_neo_ark_forest_zone_80182968, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[10].state },
    { D_neo_ark_submarine_tunnel_80187470, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[11].state },
    { D_neo_ark_pavilion_801876C4, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[12].state },
    { D_neo_ark_island_80183F48, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[13].state },
    { D_neo_ark_garden_80182B54, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[14].state },
    { D_neo_ark_power_plant_2_80182DE0, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[15].state },
    { D_neo_ark_power_plant_1_80181AF8, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[16].state },
    { D_neo_ark_savanna_zone_80180864, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[17].state },
    { D_neo_ark_south_promenade_801808E4, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[18].state },
    { NULL, NULL },
    { D_neo_ark_shrine_801866C8, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[20].state },
    { D_shelter_b6_nursery_801874A4, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[21].state },
    { D_shelter_b6_growth_room_80180338, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[21].state },
    { D_shelter_b6_corridor_80180304, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[23].state },
    { D_shelter_b6_training_room_801859DC, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[24].state },
    { D_neo_ark_r26_8017E994, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[25].state },
    { D_neo_ark_bridge_80184A50, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[26].state },
    { D_shelter_1f_tent_80184230, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[27].state },
    { D_neo_ark_woodland_path_8018471C, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[28].state },
    { D_neo_ark_submarine_gallery_80185860, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[29].state },
    { D_neo_ark_r31_8017DBB8, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[30].state },
    { D_neo_ark_pyramid_80181728, &gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE].areas[31].state },
    { NULL, NULL },
};
/// End marker following the stage 5 room table.
u32 D_8010D1BC[2] = { 0xFFFF, 0 };

const TaskFuncTable3 Gp_SessionStates;
const TaskFuncTable8 Gp_LoadStateFns;
const TaskFuncTable3 Gp_RoomObjStates;

static const TaskFuncTable6 Gp_LoadWaitFns = { {
    loadingBeginViewLoadTask,
    loadingEnqueueViewResourcesTask,
    loadingUploadViewImageTask,
    _loadingWaitForMovieReadyTask,
    _loadingWaitForViewCdIdleTask,
    _loadingWaitForSceneImageTask,
} };

static const _LoadingConfigFileHundreds Gp_ConfigCdTable = { { 4, 3, 2, 5, 6 } };

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
static __inline__ void _loadingRestoreViewGraphics(s32 keepGraphics)
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
    _LoadingFileKey  fileKey;
    _LoadingFileArgs loadArgs;
    u16              weaponIndex;

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
    GameLocationKey* location;
    _LoadingFileKey  fileKey;
    _LoadingFileArgs loadArgs;

    location = &gGameSession->location.loc;
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
    for (resourceSlotIndex = 0; resourceSlotIndex < ARRAY_SIZE(D_8006C338); resourceSlotIndex++) {
        if (D_8006C338[resourceSlotIndex].kind == FILE_SYSTEM_RESOURCE_IMAGE && mappedViewIndex - 1 == resourceSlotIndex) {
            // Keep retrying the retained chunk, including after timer failure.
            while (fsUploadImageChunk(D_8006C338[resourceSlotIndex].data, LOADING_VIEW_IMAGE_IGNORE_GPU_TIME_LIMIT) != FILE_SYSTEM_IMAGE_UPLOAD_COMPLETE) {
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

    stateHandlers = Gp_LoadWaitFns;
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
static __inline__ void _loadingResetViewDrawing(void)
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
    viewGateTask->spawnArg1.value = liveSave->state.location.loc.view;
    _loadingResetViewDrawing();
    gGameSession->location.loc.view = liveSave->state.location.loc.view;
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

    viewGateTask                                               = gameGetTaskSlot(GAME_TASK_SLOT_VIEW_GATE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = logicalViewIndex;
    gGameSession->location.loc.view                            = logicalViewIndex;
    viewGateTask->spawnArg1.value                              = (u8)logicalViewIndex;
    padStartInputBlock(LOADING_VIEW_INPUT_PORT);
    viewQueueCurrentCamera(VIEW_PACKET_LIST_DEFAULT);
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
    taskSpawn(LOADING_VIEW_TASK_BANK, LOADING_VIEW_TASK_TYPE, LOADING_VIEW_RESUME_GAME_LOOP, 0);
}

void viewCommitIndexTask(Task* task)
{
    u8 viewIndex;

    viewIndex                                                  = (u8)task->spawnArg1.value;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = viewIndex;
    gGameSession->location.loc.view                            = viewIndex;
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
    u8               mappedViewIndex;
    u8               resourceSlotIndex;
    GameSession*     session;
    _LoadingFileArgs loadArgs;
    _LoadingFileKey  fileKey;

    // Restore the retained image before the queued file reload can replace resources.
    mappedViewIndex = viewGetMappedIndex();
    for (resourceSlotIndex = 0; resourceSlotIndex < ARRAY_SIZE(D_8006C338); resourceSlotIndex++) {
        if (D_8006C338[resourceSlotIndex].kind == FILE_SYSTEM_RESOURCE_IMAGE) {
            if (mappedViewIndex - 1 == resourceSlotIndex) {
                while (fsUploadImageChunk(D_8006C338[resourceSlotIndex].data, 1) != FILE_SYSTEM_IMAGE_UPLOAD_COMPLETE) {
                }
                break;
            }
        }
    }
    session                 = gGameSession;
    fileKey.stage           = session->location.loc.stage;
    fileKey.fileGroup       = session->location.loc.area;
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
    _LoadingFileKey            fileKey;
    _LoadingFileArgs           loadArgs;
    _LoadingConfigFileHundreds fileHundredsByVariant;

    fileHundredsByVariant = Gp_ConfigCdTable;
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
    _LoadingFileKey  fileKey;
    _LoadingFileArgs loadArgs;
    u8               weaponIndex;

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
    _LoadingFileKey  fileKey;
    _LoadingFileArgs loadArgs;

    // Mount the new CDF before loading its stage-specific map and room names.
    cdCmdEnqueue(CD_COMMAND_MOUNT_STAGE, &gGameSession->location.loc, NULL);
    fileKey.stage             = LOADING_GLOBAL_CDF;
    fileKey.fileGroup         = LOADING_STAGE_MAP_FILE_GROUP;
    fileKey.fileIndex         = gGameSession->location.loc.stage;
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
    _LoadingFileArgs loadArgs;
    _LoadingFileKey* fileKey;

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

const TaskFuncTable3 Gp_SessionStates = { {
    gameFlowPrepareSessionReloadTask,
    gameFlowHoldSessionDisplayTask,
    gameFlowRebuildSessionTask,
} };
const TaskFuncTable8 Gp_LoadStateFns  = { {
    loadingPrepareCharacterResourcesTask,
    loadingEnqueueStageResourcesTask,
    loadingInitializeAreaMemoryAndAudioTask,
    loadingEnqueueAreaAndCompanionResourcesTask,
    loadingPrepareAreaStateTask,
    loadingPollAreaResourcesTask,
    loadingHoldFadeAndReleaseBootImageTask,
    loadingFinishSessionLoadTask,
} };
const TaskFuncTable3 Gp_RoomObjStates = { {
    loadingInitRoomResourcesTask,
    loadingUpdateRoomResourcesTask,
    taskKill,
} };
