#include "common.h"

#include "psyq/abs.h"
#include "psyq/inline_c.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actors_shared_8013a0b0.h"
#include "actors/actors_shared_8016a538.h"
#include "actors/actors_shared_801692e8.h"

/// Three packed halfwords filled by `func_actor_400600_80139F4C`: the actor's
/// view-space X/Y (the translation of the `Gp_WorldToLocal` result) and the low
/// half of the root coordinate's world Z. `func_actor_400600_801361AC` passes
/// `Actor400600Work::field_88` as the destination, so the slot lives inside the
/// work block.
typedef struct Actor400600ViewPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
} Actor400600ViewPos;
STATIC_ASSERT_SIZEOF(Actor400600ViewPos, 0x6);

/// The word at `Actor400600Work::field_710`. Its low half is a frame countdown
/// (`func_actor_400600_80138AB8` ticks it down, `func_actor_400600_8013C074`
/// seeds it from the LCG); its high half is a flag halfword.
/// `func_actor_400600_8013892C` tests bit 0 of `flags` on its own and then the
/// whole word against `0x01020000`, so both views are modelled explicitly.
typedef union Actor400600Timer {
    /* 0x0 */ s32 word;
    struct {
        /* 0x0 */ s16 timer;
        /* 0x2 */ u16 flags;
    } h;
} Actor400600Timer;
STATIC_ASSERT_SIZEOF(Actor400600Timer, 0x4);

/// The word at `Actor400600Work::field_75C`. Its two high bytes are a state
/// pair written one at a time; `func_actor_400600_80136558` reads the whole word
/// and dispatches on its high half, so both views are modelled explicitly.
typedef union Actor400600State {
    /* 0x0 */ s32 word;
    struct {
        /* 0x0 */ s16 field_75C; // cleared on its own by func_actor_400600_80133FC0
        /* 0x2 */ u8  field_75E; // compared unsigned by func_actor_400600_8013C874
        /* 0x3 */ s8  field_75F;
    } b;
} Actor400600State;
STATIC_ASSERT_SIZEOF(Actor400600State, 0x4);

/// Payload `func_actor_400600_80133FC0` sends the `gameGetPtrSlot(3)` task as
/// message 0x3FF, with `field_0` pointing at `D_actor_400600_80151A48`.
typedef struct Actor400600Msg3FF {
    /* 0x00 */ void* field_0;
    /* 0x04 */ s32   field_4;
    /* 0x08 */ s32   field_8;
    /* 0x0C */ s32   field_C;
    /* 0x10 */ s32   field_10;
} Actor400600Msg3FF;
STATIC_ASSERT_SIZEOF(Actor400600Msg3FF, 0x14);

/// Reply buffer `func_actor_400600_80133FC0` passes with message 0x3F8; only
/// `field_14` is seeded (to 8) before the query.
typedef struct Actor400600Msg3F8 {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s32  field_14;
} Actor400600Msg3F8;
STATIC_ASSERT_SIZEOF(Actor400600Msg3F8, 0x18);

/// Per-actor state block for the `actor_400600` overlay.
///
/// `func_actor_400600_80133434` allocates it with `memCalloc(0x770)` and
/// stores it in the `Task::work` slot (0x1C): an enemy actor reuses that
/// pointer field for its own work block, so it is *not* a `TaskIdMap` here.
/// Reach it with `(Actor400600Work*)task->work`.
typedef struct Actor400600Work {
    /* 0x000 */ MATRIX             matrix_0;  // copy of the root coordinate's local matrix
    /* 0x020 */ MATRIX             matrix_20; // color matrix for the child models
    /* 0x040 */ MATRIX             matrix_40; // light matrix for the child models
    /* 0x060 */ byte               pad_60[0x10];
    /* 0x070 */ VECTOR             field_70;  // copy of the root coordinate's translation
    /* 0x080 */ u16                field_80;  // pitch, see func_actor_400600_80139948
    /* 0x082 */ u16                field_82;  // yaw, see func_actor_400600_80139948
    /* 0x084 */ u16                field_84;  // roll, see func_actor_400600_80139948
    /* 0x086 */ byte               pad_86[0x2];
    /* 0x088 */ Actor400600ViewPos field_88;
    /* 0x08E */ byte               pad_8E[0x2];
    /* 0x090 */ u16                field_90; // spawn position X (low half)
    /* 0x092 */ u16                field_92; // seeds field_73E on state entry
    /* 0x094 */ u16                field_94; // spawn position Z (low half)
    /* 0x096 */ byte               pad_96[0x2];
    /* 0x098 */ u16                field_98; // low half of the root coordinate's world X
    /* 0x09A */ u16                field_9A; // copy of field_92
    /* 0x09C */ u16                field_9C; // low half of the root coordinate's world Z
    /* 0x09E */ byte               pad_9E[0xA];
    /* 0x0A8 */ Actor400600ViewPos field_A8; // copied to the stack for func_actor_400600_80139C00
    /* 0x0AE */ byte               pad_AE[0x2];
    /* 0x0B0 */ GpAnimCtx          anim;     // slots 1..0x11 reset by func_actor_400600_80139A78
    /* 0x0C4 */ GpAnimSlot         slots[0x12];
    /* 0x394 */ byte               pad_394[0x120];
    /* 0x4B4 */ GpObj              obj_4B4;    // collision node; flags bit 0x8000 cleared
    /* 0x4D4 */ GpRec18            rec_4D4[8]; // occupancy cleared by func_actor_400600_80138D78
    /* 0x594 */ GpObj              obj_594;    // collision node; flags bit 0x8000 cleared
    /* 0x5B4 */ GpRec18            rec_5B4[1]; // obj_594's table (flags kind 1)
    /* 0x5CC */ GpObj              obj_5CC;    // collision node; flags bit 0x8000 cleared
    /* 0x5EC */ GpRec18            rec_5EC[1]; // obj_5CC's table (flags kind 1)
    /* 0x604 */ GpObj              obj_604;    // collision node; flags bit 0x4000 cleared
    /* 0x624 */ GpActorD4Rec       rec_624;    // obj_604's payload (flags kind 3)
    /* 0x63C */ GpRec18            rec_63C[8]; // occupancy cleared by func_actor_400600_80138D78
    /* 0x6FC */ GpEffArg           eff_6FC;    // fourth model part's coordinate
    /* 0x704 */ Task*              field_704;  // child task, killed on death
    /* 0x708 */ Task*              field_708;  // child task, killed on death
    /* 0x70C */ byte               pad_70C[0x4];
    /* 0x710 */ Actor400600Timer   field_710;
    /* 0x714 */ s16                field_714; // reset to 0x1000 on death
    /* 0x716 */ u16                field_716; // frame counter, bumped by func_actor_400600_80138D78
    /* 0x718 */ u16                field_718; // per-state frame counter
    /* 0x71A */ s16                field_71A;
    /* 0x71C */ u16                field_71C; // state index
    /* 0x71E */ u16                field_71E; // sub-state index
    /* 0x720 */ s16                field_720;
    /* 0x722 */ s16                field_722; // velocity step (can go negative)
    /* 0x724 */ s16                field_724; // accumulated step
    /* 0x726 */ s16                field_726;
    /* 0x728 */ s16                field_728;
    /* 0x72A */ u16                field_72A;
    /* 0x72C */ u16                field_72C;
    /* 0x72E */ s16                field_72E;
    /* 0x730 */ s16                field_730;
    /* 0x732 */ s16                field_732; // countdown seeded by func_actor_400600_80138AF0
    /* 0x734 */ s16                field_734; // model slot id handed to func_actor_400600_80139FE0
    /* 0x736 */ byte               pad_736[0x4];
    /* 0x73A */ s16                field_73A; // fade level, lerped toward 0xFF
    /* 0x73C */ s16                field_73C;
    /* 0x73E */ u16                field_73E;
    /* 0x740 */ s16                field_740;
    /* 0x742 */ s16                field_742; // animation request kind
    /* 0x744 */ s16                field_744; // animation id now playing
    /* 0x746 */ s16                field_746; // animation id
    /* 0x748 */ s16                field_748; // sound step index (func_actor_400600_801361AC)
    /* 0x74A */ s16                field_74A; // hit cooldown, seeded from Gp_GetIdParam2
    /* 0x74C */ s16                field_74C;
    /* 0x74E */ s16                field_74E;
    /* 0x750 */ u16                field_750; // countdown to state 0xB
    /* 0x752 */ s16                field_752;
    /* 0x754 */ s16                field_754;
    /* 0x756 */ u16                field_756; // countdown to the next state-2 transition
    /* 0x758 */ s16                field_758;
    /* 0x75A */ s16                field_75A;
    /* 0x75C */ Actor400600State   field_75C;
    /* 0x760 */ s8                 field_760;
    /* 0x761 */ byte               pad_761;
    /* 0x762 */ u8                 field_762;
    /* 0x763 */ u8                 field_763;
    /* 0x764 */ u8                 field_764;
    /* 0x765 */ s8                 field_765;
    /* 0x766 */ s8                 field_766;
    /* 0x767 */ s8                 field_767;
    /* 0x768 */ u8                 field_768;
    /* 0x769 */ u8                 field_769; // sub-variant flag, gates state indices
    /* 0x76A */ u8                 field_76A; // distance mode: 0 none, 1 XZ, 2 XY
    /* 0x76B */ u8                 field_76B;
    /* 0x76C */ u8                 field_76C; // nonzero: landing spawns the dust ring
    /* 0x76D */ u8                 field_76D;
    /* 0x76E */ u8                 field_76E; // set when spawned in map 0x0314
    /* 0x76F */ byte               pad_76F;
} Actor400600Work;
STATIC_ASSERT_SIZEOF(Actor400600Work, 0x770);

/// 0x3C-byte scratchpad frame `func_actor_400600_801383E4` carves off
/// `G_SCRATCH_HEAD`: the four widened corners of the quad and
/// `RotTransPers4`'s outputs. Same tail as `ActorsShared80163354Scratch`.
typedef struct Actor400600QuadScratch {
    /* 0x00 */ SVECTOR corner0;
    /* 0x08 */ SVECTOR corner1;
    /* 0x10 */ SVECTOR corner2;
    /* 0x18 */ SVECTOR corner3;
    /* 0x20 */ s32     screen0;
    /* 0x24 */ s32     screen1;
    /* 0x28 */ s32     screen2;
    /* 0x2C */ s32     screen3;
    /* 0x30 */ s32     perspective;
    /* 0x34 */ s32     flags;
    /* 0x38 */ s32     depth;
} Actor400600QuadScratch;
STATIC_ASSERT_SIZEOF(Actor400600QuadScratch, 0x3C);

/// 0x8C-byte scratchpad frame `func_actor_400600_80132294` carves off
/// `G_SCRATCH_HEAD` to draw a textured quad between two model parts. Same
/// layout as `ActorsShared80163354Scratch` without the trailing half offsets,
/// which this variant keeps in registers.
typedef struct Actor400600BeamScratch {
    /* 0x00 */ MATRIX  firstMatrix;  // first part's `workm` in view space
    /* 0x20 */ MATRIX  secondMatrix; // second part's `workm` in view space
    /* 0x40 */ SVECTOR first;
    /* 0x48 */ SVECTOR second;
    /* 0x50 */ SVECTOR corner0;
    /* 0x58 */ SVECTOR corner1;
    /* 0x60 */ SVECTOR corner2;
    /* 0x68 */ SVECTOR corner3;
    /* 0x70 */ s32     screen0;
    /* 0x74 */ s32     screen1;
    /* 0x78 */ s32     screen2;
    /* 0x7C */ s32     screen3;
    /* 0x80 */ s32     perspective;
    /* 0x84 */ s32     flags;
    /* 0x88 */ s32     depth;
} Actor400600BeamScratch;
STATIC_ASSERT_SIZEOF(Actor400600BeamScratch, 0x8C);

/// One entry of `D_actor_400600_80151B40`, a world-space XZ rectangle table
/// ended by an entry whose `id` is -1. `func_actor_400600_8013886C` returns the
/// `id` of the first rectangle containing the actor (edges inclusive).
typedef struct Actor400600Zone {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
    /* 0x4 */ s16 w;
    /* 0x6 */ s16 h;
    /* 0x8 */ s16 id;
} Actor400600Zone;
STATIC_ASSERT_SIZEOF(Actor400600Zone, 0xA);

