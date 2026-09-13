#ifndef ACTOR_405800_H
#define ACTOR_405800_H

#include "common.h"
#include "main/task.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include <psyq/libgte.h>

/// Packed halfwords `func_actor_405800_80138514` reads as the desired root
/// translation. Only `x` and `z` are used; the middle halfword is kept so the
/// layout matches `Actor400600ViewPos`.
typedef struct Actor405800ViewPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
} Actor405800ViewPos;
STATIC_ASSERT_SIZEOF(Actor405800ViewPos, 0x6);

/// Status flags at + 0x83C of the work block, read through two widths: bit 0
/// as a halfword, then bits 0x102 as a word (`func_actor_405800_80137908`).
/// The high half is `field_83E`, the scale reset to 0x1000 on death.
typedef union Actor405800Flags83C {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
    struct {
        /* 0x0 */ u16 pad;
        /* 0x2 */ s16 field_83E;
    } h;
} Actor405800Flags83C;
STATIC_ASSERT_SIZEOF(Actor405800Flags83C, 0x4);

/// Payload `func_actor_405800_801340E0` sends the `Game_GetPtrSlot(3)` task as
/// message 0x3FF, with `field_0` pointing at `D_actor_405800_801513F8`.
typedef struct Actor405800Msg3FF {
    /* 0x00 */ void* field_0;
    /* 0x04 */ s32   field_4;
    /* 0x08 */ s32   field_8;
    /* 0x0C */ s32   field_C;
    /* 0x10 */ s32   field_10;
} Actor405800Msg3FF;
STATIC_ASSERT_SIZEOF(Actor405800Msg3FF, 0x14);

/// Reply buffer `func_actor_405800_801340E0` passes with message 0x3F8; only
/// `field_14` is seeded (to 0x18) before the query.
typedef struct Actor405800Msg3F8 {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s32  field_14;
} Actor405800Msg3F8;
STATIC_ASSERT_SIZEOF(Actor405800Msg3F8, 0x18);

/// 0x8C-byte scratchpad frame `func_actor_405800_801329C8` carves off
/// `G_SCRATCH_HEAD` to draw a textured quad between two model parts. Same
/// layout as `ActorsShared80163354Scratch` without the trailing half offsets,
/// which this variant keeps in registers.
typedef struct Actor405800BeamScratch {
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
} Actor405800BeamScratch;
STATIC_ASSERT_SIZEOF(Actor405800BeamScratch, 0x8C);

