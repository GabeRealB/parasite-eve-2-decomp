#include "room_effects.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "actors/actor_510900.h"

#include "actors/actor_800100.h"

#include "main/display.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

#include "pe/antibody.h"

#include "pe/apobiosis.h"

#include "pe/combustion.h"

#include "pe/energyball.h"

#include "pe/energyshot.h"

#include "pe/flare.h"

#include "pe/healing.h"

#include "pe/inferno.h"

#include "pe/lifedrain.h"

#include "pe/metabolism.h"

#include "pe/necrosis.h"

#include "pe/ofuda.h"

#include "pe/pepper_spray.h"

#include "pe/plasma.h"

#include "pe/pyrokinesis.h"

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

#include "rooms/acropolis_west_elevator_hall.h"

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

#include "rooms/mist_shooting_gallery.h"

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

#include "rooms/shelter_r47.h"

#include "rooms/shelter_r48.h"

#include "rooms/shelter_r49.h"

#include "weapons/gunblade.h"

#include "weapons/hypervelocity.h"

#include "weapons/m4a1_bayonet.h"

#include "weapons/m4a1_hammer.h"

#include "weapons/m4a1_javelin.h"

#include "weapons/m4a1_pyke.h"

#include "weapons/mp5a5.h"

#include "weapons/p229.h"

#include "weapons/tonfa_baton.h"

/// World +Y distance of a ground probe, in game-coordinate units.
enum { WORLD_COLLISION_GROUND_PROBE_LENGTH = 0x1000 };

/// Temporary view-space segment for projecting a position onto room geometry.
///
/// XYZ coordinates use signed 16-bit game units; narrowing the source and adding
/// the rotated offset retain only the low 16 bits. The endpoint also receives
/// the accepted hit. Both SDK fourth halfwords are unused and uninitialized.
/// This word-aligned scratch-stack block stays live through nested collision
/// queries and is released before its caller returns.
typedef struct {
    SVECTOR origin;   // Fixed view-space source position, narrowed from 32-bit XYZ
    SVECTOR endpoint; // World +Y offset, then view-space segment end, then accepted hit
} _WorldCollisionGroundProbeScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionGroundProbeScratch, 0x10);

/* Define BSS before API headers to preserve first-declaration order. */
s32 gRoomEffectSparkEmitterId;

s32 gEnergyBallInFlightCount;

s32 gRoomEffectMoteId;

s32 gRoomEffectTwinTrailId;

s32 gRoomEffectFlyingSparkId;

s32 gRoomEffectGlowDiscId;

s32 gRoomEffectWaterSprayId;

s32 gRoomEffectOrangeBurstId;

RoomEffectState* gRoomEffectState;

s32 gRoomEffectHaloId;

Task* Gp_State1CTask;

s32 gRoomEffectWaterRippleId;

s32 gRoomEffectSparkBurstId;

s32 gRoomEffectOrangeBurst2Id;

s32 gRoomEffectFlashId;

#include "gameplay/effect_tasks.h"
#include "gameplay/room_effects.h"

extern s32 D_80111B70[20];

extern s32 D_80111BC0[38];

extern s32 D_80111C58[38];

extern s32 D_80111CF0[49];

extern s32 D_80111DB4[33];

static const TaskFuncTable3 D_80097678;

enum { ROOM_EFFECT_NORMAL_SPAWN_LIMIT = 0x81 };

/// Spinning billboard cells: 32-texel origin strides and a 31-texel inclusive span.
enum {
    EFFECT_BILLBOARD_CELL_SHIFT = 5,
    EFFECT_BILLBOARD_UV_SPAN    = (1 << EFFECT_BILLBOARD_CELL_SHIFT) - 1,
};

/// Fixed texture-page fields shared by the untextured effect blend commands.
enum {
    GPU_EFFECT_TEXTURE_DEPTH_4BIT = 0,
    GPU_EFFECT_TEXTURE_PAGE_X     = 640,
    GPU_EFFECT_FIXED_DEPTH_SHIFT  = 4,
};

/// Camera-facing radial geometry: sixteen spokes and Q12 trigonometric values.
enum {
    EFFECT_RADIAL_ANGLE_TURN         = 0x1000,
    EFFECT_RADIAL_ANGLE_STEP         = 0x100,
    EFFECT_RADIAL_TRIG_FRACTION_BITS = 12,
    EFFECT_RADIAL_PERSPECTIVE_SCALE  = 64,
};

/// Builds and queries a world +Y ground segment in an already reserved scratch block.
///
/// `savedCursor` points immediately above `probe`, whose origin X/Y are filled.
/// `sourceZBits` supplies the low 16 source-Z bits; `segmentEnd` and `hitResult`
/// are writable pointer/status locals. Arguments must be side-effect-free local
/// identifiers: pointer and output arguments occur repeatedly. Captures the
/// current view rotation and room grid, changes GTE state and the endpoint,
/// retains the saved-cursor X reload, and leaves the caller's block reserved.
/// Expands to several statements; invoke only as a phase inside a braced block.
#define WORLD_COLLISION_QUERY_GROUND_PROJECTION(savedCursor, probe, sourceZBits, segmentEnd, hitResult) \
    (probe)->endpoint.vx = 0;                                                                           \
    (probe)->endpoint.vy = WORLD_COLLISION_GROUND_PROBE_LENGTH;                                         \
    (probe)->endpoint.vz = 0;                                                                           \
    (probe)->origin.vz   = (sourceZBits);                                                               \
    gte_SetRotMatrix(&gGfxViewCoord.workm);                                                             \
    (segmentEnd) = &(savedCursor)[-1].endpoint;                                                         \
    gte_ldv0(segmentEnd);                                                                               \
    gte_rtv0();                                                                                         \
    gte_stsv(segmentEnd);                                                                               \
    (probe)->endpoint.vx += (savedCursor)[-1].origin.vx;                                                \
    (probe)->endpoint.vy += (probe)->origin.vy;                                                         \
    (probe)->endpoint.vz += (probe)->origin.vz;                                                         \
    (hitResult)           = worldCollisionProbeGridSegment((segmentEnd), &(probe)->origin, (segmentEnd), NULL);

static void _effectDarknessScreenDimTaskE8(Task* task);

static void Gp_InitState1C(Task* arg0);

static void Gp_TickState1C(Task* unused);

static void _worldCoordTickTransientPointLights(void);

static void _worldCoordInitTransientPointLights(void);

void func_800EA420(Task* arg0);

static void _effectStatusScreenTintTaskF(Task* task);

static void _effectExitTask(Task* task);

static void _gpuSetPrimitiveBlendModeFixedDepth(void* primitive, s32 blendMode, s32 depth);

// Retained effect slots without a proven owning room. See the local type audit.
void func_mist_parking_8018345C(Task* task);

/// Unresolved weapon-overlay callback for effect-bank slots 0xBB and 0xBD.
///
/// Both slots allocate a single-coordinate body and pass a live `task`.
/// The code at this imported address depends on the loaded weapon overlay;
/// its owner, behavior and spawn-argument contract are unproven.
extern void func_8011D1E0(Task* task);

