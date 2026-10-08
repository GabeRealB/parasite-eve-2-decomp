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

s32 mapDryfieldFullResolveRoomVariant(const RoomEventMsg* request, RoomEventMsg* reply)
{
    if ((request->areaId == GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
        reply->room = gameFlagGetNibble(GAME_FLAG_NIGHT_JUNK_YARD_EVENT_SEEN) + 1;
    }
    return 1;
}

GfxImageSlot D_map_dryfield_full_801799A4[39] = {
    GFX_IMAGE_SLOT(0x3A8E0),
    GFX_IMAGE_SLOT(0x41C20),
    GFX_IMAGE_SLOT(0x4A180),
    GFX_IMAGE_SLOT(0x4DB70),
    { NULL, 0 },
    GFX_IMAGE_SLOT(0x54AE0),
    GFX_IMAGE_SLOT(0x56830),
    GFX_IMAGE_SLOT(0x56250),
    GFX_IMAGE_SLOT(0x54DA0),
    GFX_IMAGE_SLOT(0x4A0B0),
    { NULL, 0 },
    GFX_IMAGE_SLOT(0x553D0),
    GFX_IMAGE_SLOT(0x55240),
    GFX_IMAGE_SLOT(0x54A60),
    GFX_IMAGE_SLOT(0x55300),
    GFX_IMAGE_SLOT(0x53FA0),
    GFX_IMAGE_SLOT(0x566C0),
    GFX_IMAGE_SLOT(0x4CB50),
    GFX_IMAGE_SLOT(0x4A7F0),
    GFX_IMAGE_SLOT(0x57960),
    GFX_IMAGE_SLOT(0x52820),
    GFX_IMAGE_SLOT(0x54720),
    GFX_IMAGE_SLOT(0x554B0),
    GFX_IMAGE_SLOT(0x495E0),
    GFX_IMAGE_SLOT(0x4B2D0),
    GFX_IMAGE_SLOT(0x53740),
    GFX_IMAGE_SLOT(0x50350),
    GFX_IMAGE_SLOT(0x44320),
    GFX_IMAGE_SLOT(0x53F20),
    GFX_IMAGE_SLOT(0x3F550),
    GFX_IMAGE_SLOT(0x4A170),
    GFX_IMAGE_SLOT(0x53970),
    GFX_IMAGE_SLOT(0x52180),
    { NULL, 0 },
    GFX_IMAGE_SLOT(0x54570),
    { NULL, 0 },
    { NULL, 0 },
    { NULL, 0 },
    GFX_IMAGE_SLOT(0x55610),
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
    { &D_mappic_s3_02_8012EFBC, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F04C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_02_8012F0DC, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
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
    { &D_mappic_s3_00_8012F084, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_00_8012F19C, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_00_8012F22C, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_00_8012F2BC, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_03_8012EFD8, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s3_03_8012F084, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
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

static AreaObjectSpawn D_map_dryfield_full_8017A40C[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_dryfield_full_8017A41C[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_dryfield_full_8017A42C[2] = {
    { 0x82, { { { TASK_BODY_TMD, 0x62 } }, Gp_ItemPickupTilt, { &gDryfieldNightTrailerCoachAcropolisSanctuaryModel090F0 } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_dryfield_full_8017A44C[2] = {
    { 0x117, { { { TASK_BODY_TMD, 0x62 } }, areaObjectModelTask, { &gDryfieldNightMotelLoftActor135400Model071AC } } },
    { AREA_OBJECT_SPAWN_END },
};

AreaObjectRoom D_map_dryfield_full_8017A46C[40] = {
    { { NULL }, NULL },
    { { D_80114588 }, D_map_dryfield_full_8017A41C },
    { { D_801145F8 }, D_map_dryfield_full_8017A40C },
    { { D_80114628 }, D_map_dryfield_full_8017A40C },
    { { NULL }, NULL },
    { { D_80114658 }, D_map_dryfield_full_8017A40C },
    { { D_80114678 }, D_map_dryfield_full_8017A40C },
    { { D_80114698 }, D_map_dryfield_full_8017A40C },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_801146A8 }, D_map_dryfield_full_8017A40C },
    { { D_801146D8 }, D_map_dryfield_full_8017A40C },
    { { NULL }, NULL },
    { { D_801146F8 }, D_map_dryfield_full_8017A40C },
    { { NULL }, NULL },
    { { D_80114718 }, D_map_dryfield_full_8017A40C },
    { { D_80114748 }, D_map_dryfield_full_8017A40C },
    { { D_80114768 }, D_map_dryfield_full_8017A40C },
    { { NULL }, NULL },
    { { D_80114798 }, D_map_dryfield_full_8017A40C },
    { { D_80114808 }, D_map_dryfield_full_8017A40C },
    { { D_80114828 }, D_map_dryfield_full_8017A40C },
    { { D_80114848 }, D_map_dryfield_full_8017A40C },
    { { D_80114888 }, D_map_dryfield_full_8017A40C },
    { { D_801148A8 }, D_map_dryfield_full_8017A40C },
    { { D_801148D8 }, D_map_dryfield_full_8017A42C },
    { { D_80114918 }, D_map_dryfield_full_8017A40C },
    { { NULL }, NULL },
    { { D_80114938 }, D_map_dryfield_full_8017A40C },
    { { D_80114968 }, D_map_dryfield_full_8017A44C },
    { { D_80114998 }, D_map_dryfield_full_8017A40C },
    { { NULL }, NULL },
    { { D_801149B8 }, D_map_dryfield_full_8017A40C },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { .sentinel = AREA_OBJECT_ROOM_END }, NULL },
};

TaskDesc D_map_dryfield_full_8017A5AC[] = {
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_gas_station_8017FB70, { .value = GP_TASK_LOC_KEY(3, 1, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_main_street_8017E0C0, { .value = GP_TASK_LOC_KEY(3, 2, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_general_store_8017DE88, { .value = GP_TASK_LOC_KEY(3, 3, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_back_street_8017D788, { .value = GP_TASK_LOC_KEY(3, 5, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightSouvenirShopRoomTask, { .value = GP_TASK_LOC_KEY(3, 6, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightWarehouseRoomTask, { .value = GP_TASK_LOC_KEY(3, 7, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightDilapidatedHouseRoomTask, { .value = GP_TASK_LOC_KEY(3, 9, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelRoom1RoomTask, { .value = GP_TASK_LOC_KEY(3, 11, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelRoom2RoomTask, { .value = GP_TASK_LOC_KEY(3, 12, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelRoom3RoomTask, { .value = GP_TASK_LOC_KEY(3, 13, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelRoom4RoomTask, { .value = GP_TASK_LOC_KEY(3, 14, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_parking_lot_8017DC30, { .value = GP_TASK_LOC_KEY(3, 15, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_toilet_8017D724, { .value = GP_TASK_LOC_KEY(3, 16, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_motel_lobby_8017FE38, { .value = GP_TASK_LOC_KEY(3, 17, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_saloon_g_r_8017E050, { .value = GP_TASK_LOC_KEY(3, 18, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_g_r_kitchen_8017D9A4, { .value = GP_TASK_LOC_KEY(3, 19, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_water_tower_8017DB28, { .value = GP_TASK_LOC_KEY(3, 20, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_water_tank_8017D984, { .value = GP_TASK_LOC_KEY(3, 21, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_breezeway_8017D680, { .value = GP_TASK_LOC_KEY(3, 22, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightFactoryEntryTask, { .value = GP_TASK_LOC_KEY(3, 23, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightGarageRoomTask, { .value = GP_TASK_LOC_KEY(3, 24, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_driveway_8017DD8C, { .value = GP_TASK_LOC_KEY(3, 25, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_junk_yard_8017D960, { .value = GP_TASK_LOC_KEY(3, 26, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_trailer_coach_801828CC, { .value = GP_TASK_LOC_KEY(3, 27, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelRoom5RoomTask, { .value = GP_TASK_LOC_KEY(3, 28, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_motel_balcony_8017DD78, { .value = GP_TASK_LOC_KEY(3, 29, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldNightMotelRoom6RoomTask, { .value = GP_TASK_LOC_KEY(3, 30, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_motel_loft_8017D964, { .value = GP_TASK_LOC_KEY(3, 31, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_water_hole_8017DE30, { .value = GP_TASK_LOC_KEY(3, 32, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_cellar_8017D748, { .value = GP_TASK_LOC_KEY(3, 34, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_underpass_8017D95C, { .value = GP_TASK_LOC_KEY(3, 38, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_night_r08_8017D6C0, { .value = GP_TASK_LOC_KEY(3, 8, 0) } },
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

WorldCoordRoomLighting* D_map_dryfield_full_8017A774[38] = {
    D_dryfield_night_gas_station_80189DB0,
    D_dryfield_night_main_street_801822B4,
    D_dryfield_night_general_store_8017E82C,
    NULL,
    D_dryfield_night_back_street_801803BC,
    D_dryfield_night_souvenir_shop_8017E0E4,
    D_dryfield_night_warehouse_8017E8E8,
    D_dryfield_night_r08_8018067C,
    D_dryfield_night_dilapidated_house_8018738C,
    NULL,
    D_dryfield_night_motel_room_1_8017DA64,
    D_dryfield_night_motel_room_2_8017DA5C,
    D_dryfield_night_motel_room_3_8017DA9C,
    D_dryfield_night_motel_room_4_8017DA90,
    D_dryfield_night_parking_lot_8017EE14,
    D_dryfield_night_toilet_8017DAB0,
    D_dryfield_night_motel_lobby_80182918,
    D_dryfield_night_saloon_g_r_80185180,
    D_dryfield_night_g_r_kitchen_8017E2BC,
    D_dryfield_night_water_tower_8017E74C,
    D_dryfield_night_water_tank_8017EE50,
    D_dryfield_night_breezeway_8017E6E4,
    D_dryfield_night_factory_80186F24,
    D_dryfield_night_garage_801833F4,
    D_dryfield_night_driveway_801805E0,
    D_dryfield_night_junk_yard_80180784,
    D_dryfield_night_trailer_coach_80189500,
    D_dryfield_night_motel_room_5_8017DA70,
    D_dryfield_night_motel_balcony_80182E00,
    D_dryfield_night_motel_room_6_80182F00,
    D_dryfield_night_motel_loft_8017EDB0,
    D_dryfield_night_water_hole_80180A44,
    NULL,
    D_dryfield_night_cellar_8017DAF0,
    NULL,
    NULL,
    NULL,
    D_dryfield_night_underpass_8017DDD0,
};

DirectionWarpEntry* D_map_dryfield_full_8017A80C[38] = {
    D_dryfield_night_gas_station_80189E88,
    D_dryfield_night_main_street_80182310,
    D_dryfield_night_general_store_8017E84C,
    NULL,
    D_dryfield_night_back_street_801803CC,
    D_dryfield_night_souvenir_shop_8017E104,
    D_dryfield_night_warehouse_8017E944,
    D_dryfield_night_r08_8018069C,
    D_dryfield_night_dilapidated_house_801873AC,
    NULL,
    D_dryfield_night_motel_room_1_8017DAA8,
    D_dryfield_night_motel_room_2_8017DA7C,
    D_dryfield_night_motel_room_3_8017DABC,
    D_dryfield_night_motel_room_4_8017DAB0,
    D_dryfield_night_parking_lot_8017EE58,
    D_dryfield_night_toilet_8017DAD0,
    D_dryfield_night_motel_lobby_80182928,
    D_dryfield_night_saloon_g_r_801851BC,
    D_dryfield_night_g_r_kitchen_8017E2DC,
    D_dryfield_night_water_tower_8017E76C,
    D_dryfield_night_water_tank_8017EE7C,
    D_dryfield_night_breezeway_8017E704,
    D_dryfield_night_factory_80186F58,
    D_dryfield_night_garage_80183440,
    D_dryfield_night_driveway_80180628,
    D_dryfield_night_junk_yard_801807CC,
    D_dryfield_night_trailer_coach_80189520,
    D_dryfield_night_motel_room_5_8017DAB8,
    D_dryfield_night_motel_balcony_80182EAC,
    D_dryfield_night_motel_room_6_80182F20,
    D_dryfield_night_motel_loft_8017EDFC,
    D_dryfield_night_water_hole_80180AAC,
    NULL,
    D_dryfield_night_cellar_8017DB34,
    NULL,
    NULL,
    NULL,
    D_dryfield_night_underpass_8017DE60,
};

static ViewCount* D_map_dryfield_full_8017A8A4[38] = {
    D_dryfield_night_gas_station_80189E80,
    D_dryfield_night_main_street_80182308,
    D_dryfield_night_general_store_8017E848,
    NULL,
    D_dryfield_night_back_street_801803C8,
    D_dryfield_night_souvenir_shop_8017E100,
    D_dryfield_night_warehouse_8017E93C,
    D_dryfield_night_r08_80180698,
    D_dryfield_night_dilapidated_house_801873A8,
    NULL,
    D_dryfield_night_motel_room_1_8017DAA4,
    D_dryfield_night_motel_room_2_8017DA78,
    D_dryfield_night_motel_room_3_8017DAB8,
    D_dryfield_night_motel_room_4_8017DAAC,
    D_dryfield_night_parking_lot_8017EE54,
    D_dryfield_night_toilet_8017DACC,
    D_dryfield_night_motel_lobby_80182924,
    D_dryfield_night_saloon_g_r_801851B8,
    D_dryfield_night_g_r_kitchen_8017E2D8,
    D_dryfield_night_water_tower_8017E768,
    D_dryfield_night_water_tank_8017EE78,
    D_dryfield_night_breezeway_8017E700,
    D_dryfield_night_factory_80186F54,
    D_dryfield_night_garage_8018343C,
    D_dryfield_night_driveway_80180624,
    D_dryfield_night_junk_yard_801807C8,
    D_dryfield_night_trailer_coach_8018951C,
    D_dryfield_night_motel_room_5_8017DAB4,
    D_dryfield_night_motel_balcony_80182EA4,
    D_dryfield_night_motel_room_6_80182F1C,
    D_dryfield_night_motel_loft_8017EDF8,
    D_dryfield_night_water_hole_80180AA4,
    NULL,
    D_dryfield_night_cellar_8017DB30,
    NULL,
    NULL,
    NULL,
    D_dryfield_night_underpass_8017DE54,
};

ViewCountTable D_map_dryfield_full_8017A93C = { D_map_dryfield_full_8017A8A4 };

static WorldCollisionRoomResources* D_map_dryfield_full_8017A940[38] = {
    D_dryfield_night_gas_station_80189DD0,
    D_dryfield_night_main_street_80182284,
    D_dryfield_night_general_store_8017E834,
    NULL,
    D_dryfield_night_back_street_801803AC,
    D_dryfield_night_souvenir_shop_8017E0EC,
    D_dryfield_night_warehouse_8017E900,
    D_dryfield_night_r08_80180684,
    D_dryfield_night_dilapidated_house_80187394,
    NULL,
    D_dryfield_night_motel_room_1_8017DA74,
    D_dryfield_night_motel_room_2_8017DA64,
    D_dryfield_night_motel_room_3_8017DAA4,
    D_dryfield_night_motel_room_4_8017DA98,
    D_dryfield_night_parking_lot_8017EE24,
    D_dryfield_night_toilet_8017DAB8,
    D_dryfield_night_motel_lobby_80182908,
    D_dryfield_night_saloon_g_r_80185190,
    D_dryfield_night_g_r_kitchen_8017E2C4,
    D_dryfield_night_water_tower_8017E754,
    D_dryfield_night_water_tank_8017EE58,
    D_dryfield_night_breezeway_8017E6EC,
    D_dryfield_night_factory_80186F34,
    D_dryfield_night_garage_80183404,
    D_dryfield_night_driveway_801805F0,
    D_dryfield_night_junk_yard_80180794,
    D_dryfield_night_trailer_coach_80189508,
    D_dryfield_night_motel_room_5_8017DA80,
    D_dryfield_night_motel_balcony_80182E18,
    D_dryfield_night_motel_room_6_80182F08,
    D_dryfield_night_motel_loft_8017EDC0,
    D_dryfield_night_water_hole_80180A04,
    NULL,
    D_dryfield_night_cellar_8017DB00,
    NULL,
    NULL,
    NULL,
    D_dryfield_night_underpass_8017DD70,
};

WorldCollisionStageResources D_map_dryfield_full_8017A9D8 = { D_map_dryfield_full_8017A940 };

static ViewCamera* D_map_dryfield_full_8017A9DC[38] = {
    D_dryfield_night_gas_station_8018B780,
    D_dryfield_night_main_street_80184564,
    D_dryfield_night_general_store_8017F4A8,
    NULL,
    D_dryfield_night_back_street_80180B58,
    D_dryfield_night_souvenir_shop_8017E628,
    D_dryfield_night_warehouse_8017EF2C,
    D_dryfield_night_r08_80181498,
    D_dryfield_night_dilapidated_house_80187D68,
    NULL,
    D_dryfield_night_motel_room_1_8017E0BC,
    D_dryfield_night_motel_room_2_8017E1A8,
    D_dryfield_night_motel_room_3_8017E1A4,
    D_dryfield_night_motel_room_4_8017E1F4,
    D_dryfield_night_parking_lot_8017FAF4,
    D_dryfield_night_toilet_8017DDAC,
    D_dryfield_night_motel_lobby_80182DD8,
    D_dryfield_night_saloon_g_r_80185B74,
    D_dryfield_night_g_r_kitchen_8017E578,
    D_dryfield_night_water_tower_8017F418,
    D_dryfield_night_water_tank_8017F4D4,
    D_dryfield_night_breezeway_8017EBE8,
    D_dryfield_night_factory_80187C14,
    D_dryfield_night_garage_801843F8,
    D_dryfield_night_driveway_80180C30,
    D_dryfield_night_junk_yard_801811DC,
    D_dryfield_night_trailer_coach_80189A44,
    D_dryfield_night_motel_room_5_8017E060,
    D_dryfield_night_motel_balcony_80184004,
    D_dryfield_night_motel_room_6_801839A8,
    D_dryfield_night_motel_loft_8017F144,
    D_dryfield_night_water_hole_80180F74,
    NULL,
    D_dryfield_night_cellar_8017DE84,
    NULL,
    NULL,
    NULL,
    D_dryfield_night_underpass_8017E6F8,
};

ViewCameraTable D_map_dryfield_full_8017AA74 = { D_map_dryfield_full_8017A9DC };

static u8** D_map_dryfield_full_8017AA78[38] = {
    D_dryfield_night_gas_station_80189E70,
    D_dryfield_night_main_street_801822FC,
    D_dryfield_night_general_store_8017E844,
    NULL,
    D_dryfield_night_back_street_801803C4,
    D_dryfield_night_souvenir_shop_8017E0FC,
    D_dryfield_night_warehouse_8017E930,
    D_dryfield_night_r08_80180694,
    D_dryfield_night_dilapidated_house_801873A4,
    NULL,
    D_dryfield_night_motel_room_1_8017DAA0,
    D_dryfield_night_motel_room_2_8017DA74,
    D_dryfield_night_motel_room_3_8017DAB4,
    D_dryfield_night_motel_room_4_8017DAA8,
    D_dryfield_night_parking_lot_8017EE4C,
    D_dryfield_night_toilet_8017DAC8,
    D_dryfield_night_motel_lobby_80182920,
    D_dryfield_night_saloon_g_r_801851B0,
    D_dryfield_night_g_r_kitchen_8017E2D4,
    D_dryfield_night_water_tower_8017E764,
    D_dryfield_night_water_tank_8017EE74,
    D_dryfield_night_breezeway_8017E6FC,
    D_dryfield_night_factory_80186F1C,
    D_dryfield_night_garage_80183434,
    D_dryfield_night_driveway_8018061C,
    D_dryfield_night_junk_yard_801807C0,
    D_dryfield_night_trailer_coach_80189518,
    D_dryfield_night_motel_room_5_8017DAAC,
    D_dryfield_night_motel_balcony_80182E98,
    D_dryfield_night_motel_room_6_80182F18,
    D_dryfield_night_motel_loft_8017EDF0,
    D_dryfield_night_water_hole_80180A94,
    NULL,
    D_dryfield_night_cellar_8017DB28,
    NULL,
    NULL,
    NULL,
    D_dryfield_night_underpass_8017DE3C,
};

ViewIndexTable D_map_dryfield_full_8017AB10 = { D_map_dryfield_full_8017AA78 };

static SpriteView* D_map_dryfield_full_8017AB14[38] = {
    D_dryfield_night_gas_station_8018F6C4,
    D_dryfield_night_main_street_801875E4,
    D_dryfield_night_general_store_80184278,
    NULL,
    D_dryfield_night_back_street_80180D34,
    D_dryfield_night_souvenir_shop_8017EF08,
    D_dryfield_night_warehouse_8017F46C,
    D_dryfield_night_r08_80181728,
    D_dryfield_night_dilapidated_house_8018921C,
    NULL,
    D_dryfield_night_motel_room_1_8017FE98,
    D_dryfield_night_motel_room_2_80180110,
    D_dryfield_night_motel_room_3_80180118,
    D_dryfield_night_motel_room_4_8017FB88,
    D_dryfield_night_parking_lot_801805AC,
    D_dryfield_night_toilet_8017EC40,
    D_dryfield_night_motel_lobby_80183D1C,
    D_dryfield_night_saloon_g_r_80187FC8,
    D_dryfield_night_g_r_kitchen_8017E6A8,
    D_dryfield_night_water_tower_80182040,
    D_dryfield_night_water_tank_801801CC,
    D_dryfield_night_breezeway_8017FD10,
    D_dryfield_night_factory_80189A24,
    D_dryfield_night_garage_80186258,
    D_dryfield_night_driveway_80181870,
    D_dryfield_night_junk_yard_80183700,
    D_dryfield_night_trailer_coach_8018B64C,
    D_dryfield_night_motel_room_5_80180994,
    D_dryfield_night_motel_balcony_8018D078,
    D_dryfield_night_motel_room_6_801857C0,
    D_dryfield_night_motel_loft_8017FBE4,
    D_dryfield_night_water_hole_80182384,
    NULL,
    D_dryfield_night_cellar_8017FAB8,
    NULL,
    NULL,
    NULL,
    D_dryfield_night_underpass_8017F420,
};

SpriteAreaTable D_map_dryfield_full_8017ABAC = { D_map_dryfield_full_8017AB14 };

WorldCollisionSurfaceProperties** D_map_dryfield_full_8017ABB0[38] = {
    D_dryfield_night_gas_station_80190780,
    D_dryfield_night_main_street_80188B84,
    D_dryfield_night_general_store_80185894,
    NULL,
    D_dryfield_night_back_street_8018161C,
    D_dryfield_night_souvenir_shop_8017F6CC,
    D_dryfield_night_warehouse_8017FC24,
    D_dryfield_night_r08_8018195C,
    D_dryfield_night_dilapidated_house_8018A0E4,
    NULL,
    D_dryfield_night_motel_room_1_80180844,
    D_dryfield_night_motel_room_2_80180A90,
    D_dryfield_night_motel_room_3_80180DC4,
    D_dryfield_night_motel_room_4_8018039C,
    D_dryfield_night_parking_lot_8018153C,
    D_dryfield_night_toilet_8017F3D8,
    D_dryfield_night_motel_lobby_8018448C,
    D_dryfield_night_saloon_g_r_80188F84,
    D_dryfield_night_g_r_kitchen_8017EC04,
    D_dryfield_night_water_tower_80182C30,
    D_dryfield_night_water_tank_80180890,
    D_dryfield_night_breezeway_801804B8,
    D_dryfield_night_factory_8018A79C,
    D_dryfield_night_garage_801875B8,
    D_dryfield_night_driveway_801820F0,
    D_dryfield_night_junk_yard_801844C4,
    D_dryfield_night_trailer_coach_8018C1E8,
    D_dryfield_night_motel_room_5_80181230,
    D_dryfield_night_motel_balcony_8018F2AC,
    D_dryfield_night_motel_room_6_80186250,
    D_dryfield_night_motel_loft_8018090C,
    D_dryfield_night_water_hole_801835F8,
    NULL,
    D_dryfield_night_cellar_801807F4,
    NULL,
    NULL,
    NULL,
    D_dryfield_night_underpass_80180374,
};

AreaPlacement D_map_dryfield_full_8017AC48[13] = {
    { 0x10, 1, 3, 0x4240, 0, -0x10C0, -0x262, 0, 0, 2, 0 },
    { 0x10, 2, 3, 0x3C01, -0x2BC, -0x940, -0x600, 0, 0, 2, 0 },
    { 0x10, 3, 3, 0x3780, -0x498, -0x1280, 0x262, 0, 0, 2, 0 },
    { 0x10, 4, 3, 0x3701, 0, -0x9C0, 0x400, 0, 0, 2, 0 },
    { 0x10, 5, 4, 0x3D40, 0, -0x1520, 0x400, 0, 0, 2, 0 },
    { 0x10, 6, 5, 0x41C0, 0, -0xFA0, 0x400, 0, 0, 2, 0 },
    { 0x10, 7, 6, 0x3D40, 0, -0x1520, 0x400, 0, 0, 2, 0 },
    { 0x10, 8, 7, 0x41C0, 0, -0xFA0, 0x400, 0, 0, 2, 0 },
    { 0x10, 9, 8, 0x3D40, 0, -0x1520, 0x400, 0, 0, 2, 0 },
    { 0x10, 0xA, 9, 0x41C0, 0, -0xFA0, 0x400, 0, 0, 2, 0 },
    { 0x10, 0xB, 0xA, 0x3D40, 0, -0x1520, 0x400, 0, 0, 2, 0 },
    { 0x10, 0xC, 0xB, 0x41C0, 0, -0xFA0, 0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017AD18[3] = {
    { 6, 0, 1, 0x2008, 0, -0x125C, 0xCE4, 0, 0, 2, 0 },
    { 6, 0, 1, 0x3264, 0, -0x640, 0xA8C, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017AD48[3] = {
    { 1, 0, 1, 0x1DB0, 0, -0x1518, 0x190, 0, 0, 2, 0 },
    { 1, 0, 1, 0x32C8, 0, -0x514, 0x9C4, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017AD78[3] = {
    { 0x17, 5, 1, 0x1388, 0, -0x1388, 0x400, 0, 0, 2, 0 },
    { 0x39, 0, 0, 0x189C, 0, -0x6A4, 0x834, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017ADA8[5] = {
    { 0xF, 0, 2, 0x2904, -0xAF0, -0x1770, 0x640, 0, 2, 4, 0 },
    { 0xF, 0, 2, 0x157C, -0x9C4, -0x4B0, 0, 0, 2, 4, 0 },
    { 0xF, 0, 2, 0x1D4C, -0x8FC, -0x4B0, 0, 0, 2, 4, 0 },
    { 6, 0, 1, 0x2C24, 0, -0xC4E, 0xABE, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017ADF8[8] = {
    { 8, 0, 0, -0x1770, -0x960, -0xED8, 0x400, 0, 0, 2, 2 },
    { 8, 0, 0, -0x1644, -0x898, -0xF3C, 0xC80, 0, 0, 2, 2 },
    { 8, 0, 0, -0x1644, -0x9C4, 0x76C, 0xCE4, 0, 0, 2, 2 },
    { 8, 0, 0, -0x898, -0xAF0, 0x25E4, 0x384, 0, 0, 2, 2 },
    { 8, 0, 0, -0x5DC, -0x898, 0x2454, 0xED8, 0, 0, 2, 2 },
    { 8, 0, 0, -0x320, -0xA28, 0x28A0, 0xA8C, 0, 0, 2, 2 },
    { 1, 0, 1, -0x3E8, 0, 0xCE4, 0xC00, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017AE78[2] = {
    { 0x6A, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017AE98[3] = {
    { 3, 0, 0, -0xA28, 0, 0x3E8, 0x800, 0, 0, 2, 0 },
    { 3, 0, 1, -0x6A4, 0, 0x189C, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017AEC8[5] = {
    { 8, 0, 0, 0xA28, -0xB54, 0x6A4, 0xE74, 0, 0, 2, 2 },
    { 8, 0, 0, 0x898, -0xAF0, 0x640, 0x320, 0, 0, 2, 2 },
    { 8, 0, 0, 0x834, -0xB54, 0x1068, 0x4B0, 0, 0, 2, 2 },
    { 8, 0, 0, 0xAF0, -0xAF0, 0x1004, 0xC80, 0, 0, 2, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017AF18[4] = {
    { 0x10, 0, 0, 0x708, -0x3E8, 0xA28, 0x384, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0xA8C, -0x3E8, 0x15E0, 0x800, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x514, 0, 0xBB8, 0x64, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017AF58[9] = {
    { 8, 0, 0, 0xBB8, -0x9C4, 0x1B58, 0, 0, 0, 2, 3 },
    { 8, 0, 0, 0x1B58, -0x9C4, 0x1AF4, 0, 0, 0, 2, 3 },
    { 8, 0, 0, 0x1D4C, -0x960, 0x1BBC, 0, 0, 0, 2, 3 },
    { 8, 0, 0, 0x1C84, -0x8FC, 0x1B58, 0, 0, 0, 2, 3 },
    { 0x19, 0, 0, 0x1C20, 0, 0x258, 0xC00, 0, 2, 4, 1 },
    { 0x19, 0, 0, 0x173E, 0, 0xAF0, 0xC00, 0, 2, 4, 1 },
    { 0x19, 0, 0, 0x19C8, 0, 0x1324, 0xC00, 0, 2, 4, 1 },
    { 0x19, 0, 0, 0x13BA, 0, 0x1BBC, 0x384, 0, 2, 4, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017AFE8[6] = {
    { 7, 0, 0, 0x17D4, 0, 0x44C, 0x1F4, 0, 0, 2, 5 },
    { 7, 0, 0, 0x1A90, 0, 0x384, 0xE10, 0, 0, 2, 5 },
    { 7, 0, 0, 0x1644, 0, 0x4B0, 0x400, 0, 0, 2, 5 },
    { 0x10, 0, 0, 0x12C, 0, 0xDAC, 0x3E8, 0, 2, 4, 0 },
    { 0x10, 0, 0, 0x20D0, 0, 0x898, 0xE10, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B048[7] = {
    { 0x19, 0, 2, -0x1838, 0, 0x1194, 0x400, 0, 0, 2, 1 },
    { 0x19, 0, 2, -0x1F4, 0, 0xF3C, 0x12C, 0, 0, 2, 1 },
    { 0x19, 0, 2, 0x12C0, 0, 0xD48, 0x190, 0, 0, 2, 1 },
    { 0x19, 0, 0, -0x1900, 0, 0x1004, 0xDAC, 0, 0, 2, 1 },
    { 0x19, 0, 0, -0x1F4, 0, 0x1068, 0, 0, 0, 2, 1 },
    { 0x19, 0, 0, 0x12C0, 0, 0x12C0, 0x4B0, 0, 0, 2, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B0B8[7] = {
    { 0xF, 0, 2, -0xBB8, -0x898, 0x1770, 0, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0x7D0, -0x8FC, 0x1770, 0, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0x1964, -0x7D0, 0x1770, 0, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0x20D0, -0x5DC, 0x7D0, 0x800, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0x1C20, -0x6A4, 0x7D0, 0x800, 0, 0, 2, 0 },
    { 1, 0, 1, 0x170C, 1, 0xED8, 0xC00, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B128[8] = {
    { 1, 0, 1, 0x2710, 0, 0xAF0, 0xC80, 0, 0, 2, 0 },
    { 1, 0, 1, 0xAF0, 0, 0xED8, 0xD48, 0, 0, 2, 0 },
    { 8, 0, 0, -0x2580, -0x960, 0x13EC, 0xA8C, 0, 2, 4, 2 },
    { 8, 0, 0, -0x25E4, -0xA28, 0x1194, 0xC00, 0, 2, 4, 2 },
    { 8, 0, 0, -0x2580, -0x9C4, 0xED8, 0xD48, 0, 2, 4, 2 },
    { 8, 0, 0, -0x2BC, -0x7D0, 0x16A8, 0xF3C, 0, 2, 4, 2 },
    { 8, 0, 0, -0x258, -0x708, 0x1644, 0, 0, 2, 4, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B1A8[2] = {
    { 0x39, 0xC, 1, 0x1770, 0, 0x12C0, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B1C8[15] = {
    { 0x25, 0, 2, 0x7D0, -0x9C4, 0x1770, 0, 0, 0, 2, 6 },
    { 0x25, 0, 2, 0x834, -0x7D0, 0x1770, 0, 0, 0, 2, 6 },
    { 0x25, 0, 2, 0x9C4, -0xA8C, 0x1770, 0, 0, 0, 2, 6 },
    { 0x25, 0, 2, 0xB54, -0x960, 0x1770, 0, 0, 0, 2, 6 },
    { 0x25, 0, 2, 0xDAC, -0x9C4, 0x1770, 0, 0, 0, 2, 6 },
    { 0x25, 0, 2, 0xF3C, -0xA8C, 0x1770, 0, 0, 0, 2, 6 },
    { 0x25, 0, 2, 0x1964, -0x708, 0x7D0, 0x800, 0, 0, 2, 6 },
    { 0x25, 0, 2, 0x1AF4, -0x578, 0x7D0, 0x800, 0, 0, 2, 6 },
    { 0x25, 0, 2, 0x1C20, -0x640, 0x7D0, 0x800, 0, 0, 2, 6 },
    { 0x25, 0, 2, 0x1CE8, -0x4B0, 0x7D0, 0x800, 0, 0, 2, 6 },
    { 0x25, 0, 2, 0x1DB0, -0x6A4, 0x7D0, 0x800, 0, 0, 2, 6 },
    { 0x25, 0, 2, 0x1E78, -0x4B0, 0x7D0, 0x800, 0, 0, 2, 6 },
    { 0x25, 0, 2, 0x1FA4, -0x5DC, 0x7D0, 0x800, 0, 0, 2, 6 },
    { 0x25, 0, 2, 0x20D0, -0x6A4, 0x7D0, 0x800, 0, 0, 2, 6 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B2B8[5] = {
    { 8, 0, 0, 0x1324, -0x320, -0xED8, 0x960, 0, 0, 2, 2 },
    { 8, 0, 0, 0x11F8, -0x4B0, -0xE10, 0x800, 0, 0, 2, 2 },
    { 8, 0, 0, 0x1004, -0x1F4, -0xE74, 0x6A4, 0, 0, 2, 2 },
    { 8, 0, 0, 0xED8, -0x640, -0xDAC, 0x578, 0, 0, 2, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B308[5] = {
    { 0x10, 0, 0, 0x15E0, 0, -0xC1C, 0x578, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x4B0, 0, -0x320, 0xDAC, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x1450, 0, -0x4E2, 0x708, 0, 0, 2, 0 },
    { 0x10, 0, 0, 0x13EC, 0, -0x190, 0x2BC, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B358[11] = {
    { 8, 0, 0, 0x4B0, -0x7D0, -0x1F4, 0, 0, 0, 2, 3 },
    { 8, 0, 0, 0x578, -0x898, -0x1F4, 0, 0, 0, 2, 3 },
    { 8, 0, 0, 0x514, -0x708, -0x1F4, 0, 0, 0, 2, 3 },
    { 8, 0, 0, 0x1324, -0x320, -0xED8, 0x960, 0, 0, 2, 3 },
    { 8, 0, 0, 0x11F8, -0x4B0, -0xE10, 0x800, 0, 0, 2, 3 },
    { 8, 0, 0, 0x1004, -0x1F4, -0xE74, 0x6A4, 0, 0, 2, 3 },
    { 8, 0, 0, 0xED8, -0x640, -0xDAC, 0x578, 0, 0, 2, 3 },
    { 0x19, 0, 0, 0x141E, 0, -0x190, 0xAF0, 0, 2, 4, 3 },
    { 0x19, 0, 0, 0x1518, 0, -0xC4E, 0xC00, 0, 2, 4, 2 },
    { 0x19, 0, 0, 0x4B0, 0, -0x320, 0x47E, 0, 2, 4, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B408[3] = {
    { 0x19, 0, 0, 0x6A4, 0, -0x4B0, 0x5DC, 0, 0, 2, 1 },
    { 0x19, 0, 0, 0x11F8, 0, -0xC80, 0xDAC, 0, 0, 2, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_full_8017B438[7] = {
    { 0x28, 1, 0, 0x5DC, 0, -0x190, 0xFA0, 0, 0, 2, 0 },
    { 0x28, 1, 0, 0x190, 0, -0xB22, 0xBB8, 0, 0, 2, 0 },
    { 0x28, 1, 0, 0xA8C, 0, -0x47E, 0xF3C, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0xC80, 0, -0x514, 0xAF0, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0x1194, 0, -0x6A4, 0xC00, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0x1388, 0, -0x9C4, 0x898, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
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
