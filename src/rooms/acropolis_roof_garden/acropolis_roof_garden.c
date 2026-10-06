#include "rooms/acropolis_roof_garden.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/falling_leaves.h"
#include "../../shared/actor_contacts.h"
#include "../../shared/glow_draw.h"

/// Messages the room task answers, terminated by id `TASK_MESSAGE_TABLE_END`.
extern TaskMessageEntry D_acropolis_roof_garden_80183BDC[];
extern Task*            D_acropolis_roof_garden_80183C0C;
extern TaskDesc         D_acropolis_roof_garden_80183C10[];
extern EvsCommand       D_acropolis_roof_garden_80183D74[];
extern EvsCommand       D_acropolis_roof_garden_80184194[];
extern s32              D_acropolis_roof_garden_8018432C;
extern EvsCommand       D_acropolis_roof_garden_80184B08[];

/// Ten spawn offsets for the roof garden's ambient effects, indexed 0..9 by
/// the effect task's first-frame burst.
extern SVECTOR D_acropolis_roof_garden_80184BF8[10];

/// Per-variant mask of camera views the ambient sprite is visible from,
/// indexed by the low nibble of `Task::spawnArg1`.
extern u16 D_acropolis_roof_garden_80184C48[];

extern s16 D_acropolis_roof_garden_80184C5C[];

/// Volume the ambience task last handed the sound driver.
extern s32 D_acropolis_roof_garden_80186E94;

/// Whole-unit X/Y/Z displacement left by `ActorContact_PushContact`.
extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

/// Initializes a flare quad's command and colours, with vertex 2 lit and a black rim.
///
/// `quad` must be a stable writable POLY_G4 pointer without side effects; it is
/// evaluated repeatedly. `red` and `green` each evaluate once, after command and
/// rim writes, and narrow to bytes. No identifiers are captured or retained.
#define ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, red, green) \
    ((void)(setPolyG4((quad)),                                  \
            setRGB0((quad), 0, 0, 0),                           \
            setRGB1((quad), 0, 0, 0),                           \
            setRGB2((quad), (red), (green), 0),                 \
            setRGB3((quad), 0, 0, 0)))

static void func_acropolis_roof_garden_8017DB74(Task* arg0);
static void func_acropolis_roof_garden_8017DBEC(Task* task);

/// State handlers of the room task: set-up, the per-frame tick and `taskKill`.
static const TaskFuncTable3 D_acropolis_roof_garden_8017D5C4 = {
    { func_acropolis_roof_garden_8017DB74, func_acropolis_roof_garden_8017DBEC, taskKill },
};

extern WorldCollisionGrid     D_acropolis_roof_garden_801854A4[1];
extern WorldCollisionOccluder D_acropolis_roof_garden_80186D14[2];
extern WorldCollisionTrigger  D_acropolis_roof_garden_801854C8[6];
extern WorldCollisionTrigger  D_acropolis_roof_garden_80185690[7];
extern WorldCoordRoomLights   D_acropolis_roof_garden_80186BDC[1];

extern AnimationPlayRequest     D_acropolis_roof_garden_80184ACC;
extern AnimationPlayRequest     D_acropolis_roof_garden_80184AE0;
extern AnimationBankCopyRequest D_acropolis_roof_garden_80184AB0;
static void                     _acropolisRoofGardenReleaseMaggotCaterpillarEntrance(void);

extern AnimationPlayRequest     D_acropolis_roof_garden_80183C44;
extern AnimationPlayRequest     D_acropolis_roof_garden_80183CCC;
extern AnimationPlayRequest     D_acropolis_roof_garden_80183CE0;
extern AnimationPlayRequest     D_acropolis_roof_garden_80183CF4;
extern AnimationPlayRequest     D_acropolis_roof_garden_80183D08;
extern AnimationPlayRequest     D_acropolis_roof_garden_80183D1C;
extern AnimationPlayRequest     D_acropolis_roof_garden_80183D30;
extern AnimationPlayRequest     D_acropolis_roof_garden_80183D44;
extern AnimationPlayRequest     D_acropolis_roof_garden_80183D58;
extern AnimationBankCopyRequest D_acropolis_roof_garden_80183CC4;
extern ActorTransform           D_acropolis_roof_garden_80183C58;
extern ActorTransform           D_acropolis_roof_garden_80183C70;
extern ActorTransform           D_acropolis_roof_garden_80183CA0;
void                            func_acropolis_roof_garden_8017DAD4(s32);

static AnimationSet _gAcropolisRoofGardenAnimation04164;
static AnimationSet _gAcropolisRoofGardenAnimation060FC;
static AnimationSet _gAcropolisRoofGardenAnimation065F4;

s32        func_acropolis_roof_garden_8017D71C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32 _acropolisRoofGardenRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
s32        func_acropolis_roof_garden_8017D7A0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32        func_acropolis_roof_garden_8017D868(Task*, s32, s32, s32);
s32        func_acropolis_roof_garden_8017D8AC(Task*, s32, s32, s32);
void       func_acropolis_roof_garden_8017D5D4(Task*);
void       func_acropolis_roof_garden_8017D970(Task*);
void       func_acropolis_roof_garden_8017DA48(Task*);

static AnimationPackedPose _gAcropolisRoofGardenAnimation04164Bank1[38] = {
#include "assets/acropolis_roof_garden_animation_04164_bank1.inc"
};

static AnimationPackedRotation _gAcropolisRoofGardenAnimation04164Bank4[571] = {
#include "assets/acropolis_roof_garden_animation_04164_bank4.inc"
};

static AnimationRecord _gAcropolisRoofGardenAnimation04164Records[666] = {
#include "assets/acropolis_roof_garden_animation_04164_records.inc"
};

static u16 _gAcropolisRoofGardenAnimation04164Indices[20] = {
#include "assets/acropolis_roof_garden_animation_04164_indices.inc"
};

