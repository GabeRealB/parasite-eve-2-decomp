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
#define MAGGOT_CATERPILLAR_KIND MAGGOT
#include "../../shared/maggot_caterpillar.h"

extern ActorSpriteUv gMaggotCaterpillarPuffCells[];
extern s16           gMaggotCaterpillarPuffRadius[];

extern EnemyParams   gMaggotCaterpillarParams;
extern SVECTOR       gMaggotCaterpillarLeapInSpots[];
extern s16           gMaggotCaterpillarLeapInYaws[];
extern SVECTOR       gMaggotCaterpillarDropInSpots[];
extern s16           gMaggotCaterpillarDropInYaws[];
extern TaskDesc      gMaggotCaterpillarBodyTask;
extern AnimationSet* gMaggotCaterpillarAnimSets[15];
extern u16           gMaggotCaterpillarIdleDelay[];
extern u16           gMaggotCaterpillarRoamDelay[];
extern u16           gMaggotCaterpillarDropSpeed[];
extern s16           gMaggotCaterpillarLeapInDelay[];
extern s16           gMaggotCaterpillarDropInDelay[];
extern s16           gMaggotCaterpillarDropInSpeed[];
extern s16           gMaggotCaterpillarSprayTail;
extern s16           gMaggotCaterpillarPounceLead;
extern s16           gMaggotCaterpillarPounceStride[][2];
extern s16           gMaggotCaterpillarReboundStride[][2];
extern DamageAttack  gMaggotCaterpillarAttacks[6];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/* Animation id -> slot blend value table in this overlay's own data. */
extern s16 gMaggotCaterpillarAnimBlend[];

/* `D_80067704` is the third word of a `D_800676A8` record: it selects the model
 * stream the next `Gp_SpawnEff` uses for the effect's own `TmdObject`. Declared
 * as a one-element array so GCC 2.8.1 cannot treat the store as non-aliasing
 * with the struct traffic that follows and sink it past the loads. */
extern void* D_80067704[1];

/* Model stream in this overlay's own data. */
extern TmdSource gMaggotCaterpillarHuskModel;

extern AnimationSet Actor02600_D06198;
extern AnimationSet Actor02600_D0644C;
extern AnimationSet Actor02600_D068D0;
extern AnimationSet Actor02600_D06E14;
extern AnimationSet Actor02600_D074DC;
extern AnimationSet Actor02600_D077A4;
extern AnimationSet Actor02600_D07934;
extern AnimationSet Actor02600_D07B48;
extern AnimationSet Actor02600_D07E88;
extern AnimationSet Actor02600_D08140;
extern AnimationSet Actor02600_D083EC;
extern AnimationSet Actor02600_D08778;
extern AnimationSet Actor02600_D08928;
extern TmdSource    Actor02600_D0576C;

TmdBone Actor02600_D03FDC[8] = {
#include "assets/caterpillar_maggot_body_skeleton.inc"
};

u32 Actor02600_D040FC[8] = {
#include "assets/caterpillar_maggot_body_partVerts.inc"
};

SVECTOR Actor02600_D0411C[101] = {
#include "assets/caterpillar_maggot_body_verts.inc"
};

SVECTOR Actor02600_D04444[107] = {
#include "assets/caterpillar_maggot_body_normals.inc"
};

u32 Actor02600_D0479C[1012] = {
#include "assets/caterpillar_maggot_body_stream.inc"
};

TmdSource Actor02600_D0576C = {
    0,
    5400,
    1872,
    8,
    Actor02600_D040FC,
    Actor02600_D0411C,
    Actor02600_D04444,
    Actor02600_D03FDC,
    Actor02600_D0479C,
};

TmdBone Actor02600_D05790[1] = {
#include "assets/caterpillar_maggot_burst_head_skeleton.inc"
};

u32 Actor02600_D057B4[1] = {
#include "assets/caterpillar_maggot_burst_head_partVerts.inc"
};

SVECTOR Actor02600_D057B8[40] = {
#include "assets/caterpillar_maggot_burst_head_verts.inc"
};

