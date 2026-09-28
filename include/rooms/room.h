#ifndef ROOMS_ROOM_H
#define ROOMS_ROOM_H

#include "common.h"

#include <psyq/libgte.h>
#include "main/task.h"
#include "overlay.h"

/// A scripted event a room starts in answer to a message. The room's message
/// handler builds the record, and if the event has not happened yet it copies
/// the record into the room's own pending copy and spawns the room's event
/// task, which works through it: it runs the capture command, starts the fade
/// task if asked to once the capture has finished, plays the stage sound and
/// waits for it, and then moves the player on.
typedef struct RoomLatchedEvent {
    s32 capCmd;   // Capture command the event task runs first
    s32 stageSnd; // Packed stage sound played once the capture has finished; 0 for none
    s16 flagId;   // Game-flag nibble that records the event as done, set when it starts; 0 for none
    u8  fade;     // Non-zero starts the fade task (bank 1, type 0x31) over 30 frames once the capture has finished
} RoomLatchedEvent;
STATIC_ASSERT_SIZEOF(RoomLatchedEvent, 0xC);

/// What a room's cutscene runner plays: the record a room hands the runner
/// task as `Task::spawnArg2`. The runner forces the scene's view into the save
/// location, loads the capture file and starts the capture slot with the
/// scene's sound task beside it, lets the player cut it short, and restores
/// everything when it ends.
typedef struct RoomCutsceneRec {
    s8  field_0;  // Positive: the view forced into the save location for the scene; otherwise its negation is the view restored after
    s8  field_1;  // Capture slot the scene starts, which also picks the command run after it
    s8  field_2;  // Non-zero skips straight to the abort path
    s8  field_3;  // Capture file to load first; 0 for none
    s32 field_4;  // Sound event at the start
    s32 field_8;  // Sound event at the end
    s32 field_C;  // Sound event after a scene that was not skipped
    s32 field_10; // Sound the scene's sound task plays, and that task's spawn argument
    s16 field_14; // First of the pair handed on once the capture file is loaded; 0 selects 0x3C0 with a second of 0
    s16 field_16; // Second of that pair
} RoomCutsceneRec;
STATIC_ASSERT_SIZEOF(RoomCutsceneRec, 0x18);

/// One row of a shop's price ladder, a table of thirteen in the room's data.
/// The row's three items join the shop's stock once the row's bit is set in
/// `Mc_SaveData[0].shopTiers`. The rooms read only `items`; the leading word grows
/// row by row up to `S32_MAX` in the last, which reads as the spend that
/// unlocks the row, but nothing here confirms it.
typedef struct RoomShopTier {
    s32  spendThreshold;
    s16  items[3];
    byte pad_A[0x2];
} RoomShopTier;
STATIC_ASSERT_SIZEOF(RoomShopTier, 0xC);

/// Where a room sends the player when it moves them on, staged by the room
/// before it spawns its departure task. The room first runs `area`, `warp`
/// and `room` through its message handler, which may rewrite them. The task
/// then turns the player to `facing`, plays `sndEvent` and waits for it, and
/// finally commits the four location bytes as the save location and restarts
/// the player task.
typedef struct RoomDeparture {
    u8   stage;
    u8   area;
    u8   warp;
    u8   room;
    s16  facing;   // Heading sent to the player as message 0x3EE; -1 sends nothing
    byte pad_6[0x2];
    s32  sndEvent; // Sound event played before leaving; 0 for none
} RoomDeparture;
STATIC_ASSERT_SIZEOF(RoomDeparture, 0xC);

/// The scratchpad block a mirror task takes while it rebuilds the reflected
/// coordinate frame in its `RoomMirrorWork`. A floor mirror only needs
/// `viewRow`, the view matrix's second row, which it negates through the GTE.
/// The other mirrors reflect through a plane: `normal` is the plane's unit
/// normal, `refAxis` the coordinate axis least aligned with it (picked through
/// `leastAbs`, `leastAxis` and `axisAbs`), `basis` the orthonormal frame built
/// from the two and `reflect` the reflection matrix derived from it. `offset`
/// is the plane's position relative to the view, rotated in place into the
/// reflected frame.
typedef struct RoomMirrorPlaneScratch {
    SVECTOR viewRow;
    SVECTOR refAxis;
    byte    unknown_10[8];
    MATRIX  basis;
    MATRIX  reflect;
    SVECTOR normal;
    SVECTOR offset;
    s16     leastAbs;
    s16     leastAxis;
    s16     axisAbs;
    byte    unknown_6E[2];
} RoomMirrorPlaneScratch;
STATIC_ASSERT_SIZEOF(RoomMirrorPlaneScratch, 0x70);