static AnimationSet _gAcropolisRoofGardenAnimation04164 = {
    _gAcropolisRoofGardenAnimation04164Records,
    _gAcropolisRoofGardenAnimation04164Indices,
    { NULL, _gAcropolisRoofGardenAnimation04164Bank1, NULL, NULL, _gAcropolisRoofGardenAnimation04164Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisRoofGardenAnimation060FCBank1[40] = {
#include "assets/acropolis_roof_garden_animation_060FC_bank1.inc"
};

static AnimationPackedRotation _gAcropolisRoofGardenAnimation060FCBank4[860] = {
#include "assets/acropolis_roof_garden_animation_060FC_bank4.inc"
};

static AnimationRecord _gAcropolisRoofGardenAnimation060FCRecords[1022] = {
#include "assets/acropolis_roof_garden_animation_060FC_records.inc"
};

static u16 _gAcropolisRoofGardenAnimation060FCIndices[20] = {
#include "assets/acropolis_roof_garden_animation_060FC_indices.inc"
};

static AnimationSet _gAcropolisRoofGardenAnimation060FC = {
    _gAcropolisRoofGardenAnimation060FCRecords,
    _gAcropolisRoofGardenAnimation060FCIndices,
    { NULL, _gAcropolisRoofGardenAnimation060FCBank1, NULL, NULL, _gAcropolisRoofGardenAnimation060FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisRoofGardenAnimation065F4Bank1[6] = {
#include "assets/acropolis_roof_garden_animation_065F4_bank1.inc"
};

static AnimationPackedRotation _gAcropolisRoofGardenAnimation065F4Bank4[87] = {
#include "assets/acropolis_roof_garden_animation_065F4_bank4.inc"
};

static AnimationRecord _gAcropolisRoofGardenAnimation065F4Records[193] = {
#include "assets/acropolis_roof_garden_animation_065F4_records.inc"
};

static u16 _gAcropolisRoofGardenAnimation065F4Indices[20] = {
#include "assets/acropolis_roof_garden_animation_065F4_indices.inc"
};

static AnimationSet _gAcropolisRoofGardenAnimation065F4 = {
    _gAcropolisRoofGardenAnimation065F4Records,
    _gAcropolisRoofGardenAnimation065F4Indices,
    { NULL, _gAcropolisRoofGardenAnimation065F4Bank1, NULL, NULL, _gAcropolisRoofGardenAnimation065F4Bank4, NULL, NULL, NULL },
};

enum { ACROPOLIS_ROOF_GARDEN_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskMessageEntry D_acropolis_roof_garden_80183BDC[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_roof_garden_8017D71C },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_acropolis_roof_garden_8017D7A0 },
    { ROOM_MESSAGE_COMMAND, func_acropolis_roof_garden_8017D8AC },
    { ACROPOLIS_ROOF_GARDEN_MESSAGE_USE_KEY_ITEM, _acropolisRoofGardenRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, func_acropolis_roof_garden_8017D868 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

Task* D_acropolis_roof_garden_80183C0C = NULL;

TaskDesc D_acropolis_roof_garden_80183C10[4] = {
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_roof_garden_8017D5D4, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_roof_garden_8017D970, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_roof_garden_8017DA48, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

ActorCommand D_acropolis_roof_garden_80183C40 = { { .loc = { 1, 13 } }, 0 };

AnimationPlayRequest D_acropolis_roof_garden_80183C44 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_acropolis_roof_garden_80183C58 = { 0 };

ActorTransform D_acropolis_roof_garden_80183C70 = { { 0, 128, 0, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_roof_garden_80183C88 = { { 0, 128, 0, 0 }, { 0, 407, 0, 0 } };

ActorTransform D_acropolis_roof_garden_80183CA0 = { { -7508, 0, -8572, 0 }, { 0, 407, 0, 0 } };

AnimationSet* D_acropolis_roof_garden_80183CB8[3] = {
    &_gAcropolisRoofGardenAnimation04164,
    &_gAcropolisRoofGardenAnimation060FC,
    &_gAcropolisRoofGardenAnimation065F4,
};

AnimationBankCopyRequest D_acropolis_roof_garden_80183CC4 = { { .sets = D_acropolis_roof_garden_80183CB8 }, ARRAY_SIZE(D_acropolis_roof_garden_80183CB8) };

AnimationPlayRequest D_acropolis_roof_garden_80183CCC = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_roof_garden_80183CE0 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_roof_garden_80183CF4 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_roof_garden_80183D08 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_roof_garden_80183D1C = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_roof_garden_80183D30 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_roof_garden_80183D44 = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_roof_garden_80183D58 = { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsSceneKey D_acropolis_roof_garden_80183D6C = { 1, 8, 11 };

EvsCommand D_acropolis_roof_garden_80183D74[44] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_acropolis_roof_garden_80183CC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_roof_garden_80183C70 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_roof_garden_80183CCC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_roof_garden_8017DAD4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x31080005 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 154 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_roof_garden_80183CF4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 61 }, { .value = 64 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_roof_garden_80183C58 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_roof_garden_80183D08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_roof_garden_80183D1C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x31080004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_roof_garden_80183D30 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_roof_garden_80183D44 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 79 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_roof_garden_80183D58 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x31080001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x31080002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x31080003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_roof_garden_80183CE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_roof_garden_8017DAD4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 115 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_roof_garden_80183C44 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_roof_garden_80183CA0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_roof_garden_80184194[17] = {
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x30000000 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_roof_garden_80183CA0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_roof_garden_80183C44 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_roof_garden_80183D08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_roof_garden_8017DAD4 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s32 D_acropolis_roof_garden_8018432C = 0;

static AnimationPackedPose _gAcropolisRoofGardenAnimation0704CBank1[6] = {
#include "assets/acropolis_roof_garden_animation_0704C_bank1.inc"
};

static AnimationPackedRotation _gAcropolisRoofGardenAnimation0704CBank4[46] = {
#include "assets/acropolis_roof_garden_animation_0704C_bank4.inc"
};

static AnimationRecord _gAcropolisRoofGardenAnimation0704CRecords[109] = {
#include "assets/acropolis_roof_garden_animation_0704C_records.inc"
};

static u16 _gAcropolisRoofGardenAnimation0704CIndices[20] = {
#include "assets/acropolis_roof_garden_animation_0704C_indices.inc"
};

static AnimationSet _gAcropolisRoofGardenAnimation0704C = {
    _gAcropolisRoofGardenAnimation0704CRecords,
    _gAcropolisRoofGardenAnimation0704CIndices,
    { NULL, _gAcropolisRoofGardenAnimation0704CBank1, NULL, NULL, _gAcropolisRoofGardenAnimation0704CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisRoofGardenAnimation074BCBank1[10] = {
#include "assets/acropolis_roof_garden_animation_074BC_bank1.inc"
};

static AnimationPackedRotation _gAcropolisRoofGardenAnimation074BCBank4[95] = {
#include "assets/acropolis_roof_garden_animation_074BC_bank4.inc"
};

static AnimationRecord _gAcropolisRoofGardenAnimation074BCRecords[139] = {
#include "assets/acropolis_roof_garden_animation_074BC_records.inc"
};

static u16 _gAcropolisRoofGardenAnimation074BCIndices[20] = {
#include "assets/acropolis_roof_garden_animation_074BC_indices.inc"
};

static AnimationSet _gAcropolisRoofGardenAnimation074BC = {
    _gAcropolisRoofGardenAnimation074BCRecords,
    _gAcropolisRoofGardenAnimation074BCIndices,
    { NULL, _gAcropolisRoofGardenAnimation074BCBank1, NULL, NULL, _gAcropolisRoofGardenAnimation074BCBank4, NULL, NULL, NULL },
};

AnimationSet* D_acropolis_roof_garden_80184AA4[3] = {
    NULL,
    &_gAcropolisRoofGardenAnimation0704C,
    &_gAcropolisRoofGardenAnimation074BC,
};

AnimationBankCopyRequest D_acropolis_roof_garden_80184AB0 = { { .sets = D_acropolis_roof_garden_80184AA4 }, ARRAY_SIZE(D_acropolis_roof_garden_80184AA4) };

AnimationPlayRequest D_acropolis_roof_garden_80184AB8 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_roof_garden_80184ACC = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_roof_garden_80184AE0 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_acropolis_roof_garden_80184AF4 = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_acropolis_roof_garden_80184B08[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_acropolis_roof_garden_80184AB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_roof_garden_80184ACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisRoofGardenReleaseMaggotCaterpillarEntrance }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_roof_garden_80184AE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

SVECTOR D_acropolis_roof_garden_80184BF8[10] = {
    { -6750, -1970, -0x2896, 0 },
    { -8220, -1970, -0x2896, 0 },
    { -2680, -6600, -1240, 0 },
    { -5840, -548, -9840, 0 },
    { -8740, -548, -9840, 0 },
    { -3430, -548, -7300, 0 },
    { -7620, -548, -7200, 0 },
    { -6300, -548, -6590, 0 },
    { -2890, -548, -2250, 0 },
    { -6470, -548, -2250, 0 },
};

u16 D_acropolis_roof_garden_80184C48[10] = {
    2,
    2,
    4,
    2,
    2,
    4,
    4,
    12,
    16,
    24,
};

s16 D_acropolis_roof_garden_80184C5C[24] = {
    -4096,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    3784,
    4096,
    3784,
    2896,
    1567,
    0,
    -1567,
    -2896,
    -3784,
    -4096,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    591,
};

WorldCollisionRoomResources D_acropolis_roof_garden_80184C8C[1] = {
    { D_acropolis_roof_garden_801854A4, D_acropolis_roof_garden_801854C8, D_acropolis_roof_garden_80185690, D_acropolis_roof_garden_80186D14 },
};

u8* D_acropolis_roof_garden_80184C9C[1] = {
    gViewIdentityMap,
};

ViewCount D_acropolis_roof_garden_80184CA0[1] = { 7 };

WorldCoordRoomLighting D_acropolis_roof_garden_80184CA4[1] = {
    { D_acropolis_roof_garden_80186BDC, NULL },
};

DirectionWarpEntry D_acropolis_roof_garden_80184CAC[2] = {
    { { { .word = 0 }, -7458, 0, -9712 }, { 0, 0, 0, 0 }, { { .word = 0 }, -7458, 0, -9712 }, { 0, 0, 0, 0 }, 0x510D0002, 0x510D0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, -7458, 0, -9712 }, { 0, 0, 0, 0 }, { { .word = 0 }, -7458, 0, -9712 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gAcropolisRoofGardenCollision07EE4Normals[31] = {
#include "assets/acropolis_roof_garden_collision_07EE4_normals.inc"
};

static SVECTOR _gAcropolisRoofGardenCollision07EE4Verts[92] = {
#include "assets/acropolis_roof_garden_collision_07EE4_verts.inc"
};

static WorldCollisionGridFace _gAcropolisRoofGardenCollision07EE4Faces[53] = {
#include "assets/acropolis_roof_garden_collision_07EE4_faces.inc"
};

static s16 _gAcropolisRoofGardenCollision07EE4Cells[136] = {
#include "assets/acropolis_roof_garden_collision_07EE4_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisRoofGardenCollision07EE4Cells[i])
static s16* _gAcropolisRoofGardenCollision07EE4Table[9] = {
#include "assets/acropolis_roof_garden_collision_07EE4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_roof_garden_801854A4[1] = {
    { NULL, _gAcropolisRoofGardenCollision07EE4Normals, _gAcropolisRoofGardenCollision07EE4Verts, _gAcropolisRoofGardenCollision07EE4Faces, _gAcropolisRoofGardenCollision07EE4Table, 0x28DD, 0x299C, 3, 3, 4000, 53 },
};

WorldCollisionTrigger D_acropolis_roof_garden_801854C8[6] = {
    { NULL, NULL, NULL, { -7264, -1440, -8768, 0 }, { { -1017, 5600, 185, 0 }, { 991, 5600, -215, 0 }, { -1017, -5600, 185, 0 }, { 991, -5600, -215, 0 } }, { 801, 0, 4025, 0 }, { 0, 0, 0, 0 }, 5678, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4544, -1536, -6880, 0 }, { { -1024, 5600, 0, 0 }, { 1024, 5600, 0, 0 }, { -1024, -5600, 0, 0 }, { 1024, -5600, 0, 0 } }, { 0, 0, 4105, 0 }, { 0, 0, 0, 0 }, 5678, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7296, -1696, -8448, 0 }, { { 982, 5600, -261, 0 }, { -1004, 5600, 237, 0 }, { 982, -5600, -261, 0 }, { -1004, -5600, 237, 0 } }, { -999, 0, -3982, 0 }, { 0, 0, 0, 0 }, 5678, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4480, -1856, -6560, 0 }, { { 1024, 5600, 0, 0 }, { -1024, 5600, 0, 0 }, { 1024, -5600, 0, 0 }, { -1024, -5600, 0, 0 } }, { 0, 0, -4106, 0 }, { 0, 0, 0, 0 }, 5678, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4736, -1568, -4000, 0 }, { { 2496, 5600, 0, 0 }, { -2496, 5600, 0, 0 }, { 2496, -5600, 0, 0 }, { -2496, -5600, 0, 0 } }, { 0, 0, -4103, 0 }, { 0, 0, 0, 0 }, 6122, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4608, -1824, -4640, 0 }, { { -2496, 5600, 0, 0 }, { 2496, 5600, 0, 0 }, { -2496, -5600, 0, 0 }, { 2496, -5600, 0, 0 } }, { 0, 0, 4102, 0 }, { 0, 0, 0, 0 }, 6122, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_roof_garden_80185690[7] = {
    { NULL, NULL, NULL, { -7465, -32, -9928, 0 }, { { -752, 0, -416, 0 }, { 752, 0, -416, 0 }, { -752, 0, 416, 0 }, { 752, 0, 416, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, 4096, 0 }, 858, WORLD_COLLISION_TRIGGER_ACTION_WARP, 12, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4608, -128, -4816, 0 }, { { -1040, 0, -608, 0 }, { 1040, 0, -608, 0 }, { -1040, 0, 608, 0 }, { 1040, 0, 608, 0 } }, { 0, 4116, 0, 0 }, { -201, 0, -4091, 0 }, 1200, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4640, -96, -3776, 0 }, { { -752, 0, -368, 0 }, { 752, 0, -368, 0 }, { -752, 0, 368, 0 }, { 752, 0, 368, 0 } }, { 0, 4100, 0, 0 }, { -1, 0, 4095, 0 }, 836, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4400, -96, -2080, 0 }, { { -640, 0, -560, 0 }, { 640, 0, -560, 0 }, { -640, 0, 560, 0 }, { 640, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 849, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7697, -160, -8384, 0 }, { { -736, 0, -1136, 0 }, { 1280, 0, -1136, 0 }, { -1279, 0, 1136, 0 }, { 737, 0, 1136, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 1707, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 0, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -4640, -64, -6544, 0 }, { { -1296, 0, -208, 0 }, { 1296, 0, -208, 0 }, { -1296, 0, 208, 0 }, { 1296, 0, 208, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 1311, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -4640, -64, -7104, 0 }, { { -1296, 0, -208, 0 }, { 1296, 0, -208, 0 }, { -1296, 0, 208, 0 }, { 1296, 0, 208, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 1311, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_acropolis_roof_garden_801858A4[3] = {
    { 110, 108, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor110800ViewFigureTasks },
    { 55, 55, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205500_801528DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_roof_garden_801858C8[3] = {
    { 55, 55, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_105500_8013A8DC },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &Actor00700_D075A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_roof_garden_801858EC[3] = {
    { 110, 108, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor110800ViewFigureTasks },
    { 26, 26, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_202600_801528D4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_roof_garden_80185910[3] = {
    { 26, 26, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &Actor00700_D075A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_roof_garden_80185934[12] = {
    { NULL, NULL },
    { D_map_akropolis_8017BABC, D_acropolis_roof_garden_801858A4 },
    { D_map_akropolis_8017BB1C, D_acropolis_roof_garden_801858C8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_akropolis_8017BB9C, D_acropolis_roof_garden_801858EC },
    { D_map_akropolis_8017BBFC, D_acropolis_roof_garden_80185910 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

SpriteBatch D_acropolis_roof_garden_80185994[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_roof_garden_801859A4[26] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 40, 729, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 40, 765, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, 40, 701, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -152, 40, 698, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, 40, 661, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -120, 40, 653, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 64, 658, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -96, 40, 650, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, 56, 645, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 56, 652, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 40, 686, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -24, 40, 751, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, 40, 839, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 40, 883, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, 40, 950, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 48, 1072, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, 40, 1011, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, 40, 817, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 104, 40, 667, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 40, 551, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 120, 40, 473, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 104, 453, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 40, 421, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 48, 388, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 64, 362, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 96, 352, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_roof_garden_80185BAC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_roof_garden_80185BC4[57] = {
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 32, 56, 563, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 40, 48, 551, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 40, 80, 563, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 48, 40, 518, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 56, 48, 499, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 48, 479, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 72, 48, 456, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 80, 424, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 104, 471, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 80, 417, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 48, 418, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 48, 403, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 96, 56, 412, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 104, 56, 401, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 112, 64, 392, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 120, 64, 377, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 128, 64, 346, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 136, 64, 326, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 136, 88, 349, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 104, 480, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 72, 561, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 56, 558, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 40, 599, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 72, 608, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 64 } }, 64, 40, 557, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 136, 16, 560, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 48, -120, 855, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 40 } }, 32, -96, 920, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 40 } }, 32, -56, 1272, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, 88, -120, 1111, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 32 } }, 88, -40, 1225, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 112, -8, 1250, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 72, -16, 1275, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 88, -80, 1111, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 112, -72, 1109, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 136, -80, 1112, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 56 } }, -16, 16, 1104, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 144 } }, -112, -64, 1064, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 120 } }, -56, -48, 1149, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, 16, 1285, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 112, 40, 1204, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -8, -8, 1563, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 8, -32, 1580, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 32, 0, 1639, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, 48, 600, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -144, 40, 650, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -128, 48, 700, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -112, 40, 750, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 40, 800, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, 40, 850, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 32, 900, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -48, 32, 950, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -24, 32, 1050, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 0, 32, 1100, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, 32, 1100, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 32, 40, 1100, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 48, 1100, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_roof_garden_80186038[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 7, 0, 0, { 4, 0 } },
    { 26, 10, 0, 0, { 1, 0 } },
    { 36, 3, 0, 0, { 5, 0 } },
    { 39, 2, 0, 0, { 0, 0 } },
    { 41, 3, 0, 0, { 6, 0 } },
    { 44, 13, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_roof_garden_80186080[21] = {
    { 143, 0x3FC0, { .fields = { 32, 104 } }, -160, -96, 462, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, -56, 462, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 104 } }, -160, 16, 472, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, -40, 8, 525, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 88, 500, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 88 } }, -120, 32, 522, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 88 } }, -80, 32, 522, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 64, 664, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 96 } }, 96, 0, 624, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 8, 663, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 136, 48, 599, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -24, 966, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -40, -48, 1008, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -48, -48, 1105, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 152 } }, -32, -96, 974, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, 8, -48, 963, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, -16, 1550, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -104, -8, 1500, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, -8, 1450, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, -8, 1400, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 24, 1350, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_roof_garden_80186224[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 3, 0 } },
    { 7, 4, 0, 0, { 0, 0 } },
    { 11, 5, 0, 0, { 2, 0 } },
    { 16, 5, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_roof_garden_80186254[47] = {
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -48, -32, 950, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, -40, 1019, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 48 } }, -64, -80, 875, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -8, -80, 909, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 0, -104, 923, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, -56, -120, 667, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 72 } }, -160, 48, 153, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, 88, 296, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -56, 96, 294, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 88, 298, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 24, 96, 298, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 56, 88, 302, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, 104, 299, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -120, 1294, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -120, 1265, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -120, 1302, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -120, 1240, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -120, 1256, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -120, 1184, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -120, 1118, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -120, 1109, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -112, 1316, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -112, 1284, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -112, 1281, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -112, 1275, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -112, 1229, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -112, 1209, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -112, 1147, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -112, 1048, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -104, 1307, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -104, 1300, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -104, 1276, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -104, 1258, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -104, 1258, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -104, 1189, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -104, 1085, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -96, 1287, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -96, 1268, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -96, 1240, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -96, 1196, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -96, 1081, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -96, 1069, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -88, 1267, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -88, 1199, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -88, 1174, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -88, 1070, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -88, 1054, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_roof_garden_80186600[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 7, 0, 0, { 2, 0 } },
    { 13, 34, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_roof_garden_80186628[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_roof_garden_80186638[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_roof_garden_80186648[7] = {
    { { .empty = D_acropolis_roof_garden_80185994 }, D_acropolis_roof_garden_80185994, NULL },
    { { .elements = D_acropolis_roof_garden_801859A4 }, D_acropolis_roof_garden_80185BAC, NULL },
    { { .elements = D_acropolis_roof_garden_80185BC4 }, D_acropolis_roof_garden_80186038, NULL },
    { { .elements = D_acropolis_roof_garden_80186080 }, D_acropolis_roof_garden_80186224, NULL },
    { { .elements = D_acropolis_roof_garden_80186254 }, D_acropolis_roof_garden_80186600, NULL },
    { { .empty = D_acropolis_roof_garden_80186628 }, D_acropolis_roof_garden_80186628, NULL },
    { { .empty = D_acropolis_roof_garden_80186638 }, D_acropolis_roof_garden_80186638, NULL },
};

/// Authored point lights for model shading throughout the roof garden.
///
/// Positions and inner/outer radii use integer world units; RGB intensities
/// have 12 fractional bits (ONE is full intensity). Every light accepts every view.
/// Gameplay updates the transform caches and attenuation, so these records stay
/// writable and must remain live while the room overlay is in use.
static WorldCoordPointLight _gAcropolisRoofGardenPointLights[] = {
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -5884, -700, -9680 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 3686, 3276 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -6210, -700, -2566 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 3686, 3276 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -4984, -100, -5134 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 2457, 3276, ONE },
        },
        .inner = 100,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -4153, -100, -5134 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 2457, 3276, ONE },
        },
        .inner = 100,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -4153, -1500, -4622 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 2457, 3276, ONE },
        },
        .inner = 100,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -5007, -1500, -4622 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 2457, 3276, ONE },
        },
        .inner = 100,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -4654, -200, -3856 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 2457, 2867 },
        },
        .inner = 100,
        .outer = 2000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -4654, -2000, -2126 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 819, 1638 },
        },
        .inner = 100,
        .outer = 2000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -8188, -1800, -0x2864 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 2867, 3686, ONE },
        },
        .inner = 200,
        .outer = 2000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -6846, -1800, -0x2864 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { 2867, 3686, ONE },
        },
        .inner = 200,
        .outer = 2000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -8931, -700, -9680 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 3686, 3276 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -6248, -700, -6705 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 3686, 3276 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -3472, -700, -7111 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 3686, 3276 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -3202, -700, -2566 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                },
            },
            .color = { ONE, 3686, 3276 },
        },
        .inner = 500,
        .outer = 4000,
    },
};

