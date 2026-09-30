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
#include "gameplay/effect_tasks.h"
#include "effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "actors/actor_510900.h"

#include "actors/actor_800100.h"

#include "main/display.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
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

/// 0x10-byte scratch from `G_SCRATCH_HEAD` used by `Gp_TraceGroundCoord` and
/// `func_800EA1A8`. `pos` is the low halves of the source XYZ. `dir`
/// starts as `(0, 0x1000, 0)`, is rotated by `gGfxViewCoord.workm`, then added
/// onto `pos` and passed to `func_800DE7CC`.
typedef struct _GpRayScratch {
    /* 0x0 */ SVECTOR pos;
    /* 0x8 */ SVECTOR dir;
} GpRayScratch;
STATIC_ASSERT_SIZEOF(GpRayScratch, 0x10);

/* Define BSS before API headers to preserve first-declaration order. */
s32 D_80115720;

s32 D_80115724;

s32 D_80115728;

s32 D_8011572C;

s32 D_80115730;

s32 D_80115734;

s32 D_80115738;

s32 D_8011573C;

GpState1C* Gp_State1C;

s32 D_80115744;

Task* Gp_State1CTask;

s32 D_8011574C;

s32 D_80115750;

s32 D_80115754;

s32 D_80115758;

#include "gameplay/room_effects.h"

extern s32 D_80111B70[20];

extern s32 D_80111BC0[38];

extern s32 D_80111C58[38];

extern s32 D_80111CF0[49];

extern s32 D_80111DB4[33];

static const TaskFuncTable3 D_80097678;

