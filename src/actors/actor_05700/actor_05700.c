#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

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
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#include "../../shared/player_detection.h"
#define GOLEM_PAWN_ROOK_TYPE   GOLEM_ROOK
#define GOLEM_PAWN_ROOK_WEAPON GOLEM_GRENADE_LAUNCHER
#include "../../shared/golem_pawn_rook.h"

/// Sound ids this actor's cues play, indexed by `GolemPawnRookWork.field_6D6`
/// (row `field_6D6` starts at the second word, the `- 1` in the body).
extern s32 gGolemPawnRookVoiceCues[];

/// Per-animation frame marks: row `field_694` holds the frame the 0x1C, 0x28
/// and 0x7A marks of `golemPawnRookLungeStrikeState` are measured from.
extern s16 gGolemPawnRookAnimBlendFrames[];

/// The body objects' variant flag comes from `gGolemPawnRookAttacks`.
extern DamageAttack gGolemPawnRookAttacks[5];

/// Per-weapon-id weak-point flags (`id & 0x7F`) for the two hit families,
/// picked by the id's 0x8000 bit.
extern s16 gGolemPawnRookWeakPointWeapons[];
extern s16 gGolemPawnRookWeakPointPe[];

extern AnimationSet Actor05700_D0BAAC;
extern AnimationSet Actor05700_D0C414;
extern AnimationSet Actor05700_D0CA3C;
extern AnimationSet Actor05700_D0D23C;
extern AnimationSet Actor05700_D0D444;
extern AnimationSet Actor05700_D0DA50;
extern AnimationSet Actor05700_D0DF80;
extern AnimationSet Actor05700_D0E464;
extern AnimationSet Actor05700_D0E8C4;
extern AnimationSet Actor05700_D0F980;
extern AnimationSet Actor05700_D108D8;
extern AnimationSet Actor05700_D10DFC;
extern AnimationSet Actor05700_D1180C;
extern AnimationSet Actor05700_D122B4;
extern AnimationSet Actor05700_D12894;
extern AnimationSet Actor05700_D13588;
extern AnimationSet Actor05700_D14600;
extern AnimationSet Actor05700_D14AD4;
extern AnimationSet Actor05700_D14DE4;
extern AnimationSet Actor05700_D14FC0;
extern AnimationSet Actor05700_D15F08;
extern AnimationSet Actor05700_D1649C;
extern AnimationSet Actor05700_D16764;
extern AnimationSet Actor05700_D16940;
extern AnimationSet Actor05700_D170CC;
extern TmdSource    Actor05700_D0A3D8;
extern TmdSource    Actor05700_D0A824;
extern TmdSource    Actor05700_D0AB4C;
extern TmdSource    Actor05700_D0AE78;
void                Actor05700_Fn05040(Task*);
void                Actor05700_Fn0517C(Task*);
void                Actor05700_Fn05270(Task*);
void                Actor05700_Fn05470(Task*);

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

TmdBone Actor05700_D0550C[19] = {
#include "assets/rook_golem_body_skeleton.inc"
};

u32 Actor05700_D057B8[19] = {
#include "assets/rook_golem_body_partVerts.inc"
};

SVECTOR Actor05700_D05804[328] = {
#include "assets/rook_golem_body_verts.inc"
};

SVECTOR Actor05700_D06244[344] = {
#include "assets/rook_golem_body_normals.inc"
};

u32 Actor05700_D06D04[3509] = {
#include "assets/rook_golem_body_stream.inc"
};

TmdSource Actor05700_D0A3D8 = {
    0,
    19580,
    4888,
    19,
    Actor05700_D057B8,
    Actor05700_D05804,
    Actor05700_D06244,
    Actor05700_D0550C,
    Actor05700_D06D04,
};

TmdBone Actor05700_D0A3FC[1] = {
#include "assets/golem_grenade_launcher_skeleton.inc"
};

u32 Actor05700_D0A420[1] = {
#include "assets/golem_grenade_launcher_partVerts.inc"
};

SVECTOR Actor05700_D0A424[24] = {
#include "assets/golem_grenade_launcher_verts.inc"
};

SVECTOR Actor05700_D0A4E4[24] = {
#include "assets/golem_grenade_launcher_normals.inc"
};

