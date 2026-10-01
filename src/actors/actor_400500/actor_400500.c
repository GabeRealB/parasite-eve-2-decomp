#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "actors/actors_shared_80131fc8.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/model_lighting.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
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
#include "main/mc.h"
#include "main/mc_types.h"
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
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/frame_capture.h"
#include "../../shared/coord_math.h"

/// State handlers copied onto the stack by func_actor_400500_80135770.
typedef struct Actor400500TaskFuncTable13 {
    TaskFunc funcs[13];
} Actor400500TaskFuncTable13;
STATIC_ASSERT_SIZEOF(Actor400500TaskFuncTable13, 0x34);

/// View-space sample written by `func_actor_400500_8013DBCC`: the X and Z of
/// the translation `Gp_WorldToLocal` produces for one of the actor's
/// coordinate nodes. `func_actor_400500_80132C54` passes
/// `Actor400500Work::field_9A0` as the destination, so the slot lives inside
/// the work block. Only `x` and `z` are ever written; the middle halfword is
/// kept so the layout matches the sibling `Actor400600ViewPos`.
typedef struct Actor400500ViewPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
} Actor400500ViewPos;
STATIC_ASSERT_SIZEOF(Actor400500ViewPos, 0x6);

/// Dual-width hit flags at `Actor400500Work` + 0x4C. Guards test bit 0 as a
/// halfword and then bits 0x102 as a word, the same shape as
/// `Actor341700Flags` / `ActorsShared8016974c`.
typedef union Actor400500HitFlags {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
} Actor400500HitFlags;
STATIC_ASSERT_SIZEOF(Actor400500HitFlags, 0x4);

/// Prefix view of `Actor400500Work` for the dual-width flags at +0x4C.
/// That address is `slots[1].flags` / `field_12` (0x10 into the second
/// animation slot), the same overlap `ActorsShared80168d3cWork` uses for
/// `flags_EC`. Bit 0x100 of the halfword is `ANIMATION_SLOT_SETTLED`.
typedef struct Actor400500HitView {
    /* 0x00 */ byte                pad[0x4C];
    /* 0x4C */ Actor400500HitFlags flags_4C;
} Actor400500HitView;

/// Per-actor state block for the `actor_400500` overlay.
///
/// `func_actor_400500_80135414` is the overlay's only allocator: it calls
/// `memCalloc(0xA50, 0)` and stores the result in the `Task::work` slot
/// (0x1C), which an enemy actor reuses for its own work block, so it is *not*
/// a `TaskIdMap` here. Reach it with `(Actor400500Work*)task->work`. The
/// same function hands `&work->lightMtx` / `&work->colorMtx` to the
/// `TmdObject` at `Task::extra` (`lightMtx` / `colorMtx`) and `work->rec0`
/// to `Enemy::recs`; the size below is the allocation, not a guess.
/// `obj1`/`obj2` share `rec1`; `obj3`/`obj4` share `rec2`.
///
/// `field_A06` / `field_A08` are the state and sub-state indices the handler
/// tables walk and `field_A04` is the per-state frame counter, mirroring
/// `Actor400600Work::field_71C` / `field_71E` / `field_718`.
///
/// The block opens with the 0x14-byte animation context and eighteen 0x28-byte
/// slots. `func_actor_400500_8013DC4C` walks slots 1..17, copies the low byte
/// of `field_9F8` into each slot's `field_9`, resets them from `field_9FE`,
/// and latches that id in `field_9FC`. The second slot's `flags` overlaps
/// `Actor400500HitView::flags_4C`.
typedef struct Actor400500Work {
    /* 0x000 */ AnimationContext      anim;
    /* 0x014 */ AnimationSlot         slots[0x12];
    /* 0x2E4 */ byte                  pad_2E4[0x524];
    /* 0x808 */ MATRIX                matrix_808; // model root coord, copied on the light-mode path
    /* 0x828 */ WorldCollisionBody    obj0;
    /* 0x848 */ WorldCollisionContact rec0[3];
    /* 0x890 */ WorldCollisionBody    obj1;
    /* 0x8B0 */ WorldCollisionBody    obj2;
    /* 0x8D0 */ WorldCollisionContact rec1[1];
    /* 0x8E8 */ WorldCollisionBody    obj3;
    /* 0x908 */ WorldCollisionBody    obj4;
    /* 0x928 */ WorldCollisionContact rec2[1];
    /* 0x940 */ EffectSpawnArg        eff_940; // part-3 coord, scale 0x100, count 3
    /* 0x948 */ s16                   field_948;
    /* 0x94A */ s16                   field_94A;
    /* 0x94C */ s16                   field_94C;
    /* 0x94E */ byte                  pad_94E[2];
    /* 0x950 */ u16                   field_950; // low half of root coord.t[0]
    /* 0x952 */ byte                  pad_952[2];
    /* 0x954 */ u16                   field_954; // low half of root coord.t[2]
    /* 0x956 */ byte                  pad_956[6];
    /* 0x95C */ MATRIX                colorMtx;  // TmdObject::colorMtx
    /* 0x97C */ MATRIX                lightMtx;  // TmdObject::lightMtx
    /* 0x99C */ byte                  pad_99C[4];
    /* 0x9A0 */ Actor400500ViewPos    field_9A0;
    /* 0x9A6 */ byte                  pad_9A6[0x16];
    /* 0x9BC */ s16                   field_9BC;
    /* 0x9BE */ byte                  pad_9BE[2];
    /* 0x9C0 */ VECTOR                field_9C0; // own position, copied from the model root coord.t
    /* 0x9D0 */ SVECTOR               field_9D0;
    /* 0x9D8 */ SVECTOR               field_9D8; // ApplyMatrixSV dest; vz is the former field_9DC
    /* 0x9E0 */ s16                   field_9E0;
    /* 0x9E2 */ s16                   field_9E2;
    /* 0x9E4 */ s16                   field_9E4;
    /* 0x9E6 */ byte                  pad_9E6[0xA];
    /* 0x9F0 */ Task*                 field_9F0[2]; // child tasks, killed on death
    /* 0x9F8 */ s16                   field_9F8;    // animation speed / step scale
    /* 0x9FA */ s16                   field_9FA;    // animation request kind
    /* 0x9FC */ u16                   field_9FC;    // last animation id the slots were reset to
    /* 0x9FE */ s16                   field_9FE;    // animation id
    /* 0xA00 */ s16                   field_A00;    // blend frame; incremented as u16, passed signed to 8013DD8C
    /* 0xA02 */ s16                   field_A02;    // identity scale written with the matrix copy
    /* 0xA04 */ u16                   field_A04;    // per-state frame counter
    /* 0xA06 */ u16                   field_A06;    // state index
    /* 0xA08 */ u16                   field_A08;    // sub-state index
    /* 0xA0A */ u16                   field_A0A;
    /* 0xA0C */ byte                  pad_A0C[2];
    /* 0xA0E */ s16                   field_A0E; // extra arg forwarded to func_800B4114, then cleared
    /* 0xA10 */ s16                   field_A10;
    /* 0xA12 */ s16                   field_A12;
    /* 0xA14 */ byte                  pad_A14[0x2];
    /* 0xA16 */ s16                   field_A16; // distance, compared to a range
    /* 0xA18 */ s16                   field_A18;
    /* 0xA1A */ s16                   field_A1A; // 1: sample part 0xE when heading is 0x400/0xC00
    /* 0xA1C */ u16                   field_A1C; // mode; 2, 3 and 6 take the heading-0 path
    /* 0xA1E */ u16                   field_A1E; // flags; bit 0x1 and bit 0x2 gate animations
    /* 0xA20 */ s16                   field_A20;
    /* 0xA22 */ u16                   field_A22; // frame counter used when field_A1C == 5
    /* 0xA24 */ s16                   field_A24; // copied to TmdObject::shading.colorBlend
    /* 0xA26 */ u16                   field_A26; // heading countdown, decremented by 0x80
    /* 0xA28 */ s16                   field_A28;
    /* 0xA2A */ s16                   field_A2A; // fade sub-state timer
    /* 0xA2C */ s16                   field_A2C; // countdown written with message kind 1
    /* 0xA2E */ s16                   field_A2E; // duration copied onto field_A30
    /* 0xA30 */ s16                   field_A30; // blocks setting field_A46 to 0x80 while nonzero
    /* 0xA32 */ s16                   field_A32; // heading; >>3 as u16, compared to 0 as s16
    /* 0xA34 */ s16                   field_A34; // gates the field_A1A==3 sub-state write
    /* 0xA36 */ u16                   field_A36; // angle, range-tested as (a - 0x300) <= 0xA00
    /* 0xA38 */ s16                   field_A38;
    /* 0xA3A */ s16                   field_A3A;
    /* 0xA3C */ s16                   field_A3C;
    /* 0xA3E */ s16                   field_A3E;
    /* 0xA40 */ s16                   field_A40;
    /* 0xA42 */ s16                   field_A42;
    /* 0xA44 */ s16                   field_A44; // hit cooldown
    /* 0xA46 */ s8                    field_A46; // signed flag; 0x81 means active mode 1
    /* 0xA47 */ s8                    field_A47;
    /* 0xA48 */ s8                    field_A48; // session-message handshake state
    /* 0xA49 */ s8                    field_A49;
    /* 0xA4A */ s8                    field_A4A;
    /* 0xA4B */ s8                    field_A4B; // last message kind 1..4
    /* 0xA4C */ s8                    field_A4C; // set with kind 1
    /* 0xA4D */ u8                    field_A4D; // selects message 0x3FF instead of 0x3F4
    /* 0xA4E */ byte                  pad_A4E[2];
} Actor400500Work;
STATIC_ASSERT_SIZEOF(Actor400500Work, 0xA50);

extern ActorZone D_actor_400500_80153D6C[];

/// Still called by actor_206100, which includes this header; remove once that
/// entry is demoted.
void        ActorsShared80132c4c(MATRIX* src, MATRIX* dst);
static void func_actor_400500_80132628(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, s32 shade);
static void func_actor_400500_80138088(Task* task);
static s32  func_actor_400500_8013B720(GfxCoord* coord, MATRIX* matrix);

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `TmdObject` loads that
 * follow. Declared as a scalar, `func_actor_400500_80134B88` scores 87.27%
 * (12 register and 8 reorder penalties); as a one-element array it is exact,
 * the same remedy `actor_400600` needed for the same global. */
extern void* D_800678F0[1];

/* The records closing three of the overlay's model streams, selected through
   `D_800678F0`. */
extern TmdSource D_actor_400500_8014393C;
extern TmdSource D_actor_400500_80143F40;
extern TmdSource D_actor_400500_80144624;

extern EnemyParams D_actor_400500_80153C90;
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s32, u16*);
    } handler;
} Actor400500MessageEntry;
STATIC_ASSERT_SIZEOF(Actor400500MessageEntry, 8);

extern Actor400500MessageEntry D_actor_400500_80153CA0[2];
extern u8                      D_actor_400500_80153CC0[];
extern TaskDesc                D_actor_400500_80153D48[];
extern u16                     D_actor_400500_80153DB4[];
extern u8                      D_actor_400500_80153DD4[];

static void func_actor_400500_80132438(Task* arg0);
static void func_actor_400500_80132AB0(Task* arg0, s16 arg1, s32 arg2);
static s32  func_actor_400500_80132D74(Task* arg0);
static void func_actor_400500_80132E94(Task* arg0);
static s32  func_actor_400500_80133160(Task* arg0);
static s32  func_actor_400500_80133358(Task* arg0);
static s32  func_actor_400500_80133460(Task* arg0);
static void func_actor_400500_801335E8(Task* arg0);
static void func_actor_400500_80133B14(Task* arg0);
static void func_actor_400500_8013403C(Task* arg0);
static void func_actor_400500_8013456C(Task* arg0);
static void func_actor_400500_80135770(Task* arg0);
static void func_actor_400500_80135EBC(Task* arg0);
static void func_actor_400500_801361EC(Task* arg0);
static void func_actor_400500_8013662C(Task* arg0);
static void func_actor_400500_80136864(Task* arg0);
static void func_actor_400500_801369A4(Task* arg0);
static void func_actor_400500_80136B94(Task* arg0);
static void func_actor_400500_80136D00(Task* arg0);
static void func_actor_400500_80136EB8(Task* arg0);
static void func_actor_400500_80137034(Task* arg0);
static void func_actor_400500_801371A0(Task* arg0);
static void func_actor_400500_80137338(Task* arg0);
static void func_actor_400500_80137478(Task* arg0);
static void func_actor_400500_801385D0(Task* arg0);
static void func_actor_400500_801387E8(Task* arg0);
static void func_actor_400500_8013899C(Task* arg0);
static void func_actor_400500_80138B78(Task* arg0);
static void func_actor_400500_80138CE8(Task* arg0);
static void func_actor_400500_80138DC4(Task* arg0);
static void func_actor_400500_80138EA0(Task* arg0);
static void func_actor_400500_8013905C(Task* arg0);
static void func_actor_400500_801391B0(Task* arg0);
static void func_actor_400500_801392D8(Task* arg0);
static void func_actor_400500_80139448(Task* arg0);
static void func_actor_400500_801395D0(Task* arg0);
static void func_actor_400500_8013973C(Task* arg0);
static void func_actor_400500_80139AC4(Task* arg0);
static void func_actor_400500_80139C1C(Task* arg0);
static void func_actor_400500_80139D70(Task* arg0);
static void func_actor_400500_80139F6C(Task* arg0);
static void func_actor_400500_8013A0B8(Task* arg0);
static void func_actor_400500_8013A484(Task* arg0);
static void func_actor_400500_8013A5D8(Task* arg0);
static void func_actor_400500_8013A700(Task* arg0);
static void func_actor_400500_8013A8E4(Task* arg0);
static void func_actor_400500_8013AA98(Task* arg0);
static void func_actor_400500_8013ABE4(Task* arg0);
static void func_actor_400500_8013AD60(Task* arg0);
static void func_actor_400500_8013AF44(Task* arg0);
static void func_actor_400500_8013B228(Task* arg0);
static void func_actor_400500_8013B374(Task* arg0);
static void func_actor_400500_8013B4A4(Task* arg0);
static void func_actor_400500_8013B5E0(Task* arg0);
static void func_actor_400500_8013BA24(Task* arg0);
static void func_actor_400500_8013BAA4(Task* arg0);
static void func_actor_400500_8013BB18(Task* arg0);
static void func_actor_400500_8013BBB0(Task* arg0);
static void func_actor_400500_8013BC9C(Task* arg0);
static void func_actor_400500_8013BCCC(Task* arg0);
static void func_actor_400500_8013BD64(Task* arg0);
static void func_actor_400500_8013BE50(Task* arg0);
static void func_actor_400500_8013BEC4(Task* arg0);
static void func_actor_400500_8013BFB0(Task* arg0);
static void func_actor_400500_8013C018(Task* arg0);
static void func_actor_400500_8013C108(Task* arg0);
static void func_actor_400500_8013C174(Task* arg0);
static void func_actor_400500_8013C218(Task* arg0);
static void func_actor_400500_8013C348(Task* arg0);
static void func_actor_400500_8013C3C4(Task* arg0);
static void func_actor_400500_8013C474(Task* arg0);
static void func_actor_400500_8013C508(Task* arg0);
static void func_actor_400500_8013C578(Task* arg0);
static void func_actor_400500_8013C61C(Task* arg0);
static void func_actor_400500_8013C750(Task* arg0);
static void func_actor_400500_8013C7A4(Task* arg0);
static void func_actor_400500_8013C818(Task* task);
static void func_actor_400500_8013C908(Task* arg0);
static void func_actor_400500_8013C9D4(Task* arg0);
static void func_actor_400500_8013CA38(Task* arg0);
static void func_actor_400500_8013CB0C(Task* arg0);
static void func_actor_400500_8013CBD8(Task* arg0);
static void func_actor_400500_8013CCDC(Task* arg0);
static void func_actor_400500_8013CDA8(Task* arg0);
static void func_actor_400500_8013CE9C(Task* arg0);
static void func_actor_400500_8013CF68(Task* arg0);
static void func_actor_400500_8013D078(Task* arg0);
static void func_actor_400500_8013D144(Task* arg0);
static void func_actor_400500_8013D210(Task* arg0);
static void func_actor_400500_8013D274(Task* arg0);
static void func_actor_400500_8013D2D8(Task* arg0);
static void func_actor_400500_8013D3B8(Task* arg0);
static void func_actor_400500_8013D420(Task* arg0);
static void func_actor_400500_8013D4F0(Task* arg0);
static void func_actor_400500_8013D59C(Task* arg0);
static void func_actor_400500_8013D630(Task* arg0);
static void func_actor_400500_8013D6A0(Task* arg0);
static void func_actor_400500_8013D744(Task* arg0);
static void func_actor_400500_8013D878(Task* arg0);
static void func_actor_400500_8013D8CC(Task* arg0);
static void func_actor_400500_8013D958(Task* arg0);
static void func_actor_400500_8013D9DC(Task* arg0);
static void func_actor_400500_8013D9F4(Task* arg0);
static void func_actor_400500_8013DA24(Task* arg0);
static void func_actor_400500_8013DA68(Task* arg0);
static void func_actor_400500_8013DACC(Task* arg0);
static void func_actor_400500_8013DB64(Task* arg0, s16 arg1);
static s32  func_actor_400500_8013DB78(Task* arg0);
static void func_actor_400500_8013DBCC(Task* arg0, s16 arg1, Actor400500ViewPos* arg2);
static void func_actor_400500_8013DC4C(Task* arg0);
static void func_actor_400500_8013DCBC(Task* arg0, s16 arg1, s16 arg2);
static void func_actor_400500_8013DCD4(Task* arg0);
static s32  func_actor_400500_8013DD8C(Task* arg0, s16 arg1);
static s32  func_actor_400500_8013DDEC(Task* arg0);
static void func_actor_400500_8013DE2C(MATRIX* src, MATRIX* dst);
static void func_actor_400500_8013DEFC(Task* arg0);
static void func_actor_400500_8013DF50(Task* arg0);
static void func_actor_400500_8013DF74(Task* arg0);
static void func_actor_400500_8013DFE4(Task* arg0);

extern TmdSource D_actor_400500_80142C20;
void             func_actor_400500_8013DE98(Task*);

extern TmdSource D_actor_400500_80142E70;
extern TmdSource D_actor_400500_801430E8;
void             func_actor_400500_8013DAE4(Task*, s32, u16*);
void             func_actor_400500_8013DF64(Task*);
void             func_actor_400500_8013DF6C(Task*);

TmdBone D_actor_400500_8013E038[18] = {
#include "assets/actor_400500_model_10E00_skeleton.inc"
};

u32 D_actor_400500_8013E2C0[18] = {
#include "assets/actor_400500_model_10E00_partVerts.inc"
};

SVECTOR D_actor_400500_8013E308[257] = {
#include "assets/actor_400500_model_10E00_verts.inc"
};

SVECTOR D_actor_400500_8013EB10[249] = {
#include "assets/actor_400500_model_10E00_normals.inc"
};

u32 D_actor_400500_8013F2D8[3666] = {
#include "assets/actor_400500_model_10E00_stream.inc"
};

TmdSource D_actor_400500_80142C20 = {
    0,
    36152,
    13736,
    18,
    D_actor_400500_8013E2C0,
    D_actor_400500_8013E308,
    D_actor_400500_8013EB10,
    D_actor_400500_8013E038,
    D_actor_400500_8013F2D8,
};

TmdBone D_actor_400500_80142C44[1] = {
#include "assets/actor_400500_model_11050_skeleton.inc"
};

u32 D_actor_400500_80142C68[1] = {
#include "assets/actor_400500_model_11050_partVerts.inc"
};

SVECTOR D_actor_400500_80142C6C[10] = {
#include "assets/actor_400500_model_11050_verts.inc"
};

SVECTOR D_actor_400500_80142CBC[12] = {
#include "assets/actor_400500_model_11050_normals.inc"
};

u32 D_actor_400500_80142D1C[85] = {
#include "assets/actor_400500_model_11050_stream.inc"
};

TmdSource D_actor_400500_80142E70 = {
    0,
    528,
    0,
    1,
    D_actor_400500_80142C68,
    D_actor_400500_80142C6C,
    D_actor_400500_80142CBC,
    D_actor_400500_80142C44,
    D_actor_400500_80142D1C,
};

TmdBone D_actor_400500_80142E94[1] = {
#include "assets/actor_400500_model_112C8_skeleton.inc"
};

u32 D_actor_400500_80142EB8[1] = {
#include "assets/actor_400500_model_112C8_partVerts.inc"
};

SVECTOR D_actor_400500_80142EBC[10] = {
#include "assets/actor_400500_model_112C8_verts.inc"
};

SVECTOR D_actor_400500_80142F0C[17] = {
#include "assets/actor_400500_model_112C8_normals.inc"
};

u32 D_actor_400500_80142F94[85] = {
#include "assets/actor_400500_model_112C8_stream.inc"
};

TmdSource D_actor_400500_801430E8 = {
    0,
    528,
    0,
    1,
    D_actor_400500_80142EB8,
    D_actor_400500_80142EBC,
    D_actor_400500_80142F0C,
    D_actor_400500_80142E94,
    D_actor_400500_80142F94,
};

TmdBone D_actor_400500_8014310C[1] = {
#include "assets/actor_400500_model_11B1C_skeleton.inc"
};

u32 D_actor_400500_80143130[1] = {
#include "assets/actor_400500_model_11B1C_partVerts.inc"
};

SVECTOR D_actor_400500_80143134[36] = {
#include "assets/actor_400500_model_11B1C_verts.inc"
};

SVECTOR D_actor_400500_80143254[50] = {
#include "assets/actor_400500_model_11B1C_normals.inc"
};

u32 D_actor_400500_801433E4[342] = {
#include "assets/actor_400500_model_11B1C_stream.inc"
};

TmdSource D_actor_400500_8014393C = {
    0,
    2300,
    0,
    1,
    D_actor_400500_80143130,
    D_actor_400500_80143134,
    D_actor_400500_80143254,
    D_actor_400500_8014310C,
    D_actor_400500_801433E4,
};

TmdBone D_actor_400500_80143960[1] = {
#include "assets/actor_400500_model_12120_skeleton.inc"
};

u32 D_actor_400500_80143984[1] = {
#include "assets/actor_400500_model_12120_partVerts.inc"
};

SVECTOR D_actor_400500_80143988[25] = {
#include "assets/actor_400500_model_12120_verts.inc"
};

SVECTOR D_actor_400500_80143A50[33] = {
#include "assets/actor_400500_model_12120_normals.inc"
};

u32 D_actor_400500_80143B58[250] = {
#include "assets/actor_400500_model_12120_stream.inc"
};

TmdSource D_actor_400500_80143F40 = {
    0,
    1644,
    0,
    1,
    D_actor_400500_80143984,
    D_actor_400500_80143988,
    D_actor_400500_80143A50,
    D_actor_400500_80143960,
    D_actor_400500_80143B58,
};

TmdBone D_actor_400500_80143F64[1] = {
#include "assets/actor_400500_model_12804_skeleton.inc"
};

u32 D_actor_400500_80143F88[1] = {
#include "assets/actor_400500_model_12804_partVerts.inc"
};

SVECTOR D_actor_400500_80143F8C[31] = {
#include "assets/actor_400500_model_12804_verts.inc"
};

SVECTOR D_actor_400500_80144084[39] = {
#include "assets/actor_400500_model_12804_normals.inc"
};

u32 D_actor_400500_801441BC[282] = {
#include "assets/actor_400500_model_12804_stream.inc"
};

TmdSource D_actor_400500_80144624 = {
    0,
    1900,
    0,
    1,
    D_actor_400500_80143F88,
    D_actor_400500_80143F8C,
    D_actor_400500_80144084,
    D_actor_400500_80143F64,
    D_actor_400500_801441BC,
};

AnimationPackedPose D_actor_400500_80144648[26] = {
#include "assets/actor_400500_animation_134F0_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80144780[323] = {
#include "assets/actor_400500_animation_134F0_bank4.inc"
};

AnimationRecord D_actor_400500_80144C8C[408] = {
#include "assets/actor_400500_animation_134F0_records.inc"
};

u16 D_actor_400500_801452EC[18] = {
#include "assets/actor_400500_animation_134F0_indices.inc"
};

