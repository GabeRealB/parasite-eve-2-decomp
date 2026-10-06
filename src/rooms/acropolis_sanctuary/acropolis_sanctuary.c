#include "rooms/acropolis_sanctuary.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_210700.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/actor_contacts.h"

extern AnimationSet* D_acropolis_sanctuary_80180918[9];

extern WorldCollisionTrigger D_acropolis_sanctuary_80183AE4[17];

/// Script-selected phases and placement status of the sanctuary cutscene.
enum {
    ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_INITIAL      = 0,
    ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_HOLD         = 1,
    ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_PLACE_PLAYER = 2,
    ACROPOLIS_SANCTUARY_CUTSCENE_PLACEMENT_PENDING  = 0,
    ACROPOLIS_SANCTUARY_CUTSCENE_PLACEMENT_APPLIED  = 1,
};

/// Task-owned state for the sanctuary cutscene's script-cued player placement.
///
/// Stored in `Task::work` on the primary heap and freed by default task teardown.
/// Script requests require the initialized cutscene task to remain live; the
/// borrowed player task must remain live through its animation and placement
/// messages. Each placement request resets the one-shot status.
typedef struct {
    Task* playerTask;       // Borrowed player task captured at creation
    u16   phase;            // Script phase (0 initial, 1 hold, 2 place player)
    u16   placementApplied; // Player setup status (0 pending, 1 applied)
    u8    field_8[4];       // Cleared at creation; role and field boundaries unproven
} _AcropolisSanctuaryCutsceneWork;
STATIC_ASSERT_SIZEOF(_AcropolisSanctuaryCutsceneWork, 0xC);

/// One tile of the sanctuary's mosaic: the piece of the mosaic sheet it shows
/// and how it comes away from the wall.
///
/// The sheet is 180 x 256 texels, covered without gaps by 15 x 16 texel cells
/// and 30 x 32 blocks of four. A tile's texel origin is also its place on the
/// wall, a texel being 2147/256 world units wide and 1145/128 tall, so the
/// mosaic is laid out exactly as it is drawn on the sheet. The middle of the
/// mosaic is thrown clear at once; the tiles around it stay in place longer the
/// further out they sit, then work loose and crumble.
typedef struct {
    s16 extentU;    // Texels from `originU` to the tile's last column (14 or 29)
    s16 extentV;    // Texels from `originV` to the tile's last row (15 or 31)
    s16 originU;    // Left texel column on the mosaic sheet, 0..165
    s16 originV;    // Top texel row on the mosaic sheet, 0..240
    s16 thrown;     // How the tile leaves (0 works loose slowly and sheds shards, 1 thrown clear)
    s16 holdFrames; // Frames the tile stays in place before it moves; 0 when thrown
    s16 sizeClass;  // Tile size (0 one 15 x 16 texel cell, 1 a 30 x 32 block)
} _AcropolisSanctuaryMosaicTile;
STATIC_ASSERT_SIZEOF(_AcropolisSanctuaryMosaicTile, 0xE);

/// Scratch-stack block one mosaic shard's triangle is drawn from.
///
/// Each corner is staged in `corners` as one of a mosaic tile's corner offsets,
/// in the shard's own frame, and replaced in place twice: first by that offset
/// scaled to the shard's size, then by the corner's world position, narrowed to
/// signed 16-bit coordinate units. One RTPT then projects the three together;
/// the screen positions go straight into the packet, so the block keeps none of
/// them.
///
/// The block is one word longer than the words the drawer uses, and the room's
/// whole-tile block carries the same spare word after its four corners.
///
/// Reserve the whole block and release it before the drawer returns; no pointer
/// into it survives release.
typedef struct {
    s32     otz;         // SZ3 / 4 of the projection, a quarter of the last corner's depth; draw threshold and ordering-table depth
    SVECTOR corners[3];  // Local corner workspace, then the world positions supplied to the projection
    u8      field_1C[4]; // Reserved with the block but never read or written; role and field boundaries unproven
} _AcropolisSanctuaryMosaicShardScratch;
STATIC_ASSERT_SIZEOF(_AcropolisSanctuaryMosaicShardScratch, 0x20);

/// Scratch-stack block one whole mosaic tile's quad is drawn from.
///
/// Each corner is staged in `corners` as one of the tile's size-class corner
/// offsets, in the tile's own frame, and replaced in place by the corner's
/// world position, narrowed to signed 16-bit coordinate units. An RTPS for the
/// first corner and one RTPT for the other three then project them; the screen
/// positions go straight into the packet, so the block keeps none of them.
/// Unlike `_AcropolisSanctuaryMosaicShardScratch` the corners are never scaled:
/// an intact tile is always drawn at its size class's own dimensions.
///
/// The block is one word longer than the words the drawer uses, as the shard
/// block is after its three corners.
///
/// Reserve the whole block and release it before the drawer returns; no pointer
/// into it survives release.
typedef struct {
    s32     otz;         // SZ3 / 4 of the projection, a quarter of the last corner's depth; draw threshold and ordering-table depth
    SVECTOR corners[4];  // Local corner workspace, then the world positions supplied to the projection
    u8      field_24[4]; // Reserved with the block but never read or written; role and field boundaries unproven
} _AcropolisSanctuaryMosaicTileScratch;
STATIC_ASSERT_SIZEOF(_AcropolisSanctuaryMosaicTileScratch, 0x28);

/// Grey levels of the sanctuary flame sprite, one byte per variant.
///
/// Bits 8..9 of the flame's spawn argument select the variant. That bitfield
/// is two bits wide, but only variants 0, 1 and 2 have a byte here. Variants
/// 0 and 1 store the same level and variant 2 is dimmer; the twelve flames
/// this room places select 0 and 2. The overlay keeps two tables, the resting
/// grey and the amount added on odd frames. The drawer copies a table and
/// then reads one byte, so the padding after the table is not a fourth level.
typedef struct {
    u8 grey[3]; // 0..255, written to R, G and B; variants 0 and 1 bright, 2 dim
} _AcropolisSanctuaryFlameGrey;
STATIC_ASSERT_SIZEOF(_AcropolisSanctuaryFlameGrey, 3);

/// `gPlayerStatus.weapon` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on,
/// `gDisplayState.pendingMode` and `Gp_StateC08.mode` gate the cutscene task's setup (the latter is 1 while the attachment wheel is open) and `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` picks which of the two
/// weapon-id bases that record uses. `gDisplayState.spriteVariant` is set to 1 alongside the
/// save writes when the task hands off to task 0x11, the same way the fountain
/// and helicopter-pad rooms set it.

extern TaskMessageEntry              D_acropolis_sanctuary_8018081C[];
extern ActorTransform                D_acropolis_sanctuary_801808BC;
extern AnimationPlayRequest          D_acropolis_sanctuary_801809F8;
extern AnimationPlayRequest          D_acropolis_sanctuary_80180A0C;
extern AnimationPlayRequest          D_acropolis_sanctuary_80180AE8;
extern EvsCommand                    D_acropolis_sanctuary_80180B0C[];
extern EvsCommand                    D_acropolis_sanctuary_80181664[];
extern EvsCommand                    D_acropolis_sanctuary_80181814[];
extern TaskDesc                      D_acropolis_sanctuary_80182240;
extern WorldCollisionGrid            D_acropolis_sanctuary_801822EC;
extern TaskMessageEntry              D_acropolis_sanctuary_80182310[];
extern _AcropolisSanctuaryMosaicTile D_acropolis_sanctuary_80182320[];
extern SVECTOR                       D_acropolis_sanctuary_80182710[][4];
extern s16                           D_acropolis_sanctuary_80182750[];
extern s32                           D_acropolis_sanctuary_80182770;
extern SVECTOR                       D_acropolis_sanctuary_80182774[];
extern u16                           D_acropolis_sanctuary_801827D4[];
extern WorldCollisionGrid            D_acropolis_sanctuary_80183568;
extern AreaApplyRec                  D_acropolis_sanctuary_80186418[];
extern Task*                         D_acropolis_sanctuary_80186C90;

/// Whole-unit X/Y/Z displacement left by the last call of
/// `ActorContact_PushContact`.
extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

/// Payloads the sanctuary cutscene task sends: `..._801820E4` is the record
/// slot-3 msg 0x3F4 takes and `..._801820F0` / `..._801821C8` the script pair
/// `func_800E8634` is started on.
extern AnimationSet* D_acropolis_sanctuary_801820E4[1];
extern EvsCommand    D_acropolis_sanctuary_801820F0[];
extern EvsCommand    D_acropolis_sanctuary_801821C8[];

static void func_acropolis_sanctuary_8017D5E0(Task* task);
static void func_acropolis_sanctuary_8017D930(Task* arg0);
static void func_acropolis_sanctuary_8017DD78(void);
static void _acropolisSanctuarySelectViewSpriteBatch(s32 useSecondBatch, s32 viewId);

