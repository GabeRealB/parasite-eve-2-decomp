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

GfxImageSlot D_map_dryfield_80179A14[39] = {
    GFX_IMAGE_SLOT(0x3A8E0),
    GFX_IMAGE_SLOT(0x4EE30),
    GFX_IMAGE_SLOT(0x4F100),
    GFX_IMAGE_SLOT(0x4DE10),
    GFX_IMAGE_SLOT(0x585E0),
    GFX_IMAGE_SLOT(0x54F10),
    GFX_IMAGE_SLOT(0x56780),
    GFX_IMAGE_SLOT(0x53770),
    GFX_IMAGE_SLOT(0x54E70),
    GFX_IMAGE_SLOT(0x49580),
    { NULL, 0 },
    GFX_IMAGE_SLOT(0x543A0),
    GFX_IMAGE_SLOT(0x558B0),
    GFX_IMAGE_SLOT(0x58030),
    GFX_IMAGE_SLOT(0x57D70),
    GFX_IMAGE_SLOT(0x560A0),
    GFX_IMAGE_SLOT(0x4B730),
    GFX_IMAGE_SLOT(0x545C0),
    GFX_IMAGE_SLOT(0x523E0),
    GFX_IMAGE_SLOT(0x57030),
    GFX_IMAGE_SLOT(0x4C970),
    GFX_IMAGE_SLOT(0x4BD80),
    GFX_IMAGE_SLOT(0x514F0),
    GFX_IMAGE_SLOT(0x495E0),
    GFX_IMAGE_SLOT(0x53ED0),
    GFX_IMAGE_SLOT(0x551F0),
    GFX_IMAGE_SLOT(0x532A0),
    GFX_IMAGE_SLOT(0x49210),
    GFX_IMAGE_SLOT(0x57F20),
    GFX_IMAGE_SLOT(0x4D6F0),
    GFX_IMAGE_SLOT(0x4C850),
    GFX_IMAGE_SLOT(0x56A60),
    GFX_IMAGE_SLOT(0x52EE0),
    { NULL, 0 },
    GFX_IMAGE_SLOT(0x541F0),
    { NULL, 0 },
    { NULL, 0 },
    { NULL, 0 },
    GFX_IMAGE_SLOT(0x54820),
};

