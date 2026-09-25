#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include "psyq/abs.h"

#include "actors/actor.h"
#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// Work block this overlay hangs off the task's `Task::work` slot (0x1C),
/// which is not a `TaskIdMap` here. Reach it with
/// `(Actor560800Work*)task->work`.
///
/// `func_actor_560800_80135BD8` allocates it with `Mem_Malloc(0x68, 0)`, so the
/// size below is the allocation and not a guess, and fills the first slots with
/// the sub-tasks from `gameGetPtrSlot(3)` and `D_actor_560800_801718F0`
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
    /* 0x00 */ Task* field_0; // gameGetPtrSlot(3)
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
    /* 0x34 */ s16   field_34;
    /* 0x36 */ byte  pad_36[2];
    /* 0x38 */ s16   field_38;
    /* 0x3A */ s16   field_3A;
    /* 0x3C */ byte  pad_3C[4];
    /* 0x40 */ s16   field_40;
    /* 0x42 */ s16   field_42;
    /* 0x44 */ s16   field_44;
    /* 0x46 */ byte  pad_46[0x12];
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
/// from `D_actor_560800_801718F0` index 5 (`func_actor_560800_80132C60`).
/// That function allocates it with `Mem_Malloc(0x4CC, 0)`, `Mem_Set`s the same
/// 0x4CC bytes and stores it in its own `Task::work` (0x1C), so the size below
/// is the allocation, not a guess. It is a third work block in this overlay,
/// distinct from `Actor560800Work` and `OverlayFadeWork`.
///
/// `anim` is the animation context the block itself is handed to
/// `Gp_AnimResetSlot` as, laid out the way every actor carries it: the context
/// at 0, its 0x14 slots at +0x14 and the 0x90-byte scratch `func_800B3F84`
/// takes as its `arg3` at +0x334. `func_actor_560800_80132C60` passes exactly
/// `block`, `block + 0x334` and `block + 0x14` to that call and then stores
/// 0x14 in `field_4BA`, which is why the slots array is sized 0x14 and the
/// scratch sits where it does.
///
/// `field_4B8` is the animation id the slots are seeded with, `field_4BA` the
/// slot count the reset loop walks 1..count, and `field_4C8` the 0x10 written into each
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
/// and stores in its own `Task::work` (0x1C), so the size below is the
/// allocation, not a guess. A fourth work block in this overlay, distinct from
/// `Actor560800Work`, `Actor560800AnimWork` and `OverlayFadeWork`, and the
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

/// Work block of the message-handler task whose `Task::msgTable` table is
/// `D_actor_560800_801756D4`: `func_actor_560800_801386D4` allocates it with
/// `Mem_Malloc(0x4C, 0)`, `Mem_Set`s the same 0x4C bytes and stores it in that
/// task's `Task::work` (0x1C), so the size below is the allocation, not a
/// guess. A fifth work block in this overlay, distinct from `Actor560800Work`,
/// `Actor560800AnimWork`, `Actor560800ModelWork` and `OverlayFadeWork`.
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

/// Controller task of this overlay, published by `func_actor_560800_80135BD8`
/// and read by the sub-task handlers.
extern Task* D_actor_560800_8017578C;

/// Animation block `func_actor_560800_80136378` points the `animBlock` of its
/// `GpAnimArg` at when it sends message 0x3F4 - the same role
/// `D_actor_400600_80151A48` plays in that overlay.
extern u8 D_actor_560800_8016EA40[];

extern Actor560800AnimStep D_actor_560800_8016EBE8[];
extern s32                 D_actor_560800_8016F1CC[];

/// Animation bank `func_actor_560800_801376E0` hands `func_800B3F84` as its
/// second argument: a null entry then one animation set per slot of
/// `Actor560800ModelWork`, indexed by the animation id.
extern GpAnimSet* D_actor_560800_801752F0[];

/// Elapsed frames, one per phase id 1..3, written by
/// `func_actor_560800_80135AEC` as the frames since that phase's timestamp.
/// The counter has wrapped if the timestamp is ahead of `gDisplayState.frameCount`,
/// which is the one case the elapsed count is short by one.
extern s32 D_actor_560800_80175790;
extern s32 D_actor_560800_80175794;
extern s32 D_actor_560800_80175798;

/// Phase timestamps, one per phase id 1..3: `func_actor_560800_80136930`
/// stamps `gDisplayState.frameCount` (the frame counter) into the slot its argument
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

/// Task descriptor table the actor spawns most of its sub-tasks from, by
/// index.
extern TaskDesc D_actor_560800_801718F0;

extern u8      D_80071075;
extern s8      D_8007218A;
extern MATRIX* D_80073B8C[1];
extern u8      D_80073BA9;
extern s8      D_80114C12;

void func_actor_560800_80133970(Task* arg0);
void func_actor_560800_80134258(Task* arg0);
void func_actor_560800_80134384(Task* arg0);
void func_actor_560800_80134BFC(Task* arg0);

extern s8       D_8007106B;
extern TaskDesc D_actor_560800_8016EA28;
extern TaskDesc D_actor_560800_8017575C;
extern void     D_actor_560800_8016EA74;
extern void     D_actor_560800_8016EB04;
extern void     D_actor_560800_8016EB30;
extern void     D_actor_560800_8016EC1C;
extern void     D_actor_560800_8016ECAC;
extern void     D_actor_560800_8016ECC4;
extern void     D_actor_560800_8016F154;
extern void     D_actor_560800_8016F34C;
extern s32      D_actor_560800_8016F57C[];

s32 func_actor_560800_80132498(Task* arg0);

/// Declared locally with a signed `arg2`; see the note in `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_8017F450(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);

void func_actor_560800_801321A0(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    s16         slot;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key = gGameSession->at4;
    if (task->spawnArg1 != 0) {
        key.loc.view = 0x65;
    } else {
        key.loc.view = 0x64;
    }
    slot = Stream_FindSlot(key.raw.data, 0, 0);
    {
        register s32 cmd asm("a0");
        register s32 zero asm("a1");
        register u8* p asm("a2");
        cmd  = 0x61;
        zero = 0;
        p    = slotParam;
        SOFT_TOUCH_REG4(cmd, zero, p, slot);
        slotParam[0] = slot;
        CdCmd_Enqueue(cmd, zero, p);
    }
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    taskKill(task);
    Display_ResetHeapWrapper();
}

s32 func_actor_560800_80132340(Task* arg0)
{
    Actor560800Work*     work;
    Actor560800AnimStep* table;
    Actor560800AnimStep* entry;
    Actor560800AnimStep* entry2;
    GpAnimArg            msg;
    u16                  anim;
    u16                  anim2;

    work = (Actor560800Work*)arg0->work;
    if (work->field_0 == NULL) {
        return 1;
    }
    table = D_actor_560800_8016EBE8;
    entry = &table[(u16)work->field_60];
    if (entry->field_0 != 0) {
        if (work->field_62 >= entry->field_0) {
            if (entry->field_2 < 0) {
                return 1;
            }
            anim              = entry->field_2;
            msg.animBlock.ptr = D_actor_560800_8016EA40;
            work->field_60    = anim;
            msg.field_4       = anim;
            msg.field_8       = 1;
            msg.field_C       = 0xA;
            msg.field_10      = 1;
            Gp_DispatchMsg(work->field_0, 0x3F4, (s32)&msg, 0);
            work->field_62 = 0;
        } else {
            work->field_62 += 1;
        }
    } else {
        if (Gp_DispatchMsg(work->field_0, 0x3ED, 0, 0) != 0) {
            return 0;
        }
        entry2 = &D_actor_560800_8016EBE8[(u16)work->field_60];
        if (entry2->field_2 < 0) {
            return 1;
        }
        work = (Actor560800Work*)arg0->work;
        if (work->field_0 != NULL) {
            anim2             = entry2->field_2;
            msg.animBlock.ptr = D_actor_560800_8016EA40;
            work->field_60    = anim2;
            msg.field_4       = anim2;
            msg.field_8       = 1;
            msg.field_C       = 0xA;
            msg.field_10      = 1;
            Gp_DispatchMsg(work->field_0, 0x3F4, (s32)&msg, 0);
            work->field_62 = 0;
        }
    }
    return 0;
}

/// Reseeds the animation slots of the task's own `Actor560800AnimWork`: the
/// id goes to `field_4B8` with `rate` in `field_4C8`, `field_4BE` is cleared,
/// and slots 1..`field_4BA` are blended through `func_800B4114`. The
/// `SOFT_BARRIER()` is the same sched1 pin `func_actor_560800_801364A0` needs.
static inline void Actor560800_ReseedAnim(Task* arg0, u16 id, s16 rate)
{
    Actor560800AnimWork* w;
    u16                  i;

    w            = (Actor560800AnimWork*)arg0->work;
    w->field_4B8 = id;
    w->field_4C8 = rate;
    w->field_4BE = 0;
    SOFT_BARRIER();
    for (i = 1; i < w->field_4BA; i++) {
        func_800B4114(&w->anim, i, id, 0, 10);
    }
}

