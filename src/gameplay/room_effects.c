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

/// Grayscale fade task controlled by `gPlayerStatus.statusFlags` bit 0.
/// Alternates LCG-selected brightness targets, then fades out and releases
/// its `EffectWork` when the flag stays clear.
void func_800EC47C(Task* arg0);

static void Gp_InitState1C(Task* arg0);

static void Gp_TickState1C(Task* unused);

static void Gp_DecRoomCoordRefs(void);

static void Gp_InitRoomCoords(void);

void func_800EA420(Task* arg0);

void Gp_FadeWaveTask(Task* arg0);

static void Gp_KillState1CTask(Task* arg0);

static void Gp_AddTpage(P_TAG* arg0, s32 arg1, s32 arg2);

// Retained effect slots without a proven owning room. See the local type audit.
void func_8018345C(Task* task);

void func_8017FAAC(Task* task);

void func_8011D1E0(Task* task);

/// Task bank 6: effects and loaded-overlay task entry points.
TaskDesc D_8010FC2C[667] = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { NULL } },                                                   // 0x000
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { NULL } },                                                   // 0x001
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { NULL } },                                                   // 0x002
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { NULL } },                                                   // 0x003
    { { { TASK_BODY_NONE, 0x4F } }, func_800EA420, { NULL } },                                              // 0x004
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_motel_balcony_801802DC, { NULL } },                      // 0x005
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_gas_station_80181D80, { NULL } },                  // 0x006
    { { { TASK_BODY_NONE, 0x70 } }, Gp_EffCtlTask07, { NULL } },                                            // 0x007
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_gas_station_801827E4, { NULL } },                  // 0x008
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_gas_station_801830CC, { NULL } },                  // 0x009
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_back_street_8017E390, { NULL } },                  // 0x00A
    { { { TASK_BODY_COORD, 0x70 } }, func_hypervelocity_8011F270, { NULL } },                               // 0x00B
    { { { TASK_BODY_COORD, 0x70 } }, func_hypervelocity_8011D830, { NULL } },                               // 0x00C
    { { { TASK_BODY_COORD, 0x70 } }, func_hypervelocity_8011F168, { NULL } },                               // 0x00D
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask0E, { NULL } },                                           // 0x00E
    { { { TASK_BODY_COORD, 0x70 } }, Gp_FadeWaveTask, { NULL } },                                           // 0x00F
    { { { TASK_BODY_COORD, 0x70 } }, func_pyrokinesis_8012EF48, { NULL } },                                 // 0x010
    { { { TASK_BODY_COORD, 0x70 } }, func_pyrokinesis_80131CE4, { NULL } },                                 // 0x011
    { { { TASK_BODY_COORD, 0x70 } }, func_metabolism_8012EF34, { NULL } },                                  // 0x012
    { { { TASK_BODY_COORD, 0x70 } }, func_metabolism_8012F5A0, { NULL } },                                  // 0x013
    { { { TASK_BODY_COORD, 0x70 } }, func_plasma_8012EF34, { NULL } },                                      // 0x014
    { { { TASK_BODY_COORD, 0x70 } }, func_healing_8012EF34, { NULL } },                                     // 0x015
    { { { TASK_BODY_COORD, 0x70 } }, func_healing_8012F494, { NULL } },                                     // 0x016
    { { { TASK_BODY_COORD, 0x70 } }, func_healing_8012F5E4, { NULL } },                                     // 0x017
    { { { TASK_BODY_COORD, 0x70 } }, func_necrosis_8012EF34, { NULL } },                                    // 0x018
    { { { TASK_BODY_COORD, 0x70 } }, func_necrosis_8012F52C, { NULL } },                                    // 0x019
    { { { TASK_BODY_COORD, 0x70 } }, func_necrosis_8012FAF8, { NULL } },                                    // 0x01A
    { { { TASK_BODY_COORD, 0x70 } }, func_combustion_8012EF34, { NULL } },                                  // 0x01B
    { { { TASK_BODY_COORD, 0x70 } }, func_combustion_8012F2BC, { NULL } },                                  // 0x01C
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_helicopter_landing_pad_801818F0, { NULL } },            // 0x01D
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_west_elevator_hall_8017F7D4, { NULL } },                // 0x01E
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_west_elevator_hall_8017FAE8, { NULL } },                // 0x01F
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_west_elevator_hall_8017FE18, { NULL } },                // 0x020
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_east_elevator_hall_8017F5B4, { NULL } },                // 0x021
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_east_elevator_hall_8017F77C, { NULL } },                // 0x022
    { { { TASK_BODY_COORD, 0x70 } }, func_8017FAAC, { NULL } },                                             // 0x023
    { { { TASK_BODY_COORD, 0x70 } }, func_hypervelocity_8011D1E8, { NULL } },                               // 0x024
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_west_elevator_hall_8017FFE4, { NULL } },                // 0x025
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_fountain_8017E014, { NULL } },                          // 0x026
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_observatory_8017E6F8, { NULL } },                       // 0x027
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_observatory_8017E424, { NULL } },                       // 0x028
    { { { TASK_BODY_COORD, 0x70 } }, func_m4a1_hammer_8011D1E0, { NULL } },                                 // 0x029
    { { { TASK_BODY_COORD, 0x70 } }, func_m4a1_pyke_8011D1F8, { NULL } },                                   // 0x02A
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask2B, { NULL } },                                           // 0x02B
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { NULL } },                                                   // 0x02C
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_square_801823DC, { NULL } },                            // 0x02D
    { { { TASK_BODY_COORD, 0x70 } }, func_8018345C, { NULL } },                                             // 0x02E
    { { { TASK_BODY_COORD, 0x70 } }, func_m4a1_javelin_8011D1E4, { NULL } },                                // 0x02F
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask30, { NULL } },                                           // 0x030
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill, { NULL } },                                                   // 0x031
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask32, { NULL } },                                           // 0x032
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_west_elevator_hall_8017F990, { NULL } },                // 0x033
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask34, { NULL } },                                           // 0x034
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask35, { NULL } },                                           // 0x035
    { { { TASK_BODY_TMD, 0x70 } }, Gp_EffModelTask, { &D_80111FC8 } },                                      // 0x036
    { { { TASK_BODY_TMD, 0x70 } }, Gp_EffAttachTask37, { &D_8011231C } },                                   // 0x037
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_substation_8017D874, { NULL } },                          // 0x038
    { { { TASK_BODY_COORD, 0x70 } }, func_mist_parking_80184728, { NULL } },                                // 0x039
    { { { TASK_BODY_COORD, 0x70 } }, func_tonfa_baton_8011D1EC, { NULL } },                                 // 0x03A
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask3B, { NULL } },                                           // 0x03B
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_breezeway_80181264, { NULL } },                          // 0x03C
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_balcony_8017F84C, { NULL } },                // 0x03D
    { { { TASK_BODY_COORD, 0x70 } }, func_m4a1_bayonet_8011D1E4, { NULL } },                                // 0x03E
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask3F, { NULL } },                                           // 0x03F
    { { { TASK_BODY_COORD, 0x70 } }, func_p229_8011D1DC, { NULL } },                                        // 0x040
    { { { TASK_BODY_COORD, 0x70 } }, func_mp5a5_8011D1E0, { NULL } },                                       // 0x041
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask42, { NULL } },                                           // 0x042
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_510900_80131F24, { NULL } },                                // 0x043
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_510900_801340E8, { NULL } },                                // 0x044
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_510900_80132D4C, { NULL } },                                // 0x045
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask46, { NULL } },                                           // 0x046
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_square_801825DC, { NULL } },                            // 0x047
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_security_room_801805A4, { NULL } },                     // 0x048
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_security_room_80180E34, { NULL } },                     // 0x049
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_promenade_8017E03C, { NULL } },                         // 0x04A
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_promenade_8017E634, { NULL } },                         // 0x04B
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_510900_801332EC, { NULL } },                                // 0x04C
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_plaza_8018251C, { NULL } },                             // 0x04D
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_fire_escape_8017FF7C, { NULL } },                       // 0x04E
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_fire_escape_80180B20, { NULL } },                       // 0x04F
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_balcony_80180580, { NULL } },                // 0x050
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_forked_road_8017E81C, { NULL } },                       // 0x051
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_510900_8013371C, { NULL } },                                // 0x052
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask53, { NULL } },                                           // 0x053
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask54, { NULL } },                                           // 0x054
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask55, { NULL } },                                           // 0x055
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_promenade_8017E394, { NULL } },                         // 0x056
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_promenade_8017ED44, { NULL } },                         // 0x057
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_woodland_path_8017F4A0, { NULL } },                       // 0x058
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_510900_80133C84, { NULL } },                                // 0x059
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_helicopter_landing_pad_8017FA30, { NULL } },            // 0x05A
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_helicopter_landing_pad_801802E0, { NULL } },            // 0x05B
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask5C, { NULL } },                                           // 0x05C
    { { { TASK_BODY_NONE, 0x70 } }, taskKill, { NULL } },                                                   // 0x05D
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_helicopter_landing_pad_80181064, { NULL } },            // 0x05E
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_helicopter_landing_pad_80180E40, { NULL } },            // 0x05F
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_cafeteria_8017E708, { NULL } },                         // 0x060
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_cafeteria_8017EA90, { NULL } },                         // 0x061
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_promenade_8017F0BC, { NULL } },                         // 0x062
    { { { TASK_BODY_NONE, 0x70 } }, taskKill, { NULL } },                                                   // 0x063
    { { { TASK_BODY_TMD, 0x70 } }, func_acropolis_cafeteria_8017F390, { &gAcropolisCafeteriaModel077D8 } }, // 0x064
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_510900_80134284, { NULL } },                                // 0x065
    { { { TASK_BODY_TMD, 0x70 } }, Gp_EffModelTask, { &D_80112200 } },                                      // 0x066
    { { { TASK_BODY_TMD, 0x70 } }, Gp_EffModelTask, { &D_801120E4 } },                                      // 0x067
    { { { TASK_BODY_TMD, 0x70 } }, Gp_EffModelTask, { &D_8011231C } },                                      // 0x068
    { { { TASK_BODY_COORD, 0x70 } }, func_pyrokinesis_80130C54, { NULL } },                                 // 0x069
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask6A, { NULL } },                                           // 0x06A
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask6B, { NULL } },                                           // 0x06B
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask6C, { NULL } },                                           // 0x06C
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask6D, { NULL } },                                           // 0x06D
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask6E, { NULL } },                                           // 0x06E
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask6F, { NULL } },                                           // 0x06F
    { { { TASK_BODY_COORD, 0x70 } }, func_800F289C, { NULL } },                                             // 0x070
    { { { TASK_BODY_COORD, 0x70 } }, func_800F4308, { NULL } },                                             // 0x071
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask72, { NULL } },                                           // 0x072
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_motel_balcony_80180D40, { NULL } },                      // 0x073
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_motel_balcony_80181628, { NULL } },                      // 0x074
    { { { TASK_BODY_NONE, 0x70 } }, taskKill, { NULL } },                                                   // 0x075
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask76, { NULL } },                                           // 0x076
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_sanctuary_8017E00C, { NULL } },                         // 0x077
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_sanctuary_8017E134, { NULL } },                         // 0x078
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_sanctuary_8017E338, { NULL } },                         // 0x079
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_sanctuary_8017EC90, { NULL } },                         // 0x07A
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_security_room_80181108, { NULL } },                     // 0x07B
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask7C, { NULL } },                                           // 0x07C
    { { { TASK_BODY_NONE, 0x70 } }, taskKill, { NULL } },                                                   // 0x07D
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_balcony_80181E7C, { NULL } },                // 0x07E
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask7F, { NULL } },                                           // 0x07F
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask80, { NULL } },                                           // 0x080
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask81, { NULL } },                                           // 0x081
    { { { TASK_BODY_NONE, 0x70 } }, taskKill, { NULL } },                                                   // 0x082
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_patio_8017E100, { NULL } },                             // 0x083
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_hallway_8017D828, { NULL } },                           // 0x084
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_forked_road_8017E298, { NULL } },                       // 0x085
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_roof_garden_8017DCDC, { NULL } },                       // 0x086
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_patio_8017E324, { NULL } },                             // 0x087
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_fountain_8017DD44, { NULL } },                          // 0x088
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_forked_road_8017E410, { NULL } },                       // 0x089
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_roof_garden_8017DE90, { NULL } },                       // 0x08A
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_sanctuary_8017F4E8, { NULL } },                         // 0x08B
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_fire_escape_80180154, { NULL } },                       // 0x08C
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask8D, { NULL } },                                           // 0x08D
    { { { TASK_BODY_COORD, 0x70 } }, func_800FF710, { NULL } },                                             // 0x08E
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_patio_8017E730, { NULL } },                             // 0x08F
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_roof_garden_8017E29C, { NULL } },                       // 0x090
    { { { TASK_BODY_TMD, 0x70 } }, Gp_EffModelTask, { &D_801124B8 } },                                      // 0x091
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffLineTask92, { NULL } },                                          // 0x092
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_balcony_801809CC, { NULL } },                // 0x093
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_balcony_80181024, { NULL } },                // 0x094
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_balcony_8018158C, { NULL } },                // 0x095
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_plaza_80182054, { NULL } },                             // 0x096
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_back_street_8017EDF4, { NULL } },                  // 0x097
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_plaza_801802C0, { NULL } },                             // 0x098
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_plaza_801811D0, { NULL } },                             // 0x099
    { { { TASK_BODY_COORD, 0x70 } }, func_800F91AC, { NULL } },                                             // 0x09A
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTask9B, { NULL } },                                           // 0x09B
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffPolyTask9C, { NULL } },                                          // 0x09C
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_cafeteria_8017E89C, { NULL } },                         // 0x09D
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTask9E, { NULL } },                                           // 0x09E
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_toilet_8017DEF4, { NULL } },                             // 0x09F
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_security_room_801817A4, { NULL } },                     // 0x0A0
    { { { TASK_BODY_COORD, 0x70 } }, func_800ED42C, { NULL } },                                             // 0x0A1
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_toilet_8017DCF0, { NULL } },                             // 0x0A2
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffLineTaskA3, { NULL } },                                          // 0x0A3
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffTileTaskA4, { NULL } },                                          // 0x0A4
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTaskA5, { NULL } },                                           // 0x0A5
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTaskA6, { NULL } },                                           // 0x0A6
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTaskA7, { NULL } },                                           // 0x0A7
    { { { TASK_BODY_COORD, 0x70 } }, func_800FAA14, { NULL } },                                             // 0x0A8
    { { { TASK_BODY_COORD, 0x70 } }, func_combustion_8012F888, { NULL } },                                  // 0x0A9
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_reservoir_801813F0, { NULL } },                        // 0x0AA
    { { { TASK_BODY_COORD, 0x70 } }, func_lifedrain_8012EF48, { NULL } },                                   // 0x0AB
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTaskAC, { NULL } },                                           // 0x0AC
    { { { TASK_BODY_COORD, 0x70 } }, func_lifedrain_8012F9A8, { NULL } },                                   // 0x0AD
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTaskAE, { NULL } },                                           // 0x0AE
    { { { TASK_BODY_COORD, 0x70 } }, func_lifedrain_8012FAF8, { NULL } },                                   // 0x0AF
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_bridge_8017F868, { NULL } },                            // 0x0B0
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_bridge_801812F4, { NULL } },                            // 0x0B1
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_bridge_801819C8, { NULL } },                            // 0x0B2
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_bridge_80181D28, { NULL } },                            // 0x0B3
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_bridge_80180320, { NULL } },                            // 0x0B4
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_bridge_8018063C, { NULL } },                            // 0x0B5
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_bridge_8018099C, { NULL } },                            // 0x0B6
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_bridge_80180CC0, { NULL } },                            // 0x0B7
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_bridge_80180FF0, { NULL } },                            // 0x0B8
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_bridge_80182694, { NULL } },                            // 0x0B9
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_bridge_80182AF8, { NULL } },                            // 0x0BA
    { { { TASK_BODY_COORD, 0x70 } }, func_8011D1E0, { NULL } },                                             // 0x0BB
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_bridge_80182394, { NULL } },                            // 0x0BC
    { { { TASK_BODY_COORD, 0x70 } }, func_8011D1E0, { NULL } },                                             // 0x0BD
    { { { TASK_BODY_COORD, 0x70 } }, func_inferno_8012EF88, { NULL } },                                     // 0x0BE
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_underpass_8017DE30, { NULL } },                          // 0x0BF
    { { { TASK_BODY_COORD, 0x70 } }, func_apobiosis_8012EF4C, { NULL } },                                   // 0x0C0
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTaskC1, { NULL } },                                           // 0x0C1
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_gas_station_80181A78, { NULL } },                        // 0x0C2
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_main_street_8017E4B0, { NULL } },                        // 0x0C3
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_general_store_8017E150, { NULL } },                      // 0x0C4
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_back_street_8017D970, { NULL } },                        // 0x0C5
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_souvenir_shop_8017DFD4, { NULL } },                      // 0x0C6
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_warehouse_8017F494, { NULL } },                          // 0x0C7
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_dilapidated_house_80183BF8, { NULL } },                  // 0x0C8
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_motel_room_1_8017E0A0, { NULL } },                       // 0x0C9
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_motel_room_2_8017D6B4, { NULL } },                       // 0x0CA
    { { { TASK_BODY_COORD, 0x70 } }, func_antibody_8012EF34, { NULL } },                                    // 0x0CB
    { { { TASK_BODY_COORD, 0x70 } }, func_energyshot_8012EF34, { NULL } },                                  // 0x0CC
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_parking_lot_8017DBAC, { NULL } },                        // 0x0CD
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_toilet_8017E64C, { NULL } },                             // 0x0CE
    { { { TASK_BODY_COORD, 0x70 } }, func_energyball_8012EF48, { NULL } },                                  // 0x0CF
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_saloon_g_r_8017DA70, { NULL } },                         // 0x0D0
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_g_r_kitchen_8017EB04, { NULL } },                        // 0x0D1
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_water_tower_80180348, { NULL } },                        // 0x0D2
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_water_tank_8017F084, { NULL } },                         // 0x0D3
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_breezeway_8017FF7C, { NULL } },                          // 0x0D4
    { { { TASK_BODY_COORD, 0x70 } }, factoryDayDrawGlows, { NULL } },                                       // 0x0D5
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_garage_8017DC68, { NULL } },                             // 0x0D6
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_driveway_8017DE6C, { NULL } },                           // 0x0D7
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_junk_yard_8017DD0C, { NULL } },                          // 0x0D8
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_trailer_coach_801838DC, { NULL } },                      // 0x0D9
    { { { TASK_BODY_COORD, 0x70 } }, func_inferno_8012F530, { NULL } },                                     // 0x0DA
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_motel_balcony_8017DC28, { NULL } },                      // 0x0DB
    { { { TASK_BODY_COORD, 0x70 } }, motelRoom6DayDrawGlow, { NULL } },                                     // 0x0DC
    { { { TASK_BODY_COORD, 0x70 } }, func_energyshot_8012FFB8, { NULL } },                                  // 0x0DD
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_water_hole_8017E040, { NULL } },                         // 0x0DE
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_cellar_8017DAEC, { NULL } },                             // 0x0DF
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTaskE0, { NULL } },                                           // 0x0E0
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTaskE1, { NULL } },                                           // 0x0E1
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffSprTaskE2, { NULL } },                                           // 0x0E2
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTaskE3, { NULL } },                                           // 0x0E3
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_back_street_8017F6DC, { NULL } },                  // 0x0E4
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_junk_yard_8017E5C8, { NULL } },                    // 0x0E5
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_junk_yard_8017F02C, { NULL } },                    // 0x0E6
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_junk_yard_8017F914, { NULL } },                    // 0x0E7
    { { { TASK_BODY_COORD, 0x70 } }, func_800EC47C, { NULL } },                                             // 0x0E8
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_mesa_8017F230, { NULL } },                                   // 0x0E9
    { { { TASK_BODY_COORD, 0x70 } }, func_lifedrain_801308C0, { NULL } },                                   // 0x0EA
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_mesa_8017FC94, { NULL } },                                   // 0x0EB
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_mesa_8018057C, { NULL } },                                   // 0x0EC
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_lower_sewer_8017FEB0, { NULL } },                      // 0x0ED
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_lower_sewer_80180914, { NULL } },                      // 0x0EE
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_lower_sewer_801811FC, { NULL } },                      // 0x0EF
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_80182734, { NULL } },                      // 0x0F0
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_80183198, { NULL } },                      // 0x0F1
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_80183A80, { NULL } },                      // 0x0F2
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTaskF3, { NULL } },                                           // 0x0F3
    { { { TASK_BODY_COORD, 0x70 } }, Gp_EffCtlTaskF4, { NULL } },                                           // 0x0F4
    { { { TASK_BODY_COORD, 0x70 } }, func_antibody_8012F734, { NULL } },                                    // 0x0F5
    { { { TASK_BODY_COORD, 0x70 } }, func_pyrokinesis_801311B8, { NULL } },                                 // 0x0F6
    { { { TASK_BODY_COORD, 0x70 } }, func_apobiosis_8012FE10, { NULL } },                                   // 0x0F7
    { { { TASK_BODY_COORD, 0x70 } }, func_energyball_8012F180, { NULL } },                                  // 0x0F8
    { { { TASK_BODY_COORD, 0x70 } }, func_energyball_8013107C, { NULL } },                                  // 0x0F9
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_forest_zone_8017E3C0, { NULL } },                         // 0x0FA
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_forest_zone_8017DC20, { NULL } },                         // 0x0FB
    { { { TASK_BODY_COORD, 0x70 } }, func_pyrokinesis_8012FAC8, { NULL } },                                 // 0x0FC
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_water_hole_8017EC90, { NULL } },                         // 0x0FD
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_water_hole_8017F118, { NULL } },                         // 0x0FE
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_water_hole_8017F254, { NULL } },                   // 0x0FF
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_gas_station_80180E9C, { NULL } },                  // 0x100
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_main_street_8017E484, { NULL } },                  // 0x101
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_general_store_8017E6C8, { NULL } },                // 0x102
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_back_street_8017D7E0, { NULL } },                  // 0x103
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_souvenir_shop_8017DFF4, { NULL } },                // 0x104
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_warehouse_8017E778, { NULL } },                    // 0x105
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_dilapidated_house_8017E670, { NULL } },            // 0x106
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_room_1_8017D9B0, { NULL } },                 // 0x107
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_room_2_8017D990, { NULL } },                 // 0x108
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_room_3_8017D9B4, { NULL } },                 // 0x109
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_room_4_8017D990, { NULL } },                 // 0x10A
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_parking_lot_8017DC88, { NULL } },                  // 0x10B
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_toilet_8017D9F8, { NULL } },                       // 0x10C
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_lobby_801812F8, { NULL } },                  // 0x10D
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_saloon_g_r_8017E6C8, { NULL } },                   // 0x10E
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_g_r_kitchen_8017E1E4, { NULL } },                  // 0x10F
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_water_tower_8017DB80, { NULL } },                  // 0x110
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_water_tank_8017DD8C, { NULL } },                   // 0x111
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_breezeway_8017E5BC, { NULL } },                    // 0x112
    { { { TASK_BODY_COORD, 0x70 } }, factoryNightDrawGlows, { NULL } },                                     // 0x113
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_garage_80181518, { NULL } },                       // 0x114
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_driveway_8017E5CC, { NULL } },                     // 0x115
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_junk_yard_8017DA14, { NULL } },                    // 0x116
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_trailer_coach_80182924, { NULL } },                // 0x117
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_room_5_8017D9A4, { NULL } },                 // 0x118
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_balcony_8017E554, { NULL } },                // 0x119
    { { { TASK_BODY_COORD, 0x70 } }, motelRoom6NightDrawGlow, { NULL } },                                   // 0x11A
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_loft_8017DB64, { NULL } },                   // 0x11B
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_water_hole_8017E6D0, { NULL } },                   // 0x11C
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_cellar_8017DA28, { NULL } },                       // 0x11D
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_underpass_8017DC3C, { NULL } },                    // 0x11E
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_water_hole_8017F6DC, { NULL } },                   // 0x11F
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_mesa_8017ED08, { NULL } },                                   // 0x120
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_cavern_8017E474, { NULL } },                                 // 0x121
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_tunnel_entrance_8017D720, { NULL } },                        // 0x122
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_tunnel_8017D7D4, { NULL } },                                 // 0x123
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_gorge_8017D9F8, { NULL } },                                  // 0x124
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_refuge_80181454, { NULL } },                                 // 0x125
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_forked_tunnel_8017E78C, { NULL } },                          // 0x126
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_secret_passage_8017D9D4, { NULL } },                         // 0x127
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_elevator_hall_8017DC80, { NULL } },                    // 0x128
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_south_maintenance_walkway_8017DA8C, { NULL } },        // 0x129
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_8017D7EC, { NULL } },                        // 0x12A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_8017DBC8, { NULL } },        // 0x12B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_armory_801807E4, { NULL } },                           // 0x12C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_sleeping_quarters_8017D8E0, { NULL } },                // 0x12D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_main_corridor_8017DDF0, { NULL } },                    // 0x12E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_sterilization_room_8018188C, { NULL } },               // 0x12F
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_pod_access_tunnel_8017E7D4, { NULL } },                // 0x130
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_8017F150, { NULL } },                     // 0x131
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_access_tunnel_8017DD60, { NULL } },                    // 0x132
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_underground_parking_80184A18, { NULL } },              // 0x133
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_golem_freezer_1_8017DA7C, { NULL } },                  // 0x134
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_bottom_8017D760, { NULL } },                       // 0x135
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_pod_service_gantry_8017FA7C, { NULL } },               // 0x136
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_transfer_tunnel_8017D6D0, { NULL } },                  // 0x137
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_access_tunnel_8017E1BC, { NULL } },       // 0x138
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_elevator_8017DB70, { NULL } },                         // 0x139
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_elevator_hall_8017DD60, { NULL } },                    // 0x13A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_south_maintenance_walkway_8017DCC4, { NULL } },        // 0x13B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_operating_room_8017DDB8, { NULL } },                   // 0x13C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_north_maintenance_walkway_8017DDE8, { NULL } },        // 0x13D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_laboratory_80180548, { NULL } },                       // 0x13E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_breeding_room_8017D898, { NULL } },                    // 0x13F
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_main_corridor_8017EC34, { NULL } },                    // 0x140
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_septic_tank_8017EB7C, { NULL } },                      // 0x141
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_access_tunnel_8017DC6C, { NULL } },                // 0x142
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_dumping_hole_80183F84, { NULL } },                     // 0x143
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_garbage_incinerator_8018110C, { NULL } },              // 0x144
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_incinerator_control_room_8017FD10, { NULL } },         // 0x145
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_elevator_hall_8017DE70, { NULL } },                    // 0x146
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_lower_sewer_8017E400, { NULL } },                      // 0x147
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_8017E5F8, { NULL } },                      // 0x148
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_reservoir_8017FB84, { NULL } },                        // 0x149
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_water_supply_8017EE54, { NULL } },                     // 0x14A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r47_801858BC, { NULL } },                                 // 0x14B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_8017E3B8, { NULL } },                                 // 0x14C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_airlock_8017D6D0, { NULL } },                          // 0x14D
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_observatory_80180124, { NULL } },                         // 0x14E
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_eve_access_tunnel_8017E15C, { NULL } },                   // 0x14F
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_eve_elevator_8017D71C, { NULL } },                        // 0x150
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_north_promenade_8017D720, { NULL } },                     // 0x151
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_submarine_tunnel_8017F48C, { NULL } },                    // 0x152
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pavilion_8017FC10, { NULL } },                            // 0x153
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_island_8017FB2C, { NULL } },                              // 0x154
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_garden_8017EA9C, { NULL } },                              // 0x155
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_power_plant_2_8017D8AC, { NULL } },                       // 0x156
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_power_plant_1_8017DA18, { NULL } },                       // 0x157
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_savanna_zone_8017D9AC, { NULL } },                        // 0x158
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_south_promenade_8017D6D0, { NULL } },                     // 0x159
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_altar_8017EF84, { NULL } },                               // 0x15A
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_shrine_8017F8DC, { NULL } },                              // 0x15B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_nursery_801800A0, { NULL } },                          // 0x15C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_growth_room_8017D9D8, { NULL } },                      // 0x15D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_corridor_8017E238, { NULL } },                         // 0x15E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_training_room_8017DDE8, { NULL } },                    // 0x15F
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_bridge_8017E954, { NULL } },                              // 0x160
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_woodland_path_8017EA08, { NULL } },                       // 0x161
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r49_8017D9D0, { NULL } },                                 // 0x162
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_parking_garage_8017DF6C, { NULL } },                   // 0x163
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_vehicular_airlock_8017DAA0, { NULL } },                // 0x164
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_bulwark_8017E2A4, { NULL } },                          // 0x165
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_heliport_80180B4C, { NULL } },                         // 0x166
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_guardroom_8017DA28, { NULL } },                        // 0x167
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_r26_8017D778, { NULL } },                                 // 0x168
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_tent_8017FE10, { NULL } },                             // 0x169
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_main_corridor_8017EF24, { NULL } },                    // 0x16A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_main_corridor_8017F3AC, { NULL } },                    // 0x16B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_septic_tank_8017F040, { NULL } },                      // 0x16C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_septic_tank_8017F4C8, { NULL } },                      // 0x16D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_lower_sewer_8017EEE4, { NULL } },                      // 0x16E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_lower_sewer_8017F36C, { NULL } },                      // 0x16F
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_8017E8B8, { NULL } },                      // 0x170
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_8017ED40, { NULL } },                      // 0x171
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_reservoir_801803DC, { NULL } },                        // 0x172
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_reservoir_80180864, { NULL } },                        // 0x173
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_water_supply_8017F24C, { NULL } },                     // 0x174
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_water_supply_8017F6D4, { NULL } },                     // 0x175
    { { { TASK_BODY_COORD, 0x70 } }, waterRippleTaskFixedCoord, { NULL } },                                 // 0x176
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pavilion_8017F0CC, { NULL } },                            // 0x177
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_island_8017EB68, { NULL } },                              // 0x178
    { { { TASK_BODY_COORD, 0x70 } }, waterDriftTaskU16FixedCoord, { NULL } },                               // 0x179
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_bridge_8017EF70, { NULL } },                              // 0x17A
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_bridge_8017F3F8, { NULL } },                              // 0x17B
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_roof_garden_8017F10C, { NULL } },                       // 0x17C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_sterilization_room_801823D8, { NULL } },               // 0x17D
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_r08_8017D718, { NULL } },                          // 0x17E
    { { { TASK_BODY_COORD, 0x70 } }, func_m4a1_pyke_8011D7D4, { NULL } },                                   // 0x17F
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_800100_80161F20, { NULL } },                                // 0x180
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_800100_801624F0, { NULL } },                                // 0x181
    { { { TASK_BODY_COORD, 0x70 } }, func_m4a1_hammer_8011DD08, { NULL } },                                 // 0x182
    { { { TASK_BODY_COORD, 0x70 } }, func_m4a1_javelin_8011F4E8, { NULL } },                                // 0x183
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_510900_8013482C, { NULL } },                                // 0x184
    { { { TASK_BODY_COORD, 0x70 } }, func_actor_510900_801346D4, { NULL } },                                // 0x185
    { { { TASK_BODY_COORD, 0x70 } }, func_gunblade_8011D1E4, { NULL } },                                    // 0x186
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_woodland_path_8017F928, { NULL } },                       // 0x187
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_dilapidated_house_80181F08, { NULL } },                  // 0x188
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_8017E4C4, { NULL } },                                 // 0x189
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_8017EC18, { NULL } },                                 // 0x18A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_8017F6C0, { NULL } },                                 // 0x18B
    { { { TASK_BODY_COORD, 0x70 } }, shelterR48SpriteDriftTask, { NULL } },                                 // 0x18C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_8017E704, { NULL } },                                 // 0x18D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_8017E9B8, { NULL } },                                 // 0x18E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_8017EFD8, { NULL } },                                 // 0x18F
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_801810B0, { NULL } },                                 // 0x190
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_8018147C, { NULL } },                                 // 0x191
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_submarine_gallery_8017EFEC, { NULL } },                   // 0x192
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_submarine_gallery_8017F288, { NULL } },                   // 0x193
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_submarine_gallery_8017F710, { NULL } },                   // 0x194
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_r48_80181704, { NULL } },                                 // 0x195
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_garbage_incinerator_80182368, { NULL } },              // 0x196
    { { { TASK_BODY_COORD, 0x70 } }, func_pepper_spray_8012EF34, { NULL } },                                // 0x197
    { { { TASK_BODY_COORD, 0x70 } }, ofudaEffectTask, { NULL } },                                           // 0x198
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_dumping_hole_8018521C, { NULL } },                     // 0x199
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_dumping_hole_80186218, { NULL } },                     // 0x19A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_dumping_hole_80186D4C, { NULL } },                     // 0x19B
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteDebrisTask, { NULL } },                                    // 0x19C
    { { { TASK_BODY_COORD, 0x70 } }, flareEffectTask, { NULL } },                                           // 0x19D
    { { { TASK_BODY_COORD, 0x70 } }, flareSparkTask, { NULL } },                                            // 0x19E
    { { { TASK_BODY_COORD, 0x70 } }, func_mist_shooting_gallery_801811EC, { NULL } },                       // 0x19F
    { { { TASK_BODY_COORD, 0x70 } }, func_combustion_801308E0, { NULL } },                                  // 0x1A0
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_growth_room_8017E564, { NULL } },                      // 0x1A1
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_growth_room_8017EAC8, { NULL } },                      // 0x1A2
    { { { TASK_BODY_TMD, 0x70 } }, func_shelter_b6_nursery_80181314, { &gShelterB6NurseryModel07BAC } },    // 0x1A3
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_nursery_80181820, { NULL } },                          // 0x1A4
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_nursery_80182730, { NULL } },                          // 0x1A5
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_golem_freezer_1_8017DFFC, { NULL } },                  // 0x1A6
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_training_room_8017EE70, { NULL } },                    // 0x1A7
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_training_room_8017F8B8, { NULL } },                    // 0x1A8
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_training_room_80180DB4, { NULL } },                    // 0x1A9
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_training_room_801811AC, { NULL } },                    // 0x1AA
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_training_room_80181A3C, { NULL } },                    // 0x1AB
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_training_room_8018245C, { NULL } },                    // 0x1AC
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_training_room_801825C0, { NULL } },                    // 0x1AD
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_training_room_801826E0, { NULL } },                    // 0x1AE
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_training_room_80182804, { NULL } },                    // 0x1AF
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_motel_loft_8017E090, { NULL } },                   // 0x1B0
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_main_street_8017E830, { NULL } },                        // 0x1B1
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_main_street_8017F3B0, { NULL } },                  // 0x1B2
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_bottom_8017D850, { NULL } },                       // 0x1B3
    { { { TASK_BODY_COORD, 0x70 } }, shelterB1PodServiceGantrySpriteDriftTask, { NULL } },                  // 0x1B4
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_access_tunnel_8017E6E0, { NULL } },                // 0x1B5
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_r08_8017D5F8, { NULL } },                                // 0x1B6
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldR08SpriteDriftTask, { NULL } },                                // 0x1B7
    { { { TASK_BODY_COORD, 0x70 } }, dryfieldR08SpriteDriftTask, { NULL } },                                // 0x1B8
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_bottom_80181B48, { NULL } },                       // 0x1B9
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_bottom_8017EC78, { NULL } },                       // 0x1BA
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_bottom_8017F448, { NULL } },                       // 0x1BB
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_bottom_8018016C, { NULL } },                       // 0x1BC
    { { { TASK_BODY_COORD, 0x70 } }, func_mist_shooting_gallery_80182064, { NULL } },                       // 0x1BD
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_pod_service_gantry_8017E880, { NULL } },               // 0x1BE
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_bottom_80180898, { NULL } },                       // 0x1BF
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_bottom_80180F10, { NULL } },                       // 0x1C0
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_woodland_path_8017ED00, { NULL } },                       // 0x1C1
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_bulwark_8017E38C, { NULL } },                          // 0x1C2
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_bulwark_8017EDF0, { NULL } },                          // 0x1C3
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_bulwark_8017F6D8, { NULL } },                          // 0x1C4
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pyramid_8017DBF0, { NULL } },                             // 0x1C5
    { { { TASK_BODY_COORD, 0x70 } }, effectSpriteRiseTask, { NULL } },                                      // 0x1C6
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_r08_8017E5B0, { NULL } },                          // 0x1C7
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_elevator_hall_80180D18, { NULL } },                    // 0x1C8
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_south_maintenance_walkway_8017E760, { NULL } },        // 0x1C9
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_80180DCC, { NULL } },                        // 0x1CA
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_80180EDC, { NULL } },        // 0x1CB
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_main_corridor_801810F8, { NULL } },                    // 0x1CC
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_pod_access_tunnel_8017F138, { NULL } },                // 0x1CD
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_transfer_tunnel_8018092C, { NULL } },                  // 0x1CE
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_access_tunnel_8017E2D8, { NULL } },       // 0x1CF
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_elevator_hall_801817FC, { NULL } },                    // 0x1D0
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_south_maintenance_walkway_8017E99C, { NULL } },        // 0x1D1
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_north_maintenance_walkway_80181BB4, { NULL } },        // 0x1D2
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_main_corridor_8018094C, { NULL } },                    // 0x1D3
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_septic_tank_80180BE0, { NULL } },                      // 0x1D4
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_access_tunnel_80181C2C, { NULL } },                // 0x1D5
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_parking_garage_8017EC0C, { NULL } },                   // 0x1D6
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_vehicular_airlock_8017ECBC, { NULL } },                // 0x1D7
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_north_promenade_8017FDD4, { NULL } },                     // 0x1D8
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_forest_zone_8017E420, { NULL } },                         // 0x1D9
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pavilion_8017FCB0, { NULL } },                            // 0x1DA
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_island_8017FB9C, { NULL } },                              // 0x1DB
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_power_plant_2_8017DDF4, { NULL } },                       // 0x1DC
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_savanna_zone_8017DA0C, { NULL } },                        // 0x1DD
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_south_promenade_8017D720, { NULL } },                     // 0x1DE
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_shrine_8017FEA0, { NULL } },                              // 0x1DF
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_nursery_80182D28, { NULL } },                          // 0x1E0
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_bridge_8017FF84, { NULL } },                              // 0x1E1
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pyramid_8017DC50, { NULL } },                             // 0x1E2
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_r08_8017F014, { NULL } },                          // 0x1E3
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_elevator_hall_8018177C, { NULL } },                    // 0x1E4
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_south_maintenance_walkway_8017F1C4, { NULL } },        // 0x1E5
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_80181830, { NULL } },                        // 0x1E6
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_80181940, { NULL } },        // 0x1E7
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_main_corridor_80181B5C, { NULL } },                    // 0x1E8
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_pod_access_tunnel_8017FB9C, { NULL } },                // 0x1E9
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_transfer_tunnel_80181390, { NULL } },                  // 0x1EA
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_access_tunnel_8017ED3C, { NULL } },       // 0x1EB
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_elevator_hall_80182260, { NULL } },                    // 0x1EC
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_south_maintenance_walkway_8017F400, { NULL } },        // 0x1ED
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_north_maintenance_walkway_80182618, { NULL } },        // 0x1EE
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_main_corridor_801813B0, { NULL } },                    // 0x1EF
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_septic_tank_80181644, { NULL } },                      // 0x1F0
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_access_tunnel_80182690, { NULL } },                // 0x1F1
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_parking_garage_8017F670, { NULL } },                   // 0x1F2
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_vehicular_airlock_8017F720, { NULL } },                // 0x1F3
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_north_promenade_80180838, { NULL } },                     // 0x1F4
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_forest_zone_8017EE84, { NULL } },                         // 0x1F5
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pavilion_80180714, { NULL } },                            // 0x1F6
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_island_80180600, { NULL } },                              // 0x1F7
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_power_plant_2_8017E858, { NULL } },                       // 0x1F8
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_savanna_zone_8017E470, { NULL } },                        // 0x1F9
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_south_promenade_8017E184, { NULL } },                     // 0x1FA
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_shrine_80180904, { NULL } },                              // 0x1FB
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_nursery_8018378C, { NULL } },                          // 0x1FC
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_bridge_801809E8, { NULL } },                              // 0x1FD
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pyramid_8017E6B4, { NULL } },                             // 0x1FE
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_r08_8017F8FC, { NULL } },                          // 0x1FF
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_elevator_hall_80182064, { NULL } },                    // 0x200
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_south_maintenance_walkway_8017FAAC, { NULL } },        // 0x201
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_80182118, { NULL } },                        // 0x202
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_80182228, { NULL } },        // 0x203
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_main_corridor_80182444, { NULL } },                    // 0x204
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_pod_access_tunnel_80180484, { NULL } },                // 0x205
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_transfer_tunnel_80181C78, { NULL } },                  // 0x206
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_access_tunnel_8017F624, { NULL } },       // 0x207
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_elevator_hall_80182B48, { NULL } },                    // 0x208
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_south_maintenance_walkway_8017FCE8, { NULL } },        // 0x209
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_north_maintenance_walkway_80182F00, { NULL } },        // 0x20A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_main_corridor_80181C98, { NULL } },                    // 0x20B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_septic_tank_80181F2C, { NULL } },                      // 0x20C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_access_tunnel_80182F78, { NULL } },                // 0x20D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_parking_garage_8017FF58, { NULL } },                   // 0x20E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_1f_vehicular_airlock_80180008, { NULL } },                // 0x20F
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_north_promenade_80181120, { NULL } },                     // 0x210
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_forest_zone_8017F76C, { NULL } },                         // 0x211
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pavilion_80180FFC, { NULL } },                            // 0x212
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_island_80180EE8, { NULL } },                              // 0x213
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_power_plant_2_8017F140, { NULL } },                       // 0x214
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_savanna_zone_8017ED58, { NULL } },                        // 0x215
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_south_promenade_8017EA6C, { NULL } },                     // 0x216
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_shrine_801811EC, { NULL } },                              // 0x217
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_nursery_80184074, { NULL } },                          // 0x218
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_bridge_801812D0, { NULL } },                              // 0x219
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pyramid_8017EF9C, { NULL } },                             // 0x21A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_south_maintenance_walkway_80180C4C, { NULL } },        // 0x21B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_south_maintenance_walkway_801806F4, { NULL } },        // 0x21C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_south_maintenance_walkway_801818AC, { NULL } },        // 0x21D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_80182D60, { NULL } },                        // 0x21E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_80182E70, { NULL } },        // 0x21F
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_sleeping_quarters_8017E6DC, { NULL } },                // 0x220
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_south_maintenance_walkway_80180930, { NULL } },        // 0x221
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_operating_room_8017ECFC, { NULL } },                   // 0x222
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_elevator_hall_80180E18, { NULL } },                    // 0x223
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_801846C8, { NULL } },                      // 0x224
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_reservoir_80182B1C, { NULL } },                        // 0x225
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_water_supply_801809DC, { NULL } },                     // 0x226
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pavilion_80181C44, { NULL } },                            // 0x227
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_garden_8017F790, { NULL } },                              // 0x228
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_801832B8, { NULL } },                        // 0x229
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_801833C8, { NULL } },        // 0x22A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_sleeping_quarters_8017EC34, { NULL } },                // 0x22B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_south_maintenance_walkway_80180E88, { NULL } },        // 0x22C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_operating_room_8017F254, { NULL } },                   // 0x22D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_elevator_hall_80181370, { NULL } },                    // 0x22E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_80184C20, { NULL } },                      // 0x22F
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_reservoir_80183074, { NULL } },                        // 0x230
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_water_supply_80180F34, { NULL } },                     // 0x231
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pavilion_8018219C, { NULL } },                            // 0x232
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_garden_8017FCE8, { NULL } },                              // 0x233
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_80183F18, { NULL } },                        // 0x234
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_80184028, { NULL } },        // 0x235
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_sleeping_quarters_8017F894, { NULL } },                // 0x236
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_south_maintenance_walkway_80181AE8, { NULL } },        // 0x237
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_operating_room_8017FEB4, { NULL } },                   // 0x238
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_elevator_hall_80181FD0, { NULL } },                    // 0x239
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_80185880, { NULL } },                      // 0x23A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_reservoir_80183CD4, { NULL } },                        // 0x23B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_water_supply_80181B94, { NULL } },                     // 0x23C
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_pavilion_80182DFC, { NULL } },                            // 0x23D
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_garden_80180948, { NULL } },                              // 0x23E
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_cavern_80180320, { NULL } },                                 // 0x23F
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_secret_passage_8017F948, { NULL } },                         // 0x240
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_secret_passage_8017F5B0, { NULL } },                         // 0x241
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_secret_passage_8017E868, { NULL } },                         // 0x242
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_secret_passage_80180D58, { NULL } },                         // 0x243
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_cavern_8017F240, { NULL } },                                 // 0x244
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_elevator_hall_8017E6F4, { NULL } },                    // 0x245
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_8017E7A8, { NULL } },                        // 0x246
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_8017E8B8, { NULL } },        // 0x247
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_main_corridor_8017EAD4, { NULL } },                    // 0x248
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_transfer_tunnel_8017E308, { NULL } },                  // 0x249
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_elevator_hall_8017F1D8, { NULL } },                    // 0x24A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_north_maintenance_walkway_8017F590, { NULL } },        // 0x24B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_access_tunnel_8017F608, { NULL } },                // 0x24C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_elevator_hall_8017E7F4, { NULL } },                    // 0x24D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_80180110, { NULL } },                      // 0x24E
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_north_promenade_8017D7B0, { NULL } },                     // 0x24F
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_cavern_8017FF88, { NULL } },                                 // 0x250
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_elevator_hall_8017F43C, { NULL } },                    // 0x251
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_8017F4F0, { NULL } },                        // 0x252
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_8017F600, { NULL } },        // 0x253
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_main_corridor_8017F81C, { NULL } },                    // 0x254
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_transfer_tunnel_8017F050, { NULL } },                  // 0x255
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_elevator_hall_8017FF20, { NULL } },                    // 0x256
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_north_maintenance_walkway_801802D8, { NULL } },        // 0x257
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_access_tunnel_80180350, { NULL } },                // 0x258
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_elevator_hall_8017F53C, { NULL } },                    // 0x259
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_80180E58, { NULL } },                      // 0x25A
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_north_promenade_8017E4F8, { NULL } },                     // 0x25B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_elevator_hall_8017F7D4, { NULL } },                    // 0x25C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_8017F888, { NULL } },                        // 0x25D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_8017F998, { NULL } },        // 0x25E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_main_corridor_8017FBB4, { NULL } },                    // 0x25F
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_transfer_tunnel_8017F3E8, { NULL } },                  // 0x260
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_elevator_hall_801802B8, { NULL } },                    // 0x261
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_north_maintenance_walkway_80180670, { NULL } },        // 0x262
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_access_tunnel_801806E8, { NULL } },                // 0x263
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_elevator_hall_8017F8D4, { NULL } },                    // 0x264
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_801811F0, { NULL } },                      // 0x265
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_north_promenade_8017E890, { NULL } },                     // 0x266
    { { { TASK_BODY_COORD, 0x70 } }, func_mine_cavern_80181730, { NULL } },                                 // 0x267
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_elevator_hall_80180BE4, { NULL } },                    // 0x268
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_storeroom_80180C98, { NULL } },                        // 0x269
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_north_maintenance_walkway_80180DA8, { NULL } },        // 0x26A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_main_corridor_80180FC4, { NULL } },                    // 0x26B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_transfer_tunnel_801807F8, { NULL } },                  // 0x26C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_elevator_hall_801816C8, { NULL } },                    // 0x26D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_north_maintenance_walkway_80181A80, { NULL } },        // 0x26E
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_pod_access_tunnel_80181AF8, { NULL } },                // 0x26F
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b3_elevator_hall_80180CE4, { NULL } },                    // 0x270
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b4_upper_sewer_80182600, { NULL } },                      // 0x271
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_north_promenade_8017FCA0, { NULL } },                     // 0x272
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_dilapidated_house_80182744, { NULL } },                  // 0x273
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_dilapidated_house_80183C8C, { NULL } },                  // 0x274
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_dilapidated_house_80183D5C, { NULL } },                  // 0x275
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_8017FF80, { NULL } },                     // 0x276
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_801804D8, { NULL } },                     // 0x277
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_80181138, { NULL } },                     // 0x278
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_access_tunnel_8018026C, { NULL } },       // 0x279
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_access_tunnel_801807C4, { NULL } },       // 0x27A
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b1_control_room_access_tunnel_80181424, { NULL } },       // 0x27B
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_breeding_room_8017E774, { NULL } },                    // 0x27C
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_breeding_room_8017ECCC, { NULL } },                    // 0x27D
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b2_breeding_room_8017F92C, { NULL } },                    // 0x27E
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_submarine_tunnel_8017F4DC, { NULL } },                    // 0x27F
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_submarine_tunnel_8017FA34, { NULL } },                    // 0x280
    { { { TASK_BODY_COORD, 0x70 } }, func_neo_ark_submarine_tunnel_80180694, { NULL } },                    // 0x281
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_motel_balcony_8017DCB8, { NULL } },                      // 0x282
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_motel_balcony_8017EA00, { NULL } },                      // 0x283
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_motel_balcony_8017ED98, { NULL } },                      // 0x284
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_motel_balcony_801801A8, { NULL } },                      // 0x285
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_main_street_8017FA68, { NULL } },                  // 0x286
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_main_street_801807B0, { NULL } },                  // 0x287
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_main_street_80180B48, { NULL } },                  // 0x288
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_night_main_street_80181F58, { NULL } },                  // 0x289
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_toilet_8017E69C, { NULL } },                             // 0x28A
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_toilet_8017EBF4, { NULL } },                             // 0x28B
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_toilet_8017F854, { NULL } },                             // 0x28C
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_cafeteria_8017F948, { NULL } },                         // 0x28D
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_cafeteria_801803AC, { NULL } },                         // 0x28E
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_cafeteria_80180C94, { NULL } },                         // 0x28F
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_forked_road_8017EF80, { NULL } },                       // 0x290
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_forked_road_8017F9E4, { NULL } },                       // 0x291
    { { { TASK_BODY_COORD, 0x70 } }, func_acropolis_forked_road_801802CC, { NULL } },                       // 0x292
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_main_street_8017EEE8, { NULL } },                        // 0x293
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_main_street_8017F94C, { NULL } },                        // 0x294
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_main_street_80180234, { NULL } },                        // 0x295
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_back_street_8017D9D0, { NULL } },                        // 0x296
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_back_street_8017E434, { NULL } },                        // 0x297
    { { { TASK_BODY_COORD, 0x70 } }, func_dryfield_back_street_8017ED1C, { NULL } },                        // 0x298
    { { { TASK_BODY_COORD, 0x70 } }, func_shelter_b6_corridor_8017ECA8, { NULL } },                         // 0x299
    { { { TASK_BODY_COORD, 0x70 } }, func_gunblade_8011DAA4, { NULL } },                                    // 0x29A
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
    Gp_InitRoomCoords();

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
        SndEvt_EnqueueType7(0xFF0D, 1);
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
        Gp_DecRoomCoordRefs();
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

