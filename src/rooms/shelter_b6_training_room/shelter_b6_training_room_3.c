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
static void _shelterB6TrainingRoomDrawRingBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex);
static void _shelterB6TrainingRoomDrawEnergyStrip(const GfxCoord* startCoord, const GfxCoord* endCoord, s32 textureFrame, s16 widthScale);

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

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// RGB nibbles passed to the room's fixed-glow drawers, scaled by 16 per channel.
enum {
    SHELTER_B6_TRAINING_ROOM_GLOW_RGB_210 = 0x210,
    SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444 = 0x444,
    SHELTER_B6_TRAINING_ROOM_GLOW_RGB_044 = 0x044,
    SHELTER_B6_TRAINING_ROOM_GLOW_RGB_600 = 0x600
};

/// Draws eight additive flickering discs at the room's fixed glow positions.
///
/// Seven centres are cyan and one is red; odd animation frames add eight to
/// each RGB channel. The caller selects visibility in mapped views 3..6.
/// Requires a composed view matrix, nonzero projected depths, scratch-stack
/// space for one `GlowCentreScratch`, and a current ordering table and frame
/// arena for up to 32 `POLY_G4` packets plus their blend commands. Scratch is
/// released after each disc; queued packets live until GPU drawing completes.
/// Overwrites GTE transform and projection registers.
static inline void _shelterB6TrainingRoomDrawFixedGlows(void)
{
    // Pixel radius is the signed scale times 64, divided by camera Z / 4.
    enum {
        SHELTER_B6_TRAINING_ROOM_FIXED_GLOW_SMALL_RADIUS_SCALE  = 0x100,
        SHELTER_B6_TRAINING_ROOM_FIXED_GLOW_MEDIUM_RADIUS_SCALE = 0x180,
        SHELTER_B6_TRAINING_ROOM_FIXED_GLOW_LARGE_RADIUS_SCALE  = 0x200
    };

    glowDrawDisc(&D_shelter_b6_training_room_80184334[0], SHELTER_B6_TRAINING_ROOM_FIXED_GLOW_MEDIUM_RADIUS_SCALE, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_044);
    glowDrawDisc(&D_shelter_b6_training_room_80184334[1], SHELTER_B6_TRAINING_ROOM_FIXED_GLOW_LARGE_RADIUS_SCALE, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_044);
    glowDrawDisc(&D_shelter_b6_training_room_80184334[2], SHELTER_B6_TRAINING_ROOM_FIXED_GLOW_SMALL_RADIUS_SCALE, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_044);
    glowDrawDisc(&D_shelter_b6_training_room_80184334[3], SHELTER_B6_TRAINING_ROOM_FIXED_GLOW_SMALL_RADIUS_SCALE, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_044);
    glowDrawDisc(&D_shelter_b6_training_room_80184334[4], SHELTER_B6_TRAINING_ROOM_FIXED_GLOW_SMALL_RADIUS_SCALE, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_044);
    glowDrawDisc(&D_shelter_b6_training_room_80184334[5], SHELTER_B6_TRAINING_ROOM_FIXED_GLOW_SMALL_RADIUS_SCALE, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_044);
    glowDrawDisc(&D_shelter_b6_training_room_80184334[6], SHELTER_B6_TRAINING_ROOM_FIXED_GLOW_MEDIUM_RADIUS_SCALE, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_600);
    glowDrawDisc(&D_shelter_b6_training_room_80184334[7], SHELTER_B6_TRAINING_ROOM_FIXED_GLOW_LARGE_RADIUS_SCALE, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_044);
}

void shelterB6TrainingRoomGlowTask(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_GLOW_INIT = 0,
        SHELTER_B6_TRAINING_ROOM_GLOW_DRAW = 1
    };
    s32 segmentIndex;
    s32 seedPass;

    if (task->state == SHELTER_B6_TRAINING_ROOM_GLOW_INIT) {
        D_shelter_b6_training_room_80185C98 = 0;
        // Preserve all three seed passes over the selected band, including the discarded draws.
        for (seedPass = 0; seedPass < 3; seedPass++) {
            for (segmentIndex = 0; segmentIndex < SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT; segmentIndex++) {
                D_shelter_b6_training_room_80185C60[task->spawnArg1.value][segmentIndex] = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
            }
        }
        task->state = SHELTER_B6_TRAINING_ROOM_GLOW_DRAW;
    }

    // Draw only the fixtures visible in this mapped camera view.
    switch (viewGetMappedIndex() & 0xFF) {
        case 2:
            _glowDrawCapsule(&D_shelter_b6_training_room_80184334[10], 0x180, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_210);
            break;
        case 3:
            _glowDrawCapsule(&D_shelter_b6_training_room_80184334[10], 0x180, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_210);
            _glowDrawCapsule(&D_shelter_b6_training_room_80184334[8], 0x180, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_210);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[12], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[13], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[14], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[15], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[16], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[18], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[19], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[20], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[21], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[22], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            _shelterB6TrainingRoomDrawFixedGlows();
            break;
        case 4:
            _glowDrawCapsule(&D_shelter_b6_training_room_80184334[8], 0x180, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_210);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[12], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[13], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[14], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[18], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[19], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[20], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            _shelterB6TrainingRoomDrawFixedGlows();
            break;
        case 5:
            _glowDrawCapsule(&D_shelter_b6_training_room_80184334[8], 0x180, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_210);
            _shelterB6TrainingRoomDrawFixedGlows();
            break;
        case 6:
            _shelterB6TrainingRoomDrawFixedGlows();
            break;
        case 7:
            _glowDrawCapsule(&D_shelter_b6_training_room_80184334[10], 0x180, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_210);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[17], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[23], 0x280, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_444);
            break;
        case 8:
            glowDrawDisc(&D_shelter_b6_training_room_80184334[2], 0x100, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_044);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[3], 0x100, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_044);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[4], 0x100, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_044);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[5], 0x100, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_044);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[6], 0x180, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_600);
            glowDrawDisc(&D_shelter_b6_training_room_80184334[7], 0x200, SHELTER_B6_TRAINING_ROOM_GLOW_RGB_044);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"

