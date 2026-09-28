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

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

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
// dryfield_night_gas_station
extern GpRoomCoordRec  D_dryfield_night_gas_station_80189DB0[];
extern GpRoomObjRec    D_dryfield_night_gas_station_80189DD0[];
extern u8*             D_dryfield_night_gas_station_80189E70[];
extern GpViewCountRec  D_dryfield_night_gas_station_80189E80[];
extern GpWarpRec       D_dryfield_night_gas_station_80189E88[];
extern GpViewRec       D_dryfield_night_gas_station_8018B780[];
extern GpSprtRec       D_dryfield_night_gas_station_8018F6C4[];
extern GpRoomParamRec* D_dryfield_night_gas_station_80190780[];

// dryfield_night_main_street
extern GpRoomObjRec    D_dryfield_night_main_street_80182284[];
extern GpRoomCoordRec  D_dryfield_night_main_street_801822B4[];
extern u8*             D_dryfield_night_main_street_801822FC[];
extern GpViewCountRec  D_dryfield_night_main_street_80182308[];
extern GpWarpRec       D_dryfield_night_main_street_80182310[];
extern GpViewRec       D_dryfield_night_main_street_80184564[];
extern GpSprtRec       D_dryfield_night_main_street_801875E4[];
extern GpRoomParamRec* D_dryfield_night_main_street_80188B84[];

// dryfield_night_general_store
extern GpRoomCoordRec  D_dryfield_night_general_store_8017E82C[];
extern GpRoomObjRec    D_dryfield_night_general_store_8017E834[];
extern u8*             D_dryfield_night_general_store_8017E844[];
extern GpViewCountRec  D_dryfield_night_general_store_8017E848[];
extern GpWarpRec       D_dryfield_night_general_store_8017E84C[];
extern GpViewRec       D_dryfield_night_general_store_8017F4A8[];
extern GpSprtRec       D_dryfield_night_general_store_80184278[];
extern GpRoomParamRec* D_dryfield_night_general_store_80185894[];

// dryfield_night_back_street
extern GpRoomObjRec    D_dryfield_night_back_street_801803AC[];
extern GpRoomCoordRec  D_dryfield_night_back_street_801803BC[];
extern u8*             D_dryfield_night_back_street_801803C4[];
extern GpViewCountRec  D_dryfield_night_back_street_801803C8[];
extern GpWarpRec       D_dryfield_night_back_street_801803CC[];
extern GpViewRec       D_dryfield_night_back_street_80180B58[];
extern GpSprtRec       D_dryfield_night_back_street_80180D34[];
extern GpRoomParamRec* D_dryfield_night_back_street_8018161C[];

// dryfield_night_souvenir_shop
extern GpRoomCoordRec  D_dryfield_night_souvenir_shop_8017E0E4[];
extern GpRoomObjRec    D_dryfield_night_souvenir_shop_8017E0EC[];
extern u8*             D_dryfield_night_souvenir_shop_8017E0FC[];
extern GpViewCountRec  D_dryfield_night_souvenir_shop_8017E100[];
extern GpWarpRec       D_dryfield_night_souvenir_shop_8017E104[];
extern GpViewRec       D_dryfield_night_souvenir_shop_8017E628[];
extern GpSprtRec       D_dryfield_night_souvenir_shop_8017EF08[];
extern GpRoomParamRec* D_dryfield_night_souvenir_shop_8017F6CC[];

// dryfield_night_warehouse
extern GpRoomCoordRec  D_dryfield_night_warehouse_8017E8E8[];
extern GpRoomObjRec    D_dryfield_night_warehouse_8017E900[];
extern u8*             D_dryfield_night_warehouse_8017E930[];
extern GpViewCountRec  D_dryfield_night_warehouse_8017E93C[];
extern GpWarpRec       D_dryfield_night_warehouse_8017E944[];
extern GpViewRec       D_dryfield_night_warehouse_8017EF2C[];
extern GpSprtRec       D_dryfield_night_warehouse_8017F46C[];
extern GpRoomParamRec* D_dryfield_night_warehouse_8017FC24[];

// dryfield_night_r08
extern GpRoomCoordRec  D_dryfield_night_r08_8018067C[];
extern GpRoomObjRec    D_dryfield_night_r08_80180684[];
extern u8*             D_dryfield_night_r08_80180694[];
extern GpViewCountRec  D_dryfield_night_r08_80180698[];
extern GpWarpRec       D_dryfield_night_r08_8018069C[];
extern GpViewRec       D_dryfield_night_r08_80181498[];
extern GpSprtRec       D_dryfield_night_r08_80181728[];
extern GpRoomParamRec* D_dryfield_night_r08_8018195C[];

// dryfield_night_dilapidated_house
extern GpRoomCoordRec  D_dryfield_night_dilapidated_house_8018738C[];
extern GpRoomObjRec    D_dryfield_night_dilapidated_house_80187394[];
extern u8*             D_dryfield_night_dilapidated_house_801873A4[];
extern GpViewCountRec  D_dryfield_night_dilapidated_house_801873A8[];
extern GpWarpRec       D_dryfield_night_dilapidated_house_801873AC[];
extern GpViewRec       D_dryfield_night_dilapidated_house_80187D68[];
extern GpSprtRec       D_dryfield_night_dilapidated_house_8018921C[];
extern GpRoomParamRec* D_dryfield_night_dilapidated_house_8018A0E4[];

// dryfield_night_motel_room_1
extern GpRoomCoordRec  D_dryfield_night_motel_room_1_8017DA64[];
extern GpRoomObjRec    D_dryfield_night_motel_room_1_8017DA74[];
extern u8*             D_dryfield_night_motel_room_1_8017DAA0[];
extern GpViewCountRec  D_dryfield_night_motel_room_1_8017DAA4[];
extern GpWarpRec       D_dryfield_night_motel_room_1_8017DAA8[];
extern GpViewRec       D_dryfield_night_motel_room_1_8017E0BC[];
extern GpSprtRec       D_dryfield_night_motel_room_1_8017FE98[];
extern GpRoomParamRec* D_dryfield_night_motel_room_1_80180844[];

// dryfield_night_motel_room_2
extern GpRoomCoordRec  D_dryfield_night_motel_room_2_8017DA5C[];
extern GpRoomObjRec    D_dryfield_night_motel_room_2_8017DA64[];
extern u8*             D_dryfield_night_motel_room_2_8017DA74[];
extern GpViewCountRec  D_dryfield_night_motel_room_2_8017DA78[];
extern GpWarpRec       D_dryfield_night_motel_room_2_8017DA7C[];
extern GpViewRec       D_dryfield_night_motel_room_2_8017E1A8[];
extern GpSprtRec       D_dryfield_night_motel_room_2_80180110[];
extern GpRoomParamRec* D_dryfield_night_motel_room_2_80180A90[];

// dryfield_night_motel_room_3
extern GpRoomCoordRec  D_dryfield_night_motel_room_3_8017DA9C[];
extern GpRoomObjRec    D_dryfield_night_motel_room_3_8017DAA4[];
extern u8*             D_dryfield_night_motel_room_3_8017DAB4[];
extern GpViewCountRec  D_dryfield_night_motel_room_3_8017DAB8[];
extern GpWarpRec       D_dryfield_night_motel_room_3_8017DABC[];
extern GpViewRec       D_dryfield_night_motel_room_3_8017E1A4[];
extern GpSprtRec       D_dryfield_night_motel_room_3_80180118[];
extern GpRoomParamRec* D_dryfield_night_motel_room_3_80180DC4[];