/// Task bank 6: effects and loaded-overlay task entry points.
TaskDesc D_8010FC2C[667] = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { NULL } },                                                                // 0x000
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { NULL } },                                                                // 0x001
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { NULL } },                                                                // 0x002
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { NULL } },                                                                // 0x003
    { { { TASK_BODY_NONE, 0x4F } }, func_800EA420, { NULL } },                                                           // 0x004
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldMotelBalconyRoomVisualEffectsFlashTask, { NULL } },                         // 0x005
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightGasStationRoomVisualEffectsFlashTask, { NULL } },                      // 0x006
    { { { TASK_BODY_NONE, 0x70 } }, Gp_EffCtlTask07, { NULL } },                                                         // 0x007
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightGasStationRoomVisualEffectsTwinTrailTask, { NULL } },                  // 0x008
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_gas_station_801830CC, { NULL } },                               // 0x009
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightBackStreetRoomVisualEffectsFlashTask, { NULL } },                      // 0x00A
    { { { TASK_BODY_COORD, 0x70 } }, hypervelocityDischargeConeTask, { NULL } },                                         // 0x00B
    { { { TASK_BODY_COORD, 0x70 } }, func_hypervelocity_8011D830, { NULL } },                                            // 0x00C
    { { { TASK_BODY_COORD, 0x70 } }, hypervelocityShockRingTask, { NULL } },                                             // 0x00D
    { { { TASK_BODY_COORD, 0x70 } }, effectControlTask0E, { NULL } },                                                    // 0x00E
    { { { TASK_BODY_COORD, 0x70 } }, _effectStatusScreenTintTaskF, { NULL } },                                           // 0x00F
    { { { TASK_BODY_COORD, 0x70 } }, func_pyrokinesis_8012EF48, { NULL } },                                              // 0x010
    { { { TASK_BODY_COORD, 0x70 } }, pyrokinesisLaunchConeTask, { NULL } },                                              // 0x011
    { { { TASK_BODY_COORD, 0x70 } }, func_metabolism_8012EF34, { NULL } },                                               // 0x012
    { { { TASK_BODY_COORD, 0x70 } }, metabolismSparkleTask, { NULL } },                                                  // 0x013
    { { { TASK_BODY_COORD, 0x70 } }, func_plasma_8012EF34, { NULL } },                                                   // 0x014
    { { { TASK_BODY_COORD, 0x70 } }, func_healing_8012EF34, { NULL } },                                                  // 0x015
    { { { TASK_BODY_COORD, 0x70 } }, healingRisingSparkTask, { NULL } },                                                 // 0x016
    { { { TASK_BODY_COORD, 0x70 } }, func_healing_8012F5E4, { NULL } },                                                  // 0x017
    { { { TASK_BODY_COORD, 0x70 } }, func_necrosis_8012EF34, { NULL } },                                                 // 0x018
    { { { TASK_BODY_COORD, 0x70 } }, func_necrosis_8012F52C, { NULL } },                                                 // 0x019
    { { { TASK_BODY_COORD, 0x70 } }, necrosisMistPuffTask, { NULL } },                                                   // 0x01A
    { { { TASK_BODY_COORD, 0x70 } }, func_combustion_8012EF34, { NULL } },                                               // 0x01B
    { { { TASK_BODY_COORD, 0x70 } }, func_combustion_8012F2BC, { NULL } },                                               // 0x01C
    { { { TASK_BODY_COORD, 0x70 } }, acropolisHelicopterLandingPadPerimeterLightsTask, { NULL } },                       // 0x01D
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_west_elevator_hall_8017F7D4, { NULL } },                             // 0x01E
    { { { TASK_BODY_COORD, 0x70 } }, acropolisWestElevatorHallRedBeaconTask, { NULL } },                                 // 0x01F
    { { { TASK_BODY_COORD, 0x70 } }, acropolisWestElevatorHallScanlineDistortionTask, { NULL } },                        // 0x020
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_east_elevator_hall_8017F5B4, { NULL } },                             // 0x021
    { { { TASK_BODY_COORD, 0x70 } }, acropolisEastElevatorHallRedBeaconTask, { NULL } },                                 // 0x022
    { { { TASK_BODY_COORD, 0x70 } }, acropolisEastElevatorHallPointTileTask, { NULL } },                                 // 0x023
    { { { TASK_BODY_COORD, 0x70 } }, func_hypervelocity_8011D1E8, { NULL } },                                            // 0x024
    { { { TASK_BODY_COORD, 0x70 } }, acropolisWestElevatorHallLightGlowTask, { NULL } },                                 // 0x025
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_fountain_8017E014, { NULL } },                                       // 0x026
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_observatory_8017E6F8, { NULL } },                                    // 0x027
    { { { TASK_BODY_COORD, 0x70 } }, acropolisObservatoryAmbientGlowTask, { NULL } },                                    // 0x028
    { { { TASK_BODY_COORD, 0x70 } }, m4a1HammerGlowTask, { NULL } },                                                     // 0x029
    { { { TASK_BODY_COORD, 0x70 } }, func_m4a1_pyke_8011D1F8, { NULL } },                                                // 0x02A
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask2B, { NULL } },                                                        // 0x02B
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { NULL } },                                                                // 0x02C
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_square_801823DC, { NULL } },                                         // 0x02D
    { { { TASK_BODY_COORD, 0x70 } }, func_mist_parking_8018345C, { NULL } },                                             // 0x02E
    { { { TASK_BODY_COORD, 0x70 } }, m4a1JavelinGuideBeamTask, { NULL } },                                               // 0x02F
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask30, { NULL } },                                                        // 0x030
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { NULL } },                                                                // 0x031
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask32, { NULL } },                                                     // 0x032
    { { { TASK_BODY_COORD, 0x70 } }, acropolisWestElevatorHallBayLightingTask, { NULL } },                               // 0x033
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask34, { NULL } },                                                     // 0x034
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask35, { NULL } },                                                     // 0x035
    { { { TASK_BODY_TMD, 0x70 } }, effectThrownModelTask, { &D_80111FC8 } },                                             // 0x036
    { { { TASK_BODY_TMD, 0x70 } }, Gp_EffAttachTask37, { &D_8011231C } },                                                // 0x037
    { { { TASK_BODY_COORD, 0x70 } }, neoArkSubstationDrawLightGlowsTask, { NULL } },                                     // 0x038
    { { { TASK_BODY_COORD, 0x70 } }, mistParkingDrawGlowsTask, { NULL } },                                               // 0x039
    { { { TASK_BODY_COORD, 0x70 } }, func_tonfa_baton_8011D1EC, { NULL } },                                              // 0x03A
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask3B, { NULL } },                                                        // 0x03B
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_breezeway_80181264, { NULL } },                                       // 0x03C
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMotelBalconyDebrisTask, { NULL } },                                    // 0x03D
    { { { TASK_BODY_COORD, 0x70 } }, m4a1BayonetTrailTask, { NULL } },                                                   // 0x03E
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask3F, { NULL } },                                                     // 0x03F
    { { { TASK_BODY_COORD, 0x70 } }, p229MuzzleFlashTask, { NULL } },                                                    // 0x040
    { { { TASK_BODY_COORD, 0x70 } }, mp5a5MuzzleFlashTask, { NULL } },                                                   // 0x041
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask42, { NULL } },                                                     // 0x042
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_510900_80131F24, { NULL } },                                             // 0x043
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_510900_801340E8, { NULL } },                                             // 0x044
    { { { TASK_BODY_COORD, 0x70 } }, actor510900FlameSpriteTask45, { NULL } },                                           // 0x045
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask46, { NULL } },                                                     // 0x046
    { { { TASK_BODY_COORD, 0x70 } }, acropolisSquareBeaconGlowTask, { NULL } },                                          // 0x047
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_security_room_801805A4, { NULL } },                                  // 0x048
    { { { TASK_BODY_COORD, 0x70 } }, acropolisSecurityRoomMonitorFeedTask, { NULL } },                                   // 0x049
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_promenade_8017E03C, { NULL } },                                      // 0x04A
    { { { TASK_BODY_COORD, 0x70 } }, acropolisPromenadeGlowStarTask, { NULL } },                                         // 0x04B
    { { { TASK_BODY_COORD, 0x70 } }, actor510900FlameSpriteTask4C, { NULL } },                                           // 0x04C
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_plaza_8018251C, { NULL } },                                          // 0x04D
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_fire_escape_8017FF7C, { NULL } },                                    // 0x04E
    { { { TASK_BODY_COORD, 0x70 } }, acropolisFireEscapeFlareTask, { NULL } },                                           // 0x04F
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_balcony_80180580, { NULL } },                             // 0x050
    { { { TASK_BODY_COORD, 0x70 } }, acropolisForkedRoadLeafFallTask, { NULL } },                                        // 0x051
    { { { TASK_BODY_COORD, 0x70 } }, actor510900FlameSpriteTask52, { NULL } },                                           // 0x052
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask53, { NULL } },                                                     // 0x053
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask54, { NULL } },                                                        // 0x054
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask55, { NULL } },                                                     // 0x055
    { { { TASK_BODY_COORD, 0x70 } }, acropolisPromenadeScreenDripTask, { NULL } },                                       // 0x056
    { { { TASK_BODY_COORD, 0x70 } }, acropolisPromenadeGroundGlowTask, { NULL } },                                       // 0x057
    { { { TASK_BODY_COORD, 0x70 } }, neoArkWoodlandPathWaterRippleTask, { NULL } },                                      // 0x058
    { { { TASK_BODY_COORD, 0x70 } }, actor510900FlameSpriteTask59, { NULL } },                                           // 0x059
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_helicopter_landing_pad_8017FA30, { NULL } },                         // 0x05A
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_helicopter_landing_pad_801802E0, { NULL } },                         // 0x05B
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask5C, { NULL } },                                                        // 0x05C
    { { { TASK_BODY_NONE, 0x70 } }, taskKill, { NULL } },                                                                // 0x05D
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_helicopter_landing_pad_80181064, { NULL } },                         // 0x05E
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_helicopter_landing_pad_80180E40, { NULL } },                         // 0x05F
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_cafeteria_8017E708, { NULL } },                                      // 0x060
    { { { TASK_BODY_COORD, 0x70 } }, acropolisCafeteriaPuffTask, { NULL } },                                             // 0x061
    { { { TASK_BODY_COORD, 0x70 } }, acropolisPromenadeGlowLampTask, { NULL } },                                         // 0x062
    { { { TASK_BODY_NONE, 0x70 } }, taskKill, { NULL } },                                                                // 0x063
    { { { TASK_BODY_TMD, 0x70 } }, acropolisCafeteriaModelWanderTask, { &gAcropolisCafeteriaModel077D8 } },              // 0x064
    { { { TASK_BODY_COORD, 0x70 } }, actor510900DebrisStreakTask, { NULL } },                                            // 0x065
    { { { TASK_BODY_TMD, 0x70 } }, effectThrownModelTask, { &D_80112200 } },                                             // 0x066
    { { { TASK_BODY_TMD, 0x70 } }, effectThrownModelTask, { &D_801120E4 } },                                             // 0x067
    { { { TASK_BODY_TMD, 0x70 } }, effectThrownModelTask, { &D_8011231C } },                                             // 0x068
    { { { TASK_BODY_COORD, 0x70 } }, pyrokinesisFlamePuffTask, { NULL } },                                               // 0x069
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask6A, { NULL } },                                                        // 0x06A
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask6B, { NULL } },                                                        // 0x06B
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask6C, { NULL } },                                                        // 0x06C
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask6D, { NULL } },                                                        // 0x06D
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask6E, { NULL } },                                                        // 0x06E
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask6F, { NULL } },                                                     // 0x06F
    { { { TASK_BODY_COORD, 0x70 } }, func_800F289C, { NULL } },                                                          // 0x070
    { { { TASK_BODY_COORD, 0x70 } }, func_800F4308, { NULL } },                                                          // 0x071
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask72, { NULL } },                                                     // 0x072
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldMotelBalconyRoomVisualEffectsTwinTrailTask, { NULL } },                     // 0x073
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_motel_balcony_80181628, { NULL } },                                   // 0x074
    { { { TASK_BODY_NONE, 0x70 } }, taskKill, { NULL } },                                                                // 0x075
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask76, { NULL } },                                                     // 0x076
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_sanctuary_8017E00C, { NULL } },                                      // 0x077
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_sanctuary_8017E134, { NULL } },                                      // 0x078
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_sanctuary_8017E338, { NULL } },                                      // 0x079
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_sanctuary_8017EC90, { NULL } },                                      // 0x07A
    { { { TASK_BODY_COORD, 0x70 } }, acropolisSecurityRoomFallingQuadTask, { NULL } },                                   // 0x07B
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask7C, { NULL } },                                                     // 0x07C
    { { { TASK_BODY_NONE, 0x70 } }, taskKill, { NULL } },                                                                // 0x07D
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMotelBalconyFlameTask, { NULL } },                                     // 0x07E
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask7F, { NULL } },                                                        // 0x07F
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask80, { NULL } },                                                     // 0x080
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask81, { NULL } },                                                        // 0x081
    { { { TASK_BODY_NONE, 0x70 } }, taskKill, { NULL } },                                                                // 0x082
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_patio_8017E100, { NULL } },                                          // 0x083
    { { { TASK_BODY_COORD, 0x70 } }, acropolisHallwayEffectControlTask84, { NULL } },                                    // 0x084
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_forked_road_8017E298, { NULL } },                                    // 0x085
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_roof_garden_8017DCDC, { NULL } },                                    // 0x086
    { { { TASK_BODY_COORD, 0x70 } }, acropolisPatioFountainJetTask, { NULL } },                                          // 0x087
    { { { TASK_BODY_COORD, 0x70 } }, acropolisFountainSprayTask, { NULL } },                                             // 0x088
    { { { TASK_BODY_COORD, 0x70 } }, acropolisForkedRoadWallLampTask, { NULL } },                                        // 0x089
    { { { TASK_BODY_COORD, 0x70 } }, acropolisRoofGardenLightGlowTask, { NULL } },                                       // 0x08A
    { { { TASK_BODY_COORD, 0x70 } }, acropolisSanctuaryFlameTask, { NULL } },                                            // 0x08B
    { { { TASK_BODY_COORD, 0x70 } }, acropolisFireEscapeFlickerLightTask, { NULL } },                                    // 0x08C
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask8D, { NULL } },                                                     // 0x08D
    { { { TASK_BODY_COORD, 0x70 } }, func_800FF710, { NULL } },                                                          // 0x08E
    { { { TASK_BODY_COORD, 0x70 } }, acropolisPatioFountainMistTask, { NULL } },                                         // 0x08F
    { { { TASK_BODY_COORD, 0x70 } }, acropolisRoofGardenFlareTask, { NULL } },                                           // 0x090
    { { { TASK_BODY_TMD, 0x70 } }, effectThrownModelTask, { &D_801124B8 } },                                             // 0x091
    { { { TASK_BODY_COORD, 0x70 } }, effectLineTask92, { NULL } },                                                       // 0x092
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_balcony_801809CC, { NULL } },                             // 0x093
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_balcony_80181024, { NULL } },                             // 0x094
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMotelBalconyDriftPuffTask, { NULL } },                                 // 0x095
    { { { TASK_BODY_COORD, 0x70 } }, acropolisPlazaLightGlowTask, { NULL } },                                            // 0x096
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightBackStreetRoomVisualEffectsTwinTrailTask, { NULL } },                  // 0x097
    { { { TASK_BODY_COORD, 0x70 } }, acropolisPlazaSirenLightTask, { NULL } },                                           // 0x098
    { { { TASK_BODY_COORD, 0x70 } }, acropolisPlazaLightFlareTask, { NULL } },                                           // 0x099
    { { { TASK_BODY_COORD, 0x70 } }, func_800F91AC, { NULL } },                                                          // 0x09A
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask9B, { NULL } },                                                        // 0x09B
    { { { TASK_BODY_COORD, 0x70 } }, effectPolyTask9C, { NULL } },                                                       // 0x09C
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_cafeteria_8017E89C, { NULL } },                                      // 0x09D
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTask9E, { NULL } },                                                     // 0x09E
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldToiletJetPuffTask, { NULL } },                                              // 0x09F
    { { { TASK_BODY_COORD, 0x70 } }, acropolisSecurityRoomMonitorGlowTask, { NULL } },                                   // 0x0A0
    { { { TASK_BODY_COORD, 0x70 } }, func_800ED42C, { NULL } },                                                          // 0x0A1
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_toilet_8017DCF0, { NULL } },                                          // 0x0A2
    { { { TASK_BODY_COORD, 0x70 } }, effectLineTaskA3, { NULL } },                                                       // 0x0A3
    { { { TASK_BODY_COORD, 0x70 } }, effectTileTaskA4, { NULL } },                                                       // 0x0A4
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTaskA5, { NULL } },                                                        // 0x0A5
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTaskA6, { NULL } },                                                        // 0x0A6
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTaskA7, { NULL } },                                                     // 0x0A7
    { { { TASK_BODY_COORD, 0x70 } }, func_800FAA14, { NULL } },                                                          // 0x0A8
    { { { TASK_BODY_COORD, 0x70 } }, combustionEmberTask, { NULL } },                                                    // 0x0A9
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4ReservoirBurstSpriteTask, { NULL } },                                      // 0x0AA
    { { { TASK_BODY_COORD, 0x70 } }, func_lifedrain_8012EF48, { NULL } },                                                // 0x0AB
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTaskAC, { NULL } },                                                        // 0x0AC
    { { { TASK_BODY_COORD, 0x70 } }, lifedrainRisingSparkTask, { NULL } },                                               // 0x0AD
    { { { TASK_BODY_COORD, 0x70 } }, effectControlTaskAE, { NULL } },                                                    // 0x0AE
    { { { TASK_BODY_COORD, 0x70 } }, func_lifedrain_8012FAF8, { NULL } },                                                // 0x0AF
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_bridge_8017F868, { NULL } },                                         // 0x0B0
    { { { TASK_BODY_COORD, 0x70 } }, acropolisBridgeGlowStarTask, { NULL } },                                            // 0x0B1
    { { { TASK_BODY_COORD, 0x70 } }, acropolisBridgeGroundGlowTask, { NULL } },                                          // 0x0B2
    { { { TASK_BODY_COORD, 0x70 } }, acropolisBridgeGlowLampTask, { NULL } },                                            // 0x0B3
    { { { TASK_BODY_COORD, 0x70 } }, acropolisBridgeFallingStreakTask, { NULL } },                                       // 0x0B4
    { { { TASK_BODY_COORD, 0x70 } }, acropolisBridgeMidDustStreakTask, { NULL } },                                       // 0x0B5
    { { { TASK_BODY_COORD, 0x70 } }, acropolisBridgeLowDustStreakTask, { NULL } },                                       // 0x0B6
    { { { TASK_BODY_COORD, 0x70 } }, acropolisBridgeTallDustStreakTask, { NULL } },                                      // 0x0B7
    { { { TASK_BODY_COORD, 0x70 } }, acropolisBridgeParticleStreakTask, { NULL } },                                      // 0x0B8
    { { { TASK_BODY_COORD, 0x70 } }, acropolisBridgeWaterRippleTask, { NULL } },                                         // 0x0B9
    { { { TASK_BODY_COORD, 0x70 } }, acropolisBridgeEffectSpriteDebrisTask, { NULL } },                                  // 0x0BA
    { { { TASK_BODY_COORD, 0x70 } }, func_8011D1E0, { NULL } },                                                          // 0x0BB
    { { { TASK_BODY_COORD, 0x70 } }, acropolisBridgeDustMoteTask, { NULL } },                                            // 0x0BC
    { { { TASK_BODY_COORD, 0x70 } }, func_8011D1E0, { NULL } },                                                          // 0x0BD
    { { { TASK_BODY_COORD, 0x70 } }, func_inferno_8012EF88, { NULL } },                                                  // 0x0BE
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldUnderpassDrawFlaresTask, { NULL } },                                        // 0x0BF
    { { { TASK_BODY_COORD, 0x70 } }, func_apobiosis_8012EF4C, { NULL } },                                                // 0x0C0
    { { { TASK_BODY_COORD, 0x70 } }, effectPolyTaskC1, { NULL } },                                                       // 0x0C1
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldGasStationCyanGlowTask, { NULL } },                                         // 0x0C2
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_main_street_8017E4B0, { NULL } },                                     // 0x0C3
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldGeneralStoreNoOpEffectTask, { NULL } },                                     // 0x0C4
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldBackStreetConfigureEffectsTask, { NULL } },                                 // 0x0C5
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldSouvenirShopLightPrismsTask, { NULL } },                                    // 0x0C6
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldWarehouseDrawGlowsTask, { NULL } },                                         // 0x0C7
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldDilapidatedHouseLightPrismTask, { NULL } },                                 // 0x0C8
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldMotelRoom1NoOpEffectTask, { NULL } },                                       // 0x0C9
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldMotelRoom2EffectNoopTaskCA, { NULL } },                                     // 0x0CA
    { { { TASK_BODY_COORD, 0x70 } }, func_antibody_8012EF34, { NULL } },                                                 // 0x0CB
    { { { TASK_BODY_COORD, 0x70 } }, func_energyshot_8012EF34, { NULL } },                                               // 0x0CC
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldParkingLotUpdateViewEffectGateTask, { NULL } },                             // 0x0CD
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldToiletConfigureEffectsTask, { NULL } },                                     // 0x0CE
    { { { TASK_BODY_COORD, 0x70 } }, func_energyball_8012EF48, { NULL } },                                               // 0x0CF
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldSaloonGRDrawLightEffectsTask, { NULL } },                                   // 0x0D0
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldGRKitchenDrawLightBeamsTask, { NULL } },                                    // 0x0D1
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldWaterTowerUpdateViewEffectGateTask, { NULL } },                             // 0x0D2
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldWaterTankUpdateViewEffectGateTask, { NULL } },                              // 0x0D3
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_breezeway_8017FF7C, { NULL } },                                       // 0x0D4
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldFactoryDrawGlowsTask, { NULL } },                                           // 0x0D5
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldGarageEffectNoopTaskD6, { NULL } },                                         // 0x0D6
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldDrivewayEnableAmbientEffectsTask, { NULL } },                               // 0x0D7
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldJunkYardEnableViewEffectsTask, { NULL } },                                  // 0x0D8
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldTrailerCoachDrawGlowsTask, { NULL } },                                      // 0x0D9
    { { { TASK_BODY_COORD, 0x70 } }, infernoFlameFanTask, { NULL } },                                                    // 0x0DA
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldMotelBalconyInitRoomEffectsTask, { NULL } },                                // 0x0DB
    { { { TASK_BODY_COORD, 0x70 } }, motelRoom6DayDrawGlow, { NULL } },                                                  // 0x0DC
    { { { TASK_BODY_COORD, 0x70 } }, energyshotRisingBillboardTask, { NULL } },                                          // 0x0DD
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_water_hole_8017E040, { NULL } },                                      // 0x0DE
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldCellarDrawGlowsTask, { NULL } },                                            // 0x0DF
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTaskE0, { NULL } },                                                     // 0x0E0
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTaskE1, { NULL } },                                                     // 0x0E1
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTaskE2, { NULL } },                                                     // 0x0E2
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTaskE3, { NULL } },                                                        // 0x0E3
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_back_street_8017F6DC, { NULL } },                               // 0x0E4
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightJunkYardRoomVisualEffectsFlashTask, { NULL } },                        // 0x0E5
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightJunkYardRoomVisualEffectsTwinTrailTask, { NULL } },                    // 0x0E6
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_junk_yard_8017F914, { NULL } },                                 // 0x0E7
    { { { TASK_BODY_COORD, 0x70 } }, _effectDarknessScreenDimTaskE8, { NULL } },                                         // 0x0E8
    { { { TASK_BODY_COORD, 0x70 } }, mineMesaRoomVisualEffectsFlashTask, { NULL } },                                     // 0x0E9
    { { { TASK_BODY_COORD, 0x70 } }, lifedrainExpandingGlowBandTask, { NULL } },                                         // 0x0EA
    { { { TASK_BODY_COORD, 0x70 } }, mineMesaRoomVisualEffectsTwinTrailTask, { NULL } },                                 // 0x0EB
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_mesa_8018057C, { NULL } },                                                // 0x0EC
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4LowerSewerRoomVisualEffectsFlashTask, { NULL } },                          // 0x0ED
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4LowerSewerRoomVisualEffectsTwinTrailTask, { NULL } },                      // 0x0EE
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_lower_sewer_801811FC, { NULL } },                                   // 0x0EF
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4UpperSewerRoomVisualEffectsFlashTask, { NULL } },                          // 0x0F0
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4UpperSewerRoomVisualEffectsTwinTrailTask, { NULL } },                      // 0x0F1
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_80183A80, { NULL } },                                   // 0x0F2
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTaskF3, { NULL } },                                                        // 0x0F3
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteTaskF4, { NULL } },                                                     // 0x0F4
    { { { TASK_BODY_COORD, 0x70 } }, antibodyMoteTask, { NULL } },                                                       // 0x0F5
    { { { TASK_BODY_COORD, 0x70 } }, pyrokinesisFlameRingTask, { NULL } },                                               // 0x0F6
    { { { TASK_BODY_COORD, 0x70 } }, apobiosisShardTask, { NULL } },                                                     // 0x0F7
    { { { TASK_BODY_COORD, 0x70 } }, func_energyball_8012F180, { NULL } },                                               // 0x0F8
    { { { TASK_BODY_COORD, 0x70 } }, energyballImpactRingTask, { NULL } },                                               // 0x0F9
    { { { TASK_BODY_COORD, 0x70 } }, neoArkForestZoneConfigureEffectsTask, { NULL } },                                   // 0x0FA
    { { { TASK_BODY_COORD, 0x70 } }, neoArkForestZoneLeafFallTask, { NULL } },                                           // 0x0FB
    { { { TASK_BODY_COORD, 0x70 } }, func_pyrokinesis_8012FAC8, { NULL } },                                              // 0x0FC
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldWaterHoleWaterRippleTask, { NULL } },                                       // 0x0FD
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldWaterHoleWaterDriftTaskU16, { NULL } },                                     // 0x0FE
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightWaterHoleWaterRippleTask, { NULL } },                                  // 0x0FF
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_gas_station_80180E9C, { NULL } },                               // 0x100
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_main_street_8017E484, { NULL } },                               // 0x101
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightGeneralStoreDrawLightShaftsTask, { NULL } },                           // 0x102
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightBackStreetDrawGlowsTask, { NULL } },                                   // 0x103
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightSouvenirShopPrismLightTask, { NULL } },                                // 0x104
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightWarehouseDrawGlowsTask, { NULL } },                                    // 0x105
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightDilapidatedHouseDrawLightPrismsTask, { NULL } },                       // 0x106
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMotelRoom1DrawGlowsTask, { NULL } },                                   // 0x107
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMotelRoom2DrawFlaresTask, { NULL } },                                  // 0x108
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMotelRoom3DrawFlaresTask, { NULL } },                                  // 0x109
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMotelRoom4DrawFlaresTask, { NULL } },                                  // 0x10A
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightParkingLotDrawGlowsTask, { NULL } },                                   // 0x10B
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightToiletDrawFlareTask, { NULL } },                                       // 0x10C
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMotelLobbyDrawGlowsTask, { NULL } },                                   // 0x10D
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightSaloonGRDrawGlowsTask, { NULL } },                                     // 0x10E
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightGRKitchenDrawLightShaftsTask, { NULL } },                              // 0x10F
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightWaterTowerDrawGlowsTask, { NULL } },                                   // 0x110
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightWaterTankNoOpEffectTask, { NULL } },                                   // 0x111
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightBreezewayDrawGlowsTask, { NULL } },                                    // 0x112
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightFactoryDrawGlowsTask, { NULL } },                                      // 0x113
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightGarageDrawGlowsTask, { NULL } },                                       // 0x114
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightDrivewayDrawLightShaftsTask, { NULL } },                               // 0x115
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightJunkYardDrawGlowsTask, { NULL } },                                     // 0x116
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightTrailerCoachDrawGlowsTask, { NULL } },                                 // 0x117
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMotelRoom5DrawFlareTask, { NULL } },                                   // 0x118
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_balcony_8017E554, { NULL } },                             // 0x119
    { { { TASK_BODY_COORD, 0x70 } }, motelRoom6NightDrawGlow, { NULL } },                                                // 0x11A
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_loft_8017DB64, { NULL } },                                // 0x11B
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_water_hole_8017E6D0, { NULL } },                                // 0x11C
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightCellarDrawGlowsTask, { NULL } },                                       // 0x11D
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightUnderpassDrawFlaresTask, { NULL } },                                   // 0x11E
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightWaterHoleWaterDriftTaskU16, { NULL } },                                // 0x11F
    { { { TASK_BODY_COORD, 0x70 } }, mineMesaDrawViewFlaresTask, { NULL } },                                             // 0x120
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_cavern_8017E474, { NULL } },                                              // 0x121
    { { { TASK_BODY_COORD, 0x70 } }, mineTunnelEntranceDrawFlaresTask, { NULL } },                                       // 0x122
    { { { TASK_BODY_COORD, 0x70 } }, mineTunnelDrawViewFlaresTask, { NULL } },                                           // 0x123
    { { { TASK_BODY_COORD, 0x70 } }, mineGorgeDrawViewFlaresTask, { NULL } },                                            // 0x124
    { { { TASK_BODY_COORD, 0x70 } }, mineRefugeDrawGlowsTask, { NULL } },                                                // 0x125
    { { { TASK_BODY_COORD, 0x70 } }, mineForkedTunnelDrawViewFlaresTask, { NULL } },                                     // 0x126
    { { { TASK_BODY_COORD, 0x70 } }, mineSecretPassageDrawLightGlowsTask, { NULL } },                                    // 0x127
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ElevatorHallDrawGlowsTask, { NULL } },                                     // 0x128
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1SouthMaintenanceWalkwayDrawGlowsTask, { NULL } },                          // 0x129
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1StoreroomDrawGlowsTask, { NULL } },                                        // 0x12A
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1NorthMaintenanceWalkwayDrawGlowsTask, { NULL } },                          // 0x12B
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ArmoryDrawGlowsTask, { NULL } },                                           // 0x12C
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1SleepingQuartersDrawViewLightsTask, { NULL } },                            // 0x12D
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1MainCorridorDrawViewLightsTask, { NULL } },                                // 0x12E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_sterilization_room_8018188C, { NULL } },                            // 0x12F
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1PodAccessTunnelDrawGlowsTask, { NULL } },                                  // 0x130
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ControlRoomDrawGlowsTask, { NULL } },                                      // 0x131
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1AccessTunnelDrawGlowsTask, { NULL } },                                     // 0x132
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1UndergroundParkingDrawGlowsTask, { NULL } },                               // 0x133
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_golem_freezer_1_8017DA7C, { NULL } },                               // 0x134
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodBottomShadowTask, { NULL } },                                           // 0x135
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1PodServiceGantryInitEffectsTask, { NULL } },                               // 0x136
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1TransferTunnelDrawGlowsTask, { NULL } },                                   // 0x137
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ControlRoomAccessTunnelDrawGlowsTask, { NULL } },                          // 0x138
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2ElevatorEffectNoopTask, { NULL } },                                        // 0x139
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2ElevatorHallDrawGlowsTask, { NULL } },                                     // 0x13A
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2SouthMaintenanceWalkwayDrawGlowsTask, { NULL } },                          // 0x13B
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2OperatingRoomDrawGlowsTask, { NULL } },                                    // 0x13C
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2NorthMaintenanceWalkwayGlowTask, { NULL } },                               // 0x13D
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2LaboratoryGlowTask, { NULL } },                                            // 0x13E
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2BreedingRoomDrawGlowsTask, { NULL } },                                     // 0x13F
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2MainCorridorDrawViewGlowsTask, { NULL } },                                 // 0x140
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2SepticTankGlowTask, { NULL } },                                            // 0x141
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodAccessTunnelDrawLightBeamsTask, { NULL } },                             // 0x142
    { { { TASK_BODY_COORD, 0x70 } }, shelterB3DumpingHoleDrawViewGlowsTask, { NULL } },                                  // 0x143
    { { { TASK_BODY_COORD, 0x70 } }, shelterB3GarbageIncineratorDrawLightsTask, { NULL } },                              // 0x144
    { { { TASK_BODY_COORD, 0x70 } }, shelterB3IncineratorControlRoomDrawViewGlowsTask, { NULL } },                       // 0x145
    { { { TASK_BODY_COORD, 0x70 } }, shelterB3ElevatorHallDrawGlowsTask, { NULL } },                                     // 0x146
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4LowerSewerDrawGlowsTask, { NULL } },                                       // 0x147
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4UpperSewerDrawGlowsTask, { NULL } },                                       // 0x148
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_reservoir_8017FB84, { NULL } },                                     // 0x149
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_water_supply_8017EE54, { NULL } },                                  // 0x14A
    { { { TASK_BODY_COORD, 0x70 } }, shelterR47DrawViewGlowsTask, { NULL } },                                            // 0x14B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_8017E3B8, { NULL } },                                              // 0x14C
    { { { TASK_BODY_COORD, 0x70 } }, shelter1fAirlockDrawViewGlowsTask, { NULL } },                                      // 0x14D
    { { { TASK_BODY_COORD, 0x70 } }, neoArkObservatoryGlowTask, { NULL } },                                              // 0x14E
    { { { TASK_BODY_COORD, 0x70 } }, neoArkEveAccessTunnelDrawViewGlowsTask, { NULL } },                                 // 0x14F
    { { { TASK_BODY_COORD, 0x70 } }, neoArkEveElevatorIdleEffectTask, { NULL } },                                        // 0x150
    { { { TASK_BODY_COORD, 0x70 } }, neoArkNorthPromenadeBindRoomEffectsTask, { NULL } },                                // 0x151
    { { { TASK_BODY_COORD, 0x70 } }, neoArkSubmarineTunnelConfigureEffectsTask, { NULL } },                              // 0x152
    { { { TASK_BODY_COORD, 0x70 } }, neoArkPavilionInstallRoomEffectsTask, { NULL } },                                   // 0x153
    { { { TASK_BODY_COORD, 0x70 } }, neoArkIslandInstallRoomEffectIdsTask, { NULL } },                                   // 0x154
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_garden_8017EA9C, { NULL } },                                           // 0x155
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_power_plant_2_8017D8AC, { NULL } },                                    // 0x156
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_power_plant_1_8017DA18, { NULL } },                                    // 0x157
    { { { TASK_BODY_COORD, 0x70 } }, neoArkSavannaZoneConfigureEffectsTask, { NULL } },                                  // 0x158
    { { { TASK_BODY_COORD, 0x70 } }, neoArkSouthPromenadeRegisterGolemEffectsTask, { NULL } },                           // 0x159
    { { { TASK_BODY_COORD, 0x70 } }, neoArkAltarEffectNoopTask, { NULL } },                                              // 0x15A
    { { { TASK_BODY_COORD, 0x70 } }, neoArkShrineFlareTask, { NULL } },                                                  // 0x15B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_nursery_801800A0, { NULL } },                                       // 0x15C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_growth_room_8017D9D8, { NULL } },                                   // 0x15D
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6CorridorDrawViewGlowsTask, { NULL } },                                     // 0x15E
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6TrainingRoomGlowTask, { NULL } },                                          // 0x15F
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_bridge_8017E954, { NULL } },                                           // 0x160
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_woodland_path_8017EA08, { NULL } },                                    // 0x161
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r49_8017D9D0, { NULL } },                                              // 0x162
    { { { TASK_BODY_COORD, 0x70 } }, shelter1fParkingGarageDrawViewGlowsTask, { NULL } },                                // 0x163
    { { { TASK_BODY_COORD, 0x70 } }, shelter1fVehicularAirlockDrawLightsTask, { NULL } },                                // 0x164
    { { { TASK_BODY_COORD, 0x70 } }, shelter1fBulwarkDrawGlowsTask, { NULL } },                                          // 0x165
    { { { TASK_BODY_COORD, 0x70 } }, shelter1fHeliportNoOpEffectTask, { NULL } },                                        // 0x166
    { { { TASK_BODY_COORD, 0x70 } }, shelter1fGuardroomEffectNoopTask, { NULL } },                                       // 0x167
    { { { TASK_BODY_COORD, 0x70 } }, neoArkR26EffectNoopTask, { NULL } },                                                // 0x168
    { { { TASK_BODY_COORD, 0x70 } }, shelter1fTentDrawViewGlowsTask, { NULL } },                                         // 0x169
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2MainCorridorWaterRippleTask, { NULL } },                                   // 0x16A
    { { { TASK_BODY_COORD, 0x70 } }, waterDriftTaskNoUpdate, { NULL } },                                                 // 0x16B
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2SepticTankWaterRippleTask, { NULL } },                                     // 0x16C
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2SepticTankWaterDriftTask, { NULL } },                                      // 0x16D
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4LowerSewerWaterRippleTask, { NULL } },                                     // 0x16E
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4LowerSewerWaterDriftTaskU16, { NULL } },                                   // 0x16F
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4UpperSewerWaterRippleTask, { NULL } },                                     // 0x170
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4UpperSewerWaterDriftTask, { NULL } },                                      // 0x171
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4ReservoirWaterRippleTask, { NULL } },                                      // 0x172
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4ReservoirWaterDriftTask, { NULL } },                                       // 0x173
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4WaterSupplyWaterRippleTask, { NULL } },                                    // 0x174
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4WaterSupplyWaterDriftTask, { NULL } },                                     // 0x175
    { { { TASK_BODY_COORD, 0x70 } }, neoArkPavilionWaterRippleTaskFixedCoord, { NULL } },                                // 0x176
    { { { TASK_BODY_COORD, 0x70 } }, neoArkPavilionWaterDriftTaskU16FixedCoord, { NULL } },                              // 0x177
    { { { TASK_BODY_COORD, 0x70 } }, neoArkIslandWaterRippleTaskFixedCoord, { NULL } },                                  // 0x178
    { { { TASK_BODY_COORD, 0x70 } }, neoArkIslandWaterDriftTaskU16FixedCoord, { NULL } },                                // 0x179
    { { { TASK_BODY_COORD, 0x70 } }, neoArkBridgeWaterRippleTask, { NULL } },                                            // 0x17A
    { { { TASK_BODY_COORD, 0x70 } }, neoArkBridgeWaterDriftTask, { NULL } },                                             // 0x17B
    { { { TASK_BODY_COORD, 0x70 } }, acropolisRoofGardenLeafFallTask, { NULL } },                                        // 0x17C
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1SterilizationRoomPuffTask, { NULL } },                                     // 0x17D
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightR08DrawGlowsTask, { NULL } },                                          // 0x17E
    { { { TASK_BODY_COORD, 0x70 } }, m4a1PykeFlameTask, { NULL } },                                                      // 0x17F
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_800100_80161F20, { NULL } },                                             // 0x180
    { { { TASK_BODY_COORD, 0x70 } }, actor800100PykeFlameTask, { NULL } },                                               // 0x181
    { { { TASK_BODY_COORD, 0x70 } }, m4a1HammerImpactFlashTask, { NULL } },                                              // 0x182
    { { { TASK_BODY_COORD, 0x70 } }, m4a1JavelinContactFlashTask, { NULL } },                                            // 0x183
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_510900_8013482C, { NULL } },                                             // 0x184
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_510900_801346D4, { NULL } },                                             // 0x185
    { { { TASK_BODY_COORD, 0x70 } }, func_gunblade_8011D1E4, { NULL } },                                                 // 0x186
    { { { TASK_BODY_COORD, 0x70 } }, neoArkWoodlandPathWaterDriftTaskU16, { NULL } },                                    // 0x187
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldDilapidatedHouseTwinTrailTask, { NULL } },                                  // 0x188
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_8017E4C4, { NULL } },                                              // 0x189
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_8017EC18, { NULL } },                                              // 0x18A
    { { { TASK_BODY_COORD, 0x70 } }, shelterR48WaterDriftTaskU16, { NULL } },                                            // 0x18B
    { { { TASK_BODY_COORD, 0x70 } }, shelterR48SpriteDriftTask, { NULL } },                                              // 0x18C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_8017E704, { NULL } },                                              // 0x18D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_8017E9B8, { NULL } },                                              // 0x18E
    { { { TASK_BODY_COORD, 0x70 } }, shelterR48RingWallTask, { NULL } },                                                 // 0x18F
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_801810B0, { NULL } },                                              // 0x190
    { { { TASK_BODY_COORD, 0x70 } }, shelterR48ShockwaveRingsTask, { NULL } },                                           // 0x191
    { { { TASK_BODY_COORD, 0x70 } }, neoArkSubmarineGalleryDrawViewGlowsTask, { NULL } },                                // 0x192
    { { { TASK_BODY_COORD, 0x70 } }, neoArkSubmarineGalleryWaterRippleTask, { NULL } },                                  // 0x193
    { { { TASK_BODY_COORD, 0x70 } }, neoArkSubmarineGalleryWaterDriftTaskU16, { NULL } },                                // 0x194
    { { { TASK_BODY_COORD, 0x70 } }, shelterR48PinkRingFlashTask, { NULL } },                                            // 0x195
    { { { TASK_BODY_COORD, 0x70 } }, shelterB3GarbageIncineratorEffectSpriteDriftTaskAimed, { NULL } },                  // 0x196
    { { { TASK_BODY_COORD, 0x70 } }, pepperSprayEffectTask, { NULL } },                                                  // 0x197
    { { { TASK_BODY_COORD, 0x70 } }, ofudaEffectTask, { NULL } },                                                        // 0x198
    { { { TASK_BODY_COORD, 0x70 } }, shelterB3DumpingHoleEffectSpriteDriftTaskAimed, { NULL } },                         // 0x199
    { { { TASK_BODY_COORD, 0x70 } }, shelterB3DumpingHoleGluttonRainParticleTask, { NULL } },                            // 0x19A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_dumping_hole_80186D4C, { NULL } },                                  // 0x19B
    { { { TASK_BODY_COORD, 0x70 } }, shelterB3GarbageIncineratorEffectSpriteDebrisTask, { NULL } },                      // 0x19C
    { { { TASK_BODY_COORD, 0x70 } }, flareEffectTask, { NULL } },                                                        // 0x19D
    { { { TASK_BODY_COORD, 0x70 } }, flareSparkTask, { NULL } },                                                         // 0x19E
    { { { TASK_BODY_COORD, 0x70 } }, mistShootingGalleryDrawLightGlowsTask, { NULL } },                                  // 0x19F
    { { { TASK_BODY_COORD, 0x70 } }, func_combustion_801308E0, { NULL } },                                               // 0x1A0
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6GrowthRoomMistTask, { NULL } },                                            // 0x1A1
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6GrowthRoomDriftPuffTask, { NULL } },                                       // 0x1A2
    { { { TASK_BODY_TMD, 0x70 } }, func_shelter_b6_nursery_80181314, { &gShelterB6NurseryModel07BAC } },                 // 0x1A3
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6NurseryAnimatedParticleTask, { NULL } },                                   // 0x1A4
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6NurserySparkShowerShardTask, { NULL } },                                   // 0x1A5
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1GolemFreezer1FloorMistTask, { NULL } },                                    // 0x1A6
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6TrainingRoomOrangeBurstTask, { NULL } },                                   // 0x1A7
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6TrainingRoomSummonRingTask, { NULL } },                                    // 0x1A8
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_training_room_80180DB4, { NULL } },                                 // 0x1A9
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6TrainingRoomRingBandTask, { NULL } },                                      // 0x1AA
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6TrainingRoomEnergyArcTask, { NULL } },                                     // 0x1AB
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6TrainingRoomHitFlashTask, { NULL } },                                      // 0x1AC
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6TrainingRoomSinkingSpriteTask, { NULL } },                                 // 0x1AD
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_training_room_801826E0, { NULL } },                                 // 0x1AE
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_training_room_80182804, { NULL } },                                 // 0x1AF
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMotelLoftFallingShardTask, { NULL } },                                 // 0x1B0
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldMainStreetPuffTask, { NULL } },                                             // 0x1B1
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMainStreetPuffTask, { NULL } },                                        // 0x1B2
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodBottomEffectSpriteDriftTask, { NULL } },                                // 0x1B3
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1PodServiceGantrySpriteDriftTask, { NULL } },                               // 0x1B4
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodAccessTunnelEffectSpriteDriftTask, { NULL } },                          // 0x1B5
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldR08LampGlowTask, { NULL } },                                                // 0x1B6
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldR08SpriteDriftTask, { NULL } },                                             // 0x1B7
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldR08SpriteDriftTask, { NULL } },                                             // 0x1B8
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodBottomShockRingTask, { NULL } },                                        // 0x1B9
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodBottomArcFlashTask, { NULL } },                                         // 0x1BA
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodBottomEnergyRingTask, { NULL } },                                       // 0x1BB
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodBottomChargeBurstTask, { NULL } },                                      // 0x1BC
    { { { TASK_BODY_COORD, 0x70 } }, mistShootingGalleryTracerTask, { NULL } },                                          // 0x1BD
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1PodServiceGantryWaterDriftTaskU16, { NULL } },                             // 0x1BE
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodBottomEffectSpriteRiseTask, { NULL } },                                 // 0x1BF
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodBottomLightBeamTask, { NULL } },                                        // 0x1C0
    { { { TASK_BODY_COORD, 0x70 } }, neoArkWoodlandPathLeafFallTask, { NULL } },                                         // 0x1C1
    { { { TASK_BODY_COORD, 0x70 } }, shelter1fBulwarkRoomVisualEffectsFlashTask, { NULL } },                             // 0x1C2
    { { { TASK_BODY_COORD, 0x70 } }, shelter1fBulwarkRoomVisualEffectsTwinTrailTask, { NULL } },                         // 0x1C3
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_bulwark_8017F6D8, { NULL } },                                       // 0x1C4
    { { { TASK_BODY_COORD, 0x70 } }, neoArkPyramidConfigureEffectsTask, { NULL } },                                      // 0x1C5
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1PodServiceGantryEffectSpriteRiseTask, { NULL } },                          // 0x1C6
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightR08RoomVisualEffectsFlashTask, { NULL } },                             // 0x1C7
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ElevatorHallRoomVisualEffectsFlashTask, { NULL } },                        // 0x1C8
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlashTask, { NULL } },             // 0x1C9
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1StoreroomRoomVisualEffectsFlashTask, { NULL } },                           // 0x1CA
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsFlashTask, { NULL } },             // 0x1CB
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1MainCorridorRoomVisualEffectsFlashTask, { NULL } },                        // 0x1CC
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1PodAccessTunnelRoomVisualEffectsFlashTask, { NULL } },                     // 0x1CD
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1TransferTunnelRoomVisualEffectsFlashTask, { NULL } },                      // 0x1CE
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlashTask, { NULL } },             // 0x1CF
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2ElevatorHallRoomVisualEffectsFlashTask, { NULL } },                        // 0x1D0
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlashTask, { NULL } },             // 0x1D1
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2NorthMaintenanceWalkwayRoomVisualEffectsFlashTask, { NULL } },             // 0x1D2
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2MainCorridorRoomVisualEffectsFlashTask, { NULL } },                        // 0x1D3
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2SepticTankRoomVisualEffectsFlashTask, { NULL } },                          // 0x1D4
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodAccessTunnelRoomVisualEffectsFlashTask, { NULL } },                     // 0x1D5
    { { { TASK_BODY_COORD, 0x70 } }, shelter1fParkingGarageRoomVisualEffectsFlashTask, { NULL } },                       // 0x1D6
    { { { TASK_BODY_COORD, 0x70 } }, shelter1fVehicularAirlockRoomVisualEffectsFlashTask, { NULL } },                    // 0x1D7
    { { { TASK_BODY_COORD, 0x70 } }, neoArkNorthPromenadeRoomVisualEffectsFlashTask, { NULL } },                         // 0x1D8
    { { { TASK_BODY_COORD, 0x70 } }, neoArkForestZoneRoomVisualEffectsFlashTask, { NULL } },                             // 0x1D9
    { { { TASK_BODY_COORD, 0x70 } }, neoArkPavilionRoomVisualEffectsFlashTask, { NULL } },                               // 0x1DA
    { { { TASK_BODY_COORD, 0x70 } }, neoArkIslandRoomVisualEffectsFlashTask, { NULL } },                                 // 0x1DB
    { { { TASK_BODY_COORD, 0x70 } }, neoArkPowerPlant2RoomVisualEffectsFlashTask, { NULL } },                            // 0x1DC
    { { { TASK_BODY_COORD, 0x70 } }, neoArkSavannaZoneRoomVisualEffectsFlashTask, { NULL } },                            // 0x1DD
    { { { TASK_BODY_COORD, 0x70 } }, neoArkSouthPromenadeRoomVisualEffectsFlashTask, { NULL } },                         // 0x1DE
    { { { TASK_BODY_COORD, 0x70 } }, neoArkShrineRoomVisualEffectsFlashTask, { NULL } },                                 // 0x1DF
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6NurseryRoomVisualEffectsFlashTask, { NULL } },                             // 0x1E0
    { { { TASK_BODY_COORD, 0x70 } }, neoArkBridgeRoomVisualEffectsFlashTask, { NULL } },                                 // 0x1E1
    { { { TASK_BODY_COORD, 0x70 } }, neoArkPyramidRoomVisualEffectsFlashTask, { NULL } },                                // 0x1E2
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightR08RoomVisualEffectsTwinTrailTask, { NULL } },                         // 0x1E3
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ElevatorHallRoomVisualEffectsTwinTrailTask, { NULL } },                    // 0x1E4
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1SouthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask, { NULL } },         // 0x1E5
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1StoreroomRoomVisualEffectsTwinTrailTask, { NULL } },                       // 0x1E6
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask, { NULL } },         // 0x1E7
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1MainCorridorRoomVisualEffectsTwinTrailTask, { NULL } },                    // 0x1E8
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1PodAccessTunnelRoomVisualEffectsTwinTrailTask, { NULL } },                 // 0x1E9
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1TransferTunnelRoomVisualEffectsTwinTrailTask, { NULL } },                  // 0x1EA
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ControlRoomAccessTunnelRoomVisualEffectsTwinTrailTask, { NULL } },         // 0x1EB
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2ElevatorHallRoomVisualEffectsTwinTrailTask, { NULL } },                    // 0x1EC
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2SouthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask, { NULL } },         // 0x1ED
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2NorthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask, { NULL } },         // 0x1EE
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2MainCorridorRoomVisualEffectsTwinTrailTask, { NULL } },                    // 0x1EF
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2SepticTankRoomVisualEffectsTwinTrailTask, { NULL } },                      // 0x1F0
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodAccessTunnelRoomVisualEffectsTwinTrailTask, { NULL } },                 // 0x1F1
    { { { TASK_BODY_COORD, 0x70 } }, shelter1fParkingGarageRoomVisualEffectsTwinTrailTask, { NULL } },                   // 0x1F2
    { { { TASK_BODY_COORD, 0x70 } }, shelter1fVehicularAirlockRoomVisualEffectsTwinTrailTask, { NULL } },                // 0x1F3
    { { { TASK_BODY_COORD, 0x70 } }, neoArkNorthPromenadeRoomVisualEffectsTwinTrailTask, { NULL } },                     // 0x1F4
    { { { TASK_BODY_COORD, 0x70 } }, neoArkForestZoneRoomVisualEffectsTwinTrailTask, { NULL } },                         // 0x1F5
    { { { TASK_BODY_COORD, 0x70 } }, neoArkPavilionRoomVisualEffectsTwinTrailTask, { NULL } },                           // 0x1F6
    { { { TASK_BODY_COORD, 0x70 } }, neoArkIslandRoomVisualEffectsTwinTrailTask, { NULL } },                             // 0x1F7
    { { { TASK_BODY_COORD, 0x70 } }, neoArkPowerPlant2RoomVisualEffectsTwinTrailTask, { NULL } },                        // 0x1F8
    { { { TASK_BODY_COORD, 0x70 } }, neoArkSavannaZoneRoomVisualEffectsTwinTrailTask, { NULL } },                        // 0x1F9
    { { { TASK_BODY_COORD, 0x70 } }, neoArkSouthPromenadeRoomVisualEffectsTwinTrailTask, { NULL } },                     // 0x1FA
    { { { TASK_BODY_COORD, 0x70 } }, neoArkShrineRoomVisualEffectsTwinTrailTask, { NULL } },                             // 0x1FB
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6NurseryRoomVisualEffectsTwinTrailTask, { NULL } },                         // 0x1FC
    { { { TASK_BODY_COORD, 0x70 } }, neoArkBridgeRoomVisualEffectsTwinTrailTask, { NULL } },                             // 0x1FD
    { { { TASK_BODY_COORD, 0x70 } }, neoArkPyramidRoomVisualEffectsTwinTrailTask, { NULL } },                            // 0x1FE
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_r08_8017F8FC, { NULL } },                                       // 0x1FF
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_elevator_hall_80182064, { NULL } },                                 // 0x200
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_south_maintenance_walkway_8017FAAC, { NULL } },                     // 0x201
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_80182118, { NULL } },                                     // 0x202
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_80182228, { NULL } },                     // 0x203
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_main_corridor_80182444, { NULL } },                                 // 0x204
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_pod_access_tunnel_80180484, { NULL } },                             // 0x205
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_transfer_tunnel_80181C78, { NULL } },                               // 0x206
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_access_tunnel_8017F624, { NULL } },                    // 0x207
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_elevator_hall_80182B48, { NULL } },                                 // 0x208
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_south_maintenance_walkway_8017FCE8, { NULL } },                     // 0x209
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_north_maintenance_walkway_80182F00, { NULL } },                     // 0x20A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_main_corridor_80181C98, { NULL } },                                 // 0x20B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_septic_tank_80181F2C, { NULL } },                                   // 0x20C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_access_tunnel_80182F78, { NULL } },                             // 0x20D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_parking_garage_8017FF58, { NULL } },                                // 0x20E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_vehicular_airlock_80180008, { NULL } },                             // 0x20F
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_north_promenade_80181120, { NULL } },                                  // 0x210
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_forest_zone_8017F76C, { NULL } },                                      // 0x211
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pavilion_80180FFC, { NULL } },                                         // 0x212
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_island_80180EE8, { NULL } },                                           // 0x213
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_power_plant_2_8017F140, { NULL } },                                    // 0x214
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_savanna_zone_8017ED58, { NULL } },                                     // 0x215
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_south_promenade_8017EA6C, { NULL } },                                  // 0x216
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_shrine_801811EC, { NULL } },                                           // 0x217
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_nursery_80184074, { NULL } },                                       // 0x218
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_bridge_801812D0, { NULL } },                                           // 0x219
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pyramid_8017EF9C, { NULL } },                                          // 0x21A
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask, { NULL } },       // 0x21B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_south_maintenance_walkway_801806F4, { NULL } },                     // 0x21C
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask, { NULL } }, // 0x21D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_80182D60, { NULL } },                                     // 0x21E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_80182E70, { NULL } },                     // 0x21F
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_sleeping_quarters_8017E6DC, { NULL } },                             // 0x220
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_south_maintenance_walkway_80180930, { NULL } },                     // 0x221
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_operating_room_8017ECFC, { NULL } },                                // 0x222
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_elevator_hall_80180E18, { NULL } },                                 // 0x223
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_801846C8, { NULL } },                                   // 0x224
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_reservoir_80182B1C, { NULL } },                                     // 0x225
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_water_supply_801809DC, { NULL } },                                  // 0x226
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pavilion_80181C44, { NULL } },                                         // 0x227
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_garden_8017F790, { NULL } },                                           // 0x228
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1StoreroomRoomVisualEffectsFlyingSparkTask, { NULL } },                     // 0x229
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask, { NULL } },       // 0x22A
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1SleepingQuartersRoomVisualEffectsFlyingSparkTask, { NULL } },              // 0x22B
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask, { NULL } },       // 0x22C
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2OperatingRoomRoomVisualEffectsFlyingSparkTask, { NULL } },                 // 0x22D
    { { { TASK_BODY_COORD, 0x70 } }, shelterB3ElevatorHallRoomVisualEffectsFlyingSparkTask, { NULL } },                  // 0x22E
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4UpperSewerRoomVisualEffectsFlyingSparkTask, { NULL } },                    // 0x22F
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4ReservoirRoomVisualEffectsFlyingSparkTask, { NULL } },                     // 0x230
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4WaterSupplyRoomVisualEffectsFlyingSparkTask, { NULL } },                   // 0x231
    { { { TASK_BODY_COORD, 0x70 } }, neoArkPavilionRoomVisualEffectsFlyingSparkTask, { NULL } },                         // 0x232
    { { { TASK_BODY_COORD, 0x70 } }, neoArkGardenRoomVisualEffectsFlyingSparkTask, { NULL } },                           // 0x233
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1StoreroomRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },               // 0x234
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask, { NULL } }, // 0x235
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1SleepingQuartersRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },        // 0x236
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask, { NULL } }, // 0x237
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2OperatingRoomRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },           // 0x238
    { { { TASK_BODY_COORD, 0x70 } }, shelterB3ElevatorHallRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },            // 0x239
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4UpperSewerRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },              // 0x23A
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4ReservoirRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },               // 0x23B
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4WaterSupplyRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },             // 0x23C
    { { { TASK_BODY_COORD, 0x70 } }, neoArkPavilionRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },                   // 0x23D
    { { { TASK_BODY_COORD, 0x70 } }, neoArkGardenRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },                     // 0x23E
    { { { TASK_BODY_COORD, 0x70 } }, mineCavernRoomVisualEffectsHaloOrangeBurstTask, { NULL } },                         // 0x23F
    { { { TASK_BODY_COORD, 0x70 } }, mineSecretPassageRoomVisualEffectsHaloOrangeBurstTask, { NULL } },                  // 0x240
    { { { TASK_BODY_COORD, 0x70 } }, mineSecretPassageRoomVisualEffectsHaloTask, { NULL } },                             // 0x241
    { { { TASK_BODY_COORD, 0x70 } }, mineSecretPassageRoomVisualEffectsMoteTask, { NULL } },                             // 0x242
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_secret_passage_80180D58, { NULL } },                                      // 0x243
    { { { TASK_BODY_COORD, 0x70 } }, mineCavernRoomVisualEffectsMoteTask, { NULL } },                                    // 0x244
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ElevatorHallRoomVisualEffectsMoteTask, { NULL } },                         // 0x245
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1StoreroomRoomVisualEffectsMoteTask, { NULL } },                            // 0x246
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsMoteTask, { NULL } },              // 0x247
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1MainCorridorRoomVisualEffectsMoteTask, { NULL } },                         // 0x248
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1TransferTunnelRoomVisualEffectsMoteTask, { NULL } },                       // 0x249
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2ElevatorHallRoomVisualEffectsMoteTask, { NULL } },                         // 0x24A
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2NorthMaintenanceWalkwayRoomVisualEffectsMoteTask, { NULL } },              // 0x24B
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodAccessTunnelRoomVisualEffectsMoteTask, { NULL } },                      // 0x24C
    { { { TASK_BODY_COORD, 0x70 } }, shelterB3ElevatorHallRoomVisualEffectsMoteTask, { NULL } },                         // 0x24D
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4UpperSewerRoomVisualEffectsMoteTask, { NULL } },                           // 0x24E
    { { { TASK_BODY_COORD, 0x70 } }, neoArkNorthPromenadeRoomVisualEffectsMoteTask, { NULL } },                          // 0x24F
    { { { TASK_BODY_COORD, 0x70 } }, mineCavernRoomVisualEffectsHaloTask, { NULL } },                                    // 0x250
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ElevatorHallRoomVisualEffectsHaloTask, { NULL } },                         // 0x251
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1StoreroomRoomVisualEffectsHaloTask, { NULL } },                            // 0x252
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsHaloTask, { NULL } },              // 0x253
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1MainCorridorRoomVisualEffectsHaloTask, { NULL } },                         // 0x254
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1TransferTunnelRoomVisualEffectsHaloTask, { NULL } },                       // 0x255
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2ElevatorHallRoomVisualEffectsHaloTask, { NULL } },                         // 0x256
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2NorthMaintenanceWalkwayRoomVisualEffectsHaloTask, { NULL } },              // 0x257
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodAccessTunnelRoomVisualEffectsHaloTask, { NULL } },                      // 0x258
    { { { TASK_BODY_COORD, 0x70 } }, shelterB3ElevatorHallRoomVisualEffectsHaloTask, { NULL } },                         // 0x259
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4UpperSewerRoomVisualEffectsHaloTask, { NULL } },                           // 0x25A
    { { { TASK_BODY_COORD, 0x70 } }, neoArkNorthPromenadeRoomVisualEffectsHaloTask, { NULL } },                          // 0x25B
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ElevatorHallRoomVisualEffectsHaloOrangeBurstTask, { NULL } },              // 0x25C
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1StoreroomRoomVisualEffectsHaloOrangeBurstTask, { NULL } },                 // 0x25D
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsHaloOrangeBurstTask, { NULL } },   // 0x25E
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1MainCorridorRoomVisualEffectsHaloOrangeBurstTask, { NULL } },              // 0x25F
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1TransferTunnelRoomVisualEffectsHaloOrangeBurstTask, { NULL } },            // 0x260
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2ElevatorHallRoomVisualEffectsHaloOrangeBurstTask, { NULL } },              // 0x261
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2NorthMaintenanceWalkwayRoomVisualEffectsHaloOrangeBurstTask, { NULL } },   // 0x262
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2PodAccessTunnelRoomVisualEffectsHaloOrangeBurstTask, { NULL } },           // 0x263
    { { { TASK_BODY_COORD, 0x70 } }, shelterB3ElevatorHallRoomVisualEffectsHaloOrangeBurstTask, { NULL } },              // 0x264
    { { { TASK_BODY_COORD, 0x70 } }, shelterB4UpperSewerRoomVisualEffectsHaloOrangeBurstTask, { NULL } },                // 0x265
    { { { TASK_BODY_COORD, 0x70 } }, neoArkNorthPromenadeRoomVisualEffectsHaloOrangeBurstTask, { NULL } },               // 0x266
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_cavern_80181730, { NULL } },                                              // 0x267
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_elevator_hall_80180BE4, { NULL } },                                 // 0x268
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_80180C98, { NULL } },                                     // 0x269
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_80180DA8, { NULL } },                     // 0x26A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_main_corridor_80180FC4, { NULL } },                                 // 0x26B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_transfer_tunnel_801807F8, { NULL } },                               // 0x26C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_elevator_hall_801816C8, { NULL } },                                 // 0x26D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_north_maintenance_walkway_80181A80, { NULL } },                     // 0x26E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_access_tunnel_80181AF8, { NULL } },                             // 0x26F
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_elevator_hall_80180CE4, { NULL } },                                 // 0x270
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_80182600, { NULL } },                                   // 0x271
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_north_promenade_8017FCA0, { NULL } },                                  // 0x272
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_dilapidated_house_80182744, { NULL } },                               // 0x273
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldDilapidatedHouseFlameConeTask, { NULL } },                                  // 0x274
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldDilapidatedHouseFlameRingTask, { NULL } },                                  // 0x275
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_8017FF80, { NULL } },                                  // 0x276
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ControlRoomRoomVisualEffectsFlyingSparkTask, { NULL } },                   // 0x277
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ControlRoomRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },             // 0x278
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_access_tunnel_8018026C, { NULL } },                    // 0x279
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlyingSparkTask, { NULL } },       // 0x27A
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlyingOrangeBurstTask, { NULL } }, // 0x27B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_breeding_room_8017E774, { NULL } },                                 // 0x27C
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2BreedingRoomRoomVisualEffectsFlyingSparkTask, { NULL } },                  // 0x27D
    { { { TASK_BODY_COORD, 0x70 } }, shelterB2BreedingRoomRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },            // 0x27E
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_submarine_tunnel_8017F4DC, { NULL } },                                 // 0x27F
    { { { TASK_BODY_COORD, 0x70 } }, neoArkSubmarineTunnelRoomVisualEffectsFlyingSparkTask, { NULL } },                  // 0x280
    { { { TASK_BODY_COORD, 0x70 } }, neoArkSubmarineTunnelRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },            // 0x281
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldMotelBalconyRoomVisualEffectsMoteTask, { NULL } },                          // 0x282
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldMotelBalconyRoomVisualEffectsHaloTask, { NULL } },                          // 0x283
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldMotelBalconyRoomVisualEffectsHaloOrangeBurstTask, { NULL } },               // 0x284
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_motel_balcony_801801A8, { NULL } },                                   // 0x285
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMainStreetRoomVisualEffectsMoteTask, { NULL } },                       // 0x286
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMainStreetRoomVisualEffectsHaloTask, { NULL } },                       // 0x287
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldNightMainStreetRoomVisualEffectsHaloOrangeBurstTask, { NULL } },            // 0x288
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_main_street_80181F58, { NULL } },                               // 0x289
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_toilet_8017E69C, { NULL } },                                          // 0x28A
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldToiletFlyingSparkTask, { NULL } },                                          // 0x28B
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldToiletFlyingOrangeBurstTask, { NULL } },                                    // 0x28C
    { { { TASK_BODY_COORD, 0x70 } }, acropolisCafeteriaRoomVisualEffectsFlashTask, { NULL } },                           // 0x28D
    { { { TASK_BODY_COORD, 0x70 } }, acropolisCafeteriaRoomVisualEffectsTwinTrailTask, { NULL } },                       // 0x28E
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_cafeteria_80180C94, { NULL } },                                      // 0x28F
    { { { TASK_BODY_COORD, 0x70 } }, acropolisForkedRoadRoomVisualEffectsFlashTask, { NULL } },                          // 0x290
    { { { TASK_BODY_COORD, 0x70 } }, acropolisForkedRoadRoomVisualEffectsTwinTrailTask, { NULL } },                      // 0x291
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_forked_road_801802CC, { NULL } },                                    // 0x292
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldMainStreetRoomVisualEffectsFlashTask, { NULL } },                           // 0x293
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldMainStreetRoomVisualEffectsTwinTrailTask, { NULL } },                       // 0x294
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_main_street_80180234, { NULL } },                                     // 0x295
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldBackStreetRoomVisualEffectsFlashTask, { NULL } },                           // 0x296
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldBackStreetRoomVisualEffectsTwinTrailTask, { NULL } },                       // 0x297
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_back_street_8017ED1C, { NULL } },                                     // 0x298
    { { { TASK_BODY_COORD, 0x70 } }, shelterB6CorridorPlayerHitGlowTask, { NULL } },                                     // 0x299
    { { { TASK_BODY_COORD, 0x70 } }, func_gunblade_8011DAA4, { NULL } },                                                 // 0x29A
};
s32 D_80111B70[20] = {
    45,
    33,
    131,
    96,
    77,
    72,
    132,
    38,
    133,
    39,
    74,
    119,
    134,
    176,
    78,
    29,
    30,
    0,
    57,
    415,
};
s32 D_80111BC0[38] = {
    194,
    195,
    196,
    0,
    197,
    198,
    199,
    438,
    200,
    0,
    201,
    202,
    0,
    0,
    205,
    206,
    0,
    208,
    209,
    210,
    211,
    212,
    213,
    214,
    215,
    216,
    217,
    0,
    219,
    220,
    0,
    222,
    0,
    223,
    0,
    0,
    0,
    191,
};
s32 D_80111C58[38] = {
    256,
    257,
    258,
    0,
    259,
    260,
    261,
    382,
    262,
    0,
    263,
    264,
    265,
    266,
    267,
    268,
    269,
    270,
    271,
    272,
    273,
    274,
    275,
    276,
    277,
    278,
    279,
    280,
    281,
    282,
    283,
    284,
    0,
    285,
    0,
    0,
    0,
    286,
};
s32 D_80111CF0[49] = {
    288,
    289,
    290,
    291,
    292,
    293,
    294,
    295,
    296,
    297,
    298,
    299,
    300,
    301,
    302,
    303,
    304,
    305,
    306,
    307,
    308,
    309,
    310,
    311,
    312,
    313,
    314,
    315,
    316,
    317,
    318,
    319,
    320,
    321,
    322,
    0,
    0,
    0,
    323,
    324,
    325,
    326,
    327,
    328,
    329,
    330,
    331,
    332,
    354,
};
s32 D_80111DB4[33] = {
    355,
    356,
    357,
    358,
    333,
    359,
    334,
    335,
    336,
    337,
    250,
    338,
    339,
    340,
    341,
    342,
    343,
    344,
    345,
    346,
    347,
    348,
    349,
    350,
    351,
    360,
    352,
    361,
    353,
    402,
    0,
    453,
    56,
};

