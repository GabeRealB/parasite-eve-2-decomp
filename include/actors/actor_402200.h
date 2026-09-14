#ifndef ACTOR_402200_H
#define ACTOR_402200_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"

/// The actor's per-part attach coordinates, 0x50 apart, hanging off the
/// display object's 0x08 slot (`TmdObject::field_8`, the trailing per-part
/// `GsCOORDINATE2` array). This overlay's code reaches the root and the fourth
/// part: `func_actor_402200_80137444` parks `&field_F0` in the work block's
/// 0x65C slot, and the shared ground-quad body `ActorsShared8013806c` takes the
/// shadow quad's horizontal position from `field_F0` and its height from
/// `field_0`.
typedef struct Actor402200Coord {
    /* 0x000 */ GsCOORDINATE2 field_0;
    /* 0x050 */ byte          pad_50[0xA0];
    /* 0x0F0 */ GsCOORDINATE2 field_F0;
} Actor402200Coord;

/// Display object hung off `Actor402200::field_2C`; `field_8` is the per-part
/// coordinate array above.
typedef struct Actor402200Obj2C {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ Actor402200Coord* field_8;
} Actor402200Obj2C;

/// Per-instance work block the overlay's setup `func_actor_402200_80137444`
/// allocates with `Mem_Calloc(0x71C)` and parks in the 0x1C slot below (the
/// task's `Task::idMap`, which is not a `TaskIdMap` here).
///
/// `field_6E2` is the ground-shadow shade the shared ground-quad body
/// `ActorsShared8013806c` hands to `Gp_DrawEffGroundQuad`, which draws nothing
/// for a negative value: the frame code turns the calloc'd zero into -1 the
/// first time it runs, so an actor that never raises the shade casts no shadow.
///
/// `field_6F4` is the actor's phase, written and read as a signed halfword:
/// the frame handler clears it on entry, `ActorsShared801381e0` raises it to 1
/// while the remaining-enemy count is positive, and the handler branches on
/// 0 / 1 thereafter.
///
/// `field_6EA` is a pending tint request: `func_actor_402200_80137FB0` reads it
/// once a frame and, while it is 1 or 2, hands the display object the matching
/// translate (0, 0, 0x400 or full 0xFFF) and clears it, so each request is
/// consumed on the frame it is raised.
///
/// `field_718` arms a one-shot vocal cue and `field_71A` is its frame counter.
/// While the flag is clear the body does nothing; once it is set the counter
/// runs up, plays the actor's cue at 0x14, and at 0x5F asks the scene for
/// message 0x3ED - clearing the flag and sending 0x3F1 instead if the scene
/// refuses it.
/// `field_3C` is the animation slot the cue body `func_actor_402200_80135BE0`
/// hands to `Gp_AnimGetRec`: the second of the 0x28-byte slots the actor work
/// blocks lay out from 0x14, the same one the other actor overlays' cue bodies
/// play from. `field_6CA` latches the record's two cue bits (`0x30`) for the
/// next frame, and `field_712` is the running entry index into the overlay's
/// cue-id table `D_actor_402200_80138420` - zero disarms the body, and while it
/// is set the two adjacent words `[field_712 * 2 - 1]` and `[field_712 * 2]`
/// are the cue ids it plays.
typedef struct Actor402200Work {
    /* 0x000 */ byte       pad_0[0x3C];
    /* 0x03C */ GpAnimSlot field_3C;
    /* 0x064 */ byte       pad_64[0x430];
    /// Hit descriptor the flinch handler `func_actor_402200_80131F54` and the
    /// hurt states `func_actor_402200_80133AEC` / `func_actor_402200_80134194`
    /// store on the frame a hit lands: the damage amount `field_716` with the
    /// tag bits 0x30000 OR'd in.
    /* 0x494 */ s32  field_494;
    /* 0x498 */ byte pad_498[2];
    /// Hit-pending flags, raised together with `field_494`: bit 0x8000 is the
    /// flag the hit handler clears when it consumes the descriptor. The shared
    /// body `ActorsShared80137a20` tests the same bit through its own work
    /// block, and `func_actor_402200_80131F54` clears `field_6C6` as it raises
    /// it.
    /* 0x49A */ u16  field_49A;
    /* 0x49C */ byte pad_49C[0x220];
    /// Sound event id the sequence body `func_actor_402200_8013539C` queues: the
    /// overlay's cue word `D_actor_402200_80138468` with the `GpEnemy` work id's
    /// high nibble in bits 8-11, the same construction the cue body
    /// `func_actor_402200_80135BE0` uses on `D_actor_402200_80138420`. Stored
    /// back to the block and re-read from there as the first argument of
    /// `SndEvt_EnqueueType6`.
    /* 0x6BC */ s32 field_6BC;
    /// Animation id the frame code reseeds slots 1..0x12 with; the reseed body
    /// `func_actor_402200_80137EEC` also indexes the blend table
    /// `D_actor_402200_801383AC` with it.
    /* 0x6C0 */ s16 field_6C0;
    /// Animation id the slots were last reseeded with, so the reseed runs once
    /// per change rather than every frame.
    /* 0x6C2 */ s16 field_6C2;
    /// Frames the current animation has been ticking; the reseed clears it and
    /// the tick path walks it up by one a frame.
    /* 0x6C4 */ s16 field_6C4;
    /// Flinch countdown: `func_actor_402200_80131F54` arms it from
    /// `Gp_GetIdParam2` when a hit lands and ticks it down a frame at a time,
    /// raising `field_494`/`field_49A` on the frame it runs out. While it is
    /// non-zero a hit is already being flinched, so the sequence bodies arm
    /// the pair immediately only when it is zero.
    /* 0x6C6 */ s16 field_6C6;
    /// Cleared on the frame the sequence body `func_actor_402200_8013539C`
    /// reseeds the animation.
    /* 0x6C8 */ s16 field_6C8;
    /* 0x6CA */ u16 field_6CA;
    /// Set to 4 when the sequence restarts in mode 2, cleared otherwise.
    /* 0x6CC */ s16 field_6CC;
    /// State `func_actor_402200_8013539C` advances: 0 reseeds the animation at
    /// `field_6C0` and arms the cue, 1 waits for `field_6C4` to reach 0x37 and
    /// then drops the state back to 0 so the reseed runs again.
    /// The attack sequence `func_actor_402200_801354B0` runs the same shape
    /// over three states: its state 0 picks between slot sets 9 and 0xA on
    /// `field_6D2` and parks the state on the matching one, and states 1 / 2
    /// each wait out their own `field_6C4` threshold (0x50 and 0x3B) before
    /// dropping back to 0.
    /* 0x6CE */ s16  field_6CE;
    /* 0x6D0 */ byte pad_6D0[2];
    /// Which-side flag the target body `func_actor_402200_80131F54` raises from
    /// a dot product of the offset to the actor it is tracking: 1 when the
    /// product comes out zero, 0 otherwise. The sequence bodies branch on it -
    /// `func_actor_402200_801354B0` picks between slot sets 9 and 0xA, and
    /// `func_actor_402200_80135630` / `func_actor_402200_80135A24` between 0xD
    /// and the set at `field_6C0`.
    /* 0x6D2 */ s16 field_6D2;
    /// Countdown `func_actor_402200_801347F4` rolls from the `Gp_LcgState` LCG
    /// (0x4B..0x6A) when it reseeds the animation, and ticks down a frame at a
    /// time until it runs out and the cue fires.
    /* 0x6D4 */ u16  field_6D4;
    /* 0x6D6 */ byte pad_6D6[4];
    /// Timer pair the reseed arms alongside `field_6DE`.
    /* 0x6DA */ s16 field_6DA;
    /* 0x6DC */ s16 field_6DC;
    /// Third timer the reseed arms; written last of the three.
    /* 0x6DE */ s16 field_6DE;
    /// Fourth timer `func_actor_402200_801347F4` clears alongside the trio
    /// above when its countdown runs out.
    /* 0x6E0 */ s16  field_6E0;
    /* 0x6E2 */ s16  field_6E2;
    /* 0x6E4 */ byte pad_6E4[6];
    /* 0x6EA */ s16  field_6EA;
    /// Sequence mode `func_actor_402200_8013539C` tests: the reseed arms the
    /// cue unless it is already 1, and a restart that finds it 1 flips it to 2.
    /* 0x6EC */ s16  field_6EC;
    /* 0x6EE */ byte pad_6EE[6];
    /* 0x6F4 */ s16  field_6F4;
    /* 0x6F6 */ byte pad_6F6[0x1C];
    /* 0x712 */ s16  field_712;
    /* 0x714 */ byte pad_714[2];
    /// Damage amount the hit handlers OR into `field_494`; read as a signed
    /// halfword on the frame the hit lands.
    /* 0x716 */ s16 field_716;
    /* 0x718 */ s16 field_718;
    /* 0x71A */ s16 field_71A;
} Actor402200Work;
STATIC_ASSERT_SIZEOF(Actor402200Work, 0x71C);

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block above, `field_20` the `GpEnemy` the spawner left in the task's
/// `Task::spawnArg2` slot, and `field_2C` the display object. Same shape as the
/// other actor overlays' contexts.
typedef struct Actor402200 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor402200Work*  field_1C;
    /* 0x20 */ GpEnemy*          field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor402200Obj2C* field_2C;
} Actor402200;

/// 0x18-byte block `func_actor_402200_80138208` takes from `G_SCRATCH_HEAD`
/// while projecting the actor's origin through its attach coordinate and
/// working out the ordering-table depth. `vec` is the zeroed origin the GTE
/// reads; the rest are the projection's results, and their order is the one
/// `rtps` writes them in: screen xy, depth cue, `FLAG`, and the average
/// screen z `otz`. Same block the other actor overlays spell
/// `<overlay>ProjectScratch`.
typedef struct Actor402200ProjectScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     sxy;
    /* 0x0C */ s32     dp;
    /* 0x10 */ s32     flag;
    /* 0x14 */ s32     otz;
} Actor402200ProjectScratch;
STATIC_ASSERT_SIZEOF(Actor402200ProjectScratch, 0x18);

/// Per-animation-id value `func_actor_402200_80137EEC` hands `func_800B4114`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 D_actor_402200_801383AC[];

/// The game's shared 32-bit LCG state: every draw is
/// `Gp_LcgState = Gp_LcgState * 5 + 0x71357911`, read back from the global,
/// with the caller taking the bits it wants out of the high half.
extern u32 Gp_LcgState;

#endif