// dryfield_night_motel_room_4
extern GpRoomCoordRec  D_dryfield_night_motel_room_4_8017DA90[];
extern GpRoomObjRec    D_dryfield_night_motel_room_4_8017DA98[];
extern u8*             D_dryfield_night_motel_room_4_8017DAA8[];
extern GpViewCountRec  D_dryfield_night_motel_room_4_8017DAAC[];
extern GpWarpRec       D_dryfield_night_motel_room_4_8017DAB0[];
extern GpViewRec       D_dryfield_night_motel_room_4_8017E1F4[];
extern GpSprtRec       D_dryfield_night_motel_room_4_8017FB88[];
extern GpRoomParamRec* D_dryfield_night_motel_room_4_8018039C[];

// dryfield_night_parking_lot
extern GpRoomCoordRec  D_dryfield_night_parking_lot_8017EE14[];
extern GpRoomObjRec    D_dryfield_night_parking_lot_8017EE24[];
extern u8*             D_dryfield_night_parking_lot_8017EE4C[];
extern GpViewCountRec  D_dryfield_night_parking_lot_8017EE54[];
extern GpWarpRec       D_dryfield_night_parking_lot_8017EE58[];
extern GpViewRec       D_dryfield_night_parking_lot_8017FAF4[];
extern GpSprtRec       D_dryfield_night_parking_lot_801805AC[];
extern GpRoomParamRec* D_dryfield_night_parking_lot_8018153C[];

// dryfield_night_toilet
extern GpRoomCoordRec  D_dryfield_night_toilet_8017DAB0[];
extern GpRoomObjRec    D_dryfield_night_toilet_8017DAB8[];
extern u8*             D_dryfield_night_toilet_8017DAC8[];
extern GpViewCountRec  D_dryfield_night_toilet_8017DACC[];
extern GpWarpRec       D_dryfield_night_toilet_8017DAD0[];
extern GpViewRec       D_dryfield_night_toilet_8017DDAC[];
extern GpSprtRec       D_dryfield_night_toilet_8017EC40[];
extern GpRoomParamRec* D_dryfield_night_toilet_8017F3D8[];

// dryfield_night_motel_lobby
extern GpRoomObjRec    D_dryfield_night_motel_lobby_80182908[];
extern GpRoomCoordRec  D_dryfield_night_motel_lobby_80182918[];
extern u8*             D_dryfield_night_motel_lobby_80182920[];
extern GpViewCountRec  D_dryfield_night_motel_lobby_80182924[];
extern GpWarpRec       D_dryfield_night_motel_lobby_80182928[];
extern GpViewRec       D_dryfield_night_motel_lobby_80182DD8[];
extern GpSprtRec       D_dryfield_night_motel_lobby_80183D1C[];
extern GpRoomParamRec* D_dryfield_night_motel_lobby_8018448C[];

// dryfield_night_saloon_g_r
extern GpRoomCoordRec  D_dryfield_night_saloon_g_r_80185180[];
extern GpRoomObjRec    D_dryfield_night_saloon_g_r_80185190[];
extern u8*             D_dryfield_night_saloon_g_r_801851B0[];
extern GpViewCountRec  D_dryfield_night_saloon_g_r_801851B8[];
extern GpWarpRec       D_dryfield_night_saloon_g_r_801851BC[];
extern GpViewRec       D_dryfield_night_saloon_g_r_80185B74[];
extern GpSprtRec       D_dryfield_night_saloon_g_r_80187FC8[];
extern GpRoomParamRec* D_dryfield_night_saloon_g_r_80188F84[];

// dryfield_night_g_r_kitchen
extern GpRoomCoordRec  D_dryfield_night_g_r_kitchen_8017E2BC[];
extern GpRoomObjRec    D_dryfield_night_g_r_kitchen_8017E2C4[];
extern u8*             D_dryfield_night_g_r_kitchen_8017E2D4[];
extern GpViewCountRec  D_dryfield_night_g_r_kitchen_8017E2D8[];
extern GpWarpRec       D_dryfield_night_g_r_kitchen_8017E2DC[];
extern GpViewRec       D_dryfield_night_g_r_kitchen_8017E578[];
extern GpSprtRec       D_dryfield_night_g_r_kitchen_8017E6A8[];
extern GpRoomParamRec* D_dryfield_night_g_r_kitchen_8017EC04[];

// dryfield_night_water_tower
extern GpRoomCoordRec  D_dryfield_night_water_tower_8017E74C[];
extern GpRoomObjRec    D_dryfield_night_water_tower_8017E754[];
extern u8*             D_dryfield_night_water_tower_8017E764[];
extern GpViewCountRec  D_dryfield_night_water_tower_8017E768[];
extern GpWarpRec       D_dryfield_night_water_tower_8017E76C[];
extern GpViewRec       D_dryfield_night_water_tower_8017F418[];
extern GpSprtRec       D_dryfield_night_water_tower_80182040[];
extern GpRoomParamRec* D_dryfield_night_water_tower_80182C30[];

// dryfield_night_water_tank
extern GpRoomCoordRec  D_dryfield_night_water_tank_8017EE50[];
extern GpRoomObjRec    D_dryfield_night_water_tank_8017EE58[];
extern u8*             D_dryfield_night_water_tank_8017EE74[];
extern GpViewCountRec  D_dryfield_night_water_tank_8017EE78[];
extern GpWarpRec       D_dryfield_night_water_tank_8017EE7C[];
extern GpViewRec       D_dryfield_night_water_tank_8017F4D4[];
extern GpSprtRec       D_dryfield_night_water_tank_801801CC[];
extern GpRoomParamRec* D_dryfield_night_water_tank_80180890[];

// dryfield_night_breezeway
extern GpRoomCoordRec  D_dryfield_night_breezeway_8017E6E4[];
extern GpRoomObjRec    D_dryfield_night_breezeway_8017E6EC[];
extern u8*             D_dryfield_night_breezeway_8017E6FC[];
extern GpViewCountRec  D_dryfield_night_breezeway_8017E700[];
extern GpWarpRec       D_dryfield_night_breezeway_8017E704[];
extern GpViewRec       D_dryfield_night_breezeway_8017EBE8[];
extern GpSprtRec       D_dryfield_night_breezeway_8017FD10[];
extern GpRoomParamRec* D_dryfield_night_breezeway_801804B8[];

// dryfield_night_factory
extern u8*             D_dryfield_night_factory_80186F1C[];
extern GpRoomCoordRec  D_dryfield_night_factory_80186F24[];
extern GpRoomObjRec    D_dryfield_night_factory_80186F34[];
extern GpViewCountRec  D_dryfield_night_factory_80186F54[];
extern GpWarpRec       D_dryfield_night_factory_80186F58[];
extern GpViewRec       D_dryfield_night_factory_80187C14[];
extern GpSprtRec       D_dryfield_night_factory_80189A24[];
extern GpRoomParamRec* D_dryfield_night_factory_8018A79C[];

// dryfield_night_garage
extern GpRoomCoordRec  D_dryfield_night_garage_801833F4[];
extern GpRoomObjRec    D_dryfield_night_garage_80183404[];
extern u8*             D_dryfield_night_garage_80183434[];
extern GpViewCountRec  D_dryfield_night_garage_8018343C[];
extern GpWarpRec       D_dryfield_night_garage_80183440[];
extern GpViewRec       D_dryfield_night_garage_801843F8[];
extern GpSprtRec       D_dryfield_night_garage_80186258[];
extern GpRoomParamRec* D_dryfield_night_garage_801875B8[];

