#include "mapui/map_neo_ark.h"

#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/battle_reward.h"
#include "gameplay/direction.h"
#include "gameplay/inventory.h"
#include "gameplay/map.h"
#include "gameplay/object_task.h"
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

/// Catalog and UI kinds stored by this stage's saved-object lists.
///
/// Banks 0/1 are catalog IDs; banks 7/8 select item-box and save-point UI.
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
            frameBytes                   = gStreamMovieWidthPixels * gStreamMovieHeightRows * 2;
            gMdecMovieExpandedBuffers[0] = (u_long*)((u8*)gStreamMovieRingBuffer + MAP_NEO_ARK_MOVIE_RING_BYTES);
            gMdecMovieExpandedBuffers[1] = gStreamMovieWorkspace;
            gMdecMovieOutputBuffers[0]   = (u_long*)((u8*)gStreamMovieWorkspace + frameBytes);
            gMdecMovieOutputBuffers[1]   = (u_long*)((u8*)gMdecMovieOutputBuffers[0] + frameBytes);
            break;
        case GAME_AREA_NEO_ARK_EVE_ELEVATOR:
            queue->movieVramStaging        = 1;
            queue->movieStagingX           = MAP_NEO_ARK_MOVIE_ELEVATOR_STAGING_X_WORDS;
            queue->movieStagingY           = MAP_NEO_ARK_MOVIE_ELEVATOR_STAGING_Y_ROWS;
            gStreamMovieBackgroundPlayback = true;
            gMdecMovieExpandedBuffers[0]   = (u_long*)((u8*)gStreamMovieRingBuffer + MAP_NEO_ARK_MOVIE_RING_BYTES);
            gMdecMovieOutputBuffers[1]     = gStreamMovieWorkspace;
            gMdecMovieOutputBuffers[0]     = gStreamMovieWorkspace;
            gMdecMovieExpandedBuffers[1]   = (u_long*)((u8*)gMdecMovieExpandedBuffers[0] + gStreamMovieWidthPixels * gStreamMovieHeightRows);
            gGameSession->field_80         = 0;
            queue->field_24A               = 1;
            break;
    }
    gStreamMovieBufferLayoutEnd = (u8*)gMdecMovieOutputBuffers[1] + gStreamMovieWidthPixels * gStreamMovieHeightRows * 2;
    gGameSession->field_7C      = 0;
    gGameSession->field_7E      = 0;
}

/// Binds the room-variant fragment to this overlay's cross-room export.
///
/// The replacement is a function identifier with the `RoomVariantResolver`
/// signature. The binding applies only to the included definition.
#define ROOM_VARIANT_RESOLVE_NEO_ARK mapNeoArkResolveRoomVariant
#include "../../shared/room/room_variants_neo_ark.inc.c"
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
static inline s32 _mapNeoArkApplyAreaMusicVolume(LinInterp* songRamp)
{
    u32 masterGain;
    u32 volumeProduct;

    masterGain    = (u8)midiGetMasterVolume();
    volumeProduct = masterGain * gSndVolumeNeoArkLevel;
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
        if (gSndVolumeNeoArkDuckUpdates == 0) {
            gSndVolumeNeoArkDuckUpdates    = 1;
            gSndVolumeNeoArkRestoreUpdates = 0;
            gSndVolumeNeoArkLevel          = fullVolume;
        } else if (gSndVolumeNeoArkDuckUpdates < MAP_NEO_ARK_MUSIC_DUCK_WAIT_UPDATES) {
            gSndVolumeNeoArkDuckUpdates = gSndVolumeNeoArkDuckUpdates + 1;
        } else if ((fullVolume >> 1) < (u32)gSndVolumeNeoArkLevel) {
            gSndVolumeNeoArkLevel = gSndVolumeNeoArkLevel - MAP_NEO_ARK_MUSIC_VOLUME_STEP;
        } else {
            gSndVolumeNeoArkDuckUpdates    = MAP_NEO_ARK_MUSIC_RAMP_SETTLED;
            gSndVolumeNeoArkRestoreUpdates = 0;
            gSndVolumeNeoArkLevel          = fullVolume >> 1;
        }
        volume = _mapNeoArkApplyAreaMusicVolume(ramp);
    } else if (areaId == GAME_AREA_NEO_ARK_POWER_PLANT_2) {
        if (gSndVolumeNeoArkRestoreUpdates == 0) {
            gSndVolumeNeoArkDuckUpdates    = 0;
            gSndVolumeNeoArkRestoreUpdates = 1;
            gSndVolumeNeoArkLevel          = fullVolume >> 1;
        } else if (gSndVolumeNeoArkRestoreUpdates < MAP_NEO_ARK_MUSIC_RESTORE_WAIT_UPDATES) {
            gSndVolumeNeoArkRestoreUpdates = gSndVolumeNeoArkRestoreUpdates + 1;
        } else if ((u32)gSndVolumeNeoArkLevel < fullVolume) {
            gSndVolumeNeoArkLevel = gSndVolumeNeoArkLevel + MAP_NEO_ARK_MUSIC_VOLUME_STEP;
        } else {
            gSndVolumeNeoArkDuckUpdates    = 0;
            gSndVolumeNeoArkRestoreUpdates = MAP_NEO_ARK_MUSIC_RAMP_SETTLED;
            gSndVolumeNeoArkLevel          = fullVolume;
        }
        volume = _mapNeoArkApplyAreaMusicVolume(ramp);
    } else {
        scaledVolume                   = midiGetMasterVolume() & 0xFF;
        scaledVolume                   = scaledVolume * fullVolume;
        volume                         = linInterpApply(ramp, scaledVolume / (u32)MAP_NEO_ARK_MIDI_GAIN_FULL);
        gSndVolumeNeoArkDuckUpdates    = 0;
        gSndVolumeNeoArkRestoreUpdates = 0;
    }
    return volume;
}

GfxImageSlot gMapNeoArkImageSlotsByArea[34] = {
    [0]                                      = GRAPHICS_IMAGE_SLOT(0x3A8E0),
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE]    = GRAPHICS_IMAGE_SLOT(0x54B20),
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK] = GRAPHICS_IMAGE_SLOT(0x53DB0),
    [GAME_AREA_SHELTER_1F_BULWARK]           = GRAPHICS_IMAGE_SLOT(0x55640),
    [GAME_AREA_SHELTER_1F_HELIPORT]          = GRAPHICS_IMAGE_SLOT(0x4F560),
    [GAME_AREA_SHELTER_1F_AIRLOCK]           = GRAPHICS_IMAGE_SLOT(0x56AA0),
    [GAME_AREA_SHELTER_1F_GUARDROOM]         = GRAPHICS_IMAGE_SLOT(0x58820),
    [GAME_AREA_NEO_ARK_OBSERVATORY]          = GRAPHICS_IMAGE_SLOT(0x4A320),
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL]    = GRAPHICS_IMAGE_SLOT(0x550A0),
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR]         = GRAPHICS_IMAGE_SLOT(0x58F40),
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE]      = GRAPHICS_IMAGE_SLOT(0x52360),
    [GAME_AREA_NEO_ARK_FOREST_ZONE]          = GRAPHICS_IMAGE_SLOT(0x53320),
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL]     = GRAPHICS_IMAGE_SLOT(0x4B7D0),
    [GAME_AREA_NEO_ARK_PAVILION]             = GRAPHICS_IMAGE_SLOT(0x4DDF0),
    [GAME_AREA_NEO_ARK_ISLAND]               = GRAPHICS_IMAGE_SLOT(0x524B0),
    [GAME_AREA_NEO_ARK_GARDEN]               = GRAPHICS_IMAGE_SLOT(0x52610),
    [GAME_AREA_NEO_ARK_POWER_PLANT_2]        = GRAPHICS_IMAGE_SLOT(0x52190),
    [GAME_AREA_NEO_ARK_POWER_PLANT_1]        = GRAPHICS_IMAGE_SLOT(0x53240),
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE]         = GRAPHICS_IMAGE_SLOT(0x55170),
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE]      = GRAPHICS_IMAGE_SLOT(0x54BB0),
    [GAME_AREA_NEO_ARK_ALTAR]                = GRAPHICS_IMAGE_SLOT(0x55920),
    [GAME_AREA_NEO_ARK_SHRINE]               = GRAPHICS_IMAGE_SLOT(0x4D8E0),
    [GAME_AREA_SHELTER_B6_NURSERY]           = GRAPHICS_IMAGE_SLOT(0x47E00),
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM]       = GRAPHICS_IMAGE_SLOT(0x55620),
    [GAME_AREA_SHELTER_B6_CORRIDOR]          = GRAPHICS_IMAGE_SLOT(0x50F30),
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM]     = GRAPHICS_IMAGE_SLOT(0x50AF0),
    [GAME_AREA_NEO_ARK_R26]                  = GRAPHICS_IMAGE_SLOT(0x57570),
    [GAME_AREA_NEO_ARK_BRIDGE]               = GRAPHICS_IMAGE_SLOT(0x514A0),
    [GAME_AREA_SHELTER_1F_TENT]              = GRAPHICS_IMAGE_SLOT(0x4C020),
    [GAME_AREA_NEO_ARK_WOODLAND_PATH]        = GRAPHICS_IMAGE_SLOT(0x50890),
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY]    = GRAPHICS_IMAGE_SLOT(0x4F920),
    [GAME_AREA_NEO_ARK_R31]                  = GRAPHICS_IMAGE_SLOT(0x58C50),
    [GAME_AREA_NEO_ARK_PYRAMID]              = GRAPHICS_IMAGE_SLOT(0x53AA0),
    [GAME_AREA_NEO_ARK_SUBSTATION]           = GRAPHICS_IMAGE_SLOT(0x55DA0),
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

