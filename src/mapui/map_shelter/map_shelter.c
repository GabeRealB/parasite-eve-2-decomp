#include "mapui/map_shelter.h"

#include <psyq/sys/types.h>

#include "types.h"

#include "actors/actor_503500.h"

#include "gameplay/area_flags.h"
#include "gameplay/battle_reward.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/item_menu.h"
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

#include "rooms/mine_cavern.h"

#include "rooms/mine_forked_tunnel.h"

#include "rooms/mine_gorge.h"

#include "rooms/mine_mesa.h"

#include "rooms/mine_refuge.h"

#include "rooms/mine_secret_passage.h"

#include "rooms/mine_tunnel.h"

#include "rooms/mine_tunnel_entrance.h"

#include "rooms/shelter_1f_heliport_s4.h"

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

#include "rooms/shelter_r36.h"

#include "rooms/shelter_r37.h"

#include "rooms/shelter_r47.h"

#include "rooms/shelter_r48.h"

#include "rooms/shelter_r49.h"

/// Binds the shared Mine/Shelter resolver to this overlay's public export.
///
/// Keep this s32 (RoomEventMsg*, RoomEventMsg*) identifier binding defined through
/// the shared header and fragment; it evaluates nothing and is undefined below.
#define ROOM_VARIANT_RESOLVE_SHELTER mapShelterRoomVariantResolve
#include "../../shared/room_variants.h"

/* The Mesa, mine and Shelter stage's map UI overlay: a hook the stage's rooms
 * call at this map's slot address, and the per-stage tables gameplay and main
 * index by stage. Most point into the stage's room packages or at the map
 * pictures' marker models; for some rooms the overlay carries the room's
 * coordinate and object records itself, and the flagged item and enemy
 * placement lists.
 */

/// The overlay's own flagged item and enemy placement lists, which the
/// `AreaObjectRoom` table names before they are defined.
static AreaObjectPlace D_map_shelter_8017B6D8[2];
static AreaObjectPlace D_map_shelter_8017B6F8[3];
static AreaObjectPlace D_map_shelter_8017B728[5];
static AreaObjectPlace D_map_shelter_8017B778[3];
static AreaObjectPlace D_map_shelter_8017B7A8[3];
static AreaObjectPlace D_map_shelter_8017B7D8[2];
static AreaObjectPlace D_map_shelter_8017B7F8[5];
static AreaObjectPlace D_map_shelter_8017B848[2];
static AreaObjectPlace D_map_shelter_8017B868[3];
static AreaObjectPlace D_map_shelter_8017B898[15];
static AreaObjectPlace D_map_shelter_8017B988[1];
static AreaObjectPlace D_map_shelter_8017B998[2];
static AreaObjectPlace D_map_shelter_8017B9B8[3];
static AreaObjectPlace D_map_shelter_8017B9E8[3];
static AreaObjectPlace D_map_shelter_8017BA18[5];
static AreaObjectPlace D_map_shelter_8017BA68[4];
static AreaObjectPlace D_map_shelter_8017BAA8[2];
static AreaObjectPlace D_map_shelter_8017BAC8[2];
static AreaObjectPlace D_map_shelter_8017BAE8[4];
static AreaObjectPlace D_map_shelter_8017BB28[3];

#include "../../shared/room_variants_shelter.inc.c"
#undef ROOM_VARIANT_RESOLVE_SHELTER

GfxImageSlot gMapShelterImageSlotsByArea[50] = {
    [0]                                               = GFX_IMAGE_SLOT(0x3A8E0),
    [GAME_AREA_MINE_MESA]                             = GFX_IMAGE_SLOT(0x4B5F0),
    [GAME_AREA_MINE_CAVERN]                           = GFX_IMAGE_SLOT(0x444B0),
    [GAME_AREA_MINE_TUNNEL_ENTRANCE]                  = GFX_IMAGE_SLOT(0x57100),
    [GAME_AREA_MINE_TUNNEL]                           = GFX_IMAGE_SLOT(0x56010),
    [GAME_AREA_MINE_GORGE]                            = GFX_IMAGE_SLOT(0x520D0),
    [GAME_AREA_MINE_REFUGE]                           = GFX_IMAGE_SLOT(0x4F6C0),
    [GAME_AREA_MINE_FORKED_TUNNEL]                    = GFX_IMAGE_SLOT(0x50950),
    [GAME_AREA_MINE_SECRET_PASSAGE]                   = GFX_IMAGE_SLOT(0x529D0),
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL]              = GFX_IMAGE_SLOT(0x518A0),
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY]  = GFX_IMAGE_SLOT(0x53440),
    [GAME_AREA_SHELTER_B1_STOREROOM]                  = GFX_IMAGE_SLOT(0x4E150),
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY]  = GFX_IMAGE_SLOT(0x50CB0),
    [GAME_AREA_SHELTER_B1_ARMORY]                     = GFX_IMAGE_SLOT(0x4ED60),
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS]          = GFX_IMAGE_SLOT(0x4EB50),
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR]              = GFX_IMAGE_SLOT(0x4FC60),
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM]         = GFX_IMAGE_SLOT(0x43FE0),
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL]          = GFX_IMAGE_SLOT(0x50B80),
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM]               = GFX_IMAGE_SLOT(0x518D0),
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL]              = GFX_IMAGE_SLOT(0x56480),
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING]        = GFX_IMAGE_SLOT(0x41CA0),
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1]            = GFX_IMAGE_SLOT(0x567F0),
    [GAME_AREA_SHELTER_B2_POD_BOTTOM]                 = GFX_IMAGE_SLOT(0x4C350),
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY]         = GFX_IMAGE_SLOT(0x50060),
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL]            = GFX_IMAGE_SLOT(0x53DC0),
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL] = GFX_IMAGE_SLOT(0x548D0),
    [GAME_AREA_SHELTER_B2_ELEVATOR]                   = GFX_IMAGE_SLOT(0x578A0),
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL]              = GFX_IMAGE_SLOT(0x518C0),
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY]  = GFX_IMAGE_SLOT(0x53240),
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM]             = GFX_IMAGE_SLOT(0x50B50),
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY]  = GFX_IMAGE_SLOT(0x4FC40),
    [GAME_AREA_SHELTER_B2_LABORATORY]                 = GFX_IMAGE_SLOT(0x478B0),
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM]              = GFX_IMAGE_SLOT(0x4D930),
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR]              = GFX_IMAGE_SLOT(0x4B480),
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK]                = GFX_IMAGE_SLOT(0x4F300),
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL]          = GFX_IMAGE_SLOT(0x50E70),
    [GAME_AREA_SHELTER_R36]                           = GFX_IMAGE_SLOT(0x53830),
    [GAME_AREA_SHELTER_R37]                           = GFX_IMAGE_SLOT(0x59070),
    [GAME_AREA_SHELTER_1F_HELIPORT_S4]                = GFX_IMAGE_SLOT(0x58EF0),
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE]               = GFX_IMAGE_SLOT(0x44E20),
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR]        = GFX_IMAGE_SLOT(0x417F0),
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM]   = GFX_IMAGE_SLOT(0x4FEF0),
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL]              = GFX_IMAGE_SLOT(0x513B0),
    [GAME_AREA_SHELTER_B4_LOWER_SEWER]                = GFX_IMAGE_SLOT(0x52420),
    [GAME_AREA_SHELTER_B4_UPPER_SEWER]                = GFX_IMAGE_SLOT(0x4BF20),
    [GAME_AREA_SHELTER_B4_RESERVOIR]                  = GFX_IMAGE_SLOT(0x4DA70),
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY]               = GFX_IMAGE_SLOT(0x51640),
    [GAME_AREA_SHELTER_R47]                           = GFX_IMAGE_SLOT(0x422F0),
    [GAME_AREA_SHELTER_R48]                           = GFX_IMAGE_SLOT(0x46F00),
    [GAME_AREA_SHELTER_R49]                           = GFX_IMAGE_SLOT(0x58970),
};

u8 D_map_shelter_80179CD0[7] = { 0, 0x17, 0x17, 0x17, 0x18, 0x18, 0x18 };