// dryfield_night_driveway
extern GpRoomCoordRec  D_dryfield_night_driveway_801805E0[];
extern GpRoomObjRec    D_dryfield_night_driveway_801805F0[];
extern u8*             D_dryfield_night_driveway_8018061C[];
extern GpViewCountRec  D_dryfield_night_driveway_80180624[];
extern GpWarpRec       D_dryfield_night_driveway_80180628[];
extern GpViewRec       D_dryfield_night_driveway_80180C30[];
extern GpSprtRec       D_dryfield_night_driveway_80181870[];
extern GpRoomParamRec* D_dryfield_night_driveway_801820F0[];

// dryfield_night_junk_yard
extern GpRoomCoordRec  D_dryfield_night_junk_yard_80180784[];
extern GpRoomObjRec    D_dryfield_night_junk_yard_80180794[];
extern u8*             D_dryfield_night_junk_yard_801807C0[];
extern GpViewCountRec  D_dryfield_night_junk_yard_801807C8[];
extern GpWarpRec       D_dryfield_night_junk_yard_801807CC[];
extern GpViewRec       D_dryfield_night_junk_yard_801811DC[];
extern GpSprtRec       D_dryfield_night_junk_yard_80183700[];
extern GpRoomParamRec* D_dryfield_night_junk_yard_801844C4[];

// dryfield_night_trailer_coach
extern GpRoomCoordRec  D_dryfield_night_trailer_coach_80189500[];
extern GpRoomObjRec    D_dryfield_night_trailer_coach_80189508[];
extern u8*             D_dryfield_night_trailer_coach_80189518[];
extern GpViewCountRec  D_dryfield_night_trailer_coach_8018951C[];
extern GpWarpRec       D_dryfield_night_trailer_coach_80189520[];
extern GpViewRec       D_dryfield_night_trailer_coach_80189A44[];
extern GpSprtRec       D_dryfield_night_trailer_coach_8018B64C[];
extern GpRoomParamRec* D_dryfield_night_trailer_coach_8018C1E8[];

// dryfield_night_motel_room_5
extern GpRoomCoordRec  D_dryfield_night_motel_room_5_8017DA70[];
extern GpRoomObjRec    D_dryfield_night_motel_room_5_8017DA80[];
extern u8*             D_dryfield_night_motel_room_5_8017DAAC[];
extern GpViewCountRec  D_dryfield_night_motel_room_5_8017DAB4[];
extern GpWarpRec       D_dryfield_night_motel_room_5_8017DAB8[];
extern GpViewRec       D_dryfield_night_motel_room_5_8017E060[];
extern GpSprtRec       D_dryfield_night_motel_room_5_80180994[];
extern GpRoomParamRec* D_dryfield_night_motel_room_5_80181230[];

// dryfield_night_motel_balcony
extern GpRoomCoordRec  D_dryfield_night_motel_balcony_80182E00[];
extern GpRoomObjRec    D_dryfield_night_motel_balcony_80182E18[];
extern u8*             D_dryfield_night_motel_balcony_80182E98[];
extern GpViewCountRec  D_dryfield_night_motel_balcony_80182EA4[];
extern GpWarpRec       D_dryfield_night_motel_balcony_80182EAC[];
extern GpViewRec       D_dryfield_night_motel_balcony_80184004[];
extern GpSprtRec       D_dryfield_night_motel_balcony_8018D078[];
extern GpRoomParamRec* D_dryfield_night_motel_balcony_8018F2AC[];

// dryfield_night_motel_room_6
extern GpRoomCoordRec  D_dryfield_night_motel_room_6_80182F00[];
extern GpRoomObjRec    D_dryfield_night_motel_room_6_80182F08[];
extern u8*             D_dryfield_night_motel_room_6_80182F18[];
extern GpViewCountRec  D_dryfield_night_motel_room_6_80182F1C[];
extern GpWarpRec       D_dryfield_night_motel_room_6_80182F20[];
extern GpViewRec       D_dryfield_night_motel_room_6_801839A8[];
extern GpSprtRec       D_dryfield_night_motel_room_6_801857C0[];
extern GpRoomParamRec* D_dryfield_night_motel_room_6_80186250[];

// dryfield_night_motel_loft
extern GpRoomCoordRec  D_dryfield_night_motel_loft_8017EDB0[];
extern GpRoomObjRec    D_dryfield_night_motel_loft_8017EDC0[];
extern u8*             D_dryfield_night_motel_loft_8017EDF0[];
extern GpViewCountRec  D_dryfield_night_motel_loft_8017EDF8[];
extern GpWarpRec       D_dryfield_night_motel_loft_8017EDFC[];
extern GpViewRec       D_dryfield_night_motel_loft_8017F144[];
extern GpSprtRec       D_dryfield_night_motel_loft_8017FBE4[];
extern GpRoomParamRec* D_dryfield_night_motel_loft_8018090C[];

// dryfield_night_water_hole
extern GpRoomObjRec    D_dryfield_night_water_hole_80180A04[];
extern GpRoomCoordRec  D_dryfield_night_water_hole_80180A44[];
extern u8*             D_dryfield_night_water_hole_80180A94[];
extern GpViewCountRec  D_dryfield_night_water_hole_80180AA4[];
extern GpWarpRec       D_dryfield_night_water_hole_80180AAC[];
extern GpViewRec       D_dryfield_night_water_hole_80180F74[];
extern GpSprtRec       D_dryfield_night_water_hole_80182384[];
extern GpRoomParamRec* D_dryfield_night_water_hole_801835F8[];

// dryfield_night_cellar
extern GpRoomCoordRec  D_dryfield_night_cellar_8017DAF0[];
extern GpRoomObjRec    D_dryfield_night_cellar_8017DB00[];
extern u8*             D_dryfield_night_cellar_8017DB28[];
extern GpViewCountRec  D_dryfield_night_cellar_8017DB30[];
extern GpWarpRec       D_dryfield_night_cellar_8017DB34[];
extern GpViewRec       D_dryfield_night_cellar_8017DE84[];
extern GpSprtRec       D_dryfield_night_cellar_8017FAB8[];
extern GpRoomParamRec* D_dryfield_night_cellar_801807F4[];

// dryfield_night_underpass
extern GpRoomObjRec    D_dryfield_night_underpass_8017DD70[];
extern GpRoomCoordRec  D_dryfield_night_underpass_8017DDD0[];
extern u8*             D_dryfield_night_underpass_8017DE3C[];
extern GpViewCountRec  D_dryfield_night_underpass_8017DE54[];
extern GpWarpRec       D_dryfield_night_underpass_8017DE60[];
extern GpViewRec       D_dryfield_night_underpass_8017E6F8[];
extern GpSprtRec       D_dryfield_night_underpass_8017F420[];
extern GpRoomParamRec* D_dryfield_night_underpass_80180374[];

// mine_mesa
extern GpRoomObjRec    D_mine_mesa_80186538[];
extern u8*             D_mine_mesa_80186548[];
extern GpRoomCoordRec  D_mine_mesa_8018654C[];
extern GpViewCountRec  D_mine_mesa_80186554[];
extern GpWarpRec       D_mine_mesa_80186558[];
extern GpViewRec       D_mine_mesa_80187030[];
extern GpSprtRec       D_mine_mesa_80188744[];
extern GpRoomParamRec* D_mine_mesa_80189A60[];

// mine_cavern
extern GpRoomObjRec    D_mine_cavern_80188FE0[];
extern GpRoomCoordRec  D_mine_cavern_80189010[];
extern u8*             D_mine_cavern_80189060[];
extern GpViewCountRec  D_mine_cavern_8018906C[];
extern GpWarpRec       D_mine_cavern_80189074[];
extern GpViewRec       D_mine_cavern_80189840[];
extern GpSprtRec       D_mine_cavern_8018CD10[];
extern GpRoomParamRec* D_mine_cavern_8018E30C[];