AnimationSet D_actor_400500_80145310 = {
    D_actor_400500_80144C8C,
    D_actor_400500_801452EC,
    { NULL, D_actor_400500_80144648, NULL, NULL, D_actor_400500_80144780, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80145338[22] = {
#include "assets/actor_400500_animation_13C28_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80145440[156] = {
#include "assets/actor_400500_animation_13C28_bank4.inc"
};

AnimationRecord D_actor_400500_801456B0[221] = {
#include "assets/actor_400500_animation_13C28_records.inc"
};

u16 D_actor_400500_80145A24[18] = {
#include "assets/actor_400500_animation_13C28_indices.inc"
};

AnimationSet D_actor_400500_80145A48 = {
    D_actor_400500_801456B0,
    D_actor_400500_80145A24,
    { NULL, D_actor_400500_80145338, NULL, NULL, D_actor_400500_80145440, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80145A70[14] = {
#include "assets/actor_400500_animation_1430C_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80145B18[157] = {
#include "assets/actor_400500_animation_1430C_bank4.inc"
};

AnimationRecord D_actor_400500_80145D8C[223] = {
#include "assets/actor_400500_animation_1430C_records.inc"
};

u16 D_actor_400500_80146108[18] = {
#include "assets/actor_400500_animation_1430C_indices.inc"
};

AnimationSet D_actor_400500_8014612C = {
    D_actor_400500_80145D8C,
    D_actor_400500_80146108,
    { NULL, D_actor_400500_80145A70, NULL, NULL, D_actor_400500_80145B18, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80146154[25] = {
#include "assets/actor_400500_animation_14BA4_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80146280[191] = {
#include "assets/actor_400500_animation_14BA4_bank4.inc"
};

AnimationRecord D_actor_400500_8014657C[265] = {
#include "assets/actor_400500_animation_14BA4_records.inc"
};

u16 D_actor_400500_801469A0[18] = {
#include "assets/actor_400500_animation_14BA4_indices.inc"
};

AnimationSet D_actor_400500_801469C4 = {
    D_actor_400500_8014657C,
    D_actor_400500_801469A0,
    { NULL, D_actor_400500_80146154, NULL, NULL, D_actor_400500_80146280, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_801469EC[26] = {
#include "assets/actor_400500_animation_15A60_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80146B24[380] = {
#include "assets/actor_400500_animation_15A60_bank4.inc"
};

AnimationRecord D_actor_400500_80147114[466] = {
#include "assets/actor_400500_animation_15A60_records.inc"
};

u16 D_actor_400500_8014785C[18] = {
#include "assets/actor_400500_animation_15A60_indices.inc"
};

AnimationSet D_actor_400500_80147880 = {
    D_actor_400500_80147114,
    D_actor_400500_8014785C,
    { NULL, D_actor_400500_801469EC, NULL, NULL, D_actor_400500_80146B24, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_801478A8[30] = {
#include "assets/actor_400500_animation_168E0_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80147A10[371] = {
#include "assets/actor_400500_animation_168E0_bank4.inc"
};

AnimationRecord D_actor_400500_80147FDC[448] = {
#include "assets/actor_400500_animation_168E0_records.inc"
};

u16 D_actor_400500_801486DC[18] = {
#include "assets/actor_400500_animation_168E0_indices.inc"
};

AnimationSet D_actor_400500_80148700 = {
    D_actor_400500_80147FDC,
    D_actor_400500_801486DC,
    { NULL, D_actor_400500_801478A8, NULL, NULL, D_actor_400500_80147A10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80148728[17] = {
#include "assets/actor_400500_animation_171A0_bank1.inc"
};

AnimationPackedRotation D_actor_400500_801487F4[214] = {
#include "assets/actor_400500_animation_171A0_bank4.inc"
};

AnimationRecord D_actor_400500_80148B4C[276] = {
#include "assets/actor_400500_animation_171A0_records.inc"
};

u16 D_actor_400500_80148F9C[18] = {
#include "assets/actor_400500_animation_171A0_indices.inc"
};

AnimationSet D_actor_400500_80148FC0 = {
    D_actor_400500_80148B4C,
    D_actor_400500_80148F9C,
    { NULL, D_actor_400500_80148728, NULL, NULL, D_actor_400500_801487F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80148FE8[19] = {
#include "assets/actor_400500_animation_17B18_bank1.inc"
};

AnimationPackedRotation D_actor_400500_801490CC[233] = {
#include "assets/actor_400500_animation_17B18_bank4.inc"
};

AnimationRecord D_actor_400500_80149470[297] = {
#include "assets/actor_400500_animation_17B18_records.inc"
};

u16 D_actor_400500_80149914[18] = {
#include "assets/actor_400500_animation_17B18_indices.inc"
};

AnimationSet D_actor_400500_80149938 = {
    D_actor_400500_80149470,
    D_actor_400500_80149914,
    { NULL, D_actor_400500_80148FE8, NULL, NULL, D_actor_400500_801490CC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80149960[9] = {
#include "assets/actor_400500_animation_17FEC_bank1.inc"
};

AnimationPackedRotation D_actor_400500_801499CC[114] = {
#include "assets/actor_400500_animation_17FEC_bank4.inc"
};

AnimationRecord D_actor_400500_80149B94[149] = {
#include "assets/actor_400500_animation_17FEC_records.inc"
};

u16 D_actor_400500_80149DE8[18] = {
#include "assets/actor_400500_animation_17FEC_indices.inc"
};

AnimationSet D_actor_400500_80149E0C = {
    D_actor_400500_80149B94,
    D_actor_400500_80149DE8,
    { NULL, D_actor_400500_80149960, NULL, NULL, D_actor_400500_801499CC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80149E34[23] = {
#include "assets/actor_400500_animation_18BCC_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80149F48[295] = {
#include "assets/actor_400500_animation_18BCC_bank4.inc"
};

AnimationRecord D_actor_400500_8014A3E4[377] = {
#include "assets/actor_400500_animation_18BCC_records.inc"
};

u16 D_actor_400500_8014A9C8[18] = {
#include "assets/actor_400500_animation_18BCC_indices.inc"
};

AnimationSet D_actor_400500_8014A9EC = {
    D_actor_400500_8014A3E4,
    D_actor_400500_8014A9C8,
    { NULL, D_actor_400500_80149E34, NULL, NULL, D_actor_400500_80149F48, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8014AA14[9] = {
#include "assets/actor_400500_animation_19124_bank1.inc"
};

AnimationPackedRotation D_actor_400500_8014AA80[125] = {
#include "assets/actor_400500_animation_19124_bank4.inc"
};

AnimationRecord D_actor_400500_8014AC74[171] = {
#include "assets/actor_400500_animation_19124_records.inc"
};

u16 D_actor_400500_8014AF20[18] = {
#include "assets/actor_400500_animation_19124_indices.inc"
};

AnimationSet D_actor_400500_8014AF44 = {
    D_actor_400500_8014AC74,
    D_actor_400500_8014AF20,
    { NULL, D_actor_400500_8014AA14, NULL, NULL, D_actor_400500_8014AA80, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8014AF6C[18] = {
#include "assets/actor_400500_animation_19AE4_bank1.inc"
};

AnimationPackedRotation D_actor_400500_8014B044[242] = {
#include "assets/actor_400500_animation_19AE4_bank4.inc"
};

AnimationRecord D_actor_400500_8014B40C[309] = {
#include "assets/actor_400500_animation_19AE4_records.inc"
};

u16 D_actor_400500_8014B8E0[18] = {
#include "assets/actor_400500_animation_19AE4_indices.inc"
};

AnimationSet D_actor_400500_8014B904 = {
    D_actor_400500_8014B40C,
    D_actor_400500_8014B8E0,
    { NULL, D_actor_400500_8014AF6C, NULL, NULL, D_actor_400500_8014B044, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8014B92C[9] = {
#include "assets/actor_400500_animation_19F10_bank1.inc"
};

AnimationPackedRotation D_actor_400500_8014B998[95] = {
#include "assets/actor_400500_animation_19F10_bank4.inc"
};

AnimationRecord D_actor_400500_8014BB14[126] = {
#include "assets/actor_400500_animation_19F10_records.inc"
};

u16 D_actor_400500_8014BD0C[18] = {
#include "assets/actor_400500_animation_19F10_indices.inc"
};

AnimationSet D_actor_400500_8014BD30 = {
    D_actor_400500_8014BB14,
    D_actor_400500_8014BD0C,
    { NULL, D_actor_400500_8014B92C, NULL, NULL, D_actor_400500_8014B998, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8014BD58[9] = {
#include "assets/actor_400500_animation_1A47C_bank1.inc"
};

AnimationPackedRotation D_actor_400500_8014BDC4[136] = {
#include "assets/actor_400500_animation_1A47C_bank4.inc"
};

AnimationRecord D_actor_400500_8014BFE4[165] = {
#include "assets/actor_400500_animation_1A47C_records.inc"
};

u16 D_actor_400500_8014C278[18] = {
#include "assets/actor_400500_animation_1A47C_indices.inc"
};

AnimationSet D_actor_400500_8014C29C = {
    D_actor_400500_8014BFE4,
    D_actor_400500_8014C278,
    { NULL, D_actor_400500_8014BD58, NULL, NULL, D_actor_400500_8014BDC4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8014C2C4[15] = {
#include "assets/actor_400500_animation_1AE34_bank1.inc"
};

AnimationPackedRotation D_actor_400500_8014C378[227] = {
#include "assets/actor_400500_animation_1AE34_bank4.inc"
};

AnimationRecord D_actor_400500_8014C704[331] = {
#include "assets/actor_400500_animation_1AE34_records.inc"
};

u16 D_actor_400500_8014CC30[18] = {
#include "assets/actor_400500_animation_1AE34_indices.inc"
};

AnimationSet D_actor_400500_8014CC54 = {
    D_actor_400500_8014C704,
    D_actor_400500_8014CC30,
    { NULL, D_actor_400500_8014C2C4, NULL, NULL, D_actor_400500_8014C378, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8014CC7C[10] = {
#include "assets/actor_400500_animation_1B3A0_bank1.inc"
};

AnimationPackedRotation D_actor_400500_8014CCF4[129] = {
#include "assets/actor_400500_animation_1B3A0_bank4.inc"
};

AnimationRecord D_actor_400500_8014CEF8[169] = {
#include "assets/actor_400500_animation_1B3A0_records.inc"
};

u16 D_actor_400500_8014D19C[18] = {
#include "assets/actor_400500_animation_1B3A0_indices.inc"
};

AnimationSet D_actor_400500_8014D1C0 = {
    D_actor_400500_8014CEF8,
    D_actor_400500_8014D19C,
    { NULL, D_actor_400500_8014CC7C, NULL, NULL, D_actor_400500_8014CCF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8014D1E8[16] = {
#include "assets/actor_400500_animation_1C12C_bank1.inc"
};

AnimationPackedRotation D_actor_400500_8014D2A8[363] = {
#include "assets/actor_400500_animation_1C12C_bank4.inc"
};

AnimationRecord D_actor_400500_8014D854[437] = {
#include "assets/actor_400500_animation_1C12C_records.inc"
};

u16 D_actor_400500_8014DF28[18] = {
#include "assets/actor_400500_animation_1C12C_indices.inc"
};

AnimationSet D_actor_400500_8014DF4C = {
    D_actor_400500_8014D854,
    D_actor_400500_8014DF28,
    { NULL, D_actor_400500_8014D1E8, NULL, NULL, D_actor_400500_8014D2A8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8014DF74[12] = {
#include "assets/actor_400500_animation_1C7BC_bank1.inc"
};

AnimationPackedRotation D_actor_400500_8014E004[167] = {
#include "assets/actor_400500_animation_1C7BC_bank4.inc"
};

AnimationRecord D_actor_400500_8014E2A0[198] = {
#include "assets/actor_400500_animation_1C7BC_records.inc"
};

u16 D_actor_400500_8014E5B8[18] = {
#include "assets/actor_400500_animation_1C7BC_indices.inc"
};

AnimationSet D_actor_400500_8014E5DC = {
    D_actor_400500_8014E2A0,
    D_actor_400500_8014E5B8,
    { NULL, D_actor_400500_8014DF74, NULL, NULL, D_actor_400500_8014E004, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8014E604[5] = {
#include "assets/actor_400500_animation_1CB44_bank1.inc"
};

AnimationPackedRotation D_actor_400500_8014E640[79] = {
#include "assets/actor_400500_animation_1CB44_bank4.inc"
};

AnimationRecord D_actor_400500_8014E77C[113] = {
#include "assets/actor_400500_animation_1CB44_records.inc"
};

u16 D_actor_400500_8014E940[18] = {
#include "assets/actor_400500_animation_1CB44_indices.inc"
};

AnimationSet D_actor_400500_8014E964 = {
    D_actor_400500_8014E77C,
    D_actor_400500_8014E940,
    { NULL, D_actor_400500_8014E604, NULL, NULL, D_actor_400500_8014E640, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8014E98C[11] = {
#include "assets/actor_400500_animation_1D0E4_bank1.inc"
};

AnimationPackedRotation D_actor_400500_8014EA10[139] = {
#include "assets/actor_400500_animation_1D0E4_bank4.inc"
};

AnimationRecord D_actor_400500_8014EC3C[169] = {
#include "assets/actor_400500_animation_1D0E4_records.inc"
};

u16 D_actor_400500_8014EEE0[18] = {
#include "assets/actor_400500_animation_1D0E4_indices.inc"
};

AnimationSet D_actor_400500_8014EF04 = {
    D_actor_400500_8014EC3C,
    D_actor_400500_8014EEE0,
    { NULL, D_actor_400500_8014E98C, NULL, NULL, D_actor_400500_8014EA10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8014EF2C[10] = {
#include "assets/actor_400500_animation_1D560_bank1.inc"
};

AnimationPackedRotation D_actor_400500_8014EFA4[104] = {
#include "assets/actor_400500_animation_1D560_bank4.inc"
};

AnimationRecord D_actor_400500_8014F144[134] = {
#include "assets/actor_400500_animation_1D560_records.inc"
};

u16 D_actor_400500_8014F35C[18] = {
#include "assets/actor_400500_animation_1D560_indices.inc"
};

AnimationSet D_actor_400500_8014F380 = {
    D_actor_400500_8014F144,
    D_actor_400500_8014F35C,
    { NULL, D_actor_400500_8014EF2C, NULL, NULL, D_actor_400500_8014EFA4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8014F3A8[40] = {
#include "assets/actor_400500_animation_1E0CC_bank1.inc"
};

AnimationPackedRotation D_actor_400500_8014F588[266] = {
#include "assets/actor_400500_animation_1E0CC_bank4.inc"
};

AnimationRecord D_actor_400500_8014F9B0[326] = {
#include "assets/actor_400500_animation_1E0CC_records.inc"
};

u16 D_actor_400500_8014FEC8[18] = {
#include "assets/actor_400500_animation_1E0CC_indices.inc"
};

AnimationSet D_actor_400500_8014FEEC = {
    D_actor_400500_8014F9B0,
    D_actor_400500_8014FEC8,
    { NULL, D_actor_400500_8014F3A8, NULL, NULL, D_actor_400500_8014F588, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8014FF14[20] = {
#include "assets/actor_400500_animation_1E718_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80150004[133] = {
#include "assets/actor_400500_animation_1E718_bank4.inc"
};

AnimationRecord D_actor_400500_80150218[191] = {
#include "assets/actor_400500_animation_1E718_records.inc"
};

u16 D_actor_400500_80150514[18] = {
#include "assets/actor_400500_animation_1E718_indices.inc"
};

AnimationSet D_actor_400500_80150538 = {
    D_actor_400500_80150218,
    D_actor_400500_80150514,
    { NULL, D_actor_400500_8014FF14, NULL, NULL, D_actor_400500_80150004, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80150560[16] = {
#include "assets/actor_400500_animation_1ECA4_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80150620[117] = {
#include "assets/actor_400500_animation_1ECA4_bank4.inc"
};

AnimationRecord D_actor_400500_801507F4[171] = {
#include "assets/actor_400500_animation_1ECA4_records.inc"
};

u16 D_actor_400500_80150AA0[18] = {
#include "assets/actor_400500_animation_1ECA4_indices.inc"
};

AnimationSet D_actor_400500_80150AC4 = {
    D_actor_400500_801507F4,
    D_actor_400500_80150AA0,
    { NULL, D_actor_400500_80150560, NULL, NULL, D_actor_400500_80150620, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80150AEC[6] = {
#include "assets/actor_400500_animation_1F010_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80150B34[78] = {
#include "assets/actor_400500_animation_1F010_bank4.inc"
};

AnimationRecord D_actor_400500_80150C6C[104] = {
#include "assets/actor_400500_animation_1F010_records.inc"
};

u16 D_actor_400500_80150E0C[18] = {
#include "assets/actor_400500_animation_1F010_indices.inc"
};

AnimationSet D_actor_400500_80150E30 = {
    D_actor_400500_80150C6C,
    D_actor_400500_80150E0C,
    { NULL, D_actor_400500_80150AEC, NULL, NULL, D_actor_400500_80150B34, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80150E58[11] = {
#include "assets/actor_400500_animation_1F510_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80150EDC[113] = {
#include "assets/actor_400500_animation_1F510_bank4.inc"
};

AnimationRecord D_actor_400500_801510A0[155] = {
#include "assets/actor_400500_animation_1F510_records.inc"
};

u16 D_actor_400500_8015130C[18] = {
#include "assets/actor_400500_animation_1F510_indices.inc"
};

AnimationSet D_actor_400500_80151330 = {
    D_actor_400500_801510A0,
    D_actor_400500_8015130C,
    { NULL, D_actor_400500_80150E58, NULL, NULL, D_actor_400500_80150EDC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80151358[10] = {
#include "assets/actor_400500_animation_1FB0C_bank1.inc"
};

AnimationPackedRotation D_actor_400500_801513D0[150] = {
#include "assets/actor_400500_animation_1FB0C_bank4.inc"
};

AnimationRecord D_actor_400500_80151628[184] = {
#include "assets/actor_400500_animation_1FB0C_records.inc"
};

u16 D_actor_400500_80151908[18] = {
#include "assets/actor_400500_animation_1FB0C_indices.inc"
};

AnimationSet D_actor_400500_8015192C = {
    D_actor_400500_80151628,
    D_actor_400500_80151908,
    { NULL, D_actor_400500_80151358, NULL, NULL, D_actor_400500_801513D0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80151954[2] = {
#include "assets/actor_400500_animation_1FCD0_bank1.inc"
};

AnimationPackedRotation D_actor_400500_8015196C[16] = {
#include "assets/actor_400500_animation_1FCD0_bank4.inc"
};

AnimationRecord D_actor_400500_801519AC[72] = {
#include "assets/actor_400500_animation_1FCD0_records.inc"
};

u16 D_actor_400500_80151ACC[18] = {
#include "assets/actor_400500_animation_1FCD0_indices.inc"
};

AnimationSet D_actor_400500_80151AF0 = {
    D_actor_400500_801519AC,
    D_actor_400500_80151ACC,
    { NULL, D_actor_400500_80151954, NULL, NULL, D_actor_400500_8015196C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80151B18[2] = {
#include "assets/actor_400500_animation_1FE94_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80151B30[16] = {
#include "assets/actor_400500_animation_1FE94_bank4.inc"
};

AnimationRecord D_actor_400500_80151B70[72] = {
#include "assets/actor_400500_animation_1FE94_records.inc"
};

u16 D_actor_400500_80151C90[18] = {
#include "assets/actor_400500_animation_1FE94_indices.inc"
};

AnimationSet D_actor_400500_80151CB4 = {
    D_actor_400500_80151B70,
    D_actor_400500_80151C90,
    { NULL, D_actor_400500_80151B18, NULL, NULL, D_actor_400500_80151B30, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80151CDC[2] = {
#include "assets/actor_400500_animation_20058_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80151CF4[16] = {
#include "assets/actor_400500_animation_20058_bank4.inc"
};

AnimationRecord D_actor_400500_80151D34[72] = {
#include "assets/actor_400500_animation_20058_records.inc"
};

u16 D_actor_400500_80151E54[18] = {
#include "assets/actor_400500_animation_20058_indices.inc"
};

AnimationSet D_actor_400500_80151E78 = {
    D_actor_400500_80151D34,
    D_actor_400500_80151E54,
    { NULL, D_actor_400500_80151CDC, NULL, NULL, D_actor_400500_80151CF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80151EA0[2] = {
#include "assets/actor_400500_animation_2021C_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80151EB8[16] = {
#include "assets/actor_400500_animation_2021C_bank4.inc"
};

AnimationRecord D_actor_400500_80151EF8[72] = {
#include "assets/actor_400500_animation_2021C_records.inc"
};

u16 D_actor_400500_80152018[18] = {
#include "assets/actor_400500_animation_2021C_indices.inc"
};

AnimationSet D_actor_400500_8015203C = {
    D_actor_400500_80151EF8,
    D_actor_400500_80152018,
    { NULL, D_actor_400500_80151EA0, NULL, NULL, D_actor_400500_80151EB8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80152064[8] = {
#include "assets/actor_400500_animation_206C4_bank1.inc"
};

AnimationPackedRotation D_actor_400500_801520C4[105] = {
#include "assets/actor_400500_animation_206C4_bank4.inc"
};

AnimationRecord D_actor_400500_80152268[150] = {
#include "assets/actor_400500_animation_206C4_records.inc"
};

u16 D_actor_400500_801524C0[18] = {
#include "assets/actor_400500_animation_206C4_indices.inc"
};

AnimationSet D_actor_400500_801524E4 = {
    D_actor_400500_80152268,
    D_actor_400500_801524C0,
    { NULL, D_actor_400500_80152064, NULL, NULL, D_actor_400500_801520C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_8015250C[25] = {
#include "assets/actor_400500_animation_2141C_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80152638[343] = {
#include "assets/actor_400500_animation_2141C_bank4.inc"
};

AnimationRecord D_actor_400500_80152B94[416] = {
#include "assets/actor_400500_animation_2141C_records.inc"
};

u16 D_actor_400500_80153214[20] = {
#include "assets/actor_400500_animation_2141C_indices.inc"
};

AnimationSet D_actor_400500_8015323C = {
    D_actor_400500_80152B94,
    D_actor_400500_80153214,
    { NULL, D_actor_400500_8015250C, NULL, NULL, D_actor_400500_80152638, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80153264[15] = {
#include "assets/actor_400500_animation_21C64_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80153318[203] = {
#include "assets/actor_400500_animation_21C64_bank4.inc"
};

AnimationRecord D_actor_400500_80153644[262] = {
#include "assets/actor_400500_animation_21C64_records.inc"
};

u16 D_actor_400500_80153A5C[20] = {
#include "assets/actor_400500_animation_21C64_indices.inc"
};

AnimationSet D_actor_400500_80153A84 = {
    D_actor_400500_80153644,
    D_actor_400500_80153A5C,
    { NULL, D_actor_400500_80153264, NULL, NULL, D_actor_400500_80153318, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_400500_80153AAC[2] = {
#include "assets/actor_400500_animation_21E3C_bank1.inc"
};

AnimationPackedRotation D_actor_400500_80153AC4[16] = {
#include "assets/actor_400500_animation_21E3C_bank4.inc"
};

AnimationRecord D_actor_400500_80153B04[76] = {
#include "assets/actor_400500_animation_21E3C_records.inc"
};

u16 D_actor_400500_80153C34[20] = {
#include "assets/actor_400500_animation_21E3C_indices.inc"
};

AnimationSet D_actor_400500_80153C5C = {
    D_actor_400500_80153B04,
    D_actor_400500_80153C34,
    { NULL, D_actor_400500_80153AAC, NULL, NULL, D_actor_400500_80153AC4, NULL, NULL, NULL },
};

DamageAttack D_actor_400500_80153C84[3] = {
    { 28, 7 },
    { 35, 10 },
    { 8, 7 },
};

EnemyParams D_actor_400500_80153C90 = { D_actor_400500_80153C84, 450, 500, 200, 15, 100, 8, 100, 10 };

Actor400500MessageEntry D_actor_400500_80153CA0[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = func_actor_400500_8013DAE4 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

/// Borrowed player animation table; entry zero is unused.
static AnimationSet* _gActor400500PlayerAnimationSets[4] = {
    NULL,
    &D_actor_400500_8015323C,
    &D_actor_400500_80153A84,
    &D_actor_400500_80153C5C,
};

u8 D_actor_400500_80153CC0[136] = {
    0,
    0,
    0,
    0,
    16,
    83,
    20,
    128,
    72,
    90,
    20,
    128,
    44,
    97,
    20,
    128,
    196,
    105,
    20,
    128,
    128,
    120,
    20,
    128,
    0,
    135,
    20,
    128,
    192,
    143,
    20,
    128,
    56,
    153,
    20,
    128,
    12,
    158,
    20,
    128,
    236,
    169,
    20,
    128,
    68,
    175,
    20,
    128,
    4,
    185,
    20,
    128,
    48,
    189,
    20,
    128,
    156,
    194,
    20,
    128,
    84,
    204,
    20,
    128,
    192,
    209,
    20,
    128,
    76,
    223,
    20,
    128,
    220,
    229,
    20,
    128,
    100,
    233,
    20,
    128,
    4,
    239,
    20,
    128,
    128,
    243,
    20,
    128,
    236,
    254,
    20,
    128,
    56,
    5,
    21,
    128,
    196,
    10,
    21,
    128,
    48,
    14,
    21,
    128,
    48,
    19,
    21,
    128,
    44,
    25,
    21,
    128,
    240,
    26,
    21,
    128,
    180,
    28,
    21,
    128,
    120,
    30,
    21,
    128,
    60,
    32,
    21,
    128,
    228,
    36,
    21,
    128,
    0,
    0,
    0,
    0,
};

TaskDesc D_actor_400500_80153D48[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_400500_8013DF64, { .model = &D_actor_400500_801430E8 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_400500_8013DF6C, { .model = &D_actor_400500_80142E70 } },
};

TaskDesc D_actor_400500_80153D60 = { { { TASK_BODY_TMD, 96 } }, func_actor_400500_8013DE98, { .model = &D_actor_400500_80142C20 } };

ActorZone D_actor_400500_80153D6C[7] = {
    { -1700, -0x27D8, 1700, 3200, 4 },
    { 0, -0x27D8, 0x3BC4, 3200, 1 },
    { 0x3A98, -0x27D8, 3000, 3200, 2 },
    { 0x3A98, -7200, 3000, 5200, 3 },
    { 4200, -0x2E18, 2200, 1600, 5 },
    { 0x3A98, -2000, 3000, 1800, 6 },
    { 0, 0, 0, 0, -1 },
};

u16 D_actor_400500_80153DB4[16] = {
    0,
    0,
    0,
    0,
    0,
    0,
    32,
    48,
    64,
    80,
    96,
    80,
    64,
    48,
    0,
    0,
};

u8 D_actor_400500_80153DD4[33] = { 26, 26, 26, 27, 27, 15, 15, 26, 15, 26, 26, 27, 27, 15, 15, 26, 26, 27, 27, 30, 27, 26, 26, 26, 26, 26, 15, 15, 15, 15, 15, 15, 15 };

static void               func_actor_400500_80132000(Task* arg0);
static void               func_actor_400500_8013226C(Task* arg0);
static void               func_actor_400500_80132C54(Task* arg0);
static inline void        _actor400500SetAnim(Task* task, s16 id, s16 rate);
static inline void        _actor400500SetState(Task* task, s32 state, s32 subState);
static inline void        _actor400500TickAnim(Task* task);
static inline s32         _actor400500HitFlagged(Task* task);
static inline void        _actor400500SampleView(Task* task, s16 part, Actor400500ViewPos* pos);
static inline void        _actor400500AnchorPart(Task* task, s16 part, Actor400500ViewPos* pos);
static inline void        _actor400500EnqueueSound(Task* task, s32 sound);
static inline void        _actor400500PlaySound(Task* task, s32 id);
static void               func_actor_400500_801348D8(Task* arg0, s32 arg1);
static void               func_actor_400500_80134B88(Task* arg0);
static void               func_actor_400500_80135414(Task* arg0);
static __inline__ s32     lookup_zone(Task* task);
static __inline__ VECTOR* push_color(GfxCoord* coord);
static __inline__ void    pop_scratch(s32 n);
static __inline__ u8*     push_proj(void);
static void               func_actor_400500_801375B8(Task* arg0);
static inline void        _actor400500RequestMode(Task* task, s32 mode);
static inline s32         _actor400500CoordToView(GfxCoord* coord, MATRIX* matrix);
static inline void        _actor400500TurnPart(GfxCoord* part, u16 heading);
static inline s16         _actor400500PlayerDistance(GfxCoord* part);
static inline void        _actor400500PlayAnim(Task* task, s32 id);
static void               func_actor_400500_8013771C(Task* arg0);
static inline void        _actor400500UpdateColor(Task* arg0, GfxCoord* coord, TmdObject* obj);

static void func_actor_400500_80132000(Task* arg0)
{
    Actor400500Work* work;

    work = (Actor400500Work*)arg0->work;

    work->obj0.coord            = &arg0->extra.tmd->coords[3];
    work->obj0.context.contacts = work->rec0;
    work->obj0.pos.vz           = 0x110;
    work->obj0.pos.vx           = 0;
    work->obj0.pos.vy           = 0;
    work->obj0.key              = 0x30005;
    work->obj0.radius           = 0x260;
    work->obj0.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj0);
    Gp_InitRec18Table(work->rec0, 3, 0);
    work->obj0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->obj1.key              = Gp_PackObjPair(arg0->spawnArg2.pointer, 0);
    work->obj1.coord            = &arg0->extra.tmd->coords[7];
    work->obj1.context.contacts = work->rec1;
    work->obj1.pos.vx           = -0x460;
    work->obj1.pos.vy           = 0;
    work->obj1.pos.vz           = 0;
    work->obj1.radius           = 0x290;
    work->obj1.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->rec1, 1, 0);
    work->obj1.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->obj2.key              = Gp_PackObjPair(arg0->spawnArg2.pointer, 0);
    work->obj2.coord            = &arg0->extra.tmd->coords[7];
    work->obj2.context.contacts = work->rec1;
    work->obj2.pos.vx           = -0x200;
    work->obj2.pos.vy           = 0;
    work->obj2.pos.vz           = 0;
    work->obj2.radius           = 0x250;
    work->obj2.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj2);
    Gp_InitRec18Table(work->rec1, 1, 0);
    work->obj2.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->obj3.key              = Gp_PackObjPair(arg0->spawnArg2.pointer, 0);
    work->obj3.coord            = &arg0->extra.tmd->coords[10];
    work->obj3.context.contacts = work->rec2;
    work->obj3.pos.vx           = 0x460;
    work->obj3.pos.vy           = 0;
    work->obj3.pos.vz           = 0;
    work->obj3.radius           = 0x290;
    work->obj3.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj3);
    Gp_InitRec18Table(work->rec2, 1, 0);
    work->obj3.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->obj4.key              = Gp_PackObjPair(arg0->spawnArg2.pointer, 0);
    work->obj4.coord            = &arg0->extra.tmd->coords[10];
    work->obj4.context.contacts = work->rec2;
    work->obj4.pos.vx           = 0x200;
    work->obj4.pos.vy           = 0;
    work->obj4.pos.vz           = 0;
    work->obj4.radius           = 0x250;
    work->obj4.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj4);
    Gp_InitRec18Table(work->rec2, 1, 0);
    work->obj4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

static void func_actor_400500_8013226C(Task* arg0)
{
    Actor400500Work* work;
    OverlayMat       rot;
    OverlayMat*      src;
    GfxCoord*        parts;
    GfxCoord*        part7;
    GfxCoord*        part10;
    GfxCoord*        coord;
    Task*            child;
    TmdObject*       extra;
    TmdObject*       tmd;
    TmdObject*       parentTmd;

    parts              = arg0->extra.tmd->coords;
    work               = (Actor400500Work*)arg0->work;
    part7              = &parts[7];
    part10             = &parts[10];
    child              = Task_SpawnFromTable(D_actor_400500_80153D48, 0, 0, 0);
    work->field_9F0[0] = child;
    extra              = child->extra.tmd;
    coord              = extra->coords;
    extra->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord->parent      = part10;
    coord->coord.t[0]  = 0x400;
    coord->coord.t[1]  = 0;
    coord->coord.t[2]  = 0;
    src                = &rot;
    rot.ident.m00_m01  = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    RotMatrixY((s16)(-0x180), &src->mat);
    func_actor_400500_8013DE2C(&src->mat, &coord->coord);
    parentTmd              = arg0->extra.tmd;
    tmd                    = child->extra.tmd;
    tmd->texturePageOffset = parentTmd->texturePageOffset;
    tmd->clutRowOffset     = parentTmd->clutRowOffset;
    if (tmd->buffer != NULL) {
        tmdProcessStream(tmd);
        tmdProcessStream(tmd);
    }
    child                  = Task_SpawnFromTable(D_actor_400500_80153D48, 1, 0, 0);
    work->field_9F0[1]     = child;
    extra                  = child->extra.tmd;
    coord                  = extra->coords;
    extra->flags           = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord->parent          = part7;
    coord->coord.t[0]      = -0x400;
    coord->coord.t[1]      = 0;
    coord->coord.t[2]      = 0;
    parentTmd              = arg0->extra.tmd;
    tmd                    = child->extra.tmd;
    tmd->texturePageOffset = parentTmd->texturePageOffset;
    tmd->clutRowOffset     = parentTmd->clutRowOffset;
    if (tmd->buffer != NULL) {
        tmdProcessStream(tmd);
        tmdProcessStream(tmd);
    }
    rot.ident.m00_m01  = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    RotMatrixY((s16)(0x180), &src->mat);
    func_actor_400500_8013DE2C(&src->mat, &coord->coord);
}

static void func_actor_400500_80132438(Task* arg0)
{
    SVECTOR          dir;
    SVECTOR*         dirp;
    SVECTOR          delta;
    OverlayMat       rot;
    OverlayMat*      src;
    Actor400500Work* work;
    GfxCoord*        coord;
    GfxCoord*        other;
    s16              dist;
    s16              heading;
    s16              vz;
    s32              y;
    s32              z;
    s32              one;
    u16              counter;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] != NULL) {
        other              = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        work->field_9C0.vx = coord->coord.t[0];
        work->field_9C0.vy = coord->coord.t[1];
        work->field_9C0.vz = coord->coord.t[2];
        if ((s16)work->field_A1C != 5) {
            dir.vx = (u16)other->coord.t[0] - (u16)coord->coord.t[0];
            dir.vy = (u16)other->coord.t[1] - (u16)coord->coord.t[1];
            dir.vz = (u16)other->coord.t[2] - (u16)coord->coord.t[2];
        } else {
            work->field_A32 = 2;
            counter         = work->field_A22 + 1;
            work->field_A22 = counter;
            if (!(counter & 0x100)) {
                dir.vx = 0x2710 - (u16)coord->coord.t[0];
            } else {
                dir.vx = 0x3E8 - (u16)coord->coord.t[0];
            }
            y      = -0x3E8;
            dir.vy = y - (u16)coord->coord.t[1];
            z      = -0x20D0;
            dir.vz = z - (u16)coord->coord.t[2];
        }
        dist = SquareRoot0((dir.vx * dir.vx) + (dir.vz * dir.vz));
        do {
            work->field_9E0 = (u16)dir.vx;
            dirp            = &dir;
            work->field_9E2 = (u16)dir.vy;
        } while (0);
        vz              = (u16)dir.vz;
        work->field_A16 = dist;
        work->field_9E4 = vz;
        VectorNormalSS(dirp, dirp);
        work->field_A36    = (ratan2(dir.vx, dir.vz) - (u16)work->field_94A) & 0xFFF;
        delta.vx           = (u16)other->coord.t[0] - (u16)work->field_9D0.vx;
        delta.vy           = (u16)other->coord.t[1] - (u16)work->field_9D0.vy;
        one                = 0x1000;
        delta.vz           = (u16)other->coord.t[2] - (u16)work->field_9D0.vz;
        src                = &rot;
        rot.ident.m00_m01  = one;
        rot.ident.m02_m10  = 0;
        src->ident.m11_m12 = one;
        rot.ident.m20_m21  = 0;
        src->ident.m22     = one;
        heading            = work->field_94A;
        RotMatrixY(-heading, &src->mat);
        ApplyMatrixSV(&src->mat, &delta, &work->field_9D8);
    }
}

static void func_actor_400500_80132628(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, s32 shade)
{
    MATRIX    firstMatrix;
    MATRIX    secondMatrix;
    SVECTOR   first;
    SVECTOR   second;
    SVECTOR   corner0;
    SVECTOR   corner1;
    SVECTOR   corner2;
    SVECTOR   corner3;
    long      screen0;
    long      screen1;
    long      screen2;
    long      screen3;
    long      perspective;
    long      flags;
    s16       angle;
    GfxCoord* secondCoord;
    GfxCoord* firstCoord;
    s32       offset0;
    s32       offset1;
    s32       offset2;
    s32       offset3;
    s32       halfX;
    s32       halfZ;
    s32       depth;
    GfxCoord* coords;
    GfxCoord* viewCoord;
    POLY_FT4* poly;
    u8        room;
    u8        col;

    col         = shade;
    coords      = task->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &firstCoord->workm, &firstMatrix);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &secondCoord->workm, &secondMatrix);
        first.vy   = (s16)height;
        second.vy  = (s16)height;
        first.vx   = firstMatrix.t[0];
        first.vz   = firstMatrix.t[2];
        second.vx  = secondMatrix.t[0];
        second.vz  = secondMatrix.t[2];
        angle      = ratan2((s16)secondMatrix.t[0] - (s16)firstMatrix.t[0], (s16)secondMatrix.t[2] - (s16)firstMatrix.t[2]);
        halfX      = (first.vx - second.vx) / 2;
        halfZ      = (first.vz - second.vz) / 2;
        offset0    = rcos(angle) * width;
        corner0.vy = (s16)height;
        corner0.vx = halfX + (first.vx - (offset0 >> 0xC));
        corner0.vz = halfZ + (first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1    = rcos(angle) * width;
        corner1.vy = (s16)height;
        corner1.vx = halfX + (first.vx + (offset1 >> 0xC));
        corner1.vz = halfZ + (first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2    = rcos(angle) * width;
        corner2.vy = (s16)height;
        corner2.vx = (second.vx - (offset2 >> 0xC)) - halfX;
        corner2.vz = (second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3    = rcos(angle) * width;
        corner3.vy = (s16)height;
        corner3.vx = (second.vx + (offset3 >> 0xC)) - halfX;
        corner3.vz = (second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        /* `gGfxViewCoord`, reached back from its `workm`: the address is built
           from `gGfxViewCoord.workm`, whose high half the GTE loads below share. */
        viewCoord               = &gGfxViewCoord;
        viewCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(viewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        depth = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screen0, &screen1, &screen2, &screen3,
                              &perspective, &flags);
        if (flags >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 9);
            poly->code                     = 0x2E;
            GPU_PRIMITIVE_XY_WORD(poly, 0) = screen0;
            GPU_PRIMITIVE_XY_WORD(poly, 1) = screen1;
            GPU_PRIMITIVE_XY_WORD(poly, 2) = screen2;
            GPU_PRIMITIVE_XY_WORD(poly, 3) = screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            room        = gGameSession->location.loc.room;
            if ((room == 1) || (room == 3) || (room == 5) || (room == 6)) {
                poly->r0 = col;
                poly->g0 = col;
                poly->b0 = col;
            } else {
                poly->r0 = shade;
                poly->g0 = col >> 1;
                poly->b0 = shade;
            }
            addPrim((&gGpuCurrentOt[((((u32)(depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
        }
    }
}

static void func_actor_400500_80132AB0(Task* arg0, s16 arg1, s32 arg2)
{
    s32 temp_s2;

    temp_s2 = arg2 & 0xFF;
    func_actor_400500_80132628(arg0, 3, 9, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 9, 0xA, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xA, 0xB, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 3, 6, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 6, 7, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 7, 8, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 5, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 0xC, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xC, 0xD, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xD, 0xE, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 0xF, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xF, 0x10, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0x10, 0x11, 0x100, (s32)arg1, temp_s2);
}

static void func_actor_400500_80132C54(Task* arg0)
{
    Actor400500Work* work;
    OverlayMat       rot;
    OverlayMat*      src;
    GfxCoord*        coord;
    s32              tx;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp) {
        case 1:
            tx                = 0x800;
            work->field_94A   = tx;
            work->field_94C   = tx;
            tx                = 0x14A0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
        case 2:
            tx                = 0x800;
            work->field_94C   = tx;
            tx                = 0x4074;
            work->field_94A   = 0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
        case 3:
            tx                = 0xC00;
            work->field_94A   = tx;
            tx                = 0x800;
            work->field_94C   = tx;
            tx                = 0xFA0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
    }
    tx                 = 0x1000;
    src                = &rot;
    rot.ident.m00_m01  = tx;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = tx;
    src->ident.m20_m21 = 0;
    src->ident.m22     = tx;
    RotMatrixZ(work->field_94C, &src->mat);
    RotMatrixY(work->field_94A, &src->mat);
    func_actor_400500_8013DE2C(&src->mat, &coord->coord);
    func_actor_400500_8013DBCC(arg0, 0xB, &work->field_9A0);
}

static s32 func_actor_400500_80132D74(Task* arg0)
{
    Actor400500Work* work;

    work = (Actor400500Work*)arg0->work;
    if ((s16)work->field_A1C != 5) {
        if ((work->field_A16 < (0x500 - (work->field_9D8.vz * 8))) &&
            ((u32)(work->field_A36 - 0x2E0) >= 0xA41U)) {
            if ((((u16)work->field_A32 >> 3) == 0) && !(work->field_A1E & 1)) {
                func_actor_400500_8013DB64(arg0, 1);
                return 1;
            }
            return 0;
        }
        if ((work->field_A16 < (0x640 - (work->field_9D8.vz * 8))) &&
            ((u32)(work->field_A36 - 0x300) >= 0xA01U) &&
            (work->field_A32 == 0)) {
            if (!(((Actor400500Work*)arg0->work)->field_A1E & 1)) {
                func_actor_400500_8013DB64(arg0, 2);
            } else {
                func_actor_400500_8013DB64(arg0, 3);
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

static void func_actor_400500_80132E94(Task* arg0)
{
    Actor400500Work* work;
    Enemy*           enemy;
    TmdObject*       extra;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (Actor400500Work*)arg0->work;
    extra = arg0->extra.tmd;
    if (work->field_A46 < 0) {
        if (!((u8)work->field_A46 & 1)) {
            switch (work->field_A47) {
                case 0:
                    work->field_A20 = (u16)work->field_A20 + ((s16)(0xFF - (u16)work->field_A20) >> 2);
                    if (work->field_A20 >= 0xF8) {
                        work->field_A20 = 0xFF;
                        work->field_A2A = 0;
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    func_8009EA50(work->field_A20);
                    break;
                case 1:
                    work->field_A2A = (u16)work->field_A2A + 1;
                    if (work->field_A2C < work->field_A2A) {
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    break;
                case 2:
                    work->field_A24 = (u16)work->field_A24 + ((s16) - (u16)work->field_A24 >> 2);
                    work->field_A28 = (u16)work->field_A28 + (-work->field_A28 >> 2);
                    if (work->field_A24 == 0) {
                        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                        if ((u8)work->field_A4C == 0) {
                            enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
                        }
                        work->field_A28 = 0;
                        work->field_A46 = 0;
                        extra->flags   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    extra->shading.colorBlend = work->field_A24;
                    break;
            }
        } else {
            switch (work->field_A47) {
                case 0:
                    enemy->node.state.parts.flags = 0;
                    if ((u8)work->field_A4C == 0) {
                        enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
                    }
                    extra->flags   &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->field_A24 = (u16)work->field_A24 + ((s16)(0x1000 - (u16)work->field_A24) >> 2);
                    work->field_A28 = (u16)work->field_A28 + ((0xFF - work->field_A28) >> 2);
                    if (work->field_A24 >= 0xFF0) {
                        work->field_A28 = 0xFF;
                        work->field_A24 = 0x1000;
                        work->field_A2A = 0;
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    extra->shading.colorBlend = work->field_A24;
                    break;
                case 1:
                    work->field_A2A = (u16)work->field_A2A + 1;
                    if (work->field_A2A >= 0x11) {
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    break;
                case 2:
                    work->field_A20 = (u16)work->field_A20 + ((s16) - (u16)work->field_A20 >> 2);
                    if (work->field_A20 < 9) {
                        work->field_A20 = 0;
                        work->field_A46 = 0;
                        func_actor_400500_8013B4A4(arg0);
                        if (work->field_A30 == 0) {
                            work->field_A30 = (u16)work->field_A2E;
                        }
                    }
                    func_8009EA50(work->field_A20);
                    break;
            }
        }
    }
    if (work->field_A30 > 0) {
        work->field_A30 = (u16)work->field_A30 - 1;
    }
}

static s32 func_actor_400500_80133160(Task* arg0)
{
    Actor400500Work* work;
    s32              soundId;
    s32              pan;
    u16              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A38 == 1) {
        if (work->field_A3A == 0) {
            func_actor_400500_8013DCBC(arg0, 0x17, 0x10);
            work->field_A04 = 0;
            work->field_A3A = 1;
        }
        work->field_A04 = work->field_A04 + 1;
        if ((work->field_A04 & 0xF) == 8) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050001;
            pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            pan   <<= 24;
            pan   >>= 24;
            SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        heading         = (u16)work->field_94A - D_actor_400500_80153DB4[work->field_A04 & 0xF];
        work->field_94A = heading;
        if ((func_actor_400500_8013DDEC(arg0) << 0x10) != 0) {
            work->field_A04 = 0;
            work->field_A38 = 0;
            work->field_94A = (u16)work->field_94A & 0xE00;
        }
        func_actor_400500_8013DF50(arg0);
        return 1;
    }
    if (work->field_A38 == 2) {
        if (work->field_A3A == 0) {
            func_actor_400500_8013DCBC(arg0, 0x18, 0x10);
            work->field_A04 = 0;
            work->field_A3A = 1;
        }
        work->field_A04 = work->field_A04 + 1;
        if ((work->field_A04 & 0xF) == 8) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050002;
            pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            pan   <<= 24;
            pan   >>= 24;
            SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        heading         = (u16)work->field_94A + D_actor_400500_80153DB4[work->field_A04 & 0xF];
        work->field_94A = heading;
        if ((func_actor_400500_8013DDEC(arg0) << 0x10) != 0) {
            work->field_A04 = 0;
            work->field_A38 = 0;
            work->field_94A = (u16)work->field_94A & 0xE00;
        }
        func_actor_400500_8013DF50(arg0);
        return 1;
    }
    return 0;
}

static s32 func_actor_400500_80133358(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    s16                 mode;
    s16                 sub;
    s32                 flag;
    s32                 cond;
    s32                 ret;

    work = (Actor400500Work*)arg0->work;
    mode = work->field_A3C;
    if (mode == 1) {
        ret = 0;
        sub = work->field_A3E;
        if (sub == mode) {
            goto zero_both;
        }
        if ((sub == 2) || (sub == 4)) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A0E = 4;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xA;
            work2->field_9FA = 1;
            work->field_A3E  = 0;
            goto check_hit;
        }
        if (sub != 3) {
            goto check_hit;
        }
        func_actor_400500_8013DB64(arg0, 0xC);
        ret = 1;
    zero_both:
        work->field_A3C = 0;
        work->field_A3E = 0;
        return ret;
    check_hit:
        hit = (Actor400500HitView*)arg0->work;
        if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_A3C = 0;
        }
        return 1;
    }
    return 0;
}

static s32 func_actor_400500_80133460(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    s16                 mode;
    s16                 sub;
    s32                 flag;
    s32                 cond;

    work = (Actor400500Work*)arg0->work;
    mode = work->field_A3C;
    if (mode == 1) {
        sub = work->field_A3E;
        if (sub == mode) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x1C;
            work2->field_9FE = 0xB;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        } else if (sub == 2) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xC;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        } else if (sub == 4) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xC;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        } else if (sub == 3) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xE;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        }
        hit = (Actor400500HitView*)arg0->work;
        if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_A3C = 0;
        }
        return 1;
    }
    return 0;
}

/// Requests animation `id` at `rate`: the next `_actor400500TickAnim` resets
/// the slots to it.
static inline void _actor400500SetAnim(Task* task, s16 id, s16 rate)
{
    Actor400500Work* work = (Actor400500Work*)task->work;

    work->field_9F8 = rate;
    work->field_9FE = id;
    work->field_9FA = 2;
}

/// Writes `state` and `subState` into the enemy's state and sub-state indices.
static inline void _actor400500SetState(Task* task, s32 state, s32 subState)
{
    Actor400500Work* work;

    work            = (Actor400500Work*)task->work;
    work->field_A06 = state;
    work->field_A08 = subState;
}

/// Advances the enemy's animation state machine by one frame, then ticks
/// animation slots 1..0x11 at the current rate.
static inline void _actor400500TickAnim(Task* task)
{
    Actor400500Work* work;
    s32              i;

    work = (Actor400500Work*)task->work;
    if (work->field_9FA == 1) {
        if ((s16)work->field_9FC != work->field_9FE) {
            work->field_A00 = 0;
        } else {
            work->field_A00 = func_actor_400500_8013DD8C(task, work->field_A00);
        }
        func_actor_400500_8013DCD4(task);
        work->field_9FA = 3;
    } else if (work->field_9FA == 2) {
        func_actor_400500_8013DC4C(task);
        work->field_9FA = 3;
        work->field_A00 = 0;
    } else if (work->field_9FA == 3) {
        work->field_A00 = (u16)work->field_A00 + 1;
    }
    i = 1;
    do {
        work->slots[i].rate = work->field_9F8;
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x12);
}

/// Returns 1 when slot 1 reports a reached boundary, control jump, or held boundary pose.
static inline s32 _actor400500HitFlagged(Task* task)
{
    Actor400500HitView* hit = (Actor400500HitView*)task->work;

    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}

/// Records in `pos` the view-space X and Z of the actor's node `part`.
static inline void _actor400500SampleView(Task* task, s16 part, Actor400500ViewPos* pos)
{
    MATRIX    local;
    GfxCoord* coord;

    coord = &task->extra.tmd->coords[part];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &local);
    pos->x              = local.t[0];
    pos->z              = local.t[2];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Moves the root node in X and Z so that node `part` stays at the view-space
/// position `pos` recorded by `_actor400500SampleView`.
static inline void _actor400500AnchorPart(Task* task, s16 part, Actor400500ViewPos* pos)
{
    MATRIX    root;
    MATRIX    local;
    GfxCoord* coord;
    GfxCoord* coords;

    coords = task->extra.tmd->coords;
    coord  = &coords[part];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coords[0].workm, &root);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &local);
    coords[0].coord.t[0]   = pos->x - (local.t[0] - root.t[0]);
    coords[0].coord.t[2]   = pos->z - (local.t[2] - root.t[2]);
    coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    coord->composeStamp    = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    Gp_UpdateCoord(coords);
}

/// Queues the complete sound id `sound` from the enemy's position.
static inline void _actor400500EnqueueSound(Task* task, s32 sound)
{
    s32 pan;

    pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Queues sound `id` from the enemy's position, in its placement's sound bank.
static inline void _actor400500PlaySound(Task* task, s32 id)
{
    s32 sound;
    s32 pan;

    sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
    pan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Plays animation 2 at rate 0x18, moving the root so that node 0xB holds its
/// view-space position from frame 0 to 0xB00 / rate / 16 and node 8 from
/// 0xC00 / rate / 16 to 0x1500 / rate / 16, each with a sound on its first
/// frame. The cycle restarts at frame 0 while the hit flags are set.
static void func_actor_400500_801335E8(Task* arg0)
{
    Actor400500Work* work;
    GfxCoord*        coord;
    u8               tmp0;
    u8               tmp1;
    u8               tmp2;
    u8               end0;
    u8               start1;
    u8               end1;
    s32              start0;
    s32              sound;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_9FE != 2) {
        _actor400500SetAnim(arg0, 2, 0x18);
        _actor400500TickAnim(arg0);
    }
    if (((Actor400500Work*)arg0->work)->field_9F8 == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((Actor400500Work*)arg0->work)->field_9F8) >> 4;
    }
    end0 = tmp0;
    if (((Actor400500Work*)arg0->work)->field_9F8 == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((Actor400500Work*)arg0->work)->field_9F8) >> 4;
    }
    start1 = tmp1;
    if (((Actor400500Work*)arg0->work)->field_9F8 == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((Actor400500Work*)arg0->work)->field_9F8) >> 4;
    }
    end1 = tmp2;
    if (_actor400500HitFlagged(arg0)) {
        work->field_A00 = 0;
    }
    /* The first window starts at frame 0, which the original compares against
     * in a register; a literal 0 is folded into `$zero`. */
    SOFT_MOVE_ZERO(start0);
    if (work->field_A00 == start0) {
        _actor400500SampleView(arg0, 0xB, &work->field_9A0);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050001;
        _actor400500EnqueueSound(arg0, sound);
    }
    if (work->field_A00 == start1) {
        _actor400500SampleView(arg0, 8, &work->field_9A0);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050002;
        _actor400500EnqueueSound(arg0, sound);
    }
    if (work->field_A00 >= start0 && work->field_A00 <= end0) {
        _actor400500AnchorPart(arg0, 0xB, &work->field_9A0);
    }
    if (work->field_A00 >= start1 && work->field_A00 <= end1) {
        _actor400500AnchorPart(arg0, 8, &work->field_9A0);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Plays animation 2 at the current rate, moving the root so that node 0xB
/// holds its view-space position from frame 0 to 0xB00 / rate / 16 and node 8
/// from 0xC00 / rate / 16 to 0x1500 / rate / 16, each with a sound on its
/// first frame. The cycle restarts at frame 0 while the hit flags are set.
static void func_actor_400500_80133B14(Task* arg0)
{
    Actor400500Work* work;
    GfxCoord*        coord;
    u8               tmp0;
    u8               tmp1;
    u8               tmp2;
    u8               end0;
    u8               start1;
    u8               end1;
    s32              start0;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_9FE != 2) {
        _actor400500SetAnim(arg0, 2, work->field_9F8);
        _actor400500TickAnim(arg0);
    }
    if (((Actor400500Work*)arg0->work)->field_9F8 == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((Actor400500Work*)arg0->work)->field_9F8) >> 4;
    }
    end0 = tmp0;
    if (((Actor400500Work*)arg0->work)->field_9F8 == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((Actor400500Work*)arg0->work)->field_9F8) >> 4;
    }
    start1 = tmp1;
    if (((Actor400500Work*)arg0->work)->field_9F8 == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((Actor400500Work*)arg0->work)->field_9F8) >> 4;
    }
    end1 = tmp2;
    if (_actor400500HitFlagged(arg0)) {
        work->field_A00 = 0;
    }
    /* The first window starts at frame 0, which the original compares against
     * in a register; a literal 0 is folded into `$zero`. */
    SOFT_MOVE_ZERO(start0);
    if (work->field_A00 == start0) {
        _actor400500SampleView(arg0, 0xB, &work->field_9A0);
        _actor400500PlaySound(arg0, 0x40050001);
    }
    if (work->field_A00 == start1) {
        _actor400500SampleView(arg0, 8, &work->field_9A0);
        _actor400500PlaySound(arg0, 0x40050002);
    }
    if (work->field_A00 >= start0 && work->field_A00 <= end0) {
        _actor400500AnchorPart(arg0, 0xB, &work->field_9A0);
    }
    if (work->field_A00 >= start1 && work->field_A00 <= end1) {
        _actor400500AnchorPart(arg0, 8, &work->field_9A0);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Plays animation 4 at rate 0x10, moving the root so that node 8 holds its
/// view-space position from frame 0 to 0xD00 / rate / 16 and node 0xB from
/// 0xE00 / rate / 16 to 0x1B00 / rate / 16, each with a sound on its first
/// frame. The cycle restarts at frame 0 while the hit flags are set.
static void func_actor_400500_8013403C(Task* arg0)
{
    Actor400500Work* work;
    GfxCoord*        coord;
    u8               tmp0;
    u8               tmp1;
    u8               tmp2;
    u8               end0;
    u8               start1;
    u8               end1;
    s32              start0;
    s32              sound;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_9FE != 4) {
        _actor400500SetAnim(arg0, 4, 0x10);
        _actor400500TickAnim(arg0);
    }
    if (((Actor400500Work*)arg0->work)->field_9F8 == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xD00 / ((Actor400500Work*)arg0->work)->field_9F8) >> 4;
    }
    end0 = tmp0;
    if (((Actor400500Work*)arg0->work)->field_9F8 == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xE00 / ((Actor400500Work*)arg0->work)->field_9F8) >> 4;
    }
    start1 = tmp1;
    if (((Actor400500Work*)arg0->work)->field_9F8 == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1B00 / ((Actor400500Work*)arg0->work)->field_9F8) >> 4;
    }
    end1 = tmp2;
    if (_actor400500HitFlagged(arg0)) {
        work->field_A00 = 0;
    }
    /* The first window starts at frame 0, which the original compares against
     * in a register; a literal 0 is folded into `$zero`. */
    SOFT_MOVE_ZERO(start0);
    if (work->field_A00 == start0) {
        _actor400500SampleView(arg0, 8, &work->field_9A0);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050001;
        _actor400500EnqueueSound(arg0, sound);
    }
    if (work->field_A00 == start1) {
        _actor400500SampleView(arg0, 0xB, &work->field_9A0);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050002;
        _actor400500EnqueueSound(arg0, sound);
    }
    if (work->field_A00 >= start0 && work->field_A00 <= end0) {
        _actor400500AnchorPart(arg0, 8, &work->field_9A0);
    }
    if (work->field_A00 >= start1 && work->field_A00 <= end1) {
        _actor400500AnchorPart(arg0, 0xB, &work->field_9A0);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_400500_8013456C(Task* arg0)
{
    Actor400500Work* work;
    Enemy*           enemy;
    u32              dmg;
    u32              amount;
    s16              amount16;
    s16              hp;
    u8               flags;
    s32              tmp;
    s16              tick;
    s32              i;

    work  = (Actor400500Work*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    for (i = 0; i < 3; i++) {
        if ((work->rec0[i].key.value & 0xFFFF0000) == 0x20000) {
            if (work->field_A44 == 0) {
                work->field_A3C = 1;
                dmg             = Gp_ComputeDamage(work->rec0[i].key.value, work->field_A16, 0, 0);
                amount          = dmg;
                work->field_A44 = Gp_GetIdParam2(work->rec0[i].key.value);
                if (Gp_RollEnemyChance(enemy, work->rec0[i].key.value, 0) != 0) {
                    amount = ((u32)dmg << 16) >> 14;
                    Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 0, NULL);
                }
                amount16 = amount;
                func_800E2C78(enemy, work->rec0[i].key.value, amount16, 0);
                func_800DA6E8(&enemy->node, amount16, 0);
                hp        = (u16)enemy->hp - amount;
                enemy->hp = hp;
                if ((hp << 16) <= 0) {
                    enemy->hp       = 0;
                    work->field_A42 = 1;
                }
                func_800FDB18(
                    Gp_GetIdParam1(work->rec0[i].key.value) & 0xFFFF,
                    &arg0->extra.tmd->coords[3],
                    NULL,
                    &work->eff_940);
                if (amount16 >= 0x32) {
                    work->field_A3E = 2;
                    amount          = dmg;
                    work->field_A40 = 2;
                } else {
                    work->field_A3E = 1;
                    work->field_A40 = 1;
                }
            } else if ((Gp_GetIdParam1(work->rec0[i].key.value) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &arg0->extra.tmd->coords[1], NULL, &work->eff_940);
            }
            switch (Gp_GetIdParam0(work->rec0[i].key.value) & 0xFFFF) {
                case 0:
                    break;
                case 1:
                    Gp_SetObjFlag1(enemy);
                    break;
                case 2:
                    Gp_SetObjFlag2(enemy, work->rec0[i].key.value, 0);
                    break;
                case 3:
                    Gp_SetObjFlag4(enemy, work->rec0[i].key.value, 0);
                    break;
                case 4:
                    work->field_A3E = 4;
                    work->field_A40 = 4;
                    break;
                case 5:
                    work->field_A3E = 2;
                    work->field_A40 = 2;
                    break;
                case 6:
                    work->field_A3E = 4;
                    work->field_A40 = 4;
                    break;
                case 7:
                    work->field_A3E = 2;
                    work->field_A40 = 2;
                    break;
                case 8:
                case 9:
                    work->field_A4A = 1;
                    break;
            }
        }
    }

    flags = enemy->reactionFlags;
    if (flags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
        work->field_A3E      = 2;
        work->field_A40      = 2;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->field_A3E       = 3;
        work->field_A40       = 3;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        tmp  = Gp_TickObjFlag4(enemy);
        tick = tmp;
        if (tick != 0) {
            enemy->hp = (u16)enemy->hp - tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if (enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->field_A3C = 1;
            work->field_A3E = 2;
            work->field_A40 = 2;
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
    Gp_ClearRec18Occupied(work->rec0);
    if (work->field_A44 > 0) {
        work->field_A44 = (u16)work->field_A44 - 1;
        return;
    }
    work->field_A44 = 0;
}

static void func_actor_400500_801348D8(Task* arg0, s32 arg1)
{
    SVECTOR          pos;
    GfxCoord*        coords;
    GfxCoord*        joint;
    GfxCoord*        player;
    Task*            slot;
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              i;
    s32              cur;
    s32              sample;

    coords = arg0->extra.tmd->coords;
    slot   = *gPlayerActorTasks;
    joint  = coords + 8;
    work   = (Actor400500Work*)arg0->work;
    if (slot != NULL) {
        player = slot->extra.tmd->coords;
        work2  = work;
        if (work->field_9FA == 1) {
            if ((s16)work->field_9FC != work->field_9FE) {
                work->field_A00 = 0;
            } else {
                work->field_A00 = func_actor_400500_8013DD8C(arg0, work->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work->field_9FA = 3;
            work->field_A00 = 0;
        } else if (work->field_9FA == 3) {
            work->field_A00 = (u16)work->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&gGfxViewCoord);
        joint->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(joint);
        pos.vx = 0x160;
        pos.vy = 0x148;
        pos.vz = 0x2C0;
        coordLocalToWorld(joint, &pos);
        if ((arg1 << 0x10) == 0) {
            player->coord.t[0] = pos.vx;
            player->coord.t[2] = pos.vz;
        } else {
            sample             = pos.vx;
            cur                = player->coord.t[0];
            cur               += (sample - cur) >> 2;
            player->coord.t[0] = cur;
            sample             = pos.vz;
            cur                = player->coord.t[2];
            cur               += (sample - cur) >> 2;
            player->coord.t[2] = cur;
        }
        player->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(player);
        work->field_9F8 = -0x10;
        work3           = (Actor400500Work*)arg0->work;
        if (work3->field_9FA == 1) {
            if ((s16)work3->field_9FC != work3->field_9FE) {
                work3->field_A00 = 0;
            } else {
                work3->field_A00 = func_actor_400500_8013DD8C(arg0, work3->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work3->field_9FA = 3;
        } else if (work3->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work3->field_9FA = 3;
            work3->field_A00 = 0;
        } else if (work3->field_9FA == 3) {
            work3->field_A00 = (u16)work3->field_A00 + 1;
        }
        i = 1;
        do {
            work3->slots[i].rate = work3->field_9F8;
            Gp_AnimTickIndex(&work3->anim, i);
            i++;
        } while (i < 0x12);
        work->field_9F8 = 0x10;
    }
}

static void func_actor_400500_80134B88(Task* arg0)
{
    EffectWork* eff;
    EffectWork* eff2;
    EffectWork* eff3;
    TmdObject*  dst;
    TmdObject*  dst2;
    TmdObject*  dst3;
    TmdObject*  src;
    TmdObject*  src2;
    TmdObject*  src3;

    D_800678F0[0] = &D_actor_400500_8014393C;
    eff           = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[3], 0x200, NULL);
    if (eff != NULL) {
        src                    = arg0->extra.tmd;
        dst                    = eff->task->extra.tmd;
        dst->texturePageOffset = src->texturePageOffset;
        dst->clutRowOffset     = src->clutRowOffset;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
    D_800678F0[0] = &D_actor_400500_80143F40;
    eff2          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[1], 0x200, NULL);
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
    D_800678F0[0] = &D_actor_400500_80144624;
    eff3          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[1], 0x200, NULL);
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
    Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[2], 0x200, NULL);
    Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[3], 0x200, NULL);
}

#include "../../shared/frame_capture.inc.c"

static void func_actor_400500_80135414(Task* arg0)
{
    TmdObject*       extra;
    Enemy*           enemy;
    GfxCoord*        coord;
    Actor400500Work* work;
    Actor400500Work* work4;
    Actor400500Work* work5;
    GfxCoord*        player;
    GfxCoord*        coord2;
    TmdObject*       extra2;
    Actor400500Work* work7;
    s32              flag;
    u8               mode;

    extra      = arg0->extra.tmd;
    enemy      = arg0->spawnArg2.pointer;
    coord      = extra->coords;
    arg0->work = memCalloc(0xA50, 0);
    work       = (Actor400500Work*)arg0->work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    extra->lightMtx   = &work->lightMtx;
    extra->colorMtx   = &work->colorMtx;
    extra->flags      = 0;
    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &arg0->extra.tmd->coords[3];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->recs                   = work->rec0;
    enemy->param                  = &D_actor_400500_80153C90;
    enemy->hp = enemy->hpMax = D_actor_400500_80153C90.hpMax;
    func_800B3F84(&work->anim, D_actor_400500_80153CC0, extra, work->pad_2E4,
                  work->slots);
    coord->parent = &gGfxViewCoord;
    _actor400500SetAnim(arg0, 2, 0x18);
    _actor400500TickAnim(arg0);
    arg0->msgTable = D_actor_400500_80153CA0;
    func_actor_400500_80132C54(arg0);
    work4 = (Actor400500Work*)arg0->work;
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] != NULL) {
        player              = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        work4->field_9D0.vx = (u16)player->coord.t[0];
        work4->field_9D0.vy = (u16)player->coord.t[1];
        work4->field_9D0.vz = (u16)player->coord.t[2];
    }
    work5  = (Actor400500Work*)arg0->work;
    mode   = gGameSession->location.loc.room;
    extra2 = arg0->extra.tmd;
    if ((mode == 1) || (mode == 3) || (mode == 5) || (mode == 6)) {
        work5->field_A20 = 0xFF;
        work5->field_A24 = 0;
        work5->field_A28 = 0;
        work5->field_A2C = 0x10;
    } else {
        work5->field_A24 = 0x1000;
        work5->field_A28 = 0xFF;
        work5->field_A20 = 0;
        work5->field_A2C = 0x2000;
    }
    func_8009EA50(work5->field_A20);
    extra2->shading.colorBlend = work5->field_A24;
    func_actor_400500_80132000(arg0);
    func_actor_400500_8013226C(arg0);
    Gp_IncStateF0Ref(0);
    coord2                   = arg0->extra.tmd->coords;
    work->eff_940.spawnArgLo = 0x100;
    work->eff_940.spawnArgHi = 3;
    work->eff_940.coord      = &coord2[3];
    _actor400500SetState(arg0, 6, 0);
    gStageSceneMusicEntry = 2;
    work7                 = (Actor400500Work*)arg0->work;
    if (((work7->field_A46 >= 0) || ((u8)work7->field_A46 & 0x7F)) && (work7->field_A30 == 0)) {
        flag             = 0x80;
        work7->field_A46 = flag;
        work7->field_A47 = 0;
    }
    arg0->state = arg0->state + 1;
}

/// Handlers the task entry `func_actor_400500_8013DE98` runs by task state:
/// set-up, the per-frame state machine run by `field_A06`, a second one run by
/// `field_A06` from task state 2, and the teardown that kills the child tasks
/// and destroys the enemy 0x12D frames later.
static const TaskFuncTable4 D_actor_400500_80131E4C = { {
    func_actor_400500_80135414,
    func_actor_400500_80135770,
    func_actor_400500_8013A700,
    func_actor_400500_8013DEFC,
} };

static __inline__ s32 lookup_zone(Task* task)
{
    ActorZone* zone;
    u16        id_u;
    s16        zone_id;
    GfxCoord*  root;
    u16        px_u, pz_u;
    s16        px, pz;

    zone    = D_actor_400500_80153D6C;
    id_u    = (u16)zone->id;
    root    = task->extra.tmd->coords;
    zone_id = zone->id;
    px_u    = (u16)root->coord.t[0];
    pz_u    = (u16)root->coord.t[2];
    if (zone_id != -1) {
        px = (s16)px_u;
        pz = (s16)pz_u;
        do {
            if ((px >= zone->x) && ((zone->x + zone->w) >= px) &&
                (pz >= zone->z) && ((zone->z + zone->h) >= pz)) {
                return (s16)id_u;
            }
            zone++;
            id_u = (u16)zone->id;
        } while (zone->id != -1);
    }
    return 0;
}

static __inline__ VECTOR* push_color(GfxCoord* coord)
{
    VECTOR* block = (VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10);

    ((VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10))->vx = coord->workm.t[0];
    block->vy                                        = coord->workm.t[1];
    block->vz                                        = coord->workm.t[2];
    SCRATCH_STACK_CURSOR(VECTOR)                     = block;
    return block;
}

static __inline__ void pop_scratch(s32 n)
{
    SCRATCH_STACK_RELEASE_BYTES(n);
}

static __inline__ u8* push_proj(void)
{
    u8*                  head  = SCRATCH_STACK_CURSOR(u8);
    ActorProjectScratch* block = (ActorProjectScratch*)(head - 0x18);

    SCRATCH_STACK_CURSOR(ActorProjectScratch)     = block;
    ((ActorProjectScratch*)(head - 0x18))->vec.vx = 0;
    block->vec.vy                                 = 0;
    block->vec.vz                                 = 0;
    return head;
}

static const Actor400500TaskFuncTable13 D_actor_400500_80131E5C = { {
    func_actor_400500_80135EBC,
    func_actor_400500_8013BA24,
    func_actor_400500_801385D0,
    func_actor_400500_8013899C,
    func_actor_400500_80138EA0,
    func_actor_400500_8013905C,
    func_actor_400500_801392D8,
    func_actor_400500_801395D0,
    func_actor_400500_80139C1C,
    func_actor_400500_80139F6C,
    func_actor_400500_8013AD60,
    func_actor_400500_8013B5E0,
    func_actor_400500_8013A484,
} };

static void func_actor_400500_80135770(Task* arg0)
{
    Actor400500Work*           work;
    Enemy*                     enemy;
    TmdObject*                 extra0;
    TmdObject*                 obj;
    Task*                      slot;
    GfxCoord*                  part2;
    PlayerStatus*              cfg;
    AnimationPlayRequest       msg;
    Actor400500TaskFuncTable13 sp;
    OverlayMat                 rot;
    s8                         handshake;
    Actor400500Work*           work_pos;
    Actor400500Work*           work_dead;
    Actor400500Work*           work_rot;
    OverlayMat*                src;
    s32                        one;
    s16                        ang;
    s16                        ang_z;
    s16                        ang_y;
    GfxCoord*                  rot_root;
    GfxCoord*                  player;
    TmdObject*                 extra;
    TmdObject*                 extra2;
    TmdObject*                 trans_obj;
    VECTOR*                    color;
    u8*                        head;
    GfxCoord*                  color_part;
    u8                         mode;
    s16                        trans;
    s16                        trans_y;
    ActorProjectScratch*       proj;
    SVECTOR*                   vecp;
    MATRIX*                    workm;

    cfg    = &gPlayerStatus;
    work   = (Actor400500Work*)arg0->work;
    enemy  = (Enemy*)arg0->spawnArg2.pointer;
    extra0 = arg0->extra.tmd;
    part2  = extra0->coords + 2;
    obj    = extra0;
    slot   = *gPlayerActorTasks;
    sp     = D_actor_400500_80131E5C;

    handshake = work->field_A48;
    switch (handshake) {
        case 1:
            if (Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3ED, 0, 0) == 0) {
                work->field_A48 = 2;
            }
            break;
        case 2:
            msg.source.sets          = _gActor400500PlayerAnimationSets;
            msg.blend                = ANIMATION_BLEND_RESET;
            msg.blendFrames          = 0;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            if (work->field_A4D != 0) {
                msg.animationId = 3;
                work->field_A48 = 4;
                Gp_DispatchMsgPtr(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &msg, 0);
            } else {
                msg.animationId = handshake;
                work->field_A48 = 3;
                Gp_DispatchMsgPtr(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            }
            break;
        case 3:
            if (Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3ED, 0, 0) == 0) {
                Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 0, 0);
                work->field_A48 = 0;
            }
            break;
    }

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            func_actor_400500_80132438(arg0);
            work->field_A1A = lookup_zone(arg0);
            if (slot == NULL) {
                work->field_A1C = 0;
            } else {
                work->field_A1C = lookup_zone(slot);
            }
            sp.funcs[(s16)work->field_A06](arg0);
            work_pos = (Actor400500Work*)arg0->work;
            if (*gPlayerActorTasks != NULL) {
                player                 = (*gPlayerActorTasks)->extra.tmd->coords;
                work_pos->field_9D0.vx = (u16)player->coord.t[0];
                work_pos->field_9D0.vy = (u16)player->coord.t[1];
                work_pos->field_9D0.vz = (u16)player->coord.t[2];
            }
            src                 = &rot;
            work_rot            = (Actor400500Work*)arg0->work;
            rot_root            = arg0->extra.tmd->coords;
            ang                 = work_rot->field_948;
            ang_y               = work_rot->field_94A;
            work_rot->field_948 = ang & 0xFFF;
            ang_z               = work_rot->field_94C;
            work_rot->field_94A = ang_y & 0xFFF;
            work_rot->field_94C = ang_z & 0xFFF;
            one                 = 0x1000;
            rot.ident.m00_m01   = one;
            rot.ident.m02_m10   = 0;
            src->ident.m11_m12  = one;
            rot.ident.m20_m21   = 0;
            src->ident.m22      = one;
            RotMatrixZ(work_rot->field_94C, &src->mat);
            RotMatrixX(work_rot->field_948, &src->mat);
            RotMatrixY(work_rot->field_94A, &src->mat);
            func_actor_400500_8013DE2C(&src->mat, &rot_root->coord);
            func_actor_400500_80132E94(arg0);
            if (work->field_A32 > 0) {
                work->field_A32 = (u16)work->field_A32 - 1;
                work->field_A34 = 1;
            } else {
                work->field_A34 = 0;
            }
            obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            if ((enemy->hp > 0) || (cfg->hp <= 0)) {
                func_actor_400500_8013456C(arg0);
            } else if ((work->field_A42 == 0) && (work->field_A48 == 0)) {
                work_dead            = (Actor400500Work*)arg0->work;
                arg0->state          = 2;
                work_dead->field_A06 = 0;
                work_dead->field_A08 = 0;
            }
        case SCENE_COMBAT_ACTORS_PAUSED:
            extra      = arg0->extra.tmd;
            extra2     = extra;
            color_part = extra->coords + 1;
            color      = push_color(color_part);
            Gp_UpdateActorColor(arg0->spawnArg2.pointer, color, 0, 0);
            mode = gGameSession->location.loc.room;
            if ((mode == 1) || (mode == 3) || (mode == 5) || (mode == 6)) {
                trans_obj = extra2;
                trans     = 0x200;
                trans_y   = trans;
            } else {
                trans_obj = extra;
                trans     = 0x400;
                trans_y   = 0x1000;
            }
            Gp_SetObjTrans(trans_obj, trans, trans_y, trans);
            pop_scratch(0x10);
            if (gGameSession->sceneUpdatesPaused != 0) {
                func_actor_400500_80132AB0(arg0, -0xFA0, (u8)work->field_A28);
                return;
            }
            func_actor_400500_80132AB0(arg0, -0xFA0, ((u16)work->field_A28 >> 2) & 0xFF);
            func_actor_400500_80132AB0(arg0, -0x3E8, (u8)work->field_A28);
            head = push_proj();
            proj = (ActorProjectScratch*)(head - 0x18);
            Gp_UpdateCoord(part2);
            vecp  = &proj->vec;
            workm = &part2->workm;
            gte_SetRotMatrix(workm);
            gte_SetTransMatrix(workm);
            gte_ldv0(vecp);
            gte_rtps();
            gte_stsxy(&((ActorProjectScratch*)(head - 0x18))->sxy);
            gte_stdp(&((ActorProjectScratch*)(head - 0x18))->dp);
            gte_stflg(&((ActorProjectScratch*)(head - 0x18))->flag);
            gte_stszotz(&((ActorProjectScratch*)(head - 0x18))->otz);
            if (proj->flag < 0) {
                proj->otz = 0;
            }
            proj->otz = (proj->otz >> 4) + 0x1E;
            frameCaptureQueue(proj->otz);
            pop_scratch(0x18);
            return;
    }
}

/// Sub-state handlers `func_actor_400500_80135EBC` copies onto the stack and runs
/// by `field_A08` every frame.
static const TaskFuncTable11 D_actor_400500_80131E90 = { {
    func_actor_400500_801361EC,
    func_actor_400500_8013662C,
    func_actor_400500_80136864,
    func_actor_400500_801369A4,
    func_actor_400500_80136B94,
    func_actor_400500_80136D00,
    func_actor_400500_80136EB8,
    func_actor_400500_80137034,
    func_actor_400500_801371A0,
    func_actor_400500_80137338,
    func_actor_400500_80137478,
} };

/// Handlers `func_actor_400500_80135EBC` also runs, by `field_A0A`, while the
/// enemy is out of hit points.
static const TaskFuncTable3 D_actor_400500_80131EBC = { {
    func_actor_400500_8013BAA4,
    func_actor_400500_8013BB18,
    func_actor_400500_8013BBB0,
} };

static void func_actor_400500_80135EBC(Task* arg0)
{
    Actor400500Work* work;
    Enemy*           enemy;
    TaskFuncTable11  sp10;
    TaskFuncTable3   sp40;
    Actor400500Work* work2;
    s32              i;
    Actor400500Work* work3;
    s32              flag;
    Actor400500Work* work4;

    work  = (Actor400500Work*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    sp10  = D_actor_400500_80131E90;
    sp40  = D_actor_400500_80131EBC;
    if (enemy->hp <= 0) {
        sp40.funcs[(s16)work->field_A0A](arg0);
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
        work3 = (Actor400500Work*)arg0->work;
        if ((work3->field_A46 >= 0) || (((u8)work3->field_A46 & 0x7F) != 1)) {
            flag             = 0x81;
            work3->field_A46 = flag;
            work3->field_A47 = 0;
        }
        work->obj1.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj2.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        return;
    }
    if ((s16)work->field_A08 != 0) {
        work4 = (Actor400500Work*)arg0->work;
        if (work4->field_9FA == 1) {
            if ((s16)work4->field_9FC != work4->field_9FE) {
                work4->field_A00 = 0;
            } else {
                work4->field_A00 = func_actor_400500_8013DD8C(arg0, work4->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work4->field_9FA = 3;
        } else if (work4->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work4->field_9FA = 3;
            work4->field_A00 = 0;
        } else if (work4->field_9FA == 3) {
            work4->field_A00 = (u16)work4->field_A00 + 1;
        }
        i = 1;
        do {
            work4->slots[i].rate = work4->field_9F8;
            Gp_AnimTickIndex(&work4->anim, i);
            i++;
        } while (i < 0x12);
    }
    sp10.funcs[(s16)work->field_A08](arg0);
    work3 = (Actor400500Work*)arg0->work;
    if (((work3->field_A46 >= 0) || ((u8)work3->field_A46 & 0x7F)) && (work3->field_A30 == 0)) {
        flag             = 0x80;
        work3->field_A46 = flag;
        work3->field_A47 = 0;
    }
}

static void func_actor_400500_801361EC(Task* arg0)
{
    OverlayMat          rot;
    MATRIX              local;
    OverlayMat*         src;
    MATRIX*             dst;
    Actor400500Work*    work;
    Actor400500Work*    workA;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500Work*    work4;
    GfxCoord*           coord;
    GfxCoord*           coords;
    GfxCoord*           coords2;
    Actor400500ViewPos* pos;
    Actor400500ViewPos* pos2;
    Actor400500ViewPos* pos3;
    Actor400500ViewPos* pos4;
    s32                 flag;
    s32                 flag2;
    s32                 heading;
    s32                 i;
    s32                 tx;
    s32                 a1c;

    work    = (Actor400500Work*)arg0->work;
    heading = (u16)work->field_94A & 0xFFF;
    coord   = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        workA = (Actor400500Work*)arg0->work;
        if (workA->field_A49 != 0) {
            workA->field_A49 = 0;
            if (workA->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0)) {
            work3            = (Actor400500Work*)arg0->work;
            work3->field_A38 = 0;
            work3->field_A3A = 0;
            work4            = (Actor400500Work*)arg0->work;
            work4->field_9F8 = 0x18;
            work4->field_9FE = 2;
            work4->field_9FA = 2;
            work2            = (Actor400500Work*)arg0->work;
            if (work2->field_9FA == 1) {
                if ((s16)work2->field_9FC != work2->field_9FE) {
                    work2->field_A00 = 0;
                } else {
                    work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
                }
                func_actor_400500_8013DCD4(arg0);
                work2->field_9FA = 3;
            } else if (work2->field_9FA == 2) {
                func_actor_400500_8013DC4C(arg0);
                work2->field_9FA = 3;
                work2->field_A00 = 0;
            } else if (work2->field_9FA == 3) {
                work2->field_A00 = (u16)work2->field_A00 + 1;
            }
            i = 1;
            do {
                work2->slots[i].rate = work2->field_9F8;
                Gp_AnimTickIndex(&work2->anim, i);
                i++;
            } while (i < 0x12);
            switch ((s16)((u16)work->field_A1A - 1)) {
                case 3:
                    if (heading != 0x400) {
                        goto case3_ne;
                    }
                    work->field_A08 = 1;
                    break;
                case3_ne:
                    work->field_A08 = 4;
                    break;
                case 0:
                    if (work->field_9E0 >= 0) {
                        goto case0_ge;
                    }
                    work->field_A08 = 3;
                    break;
                case0_ge:
                    work->field_A08 = 1;
                    break;
                case 1:
                    a1c = (s16)work->field_A1C;
                    if ((a1c == 1) || (a1c == 4) || (a1c == 5)) {
                        work->field_A08 = 3;
                    } else if ((a1c == 3) && (heading == 0) && (coord->coord.t[0] >= 0x4074)) {
                        work->field_A08 = 6;
                    } else if ((s16)work->field_A1C == 2) {
                        if (coord->coord.t[2] < -0x209D) {
                            goto a1c2_lt;
                        }
                        work->field_A08 = 6;
                        break;
                    a1c2_lt:
                        work->field_A08 = 1;
                        break;
                    } else {
                        work->field_A08 = 1;
                    }
                    break;
                case 2:
                    if (work->field_9E4 >= 0) {
                        goto case2_ge;
                    }
                    work->field_A08 = 8;
                    break;
                case2_ge:
                    work->field_A08 = 6;
                    break;
                case 5:
                    if (heading != 0x800) {
                        goto case5_ne;
                    }
                    work->field_A08 = 8;
                    break;
                case5_ne:
                    work->field_A08 = 7;
                    break;
                default:
                    tx                 = -0x3E8;
                    coord->coord.t[0]  = tx;
                    tx                 = -0xFA0;
                    coord->coord.t[1]  = tx;
                    tx                 = -0x2116;
                    coord->coord.t[2]  = tx;
                    tx                 = 0x400;
                    work->field_94A    = tx;
                    tx                 = 0x800;
                    work->field_94C    = tx;
                    tx                 = 0x1000;
                    src                = &rot;
                    work->field_948    = 0;
                    work->field_A1E    = 0;
                    rot.ident.m00_m01  = tx;
                    src->ident.m02_m10 = 0;
                    src->ident.m11_m12 = tx;
                    src->ident.m20_m21 = 0;
                    src->ident.m22     = tx;
                    RotMatrixZ(work->field_94C, &src->mat);
                    RotMatrixY(work->field_94A, &src->mat);
                    dst          = &coord->coord;
                    dst->m[0][0] = src->mat.m[0][0];
                    dst->m[0][1] = src->mat.m[0][1];
                    dst->m[0][2] = src->mat.m[0][2];
                    dst->m[1][0] = src->mat.m[1][0];
                    dst->m[1][1] = src->mat.m[1][1];
                    dst->m[1][2] = src->mat.m[1][2];
                    dst->m[2][0] = src->mat.m[2][0];
                    dst->m[2][1] = src->mat.m[2][1];
                    dst->m[2][2] = src->mat.m[2][2];
                    pos2         = &work->field_9A0;
                    coords       = arg0->extra.tmd->coords;
                    Gp_UpdateCoord(&coords[11]);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &coords[11].workm, &local);
                    pos                     = pos2;
                    pos->x                  = local.t[0];
                    pos->z                  = local.t[2];
                    coords[11].composeStamp = GRAPHICS_COORD_DIRTY;
                    work->field_A08         = 1;
                    break;
            }
            pos4    = &work->field_9A0;
            coords2 = arg0->extra.tmd->coords;
            Gp_UpdateCoord(&coords2[11]);
            Gp_WorldToLocal(&gGfxViewCoord.workm, &coords2[11].workm, &local);
            pos3                     = pos4;
            pos3->x                  = local.t[0];
            pos3->z                  = local.t[2];
            coords2[11].composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
}

static void func_actor_400500_8013662C(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GfxCoord*        coord;
    s32              flag;
    s32              flag2;
    s32              heading;
    s32              a1a;
    s32              val;
    u32              rnd;
    u16              a1c;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) != 0x400) {
                if (((0x400 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                a1a = work->field_A1A;
                if (a1a != 1) {
                    if ((a1a == 2) && (coord->coord.t[0] >= 0x4075)) {
                        a1c = work->field_A1C;
                        if (((u32)(a1c - 2) < 2U) || ((s16)a1c == 6)) {
                            work->field_A08 = 5;
                        } else {
                            work->field_A08 = a1a;
                        }
                    } else {
                        func_actor_400500_801335E8(arg0);
                    }
                } else {
                    if (work->field_A34 == 0) {
                        if (work->field_9E0 <= 0) {
                            work->field_A08 = 2;
                        }
                    } else if (work->field_9E0 < -0xF9F) {
                        rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = rnd;
                        if (((rnd >> 0x10) & 0x1F) == 0) {
                            if (!(work->field_A1E & 1)) {
                                work2 = (Actor400500Work*)arg0->work;
                                val   = 7;
                            } else {
                                work2 = (Actor400500Work*)arg0->work;
                                val   = 8;
                            }
                            work2->field_A06 = val;
                            work2->field_A08 = 0;
                        }
                    }
                    func_actor_400500_801335E8(arg0);
                }
            }
            coord->coord.t[2] = -0x209E;
        }
    }
}

static void func_actor_400500_80136864(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0xC00) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 3;
                return;
            }
            if (((0xC00 - heading) << 0x14) > 0) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A38 = 2;
                work3->field_A3A = 0;
            } else {
                work4            = (Actor400500Work*)arg0->work;
                work4->field_A38 = 1;
                work4->field_A3A = 0;
            }
        }
    }
}

static void func_actor_400500_801369A4(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GfxCoord*        coord;
    s32              flag;
    s32              flag2;
    s32              heading;
    s32              a1a;
    s32              val;
    u32              rnd;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) != 0xC00) {
                if (((0xC00 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                a1a = work->field_A1A;
                if (a1a != 1) {
                    if (a1a == 4) {
                        work->field_A08 = a1a;
                    }
                } else if (work->field_A34 == 0) {
                    if (work->field_9E0 >= 0) {
                        work->field_A08 = 4;
                    }
                } else if (work->field_9E0 >= 0xFA0) {
                    rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = rnd;
                    if (((rnd >> 0x10) & 0x1F) == 0) {
                        if (!(work->field_A1E & 1)) {
                            work2 = (Actor400500Work*)arg0->work;
                            val   = 7;
                        } else {
                            work2 = (Actor400500Work*)arg0->work;
                            val   = 8;
                        }
                        work2->field_A06 = val;
                        work2->field_A08 = 0;
                    }
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[2] = -0x209E;
        }
    }
}

static void func_actor_400500_80136B94(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0x400) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 1;
                return;
            }
            if ((s16)work->field_A1C == 4) {
                if (work->field_9E4 > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
            } else if (((0x400 - heading) << 0x14) > 0) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A38 = 2;
            } else {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A38 = 1;
            }
            work2->field_A3A = 0;
        }
    }
}

static void func_actor_400500_80136D00(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            if (((u32)(work->field_A1C - 2) < 2U) || ((s16)work->field_A1C == 6)) {
                heading = (u16)work->field_94A;
                if ((heading & 0xFFF) == 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_9F8 = 0x18;
                    work2->field_9FE = 2;
                    work2->field_9FA = 2;
                    work->field_A08  = 6;
                    return;
                }
                if (((0 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
            } else {
                heading = (u16)work->field_94A;
                if ((heading & 0xFFF) == 0xC00) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_9F8 = 0x18;
                    work2->field_9FE = 2;
                    work2->field_9FA = 2;
                    work->field_A08  = 3;
                    return;
                }
                if (((0xC00 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
            }
            work2->field_A3A = 0;
        }
    }
}

static void func_actor_400500_80136EB8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GfxCoord*        coord;
    s32              flag;
    s32              flag2;
    s32              heading;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if (heading & 0xFFF) {
                if (((0 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                if (work->field_A1A != 3) {
                    if (work->field_A1A == 6) {
                        work->field_A08 = 7;
                    }
                } else if ((work->field_A34 == 0) && (work->field_9E4 < 0)) {
                    work->field_A08 = 7;
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[0] = 0x4074;
        }
    }
}

static void func_actor_400500_80137034(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0x800) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 8;
                return;
            }
            if ((s16)work->field_A1C == 6) {
                if (work->field_9E0 > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
            } else if (((0x800 - heading) << 0x14) > 0) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A38 = 2;
            } else {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A38 = 1;
            }
            work2->field_A3A = 0;
        }
    }
}

static void func_actor_400500_801371A0(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GfxCoord*        coord;
    s32              flag;
    s32              flag2;
    s32              heading;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) != 0x800) {
                if (((0x800 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                switch (work->field_A1A) {
                    case 2:
                        if (coord->coord.t[2] < -0x209E) {
                            work->field_A08 = 9;
                        }
                        break;
                    case 3:
                        if ((work->field_A34 == 0) && (work->field_9E4 > 0)) {
                            work->field_A08 = 0xA;
                        }
                        break;
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[0] = 0x4074;
        }
    }
}

static void func_actor_400500_80137338(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0xC00) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 3;
                return;
            }
            if (((0xC00 - heading) << 0x14) > 0) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A38 = 2;
                work3->field_A3A = 0;
            } else {
                work4            = (Actor400500Work*)arg0->work;
                work4->field_A38 = 1;
                work4->field_A3A = 0;
            }
        }
    }
}

static void func_actor_400500_80137478(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 6;
                return;
            }
            if (((0 - heading) << 0x14) > 0) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A38 = 2;
                work3->field_A3A = 0;
            } else {
                work4            = (Actor400500Work*)arg0->work;
                work4->field_A38 = 1;
                work4->field_A3A = 0;
            }
        }
    }
}

static void func_actor_400500_801375B8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              i;

    work              = (Actor400500Work*)arg0->work;
    work->obj0.radius = 0x130;
    work2             = (Actor400500Work*)arg0->work;
    if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
        flag             = 0x81;
        work2->field_A46 = flag;
        work2->field_A47 = 0;
    }
    work3            = (Actor400500Work*)arg0->work;
    work3->field_9F8 = 0x10;
    work3->field_9FE = 5;
    work3->field_9FA = 2;
    work4            = (Actor400500Work*)arg0->work;
    if (work4->field_9FA == 1) {
        if ((s16)work4->field_9FC != work4->field_9FE) {
            work4->field_A00 = 0;
        } else {
            work4->field_A00 = func_actor_400500_8013DD8C(arg0, work4->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work4->field_9FA = 3;
    } else if (work4->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work4->field_9FA = 3;
        work4->field_A00 = 0;
    } else if (work4->field_9FA == 3) {
        work4->field_A00 = (u16)work4->field_A00 + 1;
    }
    i = 1;
    do {
        work4->slots[i].rate = work4->field_9F8;
        Gp_AnimTickIndex(&work4->anim, i);
        i++;
    } while (i < 0x12);
    work->field_A04 = 0;
    work->field_A18 = 0;
    work->field_9BC = 0;
    work->field_A08 = work->field_A08 + 1;
}

/// Stores `mode` in `field_A46` and clears `field_A47`, unless `field_A46`
/// already holds `mode` with bit 7 set or `field_A30` is nonzero. Only the low
/// 7 bits of the two values are compared.
static inline void _actor400500RequestMode(Task* task, s32 mode)
{
    Actor400500Work* work;

    work = (Actor400500Work*)task->work;
    if (((work->field_A46 >= 0) || ((work->field_A46 & 0x7F) != (mode & 0x7F))) && (work->field_A30 == 0)) {
        work->field_A46 = mode;
        work->field_A47 = 0;
    }
}

/// Composes `coord`'s matrix with each ancestor's normalised matrix up the
/// `parent` chain, leaving the product in `matrix`. Returns 1 when the chain
/// reaches the view coordinate and 0 when it ends before it.
static inline s32 _actor400500CoordToView(GfxCoord* coord, MATRIX* matrix)
{
    MATRIX    result;
    MATRIX    parent;
    GfxCoord* current;

    current = coord->parent;
    *matrix = coord->coord;
    while (1) {
        if (current == NULL) {
            return 0;
        }
        if (current == &gGfxViewCoord) {
            return 1;
        }
        parent = current->coord;
        MatrixNormal(&parent, &parent);
        gte_SetRotMatrix(&parent);
        MulRotMatrix(matrix);
        MatrixNormal(matrix, &result);
        *matrix = result;
        current = current->parent;
    }
}

/// Turns `part` by `heading` in view space: builds the part's view-space
/// matrix in a scratch-pad block, applies `heading` with `RotMatrixY`,
/// takes it back into the part's own space with `func_actor_400500_8013B720`,
/// and copies the resulting rotation (not the translation) into the part
/// before refreshing it.
static inline void _actor400500TurnPart(GfxCoord* part, u16 heading)
{
    MATRIX* matrix;

    matrix = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    _actor400500CoordToView(part, matrix);
    RotMatrixY((s16)(heading), matrix);
    func_actor_400500_8013B720(part, matrix);
    memcpy(part->coord.m, matrix->m, sizeof(part->coord.m));
    part->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(part);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Returns the horizontal distance in view space between the player's part 4
/// and `part`, or 0x7FFF when there is no player.
static inline s16 _actor400500PlayerDistance(GfxCoord* part)
{
    MATRIX    playerView;
    MATRIX    partView;
    GfxCoord* playerCoords;
    SVECTOR   delta;

    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] == NULL) {
        return 0x7FFF;
    }
    playerCoords = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    Gp_UpdateCoord(&playerCoords[4]);
    Gp_UpdateCoord(part);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &playerCoords[4].workm, &playerView);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &part->workm, &partView);
    delta.vx = (u16)playerView.t[0] - (u16)partView.t[0];
    delta.vz = (u16)playerView.t[2] - (u16)partView.t[2];
    return SquareRoot0((delta.vx * delta.vx) + (delta.vz * delta.vz));
}

/// Starts animation `id` at rate 0x10.
static inline void _actor400500PlayAnim(Task* task, s32 id)
{
    Actor400500Work* work;

    work            = (Actor400500Work*)task->work;
    work->field_A0E = 2;
    work->field_9F8 = 0x10;
    work->field_9FE = id;
    work->field_9FA = 1;
}

static void func_actor_400500_8013771C(Task* arg0)
{
    SVECTOR             in;
    SVECTOR             out;
    OverlayMat          rot;
    OverlayMat*         src;
    Actor400500Work*    work;
    Actor400500HitView* hit;
    GameActor*          player;
    void*               spawn;
    s16                 vz;
    s32                 ident;
    s32                 r;
    s32                 cond;

    work            = (Actor400500Work*)arg0->work;
    player          = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->work;
    spawn           = arg0->spawnArg2.pointer;
    work->field_A04 = work->field_A04 + 1;
    _actor400500TickAnim(arg0);
    src = &rot;
    if ((s16)work->field_A04 < 0xF) {
        in.vx              = (u16)work->field_9E0;
        in.vy              = 0;
        vz                 = (u16)work->field_9E4;
        ident              = 0x1000;
        rot.ident.m00_m01  = ident;
        rot.ident.m02_m10  = 0;
        in.vz              = vz;
        src->ident.m11_m12 = ident;
        rot.ident.m20_m21  = 0;
        src->ident.m22     = ident;
        RotMatrixY(work->field_94A, &src->mat);
        ApplyMatrixSV(&src->mat, &in, &out);
        r                = ratan2(out.vx, work->field_9E2 - 0x6A0);
        work->field_9BC += ((-r - (u16)work->field_9BC) << 20) >> 23;
    } else {
        work->field_9BC += -((u16)work->field_9BC << 20) >> 23;
    }
    if ((s16)work->field_A04 == 7) {
        if (player->mode != GAME_ACTOR_MODE_SCRIPTED) {
            if (_actor400500PlayerDistance(&arg0->extra.tmd->coords[8]) < 0x500) {
                AnimationPlayRequest msg;

                msg.source.sets          = _gActor400500PlayerAnimationSets;
                msg.animationId          = 1;
                msg.blend                = ANIMATION_BLEND_RESET;
                msg.blendFrames          = 0;
                msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                Gp_DispatchMsgPtr(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
                work->field_A48      = 1;
                Gp_StateC08.field_6 |= 1;
                work->field_A18      = 1;
            }
        }
    }
    if (((s16)work->field_A04 == 0xE) && (work->field_A18 == 0)) {
        _actor400500PlayAnim(arg0, 6);
        work->field_A04 = 0;
        _actor400500RequestMode(arg0, 0x80);
        work->field_A08 = work->field_A08 + 1;
    }
    _actor400500TurnPart(&arg0->extra.tmd->coords[6], work->field_9BC);
    _actor400500TurnPart(&arg0->extra.tmd->coords[9], work->field_9BC);
    if (((u32)(work->field_A04 - 7) < 4U) && (work->field_A18 == 1)) {
        func_actor_400500_801348D8(arg0, 1);
    }
    if (((u32)(work->field_A04 - 0xB) < 0x14U) && (work->field_A18 == 1)) {
        func_actor_400500_801348D8(arg0, 0);
    }
    if (((s16)work->field_A04 == 0xC) && (work->field_A18 != 0)) {
        _actor400500PlaySound(arg0, 6);
    }
    if ((s16)work->field_A04 == 0x1E) {
        _actor400500PlaySound(arg0, 0x40050007);
    }
    if ((s16)work->field_A04 == 0x2A) {
        Gp_SpawnPadLerp(8, 0xC0U, 8U);
        _actor400500PlaySound(arg0, 0x40050008);
    }
    if ((s16)work->field_A04 == 0x1F) {
        Gp_SpawnPadLerp(6, 0xFFU, 0x80U);
        if (actorPlayerContactMessage(spawn, 1) != 0) {
            player->state   = 0xA;
            work->field_A4D = 1;
        }
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        _actor400500SetState(arg0, 0, 0);
        work->field_A32   = 0x3C;
        work->obj0.radius = 0x260;
    }
}

static void func_actor_400500_80138088(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    s32                 i;
    s32                 cond;

    work            = (Actor400500Work*)arg0->work;
    work->field_A04 = work->field_A04 + 1;
    work2           = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);

    work->field_9BC += -(work->field_9BC * 16) >> 7;
    _actor400500TurnPart(&arg0->extra.tmd->coords[6], work->field_9BC);
    _actor400500TurnPart(&arg0->extra.tmd->coords[9], work->field_9BC);

    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj0.radius = 0x260;
        _actor400500SetState(arg0, 0, 0);
        _actor400500RequestMode(arg0, 0x80);
        work->field_A32 = 0x3C;
    }
}

/// Sub-state handlers `func_actor_400500_8013BA24` copies onto the stack and runs by `field_A08`.
static const TaskFuncTable3 D_actor_400500_80131EE4 = { {
    func_actor_400500_801375B8,
    func_actor_400500_8013771C,
    func_actor_400500_80138088,
} };

/// Sub-state handlers `func_actor_400500_801385D0` copies onto the stack and runs
/// by `field_A08` while the enemy is alive.
static const TaskFuncTable3 D_actor_400500_80131EF0 = { {
    func_actor_400500_8013BE50,
    func_actor_400500_8013BEC4,
    func_actor_400500_801387E8,
} };

/// Handlers `func_actor_400500_801385D0` runs by `field_A0A`, instead of the
/// sub-state, once the enemy is out of hit points.
static const TaskFuncTable3 D_actor_400500_80131EFC = { {
    func_actor_400500_8013BC9C,
    func_actor_400500_8013BCCC,
    func_actor_400500_8013BD64,
} };

static void func_actor_400500_801385D0(Task* arg0)
{
    Actor400500Work* work;
    Enemy*           enemy;
    TaskFuncTable3   sp10;
    TaskFuncTable3   sp20;
    Actor400500Work* workA;
    Actor400500Work* work2;
    s32              skip;
    s32              i;

    work  = (Actor400500Work*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    sp10  = D_actor_400500_80131EF0;
    sp20  = D_actor_400500_80131EFC;
    if (enemy->hp <= 0) {
        if (work->field_A40 == 4) {
            work->field_A42 = 0;
        } else {
            sp20.funcs[(s16)work->field_A0A](arg0);
        }
        work->obj1.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj2.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        goto common;
    }
    workA = (Actor400500Work*)arg0->work;
    if (workA->field_A4A != 0) {
        workA->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        skip = 1;
    } else {
        skip = 0;
    }
    if (skip == 0) {
        sp10.funcs[(s16)work->field_A08](arg0);
        ((Actor400500Work*)arg0->work)->field_A49 = 0;
        func_actor_400500_80133358(arg0);
    common:
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
    }
}

static void func_actor_400500_801387E8(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    OverlayMat          rot;
    s32                 soundId;
    s32                 pan;
    s32                 cond;
    s32                 flag;
    u16                 frame;

    work            = (Actor400500Work*)arg0->work;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame == 0xB) {
        work->obj1.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj2.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        soundId           = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050005;
        pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_A04 >= 0x12) {
        if ((s16)work->field_A26 != 0) {
            work->field_A26 = (u16)work->field_A26 - 0x80;
        } else {
            ((Actor400500Work*)arg0->work)->field_9F0[1]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->obj1.flags                                              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj2.flags                                              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor400500Work*)arg0->work;
        work2->field_A06 = 0;
        work2->field_A08 = 0;
        work3            = (Actor400500Work*)arg0->work;
        if (((work3->field_A46 >= 0) || ((u8)work3->field_A46 & 0x7F)) && (work3->field_A30 == 0)) {
            flag             = 0x80;
            work3->field_A46 = flag;
            work3->field_A47 = 0;
        }
        work->field_A32 = 0x3C;
    }
}

/// Sub-state handlers `func_actor_400500_8013899C` copies onto the stack and runs by `field_A08`.
static const TaskFuncTable5 D_actor_400500_80131F08 = { {
    func_actor_400500_8013BFB0,
    func_actor_400500_8013C018,
    func_actor_400500_80138B78,
    func_actor_400500_80138CE8,
    func_actor_400500_80138DC4,
} };

static void func_actor_400500_8013899C(Task* arg0)
{
    Actor400500Work* work;
    Enemy*           enemy;
    TaskFuncTable5   sp;
    Actor400500Work* workA;
    Actor400500Work* work2;
    s32              i;
    Actor400500Work* work3;
    s32              skip;

    work  = (Actor400500Work*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    sp    = D_actor_400500_80131F08;
    if (enemy->hp > 0) {
        workA = (Actor400500Work*)arg0->work;
        if (workA->field_A4A != 0) {
            workA->field_A4A = 0;
            func_actor_400500_8013DB64(arg0, 5);
            skip = 1;
        } else {
            skip = 0;
        }
        if (skip == 0) {
            sp.funcs[(s16)work->field_A08](arg0);
            goto common;
        }
    } else {
        work->field_A42   = 0;
        work->obj1.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj2.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    common:
        func_actor_400500_80133358(arg0);
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A49 = 0;
    }
}

static void func_actor_400500_80138B78(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    OverlayMat          rot;
    s32                 soundId;
    s32                 pan;
    s32                 cond;
    u16                 frame;

    work            = (Actor400500Work*)arg0->work;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame == 0xD) {
        work->obj3.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        soundId           = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050005;
        pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_A04 >= 0x10) {
        if ((s16)work->field_A26 != 0) {
            work->field_A26 = (u16)work->field_A26 - 0x80;
        } else {
            ((Actor400500Work*)arg0->work)->field_9F0[0]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->obj3.flags                                              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->obj4.flags                                              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A08 = work->field_A08 + 1;
    }
}

static void func_actor_400500_80138CE8(Task* arg0)
{
    Actor400500Work* work;
    OverlayMat       rot;
    OverlayMat*      src;
    Task*            child;
    GfxCoord*        coord;
    s32              angle;

    work                    = (Actor400500Work*)arg0->work;
    src                     = &rot;
    angle                   = work->field_A26 - 0x80;
    work->field_A26         = angle;
    child                   = ((Actor400500Work*)arg0->work)->field_9F0[0];
    child->extra.tmd->flags = 0;
    coord                   = child->extra.tmd->coords;
    rot.ident.m00_m01       = 0x1000;
    rot.ident.m02_m10       = 0;
    src->ident.m11_m12      = 0x1000;
    rot.ident.m20_m21       = 0;
    src->ident.m22          = 0x1000;
    RotMatrixY((s16)(-angle), &src->mat);
    func_actor_400500_8013DE2C(&src->mat, &coord->coord);
    if ((s16)work->field_A26 <= 0) {
        ((Actor400500Work*)arg0->work)->field_9F0[0]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_A08                                                = work->field_A08 + 1;
    }
}

static void func_actor_400500_80138DC4(Task* arg0)
{
    Actor400500HitView* hit;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    s32                 cond;
    s32                 flag;
    u32                 rnd;

    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work = (Actor400500Work*)arg0->work;
        if (((work->field_A46 >= 0) || ((u8)work->field_A46 & 0x7F)) && (work->field_A30 == 0)) {
            flag            = 0x80;
            work->field_A46 = flag;
            work->field_A47 = 0;
        }
        ((Actor400500Work*)hit)->field_A32 = 0x3C;
        rnd                                = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState                    = rnd;
        if (!((rnd >> 0x10) & 3)) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A06 = 0;
            work2->field_A08 = 0;
            return;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 8;
        work3->field_A08 = 0;
    }
}

/// Sub-state handlers `func_actor_400500_80138EA0` copies onto the stack and runs by `field_A08`.
static const TaskFuncTable4 D_actor_400500_80131F1C = { {
    func_actor_400500_8013C108,
    func_actor_400500_8013C174,
    func_actor_400500_8013C218,
    func_actor_400500_8013C348,
} };

static void func_actor_400500_80138EA0(Task* arg0)
{
    Actor400500Work* work;
    Enemy*           enemy;
    TaskFuncTable4   sp;
    Actor400500Work* work2;
    s32              i;
    Actor400500Work* work3;

    work  = (Actor400500Work*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    sp    = D_actor_400500_80131F1C;
    if ((enemy->hp <= 0) && (work->field_A40 == 4)) {
        work->field_A42   = 0;
        work->obj1.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj2.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj3.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        return;
    }
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
}

/// Sub-state handlers `func_actor_400500_8013905C` copies onto the stack and runs by `field_A08`.
static const TaskFuncTable7 D_actor_400500_80131F2C = { {
    func_actor_400500_801391B0,
    func_actor_400500_8013C3C4,
    func_actor_400500_8013C474,
    func_actor_400500_8013C508,
    func_actor_400500_8013C578,
    func_actor_400500_8013C61C,
    func_actor_400500_8013C750,
} };

static void func_actor_400500_8013905C(Task* arg0)
{
    Actor400500Work* work;
    TaskFuncTable7   sp;
    Actor400500Work* work2;
    s32              i;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131F2C;
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
}

static void func_actor_400500_801391B0(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    u16              flags;
    u8               unused[0x30];

    work = (Actor400500Work*)arg0->work;
    if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != 1)) {
        flag            = 0x81;
        work->field_A46 = flag;
        work->field_A47 = 0;
    }
    ((Actor400500Work*)arg0->work)->field_9F0[1]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->obj1.flags                                              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj2.flags                                              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    ((Actor400500Work*)arg0->work)->field_9F0[0]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->obj3.flags                                              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    flags                                                          = work->field_A1E;
    work->obj4.flags                                              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    if (!(flags & 1)) {
        work->field_A42  = 1;
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 0xF;
        work2->field_9FA = 2;
        work->field_A08  = 3;
    } else if (!(flags & 2)) {
        work3            = (Actor400500Work*)arg0->work;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 0xF;
        work3->field_9FA = 2;
        work->field_A08  = 1;
    } else {
        work4            = (Actor400500Work*)arg0->work;
        work4->field_9F8 = 0x10;
        work4->field_9FE = 0x11;
        work4->field_9FA = 2;
        work->field_A08  = 1;
    }
}

static void func_actor_400500_801392D8(Task* arg0)
{
    Actor400500Work* work             = (Actor400500Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400500_8013C7A4, func_actor_400500_80139448 };
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              i;

    fns[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
    work4            = (Actor400500Work*)arg0->work;
    if (work4->field_A4A != 0) {
        work4->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
    }
}

static void func_actor_400500_80139448(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    Actor400500Work* work5;
    s16              mode;
    s32              soundId;
    s32              pan;
    s32              soundId2;
    s32              pan2;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A16 < 0x9C4) {
        Gp_ArmStateF0(1);
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050004;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work2 = (Actor400500Work*)arg0->work;
        if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
            flag             = 0x81;
            work2->field_A46 = flag;
            work2->field_A47 = 0;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 2;
        work3->field_A08 = 0;
        return;
    }
    mode = work->field_A3C;
    if (mode == 1) {
        soundId2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050004;
        pan2     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work4 = (Actor400500Work*)arg0->work;
        if ((work4->field_A46 >= 0) || (((u8)work4->field_A46 & 0x7F) != mode)) {
            flag             = 0x81;
            work4->field_A46 = flag;
            work4->field_A47 = 0;
        }
        work5            = (Actor400500Work*)arg0->work;
        work5->field_A06 = 0;
        work5->field_A08 = 0;
    }
}

/// Sub-state handlers `func_actor_400500_801395D0` copies onto the stack and runs by `field_A08`.
static const TaskFuncTable3 D_actor_400500_80131F48 = { {
    func_actor_400500_8013C818,
    func_actor_400500_8013973C,
    func_actor_400500_80139AC4,
} };

static void func_actor_400500_801395D0(Task* arg0)
{
    Actor400500Work* work;
    TaskFuncTable3   sp;
    Actor400500Work* work2;
    s32              i;
    Actor400500Work* work3;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131F48;
    if ((s16)work->field_A08 != 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
    }
    sp.funcs[(s16)work->field_A08](arg0);
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
}

static void func_actor_400500_8013973C(Task* arg0)
{
    OverlayMat          rot;
    MATRIX              local0;
    MATRIX              local3;
    OverlayMat*         src;
    MATRIX*             view;
    Actor400500Work*    work;
    Actor400500Work*    workRot;
    Actor400500Work*    workAnim;
    Actor400500Work*    work3;
    GfxCoord*           coordsEarly;
    GfxCoord*           coordsMain;
    GfxCoord*           coordsRot;
    GfxCoord*           part3;
    GfxCoord*           root;
    Actor400500ViewPos* pos;
    Actor400500ViewPos* pos2;
    Actor400500ViewPos* posMain;
    Actor400500ViewPos* posMain2;
    s32                 i;
    s32                 three;
    s32                 curX;
    s32                 curZ;
    s32                 tgtX;
    s32                 tgtZ;
    s32                 dx;
    s32                 dz;
    u16                 step;
    u16                 accum;
    u16                 pitch;
    s32                 y;
    s32                 viewZ;

    work = (Actor400500Work*)arg0->work;
    root = arg0->extra.tmd->coords;
    if ((s16)++work->field_A04 < 8) {
        pos2        = &work->field_9A0;
        coordsEarly = arg0->extra.tmd->coords;
        Gp_UpdateCoord(&coordsEarly[3]);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &coordsEarly[3].workm, &rot.mat);
        pos                         = pos2;
        pos->x                      = rot.mat.t[0];
        pos->z                      = rot.mat.t[2];
        coordsEarly[3].composeStamp = GRAPHICS_COORD_DIRTY;
        return;
    }
    tgtX              = (s16)work->field_950;
    curX              = work->field_9A0.x;
    tgtZ              = (s16)work->field_954;
    curZ              = work->field_9A0.z;
    work->field_9A0.x = (u16)work->field_9A0.x + ((tgtX - curX) >> 2);
    work->field_9A0.z = (u16)work->field_9A0.z + ((tgtZ - curZ) >> 2);
    posMain2          = &work->field_9A0;
    coordsMain        = arg0->extra.tmd->coords;
    part3             = &coordsMain[3];
    Gp_UpdateCoord(part3);
    view = &gGfxViewCoord.workm;
    Gp_WorldToLocal(view, &coordsMain->workm, &local0);
    Gp_WorldToLocal(view, &coordsMain[3].workm, &local3);
    posMain                    = posMain2;
    dx                         = local3.t[0] - local0.t[0];
    coordsMain->coord.t[0]     = posMain->x - dx;
    viewZ                      = posMain->z;
    dz                         = local3.t[2] - local0.t[2];
    coordsMain->coord.t[2]     = viewZ - dz;
    coordsMain->composeStamp   = GRAPHICS_COORD_DIRTY;
    coordsMain[3].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(part3);
    Gp_UpdateCoord(coordsMain);
    step             = (u16)work->field_A10 + 2;
    accum            = (u16)work->field_A12 + step;
    work->field_A12  = accum;
    work->field_A10  = step;
    y                = root->coord.t[1] + (s16)accum;
    root->coord.t[1] = y;
    pitch            = work->field_948;
    three            = 3;
    if ((pitch & 0xFFF) != 0x800) {
        work->field_948 = pitch - 0x80;
    }
    if (root->coord.t[1] >= -0x3E7) {
        y                   = -0x3E8;
        root->coord.t[0]    = (s16)work->field_950;
        root->coord.t[2]    = (s16)work->field_954;
        root->coord.t[1]    = y;
        work->field_A08     = work->field_A08 + 1;
        root->coord.t[1]    = y;
        src                 = &rot;
        work->field_948     = 0;
        work->field_94C     = 0;
        work->field_94A     = (u16)work->field_94A + 0x800;
        workRot             = (Actor400500Work*)arg0->work;
        coordsRot           = arg0->extra.tmd->coords;
        workRot->field_948 &= 0xFFF;
        workRot->field_94A &= 0xFFF;
        workRot->field_94C &= 0xFFF;
        rot.ident.m00_m01   = 0x1000;
        rot.ident.m02_m10   = 0;
        src->ident.m11_m12  = 0x1000;
        rot.ident.m20_m21   = 0;
        src->ident.m22      = 0x1000;
        RotMatrixZ(workRot->field_94C, &src->mat);
        RotMatrixX(workRot->field_948, &src->mat);
        RotMatrixY(workRot->field_94A, &src->mat);
        func_actor_400500_8013DE2C(&src->mat, &coordsRot->coord);
        work3            = (Actor400500Work*)arg0->work;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 0x19;
        work3->field_9FA = 2;
        workAnim         = (Actor400500Work*)arg0->work;
        if (workAnim->field_9FA == 1) {
            if ((s16)workAnim->field_9FC != workAnim->field_9FE) {
                workAnim->field_A00 = 0;
            } else {
                workAnim->field_A00 = func_actor_400500_8013DD8C(arg0, workAnim->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            workAnim->field_9FA = 3;
        } else if (workAnim->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            workAnim->field_9FA = three;
            workAnim->field_A00 = 0;
        } else if (workAnim->field_9FA == three) {
            workAnim->field_A00 = (u16)workAnim->field_A00 + 1;
        }
        i = 1;
        do {
            workAnim->slots[i].rate = workAnim->field_9F8;
            Gp_AnimTickIndex(&workAnim->anim, i);
            i++;
        } while (i < 0x12);
        root->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(root);
        work->field_A04 = 0;
    }
}

static void func_actor_400500_80139AC4(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    Enemy*              enemy;
    s32                 soundId;
    s32                 pan;
    s32                 flag;
    s32                 cond;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (Actor400500Work*)arg0->work;
    if (enemy->hp > 0) {
        if ((s16)++work->field_A04 == 1) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050003;
            pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A4A != 0) {
            work2->field_A4A = 0;
            func_actor_400500_8013DB64(arg0, 5);
            flag = 1;
        } else {
            flag = 0;
        }
        if (flag == 0) {
            hit = (Actor400500HitView*)arg0->work;
            if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
                (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
                cond = 1;
            } else {
                cond = 0;
            }
            if (cond) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A06 = 0;
                work3->field_A08 = 0;
                work->field_A1E |= 1;
            }
        }
    } else {
        work->field_A42 = 0;
    }
}

/// Sub-state handlers `func_actor_400500_80139C1C` copies onto the stack and runs by `field_A08`.
static const TaskFuncTable3 D_actor_400500_80131F54 = { {
    func_actor_400500_8013C908,
    func_actor_400500_80139D70,
    func_actor_400500_8013C9D4,
} };

static void func_actor_400500_80139C1C(Task* arg0)
{
    Actor400500Work* work;
    TaskFuncTable3   sp;
    Actor400500Work* work2;
    s32              i;
    Actor400500Work* work3;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131F54;
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
}

static void func_actor_400500_80139D70(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GfxCoord*        coord;
    Enemy*           enemy;
    u16              step;
    u16              accum;
    s32              y;
    s16              angle;
    s32              soundId;
    s32              pan;
    s32              soundId2;
    s32              pan2;

    coord = arg0->extra.tmd->coords;
    work  = (Actor400500Work*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((s16)++work->field_A04 < 9) {
        if (enemy->hp <= 0) {
            work->field_A42 = 0;
            return;
        }
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A4A != 0) {
            work2->field_A4A = 0;
            func_actor_400500_8013DB64(arg0, 5);
        }
    } else {
        step              = (u16)work->field_A10 - 2;
        accum             = (u16)work->field_A12 + step;
        work->field_A12   = accum;
        work->field_A10   = step;
        y                 = coord->coord.t[1] - (s16)accum;
        coord->coord.t[1] = y;
        if (work->field_948 < 0x800) {
            angle           = (u16)work->field_948 + 0x98;
            work->field_948 = angle;
            if (angle >= 0x801) {
                work->field_948 = 0x800;
            }
        }
        if (coord->coord.t[1] < -0xFA0) {
            coord->coord.t[1] = -0xFA0;
            work->field_A08   = work->field_A08 + 1;
            coord->coord.t[1] = -0xFA0;
            work->field_948   = 0;
            work->field_94C   = 0x800;
            work->field_94A   = (u16)work->field_94A + 0x800;
            work2             = (Actor400500Work*)arg0->work;
            work2->field_9F8  = 0x10;
            work2->field_9FE  = 0x19;
            work2->field_9FA  = 2;
            soundId           = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050001;
            pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            soundId2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050002;
            pan2     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(soundId2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
    }
}

static void func_actor_400500_80139F6C(Task* arg0)
{
    Actor400500Work* work             = (Actor400500Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400500_8013CA38, func_actor_400500_8013A0B8 };
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              i;

    fns[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
}

static void func_actor_400500_8013A0B8(Task* arg0)
{
    OverlayMat          rot;
    MATRIX              local2;
    OverlayMat*         src;
    Actor400500Work*    work;
    Actor400500Work*    ang;
    Actor400500Work*    work3;
    Actor400500Work*    nextWork;
    Actor400500Work*    anim;
    Actor400500HitView* hit;
    Actor400500ViewPos* pos;
    GfxCoord*           coords;
    GfxCoord*           coord;
    GfxCoord*           coord14;
    Enemy*              enemy;
    MATRIX*             view;
    Actor400500ViewPos* pos2;
    s32                 z;
    s32                 cond;
    s32                 flag;
    s32                 i;
    s32                 dx;
    s32                 dz;
    s32                 delta;
    s32                 neg;
    u32                 rnd;

    neg    = -1;
    coords = arg0->extra.tmd->coords;
    work   = (Actor400500Work*)arg0->work;
    enemy  = (Enemy*)arg0->spawnArg2.pointer;
    if ((s16)work->field_A04 == neg) {
        coord14 = &coords[0xE];
        Gp_UpdateCoord(coord14);
        pos2 = &work->field_9A0;
        view = &gGfxViewCoord.workm;
        Gp_WorldToLocal(view, &coords->workm, &rot.mat);
        Gp_WorldToLocal(view, &coord14->workm, &local2);
        dx                    = local2.t[0] - rot.mat.t[0];
        coords->coord.t[0]    = work->field_9A0.x - dx;
        pos                   = pos2;
        z                     = pos->z;
        delta                 = local2.t[2] - rot.mat.t[2];
        coords->composeStamp  = GRAPHICS_COORD_DIRTY;
        coord14->composeStamp = GRAPHICS_COORD_DIRTY;
        dz                    = delta;
        coords->coord.t[2]    = z - dz;
        Gp_UpdateCoord(coord14);
        Gp_UpdateCoord(coords);
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        src = &rot;
        if (enemy->hp > 0) {
            work->field_A1E   &= 0xFFFD;
            work->field_94A    = ((u16)work->field_94A + 0x800) & 0xFFF;
            ang                = (Actor400500Work*)arg0->work;
            coord              = arg0->extra.tmd->coords;
            ang->field_948    &= 0xFFF;
            ang->field_94A    &= 0xFFF;
            ang->field_94C    &= 0xFFF;
            rot.ident.m00_m01  = 0x1000;
            rot.ident.m02_m10  = 0;
            src->ident.m11_m12 = 0x1000;
            rot.ident.m20_m21  = 0;
            src->ident.m22     = 0x1000;
            RotMatrixZ(ang->field_94C, &src->mat);
            RotMatrixX(ang->field_948, &src->mat);
            RotMatrixY(ang->field_94A, &src->mat);
            func_actor_400500_8013DE2C(&src->mat, &coord->coord);
            work3            = (Actor400500Work*)arg0->work;
            work3->field_9F8 = 0x10;
            work3->field_9FE = 1;
            work3->field_9FA = 2;
            anim             = (Actor400500Work*)arg0->work;
            if (anim->field_9FA == 1) {
                if ((s16)anim->field_9FC != anim->field_9FE) {
                    anim->field_A00 = 0;
                } else {
                    anim->field_A00 = func_actor_400500_8013DD8C(arg0, anim->field_A00);
                }
                func_actor_400500_8013DCD4(arg0);
                anim->field_9FA = 3;
            } else if (anim->field_9FA == 2) {
                func_actor_400500_8013DC4C(arg0);
                anim->field_9FA = 3;
                anim->field_A00 = 0;
            } else if (anim->field_9FA == 3) {
                anim->field_A00 = (u16)anim->field_A00 + 1;
            }
            i = 1;
            do {
                anim->slots[i].rate = anim->field_9F8;
                Gp_AnimTickIndex(&anim->anim, i);
                i++;
            } while (i < 0x12);
            coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coords);
            work3 = (Actor400500Work*)arg0->work;
            if (work3->field_A4A != 0) {
                work3->field_A4A = 0;
                func_actor_400500_8013DB64(arg0, 5);
                flag = 1;
            } else {
                flag = 0;
            }
            if (flag == 0) {
                rnd             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rnd;
                if (((rnd >> 0x10) & 1) == 0) {
                    nextWork            = (Actor400500Work*)arg0->work;
                    nextWork->field_A06 = 0;
                    nextWork->field_A08 = 0;
                    return;
                }
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A06 = 8;
                work3->field_A08 = 0;
            }
        } else {
            work->field_A42    = 0;
            work->field_94A    = ((u16)work->field_94A + 0x800) & 0xFFF;
            ang                = (Actor400500Work*)arg0->work;
            coord              = arg0->extra.tmd->coords;
            ang->field_948    &= 0xFFF;
            ang->field_94A    &= 0xFFF;
            ang->field_94C    &= 0xFFF;
            rot.ident.m00_m01  = 0x1000;
            rot.ident.m02_m10  = 0;
            src->ident.m11_m12 = 0x1000;
            rot.ident.m20_m21  = 0;
            src->ident.m22     = 0x1000;
            RotMatrixZ(ang->field_94C, &src->mat);
            RotMatrixX(ang->field_948, &src->mat);
            RotMatrixY(ang->field_94A, &src->mat);
            func_actor_400500_8013DE2C(&src->mat, &coord->coord);
            coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coords);
        }
    }
}

/// Sub-state handlers `func_actor_400500_8013A484` copies onto the stack and runs by `field_A08`.
static const TaskFuncTable7 D_actor_400500_80131F60 = { {
    func_actor_400500_8013A5D8,
    func_actor_400500_8013D4F0,
    func_actor_400500_8013D59C,
    func_actor_400500_8013D630,
    func_actor_400500_8013D6A0,
    func_actor_400500_8013D744,
    func_actor_400500_8013D878,
} };

static void func_actor_400500_8013A484(Task* arg0)
{
    Actor400500Work* work;
    TaskFuncTable7   sp;
    Actor400500Work* work2;
    s32              i;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131F60;
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
}

static void func_actor_400500_8013A5D8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    u16              flags;
    u8               unused[0x30];

    work = (Actor400500Work*)arg0->work;
    if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != 1)) {
        flag            = 0x81;
        work->field_A46 = flag;
        work->field_A47 = 0;
    }
    ((Actor400500Work*)arg0->work)->field_9F0[1]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->obj1.flags                                              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj2.flags                                              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    ((Actor400500Work*)arg0->work)->field_9F0[0]->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->obj3.flags                                              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    flags                                                          = work->field_A1E;
    work->obj4.flags                                              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    if (!(flags & 1)) {
        work->field_A42  = 1;
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 0xF;
        work2->field_9FA = 2;
        work->field_A08  = 3;
    } else if (!(flags & 2)) {
        work3            = (Actor400500Work*)arg0->work;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 0xF;
        work3->field_9FA = 2;
        work->field_A08  = 1;
    } else {
        work4            = (Actor400500Work*)arg0->work;
        work4->field_9F8 = 0x10;
        work4->field_9FE = 0x11;
        work4->field_9FA = 2;
        work->field_A08  = 1;
    }
}

/// State handlers `func_actor_400500_8013A700` copies onto the stack and runs
/// by `field_A06`.
static const TaskFuncTable10 D_actor_400500_80131F7C = { {
    func_actor_400500_8013A8E4,
    func_actor_400500_8013AA98,
    func_actor_400500_8013D8CC,
    func_actor_400500_8013D958,
    func_actor_400500_8013ABE4,
    func_actor_400500_8013D9DC,
    func_actor_400500_8013D9F4,
    func_actor_400500_8013DA24,
    func_actor_400500_8013DA68,
    func_actor_400500_8013DACC,
} };

/// Relights the actor for the world position of `coord`, staged in a `VECTOR`
/// taken off the scratch stack, and sets the back colour of `obj`: a dim grey
/// in rooms 1, 3, 5 and 6 of the stage, a green tint everywhere else.
static inline void _actor400500UpdateColor(Task* arg0, GfxCoord* coord, TmdObject* obj)
{
    VECTOR* block;
    u8      room;

    block                        = (VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10);
    block->vx                    = coord->workm.t[0];
    block->vy                    = coord->workm.t[1];
    block->vz                    = coord->workm.t[2];
    SCRATCH_STACK_CURSOR(VECTOR) = block;
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, block, 0, 0);
    room = gGameSession->location.loc.room;
    if ((room == 1) || (room == 3) || (room == 5) || (room == 6)) {
        Gp_SetObjTrans(obj, 0x200, 0x200, 0x200);
    } else {
        Gp_SetObjTrans(obj, 0x400, 0x1000, 0x400);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_400500_8013A700(Task* arg0)
{
    TmdObject*       extra;
    Actor400500Work* work;
    TaskFuncTable10  sp;

    extra = arg0->extra.tmd;
    work  = (Actor400500Work*)arg0->work;
    sp    = D_actor_400500_80131F7C;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            sp.funcs[(s16)work->field_A06](arg0);
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor400500UpdateColor(arg0, &arg0->extra.tmd->coords[1], arg0->extra.tmd);
            if (work->field_A28 != 0) {
                func_actor_400500_80132AB0(arg0, -0xFA0, (u8)(work->field_A28 >> 2));
                func_actor_400500_80132AB0(arg0, -0x3E8, (u8)work->field_A28);
            }
            return;
    }
}

static void func_actor_400500_8013A8E4(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Enemy*           enemy;
    s32              mapped;
    s32              i;

    work            = (Actor400500Work*)arg0->work;
    enemy           = (Enemy*)arg0->spawnArg2.pointer;
    mapped          = D_actor_400500_80153DD4[work->field_9FE];
    work->field_9F8 = 0x10;
    work->field_9FA = 2;
    work->field_9FE = mapped;
    work2           = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add(arg0, 0);
    enemy->recs = 0;
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj1);
    Gp_UnlinkObj(&work->obj3);
    Gp_UnlinkObj(&work->obj2);
    Gp_UnlinkObj(&work->obj4);
    GameFlag_SetNibble(0xCE, 1);
    if (work->field_A40 == 4) {
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 6;
        work3->field_A08 = 0;
        return;
    }
    work->field_A06 = work->field_A06 + 1;
}

static void func_actor_400500_8013AA98(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    s32                 i;
    s32                 cond;

    work  = (Actor400500Work*)arg0->work;
    work2 = work;
    if (work->field_9FA == 1) {
        if ((s16)work->field_9FC != work->field_9FE) {
            work->field_A00 = 0;
        } else {
            work->field_A00 = func_actor_400500_8013DD8C(arg0, work->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work->field_9FA = 3;
        work->field_A00 = 0;
    } else if (work->field_9FA == 3) {
        work->field_A00 = (u16)work->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A06 = work->field_A06 + 1;
    }
}

static void func_actor_400500_8013ABE4(Task* arg0)
{
    Actor400500Work* work;
    TmdObject*       model;
    GfxCoord*        coord;
    VECTOR           scale;
    SVECTOR          pos;
    u16              frame;

    work  = (Actor400500Work*)arg0->work;
    model = arg0->extra.tmd;
    coord = model->coords;

    work->field_A20           = (u16)work->field_A20 + ((s16)(0xFF - (u16)work->field_A20) >> 4);
    work->field_A24           = (u16)work->field_A24 + ((s16) - (u16)work->field_A24 >> 4);
    work->field_A28           = (u16)work->field_A28 + (-work->field_A28 >> 4);
    model->shading.colorBlend = work->field_A24;
    func_8009EA50(work->field_A20);

    work->field_A02 = (u16)work->field_A02 - 0x30;
    scale.vx        = 0x1000;
    scale.vy        = work->field_A02;
    scale.vz        = 0x1000;
    coord->coord    = work->matrix_808;
    ScaleMatrix(&coord->coord, &scale);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame == 0x10) {
        pos.vx = 0;
        pos.vy = 0;
        pos.vz = 0;
        Gp_SpawnEff(0x600A5, coord, 5, &pos);
    }
    if ((s16)work->field_A04 >= 0x41) {
        model->flags   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->field_A06 = work->field_A06 + 1;
    }
}

/// Sub-state handlers `func_actor_400500_8013AD60` copies onto the stack and runs by `field_A08`.
static const TaskFuncTable11 D_actor_400500_80131FA4 = { {
    func_actor_400500_8013AF44,
    func_actor_400500_8013B228,
    func_actor_400500_8013CB0C,
    func_actor_400500_8013CBD8,
    func_actor_400500_8013CCDC,
    func_actor_400500_8013B374,
    func_actor_400500_8013CDA8,
    func_actor_400500_8013CE9C,
    func_actor_400500_8013CF68,
    func_actor_400500_8013D078,
    func_actor_400500_8013D144,
} };

static void func_actor_400500_8013AD60(Task* arg0)
{
    Actor400500Work* work;
    TaskFuncTable11  sp;
    Enemy*           enemy;
    Actor400500Work* work2;
    s32              i;
    Actor400500Work* work3;
    s32              flag;

    work  = (Actor400500Work*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    sp    = D_actor_400500_80131FA4;
    if ((s16)work->field_A08 != 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
    }
    if (enemy->hp > 0) {
        sp.funcs[(s16)work->field_A08](arg0);
    } else {
        work->field_A42 = 0;
    }
    work3 = (Actor400500Work*)arg0->work;
    if (((work3->field_A46 >= 0) || ((u8)work3->field_A46 & 0x7F)) && (work3->field_A30 == 0)) {
        flag             = 0x80;
        work3->field_A46 = flag;
        work3->field_A47 = 0;
    }
}

static void func_actor_400500_8013AF44(Task* arg0)
{
    MATRIX              local;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    GfxCoord*           coord;
    GfxCoord*           coords;
    Actor400500ViewPos* pos;
    Actor400500ViewPos* pos2;
    s32                 flag;
    s32                 heading;
    s32                 i;
    u16                 a1c;

    work    = (Actor400500Work*)arg0->work;
    heading = (u16)work->field_94A & 0xFFF;
    coord   = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0)) {
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 4;
        work2->field_9FA = 2;
        work3            = (Actor400500Work*)arg0->work;
        if (work3->field_9FA == 1) {
            if ((s16)work3->field_9FC != work3->field_9FE) {
                work3->field_A00 = 0;
            } else {
                work3->field_A00 = func_actor_400500_8013DD8C(arg0, work3->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work3->field_9FA = 3;
        } else if (work3->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work3->field_9FA = 3;
            work3->field_A00 = 0;
        } else if (work3->field_9FA == 3) {
            work3->field_A00 = (u16)work3->field_A00 + 1;
        }
        i = 1;
        do {
            work3->slots[i].rate = work3->field_9F8;
            Gp_AnimTickIndex(&work3->anim, i);
            i++;
        } while (i < 0x12);
        switch ((s16)((u16)work->field_A1A - 1)) {
            case 3:
                if (heading != 0x400) {
                    work->field_A08 = 4;
                } else {
                    work->field_A08 = 1;
                }
                break;
            case 0:
                if (work->field_9E0 >= 0) {
                    work->field_A08 = 3;
                } else {
                    work->field_A08 = 1;
                }
                break;
            case 1:
                a1c = work->field_A1C;
                if ((u32)(a1c - 1) < 2U) {
                    work->field_A08 = 3;
                } else if (((s16)a1c == 4) || ((s16)work->field_A1C == 5)) {
                    work->field_A08 = 3;
                } else if (((s16)a1c == 3) && (heading == 0)) {
                    if (coord->coord.t[0] >= 0x4074) {
                        work->field_A08 = 6;
                    } else {
                        work->field_A08 = 1;
                    }
                } else {
                    work->field_A08 = 1;
                }
                break;
            case 2:
                if (work->field_9E4 < 0) {
                    work->field_A08 = 6;
                } else {
                    work->field_A08 = 8;
                }
                break;
            case 5:
                if (heading == 0x800) {
                    work->field_A08 = 8;
                } else {
                    work->field_A08 = 7;
                }
                break;
            default:
                coord->coord.t[0] = -0x3E8;
                coord->coord.t[1] = -0xFA0;
                coord->coord.t[2] = -0x2116;
                work->field_94A   = 0x400;
                work->field_A08   = 1;
                break;
        }
        pos2   = &work->field_9A0;
        coords = arg0->extra.tmd->coords;
        Gp_UpdateCoord(&coords[8]);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &coords[8].workm, &local);
        pos                    = pos2;
        pos->x                 = local.t[0];
        pos->z                 = local.t[2];
        coords[8].composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

static void func_actor_400500_8013B228(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    GfxCoord*        coord;
    s32              flag;
    s32              a1a;
    u16              a1c;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) != 0x400) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A06 = 9;
            work2->field_A08 = 0;
        } else {
            a1a = work->field_A1A;
            if (a1a != 1) {
                if ((a1a == 2) && (coord->coord.t[0] >= 0x4075)) {
                    a1c = work->field_A1C;
                    if (((u32)(a1c - 2) < 2U) || ((s16)a1c == 6)) {
                        work->field_A08 = 5;
                    } else {
                        work->field_A08 = a1a;
                    }
                } else {
                    func_actor_400500_8013403C(arg0);
                }
            } else {
                if (work->field_9E0 < -0xF9F) {
                    work3            = (Actor400500Work*)arg0->work;
                    work3->field_A06 = 9;
                    work3->field_A08 = 0;
                }
                func_actor_400500_8013403C(arg0);
            }
        }
        coord->coord.t[2] = -0x209E;
    }
}

static void func_actor_400500_8013B374(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    u16              a1c;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        a1c = work->field_A1C;
        if (((u32)(a1c - 2) < 2U) || ((s16)a1c == 6)) {
            if (!((u16)work->field_94A & 0xFFF)) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x10;
                work2->field_9FE = 4;
                work2->field_9FA = 2;
                work->field_A08  = 6;
                return;
            }
        } else if (((u16)work->field_94A & 0xFFF) == 0xC00) {
            work3            = (Actor400500Work*)arg0->work;
            work3->field_9F8 = 0x10;
            work3->field_9FE = 4;
            work3->field_9FA = 2;
            work->field_A08  = 3;
            return;
        }
        work4            = (Actor400500Work*)arg0->work;
        work4->field_A06 = 9;
        work4->field_A08 = 0;
    }
}

static void func_actor_400500_8013B4A4(Task* arg0)
{
    Actor400500Work* work;
    Enemy*           enemy;
    u8               mode;

    work  = (Actor400500Work*)arg0->work;
    mode  = gGameSession->location.loc.room;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((mode == 1) || (mode == 3) || (mode == 5) || (mode == 6)) {
        s16 hp;
        s32 maxHp;
        s32 quarter;

        hp      = enemy->hp;
        maxHp   = enemy->hpMax << 0x10;
        quarter = maxHp >> 0x12;
        if ((quarter + (maxHp >> 0x11)) < hp) {
            work->field_A2C = 0x10;
            work->field_A2E = 0;
            return;
        }
        if (quarter < hp) {
            work->field_A2C = 0x20;
            work->field_A2E = 0x40;
            return;
        }
        if ((maxHp >> 0x13) < hp) {
            work->field_A2C = 0x30;
            work->field_A2E = 0x80;
            return;
        }
        if ((maxHp >> 0x14) < hp) {
            work->field_A2C = 0x40;
            work->field_A2E = 0xC0;
            return;
        }
        work->field_A2C = 0x50;
        work->field_A2E = 0x100;
        return;
    } else {
        s16 hp;
        s32 maxHp;
        s32 quarter;

        hp      = enemy->hp;
        maxHp   = enemy->hpMax << 0x10;
        quarter = maxHp >> 0x12;
        if ((quarter + (maxHp >> 0x11)) < hp) {
            work->field_A2C = 0x2000;
            work->field_A2E = 0x20;
            return;
        }
        if (quarter < hp) {
            work->field_A2C = 0x2000;
            work->field_A2E = 0x40;
            return;
        }
        if ((maxHp >> 0x13) < hp) {
            work->field_A2C = 0x2000;
            work->field_A2E = 0x80;
            return;
        }
        if ((maxHp >> 0x14) < hp) {
            work->field_A2C = 0x2000;
            work->field_A2E = 0xC0;
            return;
        }
        work->field_A2C = 0x2000;
        work->field_A2E = 0x100;
    }
}

/// Sub-state handlers `func_actor_400500_8013B5E0` copies onto the stack and runs by `field_A08`.
static const TaskFuncTable5 D_actor_400500_80131FEC = { {
    func_actor_400500_8013D210,
    func_actor_400500_8013D274,
    func_actor_400500_8013D2D8,
    func_actor_400500_8013D3B8,
    func_actor_400500_8013D420,
} };

static void func_actor_400500_8013B5E0(Task* arg0)
{
    Actor400500Work* work;
    TaskFuncTable5   sp;
    Actor400500Work* work2;
    s32              i;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131FEC;
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
}

static s32 func_actor_400500_8013B720(GfxCoord* arg0, MATRIX* arg1)
{
    MATRIX    matrix;
    MATRIX    parent;
    MATRIX    normal;
    MATRIX    transposed;
    GfxCoord* coord;
    GfxCoord* view;
    MATRIX*   parentp;

    coord = arg0->parent;
    if (coord == &gGfxViewCoord) {
        return 0;
    }
    view    = &gGfxViewCoord;
    parentp = &parent;
    matrix  = coord->coord;
    while (1) {
        coord = coord->parent;
        if (coord == NULL) {
            return 0;
        }
        if (coord == view) {
            break;
        }
        parent = coord->coord;
        MatrixNormal(parentp, parentp);
        gte_SetRotMatrix(parentp);
        MulRotMatrix(&matrix);
        MatrixNormal(&matrix, &normal);
        matrix = normal;
    }
    gte_TransposeMatrix(&matrix, &transposed);
    gte_SetRotMatrix(&transposed);
    MulRotMatrix(arg1);
    return 1;
}

#include "../../shared/coord_math_local_to_world.inc.c"

static void func_actor_400500_8013BA24(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    TaskFuncTable3   sp;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131EE4;
    sp.funcs[(s16)work->field_A08](arg0);
    work2            = (Actor400500Work*)arg0->work;
    work2->field_A3C = 0;
    work2->field_A3E = 0;
    work2            = (Actor400500Work*)arg0->work;
    work2->field_A49 = 0;
}

static void func_actor_400500_8013BAA4(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A40 == 4) {
        work->field_A42 = 0;
        return;
    }
    if (!(work->field_A1E & 1)) {
        work->field_A42  = 1;
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 1;
        work2->field_9FA = 2;
        work->field_A04  = 0;
        work->field_A18  = 0;
        work->field_A10  = 0;
        work->field_A12  = 0;
        work->field_A0A  = work->field_A0A + 1;
        return;
    }
    work->field_A42 = 0;
}

static void func_actor_400500_8013BB18(Task* arg0)
{
    Actor400500Work* work;
    GfxCoord*        coord;

    work               = (Actor400500Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->field_A10   += 2;
    work->field_A12   += work->field_A10;
    coord->coord.t[1] += work->field_A12;
    if (coord->coord.t[1] >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->field_A0A++;
        _actor400500SetAnim(arg0, 0x13, 0x10);
        work->field_94C  += 0x800;
        coord->coord.t[1] = -0x3E8;
        work->field_A04   = 0;
    }
}

static void func_actor_400500_8013BBB0(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    s32                 soundId;
    s32                 pan;
    s32                 cond;

    work = (Actor400500Work*)arg0->work;
    if ((s16)work->field_A04 == 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050006;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_A04 = work->field_A04 + 1;
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A42 = 0;
    }
}

static void func_actor_400500_8013BC9C(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    work->field_A42 = 1;
    work->field_A04 = 0;
    work->field_A18 = 0;
    work->field_A10 = 0;
    work->field_A12 = 0;
    work->field_A0A = work->field_A0A + 1;
}

static void func_actor_400500_8013BCCC(Task* arg0)
{
    Actor400500Work* work;
    GfxCoord*        coord;

    work               = (Actor400500Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->field_A10   += 2;
    work->field_A12   += work->field_A10;
    coord->coord.t[1] += work->field_A12;
    if (coord->coord.t[1] >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->field_A0A++;
        _actor400500SetAnim(arg0, 0x13, 0x10);
        work->field_94C  += 0x800;
        coord->coord.t[1] = -0x3E8;
        work->field_A04   = 0;
    }
}

static void func_actor_400500_8013BD64(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    s32                 soundId;
    s32                 pan;
    s32                 cond;

    work = (Actor400500Work*)arg0->work;
    if ((s16)work->field_A04 == 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050006;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_A04 = work->field_A04 + 1;
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A42 = 0;
    }
}

static void func_actor_400500_8013BE50(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work            = (Actor400500Work*)arg0->work;
    work->field_9F8 = 0x10;
    work2           = (Actor400500Work*)arg0->work;
    if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
        flag             = 0x81;
        work2->field_A46 = flag;
        work2->field_A47 = 0;
    }
    work3            = (Actor400500Work*)arg0->work;
    work3->field_9F8 = 0x10;
    work3->field_9FE = 1;
    work3->field_9FA = 2;
    work->field_A04  = 0;
    work->field_A18  = 0;
    work->field_A26  = 0;
    work->field_A08  = work->field_A08 + 1;
}

static void func_actor_400500_8013BEC4(Task* arg0)
{
    Actor400500Work* work;
    OverlayMat       rot;
    OverlayMat*      src;
    Task*            child;
    GfxCoord*        coord;
    Actor400500Work* work2;
    s32              angle;

    work                    = (Actor400500Work*)arg0->work;
    src                     = &rot;
    work->field_A26         = work->field_A26 + 0x80;
    work->field_A04         = work->field_A04 + 1;
    child                   = ((Actor400500Work*)arg0->work)->field_9F0[1];
    angle                   = work->field_A26;
    child->extra.tmd->flags = 0;
    coord                   = child->extra.tmd->coords;
    rot.ident.m00_m01       = 0x1000;
    rot.ident.m02_m10       = 0;
    src->ident.m11_m12      = 0x1000;
    rot.ident.m20_m21       = 0;
    src->ident.m22          = 0x1000;
    RotMatrixY((s16)(angle), &src->mat);
    func_actor_400500_8013DE2C(&src->mat, &coord->coord);
    if ((s16)work->field_A26 >= 0x200) {
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 8;
        work2->field_9FA = 2;
        work->field_A18  = 0;
        work->field_A04  = 0;
        work->field_A08  = work->field_A08 + 1;
    }
}

static void func_actor_400500_8013BFB0(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != 1)) {
        flag            = 0x81;
        work->field_A46 = flag;
        work->field_A47 = 0;
    }
    work2            = (Actor400500Work*)arg0->work;
    work2->field_9F8 = 0x10;
    work2->field_9FE = 1;
    work2->field_9FA = 2;
    work->field_A04  = 0;
    work->field_A18  = 0;
    work->field_A26  = 0;
    work->field_A08  = work->field_A08 + 1;
}

static void func_actor_400500_8013C018(Task* arg0)
{
    Actor400500Work* work;
    OverlayMat       rot;
    OverlayMat*      src;
    Task*            child;
    GfxCoord*        coord;
    Actor400500Work* work2;
    s32              angle;

    work                    = (Actor400500Work*)arg0->work;
    src                     = &rot;
    work->field_A26         = work->field_A26 + 0x80;
    work->field_A04         = work->field_A04 + 1;
    child                   = ((Actor400500Work*)arg0->work)->field_9F0[0];
    angle                   = work->field_A26;
    child->extra.tmd->flags = 0;
    coord                   = child->extra.tmd->coords;
    rot.ident.m00_m01       = 0x1000;
    rot.ident.m02_m10       = 0;
    src->ident.m11_m12      = 0x1000;
    rot.ident.m20_m21       = 0;
    src->ident.m22          = 0x1000;
    RotMatrixY((s16)(-angle), &src->mat);
    func_actor_400500_8013DE2C(&src->mat, &coord->coord);
    if ((s16)work->field_A26 >= 0x200) {
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 7;
        work2->field_9FA = 2;
        work->field_A18  = 0;
        work->field_A04  = 0;
        work->field_A08  = work->field_A08 + 1;
    }
}

static void func_actor_400500_8013C108(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != 1)) {
        flag            = 0x81;
        work->field_A46 = flag;
        work->field_A47 = 0;
    }
    work2            = (Actor400500Work*)arg0->work;
    work2->field_9F8 = 0x10;
    work2->field_9FE = 1;
    work2->field_9FA = 2;
    work->field_A04  = 0;
    work->field_A18  = 0;
    work->field_A10  = 0;
    work->field_A12  = 0;
    work->field_A08  = work->field_A08 + 1;
}

static void func_actor_400500_8013C174(Task* arg0)
{
    Actor400500Work* work;
    GfxCoord*        coord;

    work               = (Actor400500Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->field_A10   += 2;
    work->field_A12   += work->field_A10;
    coord->coord.t[1] += work->field_A12;
    if (coord->coord.t[1] >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->field_A08++;
        _actor400500SetAnim(arg0, 0x13, 0x10);
        work->field_94C  += 0x800;
        coord->coord.t[1] = -0x3E8;
        work->field_A04   = 0;
        work->field_A1E  |= 2;
    }
}

static void func_actor_400500_8013C218(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    Actor400500Work*    work2;
    Enemy*              enemy;
    s32                 soundId;
    s32                 pan;
    s32                 cond;

    work  = (Actor400500Work*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((s16)work->field_A04 == 0) {
        soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050006;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_A04 = work->field_A04 + 1;
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (enemy->hp > 0) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0x14;
            work2->field_9FA = 2;
            work->field_A08  = work->field_A08 + 1;
        } else {
            work->field_A42 = 0;
        }
    }
}

static void func_actor_400500_8013C348(Task* arg0)
{
    Actor400500HitView* hit;
    Actor400500Work*    work2;
    Enemy*              enemy;
    s32                 cond;

    hit   = (Actor400500HitView*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (enemy->hp > 0) {
            work2                              = (Actor400500Work*)arg0->work;
            work2->field_A06                   = 0xA;
            work2->field_A08                   = 0;
            ((Actor400500Work*)hit)->field_A1E = ((Actor400500Work*)hit)->field_A1E | 1;
            return;
        }
        ((Actor400500Work*)hit)->field_A42 = 0;
    }
}

static void func_actor_400500_8013C3C4(Task* arg0)
{
    Enemy*              enemy;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    s32                 cond;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (Actor400500Work*)arg0->work;
    if (enemy->hp > 0) {
        hit = (Actor400500HitView*)work;
        if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            if (!(work->field_A1E & 2)) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x10;
                work2->field_9FE = 0x10;
                work2->field_9FA = 2;
            } else {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_9F8 = 0x10;
                work3->field_9FE = 0x12;
                work3->field_9FA = 2;
            }
            work->field_A08 = 2;
        }
    } else {
        work->field_A42 = 0;
    }
}

static void func_actor_400500_8013C474(Task* arg0)
{
    Enemy*              enemy;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    s32                 cond;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (Actor400500Work*)arg0->work;
    if (enemy->hp > 0) {
        hit = (Actor400500HitView*)work;
        if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_A4A = 0;
            if (!(work->field_A1E & 2)) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A06 = 0;
                work2->field_A08 = 0;
            } else {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A06 = 0xA;
                work3->field_A08 = 0;
            }
        }
    } else {
        work->field_A42 = 0;
    }
}

static void func_actor_400500_8013C508(Task* arg0)
{
    Actor400500HitView* hit;
    Actor400500Work*    work;
    Enemy*              enemy;
    s32                 cond;

    hit   = (Actor400500HitView*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond || (enemy->hp <= 0)) {
        work            = (Actor400500Work*)hit;
        work->field_A04 = 0;
        work->field_A18 = 0;
        work->field_A10 = 0;
        work->field_A12 = 0;
        work->field_A08 = work->field_A08 + 1;
    }
}

static void func_actor_400500_8013C578(Task* arg0)
{
    Actor400500Work* work;
    GfxCoord*        coord;

    work               = (Actor400500Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->field_A10   += 2;
    work->field_A12   += work->field_A10;
    coord->coord.t[1] += work->field_A12;
    if (coord->coord.t[1] >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->field_A08++;
        _actor400500SetAnim(arg0, 0x13, 0x10);
        work->field_94C  += 0x800;
        coord->coord.t[1] = -0x3E8;
        work->field_A04   = 0;
        work->field_A1E  |= 1;
    }
}

static void func_actor_400500_8013C61C(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    Actor400500Work*    work2;
    Enemy*              enemy;
    s32                 soundId;
    s32                 pan;
    s32                 cond;

    work  = (Actor400500Work*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((s16)work->field_A04 == 0) {
        soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050006;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_A04 = work->field_A04 + 1;
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A4A = 0;
        if (enemy->hp > 0) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0x14;
            work2->field_9FA = 2;
            work->field_A08  = work->field_A08 + 1;
        } else {
            work->field_A42 = 0;
        }
    }
}

static void func_actor_400500_8013C750(Task* arg0)
{
    Actor400500HitView* work;
    Actor400500Work*    work2;
    s32                 cond;

    work = (Actor400500HitView*)arg0->work;
    if ((work->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor400500Work*)arg0->work;
        work2->field_A06 = 0xA;
        work2->field_A08 = 0;
    }
}

static void func_actor_400500_8013C7A4(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work            = (Actor400500Work*)arg0->work;
    work->field_9F8 = 0x10;
    work2           = (Actor400500Work*)arg0->work;
    if (((work2->field_A46 >= 0) || ((u8)work2->field_A46 & 0x7F)) && (work2->field_A30 == 0)) {
        flag             = 0x80;
        work2->field_A46 = flag;
        work2->field_A47 = 0;
    }
    work3            = (Actor400500Work*)arg0->work;
    work3->field_9F8 = 0x10;
    work3->field_9FE = 1;
    work3->field_9FA = 2;
    work->field_A08  = work->field_A08 + 1;
}

static void func_actor_400500_8013C818(Task* task)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GfxCoord*        coord;
    s32              soundId;
    s32              pan;

    coord   = task->extra.tmd->coords;
    soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050004;
    work    = (Actor400500Work*)task->work;
    pan     = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    work2            = (Actor400500Work*)task->work;
    work2->field_9F8 = 0x10;
    work2->field_9FE = 0x20;
    work2->field_9FA = 2;
    work->field_A04  = 0;
    work->field_A18  = 0;
    work->field_A10  = 0x80;
    work->field_A12  = 0;
    work->field_A04  = 0;
    work->field_A08  = work->field_A08 + 1;
    work->field_950  = (u16)coord->coord.t[0];
    work->field_954  = (u16)coord->coord.t[2];
}

static void func_actor_400500_8013C908(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              soundId;
    s32              pan;

    work    = (Actor400500Work*)arg0->work;
    soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050004;
    pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work2            = (Actor400500Work*)arg0->work;
    work2->field_9F8 = 0x10;
    work2->field_9FE = 0x15;
    work2->field_9FA = 2;
    work->field_A04  = 0;
    work->field_A18  = 0;
    work->field_A10  = 0;
    work->field_A12  = 0x12C;
    work->field_948  = 0;
    work->field_A08  = work->field_A08 + 1;
}

static void func_actor_400500_8013C9D4(Task* arg0)
{
    Actor400500HitView* work;
    Actor400500Work*    work2;
    s32                 cond;

    work = (Actor400500HitView*)arg0->work;
    if ((work->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2                                = (Actor400500Work*)arg0->work;
        work2->field_A06                     = 0;
        work2->field_A08                     = 0;
        ((Actor400500Work*)work)->field_A1E &= 0xFFFE;
    }
}

static void func_actor_400500_8013CA38(Task* arg0)
{
    MATRIX              local;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    GfxCoord*           coords;
    Actor400500ViewPos* pos;
    Actor400500ViewPos* pos2;
    s32                 heading;
    s32                 masked;
    s32                 neg;

    work             = (Actor400500Work*)arg0->work;
    heading          = (u16)work->field_94A;
    work->field_A04  = 0;
    work2            = (Actor400500Work*)arg0->work;
    work2->field_9F8 = 0x10;
    work2->field_9FE = 0x16;
    work2->field_9FA = 2;
    masked           = heading & 0xFFF;
    if ((work->field_A1A == 1) && ((masked == 0x400) || (masked == 0xC00))) {
        neg             = -1;
        work->field_A04 = neg;
        pos2            = &work->field_9A0;
        coords          = arg0->extra.tmd->coords;
        Gp_UpdateCoord(&coords[0xE]);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &coords[0xE].workm, &local);
        pos                      = pos2;
        pos->x                   = local.t[0];
        pos->z                   = local.t[2];
        coords[0xE].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    work->field_A08 = work->field_A08 + 1;
}

static void func_actor_400500_8013CB0C(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) == 0xC00) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 3;
            return;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

static void func_actor_400500_8013CBD8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    GfxCoord*        coord;
    s32              flag;
    s32              a1a;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) != 0xC00) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A06 = 9;
            work2->field_A08 = 0;
        } else {
            a1a = work->field_A1A;
            if (a1a != 1) {
                if (a1a == 4) {
                    work->field_A08 = a1a;
                }
            } else if (work->field_9E0 >= 0xFA0) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A06 = 9;
                work3->field_A08 = 0;
            }
            func_actor_400500_8013403C(arg0);
        }
        coord->coord.t[2] = -0x209E;
    }
}

static void func_actor_400500_8013CCDC(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) == 0x400) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 1;
            return;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

static void func_actor_400500_8013CDA8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GfxCoord*        coord;
    s32              flag;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if ((u16)work->field_94A & 0xFFF) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A06 = 9;
            work2->field_A08 = 0;
        } else {
            if (work->field_A1A != 3) {
                if (work->field_A1A == 6) {
                    work->field_A08 = 7;
                }
            } else if (work->field_9E4 > 0) {
                work->field_A08 = 7;
            }
            func_actor_400500_8013403C(arg0);
        }
        coord->coord.t[0] = 0x4074;
    }
}

static void func_actor_400500_8013CE9C(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) == 0x800) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 8;
            return;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

static void func_actor_400500_8013CF68(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GfxCoord*        coord;
    s32              flag;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) != 0x800) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A06 = 9;
            work2->field_A08 = 0;
        } else {
            switch (work->field_A1A) {
                case 2:
                    if (coord->coord.t[2] < -0x209E) {
                        work->field_A08 = 9;
                    }
                    break;
                case 3:
                    if (work->field_9E4 < 0) {
                        work->field_A08 = 0xA;
                    }
                    break;
            }
            func_actor_400500_8013403C(arg0);
        }
        coord->coord.t[0] = 0x4074;
    }
}

static void func_actor_400500_8013D078(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) == 0xC00) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 3;
            return;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

static void func_actor_400500_8013D144(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) == 0) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 6;
            return;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

static void func_actor_400500_8013D210(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GfxCoord*        coord;

    work              = (Actor400500Work*)arg0->work;
    coord             = arg0->extra.tmd->coords;
    work->field_94C   = 0x800;
    work->field_94A   = 0;
    coord->coord.t[0] = 0x4074;
    coord->coord.t[1] = -0xFA0;
    coord->coord.t[2] = -0x2710;
    work->field_A04   = 0;
    work2             = (Actor400500Work*)arg0->work;
    work2->field_9F8  = 4;
    work2->field_9FE  = 1;
    work2->field_9FA  = 2;
    work->field_A08   = work->field_A08 + 1;
}

static void func_actor_400500_8013D274(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if ((u8)work->field_A4B == 2) {
        work->field_A2C = 0x258;
        work2           = (Actor400500Work*)arg0->work;
        if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
            flag             = 0x81;
            work2->field_A46 = flag;
            work2->field_A47 = 0;
        }
        work->field_A04 = 0;
        work->field_A08 = work->field_A08 + 1;
    }
}

static void func_actor_400500_8013D2D8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    GfxCoord*        coord;
    s32              flag;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if ((s16)++work->field_A04 == 1) {
        work2 = (Actor400500Work*)arg0->work;
        if (((work2->field_A46 >= 0) || ((u8)work2->field_A46 & 0x7F)) && (work2->field_A30 == 0)) {
            flag             = 0x80;
            work2->field_A46 = flag;
            work2->field_A47 = 0;
        }
    }
    func_actor_400500_80133B14(arg0);
    if (coord->coord.t[2] >= -0x225F) {
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A0E = 0xA;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 1;
        work3->field_9FA = 1;
        work->field_A08  = work->field_A08 + 1;
    }
}

static void func_actor_400500_8013D3B8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if ((u8)work->field_A4B == 3) {
        work->field_A04 = 0;
        work->field_A2C = 0x10;
        work2           = (Actor400500Work*)arg0->work;
        if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
            flag             = 0x81;
            work2->field_A46 = flag;
            work2->field_A47 = 0;
        }
        work->field_A08 = work->field_A08 + 1;
    }
}

static void func_actor_400500_8013D420(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              soundId;
    s32              pan;

    work = (Actor400500Work*)arg0->work;
    if ((s16)++work->field_A04 == 0x1E) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050004;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if ((u8)work->field_A4B == 4) {
        work->field_A4C  = 0;
        work2            = (Actor400500Work*)arg0->work;
        work2->field_A06 = 0;
        work2->field_A08 = 0;
    }
}

static void func_actor_400500_8013D4F0(Task* arg0)
{
    Enemy*           enemy;
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (Actor400500Work*)arg0->work;
    if (enemy->hp > 0) {
        if (Gp_TickObjFlag2(enemy) != 0) {
            if (!(work->field_A1E & 2)) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x10;
                work2->field_9FE = 0x10;
                work2->field_9FA = 2;
            } else {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_9F8 = 0x10;
                work3->field_9FE = 0x12;
                work3->field_9FA = 2;
            }
            work->field_A08 = 2;
        }
    } else {
        work->field_A42 = 0;
    }
}

static void func_actor_400500_8013D59C(Task* arg0)
{
    Enemy*              enemy;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    s32                 cond;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (Actor400500Work*)arg0->work;
    if (enemy->hp > 0) {
        hit = (Actor400500HitView*)work;
        if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_A4A = 0;
            if (!(work->field_A1E & 2)) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A06 = 0;
                work2->field_A08 = 0;
            } else {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A06 = 0xA;
                work3->field_A08 = 0;
            }
        }
    } else {
        work->field_A42 = 0;
    }
}

static void func_actor_400500_8013D630(Task* arg0)
{
    Actor400500HitView* hit;
    Actor400500Work*    work;
    Enemy*              enemy;
    s32                 cond;

    hit   = (Actor400500HitView*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond || (enemy->hp <= 0)) {
        work            = (Actor400500Work*)hit;
        work->field_A04 = 0;
        work->field_A18 = 0;
        work->field_A10 = 0;
        work->field_A12 = 0;
        work->field_A08 = work->field_A08 + 1;
    }
}

static void func_actor_400500_8013D6A0(Task* arg0)
{
    Actor400500Work* work;
    GfxCoord*        coord;

    work               = (Actor400500Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->field_A10   += 2;
    work->field_A12   += work->field_A10;
    coord->coord.t[1] += work->field_A12;
    if (coord->coord.t[1] >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->field_A08++;
        _actor400500SetAnim(arg0, 0x13, 0x10);
        work->field_94C  += 0x800;
        coord->coord.t[1] = -0x3E8;
        work->field_A04   = 0;
        work->field_A1E  |= 1;
    }
}

static void func_actor_400500_8013D744(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    Actor400500Work*    work2;
    Enemy*              enemy;
    s32                 soundId;
    s32                 pan;
    s32                 cond;

    work  = (Actor400500Work*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    if ((s16)work->field_A04 == 0) {
        soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40050006;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_A04 = work->field_A04 + 1;
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (hit->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A4A = 0;
        if (enemy->hp > 0) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0x14;
            work2->field_9FA = 2;
            work->field_A08  = work->field_A08 + 1;
        } else {
            work->field_A42 = 0;
        }
    }
}

static void func_actor_400500_8013D878(Task* arg0)
{
    Actor400500HitView* work;
    Actor400500Work*    work2;
    s32                 cond;

    work = (Actor400500HitView*)arg0->work;
    if ((work->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor400500Work*)arg0->work;
        work2->field_A06 = 0xA;
        work2->field_A08 = 0;
    }
}

static void func_actor_400500_8013D8CC(Task* arg0)
{
    Actor400500Work* work;
    TmdObject*       model;
    GfxCoord*        coord;

    model            = arg0->extra.tmd;
    work             = (Actor400500Work*)arg0->work;
    coord            = model->coords;
    work->field_A02  = 0x1000;
    work->matrix_808 = coord->coord;
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
    work->field_A04 = 0;
    work->field_A06 = work->field_A06 + 1;
}

static void func_actor_400500_8013D958(Task* arg0)
{
    Actor400500Work* work;
    TmdObject*       model;
    u16              frame;

    work            = (Actor400500Work*)arg0->work;
    model           = arg0->extra.tmd;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame >= 0x18) {
        work->field_A20 = 0;
        work->field_A24 = 0x1000;
        work->field_A28 = 0xFF;
        func_8009EA50(work->field_A20);
        model->shading.colorBlend = work->field_A24;
        work->field_A04           = 0;
        work->field_A06           = work->field_A06 + 1;
    }
}

static void func_actor_400500_8013D9DC(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    arg0->state     = 3;
    work->field_A06 = 0;
    work->field_A08 = 0;
}

static void func_actor_400500_8013D9F4(Task* arg0)
{
    TmdObject*       model;
    Actor400500Work* work;

    model           = arg0->extra.tmd;
    work            = (Actor400500Work*)arg0->work;
    model->flags   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->field_A04 = 0;
    work->field_A28 = 0;
    work->field_A06 = work->field_A06 + 1;
}

static void func_actor_400500_8013DA24(Task* arg0)
{
    Actor400500Work* work;
    u16              frame;

    work            = (Actor400500Work*)arg0->work;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame >= 2) {
        work->field_A06 = work->field_A06 + 1;
    }
}

static void func_actor_400500_8013DA68(Task* arg0)
{
    TmdObject*       model;
    Actor400500Work* work;

    model = arg0->extra.tmd;
    work  = (Actor400500Work*)arg0->work;
    Tmd_FreeBuffers(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    func_actor_400500_80134B88(arg0);
    work->field_A06 = work->field_A06 + 1;
}

static void func_actor_400500_8013DACC(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    arg0->state     = 3;
    work->field_A06 = 0;
    work->field_A08 = 0;
}

void func_actor_400500_8013DAE4(Task* arg0, s32 arg1, u16* arg2)
{
    Actor400500Work* work;
    s32              kind;

    work = (Actor400500Work*)arg0->work;
    kind = arg2[1];
    switch (kind) {
        case 1:
            work->field_A4C = kind;
            work->field_A2C = 0x1E;
            work->field_A4B = kind;
            func_actor_400500_8013DB64(arg0, 0xB);
            break;
        case 2:
            work->field_A4B = kind;
            break;
        case 3:
            work->field_A4B = kind;
            break;
        case 4:
            work->field_A4B = kind;
            break;
    }
}

static void func_actor_400500_8013DB64(Task* arg0, s16 arg1)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    work->field_A06 = arg1;
    work->field_A08 = 0;
}

static s32 func_actor_400500_8013DB78(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    if ((work->field_A16 < (0x640 - (work->field_9D8.vz * 8))) && ((u32)(work->field_A36 - 0x300) >= 0xA01U)) {
        work->field_A06 = 9;
        work->field_A08 = 0;
        return 1;
    }
    return 0;
}

static void func_actor_400500_8013DBCC(Task* arg0, s16 arg1, Actor400500ViewPos* arg2)
{
    MATRIX    local;
    GfxCoord* coord;

    coord = &arg0->extra.tmd->coords[arg1];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &local);
    arg2->x             = local.t[0];
    arg2->z             = local.t[2];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_400500_8013DC4C(Task* arg0)
{
    Actor400500Work* work;
    s32              i;

    work = (Actor400500Work*)arg0->work;
    i    = 1;
    do {
        work->slots[i].rate = work->field_9F8;
        Gp_AnimResetSlot(&work->anim, i, work->field_9FE);
        i++;
    } while (i < 0x12);
    work->field_9FC = work->field_9FE;
}

static void func_actor_400500_8013DCBC(Task* arg0, s16 arg1, s16 arg2)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    work->field_9F8 = arg2;
    work->field_9FE = arg1;
    work->field_9FA = 2;
}

static void func_actor_400500_8013DCD4(Task* arg0)
{
    s32              same;
    Actor400500Work* work;
    s32              i;

    work = (Actor400500Work*)arg0->work;
    i    = 1;
    same = (s16)work->field_9FC == work->field_9FE;
    do {
        if (same) {
            do {
                work->slots[i].rate = work->field_9F8;
                i++;
            } while (i < 0x12);
        } else {
            do {
                work->slots[i].rate = work->field_9F8;
                func_800B4114(&work->anim, i, work->field_9FE, 0, work->field_A0E);
                i++;
            } while (i < 0x12);
            work->field_A0E = 0;
        }
    } while (0);
    work->field_9FC = work->field_9FE;
}

static s32 func_actor_400500_8013DD8C(Task* arg0, s16 arg1)
{
    Actor400500Work* work;
    s16              scale;

    work  = (Actor400500Work*)arg0->work;
    scale = work->field_9F8;
    if (scale == 0) {
        return 0;
    }
    return (((arg1 << 0x10) >> 8) / scale << 0xC) >> 0x10;
}

static s32 func_actor_400500_8013DDEC(Task* arg0)
{
    Actor400500HitView* work = (Actor400500HitView*)arg0->work;

    if ((work->flags_4C.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_4C.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation
/// alone. The actor builds a part's rotation in a scratch matrix and pushes
/// it into the part's `GfxCoord::coord` with this, so the part keeps its
/// position.
static void func_actor_400500_8013DE2C(MATRIX* src, MATRIX* dst)
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

/// Per-frame entry point of the actor's task: runs the handler its state
/// selects from `D_actor_400500_80131E4C` - set-up, the per-frame state
/// machine, the death sequence and the post-death timeout. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_actor_400500_8013DE98(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_400500_80131E4C;
    states.funcs[task->state](task);
}

static void func_actor_400500_8013DEFC(Task* arg0)
{
    Actor400500Work* work                = (Actor400500Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_400500_8013DF74,
        func_actor_400500_8013DFE4,
    };

    states[(s16)work->field_A06](arg0);
}

static void func_actor_400500_8013DF50(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    work->field_A3C = 0;
    work->field_A3E = 0;
}

void func_actor_400500_8013DF64(Task* task)
{
}

void func_actor_400500_8013DF6C(Task* task)
{
}

static void func_actor_400500_8013DF74(Task* arg0)
{
    Actor400500Work* work;
    Task*            child;
    s32              i;

    work = (Actor400500Work*)arg0->work;
    for (i = 0; i < 2; i++) {
        child = work->field_9F0[i];
        if (child != NULL) {
            taskKill(child);
        }
    }
    work->field_A04 = 0;
    work->field_A06 = work->field_A06 + 1;
}

static void func_actor_400500_8013DFE4(Task* arg0)
{
    Actor400500Work* work;
    u16              frame;

    work            = (Actor400500Work*)arg0->work;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame >= 0x12D) {
        Gp_DestroyEnemy(arg0->spawnArg2.pointer, arg0);
    }
}
