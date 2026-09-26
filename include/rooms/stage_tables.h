#ifndef ROOMS_STAGE_TABLES_H
#define ROOMS_STAGE_TABLES_H

/* Room objects the per-stage tables reach. A map UI overlay carries one
 * pointer per room for each kind of room record - its objects, views, sprites,
 * exits and so on - and each points into that room's own package, which is
 * loaded at the room slot while the room is current. Every room loads at the
 * same address, so each symbol is named after the room that holds it. The
 * room task entries and models the same overlay names are in rooms/room.h.
 */

#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

// dryfield_gas_station
extern GpRoomObjRec    D_dryfield_gas_station_8018314C[];
extern u8*             D_dryfield_gas_station_8018315C[];
extern GpRoomCoordRec  D_dryfield_gas_station_80183160[];
extern GpViewCountRec  D_dryfield_gas_station_80183168[];
extern GpWarpRec       D_dryfield_gas_station_8018316C[];
extern GpViewRec       D_dryfield_gas_station_80183EC8[];
extern GpSprtRec       D_dryfield_gas_station_801842A8[];
extern GpRoomParamRec* D_dryfield_gas_station_80184BAC[];

// dryfield_main_street
extern GpRoomObjRec    D_dryfield_main_street_80181BBC[];
extern u8*             D_dryfield_main_street_80181BCC[];
extern GpViewCountRec  D_dryfield_main_street_80181BD0[];
extern GpRoomCoordRec  D_dryfield_main_street_80181BD4[];
extern GpWarpRec       D_dryfield_main_street_80181BDC[];
extern GpViewRec       D_dryfield_main_street_80182CC0[];
extern GpSprtRec       D_dryfield_main_street_80184308[];
extern GpRoomParamRec* D_dryfield_main_street_801855EC[];

// dryfield_general_store
extern GpRoomObjRec    D_dryfield_general_store_8017E670[];
extern u8*             D_dryfield_general_store_8017E680[];
extern GpViewCountRec  D_dryfield_general_store_8017E684[];
extern GpRoomCoordRec  D_dryfield_general_store_8017E688[];
extern GpWarpRec       D_dryfield_general_store_8017E690[];
extern GpViewRec       D_dryfield_general_store_8017F25C[];
extern GpSprtRec       D_dryfield_general_store_8018402C[];
extern GpRoomParamRec* D_dryfield_general_store_801856D8[];

// dryfield_r04
extern GpRoomObjRec   D_dryfield_r04_8017D5C4[];
extern u8*            D_dryfield_r04_8017D5D4[];
extern GpViewCountRec D_dryfield_r04_8017D5D8[];
extern GpWarpRec      D_dryfield_r04_8017D5E4[];
/// The location's collision grid header; the grid is an asset.
extern GpGridParams    D_dryfield_r04_8017E1F4;
extern GpViewRec       D_dryfield_r04_8017E218[];
extern GpSprtRec       D_dryfield_r04_8017E280[];
extern GpRoomParamRec* D_dryfield_r04_8017E2AC[];

// dryfield_back_street
extern GpRoomObjRec    D_dryfield_back_street_8017F9B4[];
extern u8*             D_dryfield_back_street_8017F9C4[];
extern GpViewCountRec  D_dryfield_back_street_8017F9C8[];
extern GpRoomCoordRec  D_dryfield_back_street_8017F9CC[];
extern GpWarpRec       D_dryfield_back_street_8017F9D4[];
extern GpViewRec       D_dryfield_back_street_801802A8[];
extern GpSprtRec       D_dryfield_back_street_80180484[];
extern GpRoomParamRec* D_dryfield_back_street_80181034[];

// dryfield_souvenir_shop
extern GpRoomObjRec    D_dryfield_souvenir_shop_8017E0BC[];
extern u8*             D_dryfield_souvenir_shop_8017E0CC[];
extern GpViewCountRec  D_dryfield_souvenir_shop_8017E0D0[];
extern GpRoomCoordRec  D_dryfield_souvenir_shop_8017E0D4[];
extern GpWarpRec       D_dryfield_souvenir_shop_8017E0DC[];
extern GpViewRec       D_dryfield_souvenir_shop_8017E600[];
extern GpSprtRec       D_dryfield_souvenir_shop_8017EED0[];
extern GpRoomParamRec* D_dryfield_souvenir_shop_8017F640[];

// dryfield_warehouse
extern GpRoomObjRec    D_dryfield_warehouse_8017FBBC[];
extern u8*             D_dryfield_warehouse_8017FC04[];
extern GpViewCountRec  D_dryfield_warehouse_8017FC10[];
extern GpRoomCoordRec  D_dryfield_warehouse_8017FC18[];
extern GpWarpRec       D_dryfield_warehouse_8017FC30[];
extern GpViewRec       D_dryfield_warehouse_8018105C[];
extern GpSprtRec       D_dryfield_warehouse_80181638[];
extern GpRoomParamRec* D_dryfield_warehouse_80182194[];

// dryfield_r08
extern GpRoomObjRec    D_dryfield_r08_8017F6DC[];
extern u8*             D_dryfield_r08_8017F6FC[];
extern GpViewCountRec  D_dryfield_r08_8017F704[];
extern GpRoomCoordRec  D_dryfield_r08_8017F708[];
extern GpWarpRec       D_dryfield_r08_8017F718[];
extern GpViewRec       D_dryfield_r08_8017FBBC[];
extern GpSprtRec       D_dryfield_r08_80180918[];
extern GpRoomParamRec* D_dryfield_r08_80180C04[];

// dryfield_dilapidated_house
extern GpRoomObjRec    D_dryfield_dilapidated_house_80186954[];
extern u8*             D_dryfield_dilapidated_house_80186964[];
extern GpViewCountRec  D_dryfield_dilapidated_house_80186968[];
extern GpRoomCoordRec  D_dryfield_dilapidated_house_8018696C[];
extern GpWarpRec       D_dryfield_dilapidated_house_80186974[];
extern GpViewRec       D_dryfield_dilapidated_house_80187308[];
extern GpSprtRec       D_dryfield_dilapidated_house_80188C0C[];
extern GpRoomParamRec* D_dryfield_dilapidated_house_80189A80[];

// dryfield_motel_room_1
extern GpRoomObjRec    D_dryfield_motel_room_1_8017E484[];
extern u8*             D_dryfield_motel_room_1_8017E4B0[];
extern GpViewCountRec  D_dryfield_motel_room_1_8017E4B8[];
extern GpRoomCoordRec  D_dryfield_motel_room_1_8017E4BC[];
extern GpWarpRec       D_dryfield_motel_room_1_8017E4CC[];
extern GpViewRec       D_dryfield_motel_room_1_8017EAE0[];
extern GpSprtRec       D_dryfield_motel_room_1_80180C90[];
extern GpRoomParamRec* D_dryfield_motel_room_1_8018157C[];

