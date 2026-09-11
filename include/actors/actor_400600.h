#ifndef ACTOR_400600_H
#define ACTOR_400600_H

#include "common.h"
#include "main/task.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

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

/// Payload `func_actor_400600_80133FC0` sends the `Game_GetPtrSlot(3)` task as
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
/// `func_actor_400600_80133434` allocates it with `Mem_Calloc(0x770)` and
/// stores it in the `Task::idMap` slot (0x1C): an enemy actor reuses that
/// pointer field for its own work block, so it is *not* a `TaskIdMap` here.
/// Reach it with `(Actor400600Work*)task->idMap`.
typedef struct Actor400600Work {
    /* 0x000 */ MATRIX             matrix_0;  // copy of the root coordinate's local matrix
    /* 0x020 */ MATRIX             matrix_20; // color matrix for the child models
    /* 0x040 */ MATRIX             matrix_40; // light matrix for the child models
    /* 0x060 */ byte               pad_60[0x20];
    /* 0x080 */ u16                field_80;  // pitch, see ActorsShared80139948
    /* 0x082 */ u16                field_82;  // yaw, see ActorsShared80139948
    /* 0x084 */ u16                field_84;  // roll, see ActorsShared80139948
    /* 0x086 */ byte               pad_86[0x2];
    /* 0x088 */ Actor400600ViewPos field_88;
    /* 0x08E */ byte               pad_8E[0x4];
    /* 0x092 */ u16                field_92; // seeds field_73E on state entry
    /* 0x094 */ byte               pad_94[0x4];
    /* 0x098 */ u16                field_98; // low half of the root coordinate's world X
    /* 0x09A */ u16                field_9A; // copy of field_92
    /* 0x09C */ u16                field_9C; // low half of the root coordinate's world Z
    /* 0x09E */ byte               pad_9E[0xA];
    /* 0x0A8 */ Actor400600ViewPos field_A8; // copied to the stack for ActorsShared80139c00
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
    /* 0x6FC */ byte               pad_6FC[0x8];
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
    /* 0x734 */ byte               pad_734[0x6];
    /* 0x73A */ s16                field_73A; // fade level, lerped toward 0xFF
    /* 0x73C */ s16                field_73C;
    /* 0x73E */ u16                field_73E;
    /* 0x740 */ s16                field_740;
    /* 0x742 */ s16                field_742; // animation request kind
    /* 0x744 */ s16                field_744; // animation id now playing
    /* 0x746 */ s16                field_746; // animation id
    /* 0x748 */ s16                field_748; // sound step index (func_actor_400600_801361AC)
    /* 0x74A */ byte               pad_74A[0x6];
    /* 0x750 */ u16                field_750; // countdown to state 0xB
    /* 0x752 */ s16                field_752;
    /* 0x754 */ s16                field_754;
    /* 0x756 */ u16                field_756; // countdown to the next state-2 transition
    /* 0x758 */ s16                field_758;
    /* 0x75A */ s16                field_75A;
    /* 0x75C */ Actor400600State   field_75C;
    /* 0x760 */ byte               pad_760[0x2];
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
    /* 0x76D */ byte               pad_76D[0x3];
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

#endif