/// Area-shape substitutions outside the ordinary area-ID slots.
enum {
    MAP_NEO_ARK_ALT_ISLAND_SHAPE_INDEX = MAP_NEO_ARK_AREA_ID_LIMIT + 1,
    MAP_NEO_ARK_ALT_BRIDGE_SHAPE_INDEX = MAP_NEO_ARK_AREA_ID_LIMIT + 2,
};

/// Shared endpoint flag IDs not yet declared in the public game-flag catalog.
enum {
    MAP_NEO_ARK_MARKER_FLAG_GARDEN_WARP_3             = 0x1B5,
    MAP_NEO_ARK_MARKER_FLAG_EVE_ACCESS_TUNNEL_SHELTER = 0x1AF,
    MAP_NEO_ARK_MARKER_FLAG_VEHICULAR_AIRLOCK_BULWARK = 0x1AC,
};

/// Execution priority of this stage's area-default room tasks.
enum { MAP_NEO_ARK_ROOM_TASK_PRIORITY = 32 };

/// Catalog IDs used only by this stage's battle-reward lists.
enum {
    MAP_NEO_ARK_REWARD_RECOVERY_1      = 1,
    MAP_NEO_ARK_REWARD_RECOVERY_2      = 2,
    MAP_NEO_ARK_REWARD_RECOVERY_3      = 3,
    MAP_NEO_ARK_REWARD_EAU_DE_TOILETTE = 0x3E,
    MAP_NEO_ARK_REWARD_JAVELIN         = 0x44,
    MAP_NEO_ARK_REWARD_M249            = 0x90,
    MAP_NEO_ARK_REWARD_9MM_SPARTAN     = 0xA2,
    MAP_NEO_ARK_REWARD_RIOT            = 0xAB,
    MAP_NEO_ARK_REWARD_FIREFLY         = 0xAD,
    MAP_NEO_ARK_REWARD_R_SLUG          = 0xAE,
};

/// Resource IDs shared by the two R26 scripted-walker recipes.
enum {
    MAP_NEO_ARK_SCENE_ACTOR_RESOURCE_FIRST_WALKER  = 111,
    MAP_NEO_ARK_SCENE_ACTOR_RESOURCE_SECOND_WALKER = 112,
};

/// Page-zero record terminating this stage's marker list.
enum { MAP_NEO_ARK_MARKER_PAGE_END = 0 };

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