extern Actor400600Zone D_actor_400600_80151B40[];

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`; `D_80115414` and `D_80115417` are
 * bytes of the run of gameplay flags at 0x80115408..0x8011541B.
 *
 * Storing to a bare `extern` global next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `Actor400600Work` loads
 * that follow. Two remedies work and which one is needed was measured, not
 * chosen: the byte store to `D_80115417` matches with `SOFT_BARRIER()` after
 * it, so that one is declared as the scalar it is; the pointer store to
 * `D_800678F0` checksums wrong with the barrier and matches only as an
 * aggregate, so its one-element array stays and is doing real work.
 * `D_80115414` is the aggregate case too: one of its stores sits between
 * struct stores on both sides, and the barrier trades the sink for a hoist
 * above the preceding flag updates. */
extern void* D_800678F0[1];
extern s8    D_80115414[1];
extern s8    D_80115417;

extern u8 D_801153F4;

extern s32 D_80115738;
extern s32 D_8011574C;

extern GpU16Pair  D_actor_400600_80144EA8;
extern GpPairSrcE D_actor_400600_80144EB0; // the enemy's parameter record

extern u8 D_actor_400600_80151A54[];       // animation bank handed to func_800B3F84
extern u8 D_actor_400600_80151AE0[];

extern u8 D_actor_400600_80151A48[];

extern TaskDesc D_actor_400600_80151AF8;

extern u8 D_actor_400600_8014220C[];
extern u8 D_actor_400600_80143604[];
extern u8 D_actor_400600_80143B24[];
extern u8 D_actor_400600_80144994[];

extern u8 D_actor_400600_80151B1C[];

/* Part indices into the model's coordinate array, terminated by -1. */
extern s16 D_actor_400600_80151B88[];

void func_8004BFF8(s32 angle, MATRIX* matrix);
void func_8017D9B8(s32);
void Gp_SpawnPadLerp(s16 arg0, u8 arg1, u8 arg2);

void func_actor_400600_8013203C(Task* arg0);
void func_actor_400600_80132294(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade);
void func_actor_400600_80132704(Task* arg0, s16 arg1, u8 arg2);
void func_actor_400600_801328A8(Task* arg0);
void func_actor_400600_801329EC(Task* arg0);
void func_actor_400600_80132B3C(Task* arg0);
void func_actor_400600_80132C70(Task* arg0);
void func_actor_400600_80132E10(Task* arg0);
void func_actor_400600_80132F3C(Task* arg0);
void func_actor_400600_80133118(Task* arg0);
void func_actor_400600_801332F4(Task* arg0);
void func_actor_400600_80133434(Task* arg0);
void func_actor_400600_801337A8(Task* arg0);
void func_actor_400600_80133B88(Task* arg0);
void func_actor_400600_80133CB0(Task* arg0);
void func_actor_400600_80133E38(Task* arg0);
void func_actor_400600_80133FC0(Task* arg0);
void func_actor_400600_80134218(Task* arg0);
void func_actor_400600_80134570(Task* arg0);
void func_actor_400600_8013479C(Task* arg0);
void func_actor_400600_80134970(Task* arg0);
void func_actor_400600_80134B98(Task* arg0);
void func_actor_400600_80134E28(Task* arg0);
void func_actor_400600_801350F4(Task* arg0);
void func_actor_400600_80135450(Task* arg0);
void func_actor_400600_80135578(Task* arg0);
void func_actor_400600_801356E0(Task* arg0);
void func_actor_400600_80135998(Task* arg0, s16 arg1);
void func_actor_400600_80135DDC(Task* arg0);
void func_actor_400600_801361AC(Task* arg0);
void func_actor_400600_80136558(Task* arg0);
void func_actor_400600_80136670(Task* arg0);
void func_actor_400600_80136968(Task* arg0);
s32  func_actor_400600_80136FA8(Task* arg0);
s32  func_actor_400600_801370F4(Task* arg0);
void func_actor_400600_80137240(Task* arg0);
void func_actor_400600_80137498(Task* arg0, s16 arg1);
s32  func_actor_400600_801376EC(Task* arg0);
void func_actor_400600_80137840(Task* arg0);
s32  func_actor_400600_80137AF0(Task* arg0);
s32  func_actor_400600_80137C34(Task* arg0);
void func_actor_400600_80137EF0(Task* arg0);
void func_actor_400600_80138224(Task* arg0, s16 arg1, u8 arg2);
void func_actor_400600_801383E4(SVECTOR* arg0, SVECTOR* arg1, s16 width, u8 shade);
void func_actor_400600_801387DC(Task* arg0, s32 arg1);
s32  func_actor_400600_8013886C(Task* arg0);
s32  func_actor_400600_8013892C(Task* arg0);
void func_actor_400600_8013896C(Task* arg0, s16 arg1);
void func_actor_400600_80138A24(Task* arg0, s16 arg1);
void func_actor_400600_80138AA4(Task* arg0);
void func_actor_400600_80138AB8(Task* arg0);
void func_actor_400600_80138AF0(Task* arg0, s32 arg1);
void func_actor_400600_80138B40(Task* arg0);
void func_actor_400600_80138B5C(Task* arg0, s32 arg1);
void func_actor_400600_80138C34(Task* arg0);
void func_actor_400600_80138D78(Task* arg0);
void func_actor_400600_80138EA0(Task* arg0);
void func_actor_400600_80138FD4(Task* arg0);
void func_actor_400600_801390FC(Task* arg0);
void func_actor_400600_80139110(Task* arg0);
void func_actor_400600_80139218(Task* arg0);
void func_actor_400600_80139280(Task* arg0);
void func_actor_400600_801392E8(Task* arg0);
void func_actor_400600_8013935C(Task* arg0);
void func_actor_400600_801393D0(Task* arg0);
void func_actor_400600_80139444(Task* arg0);
void func_actor_400600_801394E0(Task* arg0);
void func_actor_400600_80139560(Task* arg0);
void func_actor_400600_80139608(Task* arg0);
void func_actor_400600_80139670(Task* arg0);
void func_actor_400600_801396E4(Task* arg0);
void func_actor_400600_80139764(Task* arg0);
void func_actor_400600_801397E4(Task* arg0);
void func_actor_400600_80139878(Task* arg0);
void func_actor_400600_801398E0(Task* arg0);
void func_actor_400600_80139948(Task* arg0);
void func_actor_400600_80139A78(Task* arg0);
void func_actor_400600_80139AE8(Task* arg0);
s16  func_actor_400600_80139BA0(Task* arg0, s16 arg1);
void func_actor_400600_80139C00(Task* arg0, SVECTOR* target, s32 step);
void func_actor_400600_80139CAC(Task* arg0);
void func_actor_400600_80139D98(Task* arg0, s16 arg1, s16 arg2);
void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_actor_400600_80139DCC(Task* task, s16 index, Actor400600ViewPos* out);
void func_actor_400600_80139E68(Task* arg0, s16 arg1, Actor400600ViewPos* arg2);
void func_actor_400600_80139F4C(Task* arg0, s16 arg1, Actor400600ViewPos* arg2);
void func_actor_400600_80139FE0(Task* arg0, s16 arg1, Actor400600ViewPos* arg2);
s32  func_actor_400600_8013A0B0(Task* arg0);
void func_actor_400600_8013A0F0(Task* arg0);
void func_actor_400600_8013A170(Task* arg0);
void func_actor_400600_8013A26C(Task* arg0);
void func_actor_400600_8013A2C0(Task* task);
void func_actor_400600_8013A338(Task* arg0, s32 arg1, u16* arg2);
void func_actor_400600_8013A3A8(Task* arg0);
void func_actor_400600_8013A3B8(void);
void func_actor_400600_8013A3C0(void);
void func_actor_400600_8013A3C8(Task* arg0);
void func_actor_400600_8013A4AC(Task* arg0);
void func_actor_400600_8013A518(Task* arg0);
void func_actor_400600_8013A570(Task* arg0);
void func_actor_400600_8013A638(Task* arg0);
void func_actor_400600_8013A6C4(Task* arg0);
void func_actor_400600_8013A808(Task* arg0);
void func_actor_400600_8013A820(Task* arg0);
void func_actor_400600_8013A864(Task* arg0);
void func_actor_400600_8013A908(Task* arg0);
void func_actor_400600_8013A990(Task* arg0);
void func_actor_400600_8013AA5C(Task* arg0);
void func_actor_400600_8013AAD8(Task* arg0);
void func_actor_400600_8013AB44(Task* arg0);
void func_actor_400600_8013AB98(Task* arg0);
void func_actor_400600_8013AC14(Task* arg0);
void func_actor_400600_8013AD3C(Task* arg0);
void func_actor_400600_8013ADA4(Task* arg0);
void func_actor_400600_8013AE88(Task* arg0);
void func_actor_400600_8013AF04(Task* arg0);
void func_actor_400600_8013B018(Task* arg0);
void func_actor_400600_8013B0FC(Task* arg0);
void func_actor_400600_8013B150(Task* arg0);
void func_actor_400600_8013B1DC(Task* arg0);
void func_actor_400600_8013B2A8(Task* arg0);
void func_actor_400600_8013B394(Task* arg0);
void func_actor_400600_8013B410(Task* arg0);
void func_actor_400600_8013B520(Task* arg0);
void func_actor_400600_8013B640(void);
void func_actor_400600_8013B6F4(Task* arg0);
void func_actor_400600_8013B740(Task* arg0);
void func_actor_400600_8013B830(Task* arg0);
void func_actor_400600_8013B8AC(Task* arg0);
void func_actor_400600_8013B984(Task* arg0);
void func_actor_400600_8013BA00(Task* arg0);
void func_actor_400600_8013BA6C(Task* arg0);
void func_actor_400600_8013BAEC(Task* arg0);
void func_actor_400600_8013BB88(Task* arg0);
void func_actor_400600_8013BBF4(Task* arg0);
void func_actor_400600_8013BC68(Task* arg0);
void func_actor_400600_8013BCD8(Task* arg0);
void func_actor_400600_8013BD54(Task* arg0);
void func_actor_400600_8013BDF0(Task* arg0);
void func_actor_400600_8013BE58(Task* arg0);
void func_actor_400600_8013BE90(Task* arg0);
void func_actor_400600_8013BF48(Task* arg0);
void func_actor_400600_8013BF80(Task* arg0);
void func_actor_400600_8013BFD4(Task* arg0);
void func_actor_400600_8013C038(Task* arg0);
void func_actor_400600_8013C074(Task* arg0);
void func_actor_400600_8013C104(Task* arg0);
void func_actor_400600_8013C124(Task* arg0);
void func_actor_400600_8013C1C0(Task* arg0);
void func_actor_400600_8013C238(Task* arg0);
void func_actor_400600_8013C2D4(Task* arg0);
void func_actor_400600_8013C394(Task* arg0);
void func_actor_400600_8013C410(Task* arg0);
void func_actor_400600_8013C4AC(Task* arg0);
void func_actor_400600_8013C518(Task* arg0);
void func_actor_400600_8013C534(Task* arg0);
void func_actor_400600_8013C598(Task* arg0);
void func_actor_400600_8013C5F8(Task* arg0);
void func_actor_400600_8013C6B0(SVECTOR* pos, GpRec18* rec, SVECTOR* out);
s32  func_actor_400600_8013C7E8(s16 arg0, s16 arg1);
void func_actor_400600_8013C874(Task* arg0);
void func_actor_400600_8013C940(Task* arg0);
void func_actor_400600_8013C9DC(Task* arg0);
s32  func_actor_400600_8013CACC(Task* arg0);
void func_actor_400600_8013CB40(Task* arg0, u8 arg1);
void func_actor_400600_8013CB70(Task* arg0, s32 arg1);
void func_actor_400600_8013CC04(Task* arg0, s16 arg1);

/// `func_actor_400600_80139948`'s body, inlined: wrap the three angles to 12 bits and
/// rebuild the model root's rotation from them. Inlining is what keeps each
/// `G_SCRATCH_HEAD` access in the absolute `lui`/`lw` form instead of a
/// register CSE would otherwise hoist the address into.
static __inline__ void Actor400600_RebuildRotation(Task* arg0)
{
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->coords;
    MATRIX*          m;
    MATRIX*          dst;

    work->field_80           &= 0xFFF;
    work->field_82           &= 0xFFF;
    work->field_84           &= 0xFFF;
    m                         = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    RotMatrixZ((s16)work->field_84, m);
    RotMatrixX((s16)work->field_80, m);
    func_8004BFF8((s16)work->field_82, m);
    dst                   = &coord->coord;
    dst->m[0][0]          = m->m[0][0];
    dst->m[0][1]          = m->m[0][1];
    dst->m[0][2]          = m->m[0][2];
    dst->m[1][0]          = m->m[1][0];
    dst->m[1][1]          = m->m[1][1];
    dst->m[1][2]          = m->m[1][2];
    dst->m[2][0]          = m->m[2][0];
    dst->m[2][1]          = m->m[2][1];
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
    dst->m[2][2]          = m->m[2][2];
}

/// `func_actor_400600_80139CAC`'s body, inlined: advance the pending animation
/// request, then tick every model slot at the current rate.
static __inline__ void Actor400600_TickAnim(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    s32              i;

    if (work->field_742 == 1) {
        if (work->field_744 != work->field_746) {
            work->field_748 = 0;
        } else {
            work->field_748 = func_actor_400600_80139BA0(arg0, work->field_748);
        }
        func_actor_400600_80139AE8(arg0);
        work->field_742 = 3;
    } else if (work->field_742 == 2) {
        func_actor_400600_80139A78(arg0);
        work->field_742 = 3;
        work->field_748 = 0;
    } else if (work->field_742 == 3) {
        work->field_748++;
    }
    i = 1;
    do {
        work->slots[i].rate = work->field_726;
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x12);
}

/// `func_actor_400600_8013A2C0`'s body, inlined: push the model's second coordinate's
/// world position onto `G_SCRATCH_HEAD` and hand it to `Gp_UpdateActorColor`.
static __inline__ void Actor400600_UpdateColor(Task* arg0)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)arg0->extra)->coords[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(arg0->spawnArg2, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

void func_actor_400600_8013203C(Task* arg0)
{
    Actor400600Work* work;

    work                   = (Actor400600Work*)arg0->work;
    work->obj_4B4.coord    = &((TmdObject*)arg0->extra)->coords[3];
    work->obj_4B4.ctx.recs = work->rec_4D4;
    work->obj_4B4.pos.vx   = 0;
    work->obj_4B4.pos.vy   = 0x96;
    work->obj_4B4.pos.vz   = 0x110;
    work->obj_4B4.key      = 0x30006;
    if (gGameSession->at4.loc.stage == 3 && (gGameSession->at4.loc.area == 0x1F || gGameSession->at4.loc.area == 0x1D)) {
        work->obj_4B4.radius = 0x260;
    } else {
        work->obj_4B4.radius = 0x200;
    }
    work->obj_4B4.flags = 1;
    Gp_LinkObj(2, &work->obj_4B4);
    Gp_InitRec18Table(work->rec_4D4, 8, 0);
    work->rec_624.end0.vz    = 0xBB8;
    work->rec_624.end0Radius = 0xA;
    work->rec_624.end1Radius = 0xA;
    work->rec_624.end0.vx    = 0;
    work->rec_624.end1.vz    = 0;
    work->rec_624.end1.vx    = 0;
    work->rec_624.recs       = work->rec_63C;
    work->obj_4B4.flags     |= 0x8000;
    work->obj_604.coord      = ((TmdObject*)arg0->extra)->coords;
    work->obj_604.ctx.d4rec  = &work->rec_624;
    work->obj_604.pos.vx     = 0;
    work->obj_604.pos.vy     = -0x190;
    work->obj_604.pos.vz     = 0;
    work->obj_604.key        = 0x30006;
    work->obj_604.radius     = 0;
    work->obj_604.flags      = 3;
    Gp_LinkObj(2, &work->obj_604);
    Gp_InitRec18Table(work->rec_63C, 8, 0);
    work->obj_604.flags   &= 0x3FFF;
    work->obj_594.key      = Gp_PackPair(&D_actor_400600_80144EA8, 0);
    work->obj_594.coord    = &((TmdObject*)arg0->extra)->coords[7];
    work->obj_594.ctx.recs = work->rec_5B4;
    work->obj_594.pos.vx   = -0x200;
    work->obj_594.pos.vy   = 0;
    work->obj_594.pos.vz   = 0;
    work->obj_594.radius   = 0x190;
    work->obj_594.flags    = 1;
    Gp_LinkObj(3, &work->obj_594);
    Gp_InitRec18Table(work->rec_5B4, 1, 0);
    work->obj_594.flags   &= 0x7FFF;
    work->obj_5CC.key      = Gp_PackPair(&D_actor_400600_80144EA8, 0);
    work->obj_5CC.coord    = &((TmdObject*)arg0->extra)->coords[10];
    work->obj_5CC.ctx.recs = work->rec_5EC;
    work->obj_5CC.pos.vx   = 0x200;
    work->obj_5CC.pos.vy   = 0;
    work->obj_5CC.pos.vz   = 0;
    work->obj_5CC.radius   = 0x190;
    work->obj_5CC.flags    = 1;
    Gp_LinkObj(3, &work->obj_5CC);
    Gp_InitRec18Table(work->rec_5EC, 1, 0);
    work->obj_5CC.flags &= 0x7FFF;
}

void func_actor_400600_80132294(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade)
{
    Actor400600BeamScratch* s;
    s16                     angle;
    GsCOORDINATE2*          secondCoord;
    GsCOORDINATE2*          firstCoord;
    s32                     offset0;
    s32                     offset1;
    s32                     offset2;
    s32                     offset3;
    s32                     halfX;
    s32                     halfY;
    GsCOORDINATE2*          coords;
    POLY_FT4*               poly;

    coords      = ((TmdObject*)task->extra)->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (Actor400600BeamScratch*)(*(u8**)G_SCRATCH_HEAD -= sizeof(Actor400600BeamScratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->first.vx       = s->firstMatrix.t[0];
        s->first.vy       = s->firstMatrix.t[1];
        s->second.vx      = s->secondMatrix.t[0];
        s->second.vy      = s->secondMatrix.t[1];
        s->first.vz       = height;
        s->second.vz      = height;
        angle             = ratan2(s->second.vx - s->first.vx, s->second.vy - s->first.vy);
        halfX             = (s->first.vx - s->second.vx) / 2;
        halfY             = (s->first.vy - s->second.vy) / 2;
        s->corner0.vx     = halfX + (s->first.vx - ((s32)(rcos(angle) * width) >> 0xC));
        offset0           = rsin(angle) * width;
        s->corner0.vz     = height;
        s->corner0.vy     = halfY + (s->first.vy + (offset0 >> 0xC));
        s->corner1.vx     = halfX + (s->first.vx + ((s32)(rcos(angle) * width) >> 0xC));
        offset1           = rsin(angle) * width;
        s->corner1.vz     = height;
        s->corner1.vy     = halfY + (s->first.vy - (offset1 >> 0xC));
        s->corner2.vx     = (s->second.vx - ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
        offset2           = rsin(angle) * width;
        s->corner2.vz     = height;
        s->corner2.vy     = (s->second.vy + (offset2 >> 0xC)) - halfY;
        s->corner3.vx     = (s->second.vx + ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
        offset3           = rsin(angle) * width;
        s->corner3.vz     = height;
        s->corner3.vy     = (s->second.vy - (offset3 >> 0xC)) - halfY;
        gGfxViewCoord.flg = 0;
        Gp_UpdateCoord(&gGfxViewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = (u8*)poly + 0x28;
            setlen(poly, 9);
            poly->code       = 0x2E;
            *(s32*)&poly->x0 = s->screen0;
            *(s32*)&poly->x1 = s->screen1;
            *(s32*)&poly->x2 = s->screen2;
            *(s32*)&poly->x3 = s->screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
        }
        *(u8**)G_SCRATCH_HEAD += sizeof(Actor400600BeamScratch);
    }
}

void func_actor_400600_80132704(Task* arg0, s16 arg1, u8 arg2)
{
    func_actor_400600_80132294(arg0, 3, 9, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 9, 0xA, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xA, 0xB, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 3, 6, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 6, 7, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 7, 8, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 5, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 0xC, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xC, 0xD, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xD, 0xE, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 0xF, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xF, 0x10, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0x10, 0x11, 0x80, arg1, arg2);
}

void func_actor_400600_801328A8(Task* arg0)
{
    GsCOORDINATE2*   coords;
    Actor400600Work* work;
    s32              sound;
    s32              pan;

    coords              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    work                = (Actor400600Work*)arg0->work;
    coords->coord.t[0] += (0x4364 - coords->coord.t[0]) >> 2;
    coords->coord.t[2] += (0x760 - coords->coord.t[2]) >> 2;
    work->field_722    += 2;
    work->field_724    += work->field_722;
    coords->coord.t[1] += work->field_724;
    if (coords->coord.t[1] >= -0x508) {
        Gp_SpawnPadLerp(0xA, 0xC0, 0x80);
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x531A0009;
        pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        func_8017D9B8(1);
        func_actor_400600_80139D98(arg0, 0x19, 0x30);
        coords->coord.t[1] = -0x508;
        work->field_71C++;
    }
}

void func_actor_400600_801329EC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    u32              sound;
    s32              pan;

    work = (Actor400600Work*)arg0->work;
    if ((s16)work->field_718 == 0) {
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x531A000A;
        pan     = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    work->field_718++;
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40060004;
        pan     = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
        work3                = (Actor400600Work*)arg0->work;
        work3->field_71C     = 2;
        work3->field_71E     = 0;
    }
}

void func_actor_400600_80132B3C(Task* arg0)
{
    GsCOORDINATE2*   coords;
    Actor400600Work* work;
    s32              sound;
    s32              pan;

    coords              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    work                = (Actor400600Work*)arg0->work;
    coords->coord.t[0] += (0x1C54 - coords->coord.t[0]) >> 2;
    coords->coord.t[2] += (0xED5 - coords->coord.t[2]) >> 2;
    work->field_722    += 2;
    work->field_724    += work->field_722;
    coords->coord.t[1] += work->field_724;
    if (coords->coord.t[1] >= 0) {
        Gp_SpawnPadLerp(0x10, 0x80, 0x40);
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x531A000A;
        pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        func_actor_400600_80139D98(arg0, 0x19, 0x10);
        coords->coord.t[1] = 0;
        work->field_71C++;
    }
}

void func_actor_400600_80132C70(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    Actor400600Work* work4;
    GsCOORDINATE2*   coords;
    u8               mode;
    s16              yaw;

    work   = (Actor400600Work*)arg0->work;
    mode   = work->field_762;
    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (mode == 1) {
        coords->coord.t[0] = 0x36B0;
        coords->coord.t[1] = -0x320;
        coords->coord.t[2] = -0x7D0;
        work->field_80     = 0;
        work->field_82     = 0x400;
        work->field_84     = 0x400;
        work->field_718    = 0;
        func_actor_400600_80139D98(arg0, 2, 0x10);
        work->field_73C = -0x7D0;
        work->field_71C++;
    } else if (mode == 2) {
        coords->coord.t[0] = 0x4A38;
        coords->coord.t[1] = -0x320;
        coords->coord.t[2] = -0xFA0;
        yaw                = -0x400;
        work->field_82     = yaw;
        work->field_80     = 0;
        work->field_84     = 0x400;
        work->field_718    = 0;
        func_actor_400600_80139D98(arg0, 2, 0x10);
        work->field_73C  = -0xFA0;
        work2            = (Actor400600Work*)arg0->work;
        work2->field_71C = 5;
        work2->field_71E = 0;
    } else if (mode == 3) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        coords->coord.t[0]   = 0x2AF8;
        coords->coord.t[2]   = -0x3E8;
        coords->coord.t[1]   = 0;
        work->field_80       = 0;
        work->field_82       = 0xC00;
        work3                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work3->field_71C     = 0;
        work3->field_71E     = 0;
    } else if (mode == 4) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        coords->coord.t[0]   = 0x3A98;
        coords->coord.t[2]   = -0xBB8;
        coords->coord.t[1]   = 0;
        work->field_80       = 0;
        work->field_82       = 0x400;
        work4                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work4->field_71C     = 0;
        work4->field_71E     = 0;
    }
}

void func_actor_400600_80132E10(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    u32              sound;
    s32              pan;

    work->field_718 = work->field_718 + 1;
    func_actor_400600_801361AC(arg0);
    if ((s16)work->field_718 == 0x26) {
        func_actor_400600_80138B5C(arg0, 0);
    }
    if ((s16)work->field_718 >= 0x27) {
        work->field_73A = (u16)work->field_73A + ((0xFF - work->field_73A) >> 4);
    }
    if ((s16)work->field_718 == 0x5A) {
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x404A0004;
        pan     = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->field_718 = 0;
        work->field_722 = -0xA;
        work->field_724 = 0;
        func_actor_400600_80139DB0(arg0, 0x15, 0x10, 4);
        work->field_71C = work->field_71C + 1;
    }
}

void func_actor_400600_80132F3C(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coords;
    s32              sound;
    s32              pan;

    work   = (Actor400600Work*)arg0->work;
    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    work->field_718++;
    if ((s16)work->field_718 >= 0x11) {
        work->field_73A    += -work->field_73A >> 3;
        work->field_722    += 2;
        work->field_724    += work->field_722;
        coords->coord.t[1] += work->field_724;
        coords->coord.t[2] += (-0xBB8 - coords->coord.t[2]) >> 3;
        coords->coord.t[0] += (0x4588 - coords->coord.t[0]) >> 2;
        work->field_82     += (0xC00 - (s16)work->field_82) >> 2;
        work->field_84     += -(s16)work->field_84 >> 3;
        if ((s16)work->field_718 == 0x22) {
            func_actor_400600_8013896C(arg0, 0);
        }
        if ((s16)work->field_718 == 0x23 || (s16)work->field_718 == 0x25) {
            Gp_SpawnEff(D_8011574C, coords, 0x38, NULL);
        }
        if (coords->coord.t[1] >= 0) {
            sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x404A0003;
            pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
            func_actor_400600_80139D98(arg0, 0x19, 0x10);
            coords->coord.t[1] = 0;
            work->field_71C++;
        }
    }
}

void func_actor_400600_80133118(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coords;
    s32              sound;
    s32              pan;

    work   = (Actor400600Work*)arg0->work;
    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    work->field_718++;
    if ((s16)work->field_718 >= 0x11) {
        work->field_73A    += -work->field_73A >> 3;
        work->field_722    += 2;
        work->field_724    += work->field_722;
        coords->coord.t[1] += work->field_724;
        coords->coord.t[2] += (-0xBB8 - coords->coord.t[2]) >> 3;
        coords->coord.t[0] += (0x3A98 - coords->coord.t[0]) >> 2;
        work->field_82     += (0x400 - (s16)work->field_82) >> 2;
        work->field_84     += -(s16)work->field_84 >> 3;
        if ((s16)work->field_718 == 0x22) {
            func_actor_400600_8013896C(arg0, 0);
        }
        if ((s16)work->field_718 == 0x23 || (s16)work->field_718 == 0x25) {
            Gp_SpawnEff(D_8011574C, coords, 0x38, NULL);
        }
        if (coords->coord.t[1] >= 0) {
            sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x404A0003;
            pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
            func_actor_400600_80139D98(arg0, 0x19, 0x10);
            coords->coord.t[1] = 0;
            work->field_71C++;
        }
    }
}

void func_actor_400600_801332F4(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    GsCOORDINATE2*   coord;
    s32              mode;

    work  = (Actor400600Work*)arg0->work;
    model = (TmdObject*)arg0->extra;
    mode  = work->field_762;
    coord = model->coords;
    if (mode == 1) {
        Gp_SpawnPadLerp(0x14, 0xFF, 0x80);
        coord->coord.t[0] = 0xCE4;
        coord->coord.t[1] = -0xBB8;
        coord->coord.t[2] = 0;
        work->field_80    = 0;
        work->field_82    = 0xC00;
        work->field_84    = 0;
        model->flags     &= 0xFF7F;
        work->field_718   = 0;
        work->field_722   = 0;
        work->field_724   = 0;
        func_actor_400600_80139D98(arg0, 0x15, 0x10);
        func_actor_400600_80138B5C(arg0, 0);
        work->field_71C = work->field_71C + 1;
    } else if (mode == 2) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        coord->coord.t[0]    = -0x6A4;
        coord->coord.t[2]    = -0x514;
        coord->coord.t[1]    = 0;
        work->field_82       = 0x400;
        work->field_73A      = 0xFF;
        work->field_80       = 0;
        work->field_84       = 0;
        D_80115414[0]        = mode;
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
    }
}

const TaskFuncTable12 D_actor_400600_80131E24 = { {
    func_actor_400600_8013A3C8,
    func_actor_400600_8013A4AC,
    func_actor_400600_8013A518,
    func_actor_400600_8013A570,
    func_actor_400600_8013A638,
    func_actor_400600_8013A6C4,
    func_actor_400600_8013A808,
    func_actor_400600_8013A820,
    func_actor_400600_8013A864,
    func_actor_400600_8013A908,
    func_actor_400600_8013A990,
    func_actor_400600_8013AA5C,
} };

const TaskFuncTable6 D_actor_400600_80131E54 = { {
    func_actor_400600_8013AB98,
    func_actor_400600_8013AC14,
    func_actor_400600_801328A8,
    func_actor_400600_8013AD3C,
    func_actor_400600_8013ADA4,
    func_actor_400600_801329EC,
} };

const TaskFuncTable4 D_actor_400600_80131E6C = { {
    func_actor_400600_8013AE88,
    func_actor_400600_8013AF04,
    func_actor_400600_80132B3C,
    func_actor_400600_8013B018,
} };

const TaskFuncTable8 D_actor_400600_80131E7C = { {
    func_actor_400600_8013B0FC,
    func_actor_400600_80132C70,
    func_actor_400600_80132E10,
    func_actor_400600_80132F3C,
    func_actor_400600_8013B150,
    func_actor_400600_8013B1DC,
    func_actor_400600_80133118,
    func_actor_400600_8013B2A8,
} };

const TaskFuncTable4 D_actor_400600_80131E9C = { {
    func_actor_400600_8013B394,
    func_actor_400600_801332F4,
    func_actor_400600_8013B410,
    func_actor_400600_8013B520,
} };

const TaskFuncTable9 D_actor_400600_80131EAC = { {
    func_actor_400600_80133434,
    func_actor_400600_801337A8,
    func_actor_400600_8013A170,
    func_actor_400600_8013A26C,
    func_actor_400600_80138C34,
    func_actor_400600_80138D78,
    func_actor_400600_80138EA0,
    func_actor_400600_80138FD4,
    func_actor_400600_80137EF0,
} };

void func_actor_400600_80133434(Task* arg0)
{
    TmdObject*       model;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    Actor400600Work* work;
    Actor400600Work* w2;
    Actor400600Work* w3;
    Actor400600Work* w4;
    u32              rnd;

    model      = (TmdObject*)arg0->extra;
    enemy      = (GpEnemy*)arg0->spawnArg2;
    coord      = model->coords;
    arg0->work = memCalloc(0x770U, false);
    work       = (Actor400600Work*)arg0->work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    func_actor_400600_8013B640();
    if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x03140000) {
        work->field_76E = 1;
    }
    model->lightMtx   = &work->matrix_40;
    model->colorMtx   = &work->matrix_20;
    model->flags      = 0;
    arg0->msgTable    = D_actor_400600_80151AE0;
    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)arg0->extra)->coords[3];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags        = 5;
    enemy->param             = &D_actor_400600_80144EB0;
    enemy->recs              = work->rec_4D4;
    work->eff_6FC.coord      = &((TmdObject*)arg0->extra)->coords[3];
    work->eff_6FC.spawnArgLo = 0x300;
    work->eff_6FC.spawnArgHi = 2;
    enemy->hp = enemy->hpMax = D_actor_400600_80144EB0.hpMax;
    func_800B3F84(&work->anim, D_actor_400600_80151A54, model, work->pad_394, work->slots);

    w2            = (Actor400600Work*)arg0->work;
    w2->field_726 = 0x10;
    w2->field_746 = 1;
    w2->field_742 = 2;

    Actor400600_TickAnim(arg0);

    coord->sub     = &gGfxViewCoord;
    work->field_82 = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    func_actor_400600_8013203C(arg0);
    work->obj_4B4.flags |= 0x4000;
    func_actor_400600_801356E0(arg0);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    func_actor_400600_80138A24(arg0, 1);
    w3              = (Actor400600Work*)arg0->work;
    w3->field_71C   = 0;
    w3->field_71E   = 0;
    work->field_90  = coord->coord.t[0];
    work->field_92  = coord->coord.t[1];
    work->field_94  = coord->coord.t[2];
    rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
    Gp_LcgState     = rnd;
    work->field_716 = rnd >> 0x10;
    work->field_73E = work->field_92;
    switch ((u8)arg0->spawnArg1 >> 4) {
        case 0:
            w4            = (Actor400600Work*)arg0->work;
            arg0->state   = 1;
            w4->field_71C = 0;
            w4->field_71E = 0;
            break;
        case 1:
            work->field_76C = 1;
            w4              = (Actor400600Work*)arg0->work;
            arg0->state     = 4;
            w4->field_71C   = 0;
            w4->field_71E   = 0;
            break;
        case 2:
            w4            = (Actor400600Work*)arg0->work;
            arg0->state   = 5;
            w4->field_71C = 0;
            w4->field_71E = 0;
            break;
        case 3:
            w4            = (Actor400600Work*)arg0->work;
            arg0->state   = 6;
            w4->field_71C = 0;
            w4->field_71E = 0;
            break;
        case 4:
            w4            = (Actor400600Work*)arg0->work;
            arg0->state   = 7;
            w4->field_71C = 0;
            w4->field_71E = 0;
            break;
        case 5:
            w4            = (Actor400600Work*)arg0->work;
            arg0->state   = 8;
            w4->field_71C = 0;
            w4->field_71E = 0;
            break;
    }
}

const TaskFuncTable18 D_actor_400600_80131EEC = { {
    func_actor_400600_801390FC,
    func_actor_400600_80133B88,
    func_actor_400600_80139110,
    func_actor_400600_80139218,
    func_actor_400600_80139280,
    func_actor_400600_801392E8,
    func_actor_400600_8013935C,
    func_actor_400600_801393D0,
    func_actor_400600_80139444,
    func_actor_400600_801394E0,
    func_actor_400600_80139560,
    func_actor_400600_80139608,
    func_actor_400600_80139670,
    func_actor_400600_801396E4,
    func_actor_400600_80139764,
    func_actor_400600_801397E4,
    func_actor_400600_80139878,
    func_actor_400600_801398E0,
} };

void func_actor_400600_801337A8(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    TaskFuncTable18  fns   = D_actor_400600_80131EEC;

    switch (D_801153F4) {
        case 2:
            model->flags |= 0x80;
            func_actor_400600_801387DC(arg0, -1);
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80138AB8(arg0);
            func_actor_400600_80137840(arg0);
            func_actor_400600_80136558(arg0);
            Actor400600_TickAnim(arg0);
            work->field_710.h.flags = work->slots[1].flags;
            Actor400600_RebuildRotation(arg0);
            func_actor_400600_80136968(arg0);
            if (enemy->hp <= 0 && (u8)work->field_767 == 0) {
                Actor400600Work* w = (Actor400600Work*)arg0->work;
                arg0->state        = 2;
                w->field_71C       = 0;
                w->field_71E       = 0;
            }
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            Actor400600_UpdateColor(arg0);
            func_actor_400600_80138224(arg0, work->field_73E, work->field_73A);
            if (work->field_75C.b.field_75E == 0) {
                model->flags &= ~0x80;
                func_actor_400600_801387DC(arg0, -1);
            }
            break;
    }
}

void func_actor_400600_80133B88(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    s32              sound;
    s32              pan;
    u32              rnd;

    work = (Actor400600Work*)arg0->work;
    if (work->field_728 < 0xBB8 || D_80115417 != 0) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40060004;
        pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        func_actor_400600_80138B5C(arg0, 0);
        Gp_ArmStateF0(1);
        rnd              = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = rnd;
        work->field_758  = ((rnd >> 0x10) & 0x3F) + 0x1E;
        work2            = (Actor400600Work*)arg0->work;
        work2->field_71C = 2;
        work2->field_71E = 0;
        return;
    }
    if ((s16)func_actor_400600_80136FA8(arg0) != 0) {
        Gp_ArmStateF0(1);
    }
}

void func_actor_400600_80133CB0(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              id;
    u32              sound;
    s32              pan;

    work = (Actor400600Work*)arg0->work;
    work->field_718++;
    if ((s16)work->field_718 == 0x15) {
        id = 0x40060005;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan   = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->obj_5CC.flags |= 0x8000;
    }
    if ((s16)work->field_718 == 0x1C) {
        work->obj_5CC.flags &= 0x7FFF;
    }
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        func_actor_400600_80138AF0(arg0, 0x2D);
        func_actor_400600_80138AA4(arg0);
        if (work->field_768 == 0 && work->field_728 < 0x578 && (u16)(work->field_72C - 0x200) > 0xC00 && (u16)(work->field_72A - 0x200) > 0xC00) {
            work2            = (Actor400600Work*)arg0->work;
            work2->field_71C = 9;
            work2->field_71E = 0;
        } else {
            work3            = (Actor400600Work*)arg0->work;
            work3->field_71C = 2;
            work3->field_71E = 0;
        }
    }
}

void func_actor_400600_80133E38(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              id;
    u32              sound;
    s32              pan;

    work = (Actor400600Work*)arg0->work;
    work->field_718++;
    if ((s16)work->field_718 == 0x15) {
        id = 0x40060005;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan   = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->obj_594.flags |= 0x8000;
    }
    if ((s16)work->field_718 == 0x1C) {
        work->obj_594.flags &= 0x7FFF;
    }
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        func_actor_400600_80138AF0(arg0, 0x2D);
        func_actor_400600_80138AA4(arg0);
        if (work->field_768 == 0 && work->field_728 < 0x578 && (u16)(work->field_72C - 0x200) > 0xC00 && (u16)(work->field_72A - 0x200) > 0xC00) {
            work2            = (Actor400600Work*)arg0->work;
            work2->field_71C = 9;
            work2->field_71E = 0;
        } else {
            work3            = (Actor400600Work*)arg0->work;
            work3->field_71C = 2;
            work3->field_71E = 0;
        }
    }
}

void func_actor_400600_80133FC0(Task* arg0)
{
    Actor400600Msg3FF msg;
    Actor400600Msg3F8 query;
    Actor400600Work*  work;
    Actor400600Work*  work2;
    Actor400600Work*  work3;
    s32               base;
    s32               sound;
    s32               pan;

    work = (Actor400600Work*)arg0->work;
    if (((GameActor*)Gp_ActorSlots[0]->work)->field_954 == 2 || (func_actor_400600_801376EC(arg0) << 0x10) != 0 || work->field_728 >= 0x7D0 || (u32)(work->field_72C - 0x200) < 0xC01U) {
        func_actor_400600_80138B40(arg0);
        work2            = (Actor400600Work*)arg0->work;
        work2->field_71C = 2;
        work2->field_71E = 0;
        func_actor_400600_80135998(arg0, work->field_752);
        return;
    }
    query.field_14 = 8;
    if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&query, 0) != 0) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
        if (work->field_768 == 0) {
            work3            = (Actor400600Work*)arg0->work;
            work3->field_71C = 2;
            work3->field_71E = 0;
            return;
        }
        work2            = (Actor400600Work*)arg0->work;
        work2->field_71C = 0xD;
        work2->field_71E = 0;
        return;
    }
    work->field_73E = work->field_92;
    func_actor_400600_80138B40(arg0);
    work->field_768      = 0;
    Gp_StateC08.field_6 |= 1;
    work->field_767      = 1;
    work->field_9A       = work->field_92;
    msg.field_0          = D_actor_400600_80151A48;
    msg.field_8          = 0;
    msg.field_C          = 0;
    msg.field_10         = 0;
    msg.field_4          = 1;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&msg, 0);
    work->obj_4B4.flags &= 0x3FFF;
    work2                = (Actor400600Work*)arg0->work;
    work2->field_720     = 4;
    work2->field_726     = 0x10;
    work2->field_746     = 5;
    work2->field_742     = 1;
    work->field_718      = 0;
    work->field_71A      = 0;
    base                 = 0x40060004;
    if ((arg0->spawnArg1 & 0xF0) == 0x10) {
        base = 0x404A0004;
    }
    sound = base | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
    pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    work->field_75C.b.field_75C = 0;
    work->field_71E++;
}

void func_actor_400600_80134218(Task* arg0)
{
    Actor400600Msg3FF msg;
    SVECTOR           vec;
    Actor400600Work*  work;
    Actor400600Work*  work2;
    GpEnemy*          enemy;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    player;
    GsCOORDINATE2*    root;
    s32               id;
    s32               sound;
    s32               pan;
    s32               sound2;
    s32               pan2;
    s32               y;
    s32               ty;

    work               = (Actor400600Work*)arg0->work;
    coord              = ((TmdObject*)arg0->extra)->coords;
    enemy              = (GpEnemy*)arg0->spawnArg2;
    player             = ((TmdObject*)Gp_ActorSlots[0]->extra)->coords;
    work->field_84    += -(s16)work->field_84 >> 2;
    coord->coord.t[0] += (player->coord.t[0] - coord->coord.t[0]) >> 2;
    coord->coord.t[2] += (player->coord.t[2] - coord->coord.t[2]) >> 2;
    y                  = coord->coord.t[1];
    ty                 = y + 900;
    coord->coord.t[1]  = y + ((player->coord.t[1] - ty) >> 2);
    work->field_718++;
    if (++work->field_71A == 8) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 6;
        pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_763 == 1 || work->field_764 == 1 || enemy->hp <= 0 || work->field_75C.b.field_75C >= 3) {
        work->field_763 = 0;
        if (work->field_764 == 0) {
            msg.field_0  = D_actor_400600_80151A48;
            msg.field_8  = 1;
            msg.field_C  = 8;
            msg.field_10 = 0;
            msg.field_4  = 2;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F4, (s32)&msg, 0);
        }
        work2                = (Actor400600Work*)arg0->work;
        work2->field_720     = 8;
        work2->field_726     = 0x10;
        work2->field_746     = 6;
        work2->field_742     = 1;
        work->field_722      = 0;
        work->field_724      = 0;
        work->field_718      = 0;
        work->obj_4B4.flags |= 0x4000;
        work->field_71E++;
        return;
    }
    if ((s16)work->field_718 == 0xD || (s16)work->field_718 == 0x1A) {
        root = &((TmdObject*)Gp_ActorSlots[0]->extra)->coords[4];
        Gp_SpawnPadLerp(0xA, 0xC0, 8);
        id = 0x40060009;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0009;
        }
        sound2 = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan2   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound2, pan2, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 1), 0) != 0) {
            work->field_764 = 1;
        }
        vec.vx = 0;
        vec.vy = -200;
        vec.vz = 0;
        Gp_SpawnEff(0x6009B, root, 0x10100, &vec);
    }
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        work->field_718 = 0;
        work->field_75C.b.field_75C++;
    }
}

void func_actor_400600_80134570(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   player;
    GsCOORDINATE2*   root;
    SVECTOR          vec;
    s32              i;
    s32              y;
    s32              id;
    s32              sound;
    s32              pan;
    s16              vy;

    work            = (Actor400600Work*)arg0->work;
    coord           = ((TmdObject*)arg0->extra)->coords;
    player          = ((TmdObject*)Gp_ActorSlots[0]->extra)->coords;
    work->field_84 += -(s16)work->field_84 >> 2;
    work->field_718++;
    if ((s16)work->field_718 >= 8) {
        work->obj_4B4.flags |= 0x8000;
        work->field_722     += 2;
        work->field_724     += work->field_722;
        y                    = coord->coord.t[1] + work->field_724;
        coord->coord.t[1]    = y;
        if (y >= (s16)work->field_9A) {
            coord->coord.t[1] = (s16)work->field_9A;
            player->flg       = 0;
            Gp_UpdateCoord(player);
            if (work->field_76C != 0) {
                vy   = -0x1A4;
                root = ((TmdObject*)arg0->extra)->coords;
                Gp_SpawnEff(D_8011574C, root, 0x40, NULL);
                for (i = 0; i < 16; i++) {
                    vec.vx = (u32)rsin(i << 8) >> 3;
                    vec.vy = vy;
                    vec.vz = (u32)rcos(i << 8) >> 3;
                    Gp_SpawnEff(D_80115738, root, 0x01202148, &vec);
                }
            }
            id = 0x40060003;
            if ((arg0->spawnArg1 & 0xF0) == 0x10) {
                id = 0x404A0003;
            }
            sound = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
            pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
            work->obj_4B4.flags |= 0x4000;
            work2                = (Actor400600Work*)arg0->work;
            work2->field_720     = 2;
            work2->field_746     = 0x19;
            work2->field_726     = 0x10;
            work2->field_742     = 1;
            work->field_71E++;
        }
    }
}

void func_actor_400600_8013479C(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s16              v;

    work = (Actor400600Work*)arg0->work;
    v    = func_actor_400600_801376EC(arg0);
    if (v != 0) {
        if ((u16)(v - 0x4E9) >= 0x6D0U) {
            func_actor_400600_80138B40(arg0);
            work3            = (Actor400600Work*)arg0->work;
            work3->field_71C = 2;
            work3->field_71E = 0;
            return;
        }
        work->field_98 = ((rsin((s16)work->field_82 + 0x800) * (v - 0x100)) >> 12) / 20;
        work->field_9C = ((rcos((s16)work->field_82 + 0x800) * (v - 0x100)) >> 12) / 20;
    } else {
        work->field_98 = ((rsin((s16)work->field_82 + 0x800) * 3000) >> 12) / 20;
        work->field_9C = ((rcos((s16)work->field_82 + 0x800) * 3000) >> 12) / 20;
    }
    func_actor_400600_80138B40(arg0);
    work2            = (Actor400600Work*)arg0->work;
    work2->field_720 = 4;
    work2->field_726 = 0x10;
    work2->field_746 = 0x15;
    work2->field_742 = 1;
    work->field_722  = -0x2A;
    work->field_724  = 0;
    work->field_718  = 0;
    work->field_71E++;
}

void func_actor_400600_80134970(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   root;
    SVECTOR          vec;
    s32              i;
    s32              y;
    s32              id;
    s32              sound;
    s32              pan;
    s16              vy;

    work  = (Actor400600Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    work->field_718++;
    if ((s16)work->field_718 < 0x11) {
        func_actor_400600_80136FA8(arg0);
        return;
    }
    if ((s16)work->field_718 == 0x11) {
        work->field_730 = 0;
        work->field_767 = 1;
    }
    coord->coord.t[0] += (s16)work->field_98;
    coord->coord.t[2] += (s16)work->field_9C;
    work->field_722   += 6;
    work->field_724   += work->field_722;
    y                  = coord->coord.t[1] + work->field_724;
    coord->coord.t[1]  = y;
    if (y >= (s16)work->field_92) {
        coord->coord.t[1] = (s16)work->field_92;
        vy                = -0x1A4;
        if (work->field_76C != 0) {
            root = ((TmdObject*)arg0->extra)->coords;
            Gp_SpawnEff(D_8011574C, root, 0x40, NULL);
            for (i = 0; i < 16; i++) {
                vec.vx = (u32)rsin(i << 8) >> 3;
                vec.vy = vy;
                vec.vz = (u32)rcos(i << 8) >> 3;
                Gp_SpawnEff(D_80115738, root, 0x01202148, &vec);
            }
        }
        id = 0x40060003;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work2            = (Actor400600Work*)arg0->work;
        work2->field_720 = 2;
        work2->field_746 = 0x19;
        work2->field_726 = 0x10;
        work2->field_742 = 1;
        work->field_767  = 0;
        work->field_71E++;
    }
}

void func_actor_400600_80134B98(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    GsCOORDINATE2*   coord;

    work  = (Actor400600Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    func_actor_400600_80139E68(arg0, 0xE, &work->field_88);
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        work->field_82   = (work->field_82 + 0x800) & 0xFFF;
        work2            = (Actor400600Work*)arg0->work;
        work2->field_726 = 0x10;
        work2->field_746 = 2;
        work2->field_742 = 2;
        Actor400600_RebuildRotation(arg0);
        Actor400600_TickAnim(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        func_actor_400600_80139DCC(arg0, 0xB, &work->field_88);
        work->field_769  = 0;
        work3            = (Actor400600Work*)arg0->work;
        work3->field_71C = 2;
        work3->field_71E = 0;
    }
}

void func_actor_400600_80134E28(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GsCOORDINATE2*   coord;
    s32              id;
    s32              sound;
    s32              pan;
    s32              y;

    work  = (Actor400600Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    work->field_718++;
    if ((s16)work->field_718 < 0x11) {
        func_actor_400600_80136FA8(arg0);
        return;
    }
    if ((s16)work->field_718 == 0x11) {
        work->field_730      = 0;
        work->field_767      = 1;
        work->field_73E      = work->field_9A;
        work->obj_4B4.flags &= ~0x4000;
    }
    /* A separate statement: written inline, fold turns `a - (y + 400)` into
     * `(a - 400) - y`. */
    y                  = coord->coord.t[1] + 0x190;
    coord->coord.t[1] += ((s16)work->field_9A - y) >> 3;
    work->field_80    += (0x800 - (s16)work->field_80) >> 3;
    if ((s16)work->field_9A >= coord->coord.t[1]) {
        id = 0x40060003;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->obj_4B4.flags |= 0x4000;
        coord->coord.t[1]    = (s16)work->field_9A;
        work->field_80       = 0;
        work->field_84       = 0x800;
        work->field_82      += 0x800;
        Actor400600_RebuildRotation(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work2            = (Actor400600Work*)arg0->work;
        work2->field_720 = 2;
        work2->field_726 = 0x10;
        work2->field_746 = 0x19;
        work2->field_742 = 1;
        work->field_767  = 0;
        work->field_768  = 1;
        work->field_71E++;
    }
}

void func_actor_400600_801350F4(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GsCOORDINATE2*   coord;

    work  = (Actor400600Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    work->field_718++;
    if ((s16)work->field_718 < 8) {
        func_actor_400600_80139DCC(arg0, 3, &work->field_88);
        return;
    }
    work->field_88.x += ((s16)work->field_98 - work->field_88.x) >> 2;
    work->field_88.z += ((s16)work->field_9C - work->field_88.z) >> 2;
    func_actor_400600_80139E68(arg0, 3, &work->field_88);
    work->field_722   += 2;
    work->field_724   += work->field_722;
    coord->coord.t[1] += work->field_724;
    if ((work->field_80 & 0xFFF) != 0x800) {
        work->field_80 -= 0x80;
    }
    if ((s16)work->field_92 < coord->coord.t[1]) {
        work->field_768   = 0;
        coord->coord.t[0] = (s16)work->field_98;
        coord->coord.t[1] = (s16)work->field_92;
        coord->coord.t[2] = (s16)work->field_9C;
        work->field_80    = 0;
        work->field_84    = 0;
        work->field_82   += 0x800;
        Actor400600_RebuildRotation(arg0);
        work2            = (Actor400600Work*)arg0->work;
        work2->field_720 = 2;
        work2->field_726 = 0x10;
        work2->field_746 = 0x19;
        work2->field_742 = 1;
        Actor400600_TickAnim(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work->field_718 = 0;
        work->field_71E++;
    }
}

void func_actor_400600_80135450(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u32              sound;
    s32              id;
    s32              pan;
    u32              rnd;

    work = (Actor400600Work*)arg0->work;
    if ((s16)work->field_718 == 0) {
        id = 0x40060003;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan   = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->field_767 = 0;
        work->field_718++;
    }
    if ((func_actor_400600_801370F4(arg0) << 0x10) == 0 && (func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        rnd                     = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState             = rnd;
        work->field_710.h.timer = ((rnd >> 0x10) & 0x1F) + 0xD2;
        work2                   = (Actor400600Work*)arg0->work;
        work2->field_71C        = 2;
        work2->field_71E        = 0;
    }
}

void func_actor_400600_80135578(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    u32              sound;
    s32              id;
    s32              pan;
    u32              rnd;

    work = (Actor400600Work*)arg0->work;
    if ((s16)work->field_718 == 0) {
        work->field_767 = 0;
        id              = 0x40060006;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0006;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan   = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->field_718++;
    }
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        if (work->field_730 != 3) {
            work2            = (Actor400600Work*)arg0->work;
            work2->field_720 = 2;
            work2->field_726 = 0x10;
            work2->field_746 = 0x14;
            work2->field_742 = 1;
            work->field_71E++;
            return;
        }
        work->field_730  = 0;
        rnd              = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = rnd;
        work->field_750  = ((rnd >> 0x10) & 0x7F) + 0x1E;
        work3            = (Actor400600Work*)arg0->work;
        work3->field_71C = 5;
        work3->field_71E = 0;
    }
}

/// Spawn the two child models from `D_actor_400600_80151AF8`, parent them to
/// root parts 10 and 7 at +/-0x200 along X, turn each by -/+0x180 from an
/// identity rotation, copy the parent's texture page and CLUT row, and point
/// their light / color matrices at this actor's own.
void func_actor_400600_801356E0(Task* arg0)
{
    Actor400600Work*         work;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           root;
    GsCOORDINATE2*           parent;
    GsCOORDINATE2*           parent2;
    Task*                    task;
    TmdObject*               obj;
    TmdObject*               dst;
    TmdObject*               src;
    MATRIX*                  mdst;
    ActorsShared8016a538Mat* pm;
    ActorsShared8016a538Mat* pm2;
    ActorsShared8016a538Mat  m;

    root            = ((TmdObject*)arg0->extra)->coords;
    work            = (Actor400600Work*)arg0->work;
    parent          = &root[7];
    parent2         = &root[10];
    task            = Task_SpawnFromTable(&D_actor_400600_80151AF8, 0, 0, 0);
    work->field_704 = task;
    if (task != NULL) {
        obj               = (TmdObject*)task->extra;
        coord             = obj->coords;
        obj->flags        = 0x80;
        coord->coord.t[0] = 0x200;
        coord->sub        = parent2;
        coord->coord.t[1] = 0;
        coord->coord.t[2] = 0;
        pm                = &m;
        pm->ident.m00_m01 = 0x1000;
        pm->ident.m02_m10 = 0;
        pm->ident.m11_m12 = 0x1000;
        pm->ident.m20_m21 = 0;
        pm->ident.m22     = 0x1000;
        func_8004BFF8(-0x180, &pm->mat);
        mdst          = &coord->coord;
        mdst->m[0][0] = pm->mat.m[0][0];
        mdst->m[0][1] = pm->mat.m[0][1];
        mdst->m[0][2] = pm->mat.m[0][2];
        mdst->m[1][0] = pm->mat.m[1][0];
        mdst->m[1][1] = pm->mat.m[1][1];
        mdst->m[1][2] = pm->mat.m[1][2];
        mdst->m[2][0] = pm->mat.m[2][0];
        mdst->m[2][1] = pm->mat.m[2][1];
        mdst->m[2][2] = pm->mat.m[2][2];
        src           = (TmdObject*)arg0->extra;
        dst           = (TmdObject*)task->extra;
        dst->tpage    = src->tpage;
        dst->clut     = src->clut;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
        obj->lightMtx = &work->matrix_40;
        obj->colorMtx = &work->matrix_20;
    }
    task = work->field_708 = Task_SpawnFromTable(&D_actor_400600_80151AF8, 1, 0, 0);
    if (task != NULL) {
        obj               = (TmdObject*)task->extra;
        coord             = obj->coords;
        obj->flags        = 0x80;
        coord->sub        = parent;
        coord->coord.t[0] = -0x200;
        coord->coord.t[1] = 0;
        coord->coord.t[2] = 0;
        src               = (TmdObject*)arg0->extra;
        dst               = (TmdObject*)task->extra;
        dst->tpage        = src->tpage;
        dst->clut         = src->clut;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
        pm2                = &m;
        pm2->ident.m00_m01 = 0x1000;
        pm2->ident.m02_m10 = 0;
        pm2->ident.m11_m12 = 0x1000;
        pm2->ident.m20_m21 = 0;
        pm2->ident.m22     = 0x1000;
        func_8004BFF8(0x180, &pm2->mat);
        mdst          = &coord->coord;
        mdst->m[0][0] = pm2->mat.m[0][0];
        mdst->m[0][1] = pm2->mat.m[0][1];
        mdst->m[0][2] = pm2->mat.m[0][2];
        mdst->m[1][0] = pm2->mat.m[1][0];
        mdst->m[1][1] = pm2->mat.m[1][1];
        mdst->m[1][2] = pm2->mat.m[1][2];
        mdst->m[2][0] = pm2->mat.m[2][0];
        mdst->m[2][1] = pm2->mat.m[2][1];
        mdst->m[2][2] = pm2->mat.m[2][2];
        obj->lightMtx = &work->matrix_40;
        obj->colorMtx = &work->matrix_20;
    }
}

/// Animation state 2: the landing slam. Same two sound/tracking windows as
/// `func_actor_400600_80135DDC`, plus the dust ring each window spawns when
/// `field_76C` says the actor is over ground.
void func_actor_400600_80135998(Task* arg0, s16 arg1)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;
    /* The first window starts at frame 0, written as the literal both of its
     * tests fold against; the wider first temp is what keeps each zero arm a
     * fresh constant instead of a copy of the previous one. */
    u32     tmp0;
    u8      tmp1;
    u8      tmp2;
    u8      end0;
    u8      start1;
    u8      end1;
    s32     id;
    u32     sound;
    u32     voice;
    s32     pan;
    SVECTOR vec;

    work  = (Actor400600Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->field_746 != 2) {
        work->field_726 = 0x10;
        work->field_746 = 2;
        work->field_742 = 2;
        Actor400600_TickAnim(arg0);
    }
    if (((Actor400600Work*)arg0->work)->field_726 == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((Actor400600Work*)arg0->work)->field_726) >> 4;
    }
    end0 = tmp0;
    if (((Actor400600Work*)arg0->work)->field_726 == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((Actor400600Work*)arg0->work)->field_726) >> 4;
    }
    start1 = tmp1;
    if (((Actor400600Work*)arg0->work)->field_726 == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((Actor400600Work*)arg0->work)->field_726) >> 4;
    }
    end1 = tmp2;
    if ((func_actor_400600_8013892C(arg0) << 0x10) != 0) {
        work->field_748 = 0;
        work->field_726 = arg1;
    }
    if (work->field_748 == 0) {
        func_actor_400600_80139DCC(arg0, 0xB, &work->field_88);
        id = 0x40060001;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0001;
        }
        /* `voice` is a plain copy that the compiler propagates away; writing
         * `sound = id | sound` instead swaps the operands of the `or`. */
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        if (work->field_76C != 0) {
            vec.vx = 0;
            vec.vy = -0x1A4;
            vec.vz = 0;
            Gp_SpawnEff(D_8011574C, coord, 0x40, &vec);
        }
    }
    if (work->field_748 == start1) {
        func_actor_400600_80139DCC(arg0, 8, &work->field_88);
        id = 0x40060002;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0002;
        }
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        if (work->field_76C != 0) {
            vec.vx = 0;
            vec.vy = -0x1A4;
            vec.vz = 0;
            Gp_SpawnEff(D_8011574C, coord, 0x40, &vec);
        }
    }
    if (work->field_748 >= 0 && work->field_748 <= end0) {
        func_actor_400600_80139E68(arg0, 0xB, &work->field_88);
        work->field_734 = 8;
    }
    if (work->field_748 >= start1 && work->field_748 <= end1) {
        func_actor_400600_80139E68(arg0, 8, &work->field_88);
        work->field_734 = 0xB;
    }
    coord->flg = 0;
}

/// Animation state 4: drives the two sound/tracking windows the same way
/// `func_actor_400600_801361AC` does, one frame-count pair per sound event.
void func_actor_400600_80135DDC(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;
    /* The first window starts at frame 0. `start0` is still its own `u8`: the
     * width is what folds both of its tests against a literal zero, and the
     * wider first temp below is what keeps the zero arm a fresh constant
     * instead of a copy of it. */
    u8  start0;
    u32 tmp0;
    u8  tmp1;
    u8  tmp2;
    u8  end0;
    u8  start1;
    u8  end1;
    s32 id;
    u32 sound;
    u32 voice;
    s32 pan;

    work  = (Actor400600Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->field_746 != 4) {
        work->field_726 = 0x10;
        work->field_720 = 4;
        work->field_746 = 4;
        work->field_742 = 1;
        Actor400600_TickAnim(arg0);
    }
    start0 = 0;
    if (((Actor400600Work*)arg0->work)->field_726 == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xD00 / ((Actor400600Work*)arg0->work)->field_726) >> 4;
    }
    end0 = tmp0;
    if (((Actor400600Work*)arg0->work)->field_726 == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xE00 / ((Actor400600Work*)arg0->work)->field_726) >> 4;
    }
    start1 = tmp1;
    if (((Actor400600Work*)arg0->work)->field_726 == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1B00 / ((Actor400600Work*)arg0->work)->field_726) >> 4;
    }
    end1 = tmp2;
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        work->field_748 = 0;
    }
    if (work->field_748 == start0) {
        func_actor_400600_80139DCC(arg0, 8, &work->field_88);
        id = 0x40060001;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0001;
        }
        /* `voice` is a plain copy that the compiler propagates away; writing
         * `sound = id | sound` instead swaps the operands of the `or`. */
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_748 == start1) {
        func_actor_400600_80139DCC(arg0, 0xB, &work->field_88);
        id = 0x40060002;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0002;
        }
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_748 >= start0 && work->field_748 <= end0) {
        func_actor_400600_80139E68(arg0, 8, &work->field_88);
    }
    if (work->field_748 >= start1 && work->field_748 <= end1) {
        func_actor_400600_80139E68(arg0, 0xB, &work->field_88);
    }
    coord->flg = 0;
}

/// Drives the two sound/tracking windows of the current animation. The window
/// bounds are frame counts derived from the playback rate (`field_726`), each
/// read back through `arg0->work` rather than the cached `work` pointer.
void func_actor_400600_801361AC(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;
    /* The bounds are computed into their own temporaries first; a `u8` temp is
     * what keeps the zero arm of each test out of the surrounding block. */
    u8  tmp0;
    u8  tmp1;
    u8  tmp2;
    u8  end0;
    u8  start1;
    u8  end1;
    s32 start0;
    u32 sound;
    s32 pan;

    work  = (Actor400600Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->field_746 != 2) {
        work->field_726 = 0x10;
        work->field_746 = 2;
        work->field_742 = 2;
        Actor400600_TickAnim(arg0);
    }
    if (((Actor400600Work*)arg0->work)->field_726 == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((Actor400600Work*)arg0->work)->field_726) >> 4;
    }
    end0 = tmp0;
    if (((Actor400600Work*)arg0->work)->field_726 == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((Actor400600Work*)arg0->work)->field_726) >> 4;
    }
    start1 = tmp1;
    if (((Actor400600Work*)arg0->work)->field_726 == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((Actor400600Work*)arg0->work)->field_726) >> 4;
    }
    end1 = tmp2;
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        work->field_748 = 0;
    }
    /* The first window starts at frame 0, and the original compares against it
     * in a register: a literal 0 is folded into `$zero` by CSE. */
    SOFT_MOVE_ZERO(start0);
    if (work->field_748 == start0) {
        func_actor_400600_80139F4C(arg0, 0xB, &work->field_88);
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x404A000A;
        pan     = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_748 == start1) {
        func_actor_400600_80139F4C(arg0, 8, &work->field_88);
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x404A000B;
        pan     = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_748 >= start0 && work->field_748 <= end0) {
        func_actor_400600_80139FE0(arg0, 0xB, &work->field_88);
        work->field_734 = 8;
    }
    if (work->field_748 >= start1 && work->field_748 <= end1) {
        func_actor_400600_80139FE0(arg0, 8, &work->field_88);
        work->field_734 = 0xB;
    }
    coord->flg = 0;
}

void func_actor_400600_80136558(Task* arg0)
{
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    TmdObject*       model = (TmdObject*)arg0->extra;
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    s32              state = work->field_75C.word & 0xFFFF0000;
    s16              count;

    if (state == 0x1000000) {
        work->field_73A = (u16)work->field_73A + ((0xFF - work->field_73A) >> 5);
        work->field_740++;
        if (work->field_740 >= 0x20) {
            model->flags &= ~2;
            func_actor_400600_801387DC(arg0, -1);
            work->field_740             = 0;
            work->field_75C.b.field_75F = 0;
        }
    } else if (state == 0x1010000) {
        work->field_73A = (u16)work->field_73A + (-work->field_73A >> 3);
        count           = work->field_740 + 1;
        work->field_740 = count;
        if (count >= 0x12) {
            if (work->field_75A != 0) {
                enemy->node.flags = 4;
            } else {
                enemy->node.flags = 5;
            }
            model->flags |= 0x80;
            func_actor_400600_801387DC(arg0, -1);
            work->field_740             = 0;
            work->field_75C.b.field_75F = 0;
            work->field_73A             = 0;
        }
    }
}

void func_actor_400600_80136670(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   player;
    GameActor*       actor;
    Task*            slot;
    SVECTOR          v;
    s16              a;
    s16              b;

    work              = (Actor400600Work*)arg0->work;
    coord             = ((TmdObject*)arg0->extra)->coords;
    slot              = Gp_ActorSlots[0];
    work->field_70.vx = coord->coord.t[0];
    work->field_70.vy = coord->coord.t[1];
    work->field_70.vz = coord->coord.t[2];
    if (slot == NULL) {
        return;
    }
    player = ((TmdObject*)slot->extra)->coords;
    actor  = slot->work;
    if (work->field_76E != 0) {
        a = func_actor_400600_8013886C(arg0);
        b = func_actor_400600_8013886C((Task*)slot);
        if (a == 2 && (b == 1 || b == 6)) {
            work->field_A8.x = 0x1194;
            work->field_A8.y = 0;
            work->field_A8.z = 0;
        } else if (a == 3 && ((b >= 1 && b <= 2) || b == 6)) {
            work->field_A8.x = 0x1194;
            work->field_A8.y = 0;
            work->field_A8.z = -0x1194;
        } else if (a == 4 && ((b >= 1 && b <= 3) || b == 6)) {
            work->field_A8.x = -0x125C;
            work->field_A8.y = 0;
            work->field_A8.z = -0x1194;
        } else if (a == 5 && ((b >= 1 && b <= 4) || b == 6)) {
            work->field_A8.x = -0x1194;
            work->field_A8.y = 0;
            work->field_A8.z = 0x1194;
        } else if (b == 1 && a == 6) {
            work->field_A8.x = 0;
            work->field_A8.y = 0;
            work->field_A8.z = 0;
        } else if (b != 1 && a == 1) {
            work->field_A8.x = 0x1194;
            work->field_A8.y = 0;
            work->field_A8.z = 0;
        } else {
            work->field_A8.x = player->coord.t[0];
            work->field_A8.y = player->coord.t[1];
            work->field_A8.z = player->coord.t[2];
        }
    } else {
        work->field_A8.x = player->coord.t[0];
        work->field_A8.y = player->coord.t[1];
        work->field_A8.z = player->coord.t[2];
    }
    v.vx            = work->field_A8.x - coord->coord.t[0];
    v.vy            = work->field_A8.y - coord->coord.t[1];
    v.vz            = work->field_A8.z - coord->coord.t[2];
    work->field_728 = SquareRoot0(v.vx * v.vx + v.vz * v.vz);
    VectorNormalSS(&v, &v);
    work->field_72C = (ratan2(v.vx, v.vz) - work->field_82) & 0xFFF;
    work->field_72A = (ratan2(-v.vx, -v.vz) - actor->field_52) & 0xFFF;
}

const TaskFuncTable3 D_actor_400600_80131F34 = { {
    func_actor_400600_8013BA6C,
    func_actor_400600_8013BAEC,
    func_actor_400600_8013BB88,
} };

const TaskFuncTable8 D_actor_400600_80131F40 = { {
    func_actor_400600_8013BCD8,
    func_actor_400600_8013BD54,
    func_actor_400600_8013BDF0,
    func_actor_400600_8013BE58,
    func_actor_400600_80133FC0,
    func_actor_400600_80134218,
    func_actor_400600_80134570,
    func_actor_400600_8013BE90,
} };

const TaskFuncTable4 D_actor_400600_80131F60 = { {
    func_actor_400600_8013BF48,
    func_actor_400600_8013479C,
    func_actor_400600_80134970,
    func_actor_400600_8013BF80,
} };

const TaskFuncTable3 D_actor_400600_80131F70 = { {
    func_actor_400600_8013C038,
    func_actor_400600_80134E28,
    func_actor_400600_8013C074,
} };

const TaskFuncTable4 D_actor_400600_80131F7C = { {
    func_actor_400600_8013C104,
    func_actor_400600_8013C124,
    func_actor_400600_801350F4,
    func_actor_400600_80135450,
} };

const TaskFuncTable4 D_actor_400600_80131F8C = { {
    func_actor_400600_8013C1C0,
    func_actor_400600_8013C238,
    func_actor_400600_80135578,
    func_actor_400600_8013C2D4,
} };

const TaskFuncTable3 D_actor_400600_80131F9C = { {
    func_actor_400600_8013C394,
    func_actor_400600_8013C410,
    func_actor_400600_8013C4AC,
} };

void func_actor_400600_80136968(Task* arg0)
{
    SVECTOR          push;
    SVECTOR          pos;
    GpDeltaScratch   delta;
    GsCOORDINATE2*   eff;
    s16              maxX;
    s16              maxZ;
    s16              stepX;
    s16              stepZ;
    u8               blocked;
    Actor400600Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    s16              amount;
    s32              dmg;
    s32              tmp;
    s16              tick;
    s32              i;

    maxX            = 0;
    maxZ            = 0;
    stepX           = 0;
    stepZ           = 0;
    blocked         = 0;
    coord           = ((TmdObject*)arg0->extra)->coords;
    work            = (Actor400600Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    work->field_72E = 0;
    eff             = &coord[3];

    for (i = 0; i < 8; i++) {
        switch (work->rec_4D4[i].key & 0xFFFF0000) {
            case 0x10000:
            case 0x30000:
                pos.vx = coord->workm.t[0];
                pos.vy = coord->workm.t[1];
                pos.vz = coord->workm.t[2];
                func_actor_400600_8013C6B0(&pos, &work->rec_4D4[i], &push);
                if (ABS(maxX) < ABS(push.vx)) {
                    maxX = push.vx;
                }
                if (ABS(maxZ) < ABS(push.vz)) {
                    maxZ = push.vz;
                }
                break;
            case 0x20000:
                if (work->field_74A == 0) {
                    work->field_72E = 1;
                    dmg             = Gp_ComputeDamage(work->rec_4D4[i].key, work->field_728, 0, 0);
                    amount          = dmg;
                    work->field_74A = Gp_GetIdParam2(work->rec_4D4[i].key);
                    if (Gp_RollEnemyChance(enemy, work->rec_4D4[i].key, 0) != 0) {
                        amount = ((u32)dmg << 16) >> 14;
                        Gp_SpawnEff(0x6009C, &((TmdObject*)arg0->extra)->coords[3], 0, NULL);
                    }
                    func_800E2C78((GpObj40*)enemy, work->rec_4D4[i].key, amount, 0);
                    func_800DA6E8(&enemy->node, amount, 0);
                    enemy->hp -= amount;
                    if (enemy->hp < 0) {
                        enemy->hp = 0;
                    }
                    if ((work->rec_4D4[i].key & 0x7F) == 0xE) {
                        if (!(work->rec_4D4[i].key & 0x8000)) {
                            work->field_75A = 0x258;
                        }
                    } else {
                        func_800FDB18(Gp_GetIdParam1(work->rec_4D4[i].key) & 0xFFFF,
                                      &((TmdObject*)arg0->extra)->coords[4], NULL, &work->eff_6FC);
                    }
                    if (amount >= 0x64) {
                        work->field_730 = 2;
                    } else {
                        work->field_730 = 1;
                    }
                    switch (Gp_GetIdParam0(work->rec_4D4[i].key) & 0xFFFF) {
                        case 0:
                            break;
                        case 1:
                            Gp_SetObjFlag1((GpObj4C*)enemy);
                            break;
                        case 2:
                            Gp_SetObjFlag2((GpObj5D*)enemy, work->rec_4D4[i].key, 0);
                            break;
                        case 3:
                            Gp_SetObjFlag4((GpObj5C*)enemy, work->rec_4D4[i].key, 0);
                            break;
                        case 4:
                            work->field_730 = 4;
                            break;
                        case 5:
                            work->field_730 = 2;
                            break;
                        case 6:
                            work->field_730 = 4;
                            break;
                        case 7:
                            work->field_730 = 2;
                            break;
                        case 8:
                            work->field_730 = 3;
                            break;
                        case 9:
                            work->field_730 = 3;
                            break;
                    }
                } else if ((Gp_GetIdParam1(work->rec_4D4[i].key) & 0xFFFF) == 0xD) {
                    func_800FDB18(0xD, &((TmdObject*)arg0->extra)->coords[1], NULL, &work->eff_6FC);
                }
                break;
        }
    }

    if (enemy->reactionFlags & 1) {
        enemy->reactionFlags &= 0xFE;
        work->field_730       = 5;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= 0xFD;
        work->field_730       = 3;
    }
    if (enemy->reactionFlags & 0xC) {
        work->field_760 = 1;
        tmp             = Gp_TickObjFlag4((GpObj5C*)enemy);
        tick            = tmp;
        if (tick != 0) {
            enemy->hp -= tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if (enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->field_72E = 1;
            work->field_730 = 2;
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
    }

    switch (func_800E0C10(work->rec_4D4, &delta, 8, NULL)) {
        case 0:
            break;
        case 1:
            stepZ = delta.vz.h.hi;
            stepX = delta.vx.w >> 16;
            if (delta.vx.w & 0xFFFF) {
                if (delta.vx.w > 0) {
                    stepX++;
                } else {
                    stepX--;
                }
            }
            if (delta.vz.w & 0xFFFF) {
                if (delta.vz.w > 0) {
                    stepZ++;
                } else {
                    stepZ--;
                }
            }
            break;
        case 2:
            coord->coord.t[0] = work->field_70.vx;
            coord->coord.t[2] = work->field_70.vz;
            coord->flg        = 0;
            blocked           = 1;
            break;
    }

    Gp_ClearRec18Occupied(work->rec_4D4);
    if (work->field_74A > 0) {
        work->field_74A--;
    } else {
        work->field_74A = 0;
    }
    if (blocked == 0) {
        work->field_88.x  += (s16)func_actor_400600_8013C7E8(stepX, maxX);
        work->field_88.z  += (s16)func_actor_400600_8013C7E8(stepZ, maxZ);
        coord->coord.t[0] += (s16)func_actor_400600_8013C7E8(stepX, maxX);
        coord->coord.t[2] += (s16)func_actor_400600_8013C7E8(stepZ, maxZ);
        coord->flg         = 0;
    }
    if (work->field_75A != 0) {
        work->field_75A--;
        if ((work->field_75A & 7) == 1) {
            Gp_SpawnEff(0x60080, eff, 0x10200, NULL);
        }
    }
}

s32 func_actor_400600_80136FA8(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;

    work = (Actor400600Work*)arg0->work;
    if (work->field_72E != 1) {
        return 0;
    }
    if (work->field_768 == 0) {
        switch (work->field_730) {
            case 1:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 3;
                work2->field_71E = 0;
                break;
            case 2:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 4;
                work2->field_71E = 0;
                break;
            case 3:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 5;
                work2->field_71E = 0;
                break;
            case 4:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 4;
                work2->field_71E = 0;
                break;
            case 5:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 0xF;
                work2->field_71E = 0;
                break;
        }
        work->field_730 = 0;
    } else {
        switch (work->field_730) {
            case 1:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 3;
                work2->field_71E = 0;
                work->field_730  = 0;
                break;
            case 2:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 4;
                work2->field_71E = 0;
                work->field_730  = 0;
                break;
            case 3:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 0xE;
                work2->field_71E = 0;
                break;
            case 4:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 4;
                work2->field_71E = 0;
                work->field_730  = 0;
                break;
            case 5:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 0xE;
                work2->field_71E = 0;
                work->field_730  = 0;
                break;
        }
    }
    work->field_76D = 0;
    func_actor_400600_80138B40(arg0);
    return 1;
}

s32 func_actor_400600_801370F4(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;

    work = (Actor400600Work*)arg0->work;
    if (work->field_768 == 0) {
        switch (work->field_730) {
            case 1:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 3;
                work2->field_71E = 0;
                work->field_730  = 0;
                return 1;
            case 2:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 4;
                work2->field_71E = 0;
                work->field_730  = 0;
                return 1;
            case 3:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 5;
                work2->field_71E = 0;
                work->field_730  = 0;
                return 1;
            case 4:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 4;
                work2->field_71E = 0;
                work->field_730  = 0;
                return 1;
            case 5:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 0xF;
                work2->field_71E = 0;
                work->field_730  = 0;
                return 1;
        }
        work->field_730 = 0;
        return 0;
    } else {
        switch (work->field_730) {
            case 1:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 3;
                work2->field_71E = 0;
                work->field_730  = 0;
                return 1;
            case 2:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 4;
                work2->field_71E = 0;
                work->field_730  = 0;
                return 1;
            case 3:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 0xE;
                work2->field_71E = 0;
                return 1;
            case 4:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 4;
                work2->field_71E = 0;
                work->field_730  = 0;
                return 1;
            case 5:
                work2            = (Actor400600Work*)arg0->work;
                work2->field_71C = 0xE;
                work2->field_71E = 0;
                work->field_730  = 0;
                return 1;
        }
        work->field_730 = 0;
        return 0;
    }
}

void func_actor_400600_80137240(Task* arg0)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    GpEffWork* eff3;
    GpEffWork* eff4;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* dst3;
    TmdObject* dst4;
    TmdObject* src;
    TmdObject* src2;
    TmdObject* src3;
    TmdObject* src4;

    D_800678F0[0] = D_actor_400600_8014220C;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[4], 0x200, NULL);
    if (eff != NULL) {
        src        = (TmdObject*)arg0->extra;
        dst        = (TmdObject*)eff->task->extra;
        dst->tpage = src->tpage;
        dst->clut  = src->clut;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
    D_800678F0[0] = D_actor_400600_80143604;
    eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[2], 0x200, NULL);
    if (eff2 != NULL) {
        src2        = (TmdObject*)arg0->extra;
        dst2        = (TmdObject*)eff2->task->extra;
        dst2->tpage = src2->tpage;
        dst2->clut  = src2->clut;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    D_800678F0[0] = D_actor_400600_80143B24;
    eff3          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[16], 0x200, NULL);
    if (eff3 != NULL) {
        src3        = (TmdObject*)arg0->extra;
        dst3        = (TmdObject*)eff3->task->extra;
        dst3->tpage = src3->tpage;
        dst3->clut  = src3->clut;
        if (dst3->buffer != NULL) {
            tmdProcessStream(dst3);
            tmdProcessStream(dst3);
        }
    }
    D_800678F0[0] = D_actor_400600_80144994;
    eff4          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[10], 0x200, NULL);
    if (eff4 != NULL) {
        src4        = (TmdObject*)arg0->extra;
        dst4        = (TmdObject*)eff4->task->extra;
        dst4->tpage = src4->tpage;
        dst4->clut  = src4->clut;
        if (dst4->buffer != NULL) {
            tmdProcessStream(dst4);
            tmdProcessStream(dst4);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[2], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[3], 0x200, NULL);
}

/* `n` is one variable carrying first `out.vz` and then the speed: the reuse is
 * an anti-dependence that keeps the `field_4` store ahead of `li 10` in sched1,
 * so the tail matches case 2's and jump2 cross-jumps them. Each branch keeps
 * its own matrix pointer so local-alloc puts it in `$s0` ahead of `work`. */
void func_actor_400600_80137498(Task* arg0, s16 arg1)
{
    Actor400600Work*        work = (Actor400600Work*)arg0->work;
    SVECTOR                 v;
    SVECTOR                 out;
    ActorsShared8016a538Mat rot;
    s16                     n;

    work->field_76A = arg1;
    switch (arg1) {
        case 0:
            if (work->field_768 == 0) {
                ActorsShared8016a538Mat* m = &rot;

                v.vx              = work->field_A8.x - ((TmdObject*)arg0->extra)->coords->coord.t[0];
                v.vy              = work->field_A8.y - ((TmdObject*)arg0->extra)->coords->coord.t[1] - 0x384;
                v.vz              = work->field_A8.z - ((TmdObject*)arg0->extra)->coords->coord.t[2];
                rot.ident.m00_m01 = 0x1000;
                rot.ident.m02_m10 = 0;
                m->ident.m11_m12  = 0x1000;
                rot.ident.m20_m21 = 0;
                m->ident.m22      = 0x1000;
                rot.mat.t[0]      = 0;
                rot.mat.t[1]      = 0;
                rot.mat.t[2]      = 0;
                func_8004BFF8(-(s16)work->field_82, &m->mat);
                ApplyMatrixSV(&m->mat, &v, &out);
            } else {
                ActorsShared8016a538Mat* m = &rot;

                v.vx              = work->field_A8.x - ((TmdObject*)arg0->extra)->coords->coord.t[0];
                v.vy              = work->field_A8.y - ((TmdObject*)arg0->extra)->coords->coord.t[1] - 0x640;
                v.vz              = work->field_A8.z - ((TmdObject*)arg0->extra)->coords->coord.t[2];
                rot.ident.m00_m01 = 0x1000;
                rot.ident.m02_m10 = 0;
                m->ident.m11_m12  = 0x1000;
                rot.ident.m20_m21 = 0;
                m->ident.m22      = 0x1000;
                rot.mat.t[0]      = 0;
                rot.mat.t[1]      = 0;
                rot.mat.t[2]      = 0;
                func_8004BFF8(-(s16)work->field_82, &m->mat);
                RotMatrixZ(-(s16)work->field_84, &m->mat);
                ApplyMatrixSV(&m->mat, &v, &out);
            }
            work->rec_624.end0.vx    = out.vx;
            work->rec_624.end0.vy    = out.vy;
            n                        = out.vz;
            work->rec_624.end0.vz    = n;
            n                        = 0xA;
            work->rec_624.end0Radius = n;
            work->rec_624.end1Radius = n;
            break;
        case 1:
            work->rec_624.end0.vx    = 0;
            work->rec_624.end0.vy    = 0x190;
            work->rec_624.end0.vz    = -0xBB8;
            work->rec_624.end0Radius = 0x50;
            work->rec_624.end1Radius = 0x50;
            break;
        case 2:
            work->rec_624.end0.vx    = 0;
            work->rec_624.end0.vy    = -0xBB8;
            work->rec_624.end0.vz    = 0;
            work->rec_624.end0Radius = 0xA;
            work->rec_624.end1Radius = 0xA;
            break;
    }
    work->rec_624.end1.vx = 0;
    work->rec_624.end1.vy = 0x64;
    work->rec_624.end1.vz = 0;
    Gp_ClearRec18Occupied(work->rec_63C);
    work->obj_604.flags |= 0x4000;
}

s32 func_actor_400600_801376EC(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          v;
    s16              dist;
    s32              i;

    dist  = 0;
    work  = (Actor400600Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    for (i = 0; i < 8; i++) {
        if ((work->rec_63C[i].key & 0xFFFF0000) != 0x100000) {
            dist = 0;
        } else {
            if (work->field_76A == 0) {
                dist = 1;
            } else if (work->field_76A == 1) {
                v.vx = work->rec_63C[i].point.vx - coord->workm.t[0];
                v.vy = 0;
                v.vz = work->rec_63C[i].point.vz - coord->workm.t[2];
                dist = SquareRoot0(v.vx * v.vx + v.vz * v.vz);
                if (dist == 0) {
                    dist = 1;
                }
            } else if (work->field_76A == 2) {
                v.vx = work->rec_63C[i].point.vx - coord->workm.t[0];
                v.vy = work->rec_63C[i].point.vy - coord->workm.t[1];
                v.vz = 0;
                dist = SquareRoot0(v.vx * v.vx + v.vy * v.vy);
                if (dist == 0) {
                    dist = 1;
                }
            }
            break;
        }
    }
    Gp_ClearRec18Occupied(work->rec_63C);
    return dist;
}

void func_actor_400600_80137840(Task* arg0)
{
    Actor400600Work*         work;
    Actor400600Work*         work2;
    Actor400600Work*         work3;
    Actor400600Work*         work4;
    Actor400600Work*         work5;
    Task*                    child;
    Task*                    child2;
    Task*                    child3;
    Task*                    child4;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           coord2;
    GsCOORDINATE2*           coord3;
    GsCOORDINATE2*           coord4;
    ActorsShared8016a538Mat  rot;
    ActorsShared8016a538Mat* m1;
    ActorsShared8016a538Mat* m2;
    ActorsShared8016a538Mat* m3;
    ActorsShared8016a538Mat* m4;
    MATRIX*                  dst;
    MATRIX*                  dst2;
    MATRIX*                  dst3;
    MATRIX*                  dst4;
    s16                      angle1;
    s32                      angle2;
    s16                      angle3;
    s16                      angle4;

    work = (Actor400600Work*)arg0->work;
    if ((u8)work->field_765 != 0) {
        work->field_74E += (0x380 - work->field_74E) >> 2;
        angle1           = work->field_74E;
        work2            = (Actor400600Work*)arg0->work;
        child            = work2->field_708;
        if (child != NULL) {
            coord             = ((TmdObject*)child->extra)->coords;
            rot.ident.m00_m01 = 0x1000;
            rot.ident.m02_m10 = 0;
            m1                = &rot;
            m1->ident.m11_m12 = 0x1000;
            rot.ident.m20_m21 = 0;
            m1->ident.m22     = 0x1000;
            func_8004BFF8(angle1, &m1->mat);
            dst          = &coord->coord;
            dst->m[0][0] = rot.mat.m[0][0];
            dst->m[0][1] = rot.mat.m[0][1];
            dst->m[0][2] = rot.mat.m[0][2];
            dst->m[1][0] = rot.mat.m[1][0];
            dst->m[1][1] = rot.mat.m[1][1];
            dst->m[1][2] = rot.mat.m[1][2];
            dst->m[2][0] = rot.mat.m[2][0];
            dst->m[2][1] = rot.mat.m[2][1];
            dst->m[2][2] = rot.mat.m[2][2];
        }
    } else {
        work->obj_594.flags &= 0x7FFF;
        work->field_74E     += -work->field_74E >> 3;
        angle2               = work->field_74E;
        TOUCH_REG_USE(arg0, angle2);
        work3  = (Actor400600Work*)arg0->work;
        child2 = work3->field_708;
        if (child2 != NULL) {
            coord2            = ((TmdObject*)child2->extra)->coords;
            rot.ident.m00_m01 = 0x1000;
            rot.ident.m02_m10 = 0;
            m2                = &rot;
            m2->ident.m11_m12 = 0x1000;
            rot.ident.m20_m21 = 0;
            m2->ident.m22     = 0x1000;
            func_8004BFF8(angle2, &m2->mat);
            dst2          = &coord2->coord;
            dst2->m[0][0] = rot.mat.m[0][0];
            dst2->m[0][1] = rot.mat.m[0][1];
            dst2->m[0][2] = rot.mat.m[0][2];
            dst2->m[1][0] = rot.mat.m[1][0];
            dst2->m[1][1] = rot.mat.m[1][1];
            dst2->m[1][2] = rot.mat.m[1][2];
            dst2->m[2][0] = rot.mat.m[2][0];
            dst2->m[2][1] = rot.mat.m[2][1];
            dst2->m[2][2] = rot.mat.m[2][2];
        }
    }
    if ((u8)work->field_766 != 0) {
        work->field_74C += (0x380 - work->field_74C) >> 2;
        angle3           = work->field_74C;
        work4            = (Actor400600Work*)arg0->work;
        child3           = work4->field_704;
        if (child3 != NULL) {
            coord3            = ((TmdObject*)child3->extra)->coords;
            rot.ident.m00_m01 = 0x1000;
            rot.ident.m02_m10 = 0;
            m3                = &rot;
            m3->ident.m11_m12 = 0x1000;
            rot.ident.m20_m21 = 0;
            m3->ident.m22     = 0x1000;
            func_8004BFF8((s16)-angle3, &m3->mat);
            dst3          = &coord3->coord;
            dst3->m[0][0] = rot.mat.m[0][0];
            dst3->m[0][1] = rot.mat.m[0][1];
            dst3->m[0][2] = rot.mat.m[0][2];
            dst3->m[1][0] = rot.mat.m[1][0];
            dst3->m[1][1] = rot.mat.m[1][1];
            dst3->m[1][2] = rot.mat.m[1][2];
            dst3->m[2][0] = rot.mat.m[2][0];
            dst3->m[2][1] = rot.mat.m[2][1];
            dst3->m[2][2] = rot.mat.m[2][2];
        }
    } else {
        work->obj_5CC.flags &= 0x7FFF;
        work->field_74C     += -work->field_74C >> 3;
        angle4               = work->field_74C;
        work5                = (Actor400600Work*)arg0->work;
        child4               = work5->field_704;
        if (child4 != NULL) {
            coord4            = ((TmdObject*)child4->extra)->coords;
            rot.ident.m00_m01 = 0x1000;
            rot.ident.m02_m10 = 0;
            m4                = &rot;
            m4->ident.m11_m12 = 0x1000;
            rot.ident.m20_m21 = 0;
            m4->ident.m22     = 0x1000;
            func_8004BFF8((s16)-angle4, &m4->mat);
            dst4          = &coord4->coord;
            dst4->m[0][0] = rot.mat.m[0][0];
            dst4->m[0][1] = rot.mat.m[0][1];
            dst4->m[0][2] = rot.mat.m[0][2];
            dst4->m[1][0] = rot.mat.m[1][0];
            dst4->m[1][1] = rot.mat.m[1][1];
            dst4->m[1][2] = rot.mat.m[1][2];
            dst4->m[2][0] = rot.mat.m[2][0];
            dst4->m[2][1] = rot.mat.m[2][1];
            dst4->m[2][2] = rot.mat.m[2][2];
        }
    }
    if ((u32)(work->field_71C - 6) >= 2U) {
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
    }
}

s32 func_actor_400600_80137AF0(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    TmdObject*       model2;
    GpEnemy*         enemy;

    work = (Actor400600Work*)arg0->work;
    if (work->field_758 > 0) {
        work->field_758--;
        return 0;
    }
    if ((u32)(work->field_72C - 0x400) >= 0x801U && (u32)(work->field_72A - 0x300) >= 0xA01U) {
        if (work->field_728 < 0xBB8) {
            model = (TmdObject*)arg0->extra;
            if (work->field_75C.b.field_75E != 1) {
                work->field_75C.b.field_75E = 1;
                work->field_75C.b.field_75F = 1;
                work->field_740             = 0;
                model->flags               |= 2;
                Gp_SetLightMode(arg0->spawnArg2, 2);
                func_actor_400600_801387DC(arg0, 2);
            }
            work2            = (Actor400600Work*)arg0->work;
            work2->field_71C = 0x11;
            work2->field_71E = 0;
            return 1;
        }
    } else {
        work = (Actor400600Work*)arg0->work;
    }
    model2 = (TmdObject*)arg0->extra;
    enemy  = (GpEnemy*)arg0->spawnArg2;
    if (work->field_75C.b.field_75E != 0) {
        work->field_75C.b.field_75E = 0;
        work->field_75C.b.field_75F = 1;
        work->field_740             = 0;
        model2->flags               = (model2->flags | 2) & 0xFF7F;
        Gp_SetLightMode(arg0->spawnArg2, 0);
        enemy->node.flags = 4;
        func_actor_400600_801387DC(arg0, 0);
    }
    return 0;
}

s32 func_actor_400600_80137C34(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    GsCOORDINATE2*   coord;
    u32              rnd1;
    u32              rnd2;
    u32              rnd;
    s32              dist;
    s16              y;

    work        = (Actor400600Work*)arg0->work;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    coord       = ((TmdObject*)arg0->extra)->coords;
    rnd         = (u32)Gp_LcgState >> 0x10;
    switch (work->field_76D) {
        case 0:
            if (work->field_732 != 0) {
                return 0;
            }
            if (work->field_768 == 0) {
                if ((rnd & 0xF) == 0) {
                    if (!(arg0->spawnArg1 & 1) && work->field_710.h.timer == 0) {
                        func_actor_400600_80137498(arg0, 2);
                        work->field_76D = 1;
                    }
                    return 0;
                }
                if ((rnd & 7) == 1 || (rnd & 7) == 2) {
                    if (work->field_728 < 0x5DC && (u32)(work->field_72C - 0x200) >= 0xC01U) {
                        work2            = (Actor400600Work*)arg0->work;
                        work2->field_71C = 8;
                        work2->field_71E = 0;
                        return 1;
                    }
                } else if (work->field_728 < 0x5DC) {
                    if ((s16)work->field_72C < 0x400) {
                        work2            = (Actor400600Work*)arg0->work;
                        work2->field_71C = 6;
                        work2->field_71E = 0;
                        return 1;
                    }
                    if ((s16)work->field_72C > 0xC00) {
                        work2            = (Actor400600Work*)arg0->work;
                        work2->field_71C = 7;
                        work2->field_71E = 0;
                        return 1;
                    }
                }
            } else if ((rnd & 7) == 0) {
                if (work->field_710.h.timer == 0) {
                    work2            = (Actor400600Work*)arg0->work;
                    work2->field_71C = 0x10;
                    work2->field_71E = 0;
                    return 1;
                }
            } else if ((rnd & 0xF) == 1) {
                if (work->field_710.h.timer == 0) {
                    work2            = (Actor400600Work*)arg0->work;
                    work2->field_71C = 0xD;
                    work2->field_71E = 0;
                    return 1;
                }
            } else {
                rnd1                                      = ((u32)Gp_LcgState * 5) + 0x71357911;
                rnd2                                      = (rnd1 * 5) + 0x71357911;
                Gp_LcgState                               = rnd2;
                ((Actor400600Work*)arg0->work)->field_732 = 0x3C + ((rnd1 >> 0x10) & 0x3F) + ((rnd2 >> 0x10) & 0xF);
                return 0;
            }
            return 0;
        case 1:
            work->field_76D = 0;
            dist            = func_actor_400600_801376EC(arg0);
            if ((u16)(dist - 0x7D1) < 0x3E8) {
                if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x4080000) {
                    y = -0x9C4;
                } else {
                    y = coord->coord.t[1] - dist;
                }
                work->field_9A                                 = y;
                work3                                          = (Actor400600Work*)arg0->work;
                work3->field_71C                               = 0xC;
                work3->field_71E                               = 0;
                ((Actor400600Work*)arg0->work)->obj_604.flags &= 0xBFFF;
                return 1;
            }
            ((Actor400600Work*)arg0->work)->obj_604.flags &= 0xBFFF;
            return 0;
    }
    return 0;
}

const TaskFuncTable3 D_actor_400600_80132030 = { {
    func_actor_400600_8013C874,
    func_actor_400600_8013C940,
    func_actor_400600_8013C9DC,
} };

void func_actor_400600_80137EF0(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    TaskFuncTable3   fns   = D_actor_400600_80132030;

    switch (D_801153F4) {
        case 2:
            model->flags |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            Actor400600_TickAnim(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80136558(arg0);
            Actor400600_RebuildRotation(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            Actor400600_UpdateColor(arg0);
            func_actor_400600_80138224(arg0, 0, work->field_73A);
            break;
    }
}

/// Refreshes the parts listed in `D_actor_400600_80151B88`, projects each into
/// view space with `arg1` as the Y, and passes nine fixed pairs of the resulting
/// points to `func_actor_400600_801383E4` along with `arg2` (the fade level at
/// every call site).
void func_actor_400600_80138224(Task* arg0, s16 arg1, u8 arg2)
{
    MATRIX         mtx;
    SVECTOR        pts[11];
    GsCOORDINATE2* coord;
    GsCOORDINATE2* root;
    s32            i;

    root              = ((TmdObject*)arg0->extra)->coords;
    gGfxViewCoord.flg = 0;
    root->flg         = 0;
    for (i = 0; D_actor_400600_80151B88[i] != -1; i++) {
        coord      = &((TmdObject*)arg0->extra)->coords[D_actor_400600_80151B88[i]];
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &mtx);
        pts[i].vx = mtx.t[0];
        pts[i].vy = arg1;
        pts[i].vz = mtx.t[2];
    }
    func_actor_400600_801383E4(&pts[1], &pts[5], 0x80, arg2);
    func_actor_400600_801383E4(&pts[5], &pts[6], 0x80, arg2);
    func_actor_400600_801383E4(&pts[1], &pts[3], 0x80, arg2);
    func_actor_400600_801383E4(&pts[3], &pts[4], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[2], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[7], 0x80, arg2);
    func_actor_400600_801383E4(&pts[7], &pts[8], 0x80, arg2);
    func_actor_400600_801383E4(&pts[0], &pts[9], 0x80, arg2);
    func_actor_400600_801383E4(&pts[9], &pts[10], 0x80, arg2);
}

/// Draws a semi-transparent textured quad along the segment from `arg0` to
/// `arg1`: widened by `width` either side, pulled in by half its length at both
/// ends, and shaded grey `shade`. The per-model counterpart of
/// `ActorsShared80163354`, taking view-space points instead of joints.
void func_actor_400600_801383E4(SVECTOR* arg0, SVECTOR* arg1, s16 width, u8 shade)
{
    Actor400600QuadScratch* s;
    s16                     angle;
    s32                     halfX;
    s32                     halfZ;
    POLY_FT4*               poly;

    gGfxViewCoord.flg = 0;
    s                 = (Actor400600QuadScratch*)(*(u8**)G_SCRATCH_HEAD -= sizeof(Actor400600QuadScratch));
    Gp_UpdateCoord(&gGfxViewCoord);
    angle         = ratan2(arg1->vx - arg0->vx, arg1->vz - arg0->vz);
    halfX         = (arg0->vx - arg1->vx) / 2;
    halfZ         = (arg0->vz - arg1->vz) / 2;
    s->corner0.vx = halfX + (arg0->vx - ((s32)(rcos(angle) * width) >> 0xC));
    s->corner0.vy = arg0->vy;
    s->corner0.vz = halfZ + (arg0->vz + ((s32)(rsin(angle) * width) >> 0xC));
    s->corner1.vx = halfX + (arg0->vx + ((s32)(rcos(angle) * width) >> 0xC));
    s->corner1.vy = arg0->vy;
    s->corner1.vz = halfZ + (arg0->vz - ((s32)(rsin(angle) * width) >> 0xC));
    s->corner2.vx = (arg1->vx - ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
    s->corner2.vy = arg1->vy;
    s->corner2.vz = (arg1->vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
    s->corner3.vx = (arg1->vx + ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
    s->corner3.vy = arg1->vy;
    s->corner3.vz = (arg1->vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                             &s->screen2, &s->screen3, &s->perspective, &s->flags);
    if (s->flags >= 0) {
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)poly + 0x28;
        setlen(poly, 9);
        poly->code       = 0x2E;
        *(s32*)&poly->x0 = s->screen0;
        *(s32*)&poly->x1 = s->screen1;
        *(s32*)&poly->x2 = s->screen2;
        *(s32*)&poly->x3 = s->screen3;
        setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
        poly->tpage = 0x48;
        poly->clut  = 0x4283;
        setRGB0(poly, shade, shade, shade);
        addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor400600QuadScratch);
}

/// Copies this actor's model flags onto both child tasks' models and, for a
/// non-negative `arg1`, sets the children's light mode to it.
void func_actor_400600_801387DC(Task* arg0, s32 arg1)
{
    Actor400600Work* work;
    TmdObject*       model;
    Task*            child;

    work  = (Actor400600Work*)arg0->work;
    model = (TmdObject*)arg0->extra;
    if (work->field_704 != NULL) {
        child                             = work->field_704;
        ((TmdObject*)child->extra)->flags = model->flags;
        if (arg1 >= 0) {
            Gp_SetLightMode(child->spawnArg2, arg1);
        }
    }
    if (work->field_708 != NULL) {
        child                             = work->field_708;
        ((TmdObject*)child->extra)->flags = model->flags;
        if (arg1 >= 0) {
            Gp_SetLightMode(child->spawnArg2, arg1);
        }
    }
}

/// Returns the id of the first `D_actor_400600_80151B40` zone containing the
/// actor's world XZ position, or 0 if none does.
s32 func_actor_400600_8013886C(Task* arg0)
{
    GpCoordXZ*       coord;
    Actor400600Zone* zone;
    s16              x;
    s16              z;

    coord = (GpCoordXZ*)((TmdObject*)arg0->extra)->coords;
    x     = coord->field_18;
    z     = coord->field_20;
    for (zone = D_actor_400600_80151B40; zone->id != -1; zone++) {
        if (zone->x <= x && x <= zone->x + zone->w && zone->z <= z && z <= zone->z + zone->h) {
            return zone->id;
        }
    }
    return 0;
}

s32 func_actor_400600_8013892C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if ((work->field_710.h.flags & 1) || (work->field_710.word & 0x01020000)) {
        return 1;
    }
    return 0;
}

void func_actor_400600_8013896C(Task* arg0, s16 arg1)
{
    GsCOORDINATE2* coord;
    SVECTOR        vec;
    s32            i;

    coord = ((TmdObject*)arg0->extra)->coords;
    Gp_SpawnEff(D_8011574C, coord, 0x40, NULL);
    for (i = 0; i < 16; i++) {
        vec.vx = (u32)rsin(i << 8) >> 3;
        vec.vy = arg1;
        vec.vz = (u32)rcos(i << 8) >> 3;
        Gp_SpawnEff(D_80115738, coord, 0x01202148, &vec);
    }
}

void func_actor_400600_80138A24(Task* arg0, s16 arg1)
{
    Actor400600Work* work;
    TmdObject*       model;
    GpEnemy*         enemy;

    work  = (Actor400600Work*)arg0->work;
    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    if (arg1 != 0) {
        enemy->node.flags = 5;
        model->flags     |= 0x80;
        Gp_SetLightMode(arg0->spawnArg2, 2);
        func_actor_400600_801387DC(arg0, 2);
        work->field_75C.b.field_75E = 1;
        work->field_740             = 0;
        work->field_75C.b.field_75F = 0;
        work->field_73A             = 0;
    }
}

void func_actor_400600_80138AA4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_766 = 0;
    work->field_765 = 0;
}

void func_actor_400600_80138AB8(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if (work->field_732 > 0) {
        work->field_732 = (u16)work->field_732 - 1;
    }
    if (work->field_710.h.timer > 0) {
        work->field_710.h.timer = (u16)work->field_710.h.timer - 1;
    }
}

void func_actor_400600_80138AF0(Task* arg0, s32 arg1)
{
    u32 rnd1;
    u32 rnd2;

    rnd1                                      = ((u32)Gp_LcgState * 5) + 0x71357911;
    rnd2                                      = (rnd1 * 5) + 0x71357911;
    Gp_LcgState                               = rnd2;
    ((Actor400600Work*)arg0->work)->field_732 = arg1 + ((rnd1 >> 0x10) & 0x3F) + ((rnd2 >> 0x10) & 0xF);
}

void func_actor_400600_80138B40(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->obj_604.flags &= 0xBFFF;
}

void func_actor_400600_80138B5C(Task* arg0, s32 arg1)
{
    Actor400600Work* work;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400600Work*)arg0->work;
    if (!(arg1 & 0xFF)) {
        if (work->field_75C.b.field_75E != 0) {
            work->field_75C.b.field_75E = 0;
            work->field_75C.b.field_75F = 1;
            work->field_740             = 0;
            model->flags                = (model->flags | 2) & 0xFF7F;
            Gp_SetLightMode(arg0->spawnArg2, 0);
            enemy->node.flags = 4;
            func_actor_400600_801387DC(arg0, 0);
        }
    } else if (work->field_75C.b.field_75E != 1) {
        work->field_75C.b.field_75E = 1;
        work->field_75C.b.field_75F = 1;
        work->field_740             = 0;
        model->flags               |= 2;
        Gp_SetLightMode(arg0->spawnArg2, 2);
        func_actor_400600_801387DC(arg0, 2);
    }
}

void func_actor_400600_80138C34(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    TaskFuncTable8   fns   = D_actor_400600_80131E7C;

    switch (D_801153F4) {
        case 2:
            model->flags |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            func_actor_400600_80139CAC(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80136558(arg0);
            func_actor_400600_80139948(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            func_actor_400600_8013A2C0(arg0);
            func_actor_400600_80132704(arg0, work->field_73C, work->field_73A);
            break;
    }
}

void func_actor_400600_80138D78(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    TaskFuncTable4   fns   = D_actor_400600_80131E9C;

    switch (D_801153F4) {
        case 2:
            model->flags |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            func_actor_400600_80139CAC(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80136558(arg0);
            func_actor_400600_80139948(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            func_actor_400600_8013A2C0(arg0);
            func_actor_400600_80138224(arg0, 0, work->field_73A);
            break;
    }
}

void func_actor_400600_80138EA0(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    TaskFuncTable6   fns   = D_actor_400600_80131E54;

    switch (D_801153F4) {
        case 2:
            model->flags |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80139CAC(arg0);
            func_actor_400600_80136558(arg0);
            func_actor_400600_80139948(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            func_actor_400600_8013A2C0(arg0);
            func_actor_400600_80138224(arg0, 0, (u8)work->field_73A);
            break;
    }
}

void func_actor_400600_80138FD4(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    TaskFuncTable4   fns   = D_actor_400600_80131E6C;

    switch (D_801153F4) {
        case 2:
            model->flags |= 0x80;
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80139CAC(arg0);
            func_actor_400600_80136558(arg0);
            func_actor_400600_80139948(arg0);
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            func_actor_400600_8013A2C0(arg0);
            func_actor_400600_80138224(arg0, 0, work->field_73A);
            break;
    }
}

void func_actor_400600_801390FC(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_71C = 1;
    work->field_71E = 0;
}

void func_actor_400600_80139110(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013B6F4, func_actor_400600_8013B740 };

    func_actor_400600_80138AA4(arg0);
    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[(s16)work->field_71E](arg0);
        if ((s16)func_actor_400600_80137C34(arg0) == 0 && (s16)func_actor_400600_80137AF0(arg0) == 0 && (*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x4080000 && work->field_768 != 0 && ((TmdObject*)arg0->extra)->coords->coord.t[0] > 10000) {
            Actor400600Work* cur = (Actor400600Work*)arg0->work;

            cur->field_71C = 0xD;
            cur->field_71E = 0;
        }
    }
}

void func_actor_400600_80139218(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013B830, func_actor_400600_8013B8AC };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

void func_actor_400600_80139280(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013B984, func_actor_400600_8013BA00 };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

void func_actor_400600_801392E8(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    TaskFuncTable3   fns  = D_actor_400600_80131F34;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_8013935C(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013BBF4, func_actor_400600_80133CB0 };

    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[(s16)work->field_71E](arg0);
    }
}

void func_actor_400600_801393D0(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013BC68, func_actor_400600_80133E38 };

    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        fns[(s16)work->field_71E](arg0);
    }
}

void func_actor_400600_80139444(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    TaskFuncTable8   fns  = D_actor_400600_80131F40;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_801394E0(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    TaskFuncTable4   fns  = D_actor_400600_80131F60;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_80139560(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    SVECTOR          pos;
    s16              count;

    work = (Actor400600Work*)arg0->work;
    func_actor_400600_80138AA4(arg0);
    if ((s16)func_actor_400600_80136FA8(arg0) == 0) {
        count           = work->field_750 - 1;
        work->field_750 = count;
        if (count == 0) {
            work2            = (Actor400600Work*)arg0->work;
            work2->field_71C = 0xB;
            work2->field_71E = 0;
            return;
        }
        pos.vx = work->field_A8.x;
        pos.vy = work->field_A8.y;
        pos.vz = work->field_A8.z;
        func_actor_400600_80139C00(arg0, &pos, 0x18);
        func_actor_400600_80135DDC(arg0);
    }
}

void func_actor_400600_80139608(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013BFD4, func_actor_400600_80134B98 };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

void func_actor_400600_80139670(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    TaskFuncTable3   fns  = D_actor_400600_80131F70;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_801396E4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    TaskFuncTable4   fns  = D_actor_400600_80131F7C;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_80139764(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    TaskFuncTable4   fns  = D_actor_400600_80131F8C;

    func_actor_400600_80138AA4(arg0);
    fns.funcs[(s16)work->field_71E](arg0);
}

void func_actor_400600_801397E4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    TaskFuncTable3   fns  = D_actor_400600_80131F9C;

    func_actor_400600_80138AA4(arg0);
    if ((func_actor_400600_8013CACC(arg0) << 0x10) == 0) {
        fns.funcs[(s16)work->field_71E](arg0);
    }
}

void func_actor_400600_80139878(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013C518, func_actor_400600_8013C534 };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

void func_actor_400600_801398E0(Task* arg0)
{
    Actor400600Work* work             = (Actor400600Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400600_8013C598, func_actor_400600_8013C5F8 };

    func_actor_400600_80138AA4(arg0);
    fns[(s16)work->field_71E](arg0);
}

/// Wraps the actor's pitch, yaw and roll (`field_80`, `field_82`, `field_84`)
/// to 12 bits and rebuilds the model root's rotation from them: an identity
/// matrix taken off `G_SCRATCH_HEAD` is turned by roll, pitch and then yaw,
/// and its 3x3 copied into the root coordinate, whose translation is left
/// alone.
void func_actor_400600_80139948(Task* arg0)
{
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->coords;
    MATRIX*          m;
    MATRIX*          dst;

    work->field_80           &= 0xFFF;
    work->field_82           &= 0xFFF;
    work->field_84           &= 0xFFF;
    m                         = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    RotMatrixZ((s16)work->field_84, m);
    RotMatrixX((s16)work->field_80, m);
    func_8004BFF8((s16)work->field_82, m);
    dst                   = &coord->coord;
    dst->m[0][0]          = m->m[0][0];
    dst->m[0][1]          = m->m[0][1];
    dst->m[0][2]          = m->m[0][2];
    dst->m[1][0]          = m->m[1][0];
    dst->m[1][1]          = m->m[1][1];
    dst->m[1][2]          = m->m[1][2];
    dst->m[2][0]          = m->m[2][0];
    dst->m[2][1]          = m->m[2][1];
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
    dst->m[2][2]          = m->m[2][2];
}

void func_actor_400600_80139A78(Task* arg0)
{
    Actor400600Work* work;
    s32              i;

    work = (Actor400600Work*)arg0->work;
    i    = 1;
    do {
        work->slots[i].rate = work->field_726;
        Gp_AnimResetSlot(&work->anim, i, work->field_746);
        i++;
    } while (i < 0x12);
    work->field_744 = work->field_746;
}

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_400600_80139AE8(Task* arg0)
{
    Actor400600Work* work;
    s32              i;

    work = (Actor400600Work*)arg0->work;
    if (work->field_744 == work->field_746) {
        i = 1;
        do {
            work->slots[i].rate = work->field_726;
            i++;
        } while (i < 0x12);
    } else {
        i = 1;
        do {
            work->slots[i].rate = work->field_726;
            func_800B4114(&work->anim, i, work->field_746, 0, work->field_720);
            i++;
        } while (i < 0x12);
        work->field_720 = 0;
    }
    work->field_744 = work->field_746;
}

s16 func_actor_400600_80139BA0(Task* arg0, s16 arg1)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if (work->field_726 == 0) {
        return 0;
    }
    return ((arg1 << 8) / work->field_726 << 12) >> 16;
}

/// Turns the actor's yaw (`field_82`) by `step` toward the world point
/// `target`, of which only `vx` and `vz` are read, leaving it alone while the
/// heading error is within 0x100. Clears the model root's `flg` first so the
/// root is recomputed.
void func_actor_400600_80139C00(Task* arg0, SVECTOR* target, s32 step)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    GsCOORDINATE2*   coords;
    SVECTOR          vec;
    s32              diff;
    s32              yaw;
    u16              angle;

    coords      = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    coords->flg = 0;
    vec.vx      = target->vx - coords->coord.t[0];
    vec.vy      = 0;
    vec.vz      = target->vz - coords->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    angle = work->field_82;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > 0x100) {
        work->field_82 = angle - step;
    } else if (diff < -0x100) {
        work->field_82 = angle + step;
    }
}

void func_actor_400600_80139CAC(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    s32              i;

    if (work->field_742 == 1) {
        if (work->field_744 != work->field_746) {
            work->field_748 = 0;
        } else {
            work->field_748 = func_actor_400600_80139BA0(arg0, work->field_748);
        }
        func_actor_400600_80139AE8(arg0);
        work->field_742 = 3;
    } else if (work->field_742 == 2) {
        func_actor_400600_80139A78(arg0);
        work->field_742 = 3;
        work->field_748 = 0;
    } else if (work->field_742 == 3) {
        work->field_748++;
    }
    i = 1;
    do {
        work->slots[i].rate = work->field_726;
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x12);
}

void func_actor_400600_80139D98(Task* arg0, s16 arg1, s16 arg2)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_726 = arg2;
    work->field_746 = arg1;
    work->field_742 = 2;
}

void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_720 = arg3;
    work->field_726 = arg2;
    work->field_746 = arg1;
    work->field_742 = 1;
}

/// Refreshes the view coordinate and coordinate `index` of the actor's model,
/// then stores that coordinate's view-space X and Z translation to `out`; `y`
/// is left untouched. Every caller passes the work block's `field_88`.
void func_actor_400600_80139DCC(Task* task, s16 index, Actor400600ViewPos* out)
{
    MATRIX         local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coords;

    coords            = (GsCOORDINATE2*)((TmdObject*)task->extra)->coords;
    gGfxViewCoord.flg = 0;
    coord             = &coords[index];
    Gp_UpdateCoord(&gGfxViewCoord);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &local);
    out->x     = local.t[0];
    out->z     = local.t[2];
    coord->flg = 0;
}

void func_actor_400600_80139E68(Task* arg0, s16 arg1, Actor400600ViewPos* arg2)
{
    MATRIX         root;
    MATRIX         local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coords;

    coords            = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    gGfxViewCoord.flg = 0;
    coord             = &coords[arg1];
    Gp_UpdateCoord(&gGfxViewCoord);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coords[0].workm, &root);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &local);
    coords[0].coord.t[0] = arg2->x - (local.t[0] - root.t[0]);
    coords[0].coord.t[2] = arg2->z - (local.t[2] - root.t[2]);
    coords[0].flg        = 0;
    coord->flg           = 0;
    Gp_UpdateCoord(coord);
    Gp_UpdateCoord(coords);
}

void func_actor_400600_80139F4C(Task* arg0, s16 arg1, Actor400600ViewPos* arg2)
{
    MATRIX         local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coords;

    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    coord  = &coords[arg1];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &local);
    arg2->x    = local.t[0];
    arg2->y    = local.t[1];
    arg2->z    = coords[0].coord.t[2];
    coord->flg = 0;
}

void func_actor_400600_80139FE0(Task* arg0, s16 arg1, Actor400600ViewPos* arg2)
{
    MATRIX         root;
    MATRIX         local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coords;

    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    coord  = &coords[arg1];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords[0].workm, &root);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &local);
    coords[0].coord.t[0] = arg2->x - (local.t[0] - root.t[0]);
    coords[0].coord.t[1] = arg2->y - (local.t[1] - root.t[1]);
    coords[0].flg        = 0;
    coord->flg           = 0;
    Gp_UpdateCoord(coord);
    Gp_UpdateCoord(coords);
}

/// Returns 1 when any of bits 0, 1 or 8 of the second animation slot's
/// `slots[1].flags` is set - the walk took the clip's end, followed a control
/// entry, or the clip has settled on its last pose - and 0 otherwise. Bit 0 is
/// read as a halfword and the other two through the word starting there,
/// which is why the work block is seen through `ActorsShared8013a0b0Work`.
s32 func_actor_400600_8013A0B0(Task* arg0)
{
    ActorsShared8013a0b0Work* work = (ActorsShared8013a0b0Work*)arg0->work;

    if ((work->flags_FC.half & 1) || (work->flags_FC.word & 0x102)) {
        return 1;
    }
    return 0;
}

void func_actor_400600_8013A0F0(Task* arg0)
{
    TaskFuncTable9 states = D_actor_400600_80131EAC;

    states.funcs[arg0->state](arg0);
}

void func_actor_400600_8013A170(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    TaskFuncTable12  fns   = D_actor_400600_80131E24;

    switch (D_801153F4) {
        case 2:
            model->flags |= 0x80;
            func_actor_400600_801387DC(arg0, -1);
            break;
        case 0:
            fns.funcs[(s16)work->field_71C](arg0);
        case 1:
            func_actor_400600_8013A2C0(arg0);
            func_actor_400600_80138224(arg0, work->field_73E, work->field_73A);
            break;
    }
}

void func_actor_400600_8013A26C(Task* arg0)
{
    Actor400600Work* work                = (Actor400600Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_400600_8013AAD8,
        func_actor_400600_8013AB44,
    };

    states[(s16)work->field_71C](arg0);
}

/// Colours the actor from its model's second coordinate: takes a 0x10-byte
/// `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's world
/// position and hands it to `Gp_UpdateActorColor` for the task's `spawnArg2`,
/// with no blend parameters.
void func_actor_400600_8013A2C0(Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)task->extra)->coords[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(task->spawnArg2, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

void func_actor_400600_8013A338(Task* arg0, s32 arg1, u16* arg2)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    switch (arg2[1]) {
        case 0:
            work->field_762 = 1;
            break;
        case 1:
            work->field_762 = 2;
            break;
        case 2:
            work->field_762 = 3;
            break;
        case 3:
            work->field_762 = 4;
            break;
    }
}

void func_actor_400600_8013A3A8(Task* arg0)
{
    ((Actor400600Work*)arg0->work)->field_763 = 1;
}

void func_actor_400600_8013A3B8(void)
{
}

void func_actor_400600_8013A3C0(void)
{
}

void func_actor_400600_8013A3C8(Task* arg0)
{
    GpEnemy*         enemy;
    Actor400600Work* work;
    TmdObject*       model;

    enemy                = (GpEnemy*)arg0->spawnArg2;
    work                 = (Actor400600Work*)arg0->work;
    model                = (TmdObject*)arg0->extra;
    work->obj_5CC.flags &= 0x7FFF;
    work->obj_594.flags &= 0x7FFF;
    Gp_UnlinkNode(&enemy->node);
    if (work->field_730 == 4) {
        model->flags |= 0x80;
        func_actor_400600_801387DC(arg0, -1);
        work->field_718 = 0;
        func_actor_400600_8013CC04(arg0, 7);
    } else if (work->field_768 == 0) {
        model->flags &= 0xFF7F;
        Gp_SetLightMode(arg0->spawnArg2, 0);
        func_actor_400600_801387DC(arg0, 0);
        work->field_71C = work->field_71C + 1;
    } else {
        func_actor_400600_8013CC04(arg0, 9);
    }
}

void func_actor_400600_8013A4AC(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    func_actor_400600_80139D98(arg0, D_actor_400600_80151B1C[work->field_746], 0x10);
    func_actor_400600_80139CAC(arg0);
    work->field_71C = work->field_71C + 1;
}

void func_actor_400600_8013A518(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    func_actor_400600_80139CAC(arg0);
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        work->field_71C = work->field_71C + 1;
    }
}

