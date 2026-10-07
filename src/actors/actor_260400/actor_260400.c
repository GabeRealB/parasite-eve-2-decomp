#include "actors/actor_260400.h"

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
#include "gameplay/items.h"
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
// The scripted walk's update and walk-to handler run on this package's block.
#define SCRIPTED_WALK_WORK_T _Actor260400Work
#include "../../shared/scripted_walk.h"
#include "../../shared/walker.h"

/// The clips the package adds to the player's animation bank, with the records
/// stored after them.
///
/// The package's event scripts send `data.copy` to the player. The copy takes
/// `ANIMATION_BANK_EXTENSION_CAPACITY` words from the start of the storage,
/// which is more than the clip table holds: the ten set pointers occupy
/// extended ids 47-56, and the request itself, the two play requests, the
/// first placement and the position of the second are written into the bank
/// after them. The player's requests select ids 47-49, 53-56 and the bank's
/// own id 1 only, so neither a NULL entry nor a following word is played as a
/// clip.
///
/// The play requests and the placements have no other connection to the
/// clips; they are part of this object only because the copied span reaches
/// into the second placement. The storage is only read.
typedef union {
    struct {
        AnimationSet*            sets[10];                  // Player clips for extended ids 47-56; NULL at ids 50 to 52, which nothing requests
        AnimationBankCopyRequest copy;                      // Installs the first `ANIMATION_BANK_EXTENSION_CAPACITY` words of this storage in the player's bank extension
        AnimationPlayRequest     playerBaseClipRequests[2]; // Play the player bank's own clip 1 (0 restarted, as the first conversation opens and when it ends or is skipped; 1 blended in over 8 frames, as two of the later conversations open)
        ActorTransform           actorPlacements[2];        // Where the scripts stand the actor at scene placement 0, the same spot in both (0 after the first conversation ends or is skipped, 1 as it opens and each time the actor returns during it)
    } data;                                                 // The records by name
    s32 words[34];                                          // The same storage as the copy reads it; the last two words lie beyond the copied span
} _Actor260400AnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor260400AnimationBankExtensionStorage, 136);

extern _Actor260400AnimationBankExtensionStorage D_actor_260400_8014C668;

/// Work block of the package's scripted walker, the wounded Rupert Broderick,
/// allocated zeroed by the walker's spawn state and kept both at `Task::work`
/// and in `_gScriptedWalkWork`.
///
/// The scripted walk library's fragments are compiled against it through
/// `SCRIPTED_WALK_WORK_T` and use its leading members: the matrices, the rig,
/// the enemy state and the turn counter. What follows is the walker's revolver: a task of
/// its own that draws the one-part Mongoose model and hangs that model's
/// coordinate off part 8 of the walker's rig, so the revolver follows that
/// part. The spawn state starts it and the walker's exit callback kills it.
/// A spawn that fails leaves `mongoose` NULL, which the message handlers do
/// not check for.
///
/// The revolver is drawn only between two actor commands of the package's
/// scripts, and not at all once the player holds a Mongoose.
typedef struct {
    MATRIX          light;         // Light-direction matrix lent to the model object
    MATRIX          color;         // Light-colour matrix lent to the model object
    ActorAnimRig20  rig;           // Playback storage of the twenty-part body model
    ActorEnemyState st;            // Animation request, heading last given the root and frames of walk left
    s16             turnFrames;    // Frames the update still turns the model for while the turn clip plays; command 0 starts 20
    Task*           mongoose;      // Task of the revolver model, started from spawn-table entry 1
    u8              mongooseShown; // Nonzero while the revolver is drawn with the walker: set by command 1 unless the player holds a Mongoose, cleared by command 2. While zero the model-draw message keeps the revolver hidden
} _Actor260400Work;
STATIC_ASSERT_SIZEOF(_Actor260400Work, 0x4F8);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern EvsCommand D_actor_260400_8014C788[];
extern EvsCommand D_actor_260400_8014CF38[];
extern EvsCommand D_actor_260400_8014D118[];
extern EvsCommand D_actor_260400_8014D208[];
extern EvsCommand D_actor_260400_8014D340[];
extern EvsCommand D_actor_260400_8014D4A8[];
extern EvsCommand D_actor_260400_8014D610[];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_260400_80154BE8[6];
extern TaskDesc         D_actor_260400_80154C18[];
extern u8               D_actor_260400_80154C30[];

/// Borrowed work block of the scripted walker and its revolver attachment.
///
/// The spawn and dispatcher publish the allocation also held by `Task::work`.
/// Animation and message handlers require it to remain live; task teardown
/// releases it without clearing this pointer.
static _Actor260400Work* _gScriptedWalkWork;

/// The actor's own task, published by the spawn routine: the revolver task
/// hangs its model off this task's model parts, the play-animation handler runs
/// the update on it, and the visibility handler reaches its model.
extern Task* D_actor_260400_80154C74;

/// Approach mode of the scripted walker, stored as `SCRIPTED_WALK_MODE_*`.
///
/// The walk-to message narrows its argument to this signed halfword. Its value
/// selects a 60-unit forward, 15-unit backward or 25-unit forward step until
/// the next approach message.
static s16 _gScriptedWalkMode;

static void func_actor_260400_8014A5AC(Enemy* enemy, Task* task);
static void _actor260400ExitScriptedWalker(Task* task);

