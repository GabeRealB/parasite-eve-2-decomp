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

#include "stages/akropolis/actors/helicopter_landing_pad_no_9.h"

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

#include "pe/combat_light.h"

#include "pe/combustion.h"

#include "pe/energyball.h"

#include "pe/energyshot.h"

#include "pe/flare.h"

#include "pe/healing.h"

#include "pe/inferno.h"

#include "pe/lifedrain.h"

#include "pe/metabolism.h"

#include "pe/necrosis.h"

#include "pe/pepper_spray.h"

#include "pe/plasma.h"

#include "pe/pyrokinesis.h"

#include "stages/akropolis/rooms/bridge.h"

#include "stages/akropolis/rooms/cafeteria.h"

#include "stages/akropolis/rooms/east_elevator_hall.h"

#include "stages/akropolis/rooms/fire_escape.h"

#include "stages/akropolis/rooms/forked_road.h"

#include "stages/akropolis/rooms/fountain.h"

#include "stages/akropolis/rooms/hallway.h"

#include "stages/akropolis/rooms/helicopter_landing_pad.h"

#include "stages/akropolis/rooms/observatory.h"

#include "stages/akropolis/rooms/patio.h"

#include "stages/akropolis/rooms/plaza.h"

#include "stages/akropolis/rooms/promenade.h"

#include "stages/akropolis/rooms/roof_garden.h"

#include "stages/akropolis/rooms/sanctuary.h"

#include "stages/akropolis/rooms/security_room.h"

#include "stages/akropolis/rooms/square.h"

#include "stages/akropolis/rooms/west_elevator_hall.h"

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

#include "stages/akropolis/rooms/mist_parking.h"

#include "stages/akropolis/rooms/mist_shooting_gallery.h"

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

/// Last successfully initialized room-effect controller task.
///
/// Borrows the task without extending its lifetime; teardown does not clear it.
Task* _gRoomEffectTask;

s32 gRoomEffectWaterRippleId;

s32 gRoomEffectSparkBurstId;

s32 gRoomEffectOrangeBurst2Id;

s32 gRoomEffectFlashId;

#include "gameplay/effect_tasks.h"
#include "gameplay/room_effects.h"

/// Zero area-table entries suppress the ambient-effect task.
enum { ROOM_EFFECT_AMBIENT_TASK_NONE = 0 };

static const s32 _gRoomEffectShelterNeoArkTaskTypes[33];

static const TaskFuncTable3 _gRoomEffectControllerStates;

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
/// `savedCursor` must equal `probe + 1`; both views address the same reserved
/// `_WorldCollisionGroundProbeScratch`, whose origin X/Y are already filled.
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

static void _roomEffectInitController(Task* task);

static void _roomEffectTickController(Task* unusedTask);

static void _worldCoordTickTransientPointLights(void);

static void _worldCoordInitTransientPointLights(void);

static void _roomEffectControllerTask(Task* task);

static void _effectStatusScreenTintTaskF(Task* task);

static void _effectExitTask(Task* task);

static void _gpuSetPrimitiveBlendModeFixedDepth(void* primitive, s32 blendMode, s32 depth);

/// Unresolved weapon-overlay callback for effect-bank slots 0xBB and 0xBD.
///
/// Both slots allocate a single-coordinate body and pass a live `task`.
/// The code at this imported address depends on the loaded weapon overlay;
/// its owner, behavior and spawn-argument contract are unproven.
extern void func_8011D1E0(Task* task);

