#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "actors/waypoints.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "rooms/neo_ark_bridge.h"

#include "rooms/neo_ark_island.h"

#include "rooms/neo_ark_pavilion.h"

#include "rooms/neo_ark_submarine_gallery.h"

#include "rooms/neo_ark_submarine_tunnel.h"

#include "rooms/shelter_b2_main_corridor.h"

#include "rooms/shelter_b2_septic_tank.h"

#include "rooms/shelter_b4_lower_sewer.h"

#include "rooms/shelter_b4_reservoir.h"

#include "rooms/shelter_b4_upper_sewer.h"
#include "../../shared/limb_shadows.h"
#include "../../shared/coord_math.h"
#include "../../shared/diver.h"

typedef struct {
    TaskFunc funcs[15];
} TaskFuncTable15;

typedef struct Actor100400QuadWork {
    /* 0x00 */ Enemy*  field_0;
    /* 0x04 */ SVECTOR vertices[4];
    /* 0x24 */ u8      intensity;
} Actor100400QuadWork;

/// 0x64-byte work block of the marker task `Actor00400_Fn064B0` spawns from
/// `Actor00400_D16028[1]`: a display object and its two `WorldCollisionContact` slots, then
/// the view-space span between the marker's base and tip.
typedef struct Actor100400MarkerWork {
    /* 0x00 */ byte                  pad_0[8];
    /* 0x08 */ WorldCollisionBody    obj;
    /* 0x28 */ WorldCollisionContact recs[2];
    /* 0x58 */ s16                   field_58;
    /* 0x5A */ s16                   field_5A;
    /* 0x5C */ s16                   field_5C;
    /* 0x5E */ byte                  pad_5E[2];
    /* 0x60 */ s32                   field_60;
} Actor100400MarkerWork;
STATIC_ASSERT_SIZEOF(Actor100400MarkerWork, 0x64);

/// 0x1C-byte scratch `Actor00400_Fn03318` carves off the scratch stack to hold
/// `RotTransPers4`'s outputs for the quad it projects: the four screen-space
/// corners, the perspective term, the clip flags and the average depth used as
/// the OT key.
typedef struct Actor100400TextQuadScratch {
    /* 0x00 */ long screen0;
    /* 0x04 */ long screen1;
    /* 0x08 */ long screen2;
    /* 0x0C */ long screen3;
    /* 0x10 */ long perspective;
    /* 0x14 */ long flags;
    /* 0x18 */ s32  depth;
} Actor100400TextQuadScratch;
STATIC_ASSERT_SIZEOF(Actor100400TextQuadScratch, 0x1C);

/// 8-byte waypoint record in the `Actor100400Work.field_608` array, walked
/// until `field_6` is -1. `field_0` / `field_4` are the X and Z the actor
/// steers toward; `field_6` selects the kind, where 1 is only eligible for
/// the record `field_64A` already points at.

/// 0x1C-byte scratch taken off the scratch stack by `Actor00400_Fn031A4` while
/// it searches `Actor100400Work.field_608` for the nearest record: `delta`
/// holds the XZ difference from `field_5E4`, `best` the smallest distance seen
/// so far and `index` the record being tested.
typedef struct Actor100400NearestScratch {
    /* 0x00 */ VECTOR delta;
    /* 0x10 */ s32    best;
    /* 0x14 */ s32    dist;
    /* 0x18 */ s16    index;
    /* 0x1A */ s16    bestIndex;
} Actor100400NearestScratch;
STATIC_ASSERT_SIZEOF(Actor100400NearestScratch, 0x1C);

/// 8-byte waypoint indexed by `Actor100400Work.field_65B` (wraps at 8);
/// `field_2` offsets the Y base in `Actor00400_Fn05D00`, and
/// `Actor00400_Fn02208` measures the XZ distance from the root coordinate.

typedef union Actor100400Mat {
    GfxMatrix matrix; // SDK and packed-coefficient views of the working matrix
    /// The same storage reused as the view-space position `coordLocalToWorld`
    /// fills in, once the rotation it held has been handed to the coordinate.
    SVECTOR vec;
} Actor100400Mat;
STATIC_ASSERT_SIZEOF(Actor100400Mat, 0x20);

typedef union Actor100400Flags {
    u32 word;
    u16 half;
    struct {
        u16 lo;
        s16 field_62E;
    } hi;
} Actor100400Flags;

typedef struct Actor100400Work {
    /* 0x000 */ AnimationContext      anim;
    /* 0x014 */ AnimationSlot         slots[15];
    /* 0x26C */ byte                  poses[0xF0];
    /* 0x35C */ WorldCollisionBody    obj_35C;
    /* 0x37C */ WorldCollisionBody    obj_37C;
    /* 0x39C */ WorldCollisionContact field_39C[6];
    /* 0x42C */ WorldCollisionBody    obj_42C;
    /* 0x44C */ WorldCollisionContact field_44C[6];
    /* 0x4DC */ WorldCollisionBody    obj_4DC;
    /* 0x4FC */ WorldCollisionContact rec_4FC[3];
    /* 0x544 */ byte                  pad_544[2];
    /* 0x546 */ u16                   field_546;
    /* 0x548 */ byte                  pad_548[4];
    /* 0x54C */ s16                   field_54C;
    /* 0x54E */ s16                   field_54E;
    /* 0x550 */ s16                   field_550;
    /* 0x552 */ byte                  pad_552[2];
    /* 0x554 */ s16                   field_554;
    /* 0x556 */ s16                   field_556;
    /* 0x558 */ s16                   field_558;
    /* 0x55A */ byte                  pad_55A[0xA];
    /* 0x564 */ s16                   field_564;
    /* 0x566 */ byte                  pad_566[2];
    /* 0x568 */ s16                   field_568;
    /* 0x56A */ byte                  pad_56A[2];
    /* 0x56C */ SVECTOR               field_56C;
    /* 0x574 */ SVECTOR               field_574;
    /* 0x57C */ MATRIX                field_57C;
    /* 0x59C */ MATRIX                field_59C;
    /* 0x5BC */ MATRIX                field_5BC;
    /* 0x5DC */ EffectSpawnArg        field_5DC;
    /* 0x5E4 */ SVECTOR               field_5E4;
    /* 0x5EC */ SVECTOR               field_5EC;
    /* 0x5F4 */ SVECTOR               field_5F4;
    /* 0x5FC */ byte                  pad_5FC[0xC];
    /* 0x608 */ SVECTOR*              field_608;
    /* 0x60C */ SVECTOR*              field_60C;
    /* 0x610 */ s32                   field_610;
    /* 0x614 */ s16                   field_614[3];
    /* 0x61A */ byte                  pad_61A[2];
    /* 0x61C */ s16                   field_61C;
    /* 0x61E */ s16                   field_61E;
    /* 0x620 */ s16                   field_620;
    /* 0x622 */ s16                   field_622;
    /* 0x624 */ s16                   animRequest;
    /* 0x626 */ s16                   animPlaying;
    /* 0x628 */ s16                   animClip;
    /* 0x62A */ s16                   field_62A;
    /* 0x62C */ Actor100400Flags      flags_62C;
    /* 0x630 */ s16                   field_630;
    /* 0x632 */ s16                   animStep;
    /* 0x634 */ u16                   field_634;
    /* 0x636 */ s16                   field_636;
    /* 0x638 */ s16                   field_638;
    /* 0x63A */ u16                   subState;
    /* 0x63C */ s16                   animBlend;
    /* 0x63E */ s16                   field_63E;
    /* 0x640 */ s16                   field_640;
    /* 0x642 */ s16                   field_642;
    /* 0x644 */ s16                   field_644;
    /* 0x646 */ s16                   field_646;
    /* 0x648 */ s16                   field_648;
    /* 0x64A */ s16                   field_64A;
    /* 0x64C */ s16                   field_64C;
    /* 0x64E */ s16                   field_64E;
    /* 0x650 */ s16                   field_650;
    /* 0x652 */ s16                   field_652;
    /* 0x654 */ s16                   field_654;
    /* 0x656 */ byte                  pad_656[2];
    /* 0x658 */ u16                   field_658;
    /* 0x65A */ u8                    field_65A;
    /* 0x65B */ u8                    field_65B;
    /* 0x65C */ byte                  pad_65C[1];
    /* 0x65D */ u8                    field_65D;
    /* 0x65E */ u8                    field_65E;
    /* 0x65F */ u8                    field_65F;
    /* 0x660 */ u8                    field_660;
    /* 0x661 */ u8                    field_661;
    /* 0x662 */ byte                  pad_662[1];
    /* 0x663 */ u8                    field_663;
    /* 0x664 */ u8                    field_664;
    /* 0x665 */ s8                    field_665;
    /* 0x666 */ u8                    field_666;
} Actor100400Work;

/// The Diver library's name for this package's work block (see diver.h).
typedef Actor100400Work DiverWork;

/// One 0x14-byte row of `Actor00400_D15F20`, the per-room spawn table the entry
/// state walks until `area` reads 0xFF. A row matches when its `area` / `room`
/// equal `GameSession.location.loc.stage` / `location.loc.area`; `flags` bit 1 rejects the actor
/// outright and bit 2 hides its root coordinate. The three pointers are
/// optional overrides taken from the room overlay: `waypointSets` is indexed by
/// the spawn argument's second nibble, `records` becomes
/// `Actor100400Work.field_608` and `height` seeds `field_64E`.
typedef struct Actor100400AreaConfig {
    /* 0x00 */ SVECTOR** waypointSets;
    /* 0x04 */ SVECTOR*  records;
    /* 0x08 */ s16*      height;
    /* 0x0C */ s16       area;
    /* 0x0E */ s16       room;
    /* 0x10 */ u16       flags;
    /* 0x12 */ byte      pad_12[2];
} Actor100400AreaConfig;
STATIC_ASSERT_SIZEOF(Actor100400AreaConfig, 0x14);

static void Actor00400_Fn0875C(Task* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);
static void Actor00400_Fn088EC(Task* arg0, s16 arg1, s16 arg2, s16 arg3);
static void Actor00400_Fn02648(Task* arg0, s32 arg1);
static void Actor00400_Fn0237C(Task* arg0);
static void Actor00400_Fn02FF8(Task* arg0);
static void Actor00400_Fn0A190(Task* arg0);
static void Actor00400_Fn089C8(Task* arg0);
static void Actor00400_Fn03920(Task* arg0);
static void Actor00400_Fn04580(Task* arg0);
static void Actor00400_Fn04B48(Task* arg0);
static void Actor00400_Fn04E18(Task* arg0);
static void Actor00400_Fn040DC(Task* arg0);
static void Actor00400_Fn06B7C(Task* arg0);
static void Actor00400_Fn070C0(Task* arg0);
static void Actor00400_Fn08624(Task* arg0);
static s16  Actor00400_Fn086FC(Task* arg0, s16 arg1);
static void Actor00400_Fn08814(Task* arg0);
static s16  Actor00400_Fn08908(Task* arg0);
static void Actor00400_Fn0824C(Task* arg0, s16 arg1, s16 arg2, SVECTOR* arg3);
static void Actor00400_Fn08464(Task* arg0, s16 arg1, s16 arg2, SVECTOR* arg3);
static void Actor00400_Fn060CC(Task* arg0);
static void Actor00400_Fn097C8(Task* arg0);
static void Actor00400_Fn06EA4(Task* arg0);
static void Actor00400_Fn08ADC(Task* arg0);
static void Actor00400_Fn08A88(Task* arg0);
static void Actor00400_Fn08B40(Task* arg0);
static void Actor00400_Fn08B94(Task* arg0);
static void Actor00400_Fn06F64(Task* arg0);
static void Actor00400_Fn0A880(Task* arg0);
static void Actor00400_Fn04900(Task* arg0);
static void Actor00400_Fn0A940(Task* arg0);
static void Actor00400_Fn04A1C(Task* arg0);
static void Actor00400_Fn0A9F4(Task* arg0);
static void Actor00400_Fn0AA40(Task* arg0);
static void Actor00400_Fn0A3D4(Task* arg0);
static void Actor00400_Fn0A414(Task* arg0);
static void Actor00400_Fn098A8(Task* arg0);
static void Actor00400_Fn09924(Task* arg0);
static s16  Actor00400_Fn02154(Task* arg0);
static void Actor00400_Fn0A5B8(Task* arg0);
static void Actor00400_Fn019B4(Task* arg0);
static void Actor00400_Fn0814C(Task* arg0, s16 arg1, SVECTOR* arg2, s16 arg3);
static void Actor00400_Fn08A1C(MATRIX* src, MATRIX* dst);

static s32  Actor00400_Fn02208(Task* arg0);
static void Actor00400_Fn0A680(Task* arg0);
static void Actor00400_Fn0A6B0(Task* arg0);
static void Actor00400_Fn0A704(Task* arg0);
static void Actor00400_Fn0A760(Task* arg0);
static void Actor00400_Fn0A7F0(Task* arg0);
static void Actor00400_Fn0A82C(Task* arg0);
static void Actor00400_Fn0A034(Task* arg0);
static void Actor00400_Fn0A510(Task* arg0);
static void Actor00400_Fn0A57C(Task* arg0);

/* States the dispatch tables name before their definitions. */
static void Actor00400_Fn042C0(Task* arg0);
static void Actor00400_Fn04414(Task* arg0);
static void Actor00400_Fn04CF8(Task* arg0);
static void Actor00400_Fn05728(Task* arg0);
static void Actor00400_Fn07738(Task* arg0);
static void Actor00400_Fn077F4(Task* arg0);
static void Actor00400_Fn078C8(Task* arg0);
static void Actor00400_Fn0793C(Task* arg0);
static void Actor00400_Fn07998(Task* task);
static void Actor00400_Fn079A0(Task* task);
static void Actor00400_Fn079A8(Task* arg0);
static void Actor00400_Fn079FC(Task* arg0);
static void Actor00400_Fn07ABC(Task* arg0);
static void Actor00400_Fn07B10(Task* arg0);
static void Actor00400_Fn07B98(Task* arg0);
static void Actor00400_Fn07C04(Task* arg0);
static void Actor00400_Fn07CC4(Task* arg0);
static void Actor00400_Fn07DE0(Task* arg0);
static void Actor00400_Fn07E20(Task* arg0);
static void Actor00400_Fn07E74(Task* arg0);
static void Actor00400_Fn07EE8(Task* arg0);
static void Actor00400_Fn07F18(Task* arg0);
static void Actor00400_Fn07F44(Task* arg0);
static void Actor00400_Fn07F88(Task* arg0);
static void Actor00400_Fn07FEC(Task* arg0);
static void Actor00400_Fn08C54(Task* arg0);
static void Actor00400_Fn08D70(Task* arg0);
static void Actor00400_Fn08DFC(Task* arg0);
static void Actor00400_Fn08E50(Task* arg0);
static void Actor00400_Fn08FB0(Task* arg0);
static void Actor00400_Fn08FC8(Task* arg0);
static void Actor00400_Fn08FF4(Task* arg0);
static void Actor00400_Fn09038(Task* arg0);
static void Actor00400_Fn0909C(Task* arg0);
static void Actor00400_Fn090B4(Task* arg0);
static void Actor00400_Fn09124(Task* arg0);
static void Actor00400_Fn091F8(Task* arg0);
static void Actor00400_Fn09260(Task* arg0);
static void Actor00400_Fn092D4(Task* arg0);
static void Actor00400_Fn09348(Task* arg0);
static void Actor00400_Fn093BC(Task* task);
static void Actor00400_Fn093C4(Task* arg0);
static void Actor00400_Fn09418(Task* arg0);
static void Actor00400_Fn0946C(Task* arg0);
static void Actor00400_Fn094C0(Task* arg0);
static void Actor00400_Fn094DC(Task* arg0);
static void Actor00400_Fn095D8(Task* arg0);
static void Actor00400_Fn096C0(Task* arg0);
static void Actor00400_Fn09A1C(Task* arg0);
static void Actor00400_Fn09A48(Task* arg0);
static void Actor00400_Fn09A8C(Task* arg0);
static void Actor00400_Fn09AE0(Task* arg0);
static void Actor00400_Fn09B44(Task* arg0);
static void Actor00400_Fn09B74(Task* arg0);
static void Actor00400_Fn09BDC(Task* arg0);
static void Actor00400_Fn09C04(Task* arg0);
static void Actor00400_Fn09C84(Task* arg0);
static void Actor00400_Fn09CCC(Task* arg0);
static void Actor00400_Fn09D3C(Task* arg0);
static void Actor00400_Fn09D98(Task* arg0);
static void Actor00400_Fn09E70(Task* arg0);
static void Actor00400_Fn09F18(Task* arg0);
static void Actor00400_Fn09FDC(Task* arg0);

extern EnemyParams Actor00400_D0FDC8;
/// Pair table `Actor00400_Fn0A190` packs, at index 1, into the marker object's
/// `key`.
extern DamageAttack          Actor00400_D0FDC0[2];
extern TaskDesc              Actor00400_D16028[];
extern Actor100400AreaConfig Actor00400_D15F20[];
// Typed callback views for the task message dispatcher.

extern TaskMessageEntry Actor00400_D16010[3];

extern u16           Actor00400_D1609C[8];
extern AnimationSet* Actor00400_D1604C[20];
static TmdSource     _gActor00400DiverBurstHead;
static TmdSource     _gActor00400DiverBurstArmRight;
static TmdSource     _gActor00400DiverBurstArmLeft1;
static TmdSource     _gActor00400DiverBurstLegRight;
static TmdSource     _gActor00400DiverBurstArmLeft2;
static TmdSource     _gActor00400DiverEnergyBall;
extern void*         D_800678F0[1];

/* Inline rotation traversal helpers. Every ancestor rotation is copied out
   and renormalised before it is fed to the GTE, instead of being loaded
   straight from the coordinate. */

static AnimationSet _gActor00400Actor100400Animation10348;
static AnimationSet _gActor00400Actor100400Animation105F4;
static AnimationSet _gActor00400Actor100400Animation1097C;
static AnimationSet _gActor00400Actor100400Animation11054;
static AnimationSet _gActor00400Actor100400Animation11918;
static AnimationSet _gActor00400Actor100400Animation11C64;
static AnimationSet _gActor00400Actor100400Animation1236C;
static AnimationSet _gActor00400Actor100400Animation12C50;
static AnimationSet _gActor00400Actor100400Animation132B0;
static AnimationSet _gActor00400Actor100400Animation136E8;
static AnimationSet _gActor00400Actor100400Animation13C28;
static AnimationSet _gActor00400Actor100400Animation13FB0;
static AnimationSet _gActor00400Actor100400Animation14608;
static AnimationSet _gActor00400Actor100400Animation14C08;
static AnimationSet _gActor00400Actor100400Animation15050;
static AnimationSet _gActor00400Actor100400Animation15340;
static AnimationSet _gActor00400Actor100400Animation15624;
static AnimationSet _gActor00400Actor100400Animation15B34;
static AnimationSet _gActor00400Actor100400Animation15EF8;
static TmdSource    _gActor00400DiverBody;
void                Actor00400_Fn076E8(Task*);
void                Actor00400_Fn08004(Task*);
void                Actor00400_Fn0805C(Task* task, s32 msgId, ActorCommand* request, s32 arg3);
void                Actor00400_Fn08354(Task*, s32, s32, s32);
void                Actor00400_Fn08948(Task*);

static TmdBone _gActor00400DiverBodySkeleton[15] = {
#include "assets/diver_body_skeleton.inc"
};

static u32 _gActor00400DiverBodyPartVerts[15] = {
#include "assets/diver_body_partVerts.inc"
};

static SVECTOR _gActor00400DiverBodyVerts[145] = {
#include "assets/diver_body_verts.inc"
};

static SVECTOR _gActor00400DiverBodyNormals[142] = {
#include "assets/diver_body_normals.inc"
};

static u32 _gActor00400DiverBodyStream[2494] = {
#include "assets/diver_body_stream.inc"
};

static TmdSource _gActor00400DiverBody = {
    0,
    11652,
    5104,
    15,
    _gActor00400DiverBodyPartVerts,
    _gActor00400DiverBodyVerts,
    _gActor00400DiverBodyNormals,
    _gActor00400DiverBodySkeleton,
    _gActor00400DiverBodyStream,
};

static TmdBone _gActor00400DiverBurstHeadSkeleton[1] = {
#include "assets/diver_burst_head_skeleton.inc"
};

static u32 _gActor00400DiverBurstHeadPartVerts[1] = {
#include "assets/diver_burst_head_partVerts.inc"
};

static SVECTOR _gActor00400DiverBurstHeadVerts[35] = {
#include "assets/diver_burst_head_verts.inc"
};

static SVECTOR _gActor00400DiverBurstHeadNormals[44] = {
#include "assets/diver_burst_head_normals.inc"
};

static u32 _gActor00400DiverBurstHeadStream[360] = {
#include "assets/diver_burst_head_stream.inc"
};

static TmdSource _gActor00400DiverBurstHead = {
    0,
    2388,
    0,
    1,
    _gActor00400DiverBurstHeadPartVerts,
    _gActor00400DiverBurstHeadVerts,
    _gActor00400DiverBurstHeadNormals,
    _gActor00400DiverBurstHeadSkeleton,
    _gActor00400DiverBurstHeadStream,
};

static TmdBone _gActor00400DiverBurstArmRightSkeleton[1] = {
#include "assets/diver_burst_arm_right_skeleton.inc"
};

static u32 _gActor00400DiverBurstArmRightPartVerts[1] = {
#include "assets/diver_burst_arm_right_partVerts.inc"
};

static SVECTOR _gActor00400DiverBurstArmRightVerts[14] = {
#include "assets/diver_burst_arm_right_verts.inc"
};

static SVECTOR _gActor00400DiverBurstArmRightNormals[24] = {
#include "assets/diver_burst_arm_right_normals.inc"
};

static u32 _gActor00400DiverBurstArmRightStream[143] = {
#include "assets/diver_burst_arm_right_stream.inc"
};

static TmdSource _gActor00400DiverBurstArmRight = {
    0,
    904,
    0,
    1,
    _gActor00400DiverBurstArmRightPartVerts,
    _gActor00400DiverBurstArmRightVerts,
    _gActor00400DiverBurstArmRightNormals,
    _gActor00400DiverBurstArmRightSkeleton,
    _gActor00400DiverBurstArmRightStream,
};

static TmdBone _gActor00400DiverBurstArmLeft1Skeleton[1] = {
#include "assets/diver_burst_arm_left_1_skeleton.inc"
};

static u32 _gActor00400DiverBurstArmLeft1PartVerts[1] = {
#include "assets/diver_burst_arm_left_1_partVerts.inc"
};

static SVECTOR _gActor00400DiverBurstArmLeft1Verts[14] = {
#include "assets/diver_burst_arm_left_1_verts.inc"
};

static SVECTOR _gActor00400DiverBurstArmLeft1Normals[24] = {
#include "assets/diver_burst_arm_left_1_normals.inc"
};

static u32 _gActor00400DiverBurstArmLeft1Stream[143] = {
#include "assets/diver_burst_arm_left_1_stream.inc"
};

static TmdSource _gActor00400DiverBurstArmLeft1 = {
    0,
    904,
    0,
    1,
    _gActor00400DiverBurstArmLeft1PartVerts,
    _gActor00400DiverBurstArmLeft1Verts,
    _gActor00400DiverBurstArmLeft1Normals,
    _gActor00400DiverBurstArmLeft1Skeleton,
    _gActor00400DiverBurstArmLeft1Stream,
};

static TmdBone _gActor00400DiverBurstLegRightSkeleton[1] = {
#include "assets/diver_burst_leg_right_skeleton.inc"
};

static u32 _gActor00400DiverBurstLegRightPartVerts[1] = {
#include "assets/diver_burst_leg_right_partVerts.inc"
};

static SVECTOR _gActor00400DiverBurstLegRightVerts[19] = {
#include "assets/diver_burst_leg_right_verts.inc"
};