s32 Gp_TraceGroundCoord(GfxCoord* arg0, GfxCoord* arg1)
{
    _WorldCollisionGroundProbeScratch* scratchEnd;
    _WorldCollisionGroundProbeScratch* scratch;
    SVECTOR*                           endpoint;
    MATRIX*                            world;
    s32                                ret;
    u16                                originZBits;

    scratchEnd                                              = SCRATCH_STACK_CURSOR(_WorldCollisionGroundProbeScratch);
    scratch                                                 = scratchEnd - 1;
    scratchEnd[-1].origin.vx                                = (u16)arg0->workm.t[0];
    scratch->origin.vy                                      = (u16)arg0->workm.t[1];
    originZBits                                             = (u16)arg0->workm.t[2];
    SCRATCH_STACK_CURSOR(_WorldCollisionGroundProbeScratch) = scratch;
    // Build the world +Y probe offset in view space before adding the origin.
    scratch->endpoint.vx = 0;
    scratch->endpoint.vy = WORLD_COLLISION_GROUND_PROBE_LENGTH;
    scratch->endpoint.vz = 0;
    scratch->origin.vz   = originZBits;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    endpoint = &scratchEnd[-1].endpoint;
    gte_ldv0(endpoint);
    gte_rtv0();
    gte_stsv(endpoint);
    scratch->endpoint.vx += scratchEnd[-1].origin.vx;
    scratch->endpoint.vy += scratch->origin.vy;
    scratch->endpoint.vz += scratch->origin.vz;
    // The collision query replaces the endpoint with its accepted intersection.
    ret = func_800DE7CC(endpoint, &scratch->origin, endpoint, NULL);
    if (ret == 1) {
        world            = &gGfxViewCoord.workm;
        arg1->workm.t[0] = scratch->endpoint.vx;
        arg1->workm.t[1] = scratch->endpoint.vy;
        arg1->workm.t[2] = scratch->endpoint.vz;
        gfxMakeRelativeTransform(world, &arg1->workm, &arg1->coord);
        arg1->parent       = PARENT_OF(world, GfxCoord, workm);
        arg1->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(arg1);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGroundProbeScratch);
    return ret;
}