TaskDesc gEffectTaskDescriptors[667] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill, { NULL } },                                                               // 0x000
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill, { NULL } },                                                               // 0x001
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill, { NULL } },                                                               // 0x002
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill, { NULL } },                                                               // 0x003
    { { { TASK_BODY_NONE, 0x4F } }, _roomEffectControllerTask, { NULL } },                                                               // 0x004
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMotelBalconyRoomVisualEffectsFlashTask, { NULL } },                         // 0x005
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightGasStationRoomVisualEffectsFlashTask, { NULL } },                      // 0x006
    { { { TASK_BODY_NONE, TASK_PRIORITY_EFFECT } }, effectControlTask07, { NULL } },                                                     // 0x007
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightGasStationRoomVisualEffectsTwinTrailTask, { NULL } },                  // 0x008
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightGasStationRoomVisualEffectsSparkBurstTask, { NULL } },                 // 0x009
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightBackStreetRoomVisualEffectsFlashTask, { NULL } },                      // 0x00A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, hypervelocityDischargeConeTask, { NULL } },                                         // 0x00B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, hypervelocityRoundTask, { NULL } },                                                 // 0x00C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, hypervelocityShockRingTask, { NULL } },                                             // 0x00D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectControlTask0E, { NULL } },                                                    // 0x00E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, _effectStatusScreenTintTaskF, { NULL } },                                           // 0x00F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, pyrokinesisCastTask, { NULL } },                                                    // 0x010
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, pyrokinesisLaunchConeTask, { NULL } },                                              // 0x011
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, metabolismCastTask, { NULL } },                                                     // 0x012
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, metabolismSparkleTask, { NULL } },                                                  // 0x013
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, plasmaCastTask, { NULL } },                                                         // 0x014
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, healingAuraTask, { NULL } },                                                        // 0x015
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, healingRisingSparkTask, { NULL } },                                                 // 0x016
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, healingSparkleTask, { NULL } },                                                     // 0x017
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, necrosisCastTask, { NULL } },                                                       // 0x018
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, necrosisTrailPuffTask, { NULL } },                                                  // 0x019
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, necrosisMistPuffTask, { NULL } },                                                   // 0x01A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, combustionFlameEmitterTask, { NULL } },                                             // 0x01B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, combustionFlameTask, { NULL } },                                                    // 0x01C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisHelicopterLandingPadPerimeterLightsTask, { NULL } },                       // 0x01D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisWestElevatorHallViewEffectsTask, { NULL } },                               // 0x01E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisWestElevatorHallRedBeaconTask, { NULL } },                                 // 0x01F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisWestElevatorHallScanlineDistortionTask, { NULL } },                        // 0x020
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisEastElevatorHallReflectionsAndBeaconsTask, { NULL } },                     // 0x021
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisEastElevatorHallRedBeaconTask, { NULL } },                                 // 0x022
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisEastElevatorHallPointTileTask, { NULL } },                                 // 0x023
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, hypervelocityChargeEffectTask, { NULL } },                                          // 0x024
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisWestElevatorHallLightGlowTask, { NULL } },                                 // 0x025
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisFountainViewEffectsTask, { NULL } },                                       // 0x026
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisObservatoryLensFlareEmitterTask, { NULL } },                               // 0x027
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisObservatoryAmbientGlowTask, { NULL } },                                    // 0x028
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, m4a1HammerGlowTask, { NULL } },                                                     // 0x029
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, m4a1PykeNozzleTask, { NULL } },                                                     // 0x02A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectControlTask2B, { NULL } },                                                    // 0x02B
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill, { NULL } },                                                               // 0x02C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisSquareRoomEffectTask, { NULL } },                                          // 0x02D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mistParkingContinueDepartureChoiceTask, { NULL } },                                 // 0x02E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, m4a1JavelinGuideBeamTask, { NULL } },                                               // 0x02F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectGravityParticleTask, { NULL } },                                              // 0x030
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill, { NULL } },                                                               // 0x031
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask32, { NULL } },                                                     // 0x032
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisWestElevatorHallBayLightingTask, { NULL } },                               // 0x033
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask34, { NULL } },                                                     // 0x034
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask35, { NULL } },                                                     // 0x035
    { { { TASK_BODY_TMD, TASK_PRIORITY_EFFECT } }, effectThrownModelTask, { &gEffectBulletCasingModel } },                               // 0x036
    { { { TASK_BODY_TMD, TASK_PRIORITY_EFFECT } }, effectBurstModelPartTask, { &gEffectShotgunShellCasingModel } },                      // 0x037
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSubstationDrawLightGlowsTask, { NULL } },                                     // 0x038
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mistParkingDrawGlowsTask, { NULL } },                                               // 0x039
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, tonfaBatonSwingTrailTask, { NULL } },                                               // 0x03A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectControlTask3B, { NULL } },                                                    // 0x03B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldBreezewayBouncingParticleTask, { NULL } },                                  // 0x03C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelBalconyDebrisTask, { NULL } },                                    // 0x03D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, m4a1BayonetTrailTask, { NULL } },                                                   // 0x03E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask3F, { NULL } },                                                     // 0x03F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, p229MuzzleFlashTask, { NULL } },                                                    // 0x040
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mp5a5MuzzleFlashTask, { NULL } },                                                   // 0x041
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask42, { NULL } },                                                     // 0x042
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, helicopterLandingPadNo9FlameJetTask, { NULL } },                                    // 0x043
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, helicopterLandingPadNo9MuzzleFlashTask, { NULL } },                                 // 0x044
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, helicopterLandingPadNo9GolemFlameTask, { NULL } },                                  // 0x045
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask46, { NULL } },                                                     // 0x046
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisSquareTelephoneLightTask, { NULL } },                                      // 0x047
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisSecurityRoomLockLampsTask, { NULL } },                                     // 0x048
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisSecurityRoomLockLampTask, { NULL } },                                      // 0x049
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisPromenadeRoomEffectTask, { NULL } },                                       // 0x04A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisPromenadeGlowStarTask, { NULL } },                                         // 0x04B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, helicopterLandingPadNo9FlameTongueTask, { NULL } },                                 // 0x04C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisPlazaSpawnAmbientLightsTask, { NULL } },                                   // 0x04D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisFireEscapeLightEmitterTask, { NULL } },                                    // 0x04E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisFireEscapeFlareTask, { NULL } },                                           // 0x04F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelBalconyBreakBurstTask, { NULL } },                                // 0x050
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisForkedRoadLeafFallTask, { NULL } },                                        // 0x051
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, helicopterLandingPadNo9FlameSpriteTask, { NULL } },                                 // 0x052
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask53, { NULL } },                                                     // 0x053
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask54, { NULL } },                                                     // 0x054
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask55, { NULL } },                                                     // 0x055
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisPromenadeScreenDripTask, { NULL } },                                       // 0x056
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisPromenadeGroundGlowTask, { NULL } },                                       // 0x057
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkWoodlandPathWaterRippleTask, { NULL } },                                      // 0x058
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, helicopterLandingPadNo9GunfireParticleTask, { NULL } },                             // 0x059
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisHelicopterLandingPadEmberTask, { NULL } },                                 // 0x05A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisHelicopterLandingPadDamagedLightSparksTask, { NULL } },                    // 0x05B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask5C, { NULL } },                                                     // 0x05C
    { { { TASK_BODY_NONE, TASK_PRIORITY_EFFECT } }, taskKill, { NULL } },                                                                // 0x05D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisHelicopterLandingPadLensFlareTask, { NULL } },                             // 0x05E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisHelicopterLandingPadFlareEmitterTask, { NULL } },                          // 0x05F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisCafeteriaInitRoomEffectsTask, { NULL } },                                  // 0x060
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisCafeteriaPuffTask, { NULL } },                                             // 0x061
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisPromenadeGlowLampTask, { NULL } },                                         // 0x062
    { { { TASK_BODY_NONE, TASK_PRIORITY_EFFECT } }, taskKill, { NULL } },                                                                // 0x063
    { { { TASK_BODY_TMD, TASK_PRIORITY_EFFECT } }, akropolisCafeteriaModelWanderTask, { &gAkropolisCafeteriaWanderingCreatureModel } },  // 0x064
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, helicopterLandingPadNo9DebrisStreakTask, { NULL } },                                // 0x065
    { { { TASK_BODY_TMD, TASK_PRIORITY_EFFECT } }, effectThrownModelTask, { &gEffectP229CasingModel } },                                 // 0x066
    { { { TASK_BODY_TMD, TASK_PRIORITY_EFFECT } }, effectThrownModelTask, { &gEffectRifleCasingModel } },                                // 0x067
    { { { TASK_BODY_TMD, TASK_PRIORITY_EFFECT } }, effectThrownModelTask, { &gEffectShotgunShellCasingModel } },                         // 0x068
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, pyrokinesisFlamePuffTask, { NULL } },                                               // 0x069
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectControlTask6A, { NULL } },                                                    // 0x06A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectControlTask6B, { NULL } },                                                    // 0x06B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectControlTask6C, { NULL } },                                                    // 0x06C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectControlTask6D, { NULL } },                                                    // 0x06D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectControlTask6E, { NULL } },                                                    // 0x06E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask6F, { NULL } },                                                     // 0x06F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask70, { NULL } },                                                     // 0x070
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectControlTask71, { NULL } },                                                    // 0x071
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask72, { NULL } },                                                     // 0x072
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMotelBalconyRoomVisualEffectsTwinTrailTask, { NULL } },                     // 0x073
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMotelBalconyRoomVisualEffectsSparkBurstTask, { NULL } },                    // 0x074
    { { { TASK_BODY_NONE, TASK_PRIORITY_EFFECT } }, taskKill, { NULL } },                                                                // 0x075
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask76, { NULL } },                                                     // 0x076
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisSanctuaryRoomEffectTask, { NULL } },                                       // 0x077
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisSanctuaryMosaicTask, { NULL } },                                           // 0x078
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisSanctuaryMosaicTileTask, { NULL } },                                       // 0x079
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisSanctuaryMosaicShardTask, { NULL } },                                      // 0x07A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisSecurityRoomFallingQuadTask, { NULL } },                                   // 0x07B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask7C, { NULL } },                                                     // 0x07C
    { { { TASK_BODY_NONE, TASK_PRIORITY_EFFECT } }, taskKill, { NULL } },                                                                // 0x07D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelBalconyFlameTask, { NULL } },                                     // 0x07E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectControlTask7F, { NULL } },                                                    // 0x07F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask80, { NULL } },                                                     // 0x080
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectProjectileGlowTask, { NULL } },                                               // 0x081
    { { { TASK_BODY_NONE, TASK_PRIORITY_EFFECT } }, taskKill, { NULL } },                                                                // 0x082
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisPatioSpawnLightEffectsTask, { NULL } },                                    // 0x083
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisHallwayRoomEffectTask, { NULL } },                                         // 0x084
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisForkedRoadInitializeRoomEffectsTask, { NULL } },                           // 0x085
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisRoofGardenAmbientEffectsTask, { NULL } },                                  // 0x086
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisPatioLightGlowTask, { NULL } },                                            // 0x087
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisFountainSprayTask, { NULL } },                                             // 0x088
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisForkedRoadWallLampTask, { NULL } },                                        // 0x089
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisRoofGardenLightGlowTask, { NULL } },                                       // 0x08A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisSanctuaryFlameTask, { NULL } },                                            // 0x08B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisFireEscapeFlickerLightTask, { NULL } },                                    // 0x08C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask8D, { NULL } },                                                     // 0x08D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectHitSparkBurstTask, { NULL } },                                                // 0x08E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisPatioLightMoteTask, { NULL } },                                            // 0x08F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisRoofGardenFlareTask, { NULL } },                                           // 0x090
    { { { TASK_BODY_TMD, TASK_PRIORITY_EFFECT } }, effectThrownModelTask, { &gEffectThrownModel91 } },                                   // 0x091
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectLineTask92, { NULL } },                                                       // 0x092
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelBalconyFallingParticleTask, { NULL } },                           // 0x093
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelBalconyLampBurstTask, { NULL } },                                 // 0x094
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelBalconyDriftPuffTask, { NULL } },                                 // 0x095
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisPlazaLightGlowTask, { NULL } },                                            // 0x096
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightBackStreetRoomVisualEffectsTwinTrailTask, { NULL } },                  // 0x097
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisPlazaSirenLightTask, { NULL } },                                           // 0x098
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisPlazaLightFlareTask, { NULL } },                                           // 0x099
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectHitPuffEmitterTask, { NULL } },                                               // 0x09A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectHitSplatterSprayTask, { NULL } },                                             // 0x09B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectPolyTask9C, { NULL } },                                                       // 0x09C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisCafeteriaPuffEmitterTask, { NULL } },                                      // 0x09D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTask9E, { NULL } },                                                     // 0x09E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldToiletJetPuffTask, { NULL } },                                              // 0x09F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisSecurityRoomLockLampGlowTask, { NULL } },                                  // 0x0A0
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectControlTaskA1, { NULL } },                                                    // 0x0A1
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldToiletSprayEmitterTask, { NULL } },                                         // 0x0A2
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectLineTaskA3, { NULL } },                                                       // 0x0A3
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectTileTaskA4, { NULL } },                                                       // 0x0A4
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectCorpseBurnTask, { NULL } },                                                   // 0x0A5
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectDeathFlameTask, { NULL } },                                                   // 0x0A6
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTaskA7, { NULL } },                                                     // 0x0A7
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectPeChargeTask, { NULL } },                                                     // 0x0A8
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, combustionEmberTask, { NULL } },                                                    // 0x0A9
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4ReservoirBurstSpriteTask, { NULL } },                                      // 0x0AA
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, lifedrainCastTask, { NULL } },                                                      // 0x0AB
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectAntibodyAuraTask, { NULL } },                                                 // 0x0AC
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, lifedrainRisingSparkTask, { NULL } },                                               // 0x0AD
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectControlTaskAE, { NULL } },                                                    // 0x0AE
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, lifedrainMoteTask, { NULL } },                                                      // 0x0AF
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisBridgeAmbientEffectsTask, { NULL } },                                      // 0x0B0
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisBridgeGlowStarTask, { NULL } },                                            // 0x0B1
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisBridgeGroundGlowTask, { NULL } },                                          // 0x0B2
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisBridgeGlowLampTask, { NULL } },                                            // 0x0B3
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisBridgeFallingStreakTask, { NULL } },                                       // 0x0B4
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisBridgeMidDustStreakTask, { NULL } },                                       // 0x0B5
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisBridgeLowDustStreakTask, { NULL } },                                       // 0x0B6
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisBridgeTallDustStreakTask, { NULL } },                                      // 0x0B7
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisBridgeParticleStreakTask, { NULL } },                                      // 0x0B8
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisBridgeWaterRippleTask, { NULL } },                                         // 0x0B9
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisBridgeEffectSpriteDebrisTask, { NULL } },                                  // 0x0BA
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, func_8011D1E0, { NULL } },                                                          // 0x0BB
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisBridgeDustMoteTask, { NULL } },                                            // 0x0BC
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, func_8011D1E0, { NULL } },                                                          // 0x0BD
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, infernoCastTask, { NULL } },                                                        // 0x0BE
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldUnderpassDrawFlaresTask, { NULL } },                                        // 0x0BF
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, apobiosisCastTask, { NULL } },                                                      // 0x0C0
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectPolyTaskC1, { NULL } },                                                       // 0x0C1
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldGasStationCyanGlowTask, { NULL } },                                         // 0x0C2
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMainStreetRoomEffectsTask, { NULL } },                                      // 0x0C3
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldGeneralStoreNoOpEffectTask, { NULL } },                                     // 0x0C4
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldBackStreetConfigureEffectsTask, { NULL } },                                 // 0x0C5
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldSouvenirShopLightPrismsTask, { NULL } },                                    // 0x0C6
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldWarehouseDrawGlowsTask, { NULL } },                                         // 0x0C7
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldDilapidatedHouseLightPrismTask, { NULL } },                                 // 0x0C8
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMotelRoom1NoOpEffectTask, { NULL } },                                       // 0x0C9
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMotelRoom2EffectNoopTaskCA, { NULL } },                                     // 0x0CA
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, antibodyCastTask, { NULL } },                                                       // 0x0CB
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, energyshotCastTask, { NULL } },                                                     // 0x0CC
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldParkingLotUpdateViewEffectGateTask, { NULL } },                             // 0x0CD
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldToiletConfigureEffectsTask, { NULL } },                                     // 0x0CE
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, energyballCastTask, { NULL } },                                                     // 0x0CF
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldSaloonGRDrawLightEffectsTask, { NULL } },                                   // 0x0D0
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldGRKitchenDrawLightBeamsTask, { NULL } },                                    // 0x0D1
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldWaterTowerUpdateViewEffectGateTask, { NULL } },                             // 0x0D2
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldWaterTankUpdateViewEffectGateTask, { NULL } },                              // 0x0D3
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldBreezewayAmbientEffectsTask, { NULL } },                                    // 0x0D4
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldFactoryDrawGlowsTask, { NULL } },                                           // 0x0D5
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldGarageEffectNoopTaskD6, { NULL } },                                         // 0x0D6
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldDrivewayEnableAmbientEffectsTask, { NULL } },                               // 0x0D7
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldJunkYardEnableViewEffectsTask, { NULL } },                                  // 0x0D8
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldTrailerCoachDrawGlowsTask, { NULL } },                                      // 0x0D9
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, infernoFlameFanTask, { NULL } },                                                    // 0x0DA
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMotelBalconyInitRoomEffectsTask, { NULL } },                                // 0x0DB
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMotelRoom6DrawGlowTask, { NULL } },                                         // 0x0DC
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, energyshotRisingBillboardTask, { NULL } },                                          // 0x0DD
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldWaterHoleRoomEffectsTask, { NULL } },                                       // 0x0DE
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldCellarDrawGlowsTask, { NULL } },                                            // 0x0DF
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTaskE0, { NULL } },                                                     // 0x0E0
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTaskE1, { NULL } },                                                     // 0x0E1
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTaskE2, { NULL } },                                                     // 0x0E2
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectHitSmokeEmitterTask, { NULL } },                                              // 0x0E3
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightBackStreetRoomVisualEffectsSparkBurstTask, { NULL } },                 // 0x0E4
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightJunkYardRoomVisualEffectsFlashTask, { NULL } },                        // 0x0E5
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightJunkYardRoomVisualEffectsTwinTrailTask, { NULL } },                    // 0x0E6
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightJunkYardRoomVisualEffectsSparkBurstTask, { NULL } },                   // 0x0E7
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, _effectDarknessScreenDimTaskE8, { NULL } },                                         // 0x0E8
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineMesaRoomVisualEffectsFlashTask, { NULL } },                                     // 0x0E9
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, lifedrainExpandingGlowBandTask, { NULL } },                                         // 0x0EA
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineMesaRoomVisualEffectsTwinTrailTask, { NULL } },                                 // 0x0EB
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineMesaRoomVisualEffectsSparkBurstTask, { NULL } },                                // 0x0EC
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4LowerSewerRoomVisualEffectsFlashTask, { NULL } },                          // 0x0ED
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4LowerSewerRoomVisualEffectsTwinTrailTask, { NULL } },                      // 0x0EE
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4LowerSewerRoomVisualEffectsSparkBurstTask, { NULL } },                     // 0x0EF
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4UpperSewerRoomVisualEffectsFlashTask, { NULL } },                          // 0x0F0
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4UpperSewerRoomVisualEffectsTwinTrailTask, { NULL } },                      // 0x0F1
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4UpperSewerRoomVisualEffectsSparkBurstTask, { NULL } },                     // 0x0F2
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectEnergyShotAuraTask, { NULL } },                                               // 0x0F3
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, effectSpriteTaskF4, { NULL } },                                                     // 0x0F4
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, antibodyMoteTask, { NULL } },                                                       // 0x0F5
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, pyrokinesisFlameRingTask, { NULL } },                                               // 0x0F6
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, apobiosisShardTask, { NULL } },                                                     // 0x0F7
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, energyballProjectileTask, { NULL } },                                               // 0x0F8
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, energyballImpactRingTask, { NULL } },                                               // 0x0F9
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkForestZoneConfigureEffectsTask, { NULL } },                                   // 0x0FA
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkForestZoneLeafFallTask, { NULL } },                                           // 0x0FB
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, pyrokinesisVolleyTask, { NULL } },                                                  // 0x0FC
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldWaterHoleWaterRippleTask, { NULL } },                                       // 0x0FD
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldWaterHoleWaterDriftTaskU16, { NULL } },                                     // 0x0FE
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightWaterHoleWaterRippleTask, { NULL } },                                  // 0x0FF
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightGasStationAmbientEffectsTask, { NULL } },                              // 0x100
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMainStreetAmbientEffectsTask, { NULL } },                              // 0x101
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightGeneralStoreDrawLightShaftsTask, { NULL } },                           // 0x102
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightBackStreetDrawGlowsTask, { NULL } },                                   // 0x103
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightSouvenirShopPrismLightTask, { NULL } },                                // 0x104
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightWarehouseDrawGlowsTask, { NULL } },                                    // 0x105
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightDilapidatedHouseDrawLightPrismsTask, { NULL } },                       // 0x106
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelRoom1DrawGlowsTask, { NULL } },                                   // 0x107
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelRoom2DrawFlaresTask, { NULL } },                                  // 0x108
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelRoom3DrawFlaresTask, { NULL } },                                  // 0x109
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelRoom4DrawFlaresTask, { NULL } },                                  // 0x10A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightParkingLotDrawGlowsTask, { NULL } },                                   // 0x10B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightToiletDrawFlareTask, { NULL } },                                       // 0x10C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelLobbyDrawGlowsTask, { NULL } },                                   // 0x10D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightSaloonGRDrawGlowsTask, { NULL } },                                     // 0x10E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightGRKitchenDrawLightShaftsTask, { NULL } },                              // 0x10F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightWaterTowerDrawGlowsTask, { NULL } },                                   // 0x110
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightWaterTankNoOpEffectTask, { NULL } },                                   // 0x111
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightBreezewayDrawGlowsTask, { NULL } },                                    // 0x112
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightFactoryDrawGlowsTask, { NULL } },                                      // 0x113
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightGarageDrawGlowsTask, { NULL } },                                       // 0x114
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightDrivewayDrawLightShaftsTask, { NULL } },                               // 0x115
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightJunkYardDrawGlowsTask, { NULL } },                                     // 0x116
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightTrailerCoachDrawGlowsTask, { NULL } },                                 // 0x117
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelRoom5DrawFlareTask, { NULL } },                                   // 0x118
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelBalconyAmbientEffectsTask, { NULL } },                            // 0x119
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelRoom6DrawGlowTask, { NULL } },                                    // 0x11A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelLoftFlaresAndShardsTask, { NULL } },                              // 0x11B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightWaterHoleSplashAndLightShaftsTask, { NULL } },                         // 0x11C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightCellarDrawGlowsTask, { NULL } },                                       // 0x11D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightUnderpassDrawFlaresTask, { NULL } },                                   // 0x11E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightWaterHoleWaterDriftTaskU16, { NULL } },                                // 0x11F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineMesaDrawViewFlaresTask, { NULL } },                                             // 0x120
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineCavernDrawRoomLightGlowsTask, { NULL } },                                       // 0x121
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineTunnelEntranceDrawFlaresTask, { NULL } },                                       // 0x122
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineTunnelDrawViewFlaresTask, { NULL } },                                           // 0x123
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineGorgeDrawViewFlaresTask, { NULL } },                                            // 0x124
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineRefugeDrawGlowsTask, { NULL } },                                                // 0x125
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineForkedTunnelDrawViewFlaresTask, { NULL } },                                     // 0x126
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineSecretPassageDrawLightGlowsTask, { NULL } },                                    // 0x127
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ElevatorHallDrawGlowsTask, { NULL } },                                     // 0x128
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1SouthMaintenanceWalkwayDrawGlowsTask, { NULL } },                          // 0x129
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1StoreroomDrawGlowsTask, { NULL } },                                        // 0x12A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1NorthMaintenanceWalkwayDrawGlowsTask, { NULL } },                          // 0x12B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ArmoryDrawGlowsTask, { NULL } },                                           // 0x12C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1SleepingQuartersDrawViewLightsTask, { NULL } },                            // 0x12D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1MainCorridorDrawViewLightsTask, { NULL } },                                // 0x12E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1SterilizationRoomEffectsTask, { NULL } },                                  // 0x12F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1PodAccessTunnelDrawGlowsTask, { NULL } },                                  // 0x130
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ControlRoomDrawGlowsTask, { NULL } },                                      // 0x131
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1AccessTunnelDrawGlowsTask, { NULL } },                                     // 0x132
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1UndergroundParkingDrawGlowsTask, { NULL } },                               // 0x133
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1GolemFreezer1AmbientEffectsTask, { NULL } },                               // 0x134
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodBottomShadowTask, { NULL } },                                           // 0x135
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1PodServiceGantryInitEffectsTask, { NULL } },                               // 0x136
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1TransferTunnelDrawGlowsTask, { NULL } },                                   // 0x137
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ControlRoomAccessTunnelDrawGlowsTask, { NULL } },                          // 0x138
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2ElevatorEffectNoopTask, { NULL } },                                        // 0x139
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2ElevatorHallDrawGlowsTask, { NULL } },                                     // 0x13A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2SouthMaintenanceWalkwayDrawGlowsTask, { NULL } },                          // 0x13B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2OperatingRoomDrawGlowsTask, { NULL } },                                    // 0x13C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2NorthMaintenanceWalkwayGlowTask, { NULL } },                               // 0x13D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2LaboratoryGlowTask, { NULL } },                                            // 0x13E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2BreedingRoomDrawGlowsTask, { NULL } },                                     // 0x13F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2MainCorridorDrawViewGlowsTask, { NULL } },                                 // 0x140
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2SepticTankGlowTask, { NULL } },                                            // 0x141
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodAccessTunnelDrawLightBeamsTask, { NULL } },                             // 0x142
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3DumpingHoleDrawViewGlowsTask, { NULL } },                                  // 0x143
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3GarbageIncineratorDrawLightsTask, { NULL } },                              // 0x144
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3IncineratorControlRoomDrawViewGlowsTask, { NULL } },                       // 0x145
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3ElevatorHallDrawGlowsTask, { NULL } },                                     // 0x146
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4LowerSewerDrawGlowsTask, { NULL } },                                       // 0x147
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4UpperSewerDrawGlowsTask, { NULL } },                                       // 0x148
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4ReservoirAmbientEffectsTask, { NULL } },                                   // 0x149
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4WaterSupplySplashAndLightBeamsTask, { NULL } },                            // 0x14A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterR47DrawViewGlowsTask, { NULL } },                                            // 0x14B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterR48InitRingsAndDrawGlowTask, { NULL } },                                     // 0x14C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fAirlockDrawViewGlowsTask, { NULL } },                                      // 0x14D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkObservatoryGlowTask, { NULL } },                                              // 0x14E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkEveAccessTunnelDrawViewGlowsTask, { NULL } },                                 // 0x14F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkEveElevatorIdleEffectTask, { NULL } },                                        // 0x150
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkNorthPromenadeBindRoomEffectsTask, { NULL } },                                // 0x151
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSubmarineTunnelConfigureEffectsTask, { NULL } },                              // 0x152
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPavilionInstallRoomEffectsTask, { NULL } },                                   // 0x153
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkIslandInstallRoomEffectIdsTask, { NULL } },                                   // 0x154
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkGardenAmbienceTask, { NULL } },                                               // 0x155
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPowerPlant2UpdateViewLightingTask, { NULL } },                                // 0x156
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPowerPlant1DrawLightGlowsTask, { NULL } },                                    // 0x157
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSavannaZoneConfigureEffectsTask, { NULL } },                                  // 0x158
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSouthPromenadeRegisterGolemEffectsTask, { NULL } },                           // 0x159
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkAltarEffectNoopTask, { NULL } },                                              // 0x15A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkShrineFlareTask, { NULL } },                                                  // 0x15B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6NurseryViewEffectsTask, { NULL } },                                        // 0x15C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6GrowthRoomAmbientEffectsTask, { NULL } },                                  // 0x15D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6CorridorDrawViewGlowsTask, { NULL } },                                     // 0x15E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6TrainingRoomGlowTask, { NULL } },                                          // 0x15F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkBridgeAmbientEffectsTask, { NULL } },                                         // 0x160
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkWoodlandPathPlayerWaterSplashTask, { NULL } },                                // 0x161
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterR49EffectNoopTask, { NULL } },                                               // 0x162
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fParkingGarageDrawViewGlowsTask, { NULL } },                                // 0x163
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fVehicularAirlockDrawLightsTask, { NULL } },                                // 0x164
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fBulwarkDrawGlowsTask, { NULL } },                                          // 0x165
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fHeliportNoOpEffectTask, { NULL } },                                        // 0x166
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fGuardroomEffectNoopTask, { NULL } },                                       // 0x167
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkR26EffectNoopTask, { NULL } },                                                // 0x168
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fTentDrawViewGlowsTask, { NULL } },                                         // 0x169
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2MainCorridorWaterRippleTask, { NULL } },                                   // 0x16A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2MainCorridorWaterDriftTask, { NULL } },                                    // 0x16B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2SepticTankWaterRippleTask, { NULL } },                                     // 0x16C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2SepticTankWaterDriftTask, { NULL } },                                      // 0x16D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4LowerSewerWaterRippleTask, { NULL } },                                     // 0x16E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4LowerSewerWaterDriftTaskU16, { NULL } },                                   // 0x16F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4UpperSewerWaterRippleTask, { NULL } },                                     // 0x170
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4UpperSewerWaterDriftTask, { NULL } },                                      // 0x171
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4ReservoirWaterRippleTask, { NULL } },                                      // 0x172
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4ReservoirWaterDriftTask, { NULL } },                                       // 0x173
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4WaterSupplyWaterRippleTask, { NULL } },                                    // 0x174
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4WaterSupplyWaterDriftTask, { NULL } },                                     // 0x175
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPavilionWaterRippleTask, { NULL } },                                          // 0x176
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPavilionWaterSprayTask, { NULL } },                                           // 0x177
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkIslandWaterRippleTask, { NULL } },                                            // 0x178
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkIslandWaterSprayTask, { NULL } },                                             // 0x179
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkBridgeWaterRippleTask, { NULL } },                                            // 0x17A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkBridgeWaterDriftTask, { NULL } },                                             // 0x17B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisRoofGardenLeafFallTask, { NULL } },                                        // 0x17C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1SterilizationRoomPuffTask, { NULL } },                                     // 0x17D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightR08DrawGlowsTask, { NULL } },                                          // 0x17E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, m4a1PykeFlameTask, { NULL } },                                                      // 0x17F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, actor800100PykeEmitterTask, { NULL } },                                             // 0x180
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, actor800100PykeFlameTask, { NULL } },                                               // 0x181
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, m4a1HammerImpactFlashTask, { NULL } },                                              // 0x182
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, m4a1JavelinContactFlashTask, { NULL } },                                            // 0x183
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, helicopterLandingPadNo9ExplosionFireballTask, { NULL } },                           // 0x184
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, helicopterLandingPadNo9ImpactBurstTask, { NULL } },                                 // 0x185
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, gunbladeTrailTask, { NULL } },                                                      // 0x186
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkWoodlandPathWaterDriftTaskU16, { NULL } },                                    // 0x187
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldDilapidatedHouseTwinTrailTask, { NULL } },                                  // 0x188
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterR48SmallOrbTask, { NULL } },                                                 // 0x189
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterR48YellowRingFlashTask, { NULL } },                                          // 0x18A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterR48WaterDriftTaskU16, { NULL } },                                            // 0x18B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterR48SpriteDriftTask, { NULL } },                                              // 0x18C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterR48ProjectileTask, { NULL } },                                               // 0x18D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterR48LargeOrbTask, { NULL } },                                                 // 0x18E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterR48RingWallTask, { NULL } },                                                 // 0x18F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterR48OrangeRingFlashTask, { NULL } },                                          // 0x190
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterR48ShockwaveRingsTask, { NULL } },                                           // 0x191
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSubmarineGalleryDrawViewGlowsTask, { NULL } },                                // 0x192
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSubmarineGalleryWaterRippleTask, { NULL } },                                  // 0x193
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSubmarineGalleryWaterDriftTaskU16, { NULL } },                                // 0x194
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterR48PinkRingFlashTask, { NULL } },                                            // 0x195
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3GarbageIncineratorEffectSpriteDriftTaskAimed, { NULL } },                  // 0x196
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, combatLightEffectTask, { NULL } },                                                  // 0x197
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, flareEffectTask, { NULL } },                                                        // 0x198
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3DumpingHoleEffectSpriteDriftTaskAimed, { NULL } },                         // 0x199
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3DumpingHoleGluttonRainParticleTask, { NULL } },                            // 0x19A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3DumpingHoleGluttonRainBlobTask, { NULL } },                                // 0x19B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3GarbageIncineratorEffectSpriteDebrisTask, { NULL } },                      // 0x19C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, pepperSprayEffectTask, { NULL } },                                                  // 0x19D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, pepperSpraySparkTask, { NULL } },                                                   // 0x19E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mistShootingGalleryDrawLightGlowsTask, { NULL } },                                  // 0x19F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, combustionCastTask, { NULL } },                                                     // 0x1A0
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6GrowthRoomMistTask, { NULL } },                                            // 0x1A1
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6GrowthRoomDriftPuffTask, { NULL } },                                       // 0x1A2
    { { { TASK_BODY_TMD, TASK_PRIORITY_EFFECT } }, shelterB6NurseryDebrisChunkTask, { &gShelterB6NurseryModel07BAC } },                  // 0x1A3
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6NurseryAnimatedParticleTask, { NULL } },                                   // 0x1A4
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6NurserySparkShowerShardTask, { NULL } },                                   // 0x1A5
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1GolemFreezer1FloorMistTask, { NULL } },                                    // 0x1A6
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6TrainingRoomOrangeBurstTask, { NULL } },                                   // 0x1A7
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6TrainingRoomSummonRingTask, { NULL } },                                    // 0x1A8
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6TrainingRoomChargeBurstTask, { NULL } },                                   // 0x1A9
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6TrainingRoomRingBandTask, { NULL } },                                      // 0x1AA
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6TrainingRoomEnergyArcTask, { NULL } },                                     // 0x1AB
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6TrainingRoomHitFlashTask, { NULL } },                                      // 0x1AC
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6TrainingRoomSinkingSpriteTask, { NULL } },                                 // 0x1AD
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6TrainingRoomDescendingSpriteTask, { NULL } },                              // 0x1AE
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6TrainingRoomHealSpiralTask, { NULL } },                                    // 0x1AF
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMotelLoftFallingShardTask, { NULL } },                                 // 0x1B0
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMainStreetPuffTask, { NULL } },                                             // 0x1B1
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMainStreetPuffTask, { NULL } },                                        // 0x1B2
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodBottomEffectSpriteDriftTask, { NULL } },                                // 0x1B3
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1PodServiceGantrySpriteDriftTask, { NULL } },                               // 0x1B4
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodAccessTunnelEffectSpriteDriftTask, { NULL } },                          // 0x1B5
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldR08LampGlowTask, { NULL } },                                                // 0x1B6
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldR08SpriteDriftTask, { NULL } },                                             // 0x1B7
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldR08SpriteDriftTask, { NULL } },                                             // 0x1B8
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodBottomShockRingTask, { NULL } },                                        // 0x1B9
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodBottomArcFlashTask, { NULL } },                                         // 0x1BA
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodBottomEnergyRingTask, { NULL } },                                       // 0x1BB
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodBottomChargeBurstTask, { NULL } },                                      // 0x1BC
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mistShootingGalleryTracerTask, { NULL } },                                          // 0x1BD
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1PodServiceGantryWaterDriftTaskU16, { NULL } },                             // 0x1BE
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodBottomEffectSpriteRiseTask, { NULL } },                                 // 0x1BF
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodBottomLightBeamTask, { NULL } },                                        // 0x1C0
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkWoodlandPathLeafFallTask, { NULL } },                                         // 0x1C1
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fBulwarkRoomVisualEffectsFlashTask, { NULL } },                             // 0x1C2
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fBulwarkRoomVisualEffectsTwinTrailTask, { NULL } },                         // 0x1C3
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fBulwarkRoomVisualEffectsSparkBurstTask, { NULL } },                        // 0x1C4
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPyramidConfigureEffectsTask, { NULL } },                                      // 0x1C5
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1PodServiceGantryEffectSpriteRiseTask, { NULL } },                          // 0x1C6
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightR08RoomVisualEffectsFlashTask, { NULL } },                             // 0x1C7
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ElevatorHallRoomVisualEffectsFlashTask, { NULL } },                        // 0x1C8
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlashTask, { NULL } },             // 0x1C9
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1StoreroomRoomVisualEffectsFlashTask, { NULL } },                           // 0x1CA
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsFlashTask, { NULL } },             // 0x1CB
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1MainCorridorRoomVisualEffectsFlashTask, { NULL } },                        // 0x1CC
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1PodAccessTunnelRoomVisualEffectsFlashTask, { NULL } },                     // 0x1CD
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1TransferTunnelRoomVisualEffectsFlashTask, { NULL } },                      // 0x1CE
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlashTask, { NULL } },             // 0x1CF
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2ElevatorHallRoomVisualEffectsFlashTask, { NULL } },                        // 0x1D0
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlashTask, { NULL } },             // 0x1D1
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2NorthMaintenanceWalkwayRoomVisualEffectsFlashTask, { NULL } },             // 0x1D2
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2MainCorridorRoomVisualEffectsFlashTask, { NULL } },                        // 0x1D3
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2SepticTankRoomVisualEffectsFlashTask, { NULL } },                          // 0x1D4
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodAccessTunnelRoomVisualEffectsFlashTask, { NULL } },                     // 0x1D5
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fParkingGarageRoomVisualEffectsFlashTask, { NULL } },                       // 0x1D6
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fVehicularAirlockRoomVisualEffectsFlashTask, { NULL } },                    // 0x1D7
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkNorthPromenadeRoomVisualEffectsFlashTask, { NULL } },                         // 0x1D8
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkForestZoneRoomVisualEffectsFlashTask, { NULL } },                             // 0x1D9
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPavilionRoomVisualEffectsFlashTask, { NULL } },                               // 0x1DA
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkIslandRoomVisualEffectsFlashTask, { NULL } },                                 // 0x1DB
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPowerPlant2RoomVisualEffectsFlashTask, { NULL } },                            // 0x1DC
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSavannaZoneRoomVisualEffectsFlashTask, { NULL } },                            // 0x1DD
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSouthPromenadeRoomVisualEffectsFlashTask, { NULL } },                         // 0x1DE
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkShrineRoomVisualEffectsFlashTask, { NULL } },                                 // 0x1DF
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6NurseryRoomVisualEffectsFlashTask, { NULL } },                             // 0x1E0
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkBridgeRoomVisualEffectsFlashTask, { NULL } },                                 // 0x1E1
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPyramidRoomVisualEffectsFlashTask, { NULL } },                                // 0x1E2
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightR08RoomVisualEffectsTwinTrailTask, { NULL } },                         // 0x1E3
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ElevatorHallRoomVisualEffectsTwinTrailTask, { NULL } },                    // 0x1E4
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1SouthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask, { NULL } },         // 0x1E5
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1StoreroomRoomVisualEffectsTwinTrailTask, { NULL } },                       // 0x1E6
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask, { NULL } },         // 0x1E7
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1MainCorridorRoomVisualEffectsTwinTrailTask, { NULL } },                    // 0x1E8
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1PodAccessTunnelRoomVisualEffectsTwinTrailTask, { NULL } },                 // 0x1E9
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1TransferTunnelRoomVisualEffectsTwinTrailTask, { NULL } },                  // 0x1EA
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ControlRoomAccessTunnelRoomVisualEffectsTwinTrailTask, { NULL } },         // 0x1EB
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2ElevatorHallRoomVisualEffectsTwinTrailTask, { NULL } },                    // 0x1EC
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2SouthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask, { NULL } },         // 0x1ED
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2NorthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask, { NULL } },         // 0x1EE
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2MainCorridorRoomVisualEffectsTwinTrailTask, { NULL } },                    // 0x1EF
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2SepticTankRoomVisualEffectsTwinTrailTask, { NULL } },                      // 0x1F0
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodAccessTunnelRoomVisualEffectsTwinTrailTask, { NULL } },                 // 0x1F1
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fParkingGarageRoomVisualEffectsTwinTrailTask, { NULL } },                   // 0x1F2
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fVehicularAirlockRoomVisualEffectsTwinTrailTask, { NULL } },                // 0x1F3
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkNorthPromenadeRoomVisualEffectsTwinTrailTask, { NULL } },                     // 0x1F4
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkForestZoneRoomVisualEffectsTwinTrailTask, { NULL } },                         // 0x1F5
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPavilionRoomVisualEffectsTwinTrailTask, { NULL } },                           // 0x1F6
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkIslandRoomVisualEffectsTwinTrailTask, { NULL } },                             // 0x1F7
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPowerPlant2RoomVisualEffectsTwinTrailTask, { NULL } },                        // 0x1F8
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSavannaZoneRoomVisualEffectsTwinTrailTask, { NULL } },                        // 0x1F9
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSouthPromenadeRoomVisualEffectsTwinTrailTask, { NULL } },                     // 0x1FA
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkShrineRoomVisualEffectsTwinTrailTask, { NULL } },                             // 0x1FB
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6NurseryRoomVisualEffectsTwinTrailTask, { NULL } },                         // 0x1FC
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkBridgeRoomVisualEffectsTwinTrailTask, { NULL } },                             // 0x1FD
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPyramidRoomVisualEffectsTwinTrailTask, { NULL } },                            // 0x1FE
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightR08RoomVisualEffectsSparkBurstTask, { NULL } },                        // 0x1FF
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ElevatorHallRoomVisualEffectsSparkBurstTask, { NULL } },                   // 0x200
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1SouthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask, { NULL } },        // 0x201
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1StoreroomRoomVisualEffectsSparkBurstTask, { NULL } },                      // 0x202
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask, { NULL } },        // 0x203
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1MainCorridorRoomVisualEffectsSparkBurstTask, { NULL } },                   // 0x204
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1PodAccessTunnelRoomVisualEffectsSparkBurstTask, { NULL } },                // 0x205
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1TransferTunnelRoomVisualEffectsSparkBurstTask, { NULL } },                 // 0x206
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ControlRoomAccessTunnelRoomVisualEffectsSparkBurstTask, { NULL } },        // 0x207
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2ElevatorHallRoomVisualEffectsSparkBurstTask, { NULL } },                   // 0x208
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2SouthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask, { NULL } },        // 0x209
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2NorthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask, { NULL } },        // 0x20A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2MainCorridorRoomVisualEffectsSparkBurstTask, { NULL } },                   // 0x20B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2SepticTankRoomVisualEffectsSparkBurstTask, { NULL } },                     // 0x20C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodAccessTunnelRoomVisualEffectsSparkBurstTask, { NULL } },                // 0x20D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fParkingGarageRoomVisualEffectsSparkBurstTask, { NULL } },                  // 0x20E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelter1fVehicularAirlockRoomVisualEffectsSparkBurstTask, { NULL } },               // 0x20F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkNorthPromenadeRoomVisualEffectsSparkBurstTask, { NULL } },                    // 0x210
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkForestZoneRoomVisualEffectsSparkBurstTask, { NULL } },                        // 0x211
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPavilionRoomVisualEffectsSparkBurstTask, { NULL } },                          // 0x212
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkIslandRoomVisualEffectsSparkBurstTask, { NULL } },                            // 0x213
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPowerPlant2RoomVisualEffectsSparkBurstTask, { NULL } },                       // 0x214
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSavannaZoneRoomVisualEffectsSparkBurstTask, { NULL } },                       // 0x215
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSouthPromenadeRoomVisualEffectsSparkBurstTask, { NULL } },                    // 0x216
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkShrineRoomVisualEffectsSparkBurstTask, { NULL } },                            // 0x217
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6NurseryRoomVisualEffectsSparkBurstTask, { NULL } },                        // 0x218
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkBridgeRoomVisualEffectsSparkBurstTask, { NULL } },                            // 0x219
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPyramidRoomVisualEffectsSparkBurstTask, { NULL } },                           // 0x21A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask, { NULL } },       // 0x21B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1SouthMaintenanceWalkwayRoomVisualEffectsGlowDiscTask, { NULL } },          // 0x21C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask, { NULL } }, // 0x21D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1StoreroomRoomVisualEffectsGlowDiscTask, { NULL } },                        // 0x21E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsGlowDiscTask, { NULL } },          // 0x21F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1SleepingQuartersRoomVisualEffectsGlowDiscTask, { NULL } },                 // 0x220
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2SouthMaintenanceWalkwayRoomVisualEffectsGlowDiscTask, { NULL } },          // 0x221
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2OperatingRoomRoomVisualEffectsGlowDiscTask, { NULL } },                    // 0x222
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3ElevatorHallRoomVisualEffectsGlowDiscTask, { NULL } },                     // 0x223
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4UpperSewerRoomVisualEffectsGlowDiscTask, { NULL } },                       // 0x224
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4ReservoirRoomVisualEffectsGlowDiscTask, { NULL } },                        // 0x225
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4WaterSupplyRoomVisualEffectsGlowDiscTask, { NULL } },                      // 0x226
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPavilionRoomVisualEffectsGlowDiscTask, { NULL } },                            // 0x227
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkGardenRoomVisualEffectsGlowDiscTask, { NULL } },                              // 0x228
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1StoreroomRoomVisualEffectsFlyingSparkTask, { NULL } },                     // 0x229
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask, { NULL } },       // 0x22A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1SleepingQuartersRoomVisualEffectsFlyingSparkTask, { NULL } },              // 0x22B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask, { NULL } },       // 0x22C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2OperatingRoomRoomVisualEffectsFlyingSparkTask, { NULL } },                 // 0x22D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3ElevatorHallRoomVisualEffectsFlyingSparkTask, { NULL } },                  // 0x22E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4UpperSewerRoomVisualEffectsFlyingSparkTask, { NULL } },                    // 0x22F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4ReservoirRoomVisualEffectsFlyingSparkTask, { NULL } },                     // 0x230
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4WaterSupplyRoomVisualEffectsFlyingSparkTask, { NULL } },                   // 0x231
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPavilionRoomVisualEffectsFlyingSparkTask, { NULL } },                         // 0x232
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkGardenRoomVisualEffectsFlyingSparkTask, { NULL } },                           // 0x233
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1StoreroomRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },               // 0x234
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask, { NULL } }, // 0x235
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1SleepingQuartersRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },        // 0x236
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask, { NULL } }, // 0x237
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2OperatingRoomRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },           // 0x238
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3ElevatorHallRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },            // 0x239
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4UpperSewerRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },              // 0x23A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4ReservoirRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },               // 0x23B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4WaterSupplyRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },             // 0x23C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkPavilionRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },                   // 0x23D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkGardenRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },                     // 0x23E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineCavernRoomVisualEffectsHaloOrangeBurstTask, { NULL } },                         // 0x23F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineSecretPassageRoomVisualEffectsHaloOrangeBurstTask, { NULL } },                  // 0x240
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineSecretPassageRoomVisualEffectsHaloTask, { NULL } },                             // 0x241
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineSecretPassageRoomVisualEffectsMoteTask, { NULL } },                             // 0x242
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineSecretPassageRoomVisualEffectsSparkEmitterTask, { NULL } },                     // 0x243
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineCavernRoomVisualEffectsMoteTask, { NULL } },                                    // 0x244
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ElevatorHallRoomVisualEffectsMoteTask, { NULL } },                         // 0x245
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1StoreroomRoomVisualEffectsMoteTask, { NULL } },                            // 0x246
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsMoteTask, { NULL } },              // 0x247
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1MainCorridorRoomVisualEffectsMoteTask, { NULL } },                         // 0x248
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1TransferTunnelRoomVisualEffectsMoteTask, { NULL } },                       // 0x249
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2ElevatorHallRoomVisualEffectsMoteTask, { NULL } },                         // 0x24A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2NorthMaintenanceWalkwayRoomVisualEffectsMoteTask, { NULL } },              // 0x24B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodAccessTunnelRoomVisualEffectsMoteTask, { NULL } },                      // 0x24C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3ElevatorHallRoomVisualEffectsMoteTask, { NULL } },                         // 0x24D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4UpperSewerRoomVisualEffectsMoteTask, { NULL } },                           // 0x24E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkNorthPromenadeRoomVisualEffectsMoteTask, { NULL } },                          // 0x24F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineCavernRoomVisualEffectsHaloTask, { NULL } },                                    // 0x250
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ElevatorHallRoomVisualEffectsHaloTask, { NULL } },                         // 0x251
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1StoreroomRoomVisualEffectsHaloTask, { NULL } },                            // 0x252
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsHaloTask, { NULL } },              // 0x253
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1MainCorridorRoomVisualEffectsHaloTask, { NULL } },                         // 0x254
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1TransferTunnelRoomVisualEffectsHaloTask, { NULL } },                       // 0x255
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2ElevatorHallRoomVisualEffectsHaloTask, { NULL } },                         // 0x256
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2NorthMaintenanceWalkwayRoomVisualEffectsHaloTask, { NULL } },              // 0x257
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodAccessTunnelRoomVisualEffectsHaloTask, { NULL } },                      // 0x258
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3ElevatorHallRoomVisualEffectsHaloTask, { NULL } },                         // 0x259
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4UpperSewerRoomVisualEffectsHaloTask, { NULL } },                           // 0x25A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkNorthPromenadeRoomVisualEffectsHaloTask, { NULL } },                          // 0x25B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ElevatorHallRoomVisualEffectsHaloOrangeBurstTask, { NULL } },              // 0x25C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1StoreroomRoomVisualEffectsHaloOrangeBurstTask, { NULL } },                 // 0x25D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsHaloOrangeBurstTask, { NULL } },   // 0x25E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1MainCorridorRoomVisualEffectsHaloOrangeBurstTask, { NULL } },              // 0x25F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1TransferTunnelRoomVisualEffectsHaloOrangeBurstTask, { NULL } },            // 0x260
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2ElevatorHallRoomVisualEffectsHaloOrangeBurstTask, { NULL } },              // 0x261
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2NorthMaintenanceWalkwayRoomVisualEffectsHaloOrangeBurstTask, { NULL } },   // 0x262
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodAccessTunnelRoomVisualEffectsHaloOrangeBurstTask, { NULL } },           // 0x263
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3ElevatorHallRoomVisualEffectsHaloOrangeBurstTask, { NULL } },              // 0x264
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4UpperSewerRoomVisualEffectsHaloOrangeBurstTask, { NULL } },                // 0x265
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkNorthPromenadeRoomVisualEffectsHaloOrangeBurstTask, { NULL } },               // 0x266
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, mineCavernRoomVisualEffectsSparkEmitterTask, { NULL } },                            // 0x267
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ElevatorHallRoomVisualEffectsSparkEmitterTask, { NULL } },                 // 0x268
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1StoreroomRoomVisualEffectsSparkEmitterTask, { NULL } },                    // 0x269
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1NorthMaintenanceWalkwayRoomVisualEffectsSparkEmitterTask, { NULL } },      // 0x26A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1MainCorridorRoomVisualEffectsSparkEmitterTask, { NULL } },                 // 0x26B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1TransferTunnelRoomVisualEffectsSparkEmitterTask, { NULL } },               // 0x26C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2ElevatorHallRoomVisualEffectsSparkEmitterTask, { NULL } },                 // 0x26D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2NorthMaintenanceWalkwayRoomVisualEffectsSparkEmitterTask, { NULL } },      // 0x26E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2PodAccessTunnelRoomVisualEffectsSparkEmitterTask, { NULL } },              // 0x26F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB3ElevatorHallRoomVisualEffectsSparkEmitterTask, { NULL } },                 // 0x270
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB4UpperSewerRoomVisualEffectsSparkEmitterTask, { NULL } },                   // 0x271
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkNorthPromenadeRoomVisualEffectsSparkEmitterTask, { NULL } },                  // 0x272
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldDilapidatedHouseFireBlastTask, { NULL } },                                  // 0x273
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldDilapidatedHouseFlameConeTask, { NULL } },                                  // 0x274
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldDilapidatedHouseFlameRingTask, { NULL } },                                  // 0x275
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ControlRoomRoomVisualEffectsGlowDiscTask, { NULL } },                      // 0x276
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ControlRoomRoomVisualEffectsFlyingSparkTask, { NULL } },                   // 0x277
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ControlRoomRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },             // 0x278
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ControlRoomAccessTunnelRoomVisualEffectsGlowDiscTask, { NULL } },          // 0x279
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlyingSparkTask, { NULL } },       // 0x27A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlyingOrangeBurstTask, { NULL } }, // 0x27B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2BreedingRoomRoomVisualEffectsGlowDiscTask, { NULL } },                     // 0x27C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2BreedingRoomRoomVisualEffectsFlyingSparkTask, { NULL } },                  // 0x27D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB2BreedingRoomRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },            // 0x27E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSubmarineTunnelRoomVisualEffectsGlowDiscTask, { NULL } },                     // 0x27F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSubmarineTunnelRoomVisualEffectsFlyingSparkTask, { NULL } },                  // 0x280
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, neoArkSubmarineTunnelRoomVisualEffectsFlyingOrangeBurstTask, { NULL } },            // 0x281
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMotelBalconyRoomVisualEffectsMoteTask, { NULL } },                          // 0x282
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMotelBalconyRoomVisualEffectsHaloTask, { NULL } },                          // 0x283
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMotelBalconyRoomVisualEffectsHaloOrangeBurstTask, { NULL } },               // 0x284
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMotelBalconyRoomVisualEffectsSparkEmitterTask, { NULL } },                  // 0x285
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMainStreetRoomVisualEffectsMoteTask, { NULL } },                       // 0x286
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMainStreetRoomVisualEffectsHaloTask, { NULL } },                       // 0x287
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMainStreetRoomVisualEffectsHaloOrangeBurstTask, { NULL } },            // 0x288
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldNightMainStreetRoomVisualEffectsSparkEmitterTask, { NULL } },               // 0x289
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldToiletRoomVisualEffectsGlowDiscTask, { NULL } },                            // 0x28A
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldToiletFlyingSparkTask, { NULL } },                                          // 0x28B
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldToiletFlyingOrangeBurstTask, { NULL } },                                    // 0x28C
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisCafeteriaRoomVisualEffectsFlashTask, { NULL } },                           // 0x28D
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisCafeteriaRoomVisualEffectsTwinTrailTask, { NULL } },                       // 0x28E
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisCafeteriaRoomVisualEffectsSparkBurstTask, { NULL } },                      // 0x28F
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisForkedRoadRoomVisualEffectsFlashTask, { NULL } },                          // 0x290
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisForkedRoadRoomVisualEffectsTwinTrailTask, { NULL } },                      // 0x291
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, akropolisForkedRoadRoomVisualEffectsSparkBurstTask, { NULL } },                     // 0x292
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMainStreetRoomVisualEffectsFlashTask, { NULL } },                           // 0x293
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMainStreetRoomVisualEffectsTwinTrailTask, { NULL } },                       // 0x294
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldMainStreetRoomVisualEffectsSparkBurstTask, { NULL } },                      // 0x295
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldBackStreetRoomVisualEffectsFlashTask, { NULL } },                           // 0x296
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldBackStreetRoomVisualEffectsTwinTrailTask, { NULL } },                       // 0x297
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, dryfieldBackStreetRoomVisualEffectsSparkBurstTask, { NULL } },                      // 0x298
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, shelterB6CorridorPlayerHitGlowTask, { NULL } },                                     // 0x299
    { { { TASK_BODY_COORD, TASK_PRIORITY_EFFECT } }, gunbladeChargeFlashTask, { NULL } },                                                // 0x29A
};
/// Ambient-effect task types for Acropolis/MIST, indexed by area minus one.
///
/// Areas 1..20 are supported. Zero suppresses the ambient task; nonzero entries
/// select bank-6 descriptors and require the area's effect resources and callback
/// code to remain loaded. Values are task types, not packed effect IDs.
static const s32 _gRoomEffectAcropolisTaskTypes[20] __attribute__((section(".data"))) = {
    [GAME_AREA_AKROPOLIS_SQUARE - 1]                 = 0x02D,
    [GAME_AREA_AKROPOLIS_EAST_ELEVATOR_HALL - 1]     = 0x021,
    [GAME_AREA_AKROPOLIS_PATIO - 1]                  = 0x083,
    [GAME_AREA_AKROPOLIS_CAFETERIA - 1]              = 0x060,
    [GAME_AREA_AKROPOLIS_PLAZA - 1]                  = 0x04D,
    [GAME_AREA_AKROPOLIS_SECURITY_ROOM - 1]          = 0x048,
    [GAME_AREA_AKROPOLIS_HALLWAY - 1]                = 0x084,
    [GAME_AREA_AKROPOLIS_FOUNTAIN - 1]               = 0x026,
    [GAME_AREA_AKROPOLIS_FORKED_ROAD - 1]            = 0x085,
    [GAME_AREA_AKROPOLIS_OBSERVATORY - 1]            = 0x027,
    [GAME_AREA_AKROPOLIS_PROMENADE - 1]              = 0x04A,
    [GAME_AREA_AKROPOLIS_SANCTUARY - 1]              = 0x077,
    [GAME_AREA_AKROPOLIS_ROOF_GARDEN - 1]            = 0x086,
    [GAME_AREA_AKROPOLIS_BRIDGE - 1]                 = 0x0B0,
    [GAME_AREA_AKROPOLIS_FIRE_ESCAPE - 1]            = 0x04E,
    [GAME_AREA_AKROPOLIS_HELICOPTER_LANDING_PAD - 1] = 0x01D,
    [GAME_AREA_AKROPOLIS_WEST_ELEVATOR_HALL - 1]     = 0x01E,
    [GAME_AREA_MIST_R18 - 1]                         = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_MIST_PARKING - 1]                     = 0x039,
    [GAME_AREA_MIST_SHOOTING_GALLERY - 1]            = 0x19F,
};
/// Ambient-effect task types for daytime Dryfield, indexed by area minus one.
///
/// Areas 1..38 are supported. Zero suppresses the ambient task; nonzero entries
/// select bank-6 descriptors and require the area's effect resources and callback
/// code to remain loaded. Values are task types, not packed effect IDs.
static const s32 _gRoomEffectDryfieldTaskTypes[38] __attribute__((section(".data"))) = {
    [GAME_AREA_DRYFIELD_GAS_STATION - 1]       = 0x0C2,
    [GAME_AREA_DRYFIELD_MAIN_STREET - 1]       = 0x0C3,
    [GAME_AREA_DRYFIELD_GENERAL_STORE - 1]     = 0x0C4,
    [GAME_AREA_DRYFIELD_R04 - 1]               = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_DRYFIELD_BACK_STREET - 1]       = 0x0C5,
    [GAME_AREA_DRYFIELD_SOUVENIR_SHOP - 1]     = 0x0C6,
    [GAME_AREA_DRYFIELD_WAREHOUSE - 1]         = 0x0C7,
    [GAME_AREA_DRYFIELD_R08 - 1]               = 0x1B6,
    [GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE - 1] = 0x0C8,
    [9]                                        = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_1 - 1]      = 0x0C9,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_2 - 1]      = 0x0CA,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_3 - 1]      = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_4 - 1]      = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_DRYFIELD_PARKING_LOT - 1]       = 0x0CD,
    [GAME_AREA_DRYFIELD_TOILET - 1]            = 0x0CE,
    [GAME_AREA_DRYFIELD_MOTEL_LOBBY - 1]       = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_DRYFIELD_SALOON_G_R - 1]        = 0x0D0,
    [GAME_AREA_DRYFIELD_G_R_KITCHEN - 1]       = 0x0D1,
    [GAME_AREA_DRYFIELD_WATER_TOWER - 1]       = 0x0D2,
    [GAME_AREA_DRYFIELD_WATER_TANK - 1]        = 0x0D3,
    [GAME_AREA_DRYFIELD_BREEZEWAY - 1]         = 0x0D4,
    [GAME_AREA_DRYFIELD_FACTORY - 1]           = 0x0D5,
    [GAME_AREA_DRYFIELD_GARAGE - 1]            = 0x0D6,
    [GAME_AREA_DRYFIELD_DRIVEWAY - 1]          = 0x0D7,
    [GAME_AREA_DRYFIELD_JUNK_YARD - 1]         = 0x0D8,
    [GAME_AREA_DRYFIELD_TRAILER_COACH - 1]     = 0x0D9,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_5 - 1]      = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY - 1]     = 0x0DB,
    [GAME_AREA_DRYFIELD_MOTEL_ROOM_6 - 1]      = 0x0DC,
    [GAME_AREA_DRYFIELD_MOTEL_LOFT - 1]        = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_DRYFIELD_WATER_HOLE - 1]        = 0x0DE,
    [32]                                       = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_DRYFIELD_CELLAR - 1]            = 0x0DF,
    [34]                                       = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [35]                                       = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [36]                                       = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_DRYFIELD_UNDERPASS - 1]         = 0x0BF,
};
/// Ambient-effect task types for nighttime Dryfield, indexed by area minus one.
///
/// Areas 1..38 are supported. Zero suppresses the ambient task; nonzero entries
/// select bank-6 descriptors and require the area's effect resources and callback
/// code to remain loaded. Values are task types, not packed effect IDs.
static const s32 _gRoomEffectDryfieldNightTaskTypes[38] __attribute__((section(".data"))) = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION - 1]       = 0x100,
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET - 1]       = 0x101,
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE - 1]     = 0x102,
    [3]                                              = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET - 1]       = 0x103,
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP - 1]     = 0x104,
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE - 1]         = 0x105,
    [GAME_AREA_DRYFIELD_NIGHT_R08 - 1]               = 0x17E,
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE - 1] = 0x106,
    [9]                                              = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_1 - 1]      = 0x107,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2 - 1]      = 0x108,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_3 - 1]      = 0x109,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_4 - 1]      = 0x10A,
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT - 1]       = 0x10B,
    [GAME_AREA_DRYFIELD_NIGHT_TOILET - 1]            = 0x10C,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY - 1]       = 0x10D,
    [GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R - 1]        = 0x10E,
    [GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN - 1]       = 0x10F,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER - 1]       = 0x110,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TANK - 1]        = 0x111,
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY - 1]         = 0x112,
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY - 1]           = 0x113,
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE - 1]            = 0x114,
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY - 1]          = 0x115,
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD - 1]         = 0x116,
    [GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH - 1]     = 0x117,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5 - 1]      = 0x118,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY - 1]     = 0x119,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6 - 1]      = 0x11A,
    [GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT - 1]        = 0x11B,
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE - 1]        = 0x11C,
    [32]                                             = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_DRYFIELD_NIGHT_CELLAR - 1]            = 0x11D,
    [34]                                             = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [35]                                             = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [36]                                             = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS - 1]         = 0x11E,
};
/// Ambient-effect task types for Mine/Shelter, indexed by area minus one.
///
/// Areas 1..49 are supported. Zero suppresses the ambient task; nonzero entries
/// select bank-6 descriptors and require the area's effect resources and callback
/// code to remain loaded. Values are task types, not packed effect IDs.
static const s32 _gRoomEffectMineShelterTaskTypes[49] __attribute__((section(".data"))) = {
    [GAME_AREA_MINE_MESA - 1]                             = 0x120,
    [GAME_AREA_MINE_CAVERN - 1]                           = 0x121,
    [GAME_AREA_MINE_TUNNEL_ENTRANCE - 1]                  = 0x122,
    [GAME_AREA_MINE_TUNNEL - 1]                           = 0x123,
    [GAME_AREA_MINE_GORGE - 1]                            = 0x124,
    [GAME_AREA_MINE_REFUGE - 1]                           = 0x125,
    [GAME_AREA_MINE_FORKED_TUNNEL - 1]                    = 0x126,
    [GAME_AREA_MINE_SECRET_PASSAGE - 1]                   = 0x127,
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL - 1]              = 0x128,
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY - 1]  = 0x129,
    [GAME_AREA_SHELTER_B1_STOREROOM - 1]                  = 0x12A,
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY - 1]  = 0x12B,
    [GAME_AREA_SHELTER_B1_ARMORY - 1]                     = 0x12C,
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS - 1]          = 0x12D,
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR - 1]              = 0x12E,
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM - 1]         = 0x12F,
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL - 1]          = 0x130,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM - 1]               = 0x131,
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL - 1]              = 0x132,
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING - 1]        = 0x133,
    [GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 - 1]            = 0x134,
    [GAME_AREA_SHELTER_B2_POD_BOTTOM - 1]                 = 0x135,
    [GAME_AREA_SHELTER_B1_POD_SERVICE_GANTRY - 1]         = 0x136,
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL - 1]            = 0x137,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL - 1] = 0x138,
    [GAME_AREA_SHELTER_B2_ELEVATOR - 1]                   = 0x139,
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL - 1]              = 0x13A,
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY - 1]  = 0x13B,
    [GAME_AREA_SHELTER_B2_OPERATING_ROOM - 1]             = 0x13C,
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY - 1]  = 0x13D,
    [GAME_AREA_SHELTER_B2_LABORATORY - 1]                 = 0x13E,
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM - 1]              = 0x13F,
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR - 1]              = 0x140,
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK - 1]                = 0x141,
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL - 1]          = 0x142,
    [GAME_AREA_SHELTER_R36 - 1]                           = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_SHELTER_R37 - 1]                           = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_SHELTER_1F_HELIPORT_S4 - 1]                = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_SHELTER_B3_DUMPING_HOLE - 1]               = 0x143,
    [GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR - 1]        = 0x144,
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM - 1]   = 0x145,
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL - 1]              = 0x146,
    [GAME_AREA_SHELTER_B4_LOWER_SEWER - 1]                = 0x147,
    [GAME_AREA_SHELTER_B4_UPPER_SEWER - 1]                = 0x148,
    [GAME_AREA_SHELTER_B4_RESERVOIR - 1]                  = 0x149,
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY - 1]               = 0x14A,
    [GAME_AREA_SHELTER_R47 - 1]                           = 0x14B,
    [GAME_AREA_SHELTER_R48 - 1]                           = 0x14C,
    [GAME_AREA_SHELTER_R49 - 1]                           = 0x162,
};
/// Ambient-effect task types for the Shelter/Neo Ark stage, indexed by area minus one.
///
/// Areas 1..33 are supported. A zero entry suppresses the area's ambient task;
/// nonzero entries select bank-6 descriptors and require that area's effect
/// resources and callbacks to be loaded. These are task types, not packed IDs.
static const s32 _gRoomEffectShelterNeoArkTaskTypes[33] __attribute__((section(".data"))) = {
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE - 1]    = 0x163,
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK - 1] = 0x164,
    [GAME_AREA_SHELTER_1F_BULWARK - 1]           = 0x165,
    [GAME_AREA_SHELTER_1F_HELIPORT - 1]          = 0x166,
    [GAME_AREA_SHELTER_1F_AIRLOCK - 1]           = 0x14D,
    [GAME_AREA_SHELTER_1F_GUARDROOM - 1]         = 0x167,
    [GAME_AREA_NEO_ARK_OBSERVATORY - 1]          = 0x14E,
    [GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL - 1]    = 0x14F,
    [GAME_AREA_NEO_ARK_EVE_ELEVATOR - 1]         = 0x150,
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE - 1]      = 0x151,
    [GAME_AREA_NEO_ARK_FOREST_ZONE - 1]          = 0x0FA,
    [GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL - 1]     = 0x152,
    [GAME_AREA_NEO_ARK_PAVILION - 1]             = 0x153,
    [GAME_AREA_NEO_ARK_ISLAND - 1]               = 0x154,
    [GAME_AREA_NEO_ARK_GARDEN - 1]               = 0x155,
    [GAME_AREA_NEO_ARK_POWER_PLANT_2 - 1]        = 0x156,
    [GAME_AREA_NEO_ARK_POWER_PLANT_1 - 1]        = 0x157,
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE - 1]         = 0x158,
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE - 1]      = 0x159,
    [GAME_AREA_NEO_ARK_ALTAR - 1]                = 0x15A,
    [GAME_AREA_NEO_ARK_SHRINE - 1]               = 0x15B,
    [GAME_AREA_SHELTER_B6_NURSERY - 1]           = 0x15C,
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM - 1]       = 0x15D,
    [GAME_AREA_SHELTER_B6_CORRIDOR - 1]          = 0x15E,
    [GAME_AREA_SHELTER_B6_TRAINING_ROOM - 1]     = 0x15F,
    [GAME_AREA_NEO_ARK_R26 - 1]                  = 0x168,
    [GAME_AREA_NEO_ARK_BRIDGE - 1]               = 0x160,
    [GAME_AREA_SHELTER_1F_TENT - 1]              = 0x169,
    [GAME_AREA_NEO_ARK_WOODLAND_PATH - 1]        = 0x161,
    [GAME_AREA_NEO_ARK_SUBMARINE_GALLERY - 1]    = 0x192,
    [GAME_AREA_NEO_ARK_R31 - 1]                  = ROOM_EFFECT_AMBIENT_TASK_NONE,
    [GAME_AREA_NEO_ARK_PYRAMID - 1]              = 0x1C5,
    [GAME_AREA_NEO_ARK_SUBSTATION - 1]           = 0x038,
};