static TmdSource _gActor260400RupertBroderickHurtMongoose;
static TmdSource _gActor260400RupertBroderickHurtBody;
void             func_actor_260400_8014A550(Task*);
static void      _actor260400MongooseTask(Task* task);

static s32 _actor260400PlayScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32 _actor260400SetScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument);
s32        func_actor_260400_8014AAA4(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);

extern AnimationPlayRequest D_actor_260400_8014C4D8;
extern AnimationPlayRequest D_actor_260400_8014C4EC;
extern AnimationPlayRequest D_actor_260400_8014C500;
extern AnimationPlayRequest D_actor_260400_8014C514;
extern AnimationPlayRequest D_actor_260400_8014C528;
extern AnimationPlayRequest D_actor_260400_8014C53C;
extern AnimationPlayRequest D_actor_260400_8014C550;
extern AnimationPlayRequest D_actor_260400_8014C564;
extern AnimationPlayRequest D_actor_260400_8014C578;
extern AnimationPlayRequest D_actor_260400_8014C58C;
extern AnimationPlayRequest D_actor_260400_8014C5A0;
extern AnimationPlayRequest D_actor_260400_8014C5B4;
extern AnimationPlayRequest D_actor_260400_8014C5DC;
extern AnimationPlayRequest D_actor_260400_8014C5F0;
extern AnimationPlayRequest D_actor_260400_8014C604;
extern AnimationPlayRequest D_actor_260400_8014C618;
extern AnimationPlayRequest D_actor_260400_8014C62C;
extern AnimationPlayRequest D_actor_260400_8014C640;
extern AnimationPlayRequest D_actor_260400_8014C654;
static void                 _actor260400SelectConversationCaptions(s32 useConversationFile);

static AnimationSet _gActor260400Animation0105C;
static AnimationSet _gActor260400Animation01300;
static AnimationSet _gActor260400Animation017AC;
static AnimationSet _gActor260400Animation01B64;
static AnimationSet _gActor260400Animation0204C;
static AnimationSet _gActor260400Animation02350;
static AnimationSet _gActor260400Animation0267C;

static AnimationPackedPose _gActor260400Animation0105CBank1[2] = {
#include "assets/actor_260400_animation_0105C_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation0105CBank4[37] = {
#include "assets/actor_260400_animation_0105C_bank4.inc"
};

static AnimationRecord _gActor260400Animation0105CRecords[72] = {
#include "assets/actor_260400_animation_0105C_records.inc"
};

static u16 _gActor260400Animation0105CIndices[20] = {
#include "assets/actor_260400_animation_0105C_indices.inc"
};