void shelterB6TrainingRoomOrangeBurstTask(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_ORANGE_BURST_INIT = 0,
        SHELTER_B6_TRAINING_ROOM_ORANGE_BURST_FADE = 1
    };
    u8          rgb[3];
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s16         radiusScale;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }
    work->age++;
    if (task->state == SHELTER_B6_TRAINING_ROOM_ORANGE_BURST_INIT) {
        work->age    = 1;
        work->scale  = 0xE0;
        work->angle  = 0x100;
        work->period = 0xE0;
        work->step   = 0x100;
        task->state  = SHELTER_B6_TRAINING_ROOM_ORANGE_BURST_FADE;
    }
    rgb[0]      = work->scale;
    rgb[1]      = work->scale >> 1;
    rgb[2]      = work->scale >> 2;
    radiusScale = work->angle + 0x10;
    work->angle = radiusScale;
    effectDrawGouraudDisc(coord, (s16)(radiusScale * 2), rgb);
    _roomVisualEffectsDrawHaloBurstGlow(coord, work->angle);
    // Exhaust the expanding outer band before fading the inner disc.
    if (work->period >= 0x19) {
        rgb[0] = work->period;
        rgb[1] = work->period >> 1;
        rgb[2] = work->period >> 2;
        effectDrawOuterGlowBand(coord, (s16)(work->step * 3 / 2), 0x60, rgb);
        work->period -= 0x18;
        work->step   += 0x80;
        return;
    }
    work->scale -= 0x10;
    if (work->scale < 0x10) {
        effectKillTask(work, task);
    }
}

#include "../../shared/room_visual_effects.inc.c"
#include "../../shared/room_visual_effects_glow_quad.inc.c"

void shelterB6TrainingRoomSummonRingTask(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_SUMMON_RING_INIT    = 0,
        SHELTER_B6_TRAINING_ROOM_SUMMON_RING_GROW    = 1,
        SHELTER_B6_TRAINING_ROOM_SUMMON_RING_FADE    = 2,
        SHELTER_B6_TRAINING_ROOM_SUMMON_RING_WAIT    = 3,
        SHELTER_B6_TRAINING_ROOM_SUMMON_RING_RELEASE = 4
    };
    EffectWork* work;
    GfxCoord*   coord;
    u8          rgb[3];

    /// Stages violet RGB bytes and draws both discs and the outer band.
    ///
    /// Requires a live EffectWork, composed GfxCoord, writable RGB[3] and
    /// signed sizing radius. Arguments must have no side effects: work, coord
    /// and rgb are evaluated repeatedly. Captures no caller identifiers.
    /// Expands to multiple statements; use only in braced blocks. Undefined
    /// before this task ends.