/// Key-item use message handled by this room's task.
enum { ACROPOLIS_SANCTUARY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Unsigned byte fields in a script's packed view-sprite selection.
enum {
    ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_FIELD_MASK  = 0xFF,
    ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_BATCH_SHIFT = 8,
};

/// Sets a flame billboard's square screen bounds around its projected centre.
///
/// Borrows a writable `flameQuad` and read-only `flameScratch` for this call.
/// Initialize `screenPos` and a nonnegative `halfExtent` in pixels; their sums
/// and differences must fit signed 32-bit arithmetic. Each edge narrows to a
/// signed 16-bit coordinate before its paired stores. Vertex indices 0..3 are
/// top-left, top-right, bottom-left and bottom-right. Writes only X/Y coordinates.
static inline void _acropolisSanctuarySetFlameQuadBounds(POLY_FT4* flameQuad, const RoomGlowSpriteScratch* flameScratch)
{
    s16 left;
    s16 right;
    s16 top;
    s16 bottom;

    left          = flameScratch->screenPos.vx - flameScratch->halfExtent;
    flameQuad->x0 = flameQuad->x2 = left;

    right         = flameScratch->screenPos.vx + flameScratch->halfExtent;
    flameQuad->x1 = flameQuad->x3 = right;

    top           = flameScratch->screenPos.vy - flameScratch->halfExtent;
    flameQuad->y0 = flameQuad->y1 = top;

    bottom        = flameScratch->screenPos.vy + flameScratch->halfExtent;
    flameQuad->y2 = flameQuad->y3 = bottom;
}

/// State handlers of the room task: set-up, the per-frame entry fixup and
/// `taskKill`.
static const TaskFuncTable3 D_acropolis_sanctuary_8017D5C4 = {
    { func_acropolis_sanctuary_8017D930, func_acropolis_sanctuary_8017D5E0, taskKill },
};

/// Offset the room task's model-coordinate effect is spawned with.
static const SVECTOR D_acropolis_sanctuary_8017D5D0 = { -0x27F6, -0x17CA, -0x1C3E, 0 };

/// Resting grey of the sanctuary flame, one byte per variant. The next
/// constant is word-aligned, so a zero byte follows these three.
static const _AcropolisSanctuaryFlameGrey D_acropolis_sanctuary_8017D5D8 = { { 0x60, 0x60, 0x10 } };
/// Grey added to the resting level on odd frames, one byte per variant.
static const _AcropolisSanctuaryFlameGrey D_acropolis_sanctuary_8017D5DC = { { 0x10, 0x10, 0x08 } };
/// A non-zero padding byte the original toolchain left. Nothing refers to it.
static const u8 D_acropolis_sanctuary_8017D5DF = 0xF1;

s32 func_acropolis_sanctuary_8017F918(Task*, s32, s32, s32);

void func_acropolis_sanctuary_8017DA40(Task*);

void func_acropolis_sanctuary_8017DCE0(s32);

extern WorldCollisionGrid    D_acropolis_sanctuary_80183568;
extern WorldCollisionTrigger D_acropolis_sanctuary_8018358C[18];
extern WorldCoordRoomLights  D_acropolis_sanctuary_801843EC[1];

extern AnimationPlayRequest     D_acropolis_sanctuary_80180904;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180944;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180958;
extern AnimationPlayRequest     D_acropolis_sanctuary_8018096C;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180980;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180994;
extern AnimationPlayRequest     D_acropolis_sanctuary_801809A8;
extern AnimationPlayRequest     D_acropolis_sanctuary_801809BC;
extern AnimationPlayRequest     D_acropolis_sanctuary_801809D0;
extern AnimationPlayRequest     D_acropolis_sanctuary_801809E4;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180A20;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180A34;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180A48;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180A5C;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180A70;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180A84;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180A98;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180AAC;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180AC0;
extern AnimationPlayRequest     D_acropolis_sanctuary_80180AD4;
extern AnimationBankCopyRequest D_acropolis_sanctuary_8018093C;
extern EvsSceneKey              D_acropolis_sanctuary_80180AFC;
extern ActorTransform           D_acropolis_sanctuary_80180844;
extern ActorTransform           D_acropolis_sanctuary_8018085C;
extern ActorTransform           D_acropolis_sanctuary_8018088C;
extern ActorTransform           D_acropolis_sanctuary_801808A4;
extern ActorTransform           D_acropolis_sanctuary_801808D4;
extern ActorTransform           D_acropolis_sanctuary_801808EC;
static void                     _acropolisSanctuaryApplyScriptSpriteSelection(u32 packedSelection);
void                            func_acropolis_sanctuary_8017D8CC(void);

s32        func_acropolis_sanctuary_8017D73C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32 _acropolisSanctuaryRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
s32        func_acropolis_sanctuary_8017D810(Task*, s32, s32, s32);
s32        func_acropolis_sanctuary_8017D848(Task*, s32, RoomEventMsg*, RoomEventMsg*);

static AnimationPackedPose _gAcropolisSanctuaryAnimation03234Bank1[8] = {
#include "assets/acropolis_sanctuary_animation_03234_bank1.inc"
};

static AnimationPackedRotation _gAcropolisSanctuaryAnimation03234Bank4[115] = {
#include "assets/acropolis_sanctuary_animation_03234_bank4.inc"
};

static AnimationRecord _gAcropolisSanctuaryAnimation03234Records[150] = {
#include "assets/acropolis_sanctuary_animation_03234_records.inc"
};

static u16 _gAcropolisSanctuaryAnimation03234Indices[20] = {
#include "assets/acropolis_sanctuary_animation_03234_indices.inc"
};

static AnimationSet _gAcropolisSanctuaryAnimation03234 = {
    _gAcropolisSanctuaryAnimation03234Records,
    _gAcropolisSanctuaryAnimation03234Indices,
    { NULL, _gAcropolisSanctuaryAnimation03234Bank1, NULL, NULL, _gAcropolisSanctuaryAnimation03234Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_acropolis_sanctuary_8018081C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_sanctuary_8017D73C },
    { ROOM_MESSAGE_COMMAND, func_acropolis_sanctuary_8017D810 },
    { ACROPOLIS_SANCTUARY_MESSAGE_USE_KEY_ITEM, _acropolisSanctuaryRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_acropolis_sanctuary_8017D848 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActorTransform D_acropolis_sanctuary_80180844 = { { -9700, 0, -7910, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_acropolis_sanctuary_8018085C = { 0 };

ActorTransform D_acropolis_sanctuary_80180874 = { { 0, 0, -900, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_acropolis_sanctuary_8018088C = { { -100, 0, -400, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_sanctuary_801808A4 = { { -100, 0, -400, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_sanctuary_801808BC = { { 0, 0, 480, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_sanctuary_801808D4 = { { 0, 0, -900, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_sanctuary_801808EC = { { -5724, 0, -8276, 0 }, { 0, -1024, 0, 0 } };

AnimationPlayRequest D_acropolis_sanctuary_80180904 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationSet* D_acropolis_sanctuary_80180918[9] = {
    &gActor210700Animation01E2C,
    &gActor210700Animation027F8,
    &gActor210700Animation02B54,
    &gActor210700Animation02E0C,
    &gActor210700Animation0318C,
    &gActor210700Animation03424,
    &gActor210700Animation03BD0,
    &gActor210700Animation067E8,
    &_gAcropolisSanctuaryAnimation03234,
};

AnimationBankCopyRequest D_acropolis_sanctuary_8018093C = { { .sets = D_acropolis_sanctuary_80180918 }, ARRAY_SIZE(D_acropolis_sanctuary_80180918) };

AnimationPlayRequest D_acropolis_sanctuary_80180944 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180958 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_8018096C = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180980 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180994 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_801809A8 = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_801809BC = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_801809D0 = { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_801809E4 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_801809F8 = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A0C = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A20 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A34 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A48 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A5C = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A70 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A84 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A98 = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180AAC = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180AC0 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180AD4 = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180AE8 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsSceneKey D_acropolis_sanctuary_80180AFC = { 1, 7, 11 };

EvsSceneKey D_acropolis_sanctuary_80180B04 = { 1, 7, 21 };

EvsCommand D_acropolis_sanctuary_80180B0C[121] = {
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 61 }, { .value = 64 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_sanctuary_8018085C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180904 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_acropolis_sanctuary_8018093C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_acropolis_sanctuary_80180AFC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = _acropolisSanctuaryApplyScriptSpriteSelection }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = _acropolisSanctuaryApplyScriptSpriteSelection }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180A70 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180A20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_sanctuary_80180844 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_acropolis_sanctuary_8017D8CC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180A84 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180A34 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180A48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180A5C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_sanctuary_8018085C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180944 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = _acropolisSanctuaryApplyScriptSpriteSelection }, { .value = (1 << ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_BATCH_SHIFT) | 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = _acropolisSanctuaryApplyScriptSpriteSelection }, { .value = (1 << ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_BATCH_SHIFT) | 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_ROOM_EFFECT }, { .value = 0 }, { .value = 3101 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_sanctuary_8018088C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180958 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_sanctuary_801808BC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180AAC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 73 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_sanctuary_801808A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_8018096C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180980 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180AC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180AD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_8018096C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180980 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180AC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180AD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_8018096C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180980 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180AC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180AD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180994 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 71 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809BC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_sanctuary_801808D4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 56 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809E4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180AD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180904 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_sanctuary_801808EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_sanctuary_80181664[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180904 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_sanctuary_801808EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_sanctuary_801808BC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180AE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_sanctuary_80181814[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180904 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180AC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180AD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_sanctuary_80180A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static AnimationPackedPose _gAcropolisSanctuaryAnimation04708Bank1[7] = {
#include "assets/acropolis_sanctuary_animation_04708_bank1.inc"
};

static AnimationPackedRotation _gAcropolisSanctuaryAnimation04708Bank4[84] = {
#include "assets/acropolis_sanctuary_animation_04708_bank4.inc"
};

static AnimationRecord _gAcropolisSanctuaryAnimation04708Records[120] = {
#include "assets/acropolis_sanctuary_animation_04708_records.inc"
};

static u16 _gAcropolisSanctuaryAnimation04708Indices[20] = {
#include "assets/acropolis_sanctuary_animation_04708_indices.inc"
};

static AnimationSet _gAcropolisSanctuaryAnimation04708 = {
    _gAcropolisSanctuaryAnimation04708Records,
    _gAcropolisSanctuaryAnimation04708Indices,
    { NULL, _gAcropolisSanctuaryAnimation04708Bank1, NULL, NULL, _gAcropolisSanctuaryAnimation04708Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisSanctuaryAnimation04AFCBank1[8] = {
#include "assets/acropolis_sanctuary_animation_04AFC_bank1.inc"
};

static AnimationPackedRotation _gAcropolisSanctuaryAnimation04AFCBank4[88] = {
#include "assets/acropolis_sanctuary_animation_04AFC_bank4.inc"
};

static AnimationRecord _gAcropolisSanctuaryAnimation04AFCRecords[121] = {
#include "assets/acropolis_sanctuary_animation_04AFC_records.inc"
};

static u16 _gAcropolisSanctuaryAnimation04AFCIndices[20] = {
#include "assets/acropolis_sanctuary_animation_04AFC_indices.inc"
};

static AnimationSet _gAcropolisSanctuaryAnimation04AFC = {
    _gAcropolisSanctuaryAnimation04AFCRecords,
    _gAcropolisSanctuaryAnimation04AFCIndices,
    { NULL, _gAcropolisSanctuaryAnimation04AFCBank1, NULL, NULL, _gAcropolisSanctuaryAnimation04AFCBank4, NULL, NULL, NULL },
};

AnimationSet* D_acropolis_sanctuary_801820E4[1] = {
    &_gAcropolisSanctuaryAnimation04708,
};

EvsSceneKey D_acropolis_sanctuary_801820E8 = { 1, 14, 11 };

EvsCommand D_acropolis_sanctuary_801820F0[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_sanctuary_8017DCE0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_sanctuary_8017DCE0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_sanctuary_801821C8[5] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_acropolis_sanctuary_80182240 = { { { TASK_BODY_NONE, 192 } }, func_acropolis_sanctuary_8017DA40, { .value = 0 } };

static SVECTOR _gAcropolisSanctuaryCollision04D2CNormals[4] = {
#include "assets/acropolis_sanctuary_collision_04D2C_normals.inc"
};

static SVECTOR _gAcropolisSanctuaryCollision04D2CVerts[8] = {
#include "assets/acropolis_sanctuary_collision_04D2C_verts.inc"
};

static WorldCollisionGridFace _gAcropolisSanctuaryCollision04D2CFaces[4] = {
#include "assets/acropolis_sanctuary_collision_04D2C_faces.inc"
};

static s16 _gAcropolisSanctuaryCollision04D2CCells[6] = {
#include "assets/acropolis_sanctuary_collision_04D2C_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisSanctuaryCollision04D2CCells[i])
static s16* _gAcropolisSanctuaryCollision04D2CTable[1] = {
#include "assets/acropolis_sanctuary_collision_04D2C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_sanctuary_801822EC = { NULL, _gAcropolisSanctuaryCollision04D2CNormals, _gAcropolisSanctuaryCollision04D2CVerts, _gAcropolisSanctuaryCollision04D2CFaces, _gAcropolisSanctuaryCollision04D2CTable, 4941, 8914, 1, 1, 4000, 4 };

TaskMessageEntry D_acropolis_sanctuary_80182310[2] = {
    { 3101, func_acropolis_sanctuary_8017F918 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

_AcropolisSanctuaryMosaicTile D_acropolis_sanctuary_80182320[72] = {
    { 29, 31, 0, 0, 0, 24, 1 },
    { 29, 31, 30, 0, 0, 16, 1 },
    { 14, 15, 60, 0, 0, 16, 0 },
    { 14, 15, 75, 0, 0, 16, 0 },
    { 14, 15, 90, 0, 0, 16, 0 },
    { 14, 15, 105, 0, 0, 16, 0 },
    { 29, 31, 120, 0, 0, 16, 1 },
    { 29, 31, 150, 0, 0, 24, 1 },
    { 29, 31, 60, 16, 0, 8, 1 },
    { 29, 31, 90, 16, 0, 8, 1 },
    { 29, 31, 0, 32, 0, 16, 1 },
    { 29, 31, 30, 32, 0, 8, 1 },
    { 29, 31, 120, 32, 0, 8, 1 },
    { 29, 31, 150, 32, 0, 16, 1 },
    { 29, 31, 60, 48, 1, 0, 1 },
    { 29, 31, 90, 48, 1, 0, 1 },
    { 29, 31, 0, 64, 0, 8, 1 },
    { 29, 31, 30, 64, 1, 0, 1 },
    { 29, 31, 120, 64, 1, 0, 1 },
    { 29, 31, 150, 64, 0, 8, 1 },
    { 14, 15, 60, 80, 1, 0, 0 },
    { 29, 31, 75, 80, 1, 0, 1 },
    { 14, 15, 105, 80, 1, 0, 0 },
    { 14, 15, 0, 96, 0, 8, 0 },
    { 29, 31, 15, 96, 1, 0, 1 },
    { 14, 15, 45, 96, 1, 0, 0 },
    { 14, 15, 60, 96, 1, 0, 0 },
    { 14, 15, 105, 96, 1, 0, 0 },
    { 14, 15, 120, 96, 1, 0, 0 },
    { 29, 31, 135, 96, 1, 0, 1 },
    { 14, 15, 165, 96, 0, 8, 0 },
    { 14, 15, 0, 112, 0, 16, 0 },
    { 29, 31, 45, 112, 1, 0, 1 },
    { 14, 15, 75, 112, 1, 0, 0 },
    { 14, 15, 90, 112, 1, 0, 0 },
    { 29, 31, 105, 112, 1, 0, 1 },
    { 14, 15, 165, 112, 0, 16, 0 },
    { 14, 15, 0, 128, 0, 24, 0 },
    { 29, 31, 15, 128, 1, 0, 1 },
    { 14, 15, 75, 128, 1, 0, 0 },
    { 14, 15, 90, 128, 1, 0, 0 },
    { 29, 31, 135, 128, 1, 0, 1 },
    { 14, 15, 165, 128, 0, 24, 0 },
    { 14, 15, 0, 144, 0, 16, 0 },
    { 14, 15, 45, 144, 1, 0, 0 },
    { 14, 15, 60, 144, 1, 0, 0 },
    { 29, 31, 75, 144, 1, 0, 1 },
    { 14, 15, 105, 144, 1, 0, 0 },
    { 14, 15, 120, 144, 1, 0, 0 },
    { 14, 15, 165, 144, 0, 16, 0 },
    { 29, 31, 0, 160, 0, 8, 1 },
    { 29, 31, 30, 160, 1, 0, 1 },
    { 14, 15, 60, 160, 1, 0, 0 },
    { 14, 15, 105, 160, 1, 0, 0 },
    { 29, 31, 120, 160, 1, 0, 1 },
    { 29, 31, 150, 160, 0, 8, 1 },
    { 29, 31, 60, 178, 1, 0, 1 },
    { 29, 31, 90, 178, 1, 0, 1 },
    { 29, 31, 0, 192, 0, 16, 1 },
    { 29, 31, 30, 192, 1, 0, 1 },
    { 29, 31, 120, 192, 1, 0, 1 },
    { 29, 31, 150, 192, 0, 16, 1 },
    { 29, 31, 60, 208, 1, 0, 1 },
    { 29, 31, 90, 208, 1, 0, 1 },
    { 29, 31, 0, 224, 0, 24, 1 },
    { 29, 31, 30, 224, 0, 8, 1 },
    { 29, 31, 120, 224, 0, 8, 1 },
    { 29, 31, 150, 224, 0, 24, 1 },
    { 14, 15, 60, 240, 0, 8, 0 },
    { 14, 15, 75, 240, 0, 16, 0 },
    { 14, 15, 90, 240, 0, 16, 0 },
    { 14, 15, 105, 240, 0, 8, 0 },
};

/// Corner offsets of the sanctuary mosaic's two tile sizes, measured from the
/// tile's centre.
///
/// The first index is `_AcropolisSanctuaryMosaicTile::sizeClass`: 0 spans a 15 x 16
/// texel cell of the mosaic sheet and 1 a 30 x 32 one, twice as large each way.
/// The second is the corner, in `POLY_FT4` vertex order. The quad lies in the
/// tile's local YZ plane, with the sheet's u running towards -z and its v
/// towards +y, so corner 0 is the one at the tile's texel origin; subtracting
/// it turns that origin's place on the wall into the centre a tile is spawned at. A shard is the
/// triangle of the first three corners of class 0, scaled by the shard's size.
SVECTOR D_acropolis_sanctuary_80182710[2][4] = {
    { { 0, -72, 63, 0 }, { 0, -72, -62, 0 }, { 0, 71, 63, 0 }, { 0, 71, -62, 0 } },
    { { 0, -143, 126, 0 }, { 0, -143, -125, 0 }, { 0, 143, 126, 0 }, { 0, 143, -125, 0 } },
};

s16 D_acropolis_sanctuary_80182750[16] = {
    20,
    22,
    25,
    26,
    27,
    28,
    33,
    34,
    39,
    40,
    44,
    45,
    47,
    48,
    52,
    53,
};

s32 D_acropolis_sanctuary_80182770 = 0;

SVECTOR D_acropolis_sanctuary_80182774[12] = {
    { -0x2738, -2088, -6680, 0 },
    { -0x2738, -2088, -9280, 0 },
    { -2720, -2088, -6460, 0 },
    { -2720, -2088, -9570, 0 },
    { -6690, -2088, -4220, 0 },
    { -8310, -2028, -4220, 0 },
    { -2420, -1630, -7210, 0 },
    { -2420, -1680, -7330, 0 },
    { -2420, -1630, -7450, 0 },
    { -2420, -1630, -8490, 0 },
    { -2420, -1680, -8600, 0 },
    { -2420, -1630, -8730, 0 },
};

u16 D_acropolis_sanctuary_801827D4[12] = {
    2626,
    2130,
    5420,
    300,
    0x3460,
    0x3440,
    4524,
    4524,
    4524,
    428,
    428,
    428,
};

WorldCollisionRoomResources D_acropolis_sanctuary_801827EC[1] = {
    { &D_acropolis_sanctuary_80183568, D_acropolis_sanctuary_8018358C, D_acropolis_sanctuary_80183AE4, NULL },
};

u8* D_acropolis_sanctuary_801827FC[1] = {
    gViewIdentityMap,
};

ViewCount D_acropolis_sanctuary_80182800[1] = { 16 };

WorldCoordRoomLighting D_acropolis_sanctuary_80182804[1] = {
    { D_acropolis_sanctuary_801843EC, NULL },
};

DirectionWarpEntry D_acropolis_sanctuary_8018280C[3] = {
    { { { .word = 1024 }, -9757, 2, -8070 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -9757, 2, -8070 }, { 0, 0, 0, 0 }, 0x510C0002, 0x510C0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 493 },
    { { { .word = 2048 }, -7465, 2, -4239 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -7465, 2, -4239 }, { 0, 0, 0, 0 }, 0x510C0004, 0x510C0003, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, 492 },
    { { { .word = 1024 }, -9757, 2, -8070 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -9757, 2, -8070 }, { 0, 0, 0, 0 }, 0x510C0002, 0x510C0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 493 },
};

static SVECTOR _gAcropolisSanctuaryCollision05FA8Normals[42] = {
#include "assets/acropolis_sanctuary_collision_05FA8_normals.inc"
};

static SVECTOR _gAcropolisSanctuaryCollision05FA8Verts[136] = {
#include "assets/acropolis_sanctuary_collision_05FA8_verts.inc"
};

static WorldCollisionGridFace _gAcropolisSanctuaryCollision05FA8Faces[85] = {
#include "assets/acropolis_sanctuary_collision_05FA8_faces.inc"
};

static s16 _gAcropolisSanctuaryCollision05FA8Cells[386] = {
#include "assets/acropolis_sanctuary_collision_05FA8_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisSanctuaryCollision05FA8Cells[i])
static s16* _gAcropolisSanctuaryCollision05FA8Table[9] = {
#include "assets/acropolis_sanctuary_collision_05FA8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_sanctuary_80183568 = { NULL, _gAcropolisSanctuaryCollision05FA8Normals, _gAcropolisSanctuaryCollision05FA8Verts, _gAcropolisSanctuaryCollision05FA8Faces, _gAcropolisSanctuaryCollision05FA8Table, 0x2A44, 0x332D, 3, 3, 4000, 85 };

WorldCollisionTrigger D_acropolis_sanctuary_8018358C[18] = {
    { NULL, NULL, NULL, { -7168, -1376, -8000, 0 }, { { 0, -2048, -1504, 0 }, { 0, 2048, -1504, 0 }, { 0, -2048, 1504, 0 }, { 0, 2048, 1504, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 0, 0 }, 2534, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9729, -1344, -5953, 0 }, { { 1242, -2048, -514, 0 }, { 1242, 2048, -514, 0 }, { -1241, -2048, 515, 0 }, { -1241, 2048, 515, 0 } }, { -1570, 0, -3788, 0 }, { 0, 0, 0, 0 }, 2442, 0, 2, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9665, -1376, -0x2721, 0 }, { { -1241, -2048, -514, 0 }, { -1241, 2048, -514, 0 }, { 1242, -2048, 514, 0 }, { 1242, 2048, 514, 0 } }, { -1569, 0, 3787, 0 }, { 0, 0, 0, 0 }, 2442, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7456, -1088, -7981, 0 }, { { 0, -2048, 1376, 0 }, { 0, 2048, 1376, 0 }, { 0, -2048, -1376, 0 }, { 0, 2048, -1376, 0 } }, { 4105, 0, 0, 0 }, { 0, 0, 0, 0 }, 2455, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9953, -1344, -9504, 0 }, { { 1242, -2048, 514, 0 }, { 1242, 2048, 514, 0 }, { -1241, -2048, -514, 0 }, { -1241, 2048, -514, 0 } }, { 1568, 0, -3788, 0 }, { 0, 0, 0, 0 }, 2442, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9985, -1216, -6529, 0 }, { { -1265, -2048, 453, 0 }, { -1265, 2048, 453, 0 }, { 1265, -2048, -453, 0 }, { 1265, 2048, -453, 0 } }, { 1382, 0, 3859, 0 }, { 0, 0, 0, 0 }, 2442, 0, 7, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7203, -1248, -0x2D63, 0 }, { { -5, -2048, 1057, 0 }, { -5, 2048, 1057, 0 }, { 5, -2048, -1056, 0 }, { 5, 2048, -1056, 0 } }, { 4099, 0, 19, 0 }, { 0, 0, 0, 0 }, 2304, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6850, -1312, -0x2D81, 0 }, { { 5, -2048, -1184, 0 }, { 5, 2048, -1184, 0 }, { -5, -2048, 1185, 0 }, { -5, 2048, 1185, 0 } }, { -4112, 0, -18, 0 }, { 0, 0, 0, 0 }, 2360, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6704, -1280, -4259, 0 }, { { -5, -2048, 1217, 0 }, { -5, 2048, 1217, 0 }, { 5, -2048, -1216, 0 }, { 5, 2048, -1216, 0 } }, { 4103, 0, 16, 0 }, { 0, 0, 0, 0 }, 2374, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6399, -1344, -4258, 0 }, { { 5, -2048, -1312, 0 }, { 5, 2048, -1312, 0 }, { -5, -2048, 1313, 0 }, { -5, 2048, 1313, 0 } }, { -4098, 0, -16, 0 }, { 0, 0, 0, 0 }, 2428, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4337, -1344, -5456, 0 }, { { -1472, -2048, -864, 0 }, { -1472, 2048, -864, 0 }, { 1473, -2048, 864, 0 }, { 1473, 2048, 864, 0 } }, { -2078, 0, 3540, 0 }, { 0, 0, 0, 0 }, 2660, 0, 6, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4401, -1312, -4928, 0 }, { { 1457, -2048, 896, 0 }, { 1457, 2048, 896, 0 }, { -1456, -2048, -896, 0 }, { -1456, 2048, -896, 0 } }, { 2149, 0, -3494, 0 }, { 0, 0, 0, 0 }, 2660, 0, 3, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3907, -1312, -0x2763, 0 }, { { 1810, -2048, -622, 0 }, { 1810, 2048, -622, 0 }, { -1809, -2048, 623, 0 }, { -1809, 2048, 623, 0 } }, { -1334, 0, -3877, 0 }, { 0, 0, 0, 0 }, 2792, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3970, -1312, -0x2884, 0 }, { { -2065, -2048, 751, 0 }, { -2065, 2048, 751, 0 }, { 2066, -2048, -750, 0 }, { 2066, 2048, -750, 0 } }, { 1405, 0, 3867, 0 }, { 0, 0, 0, 0 }, 2996, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2955, -1312, -8921, 0 }, { { 1025, -2048, -671, 0 }, { 1025, 2048, -671, 0 }, { -1026, -2048, 670, 0 }, { -1026, 2048, 670, 0 } }, { -2250, 0, -3441, 0 }, { 0, 0, 0, 0 }, 2374, 0, 3, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3298, -1344, -6786, 0 }, { { 958, -2048, 764, 0 }, { 958, 2048, 764, 0 }, { -958, -2048, -764, 0 }, { -958, 2048, -764, 0 } }, { 2562, 0, -3214, 0 }, { 0, 0, 0, 0 }, 2374, 0, 8, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2979, -1280, -6915, 0 }, { { -953, -2048, -769, 0 }, { -953, 2048, -769, 0 }, { 954, -2048, 770, 0 }, { 954, 2048, 770, 0 } }, { -2582, 0, 3198, 0 }, { 0, 0, 0, 0 }, 2374, 0, 3, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3201, -1344, -9089, 0 }, { { -1013, -2048, 690, 0 }, { -1013, 2048, 690, 0 }, { 1013, -2048, -690, 0 }, { 1013, 2048, -690, 0 } }, { 2314, 0, 3398, 0 }, { 0, 0, 0, 0 }, 2374, 0, 8, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_sanctuary_80183AE4[17] = {
    { NULL, NULL, NULL, { -9920, -42, -7968, 0 }, { { -287, 0, -1024, 0 }, { 288, 0, -1024, 0 }, { -287, 0, 1024, 0 }, { 288, 0, 1024, 0 } }, { 0, 4106, 0, 0 }, { 4096, 0, 0, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_WARP, 11, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7728, -24, -4448, 0 }, { { -735, 0, -256, 0 }, { 736, 0, -256, 0 }, { -735, 0, 256, 0 }, { 736, 0, 256, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 778, WORLD_COLLISION_TRIGGER_ACTION_WARP, 13, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3648, -64, -8016, 0 }, { { -319, 0, -432, 0 }, { 320, 0, -432, 0 }, { -319, 0, 432, 0 }, { 320, 0, 432, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 535, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5312, -64, -8288, 0 }, { { -543, 0, -896, 0 }, { 544, 0, -896, 0 }, { -543, 0, 896, 0 }, { 544, 0, 896, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, 0, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2736, -61, -7896, 0 }, { { -655, 0, -680, 0 }, { 656, 0, -1448, 0 }, { -655, 0, 856, 0 }, { 656, 0, 1272, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1588, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7712, -32, -4416, 0 }, { { -895, 0, -672, 0 }, { 896, 0, -672, 0 }, { -895, 0, 672, 0 }, { 896, 0, 672, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -6608, -64, -8086, 0 }, { { -3, 0, 125, 0 }, { -105, 0, -3, 0 }, { 113, 0, -14, 0 }, { -2, 0, -106, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4095, 0 }, 124, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -4768, -64, -6992, 0 }, { { -751, 0, -720, 0 }, { 752, 0, -720, 0 }, { -751, 0, 720, 0 }, { 752, 0, 720, 0 } }, { 0, 4097, 0, 0 }, { -995, 0, 3973, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_CAP, 0, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4768, -64, -8864, 0 }, { { -751, 0, -624, 0 }, { 752, 0, -624, 0 }, { -751, 0, 624, 0 }, { 752, 0, 624, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 976, WORLD_COLLISION_TRIGGER_ACTION_CAP, 0, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6446, -64, -7903, 0 }, { { -115, 0, 362, 0 }, { -377, 0, 99, 0 }, { 385, 0, -104, 0 }, { 110, 0, -356, 0 } }, { 0, 4100, 0, 0 }, { 2895, 0, 2895, 0 }, 398, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6800, -64, -8240, 0 }, { { -99, 0, 378, 0 }, { -393, 0, 83, 0 }, { 401, 0, -88, 0 }, { 94, 0, -372, 0 } }, { 0, 4106, 0, 0 }, { -2896, 0, -2896, 0 }, 409, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6802, -64, -7904, 0 }, { { 58, 0, 362, 0 }, { -364, 0, -61, 0 }, { 371, 0, 56, 0 }, { -64, 0, -356, 0 } }, { 0, 4095, 0, 0 }, { -2897, 0, 2895, 0 }, 374, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6448, -64, -8256, 0 }, { { 74, 0, 346, 0 }, { -348, 0, -77, 0 }, { 355, 0, 72, 0 }, { -80, 0, -340, 0 } }, { 0, 4095, 0, 0 }, { 2896, 0, -2896, 0 }, 362, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6633, -64, -7809, 0 }, { { 1, 0, 277, 0 }, { -186, 0, 5, 0 }, { 178, 0, -9, 0 }, { 10, 0, -270, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4093, 0 }, 275, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6617, -64, -8385, 0 }, { { -15, 0, 277, 0 }, { -170, 0, 5, 0 }, { 194, 0, -9, 0 }, { -6, 0, -270, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4093, 0 }, 277, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6305, -64, -8097, 0 }, { { -276, 0, -8, 0 }, { -4, 0, -163, 0 }, { 10, 0, 170, 0 }, { 270, 0, 1, 0 } }, { 0, 4094, 0, 0 }, { 4093, 0, 0, 0 }, 275, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6912, -64, -8073, 0 }, { { -276, 0, 0, 0 }, { -4, 0, -155, 0 }, { 10, 0, 146, 0 }, { 270, 0, 9, 0 } }, { 0, 4095, 0, 0 }, { -4093, 0, 0, 0 }, 275, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_acropolis_sanctuary_80183FF0[3] = {
    { 27, 107, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_110700_8013BF94 },
    { 102, 107, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_210700_801585CC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_sanctuary_80184014[2] = {
    { 102, 107, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_210700_801585CC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_sanctuary_8018402C[12] = {
    { NULL, NULL },
    { D_map_akropolis_8017BA5C, D_acropolis_sanctuary_80183FF0 },
    { D_map_akropolis_8017BA8C, D_acropolis_sanctuary_80184014 },
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

/// Authored point lights contributing in every view of the Acropolis sanctuary.
///
/// Positions and radii use integer world units; RGB intensities have 12
/// fractional bits (`ONE` is 1.0). The room-light collection borrows this
/// writable table while the overlay is loaded: coordinate updates set its
/// parents and composed matrices, and lighting queries overwrite attenuation.
static WorldCoordPointLight _gAcropolisSanctuaryPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7720, -3500, -5973 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2867, 2867, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 4575,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4989, -3500, -6313 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2867, 2867, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 4488,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5050, -3500, -10034 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2867, 2867, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 4671,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7720, -3500, -10034 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2867, 2867, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 4284,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -10120, -2000, -9282 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 4500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -10110, -2000, -6674 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 4499,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2870, -2000, -6481 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 4500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2870, -2000, -9568 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 4500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6180, -3500, -8019 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE / 2, ONE / 2, ONE / 2 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 10000,
        .outer = 10000,
    },
};

WorldCoordRoomLights D_acropolis_sanctuary_801843EC[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisSanctuaryPointLights), _gAcropolisSanctuaryPointLights, 0, NULL },
};

SpriteBatch D_acropolis_sanctuary_80184404[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_sanctuary_80184414[40] = {
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, 16, 883, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -56, 16, 701, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -64, 32, 503, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -72, 48, 480, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -80, 64, 411, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -88, 72, 343, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 88, 328, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 104, 312, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, 16, 853, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, 16, 863, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 72, 16, 843, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 80, 24, 736, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, 32, 691, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 96, 32, 638, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 32, 626, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 88, 516, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 112, 48, 504, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 120, 48, 522, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 128, 56, 512, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 136, 64, 480, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 64, 450, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, 64, 117, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 56 } }, -160, 0, 857, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -64, -16, 852, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 8, 853, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 56, 874, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 48 } }, 80, 0, 862, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 56, -16, 859, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 40, 852, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 48, 867, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 48, 510, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 72, 512, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 96 } }, -160, 24, 523, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -88, 0, 510, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -88, 24, 523, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 96, 383, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 104 } }, 112, 16, 504, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 104, 0, 504, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 16, 513, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 72, 192, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_sanctuary_80184734[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 3, 0 } },
    { 22, 8, 0, 0, { 0, 0 } },
    { 30, 9, 0, 0, { 2, 0 } },
    { 39, 1, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_sanctuary_80184764[50] = {
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -8, -24, 1316, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 0, 0, 1351, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 48, 974, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 317, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, 8, 970, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, 16, 877, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, 16, 789, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, 24, 726, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -80, 32, 693, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -88, 40, 610, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -96, 40, 602, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -104, 48, 450, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -112, 48, 510, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -120, 56, 427, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, 56, 427, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -136, 56, 445, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -144, 64, 418, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -152, 72, 385, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, 80, 387, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, 16, 915, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 64, 16, 734, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 72, 32, 636, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 80, 48, 512, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, 64, 447, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, 64, 388, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, 72, 352, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 88, 336, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -152, -8, 1073, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -152, 32, 681, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -80, 0, 1043, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, -16, 1000, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, 0, 1100, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 56, -16, 931, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, 0, 924, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 24, 729, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 40 } }, 72, 0, 937, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -8, 626, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 48 } }, -160, 16, 628, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -112, 0, 631, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -96, 16, 632, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -72, 32, 790, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 72, 0, 562, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 80, 80, 478, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 72, 48, 548, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 56 } }, 80, 24, 585, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, 88, 387, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 64, 207, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 88, 210, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 112, 337, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -8, 668, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_sanctuary_80184B4C[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 25, 0, 0, { 0, 0 } },
    { 27, 9, 0, 0, { 5, 0 } },
    { 36, 9, 0, 0, { 1, 0 } },
    { 45, 4, 0, 0, { 4, 0 } },
    { 49, 1, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_sanctuary_80184B8C[46] = {
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -128, -32, 1356, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -128, -24, 979, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 0, 571, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -104, -16, 960, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 24, 575, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -32, -16, 921, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 0, -8, 918, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, -160, 0, 571, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 56 } }, -104, 8, 565, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -40, 8, 601, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -32, 72, 537, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 187, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 112, 278, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 112, 208, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -16, 880, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 32, 906, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 0, -16, 901, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 0, 818, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 8, 697, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -24, 0, 527, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, 80, 545, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 88, 554, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -32, 8, 510, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 32, 460, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 40, 420, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 48, 391, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 64, 361, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 64, 341, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 72, 325, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 80, 305, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 88, 288, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 88, 277, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 96, 263, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 96, 253, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 104, 243, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 104, 235, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 104, 228, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -152, 104, 228, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, 8, 612, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -152, 8, 606, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 8, 598, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -136, 0, 588, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, 0, 583, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 32, 670, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 56 } }, 56, -120, 354, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -80, 600, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_sanctuary_80184F24[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 6, 0 } },
    { 1, 6, 0, 0, { 1, 0 } },
    { 7, 4, 0, 0, { 4, 0 } },
    { 11, 2, 0, 0, { 0, 0 } },
    { 13, 25, 0, 0, { 5, 0 } },
    { 38, 6, 0, 0, { 3, 0 } },
    { 44, 1, 0, 0, { 7, 0 } },
    { 45, 1, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_sanctuary_80184F74[41] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 24, 600, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 0, 754, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, 0, 607, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, 0, 611, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 0, 614, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, 0, 619, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 40, 961, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -8, -16, 945, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 32, 916, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 0, 879, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 0, 711, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 16, 0, 579, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, 56, 651, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, 80, 563, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 16, 572, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 32, 545, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, 40, 534, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 48, 48, 473, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 56, 442, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 64, 413, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 72, 393, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 80, 72, 374, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 88, 72, 360, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 80, 347, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 80, 341, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 64, 322, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 104, 329, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 64, 318, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, 72, 319, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 88, 314, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -8, -16, 882, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, -24, 886, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 40, 887, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 56 } }, 8, -16, 882, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 112, 88 } }, 48, 0, 592, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, 16, 8, 582, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 24, 96, 478, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, 112, 80, 316, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -88, -96, 583, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -160, -120, 202, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, -120, 230, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_sanctuary_801852A8[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 3, 0 } },
    { 6, 24, 0, 0, { 4, 0 } },
    { 30, 4, 0, 0, { 1, 0 } },
    { 34, 3, 0, 0, { 5, 0 } },
    { 37, 1, 0, 0, { 0, 0 } },
    { 38, 1, 0, 0, { 6, 0 } },
    { 39, 2, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_sanctuary_801852F0[32] = {
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 88, -8, 1250, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 24, 659, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -8, 0, 779, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 64, 776, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 64, 772, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 16, 727, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 16, 619, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, 24, 0, 565, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, 32, 0, 510, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 473, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 40, 422, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 48, 403, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 48, 389, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 56, 350, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 56, 337, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 64, 325, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 64, 315, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 64, 307, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 72, 275, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 72, 265, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 72, 259, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 72, 252, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 16, 520, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 24, 492, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -8, 8, 791, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, 8, 813, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 40 } }, 32, 8, 816, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 96, 48, 811, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, -8, 805, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, 32, 575, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 32, 8, 5021, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 80 } }, 48, 8, 455, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_sanctuary_80185570[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 3, 0 } },
    { 1, 23, 0, 0, { 0, 0 } },
    { 24, 5, 0, 0, { 2, 0 } },
    { 29, 3, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_sanctuary_801855A0[24] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 56, 893, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 80, 735, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 80, 804, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -72, 8, 488, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 16, 451, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -64, 8, 514, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 16, 618, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 32, 670, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 32, 701, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 32, 726, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 16, 755, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -16, 8, 808, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 8, 876, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 0, 8, 875, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 0, 835, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 48, 894, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -120, 24, 817, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 48, 899, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 40 } }, -112, 8, 887, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 144, 64 } }, -160, 8, 760, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 72, 750, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, 80, 750, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -8, 636, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 104 } }, -160, 16, 634, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_sanctuary_80185780[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { 14, 5, 0, 0, { 3, 0 } },
    { 19, 4, 0, 0, { 2, 0 } },
    { 23, 1, 0, 0, { 4, 0 } },
    { 24, 0, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_sanctuary_801857B8[9] = {
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -80, 24, 614, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -48, 32, 607, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -16, 40, 600, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -64, 766, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -56, -96, 785, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -32, -88, 663, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 0, -80, 651, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 32, -72, 666, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -64, 748, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_sanctuary_8018586C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_sanctuary_8018588C[4] = {
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -48, -48, 585, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 0, -48, 591, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -40, -16, 587, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 0, -16, 591, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_sanctuary_801858DC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_sanctuary_801858F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_sanctuary_80185904[8] = {
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -152, -24, 230, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -128, 16, 239, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 64 } }, -152, 32, 227, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -112, 96, 248, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 96 } }, -56, 24, 302, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 96 } }, 24, 16, 423, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 136 } }, 88, -40, 432, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 136, -8, 394, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_sanctuary_801859A4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_sanctuary_801859BC[32] = {
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -40, -112, 1294, { .fields = { 88, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -48, -96, 1195, { .fields = { 64, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, -48, -80, 1110, { .fields = { 56, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -40, -64, 1036, { .fields = { 64, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -16, -48, 1026, { .fields = { 88, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, -32, 1021, { .fields = { 104, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -96, 1373, { .fields = { 120, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -40, 978, { .fields = { 112, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -40, -112, 1329, { .fields = { 88, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -96, 992, { .fields = { 112, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -80, 671, { .fields = { 112, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -64, 913, { .fields = { 112, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -80, 1379, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -56, 1231, { .fields = { 120, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, -40, 1095, { .fields = { 112, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 16 } }, -24, -112, 1380, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -8, -96, 1415, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, 8, -80, 1388, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -48, -120, 1200, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -48, -104, 1059, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -56, -88, 907, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -56, -72, 671, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, -56, -64, 664, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, -48, -56, 807, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 8 } }, -40, -48, 832, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -24, -40, 845, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -16, -32, 852, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 16, -64, 1293, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, 8, -48, 1164, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 8, -32, 1027, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, 0, -24, 904, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 8, -16, 906, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_sanctuary_80185C3C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 9, 0, 0, { 2, 0 } },
    { 15, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_sanctuary_80185C64[5] = {
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -160, 32, 488, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -120, 32, 499, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 104 } }, -80, -8, 410, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 80 } }, -24, 40, 497, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -64, 96, 305, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_sanctuary_80185CC8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_sanctuary_80185CE0[8] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 338, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -8, 88, 325, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 80, 325, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, 72, 325, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 72, 48, 325, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, 64, 350, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -112, 96, 325, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -64, 96, 325, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_sanctuary_80185D80[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_sanctuary_80185D98[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_sanctuary_80185DA8[38] = {
    { 143, 0x3FC0, { .fields = { 64, 112 } }, -48, -104, 674, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 64, 88 } }, 16, -112, 618, { .fields = { 0, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 72, 80 } }, -56, 8, 622, { .fields = { 112, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 72, 112 } }, 16, -24, 566, { .fields = { 56, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 16, -112, 659, { .fields = { 112, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, -104, 637, { .fields = { 40, 8 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -96, 616, { .fields = { 40, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -88, 581, { .fields = { 88, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -56, 554, { .fields = { 104, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 72, -24, 544, { .fields = { 72, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 64, -8, 559, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 16, 546, { .fields = { 8, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 80, 40, 506, { .fields = { 112, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, -104, 690, { .fields = { 40, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -96, 705, { .fields = { 24, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -48, -88, 716, { .fields = { 64, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -48, -56, 697, { .fields = { 120, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -24, 675, { .fields = { 56, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -56, 8, 651, { .fields = { 24, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -40, 64, 606, { .fields = { 56, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -8, 80, 578, { .fields = { 64, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 24, 48, 562, { .fields = { 88, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 48, 80, 534, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 16 } }, 0, -120, 647, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, 48, -120, 599, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 32 } }, 64, -96, 568, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 56 } }, 72, -64, 554, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, 72, -8, 534, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, 80, 16, 513, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -32, -112, 688, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -48, -96, 699, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 56 } }, -64, -80, 685, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -56, -24, 672, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, -64, -8, 644, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, -72, 72, 604, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 48 } }, -56, 72, 508, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 96, 40 } }, -8, 80, 475, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 88, 80, 481, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_sanctuary_801860A0[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 19, 0, 0, { 2, 0 } },
    { 23, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_sanctuary_801860C8[16] = {
    { { .empty = D_acropolis_sanctuary_80184404 }, D_acropolis_sanctuary_80184404, NULL },
    { { .elements = D_acropolis_sanctuary_80184414 }, D_acropolis_sanctuary_80184734, NULL },
    { { .elements = D_acropolis_sanctuary_80184764 }, D_acropolis_sanctuary_80184B4C, NULL },
    { { .elements = D_acropolis_sanctuary_80184B8C }, D_acropolis_sanctuary_80184F24, NULL },
    { { .elements = D_acropolis_sanctuary_80184F74 }, D_acropolis_sanctuary_801852A8, NULL },
    { { .elements = D_acropolis_sanctuary_801852F0 }, D_acropolis_sanctuary_80185570, NULL },
    { { .elements = D_acropolis_sanctuary_801855A0 }, D_acropolis_sanctuary_80185780, NULL },
    { { .elements = D_acropolis_sanctuary_801857B8 }, D_acropolis_sanctuary_8018586C, NULL },
    { { .elements = D_acropolis_sanctuary_8018588C }, D_acropolis_sanctuary_801858DC, NULL },
    { { .empty = D_acropolis_sanctuary_801858F4 }, D_acropolis_sanctuary_801858F4, NULL },
    { { .elements = D_acropolis_sanctuary_80185904 }, D_acropolis_sanctuary_801859A4, NULL },
    { { .elements = D_acropolis_sanctuary_801859BC }, D_acropolis_sanctuary_80185C3C, NULL },
    { { .elements = D_acropolis_sanctuary_80185C64 }, D_acropolis_sanctuary_80185CC8, NULL },
    { { .elements = D_acropolis_sanctuary_80185CE0 }, D_acropolis_sanctuary_80185D80, NULL },
    { { .empty = D_acropolis_sanctuary_80185D98 }, D_acropolis_sanctuary_80185D98, NULL },
    { { .elements = D_acropolis_sanctuary_80185DA8 }, D_acropolis_sanctuary_801860A0, NULL },
};

ViewCamera D_acropolis_sanctuary_80186188[16] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 6200, 0x7530, 8000 } }, 589 },
    { { { { 345, 0, 4081 }, { 546, 4059, -46 }, { -4044, 548, 342 } }, { 4961, 1400, 8451 } }, 230 },
    { { { { 559, 0, -4057 }, { -549, 4058, -75 }, { 4020, 554, 554 } }, { 9472, 1403, 8386 } }, 230 },
    { { { { 775, 0, -4021 }, { -623, 4046, -120 }, { 3973, 635, 766 } }, { 9087, 1276, 0x2D59 } }, 230 },
    { { { { 656, 0, 4043 }, { 635, 4045, -103 }, { -3992, 643, 648 } }, { 4960, 1280, 0x2D3A } }, 230 },
    { { { { -795, 0, -4017 }, { 65, 4095, -13 }, { 4017, -66, -795 } }, { 8714, 919, 4418 } }, 230 },
    { { { { -580, 0, 4054 }, { -150, 4093, -21 }, { -4051, -151, -579 } }, { 4402, 920, 4439 } }, 230 },
    { { { { 1227, 0, -3907 }, { -3609, 1569, -1134 }, { 1496, 3783, 470 } }, { 4250, 3534, 8412 } }, 230 },
    { { { { -186, 0, -4091 }, { -25, 4095, 1 }, { 4091, 25, -186 } }, { 6573, 667, 7896 } }, 230 },
    { { { { 1471, 0, 3822 }, { 944, 3969, -363 }, { -3704, 1012, 1425 } }, { 7110, 2190, 9120 } }, 680 },
    { { { { 3484, 0, -2153 }, { -524, 3972, -847 }, { 2089, 996, 3379 } }, { 8712, 1215, 9980 } }, 230 },
    { { { { 2699, 0, 3080 }, { -2257, 2787, 1978 }, { -2096, -3001, 1837 } }, { 8528, 459, 9285 } }, 230 },
    { { { { 4014, 0, -810 }, { -213, 3951, -1058 }, { 782, 1079, 3873 } }, { 5552, 1588, 0x2BBF } }, 230 },
    { { { { 4014, 0, -810 }, { -213, 3951, -1058 }, { 782, 1079, 3873 } }, { 8025, 1652, 6179 } }, 230 },
    { { { { 4071, 0, -446 }, { -158, 3830, -1440 }, { 417, 1449, 3808 } }, { 4926, 1150, 9330 } }, 230 },
    { { { { 1261, 0, -3896 }, { 704, 4028, 228 }, { 3832, -740, 1240 } }, { 0x310A, 4285, 8624 } }, 207 },
};

WorldCollisionFootstepSounds D_acropolis_sanctuary_801863C8 = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

WorldCollisionFootstepSounds D_acropolis_sanctuary_801863D4 = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionSurfaceProperties D_acropolis_sanctuary_801863E0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_sanctuary_801863E8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_sanctuary_801863C8 },
};

WorldCollisionSurfaceProperties D_acropolis_sanctuary_801863F0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_sanctuary_801863D4 },
};

WorldCollisionSurfaceProperties* D_acropolis_sanctuary_801863F8[8] = {
    D_acropolis_sanctuary_801863E0,
    D_acropolis_sanctuary_801863E8,
    D_acropolis_sanctuary_801863F0,
    D_acropolis_sanctuary_801863E0,
    D_acropolis_sanctuary_801863E0,
    D_acropolis_sanctuary_801863E0,
    D_acropolis_sanctuary_801863E0,
    D_acropolis_sanctuary_801863E0,
};

AreaApplyRec D_acropolis_sanctuary_80186418[11] = {
    { 1, 3, 4, 1 },
    { 1, 4, 4, 1 },
    { 1, 7, 4, 1 },
    { 1, 9, 3, 17 },
    { 1, 9, 7, 33 },
    { 1, 10, 2, 17 },
    { 1, 10, 7, 33 },
    { 1, 11, 2, 17 },
    { 1, 11, 7, 33 },
    { 1, 13, 7, 33 },
    { 255, 0, 0, 0 },
};

static TmdBone _gAcropolisSanctuaryModel090F0Skeleton[3] = {
#include "assets/acropolis_sanctuary_model_090F0_skeleton.inc"
};

static u32 _gAcropolisSanctuaryModel090F0PartVerts[3] = {
#include "assets/acropolis_sanctuary_model_090F0_partVerts.inc"
};

static SVECTOR _gAcropolisSanctuaryModel090F0Verts[56] = {
#include "assets/acropolis_sanctuary_model_090F0_verts.inc"
};

static SVECTOR _gAcropolisSanctuaryModel090F0Normals[6] = {
#include "assets/acropolis_sanctuary_model_090F0_normals.inc"
};

static u32 _gAcropolisSanctuaryModel090F0Stream[215] = {
#include "assets/acropolis_sanctuary_model_090F0_stream.inc"
};

TmdSource gAcropolisSanctuaryModel090F0 = {
    0,
    1768,
    0,
    3,
    _gAcropolisSanctuaryModel090F0PartVerts,
    _gAcropolisSanctuaryModel090F0Verts,
    _gAcropolisSanctuaryModel090F0Normals,
    _gAcropolisSanctuaryModel090F0Skeleton,
    _gAcropolisSanctuaryModel090F0Stream,
};

static TmdBone _gAcropolisSanctuaryModel09584Skeleton[1] = {
#include "assets/acropolis_sanctuary_model_09584_skeleton.inc"
};

static u32 _gAcropolisSanctuaryModel09584PartVerts[1] = {
#include "assets/acropolis_sanctuary_model_09584_partVerts.inc"
};

static SVECTOR _gAcropolisSanctuaryModel09584Verts[19] = {
#include "assets/acropolis_sanctuary_model_09584_verts.inc"
};

static SVECTOR _gAcropolisSanctuaryModel09584Normals[11] = {
#include "assets/acropolis_sanctuary_model_09584_normals.inc"
};

static u32 _gAcropolisSanctuaryModel09584Stream[73] = {
#include "assets/acropolis_sanctuary_model_09584_stream.inc"
};

TmdSource gAcropolisSanctuaryModel09584 = {
    0,
    440,
    0,
    1,
    _gAcropolisSanctuaryModel09584PartVerts,
    _gAcropolisSanctuaryModel09584Verts,
    _gAcropolisSanctuaryModel09584Normals,
    _gAcropolisSanctuaryModel09584Skeleton,
    _gAcropolisSanctuaryModel09584Stream,
};

u32 D_acropolis_sanctuary_80186C8C = 0xB000000;

Task* D_acropolis_sanctuary_80186C90 = NULL;

SVECTOR ActorContact_ScratchPosition = { 0, 0, 0, 0 };

static void func_acropolis_sanctuary_801802E0(Task* task);

/// The room task's per-frame state. Once the session reaches phase 3
/// (`gameFlagGetNibble(2)` still 0), advances that flag and applies the
/// room's one-shot state, then disables the action triggers while
/// `areaGetCurrentObjectState(0x1C)` is 2. `mask` is
/// a local because the target CSEs `~0x40` into a register and uses `and`
/// rather than nine `andi`s.
static void func_acropolis_sanctuary_8017D5E0(Task* task)
{
    s32                    mask;
    WorldCollisionTrigger* p0;
    WorldCollisionTrigger* p3;
    WorldCollisionTrigger* p4;
    WorldCollisionTrigger* p5;
    WorldCollisionTrigger* p6;
    WorldCollisionTrigger* p7;
    WorldCollisionTrigger* p8;
    WorldCollisionTrigger* p9;
    WorldCollisionTrigger* p10;

    if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS) == 0 && gGameSession->location.loc.warp == 3) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS, 2);
        func_800E8634(D_acropolis_sanctuary_80180B0C, 0, D_acropolis_sanctuary_80181664);
        Gp_ApplyAreaRecs(D_acropolis_sanctuary_80186418);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 6;
        gameFlagSetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS, 5);
        gameFlagSetNibble(GAME_FLAG_OBSERVATORY_EXIT_USED, 1);
        func_800E3FAC(0xA2, 6);
        gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
        gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 5);
    }
    if (areaGetCurrentObjectState(0x1C) == 2) {
        mask = ~WORLD_COLLISION_TRIGGER_ENABLED;
        p0   = &(D_acropolis_sanctuary_80183AE4 + 6)[0];
        p3   = &(D_acropolis_sanctuary_80183AE4 + 6)[3];
        p4   = &(D_acropolis_sanctuary_80183AE4 + 6)[4];
        p5   = &(D_acropolis_sanctuary_80183AE4 + 6)[5];
        p6   = &(D_acropolis_sanctuary_80183AE4 + 6)[6];
        p7   = &(D_acropolis_sanctuary_80183AE4 + 6)[7];
        p8   = &(D_acropolis_sanctuary_80183AE4 + 6)[8];
        p9   = &(D_acropolis_sanctuary_80183AE4 + 6)[9];
        p10  = &(D_acropolis_sanctuary_80183AE4 + 6)[10];

        p0->flags  &= mask;
        p3->flags  &= mask;
        p4->flags  &= mask;
        p5->flags  &= mask;
        p6->flags  &= mask;
        p7->flags  &= mask;
        p8->flags  &= mask;
        p9->flags  &= mask;
        p10->flags &= mask;
    }
}

/// Message gate for the sanctuary's second hotspot: copies the incoming record
/// to the outgoing one, then answers message 0xB. The first time the message is
/// seen for real (`queryOnly` == 0) it latches nibble 7 to 2 and raises the room's
/// 0x13 bit-2 flag; the answer written back into `room` is 1 while nibble 2
/// is still clear and 2 once it is set.
s32 func_acropolis_sanctuary_8017D73C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 nib;

    *out = *in;
    if (in->areaId == GAME_AREA_ACROPOLIS_PROMENADE) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH) == 0) {
                gameFlagSetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH, 2);
                Gp_SetCurBit2Flag(0x13, 2);
            }
        }
        if (in->areaId == GAME_AREA_ACROPOLIS_PROMENADE && in->queryOnly == ROOM_EVENT_EXECUTE) {
            nib = gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS);
            if (nib == 0) {
                nib = 1;
            } else {
                nib = 2;
            }
            out->room = nib;
        }
    }
    return 1;
}

/// Refuses every key-item use in the sanctuary, returning 0 without side effects.
///
/// Receives `ACROPOLIS_SANCTUARY_MESSAGE_USE_KEY_ITEM` with an item ID in the
/// first payload word; all parameters are unused. The item menu treats the
/// result as "cannot use now" and retains the item.
static s32 _acropolisSanctuaryRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    enum { ACROPOLIS_SANCTUARY_KEY_ITEM_REFUSED = 0 };

    return ACROPOLIS_SANCTUARY_KEY_ITEM_REFUSED;
}

s32 func_acropolis_sanctuary_8017D810(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0 && gameFlagGetNibble(GAME_FLAG_SANCTUARY_BLOCKER_CLEARED) == 0) {
        func_800E8614(D_acropolis_sanctuary_80181814, 0);
    }
    return 0;
}

/// Message gate for the sanctuary hotspot registered under id 0x13EF: sub-id 1
/// arms the room's own task the first time it is seen, latching nibble 7 so a
/// second visit does nothing. The record is not copied to the outgoing one -
/// this handler only ever consumes the message (returns 0).
s32 func_acropolis_sanctuary_8017D848(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->warp == 1 && gameFlagGetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH) == 0) {
        gameFlagSetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH, 1);
        taskSpawnFromTable(&D_acropolis_sanctuary_80182240, 0, 0, 0);
    }
    return 0;
}

/// Applies an event script's packed view-sprite batch selection.
///
/// Bits 0..7 hold a 1-based view ID; bits 8..15 select batch 1 when zero or
/// batch 2 otherwise. Higher bits are ignored. The sanctuary scripts pass
/// views 12 and 16, whose live batch lists contain both alternatives.
static void _acropolisSanctuaryApplyScriptSpriteSelection(u32 packedSelection)
{
    _acropolisSanctuarySelectViewSpriteBatch(
        (packedSelection >> ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_BATCH_SHIFT) & ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_FIELD_MASK,
        packedSelection & ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_FIELD_MASK);
}

/// Republishes the player's weapon to slot 3: picks the room's 0x3E8 record by
/// the equipped-weapon index in `gPlayerStatus.weapon`, has `Gp_PlayerWeaponId` stamp the
/// current weapon model id into its `field_0`, then sends it.
void func_acropolis_sanctuary_8017D8CC(void)
{
    if (gPlayerStatus.weapon == 2) {
        Gp_PlayerWeaponId(&D_acropolis_sanctuary_801809F8.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &D_acropolis_sanctuary_801809F8, 0);
    } else {
        Gp_PlayerWeaponId(&D_acropolis_sanctuary_80180A0C.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &D_acropolis_sanctuary_80180A0C, 0);
    }
}

/// State 0 of the sanctuary room task: publishes the room's message-handler
/// table under pointer slot 7 and advances to the next state. Unless nibble 6
/// has already reached 1 it also chains slot-4 message list 1 onto itself and,
/// when nibble 2 is set and that slot holds a task, places the actor by sending
/// it the 0x7D3 animation record followed by the 0x7D4 placement.
static void func_acropolis_sanctuary_8017D930(Task* arg0)
{
    Task* slot;

    arg0->msgTable = D_acropolis_sanctuary_8018081C;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    arg0->state = arg0->state + 1;
    if (gameFlagGetNibble(GAME_FLAG_SANCTUARY_BLOCKER_CLEARED) != 1) {
        slot = Gp_LookupSlot4(1);
        Gp_MsgSlot4Chain(1, 1);
        if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS) != 0 && slot != NULL) {
            TASK_MESSAGE_DISPATCH_POINTER(slot, 0x7D3, &D_acropolis_sanctuary_80180AE8, 0);
            TASK_MESSAGE_DISPATCH_POINTER(slot, 0x7D4, &D_acropolis_sanctuary_801808BC, 0);
        }
    }
    func_acropolis_sanctuary_8017DD78();
}

/// Runs the room task's current state through a stack copy of the room's
/// three-entry state table: set-up, the per-frame entry fixup and `taskKill`.
void func_acropolis_sanctuary_8017D9E8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_sanctuary_8017D5C4;
    sp.funcs[task->state](task);
}

/// Installs the cutscene's animation set on the player and starts its first
/// animation from a reset pose, back on the collision grid.
///
/// `task` is the cutscene task; nothing is sent when its work block holds no
/// player. The request is consumed by the dispatch, while the set table has to
/// outlive the playback it starts.
static inline void _acropolisSanctuaryCutsceneInstallPlayerAnimation(Task* task)
{
    AnimationPlayRequest             request;
    _AcropolisSanctuaryCutsceneWork* work;

    work = task->work;
    if (work->playerTask != NULL) {
        request.source.sets          = D_acropolis_sanctuary_801820E4;
        request.animationId          = 0;
        request.blend                = ANIMATION_BLEND_RESET;
        request.blendFrames          = 0xF;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
    }
}

/// Puts the player on the cutscene's mark, upright and with every angle zero.
///
/// `work->playerTask` must be live. The transform is consumed by the dispatch.
static inline void _acropolisSanctuaryCutscenePlacePlayer(_AcropolisSanctuaryCutsceneWork* work)
{
    ActorTransform place;

    place.pos.vx = -0x1DB0;
    place.pos.vy = 0;
    place.pos.vz = -0x1130;
    place.rot.vx = 0;
    place.rot.vy = 0;
    place.rot.vz = 0;
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &place, 0);
}

/// Carries out the script's player placement the first frame it is pending.
///
/// Does nothing until the script has selected the place-player phase, and
/// nothing again once the placement is marked applied: the player's effects
/// are dropped, the cutscene animation is installed, the player is moved to
/// the mark and the save's camera view follows it.
static inline void _acropolisSanctuaryCutsceneApplyPlacement(Task* task)
{
    _AcropolisSanctuaryCutsceneWork* work;

    work = task->work;
    switch (work->phase) {
        case ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_INITIAL:
        case ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_HOLD:
            break;
        case ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_PLACE_PLAYER:
            if (work->placementApplied == ACROPOLIS_SANCTUARY_CUTSCENE_PLACEMENT_PENDING) {
                Gp_KillPlayerEffs();
                _acropolisSanctuaryCutsceneInstallPlayerAnimation(task);
                _acropolisSanctuaryCutscenePlacePlayer(work);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xE;
                work->placementApplied                                     = work->placementApplied + 1;
            }
            break;
    }
}

/// The sanctuary cutscene task. State 0 allocates the task's `_AcropolisSanctuaryCutsceneWork`
/// block, captures slot 3 in it, publishes the task itself in
/// `D_acropolis_sanctuary_80186C90` and cues the scene: slot 3 is sent the
/// 0x3E8 weapon record for the equipped weapon, the scene's sound event is
/// enqueued and its script pair is started.
///
/// State 1 drives the scene. `GameSession::eventState` reaching 0 instead stops
/// the sound, writes the room's exit into the save and hands off to task 0x11
/// before the task kills itself. Otherwise the scene fires exactly
/// once, when `func_acropolis_sanctuary_8017DCE0` has armed `phase` at 2 and
/// `placementApplied` is still 0: the player's effects are dropped, slot 3 is given the
/// 0x3F4 record and then warped to the scene's mark with a 0x3E9 placement, and
/// `placementApplied` is bumped so the next frame does nothing.
void func_acropolis_sanctuary_8017DA40(Task* arg0)
{
    AnimationPlayRequest             request;
    _AcropolisSanctuaryCutsceneWork* work;
    _AcropolisSanctuaryCutsceneWork* initialWork;
    s32                              state;
    s32                              idx;
    s32                              weaponId;

    state = arg0->state;
    switch (state) {
        case 0:
            if (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                work       = memCalloc(sizeof(*work), 0);
                arg0->work = work;
                if (work == NULL) {
                    taskKill(arg0);
                } else {
                    memFillBytes(work, 0, sizeof(*work));
                    work->playerTask               = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    D_acropolis_sanctuary_80186C90 = arg0;
                }
                initialWork = arg0->work;
                weaponId    = gPlayerStatus.weapon;
                idx         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;

                request.source.index         = idx;
                request.animationId          = 1;
                request.blend                = ANIMATION_BLEND_INTERPOLATE;
                request.blendFrames          = 0xF;
                request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(initialWork->playerTask, ANIMATION_MESSAGE_PLAY, &request, 0);
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 7), 0, 0);
                func_800E8634(D_acropolis_sanctuary_801820F0, 0, D_acropolis_sanctuary_801821C8);
                arg0->state = arg0->state + 1;
            }
            break;

        case 1:
            if (gGameSession->eventState == 0) {
                sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_ACROPOLIS_ROOF_GARDEN;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 2;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
                gDisplayState.spriteVariant                                 = 1;
                Task_Spawn(0, 0x11, 0, 0);
                taskKill(arg0);
                break;
            }
            _acropolisSanctuaryCutsceneApplyPlacement(arg0);
            break;
    }
}

/// Request entry point for the sanctuary cutscene task's work block: 0 and 1
/// arm the script at phase 1 or 2 respectively, rewinding `placementApplied` so the driver
/// runs the scene once, while 2 just plays the pair of sound events the scene
/// is cued with.
void func_acropolis_sanctuary_8017DCE0(s32 arg0)
{
    _AcropolisSanctuaryCutsceneWork* work = D_acropolis_sanctuary_80186C90->work;

    switch (arg0) {
        case 0:
            work->phase            = ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_HOLD;
            work->placementApplied = ACROPOLIS_SANCTUARY_CUTSCENE_PLACEMENT_PENDING;
            return;
        case 1:
            work->phase            = ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_PLACE_PLAYER;
            work->placementApplied = ACROPOLIS_SANCTUARY_CUTSCENE_PLACEMENT_PENDING;
            return;
        case 2:
            sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 7), SOUND_SCRIPT_STOP_NO_FADE);
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 8), 0, 0);
            return;
    }
}

/// Arms the sanctuary's blocker cage: copies the first four normals, eight
/// corners and four quads of the template at `D_acropolis_sanctuary_801822EC`
/// into the live set at `D_acropolis_sanctuary_80183568`, selecting surface class
/// 1 for each copied quad, then slides all eight corners to where the cage belongs. Nibble 6
/// is the sanctuary cutscene flag: before the scene the cage sits across the
/// doorway, afterwards it is pushed 3000 units aside and out of the way.
static void func_acropolis_sanctuary_8017DD78(void)
{
    WorldCollisionGrid* dst = &D_acropolis_sanctuary_80183568;
    WorldCollisionGrid* src = &D_acropolis_sanctuary_801822EC;
    SVECTOR             shift;
    s32                 i;

    for (i = 0; i < 4; i++) {
        dst->normals[i].vx            = src->normals[i].vx;
        dst->normals[i].vy            = src->normals[i].vy;
        dst->normals[i].vz            = src->normals[i].vz;
        dst->vertices[i * 2].vx       = src->vertices[i * 2].vx;
        dst->vertices[i * 2].vy       = src->vertices[i * 2].vy;
        dst->vertices[i * 2].vz       = src->vertices[i * 2].vz;
        dst->vertices[(i * 2) + 1].vx = src->vertices[(i * 2) + 1].vx;
        dst->vertices[(i * 2) + 1].vy = src->vertices[(i * 2) + 1].vy;
        dst->vertices[(i * 2) + 1].vz = src->vertices[(i * 2) + 1].vz;
        dst->faces[i]                 = src->faces[i];
        dst->faces[i].surfaceClass    = 1;
    }

    if (gameFlagGetNibble(GAME_FLAG_SANCTUARY_BLOCKER_CLEARED) == 0) {
        shift.vx = 200;
        shift.vy = 0;
        shift.vz = 380;
    } else {
        shift.vx = 3000;
        shift.vy = 0;
        shift.vz = 0;
    }

    for (i = 0; i < 8; i++) {
        dst->vertices[i].vx += shift.vx;
        dst->vertices[i].vy += shift.vy;
        dst->vertices[i].vz += shift.vz;
    }
}

/// Selects one of the current room view's two alternative cached-sprite batches.
///
/// Only each argument's low byte is used. Zero `useSecondBatch` shows batch 1
/// and hides batch 2; nonzero reverses them. `viewId` is 1-based and must select
/// a live view with both records. The current stage and area must be valid and
/// `spriteVariant` must be 1. Visibility is retained in the room's batch list;
/// neither sprite sources nor cached packets are reallocated.
static void _acropolisSanctuarySelectViewSpriteBatch(s32 useSecondBatch, s32 viewId)
{
    GameSession*     session  = gGameSession;
    GameLocationKey* location = &session->location.loc;
    SpriteBatch*     batches;

    batches = Gp_SprtTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1][(viewId & ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_FIELD_MASK) - 1].batches;
    if ((useSecondBatch & ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_FIELD_MASK) == 0) {
        batches[1].hidden = 0;
        batches[2].hidden = 1;
    } else {
        batches[1].hidden = 1;
        batches[2].hidden = 0;
    }
}

/// State 0 of the sanctuary's effect task: spawns the twelve 0x6008B effects
/// the room is decorated with, six with spawn arg `0x200 + i` and six with
/// `0xA00000 + i`, each anchored to the task's own coordinate and offset by its
/// entry in `D_acropolis_sanctuary_80182774`. It then publishes the task's
/// handler table under pointer slot 5 and advances the state so the spawn runs
/// once. Every frame after that it mirrors the session's stage byte into
/// `D_acropolis_sanctuary_80182770`: 1 while the byte is 0x10, held while it is
/// 0xC, 0 otherwise.
void func_acropolis_sanctuary_8017E00C(Task* task)
{
    GfxCoord*        coord;
    GameLocationKey* sess;
    s32              i;

    coord = task->extra.coordBody->coord;
    if (task->state == 0) {
        for (i = 0; i < 6; i++) {
            Gp_SpawnEff(EFFECT_ACROPOLIS_SANCTUARY_FLAME, coord, i + 0x200, &D_acropolis_sanctuary_80182774[i]);
        }
        for (i = 6; i < 12; i++) {
            Gp_SpawnEff(EFFECT_ACROPOLIS_SANCTUARY_FLAME, coord, i + 0xA00000, &D_acropolis_sanctuary_80182774[i]);
        }
        task->msgTable = D_acropolis_sanctuary_80182310;
        gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
        D_acropolis_sanctuary_80182770 = 0;
        task->state                    = task->state + 1;
    }
    sess = &gGameSession->location.loc;
    if (sess->view == 0x10) {
        D_acropolis_sanctuary_80182770 = 1;
    } else if (sess->view != 0xC) {
        D_acropolis_sanctuary_80182770 = 0;
    }
}

/// State 0 of the sanctuary's mosaic task: spawns one 0x60079 effect per tile,
/// first for all 72 entries of `D_acropolis_sanctuary_80182320` keyed by their
/// own index, then a second pass over the 16 tiles listed in
/// `D_acropolis_sanctuary_80182750` keyed by the tile index itself, so those
/// sixteen get a second effect on top. Each spawn reuses the task's own
/// `EffectWork` offset triple: x is always 0, y and z come from the tile's
/// `originV` and `originU` scaled by 1145/128 and 2147/256 and shifted by the
/// origin corner of its `sizeClass`. Any state but 0 just releases the work
/// block.
void func_acropolis_sanctuary_8017E134(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    _AcropolisSanctuaryMosaicTile* tile;
    s32                            sizeClass;
    s32                            i;
    s32                            idx;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (arg0->state != 0) {
        effectKillTask(mem, arg0);
        return;
    }
    for (i = 0; i < 0x48; i++) {
        tile         = &D_acropolis_sanctuary_80182320[i];
        sizeClass    = tile->sizeClass;
        mem->move.vx = 0;
        mem->move.vy = ((tile->originV * 1145) >> 7) - D_acropolis_sanctuary_80182710[sizeClass][0].vy;
        mem->move.vz = -((tile->originU * 2147) >> 8) - D_acropolis_sanctuary_80182710[sizeClass][0].vz;
        Gp_SpawnEff(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_TILE, coord, i, &mem->move);
    }
    for (i = 0; i < 0x10; i++) {
        idx          = D_acropolis_sanctuary_80182750[i];
        tile         = &D_acropolis_sanctuary_80182320[idx];
        sizeClass    = tile->sizeClass;
        mem->move.vx = 0;
        mem->move.vy = ((tile->originV * 1145) >> 7) - D_acropolis_sanctuary_80182710[sizeClass][0].vy;
        mem->move.vz = -((tile->originU * 2147) >> 8) - D_acropolis_sanctuary_80182710[sizeClass][0].vz;
        Gp_SpawnEff(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_TILE, coord, idx, &mem->move);
    }
    arg0->state = arg0->state + 1;
}

/// Draws one frame of a whole mosaic tile: a semi-transparent textured quad
/// whose four corners are the size class's own corner offsets, rotated by the
/// task's own `workm` and then projected through `GsWSMATRIX`, staged in an
/// `_AcropolisSanctuaryMosaicTileScratch` block taken from the scratch stack.
/// The first corner goes through `rtps` and the other three through `rtpt`;
/// tiles inside `otz` 0x11 are dropped. The texture window runs from the
/// tile's `originU` / `originV` to that plus its `extentU` / `extentV`, so the
/// quad shows its own piece of the mosaic sheet at full size -- this is the
/// intact tile, `func_acropolis_sanctuary_8017EC90` draws the shards it breaks
/// into.
///
/// The drift (`move`) and spin (`pos`) are seeded from the LCG on the first
/// drawn frame, in one of two strengths chosen by the tile's `thrown`, which
/// `scale` keeps: a fast, wide-tumbling one for a thrown tile and a slow one
/// otherwise. The hold (`angle`) is the tile's `holdFrames`, plus a random
/// 0..7 for the slow kind, and `age` is the frame counter measured against it
/// -- the tile starts to drift once its age passes the hold, and the work
/// block is released 0x3C frames past that.
///
/// While drifting, a tile of the slow kind (`scale` zero) has a 1-in-60
/// chance per frame -- or a certainty once past y = -0xBFF -- of shedding one
/// to four 0x6007A shards, tagged 0x1000 so they spawn as the airborne
/// variant. Crossing x = -0x2740 above y = -0xED7 either shatters a
/// size-class-1 tile into two to four untagged shards or, one time in four
/// and always for size class 0, bounces it by halving and inverting the
/// vertical step. Either split costs 0x64 of life. Once the room flag is set
/// and the session is not in mode 0x10, tiles past x = -0x28C0 also age by
/// 0x3C, so they clear away.
void func_acropolis_sanctuary_8017E338(Task* arg0)
{
    EffectWork*                           mem;
    GfxCoord*                             coord;
    _AcropolisSanctuaryMosaicTileScratch* tileScratch;
    POLY_FT4*                             prim;
    SVECTOR*                              scratchCorner;
    s32                                   sizeClass;
    s32                                   i;
    s32                                   n;

    mem       = arg0->spawnArg2.pointer;
    sizeClass = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].sizeClass;
    coord     = arg0->extra.coordBody->coord;
    actorRenderComposeCoord(coord);
    tileScratch = SCRATCH_STACK_RESERVE_BLOCK(_AcropolisSanctuaryMosaicTileScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < ARRAY_SIZE(tileScratch->corners); i++) {
        tileScratch->corners[i].vx = D_acropolis_sanctuary_80182710[sizeClass][i].vx;
        // The same corner as `&tileScratch->corners[i]`, spelled as a byte
        // offset from the block so that it stays a separate pointer from the
        // one the GTE macros below take. Every typed spelling of this address
        // folds into that operand's register and the loop stops matching.
        scratchCorner     = (SVECTOR*)((u8*)tileScratch + i * sizeof(SVECTOR) + OFFSET_OF(_AcropolisSanctuaryMosaicTileScratch, corners));
        scratchCorner->vy = D_acropolis_sanctuary_80182710[sizeClass][i].vy;
        scratchCorner->vz = D_acropolis_sanctuary_80182710[sizeClass][i].vz;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&tileScratch->corners[i]);
        gte_rtv0();
        gte_stsv(&tileScratch->corners[i]);
        tileScratch->corners[i].vx += coord->workm.t[0];
        scratchCorner->vy          += coord->workm.t[1];
        scratchCorner->vz          += coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&tileScratch->corners[0]);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&tileScratch->corners[1], &tileScratch->corners[2], &tileScratch->corners[3]);
    gte_rtpt();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&tileScratch->otz);
    if (tileScratch->otz >= 0x11) {
        if (mem->age == 0) {
            mem->scale = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].thrown;
            if (mem->scale != 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = -((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vx     = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vy     = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vz     = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                mem->angle      = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].holdFrames;
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = -((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = (gRandomLcgState >> 16) & 7;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = 4 - ((gRandomLcgState >> 16) & 7);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vx     = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vy     = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vz     = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->angle      = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].holdFrames +
                             ((gRandomLcgState >> 16) & 7);
            }
        }
        prim->tpage = 0x8C;
        prim->clut  = 0x4200;
        prim->code |= 3;
        prim->u0    = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originU;
        prim->v0    = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originV;
        prim->u1    = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originU +
                   D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].extentU;
        prim->v1 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originV;
        prim->u2 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originU;
        prim->v2 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originV +
                   D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].extentV;
        prim->u3 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originU +
                   D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].extentU;
        prim->v3 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originV +
                   D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].extentV;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)tileScratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_AcropolisSanctuaryMosaicTileScratch);
    if (mem->angle + 0x3C < mem->age) {
        effectKillTask(mem, arg0);
        return;
    }
    if (mem->angle < mem->age) {
        coord->coord.t[0] += mem->move.vx;
        coord->coord.t[1] += mem->move.vy;
        coord->coord.t[2] += mem->move.vz;
        gfxRotMatrixYXZ(&coord->coord, &mem->pos, GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        mem->move.vy        = mem->move.vy + 3;
        if (mem->scale == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % 60U) == 0 || coord->coord.t[1] >= -0xBFF) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                n               = ((gRandomLcgState >> 16) & 3) + 1;
                for (i = 0; i < n; i++) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_SHARD, coord, arg0->spawnArg1.value | 0x1000, NULL);
                }
                mem->age = mem->age + 0x64;
            }
        }
    }
    if (coord->coord.t[0] < -0x2740 && coord->coord.t[1] >= -0xED7) {
        if (sizeClass != 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            // The size class is dead once it has been tested, so the shard
            // count reuses its local -- keeping the two apart costs `$s5`.
            sizeClass = (gRandomLcgState >> 16) & 3;
            if (sizeClass != 0) {
                sizeClass = sizeClass + 1;
                for (i = 0; i < sizeClass; i++) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_SHARD, coord, arg0->spawnArg1.value, NULL);
                }
                mem->age = mem->age + 0x64;
            } else {
                coord->coord.t[1] -= mem->move.vy * 2;
                mem->move.vy       = -(mem->move.vy >> 1);
            }
        } else {
            coord->coord.t[1] -= mem->move.vy * 2;
            mem->move.vy       = -(mem->move.vy >> 1);
        }
    }
    if (gGameSession->location.loc.view != 0x10 && D_acropolis_sanctuary_80182770 != 0 &&
        (coord->coord.t[0] < -0x28C0 ||
         (coord->coord.t[0] < -0x2740 && coord->coord.t[1] >= -0xED7))) {
        mem->age = mem->age + 0x3C;
    }
    mem->age = mem->age + 1;
}