/// Grayscale fade task controlled by `Player_Status.peStateFlags` bit 0.
/// Alternates LCG-selected brightness targets, then fades out and releases
/// its `GpEffWork` when the flag stays clear.
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
    { 0x0, 0xC0, taskKill, { NULL } },                                                     // 0x000
    { 0x0, 0xC0, taskKill, { NULL } },                                                     // 0x001
    { 0x0, 0xC0, taskKill, { NULL } },                                                     // 0x002
    { 0x0, 0xC0, taskKill, { NULL } },                                                     // 0x003
    { 0x0, 0x4F, func_800EA420, { NULL } },                                                // 0x004
    { 0x2, 0x70, func_dryfield_motel_balcony_801802DC, { NULL } },                         // 0x005
    { 0x2, 0x70, func_dryfield_night_gas_station_80181D80, { NULL } },                     // 0x006
    { 0x0, 0x70, Gp_EffCtlTask07, { NULL } },                                              // 0x007
    { 0x2, 0x70, func_dryfield_night_gas_station_801827E4, { NULL } },                     // 0x008
    { 0x2, 0x70, func_dryfield_night_gas_station_801830CC, { NULL } },                     // 0x009
    { 0x2, 0x70, func_dryfield_night_back_street_8017E390, { NULL } },                     // 0x00A
    { 0x2, 0x70, func_hypervelocity_8011F270, { NULL } },                                  // 0x00B
    { 0x2, 0x70, func_hypervelocity_8011D830, { NULL } },                                  // 0x00C
    { 0x2, 0x70, func_hypervelocity_8011F168, { NULL } },                                  // 0x00D
    { 0x2, 0x70, Gp_EffCtlTask0E, { NULL } },                                              // 0x00E
    { 0x2, 0x70, Gp_FadeWaveTask, { NULL } },                                              // 0x00F
    { 0x2, 0x70, func_pyrokinesis_8012EF48, { NULL } },                                    // 0x010
    { 0x2, 0x70, func_pyrokinesis_80131CE4, { NULL } },                                    // 0x011
    { 0x2, 0x70, func_metabolism_8012EF34, { NULL } },                                     // 0x012
    { 0x2, 0x70, func_metabolism_8012F5A0, { NULL } },                                     // 0x013
    { 0x2, 0x70, func_plasma_8012EF34, { NULL } },                                         // 0x014
    { 0x2, 0x70, func_healing_8012EF34, { NULL } },                                        // 0x015
    { 0x2, 0x70, func_healing_8012F494, { NULL } },                                        // 0x016
    { 0x2, 0x70, func_healing_8012F5E4, { NULL } },                                        // 0x017
    { 0x2, 0x70, func_necrosis_8012EF34, { NULL } },                                       // 0x018
    { 0x2, 0x70, func_necrosis_8012F52C, { NULL } },                                       // 0x019
    { 0x2, 0x70, func_necrosis_8012FAF8, { NULL } },                                       // 0x01A
    { 0x2, 0x70, func_combustion_8012EF34, { NULL } },                                     // 0x01B
    { 0x2, 0x70, func_combustion_8012F2BC, { NULL } },                                     // 0x01C
    { 0x2, 0x70, func_acropolis_helicopter_landing_pad_801818F0, { NULL } },               // 0x01D
    { 0x2, 0x70, func_acropolis_west_elevator_hall_8017F7D4, { NULL } },                   // 0x01E
    { 0x2, 0x70, func_acropolis_west_elevator_hall_8017FAE8, { NULL } },                   // 0x01F
    { 0x2, 0x70, func_acropolis_west_elevator_hall_8017FE18, { NULL } },                   // 0x020
    { 0x2, 0x70, func_acropolis_east_elevator_hall_8017F5B4, { NULL } },                   // 0x021
    { 0x2, 0x70, func_acropolis_east_elevator_hall_8017F77C, { NULL } },                   // 0x022
    { 0x2, 0x70, func_8017FAAC, { NULL } },                                                // 0x023
    { 0x2, 0x70, func_hypervelocity_8011D1E8, { NULL } },                                  // 0x024
    { 0x2, 0x70, func_acropolis_west_elevator_hall_8017FFE4, { NULL } },                   // 0x025
    { 0x2, 0x70, func_acropolis_fountain_8017E014, { NULL } },                             // 0x026
    { 0x2, 0x70, func_acropolis_observatory_8017E6F8, { NULL } },                          // 0x027
    { 0x2, 0x70, func_acropolis_observatory_8017E424, { NULL } },                          // 0x028
    { 0x2, 0x70, func_m4a1_hammer_8011D1E0, { NULL } },                                    // 0x029
    { 0x2, 0x70, func_m4a1_pyke_8011D1F8, { NULL } },                                      // 0x02A
    { 0x2, 0x70, Gp_EffCtlTask2B, { NULL } },                                              // 0x02B
    { 0x0, 0xC0, taskKill, { NULL } },                                                     // 0x02C
    { 0x2, 0x70, func_acropolis_square_801823DC, { NULL } },                               // 0x02D
    { 0x2, 0x70, func_8018345C, { NULL } },                                                // 0x02E
    { 0x2, 0x70, func_m4a1_javelin_8011D1E4, { NULL } },                                   // 0x02F
    { 0x2, 0x70, Gp_EffSprTask30, { NULL } },                                              // 0x030
    { 0x0, 0xC0, taskKill, { NULL } },                                                     // 0x031
    { 0x2, 0x70, Gp_EffCtlTask32, { NULL } },                                              // 0x032
    { 0x2, 0x70, func_acropolis_west_elevator_hall_8017F990, { NULL } },                   // 0x033
    { 0x2, 0x70, Gp_EffSprTask34, { NULL } },                                              // 0x034
    { 0x2, 0x70, Gp_EffSprTask35, { NULL } },                                              // 0x035
    { 0x1, 0x70, Gp_EffModelTask, { &D_80111FC8 } },                                       // 0x036
    { 0x1, 0x70, Gp_EffAttachTask37, { &D_8011231C } },                                    // 0x037
    { 0x2, 0x70, func_neo_ark_substation_8017D874, { NULL } },                             // 0x038
    { 0x2, 0x70, func_mist_parking_80184728, { NULL } },                                   // 0x039
    { 0x2, 0x70, func_tonfa_baton_8011D1EC, { NULL } },                                    // 0x03A
    { 0x2, 0x70, Gp_EffCtlTask3B, { NULL } },                                              // 0x03B
    { 0x2, 0x70, func_dryfield_breezeway_80181264, { NULL } },                             // 0x03C
    { 0x2, 0x70, func_dryfield_night_motel_balcony_8017F84C, { NULL } },                   // 0x03D
    { 0x2, 0x70, func_m4a1_bayonet_8011D1E4, { NULL } },                                   // 0x03E
    { 0x2, 0x70, Gp_EffSprTask3F, { NULL } },                                              // 0x03F
    { 0x2, 0x70, func_p229_8011D1DC, { NULL } },                                           // 0x040
    { 0x2, 0x70, func_mp5a5_8011D1E0, { NULL } },                                          // 0x041
    { 0x2, 0x70, Gp_EffSprTask42, { NULL } },                                              // 0x042
    { 0x2, 0x70, func_actor_510900_80131F24, { NULL } },                                   // 0x043
    { 0x2, 0x70, func_actor_510900_801340E8, { NULL } },                                   // 0x044
    { 0x2, 0x70, func_actor_510900_80132D4C, { NULL } },                                   // 0x045
    { 0x2, 0x70, Gp_EffSprTask46, { NULL } },                                              // 0x046
    { 0x2, 0x70, func_acropolis_square_801825DC, { NULL } },                               // 0x047
    { 0x2, 0x70, func_acropolis_security_room_801805A4, { NULL } },                        // 0x048
    { 0x2, 0x70, func_acropolis_security_room_80180E34, { NULL } },                        // 0x049
    { 0x2, 0x70, func_acropolis_promenade_8017E03C, { NULL } },                            // 0x04A
    { 0x2, 0x70, func_acropolis_promenade_8017E634, { NULL } },                            // 0x04B
    { 0x2, 0x70, func_actor_510900_801332EC, { NULL } },                                   // 0x04C
    { 0x2, 0x70, func_acropolis_plaza_8018251C, { NULL } },                                // 0x04D
    { 0x2, 0x70, func_acropolis_fire_escape_8017FF7C, { NULL } },                          // 0x04E
    { 0x2, 0x70, func_acropolis_fire_escape_80180B20, { NULL } },                          // 0x04F
    { 0x2, 0x70, func_dryfield_night_motel_balcony_80180580, { NULL } },                   // 0x050
    { 0x2, 0x70, func_acropolis_forked_road_8017E81C, { NULL } },                          // 0x051
    { 0x2, 0x70, func_actor_510900_8013371C, { NULL } },                                   // 0x052
    { 0x2, 0x70, Gp_EffSprTask53, { NULL } },                                              // 0x053
    { 0x2, 0x70, Gp_EffSprTask54, { NULL } },                                              // 0x054
    { 0x2, 0x70, Gp_EffSprTask55, { NULL } },                                              // 0x055
    { 0x2, 0x70, func_acropolis_promenade_8017E394, { NULL } },                            // 0x056
    { 0x2, 0x70, func_acropolis_promenade_8017ED44, { NULL } },                            // 0x057
    { 0x2, 0x70, func_neo_ark_woodland_path_8017F4A0, { NULL } },                          // 0x058
    { 0x2, 0x70, func_actor_510900_80133C84, { NULL } },                                   // 0x059
    { 0x2, 0x70, func_acropolis_helicopter_landing_pad_8017FA30, { NULL } },               // 0x05A
    { 0x2, 0x70, func_acropolis_helicopter_landing_pad_801802E0, { NULL } },               // 0x05B
    { 0x2, 0x70, Gp_EffSprTask5C, { NULL } },                                              // 0x05C
    { 0x0, 0x70, taskKill, { NULL } },                                                     // 0x05D
    { 0x2, 0x70, func_acropolis_helicopter_landing_pad_80181064, { NULL } },               // 0x05E
    { 0x2, 0x70, func_acropolis_helicopter_landing_pad_80180E40, { NULL } },               // 0x05F
    { 0x2, 0x70, func_acropolis_cafeteria_8017E708, { NULL } },                            // 0x060
    { 0x2, 0x70, func_acropolis_cafeteria_8017EA90, { NULL } },                            // 0x061
    { 0x2, 0x70, func_acropolis_promenade_8017F0BC, { NULL } },                            // 0x062
    { 0x0, 0x70, taskKill, { NULL } },                                                     // 0x063
    { 0x1, 0x70, func_acropolis_cafeteria_8017F390, { &D_acropolis_cafeteria_80184E5C } }, // 0x064
    { 0x2, 0x70, func_actor_510900_80134284, { NULL } },                                   // 0x065
    { 0x1, 0x70, Gp_EffModelTask, { &D_80112200 } },                                       // 0x066
    { 0x1, 0x70, Gp_EffModelTask, { &D_801120E4 } },                                       // 0x067
    { 0x1, 0x70, Gp_EffModelTask, { &D_8011231C } },                                       // 0x068
    { 0x2, 0x70, func_pyrokinesis_80130C54, { NULL } },                                    // 0x069
    { 0x2, 0x70, Gp_EffCtlTask6A, { NULL } },                                              // 0x06A
    { 0x2, 0x70, Gp_EffCtlTask6B, { NULL } },                                              // 0x06B
    { 0x2, 0x70, Gp_EffCtlTask6C, { NULL } },                                              // 0x06C
    { 0x2, 0x70, Gp_EffCtlTask6D, { NULL } },                                              // 0x06D
    { 0x2, 0x70, Gp_EffCtlTask6E, { NULL } },                                              // 0x06E
    { 0x2, 0x70, Gp_EffSprTask6F, { NULL } },                                              // 0x06F
    { 0x2, 0x70, func_800F289C, { NULL } },                                                // 0x070
    { 0x2, 0x70, func_800F4308, { NULL } },                                                // 0x071
    { 0x2, 0x70, Gp_EffSprTask72, { NULL } },                                              // 0x072
    { 0x2, 0x70, func_dryfield_motel_balcony_80180D40, { NULL } },                         // 0x073
    { 0x2, 0x70, func_dryfield_motel_balcony_80181628, { NULL } },                         // 0x074
    { 0x0, 0x70, taskKill, { NULL } },                                                     // 0x075
    { 0x2, 0x70, Gp_EffSprTask76, { NULL } },                                              // 0x076
    { 0x2, 0x70, func_acropolis_sanctuary_8017E00C, { NULL } },                            // 0x077
    { 0x2, 0x70, func_acropolis_sanctuary_8017E134, { NULL } },                            // 0x078
    { 0x2, 0x70, func_acropolis_sanctuary_8017E338, { NULL } },                            // 0x079
    { 0x2, 0x70, func_acropolis_sanctuary_8017EC90, { NULL } },                            // 0x07A
    { 0x2, 0x70, func_acropolis_security_room_80181108, { NULL } },                        // 0x07B
    { 0x2, 0x70, Gp_EffSprTask7C, { NULL } },                                              // 0x07C
    { 0x0, 0x70, taskKill, { NULL } },                                                     // 0x07D
    { 0x2, 0x70, func_dryfield_night_motel_balcony_80181E7C, { NULL } },                   // 0x07E
    { 0x2, 0x70, Gp_EffCtlTask7F, { NULL } },                                              // 0x07F
    { 0x2, 0x70, Gp_EffSprTask80, { NULL } },                                              // 0x080
    { 0x2, 0x70, Gp_EffSprTask81, { NULL } },                                              // 0x081
    { 0x0, 0x70, taskKill, { NULL } },                                                     // 0x082
    { 0x2, 0x70, func_acropolis_patio_8017E100, { NULL } },                                // 0x083
    { 0x2, 0x70, func_acropolis_hallway_8017D828, { NULL } },                              // 0x084
    { 0x2, 0x70, func_acropolis_forked_road_8017E298, { NULL } },                          // 0x085
    { 0x2, 0x70, func_acropolis_roof_garden_8017DCDC, { NULL } },                          // 0x086
    { 0x2, 0x70, func_acropolis_patio_8017E324, { NULL } },                                // 0x087
    { 0x2, 0x70, func_acropolis_fountain_8017DD44, { NULL } },                             // 0x088
    { 0x2, 0x70, func_acropolis_forked_road_8017E410, { NULL } },                          // 0x089
    { 0x2, 0x70, func_acropolis_roof_garden_8017DE90, { NULL } },                          // 0x08A
    { 0x2, 0x70, func_acropolis_sanctuary_8017F4E8, { NULL } },                            // 0x08B
    { 0x2, 0x70, func_acropolis_fire_escape_80180154, { NULL } },                          // 0x08C
    { 0x2, 0x70, Gp_EffSprTask8D, { NULL } },                                              // 0x08D
    { 0x2, 0x70, func_800FF710, { NULL } },                                                // 0x08E
    { 0x2, 0x70, func_acropolis_patio_8017E730, { NULL } },                                // 0x08F
    { 0x2, 0x70, func_acropolis_roof_garden_8017E29C, { NULL } },                          // 0x090
    { 0x1, 0x70, Gp_EffModelTask, { &D_801124B8 } },                                       // 0x091
    { 0x2, 0x70, Gp_EffLineTask92, { NULL } },                                             // 0x092
    { 0x2, 0x70, func_dryfield_night_motel_balcony_801809CC, { NULL } },                   // 0x093
    { 0x2, 0x70, func_dryfield_night_motel_balcony_80181024, { NULL } },                   // 0x094
    { 0x2, 0x70, func_dryfield_night_motel_balcony_8018158C, { NULL } },                   // 0x095
    { 0x2, 0x70, func_acropolis_plaza_80182054, { NULL } },                                // 0x096
    { 0x2, 0x70, func_dryfield_night_back_street_8017EDF4, { NULL } },                     // 0x097
    { 0x2, 0x70, func_acropolis_plaza_801802C0, { NULL } },                                // 0x098
    { 0x2, 0x70, func_acropolis_plaza_801811D0, { NULL } },                                // 0x099
    { 0x2, 0x70, func_800F91AC, { NULL } },                                                // 0x09A
    { 0x2, 0x70, Gp_EffCtlTask9B, { NULL } },                                              // 0x09B
    { 0x2, 0x70, Gp_EffPolyTask9C, { NULL } },                                             // 0x09C
    { 0x2, 0x70, func_acropolis_cafeteria_8017E89C, { NULL } },                            // 0x09D
    { 0x2, 0x70, Gp_EffSprTask9E, { NULL } },                                              // 0x09E
    { 0x2, 0x70, func_dryfield_toilet_8017DEF4, { NULL } },                                // 0x09F
    { 0x2, 0x70, func_acropolis_security_room_801817A4, { NULL } },                        // 0x0A0
    { 0x2, 0x70, func_800ED42C, { NULL } },                                                // 0x0A1
    { 0x2, 0x70, func_dryfield_toilet_8017DCF0, { NULL } },                                // 0x0A2
    { 0x2, 0x70, Gp_EffLineTaskA3, { NULL } },                                             // 0x0A3
    { 0x2, 0x70, Gp_EffTileTaskA4, { NULL } },                                             // 0x0A4
    { 0x2, 0x70, Gp_EffCtlTaskA5, { NULL } },                                              // 0x0A5
    { 0x2, 0x70, Gp_EffCtlTaskA6, { NULL } },                                              // 0x0A6
    { 0x2, 0x70, Gp_EffSprTaskA7, { NULL } },                                              // 0x0A7
    { 0x2, 0x70, func_800FAA14, { NULL } },                                                // 0x0A8
    { 0x2, 0x70, func_combustion_8012F888, { NULL } },                                     // 0x0A9
    { 0x2, 0x70, func_shelter_b4_reservoir_801813F0, { NULL } },                           // 0x0AA
    { 0x2, 0x70, func_lifedrain_8012EF48, { NULL } },                                      // 0x0AB
    { 0x2, 0x70, Gp_EffCtlTaskAC, { NULL } },                                              // 0x0AC
    { 0x2, 0x70, func_lifedrain_8012F9A8, { NULL } },                                      // 0x0AD
    { 0x2, 0x70, Gp_EffCtlTaskAE, { NULL } },                                              // 0x0AE
    { 0x2, 0x70, func_lifedrain_8012FAF8, { NULL } },                                      // 0x0AF
    { 0x2, 0x70, func_acropolis_bridge_8017F868, { NULL } },                               // 0x0B0
    { 0x2, 0x70, func_acropolis_bridge_801812F4, { NULL } },                               // 0x0B1
    { 0x2, 0x70, func_acropolis_bridge_801819C8, { NULL } },                               // 0x0B2
    { 0x2, 0x70, func_acropolis_bridge_80181D28, { NULL } },                               // 0x0B3
    { 0x2, 0x70, func_acropolis_bridge_80180320, { NULL } },                               // 0x0B4
    { 0x2, 0x70, func_acropolis_bridge_8018063C, { NULL } },                               // 0x0B5
    { 0x2, 0x70, func_acropolis_bridge_8018099C, { NULL } },                               // 0x0B6
    { 0x2, 0x70, func_acropolis_bridge_80180CC0, { NULL } },                               // 0x0B7
    { 0x2, 0x70, func_acropolis_bridge_80180FF0, { NULL } },                               // 0x0B8
    { 0x2, 0x70, func_acropolis_bridge_80182694, { NULL } },                               // 0x0B9
    { 0x2, 0x70, func_acropolis_bridge_80182AF8, { NULL } },                               // 0x0BA
    { 0x2, 0x70, func_8011D1E0, { NULL } },                                                // 0x0BB
    { 0x2, 0x70, func_acropolis_bridge_80182394, { NULL } },                               // 0x0BC
    { 0x2, 0x70, func_8011D1E0, { NULL } },                                                // 0x0BD
    { 0x2, 0x70, func_inferno_8012EF88, { NULL } },                                        // 0x0BE
    { 0x2, 0x70, func_dryfield_underpass_8017DE30, { NULL } },                             // 0x0BF
    { 0x2, 0x70, func_apobiosis_8012EF4C, { NULL } },                                      // 0x0C0
    { 0x2, 0x70, Gp_EffCtlTaskC1, { NULL } },                                              // 0x0C1
    { 0x2, 0x70, func_dryfield_gas_station_80181A78, { NULL } },                           // 0x0C2
    { 0x2, 0x70, func_dryfield_main_street_8017E4B0, { NULL } },                           // 0x0C3
    { 0x2, 0x70, func_dryfield_general_store_8017E150, { NULL } },                         // 0x0C4
    { 0x2, 0x70, func_dryfield_back_street_8017D970, { NULL } },                           // 0x0C5
    { 0x2, 0x70, func_dryfield_souvenir_shop_8017DFD4, { NULL } },                         // 0x0C6
    { 0x2, 0x70, func_dryfield_warehouse_8017F494, { NULL } },                             // 0x0C7
    { 0x2, 0x70, func_dryfield_dilapidated_house_80183BF8, { NULL } },                     // 0x0C8
    { 0x2, 0x70, func_dryfield_motel_room_1_8017E0A0, { NULL } },                          // 0x0C9
    { 0x2, 0x70, func_dryfield_motel_room_2_8017D6B4, { NULL } },                          // 0x0CA
    { 0x2, 0x70, func_antibody_8012EF34, { NULL } },                                       // 0x0CB
    { 0x2, 0x70, func_energyshot_8012EF34, { NULL } },                                     // 0x0CC
    { 0x2, 0x70, func_dryfield_parking_lot_8017DBAC, { NULL } },                           // 0x0CD
    { 0x2, 0x70, func_dryfield_toilet_8017E64C, { NULL } },                                // 0x0CE
    { 0x2, 0x70, func_energyball_8012EF48, { NULL } },                                     // 0x0CF
    { 0x2, 0x70, func_dryfield_saloon_g_r_8017DA70, { NULL } },                            // 0x0D0
    { 0x2, 0x70, func_dryfield_g_r_kitchen_8017EB04, { NULL } },                           // 0x0D1
    { 0x2, 0x70, func_dryfield_water_tower_80180348, { NULL } },                           // 0x0D2
    { 0x2, 0x70, func_dryfield_water_tank_8017F084, { NULL } },                            // 0x0D3
    { 0x2, 0x70, func_dryfield_breezeway_8017FF7C, { NULL } },                             // 0x0D4
    { 0x2, 0x70, func_dryfield_factory_801825F0, { NULL } },                               // 0x0D5
    { 0x2, 0x70, func_dryfield_garage_8017DC68, { NULL } },                                // 0x0D6
    { 0x2, 0x70, func_dryfield_driveway_8017DE6C, { NULL } },                              // 0x0D7
    { 0x2, 0x70, func_dryfield_junk_yard_8017DD0C, { NULL } },                             // 0x0D8
    { 0x2, 0x70, func_dryfield_trailer_coach_801838DC, { NULL } },                         // 0x0D9
    { 0x2, 0x70, func_inferno_8012F530, { NULL } },                                        // 0x0DA
    { 0x2, 0x70, func_dryfield_motel_balcony_8017DC28, { NULL } },                         // 0x0DB
    { 0x2, 0x70, func_dryfield_motel_room_6_80182978, { NULL } },                          // 0x0DC
    { 0x2, 0x70, func_energyshot_8012FFB8, { NULL } },                                     // 0x0DD
    { 0x2, 0x70, func_dryfield_water_hole_8017E040, { NULL } },                            // 0x0DE
    { 0x2, 0x70, func_dryfield_cellar_8017DAEC, { NULL } },                                // 0x0DF
    { 0x2, 0x70, Gp_EffSprTaskE0, { NULL } },                                              // 0x0E0
    { 0x2, 0x70, Gp_EffSprTaskE1, { NULL } },                                              // 0x0E1
    { 0x2, 0x70, Gp_EffSprTaskE2, { NULL } },                                              // 0x0E2
    { 0x2, 0x70, Gp_EffCtlTaskE3, { NULL } },                                              // 0x0E3
    { 0x2, 0x70, func_dryfield_night_back_street_8017F6DC, { NULL } },                     // 0x0E4
    { 0x2, 0x70, func_dryfield_night_junk_yard_8017E5C8, { NULL } },                       // 0x0E5
    { 0x2, 0x70, func_dryfield_night_junk_yard_8017F02C, { NULL } },                       // 0x0E6
    { 0x2, 0x70, func_dryfield_night_junk_yard_8017F914, { NULL } },                       // 0x0E7
    { 0x2, 0x70, func_800EC47C, { NULL } },                                                // 0x0E8
    { 0x2, 0x70, func_mine_mesa_8017F230, { NULL } },                                      // 0x0E9
    { 0x2, 0x70, func_lifedrain_801308C0, { NULL } },                                      // 0x0EA
    { 0x2, 0x70, func_mine_mesa_8017FC94, { NULL } },                                      // 0x0EB
    { 0x2, 0x70, func_mine_mesa_8018057C, { NULL } },                                      // 0x0EC
    { 0x2, 0x70, func_shelter_b4_lower_sewer_8017FEB0, { NULL } },                         // 0x0ED
    { 0x2, 0x70, func_shelter_b4_lower_sewer_80180914, { NULL } },                         // 0x0EE
    { 0x2, 0x70, func_shelter_b4_lower_sewer_801811FC, { NULL } },                         // 0x0EF
    { 0x2, 0x70, func_shelter_b4_upper_sewer_80182734, { NULL } },                         // 0x0F0
    { 0x2, 0x70, func_shelter_b4_upper_sewer_80183198, { NULL } },                         // 0x0F1
    { 0x2, 0x70, func_shelter_b4_upper_sewer_80183A80, { NULL } },                         // 0x0F2
    { 0x2, 0x70, Gp_EffCtlTaskF3, { NULL } },                                              // 0x0F3
    { 0x2, 0x70, Gp_EffCtlTaskF4, { NULL } },                                              // 0x0F4
    { 0x2, 0x70, func_antibody_8012F734, { NULL } },                                       // 0x0F5
    { 0x2, 0x70, func_pyrokinesis_801311B8, { NULL } },                                    // 0x0F6
    { 0x2, 0x70, func_apobiosis_8012FE10, { NULL } },                                      // 0x0F7
    { 0x2, 0x70, func_energyball_8012F180, { NULL } },                                     // 0x0F8
    { 0x2, 0x70, func_energyball_8013107C, { NULL } },                                     // 0x0F9
    { 0x2, 0x70, func_neo_ark_forest_zone_8017E3C0, { NULL } },                            // 0x0FA
    { 0x2, 0x70, func_neo_ark_forest_zone_8017DC20, { NULL } },                            // 0x0FB
    { 0x2, 0x70, func_pyrokinesis_8012FAC8, { NULL } },                                    // 0x0FC
    { 0x2, 0x70, func_dryfield_water_hole_8017EC90, { NULL } },                            // 0x0FD
    { 0x2, 0x70, func_dryfield_water_hole_8017F118, { NULL } },                            // 0x0FE
    { 0x2, 0x70, func_dryfield_night_water_hole_8017F254, { NULL } },                      // 0x0FF
    { 0x2, 0x70, func_dryfield_night_gas_station_80180E9C, { NULL } },                     // 0x100
    { 0x2, 0x70, func_dryfield_night_main_street_8017E484, { NULL } },                     // 0x101
    { 0x2, 0x70, func_dryfield_night_general_store_8017E6C8, { NULL } },                   // 0x102
    { 0x2, 0x70, func_dryfield_night_back_street_8017D7E0, { NULL } },                     // 0x103
    { 0x2, 0x70, func_dryfield_night_souvenir_shop_8017DFF4, { NULL } },                   // 0x104
    { 0x2, 0x70, func_dryfield_night_warehouse_8017E778, { NULL } },                       // 0x105
    { 0x2, 0x70, func_dryfield_night_dilapidated_house_8017E670, { NULL } },               // 0x106
    { 0x2, 0x70, func_dryfield_night_motel_room_1_8017D9B0, { NULL } },                    // 0x107
    { 0x2, 0x70, func_dryfield_night_motel_room_2_8017D990, { NULL } },                    // 0x108
    { 0x2, 0x70, func_dryfield_night_motel_room_3_8017D9B4, { NULL } },                    // 0x109
    { 0x2, 0x70, func_dryfield_night_motel_room_4_8017D990, { NULL } },                    // 0x10A
    { 0x2, 0x70, func_dryfield_night_parking_lot_8017DC88, { NULL } },                     // 0x10B
    { 0x2, 0x70, func_dryfield_night_toilet_8017D9F8, { NULL } },                          // 0x10C
    { 0x2, 0x70, func_dryfield_night_motel_lobby_801812F8, { NULL } },                     // 0x10D
    { 0x2, 0x70, func_dryfield_night_saloon_g_r_8017E6C8, { NULL } },                      // 0x10E
    { 0x2, 0x70, func_dryfield_night_g_r_kitchen_8017E1E4, { NULL } },                     // 0x10F
    { 0x2, 0x70, func_dryfield_night_water_tower_8017DB80, { NULL } },                     // 0x110
    { 0x2, 0x70, func_dryfield_night_water_tank_8017DD8C, { NULL } },                      // 0x111
    { 0x2, 0x70, func_dryfield_night_breezeway_8017E5BC, { NULL } },                       // 0x112
    { 0x2, 0x70, func_dryfield_night_factory_801825F0, { NULL } },                         // 0x113
    { 0x2, 0x70, func_dryfield_night_garage_80181518, { NULL } },                          // 0x114
    { 0x2, 0x70, func_dryfield_night_driveway_8017E5CC, { NULL } },                        // 0x115
    { 0x2, 0x70, func_dryfield_night_junk_yard_8017DA14, { NULL } },                       // 0x116
    { 0x2, 0x70, func_dryfield_night_trailer_coach_80182924, { NULL } },                   // 0x117
    { 0x2, 0x70, func_dryfield_night_motel_room_5_8017D9A4, { NULL } },                    // 0x118
    { 0x2, 0x70, func_dryfield_night_motel_balcony_8017E554, { NULL } },                   // 0x119
    { 0x2, 0x70, func_dryfield_night_motel_room_6_80182AE0, { NULL } },                    // 0x11A
    { 0x2, 0x70, func_dryfield_night_motel_loft_8017DB64, { NULL } },                      // 0x11B
    { 0x2, 0x70, func_dryfield_night_water_hole_8017E6D0, { NULL } },                      // 0x11C
    { 0x2, 0x70, func_dryfield_night_cellar_8017DA28, { NULL } },                          // 0x11D
    { 0x2, 0x70, func_dryfield_night_underpass_8017DC3C, { NULL } },                       // 0x11E
    { 0x2, 0x70, func_dryfield_night_water_hole_8017F6DC, { NULL } },                      // 0x11F
    { 0x2, 0x70, func_mine_mesa_8017ED08, { NULL } },                                      // 0x120
    { 0x2, 0x70, func_mine_cavern_8017E474, { NULL } },                                    // 0x121
    { 0x2, 0x70, func_mine_tunnel_entrance_8017D720, { NULL } },                           // 0x122
    { 0x2, 0x70, func_mine_tunnel_8017D7D4, { NULL } },                                    // 0x123
    { 0x2, 0x70, func_mine_gorge_8017D9F8, { NULL } },                                     // 0x124
    { 0x2, 0x70, func_mine_refuge_80181454, { NULL } },                                    // 0x125
    { 0x2, 0x70, func_mine_forked_tunnel_8017E78C, { NULL } },                             // 0x126
    { 0x2, 0x70, func_mine_secret_passage_8017D9D4, { NULL } },                            // 0x127
    { 0x2, 0x70, func_shelter_b1_elevator_hall_8017DC80, { NULL } },                       // 0x128
    { 0x2, 0x70, func_shelter_b1_south_maintenance_walkway_8017DA8C, { NULL } },           // 0x129
    { 0x2, 0x70, func_shelter_b1_storeroom_8017D7EC, { NULL } },                           // 0x12A
    { 0x2, 0x70, func_shelter_b1_north_maintenance_walkway_8017DBC8, { NULL } },           // 0x12B
    { 0x2, 0x70, func_shelter_b1_armory_801807E4, { NULL } },                              // 0x12C
    { 0x2, 0x70, func_shelter_b1_sleeping_quarters_8017D8E0, { NULL } },                   // 0x12D
    { 0x2, 0x70, func_shelter_b1_main_corridor_8017DDF0, { NULL } },                       // 0x12E
    { 0x2, 0x70, func_shelter_b1_sterilization_room_8018188C, { NULL } },                  // 0x12F
    { 0x2, 0x70, func_shelter_b1_pod_access_tunnel_8017E7D4, { NULL } },                   // 0x130
    { 0x2, 0x70, func_shelter_b1_control_room_8017F150, { NULL } },                        // 0x131
    { 0x2, 0x70, func_shelter_b1_access_tunnel_8017DD60, { NULL } },                       // 0x132
    { 0x2, 0x70, func_shelter_b1_underground_parking_80184A18, { NULL } },                 // 0x133
    { 0x2, 0x70, func_shelter_b1_golem_freezer_1_8017DA7C, { NULL } },                     // 0x134
    { 0x2, 0x70, func_shelter_b2_pod_bottom_8017D760, { NULL } },                          // 0x135
    { 0x2, 0x70, func_shelter_b1_pod_service_gantry_8017FA7C, { NULL } },                  // 0x136
    { 0x2, 0x70, func_shelter_b1_transfer_tunnel_8017D6D0, { NULL } },                     // 0x137
    { 0x2, 0x70, func_shelter_b1_control_room_access_tunnel_8017E1BC, { NULL } },          // 0x138
    { 0x2, 0x70, func_shelter_b2_elevator_8017DB70, { NULL } },                            // 0x139
    { 0x2, 0x70, func_shelter_b2_elevator_hall_8017DD60, { NULL } },                       // 0x13A
    { 0x2, 0x70, func_shelter_b2_south_maintenance_walkway_8017DCC4, { NULL } },           // 0x13B
    { 0x2, 0x70, func_shelter_b2_operating_room_8017DDB8, { NULL } },                      // 0x13C
    { 0x2, 0x70, func_shelter_b2_north_maintenance_walkway_8017DDE8, { NULL } },           // 0x13D
    { 0x2, 0x70, func_shelter_b2_laboratory_80180548, { NULL } },                          // 0x13E
    { 0x2, 0x70, func_shelter_b2_breeding_room_8017D898, { NULL } },                       // 0x13F
    { 0x2, 0x70, func_shelter_b2_main_corridor_8017EC34, { NULL } },                       // 0x140
    { 0x2, 0x70, func_shelter_b2_septic_tank_8017EB7C, { NULL } },                         // 0x141
    { 0x2, 0x70, func_shelter_b2_pod_access_tunnel_8017DC6C, { NULL } },                   // 0x142
    { 0x2, 0x70, func_shelter_b3_dumping_hole_80183F84, { NULL } },                        // 0x143
    { 0x2, 0x70, func_shelter_b3_garbage_incinerator_8018110C, { NULL } },                 // 0x144
    { 0x2, 0x70, func_shelter_b3_incinerator_control_room_8017FD10, { NULL } },            // 0x145
    { 0x2, 0x70, func_shelter_b3_elevator_hall_8017DE70, { NULL } },                       // 0x146
    { 0x2, 0x70, func_shelter_b4_lower_sewer_8017E400, { NULL } },                         // 0x147
    { 0x2, 0x70, func_shelter_b4_upper_sewer_8017E5F8, { NULL } },                         // 0x148
    { 0x2, 0x70, func_shelter_b4_reservoir_8017FB84, { NULL } },                           // 0x149
    { 0x2, 0x70, func_shelter_b4_water_supply_8017EE54, { NULL } },                        // 0x14A
    { 0x2, 0x70, func_shelter_r47_801858BC, { NULL } },                                    // 0x14B
    { 0x2, 0x70, func_shelter_r48_8017E3B8, { NULL } },                                    // 0x14C
    { 0x2, 0x70, func_shelter_1f_airlock_8017D6D0, { NULL } },                             // 0x14D
    { 0x2, 0x70, func_neo_ark_observatory_80180124, { NULL } },                            // 0x14E
    { 0x2, 0x70, func_neo_ark_eve_access_tunnel_8017E15C, { NULL } },                      // 0x14F
    { 0x2, 0x70, func_neo_ark_eve_elevator_8017D71C, { NULL } },                           // 0x150
    { 0x2, 0x70, func_neo_ark_north_promenade_8017D720, { NULL } },                        // 0x151
    { 0x2, 0x70, func_neo_ark_submarine_tunnel_8017F48C, { NULL } },                       // 0x152
    { 0x2, 0x70, func_neo_ark_pavilion_8017FC10, { NULL } },                               // 0x153
    { 0x2, 0x70, func_neo_ark_island_8017FB2C, { NULL } },                                 // 0x154
    { 0x2, 0x70, func_neo_ark_garden_8017EA9C, { NULL } },                                 // 0x155
    { 0x2, 0x70, func_neo_ark_power_plant_2_8017D8AC, { NULL } },                          // 0x156
    { 0x2, 0x70, func_neo_ark_power_plant_1_8017DA18, { NULL } },                          // 0x157
    { 0x2, 0x70, func_neo_ark_savanna_zone_8017D9AC, { NULL } },                           // 0x158
    { 0x2, 0x70, func_neo_ark_south_promenade_8017D6D0, { NULL } },                        // 0x159
    { 0x2, 0x70, func_neo_ark_altar_8017EF84, { NULL } },                                  // 0x15A
    { 0x2, 0x70, func_neo_ark_shrine_8017F8DC, { NULL } },                                 // 0x15B
    { 0x2, 0x70, func_shelter_b6_nursery_801800A0, { NULL } },                             // 0x15C
    { 0x2, 0x70, func_shelter_b6_growth_room_8017D9D8, { NULL } },                         // 0x15D
    { 0x2, 0x70, func_shelter_b6_corridor_8017E238, { NULL } },                            // 0x15E
    { 0x2, 0x70, func_shelter_b6_training_room_8017DDE8, { NULL } },                       // 0x15F
    { 0x2, 0x70, func_neo_ark_bridge_8017E954, { NULL } },                                 // 0x160
    { 0x2, 0x70, func_neo_ark_woodland_path_8017EA08, { NULL } },                          // 0x161
    { 0x2, 0x70, func_shelter_r49_8017D9D0, { NULL } },                                    // 0x162
    { 0x2, 0x70, func_shelter_1f_parking_garage_8017DF6C, { NULL } },                      // 0x163
    { 0x2, 0x70, func_shelter_1f_vehicular_airlock_8017DAA0, { NULL } },                   // 0x164
    { 0x2, 0x70, func_shelter_1f_bulwark_8017E2A4, { NULL } },                             // 0x165
    { 0x2, 0x70, func_shelter_1f_heliport_80180B4C, { NULL } },                            // 0x166
    { 0x2, 0x70, func_shelter_1f_guardroom_8017DA28, { NULL } },                           // 0x167
    { 0x2, 0x70, func_neo_ark_r26_8017D778, { NULL } },                                    // 0x168
    { 0x2, 0x70, func_shelter_1f_tent_8017FE10, { NULL } },                                // 0x169
    { 0x2, 0x70, func_shelter_b2_main_corridor_8017EF24, { NULL } },                       // 0x16A
    { 0x2, 0x70, func_shelter_b2_main_corridor_8017F3AC, { NULL } },                       // 0x16B
    { 0x2, 0x70, func_shelter_b2_septic_tank_8017F040, { NULL } },                         // 0x16C
    { 0x2, 0x70, func_shelter_b2_septic_tank_8017F4C8, { NULL } },                         // 0x16D
    { 0x2, 0x70, func_shelter_b4_lower_sewer_8017EEE4, { NULL } },                         // 0x16E
    { 0x2, 0x70, func_shelter_b4_lower_sewer_8017F36C, { NULL } },                         // 0x16F
    { 0x2, 0x70, func_shelter_b4_upper_sewer_8017E8B8, { NULL } },                         // 0x170
    { 0x2, 0x70, func_shelter_b4_upper_sewer_8017ED40, { NULL } },                         // 0x171
    { 0x2, 0x70, func_shelter_b4_reservoir_801803DC, { NULL } },                           // 0x172
    { 0x2, 0x70, func_shelter_b4_reservoir_80180864, { NULL } },                           // 0x173
    { 0x2, 0x70, func_shelter_b4_water_supply_8017F24C, { NULL } },                        // 0x174
    { 0x2, 0x70, func_shelter_b4_water_supply_8017F6D4, { NULL } },                        // 0x175
    { 0x2, 0x70, func_neo_ark_pavilion_8017EC4C, { NULL } },                               // 0x176
    { 0x2, 0x70, func_neo_ark_pavilion_8017F0CC, { NULL } },                               // 0x177
    { 0x2, 0x70, func_neo_ark_island_8017EB68, { NULL } },                                 // 0x178
    { 0x2, 0x70, func_neo_ark_island_8017EFE8, { NULL } },                                 // 0x179
    { 0x2, 0x70, func_neo_ark_bridge_8017EF70, { NULL } },                                 // 0x17A
    { 0x2, 0x70, func_neo_ark_bridge_8017F3F8, { NULL } },                                 // 0x17B
    { 0x2, 0x70, func_acropolis_roof_garden_8017F10C, { NULL } },                          // 0x17C
    { 0x2, 0x70, func_shelter_b1_sterilization_room_801823D8, { NULL } },                  // 0x17D
    { 0x2, 0x70, func_dryfield_night_r08_8017D718, { NULL } },                             // 0x17E
    { 0x2, 0x70, func_m4a1_pyke_8011D7D4, { NULL } },                                      // 0x17F
    { 0x2, 0x70, func_actor_800100_80161F20, { NULL } },                                   // 0x180
    { 0x2, 0x70, func_actor_800100_801624F0, { NULL } },                                   // 0x181
    { 0x2, 0x70, func_m4a1_hammer_8011DD08, { NULL } },                                    // 0x182
    { 0x2, 0x70, func_m4a1_javelin_8011F4E8, { NULL } },                                   // 0x183
    { 0x2, 0x70, func_actor_510900_8013482C, { NULL } },                                   // 0x184
    { 0x2, 0x70, func_actor_510900_801346D4, { NULL } },                                   // 0x185
    { 0x2, 0x70, func_gunblade_8011D1E4, { NULL } },                                       // 0x186
    { 0x2, 0x70, func_neo_ark_woodland_path_8017F928, { NULL } },                          // 0x187
    { 0x2, 0x70, func_dryfield_dilapidated_house_80181F08, { NULL } },                     // 0x188
    { 0x2, 0x70, func_shelter_r48_8017E4C4, { NULL } },                                    // 0x189
    { 0x2, 0x70, func_shelter_r48_8017EC18, { NULL } },                                    // 0x18A
    { 0x2, 0x70, func_shelter_r48_8017F6C0, { NULL } },                                    // 0x18B
    { 0x2, 0x70, func_shelter_r48_80180210, { NULL } },                                    // 0x18C
    { 0x2, 0x70, func_shelter_r48_8017E704, { NULL } },                                    // 0x18D
    { 0x2, 0x70, func_shelter_r48_8017E9B8, { NULL } },                                    // 0x18E
    { 0x2, 0x70, func_shelter_r48_8017EFD8, { NULL } },                                    // 0x18F
    { 0x2, 0x70, func_shelter_r48_801810B0, { NULL } },                                    // 0x190
    { 0x2, 0x70, func_shelter_r48_8018147C, { NULL } },                                    // 0x191
    { 0x2, 0x70, func_neo_ark_submarine_gallery_8017EFEC, { NULL } },                      // 0x192
    { 0x2, 0x70, func_neo_ark_submarine_gallery_8017F288, { NULL } },                      // 0x193
    { 0x2, 0x70, func_neo_ark_submarine_gallery_8017F710, { NULL } },                      // 0x194
    { 0x2, 0x70, func_shelter_r48_80181704, { NULL } },                                    // 0x195
    { 0x2, 0x70, func_shelter_b3_garbage_incinerator_80182368, { NULL } },                 // 0x196
    { 0x2, 0x70, func_pepper_spray_8012EF34, { NULL } },                                   // 0x197
    { 0x2, 0x70, ofudaEffectTask, { NULL } },                                              // 0x198
    { 0x2, 0x70, func_shelter_b3_dumping_hole_8018521C, { NULL } },                        // 0x199
    { 0x2, 0x70, func_shelter_b3_dumping_hole_80186218, { NULL } },                        // 0x19A
    { 0x2, 0x70, func_shelter_b3_dumping_hole_80186D4C, { NULL } },                        // 0x19B
    { 0x2, 0x70, func_shelter_b3_garbage_incinerator_80183364, { NULL } },                 // 0x19C
    { 0x2, 0x70, flareEffectTask, { NULL } },                                              // 0x19D
    { 0x2, 0x70, flareSparkTask, { NULL } },                                               // 0x19E
    { 0x2, 0x70, func_mist_shooting_gallery_801811EC, { NULL } },                          // 0x19F
    { 0x2, 0x70, func_combustion_801308E0, { NULL } },                                     // 0x1A0
    { 0x2, 0x70, func_shelter_b6_growth_room_8017E564, { NULL } },                         // 0x1A1
    { 0x2, 0x70, func_shelter_b6_growth_room_8017EAC8, { NULL } },                         // 0x1A2
    { 0x1, 0x70, func_shelter_b6_nursery_80181314, { &D_shelter_b6_nursery_801852D0 } },   // 0x1A3
    { 0x2, 0x70, func_shelter_b6_nursery_80181820, { NULL } },                             // 0x1A4
    { 0x2, 0x70, func_shelter_b6_nursery_80182730, { NULL } },                             // 0x1A5
    { 0x2, 0x70, func_shelter_b1_golem_freezer_1_8017DFFC, { NULL } },                     // 0x1A6
    { 0x2, 0x70, func_shelter_b6_training_room_8017EE70, { NULL } },                       // 0x1A7
    { 0x2, 0x70, func_shelter_b6_training_room_8017F8B8, { NULL } },                       // 0x1A8
    { 0x2, 0x70, func_shelter_b6_training_room_80180DB4, { NULL } },                       // 0x1A9
    { 0x2, 0x70, func_shelter_b6_training_room_801811AC, { NULL } },                       // 0x1AA
    { 0x2, 0x70, func_shelter_b6_training_room_80181A3C, { NULL } },                       // 0x1AB
    { 0x2, 0x70, func_shelter_b6_training_room_8018245C, { NULL } },                       // 0x1AC
    { 0x2, 0x70, func_shelter_b6_training_room_801825C0, { NULL } },                       // 0x1AD
    { 0x2, 0x70, func_shelter_b6_training_room_801826E0, { NULL } },                       // 0x1AE
    { 0x2, 0x70, func_shelter_b6_training_room_80182804, { NULL } },                       // 0x1AF
    { 0x2, 0x70, func_dryfield_night_motel_loft_8017E090, { NULL } },                      // 0x1B0
    { 0x2, 0x70, func_dryfield_main_street_8017E830, { NULL } },                           // 0x1B1
    { 0x2, 0x70, func_dryfield_night_main_street_8017F3B0, { NULL } },                     // 0x1B2
    { 0x2, 0x70, func_shelter_b2_pod_bottom_8017D850, { NULL } },                          // 0x1B3
    { 0x2, 0x70, func_shelter_b1_pod_service_gantry_8017D8F4, { NULL } },                  // 0x1B4
    { 0x2, 0x70, func_shelter_b2_pod_access_tunnel_8017E6E0, { NULL } },                   // 0x1B5
    { 0x2, 0x70, func_dryfield_r08_8017D5F8, { NULL } },                                   // 0x1B6
    { 0x2, 0x70, func_dryfield_r08_8017D8B4, { NULL } },                                   // 0x1B7
    { 0x2, 0x70, func_dryfield_r08_8017D8B4, { NULL } },                                   // 0x1B8
    { 0x2, 0x70, func_shelter_b2_pod_bottom_80181B48, { NULL } },                          // 0x1B9
    { 0x2, 0x70, func_shelter_b2_pod_bottom_8017EC78, { NULL } },                          // 0x1BA
    { 0x2, 0x70, func_shelter_b2_pod_bottom_8017F448, { NULL } },                          // 0x1BB
    { 0x2, 0x70, func_shelter_b2_pod_bottom_8018016C, { NULL } },                          // 0x1BC
    { 0x2, 0x70, func_mist_shooting_gallery_80182064, { NULL } },                          // 0x1BD
    { 0x2, 0x70, func_shelter_b1_pod_service_gantry_8017E880, { NULL } },                  // 0x1BE
    { 0x2, 0x70, func_shelter_b2_pod_bottom_80180898, { NULL } },                          // 0x1BF
    { 0x2, 0x70, func_shelter_b2_pod_bottom_80180F10, { NULL } },                          // 0x1C0
    { 0x2, 0x70, func_neo_ark_woodland_path_8017ED00, { NULL } },                          // 0x1C1
    { 0x2, 0x70, func_shelter_1f_bulwark_8017E38C, { NULL } },                             // 0x1C2
    { 0x2, 0x70, func_shelter_1f_bulwark_8017EDF0, { NULL } },                             // 0x1C3
    { 0x2, 0x70, func_shelter_1f_bulwark_8017F6D8, { NULL } },                             // 0x1C4
    { 0x2, 0x70, func_neo_ark_pyramid_8017DBF0, { NULL } },                                // 0x1C5
    { 0x2, 0x70, func_shelter_b1_pod_service_gantry_8017F8C8, { NULL } },                  // 0x1C6
    { 0x2, 0x70, func_dryfield_night_r08_8017E5B0, { NULL } },                             // 0x1C7
    { 0x2, 0x70, func_shelter_b1_elevator_hall_80180D18, { NULL } },                       // 0x1C8
    { 0x2, 0x70, func_shelter_b1_south_maintenance_walkway_8017E760, { NULL } },           // 0x1C9
    { 0x2, 0x70, func_shelter_b1_storeroom_80180DCC, { NULL } },                           // 0x1CA
    { 0x2, 0x70, func_shelter_b1_north_maintenance_walkway_80180EDC, { NULL } },           // 0x1CB
    { 0x2, 0x70, func_shelter_b1_main_corridor_801810F8, { NULL } },                       // 0x1CC
    { 0x2, 0x70, func_shelter_b1_pod_access_tunnel_8017F138, { NULL } },                   // 0x1CD
    { 0x2, 0x70, func_shelter_b1_transfer_tunnel_8018092C, { NULL } },                     // 0x1CE
    { 0x2, 0x70, func_shelter_b1_control_room_access_tunnel_8017E2D8, { NULL } },          // 0x1CF
    { 0x2, 0x70, func_shelter_b2_elevator_hall_801817FC, { NULL } },                       // 0x1D0
    { 0x2, 0x70, func_shelter_b2_south_maintenance_walkway_8017E99C, { NULL } },           // 0x1D1
    { 0x2, 0x70, func_shelter_b2_north_maintenance_walkway_80181BB4, { NULL } },           // 0x1D2
    { 0x2, 0x70, func_shelter_b2_main_corridor_8018094C, { NULL } },                       // 0x1D3
    { 0x2, 0x70, func_shelter_b2_septic_tank_80180BE0, { NULL } },                         // 0x1D4
    { 0x2, 0x70, func_shelter_b2_pod_access_tunnel_80181C2C, { NULL } },                   // 0x1D5
    { 0x2, 0x70, func_shelter_1f_parking_garage_8017EC0C, { NULL } },                      // 0x1D6
    { 0x2, 0x70, func_shelter_1f_vehicular_airlock_8017ECBC, { NULL } },                   // 0x1D7
    { 0x2, 0x70, func_neo_ark_north_promenade_8017FDD4, { NULL } },                        // 0x1D8
    { 0x2, 0x70, func_neo_ark_forest_zone_8017E420, { NULL } },                            // 0x1D9
    { 0x2, 0x70, func_neo_ark_pavilion_8017FCB0, { NULL } },                               // 0x1DA
    { 0x2, 0x70, func_neo_ark_island_8017FB9C, { NULL } },                                 // 0x1DB
    { 0x2, 0x70, func_neo_ark_power_plant_2_8017DDF4, { NULL } },                          // 0x1DC
    { 0x2, 0x70, func_neo_ark_savanna_zone_8017DA0C, { NULL } },                           // 0x1DD
    { 0x2, 0x70, func_neo_ark_south_promenade_8017D720, { NULL } },                        // 0x1DE
    { 0x2, 0x70, func_neo_ark_shrine_8017FEA0, { NULL } },                                 // 0x1DF
    { 0x2, 0x70, func_shelter_b6_nursery_80182D28, { NULL } },                             // 0x1E0
    { 0x2, 0x70, func_neo_ark_bridge_8017FF84, { NULL } },                                 // 0x1E1
    { 0x2, 0x70, func_neo_ark_pyramid_8017DC50, { NULL } },                                // 0x1E2
    { 0x2, 0x70, func_dryfield_night_r08_8017F014, { NULL } },                             // 0x1E3
    { 0x2, 0x70, func_shelter_b1_elevator_hall_8018177C, { NULL } },                       // 0x1E4
    { 0x2, 0x70, func_shelter_b1_south_maintenance_walkway_8017F1C4, { NULL } },           // 0x1E5
    { 0x2, 0x70, func_shelter_b1_storeroom_80181830, { NULL } },                           // 0x1E6
    { 0x2, 0x70, func_shelter_b1_north_maintenance_walkway_80181940, { NULL } },           // 0x1E7
    { 0x2, 0x70, func_shelter_b1_main_corridor_80181B5C, { NULL } },                       // 0x1E8
    { 0x2, 0x70, func_shelter_b1_pod_access_tunnel_8017FB9C, { NULL } },                   // 0x1E9
    { 0x2, 0x70, func_shelter_b1_transfer_tunnel_80181390, { NULL } },                     // 0x1EA
    { 0x2, 0x70, func_shelter_b1_control_room_access_tunnel_8017ED3C, { NULL } },          // 0x1EB
    { 0x2, 0x70, func_shelter_b2_elevator_hall_80182260, { NULL } },                       // 0x1EC
    { 0x2, 0x70, func_shelter_b2_south_maintenance_walkway_8017F400, { NULL } },           // 0x1ED
    { 0x2, 0x70, func_shelter_b2_north_maintenance_walkway_80182618, { NULL } },           // 0x1EE
    { 0x2, 0x70, func_shelter_b2_main_corridor_801813B0, { NULL } },                       // 0x1EF
    { 0x2, 0x70, func_shelter_b2_septic_tank_80181644, { NULL } },                         // 0x1F0
    { 0x2, 0x70, func_shelter_b2_pod_access_tunnel_80182690, { NULL } },                   // 0x1F1
    { 0x2, 0x70, func_shelter_1f_parking_garage_8017F670, { NULL } },                      // 0x1F2
    { 0x2, 0x70, func_shelter_1f_vehicular_airlock_8017F720, { NULL } },                   // 0x1F3
    { 0x2, 0x70, func_neo_ark_north_promenade_80180838, { NULL } },                        // 0x1F4
    { 0x2, 0x70, func_neo_ark_forest_zone_8017EE84, { NULL } },                            // 0x1F5
    { 0x2, 0x70, func_neo_ark_pavilion_80180714, { NULL } },                               // 0x1F6
    { 0x2, 0x70, func_neo_ark_island_80180600, { NULL } },                                 // 0x1F7
    { 0x2, 0x70, func_neo_ark_power_plant_2_8017E858, { NULL } },                          // 0x1F8
    { 0x2, 0x70, func_neo_ark_savanna_zone_8017E470, { NULL } },                           // 0x1F9
    { 0x2, 0x70, func_neo_ark_south_promenade_8017E184, { NULL } },                        // 0x1FA
    { 0x2, 0x70, func_neo_ark_shrine_80180904, { NULL } },                                 // 0x1FB
    { 0x2, 0x70, func_shelter_b6_nursery_8018378C, { NULL } },                             // 0x1FC
    { 0x2, 0x70, func_neo_ark_bridge_801809E8, { NULL } },                                 // 0x1FD
    { 0x2, 0x70, func_neo_ark_pyramid_8017E6B4, { NULL } },                                // 0x1FE
    { 0x2, 0x70, func_dryfield_night_r08_8017F8FC, { NULL } },                             // 0x1FF
    { 0x2, 0x70, func_shelter_b1_elevator_hall_80182064, { NULL } },                       // 0x200
    { 0x2, 0x70, func_shelter_b1_south_maintenance_walkway_8017FAAC, { NULL } },           // 0x201
    { 0x2, 0x70, func_shelter_b1_storeroom_80182118, { NULL } },                           // 0x202
    { 0x2, 0x70, func_shelter_b1_north_maintenance_walkway_80182228, { NULL } },           // 0x203
    { 0x2, 0x70, func_shelter_b1_main_corridor_80182444, { NULL } },                       // 0x204
    { 0x2, 0x70, func_shelter_b1_pod_access_tunnel_80180484, { NULL } },                   // 0x205
    { 0x2, 0x70, func_shelter_b1_transfer_tunnel_80181C78, { NULL } },                     // 0x206
    { 0x2, 0x70, func_shelter_b1_control_room_access_tunnel_8017F624, { NULL } },          // 0x207
    { 0x2, 0x70, func_shelter_b2_elevator_hall_80182B48, { NULL } },                       // 0x208
    { 0x2, 0x70, func_shelter_b2_south_maintenance_walkway_8017FCE8, { NULL } },           // 0x209
    { 0x2, 0x70, func_shelter_b2_north_maintenance_walkway_80182F00, { NULL } },           // 0x20A
    { 0x2, 0x70, func_shelter_b2_main_corridor_80181C98, { NULL } },                       // 0x20B
    { 0x2, 0x70, func_shelter_b2_septic_tank_80181F2C, { NULL } },                         // 0x20C
    { 0x2, 0x70, func_shelter_b2_pod_access_tunnel_80182F78, { NULL } },                   // 0x20D
    { 0x2, 0x70, func_shelter_1f_parking_garage_8017FF58, { NULL } },                      // 0x20E
    { 0x2, 0x70, func_shelter_1f_vehicular_airlock_80180008, { NULL } },                   // 0x20F
    { 0x2, 0x70, func_neo_ark_north_promenade_80181120, { NULL } },                        // 0x210
    { 0x2, 0x70, func_neo_ark_forest_zone_8017F76C, { NULL } },                            // 0x211
    { 0x2, 0x70, func_neo_ark_pavilion_80180FFC, { NULL } },                               // 0x212
    { 0x2, 0x70, func_neo_ark_island_80180EE8, { NULL } },                                 // 0x213
    { 0x2, 0x70, func_neo_ark_power_plant_2_8017F140, { NULL } },                          // 0x214
    { 0x2, 0x70, func_neo_ark_savanna_zone_8017ED58, { NULL } },                           // 0x215
    { 0x2, 0x70, func_neo_ark_south_promenade_8017EA6C, { NULL } },                        // 0x216
    { 0x2, 0x70, func_neo_ark_shrine_801811EC, { NULL } },                                 // 0x217
    { 0x2, 0x70, func_shelter_b6_nursery_80184074, { NULL } },                             // 0x218
    { 0x2, 0x70, func_neo_ark_bridge_801812D0, { NULL } },                                 // 0x219
    { 0x2, 0x70, func_neo_ark_pyramid_8017EF9C, { NULL } },                                // 0x21A
    { 0x2, 0x70, func_shelter_b1_south_maintenance_walkway_80180C4C, { NULL } },           // 0x21B
    { 0x2, 0x70, func_shelter_b1_south_maintenance_walkway_801806F4, { NULL } },           // 0x21C
    { 0x2, 0x70, func_shelter_b1_south_maintenance_walkway_801818AC, { NULL } },           // 0x21D
    { 0x2, 0x70, func_shelter_b1_storeroom_80182D60, { NULL } },                           // 0x21E
    { 0x2, 0x70, func_shelter_b1_north_maintenance_walkway_80182E70, { NULL } },           // 0x21F
    { 0x2, 0x70, func_shelter_b1_sleeping_quarters_8017E6DC, { NULL } },                   // 0x220
    { 0x2, 0x70, func_shelter_b2_south_maintenance_walkway_80180930, { NULL } },           // 0x221
    { 0x2, 0x70, func_shelter_b2_operating_room_8017ECFC, { NULL } },                      // 0x222
    { 0x2, 0x70, func_shelter_b3_elevator_hall_80180E18, { NULL } },                       // 0x223
    { 0x2, 0x70, func_shelter_b4_upper_sewer_801846C8, { NULL } },                         // 0x224
    { 0x2, 0x70, func_shelter_b4_reservoir_80182B1C, { NULL } },                           // 0x225
    { 0x2, 0x70, func_shelter_b4_water_supply_801809DC, { NULL } },                        // 0x226
    { 0x2, 0x70, func_neo_ark_pavilion_80181C44, { NULL } },                               // 0x227
    { 0x2, 0x70, func_neo_ark_garden_8017F790, { NULL } },                                 // 0x228
    { 0x2, 0x70, func_shelter_b1_storeroom_801832B8, { NULL } },                           // 0x229
    { 0x2, 0x70, func_shelter_b1_north_maintenance_walkway_801833C8, { NULL } },           // 0x22A
    { 0x2, 0x70, func_shelter_b1_sleeping_quarters_8017EC34, { NULL } },                   // 0x22B
    { 0x2, 0x70, func_shelter_b2_south_maintenance_walkway_80180E88, { NULL } },           // 0x22C
    { 0x2, 0x70, func_shelter_b2_operating_room_8017F254, { NULL } },                      // 0x22D
    { 0x2, 0x70, func_shelter_b3_elevator_hall_80181370, { NULL } },                       // 0x22E
    { 0x2, 0x70, func_shelter_b4_upper_sewer_80184C20, { NULL } },                         // 0x22F
    { 0x2, 0x70, func_shelter_b4_reservoir_80183074, { NULL } },                           // 0x230
    { 0x2, 0x70, func_shelter_b4_water_supply_80180F34, { NULL } },                        // 0x231
    { 0x2, 0x70, func_neo_ark_pavilion_8018219C, { NULL } },                               // 0x232
    { 0x2, 0x70, func_neo_ark_garden_8017FCE8, { NULL } },                                 // 0x233
    { 0x2, 0x70, func_shelter_b1_storeroom_80183F18, { NULL } },                           // 0x234
    { 0x2, 0x70, func_shelter_b1_north_maintenance_walkway_80184028, { NULL } },           // 0x235
    { 0x2, 0x70, func_shelter_b1_sleeping_quarters_8017F894, { NULL } },                   // 0x236
    { 0x2, 0x70, func_shelter_b2_south_maintenance_walkway_80181AE8, { NULL } },           // 0x237
    { 0x2, 0x70, func_shelter_b2_operating_room_8017FEB4, { NULL } },                      // 0x238
    { 0x2, 0x70, func_shelter_b3_elevator_hall_80181FD0, { NULL } },                       // 0x239
    { 0x2, 0x70, func_shelter_b4_upper_sewer_80185880, { NULL } },                         // 0x23A
    { 0x2, 0x70, func_shelter_b4_reservoir_80183CD4, { NULL } },                           // 0x23B
    { 0x2, 0x70, func_shelter_b4_water_supply_80181B94, { NULL } },                        // 0x23C
    { 0x2, 0x70, func_neo_ark_pavilion_80182DFC, { NULL } },                               // 0x23D
    { 0x2, 0x70, func_neo_ark_garden_80180948, { NULL } },                                 // 0x23E
    { 0x2, 0x70, func_mine_cavern_80180320, { NULL } },                                    // 0x23F
    { 0x2, 0x70, func_mine_secret_passage_8017F948, { NULL } },                            // 0x240
    { 0x2, 0x70, func_mine_secret_passage_8017F5B0, { NULL } },                            // 0x241
    { 0x2, 0x70, func_mine_secret_passage_8017E868, { NULL } },                            // 0x242
    { 0x2, 0x70, func_mine_secret_passage_80180D58, { NULL } },                            // 0x243
    { 0x2, 0x70, func_mine_cavern_8017F240, { NULL } },                                    // 0x244
    { 0x2, 0x70, func_shelter_b1_elevator_hall_8017E6F4, { NULL } },                       // 0x245
    { 0x2, 0x70, func_shelter_b1_storeroom_8017E7A8, { NULL } },                           // 0x246
    { 0x2, 0x70, func_shelter_b1_north_maintenance_walkway_8017E8B8, { NULL } },           // 0x247
    { 0x2, 0x70, func_shelter_b1_main_corridor_8017EAD4, { NULL } },                       // 0x248
    { 0x2, 0x70, func_shelter_b1_transfer_tunnel_8017E308, { NULL } },                     // 0x249
    { 0x2, 0x70, func_shelter_b2_elevator_hall_8017F1D8, { NULL } },                       // 0x24A
    { 0x2, 0x70, func_shelter_b2_north_maintenance_walkway_8017F590, { NULL } },           // 0x24B
    { 0x2, 0x70, func_shelter_b2_pod_access_tunnel_8017F608, { NULL } },                   // 0x24C
    { 0x2, 0x70, func_shelter_b3_elevator_hall_8017E7F4, { NULL } },                       // 0x24D
    { 0x2, 0x70, func_shelter_b4_upper_sewer_80180110, { NULL } },                         // 0x24E
    { 0x2, 0x70, func_neo_ark_north_promenade_8017D7B0, { NULL } },                        // 0x24F
    { 0x2, 0x70, func_mine_cavern_8017FF88, { NULL } },                                    // 0x250
    { 0x2, 0x70, func_shelter_b1_elevator_hall_8017F43C, { NULL } },                       // 0x251
    { 0x2, 0x70, func_shelter_b1_storeroom_8017F4F0, { NULL } },                           // 0x252
    { 0x2, 0x70, func_shelter_b1_north_maintenance_walkway_8017F600, { NULL } },           // 0x253
    { 0x2, 0x70, func_shelter_b1_main_corridor_8017F81C, { NULL } },                       // 0x254
    { 0x2, 0x70, func_shelter_b1_transfer_tunnel_8017F050, { NULL } },                     // 0x255
    { 0x2, 0x70, func_shelter_b2_elevator_hall_8017FF20, { NULL } },                       // 0x256
    { 0x2, 0x70, func_shelter_b2_north_maintenance_walkway_801802D8, { NULL } },           // 0x257
    { 0x2, 0x70, func_shelter_b2_pod_access_tunnel_80180350, { NULL } },                   // 0x258
    { 0x2, 0x70, func_shelter_b3_elevator_hall_8017F53C, { NULL } },                       // 0x259
    { 0x2, 0x70, func_shelter_b4_upper_sewer_80180E58, { NULL } },                         // 0x25A
    { 0x2, 0x70, func_neo_ark_north_promenade_8017E4F8, { NULL } },                        // 0x25B
    { 0x2, 0x70, func_shelter_b1_elevator_hall_8017F7D4, { NULL } },                       // 0x25C
    { 0x2, 0x70, func_shelter_b1_storeroom_8017F888, { NULL } },                           // 0x25D
    { 0x2, 0x70, func_shelter_b1_north_maintenance_walkway_8017F998, { NULL } },           // 0x25E
    { 0x2, 0x70, func_shelter_b1_main_corridor_8017FBB4, { NULL } },                       // 0x25F
    { 0x2, 0x70, func_shelter_b1_transfer_tunnel_8017F3E8, { NULL } },                     // 0x260
    { 0x2, 0x70, func_shelter_b2_elevator_hall_801802B8, { NULL } },                       // 0x261
    { 0x2, 0x70, func_shelter_b2_north_maintenance_walkway_80180670, { NULL } },           // 0x262
    { 0x2, 0x70, func_shelter_b2_pod_access_tunnel_801806E8, { NULL } },                   // 0x263
    { 0x2, 0x70, func_shelter_b3_elevator_hall_8017F8D4, { NULL } },                       // 0x264
    { 0x2, 0x70, func_shelter_b4_upper_sewer_801811F0, { NULL } },                         // 0x265
    { 0x2, 0x70, func_neo_ark_north_promenade_8017E890, { NULL } },                        // 0x266
    { 0x2, 0x70, func_mine_cavern_80181730, { NULL } },                                    // 0x267
    { 0x2, 0x70, func_shelter_b1_elevator_hall_80180BE4, { NULL } },                       // 0x268
    { 0x2, 0x70, func_shelter_b1_storeroom_80180C98, { NULL } },                           // 0x269
    { 0x2, 0x70, func_shelter_b1_north_maintenance_walkway_80180DA8, { NULL } },           // 0x26A
    { 0x2, 0x70, func_shelter_b1_main_corridor_80180FC4, { NULL } },                       // 0x26B
    { 0x2, 0x70, func_shelter_b1_transfer_tunnel_801807F8, { NULL } },                     // 0x26C
    { 0x2, 0x70, func_shelter_b2_elevator_hall_801816C8, { NULL } },                       // 0x26D
    { 0x2, 0x70, func_shelter_b2_north_maintenance_walkway_80181A80, { NULL } },           // 0x26E
    { 0x2, 0x70, func_shelter_b2_pod_access_tunnel_80181AF8, { NULL } },                   // 0x26F
    { 0x2, 0x70, func_shelter_b3_elevator_hall_80180CE4, { NULL } },                       // 0x270
    { 0x2, 0x70, func_shelter_b4_upper_sewer_80182600, { NULL } },                         // 0x271
    { 0x2, 0x70, func_neo_ark_north_promenade_8017FCA0, { NULL } },                        // 0x272
    { 0x2, 0x70, func_dryfield_dilapidated_house_80182744, { NULL } },                     // 0x273
    { 0x2, 0x70, func_dryfield_dilapidated_house_80183C8C, { NULL } },                     // 0x274
    { 0x2, 0x70, func_dryfield_dilapidated_house_80183D5C, { NULL } },                     // 0x275
    { 0x2, 0x70, func_shelter_b1_control_room_8017FF80, { NULL } },                        // 0x276
    { 0x2, 0x70, func_shelter_b1_control_room_801804D8, { NULL } },                        // 0x277
    { 0x2, 0x70, func_shelter_b1_control_room_80181138, { NULL } },                        // 0x278
    { 0x2, 0x70, func_shelter_b1_control_room_access_tunnel_8018026C, { NULL } },          // 0x279
    { 0x2, 0x70, func_shelter_b1_control_room_access_tunnel_801807C4, { NULL } },          // 0x27A
    { 0x2, 0x70, func_shelter_b1_control_room_access_tunnel_80181424, { NULL } },          // 0x27B
    { 0x2, 0x70, func_shelter_b2_breeding_room_8017E774, { NULL } },                       // 0x27C
    { 0x2, 0x70, func_shelter_b2_breeding_room_8017ECCC, { NULL } },                       // 0x27D
    { 0x2, 0x70, func_shelter_b2_breeding_room_8017F92C, { NULL } },                       // 0x27E
    { 0x2, 0x70, func_neo_ark_submarine_tunnel_8017F4DC, { NULL } },                       // 0x27F
    { 0x2, 0x70, func_neo_ark_submarine_tunnel_8017FA34, { NULL } },                       // 0x280
    { 0x2, 0x70, func_neo_ark_submarine_tunnel_80180694, { NULL } },                       // 0x281
    { 0x2, 0x70, func_dryfield_motel_balcony_8017DCB8, { NULL } },                         // 0x282
    { 0x2, 0x70, func_dryfield_motel_balcony_8017EA00, { NULL } },                         // 0x283
    { 0x2, 0x70, func_dryfield_motel_balcony_8017ED98, { NULL } },                         // 0x284
    { 0x2, 0x70, func_dryfield_motel_balcony_801801A8, { NULL } },                         // 0x285
    { 0x2, 0x70, func_dryfield_night_main_street_8017FA68, { NULL } },                     // 0x286
    { 0x2, 0x70, func_dryfield_night_main_street_801807B0, { NULL } },                     // 0x287
    { 0x2, 0x70, func_dryfield_night_main_street_80180B48, { NULL } },                     // 0x288
    { 0x2, 0x70, func_dryfield_night_main_street_80181F58, { NULL } },                     // 0x289
    { 0x2, 0x70, func_dryfield_toilet_8017E69C, { NULL } },                                // 0x28A
    { 0x2, 0x70, func_dryfield_toilet_8017EBF4, { NULL } },                                // 0x28B
    { 0x2, 0x70, func_dryfield_toilet_8017F854, { NULL } },                                // 0x28C
    { 0x2, 0x70, func_acropolis_cafeteria_8017F948, { NULL } },                            // 0x28D
    { 0x2, 0x70, func_acropolis_cafeteria_801803AC, { NULL } },                            // 0x28E
    { 0x2, 0x70, func_acropolis_cafeteria_80180C94, { NULL } },                            // 0x28F
    { 0x2, 0x70, func_acropolis_forked_road_8017EF80, { NULL } },                          // 0x290
    { 0x2, 0x70, func_acropolis_forked_road_8017F9E4, { NULL } },                          // 0x291
    { 0x2, 0x70, func_acropolis_forked_road_801802CC, { NULL } },                          // 0x292
    { 0x2, 0x70, func_dryfield_main_street_8017EEE8, { NULL } },                           // 0x293
    { 0x2, 0x70, func_dryfield_main_street_8017F94C, { NULL } },                           // 0x294
    { 0x2, 0x70, func_dryfield_main_street_80180234, { NULL } },                           // 0x295
    { 0x2, 0x70, func_dryfield_back_street_8017D9D0, { NULL } },                           // 0x296
    { 0x2, 0x70, func_dryfield_back_street_8017E434, { NULL } },                           // 0x297
    { 0x2, 0x70, func_dryfield_back_street_8017ED1C, { NULL } },                           // 0x298
    { 0x2, 0x70, func_shelter_b6_corridor_8017ECA8, { NULL } },                            // 0x299
    { 0x2, 0x70, func_gunblade_8011DAA4, { NULL } },                                       // 0x29A
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
    GpState1C* p;
    s32        val;

    val = 0;
    p   = memCalloc(0x1C, val);
    if (p == NULL) {
        taskKill(arg0);
        return;
    }

    Gp_State1CTask = arg0;
    Gp_State1C     = p;
    arg0->work     = p;
    p->effectCount = 0;
    p->rumbleCount = 0;
    p->eventState  = 0;
    p->groundTrace = 1;
    p->groundShade = 0;
    Gp_SpawnEff(0x60053, 0, 0, 0);

    D_80115758        = 0;
    D_8011572C        = 0;
    D_80115750        = 0;
    D_80115730        = 0;
    D_80115734        = 0;
    D_80115754        = 0;
    D_80115728        = 0;
    D_80115744        = 0;
    D_8011573C        = 0;
    D_80115720        = 0;
    D_8011574C        = 0;
    p->roomEffectMode = 0;
    p->field_C        = 0;
    p->fadeState      = 0;
    p->screenFxFlags  = 0;
    p->peFxFlags      = 0;
    p->burstRequest   = 0;
    p->battleState    = 0;
    p->peFadeId       = 0;
    p->pendingPulses  = 0;
    D_80115738        = 0;
    D_80115724        = 0;
    arg0->state++;
    Gp_InitRoomCoords();

    switch (gGameSession->at4.loc.stage) {
        case 1:
            val = D_80111B70[gGameSession->at4.loc.area - 1];
            break;
        case 2:
            val = D_80111BC0[gGameSession->at4.loc.area - 1];
            break;
        case 3:
            val = D_80111C58[gGameSession->at4.loc.area - 1];
            break;
        case 4:
            val = D_80111CF0[gGameSession->at4.loc.area - 1];
            break;
        case 5:
            val = D_80111DB4[gGameSession->at4.loc.area - 1];
            break;
    }

    if (val != 0) {
        Gp_SpawnEff(val | 0x60000, 0, 0, 0);
    }
    Task_Spawn(6, 0x80000007, 0, 0);
}

