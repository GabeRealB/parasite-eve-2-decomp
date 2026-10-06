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

/// Eye-texture command accepted by this actor's message table.
enum { ACTOR_210700_MESSAGE_SET_EYES = 0x7E0 };

/// Ground-shadow half extent in world-coordinate units.
enum { ACTOR_210700_GROUND_SHADOW_HALF_SIZE = 0x400 };

/// No primitive-buffer release is pending in the actor's signed tick counter.
enum { ACTOR_210700_BUFFER_FREE_NONE = -1 };

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
/// `_actor210700PlayAnimation`, 0x7D4 `_actor210700Place`, 0x7D5
/// `_actor210700SetModelDraw`, 0x7E0 `_actor210700SetEyes`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_210700_801585D8[];

static void _actor210700UpdateBlink(Task* task);
static void _actor210700Task(Task* task);
static void _actor210700Init(Task* task);
static void _actor210700Update(Task* task);
static void _actor210700Exit(Task* task);
static void _actor210700BindLighting(Task* task);
static s32  _actor210700PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg);
static s32  _actor210700Place(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedSecondArg);
static s32  _actor210700SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedSecondArg);
static s32  _actor210700SetEyes(Task* task, s32 messageId, s32 eyeMode, s32 unusedSecondArg);

/// The actor's three task states - spawn, tick and teardown - which
/// `_actor210700Task` runs by `Task::state`.
static const TaskFuncTable3 D_actor_210700_80149E24 = { {
    _actor210700Init,
    _actor210700Update,
    _actor210700Exit,
} };

static AnimationSet _gActor210700Animation0CC20;
static AnimationSet _gActor210700Animation0CFE0;
static AnimationSet _gActor210700Animation0D240;
static AnimationSet _gActor210700Animation0D72C;
static AnimationSet _gActor210700Animation0DB7C;
static AnimationSet _gActor210700Animation0DE04;
static TmdSource    _gActor210700RupertBroderickBody1;

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

TaskDesc D_actor_210700_801585CC = { { { TASK_BODY_TMD, 192 } }, _actor210700Task, { .model = &_gActor210700RupertBroderickBody1 } };

TaskMessageEntry D_actor_210700_801585D8[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor210700PlayAnimation },
    { ACTOR_MESSAGE_PLACE, _actor210700Place },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor210700SetModelDraw },
    { ACTOR_210700_MESSAGE_SET_EYES, _actor210700SetEyes },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Model-relative eye-texture location: Y/height count rows, width counts VRAM words.
enum {
    ACTOR_210700_EYE_TEXTURE_Y_ROWS      = 40,
    ACTOR_210700_EYE_TEXTURE_WIDTH_WORDS = 24,
    ACTOR_210700_EYE_TEXTURE_HEIGHT_ROWS = 16,
};

/// Fills the model-relative rectangle replaced by an eye-texture upload.
///
/// Expands to four statements. Invoke as a standalone statement in a braced
/// block, never as an unbraced conditional or loop body.
/// Requires a stable writable RECT lvalue without evaluation side effects,
/// evaluated four times. X counts two positions per VRAM word. No caller
/// locals are captured, and no address is retained.
#define ACTOR_210700_INIT_EYE_TEXTURE_RECT(eyeRect)     \
    (eyeRect).x = 0;                                    \
    (eyeRect).y = ACTOR_210700_EYE_TEXTURE_Y_ROWS;      \
    (eyeRect).w = ACTOR_210700_EYE_TEXTURE_WIDTH_WORDS; \
    (eyeRect).h = ACTOR_210700_EYE_TEXTURE_HEIGHT_ROWS

/// Projects model part 1 onto the ground and draws its shadow when a floor is found.
///
/// Requires a side-effect-free live TMD task pointer with part 1's cached
/// transform, and a stable writable VECTOR3 lvalue. The task is evaluated once;
/// the point once on a miss and twice on a hit. Uses the current global room
/// shadow shade; neither argument's address is retained.
#define ACTOR_210700_DRAW_GROUND_SHADOW(task, groundPoint)                                                                     \
    do {                                                                                                                       \
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&(task)->extra.tmd->coords[1].workm), &(groundPoint)) != 0) {        \
            effectDrawGroundShadow(&(groundPoint), ACTOR_210700_GROUND_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade); \
        }                                                                                                                      \
    } while (0)

