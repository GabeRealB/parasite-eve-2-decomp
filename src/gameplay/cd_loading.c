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
#include "main/loadui.h"
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

static void Gp_LoadWaitCdBusy(Task* task);

static void Gp_LoadWaitIdle(Task* task);

static void Gp_LoadWaitDone(Task* task);

static void Gp_ReloadFromSave(void);

static void Gp_ReloadAtLoc(s32 arg0);

AreaRecord* Gp_AreaTables[6] = { NULL, D_8010CBE4, D_8010CC94, D_8010CDD4, D_8010CF14, D_8010D0AC };
AreaRecord  D_8010CBE4[21]   = {
    { NULL, NULL },
    { D_acropolis_square_80185E50, &GameFlag_AcropolisBanks[0].areas[0].state },
    { D_acropolis_east_elevator_hall_80186C50, &GameFlag_AcropolisBanks[0].areas[1].state },
    { D_acropolis_patio_80184A90, &GameFlag_AcropolisBanks[0].areas[2].state },
    { D_acropolis_cafeteria_80189DCC, &GameFlag_AcropolisBanks[0].areas[3].state },
    { D_acropolis_plaza_80199390, &GameFlag_AcropolisBanks[0].areas[4].state },
    { D_acropolis_security_room_80184088, &GameFlag_AcropolisBanks[0].areas[5].state },
    { D_acropolis_hallway_8017EA3C, &GameFlag_AcropolisBanks[0].areas[6].state },
    { D_acropolis_fountain_8017FC9C, &GameFlag_AcropolisBanks[0].areas[7].state },
    { D_acropolis_forked_road_801831C4, &GameFlag_AcropolisBanks[0].areas[8].state },
    { D_acropolis_observatory_80181264, &GameFlag_AcropolisBanks[0].areas[9].state },
    { D_acropolis_promenade_80183020, &GameFlag_AcropolisBanks[0].areas[10].state },
    { D_acropolis_sanctuary_8018402C, &GameFlag_AcropolisBanks[0].areas[11].state },
    { D_acropolis_roof_garden_80185934, &GameFlag_AcropolisBanks[0].areas[12].state },
    { D_acropolis_bridge_8019004C, &GameFlag_AcropolisBanks[0].areas[13].state },
    { D_acropolis_fire_escape_8018294C, &GameFlag_AcropolisBanks[0].areas[14].state },
    { D_acropolis_helicopter_landing_pad_801861E8, &GameFlag_AcropolisBanks[0].areas[15].state },
    { NULL, NULL },
    { D_mist_r18_80186BFC, &GameFlag_AcropolisBanks[0].areas[16].state },
    { D_mist_parking_801951B4, &GameFlag_AcropolisBanks[0].areas[17].state },
    { D_mist_shooting_gallery_8018DF74, &GameFlag_DryfieldBanks[0].areas[29].state },
};
/// End marker following the stage 1 room table.
u32        D_8010CC8C[2]  = { 0xFFFF, 0 };
AreaRecord D_8010CC94[39] = {
    { NULL, NULL },
    { D_dryfield_gas_station_80184A38, &GameFlag_DryfieldBanks[0].areas[0].state },
    { D_dryfield_main_street_80184F20, &GameFlag_DryfieldBanks[0].areas[1].state },
    { D_dryfield_general_store_80185654, &GameFlag_DryfieldBanks[0].areas[2].state },
    { NULL, NULL },
    { D_dryfield_back_street_80180A50, &GameFlag_DryfieldBanks[0].areas[3].state },
    { D_dryfield_souvenir_shop_8017F598, &GameFlag_DryfieldBanks[0].areas[4].state },
    { D_dryfield_warehouse_80182100, &GameFlag_DryfieldBanks[0].areas[5].state },
    { D_dryfield_r08_80180B88, &GameFlag_DryfieldBanks[0].areas[33].state },
    { D_dryfield_dilapidated_house_80189938, &GameFlag_DryfieldBanks[0].areas[6].state },
    { NULL, NULL },
    { D_dryfield_motel_room_1_801814DC, &GameFlag_DryfieldBanks[0].areas[7].state },
    { D_dryfield_motel_room_2_80180410, &GameFlag_DryfieldBanks[0].areas[8].state },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_parking_lot_8017FA74, &GameFlag_DryfieldBanks[0].areas[11].state },
    { D_dryfield_toilet_80182918, &GameFlag_DryfieldBanks[0].areas[12].state },
    { NULL, NULL },
    { D_dryfield_saloon_g_r_80181B1C, &GameFlag_DryfieldBanks[0].areas[13].state },
    { D_dryfield_g_r_kitchen_8017F4B8, &GameFlag_DryfieldBanks[0].areas[14].state },
    { D_dryfield_water_tower_8018757C, &GameFlag_DryfieldBanks[0].areas[15].state },
    { D_dryfield_water_tank_80188BF0, &GameFlag_DryfieldBanks[0].areas[16].state },
    { D_dryfield_breezeway_801842F8, &GameFlag_DryfieldBanks[0].areas[17].state },
    { NULL, NULL },
    { D_dryfield_garage_801800E0, &GameFlag_DryfieldBanks[0].areas[18].state },
    { D_dryfield_driveway_8017EDD4, &GameFlag_DryfieldBanks[0].areas[19].state },
    { D_dryfield_junk_yard_8017F558, &GameFlag_DryfieldBanks[0].areas[20].state },
    { D_dryfield_trailer_coach_801876F0, &GameFlag_DryfieldBanks[0].areas[21].state },
    { NULL, NULL },
    { D_dryfield_motel_balcony_80186220, &GameFlag_DryfieldBanks[0].areas[23].state },
    { D_dryfield_motel_room_6_80186764, &GameFlag_DryfieldBanks[0].areas[24].state },
    { NULL, NULL },
    { D_dryfield_water_hole_801827BC, &GameFlag_DryfieldBanks[0].areas[26].state },
    { NULL, NULL },
    { D_dryfield_cellar_80180ACC, &GameFlag_DryfieldBanks[0].areas[27].state },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_underpass_8017F868, &GameFlag_DryfieldBanks[0].areas[28].state },
};
/// End marker following the stage 2 room table.
u32        D_8010CDCC[2]  = { 0xFFFF, 0 };
AreaRecord D_8010CDD4[39] = {
    { NULL, NULL },
    { D_dryfield_night_gas_station_80190624, &GameFlag_DryfieldBanks[0].areas[0].state },
    { D_dryfield_night_main_street_80188A08, &GameFlag_DryfieldBanks[0].areas[1].state },
    { D_dryfield_night_general_store_8018572C, &GameFlag_DryfieldBanks[0].areas[2].state },
    { NULL, NULL },
    { D_dryfield_night_back_street_80181518, &GameFlag_DryfieldBanks[0].areas[3].state },
    { D_dryfield_night_souvenir_shop_8017F62C, &GameFlag_DryfieldBanks[0].areas[4].state },
    { D_dryfield_night_warehouse_8017FB98, &GameFlag_DryfieldBanks[0].areas[5].state },
    { D_dryfield_night_r08_801818D8, &GameFlag_DryfieldBanks[0].areas[33].state },
    { D_dryfield_night_dilapidated_house_80189FA4, &GameFlag_DryfieldBanks[0].areas[6].state },
    { NULL, NULL },
    { D_dryfield_night_motel_room_1_8018075C, &GameFlag_DryfieldBanks[0].areas[7].state },
    { D_dryfield_night_motel_room_2_801809A0, &GameFlag_DryfieldBanks[0].areas[8].state },
    { D_dryfield_night_motel_room_3_80180D24, &GameFlag_DryfieldBanks[0].areas[9].state },
    { D_dryfield_night_motel_room_4_801802FC, &GameFlag_DryfieldBanks[0].areas[10].state },
    { D_dryfield_night_parking_lot_80181438, &GameFlag_DryfieldBanks[0].areas[11].state },
    { D_dryfield_night_toilet_8017F354, &GameFlag_DryfieldBanks[0].areas[12].state },
    { D_dryfield_night_motel_lobby_801843C4, &GameFlag_DryfieldBanks[0].areas[34].state },
    { D_dryfield_night_saloon_g_r_80188EE4, &GameFlag_DryfieldBanks[0].areas[13].state },
    { D_dryfield_night_g_r_kitchen_8017EB88, &GameFlag_DryfieldBanks[0].areas[14].state },
    { D_dryfield_night_water_tower_80182B5C, &GameFlag_DryfieldBanks[0].areas[15].state },
    { D_dryfield_night_water_tank_80180764, &GameFlag_DryfieldBanks[0].areas[16].state },
    { D_dryfield_night_breezeway_801803E4, &GameFlag_DryfieldBanks[0].areas[17].state },
    { D_dryfield_night_factory_8018A70C, &GameFlag_DryfieldBanks[0].areas[35].state },
    { D_dryfield_night_garage_801874BC, &GameFlag_DryfieldBanks[0].areas[18].state },
    { D_dryfield_night_driveway_80181F4C, &GameFlag_DryfieldBanks[0].areas[19].state },
    { D_dryfield_night_junk_yard_801843F0, &GameFlag_DryfieldBanks[0].areas[20].state },
    { D_dryfield_night_trailer_coach_8018C15C, &GameFlag_DryfieldBanks[0].areas[21].state },
    { D_dryfield_night_motel_room_5_80181190, &GameFlag_DryfieldBanks[0].areas[22].state },
    { D_dryfield_night_motel_balcony_8018EA94, &GameFlag_DryfieldBanks[0].areas[23].state },
    { D_dryfield_night_motel_room_6_801861B4, &GameFlag_DryfieldBanks[0].areas[24].state },
    { D_dryfield_night_motel_loft_80180888, &GameFlag_DryfieldBanks[0].areas[25].state },
    { D_dryfield_night_water_hole_80183418, &GameFlag_DryfieldBanks[0].areas[26].state },
    { NULL, NULL },
    { D_dryfield_night_cellar_80180780, &GameFlag_DryfieldBanks[0].areas[27].state },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_night_underpass_801802DC, &GameFlag_DryfieldBanks[0].areas[28].state },
};
/// End marker following the stage 3 room table.
u32        D_8010CF0C[2]  = { 0xFFFF, 0 };
AreaRecord D_8010CF14[50] = {
    { NULL, NULL },
    { D_mine_mesa_801898F4, &GameFlag_ShelterBanks[0].areas[0].state },
    { D_mine_cavern_8018E238, &GameFlag_ShelterBanks[0].areas[1].state },
    { D_mine_tunnel_entrance_8017F32C, &GameFlag_ShelterBanks[0].areas[2].state },
    { D_mine_tunnel_801801CC, &GameFlag_ShelterBanks[0].areas[3].state },
    { D_mine_gorge_80183544, &GameFlag_ShelterBanks[0].areas[4].state },
    { D_mine_refuge_80182A00, &GameFlag_ShelterBanks[0].areas[5].state },
    { D_mine_forked_tunnel_80185504, &GameFlag_ShelterBanks[0].areas[6].state },
    { D_mine_secret_passage_80183340, &GameFlag_ShelterBanks[0].areas[7].state },
    { D_shelter_b1_elevator_hall_80184940, &GameFlag_ShelterBanks[0].areas[8].state },
    { D_shelter_b1_south_maintenance_walkway_80183598, &GameFlag_ShelterBanks[0].areas[9].state },
    { D_shelter_b1_storeroom_80186C9C, &GameFlag_ShelterBanks[0].areas[10].state },
    { D_shelter_b1_north_maintenance_walkway_80185A38, &GameFlag_ShelterBanks[0].areas[11].state },
    { D_shelter_b1_armory_801854E0, &GameFlag_ShelterBanks[0].areas[12].state },
    { D_shelter_b1_sleeping_quarters_80183FDC, &GameFlag_ShelterBanks[0].areas[13].state },
    { D_shelter_b1_main_corridor_80185C30, &GameFlag_ShelterBanks[0].areas[14].state },
    { D_shelter_b1_sterilization_room_8018C14C, &GameFlag_ShelterBanks[0].areas[15].state },
    { D_shelter_b1_pod_access_tunnel_80184C00, &GameFlag_ShelterBanks[0].areas[16].state },
    { D_shelter_b1_control_room_80183A98, &GameFlag_ShelterBanks[0].areas[17].state },
    { D_shelter_b1_access_tunnel_8017FE50, &GameFlag_ShelterBanks[0].areas[18].state },
    { D_shelter_b1_underground_parking_8018B5C4, &GameFlag_ShelterBanks[0].areas[19].state },
    { D_shelter_b1_golem_freezer_1_8017F17C, &GameFlag_ShelterBanks[0].areas[20].state },
    { D_shelter_b2_pod_bottom_80187678, &GameFlag_ShelterBanks[0].areas[21].state },
    { D_shelter_b1_pod_service_gantry_8017FBF8, &GameFlag_ShelterBanks[0].areas[22].state },
    { D_shelter_b1_transfer_tunnel_801830B8, &GameFlag_ShelterBanks[0].areas[23].state },
    { D_shelter_b1_control_room_access_tunnel_801825AC, &GameFlag_ShelterBanks[0].areas[24].state },
    { D_shelter_b2_elevator_8017E964, &GameFlag_ShelterBanks[0].areas[25].state },
    { D_shelter_b2_elevator_hall_80184C7C, &GameFlag_ShelterBanks[0].areas[26].state },
    { D_shelter_b2_south_maintenance_walkway_801837AC, &GameFlag_ShelterBanks[0].areas[27].state },
    { D_shelter_b2_operating_room_80184124, &GameFlag_ShelterBanks[0].areas[28].state },
    { D_shelter_b2_north_maintenance_walkway_80186258, &GameFlag_ShelterBanks[0].areas[29].state },
    { D_shelter_b2_laboratory_80186360, &GameFlag_ShelterBanks[0].areas[30].state },
    { D_shelter_b2_breeding_room_80183EEC, &GameFlag_ShelterBanks[0].areas[31].state },
    { D_shelter_b2_main_corridor_8018933C, &GameFlag_ShelterBanks[0].areas[32].state },
    { D_shelter_b2_septic_tank_80186F40, &GameFlag_ShelterBanks[0].areas[33].state },
    { D_shelter_b2_pod_access_tunnel_801855AC, &GameFlag_ShelterBanks[0].areas[34].state },
    { D_shelter_r36_8017FA40, &GameFlag_ShelterBanks[0].areas[35].state },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b3_dumping_hole_8018EC3C, &GameFlag_ShelterBanks[0].areas[38].state },
    { D_shelter_b3_garbage_incinerator_8018FA58, &GameFlag_ShelterBanks[0].areas[39].state },
    { D_shelter_b3_incinerator_control_room_80182610, &GameFlag_ShelterBanks[0].areas[40].state },
    { D_shelter_b3_elevator_hall_8018477C, &GameFlag_ShelterBanks[0].areas[41].state },
    { D_shelter_b4_lower_sewer_80183D48, &GameFlag_ShelterBanks[0].areas[42].state },
    { D_shelter_b4_upper_sewer_80188B9C, &GameFlag_ShelterBanks[0].areas[43].state },
    { D_shelter_b4_reservoir_80187350, &GameFlag_ShelterBanks[0].areas[44].state },
    { D_shelter_b4_water_supply_80184CA4, &GameFlag_ShelterBanks[0].areas[45].state },
    { D_shelter_r47_80187CB8, &GameFlag_ShelterBanks[0].areas[46].state },
    { D_shelter_r48_8018BC10, &GameFlag_ShelterBanks[0].areas[47].state },
    { D_shelter_r49_8017DD74, &GameFlag_ShelterBanks[0].areas[5].state },
};
/// End marker following the stage 4 room table.
u32        D_8010D0A4[2]  = { 0xFFFF, 0 };
AreaRecord D_8010D0AC[34] = {
    { NULL, NULL },
    { D_shelter_1f_parking_garage_801818D8, &GameFlag_NeoArkBanks[0].areas[0].state },
    { D_shelter_1f_vehicular_airlock_80182A04, &GameFlag_NeoArkBanks[0].areas[1].state },
    { D_shelter_1f_bulwark_80180DA8, &GameFlag_NeoArkBanks[0].areas[2].state },
    { D_shelter_1f_heliport_80182BF4, &GameFlag_NeoArkBanks[0].areas[3].state },
    { D_shelter_1f_airlock_8017F758, &GameFlag_NeoArkBanks[0].areas[4].state },
    { NULL, NULL },
    { D_neo_ark_observatory_8018786C, &GameFlag_NeoArkBanks[0].areas[6].state },
    { D_neo_ark_eve_access_tunnel_801806B8, &GameFlag_NeoArkBanks[0].areas[7].state },
    { NULL, NULL },
    { D_neo_ark_north_promenade_8018305C, &GameFlag_NeoArkBanks[0].areas[9].state },
    { D_neo_ark_forest_zone_80182968, &GameFlag_NeoArkBanks[0].areas[10].state },
    { D_neo_ark_submarine_tunnel_80187470, &GameFlag_NeoArkBanks[0].areas[11].state },
    { D_neo_ark_pavilion_801876C4, &GameFlag_NeoArkBanks[0].areas[12].state },
    { D_neo_ark_island_80183F48, &GameFlag_NeoArkBanks[0].areas[13].state },
    { D_neo_ark_garden_80182B54, &GameFlag_NeoArkBanks[0].areas[14].state },
    { D_neo_ark_power_plant_2_80182DE0, &GameFlag_NeoArkBanks[0].areas[15].state },
    { D_neo_ark_power_plant_1_80181AF8, &GameFlag_NeoArkBanks[0].areas[16].state },
    { D_neo_ark_savanna_zone_80180864, &GameFlag_NeoArkBanks[0].areas[17].state },
    { D_neo_ark_south_promenade_801808E4, &GameFlag_NeoArkBanks[0].areas[18].state },
    { NULL, NULL },
    { D_neo_ark_shrine_801866C8, &GameFlag_NeoArkBanks[0].areas[20].state },
    { D_shelter_b6_nursery_801874A4, &GameFlag_NeoArkBanks[0].areas[21].state },
    { D_shelter_b6_growth_room_80180338, &GameFlag_NeoArkBanks[0].areas[21].state },
    { D_shelter_b6_corridor_80180304, &GameFlag_NeoArkBanks[0].areas[23].state },
    { D_shelter_b6_training_room_801859DC, &GameFlag_NeoArkBanks[0].areas[24].state },
    { D_neo_ark_r26_8017E994, &GameFlag_NeoArkBanks[0].areas[25].state },
    { D_neo_ark_bridge_80184A50, &GameFlag_NeoArkBanks[0].areas[26].state },
    { D_shelter_1f_tent_80184230, &GameFlag_NeoArkBanks[0].areas[27].state },
    { D_neo_ark_woodland_path_8018471C, &GameFlag_NeoArkBanks[0].areas[28].state },
    { D_neo_ark_submarine_gallery_80185860, &GameFlag_NeoArkBanks[0].areas[29].state },
    { D_neo_ark_r31_8017DBB8, &GameFlag_NeoArkBanks[0].areas[30].state },
    { D_neo_ark_pyramid_80181728, &GameFlag_NeoArkBanks[0].areas[31].state },
    { NULL, NULL },
};
/// End marker following the stage 5 room table.
u32 D_8010D1BC[2] = { 0xFFFF, 0 };