/// Ticks every animation slot, then advances the script at `field_4B4`: a step
/// with a non-zero hold waits `field_0` frames in `field_4BE`, a zero hold waits
/// for every slot to finish (bit 0x100 of the slot's `flags`). Returns 1 when the next
/// step's id is negative (the script ended), 0 otherwise.
///
/// The step is re-indexed at every use rather than held in a local, and the
/// negative test is written as `>= 0` with an `else return 1`; both are needed
/// for the register choice and the jump layout.
s32 func_actor_560800_80132498(Task* arg0)
{
    Actor560800AnimWork* work;
    u16                  i;
    u16                  done;

    work = (Actor560800AnimWork*)arg0->work;
    if (((TmdObject*)arg0->extra)->flags & 0x80) {
        return 0;
    }
    for (i = 1; i < work->field_4BA; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    i    = 1;
    done = 1;
    for (; i < work->field_4BA; i++) {
        if (!(work->slots[i].flags & 0x100)) {
            done = 0;
            break;
        }
    }
    if (work->field_4B4[(u16)work->field_4B8].field_0 != 0) {
        if ((u16)work->field_4BE >= work->field_4B4[(u16)work->field_4B8].field_0) {
            if (work->field_4B4[(u16)work->field_4B8].field_2 >= 0) {
                Actor560800_ReseedAnim(arg0, work->field_4B4[(u16)work->field_4B8].field_2, work->field_4C8);
            } else {
                return 1;
            }
        } else {
            work->field_4BE++;
        }
    } else if (done) {
        if (work->field_4B4[(u16)work->field_4B8].field_2 >= 0) {
            Actor560800_ReseedAnim(arg0, work->field_4B4[(u16)work->field_4B8].field_2, work->field_4C8);
        } else {
            return 1;
        }
    }
    return 0;
}

/// Spawn handler of the floor-quad model task: state 0 allocates its
/// `Actor560800AnimWork`, seeds animation 0 (or 2 when `spawnArg1` is set),
/// and state 1 sends message 0x7D4 once when `spawnArg1` is 1. Every frame
/// draws the floor quad and ticks the animation script.
void func_actor_560800_801326C4(Task* arg0)
{
    Actor560800AnimWork* work = (Actor560800AnimWork*)arg0->work;
    SVECTOR              ofs;
    VECTOR               pos;

    switch (arg0->state) {
        case 0: {
            u16 failed;
            {
                TmdObject*           tmd   = arg0->extra;
                GsCOORDINATE2*       coord = tmd->coords;
                Actor560800AnimWork* block = Mem_Malloc(0x4CC, 0);
                GpAreaPlace*         place;
                u8                   id;

                arg0->work = (TaskIdMap*)block;
                if (block == NULL) {
                    failed = 1;
                } else {
                    coord->sub = &gGfxViewCoord;
                    Mem_Set(arg0->work, 0, 0x4CC);
                    tmd->lightMtx  = &block->light;
                    tmd->colorMtx  = &block->color;
                    arg0->msgTable = &D_actor_560800_8016F34C;
                    place          = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
                    id             = place->entryId;
                    while (id != 0xFF) {
                        if (id == 0x83) {
                            break;
                        }
                        place++;
                        id = place->entryId;
                    }
                    Gp_SetTmdBytes((TmdObject*)arg0->extra, (s8)place->tpage, (s8)place->clut);
                    Task_Reparent(D_actor_560800_8017578C, arg0);
                    failed = 0;
                }
            }
            if (failed) {
                taskKill(arg0);
                return;
            }
            ((TmdObject*)arg0->extra)->flags &= ~0x84;
            work                              = (Actor560800AnimWork*)arg0->work;
            {
                TmdObject* obj = arg0->extra;
                func_800B3F84(&work->anim, &D_actor_560800_8016EB04, obj, work->animAux, work->slots);
            }
            work->field_4BA = 0x13;
            work->field_4B4 = &D_actor_560800_8016ECAC;
            if (arg0->spawnArg1 == 0) {
                Actor560800AnimWork* w = (Actor560800AnimWork*)arg0->work;
                u16                  i;
                s32                  fade = 0x10;

                w->field_4B8 = 0;
                w->field_4C8 = fade;
                w->field_4BE = 0;
                for (i = 1; i < w->field_4BA; i++) {
                    w->slots[i].rate = fade;
                    Gp_AnimResetSlot(&w->anim, i, 0);
                }
            } else {
                Actor560800AnimWork* w = (Actor560800AnimWork*)arg0->work;
                u16                  i;
                s32                  fade = 0x10;

                w->field_4B8 = 2;
                w->field_4C8 = fade;
                w->field_4BE = 0;
                for (i = 1; i < w->field_4BA; i++) {
                    w->slots[i].rate = fade;
                    Gp_AnimResetSlot(&w->anim, i, 2);
                }
            }
            arg0->state += 1;
            break;
        }
        case 2: // an empty case: GCC then roots the case tree at 1
            break;
        case 1: {
            s32 arg = arg0->spawnArg1;
            if (arg == 1) {
                Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_560800_8016F154, 0);
                work->field_4BC = arg;
                arg0->state    += 1;
            }
        } break;
    }
    ofs.vx = 0;
    ofs.vy = 0x380;
    ofs.vz = 0;
    Gp_DrawFloorQuad(&((TmdObject*)arg0->extra)->coords[1], 0x300, &ofs);
    func_actor_560800_80132498(arg0);
    if (work->field_4BC != 0) {
        TmdObject* obj = arg0->extra;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
        pos.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
}

void func_actor_560800_80132A14(Task* arg0)
{
    Actor560800AnimWork* work = (Actor560800AnimWork*)arg0->work;
    VECTOR               pos;

    if (arg0->state == 0) {
        TmdObject*           tmd    = arg0->extra;
        Task*                parent = arg0->spawnArg2;
        GsCOORDINATE2*       coord  = tmd->coords;
        Actor560800AnimWork* block;
        GpAreaPlace*         place;
        u8                   id;

        block      = Mem_Malloc(0x4CC, 0);
        arg0->work = (TaskIdMap*)block;
        if (block == NULL) {
            taskKill(arg0);
            return;
        }
        work = block;
        switch (arg0->spawnArg1) {
            case 0:
                coord->sub = &((TmdObject*)parent->extra)->coords[12];
                break;
            case 1:
            case 2:
            case 3:
                coord->sub = &((TmdObject*)parent->extra)->coords[8];
                break;
        }
        Mem_Set(arg0->work, 0, 0x4CC);
        tmd->lightMtx = &work->light;
        tmd->colorMtx = &work->color;
        if (arg0->spawnArg1 < 2) {
            place = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
            id    = place->entryId;
            while (id != 0xFF) {
                if (id == 0x65) {
                    break;
                }
                place++;
                id = place->entryId;
            }
            Gp_SetTmdBytes((TmdObject*)arg0->extra, (s8)place->tpage, (s8)place->clut);
        } else if (arg0->spawnArg1 == 2) {
            Gp_SetTmdBytes((TmdObject*)arg0->extra, 0, 0);
        } else if (arg0->spawnArg1 == 3) {
            place = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
            id    = place->entryId;
            while (id != 0xFF) {
                if (id == 0x22) {
                    break;
                }
                place++;
                id = place->entryId;
            }
            Gp_SetTmdBytes((TmdObject*)arg0->extra, (s8)place->tpage, (s8)place->clut);
        }
        Task_Reparent(parent, arg0);
        arg0->msgTable = &D_actor_560800_8016F34C;
        arg0->state   += 1;
        return;
    }
    if (work->field_4BC != 0) {
        TmdObject* obj = arg0->extra;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
        pos.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
}

void func_actor_560800_80132C60(Task* arg0)
{
    Actor560800AnimWork* work = (Actor560800AnimWork*)arg0->work;
    SVECTOR              ofs;
    VECTOR               pos;

    if (arg0->state == 0) {
        u16 failed;
        {
            TmdObject*           tmd   = arg0->extra;
            GsCOORDINATE2*       coord = tmd->coords;
            Actor560800AnimWork* block = Mem_Malloc(0x4CC, 0);
            GpAreaPlace*         place;
            u8                   id;

            arg0->work = (TaskIdMap*)block;
            if (block == NULL) {
                failed = 1;
            } else {
                coord->sub = &gGfxViewCoord;
                Mem_Set(arg0->work, 0, 0x4CC);
                tmd->lightMtx  = &block->light;
                tmd->colorMtx  = &block->color;
                arg0->msgTable = &D_actor_560800_8016F34C;
                place          = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
                id             = place->entryId;
                while (id != 0xFF) {
                    if (id == 0x65) {
                        break;
                    }
                    place++;
                    id = place->entryId;
                }
                Gp_SetTmdBytes((TmdObject*)arg0->extra, (s8)place->tpage, (s8)place->clut);
                Task_Reparent(D_actor_560800_8017578C, arg0);
                failed = 0;
            }
        }
        if (failed) {
            taskKill(arg0);
            return;
        }
        work = (Actor560800AnimWork*)arg0->work;
        {
            TmdObject* obj = arg0->extra;
            func_800B3F84(&work->anim, &D_actor_560800_8016EA74, obj, work->animAux, work->slots);
        }
        work->field_4BA = 0x14;
        work->field_4B4 = &D_actor_560800_8016EC1C;
        {
            Actor560800AnimWork* w = (Actor560800AnimWork*)arg0->work;
            u16                  i;
            s32                  fade = 0x10;

            w->field_4B8 = 0;
            w->field_4C8 = fade;
            w->field_4BE = 0;
            for (i = 1; i < w->field_4BA; i++) {
                w->slots[i].rate = fade;
                Gp_AnimResetSlot(&w->anim, i, 0);
            }
        }
        arg0->state += 1;
    }
    ofs.vx = 0;
    ofs.vy = 0x380;
    ofs.vz = 0;
    Gp_DrawFloorQuad(&((TmdObject*)arg0->extra)->coords[1], 0x300, &ofs);
    if (work->field_4CA == 0) {
        func_actor_560800_80132498(arg0);
    }
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords[4].coord, work->field_4C0, 0);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[4].coord, work->field_4C6, 0);
    Gfx_RotMatrixZ(&((TmdObject*)arg0->extra)->coords[2].coord, work->field_4C4, 0);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_4CA != 0) {
        work->field_4C0 = 0;
        work->field_4C6 = 0;
        work->field_4C4 = 0;
    }
    if (work->field_4BC != 0) {
        TmdObject* obj = arg0->extra;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
        pos.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
}

