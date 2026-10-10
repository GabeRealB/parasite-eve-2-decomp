#include "mapui/map_neo_ark.h"

#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/battle_reward.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/map.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gfx_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_types.h"
#include "main/stream.h"
#include "main/task_types.h"

#include "mappic/mappic.h"

#include "rooms/neo_ark_altar.h"

#include "rooms/neo_ark_bridge.h"

#include "rooms/neo_ark_eve_access_tunnel.h"

#include "rooms/neo_ark_eve_elevator.h"

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

#include "rooms/neo_ark_substation.h"

#include "rooms/neo_ark_woodland_path.h"

#include "rooms/shelter_1f_airlock.h"

#include "rooms/shelter_1f_bulwark.h"

#include "rooms/shelter_1f_guardroom.h"

#include "rooms/shelter_1f_heliport.h"

#include "rooms/shelter_1f_parking_garage.h"

#include "rooms/shelter_1f_tent.h"

#include "rooms/shelter_1f_vehicular_airlock.h"

#include "rooms/shelter_b6_corridor.h"

#include "rooms/shelter_b6_growth_room.h"

#include "rooms/shelter_b6_nursery.h"

#include "rooms/shelter_b6_training_room.h"

/* The Neo Ark stage's map UI overlay (stage 5, Neo Ark and the Shelter): its
 * stream setup, map-room hook and music hook, and the per-stage tables gameplay
 * and main index by stage, most of which point into the stage's room packages
 * or at the map pictures' marker models.
 */

static AreaObjectPlace _gMapNeoArkVehicularAirlockObjectPlaces[2];
static AreaObjectPlace _gMapNeoArkHeliportObjectPlaces[14];
static AreaObjectPlace _gMapNeoArkIslandObjectPlaces[2];
static AreaObjectPlace _gMapNeoArkNurseryObjectPlaces[2];
static AreaObjectPlace _gMapNeoArkGrowthRoomObjectPlaces[4];
static AreaObjectPlace _gMapNeoArkCorridorObjectPlaces[2];
static AreaObjectPlace _gMapNeoArkTentObjectPlaces[4];
static AreaObjectPlace _gMapNeoArkSubmarineGalleryObjectPlaces[2];
static AreaObjectPlace _gMapNeoArkSubstationObjectPlaces[2];

/// Catalogue and UI kinds stored by this stage's saved-object lists.
///
/// Banks 0/1 are catalogue IDs; banks 7/8 select item-box and save-point UI.
enum {
    MAP_NEO_ARK_KIND_MP_BOOST_2               = 0x7,
    MAP_NEO_ARK_KIND_LIPSTICK                 = 0xB,
    MAP_NEO_ARK_KIND_MD_PLAYER                = 0xE,
    MAP_NEO_ARK_KIND_SKULL_CRYSTAL            = 0x36,
    MAP_NEO_ARK_KIND_PROTEIN_CAPSULE          = 0x3C,
    MAP_NEO_ARK_KIND_RINGERS_SOLUTION         = 0x3D,
    MAP_NEO_ARK_KIND_MONGOOSE                 = 0x88,
    MAP_NEO_ARK_KIND_9MM_PARABELLUM           = 0xA0,
    MAP_NEO_ARK_KIND_9MM_HYDRA                = 0xA1,
    MAP_NEO_ARK_KIND_44_MAEDA_SP              = 0xA7,
    MAP_NEO_ARK_KIND_GRENADE                  = 0xA9,
    MAP_NEO_ARK_KIND_AIRBURST                 = 0xAA,
    MAP_NEO_ARK_KIND_BUCKSHOT                 = 0xAC,
    MAP_NEO_ARK_KIND_556_RIFLE                = 0xAF,
    MAP_NEO_ARK_KIND_TEDDY_BEAR               = 0x124,
    MAP_NEO_ARK_KIND_DOUGLAS_LETTER           = 0x128,
    MAP_NEO_ARK_KIND_HELIPORT_FIRST_ITEM_BOX  = 0x703,
    MAP_NEO_ARK_KIND_HELIPORT_SECOND_ITEM_BOX = 0x704,
    MAP_NEO_ARK_KIND_HELIPORT_THIRD_ITEM_BOX  = 0x705,
    MAP_NEO_ARK_KIND_HELIPORT_FOURTH_ITEM_BOX = 0x706,
    MAP_NEO_ARK_KIND_GROWTH_ROOM_SAVE_POINT   = 0x80D,
    MAP_NEO_ARK_KIND_TENT_SAVE_POINT          = 0x80E,
};

/// Initial two-bit saved states; replenishable pickups retain state 3 on collection.
enum {
    MAP_NEO_ARK_STATE_AVAILABLE     = 1,
    MAP_NEO_ARK_STATE_REPLENISHABLE = 3,
};

/// Stage-local two-bit state slots used by the saved-object lists.
enum {
    MAP_NEO_ARK_OBJECT_SLOT_GROWTH_ROOM_SAVE_POINT          = 1,
    MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_DOUGLAS_LETTER         = 2,
    MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_MP_BOOST_2             = 3,
    MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_44_MAEDA_SP            = 4,
    MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_MONGOOSE               = 5,
    MAP_NEO_ARK_OBJECT_SLOT_VEHICULAR_AIRLOCK_TEDDY_BEAR    = 6,
    MAP_NEO_ARK_OBJECT_SLOT_NURSERY_MP_BOOST_2              = 7,
    MAP_NEO_ARK_OBJECT_SLOT_TENT_AIRBURST                   = 8,
    MAP_NEO_ARK_OBJECT_SLOT_TENT_RINGERS_SOLUTION           = 9,
    MAP_NEO_ARK_OBJECT_SLOT_SUBSTATION_9MM_HYDRA            = 10,
    MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_FIRST_ITEM_BOX         = 11,
    MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_SECOND_ITEM_BOX        = 12,
    MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_THIRD_ITEM_BOX         = 13,
    MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_FOURTH_ITEM_BOX        = 14,
    MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_9MM_PARABELLUM         = 15,
    MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_9MM_HYDRA              = 16,
    MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_BUCKSHOT               = 17,
    MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_GRENADE                = 18,
    MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_556_RIFLE              = 19,
    MAP_NEO_ARK_OBJECT_SLOT_SUBMARINE_GALLERY_SKULL_CRYSTAL = 20,
    MAP_NEO_ARK_OBJECT_SLOT_ISLAND_LIPSTICK                 = 21,
    MAP_NEO_ARK_OBJECT_SLOT_GROWTH_ROOM_RINGERS_SOLUTION    = 22,
    MAP_NEO_ARK_OBJECT_SLOT_GROWTH_ROOM_MD_PLAYER           = 23,
    MAP_NEO_ARK_OBJECT_SLOT_CORRIDOR_PROTEIN_CAPSULE        = 24,
    MAP_NEO_ARK_OBJECT_SLOT_TENT_SAVE_POINT                 = 46,
};

/// Priority of the saved-object model task.
enum { MAP_NEO_ARK_OBJECT_TASK_PRIORITY = 0x62 };

/// Resource-entry selectors in the rooms' variant-1 GOLEM recipes.
enum {
    MAP_NEO_ARK_RESOURCE_PAWN_BEAM_SWORD       = 20,
    MAP_NEO_ARK_RESOURCE_ROOK_BEAM_SWORD       = 23,
    MAP_NEO_ARK_RESOURCE_ROOK_GRENADE_LAUNCHER = 57,
};

/// Resource-entry IDs used by these room placement recipes.
///
/// Room-scoped selectors can bind different recipes at the same numeric ID.
enum {
    MAP_NEO_ARK_NORTH_PROMENADE_RESOURCE_STINGER           = 3,
    MAP_NEO_ARK_FOREST_ZONE_RESOURCE_EXTRA_HORNED_STRANGER = 10,
    MAP_NEO_ARK_RESOURCE_WATCHER                           = 21,
    MAP_NEO_ARK_RESOURCE_KNIGHT_GOLEM                      = 22,
    MAP_NEO_ARK_RESOURCE_AMOEBA                            = 24,
    MAP_NEO_ARK_SUBMARINE_TUNNEL_RESOURCE_NO9_WALKER       = 34,
    MAP_NEO_ARK_RESOURCE_EXTRA_PAWN_GOLEM                  = 56,
    MAP_NEO_ARK_OBSERVATORY_RESOURCE_SCENE_STOP            = 101,
    MAP_NEO_ARK_HELIPORT_RESOURCE_JODIE                    = 115,
    MAP_NEO_ARK_SUBMARINE_TUNNEL_RESOURCE_AYA_WALKER       = 132,
    MAP_NEO_ARK_HELIPORT_RESOURCE_RUPERT                   = 144,
};

/// Actor-specific selectors used by these placement tables.
///
/// Watcher weapon values index its five-row tuning tables; sight rows select
/// world-coordinate ranges. Diver modes use low-nibble behavior and high-nibble
/// waypoint/continuation selection; shot rows select local units per tick.
enum {
    MAP_NEO_ARK_SOLDIER_CARRIES_RIFLE               = 1,
    MAP_NEO_ARK_AMOEBA_ALTERNATE_VARIANT            = 1,
    MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_1            = 1,
    MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_2            = 2,
    MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_3            = 3,
    MAP_NEO_ARK_WATCHER_SIGHT_7000_ROW              = 6,
    MAP_NEO_ARK_BOG_DIVER_MODE_TUNNEL               = 2,
    MAP_NEO_ARK_BOG_DIVER_TUNNEL_CONTINUATION_GROUP = 3 << 4,
    MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_200_ROW        = 1,
    MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_220_ROW        = 3,
    MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_270_ROW        = 6,
};

/// Movie ring byte extent and Eve Elevator staging origin in VRAM words/rows.
enum {
    MAP_NEO_ARK_MOVIE_RING_BYTES               = 0x10000,
    MAP_NEO_ARK_MOVIE_ELEVATOR_STAGING_X_WORDS = 384,
    MAP_NEO_ARK_MOVIE_ELEVATOR_STAGING_Y_ROWS  = 256,
};

void mapNeoArkSetupMovieBuffers(const GameLocationKey* location)
{
    CdCmdQueue* queue = &gCdCmdQueue;
    s32         frameBytes;

    switch (location->area) {
        case GAME_AREA_SHELTER_1F_GUARDROOM:
            frameBytes    = D_8006AC5A * D_8006AC6C * 2;
            D_8006AC50[0] = (u_long*)((u8*)D_8006AC60 + MAP_NEO_ARK_MOVIE_RING_BYTES);
            D_8006AC50[1] = D_8006AC40;
            D_8006AC48[0] = (u_long*)((u8*)D_8006AC40 + frameBytes);
            D_8006AC48[1] = (u_long*)((u8*)D_8006AC48[0] + frameBytes);
            break;
        case GAME_AREA_NEO_ARK_EVE_ELEVATOR:
            queue->movieVramStaging = 1;
            queue->movieStagingX    = MAP_NEO_ARK_MOVIE_ELEVATOR_STAGING_X_WORDS;
            queue->movieStagingY    = MAP_NEO_ARK_MOVIE_ELEVATOR_STAGING_Y_ROWS;
            D_8006AC5C              = 1;
            D_8006AC50[0]           = (u_long*)((u8*)D_8006AC60 + MAP_NEO_ARK_MOVIE_RING_BYTES);
            D_8006AC48[1]           = D_8006AC40;
            D_8006AC48[0]           = D_8006AC40;
            D_8006AC50[1]           = (u_long*)((u8*)D_8006AC50[0] + D_8006AC5A * D_8006AC6C);
            gGameSession->field_80  = 0;
            queue->field_24A        = 1;
            break;
    }
    D_8006AC44             = (u8*)D_8006AC48[1] + D_8006AC5A * D_8006AC6C * 2;
    gGameSession->field_7C = 0;
    gGameSession->field_7E = 0;
}

/// Binds the room-variant fragment to this overlay's cross-room export.
///
/// The replacement is a function identifier with the `RoomVariantResolver`
/// signature. The binding applies only to the included definition.
#define ROOM_VARIANT_RESOLVE_NEO_ARK mapNeoArkResolveRoomVariant
#include "../../shared/room_variants_neo_ark.inc.c"
#undef ROOM_VARIANT_RESOLVE_NEO_ARK

/// Unity MIDI master gain used to normalize the selected song level.
enum { MAP_NEO_ARK_MIDI_GAIN_FULL = 127 };

/// Returns the area's ducked song level after master gain and the song fade.
///
/// The area update must first initialize the shared level from the song's
/// 16-bit mix level. Master gain is 0..127; multiplication and division by 127
/// use unsigned 32-bit arithmetic before applying the ramp's 0..65535 gain.
/// `songRamp` borrows the live song's writable ramp: applying it can clear a
/// completed step, but does not advance its clock. The result is a playback
/// level for subsequent channel and note scaling, without clamping.
static __inline__ s32 _mapNeoArkApplyAreaMusicVolume(LinInterp* songRamp)
{
    u32 masterGain;
    u32 volumeProduct;

    masterGain    = (u8)midiGetMasterVolume();
    volumeProduct = masterGain * D_800820E0;
    return linInterpApply(songRamp, volumeProduct / (u32)MAP_NEO_ARK_MIDI_GAIN_FULL);
}