MenuMapAreaShape gMapNeoArkAreaShapes[MAP_NEO_ARK_AREA_ID_LIMIT + 3] = {
    [0]                                      = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE]    = { .model = &gMappicS503ParkingGarageShape, .page = MAP_NEO_ARK_PAGE_SHELTER_1F, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK] = { .model = &gMappicS503VehicularAirlockShape, .page = MAP_NEO_ARK_PAGE_SHELTER_1F, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_1F_BULWARK]           = { .model = &gMappicS503BulwarkShape, .page = MAP_NEO_ARK_PAGE_SHELTER_1F, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_1F_HELIPORT]          = { .model = &gMappicS503HeliportShape, .page = MAP_NEO_ARK_PAGE_SHELTER_1F, .pairedArea = GAME_AREA_SHELTER_1F_TENT },
    [GAME_AREA_SHELTER_1F_AIRLOCK]           = { .model = &gMappicS503AirlockShape, .page = MAP_NEO_ARK_PAGE_SHELTER_1F, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_1F_GUARDROOM]         = { .model = &gMappicS503GuardroomShape, .page = MAP_NEO_ARK_PAGE_SHELTER_1F, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_OBSERVATORY]          = { .model = &gMappicS500ObservatoryShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL]    = { .model = &gMappicS500EveAccessTunnelShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR]         = { .model = &gMappicS502EveElevatorCorridorShape, .page = MAP_NEO_ARK_PAGE_SHELTER_B6, .pairedArea = GAME_AREA_SHELTER_B6_CORRIDOR },
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE]      = { .model = &gMappicS500NorthPromenadeShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_FOREST_ZONE]          = { .model = &gMappicS500ForestZoneShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL]     = { .model = &gMappicS500SubmarineTunnelShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_PAVILION]             = { .model = &gMappicS500PavilionShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_ISLAND]               = { .model = &gMappicS500IslandShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = GAME_AREA_NEO_ARK_SUBMARINE_GALLERY },
    [GAME_AREA_NEO_ARK_GARDEN]               = { .model = &gMappicS500GardenShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_POWER_PLANT_2]        = { .model = &gMappicS500PowerPlantsShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = GAME_AREA_NEO_ARK_POWER_PLANT_1 },
    [GAME_AREA_NEO_ARK_POWER_PLANT_1]        = { .model = &gMappicS500PowerPlantsShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = GAME_AREA_NEO_ARK_POWER_PLANT_2 },
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE]         = { .model = &gMappicS500SavannaZoneShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE]      = { .model = &gMappicS500SouthPromenadeShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_ALTAR]                = { .model = &gMappicS500AltarShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_SHRINE]               = { .model = &gMappicS500ShrineShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B6_NURSERY]           = { .model = &gMappicS502NurseryShape, .page = MAP_NEO_ARK_PAGE_SHELTER_B6, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM]       = { .model = &gMappicS502GrowthRoomShape, .page = MAP_NEO_ARK_PAGE_SHELTER_B6, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_B6_CORRIDOR]          = { .model = &gMappicS502EveElevatorCorridorShape, .page = MAP_NEO_ARK_PAGE_SHELTER_B6, .pairedArea = GAME_AREA_NEO_ARK_EVE_ELEVATOR },
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM]     = { .model = &gMappicS502TrainingRoomShape, .page = MAP_NEO_ARK_PAGE_SHELTER_B6, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_R26]                  = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_BRIDGE]               = { .model = &gMappicS500BridgeShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_SHELTER_1F_TENT]              = { .model = NULL, .page = MAP_NEO_ARK_PAGE_SHELTER_1F, .pairedArea = GAME_AREA_SHELTER_1F_HELIPORT },
    [GAME_AREA_NEO_ARK_WOODLAND_PATH]        = { .model = &gMappicS500WoodlandPathShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY]    = { .model = NULL, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = GAME_AREA_NEO_ARK_ISLAND },
    [GAME_AREA_NEO_ARK_R31]                  = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_PYRAMID]              = { .model = &gMappicS500PyramidShape, .page = MAP_NEO_ARK_PAGE_NEO_ARK, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [GAME_AREA_NEO_ARK_SUBSTATION]           = { .model = NULL, .page = MENU_MAP_AREA_SHAPE_PAGE_NONE, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    // Alternate-picture models substitute for the Island and Bridge area models.
    // Their stored page bytes are unused: these rows lie beyond the area walk.
    [MAP_NEO_ARK_ALT_ISLAND_SHAPE_INDEX] = { .model = &gMappicS500IslandAltarVariantShape, .page = 4, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    [MAP_NEO_ARK_ALT_BRIDGE_SHAPE_INDEX] = { .model = &gMappicS500BridgeAltarVariantShape, .page = 5, .pairedArea = MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
};

MenuMapMarker gMapNeoArkMarkers[MAP_NEO_ARK_MARKER_COUNT + 1] = {
    { .page = MAP_NEO_ARK_PAGE_NEO_ARK, .area = MENU_MAP_MARKER_AREA_NEVER, .x = 0, .y = 0 },
    { .page = MAP_NEO_ARK_PAGE_SHELTER_B6, .area = MENU_MAP_MARKER_AREA_ANY, .x = -47, .y = 26 },
    { .page = MAP_NEO_ARK_PAGE_NEO_ARK, .area = MENU_MAP_MARKER_AREA_ANY, .x = -47, .y = 59 },
    { .page = MAP_NEO_ARK_PAGE_NEO_ARK, .area = MENU_MAP_MARKER_AREA_ANY, .x = -80, .y = 25 },
    { .page = MAP_NEO_ARK_PAGE_SHELTER_B6, .area = MENU_MAP_MARKER_AREA_ANY, .x = 48, .y = 16 },
    { .page = MAP_NEO_ARK_PAGE_SHELTER_1F, .area = MENU_MAP_MARKER_AREA_ANY, .x = 109, .y = 0 },
    { .page = MAP_NEO_ARK_PAGE_NEO_ARK, .area = MENU_MAP_MARKER_AREA_ANY, .x = 59, .y = -75 },
    { .page = MAP_NEO_ARK_PAGE_NEO_ARK, .area = MENU_MAP_MARKER_AREA_ANY, .x = 109, .y = 8 },
    { .page = MAP_NEO_ARK_PAGE_SHELTER_1F, .area = MENU_MAP_MARKER_AREA_ANY, .x = 30, .y = 0 },
    { .page = MAP_NEO_ARK_MARKER_PAGE_END },
};

MenuMapIcon gMapNeoArkIcons[4] = {
    { .page = MAP_NEO_ARK_PAGE_SHELTER_1F, .area = GAME_AREA_SHELTER_1F_HELIPORT, .kind = MAP_NEO_ARK_ICON_KIND_FIXED_PICTURE, .condition = 0, .x = -36, .y = -21 },
    { .page = MAP_NEO_ARK_PAGE_SHELTER_1F, .area = GAME_AREA_SHELTER_1F_HELIPORT, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = 0, .x = -51, .y = -16 },
    { .page = MAP_NEO_ARK_PAGE_SHELTER_B6, .area = GAME_AREA_SHELTER_B6_NURSERY, .kind = MENU_MAP_ICON_KIND_TELEPHONE, .condition = 0, .x = 25, .y = -2 },
    { .page = MAP_NEO_ARK_ICON_PAGE_END },
};

MenuMapAreaName gMapNeoArkAreaNames[MAP_NEO_ARK_AREA_ID_LIMIT] = {
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE - 1]    = { .text = "Parking garage" },
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK - 1] = { .text = "Vehicular airlock" },
    [GAME_AREA_SHELTER_1F_BULWARK - 1]           = { .text = "Bulwark" },
    [GAME_AREA_SHELTER_1F_HELIPORT - 1]          = { .text = "Heliport" },
    [GAME_AREA_SHELTER_1F_AIRLOCK - 1]           = { .text = "Airlock" },
    [GAME_AREA_SHELTER_1F_GUARDROOM - 1]         = { .text = "Guardroom" },
    [GAME_AREA_NEO_ARK_OBSERVATORY - 1]          = { .text = "Observatory" },
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL - 1]    = { .text = "EVE access tunnel" },
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR - 1]         = { .text = "EVE elevator" },
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE - 1]      = { .text = "North promenade" },
    [GAME_AREA_NEO_ARK_FOREST_ZONE - 1]          = { .text = "Forest zone" },
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL - 1]     = { .text = "Submarine tunnel" },
    [GAME_AREA_NEO_ARK_PAVILION - 1]             = { .text = "Pavilion" },
    [GAME_AREA_NEO_ARK_ISLAND - 1]               = { .text = "Island" },
    [GAME_AREA_NEO_ARK_GARDEN - 1]               = { .text = "Garden" },
    [GAME_AREA_NEO_ARK_POWER_PLANT_2 - 1]        = { .text = "Power plant 2" },
    [GAME_AREA_NEO_ARK_POWER_PLANT_1 - 1]        = { .text = "Power plant 1" },
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE - 1]         = { .text = "Savanna zone" },
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE - 1]      = { .text = "South promenade" },
    [GAME_AREA_NEO_ARK_ALTAR - 1]                = { .text = "Altar" },
    [GAME_AREA_NEO_ARK_SHRINE - 1]               = { .text = "Shrine" },
    [GAME_AREA_SHELTER_B6_NURSERY - 1]           = { .text = "Nursery" },
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM - 1]       = { .text = "Growth room" },
    [GAME_AREA_SHELTER_B6_CORRIDOR - 1]          = { .text = "Corridor" },
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM - 1]     = { .text = "Training room" },
    // Preserve bytes 81 69 / 81 6A: the large font does not decode Shift-JIS.
    // Its 0x81 record has zero metrics; the following i/j are ordinary glyphs.
    [GAME_AREA_NEO_ARK_R26 - 1]               = { .text = "Oval Office\x81\x69"
                                                                        "evening\x81\x6A" },
    [GAME_AREA_NEO_ARK_BRIDGE - 1]            = { .text = "Bridge" },
    [GAME_AREA_SHELTER_1F_TENT - 1]           = { .text = "Tent" },
    [GAME_AREA_NEO_ARK_WOODLAND_PATH - 1]     = { .text = "Jungle zone" },
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY - 1] = { .text = "Submarine gallery" },
    [GAME_AREA_NEO_ARK_R31 - 1]               = { .text = " " },
    [GAME_AREA_NEO_ARK_PYRAMID - 1]           = { .text = "Pyramid" },
    [GAME_AREA_NEO_ARK_SUBSTATION - 1]        = { .text = "Substation" },
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

TaskDesc gMapNeoArkRoomTaskDescriptors[MAP_NEO_ARK_AREA_ID_LIMIT + 1] = {
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, shelter1fParkingGarageRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_PARKING_GARAGE, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, shelter1fVehicularAirlockRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, shelter1fBulwarkRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_BULWARK, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, shelter1fAirlockRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_AIRLOCK, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, shelter1fGuardroomRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_GUARDROOM, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkObservatoryRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_OBSERVATORY, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkEveAccessTunnelRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkEveElevatorRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_EVE_ELEVATOR, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkNorthPromenadeRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_NORTH_PROMENADE, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkForestZoneRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkSubmarineTunnelRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkPavilionRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PAVILION, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkIslandRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkGardenRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_GARDEN, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkSavannaZoneRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SAVANNA_ZONE, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkSouthPromenadeRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SOUTH_PROMENADE, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkAltarRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ALTAR, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkShrineRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, shelterB6NurseryRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_NURSERY, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, shelterB6GrowthRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_GROWTH_ROOM, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, shelterB6CorridorRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_CORRIDOR, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, shelterB6TrainingRoomRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_TRAINING_ROOM, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkBridgeRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_BRIDGE, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkR26RoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_R26, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkSubmarineGalleryMessageTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkR31RoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_R31, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, shelter1fTentRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_TENT, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkWoodlandPathRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkPowerPlant2RoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkPowerPlant1RoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_1, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, shelter1fHeliportRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_HELIPORT, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkPyramidRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PYRAMID, 0) } },
    { { { TASK_BODY_NONE, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, neoArkSubstationRoomTask, { .value = OBJECT_TASK_ROOM_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBSTATION, 0) } },
    { { { TASK_DESC_END, MAP_NEO_ARK_ROOM_TASK_PRIORITY } }, NULL, { 0 } },
};

s32 gMapNeoArkCompanionDistressTicks = 0;

