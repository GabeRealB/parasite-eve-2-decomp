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

/// Maps `gPlayerStatus.weapon` / `gPlayerStatus.weaponSlotItem` (and the 0x1B attach id) to a
/// CdCmd 0x21 payload. No-op when `gPlayerStatus.weapon` is 0 or the mapped byte is 0.
static void Gp_EnqueueWeaponCd(void);

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
    Gp_EnqueueViewCd,
    Gp_ViewLoadImage,
    Gp_LoadWaitCdBusy,
    Gp_LoadWaitIdle,
    Gp_LoadWaitDone,
} };

static const _LoadingConfigFileHundreds Gp_ConfigCdTable = { { 4, 3, 2, 5, 6 } };

/// Restores discarded model buffers before allocating the current view's sprite packets.
static __inline__ void _loadingRestoreViewGraphics(s32 keepGraphics)
{
    if (keepGraphics == 0) {
        gpuResetAndInvalidateModelBuffers();
        tmdResetAuxHeapAndRestoreBuffers();
    }
    spriteAllocateViewCachedPackets();
}

/// Maps `gPlayerStatus.weapon` / `gPlayerStatus.weaponSlotItem` (and the 0x1B attach id) to a
/// CdCmd 0x21 payload. No-op when `gPlayerStatus.weapon` is 0 or the mapped byte is 0.
static void Gp_EnqueueWeaponCd(void)
{
    u8  param1[8];
    u8  param2[8];
    u16 item;
    s32 val;
    s32 attach;
    s32 flag;

    item = gPlayerStatus.weapon;
    if (item == 0) {
        return;
    }

    param1[0] = 0;
    switch (item) {
        case 0xB:
            param1[0] = 1;
            if (gPlayerStatus.weaponSlotItem == 0xB) {
                param1[0] = 2;
            }
            if (gPlayerStatus.weaponSlotItem == 0xC) {
                param1[0] = 3;
            }
            break;
        case 0xC:
            param1[0] = 4;
            if (gPlayerStatus.weaponSlotItem == 0xB) {
                param1[0] = 5;
            }
            if (gPlayerStatus.weaponSlotItem == 0xC) {
                param1[0] = 6;
            }
            break;
        case 0xD:
            param1[0] = 7;
            if (gPlayerStatus.weaponSlotItem == 0xE) {
                param1[0] = 8;
            }
            if (gPlayerStatus.weaponSlotItem == 0xF) {
                param1[0] = 9;
            }
            break;
        case 0xE:
            param1[0] = 0xA;
            if (gPlayerStatus.weaponSlotItem == 0xE) {
                param1[0] = 0xB;
            }
            if (gPlayerStatus.weaponSlotItem == 0xF) {
                param1[0] = 0xC;
            }
            break;
        case 0xF:
            param1[0] = 0xD;
            val       = gPlayerStatus.weaponSlotItem;
            if (val == 0xE) {
                param1[0] = val;
            }
            if (val == 0xF) {
                param1[0] = val;
            }
            break;
        case 0x17:
            param1[0] = 0x13;
            if (gPlayerStatus.weaponSlotItem == 0xE) {
                param1[0] = 0x14;
            }
            if (gPlayerStatus.weaponSlotItem == 0xF) {
                param1[0] = 0x15;
            }
            break;
        case 0x1B: {
            EquipmentWeaponLoad* slot;

            param1[0] = 0x10;
            slot      = Gp_GetItemSlot(item + 0x7F);
            if (slot->secondaryItemId != INVENTORY_ITEM_NONE && slot->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
                attach = slot->secondaryItemId - 0x9F;
                if (attach == 0xB) {
                    param1[0] = 0x11;
                }
                if (attach == 0xC) {
                    param1[0] = 0x12;
                }
            }
            break;
        }
    }

    if (param1[0] == 0) {
        return;
    }

    flag      = 1;
    param1[3] = 0;
    param1[2] = flag;
    param2[0] = 0xA;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
    D_800626E8 = flag;
}