void func_actor_560800_80132F64(Task* arg0)
{
    Actor560800AnimWork* work = (Actor560800AnimWork*)arg0->work;
    SVECTOR              ofs;
    VECTOR               pos;

    if (arg0->state == 0) {
        u16 failed;
        {
            TmdObject*           tmd   = arg0->extra;
            GsCOORDINATE2*       coord = tmd->coords;
            Actor560800AnimWork* block = Mem_Malloc(0x4CC, 0);
            GpAreaPlace*         place;
            u8                   id;

            arg0->work = (TaskIdMap*)block;
            if (block == NULL) {
                failed = 1;
            } else {
                coord->sub = &gGfxViewCoord;
                Mem_Set(arg0->work, 0, 0x4CC);
                tmd->lightMtx  = &block->light;
                tmd->colorMtx  = &block->color;
                arg0->msgTable = &D_actor_560800_8016F34C;
                place          = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
                id             = place->entryId;
                while (id != 0xFF) {
                    if (id == 0x22) {
                        break;
                    }
                    place++;
                    id = place->entryId;
                }
                Gp_SetTmdBytes((TmdObject*)arg0->extra, (s8)place->tpage, (s8)place->clut);
                Task_Reparent(D_actor_560800_8017578C, arg0);
                failed = 0;
            }
        }
        if (failed) {
            taskKill(arg0);
            return;
        }
        work = (Actor560800AnimWork*)arg0->work;
        {
            TmdObject* obj = arg0->extra;
            func_800B3F84(&work->anim, &D_actor_560800_8016EB30, obj, work->animAux, work->slots);
        }
        work->field_4BA = 0x13;
        work->field_4B4 = &D_actor_560800_8016ECC4;
        {
            Actor560800AnimWork* w = (Actor560800AnimWork*)arg0->work;
            u16                  i;
            s32                  fade = 0x10;

            w->field_4B8 = 0;
            w->field_4C8 = fade;
            w->field_4BE = 0;
            for (i = 1; i < w->field_4BA; i++) {
                w->slots[i].rate = fade;
                Gp_AnimResetSlot(&w->anim, i, 0);
            }
        }
        arg0->state += 1;
    }
    if (!(((TmdObject*)arg0->extra)->flags & 0x80) && work->field_4C2 == 0) {
        ofs.vx = 0;
        ofs.vy = 0x380;
        ofs.vz = 0;
        Gp_DrawFloorQuad(&((TmdObject*)arg0->extra)->coords[1], 0x300, &ofs);
    }
    func_actor_560800_80132498(arg0);
    if (work->field_4BC != 0) {
        TmdObject* obj = arg0->extra;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
        pos.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
}

void func_actor_560800_80133204(void)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;
    Task*            task;
    VECTOR           pos;

    task = work->field_4;
    if (task != NULL) {
        TmdObject* obj = task->extra;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
        pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    task = work->field_8;
    if (task != NULL) {
        TmdObject* obj = task->extra;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
        pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    task = work->field_10;
    if (task != NULL) {
        TmdObject* obj = task->extra;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
        pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    task = work->field_14;
    if (task != NULL) {
        TmdObject* obj = task->extra;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
        pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    task = work->field_18;
    if (task != NULL) {
        TmdObject* obj = task->extra;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
        pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    task = work->field_C;
    if (task != NULL) {
        TmdObject* obj = task->extra;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
        pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    task = work->field_1C;
    if (task != NULL) {
        TmdObject* obj = task->extra;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
        pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    if (work->field_20 != NULL) {
        Actor560800Work* w = (Actor560800Work*)D_actor_560800_8017578C->work;

        ((SVECTOR*)&pos)->vy = 0;
        Gp_DispatchMsg(w->field_20, 0x7DB, (s32)&pos, 0);
    }
}

void func_actor_560800_80133540(u32 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;

    switch (arg0) {
        case 0:
            Gp_DispatchMsg(work->field_0, 0x3F3, 1, 0);
            break;
        case 1:
            Gp_DispatchMsg(work->field_4, 0x7D5, 1, 0);
            break;
        case 2:
            Gp_DispatchMsg(work->field_8, 0x7D5, 1, 0);
            Gp_DispatchMsg(work->field_10, 0x7D5, 1, 0);
            Gp_DispatchMsg(work->field_14, 0x7D5, 1, 0);
            if (work->field_18 != NULL) {
                Gp_DispatchMsg(work->field_18, 0x7D5, 1, 0);
            }
            break;
        case 3:
            Gp_DispatchMsg(work->field_C, 0x7D5, 1, 0);
            if (work->field_1C != NULL) {
                Gp_DispatchMsg(work->field_1C, 0x7D5, 1, 0);
            }
            break;
        case 4:
            Gp_DispatchMsg(work->field_20, 0x7D5, 1, 0);
            break;
        case 5:
            Gp_DispatchMsg(work->field_24, 0x7D5, 1, 0);
            break;
    }
}

void func_actor_560800_80133648(u32 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;

    switch (arg0) {
        case 0:
            Gp_DispatchMsg(work->field_0, 0x3F3, 2, 0);
            break;
        case 1:
            Gp_DispatchMsg(work->field_4, 0x7D5, 2, 0);
            break;
        case 2:
            Gp_DispatchMsg(work->field_8, 0x7D5, 2, 0);
            Gp_DispatchMsg(work->field_10, 0x7D5, 2, 0);
            Gp_DispatchMsg(work->field_14, 0x7D5, 2, 0);
            if (work->field_18 != NULL) {
                Gp_DispatchMsg(work->field_18, 0x7D5, 2, 0);
            }
            break;
        case 3:
            Gp_DispatchMsg(work->field_C, 0x7D5, 2, 0);
            if (work->field_1C != NULL) {
                Gp_DispatchMsg(work->field_1C, 0x7D5, 2, 0);
            }
            break;
        case 4:
            Gp_DispatchMsg(work->field_20, 0x7D5, 2, 0);
            break;
        case 5:
            Gp_DispatchMsg(work->field_24, 0x7D5, 2, 0);
            break;
    }
}

extern s32* D_actor_560800_8016F35C[];
extern s32* D_actor_560800_8016F3E4[];
extern s32* D_actor_560800_8016F46C[];
extern s32* D_actor_560800_8016F4F4[];

void func_actor_560800_80133750(s32 arg0)
{
    Actor560800Work* work;
    s32*             msg;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    if (work->field_66 == 0) {
        Gp_PulseState1C();
    }
    if (work->field_0 != NULL) {
        msg = D_actor_560800_8016F35C[arg0];
        if (*msg != 0) {
            func_actor_560800_80133540(0);
            Gp_DispatchMsg(work->field_0, 0x3E9, (s32)msg, 0);
        } else {
            func_actor_560800_80133648(0);
        }
    }
    if (work->field_8 != NULL) {
        msg = D_actor_560800_8016F46C[arg0];
        if (*msg != 0) {
            func_actor_560800_80133540(2);
            Gp_DispatchMsg(work->field_8, 0x7D4, (s32)msg, 0);
        } else {
            func_actor_560800_80133648(2);
        }
    }
    if (work->field_C != NULL) {
        msg = D_actor_560800_8016F3E4[arg0];
        if (*msg != 0) {
            func_actor_560800_80133540(3);
            Gp_DispatchMsg(work->field_C, 0x7D4, (s32)msg, 0);
        } else {
            func_actor_560800_80133648(3);
        }
    }
    if (work->field_4 != NULL) {
        msg = D_actor_560800_8016F4F4[arg0];
        if (*msg != 0) {
            func_actor_560800_80133540(1);
            Gp_DispatchMsg(work->field_4, 0x7D4, (s32)msg, 0);
        } else {
            func_actor_560800_80133648(1);
        }
    }
    if (work->field_20 != NULL) {
        // No table of its own: reuses the payload picked for field_4.
        if (*msg != 0) {
            func_actor_560800_80133540(4);
            Gp_DispatchMsg(work->field_20, 0x7D4, (s32)msg, 0);
        } else {
            func_actor_560800_80133648(4);
        }
    }
}

static inline void Actor560800_PlayAnim(Task* task, u16 anim)
{
    Actor560800Work* work;
    GpAnimArg        msg;

    work = (Actor560800Work*)task->work;
    if (work->field_0 != NULL) {
        msg.animBlock.ptr = D_actor_560800_8016EA40;
        work->field_60    = anim;
        msg.field_4       = anim;
        msg.field_8       = 0;
        msg.field_C       = 0;
        msg.field_10      = 1;
        Gp_DispatchMsg(work->field_0, 0x3F4, (s32)&msg, 0);
        work->field_62 = 0;
    }
}

static inline void Actor560800_PlayAnimB(Task* task, u16 anim, s32 argC)
{
    Actor560800Work* work;
    GpAnimArg        msg;

    work = (Actor560800Work*)task->work;
    if (work->field_0 != NULL) {
        msg.animBlock.ptr = D_actor_560800_8016EA40;
        work->field_60    = anim;
        msg.field_4       = anim;
        msg.field_8       = 1;
        msg.field_C       = argC;
        msg.field_10      = 1;
        Gp_DispatchMsg(work->field_0, 0x3F4, (s32)&msg, 0);
        work->field_62 = 0;
    }
}

static inline void Actor560800_PlaySe(s16 arg4)
{
    s32 msg[5];
    s32 val;

    val    = D_80073BA9;
    msg[0] = (D_8007218A == 1) ? val + 1 : val + 0x22;
    msg[1] = arg4;
    msg[2] = 0;
    msg[3] = 0;
    msg[4] = 0;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)msg, 0);
}

static inline void Actor560800_PlaySeB(s32 arg4)
{
    s32 msg[5];
    s32 val;

    val    = D_80073BA9;
    msg[0] = (D_8007218A == 1) ? val + 1 : val + 0x22;
    msg[1] = arg4;
    msg[2] = 1;
    msg[3] = 0xA;
    msg[4] = 0;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)msg, 0);
}

static inline void Actor560800_SpawnSparksA(Task* task)
{
    Actor560800Work* work;
    SVECTOR          vec;

    work   = (Actor560800Work*)task->work;
    vec.vx = 0x12C;
    vec.vy = 0;
    vec.vz = -0x1F4;
    Gp_SpawnEff(0x60046, ((TmdObject*)work->field_0->extra)->coords, 0x20000040, &vec);
    vec.vx = 0x190;
    vec.vy = 0;
    vec.vz = -0x258;
    Gp_SpawnEff(0x60046, ((TmdObject*)work->field_0->extra)->coords, 0x20000030, &vec);
    vec.vx = 0x12C;
    vec.vy = 0;
    vec.vz = -0x2BC;
    Gp_SpawnEff(0x60046, ((TmdObject*)work->field_0->extra)->coords, 0x20000020, &vec);
    vec.vx = 0x1C2;
    vec.vy = 0;
    vec.vz = -0x320;
    Gp_SpawnEff(0x60046, ((TmdObject*)work->field_0->extra)->coords, 0x20000020, &vec);
}

static inline void Actor560800_SpawnSparksB(Task* task)
{
    Actor560800Work* work;
    SVECTOR          vec;

    work   = (Actor560800Work*)task->work;
    vec.vx = 0x12C;
    vec.vy = 0;
    vec.vz = -0xC8;
    Gp_SpawnEff(0x60046, ((TmdObject*)work->field_0->extra)->coords, 0x20000040, &vec);
    vec.vx = 0x1F4;
    vec.vy = 0;
    vec.vz = -0x64;
    Gp_SpawnEff(0x60046, ((TmdObject*)work->field_0->extra)->coords, 0x20000020, &vec);
    vec.vx = 0x1C2;
    vec.vy = 0;
    vec.vz = 0;
    Gp_SpawnEff(0x60046, ((TmdObject*)work->field_0->extra)->coords, 0x20000020, &vec);
}

/// Requests driven by `field_28`, cleared once handled: the inline helpers play
/// an animation on the task at `field_0` (0x3F4), post a sound through
/// `gameGetPtrSlot(3)` (0x3E8) or spawn the 0x60046 spark effects on its part
/// coordinates. 18 and 35 are two-step sequences on `field_2A` / `field_2C`.
///
/// Shape notes, all needed for the match: helpers take only the arguments that
/// vary, because an inlined parameter is copied to a pseudo even when constant
/// and CSE would then share it; `D_80073B8C` is read as an array so the load is
/// in-struct and schedules after the `field_2C` store; the explicit clears in 19,
/// 28 and the last step of 35 decide which anim tails cross-jump together.
void func_actor_560800_80133970(Task* arg0)
{
    Actor560800Work* work;

    work = (Actor560800Work*)arg0->work;
    func_actor_560800_80132340(arg0);
    switch ((u16)work->field_28) {
        case 0:
        case 38:
            break;
        case 1:
            Actor560800_PlaySe(1);
            break;
        case 3:
            Actor560800_PlaySeB(7);
            break;
        case 9:
            Actor560800_PlayAnimB(arg0, 0, 0xA);
            break;
        case 12:
            Actor560800_PlaySe(9);
            break;
        case 16:
            Actor560800_PlayAnim(arg0, 0xC);
            break;
        case 18:
            switch ((u16)work->field_2A) {
                case 0:
                    Actor560800_PlaySe(3);
                    Gp_DispatchMsg(work->field_0, 0x3FD, 8, 0);
                    work->field_2C = 0;
                    work->field_2A++;
                    return;
                case 1:
                    if (work->field_2C < 100) {
                        work->field_2C      += 5;
                        D_80073B8C[0]->t[0] -= 5;
                        return;
                    }
                    Actor560800_PlayAnimB(arg0, 0xC, 0xA);
                    break;
                default:
                    return;
            }
            break;
        case 19:
            Actor560800_PlayAnim(arg0, 1);
            work->field_28 = 0;
            return;
        case 21:
            Actor560800_PlayAnim(arg0, 3);
            Actor560800_SpawnSparksA(arg0);
            work->field_66 = 1;
            break;
        case 28:
            Actor560800_PlayAnim(arg0, 4);
            work->field_28 = 0;
            return;
        case 29:
            Actor560800_SpawnSparksA(arg0);
            Actor560800_SpawnSparksB(arg0);
            work->field_66 = 1;
            break;
        case 22:
        case 32:
            work->field_66 = 0;
            break;
        case 33:
            Actor560800_SpawnSparksA(arg0);
            Actor560800_SpawnSparksB(arg0);
            work->field_66 = 1;
            Actor560800_PlayAnim(arg0, 5);
            break;
        case 35:
            switch ((u16)work->field_2A) {
                case 0:
                    Actor560800_PlaySeB(8);
                    Gp_DispatchMsg(work->field_0, 0x3FD, 8, 0);
                    work->field_2C = 0;
                    work->field_2A++;
                    return;
                case 1:
                    if (++work->field_2C < 11) {
                        return;
                    }
                    Actor560800_PlayAnimB(arg0, 0xC, 0x1E);
                    work->field_28 = 0;
                    return;
                default:
                    return;
            }
            break;
    }
    work->field_28 = 0;
}

/// Handles the pending request in `field_38` and clears it: 1 and 28 reset the
/// animation sub-task's `field_4C0` / `field_4CA`, 28 also reseeds its slots
/// from clip 3 at the 0x10 rate, and 22 / 24 send 0x7D5 to `field_20`.
void func_actor_560800_80134258(Task* task)
{
    Actor560800Work*     work;
    Actor560800AnimWork* anim;
    Actor560800AnimWork* ctx;
    Actor560800AnimWork* ctx2;
    SVECTOR              unused;
    u16                  i;
    u16                  rate;

    work = (Actor560800Work*)task->work;
    switch ((u16)work->field_38) {
        case 0:
        case 38:
            break;
        case 1:
            ctx            = (Actor560800AnimWork*)work->field_4->work;
            ctx->field_4C0 = 0;
            ctx->field_4CA = 1;
            break;
        case 22:
        case 24:
            Gp_DispatchMsg(work->field_20, 0x7D5, 2, 0);
            break;
        case 28:
            ctx2            = (Actor560800AnimWork*)work->field_4->work;
            ctx2->field_4C0 = 0;
            ctx2->field_4CA = 0;
            anim            = (Actor560800AnimWork*)work->field_4->work;
            i               = 1;
            anim->field_4B8 = 3;
            rate            = 0x10;
            anim->field_4C8 = rate;
            anim->field_4BE = 0;
            if (i < anim->field_4BA) {
                do {
                    anim->slots[i].rate = rate;
                    Gp_AnimResetSlot(&anim->anim, i, 3);
                    i++;
                } while (i < anim->field_4BA);
            }
            break;
    }
    work->field_38 = 0;
}

static inline void Actor560800_ResetAnimSlots(Actor560800AnimWork* anim, s16 clip)
{
    u16 i;
    u16 rate;

    anim->field_4B8 = clip;
    i               = 1;
    rate            = 0x10;
    anim->field_4C8 = rate;
    anim->field_4BE = 0;
    if (i < anim->field_4BA) {
        do {
            anim->slots[i].rate = rate;
            Gp_AnimResetSlot(&anim->anim, i, clip);
            i++;
        } while (i < anim->field_4BA);
    }
}

void func_actor_560800_80134384(Task* task)
{
    Actor560800Work*     work;
    Actor560800AnimWork* anim;
    u16                  i;
    u16                  rate;

    work = (Actor560800Work*)task->work;
    switch ((u16)work->field_30) {
        case 0:
        case 38:
            break;
        case 2:
            Actor560800_ResetAnimSlots((Actor560800AnimWork*)work->field_C->work, 1);
            break;
        case 4:
            Actor560800_ResetAnimSlots((Actor560800AnimWork*)work->field_C->work, 5);
            break;
        case 6:
            Actor560800_ResetAnimSlots((Actor560800AnimWork*)work->field_C->work, 0xc);
            break;
        case 8:
            Actor560800_ResetAnimSlots((Actor560800AnimWork*)work->field_C->work, 0x28);
            break;
        case 10:
            Actor560800_ResetAnimSlots((Actor560800AnimWork*)work->field_C->work, 0x17);
            break;
        case 11:
            Actor560800_ResetAnimSlots((Actor560800AnimWork*)work->field_C->work, 0x18);
            break;
        case 13:
            Actor560800_ResetAnimSlots((Actor560800AnimWork*)work->field_C->work, 0x19);
            break;
        case 15:
            Actor560800_ResetAnimSlots((Actor560800AnimWork*)work->field_C->work, 0xc);
            break;
        case 17:
            Actor560800_ResetAnimSlots((Actor560800AnimWork*)work->field_C->work, 0x29);
            break;
        case 22:
            Actor560800_ResetAnimSlots((Actor560800AnimWork*)work->field_C->work, 0x1f);
            break;
        case 23:
            Actor560800_ResetAnimSlots((Actor560800AnimWork*)work->field_C->work, 0x1d);
            break;
        case 24:
            switch ((u16)work->field_32) {
                case 0:
                    anim            = (Actor560800AnimWork*)work->field_C->work;
                    anim->field_4B8 = 0x2D;
                    i               = 1;
                    rate            = 0x10;
                    anim->field_4C8 = rate;
                    anim->field_4BE = 0;
                    if (i >= anim->field_4BA) {
                        work->field_34 = 0;
                        work->field_32++;
                        return;
                    }
                    for (;;) {
                        anim->slots[i].rate = rate;
                        Gp_AnimResetSlot(&anim->anim, i, 0x2D);
                        i++;
                        if (i < anim->field_4BA) {
                            continue;
                        }
                        work->field_34 = 0;
                        work->field_32++;
                        return;
                    }
                case 1:
                    if (++work->field_34 < 0xB5) {
                        return;
                    }
                    Actor560800_ReseedAnim(work->field_C, 0x1D, 0x10);
                    break;
                default:
                    return;
            }
            break;
        case 26:
            Actor560800_ResetAnimSlots((Actor560800AnimWork*)work->field_C->work, 0x21);
            ((Actor560800AnimWork*)work->field_C->work)->field_4C2 = 1;
            break;
        case 27:
            Actor560800_ResetAnimSlots((Actor560800AnimWork*)work->field_C->work, 0x24);
            break;
    }
    work->field_30 = 0;
}

/// Reseeds the sub-task's animation slots from clip 0x20 -- writing the slot
/// count with the 0x10 restart rate and every slot's `rate` -- then spawns
/// effect 0x6002B on the ninth per-part coordinate of the task at `field_8`
/// and posts the pad event that releases the input lock.
///
/// The rate is held in a local rather than written as two literals: both uses
/// have to reach the same register, and 0x10 is live across the loop's
/// `Gp_AnimResetSlot` call. `unused` is declared and never referenced - the
/// ROM's frame is 0x30 and the local is what reserves its 8 bytes.
void func_actor_560800_80134B14(s32 arg0)
{
    Actor560800Work*     work;
    Actor560800AnimWork* anim;
    SVECTOR              unused;
    u16                  i;
    u16                  rate;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    anim = (Actor560800AnimWork*)work->field_8->work;

    anim->field_4B8 = 0x20;
    rate            = 0x10;
    anim->field_4C8 = rate;
    anim->field_4BE = 0;
    i               = 1;
    if (i < anim->field_4BA) {
        do {
            anim->slots[i].rate = rate;
            Gp_AnimResetSlot(&anim->anim, i, 0x20);
            i++;
        } while (i < anim->field_4BA);
    }
    Gp_SpawnEff(0x6002B, &((TmdObject*)work->field_8->extra)->coords[8], 0x21, NULL);
    Pad_PostEvent(0, 1, 0xFF, 2);
}

static inline void Actor560800_BlendSlotsFirst(Task* task, u16 id, s16 rate)
{
    Actor560800AnimWork* w;
    u16                  i;
    u32                  first;
    u32                  count;

    w            = (Actor560800AnimWork*)task->work;
    w->field_4B8 = id;
    w->field_4C8 = rate;
    w->field_4BE = 0;
    SOFT_BARRIER();
    count = w->field_4BA;
    SOFT_BARRIER();
    __asm__("" : "=r"(first) : "0"((u16)1));
    if (first < count) {
        i = 1;
        do {
            func_800B4114(&w->anim, i, id, 0, 10);
            i++;
        } while (i < w->field_4BA);
    }
}

static inline void Actor560800_ResetSlots(Task* task, u16 id, u16 rate)
{
    Actor560800AnimWork* anim;
    u16                  i;

    anim            = (Actor560800AnimWork*)task->work;
    i               = 1;
    anim->field_4B8 = id;
    anim->field_4C8 = rate;
    anim->field_4BE = 0;
    if (i < anim->field_4BA) {
        do {
            anim->slots[i].rate = rate;
            Gp_AnimResetSlot(&anim->anim, i, id);
            i++;
        } while (i < anim->field_4BA);
    }
}

void func_actor_560800_80134BFC(Task* arg0)
{
    Actor560800Work*     work;
    Actor560800AnimWork* ctx;
    Actor560800AnimWork* ctx2;
    Actor560800AnimWork* ctx3;
    Actor560800AnimWork* ctx4;
    Actor560800AnimWork* ctx5;
    Actor560800AnimWork* ctx6;
    Actor560800AnimWork* ctx7;
    Actor560800AnimWork* blend;
    Actor560800AnimWork* anim;
    GsCOORDINATE2*       coord;
    u32                  first;
    u32                  count;
    u16                  step;
    u16                  i;

    work = (Actor560800Work*)arg0->work;
    switch ((u16)work->field_40) {
        case 0:
            break;
        case 1:
            ((Actor560800AnimWork*)work->field_8->work)->field_4CA = 1;
            break;
        case 2:
            ((Actor560800AnimWork*)work->field_8->work)->field_4C0 = 0x155;
            break;
        case 4:
            switch (step = work->field_42) {
                case 0:
                    ctx            = (Actor560800AnimWork*)work->field_8->work;
                    ctx->field_4C0 = 0;
                    ctx->field_4CA = 0;
                    Actor560800_ReseedAnim(work->field_8, 1, 0x10);
                    work->field_42++;
                    return;
                case 1:
                    ((Actor560800AnimWork*)work->field_8->work)->field_4CA = 1;
                    break;
                default:
                    return;
            }
            break;
        case 14:
            switch (step = work->field_42) {
                case 0:
                    Actor560800_ResetSlots(work->field_8, 0x19, 0x10);
                    work->field_42++;
                    return;
                case 1:
                    coord              = ((TmdObject*)work->field_8->extra)->coords;
                    coord->coord.t[0] -= 0x1E;
                    if (((TmdObject*)work->field_8->extra)->coords->coord.t[0] < D_actor_560800_8016F1CC[30]) {
                        Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&D_actor_560800_8016F1CC[30], 0);
                        Actor560800_BlendSlotsFirst(work->field_8, 0x1A, 0x10);
                        work->field_40 = 0;
                    }
                    ((TmdObject*)work->field_8->extra)->coords->flg = 0;
                    return;
                default:
                    return;
            }
            break;
        case 15:
            Actor560800_ResetSlots(work->field_8, 5, 0x10);
            break;
        case 18:
            Actor560800_ResetSlots(work->field_8, 0x1F, 0x10);
            break;
        case 20:
            switch (step = work->field_42) {
                case 0:
                    ((Actor560800AnimWork*)work->field_8->work)->field_4BC = 1;
                    Actor560800_PlaySeB(3);
                    Gp_DispatchMsg(work->field_0, 0x3FD, 8, 0);
                    work->field_44 = 0;
                    work->field_42++;
                    return;
                case 1:
                    if (work->field_44 < 300) {
                        work->field_44      += 5;
                        D_80073B8C[0]->t[0] -= 5;
                        return;
                    }
                    Actor560800_PlayAnimB(arg0, 2, 0xA);
                    work->field_44 = 0;
                    work->field_42++;
                    return;
                case 2:
                    if (++work->field_44 < 11) {
                        return;
                    }
                    func_actor_560800_80134B14(0);
                    work->field_44 = 0;
                    work->field_42++;
                    return;
                case 3:
                    if (++work->field_44 < 3) {
                        return;
                    }
                    Gp_SpawnEff(0x60055, &((TmdObject*)work->field_0->extra)->coords[6], 0, NULL);
                    Pad_PostEvent(0, 1, 0xFF, 2);
                    break;
                default:
                    return;
            }
            break;
        case 21:
            ((Actor560800AnimWork*)work->field_8->work)->field_4BC = 0;
            Actor560800_ResetSlots(work->field_8, 0xA, 0x10);
            break;
        case 22:
            Actor560800_ResetSlots(work->field_8, 0xB, 0x10);
            break;
        case 23:
            Gp_DispatchMsg(work->field_8, 0x7D5, 2, 0);
            Gp_DispatchMsg(work->field_10, 0x7D5, 2, 0);
            Gp_DispatchMsg(work->field_14, 0x7D5, 2, 0);
            if (work->field_18 != NULL) {
                Gp_DispatchMsg(work->field_18, 0x7D5, 2, 0);
            }
            break;
        case 24:
            Gp_DispatchMsg(work->field_8, 0x7D5, 1, 0);
            Gp_DispatchMsg(work->field_10, 0x7D5, 1, 0);
            Gp_DispatchMsg(work->field_14, 0x7D5, 1, 0);
            if (work->field_18 != NULL) {
                Gp_DispatchMsg(work->field_18, 0x7D5, 1, 0);
            }
            ctx2            = (Actor560800AnimWork*)work->field_8->work;
            ctx2->field_4C0 = 0x155;
            ctx2->field_4BC = 1;
            break;
        case 25:
            ctx3            = (Actor560800AnimWork*)work->field_8->work;
            ctx3->field_4C0 = 0;
            ctx3->field_4C4 = -0x71;
            Actor560800_ResetSlots(work->field_8, 0x1F, 0x10);
            break;
        case 26:
            ctx4            = (Actor560800AnimWork*)work->field_8->work;
            ctx4->field_4C4 = 0;
            ctx4->field_4BC = 0;
            Actor560800_ResetSlots(work->field_8, 0xD, 0x10);
            break;
        case 28:
            Actor560800_ResetSlots(work->field_8, 0x1A, 0x10);
            break;
        case 30:
            ctx5             = (Actor560800AnimWork*)work->field_8->work;
            ctx5->field_4C6 -= 0x1E;
            if (ctx5->field_4C6 >= -0x155) {
                return;
            }
            break;
        case 32:
            ((Actor560800AnimWork*)work->field_8->work)->field_4C6 = 0;
            Actor560800_ResetSlots(work->field_8, 0xF, 0x10);
            break;
        case 33:
            if ((u32)D_actor_560800_801757A4 > (u32)gDisplayState.frameCount) {
                D_actor_560800_80175798 = gDisplayState.frameCount - (D_actor_560800_801757A4 + 1);
            } else {
                D_actor_560800_80175798 = gDisplayState.frameCount - D_actor_560800_801757A4;
            }
            Actor560800_ResetSlots(work->field_8, 0x10, 0x10);
            break;
        case 35:
            switch (step = work->field_42) {
                case 0:
                    Actor560800_ReseedAnim(work->field_8, 7, 0x10);
                    work->field_44 = 0;
                    work->field_42++;
                    return;
                case 1:
                    if (++work->field_44 < 0x5B) {
                        return;
                    }
                    Actor560800_BlendSlotsFirst(work->field_8, 0x14, 0x10);
                    break;
                default:
                    return;
            }
            break;
        case 36:
            switch (step = work->field_42) {
                case 0:
                    Actor560800_ResetSlots(work->field_8, 0xE, 0x10);
                    work->field_44 = 0;
                    work->field_42++;
                    return;
                case 1:
                    if (++work->field_44 < 0x1F) {
                        return;
                    }
                    Pad_PostEvent(0, 1, 0xFF, 2);
                    Gp_SpawnEff(0x6002B, &((TmdObject*)work->field_8->extra)->coords[8], 0x21, NULL);
                    blend            = (Actor560800AnimWork*)work->field_C->work;
                    blend->field_4B8 = 0x20;
                    blend->field_4C8 = 8;
                    blend->field_4BE = 0;
                    SOFT_BARRIER();
                    count = blend->field_4BA;
                    SOFT_BARRIER();
                    __asm__("" : "=r"(first) : "0"((u16)1));
                    if (first < count) {
                        i = 1;
                        do {
                            func_800B4114(&blend->anim, i, 0x20, 0, 5);
                            i++;
                        } while (i < blend->field_4BA);
                    }
                    work->field_44 = 0;
                    work->field_42++;
                    return;
                case 2:
                    if (++work->field_44 < 3) {
                        return;
                    }
                    Gp_SpawnEff(0x60055, &((TmdObject*)work->field_C->extra)->coords[4], 0, NULL);
                    break;
                default:
                    return;
            }
            break;
        case 37:
            ((Actor560800AnimWork*)work->field_8->work)->field_4CA = 0;
            ctx6                                                   = (Actor560800AnimWork*)work->field_8->work;
            ctx6->field_4C0                                       -= 0x3C;
            if (ctx6->field_4C0 >= -0x200) {
                return;
            }
            break;
        case 38:
            ctx7 = (Actor560800AnimWork*)work->field_8->work;
            switch (step = work->field_42) {
                case 0:
                    anim = (Actor560800AnimWork*)work->field_8->work;
                    SOFT_TOUCH_REG(anim);
                    anim->field_4B8 = 3;
                    anim->field_4C8 = 0x10;
                    anim->field_4BE = 0;
                    SOFT_BARRIER();
                    for (i = 1; i < anim->field_4BA; i++) {
                        func_800B4114(&anim->anim, i, 3, 0, 10);
                    }
                    work->field_42++;
                    return;
                case 1:
                    ctx7->field_4C0 += 0x3C;
                    if (ctx7->field_4C0 < 0) {
                        return;
                    }
                    ctx7->field_4C0 = 0;
                    break;
                default:
                    return;
            }
            break;
        case 39:
            Actor560800_ResetSlots(work->field_8, 0x22, 8);
            break;
    }
    work->field_40 = 0;
}

void func_actor_560800_80135AEC(s32 arg0)
{
    if (arg0 == 1) {
        if ((u32)D_actor_560800_8017579C > (u32)gDisplayState.frameCount) {
            D_actor_560800_80175790 = gDisplayState.frameCount - (D_actor_560800_8017579C + 1);
        } else {
            D_actor_560800_80175790 = gDisplayState.frameCount - D_actor_560800_8017579C;
        }
    } else if (arg0 == 2) {
        if ((u32)D_actor_560800_801757A0 > (u32)gDisplayState.frameCount) {
            D_actor_560800_80175794 = gDisplayState.frameCount - (D_actor_560800_801757A0 + 1);
        } else {
            D_actor_560800_80175794 = gDisplayState.frameCount - D_actor_560800_801757A0;
        }
    } else if (arg0 == 3) {
        if ((u32)D_actor_560800_801757A4 > (u32)gDisplayState.frameCount) {
            D_actor_560800_80175798 = gDisplayState.frameCount - (D_actor_560800_801757A4 + 1);
        } else {
            D_actor_560800_80175798 = gDisplayState.frameCount - D_actor_560800_801757A4;
        }
    }
    CdCmd_CancelReplaceAndActivate();
    Gp_RestoreStreamRng();
}

void func_actor_560800_80135BD8(Task* arg0)
{
    Actor560800Work* work;
    Task*            sub5;
    Task*            sub6;
    SVECTOR          vec;

    work       = (Actor560800Work*)Mem_Malloc(0x68, 0);
    arg0->work = (TaskIdMap*)work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    Mem_Set(work, 0, 0x68);
    work->field_0           = gameGetPtrSlot(3);
    D_actor_560800_8017578C = arg0;
    work->field_4           = Task_SpawnFromTable(&D_actor_560800_801718F0, 4, 0, 0);
    sub5                    = Task_SpawnFromTable(&D_actor_560800_801718F0, 5, 0, 0);
    work->field_8           = sub5;
    work->field_10          = Task_SpawnFromTable(&D_actor_560800_801718F0, 7, 1, (s32)sub5);
    work->field_14          = Task_SpawnFromTable(&D_actor_560800_801718F0, 8, 0, (s32)work->field_8);
    work->field_18          = Task_SpawnFromTable(&D_actor_560800_801718F0, 9, 2, (s32)work->field_8);
    sub6                    = Task_SpawnFromTable(&D_actor_560800_801718F0, 6, 0, 0);
    work->field_C           = sub6;
    work->field_1C          = Task_SpawnFromTable(&D_actor_560800_801718F0, 0xA, 3, (s32)sub6);
    work->field_20          = Task_SpawnFromTable(&D_actor_560800_8017575C, 0, 0, (s32)arg0);
    work->field_24          = Task_SpawnFromTable(&D_actor_560800_8017575C, 2, 0, (s32)arg0);
    vec.vx                  = 0x5A0;
    vec.vy                  = 0x5A0;
    vec.vz                  = 0x5A0;
    Gp_SetOverrideVec(&vec);
}

void func_actor_560800_80135D54(Task* arg0)
{
    s32              msg[5];
    Actor560800Work* work;
    s32              val;

    switch (arg0->state) {
        case 0:
            if (D_80114C12 == 1 || D_80071075 != 0) {
                return;
            }
            func_actor_560800_80135BD8(arg0);
            Gp_CapFile = 0;
            Gp_LoadCapFile(0);
            func_800E6D4C(0x180, 0);
            arg0->state++;
        case 1:
            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
            func_800E8634((s32)&D_actor_560800_8016F5E0, 1, (s32)&D_actor_560800_80171800);
            arg0->state++;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                Gp_LcgState = D_actor_560800_801757A8;
                Gp_PulseState1C();
                val    = D_80073BA9;
                msg[0] = (D_8007218A == 1) ? val + 1 : val + 0x22;
                msg[1] = 1;
                msg[2] = 0;
                msg[3] = 0;
                msg[4] = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)msg, 0);
                Task_RequestKill(arg0, 0);
                return;
            }
            break;
    }
    func_actor_560800_80133970(arg0);
    func_actor_560800_80134258(arg0);
    func_actor_560800_80134384(arg0);
    func_actor_560800_80134BFC(arg0);
    work = (Actor560800Work*)arg0->work;
    switch ((u16)work->field_58) {
        case 0:
            break;
        case 1:
            if (work->field_4 != NULL) {
                taskKill(work->field_4);
            }
            Display_SpawnWithOt(&D_actor_560800_801718F0, 0xC, 0, 0);
            break;
    }
    work->field_58 = 0;
}