// dryfield_motel_room_2
extern u8*             D_dryfield_motel_room_2_8017D6E4[];
extern GpRoomObjRec    D_dryfield_motel_room_2_8017D6E8[];
extern GpViewCountRec  D_dryfield_motel_room_2_8017D6F8[];
extern GpRoomCoordRec  D_dryfield_motel_room_2_8017D6FC[];
extern GpWarpRec       D_dryfield_motel_room_2_8017D704[];
extern GpViewRec       D_dryfield_motel_room_2_8017DE30[];
extern GpSprtRec       D_dryfield_motel_room_2_8017FCD0[];
extern GpRoomParamRec* D_dryfield_motel_room_2_801804B0[];

// dryfield_motel_room_3
extern GpRoomObjRec    D_dryfield_motel_room_3_8017D6DC[];
extern u8*             D_dryfield_motel_room_3_8017D6EC[];
extern GpViewCountRec  D_dryfield_motel_room_3_8017D6F0[];
extern GpRoomCoordRec  D_dryfield_motel_room_3_8017D6F4[];
extern GpWarpRec       D_dryfield_motel_room_3_8017D6FC[];
extern GpViewRec       D_dryfield_motel_room_3_8017DDC4[];
extern GpSprtRec       D_dryfield_motel_room_3_8017DF64[];
extern GpRoomParamRec* D_dryfield_motel_room_3_8017E524[];

// dryfield_motel_room_4
extern GpRoomObjRec    D_dryfield_motel_room_4_8017D6DC[];
extern u8*             D_dryfield_motel_room_4_8017D6EC[];
extern GpViewCountRec  D_dryfield_motel_room_4_8017D6F0[];
extern GpRoomCoordRec  D_dryfield_motel_room_4_8017D6F4[];
extern GpWarpRec       D_dryfield_motel_room_4_8017D6FC[];
extern GpViewRec       D_dryfield_motel_room_4_8017DE14[];
extern GpSprtRec       D_dryfield_motel_room_4_8017DF4C[];
extern GpRoomParamRec* D_dryfield_motel_room_4_8017E470[];

// dryfield_parking_lot
extern GpRoomObjRec    D_dryfield_parking_lot_8017DC44[];
extern u8*             D_dryfield_parking_lot_8017DC54[];
extern GpViewCountRec  D_dryfield_parking_lot_8017DC58[];
extern GpRoomCoordRec  D_dryfield_parking_lot_8017DC5C[];
extern GpWarpRec       D_dryfield_parking_lot_8017DC64[];
extern GpViewRec       D_dryfield_parking_lot_8017E900[];
extern GpSprtRec       D_dryfield_parking_lot_8017F054[];
extern GpRoomParamRec* D_dryfield_parking_lot_8017FB30[];

// dryfield_toilet
extern GpRoomObjRec    D_dryfield_toilet_8018112C[];
extern u8*             D_dryfield_toilet_8018113C[];
extern GpViewCountRec  D_dryfield_toilet_80181140[];
extern GpRoomCoordRec  D_dryfield_toilet_80181144[];
extern GpWarpRec       D_dryfield_toilet_8018114C[];
extern GpViewRec       D_dryfield_toilet_80181428[];
extern GpSprtRec       D_dryfield_toilet_801821F8[];
extern GpRoomParamRec* D_dryfield_toilet_8018660C[];

// dryfield_motel_lobby
extern GpRoomObjRec    D_dryfield_motel_lobby_8017F838[];
extern u8*             D_dryfield_motel_lobby_8017F848[];
extern GpViewCountRec  D_dryfield_motel_lobby_8017F84C[];
extern GpRoomCoordRec  D_dryfield_motel_lobby_8017F850[];
extern GpWarpRec       D_dryfield_motel_lobby_8017F858[];
extern GpViewRec       D_dryfield_motel_lobby_8017FC08[];
extern GpSprtRec       D_dryfield_motel_lobby_80180AB0[];
extern GpRoomParamRec* D_dryfield_motel_lobby_80181044[];

// dryfield_saloon_g_r
extern GpRoomObjRec    D_dryfield_saloon_g_r_8017EDA0[];
extern u8*             D_dryfield_saloon_g_r_8017EDD0[];
extern GpViewCountRec  D_dryfield_saloon_g_r_8017EDD8[];
extern GpRoomCoordRec  D_dryfield_saloon_g_r_8017EDDC[];
extern GpWarpRec       D_dryfield_saloon_g_r_8017EDEC[];
extern GpViewRec       D_dryfield_saloon_g_r_8017F7A4[];
extern GpSprtRec       D_dryfield_saloon_g_r_80180E2C[];
extern GpRoomParamRec* D_dryfield_saloon_g_r_80181BBC[];

// dryfield_g_r_kitchen
extern GpRoomObjRec    D_dryfield_g_r_kitchen_8017EC28[];
extern u8*             D_dryfield_g_r_kitchen_8017EC38[];
extern GpViewCountRec  D_dryfield_g_r_kitchen_8017EC3C[];
extern GpRoomCoordRec  D_dryfield_g_r_kitchen_8017EC40[];
extern GpWarpRec       D_dryfield_g_r_kitchen_8017EC48[];
extern GpViewRec       D_dryfield_g_r_kitchen_8017EEE4[];
extern GpSprtRec       D_dryfield_g_r_kitchen_8017F014[];
extern GpRoomParamRec* D_dryfield_g_r_kitchen_8017F53C[];

// dryfield_water_tower
extern GpRoomObjRec    D_dryfield_water_tower_801827CC[];
extern u8*             D_dryfield_water_tower_801827DC[];
extern GpViewCountRec  D_dryfield_water_tower_801827E0[];
extern GpRoomCoordRec  D_dryfield_water_tower_801827E4[];
extern GpWarpRec       D_dryfield_water_tower_801827EC[];
extern GpViewRec       D_dryfield_water_tower_801835E8[];
extern GpSprtRec       D_dryfield_water_tower_80186560[];
extern GpRoomParamRec* D_dryfield_water_tower_80187608[];

// dryfield_water_tank
extern GpRoomObjRec    D_dryfield_water_tank_801868E0[];
extern u8*             D_dryfield_water_tank_801868F0[];
extern GpViewCountRec  D_dryfield_water_tank_801868F4[];
extern GpRoomCoordRec  D_dryfield_water_tank_801868F8[];
extern GpWarpRec       D_dryfield_water_tank_80186900[];
extern GpViewRec       D_dryfield_water_tank_80186EE0[];
extern GpSprtRec       D_dryfield_water_tank_80187F80[];
extern GpRoomParamRec* D_dryfield_water_tank_80188CFC[];

// dryfield_breezeway
extern GpRoomObjRec    D_dryfield_breezeway_8018316C[];
extern u8*             D_dryfield_breezeway_8018317C[];
extern GpRoomCoordRec  D_dryfield_breezeway_80183180[];
extern GpViewCountRec  D_dryfield_breezeway_80183188[];
extern GpWarpRec       D_dryfield_breezeway_8018318C[];
extern GpViewRec       D_dryfield_breezeway_8018364C[];
extern GpSprtRec       D_dryfield_breezeway_80183D9C[];
extern GpRoomParamRec* D_dryfield_breezeway_8018437C[];

