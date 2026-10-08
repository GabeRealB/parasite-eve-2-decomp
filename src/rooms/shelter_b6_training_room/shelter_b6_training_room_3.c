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

/// Beam angles are in 4096 units per turn; trig results have 12 fractional bits.
enum {
    SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN       = ONE / 2,
    SHELTER_B6_TRAINING_ROOM_BEAM_QUARTER_TURN    = ONE / 4,
    SHELTER_B6_TRAINING_ROOM_BEAM_EIGHTH_TURN     = ONE / 8,
    SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT      = 12,
    SHELTER_B6_TRAINING_ROOM_BEAM_RADIUS_SHIFT    = 6,
    SHELTER_B6_TRAINING_ROOM_BEAM_RGB_NIBBLE_MASK = 0xF
};

static void _shelterB6TrainingRoomDrawGroundBeam(const GfxCoord* startCoord, const GfxCoord* endCoord, s16 radiusScale, u16 paletteOffset);
static void _shelterB6TrainingRoomDrawRingBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex);
static void _shelterB6TrainingRoomDrawEnergyStrip(const GfxCoord* startCoord, const GfxCoord* endCoord, s32 textureFrame, s16 widthScale);

// Battle-end hold durations supplied by the normal and skipped defeat scenes.
enum {
    SHELTER_B6_TRAINING_ROOM_DEFEAT_END_DELAY_FRAMES = 100,
    SHELTER_B6_TRAINING_ROOM_SKIP_END_DELAY_FRAMES   = 8
};

TaskMessageEntry D_shelter_b6_training_room_80182AF4[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, shelterB6TrainingRoomResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, shelterB6TrainingRoomRefuseKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, shelterB6TrainingRoomIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, func_shelter_b6_training_room_8017D684 },
    { ROOM_MESSAGE_ACTOR_EVENT, shelterB6TrainingRoomStartDefeatScene },
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