void func_actor_560800_80135F50(Task* arg0)
{
    Display_SpawnWithOt(&D_actor_560800_8016EA28, 1, arg0->spawnArg1, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

void func_actor_560800_80135FA0(Task* arg0)
{
    OverlayFadeWork* work;
    OverlayFadeWork* alloc;

    work = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work    = alloc;
            work->b = 0;
            work->g = 0;
            work->r = 0;
            Task_Reparent(D_actor_560800_8017578C, arg0);
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r += (u16)arg0->spawnArg1;
            work->g += (u16)arg0->spawnArg1;
            work->b += (u16)arg0->spawnArg1;
            if (work->r >= 0x100) {
                SetDispMask(0);
                taskKill(arg0);
            }
            break;
    }
}

void func_actor_560800_80136094(Task* arg0)
{
    OverlayFadeWork* work;
    OverlayFadeWork* alloc;

    work = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work    = alloc;
            work->b = 0xFF;
            work->g = 0xFF;
            work->r = 0xFF;
            Task_Reparent(D_actor_560800_8017578C, arg0);
            goto state_inc;
        case 6:
            SetDispMask(1);
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        state_inc:
            arg0->state += 1;
            /* fallthrough */
        case 7:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r -= (u16)arg0->spawnArg1;
            work->g -= (u16)arg0->spawnArg1;
            work->b -= (u16)arg0->spawnArg1;
            if (work->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

void func_actor_560800_801361A0(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    switch (arg2) {
        case 0:
            break;
        case 1:
            extra->flags = extra->flags & 0xFF7B;
            return;
        case 2:
            extra->flags = extra->flags | 0x84;
            return;
    }
}

/// Message handler that places the task's model from `placement`: the
/// position becomes the model coordinate's translation, the Y, X and Z
/// rotations are applied in that order, and the coordinate is marked for
/// recalculation.
void func_actor_560800_801361F4(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}

/// Spawns entry 2 of the actor's task descriptor table with `arg0` as its
/// spawn argument. Script tables in the actor's data call it.
void func_actor_560800_80136280(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_560800_801718F0, 2, arg0, 0);
}

void func_actor_560800_801362B0(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_560800_801718F0, 3, arg0, 0);
}

void func_actor_560800_801362E0(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;
    GpCmdArg         msg;

    msg.command = arg0;
    Gp_DispatchMsg(work->field_20, 0x7DB, (s32)&msg, 0);
}

void func_actor_560800_8013631C(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;
    GpCmdArg         msg;

    msg.command = arg0;
    Gp_DispatchMsg(work->field_24, 0x7DB, (s32)&msg, 0);
}

void func_actor_560800_80136358(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;

    work->field_28 = arg0;
    work->field_2A = 0;
}

/// Latches the animation id in the 0x60 slot and plays that animation on the
/// task at `field_0`: message 0x3F4 with `field_8` 1, `field_C` 0xA and
/// `field_10` 1. The zero-extended id goes into the message while the store
/// keeps the raw halfword argument, so the two uses do not share a register.
void func_actor_560800_80136378(s16 arg0)
{
    Actor560800Work* work;
    GpAnimArg        msg;
    u16              anim;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    if (work->field_0 != NULL) {
        anim              = arg0;
        msg.animBlock.ptr = D_actor_560800_8016EA40;
        work->field_60    = arg0;
        msg.field_4       = anim;
        msg.field_8       = 1;
        msg.field_C       = 0xA;
        msg.field_10      = 1;
        Gp_DispatchMsg(work->field_0, 0x3F4, (s32)&msg, 0);
        work->field_62 = 0;
    }
}

/// The same animation reseed as `func_actor_560800_801364A0`, reached through
/// `field_4` instead of `field_C`: the id goes to `field_4B8` with 0x10 as the
/// restart rate in `field_4C8`, `field_4BE` is cleared, and slots 1..`field_4BA`
/// are blended through `func_800B4114`.
void func_actor_560800_801363F8(u16 arg0)
{
    Actor560800Work*     work;
    Actor560800AnimWork* anim;
    u16                  i;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    anim = (Actor560800AnimWork*)work->field_4->work;

    anim->field_4B8 = arg0;
    anim->field_4C8 = 0x10;
    anim->field_4BE = 0;
    SOFT_BARRIER();
    i = 1;
    if (i < anim->field_4BA) {
        do {
            func_800B4114(&anim->anim, i, arg0, 0, 10);
            i++;
        } while (i < anim->field_4BA);
    }
}

/// Reseeds the animation slots of the sub-task at `field_C` from `arg0`: the
/// id goes to `field_4B8` with 0x10 as the restart rate in `field_4C8`,
/// `field_4BE` is cleared, and slots 1..`field_4BA` are blended through
/// `func_800B4114`. `func_actor_560800_801363F8` is the same body reached
/// through `field_4`.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one: without it sched1
/// hoists `i = 1` above the three slot stores, so the count test runs before
/// the assignment and the guard's `beqz` delay slot gets the `field_4BE` store
/// instead of the loop's `i = 1`.
void func_actor_560800_801364A0(u16 arg0)
{
    Actor560800Work*     work;
    Actor560800AnimWork* anim;
    u16                  i;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    anim = (Actor560800AnimWork*)work->field_C->work;

    anim->field_4B8 = arg0;
    anim->field_4C8 = 0x10;
    anim->field_4BE = 0;
    SOFT_BARRIER();
    i = 1;
    if (i < anim->field_4BA) {
        do {
            func_800B4114(&anim->anim, i, arg0, 0, 10);
            i++;
        } while (i < anim->field_4BA);
    }
}

/// Copies a 64x256 strip of VRAM to (0x280, 0x100), then re-loads the chunk at
/// `D_8006C454` with `D5B498_8006C234` set to 5 for the duration (that byte is
/// the image mode `Fs_LoadImageChunk` reads for chunks whose second halfword is
/// in 0xF5..0xFF), restoring it to 0 afterwards.
void func_actor_560800_80136548(void)
{
    RECT rect;

    rect.x = 0x3C0;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x100;
    MoveImage(&rect, 0x280, 0x100);
    D5B498_8006C234 = 5;
    Fs_LoadImageChunk(D_8006C454, 1);
    D5B498_8006C234 = 0;
}

void func_actor_560800_801365B0(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;

    work->field_40 = arg0;
    work->field_42 = 0;
}

void func_actor_560800_801365D0(u16 arg0)
{
    Actor560800Work*     work;
    Actor560800AnimWork* anim;
    u16                  i;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    anim = (Actor560800AnimWork*)work->field_8->work;

    anim->field_4B8 = arg0;
    anim->field_4C8 = 0x10;
    anim->field_4BE = 0;
    SOFT_BARRIER();
    i = 1;
    if (i < anim->field_4BA) {
        do {
            func_800B4114(&anim->anim, i, arg0, 0, 10);
            i++;
        } while (i < anim->field_4BA);
    }
}

void func_actor_560800_80136678(s32 arg0)
{
    SndEvt_EnqueueType6(D_actor_560800_8016F57C[arg0], 0, 0);
}

/// Task state handler for the second spawn mode: states 1 and 2 — and state 0,
/// which first parks `D_8007106B` at 2 — only step the state, and state 3 runs
/// the hand-off. That hand-off copies a 64x256 VRAM strip from (0x380, 0) to
/// (0x200, 0x100), the same shape `func_actor_560800_80136548` uses for the
/// other strip, then re-loads the chunk at `D_8006C45C` with
/// `D5B498_8006C234` at 8 for the duration, kills this task, resets the
/// display heap and spawns `D_actor_560800_801718F0` index 0xB into the work
/// block's `field_4`. Like `func_actor_310100_801620FC`, state 3 hands the
/// finished work over rather than leaving the task alive.
void func_actor_560800_801366B0(Task* arg0)
{
    RECT             rect;
    Actor560800Work* work;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    switch (arg0->state) {
        case 0:
            D_8007106B = 2;
            /* fallthrough */
        case 1:
        case 2:
            arg0->state++;
            return;
        case 3:
            rect.x = 0x380;
            rect.y = 0;
            rect.w = 0x40;
            rect.h = 0x100;
            MoveImage(&rect, 0x200, 0x100);
            D5B498_8006C234 = 8;
            Fs_LoadImageChunk(D_8006C45C, 1);
            D5B498_8006C234 = 0;
            taskKill(arg0);
            Display_ResetHeapWrapper();
            work->field_4 = Task_SpawnOnDefaultList(&D_actor_560800_801718F0, 0xB, 1, (s32)D_actor_560800_8017578C);
            break;
    }
}

void func_actor_560800_801367C0(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;

    work->field_58 = arg0;
    work->field_5A = 0;
}

void func_actor_560800_801367E0(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;

    work->field_28 = arg0;
    work->field_2A = 0;
    work->field_38 = arg0;
    work->field_3A = 0;
    work->field_30 = arg0;
    work->field_32 = 0;
    work->field_40 = arg0;
    work->field_42 = 0;
}

void func_actor_560800_80136818(void)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;
    PlayerStatus*    cfg  = &Player_Status;
    s16              hp;

    Gp_KillPlayerEffs();

    if (cfg->hp < 0x33) {
        hp = 1;
    } else {
        hp = (u16)cfg->hp - 0x32;
    }
    do {
        cfg->hp        = hp;
        work->field_64 = 1;
    } while (0);
}