#define SHELTER_B6_TRAINING_ROOM_DRAW_SUMMON_GLOW(work, coord, outerRadius, rgb) \
    (rgb)[0] = (work)->scale >> 1;                                               \
    (rgb)[1] = (work)->scale >> 2;                                               \
    (rgb)[2] = (work)->scale;                                                    \
    effectDrawGouraudDisc((coord), (work)->angle, (rgb));                        \
    effectDrawGouraudDisc((coord), (s16)((u16)(work)->angle * 2), (rgb));        \
    effectDrawOuterGlowBand((coord), (outerRadius), 0x100, (rgb));

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        work->age++;
        switch (task->state) {
            // A positive spawn countdown is required for the brightness increment.
            case SHELTER_B6_TRAINING_ROOM_SUMMON_RING_INIT:
                task->state                         = SHELTER_B6_TRAINING_ROOM_SUMMON_RING_GROW;
                work->scale                         = 0;
                work->angle                         = 0x100;
                D_shelter_b6_training_room_80185C90 = NULL;
                work->step                          = 0x80 / task->spawnArg1.value;
            case SHELTER_B6_TRAINING_ROOM_SUMMON_RING_GROW:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    SHELTER_B6_TRAINING_ROOM_DRAW_SUMMON_GLOW(work, coord, (s16)((task->spawnArg1.value << 5) + 0x300), rgb);
                    break;
                }
                work->scale += work->step;
                work->angle += work->step << 3;
                task->spawnArg1.value--;
                SHELTER_B6_TRAINING_ROOM_DRAW_SUMMON_GLOW(work, coord, (s16)((task->spawnArg1.value << 5) + 0x300), rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale  = 0xFF;
                    task->state  = SHELTER_B6_TRAINING_ROOM_SUMMON_RING_FADE;
                    work->period = 0x600;
                    work->step   = 0;
                    // Publish the beam origin only once the buildup is complete.
                    D_shelter_b6_training_room_80185C90 = coord;
                }
                break;
            case SHELTER_B6_TRAINING_ROOM_SUMMON_RING_FADE:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    SHELTER_B6_TRAINING_ROOM_DRAW_SUMMON_GLOW(work, coord, 0x300, rgb);
                    break;
                }
                if (work->scale >= 9) {
                    SHELTER_B6_TRAINING_ROOM_DRAW_SUMMON_GLOW(work, coord, 0x300, rgb);
                    work->scale -= 8;
                    break;
                }
                task->state = SHELTER_B6_TRAINING_ROOM_SUMMON_RING_WAIT;
                break;
            // The actor retains the work block and requests state 4 when the attack ends.
            case SHELTER_B6_TRAINING_ROOM_SUMMON_RING_WAIT:
                break;
            case SHELTER_B6_TRAINING_ROOM_SUMMON_RING_RELEASE:
                effectKillTask(work, task);
                break;
        }
    } else {
        effectKillTask(work, task);
    }

#undef SHELTER_B6_TRAINING_ROOM_DRAW_SUMMON_GLOW
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

    if (worldCollisionProjectGroundCoord(from, &c0) != 1 || worldCollisionProjectGroundCoord(to, &c1) != 1) {
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
                    effectDrawGouraudDisc(coord, work->angle, rgb);
                    effectDrawGouraudDisc(coord, (s16)((u16)work->angle * 2), rgb);
                    effectDrawOuterGlowBand(coord, (s16)((task->spawnArg1.value % 15) * (work->scale << 2)), 0x100, rgb);
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
                effectDrawGouraudDisc(coord, work->angle, rgb);
                effectDrawGouraudDisc(coord, (s16)((u16)work->angle * 2), rgb);
                effectDrawOuterGlowBand(coord, (s16)((task->spawnArg1.value % 15) * (work->scale << 2)), 0x100, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    effectSpawn(EFFECT_SHELTER_B6_TRAINING_ROOM_RING_WALL, coord, 0, NULL);
                    effectSpawn(EFFECT_SHELTER_B6_TRAINING_ROOM_RING_WALL, coord, 1, NULL);
                    effectSpawn(EFFECT_SHELTER_B6_TRAINING_ROOM_RING_WALL, coord, 2, NULL);
                    work->period = 0x600;
                    work->step   = 0;
                }
                return;
            case 2:
                if (work->scale >= 5) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 1;
                    rgb[2] = work->scale >> 2;
                    effectDrawGouraudDisc(coord, work->angle, rgb);
                    effectDrawGouraudDisc(coord, (s16)((u16)work->angle * 2), rgb);
                    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                        work->scale -= 4;
                    }
                    effectDrawScreenTint(rgb, GPU_BLEND_ADD);
                    return;
                }
                break;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

void shelterB6TrainingRoomRingBandTask(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_RING_BAND_INIT        = 0,
        SHELTER_B6_TRAINING_ROOM_RING_BAND_RISE        = 1,
        SHELTER_B6_TRAINING_ROOM_RING_BAND_FADE_MIDDLE = 2,
        SHELTER_B6_TRAINING_ROOM_RING_BAND_FADE_OUTER  = 3,
        SHELTER_B6_TRAINING_ROOM_RING_BAND_RELEASE     = 4
    };
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        _shelterB6TrainingRoomDrawRingBand(work, coord, task->spawnArg1.value);
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }
    work->age++;
    switch (task->state) {
        // Flatten the effect origin to its local ground plane before the first draw.
        case SHELTER_B6_TRAINING_ROOM_RING_BAND_INIT:
            work->scale         = 0x80;
            task->state         = task->spawnArg1.value + SHELTER_B6_TRAINING_ROOM_RING_BAND_RISE;
            coord->coord.t[1]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            return;
        case SHELTER_B6_TRAINING_ROOM_RING_BAND_RISE:
            if (work->scale < 5) {
                effectKillTask(work, task);
                return;
            }
            if (work->period < 0xC00) {
                work->period += 0xC0;
            } else {
                work->scale -= 4;
            }
            work->angle += 0x20;
            work->step  += 0x18;
            _shelterB6TrainingRoomDrawRingBand(work, coord, task->spawnArg1.value);
            return;
        case SHELTER_B6_TRAINING_ROOM_RING_BAND_FADE_MIDDLE:
            if (work->scale < 4) {
                effectKillTask(work, task);
                return;
            }
            work->scale -= 3;
            work->angle += 0x40;
            work->step  += 0x18;
            _shelterB6TrainingRoomDrawRingBand(work, coord, task->spawnArg1.value);
            return;
        case SHELTER_B6_TRAINING_ROOM_RING_BAND_FADE_OUTER:
            if (work->scale < 5) {
                effectKillTask(work, task);
                return;
            }
            work->scale -= 4;
            work->angle += 0x180;
            work->step  += 0x18;
            _shelterB6TrainingRoomDrawRingBand(work, coord, task->spawnArg1.value);
            return;
        case SHELTER_B6_TRAINING_ROOM_RING_BAND_RELEASE:
            effectKillTask(work, task);
        default:
            return;
    }
}

