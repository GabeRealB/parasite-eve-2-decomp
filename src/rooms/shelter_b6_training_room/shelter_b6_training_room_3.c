#include "rooms/shelter_b6_training_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b6_training_room_private.h"

#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
#include "../../shared/streamed_scene.h"
/// Signed effect-age argument for the repeating six-cell sprite strip.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"

/// Three shapes for this room's ring bands, with the halfword in the gap after them.
///
/// The rows end two bytes short of the next word-aligned object. That gap is
/// nonzero, so it is a member rather than compiler padding. Code reads the
/// rows; nothing reads the halfword, and its value is unproven.
typedef struct {
    EffectBandShape entries[3];    // One shape per ring band; spawn arguments 0, 1 and 2
    u16             alignmentFill; // Gap before the next word-aligned object; never read
} _ShelterB6TrainingRoomBandShapeStorage;
STATIC_ASSERT_SIZEOF(_ShelterB6TrainingRoomBandShapeStorage, 0x14);
extern _ShelterB6TrainingRoomBandShapeStorage D_shelter_b6_training_room_80184404;

/// Number of vertices in each ring of a `_ShelterB6TrainingRoomBandScratch`, and
/// so the number of quads in one ring band. Not a power of two: the drawer wraps
/// the next vertex index with `%`. Vertices sit this fraction of a full turn
/// apart, a turn being `ONE` angle units, rounded down.
#define SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT 6

/// Scratch-stack workspace for drawing one of this room's ring bands.
///
/// The six-segment counterpart of `EffectBandScratch`. The drawer places one
/// vertex of each ring at every sixth of a turn in the effect coordinate's
/// local frame, rotates it by that coordinate's world matrix and adds its
/// translation, storing the result back as a world position narrowed to 16
/// bits. `topRing` is the wider ring, displaced by the band's lift along local
/// -Y; `bottomRing` is the narrower ring and stays in the local XZ plane. Quad
/// `i` takes vertices 0 and 1 from `topRing[i]` and `topRing[i + 1]`, and
/// vertices 2 and 3 from `bottomRing[i]` and `bottomRing[i + 1]`, wrapping at
/// the last segment.
///
/// Reserve one complete block on the scratch stack and release it in reverse
/// order after drawing. Pointers into the block must not survive its release.
typedef struct {
    SVECTOR topRing[SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT];    // Wider ring, lifted along local -Y; quad vertices 0 and 1
    SVECTOR bottomRing[SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT]; // Narrower ring in the local XZ plane; quad vertices 2 and 3
    s32     otz;                                                     // Ordering-table depth: SZ3 / 4 of the quad's last vertex
    s32     projectionFlags;                                         // GTE FLAG word after the segment's RTPT; bit 31 makes it negative and drops the quad
    DVECTOR sxy0;                                                    // Screen position of the current quad's vertex 0
    DVECTOR sxy1;                                                    // Screen position of vertex 1
    DVECTOR sxy2;                                                    // Screen position of vertex 2
    DVECTOR sxy3;                                                    // Screen position of vertex 3
} _ShelterB6TrainingRoomBandScratch;
STATIC_ASSERT_SIZEOF(_ShelterB6TrainingRoomBandScratch, 0x78);

extern SVECTOR D_shelter_b6_training_room_80184334[];
extern u16     D_shelter_b6_training_room_801843FC[];

static void func_shelter_b6_training_room_80180530(GfxCoord* from, GfxCoord* to, s16 size, u16 color);
static void func_shelter_b6_training_room_80181368(EffectWork* mem, GfxCoord* coord, s32 band);
static void func_shelter_b6_training_room_80181FDC(GfxCoord* arg0, GfxCoord* arg1, s32 arg2, s16 arg3);