/// The scratchpad block a mirror task takes to find where the reflection
/// lands on screen. It projects two points above and below one of the
/// reflected model's parts through `pos`: `sxyHead` and `otzHead` for the
/// upper point, `sxyFoot` and `otzFoot` for the lower. `left` to `bottom` is
/// the screen rectangle the reflection quads cover, and `texX` the x of the
/// texture page they sample the off-screen copy of the frame from.
typedef struct RoomMirrorExtentScratch {
    SVECTOR pos;
    s32     dp;
    s32     flag;
    s32     otzFoot;
    s32     otzHead;
    DVECTOR sxyFoot;
    DVECTOR sxyHead;
    u16     texX;
    s32     left;
    s32     right;
    s32     top;
    s32     bottom;
} RoomMirrorExtentScratch;
STATIC_ASSERT_SIZEOF(RoomMirrorExtentScratch, 0x34);

/// Per-surface values a room's water drawer keeps in a block it takes from
/// the scratchpad stack rather than in registers: the surface height `y`, the
/// spacing `dx` and `dz` between vertices along X and Z, the height `wave`
/// adds to the vertex being placed, and the corner `x`, `z` of the surface
/// being drawn.
typedef struct RoomWaterScratch {
    s16 y;
    s16 dx;
    s16 wave;
    s16 dz;
    s16 x;
    s16 z;
} RoomWaterScratch;
STATIC_ASSERT_SIZEOF(RoomWaterScratch, 0xC);

/// One water surface in the list a room's water drawer walks: a rectangle at
/// (`x`, `z`) spanning `width` along X and `depth` along Z. A drawer that cuts
/// surfaces into a varying number of quads takes that number from `count`;
/// every list ends at an entry whose `count` is -1.
typedef struct RoomWaterSurface {
    s16 x;
    s16 z;
    s16 width;
    s16 depth;
    s32 count;
} RoomWaterSurface;
STATIC_ASSERT_SIZEOF(RoomWaterSurface, 0xC);

/// The spawn argument of a room model task whose visibility follows a 2-bit
/// game flag: the task hides its model while the flag `flagId` names reads 2.
/// Nothing else of the record is read.
typedef struct RoomFlagModelArg {
    u8 unk0[8];
    u8 flagId;
} RoomFlagModelArg;

/// The work block a room's streamed-scene task allocates at `Task::work`. The
/// task walks the translation of `mtx` along the scene's path table once per
/// streamed frame, addresses its messages to `target`, the task in pointer
/// slot 3, and reparents itself under `script`, the scene's script task.
/// `child` is a task it spawns on the way (a skip or prompt task) and polls
/// with `Task_PollKill`; `spawned` says that it exists, since the block starts
/// out zeroed.
typedef struct RoomStreamWork {
    MATRIX* mtx;
    Task*   target;
    Task*   child;
    Task*   script;
    u16     spawned;
    byte    pad_12[0x2];
} RoomStreamWork;
STATIC_ASSERT_SIZEOF(RoomStreamWork, 0x14);

/// The scratch block a room's mote or mist-puff drawer takes from
/// `G_SCRATCH_HEAD` for one projection: `vec` is the point's world position,
/// projected with a single `RTPS` through `GsWSMATRIX`, and `otz` the depth
/// the resulting tile is linked into the ordering table at; a depth below
/// 0x11 drops it.
typedef struct RoomMoteScratch {
    s32     otz;
    SVECTOR vec;
} RoomMoteScratch;
STATIC_ASSERT_SIZEOF(RoomMoteScratch, 0xC);

/// The scratch block a room's beam drawer takes from `G_SCRATCH_HEAD`: the
/// beam's base in world space and the tip offset from it, and both points'
/// projections - `otz0`, `sx0` and `sy0` for `base`, `otz1`, `sx1` and `sy1`
/// for `tip`. `r0` and `r1` are the wedge radii at each end.
typedef struct RoomBeamScratch {
    SVECTOR base;
    SVECTOR tip;
    s32     otz0;
    s32     otz1;
    s32     flag;
    s32     r0;
    s32     r1;
    u16     sx0;
    u16     sy0;
    u16     sx1;
    u16     sy1;
} RoomBeamScratch;
STATIC_ASSERT_SIZEOF(RoomBeamScratch, 0x2C);