/// Room-controller states: initialize, update scene effects, then kill.
///
/// The controller copies the table and selects Task::state in 0..2.
/// Initialization advances to update; update holds state 1, and state 2
/// releases the task and its owned effect state.
static const TaskFuncTable3 _gRoomEffectControllerStates = { {
    _roomEffectInitController,
    _roomEffectTickController,
    taskKill,
} };

/// Allocates and publishes the room controller's state and starts its initial effects.
///
/// Enter with state zero and a valid one-based area in the selected stage's
/// ambient-effect table. The controller owns the primary-heap state through
/// `Task::work`; users borrow `gRoomEffectState` until teardown. Allocation
/// failure kills the task without publishing a new state. Effect-spawn failures
/// are ignored. Success clears the room's effect bindings and transient lights,
/// starts the player shadow, the area's ambient task and the PE dispatcher,
/// then leaves the controller in its per-update state.
static void _roomEffectInitController(Task* task)
{
    // This direct task selector retains bit 31; descriptor-stride wrapping
    // selects slot 7. It is not an effect-limit option at the task API.
    enum { ROOM_EFFECT_PE_DISPATCH_SELECTOR = (s32)0x80000007 };
    RoomEffectState* effectState;
    s32              roomEffectType;

    roomEffectType = ROOM_EFFECT_AMBIENT_TASK_NONE;
    effectState    = memCalloc(sizeof(*effectState), false);
    if (effectState == NULL) {
        taskKill(task);
        return;
    }

    // Publish the allocation for the controller's lifetime.
    _gRoomEffectTask                = task;
    gRoomEffectState                = effectState;
    task->work                      = effectState;
    effectState->effectCount        = 0;
    effectState->rumbleCount        = 0;
    effectState->effectControl      = ROOM_EFFECT_CONTROL_RUNNING;
    effectState->groundTraceEnabled = true;
    effectState->groundShadowShade  = ROOM_EFFECT_GROUND_SHADOW_UNMODULATED;
    effectSpawn(EFFECT_PLAYER_GROUND_SHADOW, NULL, 0, NULL);

    // Each newly loaded room supplies its own effect bindings after this reset.
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
    task->state++;
    _worldCoordInitTransientPointLights();

    // The stage tables contain bank-6 types, with zero disabling ambient effects.
    switch (gGameSession->location.stage) {
        case GAME_STAGE_AKROPOLIS:
            roomEffectType = _gRoomEffectAcropolisTaskTypes[gGameSession->location.area - 1];
            break;
        case GAME_STAGE_DRYFIELD:
            roomEffectType = _gRoomEffectDryfieldTaskTypes[gGameSession->location.area - 1];
            break;
        case GAME_STAGE_DRYFIELD_NIGHT:
            roomEffectType = _gRoomEffectDryfieldNightTaskTypes[gGameSession->location.area - 1];
            break;
        case GAME_STAGE_MINE_SHELTER:
            roomEffectType = _gRoomEffectMineShelterTaskTypes[gGameSession->location.area - 1];
            break;
        case GAME_STAGE_SHELTER_NEO_ARK:
            roomEffectType = _gRoomEffectShelterNeoArkTaskTypes[gGameSession->location.area - 1];
            break;
    }

    if (roomEffectType != ROOM_EFFECT_AMBIENT_TASK_NONE) {
        effectSpawn(EFFECT_ID(EFFECT_TASK_BANK, roomEffectType), NULL, 0, NULL);
    }
    taskSpawn(EFFECT_TASK_BANK, ROOM_EFFECT_PE_DISPATCH_SELECTOR, 0, 0);
}