// mine_tunnel_entrance
extern GpRoomCoordRec  D_mine_tunnel_entrance_8017DB58[];
extern GpRoomObjRec    D_mine_tunnel_entrance_8017DB60[];
extern u8*             D_mine_tunnel_entrance_8017DB70[];
extern GpViewCountRec  D_mine_tunnel_entrance_8017DB74[];
extern GpWarpRec       D_mine_tunnel_entrance_8017DB78[];
extern GpViewRec       D_mine_tunnel_entrance_8017E0E4[];
extern GpSprtRec       D_mine_tunnel_entrance_8017EA4C[];
extern GpRoomParamRec* D_mine_tunnel_entrance_8017F3E8[];

// mine_tunnel
extern GpRoomCoordRec  D_mine_tunnel_8017E154[];
extern GpRoomObjRec    D_mine_tunnel_8017E15C[];
extern u8*             D_mine_tunnel_8017E16C[];
extern GpViewCountRec  D_mine_tunnel_8017E170[];
extern GpWarpRec       D_mine_tunnel_8017E174[];
extern GpViewRec       D_mine_tunnel_8017E890[];
extern GpSprtRec       D_mine_tunnel_8017F9A4[];
extern GpRoomParamRec* D_mine_tunnel_8018032C[];

// mine_gorge
extern GpRoomCoordRec  D_mine_gorge_8017E7A8[];
extern GpRoomObjRec    D_mine_gorge_8017E7B8[];
extern u8*             D_mine_gorge_8017E7E4[];
extern GpViewCountRec  D_mine_gorge_8017E7EC[];
extern GpWarpRec       D_mine_gorge_8017E7F0[];
extern GpViewRec       D_mine_gorge_8017FA14[];
extern GpSprtRec       D_mine_gorge_801827F8[];
extern GpRoomParamRec* D_mine_gorge_80183644[];

// mine_refuge
extern GpRoomCoordRec  D_mine_refuge_801818F0[];
extern GpRoomObjRec    D_mine_refuge_801818F8[];
extern u8*             D_mine_refuge_80181908[];
extern GpViewCountRec  D_mine_refuge_8018190C[];
extern GpWarpRec       D_mine_refuge_80181910[];
extern GpViewRec       D_mine_refuge_80181BC8[];
extern GpSprtRec       D_mine_refuge_8018264C[];
extern GpRoomParamRec* D_mine_refuge_80182AB4[];

// mine_forked_tunnel
extern GpRoomCoordRec  D_mine_forked_tunnel_80183634[];
extern GpRoomObjRec    D_mine_forked_tunnel_8018363C[];
extern u8*             D_mine_forked_tunnel_8018364C[];
extern GpViewCountRec  D_mine_forked_tunnel_80183650[];
extern GpWarpRec       D_mine_forked_tunnel_80183654[];
extern GpViewRec       D_mine_forked_tunnel_80183D94[];
extern GpSprtRec       D_mine_forked_tunnel_80184D64[];
extern GpRoomParamRec* D_mine_forked_tunnel_801855C0[];

// mine_secret_passage
extern GpRoomCoordRec  D_mine_secret_passage_80180F9C[];
extern GpRoomObjRec    D_mine_secret_passage_80180FA4[];
extern u8*             D_mine_secret_passage_80180FB4[];
extern GpViewCountRec  D_mine_secret_passage_80180FB8[];
extern GpWarpRec       D_mine_secret_passage_80180FBC[];
extern GpViewRec       D_mine_secret_passage_80181604[];
extern GpSprtRec       D_mine_secret_passage_80182994[];
extern GpRoomParamRec* D_mine_secret_passage_80183420[];

// shelter_b1_elevator_hall
extern GpRoomCoordRec  D_shelter_b1_elevator_hall_80182DF8[];
extern GpRoomObjRec    D_shelter_b1_elevator_hall_80182E00[];
extern u8*             D_shelter_b1_elevator_hall_80182E10[];
extern GpViewCountRec  D_shelter_b1_elevator_hall_80182E14[];
extern GpWarpRec       D_shelter_b1_elevator_hall_80182E18[];
extern GpViewRec       D_shelter_b1_elevator_hall_80183438[];
extern GpSprtRec       D_shelter_b1_elevator_hall_80183CC4[];
extern GpRoomParamRec* D_shelter_b1_elevator_hall_801849D0[];

// shelter_b1_south_maintenance_walkway
extern GpRoomCoordRec  D_shelter_b1_south_maintenance_walkway_801823F4[];
extern GpRoomObjRec    D_shelter_b1_south_maintenance_walkway_801823FC[];
extern u8*             D_shelter_b1_south_maintenance_walkway_8018240C[];
extern GpViewCountRec  D_shelter_b1_south_maintenance_walkway_80182410[];
extern GpWarpRec       D_shelter_b1_south_maintenance_walkway_80182414[];
extern GpViewRec       D_shelter_b1_south_maintenance_walkway_801827DC[];
extern GpSprtRec       D_shelter_b1_south_maintenance_walkway_80182E18[];
extern GpRoomParamRec* D_shelter_b1_south_maintenance_walkway_80183614[];

// shelter_b1_storeroom
extern GpRoomObjRec    D_shelter_b1_storeroom_80184B50[];
extern GpRoomCoordRec  D_shelter_b1_storeroom_80184B60[];
extern u8*             D_shelter_b1_storeroom_80184B68[];
extern GpViewCountRec  D_shelter_b1_storeroom_80184B6C[];
extern GpWarpRec       D_shelter_b1_storeroom_80184B70[];
extern GpViewRec       D_shelter_b1_storeroom_801850FC[];
extern GpSprtRec       D_shelter_b1_storeroom_80186090[];
extern GpRoomParamRec* D_shelter_b1_storeroom_80186DEC[];

// shelter_b1_north_maintenance_walkway
extern u8*             D_shelter_b1_north_maintenance_walkway_80184B80[];
extern GpViewCountRec  D_shelter_b1_north_maintenance_walkway_80184B84[];
extern GpWarpRec       D_shelter_b1_north_maintenance_walkway_80184B88[];
extern GpGridParams    D_shelter_b1_north_maintenance_walkway_80184F40;
extern GpViewRec       D_shelter_b1_north_maintenance_walkway_80184F64[];
extern GpSprtRec       D_shelter_b1_north_maintenance_walkway_801853AC[];
extern GpRoomCoordSet  D_shelter_b1_north_maintenance_walkway_801855D4;
extern GpObj4A         D_shelter_b1_north_maintenance_walkway_801855EC[];
extern GpObj3A         D_shelter_b1_north_maintenance_walkway_801857B4[];
extern GpObj4A         D_shelter_b1_north_maintenance_walkway_80185A98[];
extern GpRoomParamRec* D_shelter_b1_north_maintenance_walkway_80185B4C[];

// shelter_b1_armory
extern u8*             D_shelter_b1_armory_80182580[];
extern GpViewCountRec  D_shelter_b1_armory_80182584[];
extern GpWarpRec       D_shelter_b1_armory_80182588[];
extern GpGridParams    D_shelter_b1_armory_80182ED0;
extern GpViewRec       D_shelter_b1_armory_80182EF4[];
extern GpSprtRec       D_shelter_b1_armory_80184220[];
extern GpRoomCoordSet  D_shelter_b1_armory_80184AFC;
extern GpObj4A         D_shelter_b1_armory_80184B14[];
extern GpObj4A         D_shelter_b1_armory_80184E0C[];
extern GpRoomParamRec* D_shelter_b1_armory_80185554[];