/// Advances all animated body parts once, retaining the model root's placement.
///
/// Requires the work rig to be bound and slots 1..19 initialized. Playback
/// retains its borrowed model, bank and clip data; slot zero is untouched.
static inline void _actor210700TickAnimationSlots(_Actor210700Work* work)
{
    s32 slotIndex;

    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
}

/// Advances the pending blink through closed, half-open and open eye textures.
///
/// Requires the actor's live work block and TMD body. Each active step decrements
/// the signed 16-bit countdown, uploading when its stored value becomes negative.
/// Closed and half-open eyes last `blinkFrameDelay + 1` ticks; the open step ends
/// the blink without resetting the countdown. Texture pixels remain borrowed by
/// the GPU queue after each upload.
static void _actor210700UpdateBlink(Task* task)
{
    _Actor210700Work* work;
    RECT              eyeRect;

    work = task->work;
    ACTOR_210700_INIT_EYE_TEXTURE_RECT(eyeRect);

    switch (work->blinkStep) {
        case ACTOR_210700_BLINK_CLOSED:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                actorRenderUploadTexture(task, &D_actor_210700_8015858C[0], &eyeRect);
                work->blinkCountdown = work->blinkFrameDelay;
                work->blinkStep      = work->blinkStep + 1;
            }
            break;
        case ACTOR_210700_BLINK_HALF:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                actorRenderUploadTexture(task, &D_actor_210700_8015826C[0], &eyeRect);
                work->blinkCountdown = work->blinkFrameDelay;
                work->blinkStep      = work->blinkStep + 1;
            }
            break;
        case ACTOR_210700_BLINK_OPEN:
            work->blinkCountdown = work->blinkCountdown - 1;
            if (work->blinkCountdown < 0) {
                actorRenderUploadTexture(task, &D_actor_210700_80157F4C[0], &eyeRect);
                work->blinkStep = ACTOR_210700_BLINK_NONE;
            }
            break;
    }
}

/// Dispatches Rupert Broderick's body task to initialization, update or teardown.
///
/// `task->state` must be 0 (initialize), 1 (update) or 2 (exit). The task owns a
/// twenty-part TMD body and carries its scene-owned `Enemy` in `spawnArg2`.
/// State zero allocates the work block; exit may release the task's resources.
static void _actor210700Task(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_210700_80149E24;
    stateHandlers.funcs[task->state](task);
}

/// Initializes the body at the origin with its default animation and message handlers.
///
/// Requires a live twenty-part TMD task with a scene-owned `Enemy` in `spawnArg2`.
/// Owns a zeroed primary-heap work block until task teardown; allocation failure
/// starts teardown immediately. Successful initialization advances to state 1.
static void _actor210700Init(Task* task)
{
    enum {
        ACTOR_210700_INITIAL_BANK      = 0,
        ACTOR_210700_INITIAL_ANIMATION = 1,
    };

    _Actor210700Work*    work;
    TmdObject*           model;
    ActorTransform       initialPlacement;
    AnimationPlayRequest initialAnimation;
    VECTOR3              groundPoint;

    model = task->extra.tmd;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work          = work;
    work->animId        = ACTOR_MODEL_STATE_NONE;
    work->bank          = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown = ACTOR_210700_BUFFER_FREE_NONE;
    model->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;

    // Placement enables drawing; seed only the request components the handlers read.
    initialPlacement.pos.vx = 0;
    initialPlacement.pos.vy = 0;
    initialPlacement.pos.vz = 0;
    initialPlacement.rot.vx = 0;
    initialPlacement.rot.vy = 0;
    initialPlacement.rot.vz = 0;
    _actor210700Place(task, ACTOR_MESSAGE_PLACE, &initialPlacement, 0);
    initialAnimation.source.index = ACTOR_210700_INITIAL_BANK;
    initialAnimation.animationId  = ACTOR_210700_INITIAL_ANIMATION;
    initialAnimation.blend        = ANIMATION_BLEND_RESET;
    _actor210700PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &initialAnimation, 0);
    ACTOR_210700_DRAW_GROUND_SHADOW(task, groundPoint);
    _actor210700BindLighting(task);
    task->msgTable     = D_actor_210700_801585D8;
    task->exitCallback = _actor210700Exit;
    task->state++;
}

