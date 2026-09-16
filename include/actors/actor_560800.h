#ifndef ACTOR_560800_H
#define ACTOR_560800_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block this overlay hangs off the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. Reach it with
/// `(Actor560800Work*)task->idMap`.
///
/// `func_actor_560800_80135BD8` allocates it with `Mem_Malloc(0x68, 0)`, so the
/// size below is the allocation and not a guess, and fills the first slots with
/// the sub-tasks from `Game_GetPtrSlot(3)` and `ActorsShared80136280Desc`
/// (`field_4` is filled later by `func_actor_560800_801366B0`). Slots 0x0-0x24
/// are ten task pointers: `field_8` is handed to `field_10`/`field_14`/`field_18`
/// as their spawn argument and `field_C` to `field_1C`.
///
/// Above `field_14` the slots are s16 pairs at an 8-byte stride:
/// `func_actor_560800_801367E0` writes 0x28/0x2A, 0x30/0x32, 0x38/0x3A and
/// 0x40/0x42 with the same (value, 0) shape this unit uses for 0x58/0x5A and
/// for 0x60/0x62, and `func_actor_560800_80136818` sets 0x64.
///
/// The three pointer slots at 0x1C/0x20/0x24 are `Gp_DispatchMsg` targets, not
/// flags: `func_actor_560800_80133540` sends the message its switch picks to
/// one of them, `func_actor_560800_8013631C` sends 0x7DB to `field_24`, and
/// `func_actor_560800_801362E0` sends 0x7DB to `field_20`.
typedef struct Actor560800Work {
    /* 0x00 */ Task* field_0; // Game_GetPtrSlot(3)
    /* 0x04 */ Task* field_4;
    /* 0x08 */ Task* field_8;
    /* 0x0C */ Task* field_C;
    /* 0x10 */ Task* field_10;
    /* 0x14 */ Task* field_14;
    /* 0x18 */ Task* field_18;
    /* 0x1C */ Task* field_1C;
    /* 0x20 */ Task* field_20;
    /* 0x24 */ Task* field_24;
    /* 0x28 */ s16   field_28;
    /* 0x2A */ s16   field_2A;
    /* 0x2C */ s16   field_2C;
    /* 0x2E */ byte  pad_2E[2];
    /* 0x30 */ s16   field_30;
    /* 0x32 */ s16   field_32;
    /* 0x34 */ byte  pad_34[4];
    /* 0x38 */ s16   field_38;
    /* 0x3A */ s16   field_3A;
    /* 0x3C */ byte  pad_3C[4];
    /* 0x40 */ s16   field_40;
    /* 0x42 */ s16   field_42;
    /* 0x44 */ byte  pad_44[0x14];
    /* 0x58 */ s16   field_58;
    /* 0x5A */ s16   field_5A;
    /* 0x5C */ byte  pad_5C[4];
    /* 0x60 */ s16   field_60;
    /* 0x62 */ s16   field_62;
    /* 0x64 */ s16   field_64;
    /* 0x66 */ s16   field_66;
} Actor560800Work;
STATIC_ASSERT_SIZEOF(Actor560800Work, 0x68);

/// One step of the animation script `func_actor_560800_80132340` walks,
/// indexed by `Actor560800Work::field_60`: `field_0` is the frame hold counted
/// in `field_62` (0 waits for the player to report the clip done, message
/// 0x3ED), and `field_2` is the next animation id sent as message 0x3F4; a
/// negative `field_2` ends the script.
typedef struct Actor560800AnimStep {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ s16 field_2;
} Actor560800AnimStep;
STATIC_ASSERT_SIZEOF(Actor560800AnimStep, 0x4);