/// Draws one frame of a mosaic shard: a semi-transparent textured triangle
/// whose three corners come from the first three corners of
/// `D_acropolis_sanctuary_80182710`, scaled about the origin by the shard's
/// size (`angle`) with the GTE's `gpf` interpolator and rotated by the
/// task's own `workm`, then projected through `GsWSMATRIX` with `rtpt` into an
/// `_AcropolisSanctuaryMosaicShardScratch` block taken from the scratch stack; shards inside `otz`
/// 0x11 are dropped. The texture window is the tile's `originU` / `originV`
/// corner and its `extentU` / `extentV` scaled by the same size factor, so the
/// shard shows its own piece of the mosaic sheet.
///
/// `Task::spawnArg1` is unpacked on the first frame: bits 12..15 select the
/// drift pattern, the high halfword is the size (defaulting to 0x1000) and only
/// the low 12 bits are kept, as the index into the tile table. The same frame
/// seeds the per-frame drift (`move`) and spin
/// (`pos`) from the LCG -- pattern 0 falls faster, since its
/// vertical step is seeded negative.
///
/// Each frame the shard drifts by that step, gains 3 of downward speed, and is
/// respun. Crossing x = -0x2740 above y = -0xED7 either bounces it (halving and
/// inverting the vertical step, twice at most) or, for shards at least 0x401
/// big, shatters it into one or two 0x6007A children; a big shard also has a
/// 1-in-60 chance per frame -- or a certainty once past y = -0xBFF -- of
/// splitting into two. Either way the split costs 0x3C of life. Once the room
/// flag is set and the session is not in mode 0x10, shards past x = -0x28C0
/// also age by 0x3C, so they clear away.
void func_acropolis_sanctuary_8017EC90(Task* arg0)
{
    EffectWork*                            mem;
    GfxCoord*                              coord;
    _AcropolisSanctuaryMosaicShardScratch* shardScratch;
    POLY_FT3*                              prim;
    SVECTOR*                               corner;
    s32                                    size;
    s32                                    hi;
    s32                                    i;
    s32                                    n;
    s32                                    flags;
    SVECTOR*                               scratchCorner;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (mem->age >= 0x3D || mem->index >= 2) {
        effectKillTask(mem, arg0);
        return;
    }
    actorRenderComposeCoord(coord);
    shardScratch = SCRATCH_STACK_RESERVE_BLOCK(_AcropolisSanctuaryMosaicShardScratch);
    if (mem->age == 0) {
        mem->scale = (arg0->spawnArg1.value >> 12) & 0xF;
        hi         = (s16)(arg0->spawnArg1.value >> 16);
        size       = 0x1000;
        if ((u16)hi != 0) {
            size = hi;
        }
        mem->angle            = size;
        arg0->spawnArg1.value = arg0->spawnArg1.value & 0xFFF;
        if (mem->scale != 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vx    = 8 - ((gRandomLcgState >> 16) & 0xF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vy    = (gRandomLcgState >> 16) & 0xF;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vz    = 8 - ((gRandomLcgState >> 16) & 0xF);
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vx    = 8 - ((gRandomLcgState >> 16) & 0xF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vy    = -((gRandomLcgState >> 16) & 0x1F);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vz    = 8 - ((gRandomLcgState >> 16) & 0xF);
        }
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->pos.vx     = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->pos.vy     = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->pos.vz     = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    corner = D_acropolis_sanctuary_80182710[0];
    for (i = 0; i < 3; i++) {
        shardScratch->corners[i].vx = corner[i].vx;
        // The same corner as `&shardScratch->corners[i]`, spelled as a byte
        // offset from the block so that it stays a separate pointer from the
        // one the GTE macros below take. Every typed spelling of this address
        // folds into that operand's register and the loop stops matching.
        scratchCorner     = (SVECTOR*)((u8*)shardScratch + i * sizeof(SVECTOR) + OFFSET_OF(_AcropolisSanctuaryMosaicShardScratch, corners));
        scratchCorner->vy = corner[i].vy;
        scratchCorner->vz = corner[i].vz;
        gte_lddp(mem->angle);
        gte_ldsv(&shardScratch->corners[i]);
        gte_gpf12();
        gte_stsv(&shardScratch->corners[i]);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&shardScratch->corners[i]);
        gte_rtv0();
        gte_stsv(&shardScratch->corners[i]);
        shardScratch->corners[i].vx += coord->workm.t[0];
        scratchCorner->vy           += coord->workm.t[1];
        scratchCorner->vz           += coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv3(&shardScratch->corners[0], &shardScratch->corners[1], &shardScratch->corners[2]);
    gte_rtpt();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 7);
    setcode(prim, 0x24);
    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
    gte_stszotz(&shardScratch->otz);
    if (shardScratch->otz >= 0x11) {
        prim->tpage = 0x8C;
        prim->clut  = 0x4200;
        prim->code |= 3;
        prim->u0    = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originU;
        prim->v0    = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originV;
        prim->u1    = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originU +
                   ((D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].extentU * mem->angle) >> 12);
        prim->v1 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originV;
        prim->u2 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originU;
        prim->v2 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].originV +
                   ((D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].extentV * mem->angle) >> 12);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)shardScratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    coord->coord.t[0] += mem->move.vx;
    coord->coord.t[1] += mem->move.vy;
    SCRATCH_STACK_RELEASE_BLOCK(_AcropolisSanctuaryMosaicShardScratch);
    coord->coord.t[2] += mem->move.vz;
    gfxRotMatrixYXZ(&coord->coord, &mem->pos, GRAPHICS_ROTATION_COMPOSE);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    mem->move.vy        = mem->move.vy + 3;
    if (coord->coord.t[0] < -0x2740 && coord->coord.t[1] >= -0xED7) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        n               = (gRandomLcgState >> 16) & 1;
        if (mem->angle >= 0x401 && n != 0) {
            n = n + 1;
            for (i = 0; i < n; i++) {
                Gp_SpawnEff(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_SHARD, coord, arg0->spawnArg1.value | (mem->angle << 15), NULL);
            }
            mem->age = mem->age + 0x3C;
        } else {
            coord->coord.t[1] -= mem->move.vy * 2;
            mem->move.vy       = -(mem->move.vy >> 1);
            mem->index         = mem->index + 1;
        }
    } else if (mem->angle >= 0x401) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((u16)((gRandomLcgState >> 16) % 60U) == 0 || coord->coord.t[1] >= -0xBFF) {
            for (i = 0; i < 2; i++) {
                flags = (mem->angle << 15) | 0x1000;
                Gp_SpawnEff(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_SHARD, coord, arg0->spawnArg1.value | flags, NULL);
            }
            mem->age = mem->age + 0x3C;
        }
    }
    if (gGameSession->location.loc.view != 0x10 && D_acropolis_sanctuary_80182770 != 0 &&
        (coord->coord.t[0] < -0x28C0 ||
         (coord->coord.t[0] < -0x2740 && coord->coord.t[1] >= -0xED7))) {
        mem->age = mem->age + 0x3C;
    }
    mem->age = mem->age + 1;
}