/// Draws one six-segment textured band between a lifted top ring and a ground bottom ring.
///
/// `bandIndex` is 0..2. The shape adds to `work->angle` for bottom radius,
/// `work->step` for top-ring spread and `work->period` for lift along local -Y;
/// distances narrow to 16 bits. `work->scale` supplies RGB brightness and
/// `work->age` advances each segment's signed texture phase modulo six.
/// A negative remainder wraps through the packet's UV bytes.
/// `coord->workm` must be composed; inputs are borrowed until return.
/// Requires one free scratch block, a current ordering table and room for up to
/// six `POLY_FT4` packets. Releases scratch storage on return; emitted packets
/// remain in the frame arena. Overwrites GTE transform and projection registers.
static void _shelterB6TrainingRoomDrawRingBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_RING_TEXTURE_FRAMES = 6,
        SHELTER_B6_TRAINING_ROOM_RING_CELL_SIZE      = 40,
        SHELTER_B6_TRAINING_ROOM_RING_TOP_V          = 96,
        SHELTER_B6_TRAINING_ROOM_RING_TRIG_SHIFT     = 12
    };
    _ShelterB6TrainingRoomBandScratch* scratch;
    SVECTOR*                           bottomVertex;
    POLY_FT4*                          quad;
    const EffectBandShape*             shape;
    s32                                segmentIndex;
    s32                                nextSegment;
    s32                                yaw;
    s32                                leftU;
    s16                                textureFrame;
    s16                                topRadius;
    s16                                bottomRadius;
    u16                                lift;
    u16                                liftGrowth;

    shape         = &D_shelter_b6_training_room_80184404.entries[bandIndex];
    liftGrowth    = work->period;
    bottomRadius  = work->angle;
    lift          = liftGrowth + shape->lift;
    bottomRadius += shape->baseRadius;
    topRadius     = bottomRadius + work->step + shape->spread;
    scratch       = SCRATCH_STACK_RESERVE_BLOCK(_ShelterB6TrainingRoomBandScratch);
    // Build the lifted top ring and ground bottom ring, then narrow their world positions.
    /// Builds the lifted and ground rings in a reserved scratch block.
    ///
    /// Requires a _ShelterB6TrainingRoomBandScratch*, a composed GfxCoord*,
    /// signed 16-bit radii and unsigned 16-bit lift in coordinate units. All
    /// arguments must have no side effects and may be evaluated repeatedly.
    /// Captures s32 segmentIndex/yaw, SVECTOR* bottomVertex, GsWSMATRIX and
    /// this drawer's Q12 shift constant. Expands to multiple statements; invoke
    /// unconditionally in the drawer. Undefined before it ends.