TaskDesc D_shelter_b6_training_room_801839A8 = { { { TASK_BODY_NONE, 96 } }, shelterB6TrainingRoomPlayerHeadAimTask, { .value = 0 } };

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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelterB6TrainingRoomPrepareDefeatScene }, { .value = SHELTER_B6_TRAINING_ROOM_DEFEAT_END_DELAY_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_shelter_b6_training_room_801839C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_training_room_801839E4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = shelterB6TrainingRoomStopBattlePresentation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelterB6TrainingRoomControlPlayerHeadAim }, { .value = SHELTER_B6_TRAINING_ROOM_HEAD_AIM_FADE_IN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b6_training_room_80183AB4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_shelter_b6_training_room_80183AC8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelterB6TrainingRoomControlPlayerHeadAim }, { .value = SHELTER_B6_TRAINING_ROOM_HEAD_AIM_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b6_training_room_80184124[14] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelterB6TrainingRoomPrepareDefeatScene }, { .value = SHELTER_B6_TRAINING_ROOM_SKIP_END_DELAY_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_shelter_b6_training_room_801839C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_training_room_801839E4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = shelterB6TrainingRoomStopBattlePresentation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b6_training_room_80183A4C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelterB6TrainingRoomControlPlayerHeadAim }, { .value = SHELTER_B6_TRAINING_ROOM_HEAD_AIM_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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

/// Prepares one Gouraud wedge of a beam's rounded end cap, fading from colour to black.
///
/// Borrows one complete writable `POLY_G4` packet through `capWedge`. Sets its
/// length and opaque Gouraud command, with the supplied RGB bytes at centre vertex 2
/// and black at rim vertices 0, 1 and 3. The caller supplies screen positions,
/// DMA linkage and additive blending, and keeps the packet live until the GPU
/// has consumed it.
static inline void _shelterB6TrainingRoomInitBeamCapWedge(POLY_G4* capWedge, u8 red, u8 green, u8 blue)
{
    setPolyG4(capWedge);
    setRGB0(capWedge, 0, 0, 0);
    setRGB1(capWedge, 0, 0, 0);
    setRGB2(capWedge, red, green, blue);
    setRGB3(capWedge, 0, 0, 0);
}

void shelterB6TrainingRoomDrawSummonBeam(const GfxCoord* endCoord, s16 radiusScale, u16 colorIndex)
{
    enum { SHELTER_B6_TRAINING_ROOM_BEAM_LAYER_COUNT = 2 };
    RoomBeamScratch* scratch;
    POLY_G4*         quad;
    s32              layer;
    u8               red;
    u8               green;
    u8               blue;
    s32              flicker;
    s32              scaledRadius;
    s32              sweepLimit;
    s32              baseAngle;
    s32              sweepAngle;
    s32              nextSweepAngle;
    s32              sideAngle;
    s32              baseRed;
    s32              baseGreen;

    if (D_shelter_b6_training_room_80185C90 == NULL) {
        return;
    }
    // Narrow both cached translations to s16 before projection.
    scratch            = SCRATCH_STACK_RESERVE_BLOCK(RoomBeamScratch);
    scratch->point0.vx = D_shelter_b6_training_room_80185C90->workm.t[0];
    scratch->point0.vy = D_shelter_b6_training_room_80185C90->workm.t[1];
    scratch->point0.vz = D_shelter_b6_training_room_80185C90->workm.t[2];
    scratch->point1.vx = endCoord->workm.t[0];
    scratch->point1.vy = endCoord->workm.t[1];
    scratch->point1.vz = endCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->point0);
    gte_rtps();
    gte_stsxy(&scratch->pair.sx0);
    gte_stflg(&scratch->pair.flag);
    if (scratch->pair.flag >= 0) {
        gte_stszotz(&scratch->pair.otz0);
        gte_ldv0(&scratch->point1);
        gte_rtps();
        gte_stsxy(&scratch->pair.sx1);
        gte_stflg(&scratch->pair.flag);
        gte_stszotz(&scratch->pair.otz1);
        if (scratch->pair.flag >= 0) {
            // The ground tint also uses the selected packed word as a palette offset.
            colorIndex = D_shelter_b6_training_room_801843FC[colorIndex];
            baseRed    = ((colorIndex >> 8) & SHELTER_B6_TRAINING_ROOM_BEAM_RGB_NIBBLE_MASK) << 4;
            baseGreen  = ((colorIndex >> 4) & SHELTER_B6_TRAINING_ROOM_BEAM_RGB_NIBBLE_MASK) << 4;
            flicker    = ((u8)gDisplayState.animFrame & 1) << 4;
            red        = baseRed + flicker;
            green      = baseGreen + flicker;
            blue       = ((colorIndex & SHELTER_B6_TRAINING_ROOM_BEAM_RGB_NIBBLE_MASK) << 4) + flicker;
            for (layer = 1; layer < SHELTER_B6_TRAINING_ROOM_BEAM_LAYER_COUNT + 1; layer++) {
                scaledRadius          = radiusScale * (layer << SHELTER_B6_TRAINING_ROOM_BEAM_RADIUS_SHIFT);
                scratch->pair.radius0 = scaledRadius / scratch->pair.otz0;
                scratch->pair.radius1 = scaledRadius / scratch->pair.otz1;
                sweepAngle            = (s16)ratan2((s16)scratch->pair.sy1 - (s16)scratch->pair.sy0, (s16)scratch->pair.sx0 - (s16)scratch->pair.sx1);
                if (sweepAngle < sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN) {
                    baseAngle  = sweepAngle;
                    sweepLimit = sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN;
                    // Each quarter-turn step queues an end cap, origin cap and joining side.
                    do {
                        quad           = gGpuPrimCursor;
                        gGpuPrimCursor = quad + 1;
                        _shelterB6TrainingRoomInitBeamCapWedge(quad, red, green, blue);
                        quad->x0 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        quad->y0 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        quad->x1 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sweepAngle + (SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN + SHELTER_B6_TRAINING_ROOM_BEAM_EIGHTH_TURN))) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        quad->y1 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sweepAngle + (SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN + SHELTER_B6_TRAINING_ROOM_BEAM_EIGHTH_TURN))) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        quad->x2 = scratch->pair.sx1;
                        quad->y2 = scratch->pair.sy1;
                        quad->x3 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sweepAngle + (SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN + SHELTER_B6_TRAINING_ROOM_BEAM_QUARTER_TURN))) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        quad->y3 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sweepAngle + (SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN + SHELTER_B6_TRAINING_ROOM_BEAM_QUARTER_TURN))) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->pair.otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                quad);
                        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->pair.otz1);

                        quad           = gGpuPrimCursor;
                        gGpuPrimCursor = quad + 1;
                        _shelterB6TrainingRoomInitBeamCapWedge(quad, red, green, blue);
                        quad->x0       = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(sweepAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        quad->y0       = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(sweepAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        quad->x1       = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_EIGHTH_TURN)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        quad->y1       = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_EIGHTH_TURN)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        nextSweepAngle = sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_QUARTER_TURN;
                        quad->x2       = scratch->pair.sx0;
                        quad->y2       = scratch->pair.sy0;
                        quad->x3       = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(nextSweepAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        quad->y3       = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(nextSweepAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->pair.otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                quad);
                        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->pair.otz0);

                        quad           = gGpuPrimCursor;
                        sideAngle      = baseAngle + (sweepAngle - baseAngle) * 2;
                        gGpuPrimCursor = quad + 1;
                        setPolyG4(quad);
                        setRGB0(quad, 0, 0, 0);
                        setRGB1(quad, 0, 0, 0);
                        setRGB2(quad, red, green, blue);
                        setRGB3(quad, red, green, blue);
                        quad->x0 = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(sideAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        quad->y0 = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(sideAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        quad->x1 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sideAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        quad->y1 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sideAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                        quad->x2 = scratch->pair.sx0;
                        quad->y2 = scratch->pair.sy0;
                        quad->x3 = scratch->pair.sx1;
                        quad->y3 = scratch->pair.sy1;
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->pair.otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                quad);
                        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->pair.otz1);
                        sweepAngle = nextSweepAngle;
                    } while (sweepAngle < sweepLimit);
                }

                _shelterB6TrainingRoomDrawGroundBeam(D_shelter_b6_training_room_80185C90, endCoord, radiusScale, colorIndex);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomBeamScratch);
}