WorldCoordRoomLights D_acropolis_roof_garden_80186BDC[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisRoofGardenPointLights), _gAcropolisRoofGardenPointLights, 0, NULL },
};

ViewCamera D_acropolis_roof_garden_80186BF4[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 5600, 0x7530, 5960 } }, 589 },
    { { { { -3911, 0, -1216 }, { 204, 4037, -656 }, { 1199, -687, -3855 } }, { 7887, 505, 6331 } }, 230 },
    { { { { 3788, 0, -1557 }, { 186, 4066, 454 }, { 1545, -491, 3761 } }, { 7879, 656, 0x29A1 } }, 230 },
    { { { { 3851, 0, -1394 }, { -208, 4049, -576 }, { 1379, 612, 3807 } }, { 5932, 1317, 8463 } }, 230 },
    { { { { -4010, 0, -830 }, { -785, 1329, 3793 }, { 269, 3874, -1302 } }, { 5201, 4187, 2281 } }, 230 },
    { { { { -2305, 0, -3385 }, { -1984, 3319, 1350 }, { 2743, 2400, -1867 } }, { 5865, 2196, 2875 } }, 230 },
    { { { { -3852, 0, -1389 }, { -827, 3291, 2292 }, { 1116, 2437, -3096 } }, { 4797, 459, 3558 } }, 230 },
    { { { { -3750, 0, 1646 }, { 1161, 2905, 2643 }, { -1168, 2887, -2659 } }, { 3260, 3610, 2030 } }, 230 },
};