// dryfield_factory
extern GpRoomObjRec    D_dryfield_factory_80186F10[];
extern u8*             D_dryfield_factory_80186F44[];
extern GpRoomCoordRec  D_dryfield_factory_80186F4C[];
extern GpViewCountRec  D_dryfield_factory_80186F5C[];
extern GpWarpRec       D_dryfield_factory_80186F60[];
extern GpViewRec       D_dryfield_factory_80187C1C[];
extern GpSprtRec       D_dryfield_factory_801895B0[];
extern GpRoomParamRec* D_dryfield_factory_8018A37C[];

// dryfield_garage
extern GpRoomObjRec    D_dryfield_garage_8017DCDC[];
extern u8*             D_dryfield_garage_8017DCEC[];
extern GpViewCountRec  D_dryfield_garage_8017DCF0[];
extern GpRoomCoordRec  D_dryfield_garage_8017DCF4[];
extern GpWarpRec       D_dryfield_garage_8017DCFC[];
extern GpViewRec       D_dryfield_garage_8017E670[];
extern GpSprtRec       D_dryfield_garage_8017F5E8[];
extern GpRoomParamRec* D_dryfield_garage_801801E4[];

// dryfield_driveway
extern GpRoomObjRec    D_dryfield_driveway_8017E784[];
extern u8*             D_dryfield_driveway_8017E7AC[];
extern GpRoomCoordRec  D_dryfield_driveway_8017E7B4[];
extern GpViewCountRec  D_dryfield_driveway_8017E7C4[];
extern GpWarpRec       D_dryfield_driveway_8017E7C8[];
extern GpViewRec       D_dryfield_driveway_8017EE2C[];
extern GpSprtRec       D_dryfield_driveway_8017FC44[];
extern GpRoomParamRec* D_dryfield_driveway_80180660[];

// dryfield_junk_yard
extern GpRoomObjRec    D_dryfield_junk_yard_8017ED04[];
extern GpRoomCoordRec  D_dryfield_junk_yard_8017ED14[];
extern u8*             D_dryfield_junk_yard_8017ED1C[];
extern GpViewCountRec  D_dryfield_junk_yard_8017ED20[];
extern GpWarpRec       D_dryfield_junk_yard_8017ED24[];
extern GpViewRec       D_dryfield_junk_yard_8017F5C0[];
extern GpSprtRec       D_dryfield_junk_yard_80180C28[];
extern GpRoomParamRec* D_dryfield_junk_yard_80181C28[];

// dryfield_trailer_coach
extern GpRoomObjRec    D_dryfield_trailer_coach_801871CC[];
extern GpRoomCoordRec  D_dryfield_trailer_coach_801871DC[];
extern u8*             D_dryfield_trailer_coach_801871E4[];
extern GpViewCountRec  D_dryfield_trailer_coach_801871E8[];
extern GpWarpRec       D_dryfield_trailer_coach_801871EC[];
extern GpViewRec       D_dryfield_trailer_coach_80187758[];
extern GpSprtRec       D_dryfield_trailer_coach_801891D0[];
extern GpRoomParamRec* D_dryfield_trailer_coach_80189C30[];

// dryfield_motel_room_5
extern GpRoomObjRec    D_dryfield_motel_room_5_8017D6DC[];
extern u8*             D_dryfield_motel_room_5_8017D6EC[];
extern GpViewCountRec  D_dryfield_motel_room_5_8017D6F0[];
extern GpRoomCoordRec  D_dryfield_motel_room_5_8017D6F4[];
extern GpWarpRec       D_dryfield_motel_room_5_8017D6FC[];
extern GpViewRec       D_dryfield_motel_room_5_8017DCC0[];
extern GpSprtRec       D_dryfield_motel_room_5_8017DDF8[];
extern GpRoomParamRec* D_dryfield_motel_room_5_8017E5BC[];

// dryfield_motel_balcony
extern GpRoomObjRec    D_dryfield_motel_balcony_801822D0[];
extern u8*             D_dryfield_motel_balcony_801822E0[];
extern GpViewCountRec  D_dryfield_motel_balcony_801822E4[];
extern GpRoomCoordRec  D_dryfield_motel_balcony_801822E8[];
extern GpWarpRec       D_dryfield_motel_balcony_801822F0[];
extern GpViewRec       D_dryfield_motel_balcony_80182B80[];
extern GpSprtRec       D_dryfield_motel_balcony_80185C98[];
extern GpRoomParamRec* D_dryfield_motel_balcony_80186704[];

// dryfield_motel_room_6
extern GpRoomObjRec    D_dryfield_motel_room_6_80182D98[];
extern u8*             D_dryfield_motel_room_6_80182DA8[];
extern GpViewCountRec  D_dryfield_motel_room_6_80182DAC[];
extern GpRoomCoordRec  D_dryfield_motel_room_6_80182DB0[];
extern GpWarpRec       D_dryfield_motel_room_6_80182DB8[];
extern GpViewRec       D_dryfield_motel_room_6_80183840[];
extern GpSprtRec       D_dryfield_motel_room_6_801856CC[];
extern GpRoomParamRec* D_dryfield_motel_room_6_80186808[];

// dryfield_motel_loft
extern GpRoomObjRec    D_dryfield_motel_loft_8017D6DC[];
extern u8*             D_dryfield_motel_loft_8017D6EC[];
extern GpViewCountRec  D_dryfield_motel_loft_8017D6F0[];
extern GpRoomCoordRec  D_dryfield_motel_loft_8017D6F4[];
extern GpWarpRec       D_dryfield_motel_loft_8017D6FC[];
extern GpViewRec       D_dryfield_motel_loft_8017D9E0[];
extern GpSprtRec       D_dryfield_motel_loft_8017DDD8[];
extern GpRoomParamRec* D_dryfield_motel_loft_8017E648[];

// dryfield_water_hole
extern GpRoomObjRec    D_dryfield_water_hole_8017FD2C[];
extern u8*             D_dryfield_water_hole_8017FD84[];
extern GpViewCountRec  D_dryfield_water_hole_8017FD94[];
extern GpRoomCoordRec  D_dryfield_water_hole_8017FD9C[];
extern GpWarpRec       D_dryfield_water_hole_8017FDBC[];
extern GpViewRec       D_dryfield_water_hole_80180284[];
extern GpSprtRec       D_dryfield_water_hole_80181634[];
extern GpRoomParamRec* D_dryfield_water_hole_801828AC[];

// dryfield_cellar
extern GpRoomObjRec    D_dryfield_cellar_8017DBDC[];
extern u8*             D_dryfield_cellar_8017DC04[];
extern GpViewCountRec  D_dryfield_cellar_8017DC0C[];
extern GpRoomCoordRec  D_dryfield_cellar_8017DC10[];
extern GpWarpRec       D_dryfield_cellar_8017DC20[];
extern GpViewRec       D_dryfield_cellar_8017DF78[];
extern GpSprtRec       D_dryfield_cellar_8017FE40[];
extern GpRoomParamRec* D_dryfield_cellar_80180B40[];

// dryfield_underpass
extern GpRoomObjRec    D_dryfield_underpass_8017EB20[];
extern u8*             D_dryfield_underpass_8017EBBC[];
extern GpViewCountRec  D_dryfield_underpass_8017EBD4[];
extern GpRoomCoordRec  D_dryfield_underpass_8017EBE0[];
extern GpWarpRec       D_dryfield_underpass_8017EC10[];
extern GpViewRec       D_dryfield_underpass_8017F4A8[];
extern GpSprtRec       D_dryfield_underpass_80180250[];
extern GpRoomParamRec* D_dryfield_underpass_80181164[];