void func_actor_400600_8013A570(Task* arg0)
{
    Actor400600Work* work  = (Actor400600Work*)arg0->work;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->coords;

    ((GpEnemy*)arg0->spawnArg2)->recs = 0;
    Gp_UnlinkObj(&work->obj_4B4);
    Gp_UnlinkObj(&work->obj_594);
    Gp_UnlinkObj(&work->obj_5CC);
    Gp_UnlinkObj(&work->obj_604);
    work->field_714 = 0x1000;
    work->matrix_0  = coord->coord;
    Gp_SetLightMode((GpObj4C*)arg0->spawnArg2, 1);
    work->field_718 = 0;
    work->field_71C++;
}

void func_actor_400600_8013A638(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;
    u16              frame;

    work            = (Actor400600Work*)arg0->work;
    model           = (TmdObject*)arg0->extra;
    frame           = work->field_718 + 1;
    work->field_718 = frame;
    if ((s16)frame >= 0x18) {
        model->flags |= 2;
        Gp_SetLightMode(arg0->spawnArg2, 2);
        func_actor_400600_801387DC(arg0, 2);
        work->field_718 = 0;
        work->field_71C = work->field_71C + 1;
    }
}

void func_actor_400600_8013A6C4(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;
    GsCOORDINATE2*   coord;
    VECTOR           scale;
    SVECTOR          rot;

    work             = (Actor400600Work*)arg0->work;
    model            = (TmdObject*)arg0->extra;
    coord            = model->coords;
    work->field_73A  = (u16)work->field_73A + (-work->field_73A >> 2);
    work->field_714 -= 0x30;
    scale.vx         = 0x1000;
    scale.vy         = work->field_714;
    scale.vz         = 0x1000;
    coord->coord     = work->matrix_0;
    ScaleMatrix(&coord->coord, &scale);
    coord->flg = 0;
    work->field_718++;
    if ((s16)work->field_718 == 8) {
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = 0;
        Gp_SpawnEff(0x600A5, coord, 4, &rot);
    }
    if ((s16)work->field_718 >= 0x11) {
        model->flags |= 0x80;
        func_actor_400600_801387DC(arg0, -1);
        work->field_71C = work->field_71C + 1;
    }
}