s32 func_800EA1A8(VECTOR3* arg0, VECTOR3* arg1)
{
    _WorldCollisionGroundProbeScratch* scratchEnd;
    _WorldCollisionGroundProbeScratch* scratch;
    SVECTOR*                           endpoint;
    s32                                ret;
    u16                                originZBits;

    scratchEnd                                              = SCRATCH_STACK_CURSOR(_WorldCollisionGroundProbeScratch);
    scratch                                                 = scratchEnd - 1;
    scratchEnd[-1].origin.vx                                = (u16)arg0->vx;
    scratch->origin.vy                                      = (u16)arg0->vy;
    originZBits                                             = (u16)arg0->vz;
    SCRATCH_STACK_CURSOR(_WorldCollisionGroundProbeScratch) = scratch;
    // Build the world +Y probe offset in view space before adding the origin.
    scratch->endpoint.vx = 0;
    scratch->endpoint.vy = WORLD_COLLISION_GROUND_PROBE_LENGTH;
    scratch->endpoint.vz = 0;
    scratch->origin.vz   = originZBits;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    endpoint = &scratchEnd[-1].endpoint;
    gte_ldv0(endpoint);
    gte_rtv0();
    gte_stsv(endpoint);
    scratch->endpoint.vx += scratchEnd[-1].origin.vx;
    scratch->endpoint.vy += scratch->origin.vy;
    scratch->endpoint.vz += scratch->origin.vz;
    // The collision query replaces the endpoint with its accepted intersection.
    ret = func_800DE7CC(endpoint, &scratch->origin, endpoint, NULL);
    if (ret == 1) {
        arg1->vx = scratch->endpoint.vx;
        arg1->vy = scratch->endpoint.vy;
        arg1->vz = scratch->endpoint.vz;
        ret      = scratch->endpoint.vy - scratch->origin.vy;
        if (ret == 0) {
            ret = 1;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGroundProbeScratch);
    return ret;
}

s32 func_800EA318(s16 arg0, s16 arg1, s16 arg2)
{
    s32 result;

    result = 0;
    if (arg2 != 0) {
        result = (arg1 * (arg0 << 1)) / arg2;
        if (result >= 0x100) {
            result = 0xFF;
        } else if (result == 0) {
            result = -1;
        }
    }
    return result;
}

void func_800EA3A0(s32 arg0)
{
    gRoomEffectState->lastAnimationSoundCue = arg0 + 1;
}

static void Gp_DecRoomCoordRefs(void)
{
    s32                            i;
    WorldCoordTransientPointLight* lightSlot;

    // Expire contributions in place, retaining their light records for reuse.
    lightSlot = gWorldCoordTransientPointLights;
    for (i = 0; i < ARRAY_SIZE(gWorldCoordTransientPointLights); i++) {
        if (lightSlot->framesLeft != WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
            lightSlot->framesLeft--;
        }
        lightSlot++;
    }
}

static void Gp_InitRoomCoords(void)
{
    s32                            i;
    WorldCoordTransientPointLight* lightSlot;

    lightSlot = gWorldCoordTransientPointLights;
    for (i = 0; i < ARRAY_SIZE(gWorldCoordTransientPointLights); i++) {
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
            Gp_UpdateCoord(arg1);
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
        Gp_UpdateCoord(coord);
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
        Gp_UpdateCoord(coord);
        mem->parent = &gGfxViewCoord;
    }

    task->spawnArg2.pointer = mem;
    task->exitCallback      = Gp_KillState1CTask;
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

void Gp_DrawFadeQuad(u8* arg0, s32 arg1)
{
    POLY_F4*  p;
    DR_TPAGE* dr;
    s32       x0;
    s32       x1;
    s32       yTop;
    s32       yBot;

    arg1 &= 3;
    x0    = -0xA0;
    x1    = 0xA0;
    yTop  = -0x78;
    yBot  = 0x78;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setPolyF4(p);
    setRGB0(p, arg0[0], arg0[1], arg0[2]);
    p->x0 = x0;
    p->y0 = yTop - gDisplayState.vramYOffset;
    p->x1 = x1;
    p->y1 = yTop - gDisplayState.vramYOffset;
    p->x2 = x0;
    p->y2 = yBot - gDisplayState.vramYOffset;
    p->x3 = x1;
    p->y3 = yBot - gDisplayState.vramYOffset;
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)0x10 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), p);

    setSemiTrans(p, 1);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setDrawTPage(dr, 0, 1, 0xA | (arg1 << 5));
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)0x10 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), dr);
}

