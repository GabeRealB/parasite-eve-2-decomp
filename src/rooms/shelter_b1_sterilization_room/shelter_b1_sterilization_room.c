#include "rooms/shelter_b1_sterilization_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "shelter_b1_sterilization_room_private.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_shelter.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_cutscene.h"
#include "../../shared/backdrop_crossfade.h"

/// The "%" suffix appended to a formatted percentage.
static u8 Telephone_Data_80181A78[];

/// UI descriptor of the help-line box the "Play Data" panels open beside their
/// lists.
static UiObjectDesc Telephone_Data_80181C90;

/// Task descriptor table used by the cutscene runner
/// `roomCutsceneTask`, which spawns entry 1 and
/// waits on it while the cutscene plays. The room's event handler spawns
/// entry 0 with a `RoomCutsceneRec` as its argument.
extern TaskDesc gRoomCutsceneTaskDescs[];

/// Where one of the room's internal doors leads, indexed by the door's room
/// action minus 3.
///
/// These doors stay inside the room: passing one fades the screen out,
/// switches the view and places the player on the far side.
typedef struct {
    u8 view;           // View slot on the far side, stored to the session's and the live save's location
    s8 placementIndex; // Entry of the room's door placement table the player is placed at
} _ShelterB1SterilizationRoomDoorDestination;

/// Row labels drawn by the statistics rows of `func_shelter_b1_sterilization_room_8017D794`.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// Unit suffix appended to the counted rows.
static u8 Telephone_Data_80181A70[];

/// Help lines shown for each statistics row while it is selected.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// Prompt lines drawn by the dialog handlers.
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// List state of the "Play Data" menu `func_shelter_b1_sterilization_room_8017EFE4` runs.
static UiList Telephone_Data_80181C44;

/// List state of the weapon- and PE-usage panels.
static UiList Telephone_Data_80181C6C;

/// UI descriptors the dialog handlers open on confirm.
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;

/// List state of the menu `func_shelter_b1_sterilization_room_8017EB2C` runs.
static UiList Telephone_Data_80181CF4;

// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry   D_shelter_b1_sterilization_room_80184E40[6];
extern TaskDesc           D_shelter_b1_sterilization_room_80184E70;
extern s32                D_shelter_b1_sterilization_room_80184E7C;
extern s16                D_shelter_b1_sterilization_room_80184E80[3];
extern WorldCollisionGrid D_shelter_b1_sterilization_room_80184F28;

extern AnimationPlayRequest                       D_shelter_b1_sterilization_room_80188624;
extern ActorTransform                             D_shelter_b1_sterilization_room_80188668[];
extern _ShelterB1SterilizationRoomDoorDestination D_shelter_b1_sterilization_room_80188728[];

/// Area records `roomCutsceneTask` applies when it
/// advances game flag nibble 0 from 2 to 3 in one particular view.

extern UiObjectDesc D_800611E4;

extern EvsCommand D_actor_160600_80135AC0[];
extern EvsCommand D_actor_160600_80135D78[];
extern EvsCommand D_actor_160600_80136258[];

#define TELEPHONE_TITLE_BYTES "Telephone\0\0 "
#include "../../shared/telephone.h"

static void func_shelter_b1_sterilization_room_8017FABC(Task* task);
static void func_shelter_b1_sterilization_room_80180340(s32 arg0);
static void func_shelter_b1_sterilization_room_80180464(Task* task);
static void func_shelter_b1_sterilization_room_8018049C(void);
static void func_shelter_b1_sterilization_room_80180570(GfxCoord* coord, s16* arg1);
static void func_shelter_b1_sterilization_room_80180828(Task* task);
static void func_shelter_b1_sterilization_room_80181244(Task* task);

s32  func_shelter_b1_sterilization_room_8017FC78(Task* task, s32 msgId, DirectionActionRequest* msg, s32);
s32  func_shelter_b1_sterilization_room_8017FF80(Task*, s32, s32, s32);
s32  func_shelter_b1_sterilization_room_801803E4(Task*, s32, s32, s32);
s32  func_shelter_b1_sterilization_room_801803EC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b1_sterilization_room_80180430(Task*, s32, s32, s32);
void func_shelter_b1_sterilization_room_80180188(Task*);