// shelter_b1_sleeping_quarters
extern u8*             D_shelter_b1_sleeping_quarters_80180658[];
extern GpViewCountRec  D_shelter_b1_sleeping_quarters_8018065C[];
extern GpWarpRec       D_shelter_b1_sleeping_quarters_80180660[];
extern GpGridParams    D_shelter_b1_sleeping_quarters_801810D4;
extern GpViewRec       D_shelter_b1_sleeping_quarters_801810F8[];
extern GpSprtRec       D_shelter_b1_sleeping_quarters_80182E70[];
extern GpRoomCoordSet  D_shelter_b1_sleeping_quarters_80183234;
extern GpObj4A         D_shelter_b1_sleeping_quarters_8018324C[];
extern GpObj3A         D_shelter_b1_sleeping_quarters_801837A4[];
extern GpObj4A         D_shelter_b1_sleeping_quarters_801838D0[];
extern GpRoomParamRec* D_shelter_b1_sleeping_quarters_801840B0[];

// shelter_b1_main_corridor
extern u8*             D_shelter_b1_main_corridor_801831F8[];
extern GpViewCountRec  D_shelter_b1_main_corridor_801831FC[];
extern GpWarpRec       D_shelter_b1_main_corridor_80183200[];
extern GpGridParams    D_shelter_b1_main_corridor_801840F0;
extern GpViewRec       D_shelter_b1_main_corridor_80184114[];
extern GpSprtRec       D_shelter_b1_main_corridor_80185128[];
extern GpRoomCoordSet  D_shelter_b1_main_corridor_801853E0;
extern GpObj4A         D_shelter_b1_main_corridor_801853F8[];
extern GpObj4A         D_shelter_b1_main_corridor_801858B8[];
extern GpRoomParamRec* D_shelter_b1_main_corridor_80185D04[];

// shelter_b1_sterilization_room
extern GpRoomCoordRec  D_shelter_b1_sterilization_room_80189354[];
extern GpRoomObjRec    D_shelter_b1_sterilization_room_8018936C[];
extern u8*             D_shelter_b1_sterilization_room_801893CC[];
extern GpViewCountRec  D_shelter_b1_sterilization_room_801893D8[];
extern GpWarpRec       D_shelter_b1_sterilization_room_801893E0[];
extern GpViewRec       D_shelter_b1_sterilization_room_80189E68[];
extern GpSprtRec       D_shelter_b1_sterilization_room_8018B00C[];
extern GpRoomParamRec* D_shelter_b1_sterilization_room_8018C314[];

// shelter_b1_pod_access_tunnel
extern u8*             D_shelter_b1_pod_access_tunnel_80183A14[];
extern GpViewCountRec  D_shelter_b1_pod_access_tunnel_80183A18[];
extern GpWarpRec       D_shelter_b1_pod_access_tunnel_80183A1C[];
extern GpGridParams    D_shelter_b1_pod_access_tunnel_80183C24;
extern GpViewRec       D_shelter_b1_pod_access_tunnel_80183C48[];
extern GpSprtRec       D_shelter_b1_pod_access_tunnel_8018462C[];
extern GpRoomCoordSet  D_shelter_b1_pod_access_tunnel_80184734;
extern GpObj4A         D_shelter_b1_pod_access_tunnel_8018474C[];
extern GpObj3A         D_shelter_b1_pod_access_tunnel_8018487C[];
extern GpObj4A         D_shelter_b1_pod_access_tunnel_801848B8[];
extern GpRoomParamRec* D_shelter_b1_pod_access_tunnel_80184CDC[];

// shelter_b1_control_room
extern u8*             D_shelter_b1_control_room_80181C70[];
extern GpViewCountRec  D_shelter_b1_control_room_80181C74[];
extern GpWarpRec       D_shelter_b1_control_room_80181C78[];
extern GpGridParams    D_shelter_b1_control_room_801820F8;
extern GpViewRec       D_shelter_b1_control_room_8018211C[];
extern GpSprtRec       D_shelter_b1_control_room_801833BC[];
extern GpRoomCoordSet  D_shelter_b1_control_room_801834DC;
extern GpObj4A         D_shelter_b1_control_room_801834F4[];
extern GpObj4A         D_shelter_b1_control_room_80183624[];
extern GpRoomBoundVec  D_shelter_b1_control_room_80183B48[];
extern GpRoomParamRec* D_shelter_b1_control_room_80183BC0[];

// shelter_b1_access_tunnel
extern u8*             D_shelter_b1_access_tunnel_8017E7E4[];
extern GpViewCountRec  D_shelter_b1_access_tunnel_8017E7E8[];
extern GpWarpRec       D_shelter_b1_access_tunnel_8017E7EC[];
extern GpGridParams    D_shelter_b1_access_tunnel_8017EB24;
extern GpViewRec       D_shelter_b1_access_tunnel_8017EB48[];
extern GpSprtRec       D_shelter_b1_access_tunnel_8017F6A0[];
extern GpRoomCoordSet  D_shelter_b1_access_tunnel_8017FA1C;
extern GpObj4A         D_shelter_b1_access_tunnel_8017FA34[];
extern GpObj4A         D_shelter_b1_access_tunnel_8017FBFC[];
extern GpObj3A         D_shelter_b1_access_tunnel_8017FD2C[];
extern GpRoomParamRec* D_shelter_b1_access_tunnel_8017FF24[];

// shelter_b1_underground_parking
extern GpRoomCoordRec  D_shelter_b1_underground_parking_801877B4[];
extern GpRoomObjRec    D_shelter_b1_underground_parking_801877F4[];
extern u8*             D_shelter_b1_underground_parking_8018791C[];
extern GpViewCountRec  D_shelter_b1_underground_parking_8018793C[];
extern GpWarpRec       D_shelter_b1_underground_parking_8018794C[];
extern GpViewRec       D_shelter_b1_underground_parking_80189778[];
extern GpSprtRec       D_shelter_b1_underground_parking_8018AB9C[];
extern GpRoomParamRec* D_shelter_b1_underground_parking_8018D724[];

// shelter_b1_golem_freezer_1
extern u8*             D_shelter_b1_golem_freezer_1_8017E790[];
extern GpViewCountRec  D_shelter_b1_golem_freezer_1_8017E794[];
extern GpWarpRec       D_shelter_b1_golem_freezer_1_8017E798[];
extern GpGridParams    D_shelter_b1_golem_freezer_1_8017E9C0;
extern GpViewRec       D_shelter_b1_golem_freezer_1_8017E9E4[];
extern GpSprtRec       D_shelter_b1_golem_freezer_1_8017EDB0[];
extern GpRoomCoordSet  D_shelter_b1_golem_freezer_1_8017EE64;
extern GpObj4A         D_shelter_b1_golem_freezer_1_8017EE7C[];
extern GpObj4A         D_shelter_b1_golem_freezer_1_8017EFAC[];
extern GpRoomBoundVec  D_shelter_b1_golem_freezer_1_8017F234[];
extern GpRoomParamRec* D_shelter_b1_golem_freezer_1_8017F290[];

// shelter_b2_pod_bottom
extern GpRoomObjRec    D_shelter_b2_pod_bottom_80181D14[];
extern GpRoomCoordRec  D_shelter_b2_pod_bottom_80181D24[];
extern u8*             D_shelter_b2_pod_bottom_80181D2C[];
extern GpViewCountRec  D_shelter_b2_pod_bottom_80181D30[];
extern GpWarpRec       D_shelter_b2_pod_bottom_80181D34[];
extern GpViewRec       D_shelter_b2_pod_bottom_80182B80[];
extern GpSprtRec       D_shelter_b2_pod_bottom_80185904[];
extern GpRoomParamRec* D_shelter_b2_pod_bottom_80188770[];