void func_actor_400600_8013A808(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    arg0->state     = 3;
    work->field_71C = 0;
    work->field_71E = 0;
}

void func_actor_400600_8013A820(Task* arg0)
{
    Actor400600Work* work;
    u16              frame;

    work            = (Actor400600Work*)arg0->work;
    frame           = work->field_718 + 1;
    work->field_718 = frame;
    if ((s16)frame >= 2) {
        work->field_71C = work->field_71C + 1;
    }
}

void func_actor_400600_8013A864(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = (TmdObject*)arg0->extra;
    work  = (Actor400600Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    Tmd_FreeBuffers(model);
    model->flags |= 4;
    func_actor_400600_80137240(arg0);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->recs = 0;
    Gp_UnlinkObj(&work->obj_4B4);
    Gp_UnlinkObj(&work->obj_594);
    Gp_UnlinkObj(&work->obj_5CC);
    Gp_UnlinkObj(&work->obj_604);
    work2            = (Actor400600Work*)arg0->work;
    arg0->state      = 3;
    work2->field_71C = 0;
    work2->field_71E = 0;
}

void func_actor_400600_8013A908(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    model         = (TmdObject*)arg0->extra;
    work          = (Actor400600Work*)arg0->work;
    model->flags &= 0xFF7F;
    Gp_SetLightMode(arg0->spawnArg2, 0);
    func_actor_400600_80139DB0(arg0, 9, 0x10, 2);
    work->field_722 = 0;
    work->field_724 = 0;
    work->field_73E = work->field_92;
    func_actor_400600_80139CAC(arg0);
    work->field_71C = work->field_71C + 1;
}

void func_actor_400600_8013A990(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;

    work               = (Actor400600Work*)arg0->work;
    coord              = ((TmdObject*)arg0->extra)->coords;
    work->field_722   += 2;
    work->field_724   += work->field_722;
    coord->coord.t[1] += work->field_724;
    if ((s16)work->field_92 < coord->coord.t[1]) {
        coord->coord.t[1] = (s16)work->field_92;
        func_actor_400600_80139D98(arg0, 0x13, 0x10);
        work->field_84 += 0x800;
        func_actor_400600_80139948(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work->field_718 = 0;
        work->field_768 = 0;
        work->field_769 = 1;
        work->field_71C++;
    }
    func_actor_400600_80139CAC(arg0);
}

void func_actor_400600_8013AA5C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if ((s16)work->field_718 == 0) {
        func_actor_400600_8013CB70(arg0, 0x40060006);
        work->field_718 = work->field_718 + 1;
    }
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        func_actor_400600_8013CC04(arg0, 1);
    }
    func_actor_400600_80139CAC(arg0);
}