static AnimationSet _gShelterB1SterilizationRoomAnimation07C68;
static AnimationSet _gShelterB1SterilizationRoomAnimation07E1C;
static AnimationSet _gShelterB1SterilizationRoomAnimation0853C;
static AnimationSet _gShelterB1SterilizationRoomAnimation086CC;
static AnimationSet _gShelterB1SterilizationRoomAnimation09350;
static AnimationSet _gShelterB1SterilizationRoomAnimation0A858;
static AnimationSet _gShelterB1SterilizationRoomAnimation0AF1C;
void                func_shelter_b1_sterilization_room_80180D74(Task*);
void                func_shelter_b1_sterilization_room_80180F74(Task*);
void                func_shelter_b1_sterilization_room_801811E0(Task*);

#include "../../shared/telephone_data.inc.c"

static TmdBone _gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0Skeleton[3] = {
#include "assets/acropolis_sanctuary_model_090F0_skeleton.inc"
};

static u32 _gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0PartVerts[3] = {
#include "assets/acropolis_sanctuary_model_090F0_partVerts.inc"
};

static SVECTOR _gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0Verts[56] = {
#include "assets/acropolis_sanctuary_model_090F0_verts.inc"
};

static SVECTOR _gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0Normals[6] = {
#include "assets/acropolis_sanctuary_model_090F0_normals.inc"
};

static u32 _gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0Stream[215] = {
#include "assets/acropolis_sanctuary_model_090F0_stream.inc"
};