u16 gMapNeoArkMarkerFlags[MAP_NEO_ARK_MARKER_COUNT] = {
    GAME_FLAG_MAP_MARK_B2_MAIN_CORRIDOR,
    GAME_FLAG_MAP_MARK_POWER_PLANT_2 | MENU_MAP_MARKER_ALTERNATE_PICTURE,
    GAME_FLAG_MAP_MARK_SHRINE,
    GAME_FLAG_MAP_MARK_ALTAR,
    MAP_NEO_ARK_MARKER_FLAG_GARDEN_WARP_3,
    GAME_FLAG_MAP_MARK_UNDERGROUND_PARKING,
    GAME_FLAG_MAP_MARK_POWER_PLANT_1,
    MAP_NEO_ARK_MARKER_FLAG_EVE_ACCESS_TUNNEL_SHELTER | MENU_MAP_MARKER_ALTERNATE_PICTURE,
    MAP_NEO_ARK_MARKER_FLAG_VEHICULAR_AIRLOCK_BULWARK,
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

DirectionWarpEntry* gMapNeoArkAreaWarpEntries[MAP_NEO_ARK_AREA_ID_LIMIT] = {
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE - 1]    = gShelter1fParkingGarageWarps,
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK - 1] = gShelter1fVehicularAirlockWarpEntries,
    [GAME_AREA_SHELTER_1F_BULWARK - 1]           = gShelter1fBulwarkWarpEntries,
    [GAME_AREA_SHELTER_1F_HELIPORT - 1]          = gShelter1fHeliportWarpEntries,
    [GAME_AREA_SHELTER_1F_AIRLOCK - 1]           = gShelter1fAirlockWarpEntries,
    [GAME_AREA_SHELTER_1F_GUARDROOM - 1]         = gShelter1fGuardroomWarpEntries,
    [GAME_AREA_NEO_ARK_OBSERVATORY - 1]          = gNeoArkObservatoryWarpEntries,
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL - 1]    = gNeoArkEveAccessTunnelWarpEntries,
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR - 1]         = gNeoArkEveElevatorWarpEntries,
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE - 1]      = gNeoArkNorthPromenadeWarpEntries,
    [GAME_AREA_NEO_ARK_FOREST_ZONE - 1]          = gNeoArkForestZoneWarpEntries,
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL - 1]     = gNeoArkSubmarineTunnelWarpEntries,
    [GAME_AREA_NEO_ARK_PAVILION - 1]             = gNeoArkPavilionWarpEntries,
    [GAME_AREA_NEO_ARK_ISLAND - 1]               = gNeoArkIslandWarpEntries,
    [GAME_AREA_NEO_ARK_GARDEN - 1]               = gNeoArkGardenWarpEntries,
    [GAME_AREA_NEO_ARK_POWER_PLANT_2 - 1]        = gNeoArkPowerPlant2WarpEntries,
    [GAME_AREA_NEO_ARK_POWER_PLANT_1 - 1]        = gNeoArkPowerPlant1WarpEntries,
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE - 1]         = gNeoArkSavannaZoneWarpEntries,
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE - 1]      = gNeoArkSouthPromenadeWarpEntries,
    [GAME_AREA_NEO_ARK_ALTAR - 1]                = gNeoArkAltarWarpEntries,
    [GAME_AREA_NEO_ARK_SHRINE - 1]               = gNeoArkShrineWarpEntries,
    [GAME_AREA_SHELTER_B6_NURSERY - 1]           = gShelterB6NurseryWarpEntries,
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM - 1]       = gShelterB6GrowthRoomWarpEntries,
    [GAME_AREA_SHELTER_B6_CORRIDOR - 1]          = gShelterB6CorridorWarpEntries,
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM - 1]     = gShelterB6TrainingRoomWarpEntries,
    [GAME_AREA_NEO_ARK_R26 - 1]                  = gNeoArkR26Warps,
    [GAME_AREA_NEO_ARK_BRIDGE - 1]               = gNeoArkBridgeWarpEntries,
    [GAME_AREA_SHELTER_1F_TENT - 1]              = gShelter1fTentWarpEntries,
    [GAME_AREA_NEO_ARK_WOODLAND_PATH - 1]        = gNeoArkWoodlandPathWarpEntries,
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY - 1]    = gNeoArkSubmarineGalleryWarpEntries,
    [GAME_AREA_NEO_ARK_R31 - 1]                  = gNeoArkR31WarpEntries,
    [GAME_AREA_NEO_ARK_PYRAMID - 1]              = gNeoArkPyramidWarpEntries,
    [GAME_AREA_NEO_ARK_SUBSTATION - 1]           = gNeoArkSubstationWarpEntries,
};

/// Per-area room view-count arrays borrowed by the stage view-count directory.
///
/// Select area - 1 for IDs 1..33. Every slot is populated, with no terminator;
/// each selected array has its own room bound and must remain loaded during use.
static ViewCount* _gMapNeoArkAreaViewCounts[MAP_NEO_ARK_AREA_ID_LIMIT] = {
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE - 1]    = gShelter1fParkingGarageViewCounts,
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK - 1] = gShelter1fVehicularAirlockViewCounts,
    [GAME_AREA_SHELTER_1F_BULWARK - 1]           = gShelter1fBulwarkRoomViewCounts,
    [GAME_AREA_SHELTER_1F_HELIPORT - 1]          = gShelter1fHeliportViewCounts,
    [GAME_AREA_SHELTER_1F_AIRLOCK - 1]           = gShelter1fAirlockViewCounts,
    [GAME_AREA_SHELTER_1F_GUARDROOM - 1]         = gShelter1fGuardroomRoomViewCounts,
    [GAME_AREA_NEO_ARK_OBSERVATORY - 1]          = gNeoArkObservatoryViewCounts,
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL - 1]    = gNeoArkEveAccessTunnelViewCounts,
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR - 1]         = gNeoArkEveElevatorViewCounts,
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE - 1]      = gNeoArkNorthPromenadeViewCounts,
    [GAME_AREA_NEO_ARK_FOREST_ZONE - 1]          = gNeoArkForestZoneViewCounts,
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL - 1]     = gNeoArkSubmarineTunnelViewCounts,
    [GAME_AREA_NEO_ARK_PAVILION - 1]             = gNeoArkPavilionViewCounts,
    [GAME_AREA_NEO_ARK_ISLAND - 1]               = gNeoArkIslandViewCounts,
    [GAME_AREA_NEO_ARK_GARDEN - 1]               = gNeoArkGardenViewCounts,
    [GAME_AREA_NEO_ARK_POWER_PLANT_2 - 1]        = gNeoArkPowerPlant2ViewCounts,
    [GAME_AREA_NEO_ARK_POWER_PLANT_1 - 1]        = gNeoArkPowerPlant1ViewCounts,
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE - 1]         = gNeoArkSavannaZoneViewCounts,
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE - 1]      = gNeoArkSouthPromenadeViewCounts,
    [GAME_AREA_NEO_ARK_ALTAR - 1]                = gNeoArkAltarViewCounts,
    [GAME_AREA_NEO_ARK_SHRINE - 1]               = gNeoArkShrineViewCounts,
    [GAME_AREA_SHELTER_B6_NURSERY - 1]           = gShelterB6NurseryViewCounts,
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM - 1]       = gShelterB6GrowthRoomViewCounts,
    [GAME_AREA_SHELTER_B6_CORRIDOR - 1]          = gShelterB6CorridorViewCounts,
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM - 1]     = gShelterB6TrainingRoomRoomViewCounts,
    [GAME_AREA_NEO_ARK_R26 - 1]                  = gNeoArkR26ViewCounts,
    [GAME_AREA_NEO_ARK_BRIDGE - 1]               = gNeoArkBridgeViewCounts,
    [GAME_AREA_SHELTER_1F_TENT - 1]              = gShelter1fTentRoomViewCounts,
    [GAME_AREA_NEO_ARK_WOODLAND_PATH - 1]        = gNeoArkWoodlandPathViewCounts,
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY - 1]    = gNeoArkSubmarineGalleryViewCounts,
    [GAME_AREA_NEO_ARK_R31 - 1]                  = gNeoArkR31ViewCounts,
    [GAME_AREA_NEO_ARK_PYRAMID - 1]              = gNeoArkPyramidViewCounts,
    [GAME_AREA_NEO_ARK_SUBSTATION - 1]           = gNeoArkSubstationRoomViewCounts,
};

ViewCountTable gMapNeoArkViewCountTable = { .viewCounts = _gMapNeoArkAreaViewCounts };

/// Room-1 collision resources for the Nursery area.
///
/// Borrows the mutable grid, view boundaries and action triggers from its room
/// overlay; there are no sight occluders.
/// One live descriptor, with no terminator; only room ID 1 is valid. Keep the
/// room resources loaded until the active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapNeoArkNurseryCollisionRooms[1] = {
    { .grid = &gShelterB6NurseryCollisionGrid, .viewBoundaryTriggers = gShelterB6NurseryViewBoundaryTriggers, .actionTriggers = gShelterB6NurseryActionTriggers, .occluders = NULL },
};

/// Room-1 collision resources for the Growth Room area.
///
/// Borrows the mutable grid, view boundaries and action triggers from its room
/// overlay; there are no sight occluders.
/// One live descriptor, with no terminator; only room ID 1 is valid. Keep the
/// room resources loaded until the active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapNeoArkGrowthRoomCollisionRooms[1] = {
    { .grid = &gShelterB6GrowthRoomCollisionGrid, .viewBoundaryTriggers = gShelterB6GrowthRoomViewBoundaryTriggers, .actionTriggers = gShelterB6GrowthRoomActionTriggers, .occluders = NULL },
};