static void Gp_TickState1C(Task* unused)
{
    GpState1C*  p;
    GpStateF0*  q;
    GpStateC08* r;
    s16         temp;

    if (Gp_State1C->effectCount <= 0) {
        Gp_State1C->effectCount = 0;
    }
    if (Gp_State1C->rumbleCount <= 0) {
        Gp_State1C->rumbleCount = 0;
    }
    temp = Gp_State1C->battleState;
    if ((temp == 1) && (Gp_StateF0.prefix.bytes.field_0 != temp)) {
        SndEvt_EnqueueType7(0xFF0D, 1);
        Gp_State1C->rumbleCount = 0;
    }
    p                = Gp_State1C;
    q                = &Gp_StateF0;
    p->battleState   = q->prefix.bytes.field_0;
    p->eventState    = q->field_4 | (p->pendingPulses & 0x100);
    p->fadeState     = q->field_4 | (p->pendingPulses & 0x180);
    p->pendingPulses = 0;
    if (!(p->eventState & 1)) {
        Gp_DecRoomCoordRefs();
    }
    if (Gp_State1C->fadeState >= 4) {
        r           = &Gp_StateC08;
        r->field_10 = 0;
        r->field_C  = 0;
        r->field_12 = 0;
        r->field_D  = 0;
        r->field_14 = 0;
        r->field_F  = 0;
        r->field_16 = 0;
        r->field_17 = 0;
        Gp_TriggerPeState(1, 0x80);
    }
}

