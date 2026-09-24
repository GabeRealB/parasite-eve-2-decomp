#ifndef ACTOR_521100_H
#define ACTOR_521100_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"

/// Sparse view of the actor's `TmdObject` (`Actor521100::field_2C`): the two
/// pointers `TmdObject::next` / `prev` are padded over and the display object's
/// own fields are read through it. `field_8` is the model's coordinate array
/// `TmdObject::coords` - the same `GsCOORDINATE2*` every other actor overlay
/// indexes, with the update body `func_actor_521100_80136290` reading the
/// second entry and the walk body `func_actor_521100_801358D4` the first.
typedef struct Actor521100Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor521100Obj2C;

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block below, `field_20` the `GpEnemy` the spawner left in the task's
/// `Task::spawnArg2` slot, and `field_2C` the display object. Same shape as the
/// other actor overlays' contexts.
typedef struct Actor521100 {
    /* 0x00 */ byte                    pad_0[0x1C];
    /* 0x1C */ struct Actor521100Work* field_1C;
    /* 0x20 */ GpEnemy*                field_20;
    /* 0x24 */ byte                    pad_24[8];
    /* 0x2C */ Actor521100Obj2C*       field_2C;
} Actor521100;

/// Work block of the actor `func_actor_521100_80131E8C` spawns:
/// `memCalloc(0x6C0, 0)`, hung off `Task::work`. It holds the model's
/// animation context, slots and pose buffer, the `color` / `light` matrices the
/// model is drawn under, and the collision bodies the spawn links: `obj47C`
/// and `obj514` (kind 2, over the `rec49C` and `rec534` contact tables),
/// `obj57C` / `obj59C` sharing `rec5BC`, and `obj5D4` / `obj5F4` sharing
/// `rec62C`, the first of that pair carrying the `shape` segment.
typedef struct Actor521100Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x13];
    /* 0x30C */ byte       poses[0x130];
    /* 0x43C */ MATRIX     color;
    /* 0x45C */ MATRIX     light;
    /* 0x47C */ GpObj      obj47C;
    /* 0x49C */ GpRec18    rec49C[5];
    /* 0x514 */ GpObj      obj514;
    /* 0x534 */ GpRec18    rec534[3];
    /// The two collision nodes the burn-out sequence arms, the pair
    /// `Actor510900Work`'s `obj4E4` / `obj504` carry. `func_actor_521100_80131E8C`
    /// fills both - the two pointers, `pos` and
    /// `key` / `radius` - and links them. The state bodies then raise
    /// `flags` bit 0x8000 on the frame their effect fires, hand both back with
    /// an `&= 0x7FFF` when the sequence advances, and take the word
    /// `Gp_PackPair` returns into `key`.
    /* 0x57C */ GpObj obj57C;
    /// See `obj57C`.
    /* 0x59C */ GpObj        obj59C;
    /* 0x5BC */ GpRec18      rec5BC[1];
    /* 0x5D4 */ GpObj        obj5D4;
    /* 0x5F4 */ GpObj        obj5F4;
    /* 0x614 */ GpActorD4Rec shape;
    /* 0x62C */ GpRec18      rec62C[1];
    /// `func_800FDB18` argument record `func_actor_521100_80135230` refreshes
    /// on the effect frames of the burn-out sequence.
    /* 0x644 */ GpEffArg     eff;
    /* 0x64C */ s16          field_64C; // the attach coordinate's translation, snapshotted each frame
    /* 0x64E */ s16          field_64E;
    /* 0x650 */ s16          field_650;
    /* 0x652 */ byte         pad_652[2];
    /* 0x654 */ Actor521100* field_654;
    /* 0x658 */ byte         pad_658[0x20];
    /// Residual twist of the coordinate at `field_8[3]`, two angles of the
    /// +/-(0x40..0xBF) range the hit body `func_actor_521100_801322F8` draws
    /// from `Gp_LcgState` on the frame it takes a hit. It writes them here and
    /// arms `field_680`; the untwist body `func_actor_521100_80135024` then
    /// rotates that coordinate's matrix back by them, stepping each angle 0x20
    /// towards zero per frame until both arrive and it clears the flag. Same
    /// pair as `Actor510900Work::field_570` / `field_584` and
    /// `Actor02000Work::field_688` / `field_6B4`.
    /* 0x678 */ SVECTOR field_678;
    /* 0x680 */ s16     field_680;
    /* 0x682 */ s16     field_682; // non-zero while the tick in func_actor_521100_80135B80 remaps the model's field_C
    /* 0x684 */ s16     field_684;
    /// The clip the slots are blended to and the clip they currently carry.
    /// The preset handler `func_actor_521100_80135C14` stores one clip id into
    /// both, so the blend is skipped; `func_actor_521100_80135964` later walks
    /// every slot towards `field_686` while the two differ, then ticks them
    /// once they agree. `field_68A` counts the ticks, and is cleared when a new
    /// clip is latched into `field_688`.
    /* 0x686 */ s16 field_686;
    /* 0x688 */ s16 field_688;
    /* 0x68A */ u16 field_68A;
    /* 0x68C */ s16 field_68C;
    /* 0x68E */ s16 field_68E;
    /* 0x690 */ s16 field_690;
    /* 0x692 */ s16 field_692;
    /* 0x694 */ s16 field_694;
    /// 12-bit angles. The step-1 entry body `func_actor_521100_80135680`
    /// subtracts them, wraps the difference into [-0x800, 0x800] and reads
    /// `field_6AA` when the result is under 0x200.
    /* 0x696 */ u16 field_696;
    /* 0x698 */ u16 field_698;
    /* 0x69A */ s16 field_69A; // forward speed, in 12-bit fixed point
    /* 0x69C */ s16 field_69C; // cleared together with the forward speed
    /* 0x69E */ s16 field_69E;
    /* 0x6A0 */ s16 field_6A0;
    /* 0x6A2 */ s16 field_6A2;
    /* 0x6A4 */ s16 field_6A4;
    /// Parked animation the burn-out body `func_actor_521100_80133104` clears
    /// on its own frame, the same slot `actor_102000` and `actor_105700` park
    /// into.
    /* 0x6A6 */ s16 field_6A6;
    /* 0x6A8 */ s16 field_6A8;
    /* 0x6AA */ s16 field_6AA;
    /* 0x6AC */ s16 field_6AC;
    /// Armed by `func_actor_521100_80133104` on the frame the burn-out sound
    /// fires and cleared again when the sequence advances.
    /* 0x6AE */ s16 field_6AE;
    /* 0x6B0 */ s16 field_6B0;
    /* 0x6B2 */ s16 field_6B2;
    /// The animation record's flag nibble (`rec->field_3 & 0x30`) latched for
    /// the next frame by the footstep cue body `func_actor_521100_80134D88`, so
    /// each foot fires on the frame its bit has just dropped.
    /* 0x6B4 */ u16 field_6B4;
    /// Current and previous burn-out choices, used to avoid a third repeat.
    /* 0x6B6 */ s16 field_6B6;
    /* 0x6B8 */ s16 field_6B8;
    /// Non-zero asks the burn-out bodies to hand the actor on to state 6
    /// (`field_69E = 6`) instead of back to the idle state 0; `field_6BC` is
    /// the sub-state they then start at.
    /* 0x6BA */ s16 field_6BA;
    /* 0x6BC */ u16 field_6BC;
    /* 0x6BE */ s16 field_6BE;
} Actor521100Work;
STATIC_ASSERT_SIZEOF(Actor521100Work, 0x6C0);

/// The game-wide 32-bit LCG. `func_actor_521100_80136290` draws it three times
/// in a row, scaling the second coordinate's world position by `field_488`
/// through the top half of each draw.
extern u32 Gp_LcgState;

#endif