/// Clears the four s16 pairs at 0x28/0x30/0x38/0x40, and the first time it runs
/// (0x64 still zero) kills the player effects and drops the current HP by 50,
/// then latches 0x64. Ends by pulsing gameplay state 0x1C, cancelling the
/// pending CD command and blanking the display.
void func_actor_560800_80136878(void)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;
    s16              hp;

    work->field_28 = 0;
    work->field_40 = 0;
    work->field_30 = 0;
    work->field_38 = 0;
    if ((u16)work->field_64 == 0) {
        PlayerStatus*    cfg   = &Player_Status;
        Actor560800Work* work2 = (Actor560800Work*)D_actor_560800_8017578C->work;

        Gp_KillPlayerEffs();
        if (cfg->hp < 0x33) {
            hp = 1;
        } else {
            hp = (u16)cfg->hp - 0x32;
        }
        do {
            cfg->hp         = hp;
            work2->field_64 = 1;
        } while (0);
    }
    Gp_PulseState1C();
    CdCmd_CancelReplaceAndActivate();
    SetDispMask(0);
}

/// Script callback in the actor's data tables: queues the replacement of
/// overlay 0x82.
void func_actor_560800_80136910(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

void func_actor_560800_80136930(s32 arg0)
{
    if (arg0 == 1) {
        D_actor_560800_8017579C = gDisplayState.frameCount;
    } else if (arg0 == 2) {
        D_actor_560800_801757A0 = gDisplayState.frameCount;
    } else if (arg0 == 3) {
        D_actor_560800_801757A4 = gDisplayState.frameCount;
    }
    CdCmd_EnqueueOverlay81();
}

void func_actor_560800_801369A0(void)
{
    Display_SpawnWithOt(&D_actor_560800_801718F0, 0xD, 0, 0);
    D_8007106B = 2;
    Gp_SpawnViewTasks();
}

void func_actor_560800_801369E0(Task* arg0)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        taskKill(arg0);
        Display_ResetHeapWrapper();
    }
}

void func_actor_560800_80136A20(void)
{
    Gp_CapFile = 0;
    Gp_LoadCapFile(1);
    func_800E6D4C(0x180, 0);
}

void func_actor_560800_80136A54(void)
{
    Gp_CapFile = 0;
    Gp_LoadCapFile(2);
    func_800E6D4C(0x180, 0);
}

/// Task handler in the actor's task descriptor table that only kills the task.
void func_actor_560800_80136A88(Task* task)
{
    taskKill(task);
}

