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

static AreaObjectPlace D_map_neo_ark_8017C790[2];
static AreaObjectPlace D_map_neo_ark_8017C7B0[14];
static AreaObjectPlace D_map_neo_ark_8017C890[2];
static AreaObjectPlace D_map_neo_ark_8017C8B0[2];
static AreaObjectPlace D_map_neo_ark_8017C8D0[4];
static AreaObjectPlace D_map_neo_ark_8017C910[2];
static AreaObjectPlace D_map_neo_ark_8017C930[4];
static AreaObjectPlace D_map_neo_ark_8017C970[2];
static AreaObjectPlace D_map_neo_ark_8017C990[2];

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

GfxImageSlot D_map_neo_ark_80179DB8[34] = {
    GFX_IMAGE_SLOT(0x3A8E0),
    GFX_IMAGE_SLOT(0x54B20),
    GFX_IMAGE_SLOT(0x53DB0),
    GFX_IMAGE_SLOT(0x55640),
    GFX_IMAGE_SLOT(0x4F560),
    GFX_IMAGE_SLOT(0x56AA0),
    GFX_IMAGE_SLOT(0x58820),
    GFX_IMAGE_SLOT(0x4A320),
    GFX_IMAGE_SLOT(0x550A0),
    GFX_IMAGE_SLOT(0x58F40),
    GFX_IMAGE_SLOT(0x52360),
    GFX_IMAGE_SLOT(0x53320),
    GFX_IMAGE_SLOT(0x4B7D0),
    GFX_IMAGE_SLOT(0x4DDF0),
    GFX_IMAGE_SLOT(0x524B0),
    GFX_IMAGE_SLOT(0x52610),
    GFX_IMAGE_SLOT(0x52190),
    GFX_IMAGE_SLOT(0x53240),
    GFX_IMAGE_SLOT(0x55170),
    GFX_IMAGE_SLOT(0x54BB0),
    GFX_IMAGE_SLOT(0x55920),
    GFX_IMAGE_SLOT(0x4D8E0),
    GFX_IMAGE_SLOT(0x47E00),
    GFX_IMAGE_SLOT(0x55620),
    GFX_IMAGE_SLOT(0x50F30),
    GFX_IMAGE_SLOT(0x50AF0),
    GFX_IMAGE_SLOT(0x57570),
    GFX_IMAGE_SLOT(0x514A0),
    GFX_IMAGE_SLOT(0x4C020),
    GFX_IMAGE_SLOT(0x50890),
    GFX_IMAGE_SLOT(0x4F920),
    GFX_IMAGE_SLOT(0x58C50),
    GFX_IMAGE_SLOT(0x53AA0),
    GFX_IMAGE_SLOT(0x55DA0),
};

s32 D_map_neo_ark_80179EC8[21] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0x21C00,
    0,
    0,
    0x25800,
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

u8 D_map_neo_ark_80179F1C[8] = { 0, 0x19, 0x80, 0x1A };

MenuMapArea D_map_neo_ark_80179F24[35] = {
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0x2174, -0x53D, 0x67, 4, 0x12C, 0x12C, 3 },
    { -0x1388, 0x622, 0x35, 0xFFFC, 0x12C, 0x12C, 3 },
    { 0xF6F, 0, 0x1C, 0, 0x12C, 0x12C, 3 },
    { 0x26C0, 0xD7A, 0xFFF6, 0, 0x12C, 0x12C, 3 },
    { 0x803, 0xBFE, 0x4F, 0xFFF5, 0x10E, 0x12C, 3 },
    { -0x2328, -0xE24, 0x28, 0xD, 0x12C, 0x12C, 3 },
    { 0x34BC, 0x2EE0, 0x6D, 0xFFEA, 0x12C, 0xFA, 1 },
    { -0x4B0, 0x500, 0x6D, 7, 0x12C, 0x104, 1 },
    { -0x3C0, 0, 0xFFCB, 0x1A, 0x12C, 0x12C, 2 },
    { 0x2A43, 0x54A, 0x5C, 0xFFD8, 0x12C, 0x12C, 1 },
    { 0x2323, -0x80, 0x35, 0xFFB5, 0x12C, 0x12C, 1 },
    { 0x11C0, 0, 0xFFE3, 0xFFB5, 0x118, 0x12C, 1 },
    { 0x7D0, 0x2520, 0xFFAF, 0xFFB5, 0x12C, 0x12C, 1 },
    { 0xC1C, 0x1940, 0xFFB7, 0xFFBF, 0x12C, 0x140, 1 },
    { -0xBE0, -0x319C, 0xFF9D, 8, 0x12C, 0x12C, 1 },
    { 0x202, -0x5A0, 0xFFB5, 0x18, 0x12C, 0x12C, 1 },
    { 0x21E8, -0x2904, 0xFFD1, 0x3A, 0x12C, 0x12C, 1 },
    { 0x1FE, 0x5DC, 0xA, 0x3C, 0x12C, 0x12C, 1 },
    { 0x471, 0x7D0, 0x3C, 0x3C, 0x12C, 0x12C, 1 },
    { -0xB80, -0x7F3, 7, 0xFFFE, 0x12C, 0x12C, 1 },
    { 0x20D, 0x4B0, 0xFFD9, 0x3D, 0x12C, 0x12C, 1 },
    { 0x2A8, 0, 9, 0xFFFD, 0x12C, 0x12C, 2 },
    { 0x1259, 0x29CC, 0x34, 0xFFE4, 0x12C, 0x12C, 2 },
    { 0x1AE, 0, 0xFFD2, 0x1A, 0x12C, 0x12C, 2 },
    { 0x1F4, 0x5DC, 0xFFF4, 0x1A, 0x12C, 0x12C, 2 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { -0xC58, 0x18E0, 0xFF9E, 0xFFC3, 0x12C, 0x12C, 1 },
    { -0x27E, 0x10CC, 0xFFD5, 0xFFFF, 0x12C, 0x12C, 3 },
    { -0xF37, 0x2526, 0xB, 0xFFBE, 0x122, 0x118, 1 },
    { 0x514, 0xC1C, 0xFFC8, 0xFFD1, 0x122, 0x118, 1 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { -0xB80, -0x7F3, 6, 0xFFFD, 0x12C, 0x12C, 1 },
    { 0x202, -0x5A0, 0xFFB5, 0x18, 0x12C, 0x12C, 1 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_END },
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

MenuMapIcon D_map_neo_ark_8017A26C[4] = {
    { 3, 4, 0, 0, -36, -21 },
    { 3, 4, MENU_MAP_ICON_KIND_TELEPHONE, 0, -51, -16 },
    { 2, 0x16, MENU_MAP_ICON_KIND_TELEPHONE, 0, 25, -2 },
    { 0, 0, 0, 0, 0, 0 },
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

static AreaObjectSpawn D_map_neo_ark_8017A6AC[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_neo_ark_8017A6BC[2] = {
    { 0x124, { { { TASK_BODY_TMD, 0x62 } }, func_shelter_1f_vehicular_airlock_8017D5E4, { &gShelter1fVehicularAirlockModel03A58 } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_neo_ark_8017A6DC[1] = {
    { AREA_OBJECT_SPAWN_END },
};

AreaObjectRoom D_map_neo_ark_8017A6EC[35] = {
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_map_neo_ark_8017C790 }, D_map_neo_ark_8017A6BC },
    { { NULL }, NULL },
    { { D_map_neo_ark_8017C7B0 }, D_map_neo_ark_8017A6DC },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_map_neo_ark_8017C890 }, D_map_neo_ark_8017A6AC },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_map_neo_ark_8017C8B0 }, D_map_neo_ark_8017A6AC },
    { { D_map_neo_ark_8017C8D0 }, D_map_neo_ark_8017A6AC },
    { { D_map_neo_ark_8017C910 }, D_map_neo_ark_8017A6AC },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_map_neo_ark_8017C930 }, D_map_neo_ark_8017A6AC },
    { { NULL }, NULL },
    { { D_map_neo_ark_8017C970 }, D_map_neo_ark_8017A6AC },
    { { NULL }, NULL },
    { { NULL }, NULL },
    { { D_map_neo_ark_8017C990 }, D_map_neo_ark_8017A6AC },
    { { .sentinel = AREA_OBJECT_ROOM_END }, NULL },
};