static const TaskFuncTable3 D_80097678 = { {
    Gp_InitState1C,
    Gp_TickState1C,
    taskKill,
} };

static void Gp_InitState1C(Task* arg0)
{
    RoomEffectState* effectState;
    s32              val;

    val         = 0;
    effectState = memCalloc(sizeof(*effectState), val);
    if (effectState == NULL) {
        taskKill(arg0);
        return;
    }

    // Publish the allocation for the controller's lifetime.
    Gp_State1CTask                  = arg0;
    gRoomEffectState                = effectState;
    arg0->work                      = effectState;
    effectState->effectCount        = 0;
    effectState->rumbleCount        = 0;
    effectState->effectControl      = ROOM_EFFECT_CONTROL_RUNNING;
    effectState->groundTraceEnabled = true;
    effectState->groundShadowShade  = ROOM_EFFECT_GROUND_SHADOW_UNMODULATED;
    Gp_SpawnEff(EFFECT_PLAYER_GROUND_SHADOW, 0, 0, 0);

    gRoomEffectFlashId                 = 0;
    gRoomEffectTwinTrailId             = 0;
    gRoomEffectSparkBurstId            = 0;
    gRoomEffectFlyingSparkId           = 0;
    gRoomEffectGlowDiscId              = 0;
    gRoomEffectOrangeBurst2Id          = 0;
    gRoomEffectMoteId                  = 0;
    gRoomEffectHaloId                  = 0;
    gRoomEffectOrangeBurstId           = 0;
    gRoomEffectSparkEmitterId          = 0;
    gRoomEffectWaterRippleId           = 0;
    effectState->roomEffectMode        = ROOM_EFFECT_VIEW_DISABLED;
    effectState->lastAnimationSoundCue = 0;
    effectState->peEffectControl       = ROOM_EFFECT_CONTROL_RUNNING;
    effectState->screenFxFlags         = 0;
    effectState->peFxFlags             = 0;
    effectState->burstRequest          = false;
    effectState->battleState           = 0;
    effectState->peFadeMask            = 0;
    effectState->pendingCancelFlags    = 0;
    gRoomEffectWaterSprayId            = 0;
    gEnergyBallInFlightCount           = 0;
    arg0->state++;
    _worldCoordInitTransientPointLights();

    switch (gGameSession->location.loc.stage) {
        case GAME_STAGE_ACROPOLIS:
            val = D_80111B70[gGameSession->location.loc.area - 1];
            break;
        case GAME_STAGE_DRYFIELD:
            val = D_80111BC0[gGameSession->location.loc.area - 1];
            break;
        case GAME_STAGE_DRYFIELD_NIGHT:
            val = D_80111C58[gGameSession->location.loc.area - 1];
            break;
        case GAME_STAGE_MINE_SHELTER:
            val = D_80111CF0[gGameSession->location.loc.area - 1];
            break;
        case GAME_STAGE_SHELTER_NEO_ARK:
            val = D_80111DB4[gGameSession->location.loc.area - 1];
            break;
    }

    if (val != 0) {
        Gp_SpawnEff(val | 0x60000, 0, 0, 0);
    }
    Task_Spawn(6, 0x80000007, 0, 0);
}

