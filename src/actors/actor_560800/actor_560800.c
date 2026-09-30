#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/display.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/shelter_b1_pod_service_gantry.h"
#include "../../shared/actor_messages.h"

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

/// Work block of the sub-task `Actor560800Work::field_8` points at, spawned
/// from `D_actor_560800_801718F0` index 5 (`func_actor_560800_80132C60`).
/// That function allocates it with `Mem_Malloc(0x4CC, 0)`, `Mem_Set`s the same
/// 0x4CC bytes and stores it in its own `Task::work` (0x1C), so the size below
/// is the allocation, not a guess. It is a third work block in this overlay,
/// distinct from `Actor560800Work` and `OverlayFadeWork`.
///
/// `rig` is the model's animation rig; the spawn routine stores 0x14 in
/// `field_4BA`, the slot count the reset loop walks.
///
/// `field_4B8` is the animation id the slots are seeded with, `field_4BA` the
/// slot count the reset loop walks 1..count, and `field_4C8` the 0x10 written into each
/// slot's `field_9`. `field_4CA` is a phase counter the same handler reads.
/// `field_4B4` is the animation script `func_actor_560800_80132498` walks by
/// `field_4B8`, with `field_4BE` as its hold counter.
typedef struct Actor560800AnimWork {
    /* 0x000 */ ActorAnimRig20 rig;
    /* 0x474 */ MATRIX         light;
    /* 0x494 */ MATRIX         color;
    /* 0x4B4 */ ActorAnimStep* field_4B4;
    /* 0x4B8 */ s16            field_4B8;
    /* 0x4BA */ u16            field_4BA;
    /* 0x4BC */ u16            field_4BC;
    /* 0x4BE */ s16            field_4BE;
    /* 0x4C0 */ s16            field_4C0;
    /* 0x4C2 */ s16            field_4C2;
    /* 0x4C4 */ s16            field_4C4;
    /* 0x4C6 */ s16            field_4C6;
    /* 0x4C8 */ s16            field_4C8;
    /* 0x4CA */ s16            field_4CA;
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
/// `field_270` / `field_274` / `field_278` are the three `gRandomLcgState` draws
/// `func_actor_560800_801376E0` takes at spawn; `field_280` is the slot count it
/// seeds from the spawner's `spawnArg1`, which `func_actor_560800_80137820` then
/// walks 1..count with `Gp_AnimResetSlot`. `field_27C` / `field_27E` and the
/// 0x38 bytes of `rot` (`Mem_CopyUnaligned`'s source and destination in
/// `func_actor_560800_80136AA8`) belong to the handlers, not to the spawner.
typedef struct Actor560800ModelWork {
    /* 0x000 */ AnimationContext anim;
    /* 0x014 */ AnimationSlot    slots[7];
    /* 0x12C */ byte             poseBuf[0x50];
    /* 0x17C */ MATRIX           field_17C;
    /* 0x19C */ MATRIX           light;
    /* 0x1BC */ MATRIX           color;
    /* 0x1DC */ SVECTOR          rot[7];
    /* 0x214 */ SVECTOR          swing[7];
    /* 0x24C */ s16              field_24C;
    /* 0x24E */ s16              field_24E;
    /* 0x250 */ s16              field_250;
    /* 0x252 */ byte             pad_252[2];
    /* 0x254 */ s16              field_254;
    /* 0x256 */ u16              field_256;
    /* 0x258 */ s16              field_258;
    /* 0x25A */ byte             pad_25A[2];
    /* 0x25C */ u16              swingDir[8];
    /* 0x26C */ Task*            field_26C;
    /* 0x270 */ u32              field_270;
    /* 0x274 */ u32              field_274;
    /* 0x278 */ s16              field_278;
    /* 0x27A */ byte             pad_27A[2];
    /* 0x27C */ s16              field_27C;
    /* 0x27E */ s16              field_27E;
    /* 0x280 */ s16              field_280;
    /* 0x282 */ s16              field_282;
    /* 0x284 */ byte             pad_284[2];
    /* 0x286 */ s16              field_286;
    /* 0x288 */ s16              field_288;
    /* 0x28A */ s16              field_28A;
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

extern ActorTransform D_actor_560800_80175314[];
extern ActorTransform D_actor_560800_801753D4[];
extern ActorTransform D_actor_560800_80175494[];
extern ActorTransform D_actor_560800_80175554[];
extern ActorTransform D_actor_560800_80175614[];

/// Controller task of this overlay, published by `func_actor_560800_80135BD8`
/// and read by the sub-task handlers.
extern Task* D_actor_560800_8017578C;

/// Animation block `func_actor_560800_80136378` points the `source.sets` of its
/// `AnimationPlayRequest` at when it sends message 0x3F4 - the same role
/// `D_actor_400600_80151A48` plays in that overlay.
extern AnimationSet* D_actor_560800_8016EA40[13];

extern ActorAnimStep  D_actor_560800_8016EBE8[];
extern ActorTransform D_actor_560800_8016F1CC[6];

/// Animation bank `func_actor_560800_801376E0` hands `func_800B3F84` as its
/// second argument: a null entry then one animation set per slot of
/// `Actor560800ModelWork`, indexed by the animation id.
extern AnimationSet* D_actor_560800_801752F0[];

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

/// Seed `func_actor_560800_80135D54` loads into `gRandomLcgState` before it hands
/// control back to gameplay.
extern u32 D_actor_560800_801757A8;

/// Pair of blocks `func_actor_560800_80135D54` passes to `func_800E8634`.
extern GpEvsCmd D_actor_560800_8016F5E0[];
extern GpEvsCmd D_actor_560800_80171800[];

/// Flag word whose bit 0 gates `func_actor_560800_80138BCC`'s sink step.
extern s32 D_actor_560800_801752E8;

/// Frame counter `func_actor_560800_80138FC8` raises by one per tick.
extern s32 D_actor_560800_801752EC;

extern Task* D_actor_560800_801757AC;

/// Task descriptor table the actor spawns most of its sub-tasks from, by
/// index.
extern TaskDesc D_actor_560800_801718F0[];

static void func_actor_560800_80133970(Task* arg0);
static void func_actor_560800_80134258(Task* arg0);
static void func_actor_560800_80134384(Task* arg0);
static void func_actor_560800_80134BFC(Task* arg0);

extern TaskDesc       D_actor_560800_8016EA28[];
extern TaskDesc       D_actor_560800_8017575C[];
extern AnimationSet*  D_actor_560800_8016EA74[];
extern AnimationSet*  D_actor_560800_8016EB04[];
extern AnimationSet*  D_actor_560800_8016EB30[];
extern ActorAnimStep  D_actor_560800_8016EC1C[36];
extern ActorAnimStep  D_actor_560800_8016ECAC[6];
extern ActorAnimStep  D_actor_560800_8016ECC4[46];
extern ActorTransform D_actor_560800_8016F154;
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s32, ActorCommand* request);
        void (*call1)(Task*, s32, ActorTransform*);
        void (*call2)(Task*, s32, VECTOR*);
        void (*call3)(Task*, s32, s32);
    } handler;
} Actor560800MessageEntry;
STATIC_ASSERT_SIZEOF(Actor560800MessageEntry, 8);

extern Actor560800MessageEntry D_actor_560800_8016F34C[2];
extern s32                     D_actor_560800_8016F57C[];

static s32 func_actor_560800_80132498(Task* arg0);

extern AnimationSet D_actor_560800_80173FE4;
extern AnimationSet D_actor_560800_80174294;
extern AnimationSet D_actor_560800_80174540;
extern AnimationSet D_actor_560800_801747EC;
extern AnimationSet D_actor_560800_80174A9C;
extern AnimationSet D_actor_560800_80174D64;
extern AnimationSet D_actor_560800_80175020;
extern AnimationSet D_actor_560800_801752C0;

extern TmdSource D_actor_560800_80172788;
extern TmdSource D_actor_560800_8017359C;
extern TmdSource D_actor_560800_80173D48;
void             func_actor_560800_80137820(Task*);
void             func_actor_560800_80137BEC(Task*);
void             func_actor_560800_80137F58(Task*, s32, VECTOR*);
void             func_actor_560800_801384EC(Task*, s32, ActorCommand* msg);
void             func_actor_560800_801386D4(Task*);
void             func_actor_560800_80138A4C(Task*, s32, ActorCommand* msg);
void             func_actor_560800_80138FC8(Task*);
void             func_actor_560800_80139360(Task*, s32, s32);
void             func_actor_560800_801393EC(Task*, s32, s32);
void             func_actor_560800_80139440(Task*, s32, ActorTransform* placement);

extern AnimationSet D_actor_560800_80150C20;
extern AnimationSet D_actor_560800_80150F18;
extern AnimationSet D_actor_560800_80151104;
extern AnimationSet D_actor_560800_801514D8;
extern AnimationSet D_actor_560800_80152630;
extern AnimationSet D_actor_560800_80152B7C;
extern AnimationSet D_actor_560800_80152F14;
extern AnimationSet D_actor_560800_8015317C;
extern AnimationSet D_actor_560800_80153600;
extern AnimationSet D_actor_560800_80153A84;
extern AnimationSet D_actor_560800_80153CCC;
extern AnimationSet D_actor_560800_80154000;
extern AnimationSet D_actor_560800_801541E4;
extern AnimationSet D_actor_560800_801545F4;
extern AnimationSet D_actor_560800_80154974;
extern AnimationSet D_actor_560800_80154C7C;
extern AnimationSet D_actor_560800_80155088;
extern AnimationSet D_actor_560800_8015543C;
extern AnimationSet D_actor_560800_8015589C;
extern AnimationSet D_actor_560800_80155BF0;
extern AnimationSet D_actor_560800_80155E28;
extern AnimationSet D_actor_560800_80156598;
extern AnimationSet D_actor_560800_8015688C;
extern AnimationSet D_actor_560800_80156B08;
extern AnimationSet D_actor_560800_8015729C;
extern AnimationSet D_actor_560800_80157880;
extern AnimationSet D_actor_560800_80157D38;
extern AnimationSet D_actor_560800_80158080;
extern AnimationSet D_actor_560800_801586FC;
extern AnimationSet D_actor_560800_801588D4;
extern AnimationSet D_actor_560800_80158CF0;
extern AnimationSet D_actor_560800_80158EAC;
extern AnimationSet D_actor_560800_801590A0;
extern AnimationSet D_actor_560800_80159318;
extern AnimationSet D_actor_560800_8015C5F0;
extern AnimationSet D_actor_560800_8015C8DC;
extern AnimationSet D_actor_560800_8015CA88;
extern AnimationSet D_actor_560800_8015CC60;
extern AnimationSet D_actor_560800_8015CE40;
extern AnimationSet D_actor_560800_8015D0FC;
extern AnimationSet D_actor_560800_8015D7D4;
extern AnimationSet D_actor_560800_8015DB58;
extern AnimationSet D_actor_560800_8015DD58;
extern AnimationSet D_actor_560800_8015DF14;
extern AnimationSet D_actor_560800_8015E548;
extern AnimationSet D_actor_560800_8015EC0C;
extern AnimationSet D_actor_560800_8015F88C;
extern AnimationSet D_actor_560800_80160190;
extern AnimationSet D_actor_560800_80160A78;
extern AnimationSet D_actor_560800_80160F1C;
extern AnimationSet D_actor_560800_80161548;
extern AnimationSet D_actor_560800_80161DB0;
extern AnimationSet D_actor_560800_801620B0;
extern AnimationSet D_actor_560800_80162A4C;
extern AnimationSet D_actor_560800_80162E34;
extern AnimationSet D_actor_560800_80163400;
extern AnimationSet D_actor_560800_80163848;
extern AnimationSet D_actor_560800_80163EC0;
extern AnimationSet D_actor_560800_8016436C;
extern AnimationSet D_actor_560800_80164A44;
extern AnimationSet D_actor_560800_801657B8;
extern AnimationSet D_actor_560800_80165C38;
extern AnimationSet D_actor_560800_80165E6C;
extern AnimationSet D_actor_560800_80166B28;
extern AnimationSet D_actor_560800_8016734C;
extern AnimationSet D_actor_560800_80167748;
extern AnimationSet D_actor_560800_80167A70;
extern AnimationSet D_actor_560800_80167FD8;
extern AnimationSet D_actor_560800_801685D4;
extern AnimationSet D_actor_560800_80169008;
extern AnimationSet D_actor_560800_80169D7C;
extern AnimationSet D_actor_560800_8016A0A8;
extern AnimationSet D_actor_560800_8016A810;
extern AnimationSet D_actor_560800_8016AC7C;
extern AnimationSet D_actor_560800_8016B188;
extern AnimationSet D_actor_560800_8016BD8C;
extern AnimationSet D_actor_560800_8016C30C;
extern AnimationSet D_actor_560800_8016C624;
extern AnimationSet D_actor_560800_8016CF90;
extern AnimationSet D_actor_560800_8016D5AC;
extern AnimationSet D_actor_560800_8016DD4C;
extern AnimationSet D_actor_560800_8016DE90;
extern AnimationSet D_actor_560800_8016EA00;
extern TmdSource    D_actor_560800_8013E230;
extern TmdSource    D_actor_560800_80143A08;
extern TmdSource    D_actor_560800_8014854C;
extern TmdSource    D_actor_560800_8014EDFC;
extern TmdSource    D_actor_560800_8014F250;
extern TmdSource    D_actor_560800_8014F6A4;
extern TmdSource    D_actor_560800_8014FEA8;
extern TmdSource    D_actor_560800_801502EC;
void                func_actor_560800_801326C4(Task*);
void                func_actor_560800_80132A14(Task*);
void                func_actor_560800_80132C60(Task*);
void                func_actor_560800_80132F64(Task*);
void                func_actor_560800_80133204(void);
void                func_actor_560800_80133648(u32);
void                func_actor_560800_80133750(s32);
void                func_actor_560800_80134B14(s32);
void                func_actor_560800_80135AEC(s32);
void                func_actor_560800_80135D54(Task*);
void                func_actor_560800_80135FA0(Task*);
void                func_actor_560800_80136094(Task*);
void                func_actor_560800_801361A0(Task*, s32, s32);
void                func_actor_560800_80136280(s32);
void                func_actor_560800_801362B0(s32);
void                func_actor_560800_801362E0(s16);
void                func_actor_560800_8013631C(s16);
void                func_actor_560800_80136358(s16);
void                func_actor_560800_80136378(s16);
void                func_actor_560800_801363F8(u16);
void                func_actor_560800_801364A0(u16);
void                func_actor_560800_80136548(void);
void                func_actor_560800_801365B0(s16);
void                func_actor_560800_801365D0(u16);
void                func_actor_560800_80136678(s32);
void                func_actor_560800_801366B0(Task*);
void                func_actor_560800_801367C0(s16);
void                func_actor_560800_801367E0(s16);
void                func_actor_560800_80136818(void);
void                func_actor_560800_80136878(void);
void                func_actor_560800_80136910(void);
void                func_actor_560800_80136930(s32);
void                func_actor_560800_801369A0(void);
void                func_actor_560800_801369E0(Task*);
void                func_actor_560800_80136A20(void);
void                func_actor_560800_80136A54(void);
void                func_actor_560800_80136A88(Task*);

void func_actor_560800_801321A0(Task*);
void func_actor_560800_80135F50(Task*);

TmdBone D_actor_560800_801394CC[19] = {
#include "assets/actor_560800_model_0C410_skeleton.inc"
};

u32 D_actor_560800_80139778[19] = {
#include "assets/actor_560800_model_0C410_partVerts.inc"
};

SVECTOR D_actor_560800_801397C4[312] = {
#include "assets/actor_560800_model_0C410_verts.inc"
};

SVECTOR D_actor_560800_8013A184[338] = {
#include "assets/actor_560800_model_0C410_normals.inc"
};

u32 D_actor_560800_8013AC14[3463] = {
#include "assets/actor_560800_model_0C410_stream.inc"
};

TmdSource D_actor_560800_8013E230 = {
    0,
    18392,
    6232,
    19,
    D_actor_560800_80139778,
    D_actor_560800_801397C4,
    D_actor_560800_8013A184,
    D_actor_560800_801394CC,
    D_actor_560800_8013AC14,
};

TmdBone D_actor_560800_8013E254[19] = {
#include "assets/actor_560800_model_11BE8_skeleton.inc"
};

u32 D_actor_560800_8013E500[19] = {
#include "assets/actor_560800_model_11BE8_partVerts.inc"
};

SVECTOR D_actor_560800_8013E54C[365] = {
#include "assets/actor_560800_model_11BE8_verts.inc"
};

SVECTOR D_actor_560800_8013F0B4[385] = {
#include "assets/actor_560800_model_11BE8_normals.inc"
};

u32 D_actor_560800_8013FCBC[3923] = {
#include "assets/actor_560800_model_11BE8_stream.inc"
};

TmdSource D_actor_560800_80143A08 = {
    0,
    21760,
    5992,
    19,
    D_actor_560800_8013E500,
    D_actor_560800_8013E54C,
    D_actor_560800_8013F0B4,
    D_actor_560800_8013E254,
    D_actor_560800_8013FCBC,
};

TmdBone D_actor_560800_80143A2C[20] = {
#include "assets/actor_560800_model_1672C_skeleton.inc"
};

u32 D_actor_560800_80143CFC[20] = {
#include "assets/actor_560800_model_1672C_partVerts.inc"
};

SVECTOR D_actor_560800_80143D4C[300] = {
#include "assets/actor_560800_model_1672C_verts.inc"
};

SVECTOR D_actor_560800_801446AC[298] = {
#include "assets/actor_560800_model_1672C_normals.inc"
};

u32 D_actor_560800_80144FFC[3412] = {
#include "assets/actor_560800_model_1672C_stream.inc"
};

TmdSource D_actor_560800_8014854C = {
    0,
    18224,
    5696,
    20,
    D_actor_560800_80143CFC,
    D_actor_560800_80143D4C,
    D_actor_560800_801446AC,
    D_actor_560800_80143A2C,
    D_actor_560800_80144FFC,
};

TmdBone D_actor_560800_80148570[19] = {
#include "assets/actor_560800_model_1CFDC_skeleton.inc"
};

u32 D_actor_560800_8014881C[19] = {
#include "assets/actor_560800_model_1CFDC_partVerts.inc"
};

SVECTOR D_actor_560800_80148868[432] = {
#include "assets/actor_560800_model_1CFDC_verts.inc"
};

SVECTOR D_actor_560800_801495E8[444] = {
#include "assets/actor_560800_model_1CFDC_normals.inc"
};

u32 D_actor_560800_8014A3C8[4749] = {
#include "assets/actor_560800_model_1CFDC_stream.inc"
};

TmdSource D_actor_560800_8014EDFC = {
    0,
    26564,
    6624,
    19,
    D_actor_560800_8014881C,
    D_actor_560800_80148868,
    D_actor_560800_801495E8,
    D_actor_560800_80148570,
    D_actor_560800_8014A3C8,
};

TmdBone D_actor_560800_8014EE20[1] = {
#include "assets/actor_560800_model_1D430_skeleton.inc"
};

u32 D_actor_560800_8014EE44[1] = {
#include "assets/actor_560800_model_1D430_partVerts.inc"
};

SVECTOR D_actor_560800_8014EE48[23] = {
#include "assets/actor_560800_model_1D430_verts.inc"
};

SVECTOR D_actor_560800_8014EF00[23] = {
#include "assets/actor_560800_model_1D430_normals.inc"
};

u32 D_actor_560800_8014EFB8[166] = {
#include "assets/actor_560800_model_1D430_stream.inc"
};

TmdSource D_actor_560800_8014F250 = {
    0,
    1148,
    0,
    1,
    D_actor_560800_8014EE44,
    D_actor_560800_8014EE48,
    D_actor_560800_8014EF00,
    D_actor_560800_8014EE20,
    D_actor_560800_8014EFB8,
};

TmdBone D_actor_560800_8014F274[1] = {
#include "assets/actor_560800_model_1D884_skeleton.inc"
};

u32 D_actor_560800_8014F298[1] = {
#include "assets/actor_560800_model_1D884_partVerts.inc"
};

SVECTOR D_actor_560800_8014F29C[23] = {
#include "assets/actor_560800_model_1D884_verts.inc"
};

SVECTOR D_actor_560800_8014F354[23] = {
#include "assets/actor_560800_model_1D884_normals.inc"
};

u32 D_actor_560800_8014F40C[166] = {
#include "assets/actor_560800_model_1D884_stream.inc"
};

TmdSource D_actor_560800_8014F6A4 = {
    0,
    1148,
    0,
    1,
    D_actor_560800_8014F298,
    D_actor_560800_8014F29C,
    D_actor_560800_8014F354,
    D_actor_560800_8014F274,
    D_actor_560800_8014F40C,
};

TmdBone D_actor_560800_8014F6C8[1] = {
#include "assets/actor_560800_model_1E088_skeleton.inc"
};

u32 D_actor_560800_8014F6EC[1] = {
#include "assets/actor_560800_model_1E088_partVerts.inc"
};

SVECTOR D_actor_560800_8014F6F0[43] = {
#include "assets/actor_560800_model_1E088_verts.inc"
};

SVECTOR D_actor_560800_8014F848[41] = {
#include "assets/actor_560800_model_1E088_normals.inc"
};

u32 D_actor_560800_8014F990[326] = {
#include "assets/actor_560800_model_1E088_stream.inc"
};

TmdSource D_actor_560800_8014FEA8 = {
    0,
    2300,
    0,
    1,
    D_actor_560800_8014F6EC,
    D_actor_560800_8014F6F0,
    D_actor_560800_8014F848,
    D_actor_560800_8014F6C8,
    D_actor_560800_8014F990,
};

TmdBone D_actor_560800_8014FECC[1] = {
#include "assets/actor_560800_model_1E4CC_skeleton.inc"
};

u32 D_actor_560800_8014FEF0[1] = {
#include "assets/actor_560800_model_1E4CC_partVerts.inc"
};

