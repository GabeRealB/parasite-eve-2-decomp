#include "rooms/shelter_b6_corridor.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
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

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"
#include "../../shared/glow_draw.h"

/// This room's grid object continues four unread bytes past the quad array,
/// so `screenWaveGridTask` indexes `quads` rather than the whole object.
#define SCREEN_WAVE_GRID gScreenWaveGrid.quads
#include "../../shared/screen_wave.h"

#define D_shelter_b6_corridor_8017F844 (D_shelter_b6_corridor_8017F834 + 2)
#define D_shelter_b6_corridor_8017F874 (D_shelter_b6_corridor_8017F834 + 8)

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 gScreenWaveRamp;

/// The room task's message records.
extern TaskMessageEntry D_shelter_b6_corridor_8017EF24[];

extern EvsCommand D_shelter_b6_corridor_8017F354[];
extern EvsCommand D_shelter_b6_corridor_8017F684[];

/// The context the wave task was spawned with: its ramp limit, peak, mode and
/// tint.
extern ScreenWaveCtx* gScreenWaveCtx;

/// Phase records of the wave's 9 column edges and 30 row edges.
extern ScreenWaveGridOscillator gScreenWaveColumns[10];
extern ScreenWaveGridOscillator gScreenWaveRows[30];

/// Screen-wave quad meshes for this room, plus the unread bytes after them.
///
/// `quads` is the double-buffered 8 by 30 mesh. Nothing reads `pad`; those
/// four bytes fill the gap to the next object, which begins on an eight-byte
/// boundary.
typedef struct {
    POLY_FT4 quads[2][30][8]; // one mesh per frame buffer, 30 rows by 8 quads
    u8       pad[4];          // unread; fills the gap to the next eight-byte boundary
} _ShelterB6CorridorScreenWaveGrid;
STATIC_ASSERT_SIZEOF(_ShelterB6CorridorScreenWaveGrid, 19204);

/// This room's screen-wave meshes. `screenWaveGridTask` indexes `quads`
/// through `SCREEN_WAVE_GRID`.
extern _ShelterB6CorridorScreenWaveGrid gScreenWaveGrid;

/// Eight bytes of room work storage whose role is unproven.
///
/// The room task writes `field_0` once, while setting the room up, and nothing
/// reads it back. `unknown_2` has no recovered access at all: it stands for the
/// zero bytes between that value and the next object, which begins on an
/// eight-byte boundary. Whether those bytes are further members, storage of
/// their own or alignment is unresolved.
typedef struct {
    s16 field_0;      // Set to 2 when the room task starts; never read, role unproven
    u8  unknown_2[6]; // Zero image bytes; role and grouping unproven
} _ShelterB6CorridorStorage51B0;
STATIC_ASSERT_SIZEOF(_ShelterB6CorridorStorage51B0, 8);

extern _ShelterB6CorridorStorage51B0 D_shelter_b6_corridor_801851B0;
extern s32                           D_shelter_b6_corridor_801851B8;

// Indexed views below share one contiguous table.
extern AnimationPlayRequest     D_shelter_b6_corridor_8017F27C;
extern ActorCommand             D_shelter_b6_corridor_8017F34C;
extern ActorCommand             D_shelter_b6_corridor_8017F350;
extern AnimationBankCopyRequest D_shelter_b6_corridor_8017F260;
void                            func_shelter_b6_corridor_8017E19C(s32);
void                            func_shelter_b6_corridor_8017E204(void);

void func_shelter_b6_corridor_8017E19C(s32);
void func_shelter_b6_corridor_8017E204(void);

static s32 _shelterB6CorridorRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedArg);
s32        func_shelter_b6_corridor_8017DEB0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32        func_shelter_b6_corridor_8017DF48(Task*, s32, s32, s32);
static s32 _shelterB6CorridorIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, DirectionActionRequest* unusedRequest, s32 unusedArg);
s32        func_shelter_b6_corridor_8017E028(Task*, s32, s32, s32);