/// Room-1 collision resources for the Corridor area.
///
/// Borrows the mutable grid, view boundaries and action triggers from its room
/// overlay; there are no sight occluders.
/// One live descriptor, with no terminator; only room ID 1 is valid. Keep the
/// room resources loaded until the active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapNeoArkCorridorCollisionRooms[1] = {
    { .grid = &gShelterB6CorridorCollisionGrid, .viewBoundaryTriggers = gShelterB6CorridorViewBoundaryTriggers, .actionTriggers = gShelterB6CorridorActionTriggers, .occluders = NULL },
};

/// Room-1 collision resources for Shelter B6 training room.
///
/// Area 25 selects this single descriptor; only room ID 1 is valid, with
/// no terminator. Borrows the mutable room grid, view boundaries and actions;
/// there are no sight occluders. Keep the room resources loaded until the
/// active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapNeoArkTrainingRoomCollisionRooms[1] = {
    {
        .grid                 = &gShelterB6TrainingRoomCollisionGrid,
        .viewBoundaryTriggers = gShelterB6TrainingRoomViewBoundaryTriggers,
        .actionTriggers       = gShelterB6TrainingRoomActionTriggers,
        .occluders            = NULL,
    },
};

/// Room-1 collision resources for the Bridge area.
///
/// Borrows the mutable grid, view boundaries and action triggers from its room
/// overlay; there are no sight occluders.
/// One live descriptor, with no terminator; only room ID 1 is valid. Keep the
/// room resources loaded until the active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapNeoArkBridgeCollisionRooms[1] = {
    { .grid = &gNeoArkBridgeCollisionGrid, .viewBoundaryTriggers = gNeoArkBridgeViewBoundaryTriggers, .actionTriggers = gNeoArkBridgeActionTriggers, .occluders = NULL },
};

/// Room-1 collision resources for Shelter 1F tent.
///
/// Area 28 selects this single descriptor; only room ID 1 is valid, with
/// no terminator. Borrows the mutable room grid, view boundaries and actions;
/// there are no sight occluders. Keep the room resources loaded until the
/// active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapNeoArkTentCollisionRooms[1] = {
    {
        .grid                 = &gShelter1fTentCollisionGrid,
        .viewBoundaryTriggers = gShelter1fTentViewBoundaryTriggers,
        .actionTriggers       = gShelter1fTentActionTriggers,
        .occluders            = NULL,
    },
};

/// Room-1 collision resources for the Woodland Path area.
///
/// Borrows the mutable grid, view boundaries and action triggers from its room
/// overlay, plus sight occluders.
/// One live descriptor, with no terminator; only room ID 1 is valid. Keep the
/// room resources loaded until the active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapNeoArkWoodlandPathCollisionRooms[1] = {
    { .grid = &gNeoArkWoodlandPathCollisionGrid, .viewBoundaryTriggers = gNeoArkWoodlandPathViewBoundaryTriggers, .actionTriggers = gNeoArkWoodlandPathActionTriggers, .occluders = gNeoArkWoodlandPathSightOccluders },
};

/// Room-1 collision resources for the Submarine Gallery area.
///
/// Borrows the mutable grid, view boundaries and action triggers from its room
/// overlay; there are no sight occluders.
/// One live descriptor, with no terminator; only room ID 1 is valid. Keep the
/// room resources loaded until the active grid and linked lists are retired.
static WorldCollisionRoomResources _gMapNeoArkSubmarineGalleryCollisionRooms[1] = {
    { .grid = &gNeoArkSubmarineGalleryCollisionGrid, .viewBoundaryTriggers = gNeoArkSubmarineGalleryViewBoundaryTriggers, .actionTriggers = gNeoArkSubmarineGalleryActionTriggers, .occluders = NULL },
};

/// Empty room-1 collision descriptor for the R31 scene area.
///
/// Supplies no grid, view boundaries, action triggers or occluders. The array
/// contains one live descriptor and no terminator; only room ID 1 is valid.
static WorldCollisionRoomResources _gMapNeoArkR31CollisionRooms[1] = {
    { .grid = NULL, .viewBoundaryTriggers = NULL, .actionTriggers = NULL, .occluders = NULL },
};

/// Per-area collision room arrays for Shelter/Neo Ark.
///
/// Select area - 1 for IDs 1..33. Every area entry is populated.
/// Each array uses room - 1 within its own extent, without a terminator.
/// Descriptors may be map-owned or borrowed; their mutable room resources must
/// remain loaded until the active grid and linked lists are retired.
static WorldCollisionRoomResources* _gMapNeoArkWorldCollisionAreaRooms[MAP_NEO_ARK_AREA_ID_LIMIT] = {
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE - 1]    = gShelter1fParkingGarageCollisionRooms,
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK - 1] = gShelter1fVehicularAirlockCollisionResources,
    [GAME_AREA_SHELTER_1F_BULWARK - 1]           = gShelter1fBulwarkCollisionResources,
    [GAME_AREA_SHELTER_1F_HELIPORT - 1]          = gShelter1fHeliportCollisionResources,
    [GAME_AREA_SHELTER_1F_AIRLOCK - 1]           = gShelter1fAirlockRoomCollisionResources,
    [GAME_AREA_SHELTER_1F_GUARDROOM - 1]         = gShelter1fGuardroomRoomCollisionResources,
    [GAME_AREA_NEO_ARK_OBSERVATORY - 1]          = gNeoArkObservatoryRoomCollisionResources,
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL - 1]    = gNeoArkEveAccessTunnelCollisionRooms,
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR - 1]         = gNeoArkEveElevatorCollisionRooms,
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE - 1]      = gNeoArkNorthPromenadeRoomCollisionResources,
    [GAME_AREA_NEO_ARK_FOREST_ZONE - 1]          = gNeoArkForestZoneRoomCollisionResources,
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL - 1]     = gNeoArkSubmarineTunnelRoomCollisionResources,
    [GAME_AREA_NEO_ARK_PAVILION - 1]             = gNeoArkPavilionRoomCollisionResources,
    [GAME_AREA_NEO_ARK_ISLAND - 1]               = gNeoArkIslandRoomCollisionResources,
    [GAME_AREA_NEO_ARK_GARDEN - 1]               = gNeoArkGardenCollisionRooms,
    [GAME_AREA_NEO_ARK_POWER_PLANT_2 - 1]        = gNeoArkPowerPlant2CollisionRooms,
    [GAME_AREA_NEO_ARK_POWER_PLANT_1 - 1]        = gNeoArkPowerPlant1CollisionRooms,
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE - 1]         = gNeoArkSavannaZoneRoomCollisionResources,
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE - 1]      = gNeoArkSouthPromenadeCollisionResources,
    [GAME_AREA_NEO_ARK_ALTAR - 1]                = gNeoArkAltarRoomCollisionResources,
    [GAME_AREA_NEO_ARK_SHRINE - 1]               = gNeoArkShrineCollisionRooms,
    [GAME_AREA_SHELTER_B6_NURSERY - 1]           = _gMapNeoArkNurseryCollisionRooms,
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM - 1]       = _gMapNeoArkGrowthRoomCollisionRooms,
    [GAME_AREA_SHELTER_B6_CORRIDOR - 1]          = _gMapNeoArkCorridorCollisionRooms,
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM - 1]     = _gMapNeoArkTrainingRoomCollisionRooms,
    [GAME_AREA_NEO_ARK_R26 - 1]                  = gNeoArkR26CollisionRooms,
    [GAME_AREA_NEO_ARK_BRIDGE - 1]               = _gMapNeoArkBridgeCollisionRooms,
    [GAME_AREA_SHELTER_1F_TENT - 1]              = _gMapNeoArkTentCollisionRooms,
    [GAME_AREA_NEO_ARK_WOODLAND_PATH - 1]        = _gMapNeoArkWoodlandPathCollisionRooms,
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY - 1]    = _gMapNeoArkSubmarineGalleryCollisionRooms,
    [GAME_AREA_NEO_ARK_R31 - 1]                  = _gMapNeoArkR31CollisionRooms,
    [GAME_AREA_NEO_ARK_PYRAMID - 1]              = gNeoArkPyramidRoomCollisionResources,
    [GAME_AREA_NEO_ARK_SUBSTATION - 1]           = gNeoArkSubstationRoomCollisionResources,
};

WorldCollisionStageResources gMapNeoArkWorldCollisionResources = { .areaRooms = _gMapNeoArkWorldCollisionAreaRooms };