static void Gp_TickState1C(Task* unused)
{
    RoomEffectState*  effectState;
    SceneCombatState* combat;
    AttachmentState*  attachment;
    s16               previousBattleState;

    if (gRoomEffectState->effectCount <= 0) {
        gRoomEffectState->effectCount = 0;
    }
    if (gRoomEffectState->rumbleCount <= 0) {
        gRoomEffectState->rumbleCount = 0;
    }
    previousBattleState = gRoomEffectState->battleState;
    if ((previousBattleState == ROOM_EFFECT_BATTLE_ENGAGED) && (gSceneCombatState.signals.bytes.battlePhase != previousBattleState)) {
        sndEvtRequestScriptStop(SOUND_COMMON(0x0D) | SOUND_SCRIPT_STOP_ALL_INSTANCES, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        gRoomEffectState->rumbleCount = 0;
    }
    // Publish cancellation for one update alongside the scene actor mode.
    effectState                     = gRoomEffectState;
    combat                          = &gSceneCombatState;
    effectState->battleState        = combat->signals.bytes.battlePhase;
    effectState->effectControl      = combat->actorControl | (effectState->pendingCancelFlags & ROOM_EFFECT_CANCEL_ALL);
    effectState->peEffectControl    = combat->actorControl | (effectState->pendingCancelFlags & (ROOM_EFFECT_CANCEL_PE | ROOM_EFFECT_CANCEL_ALL));
    effectState->pendingCancelFlags = 0;
    if (!(effectState->effectControl & ROOM_EFFECT_CONTROL_PAUSED)) {
        _worldCoordTickTransientPointLights();
    }
    if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        attachment                  = &Gp_StateC08;
        attachment->antibodyTicks   = 0;
        attachment->antibodyCombo   = 0;
        attachment->energyShotTicks = 0;
        attachment->energyShotCombo = 0;
        attachment->metabolismTicks = 0;
        attachment->metabolismCombo = 0;
        attachment->mindWard        = 0;
        attachment->bodyWard        = 0;
        Gp_TriggerPeState(1, PLAYER_STATUS_BERSERKER);
    }
}