SVECTOR D_actor_560800_8014FEF4[22] = {
#include "assets/actor_560800_model_1E4CC_verts.inc"
};

SVECTOR D_actor_560800_8014FFA4[24] = {
#include "assets/actor_560800_model_1E4CC_normals.inc"
};

u32 D_actor_560800_80150064[162] = {
#include "assets/actor_560800_model_1E4CC_stream.inc"
};

TmdSource D_actor_560800_801502EC = {
    0,
    1108,
    0,
    1,
    D_actor_560800_8014FEF0,
    D_actor_560800_8014FEF4,
    D_actor_560800_8014FFA4,
    D_actor_560800_8014FECC,
    D_actor_560800_80150064,
};

AnimationPackedPose D_actor_560800_80150310[20] = {
#include "assets/actor_560800_animation_1EE00_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80150400[111] = {
#include "assets/actor_560800_animation_1EE00_bank4.inc"
};

AnimationRecord D_actor_560800_801505BC[399] = {
#include "assets/actor_560800_animation_1EE00_records.inc"
};

u16 D_actor_560800_80150BF8[20] = {
#include "assets/actor_560800_animation_1EE00_indices.inc"
};

AnimationSet D_actor_560800_80150C20 = {
    D_actor_560800_801505BC,
    D_actor_560800_80150BF8,
    { NULL, D_actor_560800_80150310, NULL, NULL, D_actor_560800_80150400, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80150C48[3] = {
#include "assets/actor_560800_animation_1F0F8_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80150C6C[61] = {
#include "assets/actor_560800_animation_1F0F8_bank4.inc"
};

AnimationRecord D_actor_560800_80150D60[100] = {
#include "assets/actor_560800_animation_1F0F8_records.inc"
};

u16 D_actor_560800_80150EF0[20] = {
#include "assets/actor_560800_animation_1F0F8_indices.inc"
};

AnimationSet D_actor_560800_80150F18 = {
    D_actor_560800_80150D60,
    D_actor_560800_80150EF0,
    { NULL, D_actor_560800_80150C48, NULL, NULL, D_actor_560800_80150C6C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80150F40[2] = {
#include "assets/actor_560800_animation_1F2E4_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80150F58[15] = {
#include "assets/actor_560800_animation_1F2E4_bank4.inc"
};

AnimationRecord D_actor_560800_80150F94[82] = {
#include "assets/actor_560800_animation_1F2E4_records.inc"
};

u16 D_actor_560800_801510DC[20] = {
#include "assets/actor_560800_animation_1F2E4_indices.inc"
};

AnimationSet D_actor_560800_80151104 = {
    D_actor_560800_80150F94,
    D_actor_560800_801510DC,
    { NULL, D_actor_560800_80150F40, NULL, NULL, D_actor_560800_80150F58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015112C[16] = {
#include "assets/actor_560800_animation_1F6B8_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801511EC[63] = {
#include "assets/actor_560800_animation_1F6B8_bank4.inc"
};

AnimationRecord D_actor_560800_801512E8[114] = {
#include "assets/actor_560800_animation_1F6B8_records.inc"
};

u16 D_actor_560800_801514B0[20] = {
#include "assets/actor_560800_animation_1F6B8_indices.inc"
};

AnimationSet D_actor_560800_801514D8 = {
    D_actor_560800_801512E8,
    D_actor_560800_801514B0,
    { NULL, D_actor_560800_8015112C, NULL, NULL, D_actor_560800_801511EC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80151500[128] = {
#include "assets/actor_560800_animation_20810_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80151B00[202] = {
#include "assets/actor_560800_animation_20810_bank4.inc"
};

AnimationRecord D_actor_560800_80151E28[504] = {
#include "assets/actor_560800_animation_20810_records.inc"
};

u16 D_actor_560800_80152608[20] = {
#include "assets/actor_560800_animation_20810_indices.inc"
};

AnimationSet D_actor_560800_80152630 = {
    D_actor_560800_80151E28,
    D_actor_560800_80152608,
    { NULL, D_actor_560800_80151500, NULL, NULL, D_actor_560800_80151B00, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80152658[17] = {
#include "assets/actor_560800_animation_20D5C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80152724[83] = {
#include "assets/actor_560800_animation_20D5C_bank4.inc"
};

AnimationRecord D_actor_560800_80152870[185] = {
#include "assets/actor_560800_animation_20D5C_records.inc"
};

u16 D_actor_560800_80152B54[20] = {
#include "assets/actor_560800_animation_20D5C_indices.inc"
};

AnimationSet D_actor_560800_80152B7C = {
    D_actor_560800_80152870,
    D_actor_560800_80152B54,
    { NULL, D_actor_560800_80152658, NULL, NULL, D_actor_560800_80152724, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80152BA4[5] = {
#include "assets/actor_560800_animation_210F4_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80152BE0[72] = {
#include "assets/actor_560800_animation_210F4_bank4.inc"
};

AnimationRecord D_actor_560800_80152D00[123] = {
#include "assets/actor_560800_animation_210F4_records.inc"
};

u16 D_actor_560800_80152EEC[20] = {
#include "assets/actor_560800_animation_210F4_indices.inc"
};

AnimationSet D_actor_560800_80152F14 = {
    D_actor_560800_80152D00,
    D_actor_560800_80152EEC,
    { NULL, D_actor_560800_80152BA4, NULL, NULL, D_actor_560800_80152BE0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80152F3C[3] = {
#include "assets/actor_560800_animation_2135C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80152F60[42] = {
#include "assets/actor_560800_animation_2135C_bank4.inc"
};

AnimationRecord D_actor_560800_80153008[83] = {
#include "assets/actor_560800_animation_2135C_records.inc"
};

u16 D_actor_560800_80153154[20] = {
#include "assets/actor_560800_animation_2135C_indices.inc"
};

AnimationSet D_actor_560800_8015317C = {
    D_actor_560800_80153008,
    D_actor_560800_80153154,
    { NULL, D_actor_560800_80152F3C, NULL, NULL, D_actor_560800_80152F60, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801531A4[2] = {
#include "assets/actor_560800_animation_217E0_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801531BC[63] = {
#include "assets/actor_560800_animation_217E0_bank4.inc"
};

AnimationRecord D_actor_560800_801532B8[200] = {
#include "assets/actor_560800_animation_217E0_records.inc"
};

u16 D_actor_560800_801535D8[20] = {
#include "assets/actor_560800_animation_217E0_indices.inc"
};

AnimationSet D_actor_560800_80153600 = {
    D_actor_560800_801532B8,
    D_actor_560800_801535D8,
    { NULL, D_actor_560800_801531A4, NULL, NULL, D_actor_560800_801531BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80153628[4] = {
#include "assets/actor_560800_animation_21C64_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80153658[105] = {
#include "assets/actor_560800_animation_21C64_bank4.inc"
};

AnimationRecord D_actor_560800_801537FC[152] = {
#include "assets/actor_560800_animation_21C64_records.inc"
};

u16 D_actor_560800_80153A5C[20] = {
#include "assets/actor_560800_animation_21C64_indices.inc"
};

AnimationSet D_actor_560800_80153A84 = {
    D_actor_560800_801537FC,
    D_actor_560800_80153A5C,
    { NULL, D_actor_560800_80153628, NULL, NULL, D_actor_560800_80153658, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80153AAC[6] = {
#include "assets/actor_560800_animation_21EAC_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80153AF4[33] = {
#include "assets/actor_560800_animation_21EAC_bank4.inc"
};

AnimationRecord D_actor_560800_80153B78[75] = {
#include "assets/actor_560800_animation_21EAC_records.inc"
};

u16 D_actor_560800_80153CA4[20] = {
#include "assets/actor_560800_animation_21EAC_indices.inc"
};

AnimationSet D_actor_560800_80153CCC = {
    D_actor_560800_80153B78,
    D_actor_560800_80153CA4,
    { NULL, D_actor_560800_80153AAC, NULL, NULL, D_actor_560800_80153AF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80153CF4[7] = {
#include "assets/actor_560800_animation_221E0_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80153D48[54] = {
#include "assets/actor_560800_animation_221E0_bank4.inc"
};

AnimationRecord D_actor_560800_80153E20[110] = {
#include "assets/actor_560800_animation_221E0_records.inc"
};

u16 D_actor_560800_80153FD8[20] = {
#include "assets/actor_560800_animation_221E0_indices.inc"
};

AnimationSet D_actor_560800_80154000 = {
    D_actor_560800_80153E20,
    D_actor_560800_80153FD8,
    { NULL, D_actor_560800_80153CF4, NULL, NULL, D_actor_560800_80153D48, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80154028[2] = {
#include "assets/actor_560800_animation_223C4_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80154040[24] = {
#include "assets/actor_560800_animation_223C4_bank4.inc"
};

AnimationRecord D_actor_560800_801540A0[71] = {
#include "assets/actor_560800_animation_223C4_records.inc"
};

u16 D_actor_560800_801541BC[20] = {
#include "assets/actor_560800_animation_223C4_indices.inc"
};

AnimationSet D_actor_560800_801541E4 = {
    D_actor_560800_801540A0,
    D_actor_560800_801541BC,
    { NULL, D_actor_560800_80154028, NULL, NULL, D_actor_560800_80154040, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015420C[6] = {
#include "assets/actor_560800_animation_227D4_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80154254[53] = {
#include "assets/actor_560800_animation_227D4_bank4.inc"
};

AnimationRecord D_actor_560800_80154328[169] = {
#include "assets/actor_560800_animation_227D4_records.inc"
};

u16 D_actor_560800_801545CC[20] = {
#include "assets/actor_560800_animation_227D4_indices.inc"
};

AnimationSet D_actor_560800_801545F4 = {
    D_actor_560800_80154328,
    D_actor_560800_801545CC,
    { NULL, D_actor_560800_8015420C, NULL, NULL, D_actor_560800_80154254, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015461C[6] = {
#include "assets/actor_560800_animation_22B54_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80154664[70] = {
#include "assets/actor_560800_animation_22B54_bank4.inc"
};

AnimationRecord D_actor_560800_8015477C[116] = {
#include "assets/actor_560800_animation_22B54_records.inc"
};

u16 D_actor_560800_8015494C[20] = {
#include "assets/actor_560800_animation_22B54_indices.inc"
};

AnimationSet D_actor_560800_80154974 = {
    D_actor_560800_8015477C,
    D_actor_560800_8015494C,
    { NULL, D_actor_560800_8015461C, NULL, NULL, D_actor_560800_80154664, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015499C[2] = {
#include "assets/actor_560800_animation_22E5C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801549B4[49] = {
#include "assets/actor_560800_animation_22E5C_bank4.inc"
};

AnimationRecord D_actor_560800_80154A78[119] = {
#include "assets/actor_560800_animation_22E5C_records.inc"
};

u16 D_actor_560800_80154C54[20] = {
#include "assets/actor_560800_animation_22E5C_indices.inc"
};

AnimationSet D_actor_560800_80154C7C = {
    D_actor_560800_80154A78,
    D_actor_560800_80154C54,
    { NULL, D_actor_560800_8015499C, NULL, NULL, D_actor_560800_801549B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80154CA4[9] = {
#include "assets/actor_560800_animation_23268_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80154D10[68] = {
#include "assets/actor_560800_animation_23268_bank4.inc"
};

AnimationRecord D_actor_560800_80154E20[144] = {
#include "assets/actor_560800_animation_23268_records.inc"
};

u16 D_actor_560800_80155060[20] = {
#include "assets/actor_560800_animation_23268_indices.inc"
};

AnimationSet D_actor_560800_80155088 = {
    D_actor_560800_80154E20,
    D_actor_560800_80155060,
    { NULL, D_actor_560800_80154CA4, NULL, NULL, D_actor_560800_80154D10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801550B0[12] = {
#include "assets/actor_560800_animation_2361C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80155140[68] = {
#include "assets/actor_560800_animation_2361C_bank4.inc"
};

AnimationRecord D_actor_560800_80155250[113] = {
#include "assets/actor_560800_animation_2361C_records.inc"
};

u16 D_actor_560800_80155414[20] = {
#include "assets/actor_560800_animation_2361C_indices.inc"
};

AnimationSet D_actor_560800_8015543C = {
    D_actor_560800_80155250,
    D_actor_560800_80155414,
    { NULL, D_actor_560800_801550B0, NULL, NULL, D_actor_560800_80155140, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80155464[14] = {
#include "assets/actor_560800_animation_23A7C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015550C[81] = {
#include "assets/actor_560800_animation_23A7C_bank4.inc"
};

AnimationRecord D_actor_560800_80155650[137] = {
#include "assets/actor_560800_animation_23A7C_records.inc"
};

u16 D_actor_560800_80155874[20] = {
#include "assets/actor_560800_animation_23A7C_indices.inc"
};

AnimationSet D_actor_560800_8015589C = {
    D_actor_560800_80155650,
    D_actor_560800_80155874,
    { NULL, D_actor_560800_80155464, NULL, NULL, D_actor_560800_8015550C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801558C4[15] = {
#include "assets/actor_560800_animation_23DD0_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80155978[34] = {
#include "assets/actor_560800_animation_23DD0_bank4.inc"
};

AnimationRecord D_actor_560800_80155A00[114] = {
#include "assets/actor_560800_animation_23DD0_records.inc"
};

u16 D_actor_560800_80155BC8[20] = {
#include "assets/actor_560800_animation_23DD0_indices.inc"
};

AnimationSet D_actor_560800_80155BF0 = {
    D_actor_560800_80155A00,
    D_actor_560800_80155BC8,
    { NULL, D_actor_560800_801558C4, NULL, NULL, D_actor_560800_80155978, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80155C18[3] = {
#include "assets/actor_560800_animation_24008_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80155C3C[37] = {
#include "assets/actor_560800_animation_24008_bank4.inc"
};

AnimationRecord D_actor_560800_80155CD0[76] = {
#include "assets/actor_560800_animation_24008_records.inc"
};

u16 D_actor_560800_80155E00[20] = {
#include "assets/actor_560800_animation_24008_indices.inc"
};

AnimationSet D_actor_560800_80155E28 = {
    D_actor_560800_80155CD0,
    D_actor_560800_80155E00,
    { NULL, D_actor_560800_80155C18, NULL, NULL, D_actor_560800_80155C3C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80155E50[32] = {
#include "assets/actor_560800_animation_24778_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80155FD0[140] = {
#include "assets/actor_560800_animation_24778_bank4.inc"
};

AnimationRecord D_actor_560800_80156200[220] = {
#include "assets/actor_560800_animation_24778_records.inc"
};

u16 D_actor_560800_80156570[20] = {
#include "assets/actor_560800_animation_24778_indices.inc"
};

AnimationSet D_actor_560800_80156598 = {
    D_actor_560800_80156200,
    D_actor_560800_80156570,
    { NULL, D_actor_560800_80155E50, NULL, NULL, D_actor_560800_80155FD0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801565C0[3] = {
#include "assets/actor_560800_animation_24A6C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801565E4[64] = {
#include "assets/actor_560800_animation_24A6C_bank4.inc"
};

AnimationRecord D_actor_560800_801566E4[96] = {
#include "assets/actor_560800_animation_24A6C_records.inc"
};

u16 D_actor_560800_80156864[20] = {
#include "assets/actor_560800_animation_24A6C_indices.inc"
};

AnimationSet D_actor_560800_8015688C = {
    D_actor_560800_801566E4,
    D_actor_560800_80156864,
    { NULL, D_actor_560800_801565C0, NULL, NULL, D_actor_560800_801565E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801568B4[2] = {
#include "assets/actor_560800_animation_24CE8_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801568CC[32] = {
#include "assets/actor_560800_animation_24CE8_bank4.inc"
};

AnimationRecord D_actor_560800_8015694C[101] = {
#include "assets/actor_560800_animation_24CE8_records.inc"
};

u16 D_actor_560800_80156AE0[20] = {
#include "assets/actor_560800_animation_24CE8_indices.inc"
};

AnimationSet D_actor_560800_80156B08 = {
    D_actor_560800_8015694C,
    D_actor_560800_80156AE0,
    { NULL, D_actor_560800_801568B4, NULL, NULL, D_actor_560800_801568CC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80156B30[21] = {
#include "assets/actor_560800_animation_2547C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80156C2C[156] = {
#include "assets/actor_560800_animation_2547C_bank4.inc"
};

AnimationRecord D_actor_560800_80156E9C[246] = {
#include "assets/actor_560800_animation_2547C_records.inc"
};

u16 D_actor_560800_80157274[20] = {
#include "assets/actor_560800_animation_2547C_indices.inc"
};

AnimationSet D_actor_560800_8015729C = {
    D_actor_560800_80156E9C,
    D_actor_560800_80157274,
    { NULL, D_actor_560800_80156B30, NULL, NULL, D_actor_560800_80156C2C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801572C4[2] = {
#include "assets/actor_560800_animation_25A60_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801572DC[152] = {
#include "assets/actor_560800_animation_25A60_bank4.inc"
};

AnimationRecord D_actor_560800_8015753C[199] = {
#include "assets/actor_560800_animation_25A60_records.inc"
};

u16 D_actor_560800_80157858[20] = {
#include "assets/actor_560800_animation_25A60_indices.inc"
};

AnimationSet D_actor_560800_80157880 = {
    D_actor_560800_8015753C,
    D_actor_560800_80157858,
    { NULL, D_actor_560800_801572C4, NULL, NULL, D_actor_560800_801572DC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801578A8[6] = {
#include "assets/actor_560800_animation_25F18_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801578F0[83] = {
#include "assets/actor_560800_animation_25F18_bank4.inc"
};

AnimationRecord D_actor_560800_80157A3C[181] = {
#include "assets/actor_560800_animation_25F18_records.inc"
};

u16 D_actor_560800_80157D10[20] = {
#include "assets/actor_560800_animation_25F18_indices.inc"
};

AnimationSet D_actor_560800_80157D38 = {
    D_actor_560800_80157A3C,
    D_actor_560800_80157D10,
    { NULL, D_actor_560800_801578A8, NULL, NULL, D_actor_560800_801578F0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80157D60[5] = {
#include "assets/actor_560800_animation_26260_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80157D9C[66] = {
#include "assets/actor_560800_animation_26260_bank4.inc"
};

AnimationRecord D_actor_560800_80157EA4[109] = {
#include "assets/actor_560800_animation_26260_records.inc"
};

u16 D_actor_560800_80158058[20] = {
#include "assets/actor_560800_animation_26260_indices.inc"
};

AnimationSet D_actor_560800_80158080 = {
    D_actor_560800_80157EA4,
    D_actor_560800_80158058,
    { NULL, D_actor_560800_80157D60, NULL, NULL, D_actor_560800_80157D9C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801580A8[10] = {
#include "assets/actor_560800_animation_268DC_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80158120[142] = {
#include "assets/actor_560800_animation_268DC_bank4.inc"
};

AnimationRecord D_actor_560800_80158358[223] = {
#include "assets/actor_560800_animation_268DC_records.inc"
};

u16 D_actor_560800_801586D4[20] = {
#include "assets/actor_560800_animation_268DC_indices.inc"
};

AnimationSet D_actor_560800_801586FC = {
    D_actor_560800_80158358,
    D_actor_560800_801586D4,
    { NULL, D_actor_560800_801580A8, NULL, NULL, D_actor_560800_80158120, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80158724[3] = {
#include "assets/actor_560800_animation_26AB4_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80158748[29] = {
#include "assets/actor_560800_animation_26AB4_bank4.inc"
};

AnimationRecord D_actor_560800_801587BC[60] = {
#include "assets/actor_560800_animation_26AB4_records.inc"
};

u16 D_actor_560800_801588AC[20] = {
#include "assets/actor_560800_animation_26AB4_indices.inc"
};

AnimationSet D_actor_560800_801588D4 = {
    D_actor_560800_801587BC,
    D_actor_560800_801588AC,
    { NULL, D_actor_560800_80158724, NULL, NULL, D_actor_560800_80158748, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801588FC[3] = {
#include "assets/actor_560800_animation_26ED0_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80158920[80] = {
#include "assets/actor_560800_animation_26ED0_bank4.inc"
};

AnimationRecord D_actor_560800_80158A60[154] = {
#include "assets/actor_560800_animation_26ED0_records.inc"
};

u16 D_actor_560800_80158CC8[20] = {
#include "assets/actor_560800_animation_26ED0_indices.inc"
};

AnimationSet D_actor_560800_80158CF0 = {
    D_actor_560800_80158A60,
    D_actor_560800_80158CC8,
    { NULL, D_actor_560800_801588FC, NULL, NULL, D_actor_560800_80158920, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80158D18[2] = {
#include "assets/actor_560800_animation_2708C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80158D30[25] = {
#include "assets/actor_560800_animation_2708C_bank4.inc"
};

AnimationRecord D_actor_560800_80158D94[60] = {
#include "assets/actor_560800_animation_2708C_records.inc"
};

u16 D_actor_560800_80158E84[20] = {
#include "assets/actor_560800_animation_2708C_indices.inc"
};

AnimationSet D_actor_560800_80158EAC = {
    D_actor_560800_80158D94,
    D_actor_560800_80158E84,
    { NULL, D_actor_560800_80158D18, NULL, NULL, D_actor_560800_80158D30, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80158ED4[2] = {
#include "assets/actor_560800_animation_27280_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80158EEC[19] = {
#include "assets/actor_560800_animation_27280_bank4.inc"
};

AnimationRecord D_actor_560800_80158F38[80] = {
#include "assets/actor_560800_animation_27280_records.inc"
};

u16 D_actor_560800_80159078[20] = {
#include "assets/actor_560800_animation_27280_indices.inc"
};

AnimationSet D_actor_560800_801590A0 = {
    D_actor_560800_80158F38,
    D_actor_560800_80159078,
    { NULL, D_actor_560800_80158ED4, NULL, NULL, D_actor_560800_80158EEC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801590C8[2] = {
#include "assets/actor_560800_animation_274F8_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801590E0[50] = {
#include "assets/actor_560800_animation_274F8_bank4.inc"
};

AnimationRecord D_actor_560800_801591A8[82] = {
#include "assets/actor_560800_animation_274F8_records.inc"
};

u16 D_actor_560800_801592F0[20] = {
#include "assets/actor_560800_animation_274F8_indices.inc"
};

AnimationSet D_actor_560800_80159318 = {
    D_actor_560800_801591A8,
    D_actor_560800_801592F0,
    { NULL, D_actor_560800_801590C8, NULL, NULL, D_actor_560800_801590E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80159340[15] = {
#include "assets/actor_560800_animation_27A30_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801593F4[113] = {
#include "assets/actor_560800_animation_27A30_bank4.inc"
};

AnimationRecord D_actor_560800_801595B8[156] = {
#include "assets/actor_560800_animation_27A30_records.inc"
};

u16 D_actor_560800_80159828[20] = {
#include "assets/actor_560800_animation_27A30_indices.inc"
};

AnimationSet D_actor_560800_80159850 = {
    D_actor_560800_801595B8,
    D_actor_560800_80159828,
    { NULL, D_actor_560800_80159340, NULL, NULL, D_actor_560800_801593F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80159878[7] = {
#include "assets/actor_560800_animation_27ED8_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801598CC[102] = {
#include "assets/actor_560800_animation_27ED8_bank4.inc"
};

AnimationRecord D_actor_560800_80159A64[155] = {
#include "assets/actor_560800_animation_27ED8_records.inc"
};

u16 D_actor_560800_80159CD0[20] = {
#include "assets/actor_560800_animation_27ED8_indices.inc"
};

AnimationSet D_actor_560800_80159CF8 = {
    D_actor_560800_80159A64,
    D_actor_560800_80159CD0,
    { NULL, D_actor_560800_80159878, NULL, NULL, D_actor_560800_801598CC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80159D20[34] = {
#include "assets/actor_560800_animation_288D4_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80159EB8[225] = {
#include "assets/actor_560800_animation_288D4_bank4.inc"
};

AnimationRecord D_actor_560800_8015A23C[292] = {
#include "assets/actor_560800_animation_288D4_records.inc"
};

u16 D_actor_560800_8015A6CC[20] = {
#include "assets/actor_560800_animation_288D4_indices.inc"
};

AnimationSet D_actor_560800_8015A6F4 = {
    D_actor_560800_8015A23C,
    D_actor_560800_8015A6CC,
    { NULL, D_actor_560800_80159D20, NULL, NULL, D_actor_560800_80159EB8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015A71C[43] = {
#include "assets/actor_560800_animation_292D0_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015A920[210] = {
#include "assets/actor_560800_animation_292D0_bank4.inc"
};

AnimationRecord D_actor_560800_8015AC68[280] = {
#include "assets/actor_560800_animation_292D0_records.inc"
};

u16 D_actor_560800_8015B0C8[20] = {
#include "assets/actor_560800_animation_292D0_indices.inc"
};

AnimationSet D_actor_560800_8015B0F0 = {
    D_actor_560800_8015AC68,
    D_actor_560800_8015B0C8,
    { NULL, D_actor_560800_8015A71C, NULL, NULL, D_actor_560800_8015A920, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015B118[2] = {
#include "assets/actor_560800_animation_295E0_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015B130[46] = {
#include "assets/actor_560800_animation_295E0_bank4.inc"
};

AnimationRecord D_actor_560800_8015B1E8[124] = {
#include "assets/actor_560800_animation_295E0_records.inc"
};

u16 D_actor_560800_8015B3D8[20] = {
#include "assets/actor_560800_animation_295E0_indices.inc"
};

AnimationSet D_actor_560800_8015B400 = {
    D_actor_560800_8015B1E8,
    D_actor_560800_8015B3D8,
    { NULL, D_actor_560800_8015B118, NULL, NULL, D_actor_560800_8015B130, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015B428[2] = {
#include "assets/actor_560800_animation_29894_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015B440[36] = {
#include "assets/actor_560800_animation_29894_bank4.inc"
};

AnimationRecord D_actor_560800_8015B4D0[111] = {
#include "assets/actor_560800_animation_29894_records.inc"
};

u16 D_actor_560800_8015B68C[20] = {
#include "assets/actor_560800_animation_29894_indices.inc"
};

AnimationSet D_actor_560800_8015B6B4 = {
    D_actor_560800_8015B4D0,
    D_actor_560800_8015B68C,
    { NULL, D_actor_560800_8015B428, NULL, NULL, D_actor_560800_8015B440, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015B6DC[6] = {
#include "assets/actor_560800_animation_29B98_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015B724[46] = {
#include "assets/actor_560800_animation_29B98_bank4.inc"
};

AnimationRecord D_actor_560800_8015B7DC[109] = {
#include "assets/actor_560800_animation_29B98_records.inc"
};

u16 D_actor_560800_8015B990[20] = {
#include "assets/actor_560800_animation_29B98_indices.inc"
};

AnimationSet D_actor_560800_8015B9B8 = {
    D_actor_560800_8015B7DC,
    D_actor_560800_8015B990,
    { NULL, D_actor_560800_8015B6DC, NULL, NULL, D_actor_560800_8015B724, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015B9E0[10] = {
#include "assets/actor_560800_animation_2A294_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015BA58[147] = {
#include "assets/actor_560800_animation_2A294_bank4.inc"
};

AnimationRecord D_actor_560800_8015BCA4[250] = {
#include "assets/actor_560800_animation_2A294_records.inc"
};

u16 D_actor_560800_8015C08C[20] = {
#include "assets/actor_560800_animation_2A294_indices.inc"
};

AnimationSet D_actor_560800_8015C0B4 = {
    D_actor_560800_8015BCA4,
    D_actor_560800_8015C08C,
    { NULL, D_actor_560800_8015B9E0, NULL, NULL, D_actor_560800_8015BA58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015C0DC[4] = {
#include "assets/actor_560800_animation_2A50C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015C10C[51] = {
#include "assets/actor_560800_animation_2A50C_bank4.inc"
};

AnimationRecord D_actor_560800_8015C1D8[75] = {
#include "assets/actor_560800_animation_2A50C_records.inc"
};

u16 D_actor_560800_8015C304[20] = {
#include "assets/actor_560800_animation_2A50C_indices.inc"
};

AnimationSet D_actor_560800_8015C32C = {
    D_actor_560800_8015C1D8,
    D_actor_560800_8015C304,
    { NULL, D_actor_560800_8015C0DC, NULL, NULL, D_actor_560800_8015C10C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015C354[3] = {
#include "assets/actor_560800_animation_2A7D0_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015C378[30] = {
#include "assets/actor_560800_animation_2A7D0_bank4.inc"
};

AnimationRecord D_actor_560800_8015C3F0[118] = {
#include "assets/actor_560800_animation_2A7D0_records.inc"
};

u16 D_actor_560800_8015C5C8[20] = {
#include "assets/actor_560800_animation_2A7D0_indices.inc"
};

AnimationSet D_actor_560800_8015C5F0 = {
    D_actor_560800_8015C3F0,
    D_actor_560800_8015C5C8,
    { NULL, D_actor_560800_8015C354, NULL, NULL, D_actor_560800_8015C378, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015C618[5] = {
#include "assets/actor_560800_animation_2AABC_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015C654[40] = {
#include "assets/actor_560800_animation_2AABC_bank4.inc"
};

AnimationRecord D_actor_560800_8015C6F4[112] = {
#include "assets/actor_560800_animation_2AABC_records.inc"
};

u16 D_actor_560800_8015C8B4[20] = {
#include "assets/actor_560800_animation_2AABC_indices.inc"
};

AnimationSet D_actor_560800_8015C8DC = {
    D_actor_560800_8015C6F4,
    D_actor_560800_8015C8B4,
    { NULL, D_actor_560800_8015C618, NULL, NULL, D_actor_560800_8015C654, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015C904[2] = {
#include "assets/actor_560800_animation_2AC68_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015C91C[18] = {
#include "assets/actor_560800_animation_2AC68_bank4.inc"
};

AnimationRecord D_actor_560800_8015C964[63] = {
#include "assets/actor_560800_animation_2AC68_records.inc"
};

u16 D_actor_560800_8015CA60[20] = {
#include "assets/actor_560800_animation_2AC68_indices.inc"
};

AnimationSet D_actor_560800_8015CA88 = {
    D_actor_560800_8015C964,
    D_actor_560800_8015CA60,
    { NULL, D_actor_560800_8015C904, NULL, NULL, D_actor_560800_8015C91C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015CAB0[3] = {
#include "assets/actor_560800_animation_2AE40_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015CAD4[11] = {
#include "assets/actor_560800_animation_2AE40_bank4.inc"
};

AnimationRecord D_actor_560800_8015CB00[78] = {
#include "assets/actor_560800_animation_2AE40_records.inc"
};

u16 D_actor_560800_8015CC38[20] = {
#include "assets/actor_560800_animation_2AE40_indices.inc"
};

AnimationSet D_actor_560800_8015CC60 = {
    D_actor_560800_8015CB00,
    D_actor_560800_8015CC38,
    { NULL, D_actor_560800_8015CAB0, NULL, NULL, D_actor_560800_8015CAD4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015CC88[2] = {
#include "assets/actor_560800_animation_2B020_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015CCA0[26] = {
#include "assets/actor_560800_animation_2B020_bank4.inc"
};

AnimationRecord D_actor_560800_8015CD08[68] = {
#include "assets/actor_560800_animation_2B020_records.inc"
};

u16 D_actor_560800_8015CE18[20] = {
#include "assets/actor_560800_animation_2B020_indices.inc"
};

AnimationSet D_actor_560800_8015CE40 = {
    D_actor_560800_8015CD08,
    D_actor_560800_8015CE18,
    { NULL, D_actor_560800_8015CC88, NULL, NULL, D_actor_560800_8015CCA0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015CE68[2] = {
#include "assets/actor_560800_animation_2B2DC_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015CE80[55] = {
#include "assets/actor_560800_animation_2B2DC_bank4.inc"
};

AnimationRecord D_actor_560800_8015CF5C[94] = {
#include "assets/actor_560800_animation_2B2DC_records.inc"
};

u16 D_actor_560800_8015D0D4[20] = {
#include "assets/actor_560800_animation_2B2DC_indices.inc"
};

AnimationSet D_actor_560800_8015D0FC = {
    D_actor_560800_8015CF5C,
    D_actor_560800_8015D0D4,
    { NULL, D_actor_560800_8015CE68, NULL, NULL, D_actor_560800_8015CE80, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015D124[8] = {
#include "assets/actor_560800_animation_2B9B4_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015D184[127] = {
#include "assets/actor_560800_animation_2B9B4_bank4.inc"
};

AnimationRecord D_actor_560800_8015D380[267] = {
#include "assets/actor_560800_animation_2B9B4_records.inc"
};

u16 D_actor_560800_8015D7AC[20] = {
#include "assets/actor_560800_animation_2B9B4_indices.inc"
};

AnimationSet D_actor_560800_8015D7D4 = {
    D_actor_560800_8015D380,
    D_actor_560800_8015D7AC,
    { NULL, D_actor_560800_8015D124, NULL, NULL, D_actor_560800_8015D184, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015D7FC[13] = {
#include "assets/actor_560800_animation_2BD38_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015D898[50] = {
#include "assets/actor_560800_animation_2BD38_bank4.inc"
};

AnimationRecord D_actor_560800_8015D960[116] = {
#include "assets/actor_560800_animation_2BD38_records.inc"
};

u16 D_actor_560800_8015DB30[20] = {
#include "assets/actor_560800_animation_2BD38_indices.inc"
};

AnimationSet D_actor_560800_8015DB58 = {
    D_actor_560800_8015D960,
    D_actor_560800_8015DB30,
    { NULL, D_actor_560800_8015D7FC, NULL, NULL, D_actor_560800_8015D898, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015DB80[2] = {
#include "assets/actor_560800_animation_2BF38_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015DB98[29] = {
#include "assets/actor_560800_animation_2BF38_bank4.inc"
};

AnimationRecord D_actor_560800_8015DC0C[73] = {
#include "assets/actor_560800_animation_2BF38_records.inc"
};

u16 D_actor_560800_8015DD30[20] = {
#include "assets/actor_560800_animation_2BF38_indices.inc"
};

AnimationSet D_actor_560800_8015DD58 = {
    D_actor_560800_8015DC0C,
    D_actor_560800_8015DD30,
    { NULL, D_actor_560800_8015DB80, NULL, NULL, D_actor_560800_8015DB98, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015DD80[2] = {
#include "assets/actor_560800_animation_2C0F4_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015DD98[21] = {
#include "assets/actor_560800_animation_2C0F4_bank4.inc"
};

AnimationRecord D_actor_560800_8015DDEC[64] = {
#include "assets/actor_560800_animation_2C0F4_records.inc"
};

u16 D_actor_560800_8015DEEC[20] = {
#include "assets/actor_560800_animation_2C0F4_indices.inc"
};

AnimationSet D_actor_560800_8015DF14 = {
    D_actor_560800_8015DDEC,
    D_actor_560800_8015DEEC,
    { NULL, D_actor_560800_8015DD80, NULL, NULL, D_actor_560800_8015DD98, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015DF3C[19] = {
#include "assets/actor_560800_animation_2C728_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015E020[126] = {
#include "assets/actor_560800_animation_2C728_bank4.inc"
};

AnimationRecord D_actor_560800_8015E218[194] = {
#include "assets/actor_560800_animation_2C728_records.inc"
};

u16 D_actor_560800_8015E520[20] = {
#include "assets/actor_560800_animation_2C728_indices.inc"
};

AnimationSet D_actor_560800_8015E548 = {
    D_actor_560800_8015E218,
    D_actor_560800_8015E520,
    { NULL, D_actor_560800_8015DF3C, NULL, NULL, D_actor_560800_8015E020, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015E570[33] = {
#include "assets/actor_560800_animation_2CDEC_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015E6FC[124] = {
#include "assets/actor_560800_animation_2CDEC_bank4.inc"
};

AnimationRecord D_actor_560800_8015E8EC[190] = {
#include "assets/actor_560800_animation_2CDEC_records.inc"
};

u16 D_actor_560800_8015EBE4[20] = {
#include "assets/actor_560800_animation_2CDEC_indices.inc"
};

AnimationSet D_actor_560800_8015EC0C = {
    D_actor_560800_8015E8EC,
    D_actor_560800_8015EBE4,
    { NULL, D_actor_560800_8015E570, NULL, NULL, D_actor_560800_8015E6FC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015EC34[78] = {
#include "assets/actor_560800_animation_2DA6C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015EFDC[206] = {
#include "assets/actor_560800_animation_2DA6C_bank4.inc"
};

AnimationRecord D_actor_560800_8015F314[340] = {
#include "assets/actor_560800_animation_2DA6C_records.inc"
};

u16 D_actor_560800_8015F864[20] = {
#include "assets/actor_560800_animation_2DA6C_indices.inc"
};

AnimationSet D_actor_560800_8015F88C = {
    D_actor_560800_8015F314,
    D_actor_560800_8015F864,
    { NULL, D_actor_560800_8015EC34, NULL, NULL, D_actor_560800_8015EFDC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8015F8B4[41] = {
#include "assets/actor_560800_animation_2E370_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8015FAA0[163] = {
#include "assets/actor_560800_animation_2E370_bank4.inc"
};

AnimationRecord D_actor_560800_8015FD2C[271] = {
#include "assets/actor_560800_animation_2E370_records.inc"
};

u16 D_actor_560800_80160168[20] = {
#include "assets/actor_560800_animation_2E370_indices.inc"
};

AnimationSet D_actor_560800_80160190 = {
    D_actor_560800_8015FD2C,
    D_actor_560800_80160168,
    { NULL, D_actor_560800_8015F8B4, NULL, NULL, D_actor_560800_8015FAA0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801601B8[5] = {
#include "assets/actor_560800_animation_2EC58_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801601F4[165] = {
#include "assets/actor_560800_animation_2EC58_bank4.inc"
};

AnimationRecord D_actor_560800_80160488[370] = {
#include "assets/actor_560800_animation_2EC58_records.inc"
};

u16 D_actor_560800_80160A50[20] = {
#include "assets/actor_560800_animation_2EC58_indices.inc"
};

AnimationSet D_actor_560800_80160A78 = {
    D_actor_560800_80160488,
    D_actor_560800_80160A50,
    { NULL, D_actor_560800_801601B8, NULL, NULL, D_actor_560800_801601F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80160AA0[7] = {
#include "assets/actor_560800_animation_2F0FC_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80160AF4[86] = {
#include "assets/actor_560800_animation_2F0FC_bank4.inc"
};

AnimationRecord D_actor_560800_80160C4C[170] = {
#include "assets/actor_560800_animation_2F0FC_records.inc"
};

u16 D_actor_560800_80160EF4[20] = {
#include "assets/actor_560800_animation_2F0FC_indices.inc"
};

AnimationSet D_actor_560800_80160F1C = {
    D_actor_560800_80160C4C,
    D_actor_560800_80160EF4,
    { NULL, D_actor_560800_80160AA0, NULL, NULL, D_actor_560800_80160AF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80160F44[16] = {
#include "assets/actor_560800_animation_2F728_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80161004[137] = {
#include "assets/actor_560800_animation_2F728_bank4.inc"
};

AnimationRecord D_actor_560800_80161228[190] = {
#include "assets/actor_560800_animation_2F728_records.inc"
};

u16 D_actor_560800_80161520[20] = {
#include "assets/actor_560800_animation_2F728_indices.inc"
};

AnimationSet D_actor_560800_80161548 = {
    D_actor_560800_80161228,
    D_actor_560800_80161520,
    { NULL, D_actor_560800_80160F44, NULL, NULL, D_actor_560800_80161004, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80161570[44] = {
#include "assets/actor_560800_animation_2FF90_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80161780[149] = {
#include "assets/actor_560800_animation_2FF90_bank4.inc"
};

AnimationRecord D_actor_560800_801619D4[237] = {
#include "assets/actor_560800_animation_2FF90_records.inc"
};

u16 D_actor_560800_80161D88[20] = {
#include "assets/actor_560800_animation_2FF90_indices.inc"
};

AnimationSet D_actor_560800_80161DB0 = {
    D_actor_560800_801619D4,
    D_actor_560800_80161D88,
    { NULL, D_actor_560800_80161570, NULL, NULL, D_actor_560800_80161780, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80161DD8[8] = {
#include "assets/actor_560800_animation_30290_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80161E38[33] = {
#include "assets/actor_560800_animation_30290_bank4.inc"
};

AnimationRecord D_actor_560800_80161EBC[115] = {
#include "assets/actor_560800_animation_30290_records.inc"
};

u16 D_actor_560800_80162088[20] = {
#include "assets/actor_560800_animation_30290_indices.inc"
};

AnimationSet D_actor_560800_801620B0 = {
    D_actor_560800_80161EBC,
    D_actor_560800_80162088,
    { NULL, D_actor_560800_80161DD8, NULL, NULL, D_actor_560800_80161E38, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801620D8[41] = {
#include "assets/actor_560800_animation_30C2C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801622C4[193] = {
#include "assets/actor_560800_animation_30C2C_bank4.inc"
};

AnimationRecord D_actor_560800_801625C8[279] = {
#include "assets/actor_560800_animation_30C2C_records.inc"
};

u16 D_actor_560800_80162A24[20] = {
#include "assets/actor_560800_animation_30C2C_indices.inc"
};

AnimationSet D_actor_560800_80162A4C = {
    D_actor_560800_801625C8,
    D_actor_560800_80162A24,
    { NULL, D_actor_560800_801620D8, NULL, NULL, D_actor_560800_801622C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80162A74[2] = {
#include "assets/actor_560800_animation_31014_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80162A8C[62] = {
#include "assets/actor_560800_animation_31014_bank4.inc"
};

AnimationRecord D_actor_560800_80162B84[162] = {
#include "assets/actor_560800_animation_31014_records.inc"
};

u16 D_actor_560800_80162E0C[20] = {
#include "assets/actor_560800_animation_31014_indices.inc"
};

AnimationSet D_actor_560800_80162E34 = {
    D_actor_560800_80162B84,
    D_actor_560800_80162E0C,
    { NULL, D_actor_560800_80162A74, NULL, NULL, D_actor_560800_80162A8C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80162E5C[2] = {
#include "assets/actor_560800_animation_315E0_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80162E74[121] = {
#include "assets/actor_560800_animation_315E0_bank4.inc"
};

AnimationRecord D_actor_560800_80163058[224] = {
#include "assets/actor_560800_animation_315E0_records.inc"
};

u16 D_actor_560800_801633D8[20] = {
#include "assets/actor_560800_animation_315E0_indices.inc"
};

AnimationSet D_actor_560800_80163400 = {
    D_actor_560800_80163058,
    D_actor_560800_801633D8,
    { NULL, D_actor_560800_80162E5C, NULL, NULL, D_actor_560800_80162E74, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80163428[3] = {
#include "assets/actor_560800_animation_31A28_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016344C[100] = {
#include "assets/actor_560800_animation_31A28_bank4.inc"
};

AnimationRecord D_actor_560800_801635DC[145] = {
#include "assets/actor_560800_animation_31A28_records.inc"
};

u16 D_actor_560800_80163820[20] = {
#include "assets/actor_560800_animation_31A28_indices.inc"
};

AnimationSet D_actor_560800_80163848 = {
    D_actor_560800_801635DC,
    D_actor_560800_80163820,
    { NULL, D_actor_560800_80163428, NULL, NULL, D_actor_560800_8016344C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80163870[17] = {
#include "assets/actor_560800_animation_320A0_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016393C[145] = {
#include "assets/actor_560800_animation_320A0_bank4.inc"
};

AnimationRecord D_actor_560800_80163B80[198] = {
#include "assets/actor_560800_animation_320A0_records.inc"
};

u16 D_actor_560800_80163E98[20] = {
#include "assets/actor_560800_animation_320A0_indices.inc"
};

AnimationSet D_actor_560800_80163EC0 = {
    D_actor_560800_80163B80,
    D_actor_560800_80163E98,
    { NULL, D_actor_560800_80163870, NULL, NULL, D_actor_560800_8016393C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80163EE8[2] = {
#include "assets/actor_560800_animation_3254C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80163F00[79] = {
#include "assets/actor_560800_animation_3254C_bank4.inc"
};

AnimationRecord D_actor_560800_8016403C[194] = {
#include "assets/actor_560800_animation_3254C_records.inc"
};

u16 D_actor_560800_80164344[20] = {
#include "assets/actor_560800_animation_3254C_indices.inc"
};

AnimationSet D_actor_560800_8016436C = {
    D_actor_560800_8016403C,
    D_actor_560800_80164344,
    { NULL, D_actor_560800_80163EE8, NULL, NULL, D_actor_560800_80163F00, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80164394[2] = {
#include "assets/actor_560800_animation_32C24_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801643AC[177] = {
#include "assets/actor_560800_animation_32C24_bank4.inc"
};

AnimationRecord D_actor_560800_80164670[235] = {
#include "assets/actor_560800_animation_32C24_records.inc"
};

u16 D_actor_560800_80164A1C[20] = {
#include "assets/actor_560800_animation_32C24_indices.inc"
};

AnimationSet D_actor_560800_80164A44 = {
    D_actor_560800_80164670,
    D_actor_560800_80164A1C,
    { NULL, D_actor_560800_80164394, NULL, NULL, D_actor_560800_801643AC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80164A6C[21] = {
#include "assets/actor_560800_animation_33998_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80164B68[342] = {
#include "assets/actor_560800_animation_33998_bank4.inc"
};

AnimationRecord D_actor_560800_801650C0[436] = {
#include "assets/actor_560800_animation_33998_records.inc"
};

u16 D_actor_560800_80165790[20] = {
#include "assets/actor_560800_animation_33998_indices.inc"
};

AnimationSet D_actor_560800_801657B8 = {
    D_actor_560800_801650C0,
    D_actor_560800_80165790,
    { NULL, D_actor_560800_80164A6C, NULL, NULL, D_actor_560800_80164B68, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801657E0[17] = {
#include "assets/actor_560800_animation_33E18_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801658AC[87] = {
#include "assets/actor_560800_animation_33E18_bank4.inc"
};

AnimationRecord D_actor_560800_80165A08[130] = {
#include "assets/actor_560800_animation_33E18_records.inc"
};

u16 D_actor_560800_80165C10[20] = {
#include "assets/actor_560800_animation_33E18_indices.inc"
};

AnimationSet D_actor_560800_80165C38 = {
    D_actor_560800_80165A08,
    D_actor_560800_80165C10,
    { NULL, D_actor_560800_801657E0, NULL, NULL, D_actor_560800_801658AC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80165C60[2] = {
#include "assets/actor_560800_animation_3404C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80165C78[36] = {
#include "assets/actor_560800_animation_3404C_bank4.inc"
};

AnimationRecord D_actor_560800_80165D08[79] = {
#include "assets/actor_560800_animation_3404C_records.inc"
};

u16 D_actor_560800_80165E44[20] = {
#include "assets/actor_560800_animation_3404C_indices.inc"
};

AnimationSet D_actor_560800_80165E6C = {
    D_actor_560800_80165D08,
    D_actor_560800_80165E44,
    { NULL, D_actor_560800_80165C60, NULL, NULL, D_actor_560800_80165C78, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80165E94[67] = {
#include "assets/actor_560800_animation_34D08_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801661B8[227] = {
#include "assets/actor_560800_animation_34D08_bank4.inc"
};

AnimationRecord D_actor_560800_80166544[367] = {
#include "assets/actor_560800_animation_34D08_records.inc"
};

u16 D_actor_560800_80166B00[20] = {
#include "assets/actor_560800_animation_34D08_indices.inc"
};

AnimationSet D_actor_560800_80166B28 = {
    D_actor_560800_80166544,
    D_actor_560800_80166B00,
    { NULL, D_actor_560800_80165E94, NULL, NULL, D_actor_560800_801661B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80166B50[25] = {
#include "assets/actor_560800_animation_3552C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80166C7C[180] = {
#include "assets/actor_560800_animation_3552C_bank4.inc"
};

AnimationRecord D_actor_560800_80166F4C[246] = {
#include "assets/actor_560800_animation_3552C_records.inc"
};

u16 D_actor_560800_80167324[20] = {
#include "assets/actor_560800_animation_3552C_indices.inc"
};

AnimationSet D_actor_560800_8016734C = {
    D_actor_560800_80166F4C,
    D_actor_560800_80167324,
    { NULL, D_actor_560800_80166B50, NULL, NULL, D_actor_560800_80166C7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80167374[18] = {
#include "assets/actor_560800_animation_35928_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016744C[69] = {
#include "assets/actor_560800_animation_35928_bank4.inc"
};

AnimationRecord D_actor_560800_80167560[112] = {
#include "assets/actor_560800_animation_35928_records.inc"
};

u16 D_actor_560800_80167720[20] = {
#include "assets/actor_560800_animation_35928_indices.inc"
};

AnimationSet D_actor_560800_80167748 = {
    D_actor_560800_80167560,
    D_actor_560800_80167720,
    { NULL, D_actor_560800_80167374, NULL, NULL, D_actor_560800_8016744C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80167770[15] = {
#include "assets/actor_560800_animation_35C50_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80167824[24] = {
#include "assets/actor_560800_animation_35C50_bank4.inc"
};

AnimationRecord D_actor_560800_80167884[113] = {
#include "assets/actor_560800_animation_35C50_records.inc"
};

u16 D_actor_560800_80167A48[20] = {
#include "assets/actor_560800_animation_35C50_indices.inc"
};

AnimationSet D_actor_560800_80167A70 = {
    D_actor_560800_80167884,
    D_actor_560800_80167A48,
    { NULL, D_actor_560800_80167770, NULL, NULL, D_actor_560800_80167824, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80167A98[26] = {
#include "assets/actor_560800_animation_361B8_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80167BD0[95] = {
#include "assets/actor_560800_animation_361B8_bank4.inc"
};

AnimationRecord D_actor_560800_80167D4C[153] = {
#include "assets/actor_560800_animation_361B8_records.inc"
};

u16 D_actor_560800_80167FB0[20] = {
#include "assets/actor_560800_animation_361B8_indices.inc"
};

AnimationSet D_actor_560800_80167FD8 = {
    D_actor_560800_80167D4C,
    D_actor_560800_80167FB0,
    { NULL, D_actor_560800_80167A98, NULL, NULL, D_actor_560800_80167BD0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80168000[22] = {
#include "assets/actor_560800_animation_367B4_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80168108[124] = {
#include "assets/actor_560800_animation_367B4_bank4.inc"
};

AnimationRecord D_actor_560800_801682F8[173] = {
#include "assets/actor_560800_animation_367B4_records.inc"
};

u16 D_actor_560800_801685AC[20] = {
#include "assets/actor_560800_animation_367B4_indices.inc"
};

AnimationSet D_actor_560800_801685D4 = {
    D_actor_560800_801682F8,
    D_actor_560800_801685AC,
    { NULL, D_actor_560800_80168000, NULL, NULL, D_actor_560800_80168108, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801685FC[46] = {
#include "assets/actor_560800_animation_371E8_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80168824[175] = {
#include "assets/actor_560800_animation_371E8_bank4.inc"
};

AnimationRecord D_actor_560800_80168AE0[320] = {
#include "assets/actor_560800_animation_371E8_records.inc"
};

u16 D_actor_560800_80168FE0[20] = {
#include "assets/actor_560800_animation_371E8_indices.inc"
};

AnimationSet D_actor_560800_80169008 = {
    D_actor_560800_80168AE0,
    D_actor_560800_80168FE0,
    { NULL, D_actor_560800_801685FC, NULL, NULL, D_actor_560800_80168824, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80169030[46] = {
#include "assets/actor_560800_animation_37F5C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80169258[259] = {
#include "assets/actor_560800_animation_37F5C_bank4.inc"
};

AnimationRecord D_actor_560800_80169664[444] = {
#include "assets/actor_560800_animation_37F5C_records.inc"
};

u16 D_actor_560800_80169D54[20] = {
#include "assets/actor_560800_animation_37F5C_indices.inc"
};

AnimationSet D_actor_560800_80169D7C = {
    D_actor_560800_80169664,
    D_actor_560800_80169D54,
    { NULL, D_actor_560800_80169030, NULL, NULL, D_actor_560800_80169258, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80169DA4[2] = {
#include "assets/actor_560800_animation_38288_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80169DBC[47] = {
#include "assets/actor_560800_animation_38288_bank4.inc"
};

AnimationRecord D_actor_560800_80169E78[130] = {
#include "assets/actor_560800_animation_38288_records.inc"
};

u16 D_actor_560800_8016A080[20] = {
#include "assets/actor_560800_animation_38288_indices.inc"
};

AnimationSet D_actor_560800_8016A0A8 = {
    D_actor_560800_80169E78,
    D_actor_560800_8016A080,
    { NULL, D_actor_560800_80169DA4, NULL, NULL, D_actor_560800_80169DBC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8016A0D0[35] = {
#include "assets/actor_560800_animation_389F0_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016A274[141] = {
#include "assets/actor_560800_animation_389F0_bank4.inc"
};

AnimationRecord D_actor_560800_8016A4A8[208] = {
#include "assets/actor_560800_animation_389F0_records.inc"
};

u16 D_actor_560800_8016A7E8[20] = {
#include "assets/actor_560800_animation_389F0_indices.inc"
};

AnimationSet D_actor_560800_8016A810 = {
    D_actor_560800_8016A4A8,
    D_actor_560800_8016A7E8,
    { NULL, D_actor_560800_8016A0D0, NULL, NULL, D_actor_560800_8016A274, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8016A838[17] = {
#include "assets/actor_560800_animation_38E5C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016A904[83] = {
#include "assets/actor_560800_animation_38E5C_bank4.inc"
};

AnimationRecord D_actor_560800_8016AA50[129] = {
#include "assets/actor_560800_animation_38E5C_records.inc"
};

u16 D_actor_560800_8016AC54[20] = {
#include "assets/actor_560800_animation_38E5C_indices.inc"
};

AnimationSet D_actor_560800_8016AC7C = {
    D_actor_560800_8016AA50,
    D_actor_560800_8016AC54,
    { NULL, D_actor_560800_8016A838, NULL, NULL, D_actor_560800_8016A904, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8016ACA4[2] = {
#include "assets/actor_560800_animation_39368_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016ACBC[94] = {
#include "assets/actor_560800_animation_39368_bank4.inc"
};

AnimationRecord D_actor_560800_8016AE34[203] = {
#include "assets/actor_560800_animation_39368_records.inc"
};

u16 D_actor_560800_8016B160[20] = {
#include "assets/actor_560800_animation_39368_indices.inc"
};

AnimationSet D_actor_560800_8016B188 = {
    D_actor_560800_8016AE34,
    D_actor_560800_8016B160,
    { NULL, D_actor_560800_8016ACA4, NULL, NULL, D_actor_560800_8016ACBC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8016B1B0[92] = {
#include "assets/actor_560800_animation_39F6C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016B600[173] = {
#include "assets/actor_560800_animation_39F6C_bank4.inc"
};

AnimationRecord D_actor_560800_8016B8B4[300] = {
#include "assets/actor_560800_animation_39F6C_records.inc"
};

u16 D_actor_560800_8016BD64[20] = {
#include "assets/actor_560800_animation_39F6C_indices.inc"
};

AnimationSet D_actor_560800_8016BD8C = {
    D_actor_560800_8016B8B4,
    D_actor_560800_8016BD64,
    { NULL, D_actor_560800_8016B1B0, NULL, NULL, D_actor_560800_8016B600, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8016BDB4[8] = {
#include "assets/actor_560800_animation_3A4EC_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016BE14[89] = {
#include "assets/actor_560800_animation_3A4EC_bank4.inc"
};

AnimationRecord D_actor_560800_8016BF78[219] = {
#include "assets/actor_560800_animation_3A4EC_records.inc"
};

u16 D_actor_560800_8016C2E4[20] = {
#include "assets/actor_560800_animation_3A4EC_indices.inc"
};

AnimationSet D_actor_560800_8016C30C = {
    D_actor_560800_8016BF78,
    D_actor_560800_8016C2E4,
    { NULL, D_actor_560800_8016BDB4, NULL, NULL, D_actor_560800_8016BE14, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8016C334[9] = {
#include "assets/actor_560800_animation_3A804_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016C3A0[55] = {
#include "assets/actor_560800_animation_3A804_bank4.inc"
};

AnimationRecord D_actor_560800_8016C47C[96] = {
#include "assets/actor_560800_animation_3A804_records.inc"
};

u16 D_actor_560800_8016C5FC[20] = {
#include "assets/actor_560800_animation_3A804_indices.inc"
};

AnimationSet D_actor_560800_8016C624 = {
    D_actor_560800_8016C47C,
    D_actor_560800_8016C5FC,
    { NULL, D_actor_560800_8016C334, NULL, NULL, D_actor_560800_8016C3A0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8016C64C[43] = {
#include "assets/actor_560800_animation_3B170_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016C850[181] = {
#include "assets/actor_560800_animation_3B170_bank4.inc"
};

AnimationRecord D_actor_560800_8016CB24[273] = {
#include "assets/actor_560800_animation_3B170_records.inc"
};

u16 D_actor_560800_8016CF68[20] = {
#include "assets/actor_560800_animation_3B170_indices.inc"
};

AnimationSet D_actor_560800_8016CF90 = {
    D_actor_560800_8016CB24,
    D_actor_560800_8016CF68,
    { NULL, D_actor_560800_8016C64C, NULL, NULL, D_actor_560800_8016C850, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8016CFB8[13] = {
#include "assets/actor_560800_animation_3B78C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016D054[120] = {
#include "assets/actor_560800_animation_3B78C_bank4.inc"
};

AnimationRecord D_actor_560800_8016D234[212] = {
#include "assets/actor_560800_animation_3B78C_records.inc"
};

u16 D_actor_560800_8016D584[20] = {
#include "assets/actor_560800_animation_3B78C_indices.inc"
};

AnimationSet D_actor_560800_8016D5AC = {
    D_actor_560800_8016D234,
    D_actor_560800_8016D584,
    { NULL, D_actor_560800_8016CFB8, NULL, NULL, D_actor_560800_8016D054, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8016D5D4[38] = {
#include "assets/actor_560800_animation_3BF2C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016D79C[143] = {
#include "assets/actor_560800_animation_3BF2C_bank4.inc"
};

AnimationRecord D_actor_560800_8016D9D8[211] = {
#include "assets/actor_560800_animation_3BF2C_records.inc"
};

u16 D_actor_560800_8016DD24[20] = {
#include "assets/actor_560800_animation_3BF2C_indices.inc"
};

AnimationSet D_actor_560800_8016DD4C = {
    D_actor_560800_8016D9D8,
    D_actor_560800_8016DD24,
    { NULL, D_actor_560800_8016D5D4, NULL, NULL, D_actor_560800_8016D79C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8016DD74[2] = {
#include "assets/actor_560800_animation_3C070_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016DD8C[17] = {
#include "assets/actor_560800_animation_3C070_bank4.inc"
};

AnimationRecord D_actor_560800_8016DDD0[38] = {
#include "assets/actor_560800_animation_3C070_records.inc"
};

u16 D_actor_560800_8016DE68[20] = {
#include "assets/actor_560800_animation_3C070_indices.inc"
};

AnimationSet D_actor_560800_8016DE90 = {
    D_actor_560800_8016DDD0,
    D_actor_560800_8016DE68,
    { NULL, D_actor_560800_8016DD74, NULL, NULL, D_actor_560800_8016DD8C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8016DEB8[13] = {
#include "assets/actor_560800_animation_3CBE0_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8016DF54[259] = {
#include "assets/actor_560800_animation_3CBE0_bank4.inc"
};

AnimationRecord D_actor_560800_8016E360[414] = {
#include "assets/actor_560800_animation_3CBE0_records.inc"
};

u16 D_actor_560800_8016E9D8[20] = {
#include "assets/actor_560800_animation_3CBE0_indices.inc"
};

AnimationSet D_actor_560800_8016EA00 = {
    D_actor_560800_8016E360,
    D_actor_560800_8016E9D8,
    { NULL, D_actor_560800_8016DEB8, NULL, NULL, D_actor_560800_8016DF54, NULL, NULL, NULL },
};

TaskDesc D_actor_560800_8016EA28[2] = {
    { 0, 192, func_actor_560800_80135F50, { .model = NULL } },
    { 0, 192, func_actor_560800_801321A0, { .model = NULL } },
};

AnimationSet* D_actor_560800_8016EA40[13] = {
    &D_actor_560800_80159850,
    &D_actor_560800_80159CF8,
    &D_actor_560800_8015A6F4,
    &D_actor_560800_8015B0F0,
    &D_actor_560800_8015B400,
    &D_actor_560800_8015B6B4,
    NULL,
    NULL,
    NULL,
    NULL,
    &D_actor_560800_8015C0B4,
    &D_actor_560800_8015C32C,
    &D_actor_560800_8015B9B8,
};

AnimationSet* D_actor_560800_8016EA74[36] = {
    &D_actor_560800_80150C20,
    &D_actor_560800_80150F18,
    &D_actor_560800_80151104,
    &D_actor_560800_801514D8,
    &D_actor_560800_80152630,
    &D_actor_560800_80152B7C,
    &D_actor_560800_80152F14,
    &D_actor_560800_8015317C,
    &D_actor_560800_80153600,
    &D_actor_560800_80153A84,
    &D_actor_560800_80153CCC,
    &D_actor_560800_80154000,
    &D_actor_560800_801541E4,
    &D_actor_560800_801545F4,
    &D_actor_560800_80154974,
    &D_actor_560800_80154C7C,
    &D_actor_560800_80155088,
    &D_actor_560800_8015543C,
    &D_actor_560800_8015589C,
    &D_actor_560800_80155BF0,
    &D_actor_560800_80155E28,
    &D_actor_560800_80156598,
    &D_actor_560800_8015688C,
    NULL,
    NULL,
    &D_actor_560800_8015729C,
    &D_actor_560800_80156B08,
    &D_actor_560800_80158CF0,
    &D_actor_560800_80158EAC,
    &D_actor_560800_80157880,
    &D_actor_560800_801588D4,
    &D_actor_560800_801590A0,
    &D_actor_560800_80159318,
    &D_actor_560800_801586FC,
    &D_actor_560800_80157D38,
    &D_actor_560800_80158080,
};

AnimationSet* D_actor_560800_8016EB04[11] = {
    &D_actor_560800_8015C5F0,
    &D_actor_560800_8015C8DC,
    &D_actor_560800_8015CA88,
    &D_actor_560800_8015CC60,
    &D_actor_560800_8015CE40,
    &D_actor_560800_8015D0FC,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

AnimationSet* D_actor_560800_8016EB30[46] = {
    &D_actor_560800_8015D7D4,
    &D_actor_560800_8015DB58,
    &D_actor_560800_8015DD58,
    &D_actor_560800_8015DF14,
    &D_actor_560800_8015E548,
    &D_actor_560800_8015EC0C,
    &D_actor_560800_8015F88C,
    &D_actor_560800_80160190,
    &D_actor_560800_80160A78,
    &D_actor_560800_80160F1C,
    &D_actor_560800_80161548,
    &D_actor_560800_80161DB0,
    &D_actor_560800_801620B0,
    &D_actor_560800_80162A4C,
    &D_actor_560800_80162E34,
    &D_actor_560800_80163400,
    &D_actor_560800_80163848,
    &D_actor_560800_80163EC0,
    &D_actor_560800_8016436C,
    &D_actor_560800_80164A44,
    &D_actor_560800_801657B8,
    &D_actor_560800_80165C38,
    &D_actor_560800_80165E6C,
    &D_actor_560800_80166B28,
    &D_actor_560800_8016734C,
    &D_actor_560800_80167748,
    &D_actor_560800_80167A70,
    &D_actor_560800_80167FD8,
    &D_actor_560800_801685D4,
    &D_actor_560800_80169008,
    &D_actor_560800_80169D7C,
    &D_actor_560800_8016A0A8,
    &D_actor_560800_8016A810,
    &D_actor_560800_8016AC7C,
    &D_actor_560800_8016B188,
    &D_actor_560800_8016BD8C,
    &D_actor_560800_8016C30C,
    &D_actor_560800_8016C624,
    &D_actor_560800_8016CF90,
    &D_actor_560800_8016D5AC,
    &D_actor_560800_8016DD4C,
    &D_actor_560800_8016DE90,
    NULL,
    NULL,
    NULL,
    &D_actor_560800_8016EA00,
};

ActorAnimStep D_actor_560800_8016EBE8[13] = {
    { 0, -1 },
    { 0, 10 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 90, -1 },
    { 0, -1 },
    { 65535, -1 },
};

ActorAnimStep D_actor_560800_8016EC1C[36] = {
    { 65535, -1 },
    { 0, 2 },
    { 0, -1 },
    { 0, 4 },
    { 0, -1 },
    { 100, 6 },
    { 0, 26 },
    { 0, -1 },
    { 65535, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, 13 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 40, 28 },
    { 0, 26 },
    { 0, 26 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, 26 },
    { 60, 35 },
    { 0, 26 },
};

ActorAnimStep D_actor_560800_8016ECAC[6] = {
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
};

ActorAnimStep D_actor_560800_8016ECC4[46] = {
    { 65535, -1 },
    { 65535, -1 },
    { 0, 3 },
    { 65535, -1 },
    { 65535, -1 },
    { 0, 6 },
    { 65535, -1 },
    { 0, 8 },
    { 65535, -1 },
    { 65535, -1 },
    { 65535, -1 },
    { 0, 12 },
    { 65535, -1 },
    { 0, 14 },
    { 65535, -1 },
    { 0, 14 },
    { 0, -1 },
    { 0, 18 },
    { 65535, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, 22 },
    { 0, 18 },
    { 0, -1 },
    { 0, 15 },
    { 0, 26 },
    { 65535, -1 },
    { 0, 14 },
    { 0, 15 },
    { 0, 30 },
    { 0, 31 },
    { 65535, -1 },
    { 0, -1 },
    { 0, 34 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 65535, -1 },
    { 65535, -1 },
    { 65535, -1 },
    { 0, -1 },
    { 0, -1 },
    { 65535, -1 },
};

ActorTransform D_actor_560800_8016ED7C = { 0 };

ActorTransform D_actor_560800_8016ED94 = { 0 };

ActorTransform D_actor_560800_8016EDAC = { { 6400, 0, 3350, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016EDC4 = { 0 };

ActorTransform D_actor_560800_8016EDDC = { { 5400, 0, 3350, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016EDF4 = { { 6000, 0, 3200, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016EE0C = { 0 };

ActorTransform D_actor_560800_8016EE24 = { { 5850, 0, 3200, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016EE3C = { 0 };

ActorTransform D_actor_560800_8016EE54 = { 0 };

ActorTransform D_actor_560800_8016EE6C = { 0 };

ActorTransform D_actor_560800_8016EE84 = { 0 };

ActorTransform D_actor_560800_8016EE9C[2] = { { { 5850, 0, 3200, 0 }, { 0, 1024, 0, 0 } }, { { 5850, 0, 3200, 0 }, { 0, 1024, 0, 0 } } };

ActorTransform D_actor_560800_8016EECC = { { 5850, 0, 3200, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016EEE4 = { { 5000, 0, 3200, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016EEFC = { { 7500, 0, 3000, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016EF14 = { { 7850, 0, 3400, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016EF2C = { 0 };

ActorTransform D_actor_560800_8016EF44 = { { 7500, 0, 3400, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016EF5C = { 0 };

ActorTransform D_actor_560800_8016EF74 = { 0 };

ActorTransform D_actor_560800_8016EF8C = { { 9150, 0, 3750, 0 }, { 0, -170, 0, 0 } };

ActorTransform D_actor_560800_8016EFA4 = { 0 };

ActorTransform D_actor_560800_8016EFBC = { { 8050, 0, 3700, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016EFD4 = { 0 };

ActorTransform D_actor_560800_8016EFEC = { { 6950, 0, 3650, 0 }, { 0, 1536, 0, 0 } };

ActorTransform D_actor_560800_8016F004 = { { 6950, 0, 3650, 0 }, { 0, 1365, 0, 0 } };

ActorTransform D_actor_560800_8016F01C = { 0 };

ActorTransform D_actor_560800_8016F034 = { 0 };

ActorTransform D_actor_560800_8016F04C = { 0 };

ActorTransform D_actor_560800_8016F064 = { { 8350, 0, 4000, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016F07C = { { 8350, 0, 4000, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016F094 = { 0 };

ActorTransform D_actor_560800_8016F0AC = { { 8350, 0, 4000, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016F0C4 = { 0 };

ActorTransform D_actor_560800_8016F0DC = { 0 };

ActorTransform D_actor_560800_8016F0F4 = { 0 };

ActorTransform D_actor_560800_8016F10C = { 0 };

ActorTransform D_actor_560800_8016F124 = { { 8350, 0, 4300, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016F13C = { { 8340, 0, 4000, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016F154 = { { 8130, 0, 2850, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016F16C = { 0 };

ActorTransform D_actor_560800_8016F184 = { { 8700, 0, 3200, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_560800_8016F19C = { 0 };

ActorTransform D_actor_560800_8016F1B4 = { { 8600, 0, 3500, 0 }, { 0, -512, 0, 0 } };

ActorTransform D_actor_560800_8016F1CC[6] = {
    { { 9400, 0, 2600, 0 }, { 0, -512, 0, 0 } },
    { { 9400, 0, 2600, 0 }, { 0, -1024, 0, 0 } },
    { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
    { { 9400, 0, 2600, 0 }, { 0, -1024, 0, 0 } },
    { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
    { { 7800, 0, 3200, 0 }, { 0, -1024, 0, 0 } },
};

ActorTransform D_actor_560800_8016F25C = { { 7800, 0, 3200, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_560800_8016F274 = { { 7800, 0, 3200, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_560800_8016F28C = { { 6350, 0, 3200, 0 }, { 0, -967, 0, 0 } };

ActorTransform D_actor_560800_8016F2A4 = { { 6350, 0, 3200, 0 }, { 0, 796, 0, 0 } };

ActorTransform D_actor_560800_8016F2BC = { { 8450, 0, 2850, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_560800_8016F2D4 = { 0 };

ActorTransform D_actor_560800_8016F2EC = { { 8450, 0, 2850, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_560800_8016F304 = { 0 };

ActorTransform D_actor_560800_8016F31C = { { 8450, 0, 2850, 0 }, { 0, -512, 0, 0 } };

ActorTransform D_actor_560800_8016F334 = { { 10800, 0, 3200, 0 }, { 0, -1024, 0, 0 } };

Actor560800MessageEntry D_actor_560800_8016F34C[2] = {
    { 2005, { .call3 = func_actor_560800_801361A0 } },
    { 2004, { .call1 = actorMsgPlaceYawPitchRoll } },
};

ActorTransform* D_actor_560800_8016F35C[34] = {
    NULL,
    &D_actor_560800_8016ED7C,
    &D_actor_560800_8016ED94,
    &D_actor_560800_8016EDAC,
    &D_actor_560800_8016ED7C,
    &D_actor_560800_8016EDAC,
    &D_actor_560800_8016EDC4,
    &D_actor_560800_8016EDDC,
    &D_actor_560800_8016EDC4,
    &D_actor_560800_8016EDAC,
    &D_actor_560800_8016ED7C,
    &D_actor_560800_8016EDC4,
    &D_actor_560800_8016EDAC,
    &D_actor_560800_8016ED7C,
    &D_actor_560800_8016EDF4,
    &D_actor_560800_8016EE0C,
    &D_actor_560800_8016EDF4,
    &D_actor_560800_8016EE0C,
    &D_actor_560800_8016EDF4,
    &D_actor_560800_8016EDDC,
    &D_actor_560800_8016EDF4,
    &D_actor_560800_8016EE24,
    &D_actor_560800_8016EE3C,
    &D_actor_560800_8016ED94,
    &D_actor_560800_8016EE3C,
    &D_actor_560800_8016EE54,
    &D_actor_560800_8016EE6C,
    &D_actor_560800_8016EE84,
    D_actor_560800_8016EE9C,
    &D_actor_560800_8016EEE4,
    D_actor_560800_8016EE9C,
    &D_actor_560800_8016EEE4,
    D_actor_560800_8016EE9C,
    &D_actor_560800_8016EECC,
};

ActorTransform* D_actor_560800_8016F3E4[34] = {
    NULL,
    &D_actor_560800_8016EEFC,
    &D_actor_560800_8016EF14,
    &D_actor_560800_8016EF2C,
    &D_actor_560800_8016EEFC,
    &D_actor_560800_8016EF2C,
    &D_actor_560800_8016EF44,
    &D_actor_560800_8016EF5C,
    &D_actor_560800_8016EF44,
    &D_actor_560800_8016EF2C,
    &D_actor_560800_8016EEFC,
    &D_actor_560800_8016EF44,
    &D_actor_560800_8016EF2C,
    &D_actor_560800_8016EEFC,
    &D_actor_560800_8016EF74,
    &D_actor_560800_8016EF8C,
    &D_actor_560800_8016EF74,
    &D_actor_560800_8016EF8C,
    &D_actor_560800_8016EF74,
    &D_actor_560800_8016EF5C,
    &D_actor_560800_8016EF74,
    &D_actor_560800_8016EFA4,
    &D_actor_560800_8016EFBC,
    &D_actor_560800_8016EF14,
    &D_actor_560800_8016EFBC,
    &D_actor_560800_8016EFD4,
    &D_actor_560800_8016EFEC,
    &D_actor_560800_8016F004,
    &D_actor_560800_8016F01C,
    &D_actor_560800_8016F034,
    &D_actor_560800_8016F01C,
    &D_actor_560800_8016F034,
    &D_actor_560800_8016F01C,
    &D_actor_560800_8016F04C,
};

ActorTransform* D_actor_560800_8016F46C[34] = {
    NULL,
    D_actor_560800_8016F1CC,
    &D_actor_560800_8016F1CC[1],
    &D_actor_560800_8016F1CC[2],
    D_actor_560800_8016F1CC,
    &D_actor_560800_8016F1CC[2],
    &D_actor_560800_8016F1CC[3],
    &D_actor_560800_8016F1CC[4],
    &D_actor_560800_8016F1CC[3],
    &D_actor_560800_8016F1CC[2],
    D_actor_560800_8016F1CC,
    &D_actor_560800_8016F1CC[3],
    &D_actor_560800_8016F1CC[2],
    D_actor_560800_8016F1CC,
    &D_actor_560800_8016F334,
    &D_actor_560800_8016F25C,
    &D_actor_560800_8016F1CC[5],
    &D_actor_560800_8016F25C,
    &D_actor_560800_8016F1CC[5],
    &D_actor_560800_8016F1CC[4],
    &D_actor_560800_8016F1CC[5],
    &D_actor_560800_8016F274,
    &D_actor_560800_8016F28C,
    &D_actor_560800_8016F1CC[1],
    &D_actor_560800_8016F28C,
    &D_actor_560800_8016F2A4,
    &D_actor_560800_8016F2BC,
    &D_actor_560800_8016F2D4,
    &D_actor_560800_8016F2EC,
    &D_actor_560800_8016F304,
    &D_actor_560800_8016F2EC,
    &D_actor_560800_8016F304,
    &D_actor_560800_8016F2EC,
    &D_actor_560800_8016F31C,
};

ActorTransform* D_actor_560800_8016F4F4[34] = {
    NULL,
    &D_actor_560800_8016F064,
    &D_actor_560800_8016F07C,
    &D_actor_560800_8016F094,
    &D_actor_560800_8016F064,
    &D_actor_560800_8016F094,
    &D_actor_560800_8016F0AC,
    &D_actor_560800_8016F0C4,
    &D_actor_560800_8016F0AC,
    &D_actor_560800_8016F094,
    &D_actor_560800_8016F064,
    &D_actor_560800_8016F0AC,
    &D_actor_560800_8016F094,
    &D_actor_560800_8016F064,
    &D_actor_560800_8016F0DC,
    &D_actor_560800_8016F0F4,
    &D_actor_560800_8016F0DC,
    &D_actor_560800_8016F0F4,
    &D_actor_560800_8016F0DC,
    &D_actor_560800_8016F0C4,
    &D_actor_560800_8016F0DC,
    &D_actor_560800_8016F10C,
    &D_actor_560800_8016F124,
    &D_actor_560800_8016F07C,
    &D_actor_560800_8016F124,
    &D_actor_560800_8016F13C,
    &D_actor_560800_8016F154,
    &D_actor_560800_8016F16C,
    &D_actor_560800_8016F184,
    &D_actor_560800_8016F19C,
    &D_actor_560800_8016F184,
    &D_actor_560800_8016F19C,
    &D_actor_560800_8016F184,
    &D_actor_560800_8016F1B4,
};

s32 D_actor_560800_8016F57C[19] = {
    0,
    0x404D0001,
    0x404D0002,
    0x404D0003,
    0x404D0004,
    0x404D0005,
    0x404D0006,
    0x404D0007,
    0x404D0008,
    0x404D0009,
    0x404D000A,
    0x404D000B,
    0x404D000C,
    0x404D000D,
    0x404D000E,
    0x404D000F,
    0x404D0010,
    0x404D0011,
    0x404D0012,
};

EvsSceneKey D_actor_560800_8016F5C8 = { 6, 8, 11 };

EvsSceneKey D_actor_560800_8016F5D0 = { 6, 8, 21 };

EvsSceneKey D_actor_560800_8016F5D8 = { 6, 8, 31 };

GpEvsCmd D_actor_560800_8016F5E0[364] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_actor_560800_8016F5C8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80136910 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136930 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136280 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136678 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801365B0 }, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801365B0 }, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_801362B0 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80135AEC }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_801369A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80136A20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_actor_560800_8016F5D0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80136910 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136280 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80136548 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136930 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_80136358 }, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_80136378 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801365B0 }, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_80136378 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_80136378 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = SetDispMask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80135AEC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_801369A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80136A54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80136818 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136280 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_actor_560800_8016F5D8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80136910 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136930 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801365B0 }, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_8013631C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_8013631C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80134B14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80134B14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801363F8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_8013631C }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80134B14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80134B14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_8013631C }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_801362B0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367C0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_8013631C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU32 = func_actor_560800_80133648 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136280 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_8013631C }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_8013631C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_8013631C }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_801362B0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU32 = func_actor_560800_80133648 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80136280 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801365B0 }, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801363F8 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801363F8 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80133750 }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_801362B0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_560800_80135AEC }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_560800_80171800[10] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80136878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_560800_80136548 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 5, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_actor_560800_801718F0[14] = {
    { 0, 192, func_actor_560800_80135D54, { .model = NULL } },
    { 0, 192, func_actor_560800_80136A88, { .model = NULL } },
    { 0, 192, func_actor_560800_80136094, { .model = NULL } },
    { 0, 192, func_actor_560800_80135FA0, { .model = NULL } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_560800_801326C4, { .model = &D_actor_560800_8013E230 } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_560800_80132C60, { .model = &D_actor_560800_8014854C } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_560800_80132F64, { .model = &D_actor_560800_8014EDFC } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_560800_80132A14, { .model = &D_actor_560800_8014F250 } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_560800_80132A14, { .model = &D_actor_560800_8014F6A4 } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_560800_80132A14, { .model = &D_actor_560800_801502EC } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_560800_80132A14, { .model = &D_actor_560800_8014FEA8 } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_560800_801326C4, { .model = &D_actor_560800_80143A08 } },
    { 0, 192, func_actor_560800_801366B0, { .model = NULL } },
    { 0, 192, func_actor_560800_801369E0, { .model = NULL } },
};

TmdBone D_actor_560800_80171998[7] = {
#include "assets/actor_560800_model_40968_skeleton.inc"
};

u32 D_actor_560800_80171A94[7] = {
#include "assets/actor_560800_model_40968_partVerts.inc"
};

SVECTOR D_actor_560800_80171AB0[61] = {
#include "assets/actor_560800_model_40968_verts.inc"
};

SVECTOR D_actor_560800_80171C98[61] = {
#include "assets/actor_560800_model_40968_normals.inc"
};

u32 D_actor_560800_80171E80[578] = {
#include "assets/actor_560800_model_40968_stream.inc"
};

TmdSource D_actor_560800_80172788 = {
    0,
    3072,
    1040,
    7,
    D_actor_560800_80171A94,
    D_actor_560800_80171AB0,
    D_actor_560800_80171C98,
    D_actor_560800_80171998,
    D_actor_560800_80171E80,
};

TmdBone D_actor_560800_801727AC[7] = {
#include "assets/actor_560800_model_4177C_skeleton.inc"
};

u32 D_actor_560800_801728A8[7] = {
#include "assets/actor_560800_model_4177C_partVerts.inc"
};

SVECTOR D_actor_560800_801728C4[61] = {
#include "assets/actor_560800_model_4177C_verts.inc"
};

SVECTOR D_actor_560800_80172AAC[61] = {
#include "assets/actor_560800_model_4177C_normals.inc"
};

u32 D_actor_560800_80172C94[578] = {
#include "assets/actor_560800_model_4177C_stream.inc"
};

TmdSource D_actor_560800_8017359C = {
    0,
    3072,
    1040,
    7,
    D_actor_560800_801728A8,
    D_actor_560800_801728C4,
    D_actor_560800_80172AAC,
    D_actor_560800_801727AC,
    D_actor_560800_80172C94,
};

TmdBone D_actor_560800_801735C0[2] = {
#include "assets/actor_560800_model_41F28_skeleton.inc"
};

u32 D_actor_560800_80173608[2] = {
#include "assets/actor_560800_model_41F28_partVerts.inc"
};

SVECTOR D_actor_560800_80173610[41] = {
#include "assets/actor_560800_model_41F28_verts.inc"
};

SVECTOR D_actor_560800_80173758[49] = {
#include "assets/actor_560800_model_41F28_normals.inc"
};

u32 D_actor_560800_801738E0[282] = {
#include "assets/actor_560800_model_41F28_stream.inc"
};

TmdSource D_actor_560800_80173D48 = {
    0,
    1984,
    0,
    2,
    D_actor_560800_80173608,
    D_actor_560800_80173610,
    D_actor_560800_80173758,
    D_actor_560800_801735C0,
    D_actor_560800_801738E0,
};

AnimationPackedPose D_actor_560800_80173D6C[30] = {
#include "assets/actor_560800_animation_421C4_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80173ED4[8] = {
#include "assets/actor_560800_animation_421C4_bank4.inc"
};

AnimationRecord D_actor_560800_80173EF4[56] = {
#include "assets/actor_560800_animation_421C4_records.inc"
};

u16 D_actor_560800_80173FD4[8] = {
#include "assets/actor_560800_animation_421C4_indices.inc"
};

AnimationSet D_actor_560800_80173FE4 = {
    D_actor_560800_80173EF4,
    D_actor_560800_80173FD4,
    { NULL, D_actor_560800_80173D6C, NULL, NULL, D_actor_560800_80173ED4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_8017400C[30] = {
#include "assets/actor_560800_animation_42474_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80174174[10] = {
#include "assets/actor_560800_animation_42474_bank4.inc"
};

AnimationRecord D_actor_560800_8017419C[58] = {
#include "assets/actor_560800_animation_42474_records.inc"
};

u16 D_actor_560800_80174284[8] = {
#include "assets/actor_560800_animation_42474_indices.inc"
};

AnimationSet D_actor_560800_80174294 = {
    D_actor_560800_8017419C,
    D_actor_560800_80174284,
    { NULL, D_actor_560800_8017400C, NULL, NULL, D_actor_560800_80174174, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_801742BC[31] = {
#include "assets/actor_560800_animation_42720_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80174430[7] = {
#include "assets/actor_560800_animation_42720_bank4.inc"
};

AnimationRecord D_actor_560800_8017444C[57] = {
#include "assets/actor_560800_animation_42720_records.inc"
};

u16 D_actor_560800_80174530[8] = {
#include "assets/actor_560800_animation_42720_indices.inc"
};

AnimationSet D_actor_560800_80174540 = {
    D_actor_560800_8017444C,
    D_actor_560800_80174530,
    { NULL, D_actor_560800_801742BC, NULL, NULL, D_actor_560800_80174430, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80174568[30] = {
#include "assets/actor_560800_animation_429CC_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801746D0[9] = {
#include "assets/actor_560800_animation_429CC_bank4.inc"
};

AnimationRecord D_actor_560800_801746F4[58] = {
#include "assets/actor_560800_animation_429CC_records.inc"
};

u16 D_actor_560800_801747DC[8] = {
#include "assets/actor_560800_animation_429CC_indices.inc"
};

AnimationSet D_actor_560800_801747EC = {
    D_actor_560800_801746F4,
    D_actor_560800_801747DC,
    { NULL, D_actor_560800_80174568, NULL, NULL, D_actor_560800_801746D0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80174814[30] = {
#include "assets/actor_560800_animation_42C7C_bank1.inc"
};

AnimationPackedRotation D_actor_560800_8017497C[10] = {
#include "assets/actor_560800_animation_42C7C_bank4.inc"
};

AnimationRecord D_actor_560800_801749A4[58] = {
#include "assets/actor_560800_animation_42C7C_records.inc"
};

u16 D_actor_560800_80174A8C[8] = {
#include "assets/actor_560800_animation_42C7C_indices.inc"
};

AnimationSet D_actor_560800_80174A9C = {
    D_actor_560800_801749A4,
    D_actor_560800_80174A8C,
    { NULL, D_actor_560800_80174814, NULL, NULL, D_actor_560800_8017497C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80174AC4[30] = {
#include "assets/actor_560800_animation_42F44_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80174C2C[12] = {
#include "assets/actor_560800_animation_42F44_bank4.inc"
};

AnimationRecord D_actor_560800_80174C5C[62] = {
#include "assets/actor_560800_animation_42F44_records.inc"
};

u16 D_actor_560800_80174D54[8] = {
#include "assets/actor_560800_animation_42F44_indices.inc"
};

AnimationSet D_actor_560800_80174D64 = {
    D_actor_560800_80174C5C,
    D_actor_560800_80174D54,
    { NULL, D_actor_560800_80174AC4, NULL, NULL, D_actor_560800_80174C2C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80174D8C[30] = {
#include "assets/actor_560800_animation_43200_bank1.inc"
};

AnimationPackedRotation D_actor_560800_80174EF4[11] = {
#include "assets/actor_560800_animation_43200_bank4.inc"
};

AnimationRecord D_actor_560800_80174F20[60] = {
#include "assets/actor_560800_animation_43200_records.inc"
};

u16 D_actor_560800_80175010[8] = {
#include "assets/actor_560800_animation_43200_indices.inc"
};

AnimationSet D_actor_560800_80175020 = {
    D_actor_560800_80174F20,
    D_actor_560800_80175010,
    { NULL, D_actor_560800_80174D8C, NULL, NULL, D_actor_560800_80174EF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_560800_80175048[30] = {
#include "assets/actor_560800_animation_434A0_bank1.inc"
};

AnimationPackedRotation D_actor_560800_801751B0[8] = {
#include "assets/actor_560800_animation_434A0_bank4.inc"
};

AnimationRecord D_actor_560800_801751D0[56] = {
#include "assets/actor_560800_animation_434A0_records.inc"
};

u16 D_actor_560800_801752B0[8] = {
#include "assets/actor_560800_animation_434A0_indices.inc"
};

AnimationSet D_actor_560800_801752C0 = {
    D_actor_560800_801751D0,
    D_actor_560800_801752B0,
    { NULL, D_actor_560800_80175048, NULL, NULL, D_actor_560800_801751B0, NULL, NULL, NULL },
};

s32 D_actor_560800_801752E8 = 1;

s32 D_actor_560800_801752EC = 1;

AnimationSet* D_actor_560800_801752F0[9] = {
    NULL,
    &D_actor_560800_80173FE4,
    &D_actor_560800_80174294,
    &D_actor_560800_80174540,
    &D_actor_560800_801747EC,
    &D_actor_560800_80174A9C,
    &D_actor_560800_80174D64,
    &D_actor_560800_80175020,
    &D_actor_560800_801752C0,
};

ActorTransform D_actor_560800_80175314[8] = {
    { { 400, -2800, 900, 0 }, { 1360, 2048, 64, 0 } },
    { { 300, -2800, 800, 0 }, { 1430, 2048, 48, 0 } },
    { { 200, -2800, 800, 0 }, { 1070, 2048, 32, 0 } },
    { { 100, -2800, 1100, 0 }, { 1190, 2048, 16, 0 } },
    { { 0, -2800, 900, 0 }, { 1170, 2048, 0, 0 } },
    { { -100, -2800, 1100, 0 }, { 1480, 2048, -16, 0 } },
    { { -200, -2800, 900, 0 }, { 1150, 2048, -32, 0 } },
    { { -300, -2800, 900, 0 }, { 1200, 2048, -48, 0 } },
};

ActorTransform D_actor_560800_801753D4[8] = {
    { { 300, -3700, 900, 0 }, { 1280, 2048, 0, 0 } },
    { { 200, -3700, 800, 0 }, { 1280, 2048, 0, 0 } },
    { { 100, -3700, 800, 0 }, { 1280, 2048, 0, 0 } },
    { { 0, -3700, 1100, 0 }, { 1280, 2048, 0, 0 } },
    { { -100, -3700, 900, 0 }, { 1280, 2048, 0, 0 } },
    { { -200, -3700, 1100, 0 }, { 1280, 2048, 0, 0 } },
    { { -300, -3700, 900, 0 }, { 1280, 2048, 0, 0 } },
    { { -400, -3700, 900, 0 }, { 1280, 2048, 0, 0 } },
};

ActorTransform D_actor_560800_80175494[8] = {
    { { 250, -2800, 900, 0 }, { 1280, 2048, -80, 0 } },
    { { 200, -2800, 900, 0 }, { 1280, 2048, 64, 0 } },
    { { 150, -2800, 800, 0 }, { 1280, 2048, 48, 0 } },
    { { 100, -2800, 800, 0 }, { 1280, 2048, 32, 0 } },
    { { 50, -2800, 1100, 0 }, { 1280, 2048, 16, 0 } },
    { { 0, -2800, 900, 0 }, { 1280, 2048, 0, 0 } },
    { { -50, -2800, 1100, 0 }, { 1280, 2048, -16, 0 } },
    { { -100, -2800, 900, 0 }, { 1280, 2048, -32, 0 } },
};

ActorTransform D_actor_560800_80175554[8] = {
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_actor_560800_80175614[8] = {
    { { 0, -1100, -200, 0 }, { 1080, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 968, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1137, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 911, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1024, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1024, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1024, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1024, 2048, 0, 0 } },
};

Actor560800MessageEntry D_actor_560800_801756D4[3] = {
    { 2005, { .call3 = func_actor_560800_80139360 } },
    { 2004, { .call2 = func_actor_560800_80137F58 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = func_actor_560800_801384EC } },
};

u16 D_actor_560800_801756EC[8] = {
    1,
    2,
    3,
    1,
    2,
    3,
    1,
    2,
};

s32 D_actor_560800_801756FC[6] = {
    8340,
    -2200,
    4000,
    0,
    0,
    0,
};

s32 D_actor_560800_80175714[6] = {
    6950,
    -2200,
    3650,
    0,
    0,
    0,
};

s32 D_actor_560800_8017572C[6] = {
    6950,
    -2500,
    3500,
    0,
    0,
    0,
};

Actor560800MessageEntry D_actor_560800_80175744[3] = {
    { 2005, { .call3 = func_actor_560800_801393EC } },
    { 2004, { .call1 = func_actor_560800_80139440 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = func_actor_560800_80138A4C } },
};

TaskDesc D_actor_560800_8017575C[4] = {
    { TASK_BODY_COORD, 192, func_actor_560800_801386D4, { .model = NULL } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_560800_80137820, { .model = &D_actor_560800_80172788 } },
    { (TASK_BODY_TMD | 0x100), 192, func_actor_560800_80138FC8, { .model = &D_actor_560800_80173D48 } },
    { TASK_BODY_TMD, 192, func_actor_560800_80137BEC, { .model = &D_actor_560800_8017359C } },
};

Task* D_actor_560800_8017578C = NULL;

s32 D_actor_560800_80175790;

s32 D_actor_560800_80175794;

s32 D_actor_560800_80175798;

s32 D_actor_560800_8017579C;

s32 D_actor_560800_801757A0;

s32 D_actor_560800_801757A4;

u32 D_actor_560800_801757A8;

Task* D_actor_560800_801757AC;

extern ActorTransform* D_actor_560800_8016F35C[];

extern ActorTransform* D_actor_560800_8016F3E4[];

extern ActorTransform* D_actor_560800_8016F46C[];

extern ActorTransform* D_actor_560800_8016F4F4[];

extern Actor560800MessageEntry D_actor_560800_801756D4[3];

extern u16 D_actor_560800_801756EC[];

extern s32 D_actor_560800_801756FC[];

extern s32 D_actor_560800_80175714[];

extern s32 D_actor_560800_8017572C[];

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
extern Actor560800MessageEntry D_actor_560800_80175744[3];

static s32         func_actor_560800_80132340(Task* arg0);
static inline void Actor560800_ReseedAnim(Task* arg0, u16 id, s16 rate);
static void        func_actor_560800_80133540(u32 arg0);
static inline void Actor560800_PlayAnim(Task* task, u16 anim);
static inline void Actor560800_PlayAnimB(Task* task, u16 anim, s32 argC);
static inline void Actor560800_PlaySe(s16 arg4);
static inline void Actor560800_PlaySeB(s32 arg4);
static inline void Actor560800_SpawnSparksA(Task* task);
static inline void Actor560800_SpawnSparksB(Task* task);
static inline void Actor560800_ResetAnimSlots(Actor560800AnimWork* anim, s16 clip);
static inline void Actor560800_BlendSlotsFirst(Task* task, u16 id, s16 rate);
static inline void Actor560800_ResetSlots(Task* task, u16 id, u16 rate);
static void        func_actor_560800_80135BD8(Task* arg0);
static void        func_actor_560800_80136AA8(Task* arg0);
static void        func_actor_560800_801376E0(Task* arg0);
static void        func_actor_560800_80138BCC(Task* task);
static void        func_actor_560800_80138D04(Task* task);

void func_actor_560800_801321A0(Task* task)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue = &gCdCmdQueue;

    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state++;
            break;
        case 1:
            key = gGameSession->location;
            if (task->spawnArg1.value != 0) {
                key.loc.view = 0x65;
            } else {
                key.loc.view = 0x64;
            }
            slotParam[0] = Stream_FindSlot((u8*)&key, 0, 0);
            CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            task->state++;
            break;
        case 2:
            if (queue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
            }
            break;
        case 3:
            if (CdCmd_IsIdle()) {
                SetDispMask(0);
                task->state++;
            } else if (Pad_CheckFlag800()) {
                SetDispMask(0);
                CdCmd_ActivatePhase1();
                task->state++;
            }
            break;
        case 4:
            if (CdCmd_IsIdle()) {
                Stream_ResetRestoreState();
                task->state++;
            }
            break;
        case 5:
            if (Stream_RestoreAfterLoad(0, 1)) {
                taskKill(task);
                Display_ResetHeapWrapper();
            }
            break;
    }
}

static s32 func_actor_560800_80132340(Task* arg0)
{
    Actor560800Work*     work;
    ActorAnimStep*       table;
    ActorAnimStep*       entry;
    ActorAnimStep*       entry2;
    AnimationPlayRequest msg;
    u16                  anim;
    u16                  anim2;

    work = (Actor560800Work*)arg0->work;
    if (work->field_0 == NULL) {
        return 1;
    }
    table = D_actor_560800_8016EBE8;
    entry = &table[(u16)work->field_60];
    if (entry->hold != 0) {
        if (work->field_62 >= entry->hold) {
            if (entry->animId < 0) {
                return 1;
            }
            anim                     = entry->animId;
            msg.source.sets          = D_actor_560800_8016EA40;
            work->field_60           = anim;
            msg.animationId          = anim;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 0xA;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            Gp_DispatchMsgPtr(work->field_0, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            work->field_62 = 0;
        } else {
            work->field_62 += 1;
        }
    } else {
        if (Gp_DispatchMsg(work->field_0, 0x3ED, 0, 0) != 0) {
            return 0;
        }
        entry2 = &D_actor_560800_8016EBE8[(u16)work->field_60];
        if (entry2->animId < 0) {
            return 1;
        }
        work = (Actor560800Work*)arg0->work;
        if (work->field_0 != NULL) {
            anim2                    = entry2->animId;
            msg.source.sets          = D_actor_560800_8016EA40;
            work->field_60           = anim2;
            msg.animationId          = anim2;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 0xA;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            Gp_DispatchMsgPtr(work->field_0, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            work->field_62 = 0;
        }
    }
    return 0;
}

/// Cross-fades slots 1..`field_4BA`-1 of `work`'s animation context to animation
/// `id` over `frames` frames.
#define _ACTOR560800_BLEND_SLOTS(work, id, frames)                   \
    do {                                                             \
        u16 _i;                                                      \
        for (_i = 1; _i < (work)->field_4BA; _i++) {                 \
            func_800B4114(&(work)->rig.anim, _i, (id), 0, (frames)); \
        }                                                            \
    } while (0)

/// Restarts the animation clip's hold counter.
#define _actor560800ResetAnimHold(work) \
    do {                                \
        (work)->field_4BE = 0;          \
    } while (0)

/// Reseeds the animation slots of the task's own `Actor560800AnimWork`: the
/// id goes to `field_4B8` with `rate` in `field_4C8`, `field_4BE` is cleared,
/// and slots 1..`field_4BA` are blended through `func_800B4114`.
static inline void Actor560800_ReseedAnim(Task* arg0, u16 id, s16 rate)
{
    Actor560800AnimWork* w;
    u16                  i;

    w            = (Actor560800AnimWork*)arg0->work;
    w->field_4B8 = id;
    w->field_4C8 = rate;
    _actor560800ResetAnimHold(w);
    for (i = 1; i < w->field_4BA; i++) {
        func_800B4114(&w->rig.anim, i, id, 0, 10);
    }
}

/// Cross-fades animation slots 1..`field_4BA` of `work`'s rig to animation
/// `id` over `frames` frames.
#define _ACTOR560800_BLEND_SLOTS(work, id, frames)                   \
    do {                                                             \
        u16 _i;                                                      \
        for (_i = 1; _i < (work)->field_4BA; _i++) {                 \
            func_800B4114(&(work)->rig.anim, _i, (id), 0, (frames)); \
        }                                                            \
    } while (0)

/// Ticks every animation slot, then advances the script at `field_4B4`: a step
/// with a non-zero hold waits `hold` frames in `field_4BE`, a zero hold waits
/// for every slot to hold its boundary pose (`ANIMATION_SLOT_SETTLED`). Returns 1 when the next
/// step's id is negative (the script ended), 0 otherwise.
///
/// The step is re-indexed at every use rather than held in a local, and the
/// negative test is written as `>= 0` with an `else return 1`; both are needed
/// for the register choice and the jump layout.
static s32 func_actor_560800_80132498(Task* arg0)
{
    Actor560800AnimWork* work;
    u16                  i;
    u16                  done;

    work = (Actor560800AnimWork*)arg0->work;
    if (arg0->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
        return 0;
    }
    for (i = 1; i < work->field_4BA; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    i    = 1;
    done = 1;
    for (; i < work->field_4BA; i++) {
        if (!(work->rig.slots[i].flags & ANIMATION_SLOT_SETTLED)) {
            done = 0;
            break;
        }
    }
    if (work->field_4B4[(u16)work->field_4B8].hold != 0) {
        if ((u16)work->field_4BE >= work->field_4B4[(u16)work->field_4B8].hold) {
            if (work->field_4B4[(u16)work->field_4B8].animId >= 0) {
                Actor560800_ReseedAnim(arg0, work->field_4B4[(u16)work->field_4B8].animId, work->field_4C8);
            } else {
                return 1;
            }
        } else {
            work->field_4BE++;
        }
    } else if (done) {
        if (work->field_4B4[(u16)work->field_4B8].animId >= 0) {
            Actor560800_ReseedAnim(arg0, work->field_4B4[(u16)work->field_4B8].animId, work->field_4C8);
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
                TmdObject*           tmd   = arg0->extra.tmd;
                GfxCoord*            coord = tmd->coords;
                Actor560800AnimWork* block = Mem_Malloc(0x4CC, 0);
                AreaPlacement*       place;
                u8                   id;

                arg0->work = block;
                if (block == NULL) {
                    failed = 1;
                } else {
                    coord->parent = &gGfxViewCoord;
                    Mem_Set(arg0->work, 0, 0x4CC);
                    tmd->lightMtx  = &block->light;
                    tmd->colorMtx  = &block->color;
                    arg0->msgTable = D_actor_560800_8016F34C;
                    place          = Gp_GetNestedAreaRec(&gGameSession->location.loc)->field_0;
                    id             = place->entryId;
                    while (id != AREA_PLACEMENT_END) {
                        if (id == 0x83) {
                            break;
                        }
                        place++;
                        id = place->entryId;
                    }
                    Gp_SetTmdBytes(arg0->extra.tmd, place->texturePageOffset, place->clutRowOffset);
                    Task_Reparent(D_actor_560800_8017578C, arg0);
                    failed = 0;
                }
            }
            if (failed) {
                taskKill(arg0);
                return;
            }
            arg0->extra.tmd->flags &= ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            work                    = (Actor560800AnimWork*)arg0->work;
            {
                TmdObject* obj = arg0->extra.tmd;
                func_800B3F84(&work->rig.anim, D_actor_560800_8016EB04, obj, work->rig.poses, work->rig.slots);
            }
            work->field_4BA = 0x13;
            work->field_4B4 = D_actor_560800_8016ECAC;
            if (arg0->spawnArg1.value == 0) {
                Actor560800AnimWork* w = (Actor560800AnimWork*)arg0->work;
                u16                  i;
                s32                  fade = 0x10;

                w->field_4B8 = 0;
                w->field_4C8 = fade;
                w->field_4BE = 0;
                for (i = 1; i < w->field_4BA; i++) {
                    w->rig.slots[i].rate = fade;
                    Gp_AnimResetSlot(&w->rig.anim, i, 0);
                }
            } else {
                Actor560800AnimWork* w = (Actor560800AnimWork*)arg0->work;
                u16                  i;
                s32                  fade = 0x10;

                w->field_4B8 = 2;
                w->field_4C8 = fade;
                w->field_4BE = 0;
                for (i = 1; i < w->field_4BA; i++) {
                    w->rig.slots[i].rate = fade;
                    Gp_AnimResetSlot(&w->rig.anim, i, 2);
                }
            }
            arg0->state += 1;
            break;
        }
        case 2: // an empty case: GCC then roots the case tree at 1
            break;
        case 1: {
            s32 arg = arg0->spawnArg1.value;
            if (arg == 1) {
                Gp_DispatchMsgPtr(arg0, 0x7D4, &D_actor_560800_8016F154, 0);
                work->field_4BC = arg;
                arg0->state    += 1;
            }
        } break;
    }
    ofs.vx = 0;
    ofs.vy = 0x380;
    ofs.vz = 0;
    Gp_DrawFloorQuad(&arg0->extra.tmd->coords[1], 0x300, &ofs);
    func_actor_560800_80132498(arg0);
    if (work->field_4BC != 0) {
        TmdObject* obj = arg0->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = arg0->extra.tmd->coords->workm.t[1];
        pos.vz = arg0->extra.tmd->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
}

void func_actor_560800_80132A14(Task* arg0)
{
    Actor560800AnimWork* work = (Actor560800AnimWork*)arg0->work;
    VECTOR               pos;

    if (arg0->state == 0) {
        TmdObject*           tmd    = arg0->extra.tmd;
        Task*                parent = arg0->spawnArg2.pointer;
        GfxCoord*            coord  = tmd->coords;
        Actor560800AnimWork* block;
        AreaPlacement*       place;
        u8                   id;

        block      = Mem_Malloc(0x4CC, 0);
        arg0->work = block;
        if (block == NULL) {
            taskKill(arg0);
            return;
        }
        work = block;
        switch (arg0->spawnArg1.value) {
            case 0:
                coord->parent = &parent->extra.tmd->coords[12];
                break;
            case 1:
            case 2:
            case 3:
                coord->parent = &parent->extra.tmd->coords[8];
                break;
        }
        Mem_Set(arg0->work, 0, 0x4CC);
        tmd->lightMtx = &work->light;
        tmd->colorMtx = &work->color;
        if (arg0->spawnArg1.value < 2) {
            place = Gp_GetNestedAreaRec(&gGameSession->location.loc)->field_0;
            id    = place->entryId;
            while (id != AREA_PLACEMENT_END) {
                if (id == 0x65) {
                    break;
                }
                place++;
                id = place->entryId;
            }
            Gp_SetTmdBytes(arg0->extra.tmd, place->texturePageOffset, place->clutRowOffset);
        } else if (arg0->spawnArg1.value == 2) {
            Gp_SetTmdBytes(arg0->extra.tmd, 0, 0);
        } else if (arg0->spawnArg1.value == 3) {
            place = Gp_GetNestedAreaRec(&gGameSession->location.loc)->field_0;
            id    = place->entryId;
            while (id != AREA_PLACEMENT_END) {
                if (id == 0x22) {
                    break;
                }
                place++;
                id = place->entryId;
            }
            Gp_SetTmdBytes(arg0->extra.tmd, place->texturePageOffset, place->clutRowOffset);
        }
        Task_Reparent(parent, arg0);
        arg0->msgTable = D_actor_560800_8016F34C;
        arg0->state   += 1;
        return;
    }
    if (work->field_4BC != 0) {
        TmdObject* obj = arg0->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = arg0->extra.tmd->coords->workm.t[1];
        pos.vz = arg0->extra.tmd->coords->workm.t[2];
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
            TmdObject*           tmd   = arg0->extra.tmd;
            GfxCoord*            coord = tmd->coords;
            Actor560800AnimWork* block = Mem_Malloc(0x4CC, 0);
            AreaPlacement*       place;
            u8                   id;

            arg0->work = block;
            if (block == NULL) {
                failed = 1;
            } else {
                coord->parent = &gGfxViewCoord;
                Mem_Set(arg0->work, 0, 0x4CC);
                tmd->lightMtx  = &block->light;
                tmd->colorMtx  = &block->color;
                arg0->msgTable = D_actor_560800_8016F34C;
                place          = Gp_GetNestedAreaRec(&gGameSession->location.loc)->field_0;
                id             = place->entryId;
                while (id != AREA_PLACEMENT_END) {
                    if (id == 0x65) {
                        break;
                    }
                    place++;
                    id = place->entryId;
                }
                Gp_SetTmdBytes(arg0->extra.tmd, place->texturePageOffset, place->clutRowOffset);
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
            TmdObject* obj = arg0->extra.tmd;
            func_800B3F84(&work->rig.anim, D_actor_560800_8016EA74, obj, work->rig.poses, work->rig.slots);
        }
        work->field_4BA = 0x14;
        work->field_4B4 = D_actor_560800_8016EC1C;
        {
            Actor560800AnimWork* w = (Actor560800AnimWork*)arg0->work;
            u16                  i;
            s32                  fade = 0x10;

            w->field_4B8 = 0;
            w->field_4C8 = fade;
            w->field_4BE = 0;
            for (i = 1; i < w->field_4BA; i++) {
                w->rig.slots[i].rate = fade;
                Gp_AnimResetSlot(&w->rig.anim, i, 0);
            }
        }
        arg0->state += 1;
    }
    ofs.vx = 0;
    ofs.vy = 0x380;
    ofs.vz = 0;
    Gp_DrawFloorQuad(&arg0->extra.tmd->coords[1], 0x300, &ofs);
    if (work->field_4CA == 0) {
        func_actor_560800_80132498(arg0);
    }
    gfxRotMatrixY(&arg0->extra.tmd->coords[4].coord, work->field_4C0, 0);
    Gfx_RotMatrixX(&arg0->extra.tmd->coords[4].coord, work->field_4C6, 0);
    Gfx_RotMatrixZ(&arg0->extra.tmd->coords[2].coord, work->field_4C4, 0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_4CA != 0) {
        work->field_4C0 = 0;
        work->field_4C6 = 0;
        work->field_4C4 = 0;
    }
    if (work->field_4BC != 0) {
        TmdObject* obj = arg0->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = arg0->extra.tmd->coords->workm.t[1];
        pos.vz = arg0->extra.tmd->coords->workm.t[2];
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
            TmdObject*           tmd   = arg0->extra.tmd;
            GfxCoord*            coord = tmd->coords;
            Actor560800AnimWork* block = Mem_Malloc(0x4CC, 0);
            AreaPlacement*       place;
            u8                   id;

            arg0->work = block;
            if (block == NULL) {
                failed = 1;
            } else {
                coord->parent = &gGfxViewCoord;
                Mem_Set(arg0->work, 0, 0x4CC);
                tmd->lightMtx  = &block->light;
                tmd->colorMtx  = &block->color;
                arg0->msgTable = D_actor_560800_8016F34C;
                place          = Gp_GetNestedAreaRec(&gGameSession->location.loc)->field_0;
                id             = place->entryId;
                while (id != AREA_PLACEMENT_END) {
                    if (id == 0x22) {
                        break;
                    }
                    place++;
                    id = place->entryId;
                }
                Gp_SetTmdBytes(arg0->extra.tmd, place->texturePageOffset, place->clutRowOffset);
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
            TmdObject* obj = arg0->extra.tmd;
            func_800B3F84(&work->rig.anim, D_actor_560800_8016EB30, obj, work->rig.poses, work->rig.slots);
        }
        work->field_4BA = 0x13;
        work->field_4B4 = D_actor_560800_8016ECC4;
        {
            Actor560800AnimWork* w = (Actor560800AnimWork*)arg0->work;
            u16                  i;
            s32                  fade = 0x10;

            w->field_4B8 = 0;
            w->field_4C8 = fade;
            w->field_4BE = 0;
            for (i = 1; i < w->field_4BA; i++) {
                w->rig.slots[i].rate = fade;
                Gp_AnimResetSlot(&w->rig.anim, i, 0);
            }
        }
        arg0->state += 1;
    }
    if (!(arg0->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && work->field_4C2 == 0) {
        ofs.vx = 0;
        ofs.vy = 0x380;
        ofs.vz = 0;
        Gp_DrawFloorQuad(&arg0->extra.tmd->coords[1], 0x300, &ofs);
    }
    func_actor_560800_80132498(arg0);
    if (work->field_4BC != 0) {
        TmdObject* obj = arg0->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = arg0->extra.tmd->coords->workm.t[1];
        pos.vz = arg0->extra.tmd->coords->workm.t[2];
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
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    task = work->field_8;
    if (task != NULL) {
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    task = work->field_10;
    if (task != NULL) {
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    task = work->field_14;
    if (task != NULL) {
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    task = work->field_18;
    if (task != NULL) {
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    task = work->field_C;
    if (task != NULL) {
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    task = work->field_1C;
    if (task != NULL) {
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        func_800D7A9C(obj, &pos, 0, 3);
    }
    if (work->field_20 != NULL) {
        Actor560800Work* w = (Actor560800Work*)D_actor_560800_8017578C->work;

        ((SVECTOR*)&pos)->vy = 0;
        Gp_DispatchMsgPtr(w->field_20, 0x7DB, &pos, 0);
    }
}

static void func_actor_560800_80133540(u32 arg0)
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

void func_actor_560800_80133750(s32 arg0)
{
    Actor560800Work* work;
    ActorTransform*  msg;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    if (work->field_66 == 0) {
        Gp_PulseState1C();
    }
    if (work->field_0 != NULL) {
        msg = D_actor_560800_8016F35C[arg0];
        if (msg->pos.vx != 0) {
            func_actor_560800_80133540(0);
            Gp_DispatchMsgPtr(work->field_0, 0x3E9, msg, 0);
        } else {
            func_actor_560800_80133648(0);
        }
    }
    if (work->field_8 != NULL) {
        msg = D_actor_560800_8016F46C[arg0];
        if (msg->pos.vx != 0) {
            func_actor_560800_80133540(2);
            Gp_DispatchMsgPtr(work->field_8, 0x7D4, msg, 0);
        } else {
            func_actor_560800_80133648(2);
        }
    }
    if (work->field_C != NULL) {
        msg = D_actor_560800_8016F3E4[arg0];
        if (msg->pos.vx != 0) {
            func_actor_560800_80133540(3);
            Gp_DispatchMsgPtr(work->field_C, 0x7D4, msg, 0);
        } else {
            func_actor_560800_80133648(3);
        }
    }
    if (work->field_4 != NULL) {
        msg = D_actor_560800_8016F4F4[arg0];
        if (msg->pos.vx != 0) {
            func_actor_560800_80133540(1);
            Gp_DispatchMsgPtr(work->field_4, 0x7D4, msg, 0);
        } else {
            func_actor_560800_80133648(1);
        }
    }
    if (work->field_20 != NULL) {
        // No table of its own: reuses the payload picked for field_4.
        if (msg->pos.vx != 0) {
            func_actor_560800_80133540(4);
            Gp_DispatchMsgPtr(work->field_20, 0x7D4, msg, 0);
        } else {
            func_actor_560800_80133648(4);
        }
    }
}

static inline void Actor560800_PlayAnim(Task* task, u16 anim)
{
    Actor560800Work*     work;
    AnimationPlayRequest msg;

    work = (Actor560800Work*)task->work;
    if (work->field_0 != NULL) {
        msg.source.sets          = D_actor_560800_8016EA40;
        work->field_60           = anim;
        msg.animationId          = anim;
        msg.blend                = ANIMATION_BLEND_RESET;
        msg.blendFrames          = 0;
        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        Gp_DispatchMsgPtr(work->field_0, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
        work->field_62 = 0;
    }
}

static inline void Actor560800_PlayAnimB(Task* task, u16 anim, s32 argC)
{
    Actor560800Work*     work;
    AnimationPlayRequest msg;

    work = (Actor560800Work*)task->work;
    if (work->field_0 != NULL) {
        msg.source.sets          = D_actor_560800_8016EA40;
        work->field_60           = anim;
        msg.animationId          = anim;
        msg.blend                = ANIMATION_BLEND_INTERPOLATE;
        msg.blendFrames          = argC;
        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        Gp_DispatchMsgPtr(work->field_0, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
        work->field_62 = 0;
    }
}

static inline void Actor560800_PlaySe(s16 arg4)
{
    s32 msg[5];
    s32 val;

    val    = Player_Status.weapon;
    msg[0] = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? val + 1 : val + 0x22;
    msg[1] = arg4;
    msg[2] = 0;
    msg[3] = 0;
    msg[4] = 0;
    Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, msg, 0);
}

static inline void Actor560800_PlaySeB(s32 arg4)
{
    s32 msg[5];
    s32 val;

    val    = Player_Status.weapon;
    msg[0] = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? val + 1 : val + 0x22;
    msg[1] = arg4;
    msg[2] = 1;
    msg[3] = 0xA;
    msg[4] = 0;
    Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, msg, 0);
}

static inline void Actor560800_SpawnSparksA(Task* task)
{
    Actor560800Work* work;
    SVECTOR          vec;

    work   = (Actor560800Work*)task->work;
    vec.vx = 0x12C;
    vec.vy = 0;
    vec.vz = -0x1F4;
    Gp_SpawnEff(0x60046, work->field_0->extra.tmd->coords, 0x20000040, &vec);
    vec.vx = 0x190;
    vec.vy = 0;
    vec.vz = -0x258;
    Gp_SpawnEff(0x60046, work->field_0->extra.tmd->coords, 0x20000030, &vec);
    vec.vx = 0x12C;
    vec.vy = 0;
    vec.vz = -0x2BC;
    Gp_SpawnEff(0x60046, work->field_0->extra.tmd->coords, 0x20000020, &vec);
    vec.vx = 0x1C2;
    vec.vy = 0;
    vec.vz = -0x320;
    Gp_SpawnEff(0x60046, work->field_0->extra.tmd->coords, 0x20000020, &vec);
}

static inline void Actor560800_SpawnSparksB(Task* task)
{
    Actor560800Work* work;
    SVECTOR          vec;

    work   = (Actor560800Work*)task->work;
    vec.vx = 0x12C;
    vec.vy = 0;
    vec.vz = -0xC8;
    Gp_SpawnEff(0x60046, work->field_0->extra.tmd->coords, 0x20000040, &vec);
    vec.vx = 0x1F4;
    vec.vy = 0;
    vec.vz = -0x64;
    Gp_SpawnEff(0x60046, work->field_0->extra.tmd->coords, 0x20000020, &vec);
    vec.vx = 0x1C2;
    vec.vy = 0;
    vec.vz = 0;
    Gp_SpawnEff(0x60046, work->field_0->extra.tmd->coords, 0x20000020, &vec);
}

/// Requests driven by `field_28`, cleared once handled: the inline helpers play
/// an animation on the task at `field_0` (0x3F4), post a sound through
/// `gameGetPtrSlot(3)` (0x3E8) or spawn the 0x60046 spark effects on its part
/// coordinates. 18 and 35 are two-step sequences on `field_2A` / `field_2C`.
///
/// Shape notes, all needed for the match: helpers take only the arguments that
/// vary, because an inlined parameter is copied to a pseudo even when constant
/// and CSE would then share it; `Player_Status.coordMtx` is read as a struct member so the load is
/// in-struct and schedules after the `field_2C` store; the explicit clears in 19,
/// 28 and the last step of 35 decide which anim tails cross-jump together.
static void func_actor_560800_80133970(Task* arg0)
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
                        work->field_2C               += 5;
                        Player_Status.coordMtx->t[0] -= 5;
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
static void func_actor_560800_80134258(Task* task)
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
                    anim->rig.slots[i].rate = rate;
                    Gp_AnimResetSlot(&anim->rig.anim, i, 3);
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
            anim->rig.slots[i].rate = rate;
            Gp_AnimResetSlot(&anim->rig.anim, i, clip);
            i++;
        } while (i < anim->field_4BA);
    }
}

static void func_actor_560800_80134384(Task* task)
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
                        anim->rig.slots[i].rate = rate;
                        Gp_AnimResetSlot(&anim->rig.anim, i, 0x2D);
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
            anim->rig.slots[i].rate = rate;
            Gp_AnimResetSlot(&anim->rig.anim, i, 0x20);
            i++;
        } while (i < anim->field_4BA);
    }
    Gp_SpawnEff(0x6002B, &work->field_8->extra.tmd->coords[8], 0x21, NULL);
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
    _actor560800ResetAnimHold(w);
    count = w->field_4BA;
    __asm__("" : "=r"(first) : "0"((u16)1));
    if (first < count) {
        i = 1;
        do {
            func_800B4114(&w->rig.anim, i, id, 0, 10);
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
            anim->rig.slots[i].rate = rate;
            Gp_AnimResetSlot(&anim->rig.anim, i, id);
            i++;
        } while (i < anim->field_4BA);
    }
}

static void func_actor_560800_80134BFC(Task* arg0)
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
    GfxCoord*            coord;
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
                case 0: {
                    Actor560800AnimWork* reseed;

                    ctx               = (Actor560800AnimWork*)work->field_8->work;
                    ctx->field_4C0    = 0;
                    ctx->field_4CA    = 0;
                    reseed            = (Actor560800AnimWork*)work->field_8->work;
                    reseed->field_4B8 = 1;
                    reseed->field_4C8 = 0x10;
                    reseed->field_4BE = 0;
                    _ACTOR560800_BLEND_SLOTS(reseed, 1, 10);
                    work->field_42++;
                    return;
                }
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
                    coord              = work->field_8->extra.tmd->coords;
                    coord->coord.t[0] -= 0x1E;
                    if (work->field_8->extra.tmd->coords->coord.t[0] < D_actor_560800_8016F1CC[5].pos.vx) {
                        Gp_DispatchMsgPtr(work->field_8, 0x7D4, &D_actor_560800_8016F1CC[5], 0);
                        Actor560800_BlendSlotsFirst(work->field_8, 0x1A, 0x10);
                        work->field_40 = 0;
                    }
                    work->field_8->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
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
                        work->field_44               += 5;
                        Player_Status.coordMtx->t[0] -= 5;
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
                    Gp_SpawnEff(0x60055, &work->field_0->extra.tmd->coords[6], 0, NULL);
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
                case 0: {
                    Actor560800AnimWork* reseed;

                    reseed            = (Actor560800AnimWork*)work->field_8->work;
                    reseed->field_4B8 = 7;
                    reseed->field_4C8 = 0x10;
                    reseed->field_4BE = 0;
                    _ACTOR560800_BLEND_SLOTS(reseed, 7, 10);
                    work->field_44 = 0;
                    work->field_42++;
                    return;
                }
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
                    Gp_SpawnEff(0x6002B, &work->field_8->extra.tmd->coords[8], 0x21, NULL);
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
                            func_800B4114(&blend->rig.anim, i, 0x20, 0, 5);
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
                    Gp_SpawnEff(0x60055, &work->field_C->extra.tmd->coords[4], 0, NULL);
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
                    _ACTOR560800_BLEND_SLOTS(anim, 3, 10);
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

static void func_actor_560800_80135BD8(Task* arg0)
{
    Actor560800Work* work;
    Task*            sub5;
    Task*            sub6;
    SVECTOR          vec;

    work       = (Actor560800Work*)Mem_Malloc(0x68, 0);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    Mem_Set(work, 0, 0x68);
    work->field_0           = gameGetPtrSlot(3);
    D_actor_560800_8017578C = arg0;
    work->field_4           = Task_SpawnFromTable(D_actor_560800_801718F0, 4, 0, 0);
    sub5                    = Task_SpawnFromTable(D_actor_560800_801718F0, 5, 0, 0);
    work->field_8           = sub5;
    work->field_10          = Task_SpawnFromTable(D_actor_560800_801718F0, 7, 1, sub5);
    work->field_14          = Task_SpawnFromTable(D_actor_560800_801718F0, 8, 0, work->field_8);
    work->field_18          = Task_SpawnFromTable(D_actor_560800_801718F0, 9, 2, work->field_8);
    sub6                    = Task_SpawnFromTable(D_actor_560800_801718F0, 6, 0, 0);
    work->field_C           = sub6;
    work->field_1C          = Task_SpawnFromTable(D_actor_560800_801718F0, 0xA, 3, sub6);
    work->field_20          = Task_SpawnFromTable(D_actor_560800_8017575C, 0, 0, arg0);
    work->field_24          = Task_SpawnFromTable(D_actor_560800_8017575C, 2, 0, arg0);
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
            if (Gp_StateC08.field_A == 1 || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            func_actor_560800_80135BD8(arg0);
            Gp_CapFile = 0;
            Gp_LoadCapFile(0);
            func_800E6D4C(0x180, 0);
            arg0->state++;
        case 1:
            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
            func_800E8634(D_actor_560800_8016F5E0, 1, D_actor_560800_80171800);
            arg0->state++;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                gRandomLcgState = D_actor_560800_801757A8;
                Gp_PulseState1C();
                val    = Player_Status.weapon;
                msg[0] = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? val + 1 : val + 0x22;
                msg[1] = 1;
                msg[2] = 0;
                msg[3] = 0;
                msg[4] = 0;
                Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, msg, 0);
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
            Display_SpawnWithOt(D_actor_560800_801718F0, 0xC, 0, 0);
            break;
    }
    work->field_58 = 0;
}

void func_actor_560800_80135F50(Task* arg0)
{
    Display_SpawnWithOt(D_actor_560800_8016EA28, 1, arg0->spawnArg1.value, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
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
            arg0->work = alloc;
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
            work->r += (u16)arg0->spawnArg1.value;
            work->g += (u16)arg0->spawnArg1.value;
            work->b += (u16)arg0->spawnArg1.value;
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
            arg0->work = alloc;
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
            work->r -= (u16)arg0->spawnArg1.value;
            work->g -= (u16)arg0->spawnArg1.value;
            work->b -= (u16)arg0->spawnArg1.value;
            if (work->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

void func_actor_560800_801361A0(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = task->extra.tmd;
    switch (arg2) {
        case 0:
            break;
        case 1:
            extra->flags = extra->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
        case 2:
            extra->flags = extra->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
    }
}

#include "../../shared/actor_messages_place_ypr.inc.c"

/// Spawns entry 2 of the actor's task descriptor table with `arg0` as its
/// spawn argument. Script tables in the actor's data call it.
void func_actor_560800_80136280(s32 arg0)
{
    Task_SpawnFromTable(D_actor_560800_801718F0, 2, arg0, 0);
}

void func_actor_560800_801362B0(s32 arg0)
{
    Task_SpawnFromTable(D_actor_560800_801718F0, 3, arg0, 0);
}

void func_actor_560800_801362E0(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;
    ActorCommand     msg;

    msg.command = arg0;
    Gp_DispatchMsgPtr(work->field_20, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
}

void func_actor_560800_8013631C(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;
    ActorCommand     msg;

    msg.command = arg0;
    Gp_DispatchMsgPtr(work->field_24, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
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
    Actor560800Work*     work;
    AnimationPlayRequest msg;
    u16                  anim;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    if (work->field_0 != NULL) {
        anim                     = arg0;
        msg.source.sets          = D_actor_560800_8016EA40;
        work->field_60           = arg0;
        msg.animationId          = anim;
        msg.blend                = ANIMATION_BLEND_INTERPOLATE;
        msg.blendFrames          = 0xA;
        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        Gp_DispatchMsgPtr(work->field_0, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
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

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    anim = (Actor560800AnimWork*)work->field_4->work;

    anim->field_4B8 = arg0;
    anim->field_4C8 = 0x10;
    anim->field_4BE = 0;
    _ACTOR560800_BLEND_SLOTS(anim, arg0, 10);
}

/// Reseeds the animation slots of the sub-task at `field_C` from `arg0`: the
/// id goes to `field_4B8` with 0x10 as the restart rate in `field_4C8`,
/// `field_4BE` is cleared, and slots 1..`field_4BA` are blended through
/// `func_800B4114`. `func_actor_560800_801363F8` is the same body reached
/// through `field_4`.
void func_actor_560800_801364A0(u16 arg0)
{
    Actor560800Work*     work;
    Actor560800AnimWork* anim;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    anim = (Actor560800AnimWork*)work->field_C->work;

    anim->field_4B8 = arg0;
    anim->field_4C8 = 0x10;
    anim->field_4BE = 0;
    _ACTOR560800_BLEND_SLOTS(anim, arg0, 10);
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
    Fs_LoadImageChunk(D_8006C338[35].field_4, 1);
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

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    anim = (Actor560800AnimWork*)work->field_8->work;

    anim->field_4B8 = arg0;
    anim->field_4C8 = 0x10;
    anim->field_4BE = 0;
    _ACTOR560800_BLEND_SLOTS(anim, arg0, 10);
}

void func_actor_560800_80136678(s32 arg0)
{
    SndEvt_EnqueueType6(D_actor_560800_8016F57C[arg0], 0, 0);
}

/// Task state handler for the second spawn mode: states 1 and 2 — and state 0,
/// which first parks `gDisplayState.control.flags.flipMode` at 2 — only step the state, and state 3 runs
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
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
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
            Fs_LoadImageChunk(D_8006C338[36].field_4, 1);
            D5B498_8006C234 = 0;
            taskKill(arg0);
            Display_ResetHeapWrapper();
            work->field_4 = Task_SpawnOnDefaultList(D_actor_560800_801718F0, 0xB, 1, D_actor_560800_8017578C);
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
    Display_SpawnWithOt(D_actor_560800_801718F0, 0xD, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
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
static void func_actor_560800_80136AA8(Task* arg0)
{
    Actor560800ChainScratch* top;
    Actor560800ModelWork*    work;
    Actor560800ChainScratch* s;
    Actor560800PartsWork*    target;
    s16                      i;
    s16                      speed;
    s32                      a;

    top  = SCRATCH_STACK_CURSOR(Actor560800ChainScratch);
    work = (Actor560800ModelWork*)arg0->work;
    s = SCRATCH_STACK_CURSOR(Actor560800ChainScratch) = top - 1;
    target                                            = (Actor560800PartsWork*)work->field_26C->work;
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
    s->pos.vx = arg0->extra.tmd->coords->parent->coord.t[0] + arg0->extra.tmd->coords->coord.t[0];
    s->pos.vy = arg0->extra.tmd->coords->parent->coord.t[1] + arg0->extra.tmd->coords->coord.t[1];
    s->pos.vz = arg0->extra.tmd->coords->parent->coord.t[2] + arg0->extra.tmd->coords->coord.t[2];
    s->ang.vx = s->rot[0].vx;
    s->ang.vy = s->rot[0].vy;
    s->ang.vz = s->rot[0].vz;
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, s->rot[0].vy, 1);
    Gfx_RotMatrixX(&arg0->extra.tmd->coords->coord, s->rot[0].vx + 0x400, 0);
    Gfx_RotMatrixZ(&arg0->extra.tmd->coords->coord, s->rot[0].vz, 0);
    s->link  = arg0->extra.tmd->coords->coord;
    s->chain = s->link;
    gte_SetRotMatrix(&s->chain);
    for (i = 1; i < 6; i++) {
        gte_ldclmv(&arg0->extra.tmd->coords[i].coord);
        gte_rtir();
        gte_stclmv(&s->link);
        gte_ldclmv((char*)&arg0->extra.tmd->coords[i].coord + 2);
        gte_rtir();
        gte_stclmv((char*)&s->link + 2);
        gte_ldclmv((char*)&arg0->extra.tmd->coords[i].coord + 4);
        gte_rtir();
        gte_stclmv((char*)&s->link + 4);
        s->joint.vx = arg0->extra.tmd->coords[i + 1].coord.t[0];
        s->joint.vy = arg0->extra.tmd->coords[i + 1].coord.t[1];
        s->joint.vz = arg0->extra.tmd->coords[i + 1].coord.t[2];
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
        gfxRotMatrixY(&arg0->extra.tmd->coords[i].coord, s->rot[i].vy, 1);
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[i].coord, s->rot[i].vx, 0);
        Gfx_RotMatrixZ(&arg0->extra.tmd->coords[i].coord, s->rot[i].vz, 0);
        gte_SetRotMatrix(&s->chain);
        gte_ldclmv(&arg0->extra.tmd->coords[i].coord);
        gte_rtir();
        gte_stclmv(&s->chain);
        gte_ldclmv((char*)&arg0->extra.tmd->coords[i].coord + 2);
        gte_rtir();
        gte_stclmv((char*)&s->chain + 2);
        gte_ldclmv((char*)&arg0->extra.tmd->coords[i].coord + 4);
        gte_rtir();
        gte_stclmv((char*)&s->chain + 4);
        s->joint.vx = arg0->extra.tmd->coords[i + 1].coord.t[0];
        s->joint.vy = arg0->extra.tmd->coords[i + 1].coord.t[1];
        s->joint.vz = arg0->extra.tmd->coords[i + 1].coord.t[2];
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
    SCRATCH_STACK_RELEASE_BLOCK(Actor560800ChainScratch);
}

/// Sets up the animated model part the spawn argument names: allocates its
/// `Actor560800ModelWork`, hangs it off `Task::work`, points the object's light
/// and colour matrices into it, makes the named task this one's parent and hands
/// the part to `func_800B3F84` with the overlay's animation bank. The three
/// `gRandomLcgState` draws taken along the way seed the handlers' random headings,
/// and the slot count comes from the spawner's `spawnArg1`.
static void func_actor_560800_801376E0(Task* arg0)
{
    Actor560800ModelWork* mem;
    Actor560800ModelWork* work;
    TmdObject*            obj;
    GfxCoord*             coord;
    Task*                 child;

    obj        = arg0->extra.tmd;
    coord      = obj->coords;
    mem        = (Actor560800ModelWork*)Mem_Malloc(0x28C, 0);
    arg0->work = mem;
    if (mem == NULL) {
        taskKill(arg0);
        return;
    }
    Mem_Set(mem, 0, 0x28C);
    work            = (Actor560800ModelWork*)arg0->work;
    child           = (Task*)arg0->spawnArg2.pointer;
    work->field_26C = child;
    coord->parent   = child->extra.tmd->coords;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    Task_Reparent(work->field_26C, arg0);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_270 = gRandomLcgState >> 16;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_274 = gRandomLcgState >> 16;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_278 = (gRandomLcgState >> 16) & 0x3FF;
    func_800B3F84(&work->anim, D_actor_560800_801752F0, obj, work->poseBuf,
                  work->slots);
    work->field_280 = arg0->spawnArg1.value;
}

/// Per-frame handler of the animated model part `func_actor_560800_801376E0`
/// sets up. State 1 hides the part (`TmdObject::flags` bit 0x80) for the
/// part ids the current view excludes and otherwise runs
/// `func_actor_560800_80136AA8`; state 2 resets all seven animation slots to
/// `field_280`, state 3 ticks them, state 4 copies the coordinates of a part
/// spawned from `D_actor_560800_8017575C` and state 5 kills the task a frame
/// later. Every frame that survives rebuilds the root translation and, while
/// visible, drives `func_shelter_b1_pod_service_gantry_8017F450` and the periodic `Gp_SpawnEff`.
void func_actor_560800_80137820(Task* arg0)
{
    Actor560800ModelWork* work;
    TmdObject*            extra;
    GfxCoord*             coord;
    Actor560800ModelWork* anim;
    Task*                 child;
    s32                   i;
    u32                   tick;
    TmdObject*            obj;
    u32                   state;
    u16                   id;
    SVECTOR               unused; // never touched; only reserves the frame slot

    extra = arg0->extra.tmd;
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
            if (Gp_FindViewIndex(gGameSession->location.loc.view) == 0x16) {
                switch (work->field_280) {
                    case 1:
                    case 3:
                        obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
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
                    obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    return;
                }
            }
            func_actor_560800_80136AA8(arg0);
            break;
        case 2:
            if (work->field_280 < 4) {
                if (work->field_280 >= 2) {
                    obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    return;
                }
            }
            i    = 1;
            id   = work->field_280;
            anim = (Actor560800ModelWork*)arg0->work;
            do {
                anim->slots[i & 0xFFFF].rate = ANIMATION_RATE_ONE;
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
                if (!(anim->slots[i & 0xFFFF].flags & ANIMATION_SLOT_SETTLED)) {
                    break;
                }
            }
            break;
        case 4:
            child = Task_SpawnFromTable(D_actor_560800_8017575C, 3,
                                        D_actor_560800_801757AC->extra.tmd->coords->coord.t[1],
                                        arg0->spawnArg2.pointer);
            if (child == NULL) {
                arg0->state = 1;
                return;
            }
            i = 0;
            do {
                Mem_CopyUnaligned(&arg0->extra.tmd->coords[i & 0xFFFF].coord,
                                  &child->extra.tmd->coords[i & 0xFFFF].coord, 0x20);
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
    coord->coord.t[0]   = work->field_254 + work->field_24C;
    coord->coord.t[1]   = (s16)work->field_256 + work->field_24E;
    coord->coord.t[2]   = work->field_258 + work->field_250;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (!(obj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        func_shelter_b1_pod_service_gantry_8017F450(&arg0->extra.tmd->coords[6], work->field_280, 0x100, 0x3C36);
        if (Gp_FindViewIndex(gGameSession->location.loc.view) != 0x16) {
            tick = D_actor_560800_801752E8 + 1;
            if (!(tick & 0x7F) && ((tick >> 7) & 7) == work->field_280) {
                Gp_SpawnEff(0x601C6, &arg0->extra.tmd->coords[2], 0x800, NULL);
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
    GfxCoord*             coord;
    TmdObject*            extra;
    VECTOR                vec;
    s32                   i;
    s32                   j;
    u16                   t286;
    u16                   t288;

    work  = (Actor560800ModelWork*)task->work;
    coord = task->extra.tmd->coords;
    switch (task->state) {
        case 0:
            func_actor_560800_801376E0(task);
            coord->parent          = D_actor_560800_801757AC->extra.tmd->coords;
            task->extra.tmd->flags = 0;
            work                   = (Actor560800ModelWork*)task->work;
            i                      = 1;
            do {
                work->swingDir[i & 0xFFFF] = 0;
                i                         += 1;
            } while ((u32)(i & 0xFFFF) < 6U);
            work->field_28A     = coord->coord.t[1];
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
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
                gfxRotMatrixY(&task->extra.tmd->coords[j].coord, work->rot[j].vy, 1);
                Gfx_RotMatrixX(&task->extra.tmd->coords[j].coord,
                               work->rot[j].vx + work->swing[j].vx, 0);
                Gfx_RotMatrixZ(&task->extra.tmd->coords[j].coord,
                               work->rot[j].vz + work->swing[j].vz, 0);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 6U);
            t286              = work->field_286 + 4;
            t288              = work->field_288 + t286;
            work->field_288   = t288;
            work->field_286   = t286;
            coord->coord.t[1] = work->field_28A + task->spawnArg1.value -
                                D_actor_560800_801757AC->extra.tmd->coords->coord.t[1] +
                                (s16)t288;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (coord->coord.t[1] > 10000) {
                taskKill(task);
                return;
            }
            break;
    }
    extra  = task->extra.tmd;
    vec.vx = extra->coords->workm.t[0];
    vec.vy = task->extra.tmd->coords->workm.t[1];
    vec.vz = task->extra.tmd->coords->workm.t[2];
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
    ActorTransform*       pose;
    Actor560800ModelWork* part;
    GfxCoord*             coord;
    OverlayMat*           mat;
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
                    coord              = work->parts[i & 0xFFFF]->extra.tmd->coords;
                    mat                = (OverlayMat*)&coord->coord;
                    mat->ident.m00_m01 = 0x1000;
                    mat->ident.m02_m10 = 0;
                    mat->ident.m11_m12 = 0x1000;
                    mat->ident.m20_m21 = 0;
                    mat->ident.m22     = 0x1000;
                    coord->parent      = &gGfxViewCoord;
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
                    coord           = work->parts[i & 0xFFFF]->extra.tmd->coords;
                    coord->parent   = D_actor_560800_801757AC->extra.tmd->coords;
                    part->field_254 = pose->pos.vx;
                    part->field_256 = pose->pos.vy;
                    part->field_258 = pose->pos.vz;
                    gfxRotMatrixY(&coord->coord, pose->rot.vy, 1);
                    Gfx_RotMatrixX(&coord->coord, pose->rot.vx + 0x400, 0);
                    Gfx_RotMatrixZ(&coord->coord, pose->rot.vz, 0);
                    part->rot[0].vx = pose->rot.vx;
                    part->rot[0].vy = pose->rot.vy;
                    part->rot[0].vz = pose->rot.vz;
                    part->field_27C = flag;
                    j               = 1;
                    do {
                        gfxRotMatrixY(&work->parts[i & 0xFFFF]->extra.tmd->coords[j & 0xFFFF].coord, 0, 1);
                        Gfx_RotMatrixX(&work->parts[i & 0xFFFF]->extra.tmd->coords[j & 0xFFFF].coord, 0, 0);
                        Gfx_RotMatrixZ(&work->parts[i & 0xFFFF]->extra.tmd->coords[j & 0xFFFF].coord, 0, 0);
                        part->rot[j & 0xFFFF].vx = 0;
                        part->rot[j & 0xFFFF].vy = 0;
                        part->rot[j & 0xFFFF].vz = 0;
                        j++;
                    } while ((u32)(j & 0xFFFF) < 7U);
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
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
            part                                  = (Actor560800ModelWork*)work->parts[i & 0xFFFF]->work;
            coord                                 = work->parts[i & 0xFFFF]->extra.tmd->coords;
            task->extra.tmd->coords->coord.t[0]   = msg->vx;
            task->extra.tmd->coords->coord.t[1]   = msg->vy;
            task->extra.tmd->coords->coord.t[2]   = msg->vz;
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            part->field_254                       = pose->pos.vx;
            part->field_256                       = pose->pos.vy;
            part->field_258                       = pose->pos.vz;
            part->field_27C                       = flag;
            if (work->field_48 != 0) {
                gfxRotMatrixY(&coord->coord, pose->rot.vy, 1);
                Gfx_RotMatrixX(&coord->coord, pose->rot.vx + 0x400, 0);
                Gfx_RotMatrixZ(&coord->coord, pose->rot.vz, 0);
                part->rot[0].vx = pose->rot.vx;
                part->rot[0].vy = pose->rot.vy;
                part->rot[0].vz = pose->rot.vz;
                j               = 1;
                do {
                    gfxRotMatrixY(&work->parts[i & 0xFFFF]->extra.tmd->coords[j & 0xFFFF].coord, 0, 1);
                    Gfx_RotMatrixX(&work->parts[i & 0xFFFF]->extra.tmd->coords[j & 0xFFFF].coord, 0, 0);
                    Gfx_RotMatrixZ(&work->parts[i & 0xFFFF]->extra.tmd->coords[j & 0xFFFF].coord, 0, 0);
                    part->rot[j & 0xFFFF].vx = 0;
                    part->rot[j & 0xFFFF].vy = 0;
                    part->rot[j & 0xFFFF].vz = 0;
                    j++;
                } while ((u32)(j & 0xFFFF) < 7U);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
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
void func_actor_560800_801384EC(Task* task, s32 msgId, ActorCommand* msg)
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
                    extra  = part->extra.tmd;
                    vec.vx = extra->coords->workm.t[0];
                    vec.vy = part->extra.tmd->coords->workm.t[1];
                    vec.vz = part->extra.tmd->coords->workm.t[2];
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

/// Handler of the parts task. State 0 allocates its `Actor560800PartsWork`,
/// roots the model at `gGfxViewCoord`, reparents the spawner's task, spawns the
/// eight part tasks and swaps `gRandomLcgState` out for a zero seed; state 2 grows
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
    GfxCoord*             root;
    GfxCoord*             partCoord;
    GfxCoord*             effCoord;
    GfxCoord*             c;
    Task*                 part;
    SVECTOR               pos;
    s32                   i;
    s32                   n;
    s16                   k;

    work = (Actor560800PartsWork*)task->work;
    switch (task->state) {
        case 0:
            root       = task->extra.coordBody->coord;
            w          = (Actor560800PartsWork*)Mem_Malloc(0x4C, 0);
            task->work = w;
            if (w == NULL) {
                taskKill(task);
            } else {
                root->parent = &gGfxViewCoord;
                Mem_Set(task->work, 0, 0x4C);
                i                 = 0;
                spawned           = w;
                spawned->field_40 = (Task*)task->spawnArg2.pointer;
                task->msgTable    = D_actor_560800_801756D4;
                Task_Reparent(spawned->field_40, task);
                do {
                    spawned->parts[i & 0xFFFF] =
                        Task_SpawnFromTable(D_actor_560800_8017575C, 1, (i & 0xFFFF) + 1, task);
                    i++;
                } while ((u32)(i & 0xFFFF) < 8U);
                D_actor_560800_801757A8 = gRandomLcgState;
                gRandomLcgState         = 0;
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
                    partCoord         = part->extra.tmd->coords;
                    model->field_256 += D_actor_560800_801756EC[n & 0xFFFF];
                    if (D_actor_560800_80175314[n & 0xFFFF].pos.vy < (s16)model->field_256) {
                        model->field_256 = D_actor_560800_80175314[n & 0xFFFF].pos.vy;
                    }
                    partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                }
                n++;
            } while ((u32)(n & 0xFFFF) < 8U);
            break;
        case 3:
            for (k = 0; k < 8; k++) {
                if (work->parts[k] != NULL) {
                    effCoord = &work->parts[k]->extra.tmd->coords[3];
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
        c = ((Actor560800Work*)w->field_40->work)->field_4->extra.tmd->coords;
        Gp_ComposeParentWorld(&c[9], &w->world, &pos);
    } else if (w->field_4A == 0x22) {
        c = ((Actor560800Work*)w->field_40->work)->field_C->extra.tmd->coords;
        Gp_ComposeParentWorld(&c[9], &w->world, &pos);
    }
    w->world.t[0] = pos.vx;
    w->world.t[1] = pos.vy - 0x78;
    w->world.t[2] = pos.vz;
}

void func_actor_560800_80138A4C(Task* task, s32 msgId, ActorCommand* msg)
{
    Actor560800ModelWork* work;
    TmdObject*            extra;
    VECTOR                vec;

    work = (Actor560800ModelWork*)task->work;
    switch (msg->command) {
        case 0:
            extra  = task->extra.tmd;
            vec.vx = extra->coords->workm.t[0];
            vec.vy = task->extra.tmd->coords->workm.t[1];
            vec.vz = task->extra.tmd->coords->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
        case 1:
            task->state = 1;
            break;
        case 2:
            Gp_DispatchMsg(task, 0x7D5, 1, 0);
            Gp_DispatchMsgPtr(task, 0x7D4, D_actor_560800_801756FC, 0);
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
            Gp_DispatchMsgPtr(task, 0x7D4, D_actor_560800_80175714, 0);
            work->field_278 = 0x1000;
            task->state     = 5;
            break;
        case 6:
            Gp_DispatchMsg(task, 0x7D5, 1, 0);
            Gp_DispatchMsgPtr(task, 0x7D4, D_actor_560800_8017572C, 0);
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
static void func_actor_560800_80138BCC(Task* task)
{
    Actor560800ModelWork* work;
    GfxCoord*             coord;
    GfxCoord*             c;
    Actor560800ModelWork* w;
    MATRIX*               m;
    VECTOR                scale;

    work  = (Actor560800ModelWork*)task->work;
    coord = task->extra.tmd->coords;
    if (D_actor_560800_801752E8 & 1) {
        work->field_286++;
    }
    w                  = work;
    coord->coord.t[1] -= work->field_286;
    if (work->field_278 >= 0x800) {
        work->field_27C      = 0;
        w                    = (Actor560800ModelWork*)task->work;
        c                    = task->extra.tmd->coords;
        m                    = &c[1].coord;
        MATRIX_PAIR(m, 0, 0) = 0x1000;
        MATRIX_PAIR(m, 0, 2) = 0;
        MATRIX_PAIR(m, 1, 1) = 0x1000;
        MATRIX_PAIR(m, 2, 0) = 0;
        m->m[2][2]           = 0x1000;
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
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Per-frame rise of the model part, driven by `field_282`: phase 0 lifts the
/// root coordinate until it clears -3000, phase 1 keeps lifting while pulsing
/// the second coordinate's X/Z scale in steps of 0x32 until -1200, and phase 2
/// pulses in steps of 0xC8 until `field_278` drops below 0x1000. Phase 3 sinks
/// this part and the one `Actor560800Work::field_C` names together. Each case
/// needs its own matrix pointer: a shared one is set twice, loses sched1's
/// birthing priority, and swaps the `work`/`field_8` loads.
static void func_actor_560800_80138D04(Task* task)
{
    Actor560800ModelWork* work;
    GfxCoord*             coord;
    GfxCoord*             c;
    GfxCoord*             other;
    Actor560800ModelWork* w;
    MATRIX*               m;
    MATRIX*               m2;
    VECTOR                scale;
    s32                   one;

    work  = (Actor560800ModelWork*)task->work;
    coord = task->extra.tmd->coords;
    switch (work->field_282) {
        case 0:
            if (coord->coord.t[1] >= -3000) {
                work->field_282++;
            }
            break;
        case 1:
            if (work->field_278 <= 0x1800) {
                work->field_27C      = 1;
                w                    = (Actor560800ModelWork*)task->work;
                c                    = task->extra.tmd->coords;
                m                    = &c[1].coord;
                one                  = 0x1000;
                MATRIX_PAIR(m, 0, 0) = one;
                MATRIX_PAIR(m, 0, 2) = 0;
                MATRIX_PAIR(m, 1, 1) = one;
                MATRIX_PAIR(m, 2, 0) = 0;
                m->m[2][2]           = one;
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
                work->field_27C       = 0;
                w                     = (Actor560800ModelWork*)task->work;
                c                     = task->extra.tmd->coords;
                m2                    = &c[1].coord;
                one                   = 0x1000;
                MATRIX_PAIR(m2, 0, 0) = one;
                MATRIX_PAIR(m2, 0, 2) = 0;
                MATRIX_PAIR(m2, 1, 1) = one;
                MATRIX_PAIR(m2, 2, 0) = 0;
                m2->m[2][2]           = one;
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
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
        case 3:
            other               = ((Actor560800Work*)((Task*)task->spawnArg2.pointer)->work)->field_C->extra.tmd->coords;
            coord->coord.t[1]  -= 20;
            other->coord.t[1]  -= 20;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            other->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]  += 100;
}

void func_actor_560800_80138FC8(Task* task)
{
    Actor560800ModelWork* work;
    Actor560800ModelWork* w;
    Actor560800ModelWork* mem;
    TmdObject*            obj;
    GfxCoord*             coord;
    GfxCoord*             c;
    MATRIX*               m0;
    MATRIX*               m2;
    MATRIX*               m3;
    MATRIX*               m5;
    VECTOR                scale;
    GfxCoord*             root;

    switch (task->state) {
        case 0:
            obj        = task->extra.tmd;
            root       = obj->coords;
            task->work = Mem_Malloc(0x28C, 0);
            if (task->work == NULL) {
                taskKill(task);
            } else {
                Mem_Set(task->work, 0, 0x28C);
                mem            = (Actor560800ModelWork*)task->work;
                root->parent   = &gGfxViewCoord;
                mem->field_26C = (Task*)task->spawnArg2.pointer;
                obj->lightMtx  = &mem->light;
                obj->colorMtx  = &mem->color;
                Task_Reparent((Task*)task->spawnArg2.pointer, task);
                task->msgTable          = D_actor_560800_80175744;
                D_actor_560800_801757AC = task;
                m0                      = &root->coord;
                MATRIX_PAIR(m0, 0, 0)   = 0x1000;
                MATRIX_PAIR(m0, 0, 2)   = 0;
                MATRIX_PAIR(m0, 1, 1)   = 0x1000;
                MATRIX_PAIR(m0, 2, 0)   = 0;
                m0->m[2][2]             = 0x1000;
            }
            task->state++;
            return;
        case 1:
            task->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 2:
            coord              = task->extra.tmd->coords;
            work               = (Actor560800ModelWork*)task->work;
            coord->coord.t[1] += 5;
            if (work->field_278 <= 0x1800) {
                work->field_27C       = 1;
                c                     = task->extra.tmd->coords;
                w                     = (Actor560800ModelWork*)task->work;
                m2                    = &c[1].coord;
                MATRIX_PAIR(m2, 0, 0) = 0x1000;
                MATRIX_PAIR(m2, 0, 2) = 0;
                MATRIX_PAIR(m2, 1, 1) = 0x1000;
                MATRIX_PAIR(m2, 2, 0) = 0;
                m2->m[2][2]           = 0x1000;
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
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        case 3:
            coord              = task->extra.tmd->coords;
            work               = (Actor560800ModelWork*)task->work;
            coord->coord.t[1] += 1;
            if (work->field_278 >= 0x800) {
                work->field_27C       = 0;
                c                     = task->extra.tmd->coords;
                w                     = (Actor560800ModelWork*)task->work;
                m3                    = &c[1].coord;
                MATRIX_PAIR(m3, 0, 0) = 0x1000;
                MATRIX_PAIR(m3, 0, 2) = 0;
                MATRIX_PAIR(m3, 1, 1) = 0x1000;
                MATRIX_PAIR(m3, 2, 0) = 0;
                m3->m[2][2]           = 0x1000;
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
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        case 4:
            func_actor_560800_80138BCC(task);
            break;
        case 5:
            coord              = task->extra.tmd->coords;
            work               = (Actor560800ModelWork*)task->work;
            coord->coord.t[1] += 5;
            if (work->field_278 <= 0x1800) {
                work->field_27C       = 1;
                c                     = task->extra.tmd->coords;
                w                     = (Actor560800ModelWork*)task->work;
                m5                    = &c[1].coord;
                MATRIX_PAIR(m5, 0, 0) = 0x1000;
                MATRIX_PAIR(m5, 0, 2) = 0;
                MATRIX_PAIR(m5, 1, 1) = 0x1000;
                MATRIX_PAIR(m5, 2, 0) = 0;
                m5->m[2][2]           = 0x1000;
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
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
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
            obj = part->extra.tmd;
            switch (arg2) {
                case 0:
                    break;
                case 1:
                    obj->flags = obj->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
                    break;
                case 2:
                    obj->flags = obj->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
                    break;
            }
        }
        i += 1;
    } while ((u32)(i & 0xFFFF) < 8U);
}

void func_actor_560800_801393EC(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = task->extra.tmd;
    switch (arg2) {
        case 0:
            break;
        case 1:
            extra->flags = extra->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
        case 2:
            extra->flags = extra->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
    }
}

/// A second copy of the handler, under this file's own name.
#define actorMsgPlaceYawPitchRoll func_actor_560800_80139440
#include "../../shared/actor_messages_place_ypr.inc.c"
#undef actorMsgPlaceYawPitchRoll
