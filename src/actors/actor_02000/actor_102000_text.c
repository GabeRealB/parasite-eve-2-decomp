#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/gtemac.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
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
#include "main/fs.h"
#include "main/random.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#define GOLEM_PAWN_ROOK_TYPE   GOLEM_PAWN
#define GOLEM_PAWN_ROOK_WEAPON GOLEM_BEAM_SWORD
#include "../../shared/player_detection.h"
#include "../../shared/golem_pawn_rook.h"

static const GpEnemyTaskFuncTable3 Actor02000_D00060;
static const GpEnemyTaskFuncTable3 Actor02000_D0006C;

extern DamageAttack gGolemPawnRookAttacks[];
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    s32 value;
    u8  retained[16];
} Actor102000TextStorage7C50;
STATIC_ASSERT_SIZEOF(Actor102000TextStorage7C50, 20);

extern Actor102000TextStorage7C50 gGolemPawnRookSwingCue;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern s16 gGolemPawnRookAnimBlendFrames[];
extern s16 gGolemPawnRookWeakSpotHits[];
extern s16 gGolemPawnRookWeakSpotHitsFlagged[];
extern s32 gGolemPawnRookVoiceCues[];

extern AnimationSet Actor02000_D09C20;

extern AnimationSet Actor02000_D0A588;

extern AnimationSet Actor02000_D0ABB0;

extern AnimationSet Actor02000_D0B3B0;

extern AnimationSet Actor02000_D0CAA4;

extern AnimationSet Actor02000_D0D1E8;

extern AnimationSet Actor02000_D0E7FC;

extern AnimationSet Actor02000_D0F534;

extern AnimationSet Actor02000_D0FC6C;

extern AnimationSet Actor02000_D10190;

extern AnimationSet Actor02000_D10BA0;

extern AnimationSet Actor02000_D11648;

extern AnimationSet Actor02000_D11C28;

extern AnimationSet Actor02000_D1291C;

extern AnimationSet Actor02000_D13994;

extern AnimationSet Actor02000_D13E68;

extern AnimationSet Actor02000_D14178;

extern AnimationSet Actor02000_D14354;

extern AnimationSet Actor02000_D1529C;

extern AnimationSet Actor02000_D15830;

extern AnimationSet Actor02000_D15AF8;

extern AnimationSet Actor02000_D15CD4;

extern TmdSource Actor02000_D08AA8;

extern TmdSource Actor02000_D08FEC;

void Actor02000_Fn02D5C(Task*);

void Actor02000_Fn035E8(Task*);

void Actor02000_Fn03728(Task*);

extern AnimationSet* Actor02000_D15FE8[31];

extern TaskDesc Actor02000_D15FD0[];

extern u16* Actor02000_D15FB8[];

extern EnemyParams Actor02000_D15D10;

extern TaskFunc gGolemPawnRookStates[];

static void Actor02000_Fn0251C(Enemy* ctx, Task* actor);

#include "../../shared/golem_pawn_rook_take_hits.inc.c"

s16 gGolemPawnRookAnimBlendFrames[32] = {
    0,
    8,
    8,
    0,
    8,
    8,
    0,
    0,
    8,
    0,
    8,
    0,
    0,
    0,
    0,
    0,
    8,
    4,
    4,
    4,
    4,
    4,
    4,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
};

TmdBone Actor02000_D037C4[19] = {
#include "assets/pawn_golem_body_skeleton.inc"
};

u32 Actor02000_D03A70[19] = {
#include "assets/pawn_golem_body_partVerts.inc"
};

SVECTOR Actor02000_D03ABC[339] = {
#include "assets/pawn_golem_body_verts.inc"
};

SVECTOR Actor02000_D04554[346] = {
#include "assets/pawn_golem_body_normals.inc"
};

u32 Actor02000_D05024[3745] = {
#include "assets/pawn_golem_body_stream.inc"
};

TmdSource Actor02000_D08AA8 = {
    0,
    20476,
    5672,
    19,
    Actor02000_D03A70,
    Actor02000_D03ABC,
    Actor02000_D04554,
    Actor02000_D037C4,
    Actor02000_D05024,
};

TmdBone Actor02000_D08ACC[1] = {
#include "assets/golem_beam_sword_skeleton.inc"
};

u32 Actor02000_D08AF0[1] = {
#include "assets/golem_beam_sword_partVerts.inc"
};

SVECTOR Actor02000_D08AF4[29] = {
#include "assets/golem_beam_sword_verts.inc"
};

SVECTOR Actor02000_D08BDC[24] = {
#include "assets/golem_beam_sword_normals.inc"
};