/// Work block of the sub-task `Actor560800Work::field_8` points at, spawned
/// from `ActorsShared80136280Desc` index 5 (`func_actor_560800_80132C60`).
/// That function allocates it with `Mem_Malloc(0x4CC, 0)`, `Mem_Set`s the same
/// 0x4CC bytes and stores it in its own `Task::idMap` (0x1C), so the size below
/// is the allocation, not a guess. It is a third idMap block in this overlay,
/// distinct from `Actor560800Work` and `Actor560800FadeWork`.
///
/// `anim` is the animation context the block itself is handed to
/// `Gp_AnimResetSlot` as, laid out the way every actor carries it: the context
/// at 0, its 0x14 slots at +0x14 and the 0x90-byte scratch `func_800B3F84`
/// takes as its `arg3` at +0x334. `func_actor_560800_80132C60` passes exactly
/// `block`, `block + 0x334` and `block + 0x14` to that call and then stores
/// 0x14 in `field_4BA`, which is why the slots array is sized 0x14 and the
/// scratch sits where it does.
///
/// `field_4B8` is the animation id the slots are seeded with (the same role
/// `ActorsShared80132514Work::field_4B8` plays), `field_4BA` the slot count
/// the reset loop walks 1..count, and `field_4C8` the 0x10 written into each
/// slot's `field_9`. `field_4CA` is a phase counter the same handler reads.
/// `field_4B4` is the animation script `func_actor_560800_80132498` walks by
/// `field_4B8`, with `field_4BE` as its hold counter.
typedef struct Actor560800AnimWork {
    /* 0x000 */ GpAnimCtx            anim;
    /* 0x014 */ GpAnimSlot           slots[0x14];
    /* 0x334 */ byte                 animAux[0x90];
    /* 0x3C4 */ byte                 pad_3C4[0xB0];
    /* 0x474 */ MATRIX               light;
    /* 0x494 */ MATRIX               color;
    /* 0x4B4 */ Actor560800AnimStep* field_4B4;
    /* 0x4B8 */ s16                  field_4B8;
    /* 0x4BA */ u16                  field_4BA;
    /* 0x4BC */ u16                  field_4BC;
    /* 0x4BE */ s16                  field_4BE;
    /* 0x4C0 */ s16                  field_4C0;
    /* 0x4C2 */ s16                  field_4C2;
    /* 0x4C4 */ s16                  field_4C4;
    /* 0x4C6 */ s16                  field_4C6;
    /* 0x4C8 */ s16                  field_4C8;
    /* 0x4CA */ s16                  field_4CA;
} Actor560800AnimWork;
STATIC_ASSERT_SIZEOF(Actor560800AnimWork, 0x4CC);

/// Work block `func_actor_560800_801376E0` allocates with `Mem_Malloc(0x28C, 0)`
/// and stores in its own `Task::idMap` (0x1C), so the size below is the
/// allocation, not a guess. A fourth idMap block in this overlay, distinct from
/// `Actor560800Work`, `Actor560800AnimWork` and `Actor560800FadeWork`, and the
/// one `func_actor_560800_80137820` and `func_actor_560800_80136AA8` drive.
///
/// It opens with the animation context - the context at 0, its slots at +0x14 -
/// the way every actor carries it, then the pose buffer `func_800B3F84` takes as
/// its `arg3` at +0x12C. Seven slots is what fits between the two: 0x12C - 0x14
/// is 7 * 0x28, and `D_actor_560800_801752F0` carries seven animation sets after
/// its leading null. `light` / `color` go to the object's `field_1C` / `field_20`
/// (the lower offset is the light matrix, as in every actor).
///
/// `field_26C` is the task the spawn argument named, handed to `Task_Reparent`;
/// `field_270` / `field_274` / `field_278` are the three `Gp_LcgState` draws
/// `func_actor_560800_801376E0` takes at spawn; `field_280` is the slot count it
/// seeds from the spawner's `spawnArg1`, which `func_actor_560800_80137820` then
/// walks 1..count with `Gp_AnimResetSlot`. `field_27C` / `field_27E` and the
/// 0x38 bytes of `rot` (`Mem_CopyUnaligned`'s source and destination in
/// `func_actor_560800_80136AA8`) belong to the handlers, not to the spawner.
typedef struct Actor560800ModelWork {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[7];
    /* 0x12C */ byte       poseBuf[0x50];
    /* 0x17C */ MATRIX     field_17C;
    /* 0x19C */ MATRIX     light;
    /* 0x1BC */ MATRIX     color;
    /* 0x1DC */ SVECTOR    rot[7];
    /* 0x214 */ SVECTOR    swing[7];
    /* 0x24C */ s16        field_24C;
    /* 0x24E */ s16        field_24E;
    /* 0x250 */ s16        field_250;
    /* 0x252 */ byte       pad_252[2];
    /* 0x254 */ s16        field_254;
    /* 0x256 */ u16        field_256;
    /* 0x258 */ s16        field_258;
    /* 0x25A */ byte       pad_25A[2];
    /* 0x25C */ u16        swingDir[8];
    /* 0x26C */ Task*      field_26C;
    /* 0x270 */ u32        field_270;
    /* 0x274 */ u32        field_274;
    /* 0x278 */ s16        field_278;
    /* 0x27A */ byte       pad_27A[2];
    /* 0x27C */ s16        field_27C;
    /* 0x27E */ s16        field_27E;
    /* 0x280 */ s16        field_280;
    /* 0x282 */ s16        field_282;
    /* 0x284 */ byte       pad_284[2];
    /* 0x286 */ s16        field_286;
    /* 0x288 */ s16        field_288;
    /* 0x28A */ s16        field_28A;
} Actor560800ModelWork;
STATIC_ASSERT_SIZEOF(Actor560800ModelWork, 0x28C);