static SVECTOR _gActor00400DiverBurstLegRightNormals[33] = {
#include "assets/diver_burst_leg_right_normals.inc"
};

static u32 _gActor00400DiverBurstLegRightStream[210] = {
#include "assets/diver_burst_leg_right_stream.inc"
};

static TmdSource _gActor00400DiverBurstLegRight = {
    0,
    1360,
    0,
    1,
    _gActor00400DiverBurstLegRightPartVerts,
    _gActor00400DiverBurstLegRightVerts,
    _gActor00400DiverBurstLegRightNormals,
    _gActor00400DiverBurstLegRightSkeleton,
    _gActor00400DiverBurstLegRightStream,
};

static TmdBone _gActor00400DiverBurstArmLeft2Skeleton[1] = {
#include "assets/diver_burst_arm_left_2_skeleton.inc"
};

static u32 _gActor00400DiverBurstArmLeft2PartVerts[1] = {
#include "assets/diver_burst_arm_left_2_partVerts.inc"
};

static SVECTOR _gActor00400DiverBurstArmLeft2Verts[19] = {
#include "assets/diver_burst_arm_left_2_verts.inc"
};

static SVECTOR _gActor00400DiverBurstArmLeft2Normals[33] = {
#include "assets/diver_burst_arm_left_2_normals.inc"
};

static u32 _gActor00400DiverBurstArmLeft2Stream[210] = {
#include "assets/diver_burst_arm_left_2_stream.inc"
};

static TmdSource _gActor00400DiverBurstArmLeft2 = {
    0,
    1360,
    0,
    1,
    _gActor00400DiverBurstArmLeft2PartVerts,
    _gActor00400DiverBurstArmLeft2Verts,
    _gActor00400DiverBurstArmLeft2Normals,
    _gActor00400DiverBurstArmLeft2Skeleton,
    _gActor00400DiverBurstArmLeft2Stream,
};

static TmdBone _gActor00400DiverEnergyBallSkeleton[1] = {
#include "assets/diver_energy_ball_skeleton.inc"
};

static u32 _gActor00400DiverEnergyBallPartVerts[1] = {
#include "assets/diver_energy_ball_partVerts.inc"
};

static SVECTOR _gActor00400DiverEnergyBallVerts[24] = {
#include "assets/diver_energy_ball_verts.inc"
};

static SVECTOR _gActor00400DiverEnergyBallNormals[25] = {
#include "assets/diver_energy_ball_normals.inc"
};

static u32 _gActor00400DiverEnergyBallStream[270] = {
#include "assets/diver_energy_ball_stream.inc"
};

static TmdSource _gActor00400DiverEnergyBall = {
    0,
    1760,
    0,
    1,
    _gActor00400DiverEnergyBallPartVerts,
    _gActor00400DiverEnergyBallVerts,
    _gActor00400DiverEnergyBallNormals,
    _gActor00400DiverEnergyBallSkeleton,
    _gActor00400DiverEnergyBallStream,
};

DamageAttack Actor00400_D0FDC0[2] = {
    { 18, 5 },
    { 24, 5 },
};

EnemyParams Actor00400_D0FDC8 = { Actor00400_D0FDC0, 240, 70, 88, 3, 100, 10, 100, 0 };

static AnimationPackedPose _gActor00400Actor100400Animation10348Bank1[8] = {
#include "assets/actor_100400_animation_10348_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation10348Bank4[133] = {
#include "assets/actor_100400_animation_10348_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation10348Records[183] = {
#include "assets/actor_100400_animation_10348_records.inc"
};

