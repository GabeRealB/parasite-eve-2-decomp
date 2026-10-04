#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/paced_walk.h"
#include "../../shared/walker.h"

/// The clips this actor's meeting scenes add to the player's animation bank,
/// with the copy request that installs them and two play requests.
///
/// Each of the four scene scripts first sends the player the copy request.
/// The copy takes `ANIMATION_BANK_EXTENSION_CAPACITY` words from the start of
/// the storage, which is more than the clip table holds: the 23 table words
/// occupy extended ids 47-69, and the copy request itself and the first seven
/// words of the play requests are written into the bank after them. The
/// scripts play base id 1 and extended ids 47-65 only, so neither a NULL entry
/// nor a word following the table is played as a clip.
///
/// The two play requests open the run of requests the package keeps for the
/// player and belong to this object only because the copied span ends inside
/// the second; the requests for the extended ids follow as separate objects.
/// The storage is only read: the event script resolves a request's bank in a
/// copy of it.
typedef union {
    struct {
        AnimationSet*            sets[23];        // Player clips for extended ids 47-65, then four NULL entries (ids 66-69) no script plays
        AnimationBankCopyRequest copy;            // Installs the first 32 words of this storage in the player's bank extension
        AnimationPlayRequest     playRequests[2]; // Both select base id 1: the first restarts it, the second blends into it over 15 frames
    } data;                                       // The records by name
    s32 words[35];                                // The same storage as the copy reads it; the last three words lie beyond the copied span
} _Actor160700AnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor160700AnimationBankExtensionStorage, 140);

extern _Actor160700AnimationBankExtensionStorage D_actor_160700_801351E8;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern TaskDesc D_actor_160700_801416A8[];
extern u8       D_actor_160700_801416C0[];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_160700_80141678[6];

extern AnimationPlayRequest D_actor_160700_801354CC;
extern EvsCommand           D_actor_160700_80135664[];
extern EvsCommand           D_actor_160700_80135ACC[];
extern EvsCommand           D_actor_160700_80135BD4[];
extern EvsCommand           D_actor_160700_801362F4[];
extern EvsCommand           D_actor_160700_80136414[];

static void func_actor_160700_80132390(Enemy* enemy, Task* task);
static void func_actor_160700_80132414(Task* task);

extern AnimationPlayRequest D_actor_160700_80135288;
extern AnimationPlayRequest D_actor_160700_8013529C;
extern AnimationPlayRequest D_actor_160700_801352B0;
extern AnimationPlayRequest D_actor_160700_801352C4;
extern AnimationPlayRequest D_actor_160700_801352D8;
extern AnimationPlayRequest D_actor_160700_801352EC;
extern AnimationPlayRequest D_actor_160700_80135300;
extern AnimationPlayRequest D_actor_160700_80135314;
extern AnimationPlayRequest D_actor_160700_80135328;
extern AnimationPlayRequest D_actor_160700_8013533C;
extern AnimationPlayRequest D_actor_160700_80135350;
extern AnimationPlayRequest D_actor_160700_80135364;
extern AnimationPlayRequest D_actor_160700_80135378;
extern AnimationPlayRequest D_actor_160700_8013538C;
extern AnimationPlayRequest D_actor_160700_801353A0;
extern AnimationPlayRequest D_actor_160700_801353B4;
extern AnimationPlayRequest D_actor_160700_801353C8;
extern AnimationPlayRequest D_actor_160700_801353DC;
extern AnimationPlayRequest D_actor_160700_801353F0;
extern AnimationPlayRequest D_actor_160700_8013547C;
extern AnimationPlayRequest D_actor_160700_80135490;
extern AnimationPlayRequest D_actor_160700_801354A4;
extern AnimationPlayRequest D_actor_160700_801354B8;

static TmdSource _gActor160700PierceCarradineBody;
static TmdSource _gActor160700Actor113100Model07960;
s32              func_actor_160700_801325F0(Task*, s32, AnimationPlayRequest*, s32);
s32              func_actor_160700_8013265C(Task*, s32, s32, s32);
s32              func_actor_160700_80132738(Task*, s32, s32, s32);
void             func_actor_160700_8013233C(Task*);
void             func_actor_160700_80132808(Task*);

static AnimationPackedPose _gActor160700Animation00C90Bank1[2] = {
#include "assets/actor_160700_animation_00C90_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation00C90Bank4[26] = {
#include "assets/actor_160700_animation_00C90_bank4.inc"
};

static AnimationRecord _gActor160700Animation00C90Records[99] = {
#include "assets/actor_160700_animation_00C90_records.inc"
};

static u16 _gActor160700Animation00C90Indices[20] = {
#include "assets/actor_160700_animation_00C90_indices.inc"
};