/// 8-byte fade block `func_actor_560800_80135FA0` allocates with
/// `Mem_Malloc(8, 0)` and parks in `Task::idMap` -- a second, smaller idMap
/// block in this overlay, distinct from `Actor560800Work` and owned by the
/// fade-in task that function reparents to the controller.
///
/// The three halfwords are the RGB channels `Fade_DrawOverlay` draws: the task
/// raises all three by `spawnArg1` each frame (so `spawnArg1` is the fade rate,
/// not a colour) and kills itself and clears the display mask once the red
/// channel passes 0x100. `field_0` is never touched.
typedef struct Actor560800FadeWork {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  r;
    /* 0x4 */ u16  g;
    /* 0x6 */ u16  b;
} Actor560800FadeWork;
STATIC_ASSERT_SIZEOF(Actor560800FadeWork, 0x8);

/// Work block of the message-handler task whose `Task::field_24` table is
/// `D_actor_560800_801756D4`: `func_actor_560800_801386D4` allocates it with
/// `Mem_Malloc(0x4C, 0)`, `Mem_Set`s the same 0x4C bytes and stores it in that
/// task's `Task::idMap` (0x1C), so the size below is the allocation, not a
/// guess. A fifth idMap block in this overlay, distinct from `Actor560800Work`,
/// `Actor560800AnimWork`, `Actor560800ModelWork` and `Actor560800FadeWork`.
///
/// `parts` is the eight part tasks the same function spawns from
/// `D_actor_560800_8017575C` (index 1, spawn arg `i + 1`) and parks one per
/// slot; its teardown path clears a slot back to NULL after parking the part
/// task it names in state 4. `func_actor_560800_80139360` walks the slots and
/// applies message 0x7D5 to the `TmdObject` each part carries.
///
/// `field_40` is the task the spawner passed as `Task::spawnArg2`, reparented
/// to this one - the same role `Actor560800ModelWork::field_26C` plays. While
/// its `field_4A` is 0x83 or 0x22, `world` is the matrix `Gp_ComposeParentWorld`
/// composes from part 9 of the controller's `field_4` / `field_C` model; the
/// translation is then overwritten with the returned position, `t[1]` biased
/// by -0x78.
typedef struct Actor560800PartsWork {
    /* 0x00 */ MATRIX world;
    /* 0x20 */ Task*  parts[8];
    /* 0x40 */ Task*  field_40;
    /* 0x44 */ s16    field_44;
    /* 0x46 */ s16    field_46;
    /* 0x48 */ s16    field_48;
    /* 0x4A */ s16    field_4A;
} Actor560800PartsWork;
STATIC_ASSERT_SIZEOF(Actor560800PartsWork, 0x4C);

/// The 0xA8-byte block `func_actor_560800_80136AA8` pushes on the scratchpad
/// stack (`0x1F8003FC`). `chain` is the rotation accumulated down the part
/// chain, `link` that rotation times the current part's, and `joint` the next
/// part's translation carried through them; `pos` sums the joints from the root
/// and `ang` the parts' rotations, with `aim` their sum against the current
/// part. `rot` is the working copy of `Actor560800ModelWork::rot`, copied in and
/// back out around the walk.
typedef struct Actor560800ChainScratch {
    /* 0x00 */ MATRIX  chain;
    /* 0x20 */ MATRIX  link;
    /* 0x40 */ SVECTOR pos;
    /* 0x48 */ SVECTOR ang;
    /* 0x50 */ SVECTOR aim;
    /* 0x58 */ SVECTOR joint;
    /* 0x60 */ byte    pad_60[0x10];
    /* 0x70 */ SVECTOR rot[7];
} Actor560800ChainScratch;
STATIC_ASSERT_SIZEOF(Actor560800ChainScratch, 0xA8);