/// Camera-array directory for Shelter/Neo Ark area IDs 1..33.
///
/// Every slot borrows its area's loaded room overlay; select area - 1.
/// Room arrays have their own mapped-view bounds, with no directory terminator.
static ViewCamera* _gMapNeoArkAreaCameras[GAME_AREA_NEO_ARK_SUBSTATION] = {
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE - 1]    = gShelter1fParkingGarageViewCameras,
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK - 1] = gShelter1fVehicularAirlockViewCameras,
    [GAME_AREA_SHELTER_1F_BULWARK - 1]           = gShelter1fBulwarkCameras,
    [GAME_AREA_SHELTER_1F_HELIPORT - 1]          = gShelter1fHeliportViewCameras,
    [GAME_AREA_SHELTER_1F_AIRLOCK - 1]           = gShelter1fAirlockViewCameras,
    [GAME_AREA_SHELTER_1F_GUARDROOM - 1]         = gShelter1fGuardroomViewCameras,
    [GAME_AREA_NEO_ARK_OBSERVATORY - 1]          = gNeoArkObservatoryViewCameras,
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL - 1]    = gNeoArkEveAccessTunnelViewCameras,
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR - 1]         = gNeoArkEveElevatorViewCameras,
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE - 1]      = gNeoArkNorthPromenadeViewCameras,
    [GAME_AREA_NEO_ARK_FOREST_ZONE - 1]          = gNeoArkForestZoneViewCameras,
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL - 1]     = gNeoArkSubmarineTunnelViewCameras,
    [GAME_AREA_NEO_ARK_PAVILION - 1]             = gNeoArkPavilionViewCameras,
    [GAME_AREA_NEO_ARK_ISLAND - 1]               = gNeoArkIslandViewCameras,
    [GAME_AREA_NEO_ARK_GARDEN - 1]               = gNeoArkGardenViewCameras,
    [GAME_AREA_NEO_ARK_POWER_PLANT_2 - 1]        = gNeoArkPowerPlant2ViewCameras,
    [GAME_AREA_NEO_ARK_POWER_PLANT_1 - 1]        = gNeoArkPowerPlant1ViewCameras,
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE - 1]         = gNeoArkSavannaZoneViewCameras,
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE - 1]      = gNeoArkSouthPromenadeViewCameras,
    [GAME_AREA_NEO_ARK_ALTAR - 1]                = gNeoArkAltarViewCameras,
    [GAME_AREA_NEO_ARK_SHRINE - 1]               = gNeoArkShrineViewCameras,
    [GAME_AREA_SHELTER_B6_NURSERY - 1]           = gShelterB6NurseryViewCameras,
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM - 1]       = gShelterB6GrowthRoomViewCameras,
    [GAME_AREA_SHELTER_B6_CORRIDOR - 1]          = gShelterB6CorridorViewCameras,
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM - 1]     = gShelterB6TrainingRoomViewCameras,
    [GAME_AREA_NEO_ARK_R26 - 1]                  = gNeoArkR26ViewCameras,
    [GAME_AREA_NEO_ARK_BRIDGE - 1]               = gNeoArkBridgeViewCameras,
    [GAME_AREA_SHELTER_1F_TENT - 1]              = gShelter1fTentViewCameras,
    [GAME_AREA_NEO_ARK_WOODLAND_PATH - 1]        = gNeoArkWoodlandPathViewCameras,
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY - 1]    = gNeoArkSubmarineGalleryViewCameras,
    [GAME_AREA_NEO_ARK_R31 - 1]                  = gNeoArkR31ViewCameras,
    [GAME_AREA_NEO_ARK_PYRAMID - 1]              = gNeoArkPyramidViewCameras,
    [GAME_AREA_NEO_ARK_SUBSTATION - 1]           = gNeoArkSubstationViewCameras,
};

ViewCameraTable gMapNeoArkViewCameraTable = { .cameras = _gMapNeoArkAreaCameras };

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

/// Resource-entry selectors used by the Island-to-Shrine placement layouts.
///
/// These keys bind the selected room's resource recipes, independently of the
/// actor overlay slot. Garden's scripted-walker key differs from its file number.
enum {
    MAP_NEO_ARK_RESOURCE_DESERT_CHASER          = 1,
    MAP_NEO_ARK_RESOURCE_SCORPION               = 25,
    MAP_NEO_ARK_RESOURCE_BETA_GENERATOR         = 53,
    MAP_NEO_ARK_RESOURCE_PROTO_GENERATOR        = 54,
    MAP_NEO_ARK_GARDEN_RESOURCE_SCRIPTED_WALKER = 132,
};

/// Actor-specific placement selectors used by these layouts.
///
/// Beetle mode tens select the surface; its floor remainder selects the action.
/// Scorpion movement rows index eight-entry turn/speed tables; row 3 selects
/// their maximum authored rates. Watcher sight rows select game-unit ranges.
/// The Chaser's low selector 1 chooses tuning row 2. Maggot row 1 supplies
/// a ten-tick idle base, 2400-unit crawl base and 100-unit/tick ambush fall.
enum {
    MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE       = 0,
    MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING          = 20,
    MAP_NEO_ARK_BLACK_BEETLE_MODE_SHRINE_HIDDEN    = 30,
    MAP_NEO_ARK_GOLEM_MODE_IDLE                    = 0,
    MAP_NEO_ARK_SCORPION_MODE_ROAM                 = 0,
    MAP_NEO_ARK_SCORPION_MODE_AMBUSH_LEADER        = 1,
    MAP_NEO_ARK_SCORPION_MODE_AMBUSH_FOLLOWER      = 2,
    MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW         = 3,
    MAP_NEO_ARK_WATCHER_WEAPON_GUN                 = 4,
    MAP_NEO_ARK_WATCHER_SIGHT_4500_ROW             = 3,
    MAP_NEO_ARK_WATCHER_SIGHT_5000_ROW             = 4,
    MAP_NEO_ARK_WATCHER_SIGHT_8000_ROW             = 7,
    MAP_NEO_ARK_DESERT_CHASER_SPAWN_STEER          = 2,
    MAP_NEO_ARK_DESERT_CHASER_TUNING_ROW2_SELECTOR = 1,
    MAP_NEO_ARK_MAGGOT_SAVANNA_MOVEMENT_ROW        = 1,
};