TmdSource gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0 = {
    0,
    1768,
    0,
    3,
    _gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0PartVerts,
    _gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0Verts,
    _gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0Normals,
    _gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0Skeleton,
    _gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0Stream,
};

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_shelter_b1_sterilization_room_80184E40[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_sterilization_room_801803EC },
    { 5105, func_shelter_b1_sterilization_room_801803E4 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_sterilization_room_8017FC78 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_sterilization_room_8017FF80 },
    { ROOM_MESSAGE_SOUND, func_shelter_b1_sterilization_room_80180430 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b1_sterilization_room_80184E70 = { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_sterilization_room_80180188, { .value = 0 } };

s32 D_shelter_b1_sterilization_room_80184E7C = 0x11004;

s16 D_shelter_b1_sterilization_room_80184E80[3] = { 0 };

static SVECTOR _gShelterB1SterilizationRoomCollision07968Normals[4] = {
#include "assets/shelter_b1_sterilization_room_collision_07968_normals.inc"
};

static SVECTOR _gShelterB1SterilizationRoomCollision07968Verts[8] = {
#include "assets/shelter_b1_sterilization_room_collision_07968_verts.inc"
};

static WorldCollisionGridFace _gShelterB1SterilizationRoomCollision07968Faces[4] = {
#include "assets/shelter_b1_sterilization_room_collision_07968_faces.inc"
};

static s16 _gShelterB1SterilizationRoomCollision07968Cells[6] = {
#include "assets/shelter_b1_sterilization_room_collision_07968_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1SterilizationRoomCollision07968Cells[i])
static s16* _gShelterB1SterilizationRoomCollision07968Table[1] = {
#include "assets/shelter_b1_sterilization_room_collision_07968_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_sterilization_room_80184F28 = { NULL, _gShelterB1SterilizationRoomCollision07968Normals, _gShelterB1SterilizationRoomCollision07968Verts, _gShelterB1SterilizationRoomCollision07968Faces, _gShelterB1SterilizationRoomCollision07968Table, 641, 540, 1, 1, 4000, 4 };

static AnimationPackedPose _gShelterB1SterilizationRoomAnimation07C68Bank1[6] = {
#include "assets/shelter_b1_sterilization_room_animation_07C68_bank1.inc"
};

static AnimationPackedRotation _gShelterB1SterilizationRoomAnimation07C68Bank4[46] = {
#include "assets/shelter_b1_sterilization_room_animation_07C68_bank4.inc"
};

static AnimationRecord _gShelterB1SterilizationRoomAnimation07C68Records[109] = {
#include "assets/shelter_b1_sterilization_room_animation_07C68_records.inc"
};

static u16 _gShelterB1SterilizationRoomAnimation07C68Indices[20] = {
#include "assets/shelter_b1_sterilization_room_animation_07C68_indices.inc"
};

static AnimationSet _gShelterB1SterilizationRoomAnimation07C68 = {
    _gShelterB1SterilizationRoomAnimation07C68Records,
    _gShelterB1SterilizationRoomAnimation07C68Indices,
    { NULL, _gShelterB1SterilizationRoomAnimation07C68Bank1, NULL, NULL, _gShelterB1SterilizationRoomAnimation07C68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1SterilizationRoomAnimation07E1CBank1[2] = {
#include "assets/shelter_b1_sterilization_room_animation_07E1C_bank1.inc"
};

static AnimationPackedRotation _gShelterB1SterilizationRoomAnimation07E1CBank4[26] = {
#include "assets/shelter_b1_sterilization_room_animation_07E1C_bank4.inc"
};

static AnimationRecord _gShelterB1SterilizationRoomAnimation07E1CRecords[57] = {
#include "assets/shelter_b1_sterilization_room_animation_07E1C_records.inc"
};

static u16 _gShelterB1SterilizationRoomAnimation07E1CIndices[20] = {
#include "assets/shelter_b1_sterilization_room_animation_07E1C_indices.inc"
};

static AnimationSet _gShelterB1SterilizationRoomAnimation07E1C = {
    _gShelterB1SterilizationRoomAnimation07E1CRecords,
    _gShelterB1SterilizationRoomAnimation07E1CIndices,
    { NULL, _gShelterB1SterilizationRoomAnimation07E1CBank1, NULL, NULL, _gShelterB1SterilizationRoomAnimation07E1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1SterilizationRoomAnimation0853CBank1[8] = {
#include "assets/shelter_b1_sterilization_room_animation_0853C_bank1.inc"
};

static AnimationPackedRotation _gShelterB1SterilizationRoomAnimation0853CBank4[161] = {
#include "assets/shelter_b1_sterilization_room_animation_0853C_bank4.inc"
};

static AnimationRecord _gShelterB1SterilizationRoomAnimation0853CRecords[251] = {
#include "assets/shelter_b1_sterilization_room_animation_0853C_records.inc"
};

static u16 _gShelterB1SterilizationRoomAnimation0853CIndices[20] = {
#include "assets/shelter_b1_sterilization_room_animation_0853C_indices.inc"
};

static AnimationSet _gShelterB1SterilizationRoomAnimation0853C = {
    _gShelterB1SterilizationRoomAnimation0853CRecords,
    _gShelterB1SterilizationRoomAnimation0853CIndices,
    { NULL, _gShelterB1SterilizationRoomAnimation0853CBank1, NULL, NULL, _gShelterB1SterilizationRoomAnimation0853CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1SterilizationRoomAnimation086CCBank1[2] = {
#include "assets/shelter_b1_sterilization_room_animation_086CC_bank1.inc"
};

static AnimationPackedRotation _gShelterB1SterilizationRoomAnimation086CCBank4[17] = {
#include "assets/shelter_b1_sterilization_room_animation_086CC_bank4.inc"
};

static AnimationRecord _gShelterB1SterilizationRoomAnimation086CCRecords[57] = {
#include "assets/shelter_b1_sterilization_room_animation_086CC_records.inc"
};

static u16 _gShelterB1SterilizationRoomAnimation086CCIndices[20] = {
#include "assets/shelter_b1_sterilization_room_animation_086CC_indices.inc"
};

static AnimationSet _gShelterB1SterilizationRoomAnimation086CC = {
    _gShelterB1SterilizationRoomAnimation086CCRecords,
    _gShelterB1SterilizationRoomAnimation086CCIndices,
    { NULL, _gShelterB1SterilizationRoomAnimation086CCBank1, NULL, NULL, _gShelterB1SterilizationRoomAnimation086CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1SterilizationRoomAnimation09350Bank1[31] = {
#include "assets/shelter_b1_sterilization_room_animation_09350_bank1.inc"
};

static AnimationPackedRotation _gShelterB1SterilizationRoomAnimation09350Bank4[308] = {
#include "assets/shelter_b1_sterilization_room_animation_09350_bank4.inc"
};

static AnimationRecord _gShelterB1SterilizationRoomAnimation09350Records[380] = {
#include "assets/shelter_b1_sterilization_room_animation_09350_records.inc"
};

static u16 _gShelterB1SterilizationRoomAnimation09350Indices[20] = {
#include "assets/shelter_b1_sterilization_room_animation_09350_indices.inc"
};

static AnimationSet _gShelterB1SterilizationRoomAnimation09350 = {
    _gShelterB1SterilizationRoomAnimation09350Records,
    _gShelterB1SterilizationRoomAnimation09350Indices,
    { NULL, _gShelterB1SterilizationRoomAnimation09350Bank1, NULL, NULL, _gShelterB1SterilizationRoomAnimation09350Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1SterilizationRoomAnimation0A858Bank1[41] = {
#include "assets/shelter_b1_sterilization_room_animation_0A858_bank1.inc"
};

static AnimationPackedRotation _gShelterB1SterilizationRoomAnimation0A858Bank4[549] = {
#include "assets/shelter_b1_sterilization_room_animation_0A858_bank4.inc"
};

static AnimationRecord _gShelterB1SterilizationRoomAnimation0A858Records[654] = {
#include "assets/shelter_b1_sterilization_room_animation_0A858_records.inc"
};

static u16 _gShelterB1SterilizationRoomAnimation0A858Indices[20] = {
#include "assets/shelter_b1_sterilization_room_animation_0A858_indices.inc"
};

static AnimationSet _gShelterB1SterilizationRoomAnimation0A858 = {
    _gShelterB1SterilizationRoomAnimation0A858Records,
    _gShelterB1SterilizationRoomAnimation0A858Indices,
    { NULL, _gShelterB1SterilizationRoomAnimation0A858Bank1, NULL, NULL, _gShelterB1SterilizationRoomAnimation0A858Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB1SterilizationRoomAnimation0AF1CBank1[4] = {
#include "assets/shelter_b1_sterilization_room_animation_0AF1C_bank1.inc"
};

static AnimationPackedRotation _gShelterB1SterilizationRoomAnimation0AF1CBank4[151] = {
#include "assets/shelter_b1_sterilization_room_animation_0AF1C_bank4.inc"
};

static AnimationRecord _gShelterB1SterilizationRoomAnimation0AF1CRecords[250] = {
#include "assets/shelter_b1_sterilization_room_animation_0AF1C_records.inc"
};

static u16 _gShelterB1SterilizationRoomAnimation0AF1CIndices[20] = {
#include "assets/shelter_b1_sterilization_room_animation_0AF1C_indices.inc"
};

static AnimationSet _gShelterB1SterilizationRoomAnimation0AF1C = {
    _gShelterB1SterilizationRoomAnimation0AF1CRecords,
    _gShelterB1SterilizationRoomAnimation0AF1CIndices,
    { NULL, _gShelterB1SterilizationRoomAnimation0AF1CBank1, NULL, NULL, _gShelterB1SterilizationRoomAnimation0AF1CBank4, NULL, NULL, NULL },
};

TaskDesc D_shelter_b1_sterilization_room_80188504[9] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_sterilization_room_801811E0, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_sterilization_room_801813A0, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_sterilization_room_80180D74, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_sterilization_room_801814FC, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_sterilization_room_80181588, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_sterilization_room_80180F74, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_sterilization_room_80181634, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_sterilization_room_801816E0, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_sterilization_room_801817EC, { .value = 0 } },
};

AnimationSet* D_shelter_b1_sterilization_room_80188570[8] = {
    NULL,
    &_gShelterB1SterilizationRoomAnimation07C68,
    &_gShelterB1SterilizationRoomAnimation07E1C,
    &_gShelterB1SterilizationRoomAnimation0853C,
    &_gShelterB1SterilizationRoomAnimation086CC,
    &_gShelterB1SterilizationRoomAnimation09350,
    &_gShelterB1SterilizationRoomAnimation0A858,
    &_gShelterB1SterilizationRoomAnimation0AF1C,
};

AnimationBankCopyRequest D_shelter_b1_sterilization_room_80188590 = { { .sets = D_shelter_b1_sterilization_room_80188570 }, ARRAY_SIZE(D_shelter_b1_sterilization_room_80188570) };

AnimationPlayRequest D_shelter_b1_sterilization_room_80188598 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_sterilization_room_801885AC = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 30, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_sterilization_room_801885C0 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_sterilization_room_801885D4 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_sterilization_room_801885E8 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b1_sterilization_room_801885FC = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b1_sterilization_room_80188610 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b1_sterilization_room_80188624 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_shelter_b1_sterilization_room_80188638 = { { 5540, 0, 8600, 0 }, { 0, 2047, 0, 0 } };

ActorTransform D_shelter_b1_sterilization_room_80188650 = { { 5876, 0, 0x28D2, 0 }, { 0, 512, 0, 0 } };

ActorTransform D_shelter_b1_sterilization_room_80188668[8] = {
    { { 4885, 0, 6391, 0 }, { 0, 0, 0, 0 } },
    { { 4885, 0, 10522, 0 }, { 0, 2047, 0, 0 } },
    { { 2066, 0, 6626, 0 }, { 0, 0, 0, 0 } },
    { { 2066, 0, 10522, 0 }, { 0, 2047, 0, 0 } },
    { { 4995, 0, 5227, 0 }, { 0, 2047, 0, 0 } },
    { { 2177, 0, 5227, 0 }, { 0, 2047, 0, 0 } },
    { { 1992, 0, 11566, 0 }, { 0, 0, 0, 0 } },
    { { 4995, 0, 11566, 0 }, { 0, 0, 0, 0 } },
};

_ShelterB1SterilizationRoomDoorDestination D_shelter_b1_sterilization_room_80188728[8] = {
    { 9, 7 },
    { 5, 1 },
    { 8, 6 },
    { 7, 3 },
    { 4, 0 },
    { 3, 4 },
    { 6, 2 },
    { 3, 5 },
};

#include "../../shared/telephone.inc.c"

void func_shelter_b1_sterilization_room_8017EB2C(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

/// The three states of the room's main task, run by
/// `func_shelter_b1_sterilization_room_80180518`: set-up, the per-frame
/// handler and the kill.
static const TaskFuncTable3 D_shelter_b1_sterilization_room_8017D6A4 = {
    {
        func_shelter_b1_sterilization_room_8017FABC,
        func_shelter_b1_sterilization_room_80180464,
        taskKill,
    },
};

static void func_shelter_b1_sterilization_room_8017FABC(Task* task)
{
    Task* target;

    task->msgTable = D_shelter_b1_sterilization_room_80184E40;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == 5 && gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) == 0) {
        gameFlagSetNibble(GAME_FLAG_UNDERGROUND_PARKING_STATE, 3);
        Gp_ApplyAreaRecs(D_shelter_b1_sterilization_room_8018C334);
        if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, 1);
            gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE, 2);
            gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 8);
            func_800E8634(D_actor_160600_80135D78, 0, D_actor_160600_80136258);
            areaSetPlacementVariant(&gGameSession->location.loc, 6, AREA_VARIANT_RESET_ALWAYS);
        } else {
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, 2);
            gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE, 1);
            func_800E8634(D_actor_160600_80135AC0, 0, D_actor_160600_80136258);
        }
    }
    func_shelter_b1_sterilization_room_80180340(0);
    if (gGameSession->location.loc.variant == 5) {
        target = Gp_LookupSlot4(0);
        if (target != NULL) {
            TASK_MESSAGE_DISPATCH_POINTER(target, ACTOR_COMMAND_MESSAGE_APPLY, &D_shelter_b1_sterilization_room_80184E7C, 0);
        }
    }
    if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) != 1) {
        (D_shelter_b1_sterilization_room_8018B8A8 + 22)[0].flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
    if (gGameSession->location.loc.variant == 1) {
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 6, 0, 0);
    }
    task->state++;
}

s32 func_shelter_b1_sterilization_room_8017FC78(Task* task, s32 msgId, DirectionActionRequest* msg, s32 arg3)
{
    s32 cmd;
    s32 mask;
    s32 flags;

    switch (msg->actionId) {
        case 1:
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED) == 0) {
                if (gameFlagGetNibble(GAME_FLAG_B2_NORTH_WALKWAY_SCENE_SEEN) != 0) {
                    func_800E8634(D_shelter_b1_sterilization_room_8018873C, 0,
                                  D_shelter_b1_sterilization_room_80188AB4);
                    gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED, 1);
                }
            }
            break;
        case 2:
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED) == 1) {
                if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 1, 0, 0);
                } else {
                    Gp_RunCapCmd1(0x17);
                }
            } else {
                Gp_RunCapCmd1(0x16);
            }
            break;
        case 5:
        case 8:
            switch (msg->actionId) {
                case 5:
                    cmd  = 5;
                    mask = 2;
                    break;
                case 8:
                    cmd  = 3;
                    mask = 8;
                    break;
                default:
                    cmd  = 0;
                    mask = 0xFF;
                    break;
            }
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED) == 0 || gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == 1) {
                flags = gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_NOTES_SHOWN);
                if (cmd != 0 && !(flags & mask)) {
                    Gp_RunCapCmd1(cmd);
                    gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_NOTES_SHOWN, flags | mask);
                }
                Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 2, msg->actionId - 3, 0);
            } else {
                Gp_RunCapCmd1(0xA);
            }
            break;
        case 3:
        case 6:
        case 7:
        case 10:
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED) == 0 || gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == 1) {
                Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 4, msg->actionId - 3, 0);
            } else {
                Gp_RunCapCmd1(0xA);
            }
            break;
        case 9:
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED) == 0 || gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == 1) {
                if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_149) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 1, 0);
                } else {
                    if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION9_SCENE) == 0) {
                        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 2, 0);
                        gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION9_SCENE, 1);
                    }
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 2, msg->actionId - 3, 0);
                }
            }
            break;
        case 4:
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == 0) {
                if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION4_SCENE) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 3, 0);
                    gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION4_SCENE, 1);
                }
            } else if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION4_SCENE) < 2) {
                if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_14A) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 4, 0);
                } else {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 5, 0);
                }
                gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION4_SCENE, 2);
            }
            Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 2, msg->actionId - 3, 0);
            break;
    }
    return 0;
}