#define SHELTER_B6_TRAINING_ROOM_BUILD_BAND_RINGS(scratch, coord, topRadius, bottomRadius, lift)                                                                         \
    gte_SetTransMatrix(&GsWSMATRIX);                                                                                                                                     \
    for (segmentIndex = 0; segmentIndex < SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT; segmentIndex++) {                                                                 \
        yaw                                 = segmentIndex * (ONE / SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT);                                                        \
        (scratch)->topRing[segmentIndex].vx = (rsin(yaw) * (topRadius)) >> SHELTER_B6_TRAINING_ROOM_RING_TRIG_SHIFT;                                                     \
        (scratch)->topRing[segmentIndex].vy = -(lift);                                                                                                                   \
        (scratch)->topRing[segmentIndex].vz = (rcos(yaw) * (topRadius)) >> SHELTER_B6_TRAINING_ROOM_RING_TRIG_SHIFT;                                                     \
        gte_SetRotMatrix(&(coord)->workm);                                                                                                                               \
        gte_ldv0(&(scratch)->topRing[segmentIndex]);                                                                                                                     \
        gte_rtv0();                                                                                                                                                      \
        gte_stsv(&(scratch)->topRing[segmentIndex]);                                                                                                                     \
        (scratch)->topRing[segmentIndex].vx    = (u16)(scratch)->topRing[segmentIndex].vx + (u16)(coord)->workm.t[0];                                                    \
        (scratch)->topRing[segmentIndex].vy    = (u16)(scratch)->topRing[segmentIndex].vy + (u16)(coord)->workm.t[1];                                                    \
        (scratch)->topRing[segmentIndex].vz    = (u16)(scratch)->topRing[segmentIndex].vz + (u16)(coord)->workm.t[2];                                                    \
        (scratch)->bottomRing[segmentIndex].vx = (rsin(yaw) * (bottomRadius)) >> SHELTER_B6_TRAINING_ROOM_RING_TRIG_SHIFT;                                               \
        bottomVertex                           = (SVECTOR*)((u8*)(scratch) + segmentIndex * sizeof(SVECTOR) + OFFSET_OF(_ShelterB6TrainingRoomBandScratch, bottomRing)); \
        bottomVertex->vy                       = 0;                                                                                                                      \
        bottomVertex->vz                       = (rcos(yaw) * (bottomRadius)) >> SHELTER_B6_TRAINING_ROOM_RING_TRIG_SHIFT;                                               \
        gte_SetRotMatrix(&(coord)->workm);                                                                                                                               \
        gte_ldv0(&(scratch)->bottomRing[segmentIndex]);                                                                                                                  \
        gte_rtv0();                                                                                                                                                      \
        gte_stsv(&(scratch)->bottomRing[segmentIndex]);                                                                                                                  \
        (scratch)->bottomRing[segmentIndex].vx = (u16)(scratch)->bottomRing[segmentIndex].vx + (u16)(coord)->workm.t[0];                                                 \
        bottomVertex->vy                       = (u16)bottomVertex->vy + (u16)(coord)->workm.t[1];                                                                       \
        bottomVertex->vz                       = (u16)bottomVertex->vz + (u16)(coord)->workm.t[2];                                                                       \
    }

    SHELTER_B6_TRAINING_ROOM_BUILD_BAND_RINGS(scratch, coord, topRadius, bottomRadius, lift);
    // Project each segment and use its signed phase byte to select a texture cell.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT; segmentIndex++) {
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtps();
        gte_stsxy(&scratch->sxy0);
        nextSegment = segmentIndex + 1;
        gte_ldv3(&scratch->topRing[nextSegment % SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT], &scratch->bottomRing[segmentIndex], &scratch->bottomRing[nextSegment % SHELTER_B6_TRAINING_ROOM_BAND_SEGMENT_COUNT]);
        gte_rtpt();
        textureFrame = ((s8)D_shelter_b6_training_room_80185C60[bandIndex][segmentIndex] + work->age) % SHELTER_B6_TRAINING_ROOM_RING_TEXTURE_FRAMES;
        gte_stsxy3(&scratch->sxy1, &scratch->sxy2, &scratch->sxy3);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->otz);
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setSemiTrans(quad, true);
            setRGB0(quad, work->scale, work->scale, work->scale);
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 640, 0);
            quad->clut  = getClut(32, 266);
            leftU       = textureFrame * SHELTER_B6_TRAINING_ROOM_RING_CELL_SIZE;
            setUV4(quad, leftU, SHELTER_B6_TRAINING_ROOM_RING_TOP_V, leftU + SHELTER_B6_TRAINING_ROOM_RING_CELL_SIZE - 1, SHELTER_B6_TRAINING_ROOM_RING_TOP_V,
                   leftU, SHELTER_B6_TRAINING_ROOM_RING_TOP_V + SHELTER_B6_TRAINING_ROOM_RING_CELL_SIZE - 1,
                   leftU + SHELTER_B6_TRAINING_ROOM_RING_CELL_SIZE - 1, SHELTER_B6_TRAINING_ROOM_RING_TOP_V + SHELTER_B6_TRAINING_ROOM_RING_CELL_SIZE - 1);
            quad->x0 = (u16)scratch->sxy0.vx;
            quad->y0 = (u16)scratch->sxy0.vy;
            quad->x1 = (u16)scratch->sxy1.vx;
            quad->y1 = (u16)scratch->sxy1.vy;
            quad->x2 = (u16)scratch->sxy2.vx;
            quad->y2 = (u16)scratch->sxy2.vy;
            quad->x3 = (u16)scratch->sxy3.vx;
            quad->y3 = (u16)scratch->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_ShelterB6TrainingRoomBandScratch);

#undef SHELTER_B6_TRAINING_ROOM_BUILD_BAND_RINGS
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
        effectDrawGouraudDisc(coord, 0x200, rgb);
        effectDrawGouraudDisc(coord, 0x400, rgb);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & 3) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_FLASH_BURST, task->extra.tmd->coords + (((gRandomLcgState >> 16) & 0xF) + 3), 0x10080, NULL);
        }
    }
}