WorldCollisionOccluder D_acropolis_roof_garden_80186D14[2] = {
    { NULL, NULL, { -5888, -4064, -8128, 0 }, { { -4256, 0, -2848, 0 }, { 4256, 0, -2848, 0 }, { -4256, 0, 2848, 0 }, { 4256, 0, 2848, 0 } }, { 0, 4105, 0, 0 }, 5120, 1, 0 },
    { NULL, NULL, { -4576, -496, -4400, 0 }, { { -576, 928, 0, 0 }, { 576, 928, 0, 0 }, { -576, -928, 0, 0 }, { 576, -928, 0, 0 } }, { 0, 0, 4111, 0 }, 1086, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_acropolis_roof_garden_80186D8C = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

WorldCollisionSurfaceProperties D_acropolis_roof_garden_80186D98[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_roof_garden_80186DA0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_roof_garden_80186D8C },
};

WorldCollisionSurfaceProperties D_acropolis_roof_garden_80186DA8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_roof_garden_80186D8C },
};

WorldCollisionSurfaceProperties* D_acropolis_roof_garden_80186DB0[8] = {
    D_acropolis_roof_garden_80186D98,
    D_acropolis_roof_garden_80186DA0,
    D_acropolis_roof_garden_80186DA8,
    D_acropolis_roof_garden_80186D98,
    D_acropolis_roof_garden_80186D98,
    D_acropolis_roof_garden_80186D98,
    D_acropolis_roof_garden_80186D98,
    D_acropolis_roof_garden_80186D98,
};