/// Clears Parasite Energy buffs when the room controller cancels PE.
///
/// Requires writable live attachment/player state. Clears antibody, energy-shot
/// and metabolism timers and combos, both immunity wards and the Berserker
/// status flag. Cast selection/phase and the player's status timers survive;
/// the effect tasks consume the controller's cancellation separately.
static inline void _roomEffectClearCancelledPeStatus(void)
{
    AttachmentState* attachment;

    attachment                  = &gAttachmentState;
    attachment->antibodyTicks   = 0;
    attachment->antibodyCombo   = 0;
    attachment->energyShotTicks = 0;
    attachment->energyShotCombo = 0;
    attachment->metabolismTicks = 0;
    attachment->metabolismCombo = 0;
    attachment->mindWard        = 0;
    attachment->bodyWard        = 0;
    playerStateSetStatusEffects(true, PLAYER_STATUS_BERSERKER);
}

/// Publishes scene control and one-update cancellation, and ages transient lights.
///
/// Requires the controller's live `gRoomEffectState`. Clamps effect/rumble counts
/// at zero and stops the shared death-flame sound when leaving the engaged phase.
/// Pending ALL cancellation reaches both control words; PE cancellation reaches only PE.
/// Consuming the flags clears them for the next update. Paused control freezes
/// transient-light aging; PE cancellation clears temporary attachment wards and
/// timers plus the player's Berserker status. The task argument is ignored.
static void _roomEffectTickController(Task* unusedTask)
{
    enum { ROOM_EFFECT_DEATH_FLAME_SOUND = SOUND_COMMON(0x0D) };

    RoomEffectState*  effectState;
    SceneCombatState* combat;
    s16               previousBattleState;

    if (gRoomEffectState->effectCount <= 0) {
        gRoomEffectState->effectCount = 0;
    }
    if (gRoomEffectState->rumbleCount <= 0) {
        gRoomEffectState->rumbleCount = 0;
    }
    previousBattleState = gRoomEffectState->battleState;
    if ((previousBattleState == ROOM_EFFECT_BATTLE_ENGAGED) && (gSceneCombatState.signals.bytes.battlePhase != previousBattleState)) {
        sndEvtRequestScriptStop(ROOM_EFFECT_DEATH_FLAME_SOUND | SOUND_SCRIPT_STOP_ALL_INSTANCES, SOUND_SCRIPT_STOP_KEEP_RELEASE);
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
        _roomEffectClearCancelledPeStatus();
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

/// Dispatches the room controller's initialization, update or teardown state.
///
/// Bank-6 slot 4 has no body or spawn payload. `task->state` must be 0..2:
/// zero allocates the shared state, one publishes each update, and two releases
/// the task and its owned state. The dispatch table is copied before selection.
static void _roomEffectControllerTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gRoomEffectControllerStates;
    states.funcs[task->state](task);
}

EffectWork* effectSpawn(s32 effectId, GfxCoord* parentCoord, TaskSpawnArg spawnArg, const SVECTOR* offset)
{
    enum {
        EFFECT_SPAWN_BANK_SHIFT = 16,
        EFFECT_SPAWN_BANK_MASK  = 0x7FFF,
        EFFECT_SPAWN_TYPE_MASK  = 0xFFFF,
    };
    Task*       task;
    EffectWork* work;
    s32         taskBank;

/// Retains the borrowed spawn offset and snapshots its XYZ for effect placement.
///
/// effectWork is writable EffectWork*, spawnOffset a writable const SVECTOR*
/// local, and zeroVector a disjoint writable SVECTOR*. Arguments are evaluated
/// repeatedly and must have no side effects. Clears the complete zero vector,
/// including its pad, before retaining the original offset pointer (even NULL).
/// Only a NULL offset redirects the local to zeroVector; that temporary must
/// remain live through the caller's GTE placement. Copies signed-halfword XYZ
/// without a frame conversion and leaves pos.pad alone. No input vector is
/// retained through the temporary; a non-NULL original pointer stays borrowed.
/// Captures no caller locals; expands to statements inside a braced block.
#define EFFECT_INIT_SPAWN_OFFSET(effectWork, spawnOffset, zeroVector) \
    memset((zeroVector), 0, sizeof(*(zeroVector)));                   \
    (effectWork)->field_C = (spawnOffset);                            \
    if ((spawnOffset) == NULL) {                                      \
        (spawnOffset) = (zeroVector);                                 \
    }                                                                 \
    (effectWork)->pos.vx = (spawnOffset)->vx;                         \
    (effectWork)->pos.vy = (spawnOffset)->vy;                         \
    (effectWork)->pos.vz = (spawnOffset)->vz;

    taskBank = (effectId >> EFFECT_SPAWN_BANK_SHIFT) & EFFECT_SPAWN_BANK_MASK;
    if ((effectId >= 0) && (gRoomEffectState->effectCount >= ROOM_EFFECT_NORMAL_SPAWN_LIMIT)) {
        return NULL;
    }
    effectId &= EFFECT_SPAWN_TYPE_MASK;
    if (effectId == 0) {
        return NULL;
    }
    task = taskSpawn(taskBank, effectId, spawnArg, 0);
    if (task == NULL) {
        return NULL;
    }
    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return NULL;
    }
    gRoomEffectState->effectCount++;

    // Snapshot placement while retaining the original, borrowed spawn pointers.
    if (parentCoord != NULL) {
        GfxCoord* effectCoord;
        SVECTOR   zeroOffset;

        // Both supported body kinds share this coordinate-pointer offset.
        effectCoord = task->extra.tmd->coords;
        EFFECT_INIT_SPAWN_OFFSET(work, offset, &zeroOffset);
        if (parentCoord->parent == &gGfxViewCoord) {
            effectCoord->coord = parentCoord->coord;
            gte_SetRotMatrix(&parentCoord->coord);
            gte_SetTransMatrix(&parentCoord->coord);
            gte_ldv0(offset);
            gte_rtv0tr();
            gte_stlvnl(effectCoord->coord.t);
        } else {
            // Convert a composed placement back into the view parent's local space.
            actorRenderComposeCoord(parentCoord);
            effectCoord->workm = parentCoord->workm;
            gte_SetRotMatrix(&parentCoord->workm);
            gte_SetTransMatrix(&parentCoord->workm);
            gte_ldv0(offset);
            gte_rtv0tr();
            gte_stlvnl(effectCoord->workm.t);
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &effectCoord->workm, &effectCoord->coord);
        }
        effectCoord->parent       = &gGfxViewCoord;
        effectCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(effectCoord);
        work->parent = parentCoord;
    } else {
        GfxCoord* effectCoord;
        SVECTOR   zeroOffset;

        effectCoord = task->extra.tmd->coords;
        EFFECT_INIT_SPAWN_OFFSET(work, offset, &zeroOffset);
        // With no placement coordinate, the offset uses the SDK matrix's input space.
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(offset);
        gte_rtv0tr();
        gte_stlvnl(effectCoord->coord.t);
        effectCoord->parent       = &gGfxViewCoord;
        effectCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(effectCoord);
        work->parent = &gGfxViewCoord;
    }

    // The exit callback owns the counted allocation; the returned pointer borrows it.
    task->spawnArg2.pointer = work;
    task->exitCallback      = _effectExitTask;
    work->task              = task;
    work->index             = 0;
    work->age               = 0;
    work->scale             = 0;
    work->angle             = 0;
    work->period            = 0;
    work->step              = 0;
    work->field_4           = 0;
    work->move.vx           = 0;
    work->move.vy           = 0;
    work->move.vz           = 0;
    return work;