// acropolis_square
extern GpRoomObjRec    D_acropolis_square_80183B9C[];
extern u8*             D_acropolis_square_80183BAC[];
extern GpViewCountRec  D_acropolis_square_80183BB0[];
extern GpRoomCoordRec  D_acropolis_square_80183BB4[];
extern GpWarpRec       D_acropolis_square_80183BBC[];
extern GpSprtRec       D_acropolis_square_8018857C[];
extern GpViewRec       D_acropolis_square_80188630[];
extern GpRoomParamRec* D_acropolis_square_80188868[];

// acropolis_east_elevator_hall
extern GpRoomObjRec    D_acropolis_east_elevator_hall_80186320[];
extern u8*             D_acropolis_east_elevator_hall_80186330[];
extern GpViewCountRec  D_acropolis_east_elevator_hall_80186334[];
extern GpRoomCoordRec  D_acropolis_east_elevator_hall_80186338[];
extern GpWarpRec       D_acropolis_east_elevator_hall_80186340[];
extern GpSprtRec       D_acropolis_east_elevator_hall_80187870[];
extern GpViewRec       D_acropolis_east_elevator_hall_80187A5C[];
extern GpRoomParamRec* D_acropolis_east_elevator_hall_80187B74[];

// acropolis_patio
extern GpRoomObjRec    D_acropolis_patio_80182E68[];
extern u8*             D_acropolis_patio_80182EC0[];
extern GpViewCountRec  D_acropolis_patio_80182ECC[];
extern GpRoomCoordRec  D_acropolis_patio_80182ED4[];
extern GpWarpRec       D_acropolis_patio_80182EEC[];
extern GpSprtRec       D_acropolis_patio_80186360[];
extern GpViewRec       D_acropolis_patio_80186D5C[];
extern GpRoomParamRec* D_acropolis_patio_8018703C[];

// acropolis_cafeteria
extern GpRoomObjRec    D_acropolis_cafeteria_8018753C[];
extern u8*             D_acropolis_cafeteria_801875AC[];
extern GpViewCountRec  D_acropolis_cafeteria_801875BC[];
extern GpRoomCoordRec  D_acropolis_cafeteria_801875C4[];
extern GpWarpRec       D_acropolis_cafeteria_801875E4[];
extern GpSprtRec       D_acropolis_cafeteria_8018C48C[];
extern GpViewRec       D_acropolis_cafeteria_8018C5AC[];
extern GpRoomParamRec* D_acropolis_cafeteria_8018CA2C[];

// acropolis_plaza
extern GpRoomObjRec    D_acropolis_plaza_801988B8[];
extern u8*             D_acropolis_plaza_801988C8[];
extern GpViewCountRec  D_acropolis_plaza_801988CC[];
extern GpRoomCoordRec  D_acropolis_plaza_801988D0[];
extern GpViewRec       D_acropolis_plaza_801988D8[];
extern GpSprtRec       D_acropolis_plaza_80198A08[];
extern GpWarpRec       D_acropolis_plaza_80198A68[];
extern GpRoomParamRec* D_acropolis_plaza_80199F28[];

// acropolis_security_room
extern GpRoomObjRec    D_acropolis_security_room_801839D0[];
extern u8*             D_acropolis_security_room_801839E0[];
extern GpViewCountRec  D_acropolis_security_room_801839E4[];
extern GpRoomCoordRec  D_acropolis_security_room_801839E8[];
extern GpWarpRec       D_acropolis_security_room_801839F0[];
extern GpSprtRec       D_acropolis_security_room_80184C50[];
extern GpViewRec       D_acropolis_security_room_80184D10[];
extern GpRoomParamRec* D_acropolis_security_room_80184FA0[];

// acropolis_hallway
extern GpRoomObjRec    D_acropolis_hallway_8017E258[];
extern u8*             D_acropolis_hallway_8017E268[];
extern GpViewCountRec  D_acropolis_hallway_8017E26C[];
extern GpRoomCoordRec  D_acropolis_hallway_8017E270[];
extern GpWarpRec       D_acropolis_hallway_8017E278[];
extern GpSprtRec       D_acropolis_hallway_8017EC2C[];
extern GpViewRec       D_acropolis_hallway_8017EC68[];
extern GpRoomParamRec* D_acropolis_hallway_8017ED40[];

// acropolis_fountain
extern GpRoomObjRec    D_acropolis_fountain_8017E814[];
extern u8*             D_acropolis_fountain_8017E84C[];
extern GpViewCountRec  D_acropolis_fountain_8017E854[];
extern GpRoomCoordRec  D_acropolis_fountain_8017E858[];
extern GpWarpRec       D_acropolis_fountain_8017E868[];
extern GpSprtRec       D_acropolis_fountain_8018375C[];
extern GpViewRec       D_acropolis_fountain_80183864[];
extern GpRoomParamRec* D_acropolis_fountain_80183B90[];

// acropolis_forked_road
extern GpRoomObjRec    D_acropolis_forked_road_80182214[];
extern u8*             D_acropolis_forked_road_8018225C[];
extern GpViewCountRec  D_acropolis_forked_road_80182268[];
extern GpRoomCoordRec  D_acropolis_forked_road_80182270[];
extern GpWarpRec       D_acropolis_forked_road_80182288[];
extern GpSprtRec       D_acropolis_forked_road_801844E0[];
extern GpViewRec       D_acropolis_forked_road_80184E88[];
extern GpRoomParamRec* D_acropolis_forked_road_801850A4[];

// acropolis_observatory
extern GpRoomObjRec    D_acropolis_observatory_8017FEC8[];
extern u8*             D_acropolis_observatory_8017FEF0[];
extern GpViewCountRec  D_acropolis_observatory_8017FEF8[];
extern GpRoomCoordRec  D_acropolis_observatory_8017FEFC[];
extern GpWarpRec       D_acropolis_observatory_8017FF0C[];
extern GpSprtRec       D_acropolis_observatory_80183300[];
extern GpViewRec       D_acropolis_observatory_80183360[];
extern GpRoomParamRec* D_acropolis_observatory_801834DC[];

// acropolis_promenade
extern GpRoomObjRec    D_acropolis_promenade_80181B90[];
extern u8*             D_acropolis_promenade_80181BC0[];
extern GpViewCountRec  D_acropolis_promenade_80181BC8[];
extern GpRoomCoordRec  D_acropolis_promenade_80181BCC[];
extern GpWarpRec       D_acropolis_promenade_80181BDC[];
extern GpSprtRec       D_acropolis_promenade_80185FB4[];
extern GpViewRec       D_acropolis_promenade_80186050[];
extern GpRoomParamRec* D_acropolis_promenade_801862B0[];