void shelterB6TrainingRoomEnergyArcTask(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_ENERGY_ARC_INIT = 0,
        SHELTER_B6_TRAINING_ROOM_ENERGY_ARC_DRAW = 1
    };
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        work->age++;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        // Reparent the sprite to its live joint and restore the spawn offset.
        if (task->state == SHELTER_B6_TRAINING_ROOM_ENERGY_ARC_INIT) {
            u32 sizeRandom;

            coord->parent = work->parent;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = work->pos.vx;
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            sizeRandom          = gRandomLcgState;
            coord->coord.t[1]   = work->pos.vy;
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            coord->coord.t[2]   = work->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->scale         = ((sizeRandom >> 16) & 0x1FF) + 0x100;
            work->angle         = (gRandomLcgState >> 16) & (ONE - 1);
            work->period        = ((gRandomLcgState >> 16) & 0xF) + 6;
            task->state         = SHELTER_B6_TRAINING_ROOM_ENERGY_ARC_DRAW;
        }
        // Draw the animated sprite every tick and the link to the body anchor on odd ticks.
        spriteQuadDraw(coord, work->age, work->scale, work->angle);
        if (work->age & 1) {
            _shelterB6TrainingRoomDrawEnergyStrip(coord, D_shelter_b6_training_room_80185C94, work->age >> 1, work->scale);
        }
        if (work->age > work->period) {
            effectKillTask(work, task);
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

/// Draws an additive textured parallelogram linking two composed world positions.
///
/// Both coordinates are borrowed read-only; their translations narrow to s16.
/// A negative GTE FLAG at either end drops the strip. `textureFrame` wraps
/// modulo four over two 128-texel columns and two 24-texel rows. `widthScale`
/// is signed: the corner scale is `widthScale * 23 / (SZ3 / 4)` pixels using
/// the start depth, which must be nonzero. Opposite corners use perpendicular
/// offsets, the other pair longitudinal ones; the short edges are skewed.
/// Requires one free `EffectStripScratch` block, a current ordering table and
/// room for one `POLY_FT4`. Releases scratch on every path; emitted packets
/// remain in the frame arena. Overwrites GTE transform and projection registers.
static void _shelterB6TrainingRoomDrawEnergyStrip(const GfxCoord* startCoord, const GfxCoord* endCoord, s32 textureFrame, s16 widthScale)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_STRIP_CELL_WIDTH   = 128,
        SHELTER_B6_TRAINING_ROOM_STRIP_CELL_HEIGHT  = 24,
        SHELTER_B6_TRAINING_ROOM_STRIP_COLUMN_MASK  = 1,
        SHELTER_B6_TRAINING_ROOM_STRIP_FRAME_MASK   = 3,
        SHELTER_B6_TRAINING_ROOM_STRIP_TOP_V        = 208,
        SHELTER_B6_TRAINING_ROOM_STRIP_QUARTER_TURN = ONE / 4,
        SHELTER_B6_TRAINING_ROOM_STRIP_TRIG_SHIFT   = 12
    };
    EffectStripScratch* block;
    POLY_FT4*           quad;
    s16                 screenAngle;

    /// Projects one word-aligned SVECTOR with the GTE matrices already loaded.
    ///
    /// Writes screen pixels and the full FLAG word, leaving SZ3 for depth.
    /// Each pointer is evaluated once in order; outputs must be writable and
    /// word-aligned. Captures no caller identifiers. Expands to multiple
    /// statements; use within braced blocks. Undefined before the drawer ends.
