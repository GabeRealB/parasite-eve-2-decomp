#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/rand.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/ending.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

// Exported instance: a room spawns from this package's table by name.
#define gStrideWalkTasks gActor460200StrideWalkTasks
#include "../../shared/screen_negative.h"
#include "../../shared/paced_walk.h"
#include "../../shared/walker.h"
#include "../../shared/stride_walk.h"

/// The clips the soldiers' scenes add to the player's animation bank, with the
/// copy request that installs them and the scene-actor play requests stored
/// after it.
///
/// Every event script of the package that plays an animation on the player
/// first sends it the copy request. The copy takes
/// `ANIMATION_BANK_EXTENSION_CAPACITY` words from the start of the storage,
/// which is the whole of it and more than the clip table holds: the fifteen
/// table words occupy extended ids 47-61, and the copy request and the three
/// play requests are written into the bank after them. The player's requests
/// select ids 47-57 and 61 only, so none of those following words is played
/// as a clip.
///
/// The play requests are not the player's. They are the first three of the
/// run of requests the scripts send to the scene's second placed actor, and
/// are part of this object only because the copied span reaches over them;
/// the rest of the run follows as separate objects.
typedef union {
    struct {
        AnimationSet*            sets[15];             // Player clips for extended ids 47-61; NULL at the three ids (58-60) nothing requests
        AnimationBankCopyRequest copy;                 // Installs the whole of this storage in the player's bank extension
        AnimationPlayRequest     actorPlayRequests[3]; // Requests for animation ids 1, 4 and 5 of the scene's second placed actor; nothing references the first
    } data;                                            // The records by name
    s32 words[ANIMATION_BANK_EXTENSION_CAPACITY];      // The same storage as the copy reads it
} _Actor460200AnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor460200AnimationBankExtensionStorage, ANIMATION_BANK_EXTENSION_CAPACITY * sizeof(s32));

extern _Actor460200AnimationBankExtensionStorage D_actor_460200_80135E30;

s32 func_actor_460200_80133C64(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3);

s32 func_actor_460200_80133CD0(Task* task, s32 arg1, s32 flags, s32 arg3);

// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry gPacedWalkMsgTable[6];
extern AnimationSet*    gPacedWalkAnimBank[16];

extern u8 gPacedWalkEffectParts[];

extern AnimationPlayRequest D_actor_460200_80135F14;
extern AnimationPlayRequest D_actor_460200_8013607C;
extern ActorTransform       D_actor_460200_80136234;
extern EvsCommand           D_actor_460200_80137AA0[];
extern EvsCommand           D_actor_460200_80137BA8[];
extern EvsCommand           D_actor_460200_80137CB0[];
extern EvsCommand           D_actor_460200_80137DA0[];
extern EvsCommand           D_actor_460200_80137F98[];
extern EvsCommand           D_actor_460200_80137FE0[];
extern EvsCommand           D_actor_460200_80138028[];
extern EvsCommand           D_actor_460200_80138070[];

extern TaskDesc         gStrideWalkTasks[];
extern u8               gStrideWalkAnimParams[];
extern TaskMessageEntry gStrideWalkMessages[6];
extern TaskMessageEntry D_actor_460200_801514FC[6];
extern s32              D_actor_460200_80151538;

static void _actorRenderDrawSecondFixedWalkerGroundShadow(Task* task);
static void _pacedWalkTickSoldierBAnim(Task* task);
static void _pacedWalkResetSoldierBAnim(Task* task);
static void _pacedWalkBlendSoldierBAnim(Task* task);
static void _pacedWalkUpdateSoldierC(Task* task);
static void func_actor_460200_801338C0(Enemy* enemy, Task* task);
static void func_actor_460200_80133A04(Enemy* enemy, Task* task);
static void func_actor_460200_80133A88(Task* task);
static void _actorRenderDrawThirdFixedWalkerGroundShadow(Task* task);
static void _pacedWalkTickSoldierCAnim(Task* task);
static void _pacedWalkResetSoldierCAnim(Task* task);
static void _pacedWalkBlendSoldierCAnim(Task* task);

static TmdSource _gActor460200SoldierBRifle;
static TmdSource _gActor460200SoldierBBody;
void             func_actor_460200_801330C8(Task*);

