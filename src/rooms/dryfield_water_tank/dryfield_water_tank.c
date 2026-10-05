#include "types.h"

#include "main/task_types.h"
#include "../../shared/water_tank.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
s32 D_dryfield_water_tank_80188D48;

Task* D_dryfield_water_tank_80188D4C;

#include "rooms/dryfield_water_tank.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "dryfield_water_tank_private.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "overlay.h"
#include "../../shared/screen_fade.h"

/// The clips the Dryfield water tank room adds to the player's animation bank.
///
/// One of the room's two event scripts sends `data.copy` to the player before
/// it plays any of them; the other plays a base-bank clip only. The copy takes
/// 20 words from the start of this storage: the 19 set pointers and the
/// request's own source pointer. Those words occupy extended ids 47-66. The
/// room stores a play request for each of ids 48-65, and the script sends 13 of
/// them. Id 47 stays NULL, id 66 holds the source pointer, and the stored word
/// count sits past the copied span.
typedef union {
    struct {
        AnimationSet*            sets[19]; // Player clips for extended ids 47-65; NULL at id 47, which no request selects
        AnimationBankCopyRequest copy;     // Copies the first 20 words of this storage
    } data;                                // The records by name
    s32 words[21];                         // The same storage as the copy reads it; the last word lies beyond the copied span
} _DryfieldWaterTankAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_DryfieldWaterTankAnimationBankExtensionStorage, 84);

extern _DryfieldWaterTankAnimationBankExtensionStorage D_dryfield_water_tank_80184960;

extern WorldCoordRoomAmbientEntry D_dryfield_water_tank_80188C58[11];

/// Phases of the prop's slide, held in `_DryfieldWaterTankPropSceneWork::slidePhase`.
enum {
    DRYFIELD_WATER_TANK_PROP_SLIDE_MOVING,   // Advancing in Z towards the end placement, raising dust
    DRYFIELD_WATER_TANK_PROP_SLIDE_SETTLING, // Held at the end placement until the settle count runs out
};

/// Frames the prop is held at the end of its slide before it reports arrival.
#define DRYFIELD_WATER_TANK_PROP_SETTLE_FRAMES 61

/// Work block of each task in the water tank room's prop scene, kept at `Task::work`.
///
/// The scene is two tasks spawned from one table: a driver without a model,
/// which carries out the requests the scene's event script posts, and the prop
/// task whose model slides across the room. Each allocates its own zeroed copy
/// of this block and stores the player task in it; beyond that the driver uses
/// only `propTask` and `request`, and the prop only its matrices and the slide
/// fields, so the remaining members of either copy stay zero.
typedef struct {
    MATRIX lightMtx;     // Prop: light matrix its model is drawn with
    MATRIX colorMtx;     // Prop: colour matrix its model is drawn with
    Task*  playerTask;   // Player task at allocation; the driver hides and shows its model
    Task*  propTask;     // Driver: the scene's prop task, which it spawns
    byte   field_48[4];  // Never accessed; role unproven
    u16    slidePhase;   // Prop: phase of the slide (DRYFIELD_WATER_TANK_PROP_SLIDE_*)
    s16    settleFrames; // Prop: frames spent settling, counted up to DRYFIELD_WATER_TANK_PROP_SETTLE_FRAMES
    u16    request;      // Driver: request to carry out this frame, then cleared (DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_*)
    s16    field_52;     // Cleared whenever a request is posted and never read; role unproven
    s16    field_54;     // Cleared whenever the slide is restarted and never read; role unproven
    byte   field_56[2];  // Never accessed; role unproven
} _DryfieldWaterTankPropSceneWork;
STATIC_ASSERT_SIZEOF(_DryfieldWaterTankPropSceneWork, 0x58);

/// Main-executable flag word with no module header yet: while its bit 2 is
/// raised the model task nudges the model 5 units off each position it snaps to.

// Message-table callbacks use the argument views required by this TU.

extern EvsCommand   D_dryfield_water_tank_80184E0C[];
extern EvsCommand   D_dryfield_water_tank_801859DC[];
extern TaskDesc     D_dryfield_water_tank_801868A4[];
extern AreaApplyRec D_dryfield_water_tank_80188D1C[];
extern Task*        D_dryfield_water_tank_80188D44;

static void func_dryfield_water_tank_8017DB48(void);

extern AreaResource D_dryfield_water_tank_80188BCC[2];

extern WorldCollisionGrid    D_dryfield_water_tank_80186EBC[1];
extern WorldCollisionTrigger D_dryfield_water_tank_80187FF8[4];
extern WorldCollisionTrigger D_dryfield_water_tank_80188920[9];
extern WorldCoordRoomLights  D_dryfield_water_tank_80188908[1];

_DryfieldWaterTankAnimationBankExtensionStorage D_dryfield_water_tank_80184960 = { .data = { { NULL, &gDryfieldWaterTankAnimation034BC, &gDryfieldWaterTankAnimation0377C, &gDryfieldWaterTankAnimation03A60, &gDryfieldWaterTankAnimation03CB4, &gDryfieldWaterTankAnimation047BC, &gDryfieldWaterTankAnimation04C98, &gDryfieldWaterTankAnimation04FEC, &gDryfieldWaterTankAnimation051E4, &gDryfieldWaterTankAnimation05704, &gDryfieldWaterTankAnimation059E4, &gDryfieldWaterTankAnimation05DB8, &gDryfieldWaterTankAnimation061A8, &gDryfieldWaterTankAnimation06514, &gDryfieldWaterTankAnimation06840, &gDryfieldWaterTankAnimation04000, &gDryfieldWaterTankAnimation04AA0, &gDryfieldWaterTankAnimation06F48, &gDryfieldWaterTankAnimation06C68 }, { { .words = D_dryfield_water_tank_80184960.words }, 20 } } };