static AnimationSet _gActor260400Animation0105C = {
    _gActor260400Animation0105CRecords,
    _gActor260400Animation0105CIndices,
    { NULL, _gActor260400Animation0105CBank1, NULL, NULL, _gActor260400Animation0105CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation01300Bank1[2] = {
#include "assets/actor_260400_animation_01300_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation01300Bank4[49] = {
#include "assets/actor_260400_animation_01300_bank4.inc"
};

static AnimationRecord _gActor260400Animation01300Records[94] = {
#include "assets/actor_260400_animation_01300_records.inc"
};

static u16 _gActor260400Animation01300Indices[20] = {
#include "assets/actor_260400_animation_01300_indices.inc"
};

static AnimationSet _gActor260400Animation01300 = {
    _gActor260400Animation01300Records,
    _gActor260400Animation01300Indices,
    { NULL, _gActor260400Animation01300Bank1, NULL, NULL, _gActor260400Animation01300Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation017ACBank1[4] = {
#include "assets/actor_260400_animation_017AC_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation017ACBank4[81] = {
#include "assets/actor_260400_animation_017AC_bank4.inc"
};

static AnimationRecord _gActor260400Animation017ACRecords[186] = {
#include "assets/actor_260400_animation_017AC_records.inc"
};

static u16 _gActor260400Animation017ACIndices[20] = {
#include "assets/actor_260400_animation_017AC_indices.inc"
};

static AnimationSet _gActor260400Animation017AC = {
    _gActor260400Animation017ACRecords,
    _gActor260400Animation017ACIndices,
    { NULL, _gActor260400Animation017ACBank1, NULL, NULL, _gActor260400Animation017ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation01B64Bank1[3] = {
#include "assets/actor_260400_animation_01B64_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation01B64Bank4[81] = {
#include "assets/actor_260400_animation_01B64_bank4.inc"
};

static AnimationRecord _gActor260400Animation01B64Records[128] = {
#include "assets/actor_260400_animation_01B64_records.inc"
};

static u16 _gActor260400Animation01B64Indices[20] = {
#include "assets/actor_260400_animation_01B64_indices.inc"
};

static AnimationSet _gActor260400Animation01B64 = {
    _gActor260400Animation01B64Records,
    _gActor260400Animation01B64Indices,
    { NULL, _gActor260400Animation01B64Bank1, NULL, NULL, _gActor260400Animation01B64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation0204CBank1[8] = {
#include "assets/actor_260400_animation_0204C_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation0204CBank4[109] = {
#include "assets/actor_260400_animation_0204C_bank4.inc"
};

static AnimationRecord _gActor260400Animation0204CRecords[161] = {
#include "assets/actor_260400_animation_0204C_records.inc"
};

static u16 _gActor260400Animation0204CIndices[20] = {
#include "assets/actor_260400_animation_0204C_indices.inc"
};

static AnimationSet _gActor260400Animation0204C = {
    _gActor260400Animation0204CRecords,
    _gActor260400Animation0204CIndices,
    { NULL, _gActor260400Animation0204CBank1, NULL, NULL, _gActor260400Animation0204CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation02350Bank1[6] = {
#include "assets/actor_260400_animation_02350_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation02350Bank4[46] = {
#include "assets/actor_260400_animation_02350_bank4.inc"
};

static AnimationRecord _gActor260400Animation02350Records[109] = {
#include "assets/actor_260400_animation_02350_records.inc"
};

static u16 _gActor260400Animation02350Indices[20] = {
#include "assets/actor_260400_animation_02350_indices.inc"
};

static AnimationSet _gActor260400Animation02350 = {
    _gActor260400Animation02350Records,
    _gActor260400Animation02350Indices,
    { NULL, _gActor260400Animation02350Bank1, NULL, NULL, _gActor260400Animation02350Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation0267CBank1[7] = {
#include "assets/actor_260400_animation_0267C_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation0267CBank4[56] = {
#include "assets/actor_260400_animation_0267C_bank4.inc"
};

static AnimationRecord _gActor260400Animation0267CRecords[106] = {
#include "assets/actor_260400_animation_0267C_records.inc"
};

static u16 _gActor260400Animation0267CIndices[20] = {
#include "assets/actor_260400_animation_0267C_indices.inc"
};

static AnimationSet _gActor260400Animation0267C = {
    _gActor260400Animation0267CRecords,
    _gActor260400Animation0267CIndices,
    { NULL, _gActor260400Animation0267CBank1, NULL, NULL, _gActor260400Animation0267CBank4, NULL, NULL, NULL },
};

AnimationPlayRequest D_actor_260400_8014C4C4 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C4D8 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C4EC = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C500 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C514 = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C528 = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C53C = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C550 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C564 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C578 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C58C = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C5A0 = { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C5B4 = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_260400_8014C5C8 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C5DC = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C5F0 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C604 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C618 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C62C = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C640 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260400_8014C654 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

_Actor260400AnimationBankExtensionStorage D_actor_260400_8014C668 = { .data = { { &_gActor260400Animation0105C, &_gActor260400Animation01300, &_gActor260400Animation017AC, NULL, NULL, NULL, &_gActor260400Animation01B64, &_gActor260400Animation0204C, &_gActor260400Animation02350, &_gActor260400Animation0267C }, { { .words = D_actor_260400_8014C668.words }, ANIMATION_BANK_EXTENSION_CAPACITY }, { { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE } }, { { { 860, 0, 6730, 0 }, { 0, -2048, 0, 0 } }, { { 860, 0, 6730, 0 }, { 0, -2048, 0, 0 } } } } };

ActorTransform D_actor_260400_8014C6F0 = { { 860, 0, 6910, 0 }, { 0, -2161, 0, 0 } };

ActorTransform D_actor_260400_8014C708 = { { 860, 0, 6640, 0 }, { 0, -2275, 0, 0 } };

ActorTransform D_actor_260400_8014C720 = { { 920, 0, 6000, 0 }, { 0, -113, 0, 0 } };

ActorTransform D_actor_260400_8014C738 = { { 1010, 0, 5840, 0 }, { 0, -113, 0, 0 } };

ActorTransform D_actor_260400_8014C750 = { { 1210, 0, 5610, 0 }, { 0, -227, 0, 0 } };

ActorTransform D_actor_260400_8014C768 = { { 860, 0, 6180, 0 }, { 0, 0, 0, 0 } };

ActorCommand D_actor_260400_8014C780 = { { .loc = { 5, 4 } }, 1 };

ActorCommand D_actor_260400_8014C784 = { { .loc = { 5, 4 } }, 2 };

EvsCommand D_actor_260400_8014C788[82] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor260400SelectConversationCaptions }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C668.data.playerBaseClipRequests[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260400_8014C668.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260400_8014C720 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260400_8014C668.data.actorPlacements[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C4EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260400_8014C720 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260400_8014C6F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C618 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C5B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_260400_8014C780 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260400_8014C738 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260400_8014C708 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C500 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C5DC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_260400_8014C784 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260400_8014C750 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C5F0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 65 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260400_8014C668.data.actorPlacements[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C4D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C514 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C528 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C53C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260400_8014C750 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C604 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 72 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260400_8014C668.data.actorPlacements[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C550 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C564 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C618 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260400_8014C668.data.actorPlacements[0] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C4D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260400_8014C768 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C668.data.playerBaseClipRequests[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor260400SelectConversationCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260400_8014CF38[20] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C668.data.playerBaseClipRequests[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260400_8014C668.data.actorPlacements[0] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C4D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260400_8014C768 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_260400_8014C784 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor260400SelectConversationCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260400_8014D118[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260400_8014C668.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 26 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C668.data.playerBaseClipRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C528 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C53C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C4D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260400_8014D208[13] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260400_8014C668.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C528 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C654 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C53C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C4D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260400_8014D340[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260400_8014C668.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C528 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C53C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C578 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C4D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C618 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260400_8014D4A8[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260400_8014C668.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C640 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C528 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C53C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C58C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C4D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C62C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260400_8014D610[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260400_8014C668.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260400_8014C668.data.playerBaseClipRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C5A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260400_8014C4D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static AnimationPackedPose _gActor260400Animation03B24Bank1[3] = {
#include "assets/actor_260400_animation_03B24_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation03B24Bank4[26] = {
#include "assets/actor_260400_animation_03B24_bank4.inc"
};

static AnimationRecord _gActor260400Animation03B24Records[106] = {
#include "assets/actor_260400_animation_03B24_records.inc"
};

static u16 _gActor260400Animation03B24Indices[20] = {
#include "assets/actor_260400_animation_03B24_indices.inc"
};

static AnimationSet _gActor260400Animation03B24 = {
    _gActor260400Animation03B24Records,
    _gActor260400Animation03B24Indices,
    { NULL, _gActor260400Animation03B24Bank1, NULL, NULL, _gActor260400Animation03B24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation03CCCBank1[2] = {
#include "assets/actor_260400_animation_03CCC_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation03CCCBank4[16] = {
#include "assets/actor_260400_animation_03CCC_bank4.inc"
};

static AnimationRecord _gActor260400Animation03CCCRecords[64] = {
#include "assets/actor_260400_animation_03CCC_records.inc"
};

static u16 _gActor260400Animation03CCCIndices[20] = {
#include "assets/actor_260400_animation_03CCC_indices.inc"
};

static AnimationSet _gActor260400Animation03CCC = {
    _gActor260400Animation03CCCRecords,
    _gActor260400Animation03CCCIndices,
    { NULL, _gActor260400Animation03CCCBank1, NULL, NULL, _gActor260400Animation03CCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation03EDCBank1[4] = {
#include "assets/actor_260400_animation_03EDC_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation03EDCBank4[32] = {
#include "assets/actor_260400_animation_03EDC_bank4.inc"
};

static AnimationRecord _gActor260400Animation03EDCRecords[68] = {
#include "assets/actor_260400_animation_03EDC_records.inc"
};

static u16 _gActor260400Animation03EDCIndices[20] = {
#include "assets/actor_260400_animation_03EDC_indices.inc"
};

static AnimationSet _gActor260400Animation03EDC = {
    _gActor260400Animation03EDCRecords,
    _gActor260400Animation03EDCIndices,
    { NULL, _gActor260400Animation03EDCBank1, NULL, NULL, _gActor260400Animation03EDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation0409CBank1[2] = {
#include "assets/actor_260400_animation_0409C_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation0409CBank4[18] = {
#include "assets/actor_260400_animation_0409C_bank4.inc"
};

static AnimationRecord _gActor260400Animation0409CRecords[68] = {
#include "assets/actor_260400_animation_0409C_records.inc"
};

static u16 _gActor260400Animation0409CIndices[20] = {
#include "assets/actor_260400_animation_0409C_indices.inc"
};

static AnimationSet _gActor260400Animation0409C = {
    _gActor260400Animation0409CRecords,
    _gActor260400Animation0409CIndices,
    { NULL, _gActor260400Animation0409CBank1, NULL, NULL, _gActor260400Animation0409CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation0434CBank1[2] = {
#include "assets/actor_260400_animation_0434C_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation0434CBank4[33] = {
#include "assets/actor_260400_animation_0434C_bank4.inc"
};

static AnimationRecord _gActor260400Animation0434CRecords[113] = {
#include "assets/actor_260400_animation_0434C_records.inc"
};

static u16 _gActor260400Animation0434CIndices[20] = {
#include "assets/actor_260400_animation_0434C_indices.inc"
};

static AnimationSet _gActor260400Animation0434C = {
    _gActor260400Animation0434CRecords,
    _gActor260400Animation0434CIndices,
    { NULL, _gActor260400Animation0434CBank1, NULL, NULL, _gActor260400Animation0434CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation044F4Bank1[2] = {
#include "assets/actor_260400_animation_044F4_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation044F4Bank4[20] = {
#include "assets/actor_260400_animation_044F4_bank4.inc"
};

static AnimationRecord _gActor260400Animation044F4Records[60] = {
#include "assets/actor_260400_animation_044F4_records.inc"
};

static u16 _gActor260400Animation044F4Indices[20] = {
#include "assets/actor_260400_animation_044F4_indices.inc"
};

static AnimationSet _gActor260400Animation044F4 = {
    _gActor260400Animation044F4Records,
    _gActor260400Animation044F4Indices,
    { NULL, _gActor260400Animation044F4Bank1, NULL, NULL, _gActor260400Animation044F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation047F0Bank1[4] = {
#include "assets/actor_260400_animation_047F0_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation047F0Bank4[54] = {
#include "assets/actor_260400_animation_047F0_bank4.inc"
};

static AnimationRecord _gActor260400Animation047F0Records[105] = {
#include "assets/actor_260400_animation_047F0_records.inc"
};

static u16 _gActor260400Animation047F0Indices[20] = {
#include "assets/actor_260400_animation_047F0_indices.inc"
};

static AnimationSet _gActor260400Animation047F0 = {
    _gActor260400Animation047F0Records,
    _gActor260400Animation047F0Indices,
    { NULL, _gActor260400Animation047F0Bank1, NULL, NULL, _gActor260400Animation047F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation04A88Bank1[4] = {
#include "assets/actor_260400_animation_04A88_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation04A88Bank4[28] = {
#include "assets/actor_260400_animation_04A88_bank4.inc"
};

static AnimationRecord _gActor260400Animation04A88Records[106] = {
#include "assets/actor_260400_animation_04A88_records.inc"
};

static u16 _gActor260400Animation04A88Indices[20] = {
#include "assets/actor_260400_animation_04A88_indices.inc"
};

static AnimationSet _gActor260400Animation04A88 = {
    _gActor260400Animation04A88Records,
    _gActor260400Animation04A88Indices,
    { NULL, _gActor260400Animation04A88Bank1, NULL, NULL, _gActor260400Animation04A88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation04D14Bank1[4] = {
#include "assets/actor_260400_animation_04D14_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation04D14Bank4[34] = {
#include "assets/actor_260400_animation_04D14_bank4.inc"
};

static AnimationRecord _gActor260400Animation04D14Records[97] = {
#include "assets/actor_260400_animation_04D14_records.inc"
};

static u16 _gActor260400Animation04D14Indices[20] = {
#include "assets/actor_260400_animation_04D14_indices.inc"
};

static AnimationSet _gActor260400Animation04D14 = {
    _gActor260400Animation04D14Records,
    _gActor260400Animation04D14Indices,
    { NULL, _gActor260400Animation04D14Bank1, NULL, NULL, _gActor260400Animation04D14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation05094Bank1[3] = {
#include "assets/actor_260400_animation_05094_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation05094Bank4[66] = {
#include "assets/actor_260400_animation_05094_bank4.inc"
};

static AnimationRecord _gActor260400Animation05094Records[129] = {
#include "assets/actor_260400_animation_05094_records.inc"
};

static u16 _gActor260400Animation05094Indices[20] = {
#include "assets/actor_260400_animation_05094_indices.inc"
};

static AnimationSet _gActor260400Animation05094 = {
    _gActor260400Animation05094Records,
    _gActor260400Animation05094Indices,
    { NULL, _gActor260400Animation05094Bank1, NULL, NULL, _gActor260400Animation05094Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation0526CBank1[3] = {
#include "assets/actor_260400_animation_0526C_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation0526CBank4[19] = {
#include "assets/actor_260400_animation_0526C_bank4.inc"
};

static AnimationRecord _gActor260400Animation0526CRecords[70] = {
#include "assets/actor_260400_animation_0526C_records.inc"
};

static u16 _gActor260400Animation0526CIndices[20] = {
#include "assets/actor_260400_animation_0526C_indices.inc"
};

static AnimationSet _gActor260400Animation0526C = {
    _gActor260400Animation0526CRecords,
    _gActor260400Animation0526CIndices,
    { NULL, _gActor260400Animation0526CBank1, NULL, NULL, _gActor260400Animation0526CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260400Animation05474Bank1[2] = {
#include "assets/actor_260400_animation_05474_bank1.inc"
};

static AnimationPackedRotation _gActor260400Animation05474Bank4[26] = {
#include "assets/actor_260400_animation_05474_bank4.inc"
};

static AnimationRecord _gActor260400Animation05474Records[78] = {
#include "assets/actor_260400_animation_05474_records.inc"
};

static u16 _gActor260400Animation05474Indices[20] = {
#include "assets/actor_260400_animation_05474_indices.inc"
};

static AnimationSet _gActor260400Animation05474 = {
    _gActor260400Animation05474Records,
    _gActor260400Animation05474Indices,
    { NULL, _gActor260400Animation05474Bank1, NULL, NULL, _gActor260400Animation05474Bank4, NULL, NULL, NULL },
};

static TmdBone _gActor260400RupertBroderickHurtMongooseSkeleton[1] = {
#include "assets/rupert_broderick_hurt_mongoose_skeleton.inc"
};

static u32 _gActor260400RupertBroderickHurtMongoosePartVerts[1] = {
#include "assets/rupert_broderick_hurt_mongoose_partVerts.inc"
};

static SVECTOR _gActor260400RupertBroderickHurtMongooseVerts[28] = {
#include "assets/rupert_broderick_hurt_mongoose_verts.inc"
};

static SVECTOR _gActor260400RupertBroderickHurtMongooseNormals[28] = {
#include "assets/rupert_broderick_hurt_mongoose_normals.inc"
};

static u32 _gActor260400RupertBroderickHurtMongooseStream[211] = {
#include "assets/rupert_broderick_hurt_mongoose_stream.inc"
};

static TmdSource _gActor260400RupertBroderickHurtMongoose = {
    0,
    1464,
    0,
    1,
    _gActor260400RupertBroderickHurtMongoosePartVerts,
    _gActor260400RupertBroderickHurtMongooseVerts,
    _gActor260400RupertBroderickHurtMongooseNormals,
    _gActor260400RupertBroderickHurtMongooseSkeleton,
    _gActor260400RupertBroderickHurtMongooseStream,
};

static TmdBone _gActor260400RupertBroderickHurtBodySkeleton[20] = {
#include "assets/rupert_broderick_hurt_body_skeleton.inc"
};

static u32 _gActor260400RupertBroderickHurtBodyPartVerts[20] = {
#include "assets/rupert_broderick_hurt_body_partVerts.inc"
};

static SVECTOR _gActor260400RupertBroderickHurtBodyVerts[343] = {
#include "assets/rupert_broderick_hurt_body_verts.inc"
};

static SVECTOR _gActor260400RupertBroderickHurtBodyNormals[334] = {
#include "assets/rupert_broderick_hurt_body_normals.inc"
};

static u32 _gActor260400RupertBroderickHurtBodyStream[3801] = {
#include "assets/rupert_broderick_hurt_body_stream.inc"
};

static TmdSource _gActor260400RupertBroderickHurtBody = {
    0,
    20988,
    5348,
    20,
    _gActor260400RupertBroderickHurtBodyPartVerts,
    _gActor260400RupertBroderickHurtBodyVerts,
    _gActor260400RupertBroderickHurtBodyNormals,
    _gActor260400RupertBroderickHurtBodySkeleton,
    _gActor260400RupertBroderickHurtBodyStream,
};

/// Latched duration of the next child-part blend, in whole normal-rate frames.
///
/// Play requests narrow `AnimationPlayRequest.blendFrames` to this signed
/// halfword; walk completion replaces it with `SCRIPTED_WALK_IDLE_BLEND_FRAMES`.
/// Plain resets leave it intact. Zero requests no transition time; 0..2047
/// keeps the playback timer nonnegative. The range is not checked.
static s16 _gScriptedWalkBlendFrames = SCRIPTED_WALK_DEFAULT_BLEND_FRAMES;

TaskMessageEntry D_actor_260400_80154BE8[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor260400PlayScriptedWalkerAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor260400SetScriptedWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, _scriptedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_260400_8014AAA4 },
    { ACTOR_MESSAGE_WALK_TO, scriptedWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_260400_80154C18[2] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_260400_8014A550, { .model = &_gActor260400RupertBroderickHurtBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor260400MongooseTask, { .model = &_gActor260400RupertBroderickHurtMongoose } },
};

u8 D_actor_260400_80154C30[64] = {
    0,
    0,
    0,
    0,
    68,
    217,
    20,
    128,
    236,
    218,
    20,
    128,
    252,
    220,
    20,
    128,
    188,
    222,
    20,
    128,
    108,
    225,
    20,
    128,
    20,
    227,
    20,
    128,
    16,
    230,
    20,
    128,
    168,
    232,
    20,
    128,
    52,
    235,
    20,
    128,
    180,
    238,
    20,
    128,
    140,
    240,
    20,
    128,
    148,
    242,
    20,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

Task* D_actor_260400_80154C74;

static void func_actor_260400_80149FE0(Enemy* enemy, Task* task);

void actor260400StartHeliportConversation(void)
{
    enum {
        ACTOR_260400_TALK_INTRO            = 0,
        ACTOR_260400_TALK_AWAIT_GIFTS      = 1,
        ACTOR_260400_TALK_AFTER_GIFTS      = 2,
        ACTOR_260400_TALK_FOURTH           = 3,
        ACTOR_260400_TALK_REPEAT           = 4,
        ACTOR_260400_MAEDA_AMMO_OBJECT     = 4,
        ACTOR_260400_MONGOOSE_OBJECT       = 5,
        ACTOR_260400_GIFT_OBJECT_AVAILABLE = 1,
    };

    switch (gameFlagGetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS)) {
        case ACTOR_260400_TALK_INTRO:
            evsStartScriptWithSkip(D_actor_260400_8014C788, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_260400_8014CF38);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260400_TALK_AWAIT_GIFTS);
            break;
        case ACTOR_260400_TALK_AWAIT_GIFTS:
            // Repeat the reminder until neither gift has its available state.
            if ((areaGetCurrentObjectState(ACTOR_260400_MAEDA_AMMO_OBJECT) == ACTOR_260400_GIFT_OBJECT_AVAILABLE) ||
                (areaGetCurrentObjectState(ACTOR_260400_MONGOOSE_OBJECT) == ACTOR_260400_GIFT_OBJECT_AVAILABLE)) {
                evsStartScript(D_actor_260400_8014D118, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            } else {
                evsStartScript(D_actor_260400_8014D208, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260400_TALK_AFTER_GIFTS);
            }
            break;
        case ACTOR_260400_TALK_AFTER_GIFTS:
            evsStartScript(D_actor_260400_8014D340, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260400_TALK_FOURTH);
            break;
        case ACTOR_260400_TALK_FOURTH:
            evsStartScript(D_actor_260400_8014D4A8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260400_TALK_REPEAT);
            break;
        case ACTOR_260400_TALK_REPEAT:
            evsStartScript(D_actor_260400_8014D610, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

/// Selects the opening conversation's CAP file and texture page, or restores defaults.
///
/// Event scripts pass nonzero on entry and zero after completion or skip.
/// The selected file is data resource ordinal 1 of the loaded bundle, with
/// its text texture at (832, 0) VRAM pixels. Resources must already be loaded
/// and remain live through playback. A missing resource leaves the cleared
/// current-file pointer NULL; zero restores the default file and playback state.
static void _actor260400SelectConversationCaptions(s32 useConversationFile)
{
    enum {
        ACTOR_260400_CONVERSATION_CAP_FILE_ORDINAL = 1,
        ACTOR_260400_CONVERSATION_TEXTURE_VRAM_X   = 832,
        ACTOR_260400_CONVERSATION_TEXTURE_VRAM_Y   = 0,
    };

    if (useConversationFile != 0) {
        Gp_CapFile = NULL;
        capSelectLoadedFile(ACTOR_260400_CONVERSATION_CAP_FILE_ORDINAL);
        capSetTexturePage(ACTOR_260400_CONVERSATION_TEXTURE_VRAM_X, ACTOR_260400_CONVERSATION_TEXTURE_VRAM_Y);
        return;
    }
    capReset();
}

void actor260400RestoreHeliportPlacement(void)
{
    enum { ACTOR_260400_HELIPORT_PLACEMENT_INDEX = 0 };
    Task* walkerTask;

    walkerTask = sceneFindPlacedActor(ACTOR_260400_HELIPORT_PLACEMENT_INDEX);
    if (walkerTask != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(walkerTask, ACTOR_MESSAGE_PLACE, &D_actor_260400_8014C668.data.actorPlacements[0], 0);
    }
}

/// Spawn routine (state 0 of `func_actor_260400_8014A550`): allocates the work
/// block and publishes it in `_gScriptedWalkWork` and the task's `work`
/// slot, binds the model to the view and hands it the block's light and colour
/// matrices, publishes the task in `D_actor_260400_80154C74`, relights the
/// model from a point 0x320 above its translation and binds the animation
/// stream. It then starts the revolver task and textures the revolver's model from
/// the area placement record the spawning enemy names, before running the
/// first update with the reset mode 2 / id 1 it seeds.
static void func_actor_260400_80149FE0(Enemy* enemy, Task* task)
{
    VECTOR     vec;
    GfxCoord*  coord;
    TmdObject* obj;
    Task*      spawned;
    void*      work;

    obj                = task->extra.tmd;
    coord              = obj->coords;
    work               = memCalloc(sizeof(_Actor260400Work), 0);
    _gScriptedWalkWork = work;
    task->work         = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor260400ExitScriptedWalker;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->flags                       = 0;
    obj->lightMtx                    = &_gScriptedWalkWork->light;
    obj->colorMtx                    = &_gScriptedWalkWork->color;
    vec.vx                           = coord->workm.t[0];
    vec.vy                           = coord->workm.t[1] - 0x320;
    D_actor_260400_80154C74          = task;
    vec.vz                           = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationInitContext(&_gScriptedWalkWork->rig.anim, (AnimationSet**)D_actor_260400_80154C30, obj,
                         _gScriptedWalkWork->rig.poses, _gScriptedWalkWork->rig.slots);
    _gScriptedWalkWork->st.animId = 1;
    _gScriptedWalkWork->st.state  = ACTOR_ENEMY_ANIM_RESET;
    spawned                       = taskSpawnFromTable(D_actor_260400_80154C18, 1, 8, 0);
    if (spawned != NULL) {
        _gScriptedWalkWork->mongoose = spawned;
        actorTintTask(spawned, (Enemy*)task->spawnArg2.pointer);
    }
    _gScriptedWalkWork->st.travel     = 0;
    _gScriptedWalkWork->turnFrames    = 0;
    _gScriptedWalkWork->mongooseShown = 0;
    task->msgTable                    = D_actor_260400_80154BE8;
    _scriptedWalkUpdate(task);
    task->state++;
}

#include "../../shared/scripted_walk_update.inc.c"

/// Two-state task handler: publishes the task's work block in
/// `_gScriptedWalkWork` on the way through, then calls the spawn routine
/// or the per-frame state, whichever `Task::state` selects from a table built
/// on the stack.
void func_actor_260400_8014A550(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_260400_80149FE0,
        func_actor_260400_8014A5AC,
    };

    _gScriptedWalkWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame  func_actor_260400_8014A5AC
#define walkerUpdate _scriptedWalkUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the scripted walker's enemy and task, then tears down its Mongoose task.
///
/// Installed as the walker's exit callback. Requires its enemy, allocated work
/// and successfully spawned Mongoose task. The binary reads the Mongoose pointer
/// from the released work after parent teardown; the published walker pointers
/// are left unchanged. Models follow `taskKill`'s delayed or immediate release.
static void _actor260400ExitScriptedWalker(Task* task)
{
    _Actor260400Work* walkerWork = task->work;

    enemyDestroy(task->spawnArg2.pointer, task);
    // Retain the binary's post-free read and teardown order.
    taskKill(walkerWork->mongoose);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Attaches a drawable Mongoose model root to a borrowed walker coordinate.
///
/// The model owns its root; the walker coordinate must remain live while the
/// attachment uses it. Translation and rotation of the root are retained.
static __inline__ void _actor260400AttachMongooseModel(TmdObject* model, GfxCoord* root, GfxCoord* walkerPart)
{
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    model->flags       = 0;
    root->parent       = walkerPart;
}

/// Parents the Mongoose to the scripted walker and updates its room lighting.
///
/// Requires live TMD models on this task and the published walker task.
/// `spawnArg1.value` must index 0..19 of the walker's coordinates; the spawn
/// uses part 8. State 0 attaches the root, clears draw flags and enters state 1.
/// State 1 samples all three lights at the walker's composed world translation,
/// 800 world units above it in negative Y. The parent coordinate is borrowed
/// until teardown; transforms must be composed and lighting scratch/GTE state
/// available. Other states do nothing.
static void _actor260400MongooseTask(Task* task)
{
    enum {
        ACTOR_260400_MONGOOSE_ATTACH       = 0,
        ACTOR_260400_MONGOOSE_LIGHT        = 1,
        ACTOR_260400_MONGOOSE_LIGHT_HEIGHT = 800,
        ACTOR_260400_MONGOOSE_LIGHT_COUNT  = 3,
    };
    TmdObject* mongooseModel = task->extra.tmd;
    GfxCoord*  mongooseRoot  = mongooseModel->coords;
    GfxCoord*  walkerCoords  = D_actor_260400_80154C74->extra.tmd->coords;
    GfxCoord*  walkerPart    = walkerCoords + task->spawnArg1.value;
    VECTOR     lightSample;

    switch (task->state) {
        case ACTOR_260400_MONGOOSE_ATTACH:
            _actor260400AttachMongooseModel(mongooseModel, mongooseRoot, walkerPart);
            task->state++;
            break;
        case ACTOR_260400_MONGOOSE_LIGHT:
            lightSample.vx = walkerCoords->workm.t[0];
            lightSample.vy = walkerCoords->workm.t[1] - ACTOR_260400_MONGOOSE_LIGHT_HEIGHT;
            lightSample.vz = walkerCoords->workm.t[2];
            worldCoordSetModelLighting(mongooseModel, &lightSample, 0, ACTOR_260400_MONGOOSE_LIGHT_COUNT);
            break;
    }
}

#include "../../shared/scripted_walk_tick_anim.inc.c"

#include "../../shared/scripted_walk_reset_anim.inc.c"

#include "../../shared/scripted_walk_blend_anim.inc.c"

/// Reseeds the published scripted walker from a borrowed animation request.
///
/// Requires a live walker and initialized rig. Loaded clip keys are 1..12.
/// The signed check rejects IDs >= 16 with -1, but accepts negative IDs and NULL
/// entries 0/13/14/15; acceptance alone does not establish a playable clip.
/// An accepted ID narrows to `s16`. Nonzero blend latches the low signed halfword
/// of `blendFrames`, in whole normal-rate frames (0..2047 keeps playback time
/// nonnegative); zero blend restarts and leaves that latch intact. Reseeding
/// runs immediately. Receiver, message ID, second payload and the other request
/// words are ignored. Returns 0 after acceptance and retains no request pointer.
static s32 _actor260400PlayScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum { ACTOR_260400_ANIMATION_TABLE_CAPACITY = 16 };

    if (request->animationId < ACTOR_260400_ANIMATION_TABLE_CAPACITY) {
        _gScriptedWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gScriptedWalkBlendFrames    = request->blendFrames;
        } else {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkWork->st.field_6 = 0;
        // Apply the restart now; ordinary movement waits for a later update.
        _scriptedWalkUpdate(D_actor_260400_80154C74);
        return 0;
    }
    return -1;
}

/// Replaces the scripted walker and Mongoose models' draw flags.
///
/// Requires the live published walker, work block and successfully spawned
/// Mongoose model. Bit 0 permits active drawing; bit 1 suppresses automatic
/// primitive-buffer allocation. All other model flags are cleared and other
/// request bits ignored. The Mongoose stays hidden with automatic buffering
/// suppressed until its command enables it. No buffers are allocated or freed.
/// Receiver, message ID and second payload are ignored. Returns 0.
static s32 _actor260400SetScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument)
{
    enum {
        ACTOR_260400_WALKER_DRAW_SHOW             = 1 << 0,
        ACTOR_260400_WALKER_DRAW_SKIP_AUTO_BUFFER = 1 << 1,
    };
    TmdObject* walkerModel;
    TmdObject* mongooseModel;

    walkerModel   = D_actor_260400_80154C74->extra.tmd;
    mongooseModel = _gScriptedWalkWork->mongoose->extra.tmd;

    if (drawFlags & ACTOR_260400_WALKER_DRAW_SHOW) {
        walkerModel->flags   = 0;
        mongooseModel->flags = 0;
    } else {
        walkerModel->flags   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        mongooseModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_260400_WALKER_DRAW_SKIP_AUTO_BUFFER) {
        walkerModel->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        mongooseModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    if (_gScriptedWalkWork->mongooseShown == 0) {
        mongooseModel->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    }
    return 0;
}

#include "../../shared/scripted_walk_place.inc.c"

/// Message 0x7DB: the payload's halfword at 0x2 selects the action. Case 0
/// starts a turn of 0x14 steps; case 1 enables and shows the revolver's model,
/// but only while `func_800B7420(0x88)` returns 0; case 2 disables it and
/// hides the model again (flags 0x84).
s32 func_actor_260400_8014AAA4(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    TmdObject* obj;
    s32        mode;

    obj  = _gScriptedWalkWork->mongoose->extra.tmd;
    mode = msg->command;

    switch (mode) {
        case 0:
            _gScriptedWalkWork->turnFrames = 0x14;
            break;
        case 1:
            if (func_800B7420(0x88) == 0) {
                _gScriptedWalkWork->mongooseShown = mode;
                obj->flags                        = 0;
            }
            break;
        case 2:
            _gScriptedWalkWork->mongooseShown = 0;
            obj->flags                        = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
    }
    return 0;
}

#include "../../shared/scripted_walk_to.inc.c"