static TmdBone _gAcropolisRoofGardenModel09868Skeleton[1] = {
#include "assets/acropolis_roof_garden_model_09868_skeleton.inc"
};

static u32 _gAcropolisRoofGardenModel09868PartVerts[1] = {
#include "assets/acropolis_roof_garden_model_09868_partVerts.inc"
};

static SVECTOR _gAcropolisRoofGardenModel09868Verts[4] = {
#include "assets/acropolis_roof_garden_model_09868_verts.inc"
};

static SVECTOR _gAcropolisRoofGardenModel09868Normals[2] = {
#include "assets/acropolis_roof_garden_model_09868_normals.inc"
};

static u32 _gAcropolisRoofGardenModel09868Stream[18] = {
#include "assets/acropolis_roof_garden_model_09868_stream.inc"
};

TmdSource gAcropolisRoofGardenModel09868 = {
    0,
    104,
    0,
    1,
    _gAcropolisRoofGardenModel09868PartVerts,
    _gAcropolisRoofGardenModel09868Verts,
    _gAcropolisRoofGardenModel09868Normals,
    _gAcropolisRoofGardenModel09868Skeleton,
    _gAcropolisRoofGardenModel09868Stream,
};

s32 D_acropolis_roof_garden_80186E94 = 0;

SVECTOR ActorContact_ScratchPosition;

/// Keeps the roof garden's ambience (sound id 0x510D0005) in step with the
/// session's weather/time state: state 5 plays it at 0x1E, state 7 at full
/// 0x64 and anything else silences it. `D_acropolis_roof_garden_80186E94`
/// latches the volume currently playing, so the task only talks to the sound
/// driver on a change - starting the loop, fading it out, or ramping it to the
/// new level. The driver wants attenuation rather than volume, hence the
/// `(0x64 - vol) * 127 / 100` conversion.
void func_acropolis_roof_garden_8017D5D4(Task* task)
{
    s32 vol;
    u8  state;
    s32 prev;

    switch (task->state) {
        case 0:
            D_acropolis_roof_garden_80186E94 = 0;
            task->state                      = task->state + 1;
            return;
        case 1:
            break;
        default:
            return;
    }

    state = gGameSession->location.loc.view;
    if (state != 5) {
        vol = 0;
        if (state == 7) {
            vol = 0x64;
        }
    } else {
        vol = 0x1E;
    }

    prev = D_acropolis_roof_garden_80186E94;
    if (vol == prev) {
        return;
    }
    if (prev == 0) {
        sndEvtRequestScriptStart(SOUND_ACROPOLIS_ROOF_GARDEN_AMBIENCE, 0, (s8)(((0x64 - vol) * 127) / 100));
    } else if (vol == 0) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_ROOF_GARDEN_AMBIENCE, 0x1E);
    } else {
        sndEvtRequestScriptMix(SOUND_ACROPOLIS_ROOF_GARDEN_AMBIENCE, 0, (s8)(((0x64 - vol) * 127) / 100));
    }
    D_acropolis_roof_garden_80186E94 = vol;
}

/// Message gate for the roof garden's hotspot: copies the incoming record to
/// the outgoing one, then runs the message's one-shot side effect.
///
/// Message 0xC, when not a "report only" query (`queryOnly == 0`) and its nibble
/// is still clear, advances nibble 7 to 2 and sets collection bit 0x13 to 2.
/// The copy itself is unedited, so the answer is always "allowed".
s32 func_acropolis_roof_garden_8017D71C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->areaId == GAME_AREA_ACROPOLIS_SANCTUARY && in->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH) == 0) {
        gameFlagSetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH, 2);
        Gp_SetCurBit2Flag(0x13, 2);
    }
    return 1;
}

/// Refuses every key-item use request in the roof garden.
///
/// All arguments are ignored. Returning zero selects the inventory's
/// "No use now" notice without consuming the selected item.
static s32 _acropolisRoofGardenRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { ACROPOLIS_ROOF_GARDEN_KEY_ITEM_USE_REFUSED = 0 };

    return ACROPOLIS_ROOF_GARDEN_KEY_ITEM_USE_REFUSED;
}

s32 func_acropolis_roof_garden_8017D7A0(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    switch (in->warp) {
        case 1:
            if (((gGameSession->location.loc.variant == 1) || (gGameSession->location.loc.variant == 7)) && (gameFlagGetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS) == 0)) {
                gameFlagSetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS, 1);
            }
            break;
        case 2:
            if (((gGameSession->location.loc.variant == 1) || (gGameSession->location.loc.variant == 7)) && (gameFlagGetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS) == 1)) {
                func_800E8614(D_acropolis_roof_garden_80184B08, 1);
                gameFlagSetNibble(GAME_FLAG_ROOF_GARDEN_PROGRESS, 2);
            }
            break;
    }
    return 1;
}