s32 func_shelter_b1_sterilization_room_8017FF80(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0x11) {
        if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_FIRST_SCENE) == 0) {
            gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_FIRST_SCENE, 1);
            Gp_SpawnIfCapIdle(0x18, 1);
            return;
        }
        D_shelter_b1_sterilization_room_8018C344.view            = 0x13;
        D_shelter_b1_sterilization_room_8018C344.capSlot         = 1;
        D_shelter_b1_sterilization_room_8018C344.capFile         = 2;
        D_shelter_b1_sterilization_room_8018C344.skipScene       = 0;
        D_shelter_b1_sterilization_room_8018C344.startSound      = 0x5410000C;
        D_shelter_b1_sterilization_room_8018C344.endSound        = 0x5410000F;
        D_shelter_b1_sterilization_room_8018C344.sceneSound      = 0x5410000D;
        D_shelter_b1_sterilization_room_8018C344.afterSceneSound = 0x5410000E;
        Task_SpawnFromTable(gRoomCutsceneTaskDescs, 0, 6, &D_shelter_b1_sterilization_room_8018C344);
    }
    if (arg2 == 0x15) {
        Gp_RunCapCmd1(arg2);
    }
    if (arg2 == 4) {
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 7, 0, 0);
    }
    if (arg2 == 0x13) {
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 7, 1, 0);
    }
    if (arg2 == 0xE) {
        if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED) == 1 && gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == 0) {
            if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_14F) == 0) {
                Gp_MsgPlayerWeapon(0);
                func_800E8634(D_shelter_b1_sterilization_room_80188ED4, 0, D_shelter_b1_sterilization_room_80188FDC);
                gameFlagSetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_14F, 1);
            } else {
                Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 0xB, 0);
                gameFlagSetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_14F, 2);
            }
        } else {
            Gp_RunCapCmd1(arg2);
        }
    }
    if (arg2 == 0xC || arg2 == 0xD) {
        if (gGameSession->location.loc.room == 3) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_b1_sterilization_room_80184E70, 0, arg2, 0);
        }
    }
    return 0;
}