MenuMapArea D_map_shelter_80179CD8[51] = {
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0x13B, 0xDEA, 0x4B, 0xFFF5, 0x12C, 0x12C, 2 },
    { 0x4522, 0x9C4, 0x15, 0xFFEB, 0x12C, 0x12C, 1 },
    { 0x4204, 0x4B0, 0x3F, 0xFFF3, 0x12C, 0x12C, 2 },
    { 0x3CF0, 0x4B0, 2, 0xFFF5, 0x12C, 0x12C, 2 },
    { 0x43F8, 0x640, 0xFFC8, 0xFFF4, 0x12C, 0x12C, 2 },
    { 0x5C0, 0x12B, 0xFF90, 0xFFEB, 0x12C, 0x12C, 2 },
    { 0x92E, 0x2904, 0x11, 2, 0x12C, 0x12C, 2 },
    { 0x514, 0x36B0, 0xFFE0, 0xFFFC, 0x12C, 0x12C, 1 },
    { -0x2EE0, 0, 0xFFF1, 0x5F, 0x10E, 0x12C, 3 },
    { -0x935, -0xF96, 0x4D, 0x4F, 0x12C, 0x12C, 3 },
    { 0x1388, -0x820, 0x5A, 0x2A, 0xF0, 0x12C, 3 },
    { -0x7D0, 0xE10, 0x4D, 0xFFF9, 0x12C, 0x12C, 3 },
    { 0, 0x834, 0x21, 0x34, 0x104, 0x104, 3 },
    { 0, -0x35C, 0x23, 0x18, 0x12C, 0x118, 3 },
    { 0, -0x4A38, 0xE, 0x55, 0x12C, 0x118, 3 },
    { 0xD80, 0x3C0, 0xF, 0xB, 0xDC, 0x118, 3 },
    { 0x6A2, -0x178C, 0xE, 0xFFD5, 0x12C, 0x104, 3 },
    { 0x2774, 0x520, 0xFFE1, 0x24, 0x12C, 0x12C, 3 },
    { 0x26E1, 0x5C2, 0xFFDF, 0x26, 0x10E, 0x12C, 3 },
    { 0x779, -0x1C0C, 0xFFBA, 8, 0x12C, 0x12C, 3 },
    { 0x1A6B, 0x3E8, 0xFFD2, 0x21, 0x12C, 0x12C, 3 },
    { 0x606, 0x1CCE, 0xFFFA, 0xFFB2, 0x12C, 0x12C, 4 },
    { 0, 0, 0, 0, 0x12C, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0x1846, 0x3C, 0xFFF8, 0x18, 0x12C, 0x12C, 3 },
    { 0x150D, 0xC2, 0xFFF8, 0x35, 0x12C, 0x12C, 3 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { -0x1559, 0x410, 0xFFE2, 0x59, 0x11B, 0x12C, 4 },
    { -0x96C, -0xF6A, 0x1F, 0x4D, 0x12C, 0x12C, 4 },
    { 0x261B, 0x409, 0x2B, 0x27, 0x118, 0x12C, 4 },
    { 0x7D0, -0x1194, 0x2D, 0x11, 0x12C, 0x12C, 4 },
    { 0, 0x2BC, 0xFFF3, 0x38, 0x104, 0x12C, 4 },
    { 0, 0, 0xFFF3, 0x16, 0x118, 0x118, 4 },
    { -0x43, -0x4909, 0xFFE0, 0x53, 0x12C, 0x10E, 4 },
    { -0x59, -0x3395, 0xFFE0, 7, 0x12C, 0x12C, 4 },
    { 0x761, -0x251C, 0xFFE0, 0xFFCF, 0x10E, 0x12C, 4 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0x4CF4, -0x1162, 0xFFEB, 0xFFC5, 0x12C, 0x12C, 5 },
    { 0x280, -0xA60, 0xFFF7, 0xFFCC, 0x12C, 0x12C, 5 },
    { -0x14D5, 0xBA8, 0x23, 0x23, 0x12C, 0x12C, 5 },
    { -0x179F, 0x5DC, 0x22, 0x3E, 0x12C, 0x12C, 5 },
    { -0x1E8D, 0x995, 0xFFB6, 0xFFEA, 0x12C, 0x12C, 6 },
    { -0x23F3, 0x193A, 3, 0xFFEC, 0x122, 0x12C, 6 },
    { 0x148A, -0xB74, 0x4D, 0xE, 0x12C, 0x12C, 6 },
    { 0x190, -0x400, 0x36, 0xFFE3, 0x12C, 0x12C, 6 },
    { 0x9E7, 0xC94, 0x2D, 0xFFC5, 0x12C, 0x12C, 3 },
    { 0x606, 0x1CCE, 0xFFFA, 0xFFB2, 0x12C, 0x12C, 4 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_END },
};

MenuMapAreaShape D_map_shelter_80179FA4[50] = {
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_02_8012EFA0, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_00_8012EFBC, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_02_8012F030, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_02_8012F0C0, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_02_8012F150, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_02_8012F1E0, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_02_8012F28C, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_00_8012F0D8, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012EFD8, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F084, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F114, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F1C0, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F26C, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F318, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F3F8, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F488, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F584, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F664, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F710, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F7F0, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F8D0, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_04_8012F6BC, 4, 0x30 },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F960, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012F9F0, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_04_8012EFD8, 4, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_04_8012F084, 4, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_04_8012F114, 4, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_04_8012F1C0, 4, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_04_8012F26C, 4, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_04_8012F318, 4, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_04_8012F468, 4, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_04_8012F4F8, 4, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_04_8012F5F4, 4, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_05_8012EFF0, 5, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_05_8012F14C, 5, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_05_8012F1F8, 5, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_05_8012F32C, 5, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_06_8012F260, 6, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_06_8012F0B8, 6, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_06_8012EFF0, 6, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_06_8012F198, 6, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_03_8012FAEC, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s4_04_8012F6BC, 4, 0x16 },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
};

MenuMapMarker D_map_shelter_8017A134[31] = {
    { 3, MENU_MAP_MARKER_AREA_ANY, 14, 90 },
    { 3, MENU_MAP_MARKER_AREA_ANY, 58, 44 },
    { 5, MENU_MAP_MARKER_AREA_ANY, 35, 57 },
    { 4, MENU_MAP_MARKER_AREA_ANY, 45, 18 },
    { 4, MENU_MAP_MARKER_AREA_ANY, 28, 77 },
    { 4, MENU_MAP_MARKER_AREA_ANY, 44, 44 },
    { 4, MENU_MAP_MARKER_AREA_ANY, -32, 85 },
    { 3, MENU_MAP_MARKER_AREA_ANY, 0, 24 },
    { 3, MENU_MAP_MARKER_AREA_ANY, -34, 45 },
    { 4, MENU_MAP_MARKER_AREA_ANY, -46, 22 },
    { 4, MENU_MAP_MARKER_AREA_ANY, -19, 51 },
    { 3, MENU_MAP_MARKER_AREA_ANY, 36, -58 },
    { 4, MENU_MAP_MARKER_AREA_ANY, -12, -76 },
    { 2, MENU_MAP_MARKER_AREA_ANY, -116, -12 },
    { 5, MENU_MAP_MARKER_AREA_ANY, 64, 55 },
    { 6, MENU_MAP_MARKER_AREA_ANY, 49, 14 },
    { 6, MENU_MAP_MARKER_AREA_ANY, 102, -29 },
    { 3, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 70, 96 },
    { 4, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, 26, 95 },
    { 1, MENU_MAP_MARKER_AREA_ANY, -31, -11 },
    { 3, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 25, -68 },
    { 3, MENU_MAP_MARKER_AREA_ANY, -94, -16 },
    { 3, MENU_MAP_MARKER_AREA_ANY, 77, -4 },
    { 3, MENU_MAP_MARKER_AREA_ANY, -37, 34 },
    { 3, MENU_MAP_MARKER_AREA_ANY, 90, 25 },
    { 4, MENU_MAP_MARKER_AREA_ANY, -49, 51 },
    { 3, MENU_MAP_MARKER_AREA_ANY, 90, 45 },
    { 3, MENU_MAP_MARKER_AREA_ANY, 52, 70 },
    { 4, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, -20, -69 },
    { 5, GAME_AREA_SHELTER_B3_ELEVATOR_HALL, 39, 64 },
    { 0, 0, 0, 0 },
};

MenuMapIcon D_map_shelter_8017A1F0[15] = {
    { 2, 1, 0, GAME_FLAG_09A, 97, -13 },
    { 2, 6, MENU_MAP_ICON_KIND_TELEPHONE, 0, -114, -27 },
    { 3, 0x10, MENU_MAP_ICON_KIND_TELEPHONE, 0, 3, -36 },
    { 3, 0x10, 0, 0, 26, -36 },
    { 3, 0x14, MENU_MAP_ICON_KIND_TELEPHONE, 0, -76, 0 },
    { 3, 0x14, 0, GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE, -77, -16 },
    { 4, 0x1F, MENU_MAP_ICON_KIND_TELEPHONE, 0, -2, 68 },
    { 4, 0x1F, 0, 0, 0, 43 },
    { 5, 0x29, MENU_MAP_ICON_KIND_TELEPHONE, 0, 38, 50 },
    { 3, 0x2F, MENU_MAP_ICON_KIND_TELEPHONE, 0, 82, -80 },
    { 3, 0xC, MENU_MAP_ICON_KIND_OBJECTIVE, 0x20, 90, 0 },
    { 3, 0x2F, MENU_MAP_ICON_KIND_OBJECTIVE, 0x28, 85, -73 },
    { 3, 0x2F, MENU_MAP_ICON_KIND_OBJECTIVE, 0x29, 85, -73 },
    { 4, 0x21, MENU_MAP_ICON_KIND_OBJECTIVE, 0x2E, -48, 52 },
    { 0, 0, 0, 0, 0, 0 },
};

MenuMapAreaName D_map_shelter_8017A268[49] = {
    { "Mesa" },
    { "Cavern" },
    { "Tunnel entrance" },
    { "Tunnel" },
    { "Gorge" },
    { "Refuge" },
    { "Forked tunnel" },
    { "Secret passage" },
    { "Elevator hall" },
    { "South maintenance walkway" },
    { "Storeroom" },
    { "North maintenance walkway" },
    { "Armory" },
    { "Sleeping quarters" },
    { "Main corridor" },
    { "Sterilization room" },
    { "Pod access tunnel" },
    { "Control room" },
    { "Access tunnel" },
    { "Underground parking" },
    { "Golem freezer 1" },
    { "Pod bottom" },
    { "Pod service gantry" },
    { "Transfer tunnel" },
    { "Control room access tunnel" },
    { "Elevator" },
    { "Elevator hall" },
    { "South maintenance walkway" },
    { "Operating room" },
    { "North maintenance walkway" },
    { "Laboratory" },
    { "Breeding room" },
    { "Main corridor" },
    { "Septic tank" },
    { "Pod access tunnel" },
    { "Oval Office\x81\x69night\x81\x6A" },
    { "" },
    { "" },
    { "Dumping hole" },
    { "Garbage incinerator" },
    { "Incinerator control room" },
    { "Elevator hall" },
    { "Lower sewer" },
    { "Upper sewer" },
    { "Reservoir" },
    { "Water supply" },
    { "Pod deck" },
    { "Pod bottom" },
    { "Oval Office\x81\x69morning\x81\x6A" },
};