// acropolis_sanctuary
extern GpRoomObjRec    D_acropolis_sanctuary_801827EC[];
extern u8*             D_acropolis_sanctuary_801827FC[];
extern GpViewCountRec  D_acropolis_sanctuary_80182800[];
extern GpRoomCoordRec  D_acropolis_sanctuary_80182804[];
extern GpWarpRec       D_acropolis_sanctuary_8018280C[];
extern GpSprtRec       D_acropolis_sanctuary_801860C8[];
extern GpViewRec       D_acropolis_sanctuary_80186188[];
extern GpRoomParamRec* D_acropolis_sanctuary_801863F8[];

// acropolis_roof_garden
extern GpRoomObjRec    D_acropolis_roof_garden_80184C8C[];
extern u8*             D_acropolis_roof_garden_80184C9C[];
extern GpViewCountRec  D_acropolis_roof_garden_80184CA0[];
extern GpRoomCoordRec  D_acropolis_roof_garden_80184CA4[];
extern GpWarpRec       D_acropolis_roof_garden_80184CAC[];
extern GpSprtRec       D_acropolis_roof_garden_80186648[];
extern GpViewRec       D_acropolis_roof_garden_80186BF4[];
extern GpRoomParamRec* D_acropolis_roof_garden_80186DB0[];

// acropolis_bridge
extern GpRoomObjRec    D_acropolis_bridge_80189A54[];
extern u8*             D_acropolis_bridge_80189A80[];
extern GpViewCountRec  D_acropolis_bridge_80189A88[];
extern GpRoomCoordRec  D_acropolis_bridge_80189A8C[];
extern GpWarpRec       D_acropolis_bridge_80189AB4[];
extern GpSprtRec       D_acropolis_bridge_8018FFA4[];
extern GpViewRec       D_acropolis_bridge_80190A24[];
extern GpRoomParamRec* D_acropolis_bridge_80190C34[];

// acropolis_fire_escape
extern GpRoomObjRec    D_acropolis_fire_escape_80181DAC[];
extern u8*             D_acropolis_fire_escape_80181DBC[];
extern GpViewCountRec  D_acropolis_fire_escape_80181DC0[];
extern GpRoomCoordRec  D_acropolis_fire_escape_80181DC4[];
extern GpWarpRec       D_acropolis_fire_escape_80181DCC[];
extern GpSprtRec       D_acropolis_fire_escape_80182E18[];
extern GpViewRec       D_acropolis_fire_escape_80182E90[];
extern GpRoomParamRec* D_acropolis_fire_escape_80183020[];

// acropolis_helicopter_landing_pad
extern GpRoomObjRec    D_acropolis_helicopter_landing_pad_80184F10[];
extern u8*             D_acropolis_helicopter_landing_pad_80184F3C[];
extern GpViewCountRec  D_acropolis_helicopter_landing_pad_80184F40[];
extern GpRoomCoordRec  D_acropolis_helicopter_landing_pad_80184F44[];
extern GpWarpRec       D_acropolis_helicopter_landing_pad_80184F4C[];
extern GpSprtRec       D_acropolis_helicopter_landing_pad_80187824[];
extern GpViewRec       D_acropolis_helicopter_landing_pad_80187968[];
extern GpRoomParamRec* D_acropolis_helicopter_landing_pad_80187DC8[];

// acropolis_west_elevator_hall
extern GpRoomObjRec    D_acropolis_west_elevator_hall_80185024[];
extern u8*             D_acropolis_west_elevator_hall_80185034[];
extern GpViewCountRec  D_acropolis_west_elevator_hall_80185038[];
extern GpRoomCoordRec  D_acropolis_west_elevator_hall_8018503C[];
extern GpWarpRec       D_acropolis_west_elevator_hall_80185044[];
extern GpSprtRec       D_acropolis_west_elevator_hall_80186408[];
extern GpViewRec       D_acropolis_west_elevator_hall_801869FC[];
extern GpRoomParamRec* D_acropolis_west_elevator_hall_80186AC4[];

// mist_r18
extern GpRoomObjRec    D_mist_r18_8018660C[];
extern u8*             D_mist_r18_8018661C[];
extern GpViewCountRec  D_mist_r18_80186620[];
extern GpRoomCoordRec  D_mist_r18_80186624[];
extern GpWarpRec       D_mist_r18_8018662C[];
extern GpViewRec       D_mist_r18_8018671C[];
extern GpSprtRec       D_mist_r18_80186B60[];
extern GpRoomParamRec* D_mist_r18_80186E70[];

// mist_parking
extern GpRoomObjRec    D_mist_parking_8019155C[];
extern u8*             D_mist_parking_801915B0[];
extern GpViewCountRec  D_mist_parking_801915C0[];
extern GpRoomCoordRec  D_mist_parking_801915C8[];
extern GpWarpRec       D_mist_parking_801915E8[];
extern GpViewRec       D_mist_parking_80192228[];
extern GpSprtRec       D_mist_parking_8019399C[];
extern GpRoomParamRec* D_mist_parking_801952F0[];

// mist_shooting_gallery
extern GpRoomObjRec    D_mist_shooting_gallery_801853A8[];
extern u8*             D_mist_shooting_gallery_801853B8[];
extern GpViewCountRec  D_mist_shooting_gallery_801853BC[];
extern GpRoomCoordRec  D_mist_shooting_gallery_801853C0[];
extern GpWarpRec       D_mist_shooting_gallery_801853C8[];
extern GpViewRec       D_mist_shooting_gallery_8018998C[];
extern GpSprtRec       D_mist_shooting_gallery_8018BD10[];
extern GpRoomParamRec* D_mist_shooting_gallery_8018E09C[];

// shelter_1f_parking_garage
extern GpRoomObjRec    D_shelter_1f_parking_garage_80180C64[];
extern GpRoomCoordRec  D_shelter_1f_parking_garage_80180C74[];
extern u8*             D_shelter_1f_parking_garage_80180C7C[];
extern GpViewCountRec  D_shelter_1f_parking_garage_80180C80[];
extern GpWarpRec       D_shelter_1f_parking_garage_80180C84[];
extern GpViewRec       D_shelter_1f_parking_garage_8018100C[];
extern GpSprtRec       D_shelter_1f_parking_garage_80181430[];
extern GpRoomParamRec* D_shelter_1f_parking_garage_80181954[];

// shelter_1f_vehicular_airlock
extern GpRoomObjRec    D_shelter_1f_vehicular_airlock_801820FC[];
extern GpRoomCoordRec  D_shelter_1f_vehicular_airlock_8018210C[];
extern u8*             D_shelter_1f_vehicular_airlock_80182114[];
extern GpViewCountRec  D_shelter_1f_vehicular_airlock_80182118[];
extern GpWarpRec       D_shelter_1f_vehicular_airlock_8018211C[];
extern GpViewRec       D_shelter_1f_vehicular_airlock_8018245C[];
extern GpSprtRec       D_shelter_1f_vehicular_airlock_801824F8[];
extern GpRoomParamRec* D_shelter_1f_vehicular_airlock_80182A80[];

