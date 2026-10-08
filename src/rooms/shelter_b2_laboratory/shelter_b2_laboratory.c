#include "rooms/shelter_b2_laboratory.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
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
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stage_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"
// Symbol is `RoomCutsceneSoundTaskStorage`; the runner reads `task`.
#define ROOM_CUTSCENE_SOUND_TASK gRoomCutsceneSoundTask.task
#include "../../shared/room_cutscene.h"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

/// Scratch block this room's diamond glow drawer takes from the scratch stack
/// for one projected centre.
///
/// One perspective transform of a world point through `gGfxViewCoord.workm`
/// writes the screen position and the GTE flag word. A negative flag word
/// means the transform reported an error, and the drawer links nothing.
/// Otherwise it stores the ordering-table depth and the on-screen half-extent,
/// a size divided by that depth, and builds the diamonds around the centre.
/// `sx` and `sy` are written by one screen-XY store, so they stay adjacent.
///
/// `GlowCentreScratch` holds the same four values in 16 bytes. This record
/// keeps eight more bytes between the half-extent and the screen position;
/// the drawer reserves and releases them with the rest of the block and never
/// reads or writes them, so the records stay separate.
typedef struct {
    s32 otz;        // Ordering-table depth of the centre; also the divisor for the half-extent
    s32 flag;       // GTE flag word; negative means the transform reported an error
    s32 radius;     // On-screen half-extent of the diamonds around the centre
    u8  unknown[8]; // Reserved with the block and never accessed; role and grouping unproven
    u16 sx;         // Projected centre, x
    u16 sy;         // Projected centre, y
} _ShelterB2LaboratoryGlowDiamondScratch;
STATIC_ASSERT_SIZEOF(_ShelterB2LaboratoryGlowDiamondScratch, 0x18);

extern UiObjectDesc D_800611E4;

extern TaskDesc D_actor_143000_80134564;

/// View saved when the cutscene starts and restored when it ends.

/// Telephone menu title, including retained bytes after its terminator.
static const char Telephone_Data_8017D638[];

/// Captions of the telephone menu's rows ("Save", "Play Data", "Weapon Data",
/// "PE Data").
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// Row captions of the play-data panel, one per row.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// Suffix appended after a plain count on rows 1, 2, 3 and 6 of the play-data
/// panel.
static u8 Telephone_Data_80181A70[];

/// Suffix appended after a percentage.
static u8 Telephone_Data_80181A78[];

/// Help strings handed to the UI holder while the cursor rests on a row of
/// the play-data panel, one per row.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// Lists of the play-data panel, the usage panel and the telephone menu, and
/// the descriptors of the panels they spawn.
static UiList       Telephone_Data_80181C44;
static UiList       Telephone_Data_80181C6C;
static UiObjectDesc Telephone_Data_80181C90;
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;
static UiList       Telephone_Data_80181CF4;

/// Task tables the room spawns from: the cutscene, the flag-gated exit's
/// transition, and the console's tasks.
extern TaskDesc gRoomCutsceneTaskDescs[];
extern TaskDesc gRoomEventTaskDesc;
extern TaskDesc D_shelter_b2_laboratory_80182A6C[];

/// Message table of the room's message task.
extern TaskMessageEntry D_shelter_b2_laboratory_80182A38[];

/// Task spawned by `_shelterB2LaboratoryConsoleKeypadTask` and polled until it
/// dies.
extern Task* D_shelter_b2_laboratory_80182A68;

/// Per-view attenuation, in percent, of the looping sound played by
/// `_shelterB2LaboratoryAmbienceTask`.
extern s8 D_shelter_b2_laboratory_80182A90[];

/// Glow positions `shelterB2LaboratoryGlowTask` draws per view:
/// `_glowDrawCapsule` takes the pair `pt[n]`, `pt[n + 1]`;
/// `_shelterB2LaboratoryDrawGlowDiamond` and
/// `_glowDrawPulsingDisc` a single point.
extern SVECTOR D_shelter_b2_laboratory_80182AA0[45];

/// This room's cutscene sound-task slot. The runner uses `task`.
extern RoomCutsceneSoundTaskStorage gRoomCutsceneSoundTask;

/// Copies of the message and request that started the pending exit
/// transition, read back by `roomEventTask`.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Set when `_roomEventGate` started a transition,
/// cleared on every other call.
extern u8 gRoomEventActive;

/// Non-zero while the looping sound of `_shelterB2LaboratoryAmbienceTask`
/// should keep playing; set by `shelterB2LaboratoryStartAmbience`.
extern s32 D_shelter_b2_laboratory_801864B8;

/// Parameters of the cutscene `_shelterB2LaboratoryCommandMessage` starts.
extern RoomCutsceneRecStorage D_shelter_b2_laboratory_801864BC;

/// World position the looping sound is panned and attenuated from.
extern GfxCoord D_shelter_b2_laboratory_801864DC;

/// Non-zero makes the view glows of `shelterB2LaboratoryGlowTask`
/// pulse faster. Written through `_shelterB2LaboratorySetFastGlowPulse`; the
/// glow task clears it when it starts.
extern u16 D_shelter_b2_laboratory_80186540;

#define TELEPHONE_TITLE_BYTES "Telephone\0\xFA\xA9"
#include "../../shared/telephone.h"

static s32  _shelterB2LaboratoryRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg);
static void _shelterB2LaboratoryInitRoomTask(Task* task);
static void _shelterB2LaboratoryIdleMessageTask(Task* task);
static void _shelterB2LaboratoryDrawGlowDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _shelterB2LaboratorySetFastGlowPulse(s16 enabled);

/// Values used by room events to select the cyan glow's pulse rate.
enum {
    SHELTER_B2_LABORATORY_GLOW_PULSE_SLOW = 0,
    SHELTER_B2_LABORATORY_GLOW_PULSE_FAST = 1,
};

static s32  _shelterB2LaboratoryCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg);
static s32  _shelterB2LaboratoryResolveRoomTransition(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelterB2LaboratoryConsoleActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32  _shelterB2LaboratoryCueSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueId, s32 unusedSecondArg);
static void _shelterB2LaboratoryAmbienceTask(Task* task);
static void _shelterB2LaboratoryConsoleKeypadTask(Task* task);
static void _shelterB2LaboratoryFinishConsoleSceneTask(Task* task);

extern WorldCollisionSurfaceProperties D_shelter_b2_laboratory_80186450[1];
extern WorldCollisionSurfaceProperties D_shelter_b2_laboratory_80186458[1];
extern WorldCollisionSurfaceProperties D_shelter_b2_laboratory_80186460[1];

#include "../../shared/telephone_data.inc.c"

static TmdBone _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Skeleton[3] = {
#include "assets/acropolis_sanctuary_model_090F0_skeleton.inc"
};

static u32 _gShelterB2LaboratoryAcropolisSanctuaryModel090F0PartVerts[3] = {
#include "assets/acropolis_sanctuary_model_090F0_partVerts.inc"
};

static SVECTOR _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Verts[56] = {
#include "assets/acropolis_sanctuary_model_090F0_verts.inc"
};

static SVECTOR _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Normals[6] = {
#include "assets/acropolis_sanctuary_model_090F0_normals.inc"
};

static u32 _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Stream[215] = {
#include "assets/acropolis_sanctuary_model_090F0_stream.inc"
};

TmdSource gShelterB2LaboratoryAcropolisSanctuaryModel090F0 = {
    0,
    1768,
    0,
    3,
    _gShelterB2LaboratoryAcropolisSanctuaryModel090F0PartVerts,
    _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Verts,
    _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Normals,
    _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Skeleton,
    _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Stream,
};

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