s32 Gp_TraceGroundCoord(GfxCoord* arg0, GfxCoord* arg1)
{
    u8*           head;
    GpRayScratch* block;
    SVECTOR*      dir;
    MATRIX*       world;
    s32           ret;
    u16           vz;

    head                                   = SCRATCH_HEAD(u8);
    block                                  = (GpRayScratch*)(head - 0x10);
    ((GpRayScratch*)(head - 0x10))->pos.vx = (u16)arg0->workm.t[0];
    block->pos.vy                          = (u16)arg0->workm.t[1];
    vz                                     = (u16)arg0->workm.t[2];
    SCRATCH_HEAD(GpRayScratch)             = block;
    block->dir.vx                          = 0;
    block->dir.vy                          = 0x1000;
    block->dir.vz                          = 0;
    block->pos.vz                          = vz;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    dir = (SVECTOR*)(head - 8);
    gte_ldv0(dir);
    gte_rtv0();
    gte_stsv(dir);
    block->dir.vx += ((GpRayScratch*)(head - 0x10))->pos.vx;
    block->dir.vy += block->pos.vy;
    block->dir.vz += block->pos.vz;
    ret            = func_800DE7CC(dir, &block->pos, dir, NULL);
    if (ret == 1) {
        world            = &gGfxViewCoord.workm;
        arg1->workm.t[0] = block->dir.vx;
        arg1->workm.t[1] = block->dir.vy;
        arg1->workm.t[2] = block->dir.vz;
        Gp_WorldToLocal(world, &arg1->workm, &arg1->coord);
        arg1->parent       = PARENT_OF(world, GfxCoord, workm);
        arg1->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(arg1);
    }
    SCRATCH_POP_BYTES(0x10);
    return ret;
}