/// The scratch block a room's glow-sprite drawer takes from `G_SCRATCH_HEAD`:
/// `pos` is the task coordinate's translation, projected through `GsWSMATRIX`
/// into `sxy`; `otz` is the resulting depth and `half` the half extent the
/// camera-facing quad is drawn at, divided by `otz` so the sprite shrinks with
/// distance.
typedef struct RoomGlowSpriteScratch {
    s32     otz;
    s32     half;
    SVECTOR pos;
    DVECTOR sxy;
} RoomGlowSpriteScratch;
STATIC_ASSERT_SIZEOF(RoomGlowSpriteScratch, 0x14);

/// The scratch block a room's disc drawer takes from `G_SCRATCH_HEAD`: the
/// depth of the projected centre, the two on-screen radii derived from it, the
/// GTE flag word and the projected centre.
typedef struct RoomDiscScratch {
    s32 otz;
    s32 rOuter;
    s32 rInner;
    s32 flag;
    u16 sx;
    u16 sy;
} RoomDiscScratch;
STATIC_ASSERT_SIZEOF(RoomDiscScratch, 0x14);

/// The scratch block a room's quad drawer projects one quad in when it keeps
/// the screen corners: the four corners in world space, the GTE depth and
/// flag of their projection (a negative flag rejects the quad), and the
/// projected corners, copied onto the primitive once one is allocated.
typedef struct RoomQuadProjScratch {
    SVECTOR v[4];
    s32     otz;
    s32     flag;
    DVECTOR sxy[4];
} RoomQuadProjScratch;
STATIC_ASSERT_SIZEOF(RoomQuadProjScratch, 0x38);

/// The scratch block a room's light-shaft drawer takes from `G_SCRATCH_HEAD`
/// for one shaft: the depth of its projection and its four corners in world
/// space - the two roots, then the tip reached from each.
typedef struct RoomLightShaftScratch {
    s32     otz;
    SVECTOR rootA;
    SVECTOR rootB;
    SVECTOR tipA;
    SVECTOR tipB;
} RoomLightShaftScratch;
STATIC_ASSERT_SIZEOF(RoomLightShaftScratch, 0x24);

/// One row of a halo effect's shade table, picked by the effect's palette
/// selector: each field is the right shift applied to the effect's fade level
/// to get that colour channel, so a row sets the tint of the halo.
typedef struct RoomHaloShade {
    s16 r;
    s16 g;
    s16 b;
} RoomHaloShade;
STATIC_ASSERT_SIZEOF(RoomHaloShade, 0x6);

/// One row of a ring effect's per-band table, added to the effect work's ring
/// parameters: `rInner` widens the ring drawn at the origin height, `yOff`
/// raises the second ring, and `rExtra` widens the second ring beyond the
/// first.
typedef struct RoomRingShape {
    s16 rInner;
    s16 yOff;
    s16 rExtra;
} RoomRingShape;
STATIC_ASSERT_SIZEOF(RoomRingShape, 0x6);

/// Task entries the resident task descriptor tables name. A table in main or
/// gameplay reaches each of these by name, so they are the family's interface
/// to the resident code.
void func_acropolis_fountain_8017DCD4(Task* arg0);
void func_acropolis_security_room_8017ED68(Task* task);
void func_acropolis_helicopter_landing_pad_8017EF8C(Task* arg0);
void func_acropolis_bridge_8017F788(Task* task);
void func_acropolis_security_room_80180294(Task* task);
void func_acropolis_cafeteria_80181E70(Task* task);

/// Models those descriptors attach.
extern TmdSource D_acropolis_cafeteria_801858C4;
extern TmdSource D_acropolis_cafeteria_8018625C;
extern TmdSource D_acropolis_cafeteria_80186CAC;
extern TmdSource D_acropolis_cafeteria_80187518;