void func_actor_400600_8013AAD8(Task* arg0)
{
    Actor400600Work* work;
    Task*            child;
    Task*            child2;

    work       = (Actor400600Work*)arg0->work;
    D_80115417 = 1;
    SOFT_BARRIER();
    child = work->field_704;
    if (child != NULL) {
        taskKill(child);
    }
    child2 = work->field_708;
    if (child2 != NULL) {
        taskKill(child2);
    }
    work->field_718 = 0;
    work->field_71C = work->field_71C + 1;
}

void func_actor_400600_8013AB44(Task* arg0)
{
    Actor400600Work* work;
    u16              frame;

    work            = (Actor400600Work*)arg0->work;
    frame           = work->field_718 + 1;
    work->field_718 = frame;
    if ((s16)frame >= 0x97) {
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

void func_actor_400600_8013AB98(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    work            = (Actor400600Work*)arg0->work;
    model           = (TmdObject*)arg0->extra;
    work->field_73A = 0;
    func_actor_400600_80138B5C(arg0, 1);
    work->obj_4B4.flags &= 0x7FFF;
    work->obj_594.flags &= 0x7FFF;
    work->obj_5CC.flags &= 0x7FFF;
    model->flags        |= 0x80;
    work->field_71C      = work->field_71C + 1;
}

void func_actor_400600_8013AC14(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    GsCOORDINATE2*   coord;

    work  = (Actor400600Work*)arg0->work;
    model = (TmdObject*)arg0->extra;
    coord = model->coords;
    if (work->field_762 == 1) {
        func_8017D9B8(0);
        coord->coord.t[0] = 0x40C8;
        coord->coord.t[1] = -0x708;
        coord->coord.t[2] = -0x3E8;
        work->field_80    = 0;
        work->field_82    = 0;
        work->field_84    = 0;
        model->flags     &= 0xFF7F;
        work->field_722   = 0;
        work->field_724   = 0;
        func_actor_400600_80139D98(arg0, 0x15, 0x10);
        func_actor_400600_80138B5C(arg0, 0);
        work->field_71C = work->field_71C + 1;
    } else if (work->field_762 == 3) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        coord->coord.t[0]    = 0x32C2;
        coord->coord.t[2]    = 0x960;
        coord->coord.t[1]    = 0;
        work->field_80       = 0;
        work->field_82       = 0xC00;
        work->field_84       = 0;
        work->field_73A      = 0;
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
    }
}

void func_actor_400600_8013AD3C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        work->field_718 = 0;
        work->field_722 = 0;
        work->field_724 = 0;
        func_actor_400600_80139D98(arg0, 0x22, 0x10);
        work->field_71C = work->field_71C + 1;
    }
}