// shelter_b1_pod_service_gantry
extern u8*             D_shelter_b1_pod_service_gantry_8017FB1C[];
extern GpViewCountRec  D_shelter_b1_pod_service_gantry_8017FB20[];
extern GpWarpRec       D_shelter_b1_pod_service_gantry_8017FB24[];
extern GpGridParams    D_shelter_b1_pod_service_gantry_801801C4;
extern GpViewRec       D_shelter_b1_pod_service_gantry_801801E8[];
extern GpSprtRec       D_shelter_b1_pod_service_gantry_80181BA0[];
extern GpRoomCoordSet  D_shelter_b1_pod_service_gantry_801824F4;
extern GpRoomParamRec* D_shelter_b1_pod_service_gantry_80182520[];

// shelter_b1_transfer_tunnel
extern u8*             D_shelter_b1_transfer_tunnel_80182954[];
extern GpViewCountRec  D_shelter_b1_transfer_tunnel_80182958[];
extern GpWarpRec       D_shelter_b1_transfer_tunnel_8018295C[];
extern GpGridParams    D_shelter_b1_transfer_tunnel_80182AEC;
extern GpViewRec       D_shelter_b1_transfer_tunnel_80182B10[];
extern GpSprtRec       D_shelter_b1_transfer_tunnel_80182BE0[];
extern GpRoomCoordSet  D_shelter_b1_transfer_tunnel_80182D90;
extern GpObj4A         D_shelter_b1_transfer_tunnel_80182DA8[];
extern GpObj4A         D_shelter_b1_transfer_tunnel_80182ED8[];
extern GpRoomParamRec* D_shelter_b1_transfer_tunnel_80183184[];

// shelter_b1_control_room_access_tunnel
extern u8*             D_shelter_b1_control_room_access_tunnel_80181F00[];
extern GpViewCountRec  D_shelter_b1_control_room_access_tunnel_80181F04[];
extern GpWarpRec       D_shelter_b1_control_room_access_tunnel_80181F08[];
extern GpGridParams    D_shelter_b1_control_room_access_tunnel_80182070;
extern GpViewRec       D_shelter_b1_control_room_access_tunnel_80182094[];
extern GpSprtRec       D_shelter_b1_control_room_access_tunnel_80182130[];
extern GpRoomCoordSet  D_shelter_b1_control_room_access_tunnel_801822D4;
extern GpObj4A         D_shelter_b1_control_room_access_tunnel_801822EC[];
extern GpObj4A         D_shelter_b1_control_room_access_tunnel_80182384[];
extern GpRoomParamRec* D_shelter_b1_control_room_access_tunnel_80182678[];

// shelter_b2_elevator
extern u8*             D_shelter_b2_elevator_8017DFD8[];
extern GpViewCountRec  D_shelter_b2_elevator_8017DFDC[];
extern GpWarpRec       D_shelter_b2_elevator_8017DFE0[];
extern GpGridParams    D_shelter_b2_elevator_8017E0E4;
extern GpViewRec       D_shelter_b2_elevator_8017E108[];
extern GpSprtRec       D_shelter_b2_elevator_8017E7BC[];
extern GpRoomCoordSet  D_shelter_b2_elevator_8017E840;
extern GpObj4A         D_shelter_b2_elevator_8017E858[];
extern GpObj4A         D_shelter_b2_elevator_8017E8F0[];
extern GpRoomParamRec* D_shelter_b2_elevator_8017E9D8[];

// shelter_b2_elevator_hall
extern u8*             D_shelter_b2_elevator_hall_801838DC[];
extern GpViewCountRec  D_shelter_b2_elevator_hall_801838E0[];
extern GpWarpRec       D_shelter_b2_elevator_hall_801838E4[];
extern GpGridParams    D_shelter_b2_elevator_hall_80183DB4;
extern GpViewRec       D_shelter_b2_elevator_hall_80183DD8[];
extern GpSprtRec       D_shelter_b2_elevator_hall_80184120[];
extern GpRoomCoordSet  D_shelter_b2_elevator_hall_801846B4;
extern GpObj4A         D_shelter_b2_elevator_hall_801846CC[];
extern GpObj3A         D_shelter_b2_elevator_hall_8018492C[];
extern GpObj4A         D_shelter_b2_elevator_hall_80184968[];
extern GpRoomParamRec* D_shelter_b2_elevator_hall_80184D5C[];

// shelter_b2_south_maintenance_walkway
extern u8*             D_shelter_b2_south_maintenance_walkway_8018263C[];
extern GpViewCountRec  D_shelter_b2_south_maintenance_walkway_80182640[];
extern GpWarpRec       D_shelter_b2_south_maintenance_walkway_80182644[];
extern GpGridParams    D_shelter_b2_south_maintenance_walkway_801829E8;
extern GpViewRec       D_shelter_b2_south_maintenance_walkway_80182A0C[];
extern GpSprtRec       D_shelter_b2_south_maintenance_walkway_80183018[];
extern GpRoomCoordSet  D_shelter_b2_south_maintenance_walkway_80183294;
extern GpObj4A         D_shelter_b2_south_maintenance_walkway_801832AC[];
extern GpObj4A         D_shelter_b2_south_maintenance_walkway_80183474[];
extern GpObj3A         D_shelter_b2_south_maintenance_walkway_8018385C[];
extern GpRoomParamRec* D_shelter_b2_south_maintenance_walkway_801838B4[];

// shelter_b2_operating_room
extern u8*             D_shelter_b2_operating_room_80180BC8[];
extern GpViewCountRec  D_shelter_b2_operating_room_80180BCC[];
extern GpWarpRec       D_shelter_b2_operating_room_80180BD0[];
extern GpGridParams    D_shelter_b2_operating_room_80181364;
extern GpViewRec       D_shelter_b2_operating_room_80181388[];
extern GpSprtRec       D_shelter_b2_operating_room_80183184[];
extern GpRoomCoordSet  D_shelter_b2_operating_room_80183718;
extern GpObj4A         D_shelter_b2_operating_room_80183730[];
extern GpObj3A         D_shelter_b2_operating_room_80183A28[];
extern GpObj4A         D_shelter_b2_operating_room_80183ADC[];
extern GpRoomBoundVec  D_shelter_b2_operating_room_80184184[];
extern GpRoomParamRec* D_shelter_b2_operating_room_801841F4[];

// shelter_b2_north_maintenance_walkway
extern u8*             D_shelter_b2_north_maintenance_walkway_80183C5C[];
extern GpViewCountRec  D_shelter_b2_north_maintenance_walkway_80183C60[];
extern GpWarpRec       D_shelter_b2_north_maintenance_walkway_80183C64[];
extern GpGridParams    D_shelter_b2_north_maintenance_walkway_8018401C;
extern GpViewRec       D_shelter_b2_north_maintenance_walkway_80184040[];
extern GpSprtRec       D_shelter_b2_north_maintenance_walkway_80185B04[];
extern GpRoomCoordSet  D_shelter_b2_north_maintenance_walkway_80185D44;
extern GpObj4A         D_shelter_b2_north_maintenance_walkway_80185D5C[];
extern GpObj4A         D_shelter_b2_north_maintenance_walkway_80185F24[];
extern GpObj3A         D_shelter_b2_north_maintenance_walkway_80186308[];
extern GpRoomParamRec* D_shelter_b2_north_maintenance_walkway_80186360[];