u32 Actor05700_D0A5A4[160] = {
#include "assets/golem_grenade_launcher_stream.inc"
};

TmdSource Actor05700_D0A824 = {
    0,
    1144,
    0,
    1,
    Actor05700_D0A420,
    Actor05700_D0A424,
    Actor05700_D0A4E4,
    Actor05700_D0A3FC,
    Actor05700_D0A5A4,
};

TmdBone Actor05700_D0A848[1] = {
#include "assets/rook_golem_shield_skeleton.inc"
};

u32 Actor05700_D0A86C[1] = {
#include "assets/rook_golem_shield_partVerts.inc"
};

SVECTOR Actor05700_D0A870[16] = {
#include "assets/rook_golem_shield_verts.inc"
};

SVECTOR Actor05700_D0A8F0[20] = {
#include "assets/rook_golem_shield_normals.inc"
};

u32 Actor05700_D0A990[111] = {
#include "assets/rook_golem_shield_stream.inc"
};

TmdSource Actor05700_D0AB4C = {
    0,
    780,
    0,
    1,
    Actor05700_D0A86C,
    Actor05700_D0A870,
    Actor05700_D0A8F0,
    Actor05700_D0A848,
    Actor05700_D0A990,
};

TmdBone Actor05700_D0AB70[1] = {
#include "assets/golem_grenade_skeleton.inc"
};

u32 Actor05700_D0AB94[1] = {
#include "assets/golem_grenade_partVerts.inc"
};

SVECTOR Actor05700_D0AB98[12] = {
#include "assets/golem_grenade_verts.inc"
};

SVECTOR Actor05700_D0ABF8[28] = {
#include "assets/golem_grenade_normals.inc"
};

u32 Actor05700_D0ACD8[104] = {
#include "assets/golem_grenade_stream.inc"
};

TmdSource Actor05700_D0AE78 = {
    0,
    720,
    0,
    1,
    Actor05700_D0AB94,
    Actor05700_D0AB98,
    Actor05700_D0ABF8,
    Actor05700_D0AB70,
    Actor05700_D0ACD8,
};

AnimationPackedPose Actor05700_D0AE9C[21] = {
#include "assets/actor_105700_animation_0BAAC_bank1.inc"
};

AnimationPackedRotation Actor05700_D0AF98[317] = {
#include "assets/actor_105700_animation_0BAAC_bank4.inc"
};

AnimationRecord Actor05700_D0B48C[382] = {
#include "assets/actor_105700_animation_0BAAC_records.inc"
};

u16 Actor05700_D0BA84[20] = {
#include "assets/actor_105700_animation_0BAAC_indices.inc"
};