/// Draws an additive capsule between the ground hits below two composed coordinates.
///
/// Borrows both coordinates, requiring both ground probes to return 1 and
/// both GTE projections to have nonnegative flags. Signed `radiusScale` gives
/// a pixel radius of radiusScale * 64 / (SZ3 / 4); depths must be nonzero.
/// `paletteOffset` is a halfword index into the beam palette, whose RGB
/// nibbles are doubled with a two-unit odd-frame flicker. Its caller passes
/// an already looked-up packed colour; the resulting lookup bounds are unproven.
/// Queues six Gouraud quads and additive blend commands; side quads sort at
/// the end depth. Releases its scratch block; packets stay in the frame arena.
static void _shelterB6TrainingRoomDrawGroundBeam(const GfxCoord* startCoord, const GfxCoord* endCoord, s16 radiusScale, u16 paletteOffset)
{
    GfxCoord         groundStart;
    GfxCoord         groundEnd;
    RoomBeamScratch* scratch;
    POLY_G4*         quad;
    u8               red;
    u8               green;
    u8               blue;
    s32              flicker;
    s32              scaledRadius;
    s32              sweepLimit;
    s32              baseAngle;
    s32              sweepAngle;
    s32              nextSweepAngle;
    s32              sideAngle;
    DisplayState*    display;

    if (worldCollisionProjectGroundCoord(startCoord, &groundStart) != 1 || worldCollisionProjectGroundCoord(endCoord, &groundEnd) != 1) {
        return;
    }
    scratch            = SCRATCH_STACK_RESERVE_BLOCK(RoomBeamScratch);
    scratch->point0.vx = groundStart.workm.t[0];
    scratch->point0.vy = groundStart.workm.t[1];
    scratch->point0.vz = groundStart.workm.t[2];
    scratch->point1.vx = groundEnd.workm.t[0];
    scratch->point1.vy = groundEnd.workm.t[1];
    scratch->point1.vz = groundEnd.workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->point0);
    gte_rtps();
    gte_stsxy(&scratch->pair.sx0);
    gte_stflg(&scratch->pair.flag);
    if (scratch->pair.flag >= 0) {
        gte_stszotz(&scratch->pair.otz0);
        gte_ldv0(&scratch->point1);
        gte_rtps();
        gte_stsxy(&scratch->pair.sx1);
        gte_stflg(&scratch->pair.flag);
        gte_stszotz(&scratch->pair.otz1);
        if (scratch->pair.flag >= 0) {
            paletteOffset         = D_shelter_b6_training_room_801843FC[paletteOffset];
            red                   = ((paletteOffset >> 8) & SHELTER_B6_TRAINING_ROOM_BEAM_RGB_NIBBLE_MASK) << 1;
            green                 = ((paletteOffset >> 4) & SHELTER_B6_TRAINING_ROOM_BEAM_RGB_NIBBLE_MASK) << 1;
            blue                  = (paletteOffset & SHELTER_B6_TRAINING_ROOM_BEAM_RGB_NIBBLE_MASK) << 1;
            display               = &gDisplayState;
            flicker               = ((u8)display->animFrame & 1) << 1;
            red                  += flicker;
            green                += flicker;
            blue                 += flicker;
            scaledRadius          = radiusScale << SHELTER_B6_TRAINING_ROOM_BEAM_RADIUS_SHIFT;
            scratch->pair.radius0 = scaledRadius / scratch->pair.otz0;
            scratch->pair.radius1 = scaledRadius / scratch->pair.otz1;
            sweepAngle            = (s16)ratan2((s16)scratch->pair.sy1 - (s16)scratch->pair.sy0, (s16)scratch->pair.sx0 - (s16)scratch->pair.sx1);
            if (sweepAngle < sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN) {
                baseAngle  = sweepAngle;
                sweepLimit = sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN;
                do {
                    quad           = gGpuPrimCursor;
                    gGpuPrimCursor = quad + 1;
                    _shelterB6TrainingRoomInitBeamCapWedge(quad, red, green, blue);
                    quad->x0 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    quad->y0 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    quad->x1 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sweepAngle + (SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN + SHELTER_B6_TRAINING_ROOM_BEAM_EIGHTH_TURN))) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    quad->y1 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sweepAngle + (SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN + SHELTER_B6_TRAINING_ROOM_BEAM_EIGHTH_TURN))) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    quad->x2 = scratch->pair.sx1;
                    quad->y2 = scratch->pair.sy1;
                    quad->x3 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sweepAngle + (SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN + SHELTER_B6_TRAINING_ROOM_BEAM_QUARTER_TURN))) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    quad->y3 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sweepAngle + (SHELTER_B6_TRAINING_ROOM_BEAM_HALF_TURN + SHELTER_B6_TRAINING_ROOM_BEAM_QUARTER_TURN))) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->pair.otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            quad);
                    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->pair.otz1);

                    quad           = gGpuPrimCursor;
                    gGpuPrimCursor = quad + 1;
                    _shelterB6TrainingRoomInitBeamCapWedge(quad, red, green, blue);
                    quad->x0       = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(sweepAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    quad->y0       = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(sweepAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    quad->x1       = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_EIGHTH_TURN)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    quad->y1       = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_EIGHTH_TURN)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    nextSweepAngle = sweepAngle + SHELTER_B6_TRAINING_ROOM_BEAM_QUARTER_TURN;
                    quad->x2       = scratch->pair.sx0;
                    quad->y2       = scratch->pair.sy0;
                    quad->x3       = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(nextSweepAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    quad->y3       = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(nextSweepAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->pair.otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            quad);
                    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->pair.otz0);

                    quad           = gGpuPrimCursor;
                    sideAngle      = baseAngle + (sweepAngle - baseAngle) * 2;
                    gGpuPrimCursor = quad + 1;
                    setPolyG4(quad);
                    setRGB0(quad, 0, 0, 0);
                    setRGB1(quad, 0, 0, 0);
                    setRGB2(quad, red, green, blue);
                    setRGB3(quad, red, green, blue);
                    quad->x0 = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(sideAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    quad->y0 = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(sideAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    quad->x1 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sideAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    quad->y1 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sideAngle)) >> SHELTER_B6_TRAINING_ROOM_BEAM_TRIG_SHIFT);
                    quad->x2 = scratch->pair.sx0;
                    quad->y2 = scratch->pair.sy0;
                    quad->x3 = scratch->pair.sx1;
                    quad->y3 = scratch->pair.sy1;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->pair.otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            quad);
                    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->pair.otz1);
                    sweepAngle = nextSweepAngle;
                } while (sweepAngle < sweepLimit);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomBeamScratch);
}