TaskMessageEntry D_shelter_b6_training_room_80182AF4[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b6_training_room_8017D640 },
    { 5105, func_shelter_b6_training_room_8017D638 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b6_training_room_8017D75C },
    { ROOM_MESSAGE_COMMAND, func_shelter_b6_training_room_8017D684 },
    { ROOM_MESSAGE_ACTOR_EVENT, func_shelter_b6_training_room_8017D764 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s32 D_shelter_b6_training_room_80182B24 = 0x11805;

static AnimationPackedPose _gShelterB6TrainingRoomAnimation05844Bank1[6] = {
#include "assets/shelter_b6_training_room_animation_05844_bank1.inc"
};

static AnimationPackedRotation _gShelterB6TrainingRoomAnimation05844Bank4[46] = {
#include "assets/shelter_b6_training_room_animation_05844_bank4.inc"
};

static AnimationRecord _gShelterB6TrainingRoomAnimation05844Records[109] = {
#include "assets/shelter_b6_training_room_animation_05844_records.inc"
};

static u16 _gShelterB6TrainingRoomAnimation05844Indices[20] = {
#include "assets/shelter_b6_training_room_animation_05844_indices.inc"
};

static AnimationSet _gShelterB6TrainingRoomAnimation05844 = {
    _gShelterB6TrainingRoomAnimation05844Records,
    _gShelterB6TrainingRoomAnimation05844Indices,
    { NULL, _gShelterB6TrainingRoomAnimation05844Bank1, NULL, NULL, _gShelterB6TrainingRoomAnimation05844Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB6TrainingRoomAnimation05F74Bank1[13] = {
#include "assets/shelter_b6_training_room_animation_05F74_bank1.inc"
};

static AnimationPackedRotation _gShelterB6TrainingRoomAnimation05F74Bank4[171] = {
#include "assets/shelter_b6_training_room_animation_05F74_bank4.inc"
};

static AnimationRecord _gShelterB6TrainingRoomAnimation05F74Records[230] = {
#include "assets/shelter_b6_training_room_animation_05F74_records.inc"
};

static u16 _gShelterB6TrainingRoomAnimation05F74Indices[20] = {
#include "assets/shelter_b6_training_room_animation_05F74_indices.inc"
};

static AnimationSet _gShelterB6TrainingRoomAnimation05F74 = {
    _gShelterB6TrainingRoomAnimation05F74Records,
    _gShelterB6TrainingRoomAnimation05F74Indices,
    { NULL, _gShelterB6TrainingRoomAnimation05F74Bank1, NULL, NULL, _gShelterB6TrainingRoomAnimation05F74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB6TrainingRoomAnimation061B8Bank1[3] = {
#include "assets/shelter_b6_training_room_animation_061B8_bank1.inc"
};

static AnimationPackedRotation _gShelterB6TrainingRoomAnimation061B8Bank4[28] = {
#include "assets/shelter_b6_training_room_animation_061B8_bank4.inc"
};

static AnimationRecord _gShelterB6TrainingRoomAnimation061B8Records[88] = {
#include "assets/shelter_b6_training_room_animation_061B8_records.inc"
};

static u16 _gShelterB6TrainingRoomAnimation061B8Indices[20] = {
#include "assets/shelter_b6_training_room_animation_061B8_indices.inc"
};

static AnimationSet _gShelterB6TrainingRoomAnimation061B8 = {
    _gShelterB6TrainingRoomAnimation061B8Records,
    _gShelterB6TrainingRoomAnimation061B8Indices,
    { NULL, _gShelterB6TrainingRoomAnimation061B8Bank1, NULL, NULL, _gShelterB6TrainingRoomAnimation061B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB6TrainingRoomAnimation063C0Bank1[2] = {
#include "assets/shelter_b6_training_room_animation_063C0_bank1.inc"
};

static AnimationPackedRotation _gShelterB6TrainingRoomAnimation063C0Bank4[29] = {
#include "assets/shelter_b6_training_room_animation_063C0_bank4.inc"
};

static AnimationRecord _gShelterB6TrainingRoomAnimation063C0Records[75] = {
#include "assets/shelter_b6_training_room_animation_063C0_records.inc"
};

static u16 _gShelterB6TrainingRoomAnimation063C0Indices[20] = {
#include "assets/shelter_b6_training_room_animation_063C0_indices.inc"
};

static AnimationSet _gShelterB6TrainingRoomAnimation063C0 = {
    _gShelterB6TrainingRoomAnimation063C0Records,
    _gShelterB6TrainingRoomAnimation063C0Indices,
    { NULL, _gShelterB6TrainingRoomAnimation063C0Bank1, NULL, NULL, _gShelterB6TrainingRoomAnimation063C0Bank4, NULL, NULL, NULL },
};

TaskDesc D_shelter_b6_training_room_801839A8 = { { { TASK_BODY_NONE, 96 } }, func_shelter_b6_training_room_8017D9C8, { .value = 0 } };

AnimationSet* D_shelter_b6_training_room_801839B4[5] = {
    NULL,
    &_gShelterB6TrainingRoomAnimation05844,
    &_gShelterB6TrainingRoomAnimation05F74,
    &_gShelterB6TrainingRoomAnimation061B8,
    &_gShelterB6TrainingRoomAnimation063C0,
};

AnimationBankCopyRequest D_shelter_b6_training_room_801839C8 = { { .sets = D_shelter_b6_training_room_801839B4 }, ARRAY_SIZE(D_shelter_b6_training_room_801839B4) };

AnimationPlayRequest D_shelter_b6_training_room_801839D0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_training_room_801839E4 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_training_room_801839F8 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183A0C = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183A20 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_shelter_b6_training_room_80183A34 = { { 2300, 0, 8900, 0 }, { 0, 910, 0, 0 } };

ActorTransform D_shelter_b6_training_room_80183A4C = { { 2300, 0, 9000, 0 }, { 0, 1707, 0, 0 } };

AnimationPlayRequest D_shelter_b6_training_room_80183A64 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183A78 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183A8C = { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183AA0 = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183AB4 = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b6_training_room_80183AC8 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_shelter_b6_training_room_80183ADC = { { 2850, 0, 8050, 0 }, { 0, -228, 0, 0 } };

AnimationPlayRequest D_shelter_b6_training_room_80183AF4[5] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE },
};

ActorTransform D_shelter_b6_training_room_80183B58 = { { 3250, 0, 9500, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_shelter_b6_training_room_80183B70 = { { 3250, 0, 9500, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_shelter_b6_training_room_80183B88 = { { 5000, 0, 0x2710, 0 }, { 0, 1024, 0, 0 } };

ActorCommand D_shelter_b6_training_room_80183BA0 = { { .loc = { 5, 24 } }, 1 };

ActorCommand D_shelter_b6_training_room_80183BA4 = { { .loc = { 5, 24 } }, 2 };

ActorMotionWalkAnim D_shelter_b6_training_room_80183BA8 = { .animationId = 3, .nextAnimId = 4 };

ActorCommand D_shelter_b6_training_room_80183BB0 = { { .loc = { 5, 24 } }, 1 };

EvsCommand D_shelter_b6_training_room_80183BB4[58] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_training_room_8017DAF8 }, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_shelter_b6_training_room_801839C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_training_room_801839E4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_training_room_8017DB70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = D_shelter_b6_training_room_80184124 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b6_training_room_80183A34 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_shelter_b6_training_room_80183B58 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_shelter_b6_training_room_80183ADC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 4 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_training_room_8017D940 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_training_room_801839F8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b6_training_room_80183A78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_training_room_80183BA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_shelter_b6_training_room_80183B70 } }, { .message = { .pointer = &D_shelter_b6_training_room_80183BA8 } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55190001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_shelter_b6_training_room_80183B88 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b6_training_room_80183A4C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_training_room_80183A0C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b6_training_room_80183A8C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55190004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b6_training_room_80183AA0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b6_training_room_80183A8C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_training_room_80183A20 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55190002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_training_room_8017D974 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b6_training_room_80183AB4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b6_training_room_80183AC8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_training_room_8017D974 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b6_training_room_80184124[14] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_training_room_8017DAF8 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_shelter_b6_training_room_801839C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_training_room_801839E4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_training_room_8017DB70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b6_training_room_80183A4C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_training_room_8017D974 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b6_training_room_80184274[7] = {
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55190005 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_training_room_8017DAC8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_training_room_8017DB28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_shelter_b6_training_room_8018431C[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b6_training_room_8017DD98, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, streamedScenePlayThenHold, { .value = 0 } },
};

SVECTOR D_shelter_b6_training_room_80184334[25] = {
    { 260, -1440, 0x283C, 0 },
    { 565, -1405, 0x283C, 0 },
    { 910, -1470, 0x283C, 0 },
    { 910, -1240, 0x283C, 0 },
    { 1180, -1470, 0x283C, 0 },
    { 1180, -1240, 0x283C, 0 },
    { 1765, -1440, 0x283C, 0 },
    { 2065, -1410, 0x283C, 0 },
    { 4950, -2250, 9750, 0 },
    { 4950, -2250, 9250, 0 },
    { 50, -2250, 1750, 0 },
    { 50, -2250, 1250, 0 },
    { 350, -4400, 9750, 0 },
    { 350, -4400, 7750, 0 },
    { 350, -4400, 6250, 0 },
    { 350, -4400, 4250, 0 },
    { 350, -4400, 2750, 0 },
    { 350, -4400, 750, 0 },
    { 4650, -4400, 9750, 0 },
    { 4650, -4400, 7750, 0 },
    { 4650, -4400, 6250, 0 },
    { 4650, -4400, 4250, 0 },
    { 4650, -4400, 2750, 0 },
    { 4650, -4400, 750, 0 },
    { 2500, -1000, 7750, 0 },
};

u16 D_shelter_b6_training_room_801843FC[4] = {
    258,
    532,
    1064,
    1596,
};

_ShelterB6TrainingRoomBandShapeStorage D_shelter_b6_training_room_80184404 = { { { 256, 2048, 512 }, { 512, 1536, 768 }, { 768, 1024, 1024 } }, 0xF23F };

void func_shelter_b6_training_room_8017DDE8(Task* task)
{
    s32 i;
    s32 j;

    if (task->state == 0) {
        D_shelter_b6_training_room_80185C98 = 0;
        for (j = 0; j < 3; j++) {
            for (i = 0; i < 6; i++) {
                D_shelter_b6_training_room_80185C60[task->spawnArg1.value][i] = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
            }
        }
        task->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            glowDrawCapsule(&D_shelter_b6_training_room_80184334[10], 0x180, 0x210);
            break;
        case 3:
            glowDrawCapsule(&D_shelter_b6_training_room_80184334[10], 0x180, 0x210);
            glowDrawCapsule(&D_shelter_b6_training_room_80184334[8], 0x180, 0x210);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[12], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[13], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[14], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[15], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[16], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[18], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[19], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[20], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[21], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[22], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 4:
            glowDrawCapsule(&D_shelter_b6_training_room_80184334[8], 0x180, 0x210);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[12], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[13], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[14], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[18], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[19], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[20], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 5:
            glowDrawCapsule(&D_shelter_b6_training_room_80184334[8], 0x180, 0x210);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 6:
            glowDrawDisc(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 7:
            glowDrawCapsule(&D_shelter_b6_training_room_80184334[10], 0x180, 0x210);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[17], 0x280, 0x444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[23], 0x280, 0x444);
            break;
        case 8:
            glowDrawDisc(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"

void func_shelter_b6_training_room_8017EE70(Task* arg0)
{
    u8          rgb[3];
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s16         step;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x100;
            mem->period = 0xE0;
            mem->step   = 0x100;
            arg0->state = 1;
        }
        rgb[0]     = mem->scale;
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        Gp_DrawRing(coord, (s16)(step * 2), rgb);
        RoomFx_DrawBurstGlow(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            Gp_DrawArc(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x80;
            return;
        }
        mem->scale -= 0x10;
        if (mem->scale < 0x10) {
        kill:
            effectKillTask(mem, arg0);
        }
    }
}

#include "../../shared/room_visual_effects.inc.c"
#include "../../shared/room_visual_effects_glow_quad.inc.c"

void func_shelter_b6_training_room_8017F8B8(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        work->age++;
        switch (task->state) {
            case 0:
                task->state                         = 1;
                work->scale                         = 0;
                work->angle                         = 0x100;
                D_shelter_b6_training_room_80185C90 = NULL;
                work->step                          = 0x80 / task->spawnArg1.value;
            case 1:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    rgb[0] = work->scale >> 1;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, (s16)((task->spawnArg1.value << 5) + 0x300), 0x100, rgb);
                    break;
                }
                work->scale += work->step;
                work->angle += work->step << 3;
                task->spawnArg1.value--;
                rgb[0] = work->scale >> 1;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawArc(coord, (s16)((task->spawnArg1.value << 5) + 0x300), 0x100, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale                         = 0xFF;
                    task->state                         = 2;
                    work->period                        = 0x600;
                    work->step                          = 0;
                    D_shelter_b6_training_room_80185C90 = coord;
                }
                break;
            case 2:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    rgb[0] = work->scale >> 1;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, 0x300, 0x100, rgb);
                    break;
                }
                if (work->scale >= 9) {
                    rgb[0] = work->scale >> 1;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, 0x300, 0x100, rgb);
                    work->scale -= 8;
                    break;
                }
                task->state = 3;
                break;
            case 3:
                break;
            case 4:
                effectKillTask(work, task);
                break;
        }
    } else {
        effectKillTask(work, task);
    }
}

/// Draws a glowing capsule from the anchor coordinate
/// `D_shelter_b6_training_room_80185C90` to `coord`, doing nothing while no
/// anchor is set. Both world positions are projected, and nothing is drawn
/// unless both land on-screen. The capsule is drawn in two passes whose end
/// radii are `size * pass * 64 / otz`, each followed by the matching ground
/// capsule from `func_shelter_b6_training_room_80180530`. The fill colour comes
/// from the 4-bit-per-channel palette entry `color`, scaled by 16 and
/// brightened on alternate fields; the outer vertices are black.
void func_shelter_b6_training_room_8017FC40(GfxCoord* coord, s16 size, u16 color)
{
    RoomBeamScratch* block;
    POLY_G4*         prim;
    s32              pass;
    u8               r;
    u8               g;
    u8               b;
    s32              blend;
    s32              scaled;
    s32              limit;
    s32              angStart;
    s32              ang;
    s32              next;
    s32              mid;
    s32              tr;
    s32              tg;

    if (D_shelter_b6_training_room_80185C90 == NULL) {
        return;
    }
    block            = SCRATCH_STACK_RESERVE_BLOCK(RoomBeamScratch);
    block->point0.vx = D_shelter_b6_training_room_80185C90->workm.t[0];
    block->point0.vy = D_shelter_b6_training_room_80185C90->workm.t[1];
    block->point0.vz = D_shelter_b6_training_room_80185C90->workm.t[2];
    block->point1.vx = coord->workm.t[0];
    block->point1.vy = coord->workm.t[1];
    block->point1.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->point0);
    gte_rtps();
    gte_stsxy(&block->pair.sx0);
    gte_stflg(&block->pair.flag);
    if (block->pair.flag >= 0) {
        gte_stszotz(&block->pair.otz0);
        gte_ldv0(&block->point1);
        gte_rtps();
        gte_stsxy(&block->pair.sx1);
        gte_stflg(&block->pair.flag);
        gte_stszotz(&block->pair.otz1);
        if (block->pair.flag >= 0) {
            color = D_shelter_b6_training_room_801843FC[color];
            tr    = ((color >> 8) & 0xF) << 4;
            tg    = ((color >> 4) & 0xF) << 4;
            blend = ((u8)gDisplayState.animFrame & 1) << 4;
            r     = tr + blend;
            g     = tg + blend;
            b     = ((color & 0xF) << 4) + blend;
            for (pass = 1; pass < 3; pass++) {
                scaled              = size * (pass << 6);
                block->pair.radius0 = scaled / block->pair.otz0;
                block->pair.radius1 = scaled / block->pair.otz1;
                ang                 = (s16)ratan2((s16)block->pair.sy1 - (s16)block->pair.sy0, (s16)block->pair.sx0 - (s16)block->pair.sx1);
                if (ang < ang + 0x800) {
                    angStart = ang;
                    limit    = ang + 0x800;
                    do {
                        prim           = gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->pair.sx1 + ((block->pair.radius1 * rsin(ang + 0x800)) >> 12);
                        prim->y0 = block->pair.sy1 + ((block->pair.radius1 * rcos(ang + 0x800)) >> 12);
                        prim->x1 = block->pair.sx1 + ((block->pair.radius1 * rsin(ang + 0xA00)) >> 12);
                        prim->y1 = block->pair.sy1 + ((block->pair.radius1 * rcos(ang + 0xA00)) >> 12);
                        prim->x2 = block->pair.sx1;
                        prim->y2 = block->pair.sy1;
                        prim->x3 = block->pair.sx1 + ((block->pair.radius1 * rsin(ang + 0xC00)) >> 12);
                        prim->y3 = block->pair.sy1 + ((block->pair.radius1 * rcos(ang + 0xC00)) >> 12);
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->pair.otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                prim);
                        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->pair.otz1);

                        prim           = gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->pair.sx0 + ((block->pair.radius0 * rsin(ang)) >> 12);
                        prim->y0 = block->pair.sy0 + ((block->pair.radius0 * rcos(ang)) >> 12);
                        prim->x1 = block->pair.sx0 + ((block->pair.radius0 * rsin(ang + 0x200)) >> 12);
                        prim->y1 = block->pair.sy0 + ((block->pair.radius0 * rcos(ang + 0x200)) >> 12);
                        next     = ang + 0x400;
                        prim->x2 = block->pair.sx0;
                        prim->y2 = block->pair.sy0;
                        prim->x3 = block->pair.sx0 + ((block->pair.radius0 * rsin(next)) >> 12);
                        prim->y3 = block->pair.sy0 + ((block->pair.radius0 * rcos(next)) >> 12);
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->pair.otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                prim);
                        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->pair.otz0);

                        prim           = gGpuPrimCursor;
                        mid            = angStart + (ang - angStart) * 2;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, r, g, b);
                        prim->x0 = block->pair.sx0 + ((block->pair.radius0 * rsin(mid)) >> 12);
                        prim->y0 = block->pair.sy0 + ((block->pair.radius0 * rcos(mid)) >> 12);
                        prim->x1 = block->pair.sx1 + ((block->pair.radius1 * rsin(mid)) >> 12);
                        prim->y1 = block->pair.sy1 + ((block->pair.radius1 * rcos(mid)) >> 12);
                        prim->x2 = block->pair.sx0;
                        prim->y2 = block->pair.sy0;
                        prim->x3 = block->pair.sx1;
                        prim->y3 = block->pair.sy1;
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->pair.otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                prim);
                        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->pair.otz1);
                        ang = next;
                    } while (ang < limit);
                }
                func_shelter_b6_training_room_80180530(D_shelter_b6_training_room_80185C90, coord, size, color);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomBeamScratch);
}