TaskDesc D_map_neo_ark_8017A804[] = {
    { { { TASK_BODY_NONE, 0x20 } }, func_shelter_1f_parking_garage_8017DF14, { .value = GP_TASK_LOC_KEY(5, 1, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_shelter_1f_vehicular_airlock_8017DA48, { .value = GP_TASK_LOC_KEY(5, 2, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_shelter_1f_bulwark_8017DC20, { .value = GP_TASK_LOC_KEY(5, 3, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelter1fAirlockRoomTask, { .value = GP_TASK_LOC_KEY(5, 5, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_shelter_1f_guardroom_8017D880, { .value = GP_TASK_LOC_KEY(5, 6, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_observatory_8017FDDC, { .value = GP_TASK_LOC_KEY(5, 7, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_eve_access_tunnel_8017E038, { .value = GP_TASK_LOC_KEY(5, 8, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_eve_elevator_8017D6C4, { .value = GP_TASK_LOC_KEY(5, 9, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_north_promenade_8017D6C8, { .value = GP_TASK_LOC_KEY(5, 10, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_forest_zone_8017DBBC, { .value = GP_TASK_LOC_KEY(5, 11, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_submarine_tunnel_8017F434, { .value = GP_TASK_LOC_KEY(5, 12, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_pavilion_8017EBF4, { .value = GP_TASK_LOC_KEY(5, 13, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_island_8017EB10, { .value = GP_TASK_LOC_KEY(5, 14, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_garden_8017EA44, { .value = GP_TASK_LOC_KEY(5, 15, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_savanna_zone_8017D954, { .value = GP_TASK_LOC_KEY(5, 18, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkSouthPromenadeRoomTask, { .value = GP_TASK_LOC_KEY(5, 19, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkAltarRoomTask, { .value = GP_TASK_LOC_KEY(5, 20, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkShrineRoomTask, { .value = GP_TASK_LOC_KEY(5, 21, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_shelter_b6_nursery_8017FF9C, { .value = GP_TASK_LOC_KEY(5, 22, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_shelter_b6_growth_room_8017D7D4, { .value = GP_TASK_LOC_KEY(5, 23, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB6CorridorRoomTask, { .value = GP_TASK_LOC_KEY(5, 24, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelterB6TrainingRoomRoomTask, { .value = GP_TASK_LOC_KEY(5, 25, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkBridgeRoomTask, { .value = GP_TASK_LOC_KEY(5, 27, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_r26_8017D720, { .value = GP_TASK_LOC_KEY(5, 26, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_submarine_gallery_8017EBCC, { .value = GP_TASK_LOC_KEY(5, 30, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, neoArkR31RoomTask, { .value = GP_TASK_LOC_KEY(5, 31, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_shelter_1f_tent_8017FDB8, { .value = GP_TASK_LOC_KEY(5, 28, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_woodland_path_8017E9B0, { .value = GP_TASK_LOC_KEY(5, 29, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_power_plant_2_8017D854, { .value = GP_TASK_LOC_KEY(5, 16, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_power_plant_1_8017D9C0, { .value = GP_TASK_LOC_KEY(5, 17, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, shelter1fHeliportRoomTask, { .value = GP_TASK_LOC_KEY(5, 4, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_pyramid_8017DB98, { .value = GP_TASK_LOC_KEY(5, 32, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_neo_ark_substation_8017D81C, { .value = GP_TASK_LOC_KEY(5, 33, 0) } },
    { { { TASK_DESC_END, 0x20 } }, NULL, { 0 } },
};

/// Four bytes between the task table and the flag table that nothing
/// is known to read.
s32 D_map_neo_ark_8017A99C = 0;

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

static WorldCoordRoomLighting D_map_neo_ark_8017A9B4[1] = {
    { &D_shelter_b6_nursery_80187294, D_shelter_b6_nursery_8018789C },
};

static WorldCoordRoomLighting D_map_neo_ark_8017A9BC[1] = {
    { &D_shelter_b6_growth_room_8017FF78, D_shelter_b6_growth_room_80180730 },
};

static WorldCoordRoomLighting D_map_neo_ark_8017A9C4[1] = {
    { &D_shelter_b6_corridor_801800E8, D_shelter_b6_corridor_801804E8 },
};

static WorldCoordRoomLighting D_map_neo_ark_8017A9CC[1] = {
    { &D_shelter_b6_training_room_80185768, D_shelter_b6_training_room_80185BC0 },
};

static WorldCoordRoomLighting D_map_neo_ark_8017A9D4[1] = {
    { &D_neo_ark_bridge_8018470C, NULL },
};

static WorldCoordRoomLighting D_map_neo_ark_8017A9DC[1] = {
    { &D_shelter_1f_tent_80183A7C, NULL },
};

static WorldCoordRoomLighting D_map_neo_ark_8017A9E4[1] = {
    { &D_neo_ark_woodland_path_80183F84, D_neo_ark_woodland_path_8018477C },
};

static WorldCoordRoomLighting D_map_neo_ark_8017A9EC[1] = {
    { &D_neo_ark_submarine_gallery_80185284, NULL },
};

static WorldCoordRoomLighting D_map_neo_ark_8017A9F4[1] = {
    { &D_neo_ark_r31_8017DB7C, NULL },
};

WorldCoordRoomLighting* D_map_neo_ark_8017A9FC[33] = {
    D_shelter_1f_parking_garage_80180C74,
    D_shelter_1f_vehicular_airlock_8018210C,
    D_shelter_1f_bulwark_801803C0,
    D_shelter_1f_heliport_801812E0,
    D_shelter_1f_airlock_8017E5AC,
    D_shelter_1f_guardroom_8017DA88,
    D_neo_ark_observatory_801815B4,
    D_neo_ark_eve_access_tunnel_8017EB88,
    D_neo_ark_eve_elevator_8017D75C,
    D_neo_ark_north_promenade_80181DC4,
    D_neo_ark_forest_zone_801820B4,
    D_neo_ark_submarine_tunnel_80181E00,
    D_neo_ark_pavilion_801838D4,
    D_neo_ark_island_80181BA4,
    D_neo_ark_garden_8018141C,
    D_neo_ark_power_plant_2_801806A0,
    D_neo_ark_power_plant_1_8017F1D8,
    D_neo_ark_savanna_zone_8017F9F4,
    D_neo_ark_south_promenade_8017F6EC,
    D_neo_ark_altar_8017F0C4,
    D_neo_ark_shrine_80182784,
    D_map_neo_ark_8017A9B4,
    D_map_neo_ark_8017A9BC,
    D_map_neo_ark_8017A9C4,
    D_map_neo_ark_8017A9CC,
    D_neo_ark_r26_8017E0DC,
    D_map_neo_ark_8017A9D4,
    D_map_neo_ark_8017A9DC,
    D_map_neo_ark_8017A9E4,
    D_map_neo_ark_8017A9EC,
    D_map_neo_ark_8017A9F4,
    D_neo_ark_pyramid_8017FC48,
    D_neo_ark_substation_8017E400,
};

DirectionWarpEntry* D_map_neo_ark_8017AA80[33] = {
    D_shelter_1f_parking_garage_80180C84,
    D_shelter_1f_vehicular_airlock_8018211C,
    D_shelter_1f_bulwark_801803D0,
    D_shelter_1f_heliport_801812F0,
    D_shelter_1f_airlock_8017E5BC,
    D_shelter_1f_guardroom_8017DA98,
    D_neo_ark_observatory_801815E8,
    D_neo_ark_eve_access_tunnel_8017EB98,
    D_neo_ark_eve_elevator_8017D76C,
    D_neo_ark_north_promenade_80181DD4,
    D_neo_ark_forest_zone_801820C4,
    D_neo_ark_submarine_tunnel_80181E20,
    D_neo_ark_pavilion_801838F8,
    D_neo_ark_island_80181BB4,
    D_neo_ark_garden_8018142C,
    D_neo_ark_power_plant_2_801806B0,
    D_neo_ark_power_plant_1_8017F1E8,
    D_neo_ark_savanna_zone_8017FA04,
    D_neo_ark_south_promenade_8017F70C,
    D_neo_ark_altar_8017F0FC,
    D_neo_ark_shrine_80182814,
    D_shelter_b6_nursery_8018530C,
    D_shelter_b6_growth_room_8017F380,
    D_shelter_b6_corridor_8017F8BC,
    D_shelter_b6_training_room_80184420,
    D_neo_ark_r26_8017E0EC,
    D_neo_ark_bridge_80181F88,
    D_shelter_1f_tent_80181D4C,
    D_neo_ark_woodland_path_8018169C,
    D_neo_ark_submarine_gallery_80181A10,
    D_neo_ark_r31_8017DA24,
    D_neo_ark_pyramid_8017FC6C,
    D_neo_ark_substation_8017E410,
};

static ViewCount* D_map_neo_ark_8017AB04[33] = {
    D_shelter_1f_parking_garage_80180C80,
    D_shelter_1f_vehicular_airlock_80182118,
    D_shelter_1f_bulwark_801803CC,
    D_shelter_1f_heliport_801812EC,
    D_shelter_1f_airlock_8017E5B8,
    D_shelter_1f_guardroom_8017DA94,
    D_neo_ark_observatory_801815E4,
    D_neo_ark_eve_access_tunnel_8017EB94,
    D_neo_ark_eve_elevator_8017D768,
    D_neo_ark_north_promenade_80181DD0,
    D_neo_ark_forest_zone_801820C0,
    D_neo_ark_submarine_tunnel_80181E1C,
    D_neo_ark_pavilion_801838F4,
    D_neo_ark_island_80181BB0,
    D_neo_ark_garden_80181428,
    D_neo_ark_power_plant_2_801806AC,
    D_neo_ark_power_plant_1_8017F1E4,
    D_neo_ark_savanna_zone_8017FA00,
    D_neo_ark_south_promenade_8017F708,
    D_neo_ark_altar_8017F0F8,
    D_neo_ark_shrine_80182808,
    D_shelter_b6_nursery_80185308,
    D_shelter_b6_growth_room_8017F37C,
    D_shelter_b6_corridor_8017F8B8,
    D_shelter_b6_training_room_8018441C,
    D_neo_ark_r26_8017E0E8,
    D_neo_ark_bridge_80181F84,
    D_shelter_1f_tent_80181D48,
    D_neo_ark_woodland_path_80181698,
    D_neo_ark_submarine_gallery_80181A0C,
    D_neo_ark_r31_8017DA20,
    D_neo_ark_pyramid_8017FC68,
    D_neo_ark_substation_8017E40C,
};

ViewCountTable D_map_neo_ark_8017AB88 = { D_map_neo_ark_8017AB04 };

static WorldCollisionRoomResources D_map_neo_ark_8017AB8C[1] = {
    { &D_shelter_b6_nursery_801858A0, D_shelter_b6_nursery_801872AC, D_shelter_b6_nursery_8018750C, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017AB9C[1] = {
    { &D_shelter_b6_growth_room_8017FAF0, D_shelter_b6_growth_room_8017FF90, D_shelter_b6_growth_room_801803A0, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017ABAC[1] = {
    { &D_shelter_b6_corridor_8017FA90, D_shelter_b6_corridor_80180100, D_shelter_b6_corridor_8018036C, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017ABBC[1] = {
    { &D_shelter_b6_training_room_80184734, D_shelter_b6_training_room_80185780, D_shelter_b6_training_room_80185A44, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017ABCC[1] = {
    { &D_neo_ark_bridge_80182814, D_neo_ark_bridge_80184724, D_neo_ark_bridge_80184AB8, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017ABDC[1] = {
    { &D_shelter_1f_tent_801822F0, D_shelter_1f_tent_80183A94, D_shelter_1f_tent_80183CF4, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017ABEC[1] = {
    { &D_neo_ark_woodland_path_80181D5C, D_neo_ark_woodland_path_80183F9C, D_neo_ark_woodland_path_8018445C, D_neo_ark_woodland_path_801847D4 },
};

static WorldCollisionRoomResources D_map_neo_ark_8017ABFC[1] = {
    { &D_neo_ark_submarine_gallery_8018239C, D_neo_ark_submarine_gallery_8018529C, D_neo_ark_submarine_gallery_801854FC, NULL },
};

static WorldCollisionRoomResources D_map_neo_ark_8017AC0C[1] = {
    { NULL, NULL, NULL, NULL },
};

static WorldCollisionRoomResources* D_map_neo_ark_8017AC1C[33] = {
    D_shelter_1f_parking_garage_80180C64,
    D_shelter_1f_vehicular_airlock_801820FC,
    D_shelter_1f_bulwark_801803B0,
    D_shelter_1f_heliport_801812D0,
    D_shelter_1f_airlock_8017E59C,
    D_shelter_1f_guardroom_8017DA78,
    D_neo_ark_observatory_80181594,
    D_neo_ark_eve_access_tunnel_8017EB78,
    D_neo_ark_eve_elevator_8017D74C,
    D_neo_ark_north_promenade_80181DB4,
    D_neo_ark_forest_zone_801820A4,
    D_neo_ark_submarine_tunnel_80181E08,
    D_neo_ark_pavilion_801838B4,
    D_neo_ark_island_80181B94,
    D_neo_ark_garden_8018140C,
    D_neo_ark_power_plant_2_80180690,
    D_neo_ark_power_plant_1_8017F1C8,
    D_neo_ark_savanna_zone_8017F9E4,
    D_neo_ark_south_promenade_8017F6F4,
    D_neo_ark_altar_8017F094,
    D_neo_ark_shrine_80182724,
    D_map_neo_ark_8017AB8C,
    D_map_neo_ark_8017AB9C,
    D_map_neo_ark_8017ABAC,
    D_map_neo_ark_8017ABBC,
    D_neo_ark_r26_8017E0CC,
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
    D_shelter_1f_parking_garage_8018100C,
    D_shelter_1f_vehicular_airlock_8018245C,
    D_shelter_1f_bulwark_8018066C,
    D_shelter_1f_heliport_80181998,
    D_shelter_1f_airlock_8017E85C,
    D_shelter_1f_guardroom_8017DC14,
    D_neo_ark_observatory_80181FC8,
    D_neo_ark_eve_access_tunnel_8017F080,
    D_neo_ark_eve_elevator_8017DA50,
    D_neo_ark_north_promenade_80182410,
    D_neo_ark_forest_zone_80182298,
    D_neo_ark_submarine_tunnel_80182500,
    D_neo_ark_pavilion_80184208,
    D_neo_ark_island_801826EC,
    D_neo_ark_garden_801816E8,
    D_neo_ark_power_plant_2_80180DE8,
    D_neo_ark_power_plant_1_801800B4,
    D_neo_ark_savanna_zone_8017FBF4,
    D_neo_ark_south_promenade_8017FDB0,
    D_neo_ark_altar_8017F5A0,
    D_neo_ark_shrine_801836BC,
    D_shelter_b6_nursery_801858C4,
    D_shelter_b6_growth_room_8017FB14,
    D_shelter_b6_corridor_8017FAB4,
    D_shelter_b6_training_room_80184758,
    D_neo_ark_r26_8017E1C0,
    D_neo_ark_bridge_80182838,
    D_shelter_1f_tent_80182314,
    D_neo_ark_woodland_path_80181D80,
    D_neo_ark_submarine_gallery_801823C0,
    D_neo_ark_r31_8017DA5C,
    D_neo_ark_pyramid_801802E8,
    D_neo_ark_substation_8017E8C8,
};

ViewCameraTable D_map_neo_ark_8017AD28 = { D_map_neo_ark_8017ACA4 };

static u8** D_map_neo_ark_8017AD2C[33] = {
    D_shelter_1f_parking_garage_80180C7C,
    D_shelter_1f_vehicular_airlock_80182114,
    D_shelter_1f_bulwark_801803C8,
    D_shelter_1f_heliport_801812E8,
    D_shelter_1f_airlock_8017E5B4,
    D_shelter_1f_guardroom_8017DA90,
    D_neo_ark_observatory_801815DC,
    D_neo_ark_eve_access_tunnel_8017EB90,
    D_neo_ark_eve_elevator_8017D764,
    D_neo_ark_north_promenade_80181DCC,
    D_neo_ark_forest_zone_801820BC,
    D_neo_ark_submarine_tunnel_80181E18,
    D_neo_ark_pavilion_801838EC,
    D_neo_ark_island_80181BAC,
    D_neo_ark_garden_80181424,
    D_neo_ark_power_plant_2_801806A8,
    D_neo_ark_power_plant_1_8017F1E0,
    D_neo_ark_savanna_zone_8017F9FC,
    D_neo_ark_south_promenade_8017F704,
    D_neo_ark_altar_8017F0EC,
    D_neo_ark_shrine_801827F0,
    D_shelter_b6_nursery_80185304,
    D_shelter_b6_growth_room_8017F378,
    D_shelter_b6_corridor_8017F8B4,
    D_shelter_b6_training_room_80184418,
    D_neo_ark_r26_8017E0E4,
    D_neo_ark_bridge_80181F80,
    D_shelter_1f_tent_80181D44,
    D_neo_ark_woodland_path_80181694,
    D_neo_ark_submarine_gallery_80181A08,
    D_neo_ark_r31_8017DA1C,
    D_neo_ark_pyramid_8017FC60,
    D_neo_ark_substation_8017E408,
};

ViewIndexTable D_map_neo_ark_8017ADB0 = { D_map_neo_ark_8017AD2C };

static SpriteView* D_map_neo_ark_8017ADB4[33] = {
    D_shelter_1f_parking_garage_80181430,
    D_shelter_1f_vehicular_airlock_801824F8,
    D_shelter_1f_bulwark_801807B0,
    D_shelter_1f_heliport_80181EC0,
    D_shelter_1f_airlock_8017F07C,
    D_shelter_1f_guardroom_8017DCE0,
    D_neo_ark_observatory_801860E8,
    D_neo_ark_eve_access_tunnel_801800A0,
    D_neo_ark_eve_elevator_8017DB20,
    D_neo_ark_north_promenade_80182CA4,
    D_neo_ark_forest_zone_80182594,
    D_neo_ark_submarine_tunnel_80186B78,
    D_neo_ark_pavilion_801873B8,
    D_neo_ark_island_80183B14,
    D_neo_ark_garden_80182540,
    D_neo_ark_power_plant_2_8018205C,
    D_neo_ark_power_plant_1_801814F0,
    D_neo_ark_savanna_zone_801803F4,
    D_neo_ark_south_promenade_801803E4,
    D_neo_ark_altar_8017FE38,
    D_neo_ark_shrine_80185280,
    D_shelter_b6_nursery_80186FD0,
    D_shelter_b6_growth_room_8017FEB8,
    D_shelter_b6_corridor_8018004C,
    D_shelter_b6_training_room_80184D78,
    D_neo_ark_r26_8017E898,
    D_neo_ark_bridge_80184564,
    D_shelter_1f_tent_801838E4,
    D_neo_ark_woodland_path_80183C6C,
    D_neo_ark_submarine_gallery_80184D10,
    D_neo_ark_r31_8017DAF8,
    D_neo_ark_pyramid_80180E18,
    D_neo_ark_substation_8017F584,
};

SpriteAreaTable D_map_neo_ark_8017AE38 = { D_map_neo_ark_8017ADB4 };

WorldCollisionSurfaceProperties** D_map_neo_ark_8017AE3C[33] = {
    D_shelter_1f_parking_garage_80181954,
    D_shelter_1f_vehicular_airlock_80182A80,
    D_shelter_1f_bulwark_80180E9C,
    D_shelter_1f_heliport_80182C78,
    D_shelter_1f_airlock_8017F84C,
    D_shelter_1f_guardroom_8017DFF4,
    D_neo_ark_observatory_80187A08,
    D_neo_ark_eve_access_tunnel_80180780,
    D_neo_ark_eve_elevator_8017DC30,
    D_neo_ark_north_promenade_801832EC,
    D_neo_ark_forest_zone_80182CE4,
    D_neo_ark_submarine_tunnel_801878EC,
    D_neo_ark_pavilion_801879EC,
    D_neo_ark_island_80183FE8,
    D_neo_ark_garden_80182BD8,
    D_neo_ark_power_plant_2_80182F50,
    D_neo_ark_power_plant_1_80181BE0,
    D_neo_ark_savanna_zone_80180968,
    D_neo_ark_south_promenade_801809AC,
    D_neo_ark_altar_8018005C,
    D_neo_ark_shrine_80186844,
    D_shelter_b6_nursery_80187958,
    D_shelter_b6_growth_room_801807A8,
    D_shelter_b6_corridor_80180548,
    D_shelter_b6_training_room_80185C38,
    D_neo_ark_r26_8017EA30,
    D_neo_ark_bridge_80184BD4,
    D_shelter_1f_tent_801842B4,
    D_neo_ark_woodland_path_80184910,
    D_neo_ark_submarine_gallery_801858EC,
    D_neo_ark_r31_8017DC34,
    D_neo_ark_pyramid_80181884,
    D_neo_ark_substation_80180328,
};

AreaPlacement D_map_neo_ark_8017AEC0[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017AED0[3] = {
    { 0x14, 0, 0, -0x6A4, 0, 0, 0xDAC, 0, 0, 2, 0 },
    { 0x39, 0, 0, -0x2328, 0, 0, 0x400, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017AF00[3] = {
    { 0x17, 0, 0, -0xFA0, 0, -0x3E8, 0x400, 0, 0, 2, 0 },
    { 0x39, 0, 0, -0xFA0, 0, 0x3E8, 0x400, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017AF30[5] = {
    { 0x73, 0, 0, 0x6B3, 0, 0x1AA2, 0x800, 0, 0, 2, 0 },
    { 0x74, 0, 1, 0x35C, 0, 0xBF5, 0x400, 0, 2, 4, 0 },
    { 0x74, 0, 1, 0x2710, 0, 0x131C, 0xC00, 0, 2, 4, 0 },
    { 0x74, 0, 0, 0x1CB5, 0, -0x3C9, 0x200, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017AF80[5] = {
    { 0x90, 0, 0, 0x6B3, 0, 0x1AA2, 0x800, 0, 0, 2, 0 },
    { 0x74, 0, 1, 0x35C, 0, 0xBF5, 0x400, 0, 2, 4, 0 },
    { 0x74, 0, 1, 0x2710, 0, 0x131C, 0xC00, 0, 2, 4, 0 },
    { 0x74, 0, 0, 0x1CB5, 0, -0x3C9, 0x200, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017AFD0[2] = {
    { 0x16, 0, 0, -0x5DC, 0, 0x12C0, 0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017AFF0[2] = {
    { 0x27, 0, 0, -0x5DC, 0, 0x12C0, 0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B010[2] = {
    { 0x65, 0, 0, 0, 0, 0, 0, 0, -1, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B030[5] = {
    { 0x15, 2, 0, -0x190, -0x7D0, 0x1900, 0xC00, 0, 0, 2, 6 },
    { 0x15, 2, 0, -0x802, -0x7D0, 0x1900, 0x400, 0, 0, 2, 6 },
    { 0x15, 3, 0, -0x190, -0x7D0, 0xE10, 0xC00, 0, 0, 2, 6 },
    { 0x15, 3, 0, -0x802, -0x7D0, 0xE10, 0x400, 0, 0, 2, 6 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B080[7] = {
    { 0x34, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x15, 2, 0, -0x190, -0x7D0, 0x1900, 0xC00, 0, 2, 4, 6 },
    { 0x15, 2, 0, -0x802, -0x7D0, 0x1900, 0x400, 0, 2, 4, 6 },
    { 0x15, 3, 0, -0x190, -0x7D0, 0xE10, 0xC00, 0, 2, 4, 6 },
    { 0x15, 3, 0, -0x802, -0x7D0, 0xE10, 0x400, 0, 2, 4, 6 },
    { 0x15, 1, 0, -0x190, -0x7D0, 0x1388, 0xC00, 0, 2, 4, 6 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B0F0[3] = {
    { 3, 0, 0, 0x1194, 0, 0x2CEC, 0x400, 0, 0, 2, 0 },
    { 3, 0, 1, 0x2C24, 0, 0x1194, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B120[4] = {
    { 0x14, 0, 0, 0x2486, 0, 0xABE, 0, 0, 0, 2, 0 },
    { 0x14, 5, 1, 0x2DB4, 0, 0xFA0, 0, 0, 0, 2, 0 },
    { 0x14, 5, 1, 0x1964, 0, 0x2BC0, 0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B160[3] = {
    { 0x38, 4, 1, 0x251C, 0, 0x2904, 0xC00, 0, 0, 2, 0 },
    { 0x38, 0, 0, 0xBB8, 0, 0x2EE0, 0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B190[3] = {
    { 0xD, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0xA, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B1C0[3] = {
    { 0xD, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0xD, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B1F0[2] = {
    { 0xD, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0xD, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
};

AreaPlacement D_map_neo_ark_8017B210[11] = {
    { 0x26, 0, 0, -0xF6E, 0, 0x64, 0x672, 0, 0, 2, 0 },
    { 0x26, 0, 1, -0x1162, 0, -0x384, 0xCB2, 0, 0, 2, 0 },
    { 0x26, 0, 1, -0x1194, 0, 0x3E8, 0xA5A, 0, 0, 2, 0 },
    { 0x26, 0, 1, -0xA8C, 0, 0, 0xB54, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0x2BC, 0, -0x384, 0x992, 0, 0, 2, 0 },
    { 0x26, 0, 0, 0x41A, 0, 0x258, 0x4B0, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0x60E, 0, -0x384, 0x1F4, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0x9C4, 0, 0xC8, 0xABE, 0, 0, 2, 0 },
    { 0x26, 0, 0, 0xB22, 0, 0x384, 0x5AA, 0, 0, 2, 0 },
    { 0x26, 0, 0, 0xFA0, 0, -0x12C, 0x320, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B2C0[4] = {
    { 0x14, 2, 1, -0xDAC, 0, 0, 0x400, 0, 0, 2, 0 },
    { 0xD, 0x20, 4, -0x15E0, 0, -0x3E8, 0xD48, 0, 3, 5, 0 },
    { 0xD, 0x20, 4, 0x73A, 0, 0x3E8, 0x4B0, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B300[3] = {
    { 0x14, 5, 1, 0x9C4, 0, 0, 0xC00, 0, 0, 2, 0 },
    { 0x38, 3, 1, -0xFA0, 0, 0, 0xC00, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B330[3] = {
    { 4, 0, 2, -0x780, 0, 0x700, 0x800, 0, 0, 2, 0 },
    { 4, 0, 0x32, 0x1F40, 0, 0x1964, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B360[3] = {
    { 0x18, 0, 1, 0x1F4, 0xBB8, 0x190, 0xBB8, 0, 0, 2, 0 },
    { 0x18, 0, 1, -0xFA, 0xBB8, -0x190, 0xD16, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B390[3] = {
    { 0x84, 0, 0, 0x5DC, 0xBB8, 0, 0xC00, 0, 0, 2, 0 },
    { 0x22, 0, 0, 0x5DC, 0xBB8, 0, 0xC00, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B3C0[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B3D0[4] = {
    { 4, 0, 1, 0, 0, 0x1120, 0x800, 0, 0, 2, 1 },
    { 4, 0, 0x11, -0x1B80, 0, 0x2460, 0x800, 0, 0, 2, 1 },
    { 4, 0, 0x21, -0x1B58, 0, -0x1820, 0x800, 0, 0, 2, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B410[6] = {
    { 4, 0, 1, 0, 0, 0x1120, 0x800, 0, 0, 2, 3 },
    { 4, 0, 0x11, -0x1B80, 0, 0x2460, 0x800, 0, 0, 2, 3 },
    { 0x26, 0, 1, -0x12C, 0, 0x2580, 0xAF0, 0, 2, 4, 0 },
    { 0x26, 0, 1, -0xB54, 0, 0x2A30, 0x60E, 0, 2, 4, 0 },
    { 0x26, 0, 1, 0x190, 0, 0x2B5C, 0xBB8, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B470[4] = {
    { 4, 0, 1, 0, 0, 0x1120, 0x800, 0, 0, 2, 3 },
    { 4, 0, 0x11, -0x1B80, 0, 0x2460, 0x800, 0, 0, 2, 3 },
    { 4, 0, 0x21, -0x1B58, 0, -0x1820, 0x800, 0, 0, 2, 3 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B4B0[3] = {
    { 4, 0, 1, 0, 0, 0x1120, 0x800, 0, 0, 2, 6 },
    { 0x31, 0, 0, 0x12C, 0, 0x2AF8, 0xA28, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B4E0[5] = {
    { 4, 0, 6, 0x7D0, 0, 0x2AF8, 0x384, 0, 0, 2, 0 },
    { 4, 0, 7, -0xED8, 0, 0x32C8, 0x400, 0, 0, 2, 0 },
    { 4, 0, 7, -0x2EE0, 0, 0x2328, -0x12C, 0, 0, 2, 0 },
    { 4, 0, 7, -0x2328, 0, 0x2AF8, 0x258, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B530[2] = {
    { 0x39, 3, 1, -0x1F4, 0, 0x251C, 0xC00, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B550[4] = {
    { 4, 0, 1, 0, 0, 0x1120, 0x800, 0, 0, 2, 1 },
    { 4, 0, 0x11, 0x1770, 0, 0x9C4, 0x800, 0, 0, 2, 1 },
    { 4, 0, 0x21, 0x16A8, 0, 0x12C0, 0x800, 0, 0, 2, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B590[3] = {
    { 0x31, 2, 0, 0xBB8, 0, -0x1450, 0, 0, 0, 2, 0 },
    { 0x31, 1, 0, 0xA8C, 0, -0x12C, 0x76C, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B5C0[4] = {
    { 4, 0, 6, 0xDAC, -0x64, -0x1218, 0x100, 0, 0, 2, 0 },
    { 4, 0, 6, 0x13EC, -0x64, -0xFA0, 0xB00, 0, 0, 2, 0 },
    { 4, 0, 7, 0x3DE, 0, 0x960, 0x100, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017B600[1] = {
    { AREA_PLACEMENT_END },
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

AreaPlacement D_map_neo_ark_8017C070[2] = {
    { 0x27, 0, 0, 0x1B58, 0, 0x5DC, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C090[5] = {
    { 0x65, 1, 0, 0x173E, 0, 0x960, 0xC00, 0, -1, 0x10, 0 },
    { 0x14, 0, 0, 0x173E, 0, 0x1518, 0x800, 0, 1, 2, 0 },
    { 0x14, 0, 0, 0x173E, 0, 0x1518, 0x800, 0, 1, 2, 0 },
    { 0x8C, 0, 0, 0x173E, 0, 0x960, 0xC00, 0, -1, 0x10, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C0E0[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C0F0[5] = {
    { 0x83, 0, 0, 0x1F40, 0, 0, 0x400, 0, 4, 6, 0 },
    { 0x31, 1, 0, 0x1964, 0, 0x12C, 0xA8C, 0, 0, 2, 0 },
    { 0x34, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x3C, 0, 0, 0x1F40, 0, 0, 0xC00, 0, 4, 6, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C140[6] = {
    { 0x83, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0 },
    { 0x8C, 0, 0, 0, 0, 0, 0, 0, 4, 6, 0 },
    { 0x33, 0, 0, 0x9C4, 0, 0x2134, 0x800, 0, 0, 2, 0 },
    { 0x34, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x3C, 0, 1, 0x1194, 0, 0x2580, 0x800, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

static AreaPlacement D_map_neo_ark_8017C1A0[3] = {
    { 0x6F, 0, 0, 0, 0, 0xB22, 0x800, 0, 0, 2, 0 },
    { 0x70, 0, 0, 0, 0, 0x352, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C1D0[4] = {
    { 4, 0, 1, 0, 0, 0x1120, 0x800, 0, 0, 2, 1 },
    { 4, 0, 0x11, -0x1B80, 0, -0x2710, 0x800, 0, 0, 2, 1 },
    { 4, 0, 0x21, -0x1B58, 0, 0x7D0, 0x800, 0, 0, 2, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C210[6] = {
    { 4, 0, 1, 0, 0, 0x1120, 0x800, 0, 0, 2, 3 },
    { 4, 0, 0x11, -0x1B80, 0, -0x2710, 0x800, 0, 0, 2, 3 },
    { 0x26, 0, 1, -0xC4E, 0, -0x1C20, 0x320, 0, 2, 4, 0 },
    { 0x26, 0, 1, -0x9C4, 0, -0x16DA, 0x898, 0, 2, 4, 0 },
    { 0x26, 0, 1, -0x546, 0, -0x47E, 0xD48, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C270[3] = {
    { 4, 0, 1, 0, 0, 0x1120, 0x800, 0, 0, 2, 6 },
    { 0x31, 1, 0, -0xC1C, 0, -0x5DC, 0x5DC, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C2A0[4] = {
    { 4, 0, 1, 0, 0, 0x1120, 0x800, 0, 0, 2, 7 },
    { 4, 0, 0x11, -0x1B80, 0, -0x2710, 0x800, 0, 0, 2, 7 },
    { 0x31, 1, 0, -0xC1C, 0, -0x1F40, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C2E0[4] = {
    { 4, 0, 6, -0x7D0, -0x64, -0x6A4, 0xD00, 0, 0, 2, 0 },
    { 4, 0, 7, -0x320, 0, -0x1BBC, 0xC8, 0, 0, 2, 0 },
    { 4, 0, 7, 0, 0, 0xDAC, 0x4B0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C320[3] = {
    { 0x17, 6, 1, -0xBB8, 0, 0, 0, 0, 0, 2, 0 },
    { 0x17, 8, 1, -0xBB8, 0, -0xBB8, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C350[3] = {
    { 0x17, 6, 1, -0xBB8, 0, 0, 0, 0, 0, 2, 0 },
    { 0x17, 8, 1, -0xBB8, 0, -0xBB8, 0x800, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C380[4] = {
    { 0x71, 0, 0, -0x834, 0, 0x11F8, -0x190, 0, 0, 2, 0 },
    { 0x74, 0, 0, -0xFA, 0, 0x1194, -0x400, 0, 2, 4, 0 },
    { 0x75, 0, 0, -0x10B8, 0, 0x209E, 0, 0, 4, 6, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C3C0[3] = {
    { 0xD, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0xD, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C3F0[3] = {
    { 0xD, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0xD, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C420[3] = {
    { 0xD, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0xD, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C450[5] = {
    { 0xD, 0x20, 4, 0xED8, 0, 0x125C, 0x258, 0, 0, 2, 0 },
    { 0xD, 0x20, 4, 0x8FC, 0, -0xC80, 0x6A4, 0, 0, 2, 0 },
    { 0xD, 0x20, 4, 0xA8C, 0, -0x1068, 0xE74, 0, 0, 2, 0 },
    { 0xD, 0x20, 4, -0xC8, 0, -0x10CC, 0x578, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C4A0[3] = {
    { 0x3D, 0, 0, 0, 0x1F40, 0x1388, 0x800, 0, 2, 4, 0 },
    { 4, 1, 3, 0, 0x1F40, 0x1388, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C4D0[4] = {
    { 4, 0, 1, 0, 0x1F40, 0, 0x800, 0, 0, 2, 3 },
    { 4, 0, 0x11, 0, 0x1F40, 0, 0x800, 0, 0, 2, 3 },
    { 4, 0, 0x21, 0, 0x1F40, 0, 0x800, 0, 0, 2, 3 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C510[2] = {
    { 4, 0, 7, 0, 0x1388, 0, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C530[2] = {
    { 4, 0, 1, 0, 0x1F40, 0, 0x800, 0, 0, 2, 7 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C550[3] = {
    { 0x65, 0, 0, 0x204E, 0x348, 0x1CA2, -0x5C8, 0, 0, 2, 0 },
    { 0x84, 0, 0, 0x1B1C, 0x3D4, 0x1BC6, -0x71C, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C580[10] = {
    { 0x26, 0, 0xA, 0x1B58, -0x3E8, -0x157C, 0x400, 0, 0, 2, 0 },
    { 0x26, 0, 0xA, 0x1964, -0x708, -0x1900, 0x800, 0, 0, 2, 0 },
    { 0x26, 0, 0xA, 0x1B58, -0x3E8, -0x27D8, 0x400, 0, 0, 2, 0 },
    { 0x26, 0, 0xA, 0x1770, -0x6D6, -0x2134, 0, 0, 0, 2, 0 },
    { 0x26, 0, 0xA, 0x10FE, -0x384, -0x2134, 0, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0xDAC, 0, -0x25E4, 0x4E2, 0, 0, 2, 0 },
    { 0x26, 0, 1, -0x2BC, 0, -0x1C20, 0xE74, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0x64, 0, -0x1A2C, 0xDAC, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0x5DC, 0, -0x157C, 0xA5A, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C620[7] = {
    { 0x1A, 0, 2, -0x514, -0xFA0, -0x1068, 0x898, 0, 0, 2, 3 },
    { 0x1A, 0, 3, -0xA8C, -0x1B58, -0xCB2, 0x7D0, 0, 0, 2, 1 },
    { 0x1A, 0, 2, -0x992, -0x1194, -0x1518, 0x258, 0, 0, 2, 1 },
    { 0x1A, 0, 3, 0x258, -0x1388, -0x1612, 0xC80, 0, 0, 2, 1 },
    { 0x1A, 0, 3, 0x190, -0xED8, -0x1BEE, 0xE42, 0, 0, 2, 2 },
    { 0x1A, 0, 3, -0x3B6, -0x1770, -0x1B58, 0xC8, 0, 0, 2, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C690[9] = {
    { 0x26, 0, 1, -0x1F4, 0, -0x1770, 0x4E2, 0, 0, 2, 0 },
    { 0x26, 0, 1, -0x12C, 0, -0x1DE2, 0xC1C, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0xFA0, 0, -0x15E0, 0xC00, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0x1388, 0, -0x251C, 0xC00, 0, 0, 2, 0 },
    { 0x26, 0, 1, 0x190, 0, -0x13EC, 0xE42, 0, 0, 2, 0 },
    { 0x1A, 0, 3, -0x514, -0xFA0, -0x1068, 0x898, 0, 2, 4, 3 },
    { 0x1A, 0, 3, -0x992, -0x12C0, -0x1518, 0x258, 0, 2, 4, 1 },
    { 0x1A, 0, 2, 0x190, -0xDAC, -0x1BEE, 0xE42, 0, 2, 4, 2 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C720[4] = {
    { 0x14, 0, 0, 0x92E, 0, -0x16DA, 0xB54, 0, 0, 2, 0 },
    { 0x14, 2, 1, 0x41A, 0, -0x238C, 0xF3C, 0, 0, 2, 0 },
    { 0xD, 0x20, 4, -0xD48, 0, -0x1068, 0x7D0, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_neo_ark_8017C760[3] = {
    { 0x17, 3, 1, 0x4E2, 0, -0x1770, 0xCE4, 0, 0, 2, 0 },
    { 0x39, 4, 1, -0x5DC, 0, -0x18CE, 0xF0A, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END },
};

static AreaObjectPlace D_map_neo_ark_8017C790[2] = {
    { 6, 0x124, 0, 0x301, -0x1C21, 2, 0x484, 0x4F0 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_neo_ark_8017C7B0[14] = {
    { 2, 0x128, 0, 0x201 },
    { 3, 7, 0, 1 },
    { 4, 0xA7, 0, 1 },
    { 5, 0x88, 0, 1 },
    { 0xB, 0x703, 0, 1 },
    { 0xC, 0x704, 0, 1 },
    { 0xD, 0x705, 0, 1 },
    { 0xE, 0x706, 0, 1 },
    { 0xF, 0xA0, 0, 3 },
    { 0x10, 0xA1, 0, 3 },
    { 0x11, 0xAC, 0, 3 },
    { 0x12, 0xA9, 0, 3 },
    { 0x13, 0xAF, 0, 3 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_neo_ark_8017C890[2] = {
    { 0x15, 0xB, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_neo_ark_8017C8B0[2] = {
    { 7, 7, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_neo_ark_8017C8D0[4] = {
    { 1, 0x80D, 0, 1 },
    { 0x16, 0x3D, 0, 1 },
    { 0x17, 0xE, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_neo_ark_8017C910[2] = {
    { 0x18, 0x3C, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_neo_ark_8017C930[4] = {
    { 0x2E, 0x80E, 0, 1 },
    { 8, 0xAA, 0, 1 },
    { 9, 0x3D, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_neo_ark_8017C970[2] = {
    { 0x14, 0x36, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_neo_ark_8017C990[2] = {
    { 0xA, 0xA1, 0, 3 },
    { 0xFFFF },
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

StageMusicEntry D_map_neo_ark_8017CB54[340] = {
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
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x59, 0 },
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
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x59, 0 },
    { 0x4C, 1 },
    { 0x5D, 0 },
    { 0x4C, 0 },
    { 0x4C, 0 },
    { 0x4C, 0 },
    { 0x4C, 0 },
    { 0x4C, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0x49, 0 },
    { 0xFF, 0 },
    { 0x49, 0 },
    { 0x49, 0 },
    { 0x49, 0 },
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
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4C, 0 },
    { 0x4C, 0 },
    { 0x4C, 0 },
    { 0x4C, 0 },
    { 0x4C, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x52, 1 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x53, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x53, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4F, 0 },
    { 0x4F, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4F, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4C, 0 },
    { 0x4C, 0 },
    { 0x4C, 0 },
    { 0x4C, 0 },
    { 0x4C, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4E, 0 },
    { 0x4E, 0 },
    { 0x4E, 0 },
    { 0x4E, 0 },
    { 0x4E, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x55, 0 },
    { 0x43, 1 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x43, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x49, 0 },
    { 0x49, 0 },
    { 0x49, 0 },
    { 0x49, 0 },
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
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x56, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x53, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x3B, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x5D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x4D, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x52, 0 },
    { 0xFF, 0 },
    { 0x59, 0 },
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
    { 0x5D, 0 },
    { 0x4E, 0 },
    { 0x4E, 0 },
    { 0x4E, 0 },
    { 0x4E, 0 },
    { 0x4E, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x4F, 0 },
    { 0x4F, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x52, 0 },
    { 0x53, 0 },
    { 0x59, 0 },
};

StageMusicEntry D_map_neo_ark_8017CDFC[20] = {
    { 0x44, 2 },
    { 0x4F, 2 },
    { 0x49, 2 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
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