static s32 _pacedWalkPlaceSoldierB(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
s32        func_actor_460200_80133568(Task* task, s32 msgId, ActorCommand* args, s32 arg3);

static TmdSource _gActor460200SoldierCBody;
s32              func_actor_460200_80133C64(Task*, s32, AnimationPlayRequest*, s32);
s32              func_actor_460200_80133CD0(Task*, s32, s32, s32);
static s32       _pacedWalkPlaceSoldierC(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
s32              func_actor_460200_80133DC4(Task*, s32, s32, s32);
static s32       _pacedWalkSetSoldierCWalkTarget(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArgument);
void             func_actor_460200_8013386C(Task*);

extern AnimationPlayRequest D_actor_460200_80135E1C;
extern AnimationPlayRequest D_actor_460200_80135F28;
extern AnimationPlayRequest D_actor_460200_80135F3C;
extern AnimationPlayRequest D_actor_460200_80135F50;
extern AnimationPlayRequest D_actor_460200_80136090;
extern AnimationPlayRequest D_actor_460200_801360A4;
extern AnimationPlayRequest D_actor_460200_801360B8;
extern AnimationPlayRequest D_actor_460200_801360CC;
s32                         func_actor_460200_80132C8C(Task* task, s32 msgId, ActorCommand* args, s32 arg3);
void                        func_actor_460200_801327B4(Task*);

extern AnimationPlayRequest D_actor_460200_80135EB0;
extern AnimationPlayRequest D_actor_460200_80135EC4;
extern AnimationPlayRequest D_actor_460200_80135ED8;
extern AnimationPlayRequest D_actor_460200_80135EEC;
extern AnimationPlayRequest D_actor_460200_80135F00;
extern AnimationPlayRequest D_actor_460200_80135F78;
extern AnimationPlayRequest D_actor_460200_80135F8C;
extern AnimationPlayRequest D_actor_460200_80135FA0;
extern AnimationPlayRequest D_actor_460200_80135FB4;
extern AnimationPlayRequest D_actor_460200_80135FC8;
extern AnimationPlayRequest D_actor_460200_80135FDC;
extern AnimationPlayRequest D_actor_460200_80135FF0;
extern AnimationPlayRequest D_actor_460200_80136004;
extern AnimationPlayRequest D_actor_460200_80136018;
extern AnimationPlayRequest D_actor_460200_8013602C;
extern AnimationPlayRequest D_actor_460200_80136040;
extern AnimationPlayRequest D_actor_460200_801360E0;
extern AnimationPlayRequest D_actor_460200_801360F4;
extern AnimationPlayRequest D_actor_460200_80136108;
extern AnimationPlayRequest D_actor_460200_8013611C;
extern AnimationPlayRequest D_actor_460200_80136130;
extern AnimationPlayRequest D_actor_460200_80136144;
extern AnimationPlayRequest D_actor_460200_80136158;
extern AnimationPlayRequest D_actor_460200_8013616C;
extern AnimationPlayRequest D_actor_460200_80136180;
extern AnimationPlayRequest D_actor_460200_80136194;
extern AnimationPlayRequest D_actor_460200_801361A8;
extern ActorTransform       D_actor_460200_801361BC;
extern ActorTransform       D_actor_460200_801361D4;
extern ActorTransform       D_actor_460200_801361EC;
extern ActorTransform       D_actor_460200_80136204;
extern ActorTransform       D_actor_460200_8013621C;
void                        func_actor_460200_80132090(Task*);
void                        func_actor_460200_801320E0(s32);
static void                 func_actor_460200_80132124(void);
void                        func_actor_460200_80132204(s8);

extern _Actor460200AnimationBankExtensionStorage D_actor_460200_80135E30;
void                                             func_actor_460200_80131E24(Task*);

static AnimationSet _gActor460200Animation1C038;
static AnimationSet _gActor460200Animation1C304;
static AnimationSet _gActor460200Animation1C650;
static AnimationSet _gActor460200Animation1C82C;
static AnimationSet _gActor460200Animation1CD58;
static AnimationSet _gActor460200Animation1CF40;
static AnimationSet _gActor460200Animation1D300;
static AnimationSet _gActor460200Animation1D4D8;
static AnimationSet _gActor460200Animation1D8D4;
static AnimationSet _gActor460200Animation1DAC0;
static AnimationSet _gActor460200Animation1DE20;
static AnimationSet _gActor460200Animation1EA24;
static AnimationSet _gActor460200Animation1EC64;
static AnimationSet _gActor460200Animation1EE5C;
static AnimationSet _gActor460200Animation1F158;
static AnimationSet _gActor460200Animation1F478;
static AnimationSet _gActor460200Animation1F6B4;

static AnimationPackedPose _gActor460200Animation022B4Bank1[2] = {
#include "assets/actor_460200_animation_022B4_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation022B4Bank4[26] = {
#include "assets/actor_460200_animation_022B4_bank4.inc"
};

static AnimationRecord _gActor460200Animation022B4Records[102] = {
#include "assets/actor_460200_animation_022B4_records.inc"
};

static u16 _gActor460200Animation022B4Indices[20] = {
#include "assets/actor_460200_animation_022B4_indices.inc"
};

static AnimationSet _gActor460200Animation022B4 = {
    _gActor460200Animation022B4Records,
    _gActor460200Animation022B4Indices,
    { NULL, _gActor460200Animation022B4Bank1, NULL, NULL, _gActor460200Animation022B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation02520Bank1[2] = {
#include "assets/actor_460200_animation_02520_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation02520Bank4[25] = {
#include "assets/actor_460200_animation_02520_bank4.inc"
};

static AnimationRecord _gActor460200Animation02520Records[104] = {
#include "assets/actor_460200_animation_02520_records.inc"
};

static u16 _gActor460200Animation02520Indices[20] = {
#include "assets/actor_460200_animation_02520_indices.inc"
};

static AnimationSet _gActor460200Animation02520 = {
    _gActor460200Animation02520Records,
    _gActor460200Animation02520Indices,
    { NULL, _gActor460200Animation02520Bank1, NULL, NULL, _gActor460200Animation02520Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation026C0Bank1[2] = {
#include "assets/actor_460200_animation_026C0_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation026C0Bank4[21] = {
#include "assets/actor_460200_animation_026C0_bank4.inc"
};

static AnimationRecord _gActor460200Animation026C0Records[57] = {
#include "assets/actor_460200_animation_026C0_records.inc"
};

static u16 _gActor460200Animation026C0Indices[20] = {
#include "assets/actor_460200_animation_026C0_indices.inc"
};

static AnimationSet _gActor460200Animation026C0 = {
    _gActor460200Animation026C0Records,
    _gActor460200Animation026C0Indices,
    { NULL, _gActor460200Animation026C0Bank1, NULL, NULL, _gActor460200Animation026C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation029E0Bank1[2] = {
#include "assets/actor_460200_animation_029E0_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation029E0Bank4[44] = {
#include "assets/actor_460200_animation_029E0_bank4.inc"
};

static AnimationRecord _gActor460200Animation029E0Records[130] = {
#include "assets/actor_460200_animation_029E0_records.inc"
};

static u16 _gActor460200Animation029E0Indices[20] = {
#include "assets/actor_460200_animation_029E0_indices.inc"
};

static AnimationSet _gActor460200Animation029E0 = {
    _gActor460200Animation029E0Records,
    _gActor460200Animation029E0Indices,
    { NULL, _gActor460200Animation029E0Bank1, NULL, NULL, _gActor460200Animation029E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation02B8CBank1[2] = {
#include "assets/actor_460200_animation_02B8C_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation02B8CBank4[24] = {
#include "assets/actor_460200_animation_02B8C_bank4.inc"
};

static AnimationRecord _gActor460200Animation02B8CRecords[57] = {
#include "assets/actor_460200_animation_02B8C_records.inc"
};

static u16 _gActor460200Animation02B8CIndices[20] = {
#include "assets/actor_460200_animation_02B8C_indices.inc"
};

static AnimationSet _gActor460200Animation02B8C = {
    _gActor460200Animation02B8CRecords,
    _gActor460200Animation02B8CIndices,
    { NULL, _gActor460200Animation02B8CBank1, NULL, NULL, _gActor460200Animation02B8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation02DE0Bank1[2] = {
#include "assets/actor_460200_animation_02DE0_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation02DE0Bank4[25] = {
#include "assets/actor_460200_animation_02DE0_bank4.inc"
};

static AnimationRecord _gActor460200Animation02DE0Records[98] = {
#include "assets/actor_460200_animation_02DE0_records.inc"
};

static u16 _gActor460200Animation02DE0Indices[20] = {
#include "assets/actor_460200_animation_02DE0_indices.inc"
};

static AnimationSet _gActor460200Animation02DE0 = {
    _gActor460200Animation02DE0Records,
    _gActor460200Animation02DE0Indices,
    { NULL, _gActor460200Animation02DE0Bank1, NULL, NULL, _gActor460200Animation02DE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation03020Bank1[2] = {
#include "assets/actor_460200_animation_03020_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation03020Bank4[26] = {
#include "assets/actor_460200_animation_03020_bank4.inc"
};

static AnimationRecord _gActor460200Animation03020Records[92] = {
#include "assets/actor_460200_animation_03020_records.inc"
};

static u16 _gActor460200Animation03020Indices[20] = {
#include "assets/actor_460200_animation_03020_indices.inc"
};

static AnimationSet _gActor460200Animation03020 = {
    _gActor460200Animation03020Records,
    _gActor460200Animation03020Indices,
    { NULL, _gActor460200Animation03020Bank1, NULL, NULL, _gActor460200Animation03020Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation031D0Bank1[2] = {
#include "assets/actor_460200_animation_031D0_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation031D0Bank4[25] = {
#include "assets/actor_460200_animation_031D0_bank4.inc"
};

static AnimationRecord _gActor460200Animation031D0Records[57] = {
#include "assets/actor_460200_animation_031D0_records.inc"
};

static u16 _gActor460200Animation031D0Indices[20] = {
#include "assets/actor_460200_animation_031D0_indices.inc"
};

static AnimationSet _gActor460200Animation031D0 = {
    _gActor460200Animation031D0Records,
    _gActor460200Animation031D0Indices,
    { NULL, _gActor460200Animation031D0Bank1, NULL, NULL, _gActor460200Animation031D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation033B8Bank1[2] = {
#include "assets/actor_460200_animation_033B8_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation033B8Bank4[25] = {
#include "assets/actor_460200_animation_033B8_bank4.inc"
};

static AnimationRecord _gActor460200Animation033B8Records[71] = {
#include "assets/actor_460200_animation_033B8_records.inc"
};

static u16 _gActor460200Animation033B8Indices[20] = {
#include "assets/actor_460200_animation_033B8_indices.inc"
};

static AnimationSet _gActor460200Animation033B8 = {
    _gActor460200Animation033B8Records,
    _gActor460200Animation033B8Indices,
    { NULL, _gActor460200Animation033B8Bank1, NULL, NULL, _gActor460200Animation033B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation03584Bank1[2] = {
#include "assets/actor_460200_animation_03584_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation03584Bank4[22] = {
#include "assets/actor_460200_animation_03584_bank4.inc"
};

static AnimationRecord _gActor460200Animation03584Records[67] = {
#include "assets/actor_460200_animation_03584_records.inc"
};

static u16 _gActor460200Animation03584Indices[20] = {
#include "assets/actor_460200_animation_03584_indices.inc"
};

static AnimationSet _gActor460200Animation03584 = {
    _gActor460200Animation03584Records,
    _gActor460200Animation03584Indices,
    { NULL, _gActor460200Animation03584Bank1, NULL, NULL, _gActor460200Animation03584Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation03BF4Bank1[12] = {
#include "assets/actor_460200_animation_03BF4_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation03BF4Bank4[159] = {
#include "assets/actor_460200_animation_03BF4_bank4.inc"
};

static AnimationRecord _gActor460200Animation03BF4Records[197] = {
#include "assets/actor_460200_animation_03BF4_records.inc"
};

static u16 _gActor460200Animation03BF4Indices[20] = {
#include "assets/actor_460200_animation_03BF4_indices.inc"
};

static AnimationSet _gActor460200Animation03BF4 = {
    _gActor460200Animation03BF4Records,
    _gActor460200Animation03BF4Indices,
    { NULL, _gActor460200Animation03BF4Bank1, NULL, NULL, _gActor460200Animation03BF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation03FACBank1[3] = {
#include "assets/actor_460200_animation_03FAC_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation03FACBank4[81] = {
#include "assets/actor_460200_animation_03FAC_bank4.inc"
};

static AnimationRecord _gActor460200Animation03FACRecords[128] = {
#include "assets/actor_460200_animation_03FAC_records.inc"
};

static u16 _gActor460200Animation03FACIndices[20] = {
#include "assets/actor_460200_animation_03FAC_indices.inc"
};

static AnimationSet _gActor460200Animation03FAC = {
    _gActor460200Animation03FACRecords,
    _gActor460200Animation03FACIndices,
    { NULL, _gActor460200Animation03FACBank1, NULL, NULL, _gActor460200Animation03FACBank4, NULL, NULL, NULL },
};

TaskDesc D_actor_460200_80135DF4[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_actor_460200_80131E24, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

RECT gScreenNegativeFrameRect = { 0, 0, 320, 240 };

RECT gScreenNegativeStripRect = { 0, 0, 16, 240 };

AnimationPlayRequest D_actor_460200_80135E1C = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

_Actor460200AnimationBankExtensionStorage D_actor_460200_80135E30 = { .data = { { &_gActor460200Animation022B4, &_gActor460200Animation02520, &_gActor460200Animation026C0, &_gActor460200Animation029E0, &_gActor460200Animation02B8C, &_gActor460200Animation02DE0, &_gActor460200Animation03020, &_gActor460200Animation031D0, &_gActor460200Animation033B8, &_gActor460200Animation03584, &_gActor460200Animation03BF4, NULL, NULL, NULL, &_gActor460200Animation03FAC }, { { .words = D_actor_460200_80135E30.words }, ANIMATION_BANK_EXTENSION_CAPACITY }, { { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_460200_80135EB0 = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135EC4 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135ED8 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135EEC = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F00 = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F14 = { { .index = 1 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F28 = { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F3C = { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F50 = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F64 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F78 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135F8C = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135FA0 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135FB4 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135FC8 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135FDC = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80135FF0 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136004 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136018 = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_8013602C = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136040 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136054[2] = {
    { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_460200_8013607C = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136090 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_801360A4 = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_801360B8 = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_801360CC = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_801360E0 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_801360F4 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136108 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_8013611C = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136130 = { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136144 = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136158 = { { .index = 1 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_8013616C = { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136180 = { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_80136194 = { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_460200_801361A8 = { { .index = 1 }, 17, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_460200_801361BC = { { -910, 0, 5650, 0 }, { 0, -1592, 0, 0 } };

ActorTransform D_actor_460200_801361D4 = { { -1050, 0, 5418, 0 }, { 0, -1592, 0, 0 } };

ActorTransform D_actor_460200_801361EC = { { -2100, 0, 4600, 0 }, { 0, 398, 0, 0 } };

ActorTransform D_actor_460200_80136204 = { { -2100, 0, 4600, 0 }, { 0, 341, 0, 0 } };

ActorTransform D_actor_460200_8013621C = { { -2000, 0, 3800, 0 }, { 0, 1479, 0, 0 } };

ActorTransform D_actor_460200_80136234 = { { -2000, 0, 4700, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_460200_8013624C = { { 800, 0, 4490, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_460200_80136264 = { { -250, 0, 4490, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_460200_8013627C = { { -1000, 0, 4490, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_460200_80136294 = { { -1300, 0, 3300, 0 }, { 0, -568, 0, 0 } };

TaskDesc D_actor_460200_801362AC = { { { TASK_BODY_NONE, 32 } }, func_actor_460200_80132090, { .value = 0 } };

EvsCommand D_actor_460200_801362B8[233] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_460200_801320E0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_460200_80135E30.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 6 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_460200_801361BC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_801361EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_80136204 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136158 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136108 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x551C0007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013611C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_8013624C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135E30.data.actorPlayRequests[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2013 }, { .message = { .pointer = &D_actor_460200_80136264 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135E30.data.actorPlayRequests[2] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136130 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135EB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135E30.data.actorPlayRequests[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2013 }, { .message = { .pointer = &D_actor_460200_8013627C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_801361EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136144 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x551C0008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_8013621C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_80136294 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FDC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135EEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135ED8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135EC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135EEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135ED8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_460200_80132124 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_80114A24 }, { .vibrationSegments = D_80114A34 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_801361EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_80136204 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013616C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FF0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80136004 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136180 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136194 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80136018 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135FC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801361A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_8013602C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_460200_801361D4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80136040 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_80136234 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_460200_801320E0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x551C0009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x551C000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_460200_80132204 }, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137890[22] = {
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x551C0007 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x551C0008 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_460200_80132204 }, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_460200_80136234 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_460200_801361D4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_460200_801320E0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x551C0009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x551C000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137AA0[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_460200_80135E30.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137BA8[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_460200_80135E30.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137CB0[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_460200_80135E30.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135E1C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137DA0[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_460200_80135E30.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135E1C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80136090 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_801360A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_8013607C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137E90[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_460200_80135E30.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135F28 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135E1C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_460200_80135F50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135F3C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_460200_80135F14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_460200_80137F98[3] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_460200_80137E90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80137FE0[3] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_460200_80137E90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80138028[3] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_460200_80137E90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_460200_80138070[3] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_460200_80137E90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor460200SoldierABodySkeleton[20] = {
#include "assets/soldier_a_body_skeleton.inc"
};

static u32 _gActor460200SoldierABodyPartVerts[20] = {
#include "assets/soldier_a_body_partVerts.inc"
};

static SVECTOR _gActor460200SoldierABodyVerts[363] = {
#include "assets/soldier_a_body_verts.inc"
};

static SVECTOR _gActor460200SoldierABodyNormals[360] = {
#include "assets/soldier_a_body_normals.inc"
};

static u32 _gActor460200SoldierABodyStream[3907] = {
#include "assets/soldier_a_body_stream.inc"
};

static TmdSource _gActor460200SoldierABody = {
    0,
    21060,
    6448,
    20,
    _gActor460200SoldierABodyPartVerts,
    _gActor460200SoldierABodyVerts,
    _gActor460200SoldierABodyNormals,
    _gActor460200SoldierABodySkeleton,
    _gActor460200SoldierABodyStream,
};

static AnimationPackedPose _gActor460200Animation0BBB0Bank1[2] = {
#include "assets/actor_460200_animation_0BBB0_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0BBB0Bank4[28] = {
#include "assets/actor_460200_animation_0BBB0_bank4.inc"
};

static AnimationRecord _gActor460200Animation0BBB0Records[96] = {
#include "assets/actor_460200_animation_0BBB0_records.inc"
};

static u16 _gActor460200Animation0BBB0Indices[20] = {
#include "assets/actor_460200_animation_0BBB0_indices.inc"
};

static AnimationSet _gActor460200Animation0BBB0 = {
    _gActor460200Animation0BBB0Records,
    _gActor460200Animation0BBB0Indices,
    { NULL, _gActor460200Animation0BBB0Bank1, NULL, NULL, _gActor460200Animation0BBB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0BE48Bank1[2] = {
#include "assets/actor_460200_animation_0BE48_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0BE48Bank4[46] = {
#include "assets/actor_460200_animation_0BE48_bank4.inc"
};

static AnimationRecord _gActor460200Animation0BE48Records[94] = {
#include "assets/actor_460200_animation_0BE48_records.inc"
};

static u16 _gActor460200Animation0BE48Indices[20] = {
#include "assets/actor_460200_animation_0BE48_indices.inc"
};

static AnimationSet _gActor460200Animation0BE48 = {
    _gActor460200Animation0BE48Records,
    _gActor460200Animation0BE48Indices,
    { NULL, _gActor460200Animation0BE48Bank1, NULL, NULL, _gActor460200Animation0BE48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0C190Bank1[2] = {
#include "assets/actor_460200_animation_0C190_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0C190Bank4[58] = {
#include "assets/actor_460200_animation_0C190_bank4.inc"
};

static AnimationRecord _gActor460200Animation0C190Records[126] = {
#include "assets/actor_460200_animation_0C190_records.inc"
};

static u16 _gActor460200Animation0C190Indices[20] = {
#include "assets/actor_460200_animation_0C190_indices.inc"
};

static AnimationSet _gActor460200Animation0C190 = {
    _gActor460200Animation0C190Records,
    _gActor460200Animation0C190Indices,
    { NULL, _gActor460200Animation0C190Bank1, NULL, NULL, _gActor460200Animation0C190Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0C4C8Bank1[2] = {
#include "assets/actor_460200_animation_0C4C8_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0C4C8Bank4[65] = {
#include "assets/actor_460200_animation_0C4C8_bank4.inc"
};

static AnimationRecord _gActor460200Animation0C4C8Records[115] = {
#include "assets/actor_460200_animation_0C4C8_records.inc"
};

static u16 _gActor460200Animation0C4C8Indices[20] = {
#include "assets/actor_460200_animation_0C4C8_indices.inc"
};

static AnimationSet _gActor460200Animation0C4C8 = {
    _gActor460200Animation0C4C8Records,
    _gActor460200Animation0C4C8Indices,
    { NULL, _gActor460200Animation0C4C8Bank1, NULL, NULL, _gActor460200Animation0C4C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0C6E0Bank1[2] = {
#include "assets/actor_460200_animation_0C6E0_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0C6E0Bank4[28] = {
#include "assets/actor_460200_animation_0C6E0_bank4.inc"
};

static AnimationRecord _gActor460200Animation0C6E0Records[80] = {
#include "assets/actor_460200_animation_0C6E0_records.inc"
};

static u16 _gActor460200Animation0C6E0Indices[20] = {
#include "assets/actor_460200_animation_0C6E0_indices.inc"
};

static AnimationSet _gActor460200Animation0C6E0 = {
    _gActor460200Animation0C6E0Records,
    _gActor460200Animation0C6E0Indices,
    { NULL, _gActor460200Animation0C6E0Bank1, NULL, NULL, _gActor460200Animation0C6E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0CB74Bank1[2] = {
#include "assets/actor_460200_animation_0CB74_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0CB74Bank4[107] = {
#include "assets/actor_460200_animation_0CB74_bank4.inc"
};

static AnimationRecord _gActor460200Animation0CB74Records[160] = {
#include "assets/actor_460200_animation_0CB74_records.inc"
};

static u16 _gActor460200Animation0CB74Indices[20] = {
#include "assets/actor_460200_animation_0CB74_indices.inc"
};

static AnimationSet _gActor460200Animation0CB74 = {
    _gActor460200Animation0CB74Records,
    _gActor460200Animation0CB74Indices,
    { NULL, _gActor460200Animation0CB74Bank1, NULL, NULL, _gActor460200Animation0CB74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0CDECBank1[2] = {
#include "assets/actor_460200_animation_0CDEC_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0CDECBank4[28] = {
#include "assets/actor_460200_animation_0CDEC_bank4.inc"
};

static AnimationRecord _gActor460200Animation0CDECRecords[104] = {
#include "assets/actor_460200_animation_0CDEC_records.inc"
};

static u16 _gActor460200Animation0CDECIndices[20] = {
#include "assets/actor_460200_animation_0CDEC_indices.inc"
};

static AnimationSet _gActor460200Animation0CDEC = {
    _gActor460200Animation0CDECRecords,
    _gActor460200Animation0CDECIndices,
    { NULL, _gActor460200Animation0CDECBank1, NULL, NULL, _gActor460200Animation0CDECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0D040Bank1[2] = {
#include "assets/actor_460200_animation_0D040_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0D040Bank4[45] = {
#include "assets/actor_460200_animation_0D040_bank4.inc"
};

static AnimationRecord _gActor460200Animation0D040Records[78] = {
#include "assets/actor_460200_animation_0D040_records.inc"
};

static u16 _gActor460200Animation0D040Indices[20] = {
#include "assets/actor_460200_animation_0D040_indices.inc"
};

static AnimationSet _gActor460200Animation0D040 = {
    _gActor460200Animation0D040Records,
    _gActor460200Animation0D040Indices,
    { NULL, _gActor460200Animation0D040Bank1, NULL, NULL, _gActor460200Animation0D040Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0D214Bank1[2] = {
#include "assets/actor_460200_animation_0D214_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0D214Bank4[23] = {
#include "assets/actor_460200_animation_0D214_bank4.inc"
};

static AnimationRecord _gActor460200Animation0D214Records[68] = {
#include "assets/actor_460200_animation_0D214_records.inc"
};

static u16 _gActor460200Animation0D214Indices[20] = {
#include "assets/actor_460200_animation_0D214_indices.inc"
};

static AnimationSet _gActor460200Animation0D214 = {
    _gActor460200Animation0D214Records,
    _gActor460200Animation0D214Indices,
    { NULL, _gActor460200Animation0D214Bank1, NULL, NULL, _gActor460200Animation0D214Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0D3B4Bank1[2] = {
#include "assets/actor_460200_animation_0D3B4_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0D3B4Bank4[18] = {
#include "assets/actor_460200_animation_0D3B4_bank4.inc"
};

static AnimationRecord _gActor460200Animation0D3B4Records[60] = {
#include "assets/actor_460200_animation_0D3B4_records.inc"
};

static u16 _gActor460200Animation0D3B4Indices[20] = {
#include "assets/actor_460200_animation_0D3B4_indices.inc"
};

static AnimationSet _gActor460200Animation0D3B4 = {
    _gActor460200Animation0D3B4Records,
    _gActor460200Animation0D3B4Indices,
    { NULL, _gActor460200Animation0D3B4Bank1, NULL, NULL, _gActor460200Animation0D3B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0D590Bank1[2] = {
#include "assets/actor_460200_animation_0D590_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0D590Bank4[29] = {
#include "assets/actor_460200_animation_0D590_bank4.inc"
};

static AnimationRecord _gActor460200Animation0D590Records[64] = {
#include "assets/actor_460200_animation_0D590_records.inc"
};

static u16 _gActor460200Animation0D590Indices[20] = {
#include "assets/actor_460200_animation_0D590_indices.inc"
};

static AnimationSet _gActor460200Animation0D590 = {
    _gActor460200Animation0D590Records,
    _gActor460200Animation0D590Indices,
    { NULL, _gActor460200Animation0D590Bank1, NULL, NULL, _gActor460200Animation0D590Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0D7B8Bank1[2] = {
#include "assets/actor_460200_animation_0D7B8_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0D7B8Bank4[24] = {
#include "assets/actor_460200_animation_0D7B8_bank4.inc"
};

static AnimationRecord _gActor460200Animation0D7B8Records[88] = {
#include "assets/actor_460200_animation_0D7B8_records.inc"
};

static u16 _gActor460200Animation0D7B8Indices[20] = {
#include "assets/actor_460200_animation_0D7B8_indices.inc"
};

static AnimationSet _gActor460200Animation0D7B8 = {
    _gActor460200Animation0D7B8Records,
    _gActor460200Animation0D7B8Indices,
    { NULL, _gActor460200Animation0D7B8Bank1, NULL, NULL, _gActor460200Animation0D7B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0D9E4Bank1[2] = {
#include "assets/actor_460200_animation_0D9E4_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0D9E4Bank4[25] = {
#include "assets/actor_460200_animation_0D9E4_bank4.inc"
};

static AnimationRecord _gActor460200Animation0D9E4Records[88] = {
#include "assets/actor_460200_animation_0D9E4_records.inc"
};

static u16 _gActor460200Animation0D9E4Indices[20] = {
#include "assets/actor_460200_animation_0D9E4_indices.inc"
};

static AnimationSet _gActor460200Animation0D9E4 = {
    _gActor460200Animation0D9E4Records,
    _gActor460200Animation0D9E4Indices,
    { NULL, _gActor460200Animation0D9E4Bank1, NULL, NULL, _gActor460200Animation0D9E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0DC4CBank1[2] = {
#include "assets/actor_460200_animation_0DC4C_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0DC4CBank4[31] = {
#include "assets/actor_460200_animation_0DC4C_bank4.inc"
};

static AnimationRecord _gActor460200Animation0DC4CRecords[97] = {
#include "assets/actor_460200_animation_0DC4C_records.inc"
};

static u16 _gActor460200Animation0DC4CIndices[20] = {
#include "assets/actor_460200_animation_0DC4C_indices.inc"
};

static AnimationSet _gActor460200Animation0DC4C = {
    _gActor460200Animation0DC4CRecords,
    _gActor460200Animation0DC4CIndices,
    { NULL, _gActor460200Animation0DC4CBank1, NULL, NULL, _gActor460200Animation0DC4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation0DE08Bank1[2] = {
#include "assets/actor_460200_animation_0DE08_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation0DE08Bank4[25] = {
#include "assets/actor_460200_animation_0DE08_bank4.inc"
};

static AnimationRecord _gActor460200Animation0DE08Records[60] = {
#include "assets/actor_460200_animation_0DE08_records.inc"
};

static u16 _gActor460200Animation0DE08Indices[20] = {
#include "assets/actor_460200_animation_0DE08_indices.inc"
};

static AnimationSet _gActor460200Animation0DE08 = {
    _gActor460200Animation0DE08Records,
    _gActor460200Animation0DE08Indices,
    { NULL, _gActor460200Animation0DE08Bank1, NULL, NULL, _gActor460200Animation0DE08Bank4, NULL, NULL, NULL },
};

TaskMessageEntry gPacedWalkMsgTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pacedWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, pacedWalkShowPair },
    { ACTOR_MESSAGE_PLACE, PACED_WALK_PLACE },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_460200_80132C8C },
    { ACTOR_MESSAGE_WALK_TO, PACED_WALK_SET_WALK_TARGET },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_460200_8013FC80 = { { { TASK_BODY_TMD, 96 } }, func_actor_460200_801327B4, { .model = &_gActor460200SoldierABody } };

AnimationSet* gPacedWalkAnimBank[16] = {
    NULL,
    &_gActor460200Animation0BBB0,
    &_gActor460200Animation0BE48,
    &_gActor460200Animation0C190,
    &_gActor460200Animation0C4C8,
    &_gActor460200Animation0C6E0,
    &_gActor460200Animation0CB74,
    &_gActor460200Animation0CDEC,
    &_gActor460200Animation0D040,
    &_gActor460200Animation0D214,
    &_gActor460200Animation0D3B4,
    &_gActor460200Animation0D590,
    &_gActor460200Animation0D7B8,
    &_gActor460200Animation0D9E4,
    &_gActor460200Animation0DC4C,
    &_gActor460200Animation0DE08,
};

u8 gPacedWalkEffectParts[12] = {
    1,
    3,
    5,
    6,
    9,
    14,
    15,
    16,
    17,
    18,
    19,
    0,
};

static TmdBone _gActor460200SoldierBRifleSkeleton[1] = {
#include "assets/soldier_b_rifle_skeleton.inc"
};

static u32 _gActor460200SoldierBRiflePartVerts[1] = {
#include "assets/soldier_b_rifle_partVerts.inc"
};

static SVECTOR _gActor460200SoldierBRifleVerts[58] = {
#include "assets/soldier_b_rifle_verts.inc"
};

static SVECTOR _gActor460200SoldierBRifleNormals[58] = {
#include "assets/soldier_b_rifle_normals.inc"
};

static u32 _gActor460200SoldierBRifleStream[406] = {
#include "assets/soldier_b_rifle_stream.inc"
};

static TmdSource _gActor460200SoldierBRifle = {
    0,
    2940,
    0,
    1,
    _gActor460200SoldierBRiflePartVerts,
    _gActor460200SoldierBRifleVerts,
    _gActor460200SoldierBRifleNormals,
    _gActor460200SoldierBRifleSkeleton,
    _gActor460200SoldierBRifleStream,
};

static TmdBone _gActor460200SoldierBBodySkeleton[20] = {
#include "assets/soldier_b_body_skeleton.inc"
};

static u32 _gActor460200SoldierBBodyPartVerts[20] = {
#include "assets/soldier_b_body_partVerts.inc"
};

static SVECTOR _gActor460200SoldierBBodyVerts[366] = {
#include "assets/soldier_b_body_verts.inc"
};

static SVECTOR _gActor460200SoldierBBodyNormals[363] = {
#include "assets/soldier_b_body_normals.inc"
};

static u32 _gActor460200SoldierBBodyStream[3913] = {
#include "assets/soldier_b_body_stream.inc"
};

static TmdSource _gActor460200SoldierBBody = {
    0,
    21132,
    6448,
    20,
    _gActor460200SoldierBBodyPartVerts,
    _gActor460200SoldierBBodyVerts,
    _gActor460200SoldierBBodyNormals,
    _gActor460200SoldierBBodySkeleton,
    _gActor460200SoldierBBodyStream,
};

static AnimationPackedPose _gActor460200Animation14280Bank1[2] = {
#include "assets/actor_460200_animation_14280_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation14280Bank4[32] = {
#include "assets/actor_460200_animation_14280_bank4.inc"
};

static AnimationRecord _gActor460200Animation14280Records[101] = {
#include "assets/actor_460200_animation_14280_records.inc"
};

static u16 _gActor460200Animation14280Indices[20] = {
#include "assets/actor_460200_animation_14280_indices.inc"
};

static AnimationSet _gActor460200Animation14280 = {
    _gActor460200Animation14280Records,
    _gActor460200Animation14280Indices,
    { NULL, _gActor460200Animation14280Bank1, NULL, NULL, _gActor460200Animation14280Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation147F4Bank1[2] = {
#include "assets/actor_460200_animation_147F4_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation147F4Bank4[135] = {
#include "assets/actor_460200_animation_147F4_bank4.inc"
};

static AnimationRecord _gActor460200Animation147F4Records[188] = {
#include "assets/actor_460200_animation_147F4_records.inc"
};

static u16 _gActor460200Animation147F4Indices[20] = {
#include "assets/actor_460200_animation_147F4_indices.inc"
};

static AnimationSet _gActor460200Animation147F4 = {
    _gActor460200Animation147F4Records,
    _gActor460200Animation147F4Indices,
    { NULL, _gActor460200Animation147F4Bank1, NULL, NULL, _gActor460200Animation147F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation14A84Bank1[3] = {
#include "assets/actor_460200_animation_14A84_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation14A84Bank4[27] = {
#include "assets/actor_460200_animation_14A84_bank4.inc"
};

static AnimationRecord _gActor460200Animation14A84Records[108] = {
#include "assets/actor_460200_animation_14A84_records.inc"
};

static u16 _gActor460200Animation14A84Indices[20] = {
#include "assets/actor_460200_animation_14A84_indices.inc"
};

static AnimationSet _gActor460200Animation14A84 = {
    _gActor460200Animation14A84Records,
    _gActor460200Animation14A84Indices,
    { NULL, _gActor460200Animation14A84Bank1, NULL, NULL, _gActor460200Animation14A84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation15218Bank1[21] = {
#include "assets/actor_460200_animation_15218_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation15218Bank4[156] = {
#include "assets/actor_460200_animation_15218_bank4.inc"
};

static AnimationRecord _gActor460200Animation15218Records[246] = {
#include "assets/actor_460200_animation_15218_records.inc"
};

static u16 _gActor460200Animation15218Indices[20] = {
#include "assets/actor_460200_animation_15218_indices.inc"
};

static AnimationSet _gActor460200Animation15218 = {
    _gActor460200Animation15218Records,
    _gActor460200Animation15218Indices,
    { NULL, _gActor460200Animation15218Bank1, NULL, NULL, _gActor460200Animation15218Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation154ACBank1[5] = {
#include "assets/actor_460200_animation_154AC_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation154ACBank4[36] = {
#include "assets/actor_460200_animation_154AC_bank4.inc"
};

static AnimationRecord _gActor460200Animation154ACRecords[94] = {
#include "assets/actor_460200_animation_154AC_records.inc"
};

static u16 _gActor460200Animation154ACIndices[20] = {
#include "assets/actor_460200_animation_154AC_indices.inc"
};

static AnimationSet _gActor460200Animation154AC = {
    _gActor460200Animation154ACRecords,
    _gActor460200Animation154ACIndices,
    { NULL, _gActor460200Animation154ACBank1, NULL, NULL, _gActor460200Animation154ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1579CBank1[2] = {
#include "assets/actor_460200_animation_1579C_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1579CBank4[55] = {
#include "assets/actor_460200_animation_1579C_bank4.inc"
};

static AnimationRecord _gActor460200Animation1579CRecords[107] = {
#include "assets/actor_460200_animation_1579C_records.inc"
};

static u16 _gActor460200Animation1579CIndices[20] = {
#include "assets/actor_460200_animation_1579C_indices.inc"
};

static AnimationSet _gActor460200Animation1579C = {
    _gActor460200Animation1579CRecords,
    _gActor460200Animation1579CIndices,
    { NULL, _gActor460200Animation1579CBank1, NULL, NULL, _gActor460200Animation1579CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation15950Bank1[2] = {
#include "assets/actor_460200_animation_15950_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation15950Bank4[19] = {
#include "assets/actor_460200_animation_15950_bank4.inc"
};

static AnimationRecord _gActor460200Animation15950Records[64] = {
#include "assets/actor_460200_animation_15950_records.inc"
};

static u16 _gActor460200Animation15950Indices[20] = {
#include "assets/actor_460200_animation_15950_indices.inc"
};

static AnimationSet _gActor460200Animation15950 = {
    _gActor460200Animation15950Records,
    _gActor460200Animation15950Indices,
    { NULL, _gActor460200Animation15950Bank1, NULL, NULL, _gActor460200Animation15950Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation15B04Bank1[2] = {
#include "assets/actor_460200_animation_15B04_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation15B04Bank4[19] = {
#include "assets/actor_460200_animation_15B04_bank4.inc"
};

static AnimationRecord _gActor460200Animation15B04Records[64] = {
#include "assets/actor_460200_animation_15B04_records.inc"
};

static u16 _gActor460200Animation15B04Indices[20] = {
#include "assets/actor_460200_animation_15B04_indices.inc"
};

static AnimationSet _gActor460200Animation15B04 = {
    _gActor460200Animation15B04Records,
    _gActor460200Animation15B04Indices,
    { NULL, _gActor460200Animation15B04Bank1, NULL, NULL, _gActor460200Animation15B04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation15DBCBank1[3] = {
#include "assets/actor_460200_animation_15DBC_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation15DBCBank4[29] = {
#include "assets/actor_460200_animation_15DBC_bank4.inc"
};

static AnimationRecord _gActor460200Animation15DBCRecords[116] = {
#include "assets/actor_460200_animation_15DBC_records.inc"
};

static u16 _gActor460200Animation15DBCIndices[20] = {
#include "assets/actor_460200_animation_15DBC_indices.inc"
};

static AnimationSet _gActor460200Animation15DBC = {
    _gActor460200Animation15DBCRecords,
    _gActor460200Animation15DBCIndices,
    { NULL, _gActor460200Animation15DBCBank1, NULL, NULL, _gActor460200Animation15DBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation160B0Bank1[2] = {
#include "assets/actor_460200_animation_160B0_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation160B0Bank4[43] = {
#include "assets/actor_460200_animation_160B0_bank4.inc"
};

static AnimationRecord _gActor460200Animation160B0Records[120] = {
#include "assets/actor_460200_animation_160B0_records.inc"
};

static u16 _gActor460200Animation160B0Indices[20] = {
#include "assets/actor_460200_animation_160B0_indices.inc"
};

static AnimationSet _gActor460200Animation160B0 = {
    _gActor460200Animation160B0Records,
    _gActor460200Animation160B0Indices,
    { NULL, _gActor460200Animation160B0Bank1, NULL, NULL, _gActor460200Animation160B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation162A0Bank1[2] = {
#include "assets/actor_460200_animation_162A0_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation162A0Bank4[30] = {
#include "assets/actor_460200_animation_162A0_bank4.inc"
};

static AnimationRecord _gActor460200Animation162A0Records[68] = {
#include "assets/actor_460200_animation_162A0_records.inc"
};

static u16 _gActor460200Animation162A0Indices[20] = {
#include "assets/actor_460200_animation_162A0_indices.inc"
};

static AnimationSet _gActor460200Animation162A0 = {
    _gActor460200Animation162A0Records,
    _gActor460200Animation162A0Indices,
    { NULL, _gActor460200Animation162A0Bank1, NULL, NULL, _gActor460200Animation162A0Bank4, NULL, NULL, NULL },
};

TaskMessageEntry gStrideWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _strideWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _strideWalkSetModelDraw },
    { ACTOR_MESSAGE_PLACE, _pacedWalkPlaceSoldierB },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_460200_80133568 },
    { ACTOR_MESSAGE_WALK_TO, _strideWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gStrideWalkTasks[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_460200_801330C8, { .model = &_gActor460200SoldierBBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _strideWalkSubModelTask, { .model = &_gActor460200SoldierBRifle } },
};

u8 gStrideWalkAnimParams[48] = {
    0,
    0,
    0,
    0,
    160,
    96,
    20,
    128,
    164,
    104,
    20,
    128,
    20,
    102,
    20,
    128,
    56,
    112,
    20,
    128,
    188,
    117,
    20,
    128,
    112,
    119,
    20,
    128,
    36,
    121,
    20,
    128,
    220,
    123,
    20,
    128,
    208,
    126,
    20,
    128,
    192,
    128,
    20,
    128,
    204,
    114,
    20,
    128,
};

static TmdBone _gActor460200SoldierCBodySkeleton[20] = {
#include "assets/soldier_c_body_skeleton.inc"
};

static u32 _gActor460200SoldierCBodyPartVerts[20] = {
#include "assets/soldier_c_body_partVerts.inc"
};

static SVECTOR _gActor460200SoldierCBodyVerts[373] = {
#include "assets/soldier_c_body_verts.inc"
};

static SVECTOR _gActor460200SoldierCBodyNormals[398] = {
#include "assets/soldier_c_body_normals.inc"
};

static u32 _gActor460200SoldierCBodyStream[4031] = {
#include "assets/soldier_c_body_stream.inc"
};

static TmdSource _gActor460200SoldierCBody = {
    0,
    22164,
    6060,
    20,
    _gActor460200SoldierCBodyPartVerts,
    _gActor460200SoldierCBodyVerts,
    _gActor460200SoldierCBodyNormals,
    _gActor460200SoldierCBodySkeleton,
    _gActor460200SoldierCBodyStream,
};

static AnimationPackedPose _gActor460200Animation1C038Bank1[5] = {
#include "assets/actor_460200_animation_1C038_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1C038Bank4[56] = {
#include "assets/actor_460200_animation_1C038_bank4.inc"
};

static AnimationRecord _gActor460200Animation1C038Records[87] = {
#include "assets/actor_460200_animation_1C038_records.inc"
};

static u16 _gActor460200Animation1C038Indices[20] = {
#include "assets/actor_460200_animation_1C038_indices.inc"
};

static AnimationSet _gActor460200Animation1C038 = {
    _gActor460200Animation1C038Records,
    _gActor460200Animation1C038Indices,
    { NULL, _gActor460200Animation1C038Bank1, NULL, NULL, _gActor460200Animation1C038Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1C304Bank1[3] = {
#include "assets/actor_460200_animation_1C304_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1C304Bank4[33] = {
#include "assets/actor_460200_animation_1C304_bank4.inc"
};

static AnimationRecord _gActor460200Animation1C304Records[117] = {
#include "assets/actor_460200_animation_1C304_records.inc"
};

static u16 _gActor460200Animation1C304Indices[20] = {
#include "assets/actor_460200_animation_1C304_indices.inc"
};

static AnimationSet _gActor460200Animation1C304 = {
    _gActor460200Animation1C304Records,
    _gActor460200Animation1C304Indices,
    { NULL, _gActor460200Animation1C304Bank1, NULL, NULL, _gActor460200Animation1C304Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1C650Bank1[4] = {
#include "assets/actor_460200_animation_1C650_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1C650Bank4[41] = {
#include "assets/actor_460200_animation_1C650_bank4.inc"
};

static AnimationRecord _gActor460200Animation1C650Records[138] = {
#include "assets/actor_460200_animation_1C650_records.inc"
};

static u16 _gActor460200Animation1C650Indices[20] = {
#include "assets/actor_460200_animation_1C650_indices.inc"
};

static AnimationSet _gActor460200Animation1C650 = {
    _gActor460200Animation1C650Records,
    _gActor460200Animation1C650Indices,
    { NULL, _gActor460200Animation1C650Bank1, NULL, NULL, _gActor460200Animation1C650Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1C82CBank1[3] = {
#include "assets/actor_460200_animation_1C82C_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1C82CBank4[30] = {
#include "assets/actor_460200_animation_1C82C_bank4.inc"
};

static AnimationRecord _gActor460200Animation1C82CRecords[60] = {
#include "assets/actor_460200_animation_1C82C_records.inc"
};

static u16 _gActor460200Animation1C82CIndices[20] = {
#include "assets/actor_460200_animation_1C82C_indices.inc"
};

static AnimationSet _gActor460200Animation1C82C = {
    _gActor460200Animation1C82CRecords,
    _gActor460200Animation1C82CIndices,
    { NULL, _gActor460200Animation1C82CBank1, NULL, NULL, _gActor460200Animation1C82CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1CD58Bank1[4] = {
#include "assets/actor_460200_animation_1CD58_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1CD58Bank4[79] = {
#include "assets/actor_460200_animation_1CD58_bank4.inc"
};

static AnimationRecord _gActor460200Animation1CD58Records[220] = {
#include "assets/actor_460200_animation_1CD58_records.inc"
};

static u16 _gActor460200Animation1CD58Indices[20] = {
#include "assets/actor_460200_animation_1CD58_indices.inc"
};

static AnimationSet _gActor460200Animation1CD58 = {
    _gActor460200Animation1CD58Records,
    _gActor460200Animation1CD58Indices,
    { NULL, _gActor460200Animation1CD58Bank1, NULL, NULL, _gActor460200Animation1CD58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1CF40Bank1[3] = {
#include "assets/actor_460200_animation_1CF40_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1CF40Bank4[33] = {
#include "assets/actor_460200_animation_1CF40_bank4.inc"
};

static AnimationRecord _gActor460200Animation1CF40Records[60] = {
#include "assets/actor_460200_animation_1CF40_records.inc"
};

static u16 _gActor460200Animation1CF40Indices[20] = {
#include "assets/actor_460200_animation_1CF40_indices.inc"
};

static AnimationSet _gActor460200Animation1CF40 = {
    _gActor460200Animation1CF40Records,
    _gActor460200Animation1CF40Indices,
    { NULL, _gActor460200Animation1CF40Bank1, NULL, NULL, _gActor460200Animation1CF40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1D300Bank1[5] = {
#include "assets/actor_460200_animation_1D300_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1D300Bank4[63] = {
#include "assets/actor_460200_animation_1D300_bank4.inc"
};

static AnimationRecord _gActor460200Animation1D300Records[142] = {
#include "assets/actor_460200_animation_1D300_records.inc"
};

static u16 _gActor460200Animation1D300Indices[20] = {
#include "assets/actor_460200_animation_1D300_indices.inc"
};

static AnimationSet _gActor460200Animation1D300 = {
    _gActor460200Animation1D300Records,
    _gActor460200Animation1D300Indices,
    { NULL, _gActor460200Animation1D300Bank1, NULL, NULL, _gActor460200Animation1D300Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1D4D8Bank1[2] = {
#include "assets/actor_460200_animation_1D4D8_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1D4D8Bank4[23] = {
#include "assets/actor_460200_animation_1D4D8_bank4.inc"
};

static AnimationRecord _gActor460200Animation1D4D8Records[69] = {
#include "assets/actor_460200_animation_1D4D8_records.inc"
};

static u16 _gActor460200Animation1D4D8Indices[20] = {
#include "assets/actor_460200_animation_1D4D8_indices.inc"
};

static AnimationSet _gActor460200Animation1D4D8 = {
    _gActor460200Animation1D4D8Records,
    _gActor460200Animation1D4D8Indices,
    { NULL, _gActor460200Animation1D4D8Bank1, NULL, NULL, _gActor460200Animation1D4D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1D8D4Bank1[3] = {
#include "assets/actor_460200_animation_1D8D4_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1D8D4Bank4[72] = {
#include "assets/actor_460200_animation_1D8D4_bank4.inc"
};

static AnimationRecord _gActor460200Animation1D8D4Records[154] = {
#include "assets/actor_460200_animation_1D8D4_records.inc"
};

static u16 _gActor460200Animation1D8D4Indices[20] = {
#include "assets/actor_460200_animation_1D8D4_indices.inc"
};

static AnimationSet _gActor460200Animation1D8D4 = {
    _gActor460200Animation1D8D4Records,
    _gActor460200Animation1D8D4Indices,
    { NULL, _gActor460200Animation1D8D4Bank1, NULL, NULL, _gActor460200Animation1D8D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1DAC0Bank1[3] = {
#include "assets/actor_460200_animation_1DAC0_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1DAC0Bank4[34] = {
#include "assets/actor_460200_animation_1DAC0_bank4.inc"
};

static AnimationRecord _gActor460200Animation1DAC0Records[60] = {
#include "assets/actor_460200_animation_1DAC0_records.inc"
};

static u16 _gActor460200Animation1DAC0Indices[20] = {
#include "assets/actor_460200_animation_1DAC0_indices.inc"
};

static AnimationSet _gActor460200Animation1DAC0 = {
    _gActor460200Animation1DAC0Records,
    _gActor460200Animation1DAC0Indices,
    { NULL, _gActor460200Animation1DAC0Bank1, NULL, NULL, _gActor460200Animation1DAC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1DE20Bank1[4] = {
#include "assets/actor_460200_animation_1DE20_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1DE20Bank4[49] = {
#include "assets/actor_460200_animation_1DE20_bank4.inc"
};

static AnimationRecord _gActor460200Animation1DE20Records[135] = {
#include "assets/actor_460200_animation_1DE20_records.inc"
};

static u16 _gActor460200Animation1DE20Indices[20] = {
#include "assets/actor_460200_animation_1DE20_indices.inc"
};

static AnimationSet _gActor460200Animation1DE20 = {
    _gActor460200Animation1DE20Records,
    _gActor460200Animation1DE20Indices,
    { NULL, _gActor460200Animation1DE20Bank1, NULL, NULL, _gActor460200Animation1DE20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1EA24Bank1[29] = {
#include "assets/actor_460200_animation_1EA24_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1EA24Bank4[279] = {
#include "assets/actor_460200_animation_1EA24_bank4.inc"
};

static AnimationRecord _gActor460200Animation1EA24Records[383] = {
#include "assets/actor_460200_animation_1EA24_records.inc"
};

static u16 _gActor460200Animation1EA24Indices[20] = {
#include "assets/actor_460200_animation_1EA24_indices.inc"
};

static AnimationSet _gActor460200Animation1EA24 = {
    _gActor460200Animation1EA24Records,
    _gActor460200Animation1EA24Indices,
    { NULL, _gActor460200Animation1EA24Bank1, NULL, NULL, _gActor460200Animation1EA24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1EC64Bank1[4] = {
#include "assets/actor_460200_animation_1EC64_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1EC64Bank4[42] = {
#include "assets/actor_460200_animation_1EC64_bank4.inc"
};

static AnimationRecord _gActor460200Animation1EC64Records[70] = {
#include "assets/actor_460200_animation_1EC64_records.inc"
};

static u16 _gActor460200Animation1EC64Indices[20] = {
#include "assets/actor_460200_animation_1EC64_indices.inc"
};

static AnimationSet _gActor460200Animation1EC64 = {
    _gActor460200Animation1EC64Records,
    _gActor460200Animation1EC64Indices,
    { NULL, _gActor460200Animation1EC64Bank1, NULL, NULL, _gActor460200Animation1EC64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1EE5CBank1[2] = {
#include "assets/actor_460200_animation_1EE5C_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1EE5CBank4[26] = {
#include "assets/actor_460200_animation_1EE5C_bank4.inc"
};

static AnimationRecord _gActor460200Animation1EE5CRecords[74] = {
#include "assets/actor_460200_animation_1EE5C_records.inc"
};

static u16 _gActor460200Animation1EE5CIndices[20] = {
#include "assets/actor_460200_animation_1EE5C_indices.inc"
};

static AnimationSet _gActor460200Animation1EE5C = {
    _gActor460200Animation1EE5CRecords,
    _gActor460200Animation1EE5CIndices,
    { NULL, _gActor460200Animation1EE5CBank1, NULL, NULL, _gActor460200Animation1EE5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1F158Bank1[3] = {
#include "assets/actor_460200_animation_1F158_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1F158Bank4[39] = {
#include "assets/actor_460200_animation_1F158_bank4.inc"
};

static AnimationRecord _gActor460200Animation1F158Records[123] = {
#include "assets/actor_460200_animation_1F158_records.inc"
};

static u16 _gActor460200Animation1F158Indices[20] = {
#include "assets/actor_460200_animation_1F158_indices.inc"
};

static AnimationSet _gActor460200Animation1F158 = {
    _gActor460200Animation1F158Records,
    _gActor460200Animation1F158Indices,
    { NULL, _gActor460200Animation1F158Bank1, NULL, NULL, _gActor460200Animation1F158Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1F478Bank1[3] = {
#include "assets/actor_460200_animation_1F478_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1F478Bank4[52] = {
#include "assets/actor_460200_animation_1F478_bank4.inc"
};

static AnimationRecord _gActor460200Animation1F478Records[119] = {
#include "assets/actor_460200_animation_1F478_records.inc"
};

static u16 _gActor460200Animation1F478Indices[20] = {
#include "assets/actor_460200_animation_1F478_indices.inc"
};

static AnimationSet _gActor460200Animation1F478 = {
    _gActor460200Animation1F478Records,
    _gActor460200Animation1F478Indices,
    { NULL, _gActor460200Animation1F478Bank1, NULL, NULL, _gActor460200Animation1F478Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor460200Animation1F6B4Bank1[2] = {
#include "assets/actor_460200_animation_1F6B4_bank1.inc"
};

static AnimationPackedRotation _gActor460200Animation1F6B4Bank4[34] = {
#include "assets/actor_460200_animation_1F6B4_bank4.inc"
};

static AnimationRecord _gActor460200Animation1F6B4Records[83] = {
#include "assets/actor_460200_animation_1F6B4_records.inc"
};

static u16 _gActor460200Animation1F6B4Indices[20] = {
#include "assets/actor_460200_animation_1F6B4_indices.inc"
};

static AnimationSet _gActor460200Animation1F6B4 = {
    _gActor460200Animation1F6B4Records,
    _gActor460200Animation1F6B4Indices,
    { NULL, _gActor460200Animation1F6B4Bank1, NULL, NULL, _gActor460200Animation1F6B4Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_actor_460200_801514FC[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_460200_80133C64 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_460200_80133CD0 },
    { ACTOR_MESSAGE_PLACE, _pacedWalkPlaceSoldierC },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_460200_80133DC4 },
    { ACTOR_MESSAGE_WALK_TO, _pacedWalkSetSoldierCWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_460200_8015152C = { { { TASK_BODY_TMD, 96 } }, func_actor_460200_8013386C, { .model = &_gActor460200SoldierCBody } };

s32 D_actor_460200_80151538 = 0;

AnimationSet* D_actor_460200_8015153C[17] = {
    &_gActor460200Animation1C038,
    &_gActor460200Animation1C304,
    &_gActor460200Animation1C650,
    &_gActor460200Animation1C82C,
    &_gActor460200Animation1CD58,
    &_gActor460200Animation1CF40,
    &_gActor460200Animation1D300,
    &_gActor460200Animation1D4D8,
    &_gActor460200Animation1D8D4,
    &_gActor460200Animation1DAC0,
    &_gActor460200Animation1DE20,
    &_gActor460200Animation1EA24,
    &_gActor460200Animation1EC64,
    &_gActor460200Animation1EE5C,
    &_gActor460200Animation1F158,
    &_gActor460200Animation1F478,
    &_gActor460200Animation1F6B4,
};

void func_actor_460200_80132210(void);
void func_actor_460200_801322B8(void);
void func_actor_460200_80132390(void);

#include "../../shared/screen_negative_capture.inc.c"

/// The scene's negative freeze-frame (see screen_negative.h).
void func_actor_460200_80131E24(Task* task)
{
    screenNegativeCaptureTask(task);
}

#include "../../shared/screen_negative_filter.inc.c"

void func_actor_460200_80132090(Task* arg0)
{
    s32 var_v0;

    var_v0 = arg0->spawnArg1.value;
    if (var_v0 < 0) {
        stageRequestModeTaskExit();
        taskKill(arg0);
        var_v0 = arg0->spawnArg1.value;
    }
    var_v0                = var_v0 - 1;
    arg0->spawnArg1.value = var_v0;
}

void func_actor_460200_801320E0(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        capSelectLoadedFile(arg0);
        capSetTexturePage(0x340, 0);
        return;
    }
    capReset();
}

/// The same filter under the name the cutscene script's command 13 calls.
#define screenNegativeFilter func_actor_460200_80132124
#include "../../shared/screen_negative_filter.inc.c"
#undef screenNegativeFilter

void func_actor_460200_80132204(s8 arg0)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = arg0;
}

void func_actor_460200_80132210(void)
{
    Task* slot;

    slot = sceneFindPlacedActor(0);
    if (slot != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(slot, 0x7D4, &D_actor_460200_80136234, 0);
        TASK_MESSAGE_DISPATCH_POINTER(slot, 0x7D3, &D_actor_460200_8013607C, 0);
    }
    if (sceneFindPlacedActor(1) != 0) {
        Gp_MsgSlot4Chain(1, 2);
    }
    slot = sceneFindPlacedActor(2);
    if (slot != NULL) {
        Gp_MsgSlot4Chain(2, 1);
        TASK_MESSAGE_DISPATCH_POINTER(slot, 0x7D3, &D_actor_460200_80135F14, 0);
    }
}

void func_actor_460200_801322B8(void)
{
    switch (gameFlagGetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_A)) {
        case 0:
            evsStartScript(D_actor_460200_80137AA0, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_A, 1);
            break;
        case 1:
            evsStartScript(D_actor_460200_80137BA8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_A, 2);
            break;
        case 2:
            evsStartScript(D_actor_460200_80137CB0, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_A, 3);
            break;
        case 3:
            evsStartScript(D_actor_460200_80137DA0, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

void func_actor_460200_80132390(void)
{
    switch (gameFlagGetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_B)) {
        case 0:
            evsStartScript(D_actor_460200_80137F98, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_B, 1);
            break;
        case 1:
            evsStartScript(D_actor_460200_80137FE0, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_B, 2);
            break;
        case 2:
            evsStartScript(D_actor_460200_80138028, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_C_TALK_COUNT_B, 3);
            break;
        case 3:
            evsStartScript(D_actor_460200_80138070, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/paced_walk_frame.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/paced_walk_update.inc.c"

void func_actor_460200_801327B4(Task* task)
{
    EnemyTaskFunc fns[2] = { pacedWalkSpawn, pacedWalkFrame };

    fns[task->state](task->spawnArg2.pointer, task);
}

#include "../../shared/paced_walk_spawn.inc.c"

void pacedWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/paced_walk_tick_anim.inc.c"

#include "../../shared/paced_walk_reset_anim.inc.c"

#include "../../shared/paced_walk_blend_anim.inc.c"

#include "../../shared/paced_walk_play_anim.inc.c"

#include "../../shared/paced_walk_show_pair.inc.c"

#include "../../shared/paced_walk_place.inc.c"

/// Script opcode: raise the work block's `smoking`, which makes the per-frame
/// state emit smoke puffs, when the payload is exactly 1. Any other payload is
/// ignored and leaves the flag as it was.
s32 func_actor_460200_80132C8C(Task* task, s32 arg1, ActorCommand* args, s32 arg3)
{
    PacedWalkWork* work;
    u16            value;

    value = args->command;
    work  = task->work;
    if (value == 1) {
        work->smoking = value;
    }
    return 0;
}

#include "../../shared/paced_walk_to.inc.c"

#include "../../shared/stride_walk_spawn.inc.c"

#undef PACED_WALK_BLEND_ANIM
#define PACED_WALK_BLEND_ANIM _pacedWalkBlendSoldierBAnim
#undef PACED_WALK_RESET_ANIM
#define PACED_WALK_RESET_ANIM _pacedWalkResetSoldierBAnim
#undef PACED_WALK_TICK_ANIM
// Soldier B's stride update uses its own private slot tick.
#define PACED_WALK_TICK_ANIM _pacedWalkTickSoldierBAnim
#include "../../shared/stride_walk_update.inc.c"
#undef PACED_WALK_BLEND_ANIM
#undef PACED_WALK_RESET_ANIM
#undef PACED_WALK_TICK_ANIM

void func_actor_460200_801330C8(Task* task)
{
    EnemyTaskFunc fns[2] = { strideWalkSpawn, _strideWalkFrame };

    fns[task->state](task->spawnArg2.pointer, task);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawSecondFixedWalkerGroundShadow
#include "../../shared/stride_walk_frame.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

void strideWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawSecondFixedWalkerGroundShadow
#include "../../shared/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

// The second walker is a stride walker, so its copies of the paced walk
// helpers run on that walker's block.
#undef PACED_WALK_WORK_T
#define PACED_WALK_WORK_T StrideWalkWork

// Soldier B's tick uses the StrideWalkWork binding above.
#define PACED_WALK_TICK_ANIM _pacedWalkTickSoldierBAnim
#include "../../shared/paced_walk_tick_anim.inc.c"
#undef PACED_WALK_TICK_ANIM

// Soldier B's reset uses the StrideWalkWork binding above.
#define PACED_WALK_RESET_ANIM _pacedWalkResetSoldierBAnim
#include "../../shared/paced_walk_reset_anim.inc.c"
#undef PACED_WALK_RESET_ANIM

// Soldier B's blend uses the StrideWalkWork binding above.
#define PACED_WALK_BLEND_ANIM _pacedWalkBlendSoldierBAnim
#include "../../shared/paced_walk_blend_anim.inc.c"
#undef PACED_WALK_BLEND_ANIM

#include "../../shared/stride_walk_play.inc.c"

#include "../../shared/stride_walk_visibility.inc.c"

// Soldier B's placement uses the StrideWalkWork binding above.
#undef PACED_WALK_PLACE
#define PACED_WALK_PLACE _pacedWalkPlaceSoldierB
#include "../../shared/paced_walk_place.inc.c"
#undef PACED_WALK_PLACE
#define PACED_WALK_PLACE _pacedWalkPlace

#undef PACED_WALK_WORK_T
#define PACED_WALK_WORK_T PacedWalkWork

/// Turn command: sets the work block's `turnMode`, which selects whether the
/// per-frame state turns the walker's head toward the player
/// (`STRIDE_WALK_TURN_PLAYER`) or lets it settle back, to the command's value.
s32 func_actor_460200_80133568(Task* task, s32 arg1, ActorCommand* args, s32 arg3)
{
    StrideWalkWork* work = task->work;

    work->turnMode = args->command;
    return 0;
}

#include "../../shared/stride_walk_to.inc.c"

#include "../../shared/stride_walk_sub_model.inc.c"

#undef PACED_WALK_UPDATE
// Soldier C's update and animation helpers operate on its PacedWalkWork.
#define PACED_WALK_UPDATE     _pacedWalkUpdateSoldierC
#define PACED_WALK_TICK_ANIM  _pacedWalkTickSoldierCAnim
#define PACED_WALK_RESET_ANIM _pacedWalkResetSoldierCAnim
#define PACED_WALK_BLEND_ANIM _pacedWalkBlendSoldierCAnim
#include "../../shared/paced_walk_update.inc.c"
#undef PACED_WALK_UPDATE
#define PACED_WALK_UPDATE _pacedWalkUpdate
#undef PACED_WALK_TICK_ANIM
#undef PACED_WALK_RESET_ANIM
#undef PACED_WALK_BLEND_ANIM

void func_actor_460200_8013386C(Task* task)
{
    EnemyTaskFunc fns[2] = { func_actor_460200_801338C0, func_actor_460200_80133A04 };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Spawn routine of the actor whose `func_actor_460200_80133A88` exit path
/// hands it back to `enemyDestroy`: it allocates the `PacedWalkWork` block (the
/// matrix pair its sub-model reads through `TmdObject::lightMtx`/`colorMtx`
/// plus the animation state below), parks the enemy in `PacedWalkWork::enemy`
/// and runs the step body `_pacedWalkUpdateSoldierC` once in state 2.
static void func_actor_460200_801338C0(Enemy* enemy, Task* task)
{
    PacedWalkWork* work;
    void*          workMem;
    TmdObject*     obj;
    GfxCoord*      coord;
    VECTOR         vec;

    obj     = task->extra.tmd;
    coord   = obj->coords;
    workMem = memCalloc(sizeof(PacedWalkWork), 0);
    work    = workMem;
    if ((task->work = work) == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_460200_80133A88;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->flags                       = 0;
    obj->otOffset                    = 1;
    work->enemy                      = enemy;
    work->st.animId                  = 2;
    obj->lightMtx                    = &work->light;
    obj->colorMtx                    = &work->color;
    vec.vx                           = coord->workm.t[0];
    vec.vy                           = coord->workm.t[1] - 0x320;
    vec.vz                           = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationInitContext(&work->rig.anim, (AnimationSet**)&D_actor_460200_80151538, obj, work->rig.poses, work->rig.slots);
    work->st.state = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable = D_actor_460200_801514FC;
    _pacedWalkUpdateSoldierC(task);
    task->state += 1;
}

#define walkerFrame                            func_actor_460200_80133A04
#define walkerUpdate                           _pacedWalkUpdateSoldierC
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawThirdFixedWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

static void func_actor_460200_80133A88(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawThirdFixedWalkerGroundShadow
#include "../../shared/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

// Soldier C's tick uses the restored PacedWalkWork binding.
#define PACED_WALK_TICK_ANIM _pacedWalkTickSoldierCAnim
#include "../../shared/paced_walk_tick_anim.inc.c"
#undef PACED_WALK_TICK_ANIM

// Soldier C's reset uses the restored PacedWalkWork binding.
#define PACED_WALK_RESET_ANIM _pacedWalkResetSoldierCAnim
#include "../../shared/paced_walk_reset_anim.inc.c"
#undef PACED_WALK_RESET_ANIM

// Soldier C's blend uses the restored PacedWalkWork binding.
#define PACED_WALK_BLEND_ANIM _pacedWalkBlendSoldierCAnim
#include "../../shared/paced_walk_blend_anim.inc.c"
#undef PACED_WALK_BLEND_ANIM

s32 func_actor_460200_80133C64(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    PacedWalkWork* work;

    work = task->work;
    if (args->animationId < 0x12) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
            work->blendFrames = args->blendFrames;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        work->st.field_6 = 0;
        _pacedWalkUpdateSoldierC(task);
        return 0;
    }
    return -1;
}

/// A further copy, under this file's own name.
#define pacedWalkShowPair func_actor_460200_80133CD0
#include "../../shared/paced_walk_show_pair.inc.c"
#undef pacedWalkShowPair

// Soldier C's placement uses the restored PacedWalkWork binding.
#undef PACED_WALK_PLACE
#define PACED_WALK_PLACE _pacedWalkPlaceSoldierC
#include "../../shared/paced_walk_place.inc.c"
#undef PACED_WALK_PLACE
#define PACED_WALK_PLACE _pacedWalkPlace

s32 func_actor_460200_80133DC4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

// Soldier C records travel in its own PacedWalkWork.
#undef PACED_WALK_SET_WALK_TARGET
#define PACED_WALK_SET_WALK_TARGET _pacedWalkSetSoldierCWalkTarget
#include "../../shared/paced_walk_to.inc.c"
#undef PACED_WALK_SET_WALK_TARGET
#define PACED_WALK_SET_WALK_TARGET pacedWalkTo