// shelter_1f_bulwark
extern GpRoomObjRec    D_shelter_1f_bulwark_801803B0[];
extern GpRoomCoordRec  D_shelter_1f_bulwark_801803C0[];
extern u8*             D_shelter_1f_bulwark_801803C8[];
extern GpViewCountRec  D_shelter_1f_bulwark_801803CC[];
extern GpWarpRec       D_shelter_1f_bulwark_801803D0[];
extern GpViewRec       D_shelter_1f_bulwark_8018066C[];
extern GpSprtRec       D_shelter_1f_bulwark_801807B0[];
extern GpRoomParamRec* D_shelter_1f_bulwark_80180E9C[];

// shelter_1f_heliport
extern GpRoomObjRec    D_shelter_1f_heliport_801812D0[];
extern GpRoomCoordRec  D_shelter_1f_heliport_801812E0[];
extern u8*             D_shelter_1f_heliport_801812E8[];
extern GpViewCountRec  D_shelter_1f_heliport_801812EC[];
extern GpWarpRec       D_shelter_1f_heliport_801812F0[];
extern GpViewRec       D_shelter_1f_heliport_80181998[];
extern GpSprtRec       D_shelter_1f_heliport_80181EC0[];
extern GpRoomParamRec* D_shelter_1f_heliport_80182C78[];

// shelter_1f_airlock
extern GpRoomObjRec    D_shelter_1f_airlock_8017E59C[];
extern GpRoomCoordRec  D_shelter_1f_airlock_8017E5AC[];
extern u8*             D_shelter_1f_airlock_8017E5B4[];
extern GpViewCountRec  D_shelter_1f_airlock_8017E5B8[];
extern GpWarpRec       D_shelter_1f_airlock_8017E5BC[];
extern GpViewRec       D_shelter_1f_airlock_8017E85C[];
extern GpSprtRec       D_shelter_1f_airlock_8017F07C[];
extern GpRoomParamRec* D_shelter_1f_airlock_8017F84C[];

// shelter_1f_guardroom
extern GpRoomObjRec    D_shelter_1f_guardroom_8017DA78[];
extern GpRoomCoordRec  D_shelter_1f_guardroom_8017DA88[];
extern u8*             D_shelter_1f_guardroom_8017DA90[];
extern GpViewCountRec  D_shelter_1f_guardroom_8017DA94[];
extern GpWarpRec       D_shelter_1f_guardroom_8017DA98[];
extern GpViewRec       D_shelter_1f_guardroom_8017DC14[];
extern GpSprtRec       D_shelter_1f_guardroom_8017DCE0[];
extern GpRoomParamRec* D_shelter_1f_guardroom_8017DFF4[];

// neo_ark_observatory
extern GpRoomObjRec    D_neo_ark_observatory_80181594[];
extern GpRoomCoordRec  D_neo_ark_observatory_801815B4[];
extern u8*             D_neo_ark_observatory_801815DC[];
extern GpViewCountRec  D_neo_ark_observatory_801815E4[];
extern GpWarpRec       D_neo_ark_observatory_801815E8[];
extern GpViewRec       D_neo_ark_observatory_80181FC8[];
extern GpSprtRec       D_neo_ark_observatory_801860E8[];
extern GpRoomParamRec* D_neo_ark_observatory_80187A08[];

// neo_ark_eve_access_tunnel
extern GpRoomObjRec    D_neo_ark_eve_access_tunnel_8017EB78[];
extern GpRoomCoordRec  D_neo_ark_eve_access_tunnel_8017EB88[];
extern u8*             D_neo_ark_eve_access_tunnel_8017EB90[];
extern GpViewCountRec  D_neo_ark_eve_access_tunnel_8017EB94[];
extern GpWarpRec       D_neo_ark_eve_access_tunnel_8017EB98[];
extern GpViewRec       D_neo_ark_eve_access_tunnel_8017F080[];
extern GpSprtRec       D_neo_ark_eve_access_tunnel_801800A0[];
extern GpRoomParamRec* D_neo_ark_eve_access_tunnel_80180780[];

// neo_ark_eve_elevator
extern GpRoomObjRec    D_neo_ark_eve_elevator_8017D74C[];
extern GpRoomCoordRec  D_neo_ark_eve_elevator_8017D75C[];
extern u8*             D_neo_ark_eve_elevator_8017D764[];
extern GpViewCountRec  D_neo_ark_eve_elevator_8017D768[];
extern GpWarpRec       D_neo_ark_eve_elevator_8017D76C[];
extern GpViewRec       D_neo_ark_eve_elevator_8017DA50[];
extern GpSprtRec       D_neo_ark_eve_elevator_8017DB20[];
extern GpRoomParamRec* D_neo_ark_eve_elevator_8017DC30[];

// neo_ark_north_promenade
extern GpRoomObjRec    D_neo_ark_north_promenade_80181DB4[];
extern GpRoomCoordRec  D_neo_ark_north_promenade_80181DC4[];
extern u8*             D_neo_ark_north_promenade_80181DCC[];
extern GpViewCountRec  D_neo_ark_north_promenade_80181DD0[];
extern GpWarpRec       D_neo_ark_north_promenade_80181DD4[];
extern GpViewRec       D_neo_ark_north_promenade_80182410[];
extern GpSprtRec       D_neo_ark_north_promenade_80182CA4[];
extern GpRoomParamRec* D_neo_ark_north_promenade_801832EC[];

// neo_ark_forest_zone
extern GpRoomObjRec    D_neo_ark_forest_zone_801820A4[];
extern GpRoomCoordRec  D_neo_ark_forest_zone_801820B4[];
extern u8*             D_neo_ark_forest_zone_801820BC[];
extern GpViewCountRec  D_neo_ark_forest_zone_801820C0[];
extern GpWarpRec       D_neo_ark_forest_zone_801820C4[];
extern GpViewRec       D_neo_ark_forest_zone_80182298[];
extern GpSprtRec       D_neo_ark_forest_zone_80182594[];
extern GpRoomParamRec* D_neo_ark_forest_zone_80182CE4[];

// neo_ark_submarine_tunnel
extern GpRoomCoordRec  D_neo_ark_submarine_tunnel_80181E00[];
extern GpRoomObjRec    D_neo_ark_submarine_tunnel_80181E08[];
extern u8*             D_neo_ark_submarine_tunnel_80181E18[];
extern GpViewCountRec  D_neo_ark_submarine_tunnel_80181E1C[];
extern GpWarpRec       D_neo_ark_submarine_tunnel_80181E20[];
extern GpViewRec       D_neo_ark_submarine_tunnel_80182500[];
extern GpSprtRec       D_neo_ark_submarine_tunnel_80186B78[];
extern GpRoomParamRec* D_neo_ark_submarine_tunnel_801878EC[];

// neo_ark_pavilion
extern GpRoomObjRec    D_neo_ark_pavilion_801838B4[];
extern GpRoomCoordRec  D_neo_ark_pavilion_801838D4[];
extern u8*             D_neo_ark_pavilion_801838EC[];
extern GpViewCountRec  D_neo_ark_pavilion_801838F4[];
extern GpWarpRec       D_neo_ark_pavilion_801838F8[];
extern GpViewRec       D_neo_ark_pavilion_80184208[];
extern GpSprtRec       D_neo_ark_pavilion_801873B8[];
extern GpRoomParamRec* D_neo_ark_pavilion_801879EC[];