void func_shelter_b1_sterilization_room_80180188(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0x100);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3FA, 0, 0);
            task->state++;
            break;
        case 1:
            task->state++;
            break;
        case 2:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                task->state++;
            }
            break;
        case 3:
            Gp_RunCapCmd(task->spawnArg1.value, 0);
            task->state++;
            break;
        case 4:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 5:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3FA, 1, 0);
            task->state++;
            break;
        case 6:
            task->state++;
            break;
        case 7:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                task->state++;
            }
            break;
        case 8:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

#include "../../shared/room_cutscene_sound_task.inc.c"

static void func_shelter_b1_sterilization_room_80180340(s32 arg0)
{
    Task* slot   = Gp_LookupSlot4(0);
    Task* task   = slot;
    s32   isNull = (slot == NULL);

    if (isNull) {
        task = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    }
    if (slot != NULL) {
        if (gGameSession->location.loc.variant == 5 && gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) == 1) {
            D_shelter_b1_sterilization_room_80184E80[1] = 0;
        } else {
            D_shelter_b1_sterilization_room_80184E80[1] = 0x2710;
        }
    } else {
        D_shelter_b1_sterilization_room_80184E80[1] = 0x2710;
    }
    func_shelter_b1_sterilization_room_80180570(task->extra.tmd->coords, D_shelter_b1_sterilization_room_80184E80);
}