// shelter_b2_laboratory
extern u8*             D_shelter_b2_laboratory_80182C08[];
extern GpViewCountRec  D_shelter_b2_laboratory_80182C0C[];
extern GpWarpRec       D_shelter_b2_laboratory_80182C10[];
extern GpGridParams    D_shelter_b2_laboratory_8018355C;
extern GpViewRec       D_shelter_b2_laboratory_80183580[];
extern GpSprtRec       D_shelter_b2_laboratory_801854D0[];
extern GpRoomCoordSet  D_shelter_b2_laboratory_80185944;
extern GpObj4A         D_shelter_b2_laboratory_8018595C[];
extern GpObj4A         D_shelter_b2_laboratory_80185D84[];
extern GpRoomBoundVec  D_shelter_b2_laboratory_801863B8[];
extern GpRoomParamRec* D_shelter_b2_laboratory_80186468[];

// shelter_b2_breeding_room
extern u8*             D_shelter_b2_breeding_room_8018055C[];
extern GpViewCountRec  D_shelter_b2_breeding_room_80180560[];
extern GpWarpRec       D_shelter_b2_breeding_room_80180564[];
extern GpGridParams    D_shelter_b2_breeding_room_801810F4;
extern GpViewRec       D_shelter_b2_breeding_room_80181118[];
extern GpSprtRec       D_shelter_b2_breeding_room_801833D4[];
extern GpRoomCoordSet  D_shelter_b2_breeding_room_801837AC;
extern GpObj4A         D_shelter_b2_breeding_room_801837C4[];
extern GpObj4A         D_shelter_b2_breeding_room_80183F9C[];
extern GpRoomBoundVec  D_shelter_b2_breeding_room_80184624[];
extern GpObj3A         D_shelter_b2_breeding_room_8018467C[];
extern GpRoomParamRec* D_shelter_b2_breeding_room_801847F4[];

// shelter_b2_main_corridor
extern u8*             D_shelter_b2_main_corridor_801830CC[];
extern GpViewCountRec  D_shelter_b2_main_corridor_801830D0[];
extern GpWarpRec       D_shelter_b2_main_corridor_801830D4[];
extern GpGridParams    D_shelter_b2_main_corridor_80184440;
extern GpViewRec       D_shelter_b2_main_corridor_80184464[];
extern GpSprtRec       D_shelter_b2_main_corridor_80188848[];
extern GpRoomCoordSet  D_shelter_b2_main_corridor_80188BE4;
extern GpObj4A         D_shelter_b2_main_corridor_80188BFC[];
extern GpObj4A         D_shelter_b2_main_corridor_801893EC[];
extern GpRoomParamRec* D_shelter_b2_main_corridor_80189624[];

// shelter_b2_septic_tank
extern u8*             D_shelter_b2_septic_tank_8018356C[];
extern GpViewCountRec  D_shelter_b2_septic_tank_80183570[];
extern GpWarpRec       D_shelter_b2_septic_tank_80183574[];
extern GpGridParams    D_shelter_b2_septic_tank_80183E0C;
extern GpViewRec       D_shelter_b2_septic_tank_80183E30[];
extern GpSprtRec       D_shelter_b2_septic_tank_801866F4[];
extern GpRoomCoordSet  D_shelter_b2_septic_tank_80186A3C;
extern GpObj4A         D_shelter_b2_septic_tank_80186A54[];
extern GpObj4A         D_shelter_b2_septic_tank_80186C1C[];
extern GpRoomParamRec* D_shelter_b2_septic_tank_80187014[];

// shelter_b2_pod_access_tunnel
extern GpRoomObjRec    D_shelter_b2_pod_access_tunnel_80183DEC[];
extern GpRoomCoordRec  D_shelter_b2_pod_access_tunnel_80183E0C[];
extern u8*             D_shelter_b2_pod_access_tunnel_80183E24[];
extern GpViewCountRec  D_shelter_b2_pod_access_tunnel_80183E2C[];
extern GpWarpRec       D_shelter_b2_pod_access_tunnel_80183E30[];
extern GpViewRec       D_shelter_b2_pod_access_tunnel_801841D8[];
extern GpSprtRec       D_shelter_b2_pod_access_tunnel_80184C6C[];
extern GpRoomParamRec* D_shelter_b2_pod_access_tunnel_801856D8[];

// shelter_r36
extern u8*             D_shelter_r36_8017E9BC[];
extern GpViewCountRec  D_shelter_r36_8017E9C0[];
extern GpWarpRec       D_shelter_r36_8017E9C4[];
extern GpGridParams    D_shelter_r36_8017EAB0;
extern GpViewRec       D_shelter_r36_8017EAD4[];
extern GpSprtRec       D_shelter_r36_8017F318[];
extern GpRoomCoordSet  D_shelter_r36_8017F6DC;
extern GpObj4A         D_shelter_r36_8017F6F4[];
extern GpRoomParamRec* D_shelter_r36_8017FAE4[];

// shelter_r37
extern u8*             D_shelter_r37_8017D6F8[];
extern GpViewCountRec  D_shelter_r37_8017D6FC[];
extern GpWarpRec       D_shelter_r37_8017D700[];
extern GpGridParams    D_shelter_r37_8017D920;
extern GpViewRec       D_shelter_r37_8017D944[];
extern GpSprtRec       D_shelter_r37_8017D9E0[];
extern GpRoomCoordSet  D_shelter_r37_8017DD44;
extern GpObj4A         D_shelter_r37_8017DD5C[];
extern GpRoomParamRec* D_shelter_r37_8017DED8[];

// shelter_1f_heliport_s4
extern u8*             D_shelter_1f_heliport_s4_8017D6F8[];
extern GpViewCountRec  D_shelter_1f_heliport_s4_8017D6FC[];
extern GpWarpRec       D_shelter_1f_heliport_s4_8017D700[];
extern GpGridParams    D_shelter_1f_heliport_s4_8017D9C8;
extern GpViewRec       D_shelter_1f_heliport_s4_8017D9EC[];
extern GpSprtRec       D_shelter_1f_heliport_s4_8017DAF0[];
extern GpRoomCoordSet  D_shelter_1f_heliport_s4_8017DE6C;
extern GpObj4A         D_shelter_1f_heliport_s4_8017DE84[];
extern GpRoomParamRec* D_shelter_1f_heliport_s4_8017E060[];

// shelter_b3_dumping_hole
extern GpRoomObjRec    D_shelter_b3_dumping_hole_8018B678[];
extern u8*             D_shelter_b3_dumping_hole_8018B698[];
extern GpViewCountRec  D_shelter_b3_dumping_hole_8018B6A0[];
extern GpWarpRec       D_shelter_b3_dumping_hole_8018B6A4[];
extern GpViewRec       D_shelter_b3_dumping_hole_8018C410[];
extern GpSprtRec       D_shelter_b3_dumping_hole_8018E050[];
extern GpRoomCoordSet  D_shelter_b3_dumping_hole_8018E3DC;
extern GpRoomCoordSet  D_shelter_b3_dumping_hole_8018E874;
extern GpRoomBoundVec  D_shelter_b3_dumping_hole_8018F1FC[];
extern GpRoomBoundVec  D_shelter_b3_dumping_hole_8018F32C[];
extern GpRoomParamRec* D_shelter_b3_dumping_hole_8018F480[];