/// One 0x18-byte part pose: a root position and the root rotation.
/// `func_actor_560800_80137F58` loads one table of eight (one per part) into
/// the parts - `D_actor_560800_80175314` / `801753D4` / `80175494` by
/// `Actor560800PartsWork::field_46`, `80175554` as offsets from the message
/// position, `80175614` for the teardown - and `func_actor_560800_801386D4`
/// clamps the part's `Actor560800ModelWork::field_256` to `pos.vy` while it
/// grows it by the matching `D_actor_560800_801756EC` step.
typedef struct Actor560800PartPose {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor560800PartPose;
STATIC_ASSERT_SIZEOF(Actor560800PartPose, 0x18);

extern Actor560800PartPose D_actor_560800_80175314[];
extern Actor560800PartPose D_actor_560800_801753D4[];
extern Actor560800PartPose D_actor_560800_80175494[];
extern Actor560800PartPose D_actor_560800_80175554[];
extern Actor560800PartPose D_actor_560800_80175614[];

/// Word-wise view of a `MATRIX`, which lets an identity rotation be written
/// as four `sw` and one `sh` instead of nine halfword stores.
typedef union Actor560800MatWords {
    MATRIX mat;
    struct {
        /* 0x00 */ s32 m00_m01;
        /* 0x04 */ s32 m02_m10;
        /* 0x08 */ s32 m11_m12;
        /* 0x0C */ s32 m20_m21;
        /* 0x10 */ s16 m22;
    } ident;
} Actor560800MatWords;
STATIC_ASSERT_SIZEOF(Actor560800MatWords, 0x20);

/// Payload `func_actor_560800_8013631C` passes as `Gp_DispatchMsg`'s `arg2` for
/// message 0x7DB: the same 4-byte record the other actors send, whose halfword
/// at 0x2 carries the value the receiver reads.
typedef struct Actor560800Msg {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} Actor560800Msg;
STATIC_ASSERT_SIZEOF(Actor560800Msg, 0x4);

/// Controller task of this overlay, published by `func_actor_560800_80135BD8`
/// and read by the sub-task handlers.
extern Task* D_actor_560800_8017578C;

/// Animation block `func_actor_560800_80136378` points the `field_0` of its
/// `GpAnimArg` at when it sends message 0x3F4 - the same role
/// `D_actor_400600_80151A48` plays in that overlay.
extern u8 D_actor_560800_8016EA40[];

extern Actor560800AnimStep D_actor_560800_8016EBE8[];

/// Animation bank `func_actor_560800_801376E0` hands `func_800B3F84` as its
/// second argument: a null entry then one animation set per slot of
/// `Actor560800ModelWork`, indexed by the animation id.
extern GpAnimSet* D_actor_560800_801752F0[];

/// The game's shared 32-bit LCG state - same global the other actors draw from.
extern u32 Gp_LcgState;

/// Elapsed frames, one per phase id 1..3, written by
/// `func_actor_560800_80135AEC` as the frames since that phase's timestamp.
/// The counter has wrapped if the timestamp is ahead of `Display_State.field_0`,
/// which is the one case the elapsed count is short by one.
extern s32 D_actor_560800_80175790;
extern s32 D_actor_560800_80175794;
extern s32 D_actor_560800_80175798;

/// Phase timestamps, one per phase id 1..3: `func_actor_560800_80136930`
/// stamps `Display_State.field_0` (the frame counter) into the slot its argument
/// selects, and `func_actor_560800_80135AEC` reads it back per phase and stores
/// the elapsed frames in the matching slot of `D_actor_560800_80175790`.
extern s32 D_actor_560800_8017579C;
extern s32 D_actor_560800_801757A0;
extern s32 D_actor_560800_801757A4;

/// Seed `func_actor_560800_80135D54` loads into `Gp_LcgState` before it hands
/// control back to gameplay.
extern u32 D_actor_560800_801757A8;

/// Pair of blocks `func_actor_560800_80135D54` passes to `func_800E8634`.
extern s32 D_actor_560800_8016F5E0;
extern s32 D_actor_560800_80171800;

/// Flag word whose bit 0 gates `func_actor_560800_80138BCC`'s sink step.
extern s32 D_actor_560800_801752E8;

/// Frame counter `func_actor_560800_80138FC8` raises by one per tick.
extern s32 D_actor_560800_801752EC;

extern Task* D_actor_560800_801757AC;

extern u8      D_80071075;
extern s8      D_8007218A;
extern MATRIX* D_80073B8C[1];
extern u8      D_80073BA9;
extern s8      D_80114C12;

void func_actor_560800_80133970(Task* arg0);
void func_actor_560800_80134258(Task* arg0);
void func_actor_560800_80134384(Task* arg0);
void func_actor_560800_80134BFC(Task* arg0);

#endif