void func_actor_400600_8013ADA4(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;
    s32              x;
    s32              y;
    u16              frame;
    u16              step;
    u16              accum;

    work            = (Actor400600Work*)arg0->work;
    coord           = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    frame           = work->field_718 + 1;
    work->field_718 = frame;
    if ((s16)frame >= 5) {
        x                 = coord->coord.t[0];
        coord->coord.t[0] = x + ((0x40B5 - x) >> 3);
        coord->coord.t[2] = coord->coord.t[2] + 0x78;
        step              = (u16)work->field_722 + 2;
        accum             = (u16)work->field_724 + step;
        work->field_724   = accum;
        work->field_722   = step;
        y                 = coord->coord.t[1] + (s16)accum;
        coord->coord.t[1] = y;
        if (y >= 0) {
            Gp_SpawnPadLerp(0x10, 0x80, 0x20);
            work->field_718 = 0;
            func_actor_400600_80139D98(arg0, 0x19, 0x10);
            coord->coord.t[1] = 0;
            work->field_71C   = work->field_71C + 1;
        }
    }
}

void func_actor_400600_8013AE88(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    work            = (Actor400600Work*)arg0->work;
    model           = (TmdObject*)arg0->extra;
    work->field_73A = 0;
    func_actor_400600_80138B5C(arg0, 1);
    work->obj_4B4.flags &= 0x7FFF;
    work->obj_594.flags &= 0x7FFF;
    work->obj_5CC.flags &= 0x7FFF;
    model->flags        |= 0x80;
    work->field_71C      = work->field_71C + 1;
}