const TaskFuncTable3 Gp_SessionStates;
const TaskFuncTable8 Gp_LoadStateFns;
const TaskFuncTable3 Gp_RoomObjStates;

static const TaskFuncTable6 Gp_LoadWaitFns = { {
    Gp_ViewBeginLoad,
    loadingEnqueueViewResourcesTask,
    Gp_ViewLoadImage,
    Gp_LoadWaitCdBusy,
    Gp_LoadWaitIdle,
    Gp_LoadWaitDone,
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
    D_800626E8 = LOADING_VIEW_SEEK_PENDING;
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

static void Gp_LoadWaitCdBusy(Task* task)
{
    if (gCdCmdQueue.movieReady != 0) {
        task->killCountdown++;
    }
    if (task->killCountdown >= 3) {
        task->state = -1;
        Gp_FinishLoadWait(task);
    }
}

static void Gp_LoadWaitIdle(Task* task)
{
    if (cdCmdIsIdle() & 0xFFFF) {
        task->state = -2;
        Gp_FinishLoadWait(task);
    }
}

static void Gp_LoadWaitDone(Task* task)
{
    if (gCdCmdQueue.imageLoadStatus == CD_COMMAND_IMAGE_COMPLETE) {
        task->state = -1;
        Gp_FinishLoadWait(task);
    }
}

void Gp_LoadViewImages(void)
{
    u8 view;
    u8 i;

    view = viewGetMappedIndex();
    for (i = 0; i < ARRAY_SIZE(D_8006C338); i++) {
        if (D_8006C338[i].kind == FILE_SYSTEM_RESOURCE_IMAGE) {
            if (view - 1 == i) {
                while (fsUploadImageChunk(D_8006C338[i].data, 1)) {
                }
                break;
            }
        }
    }
}

void Gp_FinishLoadWait(Task* task)
{
    padClearInputBlock(0);
    if (task->spawnArg1.value == 0) {
        Stage_RequestSpecialFlag(1);
        gGameSession->viewDirty = 0;
        taskKill(task);
        displayResumeGameLoop();
    } else {
        if (task->spawnArg1.value == 1) {
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
        }
        gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_TRANSITION_STRIPS;
        taskSpawn(0, 0x17, 0, 0);
        gGameSession->viewReady = 1;
        taskKill(task);
    }
}

void Gp_LoadWaitDispatch(Task* task)
{
    TaskFuncTable6 sp;

    sp = Gp_LoadWaitFns;
    padStartInputBlock(0);
    if (task->state < 0) {
        Gp_FinishLoadWait(task);
    } else {
        sp.funcs[task->state](task);
    }
}

static void Gp_ReloadFromSave(void)
{
    Task*       slot;
    McSaveData* save;

    slot                  = gameGetTaskSlot(GAME_TASK_SLOT_VIEW_GATE);
    save                  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    slot->spawnArg1.value = save->state.location.loc.view;
    ResetGraph(1);
    gpuClearFrameOrderingTable(0);
    gpuClearFrameOrderingTable(1);
    gGameSession->location.loc.view = save->state.location.loc.view;
    padStartInputBlock(0);
    viewQueueCurrentCamera(VIEW_PACKET_LIST_NONE);
    gGameSession->viewReady = 0;
    taskSpawn(0, 0x1E, 1, 0);
}

static void Gp_ReloadAtLoc(s32 arg0)
{
    Task* slot;

    slot                                                       = gameGetTaskSlot(GAME_TASK_SLOT_VIEW_GATE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = arg0;
    gGameSession->location.loc.view                            = arg0;
    slot->spawnArg1.value                                      = (u8)arg0;
    padStartInputBlock(0);
    viewQueueCurrentCamera(VIEW_PACKET_LIST_DEFAULT);
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
    taskSpawn(0, 0x1E, 0, 0);
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
    D_800626E8 = LOADING_VIEW_SEEK_PENDING;
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
    Gp_BeginSessionTask,
} };
const TaskFuncTable8 Gp_LoadStateFns  = { {
    Gp_LoadWaitBoot,
    loadingEnqueueStageResourcesTask,
    Gp_LoadState2,
    loadingEnqueueAreaAndCompanionResourcesTask,
    loadingPrepareAreaStateTask,
    Gp_LoadWaitAreaCd,
    loadingHoldFadeAndReleaseBootImageTask,
    Gp_LoadFinishTask,
} };
const TaskFuncTable3 Gp_RoomObjStates = { {
    Gp_LinkRoomObjectsSpawn,
    loadingUpdateRoomResourcesTask,
    taskKill,
} };