/// Draws a glowing capsule between the ground points under `from` and `to`.
/// Both coordinates are traced down to the floor, and nothing is drawn unless
/// both traces land and both points project on-screen. Each end gets a
/// half-disc of radius `size * 64 / otz`, built from two gouraud quarter
/// fans, and one quad per half joins the two discs. The fill colour comes from
/// the 4-bit-per-channel palette entry `color`, doubled and brightened on
/// alternate fields; the outer vertices are black, so the glow fades outward.
static void func_shelter_b6_training_room_80180530(GfxCoord* from, GfxCoord* to, s16 size, u16 color)
{
    GfxCoord         c0;
    GfxCoord         c1;
    RoomBeamScratch* block;
    POLY_G4*         prim;
    u8               r;
    u8               g;
    u8               b;
    s32              blend;
    s32              scaled;
    s32              limit;
    s32              angStart;
    s32              ang;
    s32              next;
    s32              mid;
    DisplayState*    ds;

    if (Gp_TraceGroundCoord(from, &c0) != 1 || Gp_TraceGroundCoord(to, &c1) != 1) {
        return;
    }
    block            = SCRATCH_STACK_RESERVE_BLOCK(RoomBeamScratch);
    block->point0.vx = c0.workm.t[0];
    block->point0.vy = c0.workm.t[1];
    block->point0.vz = c0.workm.t[2];
    block->point1.vx = c1.workm.t[0];
    block->point1.vy = c1.workm.t[1];
    block->point1.vz = c1.workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->point0);
    gte_rtps();
    gte_stsxy(&block->pair.sx0);
    gte_stflg(&block->pair.flag);
    if (block->pair.flag >= 0) {
        gte_stszotz(&block->pair.otz0);
        gte_ldv0(&block->point1);
        gte_rtps();
        gte_stsxy(&block->pair.sx1);
        gte_stflg(&block->pair.flag);
        gte_stszotz(&block->pair.otz1);
        if (block->pair.flag >= 0) {
            color               = D_shelter_b6_training_room_801843FC[color];
            r                   = ((color >> 8) & 0xF) << 1;
            g                   = ((color >> 4) & 0xF) << 1;
            b                   = (color & 0xF) << 1;
            ds                  = &gDisplayState;
            blend               = ((u8)ds->animFrame & 1) << 1;
            r                  += blend;
            g                  += blend;
            b                  += blend;
            scaled              = size << 6;
            block->pair.radius0 = scaled / block->pair.otz0;
            block->pair.radius1 = scaled / block->pair.otz1;
            ang                 = (s16)ratan2((s16)block->pair.sy1 - (s16)block->pair.sy0, (s16)block->pair.sx0 - (s16)block->pair.sx1);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->pair.sx1 + ((block->pair.radius1 * rsin(ang + 0x800)) >> 12);
                    prim->y0 = block->pair.sy1 + ((block->pair.radius1 * rcos(ang + 0x800)) >> 12);
                    prim->x1 = block->pair.sx1 + ((block->pair.radius1 * rsin(ang + 0xA00)) >> 12);
                    prim->y1 = block->pair.sy1 + ((block->pair.radius1 * rcos(ang + 0xA00)) >> 12);
                    prim->x2 = block->pair.sx1;
                    prim->y2 = block->pair.sy1;
                    prim->x3 = block->pair.sx1 + ((block->pair.radius1 * rsin(ang + 0xC00)) >> 12);
                    prim->y3 = block->pair.sy1 + ((block->pair.radius1 * rcos(ang + 0xC00)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->pair.otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->pair.otz1);

                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->pair.sx0 + ((block->pair.radius0 * rsin(ang)) >> 12);
                    prim->y0 = block->pair.sy0 + ((block->pair.radius0 * rcos(ang)) >> 12);
                    prim->x1 = block->pair.sx0 + ((block->pair.radius0 * rsin(ang + 0x200)) >> 12);
                    prim->y1 = block->pair.sy0 + ((block->pair.radius0 * rcos(ang + 0x200)) >> 12);
                    next     = ang + 0x400;
                    prim->x2 = block->pair.sx0;
                    prim->y2 = block->pair.sy0;
                    prim->x3 = block->pair.sx0 + ((block->pair.radius0 * rsin(next)) >> 12);
                    prim->y3 = block->pair.sy0 + ((block->pair.radius0 * rcos(next)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->pair.otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->pair.otz0);

                    prim           = gGpuPrimCursor;
                    mid            = angStart + (ang - angStart) * 2;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->pair.sx0 + ((block->pair.radius0 * rsin(mid)) >> 12);
                    prim->y0 = block->pair.sy0 + ((block->pair.radius0 * rcos(mid)) >> 12);
                    prim->x1 = block->pair.sx1 + ((block->pair.radius1 * rsin(mid)) >> 12);
                    prim->y1 = block->pair.sy1 + ((block->pair.radius1 * rcos(mid)) >> 12);
                    prim->x2 = block->pair.sx0;
                    prim->y2 = block->pair.sy0;
                    prim->x3 = block->pair.sx1;
                    prim->y3 = block->pair.sy1;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->pair.otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->pair.otz1);
                    ang = next;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomBeamScratch);
}

void func_shelter_b6_training_room_80180DB4(Task* task)
{
    EffectWork*       work;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    u8                rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        work->age++;
        switch (task->state) {
            case 0:
                rot                 = (GfxRotationWords*)&coord->coord;
                rot->m00M01         = ONE;
                rot->m02M10         = 0;
                rot->m11M12         = ONE;
                rot->m20M21         = 0;
                rot->m22            = ONE;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->scale         = 0;
                work->angle         = 0x100;
                work->step          = 0xC0 / task->spawnArg1.value;
                if (work->step == 0) {
                    work->step = 1;
                }
                task->state = 1;
            case 1:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 1;
                    rgb[2] = work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, (s16)((task->spawnArg1.value % 15) * (work->scale << 2)), 0x100, rgb);
                    return;
                }
                work->scale += work->step;
                if (work->scale > 0xC0) {
                    work->scale = 0xC0;
                }
                work->angle = (u16)work->scale * 8 + 0x100;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 1;
                rgb[2] = work->scale >> 2;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawArc(coord, (s16)((task->spawnArg1.value % 15) * (work->scale << 2)), 0x100, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    Gp_SpawnEff(EFFECT_SHELTER_B6_TRAINING_ROOM_RING_WALL, coord, 0, NULL);
                    Gp_SpawnEff(EFFECT_SHELTER_B6_TRAINING_ROOM_RING_WALL, coord, 1, NULL);
                    Gp_SpawnEff(EFFECT_SHELTER_B6_TRAINING_ROOM_RING_WALL, coord, 2, NULL);
                    work->period = 0x600;
                    work->step   = 0;
                }
                return;
            case 2:
                if (work->scale >= 5) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 1;
                    rgb[2] = work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                        work->scale -= 4;
                    }
                    Gp_DrawFadeQuad(rgb, 1);
                    return;
                }
                break;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