s32 mapNeoArkUpdateMusicVolume(u32 fullVolume, u8 areaId, LinInterp* ramp)
{
    enum {
        MAP_NEO_ARK_MUSIC_DUCK_WAIT_UPDATES    = 121,
        MAP_NEO_ARK_MUSIC_RESTORE_WAIT_UPDATES = 241,
        MAP_NEO_ARK_MUSIC_VOLUME_STEP          = 0x300,
        MAP_NEO_ARK_MUSIC_RAMP_SETTLED         = 255,
    };
    s32 volume;
    u32 scaledVolume;

    // Delay each area transition before stepping the shared song gain.
    if (areaId == GAME_AREA_NEO_ARK_SUBSTATION) {
        if (D_800820E4 == 0) {
            D_800820E4 = 1;
            D_800820E6 = 0;
            D_800820E0 = fullVolume;
        } else if (D_800820E4 < MAP_NEO_ARK_MUSIC_DUCK_WAIT_UPDATES) {
            D_800820E4 = D_800820E4 + 1;
        } else if ((fullVolume >> 1) < (u32)D_800820E0) {
            D_800820E0 = D_800820E0 - MAP_NEO_ARK_MUSIC_VOLUME_STEP;
        } else {
            D_800820E4 = MAP_NEO_ARK_MUSIC_RAMP_SETTLED;
            D_800820E6 = 0;
            D_800820E0 = fullVolume >> 1;
        }
        volume = _mapNeoArkApplyAreaMusicVolume(ramp);
    } else if (areaId == GAME_AREA_NEO_ARK_POWER_PLANT_2) {
        if (D_800820E6 == 0) {
            D_800820E4 = 0;
            D_800820E6 = 1;
            D_800820E0 = fullVolume >> 1;
        } else if (D_800820E6 < MAP_NEO_ARK_MUSIC_RESTORE_WAIT_UPDATES) {
            D_800820E6 = D_800820E6 + 1;
        } else if ((u32)D_800820E0 < fullVolume) {
            D_800820E0 = D_800820E0 + MAP_NEO_ARK_MUSIC_VOLUME_STEP;
        } else {
            D_800820E4 = 0;
            D_800820E6 = MAP_NEO_ARK_MUSIC_RAMP_SETTLED;
            D_800820E0 = fullVolume;
        }
        volume = _mapNeoArkApplyAreaMusicVolume(ramp);
    } else {
        scaledVolume = midiGetMasterVolume() & 0xFF;
        scaledVolume = scaledVolume * fullVolume;
        volume       = linInterpApply(ramp, scaledVolume / (u32)MAP_NEO_ARK_MIDI_GAIN_FULL);
        D_800820E4   = 0;
        D_800820E6   = 0;
    }
    return volume;
}

GfxImageSlot gMapNeoArkImageSlotsByArea[34] = {
    [0]                                      = GFX_IMAGE_SLOT(0x3A8E0),
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE]    = GFX_IMAGE_SLOT(0x54B20),
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK] = GFX_IMAGE_SLOT(0x53DB0),
    [GAME_AREA_SHELTER_1F_BULWARK]           = GFX_IMAGE_SLOT(0x55640),
    [GAME_AREA_SHELTER_1F_HELIPORT]          = GFX_IMAGE_SLOT(0x4F560),
    [GAME_AREA_SHELTER_1F_AIRLOCK]           = GFX_IMAGE_SLOT(0x56AA0),
    [GAME_AREA_SHELTER_1F_GUARDROOM]         = GFX_IMAGE_SLOT(0x58820),
    [GAME_AREA_NEO_ARK_OBSERVATORY]          = GFX_IMAGE_SLOT(0x4A320),
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL]    = GFX_IMAGE_SLOT(0x550A0),
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR]         = GFX_IMAGE_SLOT(0x58F40),
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE]      = GFX_IMAGE_SLOT(0x52360),
    [GAME_AREA_NEO_ARK_FOREST_ZONE]          = GFX_IMAGE_SLOT(0x53320),
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL]     = GFX_IMAGE_SLOT(0x4B7D0),
    [GAME_AREA_NEO_ARK_PAVILION]             = GFX_IMAGE_SLOT(0x4DDF0),
    [GAME_AREA_NEO_ARK_ISLAND]               = GFX_IMAGE_SLOT(0x524B0),
    [GAME_AREA_NEO_ARK_GARDEN]               = GFX_IMAGE_SLOT(0x52610),
    [GAME_AREA_NEO_ARK_POWER_PLANT_2]        = GFX_IMAGE_SLOT(0x52190),
    [GAME_AREA_NEO_ARK_POWER_PLANT_1]        = GFX_IMAGE_SLOT(0x53240),
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE]         = GFX_IMAGE_SLOT(0x55170),
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE]      = GFX_IMAGE_SLOT(0x54BB0),
    [GAME_AREA_NEO_ARK_ALTAR]                = GFX_IMAGE_SLOT(0x55920),
    [GAME_AREA_NEO_ARK_SHRINE]               = GFX_IMAGE_SLOT(0x4D8E0),
    [GAME_AREA_SHELTER_B6_NURSERY]           = GFX_IMAGE_SLOT(0x47E00),
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM]       = GFX_IMAGE_SLOT(0x55620),
    [GAME_AREA_SHELTER_B6_CORRIDOR]          = GFX_IMAGE_SLOT(0x50F30),
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM]     = GFX_IMAGE_SLOT(0x50AF0),
    [GAME_AREA_NEO_ARK_R26]                  = GFX_IMAGE_SLOT(0x57570),
    [GAME_AREA_NEO_ARK_BRIDGE]               = GFX_IMAGE_SLOT(0x514A0),
    [GAME_AREA_SHELTER_1F_TENT]              = GFX_IMAGE_SLOT(0x4C020),
    [GAME_AREA_NEO_ARK_WOODLAND_PATH]        = GFX_IMAGE_SLOT(0x50890),
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY]    = GFX_IMAGE_SLOT(0x4F920),
    [GAME_AREA_NEO_ARK_R31]                  = GFX_IMAGE_SLOT(0x58C50),
    [GAME_AREA_NEO_ARK_PYRAMID]              = GFX_IMAGE_SLOT(0x53AA0),
    [GAME_AREA_NEO_ARK_SUBSTATION]           = GFX_IMAGE_SLOT(0x55DA0),
};

s32 gMapNeoArkMovieWorkspaceBytesByArea[21] = {
    [GAME_AREA_SHELTER_1F_GUARDROOM] = 0x21C00,
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR] = 0x25800,
};

/// Resource-entry IDs binding this map's placements to the selected room's task recipes.
enum {
    MAP_NEO_ARK_RESOURCE_BOG_DIVER             = 4,
    MAP_NEO_ARK_RESOURCE_HORNED_STRANGER       = 13,
    MAP_NEO_ARK_RESOURCE_PAWN_GOLEM            = 20,
    MAP_NEO_ARK_RESOURCE_BEAM_SWORD_ROOK       = 23,
    MAP_NEO_ARK_RESOURCE_MAGGOT                = 26,
    MAP_NEO_ARK_RESOURCE_BLACK_BEETLE          = 38,
    MAP_NEO_ARK_RESOURCE_BISHOP_GOLEM          = 39,
    MAP_NEO_ARK_RESOURCE_MOSSBACK              = 49,
    MAP_NEO_ARK_RESOURCE_STINGER               = 51,
    MAP_NEO_ARK_RESOURCE_EVE_CONTROLLER        = 52,
    MAP_NEO_ARK_RESOURCE_GRENADE_LAUNCHER_ROOK = 57,
    MAP_NEO_ARK_RESOURCE_EVE_BODY              = 60,
    MAP_NEO_ARK_RESOURCE_SEA_DIVER             = 61,
    MAP_NEO_ARK_RESOURCE_KYLE                  = 101,
    MAP_NEO_ARK_RESOURCE_TENT_SOLDIER_C        = 113,
    MAP_NEO_ARK_RESOURCE_TENT_SOLDIER_B        = 116,
    MAP_NEO_ARK_RESOURCE_TENT_SOLDIER_A        = 117,
    MAP_NEO_ARK_RESOURCE_MASKED_EVE            = 131,
    MAP_NEO_ARK_RESOURCE_R31_AYA               = 132,
    MAP_NEO_ARK_NURSERY_RESOURCE_SECOND_BODY   = 140,
    MAP_NEO_ARK_TRAINING_ROOM_RESOURCE_KYLE    = 140,
};

/// Authored map-page flags and the page-zero icon-list terminator.
///
/// Page flags are nibble IDs when used for area shading. Navigation treats
/// 0x80 as unavailable, but stage 5 bypasses that availability test entirely.
enum {
    MAP_NEO_ARK_PAGE_BOUNDARY           = 0,
    MAP_NEO_ARK_PAGE_FLAG_NEO_ARK       = 0x19,
    MAP_NEO_ARK_PAGE_FLAG_SHELTER_1F    = 0x1A,
    MAP_NEO_ARK_PAGE_FLAG_UNAVAILABLE   = 0x80,
    MAP_NEO_ARK_ICON_PAGE_END           = 0,
    MAP_NEO_ARK_ICON_KIND_FIXED_PICTURE = 0,
};

/// Spawn policies encoded by the room recipes used in these placement lists.
///
/// Bog Diver mode's low nibble selects behavior and bits 4..5 select a room
/// waypoint ring; high spawn bit 16 rejects its record. Horned Stranger uses the high spawn
/// byte for reusable/wounded behavior. Black Beetle's mode tens digit selects
/// its initial surface; the floor remainder selects its action. Maggot modes
/// 2/3 select ambush leader/follower. GOLEM mode bit 0 enables patrol.
enum {
    MAP_NEO_ARK_BOG_DIVER_MODE_WATER           = 1,
    MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT   = 4,
    MAP_NEO_ARK_BOG_DIVER_REJECT_SPAWN         = 1,
    MAP_NEO_ARK_BOG_DIVER_MODE_DIVING          = 3,
    MAP_NEO_ARK_BOG_DIVER_MODE_WOUNDED_GROUND  = 6,
    MAP_NEO_ARK_BOG_DIVER_MODE_WOUNDED_FLOAT   = 7,
    MAP_NEO_ARK_HORNED_STRANGER_REUSABLE       = 2,
    MAP_NEO_ARK_HORNED_STRANGER_WOUNDED        = 0x20,
    MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER = 1,
    MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL         = 10,
    MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_LEADER      = 2,
    MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_FOLLOWER    = 3,
    MAP_NEO_ARK_GOLEM_PATROLS                  = 1,
    MAP_NEO_ARK_NURSERY_BODY_HIDDEN            = 0,
    MAP_NEO_ARK_NURSERY_BODY_VISIBLE           = 1,
};

u8 gMapNeoArkPageFlagIds[8] = {
    [0]                           = MAP_NEO_ARK_PAGE_BOUNDARY,
    [MAP_NEO_ARK_PAGE_NEO_ARK]    = MAP_NEO_ARK_PAGE_FLAG_NEO_ARK,
    [MAP_NEO_ARK_PAGE_SHELTER_B6] = MAP_NEO_ARK_PAGE_FLAG_UNAVAILABLE,
    [MAP_NEO_ARK_PAGE_SHELTER_1F] = MAP_NEO_ARK_PAGE_FLAG_SHELTER_1F,
};

MenuMapArea gMapNeoArkAreas[MAP_NEO_ARK_AREA_ID_LIMIT + 2] = {
    [0]                                      = { .originX = 0, .originZ = 0, .mapX = 0, .mapY = 0, .scaleX = 0, .scaleZ = 0, .page = MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE]    = { .originX = 8564, .originZ = -1341, .mapX = 103, .mapY = 4, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_SHELTER_1F },
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK] = { .originX = -5000, .originZ = 1570, .mapX = 53, .mapY = -4, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_SHELTER_1F },
    [GAME_AREA_SHELTER_1F_BULWARK]           = { .originX = 3951, .originZ = 0, .mapX = 28, .mapY = 0, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_SHELTER_1F },
    [GAME_AREA_SHELTER_1F_HELIPORT]          = { .originX = 9920, .originZ = 3450, .mapX = -10, .mapY = 0, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_SHELTER_1F },
    [GAME_AREA_SHELTER_1F_AIRLOCK]           = { .originX = 2051, .originZ = 3070, .mapX = 79, .mapY = -11, .scaleX = 270, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_SHELTER_1F },
    [GAME_AREA_SHELTER_1F_GUARDROOM]         = { .originX = -9000, .originZ = -3620, .mapX = 40, .mapY = 13, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_SHELTER_1F },
    [GAME_AREA_NEO_ARK_OBSERVATORY]          = { .originX = 13500, .originZ = 12000, .mapX = 109, .mapY = -22, .scaleX = 300, .scaleZ = 250, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL]    = { .originX = -1200, .originZ = 1280, .mapX = 109, .mapY = 7, .scaleX = 300, .scaleZ = 260, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR]         = { .originX = -960, .originZ = 0, .mapX = -53, .mapY = 26, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_SHELTER_B6 },
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE]      = { .originX = 10819, .originZ = 1354, .mapX = 92, .mapY = -40, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_FOREST_ZONE]          = { .originX = 8995, .originZ = -128, .mapX = 53, .mapY = -75, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL]     = { .originX = 4544, .originZ = 0, .mapX = -29, .mapY = -75, .scaleX = 280, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_PAVILION]             = { .originX = 2000, .originZ = 9504, .mapX = -81, .mapY = -75, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_ISLAND]               = { .originX = 3100, .originZ = 6464, .mapX = -73, .mapY = -65, .scaleX = 300, .scaleZ = 320, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_GARDEN]               = { .originX = -3040, .originZ = -12700, .mapX = -99, .mapY = 8, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_POWER_PLANT_2]        = { .originX = 514, .originZ = -1440, .mapX = -75, .mapY = 24, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_POWER_PLANT_1]        = { .originX = 8680, .originZ = -10500, .mapX = -47, .mapY = 58, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE]         = { .originX = 510, .originZ = 1500, .mapX = 10, .mapY = 60, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE]      = { .originX = 1137, .originZ = 2000, .mapX = 60, .mapY = 60, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_ALTAR]                = { .originX = -2944, .originZ = -2035, .mapX = 7, .mapY = -2, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_SHRINE]               = { .originX = 525, .originZ = 1200, .mapX = -39, .mapY = 61, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_SHELTER_B6_NURSERY]           = { .originX = 680, .originZ = 0, .mapX = 9, .mapY = -3, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_SHELTER_B6 },
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM]       = { .originX = 4697, .originZ = 10700, .mapX = 52, .mapY = -28, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_SHELTER_B6 },
    [GAME_AREA_SHELTER_B6_CORRIDOR]          = { .originX = 430, .originZ = 0, .mapX = -46, .mapY = 26, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_SHELTER_B6 },
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM]     = { .originX = 500, .originZ = 1500, .mapX = -12, .mapY = 26, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_SHELTER_B6 },
    [GAME_AREA_NEO_ARK_R26]                  = { .originX = 0, .originZ = 0, .mapX = 0, .mapY = 0, .scaleX = 0, .scaleZ = 0, .page = MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_NEO_ARK_BRIDGE]               = { .originX = -3160, .originZ = 6368, .mapX = -98, .mapY = -61, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_SHELTER_1F_TENT]              = { .originX = -638, .originZ = 4300, .mapX = -43, .mapY = -1, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_SHELTER_1F },
    [GAME_AREA_NEO_ARK_WOODLAND_PATH]        = { .originX = -3895, .originZ = 9510, .mapX = 11, .mapY = -66, .scaleX = 290, .scaleZ = 280, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY]    = { .originX = 1300, .originZ = 3100, .mapX = -56, .mapY = -47, .scaleX = 290, .scaleZ = 280, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_R31]                  = { .originX = 0, .originZ = 0, .mapX = 0, .mapY = 0, .scaleX = 0, .scaleZ = 0, .page = MENU_MAP_AREA_PAGE_NONE },
    [GAME_AREA_NEO_ARK_PYRAMID]              = { .originX = -2944, .originZ = -2035, .mapX = 6, .mapY = -3, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [GAME_AREA_NEO_ARK_SUBSTATION]           = { .originX = 514, .originZ = -1440, .mapX = -75, .mapY = 24, .scaleX = 300, .scaleZ = 300, .page = MAP_NEO_ARK_PAGE_NEO_ARK },
    [MAP_NEO_ARK_AREA_ID_LIMIT + 1]          = { .originX = 0, .originZ = 0, .mapX = 0, .mapY = 0, .scaleX = 0, .scaleZ = 0, .page = MENU_MAP_AREA_PAGE_END },
};