void shelterB6TrainingRoomChargeBurstTask(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_CHARGE_INIT              = 0,
        SHELTER_B6_TRAINING_ROOM_CHARGE_BUILD             = 1,
        SHELTER_B6_TRAINING_ROOM_CHARGE_FADE              = 2,
        SHELTER_B6_TRAINING_ROOM_CHARGE_TARGET_BRIGHTNESS = 0xC0,
        SHELTER_B6_TRAINING_ROOM_CHARGE_BURST_BRIGHTNESS  = 0xFF,
        SHELTER_B6_TRAINING_ROOM_CHARGE_BASE_RADIUS       = 0x100,
        SHELTER_B6_TRAINING_ROOM_CHARGE_BAND_PERIOD       = 15,
        SHELTER_B6_TRAINING_ROOM_CHARGE_FADE_STEP         = 4
    };
    EffectWork* work;
    GfxCoord*   coord;
    u8          rgb[3];

/// Draws both orange discs and leaves their RGB for the band or screen tint.
///
/// Borrows live work and a composed coordinate; rgb must point to three writable
/// bytes. Arguments must be side-effect-free because each is evaluated repeatedly.
/// Doubles the radius's raw halfword before narrowing to s16. Expands to statements
/// and must be used inside a braced block; no identifiers are captured.
#define SHELTER_B6_TRAINING_ROOM_DRAW_CHARGE_DISCS(effectWork, effectCoord, colorBytes) \
    (colorBytes)[0] = (effectWork)->scale;                                              \
    (colorBytes)[1] = (effectWork)->scale >> 1;                                         \
    (colorBytes)[2] = (effectWork)->scale >> 2;                                         \
    effectDrawGouraudDisc((effectCoord), (effectWork)->angle, (colorBytes));            \
    effectDrawGouraudDisc((effectCoord), (s16)((u16)(effectWork)->angle * 2), (colorBytes));

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        work->age++;
        switch (task->state) {
            case SHELTER_B6_TRAINING_ROOM_CHARGE_INIT:
                gfxSetRotIdentity(&coord->coord);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->scale         = 0;
                work->angle         = SHELTER_B6_TRAINING_ROOM_CHARGE_BASE_RADIUS;
                work->step          = SHELTER_B6_TRAINING_ROOM_CHARGE_TARGET_BRIGHTNESS / task->spawnArg1.value;
                if (work->step == 0) {
                    work->step = 1;
                }
                task->state = SHELTER_B6_TRAINING_ROOM_CHARGE_BUILD;
                // Initialization draws the first buildup tick immediately.
            case SHELTER_B6_TRAINING_ROOM_CHARGE_BUILD:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    SHELTER_B6_TRAINING_ROOM_DRAW_CHARGE_DISCS(work, coord, rgb);
                    effectDrawOuterGlowBand(coord, (s16)((task->spawnArg1.value % SHELTER_B6_TRAINING_ROOM_CHARGE_BAND_PERIOD) * (work->scale << 2)), SHELTER_B6_TRAINING_ROOM_CHARGE_BASE_RADIUS, rgb);
                    return;
                }
                work->scale += work->step;
                if (work->scale > SHELTER_B6_TRAINING_ROOM_CHARGE_TARGET_BRIGHTNESS) {
                    work->scale = SHELTER_B6_TRAINING_ROOM_CHARGE_TARGET_BRIGHTNESS;
                }
                work->angle = (u16)work->scale * 8 + SHELTER_B6_TRAINING_ROOM_CHARGE_BASE_RADIUS;
                task->spawnArg1.value--;
                SHELTER_B6_TRAINING_ROOM_DRAW_CHARGE_DISCS(work, coord, rgb);
                effectDrawOuterGlowBand(coord, (s16)((task->spawnArg1.value % SHELTER_B6_TRAINING_ROOM_CHARGE_BAND_PERIOD) * (work->scale << 2)), SHELTER_B6_TRAINING_ROOM_CHARGE_BASE_RADIUS, rgb);
                if (task->spawnArg1.value == 0) {
                    // The completed charge starts all three bands and the screen flash.
                    work->scale = SHELTER_B6_TRAINING_ROOM_CHARGE_BURST_BRIGHTNESS;
                    task->state = SHELTER_B6_TRAINING_ROOM_CHARGE_FADE;
                    effectSpawn(EFFECT_SHELTER_B6_TRAINING_ROOM_RING_WALL, coord, 0, NULL);
                    effectSpawn(EFFECT_SHELTER_B6_TRAINING_ROOM_RING_WALL, coord, 1, NULL);
                    effectSpawn(EFFECT_SHELTER_B6_TRAINING_ROOM_RING_WALL, coord, 2, NULL);
                    work->period = 0x600;
                    work->step   = 0;
                }
                return;
            case SHELTER_B6_TRAINING_ROOM_CHARGE_FADE:
                if (work->scale >= SHELTER_B6_TRAINING_ROOM_CHARGE_FADE_STEP + 1) {
                    SHELTER_B6_TRAINING_ROOM_DRAW_CHARGE_DISCS(work, coord, rgb);
                    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                        work->scale -= SHELTER_B6_TRAINING_ROOM_CHARGE_FADE_STEP;
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

#undef SHELTER_B6_TRAINING_ROOM_DRAW_CHARGE_DISCS
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

void shelterB6TrainingRoomDrawBodyGlow(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_BODY_GLOW_ANCHOR_JOINT       = 1,
        SHELTER_B6_TRAINING_ROOM_BODY_GLOW_FIRST_FLASH_JOINT  = 3,
        SHELTER_B6_TRAINING_ROOM_BODY_GLOW_FLASH_JOINT_MASK   = 15,
        SHELTER_B6_TRAINING_ROOM_BODY_GLOW_FLASH_CHANCE_MASK  = 3,
        SHELTER_B6_TRAINING_ROOM_BODY_GLOW_BASE_BRIGHTNESS    = 0x40,
        SHELTER_B6_TRAINING_ROOM_BODY_GLOW_INNER_RADIUS       = 0x200,
        SHELTER_B6_TRAINING_ROOM_BODY_GLOW_OUTER_RADIUS       = 0x400,
        SHELTER_B6_TRAINING_ROOM_BODY_GLOW_FLASH_SIZE_PALETTE = (1 << 16) | 0x80
    };
    GfxCoord* anchorCoord;
    u8        rgb[3];
    u32       brightness;

    anchorCoord = task->extra.tmd->coords + SHELTER_B6_TRAINING_ROOM_BODY_GLOW_ANCHOR_JOINT;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        D_shelter_b6_training_room_80185C94 = anchorCoord;
        brightness                          = ((gDisplayState.animFrame & 1) << 4) + SHELTER_B6_TRAINING_ROOM_BODY_GLOW_BASE_BRIGHTNESS;
        rgb[0]                              = brightness;
        rgb[1]                              = brightness;
        rgb[2]                              = brightness >> 1;
        effectDrawGouraudDisc(anchorCoord, SHELTER_B6_TRAINING_ROOM_BODY_GLOW_INNER_RADIUS, rgb);
        effectDrawGouraudDisc(anchorCoord, SHELTER_B6_TRAINING_ROOM_BODY_GLOW_OUTER_RADIUS, rgb);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & SHELTER_B6_TRAINING_ROOM_BODY_GLOW_FLASH_CHANCE_MASK) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_FLASH_BURST, task->extra.tmd->coords + (((gRandomLcgState >> 16) & SHELTER_B6_TRAINING_ROOM_BODY_GLOW_FLASH_JOINT_MASK) + SHELTER_B6_TRAINING_ROOM_BODY_GLOW_FIRST_FLASH_JOINT), SHELTER_B6_TRAINING_ROOM_BODY_GLOW_FLASH_SIZE_PALETTE, NULL);
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