void func_shelter_b6_training_room_801811AC(Task* task)
{
    EffectWork* mem;
    GfxCoord*   coord;

    mem   = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1.value);
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }
    mem->age++;
    switch (task->state) {
        case 0:
            mem->scale          = 0x80;
            task->state         = task->spawnArg1.value + 1;
            coord->coord.t[1]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            return;
        case 1:
            if (mem->scale < 5) {
                goto release;
            }
            if (mem->period < 0xC00) {
                mem->period += 0xC0;
            } else {
                mem->scale -= 4;
            }
            mem->angle += 0x20;
            mem->step  += 0x18;
            func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1.value);
            return;
        case 2:
            if (mem->scale < 4) {
                goto release;
            }
            mem->scale -= 3;
            mem->angle += 0x40;
            mem->step  += 0x18;
            func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1.value);
            return;
        case 3:
            if (mem->scale < 5) {
                goto release;
            }
            mem->scale -= 4;
            mem->angle += 0x180;
            mem->step  += 0x18;
            func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1.value);
            return;
        case 4:
        release:
            effectKillTask(mem, task);
        default:
            return;
    }
}

/// Draws band `band` of a six-sided textured ring around `coord`: six
/// `POLY_FT4` quads joining a ground rim of radius `angle + baseRadius` to a rim
/// raised by `period + lift` and widened by `step + spread`. Both rims are
/// rotated by the coordinate's `workm`, translated by its `t[]` and projected
/// through `GsWSMATRIX`. Quad `i` takes its texture column from
/// `(D_shelter_b6_training_room_80185C60[band][i] + age) % 6`, so each quad
/// animates on its own phase, and `scale` sets its brightness.
static void func_shelter_b6_training_room_80181368(EffectWork* mem, GfxCoord* coord, s32 band)
{
    _ShelterB6TrainingRoomBandScratch* block;
    SVECTOR*                           bottomVertex;
    POLY_FT4*                          prim;
    EffectBandShape*                   shape;
    s32                                i;
    s32                                next;
    s32                                ang;
    s32                                u;
    s16                                frame;
    s16                                rTop;
    s16                                rBase;
    u16                                height;
    u16                                period;

    shape  = &D_shelter_b6_training_room_80184404.entries[band];
    period = mem->period;
    rBase  = mem->angle;
    height = period + shape->lift;
    rBase += shape->baseRadius;
    rTop   = rBase + mem->step + shape->spread;
    block  = SCRATCH_STACK_RESERVE_BLOCK(_ShelterB6TrainingRoomBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT; i++) {
        ang                  = i * (ONE / SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT);
        block->topRing[i].vx = (rsin(ang) * rTop) >> 12;
        block->topRing[i].vy = -height;
        block->topRing[i].vz = (rcos(ang) * rTop) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->topRing[i]);
        gte_rtv0();
        gte_stsv(&block->topRing[i]);
        block->topRing[i].vx    = (u16)block->topRing[i].vx + (u16)coord->workm.t[0];
        block->topRing[i].vy    = (u16)block->topRing[i].vy + (u16)coord->workm.t[1];
        block->topRing[i].vz    = (u16)block->topRing[i].vz + (u16)coord->workm.t[2];
        block->bottomRing[i].vx = (rsin(ang) * rBase) >> 12;
        bottomVertex            = &block->topRing[i] + SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT;
        bottomVertex->vy        = 0;
        bottomVertex->vz        = (rcos(ang) * rBase) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->bottomRing[i]);
        gte_rtv0();
        gte_stsv(&block->bottomRing[i]);
        block->bottomRing[i].vx = (u16)block->bottomRing[i].vx + (u16)coord->workm.t[0];
        bottomVertex->vy        = (u16)bottomVertex->vy + (u16)coord->workm.t[1];
        bottomVertex->vz        = (u16)bottomVertex->vz + (u16)coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT; i++) {
        gte_ldv0(&block->topRing[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = i + 1;
        gte_ldv3(&block->topRing[next % SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT], &block->bottomRing[i], &block->bottomRing[next % SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT]);
        gte_rtpt();
        frame = ((s8)D_shelter_b6_training_room_80185C60[band][i] + mem->age) % 6;
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->otz);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            prim->code = 0x2E;
            setRGB0(prim, mem->scale, mem->scale, mem->scale);
            prim->tpage = 0x2A;
            prim->clut  = 0x4282;
            u           = frame * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = (u16)block->sxy0.vx;
            prim->y0 = (u16)block->sxy0.vy;
            prim->x1 = (u16)block->sxy1.vx;
            prim->y1 = (u16)block->sxy1.vy;
            prim->x2 = (u16)block->sxy2.vx;
            prim->y2 = (u16)block->sxy2.vy;
            prim->x3 = (u16)block->sxy3.vx;
            prim->y3 = (u16)block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_ShelterB6TrainingRoomBandScratch);
}