#undef EFFECT_INIT_SPAWN_OFFSET
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
        GPU_EFFECT_DRAW_TO_DISPLAY_DISABLED = 0,
        GPU_EFFECT_DITHER_ENABLED           = 1,
        GPU_EFFECT_TEXTURE_PAGE_Y           = 0,
    };
    DR_TPAGE* blendCommand;

    setSemiTrans(primitive, true);
    blendCommand   = gGpuPrimCursor;
    gGpuPrimCursor = blendCommand + 1;
    setDrawTPage(blendCommand, GPU_EFFECT_DRAW_TO_DISPLAY_DISABLED, GPU_EFFECT_DITHER_ENABLED,
                 getTPage(GPU_EFFECT_TEXTURE_DEPTH_4BIT, blendMode, GPU_EFFECT_TEXTURE_PAGE_X, GPU_EFFECT_TEXTURE_PAGE_Y));
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(sortingDepth),
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
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(EFFECT_SCREEN_SORTING_DEPTH), quad);

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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
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
        setClut(quad, ((u16*)gEffectBillboardClutX)[paletteIndex], EFFECT_BILLBOARD_CLUT_Y);
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
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
        setClut(quad, ((u16*)gEffectModulatedBillboardClutX)[paletteIndex], EFFECT_MODULATED_BILLBOARD_CLUT_Y);
        leftU  = textureBank * EFFECT_MODULATED_BILLBOARD_BANK_U_STRIDE + (frame & (EFFECT_MODULATED_BILLBOARD_FRAME_COUNT - 1)) * EFFECT_MODULATED_BILLBOARD_CELL_SIZE;
        rightU = leftU + EFFECT_MODULATED_BILLBOARD_UV_SPAN;
        setUV4(quad, leftU, 0, rightU, 0, leftU, EFFECT_MODULATED_BILLBOARD_UV_SPAN, rightU, EFFECT_MODULATED_BILLBOARD_UV_SPAN);
        scratch->screenExtent = (packedSizeBank * EFFECT_MODULATED_BILLBOARD_UV_SPAN) / scratch->depth;
        _effectSetBillboardScreenBounds(quad, scratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
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
                randomState       = RANDOM_LCG_NEXT(gRandomLcgState);
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
        packedTint = gEffectStatusScreenTints[(cln(task->spawnArg1.value << EFFECT_STATUS_TINT_LOG_FRACTION_BITS) / EFFECT_STATUS_TINT_LOG_TWO) & EFFECT_STATUS_TINT_COLOR_INDEX_MASK];
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

void roomEffectRequestCancelAll(void)
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
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(sortingDepth),
            blendCommand);
}