/// Task entries the Dryfield map UI overlay's stage tables name: each room's
/// entry task, started for its location, and the enemy descriptors' tasks.
void func_dryfield_gas_station_8017FF8C(Task* task);
void func_dryfield_main_street_8017E168(Task* task);
void func_dryfield_general_store_8017DF5C(Task* task);
void func_dryfield_back_street_8017D918(Task* task);
void func_dryfield_souvenir_shop_8017D65C(Task* task);
void func_dryfield_warehouse_8017DA00(Task* task);
void func_dryfield_dilapidated_house_8017EB60(Task* task);
void func_dryfield_motel_room_1_8017D754(Task* task);
void func_dryfield_motel_room_2_8017D65C(Task* task);
void func_dryfield_motel_room_3_8017D65C(Task* task);
void func_dryfield_motel_room_4_8017D65C(Task* task);
void func_dryfield_parking_lot_8017DB54(Task* task);
void func_dryfield_toilet_8017D9E4(Task* task);
void func_dryfield_motel_lobby_8017F498(Task* task);
void func_dryfield_saloon_g_r_8017DA18(Task* task);
void func_dryfield_g_r_kitchen_8017D9A4(Task* task);
void func_dryfield_water_tower_8017DDD8(Task* task);
void func_dryfield_water_tank_8017DAF0(Task* task);
void func_dryfield_breezeway_8017DE68(Task* task);
void func_dryfield_factory_8017DF88(Task* task);
void func_dryfield_garage_8017DC10(Task* task);
void func_dryfield_driveway_8017DE14(Task* task);
void func_dryfield_junk_yard_8017D5F4(Task* task);
void func_dryfield_junk_yard_8017DCB4(Task* task);
void func_dryfield_trailer_coach_80182950(Task* task);
void func_dryfield_motel_room_5_8017D65C(Task* task);
void func_dryfield_motel_balcony_8017DBD0(Task* task);
void func_dryfield_motel_room_6_80181B18(Task* task);
void func_dryfield_motel_loft_8017D65C(Task* task);
void func_dryfield_water_hole_8017D840(Task* task);
void func_dryfield_cellar_8017D784(Task* task);
void func_dryfield_underpass_8017DAC8(Task* task);

/// Models the Dryfield map UI overlay's enemy descriptors attach.
extern TmdSource D_dryfield_junk_yard_8017ECE0;
extern TmdSource D_dryfield_trailer_coach_80184554;

/// Task entries the Akropolis map UI overlay's stage tables name: each room's
/// entry task, started for its location, and the enemy descriptors' tasks.
void func_acropolis_square_80182308(Task* task);
void func_acropolis_east_elevator_hall_8017F55C(Task* task);
void func_acropolis_patio_8017DF8C(Task* task);
void func_acropolis_cafeteria_8017E424(Task* task);
void func_acropolis_cafeteria_801827C4(Task* task);
void func_acropolis_cafeteria_8018286C(Task* task);
void func_acropolis_security_room_8017D984(Task* task);
void func_acropolis_hallway_8017D7D0(Task* task);
void func_acropolis_hallway_8017E120(Task* task);
void func_acropolis_fountain_8017D9C4(Task* task);
void func_acropolis_forked_road_8017D9CC(Task* task);
void func_acropolis_observatory_8017D950(Task* task);
void func_acropolis_promenade_8017DA4C(Task* task);
void func_acropolis_sanctuary_8017D9E8(Task* task);
void func_acropolis_sanctuary_80180264(Task* task);
void func_acropolis_roof_garden_8017DC74(Task* task);
void func_acropolis_roof_garden_80180160(Task* task);
void func_acropolis_bridge_8017DA0C(Task* task);
void func_acropolis_fire_escape_8017FF24(Task* task);
void func_acropolis_helicopter_landing_pad_8017D964(Task* task);
void func_acropolis_helicopter_landing_pad_8017EB00(Task* task);
void func_acropolis_helicopter_landing_pad_801822B0(Task* task);
void func_acropolis_west_elevator_hall_8017F5F4(Task* task);
void func_mist_r18_8017ED64(Task* task);
void func_mist_parking_80182898(Task* task);
void func_mist_shooting_gallery_8018018C(Task* task);
void func_mist_r21_8017D708(Task* task);

/// Models the Akropolis map UI overlay's enemy descriptors attach.
extern TmdSource D_acropolis_cafeteria_8018D230;
extern TmdSource D_acropolis_cafeteria_8018D57C;
extern TmdSource D_acropolis_security_room_80185584;
extern TmdSource D_acropolis_hallway_8017F85C;
extern TmdSource D_acropolis_sanctuary_80186A08;
extern TmdSource D_acropolis_sanctuary_80186C68;
extern TmdSource D_acropolis_roof_garden_80186E70;
extern TmdSource D_acropolis_helicopter_landing_pad_801836EC;
extern TmdSource D_acropolis_helicopter_landing_pad_80187F50;