MenuMapAreaShape D_map_neo_ark_8017A110[36] = {
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_03_8012EFA0, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_03_8012F030, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_03_8012F110, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_03_8012F1F0, 3, 0x1C },
    { &D_mappic_s5_03_8012F2B8, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_03_8012F348, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012F04C, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012F0DC, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_02_8012F224, 2, 0x18 },
    { &D_mappic_s5_00_8012F218, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012F2A8, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012F338, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012F3C8, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012F490, 1, 0x1E },
    { &D_mappic_s5_00_8012F58C, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012F61C, 1, 0x11 },
    { &D_mappic_s5_00_8012F61C, 1, 0x10 },
    { &D_mappic_s5_00_8012F6AC, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012F7E8, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012F878, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012F9D4, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_02_8012F044, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_02_8012F124, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_02_8012F224, 2, 9 },
    { &D_mappic_s5_02_8012F2B4, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012FBB4, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, 3, 4 },
    { &D_mappic_s5_00_8012FAD4, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, 1, 0xE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012FE4C, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012FCB8, 4, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s5_00_8012FD84, 5, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
};

MenuMapMarker D_map_neo_ark_8017A230[10] = {
    { 1, MENU_MAP_MARKER_AREA_NEVER, 0, 0 },
    { 2, MENU_MAP_MARKER_AREA_ANY, -47, 26 },
    { 1, MENU_MAP_MARKER_AREA_ANY, -47, 59 },
    { 1, MENU_MAP_MARKER_AREA_ANY, -80, 25 },
    { 2, MENU_MAP_MARKER_AREA_ANY, 48, 16 },
    { 3, MENU_MAP_MARKER_AREA_ANY, 109, 0 },
    { 1, MENU_MAP_MARKER_AREA_ANY, 59, -75 },
    { 1, MENU_MAP_MARKER_AREA_ANY, 109, 8 },
    { 3, MENU_MAP_MARKER_AREA_ANY, 30, 0 },
    { 0, 0, 0, 0 },
};

MenuMapIcon gMapNeoArkIcons[4] = {
    { .page = MAP_NEO_ARK_PAGE_SHELTER_1F, .area = GAME_AREA_SHELTER_1F_HELIPORT, .kind = MAP_NEO_ARK_ICON_KIND_FIXED_PICTURE, .condition = 0, .x = -36, .y = -21 },
    { .page = MAP_NEO_ARK_PAGE_SHELTER_1F, .area = GAME_AREA_SHELTER_1F_HELIPORT, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = 0, .x = -51, .y = -16 },
    { .page = MAP_NEO_ARK_PAGE_SHELTER_B6, .area = GAME_AREA_SHELTER_B6_NURSERY, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = 0, .x = 25, .y = -2 },
    { .page = MAP_NEO_ARK_ICON_PAGE_END },
};

MenuMapAreaName D_map_neo_ark_8017A28C[33] = {
    { "Parking garage" },
    { "Vehicular airlock" },
    { "Bulwark" },
    { "Heliport" },
    { "Airlock" },
    { "Guardroom" },
    { "Observatory" },
    { "EVE access tunnel" },
    { "EVE elevator" },
    { "North promenade" },
    { "Forest zone" },
    { "Submarine tunnel" },
    { "Pavilion" },
    { "Island" },
    { "Garden" },
    { "Power plant 2" },
    { "Power plant 1" },
    { "Savanna zone" },
    { "South promenade" },
    { "Altar" },
    { "Shrine" },
    { "Nursery" },
    { "Growth room" },
    { "Corridor" },
    { "Training room" },
    /* The parentheses are Shift-JIS full-width characters. */
    { "Oval Office\201ievening\201j" },
    { "Bridge" },
    { "Tent" },
    { "Jungle zone" },
    { "Submarine gallery" },
    { " " },
    { "Pyramid" },
    { "Substation" },
};

/// Terminator-only spawn recipes shared by seven saved-object lists.
///
/// These entries seed/query saved pickup and UI states without model tasks.
static AreaObjectSpawn _gMapNeoArkEmptyObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

/// Teddy Bear model recipe for the Vehicular Airlock saved-object list.
///
/// Kind matching selects the model task; its room-owned callback hides the
/// model when the saved state is collected (2). Model and callback are borrowed
/// while the vehicular-airlock overlay is loaded. Ends with AREA_OBJECT_SPAWN_END.
static AreaObjectSpawn _gMapNeoArkVehicularAirlockObjectSpawns[2] = {
    { MAP_NEO_ARK_KIND_TEDDY_BEAR, { { { TASK_BODY_TMD, MAP_NEO_ARK_OBJECT_TASK_PRIORITY } }, shelter1fVehicularAirlockUpdatePlacedModelVisibilityTask, { &gShelter1fVehicularAirlockModel03A58 } } },
    { AREA_OBJECT_SPAWN_END },
};

/// Terminator-only spawn recipes for the Heliport saved-object list.
///
/// Its pickups and item boxes have no model task recipes in this table.
static AreaObjectSpawn _gMapNeoArkHeliportObjectSpawns[1] = {
    { AREA_OBJECT_SPAWN_END },
};

AreaObjectRoom gMapNeoArkObjectRooms[35] = {
    [0]                                      = { { .list = NULL }, NULL },
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE]    = { { .list = NULL }, NULL },
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK] = { { .list = _gMapNeoArkVehicularAirlockObjectPlaces }, _gMapNeoArkVehicularAirlockObjectSpawns },
    [GAME_AREA_SHELTER_1F_BULWARK]           = { { .list = NULL }, NULL },
    [GAME_AREA_SHELTER_1F_HELIPORT]          = { { .list = _gMapNeoArkHeliportObjectPlaces }, _gMapNeoArkHeliportObjectSpawns },
    [GAME_AREA_SHELTER_1F_AIRLOCK]           = { { .list = NULL }, NULL },
    [GAME_AREA_SHELTER_1F_GUARDROOM]         = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_OBSERVATORY]          = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL]    = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR]         = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE]      = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_FOREST_ZONE]          = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL]     = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_PAVILION]             = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_ISLAND]               = { { .list = _gMapNeoArkIslandObjectPlaces }, _gMapNeoArkEmptyObjectSpawns },
    [GAME_AREA_NEO_ARK_GARDEN]               = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_POWER_PLANT_2]        = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_POWER_PLANT_1]        = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE]         = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE]      = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_ALTAR]                = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_SHRINE]               = { { .list = _gMapNeoArkNurseryObjectPlaces }, _gMapNeoArkEmptyObjectSpawns },
    [GAME_AREA_SHELTER_B6_NURSERY]           = { { .list = _gMapNeoArkGrowthRoomObjectPlaces }, _gMapNeoArkEmptyObjectSpawns },
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM]       = { { .list = _gMapNeoArkCorridorObjectPlaces }, _gMapNeoArkEmptyObjectSpawns },
    [GAME_AREA_SHELTER_B6_CORRIDOR]          = { { .list = NULL }, NULL },
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM]     = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_R26]                  = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_BRIDGE]               = { { .list = NULL }, NULL },
    [GAME_AREA_SHELTER_1F_TENT]              = { { .list = _gMapNeoArkTentObjectPlaces }, _gMapNeoArkEmptyObjectSpawns },
    [GAME_AREA_NEO_ARK_WOODLAND_PATH]        = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY]    = { { .list = _gMapNeoArkSubmarineGalleryObjectPlaces }, _gMapNeoArkEmptyObjectSpawns },
    [GAME_AREA_NEO_ARK_R31]                  = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_PYRAMID]              = { { .list = NULL }, NULL },
    [GAME_AREA_NEO_ARK_SUBSTATION]           = { { .list = _gMapNeoArkSubstationObjectPlaces }, _gMapNeoArkEmptyObjectSpawns },
    [34]                                     = { { .sentinel = AREA_OBJECT_ROOM_END }, NULL },
};