AnimationPlayRequest D_dryfield_water_tank_801849B4 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_801849C8 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_801849DC = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_801849F0 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_80184A04 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_80184A18 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_80184A2C[2] = {
    { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_water_tank_80184A54 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_80184A68 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_80184A7C = { { .index = 1 }, 57, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_80184A90 = { { .index = 1 }, 58, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_80184AA4[2] = {
    { { .index = 1 }, 59, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 60, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_water_tank_80184ACC = { { .index = 1 }, 61, ANIMATION_BLEND_INTERPOLATE, 3, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_80184AE0 = { { .index = 1 }, 62, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_80184AF4 = { { .index = 1 }, 63, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_80184B08 = { { .index = 1 }, 64, ANIMATION_BLEND_INTERPOLATE, 3, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_water_tank_80184B1C = { { .index = 1 }, 65, ANIMATION_BLEND_INTERPOLATE, 3, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_dryfield_water_tank_80184B30 = { { 2530, -0x2EE0, 610, 0 }, { 0, -2047, 0, 0 } };

ActorTransform D_dryfield_water_tank_80184B48 = { { 2388, -0x2EE0, 1137, 0 }, { 0, 960, 0, 0 } };

s32 D_dryfield_water_tank_80184B60[2] = {
    62,
    63,
};

AnimationPlayRequest D_dryfield_water_tank_80184B68[3] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_dryfield_water_tank_80184BA4 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184BB8 = { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184BCC = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184BE0 = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184BF4 = { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184C08 = { { .index = 0 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184C1C[8] = {
    { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 11, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 12, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 13, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 14, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 15, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 16, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_dryfield_water_tank_80184CBC = { { .index = 0 }, 17, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184CD0 = { { .index = 0 }, 18, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184CE4 = { { .index = 0 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184CF8 = { { .index = 0 }, 20, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184D0C = { { .index = 0 }, 21, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184D20 = { { .index = 0 }, 22, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184D34 = { { .index = 0 }, 23, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184D48 = { { .index = 0 }, 24, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184D5C = { { .index = 0 }, 25, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184D70 = { { .index = 0 }, 26, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184D84 = { { .index = 0 }, 27, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_water_tank_80184D98 = { { .index = 0 }, 28, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_dryfield_water_tank_80184DAC = { { 2530, -0x2EE0, -640, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_dryfield_water_tank_80184DC4 = { { 2530, -0x2EE0, -640, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_dryfield_water_tank_80184DDC = { { 2530, -0x2EE0, 0x2710, 0 }, { 0, 0, 0, 0 } };

TaskDesc D_dryfield_water_tank_80184DF4[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_water_tank_8017EC6C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_water_tank_8017ED30, { .value = 0 } },
};

EvsCommand D_dryfield_water_tank_80184E0C[126] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_water_tank_80184960.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_water_tank_80184B30 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_water_tank_80184DAC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_801849C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184CE4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5215000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184CD0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_water_tank_80184DC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184BA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5215000B }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_80184A68 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184C08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5215000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184BCC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_80184A7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184D5C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184D70 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184D98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_80184ACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184D0C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_80184A90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184CF8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5215000D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184D20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184CBC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184BF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_80184B08 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184D5C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_80184B1C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_80184A54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184D34 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_80184A90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184D70 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5215000E }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184D48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184BA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_801849DC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184C08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5215000F }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x52150010 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_80184A04 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_801849F0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_water_tank_80184BCC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_801849C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = func_dryfield_water_tank_8017EC38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_80184AE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 52 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_80184A18 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_dryfield_water_tank_80184DDC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = func_dryfield_water_tank_8017EC38 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_80184AE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = 4 }, { .value = 30 }, { .value = 30 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_water_tank_80184B48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_801849B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_water_tank_801859DC[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = 4 }, { .value = 30 }, { .value = 30 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_water_tank_80184B48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_801849B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Skeleton[1] = {
#include "assets/dryfield_night_water_tank_model_00FF8_skeleton.inc"
};

static u32 _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8PartVerts[1] = {
#include "assets/dryfield_night_water_tank_model_00FF8_partVerts.inc"
};

static SVECTOR _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Verts[84] = {
#include "assets/dryfield_night_water_tank_model_00FF8_verts.inc"
};

static SVECTOR _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Normals[72] = {
#include "assets/dryfield_night_water_tank_model_00FF8_normals.inc"
};

static u32 _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Stream[531] = {
#include "assets/dryfield_night_water_tank_model_00FF8_stream.inc"
};

static TmdSource _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8 = {
    0,
    3768,
    0,
    1,
    _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8PartVerts,
    _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Verts,
    _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Normals,
    _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Skeleton,
    _gDryfieldWaterTankDryfieldNightWaterTankModel00FF8Stream,
};

TaskDesc D_dryfield_water_tank_801868A4[2] = {
    { { { TASK_BODY_TMD, 192 } }, waterTankSwayTask, { .model = &_gDryfieldWaterTankDryfieldNightWaterTankModel00FF8 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gWaterTankYaw = 0;

s32 gWaterTankYawSpeed = 0;

s32 gWaterTankYawStep = 0;

s32 gWaterTankYawTarget = 0;

u16 D_dryfield_water_tank_801868CC[10] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    0,
};

WorldCollisionRoomResources D_dryfield_water_tank_801868E0[1] = {
    { D_dryfield_water_tank_80186EBC, D_dryfield_water_tank_80187FF8, D_dryfield_water_tank_80188920, NULL },
};

u8* D_dryfield_water_tank_801868F0[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_water_tank_801868F4[2] = { 10, 0 };

WorldCoordRoomLighting D_dryfield_water_tank_801868F8[1] = {
    { D_dryfield_water_tank_80188908, D_dryfield_water_tank_80188C58 },
};

DirectionWarpEntry D_dryfield_water_tank_80186900[2] = {
    { { { .word = 512 }, -2327, -0x2EDF, 1143 }, { 0, 0, 0, 0 }, { { .word = 512 }, -2327, -0x2EDF, 1143 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2560 }, 870, -0x4010, 934 }, { 0, 0, 0, 0 }, { { .word = 2560 }, 870, -0x4010, 934 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldWaterTankCollision098FCNormals[14] = {
#include "assets/dryfield_water_tank_collision_098FC_normals.inc"
};

static SVECTOR _gDryfieldWaterTankCollision098FCVerts[70] = {
#include "assets/dryfield_water_tank_collision_098FC_verts.inc"
};

static WorldCollisionGridFace _gDryfieldWaterTankCollision098FCFaces[36] = {
#include "assets/dryfield_water_tank_collision_098FC_faces.inc"
};

static s16 _gDryfieldWaterTankCollision098FCCells[118] = {
#include "assets/dryfield_water_tank_collision_098FC_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldWaterTankCollision098FCCells[i])
static s16* _gDryfieldWaterTankCollision098FCTable[4] = {
#include "assets/dryfield_water_tank_collision_098FC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_water_tank_80186EBC[1] = {
    { NULL, _gDryfieldWaterTankCollision098FCNormals, _gDryfieldWaterTankCollision098FCVerts, _gDryfieldWaterTankCollision098FCFaces, _gDryfieldWaterTankCollision098FCTable, 3500, 3300, 2, 2, 4000, 36 },
};

ViewCamera D_dryfield_water_tank_80186EE0[10] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x5DC0, 0 } }, 322 },
    { { { { 1429, 0, -3838 }, { -2523, 3086, -939 }, { 2892, 2692, 1077 } }, { 5050, 0x3E4E, 1880 } }, 257 },
    { { { { -853, 0, 4006 }, { 1212, 3903, 258 }, { -3818, 1239, -813 } }, { -5920, 0x37FA, -1350 } }, 257 },
    { { { { -1180, 0, 3922 }, { 173, 4092, 52 }, { -3918, 180, -1179 } }, { -4220, 0x42EA, -1350 } }, 230 },
    { { { { -2338, 0, 3362 }, { 0, 4096, 0 }, { -3362, 0, -2338 } }, { -6615, 0x324B, -2870 } }, 541 },
    { { { { -4013, 0, 817 }, { 0, 4096, 0 }, { -817, 0, -4013 } }, { -3050, 0x348A, -2000 } }, 680 },
    { { { { 4022, 0, 773 }, { 0, 4096, 0 }, { -773, 0, 4022 } }, { -3000, 0x348A, 1950 } }, 680 },
    { { { { -1032, 0, 3963 }, { 0, 4096, 0 }, { -3963, 0, -1032 } }, { -3180, 0x335E, -310 } }, 257 },
    { { { { 3261, 0, 2478 }, { 0, 4096, 0 }, { -2478, 0, 3261 } }, { -4900, 1150, 2500 } }, 257 },
    { { { { -2401, 0, 3317 }, { 3202, 1072, 2318 }, { -868, 3953, -628 } }, { 572, 0x42B6, 1557 } }, 380 },
};

SpriteBatch D_dryfield_water_tank_80187048[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_tank_80187058[31] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -120, 3693, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -96, -120, 1131, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -88, -120, 1028, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -88, -64, 1355, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -72, -120, 990, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -72, -48, 1277, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -56, -120, 954, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -56, -48, 1191, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, -120, 1123, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, -120, 1128, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, -120, 1045, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -56, 1416, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 56, -120, 992, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 56, -48, 1304, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 40, -120, 954, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 40, -48, 1228, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -144, 8, 1000, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, 16, 1000, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, 16, 875, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, 8, 875, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -64, 16, 875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -40, 40, 875, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -24, 48, 875, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 64, 875, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 64, 875, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, 40, 875, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 32, 750, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, 24, 875, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 16, 875, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 8, 875, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 0, 875, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tank_801872C4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_tank_801872E4[88] = {
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -96, -120, 1275, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -88, -120, 1250, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -120, 1125, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -120, 1125, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -120, 1250, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 72, -120, 1250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -120, 1250, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -120, 1250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -120, 1250, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, -120, 1047, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -48, -120, 1000, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -32, -120, 1000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, -120, 1000, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 0, -120, 1000, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, -120, 1000, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 32, -120, 1000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 0, 1500, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -88, -56, 1250, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -80, 0, 1500, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, 0, 1375, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, 0, 1375, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, 0, 1375, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -72, 0, 1500, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -64, -56, 1250, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -56, 1250, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, 0, 1375, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -80, -56, 1225, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -56, 1250, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -56, 1125, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 40, -56, 1127, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 48, -56, 1150, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, -56, 1250, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, -56, 1250, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 72, -56, 1250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, -56, 1375, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -32, -56, 1375, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -16, -56, 1125, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 0, -56, 1125, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 16, -56, 1125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, 0, 1375, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, 0, 1375, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 32, 0, 1375, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 40, 0, 1375, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 48, 0, 1500, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 56, 0, 1500, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 64, 0, 1500, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 0, 1500, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -32, 0, 1375, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -16, 0, 1375, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 0, 0, 1375, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 24, 750, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -152, -24, 900, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -144, -24, 875, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -136, 8, 750, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 8, 750, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 40, 750, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 8, 750, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -112, 40, 750, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -96, 16, 750, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 64, 750, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -88, 16, 750, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 48, 750, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, 48, 750, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, 48, 750, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, 16, 750, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, 16, 750, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 56, 750, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -16, 16, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 24, 750, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 24, 750, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, 24, 776, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, 56, 750, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, 56, 750, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 56, 750, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, 16, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 48, 750, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 64, 16, 750, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 8, 750, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 0, 750, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 112, 0, 750, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 128, 0, 750, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 136, -8, 750, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 144, -8, 750, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 24, 750, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 144, 16, 750, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 40, 750, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 32, 750, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 56 } }, -48, -40, 1125, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tank_801879C4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 50, 0, 0, { 1, 0 } },
    { 50, 37, 0, 0, { 2, 0 } },
    { 87, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_tank_801879EC[10] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, 8, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 8, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 64, 8, 625, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 56, 24, 625, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 56, 625, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 24, 625, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, 72, 625, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 104, 625, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 40, 80, 625, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 96, 625, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tank_80187AB4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_tank_80187ACC[25] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, -24, 500, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 32, 500, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -136, -24, 500, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -24, 500, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -24, 500, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, 16, 500, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 32, 500, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -88, -24, 500, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, -24, 500, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -48, -24, 500, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -16, -24, 500, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, 32, 500, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -48, 32, 500, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -16, 32, 500, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 136 } }, 16, -24, 500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -24, 500, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, -32, 500, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, -32, 500, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, -32, 500, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -32, 500, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 32, 40, 500, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 64, 40, 500, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 40, 500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, 40, 500, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 40, 500, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tank_80187CC0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 25, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_tank_80187CD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_tank_80187CE8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_tank_80187CF8[1] = {
    { 143, 0x3FC0, { .fields = { 88, 240 } }, -40, -120, 375, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tank_80187D0C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_tank_80187D24[27] = {
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 40, -88, 750, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 40, 750, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -16, 750, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, 32, 750, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 56, -80, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 0, 750, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 32, 750, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, 24, 750, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 0, 750, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, -72, 750, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -72, 750, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -32, 750, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 16, 750, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 0, -48, 1875, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 0, 0, 1875, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, 0, 1875, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -48, 1875, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, -48, 1875, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, 0, 1875, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 48, -8, 1875, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -56, 1875, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -80, -120, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -80, 0, 250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -64, 0, 250, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -64, -120, 250, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -48, -120, 250, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -48, 0, 250, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_tank_80187F40[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 3, 0 } },
    { 10, 3, 0, 0, { 0, 0 } },
    { 13, 8, 0, 0, { 2, 0 } },
    { 21, 6, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_tank_80187F70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_water_tank_80187F80[10] = {
    { { .empty = D_dryfield_water_tank_80187048 }, D_dryfield_water_tank_80187048, NULL },
    { { .elements = D_dryfield_water_tank_80187058 }, D_dryfield_water_tank_801872C4, NULL },
    { { .elements = D_dryfield_water_tank_801872E4 }, D_dryfield_water_tank_801879C4, NULL },
    { { .elements = D_dryfield_water_tank_801879EC }, D_dryfield_water_tank_80187AB4, NULL },
    { { .elements = D_dryfield_water_tank_80187ACC }, D_dryfield_water_tank_80187CC0, NULL },
    { { .empty = D_dryfield_water_tank_80187CD8 }, D_dryfield_water_tank_80187CD8, NULL },
    { { .empty = D_dryfield_water_tank_80187CE8 }, D_dryfield_water_tank_80187CE8, NULL },
    { { .elements = D_dryfield_water_tank_80187CF8 }, D_dryfield_water_tank_80187D0C, NULL },
    { { .elements = D_dryfield_water_tank_80187D24 }, D_dryfield_water_tank_80187F40, NULL },
    { { .empty = D_dryfield_water_tank_80187F70 }, D_dryfield_water_tank_80187F70, NULL },
};

WorldCollisionTrigger D_dryfield_water_tank_80187FF8[4] = {
    { NULL, NULL, NULL, { -864, -0x32D0, 2464, 0 }, { { 0, -1968, -1024, 0 }, { 0, -1968, 1024, 0 }, { 0, 1968, -1024, 0 }, { 0, 1968, 1024, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2217, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -576, -0x3310, 2464, 0 }, { { 0, -1904, 1024, 0 }, { 0, -1904, -1024, 0 }, { 0, 1904, 1024, 0 }, { 0, 1904, -1024, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2157, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 768, -0x32D0, -2368, 0 }, { { 0, -1936, 1024, 0 }, { 0, -1936, -1024, 0 }, { 0, 1936, 1024, 0 }, { 0, 1936, -1024, 0 } }, { -4103, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2187, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 544, -0x3330, -2336, 0 }, { { 0, -1904, -1024, 0 }, { 0, -1904, 1024, 0 }, { 0, 1904, -1024, 0 }, { 0, 1904, 1024, 0 } }, { 4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2157, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_water_tank_80188128[21] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -0x36B0, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -0x36B0, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -0x36B0, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, -0x36B0, -2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -0x36B0, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -0x36B0, -2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -0x36B0, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -0x36B0, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -0x36B0, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1500, -0x36B0, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -0x36B0, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -0x36B0, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -0x36B0, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1500, -0x36B0, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -0x36B0, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -0x36B0, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 61, -0x4650, 1039 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1960, -0x4650, 520 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2340, -0x4718, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 582, -0x4650, -880 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -0x4CCC, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
};

WorldCoordRoomLights D_dryfield_water_tank_80188908[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_water_tank_80188128), D_dryfield_water_tank_80188128, 0, NULL },
};

WorldCollisionTrigger D_dryfield_water_tank_80188920[9] = {
    { NULL, NULL, NULL, { -2449, -0x2F60, 640, 0 }, { { -728, 0, -256, 0 }, { 336, 0, -584, 0 }, { -624, 0, 680, 0 }, { 1016, 0, 160, 0 } }, { 0, 4096, 0, 0 }, { 401, 0, 4076, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_WARP, 20, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2512, -0x2F20, 720, 0 }, { { -840, 0, -144, 0 }, { 896, 0, -152, 0 }, { -832, 0, 120, 0 }, { 776, 0, 176, 0 } }, { 0, 4100, 0, 0 }, { 401, 0, 4076, 0 }, 907, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -1668, -0x4040, -1586, 0 }, { { 1321, 0, -1157, 0 }, { 1703, 0, 329, 0 }, { -1148, 0, 1191, 0 }, { 396, 0, 1526, 0 } }, { 0, 4115, 0, 0 }, { 401, 0, 4076, 0 }, 1755, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 1, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1432, -0x2F23, -17, 0 }, { { 159, 0, 627, 0 }, { 157, 0, -595, 0 }, { 1387, 0, 451, 0 }, { 1371, 0, -481, 0 } }, { 0, 4107, 0, 0 }, { 4094, 0, 0, 0 }, 1453, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -656, -0x2F21, 2160, 0 }, { { -841, 0, 1091, 0 }, { -619, 0, -1091, 0 }, { 611, 0, 1171, 0 }, { 851, 0, -1169, 0 } }, { 0, 4100, 0, 0 }, { 4094, 0, 0, 0 }, 1442, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 1031, -0x2F40, 1303, 0 }, { { -269, 0, 617, 0 }, { 667, 0, -339, 0 }, { 261, 0, 1577, 0 }, { 1711, 0, 194, 0 } }, { 0, 4111, 0, 0 }, { 2750, 0, 3035, 0 }, 1717, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1255, -0x4040, 1247, 0 }, { { 668, 0, -496, 0 }, { -403, 0, 647, 0 }, { 39, 0, -1272, 0 }, { -1328, 0, 97, 0 } }, { 0, 4103, 0, 0 }, { -3166, 0, -2599, 0 }, 1330, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1296, -0x4046, 1232, 0 }, { { 780, 0, -320, 0 }, { -291, 0, 823, 0 }, { 439, 0, -936, 0 }, { -928, 0, 433, 0 } }, { 0, 4096, 0, 0 }, { -3166, 0, -2599, 0 }, 1031, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1280, -0x2F20, 1560, 0 }, { { -685, 0, 241, 0 }, { 187, 0, -619, 0 }, { -251, 0, 689, 0 }, { 751, 0, -310, 0 } }, { 0, 4097, 0, 0 }, { 2750, 0, 3035, 0 }, 812, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_dryfield_water_tank_80188BCC[2] = {
    { 140, 204, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_120400_8013E748 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_water_tank_80188BE4[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_water_tank_80188BF0[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017B644, D_dryfield_water_tank_80188BCC },
    { D_map_dryfield_8017B664, D_dryfield_water_tank_80188BE4 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCoordRoomAmbientEntry D_dryfield_water_tank_80188C58[11] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_water_tank_80188C58) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1500, 1500, 1500, 1500 } },
    { .color = { 1500, 1500, 1500, 1500 } },
    { .color = { 2000, 2000, 2000, 2000 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_dryfield_water_tank_80188CB0 = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionFootstepSounds D_dryfield_water_tank_80188CBC = {
    0x10000045,
    0x10000047,
    0x10000045,
};

WorldCollisionFootstepSounds D_dryfield_water_tank_80188CC8 = {
    0x10000015,
    0x10000017,
    0x10000019,
};

WorldCollisionSurfaceProperties D_dryfield_water_tank_80188CD4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_water_tank_80188CDC[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_water_tank_80188CE4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_water_tank_80188CB0 },
};

WorldCollisionSurfaceProperties D_dryfield_water_tank_80188CEC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_water_tank_80188CBC },
};

WorldCollisionSurfaceProperties D_dryfield_water_tank_80188CF4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_water_tank_80188CC8 },
};

WorldCollisionSurfaceProperties* D_dryfield_water_tank_80188CFC[8] = {
    D_dryfield_water_tank_80188CD4,
    D_dryfield_water_tank_80188CDC,
    D_dryfield_water_tank_80188CD4,
    D_dryfield_water_tank_80188CE4,
    D_dryfield_water_tank_80188CEC,
    D_dryfield_water_tank_80188CF4,
    D_dryfield_water_tank_80188CD4,
    D_dryfield_water_tank_80188CD4,
};

AreaApplyRec D_dryfield_water_tank_80188D1C[10] = {
    { 2, 2, 3, 17 },
    { 2, 2, 7, 33 },
    { 2, 15, 3, 17 },
    { 2, 15, 7, 33 },
    { 2, 20, 3, 1 },
    { 2, 24, 2, 1 },
    { 2, 29, 3, 17 },
    { 2, 29, 8, 33 },
    { 2, 30, 2, 0 },
    { 255, 0, 0, 0 },
};

Task* D_dryfield_water_tank_80188D44 = NULL;

Task* D_dryfield_water_tank_80188D50;

static void func_dryfield_water_tank_8017D9D4(Task* task);
static void func_dryfield_water_tank_8017DA4C(Task* task);
static s32  func_dryfield_water_tank_8017DB98(Task* arg0);

void func_dryfield_water_tank_8017D618(Task* arg0)
{
    Task* task;

    task = arg0;
    switch (task->state) {
        case 0:
            if (gameFlagGetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE) == 3) {
                Gp_StartCapSlot(0xE, 1, 1);
                break;
            }
            gGameSession->eventState       = 1;
            D_dryfield_water_tank_80188D48 = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
            Gp_MsgPlayer3F3(0);
            Gp_MsgPlayerWeapon(0);
            Gp_StartCapSlot(0xE, 0, 0);
            arg0->state = task->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() == 0) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
                task->state                    = task->state + 1;
            }
            return;
        case 2:
            if (Gp_GetCapEventKey() == 0xA) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                gameFlagSetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE, 3);
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 4), 0, 0);
                taskSpawnFromTable(D_dryfield_water_tank_8017FF88, 0, 0, 0);
                func_dryfield_water_tank_8017DB48();
            } else {
                gGameSession->eventState                                   = 0;
                gGameSession->hideHud                                      = 0;
                gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_RUNNING;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = (u8)D_dryfield_water_tank_80188D48;
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
            }
            break;
        default:
            return;
    }
    taskKill(task);
}

/// Handler for message 0x13F1 in the room task's message table: accepts the
/// message and does nothing, answering 0.
s32 func_dryfield_water_tank_8017D7BC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler for message 0x13EE in the room task's message table: copies the
/// location record the sender passes onto the reply record and answers 1.
s32 func_dryfield_water_tank_8017D7C4(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    return 1;
}

s32 func_dryfield_water_tank_8017D7EC(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId == 1) {
        if (gameFlagGetNibble(GAME_FLAG_DRYFIELD_WATER_TANK_036) == 0) {
            gameFlagSetNibble(GAME_FLAG_DRYFIELD_WATER_TANK_036, 1);
            taskSpawnFromTable(D_dryfield_water_tank_8017F34C, 0, 0, 0);
            gameFlagSetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_PROGRESS, 1);
        }
    }
    if ((request->actionId == 2) && (gameFlagGetNibble(GAME_FLAG_WATER_TANK_SCENE_SEEN) == 0)) {
        gameFlagSetNibble(GAME_FLAG_WATER_TANK_SCENE_SEEN, 1);
        func_800E3FAC(0xA2, 0xE);
        gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
        gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 3);
        Gp_ApplyAreaRecs(D_dryfield_water_tank_80188D1C);
        Gp_MsgPlayerWeapon(0);
        func_800E8634(D_dryfield_water_tank_80184E0C, 0, D_dryfield_water_tank_801859DC);
    }
    if (request->actionId == 3) {
        func_800E8614(D_dryfield_water_tank_8017F114, 0);
    }
    if (request->actionId == 4) {
        func_800E8614(D_dryfield_water_tank_8017F21C, 0);
    }
    return 1;
}

/// Room message handler: on message `0xE` spawn the second entry of
/// `D_dryfield_water_tank_8017F34C`, the same table `func_dryfield_water_tank_8017D7EC`
/// takes entry 0 from.
s32 func_dryfield_water_tank_8017D910(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0xE) {
        taskSpawnFromTable(D_dryfield_water_tank_8017F34C, 1, 0, 0);
    }
    return 0;
}

/// Room event task: state 0 spawns the child from `D_dryfield_water_tank_80180794`
/// and parks it in `D_dryfield_water_tank_80188D44`, state 1 kills this task once
/// that child has been killed.
void func_dryfield_water_tank_8017D948(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            D_dryfield_water_tank_80188D44 = taskSpawnFromTable(&D_dryfield_water_tank_80180794, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(D_dryfield_water_tank_80188D44, &poll) != 0) {
                taskKill(task);
            }
            return;
    }
}

/// State 0 of the room's event task: publish the message table the room's
/// handlers hang off (`0x13EE`–`0x13F1`), take pointer slot 7, spawn the
/// cutscene task from `D_dryfield_water_tank_801868A4`, queue sound event
/// `0x52150009`, run the game-flag `0x55` dispatch in
/// `func_dryfield_water_tank_8017DB48`, then advance.
static void func_dryfield_water_tank_8017D9D4(Task* task)
{
    task->msgTable = D_dryfield_water_tank_8017F324;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(D_dryfield_water_tank_801868A4, 0, 0, 0);
    sndEvtRequestScriptStart(SOUND_WATER_TANK_AMBIENCE, 0, 0);
    func_dryfield_water_tank_8017DB48();
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room task, run every frame: while `gGameSession->viewReady`
/// is set, queue the water-tank ambience sound events `0x52150011` and
/// `0x52150012`, each as a type-6 event in the view it belongs to (4 and 0xA
/// respectively) and as a type-7 event (argument 0x2D / 0x3C) from any other
/// view.
static void func_dryfield_water_tank_8017DA4C(Task* task)
{
    if (gGameSession->viewReady != 0) {
        if (gGameSession->location.loc.view == 4) {
            Gp_EnqueueStageSnd6(SOUND_WATER_TANK_VIEW4_AMBIENCE, 0, 0);
        } else {
            Gp_EnqueueStageSnd7(SOUND_WATER_TANK_VIEW4_AMBIENCE, 0x2D);
        }
        if (gGameSession->location.loc.view == 0xA) {
            Gp_EnqueueStageSnd6(SOUND_WATER_TANK_VIEW10_AMBIENCE, 0, 0);
            return;
        }
        Gp_EnqueueStageSnd7(SOUND_WATER_TANK_VIEW10_AMBIENCE, 0x3C);
    }
}

/// The room task's three states, run from a stack copy by
/// `func_dryfield_water_tank_8017DAF0`: the entry tick, the per-frame
/// ambience, then `taskKill`.
static const TaskFuncTable3 D_dryfield_water_tank_8017D5C4 = {
    { func_dryfield_water_tank_8017D9D4, func_dryfield_water_tank_8017DA4C, taskKill },
};

/// The room task: copies its three-state table onto the stack and runs the
/// entry for the task's current state.
void func_dryfield_water_tank_8017DAF0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_water_tank_8017D5C4;
    sp.funcs[task->state](task);
}

static void func_dryfield_water_tank_8017DB48(void)
{
    switch (gameFlagGetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE)) {
        case 0:
        case 1:
        case 2:
            func_dryfield_water_tank_8017EFF4(1);
            break;
        case 3:
            func_dryfield_water_tank_8017EFF4(0);
            break;
    }
}

/// The model task's script, run each frame while the task is in state 2;
/// returning 1 tells the caller the model has arrived.
///
/// Script state 0 lowers the model: its Z grows by 0x14 a frame, its Y snaps to
/// the lowered record's `pos.vy` (nudged by the `gDisplayState.gameTick` flag), and once the
/// Z has passed that record's `pos.vz` the script steps to state 1. Every frame
/// of state 0 also spawns effect 0x60054 at the model, offset in X by the next
/// entry of the wrapping `killCountdown` table. State 1 advances the settle
/// counter; on its 0x3D-th tick it publishes the lowered record to the task
/// itself as the 0x7D4 placement and returns 1, and until then snaps the
/// model's X to that record's `pos.vx`. Every path that returns 0 clears
/// `coord->composeStamp`, so the coordinate is recomputed on the next update.
///
/// The body is the water tower's `func_dryfield_water_tower_8017E428`; as there,
/// the Z test is written with the coordinate on the left, which is what loads it
/// before the record.
static s32 func_dryfield_water_tank_8017DB98(Task* arg0)
{
    _DryfieldWaterTankPropSceneWork* work  = arg0->work;
    GfxCoord*                        coord = arg0->extra.tmd->coords;
    GfxCoord*                        effCoord;
    SVECTOR                          pos;

    switch (work->slidePhase) {
        case DRYFIELD_WATER_TANK_PROP_SLIDE_MOVING:
            coord->coord.t[2] += 0x14;
            coord->coord.t[1]  = D_dryfield_water_tank_8017FD60[1].pos.vy;
            if (gDisplayState.gameTick & 4) {
                coord->coord.t[1] += 5;
            }
            if (coord->coord.t[2] > D_dryfield_water_tank_8017FD60[1].pos.vz) {
                work->slidePhase++;
            }
            effCoord = arg0->extra.tmd->coords;
            if (arg0->killCountdown >= 0xA) {
                arg0->killCountdown = 0;
            } else {
                arg0->killCountdown = (u16)arg0->killCountdown + 1;
            }
            pos.vy = 0;
            pos.vz = 0;
            pos.vx = D_dryfield_water_tank_8017FDA8[arg0->killCountdown];
            Gp_SpawnEff(EFFECT_DUST_PUFF, effCoord, 0x80002300, &pos);
            break;

        case DRYFIELD_WATER_TANK_PROP_SLIDE_SETTLING:
            work->settleFrames++;
            if (work->settleFrames >= DRYFIELD_WATER_TANK_PROP_SETTLE_FRAMES) {
                TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_dryfield_water_tank_8017FD60[1], 0);
                return 1;
            }
            coord->coord.t[0] = D_dryfield_water_tank_8017FD60[1].pos.vx;
            if (gDisplayState.gameTick & 4) {
                coord->coord.t[0] += 5;
            }
            break;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Drives the model task the room's script spawns: state 0 allocates the
/// light/colour matrix pair for the task's `TmdObject` and reparents the task
/// to the script driver, state 1 idles, and state 2 waits for
/// `func_dryfield_water_tank_8017DB98` to report the model finished. Every
/// frame it hands the model part's translation to `worldCoordSetModelLighting`, which turns
/// it into the light/colour matrices.
void func_dryfield_water_tank_8017DD20(Task* arg0)
{
    TmdObject*                       extra;
    GfxCoord*                        coord;
    _DryfieldWaterTankPropSceneWork* work;
    TmdObject*                       mdl;
    VECTOR                           pos;

    switch (arg0->state) {
        case 0:
            extra      = arg0->extra.tmd;
            coord      = extra->coords;
            work       = memMalloc(sizeof(*work), false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                coord->parent    = &gGfxViewCoord;
                extra->flags     = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                tmdAllocPrimitiveBuffer(extra);
                extra->lightMtx = &work->lightMtx;
                extra->colorMtx = &work->colorMtx;
                arg0->msgTable  = D_dryfield_water_tank_8017FD90;
                taskReparent(D_dryfield_water_tank_80188D4C, arg0);
            }
            arg0->state += 1;
            break;
        case 1:
            break;
        case 2:
            if (func_dryfield_water_tank_8017DB98(arg0) & 0xFFFF) {
                arg0->state = 1;
            }
            break;
    }

    mdl    = arg0->extra.tmd;
    pos.vx = arg0->extra.tmd->coords->workm.t[0];
    pos.vy = arg0->extra.tmd->coords->workm.t[1];
    pos.vz = arg0->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(mdl, &pos, 0, 3);
}

/// Per-frame driver of the water tank's prop scene. It is the task parked in
/// `D_dryfield_water_tank_80188D4C`, which is how the scene script's two
/// callbacks reach the `_DryfieldWaterTankPropSceneWork` it keeps at
/// `Task::work`. State 0 allocates that block, stores the player task in it and
/// spawns the prop task from `D_dryfield_water_tank_8017FF88` as its
/// `propTask`; state 1 places the prop and starts the scene's event script;
/// state 2 asks to be killed once the event is over. Every frame it then
/// carries out the block's `request`, if one was posted, and clears it.
void func_dryfield_water_tank_8017DEA4(Task* arg0)
{
    _DryfieldWaterTankPropSceneWork* work;
    ActorCommand                     msg;
    Task**                           playerTask;

    work = arg0->work;
    switch (arg0->state) {
        case 0:
            work       = memMalloc(sizeof(*work), false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->playerTask               = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_dryfield_water_tank_80188D4C = arg0;
            }
            work           = arg0->work;
            work->propTask = taskSpawnFromTable(D_dryfield_water_tank_8017FF88, 1, 0, 0);
            arg0->state    = arg0->state + 1;
            break;
        case 1:
            TASK_MESSAGE_DISPATCH_POINTER(work->propTask, ACTOR_MESSAGE_PLACE, &D_dryfield_water_tank_8017FD60, 0);
            func_800E8634(D_dryfield_water_tank_8017FDC0, 0, D_dryfield_water_tank_8017FEC8);
            arg0->state = arg0->state + 1;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(arg0, 0);
            }
            break;
    }

    // Carry out the request the scene script posted, for this one frame.
    work = arg0->work;
    switch (work->request) {
        /* This arm does nothing, and the switch needs it as written: it is what
         * puts four values in the case list, so the decision tree roots at the
         * start-slide node the way the ROM's does. */
        case DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_NONE:
            break;
        case DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_START_SLIDE:
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
            taskMessageDispatch(work->propTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            msg.command = 2;
            TASK_MESSAGE_DISPATCH_POINTER(work->propTask, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
            break;
        case DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_SHOW_PLAYER:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = Gp_FindViewIndex(3);
            gGameSession->viewDirty                                    = 1;
            /* Through a pointer rather than as `work->playerTask`: a member load
             * is struct memory, which lets the store to the view index sink into
             * the call's delay slot; the two request tails then no longer
             * cross-jump as the original's do. */
            playerTask = &work->playerTask;
            taskMessageDispatch(*playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            break;
        case DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_PLAY_SOUNDS:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 2), 0, 0);
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 8), 0, 0);
            break;
    }
    work->request = DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_NONE;
}

/// Excludes the task's `TmdObject` from active drawing while `arg2` is zero,
/// and clears that bit otherwise. `arg1` is unused; the flag is the *third*
/// argument, so the second slot is only there to place it in `$a2`. Byte for
/// byte the actors library's `ActorsShared801346ec`, which toggles the same bit
/// of the same field for the model of the task it is handed.
void func_dryfield_water_tank_8017E0B4(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    if (arg2 != 0) {
        obj->flags = obj->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    obj->flags = obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

#include "../../shared/actor_messages_place_ypr.inc.c"

/// Message 0x7DB handler of the model task: restarts its script, clearing the
/// script state and `field_54` in its work block and moving the task to the
/// state the payload carries.
s32 func_dryfield_water_tank_8017E174(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    _DryfieldWaterTankPropSceneWork* work;
    s32                              state;

    work                = task->work;
    work->slidePhase    = DRYFIELD_WATER_TANK_PROP_SLIDE_MOVING;
    work->field_54      = 0;
    state               = msg->command;
    task->killCountdown = 0;
    task->state         = state;
}

/// Scene-script callback that posts a request to the prop scene's driver:
/// reaches the driver's work block through the task parked in
/// `D_dryfield_water_tank_80188D4C`, stores `request` (one of
/// `DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_*`) for the driver's next frame and
/// clears `field_52` beside it.
void func_dryfield_water_tank_8017E194(s16 request)
{
    _DryfieldWaterTankPropSceneWork* work;

    work           = D_dryfield_water_tank_80188D4C->work;
    work->request  = request;
    work->field_52 = 0;
}

/// Second sibling entry point into the script driver, the one that ends the
/// water-tank scene: publishes view 3's area-record index, asks the view gate
/// for a switch through `GameSession.viewDirty`, dispatches message 0x3F3 with
/// argument 1 to the driver's `playerTask`, and fires the scene's sound event.
void func_dryfield_water_tank_8017E1B4(void)
{
    _DryfieldWaterTankPropSceneWork* work;
    Task**                           playerTask;

    work                                                       = D_dryfield_water_tank_80188D4C->work;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = Gp_FindViewIndex(3);
    /* Through a pointer rather than as `work->playerTask`: a member load is
     * struct memory, which lets the store to the view index sink into the
     * call's delay slot, and the original keeps it ahead of the load. */
    playerTask = &work->playerTask;
    taskMessageDispatch(*playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    gGameSession->viewDirty = 1;
    sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TANK, 2), 0xA);
}

#include "../../shared/screen_fade_in_tile.inc.c"