s32 func_shelter_b1_sterilization_room_801803E4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one,
/// passes both to `func_map_shelter_80179A04` and returns 1.
s32 func_shelter_b1_sterilization_room_801803EC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

s32 func_shelter_b1_sterilization_room_80180430(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0x63) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x16), 0, 0);
    }
    return 0;
}

static void func_shelter_b1_sterilization_room_80180464(Task* task)
{
    if (gGameSession->location.loc.variant == 5) {
        func_shelter_b1_sterilization_room_8018049C();
    }
}

static void func_shelter_b1_sterilization_room_8018049C(void)
{
    s32 view;

    view = gGameSession->location.loc.view;
    if ((gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) == 1) && (gGameSession->eventState == 0)) {
        if (view == 2 || view == 3) {
            Gp_MsgSlot4Chain(0, 1);
        } else {
            Gp_MsgSlot4Chain(0, 0);
        }
    }
}

void func_shelter_b1_sterilization_room_80180518(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_sterilization_room_8017D6A4;
    sp.funcs[task->state](task);
}

static void func_shelter_b1_sterilization_room_80180570(GfxCoord* coord, s16* arg1)
{
    MATRIX              m;
    long                flag;
    s32                 i;
    SVECTOR*            d;
    SVECTOR*            s;
    WorldCollisionGrid* dst = &D_shelter_b1_sterilization_room_80189E44;
    WorldCollisionGrid* src = &D_shelter_b1_sterilization_room_80184F28;

    i = 0;
    do {
        dst->normals[i].vx = src->normals[i].vx;
        dst->normals[i].vy = src->normals[i].vy;
        dst->normals[i].vz = src->normals[i].vz;
        dst->faces[i]      = src->faces[i];
        i++;
    } while (i < 4);

    i = 0;
    do {
        dst->vertices[i].vx = src->vertices[i].vx;
        dst->vertices[i].vy = src->vertices[i].vy;
        dst->vertices[i].vz = src->vertices[i].vz;
        i++;
    } while (i < 8);

    m = coord->coord;

    if (arg1 != NULL) {
        m.t[0] += arg1[0];
        m.t[1] += arg1[1];
        m.t[2] += arg1[2];
    }

    d = dst->normals;
    s = src->normals;
    i = 0;
    do {
        gte_SetRotMatrix(&m);
        gte_ldv0(s);
        s++;
        gte_rtv0();
        gte_stsv(d);
        d++;
        i++;
    } while (i < 4);

    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    d = dst->vertices;
    s = src->vertices;
    for (i = 0; i < 8; i++) {
        RotTransSV(s++, d++, &flag);
    }
}