void func_shelter_b6_training_room_80181930(Task* task)
{
    GfxCoord* coord;
    u8        rgb[3];
    u32       shade;

    coord = task->extra.tmd->coords + 1;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        D_shelter_b6_training_room_80185C94 = coord;
        shade                               = ((gDisplayState.animFrame & 1) << 4) + 0x40;
        rgb[0]                              = shade;
        rgb[1]                              = shade;
        rgb[2]                              = shade >> 1;
        Gp_DrawRing(coord, 0x200, rgb);
        Gp_DrawRing(coord, 0x400, rgb);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & 3) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Gp_SpawnEff(EFFECT_FLASH_BURST, task->extra.tmd->coords + (((gRandomLcgState >> 16) & 0xF) + 3), 0x10080, NULL);
        }
    }
}

void func_shelter_b6_training_room_80181A3C(Task* task)
{
    EffectWork* mem;
    GfxCoord*   coord;

    mem   = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        mem->age++;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (task->state == 0) {
            GfxRotationWords* rot;
            u32               first;

            rot                 = (GfxRotationWords*)&coord->coord;
            coord->parent       = mem->parent;
            rot->m00M01         = ONE;
            rot->m02M10         = 0;
            rot->m11M12         = ONE;
            rot->m20M21         = 0;
            rot->m22            = ONE;
            coord->coord.t[0]   = mem->pos.vx;
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            first               = gRandomLcgState;
            coord->coord.t[1]   = mem->pos.vy;
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            coord->coord.t[2]   = mem->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            mem->scale          = ((first >> 16) & 0x1FF) + 0x100;
            mem->angle          = (gRandomLcgState >> 16) & 0xFFF;
            mem->period         = ((gRandomLcgState >> 16) & 0xF) + 6;
            task->state         = 1;
        }
        spriteQuadDraw(coord, mem->age, mem->scale, mem->angle);
        if (mem->age & 1) {
            func_shelter_b6_training_room_80181FDC(coord, D_shelter_b6_training_room_80185C94, mem->age >> 1, mem->scale);
        }
        if (mem->age > mem->period) {
            effectKillTask(mem, task);
        }
    }
}