s32 func_acropolis_roof_garden_8017D868(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 3:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 3), 0, 0);
            break;
        case 5:
            break;
        case 9:
            break;
    }
    return 0;
}

s32 func_acropolis_roof_garden_8017D8AC(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        if ((Gp_GetCurBit2Flag(0x13) == 0) || (Gp_GetCurBit2Flag(0x13) == 1)) {
            Gp_RunCapCmd1(5);
        } else {
            Gp_StartCapSlot(2, 1, 0);
        }
    }
    if (arg2 == 4) {
        if (gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) < 6) {
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 6);
        }
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
        Gp_RunCapCmd(4, 0);
        func_800E3FAC(0xA2, 7);
    }
    return 0;
}

void func_acropolis_roof_garden_8017D970(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0x14:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 6), 0, 0);
            break;
        case 0x27:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 7), 0, 0);
            break;
        case 0x39:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 8), 0, 0);
            break;
        case 0x63:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 9), 0, 0);
            break;
        case 0x72:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x0A), 0, 0);
            D_acropolis_roof_garden_80183C0C = NULL;
            taskKill(arg0);
            break;
    }
    arg0->state += 1;
}

void func_acropolis_roof_garden_8017DA48(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0x4C:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x0F), 0, 0);
            break;
        case 0x64:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 0x10), 0, 0);
            D_acropolis_roof_garden_80183C0C = NULL;
            taskKill(arg0);
            break;
    }
    arg0->state += 1;
}

void func_acropolis_roof_garden_8017DAD4(s32 arg0)
{
    switch (arg0) {
        case 0:
            D_acropolis_roof_garden_80183C0C = taskSpawnFromTable(D_acropolis_roof_garden_80183C10, 1, 0, 0);
            break;
        case 1:
            D_acropolis_roof_garden_80183C0C = taskSpawnFromTable(D_acropolis_roof_garden_80183C10, 2, 0, 0);
            break;
        case 2:
            if (D_acropolis_roof_garden_80183C0C != NULL) {
                taskKill(D_acropolis_roof_garden_80183C0C);
                D_acropolis_roof_garden_80183C0C = NULL;
            }
            break;
    }
}

static void func_acropolis_roof_garden_8017DB74(Task* arg0)
{
    arg0->msgTable = D_acropolis_roof_garden_80183BDC;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == 6) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 5;
    }
    taskSpawnFromTable(D_acropolis_roof_garden_80183C10, 0, 0, 0);
    arg0->state += 1;
}

static void func_acropolis_roof_garden_8017DBEC(Task* task)
{
    GameLocationKey key;

    if ((gGameSession->location.loc.warp == 2) && (D_acropolis_roof_garden_8018432C == 0)) {
        D_acropolis_roof_garden_8018432C = 1;
        func_800E8634(D_acropolis_roof_garden_80183D74, 0, D_acropolis_roof_garden_80184194);
        gameFlagSetNibble(GAME_FLAG_SANCTUARY_BLOCKER_CLEARED, 1);
        key.stage = GAME_STAGE_ACROPOLIS;
        key.area  = GAME_AREA_ACROPOLIS_SANCTUARY;
        areaSetPlacementVariant(&key, 3, AREA_VARIANT_RESET_ALWAYS);
    }
}

/// Runs the task's current state through a stack copy of the room's
/// three-entry state table.
void func_acropolis_roof_garden_8017DC74(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_roof_garden_8017D5C4;
    sp.funcs[task->state](task);
}

/// Releases the Maggot/Caterpillar scripted entrance for the roof garden encounter.
static void _acropolisRoofGardenReleaseMaggotCaterpillarEntrance(void)
{
    enum { ACROPOLIS_ROOF_GARDEN_ENTRANCE_RELEASED = 1 };

    gSceneCombatState.maggotCaterpillarEntranceReady = ACROPOLIS_ROOF_GARDEN_ENTRANCE_RELEASED;
}

/// Roof-garden ambient effect task. On its first frame it fires one effect per
/// entry of `D_acropolis_roof_garden_80184BF8` - two with a 0x02000000 flavour,
/// one flagged 0x04000102, then seven more - and every frame after that it adds
/// the two view-dependent effects: one while the current view is 5 or 6 (the
/// `0x30 >> view - 1` bit test) and one while it is 7.
void func_acropolis_roof_garden_8017DCDC(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    vec;
    s32         i;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (task->state == 0) {
        for (i = 0; i < 2; i++) {
            Gp_SpawnEff(EFFECT_ACROPOLIS_ROOF_GARDEN_LIGHT_GLOW, coord, i + 0x2000000, &D_acropolis_roof_garden_80184BF8[i]);
        }
        vec = D_acropolis_roof_garden_80184BF8;
        Gp_SpawnEff(EFFECT_ACROPOLIS_ROOF_GARDEN_LIGHT_GLOW, coord, 0x4000102, &vec[2]);
        for (i = 3; i < 10; i++) {
            Gp_SpawnEff(EFFECT_ACROPOLIS_ROOF_GARDEN_LIGHT_GLOW, coord, i + 0x200, &vec[i]);
        }
        task->state = task->state + 1;
    }
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if ((0x30 >> (gGameSession->location.loc.view - 1)) & 1) {
            work->move.vx = -0x12A2;
            work->move.vy = -0xDC;
            work->move.vz = -0xF19;
            Gp_SpawnEff(EFFECT_ACROPOLIS_ROOF_GARDEN_FLARE, coord, 0x60E, &work->move);
        }
        if (gGameSession->location.loc.view == 7) {
            work->move.vx = -0x12A2;
            work->move.vy = -0xDC;
            work->move.vz = -0xF19;
            Gp_SpawnEff(EFFECT_ACROPOLIS_ROOF_GARDEN_FLARE, coord, 0x8000030E, &work->move);
        }
    }
}

/// Positions a light glow quad as a square around its projected screen centre.
///
/// Borrows a writable `quad` and a separate readable `projection`; only
/// `screenPos` and the nonnegative `halfExtent` need initialization, in pixels.
/// Centre plus/minus half-extent must fit signed 32 bits. Each edge narrows to
/// signed 16 bits without clipping, before its paired corner stores. Vertex
/// indices 0..3 are top-left, top-right, bottom-left and bottom-right.
/// Writes only the eight coordinate halfwords; no GPU packets are queued.
static inline void _acropolisRoofGardenSetLightGlowBounds(POLY_FT4* quad, const RoomGlowSpriteScratch* projection)
{
    s16 left;
    s16 right;
    s16 top;
    s16 bottom;

    left     = projection->screenPos.vx - projection->halfExtent;
    quad->x2 = left;
    quad->x0 = left;
    right    = projection->screenPos.vx + projection->halfExtent;
    quad->x3 = right;
    quad->x1 = right;
    top      = projection->screenPos.vy - projection->halfExtent;
    quad->y1 = top;
    quad->y0 = top;
    bottom   = projection->screenPos.vy + projection->halfExtent;
    quad->y3 = bottom;
    quad->y2 = bottom;
}

void acropolisRoofGardenLightGlowTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_LIGHT_INITIALIZE    = 0,
        ACROPOLIS_ROOF_GARDEN_LIGHT_INDEX_MASK    = 0xF,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_SHIFT    = 8,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_MASK     = 3,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_COUNT    = 3,
        ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_SHIFT   = 16,
        ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_MASK    = 0xFFF,
        ACROPOLIS_ROOF_GARDEN_LIGHT_DEFAULT_SCALE = 640,
        ACROPOLIS_ROOF_GARDEN_LIGHT_FLICKER_STEP  = 16,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CLUT_X_STRIDE = 16,
        ACROPOLIS_ROOF_GARDEN_LIGHT_CLUT_Y        = 270,
    };
    EffectWork*            work;
    GfxCoord*              coord;
    RoomGlowSpriteScratch* projection;
    POLY_FT4*              quad;
    s32                    greyLevel;
    s32                    flickerLevel;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        if ((D_acropolis_roof_garden_80184C48[task->spawnArg1.value & ACROPOLIS_ROOF_GARDEN_LIGHT_INDEX_MASK] >> (gGameSession->location.loc.view - 1)) & 1) {
            actorRenderComposeCoord(coord);
            projection = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
            if (task->state == ACROPOLIS_ROOF_GARDEN_LIGHT_INITIALIZE) {
                // Resting grey of each sheet cell; the room only spawns cells 0..2.
                u8 restingLevels[ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_COUNT] = { 0x40, 0x60, 0x10 };

                // Decode once, retaining only the view-mask index in the spawn argument.
                if (task->spawnArg1.value & (ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_MASK << ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_SHIFT)) {
                    work->scale = (task->spawnArg1.value >> ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_SHIFT) & ACROPOLIS_ROOF_GARDEN_LIGHT_SCALE_MASK;
                } else {
                    work->scale = ACROPOLIS_ROOF_GARDEN_LIGHT_DEFAULT_SCALE;
                }
                work->angle           = (task->spawnArg1.value >> ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_SHIFT) & ACROPOLIS_ROOF_GARDEN_LIGHT_CELL_MASK;
                task->spawnArg1.value = task->spawnArg1.value & ACROPOLIS_ROOF_GARDEN_LIGHT_INDEX_MASK;
                work->period          = restingLevels[work->angle];
                task->state++;
            }
            // Rejected projections still consume one frame-arena quad.
            projection->worldPos.vx = coord->workm.t[0];
            projection->worldPos.vy = coord->workm.t[1];
            projection->worldPos.vz = coord->workm.t[2];
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&projection->worldPos);
            gte_rtps();
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            gte_stsxy(&projection->screenPos);
            gte_stszotz(&projection->otz);
            if (projection->otz >= GLOW_MIN_DEPTH) {
                flickerLevel = ((u8)gDisplayState.animFrame & 1) * ACROPOLIS_ROOF_GARDEN_LIGHT_FLICKER_STEP;
                greyLevel    = (u8)work->period + flickerLevel;
                quad->tpage  = GLOW_FLARE_TEXTURE_PAGE;
                setRGB0(quad, greyLevel, greyLevel, greyLevel);
                setSemiTrans(quad, true);
                quad->clut = getClut(work->angle * ACROPOLIS_ROOF_GARDEN_LIGHT_CLUT_X_STRIDE, ACROPOLIS_ROOF_GARDEN_LIGHT_CLUT_Y);
                quad->u0   = work->angle * GLOW_FLARE_CELL_STRIDE;
                quad->v0   = 0;
                quad->u1   = work->angle * GLOW_FLARE_CELL_STRIDE + GLOW_FLARE_CELL_LAST_TEXEL;
                quad->v1   = 0;
                quad->u2   = work->angle * GLOW_FLARE_CELL_STRIDE;
                quad->v2   = GLOW_FLARE_CELL_LAST_TEXEL;
                quad->u3   = work->angle * GLOW_FLARE_CELL_STRIDE + GLOW_FLARE_CELL_LAST_TEXEL;
                quad->v3   = GLOW_FLARE_CELL_LAST_TEXEL;

                projection->halfExtent = (work->scale * GLOW_FLARE_CELL_LAST_TEXEL) / projection->otz;
                _acropolisRoofGardenSetLightGlowBounds(quad, projection);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        quad);
            }
            SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
        }
    }
}

/// Initializes a two-segment Gouraud flare streak fading from its centre to black.
///
/// Borrows one word-aligned writable `LINE_G3`, setting its opaque command,
/// packet length and polyline terminator. Vertex 1 receives `centreRed` and
/// `centreGreen` (0..255); blue and both endpoints are zero. The caller supplies all three
/// vertex coordinates, DMA linkage and semitransparency before drawing.
static inline void _acropolisRoofGardenInitFlareStreak(LINE_G3* streak, u8 centreRed, u8 centreGreen)
{
    setLineG3(streak);
    setRGB0(streak, 0, 0, 0);
    setRGB1(streak, centreRed, centreGreen, 0);
    setRGB2(streak, 0, 0, 0);
}

