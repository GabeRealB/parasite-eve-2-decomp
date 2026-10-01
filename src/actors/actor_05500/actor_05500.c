#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "actors/actors_shared_80135c4c.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/object_fields.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
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
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#define MAGGOT_CATERPILLAR_KIND CATERPILLAR
#include "../../shared/maggot_caterpillar.h"

extern void* D_80067704[1];

extern TmdSource     gMaggotCaterpillarHuskModel;
extern DamageAttack  gMaggotCaterpillarAttacks[6];
extern EnemyParams   gMaggotCaterpillarParams;
extern u16           gMaggotCaterpillarIdleDelay[];
extern u16           gMaggotCaterpillarRoamDelay[];
extern u16           gMaggotCaterpillarDropSpeed[];
extern s16           gMaggotCaterpillarLeapInDelay[];
extern SVECTOR       gMaggotCaterpillarLeapInSpots[];
extern s16           gMaggotCaterpillarLeapInYaws[];
extern s16           gMaggotCaterpillarDropInDelay[];
extern s16           gMaggotCaterpillarDropInSpeed[];
extern SVECTOR       gMaggotCaterpillarDropInSpots[];
extern s16           gMaggotCaterpillarDropInYaws[];
extern s16           gMaggotCaterpillarAnimBlend[];
extern s16           gMaggotCaterpillarSprayTail;
extern s16           gMaggotCaterpillarPounceLead;
extern s16           gMaggotCaterpillarPounceStride[][2];
extern s16           gMaggotCaterpillarReboundStride[][2];
extern ActorSpriteUv gMaggotCaterpillarPuffCells[];
extern s16           gMaggotCaterpillarPuffRadius[];
extern TaskDesc      gMaggotCaterpillarBodyTask;
extern AnimationSet* gMaggotCaterpillarAnimSets[15];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

extern AnimationSet Actor05500_D061A0;
extern AnimationSet Actor05500_D06454;
extern AnimationSet Actor05500_D068D8;
extern AnimationSet Actor05500_D06E1C;
extern AnimationSet Actor05500_D074E4;
extern AnimationSet Actor05500_D077AC;
extern AnimationSet Actor05500_D0793C;
extern AnimationSet Actor05500_D07B50;
extern AnimationSet Actor05500_D07E90;
extern AnimationSet Actor05500_D08148;
extern AnimationSet Actor05500_D083F4;
extern AnimationSet Actor05500_D08780;
extern AnimationSet Actor05500_D08930;
extern TmdSource    Actor05500_D05774;

TmdBone Actor05500_D03FE4[8] = {
#include "assets/caterpillar_maggot_body_skeleton.inc"
};

u32 Actor05500_D04104[8] = {
#include "assets/caterpillar_maggot_body_partVerts.inc"
};

SVECTOR Actor05500_D04124[101] = {
#include "assets/caterpillar_maggot_body_verts.inc"
};

SVECTOR Actor05500_D0444C[107] = {
#include "assets/caterpillar_maggot_body_normals.inc"
};

u32 Actor05500_D047A4[1012] = {
#include "assets/caterpillar_maggot_body_stream.inc"
};

TmdSource Actor05500_D05774 = {
    0,
    5400,
    1872,
    8,
    Actor05500_D04104,
    Actor05500_D04124,
    Actor05500_D0444C,
    Actor05500_D03FE4,
    Actor05500_D047A4,
};

TmdBone Actor05500_D05798[1] = {
#include "assets/caterpillar_maggot_burst_head_skeleton.inc"
};

u32 Actor05500_D057BC[1] = {
#include "assets/caterpillar_maggot_burst_head_partVerts.inc"
};

SVECTOR Actor05500_D057C0[40] = {
#include "assets/caterpillar_maggot_burst_head_verts.inc"
};

SVECTOR Actor05500_D05900[40] = {
#include "assets/caterpillar_maggot_burst_head_normals.inc"
};

u32 Actor05500_D05A40[310] = {
#include "assets/caterpillar_maggot_burst_head_stream.inc"
};

TmdSource gMaggotCaterpillarHuskModel = {
    0,
    2144,
    0,
    1,
    Actor05500_D057BC,
    Actor05500_D057C0,
    Actor05500_D05900,
    Actor05500_D05798,
    Actor05500_D05A40,
};