/// Per-actor work block for the `actor_405800` overlay.
///
/// `func_actor_405800_801334B8` allocates it with `Mem_Calloc(0x89C, 0)` and
/// stores the result straight into the `Task::idMap` slot (0x1C), so the size
/// below is the allocation and not a guess: this actor reuses that pointer
/// field for its own work block and it is *not* a `TaskIdMap` here. It is the
/// only allocator in the overlay, so every function reaches the same block
/// with `(Actor405800Work*)task->idMap`.
///
/// `field_846` / `field_848` are the state and sub-state indices the handler
/// table walks and `field_842` is the per-state frame counter.
typedef struct Actor405800Work {
    /* 0x000 */ MATRIX              matrix_0;  // copy of the root coordinate's local matrix
    /* 0x020 */ MATRIX              matrix_20; // color matrix for the child models
    /* 0x040 */ MATRIX              matrix_40; // light matrix for the child models
    /* 0x060 */ byte                pad_60[0x10];
    /* 0x070 */ VECTOR              field_70;  // copy of the root coordinate's translation
    /* 0x080 */ u16                 field_80;  // pitch, see ActorsShared80139948
    /* 0x082 */ u16                 field_82;  // yaw, see ActorsShared80139948
    /* 0x084 */ u16                 field_84;  // roll, see ActorsShared80139948
    /* 0x086 */ byte                pad_86[2];
    /* 0x088 */ Actor405800ViewPos  field_88;
    /* 0x08E */ byte                pad_8E[2];
    /* 0x090 */ u16                 field_90; // spawn position X (low half)
    /* 0x092 */ u16                 field_92; // copied into field_86A on state entry
    /* 0x094 */ u16                 field_94; // spawn position Z (low half)
    /* 0x096 */ byte                pad_96[2];
    /* 0x098 */ u16                 field_98; // low half of the root coordinate's world X
    /* 0x09A */ s16                 field_9A;
    /* 0x09C */ u16                 field_9C; // low half of the root coordinate's world Z
    /* 0x09E */ byte                pad_9E[0xA];
    /* 0x0A8 */ SVECTOR             field_A8; // world point `ActorsShared80139c00` turns to face (it reads `vx` / `vz`)
    /* 0x0B0 */ GpAnimCtx           anim;     // slots 1..0x11 reset by func_actor_405800_80138224
    /* 0x0C4 */ GpAnimSlot          slots[0x12];
    /* 0x394 */ byte                pad_394[0x120];
    /* 0x4B4 */ GpObj               obj_4B4;    // collision node; unlinked on death
    /* 0x4D4 */ GpRec18             rec_4D4[8]; // obj_4B4 table
    /* 0x594 */ GpObj               obj_594;    // collision node; unlinked on death
    /* 0x5B4 */ GpRec18             rec_5B4[8]; // obj_594 table
    /* 0x674 */ GpObj               obj_674;    // collision node; unlinked on death
    /* 0x694 */ GpObj               obj_694;    // collision node; unlinked on death
    /* 0x6B4 */ GpObj               obj_6B4;    // collision node; unlinked on death
    /* 0x6D4 */ GpObj               obj_6D4;    // collision node; unlinked on death
    /* 0x6F4 */ GpRec18             rec_6F4[1]; // obj_6B4 / obj_674 table
    /* 0x70C */ GpRec18             rec_70C[1]; // obj_6D4 / obj_694 table
    /* 0x724 */ GpObj               obj_724;    // collision node; flags bit 0x4000 cleared by func_actor_405800_801379F8
    /* 0x744 */ GpActorD4Rec        rec_744;    // obj_724 payload (flags kind 3)
    /* 0x75C */ GpRec18             rec_75C[8]; // occupancy table behind rec_744
    /* 0x81C */ GpEffArg            eff_81C;    // fourth model part's coordinate
    /* 0x824 */ Task*               field_824;  // child task, killed on state exit
    /* 0x828 */ Task*               field_828;  // child task, killed on state exit
    /* 0x82C */ byte                pad_82C[4];
    /* 0x830 */ u16                 field_830;  // phase timer; lhu-incremented, compared as s16
    /* 0x832 */ s16                 field_832;
    /* 0x834 */ s16                 field_834;
    /* 0x836 */ s16                 field_836;
    /* 0x838 */ s16                 field_838;
    /* 0x83A */ s16                 field_83A; // nonzero: skip the field_895 / field_896 reset
    /* 0x83C */ Actor405800Flags83C flags_83C;
    /* 0x840 */ u16                 field_840; // LCG draw at spawn
    /* 0x842 */ u16                 field_842; // per-state frame counter
    /* 0x844 */ s16                 field_844; // cleared with field_842 on state entry
    /* 0x846 */ u16                 field_846; // state index
    /* 0x848 */ u16                 field_848; // sub-state index
    /* 0x84A */ s16                 field_84A;
    /* 0x84C */ s16                 field_84C;
    /* 0x84E */ s16                 field_84E;
    /* 0x850 */ s16                 field_850; // animation speed / step scale
    /* 0x852 */ s16                 field_852; // compared against 2000 to pick state 8 vs 0xD
    /* 0x854 */ u16                 field_854; // facing-delta halfword, range-checked vs 0x200..0xE00
    /* 0x856 */ u16                 field_856; // facing-delta halfword, range-checked vs 0x200..0xE00
    /* 0x858 */ s16                 field_858; // must be 1 for the pending field_85A transition
    /* 0x85A */ s16                 field_85A; // pending transition: 3 -> state 5, 5 -> state 0xF
    /* 0x85C */ s16                 field_85C; // countdown seeded by func_actor_405800_80137994, ticked by func_actor_405800_8013795C
    /* 0x85E */ s16                 field_85E; // countdown, ticked by func_actor_405800_8013795C
    /* 0x860 */ byte                pad_860[0x6];
    /* 0x866 */ s16                 field_866;
    /* 0x868 */ byte                pad_868[0x2];
    /* 0x86A */ s16                 field_86A; // seeded from field_92; set to -0x9C4 during the hop
    /* 0x86C */ byte                pad_86C[0x2];
    /* 0x86E */ s16                 field_86E; // animation request kind
    /* 0x870 */ s16                 field_870; // animation id now playing
    /* 0x872 */ s16                 field_872; // animation id
    /* 0x874 */ s16                 field_874; // sound step index
    /* 0x876 */ byte                pad_876[0x6];
    /* 0x87C */ u16                 field_87C; // down-counter
    /* 0x87E */ s16                 field_87E;
    /* 0x880 */ s16                 field_880;
    /* 0x882 */ s16                 field_882; // randomised hold, 0x5A .. 0x99 frames
    /* 0x884 */ s16                 field_884; // cleared on the state-entry path
    /* 0x886 */ byte                pad_886[0x4];
    /* 0x88A */ u8                  field_88A;
    /* 0x88B */ s8                  field_88B;
    /* 0x88C */ u8                  field_88C;
    /* 0x88D */ s8                  field_88D;
    /* 0x88E */ s8                  field_88E;
    /* 0x88F */ s8                  field_88F;
    /* 0x890 */ u8                  field_890; // nonzero: allow the state-0xD transition when root X > 10000
    /* 0x891 */ u8                  field_891;
    /* 0x892 */ u8                  field_892; // distance mode: 0 none, 1 XZ, 2 XY
    /* 0x893 */ u8                  field_893;
    /* 0x894 */ byte                pad_894;
    /* 0x895 */ u8                  field_895;
    /* 0x896 */ u8                  field_896;
    /* 0x897 */ u8                  field_897; // 0: node.field_4 uses 4/5 instead of 0/1
    /* 0x898 */ u8                  field_898;
    /* 0x899 */ byte                pad_899[0x3];
} Actor405800Work;
STATIC_ASSERT_SIZEOF(Actor405800Work, 0x89C);

#endif // ACTOR_405800_H