s32 D_map_dryfield_80179B4C[32] = {
    0,
    0x2A00,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
    { &D_mappic_s2_02_8012EFBC, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F04C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F0DC, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F188, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012F218, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
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
    { &D_mappic_s2_00_8012F2BC, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012FA7C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012FB0C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012FB9C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012FC64, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012FD60, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_02_8012FDF0, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_00_8012F1D8, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_00_8012F0B8, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_00_8012EFA0, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_00_8012F148, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_03_8012EFD8, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s2_03_8012F084, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
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

static AreaObjectSpawn D_map_dryfield_8017A504[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_dryfield_8017A514[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_dryfield_8017A524[2] = {
    { 0x114, { { { TASK_BODY_TMD, 0x62 } }, func_dryfield_junk_yard_8017D5F4, { &gDryfieldJunkYardModel01378 } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_dryfield_8017A544[2] = {
    { 0x82, { { { TASK_BODY_TMD, 0x62 } }, Gp_ItemPickupTilt, { &gDryfieldTrailerCoachAcropolisSanctuaryModel090F0 } } },
    { AREA_OBJECT_SPAWN_END },
};

AreaObjectRoom D_map_dryfield_8017A564[40] = {
    { { NULL }, NULL },
    { { D_80114588 }, D_map_dryfield_8017A514 },
    { { D_801145F8 }, D_map_dryfield_8017A504 },
    { { D_80114628 }, D_map_dryfield_8017A504 },
    { { NULL }, NULL },
    { { D_80114658 }, D_map_dryfield_8017A504 },
    { { D_80114678 }, D_map_dryfield_8017A504 },
    { { D_80114698 }, D_map_dryfield_8017A504 },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_801146A8 }, D_map_dryfield_8017A504 },
    { { D_801146D8 }, D_map_dryfield_8017A504 },
    { { NULL }, NULL },
    { { D_801146F8 }, D_map_dryfield_8017A504 },
    { { NULL }, NULL },
    { { D_80114718 }, D_map_dryfield_8017A504 },
    { { D_80114748 }, D_map_dryfield_8017A504 },
    { { D_80114768 }, D_map_dryfield_8017A504 },
    { { NULL }, NULL },
    { { D_80114798 }, D_map_dryfield_8017A504 },
    { { D_80114808 }, D_map_dryfield_8017A504 },
    { { D_80114828 }, D_map_dryfield_8017A504 },
    { { D_80114848 }, D_map_dryfield_8017A504 },
    { { D_80114888 }, D_map_dryfield_8017A504 },
    { { D_801148A8 }, D_map_dryfield_8017A524 },
    { { D_801148D8 }, D_map_dryfield_8017A544 },
    { { D_80114918 }, D_map_dryfield_8017A504 },
    { { NULL }, NULL },
    { { D_80114938 }, D_map_dryfield_8017A504 },
    { { D_80114968 }, D_map_dryfield_8017A504 },
    { { D_80114998 }, D_map_dryfield_8017A504 },
    { { NULL }, NULL },
    { { D_801149B8 }, D_map_dryfield_8017A504 },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { .sentinel = AREA_OBJECT_ROOM_END }, NULL },
};

TaskDesc D_map_dryfield_8017A6A4[] = {
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_gas_station_8017FF8C, { .value = GP_TASK_LOC_KEY(2, 1, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMainStreetTask, { .value = GP_TASK_LOC_KEY(2, 2, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_general_store_8017DF5C, { .value = GP_TASK_LOC_KEY(2, 3, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_back_street_8017D918, { .value = GP_TASK_LOC_KEY(2, 5, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldSouvenirShopRoomTask, { .value = GP_TASK_LOC_KEY(2, 6, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_warehouse_8017DA00, { .value = GP_TASK_LOC_KEY(2, 7, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldDilapidatedHouseRoomTask, { .value = GP_TASK_LOC_KEY(2, 9, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_motel_room_1_8017D754, { .value = GP_TASK_LOC_KEY(2, 11, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelRoom2Task, { .value = GP_TASK_LOC_KEY(2, 12, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelRoom3Task, { .value = GP_TASK_LOC_KEY(2, 13, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelRoom4Task, { .value = GP_TASK_LOC_KEY(2, 14, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_parking_lot_8017DB54, { .value = GP_TASK_LOC_KEY(2, 15, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldToiletRoomTask, { .value = GP_TASK_LOC_KEY(2, 16, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelLobbyTask, { .value = GP_TASK_LOC_KEY(2, 17, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_saloon_g_r_8017DA18, { .value = GP_TASK_LOC_KEY(2, 18, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_g_r_kitchen_8017D9A4, { .value = GP_TASK_LOC_KEY(2, 19, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_water_tower_8017DDD8, { .value = GP_TASK_LOC_KEY(2, 20, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldWaterTankRoomTask, { .value = GP_TASK_LOC_KEY(2, 21, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldBreezewayMessageTask, { .value = GP_TASK_LOC_KEY(2, 22, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldFactoryEntryTask, { .value = GP_TASK_LOC_KEY(2, 23, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_garage_8017DC10, { .value = GP_TASK_LOC_KEY(2, 24, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldDrivewayRoomTask, { .value = GP_TASK_LOC_KEY(2, 25, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_junk_yard_8017DCB4, { .value = GP_TASK_LOC_KEY(2, 26, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_trailer_coach_80182950, { .value = GP_TASK_LOC_KEY(2, 27, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelRoom5Task, { .value = GP_TASK_LOC_KEY(2, 28, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_motel_balcony_8017DBD0, { .value = GP_TASK_LOC_KEY(2, 29, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_motel_room_6_80181B18, { .value = GP_TASK_LOC_KEY(2, 30, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, dryfieldMotelLoftTask, { .value = GP_TASK_LOC_KEY(2, 31, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_water_hole_8017D840, { .value = GP_TASK_LOC_KEY(2, 32, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_cellar_8017D784, { .value = GP_TASK_LOC_KEY(2, 34, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_dryfield_underpass_8017DAC8, { .value = GP_TASK_LOC_KEY(2, 38, 0) } },
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

WorldCoordRoomLighting* D_map_dryfield_8017A860[38] = {
    D_dryfield_gas_station_80183160,
    D_dryfield_main_street_80181BD4,
    D_dryfield_general_store_8017E688,
    NULL,
    D_dryfield_back_street_8017F9CC,
    D_dryfield_souvenir_shop_8017E0D4,
    D_dryfield_warehouse_8017FC18,
    D_dryfield_r08_8017F708,
    D_dryfield_dilapidated_house_8018696C,
    NULL,
    D_dryfield_motel_room_1_8017E4BC,
    D_dryfield_motel_room_2_8017D6FC,
    D_dryfield_motel_room_3_8017D6F4,
    D_dryfield_motel_room_4_8017D6F4,
    D_dryfield_parking_lot_8017DC5C,
    D_dryfield_toilet_80181144,
    D_dryfield_motel_lobby_8017F850,
    D_dryfield_saloon_g_r_8017EDDC,
    D_dryfield_g_r_kitchen_8017EC40,
    D_dryfield_water_tower_801827E4,
    D_dryfield_water_tank_801868F8,
    D_dryfield_breezeway_80183180,
    D_dryfield_factory_80186F4C,
    D_dryfield_garage_8017DCF4,
    D_dryfield_driveway_8017E7B4,
    D_dryfield_junk_yard_8017ED14,
    D_dryfield_trailer_coach_801871DC,
    D_dryfield_motel_room_5_8017D6F4,
    D_dryfield_motel_balcony_801822E8,
    D_dryfield_motel_room_6_80182DB0,
    D_dryfield_motel_loft_8017D6F4,
    D_dryfield_water_hole_8017FD9C,
    NULL,
    D_dryfield_cellar_8017DC10,
    NULL,
    NULL,
    NULL,
    D_dryfield_underpass_8017EBE0,
};

DirectionWarpEntry* D_map_dryfield_8017A8F8[38] = {
    D_dryfield_gas_station_8018316C,
    D_dryfield_main_street_80181BDC,
    D_dryfield_general_store_8017E690,
    D_dryfield_r04_8017D5E4,
    D_dryfield_back_street_8017F9D4,
    D_dryfield_souvenir_shop_8017E0DC,
    D_dryfield_warehouse_8017FC30,
    D_dryfield_r08_8017F718,
    D_dryfield_dilapidated_house_80186974,
    NULL,
    D_dryfield_motel_room_1_8017E4CC,
    D_dryfield_motel_room_2_8017D704,
    D_dryfield_motel_room_3_8017D6FC,
    D_dryfield_motel_room_4_8017D6FC,
    D_dryfield_parking_lot_8017DC64,
    D_dryfield_toilet_8018114C,
    D_dryfield_motel_lobby_8017F858,
    D_dryfield_saloon_g_r_8017EDEC,
    D_dryfield_g_r_kitchen_8017EC48,
    D_dryfield_water_tower_801827EC,
    D_dryfield_water_tank_80186900,
    D_dryfield_breezeway_8018318C,
    D_dryfield_factory_80186F60,
    D_dryfield_garage_8017DCFC,
    D_dryfield_driveway_8017E7C8,
    D_dryfield_junk_yard_8017ED24,
    D_dryfield_trailer_coach_801871EC,
    D_dryfield_motel_room_5_8017D6FC,
    D_dryfield_motel_balcony_801822F0,
    D_dryfield_motel_room_6_80182DB8,
    D_dryfield_motel_loft_8017D6FC,
    D_dryfield_water_hole_8017FDBC,
    NULL,
    D_dryfield_cellar_8017DC20,
    NULL,
    NULL,
    NULL,
    D_dryfield_underpass_8017EC10,
};

static ViewCount* D_map_dryfield_8017A990[38] = {
    D_dryfield_gas_station_80183168,
    D_dryfield_main_street_80181BD0,
    D_dryfield_general_store_8017E684,
    D_dryfield_r04_8017D5D8,
    D_dryfield_back_street_8017F9C8,
    D_dryfield_souvenir_shop_8017E0D0,
    D_dryfield_warehouse_8017FC10,
    D_dryfield_r08_8017F704,
    D_dryfield_dilapidated_house_80186968,
    NULL,
    D_dryfield_motel_room_1_8017E4B8,
    D_dryfield_motel_room_2_8017D6F8,
    D_dryfield_motel_room_3_8017D6F0,
    D_dryfield_motel_room_4_8017D6F0,
    D_dryfield_parking_lot_8017DC58,
    D_dryfield_toilet_80181140,
    D_dryfield_motel_lobby_8017F84C,
    D_dryfield_saloon_g_r_8017EDD8,
    D_dryfield_g_r_kitchen_8017EC3C,
    D_dryfield_water_tower_801827E0,
    D_dryfield_water_tank_801868F4,
    D_dryfield_breezeway_80183188,
    D_dryfield_factory_80186F5C,
    D_dryfield_garage_8017DCF0,
    D_dryfield_driveway_8017E7C4,
    D_dryfield_junk_yard_8017ED20,
    D_dryfield_trailer_coach_801871E8,
    D_dryfield_motel_room_5_8017D6F0,
    D_dryfield_motel_balcony_801822E4,
    D_dryfield_motel_room_6_80182DAC,
    D_dryfield_motel_loft_8017D6F0,
    D_dryfield_water_hole_8017FD94,
    NULL,
    D_dryfield_cellar_8017DC0C,
    NULL,
    NULL,
    NULL,
    D_dryfield_underpass_8017EBD4,
};

ViewCountTable D_map_dryfield_8017AA28 = { D_map_dryfield_8017A990 };

static WorldCollisionRoomResources* D_map_dryfield_8017AA2C[38] = {
    D_dryfield_gas_station_8018314C,
    D_dryfield_main_street_80181BBC,
    D_dryfield_general_store_8017E670,
    D_dryfield_r04_8017D5C4,
    D_dryfield_back_street_8017F9B4,
    D_dryfield_souvenir_shop_8017E0BC,
    D_dryfield_warehouse_8017FBBC,
    D_dryfield_r08_8017F6DC,
    D_dryfield_dilapidated_house_80186954,
    NULL,
    D_dryfield_motel_room_1_8017E484,
    D_dryfield_motel_room_2_8017D6E8,
    D_dryfield_motel_room_3_8017D6DC,
    D_dryfield_motel_room_4_8017D6DC,
    D_dryfield_parking_lot_8017DC44,
    D_dryfield_toilet_8018112C,
    D_dryfield_motel_lobby_8017F838,
    D_dryfield_saloon_g_r_8017EDA0,
    D_dryfield_g_r_kitchen_8017EC28,
    D_dryfield_water_tower_801827CC,
    D_dryfield_water_tank_801868E0,
    D_dryfield_breezeway_8018316C,
    D_dryfield_factory_80186F10,
    D_dryfield_garage_8017DCDC,
    D_dryfield_driveway_8017E784,
    D_dryfield_junk_yard_8017ED04,
    D_dryfield_trailer_coach_801871CC,
    D_dryfield_motel_room_5_8017D6DC,
    D_dryfield_motel_balcony_801822D0,
    D_dryfield_motel_room_6_80182D98,
    D_dryfield_motel_loft_8017D6DC,
    D_dryfield_water_hole_8017FD2C,
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
    D_dryfield_main_street_80182CC0,
    D_dryfield_general_store_8017F25C,
    D_dryfield_r04_8017E218,
    D_dryfield_back_street_801802A8,
    D_dryfield_souvenir_shop_8017E600,
    D_dryfield_warehouse_8018105C,
    D_dryfield_r08_8017FBBC,
    D_dryfield_dilapidated_house_80187308,
    NULL,
    D_dryfield_motel_room_1_8017EAE0,
    D_dryfield_motel_room_2_8017DE30,
    D_dryfield_motel_room_3_8017DDC4,
    D_dryfield_motel_room_4_8017DE14,
    D_dryfield_parking_lot_8017E900,
    D_dryfield_toilet_80181428,
    D_dryfield_motel_lobby_8017FC08,
    D_dryfield_saloon_g_r_8017F7A4,
    D_dryfield_g_r_kitchen_8017EEE4,
    D_dryfield_water_tower_801835E8,
    D_dryfield_water_tank_80186EE0,
    D_dryfield_breezeway_8018364C,
    D_dryfield_factory_80187C1C,
    D_dryfield_garage_8017E670,
    D_dryfield_driveway_8017EE2C,
    D_dryfield_junk_yard_8017F5C0,
    D_dryfield_trailer_coach_80187758,
    D_dryfield_motel_room_5_8017DCC0,
    D_dryfield_motel_balcony_80182B80,
    D_dryfield_motel_room_6_80183840,
    D_dryfield_motel_loft_8017D9E0,
    D_dryfield_water_hole_80180284,
    NULL,
    D_dryfield_cellar_8017DF78,
    NULL,
    NULL,
    NULL,
    D_dryfield_underpass_8017F4A8,
};

ViewCameraTable D_map_dryfield_8017AB60 = { D_map_dryfield_8017AAC8 };

static u8** D_map_dryfield_8017AB64[38] = {
    D_dryfield_gas_station_8018315C,
    D_dryfield_main_street_80181BCC,
    D_dryfield_general_store_8017E680,
    D_dryfield_r04_8017D5D4,
    D_dryfield_back_street_8017F9C4,
    D_dryfield_souvenir_shop_8017E0CC,
    D_dryfield_warehouse_8017FC04,
    D_dryfield_r08_8017F6FC,
    D_dryfield_dilapidated_house_80186964,
    NULL,
    D_dryfield_motel_room_1_8017E4B0,
    D_dryfield_motel_room_2_8017D6E4,
    D_dryfield_motel_room_3_8017D6EC,
    D_dryfield_motel_room_4_8017D6EC,
    D_dryfield_parking_lot_8017DC54,
    D_dryfield_toilet_8018113C,
    D_dryfield_motel_lobby_8017F848,
    D_dryfield_saloon_g_r_8017EDD0,
    D_dryfield_g_r_kitchen_8017EC38,
    D_dryfield_water_tower_801827DC,
    D_dryfield_water_tank_801868F0,
    D_dryfield_breezeway_8018317C,
    D_dryfield_factory_80186F44,
    D_dryfield_garage_8017DCEC,
    D_dryfield_driveway_8017E7AC,
    D_dryfield_junk_yard_8017ED1C,
    D_dryfield_trailer_coach_801871E4,
    D_dryfield_motel_room_5_8017D6EC,
    D_dryfield_motel_balcony_801822E0,
    D_dryfield_motel_room_6_80182DA8,
    D_dryfield_motel_loft_8017D6EC,
    D_dryfield_water_hole_8017FD84,
    NULL,
    D_dryfield_cellar_8017DC04,
    NULL,
    NULL,
    NULL,
    D_dryfield_underpass_8017EBBC,
};

ViewIndexTable D_map_dryfield_8017ABFC = { D_map_dryfield_8017AB64 };

static SpriteView* D_map_dryfield_8017AC00[38] = {
    D_dryfield_gas_station_801842A8,
    D_dryfield_main_street_80184308,
    D_dryfield_general_store_8018402C,
    D_dryfield_r04_8017E280,
    D_dryfield_back_street_80180484,
    D_dryfield_souvenir_shop_8017EED0,
    D_dryfield_warehouse_80181638,
    D_dryfield_r08_80180918,
    D_dryfield_dilapidated_house_80188C0C,
    NULL,
    D_dryfield_motel_room_1_80180C90,
    D_dryfield_motel_room_2_8017FCD0,
    D_dryfield_motel_room_3_8017DF64,
    D_dryfield_motel_room_4_8017DF4C,
    D_dryfield_parking_lot_8017F054,
    D_dryfield_toilet_801821F8,
    D_dryfield_motel_lobby_80180AB0,
    D_dryfield_saloon_g_r_80180E2C,
    D_dryfield_g_r_kitchen_8017F014,
    D_dryfield_water_tower_80186560,
    D_dryfield_water_tank_80187F80,
    D_dryfield_breezeway_80183D9C,
    D_dryfield_factory_801895B0,
    D_dryfield_garage_8017F5E8,
    D_dryfield_driveway_8017FC44,
    D_dryfield_junk_yard_80180C28,
    D_dryfield_trailer_coach_801891D0,
    D_dryfield_motel_room_5_8017DDF8,
    D_dryfield_motel_balcony_80185C98,
    D_dryfield_motel_room_6_801856CC,
    D_dryfield_motel_loft_8017DDD8,
    D_dryfield_water_hole_80181634,
    NULL,
    D_dryfield_cellar_8017FE40,
    NULL,
    NULL,
    NULL,
    D_dryfield_underpass_80180250,
};

SpriteAreaTable D_map_dryfield_8017AC98 = { D_map_dryfield_8017AC00 };

WorldCollisionSurfaceProperties** D_map_dryfield_8017AC9C[38] = {
    D_dryfield_gas_station_80184BAC,
    D_dryfield_main_street_801855EC,
    D_dryfield_general_store_801856D8,
    D_dryfield_r04_8017E2AC,
    D_dryfield_back_street_80181034,
    D_dryfield_souvenir_shop_8017F640,
    D_dryfield_warehouse_80182194,
    D_dryfield_r08_80180C04,
    D_dryfield_dilapidated_house_80189A80,
    NULL,
    D_dryfield_motel_room_1_8018157C,
    D_dryfield_motel_room_2_801804B0,
    D_dryfield_motel_room_3_8017E524,
    D_dryfield_motel_room_4_8017E470,
    D_dryfield_parking_lot_8017FB30,
    D_dryfield_toilet_8018660C,
    D_dryfield_motel_lobby_80181044,
    D_dryfield_saloon_g_r_80181BBC,
    D_dryfield_g_r_kitchen_8017F53C,
    D_dryfield_water_tower_80187608,
    D_dryfield_water_tank_80188CFC,
    D_dryfield_breezeway_8018437C,
    D_dryfield_factory_8018A37C,
    D_dryfield_garage_801801E4,
    D_dryfield_driveway_80180660,
    D_dryfield_junk_yard_80181C28,
    D_dryfield_trailer_coach_80189C30,
    D_dryfield_motel_room_5_8017E5BC,
    D_dryfield_motel_balcony_80186704,
    D_dryfield_motel_room_6_80186808,
    D_dryfield_motel_loft_8017E648,
    D_dryfield_water_hole_801828AC,
    NULL,
    D_dryfield_cellar_80180B40,
    NULL,
    NULL,
    NULL,
    D_dryfield_underpass_80181164,
};

AreaPlacement D_map_dryfield_8017AD34[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017AD44[3] = {
    { 0x65, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 1, 0, 0, -0x68, 0, 0x11F8, -0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017AD74[7] = {
    { 0x19, 0, 1, -0xDAC, 0, 0x1194, 0x1F4, 0, 0, 2, 0 },
    { 0x19, 0, 1, -0x5DC, 0, 0x12C0, 0xE74, 0, 0, 2, 0 },
    { 0x19, 0, 2, -0x7D0, 0, 0x1D4C, 0x898, 0, 0, 2, 0 },
    { 0x19, 0, 2, -0xDAC, 0, 0x1964, 0x5DC, 0, 0, 2, 0 },
    { 0x19, 0, 2, -0x6A4, 0, 0x12C, 0xE74, 0, 0, 2, 0 },
    { 0x19, 0, 2, -0xC80, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017ADE4[3] = {
    { 1, 2, 0, -0x1770, 0, 0x1AF4, 0x76C, 0, 0, 2, 0 },
    { 1, 2, 0, -0x157C, 0, -0xF3C, 0xFD2, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017AE14[3] = {
    { 0x14, 6, 1, -0x898, 0, 0xFA0, 0x800, 0, 0, 2, 0 },
    { 0x17, 4, 1, -0x1838, 0, 0xFA0, 0, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017AE44[6] = {
    { 0x28, 0, 0, 0xD8E, 0, 0xE16, 0x400, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0x418, 0, 0x1373, -0x400, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0xD8C, 0, 0x12A4, 0, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0x4D2, 0, 0xE39, 0x80, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0xC31, 0, 0x74D, -0x80, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017AEA4[3] = {
    { 1, 2, 0, 0x2710, 2, 0xBB8, 0x258, 0, 2, 4, 0 },
    { 1, 2, 0, -0x1F4, 2, 0x1004, 0x4B0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017AED4[3] = {
    { 0x14, 6, 1, -0x5DC, 0, 0x1388, 0x400, 0, 0, 2, 0 },
    { 0x38, 0, 0, 0x25E4, 0, 0x1644, 0x800, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017AF04[4] = {
    { 0xF, 0, 2, 0x4B0, -0x7D0, 0, 0, 0, 0, 2, 0 },
    { 0x28, 1, 0, 0x157C, 1, -0xAF0, 0xAF0, 0, 2, 4, 0 },
    { 0x28, 1, 0, 0xBB8, 1, -0x2BC, -0x320, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017AF44[2] = {
    { 0x84, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017AF64[3] = {
    { 0x22, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0x1D, 0, 0, -0x7D0, 0, -0x708, 0, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017AF94[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017AFA4[6] = {
    { 0xC, 1, 0, 0xA28, 1, 0xAF0, 0x708, 0, 0, 2, 0 },
    { 0xC, 1, 0, 0x5DC, 1, 0xC80, 0xED8, 0, 0, 2, 0 },
    { 0xC, 0, 0, 0x1F4, 1, 0xD48, 0x400, 0, 0, 2, 0 },
    { 0xC, 1, 0, 0x3E8, 1, 0x15E0, 0x17C, 0, 0, 2, 0 },
    { 0x12, 0x10, 0, 0x708, 1, 0xBB8, 0x800, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B004[5] = {
    { 0x70, 0, 0, -0xD91, 0, 0x3E1, 0x1000, 0, 0, 2, 0 },
    { 0x70, 0, 0, -0xD91, 0, 0x3E1, 0x1000, 0, 0, 2, 0 },
    { 0xC, 0, 0, 0x6B8, 0, 0x8FA, 0x100, 0, 0, 2, 0 },
    { 0xC, 0, 0, 0x905, 0, 0xD8B, -0x3E8, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B054[6] = {
    { 0x28, 1, 0, 0xA28, 1, 0xAF0, 0x708, 0, 0, 2, 0 },
    { 0x28, 1, 0, 0x5DC, 1, 0xC80, 0xED8, 0, 0, 2, 0 },
    { 0x28, 0, 0, 0x1F4, 1, 0xD48, 0x400, 0, 0, 2, 0 },
    { 0x28, 1, 0, 0x3E8, 1, 0x15E0, 0x17C, 0, 0, 2, 0 },
    { 0x12, 0x10, 1, 0x708, 1, 0xBB8, 0x800, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B0B4[3] = {
    { 0x12, 0x10, 0, 0x960, 1, 0xFA0, 0, 0, 2, 4, 0 },
    { 0x12, 0x10, 0, 0x1194, 1, 0x1388, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B0E4[7] = {
    { 0xF, 0, 3, 0x9C4, -0xA28, 0x5DC, 0x190, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0xC8, -0x708, 0x834, 0xC00, 0, 0, 2, 0 },
    { 0xC, 1, 0, 0xBB8, 1, 0x1324, 0xA28, 0, 2, 4, 0 },
    { 0xC, 1, 0, 0x1194, 1, 0x14B4, 0xA8C, 0, 2, 4, 0 },
    { 0xC, 1, 0, 0x1F4, 1, 0x1388, 0xE74, 0, 2, 4, 0 },
    { 0xC, 1, 0, 0x1F4, 1, 0xB54, 0x320, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B154[7] = {
    { 0xF, 0, 3, 0x9C4, -0xA28, 0x5DC, 0x190, 0, 0, 2, 0 },
    { 0xF, 0, 2, 0xC8, -0x708, 0x834, 0xC00, 0, 0, 2, 0 },
    { 0x28, 1, 0, 0xBB8, 1, 0x1324, 0xA28, 0, 2, 4, 0 },
    { 0x28, 1, 0, 0x1194, 1, 0x14B4, 0xA8C, 0, 2, 4, 0 },
    { 0x28, 1, 0, 0x1F4, 1, 0x1388, 0xE74, 0, 2, 4, 0 },
    { 0x28, 1, 0, 0x1F4, 1, 0xB54, 0x320, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B1C4[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B1D4[3] = {
    { 1, 2, 2, 0x549, 0, 0x320, 0xF00, 0, 0, 2, 0 },
    { 1, 2, 2, 0x157C, 0, 0xDAC, 0xC80, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B204[8] = {
    { 0x19, 0, 2, -0x8FC, 0, 0x44C, 0x1F4, 0, 2, 4, 0 },
    { 0x19, 0, 2, 0xC1C, 0, 0x4B0, 0xE10, 0, 2, 4, 0 },
    { 0x19, 0, 2, -0x5DC, 0, 0x76C, 0x4B0, 0, 2, 4, 0 },
    { 0x19, 0, 2, 0x64, 0, 0xBB8, 0x800, 0, 2, 4, 0 },
    { 0x19, 0, 2, 0x384, 0, 0x384, 0, 0, 2, 4, 0 },
    { 0x19, 0, 2, 0x384, 0, 0x834, 0x5DC, 0, 2, 4, 0 },
    { 1, 0, 0, -0xBB8, 0, 0x5DC, 0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B284[2] = {
    { 0x16, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B2A4[3] = {
    { 9, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0 },
    { 0xA, 0x10, 0, -0x636, 1, -0x4EC, 0x17C, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_dryfield_8017B2D4[8] = {
    { 0xC, 1, 0, -0x708, 1, -0x1F4, 0, 0, 0, 2, 0 },
    { 0xC, 1, 0, -0xC8, 1, -0x708, 0xC00, 0, 0, 2, 0 },
    { 0xC, 1, 0, 0x2BC, 1, 0xC8, 0x9C4, 0, 0, 2, 0 },
    { 0xC, 1, 0, 0x320, 1, 0x258, 0x5DC, 0, 0, 2, 0 },
    { 0xC, 1, 0, 0x320, 1, 0x3E8, 0xC8, 0, 0, 2, 0 },
    { 0xC, 1, 0, 0x190, 1, 0x4B0, 0x898, 0, 0, 2, 0 },
    { 0xC, 1, 0, 0x384, 1, 0x6A4, 0x9C4, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
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