AnimationPackedPose Actor05500_D05F3C[10] = {
#include "assets/actor_105500_animation_061A0_bank1.inc"
};

AnimationPackedRotation Actor05500_D05FB4[45] = {
#include "assets/actor_105500_animation_061A0_bank4.inc"
};

AnimationRecord Actor05500_D06068[74] = {
#include "assets/actor_105500_animation_061A0_records.inc"
};

u16 Actor05500_D06190[8] = {
#include "assets/actor_105500_animation_061A0_indices.inc"
};

AnimationSet Actor05500_D061A0 = {
    Actor05500_D06068,
    Actor05500_D06190,
    { NULL, Actor05500_D05F3C, NULL, NULL, Actor05500_D05FB4, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D061C8[11] = {
#include "assets/actor_105500_animation_06454_bank1.inc"
};

AnimationPackedRotation Actor05500_D0624C[44] = {
#include "assets/actor_105500_animation_06454_bank4.inc"
};

AnimationRecord Actor05500_D062FC[82] = {
#include "assets/actor_105500_animation_06454_records.inc"
};

u16 Actor05500_D06444[8] = {
#include "assets/actor_105500_animation_06454_indices.inc"
};

AnimationSet Actor05500_D06454 = {
    Actor05500_D062FC,
    Actor05500_D06444,
    { NULL, Actor05500_D061C8, NULL, NULL, Actor05500_D0624C, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D0647C[13] = {
#include "assets/actor_105500_animation_068D8_bank1.inc"
};

AnimationPackedRotation Actor05500_D06518[96] = {
#include "assets/actor_105500_animation_068D8_bank4.inc"
};

AnimationRecord Actor05500_D06698[140] = {
#include "assets/actor_105500_animation_068D8_records.inc"
};

u16 Actor05500_D068C8[8] = {
#include "assets/actor_105500_animation_068D8_indices.inc"
};

AnimationSet Actor05500_D068D8 = {
    Actor05500_D06698,
    Actor05500_D068C8,
    { NULL, Actor05500_D0647C, NULL, NULL, Actor05500_D06518, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D06900[31] = {
#include "assets/actor_105500_animation_06E1C_bank1.inc"
};

AnimationPackedRotation Actor05500_D06A74[82] = {
#include "assets/actor_105500_animation_06E1C_bank4.inc"
};

AnimationRecord Actor05500_D06BBC[148] = {
#include "assets/actor_105500_animation_06E1C_records.inc"
};

u16 Actor05500_D06E0C[8] = {
#include "assets/actor_105500_animation_06E1C_indices.inc"
};

AnimationSet Actor05500_D06E1C = {
    Actor05500_D06BBC,
    Actor05500_D06E0C,
    { NULL, Actor05500_D06900, NULL, NULL, Actor05500_D06A74, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D06E44[27] = {
#include "assets/actor_105500_animation_074E4_bank1.inc"
};

AnimationPackedRotation Actor05500_D06F88[148] = {
#include "assets/actor_105500_animation_074E4_bank4.inc"
};

AnimationRecord Actor05500_D071D8[191] = {
#include "assets/actor_105500_animation_074E4_records.inc"
};

u16 Actor05500_D074D4[8] = {
#include "assets/actor_105500_animation_074E4_indices.inc"
};

AnimationSet Actor05500_D074E4 = {
    Actor05500_D071D8,
    Actor05500_D074D4,
    { NULL, Actor05500_D06E44, NULL, NULL, Actor05500_D06F88, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D0750C[11] = {
#include "assets/actor_105500_animation_077AC_bank1.inc"
};

AnimationPackedRotation Actor05500_D07590[51] = {
#include "assets/actor_105500_animation_077AC_bank4.inc"
};

AnimationRecord Actor05500_D0765C[80] = {
#include "assets/actor_105500_animation_077AC_records.inc"
};

u16 Actor05500_D0779C[8] = {
#include "assets/actor_105500_animation_077AC_indices.inc"
};

AnimationSet Actor05500_D077AC = {
    Actor05500_D0765C,
    Actor05500_D0779C,
    { NULL, Actor05500_D0750C, NULL, NULL, Actor05500_D07590, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D077D4[6] = {
#include "assets/actor_105500_animation_0793C_bank1.inc"
};

AnimationPackedRotation Actor05500_D0781C[26] = {
#include "assets/actor_105500_animation_0793C_bank4.inc"
};

AnimationRecord Actor05500_D07884[42] = {
#include "assets/actor_105500_animation_0793C_records.inc"
};

u16 Actor05500_D0792C[8] = {
#include "assets/actor_105500_animation_0793C_indices.inc"
};

AnimationSet Actor05500_D0793C = {
    Actor05500_D07884,
    Actor05500_D0792C,
    { NULL, Actor05500_D077D4, NULL, NULL, Actor05500_D0781C, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D07964[9] = {
#include "assets/actor_105500_animation_07B50_bank1.inc"
};

AnimationPackedRotation Actor05500_D079D0[34] = {
#include "assets/actor_105500_animation_07B50_bank4.inc"
};

AnimationRecord Actor05500_D07A58[58] = {
#include "assets/actor_105500_animation_07B50_records.inc"
};

u16 Actor05500_D07B40[8] = {
#include "assets/actor_105500_animation_07B50_indices.inc"
};

AnimationSet Actor05500_D07B50 = {
    Actor05500_D07A58,
    Actor05500_D07B40,
    { NULL, Actor05500_D07964, NULL, NULL, Actor05500_D079D0, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D07B78[13] = {
#include "assets/actor_105500_animation_07E90_bank1.inc"
};

AnimationPackedRotation Actor05500_D07C14[58] = {
#include "assets/actor_105500_animation_07E90_bank4.inc"
};

AnimationRecord Actor05500_D07CFC[97] = {
#include "assets/actor_105500_animation_07E90_records.inc"
};

u16 Actor05500_D07E80[8] = {
#include "assets/actor_105500_animation_07E90_indices.inc"
};

AnimationSet Actor05500_D07E90 = {
    Actor05500_D07CFC,
    Actor05500_D07E80,
    { NULL, Actor05500_D07B78, NULL, NULL, Actor05500_D07C14, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D07EB8[9] = {
#include "assets/actor_105500_animation_08148_bank1.inc"
};

AnimationPackedRotation Actor05500_D07F24[53] = {
#include "assets/actor_105500_animation_08148_bank4.inc"
};

AnimationRecord Actor05500_D07FF8[80] = {
#include "assets/actor_105500_animation_08148_records.inc"
};

u16 Actor05500_D08138[8] = {
#include "assets/actor_105500_animation_08148_indices.inc"
};

AnimationSet Actor05500_D08148 = {
    Actor05500_D07FF8,
    Actor05500_D08138,
    { NULL, Actor05500_D07EB8, NULL, NULL, Actor05500_D07F24, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D08170[9] = {
#include "assets/actor_105500_animation_083F4_bank1.inc"
};

AnimationPackedRotation Actor05500_D081DC[49] = {
#include "assets/actor_105500_animation_083F4_bank4.inc"
};

AnimationRecord Actor05500_D082A0[81] = {
#include "assets/actor_105500_animation_083F4_records.inc"
};

u16 Actor05500_D083E4[8] = {
#include "assets/actor_105500_animation_083F4_indices.inc"
};

AnimationSet Actor05500_D083F4 = {
    Actor05500_D082A0,
    Actor05500_D083E4,
    { NULL, Actor05500_D08170, NULL, NULL, Actor05500_D081DC, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D0841C[20] = {
#include "assets/actor_105500_animation_08780_bank1.inc"
};

AnimationPackedRotation Actor05500_D0850C[61] = {
#include "assets/actor_105500_animation_08780_bank4.inc"
};

AnimationRecord Actor05500_D08600[92] = {
#include "assets/actor_105500_animation_08780_records.inc"
};

u16 Actor05500_D08770[8] = {
#include "assets/actor_105500_animation_08780_indices.inc"
};

AnimationSet Actor05500_D08780 = {
    Actor05500_D08600,
    Actor05500_D08770,
    { NULL, Actor05500_D0841C, NULL, NULL, Actor05500_D0850C, NULL, NULL, NULL },
};

AnimationPackedPose Actor05500_D087A8[5] = {
#include "assets/actor_105500_animation_08930_bank1.inc"
};

AnimationPackedRotation Actor05500_D087E4[19] = {
#include "assets/actor_105500_animation_08930_bank4.inc"
};

AnimationRecord Actor05500_D08830[60] = {
#include "assets/actor_105500_animation_08930_records.inc"
};

u16 Actor05500_D08920[8] = {
#include "assets/actor_105500_animation_08930_indices.inc"
};

AnimationSet Actor05500_D08930 = {
    Actor05500_D08830,
    Actor05500_D08920,
    { NULL, Actor05500_D087A8, NULL, NULL, Actor05500_D087E4, NULL, NULL, NULL },
};

DamageAttack gMaggotCaterpillarAttacks[6] = {
    { 10, 0 },
    { 16, 0 },
    { 8, 1 },
    { 12, 6 },
    { 18, 6 },
    { 8, 0 },
};

EnemyParams gMaggotCaterpillarParams = { gMaggotCaterpillarAttacks, 80, 6, 28, 1, 100, 20, 100, 0 };

u16 gMaggotCaterpillarIdleDelay[8] = {
    40,
    35,
    30,
    25,
    20,
    15,
    10,
    5,
};

u16 gMaggotCaterpillarRoamDelay[8] = {
    1600,
    1800,
    2000,
    2100,
    2200,
    2300,
    2400,
    2500,
};

u16 gMaggotCaterpillarDropSpeed[8] = {
    100,
    110,
    120,
    130,
    140,
    150,
    160,
    170,
};

s16 gMaggotCaterpillarLeapInDelay[4] = {
    0,
    2,
    4,
    6,
};

SVECTOR gMaggotCaterpillarLeapInSpots[4] = {
    { -3000, 0, -600, 0 },
    { -3000, 0, -1600, 0 },
    { -2000, 0, -600, 0 },
    { -2000, 0, -1600, 0 },
};

s16 gMaggotCaterpillarLeapInYaws[4] = {
    0,
    1024,
    2048,
    3072,
};

s16 gMaggotCaterpillarDropInDelay[4] = {
    5,
    10,
    15,
    20,
};

s16 gMaggotCaterpillarDropInSpeed[4] = {
    100,
    100,
    100,
    100,
};

SVECTOR gMaggotCaterpillarDropInSpots[4] = {
    { -7000, 0, -7600, 0 },
    { -5600, 0, -7500, 0 },
    { -4700, 0, -6500, 0 },
    { -5600, 0, -5600, 0 },
};

s16 gMaggotCaterpillarDropInYaws[4] = {
    600,
    1024,
    2048,
    1800,
};

s16 gMaggotCaterpillarAnimBlend[3] = {
    0,
    10,
    0,
};

s16 gMaggotCaterpillarSprayTail = 8;

s16 gMaggotCaterpillarPounceLead = 8;

s32 Actor05500_D08A24[5] = {
    0,
    0,
    0,
    0,
    4,
};

s16 gMaggotCaterpillarPounceStride[9][2] = {
    { 20, 0 },
    { 21, 180 },
    { 22, 300 },
    { 30, 216 },
    { 35, 134 },
    { 38, 13 },
    { 40, 10 },
    { 43, 13 },
    { 50, 3 },
};

s16 gMaggotCaterpillarReboundStride[9][2] = {
    { 2, 250 },
    { 3, 10 },
    { 5, -30 },
    { 7, -55 },
    { 9, -80 },
    { 10, -300 },
    { 13, -97 },
    { 15, -55 },
    { 18, -10 },
};

ActorSpriteUv gMaggotCaterpillarPuffCells[8] = {
    { 0, 0, 0, 0 },
    { 32, 0, 0, 0 },
    { 64, 0, 0, 0 },
    { 96, 0, 0, 0 },
    { 0, 0, 32, 0 },
    { 32, 0, 32, 0 },
    { 64, 0, 32, 0 },
    { 96, 0, 32, 0 },
};

s16 gMaggotCaterpillarPuffRadius[14] = {
    8,
    12,
    16,
    20,
    24,
    28,
    32,
    32,
    32,
    36,
    36,
    36,
    40,
    0,
};

TaskDesc gMaggotCaterpillarBodyTask = { { { TASK_BODY_TMD, 96 } }, maggotCaterpillarTask, { .model = &Actor05500_D05774 } };

TaskDesc Actor05500_D08AC8 = { { { TASK_BODY_COORD, 96 } }, maggotCaterpillarPuffTask, { .value = 0 } };

AnimationSet* gMaggotCaterpillarAnimSets[15] = {
    NULL,
    &Actor05500_D061A0,
    &Actor05500_D06454,
    &Actor05500_D068D8,
    &Actor05500_D06E1C,
    &Actor05500_D074E4,
    &Actor05500_D077AC,
    &Actor05500_D0793C,
    NULL,
    &Actor05500_D07B50,
    &Actor05500_D07E90,
    &Actor05500_D08148,
    &Actor05500_D083F4,
    &Actor05500_D08780,
    &Actor05500_D08930,
};

#include "../../shared/maggot_caterpillar_resolve_contacts.inc.c"

/// State handlers of the task `maggotCaterpillarPuffTask` dispatches, indexed by the
/// task's state: `maggotCaterpillarPuffSetup` sets up its collision object,
/// `maggotCaterpillarPuffTick` runs it, and `Gp_DestroyEnemy` tears it down.
static const GpEnemyTaskFuncTable3 gMaggotCaterpillarPuffStates = {
    {
        maggotCaterpillarPuffSetup,
        maggotCaterpillarPuffTick,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the task `maggotCaterpillarTask` dispatches, indexed by the
/// task's state: `maggotCaterpillarSpawn` allocates and sets up the work block,
/// `maggotCaterpillarTick` runs the actor, and `maggotCaterpillarDyingState` handles its
/// last state.
static const GpEnemyTaskFuncTable3 gMaggotCaterpillarStates = {
    {
        maggotCaterpillarSpawn,
        maggotCaterpillarTick,
        maggotCaterpillarDyingState,
    },
};

#include "../../shared/maggot_caterpillar_wait_state.inc.c"

#include "../../shared/maggot_caterpillar_aim_state.inc.c"

#include "../../shared/maggot_caterpillar_ambush_state.inc.c"

#include "../../shared/maggot_caterpillar_roam_state.inc.c"

#include "../../shared/maggot_caterpillar_spray.inc.c"

#include "../../shared/maggot_caterpillar_pounce.inc.c"

#include "../../shared/maggot_caterpillar_hurt.inc.c"

#include "../../shared/maggot_caterpillar_entrance.inc.c"

#include "../../shared/maggot_caterpillar_burn.inc.c"

#include "../../shared/maggot_caterpillar_turn.inc.c"

#include "../../shared/maggot_caterpillar_inlines.inc.c"

#include "../../shared/maggot_caterpillar_dying.inc.c"

#include "../../shared/maggot_caterpillar_puff_tick.inc.c"

#include "../../shared/maggot_caterpillar_draw_puff.inc.c"

#include "../../shared/maggot_caterpillar_draw_thread.inc.c"

#include "../../shared/maggot_caterpillar_spawn.inc.c"

#include "../../shared/maggot_caterpillar_tick.inc.c"

#include "../../shared/maggot_caterpillar_status.inc.c"

#include "../../shared/maggot_caterpillar_run_behaviour.inc.c"

#include "../../shared/maggot_caterpillar_stun.inc.c"

#include "../../shared/maggot_caterpillar_move.inc.c"

#include "../../shared/maggot_caterpillar_tick_anim.inc.c"

#include "../../shared/maggot_caterpillar_update_color.inc.c"

#include "../../shared/maggot_caterpillar_shadow.inc.c"

#include "../../shared/maggot_caterpillar_squash.inc.c"

#include "../../shared/maggot_caterpillar_husk.inc.c"

#include "../../shared/maggot_caterpillar_shrink_node.inc.c"

#include "../../shared/maggot_caterpillar_puff_task.inc.c"

#include "../../shared/maggot_caterpillar_puff_setup.inc.c"

#include "../../shared/maggot_caterpillar_task.inc.c"