/// Swings the model's chain of parts toward the named task's work block: while
/// the pieces are walked 1..5 on the scratchpad stack, each part's X rotation
/// steps by `speed` toward the heading of that block's `world` translation seen
/// from the part's joint, and the joint positions accumulate through the GTE.
/// Part 0's X rotation oscillates on a `D_actor_560800_801752E8` phase. The
/// closing switch drives `field_27E`: close enough in Y/Z starts the dip in
/// `field_24E`, which then returns to zero.
void func_actor_560800_80136AA8(Task* arg0)
{
    Actor560800ChainScratch* top;
    Actor560800ModelWork*    work;
    Actor560800ChainScratch* s;
    Actor560800PartsWork*    target;
    s16                      i;
    s16                      speed;
    s32                      a;

    top  = *(Actor560800ChainScratch**)0x1F8003FC;
    work = (Actor560800ModelWork*)arg0->work;
    s = *(Actor560800ChainScratch**)0x1F8003FC = top - 1;
    target                                     = (Actor560800PartsWork*)work->field_26C->work;
    Mem_Set(s, 0, sizeof(Actor560800ChainScratch));
    Mem_CopyUnaligned(work->rot, s->rot, sizeof(s->rot));
    if (work->field_280 & 1) {
        speed = 2;
        switch (((D_actor_560800_801752E8 + work->field_270) * 2) & 0x300) {
            case 0x0:
            case 0x300:
                s->rot[0].vx += 4;
                s->rot[0].vx %= 0x1000;
                break;
            case 0x100:
            case 0x200:
                s->rot[0].vx -= 4;
                if (s->rot[0].vx < 0) {
                    s->rot[0].vx += 0x1000;
                }
                break;
        }
    } else {
        speed = 1;
        switch (((D_actor_560800_801752E8 + work->field_270) * 2) & 0x700) {
            case 0x0:
            case 0x100:
            case 0x600:
            case 0x700:
                s->rot[0].vx += 2;
                s->rot[0].vx %= 0x1000;
                break;
            case 0x200:
            case 0x300:
            case 0x400:
            case 0x500:
                s->rot[0].vx -= 2;
                if (s->rot[0].vx < 0) {
                    s->rot[0].vx += 0x1000;
                }
                break;
        }
    }
    s->pos.vx = ((TmdObject*)arg0->extra)->coords->sub->coord.t[0] + ((TmdObject*)arg0->extra)->coords->coord.t[0];
    s->pos.vy = ((TmdObject*)arg0->extra)->coords->sub->coord.t[1] + ((TmdObject*)arg0->extra)->coords->coord.t[1];
    s->pos.vz = ((TmdObject*)arg0->extra)->coords->sub->coord.t[2] + ((TmdObject*)arg0->extra)->coords->coord.t[2];
    s->ang.vx = s->rot[0].vx;
    s->ang.vy = s->rot[0].vy;
    s->ang.vz = s->rot[0].vz;
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, s->rot[0].vy, 1);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords->coord, s->rot[0].vx + 0x400, 0);
    Gfx_RotMatrixZ(&((TmdObject*)arg0->extra)->coords->coord, s->rot[0].vz, 0);
    s->link  = ((TmdObject*)arg0->extra)->coords->coord;
    s->chain = s->link;
    gte_SetRotMatrix(&s->chain);
    for (i = 1; i < 6; i++) {
        gte_ldclmv(&((TmdObject*)arg0->extra)->coords[i].coord);
        gte_rtir();
        gte_stclmv(&s->link);
        gte_ldclmv((char*)&((TmdObject*)arg0->extra)->coords[i].coord + 2);
        gte_rtir();
        gte_stclmv((char*)&s->link + 2);
        gte_ldclmv((char*)&((TmdObject*)arg0->extra)->coords[i].coord + 4);
        gte_rtir();
        gte_stclmv((char*)&s->link + 4);
        s->joint.vx = ((TmdObject*)arg0->extra)->coords[i + 1].coord.t[0];
        s->joint.vy = ((TmdObject*)arg0->extra)->coords[i + 1].coord.t[1];
        s->joint.vz = ((TmdObject*)arg0->extra)->coords[i + 1].coord.t[2];
        gte_SetRotMatrix(&s->link);
        gte_ldv0(&s->joint);
        gte_rtv0();
        gte_stsv(&s->joint);
        s->joint.vx += s->pos.vx;
        s->joint.vy += s->pos.vy;
        s->joint.vz += s->pos.vz;
        s->aim.vx    = (s->ang.vx + s->rot[i].vx) % 0x1000;
        s->aim.vy    = (s->ang.vy + s->rot[i].vy) % 0x1000;
        s->aim.vz    = (s->ang.vz + s->rot[i].vz) % 0x1000;
        if (s->rot[0].vy == 0) {
            a = ratan2(s->joint.vy - target->world.t[1], target->world.t[2] - s->joint.vz) % 0x1000;
            if (a < 0) {
                a += 0x1000;
            }
            s->aim.vx -= a;
            if (s->aim.vx < 0) {
                s->aim.vx += 0x1000;
            }
            if (s->aim.vx < 0x800) {
                s->rot[i].vx += speed;
                s->rot[i].vx %= 0x1000;
            } else {
                s->rot[i].vx -= speed;
                if (s->rot[i].vx < 0) {
                    s->rot[i].vx += 0x1000;
                }
            }
        } else {
            a = ratan2(target->world.t[1] - s->joint.vy, target->world.t[2] - s->joint.vz) % 0x1000;
            if (a < 0) {
                a += 0x1000;
            }
            s->aim.vx -= a;
            if (s->aim.vx < 0) {
                s->aim.vx += 0x1000;
            }
            if (s->aim.vx > 0x800) {
                s->rot[i].vx += speed;
                s->rot[i].vx %= 0x1000;
            } else {
                s->rot[i].vx -= speed;
                if (s->rot[i].vx < 0) {
                    s->rot[i].vx += 0x1000;
                }
            }
        }
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords[i].coord, s->rot[i].vy, 1);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[i].coord, s->rot[i].vx, 0);
        Gfx_RotMatrixZ(&((TmdObject*)arg0->extra)->coords[i].coord, s->rot[i].vz, 0);
        gte_SetRotMatrix(&s->chain);
        gte_ldclmv(&((TmdObject*)arg0->extra)->coords[i].coord);
        gte_rtir();
        gte_stclmv(&s->chain);
        gte_ldclmv((char*)&((TmdObject*)arg0->extra)->coords[i].coord + 2);
        gte_rtir();
        gte_stclmv((char*)&s->chain + 2);
        gte_ldclmv((char*)&((TmdObject*)arg0->extra)->coords[i].coord + 4);
        gte_rtir();
        gte_stclmv((char*)&s->chain + 4);
        s->joint.vx = ((TmdObject*)arg0->extra)->coords[i + 1].coord.t[0];
        s->joint.vy = ((TmdObject*)arg0->extra)->coords[i + 1].coord.t[1];
        s->joint.vz = ((TmdObject*)arg0->extra)->coords[i + 1].coord.t[2];
        gte_SetRotMatrix(&s->chain);
        gte_ldv0(&s->joint);
        gte_rtv0();
        gte_stsv(&s->joint);
        s->ang.vx += s->rot[i].vx;
        s->ang.vx %= 0x1000;
        s->ang.vy += s->rot[i].vy;
        s->ang.vy %= 0x1000;
        s->ang.vz += s->rot[i].vz;
        s->ang.vz %= 0x1000;
        s->pos.vx += s->joint.vx;
        s->pos.vy += s->joint.vy;
        s->pos.vz += s->joint.vz;
    }
    Mem_CopyUnaligned(s->rot, work->rot, sizeof(s->rot));
    switch (work->field_27E) {
        case 0:
            if (abs(s->pos.vy - target->world.t[1]) < 300) {
                if (abs(s->pos.vz - target->world.t[2]) < 200) {
                    work->field_27E = 1;
                }
            }
            break;
        case 1:
            work->field_24E -= 20;
            if (work->field_24E < -100) {
                work->field_27E = 2;
            }
            break;
        case 2:
            work->field_24E += 2;
            if (work->field_24E > 0) {
                work->field_24E = 0;
                work->field_27E = 0;
            }
            break;
    }
    *(Actor560800ChainScratch**)0x1F8003FC += 1;
}

/// Sets up the animated model part the spawn argument names: allocates its
/// `Actor560800ModelWork`, hangs it off `Task::work`, points the object's light
/// and colour matrices into it, makes the named task this one's parent and hands
/// the part to `func_800B3F84` with the overlay's animation bank. The three
/// `Gp_LcgState` draws taken along the way seed the handlers' random headings,
/// and the slot count comes from the spawner's `spawnArg1`.
void func_actor_560800_801376E0(Task* arg0)
{
    Actor560800ModelWork* mem;
    Actor560800ModelWork* work;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    Task*                 child;

    obj        = (TmdObject*)arg0->extra;
    coord      = obj->coords;
    mem        = (Actor560800ModelWork*)Mem_Malloc(0x28C, 0);
    arg0->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        taskKill(arg0);
        return;
    }
    Mem_Set(mem, 0, 0x28C);
    work            = (Actor560800ModelWork*)arg0->work;
    child           = (Task*)arg0->spawnArg2;
    work->field_26C = child;
    coord->sub      = ((TmdObject*)child->extra)->coords;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    Task_Reparent(work->field_26C, arg0);
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_270 = Gp_LcgState >> 16;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_274 = Gp_LcgState >> 16;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_278 = (Gp_LcgState >> 16) & 0x3FF;
    func_800B3F84(&work->anim, D_actor_560800_801752F0, obj, work->poseBuf,
                  work->slots);
    work->field_280 = arg0->spawnArg1;
}

/// Per-frame handler of the animated model part `func_actor_560800_801376E0`
/// sets up. State 1 hides the part (`TmdObject::flags` bit 0x80) for the
/// part ids the current view excludes and otherwise runs
/// `func_actor_560800_80136AA8`; state 2 resets all seven animation slots to
/// `field_280`, state 3 ticks them, state 4 copies the coordinates of a part
/// spawned from `D_actor_560800_8017575C` and state 5 kills the task a frame
/// later. Every frame that survives rebuilds the root translation and, while
/// visible, drives `func_8017F450` and the periodic `Gp_SpawnEff`.
void func_actor_560800_80137820(Task* arg0)
{
    Actor560800ModelWork* work;
    TmdObject*            extra;
    GsCOORDINATE2*        coord;
    Actor560800ModelWork* anim;
    Task*                 child;
    s32                   i;
    u32                   tick;
    TmdObject*            obj;
    u32                   state;
    u16                   id;
    SVECTOR               unused; // never touched; only reserves the frame slot

    extra = (TmdObject*)arg0->extra;
    state = arg0->state;
    work  = (Actor560800ModelWork*)arg0->work;
    coord = extra->coords;
    obj   = extra;
    switch (state) {
        case 0:
            func_actor_560800_801376E0(arg0);
            arg0->state++;
            return;
        case 1:
            if (Gp_FindViewIndex(gGameSession->at4.loc.view) == 0x16) {
                switch (work->field_280) {
                    case 1:
                    case 3:
                        obj->flags |= 0x80;
                        return;
                    case 4 ... 0x7FFF:
                        break;
                    default:
                        func_actor_560800_80136AA8(arg0);
                        goto done;
                }
            }
            if (work->field_280 < 8) {
                if (work->field_280 >= 5) {
                    obj->flags |= 0x80;
                    return;
                }
            }
            func_actor_560800_80136AA8(arg0);
            break;
        case 2:
            if (work->field_280 < 4) {
                if (work->field_280 >= 2) {
                    obj->flags |= 0x80;
                    return;
                }
            }
            i    = 1;
            id   = work->field_280;
            anim = (Actor560800ModelWork*)arg0->work;
            do {
                anim->slots[i & 0xFFFF].rate = 0x10;
                Gp_AnimResetSlot(&anim->anim, i & 0xFFFF, id);
                i++;
            } while ((u32)(i & 0xFFFF) < 7U);
            arg0->state++;
            break;
        case 3:
            anim = (Actor560800ModelWork*)arg0->work;
            i    = 1;
            do {
                Gp_AnimTickIndex(&anim->anim, i & 0xFFFF);
                i++;
            } while ((u32)(i & 0xFFFF) < 7U);
            for (i = 1; (u32)(i & 0xFFFF) < 7U; i++) {
                if (!(anim->slots[i & 0xFFFF].flags & 0x100)) {
                    break;
                }
            }
            break;
        case 4:
            child = Task_SpawnFromTable(&D_actor_560800_8017575C, 3,
                                        ((TmdObject*)D_actor_560800_801757AC->extra)->coords->coord.t[1],
                                        (s32)arg0->spawnArg2);
            if (child == NULL) {
                arg0->state = 1;
                return;
            }
            i = 0;
            do {
                Mem_CopyUnaligned(&((TmdObject*)arg0->extra)->coords[i & 0xFFFF].coord,
                                  &((TmdObject*)child->extra)->coords[i & 0xFFFF].coord, 0x20);
                i++;
            } while ((u32)(i & 0xFFFF) < 7U);
            arg0->killCountdown = 0;
            arg0->state++;
            break;
        case 5:
            if (++arg0->killCountdown >= 2) {
                taskKill(arg0);
                return;
            }
            break;
    }
done:
    coord->coord.t[0] = work->field_254 + work->field_24C;
    coord->coord.t[1] = (s16)work->field_256 + work->field_24E;
    coord->coord.t[2] = work->field_258 + work->field_250;
    coord->flg        = 0;
    if (!(obj->flags & 0x80)) {
        func_8017F450(&((TmdObject*)arg0->extra)->coords[6], work->field_280, 0x100, 0x3C36);
        if (Gp_FindViewIndex(gGameSession->at4.loc.view) != 0x16) {
            tick = D_actor_560800_801752E8 + 1;
            if (!(tick & 0x7F) && ((tick >> 7) & 7) == work->field_280) {
                Gp_SpawnEff(0x601C6, &((TmdObject*)arg0->extra)->coords[2], 0x800, NULL);
            }
        }
    }
}

/// Per-frame handler of the model part. State 0 runs the spawner, parents the
/// root coordinate to `D_actor_560800_801757AC`'s and records its height; state
/// 1 swings parts 3-5 on X/Z by 20 between +-0x154, rebuilds their rotation
/// matrices and sinks the root, killing the task once its height passes 10000.
void func_actor_560800_80137BEC(Task* task)
{
    Actor560800ModelWork* work;
    GsCOORDINATE2*        coord;
    TmdObject*            extra;
    VECTOR                vec;
    s32                   i;
    s32                   j;
    u16                   t286;
    u16                   t288;

    work  = (Actor560800ModelWork*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    switch (task->state) {
        case 0:
            func_actor_560800_801376E0(task);
            coord->sub                       = ((TmdObject*)D_actor_560800_801757AC->extra)->coords;
            ((TmdObject*)task->extra)->flags = 0;
            work                             = (Actor560800ModelWork*)task->work;
            i                                = 1;
            do {
                work->swingDir[i & 0xFFFF] = 0;
                i                         += 1;
            } while ((u32)(i & 0xFFFF) < 6U);
            work->field_28A = coord->coord.t[1];
            coord->flg      = 0;
            task->state++;
            break;
        case 1:
            i = 3;
            do {
                if (work->swingDir[i & 0xFFFF] & 1) {
                    work->swing[i & 0xFFFF].vx += 20;
                    if (work->swing[i & 0xFFFF].vx >= 0x155) {
                        work->swingDir[i & 0xFFFF] |= 1;
                    }
                } else {
                    work->swing[i & 0xFFFF].vx -= 20;
                    if (work->swing[i & 0xFFFF].vx < -0x154) {
                        work->swingDir[i & 0xFFFF] &= 0xFFFE;
                    }
                }
                if (work->swingDir[i & 0xFFFF] & 2) {
                    work->swing[i & 0xFFFF].vz += 20;
                    if (work->swing[i & 0xFFFF].vz >= 0x155) {
                        work->swingDir[i & 0xFFFF] |= 2;
                    }
                } else {
                    work->swing[i & 0xFFFF].vz -= 20;
                    if (work->swing[i & 0xFFFF].vz < -0x154) {
                        work->swingDir[i & 0xFFFF] &= 0xFFFD;
                    }
                }
                j = i & 0xFFFF;
                Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords[j].coord, work->rot[j].vy, 1);
                Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[j].coord,
                               work->rot[j].vx + work->swing[j].vx, 0);
                Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords[j].coord,
                               work->rot[j].vz + work->swing[j].vz, 0);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 6U);
            t286              = work->field_286 + 4;
            t288              = work->field_288 + t286;
            work->field_288   = t288;
            work->field_286   = t286;
            coord->coord.t[1] = work->field_28A + task->spawnArg1 -
                                ((TmdObject*)D_actor_560800_801757AC->extra)->coords->coord.t[1] +
                                (s16)t288;
            coord->flg = 0;
            if (coord->coord.t[1] > 10000) {
                taskKill(task);
                return;
            }
            break;
    }
    extra  = (TmdObject*)task->extra;
    vec.vx = extra->coords->workm.t[0];
    vec.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    vec.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    func_800D7A9C(extra, &vec, 0, 3);
}