void shelterB6TrainingRoomDescendingSpriteTask(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_INIT               = 0,
        SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_DRAW               = 1,
        SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_Y_STEP             = 32,
        SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_LIFETIME           = 60,
        SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_FRAME_MASK         = 3,
        SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_SIZE_BANK          = 0x300,
        SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_BRIGHTNESS_PALETTE = 0x80,
        SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_SHED_CHANCE_MASK   = 3
    };
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        work->age++;
        if (task->state == SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_INIT) {
            work->move.vy = SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_Y_STEP;
            work->scale   = SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_BRIGHTNESS_PALETTE;
            work->move.vx = 0;
            work->move.vz = 0;
            task->state   = SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_DRAW;
        }
        coord->coord.t[1]  += work->move.vy;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        // Draw the cached position; marking the local movement dirty does not refresh it.
        if (work->age < SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_LIFETIME) {
            if (work->age & 1) {
                work->index = (work->index + 1) & SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_FRAME_MASK;
                effectDrawModulatedBillboard(coord, work->index, SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_SIZE_BANK, SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_BRIGHTNESS_PALETTE);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & SHELTER_B6_TRAINING_ROOM_DESCENDING_SPRITE_SHED_CHANCE_MASK) == 0) {
                    effectSpawn(EFFECT_1AD, coord, 0, NULL);
                }
            }
        } else {
            effectKillTask(work, task);
        }
    }
}