SVECTOR Actor02600_D058F8[40] = {
#include "assets/caterpillar_maggot_burst_head_normals.inc"
};

u32 Actor02600_D05A38[310] = {
#include "assets/caterpillar_maggot_burst_head_stream.inc"
};

TmdSource gMaggotCaterpillarHuskModel = {
    0,
    2144,
    0,
    1,
    Actor02600_D057B4,
    Actor02600_D057B8,
    Actor02600_D058F8,
    Actor02600_D05790,
    Actor02600_D05A38,
};

AnimationPackedPose Actor02600_D05F34[10] = {
#include "assets/actor_102600_animation_06198_bank1.inc"
};

AnimationPackedRotation Actor02600_D05FAC[45] = {
#include "assets/actor_102600_animation_06198_bank4.inc"
};

AnimationRecord Actor02600_D06060[74] = {
#include "assets/actor_102600_animation_06198_records.inc"
};

u16 Actor02600_D06188[8] = {
#include "assets/actor_102600_animation_06198_indices.inc"
};

AnimationSet Actor02600_D06198 = {
    Actor02600_D06060,
    Actor02600_D06188,
    { NULL, Actor02600_D05F34, NULL, NULL, Actor02600_D05FAC, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D061C0[11] = {
#include "assets/actor_102600_animation_0644C_bank1.inc"
};

AnimationPackedRotation Actor02600_D06244[44] = {
#include "assets/actor_102600_animation_0644C_bank4.inc"
};

AnimationRecord Actor02600_D062F4[82] = {
#include "assets/actor_102600_animation_0644C_records.inc"
};

u16 Actor02600_D0643C[8] = {
#include "assets/actor_102600_animation_0644C_indices.inc"
};

AnimationSet Actor02600_D0644C = {
    Actor02600_D062F4,
    Actor02600_D0643C,
    { NULL, Actor02600_D061C0, NULL, NULL, Actor02600_D06244, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D06474[13] = {
#include "assets/actor_102600_animation_068D0_bank1.inc"
};

AnimationPackedRotation Actor02600_D06510[96] = {
#include "assets/actor_102600_animation_068D0_bank4.inc"
};

AnimationRecord Actor02600_D06690[140] = {
#include "assets/actor_102600_animation_068D0_records.inc"
};

u16 Actor02600_D068C0[8] = {
#include "assets/actor_102600_animation_068D0_indices.inc"
};

AnimationSet Actor02600_D068D0 = {
    Actor02600_D06690,
    Actor02600_D068C0,
    { NULL, Actor02600_D06474, NULL, NULL, Actor02600_D06510, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D068F8[31] = {
#include "assets/actor_102600_animation_06E14_bank1.inc"
};

AnimationPackedRotation Actor02600_D06A6C[82] = {
#include "assets/actor_102600_animation_06E14_bank4.inc"
};

AnimationRecord Actor02600_D06BB4[148] = {
#include "assets/actor_102600_animation_06E14_records.inc"
};

u16 Actor02600_D06E04[8] = {
#include "assets/actor_102600_animation_06E14_indices.inc"
};

AnimationSet Actor02600_D06E14 = {
    Actor02600_D06BB4,
    Actor02600_D06E04,
    { NULL, Actor02600_D068F8, NULL, NULL, Actor02600_D06A6C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D06E3C[27] = {
#include "assets/actor_102600_animation_074DC_bank1.inc"
};

AnimationPackedRotation Actor02600_D06F80[148] = {
#include "assets/actor_102600_animation_074DC_bank4.inc"
};

AnimationRecord Actor02600_D071D0[191] = {
#include "assets/actor_102600_animation_074DC_records.inc"
};

u16 Actor02600_D074CC[8] = {
#include "assets/actor_102600_animation_074DC_indices.inc"
};

AnimationSet Actor02600_D074DC = {
    Actor02600_D071D0,
    Actor02600_D074CC,
    { NULL, Actor02600_D06E3C, NULL, NULL, Actor02600_D06F80, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D07504[11] = {
#include "assets/actor_102600_animation_077A4_bank1.inc"
};

AnimationPackedRotation Actor02600_D07588[51] = {
#include "assets/actor_102600_animation_077A4_bank4.inc"
};

AnimationRecord Actor02600_D07654[80] = {
#include "assets/actor_102600_animation_077A4_records.inc"
};

u16 Actor02600_D07794[8] = {
#include "assets/actor_102600_animation_077A4_indices.inc"
};

AnimationSet Actor02600_D077A4 = {
    Actor02600_D07654,
    Actor02600_D07794,
    { NULL, Actor02600_D07504, NULL, NULL, Actor02600_D07588, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D077CC[6] = {
#include "assets/actor_102600_animation_07934_bank1.inc"
};

AnimationPackedRotation Actor02600_D07814[26] = {
#include "assets/actor_102600_animation_07934_bank4.inc"
};

AnimationRecord Actor02600_D0787C[42] = {
#include "assets/actor_102600_animation_07934_records.inc"
};

u16 Actor02600_D07924[8] = {
#include "assets/actor_102600_animation_07934_indices.inc"
};

AnimationSet Actor02600_D07934 = {
    Actor02600_D0787C,
    Actor02600_D07924,
    { NULL, Actor02600_D077CC, NULL, NULL, Actor02600_D07814, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D0795C[9] = {
#include "assets/actor_102600_animation_07B48_bank1.inc"
};

AnimationPackedRotation Actor02600_D079C8[34] = {
#include "assets/actor_102600_animation_07B48_bank4.inc"
};

AnimationRecord Actor02600_D07A50[58] = {
#include "assets/actor_102600_animation_07B48_records.inc"
};

u16 Actor02600_D07B38[8] = {
#include "assets/actor_102600_animation_07B48_indices.inc"
};

AnimationSet Actor02600_D07B48 = {
    Actor02600_D07A50,
    Actor02600_D07B38,
    { NULL, Actor02600_D0795C, NULL, NULL, Actor02600_D079C8, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D07B70[13] = {
#include "assets/actor_102600_animation_07E88_bank1.inc"
};

AnimationPackedRotation Actor02600_D07C0C[58] = {
#include "assets/actor_102600_animation_07E88_bank4.inc"
};

AnimationRecord Actor02600_D07CF4[97] = {
#include "assets/actor_102600_animation_07E88_records.inc"
};

u16 Actor02600_D07E78[8] = {
#include "assets/actor_102600_animation_07E88_indices.inc"
};

AnimationSet Actor02600_D07E88 = {
    Actor02600_D07CF4,
    Actor02600_D07E78,
    { NULL, Actor02600_D07B70, NULL, NULL, Actor02600_D07C0C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D07EB0[9] = {
#include "assets/actor_102600_animation_08140_bank1.inc"
};

AnimationPackedRotation Actor02600_D07F1C[53] = {
#include "assets/actor_102600_animation_08140_bank4.inc"
};

AnimationRecord Actor02600_D07FF0[80] = {
#include "assets/actor_102600_animation_08140_records.inc"
};

u16 Actor02600_D08130[8] = {
#include "assets/actor_102600_animation_08140_indices.inc"
};

AnimationSet Actor02600_D08140 = {
    Actor02600_D07FF0,
    Actor02600_D08130,
    { NULL, Actor02600_D07EB0, NULL, NULL, Actor02600_D07F1C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D08168[9] = {
#include "assets/actor_102600_animation_083EC_bank1.inc"
};

AnimationPackedRotation Actor02600_D081D4[49] = {
#include "assets/actor_102600_animation_083EC_bank4.inc"
};

AnimationRecord Actor02600_D08298[81] = {
#include "assets/actor_102600_animation_083EC_records.inc"
};

u16 Actor02600_D083DC[8] = {
#include "assets/actor_102600_animation_083EC_indices.inc"
};

AnimationSet Actor02600_D083EC = {
    Actor02600_D08298,
    Actor02600_D083DC,
    { NULL, Actor02600_D08168, NULL, NULL, Actor02600_D081D4, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D08414[20] = {
#include "assets/actor_102600_animation_08778_bank1.inc"
};

AnimationPackedRotation Actor02600_D08504[61] = {
#include "assets/actor_102600_animation_08778_bank4.inc"
};

AnimationRecord Actor02600_D085F8[92] = {
#include "assets/actor_102600_animation_08778_records.inc"
};

u16 Actor02600_D08768[8] = {
#include "assets/actor_102600_animation_08778_indices.inc"
};

AnimationSet Actor02600_D08778 = {
    Actor02600_D085F8,
    Actor02600_D08768,
    { NULL, Actor02600_D08414, NULL, NULL, Actor02600_D08504, NULL, NULL, NULL },
};

AnimationPackedPose Actor02600_D087A0[5] = {
#include "assets/actor_102600_animation_08928_bank1.inc"
};

AnimationPackedRotation Actor02600_D087DC[19] = {
#include "assets/actor_102600_animation_08928_bank4.inc"
};

AnimationRecord Actor02600_D08828[60] = {
#include "assets/actor_102600_animation_08928_records.inc"
};

u16 Actor02600_D08918[8] = {
#include "assets/actor_102600_animation_08928_indices.inc"
};

AnimationSet Actor02600_D08928 = {
    Actor02600_D08828,
    Actor02600_D08918,
    { NULL, Actor02600_D087A0, NULL, NULL, Actor02600_D087DC, NULL, NULL, NULL },
};

DamageAttack gMaggotCaterpillarAttacks[6] = {
    { 14, 0 },
    { 20, 0 },
    { 10, 1 },
    { 16, 6 },
    { 24, 6 },
    { 10, 0 },
};

EnemyParams gMaggotCaterpillarParams = { gMaggotCaterpillarAttacks, 160, 16, 68, 1, 100, 10, 100, 5 };

u16 gMaggotCaterpillarIdleDelay[8] = {
    25,
    10,
    10,
    10,
    10,
    10,
    10,
    35,
};

u16 gMaggotCaterpillarRoamDelay[8] = {
    1600,
    2400,
    2400,
    2400,
    2400,
    2400,
    2400,
    1600,
};

u16 gMaggotCaterpillarDropSpeed[8] = {
    100,
    100,
    180,
    130,
    100,
    100,
    100,
    100,
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

s32 Actor02600_D08A1C[5] = {
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

TaskDesc gMaggotCaterpillarBodyTask = { { { TASK_BODY_TMD, 96 } }, maggotCaterpillarTask, { .model = &Actor02600_D0576C } };

TaskDesc Actor02600_D08AC0 = { { { TASK_BODY_COORD, 96 } }, maggotCaterpillarPuffTask, { .value = 0 } };

AnimationSet* gMaggotCaterpillarAnimSets[15] = {
    NULL,
    &Actor02600_D06198,
    &Actor02600_D0644C,
    &Actor02600_D068D0,
    &Actor02600_D06E14,
    &Actor02600_D074DC,
    &Actor02600_D077A4,
    &Actor02600_D07934,
    NULL,
    &Actor02600_D07B48,
    &Actor02600_D07E88,
    &Actor02600_D08140,
    &Actor02600_D083EC,
    &Actor02600_D08778,
    &Actor02600_D08928,
};

#include "../../shared/maggot_caterpillar_resolve_contacts.inc.c"

/// State handlers of the projectile task `maggotCaterpillarPuffTask` dispatches,
/// indexed by `Task::state`: setup, per-frame tick and `Gp_DestroyEnemy`.
static const GpEnemyTaskFuncTable3 gMaggotCaterpillarPuffStates = {
    {
        maggotCaterpillarPuffSetup,
        maggotCaterpillarPuffTick,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the actor task `maggotCaterpillarTask` dispatches, indexed
/// by `Task::state`: spawn, per-frame tick and the dying sequence.
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
