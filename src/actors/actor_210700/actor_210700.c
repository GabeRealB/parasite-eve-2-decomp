#include "actors/actor_210700.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

extern GpuImageUpload D_actor_210700_8015858C[2];

/// `_Actor210700Work::blinkStep`: the eye image the blink posts next.
///
/// A blink request posts the half-open eyes itself, so the three steps that
/// follow close the eyes and open them again.
enum {
    ACTOR_210700_BLINK_NONE   = 0, // No blink in progress
    ACTOR_210700_BLINK_CLOSED = 1, // The closed eyes are posted next
    ACTOR_210700_BLINK_HALF   = 2, // The half-open eyes are posted next
    ACTOR_210700_BLINK_OPEN   = 3, // The open eyes are posted next, which ends the blink
};

/// Work block of Rupert Broderick's body, the package's one actor.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens with the twenty-part rig and what the rig
/// plays, the same values an `ActorModelState` holds but kept as words; the
/// model object borrows `light` and `color` for as long as the block lives.
/// The tail is the blink that swaps the eye texture of the face and the
/// delayed free of the model's buffers once the model has been hidden.
///
/// Nothing in the package reads or writes `unknown_4C0`; the block is
/// allocated at its full size, so the bytes are its own, but nothing shows
/// what they hold.
typedef struct {
    ActorAnimRig20 rig;               // Playback storage of the body model; slots 1 to 19 are driven
    s32            ticking;           // Set once a clip has been applied, never cleared: the slots are ticked each frame
    s32            animId;            // Clip the slots were last seeded with, within `bank` (`ACTOR_MODEL_STATE_NONE` before the first)
    s32            bank;              // Index, in the package's animation bank table, of the bank the rig is bound to (`ACTOR_MODEL_STATE_NONE` before the first)
    MATRIX         light;             // Light-direction matrix lent to the model object
    MATRIX         color;             // Light-colour matrix lent to the model object
    byte           unknown_4C0[0x78]; // Never accessed
    s16            blinkFrameDelay;   // Value `blinkCountdown` restarts from after the closed and the half-open eyes: each is shown for this many ticks plus one
    s16            blinkCountdown;    // Ticks left before the blink posts its next eye image, which the tick taking it below 0 does; not reset as a blink starts or ends
    s16            blinkStep;         // Eye image the blink posts next (0 `ACTOR_210700_BLINK_NONE`, else `_CLOSED`, `_HALF` or `_OPEN`)
    s16            freeCountdown;     // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} _Actor210700Work;
STATIC_ASSERT_SIZEOF(_Actor210700Work, 0x540);

/// Animation banks the play-animation handler binds, indexed by its request's
/// `source.index`.
extern AnimationSet*  D_actor_210700_801585AC[7];
extern AnimationSet** D_actor_210700_801585C8[1];

/// The actor's message table, parked in `Task::msgTable`: 0x7D3
/// `func_actor_210700_8014A224`, 0x7D4 `func_actor_210700_8014A344`, 0x7D5
/// `func_actor_210700_8014A3D4`, 0x7E0 `func_actor_210700_8014A4B0`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_210700_801585D8[];

/// Eye images the blink and the 0x7E0 handler post over the
/// model's texture.

static void func_actor_210700_80149F90(Task* task);
static void func_actor_210700_8014A0AC(Task* task);
static void func_actor_210700_8014A1E8(Task* task);
static void func_actor_210700_8014A208(Task* arg0);
s32         func_actor_210700_8014A224(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);
s32         func_actor_210700_8014A344(Task* task, s32 arg1, ActorTransform* args, s32 arg3);

/// The actor's three task states - spawn, tick and teardown - which
/// `func_actor_210700_80149F38` runs by `Task::state`.
static const TaskFuncTable3 D_actor_210700_80149E24 = { {
    func_actor_210700_80149F90,
    func_actor_210700_8014A0AC,
    func_actor_210700_8014A1E8,
} };

static AnimationSet _gActor210700Animation0CC20;
static AnimationSet _gActor210700Animation0CFE0;
static AnimationSet _gActor210700Animation0D240;
static AnimationSet _gActor210700Animation0D72C;
static AnimationSet _gActor210700Animation0DB7C;
static AnimationSet _gActor210700Animation0DE04;
static TmdSource    _gActor210700RupertBroderickBody1;
s32                 func_actor_210700_8014A224(Task*, s32, AnimationPlayRequest*, s32);
s32                 func_actor_210700_8014A344(Task* task, s32 msgId, ActorTransform* args, s32);
s32                 func_actor_210700_8014A3D4(Task*, s32, s32, s32);
s32                 func_actor_210700_8014A4B0(Task*, s32, s32, s32);
void                func_actor_210700_80149F38(Task*);

static AnimationPackedPose _gActor210700Animation01E2CBank1[49] = {
#include "assets/actor_210700_animation_01E2C_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation01E2CBank4[592] = {
#include "assets/actor_210700_animation_01E2C_bank4.inc"
};

static AnimationRecord _gActor210700Animation01E2CRecords[714] = {
#include "assets/actor_210700_animation_01E2C_records.inc"
};

static u16 _gActor210700Animation01E2CIndices[20] = {
#include "assets/actor_210700_animation_01E2C_indices.inc"
};

AnimationSet gActor210700Animation01E2C = {
    _gActor210700Animation01E2CRecords,
    _gActor210700Animation01E2CIndices,
    { NULL, _gActor210700Animation01E2CBank1, NULL, NULL, _gActor210700Animation01E2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210700Animation027F8Bank1[18] = {
#include "assets/actor_210700_animation_027F8_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation027F8Bank4[226] = {
#include "assets/actor_210700_animation_027F8_bank4.inc"
};

static AnimationRecord _gActor210700Animation027F8Records[327] = {
#include "assets/actor_210700_animation_027F8_records.inc"
};

static u16 _gActor210700Animation027F8Indices[20] = {
#include "assets/actor_210700_animation_027F8_indices.inc"
};

AnimationSet gActor210700Animation027F8 = {
    _gActor210700Animation027F8Records,
    _gActor210700Animation027F8Indices,
    { NULL, _gActor210700Animation027F8Bank1, NULL, NULL, _gActor210700Animation027F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210700Animation02B54Bank1[6] = {
#include "assets/actor_210700_animation_02B54_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation02B54Bank4[51] = {
#include "assets/actor_210700_animation_02B54_bank4.inc"
};

static AnimationRecord _gActor210700Animation02B54Records[126] = {
#include "assets/actor_210700_animation_02B54_records.inc"
};

static u16 _gActor210700Animation02B54Indices[20] = {
#include "assets/actor_210700_animation_02B54_indices.inc"
};

AnimationSet gActor210700Animation02B54 = {
    _gActor210700Animation02B54Records,
    _gActor210700Animation02B54Indices,
    { NULL, _gActor210700Animation02B54Bank1, NULL, NULL, _gActor210700Animation02B54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210700Animation02E0CBank1[5] = {
#include "assets/actor_210700_animation_02E0C_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation02E0CBank4[50] = {
#include "assets/actor_210700_animation_02E0C_bank4.inc"
};

static AnimationRecord _gActor210700Animation02E0CRecords[89] = {
#include "assets/actor_210700_animation_02E0C_records.inc"
};

static u16 _gActor210700Animation02E0CIndices[20] = {
#include "assets/actor_210700_animation_02E0C_indices.inc"
};

AnimationSet gActor210700Animation02E0C = {
    _gActor210700Animation02E0CRecords,
    _gActor210700Animation02E0CIndices,
    { NULL, _gActor210700Animation02E0CBank1, NULL, NULL, _gActor210700Animation02E0CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210700Animation0318CBank1[8] = {
#include "assets/actor_210700_animation_0318C_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation0318CBank4[67] = {
#include "assets/actor_210700_animation_0318C_bank4.inc"
};

static AnimationRecord _gActor210700Animation0318CRecords[113] = {
#include "assets/actor_210700_animation_0318C_records.inc"
};

static u16 _gActor210700Animation0318CIndices[20] = {
#include "assets/actor_210700_animation_0318C_indices.inc"
};

AnimationSet gActor210700Animation0318C = {
    _gActor210700Animation0318CRecords,
    _gActor210700Animation0318CIndices,
    { NULL, _gActor210700Animation0318CBank1, NULL, NULL, _gActor210700Animation0318CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210700Animation03424Bank1[5] = {
#include "assets/actor_210700_animation_03424_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation03424Bank4[37] = {
#include "assets/actor_210700_animation_03424_bank4.inc"
};

static AnimationRecord _gActor210700Animation03424Records[94] = {
#include "assets/actor_210700_animation_03424_records.inc"
};

static u16 _gActor210700Animation03424Indices[20] = {
#include "assets/actor_210700_animation_03424_indices.inc"
};

AnimationSet gActor210700Animation03424 = {
    _gActor210700Animation03424Records,
    _gActor210700Animation03424Indices,
    { NULL, _gActor210700Animation03424Bank1, NULL, NULL, _gActor210700Animation03424Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210700Animation03BD0Bank1[15] = {
#include "assets/actor_210700_animation_03BD0_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation03BD0Bank4[170] = {
#include "assets/actor_210700_animation_03BD0_bank4.inc"
};

static AnimationRecord _gActor210700Animation03BD0Records[256] = {
#include "assets/actor_210700_animation_03BD0_records.inc"
};

static u16 _gActor210700Animation03BD0Indices[20] = {
#include "assets/actor_210700_animation_03BD0_indices.inc"
};

AnimationSet gActor210700Animation03BD0 = {
    _gActor210700Animation03BD0Records,
    _gActor210700Animation03BD0Indices,
    { NULL, _gActor210700Animation03BD0Bank1, NULL, NULL, _gActor210700Animation03BD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210700Animation067E8Bank1[116] = {
#include "assets/actor_210700_animation_067E8_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation067E8Bank4[1028] = {
#include "assets/actor_210700_animation_067E8_bank4.inc"
};

static AnimationRecord _gActor210700Animation067E8Records[1426] = {
#include "assets/actor_210700_animation_067E8_records.inc"
};

static u16 _gActor210700Animation067E8Indices[20] = {
#include "assets/actor_210700_animation_067E8_indices.inc"
};

AnimationSet gActor210700Animation067E8 = {
    _gActor210700Animation067E8Records,
    _gActor210700Animation067E8Indices,
    { NULL, _gActor210700Animation067E8Bank1, NULL, NULL, _gActor210700Animation067E8Bank4, NULL, NULL, NULL },
};

static TmdBone _gActor210700RupertBroderickBody1Skeleton[20] = {
#include "assets/rupert_broderick_body_1_skeleton.inc"
};

static u32 _gActor210700RupertBroderickBody1PartVerts[20] = {
#include "assets/rupert_broderick_body_1_partVerts.inc"
};

static SVECTOR _gActor210700RupertBroderickBody1Verts[386] = {
#include "assets/rupert_broderick_body_1_verts.inc"
};

static SVECTOR _gActor210700RupertBroderickBody1Normals[385] = {
#include "assets/rupert_broderick_body_1_normals.inc"
};

static u32 _gActor210700RupertBroderickBody1Stream[4327] = {
#include "assets/rupert_broderick_body_1_stream.inc"
};

static TmdSource _gActor210700RupertBroderickBody1 = {
    0,
    23980,
    6012,
    20,
    _gActor210700RupertBroderickBody1PartVerts,
    _gActor210700RupertBroderickBody1Verts,
    _gActor210700RupertBroderickBody1Normals,
    _gActor210700RupertBroderickBody1Skeleton,
    _gActor210700RupertBroderickBody1Stream,
};

static AnimationPackedPose _gActor210700Animation0CC20Bank1[9] = {
#include "assets/actor_210700_animation_0CC20_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation0CC20Bank4[112] = {
#include "assets/actor_210700_animation_0CC20_bank4.inc"
};

static AnimationRecord _gActor210700Animation0CC20Records[177] = {
#include "assets/actor_210700_animation_0CC20_records.inc"
};

static u16 _gActor210700Animation0CC20Indices[20] = {
#include "assets/actor_210700_animation_0CC20_indices.inc"
};

static AnimationSet _gActor210700Animation0CC20 = {
    _gActor210700Animation0CC20Records,
    _gActor210700Animation0CC20Indices,
    { NULL, _gActor210700Animation0CC20Bank1, NULL, NULL, _gActor210700Animation0CC20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210700Animation0CFE0Bank1[7] = {
#include "assets/actor_210700_animation_0CFE0_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation0CFE0Bank4[62] = {
#include "assets/actor_210700_animation_0CFE0_bank4.inc"
};

static AnimationRecord _gActor210700Animation0CFE0Records[137] = {
#include "assets/actor_210700_animation_0CFE0_records.inc"
};

static u16 _gActor210700Animation0CFE0Indices[20] = {
#include "assets/actor_210700_animation_0CFE0_indices.inc"
};

static AnimationSet _gActor210700Animation0CFE0 = {
    _gActor210700Animation0CFE0Records,
    _gActor210700Animation0CFE0Indices,
    { NULL, _gActor210700Animation0CFE0Bank1, NULL, NULL, _gActor210700Animation0CFE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210700Animation0D240Bank1[2] = {
#include "assets/actor_210700_animation_0D240_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation0D240Bank4[32] = {
#include "assets/actor_210700_animation_0D240_bank4.inc"
};

static AnimationRecord _gActor210700Animation0D240Records[94] = {
#include "assets/actor_210700_animation_0D240_records.inc"
};

static u16 _gActor210700Animation0D240Indices[20] = {
#include "assets/actor_210700_animation_0D240_indices.inc"
};

static AnimationSet _gActor210700Animation0D240 = {
    _gActor210700Animation0D240Records,
    _gActor210700Animation0D240Indices,
    { NULL, _gActor210700Animation0D240Bank1, NULL, NULL, _gActor210700Animation0D240Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210700Animation0D72CBank1[6] = {
#include "assets/actor_210700_animation_0D72C_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation0D72CBank4[104] = {
#include "assets/actor_210700_animation_0D72C_bank4.inc"
};

static AnimationRecord _gActor210700Animation0D72CRecords[173] = {
#include "assets/actor_210700_animation_0D72C_records.inc"
};

static u16 _gActor210700Animation0D72CIndices[20] = {
#include "assets/actor_210700_animation_0D72C_indices.inc"
};

static AnimationSet _gActor210700Animation0D72C = {
    _gActor210700Animation0D72CRecords,
    _gActor210700Animation0D72CIndices,
    { NULL, _gActor210700Animation0D72CBank1, NULL, NULL, _gActor210700Animation0D72CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210700Animation0DB7CBank1[6] = {
#include "assets/actor_210700_animation_0DB7C_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation0DB7CBank4[84] = {
#include "assets/actor_210700_animation_0DB7C_bank4.inc"
};

static AnimationRecord _gActor210700Animation0DB7CRecords[154] = {
#include "assets/actor_210700_animation_0DB7C_records.inc"
};

static u16 _gActor210700Animation0DB7CIndices[20] = {
#include "assets/actor_210700_animation_0DB7C_indices.inc"
};

static AnimationSet _gActor210700Animation0DB7C = {
    _gActor210700Animation0DB7CRecords,
    _gActor210700Animation0DB7CIndices,
    { NULL, _gActor210700Animation0DB7CBank1, NULL, NULL, _gActor210700Animation0DB7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210700Animation0DE04Bank1[4] = {
#include "assets/actor_210700_animation_0DE04_bank1.inc"
};

static AnimationPackedRotation _gActor210700Animation0DE04Bank4[52] = {
#include "assets/actor_210700_animation_0DE04_bank4.inc"
};

static AnimationRecord _gActor210700Animation0DE04Records[78] = {
#include "assets/actor_210700_animation_0DE04_records.inc"
};

static u16 _gActor210700Animation0DE04Indices[20] = {
#include "assets/actor_210700_animation_0DE04_indices.inc"
};

static AnimationSet _gActor210700Animation0DE04 = {
    _gActor210700Animation0DE04Records,
    _gActor210700Animation0DE04Indices,
    { NULL, _gActor210700Animation0DE04Bank1, NULL, NULL, _gActor210700Animation0DE04Bank4, NULL, NULL, NULL },
};

u_long D_actor_210700_80157C4C[192] = {
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x64838DA2,
    0x48555D4F,
    0x49494949,
    0x54554849,
    0x65657C5E,
    0x60737A7A,
    0x48485555,
    0x60545555,
    0x64795672,
    0xA2A28E76,
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x65838DA2,
    0x4948546F,
    0x45454545,
    0x54484949,
    0x7766705E,
    0x545F6A77,
    0x48484848,
    0x54554848,
    0x6479725F,
    0xA2A28262,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x65839AA1,
    0x4948545C,
    0x45454545,
    0x54484945,
    0x7777705E,
    0x555D7C8A,
    0x48484848,
    0x55484848,
    0x65687D53,
    0xA2A28276,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x64828DA1,
    0x49485451,
    0x45454549,
    0x60554949,
    0x6877686F,
    0x545F7B77,
    0x48484848,
    0x54554848,
    0x777A705F,
    0xA2A29A83,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x789A99A1,
    0x605F706A,
    0x55485554,
    0x72605455,
    0x6A86666A,
    0x5E577787,
    0x54545454,
    0x6F605454,
    0x8F8F786A,
    0xA2A2998E,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x96A6A3A1,
    0x9B639098,
    0x7C7C9489,
    0x776A7C7C,
    0x85829388,
    0x66878E8F,
    0x7A7A6A67,
    0xA38E898A,
    0x98A5A0A6,
    0xA2A2A6A9,
    0xFFFFFFFF,
    0xB5FFFFFF,
    0xA096A6A3,
    0x989F9F98,
    0xAB919090,
    0x99A36363,
    0x77878F99,
    0x99999A87,
    0x9191A6A1,
    0xAD9F9890,
    0x9FADADAD,
    0xA2A7969E,
    0xFFFFFFFF,
    0x61FFFFFF,
    0xA2A3A3A2,
    0x96A7A3A2,
    0x61619F98,
    0x9F616161,
    0x77788FA1,
    0x9E998778,
    0x6161AEAE,
    0xA99EA59F,
    0xA1A1A7A6,
    0xA2A1A1A1,
    0xFFFFFFFF,
    0x9FFFFFFF,
    0xA6A2A28D,
    0xADA5ADA5,
    0xB1B1B1AE,
    0xAEAEADAE,
    0x65929AA9,
    0xADA68766,
    0xADA5A5AD,
    0xADADADAD,
    0x8B969EA5,
    0xA2A2A2A2,
    0xFFFFFFFF,
    0xA5FFFFFF,
    0xA5958C8D,
    0xFFB9C29C,
    0xAE61B5FF,
    0xA9A9A9A5,
    0x7C788F95,
    0xA7958785,
    0x9FA5A68B,
    0xFFFFB561,
    0x9890C5BF,
    0x8C8C9795,
    0xFFFFFFFF,
    0xABFFFFFF,
    0x9B8C828D,
    0xFFB21716,
    0xCECCFFFF,
    0x9595A890,
    0x7B65864E,
    0x998D8665,
    0xCEACA997,
    0xB7FFFFC4,
    0x9BCCD5D0,
    0x87868281,
    0xFFFFFFFF,
    0x9BB6FFFF,
    0x828F828D,
    0x9894028A,
    0x1CCB0B1,
    0x81958B9B,
    0x72677882,
    0x998E8568,
    0xCC9B95A2,
    0x98AE7EC6,
    0x87891601,
    0x8A6A6686,
    0xFFFFFFFF,
    0x8F98FFFF,
    0x787A788E,
    0x8F868282,
    0x9388938E,
    0x9A999A8E,
    0x6F706488,
    0x8E86646C,
    0x9A9A8E8E,
    0x87879A9B,
    0x66788687,
    0x7A735E73,
    0xFFFFFFFF,
    0x8691FFFF,
    0x4F746A86,
    0x72576870,
    0x6A705E74,
    0x87868A8A,
    0x7D736777,
    0x87787072,
    0x8A898987,
    0x6C6C707A,
    0x605E6C57,
    0x6A605454,
    0xFFFFFFFF,
    0x7763FFFF,
    0x535F6A77,
    0x55555460,
    0x54554848,
    0x776A7353,
    0x5D7D577A,
    0x788A705F,
    0x72707C7A,
    0x54545460,
    0x55555454,
    0x7A735455,
    0xFFFFFFFF,
    0x6691FFFF,
    0x60737A77,
    0x45454854,
    0x55484945,
    0x7C735354,
    0x545F577A,
    0x7A7A7354,
    0x5453737C,
    0x48494848,
    0x55554848,
    0x777A7254,
};

GpuImageUpload D_actor_210700_80157F4C[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 24, 16 }, D_actor_210700_80157C4C },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_210700_80157F6C[192] = {
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x64838DA2,
    0x48555D4F,
    0x49494949,
    0x54554849,
    0x65657C5E,
    0x60737A7A,
    0x48485555,
    0x60545555,
    0x64795672,
    0xA2A28E76,
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x65838DA2,
    0x4948546F,
    0x45454545,
    0x54484949,
    0x7766705E,
    0x545F6A77,
    0x48484848,
    0x54554848,
    0x6479725F,
    0xA2A28262,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x65839AA1,
    0x4948545C,
    0x45454545,
    0x54484945,
    0x7777705E,
    0x555D7C8A,
    0x48484848,
    0x55484848,
    0x65687D53,
    0xA2A28276,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x64828DA1,
    0x49485451,
    0x45454549,
    0x60554949,
    0x6877686F,
    0x545F7B77,
    0x48484848,
    0x54554848,
    0x777A705F,
    0xA2A29A83,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x789A99A1,
    0x605F706A,
    0x55485554,
    0x72605455,
    0x6A86666A,
    0x5E577787,
    0x54545454,
    0x6F605454,
    0x8F8F786A,
    0xA2A2998E,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x96A6A3A1,
    0x9B639098,
    0x7C7C9489,
    0x776A7C7C,
    0x85829388,
    0x66878E8F,
    0x7A7A6A67,
    0xA38E898A,
    0x98A5A0A6,
    0xA2A2A6A9,
    0xFFFFFFFF,
    0xB5FFFFFF,
    0xA096A6A3,
    0x989F9F98,
    0xAB919090,
    0x99A36363,
    0x77878F99,
    0x99999A87,
    0x9191A6A1,
    0xAD9F9890,
    0x9FADADAD,
    0xA2A7969E,
    0xFFFFFFFF,
    0x61FFFFFF,
    0xA2A3A3A2,
    0x96A7A3A2,
    0x61619F98,
    0x9F616161,
    0x77788FA1,
    0x9E998778,
    0x6161AEAE,
    0xA99EA59F,
    0xA1A1A7A6,
    0xA2A1A1A1,
    0xFFFFFFFF,
    0x9FFFFFFF,
    0xA2A2A28D,
    0x8C8C8C8C,
    0xA9A9A68C,
    0xAEAEA9A9,
    0x65929AA9,
    0xADA68766,
    0xA9A9A5AD,
    0x9595A9A9,
    0x95959595,
    0xA2A2A2A2,
    0xFFFFFFFF,
    0xA5FFFFFF,
    0x95958C8D,
    0xB0C2A995,
    0xC2C2B0B0,
    0xA9A9A9A9,
    0x7C788F95,
    0xA7958785,
    0xA9A9A68B,
    0xC2B0B0C2,
    0x959595C2,
    0x8C8C9795,
    0xFFFFFFFF,
    0xABFFFFFF,
    0xC28C828D,
    0xB0B0B0B0,
    0xB0B0B0B0,
    0x9595A8B0,
    0x7B65864E,
    0x998D8665,
    0xB0B0A997,
    0xB0B0B0B0,
    0x9BC2B0B0,
    0x87868281,
    0xFFFFFFFF,
    0x9BB6FFFF,
    0x828F828D,
    0x9894028A,
    0x1CCB0B1,
    0x81958B9B,
    0x72677882,
    0x998E8568,
    0xCC9B95A2,
    0x98AE7EC6,
    0x87891601,
    0x8A6A6686,
    0xFFFFFFFF,
    0x8F98FFFF,
    0x787A788E,
    0x8F868282,
    0x9388938E,
    0x9A999A8E,
    0x6F706488,
    0x8E86646C,
    0x9A9A8E8E,
    0x879A9A9A,
    0x66788687,
    0x7A735E73,
    0xFFFFFFFF,
    0x8691FFFF,
    0x4F746A86,
    0x72576870,
    0x6A705E74,
    0x87868A8A,
    0x7D736777,
    0x87787072,
    0x8A898987,
    0x6C6C707A,
    0x605E6C57,
    0x6A605454,
    0xFFFFFFFF,
    0x7763FFFF,
    0x535F6A77,
    0x55555460,
    0x54554848,
    0x776A7353,
    0x5D7D577A,
    0x788A705F,
    0x72707C7A,
    0x54545460,
    0x55555454,
    0x7A735455,
    0xFFFFFFFF,
    0x6691FFFF,
    0x60737A77,
    0x45454854,
    0x55484945,
    0x7C735354,
    0x545F577A,
    0x7A7A7354,
    0x5453737C,
    0x48494848,
    0x55554848,
    0x777A7254,
};

GpuImageUpload D_actor_210700_8015826C[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 24, 16 }, D_actor_210700_80157F6C },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_210700_8015828C[192] = {
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x64838DA2,
    0x48555D4F,
    0x49494949,
    0x54554849,
    0x65657C5E,
    0x60737A7A,
    0x48485555,
    0x60545555,
    0x64795672,
    0xA2A28E76,
    0xFFFFFFFF,
    0xB1FFFFFF,
    0x65838DA2,
    0x4948546F,
    0x45454545,
    0x54484949,
    0x7766705E,
    0x545F6A77,
    0x48484848,
    0x54554848,
    0x6479725F,
    0xA2A28262,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x65839AA1,
    0x4948545C,
    0x45454545,
    0x54484945,
    0x7777705E,
    0x555D7C8A,
    0x48484848,
    0x55484848,
    0x65687D53,
    0xA2A28276,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x64828DA1,
    0x49485451,
    0x45454549,
    0x60554949,
    0x6877686F,
    0x545F7B77,
    0x48484848,
    0x54554848,
    0x777A705F,
    0xA2A29A83,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x789A99A1,
    0x605F706A,
    0x55485554,
    0x72605455,
    0x6A86666A,
    0x5E577787,
    0x54545454,
    0x6F605454,
    0x8F8F786A,
    0xA2A2998E,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x96A6A3A1,
    0x9B639098,
    0x7C7C9489,
    0x776A7C7C,
    0x85829388,
    0x66878E8F,
    0x7A7A6A67,
    0xA38E898A,
    0x98A5A0A6,
    0xA2A2A6A9,
    0xFFFFFFFF,
    0xB5FFFFFF,
    0xA096A6A3,
    0x989F9F98,
    0xAB919090,
    0x99A36363,
    0x77878F99,
    0x99999A87,
    0x9191A6A1,
    0xAD9F9890,
    0x9FADADAD,
    0xA2A7969E,
    0xFFFFFFFF,
    0x61FFFFFF,
    0xA2A3A3A2,
    0x96A7A3A2,
    0x61619F98,
    0x9F616161,
    0x77788FA1,
    0x9E998778,
    0x6161AEAE,
    0xA99EA59F,
    0xA1A1A7A6,
    0xA2A1A1A1,
    0xFFFFFFFF,
    0x9FFFFFFF,
    0xA2A2A28D,
    0xA2A2A2A2,
    0xB19898A2,
    0xAEAEADAE,
    0x65929AA9,
    0xADA68766,
    0xADA5A5AD,
    0x8B8B9898,
    0xA2A2A2A2,
    0xA2A2A2A2,
    0xFFFFFFFF,
    0xA5FFFFFF,
    0xA2958C8D,
    0x788FA2A2,
    0xA2A28F8F,
    0xA9A9A9A6,
    0x7C788F95,
    0xA7958785,
    0xA2A2A7A7,
    0x8F788F8F,
    0x959595A2,
    0x8C8C9795,
    0xFFFFFFFF,
    0xABFFFFFF,
    0x958C828D,
    0x61ADADA9,
    0xAD616161,
    0xA9A99898,
    0x7B65864E,
    0x998D8665,
    0x61AD9897,
    0x61616161,
    0x95A9ADAD,
    0x87868281,
    0xFFFFFFFF,
    0x9BB6FFFF,
    0x828F828D,
    0x9898ADAD,
    0x98A9A9A9,
    0x818198AD,
    0x72677882,
    0x998E8568,
    0x98AD9898,
    0x98A9A9A9,
    0x87ADADAD,
    0x8A6A6686,
    0xFFFFFFFF,
    0x8F98FFFF,
    0x787A788E,
    0x8F868282,
    0x9388938E,
    0x9A999A8E,
    0x6F706488,
    0x8E86646C,
    0x9A9A8E8E,
    0x87879A9B,
    0x66788687,
    0x7A735E73,
    0xFFFFFFFF,
    0x8691FFFF,
    0x4F746A86,
    0x72576870,
    0x6A705E74,
    0x87868A8A,
    0x7D736777,
    0x87787072,
    0x8A898987,
    0x6C6C707A,
    0x605E6C57,
    0x6A605454,
    0xFFFFFFFF,
    0x7763FFFF,
    0x535F6A77,
    0x55555460,
    0x54554848,
    0x776A7353,
    0x5D7D577A,
    0x788A705F,
    0x72707C7A,
    0x54545460,
    0x55555454,
    0x7A735455,
    0xFFFFFFFF,
    0x6691FFFF,
    0x60737A77,
    0x45454854,
    0x55484945,
    0x7C735354,
    0x545F577A,
    0x7A7A7354,
    0x5453737C,
    0x48494848,
    0x55554848,
    0x777A7254,
};

GpuImageUpload D_actor_210700_8015858C[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 24, 16 }, D_actor_210700_8015828C },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

AnimationSet* D_actor_210700_801585AC[7] = {
    NULL,
    &_gActor210700Animation0CC20,
    &_gActor210700Animation0CFE0,
    &_gActor210700Animation0D240,
    &_gActor210700Animation0D72C,
    &_gActor210700Animation0DB7C,
    &_gActor210700Animation0DE04,
};

AnimationSet** D_actor_210700_801585C8[1] = {
    D_actor_210700_801585AC,
};

TaskDesc D_actor_210700_801585CC = { { { TASK_BODY_TMD, 192 } }, func_actor_210700_80149F38, { .model = &_gActor210700RupertBroderickBody1 } };

TaskMessageEntry D_actor_210700_801585D8[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_210700_8014A224 },
    { ACTOR_MESSAGE_PLACE, func_actor_210700_8014A344 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_210700_8014A3D4 },
    { 2016, func_actor_210700_8014A4B0 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_actor_210700_80149E30(Task* arg0);

/// Blink state of the actor, run by the tick state: runs
/// `_Actor210700Work::blinkCountdown` down one a frame while `blinkStep` names
/// the eye image due next, and on the frame it goes below zero posts that
/// image over the 0x18x0x10 eye rect at (0, 0x28) -- restarting the countdown
/// from `blinkFrameDelay` and advancing `blinkStep` after the closed and
/// half-open eyes, or clearing `blinkStep` after the open ones, which ends the
/// blink.
static void func_actor_210700_80149E30(Task* arg0)
{
    _Actor210700Work* work;
    RECT              rect;

    work   = arg0->work;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (work->blinkStep) {
        case ACTOR_210700_BLINK_CLOSED:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                Gp_LoadActorImage(arg0, &D_actor_210700_8015858C[0], &rect);
                work->blinkCountdown = work->blinkFrameDelay;
                work->blinkStep      = work->blinkStep + 1;
            }
            break;
        case ACTOR_210700_BLINK_HALF:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                Gp_LoadActorImage(arg0, &D_actor_210700_8015826C[0], &rect);
                work->blinkCountdown = work->blinkFrameDelay;
                work->blinkStep      = work->blinkStep + 1;
            }
            break;
        case ACTOR_210700_BLINK_OPEN:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                Gp_LoadActorImage(arg0, &D_actor_210700_80157F4C[0], &rect);
                work->blinkStep = ACTOR_210700_BLINK_NONE;
            }
            break;
    }
}

/// The actor's task entry: runs the handler for the task's current state out
/// of `D_actor_210700_80149E24` - spawn, per-frame tick or teardown - copying
/// the table onto the stack before the call.
void func_actor_210700_80149F38(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_210700_80149E24;
    sp.funcs[task->state](task);
}

/// Spawn state: allocates the zeroed work block into `Task::work` (handing
/// the task to `enemyTaskExit` if that fails), marks no bank bound, no clip
/// applied and no buffer free pending, hides the model, then runs its own
/// place and play-animation handlers directly to place the actor at the
/// origin - which shows it again - and start clip 1 of bank 0. It draws the
/// ground shadow under the model's second part, points the model at the work
/// block's light / colour matrices, installs the message table and the exit
/// callback, and advances to the tick state.
static void func_actor_210700_80149F90(Task* task)
{
    _Actor210700Work*    work;
    TmdObject*           extra;
    ActorTransform       args;
    AnimationPlayRequest anim;
    VECTOR3              pos;

    extra = task->extra.tmd;
    work  = memCalloc(sizeof(_Actor210700Work), 0);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work          = work;
    work->animId        = ACTOR_MODEL_STATE_NONE;
    work->bank          = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown = -1;
    extra->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    args.pos.vx         = 0;
    args.pos.vy         = 0;
    args.pos.vz         = 0;
    args.rot.vx         = 0;
    args.rot.vy         = 0;
    args.rot.vz         = 0;
    func_actor_210700_8014A344(task, ACTOR_MESSAGE_PLACE, &args, 0);
    anim.source.index = 0;
    anim.animationId  = 1;
    anim.blend        = ANIMATION_BLEND_RESET;
    func_actor_210700_8014A224(task, ACTOR_MESSAGE_PLAY_ANIMATION, &anim, 0);
    if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x400, gRoomEffectState->groundShadowShade);
    }
    func_actor_210700_8014A208(task);
    task->msgTable     = D_actor_210700_801585D8;
    task->exitCallback = func_actor_210700_8014A1E8;
    task->state++;
}

/// Tick state: while an animation is running ticks slots 1..0x13 and draws
/// the ground shadow under the model's second part. While the game session's
/// view is ready it invalidates and rebuilds that part's coordinate and hands
/// it to `func_800D7A9C`. It then runs the blink and counts `freeCountdown`
/// down, freeing the model buffers on the tick that finds it at 0.
static void func_actor_210700_8014A0AC(Task* task)
{
    _Actor210700Work* work;
    TmdObject*        ext;
    VECTOR3           pos;
    s32               i;

    work = task->work;
    ext  = task->extra.tmd;
    if (work->ticking != 0) {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x400, gRoomEffectState->groundShadowShade);
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&task->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    func_actor_210700_80149E30(task);
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->freeCountdown--;
    }
}

/// The actor's teardown state and `Task::exitCallback`: hands the task to
/// `enemyTaskExit`.
static void func_actor_210700_8014A1E8(Task* task)
{
    enemyTaskExit(task);
}

/// Points the model at the work block's light and colour matrices, so the
/// actor is lit from its own block rather than the defaults.
static void func_actor_210700_8014A208(Task* arg0)
{
    TmdObject*        ext;
    _Actor210700Work* work;

    work          = arg0->work;
    ext           = arg0->extra.tmd;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Play-animation handler: starts a clip. A bank index different from the
/// one the rig is bound to binds the rig to that entry of
/// `D_actor_210700_801585C8` and forgets the applied clip; a clip different
/// from the applied one then seeds slots 1..0x13 with it - blending over six
/// frames when the request's `blend` is set, whatever its `blendFrames`, and
/// from the clip's start otherwise - ticks them once and enables the per-frame
/// tick. Always returns 0.
s32 func_actor_210700_8014A224(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    _Actor210700Work* work;
    s32               i;
    TmdObject*        ext;

    work = task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->bank) {
        work->bank   = msg->source.index;
        work->animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_210700_801585C8[work->bank], ext, work->rig.poses,
                             work->rig.slots);
    }
    if (msg->animationId != work->animId) {
        work->animId = msg->animationId;
        if (msg->blend != ANIMATION_BLEND_RESET) {
            for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->animId, 0, 6);
            }
        } else {
            for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
                animationResetSlot(&work->rig.anim, i, work->animId);
            }
        }
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
        work->ticking = 1;
    }
    return 0;
}

/// Message-0x7D4 handler: places the actor. Writes the payload's translation
/// into the root coordinate's local matrix and its Euler angles into the
/// coordinate's `rot` slot, rebuilds the rotation from them, clears `composeStamp` so
/// the world matrix is recomputed, and clears `TmdObject::flags` bit 0x80 to
/// show the model. Always returns 0.
s32 func_actor_210700_8014A344(Task* task, s32 arg1, ActorTransform* args, s32 arg3)
{
    GfxCoord*  coord;
    TmdObject* extra;

    extra               = task->extra.tmd;
    coord               = extra->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags       &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return 0;
}

/// Message-0x7D5 handler: sets the model's visibility and mode bit from the
/// message's mode word. `TmdObject::flags` bit 0x80 hides the model and bit
/// 0x4 is the one modes 2 and 3 raise. Mode 0 hides the model and drops 0x4,
/// 1 shows it, reallocates its buffers through `Tmd_AllocBuffers` and drops
/// 0x4, 2 hides it, raises 0x4 and starts the work block's `freeCountdown`
/// at two ticks to freeing the buffers, and 3 shows it and raises 0x4. Handled
/// modes return 0; anything else returns 1 and changes nothing.
s32 func_actor_210700_8014A3D4(Task* arg0, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*        obj;
    _Actor210700Work* work;
    s32               ret;

    obj  = arg0->extra.tmd;
    work = arg0->work;
    ret  = 0;

    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = mode;
            obj->flags         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message-0x7E0 handler: posts one of the actor's three eye images over the
/// 0x18x0x10 eye rect at (0, 0x28) -- the closed eyes
/// `D_actor_210700_8015858C[0]` for mode 1, the open eyes
/// `D_actor_210700_80157F4C[0]` for modes 0 and 2, and the half-open eyes
/// `D_actor_210700_8015826C[0]` for mode 3, which also starts a blink,
/// `blinkStep` at the closed eyes and `blinkFrameDelay` at 1. Any other mode
/// leaves the image NULL and returns 0.
/// The mode-1 case is written first because the compiler lays the case bodies
/// out in source order and that is the order the retail image has them in.
s32 func_actor_210700_8014A4B0(Task* arg0, s32 arg1, s32 mode, s32 arg3)
{
    RECT            rect;
    GpuImageUpload* uploadList;
    s32             ret;

    ret    = 0;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (mode) {
        case 1:
            uploadList = &D_actor_210700_8015858C[0];
            break;
        case 0:
        case 2:
            uploadList = &D_actor_210700_80157F4C[0];
            break;
        case 3:
            ((_Actor210700Work*)arg0->work)->blinkStep       = ACTOR_210700_BLINK_CLOSED;
            ((_Actor210700Work*)arg0->work)->blinkFrameDelay = 1;
            uploadList                                       = &D_actor_210700_8015826C[0];
            break;
        default:
            uploadList = NULL;
            break;
    }

    if (uploadList != NULL) {
        ret = Gp_LoadActorImage(arg0, uploadList, &rect);
    }
    return ret;
}