/// Task entries the Neo Ark map UI overlay's stage tables name: each room's
/// entry task, started for its location, and the enemy descriptors' tasks.
void func_shelter_1f_parking_garage_8017DF14(Task* task);
void func_shelter_1f_vehicular_airlock_8017D5E4(Task* task);
void func_shelter_1f_vehicular_airlock_8017DA48(Task* task);
void func_shelter_1f_bulwark_8017DC20(Task* task);
void func_shelter_1f_heliport_80180768(Task* task);
void func_shelter_1f_airlock_8017D678(Task* task);
void func_shelter_1f_guardroom_8017D880(Task* task);
void func_neo_ark_observatory_8017FDDC(Task* task);
void func_neo_ark_eve_access_tunnel_8017E038(Task* task);
void func_neo_ark_eve_elevator_8017D6C4(Task* task);
void func_neo_ark_north_promenade_8017D6C8(Task* task);
void func_neo_ark_forest_zone_8017DBBC(Task* task);
void func_neo_ark_submarine_tunnel_8017F434(Task* task);
void func_neo_ark_pavilion_8017EBF4(Task* task);
void func_neo_ark_island_8017EB10(Task* task);
void func_neo_ark_garden_8017EA44(Task* task);
void func_neo_ark_power_plant_2_8017D854(Task* task);
void func_neo_ark_power_plant_1_8017D9C0(Task* task);
void func_neo_ark_savanna_zone_8017D954(Task* task);
void func_neo_ark_south_promenade_8017D678(Task* task);
void func_neo_ark_altar_8017D9E8(Task* task);
void func_neo_ark_shrine_8017D948(Task* task);
void func_shelter_b6_nursery_8017FF9C(Task* task);
void func_shelter_b6_growth_room_8017D7D4(Task* task);
void func_shelter_b6_corridor_8017E144(Task* task);
void func_shelter_b6_training_room_8017D8E8(Task* task);
void func_neo_ark_r26_8017D720(Task* task);
void func_neo_ark_bridge_8017E8FC(Task* task);
void func_shelter_1f_tent_8017FDB8(Task* task);
void func_neo_ark_woodland_path_8017E9B0(Task* task);
void func_neo_ark_submarine_gallery_8017EBCC(Task* task);
void func_neo_ark_r31_8017D990(Task* task);
void func_neo_ark_pyramid_8017DB98(Task* task);
void func_neo_ark_substation_8017D81C(Task* task);

/// Models the Neo Ark map UI overlay's enemy descriptors attach.
extern TmdSource D_shelter_1f_vehicular_airlock_80182004;
/// Task entries the Dryfield-at-night map UI overlay's stage tables name, each
/// room's entry task started for its location, and the models its enemy
/// descriptors attach.
void func_dryfield_night_gas_station_8017FB70(Task* task);
void func_dryfield_night_main_street_8017E0C0(Task* task);
void func_dryfield_night_general_store_8017DE88(Task* task);
void func_dryfield_night_back_street_8017D788(Task* task);
void func_dryfield_night_souvenir_shop_8017D65C(Task* task);
void func_dryfield_night_warehouse_8017D65C(Task* task);
void func_dryfield_night_r08_8017D6C0(Task* task);
void func_dryfield_night_dilapidated_house_8017DA18(Task* task);
void func_dryfield_night_motel_room_1_8017D6DC(Task* task);
void func_dryfield_night_motel_room_2_8017D6BC(Task* task);
void func_dryfield_night_motel_room_3_8017D6E0(Task* task);
void func_dryfield_night_motel_room_4_8017D6BC(Task* task);
void func_dryfield_night_parking_lot_8017DC30(Task* task);
void func_dryfield_night_toilet_8017D724(Task* task);
void func_dryfield_night_motel_lobby_8017FE38(Task* task);
void func_dryfield_night_saloon_g_r_8017E050(Task* task);
void func_dryfield_night_g_r_kitchen_8017D9A4(Task* task);
void func_dryfield_night_water_tower_8017DB28(Task* task);
void func_dryfield_night_water_tank_8017D984(Task* task);
void func_dryfield_night_breezeway_8017D680(Task* task);
void func_dryfield_night_factory_801809F4(Task* task);
void func_dryfield_night_garage_801803BC(Task* task);
void func_dryfield_night_driveway_8017DD8C(Task* task);
void func_dryfield_night_junk_yard_8017D960(Task* task);
void func_dryfield_night_trailer_coach_801828CC(Task* task);
void func_dryfield_night_motel_room_5_8017D6D0(Task* task);
void func_dryfield_night_motel_balcony_8017DD78(Task* task);
void func_dryfield_night_motel_room_6_80181C80(Task* task);
void func_dryfield_night_motel_loft_8017D964(Task* task);
void func_dryfield_night_water_hole_8017DE30(Task* task);
void func_dryfield_night_cellar_8017D748(Task* task);
void func_dryfield_night_underpass_8017D95C(Task* task);