void Gp_EnqueueViewCd(Task* task)
{
    GameLocationKey* sess;
    u8               param1[8];
    u8               param2[8];

    sess = &gGameSession->location.loc;
    if (CdCmd_IsIdle() & 0xFFFF) {
        param1[3] = sess->stage;
        param1[2] = sess->area;
        param1[0] = viewGetMappedIndex();
        param2[0] = 1;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
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
    if (CdCmd_IsIdle() & 0xFFFF) {
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
                while (Fs_LoadImageChunk(D_8006C338[i].data, 1)) {
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
        Task_Spawn(0, 0x17, 0, 0);
        gGameSession->viewReady = 1;
        taskKill(task);
    }
}

void Gp_LoadWaitDispatch(Task* task)
{
    TaskFuncTable6 sp;

    sp = Gp_LoadWaitFns;
    Pad_SetCooldown(0);
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
    Pad_SetCooldown(0);
    Gp_SpawnCurView(2);
    gGameSession->viewReady = 0;
    Task_Spawn(0, 0x1E, 1, 0);
}

static void Gp_ReloadAtLoc(s32 arg0)
{
    Task* slot;

    slot                                                       = gameGetTaskSlot(GAME_TASK_SLOT_VIEW_GATE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = arg0;
    gGameSession->location.loc.view                            = arg0;
    slot->spawnArg1.value                                      = (u8)arg0;
    Pad_SetCooldown(0);
    Gp_SpawnCurView(1);
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
    Task_Spawn(0, 0x1E, 0, 0);
}

void viewCommitIndexTask(Task* task)
{
    u8 viewIndex;

    viewIndex                                                  = (u8)task->spawnArg1.value;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = viewIndex;
    gGameSession->location.loc.view                            = viewIndex;
    taskKill(task);
}

void func_800A99B4(void)
{
    Display_SpawnWithOtSmall(0, 0x26, 0, 0);
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

void Gp_LoadViewAndCd(u8 arg0)
{
    u8           view;
    u8           i;
    GameSession* session;
    u8           param2[8];
    u8           param1[8];

    view = viewGetMappedIndex();
    for (i = 0; i < ARRAY_SIZE(D_8006C338); i++) {
        if (D_8006C338[i].kind == FILE_SYSTEM_RESOURCE_IMAGE) {
            if (view - 1 == i) {
                while (Fs_LoadImageChunk(D_8006C338[i].data, 1)) {
                }
                break;
            }
        }
    }
    session   = gGameSession;
    param1[3] = session->location.loc.stage;
    param1[2] = session->location.loc.area;
    param1[0] = viewGetMappedIndex();
    param2[0] = 1;
    if (arg0 != 0) {
        param2[1] = 4;
    } else {
        param2[1] = 0;
    }
    param2[3] = 0;
    param2[2] = 0;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
}

void Gp_EnqueueConfigCd(s32 arg0)
{
    u8                         param1[8];
    u8                         param2[8];
    _LoadingConfigFileHundreds table;

    table = Gp_ConfigCdTable;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId != 0) {
        param1[3] = 0;
        param1[2] = 1;
        param1[0] = 0;
        param2[0] = table.fileIdHundreds[gPlayerStatus.resourceVariant - 1];
        if ((u8)arg0 == 0) {
            param2[1] = 0;
        } else {
            param2[1] = 5;
        }
        param2[2] = 6;
        param2[3] = 0;
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
    }
}

void Gp_EnqueueHeldWeaponCd(void)
{
    u8  param1[8];
    u8  param2[8];
    u8  val;
    s32 flag;

    val = gPlayerStatus.weapon;
    if (val == 0) {
        val = 1;
    }
    flag      = 1;
    param1[0] = val;
    param1[3] = 0;
    param1[2] = flag;
    param2[0] = 3;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
    D_800626E8 = flag;
    Gp_EnqueueWeaponCd();
}

void Gp_EnqueueStageCd(void)
{
    u8 param1[8];
    u8 param2[8];

    cdCmdEnqueue(CD_COMMAND_MOUNT_STAGE, &gGameSession->location.loc, NULL);
    param1[3] = 0;
    param1[2] = 0x5A;
    param1[0] = gGameSession->location.loc.stage;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    param2[0] = 0;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
}

void Gp_EnqueueCompanionCd(u8 type, u8 variant)
{
    u8  param2[4];
    u8* param1;

    if (type == 0) {
        return;
    }

    param1                 = SCRATCH_STACK_RESERVE_BYTES(8);
    gGameSession->field_80 = 0;
    param1[3]              = 0;
    param1[2]              = 0x50;
    param1[0]              = 0;
    param2[0]              = type;
    param2[1]              = 0;
    param2[2]              = 4;
    param2[3]              = 6;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);

    if (variant != 0) {
        param1[3] = 0;
        param1[2] = 0x50;
        param1[0] = variant;
        param2[0] = type;
        param2[1] = 0;
        param2[2] = 4;
        param2[3] = 6;
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        if (variant == 5) {
            gGameSession->companionVariant                            = 3;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant = 3;
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(8);
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
    Gp_ResumeSessionTask,
    Gp_SessionState1,
    Gp_BeginSessionTask,
} };
const TaskFuncTable8 Gp_LoadStateFns  = { {
    Gp_LoadWaitBoot,
    Gp_LoadWaitStage,
    Gp_LoadState2,
    Gp_LoadWaitCompanion,
    Gp_LoadWaitSave,
    Gp_LoadWaitAreaCd,
    Gp_FadeGrayHold,
    Gp_LoadFinishTask,
} };
const TaskFuncTable3 Gp_RoomObjStates = { {
    Gp_LinkRoomObjectsSpawn,
    Gp_RoomObjState1,
    taskKill,
} };
