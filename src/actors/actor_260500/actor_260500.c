#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
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
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/footstep_walk.h"
#include "../../shared/walker.h"

/// The clips the package adds to the player's animation bank, with the records
/// stored after them.
///
/// The package's event scripts send `data.copy` to the player. The copy takes
/// `ANIMATION_BANK_EXTENSION_CAPACITY` words from the start of the storage,
/// which is more than the clip table holds: the ten set pointers occupy
/// extended ids 47-56, and the request itself, the play request, the first two
/// placements and the position of the third are written into the bank after
/// them. The player's requests select ids 47-50, 53-56 and the bank's own id 1
/// only, so neither a NULL entry nor a following word is played as a clip.
///
/// The play request and the placements have no other connection to the clips.
/// The placements are the first three of the four the package keeps for the
/// actor at scene placement 0, and are part of this object only because the
/// copied span reaches into the third; the fourth follows as a separate
/// object. The storage is only read.
typedef union {
    struct {
        AnimationSet*            sets[10];              // Player clips for extended ids 47-56; NULL at ids 51 and 52, which nothing requests
        AnimationBankCopyRequest copy;                  // Installs the first `ANIMATION_BANK_EXTENSION_CAPACITY` words of this storage in the player's bank extension
        AnimationPlayRequest     playerBaseClipRequest; // Restarts the player bank's own clip 1; played as the first conversation opens and when it ends or is skipped
        ActorTransform           actorPlacements[3];    // Where the scripts stand the actor at scene placement 0 (0 after the first conversation ends or is skipped, 1 the same spot as it opens and resumes, 2 its other spot during it)
    } data;                                             // The records by name
    s32 words[35];                                      // The same storage as the copy reads it; the last three words lie beyond the copied span
} _Actor260500AnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor260500AnimationBankExtensionStorage, 140);

extern _Actor260500AnimationBankExtensionStorage D_actor_260500_8014CAF4;

/// The work block, published by the spawn routine and by the task handler
/// `func_actor_260500_8014A460` on every frame, so the message handlers and
/// the animation loops reach it without the task.
extern FootstepWalkQuietWork* gFootstepWalkWork;

/// The actor's own task, published by the spawn routine: the play-animation
/// handler runs the update on it and the visibility handler reaches its model.
extern Task* D_actor_260500_80159E50;

/// Reset argument the blended reseed forwards: the play-animation handler
/// latches the preset's `field_C` here, and the update sets it to 10 when a
/// walk ends.
extern s16 gFootstepWalkBlendFrames;