void func_actor_400600_8013AF04(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    GsCOORDINATE2*   coord;

    work  = (Actor400600Work*)arg0->work;
    model = (TmdObject*)arg0->extra;
    coord = model->coords;
    if (work->field_762 == 2) {
        coord->coord.t[0] = 0x1FDD;
        coord->coord.t[1] = -0xE38;
        coord->coord.t[2] = 0x5CE;
        work->field_80    = 0;
        work->field_82    = 0x400;
        work->field_84    = 0;
        model->flags     &= 0xFF7F;
        work->field_722   = 0;
        work->field_724   = 0;
        func_actor_400600_80139D98(arg0, 0x15, 0x10);
        func_actor_400600_80138B5C(arg0, 0);
        work->field_71C = work->field_71C + 1;
    } else if (work->field_762 == 3) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        coord->coord.t[0]    = 0x640;
        coord->coord.t[2]    = 0x87A;
        coord->coord.t[1]    = 0;
        work->field_80       = 0;
        work->field_82       = 0x400;
        work->field_84       = 0;
        work->field_73A      = 0;
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
    }
}

void func_actor_400600_8013B018(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              soundId;
    s32              pan;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40060004;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
        work3                = (Actor400600Work*)arg0->work;
        work3->field_71C     = 2;
        work3->field_71E     = 0;
    }
}

void func_actor_400600_8013B0FC(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    work                 = (Actor400600Work*)arg0->work;
    model                = (TmdObject*)arg0->extra;
    work->field_73A      = 0;
    work->obj_4B4.flags &= 0x7FFF;
    work->obj_594.flags &= 0x7FFF;
    work->obj_5CC.flags &= 0x7FFF;
    model->flags        |= 0x80;
    work->field_71C      = work->field_71C + 1;
}

void func_actor_400600_8013B150(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        Gp_ArmStateF0(1);
        work2            = (Actor400600Work*)arg0->work;
        arg0->state      = 1;
        work2->field_71C = 0;
        work2->field_71E = 0;
        work3            = (Actor400600Work*)arg0->work;
        work3->field_71C = 2;
        work3->field_71E = 0;
    }
}