/// Queues a flare packet and prepends its additive draw mode at the same depth.
///
/// Borrows a word-aligned, initialized untextured `POLY_G4` or `LINE_G3` packet
/// to link, and a separate readable `s32` `depthWord` for this call. The depth
/// is projected SZ3 / 4, before `otDepthShift`; the caller rejects values below
/// `GLOW_MIN_DEPTH`. Keep the depth word and display depth shift stable while
/// linking. Unsigned scaling wraps to tag 0..1023 in the current ordering table.
/// Enables packet semitransparency and consumes `sizeof(DR_TPAGE)` bytes of
/// word-aligned frame-arena space without checking capacity. The table must
/// contain the selected tag; both packets must live until GPU completion.
static inline void _acropolisRoofGardenQueueFlare(void* primitive, const s32* depthWord)
{
    enum { ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TO_BYTE_OFFSET_SHIFT = 2 };

    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                ((((u32)*depthWord << gDisplayState.otDepthShift) >> ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TO_BYTE_OFFSET_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            primitive);
    // Prepending the draw mode after the primitive makes the GPU apply it first.
    gpuSetPrimitiveBlendMode(primitive, GPU_BLEND_ADD, *depthWord);
}

void acropolisRoofGardenFlareTask(Task* task)
{
    enum {
        ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_RATE_MASK      = 0xFF,
        ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT         = 8,
        ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_MASK          = 0xFF,
        ACROPOLIS_ROOF_GARDEN_FLARE_GREEN_SHIFT          = 16,
        ACROPOLIS_ROOF_GARDEN_FLARE_STREAKS              = 0x10000000,
        ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_FALLING        = 0x80,
        ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_LEVEL_MASK     = 0x7F,
        ACROPOLIS_ROOF_GARDEN_FLARE_OUTER_RADIUS_SHIFT   = 10,
        ACROPOLIS_ROOF_GARDEN_FLARE_INNER_RADIUS_SHIFT   = 7,
        ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_RADIUS_SHIFT = 9,
        ACROPOLIS_ROOF_GARDEN_FLARE_CIRCLE_STEPS         = 16,
        ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TO_TAG_SHIFT   = 4,
        ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TAG_MASK       = GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt),
    };
    GfxCoord*             coord;
    EffectWork*           work;
    RoomGlowRadiiScratch* projection;
    POLY_G4*              quad;
    LINE_G3*              streak;
    s32                   partIndex;
    s32                   pulsePhase;
    s32                   pulseOrOptions; // Folded pulse level, then packed spawn options.
    s32                   radiusScale;
    s16                   intensity;
    s16                   green;
    s16                   halfIntensity;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    // Draw one projected frame before retiring the counted work and task.
    actorRenderComposeCoord(coord);
    projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowRadiiScratch);
    projection->worldPos.vx = coord->workm.t[0];
    projection->worldPos.vy = coord->workm.t[1];
    projection->worldPos.vz = coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPos);
    gte_rtps();
    gte_stsxy(&projection->screenPos);
    gte_stszotz(&projection->otz);
    if (projection->otz >= GLOW_MIN_DEPTH) {
        // Fold the phase into a 0..127 triangle wave, then select red or green.
        pulsePhase  = gDisplayState.animFrame;
        pulsePhase *= task->spawnArg1.value & ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_RATE_MASK;
        green       = (task->spawnArg1.value >> ACROPOLIS_ROOF_GARDEN_FLARE_GREEN_SHIFT) & 1;
        if (pulsePhase & ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_FALLING) {
            pulseOrOptions = ~pulsePhase & ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_LEVEL_MASK;
        } else {
            pulseOrOptions = pulsePhase & ACROPOLIS_ROOF_GARDEN_FLARE_PULSE_LEVEL_MASK;
        }
        intensity      = pulseOrOptions * 2;
        pulseOrOptions = task->spawnArg1.value;
        if (pulseOrOptions < 0) {
            // Layer two concentric wedge fans and four half-bright rays.
            radiusScale             = (pulseOrOptions >> ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT) & ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_MASK;
            projection->outerRadius = (radiusScale << ACROPOLIS_ROOF_GARDEN_FLARE_OUTER_RADIUS_SHIFT) / projection->otz;
            projection->innerRadius = (radiusScale << ACROPOLIS_ROOF_GARDEN_FLARE_INNER_RADIUS_SHIFT) / projection->otz;
            for (partIndex = 0; partIndex < ACROPOLIS_ROOF_GARDEN_FLARE_CIRCLE_STEPS; partIndex += 2) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, (intensity * (green ^ 1)) >> 1, (green * intensity) >> 1);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 5]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 1]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 6]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 2]) >> GLOW_TRIG_SHIFT);
                _acropolisRoofGardenQueueFlare(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, intensity * (green ^ 1), green * intensity);
                quad->x0 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 5]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 1]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 6]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 2]) >> (GLOW_TRIG_SHIFT + 1));
                _acropolisRoofGardenQueueFlare(quad, &projection->otz);
            }
            // The first ray retains a Y-sample lookup at index -2.
            halfIntensity = intensity >> 1;
            for (partIndex = 2; partIndex < ACROPOLIS_ROOF_GARDEN_FLARE_CIRCLE_STEPS; partIndex += 8) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, halfIntensity * (green ^ 1), green * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * D_acropolis_roof_garden_80184C5C[partIndex]) >> GLOW_TRIG_SHIFT);
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * D_acropolis_roof_garden_80184C5C[partIndex - 4]) >> GLOW_TRIG_SHIFT);
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 4]) >> (GLOW_TRIG_SHIFT - 1));
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex]) >> (GLOW_TRIG_SHIFT - 1));
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                _acropolisRoofGardenQueueFlare(quad, &projection->otz);

                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, halfIntensity * (green ^ 1), green * halfIntensity);
                quad->x0 = projection->screenPos.vx + ((projection->innerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 4]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y0 = projection->screenPos.vy + ((projection->innerRadius * D_acropolis_roof_garden_80184C5C[partIndex]) >> (GLOW_TRIG_SHIFT + 1));
                quad->x1 = projection->screenPos.vx + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 8]) >> GLOW_TRIG_SHIFT);
                quad->y1 = projection->screenPos.vy + ((projection->outerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 4]) >> GLOW_TRIG_SHIFT);
                quad->x2 = projection->screenPos.vx;
                quad->y2 = projection->screenPos.vy;
                quad->x3 = projection->screenPos.vx + ((projection->innerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 12]) >> (GLOW_TRIG_SHIFT + 1));
                quad->y3 = projection->screenPos.vy + ((projection->innerRadius * D_acropolis_roof_garden_80184C5C[partIndex + 8]) >> (GLOW_TRIG_SHIFT + 1));
                _acropolisRoofGardenQueueFlare(quad, &projection->otz);
            }
        } else {
            projection->outerRadius = (((pulseOrOptions >> ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_SHIFT) & ACROPOLIS_ROOF_GARDEN_FLARE_RADIUS_MASK) << ACROPOLIS_ROOF_GARDEN_FLARE_DIAMOND_RADIUS_SHIFT) / projection->otz;
            for (partIndex = 0; partIndex < 2; partIndex++) {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD(quad, intensity * (green ^ 1), green * intensity);
                quad->x0 = projection->screenPos.vx - projection->outerRadius;
                quad->x1 = quad->x2 = projection->screenPos.vx;
                quad->x3            = projection->screenPos.vx + projection->outerRadius;
                quad->y0 = quad->y2 = quad->y3 = projection->screenPos.vy;
                quad->y1                       = (projection->screenPos.vy - projection->outerRadius) + projection->outerRadius * (partIndex + partIndex);
                addPrim((&gGpuCurrentOt[((u32)projection->otz << gDisplayState.otDepthShift) >> ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TO_TAG_SHIFT & ACROPOLIS_ROOF_GARDEN_FLARE_DEPTH_TAG_MASK]),
                        quad);
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->otz);
            }
            // Optional streaks cross at the diamond centre.
            if (task->spawnArg1.value & ACROPOLIS_ROOF_GARDEN_FLARE_STREAKS) {
                for (partIndex = 0; partIndex < 2; partIndex++) {
                    streak         = gGpuPrimCursor;
                    gGpuPrimCursor = streak + 1;
                    _acropolisRoofGardenInitFlareStreak(streak, intensity * (green ^ 1), green * intensity);
                    streak->x0 = projection->screenPos.vx + projection->outerRadius * (partIndex * 3 - 1);
                    streak->y0 = projection->screenPos.vy - projection->outerRadius * (partIndex + 1);
                    streak->x1 = projection->screenPos.vx;
                    streak->y1 = projection->screenPos.vy;
                    streak->x2 = projection->screenPos.vx - projection->outerRadius * (partIndex * 3 - 1);
                    streak->y2 = projection->screenPos.vy + projection->outerRadius * (partIndex + 1);
                    _acropolisRoofGardenQueueFlare(streak, &projection->otz);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowRadiiScratch);
    effectKillTask(work, task);
}

#undef ACROPOLIS_ROOF_GARDEN_INIT_FLARE_QUAD

#include "../../shared/falling_leaves_task.inc.c"

void acropolisRoofGardenLeafFallTask(Task* task)
{
    _leafFallTask(task);
}

#include "../../shared/falling_leaves_draw.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/actor_contacts_push.inc.c"

/// Item-pickup model task step: the item's mesh is only visible from views 5
/// through 7, and stays hidden once the item's 2-bit flag reads 2 (already
/// taken). The three hidden cases are written as separate tests so the two view
/// comparisons are not folded into one unsigned range check.
void func_acropolis_roof_garden_80180160(Task* task)
{
    Enemy*     enemy;
    TmdObject* tmd;
    s32        flag;
    s32        view;

    enemy = task->spawnArg2.pointer;
    tmd   = task->extra.tmd;
    flag  = Gp_GetCurBit2Flag((u8)enemy->placeKey);
    view  = viewGetMappedIndex();
    if (view >= 8) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else if (view < 5) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else if (flag == 2) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
    }
}