void shelterB6TrainingRoomHealSpiralTask(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_EMISSION_COUNT  = 21,
        SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_CENTRE_INDEX    = 24,
        SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_RADIUS          = 1000,
        SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_RISE_STEP       = 200,
        SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_PHASE_STEP_MIN  = ONE / 8,
        SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_PHASE_STEP_MASK = ONE / 8 - 1,
        SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_TRIG_SHIFT      = 12
    };
    EffectWork* work;

    work = task->spawnArg2.pointer;
    if (work->age >= SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_EMISSION_COUNT) {
        effectKillTask(work, task);
        return;
    }
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        work->age++;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        // The scale slot holds a wrapping 16-bit angle here, and move holds the next world position.
        work->scale  += ((gRandomLcgState >> 16) & SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_PHASE_STEP_MASK) + SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_PHASE_STEP_MIN;
        work->move.vx = D_shelter_b6_training_room_80184334[SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_CENTRE_INDEX].vx + ((rcos(work->scale) * SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_RADIUS) >> SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_TRIG_SHIFT);
        work->move.vy = D_shelter_b6_training_room_80184334[SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_CENTRE_INDEX].vy - work->age * SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_RISE_STEP;
        work->move.vz = D_shelter_b6_training_room_80184334[SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_CENTRE_INDEX].vz + ((rsin(work->scale) * SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_RADIUS) >> SHELTER_B6_TRAINING_ROOM_HEAL_SPIRAL_TRIG_SHIFT);
        effectSpawn(EFFECT_1AE, NULL, 0, &work->move);
    }
}