s32 func_800EA1A8(VECTOR3* arg0, VECTOR3* arg1)
{
    u8*           head;
    GpRayScratch* block;
    SVECTOR*      dir;
    s32           ret;
    u16           vz;

    head                                   = SCRATCH_HEAD(u8);
    block                                  = (GpRayScratch*)(head - 0x10);
    ((GpRayScratch*)(head - 0x10))->pos.vx = (u16)arg0->vx;
    block->pos.vy                          = (u16)arg0->vy;
    vz                                     = (u16)arg0->vz;
    SCRATCH_HEAD(GpRayScratch)             = block;
    block->dir.vx                          = 0;
    block->dir.vy                          = 0x1000;
    block->dir.vz                          = 0;
    block->pos.vz                          = vz;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    dir = (SVECTOR*)(head - 8);
    gte_ldv0(dir);
    gte_rtv0();
    gte_stsv(dir);
    block->dir.vx += ((GpRayScratch*)(head - 0x10))->pos.vx;
    block->dir.vy += block->pos.vy;
    block->dir.vz += block->pos.vz;
    ret            = func_800DE7CC(dir, &block->pos, dir, NULL);
    if (ret == 1) {
        arg1->vx = block->dir.vx;
        arg1->vy = block->dir.vy;
        arg1->vz = block->dir.vz;
        ret      = block->dir.vy - block->pos.vy;
        if (ret == 0) {
            ret = 1;
        }
    }
    SCRATCH_POP_BYTES(0x10);
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
    Gp_State1C->field_C = arg0 + 1;
}