/// Key-item menu request handled by this room's message task.
enum { SHELTER_B2_LABORATORY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskMessageEntry D_shelter_b2_laboratory_80182A38[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB2LaboratoryResolveRoomTransition },
    { SHELTER_B2_LABORATORY_MESSAGE_USE_KEY_ITEM, _shelterB2LaboratoryRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB2LaboratoryConsoleActionMessage },
    { ROOM_MESSAGE_COMMAND, _shelterB2LaboratoryCommandMessage },
    { ROOM_MESSAGE_SOUND, _shelterB2LaboratoryCueSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

Task* D_shelter_b2_laboratory_80182A68 = NULL;

TaskDesc D_shelter_b2_laboratory_80182A6C[3] = {
    { { { TASK_BODY_NONE, 32 } }, _shelterB2LaboratoryConsoleKeypadTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _shelterB2LaboratoryAmbienceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _shelterB2LaboratoryFinishConsoleSceneTask, { .value = 0 } },
};

s8 D_shelter_b2_laboratory_80182A90[16] = {
    0,
    0,
    60,
    30,
    45,
    100,
    75,
    40,
    30,
    0,
    0,
    0,
    0,
    0,
    0,
    100,
};

SVECTOR D_shelter_b2_laboratory_80182AA0[45] = {
    { 182, -1966, 5669, 0 },
    { 823, -1966, 5669, 0 },
    { 3202, -1966, 5669, 0 },
    { 3843, -1966, 5669, 0 },
    { 1642, -2807, 4491, 0 },
    { 2462, -2807, 4491, 0 },
    { 1642, -2807, 4351, 0 },
    { 2462, -2807, 4351, 0 },
    { 1709, -2807, 1442, 0 },
    { 2529, -2807, 1442, 0 },
    { 1709, -2807, 1302, 0 },
    { 2529, -2807, 1302, 0 },
    { 1709, -2807, -1369, 0 },
    { 2529, -2807, -1369, 0 },
    { 1709, -2807, -1511, 0 },
    { 2529, -2807, -1511, 0 },
    { 8935, -3150, 1448, 0 },
    { 9762, -3150, 1448, 0 },
    { 8935, -3150, 1306, 0 },
    { 9762, -3150, 1306, 0 },
    { 8902, -3150, -1343, 0 },
    { 9721, -3150, -1343, 0 },
    { 8902, -3150, -1483, 0 },
    { 9721, -3150, -1483, 0 },
    { 12886, -2946, -1615, 0 },
    { 12886, -2946, -2278, 0 },
    { -339, -2195, 2398, 0 },
    { -339, -2195, 1547, 0 },
    { 6018, -2111, 3328, 0 },
    { 6806, -2111, 3328, 0 },
    { -278, -2407, -1452, 0 },
    { -278, -2407, -1091, 0 },
    { -278, -2407, -780, 0 },
    { -278, -2407, -418, 0 },
    { -278, -2407, -97, 0 },
    { -278, -2407, 262, 0 },
    { 11312, -3085, -362, 0 },
    { 11312, -3085, -1, 0 },
    { 11312, -3085, 1054, 0 },
    { 11312, -3085, 1418, 0 },
    { 11312, -3085, 2483, 0 },
    { 11312, -3085, 2846, 0 },
    { 3100, -1163, -966, 0 },
    { 3472, -1163, -966, 0 },
    { 3181, -1161, -3312, 0 },
};

u8* D_shelter_b2_laboratory_80182C08[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b2_laboratory_80182C0C[1] = { 15 };

DirectionWarpEntry D_shelter_b2_laboratory_80182C10[2] = {
    { { { .word = 1024 }, 100, 0, 1800 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 100, 0, 1800 }, { 0, 0, 0, 0 }, 0x541F0004, 0x541F0003, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 451 },
    { { { .word = 2048 }, 6350, 0, 2900 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 6350, 0, 2900 }, { 0, 0, 0, 0 }, 0x541F0002, 0x541F0001, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB2LaboratoryCollision05F9CNormals[23] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_normals.inc"
};

static SVECTOR _gShelterB2LaboratoryCollision05F9CVerts[123] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_verts.inc"
};

static WorldCollisionGridFace _gShelterB2LaboratoryCollision05F9CFaces[51] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_faces.inc"
};

static s16 _gShelterB2LaboratoryCollision05F9CCells[220] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB2LaboratoryCollision05F9CCells[i])
static s16* _gShelterB2LaboratoryCollision05F9CTable[12] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b2_laboratory_8018355C = { NULL, _gShelterB2LaboratoryCollision05F9CNormals, _gShelterB2LaboratoryCollision05F9CVerts, _gShelterB2LaboratoryCollision05F9CFaces, _gShelterB2LaboratoryCollision05F9CTable, 1411, 4736, 4, 3, 4000, 51 };

ViewCamera D_shelter_b2_laboratory_80183580[15] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -6200, 0x7530, -1480 } }, 564 },
    { { { { 1318, 0, 3878 }, { 1296, 3860, -440 }, { -3655, 1369, 1242 } }, { -7992, 1967, -470 } }, 230 },
    { { { { 3662, 0, 1834 }, { 408, 3992, -816 }, { -1788, 912, 3570 } }, { -4133, 1597, -776 } }, 230 },
    { { { { 1559, 0, -3787 }, { -761, 4012, -313 }, { 3709, 823, 1527 } }, { -3665, 1388, -448 } }, 230 },
    { { { { 922, 0, 3990 }, { 799, 4012, -184 }, { -3909, 820, 904 } }, { -8742, 1568, 3074 } }, 230 },
    { { { { 988, 0, -3974 }, { -1089, 3939, -270 }, { 3822, 1122, 950 } }, { -4027, 2154, 3224 } }, 230 },
    { { { { 1535, 0, -3797 }, { -1082, 3926, -437 }, { 3639, 1167, 1471 } }, { -8309, 3056, 3056 } }, 230 },
    { { { { 3861, 0, -1367 }, { -277, 4010, -783 }, { 1338, 830, 3780 } }, { -9759, 2782, 2019 } }, 230 },
    { { { { -3525, 0, -2084 }, { 56, 4094, -95 }, { 2083, -110, -3524 } }, { -2270, 1220, -572 } }, 380 },
    { { { { 3982, 0, -958 }, { 27, 4094, 112 }, { 958, -115, 3980 } }, { -2860, 1220, 3450 } }, 541 },
    { { { { 4095, 0, 0 }, { 0, 4091, -197 }, { 0, 197, 4091 } }, { 0x2E72, 1215, 1870 } }, 541 },
    { { { { -794, 0, 4018 }, { -178, 4091, -35 }, { -4014, -182, -793 } }, { -6219, 895, 1568 } }, 269 },
    { { { { -4036, 0, 695 }, { 136, 4016, 793 }, { -682, 804, -3957 } }, { -3499, 1404, 2437 } }, 269 },
    { { { { 4095, 0, 0 }, { 0, 4091, -197 }, { 0, 197, 4091 } }, { 0x2E72, 1215, 1870 } }, 541 },
    { { { { 862, 0, 4004 }, { 1204, 3906, -259 }, { -3818, 1231, 822 } }, { -6593, 1887, 2833 } }, 246 },
};