/// Queues two VRAM copies of the frame buffer being drawn, a 0xC0-wide strip to
/// (0x340, 0) and the next 0x80 columns to (0x180, 0x100), bracketed by STP
/// commands that set the mask bit off before them and back on after. The
/// source row follows the live draw buffer. The task then advances a state.
static void func_shelter_b1_sterilization_room_80180828(Task* task)
{
    RECT     rect;
    DR_STP*  stp;
    DR_MOVE* mv;
    s16      x;
    s16      y;

    if (gDisplayState.drawBuffer == 0) {
        x = 0;
        y = 0;
    } else {
        x = 0;
        y = 0x110;
    }

    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 0);
    addPrim(gGpuCurrentOt + 8, stp);

    mv             = gGpuPrimCursor;
    gGpuPrimCursor = mv + 1;
    rect.x         = x;
    rect.y         = y;
    rect.w         = 0xC0;
    rect.h         = 0xF0;
    SetDrawMove(mv, &rect, 0x340, 0);
    addPrim(gGpuCurrentOt + 8, mv);

    mv             = gGpuPrimCursor;
    gGpuPrimCursor = mv + 1;
    rect.x         = x + 0xC0;
    rect.y         = y;
    rect.w         = 0x80;
    rect.h         = 0xF0;
    SetDrawMove(mv, &rect, 0x180, 0x100);
    addPrim(gGpuCurrentOt + 8, mv);

    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 1);
    addPrim(gGpuCurrentOt + 8, stp);

    task->state++;
}

#include "../../shared/backdrop_crossfade_live.inc.c"

/// Redraw the room's two backdrop halves as semi-transparent `SPRT`s in OT
/// slot 8, tinting both with `shade`, then append each half's tpage.
void crossfadeDrawBackdrop(s32 shade)
{
    SPRT* p;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = -0xA0;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0xC0;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    crossfadeSetTpage(0x340, 0);

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = 0x20;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0x80;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    crossfadeSetTpage(0x180, 0x100);
}