// shelter_b3_garbage_incinerator
extern GpRoomCoordRec  D_shelter_b3_garbage_incinerator_80187280[];
extern GpRoomObjRec    D_shelter_b3_garbage_incinerator_801872B8[];
extern u8*             D_shelter_b3_garbage_incinerator_801873F0[];
extern GpViewCountRec  D_shelter_b3_garbage_incinerator_8018740C[];
extern GpWarpRec       D_shelter_b3_garbage_incinerator_8018741C[];
extern GpViewRec       D_shelter_b3_garbage_incinerator_801883AC[];
extern GpSprtRec       D_shelter_b3_garbage_incinerator_8018D100[];
extern GpRoomParamRec* D_shelter_b3_garbage_incinerator_8018FB4C[];

// shelter_b3_incinerator_control_room
extern u8*             D_shelter_b3_incinerator_control_room_80181920[];
extern GpViewCountRec  D_shelter_b3_incinerator_control_room_80181928[];
extern GpWarpRec       D_shelter_b3_incinerator_control_room_8018192C[];
extern GpGridParams    D_shelter_b3_incinerator_control_room_80181CC0;
extern GpViewRec       D_shelter_b3_incinerator_control_room_80181CE4[];
extern GpSprtRec       D_shelter_b3_incinerator_control_room_80182140[];
extern GpRoomCoordSet  D_shelter_b3_incinerator_control_room_801824A0;
extern GpObj4A         D_shelter_b3_incinerator_control_room_801824B8[];
extern GpObj4A         D_shelter_b3_incinerator_control_room_80182668[];
extern GpObj4A         D_shelter_b3_incinerator_control_room_801827E4[];
extern GpObj3A         D_shelter_b3_incinerator_control_room_801829AC[];
extern GpRoomParamRec* D_shelter_b3_incinerator_control_room_80182A20[];

// shelter_b3_elevator_hall
extern u8*             D_shelter_b3_elevator_hall_80182B54[];
extern GpViewCountRec  D_shelter_b3_elevator_hall_80182B58[];
extern GpWarpRec       D_shelter_b3_elevator_hall_80182B5C[];
extern GpGridParams    D_shelter_b3_elevator_hall_801834C8;
extern GpViewRec       D_shelter_b3_elevator_hall_801834EC[];
extern GpSprtRec       D_shelter_b3_elevator_hall_801841DC[];
extern GpRoomCoordSet  D_shelter_b3_elevator_hall_80184410;
extern GpObj4A         D_shelter_b3_elevator_hall_80184428[];
extern GpObj4A         D_shelter_b3_elevator_hall_801847DC[];
extern GpObj3A         D_shelter_b3_elevator_hall_8018490C[];
extern GpRoomParamRec* D_shelter_b3_elevator_hall_801849E0[];

// shelter_b4_lower_sewer
extern u8*             D_shelter_b4_lower_sewer_80181FA4[];
extern GpViewCountRec  D_shelter_b4_lower_sewer_80181FA8[];
extern GpWarpRec       D_shelter_b4_lower_sewer_80181FAC[];
extern GpGridParams    D_shelter_b4_lower_sewer_801828E4;
extern GpViewRec       D_shelter_b4_lower_sewer_80182908[];
extern GpSprtRec       D_shelter_b4_lower_sewer_80182E80[];
extern GpRoomCoordSet  D_shelter_b4_lower_sewer_8018342C;
extern GpObj4A         D_shelter_b4_lower_sewer_80183444[];
extern GpObj4A         D_shelter_b4_lower_sewer_801837D4[];
extern GpRoomParamRec* D_shelter_b4_lower_sewer_80183DF4[];

// shelter_b4_upper_sewer
extern u8*             D_shelter_b4_upper_sewer_80186590[];
extern GpViewCountRec  D_shelter_b4_upper_sewer_80186594[];
extern GpWarpRec       D_shelter_b4_upper_sewer_80186598[];
extern GpGridParams    D_shelter_b4_upper_sewer_80186EF8;
extern GpViewRec       D_shelter_b4_upper_sewer_80186F1C[];
extern GpSprtRec       D_shelter_b4_upper_sewer_801879BC[];
extern GpRoomCoordSet  D_shelter_b4_upper_sewer_80188184;
extern GpObj4A         D_shelter_b4_upper_sewer_8018819C[];
extern GpObj4A         D_shelter_b4_upper_sewer_801886F4[];
extern GpObj3A         D_shelter_b4_upper_sewer_80188BFC[];
extern GpRoomParamRec* D_shelter_b4_upper_sewer_80188CFC[];

// shelter_b4_reservoir
extern GpRoomCoordRec  D_shelter_b4_reservoir_801850E8[];
extern GpRoomObjRec    D_shelter_b4_reservoir_801850F8[];
extern u8*             D_shelter_b4_reservoir_80185118[];
extern GpViewCountRec  D_shelter_b4_reservoir_80185120[];
extern GpWarpRec       D_shelter_b4_reservoir_80185124[];
extern GpViewRec       D_shelter_b4_reservoir_80185ADC[];
extern GpSprtRec       D_shelter_b4_reservoir_80186730[];
extern GpRoomParamRec* D_shelter_b4_reservoir_80187480[];

// shelter_b4_water_supply
extern u8*             D_shelter_b4_water_supply_8018273C[];
extern GpViewCountRec  D_shelter_b4_water_supply_80182740[];
extern GpWarpRec       D_shelter_b4_water_supply_80182744[];
extern GpGridParams    D_shelter_b4_water_supply_80182E3C;
extern GpViewRec       D_shelter_b4_water_supply_80182E60[];
extern GpSprtRec       D_shelter_b4_water_supply_80183F90[];
extern GpRoomCoordSet  D_shelter_b4_water_supply_801843D4;
extern GpObj4A         D_shelter_b4_water_supply_801843EC[];
extern GpObj4A         D_shelter_b4_water_supply_80184944[];
extern GpObj3A         D_shelter_b4_water_supply_80184D04[];
extern GpRoomBoundVec  D_shelter_b4_water_supply_80184D7C[];
extern GpRoomParamRec* D_shelter_b4_water_supply_80184E14[];

// shelter_r47
extern u8*             D_shelter_r47_80187674[];
extern GpViewCountRec  D_shelter_r47_80187678[];
extern GpWarpRec       D_shelter_r47_8018767C[];
extern GpObj4A         D_shelter_r47_801876B4[];
extern GpObj4A         D_shelter_r47_8018787C[];
extern GpGridParams    D_shelter_r47_8018828C;
extern GpViewRec       D_shelter_r47_801882B0[];
extern GpSprtRec       D_shelter_r47_80189C68[];
extern GpRoomCoordSet  D_shelter_r47_8018A5BC;
extern GpRoomParamRec* D_shelter_r47_8018A618[];

// shelter_r48
extern GpRoomCoordRec  D_shelter_r48_80183014[];
extern GpRoomObjRec    D_shelter_r48_8018301C[];
extern u8*             D_shelter_r48_8018302C[];
extern GpViewCountRec  D_shelter_r48_80183030[];
extern GpWarpRec       D_shelter_r48_80183034[];
extern GpViewRec       D_shelter_r48_80183F10[];
extern GpSprtRec       D_shelter_r48_80189FB4[];
extern GpRoomParamRec* D_shelter_r48_8018BE10[];

// shelter_r49
extern GpRoomObjRec    D_shelter_r49_8017DA18[];
extern u8*             D_shelter_r49_8017DA28[];
extern GpViewCountRec  D_shelter_r49_8017DA2C[];
extern GpRoomCoordRec  D_shelter_r49_8017DA30[];
extern GpWarpRec       D_shelter_r49_8017DA38[];
extern GpViewRec       D_shelter_r49_8017DAD0[];
extern GpSprtRec       D_shelter_r49_8017DCA0[];
extern GpRoomParamRec* D_shelter_r49_8017DDF8[];

#endif /* ROOMS_STAGE_TABLES_H */