/// Texel width and horizontal stride of each cell in the training effect's six-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Number of cells in the training effect's texture row, repeated as the effect ages.
#define SPRITE_QUAD_CELLS_PER_ROW 6
/// Inclusive top texel row of the training effect strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x38
#define SPRITE_QUAD_V1    0x5F
/// Perspective-sizing multiplier for the training effect sprite.
///
/// Uses the cell's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
/// Energy-arc sprite palette: VRAM X=144 words, Y=267 scanlines.
#define SPRITE_QUAD_CLUT     getClut(144, 267)
#define SPRITE_QUAD_OTZ_BIAS 0
#include "../../shared/sprite_quad_draw.inc.c"

/// Draws a textured `POLY_FT4` strip between the world positions of two
/// coordinates. Both ends are projected and the strip is dropped if either
/// fails the GTE flag test. Its half-width is `arg3 * 23 / depth`, laid
/// perpendicular to the screen-space line between the ends, and `arg2`
/// selects one of four 128x24 texture frames. The primitive is queued at the
/// first end's depth.
static void func_shelter_b6_training_room_80181FDC(GfxCoord* arg0, GfxCoord* arg1, s32 arg2, s16 arg3)
{
    EffectStripScratch** scratch;
    EffectStripScratch*  head;
    EffectStripScratch*  block;
    EffectStripScratch*  vecp;
    POLY_FT4*            prim;
    s16                  ang;
    u16                  vz;

    scratch                = &SCRATCH_STACK_CURSOR(EffectStripScratch);
    head                   = *scratch;
    head[-1].worldStart.vx = (u16)arg0->workm.t[0];
    block                  = head - 1;
    block->worldStart.vy   = (u16)arg0->workm.t[1];
    block->worldStart.vz   = (u16)arg0->workm.t[2];
    block->worldEnd.vx     = (u16)arg1->workm.t[0];
    block->worldEnd.vy     = (u16)arg1->workm.t[1];
    vz                     = (u16)arg1->workm.t[2];
    *scratch               = block;
    block->worldEnd.vz     = vz;
    vecp                   = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->worldStart);
    gte_rtps();
    gte_stsxy(&head[-1].screenStart);
    gte_stflg(&head[-1].projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&head[-1].depth);
        gte_ldv0(&head[-1].worldEnd);
        gte_rtps();
        gte_stsxy(&head[-1].screenEnd);
        gte_stflg(&head[-1].projectionFlags);
        if (block->projectionFlags >= 0) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage          = 0x28;
            prim->clut           = 0x42C8;
            prim->u0             = (arg2 & 1) << 7;
            prim->v0             = ((u32)(arg2 & 3) >> 1) * 24 - 0x30;
            prim->u1             = ((arg2 & 1) << 7) + 0x7F;
            prim->v1             = ((u32)(arg2 & 3) >> 1) * 24 - 0x30;
            prim->u2             = (arg2 & 1) << 7;
            prim->v2             = ((u32)(arg2 & 3) >> 1) * 24 - 0x19;
            prim->u3             = ((arg2 & 1) << 7) + 0x7F;
            prim->v3             = ((u32)(arg2 & 3) >> 1) * 24 - 0x19;
            ang                  = ratan2(block->screenEnd.vy - block->screenStart.vy, block->screenEnd.vx - block->screenStart.vx);
            block->cornerOffsetX = (((arg3 * 23) / block->depth) * rsin(ang)) >> 12;
            block->cornerOffsetY = (((arg3 * 23) / block->depth) * rcos(ang)) >> 12;
            prim->x0             = (u16)block->screenStart.vx + (u16)block->cornerOffsetX;
            prim->x3             = (u16)block->screenEnd.vx - (u16)block->cornerOffsetX;
            prim->y0             = (u16)block->screenStart.vy - (u16)block->cornerOffsetY;
            prim->y3             = (u16)block->screenEnd.vy + (u16)block->cornerOffsetY;
            block->cornerOffsetX = (((arg3 * 23) / block->depth) * rsin(ang + 0x400)) >> 12;
            block->cornerOffsetY = (((arg3 * 23) / block->depth) * rcos(ang + 0x400)) >> 12;
            prim->x1             = (u16)block->screenEnd.vx + (u16)block->cornerOffsetX;
            prim->x2             = (u16)block->screenStart.vx - (u16)block->cornerOffsetX;
            prim->y1             = (u16)block->screenEnd.vy - (u16)block->cornerOffsetY;
            prim->y2             = (u16)block->screenStart.vy + (u16)block->cornerOffsetY;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectStripScratch);
}