void acropolisSanctuaryFlameTask(Task* task)
{
    enum {
        ACROPOLIS_SANCTUARY_FLAME_STATE_INITIAL    = 0,
        ACROPOLIS_SANCTUARY_FLAME_PLACEMENT_MASK   = 0xF,
        ACROPOLIS_SANCTUARY_FLAME_VARIANT_SHIFT    = 8,
        ACROPOLIS_SANCTUARY_FLAME_VARIANT_MASK     = 3,
        ACROPOLIS_SANCTUARY_FLAME_SIZE_SHIFT       = 16,
        ACROPOLIS_SANCTUARY_FLAME_SIZE_MASK        = 0xFFF,
        ACROPOLIS_SANCTUARY_FLAME_SIZE_BITS        = ACROPOLIS_SANCTUARY_FLAME_SIZE_MASK << ACROPOLIS_SANCTUARY_FLAME_SIZE_SHIFT,
        ACROPOLIS_SANCTUARY_FLAME_DEFAULT_SIZE     = 640,
        ACROPOLIS_SANCTUARY_FLAME_MIN_DEPTH        = 17,
        ACROPOLIS_SANCTUARY_FLAME_TEXTURE_PAGE     = getTPage(0, GPU_BLEND_ADD, 704, 0),
        ACROPOLIS_SANCTUARY_FLAME_PALETTE_WORDS    = 16,
        ACROPOLIS_SANCTUARY_FLAME_PALETTE_Y        = 270,
        ACROPOLIS_SANCTUARY_FLAME_TEXTURE_STRIDE_U = 40,
        ACROPOLIS_SANCTUARY_FLAME_TEXTURE_EXTENT   = 39,
    };
    EffectWork*                  flameWork;
    GfxCoord*                    flameCoord;
    RoomGlowSpriteScratch*       flameScratch;
    POLY_FT4*                    flameQuad;
    _AcropolisSanctuaryFlameGrey baseGrey;
    _AcropolisSanctuaryFlameGrey flickerGrey;
    s32                          spawnParams;
    s32                          greyLevel;

    flameWork  = task->spawnArg2.pointer;
    flameCoord = task->extra.coordBody->coord;
    if ((D_acropolis_sanctuary_801827D4[task->spawnArg1.value & ACROPOLIS_SANCTUARY_FLAME_PLACEMENT_MASK] >> (gGameSession->location.loc.view - 1)) & 1) {
        actorRenderComposeCoord(flameCoord);
        flameScratch = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
        if (task->state == ACROPOLIS_SANCTUARY_FLAME_STATE_INITIAL) {
            // Initialize on the first visible update, then retain only the placement index.
            baseGrey              = D_acropolis_sanctuary_8017D5D8;
            flickerGrey           = D_acropolis_sanctuary_8017D5DC;
            spawnParams           = task->spawnArg1.value;
            flameWork->scale      = (spawnParams & ACROPOLIS_SANCTUARY_FLAME_SIZE_BITS)
                                        ? ((spawnParams >> ACROPOLIS_SANCTUARY_FLAME_SIZE_SHIFT) & ACROPOLIS_SANCTUARY_FLAME_SIZE_MASK)
                                        : ACROPOLIS_SANCTUARY_FLAME_DEFAULT_SIZE;
            flameWork->angle      = (task->spawnArg1.value >> ACROPOLIS_SANCTUARY_FLAME_VARIANT_SHIFT) & ACROPOLIS_SANCTUARY_FLAME_VARIANT_MASK;
            task->spawnArg1.value = task->spawnArg1.value & ACROPOLIS_SANCTUARY_FLAME_PLACEMENT_MASK;
            flameWork->period     = baseGrey.grey[flameWork->angle];
            flameWork->step       = flickerGrey.grey[flameWork->angle];
            task->state++;
        }
        // Project the composed centre after narrowing it to signed 16-bit coordinates.
        flameScratch->worldPos.vx = flameCoord->workm.t[0];
        flameScratch->worldPos.vy = flameCoord->workm.t[1];
        flameScratch->worldPos.vz = flameCoord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&flameScratch->worldPos);
        gte_rtps();
        // Visible placements consume a packet even when its projected depth rejects it.
        flameQuad      = gGpuPrimCursor;
        gGpuPrimCursor = flameQuad + 1;
        setPolyFT4(flameQuad);
        gte_stsxy(&flameScratch->screenPos);
        gte_stszotz(&flameScratch->otz);
        if (flameScratch->otz >= ACROPOLIS_SANCTUARY_FLAME_MIN_DEPTH) {
            greyLevel        = (u8)flameWork->period + (gDisplayState.animFrame & 1) * flameWork->step;
            flameQuad->tpage = ACROPOLIS_SANCTUARY_FLAME_TEXTURE_PAGE;
            setSemiTrans(flameQuad, 1);
            setRGB0(flameQuad, greyLevel, greyLevel, greyLevel);
            flameQuad->clut          = getClut(flameWork->angle * ACROPOLIS_SANCTUARY_FLAME_PALETTE_WORDS, ACROPOLIS_SANCTUARY_FLAME_PALETTE_Y);
            flameQuad->u0            = flameWork->angle * ACROPOLIS_SANCTUARY_FLAME_TEXTURE_STRIDE_U;
            flameQuad->v0            = 0;
            flameQuad->u1            = flameWork->angle * ACROPOLIS_SANCTUARY_FLAME_TEXTURE_STRIDE_U + ACROPOLIS_SANCTUARY_FLAME_TEXTURE_EXTENT;
            flameQuad->v1            = 0;
            flameQuad->u2            = flameWork->angle * ACROPOLIS_SANCTUARY_FLAME_TEXTURE_STRIDE_U;
            flameQuad->v2            = ACROPOLIS_SANCTUARY_FLAME_TEXTURE_EXTENT;
            flameQuad->u3            = flameWork->angle * ACROPOLIS_SANCTUARY_FLAME_TEXTURE_STRIDE_U + ACROPOLIS_SANCTUARY_FLAME_TEXTURE_EXTENT;
            flameQuad->v3            = ACROPOLIS_SANCTUARY_FLAME_TEXTURE_EXTENT;
            flameScratch->halfExtent = (flameWork->scale * ACROPOLIS_SANCTUARY_FLAME_TEXTURE_EXTENT) / flameScratch->otz;
            _acropolisSanctuarySetFlameQuadBounds(flameQuad, flameScratch);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)flameScratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    flameQuad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

/// Spawns effect 0x60078 on the room task's model coordinate, seeded with the
/// fixed offset vector `D_acropolis_sanctuary_8017D5D0`. Always consumes the event
/// (returns 0).
s32 func_acropolis_sanctuary_8017F918(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    GfxCoord* coord = task->extra.tmd->coords;
    SVECTOR   vec   = D_acropolis_sanctuary_8017D5D0;

    Gp_SpawnEff(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC, coord, 0, &vec);
    return 0;
}

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/actor_contacts_push.inc.c"

/// Per-frame visibility gate for the sanctuary's item object: hides the model
/// (`field_C` bit 0x80) while the camera sits on view 0xB or 0xD, or once the
/// item's 2-bit pickup flag has reached 2; otherwise shows it again with the
/// default flags.
void func_acropolis_sanctuary_80180264(Task* task)
{
    Enemy*     enemy = task->spawnArg2.pointer;
    TmdObject* tmd   = task->extra.tmd;
    s32        flag;
    s32        view;

    flag = areaGetCurrentObjectState((u8)enemy->placeKey);
    view = viewGetMappedIndex();
    if (view == 0xB || view == 0xD || flag == 2) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
    }
}

/// Per-frame visibility hook for an item object: hides the model (`flags`
/// 0x80) once the item's 2-bit pickup flag has reached 2, otherwise shows it
/// with the default flags. The current view is queried but not used.
static void func_acropolis_sanctuary_801802E0(Task* task)
{
    Enemy*     enemy;
    TmdObject* tmd;
    s32        flag;

    enemy = task->spawnArg2.pointer;
    tmd   = task->extra.tmd;
    flag  = areaGetCurrentObjectState((u8)enemy->placeKey);
    viewGetMappedIndex();
    if (flag == 2) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
    }
}