#define SHELTER_B6_TRAINING_ROOM_PROJECT_STRIP_POINT(worldPoint, screenPoint, projectionFlags) \
    gte_ldv0((worldPoint));                                                                    \
    gte_rtps();                                                                                \
    gte_stsxy((screenPoint));                                                                  \
    gte_stflg((projectionFlags));

    // Stage both cached world translations, retaining their low 16 bits.
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectStripScratch);
    block->worldStart.vx = startCoord->workm.t[0];
    block->worldStart.vy = startCoord->workm.t[1];
    block->worldStart.vz = startCoord->workm.t[2];
    block->worldEnd.vx   = endCoord->workm.t[0];
    block->worldEnd.vy   = endCoord->workm.t[1];
    block->worldEnd.vz   = endCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    SHELTER_B6_TRAINING_ROOM_PROJECT_STRIP_POINT(&block->worldStart, &block->screenStart, &block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        SHELTER_B6_TRAINING_ROOM_PROJECT_STRIP_POINT(&block->worldEnd, &block->screenEnd, &block->projectionFlags);
        if (block->projectionFlags >= 0) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setSemiTrans(quad, true);
            setShadeTex(quad, true);
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 512, 0);
            quad->clut  = getClut(128, 267);
            quad->u0    = (textureFrame & SHELTER_B6_TRAINING_ROOM_STRIP_COLUMN_MASK) * SHELTER_B6_TRAINING_ROOM_STRIP_CELL_WIDTH;
            quad->v0    = ((u32)(textureFrame & SHELTER_B6_TRAINING_ROOM_STRIP_FRAME_MASK) >> 1) * SHELTER_B6_TRAINING_ROOM_STRIP_CELL_HEIGHT - (256 - SHELTER_B6_TRAINING_ROOM_STRIP_TOP_V);
            quad->u1    = ((textureFrame & SHELTER_B6_TRAINING_ROOM_STRIP_COLUMN_MASK) * SHELTER_B6_TRAINING_ROOM_STRIP_CELL_WIDTH) + (SHELTER_B6_TRAINING_ROOM_STRIP_CELL_WIDTH - 1);
            quad->v1    = ((u32)(textureFrame & SHELTER_B6_TRAINING_ROOM_STRIP_FRAME_MASK) >> 1) * SHELTER_B6_TRAINING_ROOM_STRIP_CELL_HEIGHT - (256 - SHELTER_B6_TRAINING_ROOM_STRIP_TOP_V);
            quad->u2    = (textureFrame & SHELTER_B6_TRAINING_ROOM_STRIP_COLUMN_MASK) * SHELTER_B6_TRAINING_ROOM_STRIP_CELL_WIDTH;
            quad->v2    = ((u32)(textureFrame & SHELTER_B6_TRAINING_ROOM_STRIP_FRAME_MASK) >> 1) * SHELTER_B6_TRAINING_ROOM_STRIP_CELL_HEIGHT - (256 - SHELTER_B6_TRAINING_ROOM_STRIP_TOP_V - (SHELTER_B6_TRAINING_ROOM_STRIP_CELL_HEIGHT - 1));
            quad->u3    = ((textureFrame & SHELTER_B6_TRAINING_ROOM_STRIP_COLUMN_MASK) * SHELTER_B6_TRAINING_ROOM_STRIP_CELL_WIDTH) + (SHELTER_B6_TRAINING_ROOM_STRIP_CELL_WIDTH - 1);
            quad->v3    = ((u32)(textureFrame & SHELTER_B6_TRAINING_ROOM_STRIP_FRAME_MASK) >> 1) * SHELTER_B6_TRAINING_ROOM_STRIP_CELL_HEIGHT - (256 - SHELTER_B6_TRAINING_ROOM_STRIP_TOP_V - (SHELTER_B6_TRAINING_ROOM_STRIP_CELL_HEIGHT - 1));
            // Use perpendicular offsets for opposite corners, then longitudinal offsets.
            screenAngle          = ratan2(block->screenEnd.vy - block->screenStart.vy, block->screenEnd.vx - block->screenStart.vx);
            block->cornerOffsetX = (((widthScale * (SHELTER_B6_TRAINING_ROOM_STRIP_CELL_HEIGHT - 1)) / block->depth) * rsin(screenAngle)) >> SHELTER_B6_TRAINING_ROOM_STRIP_TRIG_SHIFT;
            block->cornerOffsetY = (((widthScale * (SHELTER_B6_TRAINING_ROOM_STRIP_CELL_HEIGHT - 1)) / block->depth) * rcos(screenAngle)) >> SHELTER_B6_TRAINING_ROOM_STRIP_TRIG_SHIFT;
            quad->x0             = (u16)block->screenStart.vx + (u16)block->cornerOffsetX;
            quad->x3             = (u16)block->screenEnd.vx - (u16)block->cornerOffsetX;
            quad->y0             = (u16)block->screenStart.vy - (u16)block->cornerOffsetY;
            quad->y3             = (u16)block->screenEnd.vy + (u16)block->cornerOffsetY;
            block->cornerOffsetX = (((widthScale * (SHELTER_B6_TRAINING_ROOM_STRIP_CELL_HEIGHT - 1)) / block->depth) * rsin(screenAngle + SHELTER_B6_TRAINING_ROOM_STRIP_QUARTER_TURN)) >> SHELTER_B6_TRAINING_ROOM_STRIP_TRIG_SHIFT;
            block->cornerOffsetY = (((widthScale * (SHELTER_B6_TRAINING_ROOM_STRIP_CELL_HEIGHT - 1)) / block->depth) * rcos(screenAngle + SHELTER_B6_TRAINING_ROOM_STRIP_QUARTER_TURN)) >> SHELTER_B6_TRAINING_ROOM_STRIP_TRIG_SHIFT;
            quad->x1             = (u16)block->screenEnd.vx + (u16)block->cornerOffsetX;
            quad->x2             = (u16)block->screenStart.vx - (u16)block->cornerOffsetX;
            quad->y1             = (u16)block->screenEnd.vy - (u16)block->cornerOffsetY;
            quad->y2             = (u16)block->screenStart.vy + (u16)block->cornerOffsetY;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectStripScratch);

#undef SHELTER_B6_TRAINING_ROOM_PROJECT_STRIP_POINT
}