static u16 _gActor00400Actor100400Animation10348Indices[16] = {
#include "assets/actor_100400_animation_10348_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation10348 = {
    _gActor00400Actor100400Animation10348Records,
    _gActor00400Actor100400Animation10348Indices,
    { NULL, _gActor00400Actor100400Animation10348Bank1, NULL, NULL, _gActor00400Actor100400Animation10348Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation105F4Bank1[3] = {
#include "assets/actor_100400_animation_105F4_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation105F4Bank4[49] = {
#include "assets/actor_100400_animation_105F4_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation105F4Records[95] = {
#include "assets/actor_100400_animation_105F4_records.inc"
};

static u16 _gActor00400Actor100400Animation105F4Indices[16] = {
#include "assets/actor_100400_animation_105F4_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation105F4 = {
    _gActor00400Actor100400Animation105F4Records,
    _gActor00400Actor100400Animation105F4Indices,
    { NULL, _gActor00400Actor100400Animation105F4Bank1, NULL, NULL, _gActor00400Actor100400Animation105F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation1097CBank1[3] = {
#include "assets/actor_100400_animation_1097C_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation1097CBank4[78] = {
#include "assets/actor_100400_animation_1097C_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation1097CRecords[121] = {
#include "assets/actor_100400_animation_1097C_records.inc"
};

static u16 _gActor00400Actor100400Animation1097CIndices[16] = {
#include "assets/actor_100400_animation_1097C_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation1097C = {
    _gActor00400Actor100400Animation1097CRecords,
    _gActor00400Actor100400Animation1097CIndices,
    { NULL, _gActor00400Actor100400Animation1097CBank1, NULL, NULL, _gActor00400Actor100400Animation1097CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation11054Bank1[15] = {
#include "assets/actor_100400_animation_11054_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation11054Bank4[150] = {
#include "assets/actor_100400_animation_11054_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation11054Records[225] = {
#include "assets/actor_100400_animation_11054_records.inc"
};

static u16 _gActor00400Actor100400Animation11054Indices[16] = {
#include "assets/actor_100400_animation_11054_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation11054 = {
    _gActor00400Actor100400Animation11054Records,
    _gActor00400Actor100400Animation11054Indices,
    { NULL, _gActor00400Actor100400Animation11054Bank1, NULL, NULL, _gActor00400Actor100400Animation11054Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation11918Bank1[8] = {
#include "assets/actor_100400_animation_11918_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation11918Bank4[174] = {
#include "assets/actor_100400_animation_11918_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation11918Records[345] = {
#include "assets/actor_100400_animation_11918_records.inc"
};

static u16 _gActor00400Actor100400Animation11918Indices[16] = {
#include "assets/actor_100400_animation_11918_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation11918 = {
    _gActor00400Actor100400Animation11918Records,
    _gActor00400Actor100400Animation11918Indices,
    { NULL, _gActor00400Actor100400Animation11918Bank1, NULL, NULL, _gActor00400Actor100400Animation11918Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation11C64Bank1[6] = {
#include "assets/actor_100400_animation_11C64_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation11C64Bank4[57] = {
#include "assets/actor_100400_animation_11C64_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation11C64Records[118] = {
#include "assets/actor_100400_animation_11C64_records.inc"
};

static u16 _gActor00400Actor100400Animation11C64Indices[16] = {
#include "assets/actor_100400_animation_11C64_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation11C64 = {
    _gActor00400Actor100400Animation11C64Records,
    _gActor00400Actor100400Animation11C64Indices,
    { NULL, _gActor00400Actor100400Animation11C64Bank1, NULL, NULL, _gActor00400Actor100400Animation11C64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation1236CBank1[10] = {
#include "assets/actor_100400_animation_1236C_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation1236CBank4[167] = {
#include "assets/actor_100400_animation_1236C_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation1236CRecords[235] = {
#include "assets/actor_100400_animation_1236C_records.inc"
};

static u16 _gActor00400Actor100400Animation1236CIndices[16] = {
#include "assets/actor_100400_animation_1236C_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation1236C = {
    _gActor00400Actor100400Animation1236CRecords,
    _gActor00400Actor100400Animation1236CIndices,
    { NULL, _gActor00400Actor100400Animation1236CBank1, NULL, NULL, _gActor00400Actor100400Animation1236CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation12C50Bank1[10] = {
#include "assets/actor_100400_animation_12C50_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation12C50Bank4[213] = {
#include "assets/actor_100400_animation_12C50_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation12C50Records[308] = {
#include "assets/actor_100400_animation_12C50_records.inc"
};

static u16 _gActor00400Actor100400Animation12C50Indices[16] = {
#include "assets/actor_100400_animation_12C50_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation12C50 = {
    _gActor00400Actor100400Animation12C50Records,
    _gActor00400Actor100400Animation12C50Indices,
    { NULL, _gActor00400Actor100400Animation12C50Bank1, NULL, NULL, _gActor00400Actor100400Animation12C50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation132B0Bank1[14] = {
#include "assets/actor_100400_animation_132B0_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation132B0Bank4[147] = {
#include "assets/actor_100400_animation_132B0_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation132B0Records[201] = {
#include "assets/actor_100400_animation_132B0_records.inc"
};

static u16 _gActor00400Actor100400Animation132B0Indices[16] = {
#include "assets/actor_100400_animation_132B0_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation132B0 = {
    _gActor00400Actor100400Animation132B0Records,
    _gActor00400Actor100400Animation132B0Indices,
    { NULL, _gActor00400Actor100400Animation132B0Bank1, NULL, NULL, _gActor00400Actor100400Animation132B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation136E8Bank1[6] = {
#include "assets/actor_100400_animation_136E8_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation136E8Bank4[103] = {
#include "assets/actor_100400_animation_136E8_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation136E8Records[131] = {
#include "assets/actor_100400_animation_136E8_records.inc"
};

static u16 _gActor00400Actor100400Animation136E8Indices[16] = {
#include "assets/actor_100400_animation_136E8_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation136E8 = {
    _gActor00400Actor100400Animation136E8Records,
    _gActor00400Actor100400Animation136E8Indices,
    { NULL, _gActor00400Actor100400Animation136E8Bank1, NULL, NULL, _gActor00400Actor100400Animation136E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation13C28Bank1[15] = {
#include "assets/actor_100400_animation_13C28_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation13C28Bank4[109] = {
#include "assets/actor_100400_animation_13C28_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation13C28Records[164] = {
#include "assets/actor_100400_animation_13C28_records.inc"
};

static u16 _gActor00400Actor100400Animation13C28Indices[16] = {
#include "assets/actor_100400_animation_13C28_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation13C28 = {
    _gActor00400Actor100400Animation13C28Records,
    _gActor00400Actor100400Animation13C28Indices,
    { NULL, _gActor00400Actor100400Animation13C28Bank1, NULL, NULL, _gActor00400Actor100400Animation13C28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation13FB0Bank1[6] = {
#include "assets/actor_100400_animation_13FB0_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation13FB0Bank4[78] = {
#include "assets/actor_100400_animation_13FB0_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation13FB0Records[112] = {
#include "assets/actor_100400_animation_13FB0_records.inc"
};

static u16 _gActor00400Actor100400Animation13FB0Indices[16] = {
#include "assets/actor_100400_animation_13FB0_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation13FB0 = {
    _gActor00400Actor100400Animation13FB0Records,
    _gActor00400Actor100400Animation13FB0Indices,
    { NULL, _gActor00400Actor100400Animation13FB0Bank1, NULL, NULL, _gActor00400Actor100400Animation13FB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation14608Bank1[12] = {
#include "assets/actor_100400_animation_14608_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation14608Bank4[149] = {
#include "assets/actor_100400_animation_14608_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation14608Records[203] = {
#include "assets/actor_100400_animation_14608_records.inc"
};

static u16 _gActor00400Actor100400Animation14608Indices[16] = {
#include "assets/actor_100400_animation_14608_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation14608 = {
    _gActor00400Actor100400Animation14608Records,
    _gActor00400Actor100400Animation14608Indices,
    { NULL, _gActor00400Actor100400Animation14608Bank1, NULL, NULL, _gActor00400Actor100400Animation14608Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation14C08Bank1[20] = {
#include "assets/actor_100400_animation_14C08_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation14C08Bank4[126] = {
#include "assets/actor_100400_animation_14C08_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation14C08Records[180] = {
#include "assets/actor_100400_animation_14C08_records.inc"
};

static u16 _gActor00400Actor100400Animation14C08Indices[16] = {
#include "assets/actor_100400_animation_14C08_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation14C08 = {
    _gActor00400Actor100400Animation14C08Records,
    _gActor00400Actor100400Animation14C08Indices,
    { NULL, _gActor00400Actor100400Animation14C08Bank1, NULL, NULL, _gActor00400Actor100400Animation14C08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation15050Bank1[9] = {
#include "assets/actor_100400_animation_15050_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation15050Bank4[94] = {
#include "assets/actor_100400_animation_15050_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation15050Records[135] = {
#include "assets/actor_100400_animation_15050_records.inc"
};

static u16 _gActor00400Actor100400Animation15050Indices[16] = {
#include "assets/actor_100400_animation_15050_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation15050 = {
    _gActor00400Actor100400Animation15050Records,
    _gActor00400Actor100400Animation15050Indices,
    { NULL, _gActor00400Actor100400Animation15050Bank1, NULL, NULL, _gActor00400Actor100400Animation15050Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation15340Bank1[5] = {
#include "assets/actor_100400_animation_15340_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation15340Bank4[43] = {
#include "assets/actor_100400_animation_15340_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation15340Records[112] = {
#include "assets/actor_100400_animation_15340_records.inc"
};

static u16 _gActor00400Actor100400Animation15340Indices[16] = {
#include "assets/actor_100400_animation_15340_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation15340 = {
    _gActor00400Actor100400Animation15340Records,
    _gActor00400Actor100400Animation15340Indices,
    { NULL, _gActor00400Actor100400Animation15340Bank1, NULL, NULL, _gActor00400Actor100400Animation15340Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation15624Bank1[6] = {
#include "assets/actor_100400_animation_15624_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation15624Bank4[42] = {
#include "assets/actor_100400_animation_15624_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation15624Records[107] = {
#include "assets/actor_100400_animation_15624_records.inc"
};

static u16 _gActor00400Actor100400Animation15624Indices[16] = {
#include "assets/actor_100400_animation_15624_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation15624 = {
    _gActor00400Actor100400Animation15624Records,
    _gActor00400Actor100400Animation15624Indices,
    { NULL, _gActor00400Actor100400Animation15624Bank1, NULL, NULL, _gActor00400Actor100400Animation15624Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation15B34Bank1[11] = {
#include "assets/actor_100400_animation_15B34_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation15B34Bank4[108] = {
#include "assets/actor_100400_animation_15B34_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation15B34Records[165] = {
#include "assets/actor_100400_animation_15B34_records.inc"
};

static u16 _gActor00400Actor100400Animation15B34Indices[16] = {
#include "assets/actor_100400_animation_15B34_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation15B34 = {
    _gActor00400Actor100400Animation15B34Records,
    _gActor00400Actor100400Animation15B34Indices,
    { NULL, _gActor00400Actor100400Animation15B34Bank1, NULL, NULL, _gActor00400Actor100400Animation15B34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00400Actor100400Animation15EF8Bank1[6] = {
#include "assets/actor_100400_animation_15EF8_bank1.inc"
};

static AnimationPackedRotation _gActor00400Actor100400Animation15EF8Bank4[80] = {
#include "assets/actor_100400_animation_15EF8_bank4.inc"
};

static AnimationRecord _gActor00400Actor100400Animation15EF8Records[125] = {
#include "assets/actor_100400_animation_15EF8_records.inc"
};

static u16 _gActor00400Actor100400Animation15EF8Indices[16] = {
#include "assets/actor_100400_animation_15EF8_indices.inc"
};

static AnimationSet _gActor00400Actor100400Animation15EF8 = {
    _gActor00400Actor100400Animation15EF8Records,
    _gActor00400Actor100400Animation15EF8Indices,
    { NULL, _gActor00400Actor100400Animation15EF8Bank1, NULL, NULL, _gActor00400Actor100400Animation15EF8Bank4, NULL, NULL, NULL },
};

Actor100400AreaConfig Actor00400_D15F20[12] = {
    { NULL, NULL, NULL, 4, 46, 0, { 0, 0 } },
    { D_shelter_b4_reservoir_801851D4, D_shelter_b4_reservoir_801851E4, &D_shelter_b4_reservoir_80184F80, 4, 45, 2, { 0, 0 } },
    { D_shelter_b2_septic_tank_801836A4, D_shelter_b2_septic_tank_801836B4, &D_shelter_b2_septic_tank_801832BC, 4, 34, 2, { 0, 0 } },
    { D_shelter_b4_upper_sewer_801866F8, D_shelter_b4_upper_sewer_80186708, &D_shelter_b4_upper_sewer_80186438, 4, 44, 2, { 0, 0 } },
    { D_neo_ark_bridge_801820BC, D_neo_ark_bridge_801820CC, &D_neo_ark_bridge_80181FF8.height, 5, 27, 0, { 0, 0 } },
    { D_neo_ark_submarine_gallery_80181B0C, D_neo_ark_submarine_gallery_80181B1C, &D_neo_ark_submarine_gallery_80181A48.height, 5, 30, 2, { 0, 0 } },
    { D_neo_ark_submarine_tunnel_80181F94, NULL, &D_neo_ark_submarine_tunnel_80181E90.height, 5, 12, 0, { 0, 0 } },
    { D_neo_ark_pavilion_80183A64, D_neo_ark_pavilion_80183A74, &D_neo_ark_pavilion_801839A0.height, 5, 13, 0, { 0, 0 } },
    { D_neo_ark_island_80181CE8, D_neo_ark_island_80181CF8, &D_neo_ark_island_80181C24.height, 5, 14, 0, { 0, 0 } },
    { D_shelter_b2_main_corridor_80182EEC, D_shelter_b2_main_corridor_80182EFC, &D_shelter_b2_main_corridor_80182E28, 4, 33, 0, { 0, 0 } },
    { D_shelter_b4_lower_sewer_8018210C, D_shelter_b4_lower_sewer_8018211C, &D_shelter_b4_lower_sewer_80181E6C, 4, 43, 2, { 0, 0 } },
    { NULL, NULL, NULL, 255, 0, 0, { 0, 0 } },
};

TaskMessageEntry Actor00400_D16010[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor00400_Fn0805C },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, Actor00400_Fn08354 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor00400_D16028[3] = {
    { { { TASK_BODY_TMD, 96 } }, Actor00400_Fn08948, { .model = &_gActor00400DiverBody } },
    { { { TASK_BODY_COORD, 96 } }, Actor00400_Fn08004, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, Actor00400_Fn076E8, { .value = 0 } },
};

AnimationSet* Actor00400_D1604C[20] = {
    NULL,
    &_gActor00400Actor100400Animation10348,
    &_gActor00400Actor100400Animation105F4,
    &_gActor00400Actor100400Animation1097C,
    &_gActor00400Actor100400Animation11054,
    &_gActor00400Actor100400Animation11918,
    &_gActor00400Actor100400Animation11C64,
    &_gActor00400Actor100400Animation1236C,
    &_gActor00400Actor100400Animation12C50,
    &_gActor00400Actor100400Animation132B0,
    &_gActor00400Actor100400Animation136E8,
    &_gActor00400Actor100400Animation13C28,
    &_gActor00400Actor100400Animation13FB0,
    &_gActor00400Actor100400Animation14608,
    &_gActor00400Actor100400Animation14C08,
    &_gActor00400Actor100400Animation15050,
    &_gActor00400Actor100400Animation15340,
    &_gActor00400Actor100400Animation15624,
    &_gActor00400Actor100400Animation15B34,
    &_gActor00400Actor100400Animation15EF8,
};

u16 Actor00400_D1609C[8] = {
    190,
    200,
    210,
    220,
    230,
    250,
    270,
    290,
};

static void Actor00400_Fn0A468(Task* arg0);

static void Actor00400_Fn0A4BC(Task* arg0);

static void Actor00400_Fn0A2F4(Task* arg0);

static void Actor00400_Fn0A364(Task* arg0);

static void Actor00400_Fn0962C(Task* arg0);

static void Actor00400_Fn058C4(Task* arg0);

static void            Actor00400_Fn00A14(Task* arg0);
static void            Actor00400_Fn00B48(Task* arg0);
static void            Actor00400_Fn00C84(Task* arg0);
static void            Actor00400_Fn012B0(Task* arg0, s16 arg1, s32 arg2);
static void            Actor00400_Fn01454(Task* arg0);
static void            Actor00400_Fn016A4(Task* arg0, s32 arg1);
static void            Actor00400_Fn01B90(Task* arg0);
static void            Actor00400_Fn02D48(Task* arg0);
static void            Actor00400_Fn031A4(Task* arg0, SVECTOR* arg1);
static void            Actor00400_Fn03318(SVECTOR* corner0, SVECTOR* corner1, SVECTOR* corner2, SVECTOR* corner3, u8 shade);
static __inline__ s32  Actor00400_ApplyAreaConfig(Task* arg0);
static __inline__ void Actor00400_AttachHead(Task* arg0, Enemy* obj,
                                             Actor100400Work* work, s32 hide);
static __inline__ void Actor00400_UpdateColor(Task* arg0, GfxCoord* coord,
                                              Actor100400Work* work, TmdObject* ctx);
static inline void     Actor00400_TurnToward(Task* arg0, SVECTOR* target, s32 step, s32 range);
static inline void     Actor00400_SpawnRing(Task* arg0, Actor100400Work* work, GfxCoord* coord);
static void            Actor00400_Fn05320(Task* arg0);
static void            Actor00400_Fn05D00(Task* arg0);
static void            Actor00400_Fn05EA4(Task* arg0);
static void            Actor00400_Fn061E8(Task* arg0);
static void            Actor00400_Fn06380(Task* arg0);
static inline void     Actor00400_SpawnMarker(Task* arg0);
static void            Actor00400_Fn064B0(Task* arg0);
static void            Actor00400_Fn06798(Task* arg0);
static void            Actor00400_Fn06A44(Task* arg0);
static inline s32      Actor00400_ConsumeStateRequest(Actor100400Work* work);
static void            Actor00400_Fn07400(Task* arg0);
static void            Actor00400_Fn07518(Task* arg0);
static inline s32      Actor00400_TakeStateRequest(Task* arg0);

#include "../../shared/diver_inlines.inc.c"

#include "../../shared/diver_impact_burst.inc.c"

#include "../../shared/diver_draw_spark.inc.c"

static void Actor00400_Fn00A14(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->field_646 != 0) {
        if (!(work->field_646 & 7)) {
            s32 id  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4004000B;
            s32 pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        if (work->field_646 == 0x18 || work->field_646 == 0x30) {
            func_800FDB18(7, &arg0->extra.tmd->coords[1], NULL, &work->field_5DC);
        }
        if (work->field_646 == 0x16 && work->field_666 == 0) {
            work->obj_4DC.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        }
        if (--work->field_646 == 0) {
            work->obj_4DC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
}

static void Actor00400_Fn00B48(Task* arg0)
{
    TmdObject*       ctx;
    Actor100400Work* work;
    Enemy*           obj;
    GfxCoord*        coord;
    GfxCoord*        coords;
    u16              hp;
    u8               slot;

    ctx                        = arg0->extra.tmd;
    work                       = arg0->work;
    obj                        = arg0->spawnArg2.pointer;
    ctx->lightMtx              = &work->field_59C;
    ctx->flags                 = 0;
    ctx->colorMtx              = &work->field_57C;
    coords                     = arg0->extra.tmd->coords;
    coord                      = ctx->coords;
    work->field_5DC.spawnArgLo = 0x600;
    work->field_5DC.spawnArgHi = 3;
    work->field_664            = 4;
    work->field_5DC.coord      = &coords[1];
    obj->field_4               = &coord->coord;
    obj->field_48              = 0;
    obj->bodyPos.vx            = 0;
    obj->bodyPos.vy            = 0;
    obj->bodyPos.vz            = 0;
    slot                       = work->field_664;
    obj->coord                 = &arg0->extra.tmd->coords[slot];
    Gp_LinkNode(&obj->node);
    obj->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    obj->recs                   = work->field_39C;
    obj->param                  = &Actor00400_D0FDC8;
    hp                          = Actor00400_D0FDC8.hpMax;
    obj->hpMax                  = hp;
    obj->hp                     = hp;
    coord->parent               = &gGfxViewCoord;
    animationInitContext(&work->anim, Actor00400_D1604C, ctx, (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->poses, work->slots);
    Actor00400_Fn019B4(arg0);
    work->field_556 = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
}

static void Actor00400_Fn00C84(Task* arg0)
{
    Task*            actor;
    Task*            player;
    Actor100400Work* work;
    GfxCoord*        coord;
    GfxCoord*        pc;
    SVECTOR          pos;
    u8               frame;

    actor  = arg0;
    player = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    work   = actor->work;
    coord  = actor->extra.tmd->coords;
    if (work->animClip != 4) {
        Actor00400_Fn088EC(actor, 4, 0x20, 0xA);
        Actor00400_Fn08814(actor);
    }
    frame = Actor00400_Fn086FC(actor, 0x39);
    if (Actor00400_Fn08908(actor)) {
        work->field_62A = 0;
    }
    if (work->field_62A == 0) {
        Actor00400_Fn0824C(actor, 0xB, 0xE, (SVECTOR*)&work->field_564);
        if (work->pad_65C[0] & 1) {
            s32 id  = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040002;
            s32 pan = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
        } else {
            s32 id  = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040003;
            s32 pan = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
        }
        work->pad_65C[0]++;
    }
    if (work->field_62A >= 0 && frame >= work->field_62A) {
        pc     = player->extra.tmd->coords;
        pos.vx = pc->coord.t[0];
        pos.vy = pc->coord.t[1];
        pos.vz = pc->coord.t[2];
        Actor00400_Fn0875C(actor, &pos, 8, 0x100);
        Actor00400_Fn08464(actor, 0xB, 0xE, (SVECTOR*)&work->field_564);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/limb_shadows_segment.inc.c"

static void Actor00400_Fn012B0(Task* arg0, s16 arg1, s32 arg2)
{
    s32 temp_s2;

    temp_s2 = arg2 & 0xFF;
    limbShadowDrawSegment(arg0, 1, 2, 0x258, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 2, 3, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 3, 4, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 4, 5, 0x1F4, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 1, 6, 0x320, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 6, 7, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 7, 8, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 1, 0xC, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 0xC, 0xD, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 0xD, 0xE, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 1, 9, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 9, 0xA, 0x12C, arg1, temp_s2);
    limbShadowDrawSegment(arg0, 0xA, 0xB, 0x12C, arg1, temp_s2);
}

/* Tracks the nearer of the two party members and stores the result in the
   actor's work block.

   `field_54C`..`field_550` snapshot the actor's own root translation. The
   second coordinate of the model (`coord[1]`) is taken into view space and
   each slot's root translation measured against it; the closer of the two
   lands in `field_5E4` with its XZ distance in `field_640`. The chosen
   offset is then normalised and turned into a yaw relative to the actor's
   own heading (`field_556`) in `field_634`. */
static void Actor00400_Fn01454(Task* arg0)
{
    Actor100400Work* work;
    GfxCoord*        coord;
    GfxCoord*        c0;
    GfxCoord*        c1;
    Task*            player;
    GfxCoord*        joint;
    SVECTOR          delta0;
    SVECTOR          delta1;
    SVECTOR          view;
    s32              dist0;
    s32              dist1;

    work            = arg0->work;
    coord           = arg0->extra.tmd->coords;
    player          = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    joint           = &coord[1];
    work->field_54C = coord->coord.t[0];
    work->field_54E = coord->coord.t[1];
    work->field_550 = coord->coord.t[2];
    if (player != NULL) {
        c0      = player->extra.tmd->coords;
        view.vx = 0;
        view.vy = 0;
        view.vz = 0;
        coordLocalToWorld(joint, &view);
        delta0.vx = c0->coord.t[0] - view.vx;
        delta0.vy = c0->coord.t[1] - view.vy;
        delta0.vz = c0->coord.t[2] - view.vz;
        dist0     = SquareRoot0(delta0.vx * delta0.vx + delta0.vz * delta0.vz);
        if (gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] == NULL) {
            work->field_5E4.vx = c0->coord.t[0];
            work->field_5E4.vy = c0->coord.t[1];
            work->field_5E4.vz = c0->coord.t[2];
            work->field_640    = dist0;
        } else {
            c1        = gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION]->extra.tmd->coords;
            delta1.vx = c1->coord.t[0] - view.vx;
            delta1.vy = c1->coord.t[1] - view.vy;
            delta1.vz = c1->coord.t[2] - view.vz;
            dist1     = SquareRoot0(delta1.vx * delta1.vx + delta1.vz * delta1.vz);
            if (dist1 < dist0) {
                work->field_5E4.vx = c1->coord.t[0];
                work->field_5E4.vy = c1->coord.t[1];
                work->field_5E4.vz = c1->coord.t[2];
                delta0             = delta1;
                dist0              = dist1;
            } else {
                work->field_5E4.vx = c0->coord.t[0];
                work->field_5E4.vy = c0->coord.t[1];
                work->field_5E4.vz = c0->coord.t[2];
            }
            work->field_640 = dist0;
        }
        VectorNormalSS(&delta0, &delta0);
        work->field_634 = (ratan2(delta0.vx, delta0.vz) - work->field_556) & 0xFFF;
    }
}

/* Re-aims the two upper body coordinates at the target yaw held in
   `field_546` and folds the result back into the model root.

   With `arg1 == 0` the yaw chases the heading `Actor00400_Fn0814C` reports:
   it steps 0x18 per frame while the error is more than 0x20, and decays
   toward zero once the heading leaves +/-0x5FF. A non-zero `arg1` only
   decays, twice as fast. Each of the two coordinates then gets its pitch
   re-applied about X and a third of the yaw about Y, and the composed
   inverse of all three lands in `c4`. */
static void Actor00400_Fn016A4(Task* arg0, s32 arg1)
{
    SVECTOR           euler;
    SVECTOR           rot1;
    SVECTOR           rot2;
    MATRIX            t1;
    MATRIX            t2;
    MATRIX            t3;
    Actor100400Mat    ma;
    Actor100400Mat    mb;
    Actor100400Mat    mc;
    GfxRotationWords* ia;
    GfxRotationWords* ib;
    GfxRotationWords* ic;
    GfxCoord*         base;
    GfxCoord*         c1;
    GfxCoord*         c2;
    GfxCoord*         c3;
    GfxCoord*         c4;
    Actor100400Work*  work;
    MATRIX*           m2;
    MATRIX*           m3;

    base = arg0->extra.tmd->coords;
    c1   = &base[1];
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];
    work = arg0->work;
    Actor00400_Fn0814C(arg0, 4, &euler, 0x600);
    if ((arg1 & 0xFF) == 0) {
        if ((u16)(euler.vy + 0x5FF) < 0xBFF) {
            if ((u32)((euler.vy - (s16)work->field_546) + 0x20) >= 0x41) {
                if ((s16)work->field_546 < euler.vy) {
                    work->field_546 = work->field_546 + 0x18;
                } else {
                    work->field_546 = work->field_546 - 0x18;
                }
            }
        } else {
            work->field_546 = work->field_546 + ((s32) - (s16)(work->field_546 * 0x10) >> 8);
        }
    } else {
        work->field_546 = work->field_546 + ((s32) - (s16)(work->field_546 * 0x10) >> 7);
    }

    m2 = &c2->coord;
    ia = &ma.matrix.rotationWords;
    ib = &mb.matrix.rotationWords;
    ic = &mc.matrix.rotationWords;

    ma.matrix.rotationWords.m00M01 = ONE;
    ma.matrix.rotationWords.m02M10 = 0;
    ia->m11M12                     = ONE;
    ma.matrix.rotationWords.m20M21 = 0;
    ia->m22                        = ONE;
    mb.matrix.rotationWords.m00M01 = ONE;
    mb.matrix.rotationWords.m02M10 = 0;
    ib->m11M12                     = ONE;
    mb.matrix.rotationWords.m20M21 = 0;
    ib->m22                        = ONE;
    mc.matrix.rotationWords.m00M01 = ONE;
    mc.matrix.rotationWords.m02M10 = 0;
    ic->m11M12                     = ONE;
    mc.matrix.rotationWords.m20M21 = 0;
    ic->m22                        = ONE;

    Gp_MtxToEuler(m2, &rot1);
    m3 = &c3->coord;
    Gp_MtxToEuler(m3, &rot2);
    RotMatrixX(rot1.vx, &ma.matrix.mat);
    RotMatrixX(rot2.vx, &mb.matrix.mat);
    Actor00400_Fn08A1C(&ma.matrix.mat, m2);
    Actor00400_Fn08A1C(&mb.matrix.mat, m3);
    Gp_UpdateCoord(c1);
    Gp_UpdateCoord(c2);
    Gp_UpdateCoord(c3);
    diverTurnJoint(c2, (s16)work->field_546 / 3);
    diverTurnJoint(c3, (s16)work->field_546 / 3);

    mc.matrix.rotationWords.m00M01 = ONE;
    mc.matrix.rotationWords.m02M10 = 0;
    ic->m11M12                     = ONE;
    mc.matrix.rotationWords.m20M21 = 0;
    ic->m22                        = ONE;

    RotMatrixY((s16)work->field_546 / 3, &mc.matrix.mat);
    TransposeMatrix(&c1->coord, &t1);
    TransposeMatrix(m2, &t2);
    TransposeMatrix(m3, &t3);
    MulMatrix(&t1, &t2);
    MulMatrix(&t1, &t3);
    MulMatrix(&t1, &mc.matrix.mat);
    Actor00400_Fn08A1C(&t1, &c4->coord);
}

/* Links the actor's four collision objects and clears their record tables;
   `obj_42C` participates in grid tests when `field_661` is nonzero. */
static void Actor00400_Fn019B4(Task* arg0)
{
    Actor100400Work* work = arg0->work;

    work->obj_35C.coord            = &arg0->extra.tmd->coords[1];
    work->obj_35C.context.contacts = work->field_39C;
    work->obj_35C.pos.vx           = 0;
    work->obj_35C.pos.vy           = 0;
    work->obj_35C.pos.vz           = 0;
    work->obj_35C.key              = 0x30004;
    work->obj_35C.radius           = 0x300;
    work->obj_35C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj_35C);
    Gp_InitRec18Table(work->field_39C, 6, 0);
    work->obj_35C.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->obj_37C.coord            = &arg0->extra.tmd->coords[4];
    work->obj_37C.context.contacts = work->field_39C;
    work->obj_37C.pos.vx           = 0;
    work->obj_37C.pos.vy           = 0;
    work->obj_37C.pos.vz           = 0;
    work->obj_37C.key              = 0x30004;
    work->obj_37C.radius           = 0xC0;
    work->obj_37C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj_37C);
    work->obj_37C.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->obj_4DC.coord            = &arg0->extra.tmd->coords[1];
    work->obj_4DC.context.contacts = work->rec_4FC;
    work->obj_4DC.pos.vx           = 0;
    work->obj_4DC.pos.vy           = 0;
    work->obj_4DC.pos.vz           = 0;
    work->obj_4DC.key              = Gp_PackObjPair(arg0->spawnArg2.pointer, 0);
    work->obj_4DC.radius           = 0x480;
    work->obj_4DC.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj_4DC);
    Gp_InitRec18Table(work->rec_4FC, 3, 0);
    work->obj_4DC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->obj_42C.coord            = arg0->extra.tmd->coords;
    work->obj_42C.context.contacts = work->field_44C;
    work->obj_42C.pos.vx           = 0;
    work->obj_42C.pos.vy           = 0;
    work->obj_42C.pos.vz           = 0;
    work->obj_42C.key              = 0x30004;
    work->obj_42C.radius           = 0x380;
    work->obj_42C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj_42C);
    Gp_InitRec18Table(work->field_44C, 6, 0);
    if (work->field_661 != 0) {
        work->obj_42C.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    } else {
        work->obj_42C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    }
}

/* Damage / knock-back tick: walks the six contact records, applies the hit
   the first one carries, then folds the accumulated push-back into the work
   position and the actor's coordinate. */
static void Actor00400_Fn01B90(Task* arg0)
{
    Actor100400Work*    work;
    Enemy*              obj;
    GfxCoord*           coord;
    WorldCollisionDelta delta;
    s32                 kind;
    s16                 amount;
    s32                 dmg;
    s32                 tmp;
    s32                 tick;
    s32                 i;

    kind            = 0;
    coord           = arg0->extra.tmd->coords;
    work            = arg0->work;
    obj             = arg0->spawnArg2.pointer;
    work->field_642 = 0;
    for (i = 0; i < 6; i++) {
        if ((work->field_39C[i].key.value & 0xFFFF0000) == 0x20000) {
            if (work->field_61C == 0) {
                work->field_642 = 1;
                work->field_65D = 1;
                dmg             = Gp_ComputeDamage(work->field_39C[i].key.value, work->field_640, 0, 0);
                amount          = dmg;
                work->field_61C = Gp_GetIdParam2(work->field_39C[i].key.value);
                if (Gp_RollEnemyChance(obj, work->field_39C[i].key.value, work->field_610) != 0) {
                    amount = ((u32)dmg << 16) >> 14;
                    kind   = 1;
                }
                func_800FDB18(Gp_GetIdParam1(work->field_39C[i].key.value) & 0xFFFF,
                              &arg0->extra.tmd->coords[work->field_664], 0, &work->field_5DC);
                work->field_644 = (amount < 0x3C) ? 5 : 2;
                switch (Gp_GetIdParam0(work->field_39C[i].key.value) & 0xFFFF) {
                    case 0:
                        break;
                    case 1:
                        Gp_SetObjFlag1(obj);
                        break;
                    case 2:
                        Gp_SetObjFlag2(obj, work->field_39C[i].key.value, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4(obj, work->field_39C[i].key.value, 0);
                        break;
                    case 4:
                        work->field_644 = 4;
                        break;
                    case 5:
                        work->field_644 = 2;
                        break;
                    case 6:
                        work->field_644 = 4;
                        break;
                    case 7:
                        kind            = 2;
                        work->field_644 = 2;
                        amount         += amount;
                        break;
                    case 8:
                        work->field_644 = 0;
                        work->field_642 = 0;
                        break;
                    case 9:
                        work->field_644 = 1;
                        break;
                }
                if ((work->field_39C[i].key.value & 0x7F) == 0x1C && (work->field_39C[i].key.value & 0x8000) == 0) {
                    obj->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    work->field_644     = 5;
                }
                tmp = kind;
                switch (tmp) {
                    case 1:
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[work->field_664], 0, 0);
                        break;
                    case 2:
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[work->field_664], 2, 0);
                        break;
                }
                func_800E2C78(obj, work->field_39C[i].key.value, amount, 0);
                func_800DA6E8(&obj->node, amount, 0);
                obj->hp -= amount;
                if ((s16)obj->hp < 0) {
                    obj->hp = 0;
                }
            } else if ((Gp_GetIdParam1(work->field_39C[i].key.value) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &arg0->extra.tmd->coords[1], 0, &work->field_5DC);
            }
        }
        if (work->field_642 != 0) {
            break;
        }
    }

    if (obj->reactionFlags & ENEMY_REACTION_STAGGER) {
        obj->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
        work->field_644     = 2;
    }
    if (obj->reactionFlags & ENEMY_REACTION_BUILDUP) {
        obj->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->field_644     = 3;
    }
    if (obj->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        tmp  = Gp_TickObjFlag4(obj);
        tick = (s16)tmp;
        if (tick != 0) {
            obj->hp -= tmp;
            if ((s16)obj->hp < 0) {
                obj->hp = 0;
            }
            func_800DA6E8(&obj->node, tick, 0);
            if ((s16)obj->hp < 0) {
                obj->hp = 0;
            }
            work->field_642 = 1;
            work->field_644 = 0;
        }
        if (Gp_ObjFlag4Expired(obj) != 0) {
            obj->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }

    switch (func_800E0C10(work->field_44C, &delta, 6, 0)) {
        case 0:
            break;
        case 1:
            tmp              = delta.fixed.vx.halves.integer;
            work->field_564 += tmp;
            tmp              = delta.fixed.vz.halves.integer;
            work->field_568 += tmp;
            if ((delta.fixed.vx.word & 0xFFFF) != 0) {
                if (delta.fixed.vx.word > 0) {
                    work->field_564++;
                } else {
                    work->field_564--;
                }
            }
            if ((delta.fixed.vz.word & 0xFFFF) != 0) {
                if (delta.fixed.vz.word > 0) {
                    work->field_568++;
                } else {
                    work->field_568--;
                }
            }
            tmp                = delta.fixed.vx.halves.integer;
            coord->coord.t[0] += tmp;
            tmp                = delta.fixed.vz.halves.integer;
            coord->coord.t[2] += tmp;
            if ((delta.fixed.vx.word & 0xFFFF) != 0) {
                if (delta.fixed.vx.word > 0) {
                    coord->coord.t[0]++;
                } else {
                    coord->coord.t[0]--;
                }
            }
            if ((delta.fixed.vz.word & 0xFFFF) != 0) {
                if (delta.fixed.vz.word > 0) {
                    coord->coord.t[2]++;
                } else {
                    coord->coord.t[2]--;
                }
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        case 2:
            coord->coord.t[0] = work->field_54C;
            coord->coord.t[2] = work->field_550;
            break;
    }

    Gp_ClearRec18Occupied(work->field_39C);
    Gp_ClearRec18Occupied(work->field_44C);
    if (work->field_61C > 0) {
        work->field_61C--;
    } else {
        work->field_61C = 0;
    }
}

static s16 Actor00400_Fn02154(Task* arg0)
{
    Actor100400Work* work;
    s16              state;
    s16              req;

    work = arg0->work;
    if (work->field_642 != 1) {
        goto fail;
    }
    req = work->field_644;
    if (req == 1) {
        state = 7;
    } else if (req == 2) {
        state = 8;
    } else if (req == 3) {
        state = 9;
    } else if (req == 4) {
        state = 8;
    } else {
        goto other;
    }
    work->field_638 = state;
    work->subState  = 0;
    work->field_644 = 0;
    goto ok;
other:
    if (req == 5) {
        gSceneCombatState.signals.bytes.enemyAlert = 1;
        Gp_ArmStateF0(1);
        work->field_650 = 10;
        work->field_644 = 0;
        return 0;
    }
    work->field_644 = 0;
    goto fail;
ok:
    return 1;
fail:
    return 0;
}

static s32 Actor00400_Fn02208(Task* arg0)
{
    Actor100400Work* work;
    GfxCoord*        coord;
    SVECTOR          vec;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    vec.vx = (u16)work->field_60C[work->field_65B].vx - coord->coord.t[0];
    vec.vy = (u16)work->field_60C[work->field_65B].vy - coord->coord.t[1];
    vec.vz = (u16)work->field_60C[work->field_65B].vz - coord->coord.t[2];
    if (work->animClip != 3) {
        Actor00400_Fn088EC(arg0, 3, 0x10, 0xE);
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    work->field_63E = (u16)work->field_60C[work->field_65B].vy + work->field_64E;
    if ((s16)SquareRoot0(vec.vx * vec.vx + vec.vz * vec.vz) < 400) {
        work->field_65B = (work->field_65B + 1) & 7;
        return 1;
    } else {
        Actor00400_Fn0875C(arg0, &work->field_60C[work->field_65B], 0x2C, 0x100);
        diverStepForward(arg0, 0x60, work->field_556);
        return 0;
    }
}

/* The random pick spawns in both arms rather than after the `if`: jump2
   cross-jumps the identical tails, which is what leaves the 0x20010 argument
   load ahead of the `D_800678F0` store in each arm. */
static void Actor00400_Fn0237C(Task* arg0)
{
    EffectWork* eff1;
    TmdObject*  src1;
    TmdObject*  dst1;
    EffectWork* eff2;
    TmdObject*  src2;
    TmdObject*  dst2;
    EffectWork* eff3;
    TmdObject*  src3;
    TmdObject*  dst3;
    EffectWork* eff4;
    TmdObject*  src4;
    TmdObject*  dst4;
    EffectWork* eff5;
    TmdObject*  src5;
    TmdObject*  dst5;

    D_800678F0[0] = &_gActor00400DiverBurstHead;
    eff1          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[4], 0x200, NULL);
    if (eff1 != NULL) {
        src1                    = arg0->extra.tmd;
        dst1                    = eff1->task->extra.tmd;
        dst1->texturePageOffset = src1->texturePageOffset;
        dst1->clutRowOffset     = src1->clutRowOffset;
        if (dst1->buffer != NULL) {
            tmdProcessStream(dst1);
            tmdProcessStream(dst1);
        }
    }
    D_800678F0[0] = &_gActor00400DiverBurstArmRight;
    eff2          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[11], 0x200, NULL);
    if (eff2 != NULL) {
        src2                    = arg0->extra.tmd;
        dst2                    = eff2->task->extra.tmd;
        dst2->texturePageOffset = src2->texturePageOffset;
        dst2->clutRowOffset     = src2->clutRowOffset;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    D_800678F0[0] = &_gActor00400DiverBurstArmLeft1;
    eff3          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[14], 0x200, NULL);
    if (eff3 != NULL) {
        src3                    = arg0->extra.tmd;
        dst3                    = eff3->task->extra.tmd;
        dst3->texturePageOffset = src3->texturePageOffset;
        dst3->clutRowOffset     = src3->clutRowOffset;
        if (dst3->buffer != NULL) {
            tmdProcessStream(dst3);
            tmdProcessStream(dst3);
        }
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 1) {
        D_800678F0[0] = &_gActor00400DiverBurstLegRight;
        eff4          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[8], 0x200, NULL);
    } else {
        D_800678F0[0] = &_gActor00400DiverBurstArmLeft2;
        eff4          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[8], 0x200, NULL);
    }
    if (eff4 != NULL) {
        src4                    = arg0->extra.tmd;
        dst4                    = eff4->task->extra.tmd;
        dst4->texturePageOffset = src4->texturePageOffset;
        dst4->clutRowOffset     = src4->clutRowOffset;
        if (dst4->buffer != NULL) {
            tmdProcessStream(dst4);
            tmdProcessStream(dst4);
        }
    }
    D_800678F0[0] = &_gActor00400DiverEnergyBall;
    eff5          = Gp_SpawnEff(EFFECT_BODY_CHUNK, &arg0->extra.tmd->coords[1], 0x200, NULL);
    if (eff5 != NULL) {
        src5                    = arg0->extra.tmd;
        dst5                    = eff5->task->extra.tmd;
        dst5->texturePageOffset = src5->texturePageOffset;
        dst5->clutRowOffset     = src5->clutRowOffset;
        if (dst5->buffer != NULL) {
            tmdProcessStream(dst5);
            tmdProcessStream(dst5);
        }
    }
    Gp_SpawnEff(EFFECT_030, &arg0->extra.tmd->coords[1], 0x200, NULL);
}

/// Drives the two head/neck coordinates (`base[2]`, `base[3]`) and the aim
/// coordinate (`base[4]`) from `field_660`, the "actor is aiming" flag.
///
/// While aiming, `field_652` walks a small state machine: state 0 snaps the
/// five part coordinates to their bind pose and records the current Euler
/// angles, state 1 eases those angles back towards zero (and promotes to
/// state 2 once all six components are inside 0x30), and state 2 scales the
/// parts by `field_654` while the aim coordinate keeps its own rotation.
/// When the flag drops, `field_654` is eased back to 0x1000 with the same
/// scaling pass until it passes 0xF80, after which the stored angles are
/// blended halfway towards the live ones and the state resets to 0.
///
/// `invScale` is one function-scope variable rather than a local per arm on
/// purpose: with two assignments the pseudo has two deaths, so local-alloc
/// skips it and never ties the `divmodsi4` result to its dividend. That is
/// what leaves the quotient in the divisor's register (`mflo $v1`).
static void Actor00400_Fn02648(Task* arg0, s32 arg1)
{
    VECTOR           scale;
    Actor100400Mat   rot;
    SVECTOR          euler0;
    Actor100400Mat   ma;
    SVECTOR          euler1;
    Actor100400Mat   mb;
    SVECTOR          euler2;
    Actor100400Mat   mc;
    Actor100400Work* work;
    GfxCoord*        base;
    GfxCoord*        c2;
    GfxCoord*        c3;
    GfxCoord*        c4;
    s32              invScale;

    base = arg0->extra.tmd->coords;
    work = arg0->work;
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];
    if (work->field_660 != 0) {
        switch (work->field_652) {
            case 0:
                base[0].composeStamp = GRAPHICS_COORD_DIRTY;
                base[1].composeStamp = GRAPHICS_COORD_DIRTY;
                base[2].composeStamp = GRAPHICS_COORD_DIRTY;
                base[3].composeStamp = GRAPHICS_COORD_DIRTY;
                base[4].composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(c4);
                Gp_MtxToEuler(&base[2].coord, &work->field_5EC);
                Gp_MtxToEuler(&base[3].coord, &work->field_5F4);
                work->field_652 = 1;
                work->field_654 = 0x1000;
                /* fallthrough */
            case 1: {
                GfxRotationWords* ir;

                ir                              = &rot.matrix.rotationWords;
                work->field_5EC.vx              = (u16)work->field_5EC.vx + ((s32) - (work->field_5EC.vx * 0x10) >> 6);
                work->field_5EC.vy              = (u16)work->field_5EC.vy + ((s32) - (work->field_5EC.vy * 0x10) >> 6);
                work->field_5EC.vz              = (u16)work->field_5EC.vz + ((s32) - (work->field_5EC.vz * 0x10) >> 6);
                work->field_5F4.vx              = (u16)work->field_5F4.vx + ((s32) - (work->field_5F4.vx * 0x10) >> 6);
                work->field_5F4.vy              = (u16)work->field_5F4.vy + ((s32) - (work->field_5F4.vy * 0x10) >> 6);
                work->field_5F4.vz              = (u16)work->field_5F4.vz + ((s32) - (work->field_5F4.vz * 0x10) >> 6);
                rot.matrix.rotationWords.m00M01 = ONE;
                rot.matrix.rotationWords.m02M10 = 0;
                ir->m11M12                      = ONE;
                rot.matrix.rotationWords.m20M21 = 0;
                ir->m22                         = ONE;
                RotMatrix(&work->field_5EC, &rot.matrix.mat);
                Actor00400_Fn08A1C(&rot.matrix.mat, &c2->coord);
                rot.matrix.rotationWords.m00M01 = ONE;
                rot.matrix.rotationWords.m02M10 = 0;
                ir->m11M12                      = ONE;
                rot.matrix.rotationWords.m20M21 = 0;
                ir->m22                         = ONE;
                RotMatrix(&work->field_5F4, &rot.matrix.mat);
                Actor00400_Fn08A1C(&rot.matrix.mat, &c3->coord);
                if ((abs(work->field_5EC.vx) < 0x30) && (abs(work->field_5EC.vy) < 0x30) && (abs(work->field_5EC.vz) < 0x30) &&
                    (abs(work->field_5F4.vx) < 0x30) && (abs(work->field_5F4.vy) < 0x30) && (abs(work->field_5F4.vz) < 0x30)) {
                    work->field_652 = 2;
                }
                c2->composeStamp = GRAPHICS_COORD_DIRTY;
                c3->composeStamp = GRAPHICS_COORD_DIRTY;
                c4->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(c4);
                break;
            }
            case 2: {
                GfxRotationWords* ia;
                GfxRotationWords* ib;
                GfxRotationWords* ic;
                GfxRotationWords* ir;

                Gp_MtxToEuler(&c4->coord, &euler2);
                work->field_654                = (u16)work->field_654 + ((0x2AA - work->field_654) >> 3);
                ia                             = &ma.matrix.rotationWords;
                ma.matrix.rotationWords.m00M01 = ONE;
                ma.matrix.rotationWords.m02M10 = 0;
                ia->m11M12                     = ONE;
                ma.matrix.rotationWords.m20M21 = 0;
                ia->m22                        = ONE;
                scale.vx                       = 0x1000;
                scale.vy                       = 0x1000;
                scale.vz                       = work->field_654;
                ScaleMatrix(&ma.matrix.mat, &scale);
                Actor00400_Fn08A1C(&ma.matrix.mat, &base[2].coord);
                ib                             = &mb.matrix.rotationWords;
                mb.matrix.rotationWords.m00M01 = ONE;
                mb.matrix.rotationWords.m02M10 = 0;
                ib->m11M12                     = ONE;
                mb.matrix.rotationWords.m20M21 = 0;
                ib->m22                        = ONE;
                scale.vx                       = 0x1000;
                scale.vy                       = 0x1000;
                scale.vz                       = 0x1000;
                ScaleMatrix(&mb.matrix.mat, &scale);
                Actor00400_Fn08A1C(&mb.matrix.mat, &base[3].coord);
                ic                             = &mc.matrix.rotationWords;
                mc.matrix.rotationWords.m00M01 = ONE;
                mc.matrix.rotationWords.m02M10 = 0;
                ic->m11M12                     = ONE;
                mc.matrix.rotationWords.m20M21 = 0;
                ic->m22                        = ONE;
                scale.vx                       = 0x1000;
                scale.vy                       = 0x1000;
                invScale                       = 0x1000000 / work->field_654;
                scale.vz                       = invScale;
                ScaleMatrix(&mc.matrix.mat, &scale);
                ir                              = &rot.matrix.rotationWords;
                rot.matrix.rotationWords.m00M01 = ONE;
                rot.matrix.rotationWords.m02M10 = 0;
                ir->m11M12                      = ONE;
                rot.matrix.rotationWords.m20M21 = 0;
                ir->m22                         = ONE;
                RotMatrix(&euler2, &rot.matrix.mat);
                MulMatrix(&mc.matrix.mat, &rot.matrix.mat);
                Actor00400_Fn08A1C(&mc.matrix.mat, &c4->coord);
                base[2].composeStamp = GRAPHICS_COORD_DIRTY;
                base[3].composeStamp = GRAPHICS_COORD_DIRTY;
                base[4].composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(c4);
                break;
            }
        }
    } else {
        base[0].composeStamp = GRAPHICS_COORD_DIRTY;
        base[1].composeStamp = GRAPHICS_COORD_DIRTY;
        base[2].composeStamp = GRAPHICS_COORD_DIRTY;
        base[3].composeStamp = GRAPHICS_COORD_DIRTY;
        base[4].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(c4);
        if (work->field_654 < 0xF80) {
            GfxRotationWords* ia;
            GfxRotationWords* ib;
            GfxRotationWords* ic;
            GfxRotationWords* ir;

            Gp_MtxToEuler(&c4->coord, &euler2);
            work->field_654                = (u16)work->field_654 + ((0x1000 - work->field_654) >> 3);
            ia                             = &ma.matrix.rotationWords;
            ma.matrix.rotationWords.m00M01 = ONE;
            ma.matrix.rotationWords.m02M10 = 0;
            ia->m11M12                     = ONE;
            ma.matrix.rotationWords.m20M21 = 0;
            ia->m22                        = ONE;
            scale.vx                       = 0x1000;
            scale.vy                       = 0x1000;
            scale.vz                       = work->field_654;
            ScaleMatrix(&ma.matrix.mat, &scale);
            Actor00400_Fn08A1C(&ma.matrix.mat, &base[2].coord);
            ib                             = &mb.matrix.rotationWords;
            mb.matrix.rotationWords.m00M01 = ONE;
            mb.matrix.rotationWords.m02M10 = 0;
            ib->m11M12                     = ONE;
            mb.matrix.rotationWords.m20M21 = 0;
            ib->m22                        = ONE;
            scale.vx                       = 0x1000;
            scale.vy                       = 0x1000;
            scale.vz                       = 0x1000;
            ScaleMatrix(&mb.matrix.mat, &scale);
            Actor00400_Fn08A1C(&mb.matrix.mat, &base[3].coord);
            ic                             = &mc.matrix.rotationWords;
            mc.matrix.rotationWords.m00M01 = ONE;
            mc.matrix.rotationWords.m02M10 = 0;
            ic->m11M12                     = ONE;
            mc.matrix.rotationWords.m20M21 = 0;
            ic->m22                        = ONE;
            scale.vx                       = 0x1000;
            scale.vy                       = 0x1000;
            invScale                       = 0x1000000 / work->field_654;
            scale.vz                       = invScale;
            ScaleMatrix(&mc.matrix.mat, &scale);
            ir                              = &rot.matrix.rotationWords;
            rot.matrix.rotationWords.m00M01 = ONE;
            rot.matrix.rotationWords.m02M10 = 0;
            ir->m11M12                      = ONE;
            rot.matrix.rotationWords.m20M21 = 0;
            ir->m22                         = ONE;
            RotMatrix(&euler2, &rot.matrix.mat);
            MulMatrix(&mc.matrix.mat, &rot.matrix.mat);
            Actor00400_Fn08A1C(&mc.matrix.mat, &c4->coord);
        } else {
            GfxRotationWords* ir;
            MATRIX*           m2;
            MATRIX*           m3;

            m2 = &base[2].coord;
            Gp_MtxToEuler(m2, &euler0);
            m3 = &base[3].coord;
            Gp_MtxToEuler(m3, &euler1);
            work->field_5EC.vx              = (u16)work->field_5EC.vx + ((euler0.vx - work->field_5EC.vx) >> 1);
            work->field_5EC.vy              = (u16)work->field_5EC.vy + ((euler0.vy - work->field_5EC.vy) >> 1);
            work->field_5EC.vz              = (u16)work->field_5EC.vz + ((euler0.vz - work->field_5EC.vz) >> 1);
            work->field_5F4.vx              = (u16)work->field_5F4.vx + ((euler1.vx - work->field_5F4.vx) >> 1);
            work->field_5F4.vy              = (u16)work->field_5F4.vy + ((euler1.vy - work->field_5F4.vy) >> 1);
            work->field_5F4.vz              = (u16)work->field_5F4.vz + ((euler1.vz - work->field_5F4.vz) >> 1);
            ir                              = &rot.matrix.rotationWords;
            rot.matrix.rotationWords.m00M01 = ONE;
            rot.matrix.rotationWords.m02M10 = 0;
            ir->m11M12                      = ONE;
            rot.matrix.rotationWords.m20M21 = 0;
            ir->m22                         = ONE;
            RotMatrix(&work->field_5EC, &rot.matrix.mat);
            Actor00400_Fn08A1C(&rot.matrix.mat, m2);
            rot.matrix.rotationWords.m00M01 = ONE;
            rot.matrix.rotationWords.m02M10 = 0;
            ir->m11M12                      = ONE;
            rot.matrix.rotationWords.m20M21 = 0;
            ir->m22                         = ONE;
            RotMatrix(&work->field_5F4, &rot.matrix.mat);
            Actor00400_Fn08A1C(&rot.matrix.mat, m3);
        }
        base[2].composeStamp = GRAPHICS_COORD_DIRTY;
        base[3].composeStamp = GRAPHICS_COORD_DIRTY;
        base[4].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(c4);
        work->field_652 = 0;
    }
}

/// Per-frame callback of the marker task `Actor00400_SpawnMarker` starts: it
/// walks the marker up its stored view-space span, then decides whether the
/// marker should stop being drawn.
///
/// `hidden` is raised when either of the marker's two `WorldCollisionContact` slots reports
/// one of the three kinds 1/3/5, or when `func_800E0C10`'s push-back says the
/// marker is being crowded and the current stage/room is not one of the
/// exceptions. Once it is raised - or after 0x3C frames, or when
/// `gSceneCombatState.actor00400HideRequested` is set - the object's draw flags are cleared, the task's
/// state is bumped and the effect is spawned with kind 2 instead of 1.
///
/// `composeStamp` is cleared through a scalar lvalue on purpose: written as a struct
/// member it is an in-struct MEM, and GCC 2.8.1's
/// `fixed_scalar_and_varying_struct_p` would then let the `gSceneCombatState.actorControl` load
/// hoist above the store. See DECOMPILATION_LEARNINGS.md, "Struct-typing a
/// body changes GCC 2.8.1's aliasing".
static void Actor00400_Fn02D48(Task* arg0)
{
    Actor100400MarkerWork* work;
    s32                    hidden;
    GfxCoord*              coord;
    WorldCollisionDelta    delta;
    s32                    mask;
    s32                    i;
    s32                    n;
    u16                    kind;

    hidden                = 0;
    work                  = (Actor100400MarkerWork*)arg0->work;
    coord                 = arg0->extra.tmd->coords;
    *&coord->composeStamp = GRAPHICS_COORD_DIRTY;
    kind                  = 1;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_60    += 1;
            work->field_5A    += 2;
            coord->coord.t[0] += work->field_58;
            coord->coord.t[1] += work->field_5A;
            coord->coord.t[2] += work->field_5C;
            if (Gp_FindRec18(work->recs, 0) != 0) {
                for (i = 0; i < 2; i++) {
                    switch (work->recs[i].key.value & 0xFFFF0000) {
                        case 0x10000:
                            hidden = 1;
                            break;
                        case 0x30000:
                            hidden = 1;
                            break;
                        case 0x50000:
                            hidden = 1;
                            break;
                    }
                }
            }
            n = func_800E0C10(work->recs, &delta, 2, &mask);
            if (n < 3) {
                if (n > 0) {
                    if (gGameSession->location.loc.stage == GAME_STAGE_MINE_SHELTER &&
                        (gGameSession->location.loc.area == 0x21 || gGameSession->location.loc.area == 0x2B ||
                         gGameSession->location.loc.area == 0x2C || gGameSession->location.loc.area == 0x2D ||
                         gGameSession->location.loc.area == 0x22)) {
                        if ((mask & 2) == 0) {
                            hidden = 1;
                        }
                    } else if (gGameSession->location.loc.stage == GAME_STAGE_SHELTER_NEO_ARK &&
                               (gGameSession->location.loc.area == 0xD || gGameSession->location.loc.area == 0xE ||
                                gGameSession->location.loc.area == 0x1B)) {
                        if ((mask & 2) == 0) {
                            hidden = 1;
                        }
                    } else if (gGameSession->location.loc.area == GAME_AREA_NEO_ARK_SUBMARINE_GALLERY && gGameSession->location.loc.stage == GAME_STAGE_SHELTER_NEO_ARK) {
                        if ((mask & 8) == 0) {
                            hidden = 1;
                        }
                    } else {
                        hidden = 1;
                    }
                }
            }
            Gp_ClearRec18Occupied(work->recs);
            if ((++arg0->killCountdown >= 0x3D) || (gSceneCombatState.actor00400HideRequested != 0) || (hidden != 0)) {
                arg0->killCountdown = 0;
                work->obj.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                kind                = 2;
                arg0->state        += 1;
            }
            diverImpactBurst(coord, work->field_60, kind, 0x1300);
            break;
    }
}

/// Same nearest-waypoint search as `Actor00400_Fn031A4`, but the winner is
/// stored into `field_56C` and then made current: the record `field_64A` used
/// to point at is cleared to kind 0 and the new one is marked kind 1.
static void Actor00400_Fn02FF8(Task* arg0)
{
    Actor100400NearestScratch* scratch;
    Actor100400Work*           work;
    SVECTOR*                   record;
    u8*                        head;
    s16                        index;
    s16                        kind;
    s32                        dx;
    s32                        dz;
    s32                        distance;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x1C;
    scratch                  = SCRATCH_STACK_CURSOR(Actor100400NearestScratch);
    work                     = arg0->work;
    scratch->index           = 1;
    scratch->bestIndex       = 0;
    scratch->best            = 0x7FFFFFFF;
    for (;;) {
        index  = scratch->index;
        record = (SVECTOR*)(index * sizeof(SVECTOR) + (u32)work->field_608);
        kind   = record->pad;
        if (kind == -1) {
            goto done;
        }
        if ((kind != 1) || (index == work->field_64A)) {
            scratch->delta.vx = dx = work->field_5E4.vx - record->vx;
            scratch->delta.vz = dz = work->field_5E4.vz - work->field_608[scratch->index].vz;
            distance               = SquareRoot0((dx * dx) + (dz * dz));
            scratch->dist          = distance;
            if (distance < scratch->best) {
                work->field_56C.vx = work->field_608[scratch->index].vx;
                work->field_56C.vz = work->field_608[scratch->index].vz;
                scratch->best      = scratch->dist;
                scratch->bestIndex = scratch->index;
            }
        }
        scratch->index = scratch->index + 1;
    }
done:
    if (work->field_64A != scratch->bestIndex) {
        work->field_608[work->field_64A].pad = 0;
        work->field_64A                      = scratch->bestIndex;
        work->field_608[work->field_64A].pad = 1;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

/// Finds the nearest eligible waypoint record in `field_608` and returns its
/// XZ in `arg1`. Records with `field_6 == 1` are only considered when they are
/// the one `field_64A` points at, and the walk ends at the `-1` terminator.
static void Actor00400_Fn031A4(Task* arg0, SVECTOR* arg1)
{
    Actor100400NearestScratch* scratch;
    Actor100400Work*           work;
    SVECTOR*                   record;
    u8*                        head;
    s16                        index;
    s16                        kind;
    s32                        dx;
    s32                        dz;
    s32                        distance;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x1C;
    scratch                  = SCRATCH_STACK_CURSOR(Actor100400NearestScratch);
    work                     = arg0->work;
    scratch->index           = 1;
    scratch->bestIndex       = 0;
    scratch->best            = 0x7FFFFFFF;
loop:
    index  = scratch->index;
    record = (SVECTOR*)(index * sizeof(SVECTOR) + (u32)work->field_608);
    kind   = record->pad;
    if (kind != -1) {
        if ((kind != 1) || (index == work->field_64A)) {
            scratch->delta.vx = dx = work->field_5E4.vx - record->vx;
            scratch->delta.vz = dz = work->field_5E4.vz - work->field_608[scratch->index].vz;
            distance               = SquareRoot0((dx * dx) + (dz * dz));
            scratch->dist          = distance;
            if (distance < scratch->best) {
                arg1->vx           = work->field_608[scratch->index].vx;
                arg1->vz           = work->field_608[scratch->index].vz;
                scratch->best      = scratch->dist;
                scratch->bestIndex = scratch->index;
            }
        }
        scratch->index = scratch->index + 1;
        goto loop;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

/// Projects the four `corner` vertices through the view matrix and queues one
/// semi-transparent textured quad shaded grey `shade` (half intensity on red).
static void Actor00400_Fn03318(SVECTOR* corner0, SVECTOR* corner1, SVECTOR* corner2, SVECTOR* corner3, u8 shade)
{
    Actor100400TextQuadScratch* s;
    POLY_FT4*                   poly;

    s                          = (Actor100400TextQuadScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor100400TextQuadScratch));
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    s->depth = RotTransPers4(corner0, corner1, corner2, corner3, &s->screen0, &s->screen1, &s->screen2, &s->screen3,
                             &s->perspective, &s->flags);
    if (s->flags >= 0) {
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 9);
        poly->code                     = 0x2E;
        GPU_PRIMITIVE_XY_WORD(poly, 0) = s->screen0;
        GPU_PRIMITIVE_XY_WORD(poly, 1) = s->screen1;
        GPU_PRIMITIVE_XY_WORD(poly, 2) = s->screen2;
        GPU_PRIMITIVE_XY_WORD(poly, 3) = s->screen3;
        setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
        poly->tpage = 0x48;
        poly->clut  = 0x4283;
        setRGB0(poly, shade >> 1, shade, shade);
        addPrim((&gGpuCurrentOt[((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor100400TextQuadScratch));
}

#include "../../shared/diver_turn_joint.inc.c"

/* The state tables below are defined among the functions, not with the other
   declarations, because `.rodata` follows source order: each sits between the
   jump tables of the functions around it. */

/// Kill-path states, indexed by `Task::state` in `Actor00400_Fn08004`.
static const TaskFuncTable3 Actor00400_D0002C = { {
    Actor00400_Fn0A190,
    Actor00400_Fn02D48,
    diverStrikeTeardown,
} };

/// The eight states `Actor00400_Fn08948` dispatches on `field_30`. The zero
/// word after it in the image is the alignment pad of
/// `Actor00400_Fn03920`'s jump table, not a terminator.
static const TaskFuncTable8 Actor00400_D00038 = { {
    Actor00400_Fn03920,
    Actor00400_Fn04580,
    Actor00400_Fn04B48,
    Actor00400_Fn04E18,
    Actor00400_Fn040DC,
    Actor00400_Fn089C8,
    Actor00400_Fn06B7C,
    Actor00400_Fn070C0,
} };

/// Applies the row of `Actor00400_D15F20` that matches the session's current
/// area and room to the freshly allocated work block: the row can hide the root
/// coordinate, override the waypoint set the spawn argument selects, hand the
/// actor its record list and seed its height. Returns non-zero when the actor
/// does not belong in this room - no row matched, or the matching row's `flags`
/// bit 1 rejects it - which makes the entry state destroy the enemy instead.
static __inline__ s32 Actor00400_ApplyAreaConfig(Task* arg0)
{
    Actor100400AreaConfig* cfg;
    Actor100400Work*       work;
    GameLocationKey*       ses;
    u16                    flags;

    work = arg0->work;
    ses  = &gGameSession->location.loc;
    cfg  = Actor00400_D15F20;
    while (cfg->area != 0xFF) {
        if ((ses->stage == cfg->area) && (ses->area == cfg->room)) {
            flags = cfg->flags;
            if (flags & 1) {
                return 1;
            }
            if (flags & 2) {
                work->field_661 = 1;
            }
            if (cfg->waypointSets != NULL) {
                work->field_60C = cfg->waypointSets[(arg0->spawnArg1.value >> 4) & 3];
            }
            if (cfg->records != NULL) {
                work->field_608 = cfg->records;
            }
            if (cfg->height != NULL) {
                work->field_64E = (u16)*cfg->height;
            }
            return 0;
        }
        cfg++;
    }
    return 1;
}

/// Points the object at the second part coordinate of the model and clears its
/// pending-hit byte, then sets whether the root coordinate stays hidden. Shared
/// by the two spawn states that attach the actor to its head.
///
/// `hide` is `s32` rather than `u8` so its literal lands in a different CSE mode
/// class from the `1` the callers store into `field_664`, which is what makes
/// state 6 materialise the constant twice the way retail does.
static __inline__ void Actor00400_AttachHead(Task* arg0, Enemy* obj,
                                             Actor100400Work* work, s32 hide)
{
    obj->coord                  = &arg0->extra.tmd->coords[1];
    obj->node.state.parts.flags = 0;
    work->field_661             = hide;
}

static void Actor00400_Fn03920(Task* arg0)
{
    Actor100400Work*     work;
    Actor100400Work*     w;
    Actor100400Work*     anim;
    Enemy*               obj;
    Actor100400QuadWork* quad;
    Enemy*               quadOwner;
    GfxCoord*            pos;
    GfxCoord*            coord;
    Task*                task;
    s32                  failed;
    s32                  nibble;
    s32                  index;
    u16                  y;
    s32*                 spawnArg;

    spawnArg              = &arg0->spawnArg1.value;
    gStageSceneMusicEntry = 0xB;
    obj                   = arg0->spawnArg2.pointer;
    coord                 = arg0->extra.tmd->coords;
    if ((*spawnArg >> 16) & 1) {
        enemyDestroy(obj, arg0);
        return;
    }
    arg0->work = memCalloc(sizeof(Actor100400Work), 0);
    work       = arg0->work;
    if (work == NULL) {
        enemyDestroy(obj, arg0);
        return;
    }

    failed = Actor00400_ApplyAreaConfig(arg0);
    if (failed) {
        enemyDestroy(obj, arg0);
        return;
    }

    if ((arg0->spawnArg1.value & 0xF) == 0) {
        work->field_661 = 1;
    }
    Actor00400_Fn00B48(arg0);
    arg0->msgTable = Actor00400_D16010;
    switch (arg0->spawnArg1.value & 0xF) {
        case 7:
            work->field_658 = 0xC8;
            work->field_666 = 1;
            work->field_664 = 1;
            work->field_63E = (u16)work->field_64E;
            Actor00400_AttachHead(arg0, obj, work, 0);
            w              = arg0->work;
            w->animStep    = 0x10;
            w->animClip    = 0x10;
            w->animRequest = 2;
            w              = arg0->work;
            arg0->state    = 7;
            w->field_638   = 0;
            w->subState    = 0;
            Gp_IncStateF0Ref(0);
            obj->hp = (s16)obj->hpMax / 8;
            break;
        case 6:
            work->field_666 = 0;
            work->field_664 = 1;
            Actor00400_AttachHead(arg0, obj, work, 1);
            w              = arg0->work;
            w->animStep    = 0x10;
            w->animClip    = 0xF;
            w->animRequest = 2;
            w              = arg0->work;
            arg0->state    = 6;
            w->field_638   = 0;
            w->subState    = 0;
            Gp_IncStateF0Ref(0);
            obj->hp   = (s16)obj->hpMax / 8;
            pos       = arg0->extra.tmd->coords;
            quadOwner = arg0->spawnArg2.pointer;
            y         = (u16)pos->coord.t[1];
            task      = Task_SpawnFromTable(Actor00400_D16028, 2, 0, 0);
            if (task != NULL) {
                quad = memCalloc(sizeof(Actor100400QuadWork), 0);
                if (quad == NULL) {
                    taskKill(task);
                } else {
                    task->work           = quad;
                    quad->vertices[0].vx = (u16)pos->coord.t[0] - 0x5DC;
                    quad->vertices[0].vy = y;
                    quad->vertices[0].vz = (u16)pos->coord.t[2] - 0x5DC;
                    quad->vertices[1].vx = (u16)pos->coord.t[0] + 0x5DC;
                    quad->vertices[1].vy = y;
                    quad->vertices[1].vz = (u16)pos->coord.t[2] - 0x5DC;
                    quad->vertices[2].vx = (u16)pos->coord.t[0] - 0x5DC;
                    quad->vertices[2].vy = y;
                    quad->vertices[2].vz = (u16)pos->coord.t[2] + 0x5DC;
                    quad->vertices[3].vx = (u16)pos->coord.t[0] + 0x5DC;
                    quad->vertices[3].vy = y;
                    quad->vertices[3].vz = (u16)pos->coord.t[2] + 0x5DC;
                    quad->intensity      = 0xFF;
                    quad->field_0        = quadOwner;
                }
            }
            break;
        case 0:
            work->field_666 = 0;
            work->field_661 = 1;
            w               = arg0->work;
            w->animStep     = 0x10;
            w->animClip     = 2;
            w->animRequest  = 2;
            arg0->state     = arg0->state + 1;
            break;
        case 4:
            work->field_666 = 1;
            nibble          = GameFlag_GetNibble(GAME_FLAG_0EB);
            if (nibble != 2) {
                obj->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                w                           = arg0->work;
                w->animStep                 = 0x10;
                w->animClip                 = 1;
                w->animRequest              = 2;
                w                           = arg0->work;
                arg0->state                 = 3;
                w->field_638                = 0;
                w->subState                 = 0;
                w                           = arg0->work;
                w->field_638                = 0xD;
                w->subState                 = 0;
            } else {
                w              = arg0->work;
                w->animStep    = 0x10;
                w->animClip    = 1;
                w->animRequest = nibble;
                w              = arg0->work;
                arg0->state    = 3;
                w->field_638   = 0;
                w->subState    = 0;
            }
            break;
        case 5:
            work->field_666 = 1;
            nibble          = GameFlag_GetNibble(GAME_FLAG_0EB);
            if (nibble != 2) {
                obj->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                work->field_666             = 1;
                w                           = arg0->work;
                w->animStep                 = 0x10;
                w->animClip                 = 1;
                w->animRequest              = 2;
                w                           = arg0->work;
                arg0->state                 = 3;
                w->field_638                = 0;
                w->subState                 = 0;
                w                           = arg0->work;
                w->field_638                = 0xE;
                w->subState                 = 0;
            } else {
                w              = arg0->work;
                w->animStep    = 0x10;
                w->animClip    = 1;
                w->animRequest = nibble;
                w              = arg0->work;
                arg0->state    = 3;
                w->field_638   = 0;
                w->subState    = 0;
            }
            break;
        case 1:
            if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 45, 0, 0)) {
                if (GameFlag_GetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) == 0) {
                    work->field_666 = 1;
                    w               = arg0->work;
                    w->animStep     = 0x10;
                    w->animClip     = 1;
                    w->animRequest  = 2;
                    w               = arg0->work;
                    arg0->state     = 3;
                    w->field_638    = 0;
                    w->subState     = 0;
                } else {
                    work->field_661   = 1;
                    w                 = arg0->work;
                    w->animStep       = 0x10;
                    w->animClip       = 2;
                    w->animRequest    = 2;
                    work->field_666   = 0;
                    coord->coord.t[1] = 0;
                    w                 = arg0->work;
                    arg0->state       = 1;
                    w->field_638      = 0;
                    w->subState       = 0;
                }
            } else {
                work->field_666 = 1;
                w               = arg0->work;
                w->animStep     = 0x10;
                w->animClip     = 1;
                w->animRequest  = 2;
                w               = arg0->work;
                arg0->state     = 3;
                w->field_638    = 0;
                w->subState     = 0;
            }
            break;
        case 3:
            work->field_666 = 1;
            w               = arg0->work;
            w->animStep     = 0x10;
            w->animClip     = 3;
            w->animRequest  = 2;
            Actor00400_Fn02FF8(arg0);
            coord->coord.t[0] = work->field_56C.vx;
            work->field_63E   = (u16)work->field_64E;
            coord->coord.t[1] = work->field_56C.vy + work->field_64E + 0x7D0;
            coord->coord.t[2] = work->field_56C.vz;
            Gp_IncStateF0Ref(0);
            w            = arg0->work;
            arg0->state  = 3;
            w->field_638 = 0;
            w->subState  = 0;
            w            = arg0->work;
            w->field_638 = 4;
            w->subState  = 0;
            break;
        case 2:
            work->field_666 = 1;
            work->field_65F = 1;
            w               = arg0->work;
            w->animStep     = 0x10;
            w->animClip     = 3;
            w->animRequest  = 2;
            w               = arg0->work;
            arg0->state     = 3;
            w->field_638    = 0;
            w->subState     = 0;
            if ((arg0->spawnArg1.value & 0xF0) == 0) {
                w            = arg0->work;
                w->field_638 = 0xC;
                w->subState  = 0;
            } else {
                w            = arg0->work;
                w->field_638 = 0xB;
                w->subState  = 0;
            }
            break;
    }

    anim = arg0->work;
    if (anim->animRequest == 1) {
        if (anim->animPlaying != anim->animClip) {
            anim->field_62A = 0;
        } else {
            anim->field_62A = Actor00400_Fn086FC(arg0, anim->field_62A);
        }
        Actor00400_Fn08624(arg0);
        anim->animRequest = 3;
    } else if (anim->animRequest == 2) {
        diverRestartClip(arg0);
        anim->animRequest = 3;
        anim->field_62A   = 0;
    } else if (anim->animRequest == 3) {
        anim->field_62A = (u16)anim->field_62A + 1;
    }
    for (index = 1; index < 0xF; index++) {
        animationTickSlot(&anim->anim, index);
    }
    work->field_620 = 0x1000;
    work->field_622 = 0x1000;
}

/// Colours the actor from the second attach coordinate of its model through a
/// 0x10-byte `VECTOR` taken off the scratch stack, then hides the root
/// coordinate while `field_65F` is set.
static __inline__ void Actor00400_UpdateColor(Task* arg0, GfxCoord* coord,
                                              Actor100400Work* work, TmdObject* ctx)
{
    VECTOR* block = (VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10);

    block->vx                    = coord->workm.t[0];
    block->vy                    = coord->workm.t[1];
    block->vz                    = coord->workm.t[2];
    SCRATCH_STACK_CURSOR(VECTOR) = block;
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, block, 0, 0);
    if (work->field_65F != 0) {
        Gp_SetObjTrans(ctx, 0, 0, 0);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// States `Actor00400_Fn040DC` dispatches on `Actor100400Work.field_638`.
static const TaskFuncTable11 Actor00400_D0007C = { {
    Actor00400_Fn042C0,
    Actor00400_Fn04414,
    Actor00400_Fn07CC4,
    Actor00400_Fn07DE0,
    Actor00400_Fn07E20,
    Actor00400_Fn07E74,
    Actor00400_Fn07EE8,
    Actor00400_Fn07F18,
    Actor00400_Fn07F44,
    Actor00400_Fn07F88,
    Actor00400_Fn07FEC,
} };

/// Per-frame callback for the text actor's second task. Same frame gate as
/// `Actor00400_Fn04B48`: `gSceneCombatState.actorControl` 2 only flags the model hidden, 0 runs
/// this frame's state handler before falling through to the draw half, and 1
/// is the draw half on its own.
static void Actor00400_Fn040DC(Task* arg0)
{
    TaskFuncTable11  fns;
    Actor100400Work* work;
    TmdObject*       ctx;
    TmdObject*       ctx2;
    Actor100400Work* work2;
    GfxCoord*        coord;
    s32              y;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ctx   = arg0->extra.tmd;
    fns   = Actor00400_D0007C;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->field_663 != 0) {
                break;
            }
            fns.funcs[work->field_638](arg0);
            work->flags_62C.half = work->slots[1].flags;
            if (work->field_644 != 4) {
                work->field_660 = 1;
                Actor00400_Fn02648(arg0, 1);
            }
            y                 = coord->coord.t[1];
            coord->coord.t[1] = y + ((work->field_63E + (s16)work->field_658 - y) >> 4);
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            break;
    }
}

static void Actor00400_Fn042C0(Task* arg0)
{
    Actor100400Work* work;
    Enemy*           obj;
    s32              id;

    work = arg0->work;
    obj  = arg0->spawnArg2.pointer;
    if (arg0->extra.tmd->coords->coord.t[1] - work->field_64E < 0x320) {
        work->field_63E = work->field_64E;
    }
    worldTargetUnlinkNode(&obj->node);
    Gp_ReleaseStateF0Add(arg0, 0);
    obj->recs = NULL;
    Gp_UnlinkObj(&work->obj_35C);
    Gp_UnlinkObj(&work->obj_37C);
    Gp_UnlinkObj(&work->obj_4DC);
    Gp_UnlinkObj(&work->obj_42C);
    work->field_636 = 0;
    if (work->field_644 == 4) {
        Actor100400Work* w = arg0->work;
        w->field_638       = 7;
        w->subState        = 0;
        return;
    }
    if (arg0->spawnArg1.value != 7) {
        Actor100400Work* w;
        id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        w              = arg0->work;
        w->animStep    = 0x30;
        w->animBlend   = 4;
        w->animClip    = 1;
        w->animRequest = 1;
    }
    work->field_638++;
}

static void Actor00400_Fn04414(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;
    Actor100400Work* w2;
    s32              i;
    s32              cond;

    work = arg0->work;
    w    = arg0->work;
    if (w->animRequest == 1) {
        if (w->animPlaying != w->animClip) {
            w->field_62A = 0;
        } else {
            w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
        }
        Actor00400_Fn08624(arg0);
        w->animRequest = 3;
    } else if (w->animRequest == 2) {
        diverRestartClip(arg0);
        w->animRequest = 3;
        w->field_62A   = 0;
    } else if (w->animRequest == 3) {
        w->field_62A++;
    }
    i = 1;
    do {
        animationTickSlot(&w->anim, i);
        i++;
    } while (i < 0xF);
    if (arg0->spawnArg1.value != 7) {
        w2 = arg0->work;
        if ((w2->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (w2->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond == 0) {
            return;
        }
        w2              = arg0->work;
        w2->animBlend   = 4;
        w2->animStep    = 0x10;
        w2->animClip    = 0xE;
        w2->animRequest = 1;
    }
    work->field_638++;
}

/// States `Actor00400_Fn04580` dispatches on `Actor100400Work.field_638`.
static const TaskFuncTable10 Actor00400_D000A8 = { {
    Actor00400_Fn090B4,
    Actor00400_Fn09124,
    Actor00400_Fn091F8,
    Actor00400_Fn09260,
    Actor00400_Fn092D4,
    Actor00400_Fn09348,
    Actor00400_Fn093BC,
    Actor00400_Fn093C4,
    Actor00400_Fn09418,
    Actor00400_Fn0946C,
} };

static void Actor00400_Fn04580(Task* arg0)
{
    Actor100400Work*  work = arg0->work;
    Enemy*            obj  = arg0->spawnArg2.pointer;
    TmdObject*        ctx  = arg0->extra.tmd;
    TaskFuncTable10   fns;
    Actor100400Mat    m;
    GfxRotationWords* ia;
    Actor100400Work*  w;
    Actor100400Work*  w2;
    Actor100400Work*  w3;
    Actor100400Work*  work2;
    TmdObject*        ctx2;
    GfxCoord*         coord;
    MATRIX*           dst;
    s32               i;

    fns = Actor00400_D000A8;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->field_663 != 0) {
                break;
            }
            work->flags_62C.hi.field_62E++;
            work->field_630++;
            Actor00400_Fn01454(arg0);
            fns.funcs[work->field_638](arg0);
            Actor00400_Fn00A14(arg0);
            w = arg0->work;
            if (w->animRequest == 1) {
                if (w->animPlaying != w->animClip) {
                    w->field_62A = 0;
                } else {
                    w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
                }
                Actor00400_Fn08624(arg0);
                w->animRequest = 3;
            } else if (w->animRequest == 2) {
                diverRestartClip(arg0);
                w->animRequest = 3;
                w->field_62A   = 0;
            } else if (w->animRequest == 3) {
                w->field_62A++;
            }
            i = 1;
            do {
                animationTickSlot(&w->anim, i);
                i++;
            } while (i < 0xF);
            work->flags_62C.half = work->slots[1].flags;
            Actor00400_Fn016A4(arg0, (u8)work->field_665);
            w2                            = arg0->work;
            coord                         = arg0->extra.tmd->coords;
            ia                            = &m.matrix.rotationWords;
            m.matrix.rotationWords.m00M01 = ONE;
            m.matrix.rotationWords.m02M10 = 0;
            ia->m11M12                    = ONE;
            m.matrix.rotationWords.m20M21 = 0;
            ia->m22                       = ONE;
            RotMatrixZ(w2->field_558, &m.matrix.mat);
            RotMatrixY(w2->field_556, &m.matrix.mat);
            dst                 = &coord->coord;
            dst->m[0][0]        = m.matrix.mat.m[0][0];
            dst->m[0][1]        = m.matrix.mat.m[0][1];
            dst->m[0][2]        = m.matrix.mat.m[0][2];
            dst->m[1][0]        = m.matrix.mat.m[1][0];
            dst->m[1][1]        = m.matrix.mat.m[1][1];
            dst->m[1][2]        = m.matrix.mat.m[1][2];
            dst->m[2][0]        = m.matrix.mat.m[2][0];
            dst->m[2][1]        = m.matrix.mat.m[2][1];
            dst->m[2][2]        = m.matrix.mat.m[2][2];
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->hp <= 0) {
                w3            = arg0->work;
                arg0->state   = 2;
                w3->field_638 = 0;
                w3->subState  = 0;
            }
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            Actor00400_Fn012B0(arg0, arg0->extra.tmd->coords->coord.t[1], 0x80);
            ctx->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

static void Actor00400_Fn04900(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              cond;

    work = arg0->work;
    if (work->field_642 != 0 && work->field_644 == 1) {
        work->animStep    = 0x10;
        work->animClip    = 0xC;
        work->animRequest = 2;
        id                = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        return;
    }
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        work2 = arg0->work;
        if ((work2->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (work2->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->work;
            work2->field_638 = 2;
            work2->subState  = 0;
        }
    }
}

static void Actor00400_Fn04A1C(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              cond;

    work = arg0->work;
    if (work->field_642 != 0 && work->field_644 == 2) {
        id = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work2              = arg0->work;
        work2->animBlend   = 6;
        work2->animStep    = 0x10;
        work2->animClip    = 0xD;
        work2->animRequest = 1;
        return;
    }
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        work2 = arg0->work;
        if ((work2->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (work2->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->work;
            work2->field_638 = 2;
            work2->subState  = 0;
        }
    }
}

/// States `Actor00400_Fn04B48` dispatches on `Actor100400Work.field_638`.
static const TaskFuncTable10 Actor00400_D000D0 = { {
    Actor00400_Fn04CF8,
    Actor00400_Fn08C54,
    Actor00400_Fn08D70,
    Actor00400_Fn08DFC,
    Actor00400_Fn08E50,
    Actor00400_Fn08FB0,
    Actor00400_Fn08FC8,
    Actor00400_Fn08FF4,
    Actor00400_Fn09038,
    Actor00400_Fn0909C,
} };

/// Per-frame callback for the main actor task. `gSceneCombatState.actorControl` gates the frame:
/// 2 only flags the model hidden, 0 runs this frame's state handler before
/// falling through to the draw half, and 1 is the draw half on its own.
static void Actor00400_Fn04B48(Task* arg0)
{
    TaskFuncTable10  fns;
    Actor100400Work* work;
    TmdObject*       ctx;
    TmdObject*       ctx2;
    Actor100400Work* work2;
    GfxCoord*        coord;

    work = arg0->work;
    ctx  = arg0->extra.tmd;
    fns  = Actor00400_D000D0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->field_663 != 0) {
                break;
            }
            fns.funcs[work->field_638](arg0);
            work->flags_62C.half = work->slots[1].flags;
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            coord = &ctx2->coords[1];
            Actor00400_UpdateColor(arg0, coord, work2, ctx2);
            Actor00400_Fn012B0(arg0, arg0->extra.tmd->coords->coord.t[1], (u8)work->field_648);
            break;
    }
}

static void Actor00400_Fn04CF8(Task* arg0)
{
    Actor100400Work* work;
    Enemy*           obj;
    s32              id;
    Actor100400Work* w;

    work      = arg0->work;
    obj       = arg0->spawnArg2.pointer;
    obj->recs = NULL;
    Gp_UnlinkObj(&work->obj_42C);
    Gp_UnlinkObj(&work->obj_35C);
    Gp_UnlinkObj(&work->obj_37C);
    Gp_UnlinkObj(&work->obj_4DC);
    worldTargetUnlinkNode(&obj->node);
    Gp_ReleaseStateF0Add(arg0, 0);
    work->field_648 = 0x80;
    if (work->field_644 == 4) {
        w            = arg0->work;
        w->field_638 = 6;
        w->subState  = 0;
        return;
    }
    w               = arg0->work;
    w->animBlend    = 8;
    w->animStep     = 0x10;
    w->animClip     = 0xF;
    w->animRequest  = 1;
    work->field_636 = 0;
    id              = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
    SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                        (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->field_638++;
}

/// States `Actor00400_Fn04E18` dispatches on `Actor100400Work.field_638`.
static const TaskFuncTable15 Actor00400_D000F8 = { {
    Actor00400_Fn07738,
    Actor00400_Fn077F4,
    Actor00400_Fn05728,
    Actor00400_Fn078C8,
    Actor00400_Fn0793C,
    Actor00400_Fn07998,
    Actor00400_Fn079A0,
    Actor00400_Fn079A8,
    Actor00400_Fn079FC,
    Actor00400_Fn07ABC,
    Actor00400_Fn07B10,
    Actor00400_Fn07B98,
    Actor00400_Fn07C04,
    Actor00400_Fn09C04,
    Actor00400_Fn09C84,
} };

/// Per-frame callback for the boss task. Same `gSceneCombatState.actorControl` frame gate as
/// `Actor00400_Fn04580`, with the model's Y bobbed by two `rsin` terms and the
/// display object re-pointed at the part coordinate `field_664` selects; the
/// tail hides the model again while the session sits in the two area-0xA/0xB
/// rooms of area 0x21.
static void Actor00400_Fn04E18(Task* arg0)
{
    Actor100400Work*  work   = arg0->work;
    GfxCoord*         coord0 = arg0->extra.tmd->coords;
    Enemy*            obj    = arg0->spawnArg2.pointer;
    TmdObject*        ctx    = arg0->extra.tmd;
    TaskFuncTable15   fns;
    Actor100400Mat    m;
    GfxRotationWords* ia;
    Actor100400Work*  w;
    Actor100400Work*  wA;
    Actor100400Work*  w2;
    Actor100400Work*  w3;
    Actor100400Work*  w4;
    Actor100400Work*  work2;
    Enemy*            obj2;
    TmdObject*        ctx2;
    TmdObject*        ctx3;
    TmdObject*        ctxN;
    GameLocationKey*  sess;
    GfxCoord*         coord;
    GfxCoord*         coordN;
    MATRIX*           dst;
    s32               i;

    fns = Actor00400_D000F8;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->field_663 != 0) {
                return;
            }
            work->flags_62C.hi.field_62E++;
            work->field_630++;
            Actor00400_Fn01454(arg0);
            fns.funcs[work->field_638](arg0);
            Actor00400_Fn00A14(arg0);
            wA = arg0->work;
            if (wA->field_64C != 0) {
                wA->field_64C--;
            }
            w = arg0->work;
            if (w->animRequest == 1) {
                if (w->animPlaying != w->animClip) {
                    w->field_62A = 0;
                } else {
                    w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
                }
                Actor00400_Fn08624(arg0);
                w->animRequest = 3;
            } else if (w->animRequest == 2) {
                diverRestartClip(arg0);
                w->animRequest = 3;
                w->field_62A   = 0;
            } else if (w->animRequest == 3) {
                w->field_62A++;
            }
            i = 1;
            do {
                animationTickSlot(&w->anim, i);
                i++;
            } while (i < 0xF);
            work->flags_62C.half = work->slots[1].flags;
            Actor00400_Fn02648(arg0, work->field_660);
            w2                            = arg0->work;
            coord                         = arg0->extra.tmd->coords;
            ia                            = &m.matrix.rotationWords;
            m.matrix.rotationWords.m00M01 = ONE;
            m.matrix.rotationWords.m02M10 = 0;
            ia->m11M12                    = ONE;
            m.matrix.rotationWords.m20M21 = 0;
            ia->m22                       = ONE;
            RotMatrixZ(w2->field_558, &m.matrix.mat);
            RotMatrixY(w2->field_556, &m.matrix.mat);
            dst                 = &coord->coord;
            dst->m[0][0]        = m.matrix.mat.m[0][0];
            dst->m[0][1]        = m.matrix.mat.m[0][1];
            dst->m[0][2]        = m.matrix.mat.m[0][2];
            dst->m[1][0]        = m.matrix.mat.m[1][0];
            dst->m[1][1]        = m.matrix.mat.m[1][1];
            dst->m[1][2]        = m.matrix.mat.m[1][2];
            dst->m[2][0]        = m.matrix.mat.m[2][0];
            dst->m[2][1]        = m.matrix.mat.m[2][1];
            dst->m[2][2]        = m.matrix.mat.m[2][2];
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->hp <= 0) {
                w3            = arg0->work;
                arg0->state   = 4;
                w3->field_638 = 0;
                w3->subState  = 0;
            }
            coord0->coord.t[1] += (work->field_63E - coord0->coord.t[1]) >> 4;
            if (work->field_638 < 0xB) {
                coord0->coord.t[1] += (rsin(work->flags_62C.hi.field_62E << 6) * 0x10) >> 12;
            }
            if (work->field_650 != 0) {
                work->field_650--;
                coord0->coord.t[1] += (rsin(work->field_630 << 0xA) * 0x10) >> 0xA;
            }
            obj->coord = &arg0->extra.tmd->coords[work->field_664];
            if (work->field_638 < 0xB) {
                w4       = arg0->work;
                ctxN     = arg0->extra.tmd;
                obj2     = arg0->spawnArg2.pointer;
                coordN   = &ctxN->coords[w4->field_664];
                m.vec.vx = 0;
                m.vec.vy = 0;
                m.vec.vz = 0;
                coordLocalToWorld(coordN, &m.vec);
                if (w4->field_64E + 0x190 < m.vec.vy) {
                    obj2->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                } else {
                    obj2->node.state.parts.flags = 0;
                }
            }
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            ctx->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    sess = &gGameSession->location.loc;
    ctx3 = arg0->extra.tmd;
    if (sess->stage == GAME_STAGE_MINE_SHELTER && sess->area == 0x21 && (u32)(gGameSession->location.loc.view - 0xA) < 2U) {
        ctx3->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

static inline void Actor00400_TurnToward(Task* arg0, SVECTOR* target, s32 step, s32 range)
{
    Actor100400Work* work = arg0->work;
    GfxCoord*        coords;
    SVECTOR          vec;
    s32              diff;
    s32              yaw;
    u16              angle;

    coords               = arg0->extra.tmd->coords;
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
    vec.vx               = target->vx - coords->coord.t[0];
    vec.vy               = 0;
    vec.vz               = target->vz - coords->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    angle = work->field_556;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > range) {
        work->field_556 = angle - step;
    } else if (diff < -range) {
        work->field_556 = angle + step;
    }
}

/// Spawns the 16-way ring of `0x01202148` effects the boss uses when it lands
/// and when it is knocked down: one per 1/16 turn, at the height `field_64E`
/// gives above the root coordinate.
static inline void Actor00400_SpawnRing(Task* arg0, Actor100400Work* work, GfxCoord* coord)
{
    GfxCoord* coord2;
    SVECTOR   vec;
    s32       i;
    s16       y;

    i      = 0;
    y      = work->field_64E - coord->coord.t[1] + 0xFA;
    coord2 = arg0->extra.tmd->coords;
    do {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = y;
        vec.vz = (u32)rcos(i << 8) >> 3;
        Gp_SpawnEff(gRoomEffectWaterSprayId, coord2, 0x01202148, &vec);
        i++;
    } while (i < 16);
}

static void Actor00400_Fn05320(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w1;
    Actor100400Work* w2;
    Actor100400Work* w4;
    Actor100400Work* w5;
    GfxCoord*        coord;
    s8               armed;
    s32              cond;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;
    s32              sound3;
    s32              pan3;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_636++;
    if (work->field_636 >= 0x14) {
        w1    = arg0->work;
        armed = 0;
        if (w1->field_640 < 0xDAC && (u32)(w1->field_634 - 0x600) >= 0x400U) {
            gSceneCombatState.signals.bytes.enemyAlert = 1;
            Gp_ArmStateF0(1);
            armed         = 1;
            w2            = arg0->work;
            w2->field_638 = 4;
            w2->subState  = 0;
        }
        if (armed) {
            return;
        }
        Actor00400_TurnToward(arg0, &work->field_60C[work->field_65B & 7], 0x20, 0x30);
    }
    if (work->field_636 == 8) {
        work->field_660 = 0;
        sound           = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040007;
        pan             = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->field_636 == 0xC) {
        Actor00400_SpawnRing(arg0, work, coord);
    }
    if (work->field_636 == 0x14) {
        sound2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040004;
        pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    w4 = arg0->work;
    if ((w4->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (w4->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        Actor00400_SpawnRing(arg0, work, coord);
        sound3 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040008;
        pan3   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound3, pan3, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_63E = (u16)work->field_60C[work->field_65B].vy + work->field_64E;
        w5              = arg0->work;
        w5->animBlend   = 4;
        w5->animStep    = 0x10;
        w5->animClip    = 1;
        w5->animRequest = 1;
        work->subState++;
    }
}

static void Actor00400_Fn05728(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    Actor100400Work* state;
    Actor100400Work* state2;
    Actor100400Work* state3;
    u32              random;
    s16              next;
    u8               idx;
    u8               idx2;

    work = arg0->work;
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        if (work->field_640 < 0x2710 && (u32)(work->field_634 - 0xC0) >= 0xE81U) {
            random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = random;
            if ((random >> 16) & 1) {
                work2                              = arg0->work;
                work2->field_638                   = 0xA;
                work2->subState                    = 0;
                work2->field_614[work2->field_65A] = work2->field_638;
                next                               = 4;
                if (work2->field_614[0] == work2->field_614[1] &&
                    work2->field_614[0] == work2->field_614[2] && work2->field_614[0] == 0xA) {
                    state                              = arg0->work;
                    state->field_638                   = next;
                    state->subState                    = 0;
                    work2->field_614[work2->field_65A] = next;
                    work2->field_64C                   = 0x5A;
                }
                idx              = work2->field_65A + 1;
                work2->field_65A = idx;
                if (idx >= 3U) {
                    work2->field_65A = 0;
                }
            } else {
                work->field_64C   = 0x5A;
                state2            = arg0->work;
                state2->field_638 = 4;
                state2->subState  = 0;
            }
        } else {
            state3                           = arg0->work;
            state3->field_638                = 4;
            state3->subState                 = 0;
            work->field_614[work->field_65A] = work->field_638;
            idx2                             = work->field_65A + 1;
            work->field_65A                  = idx2;
            if (idx2 >= 3U) {
                work->field_65A = 0;
            }
        }
    }
}

static void Actor00400_Fn058C4(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    Actor100400Work* state;
    Actor100400Work* state2;
    GfxCoord*        coord;
    SVECTOR          delta;
    SVECTOR          vec2;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;
    s16              next;
    u8               idx;
    u8               idx2;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_636++;
    coord->coord.t[0] += (work->field_574.vx - coord->coord.t[0]) >> 4;
    coord->coord.t[2] += (work->field_574.vz - coord->coord.t[2]) >> 4;
    work->field_63E    = work->field_64E + 0x64;
    if (work->field_636 == 8) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040007;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->field_636 == 0xC) {
        Actor00400_SpawnRing(arg0, work, coord);
    }
    if (work->field_636 == 0x14) {
        sound2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040004;
        pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->field_636 < 0x15) {
        return;
    }
    Actor00400_TurnToward(arg0, &work->field_5E4, 0x18, 0x30);
    if (work->field_636 < 0x1F) {
        return;
    }
    if (work->field_640 < 0x2710 && (u32)(work->field_634 - 0xC0) >= 0xE81U) {
        work2                              = arg0->work;
        work2->field_638                   = 0xA;
        work2->subState                    = 0;
        work2->field_614[work2->field_65A] = work2->field_638;
        next                               = 4;
        if (work2->field_614[0] == work2->field_614[1] &&
            work2->field_614[0] == work2->field_614[2] && work2->field_614[0] == 0xA) {
            state                              = arg0->work;
            state->field_638                   = next;
            state->subState                    = 0;
            work2->field_614[work2->field_65A] = next;
            work2->field_64C                   = 0x5A;
        }
        idx              = work2->field_65A + 1;
        work2->field_65A = idx;
        if (idx >= 3U) {
            work2->field_65A = 0;
        }
    } else {
        vec2.vx = vec2.vy = vec2.vz = 0;
        Actor00400_Fn031A4(arg0, &vec2);
        delta.vx = vec2.vx - coord->coord.t[0];
        delta.vy = 0;
        delta.vz = vec2.vz - coord->coord.t[2];
        if ((s16)SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz) >= 0xDAC) {
            Actor00400_Fn02FF8(arg0);
            state2            = arg0->work;
            state2->field_638 = 4;
            state2->subState  = 0;
        }
        work->field_614[work->field_65A] = work->field_638;
        idx2                             = work->field_65A + 1;
        work->field_65A                  = idx2;
        if (idx2 >= 3U) {
            work->field_65A = 0;
        }
    }
}

static void Actor00400_Fn05D00(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;
    GfxCoord*        coord;
    GfxCoord*        coord2;
    SVECTOR          vec;
    s32              sound;
    s32              pan;
    s32              i;
    s16              y;

    coord           = arg0->extra.tmd->coords;
    work            = arg0->work;
    work->field_660 = 1;
    work->field_63E = (u16)work->field_60C[work->field_65B].vy + work->field_64E;
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    if (work->animClip != 3) {
        i      = 0;
        y      = work->field_64E - coord->coord.t[1] + 0xFA;
        coord2 = arg0->extra.tmd->coords;
        do {
            vec.vx = (u32)rsin(i << 8) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 8) >> 3;
            Gp_SpawnEff(gRoomEffectWaterSprayId, coord2, 0x01202148, &vec);
            i++;
        } while (i < 16);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040008;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        w              = arg0->work;
        w->animBlend   = 4;
        w->animStep    = 0x10;
        w->animClip    = 1;
        w->animRequest = 1;
    } else {
        work->subState++;
    }
    work->subState++;
}

static void Actor00400_Fn05EA4(Task* arg0)
{
    Actor100400Work* work;
    GfxCoord*        coord;
    SVECTOR          vec;
    s32              id;
    s32              pan;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    Actor00400_Fn02FF8(arg0);
    vec.vx          = work->field_56C.vx - coord->coord.t[0];
    vec.vy          = 0;
    vec.vz          = work->field_56C.vz - coord->coord.t[2];
    work->field_63E = (u16)work->field_60C[work->field_65B].vy + work->field_64E;
    if ((s16)SquareRoot0(vec.vx * vec.vx + vec.vz * vec.vz) < 800 && work->field_64C == 0) {
        Actor100400Work* w;
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        work->field_636 = 0;
        w               = arg0->work;
        w->field_638    = 3;
        w->subState     = 0;
        return;
    }
    if (work->animClip != 3) {
        Actor100400Work* w;
        w              = arg0->work;
        w->animBlend   = 10;
        w->animStep    = 0x10;
        w->animClip    = 3;
        w->animRequest = 1;
    }
    Actor00400_TurnToward(arg0, &work->field_56C, 0x30, 0x100);
    diverStepForward(arg0, 0x60, work->field_556);
    if (!(work->field_630 & 0xF)) {
        id  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040001;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}

static void Actor00400_Fn060CC(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              cond;

    work = arg0->work;
    if (work->field_642 != 0 && work->field_644 == 1) {
        work->animStep    = 0x20;
        work->animClip    = 0xA;
        work->animRequest = 2;
        id                = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        return;
    }
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        work2 = arg0->work;
        if ((work2->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (work2->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->work;
            work2->field_638 = 2;
            work2->subState  = 0;
        }
    }
}

static void Actor00400_Fn061E8(Task* arg0)
{
    Actor100400Work* work;
    GfxCoord*        coord;
    GfxCoord*        coord2;
    SVECTOR          vec;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;
    s32              i;
    s16              y;

    work              = arg0->work;
    coord             = arg0->extra.tmd->coords;
    work->animBlend   = 3;
    work->animStep    = 0x10;
    work->animClip    = 0xB;
    work->animRequest = 1;
    sound             = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
    i                 = 0;
    pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    y      = work->field_64E - coord->coord.t[1] + 0xFA;
    coord2 = arg0->extra.tmd->coords;
    do {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = y;
        vec.vz = (u32)rcos(i << 8) >> 3;
        Gp_SpawnEff(gRoomEffectWaterSprayId, coord2, 0x01202148, &vec);
        i++;
    } while (i < 16);
    sound2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040008;
    pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->subState++;
}

static void Actor00400_Fn06380(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;

    work = arg0->work;
    work->field_636++;
    Actor00400_TurnToward(arg0, &work->field_5E4, 0x10, 0x20);
    if (work->field_636 == 1) {
        work->field_646 = 0x18;
    }
    if (work->field_636 >= 0x24) {
        work->field_636 = 0;
        w               = arg0->work;
        w->animBlend    = 8;
        w->animStep     = 0x10;
        w->animClip     = 7;
        w->animRequest  = 1;
        work->subState++;
    }
}

/// Spawns the marker task from `Actor00400_D16028[1]` and hands it a 0x64-byte
/// work block: coordinate 5 gives the task's root translation, and the view
/// space span from coordinate 4's base to the same point raised by `height` -
/// the per-enemy value `Actor00400_D1609C` selects - is stored in the work.
static inline void Actor00400_SpawnMarker(Task* arg0)
{
    AreaPlacement*         params;
    Actor100400MarkerWork* marker;
    GfxCoord*              coords;
    GfxCoord*              origin;
    GfxCoord*              span;
    GfxCoord*              dst;
    Task*                  task;
    SVECTOR                pos;
    SVECTOR                base;
    SVECTOR                tip;
    u16                    height;

    params = ((Enemy*)arg0->spawnArg2.pointer)->place;
    if (params != NULL) {
        height = Actor00400_D1609C[params->rowIndex & 7];
    } else {
        height = 0xBE;
    }
    coords = arg0->extra.tmd->coords;
    origin = &coords[5];
    span   = &coords[4];
    task   = Task_SpawnFromTable(Actor00400_D16028, 1, 0, 0);
    if (task != NULL) {
        marker = memCalloc(sizeof(Actor100400MarkerWork), false);
        if (marker == NULL) {
            taskKill(task);
        } else {
            base.vx = 0;
            base.vy = 0;
            base.vz = 0;
            tip.vx  = 0;
            tip.vy  = 0;
            tip.vz  = height;
            coordLocalToWorld(span, &base);
            coordLocalToWorld(span, &tip);
            task->work = marker;
            dst        = task->extra.tmd->coords;
            pos.vx     = 0;
            pos.vy     = 0;
            pos.vz     = 0;
            coordLocalToWorld(origin, &pos);
            dst->coord.t[0]  = pos.vx;
            dst->coord.t[1]  = pos.vy;
            dst->coord.t[2]  = pos.vz;
            marker->field_58 = tip.vx - base.vx;
            marker->field_5A = tip.vy - base.vy;
            marker->field_5C = tip.vz - base.vz;
        }
    }
}

static void Actor00400_Fn064B0(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              pan;
    s32              cond;

    work = arg0->work;
    work->field_636++;
    Actor00400_TurnToward(arg0, &work->field_5E4, 0x10, 0x20);
    if (work->field_636 == 0x29) {
        id  = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4004000A;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->field_636 == 0x2B) {
        Actor00400_SpawnMarker(arg0);
    }
    work2 = arg0->work;
    if ((work2->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work2->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = arg0->work;
        work2->field_638 = 2;
        work2->subState  = 0;
    }
}

static void Actor00400_Fn06798(Task* arg0)
{
    Actor100400Work* work;
    GfxCoord*        coord;
    SVECTOR          vec;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    vec.vx = (u16)work->field_60C[work->field_65B].vx - coord->coord.t[0];
    vec.vy = (u16)work->field_60C[work->field_65B].vy - coord->coord.t[1];
    vec.vz = (u16)work->field_60C[work->field_65B].vz - coord->coord.t[2];

    work->field_63E = (u16)work->field_60C[work->field_65B].vy;
    if ((s16)SquareRoot0(vec.vx * vec.vx + vec.vz * vec.vz) < 1000) {
        work->field_65B = (work->field_65B + 1) & 7;
        return;
    }
    if (work->animClip != 3) {
        Actor100400Work* w;
        Actor100400Work* a;
        s32              i;

        w              = arg0->work;
        w->animBlend   = 10;
        w->animStep    = 0x10;
        w->animClip    = 3;
        w->animRequest = 1;

        a = arg0->work;
        if (a->animRequest == 1) {
            if (a->animPlaying != a->animClip) {
                a->field_62A = 0;
            } else {
                a->field_62A = Actor00400_Fn086FC(arg0, a->field_62A);
            }
            Actor00400_Fn08624(arg0);
            a->animRequest = 3;
        } else if (a->animRequest == 2) {
            diverRestartClip(arg0);
            a->animRequest = 3;
            a->field_62A   = 0;
        } else if (a->animRequest == 3) {
            a->field_62A++;
        }
        i = 1;
        do {
            animationTickSlot(&a->anim, i);
            i++;
        } while (i < 0xF);
    }
    Actor00400_TurnToward(arg0, &work->field_60C[work->field_65B], 0x2C, 0x100);
    diverStepForward(arg0, 0x60, work->field_556);
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
}

static void Actor00400_Fn06A44(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;
    s32              id;
    s32              pan;

    work = arg0->work;
    work->field_636++;
    if (work->field_636 == 1) {
        SndEvt_EnqueueType6(((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54220005, 0, 0);
    }
    if (work->field_636 == 8) {
        work->field_660 = 0;
        id              = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040007;
        pan             = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->field_636 == 0x14) {
        work->field_646 = 0x18;
        w               = arg0->work;
        w->animBlend    = 4;
        w->animStep     = 0x10;
        w->animClip     = 7;
        w->animRequest  = 1;
        work->field_636 = 0;
        work->subState++;
    }
}

/// Per-frame callback for the text actor's third task, with the same
/// `gSceneCombatState.actorControl` frame gate as `Actor00400_Fn04B48`: 2 only flags the model
/// hidden, 0 runs this frame's state handler and rebuilds the root rotation
/// before falling through to the draw half, and 1 is the draw half on its own.
static void Actor00400_Fn06B7C(Task* arg0)
{
    Actor100400Work*  work             = arg0->work;
    Enemy*            obj              = arg0->spawnArg2.pointer;
    TmdObject*        ctx              = arg0->extra.tmd;
    void              (*fns[2])(Task*) = { Actor00400_Fn08A88, Actor00400_Fn08B40 };
    Actor100400Mat    m;
    GfxRotationWords* ia;
    Actor100400Work*  w;
    Actor100400Work*  w2;
    Actor100400Work*  w3;
    Actor100400Work*  work2;
    TmdObject*        ctx2;
    GfxCoord*         coord;
    MATRIX*           dst;
    s32               i;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->flags_62C.hi.field_62E++;
            work->field_630++;
            Actor00400_Fn01454(arg0);
            fns[work->field_638](arg0);
            w = arg0->work;
            if (w->animRequest == 1) {
                if (w->animPlaying != w->animClip) {
                    w->field_62A = 0;
                } else {
                    w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
                }
                Actor00400_Fn08624(arg0);
                w->animRequest = 3;
            } else if (w->animRequest == 2) {
                diverRestartClip(arg0);
                w->animRequest = 3;
                w->field_62A   = 0;
            } else if (w->animRequest == 3) {
                w->field_62A++;
            }
            i = 1;
            do {
                animationTickSlot(&w->anim, i);
                i++;
            } while (i < 0xF);
            work->flags_62C.half          = work->slots[1].flags;
            w2                            = arg0->work;
            coord                         = arg0->extra.tmd->coords;
            ia                            = &m.matrix.rotationWords;
            m.matrix.rotationWords.m00M01 = ONE;
            m.matrix.rotationWords.m02M10 = 0;
            ia->m11M12                    = ONE;
            m.matrix.rotationWords.m20M21 = 0;
            ia->m22                       = ONE;
            RotMatrixZ(w2->field_558, &m.matrix.mat);
            RotMatrixY(w2->field_556, &m.matrix.mat);
            dst                 = &coord->coord;
            dst->m[0][0]        = m.matrix.mat.m[0][0];
            dst->m[0][1]        = m.matrix.mat.m[0][1];
            dst->m[0][2]        = m.matrix.mat.m[0][2];
            dst->m[1][0]        = m.matrix.mat.m[1][0];
            dst->m[1][1]        = m.matrix.mat.m[1][1];
            dst->m[1][2]        = m.matrix.mat.m[1][2];
            dst->m[2][0]        = m.matrix.mat.m[2][0];
            dst->m[2][1]        = m.matrix.mat.m[2][1];
            dst->m[2][2]        = m.matrix.mat.m[2][2];
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->hp <= 0) {
                w3            = arg0->work;
                arg0->state   = 2;
                w3->field_638 = 0;
                w3->subState  = 0;
            }
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            Actor00400_Fn012B0(arg0, arg0->extra.tmd->coords->coord.t[1], 0x80);
            ctx->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

static inline s32 Actor00400_ConsumeStateRequest(Actor100400Work* work)
{
    s16 req;
    s32 state;

    state = work->field_642;
    if (state != 1) {
        return 0;
    }
    req = work->field_644;
    if (req == 1)
        goto set;
    if (req == 2)
        goto set;
    if (req == 3)
        goto set;
    if (req != 4)
        goto other;
set:
    /* The do/while(0) is load-bearing: flow.c weights REG_N_REFS by loop
       depth, and the two extra references it buys `work` are what let the
       pointer outrank `req` in global.c's allocation order. */
    do {
        work->field_638 = state;
        work->subState  = 0;
    } while (0);
other:
    work->field_644 = 0;
    return 1;
}

static void Actor00400_Fn06EA4(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              cond;

    work = arg0->work;
    if (Actor00400_ConsumeStateRequest(work) == 0) {
        work = arg0->work;
        if ((work->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (work->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2              = arg0->work;
            work2->animBlend   = 8;
            work2->animStep    = 4;
            work2->animClip    = 0xF;
            work2->animRequest = 1;
        }
    }
}

static void Actor00400_Fn06F64(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              id;
    s32              cond;

    work = arg0->work;
    if (work->field_642 != 0 && work->field_644 == 1) {
        work->animBlend   = 2;
        work->animStep    = 0x10;
        work->animClip    = 0x13;
        work->animRequest = 1;
        id                = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
        SndEvt_EnqueueType6(id, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        return;
    }
    if (Actor00400_ConsumeStateRequest(work) == 0) {
        work = arg0->work;
        if ((work->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (work->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work2            = arg0->work;
            work2->field_638 = 0;
            work2->subState  = 0;
        }
    }
}

/// Per-frame callback for the text actor's fourth task. Same frame gate as
/// `Actor00400_Fn06B7C`, but the draw half only recolours the actor: case 0
/// runs this frame's state handler, lerps the root coordinate's height a
/// sixteenth of the way towards `field_63E` and falls through.
static void Actor00400_Fn070C0(Task* arg0)
{
    Actor100400Work*  work             = arg0->work;
    TmdObject*        ctx              = arg0->extra.tmd;
    Enemy*            obj              = arg0->spawnArg2.pointer;
    GfxCoord*         coord0           = ctx->coords;
    void              (*fns[2])(Task*) = { Actor00400_Fn0A468, Actor00400_Fn0A4BC };
    Actor100400Mat    m;
    GfxRotationWords* ia;
    Actor100400Work*  w;
    Actor100400Work*  w2;
    Actor100400Work*  w3;
    Actor100400Work*  work2;
    TmdObject*        ctx2;
    GfxCoord*         coord;
    MATRIX*           dst;
    s32               i;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->flags_62C.hi.field_62E++;
            work->field_630++;
            Actor00400_Fn01454(arg0);
            fns[work->field_638](arg0);
            w = arg0->work;
            if (w->animRequest == 1) {
                if (w->animPlaying != w->animClip) {
                    w->field_62A = 0;
                } else {
                    w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
                }
                Actor00400_Fn08624(arg0);
                w->animRequest = 3;
            } else if (w->animRequest == 2) {
                diverRestartClip(arg0);
                w->animRequest = 3;
                w->field_62A   = 0;
            } else if (w->animRequest == 3) {
                w->field_62A++;
            }
            i = 1;
            do {
                animationTickSlot(&w->anim, i);
                i++;
            } while (i < 0xF);
            work->flags_62C.half          = work->slots[1].flags;
            w2                            = arg0->work;
            coord                         = arg0->extra.tmd->coords;
            ia                            = &m.matrix.rotationWords;
            m.matrix.rotationWords.m00M01 = ONE;
            m.matrix.rotationWords.m02M10 = 0;
            ia->m11M12                    = ONE;
            m.matrix.rotationWords.m20M21 = 0;
            ia->m22                       = ONE;
            RotMatrixZ(w2->field_558, &m.matrix.mat);
            RotMatrixY(w2->field_556, &m.matrix.mat);
            dst                 = &coord->coord;
            dst->m[0][0]        = m.matrix.mat.m[0][0];
            dst->m[0][1]        = m.matrix.mat.m[0][1];
            dst->m[0][2]        = m.matrix.mat.m[0][2];
            dst->m[1][0]        = m.matrix.mat.m[1][0];
            dst->m[1][1]        = m.matrix.mat.m[1][1];
            dst->m[1][2]        = m.matrix.mat.m[1][2];
            dst->m[2][0]        = m.matrix.mat.m[2][0];
            dst->m[2][1]        = m.matrix.mat.m[2][1];
            dst->m[2][2]        = m.matrix.mat.m[2][2];
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Actor00400_Fn01B90(arg0);
            if ((s16)obj->hp <= 0) {
                w3            = arg0->work;
                arg0->state   = 4;
                w3->field_638 = 0;
                w3->subState  = 0;
            }
            coord0->coord.t[1] += (work->field_63E - coord0->coord.t[1]) >> 4;
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            ctx2  = arg0->extra.tmd;
            work2 = arg0->work;
            Actor00400_UpdateColor(arg0, &ctx2->coords[1], work2, ctx2);
            ctx->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

static void Actor00400_Fn07400(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    Actor100400Work* work3;
    s32              phase;
    s32              cond;

    work = arg0->work;
    if (Actor00400_ConsumeStateRequest(work) == 0) {
        phase           = (u16)work->field_636 + 1;
        work->field_636 = phase;
        work->field_63E = work->field_658 + ((u16)work->field_64E + ((rsin(phase << 16 >> 10) * 0x10) >> 10));
        work2           = arg0->work;
        if ((work2->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (work2->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work3              = arg0->work;
            work3->animBlend   = 8;
            work3->animStep    = 2;
            work3->animClip    = 0x10;
            work3->animRequest = 1;
        }
    }
}

static void Actor00400_Fn07518(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              phase;

    work = arg0->work;
    if (work->field_642 != 0 && work->field_644 == 1) {
        work->animBlend   = 2;
        work->animStep    = 0x10;
        work->animClip    = 0x12;
        work->animRequest = 1;
    }
    if (Actor00400_ConsumeStateRequest(arg0->work) == 0) {
        phase           = (u16)work->field_636 + 1;
        work->field_636 = phase;
        work->field_63E = work->field_658 + ((u16)work->field_64E + ((rsin(phase << 16 >> 9) * 0x10) >> 9));
        if (work->field_636 >= 0x79) {
            work2            = arg0->work;
            work2->field_638 = 0;
            work2->subState  = 0;
        }
    }
}

#include "../../shared/diver_step_forward.inc.c"

/// Two-state dispatcher over a handler table built on the stack.
void Actor00400_Fn076E8(Task* task)
{
    TaskFunc funcs[2] = {
        Actor00400_Fn0A2F4,
        Actor00400_Fn0A364,
    };

    funcs[task->state](task);
}

/// Draws two LCG values into the work's `field_62E`/`field_630`, resets the
/// state counters and copies the root coordinate's `t[1]` into `field_63E`.
static void Actor00400_Fn07738(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    Actor100400Work* state2;
    GfxCoord*        coord;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    Gp_IncStateF0Ref(0);
    gRandomLcgState              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->flags_62C.hi.field_62E = gRandomLcgState >> 16;
    gRandomLcgState              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_630              = gRandomLcgState >> 16;
    state                        = arg0->work;
    state->animStep              = 0x10;
    state->animClip              = 1;
    state->animRequest           = 2;
    state2                       = arg0->work;
    state2->field_638            = 1;
    state2->subState             = 0;
    work->field_63E              = coord->coord.t[1];
}

/// States `Actor00400_Fn077F4` dispatches on `Actor100400Work.subState`.
static const TaskFuncTable4 Actor00400_D00134 = { {
    Actor00400_Fn094C0,
    Actor00400_Fn094DC,
    Actor00400_Fn05320,
    Actor00400_Fn095D8,
} };

static void Actor00400_Fn077F4(Task* arg0)
{
    Actor100400Work* work;
    TaskFuncTable4   handlers;
    Actor100400Work* work2;

    work     = arg0->work;
    handlers = Actor00400_D00134;
    if ((Actor00400_Fn02154(arg0) << 0x10) != 0) {
        gSceneCombatState.signals.bytes.enemyAlert = 1;
        Gp_ArmStateF0(1);
    } else if (gSceneCombatState.signals.bytes.enemyAlert != 0) {
        work2            = arg0->work;
        work2->field_638 = 4;
        work2->subState  = 0;
    } else {
        handlers.funcs[(s16)work->subState](arg0);
    }
}

static void Actor00400_Fn078C8(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0962C,
        Actor00400_Fn058C4,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[(s16)work->subState](arg0);
    }
}

/// States `Actor00400_Fn0793C` and `Actor00400_Fn09C04` dispatch on `Actor100400Work.subState`.
static const TaskFuncTable3 Actor00400_D00144 = { {
    Actor00400_Fn05D00,
    Actor00400_Fn096C0,
    Actor00400_Fn05EA4,
} };

static void Actor00400_Fn0793C(Task* arg0)
{
    Actor100400Work* work;
    TaskFuncTable3   fns;

    work = arg0->work;
    fns  = Actor00400_D00144;
    fns.funcs[(s16)work->subState](arg0);
}

static void Actor00400_Fn07998(Task* task)
{
}

static void Actor00400_Fn079A0(Task* task)
{
}

static void Actor00400_Fn079A8(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        diverState7Enter,
        Actor00400_Fn060CC,
    };

    states[(s16)work->subState](arg0);
}

static inline s32 Actor00400_TakeStateRequest(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->field_642 == 1) {
        switch (work->field_644) {
            case 2:
                work->field_638 = 8;
                work->subState  = 0;
                work->field_644 = 0;
                return 1;
            case 3:
                work->field_638 = 9;
                work->subState  = 0;
                work->field_644 = 0;
                return 1;
        }
    }
    work->field_644 = 0;
    return 0;
}

static void Actor00400_Fn079FC(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn061E8,
        Actor00400_Fn097C8,
    };
    s16 taken;

    taken = Actor00400_TakeStateRequest(arg0);
    if (taken == 0) {
        states[(s16)work->subState](arg0);
    }
}

static void Actor00400_Fn07ABC(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn098A8,
        Actor00400_Fn09924,
    };

    states[(s16)work->subState](arg0);
}

/// States `Actor00400_Fn07B10` dispatches on `Actor100400Work.subState`.
static const TaskFuncTable3 Actor00400_D00150 = { {
    Actor00400_Fn09A1C,
    Actor00400_Fn06380,
    Actor00400_Fn064B0,
} };

static void Actor00400_Fn07B10(Task* arg0)
{
    Actor100400Work* work;
    TaskFuncTable3   fns;

    work = arg0->work;
    fns  = Actor00400_D00150;
    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        fns.funcs[(s16)work->subState](arg0);
    }
}

/// States `Actor00400_Fn07B98` dispatches on `Actor100400Work.subState`.
static const TaskFuncTable3 Actor00400_D0015C = { {
    Actor00400_Fn09A48,
    Actor00400_Fn09A8C,
    Actor00400_Fn06798,
} };

static void Actor00400_Fn07B98(Task* arg0)
{
    Enemy*           obj;
    Actor100400Work* work;
    TaskFuncTable3   fns;

    obj                         = arg0->spawnArg2.pointer;
    work                        = arg0->work;
    fns                         = Actor00400_D0015C;
    work->field_660             = 1;
    obj->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    fns.funcs[(s16)work->subState](arg0);
}

/// States `Actor00400_Fn07C04` dispatches on `Actor100400Work.subState`.
static const TaskFuncTable4 Actor00400_D00168 = { {
    Actor00400_Fn09AE0,
    Actor00400_Fn09B44,
    Actor00400_Fn09B74,
    Actor00400_Fn09BDC,
} };

static void Actor00400_Fn07C04(Task* arg0)
{
    Enemy*           obj;
    Actor100400Work* work;
    TaskFuncTable4   handlers;
    Actor100400Work* work2;

    obj      = arg0->spawnArg2.pointer;
    work     = arg0->work;
    handlers = Actor00400_D00168;
    if (GameFlag_GetNibble(GAME_FLAG_SUBMARINE_TUNNEL_EVENT_SEEN) != 0) {
        work2            = arg0->work;
        work2->field_638 = 0xB;
        work2->subState  = 0;
    } else {
        work->field_660             = 1;
        obj->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        handlers.funcs[(s16)work->subState](arg0);
    }
}

static void Actor00400_Fn07CC4(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* w;
    s32              i;

    work = arg0->work;
    work->field_636++;
    w = arg0->work;
    if (w->animRequest == 1) {
        if (w->animPlaying != w->animClip) {
            w->field_62A = 0;
        } else {
            w->field_62A = Actor00400_Fn086FC(arg0, w->field_62A);
        }
        Actor00400_Fn08624(arg0);
        w->animRequest = 3;
    } else if (w->animRequest == 2) {
        diverRestartClip(arg0);
        w->animRequest = 3;
        w->field_62A   = 0;
    } else if (w->animRequest == 3) {
        w->field_62A++;
    }
    i = 1;
    do {
        animationTickSlot(&w->anim, i);
        i++;
    } while (i < 0xF);
    if (work->field_636 >= 0x3C) {
        work->field_638++;
    }
}

static void Actor00400_Fn07DE0(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->field_636 = 0;
    work->field_638 = (u16)work->field_638 + 1;
}

static void Actor00400_Fn07E20(Task* arg0)
{
    Actor100400Work* work;
    TmdObject*       ctx;

    work = arg0->work;
    ctx  = arg0->extra.tmd;
    if (++work->field_636 >= 0x18) {
        ctx->flags     |= TMD_OBJECT_SEMI_TRANS;
        work->field_636 = 0;
        work->field_638++;
    }
}

static void Actor00400_Fn07E74(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (++work->field_636 == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    if (work->field_636 > 0x20) {
        work->field_638++;
    }
}

static void Actor00400_Fn07EE8(Task* arg0)
{
    TmdObject*       ctx;
    Actor100400Work* work;

    ctx             = arg0->extra.tmd;
    ctx->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work            = arg0->work;
    arg0->state     = 5;
    work->field_638 = 0;
    work->subState  = 0;
}

static void Actor00400_Fn07F18(Task* arg0)
{
    TmdObject*       ctx;
    Actor100400Work* work;

    ctx             = arg0->extra.tmd;
    work            = arg0->work;
    ctx->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->field_636 = 0;
    work->field_638++;
}

static void Actor00400_Fn07F44(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (++work->field_636 >= 2) {
        work->field_638++;
    }
}

static void Actor00400_Fn07F88(Task* arg0)
{
    TmdObject*       model;
    Actor100400Work* work;

    model = arg0->extra.tmd;
    work  = arg0->work;
    Tmd_FreeBuffers(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    Actor00400_Fn0237C(arg0);
    work->field_638 = (u16)work->field_638 + 1;
}

static void Actor00400_Fn07FEC(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    arg0->state     = 5;
    work->field_638 = 0;
    work->subState  = 0;
}

void Actor00400_Fn08004(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = Actor00400_D0002C;
    sp.funcs[arg0->state](arg0);
}

/// States `Actor00400_Fn09C04` dispatches on `Actor100400Work.subState`.
static const TaskFuncTable7 Actor00400_D00178 = { {
    Actor00400_Fn09CCC,
    Actor00400_Fn09D3C,
    Actor00400_Fn09D98,
    Actor00400_Fn09E70,
    Actor00400_Fn06A44,
    Actor00400_Fn09F18,
    Actor00400_Fn09FDC,
} };

void Actor00400_Fn0805C(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    Actor100400Work* work;
    Enemy*           obj;
    GfxCoord*        coord;
    Actor100400Work* state;

    work  = arg0->work;
    obj   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    switch (request->command) {
        case 1:
            work->field_65E = 1;
            break;
        case 2:
            work->field_65E = 2;
            break;
        case 3:
            work->field_65E = 3;
            break;
        case 4:
            work->field_65E = 4;
            break;
        case 5:
            work->field_65E = 5;
            break;
        case 6:
            obj->node.state.parts.flags = 0;
            Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
            work->field_666   = 0;
            work->field_65E   = 6;
            coord->coord.t[1] = 0;
            arg0->state       = 1;
            state             = arg0->work;
            state->field_638  = 0;
            state->subState   = 0;
            state             = arg0->work;
            state->field_638  = 2;
            state->subState   = 0;
            break;
    }
}

static void Actor00400_Fn0814C(Task* arg0, s16 arg1, SVECTOR* arg2, s16 arg3)
{
    MATRIX           m;
    VECTOR           d;
    VECTOR           r;
    GfxCoord*        coords;
    Actor100400Work* work;

    coords = arg0->extra.tmd->coords;
    work   = arg0->work;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords[arg1].workm, &m);
    d.vx = work->field_5E4.vx - m.t[0];
    d.vy = work->field_5E4.vy - arg3 - m.t[1];
    d.vz = work->field_5E4.vz - m.t[2];
    ApplyTransposeMatrixLV(&coords->coord, &d, &r);
    arg2->vx = ratan2(-r.vy, r.vz) << 20 >> 20;
    arg2->vy = ratan2(r.vx, r.vz) << 20 >> 20;
    arg2->vz = 0;
}

static void Actor00400_Fn0824C(Task* arg0, s16 arg1, s16 arg2, SVECTOR* arg3)
{
    MATRIX    a;
    MATRIX    b;
    GfxCoord* coordA;
    GfxCoord* coordB;
    GfxCoord* coords;

    coords                     = arg0->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    coordA                     = &coords[arg1];
    coordB                     = &coords[arg2];
    Gp_UpdateCoord(&gGfxViewCoord);
    coordA->composeStamp = GRAPHICS_COORD_DIRTY;
    coordB->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coordA);
    Gp_UpdateCoord(coordB);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coordA->workm, &a);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coordB->workm, &b);
    arg3->vx             = (a.t[0] + b.t[0]) / 2;
    arg3->vz             = (a.t[2] + b.t[2]) / 2;
    coordA->composeStamp = GRAPHICS_COORD_DIRTY;
    coordB->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Reaction to message 0x7D5: `arg2` toggles the "big" flag (0x80) on the
/// actor's context. Turning it on is unconditional; turning it off first checks
/// whether the current state / animation combination still wants it held.
///
/// GCC 2.8.1 decides the store to `gSceneCombatState`, at a fixed address, cannot
/// alias the struct fields reached through `ctx` / `work`, so without the
/// barrier the scheduler sinks this `sb` past the traffic that follows it.
void Actor00400_Fn08354(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Actor100400Work* work;
    TmdObject*       ctx;
    s32              state;

    work = arg0->work;
    ctx  = arg0->extra.tmd;
    switch (arg2) {
        case 0:
            gSceneCombatState.actor00400HideRequested = 1;
            ctx->flags                               |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_663                           = 1;
            break;
        case 1:
            gSceneCombatState.actor00400HideRequested = 0;
            state                                     = arg0->state;
            if (((state == 2) || (state == 4)) && (work->field_644 == 4)) {
                ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else if ((arg0->state == 2) && ((work->field_638 == 4) || (work->field_638 == 5))) {
                ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else if ((arg0->state == 4) && (work->field_638 == 6)) {
                ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else if (arg0->state == 5) {
                ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else {
                ctx->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            work->field_663 = 0;
            break;
    }
}

static void Actor00400_Fn08464(Task* arg0, s16 arg1, s16 arg2, SVECTOR* arg3)
{
    MATRIX    root;
    MATRIX    a;
    MATRIX    b;
    GfxCoord* coordA;
    GfxCoord* coordB;
    GfxCoord* coords;

    coords                     = arg0->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    coordA                     = &coords[arg1];
    coordB                     = &coords[arg2];
    Gp_UpdateCoord(&gGfxViewCoord);
    coordA->composeStamp = GRAPHICS_COORD_DIRTY;
    coordB->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coordA);
    Gp_UpdateCoord(coordB);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords[0].workm, &root);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coordA->workm, &a);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coordB->workm, &b);
    coords[0].coord.t[0]   = arg3->vx - ((a.t[0] + b.t[0]) / 2 - root.t[0]);
    coords[0].coord.t[2]   = arg3->vz - ((a.t[2] + b.t[2]) / 2 - root.t[2]);
    coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    coordA->composeStamp   = GRAPHICS_COORD_DIRTY;
    coordB->composeStamp   = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coordA);
    Gp_UpdateCoord(coordB);
    Gp_UpdateCoord(coords);
}

#include "../../shared/diver_restart_clip.inc.c"

/// Like `diverRestartClip`, but restarts every slot through `animationSeekSlotWithBlend`
/// with the pending blend value `animBlend`, which is consumed (cleared) only
/// when the requested clip `animClip` differs from the current `animPlaying`.
static void Actor00400_Fn08624(Task* arg0)
{
    Actor100400Work* work;
    s32              i;

    work = arg0->work;
    if (work->animPlaying == work->animClip) {
        i = 1;
        do {
            work->slots[i].rate = work->animStep;
            animationSeekSlotWithBlend(&work->anim, i, work->animClip, 0, work->animBlend);
            i++;
        } while (i < 0xF);
    } else {
        i = 1;
        do {
            work->slots[i].rate = work->animStep;
            animationSeekSlotWithBlend(&work->anim, i, work->animClip, 0, work->animBlend);
            i++;
        } while (i < 0xF);
        work->animBlend = 0;
    }
    work->animPlaying = (u16)work->animClip;
}

/// Scales `arg1` (a 12-bit angle) by the ratio `work->animStep`, returning 0
/// while that field is unset.
static s16 Actor00400_Fn086FC(Task* arg0, s16 arg1)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->animStep == 0) {
        return 0;
    }
    return ((arg1 << 8) / work->animStep << 12) >> 16;
}

/// Turns `work->field_556` toward `arg1` by at most `arg2` per call, but only
/// once the shortest signed 12-bit angle difference leaves the deadband
/// `arg3 & 0x7FF`. The two conditions share one arm rather than nesting, which
/// collapses the arms into the entry block: `work` then lives over 32 insns,
/// exactly tying its allocator priority with `range`'s, and the tie falls
/// through to the declaration order - hence `range` is declared ahead of `work`.
static void Actor00400_Fn0875C(Task* arg0, SVECTOR* arg1, s32 arg2, s32 arg3)
{
    s32              range;
    Actor100400Work* work;
    GfxCoord*        coords;
    SVECTOR          vec;
    s32              diff;
    s32              yaw;
    u16              angle;

    work                 = arg0->work;
    coords               = arg0->extra.tmd->coords;
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
    range                = arg3 & 0x7FF;
    vec.vx               = (u16)arg1->vx - coords->coord.t[0];
    vec.vy               = 0;
    vec.vz               = (u16)arg1->vz - coords->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    angle = work->field_556;
    diff  = ((angle - yaw) << 20) >> 20;
    if ((diff > range) || (diff < -range)) {
        work->field_556 = (diff > range) ? (angle - arg2) : (angle + arg2);
    }
}

/// One animation-step: state 1 starts the clip `animClip` (or advances the
/// current one through `Actor00400_Fn086FC` when it is already in place, and
/// resets `field_62A` when it is not), state 2 finishes the old clip and state
/// 3 counts `field_62A` up a frame at a time. All three land in state 3 and
/// then tick animation slots 1..14.
static void Actor00400_Fn08814(Task* arg0)
{
    Actor100400Work* work;
    s32              i;

    work = arg0->work;
    if (work->animRequest == 1) {
        if (work->animPlaying != work->animClip) {
            work->field_62A = 0;
        } else {
            work->field_62A = Actor00400_Fn086FC(arg0, work->field_62A);
        }
        Actor00400_Fn08624(arg0);
        work->animRequest = 3;
    } else if (work->animRequest == 2) {
        diverRestartClip(arg0);
        work->animRequest = 3;
        work->field_62A   = 0;
    } else if (work->animRequest == 3) {
        work->field_62A++;
    }
    i = 1;
    do {
        animationTickSlot(&work->anim, i);
        i++;
    } while (i < 0xF);
}

static void Actor00400_Fn088EC(Task* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    Actor100400Work* work;

    work              = arg0->work;
    work->animBlend   = arg3;
    work->animStep    = arg2;
    work->animClip    = arg1;
    work->animRequest = 1;
}

/// Same body as src/lib/actors_shared_8016974c.c.
static s16 Actor00400_Fn08908(Task* arg0)
{
    Actor100400Work* work = arg0->work;

    if ((work->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}

void Actor00400_Fn08948(Task* arg0)
{
    TaskFuncTable8 fns;

    fns = Actor00400_D00038;
    fns.funcs[arg0->state](arg0);
}

static void Actor00400_Fn089C8(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A3D4,
        Actor00400_Fn0A414,
    };

    states[work->field_638](arg0);
}

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation row
/// alone. Same body as src/lib/actors_shared_80132c4c.c.
static void Actor00400_Fn08A1C(MATRIX* src, MATRIX* dst)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
}

static void Actor00400_Fn08A88(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn08ADC,
        Actor00400_Fn06EA4,
    };

    states[(s16)work->subState](arg0);
}

static void Actor00400_Fn08ADC(Task* arg0)
{
    Actor100400Work* state;
    Actor100400Work* work;
    u32              random;

    work               = arg0->work;
    random             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState    = random;
    state              = arg0->work;
    state->animBlend   = 8;
    state->animStep    = ((random >> 16) & 3) + 3;
    state->animClip    = 0xF;
    state->animRequest = 1;
    work->field_636    = 0;
    work->subState++;
}

static void Actor00400_Fn08B40(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn08B94,
        Actor00400_Fn06F64,
    };

    states[(s16)work->subState](arg0);
}

static void Actor00400_Fn08B94(Task* arg0)
{
    s32              sound;
    s32              pan;
    Actor100400Work* work;
    Actor100400Work* state;

    work  = arg0->work;
    sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    state              = arg0->work;
    state->animBlend   = 2;
    state->animStep    = 0x10;
    state->animClip    = 0x13;
    state->animRequest = 1;
    work->subState++;
}

static void Actor00400_Fn08C54(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    s16              mode;
    s32              i;

    work = arg0->work;
    work->field_636++;

    state = arg0->work;
    mode  = state->animRequest;
    if (mode == 1) {
        if (state->animPlaying != state->animClip) {
            state->field_62A = 0;
        } else {
            state->field_62A = Actor00400_Fn086FC(arg0, state->field_62A);
        }
        Actor00400_Fn08624(arg0);
        state->animRequest = 3;
    } else if (mode == 2) {
        diverRestartClip(arg0);
        state->animRequest = 3;
        state->field_62A   = 0;
    } else if (mode == 3) {
        state->field_62A++;
    }

    for (i = 1; i < 15; i++) {
        animationTickSlot(&state->anim, i);
    }

    if (work->field_636 >= 0x1E) {
        work->field_638++;
    }
}

static void Actor00400_Fn08D70(Task* arg0)
{
    Actor100400Work* work;
    GfxCoord*        coord;

    work            = arg0->work;
    coord           = arg0->extra.tmd->coords;
    work->field_61E = 0x1000;
    work->field_5BC = coord->coord;
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->field_636 = 0;
    work->field_638 = work->field_638 + 1;
}

static void Actor00400_Fn08DFC(Task* arg0)
{
    TmdObject*       ctx;
    Actor100400Work* work;

    work = arg0->work;
    ctx  = arg0->extra.tmd;
    if (++work->field_636 >= 0x18) {
        ctx->flags     |= TMD_OBJECT_SEMI_TRANS;
        work->field_636 = 0;
        work->field_638++;
    }
}

static void Actor00400_Fn08E50(Task* arg0)
{
    TmdObject*       ctx;
    Actor100400Work* work;
    GfxCoord*        coord;
    VECTOR           scale;
    SVECTOR          pos;

    work  = arg0->work;
    ctx   = arg0->extra.tmd;
    coord = ctx->coords;

    work->field_648 -= 7;
    if (work->field_648 < 0) {
        work->field_648 = 0;
    }
    work->field_61E -= 0x40;
    scale.vx         = 0x1000;
    scale.vy         = work->field_61E;
    scale.vz         = 0x1000;
    coord->coord     = work->field_5BC;
    ScaleMatrix(&coord->coord, &scale);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (++work->field_636 == 4) {
        pos.vx = 0;
        pos.vy = 0;
        pos.vz = 0;
        Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 4, &pos);
    }
    if (work->field_636 == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    if (work->field_636 >= 0x21) {
        ctx->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_638++;
    }
}

static void Actor00400_Fn08FB0(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    arg0->state     = 5;
    work->field_638 = 0;
    work->subState  = 0;
}

static void Actor00400_Fn08FC8(Task* arg0)
{
    TmdObject*       ctx;
    Actor100400Work* work;

    ctx             = arg0->extra.tmd;
    work            = arg0->work;
    ctx->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->field_636 = 0;
    work->field_638++;
}

static void Actor00400_Fn08FF4(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (++work->field_636 >= 2) {
        work->field_638++;
    }
}

static void Actor00400_Fn09038(Task* arg0)
{
    TmdObject*       model;
    Actor100400Work* work;

    model = arg0->extra.tmd;
    work  = arg0->work;
    Tmd_FreeBuffers(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    Actor00400_Fn0237C(arg0);
    work->field_638 = (u16)work->field_638 + 1;
}

static void Actor00400_Fn0909C(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    arg0->state     = 5;
    work->field_638 = 0;
    work->subState  = 0;
}

static void Actor00400_Fn090B4(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    Actor100400Work* state2;

    work                                                      = arg0->work;
    ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
    Gp_IncStateF0Ref(0);
    work->flags_62C.hi.field_62E = 0;
    work->field_630              = 0x174B;
    state                        = arg0->work;
    state->animStep              = 0x10;
    state->animClip              = 2;
    state->animRequest           = 2;
    state2                       = arg0->work;
    state2->field_638            = 1;
    state2->subState             = 0;
}

/// Every path out of the range test funnels through `set`, where the arm flag
/// is copied for the test below: the in-range edge arrives with the same value
/// (0), so the copy is redundant there and cse drops it, which leaves `done`
/// defined only here - and reorg then fills the branch's delay slot with a copy
/// of that one move.
static void Actor00400_Fn09124(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    Actor100400Work* state2;
    s32              active;
    s32              done;

    work   = arg0->work;
    active = 0;
    if (work->field_640 >= 0xDAC) {
        goto set;
    }
    done = 0;
    if ((u32)(work->field_634 - 0x600) >= 0x400U) {
        gSceneCombatState.signals.bytes.enemyAlert = 1;
        Gp_ArmStateF0(1);
        active           = 1;
        state            = arg0->work;
        state->field_638 = 2;
        state->subState  = 0;
    }
set:
    done = active;
    if (done == 0) {
        if ((Actor00400_Fn02154(arg0) << 0x10) != 0) {
            Gp_ArmStateF0(1);
            return;
        }
        if (work->field_642 != 0) {
            state2            = arg0->work;
            state2->field_638 = 2;
            state2->subState  = 0;
        }
    }
}

static void Actor00400_Fn091F8(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[1])(Task*) = {
        Actor00400_Fn0A5B8,
    };

    if (Actor00400_Fn02154(arg0) == 0) {
        states[(s16)work->subState](arg0);
    }
}

static void Actor00400_Fn09260(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A680,
        Actor00400_Fn0A6B0,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[(s16)work->subState](arg0);
    }
}

static void Actor00400_Fn092D4(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A704,
        Actor00400_Fn0A760,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[(s16)work->subState](arg0);
    }
}

static void Actor00400_Fn09348(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A7F0,
        Actor00400_Fn0A82C,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[(s16)work->subState](arg0);
    }
}

static void Actor00400_Fn093BC(Task* task)
{
}

static void Actor00400_Fn093C4(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A880,
        Actor00400_Fn04900,
    };

    states[(s16)work->subState](arg0);
}

static void Actor00400_Fn09418(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A940,
        Actor00400_Fn04A1C,
    };

    states[(s16)work->subState](arg0);
}

static void Actor00400_Fn0946C(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A9F4,
        Actor00400_Fn0AA40,
    };

    states[(s16)work->subState](arg0);
}

static void Actor00400_Fn094C0(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    work->field_65B = 0;
    work->subState  = work->subState + 1;
}

static void Actor00400_Fn094DC(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              pan;
    s32              sound;

    work = arg0->work;
    if ((Actor00400_Fn02208(arg0) << 0x10) != 0) {
        if (work->animClip != 5) {
            work2              = arg0->work;
            work2->animBlend   = 0x10;
            work2->animStep    = 0x10;
            work2->animClip    = 5;
            work2->animRequest = 1;
        }
        work->field_63E = work->field_64E;
        Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        work->field_636 = 0;
        work->subState  = work->subState + 1;
        return;
    }
    work->field_660 = 1;
    if (!(work->field_630 & 0xF)) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040001;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}

static void Actor00400_Fn095D8(Task* arg0)
{
    s32              cond;
    Actor100400Work* work;
    Actor100400Work* work2;

    work            = arg0->work;
    work->field_660 = 1;
    work2           = arg0->work;
    if ((work2->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work2->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->subState = 1;
    }
}

static void Actor00400_Fn0962C(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    GfxCoord*        coord;

    work               = arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->field_574.vx = work->field_56C.vx;
    work->field_636    = 0;
    work->field_574.vy = work->field_56C.vy;
    work->field_574.vz = work->field_56C.vz;
    coord->coord.t[0] += ((s16)work->field_574.vx - coord->coord.t[0]) >> 2;
    coord->coord.t[2] += ((s16)work->field_574.vz - coord->coord.t[2]) >> 2;
    state              = arg0->work;
    state->animBlend   = 0xA;
    state->animStep    = 0x10;
    state->animClip    = 1;
    state->animRequest = 1;
    work->subState     = work->subState + 1;
}

static void Actor00400_Fn096C0(Task* arg0)
{
    s32              cond;
    Actor100400Work* work;

    work = arg0->work;
    if ((work->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->subState = work->subState + 1;
    }
}

#include "../../shared/diver_state7_enter.inc.c"

static void Actor00400_Fn097C8(Task* arg0)
{
    s32              cond;
    s32              pan;
    s32              sound;
    Actor100400Work* work;
    Actor100400Work* state;

    work = arg0->work;
    if (work->field_642 != 0) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    state = arg0->work;
    if ((state->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (state->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        state            = arg0->work;
        state->field_638 = 2;
        state->subState  = 0;
    }
}

static void Actor00400_Fn098A8(Task* arg0)
{
    Actor100400Work* work;

    work              = arg0->work;
    work->animBlend   = 8;
    work->animStep    = 0x10;
    work->animClip    = 0xE;
    work->animRequest = 1;
    work->field_63E   = (u16)work->field_64E + 0x64;
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
    work->field_610 = 0x64;
    work->field_636 = 0;
    work->field_664 = 1;
    work->subState  = work->subState + 1;
}

static void Actor00400_Fn09924(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    s32              cond;
    s32              mode;

    work = arg0->work;
    mode = work->field_642;
    work->field_636++;
    if (mode == 1) {
        state              = arg0->work;
        state->animBlend   = 2;
        state->animStep    = 0x10;
        state->animClip    = 0x12;
        state->animRequest = mode;
    } else {
        state = arg0->work;
        if ((state->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (state->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            state              = arg0->work;
            state->animBlend   = 8;
            state->animStep    = 0x10;
            state->animClip    = 0x10;
            state->animRequest = 1;
        }
    }
    if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
        work->field_610  = 0;
        work->field_664  = 4;
        state            = arg0->work;
        state->field_638 = 4;
        state->subState  = 0;
    }
}

static void Actor00400_Fn09A1C(Task* arg0)
{
    Actor100400Work* work;

    work            = arg0->work;
    work->field_660 = 0;
    work->field_636 = 0;
    work->field_63E = (u16)work->field_64E + 0x64;
    work->subState  = work->subState + 1;
}

static void Actor00400_Fn09A48(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;

    work               = arg0->work;
    work->field_65B    = 0;
    state              = arg0->work;
    state->animBlend   = 0xA;
    state->animStep    = 0x10;
    state->animClip    = 3;
    state->animRequest = 1;
    work->subState     = work->subState + 1;
}

static void Actor00400_Fn09A8C(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->field_65E == 2 || GameFlag_GetNibble(GAME_FLAG_SUBMARINE_TUNNEL_EVENT_SEEN) != 0) {
        work->subState = work->subState + 1;
    }
}

static void Actor00400_Fn09AE0(Task* arg0)
{
    Actor100400Work* work;
    GfxCoord*        coord;
    Actor100400Work* state;

    work               = arg0->work;
    coord              = arg0->extra.tmd->coords;
    coord->coord.t[0]  = 0x10E0;
    coord->coord.t[1]  = 0x178;
    work->field_63E    = 0x178;
    coord->coord.t[2]  = -0xDAC;
    work->field_554    = 0;
    work->field_556    = 0;
    work->field_558    = 0;
    state              = arg0->work;
    state->animStep    = 0x10;
    state->animClip    = 3;
    state->animRequest = 2;
    work->field_636    = 0;
    work->subState     = work->subState + 1;
}

static void Actor00400_Fn09B44(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->field_65E == 1) {
        work->subState = work->subState + 1;
    }
}

static void Actor00400_Fn09B74(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (++work->field_636 < 0x30) {
        diverStepForward(arg0, 0xA0, work->field_556);
        return;
    }
    work->subState++;
}

static void Actor00400_Fn09BDC(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->field_65E == 2) {
        work->field_638 = 0xB;
        work->subState  = 0;
    }
}

static void Actor00400_Fn09C04(Task* arg0)
{
    Actor100400Work* work;
    TaskFuncTable7   fns;

    work = arg0->work;
    fns  = Actor00400_D00178;
    fns.funcs[(s16)work->subState](arg0);
}

static void Actor00400_Fn09C84(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[1])(Task*) = {
        Actor00400_Fn0A034,
    };

    states[(s16)work->subState](arg0);
}

static void Actor00400_Fn09CCC(Task* arg0)
{
    Actor100400Work* work;
    GfxCoord*        coord;
    Actor100400Work* state;

    work               = arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->field_660    = 1;
    coord->coord.t[0]  = -0x6C0;
    coord->coord.t[1]  = 0x3E8;
    work->field_63E    = 0x3E8;
    coord->coord.t[2]  = -0xBB8;
    work->field_554    = 0;
    work->field_556    = 0x800;
    work->field_558    = 0;
    state              = arg0->work;
    state->animStep    = 0x10;
    state->animClip    = 3;
    state->animRequest = 2;
    work->field_636    = 0;
    work->subState     = work->subState + 1;
}

static void Actor00400_Fn09D3C(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (GameFlag_GetNibble(GAME_FLAG_0EB) == 1) {
        work->subState = 3;
    } else if (work->field_65E == 3) {
        work->subState = work->subState + 1;
    }
}

static void Actor00400_Fn09D98(Task* arg0)
{
    u16              count;
    s32              sound;
    s32              pan;
    Actor100400Work* work;

    work            = arg0->work;
    count           = work->field_636 + 1;
    work->field_636 = count;
    if ((s16)count < 0x30) {
        diverStepForward(arg0, 0x60, work->field_556);
        if (!(work->field_630 & 0xF)) {
            sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040001;
            pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
    } else {
        work->subState += 1;
    }
}

static void Actor00400_Fn09E70(Task* arg0)
{
    Actor100400Work* work;
    GfxCoord*        coord;
    Actor100400Work* state;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_65E == 4) {
        work->field_636    = 0;
        work->field_63E    = work->field_64E;
        coord->coord.t[0]  = -0x6C0;
        coord->coord.t[2]  = -0x2008;
        work->field_554    = 0;
        work->field_556    = 0;
        work->field_558    = 0;
        state              = arg0->work;
        state->animBlend   = 8;
        state->animStep    = 0x10;
        state->animClip    = 1;
        state->animRequest = 1;
        work->subState     = work->subState + 1;
    } else {
        work->field_636   = 0;
        coord->coord.t[0] = -0x6C0;
        coord->coord.t[2] = -0x2008;
        work->field_554   = 0;
        work->field_556   = 0;
        work->field_558   = 0;
        coord->coord.t[1] = 0x3E8;
        work->field_63E   = 0x3E8;
    }
}

static void Actor00400_Fn09F18(Task* arg0)
{
    u16              count;
    s32              sound;
    s32              pan;
    Actor100400Work* work;

    work            = arg0->work;
    count           = work->field_636 + 1;
    work->field_636 = count;
    if ((s16)count == 0x26) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54220006;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_636 == 0x30) {
        work->subState += 1;
    }
}

static void Actor00400_Fn09FDC(Task* arg0)
{
    Enemy*           obj;
    Actor100400Work* work;

    obj = arg0->spawnArg2.pointer;
    if (((Actor100400Work*)arg0->work)->field_65E == 5) {
        obj->node.state.parts.flags = 0;
        Actor00400_Fn02FF8(arg0);
        Gp_IncStateF0Ref(0);
        work            = arg0->work;
        work->field_638 = 4;
        work->subState  = 0;
    }
}

static void Actor00400_Fn0A034(Task* arg0)
{
    Enemy*           obj;
    Actor100400Work* work;

    obj = arg0->spawnArg2.pointer;
    if (((Actor100400Work*)arg0->work)->field_65E == 5) {
        obj->node.state.parts.flags = 0;
        Actor00400_Fn02FF8(arg0);
        Gp_IncStateF0Ref(0);
        work            = arg0->work;
        work->field_638 = 4;
        work->subState  = 0;
    }
}

#include "../../shared/coord_math_local_to_world.inc.c"

/// First kill-path state, entered the frame the marker task is spawned:
/// `Actor00400_Fn02D48` walks it afterwards and `diverStrikeTeardown` retires it.
///
/// `task->work` is the 0x64-byte `Actor100400MarkerWork` block
/// `Actor00400_SpawnMarker` allocated, and `task->extra` the `TmdObject`
/// whose `coords` is the coordinate the marker is drawn at. That coordinate is
/// re-parented to `gGfxViewCoord` here, and the object is linked to it with its
/// two `WorldCollisionContact` slots zeroed, so the state `Actor00400_Fn02D48` runs can report
/// what the marker collides with. `field_5A` is seeded with the negative span
/// the spawner's tip overshot by, and the object's draw scale with 0x100.
///
/// The `task->extra` walk is repeated for `work->obj.coord` rather than reusing
/// `coord`: the original re-reads it, which is what the second `lw` chain in
/// the target shows.
///
/// `coord` is assigned before `work` on purpose. sched1 emits each load where
/// its source order puts it, and that position is the quantity's `birth`:
/// writing `coord` second lands its `lw` one insn later, shortening its span
/// from 70 to 68 and raising its `QTY_CMP_PRI` from 1428 to 1470 — above the
/// task pointer's 1458 — so local-alloc hands the coordinate `$s1` and the task
/// pointer `$s2` instead of the reverse. See DECOMPILATION_LEARNINGS.md,
/// "A parameter competes in local-alloc on its raw span, not its doubled
/// `REG_LIVE_LENGTH`".
static void Actor00400_Fn0A190(Task* task)
{
    Actor100400MarkerWork* work;
    GfxCoord*              coord;

    coord                      = task->extra.tmd->coords;
    work                       = (Actor100400MarkerWork*)task->work;
    task->killCountdown        = 0;
    work->field_60             = 0;
    coord->parent              = &gGfxViewCoord;
    coord->composeStamp        = GRAPHICS_COORD_DIRTY;
    work->obj.key              = Gp_PackPair(Actor00400_D0FDC0, 1);
    work->obj.coord            = task->extra.tmd->coords;
    work->obj.context.contacts = work->recs;
    work->obj.pos.vx           = 0;
    work->obj.pos.vy           = 0;
    work->obj.pos.vz           = 0;
    work->obj.radius           = 0x100;
    work->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(work->recs, 2, 0);
    work->obj.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_UpdateCoord(coord);
    work->field_5A = -0x14;
    diverImpactBurst(coord, (u16)work->field_60, 0, 0x1300);
    task->state++;
}

#include "../../shared/diver_strike_teardown.inc.c"

static void Actor00400_Fn0A2F4(Task* arg0)
{
    Actor100400QuadWork* work;
    Enemy*               object;

    work   = (Actor100400QuadWork*)arg0->work;
    object = work->field_0;
    Actor00400_Fn03318(&work->vertices[0], &work->vertices[1],
                       &work->vertices[2], &work->vertices[3], work->intensity);
    if ((s16)object->hp <= 0) {
        arg0->state++;
    }
}

static void Actor00400_Fn0A364(Task* arg0)
{
    Actor100400QuadWork* work;
    u8                   intensity;

    work = (Actor100400QuadWork*)arg0->work;
    Actor00400_Fn03318(&work->vertices[0], &work->vertices[1],
                       &work->vertices[2], &work->vertices[3], work->intensity);
    intensity       = work->intensity - 1;
    work->intensity = intensity;
    if (intensity == 0) {
        taskKill(arg0);
    }
}

static void Actor00400_Fn0A3D4(Task* arg0)
{
    Actor100400Work* work;
    SVECTOR*         record;

    work   = arg0->work;
    record = (SVECTOR*)(work->field_64A * sizeof(SVECTOR) + (u32)work->field_608);
    if (record->pad != 0) {
        record->pad = 0;
    }
    work->field_636 = 0;
    work->field_638 = (u16)work->field_638 + 1;
}

static void Actor00400_Fn0A414(Task* arg0)
{
    Actor100400Work* work;
    u16              frame;

    work            = arg0->work;
    frame           = (u16)work->field_636 + 1;
    work->field_636 = frame;
    if ((s16)frame >= 0x12D) {
        enemyDestroy(arg0->spawnArg2.pointer, arg0);
    }
}

static void Actor00400_Fn0A468(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A510,
        Actor00400_Fn07400,
    };

    states[(s16)work->subState](arg0);
}

static void Actor00400_Fn0A4BC(Task* arg0)
{
    Actor100400Work* work                = arg0->work;
    void             (*states[2])(Task*) = {
        Actor00400_Fn0A57C,
        Actor00400_Fn07518,
    };

    states[(s16)work->subState](arg0);
}

static void Actor00400_Fn0A510(Task* arg0)
{
    Actor100400Work* state;
    Actor100400Work* work;
    u32              random;

    work               = arg0->work;
    random             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_63E    = work->field_64E;
    state              = arg0->work;
    state->animBlend   = 8;
    state->animStep    = ((random >> 16) & 3) + 3;
    state->animClip    = 0x10;
    state->animRequest = 1;
    gRandomLcgState    = random;
    work->field_636    = 0;
    work->subState++;
}

static void Actor00400_Fn0A57C(Task* arg0)
{
    Actor100400Work* work;

    work              = arg0->work;
    work->animBlend   = 2;
    work->animStep    = 0x10;
    work->animClip    = 0x12;
    work->animRequest = 1;
    work->field_636   = 0;
    work->subState++;
}

static void Actor00400_Fn0A5B8(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;

    work = arg0->work;
    if (work->field_640 < 0x4E2) {
        work->field_638                  = 5;
        work->subState                   = 0;
        work->field_614[work->field_65A] = work->field_638;
        if (work->field_614[0] == work->field_614[1] &&
            work->field_614[0] == work->field_614[2] &&
            work->field_614[0] == 5) {
            state                            = arg0->work;
            state->field_638                 = 4;
            state->subState                  = 0;
            work->field_614[work->field_65A] = work->field_638;
        }
    } else {
        work->field_638                  = 4;
        work->subState                   = 0;
        work->field_614[work->field_65A] = work->field_638;
    }
    work->field_65A++;
    if (work->field_65A >= 3U) {
        work->field_65A = 0;
    }
}

static void Actor00400_Fn0A680(Task* arg0)
{
    Actor100400Work* work;

    work              = arg0->work;
    work->animBlend   = 6;
    work->animStep    = 0x10;
    work->animClip    = 6;
    work->animRequest = 1;
    work->subState++;
}

static void Actor00400_Fn0A6B0(Task* arg0)
{
    Actor100400Work* work;
    s32              cond;

    work = arg0->work;
    if ((work->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = arg0->work;
        work->field_638 = 2;
        work->subState  = 0;
    }
}

static void Actor00400_Fn0A704(Task* arg0)
{
    Actor100400Work* work;

    work = arg0->work;
    if (work->field_640 < 0x3B4) {
        work->field_638 = 2;
        work->subState  = 0;
        return;
    }
    Actor00400_Fn00C84(arg0);
    work->subState++;
}

static void Actor00400_Fn0A760(Task* arg0)
{
    Actor100400Work* work;
    s32              cond;

    work = arg0->work;
    if (work->field_640 < 0x3B4) {
        work->field_638 = 2;
        work->subState  = 0;
        return;
    }
    Actor00400_Fn00C84(arg0);
    work = arg0->work;
    if ((work->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = arg0->work;
        work->field_638 = 2;
        work->subState  = 0;
    }
}

static void Actor00400_Fn0A7F0(Task* arg0)
{
    Actor100400Work* work;

    work              = arg0->work;
    work->animBlend   = 6;
    work->animStep    = 0x10;
    work->animClip    = 9;
    work->animRequest = 1;
    work->field_646   = 0x18;
    work->subState++;
}

static void Actor00400_Fn0A82C(Task* arg0)
{
    Actor100400Work* work;
    s32              cond;

    work = arg0->work;
    if ((work->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = arg0->work;
        work->field_638 = 2;
        work->subState  = 0;
    }
}

static void Actor00400_Fn0A880(Task* arg0)
{
    s32              sound;
    s32              pan;
    Actor100400Work* work;
    Actor100400Work* state;

    work  = arg0->work;
    sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    state              = arg0->work;
    state->animBlend   = 6;
    state->animStep    = 0x10;
    state->animClip    = 0xC;
    state->animRequest = 1;
    work->subState++;
}

static void Actor00400_Fn0A940(Task* arg0)
{
    s32              sound;
    s32              pan;
    Actor100400Work* work;

    work              = arg0->work;
    work->animBlend   = 4;
    work->animStep    = 0x10;
    work->animClip    = 0xD;
    work->animRequest = 1;
    sound             = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
    pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->subState++;
}

static void Actor00400_Fn0A9F4(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;

    work               = arg0->work;
    work->field_665    = 1;
    state              = arg0->work;
    state->animBlend   = 8;
    state->animStep    = 0x10;
    state->animClip    = 0xF;
    state->animRequest = 1;
    work->field_610    = 0x64;
    work->field_636    = 0;
    work->subState++;
}

static void Actor00400_Fn0AA40(Task* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    s32              cond;

    work = arg0->work;
    work->field_636++;
    state = arg0->work;
    if ((state->flags_62C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (state->flags_62C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        state              = arg0->work;
        state->animBlend   = 8;
        state->animStep    = 0x10;
        state->animClip    = 0x11;
        state->animRequest = 1;
    }
    if (Gp_TickObjFlag2(arg0->spawnArg2.pointer)) {
        work->field_610  = 0;
        work->field_665  = 0;
        state            = arg0->work;
        state->field_638 = 2;
        state->subState  = 0;
    }
}