TaskDesc D_map_neo_ark_8017A804[] = {
    { { { TASK_BODY_NONE, 0x20 } }, shelter1fParkingGarageRoomTask, { .value = GP_TASK_LOC_KEY(5, 1, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelter1fVehicularAirlockRoomTask, { .value = GP_TASK_LOC_KEY(5, 2, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelter1fBulwarkRoomTask, { .value = GP_TASK_LOC_KEY(5, 3, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelter1fAirlockRoomTask, { .value = GP_TASK_LOC_KEY(5, 5, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelter1fGuardroomRoomTask, { .value = GP_TASK_LOC_KEY(5, 6, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkObservatoryRoomTask, { .value = GP_TASK_LOC_KEY(5, 7, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkEveAccessTunnelRoomTask, { .value = GP_TASK_LOC_KEY(5, 8, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkEveElevatorRoomTask, { .value = GP_TASK_LOC_KEY(5, 9, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkNorthPromenadeRoomTask, { .value = GP_TASK_LOC_KEY(5, 10, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkForestZoneRoomTask, { .value = GP_TASK_LOC_KEY(5, 11, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkSubmarineTunnelRoomTask, { .value = GP_TASK_LOC_KEY(5, 12, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkPavilionRoomTask, { .value = GP_TASK_LOC_KEY(5, 13, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkIslandRoomTask, { .value = GP_TASK_LOC_KEY(5, 14, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkGardenRoomTask, { .value = GP_TASK_LOC_KEY(5, 15, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkSavannaZoneRoomTask, { .value = GP_TASK_LOC_KEY(5, 18, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkSouthPromenadeRoomTask, { .value = GP_TASK_LOC_KEY(5, 19, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkAltarRoomTask, { .value = GP_TASK_LOC_KEY(5, 20, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkShrineRoomTask, { .value = GP_TASK_LOC_KEY(5, 21, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB6NurseryRoomTask, { .value = GP_TASK_LOC_KEY(5, 22, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB6GrowthRoomTask, { .value = GP_TASK_LOC_KEY(5, 23, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB6CorridorRoomTask, { .value = GP_TASK_LOC_KEY(5, 24, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB6TrainingRoomRoomTask, { .value = GP_TASK_LOC_KEY(5, 25, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkBridgeRoomTask, { .value = GP_TASK_LOC_KEY(5, 27, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkR26RoomTask, { .value = GP_TASK_LOC_KEY(5, 26, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkSubmarineGalleryMessageTask, { .value = GP_TASK_LOC_KEY(5, 30, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkR31RoomTask, { .value = GP_TASK_LOC_KEY(5, 31, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelter1fTentRoomTask, { .value = GP_TASK_LOC_KEY(5, 28, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkWoodlandPathRoomTask, { .value = GP_TASK_LOC_KEY(5, 29, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkPowerPlant2RoomTask, { .value = GP_TASK_LOC_KEY(5, 16, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkPowerPlant1RoomTask, { .value = GP_TASK_LOC_KEY(5, 17, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelter1fHeliportRoomTask, { .value = GP_TASK_LOC_KEY(5, 4, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkPyramidRoomTask, { .value = GP_TASK_LOC_KEY(5, 32, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkSubstationRoomTask, { .value = GP_TASK_LOC_KEY(5, 33, 0) } },
    { { { TASK_DESC_END, 0x20 } }, NULL, { 0 } },
};

s32 gMapNeoArkCompanionDistressTicks = 0;

u16 D_map_neo_ark_8017A9A0[9] = {
    0x1C4,
    0x9BC,
    0x1B8,
    0x1B7,
    0x1B5,
    0x1B4,
    0x1B2,
    0x9AF,
    0x1AC,
};

/// Room-1 Nursery lighting, borrowing its lights and ambient-by-view table.
///
/// The map owns this single descriptor; its room-owned light resources must
/// stay loaded during lighting queries and coordinate updates.
static WorldCoordRoomLighting _gMapNeoArkNurseryRoomLighting[1] = {
    { .lights = &gShelterB6NurseryRoomLights, .ambientTable = gShelterB6NurseryAmbientByView },
};

/// Room-1 Growth Room lighting, borrowing its lights and ambient-by-view table.
///
/// The map owns this single descriptor; its room-owned light resources must
/// stay loaded during lighting queries and coordinate updates.
static WorldCoordRoomLighting _gMapNeoArkGrowthRoomLighting[1] = {
    { .lights = &gShelterB6GrowthRoomLights, .ambientTable = gShelterB6GrowthRoomAmbientByView },
};

/// Room-1 Corridor lighting, borrowing its lights and ambient-by-view table.
///
/// The map owns this single descriptor; its room-owned light resources must
/// stay loaded during lighting queries and coordinate updates.
static WorldCoordRoomLighting _gMapNeoArkCorridorRoomLighting[1] = {
    { .lights = &gShelterB6CorridorRoomLights, .ambientTable = gShelterB6CorridorRoomAmbientByView },
};

/// Room-1 Training Room lighting, borrowing its lights and ambient-by-view table.
///
/// The map owns this single descriptor; its room-owned light resources must
/// stay loaded during lighting queries and coordinate updates.
static WorldCoordRoomLighting _gMapNeoArkTrainingRoomLighting[1] = {
    { .lights = &gShelterB6TrainingRoomLights, .ambientTable = gShelterB6TrainingRoomAmbientByView },
};

/// Room-1 Bridge lighting, borrowing its lights and using default ambient minima.
///
/// The map owns this single descriptor; its room-owned light resources must
/// stay loaded during lighting queries and coordinate updates.
static WorldCoordRoomLighting _gMapNeoArkBridgeRoomLighting[1] = {
    { .lights = &gNeoArkBridgeRoomLights, .ambientTable = NULL },
};

/// Room-1 Tent lighting, borrowing its lights and using default ambient minima.
///
/// The map owns this single descriptor; its room-owned light resources must
/// stay loaded during lighting queries and coordinate updates.
static WorldCoordRoomLighting _gMapNeoArkTentRoomLighting[1] = {
    { .lights = &gShelter1fTentRoomLights, .ambientTable = NULL },
};

/// Room-1 Woodland Path lighting, borrowing its lights and ambient-by-view table.
///
/// The map owns this single descriptor; its room-owned light resources must
/// stay loaded during lighting queries and coordinate updates.
static WorldCoordRoomLighting _gMapNeoArkWoodlandPathRoomLighting[1] = {
    { .lights = &gNeoArkWoodlandPathRoomLights, .ambientTable = gNeoArkWoodlandPathRoomAmbientByView },
};

/// Room-1 Submarine Gallery lighting, borrowing its lights and using default ambient minima.
///
/// The map owns this single descriptor; its room-owned light resources must
/// stay loaded during lighting queries and coordinate updates.
static WorldCoordRoomLighting _gMapNeoArkSubmarineGalleryRoomLighting[1] = {
    { .lights = &gNeoArkSubmarineGalleryRoomLights, .ambientTable = NULL },
};

/// Room-1 area-31 lighting, borrowing its lights and using default ambient minima.
///
/// The map owns this single descriptor; its room-owned light resources must
/// stay loaded during lighting queries and coordinate updates.
static WorldCoordRoomLighting _gMapNeoArkR31RoomLighting[1] = {
    { .lights = &gNeoArkR31RoomLights, .ambientTable = NULL },
};

WorldCoordRoomLighting* gMapNeoArkRoomLightingTables[33] = {
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE - 1]    = gShelter1fParkingGarageRoomLighting,
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK - 1] = gShelter1fVehicularAirlockRoomLighting,
    [GAME_AREA_SHELTER_1F_BULWARK - 1]           = gShelter1fBulwarkRoomLighting,
    [GAME_AREA_SHELTER_1F_HELIPORT - 1]          = gShelter1fHeliportRoomLighting,
    [GAME_AREA_SHELTER_1F_AIRLOCK - 1]           = gShelter1fAirlockRoomLighting,
    [GAME_AREA_SHELTER_1F_GUARDROOM - 1]         = gShelter1fGuardroomRoomLighting,
    [GAME_AREA_NEO_ARK_OBSERVATORY - 1]          = gNeoArkObservatoryRoomLighting,
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL - 1]    = gNeoArkEveAccessTunnelRoomLighting,
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR - 1]         = gNeoArkEveElevatorRoomLighting,
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE - 1]      = gNeoArkNorthPromenadeRoomLighting,
    [GAME_AREA_NEO_ARK_FOREST_ZONE - 1]          = gNeoArkForestZoneRoomLighting,
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL - 1]     = gNeoArkSubmarineTunnelRoomLighting,
    [GAME_AREA_NEO_ARK_PAVILION - 1]             = gNeoArkPavilionRoomLighting,
    [GAME_AREA_NEO_ARK_ISLAND - 1]               = gNeoArkIslandRoomLighting,
    [GAME_AREA_NEO_ARK_GARDEN - 1]               = gNeoArkGardenRoomLighting,
    [GAME_AREA_NEO_ARK_POWER_PLANT_2 - 1]        = gNeoArkPowerPlant2RoomLighting,
    [GAME_AREA_NEO_ARK_POWER_PLANT_1 - 1]        = gNeoArkPowerPlant1RoomLighting,
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE - 1]         = gNeoArkSavannaZoneRoomLighting,
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE - 1]      = gNeoArkSouthPromenadeRoomLighting,
    [GAME_AREA_NEO_ARK_ALTAR - 1]                = gNeoArkAltarRoomLighting,
    [GAME_AREA_NEO_ARK_SHRINE - 1]               = gNeoArkShrineRoomLighting,
    [GAME_AREA_SHELTER_B6_NURSERY - 1]           = _gMapNeoArkNurseryRoomLighting,
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM - 1]       = _gMapNeoArkGrowthRoomLighting,
    [GAME_AREA_SHELTER_B6_CORRIDOR - 1]          = _gMapNeoArkCorridorRoomLighting,
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM - 1]     = _gMapNeoArkTrainingRoomLighting,
    [GAME_AREA_NEO_ARK_R26 - 1]                  = gNeoArkR26RoomLighting,
    [GAME_AREA_NEO_ARK_BRIDGE - 1]               = _gMapNeoArkBridgeRoomLighting,
    [GAME_AREA_SHELTER_1F_TENT - 1]              = _gMapNeoArkTentRoomLighting,
    [GAME_AREA_NEO_ARK_WOODLAND_PATH - 1]        = _gMapNeoArkWoodlandPathRoomLighting,
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY - 1]    = _gMapNeoArkSubmarineGalleryRoomLighting,
    [GAME_AREA_NEO_ARK_R31 - 1]                  = _gMapNeoArkR31RoomLighting,
    [GAME_AREA_NEO_ARK_PYRAMID - 1]              = gNeoArkPyramidRoomLighting,
    [GAME_AREA_NEO_ARK_SUBSTATION - 1]           = gNeoArkSubstationRoomLighting,
};

DirectionWarpEntry* D_map_neo_ark_8017AA80[33] = {
    gShelter1fParkingGarageWarps,
    gShelter1fVehicularAirlockWarpEntries,
    gShelter1fBulwarkWarpEntries,
    gShelter1fHeliportWarpEntries,
    D_shelter_1f_airlock_8017E5BC,
    gShelter1fGuardroomWarpEntries,
    gNeoArkObservatoryWarpEntries,
    gNeoArkEveAccessTunnelWarpEntries,
    gNeoArkEveElevatorWarpEntries,
    D_neo_ark_north_promenade_80181DD4,
    gNeoArkForestZoneWarpEntries,
    gNeoArkSubmarineTunnelWarpEntries,
    D_neo_ark_pavilion_801838F8,
    D_neo_ark_island_80181BB4,
    gNeoArkGardenWarpEntries,
    gNeoArkPowerPlant2WarpEntries,
    gNeoArkPowerPlant1WarpEntries,
    D_neo_ark_savanna_zone_8017FA04,
    D_neo_ark_south_promenade_8017F70C,
    D_neo_ark_altar_8017F0FC,
    gNeoArkShrineWarpEntries,
    gShelterB6NurseryWarpEntries,
    D_shelter_b6_growth_room_8017F380,
    gShelterB6CorridorWarpEntries,
    D_shelter_b6_training_room_80184420,
    gNeoArkR26Warps,
    gNeoArkBridgeWarpEntries,
    D_shelter_1f_tent_80181D4C,
    gNeoArkWoodlandPathWarpEntries,
    gNeoArkSubmarineGalleryWarpEntries,
    D_neo_ark_r31_8017DA24,
    D_neo_ark_pyramid_8017FC6C,
    D_neo_ark_substation_8017E410,
};

static ViewCount* D_map_neo_ark_8017AB04[33] = {
    gShelter1fParkingGarageViewCounts,
    gShelter1fVehicularAirlockViewCounts,
    gShelter1fBulwarkRoomViewCounts,
    gShelter1fHeliportViewCounts,
    gShelter1fAirlockViewCounts,
    gShelter1fGuardroomRoomViewCounts,
    gNeoArkObservatoryViewCounts,
    gNeoArkEveAccessTunnelViewCounts,
    gNeoArkEveElevatorViewCounts,
    gNeoArkNorthPromenadeViewCounts,
    gNeoArkForestZoneViewCounts,
    gNeoArkSubmarineTunnelViewCounts,
    D_neo_ark_pavilion_801838F4,
    D_neo_ark_island_80181BB0,
    gNeoArkGardenViewCounts,
    gNeoArkPowerPlant2ViewCounts,
    gNeoArkPowerPlant1ViewCounts,
    D_neo_ark_savanna_zone_8017FA00,
    gNeoArkSouthPromenadeViewCounts,
    gNeoArkAltarViewCounts,
    gNeoArkShrineViewCounts,
    gShelterB6NurseryViewCounts,
    gShelterB6GrowthRoomViewCounts,
    gShelterB6CorridorViewCounts,
    D_shelter_b6_training_room_8018441C,
    gNeoArkR26ViewCounts,
    gNeoArkBridgeViewCounts,
    D_shelter_1f_tent_80181D48,
    gNeoArkWoodlandPathViewCounts,
    gNeoArkSubmarineGalleryViewCounts,
    D_neo_ark_r31_8017DA20,
    D_neo_ark_pyramid_8017FC68,
    D_neo_ark_substation_8017E40C,
};

ViewCountTable D_map_neo_ark_8017AB88 = { D_map_neo_ark_8017AB04 };

static WorldCollisionRoomResources D_map_neo_ark_8017AB8C[1] = {
    { &gShelterB6NurseryCollisionGrid, gShelterB6NurseryViewBoundaryTriggers, gShelterB6NurseryActionTriggers, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017AB9C[1] = {
    { &gShelterB6GrowthRoomCollisionGrid, gShelterB6GrowthRoomViewBoundaryTriggers, gShelterB6GrowthRoomActionTriggers, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017ABAC[1] = {
    { &gShelterB6CorridorCollisionGrid, gShelterB6CorridorViewBoundaryTriggers, gShelterB6CorridorActionTriggers, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017ABBC[1] = {
    { &gShelterB6TrainingRoomCollisionGrid, D_shelter_b6_training_room_80185780, D_shelter_b6_training_room_80185A44, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017ABCC[1] = {
    { &gNeoArkBridgeCollisionGrid, gNeoArkBridgeViewBoundaryTriggers, gNeoArkBridgeActionTriggers, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017ABDC[1] = {
    { &gShelter1fTentCollisionGrid, D_shelter_1f_tent_80183A94, D_shelter_1f_tent_80183CF4, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017ABEC[1] = {
    { &gNeoArkWoodlandPathCollisionGrid, gNeoArkWoodlandPathViewBoundaryTriggers, gNeoArkWoodlandPathActionTriggers, gNeoArkWoodlandPathSightOccluders },
};

static WorldCollisionRoomResources D_map_neo_ark_8017ABFC[1] = {
    { &gNeoArkSubmarineGalleryCollisionGrid, gNeoArkSubmarineGalleryViewBoundaryTriggers, gNeoArkSubmarineGalleryActionTriggers, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017AC0C[1] = {
    { NULL, NULL, NULL, NULL },
};

static WorldCollisionRoomResources* D_map_neo_ark_8017AC1C[33] = {
    gShelter1fParkingGarageCollisionRooms,
    gShelter1fVehicularAirlockCollisionResources,
    gShelter1fBulwarkCollisionResources,
    gShelter1fHeliportCollisionResources,
    D_shelter_1f_airlock_8017E59C,
    gShelter1fGuardroomRoomCollisionResources,
    gNeoArkObservatoryRoomCollisionResources,
    gNeoArkEveAccessTunnelCollisionRooms,
    gNeoArkEveElevatorCollisionRooms,
    D_neo_ark_north_promenade_80181DB4,
    gNeoArkForestZoneRoomCollisionResources,
    gNeoArkSubmarineTunnelRoomCollisionResources,
    D_neo_ark_pavilion_801838B4,
    D_neo_ark_island_80181B94,
    gNeoArkGardenCollisionRooms,
    gNeoArkPowerPlant2CollisionRooms,
    gNeoArkPowerPlant1CollisionRooms,
    D_neo_ark_savanna_zone_8017F9E4,
    D_neo_ark_south_promenade_8017F6F4,
    D_neo_ark_altar_8017F094,
    gNeoArkShrineCollisionRooms,
    D_map_neo_ark_8017AB8C,
    D_map_neo_ark_8017AB9C,
    D_map_neo_ark_8017ABAC,
    D_map_neo_ark_8017ABBC,
    gNeoArkR26CollisionRooms,
    D_map_neo_ark_8017ABCC,
    D_map_neo_ark_8017ABDC,
    D_map_neo_ark_8017ABEC,
    D_map_neo_ark_8017ABFC,
    D_map_neo_ark_8017AC0C,
    D_neo_ark_pyramid_8017FC28,
    D_neo_ark_substation_8017E3F0,
};

WorldCollisionStageResources D_map_neo_ark_8017ACA0 = { D_map_neo_ark_8017AC1C };

static ViewCamera* D_map_neo_ark_8017ACA4[33] = {
    gShelter1fParkingGarageViewCameras,
    gShelter1fVehicularAirlockViewCameras,
    gShelter1fBulwarkCameras,
    gShelter1fHeliportViewCameras,
    gShelter1fAirlockViewCameras,
    gShelter1fGuardroomViewCameras,
    gNeoArkObservatoryViewCameras,
    gNeoArkEveAccessTunnelViewCameras,
    gNeoArkEveElevatorViewCameras,
    gNeoArkNorthPromenadeViewCameras,
    gNeoArkForestZoneViewCameras,
    gNeoArkSubmarineTunnelViewCameras,
    gNeoArkPavilionViewCameras,
    gNeoArkIslandViewCameras,
    gNeoArkGardenViewCameras,
    gNeoArkPowerPlant2ViewCameras,
    gNeoArkPowerPlant1ViewCameras,
    gNeoArkSavannaZoneViewCameras,
    gNeoArkSouthPromenadeViewCameras,
    gNeoArkAltarViewCameras,
    gNeoArkShrineViewCameras,
    gShelterB6NurseryViewCameras,
    gShelterB6GrowthRoomViewCameras,
    gShelterB6CorridorViewCameras,
    gShelterB6TrainingRoomViewCameras,
    gNeoArkR26ViewCameras,
    gNeoArkBridgeViewCameras,
    gShelter1fTentViewCameras,
    gNeoArkWoodlandPathViewCameras,
    gNeoArkSubmarineGalleryViewCameras,
    gNeoArkR31ViewCameras,
    gNeoArkPyramidViewCameras,
    gNeoArkSubstationViewCameras,
};

ViewCameraTable D_map_neo_ark_8017AD28 = { D_map_neo_ark_8017ACA4 };

/// Per-area logical-view map directories, selected with area IDs 1..33 minus one.
///
/// All entries borrow the owning room overlay's room directories; their byte
/// maps remain mutable. Room/view IDs are also one-based; no terminators.
static u8** _gMapNeoArkAreaViewMaps[33] = {
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE - 1]    = gShelter1fParkingGarageViewMaps,
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK - 1] = gShelter1fVehicularAirlockViewMaps,
    [GAME_AREA_SHELTER_1F_BULWARK - 1]           = gShelter1fBulwarkRoomViewMaps,
    [GAME_AREA_SHELTER_1F_HELIPORT - 1]          = gShelter1fHeliportViewMaps,
    [GAME_AREA_SHELTER_1F_AIRLOCK - 1]           = gShelter1fAirlockViewMaps,
    [GAME_AREA_SHELTER_1F_GUARDROOM - 1]         = gShelter1fGuardroomViewMaps,
    [GAME_AREA_NEO_ARK_OBSERVATORY - 1]          = gNeoArkObservatoryViewMaps,
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL - 1]    = gNeoArkEveAccessTunnelViewMaps,
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR - 1]         = gNeoArkEveElevatorViewMaps,
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE - 1]      = gNeoArkNorthPromenadeViewMaps,
    [GAME_AREA_NEO_ARK_FOREST_ZONE - 1]          = gNeoArkForestZoneViewMaps,
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL - 1]     = gNeoArkSubmarineTunnelViewMaps,
    [GAME_AREA_NEO_ARK_PAVILION - 1]             = gNeoArkPavilionViewMaps,
    [GAME_AREA_NEO_ARK_ISLAND - 1]               = gNeoArkIslandViewMaps,
    [GAME_AREA_NEO_ARK_GARDEN - 1]               = gNeoArkGardenViewMaps,
    [GAME_AREA_NEO_ARK_POWER_PLANT_2 - 1]        = gNeoArkPowerPlant2ViewMaps,
    [GAME_AREA_NEO_ARK_POWER_PLANT_1 - 1]        = gNeoArkPowerPlant1ViewMaps,
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE - 1]         = gNeoArkSavannaZoneViewMaps,
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE - 1]      = gNeoArkSouthPromenadeViewMaps,
    [GAME_AREA_NEO_ARK_ALTAR - 1]                = gNeoArkAltarViewMaps,
    [GAME_AREA_NEO_ARK_SHRINE - 1]               = gNeoArkShrineViewMaps,
    [GAME_AREA_SHELTER_B6_NURSERY - 1]           = gShelterB6NurseryViewMaps,
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM - 1]       = gShelterB6GrowthRoomViewMaps,
    [GAME_AREA_SHELTER_B6_CORRIDOR - 1]          = gShelterB6CorridorViewMaps,
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM - 1]     = gShelterB6TrainingRoomViewMaps,
    [GAME_AREA_NEO_ARK_R26 - 1]                  = gNeoArkR26ViewMaps,
    [GAME_AREA_NEO_ARK_BRIDGE - 1]               = gNeoArkBridgeViewMaps,
    [GAME_AREA_SHELTER_1F_TENT - 1]              = gShelter1fTentViewMaps,
    [GAME_AREA_NEO_ARK_WOODLAND_PATH - 1]        = gNeoArkWoodlandPathViewMaps,
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY - 1]    = gNeoArkSubmarineGalleryViewMaps,
    [GAME_AREA_NEO_ARK_R31 - 1]                  = gNeoArkR31ViewMaps,
    [GAME_AREA_NEO_ARK_PYRAMID - 1]              = gNeoArkPyramidViewMaps,
    [GAME_AREA_NEO_ARK_SUBSTATION - 1]           = gNeoArkSubstationViewMaps,
};

ViewIndexTable gMapNeoArkViewIndexTable = { .viewMaps = _gMapNeoArkAreaViewMaps };

/// Per-area sprite-view arrays for the 33 Shelter/Neo Ark areas.
///
/// Select area - 1, then mapped view - 1. Every area slot borrows its room
/// overlay's mutable descriptors and source/batch lists; no terminator or
/// counts are stored. The selected room must remain loaded during use.
static SpriteView* _gMapNeoArkAreaSpriteViews[MAP_NEO_ARK_AREA_ID_LIMIT] = {
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE - 1]    = gShelter1fParkingGarageSpriteViews,
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK - 1] = gShelter1fVehicularAirlockSpriteViews,
    [GAME_AREA_SHELTER_1F_BULWARK - 1]           = gShelter1fBulwarkSpriteViews,
    [GAME_AREA_SHELTER_1F_HELIPORT - 1]          = gShelter1fHeliportSpriteViews,
    [GAME_AREA_SHELTER_1F_AIRLOCK - 1]           = gShelter1fAirlockSpriteViews,
    [GAME_AREA_SHELTER_1F_GUARDROOM - 1]         = gShelter1fGuardroomSpriteViews,
    [GAME_AREA_NEO_ARK_OBSERVATORY - 1]          = gNeoArkObservatorySpriteViews,
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL - 1]    = gNeoArkEveAccessTunnelSpriteViews,
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR - 1]         = gNeoArkEveElevatorSpriteViews,
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE - 1]      = gNeoArkNorthPromenadeSpriteViews,
    [GAME_AREA_NEO_ARK_FOREST_ZONE - 1]          = gNeoArkForestZoneSpriteViews,
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL - 1]     = gNeoArkSubmarineTunnelSpriteViews,
    [GAME_AREA_NEO_ARK_PAVILION - 1]             = gNeoArkPavilionSpriteViews,
    [GAME_AREA_NEO_ARK_ISLAND - 1]               = gNeoArkIslandSpriteViews,
    [GAME_AREA_NEO_ARK_GARDEN - 1]               = gNeoArkGardenSpriteViews,
    [GAME_AREA_NEO_ARK_POWER_PLANT_2 - 1]        = gNeoArkPowerPlant2SpriteViews,
    [GAME_AREA_NEO_ARK_POWER_PLANT_1 - 1]        = gNeoArkPowerPlant1SpriteViews,
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE - 1]         = gNeoArkSavannaZoneSpriteViews,
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE - 1]      = gNeoArkSouthPromenadeSpriteViews,
    [GAME_AREA_NEO_ARK_ALTAR - 1]                = gNeoArkAltarSpriteViews,
    [GAME_AREA_NEO_ARK_SHRINE - 1]               = gNeoArkShrineSpriteViews,
    [GAME_AREA_SHELTER_B6_NURSERY - 1]           = gShelterB6NurserySpriteViews,
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM - 1]       = gShelterB6GrowthRoomSpriteViews,
    [GAME_AREA_SHELTER_B6_CORRIDOR - 1]          = gShelterB6CorridorSpriteViews,
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM - 1]     = gShelterB6TrainingRoomSpriteViews,
    [GAME_AREA_NEO_ARK_R26 - 1]                  = gNeoArkR26SpriteViews,
    [GAME_AREA_NEO_ARK_BRIDGE - 1]               = gNeoArkBridgeSpriteViews,
    [GAME_AREA_SHELTER_1F_TENT - 1]              = gShelter1fTentSpriteViews,
    [GAME_AREA_NEO_ARK_WOODLAND_PATH - 1]        = gNeoArkWoodlandPathSpriteViews,
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY - 1]    = gNeoArkSubmarineGallerySpriteViews,
    [GAME_AREA_NEO_ARK_R31 - 1]                  = gNeoArkR31SpriteViews,
    [GAME_AREA_NEO_ARK_PYRAMID - 1]              = gNeoArkPyramidSpriteViews,
    [GAME_AREA_NEO_ARK_SUBSTATION - 1]           = gNeoArkSubstationSpriteViews,
};

SpriteAreaTable gMapNeoArkSpriteAreaTable = { .areaViews = _gMapNeoArkAreaSpriteViews };

WorldCollisionSurfaceProperties** gMapNeoArkSurfacePropertiesByArea[33] = {
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE - 1]    = gShelter1fParkingGarageSurfaceProperties,
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK - 1] = gShelter1fVehicularAirlockSurfaceProperties,
    [GAME_AREA_SHELTER_1F_BULWARK - 1]           = gShelter1fBulwarkSurfaceProperties,
    [GAME_AREA_SHELTER_1F_HELIPORT - 1]          = gShelter1fHeliportSurfacePropertiesByClass,
    [GAME_AREA_SHELTER_1F_AIRLOCK - 1]           = gShelter1fAirlockSurfaceProperties,
    [GAME_AREA_SHELTER_1F_GUARDROOM - 1]         = gShelter1fGuardroomSurfaceProperties,
    [GAME_AREA_NEO_ARK_OBSERVATORY - 1]          = gNeoArkObservatorySurfaceProperties,
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL - 1]    = gNeoArkEveAccessTunnelSurfaceProperties,
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR - 1]         = gNeoArkEveElevatorSurfaceProperties,
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE - 1]      = gNeoArkNorthPromenadeSurfaceProperties,
    [GAME_AREA_NEO_ARK_FOREST_ZONE - 1]          = gNeoArkForestZoneSurfaceProperties,
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL - 1]     = gNeoArkSubmarineTunnelSurfaceProperties,
    [GAME_AREA_NEO_ARK_PAVILION - 1]             = gNeoArkPavilionSurfaceProperties,
    [GAME_AREA_NEO_ARK_ISLAND - 1]               = gNeoArkIslandSurfaceProperties,
    [GAME_AREA_NEO_ARK_GARDEN - 1]               = gNeoArkGardenSurfacePropertiesByClass,
    [GAME_AREA_NEO_ARK_POWER_PLANT_2 - 1]        = gNeoArkPowerPlant2SurfaceProperties,
    [GAME_AREA_NEO_ARK_POWER_PLANT_1 - 1]        = gNeoArkPowerPlant1SurfacePropertiesByClass,
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE - 1]         = gNeoArkSavannaZoneSurfaceProperties,
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE - 1]      = gNeoArkSouthPromenadeSurfaceProperties,
    [GAME_AREA_NEO_ARK_ALTAR - 1]                = gNeoArkAltarSurfaceProperties,
    [GAME_AREA_NEO_ARK_SHRINE - 1]               = gNeoArkShrineSurfaceProperties,
    [GAME_AREA_SHELTER_B6_NURSERY - 1]           = gShelterB6NurserySurfaceProperties,
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM - 1]       = gShelterB6GrowthRoomSurfaceProperties,
    [GAME_AREA_SHELTER_B6_CORRIDOR - 1]          = gShelterB6CorridorSurfaceProperties,
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM - 1]     = gShelterB6TrainingRoomSurfaceProperties,
    [GAME_AREA_NEO_ARK_R26 - 1]                  = gNeoArkR26SurfaceProperties,
    [GAME_AREA_NEO_ARK_BRIDGE - 1]               = gNeoArkBridgeSurfaceProperties,
    [GAME_AREA_SHELTER_1F_TENT - 1]              = gShelter1fTentSurfacePropertiesByClass,
    [GAME_AREA_NEO_ARK_WOODLAND_PATH - 1]        = gNeoArkWoodlandPathSurfaceProperties,
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY - 1]    = gNeoArkSubmarineGallerySurfacePropertiesByClass,
    [GAME_AREA_NEO_ARK_R31 - 1]                  = gNeoArkR31SurfacePropertiesByClass,
    [GAME_AREA_NEO_ARK_PYRAMID - 1]              = gNeoArkPyramidSurfaceProperties,
    [GAME_AREA_NEO_ARK_SUBSTATION - 1]           = gNeoArkSubstationSurfaceProperties,
};

AreaPlacement gMapNeoArkParkingGarageDefaultPlacements[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkVehicularAirlockDefaultPlacements[3] = {
    { MAP_NEO_ARK_RESOURCE_PAWN_BEAM_SWORD, 0, 0, -1700, 0, 0, 0xDAC, 0, 0, 2, 0 },
    { MAP_NEO_ARK_RESOURCE_ROOK_GRENADE_LAUNCHER, 0, 0, -9000, 0, 0, 0x400, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkBulwarkDefaultPlacements[3] = {
    { MAP_NEO_ARK_RESOURCE_ROOK_BEAM_SWORD, 0, 0, -4000, 0, -1000, 0x400, 0, 0, 2, 0 },
    { MAP_NEO_ARK_RESOURCE_ROOK_GRENADE_LAUNCHER, 0, 0, -4000, 0, 1000, 0x400, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkHeliportVariant1Placements[5] = {
    { .entryId = MAP_NEO_ARK_HELIPORT_RESOURCE_JODIE, .variant = 0, .mode = 0, .x = 1715, .y = 0, .z = 6818, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_TENT_SOLDIER_B, .variant = 0, .mode = MAP_NEO_ARK_SOLDIER_CARRIES_RIFLE, .x = 860, .y = 0, .z = 3061, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_TENT_SOLDIER_B, .variant = 0, .mode = MAP_NEO_ARK_SOLDIER_CARRIES_RIFLE, .x = 10000, .y = 0, .z = 4892, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_TENT_SOLDIER_B, .variant = 0, .mode = 0, .x = 7349, .y = 0, .z = -969, .yaw = 512, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkHeliportVariant2Placements[5] = {
    { .entryId = MAP_NEO_ARK_HELIPORT_RESOURCE_RUPERT, .variant = 0, .mode = 0, .x = 1715, .y = 0, .z = 6818, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_TENT_SOLDIER_B, .variant = 0, .mode = MAP_NEO_ARK_SOLDIER_CARRIES_RIFLE, .x = 860, .y = 0, .z = 3061, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_TENT_SOLDIER_B, .variant = 0, .mode = MAP_NEO_ARK_SOLDIER_CARRIES_RIFLE, .x = 10000, .y = 0, .z = 4892, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_TENT_SOLDIER_B, .variant = 0, .mode = 0, .x = 7349, .y = 0, .z = -969, .yaw = 512, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkAirlockKnightPlacements[2] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_KNIGHT_GOLEM, .variant = 0, .mode = 0, .x = -1500, .y = 0, .z = 4800, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkAirlockBishopPlacements[2] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BISHOP_GOLEM, .variant = 0, .mode = 0, .x = -1500, .y = 0, .z = 4800, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkObservatoryDefaultPlacements[2] = {
    { .entryId = MAP_NEO_ARK_OBSERVATORY_RESOURCE_SCENE_STOP, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = -1, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkEveAccessTunnelDefaultPlacements[5] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_2, .mode = 0, .x = -400, .y = -2000, .z = 6400, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_7000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_2, .mode = 0, .x = -2050, .y = -2000, .z = 6400, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_7000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_3, .mode = 0, .x = -400, .y = -2000, .z = 3600, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_7000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_3, .mode = 0, .x = -2050, .y = -2000, .z = 3600, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_7000_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkEveAccessTunnelEncounterPlacements[7] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_EVE_CONTROLLER, .variant = 0, .mode = 1, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 0, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_2, .mode = 0, .x = -400, .y = -2000, .z = 6400, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_7000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_2, .mode = 0, .x = -2050, .y = -2000, .z = 6400, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_7000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_3, .mode = 0, .x = -400, .y = -2000, .z = 3600, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_7000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_3, .mode = 0, .x = -2050, .y = -2000, .z = 3600, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_7000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_1, .mode = 0, .x = -400, .y = -2000, .z = 5000, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_7000_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkNorthPromenadeVariant2Placements[3] = {
    { .entryId = MAP_NEO_ARK_NORTH_PROMENADE_RESOURCE_STINGER, .variant = 0, .mode = 0, .x = 4500, .y = 0, .z = 11500, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_NORTH_PROMENADE_RESOURCE_STINGER, .variant = 0, .mode = 1, .x = 11300, .y = 0, .z = 4500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkNorthPromenadeVariant3Placements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 0, .mode = 0, .x = 9350, .y = 0, .z = 2750, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 5, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 11700, .y = 0, .z = 4000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 5, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 6500, .y = 0, .z = 11200, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkNorthPromenadeVariant11Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_EXTRA_PAWN_GOLEM, .variant = 4, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 9500, .y = 0, .z = 10500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_EXTRA_PAWN_GOLEM, .variant = 0, .mode = 0, .x = 3000, .y = 0, .z = 12000, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkForestZoneVariant1Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_REUSABLE, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_FOREST_ZONE_RESOURCE_EXTRA_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_REUSABLE, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkForestZoneVariant2Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_REUSABLE, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_REUSABLE, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkForestZoneVariant3And4Placements[13] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_REUSABLE, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_REUSABLE, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    [MAP_NEO_ARK_FOREST_ZONE_VARIANT4_PLACEMENT_START] = { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = 0, .x = -3950, .y = 0, .z = 100, .yaw = 1650, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -4450, .y = 0, .z = -900, .yaw = 3250, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -4500, .y = 0, .z = 1000, .yaw = 2650, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -2700, .y = 0, .z = 0, .yaw = 2900, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 700, .y = 0, .z = -900, .yaw = 2450, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = 0, .x = 1050, .y = 0, .z = 600, .yaw = 1200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 1550, .y = 0, .z = -900, .yaw = 500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 2500, .y = 0, .z = 200, .yaw = 2750, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = 0, .x = 2850, .y = 0, .z = 900, .yaw = 1450, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = 0, .x = 4000, .y = 0, .z = -300, .yaw = 800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkForestZoneVariant5Placements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 2, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = -3500, .y = 0, .z = 0, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_WOUNDED, .mode = 4, .x = -5600, .y = 0, .z = -1000, .yaw = 3400, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_WOUNDED, .mode = 4, .x = 1850, .y = 0, .z = 1000, .yaw = 1200, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkForestZoneVariant11Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 5, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 2500, .y = 0, .z = 0, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_EXTRA_PAWN_GOLEM, .variant = 3, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = -4000, .y = 0, .z = 0, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSubmarineTunnelVariant1Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_TUNNEL, .x = -1920, .y = 0, .z = 1792, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_TUNNEL | MAP_NEO_ARK_BOG_DIVER_TUNNEL_CONTINUATION_GROUP, .x = 8000, .y = 0, .z = 6500, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSubmarineTunnelVariant2Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_AMOEBA, .variant = 0, .mode = MAP_NEO_ARK_AMOEBA_ALTERNATE_VARIANT, .x = 500, .y = 3000, .z = 400, .yaw = 3000, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_AMOEBA, .variant = 0, .mode = MAP_NEO_ARK_AMOEBA_ALTERNATE_VARIANT, .x = -250, .y = 3000, .z = -400, .yaw = 3350, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSubmarineTunnelVariant3Placements[3] = {
    { .entryId = MAP_NEO_ARK_SUBMARINE_TUNNEL_RESOURCE_AYA_WALKER, .variant = 0, .mode = 0, .x = 1500, .y = 3000, .z = 0, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_SUBMARINE_TUNNEL_RESOURCE_NO9_WALKER, .variant = 0, .mode = 0, .x = 1500, .y = 3000, .z = 0, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSubmarineTunnelVariant4Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPavilionVariant1Placements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER, .x = 0, .y = 0, .z = 4384, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_200_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER | (1 << MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT), .x = -7040, .y = 0, .z = 9312, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_200_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER | (2 << MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT), .x = -7000, .y = 0, .z = -6176, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_200_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPavilionVariant2Placements[6] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER, .x = 0, .y = 0, .z = 4384, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_220_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER | (1 << MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT), .x = -7040, .y = 0, .z = 9312, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_220_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -300, .y = 0, .z = 9600, .yaw = 2800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -2900, .y = 0, .z = 10800, .yaw = 1550, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 400, .y = 0, .z = 11100, .yaw = 3000, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPavilionVariant3Placements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER, .x = 0, .y = 0, .z = 4384, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_220_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER | (1 << MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT), .x = -7040, .y = 0, .z = 9312, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_220_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER | (2 << MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT), .x = -7000, .y = 0, .z = -6176, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_220_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPavilionVariant4Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER, .x = 0, .y = 0, .z = 4384, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_270_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_MOSSBACK, .variant = 0, .mode = 0, .x = 300, .y = 0, .z = 11000, .yaw = 2600, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPavilionVariant5Placements[5] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WOUNDED_GROUND, .x = 2000, .y = 0, .z = 11000, .yaw = 900, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WOUNDED_FLOAT, .x = -3800, .y = 0, .z = 13000, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WOUNDED_FLOAT, .x = -12000, .y = 0, .z = 9000, .yaw = -300, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WOUNDED_FLOAT, .x = -9000, .y = 0, .z = 11000, .yaw = 600, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPavilionVariant11Placements[2] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_GRENADE_LAUNCHER_ROOK, .variant = 3, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = -500, .y = 0, .z = 9500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkIslandVariant1Placements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER, .x = 0, .y = 0, .z = 4384, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_200_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER | (1 << MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT), .x = 6000, .y = 0, .z = 2500, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_200_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER | (2 << MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT), .x = 5800, .y = 0, .z = 4800, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_BOG_DIVER_SHOT_SPEED_200_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkIslandVariant2Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_MOSSBACK, .variant = 2, .mode = 0, .x = 3000, .y = 0, .z = -5200, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_MOSSBACK, .variant = 1, .mode = 0, .x = 2700, .y = 0, .z = -300, .yaw = 1900, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkIslandVariant3Placements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WOUNDED_GROUND, .x = 3500, .y = -100, .z = -4632, .yaw = 256, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WOUNDED_GROUND, .x = 5100, .y = -100, .z = -4000, .yaw = 2816, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WOUNDED_FLOAT, .x = 990, .y = 0, .z = 2400, .yaw = 256, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkIslandVariant4Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B610[2] = {
    { 0x39, 8, 1, 0x9C4, 0, 0xDAC, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B630[11] = {
    { 0x26, 0, 0, -0x12C, 0, -0x3FAC, 0xD16, 0, 0, 2, 0 },
    { 0x26, 0, 0, -0x2EE, 0, -0x4588, 0xC4E, 0, 0, 2, 0 },
    { 0x26, 0, 0, -0x1CE8, 0, -0x3FAC, 0x12C, 0, 0, 2, 0 },
    { 0x26, 0, 0, -0x1900, 0, -0x4588, 0x190, 0, 0, 2, 0 },
    { 0x26, 0, 1, -0x1F4, 0, -0x3A98, 0x384, 0, 0, 2, 0 },
    { 0x26, 0, 1, -0x578, 0, -0x477C, 0xB54, 0, 0, 2, 0 },
    { 0x26, 0, 1, -0x1676, 0, -0x4556, 0x400, 0, 0, 2, 0 },
    { 0x26, 0, 1, -0x1C20, 0, -0x3908, 0x4B0, 0, 0, 2, 0 },
    { 0x26, 0, 1, -0x157C, 0, -0x3CF0, 0xE10, 0, 0, 2, 0 },
    { 0x26, 0, 1, -0x1900, 0, -0x4268, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B6E0[2] = {
    { 0x84, 0, 0, -0x1D38, 0, -0x44B6, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B700[8] = {
    { 0x31, 0, 0, -0x226, 0, -0x3FAC, 0x9C4, 0, 0, 2, 0 },
    { 0x26, 0, 0xA, -0x2260, -0x5DC, -0x4074, 0xC00, 0, 2, 4, 0 },
    { 0x26, 0, 0xA, -0x1D4C, -0x708, -0x4A38, 0x800, 0, 2, 4, 0 },
    { 0x26, 0, 0, -0x1E78, 0, -0x43C6, 0x960, 0, 2, 4, 0 },
    { 0x26, 0, 0, -0x1BBC, 0, -0x3F16, 0x546, 0, 2, 4, 0 },
    { 0x26, 0, 0, -0x189C, 0, -0x3AFC, 0x546, 0, 2, 4, 0 },
    { 0x26, 0, 0, -0x1FD6, 0, -0x3AFC, 0xE10, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B780[5] = {
    { 0x18, 0, 0, -0xFA0, 0, -0x4650, 0x400, 0, 0, 2, 0 },
    { 0x18, 0, 0, -0x9C4, 0, -0x4650, 0xC00, 0, 0, 2, 0 },
    { 0x18, 0, 0, -0x1964, 0, -0x3CF0, 0x400, 0, 0, 2, 0 },
    { 0x18, 0, 0, -0x1900, 0, -0x4268, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B7D0[8] = {
    { 0x35, 0, 0, 0xFA0, -0x178E, -0x137E, 0, 0, 0, 2, 0 },
    { 0x15, 3, 0, 0x2328, -0x1B58, -0x9C4, 0xC00, 0, 3, 5, 4 },
    { 0x15, 2, 0, 0x2328, -0x1B58, -0x1194, 0xC00, 0, 3, 5, 4 },
    { 0x15, 3, 0, 0x2328, -0x1B58, -0x1964, 0xC00, 0, 3, 5, 4 },
    { 0x15, 2, 0, 0x2328, -0x1B58, -0x2134, 0xC00, 0, 3, 5, 4 },
    { 0x15, 1, 0, 0x2134, -0x1B58, -0x2328, 0, 0, 3, 5, 7 },
    { 0x15, 1, 0, 0x1964, -0x1B58, -0x2328, 0, 0, 3, 5, 7 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B850[6] = {
    { 0x15, 3, 0, 0x2328, -0x1B58, -0x9C4, 0xC00, 0, 3, 5, 4 },
    { 0x15, 1, 0, 0x2328, -0x1B58, -0x1194, 0xC00, 0, 3, 5, 4 },
    { 0x15, 3, 0, 0x2328, -0x1B58, -0x1964, 0xC00, 0, 3, 5, 4 },
    { 0x15, 1, 0, 0x2328, -0x1B58, -0x2134, 0xC00, 0, 3, 5, 4 },
    { 0x39, 5, 1, 0x1D4C, -0x1388, -0xBB8, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B8B0[2] = {
    { 0x27, 0, 0, 0, -0x1388, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B8D0[8] = {
    { 0x36, 0, 0, 0xFA0, -0x406, -0x1F40, 0x800, 0, 0, 2, 0 },
    { 0x15, 3, 0, 0, -0x960, -0x2A94, 0x400, 0, 3, 5, 3 },
    { 0x15, 4, 0, 0, -0x960, -0x27A6, 0x400, 0, 3, 5, 3 },
    { 0x15, 3, 0, 0, -0x960, -0x21CA, 0x400, 0, 3, 5, 3 },
    { 0x15, 4, 0, 0, -0x960, -0x1EDC, 0x400, 0, 3, 5, 3 },
    { 0x15, 3, 0, 0, -0x960, -0x1900, 0x400, 0, 3, 5, 3 },
    { 0x15, 2, 0, 0x258, -0x960, -0xBB8, 0x800, 0, 3, 5, 7 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B950[9] = {
    { 0x26, 0, 0xA, 0, -0x5DC, -0x1900, 0xC00, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0x20D0, -0x1194, -0x222E, 0x960, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0x1B8A, -0x1194, -0x238C, 0x6A4, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0x1DE2, -0x1194, -0x2C88, 0x258, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0x190, -0x1194, -0x2CEC, 0xA8C, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0x320, -0x1194, -0x2904, 0x400, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0x47E, -0x1194, -0x2422, 0x960, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0x12C, -0x1194, -0x2134, 0xED8, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B9E0[8] = {
    { 0x26, 0, 0, 0x320, 0, -0x1388, 0x800, 0, 0, 2, 0 },
    { 0x26, 0, 0, 0x320, 0, -0x1B58, 0x800, 0, 0, 2, 0 },
    { 0x26, 0, 0, 0x320, 0, -0x2328, 0x800, 0, 0, 2, 0 },
    { 0x26, 0, 0, 0x320, 0, -0x2AF8, 0x800, 0, 0, 2, 0 },
    { 0x26, 0, 0, 0xBB8, 0, -0x2AF8, 0x800, 0, 0, 2, 0 },
    { 0x26, 0, 0xA, 0x2BC, -0x514, -0xBB8, 0, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0x384, -0x1194, -0x1A2C, 0x1F4, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BA60[10] = {
    { 0x19, 0, 0, 0x190, 0, -0x2C24, 0x190, 0, 0, 2, 3 },
    { 0x19, 0, 0, 0x3E8, 0, -0x235A, 0x9F6, 0, 0, 2, 3 },
    { 0x19, 0, 0, 0xBB8, 0, -0x2AF8, 0x802, 0, 0, 2, 3 },
    { 0x19, 0, 0, 0x1162, 0, -0x2A30, 0xEA6, 0, 0, 2, 3 },
    { 0x26, 0, 0, 0x12C, 0, -0xCE4, 0xEA6, 0, 2, 4, 0 },
    { 0x26, 0, 0, 0x2BC, 0, -0x12C0, 0x32, 0, 2, 4, 0 },
    { 0x26, 0, 0, 0x12C, 0, -0x170C, 0xE42, 0, 2, 4, 0 },
    { 0x26, 0, 0, 0x4B0, 0, -0x1806, 0xC8, 0, 2, 4, 0 },
    { 0x26, 0, 0, 0x4B0, 0, -0x1B8A, 0x7D0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BB00[7] = {
    { 0x26, 0, 0x14, 0x1F4, -0x1194, -0xD7A, 0xDAC, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0x47E, -0x1194, -0xDDE, 0x1F4, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0x320, -0x1194, -0xF0A, 0, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0x15E, -0x1194, -0x109A, 0xC80, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0x3E8, -0x1194, -0x10CC, 0xED8, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0x44C, -0x1194, -0x1130, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BB70[8] = {
    { 1, 2, 1, 0xCB2, 0x32, 0x960, 0xBB8, 0, 0, 2, 0 },
    { 1, 2, 1, 0x189C, 0x32, 0x4B0, 0x226, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0xA5A, 0x32, 0xA5A, 0x578, 0, 2, 4, 3 },
    { 0x19, 0, 2, 0x834, 0x32, 0x2BC, 0x28A, 0, 2, 4, 3 },
    { 0x19, 0, 2, 0x2CBA, 0x32, 0x9C4, 0xB22, 0, 2, 4, 3 },
    { 0x19, 0, 2, 0x1644, 0x32, 0x9C4, 0x708, 0, 2, 4, 3 },
    { 0x19, 0, 2, 0x92E, 0x32, 0x672, 0x3B6, 0, 2, 4, 3 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BBF0[8] = {
    { 0x26, 0, 1, 0xE10, 0x32, 0xA5A, 0x708, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0x898, 0x32, 0x190, 0x320, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0xF3C, 0x32, 0x12C, 0x76C, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0x1162, 0x32, 0xA5A, -0x320, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0x1612, 0x32, 0x7D0, 0x320, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0x1A90, 0x32, 0, 0x400, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0x21FC, 0x32, 0x514, 0xE10, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BC70[7] = {
    { 1, 2, 1, 0xAF0, 0x32, 0x708, 0x898, 0, 0, 2, 0 },
    { 0x1A, 0, 3, 0x15E0, -0x1388, 0x3E8, 0xE10, 0, 2, 4, 1 },
    { 0x1A, 0, 3, 0x2328, -0x1770, 0x992, 0xAF0, 0, 2, 4, 1 },
    { 0x1A, 0, 3, 0xCB2, -0xE74, 0xA5A, 0x41A, 0, 2, 4, 1 },
    { 0x1A, 0, 3, 0x9F6, -0x1B58, 0x258, 0x320, 0, 2, 4, 1 },
    { 0x1A, 0, 3, 0xC1C, -0x1388, 0x672, 0x41A, 0, 2, 4, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BCE0[7] = {
    { 0x19, 0, 2, 0xDAC, 0, 0x1F4, 0x400, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0xDAC, 0, 0x7D0, 0x400, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0x24B8, 0, 0x1F4, 0xC00, 0, 0, 2, 0 },
    { 0x19, 0, 2, 0x24B8, 0, 0x7D0, 0xC00, 0, 0, 2, 0 },
    { 0x19, 0, 1, 0x16A8, 0, 0x578, 0, 0, 0, 2, 0 },
    { 1, 2, 1, 0xC4E, 0x32, 0x8C0, 0x76C, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BD50[3] = {
    { 0x14, 3, 1, 0x1964, 0, 0x7D0, 0x400, 0, 0, 2, 0 },
    { 0x38, 0, 0, 0xDAC, 0, 0x5DC, 0x400, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BD80[3] = {
    { 0x38, 3, 1, 0x1F40, 0, 0x898, 0x400, 0, 0, 2, 0 },
    { 0x39, 0, 0, 0x1770, 0, 0x12C, 0, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BDB0[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BDC0[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BDD0[7] = {
    { 0x26, 0, 0, 0xFA0, 0, 0x960, 0x400, 0, 0, 2, 0 },
    { 0x26, 0, 0xA, 0x1068, -0x5DC, 0x109A, 0, 0, 0, 2, 0 },
    { 0x26, 0, 0xA, 0x33F4, -0x5DC, 0x1770, 0x400, 0, 0, 2, 0 },
    { 0x26, 0, 0xA, 0x10CC, -0x5DC, 0x258, 0x800, 0, 0, 2, 0 },
    { 0x26, 0, 0xA, 0x2BC, -0x5DC, 0xFA0, 0xC00, 0, 0, 2, 0 },
    { 0x26, 0, 0x14, 0xFA0, -0xBB8, 0x960, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BE40[3] = {
    { 0x38, 5, 1, 0x2AF8, 0, 0xBB8, 0, 0, 0, 2, 0 },
    { 0x38, 0, 0, 0x1388, 0, 0xED8, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BE70[4] = {
    { 0x14, 6, 1, 0x2CEC, 0, 0x9C4, 0xC00, 0, 0, 2, 0 },
    { 0x14, 4, 1, 0x2AF8, 0, 0x2134, 0x800, 0, 0, 2, 0 },
    { 0x17, 0, 0, 0x1388, 0, 0x4B0, 0, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BEB0[4] = {
    { 0x26, 0, 0x1E, 0x222E, -0xFA0, -0x11C6, 0x9C4, 0, 0, 2, 0 },
    { 0x26, 0, 0x1E, 0x21CA, -0xFA0, -0x122A, 0xBB8, 0, 0, 2, 0 },
    { 0x26, 0, 0x1E, 0x2292, -0xFA0, -0x122A, 0xDAC, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BEF0[5] = {
    { 0x19, 0, 0, 0x1B58, 0, 0x5DC, 0, 0, 0, 2, 0 },
    { 0x19, 0, 0, 0x2710, 0, 0x5DC, 0, 0, 0, 2, 0 },
    { 0x19, 0, 0, 0x1B58, 0, 0x1388, 0, 0, 0, 2, 0 },
    { 0x19, 0, 0, 0xFA0, 0, 0x1388, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BF40[5] = {
    { 0x19, 0, 0, 0x1B58, 0, 0x5DC, 0xC00, 0, 0, 2, 0 },
    { 0x19, 0, 0, 0x2710, 0, 0x5DC, 0xC00, 0, 0, 2, 0 },
    { 0x19, 0, 0, 0x1B58, 0, 0x1388, 0xC00, 0, 0, 2, 0 },
    { 0x19, 0, 0, 0xFA0, 0, 0x1388, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017BF90[9] = {
    { 0x19, 0, 0, 0x189C, 0, -0xF0A, 0x190, 0, 0, 2, 3 },
    { 0x19, 0, 0, 0x1FA4, 0, -0xC80, 0xD48, 0, 0, 2, 3 },
    { 0x19, 0, 0, 0x1C84, 0, -0xFA0, 0xFA0, 0, 0, 2, 3 },
    { 0x19, 0, 0, 0x170C, 0, -0x834, 0x5AA, 0, 0, 2, 3 },
    { 0x26, 0, 1, 0x2BC, 0, 0x125C, 0x320, 0, 2, 4, 0 },
    { 0x26, 0, 1, 0x578, 0, 0x578, 0, 0, 2, 4, 0 },
    { 0x26, 0, 1, 0x128E, 0, 0x1644, 0x578, 0, 2, 4, 0 },
    { 0x26, 0, 1, 0xAF0, 0, 0x11C6, 0x190, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C020[3] = {
    { 0x14, 0, 0, 0x1B58, 0, -0xFA0, 0, 0, 0, 2, 0 },
    { 0x14, 3, 1, 0x3E8, 0, 0x1388, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C050[2] = {
    { 0x16, 0, 0, 0x1B58, 0, 0x5DC, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkShrineVariant11Placements[2] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BISHOP_GOLEM, .variant = 0, .mode = 0, .x = 7000, .y = 0, .z = 1500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkNurseryDefaultPlacements[5] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_KYLE, .variant = MAP_NEO_ARK_NURSERY_BODY_VISIBLE, .mode = 0, .x = 5950, .y = 0, .z = 2400, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = -1, .clutRowOffset = 16, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 0, .mode = 0, .x = 5950, .y = 0, .z = 5400, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 1, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 0, .mode = 0, .x = 5950, .y = 0, .z = 5400, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 1, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_NURSERY_RESOURCE_SECOND_BODY, .variant = MAP_NEO_ARK_NURSERY_BODY_HIDDEN, .mode = 0, .x = 5950, .y = 0, .z = 2400, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = -1, .clutRowOffset = 16, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkGrowthRoomDefaultPlacements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkCorridorDefaultPlacements[5] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_MASKED_EVE, .variant = 0, .mode = 0, .x = 8000, .y = 0, .z = 0, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 6, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_MOSSBACK, .variant = 1, .mode = 0, .x = 6500, .y = 0, .z = 300, .yaw = 2700, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_EVE_CONTROLLER, .variant = 0, .mode = 2, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 0, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_EVE_BODY, .variant = 0, .mode = 0, .x = 8000, .y = 0, .z = 0, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 6, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkTrainingRoomDefaultPlacements[6] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_MASKED_EVE, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_TRAINING_ROOM_RESOURCE_KYLE, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 6, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_STINGER, .variant = 0, .mode = 0, .x = 2500, .y = 0, .z = 8500, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_EVE_CONTROLLER, .variant = 0, .mode = 3, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 0, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_EVE_BODY, .variant = 0, .mode = 1, .x = 4500, .y = 0, .z = 9600, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

static AreaPlacement D_map_neo_ark_8017C1A0[3] = {
    { 0x6F, 0, 0, 0, 0, 0xB22, 0x800, 0, 0, 2, 0 },
    { 0x70, 0, 0, 0, 0, 0x352, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkBridgeVariant1Placements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER, .x = 0, .y = 0, .z = 4384, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER | (1 << MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT), .x = -7040, .y = 0, .z = -10000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER | (2 << MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT), .x = -7000, .y = 0, .z = 2000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkBridgeVariant2Placements[6] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER, .x = 0, .y = 0, .z = 4384, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER | (1 << MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT), .x = -7040, .y = 0, .z = -10000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -3150, .y = 0, .z = -7200, .yaw = 800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -2500, .y = 0, .z = -5850, .yaw = 2200, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -1350, .y = 0, .z = -1150, .yaw = 3400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkBridgeVariant3Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER, .x = 0, .y = 0, .z = 4384, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 6 },
    { .entryId = MAP_NEO_ARK_RESOURCE_MOSSBACK, .variant = 1, .mode = 0, .x = -3100, .y = 0, .z = -1500, .yaw = 1500, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkBridgeVariant4Placements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER, .x = 0, .y = 0, .z = 4384, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER | (1 << MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT), .x = -7040, .y = 0, .z = -10000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = MAP_NEO_ARK_RESOURCE_MOSSBACK, .variant = 1, .mode = 0, .x = -3100, .y = 0, .z = -8000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkBridgeVariant5Placements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WOUNDED_GROUND, .x = -2000, .y = -100, .z = -1700, .yaw = 3328, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WOUNDED_FLOAT, .x = -800, .y = 0, .z = -7100, .yaw = 200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WOUNDED_FLOAT, .x = 0, .y = 0, .z = 3500, .yaw = 1200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkBridgeVariant11Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BEAM_SWORD_ROOK, .variant = 6, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = -3000, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BEAM_SWORD_ROOK, .variant = 8, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = -3000, .y = 0, .z = -3000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkBridgeVariant12Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BEAM_SWORD_ROOK, .variant = 6, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = -3000, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BEAM_SWORD_ROOK, .variant = 8, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = -3000, .y = 0, .z = -3000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkTentDefaultPlacements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_TENT_SOLDIER_C, .variant = 0, .mode = 0, .x = -2100, .y = 0, .z = 4600, .yaw = -400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_TENT_SOLDIER_B, .variant = 0, .mode = 0, .x = -250, .y = 0, .z = 4500, .yaw = -1024, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_TENT_SOLDIER_A, .variant = 0, .mode = 0, .x = -4280, .y = 0, .z = 8350, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 4, .clutRowOffset = 6, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkWoodlandPathVariant1Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_REUSABLE, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_REUSABLE, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkWoodlandPathVariant2Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_REUSABLE, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_REUSABLE, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkWoodlandPathVariant3Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_REUSABLE, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_REUSABLE, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkWoodlandPathVariant4Placements[5] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_WOUNDED, .mode = 4, .x = 3800, .y = 0, .z = 4700, .yaw = 600, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_WOUNDED, .mode = 4, .x = 2300, .y = 0, .z = -3200, .yaw = 1700, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_WOUNDED, .mode = 4, .x = 2700, .y = 0, .z = -4200, .yaw = 3700, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_WOUNDED, .mode = 4, .x = -200, .y = 0, .z = -4300, .yaw = 1400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSubmarineGalleryVariant1Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_SEA_DIVER, .variant = 0, .mode = 0, .x = 0, .y = 8000, .z = 5000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = MAP_NEO_ARK_BOG_DIVER_REJECT_SPAWN, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_DIVING, .x = 0, .y = 8000, .z = 5000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSubmarineGalleryVariant2Placements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER, .x = 0, .y = 8000, .z = 0, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER | (1 << MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT), .x = 0, .y = 8000, .z = 0, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER | (2 << MAP_NEO_ARK_BOG_DIVER_WAYPOINT_SET_SHIFT), .x = 0, .y = 8000, .z = 0, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSubmarineGalleryVariant3Placements[2] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WOUNDED_FLOAT, .x = 0, .y = 5000, .z = 0, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSubmarineGalleryVariant11Placements[2] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BOG_DIVER, .variant = 0, .mode = MAP_NEO_ARK_BOG_DIVER_MODE_WATER, .x = 0, .y = 8000, .z = 0, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 7 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkR31DefaultPlacements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_KYLE, .variant = 0, .mode = 0, .x = 8270, .y = 840, .z = 7330, .yaw = -1480, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_R31_AYA, .variant = 0, .mode = 0, .x = 6940, .y = 980, .z = 7110, .yaw = -1820, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPyramidVariant1Placements[10] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL, .x = 7000, .y = -1000, .z = -5500, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL, .x = 6500, .y = -1800, .z = -6400, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL, .x = 7000, .y = -1000, .z = -10200, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL, .x = 6000, .y = -1750, .z = -8500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL, .x = 4350, .y = -900, .z = -8500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 3500, .y = 0, .z = -9700, .yaw = 1250, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -700, .y = 0, .z = -7200, .yaw = 3700, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 100, .y = 0, .z = -6700, .yaw = 3500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 1500, .y = 0, .z = -5500, .yaw = 2650, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPyramidVariant2Placements[7] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_LEADER, .x = -1300, .y = -4000, .z = -4200, .yaw = 2200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 3 },
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_FOLLOWER, .x = -2700, .y = -7000, .z = -3250, .yaw = 2000, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_LEADER, .x = -2450, .y = -4500, .z = -5400, .yaw = 600, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_FOLLOWER, .x = 600, .y = -5000, .z = -5650, .yaw = 3200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_FOLLOWER, .x = 400, .y = -3800, .z = -7150, .yaw = 3650, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 2 },
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_FOLLOWER, .x = -950, .y = -6000, .z = -7000, .yaw = 200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 1 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPyramidVariant3Placements[9] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -500, .y = 0, .z = -6000, .yaw = 1250, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -300, .y = 0, .z = -7650, .yaw = 3100, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 4000, .y = 0, .z = -5600, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 5000, .y = 0, .z = -9500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 400, .y = 0, .z = -5100, .yaw = 3650, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_FOLLOWER, .x = -1300, .y = -4000, .z = -4200, .yaw = 2200, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 3 },
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_FOLLOWER, .x = -2450, .y = -4800, .z = -5400, .yaw = 600, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 1 },
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_LEADER, .x = 400, .y = -3500, .z = -7150, .yaw = 3650, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPyramidVariant4Placements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 0, .mode = 0, .x = 2350, .y = 0, .z = -5850, .yaw = 2900, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 2, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 1050, .y = 0, .z = -9100, .yaw = 3900, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_HORNED_STRANGER, .variant = MAP_NEO_ARK_HORNED_STRANGER_WOUNDED, .mode = 4, .x = -3400, .y = 0, .z = -4200, .yaw = 2000, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPyramidVariant11Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BEAM_SWORD_ROOK, .variant = 3, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 1250, .y = 0, .z = -6000, .yaw = 3300, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_GRENADE_LAUNCHER_ROOK, .variant = 4, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = -1500, .y = 0, .z = -6350, .yaw = 3850, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Vehicular Airlock Teddy Bear saved-state slot and model transform.
///
/// Initial state 1 is available; AREA_OBJECT_PLACE_PROMPT requests immediate
/// collection. State bit 8 is retained with no established purpose. XYZ are
/// world units and yaw is in 4096-unit turns. Ends with AREA_OBJECT_PLACE_END.
static AreaObjectPlace _gMapNeoArkVehicularAirlockObjectPlaces[2] = {
    { MAP_NEO_ARK_OBJECT_SLOT_VEHICULAR_AIRLOCK_TEDDY_BEAR, MAP_NEO_ARK_KIND_TEDDY_BEAR, 0, 0x100 | AREA_OBJECT_PLACE_PROMPT | MAP_NEO_ARK_STATE_AVAILABLE, -7201, 2, 1156, 0x4F0 },
    { AREA_OBJECT_PLACE_END },
};

/// Heliport saved-state slots for four pickups, four item boxes and five ammo supplies.
///
/// Ammunition supplies are replenishable (state 3); other entries start
/// available (state 1). The letter requests immediate collection.
/// Ends with AREA_OBJECT_PLACE_END.
static AreaObjectPlace _gMapNeoArkHeliportObjectPlaces[14] = {
    { MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_DOUGLAS_LETTER, MAP_NEO_ARK_KIND_DOUGLAS_LETTER, 0, AREA_OBJECT_PLACE_PROMPT | MAP_NEO_ARK_STATE_AVAILABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_MP_BOOST_2, MAP_NEO_ARK_KIND_MP_BOOST_2, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_44_MAEDA_SP, MAP_NEO_ARK_KIND_44_MAEDA_SP, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_MONGOOSE, MAP_NEO_ARK_KIND_MONGOOSE, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_FIRST_ITEM_BOX, MAP_NEO_ARK_KIND_HELIPORT_FIRST_ITEM_BOX, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_SECOND_ITEM_BOX, MAP_NEO_ARK_KIND_HELIPORT_SECOND_ITEM_BOX, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_THIRD_ITEM_BOX, MAP_NEO_ARK_KIND_HELIPORT_THIRD_ITEM_BOX, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_FOURTH_ITEM_BOX, MAP_NEO_ARK_KIND_HELIPORT_FOURTH_ITEM_BOX, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_9MM_PARABELLUM, MAP_NEO_ARK_KIND_9MM_PARABELLUM, 0, MAP_NEO_ARK_STATE_REPLENISHABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_9MM_HYDRA, MAP_NEO_ARK_KIND_9MM_HYDRA, 0, MAP_NEO_ARK_STATE_REPLENISHABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_BUCKSHOT, MAP_NEO_ARK_KIND_BUCKSHOT, 0, MAP_NEO_ARK_STATE_REPLENISHABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_GRENADE, MAP_NEO_ARK_KIND_GRENADE, 0, MAP_NEO_ARK_STATE_REPLENISHABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_HELIPORT_556_RIFLE, MAP_NEO_ARK_KIND_556_RIFLE, 0, MAP_NEO_ARK_STATE_REPLENISHABLE },
    { AREA_OBJECT_PLACE_END },
};

/// Island Lipstick pickup saved-state slot, initially available.
///
/// Ends with AREA_OBJECT_PLACE_END; no model spawn recipe is supplied.
static AreaObjectPlace _gMapNeoArkIslandObjectPlaces[2] = {
    { MAP_NEO_ARK_OBJECT_SLOT_ISLAND_LIPSTICK, MAP_NEO_ARK_KIND_LIPSTICK, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { AREA_OBJECT_PLACE_END },
};

/// Nursery MP Boost2 pickup saved-state slot, initially available.
///
/// Ends with AREA_OBJECT_PLACE_END; no model spawn recipe is supplied.
static AreaObjectPlace _gMapNeoArkNurseryObjectPlaces[2] = {
    { MAP_NEO_ARK_OBJECT_SLOT_NURSERY_MP_BOOST_2, MAP_NEO_ARK_KIND_MP_BOOST_2, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { AREA_OBJECT_PLACE_END },
};

/// Growth Room save-point, Ringer's Solution and MD Player saved-state slots.
///
/// All start available; ends with AREA_OBJECT_PLACE_END. No model recipes.
static AreaObjectPlace _gMapNeoArkGrowthRoomObjectPlaces[4] = {
    { MAP_NEO_ARK_OBJECT_SLOT_GROWTH_ROOM_SAVE_POINT, MAP_NEO_ARK_KIND_GROWTH_ROOM_SAVE_POINT, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_GROWTH_ROOM_RINGERS_SOLUTION, MAP_NEO_ARK_KIND_RINGERS_SOLUTION, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_GROWTH_ROOM_MD_PLAYER, MAP_NEO_ARK_KIND_MD_PLAYER, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { AREA_OBJECT_PLACE_END },
};

/// Corridor Protein Capsule pickup saved-state slot, initially available.
///
/// Ends with AREA_OBJECT_PLACE_END; no model spawn recipe is supplied.
static AreaObjectPlace _gMapNeoArkCorridorObjectPlaces[2] = {
    { MAP_NEO_ARK_OBJECT_SLOT_CORRIDOR_PROTEIN_CAPSULE, MAP_NEO_ARK_KIND_PROTEIN_CAPSULE, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { AREA_OBJECT_PLACE_END },
};

/// Tent save-point, Airburst and Ringer's Solution saved-state slots.
///
/// All start available; ends with AREA_OBJECT_PLACE_END. No model recipes.
static AreaObjectPlace _gMapNeoArkTentObjectPlaces[4] = {
    { MAP_NEO_ARK_OBJECT_SLOT_TENT_SAVE_POINT, MAP_NEO_ARK_KIND_TENT_SAVE_POINT, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_TENT_AIRBURST, MAP_NEO_ARK_KIND_AIRBURST, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { MAP_NEO_ARK_OBJECT_SLOT_TENT_RINGERS_SOLUTION, MAP_NEO_ARK_KIND_RINGERS_SOLUTION, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { AREA_OBJECT_PLACE_END },
};

/// Submarine Gallery Skull Crystal pickup saved-state slot, initially available.
///
/// Ends with AREA_OBJECT_PLACE_END; no model spawn recipe is supplied.
static AreaObjectPlace _gMapNeoArkSubmarineGalleryObjectPlaces[2] = {
    { MAP_NEO_ARK_OBJECT_SLOT_SUBMARINE_GALLERY_SKULL_CRYSTAL, MAP_NEO_ARK_KIND_SKULL_CRYSTAL, 0, MAP_NEO_ARK_STATE_AVAILABLE },
    { AREA_OBJECT_PLACE_END },
};

/// Substation replenishable 9mm Hydra saved-state slot.
///
/// Starts at state 3 and remains replenishable after collection. Ends with
/// AREA_OBJECT_PLACE_END; no model spawn recipe is supplied.
static AreaObjectPlace _gMapNeoArkSubstationObjectPlaces[2] = {
    { MAP_NEO_ARK_OBJECT_SLOT_SUBSTATION_9MM_HYDRA, MAP_NEO_ARK_KIND_9MM_HYDRA, 0, MAP_NEO_ARK_STATE_REPLENISHABLE },
    { AREA_OBJECT_PLACE_END },
};

InventoryBattleReward D_map_neo_ark_8017C9B0[29] = {
    { GAME_LOCATION_KEY(5, 2, 1, 0), { 0xAA, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(5, 3, 1, 0), { 0xAA, 0, 0, 0xAF } },
    { GAME_LOCATION_KEY(5, 5, 1, 0), { 0xAE, 0, 0, 0x3D } },
    { GAME_LOCATION_KEY(5, 10, 3, 0), { 0xA2, 0, 0, 0xAF } },
    { GAME_LOCATION_KEY(5, 11, 1, 0), { 3, 0, 0, 7 } },
    { GAME_LOCATION_KEY(5, 11, 5, 0), { 0xA2, 0, 0, 0 } },
    { GAME_LOCATION_KEY(5, 12, 2, 0), { 0x3E, 0, 0, 0 } },
    { GAME_LOCATION_KEY(5, 16, 1, 0), { 0x3D, 0, 0, 7 } },
    { GAME_LOCATION_KEY(5, 17, 1, 0), { 3, 0, 0, 7 } },
    { GAME_LOCATION_KEY(5, 18, 5, 0), { 0xAB, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(5, 19, 5, 0), { 0xAB, 0, 0, 0xA9 } },
    { GAME_LOCATION_KEY(5, 21, 1, 0), { 2, 0, 0, 7 } },
    { GAME_LOCATION_KEY(5, 21, 5, 0), { 0xA2, 0, 0, 0xAF } },
    { GAME_LOCATION_KEY(5, 24, 1, 0), { 7, 0, 0, 3 } },
    { GAME_LOCATION_KEY(5, 25, 1, 0), { 7, 0x3E, 0, 0x3D } },
    { GAME_LOCATION_KEY(5, 30, 1, 0), { 0x3C, 0, 0, 7 } },
    { GAME_LOCATION_KEY(5, 30, 2, 0), { 0x3D, 0, 0, 0 } },
    { GAME_LOCATION_KEY(5, 32, 4, 0), { 0xA2, 0, 0, 0xAF } },
    { GAME_LOCATION_KEY(5, 10, 11, 0), { 0xAB, 0, 0, 0xA9 } },
    { GAME_LOCATION_KEY(5, 11, 11, 0), { 0xAB, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(5, 13, 11, 0), { 0xAA, 0, 0, 0 } },
    { GAME_LOCATION_KEY(5, 14, 11, 0), { 0xAA, 0, 0, 0 } },
    { GAME_LOCATION_KEY(5, 16, 11, 0), { 0xAA, 0, 0, 0 } },
    { GAME_LOCATION_KEY(5, 18, 11, 0), { 0xAA, 0, 0, 0xAB } },
    { GAME_LOCATION_KEY(5, 19, 11, 0), { 0xAF, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(5, 21, 11, 0), { 0x3C, 0, 0, 0 } },
    { GAME_LOCATION_KEY(5, 27, 11, 0), { 0xAF, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(5, 32, 11, 0), { 0xAA, 0, 0, 0xAF } },
    { INVENTORY_BATTLE_REWARD_LIST_END },
};

InventoryBattleReward D_map_neo_ark_8017CB0C[6] = {
    { GAME_LOCATION_KEY(5, 5, 7, 0), { 0x3C, 0, 0, 0 } },
    { GAME_LOCATION_KEY(5, 21, 7, 0), { 0x44, 0xAF, 0, 3 } },
    { GAME_LOCATION_KEY(5, 25, 1, 0), { 1, 0, 0, 0x3E } },
    { GAME_LOCATION_KEY(5, 30, 1, 0), { 0xAD, 0, 0, 0xAE } },
    { GAME_LOCATION_KEY(5, 16, 17, 0), { 0x90, 0xAF, 0xA7, 0xB } },
    { INVENTORY_BATTLE_REWARD_LIST_END },
};

StageMusicEntry gMapNeoArkAreaMusicEntries[(MAP_NEO_ARK_AREA_ID_LIMIT + 1) * MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = {
    [0 * MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_SHELTER_1F_PARKING_GARAGE *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_SHELTER_1F_BULWARK *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_SHELTER_1F_HELIPORT *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_SHELTER_1F_AIRLOCK *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_SHELTER_1F_GUARDROOM *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_OBSERVATORY *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = 0x49, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x49, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x49, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x49, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_EVE_ELEVATOR *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_NORTH_PROMENADE *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_FOREST_ZONE *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_PAVILION *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_ISLAND *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_GARDEN *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_POWER_PLANT_2 *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_POWER_PLANT_1 *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_SAVANNA_ZONE *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4C, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_ALTAR *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_SHRINE *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_SHELTER_B6_NURSERY *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x55, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x43, .startMode = STAGE_MUSIC_START_DEFERRED },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_SHELTER_B6_GROWTH_ROOM *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x43, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_SHELTER_B6_CORRIDOR *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x49, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x49, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x49, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x49, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_SHELTER_B6_TRAINING_ROOM *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_R26 *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x56, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_BRIDGE *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_SHELTER_1F_TENT *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x3B, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_WOODLAND_PATH *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_R31 *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_PYRAMID *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4E, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },

    [GAME_AREA_NEO_ARK_SUBSTATION *
        MAP_NEO_ARK_MUSIC_COLUMN_COUNT] = { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x4F, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x53, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x59, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
};

StageMusicEntry gMapNeoArkCountdownMusicEntries[20] = {
    { .sequenceId = 0x44, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0x4F, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0x49, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = STAGE_MUSIC_NO_SEQUENCE, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0x5D, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0x52, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0x45, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0x46, .startMode = STAGE_MUSIC_START_IMMEDIATE },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
    { .sequenceId = 0, .startMode = STAGE_MUSIC_START_WHEN_VIEW_READY },
};