void shelterB6TrainingRoomHitFlashTask(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_HIT_FLASH_INIT = 0,
        SHELTER_B6_TRAINING_ROOM_HIT_FLASH_FADE = 1
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    u8          rgb[3];

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }
    work->age++;
    if (task->state == SHELTER_B6_TRAINING_ROOM_HIT_FLASH_INIT) {
        work->scale = 0xC0;
        work->angle = 0x200;
        // A new generation supersedes every older hit flash; the stamp wraps at 16 bits.
        D_shelter_b6_training_room_80185C98++;
        task->state           = SHELTER_B6_TRAINING_ROOM_HIT_FLASH_FADE;
        task->spawnArg1.value = D_shelter_b6_training_room_80185C98;
    }
    if (task->spawnArg1.value != D_shelter_b6_training_room_80185C98) {
        effectKillTask(work, task);
        return;
    }
    rgb[0]       = work->scale;
    rgb[1]       = work->scale;
    rgb[2]       = work->scale >> 1;
    work->angle += 0x18;
    effectDrawOuterGlowBand(coord, (s16)(work->angle * 2), 0, rgb);
    effectDrawGouraudDisc(coord, (s16)((u16)work->angle * 4), rgb);
    if (work->age < 9) {
        return;
    }
    work->scale -= 0x18;
    if (work->scale < 0x18) {
        effectKillTask(work, task);
    }
}

void shelterB6TrainingRoomSinkingSpriteTask(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_SINKING_SPRITE_INIT = 0,
        SHELTER_B6_TRAINING_ROOM_SINKING_SPRITE_DRAW = 1
    };
    enum { SHELTER_B6_TRAINING_ROOM_SINKING_SPRITE_FRAME_COUNT = 8 };
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        work->age++;
        if (task->state == SHELTER_B6_TRAINING_ROOM_SINKING_SPRITE_INIT) {
            work->move.vx   = 0;
            work->move.vy   = 8;
            work->move.vz   = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = ((gRandomLcgState >> 16) & (ONE - 1)) | ONE;
            task->state     = SHELTER_B6_TRAINING_ROOM_SINKING_SPRITE_DRAW;
        }
        coord->coord.t[1]  += work->move.vy;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        // Advance on even ticks and draw on odd ticks, one sprite every two running ticks.
        if (!(work->age & 1)) {
            work->index++;
        }
        if (work->index < SHELTER_B6_TRAINING_ROOM_SINKING_SPRITE_FRAME_COUNT) {
            if (work->age & 1) {
                effectDrawSpinningBillboard(coord, work->index, 0x400, work->scale);
            }
        } else {
            effectKillTask(work, task);
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
                effectDrawModulatedBillboard(coord, mem->index, 0x300, 0x80);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 3) == 0) {
                    effectSpawn(EFFECT_1AD, coord, 0, NULL);
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
        effectSpawn(EFFECT_1AE, NULL, 0, &mem->move);
    }
}

void func_shelter_b6_training_room_8018294C(Task* task)
{
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & 7) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_TRAINING_ROOM_ENERGY_ARC, task->extra.tmd->coords + ((u16)((gRandomLcgState >> 16) % 18) + 1), 0, NULL);
        }
    }
}

void shelterB6TrainingRoomSetPartDestroyedSprites(u8 partSlot, u8 destroyed)
{
    enum { SHELTER_B6_TRAINING_ROOM_PART_INTACT    = 0,
           SHELTER_B6_TRAINING_ROOM_PART_DESTROYED = 1 };
    GameLocationKey* location = &gGameSession->location.loc;
    SpriteView*      views    = Gp_SprtTables[location->stage - 1]->areaViews[location->area - 1];
    SpriteBatch*     batches;
    s32              slot = partSlot;
    s32              destroyedState;

    // Each part exposes a different destruction sprite group in views 2 and 6.
    if (slot == 0) {
        destroyedState = destroyed;
        if (destroyedState == SHELTER_B6_TRAINING_ROOM_PART_INTACT) {
            batches           = views[2].batches;
            batches[2].hidden = 1;
            batches           = views[6].batches;
            batches[1].hidden = 1;
            return;
        }
        if (destroyedState == SHELTER_B6_TRAINING_ROOM_PART_DESTROYED) {
            batches           = views[2].batches;
            batches[2].hidden = 0;
            batches           = views[6].batches;
            batches[1].hidden = 0;
            return;
        }
    } else if (slot == 1) {
        destroyedState = destroyed;
        if (destroyedState == SHELTER_B6_TRAINING_ROOM_PART_INTACT) {
            batches           = views[2].batches;
            batches[1].hidden = slot;
            batches           = views[6].batches;
            batches[2].hidden = slot;
            return;
        }
        if (destroyedState == slot) {
            batches           = views[2].batches;
            batches[1].hidden = 0;
            batches           = views[6].batches;
            batches[2].hidden = 0;
        }
    }
}