/// Message handler of the parts task that loads a pose table into all eight
/// parts, chosen by `Actor560800PartsWork::field_46`: 0-2 place each part at
/// its table position and, while `field_48` is set, rebuild its rotation from
/// the table with the child joints reset; 3 resets each part's matrix, hangs
/// it off `gGfxViewCoord` and offsets it from the message position; 4 kills
/// parts 4-7, reparents the rest to `D_actor_560800_801757AC`'s model and
/// raises the `D_actor_560800_801752E8` / `801752EC` flags.
void func_actor_560800_80137F58(Task* task, s32 msgId, VECTOR* msg)
{
    Actor560800PartsWork* work;
    u16                   flag;
    Actor560800PartPose*  pose;
    Actor560800ModelWork* part;
    GsCOORDINATE2*        coord;
    ActorMat*             mat;
    s32                   i;
    s32                   j;

    work = (Actor560800PartsWork*)task->work;
    flag = 0;
    switch (work->field_46) {
        case 0:
            pose = D_actor_560800_80175314;
            break;
        case 1:
            pose = D_actor_560800_801753D4;
            break;
        case 2:
            pose = D_actor_560800_80175494;
            flag = 1;
            break;
        case 3:
            pose = D_actor_560800_80175554;
            i    = 0;
            do {
                if (work->parts[i & 0xFFFF] != NULL) {
                    coord              = ((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords;
                    mat                = (ActorMat*)&coord->coord;
                    mat->ident.m00_m01 = 0x1000;
                    mat->ident.m02_m10 = 0;
                    mat->ident.m11_m12 = 0x1000;
                    mat->ident.m20_m21 = 0;
                    mat->ident.m22     = 0x1000;
                    coord->sub         = &gGfxViewCoord;
                    part               = (Actor560800ModelWork*)work->parts[i & 0xFFFF]->work;
                    part->field_254    = msg->vx + pose->pos.vx;
                    part->field_256    = msg->vy + pose->pos.vy;
                    part->field_258    = msg->vz + pose->pos.vz;
                    part->field_24C    = 0;
                    part->field_24E    = 0;
                    part->field_250    = 0;
                }
                i++;
                pose++;
            } while ((u32)(i & 0xFFFF) < 8U);
            return;
        case 4:
            i = 0;
            do {
                if ((i & 0xFFFF) >= 4U) {
                    taskKill(work->parts[i & 0xFFFF]);
                    work->parts[i & 0xFFFF] = NULL;
                }
                i++;
            } while ((u32)(i & 0xFFFF) < 8U);
            pose = D_actor_560800_80175614;
            i    = 0;
            do {
                if (work->parts[i & 0xFFFF] != NULL) {
                    part            = (Actor560800ModelWork*)work->parts[i & 0xFFFF]->work;
                    coord           = ((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords;
                    coord->sub      = ((TmdObject*)D_actor_560800_801757AC->extra)->coords;
                    part->field_254 = pose->pos.vx;
                    part->field_256 = pose->pos.vy;
                    part->field_258 = pose->pos.vz;
                    Gfx_RotMatrixY(&coord->coord, pose->rot.vy, 1);
                    Gfx_RotMatrixX(&coord->coord, pose->rot.vx + 0x400, 0);
                    Gfx_RotMatrixZ(&coord->coord, pose->rot.vz, 0);
                    part->rot[0].vx = pose->rot.vx;
                    part->rot[0].vy = pose->rot.vy;
                    part->rot[0].vz = pose->rot.vz;
                    part->field_27C = flag;
                    j               = 1;
                    do {
                        Gfx_RotMatrixY(&((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords[j & 0xFFFF].coord, 0, 1);
                        Gfx_RotMatrixX(&((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords[j & 0xFFFF].coord, 0, 0);
                        Gfx_RotMatrixZ(&((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords[j & 0xFFFF].coord, 0, 0);
                        part->rot[j & 0xFFFF].vx = 0;
                        part->rot[j & 0xFFFF].vy = 0;
                        part->rot[j & 0xFFFF].vz = 0;
                        j++;
                    } while ((u32)(j & 0xFFFF) < 7U);
                    coord->flg = 0;
                }
                i++;
                pose++;
            } while ((u32)(i & 0xFFFF) < 8U);
            work->field_46          = 0;
            D_actor_560800_801752E8 = 1;
            D_actor_560800_801752EC = 1;
            return;
    }
    i = 0;
    do {
        if (work->parts[i & 0xFFFF] != NULL) {
            part                                          = (Actor560800ModelWork*)work->parts[i & 0xFFFF]->work;
            coord                                         = ((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords;
            ((TmdObject*)task->extra)->coords->coord.t[0] = msg->vx;
            ((TmdObject*)task->extra)->coords->coord.t[1] = msg->vy;
            ((TmdObject*)task->extra)->coords->coord.t[2] = msg->vz;
            ((TmdObject*)task->extra)->coords->flg        = 0;
            part->field_254                               = pose->pos.vx;
            part->field_256                               = pose->pos.vy;
            part->field_258                               = pose->pos.vz;
            part->field_27C                               = flag;
            if (work->field_48 != 0) {
                Gfx_RotMatrixY(&coord->coord, pose->rot.vy, 1);
                Gfx_RotMatrixX(&coord->coord, pose->rot.vx + 0x400, 0);
                Gfx_RotMatrixZ(&coord->coord, pose->rot.vz, 0);
                part->rot[0].vx = pose->rot.vx;
                part->rot[0].vy = pose->rot.vy;
                part->rot[0].vz = pose->rot.vz;
                j               = 1;
                do {
                    Gfx_RotMatrixY(&((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords[j & 0xFFFF].coord, 0, 1);
                    Gfx_RotMatrixX(&((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords[j & 0xFFFF].coord, 0, 0);
                    Gfx_RotMatrixZ(&((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords[j & 0xFFFF].coord, 0, 0);
                    part->rot[j & 0xFFFF].vx = 0;
                    part->rot[j & 0xFFFF].vy = 0;
                    part->rot[j & 0xFFFF].vz = 0;
                    j++;
                } while ((u32)(j & 0xFFFF) < 7U);
            }
            coord->flg = 0;
        }
        i++;
        pose++;
    } while ((u32)(i & 0xFFFF) < 8U);
    work->field_48 = 0;
    work->field_46 = 0;
}

/// Message handler of the parts task (`D_actor_560800_801756D4`): command 0
/// rebuilds each part's colour matrix from its world translation, 5 and 6 put
/// all eight parts into state 2 / 1, and the rest set this task's state and the
/// `Actor560800PartsWork` halfwords at 0x44-0x4A.
void func_actor_560800_801384EC(Task* task, s32 msgId, GpCmdArg* msg)
{
    Actor560800PartsWork* work;
    Task*                 part;
    TmdObject*            extra;
    VECTOR                vec;
    s32                   i;

    work = (Actor560800PartsWork*)task->work;
    switch (msg->command) {
        case 0:
            i = 0;
            do {
                part = work->parts[i & 0xFFFF];
                if (part != NULL) {
                    extra  = (TmdObject*)part->extra;
                    vec.vx = extra->coords->workm.t[0];
                    vec.vy = ((TmdObject*)part->extra)->coords->workm.t[1];
                    vec.vz = ((TmdObject*)part->extra)->coords->workm.t[2];
                    func_800D7A9C(extra, &vec, 0, 3);
                }
                i += 1;
            } while ((u32)(i & 0xFFFF) < 8U);
            break;
        case 1:
            work->field_46 = 1;
            work->field_48 = 1;
            work->field_4A = 0x83;
            break;
        case 2:
            task->state    = 2;
            work->field_44 = 0;
            break;
        case 3:
            task->state    = 1;
            work->field_46 = 2;
            break;
        case 4:
            work->field_46 = 0;
            work->field_48 = 1;
            break;
        case 5:
            task->state    = 1;
            work->field_46 = 3;
            i              = 0;
            do {
                work->parts[i & 0xFFFF]->state = 2;
                i                             += 1;
            } while ((u32)(i & 0xFFFF) < 8U);
            break;
        case 6:
            task->state    = 1;
            work->field_46 = 4;
            i              = 0;
            do {
                work->parts[i & 0xFFFF]->state = 1;
                i                             += 1;
            } while ((u32)(i & 0xFFFF) < 8U);
            break;
        case 7:
            task->state    = 1;
            work->field_46 = 4;
            work->field_4A = 0x22;
            break;
        case 8:
            task->state = 3;
            break;
    }
}

extern s32  D_80115738;
extern void D_actor_560800_801756D4;
extern u16  D_actor_560800_801756EC[];

/// Handler of the parts task. State 0 allocates its `Actor560800PartsWork`,
/// roots the model at `gGfxViewCoord`, reparents the spawner's task, spawns the
/// eight part tasks and swaps `Gp_LcgState` out for a zero seed; state 2 grows
/// each part's `field_256` up to its `D_actor_560800_80175314` limit; state 3
/// bursts effects on the first remaining part, puts it into state 4 and drops
/// it. Every frame the world position follows part 9 of the controller model
/// `field_4A` selects.
void func_actor_560800_801386D4(Task* task)
{
    Actor560800PartsWork* work;
    Actor560800PartsWork* w;
    Actor560800PartsWork* spawned;
    Actor560800PartsWork* grow;
    Actor560800ModelWork* model;
    GsCOORDINATE2*        root;
    GsCOORDINATE2*        partCoord;
    GsCOORDINATE2*        effCoord;
    GsCOORDINATE2*        c;
    Task*                 part;
    SVECTOR               pos;
    s32                   i;
    s32                   n;
    s16                   k;

    work = (Actor560800PartsWork*)task->work;
    switch (task->state) {
        case 0:
            root       = ((TmdObject*)task->extra)->coords;
            w          = (Actor560800PartsWork*)Mem_Malloc(0x4C, 0);
            task->work = (TaskIdMap*)w;
            if (w == NULL) {
                taskKill(task);
            } else {
                root->sub = &gGfxViewCoord;
                Mem_Set(task->work, 0, 0x4C);
                i                 = 0;
                spawned           = w;
                spawned->field_40 = (Task*)task->spawnArg2;
                task->msgTable    = &D_actor_560800_801756D4;
                Task_Reparent(spawned->field_40, task);
                do {
                    spawned->parts[i & 0xFFFF] =
                        Task_SpawnFromTable(&D_actor_560800_8017575C, 1, (i & 0xFFFF) + 1, (s32)task);
                    i++;
                } while ((u32)(i & 0xFFFF) < 8U);
                D_actor_560800_801757A8 = Gp_LcgState;
                Gp_LcgState             = 0;
            }
            task->state++;
            break;
        case 1:
            break;
        case 2:
            grow = work;
            n    = 0;
            do {
                part = grow->parts[n & 0xFFFF];
                if (part != NULL) {
                    model             = (Actor560800ModelWork*)part->work;
                    partCoord         = ((TmdObject*)part->extra)->coords;
                    model->field_256 += D_actor_560800_801756EC[n & 0xFFFF];
                    if (D_actor_560800_80175314[n & 0xFFFF].pos.vy < (s16)model->field_256) {
                        model->field_256 = D_actor_560800_80175314[n & 0xFFFF].pos.vy;
                    }
                    partCoord->flg = 0;
                }
                n++;
            } while ((u32)(n & 0xFFFF) < 8U);
            break;
        case 3:
            for (k = 0; k < 8; k++) {
                if (work->parts[k] != NULL) {
                    effCoord = &((TmdObject*)work->parts[k]->extra)->coords[3];
                    Gp_SpawnEff(D_80115738, effCoord, 0x10002380, 0);
                    Gp_SpawnEff(D_80115738, effCoord, 0x04003480, 0);
                    i = 0;
                    do {
                        Gp_SpawnEff(D_80115738, effCoord, 0x02002400, 0);
                        i++;
                        Gp_SpawnEff(0x601B4, effCoord, 0x02202300, 0);
                    } while ((u32)(i & 0xFFFF) < 4U);
                    work->parts[k]->state = 4;
                    work->parts[k]        = NULL;
                    break;
                }
            }
            task->state = 1;
            break;
    }
    w = (Actor560800PartsWork*)task->work;
    if (w->field_4A == 0x83) {
        c = ((TmdObject*)((Actor560800Work*)w->field_40->work)->field_4->extra)->coords;
        Gp_ComposeParentWorld(&c[9], &w->world, &pos);
    } else if (w->field_4A == 0x22) {
        c = ((TmdObject*)((Actor560800Work*)w->field_40->work)->field_C->extra)->coords;
        Gp_ComposeParentWorld(&c[9], &w->world, &pos);
    }
    w->world.t[0] = pos.vx;
    w->world.t[1] = pos.vy - 0x78;
    w->world.t[2] = pos.vz;
}

extern s32 D_actor_560800_801756FC[];
extern s32 D_actor_560800_80175714[];
extern s32 D_actor_560800_8017572C[];

void func_actor_560800_80138A4C(Task* task, s32 msgId, GpCmdArg* msg)
{
    Actor560800ModelWork* work;
    TmdObject*            extra;
    VECTOR                vec;

    work = (Actor560800ModelWork*)task->work;
    switch (msg->command) {
        case 0:
            extra  = (TmdObject*)task->extra;
            vec.vx = extra->coords->workm.t[0];
            vec.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
            vec.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
        case 1:
            task->state = 1;
            break;
        case 2:
            Gp_DispatchMsg(task, 0x7D5, 1, 0);
            Gp_DispatchMsg(task, 0x7D4, (s32)D_actor_560800_801756FC, 0);
            work->field_278 = 0x1000;
            task->state     = 2;
            break;
        case 3:
            task->state = 3;
            break;
        case 4:
            task->state = 4;
            break;
        case 5:
            Gp_DispatchMsg(task, 0x7D5, 1, 0);
            Gp_DispatchMsg(task, 0x7D4, (s32)D_actor_560800_80175714, 0);
            work->field_278 = 0x1000;
            task->state     = 5;
            break;
        case 6:
            Gp_DispatchMsg(task, 0x7D5, 1, 0);
            Gp_DispatchMsg(task, 0x7D4, (s32)D_actor_560800_8017572C, 0);
            work->field_282 = 0;
            task->state     = 6;
            break;
    }
}

/// Per-frame pulse of the model part: while bit 0 of
/// `D_actor_560800_801752E8` is set it raises `field_286`, which sinks the root
/// coordinate. Once `field_278` has reached 0x800 the second coordinate is reset
/// to identity and scaled on X/Z by `field_278`, which swings between 0x1000 and
/// 0x1800 in steps of 0x32 with `field_27C` as the direction. The dead `w = work`
/// store is what the match needs: see DECOMPILATION_LEARNINGS.md, "birthing".
void func_actor_560800_80138BCC(Task* task)
{
    Actor560800ModelWork* work;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        c;
    Actor560800ModelWork* w;
    MATRIX*               m;
    VECTOR                scale;

    work  = (Actor560800ModelWork*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    if (D_actor_560800_801752E8 & 1) {
        work->field_286++;
    }
    w                  = work;
    coord->coord.t[1] -= work->field_286;
    if (work->field_278 >= 0x800) {
        work->field_27C    = 0;
        w                  = (Actor560800ModelWork*)task->work;
        c                  = ((TmdObject*)task->extra)->coords;
        m                  = &c[1].coord;
        *(s32*)&m->m[0][0] = 0x1000;
        *(s32*)&m->m[0][2] = 0;
        *(s32*)&m->m[1][1] = 0x1000;
        *(s32*)&m->m[2][0] = 0;
        m->m[2][2]         = 0x1000;
        c++;
        if (w->field_27C == 0) {
            w->field_278 -= 0x32;
            if (w->field_278 < 0x1000) {
                w->field_27C = 1;
            }
        } else if (w->field_27C == 1) {
            w->field_278 += 0x32;
            if (w->field_278 > 0x1800) {
                w->field_27C = 0;
            }
        }
        scale.vx = w->field_278;
        scale.vy = 0x1000;
        scale.vz = w->field_278;
        ScaleMatrix(&c->coord, &scale);
    }
    coord->flg = 0;
}

/// Per-frame rise of the model part, driven by `field_282`: phase 0 lifts the
/// root coordinate until it clears -3000, phase 1 keeps lifting while pulsing
/// the second coordinate's X/Z scale in steps of 0x32 until -1200, and phase 2
/// pulses in steps of 0xC8 until `field_278` drops below 0x1000. Phase 3 sinks
/// this part and the one `Actor560800Work::field_C` names together. Each case
/// needs its own matrix pointer: a shared one is set twice, loses sched1's
/// birthing priority, and swaps the `work`/`field_8` loads.
void func_actor_560800_80138D04(Task* task)
{
    Actor560800ModelWork* work;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        c;
    GsCOORDINATE2*        other;
    Actor560800ModelWork* w;
    MATRIX*               m;
    MATRIX*               m2;
    VECTOR                scale;
    s32                   one;

    work  = (Actor560800ModelWork*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    switch (work->field_282) {
        case 0:
            if (coord->coord.t[1] >= -3000) {
                work->field_282++;
            }
            break;
        case 1:
            if (work->field_278 <= 0x1800) {
                work->field_27C    = 1;
                w                  = (Actor560800ModelWork*)task->work;
                c                  = ((TmdObject*)task->extra)->coords;
                m                  = &c[1].coord;
                one                = 0x1000;
                *(s32*)&m->m[0][0] = one;
                *(s32*)&m->m[0][2] = 0;
                *(s32*)&m->m[1][1] = one;
                *(s32*)&m->m[2][0] = 0;
                m->m[2][2]         = one;
                c++;
                if (w->field_27C == 0) {
                    w->field_278 -= 0x32;
                    if (w->field_278 < one) {
                        w->field_27C = 1;
                    }
                } else if (w->field_27C == 1) {
                    w->field_278 += 0x32;
                    if (w->field_278 > 0x1800) {
                        w->field_27C = 0;
                    }
                }
                scale.vx = w->field_278;
                scale.vy = 0x1000;
                scale.vz = w->field_278;
                ScaleMatrix(&c->coord, &scale);
            }
            if (coord->coord.t[1] >= -1200) {
                work->field_282++;
            }
            break;
        case 2:
            if (work->field_278 >= 0x1000) {
                work->field_27C     = 0;
                w                   = (Actor560800ModelWork*)task->work;
                c                   = ((TmdObject*)task->extra)->coords;
                m2                  = &c[1].coord;
                one                 = 0x1000;
                *(s32*)&m2->m[0][0] = one;
                *(s32*)&m2->m[0][2] = 0;
                *(s32*)&m2->m[1][1] = one;
                *(s32*)&m2->m[2][0] = 0;
                m2->m[2][2]         = one;
                c++;
                if (w->field_27C == 0) {
                    w->field_278 -= 0xC8;
                    if (w->field_278 < one) {
                        w->field_27C = 1;
                    }
                } else if (w->field_27C == 1) {
                    w->field_278 += 0xC8;
                    if (w->field_278 > 0x1800) {
                        w->field_27C = 0;
                    }
                }
                scale.vx = w->field_278;
                scale.vy = 0x1000;
                scale.vz = w->field_278;
                ScaleMatrix(&c->coord, &scale);
            } else {
                work->field_282++;
            }
            coord->flg = 0;
            return;
        case 3:
            other              = ((TmdObject*)((Actor560800Work*)((Task*)task->spawnArg2)->work)->field_C->extra)->coords;
            coord->coord.t[1] -= 20;
            other->coord.t[1] -= 20;
            coord->flg         = 0;
            other->flg         = 0;
            return;
    }
    coord->flg         = 0;
    coord->coord.t[1] += 100;
}

/// Per-frame handler of a model task: state 0 allocates its
/// `Actor560800ModelWork`, parents the root coordinate to `gGfxViewCoord`,
/// publishes the task as `D_actor_560800_801757AC` and resets the root matrix
/// to identity. States 2/5 lift the root
/// by 5 while pulsing the second coordinate's X/Z scale in steps of 0x32, state
/// 3 by 1 in steps of 0xA; 4 and 6 hand off to `func_actor_560800_80138BCC` /
/// `func_actor_560800_80138D04`. Every state but 0 advances the two frame
/// counters. Each case needs its own matrix pointer (and case 0 its own work
/// pointer): a pointer shared across cases is a global pseudo, so the local
/// 0x1000 constant takes `$v0` from it.
extern void D_actor_560800_80175744;

void func_actor_560800_80138FC8(Task* task)
{
    Actor560800ModelWork* work;
    Actor560800ModelWork* w;
    Actor560800ModelWork* mem;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        c;
    MATRIX*               m0;
    MATRIX*               m2;
    MATRIX*               m3;
    MATRIX*               m5;
    VECTOR                scale;
    GsCOORDINATE2*        root;

    switch (task->state) {
        case 0:
            obj        = (TmdObject*)task->extra;
            root       = obj->coords;
            task->work = Mem_Malloc(0x28C, 0);
            if (task->work == NULL) {
                taskKill(task);
            } else {
                Mem_Set(task->work, 0, 0x28C);
                mem            = (Actor560800ModelWork*)task->work;
                root->sub      = &gGfxViewCoord;
                mem->field_26C = (Task*)task->spawnArg2;
                obj->lightMtx  = &mem->light;
                obj->colorMtx  = &mem->color;
                Task_Reparent((Task*)task->spawnArg2, task);
                task->msgTable          = &D_actor_560800_80175744;
                D_actor_560800_801757AC = task;
                m0                      = &root->coord;
                *(s32*)&m0->m[0][0]     = 0x1000;
                *(s32*)&m0->m[0][2]     = 0;
                *(s32*)&m0->m[1][1]     = 0x1000;
                *(s32*)&m0->m[2][0]     = 0;
                m0->m[2][2]             = 0x1000;
            }
            task->state++;
            return;
        case 1:
            ((TmdObject*)task->extra)->flags |= 0x80;
            break;
        case 2:
            coord              = ((TmdObject*)task->extra)->coords;
            work               = (Actor560800ModelWork*)task->work;
            coord->coord.t[1] += 5;
            if (work->field_278 <= 0x1800) {
                work->field_27C     = 1;
                c                   = ((TmdObject*)task->extra)->coords;
                w                   = (Actor560800ModelWork*)task->work;
                m2                  = &c[1].coord;
                *(s32*)&m2->m[0][0] = 0x1000;
                *(s32*)&m2->m[0][2] = 0;
                *(s32*)&m2->m[1][1] = 0x1000;
                *(s32*)&m2->m[2][0] = 0;
                m2->m[2][2]         = 0x1000;
                c++;
                if (w->field_27C == 0) {
                    w->field_278 -= 0x32;
                    if (w->field_278 < 0x1000) {
                        w->field_27C = 1;
                    }
                } else if (w->field_27C == 1) {
                    w->field_278 += 0x32;
                    if (w->field_278 > 0x1800) {
                        w->field_27C = 0;
                    }
                }
                scale.vx = w->field_278;
                scale.vy = 0x1000;
                scale.vz = w->field_278;
                ScaleMatrix(&c->coord, &scale);
            }
            coord->flg = 0;
            break;
        case 3:
            coord              = ((TmdObject*)task->extra)->coords;
            work               = (Actor560800ModelWork*)task->work;
            coord->coord.t[1] += 1;
            if (work->field_278 >= 0x800) {
                work->field_27C     = 0;
                c                   = ((TmdObject*)task->extra)->coords;
                w                   = (Actor560800ModelWork*)task->work;
                m3                  = &c[1].coord;
                *(s32*)&m3->m[0][0] = 0x1000;
                *(s32*)&m3->m[0][2] = 0;
                *(s32*)&m3->m[1][1] = 0x1000;
                *(s32*)&m3->m[2][0] = 0;
                m3->m[2][2]         = 0x1000;
                c++;
                if (w->field_27C == 0) {
                    w->field_278 -= 0xA;
                    if (w->field_278 < 0x1000) {
                        w->field_27C = 1;
                    }
                } else if (w->field_27C == 1) {
                    w->field_278 += 0xA;
                    if (w->field_278 > 0x1800) {
                        w->field_27C = 0;
                    }
                }
                scale.vx = w->field_278;
                scale.vy = 0x1000;
                scale.vz = w->field_278;
                ScaleMatrix(&c->coord, &scale);
            }
            coord->flg = 0;
            break;
        case 4:
            func_actor_560800_80138BCC(task);
            break;
        case 5:
            coord              = ((TmdObject*)task->extra)->coords;
            work               = (Actor560800ModelWork*)task->work;
            coord->coord.t[1] += 5;
            if (work->field_278 <= 0x1800) {
                work->field_27C     = 1;
                c                   = ((TmdObject*)task->extra)->coords;
                w                   = (Actor560800ModelWork*)task->work;
                m5                  = &c[1].coord;
                *(s32*)&m5->m[0][0] = 0x1000;
                *(s32*)&m5->m[0][2] = 0;
                *(s32*)&m5->m[1][1] = 0x1000;
                *(s32*)&m5->m[2][0] = 0;
                m5->m[2][2]         = 0x1000;
                c++;
                if (w->field_27C == 0) {
                    w->field_278 -= 0x32;
                    if (w->field_278 < 0x1000) {
                        w->field_27C = 1;
                    }
                } else if (w->field_27C == 1) {
                    w->field_278 += 0x32;
                    if (w->field_278 > 0x1800) {
                        w->field_27C = 0;
                    }
                }
                scale.vx = w->field_278;
                scale.vy = 0x1000;
                scale.vz = w->field_278;
                ScaleMatrix(&c->coord, &scale);
            }
            coord->flg = 0;
            break;
        case 6:
            func_actor_560800_80138D04(task);
            break;
    }
    D_actor_560800_801752E8 += 2;
    D_actor_560800_801752EC += 1;
}

/// Message 0x7D5 handler of the task `D_actor_560800_801756D4` belongs to: the
/// visibility switch `func_actor_560800_801393EC` performs on a single model,
/// applied to every part task its `Actor560800PartsWork` still holds. `arg2` is
/// the sub-command - 1 clears the 0x84 pair of bits in the part's
/// `TmdObject::flags` and 2 sets it, anything else leaves the parts alone.
void func_actor_560800_80139360(Task* task, s32 arg1, s32 arg2)
{
    Actor560800PartsWork* work;
    TmdObject*            obj;
    Task*                 part;
    s32                   i;

    work = (Actor560800PartsWork*)task->work;
    i    = 0;
    do {
        part = work->parts[i & 0xFFFF];
        if (part != NULL) {
            obj = (TmdObject*)part->extra;
            switch (arg2) {
                case 0:
                    break;
                case 1:
                    obj->flags = obj->flags & 0xFF7B;
                    break;
                case 2:
                    obj->flags = obj->flags | 0x84;
                    break;
            }
        }
        i += 1;
    } while ((u32)(i & 0xFFFF) < 8U);
}

void func_actor_560800_801393EC(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    switch (arg2) {
        case 0:
            break;
        case 1:
            extra->flags = extra->flags & 0xFF7B;
            return;
        case 2:
            extra->flags = extra->flags | 0x84;
            return;
    }
}

/// Message handler that places the task's model from `placement`: the
/// position becomes the model coordinate's translation, the Y, X and Z
/// rotations are applied in that order, and the coordinate is marked for
/// recalculation.
void func_actor_560800_80139440(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}