/// Empty generic spawn list shared by areas with flag-only object placements.
///
/// The single terminator keeps the spawn pointer non-NULL and stops every kind
/// search before a task descriptor is read. The placement lists still seed
/// and expose their two-bit object flags.
static AreaObjectSpawn _gMapShelterEmptyObjectSpawns[1] = {
    { .kind = AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_shelter_8017A898[2] = {
    { 0x20D, { { { TASK_BODY_TMD, 0x62 } }, mineForkedTunnelAreaObjectTask, { &gMineForkedTunnelModel01B48 } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_shelter_8017A8B8[2] = {
    { 0x127, { { { TASK_BODY_TMD, 0x62 } }, shelterB1SleepingQuartersAreaObjectTask, { &gShelterB1SleepingQuartersModel02DFC } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_shelter_8017A8D8[2] = {
    { 0x70A, { { { TASK_BODY_TMD, 0x62 } }, itemPickupContainerLidTask, { &gShelterB2LaboratoryAcropolisSanctuaryModel090F0 } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_shelter_8017A8F8[2] = {
    { 0x129, { { { TASK_BODY_TMD, 0x62 } }, shelterB2BreedingRoomAreaObjectTask, { &gShelterB2BreedingRoomModel02E04 } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_shelter_8017A918[2] = {
    { 0x37, { { { TASK_BODY_TMD, 0x62 } }, itemPickupContainerLidTask, { &gShelterB3DumpingHoleAcropolisSanctuaryModel090F0 } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_shelter_8017A938[3] = {
    { 0x20B, { { { TASK_BODY_TMD, 0x62 } }, actor503500SliderTask, { &gActor503500Model14DA0 } } },
    { 0x20C, { { { TASK_BODY_TMD, 0x62 } }, actor503500SliderTask, { &gActor503500Model15820 } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_shelter_8017A968[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_shelter_8017A978[2] = {
    { 0x708, { { { TASK_BODY_TMD, 0x62 } }, itemPickupContainerLidTask, { &gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0 } } },
    { AREA_OBJECT_SPAWN_END },
};

AreaObjectRoom D_map_shelter_8017A998[51] = {
    { { NULL }, NULL },
    { { D_map_shelter_8017B6D8 }, _gMapShelterEmptyObjectSpawns },
    { { D_map_shelter_8017B6F8 }, _gMapShelterEmptyObjectSpawns },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_map_shelter_8017B728 }, _gMapShelterEmptyObjectSpawns },
    { { D_map_shelter_8017B778 }, D_map_shelter_8017A898 },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_map_shelter_8017B7A8 }, _gMapShelterEmptyObjectSpawns },
    { { D_map_shelter_8017B7D8 }, _gMapShelterEmptyObjectSpawns },
    { { D_map_shelter_8017B7F8 }, _gMapShelterEmptyObjectSpawns },
    { { D_map_shelter_8017B848 }, D_map_shelter_8017A8B8 },
    { { NULL }, NULL },
    { { D_map_shelter_8017B868 }, D_map_shelter_8017A978 },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_map_shelter_8017B898 }, D_map_shelter_8017A968 },
    { { D_map_shelter_8017B988 }, _gMapShelterEmptyObjectSpawns },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_map_shelter_8017B998 }, _gMapShelterEmptyObjectSpawns },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_map_shelter_8017B9B8 }, _gMapShelterEmptyObjectSpawns },
    { { NULL }, NULL },
    { { D_map_shelter_8017B9E8 }, D_map_shelter_8017A8D8 },
    { { D_map_shelter_8017BA18 }, D_map_shelter_8017A8F8 },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_map_shelter_8017BA68 }, D_map_shelter_8017A918 },
    { { NULL }, NULL },
    { { D_map_shelter_8017BAA8 }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_map_shelter_8017BAC8 }, NULL },
    { { NULL }, NULL },
    { { D_map_shelter_8017BAE8 }, _gMapShelterEmptyObjectSpawns },
    { { D_map_shelter_8017BB28 }, D_map_shelter_8017A938 },
    { { NULL }, NULL },
    { { .sentinel = AREA_OBJECT_ROOM_END }, NULL },
};

TaskDesc D_map_shelter_8017AB30[] = {
    { { { TASK_BODY_NONE, 0x20 } }, mineMesaRoomTask, { .value = GP_TASK_LOC_KEY(4, 1, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, mineCavernRoomTask, { .value = GP_TASK_LOC_KEY(4, 2, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, mineTunnelEntranceRoomTask, { .value = GP_TASK_LOC_KEY(4, 3, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, mineTunnelRoomTask, { .value = GP_TASK_LOC_KEY(4, 4, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, mineGorgeRoomTask, { .value = GP_TASK_LOC_KEY(4, 5, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, mineRefugeRoomTask, { .value = GP_TASK_LOC_KEY(4, 6, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, mineForkedTunnelRoomTask, { .value = GP_TASK_LOC_KEY(4, 7, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, mineSecretPassageRoomTask, { .value = GP_TASK_LOC_KEY(4, 8, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1ElevatorHallRoomTask, { .value = GP_TASK_LOC_KEY(4, 9, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1SouthMaintenanceWalkwayRoomTask, { .value = GP_TASK_LOC_KEY(4, 10, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1StoreroomTask, { .value = GP_TASK_LOC_KEY(4, 11, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1NorthMaintenanceWalkwayRoomTask, { .value = GP_TASK_LOC_KEY(4, 12, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1ArmoryRoomTask, { .value = GP_TASK_LOC_KEY(4, 13, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1SleepingQuartersRoomTask, { .value = GP_TASK_LOC_KEY(4, 14, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1MainCorridorRoomTask, { .value = GP_TASK_LOC_KEY(4, 15, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1SterilizationRoomTask, { .value = GP_TASK_LOC_KEY(4, 16, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1PodAccessTunnelRoomTask, { .value = GP_TASK_LOC_KEY(4, 17, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1AccessTunnelRoomTask, { .value = GP_TASK_LOC_KEY(4, 19, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1UndergroundParkingRoomTask, { .value = GP_TASK_LOC_KEY(4, 20, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1GolemFreezer1RoomTask, { .value = GP_TASK_LOC_KEY(4, 21, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1TransferTunnelRoomTask, { .value = GP_TASK_LOC_KEY(4, 24, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1ControlRoomAccessTunnelRoomTask, { .value = GP_TASK_LOC_KEY(4, 25, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB2ElevatorRoomTask, { .value = GP_TASK_LOC_KEY(4, 26, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB2ElevatorHallRoomTask, { .value = GP_TASK_LOC_KEY(4, 27, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB2SouthMaintenanceWalkwayRoomTask, { .value = GP_TASK_LOC_KEY(4, 28, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB2OperatingRoomTask, { .value = GP_TASK_LOC_KEY(4, 29, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB2NorthMaintenanceWalkwayRoomTask, { .value = GP_TASK_LOC_KEY(4, 30, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB2LaboratoryTask, { .value = GP_TASK_LOC_KEY(4, 31, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB2BreedingRoomRoomTask, { .value = GP_TASK_LOC_KEY(4, 32, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB2MainCorridorRoomTask, { .value = GP_TASK_LOC_KEY(4, 33, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB2SepticTankRoomTask, { .value = GP_TASK_LOC_KEY(4, 34, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB2PodAccessTunnelRoomTask, { .value = GP_TASK_LOC_KEY(4, 35, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterR36RoomTask, { .value = GP_TASK_LOC_KEY(4, 36, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterR37RoomTask, { .value = GP_TASK_LOC_KEY(4, 37, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelter1fHeliportS4RoomTask, { .value = GP_TASK_LOC_KEY(4, 38, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB3DumpingHoleRoomTask, { .value = GP_TASK_LOC_KEY(4, 39, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB3GarbageIncineratorRoomTask, { .value = GP_TASK_LOC_KEY(4, 40, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB3IncineratorControlRoomTask, { .value = GP_TASK_LOC_KEY(4, 41, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB3ElevatorHallRoomTask, { .value = GP_TASK_LOC_KEY(4, 42, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB4LowerSewerRoomTask, { .value = GP_TASK_LOC_KEY(4, 43, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB4UpperSewerRoomTask, { .value = GP_TASK_LOC_KEY(4, 44, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB4ReservoirRoomTask, { .value = GP_TASK_LOC_KEY(4, 45, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB4WaterSupplyRoomTask, { .value = GP_TASK_LOC_KEY(4, 46, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterR48RoomTask, { .value = GP_TASK_LOC_KEY(4, 48, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterR47RoomTask, { .value = GP_TASK_LOC_KEY(4, 47, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterR49RoomTask, { .value = GP_TASK_LOC_KEY(4, 49, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1ControlRoomRoomTask, { .value = GP_TASK_LOC_KEY(4, 18, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB1PodServiceGantryRoomTask, { .value = GP_TASK_LOC_KEY(4, 23, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB2PodBottomRoomTask, { .value = GP_TASK_LOC_KEY(4, 22, 0) } },
    { { { TASK_DESC_END, 0x20 } }, NULL, { 0 } },
};

u16 D_map_shelter_8017AD88[30] = {
    0x1CD,
    0x1CC,
    GAME_FLAG_MAP_MARK_B3_INCINERATOR_CONTROL_DOOR,
    0x1CA,
    0x1C9,
    0x1C8,
    0x1C7,
    0x1C6,
    0x1C5,
    0x1C4,
    0x1C3,
    0x1C2,
    0x1C1,
    0x1C0,
    0x9BF,
    0x9BE,
    0x1BD,
    0x1BB,
    0x1BB,
    0x1B9,
    0x1B6,
    0x1B4,
    0x9B3,
    0x1B1,
    0x9B0,
    0x9AF,
    0x9AE,
    0x1AD,
    0x1B6,
    0x1BB,
};

/// Model-lighting descriptors for Shelter B1 north maintenance walkway, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1NorthMaintenanceWalkwayRoomLighting[1] = {
    { .lights = &gShelterB1NorthMaintenanceWalkwayRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 armory, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1ArmoryRoomLighting[1] = {
    { .lights = &gShelterB1ArmoryRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 sleeping quarters, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1SleepingQuartersRoomLighting[1] = {
    { .lights = &gShelterB1SleepingQuartersRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 main corridor, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1MainCorridorRoomLighting[1] = {
    { .lights = &gShelterB1MainCorridorRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 pod access tunnel, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1PodAccessTunnelRoomLighting[1] = {
    { .lights = &gShelterB1PodAccessTunnelRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 control room, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights and
/// per-view ambient minima. Both inputs require that overlay to remain loaded.
static WorldCoordRoomLighting _gMapShelterB1ControlRoomLighting[1] = {
    { .lights = &gShelterB1ControlRoomRoomLights, .ambientTable = gShelterB1ControlRoomAmbientByView },
};

/// Model-lighting descriptors for Shelter B1 access tunnel, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1AccessTunnelRoomLighting[1] = {
    { .lights = &gShelterB1AccessTunnelRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 golem freezer 1, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights and
/// per-view ambient minima. Both inputs require that overlay to remain loaded.
static WorldCoordRoomLighting _gMapShelterB1GolemFreezer1RoomLighting[1] = {
    { .lights = &gShelterB1GolemFreezer1RoomLights, .ambientTable = gShelterB1GolemFreezer1AmbientByView },
};

/// Model-lighting descriptors for Shelter B1 pod service gantry, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1PodServiceGantryRoomLighting[1] = {
    { .lights = &gShelterB1PodServiceGantryRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 transfer tunnel, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1TransferTunnelRoomLighting[1] = {
    { .lights = &gShelterB1TransferTunnelRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B1 control room access tunnel, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB1ControlRoomAccessTunnelRoomLighting[1] = {
    { .lights = &gShelterB1ControlRoomAccessTunnelRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B2 elevator, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB2ElevatorRoomLighting[1] = {
    { .lights = &gShelterB2ElevatorRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B2 elevator hall, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB2ElevatorHallRoomLighting[1] = {
    { .lights = &gShelterB2ElevatorHallRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B2 south maintenance walkway, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB2SouthMaintenanceWalkwayRoomLighting[1] = {
    { .lights = &gShelterB2SouthMaintenanceWalkwayRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B2 operating room, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights and
/// per-view ambient minima. Both inputs require that overlay to remain loaded.
static WorldCoordRoomLighting _gMapShelterB2OperatingRoomLighting[1] = {
    { .lights = &gShelterB2OperatingRoomRoomLights, .ambientTable = gShelterB2OperatingRoomRoomAmbient },
};

/// Model-lighting descriptors for Shelter B2 north maintenance walkway, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB2NorthMaintenanceWalkwayRoomLighting[1] = {
    { .lights = &gShelterB2NorthMaintenanceWalkwayLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B2 laboratory, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights and
/// per-view ambient minima. Both inputs require that overlay to remain loaded.
static WorldCoordRoomLighting _gMapShelterB2LaboratoryRoomLighting[1] = {
    { .lights = &gShelterB2LaboratoryRoomLights, .ambientTable = gShelterB2LaboratoryViewAmbientTable },
};

/// Model-lighting descriptors for Shelter B2 breeding room, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights and
/// per-view ambient minima. Both inputs require that overlay to remain loaded.
static WorldCoordRoomLighting _gMapShelterB2BreedingRoomLighting[1] = {
    { .lights = &gShelterB2BreedingRoomRoomLights, .ambientTable = gShelterB2BreedingRoomAmbientByView },
};

/// Model-lighting descriptors for Shelter B2 main corridor, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB2MainCorridorRoomLighting[1] = {
    { .lights = &gShelterB2MainCorridorRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B2 septic tank, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB2SepticTankRoomLighting[1] = {
    { .lights = &gShelterB2SepticTankRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter area 36, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterR36RoomLighting[1] = {
    { .lights = &gShelterR36RoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter area 37, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterR37RoomLighting[1] = {
    { .lights = &gShelterR37RoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter 1F heliport (stage 4), room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelter1fHeliportS4RoomLighting[1] = {
    { .lights = &gShelter1fHeliportS4RoomLights, .ambientTable = NULL },
};

/// Dumping-hole lighting descriptors for Mine/Shelter area 39, rooms 1 and 2.
///
/// The map overlay owns these descriptors; their light and per-view ambient
/// inputs belong to the loaded dumping-hole overlay and require its lifetime.
static WorldCoordRoomLighting _gMapShelterDumpingHoleRoomLighting[2] = {
    { &gShelterB3DumpingHoleRoom1Lights, gShelterB3DumpingHoleRoom1AmbientByView },
    { &gShelterB3DumpingHoleRoom2Lights, gShelterB3DumpingHoleRoom2AmbientByView },
};

/// Model-lighting descriptors for Shelter B3 incinerator control room, rooms 1 and 2.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
/// Both room variants share the same light collection and default ambient.
static WorldCoordRoomLighting _gMapShelterB3IncineratorControlRoomLighting[SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_COUNT] = {
    { .lights = &gShelterB3IncineratorControlRoomLights, .ambientTable = NULL },
    { .lights = &gShelterB3IncineratorControlRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B3 elevator hall, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB3ElevatorHallRoomLighting[1] = {
    { .lights = &gShelterB3ElevatorHallRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B4 lower sewer, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB4LowerSewerRoomLighting[1] = {
    { .lights = &gShelterB4LowerSewerRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B4 upper sewer, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterB4UpperSewerRoomLighting[1] = {
    { .lights = &gShelterB4UpperSewerRoomLights, .ambientTable = NULL },
};

/// Model-lighting descriptors for Shelter B4 water supply, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights and
/// per-view ambient minima. Both inputs require that overlay to remain loaded.
static WorldCoordRoomLighting _gMapShelterB4WaterSupplyRoomLighting[1] = {
    { .lights = &gShelterB4WaterSupplyRoomLights, .ambientTable = gShelterB4WaterSupplyRoomAmbient },
};

/// Model-lighting descriptors for Shelter area 47, room 1.
///
/// Indexed by room - 1; borrows the loaded room overlay's mutable lights.
/// NULL ambient table selects default minima; the room overlay must stay loaded.
static WorldCoordRoomLighting _gMapShelterR47RoomLighting[1] = {
    { .lights = &gShelterR47RoomLights, .ambientTable = NULL },
};

WorldCoordRoomLighting* gMapShelterRoomLightingTables[49] = {
    [GAME_AREA_MINE_MESA - 1]                             = gMineMesaRoomLighting,
    [GAME_AREA_MINE_CAVERN - 1]                           = gMineCavernRoomLighting,
    [GAME_AREA_MINE_TUNNEL_ENTRANCE - 1]                  = gMineTunnelEntranceRoomLighting,
    [GAME_AREA_MINE_TUNNEL - 1]                           = gMineTunnelRoomLighting,
    [GAME_AREA_MINE_GORGE - 1]                            = gMineGorgeRoomLighting,
    [GAME_AREA_MINE_REFUGE - 1]                           = gMineRefugeRoomLighting,
    [GAME_AREA_MINE_FORKED_TUNNEL - 1]                    = gMineForkedTunnelRoomLighting,
    [GAME_AREA_MINE_SECRET_PASSAGE - 1]                   = gMineSecretPassageRoomLighting,
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL - 1]              = gShelterB1ElevatorHallRoomLighting,
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY - 1]  = gShelterB1SouthMaintenanceWalkwayRoomLighting,
    [GAME_AREA_SHELTER_B1_STOREROOM - 1]                  = gShelterB1StoreroomRoomLighting,
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY - 1]  = _gMapShelterB1NorthMaintenanceWalkwayRoomLighting,
    [GAME_AREA_SHELTER_B1_ARMORY - 1]                     = _gMapShelterB1ArmoryRoomLighting,
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS - 1]          = _gMapShelterB1SleepingQuartersRoomLighting,
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR - 1]              = _gMapShelterB1MainCorridorRoomLighting,
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM - 1]         = gShelterB1SterilizationRoomLighting,
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL - 1]          = _gMapShelterB1PodAccessTunnelRoomLighting,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM - 1]               = _gMapShelterB1ControlRoomLighting,
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL - 1]              = _gMapShelterB1AccessTunnelRoomLighting,
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING - 1]        = gShelterB1UndergroundParkingRoomLighting,
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 - 1]            = _gMapShelterB1GolemFreezer1RoomLighting,
    [GAME_AREA_SHELTER_B2_POD_BOTTOM - 1]                 = gShelterB2PodBottomRoomLighting,
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY - 1]         = _gMapShelterB1PodServiceGantryRoomLighting,
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL - 1]            = _gMapShelterB1TransferTunnelRoomLighting,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL - 1] = _gMapShelterB1ControlRoomAccessTunnelRoomLighting,
    [GAME_AREA_SHELTER_B2_ELEVATOR - 1]                   = _gMapShelterB2ElevatorRoomLighting,
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL - 1]              = _gMapShelterB2ElevatorHallRoomLighting,
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY - 1]  = _gMapShelterB2SouthMaintenanceWalkwayRoomLighting,
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM - 1]             = _gMapShelterB2OperatingRoomLighting,
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY - 1]  = _gMapShelterB2NorthMaintenanceWalkwayRoomLighting,
    [GAME_AREA_SHELTER_B2_LABORATORY - 1]                 = _gMapShelterB2LaboratoryRoomLighting,
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM - 1]              = _gMapShelterB2BreedingRoomLighting,
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR - 1]              = _gMapShelterB2MainCorridorRoomLighting,
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK - 1]                = _gMapShelterB2SepticTankRoomLighting,
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL - 1]          = gShelterB2PodAccessTunnelRoomLighting,
    [GAME_AREA_SHELTER_R36 - 1]                           = _gMapShelterR36RoomLighting,
    [GAME_AREA_SHELTER_R37 - 1]                           = _gMapShelterR37RoomLighting,
    [GAME_AREA_SHELTER_1F_HELIPORT_S4 - 1]                = _gMapShelter1fHeliportS4RoomLighting,
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE - 1]               = _gMapShelterDumpingHoleRoomLighting,
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR - 1]        = gShelterB3GarbageIncineratorRoomLighting,
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM - 1]   = _gMapShelterB3IncineratorControlRoomLighting,
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL - 1]              = _gMapShelterB3ElevatorHallRoomLighting,
    [GAME_AREA_SHELTER_B4_LOWER_SEWER - 1]                = _gMapShelterB4LowerSewerRoomLighting,
    [GAME_AREA_SHELTER_B4_UPPER_SEWER - 1]                = _gMapShelterB4UpperSewerRoomLighting,
    [GAME_AREA_SHELTER_B4_RESERVOIR - 1]                  = gShelterB4ReservoirRoomLighting,
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY - 1]               = _gMapShelterB4WaterSupplyRoomLighting,
    [GAME_AREA_SHELTER_R47 - 1]                           = _gMapShelterR47RoomLighting,
    [GAME_AREA_SHELTER_R48 - 1]                           = gShelterR48RoomLighting,
    [GAME_AREA_SHELTER_R49 - 1]                           = gShelterR49RoomLighting,
};

DirectionWarpEntry* D_map_shelter_8017AF88[49] = {
    gMineMesaWarpEntries,
    gMineCavernWarpEntries,
    gMineTunnelEntranceWarpEntries,
    D_mine_tunnel_8017E174,
    gMineGorgeWarpEntries,
    D_mine_refuge_80181910,
    gMineForkedTunnelWarpEntries,
    gMineSecretPassageWarpEntries,
    gShelterB1ElevatorHallWarpEntries,
    D_shelter_b1_south_maintenance_walkway_80182414,
    D_shelter_b1_storeroom_80184B70,
    D_shelter_b1_north_maintenance_walkway_80184B88,
    D_shelter_b1_armory_80182588,
    gShelterB1SleepingQuartersWarpEntries,
    gShelterB1MainCorridorWarpEntries,
    gShelterB1SterilizationRoomWarpEntries,
    gShelterB1PodAccessTunnelWarpEntries,
    D_shelter_b1_control_room_80181C78,
    D_shelter_b1_access_tunnel_8017E7EC,
    D_shelter_b1_underground_parking_8018794C,
    D_shelter_b1_golem_freezer_1_8017E798,
    gShelterB2PodBottomWarpEntries,
    D_shelter_b1_pod_service_gantry_8017FB24,
    gShelterB1TransferTunnelWarpEntries,
    D_shelter_b1_control_room_access_tunnel_80181F08,
    gShelterB2ElevatorWarps,
    gShelterB2ElevatorHallWarpEntries,
    D_shelter_b2_south_maintenance_walkway_80182644,
    gShelterB2OperatingRoomWarpEntries,
    D_shelter_b2_north_maintenance_walkway_80183C64,
    gShelterB2LaboratoryWarpEntries,
    gShelterB2BreedingRoomWarpEntries,
    gShelterB2MainCorridorWarpEntries,
    D_shelter_b2_septic_tank_80183574,
    gShelterB2PodAccessTunnelWarpEntries,
    gShelterR36WarpEntries,
    gShelterR37WarpEntries,
    gShelter1fHeliportS4WarpEntries,
    gShelterB3DumpingHoleWarpEntries,
    D_shelter_b3_garbage_incinerator_8018741C,
    gShelterB3IncineratorControlRoomWarpEntries,
    gShelterB3ElevatorHallWarpEntries,
    gShelterB4LowerSewerWarpEntries,
    D_shelter_b4_upper_sewer_80186598,
    gShelterB4ReservoirWarpEntries,
    gShelterB4WaterSupplyWarpEntries,
    gShelterR47WarpEntries,
    gShelterR48WarpEntries,
    gShelterR49WarpEntries,
};

static ViewCount* D_map_shelter_8017B04C[49] = {
    gMineMesaRoomViewCounts,
    gMineCavernRoomViewCounts,
    gMineTunnelEntranceRoomViewCounts,
    D_mine_tunnel_8017E170,
    gMineGorgeRoomViewCounts,
    gMineRefugeRoomViewCounts,
    gMineForkedTunnelRoomViewCounts,
    gMineSecretPassageRoomViewCounts,
    gShelterB1ElevatorHallRoomViewCounts,
    D_shelter_b1_south_maintenance_walkway_80182410,
    D_shelter_b1_storeroom_80184B6C,
    D_shelter_b1_north_maintenance_walkway_80184B84,
    D_shelter_b1_armory_80182584,
    gShelterB1SleepingQuartersRoomViewCounts,
    gShelterB1MainCorridorRoomViewCounts,
    gShelterB1SterilizationRoomViewCounts,
    gShelterB1PodAccessTunnelRoomViewCounts,
    D_shelter_b1_control_room_80181C74,
    gShelterB1AccessTunnelRoomViewCounts,
    gShelterB1UndergroundParkingRoomViewCounts,
    gShelterB1GolemFreezer1ViewCounts,
    gShelterB2PodBottomRoomViewCounts,
    D_shelter_b1_pod_service_gantry_8017FB20,
    gShelterB1TransferTunnelRoomViewCounts,
    gShelterB1ControlRoomAccessTunnelRoomViewCounts,
    gShelterB2ElevatorViewCounts,
    gShelterB2ElevatorHallRoomViewCounts,
    D_shelter_b2_south_maintenance_walkway_80182640,
    gShelterB2OperatingRoomRoomViewCounts,
    D_shelter_b2_north_maintenance_walkway_80183C60,
    gShelterB2LaboratoryRoomViewCounts,
    gShelterB2BreedingRoomRoomViewCounts,
    gShelterB2MainCorridorViewCounts,
    D_shelter_b2_septic_tank_80183570,
    gShelterB2PodAccessTunnelRoomViewCounts,
    gShelterR36ViewCounts,
    gShelterR37ViewCounts,
    gShelter1fHeliportS4RoomViewCounts,
    gShelterB3DumpingHoleViewCounts,
    D_shelter_b3_garbage_incinerator_8018740C,
    gShelterB3IncineratorControlRoomViewCounts,
    gShelterB3ElevatorHallRoomViewCounts,
    gShelterB4LowerSewerRoomViewCounts,
    gShelterB4UpperSewerViewCounts,
    gShelterB4ReservoirRoomViewCounts,
    gShelterB4WaterSupplyViewCounts,
    gShelterR47ViewCounts,
    gShelterR48ViewCounts,
    gShelterR49RoomViewCounts,
};

ViewCountTable D_map_shelter_8017B110 = { D_map_shelter_8017B04C };

static WorldCollisionRoomResources D_map_shelter_8017B114[1] = {
    { &gShelterB1NorthMaintenanceWalkwayCollisionGrid, D_shelter_b1_north_maintenance_walkway_801855EC, D_shelter_b1_north_maintenance_walkway_80185A98, D_shelter_b1_north_maintenance_walkway_801857B4 },
};

static WorldCollisionRoomResources D_map_shelter_8017B124[1] = {
    { &gShelterB1ArmoryCollisionGrid, D_shelter_b1_armory_80184B14, D_shelter_b1_armory_80184E0C, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B134[1] = {
    { &gShelterB1SleepingQuartersCollisionGrid, gShelterB1SleepingQuartersViewBoundaryTriggers, gShelterB1SleepingQuartersActionTriggers, gShelterB1SleepingQuartersOccluders },
};

static WorldCollisionRoomResources D_map_shelter_8017B144[1] = {
    { &gShelterB1MainCorridorCollisionGrid, gShelterB1MainCorridorViewBoundaries, gShelterB1MainCorridorWarpTriggers, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B154[1] = {
    { &gShelterB1PodAccessTunnelCollisionGrid, gShelterB1PodAccessTunnelViewBoundaryTriggers, gShelterB1PodAccessTunnelActionTriggers, gShelterB1PodAccessTunnelSightOccluders },
};

static WorldCollisionRoomResources D_map_shelter_8017B164[1] = {
    { &gShelterB1ControlRoomRoomCollision, D_shelter_b1_control_room_801834F4, D_shelter_b1_control_room_80183624, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B174[1] = {
    { &D_shelter_b1_access_tunnel_8017EB24, D_shelter_b1_access_tunnel_8017FA34, D_shelter_b1_access_tunnel_8017FBFC, D_shelter_b1_access_tunnel_8017FD2C },
};

static WorldCollisionRoomResources D_map_shelter_8017B184[1] = {
    { &gShelterB1GolemFreezer1RoomCollision, gShelterB1GolemFreezer1ViewBoundaryTriggers, gShelterB1GolemFreezer1ActionTriggers, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B194[1] = {
    { &gShelterB1PodServiceGantryRoomCollision, NULL, NULL, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B1A4[1] = {
    { &gShelterB1TransferTunnelRoomCollision, gShelterB1TransferTunnelViewBoundaryTriggers, gShelterB1TransferTunnelWarpTriggers, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B1B4[1] = {
    { &gShelterB1ControlRoomAccessTunnelCollisionGrid, gShelterB1ControlRoomAccessTunnelViewBoundaryTriggers, gShelterB1ControlRoomAccessTunnelWarpTriggers, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B1C4[1] = {
    { &gShelterB2ElevatorCollisionGrid, gShelterB2ElevatorViewBoundaryTriggers, gShelterB2ElevatorActionTriggers, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B1D4[1] = {
    { &gShelterB2ElevatorHallRoomCollision, gShelterB2ElevatorHallViewBoundaryTriggers, gShelterB2ElevatorHallWarpTriggers, gShelterB2ElevatorHallSightOccluders },
};

static WorldCollisionRoomResources D_map_shelter_8017B1E4[1] = {
    { &gShelterB2SouthMaintenanceWalkwayCollisionGrid, D_shelter_b2_south_maintenance_walkway_801832AC, D_shelter_b2_south_maintenance_walkway_80183474, D_shelter_b2_south_maintenance_walkway_8018385C },
};

static WorldCollisionRoomResources D_map_shelter_8017B1F4[1] = {
    { &gShelterB2OperatingRoomCollisionGrid, gShelterB2OperatingRoomViewBoundaryTriggers, gShelterB2OperatingRoomActionTriggers, gShelterB2OperatingRoomOccluders },
};

static WorldCollisionRoomResources D_map_shelter_8017B204[1] = {
    { &gShelterB2NorthMaintenanceWalkwayCollisionGrid, D_shelter_b2_north_maintenance_walkway_80185D5C, D_shelter_b2_north_maintenance_walkway_80185F24, D_shelter_b2_north_maintenance_walkway_80186308 },
};

static WorldCollisionRoomResources D_map_shelter_8017B214[1] = {
    { &gShelterB2LaboratoryCollisionGrid, gShelterB2LaboratoryViewBoundaryTriggers, gShelterB2LaboratoryActionTriggers, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B224[1] = {
    { &gShelterB2BreedingRoomCollisionGrid, gShelterB2BreedingRoomViewBoundaryTriggers, gShelterB2BreedingRoomActionTriggers, gShelterB2BreedingRoomOccluders },
};

static WorldCollisionRoomResources D_map_shelter_8017B234[1] = {
    { &gShelterB2MainCorridorCollisionGrid, gShelterB2MainCorridorViewBoundaryTriggers, gShelterB2MainCorridorActionTriggers, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B244[1] = {
    { &gShelterB2SepticTankCollisionGrid, D_shelter_b2_septic_tank_80186A54, D_shelter_b2_septic_tank_80186C1C, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B254[1] = {
    { &gShelterR36FloorCollisionGrid, gShelterR36ViewBoundaryTriggers, NULL, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B264[1] = {
    { &gShelterR37RoomCollision, gShelterR37ViewBoundaryTriggers, NULL, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B274[1] = {
    { &gShelter1fHeliportS4CollisionGrid, gShelter1fHeliportS4ViewBoundaryTriggers, NULL, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B284[2] = {
    { &gShelterB3IncineratorControlRoomCollisionGrid, gShelterB3IncineratorControlRoomViewBoundaryTriggers, gShelterB3IncineratorControlRoomRoom1ActionTriggers, gShelterB3IncineratorControlRoomOccluders },
    { &gShelterB3IncineratorControlRoomCollisionGrid, gShelterB3IncineratorControlRoomViewBoundaryTriggers, gShelterB3IncineratorControlRoomRoom2ActionTriggers, gShelterB3IncineratorControlRoomOccluders },
};

static WorldCollisionRoomResources D_map_shelter_8017B2A4[1] = {
    { &gShelterB3ElevatorHallCollisionGrid, gShelterB3ElevatorHallViewBoundaryTriggers, gShelterB3ElevatorHallActionTriggers, gShelterB3ElevatorHallOccluders },
};

static WorldCollisionRoomResources D_map_shelter_8017B2B4[1] = {
    { &gShelterB4LowerSewerCollisionGrid, gShelterB4LowerSewerViewBoundaryTriggers, gShelterB4LowerSewerActionTriggers, NULL },
};

static WorldCollisionRoomResources D_map_shelter_8017B2C4[1] = {
    { &gShelterB4UpperSewerCollisionGrid, gShelterB4UpperSewerViewBoundaryTriggers, D_shelter_b4_upper_sewer_801886F4, D_shelter_b4_upper_sewer_80188BFC },
};

static WorldCollisionRoomResources D_map_shelter_8017B2D4[1] = {
    { &gShelterB4WaterSupplyCollisionGrid, gShelterB4WaterSupplyViewBoundaryTriggers, gShelterB4WaterSupplyActionTriggers, gShelterB4WaterSupplyOccluders },
};

static WorldCollisionRoomResources D_map_shelter_8017B2E4[1] = {
    { &gShelterR47CollisionGrid, gShelterR47ViewBoundaryTriggers, gShelterR47ActionTriggers, NULL },
};

static WorldCollisionRoomResources* D_map_shelter_8017B2F4[49] = {
    gMineMesaRoomCollisionResources,
    gMineCavernRoomCollisionResources,
    gMineTunnelEntranceRoomCollisionResources,
    D_mine_tunnel_8017E15C,
    gMineGorgeRoomCollisionResources,
    D_mine_refuge_801818F8,
    gMineForkedTunnelRoomCollisionResources,
    gMineSecretPassageRoomCollisionResources,
    gShelterB1ElevatorHallRoomCollisionResources,
    D_shelter_b1_south_maintenance_walkway_801823FC,
    D_shelter_b1_storeroom_80184B50,
    D_map_shelter_8017B114,
    D_map_shelter_8017B124,
    D_map_shelter_8017B134,
    D_map_shelter_8017B144,
    gShelterB1SterilizationRoomCollisionResources,
    D_map_shelter_8017B154,
    D_map_shelter_8017B164,
    D_map_shelter_8017B174,
    D_shelter_b1_underground_parking_801877F4,
    D_map_shelter_8017B184,
    gShelterB2PodBottomRoomCollisionResources,
    D_map_shelter_8017B194,
    D_map_shelter_8017B1A4,
    D_map_shelter_8017B1B4,
    D_map_shelter_8017B1C4,
    D_map_shelter_8017B1D4,
    D_map_shelter_8017B1E4,
    D_map_shelter_8017B1F4,
    D_map_shelter_8017B204,
    D_map_shelter_8017B214,
    D_map_shelter_8017B224,
    D_map_shelter_8017B234,
    D_map_shelter_8017B244,
    gShelterB2PodAccessTunnelRoomCollisionResources,
    D_map_shelter_8017B254,
    D_map_shelter_8017B264,
    D_map_shelter_8017B274,
    gShelterB3DumpingHoleRoomCollisionResources,
    D_shelter_b3_garbage_incinerator_801872B8,
    D_map_shelter_8017B284,
    D_map_shelter_8017B2A4,
    D_map_shelter_8017B2B4,
    D_map_shelter_8017B2C4,
    gShelterB4ReservoirRoomCollisionResources,
    D_map_shelter_8017B2D4,
    D_map_shelter_8017B2E4,
    gShelterR48RoomCollisionResources,
    gShelterR49RoomCollisionResources,
};

WorldCollisionStageResources D_map_shelter_8017B3B8 = { D_map_shelter_8017B2F4 };

static ViewCamera* D_map_shelter_8017B3BC[49] = {
    gMineMesaViewCameras,
    gMineCavernViewCameras,
    gMineTunnelEntranceViewCameras,
    gMineTunnelViewCameras,
    gMineGorgeViewCameras,
    gMineRefugeViewCameras,
    gMineForkedTunnelViewCameras,
    gMineSecretPassageViewCameras,
    gShelterB1ElevatorHallViewCameras,
    gShelterB1SouthMaintenanceWalkwayViewCameras,
    D_shelter_b1_storeroom_801850FC,
    gShelterB1NorthMaintenanceWalkwayViewCameras,
    D_shelter_b1_armory_80182EF4,
    gShelterB1SleepingQuartersViewCameras,
    gShelterB1MainCorridorViewCameras,
    gShelterB1SterilizationRoomViewCameras,
    gShelterB1PodAccessTunnelViewCameras,
    gShelterB1ControlRoomViewCameras,
    gShelterB1AccessTunnelViewCameras,
    gShelterB1UndergroundParkingViewCameras,
    gShelterB1GolemFreezer1ViewCameras,
    gShelterB2PodBottomViewCameras,
    D_shelter_b1_pod_service_gantry_801801E8,
    gShelterB1TransferTunnelViewCameras,
    gShelterB1ControlRoomAccessTunnelViewCameras,
    gShelterB2ElevatorViewCameras,
    gShelterB2ElevatorHallViewCameras,
    gShelterB2SouthMaintenanceWalkwayViewCameras,
    gShelterB2OperatingRoomViewCameras,
    gShelterB2NorthMaintenanceWalkwayViewCameras,
    gShelterB2LaboratoryViewCameras,
    gShelterB2BreedingRoomViewCameras,
    gShelterB2MainCorridorViewCameras,
    gShelterB2SepticTankViewCameras,
    gShelterB2PodAccessTunnelViewCameras,
    gShelterR36ViewCameras,
    gShelterR37ViewCameras,
    gShelter1fHeliportS4ViewCameras,
    gShelterB3DumpingHoleViewCameras,
    gShelterB3GarbageIncineratorCameras,
    gShelterB3IncineratorControlRoomViewCameras,
    gShelterB3ElevatorHallViewCameras,
    gShelterB4LowerSewerViewCameras,
    gShelterB4UpperSewerViewCameras,
    gShelterB4ReservoirViewCameras,
    gShelterB4WaterSupplyViewCameras,
    gShelterR47ViewCameras,
    gShelterR48ViewCameras,
    gShelterR49ViewCameras,
};

ViewCameraTable D_map_shelter_8017B480 = { D_map_shelter_8017B3BC };

/// 49 area entries borrowing room-local logical-view map directories.
///
/// Indexed by area minus one; rooms and logical views use their own bounds.
/// NULL entries have no maps. Room pointers stay valid only while that room is loaded.
static u8** _gMapShelterAreaViewMaps[49] = {
    gMineMesaRoomViewMaps,
    gMineCavernRoomViewMaps,
    gMineTunnelEntranceRoomViewMaps,
    gMineTunnelRoomViewMaps,
    gMineGorgeRoomViewMaps,
    gMineRefugeRoomViewMaps,
    gMineForkedTunnelRoomViewMaps,
    gMineSecretPassageRoomViewMaps,
    gShelterB1ElevatorHallRoomViewMaps,
    gShelterB1SouthMaintenanceWalkwayRoomViewMaps,
    gShelterB1StoreroomRoomViewMaps,
    gShelterB1NorthMaintenanceWalkwayViewMaps,
    gShelterB1ArmoryRoomViewMaps,
    gShelterB1SleepingQuartersRoomViewMaps,
    gShelterB1MainCorridorRoomViewMaps,
    gShelterB1SterilizationRoomViewMaps,
    gShelterB1PodAccessTunnelRoomViewMaps,
    gShelterB1ControlRoomRoomViewMaps,
    gShelterB1AccessTunnelRoomViewMaps,
    gShelterB1UndergroundParkingRoomViewMaps,
    gShelterB1GolemFreezer1ViewMaps,
    gShelterB2PodBottomRoomViewMaps,
    gShelterB1PodServiceGantryRoomViewMaps,
    gShelterB1TransferTunnelRoomViewMaps,
    gShelterB1ControlRoomAccessTunnelRoomViewMaps,
    gShelterB2ElevatorViewMaps,
    gShelterB2ElevatorHallViewMaps,
    gShelterB2SouthMaintenanceWalkwayRoomViewMaps,
    gShelterB2OperatingRoomRoomViewMaps,
    gShelterB2NorthMaintenanceWalkwayRoomViewMaps,
    gShelterB2LaboratoryRoomViewMaps,
    gShelterB2BreedingRoomRoomViewMaps,
    gShelterB2MainCorridorViewMaps,
    gShelterB2SepticTankViewMaps,
    gShelterB2PodAccessTunnelRoomViewMaps,
    gShelterR36RoomViewMaps,
    gShelterR37ViewMaps,
    gShelter1fHeliportS4RoomViewMaps,
    gShelterB3DumpingHoleViewMaps,
    gShelterB3GarbageIncineratorViewMaps,
    gShelterB3IncineratorControlRoomViewMaps,
    gShelterB3ElevatorHallViewMaps,
    gShelterB4LowerSewerViewMaps,
    gShelterB4UpperSewerViewMaps,
    gShelterB4ReservoirViewMaps,
    gShelterB4WaterSupplyViewMaps,
    gShelterR47ViewMaps,
    gShelterR48ViewMaps,
    gShelterR49RoomViewMaps,
};

ViewIndexTable gMapShelterViewIndexTable = { _gMapShelterAreaViewMaps };

/// 49 area entries borrowing mapped-view sprite arrays from room overlays.
///
/// Indexed by area minus one, then the mapped view minus one. Room pointers
/// must stay loaded; an area entry supplies no sprite-view count or sentinel.
static SpriteView* _gMapShelterAreaSpriteViews[49] = {
    gMineMesaSpriteViews,
    gMineCavernSpriteViews,
    gMineTunnelEntranceSpriteViews,
    gMineTunnelSpriteViews,
    gMineGorgeSpriteViews,
    gMineRefugeSpriteViews,
    gMineForkedTunnelSpriteViews,
    gMineSecretPassageSpriteViews,
    gShelterB1ElevatorHallSpriteViews,
    gShelterB1SouthMaintenanceWalkwaySpriteViews,
    gShelterB1StoreroomSpriteViews,
    gShelterB1NorthMaintenanceWalkwaySpriteViews,
    gShelterB1ArmorySpriteViews,
    gShelterB1SleepingQuartersSpriteViews,
    gShelterB1MainCorridorSpriteViews,
    gShelterB1SterilizationRoomSpriteViews,
    gShelterB1PodAccessTunnelSpriteViews,
    gShelterB1ControlRoomSpriteViews,
    gShelterB1AccessTunnelSpriteViews,
    gShelterB1UndergroundParkingSpriteViews,
    gShelterB1GolemFreezer1SpriteViews,
    gShelterB2PodBottomSpriteViews,
    D_shelter_b1_pod_service_gantry_80181BA0,
    gShelterB1TransferTunnelSpriteViews,
    gShelterB1ControlRoomAccessTunnelSpriteViews,
    gShelterB2ElevatorSpriteViews,
    gShelterB2ElevatorHallSpriteViews,
    gShelterB2SouthMaintenanceWalkwaySpriteViews,
    gShelterB2OperatingRoomSpriteViews,
    gShelterB2NorthMaintenanceWalkwaySpriteViews,
    gShelterB2LaboratorySpriteViews,
    gShelterB2BreedingRoomSpriteViews,
    gShelterB2MainCorridorSpriteViews,
    gShelterB2SepticTankSpriteViews,
    gShelterB2PodAccessTunnelSpriteViews,
    gShelterR36SpriteViews,
    gShelterR37SpriteViews,
    gShelter1fHeliportS4SpriteViews,
    gShelterB3DumpingHoleSpriteViews,
    gShelterB3GarbageIncineratorSpriteViews,
    gShelterB3IncineratorControlRoomSpriteViews,
    gShelterB3ElevatorHallSpriteViews,
    gShelterB4LowerSewerSpriteViews,
    gShelterB4UpperSewerSpriteViews,
    gShelterB4ReservoirSpriteViews,
    gShelterB4WaterSupplySpriteViews,
    gShelterR47SpriteViews,
    gShelterR48SpriteViews,
    gShelterR49SpriteViews,
};

SpriteAreaTable gMapShelterSpriteAreaTable = { _gMapShelterAreaSpriteViews };

WorldCollisionSurfaceProperties** gMapShelterSurfacePropertiesByArea[49] = {
    [GAME_AREA_MINE_MESA - 1]                             = gMineMesaSurfacePropertiesByClass,
    [GAME_AREA_MINE_CAVERN - 1]                           = gMineCavernSurfacePropertiesByClass,
    [GAME_AREA_MINE_TUNNEL_ENTRANCE - 1]                  = gMineTunnelEntranceSurfacePropertiesByClass,
    [GAME_AREA_MINE_TUNNEL - 1]                           = gMineTunnelSurfaceProperties,
    [GAME_AREA_MINE_GORGE - 1]                            = gMineGorgeSurfacePropertiesByClass,
    [GAME_AREA_MINE_REFUGE - 1]                           = gMineRefugeSurfacePropertiesByClass,
    [GAME_AREA_MINE_FORKED_TUNNEL - 1]                    = gMineForkedTunnelSurfacePropertiesByClass,
    [GAME_AREA_MINE_SECRET_PASSAGE - 1]                   = gMineSecretPassageSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL - 1]              = gShelterB1ElevatorHallSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY - 1]  = gShelterB1SouthMaintenanceWalkwaySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_STOREROOM - 1]                  = gShelterB1StoreroomSurfaceClassProperties,
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY - 1]  = gShelterB1NorthMaintenanceWalkwaySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_ARMORY - 1]                     = gShelterB1ArmorySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS - 1]          = gShelterB1SleepingQuartersSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR - 1]              = gShelterB1MainCorridorSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM - 1]         = gShelterB1SterilizationRoomSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL - 1]          = gShelterB1PodAccessTunnelSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM - 1]               = gShelterB1ControlRoomSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL - 1]              = gShelterB1AccessTunnelSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING - 1]        = gShelterB1UndergroundParkingSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 - 1]            = gShelterB1GolemFreezer1SurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_POD_BOTTOM - 1]                 = gShelterB2PodBottomSurfaceProperties,
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY - 1]         = gShelterB1PodServiceGantrySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL - 1]            = gShelterB1TransferTunnelSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL - 1] = gShelterB1ControlRoomAccessTunnelSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_ELEVATOR - 1]                   = gShelterB2ElevatorSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL - 1]              = gShelterB2ElevatorHallSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY - 1]  = gShelterB2SouthMaintenanceWalkwaySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM - 1]             = gShelterB2OperatingRoomSurfaceProperties,
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY - 1]  = gShelterB2NorthMaintenanceWalkwaySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_LABORATORY - 1]                 = gShelterB2LaboratorySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM - 1]              = gShelterB2BreedingRoomSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR - 1]              = gShelterB2MainCorridorSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK - 1]                = gShelterB2SepticTankSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL - 1]          = gShelterB2PodAccessTunnelSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_R36 - 1]                           = gShelterR36SurfacePropertiesByClass,
    [GAME_AREA_SHELTER_R37 - 1]                           = gShelterR37SurfacePropertiesByClass,
    [GAME_AREA_SHELTER_1F_HELIPORT_S4 - 1]                = gShelter1fHeliportS4SurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE - 1]               = gShelterB3DumpingHoleSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR - 1]        = gShelterB3GarbageIncineratorSurfaceProperties,
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM - 1]   = gShelterB3IncineratorControlRoomSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL - 1]              = gShelterB3ElevatorHallSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B4_LOWER_SEWER - 1]                = gShelterB4LowerSewerSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B4_UPPER_SEWER - 1]                = gShelterB4UpperSewerSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B4_RESERVOIR - 1]                  = gShelterB4ReservoirSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY - 1]               = gShelterB4WaterSupplySurfacePropertiesByClass,
    [GAME_AREA_SHELTER_R47 - 1]                           = gShelterR47SurfaceProperties,
    [GAME_AREA_SHELTER_R48 - 1]                           = gShelterR48SurfacePropertiesByClass,
    [GAME_AREA_SHELTER_R49 - 1]                           = gShelterR49SurfacePropertiesByClass,
};

static AreaObjectPlace D_map_shelter_8017B6D8[2] = {
    { 0x1B, 0x705, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017B6F8[3] = {
    { 6, 0x12C, 0, 0x201 },
    { 8, 0x84, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017B728[5] = {
    { 0xC, 0xA0, 0, 3 },
    { 0x2F, 0x807, 0, 1 },
    { 4, 0x120, 0, 1 },
    { 5, 0x12C, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017B778[3] = {
    { 1, 0x11F, 0, 0x201 },
    { 0x37, 0x20D, 0, 0x201 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017B7A8[3] = {
    { 0xD, 0xA0, 0, 3 },
    { 0x32, 0x3D, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017B7D8[2] = {
    { 2, 0x121, 0, 0x201 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017B7F8[5] = {
    { 0xF, 0x8D, 0, 1 },
    { 0x11, 0xA0, 0, 3 },
    { 0x12, 0xA1, 0, 3 },
    { 0x13, 0xAC, 0, 3 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017B848[2] = {
    { 0x14, 0x127, 0, 1, 0x12C, -0x258, 0xC80, 0xD10 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017B868[3] = {
    { 0x1C, 0x708, 0, 1, 0x1418, 0, 0x32DD, 0x800 },
    { 0x2E, 0x808, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017B898[15] = {
    { 9, 0x121, 0, 0x201 },
    { 0xA, 0x122, 0, 0x201 },
    { 0xB, 0x123, 0, 0x201 },
    { 0x2D, 0x809, 0, 1 },
    { 0x23, 0x703, 0, 0, -0x640, 0, -0x117F, 0x400 },
    { 0x24, 0x704, 0, 0, -0x640, 4, -0xC36, 0x400 },
    { 0x25, 0x705, 0, 0, -0x640, 3, -0x619, 0x400 },
    { 0x26, 0x706, 0, 0, 0xD57, 0, -0xCAD, 0xC58 },
    { 0x27, 0xA0, 0, 3 },
    { 0x28, 0xA1, 0, 3 },
    { 0x29, 0xAC, 0, 3 },
    { 0x1D, 0xA9, 0, 3 },
    { 0x15, 0xAF, 0, 3 },
    { 0x35, 0xD, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017B988[1] = {
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017B998[2] = {
    { 3, 0x122, 0, 0x201 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017B9B8[3] = {
    { 0x16, 0x3D, 0, 1 },
    { 0x33, 0x3E, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017B9E8[3] = {
    { 0x1E, 0x70A, 0, 1, 0xC86, 0, 0x1217, 0x988 },
    { 0x2C, 0x80A, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017BA18[5] = {
    { 0x18, 0x129, 0, 1, 0x834, -0x352, -0xA28, 0x600 },
    { 0x19, 0xA0, 0, 3 },
    { 0x17, 4, 0, 1 },
    { 0x34, 5, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017BA68[4] = {
    { 0x1A, 0x37, 0, 0x101, 0xD7A, 0, -0x17C0, 0x3C0 },
    { 0x1F, 0xA1, 0, 3 },
    { 0x30, 0xA0, 0, 3 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017BAA8[2] = {
    { 0x2B, 0x80B, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017BAC8[2] = {
    { 0x31, 7, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017BAE8[4] = {
    { 7, 0x12D, 0, 0x201 },
    { 0x22, 0x125, 0, 0x200 },
    { 0x2A, 0x80C, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_shelter_8017BB28[3] = {
    { 0x20, 0x20B, 0, 1 },
    { 0x21, 0x20C, 0, 1 },
    { 0xFFFF },
};

InventoryBattleReward D_map_shelter_8017BB58[47] = {
    { GAME_LOCATION_KEY(4, 1, 1, 0), { 3, 0xA1, 0xAF, 0xA2 } },
    { GAME_LOCATION_KEY(4, 2, 4, 0), { 2, 0x84, 0xA9, 0x45 } },
    { GAME_LOCATION_KEY(4, 4, 1, 0), { 0xAD, 0, 0, 0xAE } },
    { GAME_LOCATION_KEY(4, 5, 1, 0), { 6, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 8, 1, 0), { 0xD, 0xA2, 0, 7 } },
    { GAME_LOCATION_KEY(4, 12, 1, 0), { 4, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 17, 4, 0), { 0xAF, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 27, 3, 0), { 0xAE, 0, 0, 0xAD } },
    { GAME_LOCATION_KEY(4, 28, 2, 0), { 0x3C, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 32, 1, 0), { 7, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 32, 2, 0), { 8, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 33, 2, 0), { 0xAF, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(4, 33, 3, 0), { 0xAD, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 34, 1, 0), { 0xAD, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 34, 2, 0), { 2, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 35, 3, 0), { 0xAD, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 39, 1, 0), { 0xA9, 0xAF, 3, 0xAA } },
    { GAME_LOCATION_KEY(4, 39, 2, 0), { 0x3C, 0xAF, 0, 0xAA } },
    { GAME_LOCATION_KEY(4, 40, 1, 0), { 3, 0, 0, 7 } },
    { GAME_LOCATION_KEY(4, 40, 2, 0), { 0xAD, 0xA2, 0, 0xD } },
    { GAME_LOCATION_KEY(4, 40, 3, 0), { 3, 0xAA, 0x3E, 7 } },
    { GAME_LOCATION_KEY(4, 44, 3, 0), { 0x3E, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 44, 4, 0), { 0xAD, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 9, 11, 0), { 0xAB, 0, 0, 0xAF } },
    { GAME_LOCATION_KEY(4, 10, 11, 0), { 0xA2, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 11, 11, 0), { 0xAB, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(4, 12, 11, 0), { 0xAA, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 14, 11, 0), { 7, 0, 0, 8 } },
    { GAME_LOCATION_KEY(4, 15, 11, 0), { 0xAA, 0, 0, 0xAB } },
    { GAME_LOCATION_KEY(4, 17, 11, 0), { 0xAF, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 19, 11, 0), { 8, 0, 0, 7 } },
    { GAME_LOCATION_KEY(4, 24, 11, 0), { 0xAF, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 25, 11, 0), { 0xA2, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 27, 11, 0), { 0xAA, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 28, 11, 0), { 0xD, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 33, 11, 0), { 0xAF, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 35, 11, 0), { 0xAF, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 27, 21, 0), { 0xAA, 0, 0, 0xA9 } },
    { GAME_LOCATION_KEY(4, 28, 21, 0), { 0xAF, 0, 0, 0x44 } },
    { GAME_LOCATION_KEY(4, 30, 21, 0), { 0xA2, 0, 0, 0xAF } },
    { GAME_LOCATION_KEY(4, 32, 21, 0), { 0x3C, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 33, 21, 0), { 0xAA, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 34, 21, 0), { 0xA2, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 35, 21, 0), { 0xAB, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 35, 22, 0), { 0x3D, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 48, 1, 0), { 0x3D, 0xA9, 0xAE, 0xA2 } },
    { INVENTORY_BATTLE_REWARD_LIST_END },
};

InventoryBattleReward D_map_shelter_8017BD8C[13] = {
    { GAME_LOCATION_KEY(4, 1, 7, 0), { 0x3C, 0xE, 0, 6 } },
    { GAME_LOCATION_KEY(4, 2, 4, 0), { 0x84, 0xA2, 0, 0 } },
    { GAME_LOCATION_KEY(4, 8, 1, 0), { 0xAE, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 15, 7, 0), { 0x88, 8, 0, 0x3E } },
    { GAME_LOCATION_KEY(4, 19, 11, 0), { 0xA6, 4, 0, 0xA2 } },
    { GAME_LOCATION_KEY(4, 32, 7, 0), { 0x43, 0xAF, 0, 0 } },
    { GAME_LOCATION_KEY(4, 39, 1, 0), { 0xA9, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 40, 3, 0), { 0xA9, 2, 6, 0 } },
    { GAME_LOCATION_KEY(4, 14, 17, 0), { 0x3C, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 19, 17, 0), { 0xAA, 0, 0, 4 } },
    { GAME_LOCATION_KEY(4, 35, 22, 0), { 2, 0, 0, 0 } },
    { GAME_LOCATION_KEY(4, 48, 1, 0), { 3, 0, 0, 7 } },
    { INVENTORY_BATTLE_REWARD_LIST_END },
};

StageMusicEntry D_map_shelter_8017BE28[600] = {
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
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x3C, 1 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0xFF, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x3E, 1 },
    { 0x3D, 0 },
    { 0xFF, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0xFF, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0xFF, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0xFF, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x3D, 3 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x3D, 3 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0xFF, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x3D, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x37, 3 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0xFF, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0xFF, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0xFF, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0xFF, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0xFF, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0xFF, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0xFF, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x43, 1 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0x5D, 1 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0xFF, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5F, 1 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x5A, 1 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x3F, 0 },
    { 0x3F, 0 },
    { 0x3F, 0 },
    { 0x26, 0 },
    { 0x3F, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x41, 0 },
    { 0x41, 0 },
    { 0x47, 0 },
    { 0x26, 0 },
    { 0x4B, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x60, 1 },
    { 0x61, 1 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x42, 1 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x4A, 1 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x50, 1 },
    { 0x48, 3 },
    { 0x48, 3 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x48, 0 },
    { 0xFF, 0 },
    { 0x48, 0 },
    { 0x48, 0 },
    { 0x48, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x48, 0 },
    { 0xFF, 0 },
    { 0x48, 0 },
    { 0x48, 0 },
    { 0x48, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x48, 0 },
    { 0xFF, 0 },
    { 0x48, 0 },
    { 0x48, 0 },
    { 0x48, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x2F, 0 },
    { 0xFF, 0 },
    { 0x2F, 0 },
    { 0x2F, 0 },
    { 0x2F, 0 },
    { 0x52, 0 },
    { 0x59, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x3B, 0 },
    { 0xFF, 0 },
    { 0x3B, 1 },
    { 0x3B, 1 },
    { 0xFF, 0 },
    { 0x52, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
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
    { 0x5E, 1 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x51, 1 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
};

StageMusicEntry D_map_shelter_8017C2D8[20] = {
    { 0x44, 2 },
    { 0x30, 2 },
    { 0x3C, 2 },
    { 0x42, 2 },
    { 0x4A, 0 },
    { 0x5E, 0 },
    { 0x5F, 0 },
    { 0xFF, 0 },
    { 0x5D, 2 },
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