static void Gp_DecRoomCoordRefs(void)
{
    s32        i;
    GpCoord64* p;

    p = Gp_RoomCoords;
    for (i = 0; i < 8; i++) {
        if (p->framesLeft != 0) {
            p->framesLeft--;
        }
        p++;
    }
}

static void Gp_InitRoomCoords(void)
{
    s32        i;
    GpCoord64* p;

    p = Gp_RoomCoords;
    for (i = 0; i < 8; i++) {
        p->light.head.u.coord.parent = &gGfxViewCoord;
        p->framesLeft                = 0;
        p++;
    }
}

void func_800EA420(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80097678;
    sp.funcs[arg0->state](arg0);
}

GpEffWork* Gp_SpawnEff(s32 arg0, GfxCoord* arg1, TaskSpawnArg arg2, SVECTOR* arg3)
{
    Task*      task;
    GpEffWork* mem;
    s32        bank;

    bank = (arg0 >> 16) & 0x7FFF;
    if ((arg0 >= 0) && (Gp_State1C->effectCount >= 0x81)) {
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
    mem = memCalloc(sizeof(GpEffWork), false);
    if (mem == NULL) {
        taskKill(task);
        return NULL;
    }
    Gp_State1C->effectCount++;

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
            Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &coord->coord);
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
    GpArcScratch* block;
    POLY_G4*      prim;
    DR_TPAGE*     dr;
    s32           ang;
    s32           otz;

    block         = SCRATCH_PUSH(GpArcScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->inner = ((s16)arg1 * 64) / block->otz;
        block->outer = (((s16)arg1 + (s16)arg2) * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang += 0x100) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->inner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->inner * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->inner * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->outer * rsin(ang + 0x100)) >> 12);
            prim->y3 = block->sy + ((block->outer * rcos(ang + 0x100)) >> 12);
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
    SCRATCH_POP(GpArcScratch);
}