// neo_ark_island
extern GpRoomObjRec    D_neo_ark_island_80181B94[];
extern GpRoomCoordRec  D_neo_ark_island_80181BA4[];
extern u8*             D_neo_ark_island_80181BAC[];
extern GpViewCountRec  D_neo_ark_island_80181BB0[];
extern GpWarpRec       D_neo_ark_island_80181BB4[];
extern GpViewRec       D_neo_ark_island_801826EC[];
extern GpSprtRec       D_neo_ark_island_80183B14[];
extern GpRoomParamRec* D_neo_ark_island_80183FE8[];

// neo_ark_garden
extern GpRoomObjRec    D_neo_ark_garden_8018140C[];
extern GpRoomCoordRec  D_neo_ark_garden_8018141C[];
extern u8*             D_neo_ark_garden_80181424[];
extern GpViewCountRec  D_neo_ark_garden_80181428[];
extern GpWarpRec       D_neo_ark_garden_8018142C[];
extern GpViewRec       D_neo_ark_garden_801816E8[];
extern GpSprtRec       D_neo_ark_garden_80182540[];
extern GpRoomParamRec* D_neo_ark_garden_80182BD8[];

// neo_ark_power_plant_2
extern GpRoomObjRec    D_neo_ark_power_plant_2_80180690[];
extern GpRoomCoordRec  D_neo_ark_power_plant_2_801806A0[];
extern u8*             D_neo_ark_power_plant_2_801806A8[];
extern GpViewCountRec  D_neo_ark_power_plant_2_801806AC[];
extern GpWarpRec       D_neo_ark_power_plant_2_801806B0[];
extern GpViewRec       D_neo_ark_power_plant_2_80180DE8[];
extern GpSprtRec       D_neo_ark_power_plant_2_8018205C[];
extern GpRoomParamRec* D_neo_ark_power_plant_2_80182F50[];

// neo_ark_power_plant_1
extern GpRoomObjRec    D_neo_ark_power_plant_1_8017F1C8[];
extern GpRoomCoordRec  D_neo_ark_power_plant_1_8017F1D8[];
extern u8*             D_neo_ark_power_plant_1_8017F1E0[];
extern GpViewCountRec  D_neo_ark_power_plant_1_8017F1E4[];
extern GpWarpRec       D_neo_ark_power_plant_1_8017F1E8[];
extern GpViewRec       D_neo_ark_power_plant_1_801800B4[];
extern GpSprtRec       D_neo_ark_power_plant_1_801814F0[];
extern GpRoomParamRec* D_neo_ark_power_plant_1_80181BE0[];

// neo_ark_savanna_zone
extern GpRoomObjRec    D_neo_ark_savanna_zone_8017F9E4[];
extern GpRoomCoordRec  D_neo_ark_savanna_zone_8017F9F4[];
extern u8*             D_neo_ark_savanna_zone_8017F9FC[];
extern GpViewCountRec  D_neo_ark_savanna_zone_8017FA00[];
extern GpWarpRec       D_neo_ark_savanna_zone_8017FA04[];
extern GpViewRec       D_neo_ark_savanna_zone_8017FBF4[];
extern GpSprtRec       D_neo_ark_savanna_zone_801803F4[];
extern GpRoomParamRec* D_neo_ark_savanna_zone_80180968[];

// neo_ark_south_promenade
extern GpRoomCoordRec  D_neo_ark_south_promenade_8017F6EC[];
extern GpRoomObjRec    D_neo_ark_south_promenade_8017F6F4[];
extern u8*             D_neo_ark_south_promenade_8017F704[];
extern GpViewCountRec  D_neo_ark_south_promenade_8017F708[];
extern GpWarpRec       D_neo_ark_south_promenade_8017F70C[];
extern GpViewRec       D_neo_ark_south_promenade_8017FDB0[];
extern GpSprtRec       D_neo_ark_south_promenade_801803E4[];
extern GpRoomParamRec* D_neo_ark_south_promenade_801809AC[];

// neo_ark_altar
extern GpRoomObjRec    D_neo_ark_altar_8017F094[];
extern GpRoomCoordRec  D_neo_ark_altar_8017F0C4[];
extern u8*             D_neo_ark_altar_8017F0EC[];
extern GpViewCountRec  D_neo_ark_altar_8017F0F8[];
extern GpWarpRec       D_neo_ark_altar_8017F0FC[];
extern GpViewRec       D_neo_ark_altar_8017F5A0[];
extern GpSprtRec       D_neo_ark_altar_8017FE38[];
extern GpRoomParamRec* D_neo_ark_altar_8018005C[];

// neo_ark_shrine
extern GpRoomObjRec    D_neo_ark_shrine_80182724[];
extern GpRoomCoordRec  D_neo_ark_shrine_80182784[];
extern u8*             D_neo_ark_shrine_801827F0[];
extern GpViewCountRec  D_neo_ark_shrine_80182808[];
extern GpWarpRec       D_neo_ark_shrine_80182814[];
extern GpViewRec       D_neo_ark_shrine_801836BC[];
extern GpSprtRec       D_neo_ark_shrine_80185280[];
extern GpRoomParamRec* D_neo_ark_shrine_80186844[];

// shelter_b6_nursery
extern u8*             D_shelter_b6_nursery_80185304[];
extern GpViewCountRec  D_shelter_b6_nursery_80185308[];
extern GpWarpRec       D_shelter_b6_nursery_8018530C[];
extern GpGridParams    D_shelter_b6_nursery_801858A0;
extern GpViewRec       D_shelter_b6_nursery_801858C4[];
extern GpSprtRec       D_shelter_b6_nursery_80186FD0[];
extern GpRoomCoordSet  D_shelter_b6_nursery_80187294;
extern GpObj4A         D_shelter_b6_nursery_801872AC[];
extern GpObj4A         D_shelter_b6_nursery_8018750C[];
extern GpRoomBoundVec  D_shelter_b6_nursery_8018789C[];
extern GpRoomParamRec* D_shelter_b6_nursery_80187958[];

// shelter_b6_growth_room
extern u8*             D_shelter_b6_growth_room_8017F378[];
extern GpViewCountRec  D_shelter_b6_growth_room_8017F37C[];
extern GpWarpRec       D_shelter_b6_growth_room_8017F380[];
extern GpGridParams    D_shelter_b6_growth_room_8017FAF0;
extern GpViewRec       D_shelter_b6_growth_room_8017FB14[];
extern GpSprtRec       D_shelter_b6_growth_room_8017FEB8[];
extern GpRoomCoordSet  D_shelter_b6_growth_room_8017FF78;
extern GpObj4A         D_shelter_b6_growth_room_8017FF90[];
extern GpObj4A         D_shelter_b6_growth_room_801803A0[];
extern GpRoomBoundVec  D_shelter_b6_growth_room_80180730[];
extern GpRoomParamRec* D_shelter_b6_growth_room_801807A8[];