TaskDesc D_shelter_b6_corridor_8017EF08[2] = {
    { { { TASK_BODY_NONE, 192 } }, screenWaveGridTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

enum { SHELTER_B6_CORRIDOR_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskMessageEntry D_shelter_b6_corridor_8017EF24[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b6_corridor_8017DEB0 },
    { SHELTER_B6_CORRIDOR_MESSAGE_USE_KEY_ITEM, _shelterB6CorridorRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB6CorridorIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, func_shelter_b6_corridor_8017DF48 },
    { ROOM_MESSAGE_ACTOR_EVENT, func_shelter_b6_corridor_8017E028 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static AnimationPackedPose _gShelterB6CorridorAnimation01C70Bank1[6] = {
#include "assets/shelter_b6_corridor_animation_01C70_bank1.inc"
};

static AnimationPackedRotation _gShelterB6CorridorAnimation01C70Bank4[46] = {
#include "assets/shelter_b6_corridor_animation_01C70_bank4.inc"
};

static AnimationRecord _gShelterB6CorridorAnimation01C70Records[109] = {
#include "assets/shelter_b6_corridor_animation_01C70_records.inc"
};

static u16 _gShelterB6CorridorAnimation01C70Indices[20] = {
#include "assets/shelter_b6_corridor_animation_01C70_indices.inc"
};

static AnimationSet _gShelterB6CorridorAnimation01C70 = {
    _gShelterB6CorridorAnimation01C70Records,
    _gShelterB6CorridorAnimation01C70Indices,
    { NULL, _gShelterB6CorridorAnimation01C70Bank1, NULL, NULL, _gShelterB6CorridorAnimation01C70Bank4, NULL, NULL, NULL },
};

AnimationSet* D_shelter_b6_corridor_8017F258[2] = {
    NULL,
    &_gShelterB6CorridorAnimation01C70,
};

AnimationBankCopyRequest D_shelter_b6_corridor_8017F260 = { { .sets = D_shelter_b6_corridor_8017F258 }, ARRAY_SIZE(D_shelter_b6_corridor_8017F258) };

AnimationPlayRequest D_shelter_b6_corridor_8017F268 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_corridor_8017F27C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_corridor_8017F290[5] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE },
};

ActorTransform D_shelter_b6_corridor_8017F2F4 = { { 7000, 0, 0, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_shelter_b6_corridor_8017F30C = { { 7800, 0, 0, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_shelter_b6_corridor_8017F324 = { { 0x4E20, 0, 0, 0 }, { 0, 1024, 0, 0 } };

ActorCommand D_shelter_b6_corridor_8017F33C = { { .loc = { 5, 24 } }, 1 };

ActorCommand D_shelter_b6_corridor_8017F340 = { { .loc = { 5, 24 } }, 2 };

ActorMotionWalkAnim D_shelter_b6_corridor_8017F344 = { .animationId = 3, .nextAnimId = 4 };

ActorCommand D_shelter_b6_corridor_8017F34C = { { .loc = { 5, 24 } }, 1 };

ActorCommand D_shelter_b6_corridor_8017F350 = { { .loc = { 5, 24 } }, 1 };

EvsCommand D_shelter_b6_corridor_8017F354[34] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_shelter_b6_corridor_8017F260 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_corridor_8017F27C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_corridor_8017E204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_corridor_8017F34C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_corridor_8017F350 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_corridor_8017F33C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_shelter_b6_corridor_8017F2F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_shelter_b6_corridor_8017F30C } }, { .message = { .pointer = &D_shelter_b6_corridor_8017F344 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55180004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_corridor_8017F340 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_shelter_b6_corridor_8017F324 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_corridor_8017E19C }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b6_corridor_8017F684[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_corridor_8017E204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_shelter_b6_corridor_8017F260 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_corridor_8017F27C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_corridor_8017F34C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_corridor_8017F350 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_corridor_8017E19C }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

SVECTOR D_shelter_b6_corridor_8017F834[16] = {
    { 2000, -3250, 1450, 0 },
    { 2000, -400, 1450, 0 },
    { 4000, -3250, 1450, 0 },
    { 4000, -400, 1450, 0 },
    { 6000, -3250, 1450, 0 },
    { 6000, -400, 1450, 0 },
    { 8000, -3250, 1450, 0 },
    { 8000, -400, 1450, 0 },
    { 2000, -3250, -1450, 0 },
    { 2000, -400, -1450, 0 },
    { 4000, -3250, -1450, 0 },
    { 4000, -400, -1450, 0 },
    { 6000, -3250, -1450, 0 },
    { 6000, -400, -1450, 0 },
    { 8000, -3250, -1450, 0 },
    { 8000, -400, -1450, 0 },
};

u8* D_shelter_b6_corridor_8017F8B4[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b6_corridor_8017F8B8[1] = { 5 };

DirectionWarpEntry D_shelter_b6_corridor_8017F8BC[2] = {
    { { { .word = 1024 }, 430, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 430, 0, 0 }, { 0, 0, 0, 0 }, 0x55180001, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 8400, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 8400, 0, 0 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, 0x55180003, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB6CorridorCollision024D0Normals[6] = {
#include "assets/shelter_b6_corridor_collision_024D0_normals.inc"
};

static SVECTOR _gShelterB6CorridorCollision024D0Verts[12] = {
#include "assets/shelter_b6_corridor_collision_024D0_verts.inc"
};

static WorldCollisionGridFace _gShelterB6CorridorCollision024D0Faces[12] = {
#include "assets/shelter_b6_corridor_collision_024D0_faces.inc"
};

static s16 _gShelterB6CorridorCollision024D0Cells[28] = {
#include "assets/shelter_b6_corridor_collision_024D0_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB6CorridorCollision024D0Cells[i])
static s16* _gShelterB6CorridorCollision024D0Table[3] = {
#include "assets/shelter_b6_corridor_collision_024D0_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b6_corridor_8017FA90 = { NULL, _gShelterB6CorridorCollision024D0Normals, _gShelterB6CorridorCollision024D0Verts, _gShelterB6CorridorCollision024D0Faces, _gShelterB6CorridorCollision024D0Table, 0, 1350, 3, 1, 4000, 12 };

ViewCamera D_shelter_b6_corridor_8017FAB4[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4500, 0x61A8, 0 } }, 680 },
    { { { { 715, 0, 4033 }, { 264, 4087, -46 }, { -4024, 268, 713 } }, { -7040, 1470, 560 } }, 230 },
    { { { { 682, 0, -4038 }, { -252, 4087, -42 }, { 4030, 256, 681 } }, { -1740, 1470, 560 } }, 230 },
    { { { { 955, 0, 3982 }, { -1522, 3784, 365 }, { -3680, -1565, 883 } }, { -3190, 170, 810 } }, 230 },
    { { { { 0, 0, -4096 }, { -860, 4004, 0 }, { 4004, 860, 0 } }, { -6235, 1350, 0 } }, 329 },
};

SpriteBatch D_shelter_b6_corridor_8017FB68[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b6_corridor_8017FB78[35] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 32, 1246, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -120, 906, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -88, 1012, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -56, 1009, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -24, 1100, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 8, 1037, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 40, 1007, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -112, 1025, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -80, 1170, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -48, 1167, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -16, 1162, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 16, 1162, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -96, 1174, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -72, 1278, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -48, 1287, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -24, 1296, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 0, 1303, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 24, 1288, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -88, 1256, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -120, 615, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -80, 586, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -40, 591, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 0, 605, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 40, 614, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 556, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -120, 649, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -80, 656, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -40, 662, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 0, 661, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 40, 660, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 80, 598, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 16, 672, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 40, 677, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 64, 672, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 72, 643, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_corridor_8017FE34[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b6_corridor_8017FE54[22] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -104, 1199, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -72, 1396, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -40, 1450, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -8, 1382, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 24, 1363, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -96, 1422, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -80, 1503, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -56, 1564, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -32, 1581, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -8, 1590, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 16, 1563, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -120, 745, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -80, 745, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -40, 702, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, -120, 830, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, -80, 926, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, -40, 895, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 0, 822, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 40, 799, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 0, 951, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 32, 944, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, -120, 927, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_corridor_8018000C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_corridor_8018002C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_corridor_8018003C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b6_corridor_8018004C[5] = {
    { { .empty = D_shelter_b6_corridor_8017FB68 }, D_shelter_b6_corridor_8017FB68, NULL },
    { { .elements = D_shelter_b6_corridor_8017FB78 }, D_shelter_b6_corridor_8017FE34, NULL },
    { { .elements = D_shelter_b6_corridor_8017FE54 }, D_shelter_b6_corridor_8018000C, NULL },
    { { .empty = D_shelter_b6_corridor_8018002C }, D_shelter_b6_corridor_8018002C, NULL },
    { { .empty = D_shelter_b6_corridor_8018003C }, D_shelter_b6_corridor_8018003C, NULL },
};

WorldCoordPointLight D_shelter_b6_corridor_80180088[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4600, -2750, -50 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 5000, 0x2710 },
};

WorldCoordRoomLights D_shelter_b6_corridor_801800E8 = { 0, NULL, ARRAY_SIZE(D_shelter_b6_corridor_80180088), D_shelter_b6_corridor_80180088, 0, NULL };

WorldCollisionTrigger D_shelter_b6_corridor_80180100[6] = {
    { NULL, NULL, NULL, { 4576, -2065, 96, 0 }, { { 0, -2528, 2144, 0 }, { 0, -2528, -2144, 0 }, { 0, 2528, 2144, 0 }, { 0, 2528, -2144, 0 } }, { -4112, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4384, -2177, 128, 0 }, { { 0, -2480, -2256, 0 }, { 0, -2480, 2256, 0 }, { 0, 2480, -2256, 0 }, { 0, 2480, 2256, 0 } }, { 4110, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1245, -1952, 924, 0 }, { { -571, -2528, 1161, 0 }, { 572, -2528, -1161, 0 }, { -571, 2528, 1161, 0 }, { 572, 2528, -1161, 0 } }, { -3684, 0, -1813, 0 }, { 0, 0, 4096, 0 }, 2839, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1048, -1952, 1047, 0 }, { { 611, -2480, -1198, 0 }, { -612, -2480, 1198, 0 }, { 611, 2480, -1198, 0 }, { -612, 2480, 1198, 0 } }, { 3657, 0, 1865, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1567, -2080, -1409, 0 }, { { 233, -2528, 1273, 0 }, { -232, -2528, -1273, 0 }, { 233, 2528, 1273, 0 }, { -232, 2528, -1273, 0 } }, { -4038, 0, 736, 0 }, { 0, 0, 4096, 0 }, 2839, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1439, -2016, -1409, 0 }, { { -222, -2480, -1327, 0 }, { 223, -2480, 1327, 0 }, { -222, 2480, -1327, 0 }, { 223, 2480, 1327, 0 } }, { 4050, 0, -680, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b6_corridor_801802C8[5] = {
    { 131, 505, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_350500_80168EA4 },
    { 49, 49, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101100_80147400 },
    { 52, 52, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205200_8014CA60 },
    { 60, 60, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_205200_801567C4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_shelter_b6_corridor_80180304[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017C0F0, D_shelter_b6_corridor_801802C8 },
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
    { NULL, NULL },
};

WorldCollisionTrigger D_shelter_b6_corridor_8018036C[5] = {
    { NULL, NULL, NULL, { 512, -48, 0, 0 }, { { -512, 0, -1024, 0 }, { 512, 0, -1024, 0 }, { -512, 0, 1024, 0 }, { 512, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8512, -48, 0, 0 }, { { -512, 0, -1024, 0 }, { 512, 0, -1024, 0 }, { -512, 0, 1024, 0 }, { 512, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2752, -64, 704, 0 }, { { -1056, 0, -608, 0 }, { 1056, 0, -608, 0 }, { -1056, 0, 608, 0 }, { 1056, 0, 608, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, -4096, 0 }, 1214, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, -688, 0 }, { { -1056, 0, -688, 0 }, { 1056, 0, -688, 0 }, { -1056, 0, 688, 0 }, { 1056, 0, 688, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, 4096, 0 }, 1254, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6720, -64, 688, 0 }, { { -1056, 0, -656, 0 }, { 1056, 0, -656, 0 }, { -1056, 0, 656, 0 }, { 1056, 0, 656, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, -4096, 0 }, 1241, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_b6_corridor_801804E8[6] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b6_corridor_801804E8) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 392, 437, 278, 400 } },
    { .color = { 393, 435, 277, 399 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_shelter_b6_corridor_80180518 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionFootstepSounds D_shelter_b6_corridor_80180524 = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b6_corridor_80180530[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b6_corridor_80180538[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b6_corridor_80180518 },
};

WorldCollisionSurfaceProperties D_shelter_b6_corridor_80180540[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b6_corridor_80180524 },
};

WorldCollisionSurfaceProperties* D_shelter_b6_corridor_80180548[8] = {
    D_shelter_b6_corridor_80180530,
    D_shelter_b6_corridor_80180538,
    D_shelter_b6_corridor_80180540,
    D_shelter_b6_corridor_80180530,
    D_shelter_b6_corridor_80180530,
    D_shelter_b6_corridor_80180530,
    D_shelter_b6_corridor_80180530,
    D_shelter_b6_corridor_80180530,
};

ScreenWaveCtx* gScreenWaveCtx = NULL;

// Nine active columns and one retained zero entry.
ScreenWaveGridOscillator gScreenWaveColumns[10] = { 0 };

ScreenWaveGridOscillator gScreenWaveRows[30] = { 0 };

_ShelterB6CorridorScreenWaveGrid gScreenWaveGrid = { { 0 }, { 0 } };

_ShelterB6CorridorStorage51B0 D_shelter_b6_corridor_801851B0;

s32 D_shelter_b6_corridor_801851B8;

static void _shelterB6CorridorInitRoom(Task* task);
static void _shelterB6CorridorSetImageMaskMode(Task* unusedTask);

#include "../../shared/screen_wave_grid.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Refuses key-item use in this room without changing the scene.
///
/// Message 0x13F1 carries the collected item ID and a zero second payload.
/// All arguments are ignored; returning 0 reports that the item cannot be used.
static s32 _shelterB6CorridorRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedArg)
{
    return 0;
}

s32 func_shelter_b6_corridor_8017DEB0(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u16 id;
    s32 k;

    *out = *in;
    mapNeoArkResolveRoomVariant(in, out);
    k  = in->areaId;
    id = k;
    k  = 0x19;
    if (id == 9) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd1(1);
        }
        return 0;
    }
    if (id == k) {
        return gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED;
    }
    return 1;
}

s32 func_shelter_b6_corridor_8017DF48(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 2:
            if (gameFlagGetNibble(GAME_FLAG_B6_CORRIDOR_EVE_PART_0_DOWN) != 0) {
                Gp_RunCapCmd1(5);
            } else if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                Gp_RunCapCmd1(2);
            } else {
                Gp_RunCapCmd1(8);
            }
            break;
        case 3:
            if (gameFlagGetNibble(GAME_FLAG_B6_CORRIDOR_EVE_PART_1_DOWN) != 0) {
                Gp_RunCapCmd1(6);
            } else if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                Gp_RunCapCmd1(3);
            } else {
                Gp_RunCapCmd1(9);
            }
            break;
        case 4:
            if (gameFlagGetNibble(GAME_FLAG_B6_CORRIDOR_EVE_PART_2_DOWN) != 0) {
                Gp_RunCapCmd1(7);
            } else if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
                Gp_RunCapCmd1(0xA);
            } else {
                Gp_RunCapCmd1(4);
            }
            break;
    }
    return 0;
}

/// Ignores room-action requests and returns 0 without changing the scene.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a request for this dispatch and
/// supplies a zero second payload. This room reads neither payload.
static s32 _shelterB6CorridorIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, DirectionActionRequest* unusedRequest, s32 unusedArg)
{
    return 0;
}

s32 func_shelter_b6_corridor_8017E028(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    evsStartScriptWithSkip(D_shelter_b6_corridor_8017F354, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_shelter_b6_corridor_8017F684);
    func_800E3FAC(0xA2, 0x2F);
    return 0;
}

/// Sets bit 15 in all 320-by-240 decoded background pixels, preserving their RGB bits.
///
/// `Fs_ImgBuffers` must contain the completed RGB16 frame and remain writable
/// without concurrent decoding, DMA or reuse. It updates the resident RAM
/// image for subsequent uploads. The halfword view visits both pixels of each
/// packed GPU word across the complete workspace.
static inline void _shelterB6CorridorMaskImagePixels(void)
{
    u16* pixel;
    s32  pixelIndex;

    pixel = (u16*)Fs_ImgBuffers;
    for (pixelIndex = 0; pixelIndex < (s32)(sizeof(*Fs_ImgBuffers) / sizeof(*pixel)); pixelIndex++) {
        *pixel |= FILE_SYSTEM_IMAGE_PIXEL_MASK;
        pixel++;
    }
}

/// Registers the room task, masks its loaded background and selects scene music.
///
/// State 0 requires the room's decoded image and session to be loaded. Variant
/// 1 selects scene-music entry 2 and suppresses area/ending music selection.
/// Advances to state 1 after initialization. The write-only room halfword's
/// role remains unproven.
static void _shelterB6CorridorInitRoom(Task* task)
{
    enum {
        SHELTER_B6_CORRIDOR_SCENE_MUSIC_VARIANT = 1,
        SHELTER_B6_CORRIDOR_SCENE_MUSIC_ENTRY   = 2,
    };

    task->msgTable = D_shelter_b6_corridor_8017EF24;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _shelterB6CorridorMaskImagePixels();
    D_shelter_b6_corridor_801851B0.field_0 = 2;
    if (gGameSession->location.loc.variant == SHELTER_B6_CORRIDOR_SCENE_MUSIC_VARIANT) {
        gStageSceneMusicEntry    = SHELTER_B6_CORRIDOR_SCENE_MUSIC_ENTRY;
        gGameSession->flowFlags |= GAME_SESSION_FLOW_SKIP_ENDING_MUSIC;
        gGameSession->flowFlags |= GAME_SESSION_FLOW_SKIP_AREA_MUSIC;
    }
    task->state++;
}

/// Re-arms pixel bit 15 for the next background-image MDEC decode.
///
/// The decoder resets this one-image mode after starting each decode, so the
/// room's state 1 refreshes it every tick. The task argument is unused.
static void _shelterB6CorridorSetImageMaskMode(Task* unusedTask)
{
    char stackReservation[0x10]; // Unaccessed frame storage; original contents unproven

    gCdCmdQueue.imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
}

/// The room task's three states: set the room up, the per-frame state, end.
static const TaskFuncTable3 D_shelter_b6_corridor_8017D5C4 = {
    { _shelterB6CorridorInitRoom, _shelterB6CorridorSetImageMaskMode, taskKill },
};

void shelterB6CorridorRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_shelter_b6_corridor_8017D5C4;
    states.funcs[task->state](task);
}

void func_shelter_b6_corridor_8017E19C(s32 arg0)
{
    if (!(gGameSession->flowFlags & GAME_SESSION_FLOW_REEQUIP_WEAPON)) {
        gGameSession->flowFlags                       |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        gSceneCombatState.signals.bytes.endDelayFrames = arg0;
        sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(1), 0x31);
        taskCallExit(sceneFindPlacedActor(1));
    }
}

/// Sets bit 0 of `Gp_StateC08.flags` and requests all-effect cancellation on `gRoomEffectState`.
void func_shelter_b6_corridor_8017E204(void)
{
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    roomEffectRequestCancelAll();
}

/// Draws two wall-light capsule glows from consecutive pairs of world endpoints.
///
/// Borrows four word-aligned `worldPoints`, pairing [0, 1] before [2, 3].
/// The signed low halfword of `radiusScale` gives each end's pixel radius as
/// radiusScale * 64 / (camera Z / 4). `packedColor` bits 8..11, 4..7 and 0..3
/// are RGB nibbles scaled by 16, with bit 3 inserted on odd animation frames.
/// Requires the current view transform, scratch stack, frame arena and ordering
/// table. Each capsule independently rejects negative projection flags and
/// requires nonzero endpoint depths; an accepted capsule queues six additive
/// Gouraud quads and their blend commands, retained until GPU drawing completes.
static inline void _shelterB6CorridorDrawGlowPair(const SVECTOR worldPoints[4], s32 radiusScale, s32 packedColor)
{
    _glowDrawCapsule(&worldPoints[0], radiusScale, packedColor);
    _glowDrawCapsule(&worldPoints[2], radiusScale, packedColor);
}

void shelterB6CorridorDrawViewGlowsTask(Task* task)
{
    enum {
        SHELTER_B6_CORRIDOR_GLOWS_START       = 0,
        SHELTER_B6_CORRIDOR_GLOWS_ACTIVE      = 1,
        SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE = 0x140, // Pixel radius = scale * 64 / (camera Z / 4)
        SHELTER_B6_CORRIDOR_GLOW_COLOR        = 0x442, // RGB nibbles: yellow, with odd-frame bit-3 flicker
    };
    u8 mappedViewIndex;

    if (task->state == SHELTER_B6_CORRIDOR_GLOWS_START) {
        D_shelter_b6_corridor_801851B8 = 0;
        task->state                    = SHELTER_B6_CORRIDOR_GLOWS_ACTIVE;
    }

    // Draw only the wall-light endpoint pairs visible from this camera.
    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 2:
            _shelterB6CorridorDrawGlowPair(&D_shelter_b6_corridor_8017F834[0], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            _shelterB6CorridorDrawGlowPair(&D_shelter_b6_corridor_8017F834[8], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            break;
        case 3:
            _shelterB6CorridorDrawGlowPair(&D_shelter_b6_corridor_8017F844[0], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            _glowDrawCapsule(&D_shelter_b6_corridor_8017F844[4], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            _shelterB6CorridorDrawGlowPair(&D_shelter_b6_corridor_8017F844[8], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            _glowDrawCapsule(&D_shelter_b6_corridor_8017F844[12], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            break;
        case 4:
            _glowDrawCapsule(&D_shelter_b6_corridor_8017F874[0], SHELTER_B6_CORRIDOR_GLOW_RADIUS_SCALE, SHELTER_B6_CORRIDOR_GLOW_COLOR);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

void func_shelter_b6_corridor_8017EBA4(Task* task)
{
    GfxCoord* coord;
    u8        rgb[3];
    u32       shade;

    coord = task->extra.tmd->coords + 1;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        shade  = ((gDisplayState.animFrame & 1) << 4) + 0x40;
        rgb[0] = shade;
        rgb[1] = shade;
        rgb[2] = shade >> 1;
        effectDrawGouraudDisc(coord, 0x200, rgb);
        effectDrawGouraudDisc(coord, 0x400, rgb);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & 3) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_FLASH_BURST, task->extra.tmd->coords + (((gRandomLcgState >> 16) & 0xF) + 3), 0x10080, NULL);
        }
    }
}

/// Advances the player-hit glow's radius and draws its yellow disc at a composed centre.
///
/// Borrows writable `work`: `scale` is brightness (24..192 in the live task)
/// and `angle` is the base radius numerator, increased by 24 even if culled.
/// RGB is (brightness, brightness, brightness / 2); this call does not fade it.
/// Doubled and quadrupled radii narrow to s16 before perspective scaling by
/// 64 / (camera Z / 4 + 1). `centreCoord` supplies a cached translation in the
/// current view's space; its rotation is unused. Requires the scratch stack,
/// frame arena and ordering table. Accepted projections append 24 additive
/// Gouraud quads and their blend commands, retained until GPU drawing completes.
static inline void _shelterB6CorridorDrawPlayerHitGlow(const GfxCoord* centreCoord, EffectWork* work)
{
    enum { SHELTER_B6_CORRIDOR_HIT_GLOW_RADIUS_STEP = 0x18 };
    u8  yellowRgb[3];
    u16 baseRadius;

    yellowRgb[0] = work->scale;
    yellowRgb[1] = work->scale;
    yellowRgb[2] = work->scale >> 1;
    work->angle += SHELTER_B6_CORRIDOR_HIT_GLOW_RADIUS_STEP;

    // Keep the zero-width band's packets even though its two edges coincide.
    effectDrawOuterGlowBand(centreCoord, (s16)(work->angle * 2), 0, yellowRgb);
    baseRadius = work->angle;
    effectDrawGouraudDisc(centreCoord, (s16)(baseRadius * 4), yellowRgb);
}

void shelterB6CorridorPlayerHitGlowTask(Task* task)
{
    enum {
        SHELTER_B6_CORRIDOR_HIT_GLOW_START              = 0,
        SHELTER_B6_CORRIDOR_HIT_GLOW_ACTIVE             = 1,
        SHELTER_B6_CORRIDOR_HIT_GLOW_INITIAL_BRIGHTNESS = 0xC0,
        SHELTER_B6_CORRIDOR_HIT_GLOW_INITIAL_RADIUS     = 0x200,
        SHELTER_B6_CORRIDOR_HIT_GLOW_FADE_START_TICK    = 9,
        SHELTER_B6_CORRIDOR_HIT_GLOW_BRIGHTNESS_STEP    = 0x18,
    };
    EffectWork* work;
    GfxCoord*   centreCoord;
    s16         effectControl;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    centreCoord   = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    work->age++;
    // Claim the shared generation only when this instance starts running.
    if (task->state == SHELTER_B6_CORRIDOR_HIT_GLOW_START) {
        work->scale = SHELTER_B6_CORRIDOR_HIT_GLOW_INITIAL_BRIGHTNESS;
        work->angle = SHELTER_B6_CORRIDOR_HIT_GLOW_INITIAL_RADIUS;
        D_shelter_b6_corridor_801851B8++;
        task->state           = SHELTER_B6_CORRIDOR_HIT_GLOW_ACTIVE;
        task->spawnArg1.value = D_shelter_b6_corridor_801851B8;
    }
    if (task->spawnArg1.value != D_shelter_b6_corridor_801851B8) {
        effectKillTask(work, task);
        return;
    }
    _shelterB6CorridorDrawPlayerHitGlow(centreCoord, work);
    if (work->age < SHELTER_B6_CORRIDOR_HIT_GLOW_FADE_START_TICK) {
        return;
    }
    // Draw this tick at the old brightness, then fade or retire it.
    work->scale -= SHELTER_B6_CORRIDOR_HIT_GLOW_BRIGHTNESS_STEP;
    if (work->scale < SHELTER_B6_CORRIDOR_HIT_GLOW_BRIGHTNESS_STEP) {
        effectKillTask(work, task);
    }
}

void shelterB6CorridorSetPartDestroyedSprites(u8 partSlot, u8 destroyed)
{
    const GameLocationKey* location = &gGameSession->location.loc;
    SpriteView*            views    = Gp_SprtTables[location->stage - 1]->areaViews[location->area - 1];
    SpriteBatch*           batches;
    s32                    slot = partSlot;
    s32                    destroyedState;

    // Updates the middle part in both views; captures views and batches.
    // isHidden must be a side-effect-free 0/1 value: it is evaluated twice.
#define SHELTER_B6_CORRIDOR_SET_PART_1_SPRITES_HIDDEN(isHidden) \
    do {                                                        \
        batches           = views[1].batches;                   \
        batches[2].hidden = (isHidden);                         \
        batches           = views[2].batches;                   \
        batches[2].hidden = (isHidden);                         \
    } while (0)

    // Expose each part's destroyed scenery in the views that can show it.
    if (slot == 0) {
        destroyedState = destroyed;
        if (destroyedState == SHELTER_B6_CORRIDOR_PART_INTACT) {
            batches           = views[1].batches;
            batches[1].hidden = true;
            return;
        }
        if (destroyedState == SHELTER_B6_CORRIDOR_PART_DESTROYED) {
            batches           = views[1].batches;
            batches[1].hidden = false;
            return;
        }
    } else if (slot == 1) {
        destroyedState = destroyed;
        if (destroyedState == SHELTER_B6_CORRIDOR_PART_INTACT) {
            SHELTER_B6_CORRIDOR_SET_PART_1_SPRITES_HIDDEN(true);
            return;
        }
        if (destroyedState == SHELTER_B6_CORRIDOR_PART_DESTROYED) {
            SHELTER_B6_CORRIDOR_SET_PART_1_SPRITES_HIDDEN(false);
            return;
        }
    } else if (slot == 2) {
        destroyedState = destroyed;
        if (destroyedState == SHELTER_B6_CORRIDOR_PART_INTACT) {
            batches           = views[2].batches;
            batches[1].hidden = true;
            return;
        }
        if (destroyedState == SHELTER_B6_CORRIDOR_PART_DESTROYED) {
            batches           = views[2].batches;
            batches[1].hidden = false;
        }
    }
#undef SHELTER_B6_CORRIDOR_SET_PART_1_SPRITES_HIDDEN
}