void func_shelter_b6_training_room_8018245C(Task* task)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         effectControl;
    u8          rgb[3];

    mem           = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }
    mem->age++;
    if (task->state == 0) {
        mem->scale = 0xC0;
        mem->angle = 0x200;
        D_shelter_b6_training_room_80185C98++;
        task->state           = 1;
        task->spawnArg1.value = D_shelter_b6_training_room_80185C98;
    }
    if (task->spawnArg1.value != D_shelter_b6_training_room_80185C98) {
        goto release;
    }
    rgb[0]      = mem->scale;
    rgb[1]      = mem->scale;
    rgb[2]      = mem->scale >> 1;
    mem->angle += 0x18;
    Gp_DrawArc(coord, (s16)(mem->angle * 2), 0, rgb);
    Gp_DrawRing(coord, (s16)((u16)mem->angle * 4), rgb);
    if (mem->age < 9) {
        return;
    }
    mem->scale -= 0x18;
    if (mem->scale < 0x18) {
    release:
        effectKillTask(mem, task);
    }
}

void func_shelter_b6_training_room_801825C0(Task* task)
{
    EffectWork* mem;
    GfxCoord*   coord;

    mem   = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        mem->age++;
        if (task->state == 0) {
            mem->move.vx    = 0;
            mem->move.vy    = 8;
            mem->move.vz    = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = ((gRandomLcgState >> 16) & 0xFFF) | 0x1000;
            task->state     = 1;
        }
        coord->coord.t[1]  += mem->move.vy;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (!(mem->age & 1)) {
            mem->index++;
        }
        if (mem->index < 8) {
            if (mem->age & 1) {
                Gp_DrawFxQuad(coord, mem->index, 0x400, mem->scale);
            }
        } else {
            effectKillTask(mem, task);
        }
    }
}