AnimationSet Actor05700_D0BAAC = {
    Actor05700_D0B48C,
    Actor05700_D0BA84,
    { NULL, Actor05700_D0AE9C, NULL, NULL, Actor05700_D0AF98, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D0BAD4[16] = {
#include "assets/actor_105700_animation_0C414_bank1.inc"
};

AnimationPackedRotation Actor05700_D0BB94[237] = {
#include "assets/actor_105700_animation_0C414_bank4.inc"
};

AnimationRecord Actor05700_D0BF48[297] = {
#include "assets/actor_105700_animation_0C414_records.inc"
};

u16 Actor05700_D0C3EC[20] = {
#include "assets/actor_105700_animation_0C414_indices.inc"
};

AnimationSet Actor05700_D0C414 = {
    Actor05700_D0BF48,
    Actor05700_D0C3EC,
    { NULL, Actor05700_D0BAD4, NULL, NULL, Actor05700_D0BB94, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D0C43C[12] = {
#include "assets/actor_105700_animation_0CA3C_bank1.inc"
};

AnimationPackedRotation Actor05700_D0C4CC[142] = {
#include "assets/actor_105700_animation_0CA3C_bank4.inc"
};

AnimationRecord Actor05700_D0C704[196] = {
#include "assets/actor_105700_animation_0CA3C_records.inc"
};

u16 Actor05700_D0CA14[20] = {
#include "assets/actor_105700_animation_0CA3C_indices.inc"
};

AnimationSet Actor05700_D0CA3C = {
    Actor05700_D0C704,
    Actor05700_D0CA14,
    { NULL, Actor05700_D0C43C, NULL, NULL, Actor05700_D0C4CC, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D0CA64[14] = {
#include "assets/actor_105700_animation_0D23C_bank1.inc"
};

AnimationPackedRotation Actor05700_D0CB0C[186] = {
#include "assets/actor_105700_animation_0D23C_bank4.inc"
};

AnimationRecord Actor05700_D0CDF4[264] = {
#include "assets/actor_105700_animation_0D23C_records.inc"
};

u16 Actor05700_D0D214[20] = {
#include "assets/actor_105700_animation_0D23C_indices.inc"
};

AnimationSet Actor05700_D0D23C = {
    Actor05700_D0CDF4,
    Actor05700_D0D214,
    { NULL, Actor05700_D0CA64, NULL, NULL, Actor05700_D0CB0C, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D0D264[3] = {
#include "assets/actor_105700_animation_0D444_bank1.inc"
};

AnimationPackedRotation Actor05700_D0D288[25] = {
#include "assets/actor_105700_animation_0D444_bank4.inc"
};

AnimationRecord Actor05700_D0D2EC[76] = {
#include "assets/actor_105700_animation_0D444_records.inc"
};

u16 Actor05700_D0D41C[20] = {
#include "assets/actor_105700_animation_0D444_indices.inc"
};

AnimationSet Actor05700_D0D444 = {
    Actor05700_D0D2EC,
    Actor05700_D0D41C,
    { NULL, Actor05700_D0D264, NULL, NULL, Actor05700_D0D288, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D0D46C[9] = {
#include "assets/actor_105700_animation_0DA50_bank1.inc"
};

AnimationPackedRotation Actor05700_D0D4D8[151] = {
#include "assets/actor_105700_animation_0DA50_bank4.inc"
};

AnimationRecord Actor05700_D0D734[189] = {
#include "assets/actor_105700_animation_0DA50_records.inc"
};

u16 Actor05700_D0DA28[20] = {
#include "assets/actor_105700_animation_0DA50_indices.inc"
};

AnimationSet Actor05700_D0DA50 = {
    Actor05700_D0D734,
    Actor05700_D0DA28,
    { NULL, Actor05700_D0D46C, NULL, NULL, Actor05700_D0D4D8, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D0DA78[9] = {
#include "assets/actor_105700_animation_0DF80_bank1.inc"
};

AnimationPackedRotation Actor05700_D0DAE4[125] = {
#include "assets/actor_105700_animation_0DF80_bank4.inc"
};

AnimationRecord Actor05700_D0DCD8[160] = {
#include "assets/actor_105700_animation_0DF80_records.inc"
};

u16 Actor05700_D0DF58[20] = {
#include "assets/actor_105700_animation_0DF80_indices.inc"
};

AnimationSet Actor05700_D0DF80 = {
    Actor05700_D0DCD8,
    Actor05700_D0DF58,
    { NULL, Actor05700_D0DA78, NULL, NULL, Actor05700_D0DAE4, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D0DFA8[9] = {
#include "assets/actor_105700_animation_0E464_bank1.inc"
};

AnimationPackedRotation Actor05700_D0E014[107] = {
#include "assets/actor_105700_animation_0E464_bank4.inc"
};

AnimationRecord Actor05700_D0E1C0[159] = {
#include "assets/actor_105700_animation_0E464_records.inc"
};

u16 Actor05700_D0E43C[20] = {
#include "assets/actor_105700_animation_0E464_indices.inc"
};

AnimationSet Actor05700_D0E464 = {
    Actor05700_D0E1C0,
    Actor05700_D0E43C,
    { NULL, Actor05700_D0DFA8, NULL, NULL, Actor05700_D0E014, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D0E48C[8] = {
#include "assets/actor_105700_animation_0E8C4_bank1.inc"
};

AnimationPackedRotation Actor05700_D0E4EC[101] = {
#include "assets/actor_105700_animation_0E8C4_bank4.inc"
};

AnimationRecord Actor05700_D0E680[135] = {
#include "assets/actor_105700_animation_0E8C4_records.inc"
};

u16 Actor05700_D0E89C[20] = {
#include "assets/actor_105700_animation_0E8C4_indices.inc"
};

AnimationSet Actor05700_D0E8C4 = {
    Actor05700_D0E680,
    Actor05700_D0E89C,
    { NULL, Actor05700_D0E48C, NULL, NULL, Actor05700_D0E4EC, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D0E8EC[30] = {
#include "assets/actor_105700_animation_0F980_bank1.inc"
};

AnimationPackedRotation Actor05700_D0EA54[430] = {
#include "assets/actor_105700_animation_0F980_bank4.inc"
};

AnimationRecord Actor05700_D0F10C[531] = {
#include "assets/actor_105700_animation_0F980_records.inc"
};

u16 Actor05700_D0F958[20] = {
#include "assets/actor_105700_animation_0F980_indices.inc"
};

AnimationSet Actor05700_D0F980 = {
    Actor05700_D0F10C,
    Actor05700_D0F958,
    { NULL, Actor05700_D0E8EC, NULL, NULL, Actor05700_D0EA54, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D0F9A8[27] = {
#include "assets/actor_105700_animation_108D8_bank1.inc"
};

AnimationPackedRotation Actor05700_D0FAEC[408] = {
#include "assets/actor_105700_animation_108D8_bank4.inc"
};

AnimationRecord Actor05700_D1014C[473] = {
#include "assets/actor_105700_animation_108D8_records.inc"
};

u16 Actor05700_D108B0[20] = {
#include "assets/actor_105700_animation_108D8_indices.inc"
};

AnimationSet Actor05700_D108D8 = {
    Actor05700_D1014C,
    Actor05700_D108B0,
    { NULL, Actor05700_D0F9A8, NULL, NULL, Actor05700_D0FAEC, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D10900[8] = {
#include "assets/actor_105700_animation_10DFC_bank1.inc"
};

AnimationPackedRotation Actor05700_D10960[116] = {
#include "assets/actor_105700_animation_10DFC_bank4.inc"
};

AnimationRecord Actor05700_D10B30[169] = {
#include "assets/actor_105700_animation_10DFC_records.inc"
};

u16 Actor05700_D10DD4[20] = {
#include "assets/actor_105700_animation_10DFC_indices.inc"
};

AnimationSet Actor05700_D10DFC = {
    Actor05700_D10B30,
    Actor05700_D10DD4,
    { NULL, Actor05700_D10900, NULL, NULL, Actor05700_D10960, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D10E24[19] = {
#include "assets/actor_105700_animation_1180C_bank1.inc"
};

AnimationPackedRotation Actor05700_D10F08[255] = {
#include "assets/actor_105700_animation_1180C_bank4.inc"
};

AnimationRecord Actor05700_D11304[312] = {
#include "assets/actor_105700_animation_1180C_records.inc"
};

u16 Actor05700_D117E4[20] = {
#include "assets/actor_105700_animation_1180C_indices.inc"
};

AnimationSet Actor05700_D1180C = {
    Actor05700_D11304,
    Actor05700_D117E4,
    { NULL, Actor05700_D10E24, NULL, NULL, Actor05700_D10F08, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D11834[18] = {
#include "assets/actor_105700_animation_122B4_bank1.inc"
};

AnimationPackedRotation Actor05700_D1190C[281] = {
#include "assets/actor_105700_animation_122B4_bank4.inc"
};

AnimationRecord Actor05700_D11D70[327] = {
#include "assets/actor_105700_animation_122B4_records.inc"
};

u16 Actor05700_D1228C[20] = {
#include "assets/actor_105700_animation_122B4_indices.inc"
};

AnimationSet Actor05700_D122B4 = {
    Actor05700_D11D70,
    Actor05700_D1228C,
    { NULL, Actor05700_D11834, NULL, NULL, Actor05700_D1190C, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D122DC[8] = {
#include "assets/actor_105700_animation_12894_bank1.inc"
};

AnimationPackedRotation Actor05700_D1233C[130] = {
#include "assets/actor_105700_animation_12894_bank4.inc"
};

AnimationRecord Actor05700_D12544[202] = {
#include "assets/actor_105700_animation_12894_records.inc"
};

u16 Actor05700_D1286C[20] = {
#include "assets/actor_105700_animation_12894_indices.inc"
};

AnimationSet Actor05700_D12894 = {
    Actor05700_D12544,
    Actor05700_D1286C,
    { NULL, Actor05700_D122DC, NULL, NULL, Actor05700_D1233C, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D128BC[23] = {
#include "assets/actor_105700_animation_13588_bank1.inc"
};

AnimationPackedRotation Actor05700_D129D0[326] = {
#include "assets/actor_105700_animation_13588_bank4.inc"
};

AnimationRecord Actor05700_D12EE8[414] = {
#include "assets/actor_105700_animation_13588_records.inc"
};

u16 Actor05700_D13560[20] = {
#include "assets/actor_105700_animation_13588_indices.inc"
};

AnimationSet Actor05700_D13588 = {
    Actor05700_D12EE8,
    Actor05700_D13560,
    { NULL, Actor05700_D128BC, NULL, NULL, Actor05700_D129D0, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D135B0[31] = {
#include "assets/actor_105700_animation_14600_bank1.inc"
};

AnimationPackedRotation Actor05700_D13724[429] = {
#include "assets/actor_105700_animation_14600_bank4.inc"
};

AnimationRecord Actor05700_D13DD8[512] = {
#include "assets/actor_105700_animation_14600_records.inc"
};

u16 Actor05700_D145D8[20] = {
#include "assets/actor_105700_animation_14600_indices.inc"
};

AnimationSet Actor05700_D14600 = {
    Actor05700_D13DD8,
    Actor05700_D145D8,
    { NULL, Actor05700_D135B0, NULL, NULL, Actor05700_D13724, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D14628[9] = {
#include "assets/actor_105700_animation_14AD4_bank1.inc"
};

AnimationPackedRotation Actor05700_D14694[113] = {
#include "assets/actor_105700_animation_14AD4_bank4.inc"
};

AnimationRecord Actor05700_D14858[149] = {
#include "assets/actor_105700_animation_14AD4_records.inc"
};

u16 Actor05700_D14AAC[20] = {
#include "assets/actor_105700_animation_14AD4_indices.inc"
};

AnimationSet Actor05700_D14AD4 = {
    Actor05700_D14858,
    Actor05700_D14AAC,
    { NULL, Actor05700_D14628, NULL, NULL, Actor05700_D14694, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D14AFC[5] = {
#include "assets/actor_105700_animation_14DE4_bank1.inc"
};

AnimationPackedRotation Actor05700_D14B38[65] = {
#include "assets/actor_105700_animation_14DE4_bank4.inc"
};

AnimationRecord Actor05700_D14C3C[96] = {
#include "assets/actor_105700_animation_14DE4_records.inc"
};

u16 Actor05700_D14DBC[20] = {
#include "assets/actor_105700_animation_14DE4_indices.inc"
};

AnimationSet Actor05700_D14DE4 = {
    Actor05700_D14C3C,
    Actor05700_D14DBC,
    { NULL, Actor05700_D14AFC, NULL, NULL, Actor05700_D14B38, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D14E0C[2] = {
#include "assets/actor_105700_animation_14FC0_bank1.inc"
};

AnimationPackedRotation Actor05700_D14E24[17] = {
#include "assets/actor_105700_animation_14FC0_bank4.inc"
};

AnimationRecord Actor05700_D14E68[76] = {
#include "assets/actor_105700_animation_14FC0_records.inc"
};

u16 Actor05700_D14F98[20] = {
#include "assets/actor_105700_animation_14FC0_indices.inc"
};

AnimationSet Actor05700_D14FC0 = {
    Actor05700_D14E68,
    Actor05700_D14F98,
    { NULL, Actor05700_D14E0C, NULL, NULL, Actor05700_D14E24, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D14FE8[28] = {
#include "assets/actor_105700_animation_15F08_bank1.inc"
};

AnimationPackedRotation Actor05700_D15138[394] = {
#include "assets/actor_105700_animation_15F08_bank4.inc"
};

AnimationRecord Actor05700_D15760[480] = {
#include "assets/actor_105700_animation_15F08_records.inc"
};

u16 Actor05700_D15EE0[20] = {
#include "assets/actor_105700_animation_15F08_indices.inc"
};

AnimationSet Actor05700_D15F08 = {
    Actor05700_D15760,
    Actor05700_D15EE0,
    { NULL, Actor05700_D14FE8, NULL, NULL, Actor05700_D15138, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D15F30[9] = {
#include "assets/actor_105700_animation_1649C_bank1.inc"
};

AnimationPackedRotation Actor05700_D15F9C[127] = {
#include "assets/actor_105700_animation_1649C_bank4.inc"
};

AnimationRecord Actor05700_D16198[183] = {
#include "assets/actor_105700_animation_1649C_records.inc"
};

u16 Actor05700_D16474[20] = {
#include "assets/actor_105700_animation_1649C_indices.inc"
};

AnimationSet Actor05700_D1649C = {
    Actor05700_D16198,
    Actor05700_D16474,
    { NULL, Actor05700_D15F30, NULL, NULL, Actor05700_D15F9C, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D164C4[5] = {
#include "assets/actor_105700_animation_16764_bank1.inc"
};

AnimationPackedRotation Actor05700_D16500[56] = {
#include "assets/actor_105700_animation_16764_bank4.inc"
};

AnimationRecord Actor05700_D165E0[87] = {
#include "assets/actor_105700_animation_16764_records.inc"
};

u16 Actor05700_D1673C[20] = {
#include "assets/actor_105700_animation_16764_indices.inc"
};

AnimationSet Actor05700_D16764 = {
    Actor05700_D165E0,
    Actor05700_D1673C,
    { NULL, Actor05700_D164C4, NULL, NULL, Actor05700_D16500, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D1678C[2] = {
#include "assets/actor_105700_animation_16940_bank1.inc"
};

AnimationPackedRotation Actor05700_D167A4[17] = {
#include "assets/actor_105700_animation_16940_bank4.inc"
};

AnimationRecord Actor05700_D167E8[76] = {
#include "assets/actor_105700_animation_16940_records.inc"
};

u16 Actor05700_D16918[20] = {
#include "assets/actor_105700_animation_16940_indices.inc"
};

AnimationSet Actor05700_D16940 = {
    Actor05700_D167E8,
    Actor05700_D16918,
    { NULL, Actor05700_D1678C, NULL, NULL, Actor05700_D167A4, NULL, NULL, NULL },
};

AnimationPackedPose Actor05700_D16968[15] = {
#include "assets/actor_105700_animation_170CC_bank1.inc"
};

AnimationPackedRotation Actor05700_D16A1C[171] = {
#include "assets/actor_105700_animation_170CC_bank4.inc"
};

AnimationRecord Actor05700_D16CC8[247] = {
#include "assets/actor_105700_animation_170CC_records.inc"
};

u16 Actor05700_D170A4[20] = {
#include "assets/actor_105700_animation_170CC_indices.inc"
};

AnimationSet Actor05700_D170CC = {
    Actor05700_D16CC8,
    Actor05700_D170A4,
    { NULL, Actor05700_D16968, NULL, NULL, Actor05700_D16A1C, NULL, NULL, NULL },
};

DamageAttack gGolemPawnRookAttacks[5] = {
    { 30, 5 },
    { 20, 5 },
    { 0, 8 },
    { 20, 2 },
    { 30, 7 },
};

EnemyParams gGolemPawnRookParams[1] = {
    { gGolemPawnRookAttacks, 482, 250, 400, 8, 0, 6, 0, 0 },
};

s16 gGolemPawnRookWeakPointWeapons[46] = {
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
};

s16 gGolemPawnRookWeakPointPe[56] = {
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
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
    1,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
};

s32 gGolemPawnRookVoiceCues[17] = {
    0,
    0x40390001,
    0x40390002,
    0x40390003,
    0x40390004,
    0x40390005,
    0x40390006,
    0x4039000C,
    0x4039000D,
    0x40390009,
    0x4039000A,
    0x4039000B,
    0x4039000E,
    0x4039000F,
    0x40390010,
    0x40390011,
    0x40390012,
};

s32 gGolemPawnRookShotSound = 0x40390007;

s32 gGolemPawnRookImpactSound = 0x40390008;

s32 gGolemPawnRookScreamCue = 0x40390013;

s32 gGolemPawnRookSilenceCue = 0x40390014;

s32 gGolemPawnRookBurstCue = 0x40390015;

u16 Actor05700_D1723C[22] = {
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

u16 Actor05700_D17268[40] = {
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

u16 Actor05700_D172B8[40] = {
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

u16 Actor05700_D17308[50] = {
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

u16 Actor05700_D1736C[34] = {
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

u16* gGolemPawnRookAreaParams[6] = {
    NULL,
    Actor05700_D1723C,
    Actor05700_D17268,
    Actor05700_D172B8,
    Actor05700_D17308,
    Actor05700_D1736C,
};

s16 gGolemPawnRookBeamRibbonCorners[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

TaskDesc gGolemPawnRookTasks[4] = {
    { { { TASK_BODY_TMD, 96 } }, Actor05700_Fn05470, { .model = &Actor05700_D0A3D8 } },
    { { { TASK_BODY_TMD, 96 } }, Actor05700_Fn05040, { .model = &Actor05700_D0A824 } },
    { { { TASK_BODY_TMD, 96 } }, Actor05700_Fn0517C, { .model = &Actor05700_D0AE78 } },
    { { { TASK_BODY_TMD, 96 } }, Actor05700_Fn05270, { .model = &Actor05700_D0AB4C } },
};

AnimationSet* gGolemPawnRookAnimSets[31] = {
    NULL,
    &Actor05700_D0BAAC,
    &Actor05700_D0C414,
    &Actor05700_D0CA3C,
    &Actor05700_D0D23C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &Actor05700_D0D444,
    &Actor05700_D0DA50,
    &Actor05700_D0DF80,
    &Actor05700_D0E464,
    &Actor05700_D0E8C4,
    &Actor05700_D0F980,
    &Actor05700_D108D8,
    &Actor05700_D10DFC,
    &Actor05700_D1180C,
    &Actor05700_D122B4,
    &Actor05700_D12894,
    &Actor05700_D13588,
    &Actor05700_D14600,
    &Actor05700_D14AD4,
    &Actor05700_D14DE4,
    &Actor05700_D14FC0,
    &Actor05700_D15F08,
    &Actor05700_D1649C,
    &Actor05700_D16764,
    &Actor05700_D16940,
    &Actor05700_D170CC,
};

TaskFunc gGolemPawnRookStates[15] = {
    golemPawnRookIdleState,
    golemPawnRookApproachState,
    golemPawnRookLungeCycle,
    golemPawnRookNopState,
    golemPawnRookNopState,
    golemPawnRookSilenceScreamState,
    golemPawnRookCompanionCycle,
    golemPawnRookLungeStrikeState,
    golemPawnRookHitReactionState,
    golemPawnRookRecoilState,
    golemPawnRookFlagWaitState,
    golemPawnRookKnockdownState,
    golemPawnRookDownedShiftState,
    golemPawnRookCollapseState,
    golemPawnRookDownedFinishState,
};

extern s16 gGolemPawnRookBeamRibbonCorners[][4];

/// Places a fresh body block for the actor: allocates the 0xF0-byte work
/// block, builds the root coordinate by rotating the local spawn offset through
/// the parent coordinate and re-aiming it, then links the three collision
/// bodies and their `WorldCollisionContact` tables onto the model root and hands the light /
/// colour matrices to its `TmdObject`. The sound cue that marks the placement
/// packs the room/channel bits of the spawn context into `gGolemPawnRookShotSound`.
extern s32 gGolemPawnRookShotSound;

/// Sound id of the burst cue, with the spawn context's room/channel bits
/// packed in like `gGolemPawnRookShotSound`.
extern s32 gGolemPawnRookImpactSound;

extern s32 gGolemPawnRookScreamCue;

extern s32 gGolemPawnRookSilenceCue;

/// Animation bank the work block's animation context is started on.
extern AnimationSet* gGolemPawnRookAnimSets[31];

/// The actor's spawn table: entry 3 is the model child re-skinned with the
/// placement's texture page, entry 1 the effect child, and the whole table
/// is kept in `field_66C` for later spawns.
extern TaskDesc gGolemPawnRookTasks[];

/// Per-stage tables of per-area CD cue ids; a NULL stage has no cue.
extern u16* gGolemPawnRookAreaParams[];

/// Enemy parameter record the spawn hands to its `Enemy`.
extern EnemyParams gGolemPawnRookParams[];

/// Per-state handlers of the approach cycle, indexed by `field_6A6`.
extern TaskFunc gGolemPawnRookStates[];

/// Sound id the spawn cue is played against; the low byte comes from the
/// context block's room/channel bits.
extern s32 gGolemPawnRookBurstCue;

#include "../../shared/golem_pawn_rook_hit_tick.inc.c"

#include "../../shared/golem_pawn_rook_approach.inc.c"

#include "../../shared/golem_pawn_rook_proximity.inc.c"

#include "../../shared/golem_pawn_rook_knockdown.inc.c"

#include "../../shared/golem_pawn_rook_downed_shift.inc.c"

#include "../../shared/golem_pawn_rook_collapse.inc.c"

#include "../../shared/golem_pawn_rook_turn.inc.c"

#include "../../shared/golem_pawn_rook_hit_tilt.inc.c"

#include "../../shared/golem_pawn_rook_anim_cues.inc.c"

#include "../../shared/golem_pawn_rook_dead.inc.c"

#include "../../shared/golem_pawn_rook_companion_cycle.inc.c"

/// State handlers of the model child hung off the actor's part 7 - spawn,
/// per-frame tick and teardown - dispatched through by `Actor05700_Fn05040`.
static const GpEnemyTaskFuncTable3 Actor05700_D00080 = {
    golemPawnRookGunSpawn,
    golemPawnRookGunTick,
    Gp_DestroyEnemy,
};

#include "../../shared/golem_pawn_rook_lunge_strike.inc.c"

#include "../../shared/golem_pawn_rook_laser_sight.inc.c"

#include "../../shared/golem_pawn_rook_laser_beam.inc.c"

#include "../../shared/golem_pawn_rook_bullet_spawn.inc.c"

/// Per-frame tick of the placed effect body from `golemPawnRookBulletSpawn`.
/// Mode 0 of `gSceneCombatState.actorControl` drifts the root coordinate along its Y axis, puffs
/// an effect every fourth frame and ends the cycle - burst, sound cue and
/// state 2 - on a body hit, a surface that blocks probes, or after 0x5A frames.

#include "../../shared/golem_pawn_rook_bullet_fly.inc.c"

#include "../../shared/golem_pawn_rook_silence_scream.inc.c"

/// `actorTintModel` for a spawned enemy's model.
#include "../../shared/golem_pawn_rook_spawn.inc.c"

#include "../../shared/golem_pawn_rook_inlines.inc.c"

#include "../../shared/golem_pawn_rook_frame.inc.c"

#include "../../shared/golem_pawn_rook_lunge_cycle.inc.c"

#include "../../shared/player_detection_segment.inc.c"

#include "../../shared/golem_pawn_rook_idle.inc.c"

#include "../../shared/golem_pawn_rook_hit_reaction.inc.c"

#include "../../shared/golem_pawn_rook_recoil.inc.c"

#include "../../shared/golem_pawn_rook_flag_wait.inc.c"

#include "../../shared/golem_pawn_rook_downed_finish.inc.c"

#include "../../shared/golem_pawn_rook_nop.inc.c"

void Actor05700_Fn05040(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05700_D00080;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/golem_pawn_rook_gun_spawn.inc.c"

#include "../../shared/golem_pawn_rook_gun_tick.inc.c"

/// State handlers of the effect child - spawn/setup, per-frame tick and
/// teardown - dispatched through by `Actor05700_Fn0517C`.
static const GpEnemyTaskFuncTable3 Actor05700_D0008C = {
    golemPawnRookBulletSpawn,
    golemPawnRookBulletFly,
    golemPawnRookBulletDestroy,
};

void Actor05700_Fn0517C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05700_D0008C;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/golem_pawn_rook_bullet_destroy.inc.c"

/// State handlers of the burst child parented to the actor's part 11 - spawn,
/// per-frame tick and teardown - dispatched through by `Actor05700_Fn05270`.
static const GpEnemyTaskFuncTable3 Actor05700_D00098 = {
    golemPawnRookBurstPartSpawn,
    golemPawnRookBurstPartTick,
    Gp_DestroyEnemy,
};

void Actor05700_Fn05270(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05700_D00098;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/golem_pawn_rook_burst_part_spawn.inc.c"

/// The actor's own state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by `Actor05700_Fn05470`. The tick and
/// teardown take the task as the actor view it is.
static const GpEnemyTaskFuncTable3 Actor05700_D000A4 = {
    golemPawnRookSpawn,
    golemPawnRookFrameState,
    golemPawnRookDeadState,
};

#include "../../shared/golem_pawn_rook_burst_part.inc.c"

void Actor05700_Fn05470(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05700_D000A4;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