AreaPlacement gMapNeoArkIslandVariant11Placements[2] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_GRENADE_LAUNCHER_ROOK, .variant = 8, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 2500, .y = 0, .z = 3500, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkGardenVariant1Placements[11] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = -300, .y = 0, .z = -16300, .yaw = 3350, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = -750, .y = 0, .z = -17800, .yaw = 3150, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = -7400, .y = 0, .z = -16300, .yaw = 300, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = -6400, .y = 0, .z = -17800, .yaw = 400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -500, .y = 0, .z = -15000, .yaw = 900, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -1400, .y = 0, .z = -18300, .yaw = 2900, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -5750, .y = 0, .z = -17750, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -7200, .y = 0, .z = -14600, .yaw = 1200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -5500, .y = 0, .z = -15600, .yaw = 3600, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = -6400, .y = 0, .z = -17000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkGardenVariant2Placements[2] = {
    { .entryId = MAP_NEO_ARK_GARDEN_RESOURCE_SCRIPTED_WALKER, .variant = 0, .mode = 0, .x = -7480, .y = 0, .z = -17590, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkGardenVariant3Placements[8] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_MOSSBACK, .variant = 0, .mode = 0, .x = -550, .y = 0, .z = -16300, .yaw = 2500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL, .x = -8800, .y = -1500, .z = -16500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL, .x = -7500, .y = -1800, .z = -19000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = -7800, .y = 0, .z = -17350, .yaw = 2400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = -7100, .y = 0, .z = -16150, .yaw = 1350, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = -6300, .y = 0, .z = -15100, .yaw = 1350, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = -8150, .y = 0, .z = -15100, .yaw = 3600, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkGardenVariant4Placements[5] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_AMOEBA, .variant = 0, .mode = 0, .x = -4000, .y = 0, .z = -18000, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_AMOEBA, .variant = 0, .mode = 0, .x = -2500, .y = 0, .z = -18000, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_AMOEBA, .variant = 0, .mode = 0, .x = -6500, .y = 0, .z = -15600, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_AMOEBA, .variant = 0, .mode = 0, .x = -6400, .y = 0, .z = -17000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPowerPlant2Variant1Placements[8] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BETA_GENERATOR, .variant = 0, .mode = 0, .x = 4000, .y = -6030, .z = -4990, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_3, .mode = 0, .x = 9000, .y = -7000, .z = -2500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_5000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_2, .mode = 0, .x = 9000, .y = -7000, .z = -4500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_5000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_3, .mode = 0, .x = 9000, .y = -7000, .z = -6500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_5000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_2, .mode = 0, .x = 9000, .y = -7000, .z = -8500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_5000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_1, .mode = 0, .x = 8500, .y = -7000, .z = -9000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_8000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_1, .mode = 0, .x = 6500, .y = -7000, .z = -9000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_8000_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPowerPlant2Variant11Placements[6] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_3, .mode = 0, .x = 9000, .y = -7000, .z = -2500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_5000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_1, .mode = 0, .x = 9000, .y = -7000, .z = -4500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_5000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_3, .mode = 0, .x = 9000, .y = -7000, .z = -6500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_5000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_1, .mode = 0, .x = 9000, .y = -7000, .z = -8500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_5000_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_GRENADE_LAUNCHER_ROOK, .variant = 5, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 7500, .y = -5000, .z = -3000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPowerPlant2Variant17Placements[2] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BISHOP_GOLEM, .variant = 0, .mode = 0, .x = 0, .y = -5000, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPowerPlant1Variant1Placements[8] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_PROTO_GENERATOR, .variant = 0, .mode = 0, .x = 4000, .y = -1030, .z = -8000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_3, .mode = 0, .x = 0, .y = -2400, .z = -10900, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_4500_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_GUN, .mode = 0, .x = 0, .y = -2400, .z = -10150, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_4500_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_3, .mode = 0, .x = 0, .y = -2400, .z = -8650, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_4500_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_GUN, .mode = 0, .x = 0, .y = -2400, .z = -7900, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_4500_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_3, .mode = 0, .x = 0, .y = -2400, .z = -6400, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_4500_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_WATCHER, .variant = MAP_NEO_ARK_WATCHER_WEAPON_VARIANT_2, .mode = 0, .x = 600, .y = -2400, .z = -3000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = MAP_NEO_ARK_WATCHER_SIGHT_8000_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPowerPlant1Variant2Placements[9] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL, .x = 0, .y = -1500, .z = -6400, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 8400, .y = -4500, .z = -8750, .yaw = 2400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 7050, .y = -4500, .z = -9100, .yaw = 1700, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 7650, .y = -4500, .z = -11400, .yaw = 600, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 400, .y = -4500, .z = -11500, .yaw = 2700, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 800, .y = -4500, .z = -10500, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 1150, .y = -4500, .z = -9250, .yaw = 2400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 300, .y = -4500, .z = -8500, .yaw = 3800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPowerPlant1Variant3Placements[8] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = 800, .y = 0, .z = -5000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = 800, .y = 0, .z = -7000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = 800, .y = 0, .z = -9000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = 800, .y = 0, .z = -11000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = 3000, .y = 0, .z = -11000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL, .x = 700, .y = -1300, .z = -3000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 900, .y = -4500, .z = -6700, .yaw = 500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPowerPlant1Variant4Placements[10] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 400, .y = 0, .z = -11300, .yaw = 400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 1000, .y = 0, .z = -9050, .yaw = 2550, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 3000, .y = 0, .z = -11000, .yaw = 2050, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 4450, .y = 0, .z = -10800, .yaw = 3750, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = 300, .y = 0, .z = -3300, .yaw = 3750, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = 700, .y = 0, .z = -4800, .yaw = 50, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = 300, .y = 0, .z = -5900, .yaw = 3650, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = 1200, .y = 0, .z = -6150, .yaw = 200, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = 1200, .y = 0, .z = -7050, .yaw = 2000, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkPowerPlant1Variant11Placements[7] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 500, .y = -4500, .z = -3450, .yaw = 3500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 1150, .y = -4500, .z = -3550, .yaw = 500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 800, .y = -4500, .z = -3850, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 350, .y = -4500, .z = -4250, .yaw = 3200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 1000, .y = -4500, .z = -4300, .yaw = 3800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 1100, .y = -4500, .z = -4400, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSavannaZoneVariant1Placements[8] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_DESERT_CHASER, .variant = MAP_NEO_ARK_DESERT_CHASER_SPAWN_STEER, .mode = MAP_NEO_ARK_DESERT_CHASER_TUNING_ROW2_SELECTOR, .x = 3250, .y = 50, .z = 2400, .yaw = 3000, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_DESERT_CHASER, .variant = MAP_NEO_ARK_DESERT_CHASER_SPAWN_STEER, .mode = MAP_NEO_ARK_DESERT_CHASER_TUNING_ROW2_SELECTOR, .x = 6300, .y = 50, .z = 1200, .yaw = 550, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_AMBUSH_FOLLOWER, .x = 2650, .y = 50, .z = 2650, .yaw = 1400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_AMBUSH_FOLLOWER, .x = 2100, .y = 50, .z = 700, .yaw = 650, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_AMBUSH_FOLLOWER, .x = 11450, .y = 50, .z = 2500, .yaw = 2850, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_AMBUSH_FOLLOWER, .x = 5700, .y = 50, .z = 2500, .yaw = 1800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_AMBUSH_FOLLOWER, .x = 2350, .y = 50, .z = 1650, .yaw = 950, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSavannaZoneVariant2Placements[8] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 3600, .y = 50, .z = 2650, .yaw = 1800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 2200, .y = 50, .z = 400, .yaw = 800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 3900, .y = 50, .z = 300, .yaw = 1900, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 4450, .y = 50, .z = 2650, .yaw = -800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 5650, .y = 50, .z = 2000, .yaw = 800, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 6800, .y = 50, .z = 0, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 8700, .y = 50, .z = 1300, .yaw = 3600, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSavannaZoneVariant3Placements[7] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_DESERT_CHASER, .variant = MAP_NEO_ARK_DESERT_CHASER_SPAWN_STEER, .mode = MAP_NEO_ARK_DESERT_CHASER_TUNING_ROW2_SELECTOR, .x = 2800, .y = 50, .z = 1800, .yaw = 2200, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_FOLLOWER, .x = 5600, .y = -5000, .z = 1000, .yaw = 3600, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_MAGGOT_SAVANNA_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_FOLLOWER, .x = 9000, .y = -6000, .z = 2450, .yaw = 2800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_MAGGOT_SAVANNA_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_FOLLOWER, .x = 3250, .y = -3700, .z = 2650, .yaw = 1050, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_MAGGOT_SAVANNA_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_FOLLOWER, .x = 2550, .y = -7000, .z = 600, .yaw = 800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_MAGGOT_SAVANNA_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_MAGGOT, .variant = 0, .mode = MAP_NEO_ARK_MAGGOT_MODE_AMBUSH_FOLLOWER, .x = 3100, .y = -5000, .z = 1650, .yaw = 1050, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = MAP_NEO_ARK_MAGGOT_SAVANNA_MOVEMENT_ROW },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSavannaZoneVariant4Placements[7] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_AMBUSH_FOLLOWER, .x = 3500, .y = 0, .z = 500, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_AMBUSH_FOLLOWER, .x = 3500, .y = 0, .z = 2000, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_AMBUSH_FOLLOWER, .x = 9400, .y = 0, .z = 500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_AMBUSH_FOLLOWER, .x = 9400, .y = 0, .z = 2000, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_AMBUSH_LEADER, .x = 5800, .y = 0, .z = 1400, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_DESERT_CHASER, .variant = MAP_NEO_ARK_DESERT_CHASER_SPAWN_STEER, .mode = MAP_NEO_ARK_DESERT_CHASER_TUNING_ROW2_SELECTOR, .x = 3150, .y = 50, .z = 2240, .yaw = 1900, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSavannaZoneVariant5Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 3, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 6500, .y = 0, .z = 2000, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_EXTRA_PAWN_GOLEM, .variant = 0, .mode = MAP_NEO_ARK_GOLEM_MODE_IDLE, .x = 3500, .y = 0, .z = 1500, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSavannaZoneVariant11Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_EXTRA_PAWN_GOLEM, .variant = 3, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 8000, .y = 0, .z = 2200, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_GRENADE_LAUNCHER_ROOK, .variant = 0, .mode = MAP_NEO_ARK_GOLEM_MODE_IDLE, .x = 6000, .y = 0, .z = 300, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSouthPromenadeVariant2Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSouthPromenadeVariant3Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSouthPromenadeVariant4Placements[7] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_IDLE, .x = 4000, .y = 0, .z = 2400, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL, .x = 4200, .y = -1500, .z = 4250, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL, .x = 13300, .y = -1500, .z = 6000, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL, .x = 4300, .y = -1500, .z = 600, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_WALL, .x = 700, .y = -1500, .z = 4000, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_CEILING, .x = 4000, .y = -3000, .z = 2400, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSouthPromenadeVariant5Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_EXTRA_PAWN_GOLEM, .variant = 5, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 11000, .y = 0, .z = 3000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_EXTRA_PAWN_GOLEM, .variant = 0, .mode = MAP_NEO_ARK_GOLEM_MODE_IDLE, .x = 5000, .y = 0, .z = 3800, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkSouthPromenadeVariant11Placements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 6, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 11500, .y = 0, .z = 2500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 4, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 11000, .y = 0, .z = 8500, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BEAM_SWORD_ROOK, .variant = 0, .mode = MAP_NEO_ARK_GOLEM_MODE_IDLE, .x = 5000, .y = 0, .z = 1200, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 3, .clutRowOffset = 5, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkShrineVariant1Placements[4] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_SHRINE_HIDDEN, .x = 8750, .y = -4000, .z = -4550, .yaw = 2500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_SHRINE_HIDDEN, .x = 8650, .y = -4000, .z = -4650, .yaw = 3000, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_SHRINE_HIDDEN, .x = 8850, .y = -4000, .z = -4650, .yaw = 3500, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkShrineVariant2Placements[5] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 7000, .y = 0, .z = 1500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 10000, .y = 0, .z = 1500, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 7000, .y = 0, .z = 5000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 4000, .y = 0, .z = 5000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkShrineVariant3Placements[5] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 7000, .y = 0, .z = 1500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 10000, .y = 0, .z = 1500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 7000, .y = 0, .z = 5000, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 4000, .y = 0, .z = 5000, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkShrineVariant4Placements[9] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 6300, .y = 0, .z = -3850, .yaw = 400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 8100, .y = 0, .z = -3200, .yaw = 3400, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 7300, .y = 0, .z = -4000, .yaw = 4000, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_SCORPION, .variant = 0, .mode = MAP_NEO_ARK_SCORPION_MODE_ROAM, .x = 5900, .y = 0, .z = -2100, .yaw = 1450, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = MAP_NEO_ARK_SCORPION_FAST_MOVEMENT_ROW },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 700, .y = 0, .z = 4700, .yaw = 800, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 1400, .y = 0, .z = 1400, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 4750, .y = 0, .z = 5700, .yaw = 1400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_BLACK_BEETLE, .variant = 0, .mode = MAP_NEO_ARK_BLACK_BEETLE_MODE_FLOOR_WANDER, .x = 2800, .y = 0, .z = 4550, .yaw = 400, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkShrineVariant5Placements[3] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 0, .mode = MAP_NEO_ARK_GOLEM_MODE_IDLE, .x = 7000, .y = 0, .z = -4000, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_RESOURCE_PAWN_GOLEM, .variant = 3, .mode = MAP_NEO_ARK_GOLEM_PATROLS, .x = 1000, .y = 0, .z = 5000, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaPlacement gMapNeoArkShrineVariant7Placements[2] = {
    { .entryId = MAP_NEO_ARK_RESOURCE_KNIGHT_GOLEM, .variant = 0, .mode = 0, .x = 7000, .y = 0, .z = 1500, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
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

/// Unreferenced two-actor placement list retained in the stage map.
///
/// Resource IDs match the two R26 scripted-walker recipes, with Z positions
/// 2850 and 850 and yaw 2048 and 0 (4096 units per turn). No live layout selects
/// this list, so its intended use is unproven. Ends with `AREA_PLACEMENT_END`.
static AreaPlacement _gMapNeoArkUnreferencedSceneActorPlacements[3] = {
    { .entryId = MAP_NEO_ARK_SCENE_ACTOR_RESOURCE_FIRST_WALKER, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 2850, .yaw = ACTOR_TRANSFORM_ANGLE_HALF_TURN, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = MAP_NEO_ARK_SCENE_ACTOR_RESOURCE_SECOND_WALKER, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 850, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 2, .clutRowOffset = 4, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
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

InventoryBattleReward gMapNeoArkBattleRewards[29] = {
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, 1, 0), .items = { MAP_NEO_ARK_KIND_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_BULWARK, 1, 0), .items = { MAP_NEO_ARK_KIND_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_556_RIFLE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_AIRLOCK, 1, 0), .items = { MAP_NEO_ARK_REWARD_R_SLUG, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_RINGERS_SOLUTION } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_NORTH_PROMENADE, 3, 0), .items = { MAP_NEO_ARK_REWARD_9MM_SPARTAN, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_556_RIFLE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 1, 0), .items = { MAP_NEO_ARK_REWARD_RECOVERY_3, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_MP_BOOST_2 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 5, 0), .items = { MAP_NEO_ARK_REWARD_9MM_SPARTAN, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL, 2, 0), .items = { MAP_NEO_ARK_REWARD_EAU_DE_TOILETTE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2, 1, 0), .items = { MAP_NEO_ARK_KIND_RINGERS_SOLUTION, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_MP_BOOST_2 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_1, 1, 0), .items = { MAP_NEO_ARK_REWARD_RECOVERY_3, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_MP_BOOST_2 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SAVANNA_ZONE, 5, 0), .items = { MAP_NEO_ARK_REWARD_RIOT, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SOUTH_PROMENADE, 5, 0), .items = { MAP_NEO_ARK_REWARD_RIOT, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_GRENADE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 1, 0), .items = { MAP_NEO_ARK_REWARD_RECOVERY_2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_MP_BOOST_2 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 5, 0), .items = { MAP_NEO_ARK_REWARD_9MM_SPARTAN, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_556_RIFLE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_CORRIDOR, 1, 0), .items = { MAP_NEO_ARK_KIND_MP_BOOST_2, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_REWARD_RECOVERY_3 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_TRAINING_ROOM, 1, 0), .items = { MAP_NEO_ARK_KIND_MP_BOOST_2, MAP_NEO_ARK_REWARD_EAU_DE_TOILETTE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_RINGERS_SOLUTION } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, 1, 0), .items = { MAP_NEO_ARK_KIND_PROTEIN_CAPSULE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_MP_BOOST_2 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, 2, 0), .items = { MAP_NEO_ARK_KIND_RINGERS_SOLUTION, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PYRAMID, 4, 0), .items = { MAP_NEO_ARK_REWARD_9MM_SPARTAN, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_556_RIFLE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_NORTH_PROMENADE, 11, 0), .items = { MAP_NEO_ARK_REWARD_RIOT, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_GRENADE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 11, 0), .items = { MAP_NEO_ARK_REWARD_RIOT, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PAVILION, 11, 0), .items = { MAP_NEO_ARK_KIND_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, 11, 0), .items = { MAP_NEO_ARK_KIND_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2, 11, 0), .items = { MAP_NEO_ARK_KIND_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SAVANNA_ZONE, 11, 0), .items = { MAP_NEO_ARK_KIND_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_REWARD_RIOT } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SOUTH_PROMENADE, 11, 0), .items = { MAP_NEO_ARK_KIND_556_RIFLE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 11, 0), .items = { MAP_NEO_ARK_KIND_PROTEIN_CAPSULE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_BRIDGE, 11, 0), .items = { MAP_NEO_ARK_KIND_556_RIFLE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_REWARD_9MM_SPARTAN } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PYRAMID, 11, 0), .items = { MAP_NEO_ARK_KIND_AIRBURST, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_KIND_556_RIFLE } },
    { .areaLayoutKey = INVENTORY_BATTLE_REWARD_LIST_END },
};

InventoryBattleReward gMapNeoArkHardBattleRewards[6] = {
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_AIRLOCK, 7, 0), .items = { MAP_NEO_ARK_KIND_PROTEIN_CAPSULE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 7, 0), .items = { MAP_NEO_ARK_REWARD_JAVELIN, MAP_NEO_ARK_KIND_556_RIFLE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_REWARD_RECOVERY_3 } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_B6_TRAINING_ROOM, 1, 0), .items = { MAP_NEO_ARK_REWARD_RECOVERY_1, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_REWARD_EAU_DE_TOILETTE } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, 1, 0), .items = { MAP_NEO_ARK_REWARD_FIREFLY, INVENTORY_ITEM_NONE, INVENTORY_ITEM_NONE, MAP_NEO_ARK_REWARD_R_SLUG } },
    { .areaLayoutKey = GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2, 17, 0), .items = { MAP_NEO_ARK_REWARD_M249, MAP_NEO_ARK_KIND_556_RIFLE, MAP_NEO_ARK_KIND_44_MAEDA_SP, MAP_NEO_ARK_KIND_LIPSTICK } },
    { .areaLayoutKey = INVENTORY_BATTLE_REWARD_LIST_END },
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