/// Updates body animation, ground shadow, room lighting, blinking and buffer release.
///
/// Requires successful initialization. Advances animation slots 1..19 once per
/// call when enabled. Lighting is refreshed only with a ready view. A pending
/// buffer release occurs on the call entering with countdown zero, after which
/// the counter becomes the inactive sentinel.
static void _actor210700Update(Task* task)
{
    _Actor210700Work* work;
    TmdObject*        model;
    VECTOR3           groundPoint;

    work  = task->work;
    model = task->extra.tmd;
    if (work->ticking != 0) {
        _actor210700TickAnimationSlots(work);
    }
    ACTOR_210700_DRAW_GROUND_SHADOW(task, groundPoint);
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(model, task->extra.tmd->coords[1].workm.t, 0, ARRAY_SIZE(work->light.m));
    }
    _actor210700UpdateBlink(task);

    // Release only when the countdown was already zero at entry.
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Releases the actor's target-tracking object and starts task teardown.
///
/// Used by task state 2 and the exit callback. Requires the live `Enemy` in
/// `spawnArg2`; task teardown releases the work block and schedules body release.
static void _actor210700Exit(Task* task)
{
    enemyTaskExit(task);
}

/// Binds the model to the actor's private light-direction and light-colour matrices.
///
/// Requires initialized work and a live TMD body. Both matrix pointers borrow
/// the work block and remain valid until task teardown releases it.
static void _actor210700BindLighting(Task* task)
{
    TmdObject*        model;
    _Actor210700Work* work;

    work            = task->work;
    model           = task->extra.tmd;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
}

/// Selects a body animation, blending changed clips over six normal-rate frames.
///
/// Requires initialized work, the twenty-part model and a request readable during
/// dispatch. `source.index` must be 0 and `animationId` must select a loaded clip
/// 1..6. Drives slots 1..19, retaining the root placement. A changed bank rebinds
/// playback and forces clip selection; the same bank and clip do not restart.
/// Nonzero `blend` uses six frames, ignoring `blendFrames`; zero resets each slot.
/// Blending requires previously initialized slots; the first selection from
/// zeroed work must reset them.
/// The request is not retained, but animation data remain borrowed during playback.
/// The message ID, second argument and `enableWorldCollision` are ignored. Returns 0.
static s32 _actor210700PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    enum { ACTOR_210700_ANIMATION_BLEND_FRAMES = 6 };

    _Actor210700Work* work;
    s32               slotIndex;
    TmdObject*        model;

    work  = task->work;
    model = task->extra.tmd;
    if (request->source.index != work->bank) {
        work->bank   = request->source.index;
        work->animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_210700_801585C8[work->bank], model, work->rig.poses,
                             work->rig.slots);
    }
    if (request->animationId != work->animId) {
        work->animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, ACTOR_210700_ANIMATION_BLEND_FRAMES);
            }
        } else {
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationResetSlot(&work->rig.anim, slotIndex, work->animId);
            }
        }
        // Apply the first pose immediately before enabling subsequent frame updates.
        _actor210700TickAnimationSlots(work);
        work->ticking = 1;
    }
    return 0;
}