static AnimationSet _gActor160700Animation00C90 = {
    _gActor160700Animation00C90Records,
    _gActor160700Animation00C90Indices,
    { NULL, _gActor160700Animation00C90Bank1, NULL, NULL, _gActor160700Animation00C90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation00EC0Bank1[3] = {
#include "assets/actor_160700_animation_00EC0_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation00EC0Bank4[30] = {
#include "assets/actor_160700_animation_00EC0_bank4.inc"
};

static AnimationRecord _gActor160700Animation00EC0Records[81] = {
#include "assets/actor_160700_animation_00EC0_records.inc"
};

static u16 _gActor160700Animation00EC0Indices[20] = {
#include "assets/actor_160700_animation_00EC0_indices.inc"
};

static AnimationSet _gActor160700Animation00EC0 = {
    _gActor160700Animation00EC0Records,
    _gActor160700Animation00EC0Indices,
    { NULL, _gActor160700Animation00EC0Bank1, NULL, NULL, _gActor160700Animation00EC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation01184Bank1[3] = {
#include "assets/actor_160700_animation_01184_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation01184Bank4[36] = {
#include "assets/actor_160700_animation_01184_bank4.inc"
};

static AnimationRecord _gActor160700Animation01184Records[112] = {
#include "assets/actor_160700_animation_01184_records.inc"
};

static u16 _gActor160700Animation01184Indices[20] = {
#include "assets/actor_160700_animation_01184_indices.inc"
};

static AnimationSet _gActor160700Animation01184 = {
    _gActor160700Animation01184Records,
    _gActor160700Animation01184Indices,
    { NULL, _gActor160700Animation01184Bank1, NULL, NULL, _gActor160700Animation01184Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0135CBank1[3] = {
#include "assets/actor_160700_animation_0135C_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0135CBank4[32] = {
#include "assets/actor_160700_animation_0135C_bank4.inc"
};

static AnimationRecord _gActor160700Animation0135CRecords[57] = {
#include "assets/actor_160700_animation_0135C_records.inc"
};

static u16 _gActor160700Animation0135CIndices[20] = {
#include "assets/actor_160700_animation_0135C_indices.inc"
};

static AnimationSet _gActor160700Animation0135C = {
    _gActor160700Animation0135CRecords,
    _gActor160700Animation0135CIndices,
    { NULL, _gActor160700Animation0135CBank1, NULL, NULL, _gActor160700Animation0135CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation01530Bank1[3] = {
#include "assets/actor_160700_animation_01530_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation01530Bank4[31] = {
#include "assets/actor_160700_animation_01530_bank4.inc"
};

static AnimationRecord _gActor160700Animation01530Records[57] = {
#include "assets/actor_160700_animation_01530_records.inc"
};

static u16 _gActor160700Animation01530Indices[20] = {
#include "assets/actor_160700_animation_01530_indices.inc"
};

static AnimationSet _gActor160700Animation01530 = {
    _gActor160700Animation01530Records,
    _gActor160700Animation01530Indices,
    { NULL, _gActor160700Animation01530Bank1, NULL, NULL, _gActor160700Animation01530Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation017A0Bank1[3] = {
#include "assets/actor_160700_animation_017A0_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation017A0Bank4[29] = {
#include "assets/actor_160700_animation_017A0_bank4.inc"
};

static AnimationRecord _gActor160700Animation017A0Records[98] = {
#include "assets/actor_160700_animation_017A0_records.inc"
};

static u16 _gActor160700Animation017A0Indices[20] = {
#include "assets/actor_160700_animation_017A0_indices.inc"
};

static AnimationSet _gActor160700Animation017A0 = {
    _gActor160700Animation017A0Records,
    _gActor160700Animation017A0Indices,
    { NULL, _gActor160700Animation017A0Bank1, NULL, NULL, _gActor160700Animation017A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation01A4CBank1[3] = {
#include "assets/actor_160700_animation_01A4C_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation01A4CBank4[32] = {
#include "assets/actor_160700_animation_01A4C_bank4.inc"
};

static AnimationRecord _gActor160700Animation01A4CRecords[110] = {
#include "assets/actor_160700_animation_01A4C_records.inc"
};

static u16 _gActor160700Animation01A4CIndices[20] = {
#include "assets/actor_160700_animation_01A4C_indices.inc"
};

static AnimationSet _gActor160700Animation01A4C = {
    _gActor160700Animation01A4CRecords,
    _gActor160700Animation01A4CIndices,
    { NULL, _gActor160700Animation01A4CBank1, NULL, NULL, _gActor160700Animation01A4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation01C18Bank1[3] = {
#include "assets/actor_160700_animation_01C18_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation01C18Bank4[29] = {
#include "assets/actor_160700_animation_01C18_bank4.inc"
};

static AnimationRecord _gActor160700Animation01C18Records[57] = {
#include "assets/actor_160700_animation_01C18_records.inc"
};

static u16 _gActor160700Animation01C18Indices[20] = {
#include "assets/actor_160700_animation_01C18_indices.inc"
};

static AnimationSet _gActor160700Animation01C18 = {
    _gActor160700Animation01C18Records,
    _gActor160700Animation01C18Indices,
    { NULL, _gActor160700Animation01C18Bank1, NULL, NULL, _gActor160700Animation01C18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation01E54Bank1[2] = {
#include "assets/actor_160700_animation_01E54_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation01E54Bank4[35] = {
#include "assets/actor_160700_animation_01E54_bank4.inc"
};

static AnimationRecord _gActor160700Animation01E54Records[82] = {
#include "assets/actor_160700_animation_01E54_records.inc"
};

static u16 _gActor160700Animation01E54Indices[20] = {
#include "assets/actor_160700_animation_01E54_indices.inc"
};

static AnimationSet _gActor160700Animation01E54 = {
    _gActor160700Animation01E54Records,
    _gActor160700Animation01E54Indices,
    { NULL, _gActor160700Animation01E54Bank1, NULL, NULL, _gActor160700Animation01E54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02024Bank1[3] = {
#include "assets/actor_160700_animation_02024_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02024Bank4[30] = {
#include "assets/actor_160700_animation_02024_bank4.inc"
};

static AnimationRecord _gActor160700Animation02024Records[57] = {
#include "assets/actor_160700_animation_02024_records.inc"
};

static u16 _gActor160700Animation02024Indices[20] = {
#include "assets/actor_160700_animation_02024_indices.inc"
};

static AnimationSet _gActor160700Animation02024 = {
    _gActor160700Animation02024Records,
    _gActor160700Animation02024Indices,
    { NULL, _gActor160700Animation02024Bank1, NULL, NULL, _gActor160700Animation02024Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02284Bank1[3] = {
#include "assets/actor_160700_animation_02284_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02284Bank4[26] = {
#include "assets/actor_160700_animation_02284_bank4.inc"
};

static AnimationRecord _gActor160700Animation02284Records[97] = {
#include "assets/actor_160700_animation_02284_records.inc"
};

static u16 _gActor160700Animation02284Indices[20] = {
#include "assets/actor_160700_animation_02284_indices.inc"
};

static AnimationSet _gActor160700Animation02284 = {
    _gActor160700Animation02284Records,
    _gActor160700Animation02284Indices,
    { NULL, _gActor160700Animation02284Bank1, NULL, NULL, _gActor160700Animation02284Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02454Bank1[3] = {
#include "assets/actor_160700_animation_02454_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02454Bank4[30] = {
#include "assets/actor_160700_animation_02454_bank4.inc"
};

static AnimationRecord _gActor160700Animation02454Records[57] = {
#include "assets/actor_160700_animation_02454_records.inc"
};

static u16 _gActor160700Animation02454Indices[20] = {
#include "assets/actor_160700_animation_02454_indices.inc"
};

static AnimationSet _gActor160700Animation02454 = {
    _gActor160700Animation02454Records,
    _gActor160700Animation02454Indices,
    { NULL, _gActor160700Animation02454Bank1, NULL, NULL, _gActor160700Animation02454Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02714Bank1[5] = {
#include "assets/actor_160700_animation_02714_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02714Bank4[54] = {
#include "assets/actor_160700_animation_02714_bank4.inc"
};

static AnimationRecord _gActor160700Animation02714Records[87] = {
#include "assets/actor_160700_animation_02714_records.inc"
};

static u16 _gActor160700Animation02714Indices[20] = {
#include "assets/actor_160700_animation_02714_indices.inc"
};

static AnimationSet _gActor160700Animation02714 = {
    _gActor160700Animation02714Records,
    _gActor160700Animation02714Indices,
    { NULL, _gActor160700Animation02714Bank1, NULL, NULL, _gActor160700Animation02714Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation029ACBank1[3] = {
#include "assets/actor_160700_animation_029AC_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation029ACBank4[28] = {
#include "assets/actor_160700_animation_029AC_bank4.inc"
};

static AnimationRecord _gActor160700Animation029ACRecords[109] = {
#include "assets/actor_160700_animation_029AC_records.inc"
};

static u16 _gActor160700Animation029ACIndices[20] = {
#include "assets/actor_160700_animation_029AC_indices.inc"
};

static AnimationSet _gActor160700Animation029AC = {
    _gActor160700Animation029ACRecords,
    _gActor160700Animation029ACIndices,
    { NULL, _gActor160700Animation029ACBank1, NULL, NULL, _gActor160700Animation029ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02BD4Bank1[2] = {
#include "assets/actor_160700_animation_02BD4_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02BD4Bank4[23] = {
#include "assets/actor_160700_animation_02BD4_bank4.inc"
};

static AnimationRecord _gActor160700Animation02BD4Records[89] = {
#include "assets/actor_160700_animation_02BD4_records.inc"
};

static u16 _gActor160700Animation02BD4Indices[20] = {
#include "assets/actor_160700_animation_02BD4_indices.inc"
};

static AnimationSet _gActor160700Animation02BD4 = {
    _gActor160700Animation02BD4Records,
    _gActor160700Animation02BD4Indices,
    { NULL, _gActor160700Animation02BD4Bank1, NULL, NULL, _gActor160700Animation02BD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02D7CBank1[2] = {
#include "assets/actor_160700_animation_02D7C_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02D7CBank4[23] = {
#include "assets/actor_160700_animation_02D7C_bank4.inc"
};

static AnimationRecord _gActor160700Animation02D7CRecords[57] = {
#include "assets/actor_160700_animation_02D7C_records.inc"
};

static u16 _gActor160700Animation02D7CIndices[20] = {
#include "assets/actor_160700_animation_02D7C_indices.inc"
};

static AnimationSet _gActor160700Animation02D7C = {
    _gActor160700Animation02D7CRecords,
    _gActor160700Animation02D7CIndices,
    { NULL, _gActor160700Animation02D7CBank1, NULL, NULL, _gActor160700Animation02D7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02F38Bank1[3] = {
#include "assets/actor_160700_animation_02F38_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02F38Bank4[19] = {
#include "assets/actor_160700_animation_02F38_bank4.inc"
};

static AnimationRecord _gActor160700Animation02F38Records[63] = {
#include "assets/actor_160700_animation_02F38_records.inc"
};

static u16 _gActor160700Animation02F38Indices[20] = {
#include "assets/actor_160700_animation_02F38_indices.inc"
};

static AnimationSet _gActor160700Animation02F38 = {
    _gActor160700Animation02F38Records,
    _gActor160700Animation02F38Indices,
    { NULL, _gActor160700Animation02F38Bank1, NULL, NULL, _gActor160700Animation02F38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation030F4Bank1[3] = {
#include "assets/actor_160700_animation_030F4_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation030F4Bank4[19] = {
#include "assets/actor_160700_animation_030F4_bank4.inc"
};

static AnimationRecord _gActor160700Animation030F4Records[63] = {
#include "assets/actor_160700_animation_030F4_records.inc"
};

static u16 _gActor160700Animation030F4Indices[20] = {
#include "assets/actor_160700_animation_030F4_indices.inc"
};

static AnimationSet _gActor160700Animation030F4 = {
    _gActor160700Animation030F4Records,
    _gActor160700Animation030F4Indices,
    { NULL, _gActor160700Animation030F4Bank1, NULL, NULL, _gActor160700Animation030F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation033A0Bank1[3] = {
#include "assets/actor_160700_animation_033A0_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation033A0Bank4[55] = {
#include "assets/actor_160700_animation_033A0_bank4.inc"
};

static AnimationRecord _gActor160700Animation033A0Records[87] = {
#include "assets/actor_160700_animation_033A0_records.inc"
};

static u16 _gActor160700Animation033A0Indices[20] = {
#include "assets/actor_160700_animation_033A0_indices.inc"
};

static AnimationSet _gActor160700Animation033A0 = {
    _gActor160700Animation033A0Records,
    _gActor160700Animation033A0Indices,
    { NULL, _gActor160700Animation033A0Bank1, NULL, NULL, _gActor160700Animation033A0Bank4, NULL, NULL, NULL },
};

_Actor160700AnimationBankExtensionStorage D_actor_160700_801351E8 = { .data = { { &_gActor160700Animation00C90, &_gActor160700Animation00EC0, &_gActor160700Animation01184, &_gActor160700Animation0135C, &_gActor160700Animation01530, &_gActor160700Animation017A0, &_gActor160700Animation01A4C, &_gActor160700Animation01C18, &_gActor160700Animation01E54, &_gActor160700Animation02024, &_gActor160700Animation02284, &_gActor160700Animation02454, &_gActor160700Animation02714, &_gActor160700Animation029AC, &_gActor160700Animation02BD4, &_gActor160700Animation02D7C, &_gActor160700Animation02F38, &_gActor160700Animation030F4, &_gActor160700Animation033A0, NULL, NULL, NULL, NULL }, { { .words = D_actor_160700_801351E8.words }, ANIMATION_BANK_EXTENSION_CAPACITY }, { { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_160700_80135274 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135288 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_8013529C = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801352B0 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801352C4 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801352D8 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801352EC = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135300 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135314 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135328 = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_8013533C = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135350 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135364 = { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135378 = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_8013538C = { { .index = 1 }, 60, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801353A0 = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801353B4 = { { .index = 1 }, 62, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801353C8 = { { .index = 1 }, 63, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801353DC = { { .index = 1 }, 64, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801353F0 = { { .index = 1 }, 65, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135404[6] = {
    { { .index = 1 }, 66, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 67, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 68, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 69, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_160700_8013547C = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135490 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801354A4 = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801354B8 = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801354CC = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801354E0 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801354F4 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135508 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_8013551C = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135530 = { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135544 = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135558 = { { .index = 1 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_8013556C = { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135580 = { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135594 = { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801355A8 = { { .index = 1 }, 17, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801355BC = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801355D0 = { { .index = 1 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801355E4 = { { .index = 1 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_801355F8 = { { .index = 1 }, 21, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_8013560C = { { .index = 1 }, 22, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160700_80135620 = { { .index = 1 }, 23, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_160700_80135634 = { { 2250, 0, 790, 0 }, { 0, -853, 0, 0 } };

ActorTransform D_actor_160700_8013564C = { { 2000, 0, 750, 0 }, { 0, -739, 0, 0 } };

EvsCommand D_actor_160700_80135664[47] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &D_actor_160700_801351E8.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.playRequests[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_160700_80135634 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_8013529C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135288 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_8013547C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_8013560C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_80135620 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801352B0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801352C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135288 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_80135490 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801352D8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801352EC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135300 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135314 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801352EC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.playRequests[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_160700_80135ACC[11] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_160700_80135634 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.playRequests[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_160700_80135BD4[76] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &D_actor_160700_801351E8.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.playRequests[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_160700_80135634 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801352EC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_80135508 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_8013551C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135328 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_80135530 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_8013533C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135350 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_80135544 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_80135558 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_8013556C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_80135580 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135364 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_80135594 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801355A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_80135378 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_160700_8013564C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801353A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801353B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_80135530 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_8013538C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801355BC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801355D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801355E4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801353C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801355F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801353DC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801353F0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.playRequests[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_160700_801362F4[12] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &D_actor_160700_801351E8.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801355D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801355E4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_160700_80136414[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &D_actor_160700_801351E8.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160700_801351E8.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_80135530 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801355BC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160700_801354CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor160700PierceCarradineBodySkeleton[20] = {
#include "assets/pierce_carradine_body_skeleton.inc"
};

static u32 _gActor160700PierceCarradineBodyPartVerts[20] = {
#include "assets/pierce_carradine_body_partVerts.inc"
};

static SVECTOR _gActor160700PierceCarradineBodyVerts[390] = {
#include "assets/pierce_carradine_body_verts.inc"
};

static SVECTOR _gActor160700PierceCarradineBodyNormals[407] = {
#include "assets/pierce_carradine_body_normals.inc"
};

static u32 _gActor160700PierceCarradineBodyStream[4476] = {
#include "assets/pierce_carradine_body_stream.inc"
};

static TmdSource _gActor160700PierceCarradineBody = {
    0,
    24444,
    6776,
    20,
    _gActor160700PierceCarradineBodyPartVerts,
    _gActor160700PierceCarradineBodyVerts,
    _gActor160700PierceCarradineBodyNormals,
    _gActor160700PierceCarradineBodySkeleton,
    _gActor160700PierceCarradineBodyStream,
};

static TmdBone _gActor160700Actor113100Model07960Skeleton[1] = {
#include "assets/actor_113100_model_07960_skeleton.inc"
};

static u32 _gActor160700Actor113100Model07960PartVerts[1] = {
#include "assets/actor_113100_model_07960_partVerts.inc"
};

static SVECTOR _gActor160700Actor113100Model07960Verts[14] = {
#include "assets/actor_113100_model_07960_verts.inc"
};

static SVECTOR _gActor160700Actor113100Model07960Normals[12] = {
#include "assets/actor_113100_model_07960_normals.inc"
};

static u32 _gActor160700Actor113100Model07960Stream[56] = {
#include "assets/actor_113100_model_07960_stream.inc"
};

static TmdSource _gActor160700Actor113100Model07960 = {
    0,
    340,
    0,
    1,
    _gActor160700Actor113100Model07960PartVerts,
    _gActor160700Actor113100Model07960Verts,
    _gActor160700Actor113100Model07960Normals,
    _gActor160700Actor113100Model07960Skeleton,
    _gActor160700Actor113100Model07960Stream,
};

static AnimationPackedPose _gActor160700Animation0AD1CBank1[2] = {
#include "assets/actor_160700_animation_0AD1C_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0AD1CBank4[22] = {
#include "assets/actor_160700_animation_0AD1C_bank4.inc"
};

static AnimationRecord _gActor160700Animation0AD1CRecords[98] = {
#include "assets/actor_160700_animation_0AD1C_records.inc"
};

static u16 _gActor160700Animation0AD1CIndices[20] = {
#include "assets/actor_160700_animation_0AD1C_indices.inc"
};

static AnimationSet _gActor160700Animation0AD1C = {
    _gActor160700Animation0AD1CRecords,
    _gActor160700Animation0AD1CIndices,
    { NULL, _gActor160700Animation0AD1CBank1, NULL, NULL, _gActor160700Animation0AD1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0AFECBank1[2] = {
#include "assets/actor_160700_animation_0AFEC_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0AFECBank4[48] = {
#include "assets/actor_160700_animation_0AFEC_bank4.inc"
};

static AnimationRecord _gActor160700Animation0AFECRecords[106] = {
#include "assets/actor_160700_animation_0AFEC_records.inc"
};

static u16 _gActor160700Animation0AFECIndices[20] = {
#include "assets/actor_160700_animation_0AFEC_indices.inc"
};

static AnimationSet _gActor160700Animation0AFEC = {
    _gActor160700Animation0AFECRecords,
    _gActor160700Animation0AFECIndices,
    { NULL, _gActor160700Animation0AFECBank1, NULL, NULL, _gActor160700Animation0AFECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0C104Bank1[30] = {
#include "assets/actor_160700_animation_0C104_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0C104Bank4[439] = {
#include "assets/actor_160700_animation_0C104_bank4.inc"
};

static AnimationRecord _gActor160700Animation0C104Records[545] = {
#include "assets/actor_160700_animation_0C104_records.inc"
};

static u16 _gActor160700Animation0C104Indices[20] = {
#include "assets/actor_160700_animation_0C104_indices.inc"
};

static AnimationSet _gActor160700Animation0C104 = {
    _gActor160700Animation0C104Records,
    _gActor160700Animation0C104Indices,
    { NULL, _gActor160700Animation0C104Bank1, NULL, NULL, _gActor160700Animation0C104Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0C3F4Bank1[4] = {
#include "assets/actor_160700_animation_0C3F4_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0C3F4Bank4[34] = {
#include "assets/actor_160700_animation_0C3F4_bank4.inc"
};

static AnimationRecord _gActor160700Animation0C3F4Records[122] = {
#include "assets/actor_160700_animation_0C3F4_records.inc"
};

static u16 _gActor160700Animation0C3F4Indices[20] = {
#include "assets/actor_160700_animation_0C3F4_indices.inc"
};

static AnimationSet _gActor160700Animation0C3F4 = {
    _gActor160700Animation0C3F4Records,
    _gActor160700Animation0C3F4Indices,
    { NULL, _gActor160700Animation0C3F4Bank1, NULL, NULL, _gActor160700Animation0C3F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0C5D0Bank1[3] = {
#include "assets/actor_160700_animation_0C5D0_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0C5D0Bank4[30] = {
#include "assets/actor_160700_animation_0C5D0_bank4.inc"
};

static AnimationRecord _gActor160700Animation0C5D0Records[60] = {
#include "assets/actor_160700_animation_0C5D0_records.inc"
};

static u16 _gActor160700Animation0C5D0Indices[20] = {
#include "assets/actor_160700_animation_0C5D0_indices.inc"
};

static AnimationSet _gActor160700Animation0C5D0 = {
    _gActor160700Animation0C5D0Records,
    _gActor160700Animation0C5D0Indices,
    { NULL, _gActor160700Animation0C5D0Bank1, NULL, NULL, _gActor160700Animation0C5D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0C858Bank1[3] = {
#include "assets/actor_160700_animation_0C858_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0C858Bank4[29] = {
#include "assets/actor_160700_animation_0C858_bank4.inc"
};

static AnimationRecord _gActor160700Animation0C858Records[104] = {
#include "assets/actor_160700_animation_0C858_records.inc"
};

static u16 _gActor160700Animation0C858Indices[20] = {
#include "assets/actor_160700_animation_0C858_indices.inc"
};

static AnimationSet _gActor160700Animation0C858 = {
    _gActor160700Animation0C858Records,
    _gActor160700Animation0C858Indices,
    { NULL, _gActor160700Animation0C858Bank1, NULL, NULL, _gActor160700Animation0C858Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0CA8CBank1[3] = {
#include "assets/actor_160700_animation_0CA8C_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0CA8CBank4[36] = {
#include "assets/actor_160700_animation_0CA8C_bank4.inc"
};

static AnimationRecord _gActor160700Animation0CA8CRecords[76] = {
#include "assets/actor_160700_animation_0CA8C_records.inc"
};

static u16 _gActor160700Animation0CA8CIndices[20] = {
#include "assets/actor_160700_animation_0CA8C_indices.inc"
};

static AnimationSet _gActor160700Animation0CA8C = {
    _gActor160700Animation0CA8CRecords,
    _gActor160700Animation0CA8CIndices,
    { NULL, _gActor160700Animation0CA8CBank1, NULL, NULL, _gActor160700Animation0CA8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0CC70Bank1[3] = {
#include "assets/actor_160700_animation_0CC70_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0CC70Bank4[32] = {
#include "assets/actor_160700_animation_0CC70_bank4.inc"
};

static AnimationRecord _gActor160700Animation0CC70Records[60] = {
#include "assets/actor_160700_animation_0CC70_records.inc"
};

static u16 _gActor160700Animation0CC70Indices[20] = {
#include "assets/actor_160700_animation_0CC70_indices.inc"
};

static AnimationSet _gActor160700Animation0CC70 = {
    _gActor160700Animation0CC70Records,
    _gActor160700Animation0CC70Indices,
    { NULL, _gActor160700Animation0CC70Bank1, NULL, NULL, _gActor160700Animation0CC70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0D310Bank1[8] = {
#include "assets/actor_160700_animation_0D310_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0D310Bank4[120] = {
#include "assets/actor_160700_animation_0D310_bank4.inc"
};

static AnimationRecord _gActor160700Animation0D310Records[260] = {
#include "assets/actor_160700_animation_0D310_records.inc"
};

static u16 _gActor160700Animation0D310Indices[20] = {
#include "assets/actor_160700_animation_0D310_indices.inc"
};

static AnimationSet _gActor160700Animation0D310 = {
    _gActor160700Animation0D310Records,
    _gActor160700Animation0D310Indices,
    { NULL, _gActor160700Animation0D310Bank1, NULL, NULL, _gActor160700Animation0D310Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0D638Bank1[5] = {
#include "assets/actor_160700_animation_0D638_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0D638Bank4[70] = {
#include "assets/actor_160700_animation_0D638_bank4.inc"
};

static AnimationRecord _gActor160700Animation0D638Records[97] = {
#include "assets/actor_160700_animation_0D638_records.inc"
};

static u16 _gActor160700Animation0D638Indices[20] = {
#include "assets/actor_160700_animation_0D638_indices.inc"
};

static AnimationSet _gActor160700Animation0D638 = {
    _gActor160700Animation0D638Records,
    _gActor160700Animation0D638Indices,
    { NULL, _gActor160700Animation0D638Bank1, NULL, NULL, _gActor160700Animation0D638Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0D980Bank1[3] = {
#include "assets/actor_160700_animation_0D980_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0D980Bank4[47] = {
#include "assets/actor_160700_animation_0D980_bank4.inc"
};

static AnimationRecord _gActor160700Animation0D980Records[134] = {
#include "assets/actor_160700_animation_0D980_records.inc"
};

static u16 _gActor160700Animation0D980Indices[20] = {
#include "assets/actor_160700_animation_0D980_indices.inc"
};

static AnimationSet _gActor160700Animation0D980 = {
    _gActor160700Animation0D980Records,
    _gActor160700Animation0D980Indices,
    { NULL, _gActor160700Animation0D980Bank1, NULL, NULL, _gActor160700Animation0D980Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0DC18Bank1[3] = {
#include "assets/actor_160700_animation_0DC18_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0DC18Bank4[38] = {
#include "assets/actor_160700_animation_0DC18_bank4.inc"
};

static AnimationRecord _gActor160700Animation0DC18Records[99] = {
#include "assets/actor_160700_animation_0DC18_records.inc"
};

static u16 _gActor160700Animation0DC18Indices[20] = {
#include "assets/actor_160700_animation_0DC18_indices.inc"
};

static AnimationSet _gActor160700Animation0DC18 = {
    _gActor160700Animation0DC18Records,
    _gActor160700Animation0DC18Indices,
    { NULL, _gActor160700Animation0DC18Bank1, NULL, NULL, _gActor160700Animation0DC18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0DF38Bank1[4] = {
#include "assets/actor_160700_animation_0DF38_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0DF38Bank4[38] = {
#include "assets/actor_160700_animation_0DF38_bank4.inc"
};

static AnimationRecord _gActor160700Animation0DF38Records[130] = {
#include "assets/actor_160700_animation_0DF38_records.inc"
};

static u16 _gActor160700Animation0DF38Indices[20] = {
#include "assets/actor_160700_animation_0DF38_indices.inc"
};

static AnimationSet _gActor160700Animation0DF38 = {
    _gActor160700Animation0DF38Records,
    _gActor160700Animation0DF38Indices,
    { NULL, _gActor160700Animation0DF38Bank1, NULL, NULL, _gActor160700Animation0DF38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0E1D8Bank1[2] = {
#include "assets/actor_160700_animation_0E1D8_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0E1D8Bank4[54] = {
#include "assets/actor_160700_animation_0E1D8_bank4.inc"
};

static AnimationRecord _gActor160700Animation0E1D8Records[88] = {
#include "assets/actor_160700_animation_0E1D8_records.inc"
};

static u16 _gActor160700Animation0E1D8Indices[20] = {
#include "assets/actor_160700_animation_0E1D8_indices.inc"
};

static AnimationSet _gActor160700Animation0E1D8 = {
    _gActor160700Animation0E1D8Records,
    _gActor160700Animation0E1D8Indices,
    { NULL, _gActor160700Animation0E1D8Bank1, NULL, NULL, _gActor160700Animation0E1D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0E4E4Bank1[6] = {
#include "assets/actor_160700_animation_0E4E4_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0E4E4Bank4[61] = {
#include "assets/actor_160700_animation_0E4E4_bank4.inc"
};

static AnimationRecord _gActor160700Animation0E4E4Records[96] = {
#include "assets/actor_160700_animation_0E4E4_records.inc"
};

static u16 _gActor160700Animation0E4E4Indices[20] = {
#include "assets/actor_160700_animation_0E4E4_indices.inc"
};

static AnimationSet _gActor160700Animation0E4E4 = {
    _gActor160700Animation0E4E4Records,
    _gActor160700Animation0E4E4Indices,
    { NULL, _gActor160700Animation0E4E4Bank1, NULL, NULL, _gActor160700Animation0E4E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0E8BCBank1[4] = {
#include "assets/actor_160700_animation_0E8BC_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0E8BCBank4[60] = {
#include "assets/actor_160700_animation_0E8BC_bank4.inc"
};

static AnimationRecord _gActor160700Animation0E8BCRecords[154] = {
#include "assets/actor_160700_animation_0E8BC_records.inc"
};

static u16 _gActor160700Animation0E8BCIndices[20] = {
#include "assets/actor_160700_animation_0E8BC_indices.inc"
};

static AnimationSet _gActor160700Animation0E8BC = {
    _gActor160700Animation0E8BCRecords,
    _gActor160700Animation0E8BCIndices,
    { NULL, _gActor160700Animation0E8BCBank1, NULL, NULL, _gActor160700Animation0E8BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0EA74Bank1[2] = {
#include "assets/actor_160700_animation_0EA74_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0EA74Bank4[24] = {
#include "assets/actor_160700_animation_0EA74_bank4.inc"
};

static AnimationRecord _gActor160700Animation0EA74Records[60] = {
#include "assets/actor_160700_animation_0EA74_records.inc"
};

static u16 _gActor160700Animation0EA74Indices[20] = {
#include "assets/actor_160700_animation_0EA74_indices.inc"
};

static AnimationSet _gActor160700Animation0EA74 = {
    _gActor160700Animation0EA74Records,
    _gActor160700Animation0EA74Indices,
    { NULL, _gActor160700Animation0EA74Bank1, NULL, NULL, _gActor160700Animation0EA74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0EC58Bank1[3] = {
#include "assets/actor_160700_animation_0EC58_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0EC58Bank4[32] = {
#include "assets/actor_160700_animation_0EC58_bank4.inc"
};

static AnimationRecord _gActor160700Animation0EC58Records[60] = {
#include "assets/actor_160700_animation_0EC58_records.inc"
};

static u16 _gActor160700Animation0EC58Indices[20] = {
#include "assets/actor_160700_animation_0EC58_indices.inc"
};

static AnimationSet _gActor160700Animation0EC58 = {
    _gActor160700Animation0EC58Records,
    _gActor160700Animation0EC58Indices,
    { NULL, _gActor160700Animation0EC58Bank1, NULL, NULL, _gActor160700Animation0EC58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0EF34Bank1[3] = {
#include "assets/actor_160700_animation_0EF34_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0EF34Bank4[35] = {
#include "assets/actor_160700_animation_0EF34_bank4.inc"
};

static AnimationRecord _gActor160700Animation0EF34Records[119] = {
#include "assets/actor_160700_animation_0EF34_records.inc"
};

static u16 _gActor160700Animation0EF34Indices[20] = {
#include "assets/actor_160700_animation_0EF34_indices.inc"
};

static AnimationSet _gActor160700Animation0EF34 = {
    _gActor160700Animation0EF34Records,
    _gActor160700Animation0EF34Indices,
    { NULL, _gActor160700Animation0EF34Bank1, NULL, NULL, _gActor160700Animation0EF34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0F118Bank1[3] = {
#include "assets/actor_160700_animation_0F118_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0F118Bank4[32] = {
#include "assets/actor_160700_animation_0F118_bank4.inc"
};

static AnimationRecord _gActor160700Animation0F118Records[60] = {
#include "assets/actor_160700_animation_0F118_records.inc"
};

static u16 _gActor160700Animation0F118Indices[20] = {
#include "assets/actor_160700_animation_0F118_indices.inc"
};

static AnimationSet _gActor160700Animation0F118 = {
    _gActor160700Animation0F118Records,
    _gActor160700Animation0F118Indices,
    { NULL, _gActor160700Animation0F118Bank1, NULL, NULL, _gActor160700Animation0F118Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0F3D0Bank1[2] = {
#include "assets/actor_160700_animation_0F3D0_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0F3D0Bank4[56] = {
#include "assets/actor_160700_animation_0F3D0_bank4.inc"
};

static AnimationRecord _gActor160700Animation0F3D0Records[92] = {
#include "assets/actor_160700_animation_0F3D0_records.inc"
};

static u16 _gActor160700Animation0F3D0Indices[20] = {
#include "assets/actor_160700_animation_0F3D0_indices.inc"
};

static AnimationSet _gActor160700Animation0F3D0 = {
    _gActor160700Animation0F3D0Records,
    _gActor160700Animation0F3D0Indices,
    { NULL, _gActor160700Animation0F3D0Bank1, NULL, NULL, _gActor160700Animation0F3D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0F65CBank1[2] = {
#include "assets/actor_160700_animation_0F65C_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0F65CBank4[33] = {
#include "assets/actor_160700_animation_0F65C_bank4.inc"
};

static AnimationRecord _gActor160700Animation0F65CRecords[104] = {
#include "assets/actor_160700_animation_0F65C_records.inc"
};

static u16 _gActor160700Animation0F65CIndices[20] = {
#include "assets/actor_160700_animation_0F65C_indices.inc"
};

static AnimationSet _gActor160700Animation0F65C = {
    _gActor160700Animation0F65CRecords,
    _gActor160700Animation0F65CIndices,
    { NULL, _gActor160700Animation0F65CBank1, NULL, NULL, _gActor160700Animation0F65CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0F830Bank1[2] = {
#include "assets/actor_160700_animation_0F830_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0F830Bank4[23] = {
#include "assets/actor_160700_animation_0F830_bank4.inc"
};

static AnimationRecord _gActor160700Animation0F830Records[68] = {
#include "assets/actor_160700_animation_0F830_records.inc"
};

static u16 _gActor160700Animation0F830Indices[20] = {
#include "assets/actor_160700_animation_0F830_indices.inc"
};

static AnimationSet _gActor160700Animation0F830 = {
    _gActor160700Animation0F830Records,
    _gActor160700Animation0F830Indices,
    { NULL, _gActor160700Animation0F830Bank1, NULL, NULL, _gActor160700Animation0F830Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_actor_160700_80141678[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_160700_801325F0 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_160700_8013265C },
    { ACTOR_MESSAGE_PLACE, pacedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_160700_80132738 },
    { ACTOR_MESSAGE_WALK_TO, pacedWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_160700_801416A8[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_160700_8013233C, { .model = &_gActor160700PierceCarradineBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_160700_80132808, { .model = &_gActor160700Actor113100Model07960 } },
};

u8 D_actor_160700_801416C0[100] = {
    0,
    0,
    0,
    0,
    60,
    203,
    19,
    128,
    12,
    206,
    19,
    128,
    36,
    223,
    19,
    128,
    20,
    226,
    19,
    128,
    240,
    227,
    19,
    128,
    120,
    230,
    19,
    128,
    172,
    232,
    19,
    128,
    144,
    234,
    19,
    128,
    48,
    241,
    19,
    128,
    88,
    244,
    19,
    128,
    160,
    247,
    19,
    128,
    56,
    250,
    19,
    128,
    88,
    253,
    19,
    128,
    248,
    255,
    19,
    128,
    4,
    3,
    20,
    128,
    220,
    6,
    20,
    128,
    148,
    8,
    20,
    128,
    120,
    10,
    20,
    128,
    84,
    13,
    20,
    128,
    56,
    15,
    20,
    128,
    240,
    17,
    20,
    128,
    124,
    20,
    20,
    128,
    80,
    22,
    20,
    128,
    0,
    0,
    0,
    0,
};

void        func_actor_160700_80131E24(void);
void        func_actor_160700_80131E70(void);
static void func_actor_160700_80131F70(Enemy* enemy, Task* task);

void func_actor_160700_80131E24(void)
{
    Task* slot;

    if (gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) != 0) {
        slot = Gp_LookupSlot4(0);
        if (slot != 0) {
            TASK_MESSAGE_DISPATCH_POINTER(slot, 0x7D3, &D_actor_160700_801354CC, 0);
        }
    }
}

void func_actor_160700_80131E70(void)
{
    switch (gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS)) {
        case 0:
            func_800E8634(D_actor_160700_80135664, 0, D_actor_160700_80135ACC);
            gameFlagSetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS, 1);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 0xC);
            break;
        case 1:
            func_800E8634(D_actor_160700_80135BD4, 0, D_actor_160700_80135ACC);
            gameFlagSetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS, 2);
            break;
        case 2:
            func_800E8614(D_actor_160700_801362F4, 0);
            gameFlagSetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS, 3);
            break;
        case 3:
            func_800E8614(D_actor_160700_80136414, 0);
            break;
    }
}

/// State-0 handler of the actor's dispatcher: allocates the work block, spawns
/// the sub-model and adopts it as a child, takes the model's texture page and
/// CLUT from the area placement the enemy's `placeKey` selects, sets up the
/// animation context on clip 1, installs the message table whose handlers are
/// the actor's script opcodes, and starts the animation.
static void func_actor_160700_80131F70(Enemy* enemy, Task* task)
{
    VECTOR         vec;
    PacedWalkWork* work;
    PacedWalkWork* mem;
    GfxCoord*      coord;
    TmdObject*     obj;
    Enemy*         spawned;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = memCalloc(sizeof(PacedWalkWork), false);
    work       = mem;
    task->work = mem;
    if (mem == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_160700_80132414;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->flags                       = 0;
    obj->otOffset                    = 1;
    work->enemy                      = enemy;
    spawned                          = Gp_SpawnEnemyFromTable(D_actor_160700_801416A8, 1, 0, enemy);
    actorTintModel(spawned->task->extra.tmd, enemy);
    taskReparent(task, spawned->task);
    work->pairTask  = spawned->task;
    work->st.animId = 1;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_160700_801416C0, obj,
                         work->rig.poses, work->rig.slots);
    work->st.state = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable = D_actor_160700_80141678;
    pacedWalkUpdate(task);
    task->state += 1;
}

#include "../../shared/paced_walk_update.inc.c"

/// Two-state dispatcher, its handler table built on the stack: state 0 spawns
/// the actor, state 1 runs it. Both handlers take the task's `Enemy` as
/// well as the task.
void func_actor_160700_8013233C(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_160700_80131F70,
        func_actor_160700_80132390,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_160700_80132390
#define walkerUpdate     pacedWalkUpdate
#define walkerDrawShadow walkerDrawShadow
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// Exit callback: hands the task's `Enemy` back to `enemyDestroy`.
static void func_actor_160700_80132414(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/walker_shadow.inc.c"

#include "../../shared/paced_walk_tick_anim.inc.c"

#include "../../shared/paced_walk_reset_anim.inc.c"

#include "../../shared/paced_walk_blend_anim.inc.c"

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 0x19 and above before changing playback state.
/// The blend path carries the requested duration in whole frames.
s32 func_actor_160700_801325F0(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    PacedWalkWork* work;

    work = task->work;
    if (args->animationId < 0x19) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
            work->blendFrames = args->blendFrames;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        work->st.field_6 = 0;
        pacedWalkUpdate(task);
        return 0;
    }
    return -1;
}

/// Script opcode: sets the visibility flags of the actor's model and of the
/// model of the enemy spawned alongside it. `flags` bit 0 makes both visible
/// (`TmdObject::flags` 0) and its absence hides them (0x80); bit 1 also sets
/// 0x4. The middle argument is the one every opcode of the table receives.
s32 func_actor_160700_8013265C(Task* task, s32 arg1, s32 flags, s32 arg3)
{
    TmdObject* self;
    TmdObject* other;

    self  = task->extra.tmd;
    other = ((PacedWalkWork*)task->work)->pairTask->extra.tmd;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        other->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (flags & 2) {
        self->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        other->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/paced_walk_place.inc.c"

/// Script opcode (message 0x7DB) that does nothing.
s32 func_actor_160700_80132738(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

#include "../../shared/paced_walk_to.inc.c"

/// Handler of the sub-model the actor spawns and adopts as its child. On the
/// first frame it points the sub-model's light and colour matrices at the
/// parent's, makes it visible with `flags` 0 and parents its root coordinate
/// to part 4 of the parent's model; every frame it clears the coordinate's
/// `composeStamp` so it is recomputed from that part.
void func_actor_160700_80132808(Task* task)
{
    char           pad[0x10];
    Task*          parent = task->parent;
    TmdObject*     obj    = task->extra.tmd;
    GfxCoord*      coord  = obj->coords;
    GfxCoord*      sub    = &parent->extra.tmd->coords[4];
    PacedWalkWork* work   = parent->work;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            obj->lightMtx       = &work->light;
            obj->flags          = 0;
            obj->colorMtx       = &work->color;
            coord->parent       = sub;
            task->state++;
            break;
        case 1:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