/// Approach mode the last `func_actor_260500_8014A83C` call selected; the
/// update picks its step length from it.
extern s16 gFootstepWalkMode;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern EvsCommand D_actor_260500_8014CBF8[];
extern EvsCommand D_actor_260500_8014D630[];
extern EvsCommand D_actor_260500_8014D7C8[];
extern EvsCommand D_actor_260500_8014D948[];
extern EvsCommand D_actor_260500_8014DAB0[];
extern EvsCommand D_actor_260500_8014DCC0[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_260500_80159D80[];
extern u8               D_actor_260500_80159DBC[];

static void func_actor_260500_8014A4BC(Enemy* enemy, Task* task);
static void func_actor_260500_8014A540(Task* task);

static TmdSource _gActor260500JodieBouquetBody2;
void             func_actor_260500_8014A460(Task*);

s32 func_actor_260500_8014A6C4(Task*, s32, AnimationPlayRequest*, s32);
s32 func_actor_260500_8014A754(Task*, s32, s32, s32);
s32 func_actor_260500_8014A818(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
s32 func_actor_260500_8014A83C(Task*, s32, VECTOR*, s32);

extern AnimationPlayRequest D_actor_260500_8014C874;
extern AnimationPlayRequest D_actor_260500_8014C888;
extern AnimationPlayRequest D_actor_260500_8014C89C;
extern AnimationPlayRequest D_actor_260500_8014C8B0;
extern AnimationPlayRequest D_actor_260500_8014C8C4;
extern AnimationPlayRequest D_actor_260500_8014C8D8;
extern AnimationPlayRequest D_actor_260500_8014C8EC;
extern AnimationPlayRequest D_actor_260500_8014C900;
extern AnimationPlayRequest D_actor_260500_8014C914;
extern AnimationPlayRequest D_actor_260500_8014C928;
extern AnimationPlayRequest D_actor_260500_8014C93C;
extern AnimationPlayRequest D_actor_260500_8014C950;
extern AnimationPlayRequest D_actor_260500_8014C964;
extern AnimationPlayRequest D_actor_260500_8014C978;
extern AnimationPlayRequest D_actor_260500_8014C98C;
extern AnimationPlayRequest D_actor_260500_8014C9A0;
extern AnimationPlayRequest D_actor_260500_8014C9B4;
extern AnimationPlayRequest D_actor_260500_8014C9C8;
extern AnimationPlayRequest D_actor_260500_8014C9DC;
extern AnimationPlayRequest D_actor_260500_8014C9F0;
extern AnimationPlayRequest D_actor_260500_8014CA04;
extern AnimationPlayRequest D_actor_260500_8014CA18;
extern AnimationPlayRequest D_actor_260500_8014CA2C;
extern AnimationPlayRequest D_actor_260500_8014CA54;
extern AnimationPlayRequest D_actor_260500_8014CA68;
extern AnimationPlayRequest D_actor_260500_8014CA7C;
extern AnimationPlayRequest D_actor_260500_8014CA90;
extern AnimationPlayRequest D_actor_260500_8014CAA4;
extern AnimationPlayRequest D_actor_260500_8014CAB8;
extern AnimationPlayRequest D_actor_260500_8014CACC;
extern AnimationPlayRequest D_actor_260500_8014CAE0;
void                        func_actor_260500_80149E38(s32);

static AnimationSet _gActor260500Animation01024;
static AnimationSet _gActor260500Animation01314;
static AnimationSet _gActor260500Animation01A10;
static AnimationSet _gActor260500Animation01CAC;
static AnimationSet _gActor260500Animation02064;
static AnimationSet _gActor260500Animation023E8;
static AnimationSet _gActor260500Animation026EC;
static AnimationSet _gActor260500Animation02A18;

static AnimationPackedPose _gActor260500Animation01024Bank1[3] = {
#include "assets/actor_260500_animation_01024_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation01024Bank4[81] = {
#include "assets/actor_260500_animation_01024_bank4.inc"
};

static AnimationRecord _gActor260500Animation01024Records[163] = {
#include "assets/actor_260500_animation_01024_records.inc"
};

static u16 _gActor260500Animation01024Indices[20] = {
#include "assets/actor_260500_animation_01024_indices.inc"
};

static AnimationSet _gActor260500Animation01024 = {
    _gActor260500Animation01024Records,
    _gActor260500Animation01024Indices,
    { NULL, _gActor260500Animation01024Bank1, NULL, NULL, _gActor260500Animation01024Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation01314Bank1[4] = {
#include "assets/actor_260500_animation_01314_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation01314Bank4[40] = {
#include "assets/actor_260500_animation_01314_bank4.inc"
};

static AnimationRecord _gActor260500Animation01314Records[116] = {
#include "assets/actor_260500_animation_01314_records.inc"
};

static u16 _gActor260500Animation01314Indices[20] = {
#include "assets/actor_260500_animation_01314_indices.inc"
};

static AnimationSet _gActor260500Animation01314 = {
    _gActor260500Animation01314Records,
    _gActor260500Animation01314Indices,
    { NULL, _gActor260500Animation01314Bank1, NULL, NULL, _gActor260500Animation01314Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation01A10Bank1[8] = {
#include "assets/actor_260500_animation_01A10_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation01A10Bank4[157] = {
#include "assets/actor_260500_animation_01A10_bank4.inc"
};

static AnimationRecord _gActor260500Animation01A10Records[246] = {
#include "assets/actor_260500_animation_01A10_records.inc"
};

static u16 _gActor260500Animation01A10Indices[20] = {
#include "assets/actor_260500_animation_01A10_indices.inc"
};

static AnimationSet _gActor260500Animation01A10 = {
    _gActor260500Animation01A10Records,
    _gActor260500Animation01A10Indices,
    { NULL, _gActor260500Animation01A10Bank1, NULL, NULL, _gActor260500Animation01A10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation01CACBank1[2] = {
#include "assets/actor_260500_animation_01CAC_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation01CACBank4[51] = {
#include "assets/actor_260500_animation_01CAC_bank4.inc"
};

static AnimationRecord _gActor260500Animation01CACRecords[90] = {
#include "assets/actor_260500_animation_01CAC_records.inc"
};

static u16 _gActor260500Animation01CACIndices[20] = {
#include "assets/actor_260500_animation_01CAC_indices.inc"
};

static AnimationSet _gActor260500Animation01CAC = {
    _gActor260500Animation01CACRecords,
    _gActor260500Animation01CACIndices,
    { NULL, _gActor260500Animation01CACBank1, NULL, NULL, _gActor260500Animation01CACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation02064Bank1[3] = {
#include "assets/actor_260500_animation_02064_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation02064Bank4[81] = {
#include "assets/actor_260500_animation_02064_bank4.inc"
};

static AnimationRecord _gActor260500Animation02064Records[128] = {
#include "assets/actor_260500_animation_02064_records.inc"
};

static u16 _gActor260500Animation02064Indices[20] = {
#include "assets/actor_260500_animation_02064_indices.inc"
};

static AnimationSet _gActor260500Animation02064 = {
    _gActor260500Animation02064Records,
    _gActor260500Animation02064Indices,
    { NULL, _gActor260500Animation02064Bank1, NULL, NULL, _gActor260500Animation02064Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation023E8Bank1[6] = {
#include "assets/actor_260500_animation_023E8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation023E8Bank4[69] = {
#include "assets/actor_260500_animation_023E8_bank4.inc"
};

static AnimationRecord _gActor260500Animation023E8Records[118] = {
#include "assets/actor_260500_animation_023E8_records.inc"
};

static u16 _gActor260500Animation023E8Indices[20] = {
#include "assets/actor_260500_animation_023E8_indices.inc"
};

static AnimationSet _gActor260500Animation023E8 = {
    _gActor260500Animation023E8Records,
    _gActor260500Animation023E8Indices,
    { NULL, _gActor260500Animation023E8Bank1, NULL, NULL, _gActor260500Animation023E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation026ECBank1[6] = {
#include "assets/actor_260500_animation_026EC_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation026ECBank4[46] = {
#include "assets/actor_260500_animation_026EC_bank4.inc"
};

static AnimationRecord _gActor260500Animation026ECRecords[109] = {
#include "assets/actor_260500_animation_026EC_records.inc"
};

static u16 _gActor260500Animation026ECIndices[20] = {
#include "assets/actor_260500_animation_026EC_indices.inc"
};

static AnimationSet _gActor260500Animation026EC = {
    _gActor260500Animation026ECRecords,
    _gActor260500Animation026ECIndices,
    { NULL, _gActor260500Animation026ECBank1, NULL, NULL, _gActor260500Animation026ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation02A18Bank1[7] = {
#include "assets/actor_260500_animation_02A18_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation02A18Bank4[56] = {
#include "assets/actor_260500_animation_02A18_bank4.inc"
};

static AnimationRecord _gActor260500Animation02A18Records[106] = {
#include "assets/actor_260500_animation_02A18_records.inc"
};

static u16 _gActor260500Animation02A18Indices[20] = {
#include "assets/actor_260500_animation_02A18_indices.inc"
};

static AnimationSet _gActor260500Animation02A18 = {
    _gActor260500Animation02A18Records,
    _gActor260500Animation02A18Indices,
    { NULL, _gActor260500Animation02A18Bank1, NULL, NULL, _gActor260500Animation02A18Bank4, NULL, NULL, NULL },
};

AnimationPlayRequest D_actor_260500_8014C860 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C874 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C888 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C89C = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C8B0 = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C8C4 = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C8D8 = { { .index = 1 }, 6, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C8EC = { { .index = 1 }, 7, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C900 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C914 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C928 = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C93C = { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C950 = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C964 = { { .index = 1 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C978 = { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C98C = { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9A0 = { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9B4 = { { .index = 1 }, 17, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9C8 = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9DC = { { .index = 1 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9F0 = { { .index = 1 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA04 = { { .index = 1 }, 21, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA18 = { { .index = 1 }, 22, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA2C = { { .index = 1 }, 23, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_260500_8014CA40 = { { .index = 1 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA54 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA68 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA7C = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA90 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CAA4 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CAB8 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CACC = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CAE0 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

_Actor260500AnimationBankExtensionStorage D_actor_260500_8014CAF4 = { .data = { { &_gActor260500Animation01024, &_gActor260500Animation01314, &_gActor260500Animation01A10, &_gActor260500Animation01CAC, NULL, NULL, &_gActor260500Animation02064, &_gActor260500Animation023E8, &_gActor260500Animation026EC, &_gActor260500Animation02A18 }, { { .words = D_actor_260500_8014CAF4.words }, ANIMATION_BANK_EXTENSION_CAPACITY }, { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { { 860, 0, 6730, 0 }, { 0, -2161, 0, 0 } }, { { 860, 0, 6730, 0 }, { 0, -2161, 0, 0 } }, { { 860, 0, 6910, 0 }, { 0, -2048, 0, 0 } } } } };

ActorTransform D_actor_260500_8014CB80 = { { 860, 0, 6640, 0 }, { 0, -2161, 0, 0 } };

ActorTransform D_actor_260500_8014CB98 = { { 920, 0, 6000, 0 }, { 0, -113, 0, 0 } };

ActorTransform D_actor_260500_8014CBB0 = { { 1210, 0, 5610, 0 }, { 0, -227, 0, 0 } };

ActorTransform D_actor_260500_8014CBC8 = { { 1010, 0, 5840, 0 }, { 0, -113, 0, 0 } };

ActorTransform D_actor_260500_8014CBE0 = { { 860, 0, 6180, 0 }, { 0, 0, 0, 0 } };

EvsCommand D_actor_260500_8014CBF8[109] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_260500_80149E38 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAF4.data.playerBaseClipRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260500_8014CAF4.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CB98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CAF4.data.actorPlacements[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C874 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CAF4.data.actorPlacements[2] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C888 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C89C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8C4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CBB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CB80 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CBC8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C900 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C914 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C928 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C93C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CAF4.data.actorPlacements[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C950 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CBB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA68 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CAF4.data.actorPlacements[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C964 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CAF4.data.actorPlacements[2] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CB98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C978 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C98C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C9A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014CA2C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CAF4.data.actorPlacements[0] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CBE0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAF4.data.playerBaseClipRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_260500_80149E38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260500_8014D630[17] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAF4.data.playerBaseClipRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CAF4.data.actorPlacements[0] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CBE0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_260500_80149E38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260500_8014D7C8[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260500_8014CAF4.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C9B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260500_8014D948[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260500_8014CAF4.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C9C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260500_8014DAB0[22] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260500_8014CAF4.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C9DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA68 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C9F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014CA04 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260500_8014DCC0[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260500_8014CAF4.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014CA18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static AnimationPackedPose _gActor260500Animation042F8Bank1[4] = {
#include "assets/actor_260500_animation_042F8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation042F8Bank4[55] = {
#include "assets/actor_260500_animation_042F8_bank4.inc"
};

static AnimationRecord _gActor260500Animation042F8Records[147] = {
#include "assets/actor_260500_animation_042F8_records.inc"
};

static u16 _gActor260500Animation042F8Indices[20] = {
#include "assets/actor_260500_animation_042F8_indices.inc"
};

static AnimationSet _gActor260500Animation042F8 = {
    _gActor260500Animation042F8Records,
    _gActor260500Animation042F8Indices,
    { NULL, _gActor260500Animation042F8Bank1, NULL, NULL, _gActor260500Animation042F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation0483CBank1[5] = {
#include "assets/actor_260500_animation_0483C_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation0483CBank4[103] = {
#include "assets/actor_260500_animation_0483C_bank4.inc"
};

static AnimationRecord _gActor260500Animation0483CRecords[199] = {
#include "assets/actor_260500_animation_0483C_records.inc"
};

static u16 _gActor260500Animation0483CIndices[20] = {
#include "assets/actor_260500_animation_0483C_indices.inc"
};

static AnimationSet _gActor260500Animation0483C = {
    _gActor260500Animation0483CRecords,
    _gActor260500Animation0483CIndices,
    { NULL, _gActor260500Animation0483CBank1, NULL, NULL, _gActor260500Animation0483CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation04AC4Bank1[2] = {
#include "assets/actor_260500_animation_04AC4_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation04AC4Bank4[53] = {
#include "assets/actor_260500_animation_04AC4_bank4.inc"
};

static AnimationRecord _gActor260500Animation04AC4Records[83] = {
#include "assets/actor_260500_animation_04AC4_records.inc"
};

static u16 _gActor260500Animation04AC4Indices[20] = {
#include "assets/actor_260500_animation_04AC4_indices.inc"
};

static AnimationSet _gActor260500Animation04AC4 = {
    _gActor260500Animation04AC4Records,
    _gActor260500Animation04AC4Indices,
    { NULL, _gActor260500Animation04AC4Bank1, NULL, NULL, _gActor260500Animation04AC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation04D18Bank1[3] = {
#include "assets/actor_260500_animation_04D18_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation04D18Bank4[27] = {
#include "assets/actor_260500_animation_04D18_bank4.inc"
};

static AnimationRecord _gActor260500Animation04D18Records[93] = {
#include "assets/actor_260500_animation_04D18_records.inc"
};

static u16 _gActor260500Animation04D18Indices[20] = {
#include "assets/actor_260500_animation_04D18_indices.inc"
};

static AnimationSet _gActor260500Animation04D18 = {
    _gActor260500Animation04D18Records,
    _gActor260500Animation04D18Indices,
    { NULL, _gActor260500Animation04D18Bank1, NULL, NULL, _gActor260500Animation04D18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation04ED8Bank1[3] = {
#include "assets/actor_260500_animation_04ED8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation04ED8Bank4[20] = {
#include "assets/actor_260500_animation_04ED8_bank4.inc"
};

static AnimationRecord _gActor260500Animation04ED8Records[63] = {
#include "assets/actor_260500_animation_04ED8_records.inc"
};

static u16 _gActor260500Animation04ED8Indices[20] = {
#include "assets/actor_260500_animation_04ED8_indices.inc"
};

static AnimationSet _gActor260500Animation04ED8 = {
    _gActor260500Animation04ED8Records,
    _gActor260500Animation04ED8Indices,
    { NULL, _gActor260500Animation04ED8Bank1, NULL, NULL, _gActor260500Animation04ED8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation05234Bank1[3] = {
#include "assets/actor_260500_animation_05234_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation05234Bank4[49] = {
#include "assets/actor_260500_animation_05234_bank4.inc"
};

static AnimationRecord _gActor260500Animation05234Records[137] = {
#include "assets/actor_260500_animation_05234_records.inc"
};

static u16 _gActor260500Animation05234Indices[20] = {
#include "assets/actor_260500_animation_05234_indices.inc"
};

static AnimationSet _gActor260500Animation05234 = {
    _gActor260500Animation05234Records,
    _gActor260500Animation05234Indices,
    { NULL, _gActor260500Animation05234Bank1, NULL, NULL, _gActor260500Animation05234Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation05400Bank1[3] = {
#include "assets/actor_260500_animation_05400_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation05400Bank4[29] = {
#include "assets/actor_260500_animation_05400_bank4.inc"
};

static AnimationRecord _gActor260500Animation05400Records[57] = {
#include "assets/actor_260500_animation_05400_records.inc"
};

static u16 _gActor260500Animation05400Indices[20] = {
#include "assets/actor_260500_animation_05400_indices.inc"
};

static AnimationSet _gActor260500Animation05400 = {
    _gActor260500Animation05400Records,
    _gActor260500Animation05400Indices,
    { NULL, _gActor260500Animation05400Bank1, NULL, NULL, _gActor260500Animation05400Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation057B0Bank1[4] = {
#include "assets/actor_260500_animation_057B0_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation057B0Bank4[82] = {
#include "assets/actor_260500_animation_057B0_bank4.inc"
};

static AnimationRecord _gActor260500Animation057B0Records[122] = {
#include "assets/actor_260500_animation_057B0_records.inc"
};

static u16 _gActor260500Animation057B0Indices[20] = {
#include "assets/actor_260500_animation_057B0_indices.inc"
};

static AnimationSet _gActor260500Animation057B0 = {
    _gActor260500Animation057B0Records,
    _gActor260500Animation057B0Indices,
    { NULL, _gActor260500Animation057B0Bank1, NULL, NULL, _gActor260500Animation057B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation059D4Bank1[2] = {
#include "assets/actor_260500_animation_059D4_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation059D4Bank4[26] = {
#include "assets/actor_260500_animation_059D4_bank4.inc"
};

static AnimationRecord _gActor260500Animation059D4Records[85] = {
#include "assets/actor_260500_animation_059D4_records.inc"
};

static u16 _gActor260500Animation059D4Indices[20] = {
#include "assets/actor_260500_animation_059D4_indices.inc"
};

static AnimationSet _gActor260500Animation059D4 = {
    _gActor260500Animation059D4Records,
    _gActor260500Animation059D4Indices,
    { NULL, _gActor260500Animation059D4Bank1, NULL, NULL, _gActor260500Animation059D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation05DB8Bank1[6] = {
#include "assets/actor_260500_animation_05DB8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation05DB8Bank4[63] = {
#include "assets/actor_260500_animation_05DB8_bank4.inc"
};

static AnimationRecord _gActor260500Animation05DB8Records[148] = {
#include "assets/actor_260500_animation_05DB8_records.inc"
};

static u16 _gActor260500Animation05DB8Indices[20] = {
#include "assets/actor_260500_animation_05DB8_indices.inc"
};

static AnimationSet _gActor260500Animation05DB8 = {
    _gActor260500Animation05DB8Records,
    _gActor260500Animation05DB8Indices,
    { NULL, _gActor260500Animation05DB8Bank1, NULL, NULL, _gActor260500Animation05DB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation060B8Bank1[3] = {
#include "assets/actor_260500_animation_060B8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation060B8Bank4[62] = {
#include "assets/actor_260500_animation_060B8_bank4.inc"
};

static AnimationRecord _gActor260500Animation060B8Records[101] = {
#include "assets/actor_260500_animation_060B8_records.inc"
};

static u16 _gActor260500Animation060B8Indices[20] = {
#include "assets/actor_260500_animation_060B8_indices.inc"
};

static AnimationSet _gActor260500Animation060B8 = {
    _gActor260500Animation060B8Records,
    _gActor260500Animation060B8Indices,
    { NULL, _gActor260500Animation060B8Bank1, NULL, NULL, _gActor260500Animation060B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation06474Bank1[5] = {
#include "assets/actor_260500_animation_06474_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation06474Bank4[76] = {
#include "assets/actor_260500_animation_06474_bank4.inc"
};

static AnimationRecord _gActor260500Animation06474Records[128] = {
#include "assets/actor_260500_animation_06474_records.inc"
};

static u16 _gActor260500Animation06474Indices[20] = {
#include "assets/actor_260500_animation_06474_indices.inc"
};

static AnimationSet _gActor260500Animation06474 = {
    _gActor260500Animation06474Records,
    _gActor260500Animation06474Indices,
    { NULL, _gActor260500Animation06474Bank1, NULL, NULL, _gActor260500Animation06474Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation06640Bank1[3] = {
#include "assets/actor_260500_animation_06640_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation06640Bank4[29] = {
#include "assets/actor_260500_animation_06640_bank4.inc"
};

static AnimationRecord _gActor260500Animation06640Records[57] = {
#include "assets/actor_260500_animation_06640_records.inc"
};

static u16 _gActor260500Animation06640Indices[20] = {
#include "assets/actor_260500_animation_06640_indices.inc"
};

static AnimationSet _gActor260500Animation06640 = {
    _gActor260500Animation06640Records,
    _gActor260500Animation06640Indices,
    { NULL, _gActor260500Animation06640Bank1, NULL, NULL, _gActor260500Animation06640Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation06878Bank1[4] = {
#include "assets/actor_260500_animation_06878_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation06878Bank4[41] = {
#include "assets/actor_260500_animation_06878_bank4.inc"
};

static AnimationRecord _gActor260500Animation06878Records[69] = {
#include "assets/actor_260500_animation_06878_records.inc"
};

static u16 _gActor260500Animation06878Indices[20] = {
#include "assets/actor_260500_animation_06878_indices.inc"
};

static AnimationSet _gActor260500Animation06878 = {
    _gActor260500Animation06878Records,
    _gActor260500Animation06878Indices,
    { NULL, _gActor260500Animation06878Bank1, NULL, NULL, _gActor260500Animation06878Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation06AF0Bank1[3] = {
#include "assets/actor_260500_animation_06AF0_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation06AF0Bank4[28] = {
#include "assets/actor_260500_animation_06AF0_bank4.inc"
};

static AnimationRecord _gActor260500Animation06AF0Records[101] = {
#include "assets/actor_260500_animation_06AF0_records.inc"
};

static u16 _gActor260500Animation06AF0Indices[20] = {
#include "assets/actor_260500_animation_06AF0_indices.inc"
};

static AnimationSet _gActor260500Animation06AF0 = {
    _gActor260500Animation06AF0Records,
    _gActor260500Animation06AF0Indices,
    { NULL, _gActor260500Animation06AF0Bank1, NULL, NULL, _gActor260500Animation06AF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation06DF0Bank1[6] = {
#include "assets/actor_260500_animation_06DF0_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation06DF0Bank4[61] = {
#include "assets/actor_260500_animation_06DF0_bank4.inc"
};

static AnimationRecord _gActor260500Animation06DF0Records[93] = {
#include "assets/actor_260500_animation_06DF0_records.inc"
};

static u16 _gActor260500Animation06DF0Indices[20] = {
#include "assets/actor_260500_animation_06DF0_indices.inc"
};

static AnimationSet _gActor260500Animation06DF0 = {
    _gActor260500Animation06DF0Records,
    _gActor260500Animation06DF0Indices,
    { NULL, _gActor260500Animation06DF0Bank1, NULL, NULL, _gActor260500Animation06DF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation07118Bank1[4] = {
#include "assets/actor_260500_animation_07118_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation07118Bank4[57] = {
#include "assets/actor_260500_animation_07118_bank4.inc"
};

static AnimationRecord _gActor260500Animation07118Records[113] = {
#include "assets/actor_260500_animation_07118_records.inc"
};

static u16 _gActor260500Animation07118Indices[20] = {
#include "assets/actor_260500_animation_07118_indices.inc"
};

static AnimationSet _gActor260500Animation07118 = {
    _gActor260500Animation07118Records,
    _gActor260500Animation07118Indices,
    { NULL, _gActor260500Animation07118Bank1, NULL, NULL, _gActor260500Animation07118Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation07698Bank1[8] = {
#include "assets/actor_260500_animation_07698_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation07698Bank4[119] = {
#include "assets/actor_260500_animation_07698_bank4.inc"
};

static AnimationRecord _gActor260500Animation07698Records[189] = {
#include "assets/actor_260500_animation_07698_records.inc"
};

static u16 _gActor260500Animation07698Indices[20] = {
#include "assets/actor_260500_animation_07698_indices.inc"
};

static AnimationSet _gActor260500Animation07698 = {
    _gActor260500Animation07698Records,
    _gActor260500Animation07698Indices,
    { NULL, _gActor260500Animation07698Bank1, NULL, NULL, _gActor260500Animation07698Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation07870Bank1[3] = {
#include "assets/actor_260500_animation_07870_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation07870Bank4[32] = {
#include "assets/actor_260500_animation_07870_bank4.inc"
};

static AnimationRecord _gActor260500Animation07870Records[57] = {
#include "assets/actor_260500_animation_07870_records.inc"
};

static u16 _gActor260500Animation07870Indices[20] = {
#include "assets/actor_260500_animation_07870_indices.inc"
};

static AnimationSet _gActor260500Animation07870 = {
    _gActor260500Animation07870Records,
    _gActor260500Animation07870Indices,
    { NULL, _gActor260500Animation07870Bank1, NULL, NULL, _gActor260500Animation07870Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation07B3CBank1[4] = {
#include "assets/actor_260500_animation_07B3C_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation07B3CBank4[51] = {
#include "assets/actor_260500_animation_07B3C_bank4.inc"
};

static AnimationRecord _gActor260500Animation07B3CRecords[96] = {
#include "assets/actor_260500_animation_07B3C_records.inc"
};

static u16 _gActor260500Animation07B3CIndices[20] = {
#include "assets/actor_260500_animation_07B3C_indices.inc"
};

static AnimationSet _gActor260500Animation07B3C = {
    _gActor260500Animation07B3CRecords,
    _gActor260500Animation07B3CIndices,
    { NULL, _gActor260500Animation07B3CBank1, NULL, NULL, _gActor260500Animation07B3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation07D1CBank1[3] = {
#include "assets/actor_260500_animation_07D1C_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation07D1CBank4[34] = {
#include "assets/actor_260500_animation_07D1C_bank4.inc"
};

static AnimationRecord _gActor260500Animation07D1CRecords[57] = {
#include "assets/actor_260500_animation_07D1C_records.inc"
};

static u16 _gActor260500Animation07D1CIndices[20] = {
#include "assets/actor_260500_animation_07D1C_indices.inc"
};

static AnimationSet _gActor260500Animation07D1C = {
    _gActor260500Animation07D1CRecords,
    _gActor260500Animation07D1CIndices,
    { NULL, _gActor260500Animation07D1CBank1, NULL, NULL, _gActor260500Animation07D1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation07FC8Bank1[3] = {
#include "assets/actor_260500_animation_07FC8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation07FC8Bank4[41] = {
#include "assets/actor_260500_animation_07FC8_bank4.inc"
};

static AnimationRecord _gActor260500Animation07FC8Records[101] = {
#include "assets/actor_260500_animation_07FC8_records.inc"
};

static u16 _gActor260500Animation07FC8Indices[20] = {
#include "assets/actor_260500_animation_07FC8_indices.inc"
};

static AnimationSet _gActor260500Animation07FC8 = {
    _gActor260500Animation07FC8Records,
    _gActor260500Animation07FC8Indices,
    { NULL, _gActor260500Animation07FC8Bank1, NULL, NULL, _gActor260500Animation07FC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation081E4Bank1[2] = {
#include "assets/actor_260500_animation_081E4_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation081E4Bank4[24] = {
#include "assets/actor_260500_animation_081E4_bank4.inc"
};

static AnimationRecord _gActor260500Animation081E4Records[85] = {
#include "assets/actor_260500_animation_081E4_records.inc"
};

static u16 _gActor260500Animation081E4Indices[20] = {
#include "assets/actor_260500_animation_081E4_indices.inc"
};

static AnimationSet _gActor260500Animation081E4 = {
    _gActor260500Animation081E4Records,
    _gActor260500Animation081E4Indices,
    { NULL, _gActor260500Animation081E4Bank1, NULL, NULL, _gActor260500Animation081E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation084E4Bank1[6] = {
#include "assets/actor_260500_animation_084E4_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation084E4Bank4[61] = {
#include "assets/actor_260500_animation_084E4_bank4.inc"
};

static AnimationRecord _gActor260500Animation084E4Records[93] = {
#include "assets/actor_260500_animation_084E4_records.inc"
};

static u16 _gActor260500Animation084E4Indices[20] = {
#include "assets/actor_260500_animation_084E4_indices.inc"
};

static AnimationSet _gActor260500Animation084E4 = {
    _gActor260500Animation084E4Records,
    _gActor260500Animation084E4Indices,
    { NULL, _gActor260500Animation084E4Bank1, NULL, NULL, _gActor260500Animation084E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation087E8Bank1[6] = {
#include "assets/actor_260500_animation_087E8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation087E8Bank4[46] = {
#include "assets/actor_260500_animation_087E8_bank4.inc"
};

static AnimationRecord _gActor260500Animation087E8Records[109] = {
#include "assets/actor_260500_animation_087E8_records.inc"
};

static u16 _gActor260500Animation087E8Indices[20] = {
#include "assets/actor_260500_animation_087E8_indices.inc"
};

static AnimationSet _gActor260500Animation087E8 = {
    _gActor260500Animation087E8Records,
    _gActor260500Animation087E8Indices,
    { NULL, _gActor260500Animation087E8Bank1, NULL, NULL, _gActor260500Animation087E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation08D68Bank1[10] = {
#include "assets/actor_260500_animation_08D68_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation08D68Bank4[104] = {
#include "assets/actor_260500_animation_08D68_bank4.inc"
};

static AnimationRecord _gActor260500Animation08D68Records[198] = {
#include "assets/actor_260500_animation_08D68_records.inc"
};

static u16 _gActor260500Animation08D68Indices[20] = {
#include "assets/actor_260500_animation_08D68_indices.inc"
};

static AnimationSet _gActor260500Animation08D68 = {
    _gActor260500Animation08D68Records,
    _gActor260500Animation08D68Indices,
    { NULL, _gActor260500Animation08D68Bank1, NULL, NULL, _gActor260500Animation08D68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation09148Bank1[8] = {
#include "assets/actor_260500_animation_09148_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation09148Bank4[85] = {
#include "assets/actor_260500_animation_09148_bank4.inc"
};

static AnimationRecord _gActor260500Animation09148Records[119] = {
#include "assets/actor_260500_animation_09148_records.inc"
};

static u16 _gActor260500Animation09148Indices[20] = {
#include "assets/actor_260500_animation_09148_indices.inc"
};

static AnimationSet _gActor260500Animation09148 = {
    _gActor260500Animation09148Records,
    _gActor260500Animation09148Indices,
    { NULL, _gActor260500Animation09148Bank1, NULL, NULL, _gActor260500Animation09148Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation094B0Bank1[5] = {
#include "assets/actor_260500_animation_094B0_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation094B0Bank4[74] = {
#include "assets/actor_260500_animation_094B0_bank4.inc"
};

static AnimationRecord _gActor260500Animation094B0Records[109] = {
#include "assets/actor_260500_animation_094B0_records.inc"
};

static u16 _gActor260500Animation094B0Indices[20] = {
#include "assets/actor_260500_animation_094B0_indices.inc"
};

static AnimationSet _gActor260500Animation094B0 = {
    _gActor260500Animation094B0Records,
    _gActor260500Animation094B0Indices,
    { NULL, _gActor260500Animation094B0Bank1, NULL, NULL, _gActor260500Animation094B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation097C8Bank1[5] = {
#include "assets/actor_260500_animation_097C8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation097C8Bank4[66] = {
#include "assets/actor_260500_animation_097C8_bank4.inc"
};

static AnimationRecord _gActor260500Animation097C8Records[97] = {
#include "assets/actor_260500_animation_097C8_records.inc"
};

static u16 _gActor260500Animation097C8Indices[20] = {
#include "assets/actor_260500_animation_097C8_indices.inc"
};

static AnimationSet _gActor260500Animation097C8 = {
    _gActor260500Animation097C8Records,
    _gActor260500Animation097C8Indices,
    { NULL, _gActor260500Animation097C8Bank1, NULL, NULL, _gActor260500Animation097C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation09B9CBank1[8] = {
#include "assets/actor_260500_animation_09B9C_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation09B9CBank4[84] = {
#include "assets/actor_260500_animation_09B9C_bank4.inc"
};

static AnimationRecord _gActor260500Animation09B9CRecords[117] = {
#include "assets/actor_260500_animation_09B9C_records.inc"
};

static u16 _gActor260500Animation09B9CIndices[20] = {
#include "assets/actor_260500_animation_09B9C_indices.inc"
};

static AnimationSet _gActor260500Animation09B9C = {
    _gActor260500Animation09B9CRecords,
    _gActor260500Animation09B9CIndices,
    { NULL, _gActor260500Animation09B9CBank1, NULL, NULL, _gActor260500Animation09B9CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation09F8CBank1[7] = {
#include "assets/actor_260500_animation_09F8C_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation09F8CBank4[62] = {
#include "assets/actor_260500_animation_09F8C_bank4.inc"
};

static AnimationRecord _gActor260500Animation09F8CRecords[149] = {
#include "assets/actor_260500_animation_09F8C_records.inc"
};

static u16 _gActor260500Animation09F8CIndices[20] = {
#include "assets/actor_260500_animation_09F8C_indices.inc"
};

static AnimationSet _gActor260500Animation09F8C = {
    _gActor260500Animation09F8CRecords,
    _gActor260500Animation09F8CIndices,
    { NULL, _gActor260500Animation09F8CBank1, NULL, NULL, _gActor260500Animation09F8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation0A2F8Bank1[7] = {
#include "assets/actor_260500_animation_0A2F8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation0A2F8Bank4[74] = {
#include "assets/actor_260500_animation_0A2F8_bank4.inc"
};

static AnimationRecord _gActor260500Animation0A2F8Records[104] = {
#include "assets/actor_260500_animation_0A2F8_records.inc"
};

static u16 _gActor260500Animation0A2F8Indices[20] = {
#include "assets/actor_260500_animation_0A2F8_indices.inc"
};

static AnimationSet _gActor260500Animation0A2F8 = {
    _gActor260500Animation0A2F8Records,
    _gActor260500Animation0A2F8Indices,
    { NULL, _gActor260500Animation0A2F8Bank1, NULL, NULL, _gActor260500Animation0A2F8Bank4, NULL, NULL, NULL },
};

static TmdBone _gActor260500JodieBouquetBody2Skeleton[19] = {
#include "assets/jodie_bouquet_body_2_skeleton.inc"
};

static u32 _gActor260500JodieBouquetBody2PartVerts[19] = {
#include "assets/jodie_bouquet_body_2_partVerts.inc"
};

static SVECTOR _gActor260500JodieBouquetBody2Verts[369] = {
#include "assets/jodie_bouquet_body_2_verts.inc"
};

static SVECTOR _gActor260500JodieBouquetBody2Normals[369] = {
#include "assets/jodie_bouquet_body_2_normals.inc"
};

static u32 _gActor260500JodieBouquetBody2Stream[4228] = {
#include "assets/jodie_bouquet_body_2_stream.inc"
};

static TmdSource _gActor260500JodieBouquetBody2 = {
    0,
    22752,
    6928,
    19,
    _gActor260500JodieBouquetBody2PartVerts,
    _gActor260500JodieBouquetBody2Verts,
    _gActor260500JodieBouquetBody2Normals,
    _gActor260500JodieBouquetBody2Skeleton,
    _gActor260500JodieBouquetBody2Stream,
};

s16 gFootstepWalkBlendFrames = 8;

TaskMessageEntry D_actor_260500_80159D80[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_260500_8014A6C4 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_260500_8014A754 },
    { ACTOR_MESSAGE_PLACE, footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_260500_8014A818 },
    { ACTOR_MESSAGE_WALK_TO, func_actor_260500_8014A83C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_260500_80159DB0 = { { { TASK_BODY_TMD, 192 } }, func_actor_260500_8014A460, { .model = &_gActor260500JodieBouquetBody2 } };

u8 D_actor_260500_80159DBC[144] = {
    0,
    0,
    0,
    0,
    24,
    225,
    20,
    128,
    92,
    230,
    20,
    128,
    228,
    232,
    20,
    128,
    56,
    235,
    20,
    128,
    248,
    236,
    20,
    128,
    84,
    240,
    20,
    128,
    32,
    242,
    20,
    128,
    208,
    245,
    20,
    128,
    244,
    247,
    20,
    128,
    216,
    251,
    20,
    128,
    216,
    254,
    20,
    128,
    148,
    2,
    21,
    128,
    96,
    4,
    21,
    128,
    152,
    6,
    21,
    128,
    16,
    9,
    21,
    128,
    16,
    12,
    21,
    128,
    56,
    15,
    21,
    128,
    184,
    20,
    21,
    128,
    144,
    22,
    21,
    128,
    92,
    25,
    21,
    128,
    60,
    27,
    21,
    128,
    232,
    29,
    21,
    128,
    4,
    32,
    21,
    128,
    4,
    35,
    21,
    128,
    0,
    0,
    0,
    0,
    8,
    38,
    21,
    128,
    136,
    43,
    21,
    128,
    104,
    47,
    21,
    128,
    208,
    50,
    21,
    128,
    232,
    53,
    21,
    128,
    188,
    57,
    21,
    128,
    172,
    61,
    21,
    128,
    24,
    65,
    21,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

FootstepWalkQuietWork* gFootstepWalkWork = NULL;

Task* D_actor_260500_80159E50;

s16 gFootstepWalkMode;

void        func_actor_260500_80149E80(void);
void        func_actor_260500_80149EBC(void);
static void func_actor_260500_80149FB0(Enemy* enemy, Task* task);

/// Loads cap file 2 and selects its font page (`capSetTexturePage(0x340, 0)`) when `arg0` is
/// non-zero, otherwise resets the cap state.
void func_actor_260500_80149E38(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(2);
        capSetTexturePage(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

/// Sends message 0x7D4 (placement) with the record at
/// `D_actor_260500_8014CAF4.data.actorPlacements[0]` to the task in lookup slot 4, when there is one.
void func_actor_260500_80149E80(void)
{
    Task* slot;

    slot = Gp_LookupSlot4(0);
    if (slot != 0) {
        TASK_MESSAGE_DISPATCH_POINTER(slot, 0x7D4, &D_actor_260500_8014CAF4.data.actorPlacements[0], 0);
    }
}

void func_actor_260500_80149EBC(void)
{
    switch (gameFlagGetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS)) {
        case 0:
            func_800E8634(D_actor_260500_8014CBF8, 0, D_actor_260500_8014D630);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, 1);
            break;
        case 1:
            func_800E8614(D_actor_260500_8014D7C8, 0);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, 2);
            break;
        case 2:
            func_800E8614(D_actor_260500_8014D948, 0);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, 3);
            break;
        case 3:
            func_800E8614(D_actor_260500_8014DAB0, 0);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, 4);
            break;
        case 4:
            func_800E8614(D_actor_260500_8014DCC0, 0);
            break;
    }
}

/// Spawn routine (state 0 of `func_actor_260500_8014A460`): allocates the work
/// block and publishes it in `gFootstepWalkWork` and the task's `work`
/// slot, binds the model to the view and hands it the block's light and colour
/// matrices, publishes the task in `D_actor_260500_80159E50`, relights the
/// model from a point 0x320 above its translation and binds the animation
/// stream. It then installs the message table and runs the first update with
/// the reset mode 2 / id 4 it seeds.
static void func_actor_260500_80149FB0(Enemy* enemy, Task* task)
{
    VECTOR     vec;
    GfxCoord*  coord;
    TmdObject* obj;
    void*      work;

    obj               = task->extra.tmd;
    coord             = obj->coords;
    work              = memCalloc(sizeof(FootstepWalkQuietWork), 0);
    gFootstepWalkWork = work;
    task->work        = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_260500_8014A540;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->lightMtx                    = &gFootstepWalkWork->light;
    obj->colorMtx                    = &gFootstepWalkWork->color;
    vec.vx                           = coord->workm.t[0];
    vec.vy                           = coord->workm.t[1] - 0x320;
    D_actor_260500_80159E50          = task;
    vec.vz                           = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationInitContext(&gFootstepWalkWork->rig.anim, (AnimationSet**)D_actor_260500_80159DBC, obj,
                         gFootstepWalkWork->rig.poses, gFootstepWalkWork->rig.slots);
    gFootstepWalkWork->st.animId  = 4;
    gFootstepWalkWork->st.state   = ACTOR_ENEMY_ANIM_RESET;
    gFootstepWalkWork->st.travel  = 0;
    gFootstepWalkWork->turnFrames = 0;
    task->msgTable                = D_actor_260500_80159D80;
    footstepWalkQuietUpdate(task);
    task->state++;
}

#include "../../shared/footstep_walk_quiet_update.inc.c"

/// Two-state task handler: publishes the task's work block in
/// `gFootstepWalkWork` on the way through, then calls the spawn routine
/// or the per-frame state, whichever `Task::state` selects from a table built
/// on the stack.
void func_actor_260500_8014A460(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_260500_80149FB0,
        func_actor_260500_8014A4BC,
    };

    gFootstepWalkWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_260500_8014A4BC
#define walkerUpdate     footstepWalkQuietUpdate
#define walkerDrawShadow walkerDrawShadow
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// `Task::exitCallback` the spawn routine installs: hands the task's `Enemy`
/// back to `enemyDestroy`.
static void func_actor_260500_8014A540(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/footstep_walk_tick_anim.inc.c"

#include "../../shared/footstep_walk_quiet_reset_anim.inc.c"

#include "../../shared/footstep_walk_quiet_blend_anim.inc.c"

/// Play-animation message handler: adopts the preset's animation id when it is
/// one of the first 0x24, latching the reset mode -- 1 for the blended reseed,
/// 2 for the plain one -- and the reset argument the blended reseed forwards,
/// then runs the update on the actor's task. Ids past the range are rejected
/// with -1 and leave the work block untouched.
s32 func_actor_260500_8014A6C4(Task* task, s32 arg1, AnimationPlayRequest* preset, s32 arg3)
{
    if (preset->animationId < 0x24) {
        gFootstepWalkWork->st.animId = preset->animationId;
        if (preset->blend != ANIMATION_BLEND_RESET) {
            gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            gFootstepWalkBlendFrames    = preset->blendFrames;
        } else {
            gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        gFootstepWalkWork->st.field_6 = 0;
        footstepWalkQuietUpdate(D_actor_260500_80159E50);
        return 0;
    }
    return -1;
}

/// Visibility handler: bit 0 of `arg2` shows the actor's model (flags 0) or
/// hides it (0x80), and bit 1 ORs in 0x4.
s32 func_actor_260500_8014A754(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* obj;

    obj = D_actor_260500_80159E50->extra.tmd;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (arg2 & 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/footstep_walk_place.inc.c"

/// Message 0x7DB: a zero payload halfword at 0x2 arms the work block's
/// `turnFrames` at 0x14.
s32 func_actor_260500_8014A818(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    if (msg->command == 0) {
        gFootstepWalkWork->turnFrames = 0x14;
    }
    return 0;
}

/// Approach handler: turns the model to face `target` -- away from it in mode
/// 1, where the update then walks it backwards -- keeps the mode in
/// `gFootstepWalkMode`, and stores the number of steps the walk takes:
/// the planar distance over the mode's step length, 60 in mode 0, 15 in mode 1
/// and 25 in mode 2.
s32 func_actor_260500_8014A83C(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GfxCoord*              coord;
    FootstepWalkQuietWork* work;
    s32                    steps;
    s32                    dx;
    s32                    dz;
    s32                    dist;
    s32                    angle;

    steps             = 0;
    coord             = task->extra.tmd->coords;
    work              = task->work;
    gFootstepWalkMode = mode;
    dx                = target->vx - coord->coord.t[0];
    dz                = target->vz - coord->coord.t[2];
    angle             = ratan2(dx, dz);
    work->st.yaw      = angle;
    if (gFootstepWalkMode == 1) {
        work->st.yaw = angle + 0x800;
    }
    gfxRotMatrixY(&coord->coord, work->st.yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (gFootstepWalkMode) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            steps = 0x19;
            break;
    }
    work->st.travel = dist / steps;
    return 0;
}

#include "../../shared/walker_shadow.inc.c"