/// Places the model root, records its Euler angles and enables active drawing.
///
/// Requires a live TMD body and a placement readable during dispatch. Position
/// uses the root parent's coordinate frame; angles use 4096 units per turn in
/// SDK `RotMatrix` order. Only X/Y/Z components are read, and no payload pointer
/// is retained. Invalidates the composed transform without allocating a buffer.
/// The message ID and second argument are ignored. Returns 0.
static s32 _actor210700Place(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedSecondArg)
{
    GfxCoord*  rootCoord;
    TmdObject* model;

    model                   = task->extra.tmd;
    rootCoord               = model->coords;
    rootCoord->coord.t[0]   = placement->pos.vx;
    rootCoord->coord.t[1]   = placement->pos.vy;
    rootCoord->coord.t[2]   = placement->pos.vz;
    rootCoord->param.rot.vx = placement->rot.vx;
    rootCoord->param.rot.vy = placement->rot.vy;
    rootCoord->param.rot.vz = placement->rot.vz;
    RotMatrix(&rootCoord->param.rot, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->flags           &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return 0;
}

/// Sets active visibility and automatic-buffer policy, optionally scheduling release.
///
/// Requires initialized work and a live TMD body. Modes: 0 hide with automatic
/// allocation enabled; 1 show, explicitly allocate if absent and enable automatic
/// allocation; 2 hide, disable automatic allocation and release after two
/// intervening update calls; 3 show with automatic allocation disabled. Other
/// model flags and any already pending release are retained in modes 0, 1 and 3.
/// Allocation failure is ignored. The message ID and second argument are ignored.
/// Returns 0 for modes 0..3, or 1 without changes for other values.
static s32 _actor210700SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedSecondArg)
{
    enum {
        ACTOR_210700_MODEL_DRAW_HIDE_AUTO     = 0,
        ACTOR_210700_MODEL_DRAW_SHOW_ALLOCATE = 1,
        ACTOR_210700_MODEL_DRAW_HIDE_RELEASE  = 2,
        ACTOR_210700_MODEL_DRAW_SHOW_KEEP     = 3,
    };

    TmdObject*        model;
    _Actor210700Work* work;
    s32               invalidMode;

    model       = task->extra.tmd;
    work        = task->work;
    invalidMode = 0;

    switch (drawMode) {
        case ACTOR_210700_MODEL_DRAW_HIDE_AUTO:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_210700_MODEL_DRAW_SHOW_ALLOCATE:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_210700_MODEL_DRAW_HIDE_RELEASE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            // This mode value also supplies the two-tick release countdown.
            work->freeCountdown = drawMode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_210700_MODEL_DRAW_SHOW_KEEP:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            invalidMode = 1;
            break;
    }
    return invalidMode;
}

/// Selects an eye texture or starts the half-open/closed/half-open/open blink.
///
/// Requires initialized work and a live TMD body. Modes 0 and 2 upload open eyes,
/// 1 uploads closed eyes, and 3 uploads half-open eyes and starts a blink with
/// two ticks each for the subsequent closed and half-open images. The existing
/// countdown is retained; immediate image commands do not cancel a pending blink.
/// The message ID and second argument are ignored. Returns the texture uploader's
/// result for a handled mode, or 0 without an upload for other values. Static
/// texture pixels remain live while the GPU queue borrows them.
static s32 _actor210700SetEyes(Task* task, s32 messageId, s32 eyeMode, s32 unusedSecondArg)
{
    enum {
        ACTOR_210700_EYES_OPEN           = 0,
        ACTOR_210700_EYES_CLOSED         = 1,
        ACTOR_210700_EYES_OPEN_ALTERNATE = 2,
        ACTOR_210700_EYES_BLINK          = 3,
        ACTOR_210700_BLINK_FRAME_DELAY   = 1,
    };

    RECT            eyeRect;
    GpuImageUpload* uploadList;
    s32             uploadResult;

    uploadResult = 0;
    ACTOR_210700_INIT_EYE_TEXTURE_RECT(eyeRect);

    switch (eyeMode) {
        case ACTOR_210700_EYES_CLOSED:
            uploadList = &D_actor_210700_8015858C[0];
            break;
        case ACTOR_210700_EYES_OPEN:
        case ACTOR_210700_EYES_OPEN_ALTERNATE:
            uploadList = &D_actor_210700_80157F4C[0];
            break;
        case ACTOR_210700_EYES_BLINK:
            ((_Actor210700Work*)task->work)->blinkStep       = ACTOR_210700_BLINK_CLOSED;
            ((_Actor210700Work*)task->work)->blinkFrameDelay = ACTOR_210700_BLINK_FRAME_DELAY;
            uploadList                                       = &D_actor_210700_8015826C[0];
            break;
        default:
            uploadList = NULL;
            break;
    }

    if (uploadList != NULL) {
        uploadResult = actorRenderUploadTexture(task, uploadList, &eyeRect);
    }
    return uploadResult;
}

#undef ACTOR_210700_DRAW_GROUND_SHADOW
#undef ACTOR_210700_INIT_EYE_TEXTURE_RECT