void func_actor_400600_8013B1DC(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_718 = work->field_718 + 1;
    func_actor_400600_801361AC(arg0);
    if ((s16)work->field_718 == 0x26) {
        func_actor_400600_80138B5C(arg0, 0);
    }
    if ((s16)work->field_718 >= 0x27) {
        work->field_73A = (u16)work->field_73A + ((0xFF - work->field_73A) >> 4);
    }
    if ((s16)work->field_718 == 0x50) {
        work->field_718 = 0;
        work->field_722 = -0xA;
        work->field_724 = 0;
        func_actor_400600_80139DB0(arg0, 0x15, 0x10, 4);
        work->field_71C = work->field_71C + 1;
    }
}

void func_actor_400600_8013B2A8(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              soundId;
    s32              pan;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x404A0004;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        Gp_ArmStateF0(1);
        work2            = (Actor400600Work*)arg0->work;
        arg0->state      = 1;
        work2->field_71C = 0;
        work2->field_71E = 0;
        work3            = (Actor400600Work*)arg0->work;
        work3->field_71C = 2;
        work3->field_71E = 0;
    }
}

void func_actor_400600_8013B394(Task* arg0)
{
    Actor400600Work* work;
    TmdObject*       model;

    work            = (Actor400600Work*)arg0->work;
    model           = (TmdObject*)arg0->extra;
    work->field_73A = 0;
    func_actor_400600_80138B5C(arg0, 1);
    work->obj_4B4.flags &= 0x7FFF;
    work->obj_594.flags &= 0x7FFF;
    work->obj_5CC.flags &= 0x7FFF;
    model->flags        |= 0x80;
    work->field_71C      = work->field_71C + 1;
}

void func_actor_400600_8013B410(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coords;
    s32              soundId;
    s32              pan;

    work                = (Actor400600Work*)arg0->work;
    coords              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    work->field_73A    += (0xFF - work->field_73A) >> 5;
    work->field_722    += 1;
    work->field_724    += work->field_722;
    coords->coord.t[1] += work->field_724;
    if (coords->coord.t[1] >= 0) {
        work->field_718 = 0;
        soundId         = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40060003;
        pan             = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        func_actor_400600_80139D98(arg0, 0x19, 0x10);
        coords->coord.t[1] = 0;
        work->field_71C++;
    }
}

void func_actor_400600_8013B520(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              soundId;
    s32              pan;

    work            = (Actor400600Work*)arg0->work;
    work->field_718 = work->field_718 + 1;
    if ((s16)work->field_718 == 1) {
        Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
    }
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40060004;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        D_80115414[0]        = 1;
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
        work3                = (Actor400600Work*)arg0->work;
        work3->field_71C     = 2;
        work3->field_71E     = 0;
    }
}

void func_actor_400600_8013B640(void)
{
    u8 param1[8];
    u8 param2[8];

    if (D_80115415 == 0) {
        /* Same shape as ActorsShared801692e8: each branch makes its own call
         * and jump2's cross-jumping merges the identical tails. */
        if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x3200000 && gGameSession->at4.loc.place == 1) {
            param1[2] = 0x28;
            param1[0] = 2;
            param1[3] = 0;
            param2[0] = 6;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        } else {
            param1[2] = 0x28;
            param1[0] = 1;
            param1[3] = 0;
            param2[0] = 6;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        }
        D_80115415 = 1;
    }
}

void func_actor_400600_8013B6F4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_754 = 0x18;
    work->field_76B = 0;
    work->field_752 = 0x10;
    func_actor_400600_80135998(arg0, 0x10);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013B740(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    SVECTOR          pos;
    s16              min;
    s16              step;
    u32              rnd;

    min = 0x10;
    if (work->field_728 > 0xBB8 && work->field_76B == 0) {
        rnd         = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState = rnd;
        if ((rnd >> 0x10) & 1) {
            min  = 0x18;
            step = 0x24;
        } else {
            min  = 0x14;
            step = 0x1E;
        }
        work->field_754 = step;
        work->field_76B = 1;
    }
    if (work->field_752 < min) {
        work->field_752 = min;
    }
    pos.vx = work->field_A8.x;
    pos.vy = work->field_A8.y;
    pos.vz = work->field_A8.z;
    func_actor_400600_80139C00(arg0, &pos, work->field_754);
    func_actor_400600_80135998(arg0, work->field_752);
}

void func_actor_400600_8013B830(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if (work->field_769 == 0) {
        work->field_720 = 2;
        work->field_726 = 0x20;
        work->field_746 = 9;
        work->field_742 = 1;
    } else {
        work->field_720 = 2;
        work->field_726 = 0x20;
        work->field_746 = 0xB;
        work->field_742 = 1;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013B8AC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if (work->field_72E != 0 && work->field_730 == 1) {
        if (work->field_769 == 0) {
            work->field_726 = 0x20;
            work->field_746 = 9;
            work->field_742 = 2;
        } else {
            work->field_726 = 0x20;
            work->field_746 = 0xB;
            work->field_742 = 2;
        }
        return;
    }
    if ((func_actor_400600_80136FA8(arg0) << 0x10) == 0 && (func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        if (work->field_769 == 0) {
            work2            = (Actor400600Work*)arg0->work;
            work2->field_71C = 2;
            work2->field_71E = 0;
        } else {
            work3            = (Actor400600Work*)arg0->work;
            work3->field_71C = 0xA;
            work3->field_71E = 0;
        }
    }
}

void func_actor_400600_8013B984(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if (work->field_769 == 0) {
        work->field_720 = 3;
        work->field_726 = 0x10;
        work->field_746 = 0xA;
        work->field_742 = 1;
    } else {
        work->field_720 = 3;
        work->field_726 = 0x10;
        work->field_746 = 0xC;
        work->field_742 = 1;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013BA00(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        if (work->field_769 == 0) {
            work2            = (Actor400600Work*)arg0->work;
            work2->field_71C = 2;
            work2->field_71E = 0;
        } else {
            work3            = (Actor400600Work*)arg0->work;
            work3->field_71C = 0xA;
            work3->field_71E = 0;
        }
    }
}

void func_actor_400600_8013BA6C(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if (work->field_769 == 0) {
        work->field_720 = 8;
        work->field_726 = 0x10;
        work->field_746 = 0xF;
        work->field_742 = 1;
    } else {
        work->field_720 = 3;
        work->field_726 = 0x10;
        work->field_746 = 0x11;
        work->field_742 = 1;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013BAEC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
        if (work->field_769 == 0) {
            work2            = (Actor400600Work*)arg0->work;
            work2->field_720 = 8;
            work2->field_726 = 0x10;
            work2->field_746 = 0x10;
            work2->field_742 = 1;
        } else {
            work3            = (Actor400600Work*)arg0->work;
            work3->field_720 = 8;
            work3->field_726 = 0x10;
            work3->field_746 = 0x12;
            work3->field_742 = 1;
        }
        work->field_71E = work->field_71E + 1;
    }
}

void func_actor_400600_8013BB88(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        if (work->field_769 == 0) {
            work2            = (Actor400600Work*)arg0->work;
            work2->field_71C = 2;
            work2->field_71E = 0;
        } else {
            work3            = (Actor400600Work*)arg0->work;
            work3->field_71C = 0xA;
            work3->field_71E = 0;
        }
    }
}

void func_actor_400600_8013BBF4(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_720 = 8;
    work->field_726 = 0x10;
    work->field_746 = 7;
    work->field_742 = 1;
    work->field_718 = 0;
    func_actor_400600_8013CB40(arg0, 0);
    func_actor_400600_80138B5C(arg0, 0);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013BC68(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_720 = 8;
    work->field_726 = 0x10;
    work->field_746 = 8;
    work->field_742 = 1;
    work->field_718 = 0;
    func_actor_400600_8013CB40(arg0, 1);
    func_actor_400600_80138B5C(arg0, 0);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013BCD8(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_80136FA8(arg0) << 0x10) == 0) {
        work->field_718 = 0;
        func_actor_400600_80138B5C(arg0, 0);
        work2            = (Actor400600Work*)arg0->work;
        work2->field_720 = 4;
        work2->field_726 = 0x10;
        work2->field_746 = 1;
        work2->field_742 = 1;
        work->field_71E  = work->field_71E + 1;
    }
}

void func_actor_400600_8013BD54(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u16              frame;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_80136FA8(arg0) << 0x10) == 0) {
        frame           = work->field_718 + 1;
        work->field_718 = frame;
        if ((s16)frame >= 0x11) {
            work->field_718  = 0;
            work2            = (Actor400600Work*)arg0->work;
            work2->field_720 = 4;
            work2->field_726 = 0x10;
            work2->field_746 = 0x15;
            work2->field_742 = 1;
            work->field_71E  = work->field_71E + 1;
        }
    }
}

void func_actor_400600_8013BDF0(Task* arg0)
{
    Actor400600Work* work;
    u16              frame;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_80136FA8(arg0) << 0x10) == 0) {
        frame           = work->field_718 + 1;
        work->field_718 = frame;
        if ((s16)frame >= 0x11) {
            work->field_71E = work->field_71E + 1;
        }
    }
}

void func_actor_400600_8013BE58(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    func_actor_400600_80137498(arg0, 0);
    work->field_763 = 0;
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013BE90(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    Actor400600Work* work2;

    work->field_84 += -(s16)work->field_84 >> 2;
    if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
        if (work->field_764 == 0) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
        }
        func_actor_400600_80138AF0(arg0, 0x3C);
        work->field_84   = 0;
        work2            = (Actor400600Work*)arg0->work;
        work2->field_71C = 2;
        work2->field_71E = 0;
        work->field_767  = 0;
    }
}

void func_actor_400600_8013BF48(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    func_actor_400600_80137498(arg0, 1);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013BF80(Task* arg0)
{
    Actor400600Work* work;

    if (((func_actor_400600_801370F4(arg0) << 0x10) == 0) && ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0)) {
        work            = (Actor400600Work*)arg0->work;
        work->field_71C = 2;
        work->field_71E = 0;
    }
}

void func_actor_400600_8013BFD4(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;

    work             = (Actor400600Work*)arg0->work;
    work->field_718  = 0;
    work2            = (Actor400600Work*)arg0->work;
    work2->field_720 = 2;
    work2->field_726 = 0x10;
    work2->field_746 = 0x16;
    work2->field_742 = 1;
    func_actor_400600_80139DCC(arg0, 0xE, &work->field_88);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013C038(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_720 = 4;
    work->field_726 = 0x10;
    work->field_746 = 0x15;
    work->field_742 = 1;
    work->field_718 = 0;
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013C074(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u32              rnd;

    work = (Actor400600Work*)arg0->work;
    if (((func_actor_400600_801370F4(arg0) << 0x10) == 0) && ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0)) {
        rnd                     = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState             = rnd;
        work->field_710.h.timer = ((rnd >> 0x10) & 0x1F) + 0xD2;
        work2                   = (Actor400600Work*)arg0->work;
        work2->field_71C        = 2;
        work2->field_71E        = 0;
    }
}

void func_actor_400600_8013C104(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_767 = 1;
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013C124(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GsCOORDINATE2*   coord;

    work  = (Actor400600Work*)arg0->work;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    func_actor_400600_80138B40(arg0);
    work2            = (Actor400600Work*)arg0->work;
    work2->field_720 = 4;
    work2->field_726 = 0x10;
    work2->field_746 = 0x20;
    work2->field_742 = 1;
    work->field_722  = 0x40;
    work->field_724  = 0;
    work->field_718  = 0;
    work->field_98   = coord->coord.t[0];
    work->field_9C   = coord->coord.t[2];
    work->field_73E  = work->field_92;
    work->field_71E  = work->field_71E + 1;
}

void func_actor_400600_8013C1C0(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;

    work = (Actor400600Work*)arg0->work;
    func_actor_400600_80138B5C(arg0, 0);
    work2            = (Actor400600Work*)arg0->work;
    work2->field_720 = 2;
    work2->field_726 = 0x10;
    work2->field_746 = 9;
    work2->field_742 = 1;
    work->field_767  = 1;
    work->field_722  = 0;
    work->field_724  = 0;
    work->field_71E  = work->field_71E + 1;
    work->field_73E  = work->field_92;
}

void func_actor_400600_8013C238(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GsCOORDINATE2*   coord;

    work               = (Actor400600Work*)arg0->work;
    coord              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    work->field_722    = work->field_722 + 2;
    work->field_724    = work->field_724 + work->field_722;
    coord->coord.t[1] += work->field_724;
    if ((s16)work->field_92 < coord->coord.t[1]) {
        coord->coord.t[1] = (s16)work->field_92;
        work2             = (Actor400600Work*)arg0->work;
        work2->field_726  = 0x10;
        work2->field_746  = 0x13;
        work2->field_742  = 2;
        work->field_769   = 1;
        work->field_718   = 0;
        work->field_768   = 0;
        work->field_84    = work->field_84 + 0x800;
        work->field_71E   = work->field_71E + 1;
    }
}

void func_actor_400600_8013C2D4(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u32              rnd;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_801370F4(arg0) << 0x10) != 0) {
        rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState     = rnd;
        work->field_750 = ((rnd >> 0x10) & 0x7F) + 0x1E;
    } else if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        rnd              = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = rnd;
        work->field_750  = ((rnd >> 0x10) & 0x7F) + 0x1E;
        work2            = (Actor400600Work*)arg0->work;
        work2->field_71C = 0xA;
        work2->field_71E = 0;
    }
}

void func_actor_400600_8013C394(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if (work->field_769 == 0) {
        work->field_720 = 3;
        work->field_726 = 0x10;
        work->field_746 = 0x1A;
        work->field_742 = 1;
    } else {
        work->field_720 = 3;
        work->field_726 = 0x10;
        work->field_746 = 0x1B;
        work->field_742 = 1;
    }
    func_actor_400600_80138B5C(arg0, 0);
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013C410(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        if (work->field_769 == 0) {
            work2            = (Actor400600Work*)arg0->work;
            work2->field_720 = 0x1E;
            work2->field_726 = 0x10;
            work2->field_746 = 0x10;
            work2->field_742 = 1;
        } else {
            work3            = (Actor400600Work*)arg0->work;
            work3->field_720 = 0x1E;
            work3->field_726 = 8;
            work3->field_746 = 0x14;
            work3->field_742 = 1;
        }
        work->field_71E = work->field_71E + 1;
    }
}

void func_actor_400600_8013C4AC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    if ((func_actor_400600_8013A0B0(arg0) << 0x10) != 0) {
        if (work->field_769 == 0) {
            work2            = (Actor400600Work*)arg0->work;
            work2->field_71C = 2;
            work2->field_71E = 0;
        } else {
            work3            = (Actor400600Work*)arg0->work;
            work3->field_71C = 0xA;
            work3->field_71E = 0;
        }
    }
}

void func_actor_400600_8013C518(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013C534(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;

    work = (Actor400600Work*)arg0->work;
    func_actor_400600_80138B40(arg0);
    if (work->field_728 > 2000) {
        work2            = (Actor400600Work*)arg0->work;
        work2->field_71C = 0xD;
        work2->field_71E = 0;
    } else {
        work3            = (Actor400600Work*)arg0->work;
        work3->field_71C = 8;
        work3->field_71E = 0;
    }
}

void func_actor_400600_8013C598(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    u32              rnd;

    work->field_720 = 4;
    work->field_726 = 0x10;
    work->field_746 = 1;
    work->field_742 = 1;
    rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
    Gp_LcgState     = rnd;
    work->field_756 = ((rnd >> 0x10) & 0x3F) + 0x5A;
    work->field_71E = work->field_71E + 1;
}

void func_actor_400600_8013C5F8(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u16              count;
    u32              rnd;

    work = (Actor400600Work*)arg0->work;
    if (((func_actor_400600_80136FA8(arg0) << 0x10) == 0) && ((func_actor_400600_80137C34(arg0) << 0x10) == 0)) {
        count           = work->field_756 - 1;
        work->field_756 = count;
        if ((count << 0x10) == 0) {
            rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState     = rnd;
            work->field_758 = ((rnd >> 0x10) & 0x3F) + 0x1E;
            func_actor_400600_80138B5C(arg0, 0);
            work2            = (Actor400600Work*)arg0->work;
            work2->field_71C = 2;
            work2->field_71E = 0;
        }
    }
}

void func_actor_400600_8013C6B0(SVECTOR* pos, GpRec18* rec, SVECTOR* out)
{
    VECTOR v;
    VECTOR n;
    s32    dx;
    s32    dz;
    s32    dist;
    s32    t;

    dx   = pos->vx - rec->point.vx;
    v.vy = 0;
    v.vx = dx;
    dz   = pos->vz - rec->point.vz;
    v.vz = dz;
    dist = rec->depth - SquareRoot0(dx * dx + dz * dz);
    t    = dist;
    if (dist <= 0) {
        t = 0;
    }
    dist = t;
    v.vx = pos->vx - rec->point.vx;
    v.vy = pos->vy - rec->point.vy;
    v.vz = pos->vz - rec->point.vz;
    VectorNormal(&v, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &n, &v);
    out->vx = (dist * v.vx) >> 12;
    out->vy = 0;
    out->vz = (dist * v.vz) >> 12;
}

s32 func_actor_400600_8013C7E8(s16 arg0, s16 arg1)
{
    s16 v;

    v = arg1 >> 3;
    if (arg0 == 0) {
        return v;
    }
    if ((arg0 > 0 && v < 0) || (arg0 < 0 && v > 0)) {
        return arg0;
    }
    if (arg0 > 0) {
        if (v < arg0) {
            return arg0;
        }
        return v;
    }
    if (v < arg0) {
        return v;
    }
    return arg0;
}

void func_actor_400600_8013C874(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    TmdObject*       model2;

    work            = (Actor400600Work*)arg0->work;
    model           = (TmdObject*)arg0->extra;
    work->field_73A = 0;
    work2           = (Actor400600Work*)arg0->work;
    model2          = (TmdObject*)arg0->extra;
    if (work2->field_75C.b.field_75E != 1) {
        work2->field_75C.b.field_75E = 1;
        work2->field_75C.b.field_75F = 1;
        work2->field_740             = 0;
        model2->flags               |= 2;
        Gp_SetLightMode(arg0->spawnArg2, 2);
        func_actor_400600_801387DC(arg0, 2);
    }
    work->obj_4B4.flags &= 0x7FFF;
    work->obj_594.flags &= 0x7FFF;
    work->obj_5CC.flags &= 0x7FFF;
    model->flags        |= 0x80;
    work->field_71C      = work->field_71C + 1;
}

void func_actor_400600_8013C940(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u32              rnd;

    work = (Actor400600Work*)arg0->work;
    if (D_80115414[0] == 1) {
        rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState     = rnd;
        work->field_718 = ((rnd >> 0x10) & 7) + 0x14;
        work->field_71C = work->field_71C + 1;
    } else if (D_80115414[0] == 2) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
    }
}

void func_actor_400600_8013C9DC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              soundId;
    s32              pan;

    work = (Actor400600Work*)arg0->work;
    work->field_718--;
    if ((s16)work->field_718 == 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40060003;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        work2                = (Actor400600Work*)arg0->work;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
        work3                = (Actor400600Work*)arg0->work;
        work3->field_71C     = 2;
        work3->field_71E     = 0;
    }
}

s32 func_actor_400600_8013CACC(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    if (work->field_72E == 1 && work->field_768 == 0) {
        switch (work->field_730) {
            case 3:
                work->field_71C = 5;
                work->field_71E = 0;
                work->field_730 = 0;
                return 1;
            case 5:
                work->field_71C = 0xF;
                work->field_71E = 0;
                work->field_730 = 0;
                return 1;
        }
    }
    work->field_730 = 0;
    return 0;
}

void func_actor_400600_8013CB40(Task* arg0, u8 arg1)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;
    s32              mode = arg1;

    if (mode == 0) {
        work->field_766 = 1;
    } else if (mode == 1) {
        work->field_765 = mode;
    }
}

void func_actor_400600_8013CB70(Task* arg0, s32 arg1)
{
    s32 soundId;
    s32 pan;

    if ((arg0->spawnArg1 & 0xF0) == 0x10) {
        arg1 &= 0xFF00FFFF;
        arg1 |= 0x4A0000;
    }
    soundId = arg1 | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
    pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
}

void func_actor_400600_8013CC04(Task* arg0, s16 arg1)
{
    Actor400600Work* work = (Actor400600Work*)arg0->work;

    work->field_71C = arg1;
    work->field_71E = 0;
}