void gpuSetPrimitiveBlendMode(void* primitive, s32 blendMode, s32 depth)
{
    setSemiTrans(primitive, 1);
    _gpuQueueBlendMode(blendMode, depth);
}

void roomEffectStartDarknessDim(void)
{
    if (!(gRoomEffectState->screenFxFlags & ROOM_EFFECT_SCREEN_FADE_QUAD)) {
        effectSpawn((EFFECT_DARKNESS_SCREEN_DIM | EFFECT_SPAWN_UNLIMITED), NULL, 0, NULL);
    }
}

void roomEffectStartStatusTint(s32 statusMask)
{
    gRoomEffectState->peFadeMask = (u8)statusMask;
    effectSpawn((EFFECT_STATUS_AILMENT_SCREEN_TINT | EFFECT_SPAWN_UNLIMITED), NULL, (s32)((u8)statusMask), NULL);
}

void roomEffectStartBerserkerGlow(void)
{
    RoomEffectState* effectState;

    effectState = gRoomEffectState;
    if (!(effectState->screenFxFlags & ROOM_EFFECT_SCREEN_BURST_GUARD)) {
        // Withdraw the shared claim before requesting a fresh glow task.
        effectState->peFxFlags &= (u16)~ROOM_EFFECT_PE_STATUS_BURST;
        effectSpawn((EFFECT_BERSERKER_SHOT_GLOW | EFFECT_SPAWN_UNLIMITED), NULL, 0, NULL);
    }
}