void func_shelter_b6_training_room_801826E0(Task* task)
{
    EffectWork* mem;
    GfxCoord*   coord;

    mem   = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        mem->age++;
        if (task->state == 0) {
            mem->move.vy = 0x20;
            mem->scale   = 0x80;
            mem->move.vx = 0;
            mem->move.vz = 0;
            task->state  = 1;
        }
        coord->coord.t[1]  += mem->move.vy;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (mem->age < 60) {
            if (mem->age & 1) {
                mem->index = (mem->index + 1) & 3;
                func_800EB6E8(coord, mem->index, 0x300, 0x80);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 3) == 0) {
                    Gp_SpawnEff(EFFECT_1AD, coord, 0, NULL);
                }
            }
        } else {
            effectKillTask(mem, task);
        }
    }
}

void func_shelter_b6_training_room_80182804(Task* task)
{
    EffectWork* mem;

    mem = task->spawnArg2.pointer;
    if (mem->age >= 0x15) {
        effectKillTask(mem, task);
        return;
    }
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        mem->age++;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->scale     += ((gRandomLcgState >> 16) & 0x1FF) + 0x200;
        mem->move.vx    = D_shelter_b6_training_room_80184334[24].vx + ((rcos(mem->scale) * 1000) >> 12);
        mem->move.vy    = D_shelter_b6_training_room_80184334[24].vy - mem->age * 200;
        mem->move.vz    = D_shelter_b6_training_room_80184334[24].vz + ((rsin(mem->scale) * 1000) >> 12);
        Gp_SpawnEff(EFFECT_1AE, NULL, 0, &mem->move);
    }
}

void func_shelter_b6_training_room_8018294C(Task* task)
{
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & 7) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Gp_SpawnEff(EFFECT_TRAINING_ROOM_ENERGY_ARC, task->extra.tmd->coords + ((u16)((gRandomLcgState >> 16) % 18) + 1), 0, NULL);
        }
    }
}

void func_shelter_b6_training_room_80182A14(s32 arg0, s32 arg1)
{
    GameLocationKey* sess = &gGameSession->location.loc;
    SpriteView*      rec  = Gp_SprtTables[sess->stage - 1]->areaViews[sess->area - 1];
    SpriteBatch*     batches;
    s32              run = arg0 & 0xFF;
    s32              flag;

    if (run == 0) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            batches           = rec[2].batches;
            batches[2].hidden = 1;
            batches           = rec[6].batches;
            batches[1].hidden = 1;
            return;
        }
        if (flag == 1) {
            batches           = rec[2].batches;
            batches[2].hidden = 0;
            batches           = rec[6].batches;
            batches[1].hidden = 0;
            return;
        }
    } else if (run == 1) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            batches           = rec[2].batches;
            batches[1].hidden = run;
            batches           = rec[6].batches;
            batches[2].hidden = run;
            return;
        }
        if (flag == run) {
            batches           = rec[2].batches;
            batches[1].hidden = 0;
            batches           = rec[6].batches;
            batches[2].hidden = 0;
        }
    }
}