// shelter_b6_corridor
extern u8*             D_shelter_b6_corridor_8017F8B4[];
extern GpViewCountRec  D_shelter_b6_corridor_8017F8B8[];
extern GpWarpRec       D_shelter_b6_corridor_8017F8BC[];
extern GpGridParams    D_shelter_b6_corridor_8017FA90;
extern GpViewRec       D_shelter_b6_corridor_8017FAB4[];
extern GpSprtRec       D_shelter_b6_corridor_8018004C[];
extern GpRoomCoordSet  D_shelter_b6_corridor_801800E8;
extern GpObj4A         D_shelter_b6_corridor_80180100[];
extern GpObj4A         D_shelter_b6_corridor_8018036C[];
extern GpRoomBoundVec  D_shelter_b6_corridor_801804E8[];
extern GpRoomParamRec* D_shelter_b6_corridor_80180548[];

// shelter_b6_training_room
extern u8*             D_shelter_b6_training_room_80184418[];
extern GpViewCountRec  D_shelter_b6_training_room_8018441C[];
extern GpWarpRec       D_shelter_b6_training_room_80184420[];
extern GpGridParams    D_shelter_b6_training_room_80184734;
extern GpViewRec       D_shelter_b6_training_room_80184758[];
extern GpSprtRec       D_shelter_b6_training_room_80184D78[];
extern GpRoomCoordSet  D_shelter_b6_training_room_80185768;
extern GpObj4A         D_shelter_b6_training_room_80185780[];
extern GpObj4A         D_shelter_b6_training_room_80185A44[];
extern GpRoomBoundVec  D_shelter_b6_training_room_80185BC0[];
extern GpRoomParamRec* D_shelter_b6_training_room_80185C38[];

// neo_ark_r26
extern GpRoomObjRec    D_neo_ark_r26_8017E0CC[];
extern GpRoomCoordRec  D_neo_ark_r26_8017E0DC[];
extern u8*             D_neo_ark_r26_8017E0E4[];
extern GpViewCountRec  D_neo_ark_r26_8017E0E8[];
extern GpWarpRec       D_neo_ark_r26_8017E0EC[];
extern GpViewRec       D_neo_ark_r26_8017E1C0[];
extern GpSprtRec       D_neo_ark_r26_8017E898[];
extern GpRoomParamRec* D_neo_ark_r26_8017EA30[];

// neo_ark_bridge
extern u8*             D_neo_ark_bridge_80181F80[];
extern GpViewCountRec  D_neo_ark_bridge_80181F84[];
extern GpWarpRec       D_neo_ark_bridge_80181F88[];
extern GpGridParams    D_neo_ark_bridge_80182814;
extern GpViewRec       D_neo_ark_bridge_80182838[];
extern GpSprtRec       D_neo_ark_bridge_80184564[];
extern GpRoomCoordSet  D_neo_ark_bridge_8018470C;
extern GpObj4A         D_neo_ark_bridge_80184724[];
extern GpObj4A         D_neo_ark_bridge_80184AB8[];
extern GpRoomParamRec* D_neo_ark_bridge_80184BD4[];

// shelter_1f_tent
extern u8*             D_shelter_1f_tent_80181D44[];
extern GpViewCountRec  D_shelter_1f_tent_80181D48[];
extern GpWarpRec       D_shelter_1f_tent_80181D4C[];
extern GpGridParams    D_shelter_1f_tent_801822F0;
extern GpViewRec       D_shelter_1f_tent_80182314[];
extern GpSprtRec       D_shelter_1f_tent_801838E4[];
extern GpRoomCoordSet  D_shelter_1f_tent_80183A7C;
extern GpObj4A         D_shelter_1f_tent_80183A94[];
extern GpObj4A         D_shelter_1f_tent_80183CF4[];
extern GpRoomParamRec* D_shelter_1f_tent_801842B4[];

// neo_ark_woodland_path
extern u8*             D_neo_ark_woodland_path_80181694[];
extern GpViewCountRec  D_neo_ark_woodland_path_80181698[];
extern GpWarpRec       D_neo_ark_woodland_path_8018169C[];
extern GpGridParams    D_neo_ark_woodland_path_80181D5C;
extern GpViewRec       D_neo_ark_woodland_path_80181D80[];
extern GpSprtRec       D_neo_ark_woodland_path_80183C6C[];
extern GpRoomCoordSet  D_neo_ark_woodland_path_80183F84;
extern GpObj4A         D_neo_ark_woodland_path_80183F9C[];
extern GpObj4A         D_neo_ark_woodland_path_8018445C[];
extern GpRoomBoundVec  D_neo_ark_woodland_path_8018477C[];
extern GpObj3A         D_neo_ark_woodland_path_801847D4[];
extern GpRoomParamRec* D_neo_ark_woodland_path_80184910[];

// neo_ark_submarine_gallery
extern u8*             D_neo_ark_submarine_gallery_80181A08[];
extern GpViewCountRec  D_neo_ark_submarine_gallery_80181A0C[];
extern GpWarpRec       D_neo_ark_submarine_gallery_80181A10[];
extern GpGridParams    D_neo_ark_submarine_gallery_8018239C;
extern GpViewRec       D_neo_ark_submarine_gallery_801823C0[];
extern GpSprtRec       D_neo_ark_submarine_gallery_80184D10[];
extern GpRoomCoordSet  D_neo_ark_submarine_gallery_80185284;
extern GpObj4A         D_neo_ark_submarine_gallery_8018529C[];
extern GpObj4A         D_neo_ark_submarine_gallery_801854FC[];
extern GpRoomParamRec* D_neo_ark_submarine_gallery_801858EC[];

// neo_ark_r31
extern u8*             D_neo_ark_r31_8017DA1C[];
extern GpViewCountRec  D_neo_ark_r31_8017DA20[];
extern GpWarpRec       D_neo_ark_r31_8017DA24[];
extern GpViewRec       D_neo_ark_r31_8017DA5C[];
extern GpSprtRec       D_neo_ark_r31_8017DAF8[];
extern GpRoomCoordSet  D_neo_ark_r31_8017DB7C;
extern GpRoomParamRec* D_neo_ark_r31_8017DC34[];

// neo_ark_pyramid
extern GpRoomObjRec    D_neo_ark_pyramid_8017FC28[];
extern GpRoomCoordRec  D_neo_ark_pyramid_8017FC48[];
extern u8*             D_neo_ark_pyramid_8017FC60[];
extern GpViewCountRec  D_neo_ark_pyramid_8017FC68[];
extern GpWarpRec       D_neo_ark_pyramid_8017FC6C[];
extern GpViewRec       D_neo_ark_pyramid_801802E8[];
extern GpSprtRec       D_neo_ark_pyramid_80180E18[];
extern GpRoomParamRec* D_neo_ark_pyramid_80181884[];

// neo_ark_substation
extern GpRoomObjRec    D_neo_ark_substation_8017E3F0[];
extern GpRoomCoordRec  D_neo_ark_substation_8017E400[];
extern u8*             D_neo_ark_substation_8017E408[];
extern GpViewCountRec  D_neo_ark_substation_8017E40C[];
extern GpWarpRec       D_neo_ark_substation_8017E410[];
extern GpViewRec       D_neo_ark_substation_8017E8C8[];
extern GpSprtRec       D_neo_ark_substation_8017F584[];
extern GpRoomParamRec* D_neo_ark_substation_80180328[];

#endif /* ROOMS_STAGE_TABLES_H */