void Gp_DrawRing(GfxCoord* arg0, s32 arg1, u8* rgb)
{
    GpRingScratch* block;
    POLY_G4*       prim;
    DR_TPAGE*      dr;
    s32            ang;
    s32            otz;

    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->step * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->step * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->step * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->step * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->step * rcos(ang + 0x200)) >> 12);
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
    SCRATCH_POP(GpRingScratch);
}

void Gp_DrawFxQuad(GfxCoord* arg0, u16 arg1, s16 arg2, u16 arg3)
{
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    u16              clutIdx;
    s32              u0;
    s32              u1;
    s32              ang2;

    block         = SCRATCH_PUSH(GpFxQuadScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    clutIdx = arg3 >> 12;
    arg3   &= 0xFFF;
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
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
        block->dx = (((arg2 * 31) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(GpFxQuadScratch);
}

void func_800EB6E8(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    GpRingScratch* block;
    POLY_FT4*      prim;
    u16            bank;
    u16            clutIdx;
    s32            u0;
    s32            u1;

    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    bank    = arg2 >> 12;
    arg2   &= 0xFFF;
    clutIdx = arg3 >> 12;
    arg3   &= 0xFF;
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
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
        block->step = (arg2 * 23) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->step;
        prim->x1 = prim->x3 = block->sx + block->step;
        prim->y0 = prim->y1 = block->sy - block->step;
        prim->y2 = prim->y3 = block->sy + block->step;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(GpRingScratch);
}

void Gp_DrawBand(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    DR_TPAGE*      dr;
    s32            i;
    s32            next;
    s32            ang;
    s32            otz;
    s16            r0;
    s16            r1;

    r1    = arg1 + 0x100;
    block = SCRATCH_PUSH(GpBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = (rcos(ang) * r0) >> 12;
        block->inner[i].vz = 0x100;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = (u16)block->inner[i].vx + (u16)arg0->workm.t[0];
        block->inner[i].vy = (u16)block->inner[i].vy + (u16)arg0->workm.t[1];
        block->inner[i].vz = (u16)block->inner[i].vz + (u16)arg0->workm.t[2];
        op                 = &block->inner[i] + 16;
        op->vx             = (rsin(ang) * r1) >> 12;
        op->vy             = (rcos(ang) * r1) >> 12;
        op->vz             = 0;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        op->vx = (u16)op->vx + (u16)arg0->workm.t[0];
        op->vy = (u16)op->vy + (u16)arg0->workm.t[1];
        op->vz = (u16)op->vz + (u16)arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
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
    SCRATCH_POP(GpBandScratch);
}

void Gp_DrawBandEx(GfxCoord* arg0, s16 arg1, s32 arg2, u8* rgb)
{
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    DR_TPAGE*      dr;
    s32            i;
    s32            next;
    s32            ang;
    s32            otz;
    s16            r0;
    s16            r1;

    r1    = arg1 + arg2;
    block = SCRATCH_PUSH(GpBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = 0;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx += arg0->workm.t[0];
        block->inner[i].vy += arg0->workm.t[1];
        block->inner[i].vz += arg0->workm.t[2];
        block->outer[i].vx  = (rsin(ang) * r1) >> 12;
        op                  = &block->inner[i] + 16;
        op->vy              = 0;
        op->vz              = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx += arg0->workm.t[0];
        op->vy             += arg0->workm.t[1];
        op->vz             += arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
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
    SCRATCH_POP(GpBandScratch);
}

void func_800EC47C(Task* arg0)
{
    GpEffWork* mem;
    u8         rgb[3];
    s32        current;
    s32        target;
    u16        count;
    u32        random;

    mem = arg0->spawnArg2.pointer;
    switch (arg0->state) {
        case 0:
            Gp_State1C->screenFxFlags |= 1;
            arg0->state                = 1;
            mem->angle                 = 0x10;
        case 1:
            if (mem->scale < mem->angle) {
                mem->scale += 8;
            } else {
                arg0->state = 2;
            }
            if (!(Player_Status.peStateFlags & 1)) {
                arg0->state = 3;
            }
            rgb[0] = rgb[1] = rgb[2] = mem->scale;
            Gp_DrawFadeQuad(rgb, 2);
            break;
        case 2:
            current = mem->scale;
            target  = mem->angle;
            if (current == target) {
                count       = mem->period + 1;
                random      = Gp_LcgState * 5 + 0x71357911;
                mem->period = count;
                Gp_LcgState = random;
                mem->angle  = ((count & 1) << (((random >> 16) & 1) + 4)) + 0x10;
            } else {
                if (current < target) {
                    mem->scale = current + 8;
                } else {
                    mem->scale = current - 8;
                }
            }
            if (!(Player_Status.peStateFlags & 1)) {
                arg0->state = 3;
            }
            rgb[0] = rgb[1] = rgb[2] = mem->scale;
            Gp_DrawFadeQuad(rgb, 2);
            break;
        case 3:
            if (Player_Status.peStateFlags & 1) {
                arg0->state = 0;
                rgb[0] = rgb[1] = rgb[2] = mem->scale;
                Gp_DrawFadeQuad(rgb, 2);
            } else if (mem->scale >= 9) {
                mem->scale -= 8;
                rgb[0] = rgb[1] = rgb[2] = mem->scale;
                Gp_DrawFadeQuad(rgb, 2);
            } else {
                Gp_State1C->screenFxFlags &= 0xFFFE;
                Gp_State1C->effectCount--;
                memFree(mem);
                taskKill(arg0);
            }
            break;
    }
}

void Gp_FadeWaveTask(Task* arg0)
{
    GpState1C* p;
    GpEffWork* mem;
    u16        color;
    u8         rgb[3];

    p   = Gp_State1C;
    mem = arg0->spawnArg2.pointer;
    if (p->peFadeId != arg0->spawnArg1.value) {
        p->effectCount--;
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
        Gp_State1C->effectCount--;
        memFree(mem);
        taskKill(arg0);
    }
}

void Gp_ReleaseState1CMem(void* arg0, Task* arg1)
{
    Gp_State1C->effectCount--;
    memFree(arg0);
    taskKill(arg1);
}

static void Gp_KillState1CTask(Task* arg0)
{
    void* mem;

    mem = arg0->spawnArg2.pointer;
    Gp_State1C->effectCount--;
    memFree(mem);
    taskKill(arg0);
}

void Gp_PulseState1C(void)
{
    Gp_State1C->pendingPulses |= 0x100;
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

void Gp_AddTpageShift(P_TAG* arg0, s32 arg1, s32 arg2)
{
    DR_TPAGE* p;

    setSemiTrans(arg0, 1);
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    p->code[0]     = 0xE100020A | ((arg1 & 3) << 5);
    setlen(p, 1);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)arg2 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), p);
}

void func_800EC9C8(void)
{
    if (!(Gp_State1C->screenFxFlags & 1)) {
        Gp_SpawnEff(0x800600E8, 0, 0, 0);
    }
}

void Gp_SetState1CPe(s32 arg0)
{
    Gp_State1C->peFadeId = (u8)arg0;
    Gp_SpawnEff(0x8006000F, 0, (s32)((u8)arg0), 0);
}

void func_800ECA54(void)
{
    GpState1C* p;

    p = Gp_State1C;
    if (!(p->screenFxFlags & 0x80)) {
        p->peFxFlags &= 0xF7FF;
        Gp_SpawnEff(0x8006000E, 0, 0, 0);
    }
}