extern TmdSource D_dryfield_night_trailer_coach_80184CA0;
extern TmdSource D_dryfield_night_motel_loft_8017EAF8;

/// Task entries the Shelter map UI overlay's stage tables name, each room's
/// entry task started for its location and the enemy descriptors' tasks, and
/// the models those descriptors attach.
void func_mine_mesa_8017DD98(Task* task);
void func_mine_cavern_8017DF54(Task* task);
void func_mine_tunnel_entrance_8017D6BC(Task* task);
void func_mine_tunnel_8017D77C(Task* task);
void func_mine_gorge_8017D9A0(Task* task);
void func_mine_refuge_8017FFBC(Task* task);
void func_mine_forked_tunnel_8017DBE4(Task* task);
void func_mine_forked_tunnel_8017E25C(Task* task);
void func_mine_secret_passage_8017D970(Task* task);
void func_shelter_b1_elevator_hall_8017DC28(Task* task);
void func_shelter_b1_south_maintenance_walkway_8017DA34(Task* task);
void func_shelter_b1_storeroom_8017D794(Task* task);
void func_shelter_b1_north_maintenance_walkway_8017DAFC(Task* task);
void func_shelter_b1_armory_8018078C(Task* task);
void func_shelter_b1_sleeping_quarters_8017D608(Task* task);
void func_shelter_b1_sleeping_quarters_8017D888(Task* task);
void func_shelter_b1_main_corridor_8017DD98(Task* task);
void func_shelter_b1_sterilization_room_80180518(Task* task);
void func_shelter_b1_pod_access_tunnel_8017DEE8(Task* task);
void func_shelter_b1_control_room_8017EECC(Task* task);
void func_shelter_b1_access_tunnel_8017DD08(Task* task);
void func_shelter_b1_underground_parking_801838B4(Task* task);
void func_shelter_b1_golem_freezer_1_8017D6EC(Task* task);
void func_shelter_b2_pod_bottom_8017D708(Task* task);
void func_shelter_b1_pod_service_gantry_8017D89C(Task* task);
void func_shelter_b1_transfer_tunnel_8017D678(Task* task);
void func_shelter_b1_control_room_access_tunnel_8017D68C(Task* task);
void func_shelter_b2_elevator_8017DB18(Task* task);
void func_shelter_b2_elevator_hall_8017DD08(Task* task);
void func_shelter_b2_south_maintenance_walkway_8017DC6C(Task* task);
void func_shelter_b2_operating_room_8017DD60(Task* task);
void func_shelter_b2_north_maintenance_walkway_8017DD90(Task* task);
void func_shelter_b2_laboratory_801804A4(Task* task);
void func_shelter_b2_breeding_room_8017D5F8(Task* task);
void func_shelter_b2_breeding_room_8017D840(Task* task);
void func_shelter_b2_main_corridor_8017E338(Task* task);
void func_shelter_b2_septic_tank_8017DB10(Task* task);
void func_shelter_b2_pod_access_tunnel_8017DC14(Task* task);
void func_shelter_r36_8017D9DC(Task* task);
void func_shelter_r37_8017D678(Task* task);
void func_shelter_1f_heliport_s4_8017D678(Task* task);
void func_shelter_b3_dumping_hole_8017D9A8(Task* task);
void func_shelter_b3_garbage_incinerator_8017DC7C(Task* task);
void func_shelter_b3_incinerator_control_room_8017FCB8(Task* task);
void func_shelter_b3_elevator_hall_8017DE18(Task* task);
void func_shelter_b4_lower_sewer_8017D6D4(Task* task);
void func_shelter_b4_upper_sewer_8017DC30(Task* task);
void func_shelter_b4_reservoir_8017E88C(Task* task);
void func_shelter_b4_water_supply_8017DDA4(Task* task);
void func_shelter_r47_801807B4(Task* task);
void func_shelter_r48_8017E224(Task* task);
void func_shelter_r49_8017D6C4(Task* task);

extern TmdSource D_mine_forked_tunnel_801807B4;
extern TmdSource D_shelter_b1_sleeping_quarters_801804F4;
extern TmdSource D_shelter_b1_sterilization_room_80184DF8;
extern TmdSource D_shelter_b2_laboratory_801829E4;
extern TmdSource D_shelter_b2_breeding_room_801803F0;
extern TmdSource D_shelter_b3_dumping_hole_80187550;

#endif /* ROOMS_ROOM_H */