void shelterB6TrainingRoomSpawnShieldArcs(Task* task)
{
    enum {
        SHELTER_B6_TRAINING_ROOM_SHIELD_ARC_CHANCE_MASK = 7,
        SHELTER_B6_TRAINING_ROOM_SHIELD_ARC_FIRST_JOINT = 1,
        SHELTER_B6_TRAINING_ROOM_SHIELD_ARC_JOINT_COUNT = 18
    };
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & SHELTER_B6_TRAINING_ROOM_SHIELD_ARC_CHANCE_MASK) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_TRAINING_ROOM_ENERGY_ARC, task->extra.tmd->coords + ((u16)((gRandomLcgState >> 16) % SHELTER_B6_TRAINING_ROOM_SHIELD_ARC_JOINT_COUNT) + SHELTER_B6_TRAINING_ROOM_SHIELD_ARC_FIRST_JOINT), 0, NULL);
        }
    }
}

void shelterB6TrainingRoomSetPartDestroyedSprites(u8 partSlot, u8 destroyed)
{
    enum { SHELTER_B6_TRAINING_ROOM_PART_INTACT    = 0,
           SHELTER_B6_TRAINING_ROOM_PART_DESTROYED = 1 };
    GameLocationKey* location = &gGameSession->location.loc;
    SpriteView*      views    = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];
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