void func_shelter_b1_sterilization_room_80180D74(Task* task)
{
    u8 c;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_STERILIZATION_DOOR_OPEN, 0, 0);
            task->state++;
            break;
        case 1:
            if (++task->killCountdown >= 30) {
                task->state++;
            }
            c = (task->killCountdown * 0xFF / 30) & 0xFF;
            fadeDrawOverlay(c, c, c, GPU_BLEND_SUBTRACT);
            break;
        case 2:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_shelter_b1_sterilization_room_80188728[task->spawnArg1.value].view;
            gGameSession->location.loc.view                            = D_shelter_b1_sterilization_room_80188728[task->spawnArg1.value].view;
            gGameSession->viewDirty                                    = 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE,
                                          &D_shelter_b1_sterilization_room_80188668[D_shelter_b1_sterilization_room_80188728[task->spawnArg1.value].placementIndex],
                                          0);
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_STERILIZATION_DOOR_CLOSE, 0, 0);
            fadeDrawOverlay(0xFF, 0xFF, 0xFF, GPU_BLEND_SUBTRACT);
            task->state++;
            break;
        case 3:
            if (--task->killCountdown <= 0) {
                task->state++;
            }
            c = (task->killCountdown * 0xFF / 30) & 0xFF;
            fadeDrawOverlay(c, c, c, GPU_BLEND_SUBTRACT);
            break;
        default:
            Gp_MsgPlayerWeapon(1);
            taskKill(task);
            break;
    }
}

void func_shelter_b1_sterilization_room_80180F74(Task* task)
{
    Task*            player;
    GfxCoord*        coord;
    s32              pan;
    AttachmentState* attachment;

    switch (task->state) {
        case 0:
            gGameSession->restartMode = 2;
            task->state++;
            return;
        case 1:
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == 0) {
                if (gGameSession->eventState == 0 && gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    task->killCountdown++;
                    if (task->killCountdown == 0x78) {
                        taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackPair(&D_shelter_b1_sterilization_room_80188738, 0), 0);
                    } else if (task->killCountdown >= 0x79) {
                        if (gPlayerStatus.hp > 0) {
                            coord = player->extra.tmd->coords;
                            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &D_shelter_b1_sterilization_room_80188590, 0);
                            Gp_PlayerWeaponId(&D_shelter_b1_sterilization_room_80188624.source.index);
                            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_PLAY, &D_shelter_b1_sterilization_room_80188624, 0);
                            pan = (s8)worldCoordGetOriginAudioPan(coord);
                            sndEvtRequestScriptStart(SOUND_SHELTER_B1_STERILIZATION_PLAYER_HURT, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                            task->killCountdown = 0;
                        }
                        attachment         = &Gp_StateC08;
                        attachment->flags |= ATTACHMENT_FLAG_EVENT_LOCK;
                    } else if (task->killCountdown == 0x49) {
                        Gp_MsgPlayerWeapon(1);
                    }
                    task->spawnArg1.value = 0;
                    return;
                }
                if (task->spawnArg1.value == 0) {
                    SndEvt_EnqueueType7(SOUND_SHELTER_B1_STERILIZATION_PLAYER_HURT, 1);
                    task->spawnArg1.value = 1;
                }
                return;
            }
            SndEvt_EnqueueType7(SOUND_SHELTER_B1_STERILIZATION_PLAYER_HURT, 1);
        default:
            taskKill(task);
            break;
    }
}

void func_shelter_b1_sterilization_room_8018118C(s32 arg0)
{
    if (!(((s32)D_shelter_b1_sterilization_room_8018C340 >> arg0) & 1)) {
        D_shelter_b1_sterilization_room_8018C340 |= 1 << arg0;
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, arg0, 0, 0);
    }
}

/// The four states of the backdrop task run by
/// `func_shelter_b1_sterilization_room_801811E0`: copy the frame buffer into
/// the backdrop, wait for the view, fade the copy out and kill.
static const TaskFuncTable4 D_shelter_b1_sterilization_room_8017D700 = {
    {
        func_shelter_b1_sterilization_room_80180828,
        func_shelter_b1_sterilization_room_80181244,
        crossfadeOutState,
        taskKill,
    },
};

void func_shelter_b1_sterilization_room_801811E0(Task* task)
{
    TaskFuncTable4 states;

    states = D_shelter_b1_sterilization_room_8017D700;
    states.funcs[task->state](task);
}

static void func_shelter_b1_sterilization_room_80181244(Task* task)
{
    crossfadeDrawBackdrop(0x80);
    crossfadeDrawLive(0);
    if (gGameSession->viewReady != 0) {
        task->killCountdown = 0x80;
        task->state++;
    }
}

#include "../../shared/backdrop_crossfade_out.inc.c"

#include "../../shared/backdrop_crossfade_tpage.inc.c"