void Gp_DrawArc(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    EffectShapeScratch* block;
    POLY_G4*            prim;
    DR_TPAGE*           dr;
    s32                 ang;
    s32                 otz;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        block->extent.ring.inner = ((s16)arg1 * 64) / block->depth;
        block->extent.ring.outer = (((s16)arg1 + (s16)arg2) * 64) / block->depth;
        for (ang = 0; ang < 0x1000; ang += 0x100) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->screenX + ((block->extent.ring.inner * rsin(ang)) >> 12);
            prim->y0 = block->screenY + ((block->extent.ring.inner * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->extent.ring.inner * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->screenY + ((block->extent.ring.inner * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->screenX + ((block->extent.ring.outer * rsin(ang)) >> 12);
            prim->y2 = block->screenY + ((block->extent.ring.outer * rcos(ang)) >> 12);
            prim->x3 = block->screenX + ((block->extent.ring.outer * rsin(ang + 0x100)) >> 12);
            prim->y3 = block->screenY + ((block->extent.ring.outer * rcos(ang + 0x100)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            otz = block->depth;
            setSemiTrans(prim, 1);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setDrawTPage(dr, 0, 1, 0x2A);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    dr);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void Gp_DrawRing(GfxCoord* arg0, s32 arg1, u8* rgb)
{
    EffectCentreScratch* block;
    POLY_G4*             prim;
    DR_TPAGE*            dr;
    s32                  ang;
    s32                  otz;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        block->screenExtent = ((s16)arg1 * 64) / block->depth;
        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->screenExtent * rsin(ang)) >> 12);
            prim->y0 = block->screenY + ((block->screenExtent * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->screenExtent * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->screenY + ((block->screenExtent * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->screenExtent * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->screenY + ((block->screenExtent * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            otz = block->depth;
            setSemiTrans(prim, 1);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setDrawTPage(dr, 0, 1, 0x2A);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    dr);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

void Gp_DrawFxQuad(GfxCoord* arg0, u16 arg1, s16 arg2, u16 arg3)
{
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    u16                 clutIdx;
    s32                 u0;
    s32                 u1;
    s32                 ang2;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    clutIdx = arg3 >> 12;
    arg3   &= 0xFFF;
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = 0x2A;
        setClut(prim, Gp_QuadClutX[clutIdx], 0x10B);
        u0 = arg1 << 5;
        u1 = u0 + 0x1F;
        setUV4(prim, u0, 0x18, u1, 0x18, u0, 0x37, u1, 0x37);
        block->extent.corner.x = (((arg2 * 31) / block->depth) * rsin(arg3)) >> 12;
        block->extent.corner.y = (((arg2 * 31) / block->depth) * rcos(arg3)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        ang2                   = arg3 + 0x400;
        block->extent.corner.x = (((arg2 * 31) / block->depth) * rsin(ang2)) >> 12;
        block->extent.corner.y = (((arg2 * 31) / block->depth) * rcos(ang2)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void func_800EB6E8(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    u16                  bank;
    u16                  clutIdx;
    s32                  u0;
    s32                  u1;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    bank    = arg2 >> 12;
    arg2   &= 0xFFF;
    clutIdx = arg3 >> 12;
    arg3   &= 0xFF;
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        prim->tpage = 0x2A;
        setRGB0(prim, arg3, arg3, arg3);
        setClut(prim, D_80111EB4[clutIdx], 0x10B);
        u0 = bank * 0x60 + (arg1 & 3) * 0x18;
        u1 = u0 + 0x17;
        setUV4(prim, u0, 0, u1, 0, u0, 0x17, u1, 0x17);
        block->screenExtent = (arg2 * 23) / block->depth;
        prim->x0 = prim->x2 = block->screenX - block->screenExtent;
        prim->x1 = prim->x3 = block->screenX + block->screenExtent;
        prim->y0 = prim->y1 = block->screenY - block->screenExtent;
        prim->y2 = prim->y3 = block->screenY + block->screenExtent;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

void Gp_DrawBand(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    EffectBandScratch* block;
    SVECTOR*           op;
    POLY_G4*           prim;
    DR_TPAGE*          dr;
    s32                i;
    s32                next;
    s32                ang;
    s32                otz;
    s16                r0;
    s16                r1;

    r1    = arg1 + 0x100;
    block = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < EFFECT_BAND_SEGMENT_COUNT; i++) {
        ang                  = i << 8;
        block->topRing[i].vx = (rsin(ang) * r0) >> 12;
        block->topRing[i].vy = (rcos(ang) * r0) >> 12;
        block->topRing[i].vz = 0x100;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->topRing[i]);
        gte_rtv0();
        gte_stsv(&block->topRing[i]);
        block->topRing[i].vx = (u16)block->topRing[i].vx + (u16)arg0->workm.t[0];
        block->topRing[i].vy = (u16)block->topRing[i].vy + (u16)arg0->workm.t[1];
        block->topRing[i].vz = (u16)block->topRing[i].vz + (u16)arg0->workm.t[2];
        op                   = &block->topRing[i] + EFFECT_BAND_SEGMENT_COUNT;
        op->vx               = (rsin(ang) * r1) >> 12;
        op->vy               = (rcos(ang) * r1) >> 12;
        op->vz               = 0;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->bottomRing[i]);
        gte_rtv0();
        gte_stsv(&block->bottomRing[i]);
        op->vx = (u16)op->vx + (u16)arg0->workm.t[0];
        op->vy = (u16)op->vy + (u16)arg0->workm.t[1];
        op->vz = (u16)op->vz + (u16)arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < EFFECT_BAND_SEGMENT_COUNT; i++) {
        gte_ldv0(&block->topRing[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);
        gte_ldv3(&block->topRing[next], &block->bottomRing[i], &block->bottomRing[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, rgb[0], rgb[1], rgb[2]);
            setRGB1(prim, rgb[0], rgb[1], rgb[2]);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = (u16)block->sxy0.vx;
            prim->y0 = (u16)block->sxy0.vy;
            prim->x1 = (u16)block->sxy1.vx;
            prim->y1 = (u16)block->sxy1.vy;
            prim->x2 = (u16)block->sxy2.vx;
            prim->y2 = (u16)block->sxy2.vy;
            prim->x3 = (u16)block->sxy3.vx;
            prim->y3 = (u16)block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            otz = block->otz;
            setSemiTrans(prim, 1);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setDrawTPage(dr, 0, 1, 0x2A);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    dr);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

void Gp_DrawBandEx(GfxCoord* arg0, s16 arg1, s32 arg2, u8* rgb)
{
    EffectBandScratch* block;
    SVECTOR*           op;
    POLY_G4*           prim;
    DR_TPAGE*          dr;
    s32                i;
    s32                next;
    s32                ang;
    s32                otz;
    s16                r0;
    s16                r1;

    r1    = arg1 + arg2;
    block = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < EFFECT_BAND_SEGMENT_COUNT; i++) {
        ang                  = i << 8;
        block->topRing[i].vx = (rsin(ang) * r0) >> 12;
        block->topRing[i].vy = 0;
        block->topRing[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->topRing[i]);
        gte_rtv0();
        gte_stsv(&block->topRing[i]);
        block->topRing[i].vx   += arg0->workm.t[0];
        block->topRing[i].vy   += arg0->workm.t[1];
        block->topRing[i].vz   += arg0->workm.t[2];
        block->bottomRing[i].vx = (rsin(ang) * r1) >> 12;
        op                      = &block->topRing[i] + EFFECT_BAND_SEGMENT_COUNT;
        op->vy                  = 0;
        op->vz                  = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->bottomRing[i]);
        gte_rtv0();
        gte_stsv(&block->bottomRing[i]);
        block->bottomRing[i].vx += arg0->workm.t[0];
        op->vy                  += arg0->workm.t[1];
        op->vz                  += arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < EFFECT_BAND_SEGMENT_COUNT; i++) {
        gte_ldv0(&block->topRing[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);
        gte_ldv3(&block->topRing[next], &block->bottomRing[i], &block->bottomRing[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, rgb[0], rgb[1], rgb[2]);
            setRGB1(prim, rgb[0], rgb[1], rgb[2]);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sxy0.vx;
            prim->y0 = block->sxy0.vy;
            prim->x1 = block->sxy1.vx;
            prim->y1 = block->sxy1.vy;
            prim->x2 = block->sxy2.vx;
            prim->y2 = block->sxy2.vy;
            prim->x3 = block->sxy3.vx;
            prim->y3 = block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            otz = block->otz;
            setSemiTrans(prim, 1);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setDrawTPage(dr, 0, 1, 0x2A);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    dr);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

void func_800EC47C(Task* arg0)
{
    EffectWork* mem;
    u8          rgb[3];
    s32         current;
    s32         target;
    u16         count;
    u32         random;

    mem = arg0->spawnArg2.pointer;
    switch (arg0->state) {
        case 0:
            gRoomEffectState->screenFxFlags |= ROOM_EFFECT_SCREEN_FADE_QUAD;
            arg0->state                      = 1;
            mem->angle                       = 0x10;
        case 1:
            if (mem->scale < mem->angle) {
                mem->scale += 8;
            } else {
                arg0->state = 2;
            }
            if (!(gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS)) {
                arg0->state = 3;
            }
            rgb[0] = rgb[1] = rgb[2] = mem->scale;
            Gp_DrawFadeQuad(rgb, 2);
            break;
        case 2:
            current = mem->scale;
            target  = mem->angle;
            if (current == target) {
                count           = mem->period + 1;
                random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->period     = count;
                gRandomLcgState = random;
                mem->angle      = ((count & 1) << (((random >> 16) & 1) + 4)) + 0x10;
            } else {
                if (current < target) {
                    mem->scale = current + 8;
                } else {
                    mem->scale = current - 8;
                }
            }
            if (!(gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS)) {
                arg0->state = 3;
            }
            rgb[0] = rgb[1] = rgb[2] = mem->scale;
            Gp_DrawFadeQuad(rgb, 2);
            break;
        case 3:
            if (gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS) {
                arg0->state = 0;
                rgb[0] = rgb[1] = rgb[2] = mem->scale;
                Gp_DrawFadeQuad(rgb, 2);
            } else if (mem->scale >= 9) {
                mem->scale -= 8;
                rgb[0] = rgb[1] = rgb[2] = mem->scale;
                Gp_DrawFadeQuad(rgb, 2);
            } else {
                gRoomEffectState->screenFxFlags &= (u16)~ROOM_EFFECT_SCREEN_FADE_QUAD;
                gRoomEffectState->effectCount--;
                memFree(mem);
                taskKill(arg0);
            }
            break;
    }
}

void Gp_FadeWaveTask(Task* arg0)
{
    RoomEffectState* effectState;
    EffectWork*      mem;
    u16              color;
    u8               rgb[3];

    effectState = gRoomEffectState;
    mem         = arg0->spawnArg2.pointer;
    if (effectState->peFadeMask != arg0->spawnArg1.value) {
        effectState->effectCount--;
        memFree(mem);
        taskKill(arg0);
        return;
    }

    mem->scale += 0x180;
    mem->angle  = rsin(mem->scale) >> 5;
    if (arg0->spawnArg1.value != 0) {
        color  = Gp_FadeQuadColors[(cln(arg0->spawnArg1.value << 12) / 2839) & 7];
        rgb[0] = (mem->angle * ((color >> 8) & 0xF)) >> 3;
        rgb[1] = (mem->angle * ((color >> 4) & 0xF)) >> 3;
        rgb[2] = (mem->angle * (color & 0xF)) >> 3;
        Gp_DrawFadeQuad(rgb, color >> 12);
    }
    if (mem->scale >= 0x700) {
        gRoomEffectState->effectCount--;
        memFree(mem);
        taskKill(arg0);
    }
}

void effectKillTask(void* effectWork, Task* task)
{
    // Retire the counted work before teardown dispatches child exit handlers.
    gRoomEffectState->effectCount--;
    memFree(effectWork);
    taskKill(task);
}

static void Gp_KillState1CTask(Task* arg0)
{
    void* mem;

    mem = arg0->spawnArg2.pointer;
    gRoomEffectState->effectCount--;
    memFree(mem);
    taskKill(arg0);
}

void Gp_PulseState1C(void)
{
    gRoomEffectState->pendingCancelFlags |= ROOM_EFFECT_CANCEL_ALL;
}

static void Gp_AddTpage(P_TAG* arg0, s32 arg1, s32 arg2)
{
    DR_TPAGE* p;

    setSemiTrans(arg0, 1);
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setlen(p, 1);
    p->code[0] = 0xE100020A | ((arg1 & 3) << 5);
    addPrim(gGpuCurrentOt + (arg2 >> 4), p);
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