s32 worldCollisionProjectGroundCoord(const GfxCoord* sourceCoord, GfxCoord* hitCoord)
{
    _WorldCollisionGroundProbeScratch* scratchEnd;
    _WorldCollisionGroundProbeScratch* scratch;
    SVECTOR*                           endpoint;
    MATRIX*                            viewMatrix;
    s32                                hitResult;
    u16                                originZBits;

    scratchEnd                                              = SCRATCH_STACK_CURSOR(_WorldCollisionGroundProbeScratch);
    scratch                                                 = scratchEnd - 1;
    scratchEnd[-1].origin.vx                                = (u16)sourceCoord->workm.t[0];
    scratch->origin.vy                                      = (u16)sourceCoord->workm.t[1];
    originZBits                                             = (u16)sourceCoord->workm.t[2];
    SCRATCH_STACK_CURSOR(_WorldCollisionGroundProbeScratch) = scratch;
    WORLD_COLLISION_QUERY_GROUND_PROJECTION(scratchEnd, scratch, originZBits, endpoint, hitResult);
    if (hitResult == 1) {
        viewMatrix           = &gGfxViewCoord.workm;
        hitCoord->workm.t[0] = scratch->endpoint.vx;
        hitCoord->workm.t[1] = scratch->endpoint.vy;
        hitCoord->workm.t[2] = scratch->endpoint.vz;
        gfxMakeRelativeTransform(viewMatrix, &hitCoord->workm, &hitCoord->coord);
        hitCoord->parent       = PARENT_OF(viewMatrix, GfxCoord, workm);
        hitCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(hitCoord);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGroundProbeScratch);
    return hitResult;
}