u32 Actor02000_D08C9C[212] = {
#include "assets/golem_beam_sword_stream.inc"
};

TmdSource Actor02000_D08FEC = {
    0,
    1436,
    0,
    1,
    Actor02000_D08AF0,
    Actor02000_D08AF4,
    Actor02000_D08BDC,
    Actor02000_D08ACC,
    Actor02000_D08C9C,
};

AnimationPackedPose Actor02000_D09010[21] = {
#include "assets/actor_102000_animation_09C20_bank1.inc"
};

AnimationPackedRotation Actor02000_D0910C[317] = {
#include "assets/actor_102000_animation_09C20_bank4.inc"
};

AnimationRecord Actor02000_D09600[382] = {
#include "assets/actor_102000_animation_09C20_records.inc"
};

u16 Actor02000_D09BF8[20] = {
#include "assets/actor_102000_animation_09C20_indices.inc"
};

AnimationSet Actor02000_D09C20 = {
    Actor02000_D09600,
    Actor02000_D09BF8,
    { NULL, Actor02000_D09010, NULL, NULL, Actor02000_D0910C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D09C48[16] = {
#include "assets/actor_102000_animation_0A588_bank1.inc"
};

AnimationPackedRotation Actor02000_D09D08[237] = {
#include "assets/actor_102000_animation_0A588_bank4.inc"
};

AnimationRecord Actor02000_D0A0BC[297] = {
#include "assets/actor_102000_animation_0A588_records.inc"
};

u16 Actor02000_D0A560[20] = {
#include "assets/actor_102000_animation_0A588_indices.inc"
};

AnimationSet Actor02000_D0A588 = {
    Actor02000_D0A0BC,
    Actor02000_D0A560,
    { NULL, Actor02000_D09C48, NULL, NULL, Actor02000_D09D08, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0A5B0[12] = {
#include "assets/actor_102000_animation_0ABB0_bank1.inc"
};

AnimationPackedRotation Actor02000_D0A640[142] = {
#include "assets/actor_102000_animation_0ABB0_bank4.inc"
};

AnimationRecord Actor02000_D0A878[196] = {
#include "assets/actor_102000_animation_0ABB0_records.inc"
};

u16 Actor02000_D0AB88[20] = {
#include "assets/actor_102000_animation_0ABB0_indices.inc"
};

AnimationSet Actor02000_D0ABB0 = {
    Actor02000_D0A878,
    Actor02000_D0AB88,
    { NULL, Actor02000_D0A5B0, NULL, NULL, Actor02000_D0A640, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0ABD8[14] = {
#include "assets/actor_102000_animation_0B3B0_bank1.inc"
};

AnimationPackedRotation Actor02000_D0AC80[186] = {
#include "assets/actor_102000_animation_0B3B0_bank4.inc"
};

AnimationRecord Actor02000_D0AF68[264] = {
#include "assets/actor_102000_animation_0B3B0_records.inc"
};

u16 Actor02000_D0B388[20] = {
#include "assets/actor_102000_animation_0B3B0_indices.inc"
};

AnimationSet Actor02000_D0B3B0 = {
    Actor02000_D0AF68,
    Actor02000_D0B388,
    { NULL, Actor02000_D0ABD8, NULL, NULL, Actor02000_D0AC80, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0B3D8[47] = {
#include "assets/actor_102000_animation_0CAA4_bank1.inc"
};

AnimationPackedRotation Actor02000_D0B60C[613] = {
#include "assets/actor_102000_animation_0CAA4_bank4.inc"
};

AnimationRecord Actor02000_D0BFA0[695] = {
#include "assets/actor_102000_animation_0CAA4_records.inc"
};

u16 Actor02000_D0CA7C[20] = {
#include "assets/actor_102000_animation_0CAA4_indices.inc"
};

AnimationSet Actor02000_D0CAA4 = {
    Actor02000_D0BFA0,
    Actor02000_D0CA7C,
    { NULL, Actor02000_D0B3D8, NULL, NULL, Actor02000_D0B60C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0CACC[19] = {
#include "assets/actor_102000_animation_0D1E8_bank1.inc"
};

AnimationPackedRotation Actor02000_D0CBB0[160] = {
#include "assets/actor_102000_animation_0D1E8_bank4.inc"
};

AnimationRecord Actor02000_D0CE30[228] = {
#include "assets/actor_102000_animation_0D1E8_records.inc"
};

u16 Actor02000_D0D1C0[20] = {
#include "assets/actor_102000_animation_0D1E8_indices.inc"
};

AnimationSet Actor02000_D0D1E8 = {
    Actor02000_D0CE30,
    Actor02000_D0D1C0,
    { NULL, Actor02000_D0CACC, NULL, NULL, Actor02000_D0CBB0, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0D210[66] = {
#include "assets/actor_102000_animation_0E7FC_bank1.inc"
};

AnimationPackedRotation Actor02000_D0D528[549] = {
#include "assets/actor_102000_animation_0E7FC_bank4.inc"
};

AnimationRecord Actor02000_D0DDBC[646] = {
#include "assets/actor_102000_animation_0E7FC_records.inc"
};

u16 Actor02000_D0E7D4[20] = {
#include "assets/actor_102000_animation_0E7FC_indices.inc"
};

AnimationSet Actor02000_D0E7FC = {
    Actor02000_D0DDBC,
    Actor02000_D0E7D4,
    { NULL, Actor02000_D0D210, NULL, NULL, Actor02000_D0D528, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0E824[21] = {
#include "assets/actor_102000_animation_0F534_bank1.inc"
};

AnimationPackedRotation Actor02000_D0E920[354] = {
#include "assets/actor_102000_animation_0F534_bank4.inc"
};

AnimationRecord Actor02000_D0EEA8[409] = {
#include "assets/actor_102000_animation_0F534_records.inc"
};

u16 Actor02000_D0F50C[20] = {
#include "assets/actor_102000_animation_0F534_indices.inc"
};

AnimationSet Actor02000_D0F534 = {
    Actor02000_D0EEA8,
    Actor02000_D0F50C,
    { NULL, Actor02000_D0E824, NULL, NULL, Actor02000_D0E920, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0F55C[16] = {
#include "assets/actor_102000_animation_0FC6C_bank1.inc"
};

AnimationPackedRotation Actor02000_D0F61C[177] = {
#include "assets/actor_102000_animation_0FC6C_bank4.inc"
};

AnimationRecord Actor02000_D0F8E0[217] = {
#include "assets/actor_102000_animation_0FC6C_records.inc"
};

u16 Actor02000_D0FC44[20] = {
#include "assets/actor_102000_animation_0FC6C_indices.inc"
};

AnimationSet Actor02000_D0FC6C = {
    Actor02000_D0F8E0,
    Actor02000_D0FC44,
    { NULL, Actor02000_D0F55C, NULL, NULL, Actor02000_D0F61C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0FC94[8] = {
#include "assets/actor_102000_animation_10190_bank1.inc"
};

AnimationPackedRotation Actor02000_D0FCF4[116] = {
#include "assets/actor_102000_animation_10190_bank4.inc"
};

AnimationRecord Actor02000_D0FEC4[169] = {
#include "assets/actor_102000_animation_10190_records.inc"
};

u16 Actor02000_D10168[20] = {
#include "assets/actor_102000_animation_10190_indices.inc"
};

AnimationSet Actor02000_D10190 = {
    Actor02000_D0FEC4,
    Actor02000_D10168,
    { NULL, Actor02000_D0FC94, NULL, NULL, Actor02000_D0FCF4, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D101B8[19] = {
#include "assets/actor_102000_animation_10BA0_bank1.inc"
};

AnimationPackedRotation Actor02000_D1029C[255] = {
#include "assets/actor_102000_animation_10BA0_bank4.inc"
};

AnimationRecord Actor02000_D10698[312] = {
#include "assets/actor_102000_animation_10BA0_records.inc"
};

u16 Actor02000_D10B78[20] = {
#include "assets/actor_102000_animation_10BA0_indices.inc"
};

AnimationSet Actor02000_D10BA0 = {
    Actor02000_D10698,
    Actor02000_D10B78,
    { NULL, Actor02000_D101B8, NULL, NULL, Actor02000_D1029C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D10BC8[18] = {
#include "assets/actor_102000_animation_11648_bank1.inc"
};

AnimationPackedRotation Actor02000_D10CA0[281] = {
#include "assets/actor_102000_animation_11648_bank4.inc"
};

AnimationRecord Actor02000_D11104[327] = {
#include "assets/actor_102000_animation_11648_records.inc"
};

u16 Actor02000_D11620[20] = {
#include "assets/actor_102000_animation_11648_indices.inc"
};

AnimationSet Actor02000_D11648 = {
    Actor02000_D11104,
    Actor02000_D11620,
    { NULL, Actor02000_D10BC8, NULL, NULL, Actor02000_D10CA0, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D11670[8] = {
#include "assets/actor_102000_animation_11C28_bank1.inc"
};

AnimationPackedRotation Actor02000_D116D0[130] = {
#include "assets/actor_102000_animation_11C28_bank4.inc"
};

AnimationRecord Actor02000_D118D8[202] = {
#include "assets/actor_102000_animation_11C28_records.inc"
};

u16 Actor02000_D11C00[20] = {
#include "assets/actor_102000_animation_11C28_indices.inc"
};

AnimationSet Actor02000_D11C28 = {
    Actor02000_D118D8,
    Actor02000_D11C00,
    { NULL, Actor02000_D11670, NULL, NULL, Actor02000_D116D0, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D11C50[23] = {
#include "assets/actor_102000_animation_1291C_bank1.inc"
};

AnimationPackedRotation Actor02000_D11D64[326] = {
#include "assets/actor_102000_animation_1291C_bank4.inc"
};

AnimationRecord Actor02000_D1227C[414] = {
#include "assets/actor_102000_animation_1291C_records.inc"
};

u16 Actor02000_D128F4[20] = {
#include "assets/actor_102000_animation_1291C_indices.inc"
};

AnimationSet Actor02000_D1291C = {
    Actor02000_D1227C,
    Actor02000_D128F4,
    { NULL, Actor02000_D11C50, NULL, NULL, Actor02000_D11D64, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D12944[31] = {
#include "assets/actor_102000_animation_13994_bank1.inc"
};

AnimationPackedRotation Actor02000_D12AB8[429] = {
#include "assets/actor_102000_animation_13994_bank4.inc"
};

AnimationRecord Actor02000_D1316C[512] = {
#include "assets/actor_102000_animation_13994_records.inc"
};

u16 Actor02000_D1396C[20] = {
#include "assets/actor_102000_animation_13994_indices.inc"
};

AnimationSet Actor02000_D13994 = {
    Actor02000_D1316C,
    Actor02000_D1396C,
    { NULL, Actor02000_D12944, NULL, NULL, Actor02000_D12AB8, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D139BC[9] = {
#include "assets/actor_102000_animation_13E68_bank1.inc"
};

AnimationPackedRotation Actor02000_D13A28[113] = {
#include "assets/actor_102000_animation_13E68_bank4.inc"
};

AnimationRecord Actor02000_D13BEC[149] = {
#include "assets/actor_102000_animation_13E68_records.inc"
};

u16 Actor02000_D13E40[20] = {
#include "assets/actor_102000_animation_13E68_indices.inc"
};

AnimationSet Actor02000_D13E68 = {
    Actor02000_D13BEC,
    Actor02000_D13E40,
    { NULL, Actor02000_D139BC, NULL, NULL, Actor02000_D13A28, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D13E90[5] = {
#include "assets/actor_102000_animation_14178_bank1.inc"
};

AnimationPackedRotation Actor02000_D13ECC[65] = {
#include "assets/actor_102000_animation_14178_bank4.inc"
};

AnimationRecord Actor02000_D13FD0[96] = {
#include "assets/actor_102000_animation_14178_records.inc"
};

u16 Actor02000_D14150[20] = {
#include "assets/actor_102000_animation_14178_indices.inc"
};

AnimationSet Actor02000_D14178 = {
    Actor02000_D13FD0,
    Actor02000_D14150,
    { NULL, Actor02000_D13E90, NULL, NULL, Actor02000_D13ECC, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D141A0[2] = {
#include "assets/actor_102000_animation_14354_bank1.inc"
};

AnimationPackedRotation Actor02000_D141B8[17] = {
#include "assets/actor_102000_animation_14354_bank4.inc"
};

AnimationRecord Actor02000_D141FC[76] = {
#include "assets/actor_102000_animation_14354_records.inc"
};

u16 Actor02000_D1432C[20] = {
#include "assets/actor_102000_animation_14354_indices.inc"
};

AnimationSet Actor02000_D14354 = {
    Actor02000_D141FC,
    Actor02000_D1432C,
    { NULL, Actor02000_D141A0, NULL, NULL, Actor02000_D141B8, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D1437C[28] = {
#include "assets/actor_102000_animation_1529C_bank1.inc"
};

AnimationPackedRotation Actor02000_D144CC[394] = {
#include "assets/actor_102000_animation_1529C_bank4.inc"
};

AnimationRecord Actor02000_D14AF4[480] = {
#include "assets/actor_102000_animation_1529C_records.inc"
};

u16 Actor02000_D15274[20] = {
#include "assets/actor_102000_animation_1529C_indices.inc"
};

AnimationSet Actor02000_D1529C = {
    Actor02000_D14AF4,
    Actor02000_D15274,
    { NULL, Actor02000_D1437C, NULL, NULL, Actor02000_D144CC, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D152C4[9] = {
#include "assets/actor_102000_animation_15830_bank1.inc"
};

AnimationPackedRotation Actor02000_D15330[127] = {
#include "assets/actor_102000_animation_15830_bank4.inc"
};

AnimationRecord Actor02000_D1552C[183] = {
#include "assets/actor_102000_animation_15830_records.inc"
};

u16 Actor02000_D15808[20] = {
#include "assets/actor_102000_animation_15830_indices.inc"
};

AnimationSet Actor02000_D15830 = {
    Actor02000_D1552C,
    Actor02000_D15808,
    { NULL, Actor02000_D152C4, NULL, NULL, Actor02000_D15330, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D15858[5] = {
#include "assets/actor_102000_animation_15AF8_bank1.inc"
};

AnimationPackedRotation Actor02000_D15894[56] = {
#include "assets/actor_102000_animation_15AF8_bank4.inc"
};

AnimationRecord Actor02000_D15974[87] = {
#include "assets/actor_102000_animation_15AF8_records.inc"
};

u16 Actor02000_D15AD0[20] = {
#include "assets/actor_102000_animation_15AF8_indices.inc"
};

AnimationSet Actor02000_D15AF8 = {
    Actor02000_D15974,
    Actor02000_D15AD0,
    { NULL, Actor02000_D15858, NULL, NULL, Actor02000_D15894, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D15B20[2] = {
#include "assets/actor_102000_animation_15CD4_bank1.inc"
};

AnimationPackedRotation Actor02000_D15B38[17] = {
#include "assets/actor_102000_animation_15CD4_bank4.inc"
};

AnimationRecord Actor02000_D15B7C[76] = {
#include "assets/actor_102000_animation_15CD4_records.inc"
};

u16 Actor02000_D15CAC[20] = {
#include "assets/actor_102000_animation_15CD4_indices.inc"
};

AnimationSet Actor02000_D15CD4 = {
    Actor02000_D15B7C,
    Actor02000_D15CAC,
    { NULL, Actor02000_D15B20, NULL, NULL, Actor02000_D15B38, NULL, NULL, NULL },
};

DamageAttack gGolemPawnRookAttacks[5] = {
    { 28, 5 },
    { 24, 5 },
    { 0, 8 },
    { 15, 2 },
    { 5, 0 },
};

EnemyParams Actor02000_D15D10 = { gGolemPawnRookAttacks, 425, 125, 100, 5, 50, 6, 0, 0 };

s16 gGolemPawnRookWeakSpotHits[46] = {
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
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
    1,
    1,
    0,
};

s16 gGolemPawnRookWeakSpotHitsFlagged[56] = {
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    1,
    1,
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

s32 gGolemPawnRookVoiceCues[17] = {
    0,
    0x40140001,
    0x40140002,
    0x40140003,
    0x40140004,
    0x40140005,
    0x40140006,
    0x4014000C,
    0x4014000D,
    0x40140009,
    0x4014000A,
    0x4014000B,
    0x4014000E,
    0x4014000F,
    0x40140010,
    0x40140011,
    0x40140012,
};

Actor102000TextStorage7C50 gGolemPawnRookSwingCue = { 0x40140007, { 0 } };

u16 Actor02000_D15E44[22] = {
    0,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    0,
    2,
    0,
    2,
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

u16 Actor02000_D15E70[40] = {
    0,
    0,
    4,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
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
    0,
    0,
    2,
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

u16 Actor02000_D15EC0[40] = {
    0,
    4,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
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
    4,
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
    4,
    0,
};

u16 Actor02000_D15F10[50] = {
    0,
    4,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    1,
    2,
    1,
    0,
    3,
    2,
    0,
    2,
    0,
    3,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    3,
    1,
    0,
    1,
    0,
    2,
    2,
    2,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
};

u16 Actor02000_D15F74[34] = {
    0,
    0,
    2,
    2,
    0,
    2,
    0,
    0,
    0,
    0,
    2,
    4,
    0,
    2,
    2,
    0,
    2,
    0,
    4,
    2,
    0,
    2,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    4,
    0,
};

u16* Actor02000_D15FB8[6] = {
    NULL,
    Actor02000_D15E44,
    Actor02000_D15E70,
    Actor02000_D15EC0,
    Actor02000_D15F10,
    Actor02000_D15F74,
};

TaskDesc Actor02000_D15FD0[2] = {
    { { { TASK_BODY_TMD, 96 } }, Actor02000_Fn03728, { .model = &Actor02000_D08AA8 } },
    { { { TASK_BODY_TMD, 96 } }, Actor02000_Fn035E8, { .model = &Actor02000_D08FEC } },
};

AnimationSet* Actor02000_D15FE8[31] = {
    NULL,
    &Actor02000_D09C20,
    &Actor02000_D0A588,
    &Actor02000_D0ABB0,
    &Actor02000_D0B3B0,
    &Actor02000_D0CAA4,
    &Actor02000_D0D1E8,
    &Actor02000_D0E7FC,
    &Actor02000_D0FC6C,
    &Actor02000_D0F534,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &Actor02000_D10190,
    &Actor02000_D10BA0,
    &Actor02000_D11648,
    &Actor02000_D11C28,
    &Actor02000_D1291C,
    &Actor02000_D13994,
    &Actor02000_D13E68,
    &Actor02000_D14178,
    &Actor02000_D14354,
    &Actor02000_D1529C,
    &Actor02000_D15830,
    &Actor02000_D15AF8,
    &Actor02000_D15CD4,
    NULL,
};

TaskFunc gGolemPawnRookStates[15] = {
    golemPawnRookIdleState,
    golemPawnRookApproachState,
    Actor02000_Fn02D5C,
    golemPawnRookChargeState,
    golemPawnRookBeamSwingState,
    golemPawnRookNopState,
    golemPawnRookNopState,
    golemPawnRookNopState,
    golemPawnRookHitReactionState,
    golemPawnRookRecoilState,
    golemPawnRookFlagWaitState,
    golemPawnRookKnockdownState,
    golemPawnRookDownedShiftState,
    golemPawnRookCollapseState,
    golemPawnRookDownedFinishState,
};

#include "../../shared/golem_pawn_rook_approach.inc.c"

#include "../../shared/golem_pawn_rook_proximity.inc.c"

#include "../../shared/golem_pawn_rook_knockdown.inc.c"

#include "../../shared/golem_pawn_rook_downed_shift.inc.c"

#include "../../shared/golem_pawn_rook_collapse.inc.c"

#include "../../shared/golem_pawn_rook_turn.inc.c"

#include "../../shared/golem_pawn_rook_hit_tilt.inc.c"

#include "../../shared/golem_pawn_rook_anim_cues.inc.c"

#include "../../shared/golem_pawn_rook_dead.inc.c"

#include "../../shared/golem_pawn_rook_charge_state.inc.c"

#include "../../shared/golem_pawn_rook_beam_swing.inc.c"

/// Enemy init. Allocates the 0x6E4-byte work block, points the model object at
/// the light / color matrices inside it, runs the animation context over its
/// nineteen slots, and spawns the companion enemy from `Actor02000_D15FD0`,
/// copying that model's texture page and CLUT row out of the current area
/// record. `Enemy.spawnState` then selects the variant: 0 builds the
/// full object set (list node, the four `Gp_LinkObj` nodes and their
/// `WorldCollisionContact` tables, and the optional CD prefetch of `field_6D6`),
/// while 1 and 2 only prime the animation state and hand the task to state 2.
static void Actor02000_Fn0251C(Enemy* ctx, Task* actor)
{
    GolemPawnRookWork* work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          parts;
    GfxCoord*          partsA;
    GfxCoord*          partsB;
    GfxCoord*          partsC;
    GfxCoord*          effParts;
    Enemy*             eff;
    u16*               tbl;
    u8                 param1[8];
    u8                 param2[8];
    s32                i;
    s32                one;
    s32                kind;
    s32                param;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x6E4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work                = work;
    obj->flags                 = 0;
    coord->composeStamp        = GRAPHICS_COORD_DIRTY;
    obj->lightMtx              = &work->field_45C;
    obj->colorMtx              = &work->field_43C;
    work->field_6CA            = 0x14;
    work->field_66C            = Actor02000_D15FD0;
    work->field_670.coord      = &actor->extra.tmd->coords[3];
    work->field_670.spawnArgLo = 0x500;
    work->field_670.spawnArgHi = 2;
    animationInitContext(&work->rig.anim, Actor02000_D15FE8, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < 0x13; i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
    eff = Gp_SpawnEnemyFromTable(Actor02000_D15FD0, 1, 0, ctx);
    actorTintTask(eff->task, ctx);

    one  = 1;
    kind = ctx->spawnState;
    if (kind == one) {
        goto case1;
    }
    if (kind >= 2) {
        goto ge2;
    }
    if (kind == 0) {
        goto case0;
    }
    return;
ge2:
    if (kind == 2) {
        goto case2;
    }
    return;

case0:
    ctx->field_4  = &coord->coord;
    ctx->field_48 = 0;
    Gp_LinkNode(&ctx->node);
    parts           = actor->extra.tmd->coords;
    ctx->bodyPos.vx = 0;
    ctx->bodyPos.vy = 0;
    ctx->bodyPos.vz = 0;
    ctx->param      = &Actor02000_D15D10;
    ctx->recs       = work->field_4EC;
    ctx->coord      = &parts[3];
    ctx->hp         = Actor02000_D15D10.hpMax;
    Gp_IncStateF0Ref(0);
    work->field_6AC = ctx->place->mode & 1;
    if (work->field_6AC == 0) {
        work->field_694 = one;
        work->field_6A6 = 0;
    } else {
        work->field_694 = 2;
        work->field_6A6 = one;
        param           = ctx->place->variant;
        work->field_6DA = param * 1000;
    }

    tbl = Actor02000_D15FB8[gGameSession->location.loc.stage];
    if (tbl != NULL) {
        work->field_6D6 = tbl[gGameSession->location.loc.area];
    }
    if (work->field_6D6 != 0) {
        param1[3] = 0;
        param1[2] = 0xA;
        param1[0] = work->field_6D6;
        param2[0] = 0x14;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
    }

    work->field_49C.ends[0].vz      = 0x1F40;
    work->field_49C.end0Radius      = 0x3E8;
    work->field_49C.ends[0].vx      = 0;
    work->field_49C.ends[0].vy      = 0;
    work->field_49C.ends[1].vx      = 0;
    work->field_49C.ends[1].vy      = 0;
    work->field_49C.ends[1].vz      = 0;
    work->field_49C.end1Radius      = 0x5DC;
    work->field_49C.contacts        = work->field_4B4;
    partsA                          = actor->extra.tmd->coords;
    work->field_47C.context.capsule = &work->field_49C;
    work->field_47C.pos.vx          = 0;
    work->field_47C.pos.vy          = 0;
    work->field_47C.pos.vz          = 0;
    work->field_47C.key             = 0;
    work->field_47C.radius          = 0;
    work->field_47C.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->field_47C.coord           = &partsA[4];
    Gp_LinkObj(3, &work->field_47C);
    Gp_InitRec18Table(work->field_4B4, 1, 0);
    work->field_47C.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

    partsB                           = actor->extra.tmd->coords;
    work->field_4CC.context.contacts = work->field_4EC;
    work->field_4CC.pos.vx           = 0;
    work->field_4CC.pos.vy           = 0;
    work->field_4CC.pos.vz           = 0;
    work->field_4CC.key              = 0x30014;
    work->field_4CC.radius           = 0x190;
    work->field_4CC.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->field_4CC.coord            = &partsB[3];
    Gp_LinkObj(2, &work->field_4CC);
    Gp_InitRec18Table(work->field_4EC, 5, 0);
    work->field_4CC.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    partsC                           = actor->extra.tmd->coords;
    work->field_564.pos.vy           = -0x226;
    work->field_564.context.contacts = work->field_584;
    work->field_564.pos.vx           = 0;
    work->field_564.pos.vz           = 0;
    work->field_564.key              = 0;
    work->field_564.radius           = 0x226;
    work->field_564.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->field_564.coord            = partsC;
    Gp_LinkObj(2, &work->field_564);
    Gp_InitRec18Table(work->field_584, 4, 0);
    work->field_564.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

    effParts                         = eff->task->extra.tmd->coords;
    work->field_5E4.context.contacts = work->field_604;
    work->field_5E4.pos.vx           = 0;
    work->field_5E4.pos.vy           = 0x1F4;
    work->field_5E4.pos.vz           = 0;
    work->field_5E4.key              = 0;
    work->field_5E4.radius           = 0x1F4;
    work->field_5E4.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->field_5E4.coord            = effParts;
    Gp_LinkObj(3, &work->field_5E4);
    Gp_InitRec18Table(work->field_604, 1, 0);
    work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    actor->state           = 1;
    return;

case1:
    work->field_694 = 0x19;
    work->field_6A8 = 2;
    actor->state    = 2;
    return;

case2:
    work->field_694 = 0x1D;
    work->field_6A8 = kind;
    actor->state    = kind;
}

#include "../../shared/golem_pawn_rook_inlines.inc.c"

/// Saves the root coordinate's translation in `field_678`..`field_680`, then
/// Updates the enemy's colour from `coord`'s world position and draws the
#include "../../shared/golem_pawn_rook_frame_no_dust.inc.c"

void Actor02000_Fn02D5C(Task* arg0)
{
    s16                yaw;
    s16                yaw2;
    s16                state;
    s16                deltaYaw;
    s16                deltaYaw2;
    s16                speed;
    s32                magnitude;
    s32                magnitude2;
    s16                wrapped;
    s16                wrapped2;
    s16                angle;
    s32                dx;
    s32                dz;
    u16                flags;
    u16                flags2;
    u8*                head;
    VECTOR*            delta;
    GolemPawnRookWork* work;
    GfxCoord*          coord;

    head                     = SCRATCH_STACK_CURSOR(u8);
    delta                    = (VECTOR*)(head - 0x10);
    SCRATCH_STACK_CURSOR(u8) = (u8*)delta;
    work                     = arg0->work;
    state                    = work->field_6A8;
    coord                    = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= gGolemPawnRookAnimBlendFrames[work->field_694]) {
                speed = 0x14;
            }
            work->field_69C = speed;
            work->field_69E = 0x3C;
            delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
            yaw             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
            work->field_6A2 = yaw;
            deltaYaw        = work->field_6A4 - yaw;
            magnitude       = __builtin_abs(deltaYaw);
            if (magnitude < 0x800) {
                angle = magnitude;
            } else {
                if (deltaYaw > 0) {
                    wrapped = 0x1000 - deltaYaw;
                } else {
                    wrapped = deltaYaw + 0x1000;
                }
                angle = wrapped;
            }
            if (angle >= 0x581) {
                if (work->field_6DC == 0) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 4;
                    work->field_6A8 = 1;
                    work->field_6DC = 0;
                }
            }
            if (angle < 0x80) {
                flags                 = work->field_47C.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->field_47C.flags = flags;
                if (work->field_6B2 != 0) {
                    work->field_47C.flags = (u16)(flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                    work->field_6A8       = 1;
                    work->field_6DC       = 0;
                }
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            dz              = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            delta->vz       = dz;
            dx              = delta->vx;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x8CA) {
                work->field_6A6 = 4;
                work->field_6A8 = 0;
                work->field_694 = 8;
            } else {
                work->field_6A6 = 3;
                work->field_6A8 = 0;
                work->field_694 = 5;
                work->field_6AE = 0;
            }
            break;
        case 2:
            work->field_69C       = 0;
            work->field_69E       = 0;
            flags2                = work->field_47C.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->field_47C.flags = flags2;
            if (work->field_6B2 != 0) {
                work->field_47C.flags = (u16)(flags2 & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                work->field_694       = 2;
                work->field_6A8       = 0;
            } else if (work->field_698 >= 0x60) {
                delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                delta->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
                yaw2            = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                work->field_6A2 = yaw2;
                deltaYaw2       = work->field_6A4 - yaw2;
                magnitude2      = __builtin_abs(deltaYaw2);
                if (magnitude2 < 0x800) {
                    angle = magnitude2;
                } else {
                    if (deltaYaw2 > 0) {
                        wrapped2 = 0x1000 - deltaYaw2;
                    } else {
                        wrapped2 = deltaYaw2 + 0x1000;
                    }
                    angle = wrapped2;
                }
                if (angle >= 0x581) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 2;
                    work->field_6A8 = 0;
                }
            }
            break;
        case 3:
            work->field_69C = 0;
            work->field_69E = 0x3B;
            if (work->field_698 >= 0x23) {
                work->field_694 = 2;
                work->field_6A8 = 0;
                work->field_6DC = 1;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

#include "../../shared/player_detection_segment.inc.c"

#include "../../shared/golem_pawn_rook_idle.inc.c"

#include "../../shared/golem_pawn_rook_hit_reaction.inc.c"

#include "../../shared/golem_pawn_rook_recoil.inc.c"

#include "../../shared/golem_pawn_rook_flag_wait.inc.c"

#include "../../shared/golem_pawn_rook_downed_finish.inc.c"

#include "../../shared/golem_pawn_rook_nop.inc.c"

void Actor02000_Fn035E8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02000_D00060;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

#include "../../shared/golem_pawn_rook_delayed_effect_spawn.inc.c"

#include "../../shared/golem_pawn_rook_delayed_effect_tick.inc.c"

void Actor02000_Fn03728(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02000_D0006C;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

static const GpEnemyTaskFuncTable3 Actor02000_D00060 = { {
    golemPawnRookDelayedEffectSpawn,
    golemPawnRookDelayedEffectTick,
    Gp_DestroyEnemy,
} };

static const GpEnemyTaskFuncTable3 Actor02000_D0006C = { {
    Actor02000_Fn0251C,
    golemPawnRookFrameStateNoDust,
    golemPawnRookDeadState,
} };
