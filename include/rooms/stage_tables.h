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
extern GpRoomObjRec    D_dryfield_r04_8017D5C4[];
extern u8*             D_dryfield_r04_8017D5D4[];
extern GpViewCountRec  D_dryfield_r04_8017D5D8[];
extern GpWarpRec       D_dryfield_r04_8017D5E4[];
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

#endif /* ROOMS_STAGE_TABLES_H */