s32 worldCollisionProjectGroundPoint(const VECTOR3* sourcePoint, VECTOR3* hitPoint)
{
    _WorldCollisionGroundProbeScratch* scratchEnd;
    _WorldCollisionGroundProbeScratch* scratch;
    SVECTOR*                           endpoint;
    s32                                hitResult;
    u16                                originZBits;

    scratchEnd                                              = SCRATCH_STACK_CURSOR(_WorldCollisionGroundProbeScratch);
    scratch                                                 = scratchEnd - 1;
    scratchEnd[-1].origin.vx                                = (u16)sourcePoint->vx;
    scratch->origin.vy                                      = (u16)sourcePoint->vy;
    originZBits                                             = (u16)sourcePoint->vz;
    SCRATCH_STACK_CURSOR(_WorldCollisionGroundProbeScratch) = scratch;
    WORLD_COLLISION_QUERY_GROUND_PROJECTION(scratchEnd, scratch, originZBits, endpoint, hitResult);
    if (hitResult == 1) {
        hitPoint->vx = scratch->endpoint.vx;
        hitPoint->vy = scratch->endpoint.vy;
        hitPoint->vz = scratch->endpoint.vz;
        hitResult    = scratch->endpoint.vy - scratch->origin.vy;
        if (hitResult == 0) {
            hitResult = 1;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGroundProbeScratch);
    return hitResult;
}

s32 effectGetGroundShadowShade(s16 halfSize, s16 baseShade, s16 viewYDisplacement)
{
    s32 shade;

    shade = 0;
    if (viewYDisplacement != 0) {
        shade = (baseShade * (halfSize << 1)) / viewYDisplacement;
        if (shade >= ROOM_EFFECT_GROUND_SHADOW_MAX_SHADE + 1) {
            shade = ROOM_EFFECT_GROUND_SHADOW_MAX_SHADE;
        } else if (shade == 0) {
            shade = ROOM_EFFECT_GROUND_SHADOW_DISABLED;
        }
    }
    return shade;
}

void roomEffectRecordAnimationSoundCue(s32 cueIndex)
{
    gRoomEffectState->lastAnimationSoundCue = cueIndex + 1;
}

/// Ages every nonzero transient point-light lifetime by one gameplay frame.
///
/// The room-effect controller calls this only when effect updates are unpaused.
/// Zero disables a slot; expiry retains its placement and light data for reuse.
static void _worldCoordTickTransientPointLights(void)
{
    s32                            slotIndex;
    WorldCoordTransientPointLight* lightSlot;

    // Expire contributions in place, retaining their light records for reuse.
    lightSlot = gWorldCoordTransientPointLights;
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(gWorldCoordTransientPointLights); slotIndex++) {
        if (lightSlot->framesLeft != WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
            lightSlot->framesLeft--;
        }
        lightSlot++;
    }
}

/// Disables all transient point lights and borrows the persistent view as their parent.
///
/// Retains each light's other transform, intensity and falloff fields. Writers
/// must initialize them and dirty the transform before activating a slot.
static void _worldCoordInitTransientPointLights(void)
{
    s32                            slotIndex;
    WorldCoordTransientPointLight* lightSlot;

    lightSlot = gWorldCoordTransientPointLights;
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(gWorldCoordTransientPointLights); slotIndex++) {
        lightSlot->light.head.transform.coord.parent = &gGfxViewCoord;
        lightSlot->framesLeft                        = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
        lightSlot++;
    }
}

void func_800EA420(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80097678;
    sp.funcs[arg0->state](arg0);
}

EffectWork* Gp_SpawnEff(s32 arg0, GfxCoord* arg1, TaskSpawnArg arg2, SVECTOR* arg3)
{
    Task*       task;
    EffectWork* mem;
    s32         bank;

    bank = (arg0 >> 16) & 0x7FFF;
    if ((arg0 >= 0) && (gRoomEffectState->effectCount >= ROOM_EFFECT_NORMAL_SPAWN_LIMIT)) {
        return NULL;
    }
    arg0 &= 0xFFFF;
    if (arg0 == 0) {
        return NULL;
    }
    task = Task_Spawn(bank, arg0, arg2, 0);
    if (task == NULL) {
        return NULL;
    }
    mem = memCalloc(sizeof(EffectWork), false);
    if (mem == NULL) {
        taskKill(task);
        return NULL;
    }
    gRoomEffectState->effectCount++;

    if (arg1 != NULL) {
        GfxCoord* coord;
        SVECTOR   vec;

        coord = task->extra.tmd->coords;
        memset(&vec, 0, sizeof(vec));
        mem->field_C = arg3;
        if (arg3 == NULL) {
            arg3 = &vec;
        }
        mem->pos.vx = arg3->vx;
        mem->pos.vy = arg3->vy;
        mem->pos.vz = arg3->vz;
        if (arg1->parent == &gGfxViewCoord) {
            coord->coord = arg1->coord;
            gte_SetRotMatrix(&arg1->coord);
            gte_SetTransMatrix(&arg1->coord);
            gte_ldv0(arg3);
            gte_rtv0tr();
            gte_stlvnl(coord->coord.t);
        } else {
            actorRenderComposeCoord(arg1);
            coord->workm = arg1->workm;
            gte_SetRotMatrix(&arg1->workm);
            gte_SetTransMatrix(&arg1->workm);
            gte_ldv0(arg3);
            gte_rtv0tr();
            gte_stlvnl(coord->workm.t);
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &coord->coord);
        }
        coord->parent       = &gGfxViewCoord;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        mem->parent = arg1;
    } else {
        GfxCoord* coord;
        SVECTOR   vec;

        coord = task->extra.tmd->coords;
        memset(&vec, 0, sizeof(vec));
        mem->field_C = arg3;
        if (arg3 == NULL) {
            arg3 = &vec;
        }
        mem->pos.vx = arg3->vx;
        mem->pos.vy = arg3->vy;
        mem->pos.vz = arg3->vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(arg3);
        gte_rtv0tr();
        gte_stlvnl(coord->coord.t);
        coord->parent       = &gGfxViewCoord;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        mem->parent = &gGfxViewCoord;
    }

    task->spawnArg2.pointer = mem;
    task->exitCallback      = _effectExitTask;
    mem->task               = task;
    mem->index              = 0;
    mem->age                = 0;
    mem->scale              = 0;
    mem->angle              = 0;
    mem->period             = 0;
    mem->step               = 0;
    mem->field_4            = 0;
    mem->move.vx            = 0;
    mem->move.vy            = 0;
    mem->move.vz            = 0;
    return mem;
}

/// Enables semitransparency and prepends the blend draw mode for an untextured effect primitive.
///
/// `primitive` must be an initialized, writable packet already linked in the
/// current ordering table at the same `sortingDepth`. This is the depth before
/// `otDepthShift` scaling, not a tag index: the unsigned scaled depth is
/// quantized and wrapped to one of the 1024 depth tags. The current table must
/// contain that tag. The low two bits of `blendMode` select `GPU_BLEND_*`.
/// Prepending the command makes its draw mode apply before the primitive.
///
/// Requires word-aligned space for `sizeof(DR_TPAGE)` at `gGpuPrimCursor` and
/// advances it without a capacity check. Both packets must remain live until
/// GPU drawing completes. The draw mode persists until replaced: dithering
/// enabled, drawing into the displayed area disabled, and a fixed 4-bit
/// texture page at VRAM (640, 0), unused by the untextured primitive.
static inline void _gpuSetEffectPrimitiveBlendMode(void* primitive, s32 blendMode, s32 sortingDepth)
{
    enum {
        GPU_EFFECT_DRAW_TO_DISPLAY_DISABLED   = 0,
        GPU_EFFECT_DITHER_ENABLED             = 1,
        GPU_EFFECT_TEXTURE_PAGE_Y             = 0,
        GPU_EFFECT_DEPTH_TO_BYTE_OFFSET_SHIFT = 2,
    };
    DR_TPAGE* blendCommand;

    setSemiTrans(primitive, true);
    blendCommand   = gGpuPrimCursor;
    gGpuPrimCursor = blendCommand + 1;
    setDrawTPage(blendCommand, GPU_EFFECT_DRAW_TO_DISPLAY_DISABLED, GPU_EFFECT_DITHER_ENABLED,
                 getTPage(GPU_EFFECT_TEXTURE_DEPTH_4BIT, blendMode, GPU_EFFECT_TEXTURE_PAGE_X, GPU_EFFECT_TEXTURE_PAGE_Y));
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                ((((u32)sortingDepth << gDisplayState.otDepthShift) >> GPU_EFFECT_DEPTH_TO_BYTE_OFFSET_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            blendCommand);
}

void effectDrawScreenTint(const u8* rgb, s32 blendMode)
{
    enum {
        EFFECT_SCREEN_HALF_WIDTH      = 160,
        EFFECT_SCREEN_HALF_HEIGHT     = 120,
        EFFECT_SCREEN_SORTING_DEPTH   = 16,
        EFFECT_SCREEN_BLEND_MODE_MASK = 3,
    };
    POLY_F4* quad;
    s32      leftX;
    s32      rightX;
    s32      topY;
    s32      bottomY;

    blendMode &= EFFECT_SCREEN_BLEND_MODE_MASK;
    leftX      = -EFFECT_SCREEN_HALF_WIDTH;
    rightX     = EFFECT_SCREEN_HALF_WIDTH;
    topY       = -EFFECT_SCREEN_HALF_HEIGHT;
    bottomY    = EFFECT_SCREEN_HALF_HEIGHT;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    setRGB0(quad, rgb[0], rgb[1], rgb[2]);
    quad->x0 = leftX;
    quad->y0 = topY - gDisplayState.vramYOffset;
    quad->x1 = rightX;
    quad->y1 = topY - gDisplayState.vramYOffset;
    quad->x2 = leftX;
    quad->y2 = bottomY - gDisplayState.vramYOffset;
    quad->x3 = rightX;
    quad->y3 = bottomY - gDisplayState.vramYOffset;
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)EFFECT_SCREEN_SORTING_DEPTH << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);

    _gpuSetEffectPrimitiveBlendMode(quad, blendMode, EFFECT_SCREEN_SORTING_DEPTH);
}