SpriteBatch D_shelter_b2_laboratory_8018379C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_801837AC[51] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 40, 0, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -144, 16, 0, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 24, 0, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 32, 0, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 16, 535, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 24, 521, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 40, 519, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 48, 474, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 48, 504, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 64, 471, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 64, 423, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 80, 402, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 80, 510, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 48, 0, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 88, 507, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 104, 501, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 80, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 112, 504, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 96, 351, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -8, 0, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 128, -48, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 0, 0, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 8, 0, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 16, 0, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, -72, 743, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, -72, 783, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, -32, 826, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, -72, 856, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, -40, 923, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, -72, 965, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, -24, 994, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -72, 1011, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, -32, 1042, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -32, 1096, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -72, 1065, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -72, 1110, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -32, 1159, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 16, 1140, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 8, 1212, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, -8, 1194, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -40, 1147, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 0, 1036, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -72, 1065, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -72, 1100, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -40, 1144, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -24, 1162, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -72, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -24, 0, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -16, 1447, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -24, 1456, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -24, 1480, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_80183BA8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 28, 0, 0, { 2, 0 } },
    { 47, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_80183BD0[20] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -80, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -24, 0, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -24, 0, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 16, 0, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -80, 0, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, -80, 375, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 112, -56, 375, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 112, -32, 375, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 104, 0, 375, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 104, 24, 375, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, 56, 0, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 64, 0, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 88, 0, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 96, 375, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, 88, 375, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 80, 375, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 56, 375, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 64, 375, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 88, 375, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 104, 0, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_80183D60[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_80183D78[30] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 48, -40, 1397, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -40, 1311, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 80, -40, 1267, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -40, 1048, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 16, 0, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 40, 0, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 72, 609, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 136, 48, 564, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 136, 16, 555, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 0, 560, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -88, 0, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -128, -64, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 0, 0, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, 24, 0, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 56, 678, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 64, 0, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 56, 709, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 48, 730, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 32, 736, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 32, 738, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 16, 727, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 0, 690, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 0, 643, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -88, 638, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, -64, 636, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, -40, 630, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, -16, 637, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, -64, 636, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, -40, 642, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -16, 648, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_80183FD0[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 6, 0, 0, { 2, 0 } },
    { 10, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_80183FF8[23] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -32, 0, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -24, 0, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -16, 0, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 48, 0, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 104, 72, 0, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 64, 0, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, 48, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, -32, 698, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -32, 523, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, -32, 767, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, -32, 0, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -8, 903, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 0, 778, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 0, 717, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 0, 711, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, 0, 606, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, 0, 529, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 120, 48, 536, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 112, 32, 574, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 72, 16, 727, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, 48, 813, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 48, 578, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 16, 619, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_801841C4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_laboratory_801841DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_801841EC[99] = {
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 64, 0, { .fields = { 64, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, 64, 0, { .fields = { 80, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 72, 0, { .fields = { 32, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -96, 80, 0, { .fields = { 104, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -64, 88, 0, { .fields = { 112, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 96, 0, { .fields = { 56, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 8, 96, 0, { .fields = { 48, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 88, 0, { .fields = { 56, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 24, 72, 0, { .fields = { 104, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 56, 0, { .fields = { 64, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 40, 0, { .fields = { 80, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -16, -8, 0, { .fields = { 32, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -160, -8, 0, { .fields = { 104, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -136, -8, 0, { .fields = { 88, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -96, -8, 0, { .fields = { 8, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -16, 0, 0, { .fields = { 48, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -16, 8, 0, { .fields = { 56, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, 16, 0, { .fields = { 56, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, 24, 0, { .fields = { 64, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 32, 0, { .fields = { 80, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, -8, 899, { .fields = { 120, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 16, 899, { .fields = { 0, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, 0, 826, { .fields = { 56, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 32, 859, { .fields = { 0, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 8, 707, { .fields = { 104, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 40, 734, { .fields = { 96, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 16, 627, { .fields = { 80, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, 56, 665, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 32, 558, { .fields = { 72, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 64, 570, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 40, 507, { .fields = { 56, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 72, 514, { .fields = { 64, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, 32, 503, { .fields = { 64, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, 64, 527, { .fields = { 80, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, 32, 520, { .fields = { 64, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, 64, 545, { .fields = { 64, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -40, -8, 0, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -8, 0, { .fields = { 32, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, 24, 536, { .fields = { 32, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, 56, 563, { .fields = { 8, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 24, 570, { .fields = { 0, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -96, 56, 597, { .fields = { 96, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, 16, 602, { .fields = { 96, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 40, 632, { .fields = { 32, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 16, 653, { .fields = { 0, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 40, 683, { .fields = { 0, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 8, 647, { .fields = { 72, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 16 } }, 40, 0, 811, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 32, 32, 856, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 40, 16, 827, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 32, 8, 0, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 8 } }, 32, 56, 787, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, 32, 64, 0, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 104 } }, 24, 16, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 96 } }, 16, 24, 0, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -32, -16, 0, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -96, -16, 0, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -160, -16, 0, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -24, -8, 0, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -24, 0, 0, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, -160, 8, 0, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -160, 16, 0, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, -112, 16, 0, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -72, 24, 0, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -112, 24, 0, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -160, 24, 0, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 24 } }, -160, 32, 0, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -128, 56, 639, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 64 } }, -160, 56, 0, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 48 } }, -128, 72, 0, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 24 } }, -112, 32, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -72, 32, 0, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -96, 56, 0, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -96, 88, 0, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 64 } }, -64, 56, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -40, 32, 0, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -40, 40, 0, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 48 } }, -40, 72, 0, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -8, 40, 490, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -8, 64, 508, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -8, 88, 526, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 16 } }, 0, 24, 491, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 8 } }, 32, -16, 750, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 8 } }, 24, -8, 707, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 8 } }, 16, 0, 713, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 8, 8, 688, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 0, 16, 480, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -8, 16, 467, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, -24, 8, 0, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -16, 16, 474, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -24, 16, 481, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -32, 8, 487, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -40, 8, 503, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 16 } }, -80, 8, 516, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -80, -8, 0, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -80, 0, 0, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 16 } }, -128, 0, 562, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -128, -8, 0, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -160, -8, 619, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_801849A8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 47, 0, 0, { 1, 0 } },
    { 47, 52, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_801849C8[78] = {
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -64, -32, 0, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, -56, -16, 0, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -48, 0, 0, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -40, 8, 0, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -32, 24, 0, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 40, 0, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -16, 48, 0, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 64, 0, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 80, 0, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 80, 0, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 64, 0, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 48, 0, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 40, 0, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, -8, 0, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -24, 104, 225, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 104, 315, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 96, 238, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -32, 80, 253, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 96, 309, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 80, 324, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 64, 278, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -48, 48, 311, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 40, 343, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 24, 385, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 48, 661, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 16, 644, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 16, 442, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 8, 476, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 16, 1014, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 0, 960, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 0, 541, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, -8, 631, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, -16, 779, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, -32, 1112, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -80, 0, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -64, 0, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, -56, 0, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, 0, 0, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 16, 1879, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -80, 1718, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 32, -64, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -80, 1714, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 8, 1869, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -8, 1862, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -24, 1795, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -64, 1750, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -40, 1750, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, 24, 0, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -56, 0, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -80, 713, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, -56, 713, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, -32, 725, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, -8, 750, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 8, 776, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 32, 803, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 24 } }, -64, -24, 0, { .fields = { 48, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 40, 16 } }, -56, 0, 0, { .fields = { 16, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, -48, 16, 0, { .fields = { 56, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 24 } }, -40, 40, 0, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, -32, 64, 0, { .fields = { 64, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 24 } }, -24, 80, 0, { .fields = { 96, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, -80, 112, 0, { .fields = { 0, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, -80, 72, 0, { .fields = { 88, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, -80, 40, 0, { .fields = { 88, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, -80, 8, 0, { .fields = { 104, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, -48, 112, 269, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 40, 8 } }, -56, 104, 294, { .fields = { 112, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 16 } }, -56, 88, 322, { .fields = { 24, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, -56, 80, 313, { .fields = { 0, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -56, 72, 348, { .fields = { 8, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, -64, 64, 392, { .fields = { 0, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 16 } }, -64, 48, 442, { .fields = { 24, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -64, 40, 427, { .fields = { 0, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 16 } }, -72, 24, 596, { .fields = { 24, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -72, 16, 568, { .fields = { 0, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 8 } }, -72, 8, 694, { .fields = { 8, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -80, 0, 890, { .fields = { 0, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 24 } }, -80, -24, 1164, { .fields = { 72, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_shelter_b2_laboratory_80184FE0[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 34, 0, 0, { 3, 0 } },
    { 34, 13, 0, 0, { 0, 0 } },
    { 47, 8, 0, 0, { 2, 0 } },
    { 55, 23, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_80185010[16] = {
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -56, -16, 375, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -16, -16, 375, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 56, 403, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 56, 375, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -120, 96, 375, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -80, 96, 375, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, 56, 375, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 0, 56, 438, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 48, 375, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -16, 48, 375, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -40, 96, 375, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 0, 96, 375, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, 64, 393, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 80, 64, 393, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 40, 96, 375, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, 96, 375, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_80185150[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_laboratory_80185168[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_laboratory_80185178[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_laboratory_80185188[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_laboratory_80185198[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_laboratory_801851A8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_801851B8[37] = {
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, 48, 825, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, -24, 787, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, -88, 825, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, -72, 800, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 32, 928, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 32, 920, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -64, 844, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, -88, 825, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -88, 772, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, -72, 804, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -56, 787, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, -48, 825, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, -32, 825, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 24, 825, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, -16, 825, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, -8, 864, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 0, 864, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 24, 925, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -48, 825, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -32, 825, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -16, 825, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 0, 825, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 8, 941, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 16, 929, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, 24, 787, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, -16, 0, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 24, 0, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 24, 0, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 32, 0, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 8, 864, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, 8, 890, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 24, 949, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 32, 944, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, 40, 919, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -16, 786, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -8, 857, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 0, 864, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_8018549C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 25, 0, 0, { 1, 0 } },
    { 25, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b2_laboratory_801854BC[2] = {
    { { 60, 3, 257, 235 }, 745 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteView D_shelter_b2_laboratory_801854D0[15] = {
    { { .empty = D_shelter_b2_laboratory_8018379C }, D_shelter_b2_laboratory_8018379C, NULL },
    { { .elements = D_shelter_b2_laboratory_801837AC }, D_shelter_b2_laboratory_80183BA8, NULL },
    { { .elements = D_shelter_b2_laboratory_80183BD0 }, D_shelter_b2_laboratory_80183D60, NULL },
    { { .elements = D_shelter_b2_laboratory_80183D78 }, D_shelter_b2_laboratory_80183FD0, NULL },
    { { .elements = D_shelter_b2_laboratory_80183FF8 }, D_shelter_b2_laboratory_801841C4, NULL },
    { { .empty = D_shelter_b2_laboratory_801841DC }, D_shelter_b2_laboratory_801841DC, NULL },
    { { .elements = D_shelter_b2_laboratory_801841EC }, D_shelter_b2_laboratory_801849A8, NULL },
    { { .elements = D_shelter_b2_laboratory_801849C8 }, D_shelter_b2_laboratory_80184FE0, NULL },
    { { .elements = D_shelter_b2_laboratory_80185010 }, D_shelter_b2_laboratory_80185150, NULL },
    { { .empty = D_shelter_b2_laboratory_80185168 }, D_shelter_b2_laboratory_80185168, NULL },
    { { .empty = D_shelter_b2_laboratory_80185178 }, D_shelter_b2_laboratory_80185178, NULL },
    { { .empty = D_shelter_b2_laboratory_80185188 }, D_shelter_b2_laboratory_80185188, NULL },
    { { .empty = D_shelter_b2_laboratory_80185198 }, D_shelter_b2_laboratory_80185198, NULL },
    { { .empty = D_shelter_b2_laboratory_801851A8 }, D_shelter_b2_laboratory_801851A8, NULL },
    { { .elements = D_shelter_b2_laboratory_801851B8 }, D_shelter_b2_laboratory_8018549C, D_shelter_b2_laboratory_801854BC },
};

WorldCoordPointLight D_shelter_b2_laboratory_80185584[10] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5547, -1204, 1082 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2071, 2222, 2201 }, { 0, 0 } }, 4000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6419, -1756, 3004 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 890, 1978, 1175 }, { 0, 0 } }, 1261, 1721 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2237, -2188, 1564 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1010, 2438, 694 }, { 0, 0 } }, 500, 1600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2783, -1669, 5243 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3331, 3396, 3526 }, { 0, 0 } }, 1640, 2940 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9198, -3405, -364 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1826, 2022, 2062 }, { 0, 0 } }, 2739, 7039 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2D99, -2950, -1960 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1848, 2009, 2285 }, { 0, 0 } }, 1201, 2200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 457, -3405, -852 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1569, 1642, 1699 }, { 0, 0 } }, 1979, 4319 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3290, -989, -1161 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1474, 1940, 1859 }, { 0, 0 } }, 700, 1199 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2B0A, -2989, -198 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 562, 1720, 1805 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2B0A, -3109, 2300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 562, 1280, 1485 }, { 0, 0 } }, 500, 1500 },
};

WorldCoordRoomLights D_shelter_b2_laboratory_80185944 = { 0, NULL, ARRAY_SIZE(D_shelter_b2_laboratory_80185584), D_shelter_b2_laboratory_80185584, 0, NULL };

WorldCollisionTrigger D_shelter_b2_laboratory_8018595C[14] = {
    { NULL, NULL, NULL, { 5680, -1616, 1944, 0 }, { { 28, -3232, -2247, 0 }, { -47, -3232, 2228, 0 }, { 28, 3232, -2247, 0 }, { -47, 3232, 2228, 0 } }, { 4098, 0, 68, 0 }, { 0, 0, 4096, 0 }, 3924, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5824, -1568, 2000, 0 }, { { -66, -3216, 2306, 0 }, { 23, -3216, -2349, 0 }, { -66, 3216, 2306, 0 }, { 23, 3216, -2349, 0 } }, { -4103, 0, -79, 0 }, { 0, 0, 4096, 0 }, 3965, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1760, -1601, 3552, 0 }, { { -2912, -3216, -176, 0 }, { 2912, -3216, 176, 0 }, { -2912, 3216, -176, 0 }, { 2912, 3216, 176, 0 } }, { 246, 0, -4090, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1824, -1568, 3393, 0 }, { { 2976, -3216, 208, 0 }, { -2976, -3216, -208, 0 }, { 2976, 3216, 208, 0 }, { -2976, 3216, -208, 0 } }, { -288, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6526, -1504, -2436, 0 }, { { 89, -3216, 1462, 0 }, { -89, -3216, -1462, 0 }, { 89, 3216, 1462, 0 }, { -89, 3216, -1462, 0 } }, { -4107, 0, 249, 0 }, { 0, 0, 4096, 0 }, 3528, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6432, -1600, -2386, 0 }, { { -55, -3216, -1476, 0 }, { 56, -3216, 1477, 0 }, { -55, 3216, -1476, 0 }, { 56, 3216, 1477, 0 } }, { 4095, 0, -155, 0 }, { 0, 0, 4096, 0 }, 3537, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4720, -1568, -80, 0 }, { { -1059, -3216, -27, 0 }, { 1015, -3216, -27, 0 }, { -1059, 3216, -27, 0 }, { 1015, 3216, -27, 0 } }, { 0, 0, -4107, 0 }, { 0, 0, 4096, 0 }, 3376, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4786, -1729, -209, 0 }, { { 1239, -3216, -12, 0 }, { -1257, -3216, -11, 0 }, { 1239, 3216, -12, 0 }, { -1257, 3216, -11, 0 } }, { 1, 0, 4098, 0 }, { 0, 0, 4096, 0 }, 3444, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A20, -1600, -416, 0 }, { { -1268, -3216, -168, 0 }, { 1240, -3216, 147, 0 }, { -1268, 3216, -168, 0 }, { 1240, 3216, 147, 0 } }, { 510, 0, -4068, 0 }, { 0, 0, 4096, 0 }, 3453, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A51, -1536, -544, 0 }, { { 1495, -3216, 197, 0 }, { -1521, -3216, -215, 0 }, { 1495, 3216, 197, 0 }, { -1521, 3216, -215, 0 } }, { -555, 0, 4061, 0 }, { 0, 0, 4096, 0 }, 3556, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2930, -1600, -3297, 0 }, { { 0, -3232, -832, 0 }, { 0, -3232, 832, 0 }, { 0, 3232, -832, 0 }, { 0, 3232, 832, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2981, -1472, -3312, 0 }, { { 0, -3232, 912, 0 }, { 0, -3232, -912, 0 }, { 0, 3232, 912, 0 }, { 0, 3232, -912, 0 } }, { -4104, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3357, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4431, -1536, -2401, 0 }, { { -685, -3216, 1727, 0 }, { 685, -3216, -1727, 0 }, { -685, 3216, 1727, 0 }, { 685, 3216, -1727, 0 } }, { -3821, 0, -1516, 0 }, { 0, 0, 4096, 0 }, 3709, 0, 15, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4320, -1504, -2400, 0 }, { { 685, -3216, -1727, 0 }, { -685, -3216, 1727, 0 }, { 685, 3216, -1727, 0 }, { -685, 3216, 1727, 0 } }, { 3819, 0, 1515, 0 }, { 0, 0, 4096, 0 }, 3709, 0, 5, 15, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b2_laboratory_80185D84[19] = {
    { NULL, NULL, NULL, { 0, -48, 1920, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_WARP, 33, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6384, -48, 2976, 0 }, { { 560, 0, -544, 0 }, { 560, 0, 544, 0 }, { -560, 0, -544, 0 }, { -560, 0, 544, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 778, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 35, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3489, -64, -3680, 0 }, { { -1056, 0, -224, 0 }, { 960, 0, -224, 0 }, { -1056, 0, 1024, 0 }, { 960, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 1470, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3296, -64, -816, 0 }, { { -1120, 0, -1808, 0 }, { 1120, 0, -1808, 0 }, { -1120, 0, 176, 0 }, { 1120, 0, 176, 0 } }, { 0, 4119, 0, 0 }, { 4096, 0, 0, 0 }, 2126, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 432, -64, 4112, 0 }, { { -656, 0, -1104, 0 }, { 432, 0, -1104, 0 }, { -432, 0, 1104, 0 }, { 656, 0, 1104, 0 } }, { 0, 4115, 0, 0 }, { 4096, 0, 0, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, 23, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2288, -64, 4800, 0 }, { { -2544, 0, -464, 0 }, { 2544, 0, -464, 0 }, { -2544, 0, 464, 0 }, { 2544, 0, 464, 0 } }, { 0, 4095, 0, 0 }, { 200, 0, -4091, 0 }, 2585, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8464, -64, 2336, 0 }, { { -1344, 0, -336, 0 }, { 1344, 0, -336, 0 }, { -1344, 0, 336, 0 }, { 1344, 0, 336, 0 } }, { 0, 4105, 0, 0 }, { -201, 0, -4091, 0 }, 1384, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7024, -64, 320, 0 }, { { -1104, 0, -336, 0 }, { 1104, 0, -336, 0 }, { -1104, 0, 336, 0 }, { 1104, 0, 336, 0 } }, { 0, 4110, 0, 0 }, { -201, 0, 4090, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2848, -64, 320, 0 }, { { -1104, 0, -336, 0 }, { 1104, 0, -336, 0 }, { -1104, 0, 336, 0 }, { 1104, 0, 336, 0 } }, { 0, 4110, 0, 0 }, { -402, 0, 4076, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7392, -64, -1568, 0 }, { { -1104, 0, -336, 0 }, { 1104, 0, -336, 0 }, { -1104, 0, 336, 0 }, { 1104, 0, 336, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, -4096, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 22, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x29A0, -1491, 3072, 0 }, { { -736, 0, -336, 0 }, { 736, 0, -336, 0 }, { -736, 0, 336, 0 }, { 736, 0, 336, 0 } }, { 0, 4102, 0, 0 }, { -201, 0, -4091, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2B20, -1491, 2592, 0 }, { { -416, 0, -464, 0 }, { 416, 0, -464, 0 }, { -416, 0, 464, 0 }, { 416, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { -4091, 0, 201, 0 }, 620, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2B40, -1491, 1152, 0 }, { { -416, 0, -464, 0 }, { 416, 0, -464, 0 }, { -416, 0, 464, 0 }, { 416, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { -4091, 0, 201, 0 }, 620, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2B20, -1504, -224, 0 }, { { -416, 0, -464, 0 }, { 416, 0, -464, 0 }, { -416, 0, 464, 0 }, { 416, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { -4091, 0, 201, 0 }, 620, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3120, -1491, -3136, 0 }, { { -416, 0, -1072, 0 }, { 416, 0, -1072, 0 }, { -416, 0, 1072, 0 }, { 416, 0, 1072, 0 } }, { 0, 4103, 0, 0 }, { -4091, 0, 201, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3423, -64, 4543, 0 }, { { -308, 0, 1294, 0 }, { -1474, 0, -102, 0 }, { 1475, 0, 103, 0 }, { 309, 0, -1293, 0 } }, { 0, 4099, 0, 0 }, { 4091, 0, -202, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_CAP, 27, 1, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1216, -64, 896, 0 }, { { -1104, 0, -336, 0 }, { 1104, 0, -336, 0 }, { -1104, 0, 336, 0 }, { 1104, 0, 336, 0 } }, { 0, 4110, 0, 0 }, { 798, 0, 4017, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4800, -64, 2784, 0 }, { { -1008, 0, -112, 0 }, { 1008, 0, -560, 0 }, { -1008, 0, 560, 0 }, { 1008, 0, 112, 0 } }, { 0, 4101, 0, 0 }, { -798, 0, -4017, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3248, -64, -1952, 0 }, { { -768, 0, -672, 0 }, { 768, 0, -672, 0 }, { -768, 0, 672, 0 }, { 768, 0, 672, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1019, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b2_laboratory_80186328[2] = {
    { 101, 430, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_143000_801350B0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b2_laboratory_80186340[2] = {
    { 101, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b2_laboratory_80186360[11] = {
    { NULL, NULL },
    { D_shelter_b2_laboratory_80186340, D_shelter_b2_laboratory_80186328 },
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

WorldCoordRoomAmbientEntry D_shelter_b2_laboratory_801863B8[16] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b2_laboratory_801863B8) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 952, 944, 999, 953 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_shelter_b2_laboratory_80186438 = {
    0x10000009,
    0x1000000B,
    0x10000009,
};

WorldCollisionFootstepSounds D_shelter_b2_laboratory_80186444 = {
    0x10000061,
    0x10000063,
    0x10000061,
};

WorldCollisionSurfaceProperties D_shelter_b2_laboratory_80186450[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_laboratory_80186458[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_laboratory_80186438 },
};

WorldCollisionSurfaceProperties D_shelter_b2_laboratory_80186460[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_laboratory_80186444 },
};

WorldCollisionSurfaceProperties* D_shelter_b2_laboratory_80186468[8] = {
    D_shelter_b2_laboratory_80186450,
    D_shelter_b2_laboratory_80186458,
    D_shelter_b2_laboratory_80186460,
    D_shelter_b2_laboratory_80186450,
    D_shelter_b2_laboratory_80186450,
    D_shelter_b2_laboratory_80186450,
    D_shelter_b2_laboratory_80186450,
    D_shelter_b2_laboratory_80186450,
};

AreaApplyRec D_shelter_b2_laboratory_80186488[5] = {
    { 4, 8, 3, 1 },
    { 4, 12, 3, 1 },
    { 4, 27, 3, 1 },
    { 3, 21, 12, 0 },
    { 255, 0, 0, 0 },
};

AreaApplyRec D_shelter_b2_laboratory_8018649C[2] = {
    { 4, 47, 2, 0 },
    { 255, 0, 0, 0 },
};

RoomCutsceneSoundTaskStorage gRoomCutsceneSoundTask = { NULL, { 0 } };

RoomEventMsg gRoomEventMsg = { 0, 0, 0, 0, 0, 0 };

u8 gRoomEventActive = 0;

s32 D_shelter_b2_laboratory_801864B8 = 0;

RoomCutsceneRecStorage D_shelter_b2_laboratory_801864BC = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 } };

GfxCoord D_shelter_b2_laboratory_801864DC = { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL };

RoomEventReq gRoomEventReq = { 0, 0, 0, 0, 0, 0 };

u16 D_shelter_b2_laboratory_80186540;

#include "../../shared/telephone.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

void shelterB2LaboratoryTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// Sets the four sound scripts accompanying the laboratory console scene.
///
/// Borrows a writable scene record for this call; changes only its sound IDs.
/// The laboratory sound bank must remain loaded through the runner's playback.
static inline void _shelterB2LaboratorySetConsoleSceneSounds(RoomCutsceneRec* scene)
{
    enum {
        SHELTER_B2_LABORATORY_SCENE_START_SOUND = 5,
        SHELTER_B2_LABORATORY_SCENE_END_SOUND   = 8,
        SHELTER_B2_LABORATORY_SCENE_SOUND       = 6,
        SHELTER_B2_LABORATORY_AFTER_SCENE_SOUND = 7,
    };

    scene->startSound      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_SCENE_START_SOUND);
    scene->endSound        = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_SCENE_END_SOUND);
    scene->sceneSound      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_SCENE_SOUND);
    scene->afterSceneSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_AFTER_SCENE_SOUND);
}

/// Starts the laboratory console's scene and commits its completed progress.
///
/// Handles `ROOM_MESSAGE_COMMAND` with first payload 4; other commands do
/// nothing, and the receiver and second payload are unused. Stops ambience.
/// Progress 2 advances to 3, unlocks the pod-tunnel door and updates the map,
/// route flags and objective before requesting the skipped scene and its
/// follow-up task. Other progress values request the full scene instead.
/// The cutscene runner borrows the singleton record; do not replace it or unload
/// its room, CAP and sound resources while playback runs. Spawn failure leaves
/// committed flags intact. Returns zero regardless of admission.
static s32 _shelterB2LaboratoryCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum {
        SHELTER_B2_LABORATORY_COMMAND_CONSOLE_SCENE     = 4,
        SHELTER_B2_LABORATORY_CONSOLE_SCENE_COMPLETE    = 2,
        SHELTER_B2_LABORATORY_CONSOLE_PROGRESS_SETTLED  = 3,
        SHELTER_B2_LABORATORY_SCENE_VIEW                = 13,
        SHELTER_B2_LABORATORY_SKIP_CONSOLE_SCENE        = 1,
        SHELTER_B2_LABORATORY_PLAY_CONSOLE_SCENE        = 0,
        SHELTER_B2_LABORATORY_CAP_COMPLETED_SCENE_SLOT  = 4,
        SHELTER_B2_LABORATORY_CAP_FULL_SCENE_SLOT       = 1,
        SHELTER_B2_LABORATORY_CAP_COMPLETED_SCENE_FILE  = 3,
        SHELTER_B2_LABORATORY_CAP_FULL_SCENE_FILE       = 2,
        SHELTER_B2_LABORATORY_CAP_COMPLETED_TEXTURE_X   = 384,
        SHELTER_B2_LABORATORY_CAP_COMPLETED_TEXTURE_Y   = 256,
        SHELTER_B2_LABORATORY_CAP_DEFAULT_TEXTURE_PAGE  = 0,
        SHELTER_B2_LABORATORY_CAP_AFTER_COMPLETED_SCENE = 1,
        SHELTER_B2_LABORATORY_CAP_AFTER_FULL_SCENE      = 8,
        SHELTER_B2_LABORATORY_OBJECTIVE_ROUTE_SET       = 0x28,
        SHELTER_B2_LABORATORY_OBJECTIVE_ROUTE_CLEAR     = 0x29,
        SHELTER_B2_LABORATORY_TASK_FINISH_CONSOLE_SCENE = 2,
        SHELTER_B2_LABORATORY_MAP_MARK_CLEAR            = 0,
        SHELTER_B2_LABORATORY_PROGRESS_FLAG_SET         = 1,
    };

    if (commandId == SHELTER_B2_LABORATORY_COMMAND_CONSOLE_SCENE) {
        D_shelter_b2_laboratory_801864B8 = 0;
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS) == SHELTER_B2_LABORATORY_CONSOLE_SCENE_COMPLETE) {
            // Commit progression before either deferred task can run.
            _shelterB2LaboratorySetFastGlowPulse(SHELTER_B2_LABORATORY_GLOW_PULSE_SLOW);
            gameFlagSetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS, SHELTER_B2_LABORATORY_CONSOLE_PROGRESS_SETTLED);
            gameFlagSetNibble(GAME_FLAG_B1_POD_TUNNEL_R47_DOOR_UNLOCKED, SHELTER_B2_LABORATORY_PROGRESS_FLAG_SET);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_B2_LABORATORY, SHELTER_B2_LABORATORY_MAP_MARK_CLEAR);
            gameFlagSetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_017, SHELTER_B2_LABORATORY_PROGRESS_FLAG_SET);
            gameFlagSetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_018, SHELTER_B2_LABORATORY_PROGRESS_FLAG_SET);
            if (gameFlagGetNibble(GAME_FLAG_083) != 0) {
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, SHELTER_B2_LABORATORY_OBJECTIVE_ROUTE_SET);
            } else {
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, SHELTER_B2_LABORATORY_OBJECTIVE_ROUTE_CLEAR);
            }
            D_shelter_b2_laboratory_801864BC.rec.view      = SHELTER_B2_LABORATORY_SCENE_VIEW;
            D_shelter_b2_laboratory_801864BC.rec.capSlot   = SHELTER_B2_LABORATORY_CAP_COMPLETED_SCENE_SLOT;
            D_shelter_b2_laboratory_801864BC.rec.capFile   = SHELTER_B2_LABORATORY_CAP_COMPLETED_SCENE_FILE;
            D_shelter_b2_laboratory_801864BC.rec.skipScene = SHELTER_B2_LABORATORY_SKIP_CONSOLE_SCENE;
            D_shelter_b2_laboratory_801864BC.rec.capTPageX = SHELTER_B2_LABORATORY_CAP_COMPLETED_TEXTURE_X;
            D_shelter_b2_laboratory_801864BC.rec.capTPageY = SHELTER_B2_LABORATORY_CAP_COMPLETED_TEXTURE_Y;
            _shelterB2LaboratorySetConsoleSceneSounds(&D_shelter_b2_laboratory_801864BC.rec);
            taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, SHELTER_B2_LABORATORY_CAP_AFTER_COMPLETED_SCENE, &D_shelter_b2_laboratory_801864BC.rec);
            taskSpawnFromTable(D_shelter_b2_laboratory_80182A6C, SHELTER_B2_LABORATORY_TASK_FINISH_CONSOLE_SCENE, 0, 0);
        } else {
            // A zero texture X selects the default page; the runner ignores retained Y.
            D_shelter_b2_laboratory_801864BC.rec.view      = SHELTER_B2_LABORATORY_SCENE_VIEW;
            D_shelter_b2_laboratory_801864BC.rec.capSlot   = SHELTER_B2_LABORATORY_CAP_FULL_SCENE_SLOT;
            D_shelter_b2_laboratory_801864BC.rec.capFile   = SHELTER_B2_LABORATORY_CAP_FULL_SCENE_FILE;
            D_shelter_b2_laboratory_801864BC.rec.skipScene = SHELTER_B2_LABORATORY_PLAY_CONSOLE_SCENE;
            D_shelter_b2_laboratory_801864BC.rec.capTPageX = SHELTER_B2_LABORATORY_CAP_DEFAULT_TEXTURE_PAGE;
            _shelterB2LaboratorySetConsoleSceneSounds(&D_shelter_b2_laboratory_801864BC.rec);
            taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, SHELTER_B2_LABORATORY_CAP_AFTER_FULL_SCENE, &D_shelter_b2_laboratory_801864BC.rec);
        }
    }
    return 0;
}

/// States of the room's message task, run by
/// `shelterB2LaboratoryTask`: install the message table, idle, die.
static const TaskFuncTable3 D_shelter_b2_laboratory_8017D6BC = {
    {
        _shelterB2LaboratoryInitRoomTask,
        _shelterB2LaboratoryIdleMessageTask,
        taskKill,
    },
};

/// Refreshes the laboratory ambience point's local-to-view transform.
///
/// Places the retained origin at room coordinates (3100, -1500, -3200), attaches
/// it to the current view and invalidates its cache before composing. The
/// caller reads that cache for spatial pan and attenuation; the view and its
/// ancestors must be live. Leaves the node's local rotation and Euler state
/// intact and borrows the view parent until the next refresh.
static inline void _shelterB2LaboratoryComposeAmbienceOrigin(void)
{
    D_shelter_b2_laboratory_801864DC.coord.t[0]   = 3100;
    D_shelter_b2_laboratory_801864DC.coord.t[1]   = -1500;
    D_shelter_b2_laboratory_801864DC.coord.t[2]   = -3200;
    D_shelter_b2_laboratory_801864DC.parent       = &gGfxViewCoord;
    D_shelter_b2_laboratory_801864DC.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&D_shelter_b2_laboratory_801864DC);
}

/// Runs the laboratory's looping ambience and retunes it after view changes.
///
/// Start at state 0 with the room bank loaded and the room's ambience-enable
/// latch set. Begins at the spatial pan/depth of room point (3100, -1500, -3200).
/// When the saved and active views diverge, waits three intervening callback
/// ticks, then replaces attenuation from the active view's percentage table.
/// The active view must be 0..15. Stop is checked only in the watching state;
/// clearing the latch queues a release-preserving stop and destroys the task.
/// The body, work and spawn arguments are unused. Bank resources must outlive
/// queued events and the running sound; queue-admission failures are ignored.
static void _shelterB2LaboratoryAmbienceTask(Task* task)
{
    enum {
        SHELTER_B2_LABORATORY_AMBIENCE_START           = 0,
        SHELTER_B2_LABORATORY_AMBIENCE_WATCH_VIEW      = 1,
        SHELTER_B2_LABORATORY_AMBIENCE_DELAY_FIRST     = 2,
        SHELTER_B2_LABORATORY_AMBIENCE_DELAY_SECOND    = 3,
        SHELTER_B2_LABORATORY_AMBIENCE_DELAY_THIRD     = 4,
        SHELTER_B2_LABORATORY_AMBIENCE_RETUNE          = 5,
        SHELTER_B2_LABORATORY_AMBIENCE_SOUND_ENTRY     = 0x0E,
        SHELTER_B2_LABORATORY_AMBIENCE_MAX_ATTENUATION = 127,
        SHELTER_B2_LABORATORY_AMBIENCE_PERCENT_SCALE   = 100,
    };
    s8  panOffset;
    s8  initialAttenuation;
    s32 viewAttenuation;

    // Recompose every tick so both phases use the current camera's spatial pan.
    _shelterB2LaboratoryComposeAmbienceOrigin();
    panOffset          = worldCoordGetOriginAudioPan(&D_shelter_b2_laboratory_801864DC);
    initialAttenuation = worldCoordGetOriginAudioDepth(&D_shelter_b2_laboratory_801864DC);
    switch (task->state) {
        case SHELTER_B2_LABORATORY_AMBIENCE_START:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_AMBIENCE_SOUND_ENTRY), panOffset, initialAttenuation);
            task->state++;
            break;
        case SHELTER_B2_LABORATORY_AMBIENCE_WATCH_VIEW:
            if (D_shelter_b2_laboratory_801864B8 == 0) {
                sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_AMBIENCE_SOUND_ENTRY), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                taskKill(task);
                return;
            }
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view != gGameSession->location.loc.view) {
                task->state++;
            }
            break;
        case SHELTER_B2_LABORATORY_AMBIENCE_DELAY_FIRST:
        case SHELTER_B2_LABORATORY_AMBIENCE_DELAY_SECOND:
        case SHELTER_B2_LABORATORY_AMBIENCE_DELAY_THIRD:
            task->state++;
            break;
        case SHELTER_B2_LABORATORY_AMBIENCE_RETUNE:
            // The table is a gain percentage; the sound API takes attenuation.
            viewAttenuation = SHELTER_B2_LABORATORY_AMBIENCE_MAX_ATTENUATION - D_shelter_b2_laboratory_80182A90[gGameSession->location.loc.view] * SHELTER_B2_LABORATORY_AMBIENCE_MAX_ATTENUATION / SHELTER_B2_LABORATORY_AMBIENCE_PERCENT_SCALE;
            if (viewAttenuation >= SHELTER_B2_LABORATORY_AMBIENCE_MAX_ATTENUATION + 1) {
                viewAttenuation = SHELTER_B2_LABORATORY_AMBIENCE_MAX_ATTENUATION;
            }
            sndEvtRequestScriptMix(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_AMBIENCE_SOUND_ENTRY), panOffset, (s8)viewAttenuation);
            task->state = SHELTER_B2_LABORATORY_AMBIENCE_WATCH_VIEW;
            break;
    }
}

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Refuses key-item use in the laboratory without consuming the item.
///
/// Handles message 0x13F1. All arguments are ignored; zero selects the
/// item menu's refusal notice and leaves the room and inventory unchanged.
static s32 _shelterB2LaboratoryRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg)
{
    enum { SHELTER_B2_LABORATORY_KEY_ITEM_REFUSED = 0 };

    return SHELTER_B2_LABORATORY_KEY_ITEM_REFUSED;
}

/// Resolves a laboratory departure and gates its corridor-door event.
///
/// `ROOM_EVENT_MESSAGE_RESOLVE` borrows a complete request and writable
/// eight-byte reply; they may alias and neither pointer is retained. Copies
/// the record and resolves the Shelter room variant. Progress 2 returns 2,
/// playing CAP command 5 only on execution, for any destination. Otherwise
/// the corridor door returns the gate's 0 refused, 1 bypassed or 2 eligible
/// result, and other destinations return 1. Queries suppress playback and
/// flag writes; door queries still clear the event-start latch. Keep the room,
/// map overlay and event resources loaded through any deferred transition.
static s32 _shelterB2LaboratoryResolveRoomTransition(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_B2_LABORATORY_TRANSITION_ACCEPTED    = 1,
        SHELTER_B2_LABORATORY_TRANSITION_SCENE       = 2,
        SHELTER_B2_LABORATORY_CONSOLE_SCENE_COMPLETE = 2,
        SHELTER_B2_LABORATORY_CAP_PROGRESS_BLOCK     = 5,
        SHELTER_B2_LABORATORY_CAP_CORRIDOR_DOOR      = 1,
        SHELTER_B2_LABORATORY_NO_COLLECTION_REQUIRED = 0,
        SHELTER_B2_LABORATORY_DOOR_FIRST_SOUND       = 20,
        SHELTER_B2_LABORATORY_DOOR_SECOND_SOUND      = 3,
    };

    RoomEventReq doorEvent;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (gameFlagGetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS) == SHELTER_B2_LABORATORY_CONSOLE_SCENE_COMPLETE) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            capRunCommandWithTransition(SHELTER_B2_LABORATORY_CAP_PROGRESS_BLOCK);
        }
        return SHELTER_B2_LABORATORY_TRANSITION_SCENE;
    }
    if (request->areaId != GAME_AREA_SHELTER_B2_MAIN_CORRIDOR) {
        return SHELTER_B2_LABORATORY_TRANSITION_ACCEPTED;
    }
    doorEvent.capCmd        = SHELTER_B2_LABORATORY_CAP_CORRIDOR_DOOR;
    doorEvent.missingCapCmd = SHELTER_B2_LABORATORY_CAP_CORRIDOR_DOOR;
    doorEvent.firstSnd      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_DOOR_FIRST_SOUND);
    doorEvent.secondSnd     = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_DOOR_SECOND_SOUND);
    doorEvent.flagId        = GAME_FLAG_B2_LABORATORY_DOOR_UNLOCKED;
    doorEvent.collectedBit  = SHELTER_B2_LABORATORY_NO_COLLECTION_REQUIRED;
    return _roomEventGate(&doorEvent, reply);
}

/// Handles the directed action on the laboratory console.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a readable request through this
/// dispatch; only action 1 is handled. Its first use plays the introductory
/// CAP command and commits the first-use flag. Later uses request the keypad
/// task while progress is below 2, otherwise replay CAP command 6. The
/// receiver and second payload are unused. Keep the room and actor_143000
/// resources loaded while their tasks run. Always returns zero.
static s32 _shelterB2LaboratoryConsoleActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum {
        SHELTER_B2_LABORATORY_ACTION_CONSOLE             = 1,
        SHELTER_B2_LABORATORY_CONSOLE_SCENE_COMPLETE     = 2,
        SHELTER_B2_LABORATORY_CAP_CONSOLE_INTRO          = 0x1E,
        SHELTER_B2_LABORATORY_CAP_CONSOLE_REVISIT        = 6,
        SHELTER_B2_LABORATORY_CONSOLE_FIRST_USE_RECORDED = 1,
        SHELTER_B2_LABORATORY_TASK_CONSOLE_KEYPAD        = 0,
    };

    if (request->actionId == SHELTER_B2_LABORATORY_ACTION_CONSOLE) {
        if (gameFlagGetNibble(GAME_FLAG_LABORATORY_CONSOLE_FIRST_USE) != 0) {
            if (gameFlagGetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS) < SHELTER_B2_LABORATORY_CONSOLE_SCENE_COMPLETE) {
                taskSpawnFromTable(D_shelter_b2_laboratory_80182A6C, SHELTER_B2_LABORATORY_TASK_CONSOLE_KEYPAD, 0, 0);
            } else {
                capRunCommandWithTransition(SHELTER_B2_LABORATORY_CAP_CONSOLE_REVISIT);
            }
        } else {
            capRunCommandWithTransition(SHELTER_B2_LABORATORY_CAP_CONSOLE_INTRO);
            gameFlagSetNibble(GAME_FLAG_LABORATORY_CONSOLE_FIRST_USE, SHELTER_B2_LABORATORY_CONSOLE_FIRST_USE_RECORDED);
        }
    }
    return 0;
}

/// Maps laboratory sound cue 99 to room-bank sound script 23.
///
/// Receives `ROOM_MESSAGE_SOUND` with an integer cue in the first payload;
/// other arguments and cues are ignored. Always returns zero, including when
/// the sound queue rejects the start. Keep the room sound bank loaded through
/// playback; uses its base pan and depth rather than a spatial origin.
static s32 _shelterB2LaboratoryCueSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueId, s32 unusedSecondArg)
{
    enum {
        SHELTER_B2_LABORATORY_SOUND_CUE_99       = 99,
        SHELTER_B2_LABORATORY_CUE_99_SOUND_ENTRY = 0x17
    };
    if (cueId == SHELTER_B2_LABORATORY_SOUND_CUE_99) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, SHELTER_B2_LABORATORY_CUE_99_SOUND_ENTRY), 0, 0);
    }
    return 0;
}

/// Runs the console keypad and commits its accepted-code result.
///
/// Start in state 0 with actor_143000 loaded. Retains the spawned keypad handle
/// in the room's singleton slot, then polls its requested exit on later ticks.
/// A nonzero result sets laboratory progress to 1; zero resumes player control.
/// Clears the handle and kills this task after collecting the result. Body and
/// payloads are unused. The spawn is unchecked: the keypad must have spawned,
/// and this singleton task must not overlap another keypad waiter.
static void _shelterB2LaboratoryConsoleKeypadTask(Task* task)
{
    enum {
        SHELTER_B2_LABORATORY_KEYPAD_START          = 0,
        SHELTER_B2_LABORATORY_KEYPAD_WAIT           = 1,
        SHELTER_B2_LABORATORY_CONSOLE_CODE_ACCEPTED = 1,
    };

    s32 codeAccepted;

    switch (task->state) {
        case SHELTER_B2_LABORATORY_KEYPAD_START:
            D_shelter_b2_laboratory_80182A68 = taskSpawnFromTable(&D_actor_143000_80134564, 0, 0, 0);
            task->state                     += 1;
            return;
        case SHELTER_B2_LABORATORY_KEYPAD_WAIT:
            // Polling dispatches the keypad exit; stop using its handle after success.
            if (taskPollKill(D_shelter_b2_laboratory_80182A68, &codeAccepted) != 0) {
                D_shelter_b2_laboratory_80182A68 = NULL;
                if (codeAccepted != 0) {
                    gameFlagSetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS, SHELTER_B2_LABORATORY_CONSOLE_CODE_ACCEPTED);
                } else {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                }
                taskKill(task);
            }
            return;
    }
}

/// Selects post-console music and dialogue after the scripted scene releases control.
///
/// Start in state 0. Waits for active view 13, then writes sceneEvent 14 and
/// requests ordinary stage music with no fade. On a later tick with eventState
/// zero, chooses CAP command 0x24 or 0x23 from the route flag and kills itself.
/// Body and payloads are unused. Keep the room's CAP data and the stage's map
/// music table loaded; music and CAP request admission are not checked.
static void _shelterB2LaboratoryFinishConsoleSceneTask(Task* task)
{
    enum {
        SHELTER_B2_LABORATORY_FINISH_WAIT_VIEW              = 0,
        SHELTER_B2_LABORATORY_FINISH_REQUEST_MUSIC          = 1,
        SHELTER_B2_LABORATORY_FINISH_WAIT_SCENE             = 2,
        SHELTER_B2_LABORATORY_FINISH_SCENE_VIEW             = 13,
        SHELTER_B2_LABORATORY_FINISH_MUSIC_SCENE            = 14,
        SHELTER_B2_LABORATORY_FINISH_MUSIC_REQUEST_ORDINARY = 0,
        SHELTER_B2_LABORATORY_CAP_FINISH_ROUTE_SET          = 0x24,
        SHELTER_B2_LABORATORY_CAP_FINISH_ROUTE_CLEAR        = 0x23,
    };

    switch (task->state) {
        case SHELTER_B2_LABORATORY_FINISH_WAIT_VIEW:
            if (gGameSession->location.loc.view == SHELTER_B2_LABORATORY_FINISH_SCENE_VIEW) {
                task->state = SHELTER_B2_LABORATORY_FINISH_REQUEST_MUSIC;
            }
            return;
        case SHELTER_B2_LABORATORY_FINISH_REQUEST_MUSIC:
            // Select music after the scripted view arrives, then await scene release.
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = SHELTER_B2_LABORATORY_FINISH_MUSIC_SCENE;
            gStageMusicParams.fadeOutTicks                      = 0;
            gStageMusicParams.field_2                           = 0;
            taskSpawnFromTable(&Stage_MusicTaskDesc, 0, SHELTER_B2_LABORATORY_FINISH_MUSIC_REQUEST_ORDINARY, 0);
            task->state++;
            return;
        case SHELTER_B2_LABORATORY_FINISH_WAIT_SCENE:
            if (gGameSession->eventState == 0) {
                capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_083) != 0 ? SHELTER_B2_LABORATORY_CAP_FINISH_ROUTE_SET : SHELTER_B2_LABORATORY_CAP_FINISH_ROUTE_CLEAR);
                taskKill(task);
            }
            return;
    }
}

/// Registers the laboratory's room-message receiver and advances to idle.
///
/// Runs in state 0, borrowing this overlay's message table and publishing the
/// live task in `GAME_TASK_SLOT_ROOM`. Keep both loaded while messages can
/// arrive; registration neither retains the task nor clears the slot at exit.
static void _shelterB2LaboratoryInitRoomTask(Task* task)
{
    task->msgTable = D_shelter_b2_laboratory_80182A38;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the laboratory's room-message task idle between messages.
///
/// Ignores the receiver and leaves its state unchanged; message dispatch is
/// independent of this per-frame callback.
static void _shelterB2LaboratoryIdleMessageTask(Task* task)
{
    // Preserve the callback's stack reservation.
    char unusedStack[0x10];
}

void shelterB2LaboratoryTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_shelter_b2_laboratory_8017D6BC;
    stateHandlers.funcs[task->state](task);
}

void shelterB2LaboratoryStartAmbience(void)
{
    enum {
        SHELTER_B2_LABORATORY_AMBIENCE_ENABLED = 1,
        SHELTER_B2_LABORATORY_TASK_AMBIENCE    = 1,
    };

    if (D_shelter_b2_laboratory_801864B8 == 0) {
        D_shelter_b2_laboratory_801864B8 = SHELTER_B2_LABORATORY_AMBIENCE_ENABLED;
        _shelterB2LaboratorySetFastGlowPulse(SHELTER_B2_LABORATORY_GLOW_PULSE_FAST);
        taskSpawnFromTable(D_shelter_b2_laboratory_80182A6C, SHELTER_B2_LABORATORY_TASK_AMBIENCE, 0, 0);
    }
}

void shelterB2LaboratoryGlowTask(Task* task)
{
    enum {
        SHELTER_B2_LABORATORY_GLOW_INITIALIZE                = 0,
        SHELTER_B2_LABORATORY_GLOW_DRAW                      = 1,
        SHELTER_B2_LABORATORY_GLOW_SLOW_PULSE_RATE           = 0x60,
        SHELTER_B2_LABORATORY_GLOW_FAST_PULSE_RATE           = 0x180,
        SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE        = 0x80,
        SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE      = 0x180,
        SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE = 0x200,
        SHELTER_B2_LABORATORY_GLOW_DIM_GREY                  = 0x222,
        SHELTER_B2_LABORATORY_GLOW_GREY                      = 0x333,
        SHELTER_B2_LABORATORY_GLOW_BRIGHT_GREY               = 0x444,
        SHELTER_B2_LABORATORY_GLOW_GREEN                     = 0x241,
        SHELTER_B2_LABORATORY_GLOW_BLUE                      = 0x124,
    };

    // Start with slow cyan pulsing; room events select the faster rate.
    if (task->state == SHELTER_B2_LABORATORY_GLOW_INITIALIZE) {
        D_shelter_b2_laboratory_80186540 = SHELTER_B2_LABORATORY_GLOW_PULSE_SLOW;
        task->state                      = SHELTER_B2_LABORATORY_GLOW_DRAW;
    }

    // Capsule colours are packed RGB nibbles; points 0..43 form pairs.
    // Point 44 supplies the cyan glow, drawn as a disc in view 13.
    switch (viewGetMappedIndex() & 0xFF) {
        case 2:
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[0], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[4], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[6], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[8], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[10], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[26], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREEN);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[30], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[32], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[34], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            break;
        case 3:
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[0], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BRIGHT_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[2], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BRIGHT_GREY);
            break;
        case 4:
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[24], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BRIGHT_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[28], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREEN);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[36], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[38], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[40], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            break;
        case 5:
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[0], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[4], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[6], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[8], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[10], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[12], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[14], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[26], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREEN);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[30], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[32], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[34], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[42], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                _shelterB2LaboratoryDrawGlowDiamond(&D_shelter_b2_laboratory_80182AA0[44], SHELTER_B2_LABORATORY_GLOW_FAST_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            } else {
                _shelterB2LaboratoryDrawGlowDiamond(&D_shelter_b2_laboratory_80182AA0[44], SHELTER_B2_LABORATORY_GLOW_SLOW_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            }
            break;
        case 6:
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[16], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[18], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[20], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[22], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[24], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[36], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[38], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[40], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            break;
        case 7:
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[24], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[36], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[38], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[40], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            break;
        case 8:
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[16], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[18], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[36], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[38], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[40], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            break;
        case 9:
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[42], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            break;
        case 10:
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[2], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[4], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[6], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[28], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREEN);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[42], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                _shelterB2LaboratoryDrawGlowDiamond(&D_shelter_b2_laboratory_80182AA0[44], SHELTER_B2_LABORATORY_GLOW_FAST_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            } else {
                _shelterB2LaboratoryDrawGlowDiamond(&D_shelter_b2_laboratory_80182AA0[44], SHELTER_B2_LABORATORY_GLOW_SLOW_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            }
            break;
        case 12:
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[12], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[14], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[30], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[32], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[34], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[42], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                _shelterB2LaboratoryDrawGlowDiamond(&D_shelter_b2_laboratory_80182AA0[44], SHELTER_B2_LABORATORY_GLOW_FAST_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            } else {
                _shelterB2LaboratoryDrawGlowDiamond(&D_shelter_b2_laboratory_80182AA0[44], SHELTER_B2_LABORATORY_GLOW_SLOW_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            }
            break;
        case 13:
            if (D_shelter_b2_laboratory_80186540 != 0) {
                _glowDrawPulsingDisc(&D_shelter_b2_laboratory_80182AA0[44], SHELTER_B2_LABORATORY_GLOW_FAST_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            } else {
                _glowDrawPulsingDisc(&D_shelter_b2_laboratory_80182AA0[44], SHELTER_B2_LABORATORY_GLOW_SLOW_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            }
            break;
        case 15:
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[26], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_GREEN);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[30], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[32], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_DIM_GREY);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[34], SHELTER_B2_LABORATORY_GLOW_WIDE_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            _glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[42], SHELTER_B2_LABORATORY_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B2_LABORATORY_GLOW_BLUE);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                _shelterB2LaboratoryDrawGlowDiamond(&D_shelter_b2_laboratory_80182AA0[44], SHELTER_B2_LABORATORY_GLOW_FAST_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            } else {
                _shelterB2LaboratoryDrawGlowDiamond(&D_shelter_b2_laboratory_80182AA0[44], SHELTER_B2_LABORATORY_GLOW_SLOW_PULSE_RATE, SHELTER_B2_LABORATORY_GLOW_POINT_RADIUS_SCALE);
            }
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

/// Initializes one Gouraud half of the laboratory's cyan glow diamond.
///
/// Requires a word-aligned, writable `POLY_G4` supplied by the caller.
/// Sets the opaque untextured command and packet length. Vertex 2 is the
/// diamond centre, with zero red and the low byte of `cyanIntensity` in
/// green and blue; rim vertices 0, 1 and 3 are black. The drawer supplies
/// screen coordinates, DMA linkage and additive semitransparency.
/// Packet storage remains caller-owned.
static inline void _shelterB2LaboratoryInitGlowDiamondHalf(POLY_G4* diamondHalf, s32 cyanIntensity)
{
    setPolyG4(diamondHalf);
    setRGB0(diamondHalf, 0, 0, 0);
    setRGB1(diamondHalf, 0, 0, 0);
    setRGB2(diamondHalf, 0, cyanIntensity, cyanIntensity);
    setRGB3(diamondHalf, 0, 0, 0);
}

/// Initializes one Gouraud diagonal through the laboratory's cyan glow.
///
/// Requires a word-aligned, writable `LINE_G3` supplied by the caller.
/// Sets the opaque command, packet length and polyline terminator, and
/// clears the high byte of the final vertex colour word. Vertex 1 is the
/// centre, with zero red and the low byte of `cyanIntensity` in green and
/// blue; end vertices 0 and 2 are black. The drawer supplies screen
/// coordinates, DMA linkage and additive semitransparency.
/// Packet storage remains caller-owned.
static inline void _shelterB2LaboratoryInitGlowDiagonal(LINE_G3* diagonal, s32 cyanIntensity)
{
    setLineG3(diagonal);
    setRGB0(diagonal, 0, 0, 0);
    setRGB1(diagonal, 0, cyanIntensity, cyanIntensity);
    setRGB2(diagonal, 0, 0, 0);
}

/// Draws a pulsing cyan diamond and two diagonals around a world point.
///
/// Borrows `worldPoint` during the call. The signed low halfword of `pulseRate`
/// is in 4096 angle units per animation frame; green and blue intensity is
/// `rsin(animFrame * pulseRate) / 34 + 120`. The signed low halfword of
/// `radiusScale` gives a pixel half-extent of `radiusScale * 32 / depth`,
/// where depth is camera Z / 4 and must be nonzero. Negative GTE flags reject
/// the point. The second diagonal extends twice as far as the diamond.
///
/// Requires composed view matrices, 24 free scratch-stack bytes, the frame's
/// 1024-entry depth table and space for four additive packets plus blend
/// commands. Releases the scratch block before returning; queued packets
/// borrow the frame's primitive arena until GPU completion.
static void _shelterB2LaboratoryDrawGlowDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale)
{
    _ShelterB2LaboratoryGlowDiamondScratch* block;
    POLY_G4*                                prim;
    LINE_G3*                                line;
    s32                                     pulseSine;
    s32                                     intensity;
    s32                                     screenRadius;
    s32                                     partIndex;
    s32                                     xRadiusMultiple;
    s32                                     yRadiusMultiple;
    s32                                     verticalSide;
    u16                                     screenX;
    u16                                     screenY;

    block = SCRATCH_STACK_RESERVE_BLOCK(_ShelterB2LaboratoryGlowDiamondScratch);

    // Project the centre before deriving screen radius and sorting depth.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulseSine     = rsin(gDisplayState.animFrame * (s16)pulseRate);
        screenRadius  = ((s16)radiusScale * GLOW_DIAMOND_RADIUS_SCALE) / block->otz;
        partIndex     = 0;
        intensity     = pulseSine / GLOW_PULSE_DIVISOR + GLOW_PULSE_BASE_INTENSITY;
        block->radius = screenRadius;
        // Two Gouraud halves fill the diamond.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _shelterB2LaboratoryInitGlowDiamondHalf(prim, intensity);
            prim->x0     = block->sx - (u16)block->radius;
            screenX      = block->sx;
            prim->x2     = screenX;
            prim->x1     = screenX;
            prim->x3     = block->sx + (u16)block->radius;
            screenY      = block->sy;
            prim->y3     = screenY;
            prim->y2     = screenY;
            prim->y0     = screenY;
            verticalSide = partIndex * 2;
            prim->y1     = (block->sy - (u16)block->radius) + (block->radius * verticalSide);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
            partIndex++;
        } while (partIndex < 2);

        // Overlay the centre-lit diagonals at the same sorting depth.
        partIndex = 0;
        do {
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            _shelterB2LaboratoryInitGlowDiagonal(line, intensity);
            xRadiusMultiple = partIndex * 3 - 1;
            yRadiusMultiple = partIndex + 1;
            line->x0        = block->sx + (block->radius * xRadiusMultiple);
            line->y0        = block->sy - (block->radius * yRadiusMultiple);
            line->x1        = block->sx;
            line->y1        = block->sy;
            line->x2        = block->sx - (block->radius * xRadiusMultiple);
            line->y2        = block->sy + (block->radius * yRadiusMultiple);
            addPrim((&gGpuCurrentOt[((u32)block->otz << gDisplayState.otDepthShift) >> 4 & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK >> 2)]),
                    line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, block->otz);
            partIndex = yRadiusMultiple;
        } while (partIndex < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_ShelterB2LaboratoryGlowDiamondScratch);
}

#include "../../shared/glow_draw_pulsing_disc.inc.c"

/// Selects slow (zero) or fast (nonzero) pulsing for the laboratory's cyan glow.
///
/// Stores all 16 argument bits without normalization. The glow task resets the
/// selection when it starts; callers toggle it as room events start and finish.
static void _shelterB2LaboratorySetFastGlowPulse(s16 enabled)
{
    D_shelter_b2_laboratory_80186540 = enabled;
}