void effectDrawOuterGlowBand(const GfxCoord* centreCoord, s32 innerRadius, s32 width, const u8* rgb)
{
    EffectShapeScratch* scratch;
    POLY_G4*            quad;
    s32                 angle;
    s32                 sortingDepth;

    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    scratch->worldPoint.vx = centreCoord->workm.t[0];
    scratch->worldPoint.vy = centreCoord->workm.t[1];
    scratch->worldPoint.vz = centreCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth++;
        scratch->extent.ring.inner = ((s16)innerRadius * EFFECT_RADIAL_PERSPECTIVE_SCALE) / scratch->depth;
        scratch->extent.ring.outer = (((s16)innerRadius + (s16)width) * EFFECT_RADIAL_PERSPECTIVE_SCALE) / scratch->depth;
        // Fade from a black inner edge to the coloured outer edge around a full turn.
        for (angle = 0; angle < EFFECT_RADIAL_ANGLE_TURN; angle += EFFECT_RADIAL_ANGLE_STEP) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, rgb[0], rgb[1], rgb[2]);
            setRGB3(quad, rgb[0], rgb[1], rgb[2]);
            quad->x0 = scratch->screenX + ((scratch->extent.ring.inner * rsin(angle)) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            quad->y0 = scratch->screenY + ((scratch->extent.ring.inner * rcos(angle)) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            quad->x1 = scratch->screenX + ((scratch->extent.ring.inner * rsin(angle + EFFECT_RADIAL_ANGLE_STEP)) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            quad->y1 = scratch->screenY + ((scratch->extent.ring.inner * rcos(angle + EFFECT_RADIAL_ANGLE_STEP)) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            quad->x2 = scratch->screenX + ((scratch->extent.ring.outer * rsin(angle)) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            quad->y2 = scratch->screenY + ((scratch->extent.ring.outer * rcos(angle)) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            quad->x3 = scratch->screenX + ((scratch->extent.ring.outer * rsin(angle + EFFECT_RADIAL_ANGLE_STEP)) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            quad->y3 = scratch->screenY + ((scratch->extent.ring.outer * rcos(angle + EFFECT_RADIAL_ANGLE_STEP)) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            sortingDepth = scratch->depth;
            _gpuSetEffectPrimitiveBlendMode(quad, GPU_BLEND_ADD, sortingDepth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void effectDrawGouraudDisc(const GfxCoord* centreCoord, s32 radius, const u8* rgb)
{
    EffectCentreScratch* scratch;
    POLY_G4*             quad;
    s32                  angle;
    s32                  sortingDepth;

    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    scratch->worldPoint.vx = centreCoord->workm.t[0];
    scratch->worldPoint.vy = centreCoord->workm.t[1];
    scratch->worldPoint.vz = centreCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth++;
        scratch->screenExtent = ((s16)radius * EFFECT_RADIAL_PERSPECTIVE_SCALE) / scratch->depth;
        // Each quad covers two fan triangles, fading from the centre to a black rim.
        for (angle = 0; angle < EFFECT_RADIAL_ANGLE_TURN; angle += (2 * EFFECT_RADIAL_ANGLE_STEP)) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, rgb[0], rgb[1], rgb[2]);
            setRGB3(quad, 0, 0, 0);
            quad->x0 = scratch->screenX + ((scratch->screenExtent * rsin(angle)) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            quad->y0 = scratch->screenY + ((scratch->screenExtent * rcos(angle)) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            quad->x1 = scratch->screenX + ((scratch->screenExtent * rsin(angle + EFFECT_RADIAL_ANGLE_STEP)) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            quad->y1 = scratch->screenY + ((scratch->screenExtent * rcos(angle + EFFECT_RADIAL_ANGLE_STEP)) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            quad->x2 = scratch->screenX;
            quad->y2 = scratch->screenY;
            quad->x3 = scratch->screenX + ((scratch->screenExtent * rsin(angle + (2 * EFFECT_RADIAL_ANGLE_STEP))) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            quad->y3 = scratch->screenY + ((scratch->screenExtent * rcos(angle + (2 * EFFECT_RADIAL_ANGLE_STEP))) >> EFFECT_RADIAL_TRIG_FRACTION_BITS);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            sortingDepth = scratch->depth;
            _gpuSetEffectPrimitiveBlendMode(quad, GPU_BLEND_ADD, sortingDepth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Writes a billboard corner's pixel displacement, with X rightward and Y upward.
///
/// Borrows a live `scratch` block with positive `depth`; only `extent.corner`
/// is replaced and no pointer is retained. `sizeFactor * 31 / depth` gives the
/// signed half-diagonal in pixels, truncated toward zero before Q12 rotation.
/// The signed 32-bit products must fit; the arithmetic right shift rounds
/// negative rotated products down. No depth or overflow check occurs here.
///
/// `cornerAngle` uses 4096 units per turn. For a positive half-diagonal, zero
/// points upward and a quarter turn points rightward. The drawer subtracts Y
/// from the screen centre, negates both offsets for the opposite corner, and
/// repeats the calculation a quarter turn later for the other pair.
static inline void _effectComputeBillboardCornerOffset(EffectShapeScratch* scratch, s16 sizeFactor, s32 cornerAngle)
{
    s32 halfDiagonalPixels;
    s32 trigSample;

    trigSample               = rsin(cornerAngle);
    halfDiagonalPixels       = (sizeFactor * EFFECT_BILLBOARD_UV_SPAN) / scratch->depth;
    scratch->extent.corner.x = (halfDiagonalPixels * trigSample) >> EFFECT_RADIAL_TRIG_FRACTION_BITS;
    trigSample               = rcos(cornerAngle);
    halfDiagonalPixels       = (sizeFactor * EFFECT_BILLBOARD_UV_SPAN) / scratch->depth;
    scratch->extent.corner.y = (halfDiagonalPixels * trigSample) >> EFFECT_RADIAL_TRIG_FRACTION_BITS;
}

void effectDrawSpinningBillboard(const GfxCoord* coord, u16 frame, s16 size, u16 packedAnglePalette)
{
    enum {
        EFFECT_BILLBOARD_TOP_V         = 24,
        EFFECT_BILLBOARD_BOTTOM_V      = EFFECT_BILLBOARD_TOP_V + EFFECT_BILLBOARD_UV_SPAN,
        EFFECT_BILLBOARD_PALETTE_SHIFT = 12,
        EFFECT_BILLBOARD_ANGLE_MASK    = 0xFFF,
        EFFECT_BILLBOARD_QUARTER_TURN  = 0x400,
        EFFECT_BILLBOARD_CLUT_Y        = 267,
    };

    EffectShapeScratch* scratch;
    POLY_FT4*           quad;
    u16                 paletteIndex;
    s32                 leftU;
    s32                 rightU;
    s32                 nextCornerAngle;

    // Project only the composed translation; billboard rotation is in screen space.
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    paletteIndex        = packedAnglePalette >> EFFECT_BILLBOARD_PALETTE_SHIFT;
    packedAnglePalette &= EFFECT_BILLBOARD_ANGLE_MASK;
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, true);
        setShadeTex(quad, true);
        quad->tpage = getTPage(GPU_EFFECT_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, GPU_EFFECT_TEXTURE_PAGE_X, 0);
        setClut(quad, Gp_QuadClutX[paletteIndex], EFFECT_BILLBOARD_CLUT_Y);
        leftU  = frame << EFFECT_BILLBOARD_CELL_SHIFT;
        rightU = leftU + EFFECT_BILLBOARD_UV_SPAN;
        setUV4(quad, leftU, EFFECT_BILLBOARD_TOP_V, rightU, EFFECT_BILLBOARD_TOP_V, leftU, EFFECT_BILLBOARD_BOTTOM_V, rightU, EFFECT_BILLBOARD_BOTTOM_V);
        // Rotate the perspective-scaled half-diagonal into two opposite corner pairs.
        _effectComputeBillboardCornerOffset(scratch, size, packedAnglePalette);
        quad->x0        = scratch->screenX + (u16)scratch->extent.corner.x;
        quad->x3        = scratch->screenX - (u16)scratch->extent.corner.x;
        quad->y0        = scratch->screenY - (u16)scratch->extent.corner.y;
        quad->y3        = scratch->screenY + (u16)scratch->extent.corner.y;
        nextCornerAngle = packedAnglePalette + EFFECT_BILLBOARD_QUARTER_TURN;
        _effectComputeBillboardCornerOffset(scratch, size, nextCornerAngle);
        quad->x1 = scratch->screenX + (u16)scratch->extent.corner.x;
        quad->x2 = scratch->screenX - (u16)scratch->extent.corner.x;
        quad->y1 = scratch->screenY - (u16)scratch->extent.corner.y;
        quad->y2 = scratch->screenY + (u16)scratch->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Sets the screen-space corners of a square effect billboard.
///
/// `scratch` supplies the raw projected `screenX`/`screenY` encodings and
/// `screenExtent`, a signed half-size in integer pixels already scaled by the
/// caller. For a nonnegative extent, corners 0/1 form the top row and 2/3 the
/// bottom row; corners 0/2 form the left column and 1/3 the right column.
/// Screen X increases rightward and screen Y downward.
///
/// Edge sums and differences must fit signed 32-bit arithmetic. Packet stores
/// retain the low 16 bits as signed coordinates, without clipping or clamping.
/// Borrows a live read-only scratch block and a writable `POLY_FT4`; only the
/// three input fields need initialization. Writes all eight XY halfwords and
/// retains neither pointer.
static inline void _effectSetBillboardScreenBounds(POLY_FT4* quad, const EffectCentreScratch* scratch)
{
    quad->x0 = quad->x2 = scratch->screenX - scratch->screenExtent;
    quad->x1 = quad->x3 = scratch->screenX + scratch->screenExtent;
    quad->y0 = quad->y1 = scratch->screenY - scratch->screenExtent;
    quad->y2 = quad->y3 = scratch->screenY + scratch->screenExtent;
}

void effectDrawModulatedBillboard(const GfxCoord* coord, u16 frame, u16 packedSizeBank, u16 packedBrightnessPalette)
{
    enum {
        EFFECT_MODULATED_BILLBOARD_FRAME_COUNT     = 4,
        EFFECT_MODULATED_BILLBOARD_CELL_SIZE       = 24,
        EFFECT_MODULATED_BILLBOARD_BANK_U_STRIDE   = EFFECT_MODULATED_BILLBOARD_FRAME_COUNT * EFFECT_MODULATED_BILLBOARD_CELL_SIZE,
        EFFECT_MODULATED_BILLBOARD_UV_SPAN         = EFFECT_MODULATED_BILLBOARD_CELL_SIZE - 1,
        EFFECT_MODULATED_BILLBOARD_SELECTOR_SHIFT  = 12,
        EFFECT_MODULATED_BILLBOARD_SIZE_MASK       = 0xFFF,
        EFFECT_MODULATED_BILLBOARD_BRIGHTNESS_MASK = 0xFF,
        EFFECT_MODULATED_BILLBOARD_CLUT_Y          = 267,
    };

    EffectCentreScratch* scratch;
    POLY_FT4*            quad;
    u16                  textureBank;
    u16                  paletteIndex;
    s32                  leftU;
    s32                  rightU;

    // Project only the composed translation; the quad stays aligned to the screen axes.
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    // Extract selectors before masking the packed sizing and modulation values.
    textureBank              = packedSizeBank >> EFFECT_MODULATED_BILLBOARD_SELECTOR_SHIFT;
    packedSizeBank          &= EFFECT_MODULATED_BILLBOARD_SIZE_MASK;
    paletteIndex             = packedBrightnessPalette >> EFFECT_MODULATED_BILLBOARD_SELECTOR_SHIFT;
    packedBrightnessPalette &= EFFECT_MODULATED_BILLBOARD_BRIGHTNESS_MASK;
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, true);
        quad->tpage = getTPage(GPU_EFFECT_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, GPU_EFFECT_TEXTURE_PAGE_X, 0);
        setRGB0(quad, packedBrightnessPalette, packedBrightnessPalette, packedBrightnessPalette);
        setClut(quad, D_80111EB4[paletteIndex], EFFECT_MODULATED_BILLBOARD_CLUT_Y);
        leftU  = textureBank * EFFECT_MODULATED_BILLBOARD_BANK_U_STRIDE + (frame & (EFFECT_MODULATED_BILLBOARD_FRAME_COUNT - 1)) * EFFECT_MODULATED_BILLBOARD_CELL_SIZE;
        rightU = leftU + EFFECT_MODULATED_BILLBOARD_UV_SPAN;
        setUV4(quad, leftU, 0, rightU, 0, leftU, EFFECT_MODULATED_BILLBOARD_UV_SPAN, rightU, EFFECT_MODULATED_BILLBOARD_UV_SPAN);
        scratch->screenExtent = (packedSizeBank * EFFECT_MODULATED_BILLBOARD_UV_SPAN) / scratch->depth;
        _effectSetBillboardScreenBounds(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Projects a glow band's segment into the four screen corners of its quad.
///
/// Borrows a live, word-aligned `scratch` block with both rings initialized to
/// signed 16-bit world positions. `segmentIndex` must be in
/// 0..`EFFECT_BAND_SEGMENT_COUNT`-1; the next index wraps to zero at the end.
/// The GTE world-to-screen matrices must already be set. Only `sxy0`..`sxy3`
/// and `projectionFlags` are replaced; no pointer is retained.
///
/// Corners 0/1 come from the current/next top-ring vertices, and corners 2/3
/// from the current/next bottom-ring vertices. The retained FLAG tests only
/// the final three-vertex projection. The final corner's SZ3 remains in the
/// GTE for the caller's depth read; `scratch->otz` is unchanged.
static inline void _effectProjectGlowBandSegment(EffectBandScratch* scratch, s32 segmentIndex)
{
    s32 nextSegmentIndex;

    gte_ldv0(&scratch->topRing[segmentIndex]);
    gte_rtps();
    // Save the first corner before projecting the other three advances the FIFO.
    gte_stsxy(&scratch->sxy0);
    nextSegmentIndex = (segmentIndex + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);
    gte_ldv3(&scratch->topRing[nextSegmentIndex], &scratch->bottomRing[segmentIndex], &scratch->bottomRing[nextSegmentIndex]);
    gte_rtpt();
    gte_stsxy3(&scratch->sxy1, &scratch->sxy2, &scratch->sxy3);
    gte_stflg(&scratch->projectionFlags);
}

void effectDrawRaisedGlowBand(const GfxCoord* coord, s16 innerRadius, const u8* rgb)
{
    enum { EFFECT_RAISED_GLOW_BAND_SPAN = 256 };

    EffectBandScratch* scratch;
    SVECTOR*           outerVertex;
    POLY_G4*           quad;
    s32                segmentIndex;
    s32                angle;
    s32                sortingDepth;
    s16                outerRadius;

    outerRadius = innerRadius + EFFECT_RAISED_GLOW_BAND_SPAN;
    scratch     = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Build both rings in local coordinates, then rotate and translate into world space.
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        angle                             = segmentIndex * EFFECT_RADIAL_ANGLE_STEP;
        scratch->topRing[segmentIndex].vx = (rsin(angle) * innerRadius) >> EFFECT_RADIAL_TRIG_FRACTION_BITS;
        scratch->topRing[segmentIndex].vy = (rcos(angle) * innerRadius) >> EFFECT_RADIAL_TRIG_FRACTION_BITS;
        scratch->topRing[segmentIndex].vz = EFFECT_RAISED_GLOW_BAND_SPAN;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->topRing[segmentIndex]);
        scratch->topRing[segmentIndex].vx = (u16)scratch->topRing[segmentIndex].vx + (u16)coord->workm.t[0];
        scratch->topRing[segmentIndex].vy = (u16)scratch->topRing[segmentIndex].vy + (u16)coord->workm.t[1];
        scratch->topRing[segmentIndex].vz = (u16)scratch->topRing[segmentIndex].vz + (u16)coord->workm.t[2];
        // Address the outer vertex through the complete scratch block's byte view.
        outerVertex     = (SVECTOR*)((u8*)scratch + segmentIndex * sizeof(SVECTOR) + sizeof(scratch->topRing));
        outerVertex->vx = (rsin(angle) * outerRadius) >> EFFECT_RADIAL_TRIG_FRACTION_BITS;
        outerVertex->vy = (rcos(angle) * outerRadius) >> EFFECT_RADIAL_TRIG_FRACTION_BITS;
        outerVertex->vz = 0;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->bottomRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->bottomRing[segmentIndex]);
        outerVertex->vx = (u16)outerVertex->vx + (u16)coord->workm.t[0];
        outerVertex->vy = (u16)outerVertex->vy + (u16)coord->workm.t[1];
        outerVertex->vz = (u16)outerVertex->vz + (u16)coord->workm.t[2];
    }
    // Project each segment independently; only accepted segments consume GPU packets.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        _effectProjectGlowBandSegment(scratch, segmentIndex);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->otz);
            scratch->otz++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, rgb[0], rgb[1], rgb[2]);
            setRGB1(quad, rgb[0], rgb[1], rgb[2]);
            setRGB2(quad, 0, 0, 0);
            setRGB3(quad, 0, 0, 0);
            quad->x0 = scratch->sxy0.vx;
            quad->y0 = scratch->sxy0.vy;
            quad->x1 = scratch->sxy1.vx;
            quad->y1 = scratch->sxy1.vy;
            quad->x2 = scratch->sxy2.vx;
            quad->y2 = scratch->sxy2.vy;
            quad->x3 = scratch->sxy3.vx;
            quad->y3 = scratch->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            sortingDepth = scratch->otz;
            _gpuSetEffectPrimitiveBlendMode(quad, GPU_BLEND_ADD, sortingDepth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

void effectDrawInnerGlowBand(const GfxCoord* coord, s16 innerRadius, s32 width, const u8* rgb)
{
    EffectBandScratch* scratch;
    SVECTOR*           outerVertex;
    POLY_G4*           quad;
    s32                segmentIndex;
    s32                angle;
    s32                sortingDepth;
    s16                outerRadius;

    outerRadius = innerRadius + width;
    scratch     = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Build both rings in local coordinates, then rotate and translate into world space.
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        angle                             = segmentIndex * EFFECT_RADIAL_ANGLE_STEP;
        scratch->topRing[segmentIndex].vx = (rsin(angle) * innerRadius) >> EFFECT_RADIAL_TRIG_FRACTION_BITS;
        scratch->topRing[segmentIndex].vy = 0;
        scratch->topRing[segmentIndex].vz = (rcos(angle) * innerRadius) >> EFFECT_RADIAL_TRIG_FRACTION_BITS;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->topRing[segmentIndex]);
        scratch->topRing[segmentIndex].vx   += coord->workm.t[0];
        scratch->topRing[segmentIndex].vy   += coord->workm.t[1];
        scratch->topRing[segmentIndex].vz   += coord->workm.t[2];
        scratch->bottomRing[segmentIndex].vx = (rsin(angle) * outerRadius) >> EFFECT_RADIAL_TRIG_FRACTION_BITS;
        // Address the outer vertex through the complete scratch block's byte view.
        outerVertex     = (SVECTOR*)((u8*)scratch + segmentIndex * sizeof(SVECTOR) + sizeof(scratch->topRing));
        outerVertex->vy = 0;
        outerVertex->vz = (rcos(angle) * outerRadius) >> EFFECT_RADIAL_TRIG_FRACTION_BITS;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->bottomRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->bottomRing[segmentIndex]);
        scratch->bottomRing[segmentIndex].vx += coord->workm.t[0];
        outerVertex->vy                      += coord->workm.t[1];
        outerVertex->vz                      += coord->workm.t[2];
    }
    // Project each segment independently; only accepted segments consume GPU packets.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        _effectProjectGlowBandSegment(scratch, segmentIndex);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->otz);
            scratch->otz++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, rgb[0], rgb[1], rgb[2]);
            setRGB1(quad, rgb[0], rgb[1], rgb[2]);
            setRGB2(quad, 0, 0, 0);
            setRGB3(quad, 0, 0, 0);
            quad->x0 = scratch->sxy0.vx;
            quad->y0 = scratch->sxy0.vy;
            quad->x1 = scratch->sxy1.vx;
            quad->y1 = scratch->sxy1.vy;
            quad->x2 = scratch->sxy2.vx;
            quad->y2 = scratch->sxy2.vy;
            quad->x3 = scratch->sxy3.vx;
            quad->y3 = scratch->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            sortingDepth = scratch->otz;
            _gpuSetEffectPrimitiveBlendMode(quad, GPU_BLEND_ADD, sortingDepth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

/// Releases one counted effect's work and performs default task teardown.
///
/// `task` must be live and counted in the live `effectState` controller. Call
/// once per effect. `effectWork` must be NULL or the original pointer to its
/// owned primary-heap allocation, separate from `Task::work`. Release nested
/// resources and unlink external nodes before calling.
///
/// The count decreases even for NULL, and work is freed before child exit
/// handlers run. Calls `taskKill` directly, bypassing this task's replacement
/// exit callback; its task/body lifetime rules apply. The spawn-argument
/// pointer is left unchanged after release.
static inline void _effectReleaseCountedTask(RoomEffectState* effectState, void* effectWork, Task* task)
{
    effectState->effectCount--;
    memFree(effectWork);
    taskKill(task);
}

/// Flickers a subtractive screen tint while Darkness is active, then fades it out.
///
/// Bank-6 slot 0xE8 owns a zero-initialized, counted `EffectWork` in
/// `spawnArg2.pointer`. `scale` is brightness, `angle` its target and `period`
/// the wrapping target-change count. Targets alternate between 16 and a
/// randomly selected 32 or 48, moving eight brightness units per task tick.
/// Clearing Darkness fades out; reapplying it during fade-out restarts the
/// initial ramp without clearing the current brightness or target count.
/// Claims `ROOM_EFFECT_SCREEN_FADE_QUAD` until the work is released.
static void _effectDarknessScreenDimTaskE8(Task* task)
{
    enum {
        EFFECT_DARKNESS_STATE_START     = 0,
        EFFECT_DARKNESS_STATE_FADE_IN   = 1,
        EFFECT_DARKNESS_STATE_FLICKER   = 2,
        EFFECT_DARKNESS_STATE_FADE_OUT  = 3,
        EFFECT_DARKNESS_BASE_SHIFT      = 4,
        EFFECT_DARKNESS_BASE_BRIGHTNESS = 1 << EFFECT_DARKNESS_BASE_SHIFT,
        EFFECT_DARKNESS_BRIGHTNESS_STEP = 8,
    };
    EffectWork* work;
    u8          rgb[3];
    s32         brightness;
    s32         targetBrightness;
    u16         targetChangeCount;
    u32         randomState;

    work = task->spawnArg2.pointer;
    switch (task->state) {
        case EFFECT_DARKNESS_STATE_START:
            gRoomEffectState->screenFxFlags |= ROOM_EFFECT_SCREEN_FADE_QUAD;
            task->state                      = EFFECT_DARKNESS_STATE_FADE_IN;
            work->angle                      = EFFECT_DARKNESS_BASE_BRIGHTNESS;
            // Start drawing on the first tick, preserving brightness on a restart.
        case EFFECT_DARKNESS_STATE_FADE_IN:
            if (work->scale < work->angle) {
                work->scale += EFFECT_DARKNESS_BRIGHTNESS_STEP;
            } else {
                task->state = EFFECT_DARKNESS_STATE_FLICKER;
            }
            if (!(gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS)) {
                task->state = EFFECT_DARKNESS_STATE_FADE_OUT;
            }
            rgb[0] = rgb[1] = rgb[2] = work->scale;
            effectDrawScreenTint(rgb, GPU_BLEND_SUBTRACT);
            break;
        case EFFECT_DARKNESS_STATE_FLICKER:
            // Choose a new flicker target only after reaching the previous one.
            brightness       = work->scale;
            targetBrightness = work->angle;
            if (brightness == targetBrightness) {
                targetChangeCount = work->period + 1;
                randomState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->period      = targetChangeCount;
                gRandomLcgState   = randomState;
                work->angle       = ((targetChangeCount & 1) << (((randomState >> 16) & 1) + EFFECT_DARKNESS_BASE_SHIFT)) + EFFECT_DARKNESS_BASE_BRIGHTNESS;
            } else {
                if (brightness < targetBrightness) {
                    work->scale = brightness + EFFECT_DARKNESS_BRIGHTNESS_STEP;
                } else {
                    work->scale = brightness - EFFECT_DARKNESS_BRIGHTNESS_STEP;
                }
            }
            if (!(gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS)) {
                task->state = EFFECT_DARKNESS_STATE_FADE_OUT;
            }
            rgb[0] = rgb[1] = rgb[2] = work->scale;
            effectDrawScreenTint(rgb, GPU_BLEND_SUBTRACT);
            break;
        case EFFECT_DARKNESS_STATE_FADE_OUT:
            if (gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS) {
                task->state = EFFECT_DARKNESS_STATE_START;
                rgb[0] = rgb[1] = rgb[2] = work->scale;
                effectDrawScreenTint(rgb, GPU_BLEND_SUBTRACT);
            } else if (work->scale >= EFFECT_DARKNESS_BRIGHTNESS_STEP + 1) {
                work->scale -= EFFECT_DARKNESS_BRIGHTNESS_STEP;
                rgb[0] = rgb[1] = rgb[2] = work->scale;
                effectDrawScreenTint(rgb, GPU_BLEND_SUBTRACT);
            } else {
                gRoomEffectState->screenFxFlags &= (u16)~ROOM_EFFECT_SCREEN_FADE_QUAD;
                _effectReleaseCountedTask(gRoomEffectState, work, task);
            }
            break;
    }
}

/// Draws a short colored screen pulse for the selected player-status visual bit.
///
/// Bank-6 slot 0xF owns a zero-initialized, counted `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` is zero (no drawing) or one of the
/// eight single-bit byte selectors, and must match the room's `peFadeMask`.
/// `scale` advances the sine phase in 4096 units per turn; `angle` holds its
/// Q7 amplitude. Five task ticks draw the pulse, including its last tick;
/// replacing the selector releases it before drawing. Work release precedes
/// task teardown and its child exit callbacks.
static void _effectStatusScreenTintTaskF(Task* task)
{
    enum {
        EFFECT_STATUS_TINT_PHASE_STEP        = 0x180,
        EFFECT_STATUS_TINT_PHASE_END         = 0x700,
        EFFECT_STATUS_TINT_AMPLITUDE_SHIFT   = 5,
        EFFECT_STATUS_TINT_LOG_FRACTION_BITS = 12,
        EFFECT_STATUS_TINT_LOG_TWO           = 2839, // ln(2) in the SDK's Q12 logarithm.
        EFFECT_STATUS_TINT_COLOR_INDEX_MASK  = 7,
        EFFECT_STATUS_TINT_COLOR_NIBBLE_BITS = 4,
        EFFECT_STATUS_TINT_COLOR_NIBBLE_MASK = 0xF,
        EFFECT_STATUS_TINT_COLOR_SCALE_SHIFT = 3,
    };
    RoomEffectState* effectState;
    EffectWork*      work;
    u16              packedTint;
    u8               rgb[3];

    effectState = gRoomEffectState;
    work        = task->spawnArg2.pointer;
    if (effectState->peFadeMask != task->spawnArg1.value) {
        _effectReleaseCountedTask(effectState, work, task);
        return;
    }

    work->scale += EFFECT_STATUS_TINT_PHASE_STEP;
    work->angle  = rsin(work->scale) >> EFFECT_STATUS_TINT_AMPLITUDE_SHIFT;
    if (task->spawnArg1.value != 0) {
        // Convert the byte selector through the SDK's Q12 logarithm to a table index.
        packedTint = Gp_FadeQuadColors[(cln(task->spawnArg1.value << EFFECT_STATUS_TINT_LOG_FRACTION_BITS) / EFFECT_STATUS_TINT_LOG_TWO) & EFFECT_STATUS_TINT_COLOR_INDEX_MASK];
        // Packed nibbles are blend mode, red, green and blue; scale RGB by the pulse.
        rgb[0] = (work->angle * ((packedTint >> (2 * EFFECT_STATUS_TINT_COLOR_NIBBLE_BITS)) & EFFECT_STATUS_TINT_COLOR_NIBBLE_MASK)) >> EFFECT_STATUS_TINT_COLOR_SCALE_SHIFT;
        rgb[1] = (work->angle * ((packedTint >> EFFECT_STATUS_TINT_COLOR_NIBBLE_BITS) & EFFECT_STATUS_TINT_COLOR_NIBBLE_MASK)) >> EFFECT_STATUS_TINT_COLOR_SCALE_SHIFT;
        rgb[2] = (work->angle * (packedTint & EFFECT_STATUS_TINT_COLOR_NIBBLE_MASK)) >> EFFECT_STATUS_TINT_COLOR_SCALE_SHIFT;
        effectDrawScreenTint(rgb, packedTint >> (3 * EFFECT_STATUS_TINT_COLOR_NIBBLE_BITS));
    }
    if (work->scale >= EFFECT_STATUS_TINT_PHASE_END) {
        _effectReleaseCountedTask(gRoomEffectState, work, task);
    }
}

void effectKillTask(void* effectWork, Task* task)
{
    // Retire the counted work before teardown dispatches child exit handlers.
    gRoomEffectState->effectCount--;
    memFree(effectWork);
    taskKill(task);
}

/// Exit callback releasing one counted effect's spawn-argument work before task teardown.
///
/// The live task must be counted in `gRoomEffectState`; `spawnArg2.pointer` must
/// be NULL or its owned primary-heap work allocation. Nested resources must
/// already be released, and `Task::work` must not own the same allocation.
/// The count and work retire before teardown runs child exit handlers.
static void _effectExitTask(Task* task)
{
    void* effectWork;

    effectWork = task->spawnArg2.pointer;
    gRoomEffectState->effectCount--;
    memFree(effectWork);
    taskKill(task);
}

void Gp_PulseState1C(void)
{
    gRoomEffectState->pendingCancelFlags |= ROOM_EFFECT_CANCEL_ALL;
}

/// Enables primitive blending and prepends its draw mode at a fixed-quantized depth.
///
/// `primitive` is a writable initialized polygon, line or rectangle packet
/// already linked at `depth >> 4`. That signed index must be in the current
/// ordering table; it is neither display-scaled nor wrapped. The low two bits
/// of `blendMode` select `GPU_BLEND_*`. The draw mode enables dithering,
/// excludes the displayed area and selects a 4-bit page at VRAM (640, 0).
/// Consumes one word-aligned `DR_TPAGE` in the frame arena without checking
/// capacity; both packets must live until GPU drawing completes.
static void _gpuSetPrimitiveBlendModeFixedDepth(void* primitive, s32 blendMode, s32 depth)
{
    DR_TPAGE* blendCommand;

    setSemiTrans(primitive, 1);
    blendCommand   = gGpuPrimCursor;
    gGpuPrimCursor = blendCommand + 1;
    setDrawTPage(blendCommand, false, true,
                 getTPage(GPU_EFFECT_TEXTURE_DEPTH_4BIT, blendMode, GPU_EFFECT_TEXTURE_PAGE_X, 0));
    addPrim(gGpuCurrentOt + (depth >> GPU_EFFECT_FIXED_DEPTH_SHIFT), blendCommand);
}

/// Prepends a GPU draw-mode command for blending untextured primitives.
///
/// The low two bits of `blendMode` select a `GPU_BLEND_*` mode. `sortingDepth`
/// is the unscaled sorting depth: its unsigned, shifted value wraps to one
/// of the 1024 depth tags, so the current ordering table must contain that tag.
/// Primitives needing this mode must already be linked at the same depth;
/// prepending the command makes the GPU apply it before those primitives.
///
/// Requires word-aligned space for `sizeof(DR_TPAGE)` at `gGpuPrimCursor` and
/// advances the cursor without checking capacity. The packet borrows the frame
/// arena until GPU drawing completes. Its draw state persists until replaced:
/// dithering enabled, drawing into the displayed area disabled, and a fixed
/// 4-bit texture page at VRAM (640, 0).
static inline void _gpuQueueBlendMode(s32 blendMode, s32 sortingDepth)
{
    enum {
        /// Unshifted selector prohibiting drawing into the displayed VRAM area.
        ///
        /// Passing zero to `setDrawTPage` leaves draw-mode bit 10 (0x0400)
        /// clear. This restriction remains active until another draw-mode
        /// command replaces it.
        GPU_BLEND_DRAW_TO_DISPLAY_DISABLED = 0,
        /// Unshifted selector enabling dithering in the blend draw-mode packet.
        ///
        /// `setDrawTPage` encodes this nonzero argument as bit 9 (0x0200),
        /// independently of the semitransparency selector in bits 5..6.
        GPU_BLEND_DITHER_ENABLED = 1,
        /// Unshifted texture-page depth selector for 4-bit CLUT-indexed texels.
        ///
        /// `getTPage` encodes this zero selector in draw-mode bits 7..8.
        /// The blend command fixes the texture depth even though untextured
        /// primitives do not sample the selected page.
        GPU_BLEND_TEXTURE_DEPTH_4BIT = 0,
        /// Horizontal texture-page origin in VRAM 16-bit words for the blend draw mode.
        ///
        /// Pass this 64-word-aligned origin unshifted to `getTPage`, which
        /// encodes 640 as 0xA in draw-mode bits 0..3. The command selects this
        /// page even though untextured primitives do not sample its texels.
        GPU_BLEND_TEXTURE_PAGE_X = 640,
        /// Vertical texture-page origin in VRAM rows for the blend draw mode.
        ///
        /// Pass this 256-row-aligned origin unshifted to `getTPage`. Zero
        /// leaves the Y contributions in draw-mode bits 4 and 11 clear;
        /// untextured primitives do not sample the selected page.
        GPU_BLEND_TEXTURE_PAGE_Y = 0,
    };
    DR_TPAGE* blendCommand;

    blendCommand   = gGpuPrimCursor;
    gGpuPrimCursor = blendCommand + 1;
    // The fixed texture page is unused by untextured primitives; its ABR bits select blending.
    setDrawTPage(blendCommand, GPU_BLEND_DRAW_TO_DISPLAY_DISABLED, GPU_BLEND_DITHER_ENABLED,
                 getTPage(GPU_BLEND_TEXTURE_DEPTH_4BIT, blendMode, GPU_BLEND_TEXTURE_PAGE_X, GPU_BLEND_TEXTURE_PAGE_Y));
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                ((((u32)sortingDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            blendCommand);
}

void gpuSetPrimitiveBlendMode(void* primitive, s32 blendMode, s32 depth)
{
    setSemiTrans(primitive, 1);
    _gpuQueueBlendMode(blendMode, depth);
}

void func_800EC9C8(void)
{
    if (!(gRoomEffectState->screenFxFlags & ROOM_EFFECT_SCREEN_FADE_QUAD)) {
        Gp_SpawnEff((EFFECT_DARKNESS_SCREEN_DIM | EFFECT_SPAWN_UNLIMITED), 0, 0, 0);
    }
}

void Gp_SetState1CPe(s32 arg0)
{
    gRoomEffectState->peFadeMask = (u8)arg0;
    Gp_SpawnEff((EFFECT_STATUS_AILMENT_SCREEN_TINT | EFFECT_SPAWN_UNLIMITED), 0, (s32)((u8)arg0), 0);
}

void func_800ECA54(void)
{
    RoomEffectState* effectState;

    effectState = gRoomEffectState;
    if (!(effectState->screenFxFlags & ROOM_EFFECT_SCREEN_BURST_GUARD)) {
        effectState->peFxFlags &= (u16)~ROOM_EFFECT_PE_STATUS_BURST;
        Gp_SpawnEff((EFFECT_BERSERKER_SHOT_GLOW | EFFECT_SPAWN_UNLIMITED), 0, 0, 0);
    }
}
